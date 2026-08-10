// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_sequenced_goal.c — per-brain sequenced-goal cursor substrate for bots.

Generalizes the proven G_RunScriptDispatcher cursor substrate (the one the
monster scripts already ship on) to bot goals. A cursor walks a step table;
each step returns qtrue = done (advance) / qfalse = suspend (yield, re-enter
next frame); a step-change timestamp gives the wait-until clock-poll; a nested
re-entry guard bails the outer walk when a step re-entered the driver on the
same brain.

Every step writes the bot's goal ONLY through the directive projection
(bs->ltgtype / bs->teamgoal + bs->directives.directiveLocked) — the same seam
BotDirective_FrameUpdate uses — so the unmodified AINode_Seek_LTG engine
navigates. No AINode, no goal type, no trap_BotPushGoal. The sequence state is
in the brain's own storage, not the botlib goal stack, so a combat-edge goal-
stack clear cannot discard an in-flight sequence.

Game-VM-internal: no syscall, no enum on the wire, no savegame field.
===========================================================================
*/

#include "g_local.h"
#include "../../../botlib/botlib.h"
#include "../../../botlib/be_aas.h"       /* aas_entityinfo_t, bsp_trace_t (ai_main.h protos) */
#include "../../../botlib/be_ai_goal.h"   /* bot_goal_t (ai_main.h bot_state_t) */
#include "../../../botlib/be_ai_move.h"   /* bot_moveresult_t (ai_dmq3.h protos)  */
#include "ai_main.h"                      /* bot_state_t, BotAI_Print, LTG_DEFENDKEYAREA */
#include "ai_dmq3.h"                      /* BotPointAreaNum */
#include "g_sequenced_goal.h"
#include "g_wiredintel.h"                 /* WiredIntel_JumpTouchArcHits (arc re-solve) */
#include "../../g_bot_nav.h"              /* botNavStates[] — the follower's live corridor */

/* Same channel name the other bot-AI translation units log on, so it interns to
   the one "botlib.ai" channel (LOG_DECLARE_CHANNEL statics are file-local). */
LOG_DECLARE_CHANNEL( ch_botai, "botlib.ai" );

/* =========================================================================
   LIFECYCLE
   ========================================================================= */

/*
==================
WiredIntel_SequencedGoalReset
Zero the sequence state → idle. Called once per bot at brain setup and on the
death/respawn reset path so no stale cursor survives a life. BotResetState
memsets the whole bot_state_t (which already yields an idle sequence); this
makes the idle state explicit and keeps reset a single call, matching
Belief_Init / BotDirective_Init.
==================
*/
void WiredIntel_SequencedGoalReset( botSequencedGoal_t *sg ) {
    if ( !sg ) return;
    memset( sg, 0, sizeof( *sg ) );
    /* active=qfalse, sequenceId=0, cursor=0 — all the idle sentinels are 0. */
}

/* =========================================================================
   DEFERRED ACTUATION — an actuation issued by a planning layer must survive
   to the frame's usercmd.
   =========================================================================

   THE DEFECT THIS EXISTS FOR.  The sequenced-goal driver runs before BotAI so a
   step's GOAL projection (ltgtype/teamgoal) is in place when the navigation
   engine reads it.  But BotAI's FIRST statement is trap_EA_ResetInput, which
   zeroes actionflags/dir/speed for the client.  Any EA_* a step issued is
   therefore deleted one call after it was issued, and the usercmd assembled
   later from those same botinputs never sees it.  The goal projection survives
   the ordering; the actuation does not.

   THE CLASS RULE.  An actuation issued by a planning layer must survive to the
   frame's usercmd.  If a downstream stage resets input state, the actuation must
   be (re-)asserted after that reset AND after any stage that overwrites input,
   at the last point before the input is consumed.

   WHY NOT RE-ASSERT AT THE TOP OF BotAI.  Measured counterfactual: it does not
   work.  BotAI's own movement logic (BotDeathmatchAI → the nav follower) runs
   after that point and issues its own EA_Move/EA_* for the frame, overwriting
   the re-assert — 265/265 ticks still came out with upmove=0.  The seam has to
   be after BotAI entirely.

   THE SEAM.  BotUpdateInput, immediately before trap_EA_GetInput.  It runs in a
   SEPARATE per-client loop after every BotAI call has returned, and
   trap_EA_GetInput is the read that snapshots botinputs into the bot_input_t the
   usercmd is built from.  Nothing between the applier and that read touches
   botinputs, so nothing can overwrite it the way BotAI overwrote the earlier
   attempt.

   GENERIC, NOT JUMP-SPECIFIC.  The record carries an ACTION_* bitmask plus the
   optional move/view, so any EA_* action a step needs rides the same path.  No
   step kind is named here.

   INERT WHEN IDLE.  Nothing records unless an active sequence's step asks for it,
   and the record only applies on the exact level.time it was stamped for.  A bot
   with no sequenced goal recorded nothing, so the applier writes nothing — the
   normal input path is byte-identical. */

/* Discard any intent not stamped for the current frame, then make the record
   current.  Per-frame self-clearing: a step that stops asking for an action
   simply stops re-stamping, and the stale intent expires on its own rather than
   sticking to the client. */
static void SeqActuationBeginFrame( botSequencedGoal_t *sg ) {
    if ( sg->actFrameTime != level.time ) {
        sg->actActions   = 0;
        sg->actHaveMove  = qfalse;
        sg->actHaveView  = qfalse;
        sg->actMoveSpeed = 0.0f;
        VectorClear( sg->actMoveDir );
        VectorClear( sg->actViewAngles );
        sg->actFrameTime = level.time;
    }
}

/* Record ACTION_* flags this step wants actuated this frame. Additive: several
   steps/branches in the same frame compose into one bitmask. */
void WiredIntel_SeqWantAction( bot_state_t *bs, int actionFlags ) {
    if ( !bs || !actionFlags ) return;
    SeqActuationBeginFrame( &bs->sequencedGoal );
    bs->sequencedGoal.actActions |= actionFlags;
}

/* Record the movement vector this step wants actuated this frame. */
void WiredIntel_SeqWantMove( bot_state_t *bs, vec3_t dir, float speed ) {
    if ( !bs ) return;
    SeqActuationBeginFrame( &bs->sequencedGoal );
    VectorCopy( dir, bs->sequencedGoal.actMoveDir );
    bs->sequencedGoal.actMoveSpeed = speed;
    bs->sequencedGoal.actHaveMove  = qtrue;
}

/* Record the view angles this step wants actuated this frame. */
void WiredIntel_SeqWantView( bot_state_t *bs, vec3_t viewangles ) {
    if ( !bs ) return;
    SeqActuationBeginFrame( &bs->sequencedGoal );
    VectorCopy( viewangles, bs->sequencedGoal.actViewAngles );
    bs->sequencedGoal.actHaveView = qtrue;
}

