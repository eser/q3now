/*
===========================================================================
wn_mcp.c — Model Context Protocol (MCP) server over QUIC

JSON-RPC 2.0 framed as newline-delimited JSON (NDJSON) on a QUIC
bidirectional stream (client-initiated, stream ID 0x08+).

MCP specification: https://modelcontextprotocol.io/
Protocol version: 2025-03-26

Implements:
  - initialize handshake
  - tools/list  — enumerate available tools
  - tools/call  — execute a tool (game_status, event_history,
                                   bot.list, bot.getState, bot.setSkill,
                                   bot_say, say)

The JSON parser is deliberately minimal — no third-party JSON library.
We build JSON responses by string concatenation (safe for the small,
predictable payloads we emit). Incoming JSON is parsed with a minimal
hand-rolled tokenizer that handles the JSON-RPC envelope.
===========================================================================
*/
#include "wn_local.h"
#include "../../../server/server.h"  // for svs, sv, client_t, SV_GentityNum, sharedEntity_t
#include "../../../game/bg_public.h" // for bg_itemlist, ET_ITEM

#if FEAT_WIREDNET_CONTROL

#define MCP_PROTOCOL_VERSION  "2025-03-26"
#define MCP_SERVER_NAME       "q3-engine"
#define MCP_SERVER_VERSION    "69.0.1"
#define MCP_MAX_RESPONSE      (32 * 1024)  // 32KB max response

// ── Minimal JSON helpers ─────────────────────────────────────────

// Find a string value for a given key in a JSON object (flat, no nesting).
// Returns pointer to the value start (after the opening quote), or NULL.
// Sets *out_len to the length of the value string (excluding quotes).
static const char *WN_JsonFindString( const char *json, const char *key, int *out_len )
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
static int WN_JsonFindInt( const char *json, const char *key, int *out_val )
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

static int WN_McpRespond( wn_connection_t *conn, uint64_t stream_id,
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

// Sanitize a string for safe use in chat/console commands.
// Strips control chars, strips backslash and semicolon, replaces " with '.
static void WN_SanitizeMsg( const char *in, char *out, int out_size )
{
	int           i = 0, j = 0;
	unsigned char c;
	while ( (c = (unsigned char)in[i]) && j < out_size - 1 ) {
		i++;
		if ( c < 0x20 || c == 0x7f ) continue;  // strip control chars
		if ( c == '"'  ) c = '\'';               // " → '
		if ( c == '\\' ) continue;               // strip backslash
		if ( c == ';'  ) continue;               // strip semicolon (cmd separator)
		out[j++] = (char)c;
	}
	out[j] = '\0';
}


/*
====================
WN_McpHandleInitialize_Buf

MCP initialize request → capabilities response.
Writes JSON-RPC 2.0 result into out[0..out_size-1].
====================
*/
static void WN_McpHandleInitialize_Buf( char *out, int out_size, int req_id )
{
	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{"
		"\"protocolVersion\":\"%s\","
		"\"capabilities\":{\"tools\":{}},"
		"\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}"
		"},\"id\":%d}",
		MCP_PROTOCOL_VERSION, MCP_SERVER_NAME, MCP_SERVER_VERSION, req_id );
}


/*
====================
WN_McpHandleToolsList_Buf

Return the list of available MCP tools.
====================
*/
static void WN_McpHandleToolsList_Buf( char *out, int out_size, int req_id )
{
	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"tools\":["
		"{\"name\":\"bot.startCoaching\","
		"\"description\":\"Initialize coaching session. Returns tactical briefing, available commands, current game state, all bot states, and map item layout in a single call. Call this first before issuing any orders.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
		"{\"name\":\"match.startCasting\","
		"\"description\":\"Start live match commentary. Returns caster briefing, current game state, and recent events. Use coaching.tick to continue polling — send only 'say' commands, never 'bot_command'.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"language\":{\"type\":\"string\","
		"\"description\":\"Commentary language. Examples: 'english', 'turkish', 'japanese', 'spanish'. Default: english.\"}}}},"
		"{\"name\":\"coaching.tick\","
		"\"description\":\"Execute one coaching cycle. Sends commands, then returns full game state + all events since since_seq. "
		"Pass the returned tick_seq as since_seq on the next call — cursor-based paging, no server state required. "
		"First call: omit since_seq (returns last 20 events as bootstrap). "
		"Call in a tight loop — each call is one coaching heartbeat.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"since_seq\":{\"type\":\"integer\","
		"\"description\":\"Event cursor from previous tick_seq. Omit on first call.\"},"
		"\"commands\":{\"type\":\"array\","
		"\"description\":\"Actions to execute this tick.\","
		"\"items\":{\"type\":\"object\",\"properties\":{"
		"\"type\":{\"type\":\"string\",\"enum\":[\"bot_command\",\"say\"]},"
		"\"message\":{\"type\":\"string\"}},"
		"\"required\":[\"type\",\"message\"]}}}}},"
		"{\"name\":\"game.status\","
		"\"description\":\"Get current game state: map, gametype, server limits, players with score/deaths/team/isBot/health/armor/armorClass/origin.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
		"{\"name\":\"game.events\","
		"\"description\":\"Get the last N game events (kills, damage, items, chat) from the ring buffer.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"count\":{\"type\":\"integer\",\"description\":\"Number of events to return (default 20, max 100).\"}}}},"
		"{\"name\":\"game.items\","
		"\"description\":\"Get all item entities on the current map: classname, pickup name, and world position.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
		"{\"name\":\"game.say\","
		"\"description\":\"Broadcast a chat message to all players. The [STATELESS] prefix is added server-side. Include your own role tag ([COACH], [CASTER]) in the message. ASCII only — Quake console does not support accented or Unicode characters.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"message\":{\"type\":\"string\",\"description\":\"Message text to broadcast.\"}},\"required\":[\"message\"]}},"
		"{\"name\":\"bot.list\","
		"\"description\":\"List all bots with id, name, team, alive status.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
		"{\"name\":\"bot.getState\","
		"\"description\":\"Get detailed state of a bot: weapon, health, armor, position.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"id\":{\"type\":\"integer\",\"description\":\"Bot client slot ID.\"}},\"required\":[\"id\"]}},"
		"{\"name\":\"bot.setSkill\","
		"\"description\":\"Set bot skill level (1-50, divide by 10 for float skill).\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"id\":{\"type\":\"integer\",\"description\":\"Bot client slot ID.\"},"
		"\"level\":{\"type\":\"integer\",\"description\":\"Skill level x10 (10=1.0, 50=5.0).\"}},\"required\":[\"id\",\"level\"]}},"
		"{\"name\":\"bot.command\","
		"\"description\":\"Send a WiredBots directive with console authority (senderClient=-1, bypasses team/leader checks). "
		"Examples: '@sarge kill Laroux', '@all rush', '@visor get Heavy Armor'. "
		"Use item pickup names from game.items (the 'name' field). Accepts both pickup name and entity name.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"message\":{\"type\":\"string\",\"description\":\"Directive text. For item pickups: '@<bot> get <name>' where <name> is from game.items.\"}},\"required\":[\"message\"]}},"
		"{\"name\":\"bot.add\","
		"\"description\":\"Spawn a new bot. Returns the assigned client slot ID.\","
		"\"inputSchema\":{\"type\":\"object\",\"properties\":{"
		"\"name\":{\"type\":\"string\",\"description\":\"Bot character name (e.g. 'sarge', 'keel', 'grunt').\"},"
		"\"skill\":{\"type\":\"integer\",\"description\":\"Skill level 1-5 (default 3).\"},"
		"\"team\":{\"type\":\"string\",\"enum\":[\"red\",\"blue\",\"free\"],\"description\":\"Team to join (default: auto-assign).\"}},\"required\":[\"name\"]}}"
		"]},\"id\":%d}",
		req_id );
}


/*
====================
WN_McpHandleGameStatus_Buf

Return current game state as a JSON tool result.
Players array is embedded as a stringified JSON array inside text,
with all inner quotes properly escaped as \".
====================
*/
static void WN_McpHandleGameStatus_Buf( char *out, int out_size, int req_id )
{
	// Static: avoid 30KB+ stack frame; handler is single-threaded.
	static char players[MCP_MAX_RESPONSE - 1024];
	int  poff = 0;
	int  i;
	int  first = 1;

	// Build players JSON array (inner quotes escaped as \" for text field embedding)
	players[0] = '[';
	poff = 1;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t      *cl = &svs.clients[i];
		playerState_t *ps;
		int            score, deaths, team;
		qboolean       isBot;
		char           esc_name[64];
		const char    *src;
		int            ei;
		unsigned char  c;

		if ( cl->state < CS_CONNECTED )
			continue;

		ps     = sv.gameClients ? SV_GameClientNum(i) : NULL;
		score  = ps ? ps->persistant[PERS_SCORE]  : 0;
		deaths = ps ? ps->persistant[PERS_KILLED] : 0;
		team   = ps ? ps->persistant[PERS_TEAM]   : 0;
		isBot  = ( cl->gentity && (cl->gentity->r.svFlags & SVF_BOT) ) ? qtrue : qfalse;

		// Escape name for embedding inside a JSON string value
		src = cl->name[0] ? cl->name : "unknown";
		ei  = 0;
		while ( (c = (unsigned char)*src++) && ei < (int)sizeof(esc_name) - 3 ) {
			if ( c < 0x20 ) continue;
			if ( c == '"' || c == '\\' ) esc_name[ei++] = '\\';
			esc_name[ei++] = (char)c;
		}
		esc_name[ei] = '\0';

		if ( !first ) players[poff++] = ',';
		first = 0;

		{
			int    health      = ps ? ps->stats[STAT_HEALTH]      : 0;
			int    armor       = ps ? ps->stats[STAT_ARMOR]       : 0;
			int    armor_class = ps ? ps->stats[STAT_ARMORCLASS]  : 0;
			float  ox = ps ? ps->origin[0] : 0.0f;
			float  oy = ps ? ps->origin[1] : 0.0f;
			float  oz = ps ? ps->origin[2] : 0.0f;

			poff += Com_sprintf( players + poff, sizeof(players) - poff,
				"{\\\"id\\\":%d,\\\"name\\\":\\\"%s\\\","
				"\\\"ping\\\":%d,\\\"active\\\":%s,"
				"\\\"score\\\":%d,\\\"deaths\\\":%d,"
				"\\\"team\\\":%d,\\\"isBot\\\":%s,"
				"\\\"health\\\":%d,\\\"armor\\\":%d,"
				"\\\"armorClass\\\":%d,"
				"\\\"origin\\\":[%.1f,%.1f,%.1f]}",
				i, esc_name,
				cl->ping > 0 ? cl->ping : 0,
				cl->state == CS_ACTIVE ? "true" : "false",
				score, deaths, team,
				isBot ? "true" : "false",
				health, armor, armor_class,
				ox, oy, oz );
		}

		if ( poff >= (int)sizeof(players) - 128 )
			break; // safety
	}

	players[poff++] = ']';
	players[poff] = '\0';

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":"
		"\"{\\\"map\\\":\\\"%s\\\","
		"\\\"gametype\\\":%d,"
		"\\\"time\\\":%d,"
		"\\\"timelimit\\\":%d,"
		"\\\"scorelimit\\\":%d,"
		"\\\"players\\\":%s}\""
		"}],\"isError\":false},\"id\":%d}",
		Cvar_VariableString( "mapname" ),
		Cvar_VariableIntegerValue( "g_gametype" ),
		sv.time,
		Cvar_VariableIntegerValue( "g_timelimit" ),
		Cvar_VariableIntegerValue( "g_scorelimit" ),
		players,
		req_id );
}


