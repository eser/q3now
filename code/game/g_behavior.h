// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_behavior.h -- per-monster behavior-FSM state carrier (the decision layer).

A non-client monster stores its behavior state here, in a lean POD slot drawn
from an on-demand pool (behaviorPool, g_behavior.c) — the behavior-layer analog
of the nav-movement pool (botNavState_t / navEntityPool in g_bot_nav.c). A
monster that both moves and thinks acquires one slot from each pool.

The FSM state is held as a KEY (an enum value), not a function pointer, so the
state survives a savegame/version bump — the behavior-lib maps the key to the
current handler. The struct is deliberately small and self-contained (no
pointers into other heap) so it is trivially resettable and serializable later.
The decision fields the behavior-lib needs (per-mode timers, aim state, …) are
added when that lib lands; this header establishes the carrier.
===========================================================================
*/
#ifndef G_BEHAVIOR_H
#define G_BEHAVIOR_H

#include "../qcommon/q_feats.h"

#if FEAT_MONSTER_AI

/* Behavior FSM state, stored as a key (not a function pointer) so it is
 * version-portable across a savegame. The behavior-lib maps the key to a
 * handler. All states are monster-agnostic shared native-C — any monster's FSM
 * references them; the per-monster difference lives in the .lua decide table
 * (Rule-3: new monster = a table, not a new C file).
 *
 * Two composed axes share this one flat enum:
 *   AWARENESS (perception): RELAXED → QUERY → ALERT → (combat) — how aware the
 *     monster is of a threat. Drives what it does when NOT fighting.
 *   COMBAT (action): BATTLE / HUNT / TAKECOVER — the engaged sub-states, entered
 *     once a threat is confirmed. FLEE is a no-cover retreat variant.
 * The FSM escalates up the awareness axis on sensing and enters the combat
 * cluster on engaging; it de-escalates back down when the threat is lost. */
typedef enum {
	BSTATE_IDLE = 0,     /* inert default at spawn (stand still)              */
	/* ── awareness axis ── */
	BSTATE_RELAXED,      /* no threat: patrol/wander random nav points        */
	BSTATE_QUERY,        /* sensed something (heard/glimpsed): investigate it  */
	BSTATE_ALERT,        /* threat known but unseen: actively search (fast)    */
	/* ── combat axis ── */
	BSTATE_BATTLE,       /* enemy in sight + in range: face + attack, no nav  */
	BSTATE_HUNT,         /* enemy known but unseen: walk to last-known pos     */
	BSTATE_TAKECOVER,    /* low health: retreat to a cover point, regen, hold  */
	BSTATE_FLEE,         /* no-cover panic: run directly away from the enemy   */
	BSTATE_COUNT
} behaviorStateKey_t;

/* Small scratch area the behavior-lib uses across frames. POD; grows as the lib
 * needs it. The combat states use it as named below (indices are stable so the
 * slot survives a state change that hands scratch to the next state).
 *
 *   bbFloat[0..2] — last-known/last-sensed enemy position (world xyz): seeded on
 *                   sight (Battle) or hearing (Query), consumed by Hunt/Query/
 *                   Alert as their nav goal.
 *   bbInt[1]      — level.time of the last melee swing (Battle attack timer).
 *   bbInt[2]      — level.time the enemy was last cleanly SEEN (sight hysteresis).
 *   bbInt[3]      — level.time the enemy was last HEARD (hearing recency).
 * (ent->navFollower is the source of truth for whether a nav goal is live, so
 *  the blackboard does not duplicate it.)
 */
#define BEHAVIOR_BLACKBOARD_INTS   4
#define BEHAVIOR_BLACKBOARD_FLOATS 4

/* Accumulator slots the scripted `accum` verb uses for per-monster branching. */
#define SCRIPT_ACCUM_SLOTS 8

/* Named blackboard slots (see the comment above). */
#define BB_LKP_X      0   /* bbFloat: last-known/sensed-enemy-pos x */
#define BB_LKP_Y      1   /* bbFloat: last-known/sensed-enemy-pos y */
#define BB_LKP_Z      2   /* bbFloat: last-known/sensed-enemy-pos z */

typedef struct behaviorState_s {
	int      state;          /* current behaviorStateKey_t (a key, not a fn ptr) */
	int      enemy;          /* current target entity number, or ENTITYNUM_NONE  */
	int      nextThink;      /* level.time the behavior should next re-decide     */
	int      stateEntered;   /* level.time this state was entered (for timers)    */
	/* Lua-decide opt-in: qtrue when the monster is bound to a character that
	 * carries a Lua "decide" override, so the throttled decide tick asks Lua
	 * which state to enter (the native C states still EXECUTE it). qfalse keeps
	 * the monster on the pure-C FSM — the FPS floor, byte-identical to before. */
	qboolean luaDecide;
	/* Blackboard reserve — scratch for the behavior-lib (Faz-3 fills it). */
	int      bbInt[BEHAVIOR_BLACKBOARD_INTS];
	float    bbFloat[BEHAVIOR_BLACKBOARD_FLOATS];
	qboolean active;         /* qtrue while this pool slot is in use              */

	/* ── Scripted-drive carrier ───────────────────────────────────────────
	 * When a monster is driven by a set-piece script instead of the FSM, the
	 * verb-dispatcher writes its state directly. scripted flips G_RunBehavior
	 * to skip Behavior_Decide (the native movement/act layers still run). The
	 * script is a list of {verb,args} items walked by an integer cursor — the
	 * re-entrant "instruction pointer"; a blocking verb suspends by leaving the
	 * cursor put (re-entered next frame) until its wake-condition is met.
	 * Trailing-appended (memset-zero-safe): scripted defaults qfalse, so every
	 * autonomous monster keeps the exact Fork-2 layout+behavior. */
	qboolean scripted;         /* qtrue: verb-dispatcher drives state, decide skipped */
	int      scriptTarget;     /* current gotomarker goal entity, or ENTITYNUM_NONE   */
	int      scriptCursor;     /* current verb index into the script's item list (IP) */
	int      scriptChangeTime; /* level.time the cursor last advanced (wait timing)    */
	int      scriptAccum[SCRIPT_ACCUM_SLOTS]; /* accum-verb slots for branching        */
	int      sceneStartTime;   /* level.time the last playscene fired (waitscene timing) */

	/* Server-global difficulty snapshot.  All bots and monsters share g_skill;
	 * a monster resolves it once at spawn so the hot behavior loop never polls a
	 * cvar.  3 preserves the authored/default timings exactly.  This trailing
	 * POD field is savegame-safe and is the carrier for later per-character
	 * attribute bands. */
	int      difficultySkill;  /* clamped g_skill, 1..5 */
} behaviorState_t;

#endif /* FEAT_MONSTER_AI */

#endif /* G_BEHAVIOR_H */
