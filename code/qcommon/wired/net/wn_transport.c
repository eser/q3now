/*
===========================================================================
wn_transport.c — WiredNet game transport over QUIC

Implements the player-facing network path: session handshake, snapshot and
usercmd datagrams, reliable game channels (bootstrap, download, commands,
events).  This is the sole transport — there is no UDP netchan fallback.

conn_handle_t mapping:
  CONN_INVALID          (0)               — no connection
  1..WN_MAX_CLIENTS                        — wn.game_conns[handle - 1]
  CONN_CLIENT_HANDLE    (WN_MAX_CLIENTS+1) — wtcl (outgoing client cnx)

Wire protocols:
  Stream 0x00 — binary TLV session control (CONNECT/ACCEPT/REFUSE/READY)
  Stream 0x03 — game events (msgpack, server→client)
  Stream 0x04+ — game reliable channels (bootstrap/download/commands)
  Datagrams   — snapshots (server→client) and usercmds (client→server)
===========================================================================
*/
#include "wn_local.h"
#include "../../net_transport.h"

#if !defined(DEDICATED)
#include "picotls/openssl.h"    /* ptls_openssl_verify_certificate_t, override callback */
#include <openssl/sha.h>        /* SHA256 */
#include <openssl/evp.h>        /* X509_get_pubkey, i2d_PUBKEY, EVP_PKEY_free */
#include <openssl/x509.h>       /* X509, STACK_OF(X509) */
#endif

/* ── Global transport pointer (extern declared in net_transport.h) ── */
transport_t *transport = NULL;

/* Sentinel handle for the single outgoing client connection */
#define CONN_CLIENT_HANDLE  ((conn_handle_t)(WN_MAX_CLIENTS + 1))

// ═══════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════

static wn_game_conn_t *wn_get_game_conn( conn_handle_t conn )
{
	wn_game_conn_t *gc;
	if ( conn == CONN_INVALID || conn > WN_MAX_CLIENTS )
		return NULL;
	gc = &wn.game_conns[conn - 1];
	return gc->active ? gc : NULL;
}

static picoquic_cnx_t *wn_get_cnx( conn_handle_t conn )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE )
		return ( wtcl.initialized && wtcl.cnx ) ? wtcl.cnx : NULL;
#endif
	{
		wn_game_conn_t *gc = wn_get_game_conn( conn );
		return ( gc && gc->conn ) ? gc->conn->cnx : NULL;
	}
}

static qboolean wn_reliable_channel_allows_fixed_stream( int channel )
{
	return channel == CHAN_SESSION;
}

#define WN_GAME_REL_VERSION 1

static qboolean wn_reliable_stream_is_client_owned( uint64_t stream_id )
{
	return ( stream_id & 0x01 ) == 0;
}

static int wn_reliable_channel_direction_matches( int channel, qboolean sending_from_client )
{
	switch ( channel ) {
	case CHAN_SESSION:
		return qtrue;
	case CHAN_COMMANDS:
		return sending_from_client;
	case CHAN_BOOTSTRAP:
	case CHAN_DOWNLOAD:
	case CHAN_EVENTS:
	case CHAN_SNAPSHOT_RELIABLE:
		return !sending_from_client;   /* srv→cli only */
	case CHAN_MCP:
		return qtrue;                  /* bidi: both sides may send */
	default:
		return qfalse;
	}
}

static uint64_t wn_resolve_reliable_send_stream( picoquic_cnx_t *cnx, int channel,
	qboolean sending_from_client )
{
	if ( wn_reliable_channel_allows_fixed_stream( channel ) ) {
		return (uint64_t)channel;
	}
	if ( !wn_reliable_channel_direction_matches( channel, sending_from_client ) ) {
		return UINT64_MAX;
	}
	return picoquic_get_next_local_stream_id( cnx,
		/* unidirectional channels — no reverse data expected */
		(channel == CHAN_COMMANDS || channel == CHAN_EVENTS ||
		 channel == CHAN_SNAPSHOT_RELIABLE) ? 1 : 0 );
}

static qboolean wn_reliable_channel_is_game( int channel )
{
	switch ( channel ) {
	case CHAN_BOOTSTRAP:
	case CHAN_DOWNLOAD:
	case CHAN_COMMANDS:
	case CHAN_EVENTS:
	case CHAN_SNAPSHOT_RELIABLE:
	case CHAN_MCP:
		return qtrue;
	default:
		return qfalse;
	}
}

/*
 * Stage the 2-byte reliable header across arbitrarily-chunked picoquic stream
 * deliveries. The first chunk of a stream may carry 0, 1, or 2+ header bytes.
 * This helper consumes up to 2 bytes from (*data_io, *len_io) into the
 * partial slot's staging buffer, advances the pointers past the consumed
 * bytes, and — once the header is complete — validates and stores the
 * channel into partial->channel.
 *
 * Returns:
 *   qtrue  — header bytes consumed cleanly; caller should continue with the
 *            remaining (*data_io, *len_io) as payload. If partial->channel
 *            is still -1 on return, the header is not yet complete (needs
 *            more chunks), and the caller should return without dispatching.
 *   qfalse — header is complete but malformed; caller must free the partial
 *            slot and drop the stream.
 */
static qboolean wn_reliable_stage_header( wn_rel_partial_t *partial,
	const byte **data_io, int *len_io, uint64_t stream_id, const char *tag )
{
	while ( partial->header_bytes < 2 && *len_io > 0 ) {
		partial->header_staging[partial->header_bytes++] = (*data_io)[0];
		(*data_io)++;
		(*len_io)--;
	}
	WN_DBG( "QUIC %s: stream %llu header staged %d/2 bytes\n",
		tag, (unsigned long long)stream_id, partial->header_bytes );
	if ( partial->header_bytes < 2 ) {
		return qtrue; /* not yet complete — wait for next chunk */
	}
	if ( partial->channel >= 0 ) {
		return qtrue; /* already parsed on a previous call */
	}
	if ( partial->header_staging[0] != WN_GAME_REL_VERSION ) {
		Com_DPrintf( "QUIC %s: invalid reliable header on stream %llu "
			"(version=%d expected %d) — dropped\n",
			tag, (unsigned long long)stream_id,
			(int)partial->header_staging[0], (int)WN_GAME_REL_VERSION );
		return qfalse;
	}
	if ( !wn_reliable_channel_is_game( partial->header_staging[1] ) ) {
		Com_DPrintf( "QUIC %s: invalid reliable header on stream %llu "
			"(channel=%d) — dropped\n",
			tag, (unsigned long long)stream_id,
			(int)partial->header_staging[1] );
		return qfalse;
	}
	partial->channel = partial->header_staging[1];
	WN_DBG( "QUIC %s: stream %llu header complete chan=%d\n",
		tag, (unsigned long long)stream_id, partial->channel );
	return qtrue;
}

static wn_rel_partial_t *wn_reliable_find_partial( wn_rel_partial_t *partials, uint64_t stream_id )
{
	int i;
	for ( i = 0; i < WN_REL_PARTIAL_CAP; i++ ) {
		if ( partials[i].active && partials[i].stream_id == stream_id ) {
			return &partials[i];
		}
	}
	return NULL;
}

static wn_rel_partial_t *wn_reliable_alloc_partial( wn_rel_partial_t *partials,
	uint64_t stream_id )
{
	int i;
	for ( i = 0; i < WN_REL_PARTIAL_CAP; i++ ) {
		if ( !partials[i].active ) {
			memset( &partials[i], 0, sizeof( partials[i] ) );
			partials[i].active       = qtrue;
			partials[i].stream_id    = stream_id;
			partials[i].channel      = -1; /* filled in once header is staged */
			partials[i].header_bytes = 0;
			return &partials[i];
		}
	}
	return NULL;
}

static void wn_reliable_free_partial( wn_rel_partial_t *partial )
{
	if ( partial ) {
		memset( partial, 0, sizeof( *partial ) );
	}
}

static qboolean wn_reliable_queue_push( wn_rel_msg_t *queue, volatile int *head,
	volatile int *tail, int channel, const byte *data, int len )
{
	int next_head = ( *head + 1 ) % WN_REL_QUEUE_SIZE;
	wn_rel_msg_t *msg;
	if ( next_head == *tail ) {
		Com_DPrintf( "QUIC: reliable recv queue full — message dropped (channel=%d len=%d)\n", channel, len );
		return qfalse;
	}
	msg = &queue[*head];
	msg->channel = channel;
	msg->len     = len;
	memcpy( msg->data, data, (size_t)len );
	*head = next_head;
	return qtrue;
}

