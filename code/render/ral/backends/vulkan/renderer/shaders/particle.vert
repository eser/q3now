// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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

struct ParticleParm {
	int   calc;       // 0=CONSTANT 1=LINEAR 2=CURVE 3=CURVE_TIMES_LINEAR
	int   hasCurve;   // 0 = samples[] unused
	float val0;
	float val1;
	float variance;
	float parmPad0;
	float parmPad1;
	float parmPad2;
	float samples[8]; // PARTICLE_CURVE_SAMPLES; resolved host-side
};

// Evaluate a parm at a normalised lifetime fraction. Mirrors
// ParticleParm_Eval in qcommon/wired/render/particle_curve.c — the two are
// pinned against each other by tests/particle_curve_test.c, because a
// divergence here renders something other than what was authored without
// ever failing loudly.
float parmSampleCurve( ParticleParm p, float fraction ) {
	if ( p.hasCurve == 0 )
		return 1.0;                       // neutral, never a zeroing surprise
	if ( fraction <= 0.0 ) return p.samples[0];
	if ( fraction >= 1.0 ) return p.samples[7];
	// Samples span 0..1 INCLUSIVE, so the last sample sits AT fraction 1 and
	// the span is (N-1) intervals. Using N here shifts every curve slightly.
	float pos  = fraction * 7.0;
	int   i0   = int( pos );
	int   i1   = min( i0 + 1, 7 );
	float frac = pos - float( i0 );
	return mix( p.samples[i0], p.samples[i1], frac );
}

float parmEval( ParticleParm p, float fraction, float jitterPick ) {
	fraction = clamp( fraction, 0.0, 1.0 );
	float base;
	if ( p.calc == 1 ) {                  // LINEAR
		base = mix( p.val0, p.val1, fraction );
	} else if ( p.calc == 2 ) {           // CURVE
		base = mix( p.val0, p.val1, parmSampleCurve( p, fraction ) );
	} else if ( p.calc == 3 ) {           // CURVE_TIMES_LINEAR
		base = parmSampleCurve( p, fraction ) * mix( p.val0, p.val1, fraction );
	} else {                              // CONSTANT
		base = p.val0;
	}
	return base + p.variance * jitterPick;
}

// A parm that was never authored: constant zero. Callers fall back to the
// scalar field it overrides, which is what keeps pre-curve classes
// rendering bit-identically.
bool parmIsUnset( ParticleParm p ) {
	return p.calc == 0 && p.val0 == 0.0;
}

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
	uint  colorDomain;            // Block 5d-followup: was pad4 — CD_SRGB(0)|CD_LINEAR(1)
	                              //   of the class's resolved stage-0 image; packed into bit
	                              //   31 of particleClassHandle for the fragment stage.
	uint  pad5;
	// Sprite-frame (flipbook) animation — mirrors host particleClassGPU_t.
	uint  frameSlots[16];         // PARTICLE_CLASS_MAX_FRAMES; resolved sampler slot per frame
	uint  frameCount;             // 0/1 = static; >1 = animate
	uint  frameBlend;             // 1 = interpolate adjacent frames
	uint  framePad0;
	uint  framePad1;
	// Curve-valued parameters — mirrors particleClassGPU_t. A parm with
	// calc==0 and val0==0 was never authored; the reader falls back to the
	// scalar field it overrides, which is what keeps pre-curve classes
	// rendering bit-identically.
	ParticleParm sizeParm;
	ParticleParm alphaParm;
	ParticleParm dragParm;
	ParticleParm gravityParm;
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
	// Soft-particle depth-fade params (consumed only by particle.frag; the vertex
	// shader keeps them declared so the std140 layout matches host particleFrame_t).
	float invResX;       // 1/renderWidth
	float invResY;       // 1/renderHeight
	float depthValid;    // 1.0 when the shared scene-depth copy is fresh this frame
	float exposureBias;  // consumed only by particle.frag; kept here for std140 layout match
};

layout(std430, set = 0, binding = 1) readonly buffer Pool {
	Particle particles[];
};

