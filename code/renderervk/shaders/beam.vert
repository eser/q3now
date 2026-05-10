#version 450

/*
beam.vert — primitive beam vertex shader

Each draw issues vertexCount = 6 * BEAM_AXIAL_MAX, instanceCount =
drawCount. gl_InstanceIndex selects the beam header from binding 0;
gl_VertexIndex / 6 selects the axial copy; gl_VertexIndex % 6 selects
which of the 6 quad vertices (two triangles per axial copy).

A beam is a camera-facing two-endpoint quad: the side direction is
computed each frame from cross(beamAxis, toEye), giving a quad that
faces the camera while staying perpendicular to the beam axis.
axialCopies > 1 rotates the side direction around the beam axis at
equal angular intervals to produce the "cross" pattern (e.g.
LG-style 4-way cross at 0°/45°/90°/135°).

Vertices for axial copies above the beam's `axialCopies` field are
emitted as degenerate (clip-space-behind), so the GPU rasterizer
discards them. This wastes a few vertex shader invocations but
avoids per-beam draw calls.

Layout matches the host beamHeaderGPU_t in renderervk/vk.h.
*/

#define BEAM_AXIAL_MAX 8

layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 eyeWorld;     // .xyz = camera position in world space; .w = pad
	vec4 frameParams;  // .x = identityLight (unused by beam, kept for
	                   //      ribbon symmetry),
	                   // .y = currentTime (tr.refdef.floatTime). For
	                   //      transient beams (PRIM_FLAG_TRANSIENT
	                   //      set) this IS the uvScroll reference;
	                   //      for persistent beams the reference is
	                   //      (currentTime - hdr.spawnTime).
	                   // .zw = reserved.
	vec4 stageParams;  // .x = stageIdx (this draw's stage; cast to uint).
	                   // .yzw reserved for future per-draw stage params.
};

// Phase 5F/5G — per-stage rendering parameters extracted from the q3
// shader.script. Indexed [shaderHandle * 4 + stageIdx]. Layout
// matches VkPrimitiveStageGPU in renderervk/vk.h (32 bytes).
struct PrimitiveStageGPU {
	uint imageSlot;
	uint blendPacked;     // (srcBlend << 16) | dstBlend
	uint rgbGen;
	uint alphaGen;
	vec2 uvScale;
	vec2 uvScroll;
};

layout(std430, set = 0, binding = 2) readonly buffer Stages {
	PrimitiveStageGPU stages[];
};

// Per-shader stage count, indexed by shaderHandle. Tiny SSBO (64
// uints, 256 B). The vertex shader culls per-stage draws when this
// shader's stageCount is below the current stageIdx.
layout(std430, set = 0, binding = 3) readonly buffer StageCounts {
	uint stageCounts[];
};

// Mirrors PRIM_FLAG_TRANSIENT in primitives.h. Set by the engine when
// desc->duration <= 0 (caller should not set this manually). Routes
// uvScroll to absolute frame time so per-frame re-submission doesn't
// reset the scroll phase.
const uint PRIM_FLAG_TRANSIENT = 0x0020u;

// Mirrors PRIMITIVE_STAGE_MAX in primitives.h / vk.h. Stage data SSBO
// is laid out as `stages[primSlot * PRIMITIVE_STAGE_MAX + stageIdx]`;
// stageCounts[primSlot] tells the cull below how many of those slots
// are populated.
const uint PRIMITIVE_STAGE_MAX = 4u;

struct BeamHeader {
	vec4  start;          // .xyz = world-space resolved start, .w = pad
	vec4  end;            // .xyz = world-space resolved end,   .w = pad
	vec4  startColor;     // RGBA at start vertex, fade alpha pre-multiplied
	vec4  endColor;       // RGBA at end vertex,   fade alpha pre-multiplied
	vec2  uvScroll;       // UV/second; {0,0} = static UV
	float startWidth;     // half-width at start vertex
	float endWidth;       // half-width at end vertex; 0 = sharp taper
	float spawnTime;      // tr.refdef.floatTime at RE_AddBeamToScene
	uint  shaderHandle;   // primitive-shader-image registry slot
	uint  axialCopies;    // 1..BEAM_AXIAL_MAX; copies > this are degenerate
	uint  flags;          // PRIM_FLAG_*
};