/*
==================
WiredIntel_ApplySequencedActuation

Replay this frame's recorded actuation through the real trap_EA_* entry points.
Called from BotUpdateInput immediately before trap_EA_GetInput — after
trap_EA_ResetInput and after BotAI's own movement logic, at the last point
before the input is consumed.

WHY trap_EA_Action AND NOT trap_EA_Jump FOR THE JUMP.  trap_EA_Jump is not a
plain setter: EA_Jump CLEARS ACTION_JUMP when ACTION_JUMPEDLASTFRAME is set (the
engine's hold-to-bunnyhop suppressor), and EA_ResetInput sets JUMPEDLASTFRAME on
every frame the previous frame jumped.  Replaying through EA_Jump here would
therefore self-cancel on alternating frames — the actuation would survive the
reset only half the time.  trap_EA_Action ORs the bitmask in unconditionally,
which is the correct primitive for "this planning layer asserts these actions",
and it is generic over every ACTION_* rather than one named action.  A step that
genuinely wants the alternating-frame suppressor can express it by not
re-stamping on the off frame.

Inert unless a step recorded an intent stamped for the current level.time.
==================
*/
void WiredIntel_ApplySequencedActuation( bot_state_t *bs ) {
    botSequencedGoal_t *sg;

    if ( !bs ) return;
    sg = &bs->sequencedGoal;

    /* Idle brain, or nothing recorded for THIS frame → write nothing. This is
       the byte-identical guarantee for the non-sequenced path. */
    if ( !sg->active || sg->sequenceId == 0 ) return;
    if ( sg->actFrameTime != level.time ) return;
    if ( !sg->actActions && !sg->actHaveMove && !sg->actHaveView ) return;

    if ( sg->actHaveView )
        trap_EA_View( bs->client, sg->actViewAngles );
    if ( sg->actHaveMove )
        trap_EA_Move( bs->client, sg->actMoveDir, sg->actMoveSpeed );
    if ( sg->actActions )
        trap_EA_Action( bs->client, sg->actActions );
}

/*
==================
WiredIntel_SequencedGoalHasMovementGoal

True when the brain has an ACTIVE sequence whose CURRENT step is a movement goal
(GOTO or JUMP_TOUCH) — i.e. the step has projected a destination into the bot's
ltgtype/teamgoal that goal selection must honor.  This is the predicate the SINGLE
goal-arbitration gate keys on: while it holds, BotGetLongTermGoal returns the step's
projected goal directly and the item/roam/defend selection yields.  A WAITMS /
WAIT_MOVERSTATE step has no movement goal (it holds position), so those correctly
do NOT claim movement authority — normal selection is free during a wait.
==================
*/
qboolean WiredIntel_SequencedGoalHasMovementGoal( const struct bot_state_s *bs ) {
    const botSequencedGoal_t *sg;
    if ( !bs ) return qfalse;
    sg = &bs->sequencedGoal;
    if ( !sg->active || sg->sequenceId == 0 || sg->cursor >= sg->stepCount )
        return qfalse;
    switch ( sg->steps[sg->cursor].type ) {
        case SEQ_STEP_GOTO:
        case SEQ_STEP_JUMP_TOUCH:
            return qtrue;
        default:
            return qfalse;   /* WAITMS / WAIT_MOVERSTATE hold in place */
    }
}

/* =========================================================================
   STEP PRIMITIVES — qtrue = done (advance), qfalse = suspend (re-enter)

   Each mirrors the script-verb contract exactly: run to completion returns
   qtrue so the driver advances the cursor; "not done yet" returns qfalse so
   the driver holds the cursor and re-enters next frame.
   ========================================================================= */

/* Point the bot at a fixed world origin as a navigate-then-hold long-term goal,
   written through the directive projection (bs->ltgtype = LTG_DEFENDKEYAREA +
   bs->teamgoal) with the directive LOCKED so combat/NBG diversions cannot
   override the in-flight step. This is the same origin-goal shape
   BotSetOriginGoal writes for the DEFEND/CAMP/GOTO_EXIT directives; replicated
   here (rather than calling that static helper) so the sequenced-goal substrate
   has no link-time coupling to g_wiredintel.c's internals, and so the lock is
   applied as part of the same write. */
static void SeqProjectOriginGoal( bot_state_t *bs, const vec3_t origin, float range ) {
    VectorCopy( origin, bs->teamgoal.origin );
    bs->teamgoal.areanum   = BotPointAreaNum( (float *)origin );
    bs->teamgoal.entitynum = -1;
    bs->teamgoal.flags     = 0;
    bs->ltgtype            = LTG_DEFENDKEYAREA;
    bs->teamgoal_time      = FloatTime() + 1.0f;   /* refreshed each frame */
    bs->defendaway_time    = 0;
    bs->defendaway_range   = range > 0.0f ? range : 200.0f;
    /* Lock the directive so BotFindEnemy / BotNearbyGoal cannot override the
       sequenced step mid-sequence (honored at ai_dmnet.c's directiveLocked
       checks). The sequence owns the goal until its cursor advances. */
    bs->ordered                       = qtrue;
    bs->directives.directiveLocked    = qtrue;
    /* Expire any in-flight nearby-goal so the bot cannot linger in AINode_Seek_NBG
       (which moves toward the goal STACK and never calls BotGetLongTermGoal, so the
       authority gate there is bypassed).  Zeroing nbg_time makes Seek_NBG time out
       next tick and re-enter Seek_LTG, where the single arbitration gate runs.  The
       directiveLocked flag then keeps it in Seek_LTG (its locked path does defensive
       combat only, never a full Battle_Fight that would also skip the gate). */
    bs->nbg_time                      = 0;
}

/* Precise-arrival projection: drive the bot PRECISELY onto 'origin' with NO
   defend-away back-off.  SeqProjectOriginGoal uses LTG_DEFENDKEYAREA, which patrols
   AWAY once within ~70u of the goal (BotGetLongTermGoal's defendaway) — correct for
   a ride's board point (a touch-trigger volume), wrong for a jump-touch launch foot
   the bot must stand ON.  LTG_GETITEM copies teamgoal to the movement goal every
   frame with no back-off (the same type the activation goal uses to walk the bbox
   onto a button), so the bot converges on the exact launch point.  A small item
   AABB makes "arrived" coincide with the launch foot. */
