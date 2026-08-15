// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolve_authority.h"

#include <math.h>
#include <string.h>

static qboolean DistinctPointers( const void *const *p, uint32_t count ) {
	uint32_t i, j;
	for ( i = 0; i < count; ++i ) {
		if ( !p[i] ) return qfalse;
		for ( j = i + 1; j < count; ++j )
			if ( p[i] == p[j] ) return qfalse;
	}
	return qtrue;
}

qboolean VK_TemporalResolveBuildAuthority(
		const vkTemporalResolveAuthorityInput_t *input,
		vkTemporalResolveAuthorityReceipt_t *outReceipt,
		vkTemporalResolveProductView_t *outProducts ) {
	vkTemporalResolveAuthorityReceipt_t receipt;
	vkTemporalResolveProductView_t products;
	const vkTemporalMainActivationReceipt_t *activation;
	const vkTemporalMotionRecordingAuthority_t *a;
	const vkTemporalMotionMaterializationReceipt_t *motion;
	const vkTemporalMotionMaterializationProductView_t *mp;
	const temporalHistoryFrameView_t *history;
	const temporalHistoryCommittedReceipt_t *committed;
	const vkTemporalResolvedHdrReceipt_t *resolved;
	const ralTemporalFramePlan_t *plan;
	const void *roles[12];

	if ( !input || !outReceipt || !outProducts || !input->backend
			|| !input->currentColor || !input->currentDepth
			|| !input->sceneColorAttachmentGeneration || !input->frameCount
			|| input->frameCount > 4u || input->commandSlot >= input->frameCount
			|| !isfinite( input->zNear ) || !isfinite( input->zFar )
			|| input->zNear <= 0.0f || input->zFar <= input->zNear
			|| !input->plan || !input->history || !input->activation
			|| !input->motion || !input->motionProducts || !input->resolved )
		return qfalse;
	activation = input->activation;
	a = &activation->authority;
	motion = input->motion;
	mp = input->motionProducts;
	history = input->history;
	committed = &history->committed;
	resolved = input->resolved;
	plan = input->plan;
	if ( !VK_TemporalMainActivationReceiptExact( activation, activation )
			|| activation->prepared != activation->drawSequence.count
			|| activation->temporalSegments !=
				activation->written + activation->invalidated
			|| activation->prepared !=
				activation->temporalSegments + activation->preserved
			|| activation->taggedSequence.count !=
				activation->prepared + activation->iqm.prepared
			|| activation->taggedSequence.genericCount != activation->prepared
			|| activation->taggedSequence.iqmCount != activation->iqm.prepared
			|| activation->temporalSegments +
				activation->iqm.temporalSegments == 0
			|| !activation->auxiliaryCleared || !activation->depthStoreRequired
			|| !activation->stencilStoreRequired
			|| !a->token || !a->frameId || a->worldIndex < 0
			|| !a->width || !a->height || !a->topologyEpoch || !a->planGeneration
			|| !a->targetAllocationGeneration
			|| !a->pipelineLayoutAllocationGeneration
			|| !a->materializationGeneration || !a->pipelineTableGeneration
			|| a->frameIndex != input->commandSlot
			|| !plan->enabled || !plan->historyValid
			|| plan->frameId != a->frameId || plan->generation != a->planGeneration
			|| plan->historyReadIndex > 1 || plan->historyWriteIndex > 1
			|| plan->historyReadIndex == plan->historyWriteIndex ) return qfalse;
	if ( !motion->ready || motion->worldIndex != a->worldIndex
			|| motion->width != a->width || motion->height != a->height
			|| motion->topologyEpoch != a->topologyEpoch
			|| motion->planGeneration != a->planGeneration
			|| motion->targetAllocationGeneration != a->targetAllocationGeneration
			|| motion->pipelineLayoutAllocationGeneration !=
				a->pipelineLayoutAllocationGeneration
			|| motion->allocationGeneration != a->materializationGeneration
			|| !mp->targetAllocationGeneration
			|| mp->targetAllocationGeneration != motion->targetAllocationGeneration
			|| mp->pipelineLayoutAllocationGeneration !=
				motion->pipelineLayoutAllocationGeneration
			|| mp->allocationGeneration != motion->allocationGeneration
			|| mp->scene != input->currentColor || mp->depth != input->currentDepth )
		return qfalse;
	if ( !history->historyValid || history->readIndex != plan->historyReadIndex
			|| history->writeIndex != plan->historyWriteIndex
			|| !committed->valid || committed->worldIndex != a->worldIndex
			|| committed->frameId == UINT64_MAX
			|| committed->frameId + 1u != a->frameId
			|| committed->planGeneration != a->planGeneration
			|| !committed->allocationGeneration
			|| committed->historyIndex != history->readIndex
			|| committed->width != a->width || committed->height != a->height
			|| committed->topologyEpoch != a->topologyEpoch
			|| committed->color != history->readColor
			|| committed->colorView != history->readColorView
			|| committed->depth != history->readDepth
			|| committed->depthView != history->readDepthView
			|| !history->writeColor || !history->writeColorView
			|| !history->writeDepth || !history->writeDepthView ) return qfalse;
	if ( !resolved->ready || resolved->backend != input->backend
			|| resolved->currentSceneColor != input->currentColor
			|| resolved->worldIndex != a->worldIndex
			|| resolved->width != a->width || resolved->height != a->height
			|| resolved->topologyEpoch != a->topologyEpoch
			|| resolved->planGeneration != a->planGeneration
			|| resolved->sceneColorAttachmentGeneration !=
				input->sceneColorAttachmentGeneration
			|| !resolved->allocationGeneration
			|| resolved->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT )
		return qfalse;
	roles[0] = input->currentColor; roles[1] = input->currentDepth;
	roles[2] = history->readColor; roles[3] = history->readDepth;
	roles[4] = mp->velocity; roles[5] = mp->validity;
	roles[6] = resolved->target;
	roles[7] = history->readColorView; roles[8] = history->readDepthView;
	roles[9] = mp->velocityView; roles[10] = mp->validityView;
	roles[11] = resolved->targetView;
	if ( !DistinctPointers( roles, 12u )
			|| !isfinite( plan->sceneJitterPixels[0] )
			|| !isfinite( plan->sceneJitterPixels[1] )
			|| !isfinite( plan->sceneJitterUv[0] )
			|| !isfinite( plan->sceneJitterUv[1] )
			|| !isfinite( plan->previousJitterPixels[0] )
			|| !isfinite( plan->previousJitterPixels[1] )
			|| plan->sceneJitterUv[0] !=
				plan->sceneJitterPixels[0] / (float)a->width
			|| plan->sceneJitterUv[1] !=
				plan->sceneJitterPixels[1] / (float)a->height )
		return qfalse;

	memset( &receipt, 0, sizeof( receipt ) );
	receipt.batchToken = a->token;
	receipt.frameId = a->frameId;
	receipt.previousFrameId = committed->frameId;
	receipt.worldIndex = a->worldIndex;
	receipt.width = a->width; receipt.height = a->height;
	receipt.topologyEpoch = a->topologyEpoch;
	receipt.planGeneration = a->planGeneration;
	receipt.commandSlot = input->commandSlot;
	receipt.frameCount = input->frameCount;
	receipt.historyAllocationGeneration = committed->allocationGeneration;
	receipt.historyReadIndex = history->readIndex;
	receipt.historyWriteIndex = history->writeIndex;
	receipt.previousHistorySource = committed->source;
	receipt.motionTargetAllocationGeneration = motion->targetAllocationGeneration;
	receipt.motionPipelineLayoutAllocationGeneration =
		motion->pipelineLayoutAllocationGeneration;
	receipt.motionMaterializationGeneration = motion->allocationGeneration;
	receipt.pipelineTableGeneration = a->pipelineTableGeneration;
	receipt.sceneColorAttachmentGeneration =
		input->sceneColorAttachmentGeneration;
	receipt.resolvedTargetAllocationGeneration = resolved->allocationGeneration;
	receipt.temporalSegments = activation->temporalSegments;
	receipt.written = activation->written;
	receipt.invalidated = activation->invalidated;
	// Temporal motion is authored from unjittered matrices. Convert the exact
	// planned pixel jitters to the Vulkan/sample UV convention once, inside the
	// immutable authority join, so recording never rereads mutable plan state.
	receipt.currentEffectiveJitterUv[0] =
		plan->sceneJitterPixels[0] / (float)a->width;
	receipt.currentEffectiveJitterUv[1] =
		-plan->sceneJitterPixels[1] / (float)a->height;
	receipt.previousEffectiveJitterUv[0] =
		plan->previousJitterPixels[0] / (float)a->width;
	receipt.previousEffectiveJitterUv[1] =
		-plan->previousJitterPixels[1] / (float)a->height;
	receipt.zNear = input->zNear;
	receipt.zFar = input->zFar;
	if ( !isfinite( receipt.currentEffectiveJitterUv[0] )
			|| !isfinite( receipt.currentEffectiveJitterUv[1] )
			|| !isfinite( receipt.previousEffectiveJitterUv[0] )
			|| !isfinite( receipt.previousEffectiveJitterUv[1] ) ) return qfalse;
	receipt.drawSequence = activation->drawSequence;
	receipt.activation = *activation;
	receipt.ready = qtrue;
	memset( &products, 0, sizeof( products ) );
	products.backend = input->backend;
	products.currentColor = input->currentColor;
	products.currentDepth = input->currentDepth;
	products.previousColor = history->readColor;
	products.previousColorView = history->readColorView;
	products.previousDepth = history->readDepth;
	products.previousDepthView = history->readDepthView;
	products.velocity = mp->velocity;
	products.velocityView = mp->velocityView;
	products.validity = mp->validity;
	products.validityView = mp->validityView;
	products.resolvedTarget = resolved->target;
	products.resolvedTargetView = resolved->targetView;
	*outReceipt = receipt;
	*outProducts = products;
	return qtrue;
}
