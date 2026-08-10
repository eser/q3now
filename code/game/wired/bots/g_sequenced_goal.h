// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_SEQUENCED_GOAL_H
#define G_SEQUENCED_GOAL_H

/*
===========================================================================
g_sequenced_goal.h — per-brain sequenced-goal cursor substrate for bots.

A sequenced goal is a multi-step goal whose steps run ACROSS frames with
wait-until-condition gating (e.g. drive to a point, then hold until a world
condition, then drive on). The flat single-goal model (one bs->ltgtype /
bs->teamgoal at a time) cannot express "advance only when a step reports it
is done" — so a bot on such a goal stalls.

This is the SECOND consumer of the proven cursor+yield+advance+wait-until
substrate that G_RunScriptDispatcher (g_script_verbs.c) already ships live for
monster scripts, generalized to bot goals:

  - a cursor (the instruction pointer) walks a step table one entry at a time;
  - each step returns qtrue = done → advance the cursor,
                       qfalse = suspend at this cursor, re-enter next frame (yield);
  - a step-change timestamp gives the wait-until clock-poll (the Verb_Wait analog);
  - a nested-re-entry guard bails the outer walk if a step re-entered the driver
    on the same brain (e.g. a step that triggers a combat interrupt), so the
    outer run never advances into state a nested run already moved on from.

The step table writes the bot's goal ONLY through the directive projection
(bs->ltgtype / bs->teamgoal + directiveLocked) — the same seam
BotDirective_FrameUpdate uses — so the unmodified AINode_Seek_LTG navigation
engine drives the bot. It does NOT add an AINode, a goal type, or call
trap_BotPushGoal directly (that would bypass the item-goal scoring).

The sequence state lives in the bot's OWN per-brain storage (embedded in
bot_state_t), NOT the botlib goal stack (bs->gs), so a combat-edge goal-stack
clear cannot discard an in-flight sequence. It is game-VM-internal: it never
crosses the botlib/engine ABI boundary (no syscall, no enum, no wire/savegame
field). Game-module rebuild only.

Requires: callers include g_local.h (q_shared, bg_public) and ai_main.h
          (bot_state_t) before this header so vec3_t / qboolean are visible.
===========================================================================
*/

#include "../../../qcommon/q_shared.h"

/* forward-declare the opaque brain type so this header compiles standalone;
   the impl includes ai_main.h for the full bot_state_t (matches the
   g_wiredintel.h / g_belief.h forward-declare pattern). */
struct bot_state_s;

/* ── step primitives ──────────────────────────────────────────────────────
   The kinds of step the driver can walk. The original substrate landing shipped
   the two needed to prove the driver:
     SEQ_STEP_GOTO   — drive toward a world point via the directive projection.
     SEQ_STEP_WAITMS — hold for a fixed duration (the Verb_Wait clock-poll).
   New primitives APPEND to this enum without touching the driver contract (the
   driver dispatches by kind). This is the reserved extension point — append new
   kinds before SEQ_STEP_MAX, never reorder the existing ones (the enum ordinal
   is not on any wire, but keeping order stable keeps the self-test and any
   producer table indices meaningful).

   SEQ_STEP_WAIT_MOVERSTATE — hold until a mover (by entitynum) reaches its far
     rest position (moverState == MOVER_POS2), with embedded stall-inference:
     if the mover's state stays unchanged and not-POS2 for a bounded time, the
     step infers the mover is stuck/blocked/keyed and abandons the sequence
     (give-up -> normal goal selection resumes) rather than yielding forever.
     This is the wait-until-the-elevator-arrives primitive.

   SEQ_STEP_JUMP_TOUCH -- a composed interaction step (no new actuation): once the
     bot is at the launch point, face the target, run+jump toward it, and CONFIRM
     the touch fired by watching the target mover leave its rest state. It is the
     jump-touch member of the interaction family -- the touch primitive for a button
     the bot cannot walk its bbox onto (a wall-plate over a void whose only approach
     is a run-jump arc that overlaps the button brush mid-flight).  Bounded retries;
     on exhaustion it abandons the sequence (-> normal goal selection) rather than
     looping.  Built from the existing directive projection + trap_EA_Jump /
     trap_EA_MoveForward + trap_EA_View -- no pmove / game-physics change. */
typedef enum {
    SEQ_STEP_NONE = 0,          /* empty slot / end sentinel                    */
    SEQ_STEP_GOTO,              /* drive to point (directive projection)        */
    SEQ_STEP_WAITMS,            /* hold for duration ms (clock-poll wait-until) */
    SEQ_STEP_WAIT_MOVERSTATE,   /* hold until mover at POS2 (stall-inferring)   */
    SEQ_STEP_JUMP_TOUCH,        /* run+jump to overlap a button brush mid-flight */
    SEQ_STEP_MAX
} seqStepType_t;

