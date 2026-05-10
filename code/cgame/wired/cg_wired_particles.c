/*
===========================================================================
cg_wired_particles.c — wired-render particle class registry (cgame side)

Cgame-side store of particleClass_t recipes. Each registered class gets
a dense slot index plus a 1-based handle. The class definitions are
mirrored to the renderer via trap_R_RegisterParticleClass at registration
time so the renderer's compute shader can consume them later.

The legacy CPU particle system in cg_particles.c (Rafael smoke/blood
trails) is unrelated to this file and untouched by phase 1.

The registry is intentionally small (MAX_PARTICLE_CLASSES = 64) and
uses a linear scan for name lookup — registration is rare (init time)
and lookup is expected to happen once per consumer at startup.

Names are registry-only metadata. They never enter particleClass_t,
they never cross the trap boundary, and the renderer never sees them.
===========================================================================
*/

#include "cg_local.h"
#include "../../qcommon/wired/render/particle_class.h"
#include "../../qcommon/wired/render/traps.h"

// Slot 0 is reserved as INVALID_PARTICLE_CLASS, so slot 0 of these
// arrays is left unused; valid slots are [1..cg_numParticleClasses].
// Equivalently: slot index = handle - 1 once we offset by 1.
#define CG_PARTICLE_NAME_MAX 64

static particleClass_t cg_particleClasses[MAX_PARTICLE_CLASSES];
static char            cg_particleClassNames[MAX_PARTICLE_CLASSES][CG_PARTICLE_NAME_MAX];
static int             cg_numParticleClasses;

particleClassHandle_t CG_RegisterParticleClass( const char *name, const particleClass_t *cls ) {
	int slot;

	if ( name == NULL || cls == NULL )
		return INVALID_PARTICLE_CLASS;
	if ( name[0] == '\0' )
		return INVALID_PARTICLE_CLASS;
	if ( cg_numParticleClasses >= MAX_PARTICLE_CLASSES )
		return INVALID_PARTICLE_CLASS;

	// Duplicate-name guard. Returns 0 on duplicate per the doc-comment
	// in particle_class.h ("Returns ... 0 on failure (... duplicate
	// name ...)") — explicitly NOT returning the existing handle.
	if ( CG_FindParticleClass( name ) != INVALID_PARTICLE_CLASS )
		return INVALID_PARTICLE_CLASS;

	slot = cg_numParticleClasses;
	memcpy( &cg_particleClasses[slot], cls, sizeof( *cls ) );
	Q_strncpyz( cg_particleClassNames[slot], name, CG_PARTICLE_NAME_MAX );
	cg_numParticleClasses++;

	{
		// Handles are 1-based so 0 stays as INVALID_PARTICLE_CLASS.
		particleClassHandle_t handle = (particleClassHandle_t)( slot + 1 );

		// Mirror to renderer's shadow registry. The renderer keeps a
		// host-side copy for its compute shader; this trap is the
		// only path by which it learns about a class.
		trap_R_RegisterParticleClass( handle, &cg_particleClasses[slot] );

		return handle;
	}
}

particleClassHandle_t CG_FindParticleClass( const char *name ) {
	int i;

	if ( name == NULL || name[0] == '\0' )
		return INVALID_PARTICLE_CLASS;

	for ( i = 0; i < cg_numParticleClasses; i++ ) {
		if ( !strcmp( cg_particleClassNames[i], name ) )
			return (particleClassHandle_t)( i + 1 );
	}
	return INVALID_PARTICLE_CLASS;
}

const particleClass_t *CG_GetParticleClass( particleClassHandle_t handle ) {
	if ( handle <= 0 || handle > cg_numParticleClasses )
		return NULL;
	return &cg_particleClasses[handle - 1];
}

