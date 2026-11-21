// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Renderer test fixtures. See vk_fixtures.h for the contract; everything here is
// default-off, CVAR_CHEAT and removable with -DWIRED_RENDERER_FIXTURES=OFF.

#include "vk_local.h"
#include "../../../../frontend/r_log.h"
#include "../../../../frontend/render_submission.h"
#include "../../../../frontend/render_submission_effects.h"
#include "../../../core/ral_atmosphere_conformance.h"
#include "vk_fixtures.h"

#if WIRED_RENDERER_FIXTURES

R_LOG_DECLARE_CHANNEL( rch_init, "renderer.init" );

cvar_t *r_ralEffectsSmoke;
cvar_t *r_lightingReferenceFixture;
cvar_t *r_emissiveAuthorityFixture;

void wired_renderer_fixtures_register_cvars( void )
{
	r_ralEffectsSmoke = ri.Cvar_Get( "r_ralEffectsSmoke", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_ralEffectsSmoke, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ralEffectsSmoke, "Default-off native RAL procedural-effects smoke. Injects one ribbon, "
											   "rail ribbon, beam and sprite on the first primary view of each map; "
											   "renderer.ral emits a receipt only after the corresponding RAL draw." );
	r_lightingReferenceFixture = ri.Cvar_Get( "r_lightingReferenceFixture", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_lightingReferenceFixture, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_lightingReferenceFixture,
		"Test-only canonical lighting-reference atmosphere: stable cool participating media, "
		"no precipitation or particle injection. 0 = off, 1 = on." );
	r_emissiveAuthorityFixture = ri.Cvar_Get( "r_emissiveAuthorityFixture", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_emissiveAuthorityFixture,
		"Test-only typed stationary emissive authority for the canonical lava-sanctum visual gate." );
}

// ---- scene fixtures: public RE_Add*ToScene contract only ------------------
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

void wired_renderer_fixtures_inject_scene( const refdef_t *fd )
{
#ifdef USE_VULKAN
	R_InjectRalEffectsSmoke( fd );
#else
	(void)fd;
#endif
}

// ---- frontend fixtures: through the frontend submission seam ---------------
#ifdef USE_VULKAN
static char	 s_ralAtmosphereGraphSmokeMap[MAX_QPATH];
static float s_ralAtmosphereSmokeHeightgrid[256u * 256u];

