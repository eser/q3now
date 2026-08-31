// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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
	VEL_PURE_CUBE,          // crand()*jitter per axis only
	VEL_RADIAL_FROM_SCATTER // normalized spawn scatter * speed; falls back to
	                        // a deterministic random unit vector when the
	                        // particle has no scatter. Used by expanding shells.
} velocityShape_t;

typedef enum {
	EMIT_POINT,             // emit at origin
	EMIT_PATH               // emit uniformly along origin→end
} emitMode_t;

#define PARTICLE_CLASS_MAX_PALETTE 16

/*
--------------------------------------------------------------------------
Curve-valued parameters

A particle parameter used to be a plain float, or at best a Start/End pair
interpolated linearly by age. That is enough for "fades out" and nothing
else: "fast then abruptly slow", "grow then shrink", "flicker" have no
representation at all, because linear interpolation between two endpoints
is the only shape the class can express.

particleParm_t is the fix, and it is deliberately the shape idTech 5 uses
(idParticleParm in models/particles/jobs/particleparm.h): a small value
that says HOW a number is derived over the particle's normalised lifetime,
rather than being the number. Everything reduces to one call:

    value = ParticleParm_Eval( parm, fraction, jitterPick )

where `fraction` is age/lifetime in [0,1]. That signature is the whole
reason this fits a GPU-resident engine: it is a pure function of a value
the shader already has (p.age), so evaluation stays on the GPU and
emit-and-forget is untouched. This is an expressivity change, not an
architecture change.

Curves live in a SHARED table rather than inside the class — again
following idTech 5, where idDeclParticle owns `tables` and every stage
indexes into them. A curve is a resource, not a class's private property:
one authored falloff can back a dozen effects, and a class that wants a
constant never touches the table at all (PARM_CONSTANT short-circuits, so
a class that does not opt in costs exactly what it costs today).
--------------------------------------------------------------------------
*/

typedef enum {
	PARM_CONSTANT = 0,      // val0. Ignores fraction entirely — the default,
	                        //   and the zero value, so a memset-zero class
	                        //   keeps its current behaviour bit-identical.
	PARM_LINEAR,            // mix( val0, val1, fraction ) — what sizeStart/
	                        //   sizeEnd and colorEndMult already do, now
	                        //   expressible per parameter.
	PARM_CURVE,             // table[curve] sampled at fraction, then scaled
	                        //   into [val0, val1]. The shape lives in the
	                        //   table; val0/val1 place it in range.
	PARM_CURVE_TIMES_LINEAR // table[curve] * mix( val0, val1, fraction ).
	                        //   Lets a shape (flicker, pulse) ride on top of
	                        //   an independent trend (fade) without authoring
	                        //   the product as a third curve.
} particleParmCalc_t;

// Samples per curve. 8 is a deliberate floor, not a guess: it is two
// vec4s in std430, so a curve is exactly one aligned fetch pair on the
// GPU, and it resolves the shapes particles actually need (ease, pulse,
// two-stage falloff). Raising it costs table SSBO size linearly and must
// be changed in lockstep with the GLSL mirror.
#define PARTICLE_CURVE_SAMPLES 8

// Curves available to all classes. Indexed by particleParm_t::curve;
// index 0 is reserved as "no curve" so a zeroed parm is well-defined.
#define PARTICLE_MAX_CURVES 64

