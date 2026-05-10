#version 450

/*
beam.frag — primitive beam fragment shader

Output = sampled texel × per-vertex color (with fade alpha already
premultiplied host-side). Same shared-sampler-array texturing scheme
ribbon uses; binding index differs (beam: 1, ribbon: 2) because beam's
descriptor set has no points SSBO. Slot 0 is reserved for tr.whiteImage,
so unregistered or out-of-range handles render "untextured" (white
texel × color = color).

Beam does NOT apply CGEN_VERTEX-equivalent halving. Per-vertex color
arrives via the `fragColor` varying — beam.vert linearly interpolates
between hdr.startColor and hdr.endColor across the beam's length —
and the texel-modulate is the only color transformation in the
fragment stage.
*/

#define PRIMITIVE_SHADER_IMAGE_MAX 64

// Same push block beam.vert declares; beam.frag does not consume any
// push field today (texel × color is the entire computation), but the
// declaration must match the vertex stage byte layout for Vulkan to
// accept the pipeline.
layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 eyeWorld;
	vec4 frameParams;
	vec4 stageParams;     // Phase 5G — per-draw stage index, kept in
	                      // sync with beam.vert's push declaration.
};

layout(set = 0, binding = 1) uniform sampler2D shaderImages[PRIMITIVE_SHADER_IMAGE_MAX];

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint fragShaderHandle;
// Phase 5G: per-stage image slot. Set in beam.vert from the stage's
// PrimitiveStageGPU.imageSlot. When every stage of a multi-stage
// shader samples the same image (the common case for additive
// scrolling textures registered through RE_RegisterPrimitiveShader)
// this equals fragShaderHandle; when stages use distinct images, the
// vertex shader resolves the right slot per draw.
layout(location = 3) flat in uint fragImageSlot;

layout(location = 0) out vec4 outColor;

void main() {
	uint slot = fragImageSlot < uint(PRIMITIVE_SHADER_IMAGE_MAX)
		? fragImageSlot
		: 0u;
	vec4 texel = texture(shaderImages[slot], fragUV);

	outColor = texel * fragColor;
}