static void SeqProjectGetItemGoal( bot_state_t *bs, const vec3_t origin ) {
    VectorCopy( origin, bs->teamgoal.origin );
    bs->teamgoal.areanum   = BotPointAreaNum( (float *)origin );
    bs->teamgoal.entitynum = -1;
    bs->teamgoal.flags     = GFL_ITEM;
    VectorSet( bs->teamgoal.mins, -16, -16, -16 );
    VectorSet( bs->teamgoal.maxs,  16,  16,  16 );
    bs->ltgtype            = LTG_GETITEM;
    bs->teamgoal_time      = FloatTime() + 1.0f;   /* refreshed each frame */
    bs->ordered                       = qtrue;
    bs->directives.directiveLocked    = qtrue;
    /* Expire any in-flight nearby-goal (see SeqProjectOriginGoal) so the bot leaves
       AINode_Seek_NBG for Seek_LTG, where the arbitration gate lives. */
    bs->nbg_time                      = 0;
}

/* SEQ_STEP_GOTO: drive toward the step's world point. "Done" = the bot is within
   the hold radius of the point (arrived); otherwise the goal is (re)projected
   and the step yields so the nav engine keeps driving next frame. The arrival
   test is the same distance-to-goal the origin-goal hold uses. */
static qboolean SeqStep_GoTo( bot_state_t *bs, const seqStep_t *step ) {
    float range = step->range > 0.0f ? step->range : 64.0f;

    /* (Re)project the goal every frame the step is active — teamgoal_time is a
       1 s lease, so a step spanning many frames must refresh it, exactly as the
       directive cases refresh theirs. */
    SeqProjectOriginGoal( bs, step->point, range );

    /* Arrived? Distance from the bot's own origin to the step point within the
       hold radius = done → advance. */
    if ( Distance( bs->origin, (float *)step->point ) <= range ) {
        return qtrue;
    }
    return qfalse;   /* not there yet — hold the cursor, drive again next frame */
}

/* SEQ_STEP_WAITMS: the Verb_Wait clock-poll analog. Suspend until the duration
   has elapsed since the cursor advanced onto this step. sg->changeTime is the
   scriptChangeTime analog — stamped by the driver on every advance. */
static qboolean SeqStep_WaitMs( const botSequencedGoal_t *sg, const seqStep_t *step ) {
    if ( step->durationMs <= 0 ) {
        return qtrue;   /* nothing to wait for */
    }
    return ( sg->changeTime + step->durationMs < level.time ) ? qtrue : qfalse;
}

/* Stall window for SEQ_STEP_WAIT_MOVERSTATE. If the mover's observed state
   (moverState + s.pos.trBase) stays UNCHANGED and not-POS2 for this many ms, the
   handler infers the mover is stuck/blocked/keyed and abandons the sequence. A
   few seconds: long enough to cover a normal full plat/door up-cycle (a moving
   mover re-stamps the snapshot every time trBase advances, so a slow-but-moving
   plat never trips it), short enough that a genuinely wedged bot recovers to
   normal goal selection within a couple seconds instead of latching forever. */
#define SEQ_MOVER_STALL_MS 4000

/* SEQ_STEP_WAIT_MOVERSTATE: hold until the mover named by step->moverEntitynum
   reaches its far rest position (moverState == MOVER_POS2), with embedded
   stall-inference. Result contract differs from the pure yield/advance steps:
   it can also request the whole sequence be abandoned (returned via *abandon).

     advance (qtrue)         → mover reached POS2, step done.
     yield   (qfalse)        → mover not yet at POS2, hold the cursor.
     abandon (*abandon=true) → inferred stall; caller gives up the sequence so
                               the bot falls back to normal goal selection
                               (mirrors the 1.1e-guard unreachable→clear+continue
                               philosophy — no infinite latch on a wedged plat).

   The mover is re-resolved from g_entities[entitynum] EVERY frame (the step
   stores only the int entitynum, exactly like beliefInteractable_t.entitynum, so
   nothing goes stale across frames). It reads ONLY format-neutral mover fields
   (moverState, s.pos.trBase) — both are written by the shared Q3_SetMoverState
   used by g_mover_q1.c and g_mover_q3.c alike, so this handler is fully
   format-agnostic (no classname / no q1_func_plat / no format check). This is
   the exact moverState read pattern proven at g_wiredintel.c's DoorIsClosedGate
   and the button-fired check. */
static qboolean SeqStep_WaitMoverState( const botSequencedGoal_t *sg, seqStep_t *step, qboolean *abandon ) {
    const gentity_t *mover;
    int              entnum = step->moverEntitynum;

    *abandon = qfalse;

    /* Re-resolve the mover by number each frame (bounds-checked exactly like the
       belief button/door re-resolve). A gone/out-of-range mover can never reach
       POS2, so treat it as an inferred failure → abandon rather than hang. */
    if ( entnum < 0 || entnum >= level.num_entities ) {
        *abandon = qtrue;
        return qfalse;
    }
    mover = &g_entities[entnum];
    if ( !mover->inuse ) {
        *abandon = qtrue;
        return qfalse;
    }

    /* Done: the mover is at its far rest position. Same read the door/button
       gates use — moverState is format-neutral (shared Q3_SetMoverState). */
    if ( mover->moverState == MOVER_POS2 ) {
        return qtrue;
    }

    /* Embedded stall-inference. There is no pollable engine stuck-bit
       (Q3_Blocked_Door is a transient per-frame reverse handler that leaves no
       persistent flag), so infer stall from lack of observable progress.

       The step's stall* fields snapshot the mover's (moverState, trBase). The
       snapshot clock 'stallSinceTime' resets to the moment progress is observed;
       stall is inferred when it stays put past the window. First observation on a
       freshly-entered step is detected via sg->changeTime — the driver stamps it
       to level.time on every cursor advance, so a snapshot that predates the
       current occupancy (stallSinceTime < sg->changeTime, incl. the memset-zeroed
       initial 0) means "not yet snapshotted for this step" → take the baseline,
       never stall on the entry frame. */
    if ( step->stallSinceTime < sg->changeTime ||
         step->stallMoverState != (int)mover->moverState ||
         !VectorCompare( step->stallTrBase, mover->s.pos.trBase ) ) {
        /* first observation, or observable progress → refresh snapshot + clock */
        step->stallMoverState = (int)mover->moverState;
        VectorCopy( mover->s.pos.trBase, step->stallTrBase );
        step->stallSinceTime  = level.time;
    } else if ( level.time - step->stallSinceTime >= SEQ_MOVER_STALL_MS ) {
        /* pinned unchanged past the window while not-POS2 → inferred stall.
           Give up the sequence (fall back to normal goal selection). */
        *abandon = qtrue;
        return qfalse;
    }

    return qfalse;   /* not at POS2 yet, not (yet) stalled — hold the cursor */
}

