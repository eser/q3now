// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_BELIEF_H
#define G_BELIEF_H

/*
===========================================================================
g_belief.h — per-brain belief store: the bot's addressable world-knowledge.

One per-brain structure (embedded in bot_state_t) that unifies the bot's
world-knowledge — previously scattered across ad-hoc, independently-maintained
fields — into a single addressable home queried through a small belief API:

  - interactables : discovered buttons/gates {origin, entitynum, associatedDoor}.
                    Backs the directive activation queue (the queue cursor lives
                    in botDirectiveState_t and addresses these records).
  - threats       : last-seen enemy positions {entitynum, origin, areanum,
                    lastSeenTime}. Recorded where the battle nodes already track
                    the enemy.
  - explored      : a bounded visited-area ring, recorded as the bot transitions
                    areas. A genuinely new producer; nothing consumes it yet.
  - items         : the item entities the bot knows about. NOT stored here — the
                    shared, event-driven item-goal DB (s_mapGoalDb) remains the
                    single maintained set; the belief API exposes it as the
                    item-belief VIEW so it is addressed through the same seam
                    without being duplicated.

The store is game-VM-internal: it never crosses the botlib/engine ABI boundary
(no syscall, no enum, no wire/savegame serialization). It is the seam the goal
tree and the future Lua-coroutine layer query; the existing subsystems become
its first clients.

Requires: callers include g_local.h (q_shared, bg_public, bot_goal_t) before
          this header so vec3_t / qboolean / bot_goal_t are visible.
===========================================================================
*/

#include "../../../qcommon/q_shared.h"

/* forward-declare opaque types so this header compiles standalone; the item-
   belief view out-copies a bot_goal_t, but callers/implementers include
   be_ai_goal.h themselves (matching g_wiredintel.h's forward-declare pattern)
   so this header does not fight the canonical bot_goal_t include ordering. */
struct bot_state_s;
struct bot_goal_s;

/* ── interactables: discovered ACTUATORS for blocking gates ───────────────
   One record per reachable actuator the bot has queued to open a blocking
   gate. 'origin' is the actuator's snapped approach point; 'entitynum'
   identifies the entity so the actuation is later confirmed from the gate's
   own mover state; 'associatedDoor' is the gate this chain opens (-1 while
   unassigned). This is the backing store for the directive activation queue —
   the queue's cursor (count/index/door) lives in botDirectiveState_t and
   indexes here.

   An actuator is defined by what the bot does AFTER ARRIVING at 'origin', and
   there are two kinds. Both share the entire navigate-to-a-point pipeline; they
   differ only in what the consumer must do on arrival. Before this distinction
   existed the only representable actuator was a button, so a gate whose chain is
   fired by a walk-through trigger could not be queued at all — the bot skipped
   such a gate and serviced a nearer door it could press but did not need. */
typedef enum {
    BELIEF_ACT_PRESS = 0,   /* func_button: stand at it, its touch handler fires  */
    BELIEF_ACT_TOUCH        /* brush trigger volume: ARRIVING is the actuation    */
} beliefActuatorKind_t;

#define BELIEF_MAX_INTERACTABLES 8
typedef struct {
    vec3_t  origin;          /* actuator approach point (snapped onto the mesh) */
    int     entitynum;       /* actuator entity number                          */
    int     associatedDoor;  /* gate entity this actuator opens (-1 = none)     */
    int     kind;            /* beliefActuatorKind_t; PRESS is 0, so a zeroed
                                record keeps its historical meaning            */
} beliefInteractable_t;

/* ── threats: last-seen enemy positions ──────────────────────────────────
   The bot's belief about where a threat was last observed. Recorded at the
   same battle-node sites that already update the enemy-tracking fields, so
   the store mirrors the live last-seen enemy without changing tracking. */
typedef struct {
    int     entitynum;       /* enemy entity number (-1 = none)               */
    vec3_t  origin;          /* last-seen enemy origin                        */
    int     areanum;         /* last-seen nav area (1 = valid sentinel)       */
    float   lastSeenTime;    /* FloatTime() the threat was last observed      */
} beliefThreat_t;

