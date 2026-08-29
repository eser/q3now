// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired_scripting.c -- Headless LuaJIT runtime for the wired engine

Core Lua scripting: VM lifecycle, sandbox, cvar metatable bridge,
print -> Com_Log, cmd() -> Cbuf_ExecuteText, file execution.

This is the engine-level runtime shared by client and headless server.
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
LOG_DECLARE_CHANNEL( ch_scripting, "scripting" );

/* ---- Persistent arena ------------------------------------------------ */
/* Engine-side control block for the LuaJIT runtime.  Lives in a persistent
   arena so it shows up in /meminfo and outlives Hunk_ClearLevel(). */

#define MAX_BINDING_REGISTRARS 16
#define WIREDSCRIPT_ARENA_SIZE (16 * 1024)  /* 16 KB — header + registrar table */

typedef struct {
    lua_State            *lua;
    WiredScript_BindingFn registrars[MAX_BINDING_REGISTRARS];
    int                   numRegistrars;
} WiredScriptState_t;

static arena_t            *s_wsArena = NULL;
static WiredScriptState_t *s_ws      = NULL;

/* Chunk-array API state (see chunk compile/cache section below). Declared
   here so WiredScript_Shutdown can reset them on teardown. */
static int  s_chunkArrayIdx     = 0;   /* 0 = no array currently held */
static int  s_chunkErrorWarned  = 0;   /* once-only fall-through error log gate */

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
	/* Reset chunk-array static state so a VM reload starts clean. The held
	   table (if any) lived on the now-closed lua_State's stack, so we only
	   clear the index — there is nothing to lua_pop. The once-only warning
	   gate must reset so the next VM logs its own warnings. */
	s_chunkArrayIdx    = 0;
	s_chunkErrorWarned = 0;
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

/* ---- Global enumeration (console tab-completion) ---------------------- */

