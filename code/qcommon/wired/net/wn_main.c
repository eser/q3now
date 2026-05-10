/*
===========================================================================
wn_main.c — QUIC transport lifecycle

WN_Init / WN_Shutdown / frame processing hooks.
picoquic context management and the main callback dispatcher.
===========================================================================
*/
#include "wn_local.h"
#include "../../../server/server.h"  // for netadr_t details, NS_SERVER

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/asn1.h>
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_network, "network" );

#if FEAT_WIREDNET_OBSERVER
// Platform socket headers for the TCP HTTP listener
#  ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    define wn_tcp_close(fd)  closesocket((SOCKET)(fd))
#  else
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <fcntl.h>
#    include <unistd.h>
#    define wn_tcp_close(fd)  close(fd)
#  endif
#endif

// Global QUIC state — singleton
wn_state_t wn;

// Forward declaration: demux handler registered at WN_Init
static qboolean WN_DemuxPacket( const netadr_t *from, const byte *data, int len );
static transport_demux_t s_quic_demux = { WN_DemuxPacket, "quic" };

/*
====================
WN_GenerateSelfSignedCert

Generate a self-signed EC P-256 certificate and write it (PEM) to
cert_path and key_path. Uses OpenSSL libcrypto — already linked via
picoquic → picotls → OpenSSL::Crypto. No external process required.
====================
*/
static qboolean WN_GenerateSelfSignedCert( const char *cert_path, const char *key_path )
{
	EVP_PKEY *pkey = NULL;
	X509     *cert = NULL;
	X509_NAME *name = NULL;
	FILE     *f = NULL;
	qboolean  ok = qfalse;

	/* Generate EC P-256 key (EVP_EC_gen requires OpenSSL 3.0+, already satisfied) */
	pkey = EVP_EC_gen( "P-256" );
	if ( !pkey ) {
		COM_WARN( LOG_CH(ch_network), "QUIC: EVP_EC_gen failed\n" );
		goto cleanup;
	}

	cert = X509_new();
	if ( !cert ) goto cleanup;

	X509_set_version( cert, 2 );                            /* v3 */
	ASN1_INTEGER_set( X509_get_serialNumber( cert ), 1 );
	X509_gmtime_adj( X509_getm_notBefore( cert ), 0 );
	X509_gmtime_adj( X509_getm_notAfter( cert ), 3650L * 24 * 60 * 60 );  /* 10 years */
	X509_set_pubkey( cert, pkey );

	name = X509_get_subject_name( cert );
	X509_NAME_add_entry_by_txt( name, "CN", MBSTRING_ASC,
		(unsigned char *)"wired-server", -1, -1, 0 );
	X509_set_issuer_name( cert, name );  /* self-signed */

	if ( !X509_sign( cert, pkey, EVP_sha256() ) ) {
		COM_WARN( LOG_CH(ch_network), "QUIC: X509_sign failed\n" );
		goto cleanup;
	}

	f = fopen( key_path, "wb" );
	if ( !f ) {
		COM_WARN( LOG_CH(ch_network), "QUIC: cannot write key to %s\n", key_path );
		goto cleanup;
	}
	PEM_write_PrivateKey( f, pkey, NULL, NULL, 0, NULL, NULL );
	fclose( f );
	f = NULL;

	f = fopen( cert_path, "wb" );
	if ( !f ) {
		COM_WARN( LOG_CH(ch_network), "QUIC: cannot write cert to %s\n", cert_path );
		goto cleanup;
	}
	PEM_write_X509( f, cert );
	fclose( f );
	f = NULL;

	ok = qtrue;

cleanup:
	if ( f )    fclose( f );
	if ( cert ) X509_free( cert );
	if ( pkey ) EVP_PKEY_free( pkey );
	return ok;
}

