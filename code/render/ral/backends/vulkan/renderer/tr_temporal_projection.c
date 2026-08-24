// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_projection.h"

#include <math.h>

int R_TemporalProjectionApply( float projection[16], uint32_t width,
		uint32_t height, const float jitterPixels[2], float outNdcOffset[2] ) {
	float ndc[2];
	float projected[2];

	if ( !projection || !jitterPixels || !outNdcOffset || !width || !height
			|| !isfinite( projection[8] ) || !isfinite( projection[9] )
			|| !isfinite( jitterPixels[0] ) || !isfinite( jitterPixels[1] ) ) {
		return 0;
	}

	ndc[0] = 2.0f * jitterPixels[0] / (float)width;
	ndc[1] = -2.0f * jitterPixels[1] / (float)height;
	projected[0] = projection[8] - ndc[0];
	projected[1] = projection[9] - ndc[1];
	if ( !isfinite( ndc[0] ) || !isfinite( ndc[1] )
			|| !isfinite( projected[0] ) || !isfinite( projected[1] ) ) {
		return 0;
	}

	projection[8] = projected[0];
	projection[9] = projected[1];
	outNdcOffset[0] = ndc[0];
	outNdcOffset[1] = ndc[1];
	return 1;
}
