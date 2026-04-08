/*
===========================================================================
wt_client.c — QUIC game transport, CLIENT side

Creates a picoquic CLIENT context and manages the outgoing connection to
a remote game server.  Symmetric with the server side implemented in
wt_game.c + wt_main.c.

Flow:
  1. QUIC_ClientConnect()  — picoquic CLIENT context created; QUIC TLS
                             handshake starts asynchronously
  2. WT_ClientCallback / picoquic_callback_ready
                           — stream 0x01 opened; JSON connect sent
  3. WT_ClientCallback / picoquic_callback_stream_data (stream 0x01)
                           — JSON connectResponse parsed; synthesized OOB
                             packet queued → NET_Event → CL_PacketEvent →
                             Netchan_Setup (remoteAddress.type = NA_QUIC)
  4. Netchan datagrams     — picoquic_callback_datagram → recv_queue;
                             drain via QUIC_ClientGetPacket in NET_Event
  5. Outbound netchan      — Netchan_Transmit → Sys_SendPacket (NA_QUIC)
                             → QUIC_ClientSendPacket → datagram frame
===========================================================================
*/
#include "wt_local.h"

#if FEAT_QUIC_GAME && !defined(DEDICATED)

wt_client_state_t wtcl;

// ───────────────────────────────────────────────────────────────────
// picoquic client callback
// ───────────────────────────────────────────────────────────────────

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
		/* QUIC TLS handshake complete — open Stream 0x01 (game handshake) */
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
		/* Stream 0x01: server connectResponse / refuse */
		if ( stream_id != QUIC_GAME_STREAM_ID ) {
			Com_DPrintf( "QUIC client: unexpected stream 0x%llX data\n",
				(unsigned long long)stream_id );
			break;
		}

		/* Buffer the response (may arrive in fragments) */
		if ( wtcl.hs_len + (int)length < (int)sizeof(wtcl.hs_buf) - 1 ) {
			Com_Memcpy( wtcl.hs_buf + wtcl.hs_len, bytes, length );
			wtcl.hs_len += (int)length;
			wtcl.hs_buf[ wtcl.hs_len ] = '\0';
		}

		/* Only process when FIN arrives (complete message) */
		if ( event != picoquic_callback_stream_fin )
			break;

		Com_DPrintf( "QUIC client: stream 0x01 response: %.*s\n",
			wtcl.hs_len, wtcl.hs_buf );

		{
			/* Quick JSON type check */
			char type[32];
			qboolean ok = qfalse;

			/* Simple scan: find "type":"X" */
			{
				const char *p = wtcl.hs_buf;
				const char *end = p + wtcl.hs_len;
				while ( p < end - 8 ) {
					if ( memcmp( p, "\"type\":", 7 ) == 0 ) {
						p += 7;
						while ( p < end && (*p == ' ' || *p == '"') ) p++;
						{
							int i = 0;
							while ( p < end && *p != '"' && i < (int)sizeof(type)-1 )
								type[i++] = *p++;
							type[i] = '\0';
						}
						ok = qtrue;
						break;
					}
					p++;
				}
			}

			if ( !ok || Q_stricmp( type, "connectResponse" ) != 0 ) {
				/* Server refused */
				char reason[128] = "refused";
				{
					const char *p = strstr( wtcl.hs_buf, "\"reason\":\"" );
					if ( p ) {
						p += 10;
						const char *q = strchr( p, '"' );
						if ( q ) {
							int len = (int)(q - p);
							if ( len >= (int)sizeof(reason) ) len = (int)sizeof(reason)-1;
							Com_Memcpy( reason, p, len );
							reason[len] = '\0';
						}
					}
				}
				Com_Printf( S_COLOR_YELLOW "QUIC connect refused: %s\n", reason );
				QUIC_ClientDisconnect();
				break;
			}

			/* Synthesize OOB connectResponse packet:
			   "\xff\xff\xff\xffconnectResponse"
			   We use challenge=0; sv_client.c already skips challenge
			   verification for NA_QUIC addresses. */
			{
				char         pktbuf[64];
				int          pktlen;
				int          next_head;
				wt_game_pkt_t *pkt;

				Com_sprintf( pktbuf, sizeof(pktbuf),
					"\xff\xff\xff\xff" "connectResponse 0" );
				pktlen = (int)strlen( pktbuf ) + 1;

				next_head = ( wtcl.recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
				if ( next_head == wtcl.recv_tail ) {
					Com_Printf( S_COLOR_YELLOW
						"QUIC client: recv queue full, cannot enqueue connectResponse\n" );
					break;
				}

				pkt          = &wtcl.recv_queue[wtcl.recv_head];
				pkt->from    = wtcl.server_addr;
				pkt->from.type = ( wtcl.server_addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
				pkt->len     = pktlen;
				Com_Memcpy( pkt->data, pktbuf, pktlen );
				wtcl.recv_head = next_head;
			}
		}
		break;

	case picoquic_callback_datagram:
		/* Netchan datagram from server → enqueue for NET_Event / CL_PacketEvent */
		if ( length <= 0 || length > WT_GAME_PKT_MAX ) {
			Com_DPrintf( "QUIC client: datagram len %zu out of range — dropped\n", length );
			break;
		}
		{
			int next_head = ( wtcl.recv_head + 1 ) % WT_GAME_QUEUE_SIZE;
			if ( next_head == wtcl.recv_tail ) {
				Com_DPrintf( "QUIC client: recv queue full — datagram dropped\n" );
				break;
			}
			wt_game_pkt_t *pkt = &wtcl.recv_queue[wtcl.recv_head];
			pkt->from          = wtcl.server_addr;
			pkt->from.type     = ( wtcl.server_addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
			pkt->len           = (int)length;
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


// ───────────────────────────────────────────────────────────────────
// Flush outbound: pull packets from picoquic client and send via UDP
// ───────────────────────────────────────────────────────────────────

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
			wtcl.quic,
			current_time,
			send_buf,
			sizeof( send_buf ),
			&send_len,
			&addr_to,
			&addr_from,
			&if_index,
			&log_cid,
			&last_cnx,
			&send_msg_size
		);

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
			to.type = NA_IP6;
			memcpy( to.ipv._6, &v6->sin6_addr, 16 );
			to.port = v6->sin6_port;
			to.scope_id = (uint32_t)v6->sin6_scope_id;
		}
#endif
		else {
			continue;
		}

		NET_SendPacket( NS_CLIENT, (int)send_len, send_buf, &to );
	}
}


