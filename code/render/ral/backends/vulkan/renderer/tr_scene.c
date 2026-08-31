// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "tr_local.h"
#include "../../../../frontend/r_log.h"  // rilog-channel-mechanism Turn B — renderer.cmd
#include "../../../../frontend/render_submission_effects.h"

R_LOG_DECLARE_CHANNEL( rch_cmd, "renderer.cmd" );

static int			r_firstSceneDrawSurf;
#ifdef USE_PMLIGHT
static int			r_firstSceneLitSurf;
#endif

int			r_numdlights;
static int			r_firstSceneDlight;

static int			r_numentities;
static int			r_firstSceneEntity;

static int			r_numpolys;
static int			r_firstScenePoly;

static int			r_numpolyverts;

#ifdef USE_VULKAN
static char s_ralEffectsSmokeMap[MAX_QPATH];

static void R_InjectRalEffectsSmoke( const refdef_t *fd ) {
	ribbonPoint_t points[3];
	ribbonDesc_t ribbon;
	railRibbonDesc_t rail;
	beamDesc_t beam;
	spriteDesc_t sprite;
	qhandle_t shader;
	vec3_t center;
	int i;

	if ( !r_ralEffectsSmoke || !r_ralEffectsSmoke->integer || !tr.world
	  || ( fd->rdflags & RDF_NOWORLDMODEL ) ) return;
	if ( !Q_stricmp( s_ralEffectsSmokeMap, tr.world->baseName ) ) return;
	if ( !vk.ribbon.available || !vk.railRibbon.available
	  || !vk.beam.available || !vk.sprite.available
	  || !vk.atm.available ) return;

	shader = RE_RegisterPrimitiveShader( "gfx/misc/particle" );
	if ( shader == 0 ) return;
	VectorMA( fd->vieworg, 72.0f, fd->viewaxis[0], center );

	memset( points, 0, sizeof( points ) );
	for ( i = 0; i < 3; i++ ) {
		VectorMA( center, (float)( i - 1 ) * 18.0f, fd->viewaxis[1], points[i].pos );
		points[i].pos[2] += ( i == 1 ) ? 12.0f : 0.0f;
		points[i].width = 2.5f;
		points[i].rgba[0] = 0.2f; points[i].rgba[1] = 0.8f;
		points[i].rgba[2] = 1.0f; points[i].rgba[3] = 1.0f;
	}
	memset( &ribbon, 0, sizeof( ribbon ) );
	ribbon.points = points; ribbon.numPoints = 3; ribbon.shader = shader;
	ribbon.flags = PRIM_FLAG_CAMERA_FACING;
	RE_AddRibbonToScene( &ribbon );

	memset( &beam, 0, sizeof( beam ) );
	VectorMA( center, -20.0f, fd->viewaxis[2], beam.start );
	VectorMA( center,  20.0f, fd->viewaxis[2], beam.end );
	beam.startWidth = 2.0f; beam.endWidth = 1.0f;
	beam.startColor[0] = beam.endColor[0] = 1.0f;
	beam.startColor[1] = beam.endColor[1] = 0.5f;
	beam.startColor[2] = beam.endColor[2] = 0.1f;
	beam.startColor[3] = beam.endColor[3] = 1.0f;
	beam.shader = shader; beam.axialCopies = 1;
	beam.startEntityNum = beam.endEntityNum = -1;
	RE_AddBeamToScene( &beam );

	memset( &rail, 0, sizeof( rail ) );
	VectorMA( center, -14.0f, fd->viewaxis[1], rail.start );
	VectorCopy( fd->viewaxis[0], rail.beamAxis );
	rail.beamLen = 64.0f; rail.duration = 1.0f; rail.shader = shader;
	rail.color[0] = 0.5f; rail.color[1] = 0.2f;
	rail.color[2] = 1.0f; rail.color[3] = 1.0f;
	for ( i = 0; i < RAIL_RIBBON_RING_COUNT; i++ ) {
		float a = (float)i * ( 2.0f * (float)M_PI / RAIL_RIBBON_RING_COUNT );
		VectorScale( fd->viewaxis[1], cosf( a ), rail.perpAxis[i] );
		VectorMA( rail.perpAxis[i], sinf( a ), fd->viewaxis[2], rail.perpAxis[i] );
	}
	RE_AddRailRibbonToScene( &rail );

	memset( &sprite, 0, sizeof( sprite ) );
	VectorMA( center, 18.0f, fd->viewaxis[2], sprite.origin );
	sprite.radius = 7.0f; sprite.shader = shader;
	sprite.rgba[0] = 1.0f; sprite.rgba[1] = 0.25f;
	sprite.rgba[2] = 0.1f; sprite.rgba[3] = 1.0f;
	RE_AddSpriteToScene( &sprite );

	Q_strncpyz( s_ralEffectsSmokeMap, tr.world->baseName,
		sizeof( s_ralEffectsSmokeMap ) );
}
#endif

// Lens-source occlusion stash (lens-glow B2). The game adds sources each frame;
// they are projected + written to the oracle registry at render time (see
// RB_AddLensSourceFlares), NOT here (backEnd.viewParms is invalid at scene build).
int					r_numLensSources;
static int			r_firstSceneLensSource;


/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame( void ) {

	backEndData->commands.used = 0;
	R_TemporalCommandBatchReset();

	r_firstSceneDrawSurf = 0;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = 0;
#endif

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;

	r_numLensSources = 0;
	r_firstSceneLensSource = 0;

#if FEAT_HALO
	R_ClearHalos();
#endif
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
	r_firstSceneLensSource = r_numLensSources;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	const srfPoly_t	*poly;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( ( void * )poly, sh, poly->fogIndex, 0 );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t	*poly;
	int			i, j;
	int			fogIndex;
	const fog_t		*fog;
	vec3_t		bounds[2];

	if ( !tr.registered ) {
		return;
	}
	// numVerts/numPolys arrive across the VM syscall boundary; a negative or
	// zero count must be rejected before the capacity guard below. The guard
	// `r_numpolyverts + numVerts > max_polyverts` is signed arithmetic, so a
	// negative numVerts makes the sum SMALLER and slips through — then
	// `numVerts * sizeof(*verts)` converts to a multi-gigabyte size_t and the
	// memcpy at line 150 writes out of bounds.
	if ( numVerts <= 0 || numPolys <= 0 ) {
		return;
	}
#if 0
	if ( !hShader ) {
		R_LOG( rch_cmd, SEV_WARN, "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}
#endif
	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts > max_polyverts || r_numpolys >= max_polys ) {
      /*
      NOTE TTimo this was initially a PRINT_WARNING
      but it happens a lot with high fighting scenes and particles
      since we don't plan on changing the const and making for room for those effects
      simply cut this message to developer only
      */
			R_LOG( rch_cmd, SEV_DEBUG, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];

		memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );
#if 0
		if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
			poly->verts->modulate[0] = 255;
			poly->verts->modulate[1] = 255;
			poly->verts->modulate[2] = 255;
			poly->verts->modulate[3] = 255;
		}
#endif
		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		// if no world is loaded
		if ( tr.world == NULL ) {
			fogIndex = 0;
		}
		// see if it is in a fog volume
		else if ( tr.world->numfogs == 1 ) {
			fogIndex = 0;
		} else {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( fogIndex = 1 ; fogIndex < tr.world->numfogs ; fogIndex++ ) {
				fog = &tr.world->fogs[fogIndex];
				if ( bounds[1][0] >= fog->bounds[0][0]
					&& bounds[1][1] >= fog->bounds[0][1]
					&& bounds[1][2] >= fog->bounds[0][2]
					&& bounds[0][0] <= fog->bounds[1][0]
					&& bounds[0][1] <= fog->bounds[1][1]
					&& bounds[0][2] <= fog->bounds[1][2] ) {
					break;
				}
			}
			if ( fogIndex == tr.world->numfogs ) {
				fogIndex = 0;
			}
		}
		poly->fogIndex = fogIndex;
	}
}


//=================================================================================

static int isnan_fp( const float *f )
{
	uint32_t u = *( (uint32_t*) f );
	u = 0x7F800000 - ( u & 0x7FFFFFFF );
	return (int)( u >> 31 );
}


