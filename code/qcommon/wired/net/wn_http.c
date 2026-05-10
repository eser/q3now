/*
===========================================================================
wn_http.c — HTTP/1.1 over QUIC streams + TCP listener

NOT full HTTP/3 — this is a simple HTTP/1.1 request-response parser on a
QUIC bidirectional stream. Sufficient for /health, /metrics, /status.json
and static file serving.

A plain TCP listener on the same port (27960) accepts browser connections
without QUIC. Both paths share identical handlers via wn_http_ctx_t.

Endpoints:
  GET /health      → 200 OK (load balancer / Docker HEALTHCHECK)
  GET /metrics     → Prometheus-format QUIC stats (Grafana)
  GET /status.json → live scoreboard + event feed (browser UI)
  GET /*           → static file from baseq3/web/ (Q3 VFS)
===========================================================================
*/
#include "wn_local.h"

#if FEAT_WIREDNET_OBSERVER

#include "../../../server/server.h"  // svs, sv, SV_GameClientNum, sv_maxclients
#include "../protocol.h"             // PERS_SCORE, PERS_TEAM, PERS_KILLED, ET_*

// Platform socket headers for TCP listener
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  define tcp_close(fd)   closesocket((SOCKET)(fd))
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <errno.h>
#  define tcp_close(fd)   close(fd)
#endif

#define HTTP_MAX_REQUEST    4096
#define HTTP_MAX_RESPONSE   8192
#define WN_STATUS_RESP_MAX  65536       // max /status.json response body (64KB)
#define WN_FILE_SIZE_LIMIT  (1024*1024) // 1MB static file size limit

// ──────────────────────────────────────────────────────────────────────
// HTTP context — abstracts QUIC stream vs TCP so all handlers share
// body-building code regardless of the underlying transport.
// ──────────────────────────────────────────────────────────────────────
typedef struct {
	qboolean         is_tcp;
	// QUIC path (is_tcp == qfalse)
	wn_connection_t *conn;
	uint64_t         stream_id;
	// TCP path (is_tcp == qtrue)
	int              tcp_fd;
	// both paths — caller fills addr for rate limiting
	netadr_t         addr;
} wn_http_ctx_t;


/*
====================
WN_HttpFormatHeader

Format a complete HTTP/1.1 response header into buf.
Includes Content-Type, Content-Length, CORS, and Connection: close.
Returns the number of bytes written.
====================
*/
static int WN_HttpFormatHeader( char *buf, int buf_size,
                                 int status_code, const char *status_text,
                                 const char *content_type, int body_len )
{
	return Com_sprintf( buf, buf_size,
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"\r\n",
		status_code, status_text, content_type, body_len );
}


/*
====================
WN_HttpCtxSend

Send header + body over either a QUIC stream or a TCP socket.
body may be NULL when body_len is 0.
====================
*/
static void WN_HttpCtxSend( wn_http_ctx_t *ctx,
                             int status_code, const char *status_text,
                             const char *content_type,
                             const byte *body, int body_len )
{
	char header[512];
	int  hdr_len = WN_HttpFormatHeader( header, sizeof(header),
	                                     status_code, status_text,
	                                     content_type, body_len );

	if ( ctx->is_tcp ) {
		send( ctx->tcp_fd, header, hdr_len, 0 );
		if ( body_len > 0 )
			send( ctx->tcp_fd, (const char *)body, body_len, 0 );
	} else {
		// QUIC: fin=1 on the last add_to_stream call closes the send side.
		picoquic_add_to_stream( ctx->conn->cnx, ctx->stream_id,
			(const uint8_t *)header, (size_t)hdr_len, body_len > 0 ? 0 : 1 );
		if ( body_len > 0 ) {
			picoquic_add_to_stream( ctx->conn->cnx, ctx->stream_id,
				(const uint8_t *)body, (size_t)body_len, 1 );
		}
	}
}


/* Convenience wrapper for null-terminated body strings. */
static void WN_HttpCtxSendStr( wn_http_ctx_t *ctx,
                                int status_code, const char *status_text,
                                const char *content_type, const char *body )
{
	int body_len = body ? (int)strlen(body) : 0;
	WN_HttpCtxSend( ctx, status_code, status_text, content_type,
	                (const byte *)body, body_len );
}


