// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_wired_scene_lua.c -- Wired Cinematic Scene: the `scene` Lua namespace

Exposes the authored cinematic-scene trigger surface on the engine-side
System VM (where all Lua runs; the cgame WASM VM has no lua_State). Three
functions build a global `scene` table:

    scene.play(name)   -- start the cinematic scripts/scene/<name>.lua
    scene.stop()       -- end it now, hard cut back to the player view
    scene.skip()       -- end it now (hard cut; distinct command so a later
                        upgrade can make it fire the authored end path)

The playback STATE lives in cg_t on the cgame side, so each function bridges
to the cgame via the console-command system (the same cmd() -> Cbuf path the
rest of the Wired Lua surface uses): the sceneplay/scenestop/sceneskip cgame command
handlers do the actual load + Start / active=0. scene.play resolves the bare name
to the scripts/scene/<name>.lua path here at the binding layer, so authors
write scene.play("intro"), not a full path.

Mirrors cl_wired_crosshair.c: a namespace table of lua_pushcfunction bindings,
registered on the System VM via WiredScript_RegisterBindings at PostInit.
===========================================================================
*/

#include "../../client.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
#include "cl_wired_scene_lua.h"

LOG_DECLARE_CHANNEL( ch_client, "client" );

#if FEAT_WIRED_UI

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* A scene name is a path token: the sceneplay command tokenizes on whitespace,
   so a name containing whitespace or quotes would split into extra args. Reject
   those (Lua error) rather than silently truncate — matches how the sibling
   subsystems treat authored names as single tokens. */
static qboolean WiredSceneLua_NameIsToken( const char *s ) {
	if ( !s || !*s ) {
		return qfalse;
	}
	for ( ; *s; s++ ) {
		unsigned char c = (unsigned char)*s;
		if ( c <= ' ' || c == '"' || c == ';' ) {
			return qfalse;
		}
	}
	return qtrue;
}

/* scene.play(name): load + start scripts/scene/<name>.lua via the sceneplay cgame
   command (the handler re-loads a clean definition every call). */
static int WiredSceneLua_Play( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );

	if ( !WiredSceneLua_NameIsToken( name ) ) {
		return luaL_error( L, "scene.play: name must be a single token (no spaces/quotes): '%s'", name );
	}

	Cbuf_ExecuteText( EXEC_APPEND, va( "sceneplay scripts/scene/%s.lua\n", name ) );
	return 0;
}

/* scene.stop(): end the cinematic now (hard cut to the player view). */
static int WiredSceneLua_Stop( lua_State *L ) {
	(void)L;
	Cbuf_ExecuteText( EXEC_APPEND, "scenestop\n" );
	return 0;
}

/* scene.skip(): end the cinematic now. Hard cut today (distinct command so a
   later upgrade can make skip fire the authored end path instead). */
static int WiredSceneLua_Skip( lua_State *L ) {
	(void)L;
	Cbuf_ExecuteText( EXEC_APPEND, "sceneskip\n" );
	return 0;
}

/* Register the global `scene` table on the System VM. Queued via
   WiredScript_RegisterBindings, invoked at PostInit. */
static void WiredSceneLua_Register( lua_State *L ) {
	lua_newtable( L );   /* scene */

	lua_pushcfunction( L, WiredSceneLua_Play );
	lua_setfield( L, -2, "play" );
	lua_pushcfunction( L, WiredSceneLua_Stop );
	lua_setfield( L, -2, "stop" );
	lua_pushcfunction( L, WiredSceneLua_Skip );
	lua_setfield( L, -2, "skip" );

	lua_setglobal( L, "scene" );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredScene: scene.play/stop/skip registered\n" );
}

void WiredScene_LuaInit( void ) {
	WiredScript_RegisterBindings( WiredSceneLua_Register );
}

#else /* !FEAT_WIRED_UI */

void WiredScene_LuaInit( void ) {}

#endif /* FEAT_WIRED_UI */