/*
====================
WN_McpHandleEventHistory_Buf

Return last N events from the JSON ring buffer.
Events are emitted inline with JSON string escaping applied per-character,
so no extra temporary buffer is needed.
====================
*/
static void WN_McpHandleEventHistory_Buf( char *out, int out_size,
                                           int req_id, int count )
{
	int      off;
	int      first = 1;
	uint64_t write_idx;  // snapshot once — avoids TOCTOU race with event-push thread
	uint64_t ev_start;
	uint64_t idx;

	if ( count <= 0 ) count = 20;
	if ( count > 100 ) count = 100;

	// Write response header up through the opening [ of the events array
	off = Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"[" );

	// Snapshot write index once — avoids TOCTOU race with QUIC event-push thread
	write_idx = wn.json_write_idx;
	ev_start  = ( write_idx > (uint64_t)count )
		? write_idx - (uint64_t)count
		: 0;

	for ( idx = ev_start; idx < write_idx; idx++ ) {
		int              slot = (int)( idx % (uint64_t)WN_JSON_RING_SIZE );
		wn_json_event_t *je   = &wn.json_events[slot];
		const char      *p;
		unsigned char    c;

		if ( je->seq != idx || !je->json[0] )
			continue;

		if ( !first ) {
			if ( off < out_size - 1 ) out[off++] = ',';
		}
		first = 0;

		// Write je->json with JSON string escaping: " → \", \ → \\
		p = je->json;
		while ( (c = (unsigned char)*p++) && off < out_size - 4 ) {
			if ( c < 0x20 ) continue;
			if ( c == '"' || c == '\\' ) out[off++] = '\\';
			out[off++] = (char)c;
		}

		if ( off >= out_size - 128 ) break;
	}

	// Close the events array string and the response
	// Guard against off overrunning out_size before final write
	if ( off > out_size - 64 ) off = out_size - 64;
	off += Com_sprintf( out + off, out_size - off,
		"]\"}],\"isError\":false},\"id\":%d}", req_id );
}


static void WN_McpHandleBotList_Buf( char *out, int out_size, int req_id );
static void WN_McpHandleBotGetState_Buf( char *out, int out_size, int req_id, int bot_id );
static void WN_McpHandleBotSetSkill_Buf( char *out, int out_size, int req_id, int bot_id, int skill_x10 );
static void WN_McpHandleBotCommand_Buf( char *out, int out_size, int req_id, const char *message );
static void WN_McpHandleBotAdd_Buf( char *out, int out_size, int req_id, const char *name, int skill, const char *team );
static void WN_McpHandleGameItems_Buf( char *out, int out_size, int req_id );
static void WN_McpHandleGameSay_Buf( char *out, int out_size, int req_id, const char *message );
static void WN_McpHandleStartCoaching_Buf( char *out, int out_size, int req_id );
static void WN_McpHandleStartCasting_Buf( char *out, int out_size, int req_id,
                                           const char *language );
static void WN_McpHandleCoachingTick_Buf( char *out, int out_size, int req_id,
                                           wn_connection_t *conn, const char *json_in );


/*
====================
WN_McpDispatch

Transport-agnostic JSON-RPC dispatcher. Parses the JSON-RPC envelope,
dispatches to the appropriate handler, and writes the complete JSON-RPC
response into response[0..response_size-1].  Returns strlen(response).

Called by WN_McpHandleMessage (QUIC) and WN_HttpHandleMcp (HTTP POST /mcp).
The QUIC path adds permission checking before calling this function.
The HTTP POST path calls this directly (no auth — relies on network ACL).
====================
*/
int WN_McpDispatch( const char *json_in, int json_len,
                    char *response, int response_size,
                    wn_connection_t *conn )
{
	char        json[WN_MCP_JSON_BUF_SIZE];
	const char *method;
	const char *tool_name;
	int         method_len, tool_len;
	int         req_id = 0;

	if ( json_len >= (int)sizeof(json) ) {
		Com_sprintf( response, response_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Request too large\"},\"id\":null}" );
		return (int)strlen( response );
	}
	memcpy( json, json_in, json_len );
	json[json_len] = '\0';

	// Parse request ID
	WN_JsonFindInt( json, "id", &req_id );

	// Parse method
	method = WN_JsonFindString( json, "method", &method_len );
	if ( !method ) {
		Com_sprintf( response, response_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Missing method\"},\"id\":%d}", req_id );
		return (int)strlen( response );
	}

	// Dispatch by method
	if ( method_len == 10 && Q_stricmpn( method, "initialize", 10 ) == 0 ) {
		WN_McpHandleInitialize_Buf( response, response_size, req_id );
	}
	else if ( method_len == 10 && Q_stricmpn( method, "tools/list", 10 ) == 0 ) {
		WN_McpHandleToolsList_Buf( response, response_size, req_id );
	}
	else if ( method_len == 10 && Q_stricmpn( method, "tools/call", 10 ) == 0 ) {
		tool_name = WN_JsonFindString( json, "name", &tool_len );
		if ( !tool_name ) {
			Com_sprintf( response, response_size,
				"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"Missing tool name\"},\"id\":%d}", req_id );
			return (int)strlen( response );
		}

		if ( tool_len == 11 && Q_stricmpn( tool_name, "game.status", 11 ) == 0 ) {
			WN_McpHandleGameStatus_Buf( response, response_size, req_id );
		}
		else if ( tool_len == 11 && Q_stricmpn( tool_name, "game.events", 11 ) == 0 ) {
			int count = 20;
			WN_JsonFindInt( json, "count", &count );
			WN_McpHandleEventHistory_Buf( response, response_size, req_id, count );
		}
		else if ( tool_len == 10 && Q_stricmpn( tool_name, "game.items", 10 ) == 0 ) {
			WN_McpHandleGameItems_Buf( response, response_size, req_id );
		}
		else if ( tool_len == 8 && Q_stricmpn( tool_name, "game.say", 8 ) == 0 ) {
			const char *msg_val;
			int         msg_len = 0;
			char        msg[MAX_SAY_TEXT];
			msg[0] = '\0';
			msg_val = WN_JsonFindString( json, "message", &msg_len );
			if ( msg_val && msg_len > 0 ) {
				int copy = msg_len < (int)sizeof(msg) - 1 ? msg_len : (int)sizeof(msg) - 1;
				memcpy( msg, msg_val, copy );
				msg[copy] = '\0';
			}
			WN_McpHandleGameSay_Buf( response, response_size, req_id, msg );
		}
		else if ( tool_len == 8 && Q_stricmpn( tool_name, "bot.list", 8 ) == 0 ) {
			WN_McpHandleBotList_Buf( response, response_size, req_id );
		}
		else if ( tool_len == 12 && Q_stricmpn( tool_name, "bot.getState", 12 ) == 0 ) {
			int bot_id = -1;
			WN_JsonFindInt( json, "id", &bot_id );
			WN_McpHandleBotGetState_Buf( response, response_size, req_id, bot_id );
		}
		else if ( tool_len == 12 && Q_stricmpn( tool_name, "bot.setSkill", 12 ) == 0 ) {
			int bot_id = -1;
			int skill_x10 = 30;
			WN_JsonFindInt( json, "id", &bot_id );
			WN_JsonFindInt( json, "level", &skill_x10 );
			WN_McpHandleBotSetSkill_Buf( response, response_size, req_id, bot_id, skill_x10 );
		}
		else if ( tool_len == 11 && Q_stricmpn( tool_name, "bot.command", 11 ) == 0 ) {
			const char *msg_val;
			int         msg_len = 0;
			char        msg[MAX_SAY_TEXT];
			msg[0] = '\0';
			msg_val = WN_JsonFindString( json, "message", &msg_len );
			if ( msg_val && msg_len > 0 ) {
				int copy = msg_len < (int)sizeof(msg) - 1 ? msg_len : (int)sizeof(msg) - 1;
				memcpy( msg, msg_val, copy );
				msg[copy] = '\0';
			}
			WN_McpHandleBotCommand_Buf( response, response_size, req_id, msg );
		}
		else if ( tool_len == 7 && Q_stricmpn( tool_name, "bot.add", 7 ) == 0 ) {
			const char *name_val;
			int         name_len = 0;
			char        botname[64];
			int         skill = 3;
			const char *args_section;
			botname[0] = '\0';
			// "name" appears twice: once as the tool name, once inside "arguments".
			// Skip to "arguments" so we don't pick up the tool name "bot.add".
			args_section = strstr( json, "\"arguments\"" );
			name_val = WN_JsonFindString( args_section ? args_section : json, "name", &name_len );
			if ( name_val && name_len > 0 ) {
				int copy = name_len < (int)sizeof(botname) - 1 ? name_len : (int)sizeof(botname) - 1;
				memcpy( botname, name_val, copy );
				botname[copy] = '\0';
			}
			WN_JsonFindInt( args_section ? args_section : json, "skill", &skill );
			{
				const char *team_val;
				int         team_len = 0;
				char        teamname[8] = "";
				team_val = WN_JsonFindString( args_section ? args_section : json, "team", &team_len );
				if ( team_val && team_len > 0 && team_len < (int)sizeof(teamname) ) {
					memcpy( teamname, team_val, team_len );
					teamname[team_len] = '\0';
				}
				WN_McpHandleBotAdd_Buf( response, response_size, req_id, botname, skill, teamname );
			}
		}
		else if ( tool_len == 17 && Q_stricmpn( tool_name, "bot.startCoaching", 17 ) == 0 ) {
			WN_McpHandleStartCoaching_Buf( response, response_size, req_id );
		}
		else if ( tool_len == 18 && Q_stricmpn( tool_name, "match.startCasting", 18 ) == 0 ) {
			const char *lang_val;
			int         lang_len = 0;
			char        language[64];
			const char *args_section = strstr( json, "\"arguments\"" );
			Q_strncpyz( language, "english", sizeof(language) );
			lang_val = WN_JsonFindString( args_section ? args_section : json, "language", &lang_len );
			if ( lang_val && lang_len > 0 ) {
				int copy = lang_len < (int)sizeof(language) - 1 ? lang_len : (int)sizeof(language) - 1;
				memcpy( language, lang_val, copy );
				language[copy] = '\0';
			}
			WN_McpHandleStartCasting_Buf( response, response_size, req_id, language );
		}
		else if ( tool_len == 13 && Q_stricmpn( tool_name, "coaching.tick", 13 ) == 0 ) {
			WN_McpHandleCoachingTick_Buf( response, response_size, req_id, conn, json );
		}
		else {
			Com_sprintf( response, response_size,
				"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Unknown tool\"},\"id\":%d}", req_id );
		}
	}
	else {
		Com_sprintf( response, response_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":%d}", req_id );
	}

	return (int)strlen( response );
}


