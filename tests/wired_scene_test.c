// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
 * wired_scene_test.c -- standalone unit test for the Lua scene loader.
 *
 * Spins up a minimal Lua VM (luaL_newstate), runs a scene-definition string
 * that returns a table, and asserts WiredScene_ReadTable fills wiredScene_t
 * correctly. Deterministic, no engine init: wired_scene.c is built with
 * -DWIRED_SCENE_STANDALONE so it routes diagnostics to stderr and needs no
 * q_shared / WiredScript chain. Links LuaJIT directly.
 *
 * Coverage:
 *   1. eyePath: knot count == number of knot entries; round-tripped t + pos;
 *      type/boundary decoded.
 *   2. named target subtables -> correctly-named targets with right knot counts.
 *   3. fov table -> fovStart/fovEnd/fovLenSec exact, hasFov set (incl. ["end"]).
 *   4. every live event verb decoded (type/timeMs/fparam/fparam2/sparam).
 *   5. a TCB knot's {tension,continuity,bias} land in the curve's tcb[].
 *   6. missing optional fields default cleanly; a malformed (non-table) result
 *      fails gracefully (returns 0, no crash).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../code/qcommon/wired/scene/wired_scene.h"

static int g_checks = 0;
static int g_fail   = 0;

static void check( int cond, const char *msg ) {
	g_checks++;
	if ( cond ) {
		printf( "  ok    %s\n", msg );
	} else {
		printf( "  FAIL  %s\n", msg );
		g_fail++;
	}
}

static int feq( float a, float b ) { return fabsf( a - b ) <= 1e-4f; }

/* Run a Lua chunk that returns a table, leaving the table on top. Returns 1 on
   success (table on stack), 0 otherwise (nothing extra left). */
static int run_returns_table( lua_State *L, const char *src ) {
	if ( luaL_loadstring( L, src ) != 0 ) {
		fprintf( stderr, "load error: %s\n", lua_tostring( L, -1 ) );
		lua_pop( L, 1 );
		return 0;
	}
	if ( lua_pcall( L, 0, 1, 0 ) != 0 ) {
		fprintf( stderr, "run error: %s\n", lua_tostring( L, -1 ) );
		lua_pop( L, 1 );
		return 0;
	}
	return 1;
}

static const char *kSceneLua =
	"return {\n"
	"  time = 12.5,\n"
	"  eyePath = {\n"
	"    type = 'catmullrom',\n"
	"    boundary = 'clamped',\n"
	"    knots = {\n"
	"      { t = 0,  pos = { 0, 0, 0 } },\n"
	"      { t = 10, pos = { 100, 0, 0 } },\n"
	"      { t = 20, pos = { 100, 200, 0 } },\n"
	"      { t = 30, pos = { 0, 200, 50 } },\n"
	"    },\n"
	"  },\n"
	"  targets = {\n"
	"    hero = {\n"
	"      type = 'catmullrom', boundary = 'free',\n"
	"      knots = { { t = 0, pos = { 50, 50, 10 } }, { t = 30, pos = { 60, 40, 10 } } },\n"
	"    },\n"
	"    villain = {\n"
	"      type = 'tcb',\n"
	"      knots = {\n"
	"        { t = 0,  pos = { -50, 0, 0 }, tcb = { 0.5, -0.25, 0.75 } },\n"
	"        { t = 30, pos = { -40, 10, 0 } },\n"   /* missing tcb -> defaults 0,0,0 */
	"      },\n"
	"    },\n"
	"  },\n"
	"  fov = { start = 90, ['end'] = 60, length = 3 },\n"
	"  events = {\n"
	"    { verb = 'wait',    time = 500,   seconds = 1.5 },\n"
	"    { verb = 'target',  time = 3000,  name = 'hero' },\n"
	"    { verb = 'fov',     time = 2000,  fov = 60, length = 3 },\n"
	"    { verb = 'fadeout', time = 8000,  seconds = 2 },\n"
	"    { verb = 'fadein',  time = 9000,  seconds = 1 },\n"
	"    { verb = 'feather', time = 0 },\n"
	"    { verb = 'stop',    time = 12000 },\n"
	"    { verb = 'scene',   time = 11000, name = 'next_scene' },\n"
	"    { verb = 'bogus',   time = 99999 },\n"   /* unknown verb -> skipped */
	"  },\n"
	"}\n";

