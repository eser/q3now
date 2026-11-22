// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_antialiasing.h"

void R_AntialiasingResolve( const rAntialiasingInput_t *input,
		rAntialiasingPlan_t *outPlan ) {
	rAntialiasingPlan_t plan;

	if ( !outPlan ) return;
	plan.temporalRequested = 0;
	if ( !input ) {
		*outPlan = plan;
		return;
	}

	// TAA is always granted: the entity-matrix payload it needs is the only
	// path. Any r_aa >= 1 is TAA; zero and negative values are off. The
	// diagnostic cvar is independent: it forces the temporal path on whatever
	// the mode says.
	plan.temporalRequested =
		( input->temporalDiagnostic || input->mode >= R_AA_MODE_TAA ) ? 1 : 0;
	*outPlan = plan;
}
