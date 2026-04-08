/*
===========================================================================
wt_transport.c — QUIC game transport (server + client + vtable)

Replaces wt_game.c and wt_client.c with a unified implementation that
also exports the transport_t vtable used by all engine networking code.

conn_handle_t mapping:
  CONN_INVALID          (0)               — no connection
  1..WT_MAX_CLIENTS                        — wt.game_conns[handle - 1]
  CONN_CLIENT_HANDLE    (WT_MAX_CLIENTS+1) — wtcl (outgoing client cnx)

Legacy API (QUIC_SendGamePacketToAddr, QUIC_GetGamePacket, QUIC_Client*)
is preserved unchanged for Phase A backward compatibility; removed Phase D.
===========================================================================
*/
#include "wt_local.h"
#include "../qcommon/net_transport.h"

#if FEAT_QUIC_TRANSPORT

/* ── Global transport pointer (extern declared in net_transport.h) ── */
transport_t *transport = NULL;

#if FEAT_QUIC_GAME

/* Sentinel handle for the single outgoing client connection */
#define CONN_CLIENT_HANDLE  ((conn_handle_t)(WT_MAX_CLIENTS + 1))

// ═══════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════

static wt_game_conn_t *QT_GetGameConn( conn_handle_t conn )
{
	wt_game_conn_t *gc;
	if ( conn == CONN_INVALID || conn > WT_MAX_CLIENTS )
		return NULL;
	gc = &wt.game_conns[conn - 1];
	return gc->active ? gc : NULL;
}

static picoquic_cnx_t *QT_GetCnx( conn_handle_t conn )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE )
		return ( wtcl.initialized && wtcl.cnx ) ? wtcl.cnx : NULL;
#endif
	{
		wt_game_conn_t *gc = QT_GetGameConn( conn );
		return ( gc && gc->conn ) ? gc->conn->cnx : NULL;
	}
}

static wt_game_conn_t *WT_GameFindConnForAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_game_conn_t *gc = &wt.game_conns[i];
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

/*
 * Simple JSON string-value extractor — no malloc, no external JSON dep.
 * Finds "key":"value" in a flat JSON object and copies value to buf.
 */
