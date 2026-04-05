/*
===========================================================================
wt_main.c — QUIC transport lifecycle

QUIC_Init / QUIC_Shutdown / frame processing hooks.
picoquic context management and the main callback dispatcher.
===========================================================================
*/
#include "wt_local.h"
#include "../server/server.h"  // for netadr_t details, NS_SERVER

#if FEAT_QUIC_TRANSPORT

// Global QUIC state — singleton
wt_state_t wt;

/*
====================
WT_AlpnSelectCallback

ALPN selection callback for picoquic server.
Accepts "q3v69" ALPN. Returns the index of the matching ALPN in the list,
or SIZE_MAX if no match.
====================
*/
static size_t WT_AlpnSelectCallback( picoquic_quic_t *quic,
                                      picoquic_iovec_t *list, size_t count )
{
	size_t i;
	(void)quic;

	for ( i = 0; i < count; i++ ) {
		if ( list[i].len == 5 && memcmp( list[i].base, "q3v69", 5 ) == 0 ) {
			Com_Printf( "QUIC: ALPN matched 'q3v69' at index %zu\n", i );
			return i;
		}
	}

	// Also accept "h3" for compatibility testing
	for ( i = 0; i < count; i++ ) {
		if ( list[i].len == 2 && memcmp( list[i].base, "h3", 2 ) == 0 ) {
			Com_Printf( "QUIC: ALPN matched 'h3' at index %zu\n", i );
			return i;
		}
	}

	Com_Printf( "QUIC: no matching ALPN found (%zu offered)\n", count );
	return SIZE_MAX;
}


/*
====================
WT_NetadrToSockaddr

Convert Q3 netadr_t to POSIX sockaddr_storage for picoquic.
Returns the sockaddr length (sizeof(sockaddr_in) or sizeof(sockaddr_in6)).
====================
*/
static int WT_NetadrToSockaddr( const netadr_t *a, struct sockaddr_storage *s )
{
	memset( s, 0, sizeof( *s ) );

	if ( a->type == NA_IP || a->type == NA_BROADCAST ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)s;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, a->ipv._4, 4 );
		v4->sin_port = a->port;
		return sizeof( struct sockaddr_in );
	}
#if FEAT_IPV6
	if ( a->type == NA_IP6 || a->type == NA_MULTICAST6 ) {
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)s;
		v6->sin6_family = AF_INET6;
		memcpy( &v6->sin6_addr, a->ipv._6, 16 );
		v6->sin6_port = a->port;
		v6->sin6_scope_id = a->scope_id;
		return sizeof( struct sockaddr_in6 );
	}
#endif

	return 0;
}

