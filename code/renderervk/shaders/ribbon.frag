#version 450

/*
ribbon.frag — primitive ribbon fragment shader

Output = (textured_sample × per-vertex RGBA × identityLight on RGB,
        textured_sample.a × per-vertex alpha on alpha).

The textured sample comes from the binding-2 sampler array, indexed
by the per-ribbon shader handle (flat-interpolated from the vertex
stage). Slot 0 is reserved for tr.whiteImage; out-of-range handles
clamp to slot 0, so unregistered or huge handles render
"untextured" (white texel = identity multiply on the colour path).

Alpha is identity-multiplied — matches CPU AGEN_VERTEX which doesn't
apply identityLight to alpha. The post-process gamma LUT (shifted by
overbrightBits) doubles the RGB output back at framebuffer-resolve
time, restoring visible brightness for CGEN_VERTEX-style consumers.
*/

// Must match PRIMITIVE_SHADER_IMAGE_MAX in renderervk/vk.h.
#define PRIMITIVE_SHADER_IMAGE_MAX 64

// Same push block ribbon.vert declares. ribbon.frag only consumes
// .frameParams.x (identityLight); the other fields are present so
// the layout matches the vertex stage (Vulkan requires the push
// constant declarations to be byte-compatible across stages that
// share the range).
layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 eyeWorld;
	vec4 frameParams;
};

// Per-shader-handle texture array. Populated from
// vk_primitive_shader_images[] by vk_init_primitive_shader_images
// at R_Init time and updated per-shader by
// vk_register_primitive_shader_image. Every slot is initialized to
// tr.whiteImage so unregistered handles render through identity.
layout(set = 0, binding = 2) uniform sampler2D shaderImages[PRIMITIVE_SHADER_IMAGE_MAX];

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint fragShaderHandle;

layout(location = 0) out vec4 outColor;

void main() {
	// Out-of-range handles clamp to slot 0 (tr.whiteImage) — keeps
	// the shader robust against handles that exceed the registry
	// size. The ternary compiles to a single GPU min() / cmov.
	uint slot = fragShaderHandle < uint(PRIMITIVE_SHADER_IMAGE_MAX)
		? fragShaderHandle
		: 0u;
	vec4 texel = texture(shaderImages[slot], fragUV);

	outColor = vec4(
		texel.rgb * fragColor.rgb * frameParams.x,
		texel.a   * fragColor.a
	);
}
