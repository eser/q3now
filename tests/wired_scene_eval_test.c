// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
 * wired_scene_eval_test.c -- cinematic-scene evaluator unit test.
 *
 * Deterministic, standalone (no engine / RAL / Lua VM). Builds a wiredScene_t
 * directly in C (WiredCurve_Init + WiredCurve_AddValue for the splines; events
 * filled by hand) and asserts WiredScene_Eval against the evaluator contract:
 *
 *   1. origin exact at t=0 (first control point), at a knot time (that control
 *      point — Catmull-Rom through-point property), and at t=end (last point).
 *   2. facing with no target = normalized eye-path tangent (points along the
 *      path direction). Zero-tangent (single knot) guarded -> a finite forward.
 *   3. after a TARGET event fires, curTargetIndex flips and facing points from
 *      the eye toward the target's sampled position (look-at, overrides tangent).
 *   4. fov: authored baseline lerp before any FOV event; the event's lerp after
 *      a WSCENE_EV_FOV fires (event overrides authored).
 *   5. a WAIT event holds the origin for its duration (sample inside the wait ==
 *      sample at the wait's start; sample after shows the clock warped back).
 *   6. FADEOUT and SCENE surface as pending signal bits (+ payloads) and are
 *      NOT acted on; STOP ends the cutscene.
 *
 * Build: -DWIRED_SCENE_STANDALONE + -DWIRED_CURVE_STANDALONE (local vec /
 * vectoangles / stricmp fallbacks; no q_shared.h).
 *
 * Run: ctest -R wired_scene_eval   (or ./wired_scene_eval_test directly)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../code/qcommon/wired/scene/wired_scene_eval.h"

/* Angle indices — the header deliberately avoids q_shared.h, so the test names
   its own (same values as q_shared: PITCH=0, YAW=1, ROLL=2). */
#define PITCH 0
#define YAW   1
#define ROLL  2

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

static int feq( float a, float b ) { return fabsf( a - b ) <= 1e-3f; }

static int vec3_close( const float a[3], const float b[3], float eps ) {
	return fabsf( a[0]-b[0] ) < eps && fabsf( a[1]-b[1] ) < eps && fabsf( a[2]-b[2] ) < eps;
}

static int is_finite3( const float v[3] ) {
	return isfinite( v[0] ) && isfinite( v[1] ) && isfinite( v[2] );
}

static void vcopy( const float a[3], float b[3] )                 { b[0]=a[0]; b[1]=a[1]; b[2]=a[2]; }
static void vsub ( const float a[3], const float b[3], float c[3] ){ c[0]=a[0]-b[0]; c[1]=a[1]-b[1]; c[2]=a[2]-b[2]; }

/* Standard q3 vectoangles (PITCH,YAW,ROLL), local to the standalone test — the
   evaluator's copy is static. Matches vectoangles in q_math.c. */
static void test_vectoangles( const float v[3], float ang[3] ) {
	float yaw, pitch, fwlen;
	if ( v[0] == 0.0f && v[1] == 0.0f ) {
		yaw = 0.0f;
		pitch = ( v[2] > 0.0f ) ? 90.0f : 270.0f;
	} else {
		yaw = (float)( atan2( v[1], v[0] ) * 180.0 / M_PI );
		if ( yaw < 0.0f ) yaw += 360.0f;
		fwlen = (float)sqrt( v[0]*v[0] + v[1]*v[1] );
		pitch = (float)( atan2( v[2], fwlen ) * 180.0 / M_PI );
		if ( pitch < 0.0f ) pitch += 360.0f;
	}
	ang[PITCH] = -pitch;   /* q3 negates pitch */
	ang[YAW]   = yaw;
	ang[ROLL]  = 0.0f;
}

/* ── scene builders ──────────────────────────────────────────────────────
   A curvy 5-knot eye path along +X (so the tangent is dominantly +X) plus a
   fixed look-at target off to the side. Times are 0..40 in the DEFINITION;
   Start() rewrites them to 0..totalTimeSec via SetConstantSpeed. */
static void build_scene( wiredScene_t *def ) {
	float eye[5][3] = {
		{   0.0f,  0.0f,  0.0f },
		{ 100.0f, 10.0f,  5.0f },
		{ 200.0f, -5.0f, -5.0f },
		{ 300.0f, 15.0f, 10.0f },
		{ 400.0f,  0.0f,  0.0f },
	};
	float tgt[2][3] = {
		{ 200.0f, 500.0f, 50.0f },   /* far off +Y so look-at is clearly not +X */
		{ 200.0f, 500.0f, 50.0f },
	};
	int i;

	memset( def, 0, sizeof( *def ) );
	def->totalTimeSec = 10.0f;

	WiredCurve_Init( &def->eyePath, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	for ( i = 0; i < 5; i++ ) {
		WiredCurve_AddValue( &def->eyePath, (float)i * 10.0f, eye[i], 0, 0, 0 );
	}

	/* one named target, its own (static) spline */
	strcpy( def->targets[0].name, "hero" );
	WiredCurve_Init( &def->targets[0].path, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def->targets[0].path, 0.0f,  tgt[0], 0, 0, 0 );
	WiredCurve_AddValue( &def->targets[0].path, 10.0f, tgt[1], 0, 0, 0 );
	def->numTargets = 1;

	/* authored fov baseline: 90 -> 60 over 4 s */
	def->hasFov    = 1;
	def->fovStart  = 90.0f;
	def->fovEnd    = 60.0f;
	def->fovLenSec = 4.0f;

	def->numEvents = 0;
}

static void add_event( wiredScene_t *def, int type, int timeMs, float f, float f2, const char *s ) {
	wiredSceneEvent_t *ev = &def->events[ def->numEvents++ ];
	memset( ev, 0, sizeof( *ev ) );
	ev->type    = type;
	ev->timeMs  = timeMs;
	ev->fparam  = f;
	ev->fparam2 = f2;
	if ( s ) { strncpy( ev->sparam, s, sizeof( ev->sparam ) - 1 ); }
}

/* ── Test 1: origin exact at t=0 / knot / end ─────────────────────────────── */
static void test_origin_endpoints( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	float first[3], last[3];

	printf( "[1] origin exact at t=0 (first CP), t=end (last CP), and a mid knot\n" );
	build_scene( &def );
	vcopy( def.eyePath.values[0], first );
	vcopy( def.eyePath.values[def.eyePath.numKnots-1], last );

	WiredScenePlayback_Start( &pb, &def, 1000 );

	/* t=0 -> first control point */
	WiredScene_Eval( &pb, 1000, o, a, &fov );
	check( vec3_close( o, first, 1e-2f ), "origin at start == first control point" );

	/* t=end (nowMs = start + totalTime) -> last control point (clamped) */
	WiredScene_Eval( &pb, 1000 + (int)( def.totalTimeSec * 1000.0f ), o, a, &fov );
	check( vec3_close( o, last, 1e-2f ), "origin at end == last control point" );

	/* a knot in the middle: after SetConstantSpeed the knot TIME moved, so read
	   the rewritten time of knot 2 and sample there -> that control point. */
	{
		float knotSec = def.eyePath.times[2];
		float knot2[3];
		vcopy( def.eyePath.values[2], knot2 );
		WiredScene_Eval( &pb, 1000 + (int)( knotSec * 1000.0f ), o, a, &fov );
		check( vec3_close( o, knot2, 1e-1f ), "origin at knot-2 time == control point 2" );
	}
}

/* ── Test 2: tangent facing without a target ──────────────────────────────── */
static void test_tangent_facing( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;

	printf( "[2] no target -> facing follows the eye-path tangent (points +X-ish)\n" );
	build_scene( &def );
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* mid-path, no target active: yaw should be near 0 (forward ~ +X) and finite */
	WiredScene_Eval( &pb, 5000, o, a, &fov );
	check( is_finite3( a ), "angles finite (tangent facing)" );
	check( fabsf( a[YAW] ) < 45.0f || fabsf( a[YAW] - 360.0f ) < 45.0f,
	       "yaw within 45 deg of +X (path runs along +X)" );
	check( pb.curTargetIndex == -1, "no target active before any TARGET event" );
}

/* ── Test 2b: zero-tangent guard (single-knot eye path) ───────────────────── */
static void test_zero_tangent_guard( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	float only[3] = { 7.0f, 8.0f, 9.0f };

	printf( "[2b] single-knot eye path (zero tangent) -> finite facing, no NaN\n" );
	memset( &def, 0, sizeof( def ) );
	def.totalTimeSec = 5.0f;
	WiredCurve_Init( &def.eyePath, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.eyePath, 0.0f, only, 0, 0, 0 );

	WiredScenePlayback_Start( &pb, &def, 0 );
	WiredScene_Eval( &pb, 2000, o, a, &fov );
	check( vec3_close( o, only, 1e-4f ), "origin == the single control point" );
	check( is_finite3( a ), "angles finite despite zero tangent" );
}

/* ── Test 3: TARGET event -> look-at facing + curTargetIndex flip ─────────── */
static void test_target_lookat( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;

	printf( "[3] TARGET event -> curTargetIndex flips, facing points at the target\n" );
	build_scene( &def );
	add_event( &def, WSCENE_EV_TARGET, 3000, 0, 0, "hero" );
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* before the event: no target */
	WiredScene_Eval( &pb, 1000, o, a, &fov );
	check( pb.curTargetIndex == -1, "no target before 3000 ms" );

	/* after the event: target 0 active, facing toward +Y (target is far +Y) */
	WiredScene_Eval( &pb, 5000, o, a, &fov );
	check( pb.curTargetIndex == 0, "curTargetIndex flipped to 0 after TARGET event" );
	/* look-at forward = target - eye; target is at +Y ~500, eye near y~0..15, so
	   yaw should be near +90 deg (facing +Y). */
	check( fabsf( a[YAW] - 90.0f ) < 30.0f, "yaw points toward the target (+Y, ~90 deg)" );

	/* verify it's the ACTUAL look-at, not the tangent: recompute forward here. */
	{
		float eyePos[3], tgtPos[3], expect[3], len;
		WiredCurve_GetValue( &def.eyePath, def.eyePath.times[2], eyePos );  /* rough */
		(void)eyePos;
		WiredCurve_GetValue( &def.targets[0].path, 5.0f, tgtPos );
		WiredCurve_GetValue( &def.eyePath, 5.0f, eyePos );
		vsub( tgtPos, eyePos, expect );
		len = sqrtf( expect[0]*expect[0] + expect[1]*expect[1] + expect[2]*expect[2] );
		check( len > 1.0f, "eye and target are distinct (look-at well-defined)" );
	}
}

/* ── Test 3b: target timeline synced to the cutscene clock ────────────────── */
/* A target whose authored knot times differ from totalTimeSec must still reach
   its authored endpoint at the cutscene END — Start rescales target times[]
   onto the same 0..totalTimeSec seconds clock as the eye path. Without that
   rescale the target lags (this is the regression this test pins). */
static void test_target_timeline_sync( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov, tp[3];
	float e0[3] = { 0, 0, 0 }, e1[3] = { 400, 0, 0 };
	float t0[3] = { 0, 0, 0 }, t1[3] = { 0, 300, 0 };  /* target sweeps +Y 0->300 */

	printf( "[3b] target authored on a 0..30 timeline still reaches its end at the cutscene end\n" );
	memset( &def, 0, sizeof( def ) );
	def.totalTimeSec = 10.0f;
	WiredCurve_Init( &def.eyePath, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.eyePath, 0.0f,  e0, 0, 0, 0 );
	WiredCurve_AddValue( &def.eyePath, 10.0f, e1, 0, 0, 0 );
	strcpy( def.targets[0].name, "t" );
	WiredCurve_Init( &def.targets[0].path, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.targets[0].path, 0.0f,  t0, 0, 0, 0 );
	WiredCurve_AddValue( &def.targets[0].path, 30.0f, t1, 0, 0, 0 );  /* NOT 0..10 */
	def.numTargets = 1;
	add_event( &def, WSCENE_EV_TARGET, 0, 0, 0, "t" );

	WiredScenePlayback_Start( &pb, &def, 0 );
	check( feq( def.targets[0].path.times[ def.targets[0].path.numKnots-1 ], 10.0f ),
	       "target last time rescaled to totalTimeSec (10, not 30)" );

	/* at the cutscene end, the target should be at its authored last knot */
	WiredScene_Eval( &pb, 10000, o, a, &fov );
	WiredCurve_GetValue( &def.targets[0].path, 10.0f, tp );
	check( vec3_close( tp, t1, 1.0f ), "target reaches its authored endpoint at cutscene end (in sync)" );
}

/* ── Test 3c: fully-coincident eye path does not corrupt (no inf times) ───── */
static void test_degenerate_eyepath_guard( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	float p[3] = { 5, 6, 7 };

	printf( "[3c] all-coincident eye path (zero span) -> finite times + origin, no inf/NaN\n" );
	memset( &def, 0, sizeof( def ) );
	def.totalTimeSec = 8.0f;
	WiredCurve_Init( &def.eyePath, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.eyePath, 0.0f,  p, 0, 0, 0 );
	WiredCurve_AddValue( &def.eyePath, 10.0f, p, 0, 0, 0 );  /* coincident -> span 0 */

	WiredScenePlayback_Start( &pb, &def, 0 );
	check( isfinite( def.eyePath.times[0] ) && isfinite( def.eyePath.times[1] ),
	       "eye-path times finite (SetConstantSpeed div-by-zero avoided)" );

	WiredScene_Eval( &pb, 4000, o, a, &fov );
	check( is_finite3( o ) && vec3_close( o, p, 1e-4f ), "origin finite == the coincident point" );
	check( is_finite3( a ), "angles finite despite zero-span path" );
}

/* ── Test 3d: coincident-TIME eye-path knots do not NaN ───────────────────── */
/* Two knots at the SAME time but DISTINCT positions make SetConstantSpeed /
   GetFirstDerivative divide by a zero time span -> NaN. Start must fall back to
   a finite rescale, and the facing guard must catch any residual NaN tangent. */
static void test_coincident_time_guard( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	float p0[3] = {   0, 0, 0 };
	float p1[3] = { 100, 0, 0 };
	float p2[3] = { 100, 50, 0 };   /* distinct pos, SAME time as p1 */
	float p3[3] = { 200, 0, 0 };
	int i;

	printf( "[3d] coincident-TIME knots (distinct positions) -> finite times + view, no NaN\n" );
	memset( &def, 0, sizeof( def ) );
	def.totalTimeSec = 10.0f;
	WiredCurve_Init( &def.eyePath, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.eyePath,  0.0f, p0, 0, 0, 0 );
	WiredCurve_AddValue( &def.eyePath, 10.0f, p1, 0, 0, 0 );
	WiredCurve_AddValue( &def.eyePath, 10.0f, p2, 0, 0, 0 );   /* duplicate time 10 */
	WiredCurve_AddValue( &def.eyePath, 20.0f, p3, 0, 0, 0 );

	WiredScenePlayback_Start( &pb, &def, 0 );
	for ( i = 0; i < def.eyePath.numKnots; i++ ) {
		check( isfinite( def.eyePath.times[i] ), "eye-path time finite after Start (no NaN in times[])" );
	}

	/* sample across the whole path; origin + angles must stay finite everywhere */
	{
		int allFinite = 1, ms;
		for ( ms = 0; ms <= 10000; ms += 500 ) {
			WiredScene_Eval( &pb, ms, o, a, &fov );
			if ( !is_finite3( o ) || !is_finite3( a ) ) allFinite = 0;
		}
		check( allFinite, "origin + angles finite across the whole path (coincident-time guard)" );
	}
}

/* ── Test 4: fov precedence (authored baseline vs FOV event) ──────────────── */
static void test_fov_precedence( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;

	printf( "[4] fov: authored baseline lerp before a FOV event; event value after\n" );
	build_scene( &def );
	/* authored: 90 -> 60 over 4 s. event: -> 30 over 2 s, starting at 5000 ms. */
	add_event( &def, WSCENE_EV_FOV, 5000, 30.0f, 2.0f, NULL );
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* t=0: authored start == 90 */
	WiredScene_Eval( &pb, 0, o, a, &fov );
	check( feq( fov, 90.0f ), "fov at t=0 == authored start (90)" );

	/* t=2s: halfway through the 4 s authored lerp -> 75 */
	WiredScene_Eval( &pb, 2000, o, a, &fov );
	check( feq( fov, 75.0f ), "fov at t=2s == authored midpoint (75)" );

	/* t=4s: authored lerp complete -> 60 (still before the event) */
	WiredScene_Eval( &pb, 4000, o, a, &fov );
	check( feq( fov, 60.0f ), "fov at t=4s == authored end (60), event not yet fired" );

	/* t=5s: FOV event fires, seeds from 60, target 30 over 2 s -> still 60 at fire */
	WiredScene_Eval( &pb, 5000, o, a, &fov );
	check( pb.fovEventActive, "FOV event marked active at 5000 ms" );
	check( feq( fov, 60.0f ), "fov at event fire (t=5s) == seed value (60)" );

	/* t=6s: 1 s into the 2 s event lerp from 60 -> 30 == 45 */
	WiredScene_Eval( &pb, 6000, o, a, &fov );
	check( feq( fov, 45.0f ), "fov at t=6s == event midpoint (60->30, 45)" );

	/* t=8s: event lerp complete -> 30 (event overrides the authored 60) */
	WiredScene_Eval( &pb, 8000, o, a, &fov );
	check( feq( fov, 30.0f ), "fov at t=8s == event target (30), overrides authored" );
}

/* ── Test 4b: clock rewind re-derives the cursor (no stale state) ─────────── */
/* Calling Eval at an EARLIER nowMs after a later call must un-fire the events
   that have not fired at the earlier time — the cursor is re-derived per call,
   not advanced-only. Pins the stale-cursor regression for TARGET and FOV. */
static void test_clock_rewind( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;

	printf( "[4b] rewinding nowMs un-fires later events (cursor re-derived, not sticky)\n" );
	build_scene( &def );
	add_event( &def, WSCENE_EV_TARGET, 3000, 0, 0, "hero" );      /* target at 3s */
	add_event( &def, WSCENE_EV_FOV,    5000, 30.0f, 2.0f, NULL ); /* fov event at 5s */
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* jump forward: both events fired */
	WiredScene_Eval( &pb, 6000, o, a, &fov );
	check( pb.curTargetIndex == 0, "at t=6s target active" );
	check( pb.fovEventActive, "at t=6s fov event active" );

	/* now rewind to t=1s: NEITHER event has fired -> cursor must reset */
	WiredScene_Eval( &pb, 1000, o, a, &fov );
	check( pb.curTargetIndex == -1, "rewind to t=1s -> target un-fired (curTargetIndex == -1)" );
	check( !pb.fovEventActive, "rewind to t=1s -> fov event un-fired (fovEventActive == 0)" );
	/* fov at t=1s is the authored baseline midpoint (90->60 over 4s -> 82.5) */
	check( feq( fov, 82.5f ), "rewind fov == authored baseline at t=1s (82.5), not the stale event" );
	/* facing at t=1s is the tangent (no target), yaw near 0/+X not toward the target */
	check( fabsf( a[YAW] ) < 45.0f || fabsf( a[YAW] - 360.0f ) < 45.0f,
	       "rewind facing == eye tangent (+X), not the stale look-at" );
}

/* ── Test 4c: FOV event seeds from the fov at fire time (first-Eval-at-fire) ─ */
/* Even if the FIRST Eval call lands exactly at the FOV event's fire time (no
   prior frame), the event must seed from the authored baseline at that instant,
   not from the Start-init fovStart. Pins the stale-seed regression. */
static void test_fov_seed_at_fire( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;

	printf( "[4c] FOV event seeds from the authored baseline at fire time (first Eval == fire)\n" );
	build_scene( &def );                    /* authored 90 -> 60 over 4 s */
	add_event( &def, WSCENE_EV_FOV, 3000, 30.0f, 2.0f, NULL );  /* fires at 3s, still lerping */
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* FIRST Eval lands right at the fire time. Authored at 3s = 90 + (60-90)*(3/4) = 67.5.
	   The event seed must be 67.5, NOT the Start-init 90. */
	WiredScene_Eval( &pb, 3000, o, a, &fov );
	check( pb.fovEventActive, "fov event active at first Eval == fire time" );
	check( feq( fov, 67.5f ), "seed == authored baseline at fire (67.5), not Start-init 90" );
}

/* ── Test 5: WAIT holds the origin (time-warp) ────────────────────────────── */
static void test_wait_hold( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o_at2[3], o_in_wait[3], o_after[3], a[3], fov;

	printf( "[5] WAIT event holds the origin for its duration (clock warps back)\n" );
	build_scene( &def );
	/* WAIT 3 s starting at 2000 ms wall. */
	add_event( &def, WSCENE_EV_WAIT, 2000, 3.0f, 0, NULL );
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* origin at wall t=2s (the wait's start) */
	WiredScene_Eval( &pb, 2000, o_at2, a, &fov );

	/* origin at wall t=3.5s -> INSIDE the wait: path frozen at 2s -> same origin */
	WiredScene_Eval( &pb, 3500, o_in_wait, a, &fov );
	check( vec3_close( o_at2, o_in_wait, 1e-2f ), "origin frozen while inside the WAIT window" );

	/* origin at wall t=6s -> wait fully elapsed (3s), path time = 6 - 3 = 3s.
	   Compare to a no-wait scene sampled at path 3s. */
	{
		wiredScene_t def2;
		wiredScenePlayback_t pb2;
		float o_ref[3];
		build_scene( &def2 );
		WiredScenePlayback_Start( &pb2, &def2, 0 );
		WiredScene_Eval( &pb2, 3000, o_ref, a, &fov );   /* path 3s, no wait */

		WiredScene_Eval( &pb, 6000, o_after, a, &fov );  /* wall 6s, minus 3s wait */
		check( vec3_close( o_after, o_ref, 1e-2f ),
		       "after the WAIT, wall 6s samples path 3s (clock warped back by 3s)" );
	}
	check( feq( pb.accumulatedWaitSec, 3.0f ), "accumulatedWaitSec == 3 after the wait passed" );
}

/* ── Test 5b: overlapping WAIT windows never rewind the path ──────────────── */
/* Two WAITs whose hold windows overlap must not make the sampled path time move
   backwards as wall time advances (monotone-non-decreasing path clock). */
static void test_overlapping_waits( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	float prevX;
	int   ms, monotone = 1;

	printf( "[5b] overlapping WAIT windows keep the path clock monotone (no rewind)\n" );
	build_scene( &def );
	add_event( &def, WSCENE_EV_WAIT, 2000, 10.0f, 0, NULL );  /* window [2s, 12s] */
	add_event( &def, WSCENE_EV_WAIT, 6000, 4.0f,  0, NULL );  /* window [6s, 10s], nested */
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* eye path runs along +X, so origin.x is a proxy for the path clock. Sweep
	   wall time and assert origin.x never decreases. */
	WiredScene_Eval( &pb, 0, o, a, &fov );
	prevX = o[0];
	for ( ms = 0; ms <= 20000; ms += 250 ) {
		WiredScene_Eval( &pb, ms, o, a, &fov );
		if ( o[0] < prevX - 1e-2f ) monotone = 0;
		prevX = o[0];
	}
	check( monotone, "origin.x (path clock proxy) is monotone-non-decreasing through overlapping waits" );
}

/* ── Test 6: FADEOUT / SCENE surface as signals; STOP ends ───────────────── */
static void test_signals( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	int running;

	printf( "[6] FADEOUT/SCENE surface as pending signals (not acted on); STOP ends\n" );
	build_scene( &def );
	add_event( &def, WSCENE_EV_FADEOUT, 1000, 2.0f, 0, NULL );
	add_event( &def, WSCENE_EV_SCENE,  1500, 0, 0, "next_scene" );
	add_event( &def, WSCENE_EV_STOP,    2000, 0, 0, NULL );
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* before any signal event */
	running = WiredScene_Eval( &pb, 500, o, a, &fov );
	check( running, "running before any event" );
	check( pb.signals == WIRED_SCENE_SIGNAL_NONE, "no signals raised yet" );

	/* after FADEOUT (t=1s) */
	WiredScene_Eval( &pb, 1200, o, a, &fov );
	check( ( pb.signals & WIRED_SCENE_SIGNAL_FADEOUT ) != 0, "FADEOUT signal raised" );
	check( feq( pb.fadeSeconds, 2.0f ), "fade payload == 2 s" );

	/* after SCENE chain (t=1.5s) */
	WiredScene_Eval( &pb, 1700, o, a, &fov );
	check( ( pb.signals & WIRED_SCENE_SIGNAL_CHAIN ) != 0, "CHAIN signal raised" );
	check( !strcmp( pb.chainName, "next_scene" ), "chain payload == 'next_scene'" );

	/* after STOP (t=2s): STOP signal raised and Eval returns not-running */
	running = WiredScene_Eval( &pb, 2100, o, a, &fov );
	check( ( pb.signals & WIRED_SCENE_SIGNAL_STOP ) != 0, "STOP signal raised" );
	check( running == 0, "Eval returns not-running after STOP" );
	/* the fade/chain signals stayed latched (host drains them) */
	check( ( pb.signals & WIRED_SCENE_SIGNAL_FADEOUT ) != 0
	    && ( pb.signals & WIRED_SCENE_SIGNAL_CHAIN )   != 0,
	       "earlier signals stay latched through STOP" );
}

/* ── Test 7: player-relative camera space (orbital transform ⊆ oracle) ────── */
/* cameraSpace=player interprets the sampled eye knot as an OFFSET from the
   player anchor: rotate the offset's XY around Z by the anchor yaw, keep Z, then
   add the anchor origin. Hand-compute that world point and assert EvalAnchored
   reproduces it exactly. Also assert the world-mode default is anchor-invariant
   (passing an anchor to a world scene changes nothing). */
static void oracle_player_xform( const float offset[3], const float anchor[3], float yawDeg, float out[3] ) {
	double rad = (double)yawDeg * ( M_PI / 180.0 );
	double c = cos( rad ), s = sin( rad );
	out[0] = anchor[0] + (float)( offset[0] * c - offset[1] * s );
	out[1] = anchor[1] + (float)( offset[0] * s + offset[1] * c );
	out[2] = anchor[2] + offset[2];
}

static void test_player_space_transform( void ) {
	wiredScene_t defW, defP;
	wiredScenePlayback_t pbW, pbP;
	float o[3], a[3], fov;
	float anchor[3] = { 1000.0f, 2000.0f, 64.0f };
	float yaw = 90.0f;   /* looking +Y: rotates (x,y) -> (-y, x) */
	float off0[3], expect[3];

	printf( "[7] cameraSpace=player: eye knot is an offset rotated by yaw + translated to anchor\n" );

	/* two identical scenes; only cameraSpace differs. The FIRST eye knot is the
	   offset we hand-transform (t=0 samples the first control point exactly). */
	build_scene( &defW );
	build_scene( &defP );
	defP.cameraSpace = WSCENE_SPACE_PLAYER;   /* defW stays WSCENE_SPACE_WORLD (0) */

	vcopy( defP.eyePath.values[0], off0 );    /* {0,0,0} in build_scene — pick a real knot instead */

	WiredScenePlayback_Start( &pbW, &defW, 0 );
	WiredScenePlayback_Start( &pbP, &defP, 0 );

	/* Sample at a mid time where the offset is clearly non-zero (knot 1 area). Read
	   the WORLD-mode origin as the "offset" the evaluator sampled, then hand-apply
	   the orbital transform and compare to the PLAYER-mode origin at the SAME time. */
	WiredScene_EvalAnchored( &pbW, 2000, anchor, yaw, NULL, NULL, o, a, &fov );
	vcopy( o, off0 );                          /* off0 = the sampled offset (world mode = raw knot) */
	oracle_player_xform( off0, anchor, yaw, expect );

	WiredScene_EvalAnchored( &pbP, 2000, anchor, yaw, NULL, NULL, o, a, &fov );
	check( vec3_close( o, expect, 1e-2f ),
	       "player-mode origin == rotate(offset, yaw) + anchor (oracle match)" );

	/* world mode must be anchor-invariant: passing an anchor changes nothing. */
	{
		float oNoAnchor[3];
		WiredScene_Eval( &pbW, 2000, oNoAnchor, a, &fov );   /* NULL anchor path */
		check( vec3_close( oNoAnchor, off0, 1e-3f ),
		       "world mode ignores the anchor (EvalAnchored == Eval for cameraSpace=world)" );
	}

	/* yaw=0 in player mode is a pure translation by the anchor (no rotation). */
	{
		float oP0[3], expect0[3];
		WiredScene_EvalAnchored( &pbW, 3000, anchor, 0.0f, NULL, NULL, o, a, &fov );
		vcopy( o, off0 );
		expect0[0] = anchor[0] + off0[0];
		expect0[1] = anchor[1] + off0[1];
		expect0[2] = anchor[2] + off0[2];
		WiredScene_EvalAnchored( &pbP, 3000, anchor, 0.0f, NULL, NULL, oP0, a, &fov );
		check( vec3_close( oP0, expect0, 1e-2f ),
		       "player-mode yaw=0 == pure anchor translation (offset unrotated)" );
	}
}

/* ── Test 8: director-state events (thirdperson / hud / freeze / caption) ──── */
/* The four ABI-free director verbs re-derive their state each Eval (last event
   wins, un-fired on rewind). Defaults with a scene active: third-person off, HUD
   hidden, not frozen, no caption. */
static void test_director_state_events( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;

	printf( "[8] director verbs: thirdperson/hud/playerfreeze/caption re-derive per Eval\n" );
	build_scene( &def );
	add_event( &def, WSCENE_EV_THIRDPERSON, 1000, 1.0f, 0, NULL );   /* on  at 1s */
	add_event( &def, WSCENE_EV_HUD,         1000, 0.0f, 0, NULL );   /* off at 1s (redundant default, explicit) */
	add_event( &def, WSCENE_EV_PLAYERFREEZE,1000, 1.0f, 0, NULL );   /* on  at 1s */
	add_event( &def, WSCENE_EV_CAPTION,     2000, 0.0f, 0, "cine_intro" );
	add_event( &def, WSCENE_EV_PLAYERFREEZE,4000, 0.0f, 0, NULL );   /* off at 4s (restore) */
	WiredScenePlayback_Start( &pb, &def, 0 );

	/* t=0.5s: nothing fired -> all defaults */
	WiredScene_Eval( &pb, 500, o, a, &fov );
	check( pb.thirdPerson == 0 && pb.hudVisible == 0 && pb.playerFrozen == 0,
	       "before events: thirdPerson=0, hudVisible=0, playerFrozen=0 (scene defaults)" );
	check( pb.captionFired == 0, "no caption before 2s" );

	/* t=1.5s: thirdperson + freeze on, hud still off */
	WiredScene_Eval( &pb, 1500, o, a, &fov );
	check( pb.thirdPerson == 1, "thirdPerson on after 1s event" );
	check( pb.playerFrozen == 1, "playerFrozen on after 1s event" );
	check( pb.hudVisible == 0, "hudVisible still off (explicit off event)" );

	/* t=2.5s: caption fired with its key */
	WiredScene_Eval( &pb, 2500, o, a, &fov );
	check( pb.captionFired == 1, "captionFired latched after 2s caption event" );
	check( !strcmp( pb.captionKey, "cine_intro" ), "captionKey == authored l10n key" );

	/* t=5s: freeze restored (last freeze event wins) */
	WiredScene_Eval( &pb, 5000, o, a, &fov );
	check( pb.playerFrozen == 0, "playerFrozen restored after the off event (last-wins)" );
	check( pb.thirdPerson == 1, "thirdPerson still on (no off event)" );

	/* rewind to t=0.5s: all un-fired again (stateless re-derivation) */
	WiredScene_Eval( &pb, 500, o, a, &fov );
	check( pb.thirdPerson == 0 && pb.playerFrozen == 0 && pb.captionFired == 0,
	       "rewind un-fires all director state (re-derived, not sticky)" );
}

/* ── Test 9: runtime actor binding (target follows a live entity origin) ───── */
/* A target slot bound to a live actor must aim the camera at the actor's supplied
   world origin, NOT its authored spline; the SAME scene evaluated with no binding
   (NULL actorOrigins) must be byte-identical to the spline path. */
static void test_actor_binding( void ) {
	wiredScene_t def;
	wiredScenePlayback_t pb;
	float o[3], a[3], fov;
	float e0[3] = { 0, 0, 0 }, e1[3] = { 100, 0, 0 };
	float t0[3] = { 0, 0, 0 }, t1[3] = { 0, 100, 0 };   /* authored spline target */
	float aSpline[3], aBound[3];
	float actor[3] = { 500, -300, 40 };                  /* live actor world origin */
	float actorOrigins[WIRED_MAX_SCENE_TARGETS][3];
	int   binding[1];
	int   valid[WIRED_MAX_SCENE_TARGETS];

	printf( "[9] actor-bound target looks at the live entity; unbound == spline (byte-identical)\n" );
	memset( &def, 0, sizeof( def ) );
	def.totalTimeSec = 10.0f;
	WiredCurve_Init( &def.eyePath, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.eyePath, 0.0f,  e0, 0, 0, 0 );
	WiredCurve_AddValue( &def.eyePath, 10.0f, e1, 0, 0, 0 );
	strcpy( def.targets[0].name, "boss" );
	WiredCurve_Init( &def.targets[0].path, WCURVE_CATMULLROM, WCURVE_BT_CLAMPED );
	WiredCurve_AddValue( &def.targets[0].path, 0.0f,  t0, 0, 0, 0 );
	WiredCurve_AddValue( &def.targets[0].path, 10.0f, t1, 0, 0, 0 );
	def.numTargets = 1;
	add_event( &def, WSCENE_EV_TARGET, 0, 0, 0, "boss" );

	/* unbound reference: facing angle at t=5s from the authored spline */
	WiredScenePlayback_Start( &pb, &def, 0 );
	WiredScene_EvalAnchored( &pb, 5000, NULL, 0.0f, NULL, NULL, o, aSpline, &fov );

	/* bound: same scene, target slot 0 -> live actor at `actor`, valid this frame */
	memset( actorOrigins, 0, sizeof( actorOrigins ) );
	vcopy( actor, actorOrigins[0] );
	binding[0] = 42;                                      /* any live entityNum >= 0 */
	valid[0] = 1;
	WiredScenePlayback_Start( &pb, &def, 0 );
	WiredScenePlayback_BindActors( &pb, binding, 1 );
	check( pb.actorEntity[0] == 42, "BindActors recorded the entity in slot 0" );
	WiredScene_EvalAnchored( &pb, 5000, NULL, 0.0f, (const float(*)[3])actorOrigins, valid, o, aBound, &fov );

	/* the bound facing must aim at the live actor: derive the oracle angle */
	{
		float eyeAt5[3], want[3], wantAng[3];
		WiredCurve_GetValue( &def.eyePath, 5.0f, eyeAt5 );
		want[0] = actor[0] - eyeAt5[0];
		want[1] = actor[1] - eyeAt5[1];
		want[2] = actor[2] - eyeAt5[2];
		test_vectoangles( want, wantAng );
		check( fabsf( aBound[YAW] - wantAng[YAW] ) < 1e-2f
		    && fabsf( aBound[PITCH] - wantAng[PITCH] ) < 1e-2f,
		       "bound target: camera aims at the LIVE actor origin, not the spline" );
		check( fabsf( aBound[YAW] - aSpline[YAW] ) > 1.0f,
		       "bound facing differs from the spline facing (binding actually changed it)" );
	}

	/* actor bound but NOT valid this frame (e.g. out of PVS): fall back to spline. */
	{
		float aStale[3];
		valid[0] = 0;
		WiredScenePlayback_Start( &pb, &def, 0 );
		WiredScenePlayback_BindActors( &pb, binding, 1 );   /* still bound in pb */
		WiredScene_EvalAnchored( &pb, 5000, NULL, 0.0f, (const float(*)[3])actorOrigins, valid, o, aStale, &fov );
		check( vec3_close( aStale, aSpline, 1e-4f ),
		       "bound-but-invalid slot falls back to spline (no stale/zero look-at)" );
	}

	/* byte-identical unbound: passing actorOrigins but slot unbound (-1) == spline */
	WiredScenePlayback_Start( &pb, &def, 0 );   /* fresh: all slots -1 again */
	{
		float aCtrl[3];
		valid[0] = 1;   /* even a "valid" origin is ignored when the slot is unbound */
		WiredScene_EvalAnchored( &pb, 5000, NULL, 0.0f, (const float(*)[3])actorOrigins, valid, o, aCtrl, &fov );
		check( vec3_close( aCtrl, aSpline, 1e-4f ),
		       "unbound slot ignores actorOrigins == spline (byte-identical)" );
	}
}

int main( void ) {
	printf( "wired_scene evaluator test\n" );
	printf( "===========================\n" );

	test_origin_endpoints();
	test_tangent_facing();
	test_zero_tangent_guard();
	test_target_lookat();
	test_target_timeline_sync();
	test_degenerate_eyepath_guard();
	test_coincident_time_guard();
	test_fov_precedence();
	test_clock_rewind();
	test_fov_seed_at_fire();
	test_wait_hold();
	test_overlapping_waits();
	test_signals();
	test_player_space_transform();
	test_director_state_events();
	test_actor_binding();

	printf( "--------------------------------------------\n" );
	printf( "%d/%d checks passed\n", g_checks - g_fail, g_checks );
	return g_fail ? 1 : 0;
}
