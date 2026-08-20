// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Contract for the particle curve table and parameter evaluator.
//
// WHAT IS ACTUALLY AT RISK HERE
// A particleParm_t decides what a particle looks like over its life, and it
// is evaluated TWICE: once here in C (authoring, preview, tests) and once in
// GLSL (the shader that actually draws). Nothing at runtime compares the two.
// If they drift, the failure is silent and cosmetic-looking — an effect
// renders subtly differently from what was authored, no crash, no log line,
// and the difference is easiest to notice long after the change that caused
// it. So the arithmetic is pinned here numerically, and the GLSL mirror is
// pinned against these same numbers.
//
// The second risk is the override rule. Every existing class is memset-zero
// in its new parm fields, and MUST keep rendering exactly as before. That
// makes "is this parm authored?" a compatibility contract, not a detail: get
// it wrong and every shipped effect changes at once.
//
// Compiles the EXACT production translation unit — no stubs, no copy of the
// maths. That is the whole reason particle_curve.c was split out of the cgame
// registry: a test that reimplements the thing it checks proves only that two
// wrongs agree.

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/wired/render/particle_class.h"

static int failures;

static void Check( const char *what, int ok ) {
	if ( !ok ) {
		printf( "  FAIL %s\n", what );
		failures++;
	} else {
		printf( "  ok   %s\n", what );
	}
}

// Floating-point comparison with a tolerance loose enough for float
// arithmetic and tight enough that a wrong formula cannot hide: 1e-5 is
// ~5 significant digits, while every defect this test targets (off-by-one
// in the sample span, a missing lerp, a dropped multiply) moves the result
// by whole percent.
static void CheckNear( const char *what, float got, float want ) {
	if ( fabsf( got - want ) > 1e-5f ) {
		printf( "  FAIL %-56s got %.6f, want %.6f\n", what, got, want );
		failures++;
	} else {
		printf( "  ok   %-56s (%.4f)\n", what, got );
	}
}