/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent, qboolean intShaderTime ) {
	if ( !tr.registered ) {
		return;
	}
	if ( r_numentities >= MAX_REFENTITIES ) {
		R_LOG( rch_cmd, SEV_DEBUG, "RE_AddRefEntityToScene: Dropping refEntity, reached MAX_REFENTITIES\n" );
		return;
	}
	if ( isnan_fp( &ent->origin[0] ) || isnan_fp( &ent->origin[1] ) || isnan_fp( &ent->origin[2] ) ) {
		static qboolean first_time = qtrue;
		if ( first_time ) {
			first_time = qfalse;
			R_LOG( rch_cmd, SEV_WARN, "RE_AddRefEntityToScene passed a refEntity which has an origin with a NaN component\n" );
		}
		return;
	}
	if ( (unsigned)ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Terminate( TERM_CLIENT_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	RefEntityMotion_ClearOwned( &backEndData->entities[r_numentities].motion,
		&backEndData->entities[r_numentities].hasTemporal );
	memset( &backEndData->entities[r_numentities].temporalReceipt, 0,
		sizeof( backEndData->entities[r_numentities].temporalReceipt ) );
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
	backEndData->entities[r_numentities].hasLocalIrradiance = qfalse;
	memset( backEndData->entities[r_numentities].localShQ16, 0,
		sizeof( backEndData->entities[r_numentities].localShQ16 ) );
	backEndData->entities[r_numentities].localIrradianceHash = 0u;
	backEndData->entities[r_numentities].intShaderTime = intShaderTime;

	r_numentities++;
}

qboolean RE_SetRefEntityLocalIrradiance( uint32_t sceneEntityIndex,
		const int32_t coefficientsQ16[4][3], uint64_t coefficientHash ) {
	trRefEntity_t *entity;
	uint32_t channel, localAxis;
	if ( !coefficientsQ16 || !coefficientHash ||
		sceneEntityIndex >= (uint32_t)( r_numentities - r_firstSceneEntity ) ) return qfalse;
	entity = &backEndData->entities[r_firstSceneEntity + sceneEntityIndex];
	memcpy( entity->localShQ16[0], coefficientsQ16[0], sizeof( entity->localShQ16[0] ) );
	for ( localAxis = 0u; localAxis < 3u; ++localAxis ) {
		vec3_t worldAxis;
		VectorNormalize2( entity->e.axis[localAxis], worldAxis );
		for ( channel = 0u; channel < 3u; ++channel ) {
			double value = (double)coefficientsQ16[1][channel] * worldAxis[0] +
				(double)coefficientsQ16[2][channel] * worldAxis[1] +
				(double)coefficientsQ16[3][channel] * worldAxis[2];
			if ( value < (double)INT32_MIN || value > (double)INT32_MAX ) return qfalse;
			entity->localShQ16[localAxis + 1u][channel] =
				(int32_t)( value < 0.0 ? value - 0.5 : value + 0.5 );
		}
	}
	entity->localIrradianceHash = coefficientHash;
	entity->hasLocalIrradiance = qtrue;
	return qtrue;
}

void RE_AddRefEntityToSceneTemporal( const refEntity_t *ent,
		const refEntityMotion_t *motion ) {
	int before;
	if ( !RefEntityMotion_CanAppend( ent, motion,
			(uint32_t)r_numentities, MAX_REFENTITIES ) ) return;
	before = r_numentities;
	RE_AddRefEntityToScene( ent, qfalse );
	if ( r_numentities == before + 1 ) {
		RefEntityMotion_CopyOwned( &backEndData->entities[before].motion,
			&backEndData->entities[before].hasTemporal, motion );
	}
}


/*
=====================
RE_AddDynamicLightToScene
=====================
*/
static void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifndef USE_VULKAN
	// these cards don't have the correct blend mode
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		return;
	}
#endif
#ifdef USE_PMLIGHT
	// Per-pixel dlights apply the intensity/radius scaling (the retired fake tier did
	// not). `0` (off) never reaches here — the master gate clears dlights — so this is
	// effectively unconditional, but keep the guard explicit for clarity.
	if ( r_dynamiclight->integer )
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = qfalse;
}


/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b  ) {
	dlight_t	*dl;
	if ( VectorCompare( start, end ) ) {
		RE_AddDynamicLightToScene( start, intensity, r, g, b, 0 );
		return;
	}
	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifdef USE_PMLIGHT
	// Per-pixel dlights apply the intensity/radius scaling (the retired fake tier did
	// not). `0` (off) never reaches here — the master gate clears dlights — so this is
	// effectively unconditional, but keep the guard explicit for clarity.
	if ( r_dynamiclight->integer )
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[ r_numdlights++ ];
	VectorCopy( start, dl->origin );
	VectorCopy( end, dl->origin2 );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qtrue;
}


/*
=====================
Primitive submission (wired/render).

Renderer-side handlers for trap_R_Add*ToScene / trap_R_EmitParticles.
The ribbon path is wired to the renderervk ribbon pipeline; the
others remain stubs until their respective pipelines land.
=====================
*/
void RE_AddRibbonToScene( const ribbonDesc_t *desc ) {
	uint32_t frame, dstPointBase, dstHeaderIdx;
	byte *dstPoints, *dstHeader;
	uint32_t *udst;
	float    *fdst;

	// Validation. Drop silently on any failure — the renderer never
	// owns the right to log per-frame from this path. Cgame guarantees
	// the descriptor is valid by the time it reaches this trap; the
	// guards here are belt-and-braces against engine-side bugs.
	if ( !vk.ribbon.available )
		return;
	if ( desc == NULL || desc->points == NULL )
		return;
	if ( desc->numPoints < 2 || desc->numPoints > RIBBON_MAX_POINTS )
		return;

	// Bounds-check the per-frame ring buffers.
	if ( vk.ribbon.numHeadersThisFrame >= RIBBON_HEADERS_PER_FRAME )
		return;
	if ( vk.ribbon.numPointsThisFrame + (uint32_t)desc->numPoints
	     > RIBBON_POINTS_PER_FRAME )
		return;

	frame        = vk.cmd_index;
	dstPointBase = vk.ribbon.numPointsThisFrame;
	dstHeaderIdx = vk.ribbon.numHeadersThisFrame;

	// Append points. The host ribbonPoint_t struct (vec3 pos + float
	// width + vec4 rgba = 32 B) is layout-compatible with the GPU
	// RibbonPoint (vec4 posW + vec4 rgba, std430), so a memcpy is
	// safe — pos[0..2] lands in posW.xyz and `width` in posW.w
	// without reinterpret-cast tricks.
	dstPoints = vk.ribbon.points_ptr[frame] + dstPointBase * RIBBON_POINT_BYTES;
	memcpy( dstPoints, desc->points, (size_t)desc->numPoints * RIBBON_POINT_BYTES );

	// Append header.
	// Layout (24 B std430, must match ribbon.vert RibbonHeader):
	//   offset  0..15  4 × uint  (pointOffset, pointCount, shaderHandle, flags)
	//   offset 16..23  vec2      uvScroll
	// Ribbon is transient-only; uvScroll references the absolute frame
	// clock (frameParams.y) directly, so no per-submission spawnTime is
	// needed (an earlier change dropped the dormant field). For a future
	// persistent ribbon variant, restore the field here and in
	// ribbon.vert's RibbonHeader (mirror beam's flag-branch pattern).
	dstHeader = vk.ribbon.headers_ptr[frame] + dstHeaderIdx * RIBBON_HEADER_BYTES;
	udst = (uint32_t *)dstHeader;
	fdst = (float    *)dstHeader;
	udst[0] = dstPointBase;
	udst[1] = (uint32_t)desc->numPoints;
	// cgame submits a qhandle; the GPU header carries a
	// primitive registry slot. Translate via the indirection table.
	//
	// Bits 0..30 carry the registry slot; bit 31 the
	// slot image's colour domain (CD_LINEAR → ribbon.frag samples raw).
	// ribbon.vert passes the packed value through unchanged; ribbon.frag
	// masks the slot before the range clamp. Every registered primitive
	// image is CD_SRGB today → bit 31 is 0 → no behaviour change.
	{
		uint32_t primSlot = vk_qhandle_to_prim_slot( desc->shader );
		if ( primSlot < PRIMITIVE_SHADER_IMAGE_MAX
		  && vk_primitive_shader_images[primSlot] != NULL
		  && vk_primitive_shader_images[primSlot]->colorDomain == CD_LINEAR )
			primSlot |= 0x80000000u;
		udst[2] = primSlot;
	}
	udst[3] = (uint32_t)desc->flags;
	fdst[4] = desc->uvScroll[0];
	fdst[5] = desc->uvScroll[1];

	vk.ribbon.numPointsThisFrame  += (uint32_t)desc->numPoints;
	vk.ribbon.numHeadersThisFrame += 1;
}