static qboolean wn_reliable_queue_pop( wn_rel_msg_t *queue, volatile int *head,
	volatile int *tail, int *channel_out, byte *buf, int *len_out )
{
	wn_rel_msg_t *msg;
	if ( *tail == *head ) {
		return qfalse;
	}
	msg = &queue[*tail];
	if ( msg->len > *len_out ) {
		Com_Printf( S_COLOR_RED "QUIC: wn_reliable_queue_pop: dropping oversize message "
			"(chan=%d len=%d > buf=%d) — increase caller buffer\n",
			msg->channel, msg->len, *len_out );
		*tail = ( *tail + 1 ) % WN_REL_QUEUE_SIZE;
		return qfalse;
	}
	*channel_out = msg->channel;
	*len_out     = msg->len;
	memcpy( buf, msg->data, (size_t)msg->len );
	*tail = ( *tail + 1 ) % WN_REL_QUEUE_SIZE;
	return qtrue;
}

static void wn_reliable_server_consume_stream( wn_game_conn_t *gc, uint64_t stream_id,
	const byte *data, int len, qboolean fin )
{
	wn_rel_partial_t *partial;
	const byte *payload = data;
	int payload_len = len;

	/* picoquic delivers stream data in arbitrarily-sized chunks via repeated
	 * picoquic_callback_stream_data events. The 2-byte reliable header
	 * [version:1][channel:1] sits at the very start of the stream; a chunk
	 * may carry 0, 1, 2, or more header bytes. We stage header bytes across
	 * chunks until complete, then route the rest as payload. */
	partial = wn_reliable_find_partial( gc->rel_partials, stream_id );
	if ( !partial ) {
		partial = wn_reliable_alloc_partial( gc->rel_partials, stream_id );
		if ( !partial ) {
			Com_DPrintf( "QUIC game: no partial slots for reliable stream %llu\n",
				(unsigned long long)stream_id );
			return;
		}
		WN_DBG( "QUIC game: partial slot alloc for stream %llu first_chunk=%d bytes\n",
			(unsigned long long)stream_id, len );
	}
	if ( !wn_reliable_stage_header( partial, &payload, &payload_len, stream_id, "game" ) ) {
		wn_reliable_free_partial( partial );
		return;
	}
	if ( partial->channel < 0 ) {
		/* Header still staging; nothing to append yet. If the stream already
		 * closed (fin), warn and free — we can't dispatch without a channel. */
		if ( fin ) {
			Com_DPrintf( "QUIC game: stream %llu closed with incomplete header "
				"(%d/2 bytes) — dropped\n",
				(unsigned long long)stream_id, partial->header_bytes );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( gc->conn->cnx, stream_id );
		}
		return;
	}
	if ( partial->len + payload_len > MAX_MSGLEN ) {
		Com_DPrintf( "QUIC game: reliable stream %llu overflow — dropped (%d+%d > %d)\n",
			(unsigned long long)stream_id, partial->len, payload_len, MAX_MSGLEN );
		wn_reliable_free_partial( partial );
		return;
	}
	if ( payload_len > 0 ) {
		memcpy( partial->data + partial->len, payload, (size_t)payload_len );
		partial->len += payload_len;
		WN_DBG( "QUIC game: partial slot for stream %llu: %d bytes (fin=%d)\n",
			(unsigned long long)stream_id, partial->len, (int)fin );
	}
	if ( fin ) {
		WN_DBG( "QUIC game: partial slot for stream %llu: %d bytes COMPLETE (chan=%d)\n",
			(unsigned long long)stream_id, partial->len, partial->channel );
		if ( !wn_reliable_queue_push( gc->rel_queue, &gc->rel_head, &gc->rel_tail,
			partial->channel, partial->data, partial->len ) ) {
			Com_DPrintf( "QUIC game: reliable recv queue full for stream %llu\n",
				(unsigned long long)stream_id );
		}
		wn_reliable_free_partial( partial );
		picoquic_reset_stream_ctx( gc->conn->cnx, stream_id );
	}
}

#if !defined(DEDICATED)
static void wn_reliable_client_consume_stream( uint64_t stream_id, const byte *data,
	int len, qboolean fin )
{
	wn_rel_partial_t *partial;
	const byte *payload = data;
	int payload_len = len;

	/* See wn_reliable_server_consume_stream for rationale: the 2-byte header
	 * may arrive across multiple chunks. Stage header bytes, then append. */
	partial = wn_reliable_find_partial( wtcl.rel_partials, stream_id );
	if ( !partial ) {
		partial = wn_reliable_alloc_partial( wtcl.rel_partials, stream_id );
		if ( !partial ) {
			Com_DPrintf( "QUIC client: no partial slots for stream %llu\n",
				(unsigned long long)stream_id );
			return;
		}
		WN_DBG( "QUIC client: partial slot alloc for stream %llu first_chunk=%d bytes\n",
			(unsigned long long)stream_id, len );
	}
	if ( !wn_reliable_stage_header( partial, &payload, &payload_len, stream_id, "client" ) ) {
		wn_reliable_free_partial( partial );
		return;
	}
	if ( partial->channel < 0 ) {
		/* Header still staging; nothing to append yet. If the stream already
		 * closed (fin), warn and free — we can't dispatch without a channel. */
		if ( fin ) {
			Com_DPrintf( "QUIC client: stream %llu closed with incomplete header "
				"(%d/2 bytes) — dropped\n",
				(unsigned long long)stream_id, partial->header_bytes );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl.cnx, stream_id );
		}
		return;
	}
	/* CHAN_BOOTSTRAP exceeds MAX_MSGLEN — route to the dedicated large buffer. */
	if ( partial->channel == CHAN_BOOTSTRAP ) {
		if ( wtcl.bootstrap_recv_ready ) {
			Com_DPrintf( "QUIC client: bootstrap already pending; stream %llu dropped\n",
				(unsigned long long)stream_id );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl.cnx, stream_id );
			return;
		}
		if ( wtcl.bootstrap_recv_len + payload_len > WN_BOOTSTRAP_MAX ) {
			Com_DPrintf( "QUIC client: bootstrap stream %llu overflow — dropped\n",
				(unsigned long long)stream_id );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl.cnx, stream_id );
			return;
		}
		if ( payload_len > 0 ) {
			memcpy( wtcl.bootstrap_recv_data + wtcl.bootstrap_recv_len,
				payload, (size_t)payload_len );
			wtcl.bootstrap_recv_len += payload_len;
		}
		if ( fin ) {
			wtcl.bootstrap_recv_ready = qtrue;
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl.cnx, stream_id );
		}
		return;
	}
	if ( partial->len + payload_len > MAX_MSGLEN ) {
		Com_DPrintf( "QUIC client: reliable stream %llu overflow — dropped (%d+%d > %d)\n",
			(unsigned long long)stream_id, partial->len, payload_len, MAX_MSGLEN );
		wn_reliable_free_partial( partial );
		return;
	}
	if ( payload_len > 0 ) {
		memcpy( partial->data + partial->len, payload, (size_t)payload_len );
		partial->len += payload_len;
		WN_DBG( "QUIC client: partial slot for stream %llu: %d bytes (fin=%d)\n",
			(unsigned long long)stream_id, partial->len, (int)fin );
	}
	if ( fin ) {
		WN_DBG( "QUIC client: partial slot for stream %llu: %d bytes COMPLETE (chan=%d)\n",
			(unsigned long long)stream_id, partial->len, partial->channel );
		if ( !wn_reliable_queue_push( wtcl.rel_queue, &wtcl.rel_head, &wtcl.rel_tail,
			partial->channel, partial->data, partial->len ) ) {
			Com_DPrintf( "QUIC client: reliable recv queue full for stream %llu\n",
				(unsigned long long)stream_id );
		}
		wn_reliable_free_partial( partial );
		picoquic_reset_stream_ctx( wtcl.cnx, stream_id );
	}
}
#endif

static wn_game_conn_t *WN_GameFindConnForAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->active || !gc->conn )
			continue;
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


// ═══════════════════════════════════════════════════════════════════
// Binary TLV helpers — Stream 0 session channel
// ═══════════════════════════════════════════════════════════════════

/*
 * TLV_Write — encode one TLV message into buf.
 * Layout: [type:u8][plen:u16le][payload:plen bytes]
 * Returns total bytes written, or 0 if buf is too small.
 */
static int TLV_Write( byte *buf, int bufsize, uint8_t type,
                       const byte *payload, uint16_t plen )
{
	int total = 3 + (int)plen;
	if ( bufsize < total )
		return 0;
	buf[0] = type;
	buf[1] = (byte)( plen & 0xFF );
	buf[2] = (byte)( (plen >> 8) & 0xFF );
	if ( plen > 0 && payload )
		memcpy( buf + 3, payload, plen );
	return total;
}

/*
 * TLV_Read — parse one TLV message from data.
 * Sets *type_out, *payload_out (pointer into data), *plen_out.
 * Returns qfalse when data is too short to hold a complete message.
 */