/* Progress-watchdog window for an authoritative movement goal: how long the bot
   may fail to make any headway before the sequence aborts.  Derived, not per-map:
   SEQ_MOVER_STALL_MS (4 s) is the elevator/door cycle the ride's own stall-inference
   already uses; a follower making ANY headway re-stamps the best well inside this —
   the window only fires on a genuinely wedged bot (an OMC it cannot cross, a target
   it cannot approach).  The epsilon rejects float jitter so a bot standing still
   does not count as "still making progress".

   BOTH CONSTANTS ARE REINTERPRETED, NOT REPLACED, by the corridor metric below.
   The window is a duration and is metric-independent, so SEQ_PROGRESS_WD_MS carries
   over unchanged.  The epsilon changes UNIT but not VALUE: it was a squared
   straight-line distance (8u)^2, and is now a LINEAR remaining-corridor length in
   the same 8u — the smallest closing that is real movement rather than float
   jitter, at the follower's ~70u-per-tick travel.  Squaring is what the old metric
   needed (it compared DotProduct sq-distances to avoid a sqrt); a corridor length
   is already linear, so the same 8u meaning is expressed directly.  Renamed to
   _EPS (dropping _SQ) because keeping an _SQ name on a linear quantity would be a
   unit lie. */
#define SEQ_PROGRESS_WD_MS  4000
#define SEQ_PROGRESS_WD_EPS 8.0f   /* 8u of corridor closed — real movement, not jitter */

/* Jump-touch cadence.  A launch is retried at most this many times before the
   step abandons (→ normal goal selection): a button the arc genuinely cannot
   overlap should not loop forever.  The airborne window bounds one attempt: a
   run-jump's flight time is ~2*JUMP_VELOCITY/gravity (270*2/800 ≈ 0.68 s), so a
   little over a second covers the arc plus the landing settle before the next
   attempt. */
#define SEQ_JUMPTOUCH_MAX_ATTEMPTS 6
#define SEQ_JUMPTOUCH_AIR_MS       1400
/* "At the launch point" tolerance.  The launch sits on the NAV floor EDGE (the
   producer places it at the reachable frontier toward the button), and a follower
   parks a little short of an edge poly rather than dead on it — so the fire radius
   is the nav reach epsilon (64u), not a tight few units, or the bot circles the
   edge forever without ever being "exactly" there.  The arc was validated from the
   edge; firing from within a reach-epsilon of it launches from effectively the same
   lip. */
#define SEQ_JUMPTOUCH_LAUNCH_RANGE 64.0f

/* Ground state, read LIVE.  bs->cur_ps is a per-frame SNAPSHOT taken at the top of
   BotAI (ai_main.c), which runs AFTER WiredIntel_StepSequencedGoal in the think
   order — so bs->cur_ps.groundEntityNum a sequenced step reads is one frame stale
   (the state before this frame's move, wrong right after a teleport / landing).
   Read the client's CURRENT playerState directly at the use site instead, so the
   jump only fires when the bot is genuinely standing this frame. */
static qboolean SeqBotOnGround( const bot_state_t *bs ) {
    const gentity_t *ent = &g_entities[bs->client];
    if ( !ent->client ) return qfalse;
    return ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE ) ? qtrue : qfalse;
}

/* SEQ_STEP_JUMP_TOUCH: a composed interaction — drive to the launch point, then
   run+jump so the swept player box overlaps the button brush mid-flight, firing
   the same G_TouchTriggers touch a floor-level button gets by a walk-on.  No new
   actuation: the directive projection drives the approach, trap_EA_View faces the
   target, trap_EA_Jump + trap_EA_Move issue the run-jump — all already shipped.

   The press is CONFIRMED format-neutrally: the door named by step->moverEntitynum
   leaving its closed rest state (moverState != doorClosedState) means the touch
   fired (the same moverState read the button-fired / DoorIsClosedGate checks use).
   Bounded retries: each airborne window that ends grounded without the door moving
   consumes one attempt; on exhaustion the step abandons so the bot falls back to
   normal goal selection (no infinite jump loop) — the WAIT_MOVERSTATE philosophy.

   Two phases, held in the step's own scratch (jumpPhase):
     phase 0 — approach+settle: project the launch goal; once within range AND
               grounded, face the target and fire the jump (→ phase 1).
     phase 1 — airborne: keep driving toward the target; if the door moved, done;
               if the air window elapses (landed without firing), count the
               attempt and return to phase 0 (or abandon when attempts run out). */
