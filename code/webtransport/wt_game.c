/*
===========================================================================
wt_game.c — QUIC game transport

Implements QUIC game handshake (Stream 0x01) and netchan datagram
multiplexing for WT_PERM_PLAYER connections.

Public API (wt_public.h, gated on FEAT_QUIC_GAME):
  QUIC_SendGamePacketToAddr  — Sys_SendPacket calls this for NA_QUIC/NA_QUIC6
  QUIC_GetGamePacket         — NET_Event drains game datagrams via this

Internal API (wt_local.h, called from wt_main.c / wt_connection.c):
  WT_GameAllocConn           — set up game state for a new player connection
  WT_GameFreeConn            — release game state on disconnect
  WT_GameHandleDatagram      — enqueue a received netchan datagram
  WT_GameHandleHandshake     — process Stream 0x01 connect message

Stream 0x01 protocol (client-initiated bidi):
  Client → Server: {"type":"connect","userinfo":"\\k\\v...","qport":"N"}
  Server → Client: {"type":"connectResponse"} | {"type":"refuse","reason":"..."}

After the handshake, all game traffic flows as QUIC unreliable datagrams
using the existing Q3 netchan binary packet format — no protocol change.
===========================================================================
*/
#include "wt_local.h"

#if FEAT_QUIC_GAME

// ───────────────────────────────────────────────────────────────────
// Internal helpers
// ───────────────────────────────────────────────────────────────────

static wt_game_conn_t *WT_GameFindConnForAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_game_conn_t *gc = &wt.game_conns[i];
		if ( !gc->active || !gc->conn )
			continue;
		/* Compare raw IP bytes regardless of NA_IP vs NA_QUIC type.
		   gc->conn->addr is stored as NA_IP/NA_IP6 (from picoquic sockaddr);
		   addr may be NA_QUIC/NA_QUIC6 (set by the game packet enqueue path). */
		{
			const netadr_t *a = &gc->conn->addr;
			if ( NET_IS_IPV6( a->type ) && NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._6, addr->ipv._6, 16 ) == 0 )
					return gc;
			} else if ( !NET_IS_IPV6( a->type ) && !NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._4, addr->ipv._4, 4 ) == 0 )
					return gc;
			}
		}
	}
	return NULL;
}


/*
Simple JSON string-value extractor — no malloc, no external JSON dep.
Finds "key":"value" in a flat JSON object and copies the value to buf.
Returns qtrue on success.
*/
static qboolean WT_JSONGetString( const char *json, int json_len,
                                   const char *key,
                                   char *buf, int buf_size )
{
	char search[64];
	int slen, key_len;
	const char *p   = json;
	const char *end = json + json_len;

	key_len = (int)strlen( key );
	(void)key_len;  // silence unused-variable warning when not in debug build

	Com_sprintf( search, sizeof(search), "\"%s\"", key );
	slen = (int)strlen( search );

	while ( p < end - slen ) {
		if ( memcmp( p, search, slen ) == 0 ) {
			p += slen;
			while ( p < end && (*p == ' ' || *p == ':') ) p++;
			if ( p >= end ) return qfalse;
			if ( *p == '"' ) {
				const char *val = ++p;
				while ( p < end && *p != '"' ) p++;
				if ( p >= end ) return qfalse;
				{
					int len = (int)(p - val);
					if ( len >= buf_size ) len = buf_size - 1;
					Com_Memcpy( buf, val, len );
					buf[len] = '\0';
					return qtrue;
				}
			}
		}
		p++;
	}
	return qfalse;
}


// ───────────────────────────────────────────────────────────────────
// Game conn lifecycle
// ───────────────────────────────────────────────────────────────────

/*
==================
WT_GameAllocConn

Called from wt_connection.c when WT_PERM_PLAYER is granted on a new
QUIC connection.  Finds a free game_conn slot and links it.
==================
*/
wt_game_conn_t *WT_GameAllocConn( wt_connection_t *conn )
{
	int i;

	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_game_conn_t *gc = &wt.game_conns[i];
		if ( !gc->active ) {
			Com_Memset( gc, 0, sizeof( wt_game_conn_t ) );
			gc->active   = qtrue;
			gc->conn     = conn;
			gc->hs_state = WT_GAME_HS_NONE;
			wt.num_game_conns++;
			conn->game_conn = gc;
			Com_DPrintf( "QUIC game: allocated conn slot %d for %s\n",
				i, NET_AdrToStringwPort( &conn->addr ) );
			return gc;
		}
	}

	Com_Printf( S_COLOR_YELLOW "QUIC game: no free game_conn slots\n" );
	return NULL;
}