static qboolean TLV_Read( const byte *data, int len,
                           uint8_t *type_out, const byte **payload_out,
                           uint16_t *plen_out )
{
	uint16_t plen;
	if ( len < 3 )
		return qfalse;
	plen = (uint16_t)( data[1] | ( (uint16_t)data[2] << 8 ) );
	if ( len < 3 + (int)plen )
		return qfalse;
	*type_out    = data[0];
	*plen_out    = plen;
	*payload_out = data + 3;
	return qtrue;
}


// ═══════════════════════════════════════════════════════════════════
// Game conn lifecycle  (was wn_transport.c)
// ═══════════════════════════════════════════════════════════════════

wn_game_conn_t *WN_GameAllocConn( wn_connection_t *conn )
{
	int i;
	int max_clients = wn.sv_wirednetMaxClients ? wn.sv_wirednetMaxClients->integer : WN_MAX_CLIENTS;
	if ( max_clients > WN_MAX_CLIENTS ) max_clients = WN_MAX_CLIENTS;

	/* E5: Enforce sv_wirednetMaxClients before allocating a slot. */
	if ( wn.num_game_conns >= max_clients ) {
		Com_Printf( S_COLOR_YELLOW "QUIC game: connection limit (%d) reached, refusing\n",
		            max_clients );
		return NULL;
	}

	for ( i = 0; i < max_clients; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->active ) {
			memset( gc, 0, sizeof( wn_game_conn_t ) );
			gc->active   = qtrue;
			gc->conn     = conn;
			gc->hs_state = WN_GAME_HS_NONE;
			wn.num_game_conns++;
			conn->game_conn = gc;
			Com_DPrintf( "QUIC game: allocated conn slot %d for %s\n",
				i, NET_AdrToStringwPort( &conn->addr ) );
			return gc;
		}
	}
	Com_Printf( S_COLOR_YELLOW "QUIC game: no free game_conn slots\n" );
	return NULL;
}

void WN_GameFreeConn( wn_game_conn_t *gc )
{
	if ( !gc || !gc->active )
		return;
	if ( gc->conn )
		gc->conn->game_conn = NULL;
	Com_DPrintf( "WiredNet game: freed conn slot %td\n", gc - wn.game_conns );
	memset( gc, 0, sizeof( wn_game_conn_t ) );
	if ( wn.num_game_conns > 0 )
		wn.num_game_conns--;
}


// ═══════════════════════════════════════════════════════════════════
// Datagram handling  (was wn_transport.c)
// ═══════════════════════════════════════════════════════════════════

void WN_GameHandleDatagram( wn_connection_t *conn, const byte *data, int len )
{
	wn_game_conn_t *gc;
	int             next_head;
	wn_game_pkt_t  *pkt;

	if ( len <= 0 || len > WN_GAME_PKT_MAX ) {
		Com_DPrintf( "QUIC game: datagram length %d out of range — dropped\n", len );
		return;
	}
	gc = conn ? conn->game_conn : NULL;
	if ( !gc || !gc->active ) {
		Com_DPrintf( "QUIC game: datagram from non-game connection — dropped\n" );
		return;
	}
	next_head = ( gc->recv_head + 1 ) % WN_GAME_QUEUE_SIZE;
	if ( next_head == gc->recv_tail ) {
		wn.dropped_packets++;
		Com_DPrintf( "QUIC game: recv queue full for %s — dropped\n",
			NET_AdrToStringwPort( &conn->addr ) );
		return;
	}
	pkt           = &gc->recv_queue[gc->recv_head];
	pkt->from     = conn->addr;
	pkt->from.type = ( conn->addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
	pkt->len      = len;
	memcpy( pkt->data, data, len );
	gc->recv_head = next_head;
}


// ═══════════════════════════════════════════════════════════════════
// Handshake — Stream 0x00 (binary TLV only)
// ═══════════════════════════════════════════════════════════════════

void WN_GameHandleHandshake( wn_connection_t *conn, uint64_t stream_id,
                              const byte *data, int len )
{
	uint8_t         tlv_type;
	const byte     *payload;
	uint16_t        plen;
	wn_game_conn_t *gc;
	int             i;

	if ( !conn || !conn->active )
		return;

	/* Binary TLV path */
	if ( !TLV_Read( data, len, &tlv_type, &payload, &plen ) ) {
		Com_DPrintf( "QUIC game: incomplete TLV on handshake stream — ignored\n" );
		return;
	}

	if ( tlv_type == 0x05 ) { /* READY: client has processed gamestate, ready to enter world */
		wn_game_conn_t *gc = conn->game_conn;
		if ( gc && gc->active ) {
			int slot = (int)( gc - wn.game_conns );
			if ( slot >= 0 && slot < WN_MAX_CLIENTS ) {
				wn.pending_ready[slot] = qtrue;
				Com_DPrintf( "QUIC game: TLV READY from slot %d — enqueued\n", slot );
			}
		}
		return;
	}

	if ( tlv_type != 0x01 ) {
		/* Non-CONNECT TLV on session stream — future message types, ignore for now */
		Com_DPrintf( "QUIC game: TLV type 0x%02X on session stream (not CONNECT) — ignored\n",
			(unsigned)tlv_type );
		return;
	}

	/* TLV 0x01 CONNECT: version:u16le, userinfo_len:u16le, userinfo:bytes */
	if ( plen < 4 ) {
		Com_DPrintf( "QUIC game: TLV CONNECT payload too short (%u bytes)\n", (unsigned)plen );
		return;
	}
	{
		char     userinfo[MAX_INFO_STRING];
		uint16_t userinfo_len;
		byte     accept_payload[8];
		byte     accept_tlv[16];
		int      tlv_len;
		int      slot;
		conn_handle_t conn_handle;

		/* version = payload[0..1] (currently ignored — Phase D validates) */
		userinfo_len = (uint16_t)( payload[2] | ( (uint16_t)payload[3] << 8 ) );
		if ( userinfo_len > plen - 4 )
			userinfo_len = (uint16_t)( plen - 4 );
		if ( userinfo_len >= (uint16_t)sizeof(userinfo) )
			userinfo_len = (uint16_t)( sizeof(userinfo) - 1 );
		memcpy( userinfo, payload + 4, userinfo_len );
		userinfo[userinfo_len] = '\0';

		gc = conn->game_conn;
		if ( !gc ) {
			gc = WN_GameAllocConn( conn );
			if ( !gc ) {
				const char *reason = "server full";
				uint16_t    rlen   = (uint16_t)strlen( reason );
				byte        refuse_pl[MAX_INFO_STRING + 2];
				byte        refuse_tlv[MAX_INFO_STRING + 8];
				refuse_pl[0] = (byte)( rlen & 0xFF );
				refuse_pl[1] = (byte)( (rlen >> 8) & 0xFF );
				memcpy( refuse_pl + 2, reason, rlen );
				tlv_len = TLV_Write( refuse_tlv, (int)sizeof(refuse_tlv),
					0x03, refuse_pl, (uint16_t)( rlen + 2 ) );
				if ( tlv_len > 0 )
					picoquic_add_to_stream( conn->cnx, WIREDNET_SESSION_STREAM_ID,
						refuse_tlv, (size_t)tlv_len, 0 );
				return;
			}
		}

		gc->handshake_stream_id = WIREDNET_SESSION_STREAM_ID;
		gc->hs_state            = WN_GAME_HS_PENDING;

		slot        = (int)( gc - wn.game_conns );
		conn_handle = (conn_handle_t)( slot + 1 );

		/* Enqueue pending connect for main-thread consumption */
		for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
			if ( !wn.pending_connects[i].pending ) {
				wn.pending_connects[i].conn    = conn_handle;
				Q_strncpyz( wn.pending_connects[i].userinfo, userinfo,
					sizeof( wn.pending_connects[i].userinfo ) );
				/* Write fields before setting pending flag (store barrier) */
				wn.pending_connects[i].pending = qtrue;
				break;
			}
		}
		if ( i == WN_MAX_CLIENTS ) {
			Com_Printf( S_COLOR_YELLOW
				"QUIC game: pending_connects overflow — TLV CONNECT from %s dropped\n",
				NET_AdrToStringwPort( &conn->addr ) );
			return;
		}

		/* Send TLV 0x02 ACCEPT: slot:u8, sv_fps:u8, sv_info_len:u16le(0), sv_info:"" */
		accept_payload[0] = (byte)slot;
		accept_payload[1] = 20; /* sv_fps */
		accept_payload[2] = 0;  /* sv_info_len lo */
		accept_payload[3] = 0;  /* sv_info_len hi */
		tlv_len = TLV_Write( accept_tlv, (int)sizeof(accept_tlv),
			0x02, accept_payload, 4 );
		if ( tlv_len > 0 )
			picoquic_add_to_stream( conn->cnx, WIREDNET_SESSION_STREAM_ID,
				accept_tlv, (size_t)tlv_len, 0 );

		gc->hs_state = WN_GAME_HS_ACCEPTED;

		Com_DPrintf( "QUIC game: TLV CONNECT from %s → slot %d, accept pending\n",
			NET_AdrToStringwPort( &conn->addr ), slot );
	}
}


