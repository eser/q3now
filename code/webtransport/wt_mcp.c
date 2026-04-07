/*
===========================================================================
wt_mcp.c — Model Context Protocol (MCP) server over QUIC

JSON-RPC 2.0 framed as newline-delimited JSON (NDJSON) on a QUIC
bidirectional stream (client-initiated, stream ID 0x08+).

MCP specification: https://modelcontextprotocol.io/
Protocol version: 2025-03-26

Implements:
  - initialize handshake
  - tools/list  — enumerate available tools
  - tools/call  — execute a tool (game_status, event_history)

The JSON parser is deliberately minimal — no third-party JSON library.
We build JSON responses by string concatenation (safe for the small,
predictable payloads we emit). Incoming JSON is parsed with a minimal
hand-rolled tokenizer that handles the JSON-RPC envelope.
===========================================================================
*/
#include "wt_local.h"
#include "../server/server.h"  // for svs, sv, client_t

#if FEAT_QUIC_CONTROL

#define MCP_PROTOCOL_VERSION  "2025-03-26"
#define MCP_SERVER_NAME       "q3-engine"
#define MCP_SERVER_VERSION    "69.0.1"
#define MCP_MAX_RESPONSE      (32 * 1024)  // 32KB max response

// ── Minimal JSON helpers ─────────────────────────────────────────

// Find a string value for a given key in a JSON object (flat, no nesting).
// Returns pointer to the value start (after the opening quote), or NULL.
// Sets *out_len to the length of the value string (excluding quotes).
static const char *WT_JsonFindString( const char *json, const char *key, int *out_len )
{
	const char *p;
	char search[128];
	int key_len;

	key_len = Com_sprintf( search, sizeof(search), "\"%s\"", key );
	if ( key_len <= 0 ) return NULL;

	p = strstr( json, search );
	if ( !p ) return NULL;

	p += key_len;
	// skip : and whitespace
	while ( *p == ':' || *p == ' ' || *p == '\t' ) p++;

	if ( *p != '"' ) return NULL;
	p++; // skip opening quote

	// find closing quote
	{
		const char *end = strchr( p, '"' );
		if ( !end ) return NULL;
		*out_len = (int)(end - p);
		return p;
	}
}

// Find an integer value for a given key.
static int WT_JsonFindInt( const char *json, const char *key, int *out_val )
{
	const char *p;
	char search[128];
	int key_len;

	key_len = Com_sprintf( search, sizeof(search), "\"%s\"", key );
	if ( key_len <= 0 ) return 0;

	p = strstr( json, search );
	if ( !p ) return 0;

	p += key_len;
	while ( *p == ':' || *p == ' ' || *p == '\t' ) p++;

	*out_val = atoi( p );
	return 1;
}


// ── Response builders ────────────────────────────────────────────

static int WT_McpRespond( wt_connection_t *conn, uint64_t stream_id,
                           const char *response )
{
	int len = (int)strlen( response );

	if ( !conn || !conn->cnx )
		return -1;

	// Write response + newline (NDJSON framing)
	picoquic_add_to_stream( conn->cnx, stream_id,
		(const uint8_t *)response, (size_t)len, 0 );
	picoquic_add_to_stream( conn->cnx, stream_id,
		(const uint8_t *)"\n", 1, 0 );

	return 0;
}


/*
====================
WT_McpHandleInitialize

MCP initialize request → capabilities response.
====================
*/
static void WT_McpHandleInitialize( wt_connection_t *conn, uint64_t stream_id, int req_id )
{
	char resp[2048];

	Com_sprintf( resp, sizeof(resp),
		"{\"jsonrpc\":\"2.0\",\"result\":{"
		"\"protocolVersion\":\"%s\","
		"\"capabilities\":{\"tools\":{}},"
		"\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}"
		"},\"id\":%d}",
		MCP_PROTOCOL_VERSION, MCP_SERVER_NAME, MCP_SERVER_VERSION, req_id );

	WT_McpRespond( conn, stream_id, resp );
}


