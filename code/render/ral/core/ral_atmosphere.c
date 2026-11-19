// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_atmosphere.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static qboolean BoolValid( qboolean value )
{
	return value == qfalse || value == qtrue;
}

static qboolean SkyFloatArrayValid( const float *values, uint32_t count,
		qboolean nonnegative )
{
	if ( !values ) return qfalse;
	for ( uint32_t index = 0u; index < count; ++index )
		if ( !isfinite( values[index] ) ||
			( nonnegative && values[index] < 0.0f ) ) return qfalse;
	return qtrue;
}

static qboolean SkyDistantSceneValid( qboolean requested, uint64_t identity,
		const float origin[3], float scale, float lodBias )
{
	if ( !BoolValid( requested ) ) return qfalse;
	if ( !requested ) return qtrue;
	return identity > 0u && identity < UINT64_MAX &&
		isfinite( origin[0] ) && isfinite( origin[1] ) && isfinite( origin[2] ) &&
		isfinite( scale ) && scale > 0.0f && isfinite( lodBias );
}

static qboolean RequestValid( const ralAtmospherePlanRequest_t *request )
{
	return request && request->schemaVersion == RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION &&
		   request->backendType >= RAL_BACKEND_VULKAN && request->backendType < RAL_BACKEND_COUNT &&
		   request->frameGeneration > 0u && request->frameGeneration < UINT64_MAX && request->width > 0u &&
		   request->width <= 65536u && request->height > 0u && request->height <= 65536u &&
		   request->requestedTier >= RAL_ATMOSPHERE_TIER_OFF && request->requestedTier <= RAL_ATMOSPHERE_TIER_FULL &&
		   request->localVolumeCount <= RAL_ATMOSPHERE_MAX_VOLUMES &&
		   request->lightCount <= RAL_ATMOSPHERE_MAX_LIGHTS && request->shadowedLightCount <= request->lightCount &&
		   request->shadowedLightCount <= RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS && request->maxLocalVolumes > 0u &&
		   request->maxLocalVolumes <= RAL_ATMOSPHERE_MAX_VOLUMES && request->maxLights > 0u &&
		   request->maxLights <= RAL_ATMOSPHERE_MAX_LIGHTS &&
		   request->maxShadowedLights <= RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS && BoolValid( request->mediaActive ) &&
		   BoolValid( request->skyLightingActive ) && BoolValid( request->cloudsRequested ) &&
		   BoolValid( request->historyValid ) && BoolValid( request->cameraCut ) &&
		   BoolValid( request->deviceRecreated ) && BoolValid( request->capabilities.analyticComposite ) &&
		   BoolValid( request->capabilities.compute ) && BoolValid( request->capabilities.storageBuffers ) &&
		   BoolValid( request->capabilities.temporalHistory ) && BoolValid( request->capabilities.volumetricShadows ) &&
		   BoolValid( request->capabilities.fullClouds );
}

static uint32_t MinU32( uint32_t a, uint32_t b )
{
	return a < b ? a : b;
}

static uint32_t CountBits( uint32_t value )
{
	uint32_t count = 0u;
	while ( value ) {
		count += value & 1u;
		value >>= 1u;
	}
	return count;
}

