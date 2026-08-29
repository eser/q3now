// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

#include <math.h>

qboolean RenderUi_ProjectPoint( const refUiTransform_t *transform,
		float *x, float *y ) {
	float strength, u, v;
	float q[4][2];
	float dx1, dx2, dx3, dy1, dy2, dy3, determinant;
	float g, h, a, b, c, d, e, f, denominator;
	float projectedU, projectedV;

	if ( !transform || !x || !y
			|| transform->schemaVersion != REF_UI_TRANSFORM_SCHEMA_VERSION
			|| !isfinite( transform->x ) || !isfinite( transform->y )
			|| !isfinite( transform->width ) || transform->width <= 0.0f
			|| !isfinite( transform->height ) || transform->height <= 0.0f
			|| !isfinite( transform->perspective )
			|| fabsf( transform->perspective ) > 0.35f ) return qfalse;

	strength = fabsf( transform->perspective );
	u = ( *x - transform->x ) / transform->width;
	v = ( *y - transform->y ) / transform->height;

	/* The signed scalar mirrors a calibrated helmet-display plane. At 0.20 the
	 * near corners remain anchored while the far edge contracts, and both long
	 * edges roll toward the same vanishing direction as the DOOM 2016 reference. */
	if ( transform->perspective >= 0.0f ) {
		q[0][0] = strength * 0.25f; q[0][1] = strength * 0.65f;
		q[1][0] = 1.0f - strength * 0.25f; q[1][1] = 0.0f;
		q[2][0] = 1.0f; q[2][1] = 1.0f - strength * 1.25f;
		q[3][0] = 0.0f; q[3][1] = 1.0f;
	} else {
		q[0][0] = strength * 0.25f; q[0][1] = 0.0f;
		q[1][0] = 1.0f - strength * 0.25f; q[1][1] = strength * 0.65f;
		q[2][0] = 1.0f; q[2][1] = 1.0f;
		q[3][0] = 0.0f; q[3][1] = 1.0f - strength * 1.25f;
	}

	dx1 = q[1][0] - q[2][0]; dx2 = q[3][0] - q[2][0];
	dx3 = q[0][0] - q[1][0] + q[2][0] - q[3][0];
	dy1 = q[1][1] - q[2][1]; dy2 = q[3][1] - q[2][1];
	dy3 = q[0][1] - q[1][1] + q[2][1] - q[3][1];
	determinant = dx1 * dy2 - dx2 * dy1;
	if ( fabsf( determinant ) < 0.000001f ) return qfalse;

	g = ( dx3 * dy2 - dx2 * dy3 ) / determinant;
	h = ( dx1 * dy3 - dx3 * dy1 ) / determinant;
	a = q[1][0] - q[0][0] + g * q[1][0];
	b = q[3][0] - q[0][0] + h * q[3][0];
	c = q[0][0];
	d = q[1][1] - q[0][1] + g * q[1][1];
	e = q[3][1] - q[0][1] + h * q[3][1];
	f = q[0][1];
	denominator = g * u + h * v + 1.0f;
	if ( !isfinite( denominator ) || fabsf( denominator ) < 0.000001f ) return qfalse;

	projectedU = ( a * u + b * v + c ) / denominator;
	projectedV = ( d * u + e * v + f ) / denominator;
	if ( !isfinite( projectedU ) || !isfinite( projectedV ) ) return qfalse;
	*x = transform->x + projectedU * transform->width;
	*y = transform->y + projectedV * transform->height;
	return qtrue;
}

const renderUiPrimitive_t *RenderSubmission_UiPrimitives(
		const renderSubmissionState_t *state, uint32_t *outCount ) {
	if ( !state || !outCount || !state->initialized || !state->uiPrimitives
			|| ( !state->frameOpen && !state->frameSealed ) ) return NULL;
	*outCount = state->uiPrimitiveCount;
	return state->uiPrimitives;
}
