// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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

#define PRIM_FLAG_CAMERA_FACING            \
	0x0001 // billboard around view axes;  \
		   //   for ribbons: ribbon plane  \
		   //   recomputed per frame to    \
		   //   face the camera (default   \
		   //   when neither this nor      \
		   //   PRIM_FLAG_VIEW_UP_PLANE is \
		   //   set, ribbons behave as     \
		   //   camera-facing).
#define PRIM_FLAG_ADDITIVE                                                \
	0x0002							   // additive blend (else alpha).    \
									   // For particle classes (phase 5   \
									   // and later), blend mode is       \
									   // derived from the class          \
									   // shader's stages[0] stateBits    \
									   // host-side at registration;      \
									   // this flag becomes informational \
									   // for particle. Ribbon, sprite,   \
									   // and beam still consume it.
#define PRIM_FLAG_NO_DEPTH_TEST 0x0004 // draw on top
#define PRIM_FLAG_VIEW_UP_PLANE            \
	0x0008 // ribbon plane = ribbon axis × \
		   //   world view-up. Stable      \
		   //   orientation regardless of  \
		   //   view (useful for some      \
		   //   trail effects). Mutually   \
		   //   exclusive with             \
		   //   PRIM_FLAG_CAMERA_FACING    \
		   //   (camera-facing wins if     \
		   //   both are set).
#define PRIM_FLAG_CUSTOM_NORMAL              \
	0x0010 // Ribbon-only. When set on       \
		   //   ribbonDesc_t.flags, each     \
		   //   point's `normal` field is    \
		   //   used directly as the         \
		   //   extrude axis instead of      \
		   //   the camera-facing or         \
		   //   view-up-plane derivation.    \
		   //   Caller MUST supply a         \
		   //   unit-length vector per       \
		   //   point — the vertex shader    \
		   //   does not normalize. Wins     \
		   //   over PRIM_FLAG_CAMERA_FACING \
		   //   and PRIM_FLAG_VIEW_UP_PLANE  \
		   //   when multiple are set.       \
		   //   Per-point normals (rather    \
		   //   than per-segment) make       \
		   //   the ribbon "twist" along     \
		   //   its path — required for      \
		   //   path-aligned effects         \
		   //   whose extrude axis           \
		   //   evolves with gameplay        \
		   //   geometry.
#define PRIM_FLAG_TRANSIENT                 \
	0x0020 // Beam-only (engine-managed).   \
		   //   Set automatically by        \
		   //   RE_AddBeamToScene when      \
		   //   desc->duration <= 0; cgame  \
		   //   should NOT set this flag    \
		   //   manually — any caller-set   \
		   //   value is overwritten by     \
		   //   the engine based on the     \
		   //   duration field.             \
		   //                               \
		   //   Hint to the vertex shader   \
		   //   that the beam re-spawns     \
		   //   each frame: the shader      \
		   //   uses absolute frame time    \
		   //   (frameParams.y) as the      \
		   //   uvScroll reference instead  \
		   //   of (frameParams.y -         \
		   //   spawnTime). Per-frame       \
		   //   re-submission keeps the     \
		   //   scroll continuous because   \
		   //   the reference doesn't       \
		   //   reset; without this flag    \
		   //   transient beams have age    \
		   //   == 0 every frame and        \
		   //   uvScroll is non-functional. \
		   //                               \
		   //   Persistent beams (duration  \
		   //   > 0) leave the flag clear   \
		   //   and the shader uses age =   \
		   //   max(frameParams.y -         \
		   //   spawnTime, 0), preserving   \
		   //   phase-coherent scroll over  \
		   //   the beam's lifetime.
#define PRIM_FLAG_PARTICLE_MOTION_TRAIL       \
	0x0040 // Particle-only. Stretch the       \
		   //   billboard from its current       \
		   //   position toward its recent       \
		   //   ballistic history. This is a     \
		   //   GPU vertex presentation flag:    \
		   //   simulation stays one particle,   \
		   //   with no CPU trail children or    \
		   //   per-frame effect resubmission.   \
		   //   Used for idTech-style motion     \
		   //   trails on falling impact sparks.