/* Escape a string for JSON — drops control chars, strips Q3 color codes, escapes " and \. */
static void WN_JsonEscapeStr( const char *in, char *out, int out_size )
{
	int           i = 0, j = 0;
	unsigned char c;
	while ( (c = (unsigned char)in[i]) && j < out_size - 2 ) {
		i++;
		if ( c < 0x20 ) continue;
		// Strip Q3 color codes: ^ followed by any non-null, non-^ character
		if ( c == '^' && in[i] != '\0' && in[i] != '^' ) { i++; continue; }
		if ( c == '"' || c == '\\' ) {
			if ( j >= out_size - 3 ) break;
			out[j++] = '\\';
		}
		out[j++] = (char)c;
	}
	out[j] = '\0';
}


/* djb2 hash of netadr_t bytes, excluding the last 2 (port), for per-IP identity. */
static uint32_t WN_HashAddr( const netadr_t *addr )
{
	const byte *p = (const byte *)addr;
	int         n = (int)sizeof(*addr) - 2;
	uint32_t    h = 5381;
	int         i;
	for ( i = 0; i < n; i++ )
		h = ( h << 5 ) + h + (uint32_t)p[i];
	return h;
}


/*
====================
WN_HttpRateLimitExceeded

Fixed 64-bucket per-IP rate limiter: max sv_httpRateLimit requests per second.
On hash collision the old IP is evicted (conservative, correct per-spec).
Returns qtrue if the request should be rejected with 429.
====================
*/
static qboolean WN_HttpRateLimitExceeded( const netadr_t *addr )
{
	uint32_t          hash   = WN_HashAddr( addr );
	int               bucket = (int)( hash % WN_HTTP_RATE_BUCKETS );
	wn_rate_bucket_t *b      = &wn.http_rate[bucket];
	int               now    = Sys_Milliseconds();
	int               limit;

	if ( !wn.sv_httpRateLimit )
		return qfalse;
	limit = wn.sv_httpRateLimit->integer;
	if ( limit <= 0 )
		return qfalse;

	if ( !b->used || b->ip_hash != hash ) {
		b->used       = qtrue;
		b->ip_hash    = hash;
		b->count      = 1;
		b->reset_time = now;
		return qfalse;
	}

	if ( now - b->reset_time >= 1000 ) {
		b->count      = 0;
		b->reset_time = now;
	}

	b->count++;
	return ( b->count > limit ) ? qtrue : qfalse;
}


/* Returns qfalse if path contains traversal sequences or unsafe characters. */
static qboolean WN_PathIsSafe( const char *path )
{
	const char *p;
	if ( !path || !path[0] ) return qfalse;
	for ( p = path; *p; p++ ) {
		if ( p[0] == '.' && p[1] == '.' ) return qfalse;
		if ( p[0] == '/' && p[1] == '/' ) return qfalse;
		if ( !(
			( *p >= 'a' && *p <= 'z' ) ||
			( *p >= 'A' && *p <= 'Z' ) ||
			( *p >= '0' && *p <= '9' ) ||
			*p == '-' || *p == '_' || *p == '.' || *p == '/'
		) ) return qfalse;
	}
	return qtrue;
}


/* Map file extension to MIME type string. */
static const char *WN_GetMimeType( const char *path )
{
	const char *ext = strrchr( path, '.' );
	if ( !ext ) return "application/octet-stream";
	if ( !Q_stricmp( ext, ".html" ) )   return "text/html";
	if ( !Q_stricmp( ext, ".css"  ) )   return "text/css";
	if ( !Q_stricmp( ext, ".js"   ) )   return "application/javascript";
	if ( !Q_stricmp( ext, ".json" ) )   return "application/json";
	if ( !Q_stricmp( ext, ".png"  ) )   return "image/png";
	if ( !Q_stricmp( ext, ".jpg"  ) || !Q_stricmp( ext, ".jpeg" ) ) return "image/jpeg";
	if ( !Q_stricmp( ext, ".svg"  ) )   return "image/svg+xml";
	if ( !Q_stricmp( ext, ".ico"  ) )   return "image/x-icon";
	if ( !Q_stricmp( ext, ".woff2") )   return "font/woff2";
	return "application/octet-stream";
}


