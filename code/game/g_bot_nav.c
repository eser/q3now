// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_bot_nav.c -- Detour-based bot path-following

Replaces trap_BotMoveToGoal() at 7 call sites in ai_dmnet.c / ai_dmq3.c.
All navigation calls go through trap_Nav_* traps; no engine-internal
nav types are used here.

Threading: called synchronously from BotAI think tick (single-threaded).
===========================================================================
*/

#include "../qcommon/q_feats.h"

#if FEAT_RECAST_NAVMESH

#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "ai_main.h"
#include "../qcommon/nav/nav_types.h"
#include "g_bot_nav.h"

/* One nav state per client slot. */
botNavState_t botNavStates[MAX_CLIENTS];

/* ── Navigation world-state generation ────────────────────────────────────
 *
 * A planned corridor is only valid for the graph it was planned against. The
 * three original repath triggers (no path / time interval / goal moved) all
 * observe the AGENT; none observes the WORLD. So when a mover transition
 * mutated the graph — a door opening reverts its polys from the 10x
 * NAVAREA_DOOR cost back to their recorded area, a plat reaching POS2 does the
 * same for its own polys — the newly-cheap corridor sat unused until the next
 * time-interval tick, by which point a short mover `wait` had already undone
 * the change. A cost revert that no one re-queries is not a route change.
 *
 * This counter is that missing fourth trigger. It is bumped at the generic
 * mover-transition seam and compared per agent on its next steer step; the
 * mismatch forces a fresh query against the graph as it is RIGHT NOW.
 *
 * Event-driven, not polled: the transition is the event and already owns a call
 * site (the same one that applies the area/flag mutation), so the cost is a
 * single integer store per transition regardless of agent count — no per-frame
 * scan, no walk over agents at the call site. Agents that hold no path, or are
 * mid off-mesh traversal, are unaffected: they consult the generation only where
 * they would already consider re-planning.
 *
 * Wrap-around is harmless: the comparison is an INEQUALITY, so the only cost of
 * a (2^32-transition) wrap coinciding exactly with an agent's stored value is
 * one missed repath, which the time interval then covers. */
static unsigned int navWorldGen;

/* ── SCOPING: which mover changed, so only affected corridors are invalidated ──
 *
 * CLASS RULE: a world-state invalidation must reach only agents whose current plan
 * DEPENDS on the changed state. An agent whose corridor does not touch the changed
 * geometry has no reason to re-plan — and, critically, an agent that CAUSED the
 * change must not be invalidated by its own action.
 *
 * The unscoped form (bump a global counter, every agent compares it) was correct in
 * MECHANISM and wrong in SCOPE, and the defect it produced is self-reinforcing at
 * the actuation site: the bot presses the button -> the press transitions a mover ->
 * the transition bumps the counter -> the counter invalidates the presser's OWN
 * corridor -> the presser re-plans, and the cheapest plan from where it stands is
 * the button it is standing on. It re-commits to the actuation it just completed and
 * consumes its own open-window. Measured: 37 of 49 world_changed repaths fired while
 * the agent was on a button goal, and the harness regressed (door-opened 6 -> 3,
 * launch-point repath share 51% -> 69%).
 *
 * The scoped form records WHAT changed, not merely THAT something changed. Each
 * transition writes one slot of a small ring: the changed mover's world bounds at
 * the moment of transition, stamped with the generation. An agent compares its
 * stored generation as before, but a mismatch is no longer sufficient — it must also
 * find that its OWN held corridor crosses the bounds of at least one mover that
 * changed since its stamp. A corridor that crosses nothing that moved is not stale,
 * and is left alone.
 *
 * Why an agent that caused the change cannot be invalidated by it: the agent standing
 * ON a button has, by construction, a corridor that ENDS at that button — the button
 * is its goal, the corridor terminates there, and the mover that transitioned is the
 * DOOR the button targets, which lies elsewhere on the map. The door's bounds do not
 * intersect the button-goal corridor, so the presser's own corridor fails the crossing
 * test and survives. An agent whose route genuinely runs THROUGH that door does cross
 * it, and does re-plan — which is the behaviour the trigger exists to produce.
 *
 * COST at the event: O(1) — one ring slot write, no walk over agents, exactly as
 * before. Cost at the TEST: bounded by the agent's own corridor length, and paid only
 * when that agent already has a generation mismatch pending (i.e. only after a real
 * transition, and only once per transition per agent, because the stamp is refreshed
 * whether or not the crossing test passes). No agent scan is performed at the event,
 * so this is NOT a per-transition sweep over all agents.
 *
 * The ring is small and overwrites oldest-first. An overflow (more transitions than
 * slots between one agent's steps) degrades to the CONSERVATIVE side: an agent whose
 * stamp is older than every retained slot cannot prove it is unaffected, so it
 * re-plans. That is the old global behaviour, reached only under a burst, and it is
 * safe rather than blind. */
#define NAV_WORLDCHANGE_RING  16

typedef struct {
    unsigned int gen;        /* generation stamped at this transition */
    vec3_t       absmin;     /* changed mover's world bounds, captured at transition */
    vec3_t       absmax;
} navWorldChange_t;

static navWorldChange_t navWorldChanges[NAV_WORLDCHANGE_RING];
static int              navWorldChangeHead;   /* next slot to write */

/*
=================
Nav_WorldChanged — the nav graph's cost/passability changed (see g_bot_nav.h)

'absmin'/'absmax' are the changed mover's world bounds. They are what scopes the
invalidation: an agent re-plans only if its held corridor crosses one of these.
=================
*/
void Nav_WorldChanged( const vec3_t absmin, const vec3_t absmax )
{
    navWorldChange_t *wc;

    navWorldGen++;

    wc = &navWorldChanges[ navWorldChangeHead ];
    wc->gen = navWorldGen;
    VectorCopy( absmin, wc->absmin );
    VectorCopy( absmax, wc->absmax );

    navWorldChangeHead = ( navWorldChangeHead + 1 ) % NAV_WORLDCHANGE_RING;
}

/*
=================
Nav_WorldGen — current navigation world-state generation
=================
*/
unsigned int Nav_WorldGen( void )
{
    return navWorldGen;
}

/*
=================
Nav_SegmentHitsBounds — swept-segment vs AABB (slab clip)

Does the segment p0->p1 pass through the box [bmin,bmax], with the box expanded by
the player's origin-relative half-extents so a corridor leg that merely brushes the
mover still counts? Mirrors the intel layer's swept test; kept local so the nav core
carries no dependency on the bot-intel module.
=================
*/
static qboolean Nav_SegmentHitsBounds( const vec3_t p0, const vec3_t p1,
                                       const vec3_t bmin, const vec3_t bmax )
{
    vec3_t emin, emax, d;
    float  t0 = 0.0f, t1 = 1.0f;
    int    a;

    /* Minkowski expansion by the agent's own box, in origin space. */
    emin[0] = bmin[0] - 15.0f;      emax[0] = bmax[0] + 15.0f;
    emin[1] = bmin[1] - 15.0f;      emax[1] = bmax[1] + 15.0f;
    emin[2] = bmin[2] - MAXS_Z;     emax[2] = bmax[2] - MINS_Z;

    VectorSubtract( p1, p0, d );

    for ( a = 0; a < 3; a++ ) {
        if ( d[a] > -1.0e-6f && d[a] < 1.0e-6f ) {
            /* Parallel to this slab: the segment must already lie inside it. */
            if ( p0[a] < emin[a] || p0[a] > emax[a] ) return qfalse;
            continue;
        }
        {
            float inv = 1.0f / d[a];
            float ta  = ( emin[a] - p0[a] ) * inv;
            float tb  = ( emax[a] - p0[a] ) * inv;
            if ( ta > tb ) { float s = ta; ta = tb; tb = s; }
            if ( ta > t0 ) t0 = ta;
            if ( tb < t1 ) t1 = tb;
            if ( t0 > t1 ) return qfalse;   /* slabs disjoint along the segment */
        }
    }
    return qtrue;
}

/*
=================
Nav_CorridorTouchesWorldChange — is THIS agent's corridor affected?

True when the agent's held corridor crosses the bounds of at least one mover that
transitioned after the agent's stored generation. This is the scoping predicate: a
generation mismatch alone no longer invalidates — the corridor must actually depend
on something that moved.

Returns qtrue (conservative) when the agent's stamp is older than every retained ring
slot, because the change that would have proved relevance has been overwritten.
=================
*/
static qboolean Nav_CorridorTouchesWorldChange( const botNavState_t *bn )
{
    int  i, w;
    unsigned int oldestRetained = 0;
    qboolean haveOldest = qfalse;

    if ( bn->path.count == 0 ) return qtrue;   /* no corridor to exonerate */

    for ( w = 0; w < NAV_WORLDCHANGE_RING; w++ ) {
        const navWorldChange_t *wc = &navWorldChanges[w];

        if ( wc->gen == 0 ) continue;          /* never written */

        /* Track the oldest generation still on record, for the overflow test. */
        if ( !haveOldest || wc->gen < oldestRetained ) {
            oldestRetained = wc->gen;
            haveOldest     = qtrue;
        }

        /* Only changes NEWER than this agent's stamp can make its corridor stale. */
        if ( wc->gen <= bn->worldGen ) continue;

        /* Does the corridor cross this mover? Test every leg; a single-waypoint
         * corridor has no leg, so test the point itself. */
        if ( bn->path.count == 1 ) {
            if ( Nav_SegmentHitsBounds( bn->path.positions[0], bn->path.positions[0],
                                        wc->absmin, wc->absmax ) ) {
                return qtrue;
            }
            continue;
        }
        for ( i = 0; i + 1 < bn->path.count; i++ ) {
            if ( Nav_SegmentHitsBounds( bn->path.positions[i], bn->path.positions[i + 1],
                                        wc->absmin, wc->absmax ) ) {
                return qtrue;
            }
        }
    }

    /* Overflow: the agent's stamp predates everything retained, so the transition
     * that might have affected it is no longer on record. Cannot prove irrelevance —
     * fall back to the conservative (old, global) behaviour. */
    if ( haveOldest && bn->worldGen < oldestRetained - 1 ) {
        return qtrue;
    }

    return qfalse;   /* nothing that moved touches this corridor */
}

/* ── Per-entity nav-state pool (non-client path) ──────────────────────────
 *
 * A non-client entity (a server-side pawn with no client slot) that follows a
 * nav path acquires one of these slots into ent->navState, instead of using the
 * client-keyed botNavStates[] array. The pool is small and on demand rather
 * than a flat MAX_GENTITIES array, because botNavState_t is ~4.4 KB (a full
 * navPath_t) — a gentity-sized array would cost several megabytes resident even
 * with no non-client navigators. Sized to MAX_MONSTERS, the shared cap that
 * also sizes the behavior-state pool, so a monster that both moves and thinks
 * acquires a slot from each without the caps diverging. Acquire/release are
 * balanced; the pool resets each map. */
#define NAV_ENTITY_POOL_SIZE MAX_MONSTERS

static botNavState_t navEntityPool[NAV_ENTITY_POOL_SIZE];
static qboolean      navEntityPoolUsed[NAV_ENTITY_POOL_SIZE];

/* A behavior monster drives its own animation vocabulary (a MANIM code in s.legsAnim);
 * the nav follower's cosmetic player LEGS_/TORSO_ writes must not clobber it. This is
 * true only for a thinking monster (aiThink) — the bare nav dev-follower has no
 * behavior layer and keeps the cosmetic anim. Compiles to a constant qfalse when the
 * monster AI feature is off (no monsters, so the follower writes proceed unchanged). */
static ID_INLINE qboolean Nav_IsBehaviorMonster( const gentity_t *ent )
{
#if FEAT_MONSTER_AI
    return ent->aiThink;
#else
    (void)ent;
    return qfalse;
#endif
}

/*
=================
Nav_ResetEntityPool — free every slot (called on map/game init)
=================
*/
void Nav_ResetEntityPool( void )
{
    memset( navEntityPool, 0, sizeof( navEntityPool ) );
    memset( navEntityPoolUsed, 0, sizeof( navEntityPoolUsed ) );
}

/*
=================
Nav_AcquireState — grab a free pool slot for this entity

Zeroes the slot, marks it active, points ent->navState at it. Returns NULL if
the pool is exhausted (the caller must handle having no nav-state).
=================
*/
struct botNavState_s *Nav_AcquireState( gentity_t *ent )
{
    int i;

    if ( !ent ) return NULL;
    if ( ent->navState ) return ent->navState;   /* already holds one */

    for ( i = 0; i < NAV_ENTITY_POOL_SIZE; i++ ) {
        if ( !navEntityPoolUsed[i] ) {
            navEntityPoolUsed[i] = qtrue;
            memset( &navEntityPool[i], 0, sizeof( navEntityPool[i] ) );
            navEntityPool[i].active = qtrue;
            ent->navState = &navEntityPool[i];
            return ent->navState;
        }
    }