layout(std430, set = 0, binding = 2) readonly buffer Classes {
	ParticleClassGPU classes[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
// per-particle class handle, flat-interpolated to fragment
// shader for the per-class sampler array lookup. Same value for all 6
// vertices of one particle (all share gl_InstanceIndex), so flat is
// uniform-correct.
//
// Block 5d-followup: bits 0..30 = classHandle (1..MAX_PARTICLE_CLASSES);
// bit 31 = the class's colour-domain (1 = CD_LINEAR → fragment samples
// raw; 0 = CD_SRGB → fragment decodes sRGB→linear). Today every
// particle-class image is CD_SRGB so bit 31 is always 0 — the fragment
// path is byte-identical to before this change.
layout(location = 2) flat out uint particleClassHandle;
// Sprite-frame (flipbook) outputs. frameSlot0 == FRAME_SLOT_NONE → the
// fragment uses the class-handle sampling path (static class, byte-
// identical). Otherwise frameSlot0/frameSlot1 are frame-pool sampler
// indices and frameBlend in [0,1) interpolates between them.
layout(location = 3) flat out uint  frameSlot0;
layout(location = 4) flat out uint  frameSlot1;
layout(location = 5)      out float frameBlend;

const uint FRAME_SLOT_NONE = 0xFFFFFFFFu;

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
	frameSlot0          = FRAME_SLOT_NONE;
	frameSlot1          = FRAME_SLOT_NONE;
	frameBlend          = 0.0;
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

	// alphaParm, when authored, REPLACES the palette lerp's alpha channel.
	// RGB still comes from the palette — a curve here is about how the
	// particle fades, not what colour it is, and keeping those separate
	// means an authored fade can be reused across differently-coloured
	// classes.
	if (!parmIsUnset(c.alphaParm))
		color.a = parmEval(c.alphaParm, p.age, p.sizeJitterPick);
	// pad5 carries the GPU-spawned semantic emitter tint as packed RGBA8.
	// Child stages selectively inherit its RGB/intensity lanes without
	// expanding the 64-byte particle pool stride.
	color *= unpackUnorm4x8(p.pad5);

	// Size: lerp (sizeStart + sizeJitterPick) → sizeEnd over lifetime.
	// sizeJitterPick was picked at emit time as crandom() * cls.sizeJitter
	// and stored on the particle; classes with sizeJitter == 0 store 0
	// here, so existing classes lerp identically to before.
	float effectiveStart = c.sizeStart + p.sizeJitterPick;
	float size = mix(effectiveStart, c.sizeEnd, p.age);

	// sizeParm, when authored, REPLACES that lerp outright — which is the
	// point: a start/end pair cannot express "grow then shrink", and a curve
	// can. The per-particle jitter still rides on top, so authored variety
	// survives the override.
	if (!parmIsUnset(c.sizeParm))
		size = parmEval(c.sizeParm, p.age, p.sizeJitterPick);

	vec3 worldPos = p.pos
	              + viewLeft.xyz * (sx * size)
	              + viewUp.xyz   * (uy * size);

	gl_Position         = mvp * vec4(worldPos, 1.0);
	fragUV              = uvCorner[vertInQuad];
	fragColor           = color;
	// Block 5d-followup: low bits carry the class handle; bit 31 carries
	// the resolved image's colour domain (c.colorDomain ∈ {0,1}).
	particleClassHandle = p.classHandle | (c.colorDomain << 31u);

	// Sprite-frame (flipbook) select. Static classes (frameCount <= 1)
	// forward the NONE sentinel so the fragment keeps the class-handle
	// sampling path byte-identical. Animated classes select the frame
	// from age (last-frame CLAMP, no wrap — one-shot semantics) and,
	// when frameBlend is set, forward the next frame + a blend factor.
	if ( c.frameCount > 1u ) {
		float fr     = p.age * float(c.frameCount);
		uint  last   = c.frameCount - 1u;
		uint  frame0 = uint(clamp(floor(fr), 0.0, float(last)));
		uint  frame1 = min(frame0 + 1u, last);
		frameSlot0 = c.frameSlots[frame0];
		frameSlot1 = c.frameSlots[frame1];
		frameBlend = (c.frameBlend != 0u) ? fract(fr) : 0.0;
	} else {
		frameSlot0 = FRAME_SLOT_NONE;
		frameSlot1 = FRAME_SLOT_NONE;
		frameBlend = 0.0;
	}
}
