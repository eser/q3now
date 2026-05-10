/*
primitives.h — wired-render primitive descriptors

Primitive set the renderer accepts. The renderer never knows what an
effect "is" — it only knows ribbons, beams, sprites, particles, and
decals. Effect-specific composition happens in cgame and lands here
as some combination of these primitives. Effect names never cross
this boundary.

This header declares descriptor structs and a particle class handle
ONLY. No traps, no implementation. No .c file uses it yet.

Color convention: this primitive layer uses vec4 RGBA in [0..1]
floats, NOT byte modulate.rgba[4] like the legacy refEntity_t /
polyVert_t path. Reasons: HDR Vulkan + bloom downstream pipeline
expects float input; quantization-free; shader-friendly. cgame
callers that mix this layer with the legacy path are responsible
for converting at the boundary.
*/
#pragma once

#include "../../q_shared.h"

// ── handles ─────────────────────────────────────────────────────────

// Particle class handle. Registered offline (similar to qhandle_t for
// shaders); the class encodes per-particle update logic (drift,
// burst, etc.) and any static parameters that should not be
// re-shipped every frame. 0 = invalid.
typedef int particleClassHandle_t;

// ── primitive flags (shared across all primitive descriptors) ────────

#define PRIM_FLAG_CAMERA_FACING  0x0001  // billboard around view axes;
                                         //   for ribbons: ribbon plane
                                         //   recomputed per frame to
                                         //   face the camera (default
                                         //   when neither this nor
                                         //   PRIM_FLAG_VIEW_UP_PLANE is
                                         //   set, ribbons behave as
                                         //   camera-facing).
#define PRIM_FLAG_ADDITIVE       0x0002  // additive blend (else alpha).
                                         // For particle classes (phase 5
                                         // and later), blend mode is
                                         // derived from the class
                                         // shader's stages[0] stateBits
                                         // host-side at registration;
                                         // this flag becomes informational
                                         // for particle. Ribbon, sprite,
                                         // and beam still consume it.
#define PRIM_FLAG_NO_DEPTH_TEST  0x0004  // draw on top
#define PRIM_FLAG_VIEW_UP_PLANE  0x0008  // ribbon plane = ribbon axis ×
                                         //   world view-up. Stable
                                         //   orientation regardless of
                                         //   view (useful for some
                                         //   trail effects). Mutually
                                         //   exclusive with
                                         //   PRIM_FLAG_CAMERA_FACING
                                         //   (camera-facing wins if
                                         //   both are set).
#define PRIM_FLAG_CUSTOM_NORMAL  0x0010  // Ribbon-only. When set on
                                         //   ribbonDesc_t.flags, each
                                         //   point's `normal` field is
                                         //   used directly as the
                                         //   extrude axis instead of
                                         //   the camera-facing or
                                         //   view-up-plane derivation.
                                         //   Caller MUST supply a
                                         //   unit-length vector per
                                         //   point — the vertex shader
                                         //   does not normalize. Wins
                                         //   over PRIM_FLAG_CAMERA_FACING
                                         //   and PRIM_FLAG_VIEW_UP_PLANE
                                         //   when multiple are set.
                                         //   Per-point normals (rather
                                         //   than per-segment) make
                                         //   the ribbon "twist" along
                                         //   its path — required for
                                         //   path-aligned effects
                                         //   whose extrude axis
                                         //   evolves with gameplay
                                         //   geometry.