/*
==========================
CG_RegisterRailParticleClasses

Register the two particle classes used by the rail trail's debris and
impact sparks. Called once from CG_RegisterWeapon's WP_RAILGUN case
after the rail shaders bind, so cgs.media.railRingsShader and
cgs.media.whiteShader are valid by the time we read them.

Class parameters mirror the legacy CPU loops in CG_RailTrail (spawn
time) and CG_AddRailTrails (per-frame). Each value below is annotated
against the CPU source it derives from.
==========================
*/
void CG_RegisterRailParticleClasses( void ) {
	particleClass_t cls;

	// ── rail_debris ────────────────────────────────────────────────
	// Q2-spirit grey debris emitted along the beam path. Mirrors the
	// CPU debris loop (cg_weapons.c CG_RailTrail, ~line 322-358 +
	// CG_AddRailTrails ~line 506-533):
	//   emit along trail->start..trail->end every 7.5 units → EMIT_PATH
	//   uniform sampling on the path (CPU determinism vs GPU random
	//   uniform are visually equivalent at this density)
	//   spawn scatter: crand()*3 per axis → SCATTER_CUBE, mag 3.0
	//   velocity: crand()*3 per axis (NO axial component, despite
	//     the surrounding "drift" terminology) → VEL_PURE_CUBE,
	//     jitter 3.0
	//   lifetime: alpha = 1 - frac over RAIL_TRAILTIME = 1500 ms
	//     → lifetimeMean = 1.5s, no jitter
	//   shader: cgs.media.railRingsShader (stock Q3 "railDisc",
	//     additive)
	//   color: per-particle grey 255 - (rand()&15)*8 ∈ [135..255].
	//     The class system supports a 16-entry palette, but the
	//     phase 3 vertex shader uses palette index 0 only as a
	//     deliberate simplification. Mid-grey 0.78 is the centre of
	//     the CPU range; per-particle variance is lost in the port.
	//   colorEndMult: alpha → 0 over lifetime (CPU fades alpha
	//     directly via the trail's frac).
	//   sizeStart/sizeEnd = 0.5 (CPU CG_BuildBillboardQuad radius;
	//     CPU does not shrink debris).
	//   gravityScale = 0.0: CPU code does NOT apply gravity to
	//     debris despite the misleading "with gravity drift"
	//     comment — render-time integration is purely
	//     pos = debrisOrg + elapsed * debrisDelta. This is verified
	//     parity, not a regression.
	//   drag = 0.0: CPU velocities are constant.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.railRingsShader;
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_PATH;
	cls.scatterShape       = SCATTER_CUBE;
	cls.scatterMagnitude   = 3.0f;
	cls.velocityShape      = VEL_PURE_CUBE;
	cls.axialSpeed         = 0.0f;
	cls.cubeJitter         = 3.0f;
	cls.coneHalfAngle      = 0.0f;
	cls.lifetimeMean       = 1.5f;
	cls.lifetimeJitter     = 0.0f;
	// Phase 6: 16-step grey ramp matching CPU's per-particle
	// randomness. CPU original: 255 - (rand() & 15) * 8 →
	// [255, 247, 239, ..., 143, 135] (16 distinct values).
	// Normalize to float [0.529, 1.0]; each step = 8/255 ≈ 0.0314.
	// RE_EmitParticles assigns each particle a random index in
	// [0, paletteCount), particle.vert samples
	// colorPalette[paletteIndex] per particle.
	cls.paletteCount = 16;
	for ( int i = 0; i < 16; i++ ) {
		float grey = ( 255.0f - i * 8.0f ) / 255.0f;
		cls.colorPalette[i][0] = grey;
		cls.colorPalette[i][1] = grey;
		cls.colorPalette[i][2] = grey;
		cls.colorPalette[i][3] = 1.0f;
	}
	cls.colorEndMult[0]    = 1.0f;
	cls.colorEndMult[1]    = 1.0f;
	cls.colorEndMult[2]    = 1.0f;
	cls.colorEndMult[3]    = 0.0f;
	cls.sizeStart          = 0.5f;
	cls.sizeEnd            = 0.5f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 0.0f;

	cgs.media.railDebrisClass =
		(qhandle_t)CG_RegisterParticleClass( "rail_debris", &cls );

	// ── rail_sparks ────────────────────────────────────────────────
	// Warm-white embers from the impact point. Mirrors the CPU sparks
	// loop (CG_RailTrail ~line 360-374 + CG_AddRailTrails ~line 535-563):
	//   emit AT trail->end (point, no path) → EMIT_POINT, scatter NONE
	//   velocity: impactNormal*80 + crand()*40 per axis
	//     → VEL_AXIAL_PLUS_CUBE, axialSpeed 80, cubeJitter 40
	//     (axis is supplied at emit time as desc->axis = impactNormal)
	//   lifetime: visible only while frac < 0.2 → 0.2 * RAIL_TRAILTIME
	//     = 0.3s, no jitter
	//   shader: cgs.media.whiteShader. R_FindShader's LIGHTMAP_2D
	//     auto-default gives blendFunc SRC_ALPHA / ONE_MINUS_SRC_ALPHA
	//     (verified via tr_shader.c R_CreateDefaultShading), so
	//     renderFlags = 0 (alpha blend, NOT additive).
	//   color: CPU (255, 255, 220) warm white → (1, 1, 220/255, 1).
	//   colorEndMult: alpha → 0 over lifetime.
	//   sizeStart/sizeEnd = 0.3 (CPU CG_BuildBillboardQuad radius).
	//   gravityScale = 0.5: CPU applies pos[2] -= 0.5 * 400 * t²
	//     (g = 400 units/s²); compute shader's WORLD_GRAVITY = 800
	//     (q3 cg_gravity default), so 400/800 = 0.5.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.whiteShader;
	cls.renderFlags        = 0;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.scatterMagnitude   = 0.0f;
	cls.velocityShape      = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed         = 80.0f;
	cls.cubeJitter         = 40.0f;
	cls.coneHalfAngle      = 0.0f;
	cls.lifetimeMean       = 0.3f;
	cls.lifetimeJitter     = 0.0f;
	cls.paletteCount       = 1;
	cls.colorPalette[0][0] = 1.0f;
	cls.colorPalette[0][1] = 1.0f;
	cls.colorPalette[0][2] = 220.0f / 255.0f;
	cls.colorPalette[0][3] = 1.0f;
	cls.colorEndMult[0]    = 1.0f;
	cls.colorEndMult[1]    = 1.0f;
	cls.colorEndMult[2]    = 1.0f;
	cls.colorEndMult[3]    = 0.0f;
	cls.sizeStart          = 0.3f;
	cls.sizeEnd            = 0.3f;
	cls.gravityScale       = 0.5f;
	cls.drag               = 0.0f;

	cgs.media.railSparksClass =
		(qhandle_t)CG_RegisterParticleClass( "rail_sparks", &cls );
}

