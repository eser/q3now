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
	ralSkyDistantDrawPlanReceipt_t distantParity;
	qboolean distantParityReady = qfalse;
	{
		const float visibility[] = { -1.0f, 0.0f, 0.001f, 1.0f, 4.0f, 4.005f,
			4.01f, 64.0f, 4096.0f, 65536.0f, 100000.0f };
		const float expected[] = { 65536.0f, 65536.0f, 4.01f, 4.01f, 4.01f,
			4.01f, 4.01f, 64.0f, 4096.0f, 65536.0f, 100000.0f };
		CHECK( WIRED_ATMOSPHERE_GRID_NEAR == 4.0f );
		for ( unsigned i = 0; i < sizeof( visibility ) / sizeof( visibility[0] ); ++i ) {
			CHECK( wired_atmosphere_grid_far( visibility[i] ) == expected[i] );
			CHECK( wired_atmosphere_grid_far( visibility[i] ) > WIRED_ATMOSPHERE_GRID_NEAR );
		}
	}
	{
		ralSkyTierBudgetReceipt_t budgets[4], exact;
		ralSkyGpuTelemetryReceipt_t gpuTelemetry, gpuTelemetryExact;
		for ( int tier = RAL_ATMOSPHERE_TIER_OFF;
				tier <= RAL_ATMOSPHERE_TIER_FULL; ++tier ) {
			CHECK( Ral_SkyTierBudget( (ralAtmosphereTier_t)tier,
				&budgets[tier] ) );
			exact = budgets[tier];
			CHECK( Ral_SkyTierBudgetReceiptExact( &budgets[tier], &exact ) );
			exact.maxWorkItemsPerFrame++;
			CHECK( !Ral_SkyTierBudgetReceiptExact( &budgets[tier], &exact ) );
		}
		CHECK( budgets[RAL_ATMOSPHERE_TIER_OFF].allowedWorkMask == 0u &&
			budgets[RAL_ATMOSPHERE_TIER_OFF].maxWorkItemsPerFrame == 0u &&
			!budgets[RAL_ATMOSPHERE_TIER_OFF].distantScene );
		CHECK( budgets[RAL_ATMOSPHERE_TIER_ANALYTIC].allowedWorkMask == 0u &&
			budgets[RAL_ATMOSPHERE_TIER_ANALYTIC].maxWorkItemsPerFrame == 0u &&
			budgets[RAL_ATMOSPHERE_TIER_ANALYTIC].distantScene );
		CHECK( budgets[RAL_ATMOSPHERE_TIER_WEATHER].allowedWorkMask ==
			( RAL_SKY_DIRTY_RADIANCE | RAL_SKY_DIRTY_AMBIENT_SH |
				RAL_SKY_DIRTY_SPECULAR_IBL ) &&
			budgets[RAL_ATMOSPHERE_TIER_WEATHER].maxWorkItemsPerFrame == 1u &&
			budgets[RAL_ATMOSPHERE_TIER_WEATHER].temporalCrossfade );
		CHECK( budgets[RAL_ATMOSPHERE_TIER_FULL].allowedWorkMask ==
			RAL_SKY_DIRTY_ALL &&
			budgets[RAL_ATMOSPHERE_TIER_FULL].maxWorkItemsPerFrame ==
				RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME &&
			budgets[RAL_ATMOSPHERE_TIER_FULL].physicalMode );
		memset( &gpuTelemetry, 0, sizeof( gpuTelemetry ) );
		gpuTelemetry.schemaVersion = RAL_SKY_GPU_TELEMETRY_SCHEMA_VERSION;
		gpuTelemetry.backendType = RAL_BACKEND_METAL;
		gpuTelemetry.frameGeneration = 9u;
		gpuTelemetry.sourceRevision = 4u;
		gpuTelemetry.selectedTier = RAL_ATMOSPHERE_TIER_FULL;
		gpuTelemetry.selectedMode = RAL_SKY_ENVIRONMENT_HYBRID;
		gpuTelemetry.submittedWorkMask = RAL_SKY_DIRTY_SKY_VIEW;
		gpuTelemetry.deferredWorkMask = RAL_SKY_DIRTY_RADIANCE;
		gpuTelemetry.physicalTimingFrameGeneration = 8u;
		gpuTelemetry.physicalTimingState = RAL_SKY_GPU_TIMING_VALID;
		gpuTelemetry.physicalGpuDurationNanoseconds = 1234u;
		gpuTelemetry.radianceTimingState = RAL_SKY_GPU_TIMING_UNAVAILABLE;
		gpuTelemetry.ready = qtrue;
		gpuTelemetryExact = gpuTelemetry;
		CHECK( Ral_SkyGpuTelemetryReceiptExact( &gpuTelemetry,
			&gpuTelemetryExact ) );
		gpuTelemetryExact.radianceGpuDurationNanoseconds = 1u;
		CHECK( !Ral_SkyGpuTelemetryReceiptExact( &gpuTelemetryExact,
			&gpuTelemetryExact ) );
		gpuTelemetryExact = gpuTelemetry;
		gpuTelemetryExact.physicalTimingState = RAL_SKY_GPU_TIMING_PENDING;
		CHECK( !Ral_SkyGpuTelemetryReceiptExact( &gpuTelemetryExact,
			&gpuTelemetryExact ) );
	}
	memset( &distantParity, 0, sizeof( distantParity ) );
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend ) {
		ralSkyEnvironmentPlanRequest_t sky;
		ralSkyEnvironmentPlanReceipt_t receipt, exact, before;
		memset( &sky, 0, sizeof( sky ) );
		sky.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
		sky.backendType = (ralBackendType_t)backend;
		sky.frameGeneration = (uint64_t)backend + 1u;
		sky.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
		sky.mode = RAL_SKY_ENVIRONMENT_HYBRID;
		sky.sourceRevision = 11u;
		sky.currentRadianceRevision = 10u;
		sky.previousRadianceRevision = 9u;
		sky.dirtyMask = RAL_SKY_DIRTY_ALL;
		sky.maxWorkItems = 2u;
		sky.crossfadeSeconds = 0.5f;
		sky.crossfadeProgress = 0.25f;
		sky.sunDirection[2] = 1.0f;
		sky.sunRadiance[0] = 3.0f;
		sky.sunRadiance[1] = 2.0f;
		sky.sunRadiance[2] = 1.0f;
		sky.moonDirection[2] = -1.0f;
		sky.moonRadiance[0] = 0.1f;
		sky.ambientSh[0] = 0.2f;
		sky.ambientSh[1] = 0.3f;
		sky.ambientSh[2] = 0.4f;
		sky.cloudSunOcclusion = 0.6f;
		sky.skyLuminance = 2.0f;
		sky.exposure = 1.25f;
		sky.opticsVisibility = 0.4f;
		sky.authoredSkyAvailable = qtrue;
		sky.distantSceneRequested = qtrue;
		sky.distantSceneIdentity = 77u;
		sky.distantSceneOrigin[0] = 1.0f;
		sky.distantSceneOrigin[1] = 2.0f;
		sky.distantSceneOrigin[2] = 3.0f;
		sky.distantSceneScale = 16.0f;
		sky.distantSceneLodBias = 2.0f;
		sky.capabilities.physicalLuts = qtrue;
		sky.capabilities.radianceCapture = qtrue;
		sky.capabilities.convolution = qtrue;
		sky.capabilities.cloudLighting = qtrue;
		sky.capabilities.distantScene = qtrue;
		sky.capabilities.temporalCrossfade = qtrue;
		CHECK( Ral_SkyEnvironmentPlan( &sky, &receipt ) &&
			receipt.selectedMode == RAL_SKY_ENVIRONMENT_HYBRID &&
			receipt.admittedWorkCount == 2u && receipt.deferredWorkCount == 5u &&
			receipt.admittedWorkMask == ( RAL_SKY_DIRTY_TRANSMITTANCE |
				RAL_SKY_DIRTY_MULTISCATTER ) && receipt.distantSceneActive &&
			receipt.previousProductValid && receipt.crossfadeAlpha == 0.25f &&
			receipt.sunDirection[2] == 1.0f &&
			receipt.sunRadiance[0] == 3.0f &&
			receipt.moonDirection[2] == -1.0f &&
			receipt.ambientSh[2] == 0.4f && receipt.exposure == 1.25f &&
			receipt.distantSceneIdentity == 77u &&
			!memcmp( receipt.distantSceneOrigin, sky.distantSceneOrigin,
				sizeof( receipt.distantSceneOrigin ) ) &&
			receipt.distantSceneScale == 16.0f &&
			receipt.distantSceneLodBias == 2.0f &&
			( receipt.passMask & RAL_SKY_PASS_AUTHORED_BACKGROUND ) &&
			( receipt.passMask & RAL_SKY_PASS_CROSSFADE ) );
		{
			ralSkyEnvironmentPlanRequest_t tiered = sky;
			ralSkyEnvironmentPlanReceipt_t tieredReceipt;
			tiered.requestedTier = RAL_ATMOSPHERE_TIER_OFF;
			tiered.mode = RAL_SKY_ENVIRONMENT_AUTHORED;
			CHECK( Ral_SkyEnvironmentPlan( &tiered, &tieredReceipt ) &&
				tieredReceipt.selectedMode == RAL_SKY_ENVIRONMENT_AUTHORED &&
				tieredReceipt.fallbackReason == RAL_SKY_FALLBACK_TIER &&
				tieredReceipt.admittedWorkCount == 0u &&
				tieredReceipt.deferredWorkCount == 0u &&
				!tieredReceipt.distantSceneActive && tieredReceipt.zeroWork &&
				tieredReceipt.crossfadeAlpha == 1.0f );
			tiered.requestedTier = RAL_ATMOSPHERE_TIER_ANALYTIC;
			CHECK( Ral_SkyEnvironmentPlan( &tiered, &tieredReceipt ) &&
				tieredReceipt.admittedWorkCount == 0u &&
				tieredReceipt.deferredWorkCount == 0u &&
				tieredReceipt.distantSceneActive && tieredReceipt.zeroWork );
			tiered.requestedTier = RAL_ATMOSPHERE_TIER_WEATHER;
			tiered.distantSceneRequested = qfalse;
			tiered.previousRadianceRevision = 0u;
			tiered.dirtyMask = RAL_SKY_DIRTY_RADIANCE |
				RAL_SKY_DIRTY_AMBIENT_SH | RAL_SKY_DIRTY_SPECULAR_IBL;
			tiered.maxWorkItems = RAL_SKY_ENVIRONMENT_MAX_WORK_ITEMS;
			CHECK( Ral_SkyEnvironmentPlan( &tiered, &tieredReceipt ) &&
				tieredReceipt.fallbackReason == RAL_SKY_FALLBACK_NONE &&
				tieredReceipt.admittedWorkMask == RAL_SKY_DIRTY_RADIANCE &&
				tieredReceipt.admittedWorkCount == 1u &&
				tieredReceipt.deferredWorkCount == 2u );
		}
		{
			ralSkyDistantDrawCandidate_t candidates[5];
			ralSkyDistantDrawPlanRequest_t drawRequest;
			ralSkyDistantDrawPlanReceipt_t drawReceipt, drawExact, drawBefore;
			memset( candidates, 0, sizeof( candidates ) );
			memset( &drawRequest, 0, sizeof( drawRequest ) );
			for ( uint32_t index = 0u; index < 5u; ++index ) {
				candidates[index].drawIdentity = 100u + index;
				candidates[index].sourceDrawIndex = 20u + index;
				candidates[index].lodLevelCount = 4u;
				candidates[index].visible = qtrue;
			}
			candidates[0].projectedCoverageQ16 = 65536u;
			candidates[1].projectedCoverageQ16 = 4095u;
			candidates[2].projectedCoverageQ16 = 255u;
			candidates[3].projectedCoverageQ16 = 1u;
			candidates[4].projectedCoverageQ16 = 0u;
			candidates[4].visible = qfalse;
			drawRequest.schemaVersion = RAL_SKY_DISTANT_DRAW_SCHEMA_VERSION;
			drawRequest.skyEnvironment = receipt;
			drawRequest.candidates = candidates;
			drawRequest.candidateCount = 5u;
			drawRequest.maxVisibleDraws = 3u;
			CHECK( Ral_SkyDistantDrawPlan( &drawRequest, &drawReceipt ) &&
				drawReceipt.drawClass == RAL_SKY_DRAW_CLASS_DISTANT_SCENE &&
				drawReceipt.candidateCount == 5u &&
				drawReceipt.maxVisibleDraws == 3u &&
				drawReceipt.visibleCount == 4u &&
				drawReceipt.admittedCount == 3u &&
				drawReceipt.visibilityCulledCount == 1u &&
				drawReceipt.budgetDroppedCount == 1u &&
				drawReceipt.collisionQueryCount == 0u &&
				drawReceipt.entityDrawCount == 0u &&
				drawReceipt.selections[0].lodLevel == 2u &&
				drawReceipt.selections[1].lodLevel == 4u - 1u &&
				drawReceipt.selections[2].lodLevel == 4u - 1u );
			drawExact = drawReceipt;
			CHECK( Ral_SkyDistantDrawPlanReceiptExact( &drawReceipt,
				&drawExact ) );
			/* Backend and frame identity are intentionally backend-local; every
			 * semantic admission/LOD byte must otherwise remain identical. */
			drawExact.backendType = RAL_BACKEND_VULKAN;
			drawExact.frameGeneration = 1u;
			if ( !distantParityReady ) {
				distantParity = drawExact;
				distantParityReady = qtrue;
			} else CHECK( !memcmp( &distantParity, &drawExact,
				sizeof( distantParity ) ) );
			drawExact.entityDrawCount = 1u;
			CHECK( !Ral_SkyDistantDrawPlanReceiptExact( &drawReceipt,
				&drawExact ) );
			drawBefore = drawReceipt;
			candidates[1].visible = 2;
			CHECK( !Ral_SkyDistantDrawPlan( &drawRequest, &drawReceipt ) &&
				!memcmp( &drawReceipt, &drawBefore, sizeof( drawReceipt ) ) );
		}
		{
			ralSkyEnvironmentPlanRequest_t dependency = sky;
			ralSkyEnvironmentPlanReceipt_t dependencyReceipt;
			dependency.dirtyMask = RAL_SKY_DIRTY_RADIANCE |
				RAL_SKY_DIRTY_AMBIENT_SH | RAL_SKY_DIRTY_SPECULAR_IBL;
			dependency.previousRadianceRevision = 0u;
			dependency.maxWorkItems = 3u;
			CHECK( Ral_SkyEnvironmentPlan( &dependency, &dependencyReceipt ) &&
				dependencyReceipt.admittedWorkMask == 0u &&
				dependencyReceipt.deferredWorkMask == dependency.dirtyMask );
			dependency.completedProductMask = RAL_SKY_DIRTY_SKY_VIEW;
			CHECK( Ral_SkyEnvironmentPlan( &dependency, &dependencyReceipt ) &&
				dependencyReceipt.admittedWorkMask == dependency.dirtyMask &&
				dependencyReceipt.deferredWorkMask == 0u );
		}
		{
			ralAtmospherePlanRequest_t atmosphere = Fixture(
				(ralBackendType_t)backend, RAL_ATMOSPHERE_TIER_FULL );
			ralAtmospherePlanReceipt_t integrated, integratedExact;
			atmosphere.mediaActive = qtrue;
			atmosphere.skyLightingActive = qtrue;
			atmosphere.skyEnvironment = sky;
			CHECK( Ral_AtmospherePlan( &atmosphere, &integrated ) &&
				Ral_SkyEnvironmentPlanReceiptExact(
					&integrated.skyEnvironment, &receipt ) );
			integratedExact = integrated;
			CHECK( Ral_AtmospherePlanReceiptExact( &integrated,
				&integratedExact ) );
			atmosphere = Fixture( (ralBackendType_t)backend,
				RAL_ATMOSPHERE_TIER_OFF );
			atmosphere.skyEnvironment = sky;
			atmosphere.skyEnvironment.requestedTier =
				RAL_ATMOSPHERE_TIER_OFF;
			atmosphere.skyEnvironment.mode = RAL_SKY_ENVIRONMENT_AUTHORED;
			CHECK( Ral_AtmospherePlan( &atmosphere, &integrated ) &&
				integrated.zeroWork && integrated.passMask == 0u &&
				integrated.skyEnvironment.zeroWork &&
				integrated.skyEnvironment.admittedWorkCount == 0u &&
				!integrated.skyEnvironment.distantSceneActive );
			atmosphere = Fixture( (ralBackendType_t)backend,
				RAL_ATMOSPHERE_TIER_FULL );
			atmosphere.skyEnvironment = sky;
			atmosphere.skyEnvironment.frameGeneration++;
			CHECK( !Ral_AtmospherePlan( &atmosphere, &integrated ) );
		}
		exact = receipt;
		CHECK( Ral_SkyEnvironmentPlanReceiptExact( &receipt, &exact ) );
		{
			ralSkySurfaceReceipt_t surface, surfaceExact, surfaceBefore;
			receipt.selectedMode = RAL_SKY_ENVIRONMENT_PHYSICAL;
			receipt.completedProductMask |= RAL_SKY_DIRTY_SKY_VIEW;
			receipt.currentRadianceRevision = 10u;
			receipt.previousProductValid = qfalse;
			receipt.crossfadeAlpha = 0.0f;
			/* A first target must consume the physical LUT without waiting for
			 * its own radiance publication; display still waits for that target. */
			receipt.currentRadianceRevision = 0u;
			CHECK( Ral_SkyPhysicalSourceReady( &receipt, qtrue ) );
			CHECK( !Ral_SkyPhysicalSourceReady( &receipt, qfalse ) );
			CHECK( !Ral_SkyPhysicalSourceReady( &receipt, (qboolean)2 ) );
			CHECK( !Ral_SkyPhysicalSourceReady( NULL, qtrue ) );
			CHECK( Ral_SkySurfacePlan( &receipt, qtrue, &surface ) && !surface.physicalReadable );
			receipt.completedProductMask &= ~RAL_SKY_DIRTY_SKY_VIEW;
			CHECK( !Ral_SkyPhysicalSourceReady( &receipt, qtrue ) );
			receipt.completedProductMask |= RAL_SKY_DIRTY_SKY_VIEW;
			receipt.currentRadianceRevision = 10u;
			CHECK( Ral_SkySurfacePlan( &receipt, qtrue, &surface ) &&
				surface.physicalReadable && surface.physicalBlend == 1.0f &&
				surface.authoredComposite && !surface.authoredFallback &&
				surface.physicalProductRevision == 10u );
			surfaceExact = surface;
			CHECK( Ral_SkySurfaceReceiptExact( &surface, &surfaceExact ) );
			receipt.previousProductValid = qtrue;
			receipt.crossfadeAlpha = 0.25f;
			CHECK( Ral_SkySurfacePlan( &receipt, qtrue, &surface ) &&
				surface.physicalBlend == 0.25f && surface.authoredFallback );
			CHECK( Ral_SkySurfacePlan( &receipt, qfalse, &surface ) &&
				!surface.physicalReadable && surface.physicalBlend == 0.0f &&
				surface.authoredComposite && surface.authoredFallback );
			receipt.selectedMode = RAL_SKY_ENVIRONMENT_HYBRID;
			/* HYBRID consumes the physical product for lighting only; the visible
			 * sky stays authored (no surface composite). */
			CHECK( Ral_SkyPhysicalSourceReady( &receipt, qtrue ) );
			CHECK( Ral_SkySurfacePlan( &receipt, qtrue, &surface ) &&
				!surface.physicalReadable && surface.physicalBlend == 0.0f &&
				surface.authoredFallback );
			receipt.selectedMode = RAL_SKY_ENVIRONMENT_PHYSICAL;
			receipt.previousProductValid = qfalse;
			/* r_skyPhysicalBlend scales the PHYSICAL relight; 0 degrades to authored. */
			CHECK( !Ral_SkySurfaceSetPhysicalBlendScale( -0.1f ) && !Ral_SkySurfaceSetPhysicalBlendScale( 1.5f )
				&& !Ral_SkySurfaceSetPhysicalBlendScale( NAN ) && Ral_SkySurfacePhysicalBlendScale() == 1.0f );
			CHECK( Ral_SkySurfaceSetPhysicalBlendScale( 0.5f ) && Ral_SkySurfacePlan( &receipt, qtrue, &surface )
				&& surface.physicalReadable && surface.physicalBlend == 0.5f && surface.authoredFallback );
			CHECK( Ral_SkySurfaceSetPhysicalBlendScale( 0.0f ) && Ral_SkySurfacePlan( &receipt, qtrue, &surface )
				&& !surface.physicalReadable && surface.physicalBlend == 0.0f && surface.authoredFallback
				&& Ral_SkySurfaceReceiptExact( &surface, &surface ) );
			CHECK( Ral_SkySurfaceSetPhysicalBlendScale( 1.0f ) );
			receipt.previousProductValid = qtrue;
			receipt.selectedMode = RAL_SKY_ENVIRONMENT_HYBRID;
			CHECK( Ral_SkySurfacePlan( &receipt, qtrue, &surface ) &&
				!surface.physicalReadable && surface.authoredFallback );
			surfaceBefore = surface;
			receipt.selectedMode = RAL_SKY_ENVIRONMENT_PHYSICAL;
			receipt.crossfadeAlpha = NAN;
			CHECK( !Ral_SkySurfacePlan( &receipt, qtrue, &surface ) &&
				!memcmp( &surface, &surfaceBefore, sizeof( surface ) ) );
			surfaceExact = surfaceBefore;
			surfaceExact.physicalBlend = NAN;
			CHECK( !Ral_SkySurfaceReceiptExact( &surfaceExact, &surfaceExact ) );
			CHECK( Ral_SkyEnvironmentPlan( &sky, &receipt ) );
		}
		exact.exposure = -1.0f;
		CHECK( !Ral_SkyEnvironmentPlanReceiptExact( &exact, &exact ) );
		exact = receipt;
		exact.cloudSunOcclusion = NAN;
		CHECK( !Ral_SkyEnvironmentPlanReceiptExact( &exact, &exact ) );
		exact = receipt;
		exact.admittedWorkCount++;
		CHECK( !Ral_SkyEnvironmentPlanReceiptExact( &receipt, &exact ) );
		before = receipt;
		sky.distantSceneIdentity = 0u;
		CHECK( !Ral_SkyEnvironmentPlan( &sky, &receipt ) &&
			!memcmp( &receipt, &before, sizeof( receipt ) ) );
		sky.distantSceneIdentity = 77u;
		sky.maxWorkItems = RAL_SKY_ENVIRONMENT_MAX_WORK_ITEMS + 1u;
		CHECK( !Ral_SkyEnvironmentPlan( &sky, &receipt ) &&
			!memcmp( &receipt, &before, sizeof( receipt ) ) );
		sky.maxWorkItems = 2u;
		sky.eventMask = RAL_SKY_EVENT_MAP_TRANSITION;
		sky.inFlightWorkMask = RAL_SKY_DIRTY_RADIANCE | RAL_SKY_DIRTY_AMBIENT_SH;
		CHECK( Ral_SkyEnvironmentPlan( &sky, &receipt ) &&
			receipt.cancelledWorkMask == sky.inFlightWorkMask &&
			!receipt.previousProductValid && receipt.crossfadeAlpha == 0.0f );
		sky.eventMask = 0u;
		sky.requestedTier = RAL_ATMOSPHERE_TIER_WEATHER;
		CHECK( Ral_SkyEnvironmentPlan( &sky, &receipt ) &&
			receipt.selectedMode == RAL_SKY_ENVIRONMENT_AUTHORED &&
			receipt.fallbackReason == RAL_SKY_FALLBACK_TIER &&
			( receipt.passMask & RAL_SKY_PASS_AUTHORED_BACKGROUND ) );
		{
			ralSkyEnvironmentRuntimeState_t runtime;
			ralSkyEnvironmentRuntimeRequest_t runtimeRequest;
			ralSkyEnvironmentRuntimeReceipt_t runtimeReceipt, runtimeExact;
			ralSkyEnvironmentPlanRequest_t runtimePlan, planBefore;
			uint64_t frame = 100u + (uint64_t)backend * 16u;
			memset( &runtimeRequest, 0, sizeof( runtimeRequest ) );
			CHECK( Ral_SkyEnvironmentRuntimeInit( (ralBackendType_t)backend,
				&runtime ) );
			runtimeRequest.schemaVersion = RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION;
			runtimeRequest.backendType = (ralBackendType_t)backend;
			runtimeRequest.frameGeneration = frame;
			runtimeRequest.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
			runtimeRequest.mode = RAL_SKY_ENVIRONMENT_HYBRID;
			runtimeRequest.sourceRevision = 11u;
			runtimeRequest.maxWorkItems = 2u;
			runtimeRequest.crossfadeSeconds = 0.5f;
			memcpy( runtimeRequest.sunDirection, sky.sunDirection,
				sizeof( runtimeRequest.sunDirection ) );
			memcpy( runtimeRequest.sunRadiance, sky.sunRadiance,
				sizeof( runtimeRequest.sunRadiance ) );
			memcpy( runtimeRequest.moonDirection, sky.moonDirection,
				sizeof( runtimeRequest.moonDirection ) );
			memcpy( runtimeRequest.moonRadiance, sky.moonRadiance,
				sizeof( runtimeRequest.moonRadiance ) );
			memcpy( runtimeRequest.ambientSh, sky.ambientSh,
				sizeof( runtimeRequest.ambientSh ) );
			runtimeRequest.cloudSunOcclusion = 0.6f;
			runtimeRequest.skyLuminance = 2.0f;
			runtimeRequest.exposure = 1.25f;
			runtimeRequest.opticsVisibility = 0.4f;
			runtimeRequest.authoredSkyAvailable = qtrue;
			runtimeRequest.distantSceneRequested = qtrue;
			runtimeRequest.distantSceneIdentity = 77u;
			runtimeRequest.distantSceneOrigin[0] = 1.0f;
			runtimeRequest.distantSceneOrigin[1] = 2.0f;
			runtimeRequest.distantSceneOrigin[2] = 3.0f;
			runtimeRequest.distantSceneScale = 16.0f;
			runtimeRequest.distantSceneLodBias = 2.0f;
			runtimeRequest.capabilities = sky.capabilities;
			CHECK( Ral_SkyEnvironmentRuntimeAdvance( &runtime,
				&runtimeRequest, &runtimePlan, &runtimeReceipt ) &&
				runtimeReceipt.sourceChanged &&
				runtimeReceipt.invalidatedWorkMask == RAL_SKY_DIRTY_ALL &&
				runtimePlan.dirtyMask == RAL_SKY_DIRTY_ALL &&
				runtimePlan.sunRadiance[0] == 3.0f &&
				runtimePlan.ambientSh[2] == 0.4f &&
				runtimePlan.exposure == 1.25f &&
				runtimePlan.distantSceneIdentity == 77u &&
				runtimePlan.distantSceneScale == 16.0f &&
				runtimePlan.distantSceneLodBias == 2.0f &&
				!runtimeReceipt.targetReady );
			runtimeExact = runtimeReceipt;
			CHECK( Ral_SkyEnvironmentRuntimeReceiptExact( &runtimeReceipt,
				&runtimeExact ) && Ral_SkyEnvironmentPlan( &runtimePlan, &receipt ) &&
				Ral_SkyEnvironmentRuntimeSchedule( &runtime, &receipt ) &&
				runtime.inFlightWorkMask == receipt.admittedWorkMask );
			for ( uint32_t slice = 0u; slice < 4u; ++slice ) {
				runtimeRequest.frameGeneration = ++frame;
				runtimeRequest.completedWorkMask = runtime.inFlightWorkMask;
				CHECK( Ral_SkyEnvironmentRuntimeAdvance( &runtime,
					&runtimeRequest, &runtimePlan, &runtimeReceipt ) &&
					Ral_SkyEnvironmentPlan( &runtimePlan, &receipt ) &&
					Ral_SkyEnvironmentRuntimeSchedule( &runtime, &receipt ) );
			}
			CHECK( runtime.targetReady && runtime.currentRadianceRevision == 11u &&
				runtime.completedProductMask == RAL_SKY_DIRTY_ALL &&
				runtime.pendingDirtyMask == 0u && runtime.inFlightWorkMask == 0u );
			planBefore = runtimePlan;
			runtimeRequest.frameGeneration = ++frame;
			runtimeRequest.completedWorkMask = 0u;
			runtimeRequest.sourceRevision = 12u;
			CHECK( !Ral_SkyEnvironmentRuntimeAdvance( &runtime,
				&runtimeRequest, &runtimePlan, &runtimeReceipt ) &&
				!memcmp( &runtimePlan, &planBefore, sizeof( runtimePlan ) ) );
			runtimeRequest.eventMask = RAL_SKY_EVENT_SOURCE_CHANGE;
			CHECK( Ral_SkyEnvironmentRuntimeAdvance( &runtime,
				&runtimeRequest, &runtimePlan, &runtimeReceipt ) &&
				runtime.currentRadianceRevision == 11u &&
				runtime.targetRadianceRevision == 12u &&
				runtime.completedProductMask == 0u &&
				runtimePlan.dirtyMask == RAL_SKY_DIRTY_ALL );
		}
	}
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
		/* A rendered frame is not a scene-time tick. After rewind, each pinned
		 * or paused frame must consume a new receipt instead of replaying the
		 * old map/time invalidation into the sliced sky scheduler. */
		event.eventMask = 0u;
		event.timeScale = 0.0f;
		for ( unsigned heldFrame = 0u; heldFrame < 16u; ++heldFrame ) {
			uint32_t skyEvents = UINT32_MAX;
			event.frameGeneration++;
			CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
				lifecycle.frameGeneration == event.frameGeneration &&
				lifecycle.invalidationMask == 0u && lifecycle.historyReusable &&
				lifecycle.zeroTimeStep && runtime.timelineSeconds == 0.0f );
			CHECK( Ral_AtmosphereSkyEventMask( lifecycle.invalidationMask,
				&skyEvents ) && skyEvents == 0u );
		}
		event.timeScale = 0.5f;
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
		/* Sun/time-of-day and weather transitions are explicit invalidation
		 * events. Undeclared digest drift is rejected output-atomically; a
		 * declared change invalidates history exactly once, and a stable digest
		 * reuses history again on the following frame. */
		event.frameGeneration++;
		event.eventMask = 0u;
		event.sunDigest = 0x51u;
		runtimeExact = runtime;
		CHECK( !Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   !memcmp( &runtime, &runtimeExact, sizeof( runtime ) ) );
		event.eventMask = RAL_ATMOSPHERE_EVENT_SUN_CHANGE;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   lifecycle.invalidationMask == RAL_ATMOSPHERE_INVALIDATE_SUN &&
			   !lifecycle.historyReusable && runtime.sunDigest == 0x51u );
		event.frameGeneration++;
		event.eventMask = 0u;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   lifecycle.invalidationMask == 0u && lifecycle.historyReusable );
		event.frameGeneration++;
		event.weatherDigest = 0x77u;
		runtimeExact = runtime;
		CHECK( !Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   !memcmp( &runtime, &runtimeExact, sizeof( runtime ) ) );
		event.eventMask = RAL_ATMOSPHERE_EVENT_WEATHER_TRANSITION;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   lifecycle.invalidationMask == RAL_ATMOSPHERE_INVALIDATE_WEATHER &&
			   !lifecycle.historyReusable && runtime.weatherDigest == 0x77u );
		event.frameGeneration++;
		event.eventMask = 0u;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   lifecycle.invalidationMask == 0u && lifecycle.historyReusable );
		/* A map transition may carry a new sun/weather identity without separate events. */
		event.frameGeneration++;
		event.eventMask = RAL_ATMOSPHERE_EVENT_MAP_TRANSITION;
		event.sunDigest = 0x52u;
		event.weatherDigest = 0x78u;
		event.timelineSeconds = 0.0f;
		event.climateSeed++;
		CHECK( Ral_AtmosphereRuntimeAdvance( &runtime, &event, &lifecycle ) &&
			   ( lifecycle.invalidationMask & RAL_ATMOSPHERE_INVALIDATE_MAP ) &&
			   !( lifecycle.invalidationMask & ( RAL_ATMOSPHERE_INVALIDATE_SUN | RAL_ATMOSPHERE_INVALIDATE_WEATHER ) ) &&
			   runtime.sunDigest == 0x52u && runtime.weatherDigest == 0x78u );
		{
			uint32_t skyEvents = UINT32_C( 0xfeedbeef );
			CHECK( Ral_AtmosphereSkyEventMask(
				RAL_ATMOSPHERE_INVALIDATE_SUN | RAL_ATMOSPHERE_INVALIDATE_WEATHER,
				&skyEvents ) && skyEvents ==
				( RAL_SKY_EVENT_TIME_CHANGE | RAL_SKY_EVENT_WEATHER_CHANGE ) );
		}
		{
			uint32_t skyEvents = UINT32_C( 0xfeedbeef );
			CHECK( Ral_AtmosphereSkyEventMask(
				RAL_ATMOSPHERE_INVALIDATE_CAMERA |
				RAL_ATMOSPHERE_INVALIDATE_MAP |
				RAL_ATMOSPHERE_INVALIDATE_DEVICE |
				RAL_ATMOSPHERE_INVALIDATE_TIMELINE |
				RAL_ATMOSPHERE_INVALIDATE_RESIZE |
				RAL_ATMOSPHERE_INVALIDATE_CAPABILITY,
				&skyEvents ) && skyEvents ==
				( RAL_SKY_EVENT_CAMERA_CUT |
				  RAL_SKY_EVENT_MAP_TRANSITION |
				  RAL_SKY_EVENT_DEVICE_RECREATE |
				  RAL_SKY_EVENT_TIME_CHANGE ) );
			skyEvents = UINT32_C( 0xfeedbeef );
			CHECK( !Ral_AtmosphereSkyEventMask( UINT32_C( 0x80000000 ),
				&skyEvents ) && skyEvents == UINT32_C( 0xfeedbeef ) );
		}
	}
	puts( "RAL atmosphere plan: PASS" );
	return 0;
}