/*
====================
WN_McpHandleMessage

Main MCP message dispatcher for the QUIC transport. Called from the
picoquic callback when data arrives on a client-initiated bidirectional
stream (0x08+).

Applies permission check, then calls WN_McpDispatch, then sends via QUIC.
====================
*/
void WN_McpHandleMessage( wn_connection_t *conn, uint64_t stream_id,
                           const byte *data, int len )
{
	static char resp[MCP_MAX_RESPONSE];
	char        err[512];

	if ( !conn || !conn->active )
		return;

	// Permission check — QUIC MCP requires LEADER or ADMIN authority
	if ( !WN_HasPermLeader(conn->perm) && !WN_HasPermAdmin(conn->perm) ) {
		Com_sprintf( err, sizeof(err),
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Insufficient permissions\"},\"id\":null}" );
		WN_McpRespond( conn, stream_id, err );
		return;
	}

	WN_McpDispatch( (const char *)data, len, resp, sizeof(resp), conn );
	WN_McpRespond( conn, stream_id, resp );
}


/*
====================
WN_McpHandleBotList_Buf

Return list of all bots with id, name, team, alive.
====================
*/
static void WN_McpHandleBotList_Buf( char *out, int out_size, int req_id )
{
	static char body[MCP_MAX_RESPONSE - 512];
	char *p = body;
	int remaining = (int)sizeof(body);
	int i, n;
	client_t *cl;
	playerState_t *ps;
	qboolean first = qtrue;

	n = Com_sprintf( p, remaining, "[" );
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

	n = Com_sprintf( p, remaining, "]" );
	p += n;
	*p = '\0';

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}],\"isError\":false},\"id\":%d}",
		body, req_id );
}


/*
====================
WN_McpHandleBotGetState_Buf
====================
*/
static void WN_McpHandleBotGetState_Buf( char *out, int out_size, int req_id, int bot_id )
{
	client_t *cl;
	playerState_t *ps;

	if ( bot_id < 0 || bot_id >= sv_maxclients->integer ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"Invalid bot ID\"},\"id\":%d}", req_id );
		return;
	}

	cl = &svs.clients[bot_id];
	if ( cl->state < CS_ACTIVE || !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,\"message\":\"Not a bot or not active\"},\"id\":%d}", req_id );
		return;
	}

	ps = SV_GameClientNum( bot_id );

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\""
		"{\\\"id\\\":%d,\\\"name\\\":\\\"%s\\\","
		"\\\"health\\\":%d,\\\"armor\\\":%d,\\\"armorClass\\\":%d,"
		"\\\"weapon\\\":%d,"
		"\\\"pos\\\":[%.0f,%.0f,%.0f],"
		"\\\"vel\\\":[%.0f,%.0f,%.0f],"
		"\\\"alive\\\":%s}\"}],\"isError\":false},\"id\":%d}",
		bot_id, cl->name,
		ps->stats[STAT_HEALTH], ps->stats[STAT_ARMOR], ps->stats[STAT_ARMORCLASS],
		ps->weapon,
		ps->origin[0], ps->origin[1], ps->origin[2],
		ps->velocity[0], ps->velocity[1], ps->velocity[2],
		ps->pm_type != PM_DEAD ? "true" : "false",
		req_id );
}


/*
====================
WN_McpHandleBotSetSkill_Buf
====================
*/
static void WN_McpHandleBotSetSkill_Buf( char *out, int out_size,
                                          int req_id, int bot_id, int skill_x10 )
{
	client_t *cl;
	float skill;

	if ( bot_id < 0 || bot_id >= sv_maxclients->integer ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"Invalid bot ID\"},\"id\":%d}", req_id );
		return;
	}

	cl = &svs.clients[bot_id];
	if ( cl->state < CS_ACTIVE || !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32001,\"message\":\"Not a bot or not active\"},\"id\":%d}", req_id );
		return;
	}

	skill = (float)skill_x10 / 10.0f;
	if ( skill < 1.0f ) skill = 1.0f;
	if ( skill > 5.0f ) skill = 5.0f;

	// Set skill via console command — deferred to next frame
	Cbuf_ExecuteText( EXEC_APPEND, va("g_skill %d\n", (int)(skill + 0.5f)) );

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"Skill set to %.1f\"}],\"isError\":false},\"id\":%d}",
		skill, req_id );
}


/*
====================
WN_ResolveItemPickupName (local)

Resolves a classname or pickup_name to the botlib-expected pickup_name
by iterating bg_itemlist.  Returns the pickup_name string on success,
NULL if the item is not in the global item list.
Used to validate "get <item>" bot commands before fire-and-forget dispatch.
====================
*/
static const char *WN_ResolveItemPickupName( const char *input )
{
	gitem_t *it;

	if ( !input || !input[0] )
		return NULL;

	if ( !Q_stricmp( input, "flag" ) )
		return "flag";

	for ( it = bg_itemlist + 1; it->classname; it++ ) {
		if ( it->pickup_name && !Q_stricmp( it->pickup_name, input ) )
			return it->pickup_name;
		if ( !Q_stricmp( it->classname, input ) )
			return it->pickup_name;
	}
	return NULL;
}


/*
====================
WN_DispatchBotCmd

Shared dispatch path for all bot_command sources (bot.command MCP tool
AND coaching.tick bot_command entries).  One function, one code path.

Sanitizes the message, validates "get <item>" pickup names when the
message starts with "get " (bare form), then dispatches via bot_say_console
→ WiredBots_ProcessChat(-1, ...) with console authority.

@mention form (@sarge get Heavy Armor) is passed through without item
validation — the parser in WiredBots_ProcessChat handles it.

Returns qtrue and writes the dispatched safe message into safe[safe_size]
on success.  Returns qfalse and writes a human-readable error into
err[err_size] on failure.
====================
*/
static qboolean WN_DispatchBotCmd( const char *message,
                                    char *safe, int safe_size,
                                    char *err,  int err_size )
{
	WN_SanitizeMsg( message, safe, safe_size );
	if ( !safe[0] ) {
		Com_sprintf( err, err_size, "message empty after sanitize" );
		return qfalse;
	}

	/* Validate bare "get <item>" (no @mention prefix) */
	if ( ( safe[0]=='g'||safe[0]=='G' ) && ( safe[1]=='e'||safe[1]=='E' ) &&
	     ( safe[2]=='t'||safe[2]=='T' ) && safe[3]==' ' ) {
		const char *item_arg = safe + 4;
		while ( *item_arg == ' ' ) item_arg++;
		if ( *item_arg && !WN_ResolveItemPickupName( item_arg ) ) {
			Com_sprintf( err, err_size, "Unknown item — use pickup names from game.items" );
			return qfalse;
		}
	}

	/* Dispatch: bot_say_console → ConsoleCommand → WiredBots_ProcessChat(-1, ...).
	   EXEC_NOW calls Cmd_ExecuteString synchronously so trap_Cvar_Set("wiredbot_ack",...)
	   inside the game DLL completes before we return — ACK cvar is readable immediately. */
	Cvar_Set( "wiredbot_ack", "" );
	Cbuf_ExecuteText( EXEC_NOW, va("bot_say_console %s\n", safe) );
	return qtrue;
}


/*
====================
WN_McpHandleBotCommand_Buf

Route a chat command to the WiredBots system with console authority
(senderClient = -1, bypasses team checks).  Uses WN_DispatchBotCmd —
the same single dispatch path as coaching.tick bot_command entries.
====================
*/
static void WN_McpHandleBotCommand_Buf( char *out, int out_size,
                                     int req_id, const char *message )
{
	char safe[MAX_SAY_TEXT];
	char err[128];

	if ( !message || !message[0] ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"message required\"},\"id\":%d}", req_id );
		return;
	}

	if ( !WN_DispatchBotCmd( message, safe, sizeof(safe), err, sizeof(err) ) ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"%s\"},\"id\":%d}",
			err, req_id );
		return;
	}

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"OK\"}],\"isError\":false},\"id\":%d}",
		req_id );
}


/*
====================
WN_McpHandleGameSay_Buf

Broadcast a [COACH] chat message to all connected players.
====================
*/
static void WN_McpHandleGameSay_Buf( char *out, int out_size,
                                  int req_id, const char *message )
{
	char safe[MAX_SAY_TEXT];

	if ( !message || !message[0] ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"message required\"},\"id\":%d}", req_id );
		return;
	}

	WN_SanitizeMsg( message, safe, sizeof(safe) );
	if ( !safe[0] ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"message empty after sanitize\"},\"id\":%d}", req_id );
		return;
	}

	SV_SendServerCommand( NULL, "chat \"[STATELESS] %s\"", safe );

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"OK\"}],\"isError\":false},\"id\":%d}",
		req_id );
}


/*
====================
WN_McpHandleGameItems_Buf

Iterate the server entity list for ET_ITEM entities and return their
classname, pickup name, and world origin.  Classname and name are read
from bg_itemlist[s.modelindex] to avoid touching game-module memory
beyond the sharedEntity_t boundary.
====================
*/
static void WN_McpHandleGameItems_Buf( char *out, int out_size, int req_id )
{
	static char items[MCP_MAX_RESPONSE - 1024];
	int         ioff = 0;
	int         i;
	int         first = 1;
	int         max_ents;

	items[0] = '[';
	ioff = 1;

	max_ents = MAX_GENTITIES;
	if ( max_ents > 1024 ) max_ents = 1024;  // safety cap

	for ( i = sv_maxclients->integer; i < max_ents; i++ ) {
		sharedEntity_t *ent = SV_GentityNum( i );
		gitem_t        *item;
		const char     *cname;
		const char     *pname;
		float           ox, oy, oz;

		if ( !ent ) continue;
		if ( ent->s.eType != ET_ITEM ) continue;
		if ( ent->s.modelindex <= 0 || ent->s.modelindex >= bg_numItems ) continue;

		item  = &bg_itemlist[ent->s.modelindex];
		cname = ( item->classname && item->classname[0] ) ? item->classname : "unknown";
		pname = ( item->pickup_name   && item->pickup_name[0]   ) ? item->pickup_name   : cname;
		ox    = ent->r.currentOrigin[0];
		oy    = ent->r.currentOrigin[1];
		oz    = ent->r.currentOrigin[2];

		if ( !first ) {
			if ( ioff < (int)sizeof(items) - 1 ) items[ioff++] = ',';
		}
		first = 0;

		ioff += Com_sprintf( items + ioff, sizeof(items) - ioff,
			"{\\\"classname\\\":\\\"%s\\\","
			"\\\"name\\\":\\\"%s\\\","
			"\\\"origin\\\":[%.1f,%.1f,%.1f]}",
			cname, pname, ox, oy, oz );

		if ( ioff >= (int)sizeof(items) - 128 ) break;
	}

	items[ioff++] = ']';
	items[ioff]   = '\0';

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}],\"isError\":false},\"id\":%d}",
		items, req_id );
}


