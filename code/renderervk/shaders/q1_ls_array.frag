// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// lightstyle blend + texture array animation — fragment shader
// Selects the current and next animation frame from a sampler2DArray,
// cross-fades between them, then multiplies by the 4-style lightmap sum.
//
// UBO layout mirrors vkUniform_t exactly (std140, 9 vec4s):
//   [0]  eyePos
//   [1]  light.pos   — x = tr.refdef.time (ms), y = float(numAnimFrames)
//   [2]  light.color
//   [3]  light.vector
//   [4]  fogDistanceVector
//   [5]  fogDepthVector
//   [6]  fogEyeT
//   [7]  fogColor
//   [8]  q1StyleIntensities   <- x=slot0, y=slot1, z=slot2, w=slot3

layout(set = 0, binding = 0) uniform UBO {
	vec4 _eyePos;
	vec4 _animPad;          // x = tr.refdef.time (ms); y = numAnimFrames
	vec4 _pad1;
	vec4 _pad2;
	vec4 _fogDist;
	vec4 _fogDepth;
	vec4 _fogEyeT;
	vec4 _fogColor;
	vec4 q1StyleIntensities;
};

layout(set = 1, binding = 0) uniform sampler2DArray animArray;
layout(set = 2, binding = 0) uniform sampler2D lm0;
layout(set = 3, binding = 0) uniform sampler2D lm1;
layout(set = 4, binding = 0) uniform sampler2D lm2;
layout(set = 5, binding = 0) uniform sampler2D lm3;
// set=6 intentionally unused (no diffuseMapNext in the array pipeline)

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
	// Phase 6B3'-d4-m6: sampler2DArray frame-cross-fade variant of
	// q1_ls.frag. Same colour-domain contract: animArray frames and
	// lm0..lm3 are sRGB-encoded colour — decoded to linear; the frame
	// cross-fade and the Q1 lightstyle weighted-sum × diffuse run in
	// linear domain. q1StyleIntensities are scalar weights, used raw.
	// Alpha preserved exactly (not sRGB-encoded). No Q1-overbright byte
	// boost in the lightmap loader → no LIGHTMAP_BOOST. The pre-existing
	// multi-style oscillation on batched bmodels (docs/health.md) is a
	// separate draw-order concern, not addressed here.
	float t        = _animPad.x * (1.0 / 100.0);   // convert ms → frame units (100 ms / frame)
	float nf       = max(1.0, _animPad.y);
	float animPos  = mod(t, nf);
	int   frame    = int(animPos);
	int   nextFr   = int(mod(float(frame) + 1.0, nf));
	float blend    = fract(animPos);

	vec4 diffuseA = texture(animArray, vec3(frag_tex_coord0, float(frame)));
	vec4 diffuseB = texture(animArray, vec3(frag_tex_coord0, float(nextFr)));
	diffuseA.rgb = sRGBToLinear( diffuseA.rgb );
	diffuseB.rgb = sRGBToLinear( diffuseB.rgb );
	vec4 diffuse  = mix(diffuseA, diffuseB, blend);

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