#define PRIM_FLAG_PARTICLE_SURFACE_ANCHORED    \
	0x0080 // Particle-only. The emitter is     \
		   //   intentionally anchored just in   \
		   //   front of an impact surface. Keep  \
		   //   depth testing, but do not apply   \
		   //   soft-particle intersection fade: \
		   //   that fade would punch a hole in   \
		   //   impact smoke exactly where its    \
		   //   underlying surface decal lives.   \
		   //   This is not a draw-on-top flag;   \
		   //   other geometry can still occlude  \
		   //   the particle normally.

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
// RIBBON_POINT_BYTES in code/render/ral/backends/vulkan/renderer/vk.h. Memcpy is layout-correct.
typedef struct {
	vec3_t pos;
	float  width;  // half-width perpendicular to ribbon direction
	vec4_t rgba;   // [0..1]; renderer multiplies onto shader output
	vec3_t normal; // unit extrude direction; consumed only when
				   // PRIM_FLAG_CUSTOM_NORMAL is set on the ribbon.
				   // When the flag is unset, the field is ignored
				   // (vertex shader derives its own extrude axis
				   // from view geometry). Caller MUST normalize
				   // before submitting — the vertex shader does
				   // not.
	float _pad;	   // pad to 16-byte stride; std430 requires the
				   // struct's array stride to be a multiple of
				   // its largest member alignment (vec4 = 16).
} ribbonPoint_t;

// Sanity ceiling on a single ribbon submission. Largest existing
// in-tree consumer needs 2048 control points.
#define RIBBON_MAX_POINTS 2048

typedef struct {
	const ribbonPoint_t *points; // numPoints entries; caller-owned
	int					 numPoints;
	qhandle_t			 shader;
	int					 flags; // PRIM_FLAG_*
	// UV scroll rate in UV units per second. {0, 0} = static UV
	// (unchanged from pre-uvScroll behavior). Vertex shader applies
	//   fragUV = baseUV + uvScroll * (currentTime - spawnTime)
	// where spawnTime is captured at RE_AddRibbonToScene time. For
	// transient (per-frame) ribbons, age == 0 at first draw and the
	// scroll just begins; since ribbon has no persistent pool, each
	// re-submission resets the scroll phase.
	vec2_t uvScroll;
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
	vec3_t start;
	vec3_t end;
	// Half-widths at the two ends of the beam, in world units.
	// Linearly interpolated per-vertex by the vertex shader; set
	// startWidth == endWidth for a uniform beam (legacy 5C
	// behaviour). endWidth = 0 produces a sharp taper to a point
	// at the end vertex.
	float startWidth;
	float endWidth;
	// RGBA at the two ends of the beam, in [0..1]. Linearly
	// interpolated per-vertex; set startColor == endColor for a
	// uniform beam (legacy 5C behaviour). The fade alpha from
	// duration/fadeIn/fadeOut multiplies BOTH endpoints' alpha
	// equally at draw time, so persistent-beam fade reads
	// correctly regardless of gradient shape.
	vec4_t	  startColor;
	vec4_t	  endColor;
	qhandle_t shader; // primitive shader handle; sampler-array slot

	// Lifetime. duration == 0 means transient (one-frame).
	float duration; // seconds the persistent beam lives
	float fadeIn;	// seconds (alpha 0→1 ramp at spawn)
	float fadeOut;	// seconds (alpha 1→0 ramp before expiry)

	// Axial-copy expansion. 1..8; values outside clamp.
	int axialCopies;

	// Entity attachment. -1 = world-static (start/end are world coords).
	// >= 0 = follow entity[N] each frame; start/end are local offsets.
	// Translate-only; entity rotation not applied.
	int	   startEntityNum;
	int	   endEntityNum;
	vec3_t startOffset; // local offset added to start entity origin
	vec3_t endOffset;	// local offset added to end entity origin

	// UV scroll rate in UV units per second. {0, 0} = static UV.
	// Vertex shader applies
	//   fragUV = baseUV + uvScroll * (currentTime - spawnTime)
	// where spawnTime is captured at RE_AddBeamToScene time. For
	// transient beams (duration == 0) age starts at 0 each re-submit
	// and the scroll restarts. For persistent beams (duration > 0)
	// spawnTime persists across frames and the scroll phase advances
	// continuously over the beam's lifetime.
	vec2_t uvScroll;

	int flags; // PRIM_FLAG_* (reserved)
} beamDesc_t;