    BotAI_Print( PRT_WARNING,
        "nav-state pool exhausted (%d slots) — entity %d gets no nav\n",
        NAV_ENTITY_POOL_SIZE, (int)( ent - g_entities ) );
    return NULL;
}

/*
=================
Nav_ReleaseState — return this entity's slot to the pool

Idempotent: releasing an entity with no nav-state (or a pointer outside the
pool) is a no-op. Clears ent->navState.
=================
*/
void Nav_ReleaseState( gentity_t *ent )
{
    if ( !ent || !ent->navState ) return;

    ptrdiff_t idx = ent->navState - navEntityPool;
    if ( idx >= 0 && idx < NAV_ENTITY_POOL_SIZE ) {
        navEntityPoolUsed[idx] = qfalse;
    }
    ent->navState = NULL;
}

/*
=================
Nav_SteerEntity — drive a pool-backed entity one step toward goal

Reads the entity's current origin + trajectory velocity and runs the same
client-agnostic BotNav_Steer core the bot path and the walk-test use. out is
zeroed with failure set if the entity holds no nav-state.
=================
*/
void Nav_SteerEntity( gentity_t *ent, const vec3_t goal, botNavSteerResult_t *out )
{
    if ( !out ) return;
    if ( !ent || !ent->navState ) {
        memset( out, 0, sizeof( *out ) );
        out->failure = qtrue;
        return;
    }
    /* r.currentOrigin is the entity's authoritative current position; trDelta is
     * its velocity (the trajectory model a mover uses). */
    BotNav_Steer( ent->navState, ent->r.currentOrigin, ent->s.pos.trDelta, goal,
                  (int)( ent - g_entities ), out );
}

/* ── Nav-driven mover (non-client) ────────────────────────────────────────
 *
 * A server-side pawn with no client slot rides a mover-trajectory between nav
 * waypoints and re-steers only at leg boundaries (the func_train cadence), so
 * the per-frame cost is a trajectory eval + a link — no per-frame trace. The
 * one trace it does spend (ground-clamp) fires once per LEG, not per frame. */

#define NAV_FOLLOWER_SPEED      200.0f   /* Q3 units/sec along a leg           */
#define NAV_FOLLOWER_LEG_LEN    64.0f    /* how far each straight leg advances */
#define NAV_FOLLOWER_GOAL_RADIUS 40.0f   /* within this of navGoal = arrived   */
#define NAV_FOLLOWER_GROUND_LIFT 24.0f   /* start the ground-trace this far up  */
#define NAV_FOLLOWER_GROUND_DROP 96.0f   /* down-trace depth for ground clamp  */

/*
=================
Nav_FollowerSetLeg — set a straight trajectory leg from origin toward dir

Mirrors Q3_SetMoverState's MOVER_1TO2 math: trBase = current origin, trDelta =
dir*speed, trType = TR_LINEAR_STOP, trDuration = legLen/speed. Records when the
leg expires in ent->navLegEnd so the runner knows when to re-steer.
=================
*/
static void Nav_FollowerSetLeg( gentity_t *ent, const vec3_t dir, float legLen )
{
    /* navSpeed 0 (every follower except a scripted walktomarker) uses the default
     * run speed — byte-identical to the pre-override behavior. A non-zero override
     * gives a slower cutscene-approach gait. */
    float speed = ( ent->navSpeed > 0.0f ) ? ent->navSpeed : NAV_FOLLOWER_SPEED;
    float durMs = ( legLen / speed ) * 1000.0f;
    if ( durMs < 1.0f ) durMs = 1.0f;

    VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
    VectorScale( dir, speed, ent->s.pos.trDelta );
    ent->s.pos.trType     = TR_LINEAR_STOP;
    ent->s.pos.trTime     = level.time;
    ent->s.pos.trDuration = (int)durMs;
    ent->navLegEnd        = level.time + (int)durMs;
}

/*
=================
Nav_FollowerStop — freeze the follower in place (goal reached or unreachable)
=================
*/
static void Nav_FollowerStop( gentity_t *ent )
{
    VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
    VectorClear( ent->s.pos.trDelta );
    ent->s.pos.trType = TR_STATIONARY;
    ent->s.pos.trTime = level.time;
    ent->navFollower  = qfalse;   /* stop running it; keep the entity spawned */
    /* A behavior monster owns its own animation vocabulary (a MANIM code set by
     * Behavior_UpdateAnim); the player LEGS_/TORSO_ codes below are the nav dev-
     * follower's cosmetic anim and would collide with — and clobber — the monster's
     * code, making the client clamp it to STAND (stoop + slide). Only stamp the
     * player codes on a non-monster follower (aiThink == qfalse). */
    if ( !Nav_IsBehaviorMonster( ent ) ) {
        ent->s.legsAnim   = LEGS_IDLE;
        ent->s.torsoAnim  = TORSO_STAND;
    }
}

/*
=================
Nav_FollowerGroundClamp — one down-trace to keep the leg's base on the floor

Called once per LEG (not per frame). Traces down from the leg's start origin and
snaps trBase to the floor; if nothing is hit within reach, the entity is over a
drop → switch to gravity so it falls instead of floating.
=================
*/
static void Nav_FollowerGroundClamp( gentity_t *ent )
{
    trace_t tr;
    vec3_t  from, to;

    /* Trace from a little ABOVE the leg base straight down. Starting above (not
     * exactly at) the base avoids a startsolid false-negative when the base sits
     * flush on or just inside the floor — otherwise the trace reports no floor
     * and the follower wrongly falls. The floor is normally right at the nav
     * origin, so a modest lift + a generous drop reliably finds it. */
    VectorCopy( ent->s.pos.trBase, from );
    from[2] += NAV_FOLLOWER_GROUND_LIFT;
    VectorCopy( ent->s.pos.trBase, to );
    to[2] -= NAV_FOLLOWER_GROUND_DROP;

    trap_Trace( &tr, from, ent->r.mins, ent->r.maxs, to,
                ent->s.number, ent->clipmask );

    if ( !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f ) {
        /* on ground — snap the leg base to the floor. Keep the leg's delta +
         * navLegEnd so the follower keeps riding this leg; the clamp only
         * corrects the base height, it does not force a re-steer. */
        VectorCopy( tr.endpos, ent->s.pos.trBase );
    }
    /* If the trace found no floor (a real pit) or started solid, leave the
     * horizontal leg as-is — the follower keeps its planar TR_LINEAR_STOP move
     * rather than freezing. A true fall-handling path (TR_GRAVITY over an edge)
     * is deferred; flat-navmesh travel does not need it, and switching to
     * gravity here on a false negative is exactly what stalled the follower. */
}

/*
=================
Nav_StartFollower — turn ent into a nav-driven mover heading to goal
=================
*/
qboolean Nav_StartFollower( gentity_t *ent, const vec3_t goal, int agentType )
{
    if ( !ent ) return qfalse;

    /* A flyer/swimmer steers straight at its goal and never touches the navmesh state,
     * so it needs no pool slot — skipping the acquire keeps the scarce nav-state pool for
     * the ground followers that actually path on it, and lets a flyer start even when the
     * pool is saturated. A ground follower acquires a slot as before. */
#if FEAT_MONSTER_AI
    if ( ent->navMovement == NAVMOVE_GROUND )
#endif
    {
        if ( !Nav_AcquireState( ent ) ) {
            return qfalse;   /* pool exhausted — caller handles */
        }
        /* Path this follower on the navmesh baked for its size (player mesh when 0);
         * an unbuilt size falls back to the player mesh in Nav_QueryForAgent. */
        ent->navState->agentType = agentType;
    }

    ent->navFollower   = qtrue;
    VectorCopy( goal, ent->navGoal );
    ent->physicsObject = qtrue;   /* so doors/plats push it (mover-push filter) */
    ent->s.eType       = ET_GENERAL;
    ent->clipmask      = MASK_PLAYERSOLID;

    /* Seed the first leg by steering once from the spawn origin. */
    ent->navLegEnd = level.time;   /* forces an immediate steer in the runner */
    return qtrue;
}

#if FEAT_MONSTER_AI
/*
=================
Nav_FollowerFlyStep — set the next leg for a flying/swimming monster

A flyer/swimmer has no walkable-floor path: it steers in a straight 3D line toward its
goal (keeping the vertical component the ground steer flattens away) and is NOT ground-
clamped. It reuses the exact leg/trajectory/arrival machinery (Nav_FollowerSetLeg,
TR_LINEAR_STOP, the per-frame ride + arrival test in G_RunNavFollower) — only the steer
source (goal-direction, not the navmesh) and the clamp (none) differ.

A swimmer additionally stays submerged: if the candidate leg-end leaves water, its
vertical component is dropped so the step keeps it in the water volume (Q1-authentic —
a fish does not breach). A flyer skips that check and moves freely in 3D.
=================
*/
static void Nav_FollowerFlyStep( gentity_t *ent )
{
    vec3_t dir, legEnd;
    vec_t  len;

    /* 3D direction straight at the goal — keep dir[2] (the ground steer zeroes it). */
    VectorSubtract( ent->navGoal, ent->r.currentOrigin, dir );
    len = VectorNormalize( dir );
    if ( len < 0.1f ) {
        /* effectively on the goal — the arrival test upstream will stop it */
        Nav_FollowerSetLeg( ent, dir, 0.0f );
        return;
    }

    if ( ent->navMovement == NAVMOVE_SWIM ) {
        /* Keep the fish in water: if the leg-end would leave the water volume, drop the
         * vertical so the step stays submerged (horizontal pursuit continues). Test
         * CONTENTS_WATER specifically — lava/slime are not swimmable water. */
        VectorMA( ent->r.currentOrigin, NAV_FOLLOWER_LEG_LEN, dir, legEnd );
        if ( !( trap_PointContents( legEnd, -1 ) & CONTENTS_WATER ) ) {
            dir[2] = 0.0f;
            if ( VectorNormalize( dir ) < 0.1f ) {
                /* purely-vertical step out of water — hold this leg, retry next boundary */
                Nav_FollowerSetLeg( ent, dir, 0.0f );
                return;
            }
        }
    }

    /* No ground-clamp: a flyer/swimmer sets its Z from the 3D leg, not the floor. */
    Nav_FollowerSetLeg( ent, dir, NAV_FOLLOWER_LEG_LEN );
}
#endif

/*
=================
Nav_OmcArrived — the transit arrival test, as pure arithmetic

THE ONE arrival rule.  Both callers use this: the steer core's transit block
(which owns the transit state machine) and the follower's per-frame ride.  It is
factored out rather than duplicated so a second, divergent arrival rule cannot
exist — the radius and the plane/lateral test below are the shipped ones, moved
here verbatim.

This is vector maths on omcTransitDest and the current origin: NO TRACE, no
entity touched, no state written.  That is what makes it safe to evaluate every
frame — the follower's "no per-frame trace" design is about the ground-clamp
trace, which stays boundary-only.

Optionally reports distToDest/pastPlane/latSq so a caller can record the
geometry AT the sample it took.
=================
*/
static qboolean Nav_OmcArrived( const botNavState_t *bn, const vec3_t origin,
                                float *outDist, float *outPast, float *outLatSq )
{
    vec3_t toDest, linkDir, fromDest;
    float  distToDest, linkLen, pastPlane, latSq;

    VectorSubtract( bn->omcTransitDest, origin, toDest );
    distToDest = VectorLength( toDest );

    VectorSubtract( bn->omcTransitDest, bn->omcTransitStartPos, linkDir );
    linkLen = VectorNormalize( linkDir );
    VectorSubtract( origin, bn->omcTransitDest, fromDest );
    pastPlane = ( linkLen > 0.1f ) ? DotProduct( fromDest, linkDir ) : 0.0f;

    latSq = distToDest * distToDest - pastPlane * pastPlane;
    if ( latSq < 0.0f ) latSq = 0.0f;

    if ( outDist )  *outDist  = distToDest;
    if ( outPast )  *outPast  = pastPlane;
    if ( outLatSq ) *outLatSq = latSq;

    return ( distToDest <= BOT_NAV_OMC_ARRIVE_EPS ||
             ( pastPlane >= 0.0f &&
               latSq <= BOT_NAV_OMC_ARRIVE_EPS * BOT_NAV_OMC_ARRIVE_EPS ) )
           ? qtrue : qfalse;
}

