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
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion	= RAL_ATMOSPHERE_RECEIPT_SCHEMA_VERSION;
	receipt.backendType		= request->backendType;
	receipt.frameGeneration = request->frameGeneration;
	receipt.requestedTier	= request->requestedTier;
	receipt.selectedTier	= request->requestedTier;
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

qboolean Ral_AtmosphereRuntimeAdvance( ralAtmosphereRuntimeState_t *state, const ralAtmosphereRuntimeRequest_t *request,
									   ralAtmosphereRuntimeReceipt_t *outReceipt )
{
	static const uint32_t knownEvents =
		RAL_ATMOSPHERE_EVENT_RESIZE | RAL_ATMOSPHERE_EVENT_CAMERA_CUT | RAL_ATMOSPHERE_EVENT_MAP_TRANSITION |
		RAL_ATMOSPHERE_EVENT_PAUSE | RAL_ATMOSPHERE_EVENT_RESUME | RAL_ATMOSPHERE_EVENT_DEVICE_LOST |
		RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED | RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE;
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