// ── rail ribbon (parametric spiral) ─────────────────────────────────
//
// A GPU-resident helix ribbon: the caller submits the spawn-fixed spiral
// parameters ONCE at fire time; the renderer's persistent pool tracks the
// lifetime and its vertex shader regenerates the evolving spiral geometry
// every frame from (currentTime - spawnTime). Unlike ribbonDesc_t (which
// carries a caller-built point array rebuilt each frame), nothing here is
// per-frame: the whole helix — expanding radius, unwinding spacing,
// widening width, per-point fade, and the point count itself — is a
// function of the age fraction and is derived analytically GPU-side.
//
// All fields are POD (no nested pointers), so the descriptor crosses the
// VM boundary by value like beamDesc_t. The 36-entry perpAxis ring is the
// precomputed rotation frame (mirrors railTrail_t.perpAxis); the shader
// indexes it by (segment * RAIL_RIBBON_ROTATION) % 36.
//
// The evolution constants live below (RAIL_RIBBON_*), shared verbatim by
// the cgame emitter, the renderer, and the vertex shader (passed in as
// specialization constants) so the three can never drift.
#define RAIL_RIBBON_MAX_SEGMENTS	2048   // worst-case control points (== MAX_RAIL_SEGMENTS)
#define RAIL_RIBBON_RING_COUNT		36	   // perpAxis ring entries
#define RAIL_RIBBON_SPACING			3.0f   // base ring spacing at spawn (unwinds down)
#define RAIL_RIBBON_WIDTH_BASE		1.5f   // base half-width at spawn (widens up)
#define RAIL_RIBBON_ROTATION		2	   // ring steps per segment
#define RAIL_RIBBON_RADIUS_BASE		2.0f   // radius at spawn (grows to BASE+GROW)
#define RAIL_RIBBON_RADIUS_GROW		2.0f   // radius growth over life (2 → 4)
#define RAIL_RIBBON_WIDTH_GROW		1.5f   // width growth factor over life (×2.5)
#define RAIL_RIBBON_SPACING_TIGHTEN 0.667f // spacing unwind factor over life

typedef struct {
	vec3_t	  start;							   // spiral origin (world)
	vec3_t	  beamAxis;							   // normalized beam direction
	float	  perpAxis[RAIL_RIBBON_RING_COUNT][3]; // precomputed rotation ring (36 × vec3)
	float	  beamLen;							   // total beam length
	vec4_t	  color;							   // base RGBA [0..1] (per-point fade applied GPU-side)
	float	  duration;							   // seconds the helix lives (= RAIL_TRAILTIME/1000)
	qhandle_t shader;							   // primitive shader handle; sampler-array slot
	int		  flags;							   // PRIM_FLAG_* (reserved)
} railRibbonDesc_t;

// ── sprite ──────────────────────────────────────────────────────────

// Single billboard quad: muzzle flashes, expanding flash spheres,
// etc.
typedef struct {
	vec3_t	  origin;
	float	  radius;
	vec4_t	  rgba;
	qhandle_t shader;
	int		  flags;
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
	int					  count;
	vec3_t				  origin; // emission origin
	vec3_t				  axis;	  // class-dependent: cone axis,
								  //   beam axial direction,
								  //   surface normal, etc.
	vec3_t end;					  // used only by classes whose
								  //   emitMode is along a path;
								  //   ignored otherwise.
	vec4_t colorTint;			  // multiplied onto the class's
								  //   color palette per channel
								  //   (incl. alpha). {1,1,1,1}
								  //   = no tint.
	int flags;					  // PRIM_FLAG_* override bits
								  //   only (currently none —
								  //   reserved).
} emitterDesc_t;