/* One step in a sequence. 'point'/'range' feed SEQ_STEP_GOTO; 'durationMs' feeds
   SEQ_STEP_WAITMS; 'moverEntitynum' feeds SEQ_STEP_WAIT_MOVERSTATE. A step is
   deliberately a small POD so a sequence is a plain array embedded in the brain
   — no allocation, no ABI surface.

   The three stall-* fields are the WAIT_MOVERSTATE handler's own mutable scratch
   for the embedded stall-inference: a snapshot of the mover's observed state so
   the handler can tell "still moving/progressing" (snapshot changed → re-stamp)
   from "pinned unchanged and not-POS2 for too long" (→ infer stall). They are
   unused by the other step kinds and zero for them. Storing the snapshot in the
   step (not a separate side-table) keeps the sequence a single self-contained
   POD array. */
#define SEQ_MAX_STEPS 8
typedef struct {
    seqStepType_t type;             /* which primitive                          */
    vec3_t        point;            /* GOTO / JUMP_TOUCH launch point (origin)   */
    float         range;            /* GOTO hold radius once arrived (0=default)*/
    int           durationMs;       /* WAITMS duration                          */
    int           moverEntitynum;   /* WAIT_MOVERSTATE / JUMP_TOUCH: mover/door  */
    /* WAIT_MOVERSTATE stall-inference scratch (handler-owned, mutable). All zero
       for other step kinds and on a fresh step; the handler treats a snapshot
       older than the cursor's changeTime (incl. the zeroed 0) as unsnapped. */
    int           stallMoverState;  /* last observed moverState                 */
    vec3_t        stallTrBase;      /* last observed s.pos.trBase               */
    int           stallSinceTime;   /* level.time the snapshot was last changed */
    /* JUMP_TOUCH fields (unused / zero for other kinds).  'target' is the button
       brush centre the run-jump aims at; 'jumpSpeed' is the validated launch
       speed; 'moverEntitynum' above names the DOOR whose state change confirms the
       press; the three scratch ints drive the launch cadence + bounded retries. */
    vec3_t        target;           /* JUMP_TOUCH: button brush centre to overlap */
    vec3_t        buttonMins;       /* JUMP_TOUCH: button brush AABB (arc re-solve)*/
    vec3_t        buttonMaxs;       /* JUMP_TOUCH: button brush AABB (arc re-solve)*/
    float         jumpSpeed;        /* JUMP_TOUCH: validated run speed            */
    float         firedSpeed;       /* JUMP_TOUCH: speed the live jump fired with  */
    int           doorClosedState;  /* JUMP_TOUCH: door's rest moverState (POS1)  */
    int           jumpAttempts;     /* JUMP_TOUCH: retries consumed (bounded)     */
    int           jumpPhase;        /* JUMP_TOUCH: 0=at-launch settle,1=airborne  */
    int           jumpStartTime;    /* JUMP_TOUCH: level.time the current jump began */
} seqStep_t;

/* ── the per-brain sequenced-goal cursor state ────────────────────────────
   Modeled field-for-field on the script substrate's scriptCursor /
   scriptChangeTime:
     cursor       ↔ scriptCursor      — the instruction pointer into steps[].
     changeTime   ↔ scriptChangeTime  — level.time the cursor last advanced;
                                        the WAITMS wake-condition polls against it.
   'sequenceId' is the active sequence's identity (0 = idle/no sequence); it is
   how a consumer tells "the sequence I installed is still the one running" and
   how the driver early-returns cheaply when idle. 'active' is the fast idle
   test so the driver's inert path is a single branch. */