/*
====================
WT_McpHandleToolsList

Return the list of available MCP tools.
====================
*/
static void WT_McpHandleToolsList( wt_connection_t *conn, uint64_t stream_id, int req_id )
{
	char resp[4096];

	Com_sprintf( resp, sizeof(resp),
		"{\"jsonrpc\":\"2.0\",\"result\":{\"tools\":["
		"{\"name\":\"game_status\","
		"\"description\":\"Get current game state: map, players, scores, gametype.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
		"{\"name\":\"event_history\","
		"\"description\":\"Get the last N game events (kills, damage, items, chat).\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"count\":{\"type\":\"integer\",\"description\":\"Number of events to return (default 20, max 100).\"}}}}"
#if FEAT_BOT_IMPROVEMENTS
		",{\"name\":\"bot.list\","
		"\"description\":\"List all bots with id, name, skill, team, alive status.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
		",{\"name\":\"bot.getState\","
		"\"description\":\"Get detailed state of a bot: goal, weapon, health, position.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"id\":{\"type\":\"integer\",\"description\":\"Bot client slot ID.\"}},\"required\":[\"id\"]}}"
		",{\"name\":\"bot.setSkill\","
		"\"description\":\"Set bot skill level (1-50, divide by 10 for float skill).\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"id\":{\"type\":\"integer\",\"description\":\"Bot client slot ID.\"},"
		"\"level\":{\"type\":\"integer\",\"description\":\"Skill level x10 (10=1.0, 50=5.0).\"}},\"required\":[\"id\",\"level\"]}}"
#endif
		"]},\"id\":%d}",
		req_id );

	WT_McpRespond( conn, stream_id, resp );
}


/*
====================
WT_McpHandleGameStatus

Return current game state as a JSON tool result.
====================
*/
static void WT_McpHandleGameStatus( wt_connection_t *conn, uint64_t stream_id, int req_id )
{
	char resp[MCP_MAX_RESPONSE];
	char players[MCP_MAX_RESPONSE - 512];
	int  offset = 0;
	int  i;
	int  first = 1;

	// Build players JSON array
	players[0] = '[';
	offset = 1;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t *cl = &svs.clients[i];
		if ( cl->state < CS_CONNECTED )
			continue;

		if ( !first ) {
			players[offset++] = ',';
		}
		first = 0;

		offset += Com_sprintf( players + offset, sizeof(players) - offset,
			"{\"id\":%d,\"name\":\"%s\",\"ping\":%d,\"active\":%s}",
			i,
			cl->name ? cl->name : "",
			cl->ping,
			cl->state == CS_ACTIVE ? "true" : "false" );

		if ( offset >= (int)sizeof(players) - 128 )
			break; // safety
	}

	players[offset++] = ']';
	players[offset] = '\0';

	Com_sprintf( resp, sizeof(resp),
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":"
		"\"{\\\"map\\\":\\\"%s\\\",\\\"gametype\\\":%d,\\\"time\\\":%d,\\\"players\\\":%s}\""
		"}],\"isError\":false},\"id\":%d}",
		Cvar_VariableString( "mapname" ),
		Cvar_VariableIntegerValue( "g_gametype" ),
		Sys_Milliseconds(),
		players,
		req_id );

	WT_McpRespond( conn, stream_id, resp );
}


/*
====================
WT_McpHandleEventHistory

Return last N events from the ring buffer as JSON.
====================
*/
static void WT_McpHandleEventHistory( wt_connection_t *conn, uint64_t stream_id,
                                       int req_id, int count )
{
	char resp[MCP_MAX_RESPONSE];
	int  offset;

	if ( count <= 0 ) count = 20;
	if ( count > 100 ) count = 100;

	// For now, return a simple message about event count
	// Full msgpack→JSON event decoding is a v1 feature
	offset = Com_sprintf( resp, sizeof(resp),
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":"
		"\"Event ring: %llu total events, seq range [%llu..%llu]\""
		"}],\"isError\":false},\"id\":%d}",
		(unsigned long long)wt.event_seq,
		(unsigned long long)( wt.event_seq > (uint64_t)count ? wt.event_seq - count : 0 ),
		(unsigned long long)wt.event_seq,
		req_id );

	(void)offset;
	WT_McpRespond( conn, stream_id, resp );
}


#if FEAT_BOT_IMPROVEMENTS
static void WT_McpHandleBotList( wt_connection_t *conn, uint64_t stream_id, int req_id );
static void WT_McpHandleBotGetState( wt_connection_t *conn, uint64_t stream_id, int req_id, int bot_id );
static void WT_McpHandleBotSetSkill( wt_connection_t *conn, uint64_t stream_id, int req_id, int bot_id, int skill_x10 );
#endif

