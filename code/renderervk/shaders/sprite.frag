#version 450

/*
sprite.frag — primitive sprite fragment shader

Outputs the per-vertex RGBA interpolated across the billboard quad.
Sprite consumers today (none in tree, but the pipeline is consumer-
ready) author colours at full intensity and don't go through
CGEN_VERTEX semantics on the CPU side, so no host-side halving is
applied. The push block carries frameParams.x = tr.identityLight
for symmetry with ribbon and for any future textured / CGEN_VERTEX-
equivalent sprite path that needs it; main() does not consume it.

No texture sampling for now — the bound shader handle in the header
is reserved but unused. When the texturing path lands, sample the
bound texture and multiply by fragColor here. If the future path
also has CGEN_VERTEX semantics, multiply RGB by frameParams.x at
the same point ribbon.frag does.
*/

// Same push block sprite.vert declares. main() does not currently
// read frameParams, but the declaration is kept so the push range
// layout matches the vertex stage (Vulkan requires byte-compatible
// push block declarations across stages that share the range).
layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 viewLeft;
	vec4 viewUp;
	vec4 frameParams;
};

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
	// fragUV is reserved for the future texturing path.
	outColor = fragColor;
}
