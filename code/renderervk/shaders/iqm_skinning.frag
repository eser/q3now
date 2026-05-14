// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// IQM GPU skinning fragment shader
// Receives bone-transformed normal and tangent for future normal mapping.
// Currently does simple textured output; tangent-space basis is available
// for normal map sampling when a normal map texture is bound.

layout(set = 1, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec4 frag_tangent;  // tangent (xyz) + bitangent sign (w)

layout(location = 0) out vec4 out_color;

// Phase 6B3'-d4-m3: precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches m1/m2 verbatim. linearToSRGB is unused here —
// driver DCEs it; kept for migration symmetry.
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
	// Phase 6B3'-d4-m3: decode the albedo sample to linear. texture0
	// is an sRGB-encoded colour texture (UNORM format, display-domain
	// byte content). Alpha stays raw (alpha is not sRGB-encoded).
	vec4 tex = texture(texture0, frag_tex_coord);
	out_color = vec4( sRGBToLinear( tex.rgb ), tex.a );
}