/*
=====================
RE_AddBeamToScene

Append one beam descriptor to the engine-managed pool. The pool
holds both transient (one-frame, duration == 0) and persistent
(lifetime + fade, duration > 0) beams in a single BEAM_POOL_MAX-slot
array; RB_DrawBeams walks the pool each frame, resolves
entity-attached endpoints into world space, computes fade alpha for
persistent slots, writes a compacted run of GPU headers to the
per-frame SSBO, and issues a single vkCmdDraw.

Pool exhaustion drops the submission silently — at the current
BEAM_POOL_MAX (128), this is rare in practice. The
heaviest current consumer is LG primary at 2 slots per firing player
(main body + tapered tail); 16-player matches with all firing land
~70 slots used, comfortably within budget. A noisy log on overflow
would be more disruptive than helpful.

Validates handle / range; on any failure, the pool stays unchanged.
=====================
*/
void RE_AddBeamToScene( const beamDesc_t *desc ) {
	int slot;
	int copies;

	if ( !tr.registered ) return;
	if ( desc == NULL ) return;
	if ( !vk.beam.available ) return;

	// Linear scan for a free slot. 64 entries; sub-microsecond cost.
	for ( slot = 0; slot < (int)BEAM_POOL_MAX; slot++ ) {
		if ( !vk.beam.active[slot] ) break;
	}
	if ( slot == (int)BEAM_POOL_MAX ) {
		// Pool full — drop silently.
		return;
	}

	// Clamp axialCopies to [1, BEAM_AXIAL_MAX]. Out-of-range values
	// from older or buggy callers degrade gracefully.
	copies = desc->axialCopies;
	if ( copies < 1 )                    copies = 1;
	if ( copies > (int)BEAM_AXIAL_MAX )  copies = (int)BEAM_AXIAL_MAX;

	// Copy the descriptor into the pool slot. Persistent metadata
	// is captured even for transient beams (duration == 0); the
	// lifetime check in RB_DrawBeams handles both uniformly via
	// the duration == 0 branch. spawnTime uses the current frame's
	// floatTime so persistent beams' fadeIn ramp is anchored at
	// submission time.
	vk.beam.desc[slot]              = *desc;
	vk.beam.desc[slot].axialCopies  = copies;
	// PRIM_FLAG_TRANSIENT is engine-managed, derived from duration.
	// Mask any caller-set value (cgame should not set this bit) then
	// re-set it ourselves based on duration. Other flag bits pass
	// through unchanged. Without this, transient beams' uvScroll is
	// non-functional because age == 0 every per-frame submit; the
	// vertex shader needs the bit to know to use absolute frame time
	// as the scroll reference instead.
	vk.beam.desc[slot].flags &= ~PRIM_FLAG_TRANSIENT;
	if ( desc->duration <= 0.0f ) {
		vk.beam.desc[slot].flags |= PRIM_FLAG_TRANSIENT;
	}
	// spawnTime uses tr.refdef.floatTime (front-end current-frame
	// time, set in RE_BeginScene) rather than backEnd.refdef.floatTime
	// — the back-end's refdef holds the PREVIOUS frame's value at
	// the moment RE_AddBeamToScene runs (re_AddBeamToScene is invoked
	// during cgame frame processing, before commands are flushed to
	// the back-end). Using the front-end value keeps spawnTime
	// aligned with what RB_DrawBeams will see as the current draw-
	// frame's tr.refdef.floatTime, so age = 0 on the first render
	// and fadeIn ramps cleanly from there.
	//
	// For transient beams the value is still captured (kept uniform
	// with the persistent path to avoid branchy host code), but the
	// vertex shader ignores it because PRIM_FLAG_TRANSIENT routes
	// uvScroll to absolute frameParams.y.
	vk.beam.spawnTime[slot]         = (float)tr.refdef.floatTime;
	vk.beam.duration[slot]          = desc->duration;
	vk.beam.fadeIn[slot]            = desc->fadeIn;
	vk.beam.fadeOut[slot]           = desc->fadeOut;
	vk.beam.active[slot]            = qtrue;
}

/*
=====================
RE_AddRailRibbonToScene

Claim a persistent rail-ribbon pool slot for one helix, stamping the
spawn time. The pool's draw pass (RB_DrawRailRibbons) regenerates the
evolving spiral geometry every frame from (currentTime - spawnTime)
until the duration expires, then frees the slot. Mirrors the beam
pool's slot lifecycle; the per-slot payload is the spawn-fixed spiral
descriptor (start / beamAxis / perpAxis[36] / beamLen / color), not a
point array — the geometry is derived GPU-side, so nothing is per-frame.

Pool exhaustion drops the submission silently (RAIL_RIBBON_POOL_MAX == 8,
matching the ≤8 concurrent rail trails cgame allows).
=====================
*/
void RE_AddRailRibbonToScene( const railRibbonDesc_t *desc ) {
	int slot;

	if ( !tr.registered ) return;
	if ( desc == NULL ) return;
	if ( !vk.railRibbon.available ) return;
	if ( desc->duration <= 0.0f ) return;   // helix is always persistent
	if ( desc->beamLen <= 0.0f ) return;

	for ( slot = 0; slot < (int)RAIL_RIBBON_POOL_MAX; slot++ ) {
		if ( !vk.railRibbon.active[slot] ) break;
	}
	if ( slot == (int)RAIL_RIBBON_POOL_MAX ) {
		return;   // pool full — drop silently
	}

	vk.railRibbon.desc[slot]      = *desc;
	// Front-end current-frame time (see RE_AddBeamToScene's note): keeps
	// spawnTime aligned with what RB_DrawRailRibbons reads as the draw
	// frame's floatTime, so age == 0 on the first render.
	vk.railRibbon.spawnTime[slot] = (float)tr.refdef.floatTime;
	vk.railRibbon.duration[slot]  = desc->duration;
	vk.railRibbon.active[slot]    = qtrue;
}

/*
=====================
RE_AddSpriteToScene

Append one GPU SpriteHeader (std430, 48 bytes) to the per-frame
SSBO indexed by vk.cmd_index. Drops silently on validation failure
or capacity exhaustion — the renderer never owns the right to log
per-frame from this path.

GPU layout (must match sprite.vert):
    bytes  0..15  vec4  originW       (.xyz=position, .w=radius)
    bytes 16..31  vec4  rgba
    bytes 32..35  uint  shaderHandle  (reserved)
    bytes 36..39  uint  flags         (PRIM_FLAG_*)
    bytes 40..47  uint[2] padding to std430 vec4 alignment
=====================
*/
void RE_AddSpriteToScene( const spriteDesc_t *desc ) {
	uint32_t  frame, idx;
	byte     *dst;
	float    *fdst;
	uint32_t *udst;

	if ( !vk.sprite.available )
		return;
	if ( desc == NULL || desc->radius <= 0.0f )
		return;

	if ( vk.sprite.numThisFrame >= SPRITES_PER_FRAME )
		return;

	frame = vk.cmd_index;
	idx   = vk.sprite.numThisFrame;
	dst   = vk.sprite.headers_ptr[frame] + idx * SPRITE_HEADER_BYTES;
	fdst  = (float    *)dst;
	udst  = (uint32_t *)dst;

	// originW.xyz = origin, originW.w = radius
	fdst[0] = desc->origin[0];
	fdst[1] = desc->origin[1];
	fdst[2] = desc->origin[2];
	fdst[3] = desc->radius;

	// rgba (already float [0..1] per primitives.h convention)
	fdst[4] = desc->rgba[0];
	fdst[5] = desc->rgba[1];
	fdst[6] = desc->rgba[2];
	fdst[7] = desc->rgba[3];

	// cgame qhandle → primitive registry slot translation.
	udst[8]  = vk_qhandle_to_prim_slot( desc->shader );
	udst[9]  = (uint32_t)desc->flags;
	udst[10] = 0; // pad0
	udst[11] = 0; // pad1

	vk.sprite.numThisFrame += 1;
}

