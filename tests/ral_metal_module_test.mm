// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_module.h"
#include "ral_atmosphere_conformance.h"
#include "sdl_ral_presentation.h"
#include "tr_public.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#ifndef RAL_METAL_TEST_MODULE
#error RAL_METAL_TEST_MODULE is required
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
	__FILE__,__LINE__,#x); return 1; } } while (0)

typedef qboolean (*frameReceiptFn_t)( ralMetalModuleFrameReceipt_t *outReceipt );
typedef qboolean (*frameReceiptExactFn_t)(
	const ralMetalModuleFrameReceipt_t *a,
	const ralMetalModuleFrameReceipt_t *b );

static qboolean RejectOpen( void *context,
		const ralPresentationHostOpenInfo_t *info,
		ralPresentationHostReceipt_t *outReceipt ) {
	(void)context; (void)info; (void)outReceipt;
	return qfalse;
}

int main( void ) {
	void *library;
	GetRefAPI_t getRefApi;
	frameReceiptFn_t getFrameReceipt;
	frameReceiptExactFn_t frameReceiptExact;
	refimport_t imports;
	refexport_t *exports;
	wiredSdlRalPresentationHost_t *presentationHost = NULL;
	ralPresentationHostReceipt_t hostAfterKeep;
	ralPresentationHostOpenFn realOpen;
	glconfig_t config;
	atmosphereFrameState_t atmosphere;
	refdef_t atmosphereView;
	particleClass_t breathClass;
	atmosphereEffectProfile_t breathProfile;
	atmosphereEmitter_t breathEmitter;
	atmosphereMediaVolume_t media;
	ralMetalModuleFrameReceipt_t receipt, first, before, exact;
	qhandle_t atmosphereParticleShader;
	int frontEnd = -1, backEnd = -1, i;

	library = dlopen( RAL_METAL_TEST_MODULE, RTLD_NOW | RTLD_LOCAL );
	CHECK( library != NULL );
	getRefApi = (GetRefAPI_t)dlsym( library, "GetRefAPI" );
	getFrameReceipt = (frameReceiptFn_t)dlsym( library, "WiredMetal_GetFrameReceipt" );
	frameReceiptExact = (frameReceiptExactFn_t)dlsym( library,
		"RalMetal_ModuleFrameReceiptExact" );
	CHECK( getRefApi != NULL && getFrameReceipt != NULL && frameReceiptExact != NULL );
	memset( &imports, 0, sizeof( imports ) );
	CHECK( getRefApi( REF_API_VERSION, &imports ) == NULL );
	CHECK( WiredSdlRalPresentationHost_Create( 1280u, 720u, qfalse, 701u,
		&presentationHost, &imports.PresentationHost ) );
	CHECK( getRefApi( REF_API_VERSION - 1, &imports ) == NULL );
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->initFailed == qfalse
		&& exports->CookLightingProject
		&& !exports->CookLightingProject( "/tmp" ) );
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	exports->BeginFrame( STEREO_CENTER );
	CHECK( exports->initFailed == qtrue && !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	exports->Shutdown( REF_UNLOAD_DLL );
	realOpen = imports.PresentationHost.open;
	imports.PresentationHost.open = RejectOpen;
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->initFailed == qfalse );
	memset( &config, 0, sizeof( config ) );
	exports->BeginRegistration( &config );
	CHECK( exports->initFailed == qtrue
		&& !WiredSdlRalPresentationHost_GetReceipt( presentationHost,
			&hostAfterKeep ) );
	exports->Shutdown( REF_UNLOAD_DLL );
	imports.PresentationHost.open = realOpen;
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->Shutdown && exports->BeginRegistration
		&& exports->EndRegistration && exports->BeginFrame && exports->EndFrame
		&& exports->SetAtmosphere && exports->RegisterParticleClass
		&& exports->RegisterAtmosphereEffectProfile && exports->AddAtmosphereEmitter
		&& exports->ClearScene && exports->RegisterShader && exports->GetConfig
		&& exports->GetMemoryBudget && exports->initFailed == qfalse );
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	CHECK( !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	memset( &config, 0, sizeof( config ) );
	exports->BeginRegistration( &config );
	CHECK( exports->initFailed == qfalse );
	CHECK( config.vidWidth > 0 && config.vidHeight > 0 );
	CHECK( strstr( config.renderer_string, "native Metal RAL" ) != NULL );
	CHECK( exports->GetConfig() != NULL );
	atmosphereParticleShader = exports->RegisterShader( "gfx/2d/bigchars" );
	CHECK( exports->RegisterModel( "models/players/visor/lower.md3" ) > 0
		&& exports->RegisterSkin( "models/players/visor/default.skin" ) > 0
		&& atmosphereParticleShader > 0
		&& exports->RegisterModel( "" ) == 0 );
	memset( &breathClass, 0, sizeof( breathClass ) );
	breathClass.shader = atmosphereParticleShader;
	breathClass.emitMode = EMIT_POINT;
	breathClass.scatterShape = SCATTER_SPHERE;
	breathClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	breathClass.scatterMagnitude = 1.5f;
	breathClass.axialSpeed = 8.0f;
	breathClass.cubeJitter = 1.0f;
	breathClass.lifetimeMean = 1.0f;
	breathClass.paletteCount = 1;
	breathClass.colorPalette[0][0] = 0.82f;
	breathClass.colorPalette[0][1] = 0.90f;
	breathClass.colorPalette[0][2] = 1.0f;
	breathClass.colorPalette[0][3] = 0.38f;
	breathClass.sizeStart = 1.0f;
	breathClass.sizeEnd = 8.0f;
	breathClass.drag = 0.8f;
	exports->RegisterParticleClass( 1, &breathClass );
	memset( &breathProfile, 0, sizeof( breathProfile ) );
	breathProfile.schemaVersion = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
	breathProfile.stageCount = 1u;
	breathProfile.maxParticles = 24u;
	breathProfile.seed = 216u;
	breathProfile.duration = 1.0f;
	breathProfile.lodFar = 512.0f;
	breathProfile.boundsRadius = 32.0f;
	breathProfile.stages[0].trigger = ATMOSPHERE_STAGE_CONTINUOUS;
	breathProfile.stages[0].particleClass = 1u;
	breathProfile.stages[0].parentStage = UINT32_MAX;
	breathProfile.stages[0].maxParticles = 24u;
	breathProfile.stages[0].spawnRate = 24.0f;
	breathProfile.stages[0].duration = 1.0f;
	breathProfile.stages[0].lodFar = 512.0f;
	breathProfile.stages[0].boundsRadius = 32.0f;
	breathProfile.stages[0].intensityScale = 1.0f;
	exports->RegisterAtmosphereEffectProfile( 1u, &breathProfile );
	exports->EndRegistration(); exports->BeginFrame( STEREO_CENTER );
	exports->EndFrame( &frontEnd, &backEnd );
	CHECK( exports->initFailed == qfalse && frontEnd == 0 && backEnd == 0 );
	CHECK( getFrameReceipt( &receipt ) );
	CHECK( frameReceiptExact( &receipt, &receipt ) );
	CHECK( receipt.presentation.presented == qtrue );
	CHECK( receipt.frontend.ready == qtrue );
	CHECK( receipt.frontend.registeredAssetCount == 3u );
	CHECK( receipt.frontend.registeredMaterialCount == 1u );
	CHECK( receipt.frame.state == RAL_FRAME_SHELL_PRESENTED );
	CHECK( receipt.atmosphere.requestedTier == RAL_ATMOSPHERE_TIER_OFF
		&& receipt.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_OFF
		&& receipt.atmosphere.fallbackReason == RAL_ATMOSPHERE_FALLBACK_NONE
		&& receipt.atmosphere.zeroWork == qtrue );
	/* Loading-map wireframes can legitimately exceed the portable UI budget.
	 * Bounded content overflow is dropped; it cannot poison module lifecycle. */
	exports->BeginFrame( STEREO_CENTER );
	for ( i = 0; i < 5000; ++i )
		exports->DrawLine( 1.0f, 1.0f, 2.0f, 2.0f, 1.0f,
			atmosphereParticleShader );
	exports->EndFrame( NULL, NULL );
	CHECK( exports->initFailed == qfalse && getFrameReceipt( &receipt ) );
	/* Metal executes the bounded flat-storage FULL froxel path in the same
	 * command buffer before its native world/entity product shader. */
	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED
		| ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.seed = 216u;
	atmosphere.temperatureC = -8.0f;
	atmosphere.humidity = 0.9f;
	atmosphere.indoorExposure = 1.0f;
	atmosphere.mediaDensity = 0.1f;
	atmosphere.cloudCover = 0.65f;
	atmosphere.cloudShadow = 0.4f;
	atmosphere.sunIntensity = 1.0f;
	atmosphere.precipitation[1] = 0.5f;
	atmosphere.precipitation[2] = 0.5f;
	atmosphere.ambientColor[0] = atmosphere.ambientColor[1]
		= atmosphere.ambientColor[2] = 0.2f;
	exports->SetAtmosphere( &atmosphere );
	memset( &atmosphereView, 0, sizeof( atmosphereView ) );
	atmosphereView.width = 1280; atmosphereView.height = 720;
	atmosphereView.fov_x = 90.0f; atmosphereView.fov_y = 60.0f;
	atmosphereView.viewaxis[0][0] = atmosphereView.viewaxis[1][1]
		= atmosphereView.viewaxis[2][2] = 1.0f;
	exports->BeginFrame( STEREO_CENTER );
	memset( &breathEmitter, 0, sizeof( breathEmitter ) );
	breathEmitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	breathEmitter.kind = ATMOSPHERE_EMITTER_BREATH;
	breathEmitter.id = 1u;
	breathEmitter.seed = 216u;
	breathEmitter.profile = 1u;
	breathEmitter.intensity = 1.0f;
	breathEmitter.direction[0] = 1.0f;
	breathEmitter.radius = 8.0f;
	breathEmitter.temperatureC = 37.0f;
	breathEmitter.humidity = 1.0f;
	exports->AddAtmosphereEmitter( &breathEmitter );
	exports->RenderScene( &atmosphereView, 0 );
	exports->EndFrame( NULL, NULL );
	CHECK( exports->initFailed == qfalse && getFrameReceipt( &receipt ) );
	CHECK( receipt.atmosphere.requestedTier == RAL_ATMOSPHERE_TIER_FULL
		&& receipt.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL
		&& receipt.atmosphere.fallbackReason == RAL_ATMOSPHERE_FALLBACK_NONE
		&& receipt.atmosphere.computeDispatchCount == 4u
		&& receipt.atmosphere.froxelCount == 230400u
		&& receipt.atmosphere.cloudsActive == qtrue
		&& receipt.atmosphere.compositeCount == 1u
		&& receipt.presentation.atmosphereDispatchCount == 4u
		&& receipt.presentation.atmosphereFroxelCount == 230400u
		&& receipt.presentation.atmosphereCompositeCount == 1u
		&& receipt.presentation.atmosphereCloudsActive == qtrue );
	CHECK( receipt.presentation.weather.familyMask
			== ( ( 1u << 1 ) | ( 1u << 2 ) )
		&& receipt.presentation.weather.activeParticleCount == 8192u
		&& receipt.presentation.weather.precipitationParticleCount == 8168u
		&& receipt.presentation.weather.semanticEmitterCount == 1u
		&& receipt.presentation.weather.semanticParticleCount == 24u
		&& receipt.presentation.weather.droppedParticleCount == 24u
		&& receipt.presentation.weatherDispatchCount == 1u
		&& receipt.presentation.weatherDrawCount == 1u );
	CHECK( receipt.atmosphere.zeroWork == qfalse );
	for ( int fixtureIndex = RAL_ATMOSPHERE_FIXTURE_CLEAR;
			fixtureIndex < RAL_ATMOSPHERE_FIXTURE_COUNT; ++fixtureIndex ) {
		ralAtmosphereFixtureState_t authored;
		CHECK( Ral_AtmosphereConformanceFixture(
			(ralAtmosphereFixture_t)fixtureIndex, &authored ) );
		authored.state.timelineSeconds = (float)fixtureIndex;
		exports->SetAtmosphere( &authored.state );
		exports->BeginFrame( STEREO_CENTER );
		exports->ClearScene();
		if ( authored.breathEmitter ) {
			Ral_AtmosphereConformanceBreathEmitter( 1u, &breathEmitter );
			exports->AddAtmosphereEmitter( &breathEmitter );
		}
		if ( authored.localMedia ) {
			Ral_AtmosphereConformanceLocalMedia( &media );
			exports->AddAtmosphereMediaVolume( &media );
		}
		exports->RenderScene( &atmosphereView, 0 );
		exports->EndFrame( NULL, NULL );
		CHECK( exports->initFailed == qfalse && getFrameReceipt( &receipt ) &&
			receipt.atmosphere.requestedTier ==
				(ralAtmosphereTier_t)authored.state.qualityTier &&
			receipt.atmosphere.selectedTier ==
				(ralAtmosphereTier_t)authored.state.qualityTier &&
			receipt.atmosphere.fallbackReason ==
				RAL_ATMOSPHERE_FALLBACK_NONE &&
			receipt.presentation.weather.familyMask == authored.familyMask );
		if ( authored.breathEmitter )
			CHECK( receipt.presentation.weather.semanticEmitterCount == 1u &&
				receipt.presentation.weather.semanticParticleCount == 24u );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_CLEAR ||
				fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
			CHECK( receipt.presentation.weather.zeroWork );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
			CHECK( receipt.presentation.atmosphereDispatchCount == 3u &&
				receipt.presentation.atmosphereFroxelCount == 230400u &&
				receipt.presentation.atmosphereCompositeCount == 1u );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_STORM )
			CHECK( receipt.presentation.atmosphereDispatchCount == 4u &&
				receipt.presentation.atmosphereCloudsActive &&
				receipt.presentation.weather.activeParticleCount == 8192u );
	}
	/* Cross the 16-slot backend command-identity ring and prove completed
	 * product frames recycle identities by generation rather than saturating. */
	for ( i = 0; i < 24; ++i ) {
		exact = receipt;
		exports->BeginFrame( STEREO_CENTER ); exports->EndFrame( NULL, NULL );
		CHECK( exports->initFailed == qfalse && getFrameReceipt( &receipt )
			&& receipt.frameGeneration > exact.frameGeneration );
	}
	first = receipt; exact = receipt;
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !frameReceiptExact( &receipt, &exact ) ); } while (0)
	MUTATE( moduleGeneration ); MUTATE( frameGeneration );
	MUTATE( frame.ownerGeneration ); MUTATE( host.surfaceGeneration );
	MUTATE( surface.surfaceIdentity );
	MUTATE( drawable.acquireGeneration ); MUTATE( presentation.presentGeneration );
	MUTATE( frontend.frameDigest );
	MUTATE( atmosphere.fallbackReason );
