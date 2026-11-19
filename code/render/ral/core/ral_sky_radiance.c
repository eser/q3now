// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_sky_radiance.h"

#include <limits.h>
#include <math.h>
#include <string.h>

qboolean Ral_SkyRadianceCaptureAxes( uint32_t face, float outAxes[3][3] )
{
	static const float axes[6][3][3] = {
		{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } },
		{ { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } },
		{ { -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 } },
		{ { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
		{ { 0, 0, 1 }, { 0, 1, 0 }, { -1, 0, 0 } },
		{ { 0, 0, -1 }, { 0, 1, 0 }, { 1, 0, 0 } }
	};
	if ( !outAxes || face >= RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT ) return qfalse;
	memcpy( outAxes, axes[face], sizeof( axes[face] ) );
	return qtrue;
}

qboolean Ral_SkyRadianceSourceParamsBuild(
		const ralSkyEnvironmentPlanReceipt_t *sky, float cloudCoverage,
		ralSkyRadianceSourceType_t sourceType, ralSkyRadianceSourceParams_t *outParams )
{
	ralSkyRadianceSourceParams_t params;
	if ( !outParams || !Ral_SkyEnvironmentPlanReceiptExact( sky, sky )
			|| !isfinite( cloudCoverage ) || cloudCoverage < 0.0f
			|| cloudCoverage > 1.0f
			|| sourceType < RAL_SKY_SOURCE_ANALYTIC
			|| sourceType > RAL_SKY_SOURCE_COMPOSITED_RADIANCE )
		return qfalse;
	memset( &params, 0, sizeof( params ) );
	params.meta[1] = (uint32_t)sky->selectedMode;
	params.meta[2] = ( sky->passMask & RAL_SKY_PASS_AUTHORED_BACKGROUND ) ? 1u : 0u;
	params.meta[3] = sourceType != RAL_SKY_SOURCE_ANALYTIC ? 1u : 0u;
	memcpy( params.authoredRadiance, sky->ambientSh, 3u * sizeof( float ) );
	params.authoredRadiance[3] = sky->skyLuminance;
	memcpy( params.sunDirectionIntensity, sky->sunDirection, 3u * sizeof( float ) );
	params.sunDirectionIntensity[3] = 1.0f - cloudCoverage * sky->cloudSunOcclusion;
	memcpy( params.sunRadianceExposure, sky->sunRadiance, 3u * sizeof( float ) );
	params.sunRadianceExposure[3] = sky->exposure;
	if ( sourceType == RAL_SKY_SOURCE_COMPOSITED_RADIANCE ) {
		/* source_sky only changes face convention for this source. Preserve
		 * its radiometry exactly: no second sun lobe or exposure multiplier.
		 * The existing uniform ABI expresses this without a shader variant. */
		params.sunDirectionIntensity[3] = 0.0f;
		memset( params.sunRadianceExposure, 0, 3u * sizeof( float ) );
		params.sunRadianceExposure[3] = 1.0f;
	}
	*outParams = params;
	return qtrue;
}

static qboolean BackendValid( ralBackendType_t backendType )
{
	return backendType >= RAL_BACKEND_VULKAN && backendType < RAL_BACKEND_COUNT;
}

static uint32_t SpecularMask( uint32_t mipCount )
{
	return ( ( 1u << mipCount ) - 1u ) << 15u;
}

static uint32_t RequiredMask( uint32_t mipCount )
{
	return RAL_SKY_RADIANCE_CAPTURE_MASK |
		RAL_SKY_RADIANCE_AMBIENT_SH_MASK | SpecularMask( mipCount );
}

static qboolean StateValid( const ralSkyRadianceState_t *state )
{
	const uint32_t required = state ? RequiredMask( state->specularMipCount ) : 0u;
	return state && state->schemaVersion == RAL_SKY_RADIANCE_SCHEMA_VERSION &&
		BackendValid( state->backendType ) && state->stateGeneration > 0u &&
		state->stateGeneration < UINT64_MAX &&
		state->nextProductRevision > 0u && state->nextProductRevision < UINT64_MAX &&
		state->specularMipCount > 0u &&
		state->specularMipCount <= RAL_SKY_RADIANCE_MAX_SPECULAR_MIPS &&
		( ( state->pendingSliceMask | state->inFlightSliceMask |
			state->completedSliceMask ) & ~required ) == 0u &&
		( state->pendingSliceMask & state->inFlightSliceMask ) == 0u &&
		( state->pendingSliceMask & state->completedSliceMask ) == 0u &&
		( state->inFlightSliceMask & state->completedSliceMask ) == 0u &&
		isfinite( state->crossfadeSeconds ) && state->crossfadeSeconds >= 0.0f &&
		isfinite( state->crossfadeProgress ) && state->crossfadeProgress >= 0.0f &&
		state->crossfadeProgress <= 1.0f &&
		isfinite( state->secondsSinceCapture ) && state->secondsSinceCapture >= 0.0f &&
		( state->targetReady == qfalse || state->targetReady == qtrue );
}

static qboolean RequestValid( const ralSkyRadianceRequest_t *request )
{
	return request && request->schemaVersion == RAL_SKY_RADIANCE_SCHEMA_VERSION &&
		BackendValid( request->backendType ) && request->frameGeneration > 0u &&
		request->frameGeneration < UINT64_MAX && request->sourceRevision > 0u &&
		request->sourceRevision < UINT64_MAX &&
		request->mode >= RAL_SKY_ENVIRONMENT_AUTHORED &&
		request->mode <= RAL_SKY_ENVIRONMENT_HYBRID &&
		request->specularMipCount > 0u &&
		request->specularMipCount <= RAL_SKY_RADIANCE_MAX_SPECULAR_MIPS &&
		( request->eventMask & ~RAL_SKY_EVENT_ALL ) == 0u &&
		( request->completedSliceMask & ~RAL_SKY_RADIANCE_ALL_SLICE_MASK ) == 0u &&
		( request->completedSliceMask == 0u || request->completedProductRevision > 0u ) &&
		request->maxSlicesPerFrame <= RAL_SKY_RADIANCE_MAX_SLICES_PER_FRAME &&
		isfinite( request->deltaSeconds ) && request->deltaSeconds >= 0.0f &&
		isfinite( request->crossfadeSeconds ) && request->crossfadeSeconds >= 0.0f &&
		isfinite( request->refreshIntervalSeconds ) && request->refreshIntervalSeconds >= 0.0f;
}

static uint32_t AdmitLowestBits( uint32_t mask, uint32_t budget )
{
	uint32_t admitted = 0u;
	for ( uint32_t bit = 1u; mask && budget && bit <= RAL_SKY_RADIANCE_SPECULAR_MASK;
			bit <<= 1u ) {
		if ( mask & bit ) {
			admitted |= bit;
			mask &= ~bit;
			budget--;
		}
	}
	return admitted;
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

qboolean Ral_SkyRadianceInit( ralBackendType_t backendType,
		ralSkyRadianceState_t *outState )
{
	ralSkyRadianceState_t state;
	if ( !outState || !BackendValid( backendType ) ) return qfalse;
	memset( &state, 0, sizeof( state ) );
	state.schemaVersion = RAL_SKY_RADIANCE_SCHEMA_VERSION;
	state.backendType = backendType;
	state.stateGeneration = 1u;
	state.nextProductRevision = 1u;
	state.mode = RAL_SKY_ENVIRONMENT_AUTHORED;
	state.specularMipCount = 1u;
	state.crossfadeProgress = 1.0f;
	*outState = state;
	return qtrue;
}

qboolean Ral_SkyRadianceAdvance( ralSkyRadianceState_t *state,
		const ralSkyRadianceRequest_t *request,
		ralSkyRadianceReceipt_t *outReceipt )
{
	ralSkyRadianceState_t next;
	ralSkyRadianceReceipt_t receipt;
	uint32_t required, eligible, admitted;
	qboolean first, configChanged, explicitInvalidation, hardInvalidation, refresh;
	if ( !state || !outReceipt || !StateValid( state ) || !RequestValid( request ) ||
		state->backendType != request->backendType ||
		request->frameGeneration <= state->lastFrameGeneration ||
		state->stateGeneration >= UINT64_MAX - 1u ) return qfalse;

	next = *state;
	memset( &receipt, 0, sizeof( receipt ) );
	required = RequiredMask( request->specularMipCount );
	first = next.sourceRevision == 0u;
	configChanged = next.sourceRevision != request->sourceRevision ||
		next.mode != request->mode || next.specularMipCount != request->specularMipCount;
	explicitInvalidation = ( request->eventMask & ( RAL_SKY_EVENT_CAMERA_CUT |
		RAL_SKY_EVENT_MAP_TRANSITION |
		RAL_SKY_EVENT_DEVICE_RECREATE | RAL_SKY_EVENT_SOURCE_CHANGE |
		RAL_SKY_EVENT_TIME_CHANGE | RAL_SKY_EVENT_WEATHER_CHANGE ) ) != 0u;
	hardInvalidation = ( request->eventMask & ( RAL_SKY_EVENT_MAP_TRANSITION |
		RAL_SKY_EVENT_DEVICE_RECREATE ) ) != 0u;
	if ( configChanged && !first && !explicitInvalidation ) return qfalse;
	if ( next.previousProductRevision ) {
		if ( next.crossfadeSeconds <= 0.0f ) next.crossfadeProgress = 1.0f;
		else {
			next.crossfadeProgress += request->deltaSeconds / next.crossfadeSeconds;
			if ( next.crossfadeProgress > 1.0f ) next.crossfadeProgress = 1.0f;
		}
		if ( next.crossfadeProgress >= 1.0f ) next.previousProductRevision = 0u;
	}

	/* Saturate elapsed time: long stalls never queue a backlog of captures.
	 * An immutable source and pause both produce zero periodic work. */
	next.secondsSinceCapture = request->refreshIntervalSeconds > 0.0f
		? fminf( request->refreshIntervalSeconds,
			next.secondsSinceCapture + request->deltaSeconds ) : 0.0f;
	refresh = !first && !configChanged && !explicitInvalidation
		&& next.targetReady && !next.previousProductRevision
		&& request->maxSlicesPerFrame > 0u && request->deltaSeconds > 0.0f
		&& request->refreshIntervalSeconds > 0.0f
		&& next.secondsSinceCapture >= request->refreshIntervalSeconds;
	if ( ( first || configChanged || explicitInvalidation || refresh ) &&
		next.nextProductRevision >= UINT64_MAX - 1u ) return qfalse;

	if ( first || configChanged || explicitInvalidation || refresh ) {
		receipt.sourceChanged = configChanged || first;
		receipt.refreshStarted = refresh;
		receipt.cancelledSliceMask = next.inFlightSliceMask;
		receipt.staleCompletionMask = request->completedSliceMask;
		if ( hardInvalidation ) {
			next.currentProductRevision = 0u;
			next.previousProductRevision = 0u;
			next.crossfadeProgress = 1.0f;
		}
		next.sourceRevision = request->sourceRevision;
		next.mode = request->mode;
		next.specularMipCount = request->specularMipCount;
		next.targetProductRevision = next.nextProductRevision++;
		next.pendingSliceMask = required;
		next.inFlightSliceMask = 0u;
		next.completedSliceMask = 0u;
		next.targetReady = qfalse;
		next.secondsSinceCapture = 0.0f;
		/* A replacement request cannot retime the currently visible fade.
		 * Its duration takes effect when the replacement is published. */
		if ( !next.previousProductRevision )
			next.crossfadeSeconds = request->crossfadeSeconds;
	} else if ( request->completedSliceMask ) {
		if ( request->completedProductRevision != next.targetProductRevision )
			receipt.staleCompletionMask = request->completedSliceMask;
		else {
			if ( request->completedSliceMask & ~next.inFlightSliceMask ) return qfalse;
			receipt.acceptedCompletionMask = request->completedSliceMask;
			next.inFlightSliceMask &= ~request->completedSliceMask;
			next.completedSliceMask |= request->completedSliceMask;
		}
	}

	if ( next.completedSliceMask == required && next.inFlightSliceMask == 0u &&
		!next.targetReady ) {
		next.previousProductRevision = next.currentProductRevision;
		next.currentProductRevision = next.targetProductRevision;
		next.targetReady = qtrue;
		next.crossfadeSeconds = request->crossfadeSeconds;
		next.crossfadeProgress = next.previousProductRevision && next.crossfadeSeconds > 0.0f
			? 0.0f : 1.0f;
		receipt.targetPublished = qtrue;
	}

	eligible = 0u;
	/* Every adapter has two product slots. During a crossfade the writable
	 * target slot still owns the sampled previous SH/irradiance/specular
	 * cohort. Keep the latest revision pending until that reader retires;
	 * no extra allocation, GPU wait or per-backend ownership workaround. */
	if ( !next.targetReady && !next.previousProductRevision ) {
		if ( ( next.completedSliceMask & RAL_SKY_RADIANCE_CAPTURE_MASK ) !=
				RAL_SKY_RADIANCE_CAPTURE_MASK )
			eligible = next.pendingSliceMask & RAL_SKY_RADIANCE_CAPTURE_MASK;
		else eligible = next.pendingSliceMask &
			( RAL_SKY_RADIANCE_AMBIENT_SH_MASK | SpecularMask( next.specularMipCount ) );
	}
	admitted = AdmitLowestBits( eligible, request->maxSlicesPerFrame );
	next.pendingSliceMask &= ~admitted;
	next.inFlightSliceMask |= admitted;
	next.lastFrameGeneration = request->frameGeneration;
	next.stateGeneration++;

	receipt.schemaVersion = RAL_SKY_RADIANCE_RECEIPT_SCHEMA_VERSION;
	receipt.backendType = next.backendType;
	receipt.stateGeneration = next.stateGeneration;
	receipt.frameGeneration = request->frameGeneration;
	receipt.sourceRevision = next.sourceRevision;
	receipt.targetProductRevision = next.targetProductRevision;
	receipt.currentProductRevision = next.currentProductRevision;
	receipt.previousProductRevision = next.previousProductRevision;
	receipt.requiredSliceMask = required;
	receipt.admittedSliceMask = admitted;
	receipt.pendingSliceMask = next.pendingSliceMask;
	receipt.inFlightSliceMask = next.inFlightSliceMask;
	receipt.completedSliceMask = next.completedSliceMask;
	receipt.admittedSliceCount = CountBits( receipt.admittedSliceMask );
	receipt.acceptedCompletionCount = CountBits(
		receipt.acceptedCompletionMask );
	receipt.staleCompletionCount = CountBits( receipt.staleCompletionMask );
	receipt.cancelledSliceCount = CountBits( receipt.cancelledSliceMask );
	receipt.pendingSliceCount = CountBits( receipt.pendingSliceMask );
	receipt.inFlightSliceCount = CountBits( receipt.inFlightSliceMask );
	receipt.completedSliceCount = CountBits( receipt.completedSliceMask );
	receipt.crossfadeAlpha = next.previousProductRevision ? next.crossfadeProgress : 1.0f;
	receipt.previousProductValid = next.previousProductRevision ? qtrue : qfalse;
	receipt.targetReady = next.targetReady;
	receipt.zeroWork = next.pendingSliceMask == 0u && next.inFlightSliceMask == 0u;
	receipt.ready = qtrue;
	*state = next;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkyRadianceReceiptValid(
		const ralSkyRadianceReceipt_t *receipt )
{
	const uint32_t productMask = receipt ? receipt->pendingSliceMask |
		receipt->inFlightSliceMask | receipt->completedSliceMask : 0u;
	if ( !receipt ||
			receipt->schemaVersion != RAL_SKY_RADIANCE_RECEIPT_SCHEMA_VERSION ||
			!BackendValid( receipt->backendType ) ||
			!receipt->stateGeneration || receipt->stateGeneration == UINT64_MAX ||
			!receipt->frameGeneration || receipt->frameGeneration == UINT64_MAX ||
			!receipt->sourceRevision || receipt->sourceRevision == UINT64_MAX ||
			!receipt->targetProductRevision ||
			receipt->targetProductRevision == UINT64_MAX ||
			receipt->currentProductRevision == UINT64_MAX ||
			receipt->previousProductRevision == UINT64_MAX ||
			receipt->ready != qtrue ||
			( receipt->requiredSliceMask & ~RAL_SKY_RADIANCE_ALL_SLICE_MASK ) ||
			( receipt->requiredSliceMask & ( RAL_SKY_RADIANCE_CAPTURE_MASK |
				RAL_SKY_RADIANCE_AMBIENT_SH_MASK ) ) !=
				( RAL_SKY_RADIANCE_CAPTURE_MASK |
					RAL_SKY_RADIANCE_AMBIENT_SH_MASK ) ||
			productMask != receipt->requiredSliceMask ||
			( receipt->pendingSliceMask & receipt->inFlightSliceMask ) ||
			( receipt->pendingSliceMask & receipt->completedSliceMask ) ||
			( receipt->inFlightSliceMask & receipt->completedSliceMask ) ||
			( receipt->admittedSliceMask & ~receipt->inFlightSliceMask ) ||
			( receipt->acceptedCompletionMask & ~receipt->completedSliceMask ) ||
			( receipt->staleCompletionMask & ~RAL_SKY_RADIANCE_ALL_SLICE_MASK ) ||
			( receipt->cancelledSliceMask & ~RAL_SKY_RADIANCE_ALL_SLICE_MASK ) ||
			receipt->admittedSliceCount != CountBits( receipt->admittedSliceMask ) ||
			receipt->acceptedCompletionCount !=
				CountBits( receipt->acceptedCompletionMask ) ||
			receipt->staleCompletionCount !=
				CountBits( receipt->staleCompletionMask ) ||
			receipt->cancelledSliceCount !=
				CountBits( receipt->cancelledSliceMask ) ||
			receipt->pendingSliceCount != CountBits( receipt->pendingSliceMask ) ||
			receipt->inFlightSliceCount != CountBits( receipt->inFlightSliceMask ) ||
			receipt->completedSliceCount !=
				CountBits( receipt->completedSliceMask ) ||
			!isfinite( receipt->crossfadeAlpha ) || receipt->crossfadeAlpha < 0.0f ||
			receipt->crossfadeAlpha > 1.0f ||
			( receipt->sourceChanged != qfalse && receipt->sourceChanged != qtrue ) ||
			( receipt->refreshStarted != qfalse && receipt->refreshStarted != qtrue ) ||
			( receipt->targetPublished != qfalse &&
				receipt->targetPublished != qtrue ) ||
			( receipt->previousProductValid != qfalse &&
				receipt->previousProductValid != qtrue ) ||
			( receipt->targetReady != qfalse && receipt->targetReady != qtrue ) ||
			( receipt->zeroWork != qfalse && receipt->zeroWork != qtrue ) )
		return qfalse;
	if ( ( receipt->refreshStarted && ( receipt->sourceChanged
			|| receipt->targetPublished || receipt->targetReady
			|| receipt->previousProductValid || receipt->cancelledSliceMask
			|| receipt->acceptedCompletionMask || !receipt->currentProductRevision
			|| !receipt->admittedSliceMask ) ) ||
			receipt->previousProductValid !=
			( receipt->previousProductRevision ? qtrue : qfalse ) ||
			/* The previous sampled slot cannot also be an in-flight target. */
			( receipt->previousProductValid && receipt->inFlightSliceMask ) ||
			( !receipt->previousProductValid && receipt->crossfadeAlpha != 1.0f ) ||
			receipt->zeroWork !=
			( receipt->pendingSliceMask == 0u &&
				receipt->inFlightSliceMask == 0u ? qtrue : qfalse ) ||
			( receipt->targetPublished && !receipt->targetReady ) )
		return qfalse;
	if ( receipt->targetReady )
		return receipt->currentProductRevision == receipt->targetProductRevision &&
			receipt->completedSliceMask == receipt->requiredSliceMask &&
			receipt->zeroWork;
	return receipt->currentProductRevision != receipt->targetProductRevision;
}

qboolean Ral_SkyRadianceReceiptExact( const ralSkyRadianceReceipt_t *a,
		const ralSkyRadianceReceipt_t *b )
{
	return Ral_SkyRadianceReceiptValid( a ) &&
		Ral_SkyRadianceReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) );
}

static qboolean SkyRadianceConsumerReceiptValid(
		const ralSkyRadianceConsumerReceipt_t *receipt )
{
	if ( !receipt ||
			receipt->schemaVersion !=
				RAL_SKY_RADIANCE_CONSUMER_RECEIPT_SCHEMA_VERSION ||
			!BackendValid( receipt->backendType ) ||
			!receipt->sourceRevision || receipt->sourceRevision == UINT64_MAX ||
			!receipt->currentProductRevision ||
			receipt->currentProductRevision == UINT64_MAX ||
			receipt->previousProductRevision == UINT64_MAX ||
			receipt->requiredProductMask != RAL_SKY_RADIANCE_PRODUCT_ALL ||
			receipt->currentReadableProductMask !=
				RAL_SKY_RADIANCE_PRODUCT_ALL ||
			( receipt->previousReadableProductMask &
				~RAL_SKY_RADIANCE_PRODUCT_ALL ) != 0u ||
			!isfinite( receipt->previousToCurrentBlend ) ||
			receipt->previousToCurrentBlend < 0.0f ||
			receipt->previousToCurrentBlend > 1.0f ||
			receipt->currentProductReadable != qtrue ||
			( receipt->previousProductReadable != qfalse &&
				receipt->previousProductReadable != qtrue ) ||
			( receipt->usePreviousProduct != qfalse &&
				receipt->usePreviousProduct != qtrue ) ||
			receipt->ready != qtrue )
		return qfalse;
	if ( receipt->previousProductReadable !=
			( receipt->previousProductRevision ? qtrue : qfalse ) ||
			receipt->previousReadableProductMask !=
				( receipt->previousProductRevision
					? RAL_SKY_RADIANCE_PRODUCT_ALL : 0u ) ||
			receipt->usePreviousProduct != receipt->previousProductReadable )
		return qfalse;
	return receipt->usePreviousProduct ? qtrue :
		receipt->previousToCurrentBlend == 1.0f;
}

qboolean Ral_SkyRadianceConsumerPlan(
		const ralSkyRadianceReceipt_t *radiance,
		uint32_t currentReadableProductMask,
		uint32_t previousReadableProductMask,
		ralSkyRadianceConsumerReceipt_t *outReceipt )
{
	ralSkyRadianceConsumerReceipt_t receipt;
	if ( !outReceipt || !Ral_SkyRadianceReceiptValid( radiance ) ||
			currentReadableProductMask != RAL_SKY_RADIANCE_PRODUCT_ALL ||
			( previousReadableProductMask &
				~RAL_SKY_RADIANCE_PRODUCT_ALL ) != 0u ||
			!radiance->currentProductRevision ||
			previousReadableProductMask !=
				( radiance->previousProductValid
					? RAL_SKY_RADIANCE_PRODUCT_ALL : 0u ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion =
		RAL_SKY_RADIANCE_CONSUMER_RECEIPT_SCHEMA_VERSION;
	receipt.backendType = radiance->backendType;
	receipt.sourceRevision = radiance->sourceRevision;
	receipt.currentProductRevision = radiance->currentProductRevision;
	receipt.previousProductRevision = radiance->previousProductRevision;
	receipt.requiredProductMask = RAL_SKY_RADIANCE_PRODUCT_ALL;
	receipt.currentReadableProductMask = currentReadableProductMask;
	receipt.previousReadableProductMask = previousReadableProductMask;
	receipt.previousToCurrentBlend = radiance->previousProductValid ?
		radiance->crossfadeAlpha : 1.0f;
	receipt.currentProductReadable = qtrue;
	receipt.previousProductReadable = previousReadableProductMask
		? qtrue : qfalse;
	receipt.usePreviousProduct = receipt.previousProductReadable;
	receipt.ready = qtrue;
	if ( !SkyRadianceConsumerReceiptValid( &receipt ) )
		return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_SkyRadianceConsumerAdmit(
		const ralSkyTierBudgetReceipt_t *budget,
		const ralSkyRadianceConsumerReceipt_t *completed,
		ralSkyRadianceConsumerReceipt_t *outConsumer )
{
	ralSkyRadianceConsumerReceipt_t empty;
	memset( &empty, 0, sizeof( empty ) );
	if ( !completed || !outConsumer ||
			!Ral_SkyTierBudgetReceiptExact( budget, budget ) ||
			( memcmp( completed, &empty, sizeof( empty ) ) &&
				!SkyRadianceConsumerReceiptValid( completed ) ) ) return qfalse;
	*outConsumer = budget->maxRadianceSlicesPerFrame ? *completed : empty;
	return qtrue;
}

qboolean Ral_SkyRadianceConsumerReceiptExact(
		const ralSkyRadianceConsumerReceipt_t *a,
		const ralSkyRadianceConsumerReceipt_t *b )
{
	return SkyRadianceConsumerReceiptValid( a ) &&
		SkyRadianceConsumerReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) );
}
