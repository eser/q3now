// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_motion_materialization.h"

#include <string.h>

static qboolean InputValid(
		const vkTemporalMotionMaterializationInput_t *input,
		const ralBindGroupLayout_t **outPayloadLayout,
		uint32_t *outPayloadGeneration ) {
	uint32_t i;
	if ( !input || !input->backend || input->device == VK_NULL_HANDLE
			|| !input->payload || input->worldIndex < 0 || !input->width
			|| !input->height || !input->topologyEpoch || !input->planGeneration
			|| !outPayloadLayout || !outPayloadGeneration ) return qfalse;
	for ( i = 0; i < 3; ++i ) {
		if ( !input->borrowedLayouts[i] ) return qfalse;
	}
	return R_TemporalMotionPayloadGetLayout( input->payload,
		outPayloadLayout, outPayloadGeneration );
}

static qboolean ResourceKeyEqual(
		const vkTemporalMotionMaterialization_t *owner,
		const vkTemporalMotionMaterializationInput_t *input,
		const ralBindGroupLayout_t *payloadLayout,
		uint32_t payloadGeneration ) {
	return owner->targets.ready && owner->pipelineLayout.ready
		&& owner->targets.backend == input->backend
		&& owner->targets.width == input->width
		&& owner->targets.height == input->height
		&& owner->targets.topologyEpoch == input->topologyEpoch
		&& owner->pipelineLayout.backend == input->backend
		&& owner->pipelineLayout.device == input->device
		&& owner->pipelineLayout.fog == input->fog
		&& owner->pipelineLayout.payloadLayout == payloadLayout
		&& owner->pipelineLayout.payloadLayoutGeneration == payloadGeneration
		&& memcmp( owner->pipelineLayout.borrowedLayouts, input->borrowedLayouts,
			sizeof( owner->pipelineLayout.borrowedLayouts ) ) == 0;
}

static qboolean AggregateKeyEqual(
		const vkTemporalMotionMaterialization_t *owner,
		const vkTemporalMotionMaterializationInput_t *input,
		const ralBindGroupLayout_t *payloadLayout,
		uint32_t payloadGeneration ) {
	return owner->ready && ResourceKeyEqual( owner, input, payloadLayout,
		payloadGeneration )
		&& owner->payloadLayout == payloadLayout
		&& owner->payloadLayoutGeneration == payloadGeneration
		&& owner->key.worldIndex == input->worldIndex
		&& owner->key.planGeneration == input->planGeneration;
}

void VK_TemporalMotionMaterializationInit(
		vkTemporalMotionMaterialization_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	R_TemporalMotionTargetsInit( &owner->targets );
	VK_TemporalPipelineLayoutInit( &owner->pipelineLayout );
	owner->initialized = qtrue;
}

qboolean VK_TemporalMotionMaterializationNeedsIdle(
		const vkTemporalMotionMaterialization_t *owner,
		const vkTemporalMotionMaterializationInput_t *input ) {
	const ralBindGroupLayout_t *payloadLayout;
	uint32_t payloadGeneration;
	if ( !owner || !owner->initialized || !InputValid( input,
			&payloadLayout, &payloadGeneration ) ) return qfalse;
	return VK_TemporalMotionMaterializationHasLive( owner )
		&& !ResourceKeyEqual( owner, input, payloadLayout, payloadGeneration );
}

qboolean VK_TemporalMotionMaterializationEnsureAfterFence(
		vkTemporalMotionMaterialization_t *owner,
		const vkTemporalMotionMaterializationInput_t *input,
		qboolean idleProven, const vkTemporalLayoutOps_t *layoutOps ) {
	const ralBindGroupLayout_t *payloadLayout;
	uint32_t payloadGeneration;
	uint32_t nextGeneration;
	qboolean resourceEqual;

	if ( !owner || !owner->initialized || !layoutOps ) return qfalse;
	if ( !InputValid( input, &payloadLayout, &payloadGeneration ) ) {
		owner->ready = qfalse;
		return qfalse;
	}
	if ( AggregateKeyEqual( owner, input, payloadLayout, payloadGeneration ) )
		return qtrue;
	owner->ready = qfalse;
	resourceEqual = ResourceKeyEqual( owner, input, payloadLayout,
		payloadGeneration );
	if ( resourceEqual ) {
		owner->key.worldIndex = input->worldIndex;
		owner->key.planGeneration = input->planGeneration;
		owner->key.payload = input->payload;
		owner->ready = qtrue;
		return qtrue;
	}
	if ( owner->allocationGeneration == UINT32_MAX ) return qfalse;
	if ( VK_TemporalMotionMaterializationHasLive( owner )
			&& !idleProven ) return qfalse;
	nextGeneration = owner->allocationGeneration + 1u;

	if ( !R_TemporalMotionTargetsEnsure( &owner->targets, input->backend,
			input->width, input->height, input->topologyEpoch ) ) return qfalse;
	if ( !VK_TemporalPipelineLayoutEnsure( &owner->pipelineLayout, input->backend,
			input->device, input->borrowedLayouts, payloadLayout,
			payloadGeneration, input->fog, layoutOps ) ) return qfalse;
	owner->key = *input;
	owner->payloadLayout = payloadLayout;
	owner->payloadLayoutGeneration = payloadGeneration;
	owner->allocationGeneration = nextGeneration;
	owner->ready = qtrue;
	return qtrue;
}

