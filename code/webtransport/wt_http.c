/*
===========================================================================
wt_http.c — Minimal HTTP/1.1 over QUIC streams

NOT full HTTP/3 — this is a simple HTTP/1.1 request-response parser on a
QUIC bidirectional stream. Sufficient for /health and /metrics endpoints.

Full HTTP/3 (QPACK, stream multiplexing) is a post-slice concern.

Endpoints:
  GET /health   → 200 OK (for load balancers, uptime monitors)
  GET /metrics  → Prometheus-format QUIC stats (for Grafana)
  *             → 404 Not Found
===========================================================================
*/
#include "wt_local.h"

#if FEAT_QUIC_HTTP

#define HTTP_MAX_REQUEST  4096
#define HTTP_MAX_RESPONSE 8192

/*
====================
WT_HttpSendResponse

Send an HTTP/1.1 response on a QUIC stream, then close the stream.
====================
*/
static void WT_HttpSendResponse( wt_connection_t *conn, uint64_t stream_id,
                                  int status_code, const char *status_text,
                                  const char *content_type, const char *body )
{
	char  header[512];
	int   body_len = body ? (int)strlen(body) : 0;
	int   hdr_len;

	hdr_len = Com_sprintf( header, sizeof(header),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n",
		status_code, status_text,
		content_type,
		body_len );

	// Send header
	picoquic_add_to_stream( conn->cnx, stream_id,
		(const uint8_t *)header, (size_t)hdr_len, 0 );

	// Send body + FIN
	if ( body_len > 0 ) {
		picoquic_add_to_stream( conn->cnx, stream_id,
			(const uint8_t *)body, (size_t)body_len, 1 ); // set_fin=1
	} else {
		picoquic_add_to_stream( conn->cnx, stream_id,
			(const uint8_t *)"", 0, 1 ); // FIN only
	}
}


/*
====================
WT_HttpHandleHealth

GET /health → 200 OK with basic server info.
Used by load balancers, Docker HEALTHCHECK, k8s liveness probes.
====================
*/
static void WT_HttpHandleHealth( wt_connection_t *conn, uint64_t stream_id )
{
	char body[512];

	Com_sprintf( body, sizeof(body),
		"{\"status\":\"ok\","
		"\"map\":\"%s\","
		"\"gametype\":%d,"
		"\"quic_connections\":%d,"
		"\"uptime_ms\":%d}",
		Cvar_VariableString( "mapname" ),
		Cvar_VariableIntegerValue( "g_gametype" ),
		wt.num_connections,
		Sys_Milliseconds() );

	WT_HttpSendResponse( conn, stream_id, 200, "OK", "application/json", body );
}


/*
====================
WT_HttpHandleMetrics

GET /metrics → Prometheus exposition format.
Exposes QUIC connection stats for Grafana dashboards.
====================
*/
static void WT_HttpHandleMetrics( wt_connection_t *conn, uint64_t stream_id )
{
	char body[HTTP_MAX_RESPONSE];
	int  offset = 0;

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP q3_quic_connections_active Current QUIC connections.\n"
		"# TYPE q3_quic_connections_active gauge\n"
		"q3_quic_connections_active %d\n\n",
		wt.num_connections );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP q3_quic_connections_max Maximum QUIC connections allowed.\n"
		"# TYPE q3_quic_connections_max gauge\n"
		"q3_quic_connections_max %d\n\n",
		wt.sv_quicMaxClients->integer );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP q3_quic_packets_dropped_total Packets dropped by QUIC demux.\n"
		"# TYPE q3_quic_packets_dropped_total counter\n"
		"q3_quic_packets_dropped_total %d\n\n",
		wt.dropped_packets );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP q3_quic_events_total Total game events emitted.\n"
		"# TYPE q3_quic_events_total counter\n"
		"q3_quic_events_total %llu\n\n",
		(unsigned long long)wt.event_seq );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP q3_server_uptime_ms Server uptime in milliseconds.\n"
		"# TYPE q3_server_uptime_ms gauge\n"
		"q3_server_uptime_ms %d\n",
		Sys_Milliseconds() );

	WT_HttpSendResponse( conn, stream_id, 200, "OK",
		"text/plain; version=0.0.4; charset=utf-8", body );
}


/*
====================
WT_HttpHandleRequest

Parse an incoming HTTP/1.1 request and route to the appropriate handler.
Called from the picoquic callback when data arrives on an HTTP stream.

Minimal parser — only handles "GET /path HTTP/1.1\r\n..."
====================
*/
void WT_HttpHandleRequest( wt_connection_t *conn, uint64_t stream_id,
                            const byte *data, int len )
{
	char req[HTTP_MAX_REQUEST];
	char method[16];
	char path[256];
	int  i, j;

	if ( !conn || !conn->active )
		return;

	// Copy to null-terminated buffer
	if ( len >= (int)sizeof(req) )
		len = (int)sizeof(req) - 1;
	Com_Memcpy( req, data, len );
	req[len] = '\0';

	// Parse "METHOD /path HTTP/1.x"
	i = 0;
	j = 0;

	// Extract method
	while ( i < len && req[i] != ' ' && j < (int)sizeof(method) - 1 )
		method[j++] = req[i++];
	method[j] = '\0';

	// Skip space
	while ( i < len && req[i] == ' ' ) i++;

	// Extract path
	j = 0;
	while ( i < len && req[i] != ' ' && req[i] != '?' && j < (int)sizeof(path) - 1 )
		path[j++] = req[i++];
	path[j] = '\0';

	// Only handle GET
	if ( Q_stricmp( method, "GET" ) != 0 ) {
		WT_HttpSendResponse( conn, stream_id, 405, "Method Not Allowed",
			"text/plain", "Only GET is supported.\n" );
		return;
	}

	// Route
	if ( Q_stricmp( path, "/health" ) == 0 ) {
		WT_HttpHandleHealth( conn, stream_id );
	}
	else if ( Q_stricmp( path, "/metrics" ) == 0 ) {
		WT_HttpHandleMetrics( conn, stream_id );
	}
	else {
		WT_HttpSendResponse( conn, stream_id, 404, "Not Found",
			"text/plain", "Not found.\n" );
	}
}

#endif // FEAT_QUIC_HTTP
