/*
===========================================================================
wt_connection.c — QUIC connection lifecycle, auth, permissions
===========================================================================
*/
#include "wt_local.h"

#if FEAT_QUIC_TRANSPORT

/*
====================
WT_FindConnection

Look up a connection by its picoquic handle.
Returns NULL if not found.
====================
*/
wt_connection_t *WT_FindConnection( picoquic_cnx_t *cnx )
{
	int i;
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		if ( wt.connections[i].active && wt.connections[i].cnx == cnx )
			return &wt.connections[i];
	}
	return NULL;
}


/*
====================
WT_AllocConnection

Allocate a free connection slot for a new QUIC peer.
Returns NULL if all slots are in use.
====================
*/
wt_connection_t *WT_AllocConnection( picoquic_cnx_t *cnx, netadr_t *from )
{
	int i;
	for ( i = 0; i < WT_MAX_CLIENTS; i++ ) {
		if ( !wt.connections[i].active ) {
			wt_connection_t *conn = &wt.connections[i];
			Com_Memset( conn, 0, sizeof( *conn ) );
			conn->cnx = cnx;
			conn->active = qtrue;
			conn->connect_time = Sys_Microseconds();
			conn->perm = WT_PERM_OBSERVER_MEMBER_USER; // default: lowest privilege

			if ( from ) {
				conn->addr = *from;
			}

			wt.num_connections++;
			return conn;
		}
	}
	return NULL;
}


/*
====================
WT_FreeConnection

Release a connection slot back to the pool.
====================
*/
void WT_FreeConnection( wt_connection_t *conn )
{
	if ( !conn || !conn->active )
		return;

	conn->active = qfalse;
	conn->cnx = NULL;
	wt.num_connections--;
}


/*
====================
WT_ParseAuthToken

Parse a "connection:role:authority:token" string and return the permission bitmask.
Returns 0 (OBSERVER+MEMBER+USER) if the format is invalid.

Token format in sv_quicAuthToken cvar:
  "observer:member:user:abc123,observer:leader:admin:secret456"

Each entry is comma-separated. Fields within each entry are colon-separated.
====================
*/
uint8_t WT_ParseAuthToken( const char *token_str )
{
	uint8_t perm = 0;

	if ( !token_str || !*token_str )
		return WT_PERM_OBSERVER_MEMBER_USER;

	// Parse connection type
	if ( Q_stricmpn( token_str, "player:", 7 ) == 0 ) {
		perm |= WT_PERM_PLAYER;
		token_str += 7;
	} else if ( Q_stricmpn( token_str, "observer:", 9 ) == 0 ) {
		token_str += 9;
	} else {
		return WT_PERM_OBSERVER_MEMBER_USER; // malformed
	}

	// Parse team role
	if ( Q_stricmpn( token_str, "leader:", 7 ) == 0 ) {
		perm |= WT_PERM_LEADER;
		token_str += 7;
	} else if ( Q_stricmpn( token_str, "member:", 7 ) == 0 ) {
		token_str += 7;
	} else {
		return WT_PERM_OBSERVER_MEMBER_USER; // malformed
	}

	// Parse authority (last field — no trailing colon after truncation)
	if ( Q_stricmpn( token_str, "admin", 5 ) == 0 ) {
		perm |= WT_PERM_ADMIN;
	} else if ( Q_stricmpn( token_str, "user", 4 ) == 0 ) {
		// user = no admin bit
	} else {
		return WT_PERM_OBSERVER_MEMBER_USER; // malformed
	}

	return perm;
}


/*
====================
WT_LogConnect / WT_LogDisconnect

Print structured connection events to server console.
Format: "QUIC: client connected from X [OBSERVER+MEMBER+USER] (N/M slots)"
====================
*/
void WT_LogConnect( wt_connection_t *conn )
{
	const char *conn_type = WT_HasPermPlayer(conn->perm)  ? "PLAYER"   : "OBSERVER";
	const char *role      = WT_HasPermLeader(conn->perm)   ? "LEADER"   : "MEMBER";
	const char *auth      = WT_HasPermAdmin(conn->perm)    ? "ADMIN"    : "USER";

	Com_Printf( "QUIC: client connected from %s [%s+%s+%s] (%d/%d slots)\n",
		NET_AdrToString( &conn->addr ),
		conn_type, role, auth,
		wt.num_connections, wt.sv_quicMaxClients->integer );
}

void WT_LogDisconnect( wt_connection_t *conn, const char *reason )
{
	Com_Printf( "QUIC: client disconnected from %s (reason: %s)\n",
		NET_AdrToString( &conn->addr ),
		reason ? reason : "unknown" );
}