void WN_GameHandleReliable( wn_connection_t *conn, uint64_t stream_id,
                               const byte *data, int len, qboolean fin )
{
	wn_game_conn_t *gc = conn ? conn->game_conn : NULL;
	if ( !gc || !gc->active )
		return;
	wn_reliable_server_consume_stream( gc, stream_id, data, len, fin );
}


// ═══════════════════════════════════════════════════════════════════
// Public game packet API  (was wn_transport.c)
// ═══════════════════════════════════════════════════════════════════

void WN_SendGamePacketToAddr( const netadr_t *to, const void *data, int length )
{
	wn_game_conn_t *gc;

	if ( !wn.initialized || !wn.quic ) {
#if !defined(DEDICATED)
		goto try_client;
#else
		Com_DPrintf( "WN_SendGamePacketToAddr: QUIC not initialized\n" );
		return;
#endif
	}

	gc = WN_GameFindConnForAddr( to );
	if ( gc && gc->conn && gc->conn->cnx ) {
		picoquic_queue_datagram_frame( gc->conn->cnx, (size_t)length,
		                               (const uint8_t *)data );
		return;
	}

#if !defined(DEDICATED)
try_client:
	WN_ClientSendPacket( to, data, length );
#else
	Com_DPrintf( "WN_SendGamePacketToAddr: no game conn for %s\n",
		NET_AdrToStringwPort( to ) );
#endif
}

qboolean WN_GetGamePacket( netadr_t *from, msg_t *message )
{
	/* Server usercmd datagrams (gc->recv_queue) must NOT be drained here.
	 * Doing so would starve SV_DrainQUICUsercmds / WN_ServerRecvUsercmd,
	 * which is the only path that sets cl->deltaMessage = snapshotAck.
	 * Without that update wn_outgoing_sequence outpaces deltaMessage by
	 * PACKET_BACKUP-3 and the server prints "Delta request from out of date
	 * packet" on every snapshot, locking the player in place.
	 *
	 * Client snapshot datagrams (wtcl.recv_queue) are equally off-limits:
	 * WN_ClientGetPacket already returns qfalse for the same reason
	 * (see its comment — "Letting NET_GetPacket drain this queue misroutes
	 * the raw bytes and starves the real consumer").
	 *
	 * This function now always returns qfalse.  The NET_Event while-loop
	 * that calls it is retained for compatibility but is a no-op. */
#if !defined(DEDICATED)
	if ( WN_ClientGetPacket( from, message ) )
		return qtrue;
#endif
	(void)from; (void)message;
	return qfalse;
}

/*
==================
WN_GetConnHandleByAddr

Look up a conn_handle_t for a network address. Returns CONN_INVALID when
no active game connection exists for that address. Type-agnostic (NA_QUIC
and NA_IP both match the underlying IP bytes).

Used in sv_client.c / sv_main.c to replace NET_IS_QUIC() checks.
==================
*/
conn_handle_t WN_GetConnHandleByAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->active || !gc->conn )
			continue;
		{
			const netadr_t *a = &gc->conn->addr;
			if ( NET_IS_IPV6( a->type ) && NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._6, addr->ipv._6, 16 ) == 0 )
					return (conn_handle_t)(i + 1);
			} else if ( !NET_IS_IPV6( a->type ) && !NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._4, addr->ipv._4, 4 ) == 0 )
					return (conn_handle_t)(i + 1);
			}
		}
	}
	return CONN_INVALID;
}

/*
==================
WN_GetAddrByConnHandle

Fill *out with the remote address for the given conn_handle.
Returns qtrue on success, qfalse when the handle is invalid.
==================
*/
qboolean WN_GetAddrByConnHandle( conn_handle_t conn, netadr_t *out )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE ) {
		if ( wtcl.initialized ) {
			*out = wtcl.server_addr;
			return qtrue;
		}
		return qfalse;
	}
#endif
	if ( conn != CONN_INVALID && conn <= WN_MAX_CLIENTS ) {
		wn_game_conn_t *gc = &wn.game_conns[conn - 1];
		if ( gc->active && gc->conn ) {
			*out = gc->conn->addr;
			return qtrue;
		}
	}
	return qfalse;
}

/*
==================
WN_DrainPendingConnects

Called from the main thread (WN_ProcessTimers) to consume pending game
connects enqueued by the QUIC I/O thread and call transport->accept_callback.
==================
*/
void WN_DrainPendingConnects( void )
{
	int      i;
	qboolean was_pending[WN_MAX_CLIENTS];

	if ( !transport || !transport->accept_callback )
		return;

	/* Snapshot pending flags before draining.  accept_callback may re-queue
	 * a connection back into pending_connects (e.g., via WN_RequeueConnect
	 * during async spawn); without the snapshot the loop would re-process it
	 * in the same drain cycle. */
	for ( i = 0; i < WN_MAX_CLIENTS; i++ )
		was_pending[i] = wn.pending_connects[i].pending;

	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( was_pending[i] ) {
			conn_handle_t conn = wn.pending_connects[i].conn;
			char          ui[MAX_INFO_STRING];
			Q_strncpyz( ui, wn.pending_connects[i].userinfo, sizeof(ui) );
			wn.pending_connects[i].pending = qfalse;
			transport->accept_callback( conn, ui );
		}
	}
}

/*
==================
WN_RequeueConnect

Re-enqueue a pending game-client connection that could not be admitted yet
(e.g., the server is in mid-spawn and gvm is not yet valid).  The connection
stays alive at the QUIC level; the next WN_DrainPendingConnects call will
retry accept_callback.

Called from SV_OnPlayerConnect when svs.spawn.phase != SPAWN_IDLE.
==================
*/
void WN_RequeueConnect( conn_handle_t conn, const char *userinfo )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( !wn.pending_connects[i].pending ) {
			wn.pending_connects[i].conn    = conn;
			wn.pending_connects[i].pending = qtrue;
			Q_strncpyz( wn.pending_connects[i].userinfo, userinfo,
				sizeof( wn.pending_connects[i].userinfo ) );
			return;
		}
	}
	/* All slots occupied — cannot hold the connection. */
	Com_Printf( "*** WN_RequeueConnect: no free slot for conn %llu — dropping ***\n",
		(unsigned long long)conn );
	if ( transport )
		transport->drop_client( conn, "Server starting up" );
}

/*
==================
WN_DrainPendingReady

Called from the main thread (WN_ProcessTimers) to consume pending READY
events enqueued by the QUIC I/O thread and call transport->ready_callback.
==================
*/
void WN_DrainPendingReady( void )
{
	int i;
	if ( !transport || !transport->ready_callback )
		return;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.pending_ready[i] ) {
			conn_handle_t conn = (conn_handle_t)( i + 1 );
			wn.pending_ready[i] = qfalse;
			transport->ready_callback( conn );
		}
	}
}

#if !defined(DEDICATED)

/*
==================
WN_ClientSendReady

Send TLV 0x05 READY on the session stream (CHAN_SESSION = stream 0) to inform
the server that the client finished loading and is ready to enter the world.
==================
*/
void WN_ClientSendReady( void )
{
	/* TLV: [type:u8=0x05][len_lo:u8=0x00][len_hi:u8=0x00] — no payload */
	byte ready_tlv[3] = { 0x05, 0x00, 0x00 };
	if ( transport )
		transport->send_reliable( CONN_CLIENT_HANDLE, CHAN_SESSION,
			ready_tlv, (int)sizeof( ready_tlv ) );
}

wn_client_state_t wtcl;

