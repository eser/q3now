/*
===========================================================================
wired_scripting.c -- Headless LuaJIT runtime for the wired engine

Core Lua scripting: VM lifecycle, sandbox, cvar metatable bridge,
print -> Com_Log, cmd() -> Cbuf_ExecuteText, file execution.

This is the engine-level runtime shared by client and dedicated server.
Subsystems register additional Lua bindings via
WiredScript_RegisterBindings(); they are invoked in WiredScript_PostInit().
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "arena.h"
#include "wired_scripting.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_scripting, "scripting" );

/* ---- Persistent arena ------------------------------------------------ */
/* Engine-side control block for the LuaJIT runtime.  Lives in a persistent
   arena so it shows up in /meminfo and outlives Hunk_ClearLevel(). */

#define MAX_BINDING_REGISTRARS 8
#define WIREDSCRIPT_ARENA_SIZE (16 * 1024)  /* 16 KB — header + registrar table */

typedef struct {
    lua_State            *lua;
    WiredScript_BindingFn registrars[MAX_BINDING_REGISTRARS];
    int                   numRegistrars;
} WiredScriptState_t;

static arena_t            *s_wsArena = NULL;
static WiredScriptState_t *s_ws      = NULL;

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

	// Only write to cvars that are already registered in the engine.
	// Scripts assigning to an unknown global get a plain Lua rawset instead,
	// preventing accidental side-effects on engine cvars with matching names.
	if ( !Cvar_FindVarPublic( name ) ) {
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

/* ---- print -> Com_Log ------------------------------------------------ */

static int WiredScript_Print( lua_State *L ) {
	int n = lua_gettop( L );
	int i;
	for ( i = 1; i <= n; i++ ) {
		const char *s = WiredScript_ToString( L, i );
		if ( i > 1 ) Com_Log( SEV_INFO, LOG_CH(ch_scripting), "\t" );
		Com_Log( SEV_INFO, LOG_CH(ch_scripting), "%s", s ? s : "nil" );
		lua_pop( L, 1 );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_scripting), "\n" );
	return 0;
}

/* ---- cmd() -> Cbuf_ExecuteText --------------------------------------- */

static int WiredScript_Cmd( lua_State *L ) {
	const char *cmd = luaL_checkstring( L, 1 );
	Cbuf_ExecuteText( EXEC_APPEND, va( "%s\n", cmd ) );
	return 0;
}

/* ---- lightstyle(slot, pattern) -> bool ------------------------------- */
/* Typed, validated wrapper over the "lightstyle" console command.
   slot:    integer [0, 63]  (mirrors CS_MAX_LIGHTSTYLES in bg_public.h)
   pattern: string, lowercase a-z only, or "" to turn a slot off
             max length 64   (mirrors LIGHTSTYLE_PATTERN_MAX in bg_public.h)
   Returns true on success, false if slot or pattern is invalid.
   Dispatches EXEC_NOW so the configstring is written before returning.

   Example:
     lightstyle(5, "abcdefghijklmnopqrstuvwxyz")  -- smooth pulse
     lightstyle(6, "z")                           -- full brightness
     lightstyle(7, "")                            -- off
*/
static int WiredScript_Lightstyle( lua_State *L ) {
	int         slot;
	const char *pattern;
	int         i, len;

	slot    = (int)luaL_checkinteger( L, 1 );
	pattern = luaL_checkstring( L, 2 );

	/* Validate slot */
	if ( slot < 0 || slot >= 64 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}

	/* Validate pattern before embedding in cbuf string — prevents tokenizer
	   corruption and mirrors G_ValidateLightstylePattern rules exactly. */
	len = (int)strlen( pattern );
	if ( len >= 64 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	for ( i = 0; i < len; i++ ) {
		if ( pattern[i] < 'a' || pattern[i] > 'z' ) {
			lua_pushboolean( L, 0 );
			return 1;
		}
	}

	/* Always quote pattern so tokenizer sees argv[2] regardless of length.
	   G_SetLightstyle re-validates server-side; this is defense-in-depth. */
	Cbuf_ExecuteText( EXEC_NOW, va( "lightstyle %d \"%s\"\n", slot, pattern ) );

	lua_pushboolean( L, 1 );
	return 1;
}

/* ---- Console commands ------------------------------------------------- */

static void WiredScript_Cmd_Exec( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_scripting), "usage: lua_exec <filename.lua>\n" );
		return;
	}
	WiredScript_ExecFile( Cmd_Argv( 1 ) );
}