int main( void ) {
	particleParm_t parm;
	float          ramp[PARTICLE_CURVE_SAMPLES];
	float          spike[PARTICLE_CURVE_SAMPLES];
	int            i, idxRamp, idxSpike, again;

	printf( "particle curve table + parm evaluator contract\n" );

	// ── 1. an unauthored parm is inert ──────────────────────────────────
	// This is the compatibility contract. Every class that predates curves
	// is memset-zero here, so a zeroed parm must read as "not authored" and
	// evaluate to nothing. If this ever flips, every shipped effect changes.
	memset( &parm, 0, sizeof( parm ) );
	Check( "memset-zero parm reads as unset", ParticleParm_IsUnset( &parm ) );
	CheckNear( "unset parm evaluates to 0 at fraction 0", ParticleParm_Eval( &parm, 0.0f, 0.0f ), 0.0f );
	CheckNear( "unset parm evaluates to 0 at fraction 1", ParticleParm_Eval( &parm, 1.0f, 0.0f ), 0.0f );
	Check( "NULL parm reads as unset", ParticleParm_IsUnset( NULL ) );

	// A constant with a real value IS authored — otherwise "hold at 5"
	// would be indistinguishable from "not set" and silently fall back to
	// the old scalar field.
	memset( &parm, 0, sizeof( parm ) );
	parm.calc = PARM_CONSTANT;
	parm.val0 = 5.0f;
	Check( "non-zero constant reads as authored", !ParticleParm_IsUnset( &parm ) );
	CheckNear( "constant ignores fraction (0.0)", ParticleParm_Eval( &parm, 0.0f, 0.0f ), 5.0f );
	CheckNear( "constant ignores fraction (0.5)", ParticleParm_Eval( &parm, 0.5f, 0.0f ), 5.0f );
	CheckNear( "constant ignores fraction (1.0)", ParticleParm_Eval( &parm, 1.0f, 0.0f ), 5.0f );

	// ── 2. linear ───────────────────────────────────────────────────────
	memset( &parm, 0, sizeof( parm ) );
	parm.calc = PARM_LINEAR;
	parm.val0 = 10.0f;
	parm.val1 = 20.0f;
	CheckNear( "linear at 0 is val0",      ParticleParm_Eval( &parm, 0.0f,  0.0f ), 10.0f );
	CheckNear( "linear at 1 is val1",      ParticleParm_Eval( &parm, 1.0f,  0.0f ), 20.0f );
	CheckNear( "linear at 0.5 is midpoint",ParticleParm_Eval( &parm, 0.5f,  0.0f ), 15.0f );
	// Fractions outside [0,1] are clamped rather than extrapolated: a
	// particle one frame past its lifetime must not shoot to a wild value.
	CheckNear( "fraction below 0 clamps",  ParticleParm_Eval( &parm, -1.0f, 0.0f ), 10.0f );
	CheckNear( "fraction above 1 clamps",  ParticleParm_Eval( &parm,  2.0f, 0.0f ), 20.0f );

	// ── 3. the shared curve table ───────────────────────────────────────
	// Slot 0 is reserved so a zeroed parm cannot accidentally point at a
	// real curve. Everything else depends on that.
	Check( "curve index 0 is reserved", CG_GetParticleCurve( 0 ) == NULL );

	for ( i = 0; i < PARTICLE_CURVE_SAMPLES; i++ )
		ramp[i] = (float)i / (float)( PARTICLE_CURVE_SAMPLES - 1 );   // 0 → 1
	idxRamp = CG_RegisterParticleCurve( "ramp", ramp );
	Check( "registering a curve returns a non-zero index", idxRamp > 0 );
	Check( "registered curve is retrievable", CG_GetParticleCurve( idxRamp ) != NULL );
	Check( "curve is found by name", CG_FindParticleCurve( "ramp" ) == idxRamp );

	// Dedup is what makes the table SHARED rather than merely central: two
	// classes asking for the same curve must land on one entry, so editing
	// it later reaches both.
	again = CG_RegisterParticleCurve( "ramp", ramp );
	Check( "re-registering the same name returns the same index", again == idxRamp );

	Check( "unknown name is not found", CG_FindParticleCurve( "nope" ) == 0 );
	Check( "NULL name is rejected", CG_RegisterParticleCurve( NULL, ramp ) == 0 );
	Check( "empty name is rejected", CG_RegisterParticleCurve( "", ramp ) == 0 );
	Check( "NULL samples are rejected", CG_RegisterParticleCurve( "x", NULL ) == 0 );

	// ── 4. curve sampling ───────────────────────────────────────────────
	// The samples span 0..1 INCLUSIVE, so the last sample sits exactly at
	// fraction 1 and the span is (N-1) intervals, not N. An off-by-one here
	// shifts every curve slightly — visible in motion, invisible in review.
	memset( &parm, 0, sizeof( parm ) );
	parm.calc  = PARM_CURVE;
	parm.curve = idxRamp;
	parm.val0  = 0.0f;
	parm.val1  = 100.0f;
	CheckNear( "curve at 0 samples the first entry",  ParticleParm_Eval( &parm, 0.0f, 0.0f ),   0.0f );
	CheckNear( "curve at 1 samples the last entry",   ParticleParm_Eval( &parm, 1.0f, 0.0f ), 100.0f );
	CheckNear( "curve at 0.5 interpolates the middle",ParticleParm_Eval( &parm, 0.5f, 0.0f ),  50.0f );
	// A fraction that lands BETWEEN samples must interpolate, not snap —
	// otherwise a curve is a step function and motion visibly stairsteps.
	// 1/7 is exactly sample 1 of a 8-sample ramp = 1/7 ≈ 0.142857.
	CheckNear( "curve interpolates between samples",
		ParticleParm_Eval( &parm, 0.5f / 7.0f, 0.0f ), 100.0f * ( 0.5f / 7.0f ) );

	// val0/val1 place the shape in range, so one authored shape serves many
	// magnitudes. Same curve, different range, proportional result.
	parm.val0 = 10.0f;
	parm.val1 = 20.0f;
	CheckNear( "curve range maps onto val0..val1", ParticleParm_Eval( &parm, 0.5f, 0.0f ), 15.0f );

	// A parm pointing at an unregistered curve must stay usable rather than
	// collapsing to zero — a missing curve should degrade, not blank the
	// effect out.
	parm.curve = PARTICLE_MAX_CURVES + 10;
	CheckNear( "missing curve degrades to a neutral sample",
		ParticleParm_Eval( &parm, 0.5f, 0.0f ), 20.0f );

	// ── 5. curve × linear ───────────────────────────────────────────────
	// A shape riding on an independent trend: flicker that also fades,
	// without authoring the product as a third curve.
	for ( i = 0; i < PARTICLE_CURVE_SAMPLES; i++ )
		spike[i] = 2.0f;             // constant 2 keeps the arithmetic checkable
	idxSpike = CG_RegisterParticleCurve( "spike", spike );
	Check( "second curve registers distinctly", idxSpike > 0 && idxSpike != idxRamp );

	memset( &parm, 0, sizeof( parm ) );
	parm.calc  = PARM_CURVE_TIMES_LINEAR;
	parm.curve = idxSpike;
	parm.val0  = 10.0f;
	parm.val1  = 0.0f;               // fades to nothing
	CheckNear( "curve*linear at 0 is curve*val0", ParticleParm_Eval( &parm, 0.0f, 0.0f ), 20.0f );
	CheckNear( "curve*linear at 1 is curve*val1", ParticleParm_Eval( &parm, 1.0f, 0.0f ),  0.0f );
	CheckNear( "curve*linear at 0.5 is curve*mid",ParticleParm_Eval( &parm, 0.5f, 0.0f ), 10.0f );

	// ── 6. variance is per-particle ─────────────────────────────────────
	// jitterPick is drawn once at emit and carried, so a particle keeps its
	// offset for its whole life. If variance were applied per-frame instead,
	// particles would shimmer — which is why the jitter is an INPUT here
	// rather than something the evaluator draws itself.
	memset( &parm, 0, sizeof( parm ) );
	parm.calc     = PARM_CONSTANT;
	parm.val0     = 10.0f;
	parm.variance = 4.0f;
	CheckNear( "jitter 0 gives the base value",  ParticleParm_Eval( &parm, 0.0f,  0.0f ), 10.0f );
	CheckNear( "jitter +1 adds the variance",    ParticleParm_Eval( &parm, 0.0f,  1.0f ), 14.0f );
	CheckNear( "jitter -1 subtracts it",         ParticleParm_Eval( &parm, 0.0f, -1.0f ),  6.0f );
	CheckNear( "jitter is constant across life", ParticleParm_Eval( &parm, 1.0f,  0.5f ), 12.0f );

	// The evaluator must be deterministic: same inputs, same output, every
	// call. A particle whose size is redrawn each frame flickers.
	{
		float a = ParticleParm_Eval( &parm, 0.37f, 0.25f );
		float b = ParticleParm_Eval( &parm, 0.37f, 0.25f );
		Check( "evaluation is deterministic", a == b );
	}

	// ── 7. table exhaustion is refused, not wrapped ─────────────────────
	// Wrapping would silently alias a new curve onto an existing index and
	// change effects that were already authored and correct.
	{
		int filled = 0;
		char name[32];
		for ( i = 0; i < PARTICLE_MAX_CURVES + 4; i++ ) {
			snprintf( name, sizeof( name ), "fill%d", i );
			if ( CG_RegisterParticleCurve( name, ramp ) )
				filled++;
		}
		Check( "table refuses registrations past capacity",
			filled < PARTICLE_MAX_CURVES + 4 );
		Check( "an already-registered curve survives exhaustion",
			CG_FindParticleCurve( "ramp" ) == idxRamp );
	}

	printf( "\n" );
	if ( failures ) {
		printf( "FAILED: %d contract violation(s)\n", failures );
		return 1;
	}
	printf( "PASS: curve table and parm evaluator behave as specified\n" );
	return 0;
}