#define PRIM_FLAG_TRANSIENT      0x0020  // Beam-only (engine-managed).
                                         //   Set automatically by
                                         //   RE_AddBeamToScene when
                                         //   desc->duration <= 0; cgame
                                         //   should NOT set this flag
                                         //   manually — any caller-set
                                         //   value is overwritten by
                                         //   the engine based on the
                                         //   duration field.
                                         //
                                         //   Hint to the vertex shader
                                         //   that the beam re-spawns
                                         //   each frame: the shader
                                         //   uses absolute frame time
                                         //   (frameParams.y) as the
                                         //   uvScroll reference instead
                                         //   of (frameParams.y -
                                         //   spawnTime). Per-frame
                                         //   re-submission keeps the
                                         //   scroll continuous because
                                         //   the reference doesn't
                                         //   reset; without this flag
                                         //   transient beams have age
                                         //   == 0 every frame and
                                         //   uvScroll is non-functional.
                                         //
                                         //   Persistent beams (duration
                                         //   > 0) leave the flag clear
                                         //   and the shader uses age =
                                         //   max(frameParams.y -
                                         //   spawnTime, 0), preserving
                                         //   phase-coherent scroll over
                                         //   the beam's lifetime.

// ── ribbon ──────────────────────────────────────────────────────────

// Ribbon control point. Per-point position + width + RGBA so callers
// can build coloured strips along an arbitrary curve. The renderer
// connects consecutive points with a quad pair (or screen-aligned
// quads if PRIM_FLAG_CAMERA_FACING is set).
//
// Layout is std430-compatible with the GPU `RibbonPoint` mirror in
// ribbon.vert: the host (vec3 pos + float width) lands on GPU
// posW.xyz/.w, the host (vec3 normal + float _pad) lands on GPU
// normal.xyz/.w. Total stride is 48 B (= 3 × vec4); see
// RIBBON_POINT_BYTES in renderervk/vk.h. Memcpy is layout-correct.
typedef struct {
	vec3_t pos;
	float  width;     // half-width perpendicular to ribbon direction
	vec4_t rgba;      // [0..1]; renderer multiplies onto shader output
	vec3_t normal;    // unit extrude direction; consumed only when
	                  // PRIM_FLAG_CUSTOM_NORMAL is set on the ribbon.
	                  // When the flag is unset, the field is ignored
	                  // (vertex shader derives its own extrude axis
	                  // from view geometry). Caller MUST normalize
	                  // before submitting — the vertex shader does
	                  // not.
	float  _pad;      // pad to 16-byte stride; std430 requires the
	                  // struct's array stride to be a multiple of
	                  // its largest member alignment (vec4 = 16).
} ribbonPoint_t;

// Sanity ceiling on a single ribbon submission. Largest existing
// in-tree consumer needs 2048 control points.
#define RIBBON_MAX_POINTS 2048

typedef struct {
	const ribbonPoint_t *points;     // numPoints entries; caller-owned
	int                  numPoints;
	qhandle_t            shader;
	int                  flags;      // PRIM_FLAG_*
	// UV scroll rate in UV units per second. {0, 0} = static UV
	// (unchanged from pre-uvScroll behavior). Vertex shader applies
	//   fragUV = baseUV + uvScroll * (currentTime - spawnTime)
	// where spawnTime is captured at RE_AddRibbonToScene time. For
	// transient (per-frame) ribbons, age == 0 at first draw and the
	// scroll just begins; since ribbon has no persistent pool, each
	// re-submission resets the scroll phase.
	vec2_t               uvScroll;
} ribbonDesc_t;

// ── beam ────────────────────────────────────────────────────────────

