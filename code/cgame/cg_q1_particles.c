// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cg_q1_particles.c — Q1 projectile particle trail emission.

Supports both ET_GENERAL compatibility entities selected by Q1 MDL path and
ET_MISSILE entities selected by pType. Historical puffs are sampled on a
fixed time grid in the same visual trajectory space as the rendered missile,
so prediction/nudging and high render rates cannot detach or suppress trails.

Lightning bolt (progs/bolt.mdl, bolt2.mdl, bolt3.mdl) is deferred: it
requires a beam primitive, not smoke puffs.
*/

#include "cg_local.h"

static void CG_Q1_PuffTrail( centity_t *cent, int maxStep, float radius,
	const vec4_t color, int lifetime ) {
	const vec3_t up = { 0, 0, 0 };
	int step = CG_ProjectileTrailStepForSpacing( cent, 12.0f, maxStep );
	int t;
	int lastBoundary = cent->trailTime;

	if ( cg_noProjectileTrail.integer ) return;
	if ( cent->trailTime > cg.time ) cent->trailTime = cg.time;

	// Preserve the sub-step remainder: resetting trailTime every render frame
	// makes a 25-60 ms trail emit nothing at modern frame rates. Each puff is
	// evaluated at its actual fixed-time boundary in the projectile's rendered
	// visual space, instead of stacking every historical puff at lerpOrigin.
	t = step * ( ( cent->trailTime + step ) / step );
	for ( ; t <= cg.time; t += step ) {
		vec3_t origin;
		CG_EvaluateVisualTrajectory( cent, t, origin );
		CG_SmokePuff( origin, up, radius,
			color[0], color[1], color[2], color[3] * (float)step / (float)maxStep,
			lifetime, t, 0, 0, cgs.media.smokePuffShader );
		lastBoundary = t;
	}
	cent->trailTime = lastBoundary;
}

/* ---------- nail trail -------------------------------------------------- */
#define Q1_NAIL_STEP    30      /* ms between puffs */

static void CG_Q1_NailTrail( centity_t *cent ) {
	const vec4_t color = { 0.8f, 0.8f, 0.8f, 0.25f };
	CG_Q1_PuffTrail( cent, Q1_NAIL_STEP, 4.0f, color, 400 );
}

/* ---------- rocket trail ----------------------------------------------- */
#define Q1_ROCKET_STEP  50

static void CG_Q1_RocketTrail( centity_t *cent ) {
	const vec4_t color = { 1.0f, 1.0f, 1.0f, 0.33f };
	CG_Q1_PuffTrail( cent, Q1_ROCKET_STEP, 14.0f, color, 900 );
}

/* ---------- grenade trail ---------------------------------------------- */
#define Q1_GRENADE_STEP 60

static void CG_Q1_GrenadeTrail( centity_t *cent ) {
	const vec4_t color = { 0.9f, 0.9f, 0.7f, 0.28f };
	CG_Q1_PuffTrail( cent, Q1_GRENADE_STEP, 10.0f, color, 700 );
}

/* ---------- laser trail ------------------------------------------------- */
#define Q1_LASER_STEP   25

static void CG_Q1_LaserTrail( centity_t *cent ) {
	const vec4_t color = { 1.0f, 0.8f, 0.1f, 0.35f };
	CG_Q1_PuffTrail( cent, Q1_LASER_STEP, 3.0f, color, 250 );
}

/* ---------- lavaball trail --------------------------------------------- */
#define Q1_LAVABALL_STEP 40

static void CG_Q1_LavaballTrail( centity_t *cent ) {
	const vec4_t color = { 1.0f, 0.35f, 0.05f, 0.40f };
	CG_Q1_PuffTrail( cent, Q1_LAVABALL_STEP, 8.0f, color, 500 );
}

/* ---------- dispatch table --------------------------------------------- */

typedef struct {
    const char  *mdlPath;
    void        (*trailFunc)( centity_t *cent );
} q1TrailEntry_t;

static const q1TrailEntry_t s_q1Trails[] = {
    { "progs/missile.mdl",  CG_Q1_RocketTrail  },
    { "progs/grenade.mdl",  CG_Q1_GrenadeTrail },
    { "progs/spike.mdl",    CG_Q1_NailTrail    },
    { "progs/s_spike.mdl",  CG_Q1_NailTrail    },
    { "progs/k_spike.mdl",  CG_Q1_NailTrail    },
    { "progs/laser.mdl",    CG_Q1_LaserTrail   },
    { NULL,                 NULL               }
};

/*
CG_Q1_MaybeEmitTrail
====================
Called from CG_General() for every ET_GENERAL entity each frame.
Compares the entity's configstring model path against s_q1Trails[].
No-op if modelindex is 0, the configstring is empty, or the path is not
a known Q1 MDL projectile.
*/
void CG_Q1_MaybeEmitTrail( centity_t *cent ) {
    int                     modelindex = cent->currentState.modelindex;
    const char             *mdlPath;
    const q1TrailEntry_t   *e;

    if ( !modelindex ) return;

    mdlPath = CG_ConfigString( CS_MODELS + modelindex );
    if ( !mdlPath || !mdlPath[0] ) return;

    for ( e = s_q1Trails; e->mdlPath; e++ ) {
        if ( !strcmp( mdlPath, e->mdlPath ) ) {
            e->trailFunc( cent );
            return;
        }
    }
}

/*
CG_Q1_MaybeEmitMissileTrail
============================
Q1 monster/trap projectiles are ET_MISSILE with WP_NONE and identify their
visual behavior through pType. Claim them before WP_NONE's grapple handler.
*/
qboolean CG_Q1_MaybeEmitMissileTrail( centity_t *cent ) {
	switch ( cent->currentState.pType ) {
	case PROJ_SPIKE:
		CG_Q1_NailTrail( cent );
		return qtrue;
	case PROJ_LASER:
		CG_Q1_LaserTrail( cent );
		return qtrue;
	case PROJ_LAVABALL:
		CG_Q1_LavaballTrail( cent );
		return qtrue;
	default:
		return qfalse;
	}
}
