/*
===========================================================================
g_bot_nav.c -- Detour-based bot path-following (Phase 4)

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
    /* No path yet. */
    if ( bn->path.count == 0 ) return qtrue;

    /* Time-based repath interval. */
    float now = (float)trap_Milliseconds() * 0.001f;
    if ( now >= bn->repathTime ) return qtrue;

    /* Goal has moved significantly. */
    vec3_t delta;
    VectorSubtract( goal, bn->lastGoal, delta );
    if ( VectorLength( delta ) > BOT_NAV_GOAL_CHANGE_THRESHOLD ) return qtrue;

    return qfalse;
}

/*
=================
BotNav_MoveToGoal — called in place of trap_BotMoveToGoal()
=================
*/
void BotNav_MoveToGoal( bot_state_t *bs, bot_goal_t *goal,
                         bot_moveresult_t *result )
{
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
    {
        vec3_t moved;
        VectorSubtract( bs->origin, bn->lastPos, moved );
        if ( VectorLength( moved ) >= BOT_NAV_STUCK_DIST ) {
            /* Progressing normally — update checkpoint. */
            VectorCopy( bs->origin, bn->lastPos );
            bn->lastMoveTime = now;
        } else if ( bn->lastMoveTime > 0.0f &&
                    ( now - bn->lastMoveTime ) >= BOT_NAV_STUCK_TIME ) {
            /* Stuck — force a repath but do NOT set failure. */
            bn->path.count = 0;
            bn->lastMoveTime = now;
            VectorCopy( bs->origin, bn->lastPos );
        }
    }

    /* ── Goal-abort stuck detection (FIX-1) ────────────────────────────── */
    /* Independent of the repath-stuck tracker above. If bot has not moved
     * BOT_NAV_GOAL_ABORT_DIST units in BOT_NAV_GOAL_ABORT_TIME seconds while
     * a path is active, signal failure so the AI layer picks a new goal. */
    if ( bn->path.count > 0 ) {
        if ( bn->goalAbortDeadline == 0.0f ) {
            /* First frame with an active path — start abort window. */
            VectorCopy( bs->origin, bn->goalAbortCheckPos );
            bn->goalAbortDeadline = now + BOT_NAV_GOAL_ABORT_TIME;
        } else {
            vec3_t abortMoved;
            VectorSubtract( bs->origin, bn->goalAbortCheckPos, abortMoved );
            if ( VectorLength( abortMoved ) >= BOT_NAV_GOAL_ABORT_DIST ) {
                /* Made enough progress — reset abort window. */
                VectorCopy( bs->origin, bn->goalAbortCheckPos );
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
                result->failure = qtrue;
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
    if ( bn->steeringState != NAV_STEER_OMC_TRANSIT && BotNav_NeedsRepath( bn, goal->origin ) ) {
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
                VectorSubtract( goal->origin, bn->lastGoal, delta );
                reason = ( VectorLength( delta ) > BOT_NAV_GOAL_CHANGE_THRESHOLD )
                         ? "goal_moved" : "stuck";
            }
        }
        qboolean isTimer = (strcmp(reason, "timer") == 0) ? qtrue : qfalse;

        int n = trap_Nav_FindPath( bs->origin, goal->origin, 0, &bn->path );
        if ( n <= 0 ) {
            /* Path query failed — signal failure. */
            bn->path.count = 0;
            result->failure = qtrue;
            return;
        }

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
                    bs->origin[0], bs->origin[1], bs->origin[2],
                    goal->origin[0], goal->origin[1], goal->origin[2],
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
        VectorCopy( goal->origin, bn->lastGoal );
        /* Ensure stuck-timer is live. */
        if ( bn->lastMoveTime == 0.0f ) {
            VectorCopy( bs->origin, bn->lastPos );
            bn->lastMoveTime = now;
        }
        /* Reset goal-abort window on new path (FIX-1). */
        VectorCopy( bs->origin, bn->goalAbortCheckPos );
        bn->goalAbortDeadline = now + BOT_NAV_GOAL_ABORT_TIME;
        /* Start in WALK state; crouch/OMC transitions happen below. */
        bn->steeringState = NAV_STEER_WALK;
    }

    /* ── Advance pathIdx when within advance radius ─────────────────────── */
    {
        /* FIX-3: use nav_waypoint_tolerance cvar (default 32 Q3u) as base
         * advance radius.  Dynamic speed-based lookahead added on top. */
        float baseRadius = nav_waypoint_tolerance.value > 0.0f
                           ? nav_waypoint_tolerance.value
                           : BOT_NAV_MIN_ADVANCE_RADIUS;
        float speed = VectorLength( bs->velocity );
        float advanceRadius = baseRadius;
        float dynamic = speed * 0.15f;  /* 0.15s ≈ 1.5 × 100ms tick */
        if ( dynamic > advanceRadius ) advanceRadius = dynamic;

        /* Walk forward through waypoints that are already "behind" us. */
        while ( bn->pathIdx < bn->path.count - 1 ) {
            vec3_t toWp;
            VectorSubtract( bn->path.positions[bn->pathIdx], bs->origin, toWp );
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
                /* If we advanced past an OMC waypoint, exit OMC_TRANSIT. */
                if ( bn->steeringState == NAV_STEER_OMC_TRANSIT &&
                     (bn->path.flags[bn->pathIdx] & NAV_PATHFLAG_OFFMESH_CON) ) {
                    bn->steeringState = NAV_STEER_WALK;
                    bn->path.count = 0;  /* force repath from new side of the OMC */
                }
                bn->pathIdx++;
            } else {
                break;
            }
        }
    }

    /* ── Final waypoint reached ─────────────────────────────────────────── */
    if ( bn->pathIdx >= bn->path.count ) {
        /* Goal reached: hold position, clear path so next call resets it. */
        bn->path.count = 0;
        bn->steeringState = NAV_STEER_IDLE;
        /* result->failure stays qfalse — bot should decide a new goal. */
        return;
    }

    /* ── OMC_TRANSIT: handle jump-pad / teleporter traversal ────────────── */
    if ( bn->steeringState == NAV_STEER_OMC_TRANSIT ) {
        vec3_t travelDelta;
        VectorSubtract( bs->origin, bn->omcTransitStartPos, travelDelta );
        float zdelta = travelDelta[2] < 0 ? -travelDelta[2] : travelDelta[2];
        travelDelta[2] = 0;
        float xydist = VectorLength( travelDelta );

        qboolean exitOmc = qfalse;
        if ( zdelta >= BOT_NAV_OMC_TRANSIT_Z_DELTA || xydist >= BOT_NAV_OMC_TRANSIT_XY_DIST )
            exitOmc = qtrue;
        if ( now >= bn->omcTransitTimeout ) {
            if ( nav_botdebug.integer ) {
                BotAI_Print( PRT_MESSAGE,
                    "[BOTNAV] client %d OMC_TRANSIT timeout, forcing repath\n", cn );
            }
            exitOmc = qtrue;
        }
        if ( exitOmc ) {
            bn->steeringState = NAV_STEER_WALK;
            bn->path.count = 0;  /* repath from new position */
            return;              /* let repath happen next frame */
        }
        /* During transit: keep moving toward the OMC destination waypoint. */
    }

    /* ── Enter OMC_TRANSIT when stepping onto an OMC source ─────────────── */
    if ( bn->steeringState != NAV_STEER_OMC_TRANSIT ) {
        int curIdx = bn->pathIdx;
        if ( curIdx < bn->path.count &&
             (bn->path.flags[curIdx] & NAV_PATHFLAG_OFFMESH_CON) ) {
            vec3_t toWp;
            VectorSubtract( bn->path.positions[curIdx], bs->origin, toWp );
            float distToOmc = VectorLength( toWp );
            if ( distToOmc <= BOT_NAV_OMC_APPROACH_DIST ) {
                bn->steeringState = NAV_STEER_OMC_TRANSIT;
                VectorCopy( bs->origin, bn->omcTransitStartPos );
                bn->omcTransitTimeout = now + BOT_NAV_OMC_TRANSIT_TIMEOUT;
                if ( nav_botdebug.integer ) {
                    BotAI_Print( PRT_MESSAGE,
                        "[BOTNAV] client %d OMC_TRANSIT entered at (%.0f,%.0f,%.0f)\n",
                        cn, bs->origin[0], bs->origin[1], bs->origin[2] );
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
        int curFlags  = (curIdx  < bn->path.count) ?
                        trap_Nav_GetPolyAreaFlags( bn->path.polyrefs[curIdx] )  : 0;
        int nextFlags = (nextIdx != curIdx) ?
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
    float *wp = bn->path.positions[bn->pathIdx];

    /* FIX-2: OMC approach — when next waypoint is an OMC source and bot is
     * within BOT_NAV_OMC_APPROACH_DIST, overshoot by BOT_NAV_OMC_OVERSHOOT
     * to ensure the bot's bbox fully enters the trigger brush. */
    float omcExtended[3];
    if ( bn->path.flags[bn->pathIdx] & NAV_PATHFLAG_OFFMESH_CON ) {
        vec3_t toWp;
        VectorSubtract( wp, bs->origin, toWp );
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
    VectorSubtract( wp, bs->origin, dir );
    dir[2] = 0;  /* Flatten: bot uses jump/gravity for vertical, not steering. */
    VectorNormalize( dir );

    trap_EA_Move( bs->client, dir, 400 );

    /* Crouch command when in low-ceiling passages. */
    if ( bn->steeringState == NAV_STEER_CROUCH ) {
        trap_EA_Crouch( bs->client );
    }

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

    /* Update ideal yaw toward waypoint. */
    vec3_t wpAngles;
    vectoangles( dir, wpAngles );
    bs->ideal_viewangles[YAW]   = wpAngles[YAW];
    bs->ideal_viewangles[PITCH] = wpAngles[PITCH];

    /* Fill result movedir for callers that read it. */
    VectorCopy( dir, result->movedir );
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

#endif /* FEAT_RECAST_NAVMESH */