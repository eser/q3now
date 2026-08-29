// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_cook_driver.h"

#include <string.h>

static uint32_t ExpectedBatchCount( const ralLightingCookReceipt_t *plan )
{
	uint32_t regionsPerBatch;
	if ( !plan || !plan->scheduledRegionCount ) return 1u;
	regionsPerBatch = plan->scheduledRegionCount;
	return ( plan->scheduledRegionCount + plan->deferredRegionCount +
		regionsPerBatch - 1u ) / regionsPerBatch;
}

qboolean Ral_LightingCookDriverReceiptValid(
	const ralLightingCookDriverReceipt_t *receipt )
{
	uint32_t totalRegions;
	if ( !receipt ||
		receipt->schemaVersion != RAL_LIGHTING_COOK_DRIVER_RECEIPT_SCHEMA_VERSION ||
		!Ral_LightingCookReceiptValid( &receipt->plan ) ||
		receipt->ready != qtrue )
		return qfalse;
	totalRegions = receipt->plan.scheduledRegionCount +
		receipt->plan.deferredRegionCount;
	if ( receipt->completedRegionCount > totalRegions ||
		receipt->batchCount > ExpectedBatchCount( &receipt->plan ) )
		return qfalse;
	if ( receipt->cancelled )
		return receipt->plan.state == RAL_LIGHTING_COOK_CANCELLED &&
			!receipt->producedArtifactCount && !receipt->publication.ready;
	return receipt->plan.state == RAL_LIGHTING_COOK_PLANNED &&
		receipt->batchCount &&
		receipt->completedRegionCount == totalRegions &&
		receipt->batchCount == ExpectedBatchCount( &receipt->plan ) &&
		receipt->producedArtifactCount == receipt->publication.artifactCount &&
		Ral_LightingCookPipelineReceiptValid( &receipt->publication );
}

qboolean Ral_LightingCookDriverExecute(
	const ralLightingCookDriverRequest_t *request,
	ralLightingCookDriverReceipt_t *outReceipt )
{
	ralLightingCookDriverReceipt_t result;
	ralLightingCookPipelineRequest_t publicationRequest;
	ralLightingCookRequest_t batchRequest;
	ralLightingCookReceipt_t batchPlan;
	uint32_t totalRegions, offset = 0u, artifactCount = 0u;
	qboolean firstBatch = qtrue;
	if ( !request || !outReceipt ||
		request->schemaVersion != RAL_LIGHTING_COOK_DRIVER_SCHEMA_VERSION ||
		request->pipeline.schemaVersion != RAL_LIGHTING_COOK_PIPELINE_SCHEMA_VERSION ||
		!request->produceBatch || !request->artifactStorage ||
		!request->artifactCapacity ||
		request->artifactCapacity > RAL_LIGHTING_COOK_MAX_ARTIFACTS )
		return qfalse;
	memset( &result, 0, sizeof( result ) );
	result.schemaVersion = RAL_LIGHTING_COOK_DRIVER_RECEIPT_SCHEMA_VERSION;
	if ( !Ral_LightingCookPlan( &request->pipeline.cook, &result.plan ) )
		return qfalse;
	totalRegions = request->pipeline.cook.dirtyRegionCount;
	do {
		uint32_t remaining = totalRegions - offset;
		uint32_t count = remaining > request->pipeline.cook.workerRegionBudget ?
			request->pipeline.cook.workerRegionBudget : remaining;
		uint32_t index;
		qboolean finalBatch = offset + count == totalRegions;
		if ( request->pipeline.shouldCancel &&
			request->pipeline.shouldCancel( request->pipeline.callbackContext ) ) {
			if ( !Ral_LightingCookTransition( &result.plan,
				RAL_LIGHTING_COOK_CANCELLED, 0u, 0u, &result.plan ) )
				return qfalse;
			result.cancelled = qtrue;
			result.ready = qtrue;
			if ( !Ral_LightingCookDriverReceiptValid( &result ) ) return qfalse;
			*outReceipt = result;
			return qtrue;
		}
		batchRequest = request->pipeline.cook;
		batchRequest.dirtyRegionCount = count;
		batchRequest.workerRegionBudget = count ? count : 1u;
		memset( batchRequest.dirtyRegionIds, 0,
			sizeof( batchRequest.dirtyRegionIds ) );
		for ( index = 0u; index < count; ++index )
			batchRequest.dirtyRegionIds[index] =
				request->pipeline.cook.dirtyRegionIds[offset + index];
		if ( !Ral_LightingCookPlan( &batchRequest, &batchPlan ) ||
			batchPlan.deferredRegionCount ||
			!request->produceBatch( request->producerContext, &batchPlan,
				finalBatch, request->artifactStorage, request->artifactCapacity,
				&artifactCount ) || ( !finalBatch && artifactCount ) )
			return qfalse;
		result.batchCount++;
		result.completedRegionCount += count;
		offset += count;
		firstBatch = qfalse;
	} while ( offset < totalRegions || firstBatch );
	if ( !artifactCount || artifactCount > request->artifactCapacity ) return qfalse;
	publicationRequest = request->pipeline;
	publicationRequest.cook.workerRegionBudget = totalRegions ? totalRegions : 1u;
	publicationRequest.artifacts = request->artifactStorage;
	publicationRequest.artifactCount = artifactCount;
	if ( !Ral_LightingCookPipelineExecute( &publicationRequest,
		&result.publication ) ) return qfalse;
	if ( result.publication.cook.state == RAL_LIGHTING_COOK_CANCELLED ) {
		if ( !Ral_LightingCookTransition( &result.plan,
			RAL_LIGHTING_COOK_CANCELLED, 0u, 0u, &result.plan ) ) return qfalse;
		memset( &result.publication, 0, sizeof( result.publication ) );
		result.producedArtifactCount = 0u;
		result.cancelled = qtrue;
	} else {
		result.producedArtifactCount = artifactCount;
	}
	result.ready = qtrue;
	if ( !Ral_LightingCookDriverReceiptValid( &result ) ) return qfalse;
	*outReceipt = result;
	return qtrue;
}