/*
==================
WT_GameFreeConn

Called from WT_FreeConnection on player disconnect.
==================
*/
void WT_GameFreeConn( wt_game_conn_t *gc )
{
	if ( !gc || !gc->active )
		return;

	if ( gc->conn )
		gc->conn->game_conn = NULL;

	Com_DPrintf( "QUIC game: freed conn slot %td\n", gc - wt.game_conns );
	Com_Memset( gc, 0, sizeof( wt_game_conn_t ) );

	if ( wt.num_game_conns > 0 )
		wt.num_game_conns--;
}


// ───────────────────────────────────────────────────────────────────
// Datagram handling (called from wt_main.c picoquic_callback_datagram)
// ───────────────────────────────────────────────────────────────────

/*
==================
WT_GameHandleDatagram

Enqueue a netchan-format QUIC datagram received from a game client.
Called by WT_PicoquicCallback when a datagram arrives on a connection
that has WT_PERM_PLAYER set.

SPSC ring buffer: QUIC callback is the sole producer; NET_Event main
loop is the sole consumer — no locking needed.
==================
*/
void WT_GameHandleDatagram( wt_connection_t *conn, const byte *data, int len )
{
	wt_game_conn_t *gc;
	int next_head;
	wt_game_pkt_t *pkt;

	if ( len <= 0 || len > WT_GAME_PKT_MAX ) {
		Com_DPrintf( "QUIC game: datagram length %d out of range — dropped\n", len );
		return;
	}

	gc = conn ? conn->game_conn : NULL;
	if ( !gc || !gc->active ) {
		Com_DPrintf( "QUIC game: datagram from non-game connection — dropped\n" );
		return;
	}

	next_head = ( gc->recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
	if ( next_head == gc->recv_tail ) {
		wt.dropped_packets++;
		Com_DPrintf( "QUIC game: recv queue full for %s — dropped\n",
			NET_AdrToStringwPort( &conn->addr ) );
		return;
	}

	pkt         = &gc->recv_queue[gc->recv_head];
	pkt->from   = conn->addr;
	/* Mark as QUIC so the server dispatch (SV_PacketEvent) can route it
	   through the QUIC netchan path rather than the UDP path. */
	pkt->from.type = ( conn->addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
	pkt->len    = len;
	Com_Memcpy( pkt->data, data, len );

	gc->recv_head = next_head;
}


// ───────────────────────────────────────────────────────────────────
// Handshake — Stream 0x01 (game connect flow)
// ───────────────────────────────────────────────────────────────────

/*
==================
WT_GameHandleHandshake

Process a Stream 0x01 message from a game client.
Expected JSON:
  {"type":"connect","userinfo":"\\k\\v\\...","qport":"<n>"}

Converts the QUIC connect request into a synthesized OOB Q3 "connect"
packet (same binary format as UDP OOB) and enqueues it in the game
conn's recv_queue.  NET_Event drains this queue and dispatches via
the standard Com_RunAndTimeServerPacket → SV_PacketEvent path — the
server sees a normal Q3 connect from an NA_QUIC address and calls
SVC_DirectConnect without any separate QUIC-specific server code.

The Q3 challenge check is skipped for NA_QUIC addresses (enforced in
sv_client.c) since QUIC TLS handshake already authenticates the peer.
==================
*/
void WT_GameHandleHandshake( wt_connection_t *conn, uint64_t stream_id,
                              const byte *data, int len )
{
	char type[32];
	char userinfo[MAX_INFO_STRING];
	char qport_str[8];
	wt_game_conn_t *gc;

	if ( !conn || !conn->active )
		return;

	if ( !WT_JSONGetString( (const char *)data, len, "type", type, sizeof(type) ) ) {
		Com_DPrintf( "QUIC game: handshake missing 'type' field — ignoring\n" );
		return;
	}

	if ( Q_stricmp( type, "connect" ) != 0 ) {
		Com_DPrintf( "QUIC game: unknown handshake type '%s'\n", type );
		return;
	}

	// Ensure the connection has a game conn slot
	gc = conn->game_conn;
	if ( !gc ) {
		gc = WT_GameAllocConn( conn );
		if ( !gc ) {
			// Server full — refuse immediately on the stream
			const char *refuse = "{\"type\":\"refuse\",\"reason\":\"server full\"}";
			picoquic_add_to_stream( conn->cnx, stream_id,
				(const uint8_t *)refuse, strlen( refuse ), 1 );
			return;
		}
	}

	gc->handshake_stream_id = stream_id;
	gc->hs_state = WT_GAME_HS_PENDING;

	// Extract fields (defaults if missing)
	if ( !WT_JSONGetString( (const char *)data, len, "userinfo",
	                        userinfo, sizeof(userinfo) ) )
		Q_strncpyz( userinfo, "\\name\\unnamed", sizeof(userinfo) );

	if ( !WT_JSONGetString( (const char *)data, len, "qport",
	                        qport_str, sizeof(qport_str) ) )
		Q_strncpyz( qport_str, "0", sizeof(qport_str) );

	// Build a synthesized OOB Q3 "connect" packet and enqueue it so the
	// existing SV_PacketEvent → SVC_DirectConnect path handles it.
	// Format: "\xff\xff\xff\xff" + "connect " + userinfo + " " + qport
	{
		char pktbuf[MAX_INFO_STRING + 64];
		int pktlen;
		int next_head;
		wt_game_pkt_t *pkt;

		Com_sprintf( pktbuf, sizeof(pktbuf),
			"\xff\xff\xff\xff" "connect %s %s", userinfo, qport_str );
		pktlen = (int)strlen( pktbuf ) + 1;  // include NUL

		next_head = ( gc->recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
		if ( next_head == gc->recv_tail ) {
			Com_Printf( S_COLOR_YELLOW "QUIC game: recv queue full, "
				"cannot enqueue connect from %s\n",
				NET_AdrToStringwPort( &conn->addr ) );
			return;
		}

		pkt          = &gc->recv_queue[gc->recv_head];
		pkt->from    = conn->addr;
		pkt->from.type = ( conn->addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
		pkt->len     = pktlen;
		Com_Memcpy( pkt->data, pktbuf, pktlen );
		gc->recv_head = next_head;
	}

	Com_DPrintf( "QUIC game: enqueued connect from %s\n",
		NET_AdrToStringwPort( &conn->addr ) );
}


// ───────────────────────────────────────────────────────────────────
// Public API — called from net_ip.c
// ───────────────────────────────────────────────────────────────────

/*
==================
QUIC_SendGamePacketToAddr

Send a netchan-format datagram to a QUIC game client identified by address.
Called from Sys_SendPacket when to->type is NA_QUIC or NA_QUIC6.
==================
*/
void QUIC_SendGamePacketToAddr( const netadr_t *to, const void *data, int length )
{
	wt_game_conn_t *gc;

	if ( !wt.initialized || !wt.quic ) {
#if !defined(DEDICATED)
		/* Dedicated server is the only case that requires wt.initialized.
		   On a game client, fall through to the client path below. */
		if ( !wt.initialized )
			goto try_client;
#endif
		Com_DPrintf( "QUIC_SendGamePacketToAddr: QUIC not initialized\n" );
		return;
	}

	gc = WT_GameFindConnForAddr( to );
	if ( gc && gc->conn && gc->conn->cnx ) {
		picoquic_queue_datagram_frame( gc->conn->cnx, (size_t)length,
		                               (const uint8_t *)data );
		return;
	}

#if !defined(DEDICATED)
try_client:
	/* On a non-dedicated client: check the client QUIC connection */
	QUIC_ClientSendPacket( to, data, length );
#else
	Com_DPrintf( "QUIC_SendGamePacketToAddr: no game conn for %s\n",
		NET_AdrToStringwPort( to ) );
#endif
}


/*
==================
QUIC_GetGamePacket

Dequeue one netchan-format datagram from any active game client.
Fills *from (type = NA_QUIC / NA_QUIC6) and populates *message.
Called from NET_Event after the UDP recvfrom loop.
Returns qtrue if a packet was dequeued, qfalse when all queues empty.
==================
*/
qboolean QUIC_GetGamePacket( netadr_t *from, msg_t *message )
{
	int i;

	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_game_conn_t *gc = &wt.game_conns[i];
		wt_game_pkt_t  *pkt;

		if ( !gc->active || gc->recv_tail == gc->recv_head )
			continue;

		pkt = &gc->recv_queue[gc->recv_tail];

		if ( pkt->len > message->maxsize ) {
			Com_Printf( "QUIC_GetGamePacket: oversized packet %d > %d — discarded\n",
				pkt->len, message->maxsize );
			gc->recv_tail = ( gc->recv_tail + 1 ) % WT_GAME_QUEUE_SIZE;
			continue;
		}

		*from            = pkt->from;
		message->cursize = pkt->len;
		Com_Memcpy( message->data, pkt->data, pkt->len );

		gc->recv_tail = ( gc->recv_tail + 1 ) % WT_GAME_QUEUE_SIZE;
		return qtrue;
	}

#if !defined(DEDICATED)
	/* Also drain client-side receive queue (non-dedicated client) */
	if ( QUIC_ClientGetPacket( from, message ) )
		return qtrue;
#endif

	return qfalse;
}

#endif // FEAT_QUIC_GAME