static uint32_t R_QueueParticleEmitter( const emitterDesc_t *desc,
		uint32_t deterministicSeed, uint32_t profileHandle,
		uint32_t stageIndex, uint32_t stageFlags ) {
	particleSpawnGPU_t request;
	uint32_t count, groups, requested, remaining;

	if ( !vk.particle.available ) return 0u;
	if ( desc == NULL ) return 0u;
	if ( desc->cls < 1
	  || (uint32_t)desc->cls > vk.particle.numClasses ) return 0u;
	if ( desc->count <= 0 ) return 0u;
	if ( vk.particle.spawnRequestCount >= PARTICLE_SPAWN_REQUEST_MAX ) {
		tr.pc.c_particleDroppedRequests++;
		tr.pc.c_particleDroppedParticles += desc->count;
		return 0u;
	}

	requested = (uint32_t)desc->count;
	remaining = PARTICLES_PER_POOL - vk.particle.spawnParticleCount;
	if ( remaining == 0u ) {
		tr.pc.c_particleDroppedRequests++;
		tr.pc.c_particleDroppedParticles += desc->count;
		return 0u;
	}
	count = requested;
	if ( count > remaining ) {
		count = remaining;
		tr.pc.c_particleDroppedParticles += (int)( requested - count );
	}
	memset( &request, 0, sizeof( request ) );
	VectorCopy( desc->origin, request.origin );
	VectorCopy( desc->axis, request.axis );
	VectorCopy( desc->end, request.end );
	Vector4Copy( desc->colorTint, request.colorTint );
	request.classHandle = (uint32_t)desc->cls;
	request.count = count;
	request.firstSlot = vk.particle.nextSlot;
	request.profileHandle = profileHandle;
	request.stageIndex = stageIndex;
	request.stageFlags = stageFlags;
	if ( deterministicSeed != 0u ) {
		request.seed = deterministicSeed;
	} else {
		vk.particle.spawnSeed++;
		if ( vk.particle.spawnSeed == 0u ) vk.particle.spawnSeed = 1u;
		request.seed = vk.particle.spawnSeed;
	}
	if ( !vk_particle_shadow_write_spawn( vk.particle.spawnRequestCount,
			&request ) ) return 0u;
	vk.particle.spawnRequestCount++;
	vk.particle.spawnParticleCount += count;
	groups = ( count + 63u ) / 64u;
	if ( groups > vk.particle.spawnMaxGroups )
		vk.particle.spawnMaxGroups = groups;
	vk.particle.nextSlot = ( vk.particle.nextSlot + count ) % PARTICLES_PER_POOL;

	tr.pc.c_particleEmitters++;
	tr.pc.c_particleParticles += (int)count;
	tr.pc.c_particleSpawnRequests++;
	return count;
}

void RE_EmitParticles( const emitterDesc_t *desc ) {
	R_QueueParticleEmitter( desc, 0u, 0u, 0u, 0u );
}

#define ATMOSPHERE_EFFECT_RUNTIME_MAX 256u

typedef struct {
	uint32_t id;
	uint32_t profile;
	uint32_t seed;
	uint32_t emitted[ATMOSPHERE_EFFECT_MAX_STAGES];
	uint32_t pending[ATMOSPHERE_EFFECT_MAX_STAGES];
	float rateCarry[ATMOSPHERE_EFFECT_MAX_STAGES];
	uint32_t startedMask;
	uint32_t totalEmitted;
	float startTimeline;
	float lastTimeline;
	qboolean active;
} atmosphereEffectRuntime_t;

static atmosphereEffectRuntime_t s_atmosphereEffectRuntime[
	ATMOSPHERE_EFFECT_RUNTIME_MAX];

void RE_ResetAtmosphereEffectRuntime( void ) {
	memset( s_atmosphereEffectRuntime, 0,
		sizeof( s_atmosphereEffectRuntime ) );
}

static atmosphereEffectRuntime_t *R_AtmosphereEffectRuntime(
		const atmosphereEmitter_t *emitter ) {
	atmosphereEffectRuntime_t *freeSlot = NULL;
	atmosphereEffectRuntime_t *oldest = NULL;
	uint32_t i;
	for ( i = 0u; i < ATMOSPHERE_EFFECT_RUNTIME_MAX; ++i ) {
		atmosphereEffectRuntime_t *runtime = &s_atmosphereEffectRuntime[i];
		if ( runtime->active && runtime->id == emitter->id ) {
			if ( runtime->profile != emitter->profile
					|| runtime->seed != emitter->seed
					|| vk.atm.timelineSeconds < runtime->lastTimeline ) {
				memset( runtime, 0, sizeof( *runtime ) );
				break;
			}
			return runtime;
		}
		if ( !runtime->active && freeSlot == NULL ) freeSlot = runtime;
		if ( runtime->active && ( oldest == NULL
				|| runtime->lastTimeline < oldest->lastTimeline ) ) oldest = runtime;
	}
	if ( i < ATMOSPHERE_EFFECT_RUNTIME_MAX )
		freeSlot = &s_atmosphereEffectRuntime[i];
	if ( freeSlot == NULL ) freeSlot = oldest;
	if ( freeSlot == NULL ) return NULL;
	memset( freeSlot, 0, sizeof( *freeSlot ) );
	freeSlot->id = emitter->id;
	freeSlot->profile = emitter->profile;
	freeSlot->seed = emitter->seed;
	freeSlot->startTimeline = vk.atm.timelineSeconds;
	freeSlot->lastTimeline = vk.atm.timelineSeconds;
	freeSlot->active = qtrue;
	return freeSlot;
}

// Lower persistent semantic atmosphere intent to bounded ROOT stage requests.
// Scheduling cost is O(emitters * stages), while every particle instance is
// expanded and simulated on GPU. Cumulative fractional rates make bursts and
// continuous emission frame-rate independent and prevent frame-local resubmission from
// replaying the emitter's whole lifetime. Collision/death child stages remain
// GPU event work; they are deliberately not guessed on the host.
static float R_AtmosphereStageActiveTime( float elapsed,
		const atmosphereEmitter_t *emitter,
		const atmosphereEffectProfile_t *profile,
		const atmosphereEffectStage_t *stage ) {
	float active = elapsed - stage->delay;
	if ( active < 0.0f ) active = 0.0f;
	if ( active > stage->duration ) active = stage->duration;
	if ( emitter->lifetime > 0.0f && active > emitter->lifetime )
		active = emitter->lifetime;
	if ( profile->duration > 0.0f && active > profile->duration )
		active = profile->duration;
	return active;
}

