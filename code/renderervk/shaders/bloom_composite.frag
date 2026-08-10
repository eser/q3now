// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// Dual-filtering bloom COMPOSITE: read the top (largest) reconstructed bloom mip
// and output it scaled by the bloom intensity. The pipeline blends this onto the
// scene additively (ONE/ONE), so the scene gains `bloom * intensity`. This is the
// dual-filtering counterpart of the legacy blend.frag's `(t0+t1+t2+t3)*factor`,
// except the upsample chain has already summed the pyramid into the top mip.

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float intensity = 0.5;

void main()
{
	vec3 bloom = texture( texture0, frag_tex_coord ).rgb;
	out_color = vec4( bloom * intensity, 0.0 );
}
