#include "server.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define RCON_LUA_MAX_MEMORY (1024 * 1024)

typedef struct {
	size_t used;
	size_t limit;
} rconLuaMem_t;

typedef struct {
	char *output;
	int outputLen;
	int length;
	qboolean truncated;
} rconOutputCapture_t;

static rconLuaMem_t s_rconLuaMem;
static rconOutputCapture_t *s_capture;
static int s_execStartTime;

static void RconOutput_Append( const char *text ) {
	if ( !s_capture || !s_capture->output || !text ) {
		return;
	}

	for ( int i = 0; text[i] != '\0'; i++ ) {
		if ( s_capture->length >= s_capture->outputLen - 1 ) {
			s_capture->truncated = qtrue;
			break;
		}
		s_capture->output[ s_capture->length++ ] = text[i];
	}

	s_capture->output[ s_capture->length ] = '\0';
}

static const char *RconLua_ToString( lua_State *L, int idx ) {
	const char *s;

	lua_getglobal( L, "tostring" );
	lua_pushvalue( L, idx );
	lua_call( L, 1, 1 );
	s = lua_tostring( L, -1 );
	if ( !s ) {
		s = "";
	}
	return s;
}

static void RconLua_PopToString( lua_State *L ) {
	lua_pop( L, 1 );
}

static int l_print( lua_State *L ) {
	int n = lua_gettop( L );

	for ( int i = 1; i <= n; i++ ) {
		const char *s = RconLua_ToString( L, i );
		if ( i > 1 ) {
			RconOutput_Append( "\t" );
		}
		RconOutput_Append( s );
		RconLua_PopToString( L );
	}

	RconOutput_Append( "\n" );
	return 0;
}

static void Rcon_SanitizeInline( char *dst, int dstSize, const char *src ) {
	int i;

	if ( !dst || dstSize <= 0 ) {
		return;
	}

	dst[0] = '\0';
	if ( !src ) {
		return;
	}

	for ( i = 0; src[i] && i < dstSize - 1; i++ ) {
		char c = src[i];
		if ( c == '"' || c == '\\' || c == '\n' || c == '\r' ) {
			dst[i] = ' ';
		} else {
			dst[i] = c;
		}
	}
	dst[i] = '\0';
}

static int l_server_exec( lua_State *L ) {
	const char *cmd = luaL_checkstring( L, 1 );
	if ( !cmd || !cmd[0] ) {
		return 0;
	}
	if ( !Q_stricmpn( cmd, "rcon", 4 ) || !Q_stricmpn( cmd, "rcon_login", 10 ) ) {
		return luaL_error( L, "server.exec cannot call rcon commands" );
	}
	if ( cmd && cmd[0] ) {
		Cbuf_ExecuteText( EXEC_NOW, cmd );
	}
	return 0;
}

static int l_server_say( lua_State *L ) {
	const char *msg = luaL_checkstring( L, 1 );
	char clean[1024];

	Rcon_SanitizeInline( clean, sizeof( clean ), msg );
	SV_SendServerCommand( NULL, "chat \"console: %s\"", clean );
	return 0;
}

static int l_server_time( lua_State *L ) {
	lua_pushinteger( L, svs.time );
	return 1;
}

static int l_server_map( lua_State *L ) {
	const char *mapName = luaL_checkstring( L, 1 );
	char cmd[MAX_QPATH + 16];
	char cleanMap[MAX_QPATH];

	Rcon_SanitizeInline( cleanMap, sizeof( cleanMap ), mapName );
	if ( !cleanMap[0] ) {
		return luaL_error( L, "invalid map name" );
	}

	Com_sprintf( cmd, sizeof( cmd ), "map %s\n", cleanMap );
	Cbuf_ExecuteText( EXEC_NOW, cmd );
	return 0;
}

static int l_server_maprestart( lua_State *L ) {
	(void)L;
	Cbuf_ExecuteText( EXEC_NOW, "map_restart 0\n" );
	return 0;
}

static int l_server_status( lua_State *L ) {
	int players = 0;

	for ( int i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			players++;
		}
	}

	lua_newtable( L );
	lua_pushstring( L, sv_mapname ? sv_mapname->string : "" );
	lua_setfield( L, -2, "map" );
	lua_pushinteger( L, players );
	lua_setfield( L, -2, "playerCount" );
	lua_pushinteger( L, sv.maxclients );
	lua_setfield( L, -2, "maxClients" );
	lua_pushinteger( L, svs.time );
	lua_setfield( L, -2, "time" );

	return 1;
}