/*
====================
WT_PicoquicCallback

Main picoquic callback — dispatches events per connection/stream.
picoquic calls this from picoquic_incoming_packet() processing.
Single-threaded, no locking needed.
====================
*/
static int WT_PicoquicCallback(
	picoquic_cnx_t *cnx,
	uint64_t stream_id,
	uint8_t *bytes,
	size_t length,
	picoquic_call_back_event_t event,
	void *callback_ctx,
	void *stream_ctx )
{
	wt_connection_t *conn = (wt_connection_t *)callback_ctx;

	// If callback_ctx is NULL but we have a connection for this cnx, find it.
	// This happens when picoquic fires multiple events during one incoming_packet
	// call — the first event (ready) sets the context, but subsequent events in
	// the same batch may still have the old NULL context.
	if ( !conn && cnx ) {
		conn = WT_FindConnection( cnx );
	}

	Com_DPrintf( "QUIC CB: event=%d stream=%llu conn=%s len=%zu\n",
		(int)event, (unsigned long long)stream_id,
		conn ? "yes" : "NULL", length );

	switch ( event ) {
	case picoquic_callback_almost_ready:
		// Connection almost ready — allocate state early so stream data handlers work
		if ( !conn ) {
			conn = WT_AllocConnection( cnx, NULL );
			if ( conn ) {
				picoquic_set_callback( cnx, WT_PicoquicCallback, conn );
			} else {
				picoquic_close( cnx, PICOQUIC_ERROR_SERVER_BUSY );
			}
		}
		break;

	case picoquic_callback_ready:
		// Connection fully established
		if ( !conn ) {
			conn = WT_AllocConnection( cnx, NULL );
			if ( conn ) {
				picoquic_set_callback( cnx, WT_PicoquicCallback, conn );
			} else {
				picoquic_close( cnx, PICOQUIC_ERROR_SERVER_BUSY );
				break;
			}
		}
		WT_LogConnect( conn );
		break;

	case picoquic_callback_stream_data:
	case picoquic_callback_stream_fin:
		if ( !conn || !conn->active ) {
			Com_Printf( "QUIC: stream data on stream %llu but no connection — dropping\n",
				(unsigned long long)stream_id );
			break;
		}

		Com_Printf( "QUIC: stream data: stream=%llu len=%zu\n",
			(unsigned long long)stream_id, length );

		// Stream 0x00 = control/capability negotiation
		if ( stream_id == 0x00 ) {
			WT_HandleCapabilityNegotiation( conn, stream_id, bytes, (int)length );
			break;
		}

		// Route other client-initiated bidi streams by content sniffing:
		// HTTP requests start with "GET " or "POST ", everything else is MCP (JSON-RPC).
		if ( (stream_id & 0x03) == 0x00 && stream_id >= 0x04 ) {
#if FEAT_QUIC_HTTP
			if ( length >= 4 && (
				memcmp( bytes, "GET ", 4 ) == 0 ||
				memcmp( bytes, "POST", 4 ) == 0 ||
				memcmp( bytes, "HEAD", 4 ) == 0 ) ) {
				WT_HttpHandleRequest( conn, stream_id, bytes, (int)length );
				break;
			}
#endif
#if FEAT_QUIC_CONTROL
			WT_McpHandleMessage( conn, stream_id, bytes, (int)length );
#endif
			break;
		}
		break;

	case picoquic_callback_close:
	case picoquic_callback_application_close: {
		uint64_t err_code = picoquic_get_local_error(cnx);
		uint64_t remote_err = picoquic_get_remote_error(cnx);
		Com_Printf( "QUIC: close event=%d local_err=%llu remote_err=%llu\n",
			(int)event, (unsigned long long)err_code, (unsigned long long)remote_err );
		if ( conn && conn->active ) {
			char reason[128];
			Com_sprintf( reason, sizeof(reason), "event=%d local=%llu remote=%llu",
				(int)event, (unsigned long long)err_code, (unsigned long long)remote_err );
			WT_LogDisconnect( conn, reason );
			WT_FreeConnection( conn );
		}
		picoquic_set_callback( cnx, NULL, NULL );
		break;
	}

	case picoquic_callback_request_alpn_list:
		Com_Printf( "QUIC: ALPN list requested\n" );
		// picoquic handles ALPN matching via the default_alpn parameter in picoquic_create
		break;

	case picoquic_callback_set_alpn:
		Com_Printf( "QUIC: ALPN negotiated\n" );
		break;

	case picoquic_callback_stateless_reset:
		Com_Printf( "QUIC: stateless reset received\n" );
		if ( conn && conn->active ) {
			WT_LogDisconnect( conn, "stateless reset" );
			WT_FreeConnection( conn );
		}
		picoquic_set_callback( cnx, NULL, NULL );
		return PICOQUIC_ERROR_DETECTED;
		break;

	case picoquic_callback_datagram:
		// Client→server datagrams (not used in v0 — server sends datagrams)
		break;

	case picoquic_callback_prepare_datagram:
#if FEAT_QUIC_OBSERVE
		// Just-in-time datagram encoding — write game state directly into packet
		if ( conn && conn->active && length > 0 ) {
			int encoded = WT_EncodeStateUpdate( bytes, (int)length );
			if ( encoded > 0 ) {
				// picoquic uses the return value to know the actual datagram size
				// (set via picoquic_provide_datagram_buffer_ex in newer API)
			}
		}
#endif
		break;

	case picoquic_callback_datagram_acked:
	case picoquic_callback_datagram_lost:
	case picoquic_callback_datagram_spurious:
		// Telemetry — track datagram delivery rate
		break;

	default:
		break;
	}

	return 0;
}