/*
====================
WT_MatchAuthToken

Check a client-provided token against the sv_quicAuthToken cvar.
Returns the permission bitmask for the matching entry, or 0xFF on no match.

Token cvar format: "connection:role:authority:token,connection:role:authority:token,..."
====================
*/
static uint8_t WT_MatchAuthToken( const char *client_token )
{
	const char *cvar_val;
	char        entry[256];
	const char *p, *comma;
	int         entry_len;

	if ( !client_token || !*client_token )
		return 0xFF; // no token provided

	cvar_val = wt.sv_quicAuthToken->string;
	if ( !cvar_val || !*cvar_val ) {
		// LAN mode — empty cvar = accept all with lowest privilege
		return WT_PERM_OBSERVER_MEMBER_USER;
	}

	// Iterate comma-separated entries
	p = cvar_val;
	while ( *p ) {
		// Find next comma or end
		comma = strchr( p, ',' );
		entry_len = comma ? (int)(comma - p) : (int)strlen(p);
		if ( entry_len >= (int)sizeof(entry) )
			entry_len = (int)sizeof(entry) - 1;

		Com_Memcpy( entry, p, entry_len );
		entry[entry_len] = '\0';

		// Entry format: "connection:role:authority:token"
		// Find the last colon — everything after it is the token value
		{
			char *last_colon = strrchr( entry, ':' );
			if ( last_colon ) {
				const char *stored_token = last_colon + 1;
				if ( strcmp( stored_token, client_token ) == 0 ) {
					// Match — parse the permission prefix
					*last_colon = '\0'; // temporarily truncate
					return WT_ParseAuthToken( entry );
				}
			}
		}

		p += entry_len;
		if ( *p == ',' ) p++;
	}

	return 0xFF; // no match
}


/*
====================
WT_HandleCapabilityNegotiation

Parse the client's capability request on Stream 0 and send a response
with granted channels based on their auth token and permission level.

Client sends:
{
  "protocol": 72,
  "token": "their-secret-token",
  "channels": ["datagrams", "events", "commands", "mcp", "http"]
}

Server responds:
{
  "protocol": 72,
  "granted": ["datagrams", "events"],
  "permission": {"connection": "observer", "role": "member", "authority": "user"},
  "server": {"hostname": "...", "map": "...", "gametype": 0}
}
====================
*/
void WT_HandleCapabilityNegotiation( wt_connection_t *conn, uint64_t stream_id,
                                      const byte *data, int len )
{
	char json[4096];
	char resp[4096];
	const char *token;
	int  token_len;
	uint8_t perm;
	char token_buf[256];
	const char *conn_type, *role, *auth;

	if ( !conn || conn->authenticated )
		return; // already negotiated

	// Copy to null-terminated buffer
	if ( len >= (int)sizeof(json) )
		len = (int)sizeof(json) - 1;
	Com_Memcpy( json, data, len );
	json[len] = '\0';

	// Extract token
	token = NULL;
	token_len = 0;
	{
		const char *t = strstr( json, "\"token\"" );
		if ( t ) {
			t += 7;
			while ( *t == ':' || *t == ' ' || *t == '\t' ) t++;
			if ( *t == '"' ) {
				t++;
				const char *end = strchr( t, '"' );
				if ( end ) {
					token_len = (int)(end - t);
					if ( token_len < (int)sizeof(token_buf) ) {
						Com_Memcpy( token_buf, t, token_len );
						token_buf[token_len] = '\0';
						token = token_buf;
					}
				}
			}
		}
	}

	// Authenticate
	perm = WT_MatchAuthToken( token );
	if ( perm == 0xFF ) {
		// Auth failed — reject connection
		Com_sprintf( resp, sizeof(resp),
			"{\"protocol\":69,\"error\":\"authentication failed\"}" );
		picoquic_add_to_stream( conn->cnx, stream_id,
			(const uint8_t *)resp, strlen(resp), 0 );
		picoquic_close( conn->cnx, 1 ); // application error
		return;
	}

	conn->perm = perm;
	conn->authenticated = qtrue;

	// Update logging with actual permissions
	WT_LogConnect( conn );

	// Build response
	conn_type = WT_HasPermPlayer(perm)  ? "player"   : "observer";
	role      = WT_HasPermLeader(perm)  ? "leader"   : "member";
	auth      = WT_HasPermAdmin(perm)   ? "admin"    : "user";

	Com_sprintf( resp, sizeof(resp),
		"{\"protocol\":69,\"version\":\"0.1.0\","
		"\"granted\":[\"datagrams\",\"events\"%s%s],"
		"\"permission\":{\"connection\":\"%s\",\"role\":\"%s\",\"authority\":\"%s\"},"
		"\"server\":{"
		"\"hostname\":\"%s\","
		"\"map\":\"%s\","
		"\"gametype\":%d,"
		"\"state_rate\":%d,"
		"\"event_rate\":%d"
		"}}",
		( WT_HasPermLeader(perm) || WT_HasPermAdmin(perm) ) ? ",\"commands\",\"mcp\"" : "",
#if FEAT_QUIC_HTTP
		",\"http\"",
#else
		"",
#endif
		conn_type, role, auth,
		Cvar_VariableString( "sv_hostname" ),
		Cvar_VariableString( "mapname" ),
		Cvar_VariableIntegerValue( "g_gametype" ),
		wt.sv_quicStateRate->integer,
		wt.sv_quicEventRate->integer );

	picoquic_add_to_stream( conn->cnx, stream_id,
		(const uint8_t *)resp, strlen(resp), 0 );

#if FEAT_QUIC_OBSERVE
	// Open server-initiated unidirectional event stream (0x03)
	conn->event_stream_id = picoquic_get_next_local_stream_id( conn->cnx, 1 ); // 1 = unidirectional
#endif
}

#endif // FEAT_QUIC_TRANSPORT
