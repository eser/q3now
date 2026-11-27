// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_ANTIALIASING_H
#define WIRED_RENDER_ANTIALIASING_H

/* The one place that turns r_aa into decisions, for every renderer (Vulkan, Metal, OpenGL, WebGPU):
 *   0  off: no anti-aliasing.
 *   1  TAA, the default and recommended technique; SMAA is off.
 *   2  SMAA at RENDER_AA_SMAA_QUALITY; TAA is off. Kept for players whom TAA ghosting affects, discouraged otherwise.
 * r_aa is the only anti-aliasing setting: there is no SMAA preset or threshold cvar, so no other setting can turn SMAA on (each
 * renderer used to derive this mapping by hand, and two of them let an SMAA preset run SMAA with r_aa 0). Out-of-range modes
 * behave as off. A renderer with a temporal diagnostic (Vulkan's r_temporalInputTest) passes it as `temporalDiagnostic`: it forces
 * the temporal path on and never changes SMAA. Header-only and pure, so every renderer runs the code the host test covers. */

/* What `help r_aa` tells a player, on every renderer: TAA is the recommendation, SMAA the fallback for TAA ghosting. */
#define RENDER_AA_CVAR_DESCRIPTION \
	"Anti-aliasing: 0 off, 1 TAA (the default and recommended), 2 SMAA (use it only if TAA ghosting bothers you)."

#define RENDER_AA_OFF  0
#define RENDER_AA_TAA  1
#define RENDER_AA_SMAA 2

/* Where each technique runs in the frame, on every renderer:
 *   TAA   on the HDR scene, before bloom, exposure metering and the tone map: it accumulates linear radiance, and the effects after
 *         it see the anti-aliased scene.
 *   SMAA  on the tone-mapped scene (display-linear values in [0, 1], before the display encoding), before the UI: it finds and
 *         blends edges as they will look, which is what its thresholds are tuned for. Exposure metering sees the scene before SMAA.
 */

/* The SMAA preset every renderer runs (presets 1 low .. 4 ultra; 3 is high: relative-luma threshold 0.10, 16 search steps, diagonal
 * search and corner rounding). */
#define RENDER_AA_SMAA_QUALITY 3

typedef struct {
	int temporalRequested;  /* run the temporal path this frame */
	int smaaQuality;        /* the SMAA preset to run, 0 = SMAA off */
} renderAntialiasingPlan_t;

static inline renderAntialiasingPlan_t RenderAntialiasing_Resolve( int mode, int temporalDiagnostic ) {
	renderAntialiasingPlan_t plan;
	plan.temporalRequested = ( mode == RENDER_AA_TAA || temporalDiagnostic ) ? 1 : 0;
	plan.smaaQuality = mode == RENDER_AA_SMAA ? RENDER_AA_SMAA_QUALITY : 0;
	return plan;
}

#endif