// Test-only authored atmosphere fixture. It deliberately crosses the same
// frontend contract as game content: one cold climate state, one immutable
// breath graph and one semantic emitter. Particle instances, collision/death
// child expansion, simulation and bounded overflow remain GPU-owned.
static void FrontendInjectRalAtmosphereGraphSmoke( const refdef_t *view )
{
	atmosphericDesc_t		  atmosphere;
	atmosphereEffectProfile_t profile;
	atmosphereEmitter_t		  emitter;
	atmosphereMediaVolume_t	  mediaVolume;
	particleClass_t			  cls;
	qhandle_t				  shader;
	vec3_t					  center;
	uint32_t				  requestsBefore, particlesBefore;

	if ( !WIRED_FIXTURE_EFFECTS_SMOKE() || !tr.world || !view ||
		 ( view->rdflags & RDF_NOWORLDMODEL ) || !vk.atm.available || !vk.particle.available )
		return;
	if ( !Q_stricmp( s_ralAtmosphereGraphSmokeMap, tr.world->baseName ) )
		return;

	shader = FrontendRegisterPrimitive( "gfx/misc/particle" );
	if ( shader == 0 )
		return;
	VectorMA( view->vieworg, 72.0f, view->viewaxis[0], center );

	memset( &cls, 0, sizeof( cls ) );
	cls.shader			 = shader;
	cls.renderFlags		 = PRIM_FLAG_CAMERA_FACING;
	cls.emitMode		 = EMIT_POINT;
	cls.scatterShape	 = SCATTER_SPHERE;
	cls.scatterMagnitude = 1.5f;
	cls.velocityShape	 = VEL_CONE;
	cls.axialSpeed		 = 10.0f;
	cls.coneHalfAngle	 = 0.3f;
	cls.lifetimeMean	 = 0.75f;
	cls.lifetimeJitter	 = 0.15f;
	cls.paletteCount	 = 1;
	Vector4Set( cls.colorPalette[0], 0.82f, 0.9f, 1.0f, 0.55f );
	Vector4Set( cls.colorEndMult, 1.0f, 1.0f, 1.0f, 0.0f );
	cls.sizeStart = 2.5f;
	cls.sizeEnd	  = 8.0f;
	cls.drag	  = 1.5f;
	RE_RegisterParticleClass( MAX_PARTICLE_CLASSES, &cls );

	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.type			 = ATMOSPHERE_PRECIP_SNOW;
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags		 = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_HEIGHTGRID | ATMOSPHERE_FLAG_INDOOR_EXPOSURE |
					   ATMOSPHERE_FLAG_SURFACE_CLIMATE | ATMOSPHERE_FLAG_SKY_LIGHTING |
					   ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
	atmosphere.qualityTier	= ATMOSPHERE_QUALITY_FULL;
	atmosphere.seed			= 0x52414c41u;
	atmosphere.temperatureC = -8.0f;
	atmosphere.humidity		= 0.9f;
	// Synthetic product matrix: all five portable weather families execute in
	// one bounded pool at 75% outdoor exposure. The split heightgrid below puts
	// a roof over half the authored volume while leaving the other half open.
	atmosphere.indoorExposure = 0.75f;
	for ( uint32_t i = 0u; i < 5u; ++i )
		atmosphere.precipitation[i] = 0.2f;
	atmosphere.visibility		= 768.0f;
	atmosphere.surfaceWetness	= 0.2f;
	atmosphere.surfaceFrost		= 0.7f;
	atmosphere.snowAccumulation = 0.5f;
	atmosphere.sunDirection[2]	= 1.0f;
	atmosphere.sunIntensity		= 1.0f;
	VectorSet( atmosphere.ambientColor, 0.2f, 0.25f, 0.32f );
	VectorSet( atmosphere.wind, 18.0f, 7.0f, 0.0f );
	atmosphere.cloudCover		  = 0.65f;
	atmosphere.cloudShadow		  = 0.35f;
	atmosphere.mediaDensity		  = 0.02f;
	atmosphere.mediaHeightFalloff = 0.01f;
	atmosphere.distance			  = 512.0f;
	atmosphere.bounds[0]		  = center[0] - 128.0f;
	atmosphere.bounds[1]		  = center[1] - 128.0f;
	atmosphere.bounds[2]		  = center[2] - 128.0f;
	atmosphere.bounds[3]		  = center[0] + 128.0f;
	atmosphere.bounds[4]		  = center[1] + 128.0f;
	atmosphere.bounds[5]		  = center[2] + 128.0f;
	atmosphere.worldMins[0]		  = atmosphere.bounds[0];
	atmosphere.worldMins[1]		  = atmosphere.bounds[1];
	atmosphere.worldMaxs[0]		  = atmosphere.bounds[3];
	atmosphere.worldMaxs[1]		  = atmosphere.bounds[4];
	atmosphere.gridSize			  = 256;
	if ( !FrontendTrySetAtmosphere( &atmosphere ) )
		return;
	for ( uint32_t y = 0u; y < 256u; ++y ) {
		for ( uint32_t x = 0u; x < 256u; ++x ) {
			s_ralAtmosphereSmokeHeightgrid[y * 256u + x] = x < 128u ? center[2] + 300.0f : center[2] - 128.0f;
		}
	}
	RE_SetAtmosphereHeightgrid( s_ralAtmosphereSmokeHeightgrid, (int)( 256u * 256u ) );

	memset( &mediaVolume, 0, sizeof( mediaVolume ) );
	mediaVolume.schemaVersion = WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION;
	mediaVolume.shape		  = ATMOSPHERE_MEDIA_VOLUME_SPHERE;
	mediaVolume.id			  = 0x4d495354u;
	mediaVolume.flags		  = ATMOSPHERE_MEDIA_VOLUME_NOISE;
	mediaVolume.priority	  = 100u;
	mediaVolume.seed		  = 0x464f4721u;
	VectorCopy( center, mediaVolume.origin );
	mediaVolume.radius = 96.0f;
	VectorSet( mediaVolume.albedo, 0.72f, 0.82f, 0.92f );
	mediaVolume.extinction	  = 0.035f;
	mediaVolume.anisotropy	  = 0.25f;
	mediaVolume.heightFalloff = 0.004f;
	mediaVolume.noiseScale	  = 48.0f;
	mediaVolume.timelineStart = atmosphere.timelineSeconds;
	if ( !FrontendTryAddAtmosphereMediaVolume( &mediaVolume ) )
		return;

	memset( &profile, 0, sizeof( profile ) );
	profile.schemaVersion			 = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
	profile.stageCount				 = 3u;
	profile.maxParticles			 = 24u;
	profile.seed					 = 0x42524541u;
	profile.duration				 = 1.0f;
	profile.lodFar					 = 512.0f;
	profile.boundsRadius			 = 64.0f;
	profile.stages[0].trigger		 = ATMOSPHERE_STAGE_EMISSION;
	profile.stages[0].particleClass	 = MAX_PARTICLE_CLASSES;
	profile.stages[0].parentStage	 = UINT32_MAX;
	profile.stages[0].maxParticles	 = 8u;
	profile.stages[0].burstCount	 = 4u;
	profile.stages[0].spawnRate		 = 4.0f;
	profile.stages[0].duration		 = 1.0f;
	profile.stages[0].lodFar		 = 512.0f;
	profile.stages[0].boundsRadius	 = 32.0f;
	profile.stages[0].intensityScale = 1.0f;
	profile.stages[1]				 = profile.stages[0];
	profile.stages[1].trigger		 = ATMOSPHERE_STAGE_COLLISION;
	profile.stages[1].parentStage	 = 0u;
	profile.stages[1].flags			 = ATMOSPHERE_STAGE_INHERIT_POSITION | ATMOSPHERE_STAGE_INHERIT_VELOCITY |
							  ATMOSPHERE_STAGE_INHERIT_COLOR | ATMOSPHERE_STAGE_INHERIT_INTENSITY;
	profile.stages[1].burstCount = 2u;
	profile.stages[1].spawnRate	 = 0.0f;
	profile.stages[2]			 = profile.stages[1];
	profile.stages[2].trigger	 = ATMOSPHERE_STAGE_DEATH;
	if ( !FrontendTryRegisterAtmosphereEffectProfile( ATMOSPHERE_EFFECT_MAX_PROFILES, &profile ) )
		return;

	memset( &emitter, 0, sizeof( emitter ) );
	emitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	emitter.kind		  = ATMOSPHERE_EMITTER_BREATH;
	emitter.id			  = 0x42524541u;
	emitter.seed		  = 0x544831u;
	emitter.profile		  = ATMOSPHERE_EFFECT_MAX_PROFILES;
	VectorCopy( center, emitter.origin );
	VectorCopy( view->viewaxis[0], emitter.direction );
	emitter.intensity	 = 1.0f;
	emitter.radius		 = 6.0f;
	emitter.temperatureC = 37.0f;
	emitter.humidity	 = 0.9f;
	emitter.lifetime	 = 1.0f;
	requestsBefore		 = vk.particle.spawnRequestCount;
	particlesBefore		 = vk.particle.spawnParticleCount;
	if ( !FrontendTryAddAtmosphereEmitter( &emitter ) || vk.particle.spawnRequestCount <= requestsBefore ||
		 vk.particle.spawnParticleCount <= particlesBefore )
		return;

	vk.particle.atmosphereGraphSmokeRequests  = vk.particle.spawnRequestCount - requestsBefore;
	vk.particle.atmosphereGraphSmokeParticles = vk.particle.spawnParticleCount - particlesBefore;
	vk.particle.atmosphereGraphSmokePending	  = qtrue;
	Q_strncpyz( s_ralAtmosphereGraphSmokeMap, tr.world->baseName, sizeof( s_ralAtmosphereGraphSmokeMap ) );
}

