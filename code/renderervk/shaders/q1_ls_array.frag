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
	// Pad from q1StyleIntensities (128 → 144) over the host's cascade /
	// modelMatrix / mvp span to the per-draw bindless index table at 544.
	vec4 _pad_to_packed_indices[25];  // 144 -> 544
	// Per-draw bindless index table (std140 uvec4[3] = 48 B). See vkUniform_t.
	uvec4 packed_indices[3];      // offset 544, 48 bytes
};

// q1_ls_array.frag now consumes only set 0 (uniform)
// and set 7 (bindless 2D + 2DArray). Both the sampler2D lightmaps and
// the sampler2DArray animArray ride the RAL-owned bindless table
// — the 2D entries via binding=0 (`wired_bindless_images[]`, texture2D[]),
// the 2DArray entry via binding=2 (`wired_bindless_image_arrays[]`,
// texture2DArray[]). Both bindings are SAMPLED_IMAGE at the
// VkDescriptorSetLayout side; the SPIR-V dimension declaration is what the
// driver enforces, so the 2DArray view must be written into binding=2 (not
// binding=0). The sampler dedup-pool array at binding=1 is shared between
// 2D and 2DArray reads — the same VkSampler can sample either dimension.
// Same per-draw packed index table (now in the set-0 UBO); the role's
// uint32 means a 2D slot when read via WIRED_BINDLESS_TEX and a 2DArray
// slot when read via WIRED_BINDLESS_TEX_ARRAY. The dispatch site decides
// which space the per-role slot value names by which image_t it publishes
// via vk_bindless_track.
//
//   Role 0 = animArray (2DArray; tess.shader->q1AnimArray)
//   Role 2 = lm0       (2D; style 0;  tr.lightmaps[lmIdx])
//   Role 3 = lm1       (2D; style 1;  tr.lightmapsStyle[0][lmIdx])
//   Role 4 = lm2       (2D; style 2;  tr.lightmapsStyle[1][lmIdx])
//   Role 5 = lm3       (2D; style 3;  tr.lightmapsStyle[2][lmIdx])
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform texture2D      wired_bindless_images       [];
layout(set = 1, binding = 1) uniform sampler        wired_bindless_samplers     [];
layout(set = 1, binding = 2) uniform texture2DArray wired_bindless_image_arrays [];

// Per-role packed index lives in the set-0 UBO as uvec4[3] (packed_indices
// above); role N reads component N%4 of vec4 N/4. See gen_frag.tmpl.
#define WIRED_BINDLESS_PACKED(role) packed_indices[ (role) / 4u ][ (role) % 4u ]

#define WIRED_BINDLESS_ANIMARRAY_ROLE  0u
#define WIRED_BINDLESS_LM0_ROLE        2u
#define WIRED_BINDLESS_LM1_ROLE        3u
#define WIRED_BINDLESS_LM2_ROLE        4u
#define WIRED_BINDLESS_LM3_ROLE        5u
#define WIRED_BINDLESS_TEX(role) sampler2D( \
	wired_bindless_images  [ nonuniformEXT(   WIRED_BINDLESS_PACKED( role )         & 0xFFFu ) ], \
	wired_bindless_samplers[ nonuniformEXT( ( WIRED_BINDLESS_PACKED( role ) >> 12 ) & 0xFFu  ) ] )
#define WIRED_BINDLESS_TEX_ARRAY(role) sampler2DArray( \
	wired_bindless_image_arrays[ nonuniformEXT(   WIRED_BINDLESS_PACKED( role )         & 0xFFFu ) ], \
	wired_bindless_samplers    [ nonuniformEXT( ( WIRED_BINDLESS_PACKED( role ) >> 12 ) & 0xFFu  ) ] )

layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 2) centroid in vec2 frag_tex_coord1;

layout(location = 0) out vec4 out_color;

// precise piecewise sRGB <-> linear conversion.
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
	// sampler2DArray frame-cross-fade variant of
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

	vec4 diffuseA = texture( WIRED_BINDLESS_TEX_ARRAY( WIRED_BINDLESS_ANIMARRAY_ROLE ), vec3( frag_tex_coord0, float( frame  ) ) );
	vec4 diffuseB = texture( WIRED_BINDLESS_TEX_ARRAY( WIRED_BINDLESS_ANIMARRAY_ROLE ), vec3( frag_tex_coord0, float( nextFr ) ) );
	diffuseA.rgb = sRGBToLinear( diffuseA.rgb );
	diffuseB.rgb = sRGBToLinear( diffuseB.rgb );
	vec4 diffuse  = mix(diffuseA, diffuseB, blend);

	vec4 t0 = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_LM0_ROLE ), frag_tex_coord1 ); t0.rgb = sRGBToLinear( t0.rgb );
	vec4 t1 = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_LM1_ROLE ), frag_tex_coord1 ); t1.rgb = sRGBToLinear( t1.rgb );
	vec4 t2 = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_LM2_ROLE ), frag_tex_coord1 ); t2.rgb = sRGBToLinear( t2.rgb );
	vec4 t3 = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_LM3_ROLE ), frag_tex_coord1 ); t3.rgb = sRGBToLinear( t3.rgb );

	vec4 lm = t0 * q1StyleIntensities.x
	        + t1 * q1StyleIntensities.y
	        + t2 * q1StyleIntensities.z
	        + t3 * q1StyleIntensities.w;

	out_color = diffuse * lm;
}