/*
====================
WN_HttpHandleHealth

GET /health → 200 OK with basic server info.
Used by load balancers, Docker HEALTHCHECK, k8s liveness probes.
====================
*/
static void WN_HttpHandleHealth( wn_http_ctx_t *ctx )
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
		wn.num_connections,
		Sys_Milliseconds() );

	WN_HttpCtxSendStr( ctx, 200, "OK", "application/json", body );
}


/*
====================
WN_HttpHandleMetrics

GET /metrics → Prometheus exposition format.
Exposes QUIC connection stats for Grafana dashboards.
====================
*/
static void WN_HttpHandleMetrics( wn_http_ctx_t *ctx )
{
	char body[HTTP_MAX_RESPONSE];
	int  offset = 0;

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP wired_quic_connections_active Current QUIC connections.\n"
		"# TYPE wired_quic_connections_active gauge\n"
		"wired_quic_connections_active %d\n\n",
		wn.num_connections );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP wired_quic_connections_max Maximum QUIC connections allowed.\n"
		"# TYPE wired_quic_connections_max gauge\n"
		"wired_quic_connections_max %d\n\n",
		wn.sv_wirednetMaxClients->integer );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP wired_quic_packets_dropped_total Packets dropped by QUIC demux.\n"
		"# TYPE wired_quic_packets_dropped_total counter\n"
		"wired_quic_packets_dropped_total %d\n\n",
		wn.dropped_packets );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP wired_quic_events_total Total game events emitted.\n"
		"# TYPE wired_quic_events_total counter\n"
		"wired_quic_events_total %llu\n\n",
		(unsigned long long)wn.event_seq );

	offset += Com_sprintf( body + offset, sizeof(body) - offset,
		"# HELP wired_server_uptime_ms Server uptime in milliseconds.\n"
		"# TYPE wired_server_uptime_ms gauge\n"
		"wired_server_uptime_ms %d\n",
		Sys_Milliseconds() );

	WN_HttpCtxSendStr( ctx, 200, "OK",
		"text/plain; version=0.0.4; charset=utf-8", body );
}