qboolean Ral_SkyTierBudget( ralAtmosphereTier_t tier,
		ralSkyTierBudgetReceipt_t *outReceipt )
{
	ralSkyTierBudgetReceipt_t receipt;
	if ( !outReceipt || tier < RAL_ATMOSPHERE_TIER_OFF ||
			tier > RAL_ATMOSPHERE_TIER_FULL ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_SKY_TIER_BUDGET_SCHEMA_VERSION;
	receipt.tier = tier;
	switch ( tier ) {
	case RAL_ATMOSPHERE_TIER_OFF:
		break;
	case RAL_ATMOSPHERE_TIER_ANALYTIC:
		receipt.distantScene = qtrue;
		break;
	case RAL_ATMOSPHERE_TIER_WEATHER:
		receipt.allowedWorkMask = RAL_SKY_DIRTY_RADIANCE |
			RAL_SKY_DIRTY_AMBIENT_SH | RAL_SKY_DIRTY_SPECULAR_IBL;
		receipt.maxWorkItemsPerFrame = 1u;
		receipt.maxRadianceSlicesPerFrame = 1u;
		receipt.temporalCrossfade = qtrue;
		receipt.distantScene = qtrue;
		break;
	case RAL_ATMOSPHERE_TIER_FULL:
		receipt.allowedWorkMask = RAL_SKY_DIRTY_ALL;
		receipt.maxWorkItemsPerFrame =
			RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME;
		receipt.maxRadianceSlicesPerFrame =
			RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME;
		receipt.physicalMode = qtrue;
		receipt.temporalCrossfade = qtrue;
		receipt.distantScene = qtrue;
		break;
	}
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkyTierBudgetReceiptExact(
		const ralSkyTierBudgetReceipt_t *a,
		const ralSkyTierBudgetReceipt_t *b )
{
	ralSkyTierBudgetReceipt_t expected;
	return a && b && Ral_SkyTierBudget( a->tier, &expected ) &&
		!memcmp( a, &expected, sizeof( expected ) ) &&
		!memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static qboolean SkyGpuTimingValid( ralSkyGpuTimingState_t state,
		uint64_t frameGeneration, uint64_t durationNanoseconds )
{
	if ( state < RAL_SKY_GPU_TIMING_UNAVAILABLE ||
			state > RAL_SKY_GPU_TIMING_FAILED ) return qfalse;
	if ( state == RAL_SKY_GPU_TIMING_VALID )
		return frameGeneration > 0u && frameGeneration < UINT64_MAX &&
			durationNanoseconds > 0u ? qtrue : qfalse;
	if ( durationNanoseconds != 0u ) return qfalse;
	if ( state == RAL_SKY_GPU_TIMING_PENDING )
		return frameGeneration > 0u && frameGeneration < UINT64_MAX
			? qtrue : qfalse;
	if ( state == RAL_SKY_GPU_TIMING_FAILED )
		return frameGeneration > 0u && frameGeneration < UINT64_MAX
			? qtrue : qfalse;
	return frameGeneration == 0u ? qtrue : qfalse;
}

qboolean Ral_SkyGpuTelemetryReceiptExact(
		const ralSkyGpuTelemetryReceipt_t *a,
		const ralSkyGpuTelemetryReceipt_t *b )
{
	if ( !a || !b ||
			a->schemaVersion != RAL_SKY_GPU_TELEMETRY_SCHEMA_VERSION ||
			!Ral_BackendTypeValid( a->backendType ) ||
			a->frameGeneration == 0u || a->frameGeneration == UINT64_MAX ||
			a->sourceRevision == 0u || a->sourceRevision == UINT64_MAX ||
			a->selectedTier < RAL_ATMOSPHERE_TIER_OFF ||
			a->selectedTier > RAL_ATMOSPHERE_TIER_FULL ||
			a->selectedMode < RAL_SKY_ENVIRONMENT_NONE ||
			a->selectedMode > RAL_SKY_ENVIRONMENT_HYBRID ||
			( ( a->submittedWorkMask | a->completedWorkMask |
				a->deferredWorkMask | a->cancelledWorkMask ) &
				~RAL_SKY_DIRTY_ALL ) != 0u ||
			!SkyGpuTimingValid( a->physicalTimingState,
				a->physicalTimingFrameGeneration,
				a->physicalGpuDurationNanoseconds ) ||
			!SkyGpuTimingValid( a->radianceTimingState,
				a->radianceTimingFrameGeneration,
				a->radianceGpuDurationNanoseconds ) ||
			!BoolValid( a->ready ) || !a->ready ) return qfalse;
	return !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static qboolean SkyRequestValid( const ralSkyEnvironmentPlanRequest_t *request )
{
	return request && request->schemaVersion == RAL_SKY_ENVIRONMENT_SCHEMA_VERSION &&
		request->backendType >= RAL_BACKEND_VULKAN && request->backendType < RAL_BACKEND_COUNT &&
		request->frameGeneration > 0u && request->frameGeneration < UINT64_MAX &&
		request->requestedTier <= RAL_ATMOSPHERE_TIER_FULL &&
		request->mode >= RAL_SKY_ENVIRONMENT_NONE && request->mode <= RAL_SKY_ENVIRONMENT_HYBRID &&
		request->sourceRevision > 0u && request->sourceRevision < UINT64_MAX &&
		request->currentRadianceRevision < UINT64_MAX && request->previousRadianceRevision < UINT64_MAX &&
		( request->dirtyMask & ~RAL_SKY_DIRTY_ALL ) == 0u &&
		( request->inFlightWorkMask & ~RAL_SKY_DIRTY_ALL ) == 0u &&
		( request->completedProductMask & ~RAL_SKY_DIRTY_ALL ) == 0u &&
		( request->dirtyMask & request->completedProductMask ) == 0u &&
		( request->inFlightWorkMask & request->completedProductMask ) == 0u &&
		( request->eventMask & ~RAL_SKY_EVENT_ALL ) == 0u &&
		request->maxWorkItems <= RAL_SKY_ENVIRONMENT_MAX_WORK_ITEMS &&
		isfinite( request->crossfadeSeconds ) && request->crossfadeSeconds >= 0.0f &&
		isfinite( request->crossfadeProgress ) && request->crossfadeProgress >= 0.0f &&
		request->crossfadeProgress <= 1.0f &&
		SkyFloatArrayValid( request->sunDirection, 3u, qfalse ) &&
		SkyFloatArrayValid( request->sunRadiance, 3u, qtrue ) &&
		SkyFloatArrayValid( request->moonDirection, 3u, qfalse ) &&
		SkyFloatArrayValid( request->moonRadiance, 3u, qtrue ) &&
		SkyFloatArrayValid( request->ambientSh, 27u, qfalse ) &&
		isfinite( request->cloudSunOcclusion ) &&
		request->cloudSunOcclusion >= 0.0f && request->cloudSunOcclusion <= 1.0f &&
		isfinite( request->skyLuminance ) && request->skyLuminance >= 0.0f &&
		isfinite( request->exposure ) && request->exposure >= 0.0f &&
		isfinite( request->opticsVisibility ) && request->opticsVisibility >= 0.0f &&
		request->opticsVisibility <= 1.0f && BoolValid( request->authoredSkyAvailable ) &&
		SkyDistantSceneValid( request->distantSceneRequested,
			request->distantSceneIdentity, request->distantSceneOrigin,
			request->distantSceneScale, request->distantSceneLodBias ) &&
		BoolValid( request->capabilities.physicalLuts ) &&
		BoolValid( request->capabilities.radianceCapture ) && BoolValid( request->capabilities.convolution ) &&
		BoolValid( request->capabilities.cloudLighting ) &&
		BoolValid( request->capabilities.distantScene ) && BoolValid( request->capabilities.temporalCrossfade );
}

static uint32_t SkyPassForWork( uint32_t bit )
{
	switch ( bit ) {
	case RAL_SKY_DIRTY_TRANSMITTANCE: return RAL_SKY_PASS_TRANSMITTANCE;
	case RAL_SKY_DIRTY_MULTISCATTER: return RAL_SKY_PASS_MULTISCATTER;
	case RAL_SKY_DIRTY_SKY_VIEW: return RAL_SKY_PASS_SKY_VIEW;
	case RAL_SKY_DIRTY_RADIANCE: return RAL_SKY_PASS_RADIANCE_CAPTURE;
	case RAL_SKY_DIRTY_AMBIENT_SH: return RAL_SKY_PASS_AMBIENT_SH;
	case RAL_SKY_DIRTY_SPECULAR_IBL: return RAL_SKY_PASS_SPECULAR_IBL;
	case RAL_SKY_DIRTY_CLOUD_LIGHT: return RAL_SKY_PASS_CLOUD_LIGHT;
	default: return 0u;
	}
}

static uint32_t SkyDependenciesForWork( ralSkyEnvironmentMode_t mode,
		uint32_t bit )
{
	switch ( bit ) {
	case RAL_SKY_DIRTY_MULTISCATTER:
		return RAL_SKY_DIRTY_TRANSMITTANCE;
	case RAL_SKY_DIRTY_SKY_VIEW:
		return RAL_SKY_DIRTY_TRANSMITTANCE | RAL_SKY_DIRTY_MULTISCATTER;
	case RAL_SKY_DIRTY_RADIANCE:
		return mode >= RAL_SKY_ENVIRONMENT_PHYSICAL
			? RAL_SKY_DIRTY_SKY_VIEW : 0u;
	case RAL_SKY_DIRTY_AMBIENT_SH:
	case RAL_SKY_DIRTY_SPECULAR_IBL:
		return RAL_SKY_DIRTY_RADIANCE;
	case RAL_SKY_DIRTY_CLOUD_LIGHT:
		return mode >= RAL_SKY_ENVIRONMENT_PHYSICAL
			? RAL_SKY_DIRTY_SKY_VIEW : 0u;
	default:
		return 0u;
	}
}

qboolean Ral_SkyEnvironmentPlan( const ralSkyEnvironmentPlanRequest_t *request,
		ralSkyEnvironmentPlanReceipt_t *outReceipt )
{
	ralSkyEnvironmentPlanReceipt_t receipt;
	ralSkyTierBudgetReceipt_t tierBudget;
	uint32_t pending, admitted = 0u, effectiveWorkBudget;
	qboolean stale;
	if ( !outReceipt || !SkyRequestValid( request ) ||
			!Ral_SkyTierBudget( (ralAtmosphereTier_t)request->requestedTier,
				&tierBudget ) ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_SKY_ENVIRONMENT_RECEIPT_SCHEMA_VERSION;
	receipt.backendType = request->backendType;
	receipt.frameGeneration = request->frameGeneration;
	receipt.requestedMode = request->mode;
	receipt.selectedMode = request->mode;
	receipt.sourceRevision = request->sourceRevision;
	receipt.currentRadianceRevision = request->currentRadianceRevision;
	receipt.previousRadianceRevision = request->previousRadianceRevision;
	receipt.completedProductMask = request->completedProductMask;
	memcpy( receipt.sunDirection, request->sunDirection,
		sizeof( receipt.sunDirection ) );
	memcpy( receipt.sunRadiance, request->sunRadiance,
		sizeof( receipt.sunRadiance ) );
	memcpy( receipt.moonDirection, request->moonDirection,
		sizeof( receipt.moonDirection ) );
	memcpy( receipt.moonRadiance, request->moonRadiance,
		sizeof( receipt.moonRadiance ) );
	memcpy( receipt.ambientSh, request->ambientSh,
		sizeof( receipt.ambientSh ) );
	receipt.cloudSunOcclusion = request->cloudSunOcclusion;
	receipt.skyLuminance = request->skyLuminance;
	receipt.exposure = request->exposure;
	receipt.opticsVisibility = request->opticsVisibility;
	if ( request->authoredSkyAvailable ) receipt.passMask |= RAL_SKY_PASS_AUTHORED_BACKGROUND;
	if ( receipt.selectedMode == RAL_SKY_ENVIRONMENT_HYBRID && !request->authoredSkyAvailable )
		receipt.selectedMode = RAL_SKY_ENVIRONMENT_PHYSICAL;
	if ( receipt.selectedMode >= RAL_SKY_ENVIRONMENT_PHYSICAL &&
			( !tierBudget.physicalMode || !request->capabilities.physicalLuts ) ) {
		receipt.fallbackReason = !tierBudget.physicalMode
			? RAL_SKY_FALLBACK_TIER : RAL_SKY_FALLBACK_NO_PHYSICAL_LUT;
		receipt.selectedMode = request->authoredSkyAvailable
			? RAL_SKY_ENVIRONMENT_AUTHORED : RAL_SKY_ENVIRONMENT_NONE;
	}
	if ( request->distantSceneRequested ) {
		if ( tierBudget.distantScene && request->capabilities.distantScene &&
				receipt.selectedMode != RAL_SKY_ENVIRONMENT_NONE ) {
			receipt.distantSceneIdentity = request->distantSceneIdentity;
			memcpy( receipt.distantSceneOrigin, request->distantSceneOrigin,
				sizeof( receipt.distantSceneOrigin ) );
			receipt.distantSceneScale = request->distantSceneScale;
			receipt.distantSceneLodBias = request->distantSceneLodBias;
			receipt.distantSceneActive = qtrue;
			receipt.passMask |= RAL_SKY_PASS_DISTANT_SCENE;
		} else if ( receipt.fallbackReason == RAL_SKY_FALLBACK_NONE )
			receipt.fallbackReason = !tierBudget.distantScene
				? RAL_SKY_FALLBACK_TIER : RAL_SKY_FALLBACK_NO_DISTANT_SCENE;
	}
	stale = ( request->eventMask & ( RAL_SKY_EVENT_MAP_TRANSITION |
		RAL_SKY_EVENT_DEVICE_RECREATE | RAL_SKY_EVENT_SOURCE_CHANGE ) ) != 0u;
	if ( stale ) receipt.cancelledWorkMask = request->inFlightWorkMask;
	pending = request->dirtyMask;
	if ( receipt.selectedMode < RAL_SKY_ENVIRONMENT_PHYSICAL )
		pending &= ~( RAL_SKY_DIRTY_TRANSMITTANCE | RAL_SKY_DIRTY_MULTISCATTER |
			RAL_SKY_DIRTY_SKY_VIEW | RAL_SKY_DIRTY_CLOUD_LIGHT );
	if ( pending & ~tierBudget.allowedWorkMask ) {
		pending &= tierBudget.allowedWorkMask;
		if ( receipt.fallbackReason == RAL_SKY_FALLBACK_NONE )
			receipt.fallbackReason = RAL_SKY_FALLBACK_TIER;
	}
	if ( !request->capabilities.radianceCapture ) {
		pending &= ~( RAL_SKY_DIRTY_RADIANCE | RAL_SKY_DIRTY_AMBIENT_SH |
			RAL_SKY_DIRTY_SPECULAR_IBL );
		if ( ( request->dirtyMask & RAL_SKY_DIRTY_RADIANCE ) &&
				receipt.fallbackReason == RAL_SKY_FALLBACK_NONE )
			receipt.fallbackReason = RAL_SKY_FALLBACK_NO_CAPTURE;
	}
	if ( !request->capabilities.convolution ) {
		pending &= ~( RAL_SKY_DIRTY_AMBIENT_SH | RAL_SKY_DIRTY_SPECULAR_IBL );
		if ( ( request->dirtyMask & ( RAL_SKY_DIRTY_AMBIENT_SH |
				RAL_SKY_DIRTY_SPECULAR_IBL ) ) &&
				receipt.fallbackReason == RAL_SKY_FALLBACK_NONE )
			receipt.fallbackReason = RAL_SKY_FALLBACK_NO_CONVOLUTION;
	}
	if ( !request->capabilities.cloudLighting ) {
		pending &= ~RAL_SKY_DIRTY_CLOUD_LIGHT;
		if ( ( request->dirtyMask & RAL_SKY_DIRTY_CLOUD_LIGHT ) &&
				receipt.fallbackReason == RAL_SKY_FALLBACK_NONE )
			receipt.fallbackReason = RAL_SKY_FALLBACK_NO_CLOUD_LIGHT;
	}
	effectiveWorkBudget = MinU32( request->maxWorkItems,
		tierBudget.maxWorkItemsPerFrame );
	for ( uint32_t bit = 1u; bit <= RAL_SKY_DIRTY_CLOUD_LIGHT; bit <<= 1u ) {
		const uint32_t dependencies = SkyDependenciesForWork(
			receipt.selectedMode, bit );
		const uint32_t available = request->completedProductMask | admitted;
		if ( !( pending & bit ) || receipt.admittedWorkCount >= effectiveWorkBudget ||
			( dependencies & available ) != dependencies ) continue;
		admitted |= bit;
		receipt.passMask |= SkyPassForWork( bit );
		receipt.admittedWorkCount++;
	}
	receipt.admittedWorkMask = admitted;
	receipt.deferredWorkMask = pending & ~admitted;
	receipt.deferredWorkCount = CountBits( receipt.deferredWorkMask );
	if ( request->previousRadianceRevision && !stale ) {
		receipt.previousProductValid = qtrue;
		if ( request->crossfadeSeconds > 0.0f && request->crossfadeProgress < 1.0f ) {
			if ( tierBudget.temporalCrossfade &&
					request->capabilities.temporalCrossfade ) {
				receipt.crossfadeAlpha = request->crossfadeProgress;
				receipt.passMask |= RAL_SKY_PASS_CROSSFADE;
			} else {
				receipt.crossfadeAlpha = 1.0f;
				if ( receipt.fallbackReason == RAL_SKY_FALLBACK_NONE )
					receipt.fallbackReason = !tierBudget.temporalCrossfade
						? RAL_SKY_FALLBACK_TIER :
						RAL_SKY_FALLBACK_NO_TEMPORAL_CROSSFADE;
			}
		} else receipt.crossfadeAlpha = 1.0f;
	}
	receipt.zeroWork = receipt.admittedWorkCount == 0u &&
		( receipt.passMask & ~( RAL_SKY_PASS_AUTHORED_BACKGROUND |
			RAL_SKY_PASS_DISTANT_SCENE ) ) == 0u ? qtrue : qfalse;
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkyEnvironmentPlanReceiptExact(
		const ralSkyEnvironmentPlanReceipt_t *a,
		const ralSkyEnvironmentPlanReceipt_t *b )
{
	return a && b && a->schemaVersion == RAL_SKY_ENVIRONMENT_RECEIPT_SCHEMA_VERSION &&
		a->backendType >= RAL_BACKEND_VULKAN && a->backendType < RAL_BACKEND_COUNT &&
		a->frameGeneration > 0u && a->frameGeneration < UINT64_MAX &&
		a->sourceRevision > 0u && a->sourceRevision < UINT64_MAX &&
		a->currentRadianceRevision < UINT64_MAX &&
		a->previousRadianceRevision < UINT64_MAX &&
		SkyFloatArrayValid( a->sunDirection, 3u, qfalse ) &&
		SkyFloatArrayValid( a->sunRadiance, 3u, qtrue ) &&
		SkyFloatArrayValid( a->moonDirection, 3u, qfalse ) &&
		SkyFloatArrayValid( a->moonRadiance, 3u, qtrue ) &&
		SkyFloatArrayValid( a->ambientSh, 27u, qfalse ) &&
		isfinite( a->cloudSunOcclusion ) && a->cloudSunOcclusion >= 0.0f &&
		a->cloudSunOcclusion <= 1.0f && isfinite( a->skyLuminance ) &&
		a->skyLuminance >= 0.0f && isfinite( a->exposure ) && a->exposure >= 0.0f &&
		isfinite( a->opticsVisibility ) && a->opticsVisibility >= 0.0f &&
		a->opticsVisibility <= 1.0f &&
		isfinite( a->crossfadeAlpha ) && a->crossfadeAlpha >= 0.0f &&
		a->crossfadeAlpha <= 1.0f &&
		a->requestedMode >= RAL_SKY_ENVIRONMENT_NONE &&
		a->requestedMode <= RAL_SKY_ENVIRONMENT_HYBRID &&
		a->selectedMode >= RAL_SKY_ENVIRONMENT_NONE && a->selectedMode <= RAL_SKY_ENVIRONMENT_HYBRID &&
		( !a->distantSceneActive || SkyDistantSceneValid( qtrue,
			a->distantSceneIdentity, a->distantSceneOrigin,
			a->distantSceneScale, a->distantSceneLodBias ) ) &&
		a->admittedWorkCount == CountBits( a->admittedWorkMask ) &&
		a->deferredWorkCount == CountBits( a->deferredWorkMask ) && a->ready == qtrue &&
		!memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static float s_skyPhysicalBlendScale = 1.0f;

qboolean Ral_SkySurfaceSetPhysicalBlendScale( float scale )
{
	if ( !isfinite( scale ) || scale < 0.0f || scale > 1.0f ) return qfalse;
	s_skyPhysicalBlendScale = scale;
	return qtrue;
}

float Ral_SkySurfacePhysicalBlendScale( void )
{
	return s_skyPhysicalBlendScale;
}

/* PHYSICAL and HYBRID both consume the completed physical sky-view product:
 * PHYSICAL composes it into the visible sky, HYBRID feeds only the lighting
 * products (radiance capture, IBL) while the authored background stays. */
qboolean Ral_SkyPhysicalSourceReady( const ralSkyEnvironmentPlanReceipt_t *environment,
		qboolean physicalProductReadable )
{
	return environment && physicalProductReadable == qtrue &&
		Ral_SkyEnvironmentPlanReceiptExact( environment, environment ) &&
		( environment->selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL ||
			environment->selectedMode == RAL_SKY_ENVIRONMENT_HYBRID ) &&
		( environment->completedProductMask & RAL_SKY_DIRTY_SKY_VIEW ) != 0u;
}

qboolean Ral_SkySurfacePlan( const ralSkyEnvironmentPlanReceipt_t *environment,
		qboolean physicalProductReadable,
		ralSkySurfaceReceipt_t *outReceipt )
{
	ralSkySurfaceReceipt_t receipt;
	qboolean physicalComplete;
	if ( !environment || !outReceipt ||
			!Ral_SkyEnvironmentPlanReceiptExact( environment, environment ) ||
			!BoolValid( physicalProductReadable ) ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_SKY_SURFACE_RECEIPT_SCHEMA_VERSION;
	receipt.backendType = environment->backendType;
	receipt.frameGeneration = environment->frameGeneration;
	receipt.sourceRevision = environment->sourceRevision;
	receipt.selectedMode = environment->selectedMode;
	receipt.authoredComposite =
		( environment->passMask & RAL_SKY_PASS_AUTHORED_BACKGROUND ) != 0u
		? qtrue : qfalse;
	/* The visible-sky composite is PHYSICAL-only; HYBRID keeps the authored
	 * background and uses the physical product for lighting alone. */
	physicalComplete = Ral_SkyPhysicalSourceReady( environment, physicalProductReadable ) &&
		environment->selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL &&
		environment->currentRadianceRevision != 0u;
	if ( physicalComplete ) {
		receipt.physicalProductRevision = environment->currentRadianceRevision;
		receipt.physicalReadable = qtrue;
		receipt.physicalBlend = ( environment->previousProductValid
			? environment->crossfadeAlpha : 1.0f ) * s_skyPhysicalBlendScale;
		if ( receipt.physicalBlend <= 0.0f ) {
			receipt.physicalReadable = qfalse;
			receipt.physicalProductRevision = 0u;
			receipt.physicalBlend = 0.0f;
		}
	}
	receipt.authoredFallback = !physicalComplete || receipt.physicalBlend < 1.0f
		? qtrue : qfalse;
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkySurfaceReceiptExact( const ralSkySurfaceReceipt_t *a,
		const ralSkySurfaceReceipt_t *b )
{
	return a && b &&
		a->schemaVersion == RAL_SKY_SURFACE_RECEIPT_SCHEMA_VERSION &&
		a->backendType >= RAL_BACKEND_VULKAN && a->backendType < RAL_BACKEND_COUNT &&
		a->frameGeneration > 0u && a->frameGeneration < UINT64_MAX &&
		a->sourceRevision > 0u && a->sourceRevision < UINT64_MAX &&
		a->physicalProductRevision < UINT64_MAX &&
		a->selectedMode >= RAL_SKY_ENVIRONMENT_NONE &&
		a->selectedMode <= RAL_SKY_ENVIRONMENT_HYBRID &&
		isfinite( a->physicalBlend ) && a->physicalBlend >= 0.0f &&
		a->physicalBlend <= 1.0f && BoolValid( a->authoredComposite ) &&
		BoolValid( a->authoredFallback ) &&
		BoolValid( a->physicalReadable ) &&
		( a->physicalReadable ? a->physicalProductRevision != 0u &&
			a->selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL &&
			a->physicalBlend > 0.0f : a->physicalProductRevision == 0u &&
			a->physicalBlend == 0.0f ) && a->ready == qtrue &&
		!memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static qboolean SkyDistantCandidateValid(
		const ralSkyDistantDrawCandidate_t *candidate )
{
	return candidate && candidate->drawIdentity > 0u &&
		candidate->drawIdentity < UINT64_MAX &&
		candidate->projectedCoverageQ16 <= 65536u &&
		candidate->lodLevelCount > 0u &&
		candidate->lodLevelCount <= RAL_SKY_DISTANT_MAX_LOD_LEVELS &&
		BoolValid( candidate->visible ) &&
		( !candidate->visible || candidate->projectedCoverageQ16 > 0u );
}

static uint32_t SkyDistantLodLevel( uint32_t projectedCoverageQ16,
		uint32_t lodLevelCount, float lodBias )
{
	uint32_t level = 0u;
	uint32_t threshold = 65536u;
	int32_t biased;
	float boundedBias;
	while ( level + 1u < lodLevelCount ) {
		uint32_t nextThreshold = threshold > 4u ? threshold / 4u : 1u;
		if ( projectedCoverageQ16 >= nextThreshold ) break;
		threshold = nextThreshold;
		level++;
	}
	/* Authoring remains a float, but the selected geometry LOD is deliberately
	 * integral.  Clamp the bias to the finite number of authored levels so an
	 * extreme cvar/content value cannot escape the bounded policy. */
	boundedBias = fmaxf( -(float)( RAL_SKY_DISTANT_MAX_LOD_LEVELS - 1u ),
		fminf( (float)( RAL_SKY_DISTANT_MAX_LOD_LEVELS - 1u ), lodBias ) );
	biased = (int32_t)level + (int32_t)lroundf( boundedBias );
	if ( biased < 0 ) biased = 0;
	if ( biased >= (int32_t)lodLevelCount )
		biased = (int32_t)lodLevelCount - 1;
	return (uint32_t)biased;
}

qboolean Ral_SkyDistantDrawPlan( const ralSkyDistantDrawPlanRequest_t *request,
		ralSkyDistantDrawPlanReceipt_t *outReceipt )
{
	ralSkyDistantDrawPlanReceipt_t receipt;
	const ralSkyEnvironmentPlanReceipt_t *sky;
	if ( !outReceipt || !request ||
		request->schemaVersion != RAL_SKY_DISTANT_DRAW_SCHEMA_VERSION ||
		request->candidateCount > RAL_SKY_DISTANT_MAX_CANDIDATES ||
		( request->candidateCount && !request->candidates ) ||
		request->maxVisibleDraws == 0u ||
		request->maxVisibleDraws > RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS )
		return qfalse;
	sky = &request->skyEnvironment;
	if ( !Ral_SkyEnvironmentPlanReceiptExact( sky, sky ) ||
		!sky->distantSceneActive ||
		!( sky->passMask & RAL_SKY_PASS_DISTANT_SCENE ) )
		return qfalse;
	for ( uint32_t index = 0u; index < request->candidateCount; ++index )
		if ( !SkyDistantCandidateValid( &request->candidates[index] ) )
			return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_SKY_DISTANT_DRAW_SCHEMA_VERSION;
	receipt.backendType = sky->backendType;
	receipt.frameGeneration = sky->frameGeneration;
	receipt.distantSceneIdentity = sky->distantSceneIdentity;
	receipt.drawClass = RAL_SKY_DRAW_CLASS_DISTANT_SCENE;
	receipt.candidateCount = request->candidateCount;
	receipt.maxVisibleDraws = request->maxVisibleDraws;
	memcpy( receipt.distantSceneOrigin, sky->distantSceneOrigin,
		sizeof( receipt.distantSceneOrigin ) );
	receipt.distantSceneScale = sky->distantSceneScale;
	receipt.distantSceneLodBias = sky->distantSceneLodBias;
	for ( uint32_t index = 0u; index < request->candidateCount; ++index ) {
		const ralSkyDistantDrawCandidate_t *candidate =
			&request->candidates[index];
		ralSkyDistantDrawSelection_t *selection;
		if ( !candidate->visible ) {
			receipt.visibilityCulledCount++;
			continue;
		}
		receipt.visibleCount++;
		if ( receipt.admittedCount >= request->maxVisibleDraws ) {
			receipt.budgetDroppedCount++;
			continue;
		}
		selection = &receipt.selections[receipt.admittedCount++];
		selection->drawIdentity = candidate->drawIdentity;
		selection->sourceDrawIndex = candidate->sourceDrawIndex;
		selection->lodLevel = SkyDistantLodLevel(
			candidate->projectedCoverageQ16, candidate->lodLevelCount,
			sky->distantSceneLodBias );
	}
	/* These stay explicit in the receipt so a backend cannot reinterpret a
	 * distant scene as a second gameplay world or silently submit entities. */
	receipt.collisionQueryCount = 0u;
	receipt.entityDrawCount = 0u;
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkyDistantDrawPlanReceiptExact(
		const ralSkyDistantDrawPlanReceipt_t *a,
		const ralSkyDistantDrawPlanReceipt_t *b )
{
	if ( !a || !b || a->schemaVersion != RAL_SKY_DISTANT_DRAW_SCHEMA_VERSION ||
		a->backendType < RAL_BACKEND_VULKAN ||
		a->backendType >= RAL_BACKEND_COUNT || !a->frameGeneration ||
		a->frameGeneration == UINT64_MAX || a->distantSceneIdentity == 0u ||
		a->distantSceneIdentity == UINT64_MAX ||
		a->drawClass != RAL_SKY_DRAW_CLASS_DISTANT_SCENE ||
		a->candidateCount > RAL_SKY_DISTANT_MAX_CANDIDATES ||
		a->maxVisibleDraws == 0u ||
		a->maxVisibleDraws > RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS ||
		a->admittedCount > a->maxVisibleDraws ||
		a->visibleCount != a->admittedCount + a->budgetDroppedCount ||
		a->candidateCount != a->visibleCount + a->visibilityCulledCount ||
		a->collisionQueryCount != 0u || a->entityDrawCount != 0u ||
		!SkyDistantSceneValid( qtrue, a->distantSceneIdentity,
			a->distantSceneOrigin, a->distantSceneScale,
			a->distantSceneLodBias ) || a->ready != qtrue )
		return qfalse;
	for ( uint32_t index = 0u; index < a->admittedCount; ++index ) {
		const ralSkyDistantDrawSelection_t *selection = &a->selections[index];
		if ( selection->drawIdentity == 0u ||
			selection->drawIdentity == UINT64_MAX ||
			selection->lodLevel >= RAL_SKY_DISTANT_MAX_LOD_LEVELS )
			return qfalse;
	}
	for ( uint32_t index = a->admittedCount;
		index < RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS; ++index )
		if ( memcmp( &a->selections[index],
			&(ralSkyDistantDrawSelection_t){ 0 },
			sizeof( a->selections[index] ) ) ) return qfalse;
	return !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static uint32_t SkyDirtyForMode( ralSkyEnvironmentMode_t mode )
{
	switch ( mode ) {
	case RAL_SKY_ENVIRONMENT_AUTHORED:
		return RAL_SKY_DIRTY_RADIANCE | RAL_SKY_DIRTY_AMBIENT_SH |
			RAL_SKY_DIRTY_SPECULAR_IBL;
	case RAL_SKY_ENVIRONMENT_PHYSICAL:
	case RAL_SKY_ENVIRONMENT_HYBRID:
		return RAL_SKY_DIRTY_ALL;
	default:
		return 0u;
	}
}

qboolean Ral_SkyEnvironmentRuntimeInit( ralBackendType_t backendType,
		ralSkyEnvironmentRuntimeState_t *outState )
{
	if ( !outState || backendType < RAL_BACKEND_VULKAN ||
			backendType >= RAL_BACKEND_COUNT ) return qfalse;
	memset( outState, 0, sizeof( *outState ) );
	outState->schemaVersion = RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION;
	outState->backendType = backendType;
	outState->stateGeneration = 1u;
	outState->mode = RAL_SKY_ENVIRONMENT_NONE;
	return qtrue;
}

qboolean Ral_SkyEnvironmentRuntimeAdvance( ralSkyEnvironmentRuntimeState_t *state,
		const ralSkyEnvironmentRuntimeRequest_t *request,
		ralSkyEnvironmentPlanRequest_t *outPlanRequest,
		ralSkyEnvironmentRuntimeReceipt_t *outReceipt )
{
	ralSkyEnvironmentRuntimeState_t next;
	ralSkyEnvironmentPlanRequest_t plan;
	ralSkyEnvironmentRuntimeReceipt_t receipt;
	uint32_t dirty = 0u;
	qboolean firstFrame, sourceChanged, hardInvalidation;
	if ( !state || !request || !outPlanRequest || !outReceipt ||
			state->schemaVersion != RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION ||
			request->schemaVersion != RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION ||
			request->backendType != state->backendType ||
			request->frameGeneration == 0u || request->frameGeneration == UINT64_MAX ||
			request->frameGeneration <= state->lastFrameGeneration ||
			request->requestedTier > RAL_ATMOSPHERE_TIER_FULL ||
			request->mode < RAL_SKY_ENVIRONMENT_NONE || request->mode > RAL_SKY_ENVIRONMENT_HYBRID ||
			request->sourceRevision == 0u || request->sourceRevision == UINT64_MAX ||
			( request->eventMask & ~RAL_SKY_EVENT_ALL ) != 0u ||
			( request->completedWorkMask & ~RAL_SKY_DIRTY_ALL ) != 0u ||
			( request->completedWorkMask & ~state->inFlightWorkMask ) != 0u ||
			request->maxWorkItems > RAL_SKY_ENVIRONMENT_MAX_WORK_ITEMS ||
			!isfinite( request->deltaSeconds ) || request->deltaSeconds < 0.0f ||
			!isfinite( request->timelineSeconds ) || request->timelineSeconds < 0.0f ||
			!isfinite( request->crossfadeSeconds ) || request->crossfadeSeconds < 0.0f ||
			!SkyFloatArrayValid( request->sunDirection, 3u, qfalse ) ||
			!SkyFloatArrayValid( request->sunRadiance, 3u, qtrue ) ||
			!SkyFloatArrayValid( request->moonDirection, 3u, qfalse ) ||
			!SkyFloatArrayValid( request->moonRadiance, 3u, qtrue ) ||
			!SkyFloatArrayValid( request->ambientSh, 27u, qfalse ) ||
			!isfinite( request->cloudSunOcclusion ) || request->cloudSunOcclusion < 0.0f ||
			request->cloudSunOcclusion > 1.0f || !isfinite( request->skyLuminance ) ||
			request->skyLuminance < 0.0f || !isfinite( request->exposure ) ||
			request->exposure < 0.0f || !isfinite( request->opticsVisibility ) ||
			request->opticsVisibility < 0.0f || request->opticsVisibility > 1.0f ||
			!BoolValid( request->authoredSkyAvailable ) ||
			!SkyDistantSceneValid( request->distantSceneRequested,
				request->distantSceneIdentity, request->distantSceneOrigin,
				request->distantSceneScale, request->distantSceneLodBias ) ||
			!BoolValid( request->capabilities.physicalLuts ) ||
			!BoolValid( request->capabilities.radianceCapture ) ||
			!BoolValid( request->capabilities.convolution ) ||
			!BoolValid( request->capabilities.cloudLighting ) ||
			!BoolValid( request->capabilities.distantScene ) ||
			!BoolValid( request->capabilities.temporalCrossfade ) ) return qfalse;
	firstFrame = state->lastFrameGeneration == 0u ? qtrue : qfalse;
	sourceChanged = firstFrame || request->sourceRevision != state->sourceRevision ||
		request->mode != state->mode || ( request->eventMask & RAL_SKY_EVENT_SOURCE_CHANGE );
	if ( !firstFrame && ( request->sourceRevision != state->sourceRevision ||
			request->mode != state->mode ) &&
			!( request->eventMask & RAL_SKY_EVENT_SOURCE_CHANGE ) ) return qfalse;
	hardInvalidation = ( request->eventMask & ( RAL_SKY_EVENT_MAP_TRANSITION |
		RAL_SKY_EVENT_DEVICE_RECREATE ) ) != 0u;
	next = *state;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION;
	receipt.backendType = request->backendType;
	receipt.frameGeneration = request->frameGeneration;
	receipt.sourceRevision = request->sourceRevision;
	receipt.sourceChanged = sourceChanged;
	receipt.acceptedCompletionMask = request->completedWorkMask;
	next.inFlightWorkMask &= ~request->completedWorkMask;
	next.completedProductMask |= request->completedWorkMask;
	if ( hardInvalidation || sourceChanged ) {
		receipt.cancelledWorkMask = next.inFlightWorkMask;
		next.inFlightWorkMask = 0u;
		next.pendingDirtyMask = 0u;
		next.completedProductMask = 0u;
	}
	if ( hardInvalidation ) {
		next.currentRadianceRevision = 0u;
		next.previousRadianceRevision = 0u;
		next.targetReady = qfalse;
		next.crossfadeProgress = 0.0f;
		next.completedProductMask = 0u;
	}
	if ( sourceChanged || hardInvalidation ) {
		dirty |= SkyDirtyForMode( request->mode );
		next.sourceRevision = request->sourceRevision;
		next.targetRadianceRevision = request->sourceRevision;
		next.mode = request->mode;
		next.targetReady = qfalse;
		next.crossfadeProgress = 0.0f;
	}
	if ( request->eventMask & RAL_SKY_EVENT_CAMERA_CUT )
		dirty |= request->mode >= RAL_SKY_ENVIRONMENT_PHYSICAL ? RAL_SKY_DIRTY_SKY_VIEW : 0u;
	if ( request->eventMask & RAL_SKY_EVENT_TIME_CHANGE )
		dirty |= request->mode >= RAL_SKY_ENVIRONMENT_PHYSICAL
			? ( RAL_SKY_DIRTY_SKY_VIEW | RAL_SKY_DIRTY_RADIANCE |
				RAL_SKY_DIRTY_AMBIENT_SH | RAL_SKY_DIRTY_SPECULAR_IBL |
				RAL_SKY_DIRTY_CLOUD_LIGHT ) : 0u;
	if ( request->eventMask & RAL_SKY_EVENT_WEATHER_CHANGE )
		dirty |= request->mode >= RAL_SKY_ENVIRONMENT_PHYSICAL
			? ( RAL_SKY_DIRTY_RADIANCE | RAL_SKY_DIRTY_AMBIENT_SH |
				RAL_SKY_DIRTY_SPECULAR_IBL | RAL_SKY_DIRTY_CLOUD_LIGHT ) : 0u;
	if ( dirty ) {
		const uint32_t cancelled = next.inFlightWorkMask & dirty;
		receipt.cancelledWorkMask |= cancelled;
		next.inFlightWorkMask &= ~cancelled;
		next.completedProductMask &= ~dirty;
	}
	next.pendingDirtyMask |= dirty;
	receipt.invalidatedWorkMask = dirty;
	if ( next.targetReady && next.previousRadianceRevision ) {
		if ( next.crossfadeSeconds <= 0.0f ) next.crossfadeProgress = 1.0f;
		else {
			next.crossfadeProgress += request->deltaSeconds / next.crossfadeSeconds;
			if ( next.crossfadeProgress > 1.0f ) next.crossfadeProgress = 1.0f;
		}
		if ( next.crossfadeProgress >= 1.0f ) next.previousRadianceRevision = 0u;
	}
	next.crossfadeSeconds = request->crossfadeSeconds;
	next.timelineSeconds = request->timelineSeconds;
	if ( next.stateGeneration == UINT64_MAX ) return qfalse;
	next.stateGeneration++;
	next.lastFrameGeneration = request->frameGeneration;
	memset( &plan, 0, sizeof( plan ) );
	plan.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
	plan.backendType = request->backendType;
	plan.frameGeneration = request->frameGeneration;
	plan.requestedTier = request->requestedTier;
	plan.mode = request->mode;
	plan.sourceRevision = request->sourceRevision;
	plan.currentRadianceRevision = next.currentRadianceRevision;
	plan.previousRadianceRevision = next.previousRadianceRevision;
	plan.dirtyMask = next.pendingDirtyMask & ~next.inFlightWorkMask;
	plan.inFlightWorkMask = next.inFlightWorkMask;
	plan.completedProductMask = next.completedProductMask;
	plan.eventMask = request->eventMask;
	plan.maxWorkItems = request->maxWorkItems;
	plan.crossfadeSeconds = request->crossfadeSeconds;
	plan.crossfadeProgress = next.crossfadeProgress;
	memcpy( plan.sunDirection, request->sunDirection,
		sizeof( plan.sunDirection ) );
	memcpy( plan.sunRadiance, request->sunRadiance,
		sizeof( plan.sunRadiance ) );
	memcpy( plan.moonDirection, request->moonDirection,
		sizeof( plan.moonDirection ) );
	memcpy( plan.moonRadiance, request->moonRadiance,
		sizeof( plan.moonRadiance ) );
	memcpy( plan.ambientSh, request->ambientSh,
		sizeof( plan.ambientSh ) );
	plan.cloudSunOcclusion = request->cloudSunOcclusion;
	plan.skyLuminance = request->skyLuminance;
	plan.exposure = request->exposure;
	plan.opticsVisibility = request->opticsVisibility;
	plan.authoredSkyAvailable = request->authoredSkyAvailable;
	plan.distantSceneRequested = request->distantSceneRequested;
	plan.distantSceneIdentity = request->distantSceneIdentity;
	memcpy( plan.distantSceneOrigin, request->distantSceneOrigin,
		sizeof( plan.distantSceneOrigin ) );
	plan.distantSceneScale = request->distantSceneScale;
	plan.distantSceneLodBias = request->distantSceneLodBias;
	plan.capabilities = request->capabilities;
	receipt.stateGeneration = next.stateGeneration;
	receipt.targetReady = next.targetReady;
	receipt.ready = qtrue;
	*state = next;
	*outPlanRequest = plan;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkyEnvironmentRuntimeSchedule( ralSkyEnvironmentRuntimeState_t *state,
		const ralSkyEnvironmentPlanReceipt_t *plan )
{
	ralSkyEnvironmentRuntimeState_t next;
	uint32_t retained;
	if ( !state || !plan || state->schemaVersion != RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION ||
			!Ral_SkyEnvironmentPlanReceiptExact( plan, plan ) ||
			plan->backendType != state->backendType || plan->frameGeneration != state->lastFrameGeneration ||
			plan->sourceRevision != state->sourceRevision ||
			( plan->admittedWorkMask & ~state->pendingDirtyMask ) != 0u ||
			( plan->admittedWorkMask & state->inFlightWorkMask ) != 0u ||
			( plan->deferredWorkMask & ~state->pendingDirtyMask ) != 0u ) return qfalse;
	next = *state;
	retained = plan->admittedWorkMask | plan->deferredWorkMask;
	next.pendingDirtyMask &= retained;
	next.pendingDirtyMask &= ~plan->admittedWorkMask;
	next.inFlightWorkMask |= plan->admittedWorkMask;
	if ( next.pendingDirtyMask == 0u && next.inFlightWorkMask == 0u ) {
		if ( next.currentRadianceRevision != next.targetRadianceRevision ) {
			next.previousRadianceRevision = next.currentRadianceRevision;
			next.currentRadianceRevision = next.targetRadianceRevision;
			next.crossfadeProgress = next.previousRadianceRevision ? 0.0f : 1.0f;
		}
		next.targetReady = qtrue;
	}
	*state = next;
	return qtrue;
}

qboolean Ral_SkyEnvironmentRuntimeReceiptExact(
		const ralSkyEnvironmentRuntimeReceipt_t *a,
		const ralSkyEnvironmentRuntimeReceipt_t *b )
{
	return a && b && a->schemaVersion == RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION &&
		a->backendType >= RAL_BACKEND_VULKAN && a->backendType < RAL_BACKEND_COUNT &&
		a->stateGeneration >= 2u && a->frameGeneration > 0u &&
		a->frameGeneration < UINT64_MAX && a->sourceRevision > 0u &&
		a->sourceRevision < UINT64_MAX && a->ready == qtrue &&
		!memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean Ral_AtmospherePlanWeather( const ralAtmosphereWeatherRequest_t *request,
									ralAtmosphereWeatherReceipt_t		*outReceipt )
{
	ralAtmosphereWeatherReceipt_t receipt;
	float						  intensity = 0.0f;
	if ( !request || !outReceipt || request->schemaVersion != RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION ||
		 request->tier < RAL_ATMOSPHERE_TIER_OFF || request->tier > RAL_ATMOSPHERE_TIER_FULL ||
		 request->maxParticles == 0u || request->maxParticles > ( 1u << 24 ) ||
		 request->semanticEmitterCount > RAL_ATMOSPHERE_MAX_EFFECT_WORKLOADS ||
		 request->semanticParticleCount > ( 1u << 24 ) || !isfinite( request->indoorExposure ) ||
		 request->indoorExposure < 0.0f || request->indoorExposure > 1.0f || !BoolValid( request->enabled ) ||
		 !BoolValid( request->heightgridRequested ) || !BoolValid( request->heightgridAvailable ) ||
		 !BoolValid( request->depthIntersectionRequested ) || !BoolValid( request->depthIntersectionAvailable ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
	for ( uint32_t i = 0u; i < 5u; ++i ) {
		if ( !isfinite( request->precipitation[i] ) || request->precipitation[i] < 0.0f ||
			 request->precipitation[i] > 1.0f )
			return qfalse;
		if ( request->precipitation[i] > 0.0f )
			receipt.familyMask |= 1u << i;
		intensity += request->precipitation[i];
	}
	receipt.authoredIntensity = intensity > 1.0f ? 1.0f : intensity;
	if ( request->enabled && request->tier >= RAL_ATMOSPHERE_TIER_WEATHER )
		receipt.admittedCoverage = receipt.authoredIntensity * request->indoorExposure;
	if ( request->enabled && request->tier >= RAL_ATMOSPHERE_TIER_WEATHER ) {
		const uint32_t requestedPrecipitation =
			(uint32_t)ceilf( receipt.admittedCoverage * (float)request->maxParticles );
		const uint32_t semantic			   = request->semanticParticleCount < request->maxParticles
												 ? request->semanticParticleCount
												 : request->maxParticles;
		const uint32_t remaining		   = request->maxParticles - semantic;
		receipt.semanticParticleCount	   = semantic;
		receipt.semanticEmitterCount	   = semantic ? request->semanticEmitterCount : 0u;
		receipt.precipitationParticleCount = requestedPrecipitation < remaining ? requestedPrecipitation : remaining;
		receipt.activeParticleCount		   = receipt.semanticParticleCount + receipt.precipitationParticleCount;
		receipt.droppedParticleCount =
			request->semanticParticleCount - semantic + requestedPrecipitation - receipt.precipitationParticleCount;
		if ( receipt.precipitationParticleCount > 0u ) {
			receipt.heightgridActive = request->heightgridRequested && request->heightgridAvailable;
			receipt.depthIntersectionActive =
				request->depthIntersectionRequested && request->depthIntersectionAvailable;
		}
	}
	if ( receipt.activeParticleCount == 0u )
		receipt.zeroWork = qtrue;
	receipt.ready = qtrue;
	*outReceipt	  = receipt;
	return qtrue;
}

qboolean Ral_AtmosphereWeatherReceiptExact( const ralAtmosphereWeatherReceipt_t *a,
											const ralAtmosphereWeatherReceipt_t *b )
{
	if ( !a || !b || a->schemaVersion != RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION ||
		 ( a->familyMask & ~RAL_ATMOSPHERE_WEATHER_ALL ) || !isfinite( a->authoredIntensity ) ||
		 !isfinite( a->admittedCoverage ) || a->authoredIntensity < 0.0f || a->authoredIntensity > 1.0f ||
		 a->admittedCoverage < 0.0f || a->admittedCoverage > a->authoredIntensity ||
		 !BoolValid( a->heightgridActive ) || !BoolValid( a->depthIntersectionActive ) || !BoolValid( a->zeroWork ) ||
		 a->ready != qtrue ||
		 ( a->zeroWork && ( a->activeParticleCount || a->precipitationParticleCount || a->semanticEmitterCount ||
							a->semanticParticleCount || a->droppedParticleCount || a->heightgridActive ||
							a->depthIntersectionActive ) ) ||
		 ( !a->zeroWork && ( a->activeParticleCount == 0u ||
							 a->activeParticleCount != a->precipitationParticleCount + a->semanticParticleCount ||
							 ( a->semanticParticleCount == 0u && a->semanticEmitterCount != 0u ) ) ) )
		return qfalse;
	return !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

static int SurfaceTileCompare( const ralAtmosphereSurfaceTile_t *a, const ralAtmosphereSurfaceTile_t *b )
{
	if ( a->tileX != b->tileX )
		return a->tileX < b->tileX ? -1 : 1;
	if ( a->tileY != b->tileY )
		return a->tileY < b->tileY ? -1 : 1;
	return 0;
}

qboolean Ral_AtmosphereBuildSurfaceTable( const ralAtmosphereSurfaceTile_t *source, uint32_t sourceCount,
										  ralAtmosphereSurfaceTile_t		 *outEntries,
										  ralAtmosphereSurfaceTableReceipt_t *outReceipt )
{
	ralAtmosphereSurfaceTile_t		   sorted[RAL_ATMOSPHERE_MAX_SURFACE_TILES];
	ralAtmosphereSurfaceTableReceipt_t receipt;
	uint32_t						   i;
	if ( !outEntries || !outReceipt || sourceCount > RAL_ATMOSPHERE_MAX_SURFACE_TILES ||
		 ( sourceCount > 0u && !source ) )
		return qfalse;
	memset( sorted, 0, sizeof( sorted ) );
	memset( &receipt, 0, sizeof( receipt ) );
	for ( i = 0u; i < sourceCount; ++i ) {
		uint32_t position = i;
		if ( source[i].reserved != 0u )
			return qfalse;
		for ( uint32_t channel = 0u; channel < 4u; ++channel )
			if ( !isfinite( source[i].climate[channel] ) || source[i].climate[channel] < 0.0f ||
				 source[i].climate[channel] > 1.0f )
				return qfalse;
		sorted[position] = source[i];
		while ( position > 0u && SurfaceTileCompare( &sorted[position], &sorted[position - 1u] ) < 0 ) {
			ralAtmosphereSurfaceTile_t swap = sorted[position - 1u];
			sorted[position - 1u]			= sorted[position];
			sorted[position]				= swap;
			position--;
		}
		if ( position > 0u && SurfaceTileCompare( &sorted[position], &sorted[position - 1u] ) == 0 )
			return qfalse;
		if ( position + 1u < i + 1u && SurfaceTileCompare( &sorted[position], &sorted[position + 1u] ) == 0 )
			return qfalse;
	}
	receipt.sourceCount		= sourceCount;
	receipt.admittedCount	= sourceCount;
	receipt.comparisonLimit = 8u;
	receipt.byteCount		= sourceCount * (uint32_t)sizeof( sorted[0] );
	memcpy( outEntries, sorted, sizeof( sorted ) );
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_AtmospherePlan( const ralAtmospherePlanRequest_t *request, ralAtmospherePlanReceipt_t *outReceipt )
{
	ralAtmospherePlanReceipt_t receipt;
	uint64_t				   xy;
	if ( !outReceipt || !RequestValid( request ) )
		return qfalse;
	if ( request->skyEnvironment.schemaVersion != 0u &&
			( request->skyEnvironment.backendType != request->backendType ||
			  request->skyEnvironment.frameGeneration != request->frameGeneration ||
			  request->skyEnvironment.requestedTier != (uint32_t)request->requestedTier ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion	= RAL_ATMOSPHERE_RECEIPT_SCHEMA_VERSION;
	receipt.backendType		= request->backendType;
	receipt.frameGeneration = request->frameGeneration;
	receipt.requestedTier	= request->requestedTier;
	receipt.selectedTier	= request->requestedTier;
	if ( request->skyEnvironment.schemaVersion != 0u &&
			!Ral_SkyEnvironmentPlan( &request->skyEnvironment,
				&receipt.skyEnvironment ) ) return qfalse;
	if ( request->requestedTier == RAL_ATMOSPHERE_TIER_OFF ) {
		receipt.zeroWork = qtrue;
		receipt.ready	 = qtrue;
		*outReceipt		 = receipt;
		return qtrue;
	}
	if ( !request->capabilities.analyticComposite ) {
		receipt.selectedTier   = RAL_ATMOSPHERE_TIER_OFF;
		receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_BACKEND_UNAVAILABLE;
		receipt.zeroWork	   = qtrue;
		receipt.ready		   = qtrue;
		*outReceipt			   = receipt;
		return qtrue;
	}
	if ( receipt.selectedTier == RAL_ATMOSPHERE_TIER_FULL && !request->capabilities.compute ) {
		receipt.selectedTier   = RAL_ATMOSPHERE_TIER_WEATHER;
		receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_NO_COMPUTE;
	} else if ( receipt.selectedTier == RAL_ATMOSPHERE_TIER_FULL && !request->capabilities.storageBuffers ) {
		receipt.selectedTier   = RAL_ATMOSPHERE_TIER_WEATHER;
		receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_NO_STORAGE_BUFFER;
	}
	if ( receipt.selectedTier < RAL_ATMOSPHERE_TIER_FULL || !request->mediaActive ) {
		receipt.passMask	   = RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE;
		receipt.compositeCount = 1u;
		if ( request->cloudsRequested && receipt.fallbackReason == RAL_ATMOSPHERE_FALLBACK_NONE )
			receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_CLOUD_UNSUPPORTED;
		receipt.ready = qtrue;
		*outReceipt	  = receipt;
		return qtrue;
	}
	if ( request->maxFroxelCount == 0u ) {
		receipt.selectedTier   = RAL_ATMOSPHERE_TIER_WEATHER;
		receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_FROXEL_BUDGET;
		receipt.passMask	   = RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE;
		receipt.compositeCount = 1u;
		receipt.ready		   = qtrue;
		*outReceipt			   = receipt;
		return qtrue;
	}
	receipt.froxelWidth	 = ( request->width + RAL_ATMOSPHERE_FROXEL_TILE_SIZE - 1u ) / RAL_ATMOSPHERE_FROXEL_TILE_SIZE;
	receipt.froxelHeight = ( request->height + RAL_ATMOSPHERE_FROXEL_TILE_SIZE - 1u ) / RAL_ATMOSPHERE_FROXEL_TILE_SIZE;
	xy					 = (uint64_t)receipt.froxelWidth * receipt.froxelHeight;
	receipt.froxelDepth	 = RAL_ATMOSPHERE_MAX_SLICES;
	while ( receipt.froxelDepth > RAL_ATMOSPHERE_MIN_SLICES && xy * receipt.froxelDepth > request->maxFroxelCount )
		receipt.froxelDepth /= 2u;
	if ( xy * receipt.froxelDepth > request->maxFroxelCount || xy * receipt.froxelDepth > UINT32_MAX ) {
		receipt.selectedTier   = RAL_ATMOSPHERE_TIER_WEATHER;
		receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_FROXEL_BUDGET;
		receipt.froxelWidth = receipt.froxelHeight = receipt.froxelDepth = 0u;
		receipt.passMask												 = RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE;
		receipt.compositeCount											 = 1u;
		receipt.ready													 = qtrue;
		*outReceipt														 = receipt;
		return qtrue;
	}
	receipt.froxelCount				   = (uint32_t)( xy * receipt.froxelDepth );
	receipt.admittedVolumeCount		   = MinU32( request->localVolumeCount, request->maxLocalVolumes );
	receipt.droppedVolumeCount		   = request->localVolumeCount - receipt.admittedVolumeCount;
	receipt.admittedLightCount		   = MinU32( request->lightCount, request->maxLights );
	receipt.droppedLightCount		   = request->lightCount - receipt.admittedLightCount;
	receipt.admittedShadowedLightCount = MinU32( request->shadowedLightCount, request->maxShadowedLights );
	if ( !request->capabilities.volumetricShadows )
		receipt.admittedShadowedLightCount = 0u;
	receipt.droppedShadowedLightCount = request->shadowedLightCount - receipt.admittedShadowedLightCount;
	receipt.passMask				  = RAL_ATMOSPHERE_PASS_MEDIA_INJECT | RAL_ATMOSPHERE_PASS_LIGHT_INJECT |
					   RAL_ATMOSPHERE_PASS_FROXEL_INTEGRATE | RAL_ATMOSPHERE_PASS_SINGLE_COMPOSITE;
	receipt.computeDispatchCount = 3u;
	receipt.compositeCount		 = 1u;
	if ( receipt.admittedShadowedLightCount > 0u )
		receipt.passMask |= RAL_ATMOSPHERE_PASS_SHADOW_INJECT;
	if ( request->capabilities.temporalHistory ) {
		receipt.passMask |= RAL_ATMOSPHERE_PASS_TEMPORAL_REPROJECT;
		/* Temporal reconstruction is fused into froxel integration; keeping it
		 * in the pass mask exposes the semantic without inventing a dispatch. */
		if ( request->historyValid && !request->cameraCut && !request->deviceRecreated )
			receipt.temporalReuseCount = 1u;
		else
			receipt.temporalRejectCount = 1u;
	}
	if ( request->cloudsRequested ) {
		if ( request->capabilities.fullClouds ) {
			receipt.passMask |= RAL_ATMOSPHERE_PASS_CLOUDS;
			receipt.cloudsActive = qtrue;
			receipt.computeDispatchCount++;
		} else {
			receipt.fallbackReason = RAL_ATMOSPHERE_FALLBACK_CLOUD_UNSUPPORTED;
		}
	}
	receipt.ready = qtrue;
	*outReceipt	  = receipt;
	return qtrue;
}

qboolean Ral_AtmospherePlanReceiptExact( const ralAtmospherePlanReceipt_t *a, const ralAtmospherePlanReceipt_t *b )
{
	if ( !a || !b || a->schemaVersion != RAL_ATMOSPHERE_RECEIPT_SCHEMA_VERSION || a->backendType < RAL_BACKEND_VULKAN ||
		 a->backendType >= RAL_BACKEND_COUNT || a->frameGeneration == 0u || a->frameGeneration == UINT64_MAX ||
		 a->requestedTier > RAL_ATMOSPHERE_TIER_FULL || a->selectedTier > a->requestedTier || a->compositeCount > 1u ||
		 ( a->zeroWork && ( a->passMask || a->computeDispatchCount || a->compositeCount ) ) || a->ready != qtrue )
		 return qfalse;
	if ( a->skyEnvironment.schemaVersion != 0u &&
			!Ral_SkyEnvironmentPlanReceiptExact( &a->skyEnvironment,
				&b->skyEnvironment ) ) return qfalse;
	return !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean Ral_AtmospherePlanUnavailable( ralBackendType_t backendType, uint64_t frameGeneration,
										ralAtmosphereTier_t requestedTier, ralAtmospherePlanReceipt_t *outReceipt )
{
	ralAtmospherePlanRequest_t request;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion	= RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType		= backendType;
	request.frameGeneration = frameGeneration;
	request.width			= 1u;
	request.height			= 1u;
	request.requestedTier	= requestedTier;
	request.maxLocalVolumes = 1u;
	request.maxLights		= 1u;
	return Ral_AtmospherePlan( &request, outReceipt );
}

qboolean Ral_AtmosphereRuntimeInit( ralBackendType_t backendType, ralAtmosphereRuntimeState_t *outState )
{
	if ( !outState || backendType < RAL_BACKEND_VULKAN || backendType >= RAL_BACKEND_COUNT )
		return qfalse;
	memset( outState, 0, sizeof( *outState ) );
	outState->schemaVersion	  = RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION;
	outState->backendType	  = backendType;
	outState->stateGeneration = 1u;
	return qtrue;
}

qboolean Ral_AtmosphereSkyEventMask( uint32_t invalidationMask,
		uint32_t *outSkyEventMask )
{
	static const uint32_t knownInvalidations =
		RAL_ATMOSPHERE_INVALIDATE_RESIZE |
		RAL_ATMOSPHERE_INVALIDATE_CAMERA |
		RAL_ATMOSPHERE_INVALIDATE_MAP |
		RAL_ATMOSPHERE_INVALIDATE_DEVICE |
		RAL_ATMOSPHERE_INVALIDATE_CAPABILITY |
		RAL_ATMOSPHERE_INVALIDATE_TIMELINE |
		RAL_ATMOSPHERE_INVALIDATE_SUN |
		RAL_ATMOSPHERE_INVALIDATE_WEATHER;
	uint32_t skyEvents = 0u;
	if ( !outSkyEventMask || ( invalidationMask & ~knownInvalidations ) )
		return qfalse;
	if ( invalidationMask & RAL_ATMOSPHERE_INVALIDATE_CAMERA )
		skyEvents |= RAL_SKY_EVENT_CAMERA_CUT;
	if ( invalidationMask & RAL_ATMOSPHERE_INVALIDATE_MAP )
		skyEvents |= RAL_SKY_EVENT_MAP_TRANSITION;
	if ( invalidationMask & RAL_ATMOSPHERE_INVALIDATE_DEVICE )
		skyEvents |= RAL_SKY_EVENT_DEVICE_RECREATE;
	if ( invalidationMask & RAL_ATMOSPHERE_INVALIDATE_TIMELINE )
		skyEvents |= RAL_SKY_EVENT_TIME_CHANGE;
	if ( invalidationMask & RAL_ATMOSPHERE_INVALIDATE_SUN )
		skyEvents |= RAL_SKY_EVENT_TIME_CHANGE;
	if ( invalidationMask & RAL_ATMOSPHERE_INVALIDATE_WEATHER )
		skyEvents |= RAL_SKY_EVENT_WEATHER_CHANGE;
	*outSkyEventMask = skyEvents;
	return qtrue;
}

qboolean Ral_AtmosphereRuntimeAdvance( ralAtmosphereRuntimeState_t *state, const ralAtmosphereRuntimeRequest_t *request,
									   ralAtmosphereRuntimeReceipt_t *outReceipt )
{
	static const uint32_t knownEvents =
		RAL_ATMOSPHERE_EVENT_RESIZE | RAL_ATMOSPHERE_EVENT_CAMERA_CUT | RAL_ATMOSPHERE_EVENT_MAP_TRANSITION |
		RAL_ATMOSPHERE_EVENT_PAUSE | RAL_ATMOSPHERE_EVENT_RESUME | RAL_ATMOSPHERE_EVENT_DEVICE_LOST |
		RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED | RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE |
		RAL_ATMOSPHERE_EVENT_SUN_CHANGE | RAL_ATMOSPHERE_EVENT_WEATHER_TRANSITION;
	ralAtmosphereRuntimeState_t	  next;
	ralAtmosphereRuntimeReceipt_t receipt;
	uint32_t					  invalidation = 0u;
	qboolean					  firstFrame;
	if ( !state || !request || !outReceipt || state->schemaVersion != RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION ||
		 request->schemaVersion != RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION ||
		 request->backendType != state->backendType || request->frameGeneration == 0u ||
		 request->frameGeneration == UINT64_MAX || request->frameGeneration <= state->lastFrameGeneration ||
		 request->deviceGeneration == 0u || request->deviceGeneration == UINT64_MAX || request->width == 0u ||
		 request->width > 65536u || request->height == 0u || request->height > 65536u ||
		 !isfinite( request->timelineSeconds ) || request->timelineSeconds < 0.0f || !isfinite( request->timeScale ) ||
		 request->timeScale < 0.0f || request->timeScale > 16.0f || request->climateSeed == 0u ||
		 request->capabilityDigest == 0u || ( request->eventMask & ~knownEvents ) ||
		 ( ( request->eventMask & RAL_ATMOSPHERE_EVENT_PAUSE ) && request->timeScale != 0.0f ) ||
		 ( ( request->eventMask & RAL_ATMOSPHERE_EVENT_RESUME ) && request->timeScale == 0.0f ) ||
		 ( request->eventMask & RAL_ATMOSPHERE_EVENT_PAUSE && request->eventMask & RAL_ATMOSPHERE_EVENT_RESUME ) ||
		 ( request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_LOST &&
		   request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED ) )
		return qfalse;
	firstFrame = state->lastFrameGeneration == 0u ? qtrue : qfalse;
	if ( !firstFrame && !state->deviceReady && !( request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED ) )
		return qfalse;
	if ( !firstFrame && request->timelineSeconds < state->timelineSeconds &&
		 !( request->eventMask & RAL_ATMOSPHERE_EVENT_MAP_TRANSITION ) )
		return qfalse;
	if ( !firstFrame && ( request->width != state->width || request->height != state->height ) &&
		 !( request->eventMask & RAL_ATMOSPHERE_EVENT_RESIZE ) )
		return qfalse;
	if ( !firstFrame && request->capabilityDigest != state->capabilityDigest &&
		 !( request->eventMask & RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE ) &&
		 !( request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED ) )
		return qfalse;
	if ( !firstFrame && request->deviceGeneration != state->deviceGeneration &&
		 !( request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED ) )
		return qfalse;
	/* Sun/weather identity drift must be declared (or carried by a map
	 * transition); silent drift is rejected output-atomically. */
	if ( !firstFrame && request->sunDigest != state->sunDigest &&
		 !( request->eventMask & ( RAL_ATMOSPHERE_EVENT_SUN_CHANGE | RAL_ATMOSPHERE_EVENT_MAP_TRANSITION ) ) )
		return qfalse;
	if ( !firstFrame && request->weatherDigest != state->weatherDigest &&
		 !( request->eventMask & ( RAL_ATMOSPHERE_EVENT_WEATHER_TRANSITION | RAL_ATMOSPHERE_EVENT_MAP_TRANSITION ) ) )
		return qfalse;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_SUN_CHANGE )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_SUN;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_WEATHER_TRANSITION )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_WEATHER;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_RESIZE )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_RESIZE;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_CAMERA_CUT )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_CAMERA;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_MAP_TRANSITION )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_MAP | RAL_ATMOSPHERE_INVALIDATE_TIMELINE;
	if ( request->eventMask & ( RAL_ATMOSPHERE_EVENT_DEVICE_LOST | RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED ) )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_DEVICE;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE )
		invalidation |= RAL_ATMOSPHERE_INVALIDATE_CAPABILITY;
	next = *state;
	if ( next.stateGeneration == UINT64_MAX )
		return qfalse;
	next.stateGeneration++;
	next.lastFrameGeneration = request->frameGeneration;
	next.deviceGeneration	 = request->deviceGeneration;
	next.width				 = request->width;
	next.height				 = request->height;
	next.timelineSeconds	 = request->timelineSeconds;
	next.climateSeed		 = request->climateSeed;
	next.capabilityDigest	 = request->capabilityDigest;
	next.sunDigest			 = request->sunDigest;
	next.weatherDigest		 = request->weatherDigest;
	if ( firstFrame || request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED )
		next.deviceReady = qtrue;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_DEVICE_LOST )
		next.deviceReady = qfalse;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_PAUSE )
		next.paused = qtrue;
	if ( request->eventMask & RAL_ATMOSPHERE_EVENT_RESUME )
		next.paused = qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion		= RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION;
	receipt.backendType			= request->backendType;
	receipt.stateGeneration		= next.stateGeneration;
	receipt.frameGeneration		= request->frameGeneration;
	receipt.deviceGeneration	= request->deviceGeneration;
	receipt.invalidationMask	= invalidation;
	receipt.renderAllowed		= next.deviceReady && !next.paused;
	receipt.historyReusable		= state->historyValid && invalidation == 0u && receipt.renderAllowed;
	receipt.zeroTimeStep		= request->timeScale == 0.0f;
	receipt.replayDeterministic = request->climateSeed != 0u;
	if ( invalidation != 0u )
		next.historyValid = qfalse;
	if ( receipt.renderAllowed ) {
		if ( next.historyGeneration == UINT64_MAX )
			return qfalse;
		next.historyGeneration++;
		receipt.historyGeneration = next.historyGeneration;
		next.historyValid		  = qtrue;
	} else
		receipt.historyGeneration = next.historyGeneration;
	receipt.ready = qtrue;
	*state		  = next;
	*outReceipt	  = receipt;
	return qtrue;
}

qboolean Ral_AtmosphereRuntimeReceiptExact( const ralAtmosphereRuntimeReceipt_t *a,
											const ralAtmosphereRuntimeReceipt_t *b )
{
	if ( !a || !b || a->schemaVersion != RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION || a->backendType < RAL_BACKEND_VULKAN ||
		 a->backendType >= RAL_BACKEND_COUNT || a->stateGeneration < 2u || a->frameGeneration == 0u ||
		 a->frameGeneration == UINT64_MAX || a->deviceGeneration == 0u || a->deviceGeneration == UINT64_MAX ||
		 a->replayDeterministic != qtrue || a->ready != qtrue )
		return qfalse;
	return !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}