void RE_EmitAtmosphereProfile( const atmosphereEmitter_t *emitter,
		const atmosphereEffectProfile_t *profile ) {
	atmosphereEffectRuntime_t *runtime;
	float elapsed, previousElapsed, effectiveIntensity;
	uint32_t i;
	if ( !emitter || !profile ) return;
	effectiveIntensity = RenderSubmission_AtmosphereEmitterIntensity(
		&vk.atm.state, emitter );
	runtime = R_AtmosphereEffectRuntime( emitter );
	if ( runtime == NULL ) return;
	elapsed = vk.atm.timelineSeconds - runtime->startTimeline;
	previousElapsed = runtime->lastTimeline - runtime->startTimeline;
	if ( elapsed < 0.0f ) elapsed = 0.0f;
	if ( previousElapsed < 0.0f || previousElapsed > elapsed )
		previousElapsed = elapsed;
	if ( effectiveIntensity <= 0.0f ) {
		runtime->lastTimeline = vk.atm.timelineSeconds;
		return;
	}
	for ( i = 0u; i < profile->stageCount; ++i ) {
		const atmosphereEffectStage_t *stage = &profile->stages[i];
		emitterDesc_t desc;
		float activeSeconds, previousActive, generated;
		uint32_t count, remaining, stageRemaining;
		if ( ( stage->trigger != ATMOSPHERE_STAGE_EMISSION
				&& stage->trigger != ATMOSPHERE_STAGE_CONTINUOUS )
				|| stage->parentStage != UINT32_MAX || elapsed < stage->delay ) continue;
		if ( stage->lodFar > 0.0f ) {
			vec3_t delta;
			VectorSubtract( emitter->origin, tr.refdef.vieworg, delta );
			if ( DotProduct( delta, delta ) > stage->lodFar * stage->lodFar ) continue;
		}
		activeSeconds = R_AtmosphereStageActiveTime( elapsed,
			emitter, profile, stage );
		previousActive = R_AtmosphereStageActiveTime( previousElapsed,
			emitter, profile, stage );
		if ( ( runtime->startedMask & ( 1u << i ) ) == 0u ) {
			generated = ceilf( (float)stage->burstCount * effectiveIntensity
				* stage->intensityScale );
			if ( generated > (float)UINT32_MAX ) generated = (float)UINT32_MAX;
			runtime->pending[i] = (uint32_t)generated;
			runtime->startedMask |= 1u << i;
		}
		if ( activeSeconds > previousActive && stage->spawnRate > 0.0f ) {
			runtime->rateCarry[i] += stage->spawnRate
				* ( activeSeconds - previousActive )
				* effectiveIntensity * stage->intensityScale;
			generated = floorf( runtime->rateCarry[i] );
			if ( generated > 0.0f ) {
				const uint32_t whole = generated > (float)UINT32_MAX
					? UINT32_MAX : (uint32_t)generated;
				const uint64_t pending = (uint64_t)runtime->pending[i] + whole;
				runtime->pending[i] = pending > UINT32_MAX
					? UINT32_MAX : (uint32_t)pending;
				runtime->rateCarry[i] -= (float)whole;
			}
		}
		if ( runtime->emitted[i] >= stage->maxParticles
				|| runtime->totalEmitted >= profile->maxParticles ) {
			runtime->pending[i] = 0u;
			runtime->rateCarry[i] = 0.0f;
			continue;
		}
		stageRemaining = stage->maxParticles - runtime->emitted[i];
		count = runtime->pending[i];
		if ( count > stageRemaining ) count = stageRemaining;
		remaining = profile->maxParticles - runtime->totalEmitted;
		if ( count > remaining ) count = remaining;
		if ( count == 0u ) continue;
		memset( &desc, 0, sizeof( desc ) );
		desc.cls = (particleClassHandle_t)stage->particleClass;
		desc.count = (int)count;
		VectorCopy( emitter->origin, desc.origin );
		VectorCopy( emitter->direction, desc.axis );
		VectorMA( emitter->origin, emitter->radius, emitter->direction, desc.end );
		Vector4Set( desc.colorTint, 1.0f, 1.0f, 1.0f, effectiveIntensity );
		count = R_QueueParticleEmitter( &desc,
			( emitter->seed ^ profile->seed
				^ ( 0x9e3779b9u * ( i + 1u ) )
				^ runtime->emitted[i] ) | 1u,
			emitter->profile, i, stage->flags );
		runtime->emitted[i] += count;
		runtime->totalEmitted += count;
		runtime->pending[i] -= count;
	}
	runtime->lastTimeline = vk.atm.timelineSeconds;
}

// Resolve a decal shader qhandle to a slot in the projector's texture registry
// (vk.decal.images[]). Find-or-add: a previously-resolved shader reuses its
// slot; a new one claims the next free slot and writes its image into the
// fragment shader's binding-2 sampler array. Three-tier image fallback mirrors
// the particle precedent (RE_RegisterParticleClass): class shader →
// defaultShader → tr.whiteImage. The registry has MAX_DECAL_TEXTURES slots; once
// full, every further shader collapses to slot 0 (tr.whiteImage) rather than
// OOB-indexing the bounded array — generous since decals reuse a handful of mark
// shaders.
static uint32_t R_ResolveDecalTextureSlot( qhandle_t shaderHandle ) {
	shader_t *resolvedShader;
	image_t  *resolvedImage;
	uint32_t  k;

	if ( !vk.decal.available )
		return 0;

	resolvedShader = R_GetShaderByHandle( shaderHandle );
	if ( resolvedShader && resolvedShader->stages[0]
	  && resolvedShader->stages[0]->bundle[0].image[0] ) {
		resolvedImage = resolvedShader->stages[0]->bundle[0].image[0];
	} else if ( tr.defaultShader && tr.defaultShader->stages[0]
	         && tr.defaultShader->stages[0]->bundle[0].image[0] ) {
		resolvedImage = tr.defaultShader->stages[0]->bundle[0].image[0];
	} else {
		resolvedImage = tr.whiteImage;
	}

	// Find an existing slot with this image.
	for ( k = 0; k < vk.decal.numImages && k < MAX_DECAL_TEXTURES; k++ ) {
		if ( vk.decal.images[k] == resolvedImage )
			return k;
	}

	// Registry full → fall back to slot 0 (tr.whiteImage default).
	if ( vk.decal.numImages >= MAX_DECAL_TEXTURES )
		return 0;

	// Claim the next free slot and push the image into the sampler array.
	k = vk.decal.numImages++;
	vk.decal.images[k] = resolvedImage;
	vk_decal_set_texture_image( (int)k, resolvedImage );
	return k;
}

// Per-decal blend modes. Each maps to one of the three projector pipelines
// (built in vk_init_decal) and is selected on the GPU by the vertex-stage
// degenerate-cull. Resolved renderer-side from the mark shader's stage-0 blend
// bits so the cgame descriptor carries no blend state:
//   DECAL_BLEND_ALPHA    = blood     SRC_ALPHA / ONE_MINUS_SRC_ALPHA
//   DECAL_BLEND_ADDITIVE = burn      SRC_ALPHA / ONE
//   DECAL_BLEND_COLOUR   = bullet    RGB coverage translated to bounded alpha-darken
#define DECAL_BLEND_ALPHA      0u
#define DECAL_BLEND_ADDITIVE   1u
#define DECAL_BLEND_COLOUR     2u

// Repeated hits at effectively the same surface point must not compound a
// colour-mask decal back toward mathematical black. Search only the recent
// tail of the CPU shadow ring at spawn time: this is bounded event work, adds
// no per-frame/GPU cost, and covers more than twenty full shotgun volleys.
#define DECAL_COALESCE_LOOKBACK       256u
#define DECAL_COALESCE_NORMAL_DOT      0.98f
#define DECAL_COALESCE_RADIUS_FRAC     0.50f

// Resolve a decal mark shader to its blend mode by reading the destination-blend
// bits of the shader's first stage. ONE_MINUS_SRC_COLOR → colour-blend (bullet),
// ONE → additive (burn), anything else (incl. ONE_MINUS_SRC_ALPHA / default) →
// straight alpha (blood). Falls back to alpha if the shader or its stage-0 is
// absent.
static uint32_t R_ResolveDecalBlendMode( qhandle_t shaderHandle ) {
	shader_t *resolvedShader;
	uint32_t  dstBlend;

	resolvedShader = R_GetShaderByHandle( shaderHandle );
	if ( !resolvedShader || !resolvedShader->stages[0] )
		return DECAL_BLEND_ALPHA;

	dstBlend = resolvedShader->stages[0]->stateBits & GLS_DSTBLEND_BITS;

	if ( dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR )
		return DECAL_BLEND_COLOUR;
	if ( dstBlend == GLS_DSTBLEND_ONE )
		return DECAL_BLEND_ADDITIVE;
	return DECAL_BLEND_ALPHA;
}

static qboolean R_FindCoalescedColourDecalSlot( const decalGPU_t *decal,
		uint32_t *outSlot ) {
	uint32_t lookback;

	if ( decal == NULL || outSlot == NULL
			|| decal->blendMode != DECAL_BLEND_COLOUR ) {
		return qfalse;
	}

	for ( lookback = 1u; lookback <= DECAL_COALESCE_LOOKBACK; ++lookback ) {
		decalGPU_t candidate;
		vec3_t delta;
		float minRadius;
		float maxRadius;
		float mergeRadius;
		uint32_t slot = ( vk.decal.nextSlot + DECALS_PER_POOL - lookback )
			% DECALS_PER_POOL;

		if ( !vk_decal_shadow_read( slot, &candidate ) )
			continue;
		if ( candidate.blendMode != decal->blendMode
				|| candidate.textureIndex != decal->textureIndex )
			continue;
		if ( candidate.spawnTime == 0.0f && candidate.lifetimeInv == 0.0f )
			continue;
		if ( candidate.lifetimeInv > 0.0f
				&& ( (float)tr.refdef.floatTime - candidate.spawnTime )
					* candidate.lifetimeInv >= 1.0f )
			continue;
		if ( DotProduct( candidate.normalOrient, decal->normalOrient )
				< DECAL_COALESCE_NORMAL_DOT )
			continue;

		minRadius = candidate.originRadius[3] < decal->originRadius[3]
			? candidate.originRadius[3] : decal->originRadius[3];
		maxRadius = candidate.originRadius[3] > decal->originRadius[3]
			? candidate.originRadius[3] : decal->originRadius[3];
		if ( minRadius <= 0.0f || maxRadius > minRadius * 1.25f )
			continue;

		VectorSubtract( candidate.originRadius, decal->originRadius, delta );
		mergeRadius = minRadius * DECAL_COALESCE_RADIUS_FRAC;
		if ( DotProduct( delta, delta ) > mergeRadius * mergeRadius )
			continue;

		*outSlot = slot;
		return qtrue;
	}

	return qfalse;
}

