/*
===========================================================================
cl_wired_scripting.c -- Wired Scripting: LuaJIT integration

Phase 5: LuaJIT console, cvar metatable bridge, store Lua API, sandbox.
No Lua in the render path -- Lua runs only on:
  - Console input (user types Lua expression)
  - File execution (dofile / exec autoexec.lua)
===========================================================================
*/

#include "../client.h"
#include "cl_wired_scripting.h"
#include "cl_wired_store.h"

#if FEAT_LUA

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lua_State *wired_lua = NULL;

/* ---- helpers --------------------------------------------------------- */

/* LuaJIT 2.1 (Lua 5.1 API) has no luaL_tolstring.
   Push a human-readable string for the value at idx. */
static const char *WiredScript_ToString( lua_State *L, int idx ) {
	if ( luaL_callmeta( L, idx, "__tostring" ) ) {
		if ( !lua_isstring( L, -1 ) ) {
			luaL_error( L, "'__tostring' must return a string" );
		}
		return lua_tostring( L, -1 );
	}
	switch ( lua_type( L, idx ) ) {
		case LUA_TNUMBER:
		case LUA_TSTRING:
			lua_pushvalue( L, idx );
			return lua_tostring( L, -1 );
		case LUA_TBOOLEAN:
			lua_pushstring( L, lua_toboolean( L, idx ) ? "true" : "false" );
			return lua_tostring( L, -1 );
		case LUA_TNIL:
			lua_pushliteral( L, "nil" );
			return lua_tostring( L, -1 );
		default:
			lua_pushfstring( L, "%s: %p", luaL_typename( L, idx ), lua_topointer( L, idx ) );
			return lua_tostring( L, -1 );
	}
}

/* ---- Store API -------------------------------------------------------- */

/* store.get(key) -> string or nil */
static int WiredScript_StoreGet( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e && e->text[0] ) {
		lua_pushstring( L, e->text );
	} else {
		lua_pushnil( L );
	}
	return 1;
}

/* store.set(key, value) -- writes text + value to store */
static int WiredScript_StoreSet( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Set( key );
	if ( !e ) {
		return luaL_error( L, "store full, cannot set '%s'", key );
	}
	if ( lua_type( L, 2 ) == LUA_TNUMBER ) {
		float val = (float)lua_tonumber( L, 2 );
		char buf[64];
		e->value = val;
		Com_sprintf( buf, sizeof( buf ), "%g", (double)val );
		Q_strncpyz( e->text, buf, sizeof( e->text ) );
	} else {
		const char *val = luaL_checkstring( L, 2 );
		Q_strncpyz( e->text, val, sizeof( e->text ) );
	}
	e->flags |= WUI_STORE_FLAG_DIRTY;
	return 0;
}

/* store.getvalue(key) -> number or nil */
static int WiredScript_StoreGetValue( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e ) {
		lua_pushnumber( L, e->value );
	} else {
		lua_pushnil( L );
	}
	return 1;
}

/* store.getcolor(key) -> r, g, b, a or nil */
static int WiredScript_StoreGetColor( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e ) {
		lua_pushnumber( L, e->color[0] );
		lua_pushnumber( L, e->color[1] );
		lua_pushnumber( L, e->color[2] );
		lua_pushnumber( L, e->color[3] );
		return 4;
	}
	lua_pushnil( L );
	return 1;
}

/* store.pairs(prefix) -- deferred until store iteration API exists */
static int WiredScript_StorePairs( lua_State *L ) {
	return luaL_error( L, "store.pairs() not yet implemented" );
}

static const luaL_Reg storeLib[] = {
	{ "get",      WiredScript_StoreGet },
	{ "set",      WiredScript_StoreSet },
	{ "getvalue", WiredScript_StoreGetValue },
	{ "getcolor", WiredScript_StoreGetColor },
	{ "pairs",    WiredScript_StorePairs },
	{ NULL, NULL }
};

/* ---- Cvar metatable bridge ------------------------------------------- */
/* Global table with __index/__newindex metamethods:
   sensitivity = 3.5   -> Cvar_Set("sensitivity", "3.5")
   print(sensitivity)   -> Cvar_Get("sensitivity") */

static int WiredScript_CvarIndex( lua_State *L ) {
	const char *name;
	char buf[1024];

	/* First check if it's a real global (function, table, etc.) */
	lua_rawget( L, 1 );
	if ( !lua_isnil( L, -1 ) ) {
		return 1; /* found a real global, return it */
	}
	lua_pop( L, 1 );

	/* Not a real global -- try as cvar */
	name = lua_tostring( L, 2 );
	if ( !name ) {
		lua_pushnil( L );
		return 1;
	}

	Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	if ( buf[0] == '\0' ) {
		/* Not a cvar either -- return nil */
		lua_pushnil( L );
		return 1;
	}

	/* Try to return as number if possible */
	{
		char *endptr;
		double val = strtod( buf, &endptr );
		if ( endptr != buf && *endptr == '\0' ) {
			lua_pushnumber( L, val );
		} else {
			lua_pushstring( L, buf );
		}
	}
	return 1;
}