/*
=================
G_RunNavFollower — advance one nav-driven mover this server frame

Hybrid: ride the current trajectory leg every frame (a trajectory eval + a link,
NO trace); only at a leg boundary re-steer once and set the next leg (with one
ground-clamp trace). Between waypoints the hot path is trace-free.

DURING A TRANSIT the arrival test additionally runs every frame (arithmetic
only, see Nav_OmcArrived) — otherwise arrival is sampled only on the leg lattice
and an entity can pass clean through the arrival radius mid-leg untested.
=================
*/
void G_RunNavFollower( gentity_t *ent )
{
    botNavSteerResult_t out;
    vec3_t              toGoal;

    if ( !ent->navFollower ) { G_RunThink( ent ); return; }

    /* ── ride the current leg (per-frame: eval + link, no trace) ─────────── */
    BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->r.currentOrigin );
    trap_LinkEntity( ent );

    /* Arrived? (cheap distance test, no trace) */
    VectorSubtract( ent->navGoal, ent->r.currentOrigin, toGoal );
    if ( VectorLength( toGoal ) <= NAV_FOLLOWER_GOAL_RADIUS ) {
        Nav_FollowerStop( ent );
        BotAI_Print( PRT_MESSAGE, "nav-follower: reached goal\n" );
        G_RunThink( ent );
        return;
    }

    /* ── leg boundary? re-steer once + set next leg (the only trace) ─────── */
    if ( level.time < ent->navLegEnd ) {
        /* MID-LEG ARRIVAL SAMPLING (during a transit only).
         *
         * The arrival test used to be reachable only past this early return, i.e.
         * only inside Nav_SteerEntity at a leg boundary.  A transit therefore
         * sampled arrival on a lattice whose spacing is the leg duration, and an
         * entity could cross the arrival radius mid-leg without ever being tested
         * inside it.  Measured: 36 of 36 transit samples in the previous run carried
         * pastPlane == -distToDest — the ENTRY signature — because entry was the only
         * moment the probe could fire.  Zero mid-transit samples existed.
         *
         * Nav_OmcArrived is pure vector arithmetic: no trace, no entity touched.  So
         * running it per frame costs nothing the header's "no per-frame trace" design
         * protects — the ground-clamp trace below stays boundary-only and is NOT
         * reached from here.  Non-transit frames take the original path untouched.
         *
         * On arrival, fall through to the boundary path by expiring the leg: the
         * transit teardown (steeringState/path reset) is owned by the steer core, so
         * the runner defers to it rather than duplicating the state machine. */
        if ( ent->navState &&
             ent->navState->steeringState == NAV_STEER_OMC_TRANSIT ) {
            botNavState_t *bn = ent->navState;
            float    d, p, l;
            qboolean arrived = Nav_OmcArrived( bn, ent->r.currentOrigin, &d, &p, &l );

            /* Track the TRUE closest approach, with the geometry AT that minimum, so
             * the transit-end report states an approach floor rather than an entry
             * distance.  Updated on every mid-leg frame — this is the per-frame
             * sampling point that did not exist before. */
            if ( d < bn->omcMinDist ) {
                bn->omcMinDist  = d;
                bn->omcMinPast  = p;
                bn->omcMinLatSq = l;
            }

            if ( !arrived ) {
                G_RunThink( ent );   /* mid-leg — keep riding, no steer, no trace */
                return;
            }
            ent->navLegEnd = level.time;   /* expire the leg: re-steer this frame */
        } else {
            G_RunThink( ent );   /* mid-leg — keep riding, no steer, no trace */
            return;
        }
    }

#if FEAT_MONSTER_AI
    /* A flying/swimming monster does not path on the walkable-floor navmesh: it steers
     * straight at the goal in 3D and is not ground-clamped. Everything else — the
     * per-frame ride, the arrival test above, the trajectory leg — is shared. */
    if ( ent->navMovement != NAVMOVE_GROUND ) {
        Nav_FollowerFlyStep( ent );
        /* keep the monster's own MANIM legsAnim (fly/swim -> its cruise cycle); the
         * player-anim clobber below is guarded off for behavior monsters anyway */
        G_RunThink( ent );
        return;
    }
#endif

    Nav_SteerEntity( ent, ent->navGoal, &out );

    if ( out.failure ) {
        Nav_FollowerStop( ent );
        BotAI_Print( PRT_MESSAGE, "nav-follower: unreachable/stuck\n" );
        G_RunThink( ent );
        return;
    }
    if ( out.done ) {
        Nav_FollowerStop( ent );
        BotAI_Print( PRT_MESSAGE, "nav-follower: reached goal\n" );
        G_RunThink( ent );
        return;
    }
    if ( out.holdFrame ) {
        /* repath pending — hold a short beat, no new leg.  This is NOT a transit
         * condition and its behaviour is unchanged. */
        ent->navLegEnd = level.time + 50;
        G_RunThink( ent );
        return;
    }
    if ( out.suppressMove ) {
        /* OFF-MESH TRANSIT on the mover path.
         *
         * suppressMove asserts "something other than my own locomotion will carry me".
         * On the CLIENT path that assertion is discharged per mode by the actuation
         * switch, which clears suppression for every self-powered class.  The mover
         * path has no such switch — it only ever honoured the flag and held — so a
         * monster entered a transit, held 50 ms per frame for the whole leash, and
         * never displaced.  Measured on honest code: 205 transits, 196 released by the
         * layer-2 predicate with distToDest never improving by a single unit in
         * 196/196, across WALK/RIDE/CLIMB/FALL alike — mode-independent precisely
         * because this path never consulted the mode.
         *
         * So consult it here, using the SAME semantics the header defines and the
         * client switch implements, rather than a list invented for this site:
         *   BALLISTIC — "the pad's own velocity or an instant teleport carries the
         *               bot; the follower suppresses its move for the whole transit".
         *               A real mechanism carries it, so suppression is CORRECT.
         *   RIDE      — suppress ONLY once actually standing on the mover, the same
         *               boarded test the client arm uses; before boarding the entity
         *               must walk to the board point under its own power.
         *   WALK / FALL / CLIMB — self-powered classes.  The client path clears
         *               suppression for all three, so the mover path must move too.
         *
         * The motion itself is the shipped mover actuation — the identical
         * SetLeg + GroundClamp pair used on every non-transit frame below — aimed at
         * the steer direction the core already computed.  No new constant, no new
         * mechanism, and the client path is untouched. */
        /* BALLISTIC promises a carry, but a promise is only worth the mechanism behind
         * it — the RIDE arm below already refuses to trust the mode and asks the world
         * whether the mover is really underfoot.  Apply the same discipline here.
         *
         * The two BALLISTIC mechanisms are the jump pad / target_push (an impulse
         * applied by the trigger's touch handler) and the teleporter.  BOTH are
         * delivered through trigger touch, and trigger touch is CLIENT-ONLY on two
         * independent gates: Q3_trigger_teleporter_touch returns on `!other->client`
         * (g_trigger.c:386), and G_TouchTriggers itself returns on `!ent->client`
         * (g_active.c:280) — and is only ever called from the client move path, which
         * this mover pawn does not run.  So for a follower with no client slot there
         * is no mechanism at all: nothing will ever move it along this link.
         *
         * Measured on e1m1: entities 182/186/187 entered the single trigger_teleport
         * link 169 times, each from ONE distinct origin (1 of 1 per entity), with
         * closest approach equal to the FULL link length (1600/1600, 1579/1579) and
         * latSq = 0 — i.e. never displaced by a single unit.  Layer 2 correctly
         * falsified every one of those transits, but the corridor immediately
         * re-selected the same link, so the entity spent the entire run in a
         * release/re-enter loop and made zero other-mode transits.
         *
         * Suppressing here would be asserting a carry that cannot occur.  Withholding
         * suppression lets the entity move under its own power toward the link
         * destination, exactly as WALK/FALL/CLIMB already do on this path; if that
         * genuinely cannot reach, layer 2 still falsifies it as before.  No new
         * constant and no new rule — `client` is the shipped "is this a real client"
         * field (g_local.h:131, "NULL if not a client"), the same one the teleporter
         * consults before it will carry anyone. */
        qboolean carried = ( out.omcMode == (unsigned char)NAV_TM_BALLISTIC &&
                             ent->client != NULL );
        if ( !carried && out.omcMode == (unsigned char)NAV_TM_RIDE ) {
            /* "Boarded" must be asked of the FLOOR, not of s.groundEntityNum: this
             * follower moves by trajectory legs and never writes that field, so it is
             * not maintained here (the client arm can use cur_ps.groundEntityNum
             * because the player move code does maintain it).  Ask the same way the
             * ground clamp does — one down-trace from the leg base — and read what it
             * actually hit. */
            trace_t gtr;
            vec3_t  gfrom, gto;
            VectorCopy( ent->s.pos.trBase, gfrom );
            gfrom[2] += NAV_FOLLOWER_GROUND_LIFT;
            VectorCopy( ent->s.pos.trBase, gto );
            gto[2] -= NAV_FOLLOWER_GROUND_DROP;
            trap_Trace( &gtr, gfrom, ent->r.mins, ent->r.maxs, gto,
                        ent->s.number, ent->clipmask );
            carried = ( !gtr.startsolid && !gtr.allsolid && gtr.fraction < 1.0f &&
                        gtr.entityNum >= 0 && gtr.entityNum < ENTITYNUM_WORLD &&
                        g_entities[gtr.entityNum].s.eType == ET_MOVER );
        }
        if ( carried ) {
            ent->navLegEnd = level.time + 50;
            G_RunThink( ent );
            return;
        }
        /* CLAMP THE LEG TO THE REMAINING DISTANCE.
         *
         * The arrival test lives in Nav_SteerEntity, and the runner calls that ONLY at
         * leg boundaries — mid-leg it returns early and keeps riding.  So arrival is
         * sampled on a lattice whose spacing is the leg length (64) against a radius
         * threshold of 32: an entity can pass clean through the arrival radius in the
         * middle of a leg and never be tested while it is inside.
         *
         * Measured: no boundary in the whole run ever sampled below 56, and client 209
         * boundaries ran 67, 68, 67, 68, 56 — oscillating at a fixed distance rather
         * than converging, exactly what a 64-unit step past a target 56-68 units away
         * produces.
         *
         * Shortening the last leg to the remaining distance puts a boundary ON the
         * destination instead of beyond it, so the arrival test is evaluated where the
         * entity actually is closest.  This is the shipped Nav_FollowerFlyStep
         * behaviour generalised, not a new idea: that path already passes a shortened
         * length to Nav_FollowerSetLeg when the remainder is small.  No new constant —
         * the remaining distance is data, and both LEG_LEN and ARRIVE_EPS are
         * unchanged.  Non-transit legs are untouched. */
        /* AND STEER AT THE LINK DESTINATION, not at out.movedir.
         *
         * The core's steer direction aims at path.positions[pathIdx] — the OMC SOURCE
         * waypoint — and, while the bot is within APPROACH_DIST, at a point extended
         * BOT_NAV_OMC_OVERSHOOT further along the APPROACH vector (the omcExtended
         * block).  That is correct for its purpose: it drives the bbox fully into the
         * trigger brush.  But it points away from the destination once the transit has
         * begun, so a follower that consumes it verbatim walks past the entry on its
         * approach heading.  Measured: at distToDest=56, pastPlane=-27, i.e. ~49 units
         * of LATERAL offset from the link line — not short of the destination, beside
         * it.
         *
         * The client path never sees this because every mode arm overwrites applyDir
         * with its own destination-relative target before the move is issued.  The
         * mover path had no equivalent override.  This is that override, generalised
         * from what the arms already do, using the destination the core already
         * exports and this block already reads for the clamp.
         *
         * Vertical: match the core's own non-transit policy at the steer site —
         * dir[2] = 0, "the bot uses jump/gravity for vertical, not steering".  The
         * ground clamp re-seats the leg base on the floor afterwards exactly as it
         * does for every other leg, so no vertical policy is invented here.  If the
         * destination is directly overhead the flattened vector degenerates; fall back
         * to the core's heading in that case rather than steering nowhere. */
        {
            vec3_t toDestVec, headDir;
            float  remaining, flatLen;
            VectorSubtract( out.omcDest, ent->r.currentOrigin, toDestVec );
            VectorCopy( toDestVec, headDir );
            headDir[2] = 0.0f;
            flatLen = VectorNormalize( headDir );
            /* Clamp against the HORIZONTAL remainder, because the leg travels the
             * flattened heading — clamping to the 3D distance would overshoot by the
             * vertical component on a link with any rise or drop. */
            remaining = flatLen;
            if ( flatLen < 0.1f ) {
                VectorCopy( out.movedir, headDir );
                remaining = VectorLength( toDestVec );
            }
            /* TRANSIT LEG CADENCE.  The runner re-steers and runs the arrival test
             * ONLY at leg boundaries — mid-leg it returns early and keeps riding.  A
             * full 64-unit leg at NAV_FOLLOWER_SPEED (200 u/s) is 320 ms of blind
             * travel on a heading computed once at the boundary, which is why the
             * residual at closest approach stayed mostly LATERAL (dist 41, pastPlane
             * -34 => lateral ~23): the entity commits to a line and the destination
             * drifts off it.
             *
             * Cap the TRANSIT leg at the arrival tolerance itself.  BOT_NAV_OMC_ARRIVE_EPS
             * is the radius the transit is judged by, so a leg no longer than it cannot
             * step over the target between two consecutive tests — the boundary lattice
             * becomes finer than the thing it is sampling.  Derived from the shipped
             * tolerance, not a new number, and it halves the blind interval to 160 ms.
             *
             * Transit-only: the non-transit leg below still advances the full
             * NAV_FOLLOWER_LEG_LEN, so the follower's normal traversal cost — the
             * trace-free hot path the header designs around — is unchanged. */
            {
                float legCap = BOT_NAV_OMC_ARRIVE_EPS;
                if ( remaining > 0.0f && remaining < legCap ) legCap = remaining;
                Nav_FollowerSetLeg( ent, headDir, legCap );
            }
        }
        Nav_FollowerGroundClamp( ent );
        G_RunThink( ent );
        return;
    }

    /* set the next straight leg toward the steer direction + ground-clamp it */
    Nav_FollowerSetLeg( ent, out.movedir, NAV_FOLLOWER_LEG_LEN );
    Nav_FollowerGroundClamp( ent );

    /* A behavior monster keeps its own MANIM legsAnim (set by Behavior_UpdateAnim);
     * only stamp the player LEGS_RUN cosmetic on a non-monster nav follower — else the
     * player code clobbers the monster's and the client clamps it to STAND (stoop +
     * slide). See Nav_FollowerStop for the same guard. */
    if ( !Nav_IsBehaviorMonster( ent ) ) {
        ent->s.legsAnim  = LEGS_RUN;
        ent->s.torsoAnim = TORSO_STAND;
    }

    G_RunThink( ent );
}