static qboolean SeqStep_JumpTouch( bot_state_t *bs, seqStep_t *step, qboolean *abandon ) {
    const gentity_t *door;
    int              dnum = step->moverEntitynum;

    *abandon = qfalse;

    if ( dnum < 0 || dnum >= level.num_entities ) { *abandon = qtrue; return qfalse; }
    door = &g_entities[dnum];
    if ( !door->inuse ) { *abandon = qtrue; return qfalse; }

    /* Done the instant the door leaves its closed rest state — the touch fired. */
    if ( (int)door->moverState != step->doorClosedState ) {
        return qtrue;
    }

    /* Face the target every frame (yaw+pitch toward the button brush centre) so
       the run-jump and the view agree — the follower steers by the move dir, but a
       consistent facing keeps the arc aimed. */
    {
        vec3_t dir, ang;
        VectorSubtract( step->target, bs->origin, dir );
        if ( VectorNormalize( dir ) > 0.0f ) {
            vectoangles( dir, ang );
            bs->ideal_viewangles[YAW]   = ang[YAW];
            bs->ideal_viewangles[PITCH] = 0.0f;   /* level pitch; the jump adds the loft */
            /* Recorded, not actuated here: a direct trap_EA_View would be wiped by
               trap_EA_ResetInput at the top of BotAI (see the deferred-actuation
               section). The record is applied at the pre-GetInput seam. */
            WiredIntel_SeqWantView( bs, bs->ideal_viewangles );
        }
    }

    if ( step->jumpPhase == 0 ) {
        /* Approach the launch point PRECISELY (LTG_GETITEM, no defend-away back-off
           — a DEFENDKEYAREA projection would patrol away once within ~70u, and the
           launch foot must be stood ON to fire the run-jump). */
        SeqProjectGetItemGoal( bs, step->point );

        const qboolean atLaunch  = ( Distance( bs->origin, step->point ) <= SEQ_JUMPTOUCH_LAUNCH_RANGE );
        const qboolean onGround  = SeqBotOnGround( bs );
        if ( atLaunch && onGround ) {
            /* The nav follower settles the bot roughly a step short of the computed
               launch edge (the mesh is eroded ~one agent radius in from the real
               floor lip, so the bot CENTRE stands ~a radius back).  Firing the
               install-time speed — validated for the edge point — from this actual
               settle spot lands the arc off the button.  Re-solve the ballistic arc
               from where the bot ACTUALLY stands right now: sweep speeds for one that
               overlaps the button brush from bs->origin, and fire that.  If no arc
               connects from here (still a touch too far to reach the plate), do NOT
               fire — keep approaching; the progress watchdog leashes a bot that can
               never get close enough. */
            float solvedSpeed = 0.0f;
            qboolean arcOk = WiredIntel_JumpTouchArcHits( bs->origin, step->buttonMins,
                                              step->buttonMaxs, &solvedSpeed );
            if ( arcOk ) {
                vec3_t mdir;
                VectorSubtract( step->target, bs->origin, mdir );
                mdir[2] = 0.0f;
                /* Record the launch actuation; it is applied after BotAI's reset
                   and after BotAI's own movement logic, at the last seam before
                   the input becomes the usercmd. Issuing it directly here is what
                   made every previous "fire" a ground-bound walk: the jump was
                   deleted one call later and physics never saw an upmove. */
                if ( VectorNormalize( mdir ) > 0.0f ) {
                    WiredIntel_SeqWantMove( bs, mdir, solvedSpeed > 0.0f ? solvedSpeed : 320.0f );
                } else {
                    WiredIntel_SeqWantAction( bs, ACTION_MOVEFORWARD );
                }
                WiredIntel_SeqWantAction( bs, ACTION_JUMP );
                step->jumpPhase     = 1;
                step->jumpStartTime = level.time;
                step->firedSpeed    = solvedSpeed;
            }
        }
        return qfalse;   /* still approaching / just launched — hold the cursor */
    }

    /* phase 1 — airborne: keep driving toward the target through the flight. */
    {
        vec3_t mdir;
        float  airSpeed = step->firedSpeed > 0.0f ? step->firedSpeed
                        : ( step->jumpSpeed > 0.0f ? step->jumpSpeed : 320.0f );
        VectorSubtract( step->target, bs->origin, mdir );
        mdir[2] = 0.0f;
        /* Same deferral as the launch: recorded now, applied at the pre-GetInput
           seam, so the in-flight steering actually reaches the usercmd. */
        if ( VectorNormalize( mdir ) > 0.0f )
            WiredIntel_SeqWantMove( bs, mdir, airSpeed );

        const qboolean onGround = SeqBotOnGround( bs );
        const qboolean airWindowOver = ( level.time - step->jumpStartTime >= SEQ_JUMPTOUCH_AIR_MS );

        /* The arc completed (landed) or the window elapsed without the door moving:
           one attempt spent.  Retry from the launch point, or give up if exhausted
           (the arc genuinely does not overlap the button — never loop forever). */
        if ( airWindowOver || ( onGround && level.time - step->jumpStartTime > 200 ) ) {
            step->jumpAttempts++;
            if ( step->jumpAttempts >= SEQ_JUMPTOUCH_MAX_ATTEMPTS ) {
                *abandon = qtrue;
                return qfalse;
            }
            step->jumpPhase = 0;   /* back to approach+settle for the next attempt */
        }
        return qfalse;   /* mid-flight — hold the cursor */
    }
}

/* Dispatch one step by kind. Returns the step's qtrue/qfalse verdict; sets
   *abandon when a step asks the driver to abandon the whole sequence (only
   SEQ_STEP_WAIT_MOVERSTATE / SEQ_STEP_JUMP_TOUCH do). Kept separate from the walk
   loop so the loop reads like G_RunScriptDispatcher and the primitive set grows
   here without touching the driver. 'step' is mutable because a step may own
   per-frame scratch (the stall-inference snapshot / jump-touch cadence). */
static qboolean SeqRunStep( bot_state_t *bs, botSequencedGoal_t *sg, seqStep_t *step, qboolean *abandon ) {
    *abandon = qfalse;
    switch ( step->type ) {
        case SEQ_STEP_GOTO:             return SeqStep_GoTo( bs, step );
        case SEQ_STEP_WAITMS:           return SeqStep_WaitMs( sg, step );
        case SEQ_STEP_WAIT_MOVERSTATE:  return SeqStep_WaitMoverState( sg, step, abandon );
        case SEQ_STEP_JUMP_TOUCH:       return SeqStep_JumpTouch( bs, step, abandon );
        case SEQ_STEP_NONE:
        default:
            return qtrue;   /* empty/unknown slot → advance past it (never wedge) */
    }
}

/* =========================================================================
   CORRIDOR PROGRESS — the metric the follower actually moves in.
   ========================================================================= */

/*
==================
SeqCorridorRemaining

Remaining length of the ROUTE the follower is actually walking, from the bot's
current position to the end of its corridor: the distance to the waypoint it is
steering at (pathIdx) plus every segment after it.  Returns qfalse when there is
no usable corridor to measure.

WHY NOT STRAIGHT-LINE DISTANCE.  The old watchdog measured |goal - origin| and
demanded it shrink.  That is not the metric the follower moves in.  A follower
advancing perfectly along a valid corridor makes straight-line distance FLAT or
INCREASING wherever the route bends away from the goal, detours around an
obstacle, or crosses an off-mesh connection — so the leash killed exactly the
bots that were doing the right thing.  Corridor length is monotone under correct
following by construction: every unit walked along the route removes a unit of
route ahead, whatever shape the route has.  A bend, a detour and an OMC traversal
all reduce it; none of them reduce straight-line distance reliably.

WHY IT KEEPS ITS TEETH.  The measure is anchored to the bot's LIVE origin, so it
only falls when the bot physically moves along the route.  A wedged bot holds its
position (or oscillates about it), the remaining length does not fall, and the
window still expires — the abort is preserved for the case it exists for.  A bot
with no corridor at all is not measurable here and is handled by the caller,
which keeps the wedged verdict rather than granting an exemption.

VERTICAL OSCILLATION IS NOT PROGRESS.  The observed wedge cycled Z 68->100->132
->68 with zero horizontal movement.  The distance to the steered waypoint is a
FULL 3D distance, so the Z excursion does move the number — but it moves it back
again every cycle, and the watchdog only re-stamps on a NEW MINIMUM (a strict
improvement past the epsilon).  A cycle returns to where it started, so after the
first descent no later tick beats the running best, and the window expires: the
oscillator is classified WEDGED.  This is the same reason a bot pacing back and
forth along a corridor cannot farm the leash — only net headway counts, because
only a new minimum re-stamps.
==================
*/
static qboolean SeqCorridorRemaining( const bot_state_t *bs, float *outRemaining ) {
#if FEAT_RECAST_NAVMESH
    const botNavState_t *bn;
    vec3_t  d;
    float   remaining;
    int     i;

    if ( bs->client < 0 || bs->client >= MAX_CLIENTS )
        return qfalse;
    bn = &botNavStates[bs->client];
    if ( !bn->active || bn->path.count <= 0 )
        return qfalse;                       /* no corridor to measure */
    if ( bn->pathIdx < 0 || bn->pathIdx >= bn->path.count )
        return qfalse;                       /* cursor out of the corridor */

    /* Leg the bot is currently walking: its live origin to the steered waypoint. */
    VectorSubtract( bn->path.positions[bn->pathIdx], bs->origin, d );
    remaining = VectorLength( d );

    /* Every remaining segment of the route after that waypoint. */
    for ( i = bn->pathIdx; i + 1 < bn->path.count; i++ ) {
        VectorSubtract( bn->path.positions[i + 1], bn->path.positions[i], d );
        remaining += VectorLength( d );
    }

    *outRemaining = remaining;
    return qtrue;
#else
    (void)bs; (void)outRemaining;
    return qfalse;
#endif
}

