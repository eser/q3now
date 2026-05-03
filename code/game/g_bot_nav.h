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
/* Exit OMC transit when Z-delta from start >= this OR XY-dist >= this. */
#define BOT_NAV_OMC_TRANSIT_Z_DELTA    64.0f
#define BOT_NAV_OMC_TRANSIT_XY_DIST   200.0f

/* Steering states. */
typedef enum {
    NAV_STEER_IDLE       = 0,  /* no active path                              */
    NAV_STEER_WALK       = 1,  /* normal ground movement                      */
    NAV_STEER_CROUCH     = 2,  /* low-ceiling passage — issue crouch command  */
    NAV_STEER_OMC_TRANSIT= 3,  /* inside jump-pad / teleporter traversal      */
    NAV_STEER_STUCK      = 4   /* goal abort triggered — sets result->failure */
} navSteerState_t;

typedef struct {
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
    /* Steering state machine (Phase 5). */
    int         steeringState;      /* NAV_STEER_* enum; 0 = IDLE                */
    float       omcTransitTimeout;  /* FloatTime() hard deadline for OMC_TRANSIT  */
    vec3_t      omcTransitStartPos; /* origin when OMC_TRANSIT was entered        */
    int         currentArea;        /* cached area flags for pathIdx waypoint     */
    int         nextArea;           /* cached area flags for pathIdx+1 waypoint   */
    qboolean    active;          /* qtrue while this bot slot is in use          */
} botNavState_t;

/* One slot per possible client. */
extern botNavState_t botNavStates[MAX_CLIENTS];

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