/*
=================
BotNav_Init
=================
*/
void BotNav_Init( int clientNum )
{
    if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return;

    botNavState_t *bn = &botNavStates[clientNum];
    memset( bn, 0, sizeof(*bn) );
    bn->active = qtrue;
}

/*
=================
BotNav_Shutdown
=================
*/
void BotNav_Shutdown( int clientNum )
{
    if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return;
    botNavStates[clientNum].active = qfalse;
}

/*
=================
BotNav_NeedsRepath — decide whether to request a new path
=================
*/
static qboolean BotNav_NeedsRepath( botNavState_t *bn, const vec3_t goal )
{
    /* 'bn' is mutable because the world-gen test below RETIRES a mismatch it has
     * decided is irrelevant to this corridor (see the fourth trigger). */
    /* No path yet. */
    if ( bn->path.count == 0 ) return qtrue;

    /* Time-based repath interval. */
    float now = (float)trap_Milliseconds() * 0.001f;
    if ( now >= bn->repathTime ) return qtrue;

    /* Goal has moved significantly. */
    vec3_t delta;
    VectorSubtract( goal, bn->lastGoal, delta );
    if ( VectorLength( delta ) > BOT_NAV_GOAL_CHANGE_THRESHOLD ) return qtrue;

    /* The WORLD moved — AND it moved somewhere THIS corridor depends on. The three
     * tests above all observe the agent (its path, its clock, its goal); none
     * observes the graph they were planned against. A mover transition that changes
     * navmesh cost or passability makes a corridor ACROSS THAT MOVER stale by
     * construction — the route that is cheapest NOW is not the one that path was
     * planned for. Re-query while the change is still in effect, rather than waiting
     * out the time interval (which a short mover `wait` can outlive).
     *
     * The crossing test is what keeps this from invalidating the agent that CAUSED
     * the transition: a bot standing on a button holds a corridor that terminates at
     * that button, while the mover that moved is the door the button targets,
     * elsewhere on the map. Its corridor crosses nothing that moved, so it is not
     * invalidated and does not re-commit to the actuation it just completed. See
     * Nav_CorridorTouchesWorldChange.
     *
     * RETIRE an irrelevant mismatch by advancing the stamp. Without this the agent
     * stays permanently mismatched and re-scans its corridor on EVERY step for the
     * rest of the run; with it, each transition costs an exonerated agent exactly one
     * corridor scan. The stamp is only advanced on the exoneration branch — an agent
     * that IS affected falls through to the repath, which re-stamps after a
     * successful query (so a transition landing between test and query still leaves a
     * mismatch for the next step, as before). */
    if ( bn->worldGen != Nav_WorldGen() ) {
        if ( Nav_CorridorTouchesWorldChange( bn ) ) return qtrue;
        bn->worldGen = Nav_WorldGen();
    }

    return qfalse;
}