/* =========================================================================
   PER-TICK DRIVER — walks the cursor exactly like G_RunScriptDispatcher.
   ========================================================================= */

/*
==================
WiredIntel_StepSequencedGoal

Called once per bot per think-tick at the existing driver site, before BotAI.
Walks the active sequence's cursor: run the current step; qfalse → return
(suspend at this cursor, re-enter next frame); qtrue → advance the cursor and
stamp changeTime. Carries the nested re-entry guard: if a step re-entered the
driver on the SAME brain (changing sequenceId / clearing active), the outer walk
bails instead of advancing into state the nested run already moved on from.

Inert when idle: a brain with no active sequence early-returns on the first
branch — ZERO behavior change for a normal bot.
==================
*/
void WiredIntel_StepSequencedGoal( bot_state_t *bs ) {
    botSequencedGoal_t *sg;

    if ( !bs ) return;
    sg = &bs->sequencedGoal;

    /* ── inert early-return: no active sequence → nothing to do ──────────── */
    if ( !sg->active || sg->sequenceId == 0 ) {
        return;
    }

    while ( sg->cursor < sg->stepCount ) {
        seqStep_t *step       = &sg->steps[sg->cursor];
        int sequenceIdBefore  = sg->sequenceId;
        qboolean abandon      = qfalse;

        /* ── progress watchdog (authority's leash) ──────────────────────────
           An authoritative movement goal + an unreachable/unfollowable target =
           a frozen bot.  For a movement step (GOTO / JUMP_TOUCH), track the least
           ROUTE the bot has had left to walk, and abort if it fails to better that
           for the watchdog window (release authority, log, resume normal AI).

           THE MEASURE IS CORRIDOR LENGTH, NOT STRAIGHT-LINE DISTANCE.  A leash on a
           routed goal must be evaluated in the metric the follower actually moves
           in.  Straight-line distance to the endpoint is NOT that metric: a bot
           advancing correctly along a valid corridor holds it flat or grows it
           wherever the route bends, detours, or crosses an off-mesh connection, so
           the old Euclidean test aborted precisely the bots that were following the
           route properly.  Remaining corridor length falls with every unit walked
           along the route regardless of the route's shape, so corridor-advancing
           counts as progress and the leash stops firing on healthy followers.

           IT IS STILL A LEASH.  Only a NEW MINIMUM re-stamps, so headway must be
           net: a bot that oscillates (including the Z 68->100->132->68 wedge, whose
           excursion the 3D leg distance sees and then un-sees each cycle) never
           beats its running best after the first pass and still aborts.  A bot with
           NO corridor is unmeasurable, and unmeasurable is NOT an exemption — the
           window keeps running on the last best, so a bot that cannot even get a
           route aborts on schedule rather than being granted immunity.

           A movement step is re-baselined when the cursor advances (fresh leash per
           step); a WAIT step has no movement goal, so it is exempt (its own
           stall-inference leashes it). */
        if ( step->type == SEQ_STEP_GOTO || step->type == SEQ_STEP_JUMP_TOUCH ) {
            float remaining;
            const qboolean measured = SeqCorridorRemaining( bs, &remaining );
            /* First observation on a freshly-entered step (wdSinceTime predates the
               cursor's changeTime, incl. the zeroed 0) → baseline the leash. */
            const qboolean fresh = ( sg->wdSinceTime < sg->changeTime );

            if ( fresh ) {
                sg->wdBestRemaining = measured ? remaining : 0.0f;
                sg->wdSinceTime     = level.time;
                sg->wdHaveRemaining = measured;
            } else if ( measured &&
                        ( !sg->wdHaveRemaining ||
                          remaining < sg->wdBestRemaining - SEQ_PROGRESS_WD_EPS ) ) {
                /* Strictly less route left to walk than ever before on this step →
                   the follower is advancing along its corridor.  Re-stamp. */
                sg->wdBestRemaining = remaining;
                sg->wdSinceTime     = level.time;
                sg->wdHaveRemaining = qtrue;
            } else if ( level.time - sg->wdSinceTime >= SEQ_PROGRESS_WD_MS ) {
                if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                    Com_Log( SEV_INFO, LOG_CH(ch_botai),
                        "cl=%d seq=%d step[%d] WATCHDOG abort: no corridor progress "
                        "for %dms (at %.0f %.0f %.0f, remaining=%.0f best=%.0f)\n",
                        bs->client, sg->sequenceId, sg->cursor, SEQ_PROGRESS_WD_MS,
                        bs->origin[0], bs->origin[1], bs->origin[2],
                        measured ? remaining : -1.0f,
                        sg->wdHaveRemaining ? sg->wdBestRemaining : -1.0f );
                }
                sg->active                     = qfalse;
                sg->sequenceId                 = 0;
                bs->directives.directiveLocked = qfalse;
                return;
            }
        }

        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 3 &&
             sg->changeTime == level.time ) {
            Com_Log( SEV_DEBUG, LOG_CH(ch_botai),
                "cl=%d seq=%d step[%d] type=%d\n",
                bs->client, sg->sequenceId, sg->cursor, (int)step->type );
        }

        if ( !SeqRunStep( bs, sg, step, &abandon ) ) {
            if ( abandon ) {
                /* A step inferred it can never complete (e.g. a stalled/blocked
                   mover that never reaches POS2). Give up the whole sequence and
                   fall back to normal goal selection next frame — the same
                   philosophy as the 1.1e unreachable-goal guard (clear + continue,
                   no infinite latch). Release the directive lock so the bot's own
                   AI resumes owning the goal. */
                sg->active                     = qfalse;
                sg->sequenceId                 = 0;
                bs->directives.directiveLocked = qfalse;
                return;
            }
            /* not done — suspend at this cursor, re-enter next frame (yield). */
            return;
        }

        /* A step may have re-entered the driver on the SAME brain (e.g. a step
           that fires a combat interrupt which installs/clears a sequence). That
           nested run owns the shared cursor/sequence, so the outer run must NOT
           keep walking its stale sequence — bail instead of advancing into
           corrupted state. Mirrors G_RunScriptDispatcher's activeBlock guard. */
        if ( sg->sequenceId != sequenceIdBefore || !sg->active ) {
            return;
        }

        /* done — advance the cursor and stamp the change time (the WAITMS
           wake-condition and the first-frame log both poll against it). */
        sg->cursor++;
        sg->changeTime = level.time;
    }

    /* sequence exhausted → go idle. Release the lock so the bot resumes normal
       AI (the last GoTo's hold is not kept pinned once the sequence is done). */
    sg->active                     = qfalse;
    sg->sequenceId                 = 0;
    bs->directives.directiveLocked = qfalse;
}