static void FrontendInjectRalAtmosphereFixtureSmoke( const refdef_t *view )
{
	static char mapName[MAX_QPATH];
	static int lastViewTime = -1;
	static uint32_t authoredFrame;
	static qboolean warmup;
	ralAtmosphereFixtureState_t authored;
	atmosphereEmitter_t emitter;
	atmosphereMediaVolume_t media;
	vec3_t center;
	uint32_t fixtureIndex;
	if ( !WIRED_FIXTURE_EFFECTS_SMOKE() || !tr.world ||
			!view || ( view->rdflags & RDF_NOWORLDMODEL ) ) return;
	if ( Q_stricmp( mapName, tr.world->baseName ) ) {
		Q_strncpyz( mapName, tr.world->baseName, sizeof( mapName ) );
		authoredFrame = 0u;
		lastViewTime = -1;
		warmup = qtrue;
	}
	if ( view->time == lastViewTime ) return;
	lastViewTime = view->time;
	if ( warmup ) {
		warmup = qfalse;
		return;
	}
	fixtureIndex = authoredFrame / 4u;
	if ( fixtureIndex >= RAL_ATMOSPHERE_FIXTURE_COUNT )
		fixtureIndex = RAL_ATMOSPHERE_FIXTURE_COUNT - 1u;
	if ( !Ral_AtmosphereConformanceFixture(
			(ralAtmosphereFixture_t)fixtureIndex, &authored ) ) return;
	VectorMA( view->vieworg, 72.0f, view->viewaxis[0], center );
	for ( uint32_t i = 0u; i < 3u; ++i ) {
		authored.state.bounds[i] += center[i];
		authored.state.bounds[i + 3u] += center[i];
	}
	if ( authored.familyMask ) {
		authored.state.flags |= ATMOSPHERE_FLAG_HEIGHTGRID;
		authored.state.gridSize = 256;
		authored.state.worldMins[0] = authored.state.bounds[0];
		authored.state.worldMins[1] = authored.state.bounds[1];
		authored.state.worldMaxs[0] = authored.state.bounds[3];
		authored.state.worldMaxs[1] = authored.state.bounds[4];
	}
	if ( !FrontendTrySetAtmosphere( &authored.state ) ) return;
	if ( authored.localMedia ) {
		Ral_AtmosphereConformanceLocalMedia( &media );
		VectorCopy( center, media.origin );
		FrontendAddAtmosphereMediaVolume( &media );
	}
	if ( authored.breathEmitter ) {
		Ral_AtmosphereConformanceBreathEmitter(
			ATMOSPHERE_EFFECT_MAX_PROFILES, &emitter );
		VectorCopy( center, emitter.origin );
		VectorCopy( view->viewaxis[0], emitter.direction );
		FrontendAddAtmosphereEmitter( &emitter );
	}
	authoredFrame++;
}

