// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// Shadow depth fragment shader for translucent casters. Keeps a fraction of the
// fragments equal to the caster's opacity, chosen by a fixed per-shadow-texel
// interleaved-gradient pattern, and discards the rest. The lit pass filters the shadow
// map with PCF, so the surviving texels average into a partial shadow: a glass
// sphere casts a faint shadow and the opaque object inside it casts a dark one. The
// pattern is fixed (not animated) so the filtered result does not shimmer. Depth for
// surviving fragments is written by fixed-function rasterization.

layout(location = 0) flat in float in_opacity;

void main() {
	float n = fract( 52.9829189 * fract( dot( gl_FragCoord.xy, vec2( 0.06711056, 0.00583715 ) ) ) );
	if ( n >= in_opacity )
		discard;
}