typedef struct {
    qboolean   active;                  /* qtrue → a sequence is installed       */
    int        sequenceId;              /* active sequence identity (0 = none)   */
    int        cursor;                  /* instruction pointer into steps[]      */
    int        changeTime;             /* level.time the cursor last advanced   */
    int        stepCount;               /* live entries in steps[]               */
    seqStep_t  steps[SEQ_MAX_STEPS];    /* the step table                        */
    /* Progress watchdog for an authoritative movement goal: the LEAST ROUTE the bot
       has had left to walk on the current movement step, and the time that best was
       set.  If the bot fails to better it for the watchdog window (see the driver),
       the sequence is aborted so an authoritative-but-unfollowable goal can never
       wedge the bot.  Reset when the cursor advances (each step gets a fresh leash).

       The measure is REMAINING CORRIDOR LENGTH, not straight-line distance to the
       goal: a follower advancing along a valid corridor that bends, detours, or
       crosses an off-mesh connection holds straight-line distance flat or growing,
       so a Euclidean leash aborts a correctly-following bot.  Corridor length falls
       with every unit walked along the route whatever its shape.  'wdHaveRemaining'
       distinguishes "no corridor measured yet" from a real zero, so an unmeasurable
       bot is treated as wedged rather than as having arrived. */
    float      wdBestRemaining;         /* best (min) remaining corridor length     */
    qboolean   wdHaveRemaining;         /* qtrue once a corridor has been measured  */
    int        wdSinceTime;             /* level.time wdBestRemaining last improved */

    /* ── deferred actuation (the step's EA_* intent for THIS frame) ───────────
       A step runs BEFORE BotAI, and BotAI's first statement is trap_EA_ResetInput
       — which zeroes actionflags/dir/speed.  So an EA_* call issued by a step is
       DELETED one call later and never reaches the usercmd: the goal projection
       survives that ordering, the actuation does not.  (Measured: 54 of 54 fires
       had a reset-wipe at the identical timestamp; the working ucmd jumps in the
       same run were time-disjoint from every fire.)

       Fix at the LIFECYCLE seam, not per-action: a step RECORDS what it wants
       actuated this frame here, and the record is re-applied at the last point
       before the input is consumed (WiredIntel_ApplySequencedActuation, called
       from BotUpdateInput immediately before trap_EA_GetInput).  Re-asserting at
       the top of BotAI does NOT work — BotAI's own movement logic runs afterward
       and overwrites it (measured: 265/265 ticks still upmove=0).

       Generic by construction: 'actions' is an ACTION_* bitmask, so ANY action a
       step needs rides the same path — nothing here is jump-specific.  'frameTime'
       stamps the level.time the intent was issued so a stale record can never leak
       into a later frame; an unstamped/other-frame record applies nothing, which is
       what makes a bot with no active sequence byte-identical. */
    int        actActions;              /* ACTION_* bitmask to (re-)assert          */
    vec3_t     actMoveDir;              /* EA_Move direction (valid if actHaveMove) */
    float      actMoveSpeed;            /* EA_Move speed                            */
    qboolean   actHaveMove;             /* qtrue → re-assert the move this frame    */
    vec3_t     actViewAngles;           /* EA_View angles (valid if actHaveView)    */
    qboolean   actHaveView;             /* qtrue → re-assert the view this frame    */
    int        actFrameTime;            /* level.time the intent was recorded for   */
} botSequencedGoal_t;

/* ── lifecycle ────────────────────────────────────────────────────────────*/

/* Zero the sequence state → idle. Called once per bot at brain setup and on the
   death/respawn reset so no stale cursor survives a life. */
void WiredIntel_SequencedGoalReset( botSequencedGoal_t *sg );

/* True when an active sequence's CURRENT step is a movement goal (GOTO / JUMP_TOUCH)
   that has projected a destination the goal-arbitration gate must honor.  The single
   gate in BotGetLongTermGoal keys on this: while it holds, the step's projected goal
   is returned and item/roam/defend selection yields. False for WAIT steps (they hold
   position; normal selection is free during a wait) and for an idle brain. */
qboolean WiredIntel_SequencedGoalHasMovementGoal( const struct bot_state_s *bs );

/* ── the per-tick driver ──────────────────────────────────────────────────
   Called once per bot per think-tick at the existing driver site, before BotAI.
   Walks the cursor exactly like G_RunScriptDispatcher: run the current step,
   qfalse → return (yield, hold cursor), qtrue → advance; carries the nested
   re-entry guard. A cheap early-return when the brain has no active sequence —
   ZERO behavior change for a normal bot. */
void WiredIntel_StepSequencedGoal( struct bot_state_s *bs );

/* ── deferred actuation: record now, apply at the last seam ────────────────
   A sequenced step calls the Want* recorders INSTEAD of the raw trap_EA_* calls.
   The recorders only write the brain's own record; nothing is actuated yet.

   WiredIntel_ApplySequencedActuation() replays that record through the real
   trap_EA_* entry points.  It is called from BotUpdateInput immediately before
   trap_EA_GetInput — the last point before the input is read into the usercmd,
   and AFTER both trap_EA_ResetInput (BotAI's first statement) and BotAI's own
   movement logic.  That ordering is the whole point: an actuation issued by a
   planning layer must survive to the frame's usercmd, so it has to be asserted
   after every stage that resets or overwrites input state.

   INERT unless a step recorded an intent stamped for the CURRENT level.time: a
   bot with no active sequence records nothing, so the applier writes nothing and
   the non-sequenced input path is untouched. */
void WiredIntel_SeqWantAction( struct bot_state_s *bs, int actionFlags );
void WiredIntel_SeqWantMove( struct bot_state_s *bs, vec3_t dir, float speed );
void WiredIntel_SeqWantView( struct bot_state_s *bs, vec3_t viewangles );
void WiredIntel_ApplySequencedActuation( struct bot_state_s *bs );

/* ── self-test (out of the shipped path) ──────────────────────────────────
   Mechanically proves the cursor walk + yield + wait-until + advance on scratch
   state (never a live bot), like the composite self-test. Runs at most once per
   process, only at bot_debug >= 3 — dormant at the shipped default and at the
   debug levels normal play uses (0/1/2). Uses the existing bot_debug cvar; no
   new cvar. */
void WiredIntel_SequencedGoalSelfTestOnce( void );

#endif /* G_SEQUENCED_GOAL_H */
