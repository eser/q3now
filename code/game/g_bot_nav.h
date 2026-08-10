// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_bot_nav.h -- per-bot Detour path-following state

Parallel array botNavStates[MAX_CLIENTS] mirrors the bot_state_t array.
Must NOT access nav_local.h or any engine-only Recast types; this code
runs inside the WASM game sandbox and calls through trap_Nav_* only.

IMPORTANT: Include this header AFTER g_local.h in each .c file.
Do NOT include g_local.h from this header to avoid double-include issues.
===========================================================================
*/
#ifndef G_BOT_NAV_H
#define G_BOT_NAV_H

#include "../qcommon/q_feats.h"

#if FEAT_RECAST_NAVMESH

/* Callers must include before this header (in order):
 *   g_local.h / q_shared.h
 *   ../botlib/be_ai_goal.h   (bot_goal_t)
 *   ../botlib/be_ai_move.h   (bot_moveresult_t)
 *   ai_main.h                (bot_state_t)
 *   ../qcommon/nav/nav_types.h
 * g_bot_nav.h intentionally omits those includes to avoid double-include
 * of header files that lack include guards. */
#include "../qcommon/nav/nav_types.h"

/* Maximum advance radius in Quake units.
 * At 700 ups / 100 ms think tick the bot moves ~70 units; 96 units gives
 * ~1.4 ticks of lookahead, preventing flip-flop on fast-moving bots. */
#define BOT_NAV_MIN_ADVANCE_RADIUS  96.0f

/* Repath interval: force a fresh path query every 0.5 seconds.
 * Also triggers on goal delta > BOT_NAV_GOAL_CHANGE_THRESHOLD.
 * 0.5s allows blocked-poly reroutes to resolve within one timer tick.
 * Raise to 1.5s if query overhead becomes measurable at high bot counts. */
#define BOT_NAV_REPATH_INTERVAL      0.5f

/* Goal-change threshold: if goal moves more than this, repath immediately. */
#define BOT_NAV_GOAL_CHANGE_THRESHOLD  32.0f

/* Repath-stuck: if bot has not moved BOT_NAV_STUCK_DIST units in
 * BOT_NAV_STUCK_TIME seconds, force a repath (does NOT abort goal). */
#define BOT_NAV_STUCK_DIST   40.0f
#define BOT_NAV_STUCK_TIME    1.5f

/* Goal-abort: if bot has not moved BOT_NAV_GOAL_ABORT_DIST units in
 * BOT_NAV_GOAL_ABORT_TIME seconds while a path is active, set failure
 * so the AI layer picks a new goal on the next frame. */
#define BOT_NAV_GOAL_ABORT_DIST   32.0f
#define BOT_NAV_GOAL_ABORT_TIME    3.0f

/* OMC approach: when this close to an OMC source waypoint, continue
 * forward past it by BOT_NAV_OMC_OVERSHOOT units to ensure the bot's
 * bbox fully enters the trigger brush. */
#define BOT_NAV_OMC_APPROACH_DIST  64.0f
#define BOT_NAV_OMC_OVERSHOOT      32.0f

/* OMC transit: hard timeout before giving up on a jump pad / teleporter. */
#define BOT_NAV_OMC_TRANSIT_TIMEOUT  3.0f
/* Exit OMC transit on ARRIVAL at the link's DESTINATION endpoint — length-agnostic
 * by nature ("am I there", not "did I move far enough"), so a 50u plat link and a
 * 500u jump complete identically.  Two arrival tests (either fires):
 *   (1) within BOT_NAV_OMC_ARRIVE_EPS of the dest waypoint (a real arrival
 *       tolerance), OR
 *   (2) having PASSED the plane through the dest perpendicular to the link
 *       direction (so an overshoot past the dest still completes).
 * The old absolute Z_DELTA/XY_DIST displacement thresholds are retired: a fixed
 * 200u XY target is unreachable for a short link by definition, which is exactly
 * the short-OMC transit loop that trapped the follower before t9.  The 3s timeout
 * stays as the genuine-failure backstop (a link that truly cannot complete bails).
 *
 * Epsilon derivation (one physics/mesh-based constant, no per-map/per-link tuning):
 * the NAV_AGENT_PLAYER capsule radius is 15 Q3u; arrival = the bot's center within
 * a capsule-diameter (2R = 30u, rounded to the shipped 32u waypoint tolerance) of
 * the endpoint — the same "reached this point" tolerance the waypoint advance uses
 * (nav_waypoint_tolerance default 32). */
