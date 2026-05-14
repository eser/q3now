// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// lightstyle blend — fragment shader
// Blends 4 per-slot lightmaps weighted by q1StyleIntensities from UBO,
// then multiplies by the diffuse texture.
//
// UBO layout mirrors vkUniform_t exactly (std140, 9 vec4s):
//   [0]  eyePos
//   [1]  light.pos   / ent.color[0]
//   [2]  light.color / ent.color[1]
//   [3]  light.vector/ ent.color[2]
//   [4]  fogDistanceVector
//   [5]  fogDepthVector
//   [6]  fogEyeT
//   [7]  fogColor
//   [8]  q1StyleIntensities   <- x=slot0, y=slot1, z=slot2, w=slot3

layout(set = 0, binding = 0) uniform UBO {
	vec4 _eyePos;
	vec4 _animPad;          // x = animBlend (0..1 within a 100ms frame); y/z/w unused
	vec4 _pad1;
	vec4 _pad2;
	vec4 _fogDist;
	vec4 _fogDepth;
	vec4 _fogEyeT;
	vec4 _fogColor;
	vec4 q1StyleIntensities;
};

layout(set = 1, binding = 0) uniform sampler2D diffuseMap;
layout(set = 2, binding = 0) uniform sampler2D lm0;
layout(set = 3, binding = 0) uniform sampler2D lm1;
layout(set = 4, binding = 0) uniform sampler2D lm2;
layout(set = 5, binding = 0) uniform sampler2D lm3;
layout(set = 6, binding = 0) uniform sampler2D diffuseMapNext; // next anim frame; same as diffuseMap when not animated

layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 2) centroid in vec2 frag_tex_coord1;

layout(location = 0) out vec4 out_color;

// Phase 6B3'-d4-m6: precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches m1/m2/m3/m4/m5 verbatim. linearToSRGB is unused
// here — driver DCEs it; kept for migration symmetry.
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
	// Phase 6B3'-d4-m6: decode the diffuse + lightmap colour samples to
	// linear, then run the Q1 lightstyle model in linear domain — surface
	// diffuse × sum(lightmap_s × intensity_s). The weighted sum is a true
	// sum of light contributions and the diffuse × light product is a
	// physical modulation; both are only colorimetrically correct in
	// linear. q1StyleIntensities are per-style scalar weights (not colour)
	// — used raw. Alpha (diffuse cross-fade × Σ weighted lightmap alpha)
	// is preserved exactly; alpha is not sRGB-encoded. No Q1-overbright
	// byte boost exists in the lightmap loader (LM_FillPatch writes the
	// byte verbatim), so there is no LIGHTMAP_BOOST here — brightness is
	// carried entirely by the style intensities. NOT addressed here: the
	// pre-existing multi-style shading oscillation on batched bmodels
	// (docs/health.md "Q1 multi-style lightmap shading oscillation") —
	// that's a draw-order / per-surface-styles concern, separate from this
	// colour-domain migration; its behaviour may shift slightly post-m6.
	float animBlend = _animPad.x;
	vec4 diffuseA = texture(diffuseMap,     frag_tex_coord0);
	vec4 diffuseB = texture(diffuseMapNext, frag_tex_coord0);
	diffuseA.rgb = sRGBToLinear( diffuseA.rgb );
	diffuseB.rgb = sRGBToLinear( diffuseB.rgb );
	vec4 diffuse  = mix(diffuseA, diffuseB, animBlend);

	vec4 t0 = texture(lm0, frag_tex_coord1); t0.rgb = sRGBToLinear( t0.rgb );
	vec4 t1 = texture(lm1, frag_tex_coord1); t1.rgb = sRGBToLinear( t1.rgb );
	vec4 t2 = texture(lm2, frag_tex_coord1); t2.rgb = sRGBToLinear( t2.rgb );
	vec4 t3 = texture(lm3, frag_tex_coord1); t3.rgb = sRGBToLinear( t3.rgb );

	vec4 lm = t0 * q1StyleIntensities.x
	        + t1 * q1StyleIntensities.y
	        + t2 * q1StyleIntensities.z
	        + t3 * q1StyleIntensities.w;

	out_color = diffuse * lm;
}