/*
====================
WN_AlpnSelectCallback

ALPN selection callback for picoquic server.
Accepts "q3v69" ALPN. Returns the index of the matching ALPN in the list,
or SIZE_MAX if no match.
====================
*/
static size_t WN_AlpnSelectCallback( picoquic_quic_t *quic,
                                      picoquic_iovec_t *list, size_t count )
{
	size_t i;
	(void)quic;

	for ( i = 0; i < count; i++ ) {
		if ( list[i].len == 5 && memcmp( list[i].base, "q3v69", 5 ) == 0 ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC: ALPN matched 'q3v69' at index %zu\n", i );
			return i;
		}
	}

	// Also accept "h3" for compatibility testing
	for ( i = 0; i < count; i++ ) {
		if ( list[i].len == 2 && memcmp( list[i].base, "h3", 2 ) == 0 ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC: ALPN matched 'h3' at index %zu\n", i );
			return i;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: no matching ALPN found (%zu offered)\n", count );
	return SIZE_MAX;
}


/*
====================
WN_NetadrToSockaddr

Convert Q3 netadr_t to POSIX sockaddr_storage for picoquic.
Returns the sockaddr length (sizeof(sockaddr_in) or sizeof(sockaddr_in6)).
====================
*/
static int WN_NetadrToSockaddr( const netadr_t *a, struct sockaddr_storage *s )
{
	memset( s, 0, sizeof( *s ) );

	if ( a->type == NA_IP || a->type == NA_BROADCAST || a->type == NA_QUIC ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)s;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, a->ipv._4, 4 );
		v4->sin_port = a->port;
		return sizeof( struct sockaddr_in );
	}
#if FEAT_IPV6
	if ( a->type == NA_IP6 || a->type == NA_MULTICAST6 || a->type == NA_QUIC6 ) {
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
WN_PicoquicCallback

Main picoquic callback — dispatches events per connection/stream.
picoquic calls this from picoquic_incoming_packet() processing.
Single-threaded, no locking needed.
====================
*/
static int WN_PicoquicCallback(
	picoquic_cnx_t *cnx,
	uint64_t stream_id,
	uint8_t *bytes,
	size_t length,
	picoquic_call_back_event_t event,
	void *callback_ctx,
	void *stream_ctx )
{
	wn_connection_t *conn = (wn_connection_t *)callback_ctx;

	// If callback_ctx is NULL but we have a connection for this cnx, find it.
	// This happens when picoquic fires multiple events during one incoming_packet
	// call — the first event (ready) sets the context, but subsequent events in
	// the same batch may still have the old NULL context.
	if ( !conn && cnx ) {
		conn = WN_FindConnection( cnx );
	}

	Com_Log( SEV_TRACE, LOG_CH(ch_network), "[WiredNet] QUIC CB: event=%d stream=%llu conn=%s len=%zu\n",
		(int)event, (unsigned long long)stream_id,
		conn ? "yes" : "NULL", length );

	switch ( event ) {
	case picoquic_callback_almost_ready:
		// Connection almost ready — allocate state early so stream data handlers work
		if ( !conn ) {
			conn = WN_AllocConnection( cnx, NULL );
			if ( conn ) {
				picoquic_set_callback( cnx, WN_PicoquicCallback, conn );
			} else {
				picoquic_close( cnx, PICOQUIC_ERROR_SERVER_BUSY );
			}
		}
		break;

	case picoquic_callback_ready:
		// Connection fully established
		if ( !conn ) {
			conn = WN_AllocConnection( cnx, NULL );
			if ( conn ) {
				picoquic_set_callback( cnx, WN_PicoquicCallback, conn );
			} else {
				picoquic_close( cnx, PICOQUIC_ERROR_SERVER_BUSY );
				break;
			}
		}
		WN_LogConnect( conn );
		break;

	case picoquic_callback_stream_data:
	case picoquic_callback_stream_fin:
		if ( !conn || !conn->active ) {
			Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: stream data on stream %llu but no connection — dropping\n",
				(unsigned long long)stream_id );
			break;
		}

		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC: stream data: stream=%llu len=%zu\n",
			(unsigned long long)stream_id, length );

		// Stream 0x00 = session control channel (binary TLV: CONNECT/ACCEPT/REFUSE/READY)
		if ( stream_id == 0x00 ) {
			WN_HandleCapabilityNegotiation( conn, stream_id, bytes, (int)length );
			break;
		}

		/* Game reliable traffic from clients uses client-initiated unidirectional
		 * streams dedicated to the game transport. MCP/HTTP stay on bidi streams. */
		if ( conn->game_conn && ( stream_id & 0x03 ) == 0x02 ) {
			WN_GameHandleReliable( conn, stream_id, bytes, (int)length,
				event == picoquic_callback_stream_fin );
			break;
		}

		// Route other client-initiated bidi streams by content sniffing:
		// HTTP requests start with "GET " or "POST ", everything else is MCP (JSON-RPC).
		if ( (stream_id & 0x03) == 0x00 && stream_id >= 0x04 ) {
#if FEAT_WIREDNET_OBSERVER
			if ( length >= 4 && (
				memcmp( bytes, "GET ", 4 ) == 0 ||
				memcmp( bytes, "POST", 4 ) == 0 ||
				memcmp( bytes, "HEAD", 4 ) == 0 ) ) {
				WN_HttpHandleRequest( conn, stream_id, bytes, (int)length );
				break;
			}
#endif
#if FEAT_WIREDNET_CONTROL
			WN_McpHandleMessage( conn, stream_id, bytes, (int)length );
#endif
			break;
		}
		break;

	case picoquic_callback_close:
	case picoquic_callback_application_close: {
		uint64_t err_code = picoquic_get_local_error(cnx);
		uint64_t remote_err = picoquic_get_remote_error(cnx);
		Com_Log( SEV_INFO, LOG_CH(ch_network), "WiredNet: close event=%d local_err=%llu remote_err=%llu\n",
			(int)event, (unsigned long long)err_code, (unsigned long long)remote_err );
		if ( conn && conn->active ) {
			char reason[128];
			Com_sprintf( reason, sizeof(reason), "event=%d local=%llu remote=%llu",
				(int)event, (unsigned long long)err_code, (unsigned long long)remote_err );
			WN_LogDisconnect( conn, reason );

			if ( conn->game_conn )
				WN_GameFreeConn( conn->game_conn );

			WN_FreeConnection( conn );
		}
		picoquic_set_callback( cnx, NULL, NULL );
		break;
	}

	case picoquic_callback_request_alpn_list:
		Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: ALPN list requested\n" );
		// picoquic handles ALPN matching via the default_alpn parameter in picoquic_create
		break;

	case picoquic_callback_set_alpn:
		Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: ALPN negotiated\n" );
		break;

	case picoquic_callback_stateless_reset:
		Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: stateless reset received\n" );
		if ( conn && conn->active ) {
			WN_LogDisconnect( conn, "stateless reset" );
			WN_FreeConnection( conn );
		}
		picoquic_set_callback( cnx, NULL, NULL );
		return PICOQUIC_ERROR_DETECTED;
		break;

	case picoquic_callback_datagram:
		if ( conn && conn->active && conn->game_conn )
			WN_GameHandleDatagram( conn, bytes, (int)length );

		break;

	case picoquic_callback_prepare_datagram:
#if FEAT_WIREDNET_OBSERVER
		// Just-in-time datagram encoding — write game state directly into packet
		if ( conn && conn->active && length > 0 ) {
			int encoded = WN_EncodeStateUpdate( bytes, (int)length );
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
WN_Init

Create picoquic context with self-signed cert.
Called from SV_Init.
====================
*/
void WN_Init( void )
{
	uint64_t    current_time;
	const char *cert_file = NULL;
	const char *key_file  = NULL;

	if ( wn.initialized ) {
		return;
	}

	memset( &wn, 0, sizeof( wn ) );
#if FEAT_WIREDNET_OBSERVER
	wn.tcp4_fd = -1;
	wn.tcp6_fd = -1;
#endif

	// Register cvars
	{
		static const cvarDesc_t descs[] = {
			CVAR_STRING( "sv_wirednetAuthToken",  "",   CVAR_ARCHIVE, "Authentication token for Wired QUIC transport." ),
			CVAR_INT(    "sv_wirednetMaxClients", "8",  CVAR_ARCHIVE, "Maximum QUIC/Wired clients.", 0, 0 ),
			CVAR_INT(    "sv_wirednetStateRate",  "10", CVAR_ARCHIVE, "State update rate (Hz) for QUIC transport.", 0, 0 ),
			CVAR_INT(    "sv_wirednetEventRate",  "20", CVAR_ARCHIVE, "Event update rate (Hz) for QUIC transport.", 0, 0 ),
		};
		cvar_t *h[4];
		Cvar_RegisterTable( descs, ARRAY_LEN( descs ), h );
		wn.sv_wirednetAuthToken  = h[0];
		wn.sv_wirednetMaxClients = h[1];
		wn.sv_wirednetStateRate  = h[2];
		wn.sv_wirednetEventRate  = h[3];
	}

#if FEAT_WIREDNET_OBSERVER
	{
		static const cvarDesc_t d0 = CVAR_INT( "sv_observerEventCount", "100", CVAR_ARCHIVE,
			"Maximum events buffered per observer connection.", 0, 0 );
		static const cvarDesc_t d1 = CVAR_INT( "sv_httpRateLimit", "30", CVAR_ARCHIVE,
			"HTTP observer rate limit (requests per second).", 0, 0 );
		wn.sv_observerEventCount = Cvar_Register( &d0 );
		wn.sv_httpRateLimit      = Cvar_Register( &d1 );
	}

	// TCP HTTP listener — same port as QUIC/UDP (TCP and UDP share a port namespace,
	// so both can bind to 27960 independently without conflict).
	{
		int port = Cvar_VariableIntegerValue( "net_port" );
		if ( port <= 0 ) port = PORT_SERVER;
		{
			struct sockaddr_in sa;
			int                yes = 1;
			int                fd  = (int)socket( AF_INET, SOCK_STREAM, 0 );
			if ( fd >= 0 ) {
				setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes) );
				memset( &sa, 0, sizeof(sa) );
				sa.sin_family      = AF_INET;
				sa.sin_addr.s_addr = htonl( INADDR_ANY );
				sa.sin_port        = htons( (unsigned short)port );
				if ( bind( fd, (struct sockaddr *)&sa, sizeof(sa) ) < 0 ||
				     listen( fd, 8 ) < 0 ) {
					wn_tcp_close( fd );
					COM_WARN( LOG_CH(ch_network),
						"WN_Init: TCP HTTP listener failed to bind port %d\n", port );
				} else {
#ifdef _WIN32
					{
						u_long one = 1;
						ioctlsocket( (SOCKET)fd, FIONBIO, &one );
					}
#else
					fcntl( fd, F_SETFL, fcntl( fd, F_GETFL, 0 ) | O_NONBLOCK );
#endif
					wn.tcp4_fd = fd;
				}
			}
		}
	}
#endif

	/* picoquic uses picoquic_current_time() (wall-clock UTC µs) internally
	 * for wake_time scheduling; passing Sys_Microseconds (QPC-monotonic-
	 * since-first-call) here mixes time bases and jams the scheduler. */
	current_time = picoquic_current_time();

	// Create picoquic context with TLS certificate
	// Look for cert/key files in fs_homepath/certs/ or use cvars
	{
		cert_file = Cvar_VariableString( "sv_wirednetCertFile" );
		key_file  = Cvar_VariableString( "sv_wirednetKeyFile" );

		// Default paths: resolve against fs_homepath so the server works regardless
		// of CWD. picoquic calls fopen() directly and is blind to Q3's VFS.
		static char cert_buf[MAX_OSPATH], key_buf[MAX_OSPATH];
		if ( !cert_file || !*cert_file ) {
			const char *homepath = Cvar_VariableString( "fs_homepath" );
			if ( homepath && *homepath )
				Com_sprintf( cert_buf, sizeof(cert_buf), "%s%ccerts%ccert.pem",
					homepath, PATH_SEP, PATH_SEP );
			else
				Q_strncpyz( cert_buf, "certs/cert.pem", sizeof(cert_buf) );
			cert_file = cert_buf;
		}
		if ( !key_file || !*key_file ) {
			const char *homepath = Cvar_VariableString( "fs_homepath" );
			if ( homepath && *homepath )
				Com_sprintf( key_buf, sizeof(key_buf), "%s%ccerts%ckey.pem",
					homepath, PATH_SEP, PATH_SEP );
			else
				Q_strncpyz( key_buf, "certs/key.pem", sizeof(key_buf) );
			key_file = key_buf;
		}

		{
			static const cvarDesc_t dc = CVAR_STRING( "sv_wirednetCertFile", "", CVAR_ARCHIVE,
				"Path to TLS certificate file for QUIC transport (PEM format)." );
			static const cvarDesc_t dk = CVAR_STRING( "sv_wirednetKeyFile", "", CVAR_ARCHIVE,
				"Path to TLS private key file for QUIC transport (PEM format)." );
			Cvar_Register( &dc );
			Cvar_Register( &dk );
		}

		/* Auto-generate a self-signed cert if the files are missing.
		 * This allows the engine to start without manual cert setup on first run. */
		{
			FILE *f = fopen( cert_file, "r" );
			if ( !f ) {
				char dir_buf[MAX_OSPATH];
				/* Ensure the certs directory exists */
				Q_strncpyz( dir_buf, cert_file, sizeof(dir_buf) );
				{
					char *last_sep = strrchr( dir_buf, PATH_SEP );
					if ( last_sep ) {
						*last_sep = '\0';
						Sys_Mkdir( dir_buf );
					}
				}
				Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: cert not found — generating self-signed cert...\n" );
				if ( !WN_GenerateSelfSignedCert( cert_file, key_file ) ) {
					COM_WARN( LOG_CH(ch_network), "QUIC: cert generation failed."
						" Set sv_wirednetCertFile / sv_wirednetKeyFile manually.\n" );
				} else {
					Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: self-signed cert generated at %s\n", cert_file );
				}
			} else {
				fclose( f );
			}
		}

	wn.quic = picoquic_create(
		wn.sv_wirednetMaxClients->integer,  // max connections
		cert_file,                       // cert file
		key_file,                        // key file
		NULL,                            // cert root file
		WN_ALPN,                         // ALPN: "q3v69"
		WN_PicoquicCallback,             // stream callback
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

	if ( !wn.quic ) {
		COM_ERROR( LOG_CH(ch_network), "WN_Init: picoquic_create failed. QUIC disabled.\n" );
		return;
	}

	// ALPN selection callback — required for server-side ALPN negotiation
	picoquic_set_alpn_select_fn_v2( wn.quic, WN_AlpnSelectCallback );

	// Enable QUIC datagrams (RFC 9221)
	picoquic_set_default_datagram_priority( wn.quic, 1 );
	/* Advertise datagram support so incoming client datagrams (usercmds) are not
	 * rejected with FRAME_FORMAT_ERROR (error 7).  Default is 0 = disabled. */
	picoquic_set_default_tp_value( wn.quic, picoquic_tp_max_datagram_frame_size,
	                               WN_SNAP_DGRAM_MAX );

	/* Set idle timeout to 1 hour for the server context.  The negotiated
	 * timeout is min(client, server); for loopback connections the client
	 * now sets 1 hour too, so the negotiated value is 1 hour.  This allows
	 * connections to survive long map loads (WAMR init, bot AI, AAS) without
	 * the idle timer firing.  For remote / dedicated servers this is
	 * acceptable — the game server is expected to keep clients active. */
	picoquic_set_default_idle_timeout( wn.quic, 3600000 );   /* 1 hour */

	/* E1: Disable 0-RTT — no ticket_file/ticket_key passed to picoquic_create
	 * (both NULL above), so the server never issues session tickets and clients
	 * cannot attempt early data.  No additional API call needed. */

	/* E2: Require Retry token — forces clients to prove source address ownership
	 * before the server allocates any per-connection state.  Defeats amplification
	 * attacks where an attacker spoofs the victim's IP as the source. */
	picoquic_set_cookie_mode( wn.quic, 1 );

	wn.initialized = qtrue;
	Net_RegisterDemux( &s_quic_demux );

#if FEAT_WIREDNET_OBSERVER
	WN_RecordInit();
#endif

	{
		int port = Cvar_VariableIntegerValue( "net_port" );
		if ( port <= 0 ) port = PORT_SERVER;
		Com_Log( SEV_INFO, LOG_CH(ch_network), "WiredNet: listening on port %d (IPv4), ALPN: %s, max clients: %d, cert=%s key=%s\n",
			port, WN_ALPN, wn.sv_wirednetMaxClients->integer,
			cert_file ? cert_file : "?", key_file ? key_file : "?" );
	}

	/* Publish the transport vtable so engine code can route through it */
	transport = &quic_transport;
}


/*
====================
WN_Shutdown

Graceful shutdown — send APPLICATION_CLOSE to all clients, then destroy context.
Called from SV_Shutdown.
====================
*/
void WN_Shutdown( void )
{
	int i;

	if ( !wn.initialized )
		return;

#if FEAT_WIREDNET_OBSERVER
	WN_RecordShutdown();
	if ( wn.tcp4_fd >= 0 ) { wn_tcp_close( wn.tcp4_fd ); wn.tcp4_fd = -1; }
	if ( wn.tcp6_fd >= 0 ) { wn_tcp_close( wn.tcp6_fd ); wn.tcp6_fd = -1; }
#endif

	// Graceful close — notify all connected clients
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.connections[i].active && wn.connections[i].cnx ) {
			picoquic_close( wn.connections[i].cnx, 0 );
			WN_LogDisconnect( &wn.connections[i], "server shutdown" );
			WN_FreeConnection( &wn.connections[i] );
		}
	}

	// Flush any remaining outbound packets (final close frames)
	WN_FlushOutbound();

	picoquic_free( wn.quic );
	wn.quic = NULL;
	Net_UnregisterDemux( &s_quic_demux );
	wn.initialized = qfalse;

	Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC transport shut down.\n" );
}


/*
====================
WN_ProcessTimers

Drive picoquic's internal timers (retransmit, idle, keepalive).
Called from SV_Frame.
====================
*/
void WN_ProcessTimers( void )
{
	if ( !wn.initialized )
		return;

	// picoquic handles timers internally when we call prepare_next_packet.
	// We just need to make sure FlushOutbound is called, which sends
	// any timer-driven packets (ACKs, retransmits, keepalives).
}


/*
====================
WN_FlushOutbound

Pull all pending outbound packets from picoquic and send them via NET_SendPacket.
Called BOTH after recv drain (fast ACK path) AND from SV_Frame (timer path).

  picoquic_prepare_next_packet_ex
    ├── returns outbound packet in send_buf
    ├── provides destination address
    └── returns 0 + send_len=0 when no more packets pending

This is the core "packet pump" — picoquic never touches the socket directly.
====================
*/
void WN_FlushOutbound( void )
{
	byte              send_buf[WN_PACKET_BUF_SIZE];
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

	if ( !wn.initialized )
		return;

	current_time = picoquic_current_time();

	while ( 1 ) {
		send_len = 0;
		ret = picoquic_prepare_next_packet_ex(
			wn.quic,
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
			to.type = NA_IP6;
			memcpy( to.ipv._6, &v6->sin6_addr, 16 );
			to.port = v6->sin6_port;
			to.scope_id = (uint32_t)v6->sin6_scope_id;
		}
#endif
		else {
			continue; // unknown address family, skip
		}

		Com_Log( SEV_TRACE, LOG_CH(ch_network), "[WiredNet] QUIC: sending %d bytes to %s\n", (int)send_len, NET_AdrToString(&to) );
		NET_SendPacket( NS_SERVER, (int)send_len, send_buf, &to );
	}
}


/*
====================
WN_DemuxPacket

transport_demux_t handler — check if a received UDP packet is QUIC.
Registered via Net_RegisterDemux in WN_Init; called from Net_DispatchDemux
for each recvfrom result (IPv4, IPv6, multicast).

Returns qtrue if the packet was consumed by QUIC.

Demux logic:
  1. 4-byte 0xFFFFFFFF → Q3 connectionless (checked BEFORE this function)
  2. Byte 0 & 0x80     → QUIC Long Header (Initial, Handshake, 0-RTT, Retry)
  3. (Byte 0 & 0xC0) == 0x40 → QUIC Short Header (1-RTT)
  4. Otherwise           → Netchan
====================
*/
static qboolean WN_DemuxPacket( const netadr_t *from, const byte *data, int len )
{
	uint64_t current_time;
	uint8_t  first;

	if ( !wn.initialized )
		return qfalse;

	if ( len <= 0 )
		return qfalse;

	first = data[0];

	// QUIC Long Header: bit 7 set (Initial, Handshake, 0-RTT, Retry)
	// QUIC Short Header: bits 7:6 = 01 (1-RTT established connection)
	if ( (first & 0x80) || (first & 0xC0) == 0x40 ) {
		Com_Log( SEV_TRACE, LOG_CH(ch_network), "[WiredNet] QUIC: demux hit, first=0x%02X len=%d from=%s\n",
			first, len, NET_AdrToString( from ) );
		current_time = picoquic_current_time();

		// Copy packet data to QUIC-owned buffer — net_message->data is shared
		// and may be reused by subsequent recvfrom calls in the same drain loop.
		if ( len > WN_PACKET_BUF_SIZE ) {
			COM_WARN( LOG_CH(ch_network), "QUIC: oversized packet (%d bytes), dropped\n", len );
			wn.dropped_packets++;
			return qtrue;  // consumed (dropped)
		}
		memcpy( wn.recv_buf, data, len );

		// Convert netadr_t to sockaddr for picoquic
		{
			struct sockaddr_storage ss_from;
			int ss_len = WN_NetadrToSockaddr( from, &ss_from );
			if ( ss_len == 0 ) {
				wn.dropped_packets++;
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
				wn.quic,
				wn.recv_buf,
				(size_t)len,
				(struct sockaddr *)&ss_from,
				(struct sockaddr *)&ss_to,
				0,                          // if_index
				0,                          // ECN
				current_time
			);
			Com_Log( SEV_TRACE, LOG_CH(ch_network), "[WiredNet] QUIC: picoquic_incoming_packet returned %d\n", pq_ret );
		}

		// Dual-flush: send ACKs immediately after receiving QUIC packets.
		// Without this, ACKs are delayed until the next SV_Frame (50ms at 20Hz),
		// exceeding QUIC's 25ms max_ack_delay and causing unnecessary retransmits.
		Com_Log( SEV_TRACE, LOG_CH(ch_network), "[WiredNet] WiredNet: dual-flush after incoming packet\n" );
		WN_FlushOutbound();

		// Also feed the client QUIC context (when running as non-dedicated client)
#if !defined(DEDICATED)
		WN_ClientCheckPacket( from, (byte *)data, len ); /* safe: WN_ClientCheckPacket memcpy-copies immediately, never writes through */
#endif

		cl_prof.chkpkt += (int)(Sys_Microseconds() - current_time);
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

static void WN_Status_f( void )
{
	int i;
	int count = 0;
	uint64_t now = Sys_Microseconds();

	if ( !wn.initialized ) {
		Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC transport not initialized.\n" );
		return;
	}

	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.connections[i].active )
			count++;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC connections: %d/%d\n", count, wn.sv_wirednetMaxClients->integer );

	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_connection_t *c = &wn.connections[i];
		picoquic_path_quality_t quality;
		uint64_t uptime_sec;
		float loss_pct;
		const char *conn_type, *role, *auth;

		if ( !c->active ) continue;

		uptime_sec = (now - c->connect_time) / 1000000;
		conn_type = WN_HasPermPlayer(c->perm)  ? "PLAYER"   : "OBSERVER";
		role      = WN_HasPermLeader(c->perm)   ? "LEADER"   : "MEMBER";
		auth      = WN_HasPermAdmin(c->perm)    ? "ADMIN"    : "USER";

		// Get RTT and loss stats from picoquic
		memset( &quality, 0, sizeof(quality) );
		if ( c->cnx ) {
			picoquic_get_path_quality( c->cnx, 0, &quality );
		}
		loss_pct = quality.sent > 0 ? (float)quality.lost * 100.0f / (float)quality.sent : 0.0f;

		Com_Log( SEV_INFO, LOG_CH(ch_network), "  #%d  %s  %s+%s+%s  RTT:%llums  Loss:%.1f%%  Up:%lum%lus\n",
			i,
			NET_AdrToString( &c->addr ),
			conn_type, role, auth,
			(unsigned long long)(quality.rtt / 1000), // μs → ms
			loss_pct,
			(unsigned long)(uptime_sec / 60),
			(unsigned long)(uptime_sec % 60) );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_network), "Events emitted: %llu\n", (unsigned long long)wn.event_seq );
	Com_Log( SEV_INFO, LOG_CH(ch_network), "Dropped packets (misroute): %d\n", wn.dropped_packets );
}


/*
====================
WN_RegisterCommands

Called after WN_Init to register console commands.
====================
*/
void WN_RegisterCommands( void )
{
	Cmd_AddCommand( "net_quic_status", WN_Status_f );
}
