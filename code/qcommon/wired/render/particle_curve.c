// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
particle_curve.c — shared curve table + parameter evaluator

Split out of the cgame particle registry ON PURPOSE, and the purpose is
testability. Everything here is pure: a curve table, a lookup, and an
evaluator that maps a normalised lifetime fraction to a value. No cvars,
no traps, no render handles, no cgame state.

That matters because the alternative — leaving it in cg_wired_particles.c
— would force any test to compile cg_local.h (2200+ lines) and stub the
whole cgame surface to reach two functions. This repo has been bitten by
exactly that shape before: a stub file that satisfied one build
configuration and silently failed another. A pure translation unit lets
the contract test compile the EXACT production code instead.

The evaluator is also the CPU half of a two-sided contract: the same
maths runs in GLSL so curves can be evaluated GPU-side without giving up
emit-and-forget. If the two drift, authored effects render differently
from how they preview — a failure that never crashes and is therefore
easy to miss. The contract test pins them together.
===========================================================================
*/

#include <string.h>

#include "../../q_shared.h"
#include "particle_class.h"

/*
==========================
Shared curve table

Curves are registered by name and deduplicated, so "smoke-falloff"
authored once is one entry that every class referencing it shares. That
is the point of holding them here rather than inside particleClass_t: a
curve is a resource. Editing it later reaches every effect built on it,
and two effects that should fade alike cannot silently drift apart.

Slot 0 is never handed out. It is the reserved "no curve" index, which
is what lets a memset-zero particleParm_t mean exactly one thing —
constant, table untouched — instead of accidentally pointing at whichever
curve happened to be registered first.
==========================
*/
#define CG_CURVE_NAME_MAX 32

static float cg_particleCurves[PARTICLE_MAX_CURVES][PARTICLE_CURVE_SAMPLES];
static char  cg_particleCurveNames[PARTICLE_MAX_CURVES][CG_CURVE_NAME_MAX];
static int   cg_numParticleCurves;   /* slot 0 reserved; first real index is 1 */

int CG_RegisterParticleCurve( const char *name, const float *samples ) {
	int existing, i;

	if ( !name || !name[0] || !samples )
		return 0;

	/* Dedup by name. Returning the SAME index rather than appending a copy
	   is what makes the table shared instead of merely central. */
	existing = CG_FindParticleCurve( name );
	if ( existing )
		return existing;

	/* Silent 0 on a full table, matching CG_RegisterParticleClass above:
	   registration failures are the caller's to check, and this file
	   deliberately carries no logging. A class whose curve failed to
	   register still renders — the parm falls back to its constant. */
	if ( cg_numParticleCurves + 1 >= PARTICLE_MAX_CURVES )
		return 0;

	cg_numParticleCurves++;   /* pre-increment: index 0 stays reserved */
	for ( i = 0; i < PARTICLE_CURVE_SAMPLES; i++ )
		cg_particleCurves[cg_numParticleCurves][i] = samples[i];
	/* Copied by hand rather than via Q_strncpyz so this TU pulls in NO
	   engine symbols: keeping it dependency-free is what lets the contract
	   test compile the exact production code instead of stubbing around it.
	   Semantics match Q_strncpyz — truncate and always NUL-terminate. */
	{
		int n;
		for ( n = 0; n < CG_CURVE_NAME_MAX - 1 && name[n]; n++ )
			cg_particleCurveNames[cg_numParticleCurves][n] = name[n];
		cg_particleCurveNames[cg_numParticleCurves][n] = '\0';
	}

	return cg_numParticleCurves;
}

int CG_FindParticleCurve( const char *name ) {
	int i;

	if ( !name || !name[0] )
		return 0;
	for ( i = 1; i <= cg_numParticleCurves; i++ ) {
		if ( !strcmp( cg_particleCurveNames[i], name ) )
			return i;
	}
	return 0;
}

const float *CG_GetParticleCurve( int index ) {
	if ( index <= 0 || index > cg_numParticleCurves )
		return NULL;
	return cg_particleCurves[index];
}