/*
==========================
CG_RegisterLightningParticleClasses

Register the particle class used by Lightning Gun primary impact
sparks. Called once from CG_RegisterWeapon's WP_LIGHTNING_GUN case;
cgs.media.lightningSparkShader is bound earlier in CG_RegisterGraphics
(cg_main.c), so it is valid by the time this runs.

Class parameters mirror the legacy CPU body of CG_LightningSparks
(cg_effects.c:881-927). Each value below is annotated against the
CPU source it derives from. The expressivity-extension fields
(speedJitter, velocityBias, velocityBiasJitter, sizeJitter) carry
the parts of CPU behaviour that the four base velocity shapes
cannot express:
   speedJitter            ← random speed magnitude (CPU's
                            "(100 + random()*200)" scaling).
   velocityBias[2]        ← asymmetric upward kick midpoint
                            (CPU's "vel.z += random()*100").
   velocityBiasJitter[2]  ← asymmetric upward kick halfwidth.
   sizeJitter             ← per-spark radius scatter (CPU's
                            "1.5 + random()*1.5").
==========================
*/
void CG_RegisterLightningParticleClasses( void ) {
	particleClass_t cls;

	// ── lg_sparks ────────────────────────────────────────────────
	// Blue spark shower at LG primary impact. Mirrors CPU body
	// CG_LightningSparks at cg_effects.c:881-927. Per-frame caller
	// emits 3 particles (CPU loop count = 3, line 890); the GPU
	// path mirrors this via emitter.count = 3 at the call site.
	memset( &cls, 0, sizeof( cls ) );

	cls.shader               = cgs.media.lightningSparkShader;
	// Informational per phase 5; renderer derives blend from the
	// shader's stages[0]→stateBits at registration time.
	cls.renderFlags          = PRIM_FLAG_ADDITIVE;

	// Emit at impact origin, no spawn-position scatter.
	cls.emitMode             = EMIT_POINT;
	cls.scatterShape         = SCATTER_NONE;
	cls.scatterMagnitude     = 0.0f;

	// Velocity. CPU formula (cg_effects.c:920-925):
	//   v = surfaceNormal + crand()*0.7  per axis  (pre-normalize)
	//   v = normalize(v)
	//   v *= 100 + random()*200                    (uniform speed [100, 300])
	//   v.z += random()*100                        (asymmetric +Z kick [0, 100])
	//
	// Mapped to expressivity-extended class:
	//   - VEL_CONE with axialSpeed=200 + speedJitter=100 produces
	//     uniform speed in [100, 300] (axialSpeed + crand()*100).
	//   - coneHalfAngle ≈ atan(0.7) ≈ 0.611 rad models the
	//     direction fan post-normalize; CPU's pre-normalize cube
	//     perturbation produces a slightly non-uniform distribution
	//     within this cone (clustering toward axes), but the
	//     visual is close.
	//   - velocityBias[2] = 50, velocityBiasJitter[2] = 50 yields
	//     vel.z += 50 + crand()*50 = uniform [0, 100], matching
	//     CPU's "vel.z += random()*100".
	cls.velocityShape        = VEL_CONE;
	cls.axialSpeed           = 200.0f;
	cls.speedJitter          = 100.0f;
	cls.coneHalfAngle        = 0.611f;
	cls.cubeJitter           = 0.0f;
	cls.velocityBias[0]      = 0.0f;
	cls.velocityBias[1]      = 0.0f;
	cls.velocityBias[2]      = 50.0f;
	cls.velocityBiasJitter[0] = 0.0f;
	cls.velocityBiasJitter[1] = 0.0f;
	cls.velocityBiasJitter[2] = 50.0f;

	// Lifetime. CPU: cg.time + 200 + (rand() & 0xff) ms = uniform
	// [200, 455] ms. Renderer's mean ± jitter produces uniform
	// [mean - jitter, mean + jitter]. Match: mean = 327.5 ms,
	// jitter = 127.5 ms (in seconds).
	cls.lifetimeMean         = 0.3275f;
	cls.lifetimeJitter       = 0.1275f;

	// Color. CPU shaderRGBA = (0x55, 0x99, 0xff, 0xff)
	//                      = (85/255, 153/255, 255/255, 1.0).
	// 0x99/255 written as 153.0f/255.0f to keep the integer source
	// visible at code-review time.
	cls.paletteCount         = 1;
	cls.colorPalette[0][0]   = 85.0f  / 255.0f;
	cls.colorPalette[0][1]   = 153.0f / 255.0f;
	cls.colorPalette[0][2]   = 1.0f;
	cls.colorPalette[0][3]   = 1.0f;

	// Alpha → 0 over lifetime. CPU LE_MOVE_SCALE_FADE fades both
	// the size and the alpha to 0; size handling is below.
	cls.colorEndMult[0]      = 1.0f;
	cls.colorEndMult[1]      = 1.0f;
	cls.colorEndMult[2]      = 1.0f;
	cls.colorEndMult[3]      = 0.0f;

	// Size. CPU: re->radius = 1.5 + random()*1.5 = uniform
	// [1.5, 3.0]. Symmetric crand() jitter expresses this as
	// midpoint 2.25 ± halfwidth 0.75. sizeEnd = 0 reproduces
	// LE_MOVE_SCALE_FADE's shrink-to-zero behavior.
	cls.sizeStart            = 2.25f;
	cls.sizeJitter           = 0.75f;
	cls.sizeEnd              = 0.0f;

	// Gravity. CPU TR_GRAVITY uses q3 default 800 u/s²; compute
	// shader's WORLD_GRAVITY also = 800, so gravityScale = 1.0
	// reproduces CPU fall behavior. drag = 0 because CPU has no
	// velocity damping.
	cls.gravityScale         = 1.0f;
	cls.drag                 = 0.0f;

	cgs.media.lgSparksClass =
		(qhandle_t)CG_RegisterParticleClass( "lg_sparks", &cls );
}
