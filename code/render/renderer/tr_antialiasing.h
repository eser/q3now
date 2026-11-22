// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_ANTIALIASING_H
#define WIRED_TR_ANTIALIASING_H

// The one place that turns the anti-aliasing cvars into decisions.
//
// TAA (temporal anti-aliasing) is the only anti-aliasing technique.  The user
// surface is r_aa:
//   0       off (default).
//   1, 2    TAA.  Both values mean the same thing, because many scripts and
//           docs pass `r_aa 2`.  The entity-matrix storage buffer is the only
//           path (there is no switch for it), so TAA is never refused.
//   >= 3    behaves as TAA (the cvar range is 0..2, so it cannot be set).
//   < 0     behaves as off.
//
// The diagnostic cvar r_temporalInputTest keeps forcing the temporal path on
// regardless of r_aa.  This module holds no cvar_t so it can be tested on the
// host; the renderer feeds it through R_AntialiasingPlan() in tr_init.c.

#define R_AA_MODE_OFF   0
#define R_AA_MODE_TAA   1   // r_aa 1 and 2 (and anything above) are TAA

typedef struct {
	int mode;               // r_aa; <= 0 is off, >= R_AA_MODE_TAA is TAA
	int temporalDiagnostic; // r_temporalInputTest != 0
} rAntialiasingInput_t;

typedef struct {
	int temporalRequested;  // run the temporal path this frame
} rAntialiasingPlan_t;

void R_AntialiasingResolve( const rAntialiasingInput_t *input,
		rAntialiasingPlan_t *outPlan );

#endif // WIRED_TR_ANTIALIASING_H