/*
==========================
ParticleParm_IsUnset / ParticleParm_Eval

The override rule lives HERE and only here: a parm that was never
authored is PARM_CONSTANT with val0 == 0, which is what a memset-zero
class produces. Callers ask ParticleParm_IsUnset rather than testing the
fields themselves, so "did the author set this?" has one answer shared by
the host, the renderer upload path and the tests. Three copies of that
test would be three chances to disagree.

ParticleParm_Eval is the CPU mirror of the GLSL evaluator. They are
pinned against each other by contract test, because a divergence here
would not crash — it would render something other than what was
authored, which is far harder to notice.
==========================
*/
qboolean ParticleParm_IsUnset( const particleParm_t *parm ) {
	if ( !parm )
		return qtrue;
	return ( parm->calc == PARM_CONSTANT && parm->val0 == 0.0f ) ? qtrue : qfalse;
}

static float CG_SampleCurve( const particleParm_t *parm, float fraction ) {
	const float *table = parm->samples;
	float        pos, frac;
	int          i0, i1;

	if ( !parm->hasCurve )
		return 1.0f;   /* no curve => neutral multiplier, never a zeroing surprise */

	if ( fraction <= 0.0f )
		return table[0];
	if ( fraction >= 1.0f )
		return table[PARTICLE_CURVE_SAMPLES - 1];

	/* Samples span 0..1 inclusive, so the last sample sits AT fraction 1;
	   that is why the span is (N-1), not N. Getting this wrong shifts every
	   curve slightly and is invisible without a numeric test. */
	pos  = fraction * (float)( PARTICLE_CURVE_SAMPLES - 1 );
	i0   = (int)pos;
	i1   = i0 + 1;
	if ( i1 > PARTICLE_CURVE_SAMPLES - 1 )
		i1 = PARTICLE_CURVE_SAMPLES - 1;
	frac = pos - (float)i0;

	return table[i0] + ( table[i1] - table[i0] ) * frac;
}

float ParticleParm_Eval( const particleParm_t *parm, float fraction, float jitterPick ) {
	float base, shape;

	if ( !parm )
		return 0.0f;

	if ( fraction < 0.0f ) fraction = 0.0f;
	if ( fraction > 1.0f ) fraction = 1.0f;

	switch ( parm->calc ) {
	case PARM_LINEAR:
		base = parm->val0 + ( parm->val1 - parm->val0 ) * fraction;
		break;

	case PARM_CURVE:
		/* The table carries the SHAPE in 0..1; val0/val1 place it in range.
		   Authoring a falloff once and reusing it at different magnitudes is
		   the reason for that split. */
		shape = CG_SampleCurve( parm, fraction );
		base  = parm->val0 + ( parm->val1 - parm->val0 ) * shape;
		break;

	case PARM_CURVE_TIMES_LINEAR:
		shape = CG_SampleCurve( parm, fraction );
		base  = shape * ( parm->val0 + ( parm->val1 - parm->val0 ) * fraction );
		break;

	case PARM_CONSTANT:
	default:
		base = parm->val0;
		break;
	}

	/* Variance is per-particle, not per-frame: jitterPick is drawn once at
	   emit and carried, so a particle keeps its offset for its whole life
	   instead of shimmering. */
	return base + parm->variance * jitterPick;
}


qboolean CG_ResolveParticleParmCurve( particleParm_t *parm, const char *curveName ) {
	const float *samples;
	int          i;

	if ( !parm )
		return qfalse;

	samples = CG_GetParticleCurve( CG_FindParticleCurve( curveName ) );
	if ( !samples ) {
		/* Unknown curve: leave the parm alone rather than zeroing it. A class
		   whose curve failed to register keeps whatever constant it had, which
		   still draws — blanking it would turn a registration slip into an
		   invisible effect. */
		return qfalse;
	}

	for ( i = 0; i < PARTICLE_CURVE_SAMPLES; i++ )
		parm->samples[i] = samples[i];
	parm->hasCurve = 1;
	return qtrue;
}