static int WN_ClientCallback(
	picoquic_cnx_t            *cnx,
	uint64_t                   stream_id,
	uint8_t                   *bytes,
	size_t                     length,
	picoquic_call_back_event_t event,
	void                      *callback_ctx,
	void                      *v_ctx )
{
	(void)callback_ctx;
	(void)v_ctx;

	WN_DBG( "QUIC client CB: event=%d stream=%llu conn=%s len=%zu\n",
		(int)event, (unsigned long long)stream_id,
		cnx ? "yes" : "no", length );

	switch ( event ) {

	case picoquic_callback_ready:
		Com_DPrintf( "QUIC client: connected to server %s\n",
			NET_AdrToString( &wtcl.server_addr ) );
		{
			/* Send TLV 0x01 CONNECT on session stream 0x00:
			 * version:u16le=0x0100, userinfo_len:u16le, userinfo:bytes */
			uint16_t ulen = (uint16_t)strlen( wtcl.userinfo );
			byte     connect_pl[MAX_INFO_STRING + 4];
			byte     connect_tlv[MAX_INFO_STRING + 8];
			int      tlv_len;

			connect_pl[0] = 0x00; /* version lo */
			connect_pl[1] = 0x01; /* version hi */
			connect_pl[2] = (byte)( ulen & 0xFF );
			connect_pl[3] = (byte)( (ulen >> 8) & 0xFF );
			if ( ulen > 0 )
				memcpy( connect_pl + 4, wtcl.userinfo, ulen );
			tlv_len = TLV_Write( connect_tlv, (int)sizeof(connect_tlv),
				0x01, connect_pl, (uint16_t)( 4 + ulen ) );
			if ( tlv_len > 0 ) {
				picoquic_add_to_stream( cnx, WIREDNET_SESSION_STREAM_ID,
					connect_tlv, (size_t)tlv_len, 0 );
				Com_DPrintf( "QUIC client: sent TLV CONNECT on stream 0x%02X (%d bytes)\n",
					(unsigned)WIREDNET_SESSION_STREAM_ID, tlv_len );
			}
		}
		break;

	case picoquic_callback_stream_data:
	case picoquic_callback_stream_fin:
		if ( stream_id == WIREDNET_SESSION_STREAM_ID ) {
			/* Binary TLV session channel — parse inline (messages are small) */
			const byte *p         = (const byte *)bytes;
			int         remaining = (int)length;

			while ( remaining >= 3 ) {
				uint8_t      tlv_type;
				const byte  *payload;
				uint16_t     plen;

				if ( !TLV_Read( p, remaining, &tlv_type, &payload, &plen ) )
					break; /* incomplete; next callback will bring the rest */

				if ( tlv_type == 0x02 ) { /* ACCEPT */
					wtcl.accept_pending = qtrue;
					Com_DPrintf( "QUIC client: TLV ACCEPT received\n" );
				} else if ( tlv_type == 0x03 ) { /* REFUSE */
					char reason[MAX_INFO_STRING] = "refused";
					if ( plen >= 2 ) {
						uint16_t rlen = (uint16_t)( payload[0] | ( (uint16_t)payload[1] << 8 ) );
						if ( rlen > plen - 2 )
							rlen = (uint16_t)( plen - 2 );
						if ( rlen >= (uint16_t)sizeof(reason) )
							rlen = (uint16_t)( sizeof(reason) - 1 );
						memcpy( reason, payload + 2, rlen );
						reason[rlen] = '\0';
					}
					Com_Printf( S_COLOR_YELLOW "QUIC connect refused: %s\n", reason );
					WN_ClientDisconnect();
					return 0;
				}
				p         += 3 + plen;
				remaining -= 3 + plen;
			}
			break;
		}
		/* Non-session game-reliable message from server.
		 * Each logical message uses its own QUIC stream and starts with a small
		 * game envelope that identifies the semantic channel. */
		wn_reliable_client_consume_stream( stream_id, bytes, (int)length,
			event == picoquic_callback_stream_fin );
		break;

	case picoquic_callback_datagram:
		if ( length <= 0 || length > WN_SNAP_DGRAM_MAX ) {
			Com_DPrintf( "QUIC client: datagram len %zu out of range — dropped\n", length );
			break;
		}
		{
			int            next_head = ( wtcl.recv_head + 1 ) % WN_GAME_QUEUE_SIZE;
			wn_snap_pkt_t *pkt;
			if ( next_head == wtcl.recv_tail ) {
				Com_DPrintf( "QUIC client: recv queue full — datagram dropped\n" );
				break;
			}
			pkt            = &wtcl.recv_queue[wtcl.recv_head];
			pkt->from      = wtcl.server_addr;
			pkt->from.type = ( wtcl.server_addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
			pkt->len       = (int)length;
			memcpy( pkt->data, bytes, length );
			wtcl.recv_head = next_head;
		}
		break;

	case picoquic_callback_close:
	case picoquic_callback_application_close:
	case picoquic_callback_stateless_reset:
		{
			uint64_t local_err  = cnx ? picoquic_get_local_error( cnx )  : 0;
			uint64_t remote_err = cnx ? picoquic_get_remote_error( cnx ) : 0;
			Com_Printf( "QUIC client: connection closed (event=%d local_err=%llu remote_err=%llu)\n",
				(int)event, (unsigned long long)local_err, (unsigned long long)remote_err );
		}
		/* Do NOT call WN_ClientDisconnect() here — we are inside a picoquic
		 * callback fired from picoquic_prepare_packet_ex. Calling picoquic_free
		 * at this point corrupts the splay tree that picoquic_prepare_packet_ex
		 * is still walking (use-after-free → SIGSEGV).
		 * Set pending_disconnect; WN_ClientFlushOutbound checks the flag
		 * after picoquic_prepare_next_packet_ex returns and does cleanup safely. */
		wtcl.pending_disconnect = qtrue;
		break;

	default:
		break;
	}
	return 0;
}

static void WN_ClientFlushOutbound( void )
{
	byte                     send_buf[WN_PACKET_BUF_SIZE];
	size_t                   send_len;
	size_t                   send_msg_size;
	struct sockaddr_storage  addr_to;
	struct sockaddr_storage  addr_from;
	int                      if_index;
	uint64_t                 current_time;
	picoquic_connection_id_t log_cid;
	picoquic_cnx_t          *last_cnx = NULL;
	netadr_t                 to;
	int                      ret;

	if ( !wtcl.initialized || !wtcl.quic )
		return;

	// Clear-before-send contract: anything in net_lastSendError after the
	// loop is from this frame's sends — no timestamp, no race window.
	NET_ClearLastSendError();

	current_time = Sys_Microseconds();

	while ( 1 ) {
		send_len = 0;
		ret = picoquic_prepare_next_packet_ex(
			wtcl.quic, current_time,
			send_buf, sizeof(send_buf), &send_len,
			&addr_to, &addr_from, &if_index,
			&log_cid, &last_cnx, &send_msg_size );

		if ( ret != 0 || send_len == 0 )
			break;

		memset( &to, 0, sizeof(to) );
		if ( addr_to.ss_family == AF_INET ) {
			struct sockaddr_in *v4 = (struct sockaddr_in *)&addr_to;
			to.type = NA_IP;
			memcpy( to.ipv._4, &v4->sin_addr.s_addr, 4 );
			to.port = v4->sin_port;
		}
#if FEAT_IPV6
		else if ( addr_to.ss_family == AF_INET6 ) {
			struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&addr_to;
			to.type     = NA_IP6;
			to.port     = v6->sin6_port;
			to.scope_id = (uint32_t)v6->sin6_scope_id;
			memcpy( to.ipv._6, &v6->sin6_addr, 16 );
		}
#endif
		else {
			continue;
		}
		NET_SendPacket( NS_CLIENT, (int)send_len, send_buf, &to );
	}

	// Promote any sendto error from this frame to the connect-error slot.
	if ( NET_HasLastSendError() && !wtcl.connect_failed ) {
		Com_Printf( "*** WN_ClientFlushOutbound: send error → connect_failed: %s ***\n",
			NET_LastSendError() );
		Q_strncpyz( wtcl.connect_error, NET_LastSendError(),
		            sizeof( wtcl.connect_error ) );
		wtcl.connect_failed = qtrue;
	}

	// Deferred disconnect: if the close callback fired during the loop above,
	// now that we are outside picoquic it is safe to call picoquic_free.
	if ( wtcl.pending_disconnect ) {
		Com_Printf( "*** WN_ClientFlushOutbound: pending_disconnect → WN_ClientDisconnect ***\n" );
		wtcl.pending_disconnect = qfalse;
		WN_ClientDisconnect();
	}
}

/* ═══════════════════════════════════════════════════════════════════════
 * TOFU (Trust On First Use) certificate verifier — client only.
 *
 * On first connect: extract SubjectPublicKeyInfo DER → SHA-256 hex,
 * accept the cert, record "addr:port fingerprint" in known_servers.txt.
 * On subsequent connects: reject if fingerprint changes.
 * cvar wn_cert_verify=0 disables TOFU (dev / offline mode).
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
	ptls_openssl_override_verify_certificate_t base;
	char addr[128];    /* "ip:port" key in known_servers.txt */
} wn_tofu_ctx_t;

static ptls_openssl_verify_certificate_t s_tofu_verif;
static wn_tofu_ctx_t                     s_tofu_ctx;

/* SHA-256 of SubjectPublicKeyInfo DER, hex-encoded into hex_out. */
static void wn_tofu_fingerprint( X509 *cert, char *hex_out, int hex_size )
{
	EVP_PKEY      *pkey;
	unsigned char *der_buf = NULL;
	int            der_len;
	unsigned char  digest[SHA256_DIGEST_LENGTH];
	int            i;

	pkey = X509_get_pubkey( cert );
	if ( !pkey ) {
		Q_strncpyz( hex_out, "error:no-pubkey", hex_size );
		return;
	}
	der_len = i2d_PUBKEY( pkey, &der_buf );
	EVP_PKEY_free( pkey );
	if ( der_len <= 0 || !der_buf ) {
		Q_strncpyz( hex_out, "error:i2d-failed", hex_size );
		return;
	}
	SHA256( der_buf, (size_t)der_len, digest );
	OPENSSL_free( der_buf );

	for ( i = 0; i < SHA256_DIGEST_LENGTH && i * 2 < hex_size - 1; i++ )
		sprintf( hex_out + i * 2, "%02x", (unsigned)digest[i] );
	hex_out[i * 2] = '\0';
}