// ── decal ───────────────────────────────────────────────────────────

// World-projected decal (impact mark, scorch). Flat against a
// surface; the renderer projects + clips it onto whatever it overlaps.
// origin/normal/radius define the projection box; orientation rotates
// the decal's tangent frame around the normal (the texture's roll, what
// CG_ImpactMark picks per impact). reserved[] is a frozen tail so later
// projector phases can add data without re-touching this shared ABI.
typedef struct {
	vec3_t	  origin;
	vec3_t	  normal;
	float	  radius;
	float	  orientation; // radians, rotation of the texture around `normal`
	vec4_t	  rgba;
	qhandle_t shader;
	int		  flags;	// DECAL_FLAG_* bits (see below); 0 = a normal world-projected mark
	float	  lifetime; // seconds until the mark fully fades + expires (0 = no auto-fade,
						// stays until its ring slot is reused). Was reserved[0]: int→float
						// is the same 4 bytes at the same offset, so the struct layout is
						// UNCHANGED (no ABI lockstep hazard — the reserved tail's purpose).
	int reserved[1];	// frozen for future projector fields (P3)
} decalDesc_t;

// decalDesc_t.flags bits.
//   DECAL_FLAG_NO_PROJECT — render a FREE flat quad at the descriptor's explicit
//   `origin` Z (no scene-depth box-projection, no discard-on-no-solid). For marks
//   that must sit at a caller-computed world height rather than conform to the
//   nearest solid — e.g. the player water-line wake, which lies on the water plane
//   where no solid exists. Flag clear (default) = the normal world-projected mark
//   (impact/blood/scorch), unchanged.
#define DECAL_FLAG_NO_PROJECT 0x1

// ── lens-source occlusion (lens-glow unification) ─────────────────────
// A coarse, emit-and-forget descriptor: the game registers a light source
// (origin + radius + a stable id) and the renderer's depth-sampling lens
// oracle reports back a 0..1 visibility (is the source occluded by scene
// geometry?). This is the gpu-offload-principle's thin channel — the GPU
// does the occlusion (it needs the depth buffer); the game keeps the
// game-state composition (the multi-layer flare sprites stay in cgame). The
// descriptor is FLAT (no embedded pointers) so it crosses the VM boundary as
// a single bounds-checked struct, like decalDesc_t. reserved[] is a frozen
// tail for forward growth without re-touching this shared ABI.
typedef struct {
	int		  id;		   // stable per-source key (the caller's light index); maps to a registry slot
	vec3_t	  origin;	   // world position of the light source
	float	  radius;	   // source radius (drives the oracle's screen-space sample disc)
	vec4_t	  rgba;		   // tint * intensity (informational; the oracle is occlusion-only today)
	qhandle_t shader;	   // optional source shader (informational; mirrors decalDesc_t.shader)
	int		  flags;	   // reserved for future LENS_FLAG_* bits (none defined yet)
	int		  reserved[2]; // frozen for forward growth
} lensSourceDesc_t;

// Lens-source id banding (shared cgame↔renderer). The renderer maps a registered
// source to a registry slot via slot = base + (desc.id % LENS_MAX_CGSOURCES), so
// disjoint id ranges land in disjoint slots — the three source kinds (map-flares,
// missiles, powerups) never alias. The CALLER pre-bands its id into the right
// range; the renderer stays kind-agnostic. Map-flares use the raw light index
// (< MAX_LENS_FLARE_ENTITIES = 256); entity-keyed sources wrap their entity
// number into a per-kind 32-slot band (≤32 concurrent missiles/powerups is ample;
// rare same-kind merge is a cosmetic visibility-share, never cross-kind).
#define LENS_BAND_MAPFLARE 0   // [0,256)   map-light index
#define LENS_BAND_MISSILE  256 // [256,288) 256 + (entityNum % 32)
#define LENS_BAND_POWERUP  288 // [288,320) 288 + (entityNum % 32)
#define LENS_BAND_HALO	   320 // [320,352) 320 + (light index % 32)
#define LENS_BAND_ENTSPAN  32  // per-entity-kind slot count
#define LENS_MAX_CGSOURCES 352 // cgame strip size = sum of the bands

