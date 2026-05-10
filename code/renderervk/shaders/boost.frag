#version 450

// shaders/boost.frag — post-main HDR boost pass.
//
// Samples the main color attachment, multiplies by obScale (host-
// driven via spec_constant id 1), writes the boosted value out.
// Inserted between the main render pass and the bloom-extract pass
// so that downstream stages (bloom-extract, tonemap, gamma) see
// HDR-range data instead of the pre-obScale [0,1]-ish data the
// world-surface shaders produce.
//
// Phase 6B3 path C: replaces gamma.frag's tail obScale multiply by
// moving it earlier. ribbon/beam/particle/sprite shaders keep their
// `* identityLight` halving idiom — the cancel still works because
// boost applies the same obScale, just at a different stage.
//
// 6B3a: pipeline scaffolding only; the boost pipeline is created
// but not yet bound to a dispatch. gamma.frag still applies obScale
// at the gamma pass tail until 6B3c removes it there.

layout(constant_id = 1) const float obScale = 2.0;

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec3 c = texture(texture0, frag_tex_coord).rgb;
	out_color = vec4(c * obScale, 1.0);
}