// Spawn-time write into the GPU decal ring. The front-end (cgame via the trap)
// calls this at scene-build time. It translates the cgame descriptor into the
// renderer-private decalGPU_t, resolves the shader qhandle to a sampler-array
// slot, and stamps the decal into a round-robin pool slot. RB_DrawDecals reads
// the ring in the main pass. With no cgame call site emitting decals the ring
// stays empty and nothing changes on screen.
void RE_AddDecalToScene( const decalDesc_t *desc ) {
	decalGPU_t  d;
	uint32_t    slot;

	if ( !vk.decal.available || desc == NULL )
		return;

	memset( &d, 0, sizeof( d ) );

	// originRadius.xyz = origin, .w = radius; normalOrient.xyz = normal,
	// .w = orientation — folded next to their vec3 so the projector reads one
	// vec4 per pull (see the decalGPU_t layout note in vk.h).
	VectorCopy( desc->origin, d.originRadius );
	d.originRadius[3] = desc->radius;
	VectorCopy( desc->normal, d.normalOrient );
	d.normalOrient[3] = desc->orientation;

	Vector4Copy( desc->rgba, d.rgba );

	// Resolve the shader qhandle to a registry slot at emit time (mirrors how
	// the particle path resolves class shaders at registration). The fragment
	// shader samples decalTextures[textureIndex].
	//
	// The slot is always < MAX_DECAL_TEXTURES (64), so the upper bits are free:
	// bit 31 carries the DECAL_FLAG_NO_PROJECT flag into the GPU slot (mirrors the
	// particle path's bit-31 colour-domain packing in particleClassHandle). The
	// shaders mask the low bits for the sampler index and branch on bit 31 to skip
	// the depth box-projection for a free flat quad. No spare lane exists in the
	// 64 B decalGPU_t, and the reserved decalDesc_t.flags field carries no ABI — so
	// this packing keeps both struct sizes unchanged.
	d.textureIndex = R_ResolveDecalTextureSlot( desc->shader );
	if ( desc->flags & DECAL_FLAG_NO_PROJECT ) {
		d.textureIndex |= 0x80000000u;
	}

	// Stamp the front-end's current-frame time so the GPU derives
	// age = now - spawnTime each frame, and fold the lifetime into lifetimeInv
	// (= 1/lifetime, mirroring the particle path's divide-by-zero guard).
	// lifetimeInv 0 means "no auto-fade": the mark stays until its ring slot is
	// reused. The shader picks the fade CHANNEL from the resolved blendMode
	// (alpha-blend → ramp alpha; modulate/additive → ramp rgb to black), so the
	// caller's alphaFade hint isn't needed here — the actual blend pipeline is
	// authoritative (e.g. burn marks pass alphaFade but blend as modulate).
	d.spawnTime = (float)tr.refdef.floatTime;
	if ( desc->lifetime > 0.0f ) {
		float lifetime = desc->lifetime;
		if ( lifetime < 0.001f ) lifetime = 0.001f;
		d.lifetimeInv = 1.0f / lifetime;
	} else {
		d.lifetimeInv = 0.0f;
	}

	// Pick the blend mode from the mark shader so each decal rasterizes in the
	// matching projector pipeline (the vertex shader culls non-matching modes).
	d.blendMode   = R_ResolveDecalBlendMode( desc->shader );

	// Refresh a visually indistinguishable recent colour mark instead of
	// allowing repeated bullet/pellet hits to compound indefinitely. Distinct
	// impacts still consume the round-robin ring normally.
	if ( !R_FindCoalescedColourDecalSlot( &d, &slot ) ) {
		slot              = vk.decal.nextSlot;
		vk.decal.nextSlot = ( slot + 1 ) % DECALS_PER_POOL;
	}

	if ( !vk_decal_shadow_write( slot, &d ) ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"Decal RAL shadow publication failed" );
	}
}

// Lens-source occlusion (lens-glow B2 thin channel). The game registers a light
// source each frame; the depth-sampling lens oracle (vk_lens_dispatch) reports
// whether it is occluded. RE_AddLensSourceToScene only STASHES the descriptor —
// it must NOT project here, because backEnd.viewParms is not valid at scene-build
// time (it is set at RB_DrawSurfs). RB_AddLensSourceFlares does the render-time
// projection + registry write, exactly as the flare/halo path does.
void RE_AddLensSourceToScene( const lensSourceDesc_t *desc ) {
	lensSceneSource_t *s;

	if ( !tr.registered || desc == NULL )
		return;
	if ( r_numLensSources >= MAX_LENS_SCENE_SOURCES )
		return;

	s = &backEndData->lensSources[ r_numLensSources++ ];
	VectorCopy( desc->origin, s->origin );
	s->radius = desc->radius;
	s->id     = desc->id;
}

// Read back the oracle's last-frame visibility (0..1) for a registered source id.
// Returns qfalse when no GPU oracle is available (no registry mapped, e.g. r_lens
// off / no FBO) so the game falls back to its own occlusion test (zero feature
// loss). The 1-frame delay is intentional and matches the flare/halo contract:
// the oracle samples this frame's record next frame; we read what it wrote last.
qboolean RE_GetLensVisibility( int id, float *outVis ) {
	int slot;

	if ( !vk.lensSourcesPtr || !r_lens || !r_lens->integer || !vk.ral_lens_pipeline ) {
		if ( outVis ) *outVis = 1.0f;   // no oracle → treat as fully visible
		return qfalse;
	}

	slot = LENS_SLOT_CGSOURCES + ( ( id % LENS_MAX_CGSOURCES + LENS_MAX_CGSOURCES ) % LENS_MAX_CGSOURCES );
	if ( outVis )
		*outVis = *( (float *)vk.lensSourcesPtr + (size_t)slot * ( LENS_SOURCE_VEC4S * 4 ) + 7 );
	return qtrue;
}

/* One place that spells out the parm field mapping. A loop over an array of
   parms would be shorter but would lose the compile-time check that both
   structs still have these exact members. */
#define COPY_PARM( d, srcParm ) \
	do { \
		int cI; \
		(d).calc     = (int32_t)(srcParm).calc; \
		(d).val0     = (srcParm).val0; \
		(d).val1     = (srcParm).val1; \
		(d).variance = (srcParm).variance; \
		(d).parmPad0 = 0.0f; \
		(d).parmPad1 = 0.0f; \
		(d).parmPad2 = 0.0f; \
		/* Samples arrive already resolved: the curve table lives on the \
		   authoring side, and what crosses the boundary is a flat copy. The \
		   renderer therefore needs no knowledge of curve names or indices. */ \
		(d).hasCurve = (int32_t)(srcParm).hasCurve; \
		for ( cI = 0; cI < PARTICLE_CURVE_SAMPLES; cI++ ) \
			(d).samples[cI] = (srcParm).samples[cI]; \
	} while ( 0 )