/*
=================
BotNav_Steer — client-agnostic steering core (see g_bot_nav.h)

Advances 'bn' from 'origin'/'velocity' toward 'goalOrigin' and writes the move
direction + flags into 'out'. No client-input side effects; the caller applies
the result. 'debugClient' identifies the entity in nav_botdebug log lines only.
=================
*/
void BotNav_Steer( botNavState_t *bn, const vec3_t origin, const vec3_t velocity,
                   const vec3_t goalOrigin, int debugClient,
                   botNavSteerResult_t *out )
{
    int cn = debugClient;

    memset( out, 0, sizeof(*out) );

    /* ── Position telemetry (throttled to ~once per sim-second) ─────────────
     * A dense forward-progress signal for the playthrough harness: sample the
     * bot's world position on the same debug stream as the repath lines. Uses
     * level.time (the sim clock the harness pins via fixedtime) so the cadence
     * is deterministic. Reuses the existing nav_botdebug gate — no new cvar. */
    if ( nav_botdebug.integer && level.time - bn->lastPosLogTime >= 1000 ) {
        bn->lastPosLogTime = level.time;
        BotAI_Print( PRT_MESSAGE,
            "[BOTNAV] client %d pos (%.0f %.0f %.0f)\n",
            cn, origin[0], origin[1], origin[2] );
    }

    /* ── STUCK state: single-frame signal, reset to IDLE immediately ────── */
    /* Goal abort (below) sets steeringState = NAV_STEER_STUCK and returns
     * failure.  On the very next call, clear STUCK so the AI layer's new
     * goal can proceed.  The abort window is already cleared, so a new
     * BOT_NAV_GOAL_ABORT_TIME window restarts before another abort fires. */
    if ( bn->steeringState == NAV_STEER_STUCK ) {
        bn->steeringState = NAV_STEER_IDLE;
    }

    /* ── Stuck detection (repath) ───────────────────────────────────────── */
    float now = (float)trap_Milliseconds() * 0.001f;
    /* SAME CLASS as the cursor-advance guard below: this is FREE-WALK progress
     * bookkeeping, and it cannot measure a traversal that takes time.  During an
     * off-mesh transit the agent is under the LINK's physics, not its own steering
     * — a fall suppresses the move entirely and gravity carries it, a plat ride
     * holds it still while the mover translates it, a run-jump arc is ballistic.
     * "Has not moved BOT_NAV_STUCK_DIST horizontally" is therefore the NORMAL
     * state of a transit, not evidence of being stuck, and letting it zero
     * path.count defeats the OMC repath guard from the inside exactly as the
     * cursor advance did (next tick: count==0 → reason "new" → corridor points
     * back at the source → traversal cancelled).
     *
     * A transit is NOT left unleashed: it carries its OWN deadline
     * (bn->omcTransitTimeout, checked in the arrival test below), which is the
     * bound appropriate to a link.  So HOLD this window open for the duration —
     * roll the checkpoint forward without judging it — and let the transit's own
     * leash be the single authority while it is in flight.  On exit the tracker
     * resumes from the arrival position with a full window, so a bot that wedges
     * AFTER landing is still caught on the normal schedule. */
    if ( bn->steeringState == NAV_STEER_OMC_TRANSIT ) {
        VectorCopy( origin, bn->lastPos );
        bn->lastMoveTime = now;
    } else {
        vec3_t moved;
        VectorSubtract( origin, bn->lastPos, moved );
        if ( VectorLength( moved ) >= BOT_NAV_STUCK_DIST ) {
            /* Progressing normally — update checkpoint. */
            VectorCopy( origin, bn->lastPos );
            bn->lastMoveTime = now;
        } else if ( bn->lastMoveTime > 0.0f &&
                    ( now - bn->lastMoveTime ) >= BOT_NAV_STUCK_TIME ) {
            /* Stuck — force a repath but do NOT set failure. */
            bn->path.count = 0;
            bn->lastMoveTime = now;
            VectorCopy( origin, bn->lastPos );
        }
    }

    /* ── Goal-abort stuck detection (FIX-1) ────────────────────────────── */
    /* Independent of the repath-stuck tracker above. If bot has not moved
     * BOT_NAV_GOAL_ABORT_DIST units in BOT_NAV_GOAL_ABORT_TIME seconds while
     * a path is active, signal failure so the AI layer picks a new goal.
     *
     * Held open during a transit for the same reason as the repath-stuck tracker
     * (see above): displacement under link physics is not a progress measure the
     * free-walk trackers can read, and this one is strictly worse to fire — it
     * raises out->failure, which discards the GOAL as well as the corridor.  The
     * transit's own timeout remains the leash while in flight; the abort window
     * re-arms from the arrival position on exit. */
    if ( bn->steeringState == NAV_STEER_OMC_TRANSIT ) {
        VectorCopy( origin, bn->goalAbortCheckPos );
        bn->goalAbortDeadline = now + BOT_NAV_GOAL_ABORT_TIME;
    } else if ( bn->path.count > 0 ) {
        if ( bn->goalAbortDeadline == 0.0f ) {
            /* First frame with an active path — start abort window. */
            VectorCopy( origin, bn->goalAbortCheckPos );
            bn->goalAbortDeadline = now + BOT_NAV_GOAL_ABORT_TIME;
        } else {
            vec3_t abortMoved;
            VectorSubtract( origin, bn->goalAbortCheckPos, abortMoved );
            if ( VectorLength( abortMoved ) >= BOT_NAV_GOAL_ABORT_DIST ) {
                /* Made enough progress — reset abort window. */
                VectorCopy( origin, bn->goalAbortCheckPos );
                bn->goalAbortDeadline = now + BOT_NAV_GOAL_ABORT_TIME;
            } else if ( now >= bn->goalAbortDeadline ) {
                /* Stuck too long — abort goal. */
                if ( nav_botdebug.integer ) {
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] client %d stuck, aborting goal\n", cn );
                }
                bn->path.count = 0;
                bn->goalAbortDeadline = 0.0f;
                bn->steeringState = NAV_STEER_STUCK;
                out->failure = qtrue;
                return;
            }
        }
    } else {
        /* No active path: reset abort window so it re-arms on next path. */
        bn->goalAbortDeadline = 0.0f;
    }

    /* ── Repath if needed ───────────────────────────────────────────────── */
    /* Do NOT repath during OMC_TRANSIT — the bot is mid-traversal and any
     * new path would point back to the source, aborting the traversal. */
    if ( bn->steeringState != NAV_STEER_OMC_TRANSIT && BotNav_NeedsRepath( bn, goalOrigin ) ) {
        /* Determine repath reason for logging. */
        const char *reason;
        if ( bn->path.count == 0 ) {
            reason = "new";
        } else {
            float nowDbg = (float)trap_Milliseconds() * 0.001f;
            if ( nowDbg >= bn->repathTime ) {
                reason = "timer";
            } else {
                vec3_t delta;
                VectorSubtract( goalOrigin, bn->lastGoal, delta );
                if ( VectorLength( delta ) > BOT_NAV_GOAL_CHANGE_THRESHOLD ) {
                    reason = "goal_moved";
                } else if ( bn->worldGen != Nav_WorldGen() ) {
                    /* A mover transition changed the graph under this corridor. */
                    reason = "world_changed";
                } else {
                    reason = "stuck";
                }
            }
        }
        qboolean isTimer = (strcmp(reason, "timer") == 0) ? qtrue : qfalse;

        /* trap takes non-const vec3_t but only reads the query points; the
         * const cast keeps this core's inputs read-only without a copy. */
        int n = trap_Nav_FindPath( (vec_t *)origin, (vec_t *)goalOrigin, bn->agentType, &bn->path );
        if ( n <= 0 ) {
            /* Path query failed — signal failure. */
            bn->path.count = 0;
            out->failure = qtrue;
            return;
        }
        /* Nav_FindPath now packs a reach-status bit into the scalar return (see
         * NAV_FINDPATH_PARTIAL); the follower only wants the waypoint count, so mask
         * it off. bn->path.count already holds the plain count, so path-following is
         * unaffected — this keeps the debug trace and the dedup fingerprint clean. */
        n = NAV_FINDPATH_COUNT( n );

        if ( nav_botdebug.integer ) {
            /* FIX-8: for timer repaths, only log if path actually changed.
             * Non-timer reasons always log (they indicate meaningful events). */
            qboolean pathChanged =
                (n != bn->prevPathCount) ||
                (bn->path.count > 0 &&
                 (bn->path.positions[0][0] != bn->prevFirstWp[0] ||
                  bn->path.positions[0][1] != bn->prevFirstWp[1] ||
                  bn->path.positions[0][2] != bn->prevFirstWp[2]));

            if ( !isTimer || pathChanged ) {
                qboolean hasOmc = qfalse;
                int wi;
                for ( wi = 0; wi < bn->path.count; wi++ ) {
                    if ( bn->path.flags[wi] & NAV_PATHFLAG_OFFMESH_CON ) {
                        hasOmc = qtrue;
                        break;
                    }
                }
                BotAI_Print( PRT_MESSAGE,
                    "[BOTNAV] client %d repath (reason: %s) "
                    "O=(%.0f,%.0f,%.0f) -> G=(%.0f,%.0f,%.0f): %d pts%s\n",
                    cn, reason,
                    origin[0], origin[1], origin[2],
                    goalOrigin[0], goalOrigin[1], goalOrigin[2],
                    n, hasOmc ? " [OMC]" : "" );
            }

            /* Level 2: dump every waypoint on each successful FindPath.
             * Not gated by FIX-8 dedup — verbose mode always shows full path. */
            if ( nav_botdebug.integer >= 2 ) {
                int dbi;
                for ( dbi = 0; dbi < bn->path.count; dbi++ ) {
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV]   wp %d: (%.0f,%.0f,%.0f) flags=0x%02x%s\n",
                        dbi,
                        bn->path.positions[dbi][0],
                        bn->path.positions[dbi][1],
                        bn->path.positions[dbi][2],
                        (unsigned)bn->path.flags[dbi],
                        (bn->path.flags[dbi] & NAV_PATHFLAG_OFFMESH_CON) ? " [OMC]" : "" );
                }
            }
        }

        /* Store path fingerprint for FIX-8 dedup. */
        bn->prevPathCount = n;
        if ( bn->path.count > 0 )
            VectorCopy( bn->path.positions[0], bn->prevFirstWp );

        bn->pathIdx    = 0;
        bn->repathTime = now + BOT_NAV_REPATH_INTERVAL;
        /* This corridor is now planned against the CURRENT graph. Stamping here
         * (rather than at the trigger test) means a transition that lands between
         * the test and the query still leaves a mismatch for the next step. */
        bn->worldGen   = Nav_WorldGen();
        VectorCopy( goalOrigin, bn->lastGoal );
        /* Ensure stuck-timer is live. */
        if ( bn->lastMoveTime == 0.0f ) {
            VectorCopy( origin, bn->lastPos );
            bn->lastMoveTime = now;
        }
        /* Reset goal-abort window on new path (FIX-1). */
        VectorCopy( origin, bn->goalAbortCheckPos );
        bn->goalAbortDeadline = now + BOT_NAV_GOAL_ABORT_TIME;
        /* Start in WALK state; crouch/OMC transitions happen below. */
        bn->steeringState = NAV_STEER_WALK;
    }

    /* ── Advance pathIdx when within advance radius ─────────────────────── */
    /* NOT while a traversal is in flight.  The cursor advance is FREE-WALK
     * bookkeeping: it retires a waypoint the moment the agent is within the
     * advance radius of it.  During an off-mesh traversal the agent is standing
     * ON (or has just left) the OMC source waypoint, so that radius test is
     * satisfied on the FIRST transit tick — the cursor would step past the OMC
     * waypoint, drop the steering state back to WALK and zero path.count, which
     * is exactly the guard at the repath test above being removed from inside.
     * The next tick then repaths with count==0 (reason "new") and the corridor
     * points back at the source, cancelling the traversal.
     *
     * The class: a traversal that TAKES TIME must be ended by its ARRIVAL
     * condition, not by waypoint bookkeeping.  Advancing the path cursor past an
     * off-mesh connection must not, by itself, terminate a transit still in
     * flight.  The arrival test below (dest-reached / past-plane-on-link, plus
     * the timeout leash) is the single exit, and it is length-agnostic: an
     * INSTANTANEOUS link (teleporter, pad) puts the agent at the destination on
     * the same tick, so it satisfies that same test immediately and needs no
     * separate path — while a DURATION-BEARING link (a fall, a plat ride, a
     * run-jump arc) is left alone until the agent actually arrives or the leash
     * expires.  One exit serves both classes; no branch on any traversal mode.
     *
     * Freezing the cursor here is safe because every value the transit consumes
     * was snapshotted into bn->omcTransit{Dest,Lip,StartPos,Timeout}/omcTransitMode
     * at ENTRY — nothing during transit re-reads path.positions[pathIdx]. */
    if ( bn->steeringState != NAV_STEER_OMC_TRANSIT ) {
        /* FIX-3: use nav_waypoint_tolerance cvar (default 32 Q3u) as base
         * advance radius.  Dynamic speed-based lookahead added on top. */
        float baseRadius = nav_waypoint_tolerance.value > 0.0f
                           ? nav_waypoint_tolerance.value
                           : BOT_NAV_MIN_ADVANCE_RADIUS;
        float speed = VectorLength( velocity );
        float advanceRadius = baseRadius;
        float dynamic = speed * 0.15f;  /* 0.15s ≈ 1.5 × 100ms tick */
        if ( dynamic > advanceRadius ) advanceRadius = dynamic;

        /* Walk forward through waypoints that are already "behind" us. */
        while ( bn->pathIdx < bn->path.count - 1 ) {
            vec3_t toWp;
            VectorSubtract( bn->path.positions[bn->pathIdx], origin, toWp );
            if ( VectorLength( toWp ) <= advanceRadius ) {
                if ( nav_botdebug.integer ) {
                    qboolean isOmc = (bn->path.flags[bn->pathIdx] & NAV_PATHFLAG_OFFMESH_CON)
                                     ? qtrue : qfalse;
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] client %d wp %d reached (flags=0x%02x%s)\n",
                        cn, bn->pathIdx,
                        (unsigned)bn->path.flags[bn->pathIdx],
                        isOmc ? " OMC" : "" );
                }
                /* No OMC exit here: this loop no longer runs during a transit
                 * (see the guard above), so a transit can only be ended by the
                 * arrival test.  Passing an OMC-flagged waypoint while NOT in
                 * transit is ordinary cursor movement. */
                bn->pathIdx++;
            } else {
                break;
            }
        }
    }

    /* ── Final waypoint reached ─────────────────────────────────────────── */
    if ( bn->pathIdx < 0 || bn->pathIdx >= bn->path.count ) {
        /* Goal reached (or invalid index): hold position, clear path. */
        bn->path.count = 0;
        bn->steeringState = NAV_STEER_IDLE;
        /* out->failure stays qfalse — caller should decide a new goal. */
        out->done = qtrue;
        return;
    }

    /* ── OMC_TRANSIT: handle jump-pad / teleporter traversal ────────────── */
    if ( bn->steeringState == NAV_STEER_OMC_TRANSIT ) {
        /* Exit on ARRIVAL at the link's destination endpoint (length-agnostic):
         *   (1) within BOT_NAV_OMC_ARRIVE_EPS of the dest, OR
         *   (2) having passed the plane through the dest perpendicular to the link
         *       direction (start->dest), so an overshoot still completes.
         * This replaces the old "moved far enough" displacement thresholds that a
         * short link could never satisfy (the transit loop before t9). */
        /* The arrival arithmetic (distance, along-link progress past the dest plane,
         * and the lateral offset from the link LINE) lives in Nav_OmcArrived so the
         * follower's per-frame ride and this block apply THE SAME rule.  See that
         * function for the geometry; the exit decision below is unchanged. */
        float distToDest, pastPlane, latSq;
        qboolean omcArrived = Nav_OmcArrived( bn, origin, &distToDest, &pastPlane,
                                              &latSq );

        /* Fold this boundary sample into the transit minimum too, so a transit that
         * only ever sees boundary frames still reports a real closest approach. */
        if ( distToDest < bn->omcMinDist ) {
            bn->omcMinDist  = distToDest;
            bn->omcMinPast  = pastPlane;
            bn->omcMinLatSq = latSq;
        }

        /* The throttle slot must be sized for the INDEX SPACE cn actually spans, not
         * for client slots.  cn is the debugClient PARAMETER, and BotNav_Steer has
         * three callers: the mover pawn passes an ENTITY INDEX (ent - g_entities,
         * :364), the bot path passes a client slot (:1334), and the walk-test harness
         * passes -1 (:1694).  Only the middle one is bounded by MAX_CLIENTS, so a
         * MAX_CLIENTS-sized array was written OUT OF BOUNDS by every monster transit
         * on this map class — observed entity ids reached 227 against a bound of 64.
         * MAX_GENTITIES is the shipped bound for an entity index and subsumes the
         * client slots (clients occupy the low entity numbers), so one array serves
         * all three callers.  -1 is excluded explicitly: it must index nothing. */
        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 &&
             cn >= 0 && cn < MAX_GENTITIES ) {
            static float transitLogTime[MAX_GENTITIES];
            if ( now >= transitLogTime[cn] ) {
                transitLogTime[cn] = now + 0.5f;
                BotAI_Print( PRT_MESSAGE,
                    "[BOTNAV] client %d OMC_TRANSIT: distToDest=%.0f pastPlane=%.0f (arrive eps=%.0f)\n",
                    cn, distToDest, pastPlane, (float)BOT_NAV_OMC_ARRIVE_EPS );
            }
        }

        qboolean exitOmc = qfalse;
        /* Dest-arrival: (1) within ARRIVE_EPS of the dest point, OR (2) past the dest
         * plane WHILE STILL ON the link line (lateral offset within ARRIVE_EPS) — a
         * legitimate along-link overshoot completes, but a sideways drift across the
         * plane (large lateral, e.g. the airborne WALK mis-descent at distToDest=46)
         * does not falsely complete short of the dest. */
        if ( omcArrived )
            exitOmc = qtrue;   /* arrived at (or overshot along) the link destination */
        /* Completion witness — the direct counterpart of the "OMC_TRANSIT entered"
         * line, emitted when the transit ends by ARRIVING at the link destination
         * (not by the timeout below).  Entered-vs-completed is the capability
         * measure for an off-mesh link: "entered" alone counts attempts, and a link
         * that is entered but never completed is exactly the failure this pass
         * targets.  Permanent, on the existing nav_botdebug stream — no new cvar. */
        if ( exitOmc && nav_botdebug.integer ) {
            BotAI_Print( PRT_MESSAGE,
                "[BOTNAV] client %d OMC_TRANSIT completed at (%.0f,%.0f,%.0f) dest=(%.0f,%.0f,%.0f) mode=%d\n",
                cn, origin[0], origin[1], origin[2],
                bn->omcTransitDest[0], bn->omcTransitDest[1], bn->omcTransitDest[2],
                (int)bn->omcTransitMode );
        }
        /* FALSIFIED-TRANSIT BAILOUT.  A transit asserts that SOMETHING will carry the
         * bot to the link destination — a pad's velocity, a teleport, a plat under a
         * standing rider, gravity on a fall, or the bot's own run on a self-powered
         * link.  On that assertion the follower hands control to the link and stops
         * steering toward the goal.  When the assertion is false the bot burns the
         * whole leash going nowhere.
         *
         * Test the ASSERTION ITSELF, not a proxy for it: is the distance to the
         * destination still improving?  That is what "something is carrying me there"
         * means, and it is true of every mode, link and map — so this needs no list of
         * which link types may lie, and no new list has to be maintained when a
         * producer or bake rule changes.
         *
         * An earlier cut of this test used displacement from the ENTRY POINT and was
         * inert: RIDE clears suppression before boarding and walks the bot toward the
         * board point, so it displaced well past the tolerance while distToDest stayed
         * pinned at 434 for an entire transit.  Moving is not the same as making
         * progress; only the watermark distinguishes them.
         *
         * A genuine mechanism improves the watermark continuously — a pad arc, a
         * teleport (which lands instantly and far), a plat ride, a gravity fall — so
         * the predicate stays silent on them.  The stall window is DERIVED, not
         * invented: half the transit leash the link is already judged by, which is the
         * same fraction the FALL phase-A cap uses.  Improvement is counted against
         * ARRIVE_EPS, the tolerance already deciding arrival, so a bot creeping within
         * noise of its best does not count as progress.
         *
         * On falsification, take the SAME exit the timeout already takes: end the
         * transit and repath from the current position.  That is the least destructive
         * option in the shipped vocabulary — it neither discards the goal (as
         * out->failure would) nor teleports or nudges the bot.  It changes only WHEN a
         * doomed transit ends, never what happens after it, so a transit that would
         * have completed is unaffected and one that would have timed out simply stops
         * wasting the remaining leash. */
        if ( !exitOmc ) {
            if ( distToDest < bn->omcBestToDest - BOT_NAV_OMC_ARRIVE_EPS ) {
                bn->omcBestToDest     = distToDest;
                bn->omcBestToDestTime = now;
            } else if ( now - bn->omcBestToDestTime >=
                        BOT_NAV_OMC_TRANSIT_TIMEOUT * 0.5f ) {
                if ( nav_botdebug.integer ) {
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] client %d OMC_TRANSIT not closing "
                        "(distToDest=%.0f best=%.0f mode=%d) — releasing\n",
                        cn, distToDest, bn->omcBestToDest,
                        (int)bn->omcTransitMode );
                }
                exitOmc = qtrue;
            }
        }
        if ( now >= bn->omcTransitTimeout ) {
            if ( nav_botdebug.integer ) {
                BotAI_Print( PRT_MESSAGE,
                    "[BOTNAV] client %d OMC_TRANSIT timeout, forcing repath\n", cn );
            }
            exitOmc = qtrue;
        }
        if ( exitOmc ) {
            /* TRANSIT-END REPORT — the transit's TRUE closest approach, with the
             * geometry AT that minimum.  Every previous instrument in this arc could
             * only fire at a leg boundary, and in practice that meant entry: a run of
             * 36 samples carried pastPlane == -distToDest on all 36.  omcMin* is
             * sampled per frame, so `d+p` here is NOT forced to ~0 and a reader can
             * tell a genuine mid-transit minimum from an entry sample. */
            if ( nav_botdebug.integer ) {
                BotAI_Print( PRT_MESSAGE,
                    "[BOTNAV] client %d OMC_TRANSIT end %s: minDist=%.0f "
                    "pastPlane=%.0f latSq=%.0f d+p=%.0f mode=%d (arrive eps=%.0f)\n",
                    cn, omcArrived ? "COMPLETED" : "ABANDONED",
                    bn->omcMinDist, bn->omcMinPast, bn->omcMinLatSq,
                    bn->omcMinDist + bn->omcMinPast, (int)bn->omcTransitMode,
                    (float)BOT_NAV_OMC_ARRIVE_EPS );
            }
            bn->steeringState = NAV_STEER_WALK;
            bn->path.count = 0;   /* repath from new position */
            out->holdFrame = qtrue; /* no move this frame; repath happens next */
            return;               /* let repath happen next frame */
        }
        /* During transit: keep moving toward the OMC destination waypoint. */
    }

    /* ── Enter OMC_TRANSIT when stepping onto an OMC source ─────────────── */
    if ( bn->steeringState != NAV_STEER_OMC_TRANSIT ) {
        int curIdx = bn->pathIdx;
        if ( curIdx >= 0 && curIdx < bn->path.count &&
             (bn->path.flags[curIdx] & NAV_PATHFLAG_OFFMESH_CON) ) {
            vec3_t toWp;
            VectorSubtract( bn->path.positions[curIdx], origin, toWp );
            float distToOmc = VectorLength( toWp );
            if ( distToOmc <= BOT_NAV_OMC_APPROACH_DIST ) {
                bn->steeringState = NAV_STEER_OMC_TRANSIT;
                VectorCopy( origin, bn->omcTransitStartPos );
                bn->omcTransitTimeout = now + BOT_NAV_OMC_TRANSIT_TIMEOUT;
                /* Capture the link's traversal MODE (producer-stamped, on the OMC
                 * source waypoint) + the lip (source endpoint) for the caller's
                 * per-mode actuation.  These are inert data — no entity touched. */
                bn->omcTransitMode = bn->path.traversalMode[curIdx];
                bn->omcFallPhaseB  = qfalse;
                bn->omcJumpFired   = qfalse;   /* CLIMB one-shot: fresh per transit */
                /* Closest-approach tracker: fresh per transit, seeded so the first
                 * real sample always wins.  omcTransitDest is assigned below, so the
                 * minimum is only ever updated once a dest exists. */
                bn->omcMinDist     = 1.0e9f;
                bn->omcMinPast     = 0.0f;
                bn->omcMinLatSq    = 0.0f;
                VectorCopy( bn->path.positions[curIdx], bn->omcTransitLip );
                /* FALL phase-A progress watermark, seeded at the entry HORIZONTAL
                 * distance to the lip.  The phase-A cap measures progress toward the
                 * lip (the objective), so it must start from wherever the transit
                 * legitimately began — anywhere inside APPROACH_DIST. */
                {
                    float dxLip = bn->path.positions[curIdx][0] - origin[0];
                    float dyLip = bn->path.positions[curIdx][1] - origin[1];
                    bn->omcFallBestToLip = sqrtf( dxLip * dxLip + dyLip * dyLip );
                }
                /* Record the link's DESTINATION endpoint — the arrival target the
                 * transit-exit test measures against.  In Detour's straight path an
                 * off-mesh connection is emitted as an entry point (curIdx, flagged
                 * OFFMESH_CON) followed by its exit point (curIdx+1); the exit point
                 * is where the link deposits the bot.  If there is no following
                 * waypoint (the OMC lands on the final goal), the OMC waypoint itself
                 * is the destination. */
                if ( curIdx + 1 < bn->path.count )
                    VectorCopy( bn->path.positions[curIdx + 1], bn->omcTransitDest );
                else
                    VectorCopy( bn->path.positions[curIdx], bn->omcTransitDest );
                /* Seed the transit progress watermark from the entry distance, the
                 * same way the FALL phase-A watermark is seeded above. */
                bn->omcBestToDest     = Distance( origin, bn->omcTransitDest );
                bn->omcBestToDestTime = now;
                if ( nav_botdebug.integer ) {
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] client %d OMC_TRANSIT entered at (%.0f,%.0f,%.0f) dest=(%.0f,%.0f,%.0f) mode=%d\n",
                        cn, origin[0], origin[1], origin[2],
                        bn->omcTransitDest[0], bn->omcTransitDest[1], bn->omcTransitDest[2],
                        (int)bn->omcTransitMode );
                }
            } else if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 &&
                        cn >= 0 && cn < MAX_GENTITIES ) {
                /* Same index-space correction as the transit throttle above: cn may be
                 * an entity index, a client slot, or -1.  MAX_GENTITIES bounds all of
                 * them; -1 indexes nothing. */
                static float approachLogTime[MAX_GENTITIES];
                if ( now >= approachLogTime[cn] ) {
                    approachLogTime[cn] = now + 0.5f;
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] client %d OMC approach: dist=%.0f threshold=%.0f wp=(%.0f,%.0f,%.0f)\n",
                        cn, distToOmc, (float)BOT_NAV_OMC_APPROACH_DIST,
                        bn->path.positions[curIdx][0],
                        bn->path.positions[curIdx][1],
                        bn->path.positions[curIdx][2] );
                }
            }
        }
    }

    /* ── Crouch look-ahead (check current + next waypoint) ──────────────── */
    /* Per plan §2.2: if EITHER pathIdx or pathIdx+1 has NAVPOLY_LOW_CEILING,
     * enter CROUCH now (before the bot is inside the duct).
     * Exit CROUCH only when BOTH are non-LOW_CEILING (hysteresis). */
    if ( bn->steeringState == NAV_STEER_WALK ||
         bn->steeringState == NAV_STEER_CROUCH ) {
        int curIdx  = bn->pathIdx;
        int nextIdx = (curIdx + 1 < bn->path.count) ? curIdx + 1 : curIdx;
        int curFlags  = (curIdx  >= 0 && curIdx  < bn->path.count) ?
                        trap_Nav_GetPolyAreaFlags( bn->path.polyrefs[curIdx] )  : 0;
        int nextFlags = (nextIdx != curIdx && nextIdx >= 0) ?
                        trap_Nav_GetPolyAreaFlags( bn->path.polyrefs[nextIdx] ) : curFlags;

        bn->currentArea = curFlags;
        bn->nextArea    = nextFlags;

        qboolean curLow  = (curFlags  & NAVPOLY_LOW_CEILING) ? qtrue : qfalse;
        qboolean nextLow = (nextFlags & NAVPOLY_LOW_CEILING) ? qtrue : qfalse;

        if ( bn->steeringState == NAV_STEER_WALK ) {
            if ( curLow || nextLow )
                bn->steeringState = NAV_STEER_CROUCH;
        } else { /* CROUCH */
            if ( !curLow && !nextLow )
                bn->steeringState = NAV_STEER_WALK;
        }
    }

    /* ── Steer toward current waypoint ─────────────────────────────────── */
    // NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) — pathIdx is bounded above and below by the guard at line 300; the analyzer can't follow the value across the OMC_TRANSIT block
    float *wp = bn->path.positions[bn->pathIdx];

    /* FIX-2: OMC approach — when next waypoint is an OMC source and bot is
     * within BOT_NAV_OMC_APPROACH_DIST, overshoot by BOT_NAV_OMC_OVERSHOOT
     * to ensure the bot's bbox fully enters the trigger brush. */
    float omcExtended[3];
    if ( bn->path.flags[bn->pathIdx] & NAV_PATHFLAG_OFFMESH_CON ) {
        vec3_t toWp;
        VectorSubtract( wp, origin, toWp );
        float distToOmc = VectorLength( toWp );
        if ( distToOmc <= BOT_NAV_OMC_APPROACH_DIST && distToOmc > 0.1f ) {
            /* Extend steering target past the OMC waypoint. */
            float inv = 1.0f / distToOmc;
            omcExtended[0] = wp[0] + toWp[0] * inv * BOT_NAV_OMC_OVERSHOOT;
            omcExtended[1] = wp[1] + toWp[1] * inv * BOT_NAV_OMC_OVERSHOOT;
            omcExtended[2] = wp[2];
            wp = omcExtended;
        }
    }

    vec3_t dir;
    VectorSubtract( wp, origin, dir );
    dir[2] = 0;  /* Flatten: bot uses jump/gravity for vertical, not steering. */
    VectorNormalize( dir );

    /* Steering result — caller applies (EA move gated on !suppressMove, crouch
     * gated on ->crouch).  The CORE keeps the pre-mode behavior — suppressMove for
     * the whole transit — so the harness (cn=-1) and mover pawn (NULL client), which
     * call this core directly, are BIT-IDENTICAL to today (the golden's protection).
     * A real-client caller reads the exported mode/dest/lip below and OVERRIDES the
     * actuation per mode; the core itself touches no entity. */
    out->suppressMove = ( bn->steeringState == NAV_STEER_OMC_TRANSIT );
    out->crouch       = ( bn->steeringState == NAV_STEER_CROUCH );
    out->omcTransit   = ( bn->steeringState == NAV_STEER_OMC_TRANSIT );
    out->omcMode      = bn->omcTransitMode;
    VectorCopy( bn->omcTransitDest, out->omcDest );
    VectorCopy( bn->omcTransitLip,  out->omcLip );

    /* nav_botdebug 3: per-frame steering state. */
    if ( nav_botdebug.integer >= 3 ) {
        static const char *const steerNames[] = {
            "IDLE", "WALK", "CROUCH", "OMC_TRANSIT", "STUCK"
        };
        int stIdx = (bn->steeringState >= 0 && bn->steeringState <= 4)
                    ? bn->steeringState : 0;
        BotAI_Print( PRT_MESSAGE,
            "[BOTNAV] client %d frame state=%s wp=%d/(%.0f,%.0f,%.0f)\n",
            cn, steerNames[stIdx], bn->pathIdx,
            bn->path.positions[bn->pathIdx][0],
            bn->path.positions[bn->pathIdx][1],
            bn->path.positions[bn->pathIdx][2] );
    }

    /* Fill the steer direction for the caller to apply. */
    VectorCopy( dir, out->movedir );
}