/* =========================================================================
   SELF-TEST — out of the shipped path (bot_debug >= 3, once per process).

   Proves the cursor walk + yield + wait-until + advance MECHANICALLY on scratch
   state — never a live bot — exactly like AI_CompositeSelfTest proves the
   composite substrate. The witness installs the canonical 2-step probe
   GoTo(A) -> WaitMs(N) -> GoTo(B) and asserts the exact cursor progression:

     - tick 1 at A far away: GoTo(A) yields (cursor holds at 0), and the
       directive projection wrote teamgoal.origin == A with directiveLocked;
     - tick 2 with the bot AT A: GoTo(A) done → cursor advances to WaitMs;
     - WaitMs holds (cursor stays at 1) until N ms elapse — the yield-on-qfalse
       wait-until;
     - once N ms pass: WaitMs done → cursor advances to GoTo(B);
     - GoTo(B) with the bot AT B: done → cursor advances past the end → idle,
       and the lock is released — the full A -> wait -> B walk.

   Gated on bot_debug >= 3 so it is dormant at the shipped default and at the
   debug levels normal play uses (0/1/2). Uses the existing bot_debug cvar; no
   new cvar. Removed from the shipped path by the gate, like the belief-store /
   composite self-tests.
   ========================================================================= */

/* Scratch entity slot the mover-state sub-test borrows for its fake mover. Any
   index < MAX_GENTITIES works; the sub-test snapshots this slot (and
   level.num_entities) and restores both, so it is byte-neutral. */
#define SEQ_TEST_MOVER_ENT (MAX_CLIENTS + 1)

static const vec3_t s_seqTestA = { 100.0f,  0.0f, 0.0f };
static const vec3_t s_seqTestB = { 900.0f,  0.0f, 0.0f };
#define SEQ_TEST_WAIT_MS 500
#define SEQ_TEST_SEQ_ID  0x53455131   /* 'SEQ1' — a non-zero active-sequence id */

/* Install the canonical probe sequence on a scratch brain. */
static void SeqTestInstall( bot_state_t *bs ) {
    botSequencedGoal_t *sg = &bs->sequencedGoal;
    WiredIntel_SequencedGoalReset( sg );
    sg->steps[0].type       = SEQ_STEP_GOTO;
    VectorCopy( s_seqTestA, sg->steps[0].point );
    sg->steps[0].range      = 64.0f;
    sg->steps[1].type       = SEQ_STEP_WAITMS;
    sg->steps[1].durationMs = SEQ_TEST_WAIT_MS;
    sg->steps[2].type       = SEQ_STEP_GOTO;
    VectorCopy( s_seqTestB, sg->steps[2].point );
    sg->steps[2].range      = 64.0f;
    sg->stepCount           = 3;
    sg->cursor              = 0;
    sg->changeTime          = level.time;
    sg->sequenceId          = SEQ_TEST_SEQ_ID;
    sg->active              = qtrue;
}

