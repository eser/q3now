// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

//layout(constant_id = 0) const float gamma = 1.0;
// Phase 6B3'-a: legacy obScale (constant_id 1) declaration removed
// — bloom-extract operates on linear values; host now feeds
// exposure_bias to constant_id 1 globally but bloom.frag does not
// consume it (bloom threshold compares to raw scene luminance).
//layout(constant_id = 2) const float saturation = 1.0;
layout(constant_id = 3) const float threshold = 0.32;
//layout(constant_id = 4) const float factor = 0.5;
// Block 3 (colour closure): extract_mode now selects which *metric*
// drives the soft-knee threshold subtraction — not a hard gate. All
// three modes extract the over-threshold *excess*, never the full
// pixel:
//   0 (default) — per-channel knee:  out.rgb = max(base.rgb - threshold, 0)
//   1 (average) — knee on the RGB mean, applied as a hue-preserving
//                 scale of the original colour
//   2 (luma)    — knee on Rec.709 luma, applied as a hue-preserving
//                 scale of the original colour
// The legacy `base_modulate` post-tweak (constant_id 6) is retired —
// the soft-knee subtraction subsumes its "emphasise the bright part"
// purpose. r_bloomModulate persists host-side as an inert cvar so old
// configs don't error; nothing reads it any more.
layout(constant_id = 5) const int extract_mode = 0;

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;
	vec3 excess;

	if ( extract_mode == 1 ) // average-driven soft knee
	{
		float m    = dot( base, vec3( 1.0 / 3.0 ) );
		float over = max( m - threshold, 0.0 );
		excess = base * ( over / max( m, 1e-5 ) );
	}
	else if ( extract_mode == 2 ) // luma-driven soft knee
	{
		const vec3 luma = vec3( 0.2126, 0.7152, 0.0722 );
		float m    = dot( luma, base );
		float over = max( m - threshold, 0.0 );
		excess = base * ( over / max( m, 1e-5 ) );
	}
	else // per-channel soft knee (default)
	{
		excess = max( base - vec3( threshold ), vec3( 0.0 ) );
	}

	out_color = vec4( excess, 1.0 );
}