/*
=================
BotNav_MoveToGoal — called in place of trap_BotMoveToGoal()

Thin client wrapper over BotNav_Steer: resolves this bot's nav-state slot,
computes the steer from the bot's position/velocity/goal, then applies the
result through the bot's EA usercmd pipeline (the only client-keyed part).
=================
*/
void BotNav_MoveToGoal( bot_state_t *bs, bot_goal_t *goal,
                         bot_moveresult_t *result )
{
    botNavSteerResult_t steer;

    memset( result, 0, sizeof(*result) );

    if ( !bs || !goal ) {
        result->failure = qtrue;
        return;
    }

    int cn = bs->client;
    if ( cn < 0 || cn >= MAX_CLIENTS ) {
        result->failure = qtrue;
        return;
    }

    botNavState_t *bn = &botNavStates[cn];

    if ( !bn->active ) {
        result->failure = qtrue;
        return;
    }

    BotNav_Steer( bn, bs->origin, bs->velocity, goal->origin, cn, &steer );

    if ( steer.failure ) {
        result->failure = qtrue;
        return;
    }

    /* Goal reached, off-mesh traversal exit, or any step that produced no move:
     * apply nothing this frame — matches the pre-split early returns. */
    if ( steer.done || steer.holdFrame ) {
        return;
    }

    /* ── Per-mode OMC actuation (real-client only) ──────────────────────────
     * The steer core exported omcTransit/omcMode/omcDest/omcLip and set its
     * suppressMove for the whole transit (bit-identical to today).  Here — and
     * ONLY for a real client with a live ps (so groundEntityNum is valid) — resolve
     * the per-mode actuation and OVERRIDE the move/suppress.  The gate excludes the
     * navval harness (cn<0) and the mover pawn (cn>=0 but NULL client): those never
     * reach this caller with a real ps, so their behavior is unchanged and the golden
     * holds.  A move-vector override is computed here and applied via EA. */
    qboolean applySuppress = steer.suppressMove;   /* default = core's decision */
    qboolean requestJump   = qfalse;               /* CLIMB one-shot launch this frame */
    vec3_t   applyDir;
    VectorCopy( steer.movedir, applyDir );
    qboolean realClient = ( bs->client >= 0 && bs->client < MAX_CLIENTS &&
                            g_entities[bs->client].client != NULL );
    if ( realClient && steer.omcTransit ) {
        int groundEnt = bs->cur_ps.groundEntityNum;
        qboolean onGround = ( groundEnt != ENTITYNUM_NONE );
        /* horizontal unit vector from the bot's CURRENT origin to the link dest —
         * used by WALK, FALL phase A, and RIDE (never the stale entry startPos). */
        vec3_t toDest;
        VectorSubtract( steer.omcDest, bs->origin, toDest );
        toDest[2] = 0.0f;
        qboolean haveDir = ( VectorNormalize( toDest ) > 0.0f );

        switch ( steer.omcMode ) {
        case NAV_TM_WALK:
            /* A level crossing physics does not carry — WALK to the dest.  But the
             * traversal mode is what the bot's PHYSICAL STATE is, not just the producer
             * stamp: a WALK link the bot enters AIRBORNE (arriving mid-air from a prior
             * drop/climb OMC) whose dest is BELOW it is, for this moment, a FALL — the
             * horizontal air-control cannot carry an airborne bot DOWN to the low dest
             * (it drifts past the dest plane and away, or overshoots).  In that state
             * hand off to gravity exactly as FALL phase-B does (suppress the move, let
             * the drop complete at the dest).  A grounded WALK, or one whose dest is
             * level/higher, stays a normal flat WALK.  Runtime-signal-keyed (airborne +
             * dest-below), not link-id or coordinate specific; groundEnt is only valid
             * for a real client (the harness/mover gating above), so this cannot change
             * the golden. */
            if ( !onGround && steer.omcDest[2] < bs->origin[2] ) {
                applySuppress = qtrue;   /* airborne toward a lower dest → gravity carries (FALL phase-B) */
            } else {
                applySuppress = qfalse;
                if ( haveDir ) VectorCopy( toDest, applyDir );
                /* ...and the MIRROR case, which this arm never covered.
                 *
                 * The comment above already commits this site to the bot's PHYSICAL
                 * STATE over the producer stamp, and implements one half: airborne +
                 * dest-below is really a FALL.  The other half is a GROUNDED bot whose
                 * dest sits ABOVE its own floor by more than it can step up.  applyDir
                 * is FLATTENED (toDest[2] = 0 above), so this arm can only ever push
                 * horizontally — it walks the bot into the base of the rise forever.
                 *
                 * CLASSIFICATION IS NOT THE BUG, AND THE BAKE IS LEFT ALONE.  The link
                 * mode comes from `reqRise = endFootZ - startFootZ` measured at the
                 * LINK'S OWN endpoints (nav_impl.cpp:3060-3065), and for a roughly
                 * level link WALK is correct: a bot standing AT the link's start really
                 * would walk it.  An earlier attempt re-stamped the mode and regressed,
                 * because that breaks the link for every traverser who approaches it
                 * properly.  The static link geometry and the arriving traverser's
                 * actual rise are DIFFERENT QUESTIONS, and only the second one belongs
                 * in the steer layer — so this is decided here, per transit, from the
                 * bot's own origin, and the bake is untouched.
                 *
                 * Measured on e1m1's northbound link (342,490,57)->(342,658,75) under
                 * total exit focus: the bot stands at z=24 facing a dest at z=75, a 51u
                 * rise; 15 of 15 transits ABANDONED at minDist 57-61 with ZERO run-jumps
                 * aimed at that dest, while the southbound CLIMB link completed 49/49
                 * from the same floor.  Same bot, same frame budget — the only
                 * difference was which arm ran.
                 *
                 * The threshold is the agent's own max step height, NAV_DEFAULT_AGENT's
                 * maxClimb (18.0f) — the same value the bake's WALK/CLIMB split uses
                 * (NAVTRAJ_STEPSIZE, itself the engine STEPSIZE) and the same shipped
                 * physics extent this file already cites for the agent radius.  No new
                 * constant.  A level or descending WALK is untouched, and the
                 * airborne/dest-below case above still wins because it is tested first. */
                {
                    const navAgentParams_t wiAgent = NAV_DEFAULT_AGENT;
                    /* Report the rise the gate ACTUALLY sees, once per transit, before
                     * any conjunct can reject it.  The margin here is thin — the
                     * measured entry rise is 19 against an 18 threshold — so a silent
                     * non-firing must be attributable to a named conjunct rather than
                     * guessed at across four of them. */
                    if ( nav_botdebug.integer && !bn->omcJumpFired ) {
                        static float s_riseLog[MAX_CLIENTS];
                        if ( bs->client >= 0 && bs->client < MAX_CLIENTS &&
                             FloatTime() - s_riseLog[bs->client] > 1.0f ) {
                            vec3_t lipFlat;
                            s_riseLog[bs->client] = FloatTime();
                            VectorSubtract( steer.omcLip, bs->origin, lipFlat );
                            lipFlat[2] = 0.0f;
                            BotAI_Print( PRT_MESSAGE,
                                "[BOTNAV] cl=%d WALK-rise gate: rise=%.0f (need>%.0f) onGround=%d toLip=%.0f (need<=%.0f)\n",
                                bs->client, steer.omcDest[2] - bs->origin[2],
                                wiAgent.maxClimb, onGround ? 1 : 0,
                                VectorLength( lipFlat ), (float)BOT_NAV_OMC_ARRIVE_EPS );
                        }
                    }
                    if ( onGround && !bn->omcJumpFired &&
                         steer.omcDest[2] - bs->origin[2] > wiAgent.maxClimb ) {
                        /* Reuse the shipped CLIMB launch preconditions unchanged —
                         * running with momentum (vh), heading within the same 50° cone,
                         * the same one-shot latch — and gate the launch on proximity to
                         * the LAUNCH POINT.
                         *
                         * A first attempt gated on |origin - dest| <= ARRIVE_EPS, which
                         * is unsatisfiable as a launch test: being within ARRIVE_EPS of
                         * the destination IS the arrival condition, so a jump gated on
                         * it can only fire once the traversal has already succeeded.
                         * Measured: entry distances 202 and 136 against a 32 gate, and
                         * 0 firings.  The lip and the dest are opposite ends of the
                         * link; the CLIMB arm's toBase means the former.
                         *
                         * omcLip is the link's own source waypoint (path.positions at
                         * the OMC index) — the position the bot approaches along the
                         * route, and the correct end to launch from. */
                        float vh = sqrtf( bs->cur_ps.velocity[0]*bs->cur_ps.velocity[0] +
                                          bs->cur_ps.velocity[1]*bs->cur_ps.velocity[1] );
                        if ( haveDir && vh > 100.0f ) {
                            float wantYaw = (float)( atan2( toDest[1], toDest[0] ) * 180.0 / M_PI );
                            float haveYaw = (float)( atan2( bs->cur_ps.velocity[1],
                                                            bs->cur_ps.velocity[0] ) * 180.0 / M_PI );
                            float dYaw = AngleSubtract( wantYaw, haveYaw );
                            if ( dYaw < 0 ) dYaw = -dYaw;
                            if ( dYaw <= 50.0f ) {
                                vec3_t flatToLip;
                                float  toLipDist;
                                VectorSubtract( steer.omcLip, bs->origin, flatToLip );
                                flatToLip[2] = 0.0f;
                                toLipDist = VectorLength( flatToLip );
                                if ( toLipDist <= BOT_NAV_OMC_ARRIVE_EPS ) {
                                    requestJump      = qtrue;
                                    bn->omcJumpFired = qtrue;   /* one-shot, same latch */
                                    if ( nav_botdebug.integer )
                                        BotAI_Print( PRT_MESSAGE,
                                            "[BOTNAV] cl=%d WALK-rise climb at (%.0f %.0f %.0f) -> dest (%.0f %.0f %.0f) rise=%.0f toLip=%.0f vh=%.0f\n",
                                            bs->client, bs->origin[0], bs->origin[1], bs->origin[2],
                                            steer.omcDest[0], steer.omcDest[1], steer.omcDest[2],
                                            steer.omcDest[2] - bs->origin[2], toLipDist, vh );
                                }
                            }
                        }
                    }
                }
            }
            break;

        case NAV_TM_FALL: {
            /* Two-phase gravity drop, gated on ground contact.
             *  Phase A (still on the lip): drive OFF the edge — toward the lip
             *   waypoint, else the horizontal dest vector — until the feet clear.
             *  Phase B (airborne): suppress, gravity carries — bit-identical to the
             *   proven z-196 drop; dest-arrival completes.
             *
             * PHASE-A CAP — measured against the OBJECTIVE (the lip), not against the
             * transit's start point.  The old cap compared distance TRAVELLED since
             * entry against ARRIVE_EPS (32u), while entry is permitted anywhere inside
             * APPROACH_DIST (64u): a transit legitimately entered 55u from the lip
             * exhausted the budget after 32u of CORRECT walking, still 23u short of the
             * edge, and a suppressed bot on solid ground never moves again — gravity
             * never gets its handoff.  A progress budget must be measured against the
             * objective; a "distance travelled" latch is only valid when the distance
             * to the objective is bounded by the same quantity, which it is not here.
             *
             * Lip-relative cap, two exits, both bounded, neither start-relative:
             *   (1) ARRIVED — the bot is horizontally at (or past) the lip while still
             *       grounded.  The drive-off is DONE; hand to gravity now.  "At the
             *       lip" is the AGENT RADIUS (NAV_DEFAULT_AGENT.radius, 15u) and
             *       nothing more: the bot's center cannot occupy the lip point itself,
             *       so one radius IS arrival — the same setback identity the CLIMB
             *       step-base uses.  It must NOT be inflated by ARRIVE_EPS: entry is
             *       permitted anywhere inside APPROACH_DIST (64u), so an EPS+radius
             *       (47u) threshold fires on the entry frame for any transit entered
             *       beyond 47u — measured, that re-created the identical wedge with a
             *       different number (bot frozen at 45u from the lip, latching on the
             *       entry frame, 0 completions).  An arrival test must be bounded by
             *       the agent's own extent, never by the entry radius.
             *   (2) NO PROGRESS — the bot's horizontal distance to the lip has not
             *       improved on its best-ever value by more than one arrival tolerance
             *       (ARRIVE_EPS).  THIS IS THE ANTI-WEDGE PROPERTY, and it is strictly
             *       stronger than the old cap: a mis-azimuthed bot walks AWAY from the
             *       lip, so its distance grows monotonically and it trips this the
             *       moment it has drifted one tolerance past its own watermark —
             *       bounded by ARRIVE_EPS of wrong-way travel, whereas the old cap
             *       allowed ARRIVE_EPS of travel in ANY direction including circling.
             *       A bot walking the RIGHT way keeps lowering the watermark and is
             *       never capped, however far outside 32u it legitimately entered.
             *       The watermark is monotone non-increasing, so the wedge terminates
             *       in finite time regardless of entry distance; the 3.0s transit
             *       leash remains the outer backstop, untouched. */
            if ( bn->omcFallPhaseB || !onGround ) {
                /* Latch cause witness, emitted ONCE per transit on the transition
                 * into phase B.  "clearance" = the feet genuinely left the ground
                 * (the intended handoff); the cap causes below name themselves.
                 * Permanent, on the existing nav_botdebug stream — no new cvar. */
                if ( !bn->omcFallPhaseB && nav_botdebug.integer ) {
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] cl=%d FALL phaseB cause=clearance at (%.0f %.0f %.0f)\n",
                        bs->client, bs->origin[0], bs->origin[1], bs->origin[2] );
                }
                bn->omcFallPhaseB = qtrue;   /* latch: never walk again this transit */
                applySuppress = qtrue;
            } else {
                /* Horizontal distance to the lip — the objective.  Horizontal because
                 * the lip is an EDGE the bot walks off: the z difference between the
                 * bot's origin and the lip waypoint is the drop itself, not an error. */
                float dxLip = steer.omcLip[0] - bs->origin[0];
                float dyLip = steer.omcLip[1] - bs->origin[1];
                float toLipDist = sqrtf( dxLip * dxLip + dyLip * dyLip );
                if ( toLipDist < bn->omcFallBestToLip )
                    bn->omcFallBestToLip = toLipDist;   /* watermark: monotone down */

                /* (1) arrived at the lip while grounded, or (2) no progress toward it */
                qboolean atLip     = ( toLipDist <= 15.0f /* agent radius — the bot's center cannot reach the lip point itself */ );
                qboolean noProgress= ( toLipDist >= bn->omcFallBestToLip + BOT_NAV_OMC_ARRIVE_EPS );
                if ( atLip || noProgress ) {
                    if ( nav_botdebug.integer ) {
                        BotAI_Print( PRT_MESSAGE,
                            "[BOTNAV] cl=%d FALL phaseB cause=%s at (%.0f %.0f %.0f) toLip=%.0f best=%.0f\n",
                            bs->client, atLip ? "atlip" : "noprogress",
                            bs->origin[0], bs->origin[1], bs->origin[2],
                            toLipDist, bn->omcFallBestToLip );
                    }
                    bn->omcFallPhaseB = qtrue;   /* lip reached, or not approaching it */
                    applySuppress = qtrue;
                } else {
                    applySuppress = qfalse;
                    /* drive toward the lip edge (drop off it), else the dest vector */
                    vec3_t toLip;
                    VectorSubtract( steer.omcLip, bs->origin, toLip );
                    toLip[2] = 0.0f;
                    if ( VectorNormalize( toLip ) > 0.0f )      VectorCopy( toLip, applyDir );
                    else if ( haveDir )                          VectorCopy( toDest, applyDir );
                }
            }
            break;
        }

        case NAV_TM_RIDE: {
            /* Board-then-ride: WALK onto the plat until the bot stands ON it
             * (groundEntityNum == a mover), then suppress so the mover translates the
             * standing rider (a continuous EA_Move would break rider contact via
             * G_TryPushingEntity).  Board by walking toward the plat's BOARD point —
             * the OMC lip/source (the plat's rest position), NOT the dest (the far
             * end of the ride, which is 400u away underground).  Dest-arrival fires
             * when the plat delivers the rider to the other endpoint. */
            qboolean boarded = ( onGround && groundEnt >= 0 && groundEnt < ENTITYNUM_WORLD &&
                                 ( g_entities[groundEnt].s.eType == ET_MOVER ) );
            if ( boarded ) {
                applySuppress = qtrue;   /* riding — hold, the plat carries us */
            } else {
                applySuppress = qfalse;  /* not yet on the plat — walk to the board point */
                vec3_t toBoard;
                VectorSubtract( steer.omcLip, bs->origin, toBoard );
                toBoard[2] = 0.0f;
                if ( VectorNormalize( toBoard ) > 0.0f )     VectorCopy( toBoard, applyDir );
                else if ( haveDir )                           VectorCopy( toDest, applyDir );
            }
            break;
        }

        case NAV_TM_CLIMB: {
            /* Timed run-jump UP a ledge (Landing 2 — the one genuinely-new actuation).
             * Run up toward the dest, ALIGN the heading first (air-control cannot fix
             * a jump launched off-heading), and fire EA_Jump exactly ONCE when
             * grounded AND at the step base AND facing the dest.  Gravity + the jump
             * impulse then carry the arc; the air-control EA_Move continues; dest-
             * arrival completes.  The 3s timeout is the backstop. */
            applySuppress = qfalse;                 /* run-up + air-control both move */
            if ( haveDir ) VectorCopy( toDest, applyDir );
            /* Facing test vs the direction the bot is COMMANDED to move (applyDir =
             * toward dest).  Use cur_ps.velocity heading when the bot is actually
             * running (its momentum is the launch direction gravity+impulse extend);
             * on a near-stationary frame fall back to the commanded applyDir azimuth.
             * A generous cone — a run-jump tolerates aim slop, but a launch pointed
             * away from the dest falls short (the reviewer's precondition). */
            /* A run-jump needs FORWARD MOMENTUM toward the dest: a jump fired from a
             * standstill goes straight up and falls back (the arc clears the height
             * but not the horizontal gap — measured).  So "facing dest" here means the
             * bot is actually RUNNING toward the dest: its horizontal velocity is above
             * a run threshold AND its heading is within a cone of the dest.  Below the
             * run threshold the bot has no launch momentum and must not fire yet. */
            qboolean facingDest = qfalse;
            float vh = sqrtf( bs->cur_ps.velocity[0]*bs->cur_ps.velocity[0] +
                              bs->cur_ps.velocity[1]*bs->cur_ps.velocity[1] );
            if ( haveDir && vh > 100.0f ) {   /* running (not a standstill launch) */
                float wantYaw = (float)( atan2( toDest[1], toDest[0] ) * 180.0 / M_PI );
                float haveYaw = (float)( atan2( bs->cur_ps.velocity[1], bs->cur_ps.velocity[0] ) * 180.0 / M_PI );
                float dYaw = AngleSubtract( wantYaw, haveYaw );
                if ( dYaw < 0 ) dYaw = -dYaw;
                facingDest = ( dYaw <= 50.0f );     /* running within a 50° cone of dest */
            }
            /* At the step base — the point the run-jump launches from.  The follower
             * centers the bot ~1 agent radius back from the eroded mesh lip (the
             * reviewer's setback), so the bot's closest sustained approach to the lip
             * is a radius short; requiring it INSIDE ARRIVE_EPS of the raw lip would
             * never fire.  The step base is therefore the lip TOWARD the bot by the
             * agent radius (a fixed physics extent — NAV_DEFAULT_AGENT.radius, not a
             * per-map tune): fire within ARRIVE_EPS of THAT.  Equivalent to
             * "within ARRIVE_EPS + agentR of the raw lip". */
            float toBase = Distance( bs->origin, bn->omcTransitLip ) - 15.0f /* agent radius */;
            if ( toBase < 0.0f ) toBase = 0.0f;
            if ( onGround && !bn->omcJumpFired && toBase <= BOT_NAV_OMC_ARRIVE_EPS && facingDest ) {
                requestJump = qtrue;
                bn->omcJumpFired = qtrue;   /* one-shot */
                if ( nav_botdebug.integer )
                    BotAI_Print( PRT_MESSAGE, "[BOTNAV] cl=%d CLIMB run-jump at (%.0f %.0f %.0f) -> dest (%.0f %.0f %.0f) vh=%.0f\n",
                        bs->client, bs->origin[0], bs->origin[1], bs->origin[2],
                        steer.omcDest[0], steer.omcDest[1], steer.omcDest[2], vh );
            }
            break;
        }

        case NAV_TM_BALLISTIC:
        default:
            /* BALLISTIC: the pad/teleport carries — suppress the whole transit
             * (bit-identical to the pre-mode code).  Keep the core's suppress. */
            applySuppress = qtrue;
            break;
        }
    }

    /* Apply the (possibly mode-overridden) steer through the EA pipeline. */
    if ( !applySuppress ) {
        trap_EA_Move( bs->client, applyDir, 400 );
    }

    /* CLIMB one-shot launch (real-client-gated inside the switch above). */
    if ( requestJump ) {
        trap_EA_Jump( bs->client );
    }

    /* Crouch command when in low-ceiling passages. */
    if ( steer.crouch ) {
        trap_EA_Crouch( bs->client );
    }
    /* the applied direction is what the yaw + result should reflect */
    VectorCopy( applyDir, steer.movedir );

    /* Update ideal yaw toward waypoint. */
    vec3_t wpAngles;
    vectoangles( steer.movedir, wpAngles );
    bs->ideal_viewangles[YAW]   = wpAngles[YAW];
    bs->ideal_viewangles[PITCH] = wpAngles[PITCH];
    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 )
        BotAI_Print( PRT_MESSAGE, "[AimSet/nav-wp] client %d pitch=%.1f yaw=%.1f\n",
                     bs->client, bs->ideal_viewangles[PITCH], bs->ideal_viewangles[YAW] );

    /* Fill result movedir for callers that read it. */
    VectorCopy( steer.movedir, result->movedir );
}