void WiredScript_EnumerateGlobalsAndMembers(
	void (*callback)( const char *name, int type, void *ctx ), void *ctx ) {
	lua_State *L;
	int        gtbl;

	if ( !s_ws || !s_ws->lua || !callback ) {
		return;
	}
	L = s_ws->lua;

	/* lua_next does a raw traversal, so metatables (the cvar bridge on _G,
	   and any on member tables) are bypassed and only real entries are
	   reported.  The callback must not touch this lua_State -- doing so
	   would desync the lua_next walk. */
	lua_pushvalue( L, LUA_GLOBALSINDEX );
	gtbl = lua_gettop( L );
	lua_pushnil( L );
	while ( lua_next( L, gtbl ) != 0 ) {
		/* global: key at -2, value at -1.  Only string keys are completion
		   candidates; lua_tostring must not be called on a non-string key
		   as it would mutate the key in place and break lua_next. */
		if ( lua_type( L, -2 ) == LUA_TSTRING ) {
			const char *gname = lua_tostring( L, -2 );

			callback( gname, lua_type( L, -1 ), ctx );

			/* one level of table-member flattening: report each member as
			   "<table>.<member>"; nested tables are reported but not
			   descended into. */
			if ( lua_type( L, -1 ) == LUA_TTABLE && !lua_rawequal( L, -1, gtbl ) ) {  /* skip _G: its members are the globals, already listed */
				int mtbl = lua_gettop( L );
				lua_pushnil( L );
				while ( lua_next( L, mtbl ) != 0 ) {
					if ( lua_type( L, -2 ) == LUA_TSTRING ) {
						char qualified[256];
						Com_sprintf( qualified, sizeof( qualified ), "%s.%s",
							gname, lua_tostring( L, -2 ) );
						callback( qualified, lua_type( L, -1 ), ctx );
					}
					lua_pop( L, 1 );
				}
			}
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
}

/* ---- File execution --------------------------------------------------- */

qboolean WiredScript_TryExecFile( const char *filename ) {
	fileHandle_t f;
	int len;
	char *buf;
	int status;

	if ( !s_ws || !s_ws->lua ) return qfalse;

	{
		const char *ext = strrchr( filename, '.' );
		if ( !ext || Q_stricmp( ext, ".lua" ) != 0 ) {
			COM_WARN( LOG_CH(ch_scripting), "WiredCore/Scripting: only .lua files supported\n" );
			return qfalse;
		}
	}

	len = FS_FOpenFileRead( filename, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		if ( Q_stricmp( filename, "autoexec.lua" ) != 0 ) {
			COM_WARN( LOG_CH(ch_scripting), "WiredCore/Scripting: file not found '%s'\n", filename );
		}
		return qfalse;
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
		return qfalse;
	}

	status = lua_pcall( s_ws->lua, 0, 0, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		COM_ERROR( LOG_CH(ch_scripting), "Lua exec error (%s): %s\n", filename, err ? err : "unknown" );
		lua_pop( s_ws->lua, 1 );
		return qfalse;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_scripting), "WiredCore/Scripting: executed '%s'\n", filename );
	return qtrue;
}

void WiredScript_ExecFile( const char *filename ) {
	(void) WiredScript_TryExecFile( filename );
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

/* ---- Chunk compile + cache (WiredUI compositor) -----------
 *
 * Pre-compile a Lua chunk via luaL_loadbuffer, then store it as a function
 * in LUA_REGISTRYINDEX via luaL_ref. The integer ref is the stable handle;
 * to invoke, lua_rawgeti the ref to push the function, then lua_pcall.
 *
 * Chunk-array state — when WiredScript_CallChunkArrayLen succeeds, the
 * returned table is left on the Lua stack at s_chunkArrayIdx; subsequent
 * ChunkArrayItem* helpers query into that table by 1-based index.
 * Release pops the table.
 *
 * NOTE: this chunk compile/cache/call API is intentionally mirrored by the
 * untrusted User VM copy (UserVM_*Chunk* in user_vm.c). The two VMs are kept
 * separate by tier rule (this is the trusted System VM; that is memory-capped
 * + instruction-limited with its own lua_State), so the implementation is not
 * shared. The s_chunkArrayIdx / s_chunkErrorWarned statics are declared in the
 * module-state section above (so WiredScript_Shutdown can reset them on
 * teardown); they are NOT redeclared here.
 */

int WiredScript_CompileChunk( const char *text, const char *chunkName ) {
	int status;
	int ref;

	if ( !s_ws || !s_ws->lua ) return WIRED_CHUNK_NOREF;
	if ( !text || !*text ) return WIRED_CHUNK_NOREF;

	status = luaL_loadbuffer( s_ws->lua, text, strlen( text ),
	                          chunkName ? chunkName : "wiredui-chunk" );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		Com_Log( SEV_ERROR, LOG_CH(ch_scripting),
			"WiredScript: compile failed for '%s': %s\n",
			chunkName ? chunkName : "?", err ? err : "(no message)" );
		lua_pop( s_ws->lua, 1 );
		return WIRED_CHUNK_NOREF;
	}

	ref = luaL_ref( s_ws->lua, LUA_REGISTRYINDEX );
	if ( ref == LUA_REFNIL || ref == LUA_NOREF || ref <= 0 ) {
		return WIRED_CHUNK_NOREF;
	}
	return ref;
}

void WiredScript_ReleaseChunk( int chunkRef ) {
	if ( !s_ws || !s_ws->lua ) return;
	if ( chunkRef == WIRED_CHUNK_NOREF || chunkRef == LUA_NOREF || chunkRef == LUA_REFNIL || chunkRef <= 0 ) return;
	luaL_unref( s_ws->lua, LUA_REGISTRYINDEX, chunkRef );
}

/* Internal: push the chunk onto the Lua stack. Returns qtrue iff a function
 * was pushed (caller responsible for lua_pcall + result handling). */
static qboolean wired_chunk_push( int chunkRef, const char *callerTag ) {
	if ( !s_ws || !s_ws->lua ) return qfalse;
	if ( chunkRef == WIRED_CHUNK_NOREF || chunkRef <= 0 ) return qfalse;
	lua_rawgeti( s_ws->lua, LUA_REGISTRYINDEX, chunkRef );
	if ( !lua_isfunction( s_ws->lua, -1 ) ) {
		lua_pop( s_ws->lua, 1 );
		if ( !s_chunkErrorWarned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_scripting),
				"WiredScript: chunk ref %d (%s) is not a function\n",
				chunkRef, callerTag ? callerTag : "?" );
			s_chunkErrorWarned = 1;
		}
		return qfalse;
	}
	(void) callerTag;
	return qtrue;
}

/* Internal: call the function on top of the stack with no args, leaving
 * up to `nResults` results on the stack. Returns qtrue iff the call
 * succeeded. On failure, the error message is popped + logged. */