static void wn_tofu_file_path( char *out, int size )
{
	const char *home = Cvar_VariableString( "fs_homepath" );
	if ( !home || !*home ) home = ".";
	Com_sprintf( out, size, "%s/known_servers.txt", home );
}

/*
 * Returns: 0 = found + fingerprint matches,
 *          1 = not found (first-time connect),
 *         -1 = found but fingerprint mismatch.
 */
static int wn_tofu_check( const char *addr, const char *fp )
{
	char   path[MAX_OSPATH];
	char   line[512];
	FILE  *f;
	size_t alen;

	wn_tofu_file_path( path, sizeof(path) );
	f = fopen( path, "r" );
	if ( !f )
		return 1; /* no file yet — first-time connect */

	alen = strlen( addr );
	while ( fgets( line, sizeof(line), f ) ) {
		char *p = line;
		while ( *p == ' ' || *p == '\t' ) p++;
		if ( *p == '#' || *p == '\n' || *p == '\r' || *p == '\0' )
			continue;
		if ( strncmp( p, addr, alen ) == 0 && (p[alen] == ' ' || p[alen] == '\t') ) {
			char  *stored = p + alen;
			size_t flen;
			while ( *stored == ' ' || *stored == '\t' ) stored++;
			flen = strlen( stored );
			while ( flen > 0 && (stored[flen-1] == '\n' || stored[flen-1] == '\r') )
				stored[--flen] = '\0';
			fclose( f );
			return strcmp( stored, fp ) == 0 ? 0 : -1;
		}
	}
	fclose( f );
	return 1; /* not found */
}

static void wn_tofu_save( const char *addr, const char *fp )
{
	char  path[MAX_OSPATH];
	FILE *f;

	wn_tofu_file_path( path, sizeof(path) );
	f = fopen( path, "a" );
	if ( !f ) {
		Com_Printf( S_COLOR_YELLOW "QUIC TOFU: could not write %s\n", path );
		return;
	}
	fseek( f, 0, SEEK_END );
	if ( ftell( f ) == 0 )
		fprintf( f, "# WiredNet known servers\n# address:port sha256_fingerprint\n" );
	fprintf( f, "%s %s\n", addr, fp );
	fclose( f );
}

static int tofu_override_cb(
	ptls_openssl_override_verify_certificate_t *self,
	ptls_t *tls, int ret, int ossl_ret,
	X509 *cert, STACK_OF(X509) *chain )
{
	wn_tofu_ctx_t *ctx = (wn_tofu_ctx_t *)self;
	char           fp[SHA256_DIGEST_LENGTH * 2 + 1];
	int            chk;

	(void)tls; (void)ret; (void)ossl_ret; (void)chain;

	if ( !cert ) {
		Com_Printf( S_COLOR_RED "QUIC TOFU: server presented no certificate\n" );
		return -1;
	}

	wn_tofu_fingerprint( cert, fp, sizeof(fp) );
	chk = wn_tofu_check( ctx->addr, fp );

	if ( chk == 1 ) {
		Com_Printf( "^5QUIC TOFU: new server %s — trusting (SPKI SHA256: %.16s...)\n",
			ctx->addr, fp );
		wn_tofu_save( ctx->addr, fp );
		return 0;
	} else if ( chk == 0 ) {
		Com_DPrintf( "QUIC TOFU: cert OK for %s\n", ctx->addr );
		return 0;
	} else {
		Com_Printf( S_COLOR_RED
			"QUIC TOFU: certificate changed for %s!\n"
			"  Got SPKI SHA256: %.16s...\n"
			"  Delete entry in known_servers.txt to reconnect.\n",
			ctx->addr, fp );
		return -1;
	}
}

static void tofu_free_fn( ptls_verify_certificate_t *ctx )
{
	ptls_openssl_dispose_verify_certificate( (ptls_openssl_verify_certificate_t *)ctx );
}

void WN_ClientConnect( const netadr_t *serverAddr,
                          const char *userinfo, int qport )
{
	uint64_t              current_time;
	struct sockaddr_storage ss;
	int                   ss_len = 0;

	if ( wtcl.initialized ) {
		Com_DPrintf( "WN_ClientConnect: already connected\n" );
		return;
	}

	memset( &wtcl, 0, sizeof(wtcl) );
	// Reset connect-error state so a retry starts clean (ED1 fix).
	wtcl.connect_failed  = qfalse;
	wtcl.connect_error[0] = '\0';
	Q_strncpyz( wtcl.userinfo, userinfo, sizeof(wtcl.userinfo) );
	wtcl.qport       = qport;
	wtcl.server_addr = *serverAddr;

	current_time = Sys_Microseconds();

	wtcl.quic = picoquic_create(
		1, NULL, NULL, NULL,
		WN_ALPN, NULL, NULL, NULL, NULL, NULL,
		current_time, NULL, NULL, NULL, 0 );

	if ( !wtcl.quic ) {
		Com_Printf( S_COLOR_RED "WN_ClientConnect: picoquic_create failed\n" );
		wtcl.connect_failed = qtrue;
		Q_strncpyz( wtcl.connect_error, "picoquic_create failed",
		            sizeof( wtcl.connect_error ) );
		return;
	}

	/* Certificate verification: TOFU by default; bypassed when wn_cert_verify=0. */
	Cvar_Get( "wn_cert_verify", "1", CVAR_ARCHIVE );
	if ( Cvar_VariableIntegerValue( "wn_cert_verify" ) == 0 ) {
		picoquic_set_null_verifier( wtcl.quic );
	} else {
		/* Build addr string for known_servers.txt lookup.
		 * Handle loopback before the NA_LOOPBACK→127.0.0.1 fixup below. */
		if ( serverAddr->type == NA_LOOPBACK ) {
			Com_sprintf( s_tofu_ctx.addr, sizeof(s_tofu_ctx.addr), "127.0.0.1:%d",
				(int)ntohs( serverAddr->port ) );
		} else if ( serverAddr->type == NA_IP ) {
			Com_sprintf( s_tofu_ctx.addr, sizeof(s_tofu_ctx.addr), "%d.%d.%d.%d:%d",
				(int)serverAddr->ipv._4[0], (int)serverAddr->ipv._4[1],
				(int)serverAddr->ipv._4[2], (int)serverAddr->ipv._4[3],
				(int)ntohs( serverAddr->port ) );
		} else {
			Q_strncpyz( s_tofu_ctx.addr, "unknown", sizeof(s_tofu_ctx.addr) );
		}
		s_tofu_ctx.base.cb = tofu_override_cb;
		ptls_openssl_init_verify_certificate( &s_tofu_verif, NULL );
		s_tofu_verif.override_callback = &s_tofu_ctx.base;
		picoquic_set_verify_certificate_callback( wtcl.quic,
			&s_tofu_verif.super, tofu_free_fn );
	}
	picoquic_set_default_datagram_priority( wtcl.quic, 1 );
	/* Advertise datagram support.  Without this the local_parameters.max_datagram_frame_size
	 * defaults to 0, and any received datagram triggers FRAME_FORMAT_ERROR (error 7) even
	 * though picoquic_queue_datagram_frame only guards large datagrams on the send side. */
	picoquic_set_default_tp_value( wtcl.quic, picoquic_tp_max_datagram_frame_size,
	                               WN_SNAP_DGRAM_MAX );
	/* Loopback (integrated server): client and server are the same process.
	 * An idle timeout would fire during long map loads (WAMR init, bot AI,
	 * AAS load) and force a spurious reconnect.  Use 1 hour — effectively
	 * infinite.  Note: picoquic's idle_timeout=0 path falls through to its
	 * own 30s handshake default, so 0 is NOT "infinite" here; use explicit
	 * large value.  Remote servers keep the 30-second timeout so dead
	 * connections are detected promptly. */
	if ( serverAddr->type == NA_LOOPBACK ) {
		picoquic_set_default_idle_timeout( wtcl.quic, 3600000 );   /* 1 hour */
	} else {
		picoquic_set_default_idle_timeout( wtcl.quic, 30000 );
	}

	memset( &ss, 0, sizeof(ss) );
	if ( serverAddr->type == NA_IP ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, serverAddr->ipv._4, 4 );
		v4->sin_port = serverAddr->port;
		ss_len = sizeof(struct sockaddr_in);
	}
	else if ( serverAddr->type == NA_LOOPBACK ) {
		/* localhost — map to 127.0.0.1 so picoquic can send real UDP datagrams */
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss;
		v4->sin_family      = AF_INET;
		v4->sin_addr.s_addr = htonl( INADDR_LOOPBACK );  /* 127.0.0.1 */
		v4->sin_port        = serverAddr->port;
		ss_len = sizeof(struct sockaddr_in);
		/* update stored addr so WN_ClientCallback route-checks pass */
		wtcl.server_addr.type        = NA_IP;
		wtcl.server_addr.ipv._4[0]   = 127;
		wtcl.server_addr.ipv._4[1]   = 0;
		wtcl.server_addr.ipv._4[2]   = 0;
		wtcl.server_addr.ipv._4[3]   = 1;
		wtcl.server_addr.port        = serverAddr->port;
	}