static int WiredScript_CvarNewIndex( lua_State *L ) {
	const char *name = lua_tostring( L, 2 );
	if ( !name ) return 0;

	/* Check if setting a real global (function, table) -- allow it */
	if ( lua_type( L, 3 ) == LUA_TFUNCTION || lua_type( L, 3 ) == LUA_TTABLE ) {
		lua_rawset( L, 1 );
		return 0;
	}

	/* Set cvar */
	if ( lua_type( L, 3 ) == LUA_TNUMBER ) {
		char buf[64];
		Com_sprintf( buf, sizeof( buf ), "%g", lua_tonumber( L, 3 ) );
		Cvar_Set( name, buf );
	} else if ( lua_type( L, 3 ) == LUA_TSTRING ) {
		Cvar_Set( name, lua_tostring( L, 3 ) );
	} else if ( lua_type( L, 3 ) == LUA_TBOOLEAN ) {
		Cvar_Set( name, lua_toboolean( L, 3 ) ? "1" : "0" );
	}
	return 0;
}

/* ---- Sandbox print -> Com_Printf ------------------------------------- */

static int WiredScript_Print( lua_State *L ) {
	int n = lua_gettop( L );
	int i;
	for ( i = 1; i <= n; i++ ) {
		const char *s = WiredScript_ToString( L, i );
		if ( i > 1 ) Com_Printf( "\t" );
		Com_Printf( "%s", s ? s : "nil" );
		lua_pop( L, 1 ); /* pop the string from WiredScript_ToString */
	}
	Com_Printf( "\n" );
	return 0;
}

/* ---- Engine command execution from Lua -------------------------------- */

static int WiredScript_Cmd( lua_State *L ) {
	const char *cmd = luaL_checkstring( L, 1 );
	Cbuf_ExecuteText( EXEC_APPEND, va( "%s\n", cmd ) );
	return 0;
}

/* ---- Console commands ------------------------------------------------- */

static void WiredScript_Cmd_Exec( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: lua_exec <filename.lua>\n" );
		return;
	}
	WiredScript_ExecFile( Cmd_Argv( 1 ) );
}

static void WiredScript_Cmd_Eval( void ) {
	char text[2048];
	int i;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: lua_eval <expression>\n" );
		return;
	}

	/* concatenate all args into a single string */
	text[0] = '\0';
	for ( i = 1; i < Cmd_Argc(); i++ ) {
		if ( i > 1 ) Q_strcat( text, sizeof( text ), " " );
		Q_strcat( text, sizeof( text ), Cmd_Argv( i ) );
	}

	WiredScript_TryEval( text );
}

/* ---- Initialization --------------------------------------------------- */

void WiredScript_Init( void ) {
	lua_State *L;

	if ( wired_lua ) {
		WiredScript_Shutdown();
	}

	L = luaL_newstate();
	if ( !L ) {
		Com_Printf( S_COLOR_RED "WiredScript: failed to create Lua state\n" );
		return;
	}

	/* Open SAFE standard libraries only -- remove os, io, debug after */
	luaL_openlibs( L );

	/* Remove unsafe modules */
	lua_pushnil( L ); lua_setglobal( L, "os" );
	lua_pushnil( L ); lua_setglobal( L, "io" );
	lua_pushnil( L ); lua_setglobal( L, "debug" );
	lua_pushnil( L ); lua_setglobal( L, "loadfile" );
	lua_pushnil( L ); lua_setglobal( L, "dofile" );

	/* Register store module */
	luaL_newlib( L, storeLib );
	lua_setglobal( L, "store" );

	/* Override print */
	lua_pushcfunction( L, WiredScript_Print );
	lua_setglobal( L, "print" );

	/* Add cmd() function for engine commands */
	lua_pushcfunction( L, WiredScript_Cmd );
	lua_setglobal( L, "cmd" );

	/* Set up cvar metatable on _G (Lua 5.1 API: use LUA_GLOBALSINDEX) */
	lua_pushvalue( L, LUA_GLOBALSINDEX );  /* push _G */
	lua_newtable( L );                     /* push metatable */
	lua_pushcfunction( L, WiredScript_CvarIndex );
	lua_setfield( L, -2, "__index" );
	lua_pushcfunction( L, WiredScript_CvarNewIndex );
	lua_setfield( L, -2, "__newindex" );
	lua_setmetatable( L, -2 );             /* setmetatable(_G, mt) */
	lua_pop( L, 1 );                       /* pop _G */

	wired_lua = L;

	/* Register console commands */
	Cmd_AddCommand( "lua_exec", WiredScript_Cmd_Exec );
	Cmd_AddCommand( "lua_eval", WiredScript_Cmd_Eval );

	Com_Printf( "WiredScript: LuaJIT initialized (sandbox active)\n" );

	/* Try to execute autoexec.lua */
	WiredScript_ExecFile( "autoexec.lua" );
}

