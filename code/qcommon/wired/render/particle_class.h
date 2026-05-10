/*
particle_class.h — wired-render particle class definitions

A particleClass_t is a static, offline-registered "recipe" describing
how a particular kind of particle behaves and looks. Classes encode
everything that does not vary per-shot: emit mode, scatter shape,
velocity shape, lifetime curve, color palette, size curve, physics.
Per-shot variation (where, how many, optional tint) lives in
emitterDesc_t.

The class struct is intentionally flat (no embedded pointers) so it
can be passed across the WASM-VM syscall boundary as a single struct
through one VMA() translation. Class names — descriptive labels used
for registry lookup and debugging — are NOT stored inside
particleClass_t; they live as separate metadata in the cgame-side
registry and never cross the trap boundary.

The renderer / particle system never names specific effects. Class
names (registry-side only) are descriptive of behaviour, not effects
(e.g. "axial_burst", "path_drift_grey"). Effect-specific composition
happens in cgame.
*/
#pragma once

#include "../../q_shared.h"
#include "primitives.h"

// 0 is reserved as the invalid handle. Valid handles are >= 1.
#define INVALID_PARTICLE_CLASS ((particleClassHandle_t)0)

typedef enum {
	SCATTER_NONE,           // spawn at origin
	SCATTER_CUBE,           // origin + crand()*magnitude per axis
	SCATTER_SPHERE,         // origin + random unit vec * magnitude
	SCATTER_PERP_DISC       // origin + random in disc perp. to axis
} scatterShape_t;

typedef enum {
	VEL_AXIAL,              // axis * speed
	VEL_AXIAL_PLUS_CUBE,    // axis*speed + crand()*jitter per axis
	VEL_CONE,               // axis ± half-angle * speed
	VEL_PURE_CUBE           // crand()*jitter per axis only
} velocityShape_t;

typedef enum {
	EMIT_POINT,             // emit at origin
	EMIT_PATH               // emit uniformly along origin→end
} emitMode_t;

#define PARTICLE_CLASS_MAX_PALETTE 16

typedef struct {
	qhandle_t       shader;
	int             renderFlags;     // PRIM_FLAG_* (additive, etc.)

	emitMode_t      emitMode;
	scatterShape_t  scatterShape;
	float           scatterMagnitude;

	velocityShape_t velocityShape;
	float           axialSpeed;
	float           cubeJitter;
	float           coneHalfAngle;   // radians

	// Per-particle expressivity extension. All four fields default to
	// zero-effect when unset (memset-zero) so existing classes that do
	// not opt in keep their current behavior bit-identical.
	//
	// velocityBias / velocityBiasJitter:
	//   Constant + symmetric per-axis jitter applied to vel AFTER the
	//   velocityShape produces its output. Lets a class add an
	//   axis-specific kick (e.g. +Z bias) without distorting the
	//   underlying shape's geometry. Asymmetric ranges are expressed
	//   as midpoint+halfwidth, e.g. CPU's "vel.z += [0, 100]" maps to
	//   bias=(0,0,50), biasJitter=(0,0,50).
	// speedJitter:
	//   Per-particle axialSpeed scatter, picked once at emit time as
	//   crandom() * speedJitter and added to axialSpeed before the
	//   velocityShape consumes it. VEL_AXIAL with axialSpeed=200 and
	//   speedJitter=100 produces uniform speed in [100, 300]. Has no
	//   effect on VEL_PURE_CUBE (no axial component).
	// sizeJitter:
	//   Per-particle sizeStart scatter, picked once at emit time as
	//   crandom() * sizeJitter and stored on the particle. The size
	//   curve becomes mix(sizeStart + jitterPick, sizeEnd, age).
	float           velocityBias[3];
	float           velocityBiasJitter[3];
	float           speedJitter;
	float           sizeJitter;

	float           lifetimeMean;
	float           lifetimeJitter;

	vec4_t          colorPalette[PARTICLE_CLASS_MAX_PALETTE];
	int             paletteCount;    // 1 = solid; sampled per particle
	vec4_t          colorEndMult;    // multiplied onto palette entry
	                                 //   at death; lerped over life

	float           sizeStart;
	float           sizeEnd;

	float           gravityScale;    // multiplier on global gravity
	float           drag;            // velocity damping per second
} particleClass_t;

#define MAX_PARTICLE_CLASSES 64

// Register a class. The `name` is registry-only metadata; it does
// NOT live inside particleClass_t. Returns a handle (>0) on success,
// or 0 on failure (table full, duplicate name, NULL inputs, empty
// name).
particleClassHandle_t CG_RegisterParticleClass( const char *name, const particleClass_t *cls );

// Look up a class handle by name. Returns 0 if not found.
particleClassHandle_t CG_FindParticleClass( const char *name );

// Look up a class definition by handle. Returns NULL if the handle
// is out of range or unregistered. Used by emit-time code paths.
const particleClass_t *CG_GetParticleClass( particleClassHandle_t handle );