/*
====================
WN_HttpHandleStatus

GET /status.json → live filtered scoreboard JSON for browser UI.

Included: kill/chat/item events (no positions), player scores/deaths/team/ping.
Excluded: damage, delag, bot events, positions, health, armor.
====================
*/
static void WN_HttpHandleStatus( wn_http_ctx_t *ctx )
{
	// Static buffer — avoids 64KB stack frame; HTTP handler is single-threaded.
	static char body[WN_STATUS_RESP_MAX];
	char        esc[256];
	int         off = 0;
	int         i;
	int         event_count;
	qboolean    first;
	uint64_t    ev_start;

	event_count = wn.sv_observerEventCount ? wn.sv_observerEventCount->integer : 100;
	if ( event_count < 1 )                event_count = 1;
	if ( event_count > WN_JSON_RING_SIZE ) event_count = WN_JSON_RING_SIZE;

	// ── server block ──────────────────────────────────────────────
	{
		char esc2[256];
		WN_JsonEscapeStr( Cvar_VariableString("mapname"),       esc,  sizeof(esc)  );
		WN_JsonEscapeStr( Cvar_VariableString("sv_maplongname"), esc2, sizeof(esc2) );
		off += Com_sprintf( body + off, sizeof(body) - off,
			"{\"server\":{"
			"\"map\":\"%s\","
			"\"maplong\":\"%s\","
			"\"gametype\":%d,"
			"\"time\":%d,"
			"\"timelimit\":%d,"
			"\"scorelimit\":%d"
			"},\n",
			esc, esc2,
			Cvar_VariableIntegerValue("g_gametype"),
			sv.time,
			Cvar_VariableIntegerValue("g_timelimit"),
			Cvar_VariableIntegerValue("g_scorelimit") );
	}

	// ── players block ─────────────────────────────────────────────
	off += Com_sprintf( body + off, sizeof(body) - off, "\"players\":[" );
	first = qtrue;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t      *cl = &svs.clients[i];
		playerState_t *ps;
		int            score, deaths, team;

		if ( cl->state < CS_CONNECTED )
			continue;

		ps     = sv.gameClients ? SV_GameClientNum(i) : NULL;
		score  = ps ? ps->persistant[PERS_SCORE]  : 0;
		deaths = ps ? ps->persistant[PERS_KILLED] : 0;
		team   = ps ? ps->persistant[PERS_TEAM]   : 0;

		WN_JsonEscapeStr( cl->name[0] ? cl->name : "unknown", esc, sizeof(esc) );

		{
			int shots  = ps ? ps->persistant[PERS_TOTAL_SHOTS]  : 0;
			int hits   = ps ? ps->persistant[PERS_TOTAL_HITS]   : 0;
			int dmg    = ps ? ps->persistant[PERS_TOTAL_DAMAGE] : 0;
			off += Com_sprintf( body + off, sizeof(body) - off,
				"%s{\"name\":\"%s\",\"score\":%d,\"deaths\":%d,"
				"\"team\":%d,\"ping\":%d,\"alive\":%s,"
				"\"shots\":%d,\"hits\":%d,\"dmg\":%d}",
				first ? "" : ",",
				esc, score, deaths, team,
				cl->ping > 0 ? cl->ping : 0,
				cl->state == CS_ACTIVE ? "true" : "false",
				shots, hits, dmg );
		}

		first = qfalse;
		if ( off > WN_STATUS_RESP_MAX - 1024 ) break;
	}

	off += Com_sprintf( body + off, sizeof(body) - off, "],\n" );

	// ── events block (last N from JSON ring, oldest→newest) ───────
	off += Com_sprintf( body + off, sizeof(body) - off, "\"events\":[" );
	first = qtrue;

	ev_start = ( wn.json_write_idx > (uint64_t)event_count )
		? wn.json_write_idx - (uint64_t)event_count
		: 0;

	for ( i = (int)ev_start; (uint64_t)i < wn.json_write_idx; i++ ) {
		int              slot = (int)( (uint64_t)i % WN_JSON_RING_SIZE );
		wn_json_event_t *je   = &wn.json_events[slot];

		if ( je->seq != (uint64_t)i || !je->json[0] )
			continue;

		off += Com_sprintf( body + off, sizeof(body) - off,
			"%s%s", first ? "" : ",", je->json );

		first = qfalse;
		if ( off > WN_STATUS_RESP_MAX - 512 ) break;
	}

	off += Com_sprintf( body + off, sizeof(body) - off, "]}\n" );

	WN_HttpCtxSendStr( ctx, 200, "OK", "application/json", body );
}


/*
====================
WN_HttpHandleStaticFile

Serve static web assets from baseq3/web/ via Q3's VFS.
Supports mod overrides (mymod/web/ shadows baseq3/web/).
Rejects files > 1MB and paths with traversal sequences.
====================
*/
static void WN_HttpHandleStaticFile( wn_http_ctx_t *ctx, const char *url_path )
{
	char        vfspath[MAX_QPATH];
	const char *file_path = url_path;
	const char *mime;
	void       *buf = NULL;
	int         file_len;

	if ( !file_path[0] || ( file_path[0] == '/' && file_path[1] == '\0' ) )
		file_path = "/index.html";

	if ( file_path[0] == '/' ) file_path++;

	if ( !WN_PathIsSafe( file_path ) ) {
		WN_HttpCtxSendStr( ctx, 400, "Bad Request", "text/plain", "Bad request.\n" );
		return;
	}

	Com_sprintf( vfspath, sizeof(vfspath), "web/%s", file_path );

	file_len = FS_ReadFile( vfspath, &buf );
	if ( file_len <= 0 || !buf ) {
		WN_HttpCtxSendStr( ctx, 404, "Not Found", "text/plain", "Not found.\n" );
		return;
	}

	if ( file_len > WN_FILE_SIZE_LIMIT ) {
		FS_FreeFile( buf );
		WN_HttpCtxSendStr( ctx, 403, "Forbidden", "text/plain", "File too large.\n" );
		return;
	}

	mime = WN_GetMimeType( file_path );
	WN_HttpCtxSend( ctx, 200, "OK", mime, (const byte *)buf, file_len );

	FS_FreeFile( buf );
}


#if FEAT_WIREDNET_CONTROL
#define HTTP_MCP_RESP_MAX  (32 * 1024)