static qboolean WT_JSONGetString( const char *json, int json_len,
                                   const char *key,
                                   char *buf, int buf_size )
{
	char        search[64];
	int         slen;
	const char *p   = json;
	const char *end = json + json_len;

	Com_sprintf( search, sizeof(search), "\"%s\"", key );
	slen = (int)strlen( search );

	while ( p < end - slen ) {
		if ( memcmp( p, search, slen ) == 0 ) {
			p += slen;
			while ( p < end && ( *p == ' ' || *p == ':' ) ) p++;
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


// ═══════════════════════════════════════════════════════════════════
// Game conn lifecycle  (was wt_game.c)
// ═══════════════════════════════════════════════════════════════════

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


// ═══════════════════════════════════════════════════════════════════
// Datagram handling  (was wt_game.c)
// ═══════════════════════════════════════════════════════════════════

void WT_GameHandleDatagram( wt_connection_t *conn, const byte *data, int len )
{
	wt_game_conn_t *gc;
	int             next_head;
	wt_game_pkt_t  *pkt;

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
	pkt           = &gc->recv_queue[gc->recv_head];
	pkt->from     = conn->addr;
	pkt->from.type = ( conn->addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
	pkt->len      = len;
	Com_Memcpy( pkt->data, data, len );
	gc->recv_head = next_head;
}


// ═══════════════════════════════════════════════════════════════════
// Handshake — Stream 0x01  (was wt_game.c)
// ═══════════════════════════════════════════════════════════════════

void WT_GameHandleHandshake( wt_connection_t *conn, uint64_t stream_id,
                              const byte *data, int len )
{
	char            type[32];
	char            userinfo[MAX_INFO_STRING];
	char            qport_str[8];
	wt_game_conn_t *gc;

	if ( !conn || !conn->active )
		return;

	if ( !WT_JSONGetString( (const char *)data, len, "type", type, sizeof(type) ) ) {
		Com_DPrintf( "QUIC game: handshake missing 'type' — ignored\n" );
		return;
	}
	if ( Q_stricmp( type, "connect" ) != 0 ) {
		Com_DPrintf( "QUIC game: unknown handshake type '%s'\n", type );
		return;
	}

	gc = conn->game_conn;
	if ( !gc ) {
		gc = WT_GameAllocConn( conn );
		if ( !gc ) {
			const char *refuse = "{\"type\":\"refuse\",\"reason\":\"server full\"}";
			picoquic_add_to_stream( conn->cnx, stream_id,
				(const uint8_t *)refuse, strlen( refuse ), 1 );
			return;
		}
	}

	gc->handshake_stream_id = stream_id;
	gc->hs_state            = WT_GAME_HS_PENDING;

	if ( !WT_JSONGetString( (const char *)data, len, "userinfo",
	                         userinfo, sizeof(userinfo) ) )
		Q_strncpyz( userinfo, "\\name\\unnamed", sizeof(userinfo) );
	if ( !WT_JSONGetString( (const char *)data, len, "qport",
	                         qport_str, sizeof(qport_str) ) )
		Q_strncpyz( qport_str, "0", sizeof(qport_str) );

	{
		char          pktbuf[MAX_INFO_STRING + 64];
		int           pktlen, next_head;
		wt_game_pkt_t *pkt;

		Com_sprintf( pktbuf, sizeof(pktbuf),
			"\xff\xff\xff\xff" "connect %s %s", userinfo, qport_str );
		pktlen    = (int)strlen( pktbuf ) + 1;
		next_head = ( gc->recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
		if ( next_head == gc->recv_tail ) {
			Com_Printf( S_COLOR_YELLOW
				"QUIC game: recv queue full, cannot enqueue connect from %s\n",
				NET_AdrToStringwPort( &conn->addr ) );
			return;
		}
		pkt           = &gc->recv_queue[gc->recv_head];
		pkt->from     = conn->addr;
		pkt->from.type = ( conn->addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
		pkt->len      = pktlen;
		Com_Memcpy( pkt->data, pktbuf, pktlen );
		gc->recv_head = next_head;
	}
	Com_DPrintf( "QUIC game: enqueued connect from %s\n",
		NET_AdrToStringwPort( &conn->addr ) );
}


// ═══════════════════════════════════════════════════════════════════
// Public game packet API  (was wt_game.c)
// ═══════════════════════════════════════════════════════════════════

void QUIC_SendGamePacketToAddr( const netadr_t *to, const void *data, int length )
{
	wt_game_conn_t *gc;

	if ( !wt.initialized || !wt.quic ) {
#if !defined(DEDICATED)
		goto try_client;
#else
		Com_DPrintf( "QUIC_SendGamePacketToAddr: QUIC not initialized\n" );
		return;
#endif
	}

	gc = WT_GameFindConnForAddr( to );
	if ( gc && gc->conn && gc->conn->cnx ) {
		picoquic_queue_datagram_frame( gc->conn->cnx, (size_t)length,
		                               (const uint8_t *)data );
		return;
	}

#if !defined(DEDICATED)
try_client:
	QUIC_ClientSendPacket( to, data, length );
#else
	Com_DPrintf( "QUIC_SendGamePacketToAddr: no game conn for %s\n",
		NET_AdrToStringwPort( to ) );
#endif
}

qboolean QUIC_GetGamePacket( netadr_t *from, msg_t *message )
{
	int i;
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_game_conn_t *gc  = &wt.game_conns[i];
		wt_game_pkt_t  *pkt;

		if ( !gc->active || gc->recv_tail == gc->recv_head )
			continue;

		pkt = &gc->recv_queue[gc->recv_tail];
		if ( pkt->len > message->maxsize ) {
			Com_Printf( "QUIC_GetGamePacket: oversized %d > %d — discarded\n",
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
	if ( QUIC_ClientGetPacket( from, message ) )
		return qtrue;
#endif
	return qfalse;
}

/*
==================
QUIC_GetConnHandleByAddr

Look up a conn_handle_t for a network address. Returns CONN_INVALID when
no active game connection exists for that address. Type-agnostic (NA_QUIC
and NA_IP both match the underlying IP bytes).

Used in sv_client.c / sv_main.c to replace NET_IS_QUIC() checks.
==================
*/
conn_handle_t QUIC_GetConnHandleByAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_game_conn_t *gc = &wt.game_conns[i];
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


// ═══════════════════════════════════════════════════════════════════
// Client-side QUIC context  (was wt_client.c)
// ═══════════════════════════════════════════════════════════════════

#if !defined(DEDICATED)

wt_client_state_t wtcl;

static int WT_ClientCallback(
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

	switch ( event ) {

	case picoquic_callback_ready:
		Com_Printf( "QUIC client: connected to server %s\n",
			NET_AdrToString( &wtcl.server_addr ) );
		{
			char buf[MAX_INFO_STRING + 64];
			int  len;
			Com_sprintf( buf, sizeof(buf),
				"{\"type\":\"connect\",\"userinfo\":\"%s\",\"qport\":\"%d\"}",
				wtcl.userinfo, wtcl.qport );
			len = (int)strlen( buf );
			picoquic_add_to_stream( cnx, QUIC_GAME_STREAM_ID,
				(const uint8_t *)buf, (size_t)len, 0 );
			Com_DPrintf( "QUIC client: sent connect on stream 0x%02X (%d bytes)\n",
				(unsigned)QUIC_GAME_STREAM_ID, len );
		}
		break;

	case picoquic_callback_stream_data:
	case picoquic_callback_stream_fin:
		if ( stream_id != QUIC_GAME_STREAM_ID ) {
			Com_DPrintf( "QUIC client: unexpected stream 0x%llX data\n",
				(unsigned long long)stream_id );
			break;
		}
		if ( wtcl.hs_len + (int)length < (int)sizeof(wtcl.hs_buf) - 1 ) {
			Com_Memcpy( wtcl.hs_buf + wtcl.hs_len, bytes, length );
			wtcl.hs_len += (int)length;
			wtcl.hs_buf[wtcl.hs_len] = '\0';
		}
		if ( event != picoquic_callback_stream_fin )
			break;

		Com_DPrintf( "QUIC client: stream 0x01 response: %.*s\n",
			wtcl.hs_len, wtcl.hs_buf );
		{
			char     type[32];
			qboolean ok = qfalse;
			{
				const char *p   = wtcl.hs_buf;
				const char *end = p + wtcl.hs_len;
				while ( p < end - 8 ) {
					if ( memcmp( p, "\"type\":", 7 ) == 0 ) {
						int i = 0;
						p += 7;
						while ( p < end && ( *p == ' ' || *p == '"' ) ) p++;
						while ( p < end && *p != '"' && i < (int)sizeof(type) - 1 )
							type[i++] = *p++;
						type[i] = '\0';
						ok = qtrue;
						break;
					}
					p++;
				}
			}
			if ( !ok || Q_stricmp( type, "connectResponse" ) != 0 ) {
				char reason[128] = "refused";
				const char *p = strstr( wtcl.hs_buf, "\"reason\":\"" );
				if ( p ) {
					const char *q;
					p += 10;
					q = strchr( p, '"' );
					if ( q ) {
						int rlen = (int)(q - p);
						if ( rlen >= (int)sizeof(reason) ) rlen = (int)sizeof(reason) - 1;
						Com_Memcpy( reason, p, rlen );
						reason[rlen] = '\0';
					}
				}
				Com_Printf( S_COLOR_YELLOW "QUIC connect refused: %s\n", reason );
				QUIC_ClientDisconnect();
				break;
			}
			{
				char          pktbuf[64];
				int           pktlen, next_head;
				wt_game_pkt_t *pkt;
				Com_sprintf( pktbuf, sizeof(pktbuf),
					"\xff\xff\xff\xff" "connectResponse 0" );
				pktlen    = (int)strlen( pktbuf ) + 1;
				next_head = ( wtcl.recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
				if ( next_head == wtcl.recv_tail ) {
					Com_Printf( S_COLOR_YELLOW
						"QUIC client: recv queue full, cannot enqueue connectResponse\n" );
					break;
				}
				pkt           = &wtcl.recv_queue[wtcl.recv_head];
				pkt->from     = wtcl.server_addr;
				pkt->from.type = ( wtcl.server_addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
				pkt->len      = pktlen;
				Com_Memcpy( pkt->data, pktbuf, pktlen );
				wtcl.recv_head = next_head;
			}
		}
		break;

	case picoquic_callback_datagram:
		if ( length <= 0 || length > WT_GAME_PKT_MAX ) {
			Com_DPrintf( "QUIC client: datagram len %zu out of range — dropped\n", length );
			break;
		}
		{
			int            next_head = ( wtcl.recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
			wt_game_pkt_t *pkt;
			if ( next_head == wtcl.recv_tail ) {
				Com_DPrintf( "QUIC client: recv queue full — datagram dropped\n" );
				break;
			}
			pkt            = &wtcl.recv_queue[wtcl.recv_head];
			pkt->from      = wtcl.server_addr;
			pkt->from.type = ( wtcl.server_addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
			pkt->len       = (int)length;
			Com_Memcpy( pkt->data, bytes, length );
			wtcl.recv_head = next_head;
		}
		break;

	case picoquic_callback_close:
	case picoquic_callback_application_close:
	case picoquic_callback_stateless_reset:
		Com_Printf( "QUIC client: server closed connection (event=%d)\n", (int)event );
		QUIC_ClientDisconnect();
		break;

	default:
		break;
	}
	return 0;
}

static void QUIC_ClientFlushOutbound( void )
{
	byte                     send_buf[WT_PACKET_BUF_SIZE];
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

		Com_Memset( &to, 0, sizeof(to) );
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
}

void QUIC_ClientConnect( const netadr_t *serverAddr,
                          const char *userinfo, int qport )
{
	uint64_t              current_time;
	struct sockaddr_storage ss;
	int                   ss_len = 0;

	if ( wtcl.initialized ) {
		Com_DPrintf( "QUIC_ClientConnect: already connected\n" );
		return;
	}

	Com_Memset( &wtcl, 0, sizeof(wtcl) );
	Q_strncpyz( wtcl.userinfo, userinfo, sizeof(wtcl.userinfo) );
	wtcl.qport       = qport;
	wtcl.server_addr = *serverAddr;

	current_time = Sys_Microseconds();

	wtcl.quic = picoquic_create(
		1, NULL, NULL, NULL,
		WT_ALPN, NULL, NULL, NULL, NULL, NULL,
		current_time, NULL, NULL, NULL, 0 );

	if ( !wtcl.quic ) {
		Com_Printf( S_COLOR_RED "QUIC_ClientConnect: picoquic_create failed\n" );
		return;
	}

	picoquic_set_null_verifier( wtcl.quic );
	picoquic_set_default_datagram_priority( wtcl.quic, 1 );
	picoquic_set_default_idle_timeout( wtcl.quic, 30000 );

	memset( &ss, 0, sizeof(ss) );
	if ( serverAddr->type == NA_IP ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, serverAddr->ipv._4, 4 );
		v4->sin_port = serverAddr->port;
		ss_len = sizeof(struct sockaddr_in);
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
		Com_Printf( S_COLOR_RED "QUIC_ClientConnect: unsupported address type %d\n",
			serverAddr->type );
		picoquic_free( wtcl.quic );
		wtcl.quic = NULL;
		return;
	}
	(void)ss_len;

	wtcl.cnx = picoquic_create_client_cnx(
		wtcl.quic, (struct sockaddr *)&ss,
		current_time, 0, NULL,
		WT_ALPN, WT_ClientCallback, NULL );

	if ( !wtcl.cnx ) {
		Com_Printf( S_COLOR_RED "QUIC_ClientConnect: picoquic_create_client_cnx failed\n" );
		picoquic_free( wtcl.quic );
		wtcl.quic = NULL;
		return;
	}

	if ( picoquic_start_client_cnx( wtcl.cnx ) != 0 ) {
		Com_Printf( S_COLOR_RED "QUIC_ClientConnect: picoquic_start_client_cnx failed\n" );
		picoquic_close( wtcl.cnx, 0 );
		wtcl.cnx = NULL;
		picoquic_free( wtcl.quic );
		wtcl.quic = NULL;
		return;
	}

	wtcl.initialized = qtrue;
	Com_Printf( "QUIC client: connecting to %s...\n",
		NET_AdrToStringwPort( serverAddr ) );
}

void QUIC_ClientFrame( void )
{
	if ( !wtcl.initialized || !wtcl.quic )
		return;
	QUIC_ClientFlushOutbound();
}

void QUIC_ClientDisconnect( void )
{
	if ( !wtcl.initialized )
		return;
	if ( wtcl.cnx ) {
		picoquic_close( wtcl.cnx, 0 );
		wtcl.cnx = NULL;
	}
	if ( wtcl.quic ) {
		picoquic_free( wtcl.quic );
		wtcl.quic = NULL;
	}
	Com_Memset( &wtcl, 0, sizeof(wtcl) );
}

qboolean QUIC_ClientIsConnecting( void )
{
	return wtcl.initialized && wtcl.quic != NULL;
}

qboolean QUIC_ClientCheckPacket( const netadr_t *from, byte *buf, int len )
{
	uint64_t               current_time;
	struct sockaddr_storage ss_from;
	struct sockaddr_in      ss_to;

	if ( !wtcl.initialized || !wtcl.quic )
		return qfalse;
	if ( len <= 0 || len > WT_PACKET_BUF_SIZE )
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
	Com_Memcpy( wtcl.recv_buf, buf, len );

	picoquic_incoming_packet(
		wtcl.quic, wtcl.recv_buf, (size_t)len,
		(struct sockaddr *)&ss_from,
		(struct sockaddr *)&ss_to,
		0, 0, current_time );

	QUIC_ClientFlushOutbound();
	return qtrue;
}

qboolean QUIC_ClientGetPacket( netadr_t *from, msg_t *message )
{
	wt_game_pkt_t *pkt;

	if ( !wtcl.initialized || wtcl.recv_tail == wtcl.recv_head )
		return qfalse;

	pkt = &wtcl.recv_queue[wtcl.recv_tail];
	if ( pkt->len > message->maxsize ) {
		Com_Printf( "QUIC client: oversized %d > %d — discarded\n",
			pkt->len, message->maxsize );
		wtcl.recv_tail = ( wtcl.recv_tail + 1 ) % WT_GAME_QUEUE_SIZE;
		return qfalse;
	}
	*from            = pkt->from;
	message->cursize = pkt->len;
	Com_Memcpy( message->data, pkt->data, pkt->len );
	wtcl.recv_tail = ( wtcl.recv_tail + 1 ) % WT_GAME_QUEUE_SIZE;
	return qtrue;
}

void QUIC_ClientSendPacket( const netadr_t *to, const void *data, int length )
{
	if ( !wtcl.initialized || !wtcl.cnx ) {
		Com_DPrintf( "QUIC_ClientSendPacket: no client connection\n" );
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
			Com_DPrintf( "QUIC_ClientSendPacket: address mismatch\n" );
			return;
		}
	}
	picoquic_queue_datagram_frame( wtcl.cnx, (size_t)length, (const uint8_t *)data );
}

#endif /* !DEDICATED */


// ═══════════════════════════════════════════════════════════════════
// Transport vtable implementation
// ═══════════════════════════════════════════════════════════════════

static void QT_Init( void )       { /* QUIC_Init() called directly; sets transport */ }
static void QT_Shutdown( void )   { /* QUIC_Shutdown() called directly */              }
static void QT_Frame( int msec )
{
	(void)msec;
	QUIC_ProcessTimers();
	QUIC_FlushOutbound();
#if !defined(DEDICATED)
	QUIC_ClientFrame();
#endif
}

static void QT_Listen( int port ) { (void)port; /* QUIC already bound in QUIC_Init */ }

static void QT_DropClient( conn_handle_t conn, const char *reason )
{
	picoquic_cnx_t *cnx = QT_GetCnx( conn );
	(void)reason;
	if ( cnx )
		picoquic_close( cnx, 0 );
}

static conn_handle_t QT_Connect( const char *address, int port, const char *userinfo )
{
#if !defined(DEDICATED)
	netadr_t adr;
	if ( !NET_StringToAdr( address, &adr, NA_IP ) )
		return CONN_INVALID;
	adr.port = BigShort( (short)port );
	QUIC_ClientConnect( &adr, userinfo, 0 );
	return CONN_CLIENT_HANDLE;
#else
	(void)address; (void)port; (void)userinfo;
	return CONN_INVALID;
#endif
}

static void QT_Disconnect( conn_handle_t conn, const char *reason )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE ) {
		QUIC_ClientDisconnect();
		return;
	}
#endif
	QT_DropClient( conn, reason );
}

static void QT_SendUnreliable( conn_handle_t conn, const byte *data, int len )
{
	picoquic_cnx_t *cnx = QT_GetCnx( conn );
	if ( cnx )
		picoquic_queue_datagram_frame( cnx, (size_t)len, (const uint8_t *)data );
}

static qboolean QT_RecvUnreliable( conn_handle_t *conn_out, byte *buf, int *len_out )
{
	/* Phase B: implement; Phase A still uses QUIC_GetGamePacket path */
	(void)conn_out; (void)buf; (void)len_out;
	return qfalse;
}

static void QT_SendReliable( conn_handle_t conn, int channel,
                              const byte *data, int len )
{
	/* Phase B: implement stream send per channel */
	(void)conn; (void)channel; (void)data; (void)len;
}

static qboolean QT_RecvReliable( conn_handle_t *conn_out, int *channel_out,
                                  byte *buf, int *len_out )
{
	/* Phase B: implement stream recv */
	(void)conn_out; (void)channel_out; (void)buf; (void)len_out;
	return qfalse;
}

static int QT_GetPing( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = QT_GetCnx( conn );
	if ( !cnx )
		return -1;
	picoquic_get_default_path_quality( cnx, &q );
	return (int)( q.rtt / 1000 );
}

static float QT_GetLoss( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = QT_GetCnx( conn );
	if ( !cnx )
		return 0.0f;
	picoquic_get_default_path_quality( cnx, &q );
	return ( q.sent > 0 ) ? (float)q.lost / (float)q.sent : 0.0f;
}

static int QT_GetBandwidth( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = QT_GetCnx( conn );
	if ( !cnx )
		return 0;
	picoquic_get_default_path_quality( cnx, &q );
	return (int)( q.receive_rate_estimate * 8 / 1000 );
}

static void QT_GetAddressString( conn_handle_t conn, char *buf, int buflen )
{
#if !defined(DEDICATED)
	if ( conn == CONN_CLIENT_HANDLE ) {
		Q_strncpyz( buf, NET_AdrToStringwPort( &wtcl.server_addr ), buflen );
		return;
	}
#endif
	{
		wt_game_conn_t *gc = QT_GetGameConn( conn );
		if ( gc && gc->conn )
			Q_strncpyz( buf, NET_AdrToStringwPort( &gc->conn->addr ), buflen );
		else
			Q_strncpyz( buf, "<unknown>", buflen );
	}
}

transport_t quic_transport = {
	QT_Init,
	QT_Shutdown,
	QT_Frame,
	QT_Listen,
	NULL,              /* accept_callback — set by caller (Phase B) */
	QT_DropClient,
	QT_Connect,
	QT_Disconnect,
	QT_SendUnreliable,
	QT_RecvUnreliable,
	QT_SendReliable,
	QT_RecvReliable,
	QT_GetPing,
	QT_GetLoss,
	QT_GetBandwidth,
	QT_GetAddressString,
};

#endif /* FEAT_QUIC_GAME */
#endif /* FEAT_QUIC_TRANSPORT */