// ───────────────────────────────────────────────────────────────────
// Public API
// ───────────────────────────────────────────────────────────────────

/*
==================
QUIC_ClientConnect

Initiate a QUIC client connection to serverAddr.
Creates a new picoquic CLIENT context (separate from the server's wt.quic).
The TLS handshake runs asynchronously; WT_ClientCallback fires on ready.
==================
*/
void QUIC_ClientConnect( const netadr_t *serverAddr,
                          const char *userinfo, int qport )
{
	uint64_t              current_time;
	struct sockaddr_storage ss;
	struct sockaddr_in     *v4;
#if FEAT_IPV6
	struct sockaddr_in6    *v6;
#endif
	int                    ss_len;

	if ( wtcl.initialized ) {
		Com_DPrintf( "QUIC_ClientConnect: already connected\n" );
		return;
	}

	Com_Memset( &wtcl, 0, sizeof(wtcl) );

	Q_strncpyz( wtcl.userinfo, userinfo, sizeof(wtcl.userinfo) );
	wtcl.qport      = qport;
	wtcl.server_addr = *serverAddr;

	current_time = Sys_Microseconds();

	/* Create client-mode picoquic context (no server cert needed) */
	wtcl.quic = picoquic_create(
		1,              // max connections (we only connect to 1 server)
		NULL,           // no server cert
		NULL,           // no server key
		NULL,           // no root cert (skip verification)
		WT_ALPN,        // ALPN "q3v69"
		NULL,           // default callback (set per-cnx below)
		NULL,           // default callback context
		NULL,           // connection ID callback
		NULL,           // connection ID context
		NULL,           // reset seed
		current_time,
		NULL,           // simulated time
		NULL,           // ticket file
		NULL,           // ticket encryption key
		0               // ticket encryption key length
	);

	if ( !wtcl.quic ) {
		Com_Printf( S_COLOR_RED "QUIC_ClientConnect: picoquic_create failed\n" );
		return;
	}

	/* Accept any server certificate — game servers use self-signed certs */
	picoquic_set_null_verifier( wtcl.quic );

	/* Enable QUIC datagrams on the client context */
	picoquic_set_default_datagram_priority( wtcl.quic, 1 );
	picoquic_set_default_idle_timeout( wtcl.quic, 30000 );

	/* Build sockaddr for the server */
	memset( &ss, 0, sizeof(ss) );
	if ( serverAddr->type == NA_IP ) {
		v4 = (struct sockaddr_in *)&ss;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, serverAddr->ipv._4, 4 );
		v4->sin_port = serverAddr->port;
		ss_len = sizeof(struct sockaddr_in);
	}