qboolean VK_TemporalMotionMaterializationGetReceipt(
		const vkTemporalMotionMaterialization_t *owner,
		vkTemporalMotionMaterializationReceipt_t *outReceipt ) {
	vkTemporalMotionMaterializationReceipt_t receipt;
	if ( !owner || !owner->initialized || !owner->ready || !outReceipt
			|| !owner->targets.ready || !owner->pipelineLayout.ready
			|| !owner->allocationGeneration || !owner->payloadLayoutGeneration )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.worldIndex = owner->key.worldIndex;
	receipt.width = owner->key.width;
	receipt.height = owner->key.height;
	receipt.topologyEpoch = owner->key.topologyEpoch;
	receipt.planGeneration = owner->key.planGeneration;
	receipt.payloadLayoutGeneration = owner->payloadLayoutGeneration;
	receipt.targetAllocationGeneration = owner->targets.allocationGeneration;
	receipt.pipelineLayoutAllocationGeneration =
		owner->pipelineLayout.allocationGeneration;
	receipt.allocationGeneration = owner->allocationGeneration;
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalMotionMaterializationGetProductView(
		const vkTemporalMotionMaterialization_t *owner,
		const vkTemporalMotionMaterializationReceipt_t *receipt,
		ralTexture_t *scene, ralTexture_t *depth,
		vkTemporalMotionMaterializationProductView_t *outView ) {
	vkTemporalMotionMaterializationReceipt_t current;
	vkTemporalMotionMaterializationProductView_t view;
	if ( !owner || !receipt || !outView || !scene || !depth
			|| !VK_TemporalMotionMaterializationGetReceipt( owner, &current )
			|| !receipt->ready
			|| receipt->worldIndex != current.worldIndex
			|| receipt->width != current.width || receipt->height != current.height
			|| receipt->topologyEpoch != current.topologyEpoch
			|| receipt->planGeneration != current.planGeneration
			|| receipt->payloadLayoutGeneration != current.payloadLayoutGeneration
			|| receipt->targetAllocationGeneration != current.targetAllocationGeneration
			|| receipt->pipelineLayoutAllocationGeneration !=
				current.pipelineLayoutAllocationGeneration
			|| receipt->allocationGeneration != current.allocationGeneration
			|| !owner->targets.velocity || !owner->targets.velocityView
			|| !owner->targets.validity || !owner->targets.validityView
			|| owner->targets.velocity == owner->targets.validity
			|| owner->targets.velocityView == owner->targets.validityView
			|| owner->pipelineLayout.raw == VK_NULL_HANDLE
			|| !owner->pipelineLayout.adopted ) return qfalse;
	memset( &view, 0, sizeof( view ) );
	view.scene = scene;
	view.depth = depth;
	view.velocity = owner->targets.velocity;
	view.velocityView = owner->targets.velocityView;
	view.validity = owner->targets.validity;
	view.validityView = owner->targets.validityView;
	view.pipelineLayout = owner->pipelineLayout.adopted;
	view.rawPipelineLayout = owner->pipelineLayout.raw;
	view.targetAllocationGeneration = current.targetAllocationGeneration;
	view.pipelineLayoutAllocationGeneration =
		current.pipelineLayoutAllocationGeneration;
	view.allocationGeneration = current.allocationGeneration;
	*outView = view;
	return qtrue;
}

void VK_TemporalMotionMaterializationInvalidateReceipt(
		vkTemporalMotionMaterialization_t *owner ) {
	if ( owner && owner->initialized ) owner->ready = qfalse;
}

qboolean VK_TemporalMotionMaterializationHasLive(
		const vkTemporalMotionMaterialization_t *owner ) {
	return owner && owner->initialized
		&& ( owner->targets.velocity || owner->targets.validity
			|| owner->targets.velocityView || owner->targets.validityView
			|| owner->pipelineLayout.raw != VK_NULL_HANDLE
			|| owner->pipelineLayout.adopted );
}

qboolean VK_TemporalMotionMaterializationReleaseAfterIdle(
		vkTemporalMotionMaterialization_t *owner, qboolean idleProven,
		const vkTemporalLayoutOps_t *layoutOps ) {
	uint32_t aggregateGeneration;
	if ( !owner || !owner->initialized || !idleProven || !layoutOps )
		return qfalse;
	if ( !VK_TemporalPipelineLayoutRelease( &owner->pipelineLayout,
			layoutOps ) ) return qfalse;
	aggregateGeneration = owner->allocationGeneration;
	R_TemporalMotionTargetsRelease( &owner->targets );
	memset( &owner->key, 0, sizeof( owner->key ) );
	owner->payloadLayout = NULL;
	owner->payloadLayoutGeneration = 0;
	owner->ready = qfalse;
	owner->allocationGeneration = aggregateGeneration;
	return qtrue;
}
