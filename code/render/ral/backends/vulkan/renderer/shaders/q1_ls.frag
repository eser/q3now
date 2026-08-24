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
	// Pad from q1StyleIntensities (128 → 144) over the host's cascade /
	// modelMatrix / mvp span to the per-draw bindless index table at 544.
	vec4 _pad_to_packed_indices[25];  // 144 -> 544
	// Per-draw bindless index table (std140 uvec4[3] = 48 B). See vkUniform_t.
	uvec4 packed_indices[3];      // offset 544, 48 bytes
};

// q1_ls.frag's six sampler2D consumers (diffuseMap + four
// style-lightmaps + diffuseMapNext) were relocated off the retired legacy
// set=1..6 ring bindings onto the RAL-owned bindless 2D table at set 7.
// Mirrors fog.frag and water.frag: same set-1 image+sampler
// arrays, same per-draw packed index table (now in the set-0 UBO). STEP 2 had
// retired the ring writes without replacement;
// the surviving sampler2D decls were silently picking up tr.whiteImage
// through the NULL-gap fallback at vk_bind_descriptor_sets, so every q1_ls-
// lit surface on a Q1/BSP2-import world rendered as a flat
// vec4(Σ intensities) whiteout. This turn restores all six.
//
//   Role 0 = diffuseMap     (current anim frame; pStage->bundle[0] @ idx)
//   Role 1 = diffuseMapNext (next anim frame;    pStage->bundle[0] @ idx+1)
//   Role 2 = lm0            (style 0 lightmap;   tr.lightmaps[lmIdx])
//   Role 3 = lm1            (style 1 lightmap;   tr.lightmapsStyle[0][lmIdx])
//   Role 4 = lm2            (style 2 lightmap;   tr.lightmapsStyle[1][lmIdx])
//   Role 5 = lm3            (style 3 lightmap;   tr.lightmapsStyle[2][lmIdx])
//
// All six are ordinary image_t entries already registered into the bindless
// table by R_CreateImage; the dispatch site (tr_shade.c q1_ls block)
// publishes them per-draw via vk_bindless_track. q1_ls_array.frag shares
// roles 2..5 for its lightmap consumers and keeps `animArray` on legacy
// set 1 (deferred to a later pass — sampler2DArray cannot live in this 2D
// table).
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform texture2D wired_bindless_images[];
layout(set = 1, binding = 1) uniform sampler   wired_bindless_samplers[];

// Per-role packed index lives in the set-0 UBO as uvec4[3] (packed_indices
// above); role N reads component N%4 of vec4 N/4. See gen_frag.tmpl.
#define WIRED_BINDLESS_PACKED(role) packed_indices[ (role) / 4u ][ (role) % 4u ]

#define WIRED_BINDLESS_DIFFUSE_ROLE       0u
#define WIRED_BINDLESS_DIFFUSE_NEXT_ROLE  1u
#define WIRED_BINDLESS_LM0_ROLE           2u
#define WIRED_BINDLESS_LM1_ROLE           3u
#define WIRED_BINDLESS_LM2_ROLE           4u
#define WIRED_BINDLESS_LM3_ROLE           5u
#define WIRED_BINDLESS_TEX(role) sampler2D( \
	wired_bindless_images  [ nonuniformEXT(   WIRED_BINDLESS_PACKED( role )         & 0xFFFu ) ], \
	wired_bindless_samplers[ nonuniformEXT( ( WIRED_BINDLESS_PACKED( role ) >> 12 ) & 0xFFu  ) ] )

layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 2) centroid in vec2 frag_tex_coord1;

layout(location = 0) out vec4 out_color;

// Precise piecewise sRGB <-> linear conversion.
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
	// decode the diffuse + lightmap colour samples to
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
	vec4 diffuseA = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_DIFFUSE_ROLE      ), frag_tex_coord0 );
	vec4 diffuseB = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_DIFFUSE_NEXT_ROLE ), frag_tex_coord0 );
	diffuseA.rgb = sRGBToLinear( diffuseA.rgb );
	diffuseB.rgb = sRGBToLinear( diffuseB.rgb );
	vec4 diffuse  = mix(diffuseA, diffuseB, animBlend);

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