/*
====================
WN_HttpCtxSendRedirect

Send a 302 redirect response with a Location header.
Used by the OAuth authorize endpoint.
====================
*/
static void WN_HttpCtxSendRedirect( wn_http_ctx_t *ctx, const char *location )
{
	char resp[1024];
	int  len = Com_sprintf( resp, sizeof(resp),
		"HTTP/1.1 302 Found\r\n"
		"Location: %s\r\n"
		"Content-Length: 0\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Connection: close\r\n"
		"\r\n",
		location );

	if ( ctx->is_tcp ) {
		send( ctx->tcp_fd, resp, len, 0 );
	} else {
		picoquic_add_to_stream( ctx->conn->cnx, ctx->stream_id,
			(const uint8_t *)resp, (size_t)len, 1 );
	}
}


/* Decode a URL-encoded query parameter value into out[0..out_size-1]. */
static void WN_UrlDecodeParam( const char *in, char *out, int out_size )
{
	int j = 0;
	while ( *in && j < out_size - 1 ) {
		if ( in[0] == '%' ) {
			unsigned int hi = 0, lo = 0;
			// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign) — `in` is a non-NULL string per caller contract; in[1]/in[2] are read after `*in` checked non-NUL above (URL %XX decoder)
			char c1 = in[1], c2 = in[2];
			if ( c1 >= '0' && c1 <= '9' ) hi = (unsigned int)(c1 - '0');
			else if ( c1 >= 'a' && c1 <= 'f' ) hi = (unsigned int)(c1 - 'a' + 10);
			else if ( c1 >= 'A' && c1 <= 'F' ) hi = (unsigned int)(c1 - 'A' + 10);
			else { out[j++] = *in++; continue; }

			if ( c2 >= '0' && c2 <= '9' ) lo = (unsigned int)(c2 - '0');
			else if ( c2 >= 'a' && c2 <= 'f' ) lo = (unsigned int)(c2 - 'a' + 10);
			else if ( c2 >= 'A' && c2 <= 'F' ) lo = (unsigned int)(c2 - 'A' + 10);
			else { out[j++] = *in++; continue; }

			out[j++] = (char)( (hi << 4) | lo );
			in += 3;
		} else if ( *in == '+' ) {
			out[j++] = ' ';
			in++;
		} else {
			out[j++] = *in++;
		}
	}
	out[j] = '\0';
}


/*
====================
WN_HttpHandleOAuthDiscover

GET /.well-known/oauth-authorization-server
Returns minimal RFC 8414 metadata. The MCP SDK (2025-03-26 spec) validates
this JSON before initiating the auth flow; all required fields are present.
====================
*/
static void WN_HttpHandleOAuthDiscover( wn_http_ctx_t *ctx )
{
	WN_HttpCtxSendStr( ctx, 200, "OK", "application/json",
		"{\"issuer\":\"http://localhost:27960\","
		"\"authorization_endpoint\":\"http://localhost:27960/authorize\","
		"\"token_endpoint\":\"http://localhost:27960/token\","
		"\"registration_endpoint\":\"http://localhost:27960/register\","
		"\"response_types_supported\":[\"code\"],"
		"\"grant_types_supported\":[\"authorization_code\",\"client_credentials\"],"
		"\"token_endpoint_auth_methods_supported\":[\"none\"],"
		"\"code_challenge_methods_supported\":[\"S256\"]}" );
}