/* ── explored: bounded visited-area ring ─────────────────────────────────
   The set of nav areas the bot has recently occupied. A bounded ring (most-
   recent-N distinct areas) rather than an unbounded set — cheap, allocation-
   free, and enough for the "have I been here" query. This is the one new
   producer this landing; it is INERT w.r.t. current decisions (no existing
   code reads it — its first real consumer is a later landing). */
#define BELIEF_MAX_EXPLORED 32
typedef struct {
    int     areas[BELIEF_MAX_EXPLORED]; /* ring of recent distinct area nums  */
    int     count;                      /* live entries (<= BELIEF_MAX_EXPLORED)*/
    int     head;                       /* next write slot (ring cursor)       */
    int     lastRecorded;               /* last area recorded (transition gate)*/
} beliefExplored_t;

/* ── the per-brain belief store ──────────────────────────────────────────*/
typedef struct {
    beliefInteractable_t interactables[BELIEF_MAX_INTERACTABLES];
    int                  interactableCount; /* live interactable records      */
    beliefThreat_t       lastThreat;        /* most-recent last-seen threat    */
    beliefExplored_t     explored;          /* visited-area ring               */
} beliefStore_t;

/* ── lifecycle ───────────────────────────────────────────────────────────*/
void Belief_Init( beliefStore_t *b );

/* ── interactables (buttons/gates) — the directive queue's backing store ──*/
void Belief_InteractablesClear( struct bot_state_s *bs );
/* Append a discovered actuator record; returns the record index, or -1 if full.
   'kind' is a beliefActuatorKind_t (PRESS for a button, TOUCH for a trigger). */
int  Belief_InteractableAdd( struct bot_state_s *bs, const vec3_t origin,
                             int entitynum, int associatedDoor, int kind );
int  Belief_InteractableCount( struct bot_state_s *bs );
const beliefInteractable_t *Belief_Interactable( struct bot_state_s *bs, int index );
/* Stamp the gate every currently-queued interactable is associated with. */
void Belief_InteractablesSetDoor( struct bot_state_s *bs, int doorEntity );
/* Re-point one record's approach origin. For a TOUCH actuator the target is the
   trigger VOLUME, so the approach point is a representative of it rather than a
   fixed spot: when the first representative turns out to be unroutable, another
   point inside the same volume is an equally valid way to actuate it. */
void Belief_InteractableSetOrigin( struct bot_state_s *bs, int index,
                                   const vec3_t origin );
/* True when the bot already has a known interactable within 'radius' of origin. */
qboolean Belief_KnownInteractableNear( struct bot_state_s *bs, const vec3_t origin,
                                       float radius );

/* ── threats (last-seen enemy) ───────────────────────────────────────────*/
void Belief_RecordThreat( struct bot_state_s *bs, int entitynum,
                          const vec3_t origin, int areanum, float seenTime );
const beliefThreat_t *Belief_LastThreat( struct bot_state_s *bs );

/* ── explored areas (bounded visited-set) — the new producer ─────────────*/
void     Belief_RecordVisitedArea( struct bot_state_s *bs, int areanum );
qboolean Belief_HasVisited( struct bot_state_s *bs, int areanum );
int      Belief_VisitedCount( struct bot_state_s *bs );

/* ── items (view over the shared, event-driven item-goal DB) ──────────────
   The item-belief is NOT copied into the store — the shared s_mapGoalDb stays
   the single maintained set. These expose it read-only as the item-belief view:
   the count of known item goals and the i-th item goal (out-copied verbatim).
   Returns qfalse when the index is out of range. Implemented in g_bot_scripts.c
   (where the DB lives); declared here so the belief API is one surface. */
int      Belief_ItemBeliefCount( void );
qboolean Belief_ItemBelief( int index, struct bot_goal_s *out );

#endif /* G_BELIEF_H */
