#version 450

/*
particle.frag — wired-render particle fragment shader

Phase 5: per-class texture sampling.

The vertex shader emits the particle's class handle as a flat-
interpolated uint (location 2). The handle is the same across all
six vertices of a single particle (gl_InstanceIndex is constant per
quad), so it is uniform across the draw's invocations within the
same primitive — flat is uniform-correct, no GL_EXT_nonuniform_qualifier
needed.

Binding 3 is a fixed-size 64-element COMBINED_IMAGE_SAMPLER array
populated host-side from each class's resolved
shader→stages[0]→bundle[0]→image[0] (with three-tier fallback —
class shader, defaultShader, tr.whiteImage). Slot index = handle - 1.

Output = sampled texel × interpolated vertex color. The vertex color
already carries per-particle alpha-fade (mix(palette[0], palette[0]*colorEndMult, age))
and the class-level palette tint, so multiplication is the correct
composition.
*/

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint particleClassHandle;

// Same UBO that the vertex shader and compute shader bind. The
// fragment shader does not consume any field today, but the
// declaration is kept so the std140 layout matches host-side
// particleFrame_t and stays in lockstep with the vertex/compute
// mirrors. identityLight is reserved for a future textured path
// that needs CGEN_VERTEX-equivalent halving; particle classes
// today composite as `texel * vertex_color` with no halving
// (palette colors are authored at full intensity, additive blends
// rely on the un-halved value for visibility).
layout(set = 0, binding = 0) uniform ParticleFrame {
	mat4  mvp;
	vec4  viewLeft;
	vec4  viewUp;
	vec4  eyeWorld;
	float dt;
	uint  poolSize;
	uint  numClasses;
	uint  pingPongRead;
	float identityLight;
	float pad0;
	float pad1;
	float pad2;
};

layout(set = 0, binding = 3) uniform sampler2D particleSamplers[64];

layout(location = 0) out vec4 outColor;

void main() {
	uint idx   = particleClassHandle - 1u;
	vec4 texel = texture(particleSamplers[idx], fragUV);
	outColor   = texel * fragColor;
}