// Direction-independent halo descriptor: a point-light glow that looks the
// same from any angle (no view-axis fade — distinct from the directional lens
// flare). Coarse emit-and-forget like the other scene primitives; FLAT (no embedded
// pointers) so it crosses the VM boundary as one bounds-checked struct. `visible` is
// the occlusion gate the caller drives from the shared lens oracle (the GPU does the
// depth test; the renderer just draws or culls the halo sprite). reserved[] is a
// frozen tail for forward growth.
typedef struct {
	int	   id;			// stable per-halo key (pre-banded into LENS_BAND_HALO)
	vec3_t origin;		// world position of the halo
	float  r, g, b;		// color, 0..1
	float  scale;		// radius / intensity multiplier
	int	   visible;		// occlusion gate (qboolean), oracle-driven
	int	   reserved[2]; // frozen for forward growth
} haloDesc_t;

// ── unified atmosphere ────────────────────────────────────────────────

#define WIRED_ATMOSPHERE_SCHEMA_VERSION 1u

typedef enum {
	ATMOSPHERE_QUALITY_OFF = 0,
	ATMOSPHERE_QUALITY_ANALYTIC,
	ATMOSPHERE_QUALITY_WEATHER,
	ATMOSPHERE_QUALITY_FULL
} atmosphereQualityTier_t;

typedef enum {
	ATMOSPHERE_PRECIP_NONE = 0,
	ATMOSPHERE_PRECIP_RAIN,
	ATMOSPHERE_PRECIP_SNOW,
	ATMOSPHERE_PRECIP_SLEET,
	ATMOSPHERE_PRECIP_HAIL,
	ATMOSPHERE_PRECIP_DUST_ASH
} atmospherePrecipitation_t;

#define ATMOSPHERE_FLAG_ENABLED			 0x00000001u
#define ATMOSPHERE_FLAG_HEIGHTGRID		 0x00000002u
#define ATMOSPHERE_FLAG_INDOOR_EXPOSURE	 0x00000004u
#define ATMOSPHERE_FLAG_SURFACE_CLIMATE	 0x00000008u
#define ATMOSPHERE_FLAG_SKY_LIGHTING	 0x00000010u
#define ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA 0x00000020u

typedef enum {
	ATMOSPHERE_EMITTER_NONE = 0,
	ATMOSPHERE_EMITTER_BREATH,
	ATMOSPHERE_EMITTER_STEAM,
	ATMOSPHERE_EMITTER_GROUND_MIST,
	ATMOSPHERE_EMITTER_SPRAY,
	ATMOSPHERE_EMITTER_DEBRIS,
	ATMOSPHERE_EMITTER_PRECIPITATION_IMPACT
} atmosphereEmitterKind_t;

#define WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION 1u
#define ATMOSPHERE_EFFECT_MAX_STAGES				   8u
#define ATMOSPHERE_EFFECT_MAX_PROFILES				   64u

typedef enum {
	ATMOSPHERE_STAGE_EMISSION = 0,
	ATMOSPHERE_STAGE_CONTINUOUS,
	ATMOSPHERE_STAGE_COLLISION,
	ATMOSPHERE_STAGE_DEATH
} atmosphereEffectStageTrigger_t;

#define ATMOSPHERE_STAGE_INHERIT_POSITION  0x00000001u
#define ATMOSPHERE_STAGE_INHERIT_VELOCITY  0x00000002u
#define ATMOSPHERE_STAGE_INHERIT_COLOR	   0x00000004u
#define ATMOSPHERE_STAGE_INHERIT_INTENSITY 0x00000008u