void WiredScript_Shutdown( void ) {
	if ( wired_lua ) {
		Cmd_RemoveCommand( "lua_exec" );
		Cmd_RemoveCommand( "lua_eval" );
		lua_close( wired_lua );
		wired_lua = NULL;
		Com_Printf( "WiredScript: shutdown\n" );
	}
}

/* ---- Console eval ----------------------------------------------------- */

qboolean WiredScript_TryEval( const char *text ) {
	int status;

	if ( !wired_lua || !text || !text[0] ) {
		return qfalse;
	}

	/* Skip lines that look like traditional commands (start with / or \) */
	if ( text[0] == '/' || text[0] == '\\' ) {
		return qfalse;
	}

	/* Try as expression first (prepend "return ") for REPL convenience */
	{
		char expr[2048];
		Com_sprintf( expr, sizeof( expr ), "return %s", text );
		status = luaL_loadstring( wired_lua, expr );
		if ( status == 0 ) {
			status = lua_pcall( wired_lua, 0, LUA_MULTRET, 0 );
			if ( status == 0 ) {
				/* Print results */
				int nresults = lua_gettop( wired_lua );
				if ( nresults > 0 ) {
					int i;
					for ( i = 1; i <= nresults; i++ ) {
						const char *s = WiredScript_ToString( wired_lua, i );
						if ( i > 1 ) Com_Printf( "\t" );
						Com_Printf( "%s", s ? s : "nil" );
						lua_pop( wired_lua, 1 ); /* pop the string */
					}
					Com_Printf( "\n" );
				}
				lua_settop( wired_lua, 0 );
				return qtrue;
			}
			lua_pop( wired_lua, 1 ); /* pop error */
		} else {
			lua_pop( wired_lua, 1 ); /* pop error from loadstring */
		}
	}

	/* Try as statement */
	status = luaL_loadstring( wired_lua, text );
	if ( status != 0 ) {
		/* Not valid Lua -- fall through to old command system */
		lua_pop( wired_lua, 1 );
		return qfalse;
	}

	status = lua_pcall( wired_lua, 0, 0, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( wired_lua, -1 );
		Com_Printf( S_COLOR_RED "Lua error: %s\n", err ? err : "unknown" );
		lua_pop( wired_lua, 1 );
	}
	lua_settop( wired_lua, 0 );
	return qtrue;
}

/* ---- File execution --------------------------------------------------- */

void WiredScript_ExecFile( const char *filename ) {
	fileHandle_t f;
	int len;
	char *buf;
	int status;

	if ( !wired_lua ) return;

	/* Only allow .lua files */
	{
		const char *ext = strrchr( filename, '.' );
		if ( !ext || Q_stricmp( ext, ".lua" ) != 0 ) {
			Com_Printf( S_COLOR_YELLOW "WiredScript: only .lua files supported\n" );
			return;
		}
	}

	len = FS_FOpenFileRead( filename, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		/* File not found -- silent for autoexec.lua, warn for explicit exec */
		if ( Q_stricmp( filename, "autoexec.lua" ) != 0 ) {
			Com_Printf( S_COLOR_YELLOW "WiredScript: file not found '%s'\n", filename );
		}
		return;
	}

	buf = Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

	/* Load and execute */
	{
		char chunkName[256];
		Com_sprintf( chunkName, sizeof( chunkName ), "@%s", filename );
		status = luaL_loadbuffer( wired_lua, buf, len, chunkName );
	}
	Z_Free( buf );

	if ( status != 0 ) {
		const char *err = lua_tostring( wired_lua, -1 );
		Com_Printf( S_COLOR_RED "Lua load error (%s): %s\n", filename, err ? err : "unknown" );
		lua_pop( wired_lua, 1 );
		return;
	}

	status = lua_pcall( wired_lua, 0, 0, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( wired_lua, -1 );
		Com_Printf( S_COLOR_RED "Lua exec error (%s): %s\n", filename, err ? err : "unknown" );
		lua_pop( wired_lua, 1 );
		return;
	}

	Com_Printf( "WiredScript: executed '%s'\n", filename );
}

#endif /* FEAT_LUA */
