// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
wired_scene.c -- cinematic-scene definition + Lua-table loader (plain-C)

See wired_scene.h for the table shape + data model. A scene .lua returns a
table; this reads it into a wiredScene_t. The load + table-read idiom mirrors
the crosshair loader (cl_wired_crosshair.c): compile the chunk, pcall it,
verify a table, read fields with lua_getfield / lua_rawgeti.

Field readers here are the same shape as the crosshair's xh_field_* helpers.
A {x,y,z} knot position is the crosshair's {r,g,b,a} tuple reader with count 3;
the knots[] / events[] arrays are its array-of-subtables iteration.

The wiredScene_t / wiredSceneEvent_t / WSCENE_EV_* data model is plain-C; the
Lua VM is used ONLY here at load time. WiredCurve_Init + WiredCurve_AddValue
populate the spline curves (eye path + each target).
===========================================================================
*/

#include "wired_scene.h"

#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#ifndef WIRED_SCENE_STANDALONE
#include "../../q_shared.h"
#include "../../qcommon.h"
#include "../core/scripting/wired_scripting.h"
LOG_DECLARE_CHANNEL( ch_scene, "scene" );
#define WSCENE_LOG_ERR( ... )  Com_Log( SEV_ERROR, LOG_CH(ch_scene), __VA_ARGS__ )
#define WSCENE_LOG_WARN( ... ) Com_Log( SEV_WARN,  LOG_CH(ch_scene), __VA_ARGS__ )
#else
/* Standalone unit-test build: no q_shared.h / logging chain — the test spins
   up a minimal luaL_newstate and calls WiredScene_ReadTable directly. Route
   diagnostics to stderr and provide the couple of string helpers used. */
#include <stdio.h>
#include <stdlib.h>
#define WSCENE_LOG_ERR( ... )  fprintf( stderr, "ERROR: " __VA_ARGS__ )
#define WSCENE_LOG_WARN( ... ) fprintf( stderr, "WARNING: " __VA_ARGS__ )
static int WiredScene_stricmp( const char *a, const char *b ) {
	int c1, c2;
	do {
		c1 = (unsigned char)*a++; c2 = (unsigned char)*b++;
		if ( c1 >= 'A' && c1 <= 'Z' ) c1 += 'a' - 'A';
		if ( c2 >= 'A' && c2 <= 'Z' ) c2 += 'a' - 'A';
		if ( c1 != c2 ) return c1 - c2;
	} while ( c1 );
	return 0;
}
static void WiredScene_strncpyz( char *dest, const char *src, int destsize ) {
	if ( destsize <= 0 ) return;
	strncpy( dest, src ? src : "", (size_t)destsize - 1 );
	dest[destsize - 1] = '\0';
}
#define Q_stricmp  WiredScene_stricmp
#define Q_strncpyz WiredScene_strncpyz
#endif

/* ── scalar field readers (mirror the crosshair xh_field_* helpers) ────── */

/* Number field `key` of the table at stack index `t`, or `def` if absent. */
static float WiredScene_FieldNum( lua_State *L, int t, const char *key, float def ) {
	float v = def;
	lua_getfield( L, t, key );
	if ( lua_isnumber( L, -1 ) ) v = (float)lua_tonumber( L, -1 );
	lua_pop( L, 1 );
	return v;
}

/* String field `key` of the table at `t` into `out` (WIRED_SCENE_NAME_LEN); "" if
   absent. */
static void WiredScene_FieldStr( lua_State *L, int t, const char *key, char *out, int outLen ) {
	lua_getfield( L, t, key );
	Q_strncpyz( out, lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : "", outLen );
	lua_pop( L, 1 );
}

/* Numeric tuple field `key` of the table at `t` into out[count] (e.g. a
   {x,y,z} pos or a {tension,continuity,bias} triple). Missing entries stay at
   their incoming value. Returns 1 if `key` was a table, 0 otherwise. */
static int WiredScene_FieldTuple( lua_State *L, int t, const char *key, float *out, int count ) {
	int found = 0, i;
	lua_getfield( L, t, key );
	if ( lua_istable( L, -1 ) ) {
		found = 1;
		for ( i = 0; i < count; i++ ) {
			lua_rawgeti( L, -1, i + 1 );
			if ( lua_isnumber( L, -1 ) ) out[i] = (float)lua_tonumber( L, -1 );
			lua_pop( L, 1 );
		}
	}
	lua_pop( L, 1 );
	return found;
}