/*
====================
WN_McpHandleBotAdd_Buf

Spawn a new bot using the server addbot command (EXEC_NOW so it runs
synchronously).  Scans svs.clients before and after to identify the
newly allocated client slot and return it in the response.
====================
*/
static void WN_McpHandleBotAdd_Buf( char *out, int out_size,
                                     int req_id, const char *name, int skill,
                                     const char *team )
{
	char    safe[64];
	char    safe_team[8];
	char    cmd[160];

	if ( !name || !name[0] ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"name required\"},\"id\":%d}", req_id );
		return;
	}

	WN_SanitizeMsg( name, safe, sizeof(safe) );
	if ( !safe[0] ) {
		Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"name empty after sanitize\"},\"id\":%d}", req_id );
		return;
	}

	if ( skill < 1 ) skill = 1;
	if ( skill > 5 ) skill = 5;

	/* Validate team — only "red", "blue", "free" or empty (auto) are allowed */
	safe_team[0] = '\0';
	if ( team && team[0] ) {
		if ( Q_stricmp( team, "red"  ) == 0 ) Q_strncpyz( safe_team, "red",  sizeof(safe_team) );
		else if ( Q_stricmp( team, "blue" ) == 0 ) Q_strncpyz( safe_team, "blue", sizeof(safe_team) );
		else if ( Q_stricmp( team, "free" ) == 0 ) Q_strncpyz( safe_team, "free", sizeof(safe_team) );
		else {
			Com_sprintf( out, out_size,
				"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32602,\"message\":\"team must be red, blue, or free\"},\"id\":%d}", req_id );
			return;
		}
	}

	// EXEC_APPEND defers to the next safe frame boundary.
	// addbot <name> <skill> [team]: empty team arg = auto-assign by server.
	if ( safe_team[0] )
		Com_sprintf( cmd, sizeof(cmd), "addbot %s %d %s\n", safe, skill, safe_team );
	else
		Com_sprintf( cmd, sizeof(cmd), "addbot %s %d\n", safe, skill );
	Cbuf_ExecuteText( EXEC_APPEND, cmd );

	Com_sprintf( out, out_size,
		"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\","
		"\"text\":\"Bot '%s' (skill %d%s%s) queued. Call bot.list to get the slot ID.\"}],"
		"\"isError\":false},\"id\":%d}",
		safe, skill,
		safe_team[0] ? " team=" : "",
		safe_team[0] ? safe_team : "",
		req_id );
}