// One bounded GPU stage. parentStage is UINT32_MAX for a root; otherwise it
// must name an earlier stage, which makes the graph acyclic by construction.
// A stage references an already registered particle class for curves/rendering.
typedef struct {
	uint32_t trigger;		 // atmosphereEffectStageTrigger_t
	uint32_t particleClass;	 // particleClassHandle_t, never zero
	uint32_t parentStage;	 // UINT32_MAX or an earlier stage index
	uint32_t flags;			 // ATMOSPHERE_STAGE_INHERIT_* bits
	uint32_t maxParticles;	 // hard live-work budget for this stage
	uint32_t burstCount;	 // bounded spawn at trigger time
	float	 spawnRate;		 // particles/second while active
	float	 delay;			 // seconds after parent trigger
	float	 duration;		 // seconds; must be positive
	float	 lodNear;		 // world units
	float	 lodFar;		 // world units, >= lodNear
	float	 boundsRadius;	 // conservative culling bound
	float	 intensityScale; // emitter intensity multiplier
	uint32_t reserved[3];
} atmosphereEffectStage_t;

// Immutable authored graph. Optional parentProfile inheritance is resolved at
// registration, not in the frame hot path: stageOverrideMask bit N selects the
// child stage N; clear bits inherit the already-registered parent's stage N.
typedef struct {
	uint32_t				schemaVersion;
	uint32_t				stageCount;
	uint32_t				maxParticles;	   // whole-profile hard budget
	uint32_t				flags;			   // reserved; zero in schema 1
	uint32_t				parentProfile;	   // 0 or an earlier registered profile handle
	uint32_t				stageOverrideMask; // meaningful only when parentProfile != 0
	uint32_t				seed;
	uint32_t				reserved0;
	float					duration; // 0 = emitter lifetime authority; otherwise cap
	float					lodNear;
	float					lodFar;
	float					boundsRadius;
	atmosphereEffectStage_t stages[ATMOSPHERE_EFFECT_MAX_STAGES];
	uint32_t				reserved[8];
} atmosphereEffectProfile_t;

// Pointer-free semantic emitter. The app owns actor/game decisions and submits
// only this bounded visual intent; the renderer never includes game headers or
// infers what an actor is doing. `profile` is a stable authored atmosphere FX
// profile handle, not a particle instance. Per-particle lifecycle stays on GPU.
typedef struct {
	uint32_t schemaVersion;
	uint32_t kind;	// atmosphereEmitterKind_t
	uint32_t id;	// stable app-owned key for deterministic replay
	uint32_t flags; // reserved; must be zero in schema 1
	uint32_t seed;
	uint32_t profile;
	vec3_t	 origin;
	float	 intensity; // [0,1]
	vec3_t	 direction;
	float	 radius; // world units, >= 0
	float	 temperatureC;
	float	 humidity; // [0,1]
	float	 lifetime; // seconds, 0 = one-frame semantic emitter
	float	 reserved[5];
} atmosphereEmitter_t;

#define WIRED_ATMOSPHERE_SURFACE_EVENT_SCHEMA_VERSION 1u

typedef enum {
	ATMOSPHERE_SURFACE_EVENT_FOOTPRINT = 1,
	ATMOSPHERE_SURFACE_EVENT_IMPACT,
	ATMOSPHERE_SURFACE_EVENT_TRAVERSAL
} atmosphereSurfaceEventKind_t;

// Pointer-free semantic surface interaction. The app identifies what happened;
// the renderer derives wetness/frost/snow response from AtmosphereFrameState.
// Events are frame-local and bounded; persistent tile state is renderer-owned.
typedef struct {
	uint32_t schemaVersion;
	uint32_t kind;	// atmosphereSurfaceEventKind_t
	uint32_t id;	// stable app-owned replay key, never zero
	uint32_t flags; // reserved; zero in schema 1
	vec3_t	 origin;
	float	 radius; // [0,256] world units; bounded to at most 25 tiles
	vec3_t	 direction;
	float	 strength; // [0,1]
	float	 timelineSeconds;
	float	 lifetime; // seconds, >= 0
	uint32_t reserved[2];
} atmosphereSurfaceEvent_t;

