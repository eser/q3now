// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_atmosphere.h"
#include "ral_atmosphere_conformance.h"

#include <stdio.h>
#include <string.h>

#define CHECK( x )                                                         \
	do {                                                                   \
		if ( !( x ) ) {                                                    \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); \
			return 1;                                                      \
		}                                                                  \
	} while ( 0 )

static ralAtmospherePlanRequest_t Fixture( ralBackendType_t backend, ralAtmosphereTier_t tier )
{
	ralAtmospherePlanRequest_t request;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion				   = RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType					   = backend;
	request.frameGeneration				   = (uint64_t)backend + 1u;
	request.width						   = 1280u;
	request.height						   = 720u;
	request.requestedTier				   = tier;
	request.maxFroxelCount				   = 262144u;
	request.maxLocalVolumes				   = 32u;
	request.maxLights					   = 128u;
	request.maxShadowedLights			   = 8u;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute		   = qtrue;
	request.capabilities.storageBuffers	   = qtrue;
	request.capabilities.temporalHistory   = qtrue;
	request.capabilities.volumetricShadows = qtrue;
	request.capabilities.fullClouds		   = qfalse;
	return request;
}

int main( void )
{
	{
		ralAtmosphereWeatherRequest_t weather;
		ralAtmosphereWeatherReceipt_t weatherReceipt, weatherExact;
		memset( &weather, 0, sizeof( weather ) );
		weather.schemaVersion			   = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
		weather.tier					   = RAL_ATMOSPHERE_TIER_WEATHER;
		weather.maxParticles			   = 8192u;
		weather.enabled					   = qtrue;
		weather.heightgridRequested		   = qtrue;
		weather.heightgridAvailable		   = qtrue;
		weather.depthIntersectionRequested = qtrue;
		weather.depthIntersectionAvailable = qtrue;
		weather.indoorExposure			   = 0.75f;
		for ( uint32_t i = 0u; i < 5u; ++i )
			weather.precipitation[i] = 0.2f;
		CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) &&
			   weatherReceipt.familyMask == RAL_ATMOSPHERE_WEATHER_ALL && weatherReceipt.activeParticleCount == 6144u &&
			   weatherReceipt.authoredIntensity == 1.0f && weatherReceipt.admittedCoverage == 0.75f &&
			   weatherReceipt.heightgridActive && weatherReceipt.depthIntersectionActive && !weatherReceipt.zeroWork );
		weatherExact = weatherReceipt;
		CHECK( Ral_AtmosphereWeatherReceiptExact( &weatherReceipt, &weatherExact ) );
		weather.indoorExposure = 0.0f;
		CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) && weatherReceipt.zeroWork &&
			   weatherReceipt.activeParticleCount == 0u && !weatherReceipt.heightgridActive &&
			   !weatherReceipt.depthIntersectionActive );
		weather.indoorExposure = 1.0f;
		weather.tier		   = RAL_ATMOSPHERE_TIER_ANALYTIC;
		CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) && weatherReceipt.zeroWork );
		weather.tier					   = RAL_ATMOSPHERE_TIER_WEATHER;
		weather.heightgridAvailable		   = qfalse;
		weather.depthIntersectionAvailable = qfalse;
		CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) && weatherReceipt.activeParticleCount == 8192u &&
			   !weatherReceipt.heightgridActive && !weatherReceipt.depthIntersectionActive );
		for ( uint32_t i = 0u; i < 5u; ++i )
			weather.precipitation[i] = 0.0f;
		weather.semanticEmitterCount  = 2u;
		weather.semanticParticleCount = 64u;
		CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) && weatherReceipt.familyMask == 0u &&
			   weatherReceipt.activeParticleCount == 64u && weatherReceipt.precipitationParticleCount == 0u &&
			   weatherReceipt.semanticEmitterCount == 2u && weatherReceipt.semanticParticleCount == 64u &&
			   weatherReceipt.droppedParticleCount == 0u && !weatherReceipt.zeroWork );
		for ( uint32_t i = 0u; i < 5u; ++i )
			weather.precipitation[i] = 0.2f;
		CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) && weatherReceipt.activeParticleCount == 8192u &&
			   weatherReceipt.precipitationParticleCount == 8128u && weatherReceipt.semanticParticleCount == 64u &&
			   weatherReceipt.droppedParticleCount == 64u );
		weather.precipitation[4] = 2.0f;
		CHECK( !Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) );
	}
	{
		ralAtmosphereSurfaceTile_t		   source[RAL_ATMOSPHERE_MAX_SURFACE_TILES];
		ralAtmosphereSurfaceTile_t		   table[RAL_ATMOSPHERE_MAX_SURFACE_TILES];
		ralAtmosphereSurfaceTableReceipt_t tableReceipt;
		memset( source, 0, sizeof( source ) );
		for ( uint32_t i = 0u; i < RAL_ATMOSPHERE_MAX_SURFACE_TILES; ++i ) {
			source[i].tileX		 = (int32_t)( 127u - i );
			source[i].tileY		 = (int32_t)( i % 7u ) - 3;
			source[i].eventMask	 = 1u << ( i % 3u );
			source[i].climate[0] = (float)i / 255.0f;
			source[i].climate[1] = 0.25f;
			source[i].climate[2] = 0.5f;
			source[i].climate[3] = 0.75f;
		}
		CHECK( sizeof( ralAtmosphereSurfaceTile_t ) == 32u );
		CHECK( Ral_AtmosphereBuildSurfaceTable( source, RAL_ATMOSPHERE_MAX_SURFACE_TILES, table, &tableReceipt ) &&
			   tableReceipt.sourceCount == RAL_ATMOSPHERE_MAX_SURFACE_TILES &&
			   tableReceipt.admittedCount == RAL_ATMOSPHERE_MAX_SURFACE_TILES && tableReceipt.comparisonLimit == 8u &&
			   tableReceipt.byteCount == sizeof( table ) );
		for ( uint32_t i = 1u; i < RAL_ATMOSPHERE_MAX_SURFACE_TILES; ++i )
			CHECK( table[i - 1u].tileX < table[i].tileX ||
				   ( table[i - 1u].tileX == table[i].tileX && table[i - 1u].tileY < table[i].tileY ) );
		source[1] = source[0];
		CHECK( !Ral_AtmosphereBuildSurfaceTable( source, 2u, table, &tableReceipt ) );
		source[1].tileX++;
		source[1].climate[0] = 2.0f;
		CHECK( !Ral_AtmosphereBuildSurfaceTable( source, 2u, table, &tableReceipt ) );
		memset( table, 0xff, sizeof( table ) );
		CHECK( Ral_AtmosphereBuildSurfaceTable( NULL, 0u, table, &tableReceipt ) && tableReceipt.admittedCount == 0u &&
			   table[0].tileX == 0 && table[0].climate[0] == 0.0f );
	}
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend ) {
		ralAtmospherePlanReceipt_t	  receipt, exact;
		ralAtmosphereRuntimeState_t	  runtime;
		ralAtmosphereRuntimeState_t	  runtimeExact;
		ralAtmosphereRuntimeReceipt_t lifecycle, lifecycleExact;
		ralAtmosphereRuntimeRequest_t event;
		ralAtmospherePlanRequest_t	  request = Fixture( (ralBackendType_t)backend, RAL_ATMOSPHERE_TIER_OFF );
		for ( int fixtureIndex = RAL_ATMOSPHERE_FIXTURE_CLEAR;
				fixtureIndex < RAL_ATMOSPHERE_FIXTURE_COUNT; ++fixtureIndex ) {
			ralAtmosphereFixtureState_t authored;
			ralAtmosphereWeatherRequest_t weather;
			ralAtmosphereWeatherReceipt_t weatherReceipt;
			CHECK( Ral_AtmosphereConformanceFixture(
				(ralAtmosphereFixture_t)fixtureIndex, &authored ) );
			request = Fixture( (ralBackendType_t)backend,
				(ralAtmosphereTier_t)authored.state.qualityTier );
			request.mediaActive = authored.localMedia ||
				authored.state.mediaDensity > 0.0f || authored.state.visibility > 0.0f;
			request.localVolumeCount = authored.localMedia ? 1u : 0u;
			request.cloudsRequested = authored.state.cloudCover > 0.0f;
			request.capabilities.fullClouds = qtrue;
			CHECK( Ral_AtmospherePlan( &request, &receipt ) &&
				receipt.requestedTier == (ralAtmosphereTier_t)authored.state.qualityTier &&
				receipt.selectedTier == (ralAtmosphereTier_t)authored.state.qualityTier &&
				receipt.fallbackReason == RAL_ATMOSPHERE_FALLBACK_NONE );
			memset( &weather, 0, sizeof( weather ) );
			weather.schemaVersion = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
			weather.tier = receipt.selectedTier;
			weather.maxParticles = 8192u;
			weather.enabled = qtrue;
			weather.indoorExposure = authored.state.indoorExposure;
			memcpy( weather.precipitation, authored.state.precipitation,
				sizeof( weather.precipitation ) );
			if ( authored.breathEmitter ) {
				weather.semanticEmitterCount = 1u;
				weather.semanticParticleCount = 24u;
			}
			CHECK( Ral_AtmospherePlanWeather( &weather, &weatherReceipt ) &&
				weatherReceipt.familyMask == authored.familyMask );
			if ( authored.breathEmitter )
				CHECK( weatherReceipt.semanticEmitterCount == 1u &&
					weatherReceipt.semanticParticleCount == 24u );
			if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_CLEAR ||
					fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
				CHECK( weatherReceipt.zeroWork );
			if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_STORM )
				CHECK( receipt.cloudsActive && receipt.computeDispatchCount == 4u &&
					weatherReceipt.activeParticleCount == 8192u );
		}
		request = Fixture( (ralBackendType_t)backend,
			RAL_ATMOSPHERE_TIER_OFF );
		/* Authored clear fixture: OFF is exactly zero work on every backend. */
		CHECK( Ral_AtmospherePlan( &request, &receipt ) && receipt.zeroWork == qtrue && receipt.passMask == 0u &&
			   receipt.computeDispatchCount == 0u && receipt.compositeCount == 0u );
		/* Rain and snow fixtures use weather particles plus one analytic media
		 * composition; they do not allocate a froxel grid. */
		request.requestedTier = RAL_ATMOSPHERE_TIER_WEATHER;
		request.mediaActive	  = qtrue;
		CHECK( Ral_AtmospherePlan( &request, &receipt ) && receipt.selectedTier == RAL_ATMOSPHERE_TIER_WEATHER &&
			   receipt.passMask == RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE && receipt.froxelCount == 0u &&
			   receipt.compositeCount == 1u );
		/* Cold/fog/storm fixture: bounded local media, Forward+ light/shadow
		 * injection, temporal reconstruction and one composite. Full clouds are
		 * capability-gated until a backend actually executes their pipeline. */
		request.requestedTier	   = RAL_ATMOSPHERE_TIER_FULL;
		request.localVolumeCount   = 48u;
		request.lightCount		   = 192u;
		request.shadowedLightCount = 12u;
		request.cloudsRequested	   = qtrue;
		request.skyLightingActive  = qtrue;
		request.historyValid	   = qtrue;
		CHECK(
			Ral_AtmospherePlan( &request, &receipt ) && receipt.selectedTier == RAL_ATMOSPHERE_TIER_FULL &&
			receipt.froxelWidth == 80u && receipt.froxelHeight == 45u && receipt.froxelDepth == 64u &&
			receipt.froxelCount == 230400u && receipt.admittedVolumeCount == 32u && receipt.droppedVolumeCount == 16u &&
			receipt.admittedLightCount == 128u && receipt.droppedLightCount == 64u &&
			receipt.admittedShadowedLightCount == 8u && receipt.droppedShadowedLightCount == 4u &&
			receipt.temporalReuseCount == 1u && receipt.temporalRejectCount == 0u && receipt.cloudsActive == qfalse &&
			receipt.fallbackReason == RAL_ATMOSPHERE_FALLBACK_CLOUD_UNSUPPORTED && receipt.computeDispatchCount == 3u &&
			receipt.compositeCount == 1u && ( receipt.passMask & RAL_ATMOSPHERE_PASS_SINGLE_COMPOSITE ) &&
			!( receipt.passMask & RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE ) );
		request.capabilities.fullClouds = qtrue;
		CHECK( Ral_AtmospherePlan( &request, &receipt ) && receipt.cloudsActive == qtrue &&
			   receipt.fallbackReason == RAL_ATMOSPHERE_FALLBACK_NONE &&
			   ( receipt.passMask & RAL_ATMOSPHERE_PASS_CLOUDS ) && receipt.computeDispatchCount == 4u &&
			   receipt.compositeCount == 1u );
		request.capabilities.fullClouds = qfalse;
		exact							= receipt;
		CHECK( Ral_AtmospherePlanReceiptExact( &receipt, &exact ) );
		exact.froxelCount++;
		CHECK( !Ral_AtmospherePlanReceiptExact( &receipt, &exact ) );
		/* Camera cuts and recreate events reject history deterministically. */
		request.cameraCut = qtrue;
		CHECK( Ral_AtmospherePlan( &request, &receipt ) && receipt.temporalReuseCount == 0u &&
			   receipt.temporalRejectCount == 1u );
		/* Unsupported full media is an explicit weather fallback, never a
		 * silent full-tier success. */
		request.cameraCut					= qfalse;
		request.capabilities.storageBuffers = qfalse;
		CHECK( Ral_AtmospherePlan( &request, &receipt ) && receipt.selectedTier == RAL_ATMOSPHERE_TIER_WEATHER &&
			   receipt.fallbackReason == RAL_ATMOSPHERE_FALLBACK_NO_STORAGE_BUFFER && receipt.froxelCount == 0u &&
			   receipt.passMask == RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE );
		request.capabilities.analyticComposite = qfalse;
		CHECK( Ral_AtmospherePlan( &request, &receipt ) && receipt.selectedTier == RAL_ATMOSPHERE_TIER_OFF &&
			   receipt.fallbackReason == RAL_ATMOSPHERE_FALLBACK_BACKEND_UNAVAILABLE && receipt.zeroWork == qtrue &&
			   receipt.passMask == 0u && receipt.computeDispatchCount == 0u && receipt.compositeCount == 0u );

		/* Every backend shares one fail-closed lifecycle: normal history reuse,
		 * resize/cut invalidation, pause/resume, map rewind, capability downgrade,
		 * and loss/recreate. Visual particles remain seed-derived, not network state. */
		CHECK( Ral_AtmosphereRuntimeInit( (ralBackendType_t)backend, &runtime ) );
		memset( &event, 0, sizeof( event ) );
		event.schemaVersion	   = RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION;
		event.backendType	   = (ralBackendType_t)backend;
		event.frameGeneration  = 1u;
		event.deviceGeneration = 1u;
		event.width			   = 1280u;
		event.height		   = 720u;
		event.timelineSeconds  = 10.0f;
		event.timeScale		   = 1.0f;
		event.climateSeed	   = 0x216u;
		event.capabilityDigest = 0x100u + (uint64_t)backend;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && lifecycle.renderAllowed &&
			   !lifecycle.historyReusable && lifecycle.replayDeterministic );
		event.frameGeneration++;
		event.timelineSeconds = 11.0f;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && lifecycle.historyReusable &&
			   lifecycle.invalidationMask == 0u );
		lifecycleExact = lifecycle;
		CHECK( Ral_AtmosphereRuntimeReceiptExact( &lifecycle, &lifecycleExact ) );
		lifecycleExact.deviceGeneration++;
		CHECK( !Ral_AtmosphereRuntimeReceiptExact( &lifecycle, &lifecycleExact ) );
		/* Extent drift without a resize event is rejected output-atomically. */
		lifecycleExact = lifecycle;
		runtimeExact   = runtime;
		event.frameGeneration++;
		event.width = 1600u;
		CHECK( !Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   !memcmp( &runtime, &runtimeExact, sizeof( runtime ) ) &&
			   Ral_AtmosphereRuntimeReceiptExact( &lifecycleExact, &lifecycleExact ) );
		event.eventMask = RAL_ATMOSPHERE_EVENT_RESIZE | RAL_ATMOSPHERE_EVENT_CAMERA_CUT;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && !lifecycle.historyReusable &&
			   lifecycle.invalidationMask == ( RAL_ATMOSPHERE_INVALIDATE_RESIZE | RAL_ATMOSPHERE_INVALIDATE_CAMERA ) );
		event.frameGeneration++;
		event.eventMask = RAL_ATMOSPHERE_EVENT_PAUSE;
		event.timeScale = 0.0f;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && !lifecycle.renderAllowed &&
			   lifecycle.zeroTimeStep );
		event.frameGeneration++;
		event.eventMask		  = RAL_ATMOSPHERE_EVENT_RESUME;
		event.timeScale		  = 0.5f;
		event.timelineSeconds = 11.5f;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && lifecycle.renderAllowed );
		event.frameGeneration++;
		event.eventMask		  = RAL_ATMOSPHERE_EVENT_MAP_TRANSITION;
		event.timelineSeconds = 0.0f;
		event.climateSeed++;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   ( lifecycle.invalidationMask & RAL_ATMOSPHERE_INVALIDATE_MAP ) && !lifecycle.historyReusable );
		event.frameGeneration++;
		event.eventMask = RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE;
		event.capabilityDigest++;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   lifecycle.invalidationMask == RAL_ATMOSPHERE_INVALIDATE_CAPABILITY );
		event.frameGeneration++;
		event.eventMask = RAL_ATMOSPHERE_EVENT_DEVICE_LOST;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && !lifecycle.renderAllowed );
		event.frameGeneration++;
		event.eventMask = 0u;
		CHECK( !Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) );
		event.eventMask = RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED;
		event.deviceGeneration++;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) && lifecycle.renderAllowed &&
			   !lifecycle.historyReusable && lifecycle.invalidationMask == RAL_ATMOSPHERE_INVALIDATE_DEVICE );
	}
	puts( "RAL atmosphere plan: PASS" );
	return 0;
}
