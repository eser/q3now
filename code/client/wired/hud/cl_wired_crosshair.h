// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_wired_crosshair.h -- Wired Crosshair: procedural per-weapon reticle

The procedural crosshair system (design spec: procedural-crosshair-handoff).
Three-layer Lua definition (default base -> weapon static base -> per-frame
update(state) delta), deep-merged into a draw-spec the crosshair HUD element
renders with solid-color quad/ring primitives.

This module hosts the Lua pipeline (System VM, trusted authored content),
loads scripts/crosshair/default.lua + weapons/<name>/crosshair.lua, exposes
the q3.* constant/helper table, and evaluates the merged draw-spec per frame.
State data (weapon, speed, fire/crouch, trace target) is staged by cgame into
the Wired Store under crosshair.* keys and read back here.
===========================================================================
*/

#ifndef CL_WIRED_CROSSHAIR_H
#define CL_WIRED_CROSSHAIR_H

#include "../../../qcommon/q_shared.h"

/* ── draw-spec (§5.2) ──────────────────────────────────────────────────
 * The fully-merged, per-frame reticle description the element draws. A
 * stack-allocated scratch struct — no per-frame heap (spec §8). */

typedef struct {
	qboolean enabled;
	float    length;
	float    thickness;
} cgCrosshairArm_t;

typedef struct {
	qboolean visible;     /* top-level hide gate (default true) */

	float    scale;       /* multiplier on all sizes/distances */
	vec4_t   color;       /* RGBA 0..1 */
	float    gap;         /* center -> inner edge of each arm */

	cgCrosshairArm_t arms[4];   /* top, right, bottom, left (see CG_XH_ARM_*) */

	struct { qboolean enabled; float radius; qboolean filled; } dot;
	struct { qboolean enabled; float radius; float thickness; qboolean filled; } ring;
	struct { qboolean enabled; float offset; float arm_length; float thickness; } corners;
	struct { float thickness; float alpha; } outline;
} cgCrosshairDrawSpec_t;

/* arms[] indices — render order / spec key order */
#define CG_XH_ARM_TOP    0
#define CG_XH_ARM_RIGHT  1
#define CG_XH_ARM_BOTTOM 2
#define CG_XH_ARM_LEFT   3

/* ── lifecycle ─────────────────────────────────────────────────────────
 * WiredCrosshair_Init() registers the q3.* Lua bindings (via
 * WiredScript_RegisterBindings) and queues the script load to run after the
 * VM's PostInit. Call once during client init alongside WiredStoreLua_Init. */
void WiredCrosshair_Init( void );
void WiredCrosshair_Shutdown( void );

/* Per-frame evaluation. Reads the staged crosshair.* state, selects the
 * cached per-weapon base, runs update(state) when the weapon is dynamic, and
 * deep-merges into `out`. Returns qtrue if a spec was produced (out is valid),
 * qfalse if the system is unavailable (caller should draw nothing). */
qboolean WiredCrosshair_Eval( cgCrosshairDrawSpec_t *out );

#endif /* CL_WIRED_CROSSHAIR_H */