static int findEvent( const wiredScene_t *c, int type ) {
	int i;
	for ( i = 0; i < c->numEvents; i++ ) if ( c->events[i].type == type ) return i;
	return -1;
}

int main( void ) {
	lua_State    *L;
	wiredScene_t def;
	int ok, idx;

	printf( "wired_scene Lua-loader test\n" );
	printf( "============================\n" );

	L = luaL_newstate();
	luaL_openlibs( L );

	/* ── load the well-formed scene ──────────────────────────────────── */
	if ( !run_returns_table( L, kSceneLua ) ) { printf( "chunk failed\n" ); return 1; }
	ok = WiredScene_ReadTable( &def, L, "test.scene.lua" );
	lua_pop( L, 1 );   /* pop the table */
	check( ok, "ReadTable returns success" );
	if ( !ok ) { printf( "aborting\n" ); return 1; }

	/* [1] time + eyePath */
	printf( "[1] time + eyePath knots (count + round-trip + type/boundary)\n" );
	check( feq( def.totalTimeSec, 12.5f ), "time == 12.5" );
	check( def.eyePath.numKnots == 4, "eyePath has 4 knots" );
	check( def.eyePath.curveType == WCURVE_CATMULLROM, "eyePath type == catmullrom" );
	check( def.eyePath.boundaryType == WCURVE_BT_CLAMPED, "eyePath boundary == clamped" );
	check( feq( def.eyePath.times[0], 0.0f ) && feq( def.eyePath.times[3], 30.0f ),
	       "eyePath knot times [0]=0 [3]=30" );
	check( feq( def.eyePath.values[1][0], 100.0f ) && feq( def.eyePath.values[1][1], 0.0f ) && feq( def.eyePath.values[1][2], 0.0f ),
	       "eyePath knot 1 pos == (100,0,0)" );
	check( feq( def.eyePath.values[3][0], 0.0f ) && feq( def.eyePath.values[3][1], 200.0f ) && feq( def.eyePath.values[3][2], 50.0f ),
	       "eyePath knot 3 pos == (0,200,50)" );

	/* [2] targets */
	printf( "[2] named target subtables\n" );
	check( def.numTargets == 2, "2 targets parsed" );
	{
		/* keyed table order isn't guaranteed — look up by name */
		int hero = -1, villain = -1, i;
		for ( i = 0; i < def.numTargets; i++ ) {
			if ( !strcmp( def.targets[i].name, "hero" ) )    hero = i;
			if ( !strcmp( def.targets[i].name, "villain" ) ) villain = i;
		}
		check( hero >= 0 && def.targets[hero].path.numKnots == 2, "target 'hero' has 2 knots" );
		check( villain >= 0 && def.targets[villain].path.numKnots == 2, "target 'villain' has 2 knots" );
		check( villain >= 0 && def.targets[villain].path.curveType == WCURVE_TCB, "target 'villain' type == tcb" );

		/* [5] TCB knot triple + defaulted second knot */
		printf( "[5] TCB knot tension/continuity/bias (+ default when absent)\n" );
		check( villain >= 0 && feq( def.targets[villain].path.tcb[0][0], 0.5f ), "villain knot0 tension == 0.5" );
		check( villain >= 0 && feq( def.targets[villain].path.tcb[0][1], -0.25f ), "villain knot0 continuity == -0.25" );
		check( villain >= 0 && feq( def.targets[villain].path.tcb[0][2], 0.75f ), "villain knot0 bias == 0.75" );
		check( villain >= 0 && feq( def.targets[villain].path.tcb[1][0], 0.0f ) && feq( def.targets[villain].path.tcb[1][1], 0.0f ) && feq( def.targets[villain].path.tcb[1][2], 0.0f ),
		       "villain knot1 tcb defaults to (0,0,0) when absent" );
	}

	/* [3] fov */
	printf( "[3] fov table (incl. ['end'])\n" );
	check( def.hasFov, "hasFov set" );
	check( feq( def.fovStart, 90.0f ), "fovStart == 90" );
	check( feq( def.fovEnd, 60.0f ), "fovEnd == 60 (['end'] key)" );
	check( feq( def.fovLenSec, 3.0f ), "fovLenSec == 3" );

	/* [4] events */
	printf( "[4] events (count + per-verb decode; unknown verb skipped)\n" );
	check( def.numEvents == 8, "8 live events parsed (bogus verb skipped)" );

	idx = findEvent( &def, WSCENE_EV_WAIT );
	check( idx >= 0 && def.events[idx].timeMs == 500 && feq( def.events[idx].fparam, 1.5f ),
	       "wait: timeMs=500 seconds=1.5" );
	idx = findEvent( &def, WSCENE_EV_TARGET );
	check( idx >= 0 && def.events[idx].timeMs == 3000 && !strcmp( def.events[idx].sparam, "hero" ),
	       "target: timeMs=3000 name='hero'" );
	idx = findEvent( &def, WSCENE_EV_FOV );
	check( idx >= 0 && def.events[idx].timeMs == 2000 && feq( def.events[idx].fparam, 60.0f ) && feq( def.events[idx].fparam2, 3.0f ),
	       "fov: timeMs=2000 fov=60 length=3" );
	idx = findEvent( &def, WSCENE_EV_FADEOUT );
	check( idx >= 0 && def.events[idx].timeMs == 8000 && feq( def.events[idx].fparam, 2.0f ),
	       "fadeout: timeMs=8000 seconds=2" );
	idx = findEvent( &def, WSCENE_EV_FADEIN );
	check( idx >= 0 && def.events[idx].timeMs == 9000 && feq( def.events[idx].fparam, 1.0f ),
	       "fadein: timeMs=9000 seconds=1" );
	idx = findEvent( &def, WSCENE_EV_FEATHER );
	check( idx >= 0 && def.events[idx].timeMs == 0, "feather: timeMs=0 (no payload)" );
	idx = findEvent( &def, WSCENE_EV_STOP );
	check( idx >= 0 && def.events[idx].timeMs == 12000, "stop: timeMs=12000 (no payload)" );
	idx = findEvent( &def, WSCENE_EV_SCENE );
	check( idx >= 0 && def.events[idx].timeMs == 11000 && !strcmp( def.events[idx].sparam, "next_scene" ),
	       "scene: timeMs=11000 name='next_scene'" );

	/* [6a] missing-field defaults: a nearly-empty scene */
	printf( "[6] defaults + malformed-input handling\n" );
	if ( run_returns_table( L, "return { time = 5 }\n" ) ) {
		wiredScene_t bare;
		ok = WiredScene_ReadTable( &bare, L, "bare" );
		lua_pop( L, 1 );
		check( ok && feq( bare.totalTimeSec, 5.0f ) && bare.eyePath.numKnots == 0
		       && bare.numTargets == 0 && bare.numEvents == 0 && bare.hasFov == 0,
		       "bare table (only time) -> clean empty defaults" );
	} else {
		check( 0, "bare table chunk ran" );
	}

	/* [6b] malformed: a chunk that returns a non-table (a number) must fail. */
	if ( run_returns_table( L, "return 42\n" ) ) {
		wiredScene_t bad;
		ok = WiredScene_ReadTable( &bad, L, "bad" );
		lua_pop( L, 1 );
		check( ok == 0, "non-table result -> ReadTable returns 0 (no crash)" );
	} else {
		check( 0, "non-table chunk ran" );
	}

	lua_close( L );

	printf( "--------------------------------------------\n" );
	printf( "%d/%d checks passed\n", g_checks - g_fail, g_checks );
	return g_fail ? 1 : 0;
}