#if FEAT_IPV6
	else if ( serverAddr->type == NA_IP6 ) {
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&ss;
		v6->sin6_family   = AF_INET6;
		v6->sin6_port     = serverAddr->port;
		v6->sin6_scope_id = serverAddr->scope_id;
		memcpy( &v6->sin6_addr, serverAddr->ipv._6, 16 );
		ss_len = sizeof(struct sockaddr_in6);
	}
#endif
	else {
		Com_Printf( S_COLOR_RED "WN_ClientConnect: unsupported address type %d\n",
			serverAddr->type );
		picoquic_free( wtcl.quic );
		wtcl.quic = NULL;
		wtcl.connect_failed = qtrue;
		Q_strncpyz( wtcl.connect_error,
		            va( "unsupported address type %d", serverAddr->type ),
		            sizeof( wtcl.connect_error ) );
		return;
	}
	(void)ss_len;

	wtcl.cnx = picoquic_create_client_cnx(
		wtcl.quic, (struct sockaddr *)&ss,
		current_time, 0, NULL,
		WN_ALPN, WN_ClientCallback, NULL );

	if ( !wtcl.cnx ) {
		/* picoquic_create_client_cnx already calls picoquic_start_client_cnx internally.
		 * NULL here means TLS init failed (ALPN missing, crypto error, etc.). */
		Com_Printf( S_COLOR_RED "WN_ClientConnect: picoquic_create_client_cnx failed\n" );
		picoquic_free( wtcl.quic );
		wtcl.quic = NULL;
		wtcl.connect_failed = qtrue;
		Q_strncpyz( wtcl.connect_error, "picoquic_create_client_cnx failed (TLS/crypto error)",
		            sizeof( wtcl.connect_error ) );
		return;
	}

	wtcl.initialized = qtrue;
	Com_Printf( "QUIC client: connecting to %s...\n",
		NET_AdrToStringwPort( serverAddr ) );
}

qboolean WN_ClientHasError( char *out, int outSize )
{
	if ( !wtcl.connect_failed )
		return qfalse;
	if ( out && outSize > 0 )
		Q_strncpyz( out, wtcl.connect_error, outSize );
	return qtrue;
}

void WN_ClientClearError( void )
{
	wtcl.connect_failed   = qfalse;
	wtcl.connect_error[0] = '\0';
}

void WN_ClientFrame( void )
{
	if ( !wtcl.initialized || !wtcl.quic )
		return;
	WN_ClientFlushOutbound();
}

void WN_ClientDisconnect( void )
{
	picoquic_cnx_t  *cnx;
	picoquic_quic_t *quic;

	if ( !wtcl.initialized )
		return;

	Com_Printf( "*** WN_ClientDisconnect: called while initialized ***\n" );

	/* Zero state BEFORE calling into picoquic. picoquic_free triggers
	 * picoquic_delete_cnx → picoquic_connection_disconnect → callback_close
	 * → re-enters WN_ClientDisconnect. Without this guard the re-entrant
	 * call sees wtcl.quic != NULL and calls picoquic_free a second time. */
	cnx  = wtcl.cnx;
	quic = wtcl.quic;
	memset( &wtcl, 0, sizeof(wtcl) );   /* clears initialized, cnx, quic */

	if ( cnx )
		picoquic_close( cnx, 0 );
	if ( quic )
		picoquic_free( quic );
}

qboolean WN_ClientIsConnecting( void )
{
	return wtcl.initialized && wtcl.quic != NULL;
}

qboolean WN_ClientCheckPacket( const netadr_t *from, byte *buf, int len )
{
	uint64_t               current_time;
	struct sockaddr_storage ss_from;
	struct sockaddr_in      ss_to;

	if ( !wtcl.initialized || !wtcl.quic )
		return qfalse;
	if ( len <= 0 || len > WN_PACKET_BUF_SIZE )
		return qfalse;

	memset( &ss_from, 0, sizeof(ss_from) );
	if ( from->type == NA_IP ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss_from;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, from->ipv._4, 4 );
		v4->sin_port = from->port;
	}
#if FEAT_IPV6
	else if ( from->type == NA_IP6 ) {
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&ss_from;
		v6->sin6_family   = AF_INET6;
		v6->sin6_port     = from->port;
		v6->sin6_scope_id = from->scope_id;
		memcpy( &v6->sin6_addr, from->ipv._6, 16 );
	}
#endif
	else {
		return qfalse;
	}

	memset( &ss_to, 0, sizeof(ss_to) );
	ss_to.sin_family      = AF_INET;
	ss_to.sin_addr.s_addr = INADDR_ANY;
	ss_to.sin_port        = from->port;

	current_time = Sys_Microseconds();
	memcpy( wtcl.recv_buf, buf, len );

	picoquic_incoming_packet(
		wtcl.quic, wtcl.recv_buf, (size_t)len,
		(struct sockaddr *)&ss_from,
		(struct sockaddr *)&ss_to,
		0, 0, current_time );

	WN_ClientFlushOutbound();
	return qtrue;
}

qboolean WN_ClientGetPacket( netadr_t *from, msg_t *message )
{
	(void)from; (void)message;
	/* recv_queue exclusively owned by wn_recv_unreliable / CL_CheckSnapshotDatagrams.
	 * That path knows to strip the 8-byte tick header prepended by sv_snapshot.c.
	 * Letting NET_GetPacket drain this queue misroutes the raw bytes (tick header
	 * read as svc_ command → garbage → discard) and starves the real consumer. */
	return qfalse;
}

void WN_ClientSendPacket( const netadr_t *to, const void *data, int length )
{
	if ( !wtcl.initialized || !wtcl.cnx ) {
		Com_DPrintf( "WN_ClientSendPacket: no client connection\n" );
		return;
	}
	{
		/* Verify destination matches our server (type-agnostic IP compare) */
		const netadr_t *a     = &wtcl.server_addr;
		qboolean        match = qfalse;
		if ( NET_IS_IPV6( a->type ) && NET_IS_IPV6( to->type ) )
			match = ( memcmp( a->ipv._6, to->ipv._6, 16 ) == 0 );
		else if ( !NET_IS_IPV6( a->type ) && !NET_IS_IPV6( to->type ) )
			match = ( memcmp( a->ipv._4, to->ipv._4, 4 ) == 0 );
		if ( !match ) {
			Com_DPrintf( "WN_ClientSendPacket: address mismatch\n" );
			return;
		}
	}
	picoquic_queue_datagram_frame( wtcl.cnx, (size_t)length, (const uint8_t *)data );
}

#endif /* !DEDICATED */


// ═══════════════════════════════════════════════════════════════════
// Transport vtable implementation
// ═══════════════════════════════════════════════════════════════════

static void wn_init( void )       { /* WN_Init() called directly; sets transport */ }
static void wn_shutdown( void )   { /* WN_Shutdown() called directly */              }
static void wn_frame( int msec )
{
	(void)msec;
	WN_ProcessTimers();
	WN_FlushOutbound();
#if !defined(DEDICATED)
	WN_ClientFrame();
#endif
}

static void wn_listen( int port ) { (void)port; /* QUIC already bound in WN_Init */ }

static void wn_drop_client( conn_handle_t conn, const char *reason )
{
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	Com_Printf( "*** wn_drop_client: conn=%llu reason=%s ***\n",
		(unsigned long long)conn, reason ? reason : "NULL" );
	if ( cnx )
		picoquic_close( cnx, 0 );
}

static conn_handle_t wn_connect( const char *address, int port, const char *userinfo )
{
#if !defined(DEDICATED)
	netadr_t adr;
	/* "loopback" is returned by NET_AdrToString for NA_LOOPBACK addresses.
	 * Pass it through as-is so WN_ClientConnect handles the 127.0.0.1 mapping. */
	if ( !Q_stricmp( address, "loopback" ) ) {
		memset( &adr, 0, sizeof(adr) );
		adr.type = NA_LOOPBACK;
	} else if ( !NET_StringToAdr( address, &adr, NA_IP ) ) {
		return CONN_INVALID;
	}
	adr.port = BigShort( (short)port );
	WN_ClientConnect( &adr, userinfo, 0 );
	return CONN_CLIENT_HANDLE;
#else
	(void)address; (void)port; (void)userinfo;
	return CONN_INVALID;
#endif
}

