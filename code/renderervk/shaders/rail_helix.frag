// #version 450 provided by rail_common.glsl

/*
===============
rail_helix.frag — additive blend with vertex color

Used for helix ribbon, debris, and sparks.
Renders as whiteShader equivalent: pure vertex color, additive blend.
===============
*/

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = fragColor;
}