#define WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION 1u

typedef enum {
	ATMOSPHERE_MEDIA_VOLUME_SPHERE = 1,
	ATMOSPHERE_MEDIA_VOLUME_BOX
} atmosphereMediaVolumeShape_t;

#define ATMOSPHERE_MEDIA_VOLUME_CAST_SHADOW 0x00000001u
#define ATMOSPHERE_MEDIA_VOLUME_NOISE		0x00000002u

// Pointer-free local participating-media intent. The app authors a bounded
// volume; froxel injection, light integration, temporal reconstruction and
// the single atmosphere composite remain renderer-owned.
typedef struct {
	uint32_t schemaVersion;
	uint32_t shape;	   // atmosphereMediaVolumeShape_t
	uint32_t id;	   // stable replay key, never zero
	uint32_t flags;	   // ATMOSPHERE_MEDIA_VOLUME_* bits
	uint32_t priority; // deterministic overflow ordering
	uint32_t seed;
	vec3_t	 origin;
	float	 radius;			// sphere radius; box bounding radius
	vec3_t	 extent;			// box half-extent; zero for sphere
	float	 extinction;		// >= 0
	vec3_t	 albedo;			// linear RGB [0,1]
	float	 anisotropy;		// [-0.95,0.95]
	vec3_t	 emissive;			// linear RGB >= 0
	float	 emissionIntensity; // >= 0
	float	 heightFalloff;		// >= 0
	float	 noiseScale;		// > 0 only with NOISE
	float	 timelineStart;
	float	 timelineEnd; // 0 = unbounded, otherwise >= start
	uint32_t reserved[4];
} atmosphereMediaVolume_t;

// App-neutral immutable per-frame atmosphere state. The legacy rain/snow
// prefix keeps its exact field offsets; the versioned tail is the single
// climate/timeline authority shared by fog, weather particles, surface
// response and sky lighting. The app publishes one small semantic snapshot
// per frame (and emitter/profile changes), never particle instances. The
// renderer's dedicated GPU pools self-spawn / integrate / collide / fade /
// draw. `type` uses atmospherePrecipitation_t; the five-element mixture is
// authoritative and permits sleet/hail/dust-ash combinations. `bounds` is the
// world spawn volume
// (xyz min, xyz max); the GPU respawns particles inside it and distance-
// culls around the eye (passed per-frame in the compute UBO) to `distance`.
// The collision heightgrid is NOT carried here — a struct-nested pointer
// cannot cross the cgame VM boundary safely (only top-level syscall args
// are address-translated), so it ships via the separate
// trap_R_SetAtmosphereHeightgrid syscall (mirrors AddPolyToScene's
// verts-ptr + count). worldMins/Maxs/gridSize describe that grid's xy
// extent for the GPU's heightgrid sample.
typedef struct {
	int	   type;	  // atmospherePrecipitation_t; 0/1/2 preserve legacy order
	float  bounds[6]; // xyz min, xyz max — world spawn volume
	float  distance;  // eye-relative cull radius (ATM_DISTANCE)
	vec2_t worldMins; // tracemap xy bounds (heightgrid sample origin)
	vec2_t worldMaxs;
	int	   gridSize; // heightgrid edge (TRACEMAP_SIZE = 256)

	uint32_t schemaVersion;
	uint32_t flags;
	uint32_t qualityTier; // atmosphereQualityTier_t
	uint32_t seed;
	float	 timelineSeconds;
	float	 transitionSeconds;
	float	 temperatureC;
	float	 humidity; // [0,1]
	vec3_t	 wind;
	float	 gustStrength;
	// Continuous mixture weights. `type` is only the dominant legacy hint; all
	// modern backends consume the complete mixture.
	float	 precipitation[5]; // rain, snow, sleet, hail, dust/ash; each [0,1]
	float	 visibility;	   // world units; 0 = content/default authority
	float	 indoorExposure;   // [0,1], 0 fully sheltered, 1 fully exposed
	float	 surfaceWetness;   // [0,1]
	float	 surfaceFrost;	   // [0,1]
	float	 snowAccumulation; // [0,1]
	float	 meltRate;		   // [0,1]
	vec3_t	 sunDirection;
	float	 sunIntensity; // >= 0
	vec3_t	 moonDirection;
	float	 moonIntensity;		 // >= 0
	vec3_t	 ambientColor;		 // linear RGB, each >= 0
	float	 cloudCover;		 // [0,1]
	float	 cloudShadow;		 // [0,1]
	float	 lightning;			 // [0,1]
	float	 mediaDensity;		 // >= 0
	float	 mediaHeightFalloff; // >= 0
	uint32_t reserved[8];
} atmosphericDesc_t;

