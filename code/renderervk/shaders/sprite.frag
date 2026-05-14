// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
sprite.frag — primitive sprite fragment shader

Outputs the per-vertex RGBA interpolated across the billboard quad.
Sprite consumers today (none in tree, but the pipeline is consumer-
ready) author colours at full intensity. Phase 6B3'-a's linear-
pipeline migration removed the legacy CGEN_VERTEX halving from
every primitive; sprites never applied it and continue to render
their per-vertex RGBA as-authored. The push block's `frameParams.x`
slot is dead (host writes 1.0f for layout invariance); 6B3'-f
removes the field.

No texture sampling for now — the bound shader handle in the header
is reserved but unused. When the texturing path lands, sample the
bound texture and multiply by fragColor here.
*/

// Same push block sprite.vert declares. main() does not currently
// read frameParams, but the declaration is kept so the push range
// layout matches the vertex stage (Vulkan requires byte-compatible
// push block declarations across stages that share the range).
layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 viewLeft;
	vec4 viewUp;
	vec4 frameParams;
};

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

// Phase 6B3'-d4-m5: precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches m1/m2/m3/m4 verbatim. linearToSRGB is unused
// here — driver DCEs it; kept for migration symmetry. (When the
// texturing path lands, decode the sampled texel's rgb too.)
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

void main() {
	// fragUV is reserved for the future texturing path.
	// Phase 6B3'-d4-m5: decode the per-vertex colour to linear domain
	// (sprites have no texture yet). Alpha stays raw.
	outColor = vec4( sRGBToLinear( fragColor.rgb ), fragColor.a );
}
