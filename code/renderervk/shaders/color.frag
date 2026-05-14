// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

layout(location = 0) out vec4 out_color;

layout (constant_id = 4) const int color_mode = 0;

// Phase 6B3'-d4-m1: precise piecewise sRGB -> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Driver DCE removes the function from compiled
// pipelines that don't call it. Matches A2/A3/A4's body verbatim.
vec3 sRGBToLinear( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.04045 ) );
	vec3 lo = c / 12.92;
	vec3 hi = pow( ( c + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
	return mix( hi, lo, vec3( cutoff ) );
}

vec3 linearToSRGB( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.0031308 ) );
	vec3 lo = c * 12.92;
	vec3 hi = pow( c, vec3( 1.0 / 2.4 ) ) * 1.055 - 0.055;
	return mix( hi, lo, vec3( cutoff ) );
}

void main()
{
	// Phase 6B3'-d4-m1: the literals below are authored as sRGB-display
	// values (classic 8-bit byte conventions); color.frag writes into
	// the linear-HDR FBO, so each is decoded to linear radiance here.
	// Pure 0.0 / 1.0 channels are unchanged by the transfer function;
	// the 0.2 / 0.33 mid-tones are not. The inputs are compile-time
	// constants, so the piecewise math folds at pipeline-specialisation
	// time (zero GPU cost). linearToSRGB is unused here — driver DCEs it;
	// it's kept for symmetry with the rest of the linear migration.
	if ( color_mode == 1 )
		out_color = vec4( sRGBToLinear( vec3( 1.0, 1.0, 1.0 ) ), 1.0 ); // white
	else
	if ( color_mode == 2 )
		out_color = vec4( sRGBToLinear( vec3( 0.2, 1.0, 0.2 ) ), 1.0 ); // green
	else
	if ( color_mode == 3 )
		out_color = vec4( sRGBToLinear( vec3( 1.0, 0.33, 0.2 ) ), 1.0 ); // red
	else
		out_color = vec4( sRGBToLinear( vec3( 0.0, 0.0, 0.0 ) ), 1.0 ); // black
}