static qboolean wired_chunk_pcall( int nResults, const char *callerTag ) {
	int status;
	if ( !s_ws || !s_ws->lua ) return qfalse;
	status = lua_pcall( s_ws->lua, 0, nResults, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		Com_Log( SEV_WARN, LOG_CH(ch_scripting),
			"WiredScript: chunk call failed (%s): %s\n",
			callerTag ? callerTag : "?", err ? err : "(no message)" );
		lua_pop( s_ws->lua, 1 );
		return qfalse;
	}
	return qtrue;
}

/* ---- With-arguments invocation seam (crosshair update(state)) -------- *
 *
 * The chunk-array + single-value helpers above all pcall with nargs=0. The
 * procedural-crosshair pipeline needs to call a held chunk WITH a C-built
 * argument table (update(state) -> delta) and then read a nested result
 * table. Rather than re-implement the registry-ref + error-logging dance in
 * the client module, the module includes lua.h directly (the established
 * cl_wired_store_lua.c pattern), pushes its own table argument between these
 * two calls, and reads the result off the same System VM stack. */

/* Push a compiled chunk (held by ref) onto the System VM stack and return the
 * live lua_State with the chunk function on top, or NULL if the ref is invalid
 * / Lua is down. Caller then pushes arguments and invokes WiredScript_PCallArgs. */
lua_State *WiredScript_PushChunkForArgs( int chunkRef ) {
	if ( !s_ws || !s_ws->lua ) return NULL;
	if ( !wired_chunk_push( chunkRef, "arg-chunk" ) ) return NULL;
	return s_ws->lua;
}

/* pcall the function on top of the System VM stack with `nargs` args already
 * pushed above it, leaving `nResults` results. Mirrors wired_chunk_pcall's
 * once-style error logging. On failure the error is popped + logged. */
qboolean WiredScript_PCallArgs( int nargs, int nResults, const char *callerTag ) {
	int status;
	if ( !s_ws || !s_ws->lua ) return qfalse;
	status = lua_pcall( s_ws->lua, nargs, nResults, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_ws->lua, -1 );
		Com_Log( SEV_WARN, LOG_CH(ch_scripting),
			"WiredScript: chunk call failed (%s): %s\n",
			callerTag ? callerTag : "?", err ? err : "(no message)" );
		lua_pop( s_ws->lua, 1 );
		return qfalse;
	}
	return qtrue;
}

/* Validate that the table at the given stack index is a dense 1..N array.
 * Returns N on success or -1 on failure. */
static int wired_chunk_array_validate( int tableIdx ) {
	int n, i;
	lua_State *L = s_ws->lua;

	if ( !lua_istable( L, tableIdx ) ) return -1;

	n = lua_objlen( L, tableIdx );   /* # operator equivalent — Lua 5.1 / LuaJIT */
	if ( n < 0 ) return -1;

	/* Per Q-3=a: dense 1..N validation. Walk indices, ensure each is non-nil.
	 * A hole would make lua_objlen unreliable (it scans a length boundary). */
	for ( i = 1; i <= n; i++ ) {
		lua_rawgeti( L, tableIdx, i );
		if ( lua_isnil( L, -1 ) ) {
			lua_pop( L, 1 );
			return -1;
		}
		lua_pop( L, 1 );
	}
	return n;
}

int WiredScript_CallChunkArrayLen( int chunkRef ) {
	int n;

	if ( s_chunkArrayIdx != 0 ) {
		/* Caller forgot to release the previous array; release defensively. */
		WiredScript_ChunkArrayRelease();
	}

	if ( !wired_chunk_push( chunkRef, "array-chunk" ) ) return -1;
	if ( !wired_chunk_pcall( 1, "array-chunk" ) ) return -1;

	/* Result expected on top of stack. */
	n = wired_chunk_array_validate( -1 );
	if ( n < 0 ) {
		lua_pop( s_ws->lua, 1 );
		if ( !s_chunkErrorWarned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_scripting),
				"WiredScript: chunk returned non-dense / non-array result — Q-3=a violation\n" );
			s_chunkErrorWarned = 1;
		}
		return -1;
	}

	if ( n == 0 ) {
		/* Empty table: the caller's `n <= 0` early-out won't call Release, so we
		 * must NOT leave the table on the stack / set s_chunkArrayIdx — pop it
		 * here (otherwise the slot leaks on every zero-row repeat block). */
		lua_pop( s_ws->lua, 1 );
		s_chunkArrayIdx = 0;
		return 0;
	}

	s_chunkArrayIdx = lua_gettop( s_ws->lua );   /* absolute index of the table */
	return n;
}

