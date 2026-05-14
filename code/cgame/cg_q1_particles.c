// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cg_q1_particles.c — Q1 projectile particle trail emission.

Called from CG_General() for ET_GENERAL entities whose model path matches
a known Q1 MDL projectile.  Uses CG_SmokePuff / CG_BubbleTrail (declared
in cg_local.h) because the Q3 weapon trail functions are translation-unit
static and cannot be called from here.

PREREQUISITE GAP:
Q1 projectile entities (progs/missile.mdl, progs/spike.mdl, etc.) are not
yet created by the game DLL — g_spawn.c only remaps Q1 item/weapon pickups
to Q3 equivalents.  CG_Q1_MaybeEmitTrail will be a no-op until game-side
projectile entity creation is implemented.

Lightning bolt (progs/bolt.mdl, bolt2.mdl, bolt3.mdl) is deferred: it
requires a beam primitive, not smoke puffs.
*/

#include "cg_local.h"

/* ---------- nail trail -------------------------------------------------- */
#define Q1_NAIL_STEP    30      /* ms between puffs */

static void CG_Q1_NailTrail( centity_t *cent ) {
    const vec3_t up = { 0, 0, 0 };
    int t;

    if ( cg_noProjectileTrail.integer ) return;
    if ( cent->trailTime > cg.time ) cent->trailTime = cg.time;

    for ( t = cent->trailTime + Q1_NAIL_STEP; t <= cg.time; t += Q1_NAIL_STEP ) {
        CG_SmokePuff( cent->lerpOrigin, up,
                      4.0f,
                      0.8f, 0.8f, 0.8f, 0.25f,
                      400, t, 0, 0,
                      cgs.media.smokePuffShader );
    }
    cent->trailTime = cg.time;
}

/* ---------- rocket trail ----------------------------------------------- */
#define Q1_ROCKET_STEP  50

static void CG_Q1_RocketTrail( centity_t *cent ) {
    const vec3_t up = { 0, 0, 0 };
    int t;

    if ( cg_noProjectileTrail.integer ) return;
    if ( cent->trailTime > cg.time ) cent->trailTime = cg.time;

    for ( t = cent->trailTime + Q1_ROCKET_STEP; t <= cg.time; t += Q1_ROCKET_STEP ) {
        CG_SmokePuff( cent->lerpOrigin, up,
                      14.0f,
                      1.0f, 1.0f, 1.0f, 0.33f,
                      900, t, 0, 0,
                      cgs.media.smokePuffShader );
    }
    cent->trailTime = cg.time;
}

/* ---------- grenade trail ---------------------------------------------- */
#define Q1_GRENADE_STEP 60

static void CG_Q1_GrenadeTrail( centity_t *cent ) {
    const vec3_t up = { 0, 0, 0 };
    int t;

    if ( cg_noProjectileTrail.integer ) return;
    if ( cent->trailTime > cg.time ) cent->trailTime = cg.time;

    for ( t = cent->trailTime + Q1_GRENADE_STEP; t <= cg.time; t += Q1_GRENADE_STEP ) {
        CG_SmokePuff( cent->lerpOrigin, up,
                      10.0f,
                      0.9f, 0.9f, 0.7f, 0.28f,
                      700, t, 0, 0,
                      cgs.media.smokePuffShader );
    }
    cent->trailTime = cg.time;
}

/* ---------- laser trail ------------------------------------------------- */
#define Q1_LASER_STEP   25

static void CG_Q1_LaserTrail( centity_t *cent ) {
    const vec3_t up = { 0, 0, 0 };
    int t;

    if ( cg_noProjectileTrail.integer ) return;
    if ( cent->trailTime > cg.time ) cent->trailTime = cg.time;

    for ( t = cent->trailTime + Q1_LASER_STEP; t <= cg.time; t += Q1_LASER_STEP ) {
        CG_SmokePuff( cent->lerpOrigin, up,
                      3.0f,
                      1.0f, 0.8f, 0.1f, 0.35f,   /* yellow-orange */
                      250, t, 0, 0,
                      cgs.media.smokePuffShader );
    }
    cent->trailTime = cg.time;
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