/*
=================
BotNav_MovementViewTarget — Recast replacement for trap_BotMovementViewTarget
=================
*/
qboolean BotNav_MovementViewTarget( int clientNum, const bot_goal_t *goal,
                                    vec3_t targetOut )
{
    if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return qfalse;
    botNavState_t *bn = &botNavStates[clientNum];
    if ( !bn->active ) return qfalse;

    /* Return the waypoint one step ahead of current pathIdx so the bot
     * looks toward where it is going rather than straight down at its feet. */
    if ( bn->path.count > 0 ) {
        int nextIdx = (bn->pathIdx + 1 < bn->path.count) ?
                      bn->pathIdx + 1 : bn->pathIdx;
        VectorCopy( bn->path.positions[nextIdx], targetOut );
        return qtrue;
    }

    /* No path — aim at goal directly. */
    if ( goal ) {
        VectorCopy( goal->origin, targetOut );
        return qtrue;
    }
    return qfalse;
}

/*
=================
BotNav_PredictEnemyPosition
=================
*/
void BotNav_PredictEnemyPosition( int entnum, vec3_t outOrigin, float predictTime )
{
    if ( entnum < 0 || entnum >= MAX_CLIENTS ) return;

    gclient_t *cl = &level.clients[entnum];
    if ( cl->pers.connected != CON_CONNECTED ) {
        VectorCopy( g_entities[entnum].r.currentOrigin, outOrigin );
        return;
    }

    trap_Nav_PredictEnemyPosition( cl->ps.origin, cl->ps.velocity, predictTime, outOrigin );
}