// Two-point camera-facing quad with optional axial-copy expansion
// (cross pattern) and lifetime tracking. The renderer's beam pipeline
// expands one descriptor into N camera-facing quads per frame; cgame
// submits via trap_R_AddBeamToScene.
//
// Lifetime semantics:
//   duration == 0:  transient. Beam lives one frame. cgame must
//                   re-submit each frame to keep it visible.
//                   Typical use: continuous-fire weapon beams
//                   (lightning gun, plasma sweep). spawnTime/
//                   fadeIn/fadeOut are ignored.
//   duration  > 0:  persistent. Engine tracks spawnTime, applies
//                   fadeIn → full → fadeOut → expire. cgame submits
//                   ONCE; engine handles the rest. Typical use:
//                   short-lived event beams (chain arcs, tracer
//                   trails) with simple time-based fade.
//
// axialCopies: number of camera-facing quads to render around the
// beam axis at equal angular intervals. 1 = single flat quad
// (default, ribbon-like silhouette). 4 = cross pattern (every 45°).
// Range [1, 8]; outside values clamp.
//
// Optional entity attachment: if startEntityNum >= 0, start is
// interpreted as a LOCAL offset and the engine adds the entity's
// world origin each frame (translate-only; entity rotation is NOT
// applied — same for endEntityNum/endOffset). startEntityNum < 0
// means start is already in world space.
typedef struct {
	// Geometry — interpretation depends on entity-attachment fields.
	vec3_t    start;
	vec3_t    end;
	// Half-widths at the two ends of the beam, in world units.
	// Linearly interpolated per-vertex by the vertex shader; set
	// startWidth == endWidth for a uniform beam (legacy 5C
	// behaviour). endWidth = 0 produces a sharp taper to a point
	// at the end vertex.
	float     startWidth;
	float     endWidth;
	// RGBA at the two ends of the beam, in [0..1]. Linearly
	// interpolated per-vertex; set startColor == endColor for a
	// uniform beam (legacy 5C behaviour). The fade alpha from
	// duration/fadeIn/fadeOut multiplies BOTH endpoints' alpha
	// equally at draw time, so persistent-beam fade reads
	// correctly regardless of gradient shape.
	vec4_t    startColor;
	vec4_t    endColor;
	qhandle_t shader;         // primitive shader handle; sampler-array slot

	// Lifetime. duration == 0 means transient (one-frame).
	float     duration;       // seconds the persistent beam lives
	float     fadeIn;         // seconds (alpha 0→1 ramp at spawn)
	float     fadeOut;        // seconds (alpha 1→0 ramp before expiry)

	// Axial-copy expansion. 1..8; values outside clamp.
	int       axialCopies;

	// Entity attachment. -1 = world-static (start/end are world coords).
	// >= 0 = follow entity[N] each frame; start/end are local offsets.
	// Translate-only; entity rotation not applied.
	int       startEntityNum;
	int       endEntityNum;
	vec3_t    startOffset;    // local offset added to start entity origin
	vec3_t    endOffset;      // local offset added to end entity origin

	// UV scroll rate in UV units per second. {0, 0} = static UV.
	// Vertex shader applies
	//   fragUV = baseUV + uvScroll * (currentTime - spawnTime)
	// where spawnTime is captured at RE_AddBeamToScene time. For
	// transient beams (duration == 0) age starts at 0 each re-submit
	// and the scroll restarts. For persistent beams (duration > 0)
	// spawnTime persists across frames and the scroll phase advances
	// continuously over the beam's lifetime.
	vec2_t    uvScroll;

	int       flags;          // PRIM_FLAG_* (reserved)
} beamDesc_t;

// ── sprite ──────────────────────────────────────────────────────────

// Single billboard quad: muzzle flashes, expanding flash spheres,
// etc.
typedef struct {
	vec3_t    origin;
	float     radius;
	vec4_t    rgba;
	qhandle_t shader;
	int       flags;
} spriteDesc_t;

// ── particle emitter ────────────────────────────────────────────────

// Particle spawn request. Submitted once per shot. The particle
// class (`cls`) carries all per-particle behaviour: emit mode (point
// vs along origin→end), scatter shape, velocity shape, lifetime,
// color palette, size/gravity/drag curves. The descriptor only
// tells the system WHERE, WHICH CLASS, HOW MANY, and an optional
// per-shot tint.
typedef struct {
	particleClassHandle_t cls;
	int                   count;
	vec3_t                origin;     // emission origin
	vec3_t                axis;       // class-dependent: cone axis,
	                                  //   beam axial direction,
	                                  //   surface normal, etc.
	vec3_t                end;        // used only by classes whose
	                                  //   emitMode is along a path;
	                                  //   ignored otherwise.
	vec4_t                colorTint;  // multiplied onto the class's
	                                  //   color palette per channel
	                                  //   (incl. alpha). {1,1,1,1}
	                                  //   = no tint.
	int                   flags;      // PRIM_FLAG_* override bits
	                                  //   only (currently none —
	                                  //   reserved).
} emitterDesc_t;