/*
====================
WN_HttpHandleRegister

POST /register — dynamic client registration (RFC 7591).
Echoes redirect_uris from the request body per RFC 7591 Section 3.2.
The MCP SDK validates the response with Zod and requires redirect_uris.
====================
*/
static void WN_HttpHandleRegister( wn_http_ctx_t *ctx, const char *req, int req_len )
{
	static char   resp[1024];
	const char   *body;
	const char   *key;
	const char   *arr_start;
	const char   *arr_end;
	char          redirect_uris[512];

	redirect_uris[0] = '\0';

	// Find POST body after blank line
	body = strstr( req, "\r\n\r\n" );
	if ( body ) {
		body += 4;
		// Find "redirect_uris" key in body
		key = strstr( body, "\"redirect_uris\"" );
		if ( key ) {
			// Skip past key, whitespace, colon, whitespace to find '['
			arr_start = key + 15;  // len("\"redirect_uris\"")
			while ( *arr_start && *arr_start != '[' ) arr_start++;
			if ( *arr_start == '[' ) {
				// Find matching ']' — handles simple arrays (no nested arrays)
				arr_end = arr_start + 1;
				while ( *arr_end && *arr_end != ']' ) arr_end++;
				if ( *arr_end == ']' ) {
					int arr_len = (int)(arr_end - arr_start + 1);
					if ( arr_len < (int)sizeof(redirect_uris) - 1 ) {
						Q_strncpyz( redirect_uris, arr_start, arr_len + 1 );
						redirect_uris[arr_len] = '\0';
					}
				}
			}
		}
	}

	if ( redirect_uris[0] == '\0' ) {
		// Fallback: empty array if body didn't contain redirect_uris
		Q_strncpyz( redirect_uris, "[]", sizeof(redirect_uris) );
	}

	Q_strncpyz( resp, "{\"client_id\":\"wired-mcp-client\","
		"\"client_id_issued_at\":0,"
		"\"redirect_uris\":", sizeof(resp) );
	{
		qstring_t resp_qs = QS_WrapExisting( resp, sizeof(resp) );
		QS_Append( &resp_qs, redirect_uris );
		QS_Append( &resp_qs, ","
			"\"grant_types\":[\"authorization_code\"],"
			"\"token_endpoint_auth_method\":\"none\","
			"\"response_types\":[\"code\"]}" );
	}

	WN_HttpCtxSendStr( ctx, 201, "Created", "application/json", resp );
}


/*
====================
WN_HttpHandleAuthorize

GET /authorize?...redirect_uri=...&state=...
Auto-approves the OAuth code request by immediately redirecting to the
redirect_uri with a fixed code. No user interaction needed — the server
trusts all local connections.
====================
*/
static void WN_HttpHandleAuthorize( wn_http_ctx_t *ctx, const char *req )
{
	const char *qs;
	const char *p;
	char        redirect_uri[512];
	char        state[256];
	char        location[768];

	redirect_uri[0] = '\0';
	state[0]        = '\0';

	// Query string starts after the first '?'
	qs = strchr( req, '?' );
	if ( !qs ) {
		WN_HttpCtxSendStr( ctx, 400, "Bad Request", "text/plain", "Missing query.\n" );
		return;
	}
	qs++;

	// Extract redirect_uri
	p = strstr( qs, "redirect_uri=" );
	if ( p ) {
		char raw[512];
		int  i = 0;
		p += 13;
		while ( *p && *p != '&' && *p != ' ' && *p != '\r' && i < (int)sizeof(raw) - 1 )
			raw[i++] = *p++;
		raw[i] = '\0';
		WN_UrlDecodeParam( raw, redirect_uri, sizeof(redirect_uri) );
	}

	// Extract state
	p = strstr( qs, "state=" );
	if ( p ) {
		int i = 0;
		p += 6;
		while ( *p && *p != '&' && *p != ' ' && *p != '\r' && i < (int)sizeof(state) - 1 )
			state[i++] = *p++;
		state[i] = '\0';
	}

	if ( !redirect_uri[0] ) {
		WN_HttpCtxSendStr( ctx, 400, "Bad Request", "text/plain", "Missing redirect_uri.\n" );
		return;
	}

	if ( state[0] ) {
		Com_sprintf( location, sizeof(location), "%s?code=wired-auto&state=%s",
		             redirect_uri, state );
	} else {
		Com_sprintf( location, sizeof(location), "%s?code=wired-auto", redirect_uri );
	}

	WN_HttpCtxSendRedirect( ctx, location );
}


/*
====================
WN_HttpHandleToken

POST /token — issues a Bearer token without validating credentials.
The MCP server ignores the token; this just satisfies the MCP SDK's
mandatory OAuth handshake (MCP spec 2025-03-26, HTTP transport).
====================
*/
static void WN_HttpHandleToken( wn_http_ctx_t *ctx )
{
	WN_HttpCtxSendStr( ctx, 200, "OK", "application/json",
		"{\"access_token\":\"wired-local\","
		"\"token_type\":\"Bearer\","
		"\"expires_in\":86400}" );
}