/*
=================
BotNav_WalkTest — dev-only proof that the steering core drives a non-client agent

Steps a VIRTUAL point (a bare vec3 + one standalone botNavState_t, owned by no
client and no entity) from 'start' toward 'goal' using the exact same
BotNav_Steer core + Nav_FindPath query the real path-follow uses. Each step asks
BotNav_Steer for a direction and advances the point by speed*dt (a plain Euler
integration standing in for a real mover's trajectory), until the steer reports
the goal reached, reports failure, or a step budget is exhausted.

The point is to exercise the client-independent seam end-to-end with no client
slot: if this reaches the mark, the steering core is genuinely client-agnostic.

Returns qtrue if the goal was reached; *outSteps gets the step count.
=================
*/
qboolean BotNav_WalkTest( const vec3_t start, const vec3_t goal, int *outSteps )
{
    botNavState_t       bn;          /* standalone cursor — NOT in botNavStates[] */
    botNavSteerResult_t out;
    vec3_t              origin, velocity;
    bot_goal_t          bgoal;
    const float         speed     = (float)DEFAULT_MOVESPEED_PLAYER; /* ~player run */
    const float         dt        = 0.1f;      /* 100 ms, the bot think cadence     */
    const float         reachDist = 32.0f;     /* within this of the goal = arrived  */
    const int           maxSteps  = 400;       /* ~40 s of virtual travel — bounded  */
    int                 step;

    memset( &bn, 0, sizeof( bn ) );
    bn.active = qtrue;
    VectorCopy( start, origin );
    VectorClear( velocity );

    memset( &bgoal, 0, sizeof( bgoal ) );
    VectorCopy( goal, bgoal.origin );

    for ( step = 0; step < maxSteps; step++ ) {
        /* Arrived when within reachDist of the goal (belt-and-braces alongside
         * the steer's own final-waypoint 'done' signal below). */
        vec3_t toGoal;
        VectorSubtract( goal, origin, toGoal );
        if ( VectorLength( toGoal ) <= reachDist ) {
            if ( outSteps ) *outSteps = step;
            return qtrue;
        }

        /* The client-independent seam: same steering core the real mover uses,
         * driven by a bare origin/velocity/goal — no bs->, no client slot. */
        BotNav_Steer( &bn, origin, velocity, bgoal.origin, -1, &out );

        if ( out.failure ) {
            if ( outSteps ) *outSteps = step;
            return qfalse;   /* unreachable / goal aborted */
        }
        if ( out.done ) {
            if ( outSteps ) *outSteps = step;
            return qtrue;    /* final waypoint reached */
        }
        if ( out.holdFrame || out.suppressMove ) {
            /* Repath pending or mid off-mesh transit — no advance this step. */
            continue;
        }

        /* Euler-advance the virtual point along the steer direction. This is the
         * stand-in for a real mover's trajectory; the real mover sets a
         * trajectory instead, but the steering input is identical. */
        VectorMA( origin, speed * dt, out.movedir, origin );
        VectorScale( out.movedir, speed, velocity );
    }

    if ( outSteps ) *outSteps = step;
    return qfalse;   /* step budget exhausted without arriving */
}

#endif /* FEAT_RECAST_NAVMESH */