static int l_server_quit( lua_State *L ) {
	(void)L;
	Cbuf_AddText( "quit\n" );
	return 0;
}

static int l_cvar_get( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	lua_pushstring( L, Cvar_VariableString( name ) );
	return 1;
}

static int l_cvar_set( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	const char *value = luaL_checkstring( L, 2 );
	Cvar_Set( name, value );
	return 0;
}

static int l_cvar_reset( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	Cvar_Reset( name );
	return 0;
}

typedef struct {
	lua_State *L;
	const char *pattern;
	int index;
} cvarListCtx_t;

static void RconLua_CvarListCallback( cvar_t *var, void *userdata ) {
	cvarListCtx_t *ctx = (cvarListCtx_t *)userdata;
	char line[MAX_CVAR_VALUE_STRING + MAX_CVAR_VALUE_STRING];

	if ( ctx->pattern && ctx->pattern[0] && !Com_Filter( ctx->pattern, var->name ) ) {
		return;
	}

	Com_sprintf( line, sizeof( line ), "%s = %s", var->name, var->string );
	lua_pushinteger( ctx->L, ++ctx->index );
	lua_pushstring( ctx->L, line );
	lua_settable( ctx->L, -3 );
}

static int l_cvar_list( lua_State *L ) {
	const char *pattern = NULL;
	cvarListCtx_t ctx;

	if ( lua_gettop( L ) >= 1 && lua_isstring( L, 1 ) ) {
		pattern = lua_tostring( L, 1 );
	}

	lua_newtable( L );

	ctx.L = L;
	ctx.pattern = pattern;
	ctx.index = 0;
	Cvar_ForEach( RconLua_CvarListCallback, &ctx );

	return 1;
}

static int l_players_list( lua_State *L ) {
	int n = 0;

	lua_newtable( L );
	for ( int i = 0; i < sv.maxclients; i++ ) {
		client_t *cl = &svs.clients[i];
		const playerState_t *ps;

		if ( cl->state < CS_CONNECTED ) {
			continue;
		}

		ps = SV_GameClientNum( i );

		lua_pushinteger( L, ++n );
		lua_newtable( L );
		lua_pushinteger( L, i ); lua_setfield( L, -2, "id" );
		lua_pushstring( L, cl->name ); lua_setfield( L, -2, "name" );
		lua_pushstring( L, NET_AdrToString( &cl->netchan.remoteAddress ) ); lua_setfield( L, -2, "ip" );
		lua_pushinteger( L, cl->ping ); lua_setfield( L, -2, "ping" );
		lua_pushinteger( L, ps ? ps->persistant[PERS_SCORE] : 0 ); lua_setfield( L, -2, "score" );
		lua_settable( L, -3 );
	}

	return 1;
}

static int l_players_kick( lua_State *L ) {
	int id = (int)luaL_checkinteger( L, 1 );
	const char *reason = lua_gettop( L ) >= 2 ? lua_tostring( L, 2 ) : "was kicked";

	if ( id < 0 || id >= sv.maxclients || svs.clients[id].state < CS_CONNECTED ) {
		return luaL_error( L, "invalid player id" );
	}
	if ( svs.clients[id].netchan.remoteAddress.type <= NA_LOOPBACK ) {
		return luaL_error( L, "cannot kick loopback/bot client" );
	}

	SV_DropClient( &svs.clients[id], reason && reason[0] ? reason : "was kicked" );
	return 0;
}

static int l_players_ban( lua_State *L ) {
	const char *ip = luaL_checkstring( L, 1 );
	int duration = lua_gettop( L ) >= 2 ? (int)luaL_checkinteger( L, 2 ) : 0;
	char cmd[256];
	char cleanIp[128];

	Rcon_SanitizeInline( cleanIp, sizeof( cleanIp ), ip );
	if ( !cleanIp[0] ) {
		return luaL_error( L, "invalid ip" );
	}

	if ( duration > 0 ) {
		Com_sprintf( cmd, sizeof( cmd ), "filtercmd ip \"%s\" date +%d drop\n", cleanIp, duration );
	} else {
		Com_sprintf( cmd, sizeof( cmd ), "filtercmd ip \"%s\" drop\n", cleanIp );
	}

	Cbuf_ExecuteText( EXEC_NOW, cmd );
	return 0;
}