void RE_RegisterParticleClass( particleClassHandle_t handle, const particleClass_t *cls ) {
	particleClassGPU_t local;
	particleClassGPU_t *dst;
	int i;

	if ( !vk.particle.available ) return;
	if ( cls == NULL ) return;
	if ( handle < 1 || handle > MAX_PARTICLE_CLASSES ) return;

	// Build one complete element locally, then publish it into the RAL-owned
	// class shadow as a single dirty slot.
	dst = &local;

	memset( dst, 0, sizeof( *dst ) );

	dst->shader            = (uint32_t)cls->shader;
	dst->renderFlags       = (uint32_t)cls->renderFlags;
	dst->emitMode          = (uint32_t)cls->emitMode;
	dst->scatterShape      = (uint32_t)cls->scatterShape;
	dst->scatterMagnitude  = cls->scatterMagnitude;
	dst->velocityShape     = (uint32_t)cls->velocityShape;
	dst->axialSpeed        = cls->axialSpeed;
	dst->cubeJitter        = cls->cubeJitter;
	dst->coneHalfAngle     = cls->coneHalfAngle;
	dst->lifetimeMean      = cls->lifetimeMean;
	dst->lifetimeJitter    = cls->lifetimeJitter;
	// Clamp paletteCount to [1, PARTICLE_CLASS_MAX_PALETTE].
	// 0 → would crash RE_EmitParticles's modulo at emit time.
	// >16 → would let particle.vert read colorPalette[idx] out of
	// bounds (the GLSL array is fixed at PARTICLE_CLASS_MAX_PALETTE).
	// Silent clamp; class definitions with bad paletteCount get
	// repaired rather than rejected.
	{
		int pc = cls->paletteCount;
		if ( pc < 1 ) pc = 1;
		if ( pc > PARTICLE_CLASS_MAX_PALETTE ) pc = PARTICLE_CLASS_MAX_PALETTE;
		dst->paletteCount = pc;
	}

	for ( i = 0; i < PARTICLE_CLASS_MAX_PALETTE; i++ ) {
		dst->colorPalette[i][0] = cls->colorPalette[i][0];
		dst->colorPalette[i][1] = cls->colorPalette[i][1];
		dst->colorPalette[i][2] = cls->colorPalette[i][2];
		dst->colorPalette[i][3] = cls->colorPalette[i][3];
	}

	dst->colorEndMult[0] = cls->colorEndMult[0];
	dst->colorEndMult[1] = cls->colorEndMult[1];
	dst->colorEndMult[2] = cls->colorEndMult[2];
	dst->colorEndMult[3] = cls->colorEndMult[3];

	dst->sizeStart    = cls->sizeStart;
	dst->sizeEnd      = cls->sizeEnd;
	dst->gravityScale = cls->gravityScale;
	dst->drag         = cls->drag;
	// shaderBlendIsAdditive set below; pad1..pad3 stay zero from the memset.

	// Expressivity-extension fields. .w lanes of the two vec4s are
	// unused — only .xyz carry data. Memset above already zeroed them
	// (which is the zero-effect default for any class that does not
	// opt in), so this block only matters when the class did populate
	// these fields.
	dst->velocityBias[0]       = cls->velocityBias[0];
	dst->velocityBias[1]       = cls->velocityBias[1];
	dst->velocityBias[2]       = cls->velocityBias[2];
	dst->velocityBias[3]       = 0.0f;
	dst->velocityBiasJitter[0] = cls->velocityBiasJitter[0];
	dst->velocityBiasJitter[1] = cls->velocityBiasJitter[1];
	dst->velocityBiasJitter[2] = cls->velocityBiasJitter[2];
	dst->velocityBiasJitter[3] = 0.0f;
	dst->speedJitter           = cls->speedJitter;
	dst->sizeJitter            = cls->sizeJitter;
	// pad4, pad5 stay zero from the memset.

	// Sprite-frame (flipbook) fields. frameSlots[] is resolved below
	// (left zero by the memset until then); frameCount/frameBlend carry
	// straight through. frameCount <= 1 leaves the whole flipbook path
	// inert and the single-shader path byte-identical.
	dst->frameCount = ( cls->frameCount > 1 ) ? (uint32_t)cls->frameCount : 0u;
	dst->frameBlend = ( cls->frameBlend != 0 ) ? 1u : 0u;

	// ── Curve-valued parameters.
	//
	// Copied member-wise rather than memcpy'd: the host type is
	// particleParm_t (int/float) and this one is explicit about which
	// lanes the shader reads, so a silent type or ordering change on
	// either side should be a compile error, not a garbled parm.
	//
	// No validation of `curve` here. An out-of-range index is handled
	// GPU-side by returning a neutral 1.0 sample, so a class referencing
	// a curve that failed to register still draws — degraded, not blank.
	// Rejecting it here would instead make the whole class silently
	// constant, which is harder to notice.
	COPY_PARM( dst->sizeParm,    cls->sizeParm );
	COPY_PARM( dst->alphaParm,   cls->alphaParm );
	COPY_PARM( dst->dragParm,    cls->dragParm );
	COPY_PARM( dst->gravityParm, cls->gravityParm );

	// ── Resolve class shader → image, write to sampler array.
	//
	// Three-tier fallback mirrors the IQM precedent at
	// tr_model_iqm.c:1495-1504. The resolved image_t is cached in
	// vk.particle.classImages[] so vk_init_descriptors's re-alloc
	// path can re-walk the registry after a pool reset.
	//
	// Blend mode is derived from the class shader's stages[0] state
	// bits (additive vs alpha). The vertex shader filters per blend
	// variant by reading shaderBlendIsAdditive — so the per-particle
	// PRIM_FLAG_ADDITIVE bit on cls->renderFlags is no longer
	// load-bearing for blend selection. See primitives.h.
	{
		shader_t *resolvedShader = R_GetShaderByHandle( cls->shader );
		image_t  *resolvedImage;
		uint32_t  isAdditive = 0;

		if ( resolvedShader && resolvedShader->stages[0]
		  && resolvedShader->stages[0]->bundle[0].image[0] ) {
			resolvedImage = resolvedShader->stages[0]->bundle[0].image[0];
		} else if ( tr.defaultShader && tr.defaultShader->stages[0]
		         && tr.defaultShader->stages[0]->bundle[0].image[0] ) {
			resolvedImage = tr.defaultShader->stages[0]->bundle[0].image[0];
		} else {
			resolvedImage = tr.whiteImage;
		}

		// Derive blend mode from the resolved shader's stage 0 state
		// bits. Two patterns are recognised as additive:
		//   GL_SRC_ALPHA / GL_ONE  (alpha-modulated additive — q3 sprite convention)
		//   GL_ONE       / GL_ONE  (pure additive — "blendfunc add")
		// Any other combination falls back to alpha-blend for now;
		// dst-color / modulate / etc. are out of phase-5 scope.
		if ( resolvedShader && resolvedShader->stages[0] ) {
			uint32_t blendBits = resolvedShader->stages[0]->stateBits & GLS_BLEND_BITS;
			if ( blendBits == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE )
			  || blendBits == ( GLS_SRCBLEND_ONE       | GLS_DSTBLEND_ONE ) ) {
				isAdditive = 1;
			}
		}
		dst->shaderBlendIsAdditive = isAdditive;

		// Carry the resolved image's colour domain
		// (CD_SRGB(0) | CD_LINEAR(1)) into the GPU class record;
		// particle.vert packs it into bit 31 of particleClassHandle so
		// particle.frag can sample raw vs. sRGB-decode per class. Every
		// shipped class image is CD_SRGB today → 0 → no behaviour change.
		dst->colorDomain = (uint32_t)resolvedImage->colorDomain;

		// Cache image pointer for re-alloc-after-pool-reset.
		vk.particle.classImages[ handle - 1 ] = resolvedImage;

		// Advance the RAL texture-registry generation. The actively recorded
		// slot replaces its complete arena bind group before the next draw.
		vk_particle_set_class_image( handle, resolvedImage );
	}

	// ── Sprite-frame (flipbook) resolve. Only when the class opts in
	// (frameCount > 1). Each frameShaders[i] resolves to a dedicated
	// frame-pool slot [64..95] via the same three-tier image fallback as
	// the single shader; the resolved ABSOLUTE sampler-array slot
	// (MAX_PARTICLE_CLASSES + bare pool slot = [64..95], per the vk.h
	// frameSlots[] contract) is stored in dst->frameSlots[i] so the
	// fragment's particleSamplers[frameSlot0] indexes the frame pool
	// directly. A static class (frameCount <= 1) skips this entirely →
	// byte-identical.
	if ( cls->frameCount > 1 ) {
		int n = cls->frameCount;
		int i;
		if ( n > PARTICLE_CLASS_MAX_FRAMES ) n = PARTICLE_CLASS_MAX_FRAMES;

		// Fail gracefully if the frame pool cannot fit this class's
		// frames: leave frameCount 0 (static fallback via `shader`),
		// matching how a full class table is tolerated rather than
		// corrupting the pool. Registration of the class itself stands.
		if ( vk.particle.frameNextSlot + (uint32_t)n > FRAME_POOL_SIZE ) {
			R_LOG( rch_cmd, SEV_WARN,
				"RE_RegisterParticleClass: frame pool overflow (%u + %d > %u); "
				"class %d falls back to static shader\n",
				vk.particle.frameNextSlot, n, (unsigned)FRAME_POOL_SIZE, handle );
			dst->frameCount = 0;
		} else {
			for ( i = 0; i < n; i++ ) {
				shader_t *fsh = R_GetShaderByHandle( cls->frameShaders[i] );
				image_t  *fimg;
				uint32_t  slot;

				if ( fsh && fsh->stages[0]
				  && fsh->stages[0]->bundle[0].image[0] ) {
					fimg = fsh->stages[0]->bundle[0].image[0];
				} else if ( tr.defaultShader && tr.defaultShader->stages[0]
				         && tr.defaultShader->stages[0]->bundle[0].image[0] ) {
					fimg = tr.defaultShader->stages[0]->bundle[0].image[0];
				} else {
					fimg = tr.whiteImage;
				}

				slot = vk.particle.frameNextSlot++;
				dst->frameSlots[i] = (uint32_t)MAX_PARTICLE_CLASSES + slot;   // absolute sampler-array slot [64..95], per vk.h:1233 contract
				vk_particle_set_frame_image( (int)slot, fimg );              // helper takes the BARE pool slot; it adds +MAX_PARTICLE_CLASSES itself (vk.c:7064)
			}
			dst->frameCount = (uint32_t)n;
		}
	}

	if ( !vk_particle_shadow_write_class( (uint32_t)handle - 1u, dst ) ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"Particle RAL class-shadow publication failed" );
		return;
	}
	// Registration is monotonic in the static-init use case (handle
	// equals numClasses + 1), but tolerate re-registration as
	// overwrite without bumping the counter.
	if ( (uint32_t)handle > vk.particle.numClasses ) {
		vk.particle.numClasses = (uint32_t)handle;
	}
}