/*
====================
WT_McpHandleMessage

Main MCP message dispatcher. Called from the picoquic callback when data
arrives on a client-initiated bidirectional stream (0x08+).

Parses the JSON-RPC envelope, dispatches to the appropriate handler.
====================
*/
void WT_McpHandleMessage( wt_connection_t *conn, uint64_t stream_id,
                           const byte *data, int len )
{
	char        json[WT_MCP_JSON_BUF_SIZE];
	const char *method;
	const char *tool_name;
	int         method_len, tool_len;
	int         req_id = 0;
	char        resp[512];

	if ( !conn || !conn->active )
		return;

	// Permission check — MCP requires LEADER or ADMIN authority
	if ( !WT_HasPermLeader(conn->perm) && !WT_HasPermAdmin(conn->perm) ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Insufficient permissions\"},\"id\":null}" );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}

	// Copy to null-terminated buffer (NDJSON lines may not be terminated)
	if ( len >= (int)sizeof(json) ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Request too large\"},\"id\":null}" );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}
	Com_Memcpy( json, data, len );
	json[len] = '\0';

	// Parse request ID
	WT_JsonFindInt( json, "id", &req_id );

	// Parse method
	method = WT_JsonFindString( json, "method", &method_len );
	if ( !method ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Missing method\"},\"id\":%d}", req_id );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}

	// Dispatch
	if ( method_len == 10 && Q_stricmpn( method, "initialize", 10 ) == 0 ) {
		WT_McpHandleInitialize( conn, stream_id, req_id );
	}
	else if ( method_len == 10 && Q_stricmpn( method, "tools/list", 10 ) == 0 ) {
		WT_McpHandleToolsList( conn, stream_id, req_id );
	}
	else if ( method_len == 10 && Q_stricmpn( method, "tools/call", 10 ) == 0 ) {
		// Find tool name in params
		tool_name = WT_JsonFindString( json, "name", &tool_len );
		if ( !tool_name ) {
			Com_sprintf( resp, sizeof(resp),
				"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"Missing tool name\"},\"id\":%d}", req_id );
			WT_McpRespond( conn, stream_id, resp );
			return;
		}

		if ( tool_len == 11 && Q_stricmpn( tool_name, "game_status", 11 ) == 0 ) {
			WT_McpHandleGameStatus( conn, stream_id, req_id );
		}
		else if ( tool_len == 13 && Q_stricmpn( tool_name, "event_history", 13 ) == 0 ) {
			int count = 20;
			WT_JsonFindInt( json, "count", &count );
			WT_McpHandleEventHistory( conn, stream_id, req_id, count );
		}
#if FEAT_BOT_IMPROVEMENTS
		else if ( tool_len == 8 && Q_stricmpn( tool_name, "bot.list", 8 ) == 0 ) {
			WT_McpHandleBotList( conn, stream_id, req_id );
		}
		else if ( tool_len == 12 && Q_stricmpn( tool_name, "bot.getState", 12 ) == 0 ) {
			int bot_id = -1;
			WT_JsonFindInt( json, "id", &bot_id );
			WT_McpHandleBotGetState( conn, stream_id, req_id, bot_id );
		}
		else if ( tool_len == 12 && Q_stricmpn( tool_name, "bot.setSkill", 12 ) == 0 ) {
			int bot_id = -1;
			int skill_x10 = 30; // default skill 3.0
			WT_JsonFindInt( json, "id", &bot_id );
			WT_JsonFindInt( json, "level", &skill_x10 );
			WT_McpHandleBotSetSkill( conn, stream_id, req_id, bot_id, skill_x10 );
		}
#endif
		else {
			Com_sprintf( resp, sizeof(resp),
				"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Unknown tool\"},\"id\":%d}", req_id );
			WT_McpRespond( conn, stream_id, resp );
		}
	}
	else {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":%d}", req_id );
		WT_McpRespond( conn, stream_id, resp );
	}
}

#if FEAT_BOT_IMPROVEMENTS
/*
====================
WT_McpHandleBotList

Return list of all bots with id, name, team, alive.
Server side: uses svs.clients[] and SV_GameClientNum().
====================
*/
static void WT_McpHandleBotList( wt_connection_t *conn, uint64_t stream_id, int req_id )
{
	char resp[MCP_MAX_RESPONSE];
	char *p = resp;
	int remaining = sizeof(resp);
	int i, n;
	client_t *cl;
	playerState_t *ps;
	qboolean first = qtrue;

	n = Com_sprintf( p, remaining,
		"{\"jsonrpc\":\"2.0\",\"result\":[{\"type\":\"text\",\"text\":\"[" );
	p += n; remaining -= n;

	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];
		if ( cl->state < CS_ACTIVE ) continue;
		if ( !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) continue;

		ps = SV_GameClientNum( i );

		if ( !first ) {
			n = Com_sprintf( p, remaining, "," ); p += n; remaining -= n;
		}
		first = qfalse;

		n = Com_sprintf( p, remaining,
			"{\\\"id\\\":%d,\\\"name\\\":\\\"%s\\\",\\\"team\\\":%d,\\\"alive\\\":%s}",
			i, cl->name,
			ps->persistant[PERS_TEAM],
			ps->pm_type != PM_DEAD ? "true" : "false" );
		p += n; remaining -= n;
	}

	n = Com_sprintf( p, remaining, "]\"}],\"id\":%d}", req_id );
	p += n;

	WT_McpRespond( conn, stream_id, resp );
}

