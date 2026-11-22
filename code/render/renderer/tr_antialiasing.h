// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_ANTIALIASING_H
#define WIRED_TR_ANTIALIASING_H

// The one place that turns the anti-aliasing cvars into decisions.
//
// User surface: r_aa selects the technique.
//   0  off (default).  Behaviour is exactly the pre-r_aa behaviour: r_smaa and
//      the r_temporalInputTest diagnostic keep working as developer overrides.
//   1  SMAA.  Uses the r_smaa preset, or preset 3 (high) when r_smaa is 0.
//   2  TAA.   The entity-matrix storage buffer is the only path (there is no
//      switch for it any more), so TAA is never refused.  SMAA is off while
//      TAA is active.
//
// The diagnostic cvar r_temporalInputTest keeps forcing the temporal path on
// regardless of r_aa.  This module holds no cvar_t so it can be tested on the
// host; the renderer feeds it through R_AntialiasingPlan() in tr_init.c.

#define R_AA_MODE_OFF   0
#define R_AA_MODE_SMAA  1
#define R_AA_MODE_TAA   2

#define R_AA_SMAA_PRESET_DEFAULT 3
#define R_AA_SMAA_PRESET_MAX     4

typedef struct {
	int mode;               // r_aa; out-of-range values behave as OFF
	int smaaPreset;         // r_smaa, 0..R_AA_SMAA_PRESET_MAX
	int temporalDiagnostic; // r_temporalInputTest != 0
} rAntialiasingInput_t;

typedef struct {
	int temporalRequested;  // run the temporal path this frame
	int smaaQuality;        // effective SMAA preset, 0 = SMAA off
} rAntialiasingPlan_t;

void R_AntialiasingResolve( const rAntialiasingInput_t *input,
		rAntialiasingPlan_t *outPlan );

#endif // WIRED_TR_ANTIALIASING_H