/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}


/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}


void *R_GetCommandBuffer( int bytes );

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene( const refdef_t *fd, int worldIndex ) {
#ifdef USE_VULKAN
	renderCommand_t	lastRenderCommand;
#endif
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}

	if ( r_norefresh->integer ) {
		return;
	}
	startTime = ri.Milliseconds();
	if ( fd->rdflags & RDF_HYPERSPACE ) {
		R_TemporalMarkCameraCut( worldIndex );
	}

	// Select the rendering app's world slot. A world scene reads its app's loaded
	// world; a worldless scene (UI/menu 3D, RDF_NOWORLDMODEL) leaves tr.world as-is
	// since it has no world to read. Single app: slot 0 is the only loaded world,
	// so this resolves tr.world to exactly what RE_LoadWorldMap published.
	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) {
			worldIndex = 0;
		}
		R_SetWorldSlot( worldIndex );
	}

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_RenderScene: NULL worldmodel");
	}

	memcpy( tr.refdef.text, fd->text, sizeof( tr.refdef.text ) );

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	/* c2-shadertime-pin — dev/C2-smoke override: when r_pinShaderTime is
	 * non-zero, pin tr.refdef.time / floatTime to a fixed value so the
	 * inputs to wave-shader evaluation (tess.shaderTime → EvalWaveForm,
	 * R_NoiseGet4f, deform / tcMod / rgbGen wave) sit at a constant phase
	 * across cold-cache launches. Eliminates the per-launch animation-
	 * phase bimodality that the c2-brightness-reinvestigation pinned as
	 * the residual C2 nondeterminism after the noise-table fix. Default
	 * (0.0) = no pin → today's wall-clock-driven gameplay path. */
	if ( r_pinShaderTime->value > 0.0f ) {
		tr.refdef.time = (int)( r_pinShaderTime->value * 1000.0f );
	}

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;

		// compare the area bits
		areaDiff = 0;
		for ( int i = 0; i < MAX_MAP_AREA_BYTES/sizeof(int); i++ ) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}


	// derived info

	tr.refdef.floatTime = (double)tr.refdef.time * 0.001; // -EC-: cast to double

#ifdef USE_VULKAN
	R_InjectRalEffectsSmoke( fd );
#endif

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

#ifdef USE_PMLIGHT
	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;
	// Forward+ deduped lit-surface union (each surface lit by any light appears once).
	tr.refdef.numFpUnionSurfs = 0;
	tr.refdef.fpUnionSurfs = backEndData->fpUnionSurfs;
#endif

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

#if FEAT_SHADOW_MAPPING
	// Test-only (r_dlightShadowTest, CVAR_CHEAT): append a synthetic dlight so the omni
	// shadow path can be exercised + spatially verified headless (real dlights need
	// gameplay firing). Placed at a fixed offset ABOVE the view origin so floor/wall
	// geometry occludes it from a receiver behind a wall/pillar. The SAME light feeds the
	// producer, the Forward+ lit pass, and the shadow render (one shared viewParms.dlights
	// entry → consistent index). Default 0 → no injection → byte-identical.
	if ( r_dlightShadowTest && r_dlightShadowTest->integer > 0 ) {
		float rad = (float)r_dlightShadowTest->integer;
		// Inject N synthetic lights (r_dlightShadowTestN, default 1) in a horizontal ring
		// around the view so the K-light shadow budget can be exercised + spatially verified
		// headless: each light is at a distinct azimuth so its cast shadow points a different
		// way. N=1 → a single light directly above the eye (the original placement). Each is
		// white, the SAME entry feeds producer / lit pass / shadow render (consistent index).
		int n = ( r_dlightShadowTestN && r_dlightShadowTestN->integer > 0 ) ? r_dlightShadowTestN->integer : 1;
		int e;
		if ( n > 4 ) n = 4;
		for ( e = 0; e < n && r_numdlights < (int)ARRAY_LEN( backEndData->dlights ); e++ ) {
			dlight_t *dl = &backEndData->dlights[ r_numdlights++ ];
			memset( dl, 0, sizeof( *dl ) );
			if ( n == 1 ) {
				dl->origin[0] = fd->vieworg[0];
				dl->origin[1] = fd->vieworg[1];
			} else {
				float ang = ( (float)e / (float)n ) * 2.0f * (float)M_PI;
				dl->origin[0] = fd->vieworg[0] + cos( ang ) * rad * 0.5f;
				dl->origin[1] = fd->vieworg[1] + sin( ang ) * rad * 0.5f;
			}
			dl->origin[2] = fd->vieworg[2] + 200.0f;   // 200u above the eye
			dl->radius = rad;
			dl->color[0] = 1.0f; dl->color[1] = 1.0f; dl->color[2] = 1.0f;
			dl->linear = qfalse;
		}
		tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	}
#endif

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled
	if ( r_dynamiclight->integer == 0 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	/* Every refdef rectangle is published in presentation pixels.  Keep the GL
	 * top-left -> bottom-left conversion in that same domain; the Vulkan command
	 * path scales the complete rectangle into the independent scene attachment.
	 * Mixing the 720p render height with a 1440p Retina refdef produced a negative
	 * viewport origin and clipped half of the world after vid_restart. */
	parms.viewportY = gls.windowHeight
		- ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	parms.scissorX = parms.viewportX;
	parms.scissorY = parms.viewportY;
	parms.scissorWidth = parms.viewportWidth;
	parms.scissorHeight = parms.viewportHeight;

	parms.portalView = PV_NONE;
	parms.temporalWorldIndex = ( fd->rdflags & RDF_NOWORLDMODEL ) ? -1 : worldIndex;

#ifdef USE_PMLIGHT
	parms.dlights = tr.refdef.dlights;
	parms.num_dlights = tr.refdef.num_dlights;
#endif

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	parms.stereoFrame = tr.refdef.stereoFrame;

	VectorCopy( fd->vieworg, parms.or.origin );
	VectorCopy( fd->viewaxis[0], parms.or.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.or.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.or.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

#ifdef USE_VULKAN
	lastRenderCommand = tr.lastRenderCommand;
	tr.drawSurfCmd = NULL;
	tr.numDrawSurfCmds = 0;
#endif

	R_RenderView( &parms );

#ifdef USE_VULKAN
	if ( tr.needScreenMap )
	{
		if ( lastRenderCommand == RC_DRAW_BUFFER )
		{
			// duplicate all views, including portals
			drawSurfsCommand_t *cmd, *src = NULL;

			for ( int i = 0; i < tr.numDrawSurfCmds; i++ )
			{
				cmd = R_GetCommandBuffer( sizeof( *cmd ) );
				if ( cmd )
				{
					src = tr.drawSurfCmd + i;
					*cmd = *src;
				}
				else
				{
					break;
				}
			}

			if ( src )
			{
				// first drawsurface
				tr.drawSurfCmd[0].refdef.needScreenMap = qtrue;
				// last drawsurface
				src->refdef.switchRenderPass = qtrue;
			}
		}

		tr.needScreenMap = 0;
	}
#endif

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
#endif

	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