layout(std430, set = 0, binding = 0) readonly buffer Headers {
	BeamHeader beams[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) flat out uint fragShaderHandle;
// Phase 5G: per-draw image slot lookup. Set from PrimitiveStageGPU.imageSlot
// for the current stage so the fragment shader can sample the right
// image when stage>0 uses a different image than stage 0. For LG
// (both stages share lightningbolt.jpg) this equals fragShaderHandle.
layout(location = 3) flat out uint fragImageSlot;

out gl_PerVertex {
	vec4 gl_Position;
};

void emitDegenerate() {
	// Clip-space behind the camera (z > w). Rasterizer discards.
	gl_Position      = vec4(0.0, 0.0, 2.0, 1.0);
	fragUV           = vec2(0.0);
	fragColor        = vec4(0.0);
	fragShaderHandle = 0u;
	fragImageSlot    = 0u;
}

void main() {
	uint beamIdx   = uint(gl_InstanceIndex);
	uint vertexIdx = uint(gl_VertexIndex);

	uint copyIdx  = vertexIdx / 6u;
	uint localIdx = vertexIdx % 6u;

	BeamHeader hdr = beams[beamIdx];

	// Phase 5G: per-stage cull. RB_DrawBeams loops over PRIMITIVE_STAGE_MAX
	// stages and dispatches one instanced draw per stage; shaders with
	// fewer stages must drop their over-count draws, otherwise stage 1+
	// would render with garbage stage data (the entries past stageCount
	// are zero-initialized in vk_init_primitive_shader_stages but still
	// reference imageSlot=0 = whiteImage, so without the cull we'd see
	// extra white quads).
	uint stageIdx   = uint(stageParams.x);
	uint stageCount = stageCounts[hdr.shaderHandle];
	if (stageIdx >= stageCount) {
		emitDegenerate();
		return;
	}

	PrimitiveStageGPU stage = stages[hdr.shaderHandle * PRIMITIVE_STAGE_MAX + stageIdx];

	if (copyIdx >= hdr.axialCopies) {
		emitDegenerate();
		return;
	}

	// Beam axis and length.
	vec3  startW = hdr.start.xyz;
	vec3  endW   = hdr.end.xyz;
	vec3  axis   = endW - startW;
	float len    = length(axis);
	if (len < 1e-4) {
		// Degenerate beam (start == end). Skip rather than divide by zero.
		emitDegenerate();
		return;
	}
	vec3 axisN = axis / len;

	// Camera-facing side vector. Cross(axis, toEye) gives a vector
	// perpendicular to both the beam axis (so the quad runs along
	// the beam) and the view ray (so the quad faces the camera).
	// If the camera is exactly on the beam axis, cross is near zero;
	// fall back to an arbitrary stable side perpendicular to axis.
	vec3 center = 0.5 * (startW + endW);
	vec3 toEye  = eyeWorld.xyz - center;
	vec3 sideRaw = cross(axisN, toEye);
	float sideLen = length(sideRaw);
	vec3 side;
	if (sideLen < 1e-4) {
		// Camera collinear with beam axis. Pick the world basis vector
		// least parallel to axisN as a fallback reference, then
		// re-cross. Always succeeds for any non-zero axisN.
		vec3 ad = abs(axisN);
		vec3 fb = (ad.x <= ad.y && ad.x <= ad.z) ? vec3(1.0, 0.0, 0.0)
		        : (ad.y <= ad.z)                 ? vec3(0.0, 1.0, 0.0)
		                                          : vec3(0.0, 0.0, 1.0);
		side = normalize(cross(axisN, fb));
	} else {
		side = sideRaw / sideLen;
	}

	// Apply axial-copy rotation: rotate `side` around `axisN` by
	// (copyIdx / axialCopies) * π. Half-rotation because a quad
	// covers both sides of the beam axis (so π / N gives N
	// distinguishable orientations).
	float angle = float(copyIdx) * 3.14159265358979 / float(hdr.axialCopies);
	float c     = cos(angle);
	float s     = sin(angle);
	// Rodrigues' rotation: side rotated around axisN.
	// k = axisN, v = side; v_rot = v*c + (k×v)*s + k*(k·v)*(1-c)
	vec3 rotSide = side * c
	             + cross(axisN, side) * s
	             + axisN * dot(axisN, side) * (1.0 - c);

	// Quad corners in world space, applying per-end widths:
	// startWidth at v0/v1 (start vertices), endWidth at v2/v3
	// (end vertices). When startWidth == endWidth the beam is
	// uniform; when endWidth == 0 the beam tapers to a point.
	vec3 v0 = startW - rotSide * hdr.startWidth;
	vec3 v1 = startW + rotSide * hdr.startWidth;
	vec3 v2 = endW   - rotSide * hdr.endWidth;
	vec3 v3 = endW   + rotSide * hdr.endWidth;

	// Six-vertex triangle list (no index buffer):
	//   tri 1: v0, v1, v2
	//   tri 2: v1, v3, v2
	// UV layout: u along beam (0=start, 1=end), v across width.
	// Per-vertex color: startColor at v0/v1, endColor at v2/v3;
	// fragment stage receives standard linear interpolation along
	// the beam.
	vec3 worldPos;
	vec4 vertColor;
	vec2 uv;
	switch (localIdx) {
		case 0u: worldPos = v0; vertColor = hdr.startColor; uv = vec2(0.0, 0.0); break;
		case 1u: worldPos = v1; vertColor = hdr.startColor; uv = vec2(0.0, 1.0); break;
		case 2u: worldPos = v2; vertColor = hdr.endColor;   uv = vec2(1.0, 0.0); break;
		case 3u: worldPos = v1; vertColor = hdr.startColor; uv = vec2(0.0, 1.0); break;
		case 4u: worldPos = v3; vertColor = hdr.endColor;   uv = vec2(1.0, 1.0); break;
		default: worldPos = v2; vertColor = hdr.endColor;   uv = vec2(1.0, 0.0); break;
	}

	gl_Position = mvp * vec4(worldPos, 1.0);

	// Choose the uvScroll time reference:
	//   transient beams (PRIM_FLAG_TRANSIENT, duration <= 0):
	//     scrollT = frameParams.y. The beam re-spawns each frame
	//     so spawnTime resets every submit; using absolute frame
	//     time keeps the scroll phase continuous across frames.
	//   persistent beams (engine-managed lifetime):
	//     scrollT = max(frameParams.y - spawnTime, 0). max() defends
	//     against the rare race where back-end refdef trails the
	//     front-end by < 1 frame.
	// For uvScroll == {0, 0} (helix and other consumers that don't
	// opt into scroll) both branches collapse to fragUV = uv. The
	// branch is uniform across all fragments of a single beam (every
	// fragment shares hdr.flags via gl_InstanceIndex), so this
	// compiles to a select with no real divergence.
	float scrollT;
	if ((hdr.flags & PRIM_FLAG_TRANSIENT) != 0u) {
		scrollT = frameParams.y;
	} else {
		scrollT = max(frameParams.y - hdr.spawnTime, 0.0);
	}
	// Phase 5G: per-stage UV transform. tcMod scale applies first
	// (multiplies the base [0,1] uv), then both the stage-defined
	// scroll (from shader.script tcMod scroll) and the cgame-set
	// hdr.uvScroll add together times scrollT. The additive
	// combination preserves cgame's ability to override per-frame;
	// 5H zeroes hdr.uvScroll for LG so only the stage scroll applies.
	//
	// Phase 5J: no fract() wrap. Beam binding 1 uses a REPEAT-mode
	// sampler that wraps out-of-range UVs natively. fract() wrapping
	// here would collapse V=1 to V=0 (per GLSL `fract(1.0) = 0.0`),
	// flattening the quad's V axis to a single texture row and
	// killing the beam texture's vertical variation.
	vec2 baseUV   = uv * stage.uvScale;
	vec2 totalScroll = stage.uvScroll + hdr.uvScroll;
	fragUV        = baseUV + totalScroll * scrollT;

	fragColor        = vertColor;
	fragShaderHandle = hdr.shaderHandle;
	fragImageSlot    = stage.imageSlot;
}
