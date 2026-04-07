/*
===========================================================================
wired_scripting.c -- Headless LuaJIT runtime for q3now

Core Lua scripting: VM lifecycle, sandbox, cvar metatable bridge,
print -> Com_Printf, cmd() -> Cbuf_ExecuteText, file execution.

This is the engine-level runtime shared by client and dedicated server.
Subsystems register additional Lua bindings via
WiredScript_RegisterBindings(); they are invoked in WiredScript_PostInit().
===========================================================================
*/

#include "../q_shared.h"
#include "../qcommon.h"
#include "wired_scripting.h"

#if FEAT_LUA

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lua_State *wired_lua = NULL;

#define MAX_BINDING_REGISTRARS 8
static WiredScript_BindingFn bindingRegistrars[MAX_BINDING_REGISTRARS];
static int numBindingRegistrars = 0;

/* ---- helpers --------------------------------------------------------- */

/* LuaJIT 2.1 (Lua 5.1 API) has no luaL_tolstring. */
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

/* ---- Cvar metatable bridge ------------------------------------------- */
/* _G metatable: variable reads -> Cvar_Get, writes -> Cvar_Set */

static int WiredScript_CvarIndex( lua_State *L ) {
	const char *name;
	char buf[1024];

	lua_pushvalue( L, 2 );
	lua_rawget( L, 1 );
	if ( !lua_isnil( L, -1 ) ) {
		return 1;
	}
	lua_pop( L, 1 );

	name = lua_tostring( L, 2 );
	if ( !name ) {
		lua_pushnil( L );
		return 1;
	}

	Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	if ( buf[0] == '\0' ) {
		lua_pushnil( L );
		return 1;
	}

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

	if ( lua_type( L, 3 ) == LUA_TFUNCTION || lua_type( L, 3 ) == LUA_TTABLE ) {
		lua_rawset( L, 1 );
		return 0;
	}

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

/* ---- print -> Com_Printf --------------------------------------------- */

static int WiredScript_Print( lua_State *L ) {
	int n = lua_gettop( L );
	int i;
	for ( i = 1; i <= n; i++ ) {
		const char *s = WiredScript_ToString( L, i );
		if ( i > 1 ) Com_Printf( "\t" );
		Com_Printf( "%s", s ? s : "nil" );
		lua_pop( L, 1 );
	}
	Com_Printf( "\n" );
	return 0;
}

/* ---- cmd() -> Cbuf_ExecuteText --------------------------------------- */

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

	text[0] = '\0';
	for ( i = 1; i < Cmd_Argc(); i++ ) {
		if ( i > 1 ) Q_strcat( text, sizeof( text ), " " );
		Q_strcat( text, sizeof( text ), Cmd_Argv( i ) );
	}

	WiredScript_TryEval( text );
}

/* ---- Lifecycle -------------------------------------------------------- */

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

	luaL_openlibs( L );

	/* Sandbox: remove unsafe modules */
	lua_pushnil( L ); lua_setglobal( L, "os" );
	lua_pushnil( L ); lua_setglobal( L, "io" );
	lua_pushnil( L ); lua_setglobal( L, "debug" );
	lua_pushnil( L ); lua_setglobal( L, "loadfile" );
	lua_pushnil( L ); lua_setglobal( L, "dofile" );

	lua_pushcfunction( L, WiredScript_Print );
	lua_setglobal( L, "print" );

	lua_pushcfunction( L, WiredScript_Cmd );
	lua_setglobal( L, "cmd" );

	/* Cvar metatable on _G (Lua 5.1 API: LUA_GLOBALSINDEX) */
	lua_pushvalue( L, LUA_GLOBALSINDEX );
	lua_newtable( L );
	lua_pushcfunction( L, WiredScript_CvarIndex );
	lua_setfield( L, -2, "__index" );
	lua_pushcfunction( L, WiredScript_CvarNewIndex );
	lua_setfield( L, -2, "__newindex" );
	lua_setmetatable( L, -2 );
	lua_pop( L, 1 );

	wired_lua = L;

	Cmd_AddCommand( "lua_exec", WiredScript_Cmd_Exec );
	Cmd_AddCommand( "lua_eval", WiredScript_Cmd_Eval );

	Com_Printf( "WiredScript: LuaJIT initialized (sandbox active)\n" );

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
				int nresults = lua_gettop( wired_lua );
				if ( nresults > 0 ) {
					int i;
					for ( i = 1; i <= nresults; i++ ) {
						const char *s = WiredScript_ToString( wired_lua, i );
						if ( i > 1 ) Com_Printf( "\t" );
						Com_Printf( "%s", s ? s : "nil" );
						lua_pop( wired_lua, 1 );
					}
					Com_Printf( "\n" );
				}
				lua_settop( wired_lua, 0 );
				return qtrue;
			}
			lua_pop( wired_lua, 1 );
		} else {
			lua_pop( wired_lua, 1 );
		}
	}

	/* Try as statement */
	status = luaL_loadstring( wired_lua, text );
	if ( status != 0 ) {
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

	{
		const char *ext = strrchr( filename, '.' );
		if ( !ext || Q_stricmp( ext, ".lua" ) != 0 ) {
			Com_Printf( S_COLOR_YELLOW "WiredScript: only .lua files supported\n" );
			return;
		}
	}

	len = FS_FOpenFileRead( filename, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		if ( Q_stricmp( filename, "autoexec.lua" ) != 0 ) {
			Com_Printf( S_COLOR_YELLOW "WiredScript: file not found '%s'\n", filename );
		}
		return;
	}

	buf = Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

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

/* ---- Binding registration --------------------------------------------- */

void WiredScript_RegisterBindings( WiredScript_BindingFn fn ) {
	if ( numBindingRegistrars >= MAX_BINDING_REGISTRARS ) {
		Com_Printf( S_COLOR_RED "WiredScript: binding registrar table full\n" );
		return;
	}
	bindingRegistrars[numBindingRegistrars++] = fn;
}

void WiredScript_PostInit( void ) {
	int i;

	if ( !wired_lua ) {
		return;
	}

	for ( i = 0; i < numBindingRegistrars; i++ ) {
		bindingRegistrars[i]( wired_lua );
	}
	if ( numBindingRegistrars ) {
		Com_Printf( "WiredScript: %d binding registrar(s) applied\n", numBindingRegistrars );
	}
}

/* ---- Extension point -------------------------------------------------- */

lua_State *WiredScript_GetState( void ) {
	return wired_lua;
}

#endif /* FEAT_LUA */