/*
====================
QUIC_Init

Create picoquic context with self-signed cert.
Called from SV_Init.
====================
*/
void QUIC_Init( void )
{
	uint64_t current_time;

	Com_Memset( &wt, 0, sizeof( wt ) );

	// Register cvars
	wt.sv_quic          = Cvar_Get( "sv_quic",          "1", CVAR_ARCHIVE );
	wt.sv_quicAuthToken = Cvar_Get( "sv_quicAuthToken",  "", CVAR_ARCHIVE );
	wt.sv_quicMaxClients = Cvar_Get( "sv_quicMaxClients", "8", CVAR_ARCHIVE );
	wt.sv_quicStateRate = Cvar_Get( "sv_quicStateRate", "10", CVAR_ARCHIVE );
	wt.sv_quicEventRate = Cvar_Get( "sv_quicEventRate", "20", CVAR_ARCHIVE );

	if ( !wt.sv_quic->integer ) {
		Com_Printf( "QUIC transport disabled (sv_quic 0).\n" );
		return;
	}

	current_time = Sys_Microseconds();

	// Create picoquic context with TLS certificate
	// Look for cert/key files in fs_homepath/certs/ or use cvars
	{
		const char *cert_file = Cvar_VariableString( "sv_quicCertFile" );
		const char *key_file  = Cvar_VariableString( "sv_quicKeyFile" );

		// Default paths if not configured
		if ( !cert_file || !*cert_file )
			cert_file = "certs/cert.pem";
		if ( !key_file || !*key_file )
			key_file = "certs/key.pem";

		Cvar_Get( "sv_quicCertFile", "certs/cert.pem", CVAR_ARCHIVE );
		Cvar_Get( "sv_quicKeyFile", "certs/key.pem", CVAR_ARCHIVE );

		Com_Printf( "QUIC: using cert=%s key=%s\n", cert_file, key_file );

	wt.quic = picoquic_create(
		wt.sv_quicMaxClients->integer,  // max connections
		cert_file,                       // cert file
		key_file,                        // key file
		NULL,                            // cert root file
		WT_ALPN,                         // ALPN: "q3v69"
		WT_PicoquicCallback,             // stream callback
		NULL,                            // default callback context
		NULL,                            // connection ID callback
		NULL,                            // connection ID context
		NULL,                            // reset seed
		(uint64_t)current_time,          // start time
		NULL,                            // simulated time (NULL = real time)
		NULL,                            // ticket file
		NULL,                            // ticket encryption key
		0                                // ticket encryption key length
	);

	} // end cert block

	Com_Printf( "QUIC: picoquic_create returned %p\n", (void*)wt.quic );

	if ( !wt.quic ) {
		Com_Printf( S_COLOR_RED "QUIC_Init: picoquic_create failed. QUIC disabled.\n" );
		return;
	}

	// ALPN selection callback — required for server-side ALPN negotiation
	picoquic_set_alpn_select_fn_v2( wt.quic, WT_AlpnSelectCallback );

	// Enable QUIC datagrams (RFC 9221)
	picoquic_set_default_datagram_priority( wt.quic, 1 );

	// Set idle timeout to 30 seconds (default may be too short)
	picoquic_set_default_idle_timeout( wt.quic, 30000 );

	wt.initialized = qtrue;

#if FEAT_QUIC_OBSERVE
	WT_RecordInit();
#endif

	Com_Printf( "QUIC transport initialized. ALPN: %s, max clients: %d\n",
		WT_ALPN, wt.sv_quicMaxClients->integer );
}