/* ── curve type / boundary string decode ──────────────────────────────── */

static int WiredScene_CurveType( const char *s ) {
	if ( s && !Q_stricmp( s, "tcb" ) ) return WCURVE_TCB;
	return WCURVE_CATMULLROM;   /* default + "catmullrom" */
}

static int WiredScene_Boundary( const char *s ) {
	if ( s && !Q_stricmp( s, "clamped" ) ) return WCURVE_BT_CLAMPED;
	if ( s && !Q_stricmp( s, "closed"  ) ) return WCURVE_BT_CLOSED;
	return WCURVE_BT_FREE;      /* default + "free" */
}

/* ── a knot-bearing curve block (eyePath + each target share this) ─────── */

/*
Read a `{ type=..., boundary=..., knots={ {t=,pos={},tcb={}}, ... } }` table
(at stack index `blockT`) into `curve` via WiredCurve_Init + WiredCurve_AddValue.
`name` is used only in overflow warnings. Leaves the stack as it found it.
*/
static void WiredScene_ReadCurve( lua_State *L, int blockT, wiredCurve_t *curve, const char *name ) {
	char typeStr[32]     = { 0 };
	char boundaryStr[32] = { 0 };
	int  curveType, boundaryType;
	int  n = 0, i;

	WiredScene_FieldStr( L, blockT, "type",     typeStr,     sizeof( typeStr ) );
	WiredScene_FieldStr( L, blockT, "boundary", boundaryStr, sizeof( boundaryStr ) );
	curveType    = WiredScene_CurveType( typeStr );
	boundaryType = WiredScene_Boundary( boundaryStr );

	WiredCurve_Init( curve, curveType, boundaryType );

	lua_getfield( L, blockT, "knots" );
	if ( lua_istable( L, -1 ) ) {
		int knotsT = lua_gettop( L );
		n = (int)lua_objlen( L, knotsT );   /* # operator — Lua 5.1 / LuaJIT */
		for ( i = 0; i < n; i++ ) {
			lua_rawgeti( L, knotsT, i + 1 );      /* push knots[i+1] */
			if ( lua_istable( L, -1 ) ) {
				int   kT = lua_gettop( L );
				float t   = WiredScene_FieldNum( L, kT, "t", 0.0f );
				float pos[3] = { 0.0f, 0.0f, 0.0f };
				float tcb[3] = { 0.0f, 0.0f, 0.0f };
				WiredScene_FieldTuple( L, kT, "pos", pos, 3 );
				WiredScene_FieldTuple( L, kT, "tcb", tcb, 3 );   /* optional */
				if ( WiredCurve_AddValue( curve, t, pos, tcb[0], tcb[1], tcb[2] ) < 0 ) {
					WSCENE_LOG_WARN( "scene '%s': too many knots (>%d), dropping extras\n",
					               name ? name : "(lua)", WIRED_MAX_KNOTS );
				}
			}
			lua_pop( L, 1 );                      /* pop knots[i+1] */
		}
	}
	lua_pop( L, 1 );                              /* pop knots (or the non-table) */
}

/* ── events[] ─────────────────────────────────────────────────────────── */

/* Map a verb string to a WSCENE_EV_* type, or -1 if unknown. */
static int WiredScene_VerbType( const char *verb ) {
	if ( !verb ) return -1;
	if ( !Q_stricmp( verb, "wait"    ) ) return WSCENE_EV_WAIT;
	if ( !Q_stricmp( verb, "target"  ) ) return WSCENE_EV_TARGET;
	if ( !Q_stricmp( verb, "fov"     ) ) return WSCENE_EV_FOV;
	if ( !Q_stricmp( verb, "fadeout" ) ) return WSCENE_EV_FADEOUT;
	if ( !Q_stricmp( verb, "fadein"  ) ) return WSCENE_EV_FADEIN;
	if ( !Q_stricmp( verb, "feather" ) ) return WSCENE_EV_FEATHER;
	if ( !Q_stricmp( verb, "stop"    ) ) return WSCENE_EV_STOP;
	if ( !Q_stricmp( verb, "scene"   ) ) return WSCENE_EV_SCENE;
	if ( !Q_stricmp( verb, "thirdperson"  ) ) return WSCENE_EV_THIRDPERSON;
	if ( !Q_stricmp( verb, "hud"          ) ) return WSCENE_EV_HUD;
	if ( !Q_stricmp( verb, "playerfreeze" ) ) return WSCENE_EV_PLAYERFREEZE;
	if ( !Q_stricmp( verb, "caption"      ) ) return WSCENE_EV_CAPTION;
	return -1;
}