/*
====================
WN_HttpHandleMcp

POST /mcp — HTTP transport for the MCP JSON-RPC dispatcher.
Extracts the POST body using Content-Length, calls WN_McpDispatch,
responds with application/json.  No auth check — relies on network ACL.
====================
*/
static void WN_HttpHandleMcp( wn_http_ctx_t *ctx, const char *req, int req_len )
{
	static char   response[HTTP_MCP_RESP_MAX];
	const char   *body;
	const char   *cl_hdr;
	int           body_len = 0;

	// Find body after blank line
	body = strstr( req, "\r\n\r\n" );
	if ( !body ) {
		WN_HttpCtxSendStr( ctx, 400, "Bad Request", "application/json",
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Bad request\"},\"id\":null}" );
		return;
	}
	body += 4;  // skip \r\n\r\n

	// Parse Content-Length (case-insensitive search)
	cl_hdr = strstr( req, "Content-Length:" );
	if ( !cl_hdr ) cl_hdr = strstr( req, "content-length:" );
	if ( cl_hdr ) {
		cl_hdr += 15;  // len("Content-Length:")
		while ( *cl_hdr == ' ' ) cl_hdr++;
		body_len = atoi( cl_hdr );
	}
	// Fallback: use remaining buffer
	if ( body_len <= 0 ) {
		body_len = req_len - (int)(body - req);
	}

	if ( body_len <= 0 ) {
		WN_HttpCtxSendStr( ctx, 400, "Bad Request", "application/json",
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Empty body\"},\"id\":null}" );
		return;
	}
	if ( body_len > WN_MCP_JSON_BUF_SIZE - 1 ) {
		WN_HttpCtxSendStr( ctx, 413, "Request Too Large", "application/json",
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Request too large\"},\"id\":null}" );
		return;
	}

	WN_McpDispatch( body, body_len, response, sizeof(response), NULL );
	WN_HttpCtxSendStr( ctx, 200, "OK", "application/json", response );
}
#endif  // FEAT_WIREDNET_CONTROL


/*
====================
WN_HttpRoute (internal)

Parse the request line, apply rate limit, and dispatch to the appropriate
handler.  Called from both WN_HttpHandleRequest (QUIC) and WN_TcpFrame (TCP).
====================
*/
static void WN_HttpRoute( wn_http_ctx_t *ctx, const byte *data, int len )
{
	char req[HTTP_MAX_REQUEST];
	char method[16];
	char path[256];
	int  i, j;

	// Per-IP rate limit — 429 on excess
	if ( WN_HttpRateLimitExceeded( &ctx->addr ) ) {
		WN_HttpCtxSendStr( ctx, 429, "Too Many Requests",
			"text/plain", "Rate limit exceeded.\n" );
		return;
	}

	if ( len >= (int)sizeof(req) )
		len = (int)sizeof(req) - 1;
	memcpy( req, data, len );
	req[len] = '\0';

	i = 0; j = 0;
	while ( i < len && req[i] != ' ' && j < (int)sizeof(method) - 1 )
		method[j++] = req[i++];
	method[j] = '\0';

	while ( i < len && req[i] == ' ' ) i++;

	j = 0;
	while ( i < len && req[i] != ' ' && req[i] != '?' && j < (int)sizeof(path) - 1 )
		path[j++] = req[i++];
	path[j] = '\0';

	// POST handlers: /mcp (JSON-RPC) and /token (OAuth no-auth handshake)
	if ( Q_stricmp( method, "POST" ) == 0 ) {
#if FEAT_WIREDNET_CONTROL
		if ( Q_stricmp( path, "/mcp" ) == 0 ) {
			WN_HttpHandleMcp( ctx, req, len );
		} else if ( Q_stricmp( path, "/token" ) == 0 ) {
			WN_HttpHandleToken( ctx );
		} else if ( Q_stricmp( path, "/register" ) == 0 ) {
			WN_HttpHandleRegister( ctx, req, len );
		} else {
			WN_HttpCtxSendStr( ctx, 405, "Method Not Allowed",
				"text/plain", "Method Not Allowed.\n" );
		}
#else
		WN_HttpCtxSendStr( ctx, 404, "Not Found",
			"text/plain", "MCP not enabled.\n" );
#endif
		return;
	}

	if ( Q_stricmp( method, "GET" ) != 0 ) {
		WN_HttpCtxSendStr( ctx, 405, "Method Not Allowed",
			"text/plain", "Only GET is supported.\n" );
		return;
	}

#if FEAT_WIREDNET_CONTROL
	if ( Q_stricmp( path, "/.well-known/oauth-authorization-server" ) == 0 ) {
		WN_HttpHandleOAuthDiscover( ctx );
		return;
	}
	if ( Q_stricmp( path, "/authorize" ) == 0 ) {
		WN_HttpHandleAuthorize( ctx, req );
		return;
	}
#endif

	if ( Q_stricmp( path, "/health" ) == 0 ) {
		WN_HttpHandleHealth( ctx );
	} else if ( Q_stricmp( path, "/metrics" ) == 0 ) {
		WN_HttpHandleMetrics( ctx );
	} else if ( Q_stricmp( path, "/status.json" ) == 0 ) {
		WN_HttpHandleStatus( ctx );
	} else {
		WN_HttpHandleStaticFile( ctx, path );
	}
}


