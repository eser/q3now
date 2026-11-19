// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#pragma once

#include <math.h>

/* CPU lowering of the canonical beam.vert camera-facing frame.
 * Camera position, not its forward vector, defines perspective-facing width.
 * The collinear case chooses the same least-parallel world basis as the shader;
 * this is a geometric singularity rule, not a renderer/asset fallback.
 * Returns zero for a degenerate beam without publishing partial output. */
static inline int wired_ral_beam_frame( const float start[3],
		const float end[3], const float eye[3], float outAxis[3],
		float outSide[3] )
{
	float axis[3], ray[3], side[3], length;
	for ( unsigned i = 0u; i < 3u; ++i ) {
		axis[i] = end[i] - start[i];
		ray[i] = eye[i] - 0.5f * ( start[i] + end[i] );
	}
	length = sqrtf( axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2] );
	if ( !isfinite( length ) || length < 0.0001f ) return 0;
	for ( unsigned i = 0u; i < 3u; ++i ) axis[i] /= length;
	side[0] = axis[1] * ray[2] - axis[2] * ray[1];
	side[1] = axis[2] * ray[0] - axis[0] * ray[2];
	side[2] = axis[0] * ray[1] - axis[1] * ray[0];
	length = sqrtf( side[0] * side[0] + side[1] * side[1] + side[2] * side[2] );
	if ( !isfinite( length ) ) return 0;
	if ( length < 0.0001f ) {
		const float x = fabsf( axis[0] ), y = fabsf( axis[1] ), z = fabsf( axis[2] );
		const unsigned basis = x <= y && x <= z ? 0u : ( y <= z ? 1u : 2u );
		float reference[3] = { 0.0f, 0.0f, 0.0f };
		reference[basis] = 1.0f;
		side[0] = axis[1] * reference[2] - axis[2] * reference[1];
		side[1] = axis[2] * reference[0] - axis[0] * reference[2];
		side[2] = axis[0] * reference[1] - axis[1] * reference[0];
		length = sqrtf( side[0] * side[0] + side[1] * side[1] + side[2] * side[2] );
	}
	for ( unsigned i = 0u; i < 3u; ++i ) {
		outAxis[i] = axis[i];
		outSide[i] = side[i] / length;
	}
	return 1;
}