/* Decode one event table (at index `evT`) into `ev`. Returns 1 if the verb is
   a known live verb (ev filled), 0 if unknown/absent (caller skips it). */
static int WiredScene_ReadEvent( lua_State *L, int evT, wiredSceneEvent_t *ev ) {
	char verb[WIRED_SCENE_NAME_LEN] = { 0 };
	int  type;

	memset( ev, 0, sizeof( *ev ) );
	WiredScene_FieldStr( L, evT, "verb", verb, sizeof( verb ) );
	type = WiredScene_VerbType( verb );
	if ( type < 0 ) return 0;

	ev->type   = type;
	ev->timeMs = (int)WiredScene_FieldNum( L, evT, "time", 0.0f );

	switch ( type ) {
		case WSCENE_EV_WAIT:
		case WSCENE_EV_FADEOUT:
		case WSCENE_EV_FADEIN:
			ev->fparam = WiredScene_FieldNum( L, evT, "seconds", 0.0f );
			break;
		case WSCENE_EV_FOV:
			ev->fparam  = WiredScene_FieldNum( L, evT, "fov",    0.0f );  /* target degrees */
			ev->fparam2 = WiredScene_FieldNum( L, evT, "length", 0.0f );  /* lerp seconds   */
			break;
		case WSCENE_EV_TARGET:
		case WSCENE_EV_SCENE:
			WiredScene_FieldStr( L, evT, "name", ev->sparam, sizeof( ev->sparam ) );
			break;
		case WSCENE_EV_THIRDPERSON:
		case WSCENE_EV_HUD:
		case WSCENE_EV_PLAYERFREEZE:
			ev->fparam = WiredScene_FieldNum( L, evT, "on", 0.0f );  /* 1 = on, 0 = off */
			break;
		case WSCENE_EV_CAPTION:
			WiredScene_FieldStr( L, evT, "key", ev->sparam, sizeof( ev->sparam ) );  /* l10n key */
			ev->fparam = WiredScene_FieldNum( L, evT, "seconds", 0.0f );             /* opt display secs */
			break;
		case WSCENE_EV_FEATHER:
		case WSCENE_EV_STOP:
		default:
			break;   /* no payload */
	}
	return 1;
}

/* ── top-level table read ─────────────────────────────────────────────── */