static void wn_disconnect( conn_handle_t conn, const char *reason )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE ) {
		Com_Printf( "*** wn_disconnect: CONN_CLIENT_HANDLE reason=%s ***\n",
			reason ? reason : "NULL" );
		WN_ClientDisconnect();
		return;
	}
#endif
	wn_drop_client( conn, reason );
}

static void wn_send_unreliable( conn_handle_t conn, const byte *data, int len )
{
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( cnx )
		picoquic_queue_datagram_frame( cnx, (size_t)len, (const uint8_t *)data );
}

/*
 * WN_ServerRecvUsercmd — server-only datagram drain.
 *
 * Reads one datagram from any active game_conn's recv_queue (user commands
 * sent by game clients).  Called directly by SV_DrainQUICUsercmds; never
 * called from the client frame path.
 *
 * Keeping this separate from wn_recv_unreliable prevents the loopback
 * cross-contamination where CL_CheckSnapshotDatagrams would accidentally
 * dequeue user-command datagrams instead of snapshot datagrams.
 */
qboolean WN_ServerRecvUsercmd( conn_handle_t *conn_out, byte *buf, int *len_out )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc  = &wn.game_conns[i];
		wn_game_pkt_t  *pkt;

		if ( !gc->active || gc->recv_tail == gc->recv_head )
			continue;

		pkt = &gc->recv_queue[gc->recv_tail];
		if ( pkt->len > *len_out ) {
			/* oversized — discard */
			gc->recv_tail = ( gc->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
			continue;
		}

		*conn_out = (conn_handle_t)(i + 1);
		*len_out  = pkt->len;
		memcpy( buf, pkt->data, pkt->len );
		gc->recv_tail = ( gc->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
		return qtrue;
	}
	return qfalse;
}

/*
 * wn_recv_unreliable — client-side datagram drain (vtable entry).
 *
 * Called only from CL_CheckSnapshotDatagrams via transport->recv_unreliable.
 * Reads snapshots from wtcl.recv_queue — never touches gc->recv_queue.
 */
static qboolean wn_recv_unreliable( conn_handle_t *conn_out, byte *buf, int *len_out )
{
#if !defined(DEDICATED)
	if ( wtcl.initialized && wtcl.recv_tail != wtcl.recv_head ) {
		wn_snap_pkt_t *pkt = &wtcl.recv_queue[wtcl.recv_tail];
		if ( pkt->len <= *len_out ) {
			*conn_out = CONN_CLIENT_HANDLE;
			*len_out  = pkt->len;
			memcpy( buf, pkt->data, pkt->len );
			wtcl.recv_tail = ( wtcl.recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
			return qtrue;
		}
		/* oversized — discard */
		wtcl.recv_tail = ( wtcl.recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
	}
#else
	(void)conn_out; (void)buf; (void)len_out;
#endif
	return qfalse;
}

static void wn_send_reliable( conn_handle_t conn, int channel,
                              const byte *data, int len )
{
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	uint64_t        stream_id;
	qboolean        sending_from_client;
	byte            framed_inline[MAX_MSGLEN + 2];
	byte           *framed     = framed_inline;
	qboolean        framed_heap = qfalse;
	const byte     *send_data = data;
	int             send_len  = len;
	int ret;
	if ( !cnx || !data || len <= 0 )
		return;
	sending_from_client = ( conn == CONN_CLIENT_HANDLE ) ? qtrue : qfalse;
	stream_id = wn_resolve_reliable_send_stream( cnx, channel, sending_from_client );
	if ( stream_id == UINT64_MAX ) {
		Com_Printf( S_COLOR_YELLOW "QUIC: invalid reliable channel %d for conn %llu\n",
			channel, (unsigned long long)conn );
		return;
	}
	if ( !wn_reliable_channel_allows_fixed_stream( channel ) ) {
		/* CHAN_BOOTSTRAP may exceed MAX_MSGLEN — allow up to WN_BOOTSTRAP_MAX. */
		int max_payload = ( channel == CHAN_BOOTSTRAP ) ? WN_BOOTSTRAP_MAX : MAX_MSGLEN;
		if ( !wn_reliable_channel_is_game( channel ) || len > max_payload ) {
			Com_Printf( S_COLOR_YELLOW "QUIC: invalid reliable payload for channel %d\n",
				channel );
			return;
		}
		if ( channel == CHAN_BOOTSTRAP ) {
			framed = (byte *)Z_Malloc( len + 2 );
			framed_heap = qtrue;
		}
		framed[0] = WN_GAME_REL_VERSION;
		framed[1] = (byte)channel;
		memcpy( framed + 2, data, (size_t)len );
		send_data = framed;
		send_len  = len + 2;
	}
	/* Reliable app messages are one logical message per QUIC stream. Session
	 * control remains on stream 0; other channels use fresh local streams and
	 * rely on stream FIN to delimit the message. */
	ret = picoquic_add_to_stream( cnx, stream_id, send_data, (size_t)send_len,
		wn_reliable_channel_allows_fixed_stream( channel ) ? 0 : 1 );
	if ( framed_heap ) Z_Free( framed );
	WN_DBG( "QUIC: wn_send_reliable conn=%llu channel=%d len=%d ret=%d\n",
		(unsigned long long)conn, channel, len, ret );
}

qboolean WN_ServerRecvReliable( conn_handle_t *conn_out, int *channel_out,
	byte *buf, int *len_out )
{
	int i;
	int channel;

	/* Server-side: drain game_conns rel_recv slots */
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->active )
			continue;
		channel = -1;
		if ( !wn_reliable_queue_pop( gc->rel_queue, &gc->rel_head, &gc->rel_tail,
			&channel, buf, len_out ) ) {
			continue;
		}
		*conn_out    = (conn_handle_t)(i + 1);
		*channel_out = channel;
		return qtrue;
	}

	return qfalse;
}

#if !defined(DEDICATED)
/* Return a pointer to the pending bootstrap payload and mark it consumed.
 * The pointer is valid until the next WN_ClientConnect call.
 * Returns qfalse if no bootstrap is pending. */
qboolean WN_ClientConsumeBootstrap( const byte **data_out, int *len_out )
{
	if ( !wtcl.initialized || !wtcl.bootstrap_recv_ready )
		return qfalse;
	*data_out = wtcl.bootstrap_recv_data;
	*len_out  = wtcl.bootstrap_recv_len;
	wtcl.bootstrap_recv_ready = qfalse;
	wtcl.bootstrap_recv_len   = 0;
	return qtrue;
}

qboolean WN_ClientRecvReliable( int *channel_out, byte *buf, int *len_out )
{
	if ( wtcl.initialized ) {
		if ( wn_reliable_queue_pop( wtcl.rel_queue, &wtcl.rel_head, &wtcl.rel_tail,
			channel_out, buf, len_out ) ) {
			return qtrue;
		}
	}

	return qfalse;
}
#endif

static qboolean wn_recv_reliable( conn_handle_t *conn_out, int *channel_out,
	byte *buf, int *len_out )
{
	if ( WN_ServerRecvReliable( conn_out, channel_out, buf, len_out ) )
		return qtrue;
#if !defined(DEDICATED)
	if ( WN_ClientRecvReliable( channel_out, buf, len_out ) ) {
		if ( conn_out ) *conn_out = CONN_CLIENT_HANDLE;
		return qtrue;
	}
#endif
	return qfalse;
}

static int wn_get_ping( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( !cnx )
		return -1;
	picoquic_get_default_path_quality( cnx, &q );
	return (int)( q.rtt / 1000 );
}

static float wn_get_loss( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( !cnx )
		return 0.0f;
	picoquic_get_default_path_quality( cnx, &q );
	return ( q.sent > 0 ) ? (float)q.lost / (float)q.sent : 0.0f;
}

static int wn_get_bandwidth( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( !cnx )
		return 0;
	picoquic_get_default_path_quality( cnx, &q );
	return (int)( q.receive_rate_estimate * 8 / 1000 );
}

static void wn_get_address_string( conn_handle_t conn, char *buf, int buflen )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE ) {
		Q_strncpyz( buf, NET_AdrToStringwPort( &wtcl.server_addr ), buflen );
		return;
	}
#endif
	{
		wn_game_conn_t *gc = wn_get_game_conn( conn );
		if ( gc && gc->conn )
			Q_strncpyz( buf, NET_AdrToStringwPort( &gc->conn->addr ), buflen );
		else
			Q_strncpyz( buf, "<unknown>", buflen );
	}
}

transport_t quic_transport = {
	wn_init,
	wn_shutdown,
	wn_frame,
	wn_listen,
	NULL,              /* accept_callback — set by caller (Phase B) */
	NULL,              /* ready_callback  — set by caller (Phase B2) */
	wn_drop_client,
	wn_connect,
	wn_disconnect,
	wn_send_unreliable,
	wn_recv_unreliable,
	wn_send_reliable,
	wn_recv_reliable,
	wn_get_ping,
	wn_get_loss,
	wn_get_bandwidth,
	wn_get_address_string,
};
