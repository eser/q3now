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
	parm.val0  = 0.0f;
	parm.val1  = 100.0f;
	Check( "resolving a registered curve into a parm succeeds",
		CG_ResolveParticleParmCurve( &parm, "ramp" ) );
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
	// Resolving an UNKNOWN curve must leave the parm alone rather than
	// zeroing it: a class whose curve failed to register should keep drawing
	// with whatever constant it had, not vanish.
	{
		particleParm_t missing;
		memset( &missing, 0, sizeof( missing ) );
		missing.calc = PARM_CURVE;
		missing.val0 = 5.0f;
		missing.val1 = 20.0f;
		Check( "resolving an unknown curve reports failure",
			!CG_ResolveParticleParmCurve( &missing, "no-such-curve" ) );
		Check( "a failed resolve leaves hasCurve clear", missing.hasCurve == 0 );
		CheckNear( "unresolved curve degrades to a neutral sample",
			ParticleParm_Eval( &missing, 0.5f, 0.0f ), 20.0f );
	}

	// ── 5. curve × linear ───────────────────────────────────────────────
	// A shape riding on an independent trend: flicker that also fades,
	// without authoring the product as a third curve.
	for ( i = 0; i < PARTICLE_CURVE_SAMPLES; i++ )
		spike[i] = 2.0f;             // constant 2 keeps the arithmetic checkable
	idxSpike = CG_RegisterParticleCurve( "spike", spike );
	Check( "second curve registers distinctly", idxSpike > 0 && idxSpike != idxRamp );

	memset( &parm, 0, sizeof( parm ) );
	parm.calc  = PARM_CURVE_TIMES_LINEAR;
	Check( "second curve resolves into a parm",
		CG_ResolveParticleParmCurve( &parm, "spike" ) );
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

	// ── 6b. a curve actually changes the shape ──────────────────────────
	//
	// Everything above proves the machinery evaluates; this proves it MATTERS.
	// A curve whose output tracked the linear ramp would pass every test so far
	// and buy nothing — the whole point is expressing a shape a Start/End pair
	// cannot.
	//
	// The numbers are the rocket smoke's real authoring: the same 8→56 endpoints
	// as the linear version it replaced, with an ease-out between them. Endpoints
	// must MATCH (the effect still starts and ends where it did) while the middle
	// must diverge substantially (that is the change).
	{
		static const float easeOut[PARTICLE_CURVE_SAMPLES] = {
			0.00f, 0.45f, 0.70f, 0.84f, 0.92f, 0.96f, 0.99f, 1.00f
		};
		particleParm_t curved, straight;
		float          lo, hi, maxGap;
		int            k;

		CG_RegisterParticleCurve( "ab-ease", easeOut );

		memset( &curved, 0, sizeof( curved ) );
		curved.calc = PARM_CURVE;
		curved.val0 = 8.0f;
		curved.val1 = 56.0f;
		Check( "A/B curve resolves", CG_ResolveParticleParmCurve( &curved, "ab-ease" ) );

		memset( &straight, 0, sizeof( straight ) );
		straight.calc = PARM_LINEAR;
		straight.val0 = 8.0f;
		straight.val1 = 56.0f;

		CheckNear( "curve and linear agree at birth",
			ParticleParm_Eval( &curved, 0.0f, 0.0f ),
			ParticleParm_Eval( &straight, 0.0f, 0.0f ) );
		CheckNear( "curve and linear agree at death",
			ParticleParm_Eval( &curved, 1.0f, 0.0f ),
			ParticleParm_Eval( &straight, 1.0f, 0.0f ) );

		maxGap = 0.0f;
		for ( k = 1; k < 10; k++ ) {
			float f = (float)k / 10.0f;
			float g = ParticleParm_Eval( &curved, f, 0.0f )
			        - ParticleParm_Eval( &straight, f, 0.0f );
			if ( g < 0.0f ) g = -g;
			if ( g > maxGap ) maxGap = g;
		}
		// Measured at ~19.8 units at f=0.4. The floor is well under that and
		// well over float noise: it asks "is this a different shape", not "is
		// it this exact shape", so re-authoring the curve does not break it.
		Check( "curve diverges from linear in between (>10 units)", maxGap > 10.0f );

		// Ease-out specifically means FASTER early: at a fifth of its life the
		// smoke should already be well past the linear value. This is the
		// direction check — a curve that diverged the other way would satisfy
		// the magnitude test above while looking wrong.
		lo = ParticleParm_Eval( &curved,   0.2f, 0.0f );
		hi = ParticleParm_Eval( &straight, 0.2f, 0.0f );
		Check( "ease-out grows faster than linear early", lo > hi );
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

	// ── 8. the GLSL mirror carries the same arithmetic ──────────────────
	//
	// The evaluator exists TWICE: in C above, and in GLSL for both the
	// compute integrate pass and the vertex shader. Nothing at runtime
	// compares them, and a divergence does not crash — it renders something
	// other than what was authored, which is the hardest kind of defect to
	// notice.
	//
	// Reading the shader source is a blunt check, but it is the only one
	// available without a GPU in the test harness, and it catches the
	// specific mistakes that matter: the sample span, the neutral fallback,
	// and each calc branch. A shader edit that drops one of these fails here
	// instead of shipping.
	{
		/* Absolute paths from CMake. A relative path would make this check
		   depend on the working directory, and it silently SKIPped when run
		   from the build tree — a mirror check that does not run is exactly
		   the hole it was written to close. */
#ifndef SHADER_DIR
#error "SHADER_DIR must be defined (absolute path to code/render/ral/backends/vulkan/renderer/shaders)"
#endif
		static const char *shaders[] = {
			SHADER_DIR "/particle.vert",
			SHADER_DIR "/particle_integrate.comp"
		};
		static const struct { const char *needle, *why; } mirrors[] = {
			{ "fraction * 7.0",                 "sample span is (N-1), not N" },
			{ "p.samples[7]",                   "fraction 1 reads the last sample" },
			{ "clamp( fraction, 0.0, 1.0 )",    "fraction is clamped, not extrapolated" },
			{ "return 1.0;",                    "a curve-less parm samples neutral" },
			{ "base = p.val0;",                 "CONSTANT branch" },
			{ "mix( p.val0, p.val1, fraction )","LINEAR branch" },
			{ "base + p.variance * jitterPick", "variance applies once, from the carried pick" }
		};
		int si, mi;

		for ( si = 0; si < (int)( sizeof( shaders ) / sizeof( shaders[0] ) ); si++ ) {
			char  buf[65536];
			FILE *fh = fopen( shaders[si], "rb" );
			size_t n;

			if ( !fh ) {
				printf( "  FAIL cannot open %s\n", shaders[si] );
				failures++;
				continue;
			}
			n = fread( buf, 1, sizeof( buf ) - 1, fh );
			fclose( fh );
			buf[n] = '\0';

			for ( mi = 0; mi < (int)( sizeof( mirrors ) / sizeof( mirrors[0] ) ); mi++ ) {
				char label[160];
				snprintf( label, sizeof( label ), "%s mirrors: %s",
					strrchr( shaders[si], '/' ) + 1, mirrors[mi].why );
				Check( label, strstr( buf, mirrors[mi].needle ) != NULL );
			}
		}
	}

	printf( "\n" );
	if ( failures ) {
		printf( "FAILED: %d contract violation(s)\n", failures );
		return 1;
	}
	printf( "PASS: curve table and parm evaluator behave as specified\n" );
	return 0;
}
