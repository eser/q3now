#version 450

/*
particle.vert — wired-render particle vertex shader

6 vertices per particle, two triangles forming a billboard quad
oriented around camera view axes. Each draw issues vkCmdDraw(6, N)
with N = poolSize, so gl_InstanceIndex selects the particle slot.

Two pipelines are created from this same SPIR-V, differentiated by a
specialization constant PIPELINE_BLEND_MASK:
    0 = alpha-blend pipeline (only alpha particles render)
    1 = additive pipeline    (only additive particles render)
Particles whose blend variant doesn't match the bound pipeline emit a
degenerate triangle (zero-area, GPU-rasterizer-culled). Same trick for
slots that are dead (classHandle == 0 or age >= 1) or out-of-range.

Layout matches host-side mirrors. See particle_integrate.comp for the
shared struct definitions.
*/

#define PALETTE_MAX 16

struct Particle {
	vec3  pos;
	float age;
	vec3  vel;
	float lifetimeInv;
	uint  classHandle;
	uint  paletteIndex;     // phase 6; index into class.colorPalette[]
	float sizeJitterPick;   // per-particle sizeStart offset (crandom() * cls.sizeJitter)
	uint  pad1;
	uint  pad2;
	uint  pad3;
	uint  pad4;
	uint  pad5;
};

struct ParticleClassGPU {
	uint  shader;
	uint  renderFlags;
	uint  emitMode;
	uint  scatterShape;
	float scatterMagnitude;
	uint  velocityShape;
	float axialSpeed;
	float cubeJitter;
	float coneHalfAngle;
	float lifetimeMean;
	float lifetimeJitter;
	int   paletteCount;
	vec4  colorPalette[PALETTE_MAX];
	vec4  colorEndMult;
	float sizeStart;
	float sizeEnd;
	float gravityScale;
	float drag;
	uint  shaderBlendIsAdditive;  // phase 5; 0 = alpha, 1 = additive
	uint  pad1;
	uint  pad2;
	uint  pad3;
	// Expressivity extension. .w lanes of the two vec4s unused.
	vec4  velocityBias;           // .xyz post-shape constant
	vec4  velocityBiasJitter;     // .xyz post-shape symmetric jitter
	float speedJitter;            // axialSpeed scatter at emit time
	float sizeJitter;             // sizeStart scatter at emit time
	uint  pad4;
	uint  pad5;
};

layout(set = 0, binding = 0) uniform ParticleFrame {
	mat4  mvp;
	vec4  viewLeft;
	vec4  viewUp;
	vec4  eyeWorld;
	float dt;
	uint  poolSize;
	uint  numClasses;
	uint  pingPongRead;
	float identityLight;   // tr.identityLight; CGEN_VERTEX-equivalent halving
	float pad0;
	float pad1;
	float pad2;
};

layout(std430, set = 0, binding = 1) readonly buffer Pool {
	Particle particles[];
};

layout(std430, set = 0, binding = 2) readonly buffer Classes {
	ParticleClassGPU classes[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
// Phase 5: per-particle class handle, flat-interpolated to fragment
// shader for the per-class sampler array lookup. Same value for all 6
// vertices of one particle (all share gl_InstanceIndex), so flat is
// uniform-correct.
layout(location = 2) flat out uint particleClassHandle;

out gl_PerVertex {
	vec4 gl_Position;
};

// Specialization constant: 0 for alpha pipeline, 1 for additive.
// Set at pipeline creation via VkSpecializationInfo (see vk.c).
layout(constant_id = 0) const uint PIPELINE_BLEND_MASK = 0u;

const float sideSign[6] = float[6](+1.0, +1.0, -1.0,  -1.0, +1.0, -1.0);
const float upSign[6]   = float[6](-1.0, +1.0, -1.0,  -1.0, +1.0, +1.0);
const vec2  uvCorner[6] = vec2[6](
	vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0),
	vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)
);

void emitDegenerate() {
	gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
	fragUV    = vec2(0.0);
	fragColor = vec4(0.0);
	// Any valid array index works — fragment will discard via
	// gl_Position being zero-area. Use 1 (corresponds to slot 0
	// after the handle-1 decrement) which is always populated
	// (tr.whiteImage at init, or a registered class).
	particleClassHandle = 1u;
}

void main() {
	uint particleIdx = uint(gl_InstanceIndex);
	uint vertInQuad  = uint(gl_VertexIndex) % 6u;

	if (particleIdx >= poolSize) {
		emitDegenerate();
		return;
	}

	Particle p = particles[particleIdx];

	if (p.classHandle == 0u || p.age >= 1.0 || p.classHandle > numClasses) {
		emitDegenerate();
		return;
	}

	ParticleClassGPU c = classes[p.classHandle - 1u];

	// Filter by blend variant. PIPELINE_BLEND_MASK is 0 (alpha) or 1
	// (additive) per pipeline; class.shaderBlendIsAdditive is derived
	// host-side from the class shader's stages[0]→stateBits at
	// registration time. Particle goes through this pipeline only if
	// its derived flag matches the pipeline's mask. The opposite
	// pipeline's draw call will pick up the rest.
	if (c.shaderBlendIsAdditive != PIPELINE_BLEND_MASK) {
		emitDegenerate();
		return;
	}

	float sx = sideSign[vertInQuad];
	float uy = upSign[vertInQuad];

	// Color: per-particle palette sample (phase 6). p.paletteIndex
	// was written at emit time as rand() % paletteCount; modulo
	// here is defensive against any class whose paletteCount
	// drifts from what the host clamp guarantees. max(.., 1u)
	// avoids division-by-zero if paletteCount somehow lands at 0.
	uint paletteIdx = p.paletteIndex
	                % uint(max(c.paletteCount, 1));
	vec4 baseColor  = c.colorPalette[paletteIdx];
	vec4 endColor   = baseColor * c.colorEndMult;
	vec4 color      = mix(baseColor, endColor, p.age);

	// Size: lerp (sizeStart + sizeJitterPick) → sizeEnd over lifetime.
	// sizeJitterPick was picked at emit time as crandom() * cls.sizeJitter
	// and stored on the particle; classes with sizeJitter == 0 store 0
	// here, so existing classes lerp identically to before.
	float effectiveStart = c.sizeStart + p.sizeJitterPick;
	float size = mix(effectiveStart, c.sizeEnd, p.age);

	vec3 worldPos = p.pos
	              + viewLeft.xyz * (sx * size)
	              + viewUp.xyz   * (uy * size);

	gl_Position         = mvp * vec4(worldPos, 1.0);
	fragUV              = uvCorner[vertInQuad];
	fragColor           = color;
	particleClassHandle = p.classHandle;
}