static void FrontendInjectLightingReferenceFixture( const refdef_t *view )
{
	static char injectedMap[MAX_QPATH];
	atmosphericDesc_t atmosphere;
	atmosphereMediaVolume_t media;
	vec3_t center;
	if ( !r_lightingReferenceFixture || !r_lightingReferenceFixture->integer || !tr.world || !view ||
		 ( view->rdflags & RDF_NOWORLDMODEL ) || !Q_stricmp( injectedMap, tr.world->baseName ) ) return;
	VectorMA( view->vieworg, 160.0f, view->viewaxis[0], center );
	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.type = ATMOSPHERE_PRECIP_NONE;
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SKY_LIGHTING |
		ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.seed = 0x4c415641u;
	atmosphere.temperatureC = 18.0f;
	atmosphere.humidity = 0.35f;
	atmosphere.visibility = 1400.0f;
	atmosphere.distance = 1400.0f;
	atmosphere.sunDirection[2] = 1.0f;
	atmosphere.sunIntensity = 0.2f;
	VectorSet( atmosphere.ambientColor, 0.035f, 0.055f, 0.10f );
	atmosphere.mediaDensity = 0.0008f;
	atmosphere.mediaHeightFalloff = 0.001f;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		atmosphere.bounds[axis] = center[axis] - 512.0f;
		atmosphere.bounds[axis + 3u] = center[axis] + 512.0f;
	}
	if ( !FrontendTrySetAtmosphere( &atmosphere ) ) return;
	memset( &media, 0, sizeof( media ) );
	media.schemaVersion = WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION;
	media.shape = ATMOSPHERE_MEDIA_VOLUME_SPHERE;
	media.id = 0x4c415641u;
	media.flags = ATMOSPHERE_MEDIA_VOLUME_NOISE;
	media.priority = 100u;
	media.seed = 0x434f4f4cu;
	VectorCopy( center, media.origin );
	media.radius = 320.0f;
	VectorSet( media.albedo, 0.30f, 0.42f, 0.70f );
	VectorSet( media.emissive, 0.004f, 0.008f, 0.018f );
	media.extinction = 0.0035f;
	media.emissionIntensity = 1.0f;
	media.anisotropy = 0.25f;
	media.heightFalloff = 0.001f;
	media.noiseScale = 96.0f;
	FrontendAddAtmosphereMediaVolume( &media );
	Q_strncpyz( injectedMap, tr.world->baseName, sizeof( injectedMap ) );
}

