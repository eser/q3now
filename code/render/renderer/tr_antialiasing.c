// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_antialiasing.h"

static int ClampPreset( int preset ) {
	if ( preset < 0 ) return 0;
	if ( preset > R_AA_SMAA_PRESET_MAX ) return R_AA_SMAA_PRESET_MAX;
	return preset;
}

void R_AntialiasingResolve( const rAntialiasingInput_t *input,
		rAntialiasingPlan_t *outPlan ) {
	rAntialiasingPlan_t plan;
	int preset;
	int taa;
	int smaa;

	if ( !outPlan ) return;
	plan.temporalRequested = 0;
	plan.smaaQuality = 0;
	if ( !input ) {
		*outPlan = plan;
		return;
	}

	preset = ClampPreset( input->smaaPreset );
	taa = input->mode == R_AA_MODE_TAA;
	smaa = input->mode == R_AA_MODE_SMAA;

	// The TAA mode is always granted: the entity-matrix payload it needs is the
	// only path. The diagnostic cvar is independent and unchanged: it forces the
	// temporal path on whatever the mode says.
	plan.temporalRequested = ( input->temporalDiagnostic || taa ) ? 1 : 0;

	// SMAA is suppressed only by the TAA mode.  It is deliberately not tied to
	// temporalRequested: the diagnostic must not flip SMAA.
	if ( taa ) {
		plan.smaaQuality = 0;
	} else if ( smaa && preset == 0 ) {
		plan.smaaQuality = R_AA_SMAA_PRESET_DEFAULT;
	} else {
		plan.smaaQuality = preset;
	}
	*outPlan = plan;
}