typedef struct {
	int     calc;           // particleParmCalc_t
	int     hasCurve;       // 0 = samples[] unused; 1 = samples[] is authored
	float   val0;           // constant value, or range start
	float   val1;           // range end (unused by PARM_CONSTANT)
	float   variance;       // symmetric per-particle scatter, picked once at
	                        //   emit and carried on the particle, so a parm
	                        //   can vary BETWEEN particles as well as over
	                        //   one particle's life
	float   parmPad0;       // keeps the header 32 B / 2 vec4 in std430 so the
	float   parmPad1;       //   GPU mirror needs no per-field alignment rules
	float   parmPad2;
	// The curve, RESOLVED. Curves are shared by NAME on the authoring side
	// (CG_RegisterParticleCurve dedups, so editing one reaches every class
	// built on it), but what crosses the VM boundary and reaches the GPU is
	// a flattened copy.
	//
	// That is deliberate. Passing an index instead would mean a second SSBO
	// binding and one more indirection per evaluation — on the per-particle
	// hot path — to save two kilobytes. Resolving once at registration costs
	// 32 bytes per parm and nothing per particle.
	//
	// Use CG_ResolveParticleParmCurve() to fill this from a registered curve
	// rather than writing samples by hand; that keeps the "shared by name"
	// property intact.
	float   samples[PARTICLE_CURVE_SAMPLES];
} particleParm_t;

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
	//   effect on VEL_PURE_CUBE (no axial component). VEL_RADIAL_FROM_SCATTER
	//   consumes axialSpeed as its outward speed, including speedJitter.
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

	// Sprite-frame (flipbook) animation. Appended at the end so prior
	// offsets stay byte-identical; classes that do not opt in keep
	// frameCount 0 (memset-zero) and render via `shader` exactly as
	// before. When frameCount > 1 the ring advances through
	// frameShaders[0..frameCount-1] by particle age:
	//   frame = clamp( floor(age * frameCount), 0, frameCount-1 ).
	// frameBlend 1 interpolates the two adjacent frames (silky), 0 steps.
	// Frames are separate per-frame shader handles (no atlas); the
	// renderer resolves them into a dedicated frame-texture pool.
	qhandle_t       frameShaders[16];  // PARTICLE_CLASS_MAX_FRAMES
	int             frameCount;        // 0/1 = static (use `shader`); >1 = animate
	int             frameBlend;        // 0 = stepped; 1 = interpolate adjacent frames

	// Curve-valued parameters. Appended at the end, like every extension
	// before them, so all prior offsets stay byte-identical.
	//
	// Each of these OVERRIDES its scalar counterpart when its calc is not
	// PARM_CONSTANT with val0 == 0. That rule is what makes the extension
	// free for existing classes: a memset-zero parm means "not authored",
	// the old scalar field is used, and the shader takes the same path it
	// takes today. A class opts in one parameter at a time.
	//
	//   sizeParm    overrides the sizeStart→sizeEnd lerp
	//   alphaParm   scales the palette colour's alpha over life
	//   dragParm    replaces the constant `drag`
	//   gravityParm replaces the constant `gravityScale`
	//
	// Four, not more, and chosen rather than guessed: these are the
	// parameters whose constant-ness actually blocks authoring today.
	// Adding a fifth is a struct-stride change on both sides of the
	// boundary, so it should follow evidence that an effect needs it.
	particleParm_t  sizeParm;
	particleParm_t  alphaParm;
	particleParm_t  dragParm;
	particleParm_t  gravityParm;
} particleClass_t;

// Per-class frame-count cap (rlboom = 8, glboom = 5). Sized to MATCH the
// GLSL frameSlots[] array — do not raise one without the other.
#define PARTICLE_CLASS_MAX_FRAMES 16

#define MAX_PARTICLE_CLASSES 64

// Dedicated frame-texture pool appended to the particle sampler array.
// The fragment sampler array holds MAX_PARTICLE_CLASSES per-class slots
// [0..63] PLUS FRAME_POOL_SIZE flipbook-frame slots [64..95]. Class slots
// keep their exact meaning (slot = classHandle-1); the frame pool is a
// separate allocator filled at registration when frameCount > 1.
#define FRAME_POOL_SIZE 32
#define PARTICLE_SAMPLER_COUNT ( MAX_PARTICLE_CLASSES + FRAME_POOL_SIZE )

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

// ── shared curve table ──────────────────────────────────────────────────
//
// Register a curve and get the index a particleParm_t::curve field should
// carry. `samples` is PARTICLE_CURVE_SAMPLES values covering fraction 0..1
// at even spacing; the GPU interpolates between them, so a curve is a
// shape, not a step function.
//
// Registration is BY NAME and deduplicating: registering the same name
// twice returns the same index without storing a second copy. That is what
// makes a curve a shared resource — "smoke-falloff" authored once is the
// same index in every class that asks for it, and a later change reaches
// all of them.
//
// Returns 0 on failure (table full, NULL inputs, empty name). Index 0 is
// reserved as "no curve", so 0 is never a valid registered curve and a
// zeroed particleParm_t is unambiguously "constant, no table".
int CG_RegisterParticleCurve( const char *name, const float *samples );

// Look up a registered curve index by name. Returns 0 if not found.
int CG_FindParticleCurve( const char *name );

// Read back a registered curve. Returns NULL for index 0 or an
// unregistered index. Used by the renderer upload path and by tests.
const float *CG_GetParticleCurve( int index );

// Resolve a registered curve into a parm, by name. This is how a class
// opts into a shared curve: the name is looked up once, the samples are
// copied in, and the parm becomes self-contained from then on.
//
// Returns qfalse (and leaves the parm's curve fields untouched) when the
// name is unknown, so a class referencing a curve that failed to register
// keeps its constant behaviour instead of rendering as nothing.
qboolean CG_ResolveParticleParmCurve( particleParm_t *parm, const char *curveName );

// Evaluate a parameter at a normalised lifetime fraction.
//
// `jitterPick` is the particle's own crandom() draw in [-1,1], carried
// from emit time; it scales `variance` so the same parm yields a
// different value per particle while staying deterministic for that
// particle. Pass 0 for the un-jittered value.
//
// This is the CPU mirror of the GLSL evaluator. Both must agree: the
// contract test pins them against each other, because a divergence would
// mean authored values preview differently from what ships.
float ParticleParm_Eval( const particleParm_t *parm, float fraction, float jitterPick );

// True when a parm was never authored (memset-zero) and the class's
// scalar counterpart should be used instead. Kept as a named predicate
// rather than an inline test so the override rule lives in ONE place —
// host, renderer and tests all ask the same question.
qboolean ParticleParm_IsUnset( const particleParm_t *parm );
