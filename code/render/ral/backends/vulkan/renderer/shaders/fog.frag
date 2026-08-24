// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

layout(set = 0, binding = 0) uniform UBO {
	// light/env parameters:
	vec4 eyePos;				// vertex
	vec4 lightPos;				// vertex: light origin
	vec4 lightColor;			// fragment: rgb + 1/(r*r)
	vec4 lightVector;			// fragment: linear dynamic light
//#ifdef USE_FOG
	// fog parameters:
	vec4 fogDistanceVector;		// vertex
	vec4 fogDepthVector;		// vertex
	vec4 fogEyeT;				// vertex
	vec4 fogColor;				// fragment
//#endif
	// Pad from fogColor (128) over the host's q1Style / cascade / modelMatrix /
	// mvp span to the per-draw bindless index table at offset 544.
	vec4 _pad_to_packed_indices[26];  // 128 -> 544
	// Per-draw bindless index table (std140 uvec4[3] = 48 B). See vkUniform_t.
	uvec4 packed_indices[3];      // offset 544, 48 bytes
};

// legacy-mainpath-retire STEP 6.1 — fog.frag was sampling `tr.fogImage` via a
// legacy set=2 binding that STEP 2 silently retired (the ring write at
// RB_FogPass was deleted; the NULL-gap fallback substituted tr.whiteImage,
// turning the fog into a flat-coloured volume with no density ramp). 6.1
// relocates the sampler onto the RAL-owned bindless 2D table (set 7) at role
// 3 — the same role gen_frag.tmpl uses for fog. RB_FogPass now publishes the
// fog image via vk_bindless_track(3, tr.fogImage) so vk_push_bindless_indices
// pushes the correct slot through the existing FS push range at offset 96
// of vk.pipeline_layout. The density ramp is restored.
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform texture2D wired_bindless_images[];
layout(set = 1, binding = 1) uniform sampler   wired_bindless_samplers[];

// Per-role packed index lives in the set-0 UBO as uvec4[3] (packed_indices
// above); role N reads component N%4 of vec4 N/4. See gen_frag.tmpl.
#define WIRED_BINDLESS_PACKED(role) packed_indices[ (role) / 4u ][ (role) % 4u ]

// Role 3 = fog (matches gen_frag.tmpl's role assignment).
#define WIRED_BINDLESS_FOG_ROLE 3u
#define WIRED_BINDLESS_TEX(role) sampler2D( \
	wired_bindless_images  [ nonuniformEXT(   WIRED_BINDLESS_PACKED( role )         & 0xFFFu ) ], \
	wired_bindless_samplers[ nonuniformEXT( ( WIRED_BINDLESS_PACKED( role ) >> 12 ) & 0xFFu  ) ] )

//layout(location = 0) in vec4 frag_color;
//layout(location = 1) in vec2 frag_tex_coord0;
//layout(location = 2) in vec2 frag_tex_coord1;
//layout(location = 3) in vec2 frag_tex_coord2;
layout(location = 4) in vec2 fog_tex_coord;

layout(location = 0) out vec4 out_color;

//layout(constant_id = 0) const int alpha_test_func = 0;

// Precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches the other shader copies verbatim. Both helpers are unused in
// fog.frag — fogColor arrives linear (host-decoded in VK_SetFogParams)
// and fog_texture is a non-colour density ramp — so the driver DCEs
// them; kept for migration symmetry / future use.
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
    //vec4 base = frag_color * texture(texture0, frag_tex_coord0);
	//vec4 fog = texture(fog_texture, fog_tex_coord);

    //if (alpha_test_func == 1) {
    //    if (base.a == 0.0f) discard;
    //} else if (alpha_test_func == 2) {
    //    if (base.a >= 0.5f) discard;
    //} else if (alpha_test_func == 3) {
    //    if (base.a < 0.5f) discard;
    //}

	//fog = fog * fogColor;

	//out_color = mix( base, fog, fog.a );

	vec4 fog = texture( WIRED_BINDLESS_TEX( WIRED_BINDLESS_FOG_ROLE ), fog_tex_coord );
//	fog.a = 1.0;
	out_color = fog * fogColor;
}