static void FrontendInjectEmissiveAuthorityFixture( void )
{
	renderSubmissionState_t *const submission = wired_renderer_frontend_submission();
	static char injectedMap[MAX_QPATH];
	static qboolean injectedUnnamedWorld;
	static qboolean receiptLogged;
	renderMaterialLighting_t lighting;
	qhandle_t material;
	if ( !r_emissiveAuthorityFixture || !r_emissiveAuthorityFixture->integer ||
		 !tr.world ) return;
	if ( tr.world->baseName[0] ?
		 ( injectedMap[0] && !Q_stricmp( injectedMap, tr.world->baseName ) ) : injectedUnnamedWorld ) {
		const ralEmissiveRouteReceipt_t *routes;
		const ralLightDescription_t *proxies;
		const renderEmissiveRoutingReceipt_t *receipt;
		uint32_t routeCount, proxyCount;
		if ( !receiptLogged && RenderSubmission_EmissiveAuthoritySnapshot( submission,
				&routes, &routeCount, &proxies, &proxyCount, &receipt ) && routeCount ) {
			R_LOG( rch_init, SEV_INFO,
				"emissive-authority-fixture schema=1 routes=%u proxies=%u rejected=%u digest=%llu\n",
				routeCount, proxyCount, receipt->rejectedProxyRouteCount,
				(unsigned long long)receipt->routeDigest );
			receiptLogged = qtrue;
		}
		return;
	}
	receiptLogged = qfalse;
	material = 0;
	for ( uint32_t batchIndex = 0u;
		 batchIndex < submission->worldSnapshot.batchCount; ++batchIndex ) {
		const qhandle_t candidate = submission->worldSnapshot.batches[batchIndex].baseMaterial;
		for ( uint32_t materialIndex = 0u;
			 materialIndex < submission->materialCount; ++materialIndex )
			if ( submission->materials[materialIndex].snapshot.handle == candidate &&
				strstr( submission->materials[materialIndex].name, "lava" ) ) {
				material = candidate;
				break;
			}
		if ( material ) break;
	}
	if ( !material ) {
		R_LOG( rch_init, SEV_WARN,
			"emissive-authority-fixture rejected reason=material-missing materials=%u\n",
			submission->materialCount );
		return;
	}
	memset( &lighting, 0, sizeof( lighting ) );
	lighting.schemaVersion = RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION;
	lighting.sourceGeneration = (uint64_t)(uint32_t)tr.world->dataSize + 1u;
	lighting.provenanceHash = UINT64_C( 0x6c61766173616e63 );
	lighting.diffuseReflectanceQ16[0] = RAL_LIGHT_Q16_ONE / 2;
	lighting.diffuseReflectanceQ16[1] = RAL_LIGHT_Q16_ONE / 4;
	lighting.diffuseReflectanceQ16[2] = RAL_LIGHT_Q16_ONE / 8;
	lighting.emissionRadianceQ16[0] = RAL_LIGHT_Q16_ONE * 6;
	lighting.emissionRadianceQ16[1] = RAL_LIGHT_Q16_ONE * 2;
	lighting.emissionRadianceQ16[2] = RAL_LIGHT_Q16_ONE / 2;
	lighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIONARY;
	lighting.emissiveInfluenceRangeQ16 = RAL_LIGHT_Q16_ONE * 192;
	lighting.emissiveShadowPriority = 8u;
	lighting.emissiveRequestedProxyCount = 4u;
	lighting.participatesInStaticBake = qtrue;
	lighting.emissiveInjectsAtmosphere = qtrue;
	lighting.ready = qtrue;
	if ( RenderSubmission_SetMaterialLighting( submission, material, &lighting ) ) {
		Q_strncpyz( injectedMap, tr.world->baseName, sizeof( injectedMap ) );
		injectedUnnamedWorld = tr.world->baseName[0] ? qfalse : qtrue;
	} else R_LOG( rch_init, SEV_WARN,
		"emissive-authority-fixture rejected reason=material-lighting material=%d world=%d\n",
		material, submission->worldLoaded );
}

#endif

void wired_renderer_fixtures_inject_frontend( const refdef_t *view )
{
#ifdef USE_VULKAN
	FrontendInjectRalAtmosphereGraphSmoke( view );
	FrontendInjectRalAtmosphereFixtureSmoke( view );
	FrontendInjectLightingReferenceFixture( view );
	FrontendInjectEmissiveAuthorityFixture();
#else
	(void)view;
#endif
}

#endif // WIRED_RENDERER_FIXTURES