// Canonical name for new code. `atmosphericDesc_t` remains the ABI-compatible
// spelling used by the existing renderer export and VM syscall.
typedef atmosphericDesc_t atmosphereFrameState_t;

// ── primitive shader stage info ───────────────────────────────────────
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
// VkPrimitiveStageGPU in code/render/ral/backends/vulkan/renderer/vk.h).

#define PRIMITIVE_STAGE_MAX 4

// Sentinel for the engine-internal qhandle→primitive-slot lookup
// table. Used at uint8_t storage; 0xFF disambiguates "registered to
// slot 0" (slot 0 is reserved for whiteImage but is a legal allocation
// outcome) from "never registered as primitive shader".
#define PRIMITIVE_SLOT_INVALID 0xFF

typedef enum {
	PRIM_RGBGEN_IDENTITY		  = 0,
	PRIM_RGBGEN_IDENTITY_LIGHTING = 1,
	PRIM_RGBGEN_VERTEX			  = 2,
	// Wave forms not exposed yet — most primitive shaders use static
	// modes. Adding wave: extend GPU struct to carry wave parameters;
	// this turn sticks with static modes for scope.
} primRgbGen_t;

typedef enum {
	PRIM_ALPHAGEN_IDENTITY = 0,
	PRIM_ALPHAGEN_VERTEX   = 1,
} primAlphaGen_t;

typedef enum {
	PRIM_BLEND_ZERO				   = 0,
	PRIM_BLEND_ONE				   = 1,
	PRIM_BLEND_SRC_COLOR		   = 2,
	PRIM_BLEND_ONE_MINUS_SRC_COLOR = 3,
	PRIM_BLEND_SRC_ALPHA		   = 4,
	PRIM_BLEND_ONE_MINUS_SRC_ALPHA = 5,
	PRIM_BLEND_DST_COLOR		   = 6,
	PRIM_BLEND_ONE_MINUS_DST_COLOR = 7,
	PRIM_BLEND_DST_ALPHA		   = 8,
	PRIM_BLEND_ONE_MINUS_DST_ALPHA = 9,
	PRIM_BLEND_FACTOR_COUNT		   = 10,
} primBlendFactor_t;

typedef struct {
	int				  imageSlot; // index into vk_primitive_shader_images
	primBlendFactor_t srcBlend;
	primBlendFactor_t dstBlend;
	vec2_t			  uvScale;	// post-mul; default (1, 1); legacy tcMod scale
	vec2_t			  uvScroll; // units/second; default (0, 0); legacy tcMod scroll
	primRgbGen_t	  rgbGen;
	primAlphaGen_t	  alphaGen;
} primitiveShaderStage_t;

typedef struct {
	int					   stageCount; // 1..PRIMITIVE_STAGE_MAX
	primitiveShaderStage_t stages[PRIMITIVE_STAGE_MAX];
} primitiveShaderInfo_t;