#undef MUTATE

	exports->Shutdown( REF_LEVEL_ONLY );
	/* The engine draws its loading compositor after level teardown and before
	 * the next BeginRegistration.  REF_LEVEL_ONLY must preserve that frame
	 * authority rather than tripping recoverable renderer fallback. */
	exports->BeginFrame( STEREO_CENTER ); exports->EndFrame( NULL, NULL );
	CHECK( exports->initFailed == qfalse && getFrameReceipt( &exact )
		&& exact.frameGeneration > first.frameGeneration
		&& exact.moduleGeneration == first.moduleGeneration );
	CHECK( WiredSdlRalPresentationHost_RequestResize( presentationHost,
		800u, 450u ) );
	memset( &config, 0, sizeof( config ) ); exports->BeginRegistration( &config );
	exports->BeginFrame( STEREO_CENTER ); exports->EndFrame( NULL, NULL );
	CHECK( getFrameReceipt( &receipt ) && receipt.frameGeneration > exact.frameGeneration
		&& receipt.moduleGeneration == first.moduleGeneration
		&& receipt.host.surfaceGeneration > first.host.surfaceGeneration
		&& receipt.host.logicalWidth == 800u && receipt.host.logicalHeight == 450u
		&& receipt.surface.surfaceIdentity == first.surface.surfaceIdentity );
	exports->Shutdown( REF_KEEP_WINDOW );
	CHECK( WiredSdlRalPresentationHost_GetReceipt( presentationHost,
		&hostAfterKeep )
		&& hostAfterKeep.ownerIdentity == receipt.host.ownerIdentity );
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->initFailed == qfalse );
	memset( &config, 0, sizeof( config ) ); exports->BeginRegistration( &config );
	exports->BeginFrame( STEREO_CENTER ); exports->EndFrame( NULL, NULL );
	CHECK( getFrameReceipt( &exact )
		&& exact.moduleGeneration > receipt.moduleGeneration
		&& exact.host.ownerIdentity == receipt.host.ownerIdentity
		&& exact.surface.surfaceIdentity == receipt.surface.surfaceIdentity );
	exports->EndFrame( NULL, NULL );
	CHECK( exports->initFailed == qtrue );
	receipt = exact;
	CHECK( getFrameReceipt( &exact ) && frameReceiptExact( &receipt, &exact ) );
	exports->Shutdown( REF_UNLOAD_DLL );
	before = receipt;
	CHECK( !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	CHECK( !WiredSdlRalPresentationHost_GetReceipt( presentationHost,
		&hostAfterKeep ) );
	CHECK( dlclose( library ) == 0 );
	WiredSdlRalPresentationHost_Destroy( presentationHost );
	puts( "wired Metal renderer module: PASS" );
	return 0;
}