/*
====================
WT_McpHandleBotGetState
====================
*/
static void WT_McpHandleBotGetState( wt_connection_t *conn, uint64_t stream_id, int req_id, int bot_id )
{
	char resp[MCP_MAX_RESPONSE];
	client_t *cl;
	playerState_t *ps;

	if ( bot_id < 0 || bot_id >= sv_maxclients->integer ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"Invalid bot ID\"},\"id\":%d}", req_id );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}

	cl = &svs.clients[bot_id];
	if ( cl->state < CS_ACTIVE || !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,\"message\":\"Not a bot or not active\"},\"id\":%d}", req_id );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}

	ps = SV_GameClientNum( bot_id );

	Com_sprintf( resp, sizeof(resp),
		"{\"jsonrpc\":\"2.0\",\"result\":[{\"type\":\"text\",\"text\":\""
		"{\\\"id\\\":%d,\\\"name\\\":\\\"%s\\\","
		"\\\"health\\\":%d,\\\"armor\\\":%d,\\\"armorClass\\\":%d,"
		"\\\"weapon\\\":%d,"
		"\\\"pos\\\":[%.0f,%.0f,%.0f],"
		"\\\"vel\\\":[%.0f,%.0f,%.0f],"
		"\\\"alive\\\":%s}\"}],\"id\":%d}",
		bot_id, cl->name,
		ps->stats[STAT_HEALTH], ps->stats[STAT_ARMOR], ps->stats[STAT_ARMORCLASS],
		ps->weapon,
		ps->origin[0], ps->origin[1], ps->origin[2],
		ps->velocity[0], ps->velocity[1], ps->velocity[2],
		ps->pm_type != PM_DEAD ? "true" : "false",
		req_id );

	WT_McpRespond( conn, stream_id, resp );
}

/*
====================
WT_McpHandleBotSetSkill
====================
*/
static void WT_McpHandleBotSetSkill( wt_connection_t *conn, uint64_t stream_id,
                                      int req_id, int bot_id, int skill_x10 )
{
	char resp[512];
	client_t *cl;
	float skill;

	if ( bot_id < 0 || bot_id >= sv_maxclients->integer ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"Invalid bot ID\"},\"id\":%d}", req_id );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}

	cl = &svs.clients[bot_id];
	if ( cl->state < CS_ACTIVE || !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) {
		Com_sprintf( resp, sizeof(resp),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,\"message\":\"Not a bot or not active\"},\"id\":%d}", req_id );
		WT_McpRespond( conn, stream_id, resp );
		return;
	}

	skill = (float)skill_x10 / 10.0f;
	if ( skill < 1.0f ) skill = 1.0f;
	if ( skill > 5.0f ) skill = 5.0f;

	// Set skill via console command — deferred to next frame
	Cbuf_ExecuteText( EXEC_APPEND, va("g_spSkill %d\n", (int)(skill + 0.5f)) );

	Com_sprintf( resp, sizeof(resp),
		"{\"jsonrpc\":\"2.0\",\"result\":[{\"type\":\"text\",\"text\":\"Skill set to %.1f\"}],\"id\":%d}",
		skill, req_id );

	WT_McpRespond( conn, stream_id, resp );
}
#endif // FEAT_BOT_IMPROVEMENTS


/*
====================
QUIC_ProcessCommandQueue

Process pending MCP/command requests from the QUIC command queue.
Called from SV_Frame. For v0, MCP messages are handled inline
in the picoquic callback, so this is a no-op placeholder for
future queued command processing.
====================
*/
void QUIC_ProcessCommandQueue( void )
{
	if ( !wt.initialized )
		return;

	// MCP messages are currently handled inline in WT_McpHandleMessage.
	// Future: queue commands and process them here to avoid mid-frame mutations.
}

#endif // FEAT_QUIC_CONTROL