static qboolean WiredIntel_SequencedGoalSelfTest( void ) {
    bot_state_t scratch;
    botSequencedGoal_t *sg;
    qboolean pass = qtrue;

    memset( &scratch, 0, sizeof( scratch ) );
    scratch.client = 0;
    sg = &scratch.sequencedGoal;

    SeqTestInstall( &scratch );

    /* ── tick 1: bot far from A → GoTo(A) yields, cursor holds at 0 ──────── */
    /* Place the bot far from A so the arrival test fails and the step yields. */
    VectorSet( scratch.origin, -1000.0f, 0.0f, 0.0f );
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->cursor != 0 ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: GoTo(A)-far: expected cursor held at 0, got %d\n", sg->cursor );
    }
    /* the directive projection must have written A as the goal, locked. */
    if ( scratch.ltgtype != LTG_DEFENDKEYAREA ||
         Distance( scratch.teamgoal.origin, (float *)s_seqTestA ) > 0.5f ||
         !scratch.directives.directiveLocked ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: GoTo(A) projection: expected teamgoal==A + LTG_DEFENDKEYAREA + locked (ltg=%d locked=%d)\n",
            scratch.ltgtype, scratch.directives.directiveLocked );
    }

    /* ── tick 2: bot AT A → GoTo(A) done, cursor advances to WaitMs(1) ───── */
    VectorCopy( s_seqTestA, scratch.origin );
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->cursor != 1 ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: GoTo(A)-arrived: expected cursor advance to 1, got %d\n", sg->cursor );
    }

    /* ── WaitMs holds (yield) while the duration has not elapsed ─────────── */
    /* changeTime was stamped at the advance above (level.time). Poll again with
       no time elapsed: WaitMs must yield, cursor stays at 1. */
    WiredIntel_StepSequencedGoal( &scratch );
    if ( sg->cursor != 1 ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMs-within: expected cursor held at 1 (wait not elapsed), got %d\n", sg->cursor );
    }

    /* Advance the clock PAST the wait window. Verb_Wait uses '<' so the wake is
       at changeTime + durationMs < level.time; push level.time well past it. */
    level.time = sg->changeTime + SEQ_TEST_WAIT_MS + 1;
    /* Keep the bot AT B so that when WaitMs completes and the cursor advances to
       GoTo(B), GoTo(B) also completes this same walk (arrived) → cursor runs off
       the end → idle. This is the full A -> wait -> B traversal in one tick once
       the wait elapses. */
    VectorCopy( s_seqTestB, scratch.origin );
    WiredIntel_StepSequencedGoal( &scratch );
    /* After the wait elapses: WaitMs done → cursor 2 (GoTo(B)); GoTo(B) arrived
       → cursor 3 → past stepCount → idle. */
    if ( sg->active || sg->cursor != sg->stepCount ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMs-elapsed+GoTo(B): expected walk to end + idle (active=%d cursor=%d stepCount=%d)\n",
            sg->active, sg->cursor, sg->stepCount );
    }
    /* GoTo(B) must have projected B as the goal before completing. */
    if ( Distance( scratch.teamgoal.origin, (float *)s_seqTestB ) > 0.5f ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: GoTo(B) projection: expected teamgoal==B\n" );
    }
    /* Sequence done → the lock must be released. */
    if ( scratch.directives.directiveLocked ) {
        pass = qfalse;
        BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: end-of-sequence: expected directiveLocked cleared\n" );
    }

    /* ── inert-when-idle: an idle brain must be a pure no-op ─────────────── */
    {
        bot_state_t idle;
        memset( &idle, 0, sizeof( idle ) );
        /* record a sentinel; the driver must not touch any goal field when idle */
        idle.ltgtype = 4242;
        WiredIntel_StepSequencedGoal( &idle );
        if ( idle.ltgtype != 4242 ) {
            pass = qfalse;
            BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: idle-inert: driver mutated an idle brain (ltgtype changed)\n" );
        }
    }

    /* ── WAIT_MOVERSTATE: yield-until-POS2 + advance-on-POS2 + stall-abandon ─
       Drives a scratch { GoTo(A), WAIT_MOVERSTATE(fake-ent), GoTo(B) } across
       simulated frames with a fake mover whose moverState we flip by hand — no
       live bot, no map, no real mover code. Borrows one entity slot for the fake
       mover; snapshots and restores the slot + level.num_entities so the run is
       byte-neutral. */
    {
        const int      ent = SEQ_TEST_MOVER_ENT;
        gentity_t      savedEnt = g_entities[ent];   /* snapshot to restore */
        int            savedNumEnts = level.num_entities;
        int            savedTime = level.time;
        gentity_t     *mover = &g_entities[ent];

        /* Make the slot resolvable by the handler's bounds check. */
        if ( level.num_entities <= ent ) {
            level.num_entities = ent + 1;
        }

        /* Fake mover starts at POS1 (raised == not yet arrived), stationary. */
        memset( mover, 0, sizeof( *mover ) );
        mover->inuse        = qtrue;
        mover->moverState   = MOVER_POS1;
        mover->s.pos.trType = TR_STATIONARY;
        VectorSet( mover->s.pos.trBase, 10.0f, 20.0f, 30.0f );

        /* Install { GoTo(A), WAIT_MOVERSTATE(ent), GoTo(B) } and place the bot AT
           A so GoTo(A) completes immediately and the cursor lands on the wait. */
        SeqTestInstall( &scratch );
        sg->steps[1].type            = SEQ_STEP_WAIT_MOVERSTATE;
        sg->steps[1].moverEntitynum  = ent;
        sg->steps[1].durationMs      = 0;
        VectorCopy( s_seqTestA, scratch.origin );
        VectorSet( scratch.teamgoal.origin, 0.0f, 0.0f, 0.0f );

        /* tick: GoTo(A) arrives → advance; WAIT_MOVERSTATE holds (mover at POS1).
           Bot is NOT at B, so cursor parks on the wait step (1). */
        level.time = 1000;
        WiredIntel_StepSequencedGoal( &scratch );
        if ( sg->cursor != 1 || !sg->active ) {
            pass = qfalse;
            BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMoverState-POS1: expected yield at cursor 1 (active), got cursor=%d active=%d\n",
                sg->cursor, sg->active );
        }

        /* Advance the clock but keep the mover pinned at POS1 (mid-approach with
           no observable progress): still yields, still NOT stalled yet (under the
           window). Cursor holds at 1. */
        level.time += SEQ_MOVER_STALL_MS - 1;
        WiredIntel_StepSequencedGoal( &scratch );
        if ( sg->cursor != 1 || !sg->active ) {
            pass = qfalse;
            BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMoverState-pre-stall: expected still-yield at cursor 1, got cursor=%d active=%d\n",
                sg->cursor, sg->active );
        }

        /* Flip the mover to POS2 → the wait completes and advances to GoTo(B).
           Bot is still at A (not B), so GoTo(B) yields → cursor parks at 2. */
        mover->moverState = MOVER_POS2;
        WiredIntel_StepSequencedGoal( &scratch );
        if ( sg->cursor != 2 || !sg->active ) {
            pass = qfalse;
            BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMoverState-POS2: expected advance to GoTo(B) at cursor 2, got cursor=%d active=%d\n",
                sg->cursor, sg->active );
        }

        /* ── stall-abandon: fresh sequence, mover pinned unchanged past N ms ──
           Re-install the probe, park on the wait, keep the mover at POS1 and its
           trBase fixed, and push level.time past SEQ_MOVER_STALL_MS. The step
           must infer stall and ABANDON the sequence (active cleared, lock
           released) — not yield forever, not advance. */
        memset( mover, 0, sizeof( *mover ) );
        mover->inuse        = qtrue;
        mover->moverState   = MOVER_POS1;
        mover->s.pos.trType = TR_STATIONARY;
        VectorSet( mover->s.pos.trBase, 10.0f, 20.0f, 30.0f );

        SeqTestInstall( &scratch );
        sg->steps[1].type            = SEQ_STEP_WAIT_MOVERSTATE;
        sg->steps[1].moverEntitynum  = ent;
        sg->steps[1].durationMs      = 0;
        VectorCopy( s_seqTestA, scratch.origin );

        level.time = 5000;
        WiredIntel_StepSequencedGoal( &scratch );   /* GoTo(A) done → park on wait; first snapshot taken */
        if ( sg->cursor != 1 || !sg->active ) {
            pass = qfalse;
            BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMoverState-stall-setup: expected park on wait, got cursor=%d active=%d\n",
                sg->cursor, sg->active );
        }
        /* Push past the stall window with NO change to moverState/trBase. */
        level.time += SEQ_MOVER_STALL_MS + 1;
        WiredIntel_StepSequencedGoal( &scratch );
        if ( sg->active || scratch.directives.directiveLocked ) {
            pass = qfalse;
            BotAI_Print( PRT_ERROR, "SequencedGoal-selftest: WaitMoverState-stall: expected sequence abandoned (active=0, unlocked), got active=%d locked=%d\n",
                sg->active, scratch.directives.directiveLocked );
        }

        /* Restore the borrowed slot + counters — byte-neutral exit. */
        g_entities[ent]    = savedEnt;
        level.num_entities = savedNumEnts;
        level.time         = savedTime;
    }

    BotAI_Print( PRT_MESSAGE, "SequencedGoal-selftest: cursor-walk + yield + wait-until + advance (GoTo(A) -> WaitMs -> GoTo(B)) + WaitMoverState(yield-until-POS2 / advance-on-POS2 / stall-abandon) + idle-inert: %s\n",
        pass ? "ALL PASS" : "FAILURE" );
    return pass;
}

void WiredIntel_SequencedGoalSelfTestOnce( void ) {
    static qboolean s_ran = qfalse;
    if ( s_ran ) return;
    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) < 3 ) return;
    s_ran = qtrue;
    WiredIntel_SequencedGoalSelfTest();
}