static void WiredScript_Cmd_Eval( void ) {
	QS_LOCAL( text, 2048 );
	int i;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_scripting), "usage: lua_eval <expression>\n" );
		return;
	}

	for ( i = 1; i < Cmd_Argc(); i++ ) {
		if ( i > 1 ) QS_AppendChar( &text, ' ' );
		QS_Append( &text, Cmd_Argv( i ) );
	}

	WiredScript_TryEval( QS_CStr( &text ) );
}

/* ---- Lifecycle -------------------------------------------------------- */

void WiredScript_Init( void ) {
	lua_State *L;

	/* Create the persistent arena once at engine startup.  On a second call
	   (unexpected but safe), the arena already exists — just re-initialize
	   the state struct fields inside it. */
	if ( !s_wsArena ) {
		s_wsArena = Arena_Create( "WiredScript", WIREDSCRIPT_ARENA_SIZE );
		s_ws = Arena_AllocType( s_wsArena, WiredScriptState_t );
		memset( s_ws, 0, sizeof( WiredScriptState_t ) );
	}

	if ( s_ws->lua ) {
		WiredScript_Shutdown();
	}

	/* Reset registrar count so PostInit is idempotent on re-init */
	s_ws->numRegistrars = 0;

	L = luaL_newstate();
	if ( !L ) {
		COM_ERROR( LOG_CH(ch_scripting), "WiredCore/Scripting: failed to create Lua state\n" );
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

	lua_pushcfunction( L, WiredScript_Lightstyle );
	lua_setglobal( L, "lightstyle" );

	/* Cvar metatable on _G (Lua 5.1 API: LUA_GLOBALSINDEX) */
	lua_pushvalue( L, LUA_GLOBALSINDEX );
	lua_newtable( L );
	lua_pushcfunction( L, WiredScript_CvarIndex );
	lua_setfield( L, -2, "__index" );
	lua_pushcfunction( L, WiredScript_CvarNewIndex );
	lua_setfield( L, -2, "__newindex" );
	lua_setmetatable( L, -2 );
	lua_pop( L, 1 );

	s_ws->lua = L;

	Cmd_AddCommand( "lua_exec", WiredScript_Cmd_Exec );
	Cmd_AddCommand( "lua_eval", WiredScript_Cmd_Eval );

	Com_Log( SEV_INFO, LOG_CH(ch_scripting), "WiredCore/Scripting: LuaJIT initialized (sandbox active)\n" );

	/* autoexec.lua runs at VM creation time, before WiredScript_PostInit.
	   Only baseline globals are available: print, cmd, cvar bridge.
	   Subsystem bindings (store.*, attract.*, load_menu) are NOT available here —
	   they are registered during CL_Init and applied at PostInit. */
	WiredScript_ExecFile( "autoexec.lua" );
}

void WiredScript_Shutdown( void ) {
	if ( s_ws && s_ws->lua ) {
		Cmd_RemoveCommand( "lua_exec" );
		Cmd_RemoveCommand( "lua_eval" );
		lua_close( s_ws->lua );
		s_ws->lua = NULL;
		Com_Log( SEV_INFO, LOG_CH(ch_scripting), "WiredCore/Scripting: shutdown\n" );
	}
}

/* ---- Console eval ----------------------------------------------------- */

qboolean WiredScript_TryEval( const char *text ) {
	int status;

	if ( !s_ws || !s_ws->lua || !text || !text[0] ) {
		return qfalse;
	}

	if ( text[0] == '/' || text[0] == '\\' ) {
		return qfalse;
	}

	/* Try as expression first (prepend "return ") for REPL convenience */
	{
		char expr[2048];
		Com_sprintf( expr, sizeof( expr ), "return %s", text );
		status = luaL_loadstring( s_ws->lua, expr );
		if ( status == 0 ) {
			status = lua_pcall( s_ws->lua, 0, LUA_MULTRET, 0 );
			if ( status == 0 ) {
				int nresults = lua_gettop( s_ws->lua );
				if ( nresults > 0 ) {
					int i;
					for ( i = 1; i <= nresults; i++ ) {
						const char *s = WiredScript_ToString( s_ws->lua, i );
						if ( i > 1 ) Com_Log( SEV_INFO, LOG_CH(ch_scripting), "\t" );
						Com_Log( SEV_INFO, LOG_CH(ch_scripting), "%s", s ? s : "nil" );
						lua_pop( s_ws->lua, 1 );
					}
					Com_Log( SEV_INFO, LOG_CH(ch_scripting), "\n" );
				}
				lua_settop( s_ws->lua, 0 );
				return qtrue;
			}
			lua_pop( s_ws->lua, 1 );
		} else {
			lua_pop( s_ws->lua, 1 );
		}
	}

	/* Try as statement */
	status = luaL_loadstring( s_ws->lua, text );
	if ( status != 0 ) {
		lua_pop( s_ws->lua, 1 );
		return qfalse;
	}

	status = lua_pcall( s_ws->lua, 0, 0, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		COM_ERROR( LOG_CH(ch_scripting), "Lua error: %s\n", err ? err : "unknown" );
		lua_pop( s_ws->lua, 1 );
	}
	lua_settop( s_ws->lua, 0 );
	return qtrue;
}

/* ---- File execution --------------------------------------------------- */

void WiredScript_ExecFile( const char *filename ) {
	fileHandle_t f;
	int len;
	char *buf;
	int status;

	if ( !s_ws || !s_ws->lua ) return;

	{
		const char *ext = strrchr( filename, '.' );
		if ( !ext || Q_stricmp( ext, ".lua" ) != 0 ) {
			COM_WARN( LOG_CH(ch_scripting), "WiredCore/Scripting: only .lua files supported\n" );
			return;
		}
	}

	len = FS_FOpenFileRead( filename, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		if ( Q_stricmp( filename, "autoexec.lua" ) != 0 ) {
			COM_WARN( LOG_CH(ch_scripting), "WiredCore/Scripting: file not found '%s'\n", filename );
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
		status = luaL_loadbuffer( s_ws->lua, buf, len, chunkName );
	}
	Z_Free( buf );

	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		COM_ERROR( LOG_CH(ch_scripting), "Lua load error (%s): %s\n", filename, err ? err : "unknown" );
		lua_pop( s_ws->lua, 1 );
		return;
	}

	status = lua_pcall( s_ws->lua, 0, 0, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		COM_ERROR( LOG_CH(ch_scripting), "Lua exec error (%s): %s\n", filename, err ? err : "unknown" );
		lua_pop( s_ws->lua, 1 );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_scripting), "WiredCore/Scripting: executed '%s'\n", filename );
}

/* ---- Binding registration --------------------------------------------- */

void WiredScript_RegisterBindings( WiredScript_BindingFn fn ) {
	if ( !s_ws ) {
		COM_ERROR( LOG_CH(ch_scripting), "WiredCore/Scripting: RegisterBindings called before Init\n" );
		return;
	}
	if ( s_ws->numRegistrars >= MAX_BINDING_REGISTRARS ) {
		COM_ERROR( LOG_CH(ch_scripting), "WiredCore/Scripting: binding registrar table full\n" );
		return;
	}
	s_ws->registrars[s_ws->numRegistrars++] = fn;
}

void WiredScript_PostInit( void ) {
	int i;

	if ( !s_ws || !s_ws->lua ) {
		return;
	}

	for ( i = 0; i < s_ws->numRegistrars; i++ ) {
		s_ws->registrars[i]( s_ws->lua );
	}
	if ( s_ws->numRegistrars ) {
		Com_Log( SEV_INFO, LOG_CH(ch_scripting), "WiredCore/Scripting: %d binding registrar(s) applied\n", s_ws->numRegistrars );
	}
}

/* ---- Extension point -------------------------------------------------- */

lua_State *WiredScript_GetState( void ) {
	return s_ws ? s_ws->lua : NULL;
}