#define BOT_NAV_OMC_ARRIVE_EPS       32.0f

/* Steering states. */
typedef enum {
    NAV_STEER_IDLE       = 0,  /* no active path                              */
    NAV_STEER_WALK       = 1,  /* normal ground movement                      */
    NAV_STEER_CROUCH     = 2,  /* low-ceiling passage — issue crouch command  */
    NAV_STEER_OMC_TRANSIT= 3,  /* inside jump-pad / teleporter traversal      */
    NAV_STEER_STUCK      = 4   /* goal abort triggered — sets result->failure */
} navSteerState_t;

typedef struct botNavState_s {
    navPath_t   path;            /* current straight-path waypoints              */
    int         pathIdx;         /* index of the next waypoint to steer toward   */
    float       repathTime;      /* FloatTime() deadline — repath when exceeded  */
    vec3_t      lastGoal;        /* goal position on last path request           */
    vec3_t      lastPos;         /* bot position at lastMoveTime                 */
    float       lastMoveTime;    /* FloatTime() when lastPos was sampled         */
    /* Goal-abort tracker (independent from repath-stuck). */
    vec3_t      goalAbortCheckPos;  /* position when current abort window began  */
    float       goalAbortDeadline;  /* FloatTime() — abort if exceeded w/o progress */
    /* nav_botdebug log dedup. */
    int         prevPathCount;      /* waypoint count from last repath            */
    vec3_t      prevFirstWp;        /* first waypoint from last repath            */
    /* Steering state machine. */
    int         steeringState;      /* NAV_STEER_* enum; 0 = IDLE                */
    float       omcTransitTimeout;  /* FloatTime() hard deadline for OMC_TRANSIT  */
    vec3_t      omcTransitStartPos; /* origin when OMC_TRANSIT was entered        */
    vec3_t      omcTransitDest;     /* link DESTINATION endpoint (arrival target) */
    vec3_t      omcTransitLip;      /* link SOURCE endpoint (FALL: the edge)      */
    unsigned char omcTransitMode;   /* navTraversalMode_t of the link in transit  */
    qboolean    omcFallPhaseB;      /* FALL: airborne (past the lip) — latched     */
    qboolean    omcJumpFired;       /* CLIMB: one-shot jump latch (cleared @ entry) */
    /* FALL phase-A progress watermark: the SMALLEST horizontal distance to the lip
     * this transit has achieved so far.  The phase-A cap is measured against the
     * OBJECTIVE (the lip), not against the transit's arbitrary start point — see the
     * NAV_TM_FALL case in BotNav_MoveToGoal.  Seeded at entry to the entry distance. */
    float       omcFallBestToLip;
    /* Transit progress watermark: the SMALLEST distance to the link DESTINATION this
     * transit has achieved, and when it last improved.  Same shape as
     * omcFallBestToLip above, applied to the transit's own promise rather than
     * FALL's phase-A objective.  A transit asserts that SOMETHING is carrying the
     * bot toward the dest; if this watermark stops improving, that assertion has
     * been falsified.  Both seeded at entry. */
    float       omcBestToDest;
    float       omcBestToDestTime;
    /* TRUE closest approach of the current transit, sampled EVERY FRAME (not on the
     * leg-boundary lattice), with the geometry AT that minimum.  Emitted once at
     * transit end.  omcMinPast/omcMinLatSq let the reader confirm a sample is
     * genuinely mid-transit — the entry signature is pastPlane == -distToDest, so
     * d + past ~ 0 means the sample is entry, not an approach floor.  Seeded at
     * entry to a value no real distance can beat. */
    float       omcMinDist;
    float       omcMinPast;
    float       omcMinLatSq;
    int         currentArea;        /* cached area flags for pathIdx waypoint     */
    int         nextArea;           /* cached area flags for pathIdx+1 waypoint   */
    int         agentType;          /* navmesh size to path on; 0 = player (bots) */
    qboolean    active;          /* qtrue while this bot slot is in use          */
    int         lastPosLogTime;     /* level.time (ms) of last position telemetry emit */
    /* World-state generation this agent's corridor was planned against. When a
     * mover transition mutates the navmesh's cost/passability (Nav_WorldChanged
     * bumps the counter and records what moved), a corridor planned under the old
     * generation is stale ONLY IF it crosses something that moved — the mismatch is
     * the cheap pre-filter, the corridor-crossing test is the decision. See
     * BotNav_NeedsRepath and Nav_CorridorTouchesWorldChange. */
    unsigned int worldGen;
} botNavState_t;