int WiredScene_ReadTable( wiredScene_t *out, lua_State *L, const char *name ) {
	int t;

	if ( !out || !L ) return 0;
	memset( out, 0, sizeof( *out ) );

	if ( !lua_istable( L, -1 ) ) {
		WSCENE_LOG_ERR( "scene '%s': definition did not return a table\n", name ? name : "(lua)" );
		return 0;
	}
	t = lua_gettop( L );

	/* time */
	out->totalTimeSec = WiredScene_FieldNum( L, t, "time", 0.0f );

	/* cameraSpace: "world" (default) or "player" (orbital, knots are offsets) */
	{
		char space[16] = { 0 };
		WiredScene_FieldStr( L, t, "cameraSpace", space, sizeof( space ) );
		out->cameraSpace = ( !Q_stricmp( space, "player" ) )
		                 ? WSCENE_SPACE_PLAYER : WSCENE_SPACE_WORLD;
	}

	/* eyePath */
	lua_getfield( L, t, "eyePath" );
	if ( lua_istable( L, -1 ) ) {
		WiredScene_ReadCurve( L, lua_gettop( L ), &out->eyePath, name );
	} else {
		WiredCurve_Init( &out->eyePath, WCURVE_CATMULLROM, WCURVE_BT_FREE );
	}
	lua_pop( L, 1 );

	/* targets = { <name> = { curve }, ... } — iterate the keyed subtable */
	lua_getfield( L, t, "targets" );
	if ( lua_istable( L, -1 ) ) {
		int targetsT = lua_gettop( L );
		lua_pushnil( L );                         /* first key */
		while ( lua_next( L, targetsT ) != 0 ) {
			/* key at -2, value (the curve table) at -1 */
			if ( lua_isstring( L, -2 ) && lua_istable( L, -1 ) ) {
				if ( out->numTargets >= WIRED_MAX_SCENE_TARGETS ) {
					WSCENE_LOG_WARN( "scene '%s': too many targets (>%d), dropping '%s'\n",
					               name ? name : "(lua)", WIRED_MAX_SCENE_TARGETS, lua_tostring( L, -2 ) );
				} else {
					wiredSceneTarget_t *tgt = &out->targets[out->numTargets];
					Q_strncpyz( tgt->name, lua_tostring( L, -2 ), sizeof( tgt->name ) );
					WiredScene_ReadCurve( L, lua_gettop( L ), &tgt->path, tgt->name );
					out->numTargets++;
				}
			}
			lua_pop( L, 1 );                      /* pop value, keep key for lua_next */
		}
	}
	lua_pop( L, 1 );                              /* pop targets */

	/* fov = { start, ["end"], length } */
	lua_getfield( L, t, "fov" );
	if ( lua_istable( L, -1 ) ) {
		int fovT = lua_gettop( L );
		out->fovStart  = WiredScene_FieldNum( L, fovT, "start",  0.0f );
		out->fovEnd    = WiredScene_FieldNum( L, fovT, "end",    0.0f );
		out->fovLenSec = WiredScene_FieldNum( L, fovT, "length", 0.0f );
		out->hasFov    = 1;
	}
	lua_pop( L, 1 );                              /* pop fov */

	/* events = { { verb, time, ... }, ... } */
	lua_getfield( L, t, "events" );
	if ( lua_istable( L, -1 ) ) {
		int eventsT = lua_gettop( L );
		int n = (int)lua_objlen( L, eventsT );   /* # operator — Lua 5.1 / LuaJIT */
		int i;
		for ( i = 0; i < n; i++ ) {
			lua_rawgeti( L, eventsT, i + 1 );     /* push events[i+1] */
			if ( lua_istable( L, -1 ) ) {
				wiredSceneEvent_t ev;
				if ( WiredScene_ReadEvent( L, lua_gettop( L ), &ev ) ) {
					if ( out->numEvents < WIRED_MAX_SCENE_EVENTS ) {
						out->events[out->numEvents++] = ev;
					} else {
						WSCENE_LOG_WARN( "scene '%s': too many events (>%d)\n",
						               name ? name : "(lua)", WIRED_MAX_SCENE_EVENTS );
					}
				}
			}
			lua_pop( L, 1 );                      /* pop events[i+1] */
		}
	}
	lua_pop( L, 1 );                              /* pop events */

	return 1;
}

/* ── engine load-from-file (WiredScript VM) ───────────────────────────── */

#ifndef WIRED_SCENE_STANDALONE
int WiredScene_LoadFromFile( wiredScene_t *out, const char *path ) {
	fileHandle_t f;
	int          len;
	char        *buf;
	int          ref;
	lua_State   *L;
	int          ok;

	if ( !out || !path || !*path ) return 0;

	len = FS_FOpenFileRead( path, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		if ( f ) FS_FCloseFile( f );
		WSCENE_LOG_ERR( "scene: '%s' not found\n", path );
		return 0;
	}

	buf = Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

	/* Compile → pcall (0 args, 1 result) so the returned table is on top of the
	   System VM stack — the same idiom the crosshair loader uses. */
	ref = WiredScript_CompileChunk( buf, path );
	Z_Free( buf );
	if ( ref == WIRED_CHUNK_NOREF ) {
		return 0;   /* CompileChunk already logged */
	}

	L = WiredScript_PushChunkForArgs( ref );
	if ( !L ) {
		WiredScript_ReleaseChunk( ref );
		return 0;
	}
	if ( !WiredScript_PCallArgs( 0, 1, path ) ) {
		WiredScript_ReleaseChunk( ref );
		return 0;
	}
	WiredScript_ReleaseChunk( ref );   /* the chunk fn; the result is on the stack */

	ok = WiredScene_ReadTable( out, L, path );
	lua_pop( L, 1 );                   /* pop the result table */
	return ok;
}
#endif