static int l_players_unban( lua_State *L ) {
	const char *ip = luaL_checkstring( L, 1 );
	char cmd[256];
	char cleanIp[128];

	Rcon_SanitizeInline( cleanIp, sizeof( cleanIp ), ip );
	if ( !cleanIp[0] ) {
		return luaL_error( L, "invalid ip" );
	}

	Com_sprintf( cmd, sizeof( cmd ), "bandel \"%s\"\n", cleanIp );
	Cbuf_ExecuteText( EXEC_NOW, cmd );
	return 0;
}

static int l_players_find( lua_State *L ) {
	const char *pattern = luaL_checkstring( L, 1 );
	int n = 0;

	lua_newtable( L );
	for ( int i = 0; i < sv.maxclients; i++ ) {
		client_t *cl = &svs.clients[i];
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		if ( !Com_Filter( pattern, cl->name ) ) {
			continue;
		}

		lua_pushinteger( L, ++n );
		lua_newtable( L );
		lua_pushinteger( L, i ); lua_setfield( L, -2, "id" );
		lua_pushstring( L, cl->name ); lua_setfield( L, -2, "name" );
		lua_settable( L, -3 );
	}

	return 1;
}

static int l_players_tell( lua_State *L ) {
	int id = (int)luaL_checkinteger( L, 1 );
	const char *msg = luaL_checkstring( L, 2 );
	char clean[1024];

	if ( id < 0 || id >= sv.maxclients || svs.clients[id].state < CS_CONNECTED ) {
		return luaL_error( L, "invalid player id" );
	}
	if ( svs.clients[id].netchan.remoteAddress.type <= NA_LOOPBACK ) {
		return luaL_error( L, "cannot send tell to loopback/bot client" );
	}

	Rcon_SanitizeInline( clean, sizeof( clean ), msg );
	SV_SendServerCommand( &svs.clients[id], "chat \"console: %s\"", clean );
	return 0;
}

static int l_game_timelimit( lua_State *L ) {
	int value = (int)luaL_checkinteger( L, 1 );
	Cvar_SetIntegerValue( "g_timelimit", value );
	return 0;
}

static int l_game_scorelimit( lua_State *L ) {
	int value = (int)luaL_checkinteger( L, 1 );
	Cvar_SetIntegerValue( "g_scorelimit", value );
	return 0;
}

static int l_game_gametype( lua_State *L ) {
	int value = (int)luaL_checkinteger( L, 1 );
	Cvar_SetIntegerValue( "g_gametype", value );
	return 0;
}

static int l_game_pause( lua_State *L ) {
	(void)L;
	Cvar_Set( "sv_paused", "1" );
	return 0;
}

static int l_game_unpause( lua_State *L ) {
	(void)L;
	Cvar_Set( "sv_paused", "0" );
	return 0;
}

static void RconLua_RegisterTable( lua_State *L, const char *name, const luaL_Reg *funcs ) {
	lua_newtable( L );
	luaL_register( L, NULL, funcs );
	lua_setglobal( L, name );
}

static void *rcon_lua_alloc( void *ud, void *ptr, size_t osize, size_t nsize ) {
	rconLuaMem_t *mem = (rconLuaMem_t *)ud;

	if ( nsize == 0 ) {
		if ( ptr ) {
			if ( mem->used >= osize ) {
				mem->used -= osize;
			} else {
				mem->used = 0;
			}
			free( ptr );
		}
		return NULL;
	}

	if ( nsize > osize ) {
		size_t grow = nsize - osize;
		if ( mem->used + grow > mem->limit ) {
			return NULL;
		}
		mem->used += grow;
	} else {
		mem->used -= ( osize - nsize );
	}

	return realloc( ptr, nsize );
}

static void rcon_lua_hook( lua_State *L, lua_Debug *ar ) {
	(void)L;
	(void)ar;
	if ( Sys_Milliseconds() - s_execStartTime > 1000 ) {
		luaL_error( L, "rcon script timeout (1 second limit)" );
	}
}

