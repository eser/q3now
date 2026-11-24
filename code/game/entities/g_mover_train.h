// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_MOVER_TRAIN_H
#define G_MOVER_TRAIN_H

/* Include after g_local.h (it has no include guard of its own). */

/* func_train: one path-following core (g_mover_q3.c) driven by per-format rules, so the Quake 3 train and the Quake 1 train
 * (g_mover_q1.c) share the corner linking, the leg schedule and the pushing, and differ only where their games differ.
 *
 * Legs are tick-exact: a leg starts at the instant the previous one ended (or its wait expired), not at the server frame that
 * noticed it, so a train's phase does not depend on sv_fps. The train is left at the corner it reached; the next server frame
 * pushes it through the rest of the frame's travel, so riders and obstacles see that motion. */

typedef struct {
	qboolean cornerIsMins;      /* the corner marks the train's mins (Quake 1 bmodels: origin 0, world-space bounds), not its origin */
	qboolean cornerSpeed;       /* a corner's "speed" overrides the train's for the leg that leaves it (Quake 3) */
	qboolean waitAtStart;       /* the first corner's "wait" also delays the first leg (Quake 3) */
	qboolean negativeWaitStops; /* "wait" < 0 parks the train at that corner for good (Quake 1) */
	int      defaultWaitMs;     /* pause at a corner with no "wait" (Quake 1: 100 ms) */
	qboolean fireCornerTargets; /* a corner fires its other targets when reached (Quake 3) */
} trainRules_t;

/* Links every path_corner of the train's loop (q1_ prefixed corners included). False, with a log line, on a broken path. */
qboolean G_TrainLinkCorners( gentity_t *ent );
/* The train is at the corner it was heading to (or is starting): starts the next leg, or the wait before it, per `rules`. */
void     G_TrainAdvance( gentity_t *ent, const trainRules_t *rules );

#endif