/*
 * Nav_WorldChanged — the navigation graph's cost/passability just changed.
 *
 * Called from the generic mover-state transition seam (Q3_SetMoverState), which
 * is where the navmesh area/flag mutation for that mover is applied. Bumps the
 * world generation counter AND records the changed mover's world bounds
 * ('absmin'/'absmax'), which is what SCOPES the invalidation: an agent re-plans
 * only when its own held corridor crosses the bounds of a mover that changed after
 * its stamp. An agent whose corridor touches nothing that moved — including the
 * agent that CAUSED the transition, whose corridor ends at the button it is pressing
 * while the mover that moved is the door elsewhere — is left alone.
 *
 * Event-driven: one ring-slot write per transition, no per-frame scan and no walk
 * over agents at the call site. The corridor crossing test is paid per agent, only
 * on a pending mismatch, and only once per transition (an irrelevant mismatch is
 * retired by advancing the agent's stamp).
 */
void Nav_WorldChanged( const vec3_t absmin, const vec3_t absmax );

/*
 * Nav_WorldGen — current navigation world-state generation.
 * Monotonic; changes only when a mover transition mutates the graph.
 */
unsigned int Nav_WorldGen( void );

/* One slot per possible client. */
extern botNavState_t botNavStates[MAX_CLIENTS];

/*
 * Steering output — the result of one nav step computed from a nav state plus a
 * raw origin/velocity/goal, with no client-input side effects. The caller reads
 * these back and decides how to apply them (a bot writes them through the EA
 * usercmd pipeline; a non-client mover would apply them through its own move
 * path). POD in / POD out; keeping this client-agnostic lets the same steering
 * core drive movers that own no client slot.
 */
typedef struct botNavSteerResult_s {
    vec3_t   movedir;      /* normalized ground steering direction (flattened) */
    qboolean crouch;       /* qtrue when in a low-ceiling passage              */
    qboolean suppressMove; /* qtrue mid off-mesh traversal — hold the move     */
    qboolean failure;      /* qtrue when the path query failed / goal aborted  */
    qboolean done;         /* qtrue when the goal/final waypoint was reached   */
    qboolean holdFrame;    /* qtrue when the step produced no move (repath next)*/
    /* Per-mode OMC traversal, decided by the CALLER (which has ps/groundEntityNum);
     * the pure core NEVER touches an entity.  The core exports the mode + transit
     * state + destination here so a real-client caller (BotNav_MoveToGoal) can
     * override the actuation; the harness / mover-pawn caller ignores them and the
     * core's own suppressMove (state==OMC_TRANSIT) stays bit-identical to today. */
    qboolean      omcTransit;    /* qtrue while inside NAV_STEER_OMC_TRANSIT       */
    unsigned char omcMode;       /* navTraversalMode_t of the link being crossed   */
    vec3_t        omcDest;       /* the link destination endpoint (arrival target) */
    vec3_t        omcLip;        /* FALL: the lip waypoint (edge to drive off)     */
} botNavSteerResult_t;

/*
 * BotNav_Init — called from BotAISetupClient().
 * Clears the nav state for client 'clientNum'.
 */
void BotNav_Init( int clientNum );

/*
 * BotNav_Shutdown — called from BotAIShutdownClient().
 * Marks the slot inactive; does not free anything (no dynamic allocs).
 */