void SV_RconLua_Init( void ) {
	static const luaL_Reg cvarLib[] = {
		{ "get", l_cvar_get },
		{ "set", l_cvar_set },
		{ "reset", l_cvar_reset },
		{ "list", l_cvar_list },
		{ NULL, NULL }
	};
	static const luaL_Reg serverLib[] = {
		{ "exec", l_server_exec },
		{ "say", l_server_say },
		{ "time", l_server_time },
		{ "map", l_server_map },
		{ "maprestart", l_server_maprestart },
		{ "status", l_server_status },
		{ "quit", l_server_quit },
		{ NULL, NULL }
	};
	static const luaL_Reg playersLib[] = {
		{ "list", l_players_list },
		{ "kick", l_players_kick },
		{ "ban", l_players_ban },
		{ "unban", l_players_unban },
		{ "find", l_players_find },
		{ "tell", l_players_tell },
		{ NULL, NULL }
	};
	static const luaL_Reg gameLib[] = {
		{ "scorelimit", l_game_scorelimit },
		{ "timelimit", l_game_timelimit },
		{ "gametype", l_game_gametype },
		{ "pause", l_game_pause },
		{ "unpause", l_game_unpause },
		{ NULL, NULL }
	};
	lua_State *L;

	if ( svs.rconLua.initialized ) {
		SV_RconLua_Shutdown();
	}

	s_rconLuaMem.used = 0;
	s_rconLuaMem.limit = RCON_LUA_MAX_MEMORY;
	L = lua_newstate( rcon_lua_alloc, &s_rconLuaMem );
	if ( !L ) {
		Com_Printf( S_COLOR_RED "WiredRconLua: failed to initialize\n" );
		return;
	}

	luaL_openlibs( L );

	lua_pushnil( L ); lua_setglobal( L, "io" );
	lua_pushnil( L ); lua_setglobal( L, "os" );
	lua_pushnil( L ); lua_setglobal( L, "require" );
	lua_pushnil( L ); lua_setglobal( L, "loadfile" );
	lua_pushnil( L ); lua_setglobal( L, "dofile" );
	lua_pushnil( L ); lua_setglobal( L, "load" );
	lua_pushnil( L ); lua_setglobal( L, "rawget" );
	lua_pushnil( L ); lua_setglobal( L, "rawset" );
	lua_pushnil( L ); lua_setglobal( L, "getmetatable" );
	lua_pushnil( L ); lua_setglobal( L, "setmetatable" );
	lua_pushnil( L ); lua_setglobal( L, "debug" );
	lua_pushnil( L ); lua_setglobal( L, "package" );
	lua_pushnil( L ); lua_setglobal( L, "coroutine" );
	lua_pushnil( L ); lua_setglobal( L, "collectgarbage" );
	lua_pushnil( L ); lua_setglobal( L, "_G" );

	lua_pushcfunction( L, l_print );
	lua_setglobal( L, "print" );

	lua_getglobal( L, "string" );
	lua_getfield( L, -1, "format" );
	lua_setglobal( L, "format" );
	lua_pop( L, 1 );

	RconLua_RegisterTable( L, "cvar", cvarLib );
	RconLua_RegisterTable( L, "server", serverLib );
	RconLua_RegisterTable( L, "players", playersLib );
	RconLua_RegisterTable( L, "game", gameLib );

	svs.rconLua.L = L;
	svs.rconLua.initialized = qtrue;
	memset( &svs.rconLua.currentClient, 0, sizeof( svs.rconLua.currentClient ) );
}

void SV_RconLua_Shutdown( void ) {
	if ( svs.rconLua.L ) {
		lua_close( svs.rconLua.L );
	}
	memset( &svs.rconLua, 0, sizeof( svs.rconLua ) );
	s_capture = NULL;
	s_rconLuaMem.used = 0;
}

qboolean SV_RconLua_Execute( const char *code, char *output, int outputLen ) {
	lua_State *L = svs.rconLua.L;
	rconOutputCapture_t capture;
	char expr[1024];
	int status;

	if ( !L || !code || !output || outputLen <= 0 ) {
		return qfalse;
	}

	output[0] = '\0';
	capture.output = output;
	capture.outputLen = outputLen;
	capture.length = 0;
	capture.truncated = qfalse;
	s_capture = &capture;

	s_execStartTime = Sys_Milliseconds();
	lua_sethook( L, rcon_lua_hook, LUA_MASKCOUNT, 10000 );

	Com_sprintf( expr, sizeof( expr ), "return %s", code );
	status = luaL_loadstring( L, expr );
	if ( status != 0 ) {
		lua_pop( L, 1 );
		status = luaL_loadstring( L, code );
	}

	if ( status == 0 ) {
		status = lua_pcall( L, 0, LUA_MULTRET, 0 );
	}

	lua_sethook( L, NULL, 0, 0 );

	if ( status != 0 ) {
		const char *err = lua_tostring( L, -1 );
		if ( !err ) {
			err = "unknown lua error";
		}
		Com_sprintf( output, outputLen, "error: %s", err );
		lua_pop( L, 1 );
		s_capture = NULL;
		return qfalse;
	}

	if ( capture.truncated ) {
		RconOutput_Append( "... (output truncated)\n" );
	}

	lua_settop( L, 0 );
	s_capture = NULL;
	return qtrue;
}