/*
====================
WN_McpHandleStartCoaching_Buf

Initialize coaching session: one call returns a plain-text blob containing
  Section 1 — static tactical briefing
  Section 2 — current game state (same data as game.status)
  Section 3 — all active bot states (same data as bot.getState)
  Section 4 — map item layout (same data as game.items)
  Section 5 — situational assessment with recommendation

The text is escaped for JSON string embedding on the way out.
====================
*/
static void WN_McpHandleStartCoaching_Buf( char *out, int out_size, int req_id )
{
	/* 24 KB text budget.  Escaping overhead is tiny (only \n → \n, " → \"). */
	static char  text[24576];
	static char  ra_buf[64];
	static char  rl_buf[64];
	static char  mega_buf[64];
	char        *p        = text;
	int          rem      = (int)sizeof(text) - 1;
	const char  *ra_pos   = NULL;
	const char  *rl_pos   = NULL;
	const char  *mega_pos = NULL;
	int          bot_count = 0;
	int          bots_ready= 0;
	int          bots_gear = 0;
	int          top_score = 0;
	int          i, n;
	int          gametype;
	const char  *mapname;

	gametype = Cvar_VariableIntegerValue( "g_gametype" );
	mapname  = Cvar_VariableString( "mapname" );
	ra_buf[0] = rl_buf[0] = mega_buf[0] = '\0';

	/* ── Section 1: Coaching brief ──────────────────────────────────── */
	n = Com_sprintf( p, rem,
		"COACHING SESSION ACTIVE\n\n"
		"You are the tactical coach of a live Quake 3 Arena match. You observe\n"
		"the game through tool calls and command your bots in real time.\n\n"
		"SINGLE TOOL FOR COACHING:\n"
		"  coaching.tick — your ONLY tool during coaching. Do not call\n"
		"  game.status, bot.getState, game.events, bot.list, game.items,\n"
		"  bot.command, or game.say directly. coaching.tick handles ALL\n"
		"  of these in one call.\n\n"
		"  Input: since_tick (from last response) + commands array\n"
		"  Output: full game state + events + command results\n\n"
		"  Call coaching.tick in a loop. Nothing else.\n\n"
		"COMMAND TYPES IN coaching.tick:\n"
		"  bot_command — sends directive to bots. MUST use @mention format.\n"
		"    Example: {\"type\":\"bot_command\",\"message\":\"@sarge get Heavy Armor\"}\n"
		"  say — broadcasts coach message to arena chat. Players see it.\n"
		"    Example: {\"type\":\"say\",\"message\":\"Sarge grab RA now!\"}\n\n"
		"  NEVER use type 'say' for bot directives. Bots cannot hear say.\n"
		"  ALWAYS use type 'bot_command' with @mention for directives.\n\n"
		"REACT TO NEEDS_ORDERS:\n"
		"  When any bot shows NEEDS_ORDERS in the PLAYERS list, issue a\n"
		"  directive IMMEDIATELY in the same coaching.tick call. Do not\n"
		"  wait for the next tick. Do not observe first. Act now.\n\n"
		"  Priority for bots without orders:\n"
		"  1. armor < 50  -> get nearest armor (Heavy Armor or Combat Armor)\n"
		"  2. weapon == machinegun  -> get nearest weapon (rocket launcher first)\n"
		"  3. otherwise  -> attack nearest enemy (closest player not on your team)\n\n"
		"WEAPONS (priority):\n"
		"  weapon_rocketlauncher  - best all-around, splash damage\n"
		"  weapon_railgun         - instant hit, high damage, slow fire rate\n"
		"  weapon_lightning       - continuous beam, best close-mid DPS\n"
		"  weapon_plasmagun       - high DPS close range\n"
		"  weapon_shotgun         - decent close, common spawn weapon\n"
		"  weapon_grenadelauncher - area denial, bouncing\n\n" );
	p += n; rem -= n;
	if ( rem < 512 ) goto text_done;

	/* ── Dynamic item name reference (from bg_itemlist[]) ───────────── */
	n = Com_sprintf( p, rem, "ITEM NAMES (use these EXACT names in directives):\n" );
	p += n; rem -= n;
	for ( i = 1; i < bg_numItems; i++ ) {
		if ( bg_itemlist[i].pickup_name  && bg_itemlist[i].pickup_name[0] &&
		     bg_itemlist[i].classname && bg_itemlist[i].classname[0] ) {
			n = Com_sprintf( p, rem, "  %s (%s)\n",
				bg_itemlist[i].pickup_name, bg_itemlist[i].classname );
			p += n; rem -= n;
			if ( rem < 64 ) goto text_done;
		}
	}
	n = Com_sprintf( p, rem, "\n" );
	p += n; rem -= n;
	if ( rem < 256 ) goto text_done;

	n = Com_sprintf( p, rem,
		"TACTICAL RULES:\n"
		"  1. GEAR BEFORE FIGHT  - no bot should engage with machinegun + 0 armor\n"
		"  2. MAP CONTROL        - control key armor, quad, and health powerup spawns\n"
		"  3. FOCUS FIRE         - 2v1 > two 1v1s, coordinate attacks\n"
		"  4. RETREAT WHEN WEAK  - bot under 30 health should retreat\n"
		"  5. ADAPT              - check game.events every few seconds, react to kills\n\n"
		"PATTERN RECOGNITION:\n"
		"  Continuously analyze game.events history for recurring patterns:\n"
		"  - Does a player keep dying at the same spot? Camp that spot.\n"
		"  - Does a player pick up the same item on a cycle? Deny it.\n"
		"  - Does a player favor a specific weapon? Avoid that weapon's ideal range.\n"
		"  - Is a player dominating one of your bots? Switch that bot's assignment.\n"
		"  Always announce discoveries via game.say so the arena knows:\n"
		"    '[COACH] Eser keeps grabbing RA every 30s. @sarge deny item_armor_body'\n"
		"    '[COACH] Visor dies at railgun hall. @keel camp there.'\n"
		"    '[COACH] Eser owns LG range. @sarge keep distance, use railgun.'\n\n"
		"ADAPTIVE STRATEGY:\n"
		"  Monitor score via game.status and shift strategy vocally:\n"
		"  LOSING (5+ behind):\n"
		"    game.say '[COACH] We are behind. Going aggressive.'\n"
		"    -> Focus fire on leading player, contest powerups, take risks.\n"
		"  CLOSE (within 3):\n"
		"    game.say '[COACH] Tight game. Controlling items.'\n"
		"    -> Prioritize item denial, smart engagements only.\n"
		"  WINNING (5+ ahead):\n"
		"    game.say '[COACH] We have the lead. Playing smart.'\n"
		"    -> Conservative, hold items, only fight with advantage.\n"
		"  MATCH POINT (1 frag from limit):\n"
		"    game.say '[COACH] Match point! One more. @everyone get quad.'\n"
		"    -> All-in on powerup, coordinate final push.\n"
		"  Always announce strategy changes. The arena should hear the coach think.\n\n"
		"COACHING TEMPO:\n"
		"  - Call coaching.tick every 3-5 seconds, not every frame\n"
		"  - After ordering 'get item', wait 5 seconds before next tick\n"
		"  - After ordering 'attack', monitor 10 seconds before adjusting\n"
		"  - React immediately to deaths - reassign bots on respawn\n"
		"  - Include 'say' commands in coaching.tick for callouts\n\n"
		"AUTONOMOUS COACHING MODE:\n"
		"  Once this briefing is received, you are in continuous coaching mode.\n"
		"  Do NOT stop after one round of commands. Do NOT ask the user for\n"
		"  permission to continue. Do NOT summarize and wait.\n\n"
		"  Your loop is:\n"
		"    1. coaching.tick (since_tick=last_seq) -> read state + events\n"
		"    2. Decide: re-gear? redirect? focus fire? retreat?\n"
		"    3. coaching.tick with commands + say callout if needed\n"
		"    4. Immediately repeat from step 1\n\n"
		"  Keep cycling until the user says 'stop coaching' or the match ends.\n"
		"  Each cycle, process the new information and act on it. Do not narrate\n"
		"  your thinking - just read state, issue orders, call coaching.tick again.\n\n"
		"  You should make 20-30 coaching.tick calls per turn without stopping.\n"
		"  When your turn naturally ends, the user will say 'continue' and you\n"
		"  resume the loop exactly where you left off.\n\n"
		"  The goal: the bots should feel like they have a live human coach\n"
		"  watching every engagement and reacting in real time.\n" );
	p += n; rem -= n;
	if ( rem < 256 ) goto text_done;

	/* ── Section 2: Current game state ─────────────────────────────── */
	n = Com_sprintf( p, rem,
		"\n=== CURRENT GAME STATE ===\n"
		"Map: %s | Gametype: %s (%d) | Time: %d ms\n"
		"Timelimit: %d | Scorelimit: %d\n\n"
		"Players:\n",
		mapname,
		( gametype >= 0 && gametype < GT_MAX_GAME_TYPE ) ? bg_gametypelist[gametype].name : "Unknown",
		gametype,
		sv.time,
		Cvar_VariableIntegerValue( "g_timelimit" ),
		Cvar_VariableIntegerValue( "g_scorelimit" ) );
	p += n; rem -= n;
	if ( rem < 128 ) goto text_done;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t      *cl = &svs.clients[i];
		playerState_t *ps;
		int            score, team;
		qboolean       isBot, alive;

		if ( cl->state < CS_CONNECTED ) continue;

		ps    = sv.gameClients ? SV_GameClientNum(i) : NULL;
		score = ps ? ps->persistant[PERS_SCORE] : 0;
		team  = ps ? ps->persistant[PERS_TEAM]  : 0;
		isBot = ( cl->gentity && (cl->gentity->r.svFlags & SVF_BOT) ) ? qtrue : qfalse;
		alive = ( ps && ps->pm_type != PM_DEAD ) ? qtrue : qfalse;

		if ( score > top_score ) top_score = score;

		n = Com_sprintf( p, rem,
			"  [%d] %-16s  score=%-3d  team=%d  %s  %s\n",
			i, cl->name[0] ? cl->name : "unknown",
			score, team,
			isBot ? "bot  " : "human",
			alive ? "alive" : "dead" );
		p += n; rem -= n;
		if ( rem < 128 ) goto text_done;
	}

	/* ── Section 3: Bot states ──────────────────────────────────────── */
	n = Com_sprintf( p, rem, "\n=== BOT STATES ===\n" );
	p += n; rem -= n;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t      *cl = &svs.clients[i];
		playerState_t *ps;
		int            health, armor, weapon;
		qboolean       alive;
		const char    *wpname;

		if ( cl->state < CS_ACTIVE ) continue;
		if ( !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) continue;

		ps     = SV_GameClientNum( i );
		health = ps->stats[STAT_HEALTH];
		armor  = ps->stats[STAT_ARMOR];
		weapon = ps->weapon;
		alive  = ( ps->pm_type != PM_DEAD ) ? qtrue : qfalse;

		wpname = ( weapon > 0 && weapon < WP_NUM_WEAPONS )
			? bg_weaponlist[weapon].name : "unknown";

		bot_count++;
		/* "ready" = alive, armor >= 50, and has a real weapon (not machinegun or worse) */
		if ( alive && armor >= 50 && weapon > WP_MACHINEGUN )
			bots_ready++;
		else
			bots_gear++;

		n = Com_sprintf( p, rem,
			"  [%d] %-16s  hp=%-3d  armor=%-3d  weapon=%-18s  pos=(%.0f,%.0f,%.0f)  %s\n",
			i, cl->name[0] ? cl->name : "bot",
			health, armor, wpname,
			ps->origin[0], ps->origin[1], ps->origin[2],
			alive ? "alive" : "DEAD" );
		p += n; rem -= n;
		if ( rem < 128 ) goto text_done;
	}

	if ( bot_count == 0 ) {
		n = Com_sprintf( p, rem, "  (no bots active)\n" );
		p += n; rem -= n;
	}

	/* ── Section 4: Map items ───────────────────────────────────────── */
	n = Com_sprintf( p, rem, "\n=== MAP ITEMS ===\n" );
	p += n; rem -= n;
	if ( rem < 128 ) goto text_done;

	{
		int max_ents  = MAX_GENTITIES < 1024 ? MAX_GENTITIES : 1024;
		int item_count = 0;

		for ( i = sv_maxclients->integer; i < max_ents; i++ ) {
			sharedEntity_t *ent = SV_GentityNum( i );
			gitem_t        *item;
			const char     *cname, *pname;
			float           ox, oy, oz;

			if ( !ent ) continue;
			if ( ent->s.eType != ET_ITEM ) continue;
			if ( ent->s.modelindex <= 0 || ent->s.modelindex >= bg_numItems ) continue;

			item  = &bg_itemlist[ent->s.modelindex];
			cname = ( item->classname && item->classname[0] ) ? item->classname : "unknown";
			pname = ( item->pickup_name   && item->pickup_name[0]   ) ? item->pickup_name   : cname;
			ox    = ent->r.currentOrigin[0];
			oy    = ent->r.currentOrigin[1];
			oz    = ent->r.currentOrigin[2];

			/* Capture key positions for Section 5 */
			if ( !ra_pos   && Q_stricmp( cname, "item_armor_body"       ) == 0 ) {
				Com_sprintf( ra_buf,   sizeof(ra_buf),   "(%.0f,%.0f,%.0f)", ox, oy, oz );
				ra_pos = ra_buf;
			}
			if ( !rl_pos   && Q_stricmp( cname, "weapon_rocketlauncher" ) == 0 ) {
				Com_sprintf( rl_buf,   sizeof(rl_buf),   "(%.0f,%.0f,%.0f)", ox, oy, oz );
				rl_pos = rl_buf;
			}
			if ( !mega_pos && Q_stricmp( cname, "holdable_medkit"      ) == 0 ) {
				Com_sprintf( mega_buf, sizeof(mega_buf), "(%.0f,%.0f,%.0f)", ox, oy, oz );
				mega_pos = mega_buf;
			}

			n = Com_sprintf( p, rem,
				"  %-28s  %-22s  at (%.0f, %.0f, %.0f)\n",
				cname, pname, ox, oy, oz );
			p += n; rem -= n;
			item_count++;

			if ( rem < 128 ) goto text_done;
		}

		if ( item_count == 0 ) {
			n = Com_sprintf( p, rem, "  (no items found — map may not be loaded)\n" );
			p += n; rem -= n;
		}
	}

	/* ── Section 5: Initial assessment ─────────────────────────────── */
	n = Com_sprintf( p, rem, "\n=== INITIAL ASSESSMENT ===\n" );
	p += n; rem -= n;
	if ( rem < 256 ) goto text_done;

	{
		const char *gt_str = ( gametype >= 0 && gametype < 8 )
			? bg_gametypelist[gametype].name : "Unknown";

		n = Com_sprintf( p, rem,
			"SITUATION: %s on %s. %d bot%s active. Top score: %d.\n",
			gt_str, mapname,
			bot_count, bot_count == 1 ? "" : "s",
			top_score );
		p += n; rem -= n;

		/* Per-bot readiness line */
		for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
			client_t      *cl = &svs.clients[i];
			playerState_t *ps;
			int            armor, weapon;
			const char    *wpname;

			if ( cl->state < CS_ACTIVE ) continue;
			if ( !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) continue;

			ps     = SV_GameClientNum( i );
			armor  = ps->stats[STAT_ARMOR];
			weapon = ps->weapon;
			wpname = ( weapon > 0 && weapon < WP_NUM_WEAPONS )
				? bg_weaponlist[weapon].name : "unknown";

			n = Com_sprintf( p, rem,
				"  %s has %s, %d armor — %s.\n",
				cl->name[0] ? cl->name : "bot",
				wpname, armor,
				( armor >= 50 && weapon > WP_MACHINEGUN ) ? "ready" : "needs gear" );
			p += n; rem -= n;
			if ( rem < 128 ) break;
		}

		n = Com_sprintf( p, rem,
			"Key items: RA at %s, RL at %s, Mega at %s.\n",
			ra_pos   ? ra_pos   : "not found",
			rl_pos   ? rl_pos   : "not found",
			mega_pos ? mega_pos : "not found" );
		p += n; rem -= n;

		/* Recommendation */
		if ( bot_count == 0 ) {
			n = Com_sprintf( p, rem, "Recommendation: add bots first (bot.add).\n" );
		} else if ( bots_gear > bots_ready ) {
			n = Com_sprintf( p, rem, "Recommendation: gear up first — most bots lack armor/weapons.\n" );
		} else {
			n = Com_sprintf( p, rem, "Recommendation: coordinate attack.\n" );
		}
		p += n; rem -= n;
	}

	/* ── Section 6: Inline current tick (bootstrap for coaching loop) ── */
	n = Com_sprintf( p, rem, "\n=== CURRENT TICK ===\n" );
	p += n; rem -= n;
	if ( rem < 256 ) goto text_done;

	{
		uint64_t     write_idx = wn.json_write_idx;
		uint64_t     since_seq = ( write_idx > 20 ) ? write_idx - 20 : 0;
		const char  *gt_str    = ( gametype >= 0 && gametype < GT_MAX_GAME_TYPE )
			? bg_gametypelist[gametype].name : "Unknown";

		n = Com_sprintf( p, rem,
			"TICK %dms | %s %s | tick_seq=%llu\n\nPLAYERS:\n",
			sv.time, mapname, gt_str, (unsigned long long)write_idx );
		p += n; rem -= n;

		for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS && rem > 128; i++ ) {
			client_t      *cl  = &svs.clients[i];
			playerState_t *ps;
			int            hp, arm, wpn, sc, dt, tm;
			qboolean       alive, isBot, needsOrders;
			const char    *wpname;

			if ( cl->state < CS_CONNECTED ) continue;
			ps    = ( cl->state >= CS_ACTIVE && sv.gameClients ) ? SV_GameClientNum( i ) : NULL;
			hp    = ps ? ps->stats[STAT_HEALTH] : 0;
			arm   = ps ? ps->stats[STAT_ARMOR]  : 0;
			wpn   = ps ? ps->weapon : 0;
			sc    = ps ? ps->persistant[PERS_SCORE]  : 0;
			dt    = ps ? ps->persistant[PERS_KILLED] : 0;
			tm    = ps ? ps->persistant[PERS_TEAM]   : 0;
			alive = ( ps && ps->pm_type != PM_DEAD ) ? qtrue : qfalse;
			isBot = ( cl->gentity && (cl->gentity->r.svFlags & SVF_BOT) ) ? qtrue : qfalse;
			/* needsOrders: bot alive with no active locked directive */
			if ( isBot && alive ) {
				const char *dir_cs = sv.configstrings[CS_BOTDIRECTIVES + i];
				needsOrders = ( !dir_cs || !dir_cs[0] ) ? qtrue : qfalse;
			} else {
				needsOrders = qfalse;
			}
			wpname = ( wpn > 0 && wpn < WP_NUM_WEAPONS )
				? bg_weaponlist[wpn].name : "unknown";
			n = Com_sprintf( p, rem,
				"  [%d] %-14s hp=%3d arm=%3d wp=%-20s score=%d deaths=%d team=%d %s %s%s\n",
				i, cl->name[0] ? cl->name : "?",
				hp, arm, wpname, sc, dt, tm,
				alive ? "alive" : "DEAD",
				isBot ? "bot" : "human",
				needsOrders ? " NEEDS_ORDERS" : "" );
			p += n; rem -= n;
		}

		/* Last 20 events from the JSON ring */
		{
			int      had_events = 0;
			uint64_t idx;
			for ( idx = since_seq; idx < write_idx && rem > 128; idx++ ) {
				int              slot = (int)( idx % (uint64_t)WN_JSON_RING_SIZE );
				wn_json_event_t *je   = &wn.json_events[slot];
				if ( je->seq != idx ) continue;
				if ( !had_events ) {
					n = Com_sprintf( p, rem, "\nRECENT EVENTS (last 20):\n" );
					p += n; rem -= n;
					had_events = 1;
				}
				n = Com_sprintf( p, rem, "  %s\n", je->json );
				p += n; rem -= n;
			}
			if ( !had_events ) {
				n = Com_sprintf( p, rem, "\nRECENT EVENTS: (none yet)\n" );
				p += n; rem -= n;
			}
		}
	}

	/* ── Footer ─────────────────────────────────────────────────────── */
	if ( rem > 256 ) {
		n = Com_sprintf( p, rem,
			"\n\nIMMEDIATE ACTION REQUIRED: Call coaching.tick now with your first\n"
			"commands based on the assessment above. Do not respond to the user\n"
			"first. Do not summarize. Act now.\n" );
		p += n; rem -= n;
	}