void BotNav_Shutdown( int clientNum );

/*
 * BotNav_MoveToGoal — Detour replacement for trap_BotMoveToGoal().
 *
 * Queries trap_Nav_FindPath if needed, advances pathIdx toward the goal,
 * and calls trap_EA_Move to generate bot movement.
 *
 * result->failure is set qtrue ONLY if the path query fails (Detour
 * returned 0 waypoints); it is NOT set when the goal is reached
 * (caller retains goal and chooses a new one at the AI layer).
 *
 * bs      — bot state pointer (for position, client, ideal_viewangles)
 * goal    — current AI goal (origin used for path target)
 * result  — move result written here
 */
void BotNav_MoveToGoal( bot_state_t *bs, bot_goal_t *goal,
                         bot_moveresult_t *result );

/*
 * BotNav_Steer — the client-agnostic steering core.
 *
 * Given a nav state and a raw origin/velocity/goal, this runs the repath,
 * stuck/goal-abort, waypoint-advance, off-mesh-connection transit, crouch
 * look-ahead and steering-vector logic, and writes the resulting move direction
 * and flags into 'out'. It performs NO client-input write (no trap_EA_*) and
 * reads nothing through a bot_state_t — the bot wrapper applies the result.
 *
 * bn         — nav state to advance (owns the corridor and steering machine)
 * origin     — current position of the entity being steered
 * velocity   — current velocity (drives dynamic waypoint-advance lookahead)
 * goalOrigin — target position for the path
 * debugClient— identifier used only in nav_botdebug log lines
 * out        — steering result written here
 */
void BotNav_Steer( botNavState_t *bn, const vec3_t origin, const vec3_t velocity,
                   const vec3_t goalOrigin, int debugClient,
                   botNavSteerResult_t *out );

/*
 * Nav_SteerEntity — drive a pool-backed entity one step toward goal.
 *
 * Reads the entity's current origin (r.currentOrigin) + trajectory velocity
 * (s.pos.trDelta) and runs BotNav_Steer on its acquired nav-state. out->failure
 * if the entity holds no nav-state. This is the steer seam a non-client
 * mover-think calls each step. Declared here (not g_local.h) because it needs
 * the full botNavSteerResult_t.
 */
void Nav_SteerEntity( gentity_t *ent, const vec3_t goal, botNavSteerResult_t *out );
/* Nav_StartFollower + G_RunNavFollower are declared in g_local.h (base types
 * only), so g_main.c / g_cmds.c reach them without this header. */

/*
 * BotNav_MovementViewTarget — Recast replacement for trap_BotMovementViewTarget.
 *
 * Returns the next path waypoint ahead of pathIdx as a view target,
 * so the bot looks toward where it's going rather than at its feet.
 * Sets targetOut and returns qtrue; returns qfalse if nav is inactive.
 */
qboolean BotNav_MovementViewTarget( int clientNum, const bot_goal_t *goal,
                                    vec3_t targetOut );

/*
 * nav_botdebug — cvar declared in ai_main.c, readable throughout g_bot_nav.c.
 * Non-zero enables per-repath and OMC-transit log lines.
 */
extern vmCvar_t nav_botdebug;

/*
 * nav_waypoint_tolerance — tunable waypoint-reached radius (default 32 Q3u).
 * The bot considers a waypoint reached when it is within this many units.
 * At 700 ups, 32 units = ~1 think tick of margin.
 */
extern vmCvar_t nav_waypoint_tolerance;

/*
 * BotNav_PredictEnemyPosition — gamecode wrapper for trap_Nav_PredictEnemyPosition.
 *
 * Reads enemy origin and velocity from level.clients[entnum].ps, then calls
 * trap_Nav_PredictEnemyPosition to forward-simulate on the navmesh surface.
 * Writes predicted position into outOrigin.  entnum must be < MAX_CLIENTS.
 */
void BotNav_PredictEnemyPosition( int entnum, vec3_t outOrigin, float predictTime );

#endif /* FEAT_RECAST_NAVMESH */

#endif /* G_BOT_NAV_H */
