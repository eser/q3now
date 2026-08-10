// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
#extension GL_EXT_nonuniform_qualifier : require
// Shadow depth fragment shader for alpha-tested (cut-out) casters. Samples the
// diffuse texture alpha at the interpolated UV through the same bindless table the
// main pass uses, and discards holed-out fragments so the cast shadow shows the
// texture's cut-out instead of a solid silhouette. Depth for surviving fragments
// is written by fixed-function rasterization (no color output). The discard
// reproduces the main pass's alpha-test exactly (gen_frag: alpha_test_func 1/2/3),
// so the shadow's hole boundary matches the lit surface's hole boundary.

// Bindless diffuse table — same image + sampler arrays the main pass binds, here
// at set 2 of the alpha-test depth layout (set 0 = entity SSBO, set 1 = cascadeMVP).
layout(set = 2, binding = 0) uniform texture2D wired_bindless_images[];
layout(set = 2, binding = 1) uniform sampler   wired_bindless_samplers[];

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) flat in uint in_packed;

void main() {
	// Low 24 bits hold the main pass's bindless pack (tex:12 | sampler:8 at bit 12);
	// the high 8 bits hold the alpha-test func. Same field layout as
	// WIRED_BINDLESS_PACK, so the diffuse here is byte-for-byte the main pass's. A
	// func of 0 means no discard (a solid silhouette) — used by the synthetic test's
	// forced-solid mode to produce the failure the holed-ness gate must catch.
	uint atFunc  = ( in_packed >> 24 ) & 0xFFu;
	uint imgSlot = in_packed & 0xFFFu;
	uint smpSlot = ( in_packed >> 12 ) & 0xFFu;
	float a = texture(
		sampler2D( wired_bindless_images  [ nonuniformEXT( imgSlot ) ],
		           wired_bindless_samplers [ nonuniformEXT( smpSlot ) ] ),
		in_texcoord ).a;
	// Mirror gen_frag's alpha-test discard: func 1 = GT0 (drop a == 0),
	// func 2 = LT128 (drop a >= 0.5), func 3 = GE128 (drop a < 0.5).
	if ( atFunc == 1u ) {
		if ( a == 0.0 ) discard;
	} else if ( atFunc == 2u ) {
		if ( a >= 0.5 ) discard;
	} else if ( atFunc == 3u ) {
		if ( a < 0.5 ) discard;
	}
	// Depth for surviving fragments is written by fixed-function rasterization.
}