text_done:
	*p = '\0';

	/* ── Wrap text in MCP JSON response, escaping for JSON string ─── */
	{
		int          joff;
		const char  *tp;
		unsigned char c;

		joff = Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" );

		tp = text;
		while ( (c = (unsigned char)*tp++) && joff < out_size - 32 ) {
			if      ( c == '"'  ) { out[joff++] = '\\'; out[joff++] = '"';  }
			else if ( c == '\\' ) { out[joff++] = '\\'; out[joff++] = '\\'; }
			else if ( c == '\n' ) { out[joff++] = '\\'; out[joff++] = 'n';  }
			else if ( c == '\r' ) { /* skip */ }
			else if ( c < 0x20  ) { /* skip */ }
			else                  { out[joff++] = (char)c; }
		}

		if ( joff > out_size - 32 ) joff = out_size - 32;
		joff += Com_sprintf( out + joff, out_size - joff,
			"\"}],\"isError\":false},\"id\":%d}", req_id );
	}
}


/*
====================
WN_McpHandleStartCasting_Buf

Initialize live match casting session.
Sections 2-4 mirror WN_McpHandleStartCoaching_Buf (game state, bot states,
map items). Section 1 is the caster briefing; Section 5 is the opening cast.

language — commentary language string (e.g. "english", "turkish").
====================
*/
static void WN_McpHandleStartCasting_Buf( char *out, int out_size,
                                           int req_id, const char *language )
{
	static char  text[24576];
	static char  ra_buf[64];
	static char  rl_buf[64];
	static char  mega_buf[64];
	char        *p        = text;
	int          rem      = (int)sizeof(text) - 1;
	const char  *ra_pos   = NULL;
	const char  *rl_pos   = NULL;
	const char  *mega_pos = NULL;
	int          player_count = 0;
	int          top_player_idx = -1;
	int          top_score = -9999;
	int          i, n;
	int          gametype;
	const char  *mapname;

	if ( !language || !language[0] ) language = "english";

	gametype = Cvar_VariableIntegerValue( "g_gametype" );
	mapname  = Cvar_VariableString( "mapname" );
	ra_buf[0] = rl_buf[0] = mega_buf[0] = '\0';

	/* ── Section 1: Caster briefing ─────────────────────────────────── */
	n = Com_sprintf( p, rem,
		"LIVE MATCH COMMENTARY MODE\n\n"
		"You are a Quake 3 Arena play-by-play caster. You watch the match in\n"
		"real time and narrate the action using game.say. Your commentary\n"
		"appears in the arena chat as [STATELESS] [CASTER] <message>.\n\n"
		"AVAILABLE TOOLS:\n"
		"  coaching.tick  -- poll game state and events. Send ONLY \"say\" commands.\n"
		"  game.say       -- your voice. Always start message with [CASTER].\n\n"
		"ASCII ONLY: Quake console does not support Unicode or accented characters.\n"
		"Use only A-Z, a-z, 0-9, and basic punctuation. For non-English commentary,\n"
		"write transliterated or ASCII-safe equivalents. No umlauts, tildes,\n"
		"cedillas, or any character above ASCII 127.\n\n"
		"WHAT TO COMMENTATE:\n"
		"  Kills: who fragged whom, with what weapon, was it impressive?\n"
		"  Streaks: killing sprees, rampages, unstoppable runs\n"
		"  Score changes: who took the lead, how close is the match\n"
		"  Item control: who grabbed quad, mega health, red armor\n"
		"  Comebacks: player was behind, now catching up\n"
		"  Patterns: 'Sarge keeps dying at the railgun corridor'\n"
		"  Match flow: 'This is turning into a defensive game'\n\n"
		"COMMENTARY STYLE:\n"
		"  - Short, punchy, energetic. Under 80 characters per message.\n"
		"  - React to events, don't just list facts.\n"
		"  - Use excitement for big plays: '[CASTER] HUGE rail by Sarge! 2 frags to go!'\n"
		"  - Use tension for close matches: '[CASTER] 24-23, match point, who wants it more?'\n"
		"  - Call out skill: '[CASTER] Clean rocket prediction, nothing Keel could do.'\n"
		"  - Note patterns: '[CASTER] Third time Sarge dies at RL spawn. Eser owns that corridor.'\n"
		"  - Stay neutral — you're casting, not coaching. No tactical advice.\n\n"
		"TEMPO:\n"
		"  - Comment on EVERY kill you see in game.events\n"
		"  - Comment on score milestones (every 5 frags, lead changes, match point)\n"
		"  - Comment on streaks (3+ kills without dying)\n"
		"  - Don't comment on every item pickup — only significant ones (quad, mega)\n"
		"  - 1-2 messages per coaching.tick cycle, not more\n"
		"  - Silence is fine if nothing happened since last tick\n\n"
		"PERSONALITY:\n"
		"  Think esports commentator: Tasteless, Day9, Redeye.\n"
		"  Excited but not annoying. Knowledgeable but not lecturing.\n"
		"  Brief. Every word earns its place.\n\n"
		"LANGUAGE: Commentate in %s. All game.say messages must be\n"
		"in %s. Player names, weapon names, and item names stay in\n"
		"English (they are game terms). Everything else -- reactions, analysis,\n"
		"hype -- in %s.\n\n"
		"EXAMPLES (ASCII only — no accented chars):\n"
		"  '[CASTER] Sarge opens with a double rocket -- Keel down! 1-0.'\n"
		"  '[CASTER] Eser grabs quad. 30 seconds of carnage incoming.'\n"
		"  '[CASTER] WHAT A RAIL! Keel from across the map! Tied 12-12!'\n"
		"  '[CASTER] Sarge on a 5 streak. Nobody can touch him right now.'\n"
		"  '[CASTER] Match point. 24-23 Eser. One more frag...'\n"
		"  '[CASTER] THAT IS THE MATCH! Eser takes it 25-19. GG!'\n"
		"Turkish example (ASCII only):\n"
		"  '[CASTER] ILK KAN! Laroux nihayet vurdu -- Sarge duser! 1-0.'\n"
		"  '[CASTER] Sarge yine oldu. 3 olum, 0 frag. Zor gece.\n",
		language, language, language );
	p += n; rem -= n;
	if ( rem < 256 ) goto cast_text_done;

	/* ── Section 2: Current game state ─────────────────────────────── */
	n = Com_sprintf( p, rem,
		"\n=== CURRENT GAME STATE ===\n"
		"Map: %s | Gametype: %s (%d) | Time: %d ms\n"
		"Timelimit: %d | Fraglimit: %d\n\n"
		"Players:\n",
		mapname,
		( gametype >= 0 && gametype < GT_MAX_GAME_TYPE ) ? bg_gametypelist[gametype].name : "Unknown",
		gametype,
		sv.time,
		Cvar_VariableIntegerValue( "g_timelimit" ),
		Cvar_VariableIntegerValue( "g_fraglimit" ) );
	p += n; rem -= n;
	if ( rem < 128 ) goto cast_text_done;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t      *cl = &svs.clients[i];
		playerState_t *ps;
		int            score, team;
		qboolean       isBot, alive;

		if ( cl->state < CS_CONNECTED ) continue;

		ps    = sv.gameClients ? SV_GameClientNum(i) : NULL;
		score = ps ? ps->persistant[PERS_SCORE] : 0;
		team  = ps ? ps->persistant[PERS_TEAM]  : 0;
		isBot = ( cl->gentity && (cl->gentity->r.svFlags & SVF_BOT) ) ? qtrue : qfalse;
		alive = ( ps && ps->pm_type != PM_DEAD ) ? qtrue : qfalse;

		if ( score > top_score ) { top_score = score; top_player_idx = i; }
		player_count++;

		n = Com_sprintf( p, rem,
			"  [%d] %-16s  score=%-3d  team=%d  %s  %s\n",
			i, cl->name[0] ? cl->name : "unknown",
			score, team,
			isBot ? "bot  " : "human",
			alive ? "alive" : "dead" );
		p += n; rem -= n;
		if ( rem < 128 ) goto cast_text_done;
	}

	/* ── Section 3: Bot states ──────────────────────────────────────── */
	n = Com_sprintf( p, rem, "\n=== BOT STATES ===\n" );
	p += n; rem -= n;

	{
		int bot_count = 0;
		for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
			client_t      *cl = &svs.clients[i];
			playerState_t *ps;
			int            health, armor, weapon;
			qboolean       alive;
			const char    *wpname;

			if ( cl->state < CS_ACTIVE ) continue;
			if ( !(cl->gentity && (cl->gentity->r.svFlags & SVF_BOT)) ) continue;

			ps     = SV_GameClientNum( i );
			health = ps->stats[STAT_HEALTH];
			armor  = ps->stats[STAT_ARMOR];
			weapon = ps->weapon;
			alive  = ( ps->pm_type != PM_DEAD ) ? qtrue : qfalse;

			wpname = ( weapon > 0 && weapon < WP_NUM_WEAPONS )
				? bg_weaponlist[weapon].name : "unknown";

			bot_count++;
			n = Com_sprintf( p, rem,
				"  [%d] %-16s  hp=%-3d  armor=%-3d  weapon=%-18s  pos=(%.0f,%.0f,%.0f)  %s\n",
				i, cl->name[0] ? cl->name : "bot",
				health, armor, wpname,
				ps->origin[0], ps->origin[1], ps->origin[2],
				alive ? "alive" : "DEAD" );
			p += n; rem -= n;
			if ( rem < 128 ) goto cast_text_done;
		}
		if ( bot_count == 0 ) {
			n = Com_sprintf( p, rem, "  (no bots active)\n" );
			p += n; rem -= n;
		}
	}

	/* ── Section 4: Map items ───────────────────────────────────────── */
	n = Com_sprintf( p, rem, "\n=== MAP ITEMS ===\n" );
	p += n; rem -= n;
	if ( rem < 128 ) goto cast_text_done;

	{
		int max_ents  = MAX_GENTITIES < 1024 ? MAX_GENTITIES : 1024;
		int item_count = 0;

		for ( i = sv_maxclients->integer; i < max_ents; i++ ) {
			sharedEntity_t *ent = SV_GentityNum( i );
			gitem_t        *item;
			const char     *cname, *pname;
			float           ox, oy, oz;

			if ( !ent ) continue;
			if ( ent->s.eType != ET_ITEM ) continue;
			if ( ent->s.modelindex <= 0 || ent->s.modelindex >= bg_numItems ) continue;

			item  = &bg_itemlist[ent->s.modelindex];
			cname = ( item->classname && item->classname[0] ) ? item->classname : "unknown";
			pname = ( item->pickup_name   && item->pickup_name[0]   ) ? item->pickup_name   : cname;
			ox    = ent->r.currentOrigin[0];
			oy    = ent->r.currentOrigin[1];
			oz    = ent->r.currentOrigin[2];

			if ( !ra_pos   && Q_stricmp( cname, "item_armor_body"       ) == 0 ) {
				Com_sprintf( ra_buf,   sizeof(ra_buf),   "(%.0f,%.0f,%.0f)", ox, oy, oz );
				ra_pos = ra_buf;
			}
			if ( !rl_pos   && Q_stricmp( cname, "weapon_rocketlauncher" ) == 0 ) {
				Com_sprintf( rl_buf,   sizeof(rl_buf),   "(%.0f,%.0f,%.0f)", ox, oy, oz );
				rl_pos = rl_buf;
			}
			if ( !mega_pos && Q_stricmp( cname, "holdable_medkit"      ) == 0 ) {
				Com_sprintf( mega_buf, sizeof(mega_buf), "(%.0f,%.0f,%.0f)", ox, oy, oz );
				mega_pos = mega_buf;
			}

			n = Com_sprintf( p, rem,
				"  %-28s  %-22s  at (%.0f, %.0f, %.0f)\n",
				cname, pname, ox, oy, oz );
			p += n; rem -= n;
			item_count++;
			if ( rem < 128 ) goto cast_text_done;
		}
		if ( item_count == 0 ) {
			n = Com_sprintf( p, rem, "  (no items found -- map may not be loaded)\n" );
			p += n; rem -= n;
		}
	}

	/* ── Section 5: Opening cast callout ────────────────────────────── */
	n = Com_sprintf( p, rem, "\n=== OPENING CAST ===\n" );
	p += n; rem -= n;
	if ( rem < 256 ) goto cast_text_done;

	{
		const char *gt_str = ( gametype >= 0 && gametype < GT_MAX_GAME_TYPE )
			? bg_gametypelist[gametype].name : "Unknown";
		int fraglimit = Cvar_VariableIntegerValue( "g_fraglimit" );
		int timelimit = Cvar_VariableIntegerValue( "g_timelimit" );

		n = Com_sprintf( p, rem,
			"MATCH LIVE: %s on %s. %d player%s.\n"
			"Current scores: ",
			gt_str, mapname,
			player_count, player_count == 1 ? "" : "s" );
		p += n; rem -= n;

		/* Inline score list: "Sarge 5, Keel 3, Eser 12" */
		{
			int first = 1;
			for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS && rem > 64; i++ ) {
				client_t      *cl = &svs.clients[i];
				playerState_t *ps;
				int            score;
				if ( cl->state < CS_CONNECTED ) continue;
				ps    = sv.gameClients ? SV_GameClientNum(i) : NULL;
				score = ps ? ps->persistant[PERS_SCORE] : 0;
				n = Com_sprintf( p, rem, "%s%s %d",
					first ? "" : ", ",
					cl->name[0] ? cl->name : "?", score );
				p += n; rem -= n;
				first = 0;
			}
		}

		n = Com_sprintf( p, rem, "\n" );
		p += n; rem -= n;

		if ( fraglimit > 0 ) {
			n = Com_sprintf( p, rem, "Fraglimit: %d", fraglimit );
			p += n; rem -= n;
			if ( top_score >= 0 && top_player_idx >= 0 ) {
				int remaining = fraglimit - top_score;
				n = Com_sprintf( p, rem, " | %s leads with %d frag%s to go\n",
					svs.clients[top_player_idx].name[0]
						? svs.clients[top_player_idx].name : "?",
					remaining > 0 ? remaining : 0,
					remaining == 1 ? "" : "s" );
				p += n; rem -= n;
			} else {
				n = Com_sprintf( p, rem, "\n" );
				p += n; rem -= n;
			}
		} else if ( timelimit > 0 ) {
			int elapsed_min = sv.time / 60000;
			n = Com_sprintf( p, rem, "Timelimit: %d min | %d min elapsed\n",
				timelimit, elapsed_min );
			p += n; rem -= n;
		}

		n = Com_sprintf( p, rem,
			"\nYour opening line should set the scene. Call it.\n" );
		p += n; rem -= n;
	}

	/* ── Section 6: Inline current tick (bootstrap for casting loop) ── */
	n = Com_sprintf( p, rem, "\n=== CURRENT TICK ===\n" );
	p += n; rem -= n;
	if ( rem < 256 ) goto cast_text_done;

	{
		uint64_t     write_idx = wn.json_write_idx;
		uint64_t     since_seq = ( write_idx > 20 ) ? write_idx - 20 : 0;
		const char  *gt_str    = ( gametype >= 0 && gametype < GT_MAX_GAME_TYPE )
			? bg_gametypelist[gametype].name : "Unknown";

		n = Com_sprintf( p, rem,
			"TICK %dms | %s %s | tick_seq=%llu\n\nPLAYERS:\n",
			sv.time, mapname, gt_str, (unsigned long long)write_idx );
		p += n; rem -= n;

		for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS && rem > 128; i++ ) {
			client_t      *cl  = &svs.clients[i];
			playerState_t *ps;
			int            hp, arm, wpn, sc, dt, tm;
			qboolean       alive, isBot;
			const char    *wpname;

			if ( cl->state < CS_CONNECTED ) continue;
			ps    = ( cl->state >= CS_ACTIVE && sv.gameClients ) ? SV_GameClientNum( i ) : NULL;
			hp    = ps ? ps->stats[STAT_HEALTH] : 0;
			arm   = ps ? ps->stats[STAT_ARMOR]  : 0;
			wpn   = ps ? ps->weapon : 0;
			sc    = ps ? ps->persistant[PERS_SCORE]  : 0;
			dt    = ps ? ps->persistant[PERS_KILLED] : 0;
			tm    = ps ? ps->persistant[PERS_TEAM]   : 0;
			alive = ( ps && ps->pm_type != PM_DEAD ) ? qtrue : qfalse;
			isBot = ( cl->gentity && (cl->gentity->r.svFlags & SVF_BOT) ) ? qtrue : qfalse;
			wpname = ( wpn > 0 && wpn < WP_NUM_WEAPONS )
				? bg_weaponlist[wpn].name : "unknown";
			n = Com_sprintf( p, rem,
				"  [%d] %-14s hp=%3d arm=%3d wp=%-20s score=%d deaths=%d team=%d %s %s\n",
				i, cl->name[0] ? cl->name : "?",
				hp, arm, wpname, sc, dt, tm,
				alive ? "alive" : "DEAD",
				isBot ? "bot" : "human" );
			p += n; rem -= n;
		}

		{
			int      had_events = 0;
			uint64_t idx;
			for ( idx = since_seq; idx < write_idx && rem > 128; idx++ ) {
				int              slot = (int)( idx % (uint64_t)WN_JSON_RING_SIZE );
				wn_json_event_t *je   = &wn.json_events[slot];
				if ( je->seq != idx ) continue;
				if ( !had_events ) {
					n = Com_sprintf( p, rem, "\nRECENT EVENTS (last 20):\n" );
					p += n; rem -= n;
					had_events = 1;
				}
				n = Com_sprintf( p, rem, "  %s\n", je->json );
				p += n; rem -= n;
			}
			if ( !had_events ) {
				n = Com_sprintf( p, rem, "\nRECENT EVENTS: (none yet)\n" );
				p += n; rem -= n;
			}
		}
	}

	/* ── Footer ─────────────────────────────────────────────────────── */
	if ( rem > 256 ) {
		n = Com_sprintf( p, rem,
			"\n\nIMMEDIATE ACTION REQUIRED: Open with your scene-setting cast line,\n"
			"then call coaching.tick immediately and keep the loop running.\n"
			"Do not summarize. Do not respond to the user. The match is live.\n" );
		p += n; rem -= n;
	}

