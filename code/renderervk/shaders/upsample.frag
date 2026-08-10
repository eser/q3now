// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// CoD/Jimenez dual-filtering UPSAMPLE (Jimenez 2014, "Next Generation Post
// Processing in Call of Duty: Advanced Warfare").
//
// A 3x3 tent filter over the smaller (just-produced) mip, scaled by a filter
// radius so the kernel footprint widens the glow smoothly. Weights:
//   [ 1 2 1 ]
//   [ 2 4 2 ] * 1/16
//   [ 1 2 1 ]
//
// This shader OUTPUTS only the tent-filtered sample; the progressive additive
// accumulation onto the next-larger mip is the pipeline's job (ONE/ONE blend
// onto a LOADed target), so the larger mip keeps its own downsample result and
// gains this reconstructed contribution. The smooth tent reconstruction is what
// removes the blocky/pixelized edges of a naive single-tap upscale.

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

// 1 / source-resolution (the smaller mip being read), baked per level.
layout(constant_id = 0) const float texel_x = 0.0;
layout(constant_id = 1) const float texel_y = 0.0;
// Tent footprint scale (P1 default ~1.0; tuned in P2).
layout(constant_id = 2) const float filter_radius = 1.0;

void main()
{
	vec2 uv = frag_tex_coord;
	vec2 r  = vec2( texel_x, texel_y ) * filter_radius;

	vec3 s =  texture( texture0, uv + r * vec2( -1.0, -1.0 ) ).rgb;
	s += 2.0 * texture( texture0, uv + r * vec2(  0.0, -1.0 ) ).rgb;
	s +=       texture( texture0, uv + r * vec2(  1.0, -1.0 ) ).rgb;

	s += 2.0 * texture( texture0, uv + r * vec2( -1.0,  0.0 ) ).rgb;
	s += 4.0 * texture( texture0, uv                          ).rgb;
	s += 2.0 * texture( texture0, uv + r * vec2(  1.0,  0.0 ) ).rgb;

	s +=       texture( texture0, uv + r * vec2( -1.0,  1.0 ) ).rgb;
	s += 2.0 * texture( texture0, uv + r * vec2(  0.0,  1.0 ) ).rgb;
	s +=       texture( texture0, uv + r * vec2(  1.0,  1.0 ) ).rgb;

	out_color = vec4( s * ( 1.0 / 16.0 ), 1.0 );
}