/*
====================
QUIC_Shutdown

Graceful shutdown — send APPLICATION_CLOSE to all clients, then destroy context.
Called from SV_Shutdown.
====================
*/
void QUIC_Shutdown( void )
{
	int i;

	if ( !wt.initialized )
		return;

#if FEAT_QUIC_OBSERVE
	WT_RecordShutdown();
#endif

	// Graceful close — notify all connected clients
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		if ( wt.connections[i].active && wt.connections[i].cnx ) {
			picoquic_close( wt.connections[i].cnx, 0 );
			WT_LogDisconnect( &wt.connections[i], "server shutdown" );
			WT_FreeConnection( &wt.connections[i] );
		}
	}

	// Flush any remaining outbound packets (final close frames)
	QUIC_FlushOutbound();

	picoquic_free( wt.quic );
	wt.quic = NULL;
	wt.initialized = qfalse;

	Com_Printf( "QUIC transport shut down.\n" );
}


/*
====================
QUIC_ProcessTimers

Drive picoquic's internal timers (retransmit, idle, keepalive).
Called from SV_Frame.
====================
*/
void QUIC_ProcessTimers( void )
{
	if ( !wt.initialized )
		return;

	// picoquic handles timers internally when we call prepare_next_packet.
	// We just need to make sure FlushOutbound is called, which sends
	// any timer-driven packets (ACKs, retransmits, keepalives).
}


/*
====================
QUIC_FlushOutbound

Pull all pending outbound packets from picoquic and send them via NET_SendPacket.
Called BOTH after recv drain (fast ACK path) AND from SV_Frame (timer path).

  picoquic_prepare_next_packet_ex
    ├── returns outbound packet in send_buf
    ├── provides destination address
    └── returns 0 + send_len=0 when no more packets pending

This is the core "packet pump" — picoquic never touches the socket directly.
====================
*/
void QUIC_FlushOutbound( void )
{
	byte              send_buf[WT_PACKET_BUF_SIZE];
	size_t            send_len;
	size_t            send_msg_size;
	struct sockaddr_storage addr_to;
	struct sockaddr_storage addr_from;
	int               if_index;
	uint64_t          current_time;
	netadr_t          to;
	int               ret;
	picoquic_connection_id_t log_cid;
	picoquic_cnx_t   *last_cnx = NULL;

	if ( !wt.initialized )
		return;

	current_time = Sys_Microseconds();

	while ( 1 ) {
		send_len = 0;
		ret = picoquic_prepare_next_packet_ex(
			wt.quic,
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

		// Convert sockaddr_storage to Q3 netadr_t
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
			continue; // unknown address family, skip
		}

		Com_DPrintf( "QUIC: sending %d bytes to %s\n", (int)send_len, NET_AdrToString(&to) );
		NET_SendPacket( NS_SERVER, (int)send_len, send_buf, &to );
	}
}