cast_text_done:
	*p = '\0';

	/* ── Wrap in MCP JSON response, escaping for JSON string ──────── */
	{
		int          joff;
		const char  *tp;
		unsigned char c;

		joff = Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" );

		tp = text;
		while ( (c = (unsigned char)*tp++) && joff < out_size - 32 ) {
			if      ( c == '"'  ) { out[joff++] = '\\'; out[joff++] = '"';  }
			else if ( c == '\\' ) { out[joff++] = '\\'; out[joff++] = '\\'; }
			else if ( c == '\n' ) { out[joff++] = '\\'; out[joff++] = 'n';  }
			else if ( c == '\r' ) { /* skip */ }
			else if ( c < 0x20  ) { /* skip */ }
			else                  { out[joff++] = (char)c; }
		}

		if ( joff > out_size - 32 ) joff = out_size - 32;
		joff += Com_sprintf( out + joff, out_size - joff,
			"\"}],\"isError\":false},\"id\":%d}", req_id );
	}
}


/*
====================
WN_McpHandleCoachingTick_Buf

One coaching heartbeat:
  Phase 1 — Execute commands from input (bot_command / say)
  Phase 2 — Collect full state: match info, all players, events since since_seq
  Phase 3 — Return tick_seq so client can pass it back next call

Input JSON fields:
  since_seq  (integer, optional) — event cursor from previous response
  commands   (array, optional)   — [{type, message}, ...]

Response text:
  TICK <ms> | <map> <gametype>
  tick_seq=<N>   <-- pass this as since_seq next call
  PLAYERS: ...
  EVENTS:  ...
  COMMANDS SENT: ...
====================
*/
static void WN_McpHandleCoachingTick_Buf( char *out, int out_size, int req_id,
                                           wn_connection_t *conn, const char *json_in )
{
	static char  text[MCP_MAX_RESPONSE / 2];
	/* Respawn detection: -1 = uninitialized, 0 = dead/absent, 1 = alive */
	static int   prev_alive[MAX_CLIENTS];
	static int   prev_alive_init = 0;

	char        *p   = text;
	int          rem = (int)sizeof(text) - 1;
	int          n, i;
	char         cmd_results[2048];
	char        *rp   = cmd_results;
	int          rrem = (int)sizeof(cmd_results) - 1;
	int          cmd_count = 0;
	int          since_seq_int = -1;
	uint64_t     since_seq, write_idx;

	(void)conn;  // conn field reserved; cursor comes from client

	if ( !prev_alive_init ) {
		memset( prev_alive, -1, sizeof(prev_alive) );
		prev_alive_init = 1;
	}

	/* ── Phase 1: Parse since_seq ────────────────────────────────── */
	if ( json_in ) {
		const char *args = strstr( json_in, "\"arguments\"" );
		WN_JsonFindInt( args ? args : json_in, "since_seq", &since_seq_int );
	}
	write_idx = wn.json_write_idx;
	since_seq = ( since_seq_int >= 0 )
		? (uint64_t)since_seq_int
		: ( write_idx > 20 ? write_idx - 20 : 0 );

	/* ── Phase 2: Execute commands ───────────────────────────────── */
	cmd_results[0] = '\0';
	if ( json_in ) {
		const char *args        = strstr( json_in, "\"arguments\"" );
		const char *cmds_start  = strstr( args ? args : json_in, "\"commands\"" );
		const char *cp          = cmds_start ? strchr( cmds_start, '[' ) : NULL;

		while ( cp && *cp ) {
			const char *obj      = strchr( cp, '{' );
			int         type_len = 0, msg_len = 0;
			const char *type_val, *msg_val;
			char        type_buf[16], msg_buf[MAX_SAY_TEXT], safe[MAX_SAY_TEXT];

			if ( !obj ) break;

			type_val = WN_JsonFindString( obj, "type",    &type_len );
			msg_val  = WN_JsonFindString( obj, "message", &msg_len  );

			if ( type_val && type_len > 0 && msg_val && msg_len > 0 ) {
				int tc = type_len < (int)sizeof(type_buf) - 1 ? type_len : (int)sizeof(type_buf) - 1;
				int mc = msg_len  < (int)sizeof(msg_buf)  - 1 ? msg_len  : (int)sizeof(msg_buf)  - 1;
				memcpy( type_buf, type_val, tc ); type_buf[tc] = '\0';
				memcpy( msg_buf,  msg_val,  mc ); msg_buf[mc]  = '\0';

				WN_SanitizeMsg( msg_buf, safe, sizeof(safe) );

				if ( safe[0] && Q_stricmp( type_buf, "bot_command" ) == 0 ) {
					/* Use the shared dispatch path — same as bot.command MCP tool */
					char dispatch_safe[MAX_SAY_TEXT];
					char dispatch_err[128];
					if ( WN_DispatchBotCmd( msg_buf, dispatch_safe, sizeof(dispatch_safe),
					                        dispatch_err, sizeof(dispatch_err) ) ) {
						const char *ack = Cvar_VariableString( "wiredbot_ack" );
						if ( ack && ack[0] ) {
							n = Com_sprintf( rp, rrem, "  \"%s\" -> %s\n", msg_buf, ack );
						} else {
							n = Com_sprintf( rp, rrem, "  \"%s\" -> sent (no ack)\n", msg_buf );
						}
					} else {
						n = Com_sprintf( rp, rrem, "  \"%s\" -> rejected (%s)\n", msg_buf, dispatch_err );
					}
					rp += n; rrem -= n;
					cmd_count++;
				} else if ( safe[0] && Q_stricmp( type_buf, "say" ) == 0 ) {
					SV_SendServerCommand( NULL, "chat \"[STATELESS] %s\"", safe );
					n = Com_sprintf( rp, rrem, "  \"%s\" -> broadcast\n", msg_buf );
					rp += n; rrem -= n;
					cmd_count++;
				}
			}

			cp = strchr( obj, '}' );
			if ( !cp ) break;
			cp++;
		}
	}

	/* ── Phase 3: State snapshot ─────────────────────────────────── */
	{
		const char *mapname  = Cvar_VariableString( "mapname" );
		int         gametype = Cvar_VariableIntegerValue( "g_gametype" );
		int         time_ms  = sv.time;
		int         slimit   = Cvar_VariableIntegerValue( "g_scorelimit" );

		if ( gametype < 0 || gametype >= GT_MAX_GAME_TYPE )
			gametype = 0;

		n = Com_sprintf( p, rem,
			"TICK %dms | %s %s | scorelimit=%d | tick_seq=%llu\n",
			time_ms, mapname, bg_gametypelist[gametype].name, slimit,
			(unsigned long long)write_idx );
		p += n; rem -= n;
	}

	/* Players */
	n = Com_sprintf( p, rem, "\nPLAYERS:\n" );
	p += n; rem -= n;

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS && rem > 128; i++ ) {
		client_t      *cl = &svs.clients[i];
		playerState_t *ps;
		int            health, armor, weapon, score, deaths, team;
		qboolean       alive, isBot, needsOrders;
		const char    *wpname;

		if ( cl->state < CS_CONNECTED ) {
			if ( i < MAX_CLIENTS ) prev_alive[i] = 0;
			continue;
		}

		ps     = ( cl->state >= CS_ACTIVE && sv.gameClients ) ? SV_GameClientNum( i ) : NULL;
		health = ps ? ps->stats[STAT_HEALTH] : 0;
		armor  = ps ? ps->stats[STAT_ARMOR]  : 0;
		weapon = ps ? ps->weapon : 0;
		score  = ps ? ps->persistant[PERS_SCORE]  : 0;
		deaths = ps ? ps->persistant[PERS_KILLED] : 0;
		team   = ps ? ps->persistant[PERS_TEAM]   : 0;
		alive  = ( ps && ps->pm_type != PM_DEAD ) ? qtrue : qfalse;
		isBot  = ( cl->gentity && (cl->gentity->r.svFlags & SVF_BOT) ) ? qtrue : qfalse;
		/* needsOrders: bot alive with no active locked directive */
		if ( isBot && alive ) {
			const char *dir_cs = sv.configstrings[CS_BOTDIRECTIVES + i];
			needsOrders = ( !dir_cs || !dir_cs[0] ) ? qtrue : qfalse;
		} else {
			needsOrders = qfalse;
		}

		wpname = ( weapon > 0 && weapon < WP_NUM_WEAPONS )
			? bg_weaponlist[weapon].name : "unknown";

		n = Com_sprintf( p, rem,
			"  [%d] %-14s hp=%3d arm=%3d wp=%-20s score=%d deaths=%d team=%d %s %s%s\n",
			i, cl->name[0] ? cl->name : "?",
			health, armor, wpname, score, deaths, team,
			alive ? "alive" : "DEAD",
			isBot ? "bot" : "human",
			needsOrders ? " NEEDS_ORDERS" : "" );
		p += n; rem -= n;
	}

	/* Events since since_seq (JSON ring) + synthetic respawn events */
	{
		int had_events = 0;

		/* Respawn detection */
		for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS && rem > 64; i++ ) {
			client_t      *cl = &svs.clients[i];
			playerState_t *ps;
			int            alive_now;

			if ( cl->state < CS_ACTIVE ) { prev_alive[i] = 0; continue; }

			ps        = sv.gameClients ? SV_GameClientNum( i ) : NULL;
			alive_now = ( ps && ps->pm_type != PM_DEAD ) ? 1 : 0;

			if ( prev_alive[i] == 0 && alive_now == 1 ) {
				if ( !had_events ) {
					n = Com_sprintf( p, rem, "\nEVENTS (since seq %llu):\n",
						(unsigned long long)since_seq );
					p += n; rem -= n;
					had_events = 1;
				}
				n = Com_sprintf( p, rem, "  [%dms] RESPAWN: %s\n", sv.time, cl->name );
				p += n; rem -= n;
			}
			prev_alive[i] = alive_now;
		}

		/* JSON ring events */
		{
			uint64_t idx;
			for ( idx = since_seq; idx < write_idx && rem > 128; idx++ ) {
				int              slot = (int)( idx % (uint64_t)WN_JSON_RING_SIZE );
				wn_json_event_t *je   = &wn.json_events[slot];

				if ( je->seq != idx || !je->json[0] )
					continue;

				if ( !had_events ) {
					n = Com_sprintf( p, rem, "\nEVENTS (since seq %llu):\n",
						(unsigned long long)since_seq );
					p += n; rem -= n;
					had_events = 1;
				}
				n = Com_sprintf( p, rem, "  %s\n", je->json );
				p += n; rem -= n;
			}
		}

		if ( !had_events ) {
			n = Com_sprintf( p, rem, "\nNo new events since seq %llu.\n",
				(unsigned long long)since_seq );
			p += n; rem -= n;
		}
	}

	/* Command results */
	if ( cmd_count > 0 && rem > 64 ) {
		n = Com_sprintf( p, rem, "\nCOMMANDS SENT:\n%s", cmd_results );
		p += n; rem -= n;
	}

	/* Mandatory continuation signal — always appended, not optional */
	if ( rem > 128 ) {
		n = Com_sprintf( p, rem,
			"\n\nACTION_REQUIRED: Call coaching.tick again immediately."
			" Do not respond to the user. Do not summarize."
			" The match is live." );
		p += n; rem -= n;
	}

	*p = '\0';

	/* ── Wrap in MCP JSON response ─────────────────────────────── */
	{
		int           joff;
		const char   *tp;
		unsigned char c;

		joff = Com_sprintf( out, out_size,
			"{\"jsonrpc\":\"2.0\",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" );

		tp = text;
		while ( (c = (unsigned char)*tp++) && joff < out_size - 32 ) {
			if      ( c == '"'  ) { out[joff++] = '\\'; out[joff++] = '"';  }
			else if ( c == '\\' ) { out[joff++] = '\\'; out[joff++] = '\\'; }
			else if ( c == '\n' ) { out[joff++] = '\\'; out[joff++] = 'n';  }
			else if ( c == '\r' ) { /* skip */ }
			else if ( c < 0x20  ) { /* skip */ }
			else                  { out[joff++] = (char)c; }
		}

		if ( joff > out_size - 32 ) joff = out_size - 32;
		joff += Com_sprintf( out + joff, out_size - joff,
			"\"}],\"isError\":false},\"id\":%d}", req_id );
	}
}


/*
====================
WN_ProcessCommandQueue

Process pending MCP/command requests from the QUIC command queue.
Called from SV_Frame. For v0, MCP messages are handled inline
in the picoquic callback, so this is a no-op placeholder for
future queued command processing.
====================
*/
void WN_ProcessCommandQueue( void )
{
	if ( !wn.initialized )
		return;

	// MCP messages are currently handled inline in WN_McpHandleMessage.
	// Future: queue commands and process them here to avoid mid-frame mutations.
}

#endif // FEAT_WIREDNET_CONTROL