// ── decal ───────────────────────────────────────────────────────────

// World-projected decal (impact mark, scorch). Flat against a
// surface; the system handles surface fitting and clipping.
typedef struct {
	vec3_t    origin;
	vec3_t    normal;
	float     radius;
	vec4_t    rgba;
	qhandle_t shader;
	int       flags;
} decalDesc_t;

// ── primitive shader stage info (Phase 5F) ────────────────────────────
//
// Per-stage rendering parameters extracted from a Q3 shader script.
// Mirrors the relevant stage features of `shaderStage_t` for the
// primitive pipeline (beams, ribbons, sprites). Other shader features
// (alphaFunc, depthFunc, depthWrite, vertex deformations, dlights,
// fog) intentionally not exposed — primitive pipeline is for unlit
// additive/blended overlay geometry; complex shading belongs in the
// regular pass.
//
// Layout chosen to be std430-friendly when serialized to SSBO (see
// VkPrimitiveStageGPU in renderervk/vk.h).

#define PRIMITIVE_STAGE_MAX 4

// Sentinel for the engine-internal qhandle→primitive-slot lookup
// table. Used at uint8_t storage; 0xFF disambiguates "registered to
// slot 0" (slot 0 is reserved for whiteImage but is a legal allocation
// outcome) from "never registered as primitive shader".
#define PRIMITIVE_SLOT_INVALID 0xFF

typedef enum {
	PRIM_RGBGEN_IDENTITY          = 0,
	PRIM_RGBGEN_IDENTITY_LIGHTING = 1,
	PRIM_RGBGEN_VERTEX            = 2,
	// Wave forms not exposed yet — most primitive shaders use static
	// modes. Adding wave: extend GPU struct to carry wave parameters;
	// this turn sticks with static modes for scope.
} primRgbGen_t;

typedef enum {
	PRIM_ALPHAGEN_IDENTITY = 0,
	PRIM_ALPHAGEN_VERTEX   = 1,
} primAlphaGen_t;

typedef enum {
	PRIM_BLEND_ZERO                = 0,
	PRIM_BLEND_ONE                 = 1,
	PRIM_BLEND_SRC_COLOR           = 2,
	PRIM_BLEND_ONE_MINUS_SRC_COLOR = 3,
	PRIM_BLEND_SRC_ALPHA           = 4,
	PRIM_BLEND_ONE_MINUS_SRC_ALPHA = 5,
	PRIM_BLEND_DST_COLOR           = 6,
	PRIM_BLEND_ONE_MINUS_DST_COLOR = 7,
	PRIM_BLEND_DST_ALPHA           = 8,
	PRIM_BLEND_ONE_MINUS_DST_ALPHA = 9,
	PRIM_BLEND_FACTOR_COUNT        = 10,
} primBlendFactor_t;

typedef struct {
	int               imageSlot;   // index into vk_primitive_shader_images
	primBlendFactor_t srcBlend;
	primBlendFactor_t dstBlend;
	vec2_t            uvScale;     // post-mul; default (1, 1); legacy tcMod scale
	vec2_t            uvScroll;    // units/second; default (0, 0); legacy tcMod scroll
	primRgbGen_t      rgbGen;
	primAlphaGen_t    alphaGen;
} primitiveShaderStage_t;

typedef struct {
	int                    stageCount;   // 1..PRIMITIVE_STAGE_MAX
	primitiveShaderStage_t stages[PRIMITIVE_STAGE_MAX];
} primitiveShaderInfo_t;