/*
====================
WN_HttpHandleRequest

Public QUIC entry point — called from the picoquic callback when data
arrives on an HTTP stream.  Creates a QUIC wn_http_ctx_t and routes.
====================
*/
void WN_HttpHandleRequest( wn_connection_t *conn, uint64_t stream_id,
                            const byte *data, int len )
{
	wn_http_ctx_t ctx;

	if ( !conn || !conn->active )
		return;

	memset( &ctx, 0, sizeof(ctx) );
	ctx.is_tcp    = qfalse;
	ctx.conn      = conn;
	ctx.stream_id = stream_id;
	ctx.addr      = conn->addr;

	WN_HttpRoute( &ctx, data, len );
}


/*
====================
WN_TcpFrame

Per-frame TCP accept loop.  Accepts up to 4 new connections per frame,
reads the HTTP request in a single recv(), routes via WN_HttpRoute,
then closes the socket.  One request per connection — no keep-alive.

The listen socket is set non-blocking in WN_Init so accept() returns
immediately (EAGAIN/EWOULDBLOCK) when no clients are pending.

Called from SV_Frame after WN_PushEvents.
====================
*/
void WN_TcpFrame( void )
{
	int i;

	if ( !wn.initialized || wn.tcp4_fd < 0 )
		return;

	for ( i = 0; i < 4; i++ ) {
		struct sockaddr_storage ss;
		socklen_t               ss_len = (socklen_t)sizeof(ss);
		int                     client_fd;
		netadr_t                addr;
		char                    req[HTTP_MAX_REQUEST];
		int                     recv_len;
		wn_http_ctx_t           ctx;

		client_fd = (int)accept( wn.tcp4_fd, (struct sockaddr *)&ss, &ss_len );
		if ( client_fd < 0 )
			break;  // EAGAIN / EWOULDBLOCK — no more pending connections

		// Build netadr_t for rate limiting (same pattern as WN_FlushOutbound)
		memset( &addr, 0, sizeof(addr) );
		if ( ss.ss_family == AF_INET ) {
			struct sockaddr_in *v4 = (struct sockaddr_in *)&ss;
			addr.type = NA_IP;
			memcpy( addr.ipv._4, &v4->sin_addr.s_addr, 4 );
			addr.port = v4->sin_port;
		}
#if FEAT_IPV6
		else if ( ss.ss_family == AF_INET6 ) {
			struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&ss;
			addr.type = NA_IP6;
			memcpy( addr.ipv._6, &v6->sin6_addr, 16 );
			addr.port = v6->sin6_port;
		}
#endif

		// Single recv — sufficient for an HTTP/1.0-style short request header.
		recv_len = (int)recv( client_fd, req, sizeof(req) - 1, 0 );
		if ( recv_len <= 0 ) {
			tcp_close( client_fd );
			continue;
		}

		memset( &ctx, 0, sizeof(ctx) );
		ctx.is_tcp = qtrue;
		ctx.tcp_fd = client_fd;
		ctx.addr   = addr;

		WN_HttpRoute( &ctx, (const byte *)req, recv_len );

		tcp_close( client_fd );
	}
}

#endif // FEAT_WIREDNET_OBSERVER