#if FEAT_IPV6
	else if ( serverAddr->type == NA_IP6 ) {
		v6 = (struct sockaddr_in6 *)&ss;
		v6->sin6_family = AF_INET6;
		memcpy( &v6->sin6_addr, serverAddr->ipv._6, 16 );
		v6->sin6_port = serverAddr->port;
		v6->sin6_scope_id = serverAddr->scope_id;
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

	/* Create the client connection */
	wtcl.cnx = picoquic_create_client_cnx(
		wtcl.quic,
		(struct sockaddr *)&ss,
		current_time,
		0,                 // preferred_version (0 = let picoquic pick)
		NULL,              // sni (NULL = no SNI)
		WT_ALPN,
		WT_ClientCallback,
		NULL               // callback context
	);

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


/*
==================
QUIC_ClientFrame

Drive picoquic client timers and flush outbound packets.
Called from NET_Event every main-loop iteration.
==================
*/
void QUIC_ClientFrame( void )
{
	if ( !wtcl.initialized || !wtcl.quic )
		return;

	QUIC_ClientFlushOutbound();
}


/*
==================
QUIC_ClientDisconnect

Close the QUIC client connection and release resources.
==================
*/
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


/*
==================
QUIC_ClientIsConnecting

Returns qtrue if a QUIC client connection is active (handshaking or ready).
==================
*/
qboolean QUIC_ClientIsConnecting( void )
{
	return wtcl.initialized && wtcl.quic != NULL;
}


/*
==================
QUIC_ClientCheckPacket

Feed a received UDP packet to the picoquic CLIENT context.
Called from QUIC_CheckPacket (wt_main.c) after feeding the server context.
The client context will ignore packets that don't belong to its connections.
==================
*/
qboolean QUIC_ClientCheckPacket( const netadr_t *from, byte *buf, int len )
{
	uint64_t current_time;
	struct sockaddr_storage ss_from;
	struct sockaddr_in      ss_to;

	if ( !wtcl.initialized || !wtcl.quic )
		return qfalse;

	if ( len <= 0 || len > WT_PACKET_BUF_SIZE )
		return qfalse;

	/* Build sockaddr for the remote */
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
		v6->sin6_family = AF_INET6;
		memcpy( &v6->sin6_addr, from->ipv._6, 16 );
		v6->sin6_port = from->port;
		v6->sin6_scope_id = from->scope_id;
	}
#endif
	else {
		return qfalse;
	}

	memset( &ss_to, 0, sizeof(ss_to) );
	ss_to.sin_family = AF_INET;
	ss_to.sin_addr.s_addr = INADDR_ANY;
	ss_to.sin_port = from->port;

	current_time = Sys_Microseconds();

	/* Copy to private buffer — caller's buffer may be reused immediately */
	Com_Memcpy( wtcl.recv_buf, buf, len );

	picoquic_incoming_packet(
		wtcl.quic,
		wtcl.recv_buf,
		(size_t)len,
		(struct sockaddr *)&ss_from,
		(struct sockaddr *)&ss_to,
		0, 0,
		current_time
	);

	QUIC_ClientFlushOutbound();
	return qtrue;  // consumed (even if picoquic ignored it — harmless)
}


/*
==================
QUIC_ClientGetPacket

Dequeue one packet from the client receive queue.
Called from NET_Event after QUIC_GetGamePacket (server side).
Returns qtrue if a packet was dequeued.
==================
*/
qboolean QUIC_ClientGetPacket( netadr_t *from, msg_t *message )
{
	wt_game_pkt_t *pkt;

	if ( !wtcl.initialized || wtcl.recv_tail == wtcl.recv_head )
		return qfalse;

	pkt = &wtcl.recv_queue[wtcl.recv_tail];

	if ( pkt->len > message->maxsize ) {
		Com_Printf( "QUIC client: oversized packet %d > %d — discarded\n",
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


/*
==================
QUIC_ClientSendPacket

Send a netchan datagram to the server via the client QUIC connection.
Called from QUIC_SendGamePacketToAddr when the destination matches the
server address and there is no server-side game_conn (client mode).
==================
*/
void QUIC_ClientSendPacket( const netadr_t *to, const void *data, int length )
{
	if ( !wtcl.initialized || !wtcl.cnx ) {
		Com_DPrintf( "QUIC_ClientSendPacket: no client connection\n" );
		return;
	}

	if ( !NET_CompareBaseAdr( to, &(netadr_t){ .type = wtcl.server_addr.type,
	                                            .port = to->port,
	                                            .ipv = wtcl.server_addr.ipv } ) ) {
		Com_DPrintf( "QUIC_ClientSendPacket: address mismatch\n" );
		return;
	}

	picoquic_queue_datagram_frame( wtcl.cnx, (size_t)length, (const uint8_t *)data );
}

#endif /* FEAT_QUIC_GAME && !DEDICATED */