/*
====================
QUIC_CheckPacket

Demux helper — check if a received UDP packet is QUIC.
Called from NET_GetPacket for each recvfrom result (IPv4, IPv6, multicast).

Returns qtrue if the packet was consumed by QUIC.

Demux logic:
  1. 4-byte 0xFFFFFFFF → Q3 connectionless (checked BEFORE this function)
  2. Byte 0 & 0x80     → QUIC Long Header (Initial, Handshake, 0-RTT, Retry)
  3. (Byte 0 & 0xC0) == 0x40 → QUIC Short Header (1-RTT)
  4. Otherwise           → Netchan
====================
*/
qboolean QUIC_CheckPacket( netadr_t *from, byte *buf, int len )
{
	uint64_t current_time;
	uint8_t  first;

	if ( !wt.initialized || !wt.sv_quic->integer )
		return qfalse;

	if ( len <= 0 )
		return qfalse;

	first = buf[0];

	// QUIC Long Header: bit 7 set (Initial, Handshake, 0-RTT, Retry)
	// QUIC Short Header: bits 7:6 = 01 (1-RTT established connection)
	if ( (first & 0x80) || (first & 0xC0) == 0x40 ) {
		Com_DPrintf( "QUIC: demux hit, first=0x%02X len=%d from=%s\n",
			first, len, NET_AdrToString( from ) );
		current_time = Sys_Microseconds();

		// Copy packet data to QUIC-owned buffer — net_message->data is shared
		// and may be reused by subsequent recvfrom calls in the same drain loop.
		if ( len > WT_PACKET_BUF_SIZE ) {
			Com_DPrintf( "QUIC: oversized packet (%d bytes), dropped\n", len );
			wt.dropped_packets++;
			return qtrue;  // consumed (dropped)
		}
		Com_Memcpy( wt.recv_buf, buf, len );

		// Convert netadr_t to sockaddr for picoquic
		{
			struct sockaddr_storage ss_from;
			int ss_len = WT_NetadrToSockaddr( from, &ss_from );
			if ( ss_len == 0 ) {
				wt.dropped_packets++;
				return qtrue;
			}

			// addr_to = server's local address. picoquic uses this for
			// connection routing. Use INADDR_ANY on the server port.
			struct sockaddr_in ss_to;
			memset(&ss_to, 0, sizeof(ss_to));
			ss_to.sin_family = AF_INET;
			ss_to.sin_addr.s_addr = INADDR_ANY;
			ss_to.sin_port = htons(Cvar_VariableIntegerValue("net_port"));

			int pq_ret = picoquic_incoming_packet(
				wt.quic,
				wt.recv_buf,
				(size_t)len,
				(struct sockaddr *)&ss_from,
				(struct sockaddr *)&ss_to,
				0,                          // if_index
				0,                          // ECN
				current_time
			);
			Com_DPrintf( "QUIC: picoquic_incoming_packet returned %d\n", pq_ret );
		}

		// Dual-flush: send ACKs immediately after receiving QUIC packets.
		// Without this, ACKs are delayed until the next SV_Frame (50ms at 20Hz),
		// exceeding QUIC's 25ms max_ack_delay and causing unnecessary retransmits.
		Com_DPrintf( "QUIC: dual-flush after incoming packet\n" );
		QUIC_FlushOutbound();

		return qtrue;
	}

	// Not a QUIC packet — fall through to Netchan handling
	return qfalse;
}


/*
====================
Console commands
====================
*/

#if FEAT_QUIC_TRANSPORT
static void QUIC_Status_f( void )
{
	int i;
	int count = 0;
	uint64_t now = Sys_Microseconds();

	if ( !wt.initialized ) {
		Com_Printf( "QUIC transport not initialized.\n" );
		return;
	}

	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		if ( wt.connections[i].active )
			count++;
	}

	Com_Printf( "QUIC connections: %d/%d\n", count, wt.sv_quicMaxClients->integer );

	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		wt_connection_t *c = &wt.connections[i];
		picoquic_path_quality_t quality;
		uint64_t uptime_sec;
		float loss_pct;
		const char *conn_type, *role, *auth;

		if ( !c->active ) continue;

		uptime_sec = (now - c->connect_time) / 1000000;
		conn_type = WT_HasPermPlayer(c->perm)  ? "PLAYER"   : "OBSERVER";
		role      = WT_HasPermLeader(c->perm)   ? "LEADER"   : "MEMBER";
		auth      = WT_HasPermAdmin(c->perm)    ? "ADMIN"    : "USER";

		// Get RTT and loss stats from picoquic
		Com_Memset( &quality, 0, sizeof(quality) );
		if ( c->cnx ) {
			picoquic_get_path_quality( c->cnx, 0, &quality );
		}
		loss_pct = quality.sent > 0 ? (float)quality.lost * 100.0f / (float)quality.sent : 0.0f;

		Com_Printf( "  #%d  %s  %s+%s+%s  RTT:%llums  Loss:%.1f%%  Up:%lum%lus\n",
			i,
			NET_AdrToString( &c->addr ),
			conn_type, role, auth,
			(unsigned long long)(quality.rtt / 1000), // μs → ms
			loss_pct,
			(unsigned long)(uptime_sec / 60),
			(unsigned long)(uptime_sec % 60) );
	}

	Com_Printf( "Events emitted: %llu\n", (unsigned long long)wt.event_seq );
	Com_Printf( "Dropped packets (misroute): %d\n", wt.dropped_packets );
}
#endif


/*
====================
QUIC_RegisterCommands

Called after QUIC_Init to register console commands.
====================
*/
void QUIC_RegisterCommands( void )
{
	Cmd_AddCommand( "quic_status", QUIC_Status_f );
}

#endif // FEAT_QUIC_TRANSPORT