qboolean WiredScript_ChunkArrayItemAsString( int index, char *out, size_t outSize ) {
	const char *s;
	if ( s_chunkArrayIdx == 0 || !out || outSize == 0 ) return qfalse;
	lua_rawgeti( s_ws->lua, s_chunkArrayIdx, index );
	s = lua_tostring( s_ws->lua, -1 );
	if ( s ) Q_strncpyz( out, s, outSize );
	lua_pop( s_ws->lua, 1 );
	return s != NULL;
}

qboolean WiredScript_ChunkArrayItemAsNumber( int index, double *out ) {
	int        isnum;
	lua_Number n;
	if ( s_chunkArrayIdx == 0 || !out ) return qfalse;
	lua_rawgeti( s_ws->lua, s_chunkArrayIdx, index );
	/* LuaJIT exposes lua_tonumber but no lua_tonumberx; isnum via lua_type. */
	isnum = lua_isnumber( s_ws->lua, -1 );
	n = lua_tonumber( s_ws->lua, -1 );
	lua_pop( s_ws->lua, 1 );
	if ( isnum ) *out = (double) n;
	return isnum ? qtrue : qfalse;
}

qboolean WiredScript_ChunkArrayItemFieldAsString( int index, const char *field,
                                                   char *out, size_t outSize )
{
	const char *s;
	if ( s_chunkArrayIdx == 0 || !out || outSize == 0 || !field ) return qfalse;
	lua_rawgeti( s_ws->lua, s_chunkArrayIdx, index );
	if ( !lua_istable( s_ws->lua, -1 ) ) {
		lua_pop( s_ws->lua, 1 );
		return qfalse;
	}
	lua_getfield( s_ws->lua, -1, field );
	s = lua_tostring( s_ws->lua, -1 );
	if ( s ) Q_strncpyz( out, s, outSize );
	lua_pop( s_ws->lua, 2 );
	return s != NULL;
}

void WiredScript_ChunkArrayRelease( void ) {
	if ( s_chunkArrayIdx == 0 ) return;
	lua_pop( s_ws->lua, 1 );   /* pop the table */
	s_chunkArrayIdx = 0;
}

qboolean WiredScript_CallChunkBool( int chunkRef, qboolean defaultVal ) {
	qboolean result = defaultVal;
	if ( !wired_chunk_push( chunkRef, "bool-chunk" ) ) return defaultVal;
	if ( !wired_chunk_pcall( 1, "bool-chunk" ) ) return defaultVal;
	if ( lua_isboolean( s_ws->lua, -1 ) ) {
		result = lua_toboolean( s_ws->lua, -1 ) ? qtrue : qfalse;
	} else if ( !lua_isnil( s_ws->lua, -1 ) ) {
		/* Lua truthiness: non-nil + non-false = true. */
		result = qtrue;
	} else {
		result = qfalse;
	}
	lua_pop( s_ws->lua, 1 );
	return result;
}

qboolean WiredScript_CallChunkString( int chunkRef, char *out, size_t outSize ) {
	const char *s;
	if ( !out || outSize == 0 ) return qfalse;
	if ( !wired_chunk_push( chunkRef, "string-chunk" ) ) return qfalse;
	if ( !wired_chunk_pcall( 1, "string-chunk" ) ) return qfalse;
	s = lua_tostring( s_ws->lua, -1 );
	if ( s ) Q_strncpyz( out, s, outSize );
	lua_pop( s_ws->lua, 1 );
	return s != NULL;
}

qboolean WiredScript_CallChunkNumber( int chunkRef, double *out ) {
	int        isnum;
	lua_Number n;
	if ( !out ) return qfalse;
	if ( !wired_chunk_push( chunkRef, "number-chunk" ) ) return qfalse;
	if ( !wired_chunk_pcall( 1, "number-chunk" ) ) return qfalse;
	isnum = lua_isnumber( s_ws->lua, -1 );
	n = lua_tonumber( s_ws->lua, -1 );
	lua_pop( s_ws->lua, 1 );
	if ( isnum ) *out = (double) n;
	return isnum ? qtrue : qfalse;
}
