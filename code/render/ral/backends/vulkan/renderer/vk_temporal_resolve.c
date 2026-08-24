// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolve.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static qboolean FeedbackSourceEqualExact(
		const temporalHistoryFeedbackSource_t *a,
		const temporalHistoryFeedbackSource_t *b ) {
	if ( !a || !b ) return qfalse;
	return a->backend == b->backend
		&& a->sourceSceneColor == b->sourceSceneColor
		&& a->sourcePostprocessGroup == b->sourcePostprocessGroup
		&& a->sourceHistogramGroup == b->sourceHistogramGroup
		&& a->sourceColor == b->sourceColor
		&& a->sourceColorView == b->sourceColorView
		&& a->postprocessGroup == b->postprocessGroup
		&& a->histogramGroup == b->histogramGroup
		&& a->batchToken == b->batchToken && a->frameId == b->frameId
		&& a->contentSerial == b->contentSerial
		&& a->commandSlot == b->commandSlot && a->frameCount == b->frameCount
		&& a->worldIndex == b->worldIndex && a->width == b->width
		&& a->height == b->height && a->topologyEpoch == b->topologyEpoch
		&& a->planGeneration == b->planGeneration
		&& a->sceneColorAttachmentGeneration ==
			b->sceneColorAttachmentGeneration
		&& a->targetAllocationGeneration == b->targetAllocationGeneration
		&& a->resolveOwnerAllocationGeneration ==
			b->resolveOwnerAllocationGeneration
		&& a->storeOwnerAllocationGeneration ==
			b->storeOwnerAllocationGeneration
		&& a->sceneFormat == b->sceneFormat
		&& a->producer == b->producer ? qtrue : qfalse;
}

#include "../include/vulkan/vulkan.h"

#define VK_TEMPORAL_RESOLVE_BINDINGS 8u
#define VK_TEMPORAL_RESOLVE_DEPTH_ABSOLUTE 0.125f
#define VK_TEMPORAL_RESOLVE_DEPTH_RELATIVE 0.02f
#define VK_TEMPORAL_RESOLVE_HISTORY_WEIGHT 0.875f

_Static_assert( sizeof( vkTemporalResolvePush_t ) == 48u,
	"temporal resolve push ABI must remain 48 bytes" );
_Static_assert( offsetof( vkTemporalResolvePush_t, extent ) == 0u,
	"temporal resolve extent ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t,
	currentEffectiveJitterUv ) == 8u, "temporal resolve current jitter ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t,
	previousEffectiveJitterUv ) == 16u, "temporal resolve previous jitter ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t, zNear ) == 24u,
	"temporal resolve zNear ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t, zFar ) == 28u,
	"temporal resolve zFar ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t,
	depthThresholdAbsolute ) == 32u, "temporal resolve absolute depth ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t,
	depthThresholdRelative ) == 36u, "temporal resolve relative depth ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t, historyWeight ) == 40u,
	"temporal resolve history weight ABI" );
_Static_assert( offsetof( vkTemporalResolvePush_t, historyReadIndex ) == 44u,
	"temporal resolve history index ABI" );

static uint32_t KeyPointers( const vkTemporalResolveKey_t *key,
		const void **out ) {
	uint32_t count = 0, i;
	if ( !key || !out ) return 0;
	out[count++] = key->backend;
	out[count++] = key->currentColor;
	out[count++] = key->currentDepth;
	for ( i = 0; i < 2; ++i ) {
		out[count++] = key->historyColor[i];
		out[count++] = key->historyColorView[i];
		out[count++] = key->historyDepth[i];
		out[count++] = key->historyDepthView[i];
	}
	out[count++] = key->velocity;
	out[count++] = key->velocityView;
	out[count++] = key->validity;
	out[count++] = key->validityView;
	out[count++] = key->resolvedTarget;
	out[count++] = key->resolvedTargetView;
	return count;
}

static qboolean PointersDistinct( const void *const *p, uint32_t count ) {
	uint32_t i, j;
	for ( i = 0; i < count; ++i ) {
		if ( !p[i] ) return qfalse;
		for ( j = i + 1; j < count; ++j )
			if ( p[i] == p[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean KeyValid( const vkTemporalResolveKey_t *key ) {
	const void *p[17];
	if ( !key || !key->backend || !key->width || !key->height
			|| !key->topologyEpoch || !key->sceneColorAttachmentGeneration
			|| !key->historyAllocationGeneration
			|| !key->motionTargetAllocationGeneration
			|| !key->resolvedTargetAllocationGeneration
			|| !key->computeSpirv || !key->computeSpirvSize
			|| ( key->computeSpirvSize & 3u ) != 0u ) return qfalse;
	return PointersDistinct( p, KeyPointers( key, p ) );
}

static qboolean KeyEqual( const vkTemporalResolveKey_t *a,
		const vkTemporalResolveKey_t *b ) {
	uint32_t i;
	if ( !a || !b || a->backend != b->backend || a->width != b->width
			|| a->height != b->height || a->topologyEpoch != b->topologyEpoch
			|| a->sceneColorAttachmentGeneration !=
				b->sceneColorAttachmentGeneration
			|| a->historyAllocationGeneration != b->historyAllocationGeneration
			|| a->motionTargetAllocationGeneration !=
				b->motionTargetAllocationGeneration
			|| a->resolvedTargetAllocationGeneration !=
				b->resolvedTargetAllocationGeneration
			|| a->currentColor != b->currentColor
			|| a->currentDepth != b->currentDepth
			|| a->velocity != b->velocity || a->velocityView != b->velocityView
			|| a->validity != b->validity || a->validityView != b->validityView
			|| a->resolvedTarget != b->resolvedTarget
			|| a->resolvedTargetView != b->resolvedTargetView
			|| a->computeSpirv != b->computeSpirv
			|| a->computeSpirvSize != b->computeSpirvSize ) return qfalse;
	for ( i = 0; i < 2; ++i ) {
		if ( a->historyColor[i] != b->historyColor[i]
				|| a->historyColorView[i] != b->historyColorView[i]
				|| a->historyDepth[i] != b->historyDepth[i]
				|| a->historyDepthView[i] != b->historyDepthView[i] ) return qfalse;
	}
	return qtrue;
}

static qboolean PointerInSet( const void *value, const void *const *set,
		uint32_t count ) {
	uint32_t i;
	if ( !value ) return qfalse;
	for ( i = 0; i < count; ++i ) if ( value == set[i] ) return qtrue;
	return qfalse;
}

static uint32_t ProtectedPointers( const vkTemporalResolveOwner_t *live,
		const vkTemporalResolveKey_t *key, const void **out ) {
	uint32_t count = KeyPointers( key, out );
	if ( live ) {
		count += KeyPointers( &live->key, out + count );
		out[count++] = live->currentColorView;
		out[count++] = live->currentDepthView;
		out[count++] = live->nearestSampler;
		out[count++] = live->layout;
		out[count++] = live->groups[0];
		out[count++] = live->groups[1];
		out[count++] = live->pipeline;
	}
	return count;
}

static void SanitizeCandidate( vkTemporalResolveOwner_t *candidate,
		const vkTemporalResolveOwner_t *live,
		const vkTemporalResolveKey_t *key ) {
	const void *protectedSet[48];
	const void *prior[7];
	void **roles[7];
	uint32_t protectedCount, i, j;
	if ( !candidate ) return;
	protectedCount = ProtectedPointers( live, key, protectedSet );
	roles[0] = (void **)&candidate->currentColorView;
	roles[1] = (void **)&candidate->currentDepthView;
	roles[2] = (void **)&candidate->nearestSampler;
	roles[3] = (void **)&candidate->layout;
	roles[4] = (void **)&candidate->groups[0];
	roles[5] = (void **)&candidate->groups[1];
	roles[6] = (void **)&candidate->pipeline;
	for ( i = 0; i < 7; ++i ) {
		prior[i] = *roles[i];
		if ( PointerInSet( prior[i], protectedSet, protectedCount ) ) {
			*roles[i] = NULL;
			continue;
		}
		for ( j = 0; j < i; ++j ) {
			if ( prior[i] && prior[i] == prior[j] ) {
				*roles[i] = NULL;
				break;
			}
		}
	}
}

static void DestroyOwner( vkTemporalResolveOwner_t *owner ) {
	if ( !owner ) return;
	if ( owner->pipeline ) Ral_DestroyPipeline( owner->pipeline );
	if ( owner->groups[1] && owner->groups[1] != owner->groups[0] )
		Ral_DestroyBindGroup( owner->groups[1] );
	if ( owner->groups[0] ) Ral_DestroyBindGroup( owner->groups[0] );
	if ( owner->layout ) Ral_DestroyBindGroupLayout( owner->layout );
	if ( owner->nearestSampler ) Ral_DestroySampler( owner->nearestSampler );
	if ( owner->currentDepthView
			&& owner->currentDepthView != owner->currentColorView )
		Ral_DestroyTextureView( owner->currentDepthView );
	if ( owner->currentColorView ) Ral_DestroyTextureView( owner->currentColorView );
}

static qboolean OwnerHandlesDistinct( const vkTemporalResolveOwner_t *owner ) {
	const void *p[7];
	if ( !owner ) return qfalse;
	p[0] = owner->currentColorView; p[1] = owner->currentDepthView;
	p[2] = owner->nearestSampler; p[3] = owner->layout;
	p[4] = owner->groups[0]; p[5] = owner->groups[1];
	p[6] = owner->pipeline;
	return PointersDistinct( p, 7u );
}

static qboolean CandidateAliasesProtected(
		const vkTemporalResolveOwner_t *candidate,
		const vkTemporalResolveOwner_t *live,
		const vkTemporalResolveKey_t *key ) {
	const void *protectedSet[48];
	const void *roles[7];
	uint32_t protectedCount, i;
	if ( !candidate ) return qtrue;
	protectedCount = ProtectedPointers( live, key, protectedSet );
	roles[0] = candidate->currentColorView;
	roles[1] = candidate->currentDepthView;
	roles[2] = candidate->nearestSampler;
	roles[3] = candidate->layout;
	roles[4] = candidate->groups[0];
	roles[5] = candidate->groups[1];
	roles[6] = candidate->pipeline;
	for ( i = 0; i < 7; ++i )
		if ( PointerInSet( roles[i], protectedSet, protectedCount ) ) return qtrue;
	return qfalse;
}

void VK_TemporalResolveInit( vkTemporalResolveOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalResolveNeedsIdle( const vkTemporalResolveOwner_t *owner,
		const vkTemporalResolveKey_t *key ) {
	if ( !owner || !owner->initialized || !KeyValid( key ) ) return qfalse;
	return VK_TemporalResolveHasLive( owner ) && !KeyEqual( &owner->key, key )
		? qtrue : qfalse;
}

qboolean VK_TemporalResolveEnsureAfterFence( vkTemporalResolveOwner_t *owner,
		const vkTemporalResolveKey_t *key, qboolean idleProven ) {
	vkTemporalResolveOwner_t candidate;
	ralTextureViewCreateInfo_t vci;
	ralSamplerCreateInfo_t sci;
	ralBindEntry_t entries[VK_TEMPORAL_RESOLVE_BINDINGS];
	ralBindGroupLayoutCreateInfo_t lci;
	ralBindingValue_t values[VK_TEMPORAL_RESOLVE_BINDINGS];
	ralBindGroupCreateInfo_t gci;
	ralComputePipelineCreateInfo_t pci;
	const ralBindGroupLayout_t *layouts[1];
	uint32_t i, readIndex, generation;

	if ( !owner || !owner->initialized || !KeyValid( key ) ) return qfalse;
	if ( owner->ready && KeyEqual( &owner->key, key ) ) return qtrue;
	if ( VK_TemporalResolveHasLive( owner ) && !idleProven ) return qfalse;
	if ( owner->allocationGeneration == UINT32_MAX ) return qfalse;
	generation = owner->allocationGeneration + 1u;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.initialized = qtrue;
	candidate.key = *key;
	candidate.allocationGeneration = generation;

	memset( &vci, 0, sizeof( vci ) );
	vci.viewType = RAL_TEXTURE_2D;
	vci.texture = key->currentColor;
	candidate.currentColorView = Ral_CreateTextureView( key->backend, &vci );
	if ( !candidate.currentColorView ) goto fail;
	vci.texture = key->currentDepth;
	candidate.currentDepthView = Ral_CreateTextureView( key->backend, &vci );
	if ( !candidate.currentDepthView ) goto fail;

	memset( &sci, 0, sizeof( sci ) );
	sci.minFilter = sci.magFilter = RAL_FILTER_NEAREST;
	sci.mipmapMode = RAL_MIPMAP_NEAREST;
	sci.addressU = sci.addressV = sci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
	sci.maxAnisotropy = 1.0f;
	sci.debugName = "wired-temporal-resolve-nearest-sampler";
	candidate.nearestSampler = Ral_CreateSampler( key->backend, &sci );
	if ( !candidate.nearestSampler ) goto fail;

	memset( entries, 0, sizeof( entries ) );
	for ( i = 0; i < 6; ++i )
		entries[i] = (ralBindEntry_t){ i, RAL_BIND_SAMPLED_TEXTURE, 1,
			RAL_STAGE_COMPUTE };
	entries[6] = (ralBindEntry_t){ 6, RAL_BIND_SAMPLER, 1, RAL_STAGE_COMPUTE };
	entries[7] = (ralBindEntry_t){ 7, RAL_BIND_STORAGE_TEXTURE, 1,
		RAL_STAGE_COMPUTE };
	memset( &lci, 0, sizeof( lci ) );
	lci.entries = entries;
	lci.numEntries = VK_TEMPORAL_RESOLVE_BINDINGS;
	lci.debugName = "wired-temporal-resolve-bgl";
	candidate.layout = Ral_CreateBindGroupLayout( key->backend, &lci );
	if ( !candidate.layout ) goto fail;

	for ( readIndex = 0; readIndex < 2; ++readIndex ) {
		memset( values, 0, sizeof( values ) );
		values[0] = (ralBindingValue_t){ .binding=0,
			.type=RAL_BIND_SAMPLED_TEXTURE,
			.textureView=candidate.currentColorView };
		values[1] = (ralBindingValue_t){ .binding=1,
			.type=RAL_BIND_SAMPLED_TEXTURE,
			.textureView=candidate.currentDepthView };
		values[2] = (ralBindingValue_t){ .binding=2,
			.type=RAL_BIND_SAMPLED_TEXTURE,
			.textureView=key->historyColorView[readIndex] };
		values[3] = (ralBindingValue_t){ .binding=3,
			.type=RAL_BIND_SAMPLED_TEXTURE,
			.textureView=key->historyDepthView[readIndex] };
		values[4] = (ralBindingValue_t){ .binding=4,
			.type=RAL_BIND_SAMPLED_TEXTURE, .textureView=key->velocityView };
		values[5] = (ralBindingValue_t){ .binding=5,
			.type=RAL_BIND_SAMPLED_TEXTURE, .textureView=key->validityView };
		values[6] = (ralBindingValue_t){ .binding=6,
			.type=RAL_BIND_SAMPLER, .sampler=candidate.nearestSampler };
		values[7] = (ralBindingValue_t){ .binding=7,
			.type=RAL_BIND_STORAGE_TEXTURE,
			.textureView=key->resolvedTargetView };
		memset( &gci, 0, sizeof( gci ) );
		gci.layout = candidate.layout;
		gci.values = values;
		gci.numValues = VK_TEMPORAL_RESOLVE_BINDINGS;
		gci.debugName = "wired-temporal-resolve-bg";
		candidate.groups[readIndex] = Ral_CreateBindGroup( key->backend, &gci );
		if ( !candidate.groups[readIndex] ) goto fail;
	}
	layouts[0] = candidate.layout;
	memset( &pci, 0, sizeof( pci ) );
	pci.computeSpirv = key->computeSpirv;
	pci.computeSpirvSize = key->computeSpirvSize;
	pci.bindGroupLayouts = layouts;
	pci.numBindGroupLayouts = 1;
	pci.pushConstantSize = sizeof( vkTemporalResolvePush_t );
	pci.debugName = "wired-temporal-resolve-cs";
	candidate.pipeline = Ral_CreateComputePipeline( key->backend, &pci );
	if ( !candidate.pipeline || !OwnerHandlesDistinct( &candidate )
			|| CandidateAliasesProtected( &candidate, owner, key ) ) goto fail;
	candidate.ready = qtrue;
	DestroyOwner( owner );
	*owner = candidate;
	return qtrue;
fail:
	SanitizeCandidate( &candidate, owner, key );
	DestroyOwner( &candidate );
	return qfalse;
}

qboolean VK_TemporalResolveGetReceipt( const vkTemporalResolveOwner_t *owner,
		vkTemporalResolveOwnerReceipt_t *outReceipt ) {
	vkTemporalResolveOwnerReceipt_t receipt;
	if ( !owner || !owner->initialized || !owner->ready || !outReceipt
			|| !owner->allocationGeneration || !KeyValid( &owner->key )
			|| !OwnerHandlesDistinct( owner ) ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.allocationGeneration = owner->allocationGeneration;
	receipt.width = owner->key.width; receipt.height = owner->key.height;
	receipt.topologyEpoch = owner->key.topologyEpoch;
	receipt.sceneColorAttachmentGeneration =
		owner->key.sceneColorAttachmentGeneration;
	receipt.historyAllocationGeneration = owner->key.historyAllocationGeneration;
	receipt.motionTargetAllocationGeneration =
		owner->key.motionTargetAllocationGeneration;
	receipt.resolvedTargetAllocationGeneration =
		owner->key.resolvedTargetAllocationGeneration;
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

static qboolean AuthorityMatchesActivation(
		const vkTemporalResolveAuthorityReceipt_t *authority,
		const vkTemporalMainActivationReceipt_t *activation ) {
	const vkTemporalMotionRecordingAuthority_t *a;
	if ( !authority || !authority->ready || !activation || !activation->ready )
		return qfalse;
	a = &activation->authority;
	return activation->temporalSegments
		+ activation->iqm.temporalSegments > 0
		&& VK_TemporalMainActivationReceiptExact(
			&authority->activation, activation )
		&& activation->auxiliaryCleared
		&& activation->depthStoreRequired
		&& activation->stencilStoreRequired
		&& authority->batchToken == a->token
		&& authority->frameId == a->frameId
		&& authority->worldIndex == a->worldIndex
		&& authority->width == a->width && authority->height == a->height
		&& authority->topologyEpoch == a->topologyEpoch
		&& authority->planGeneration == a->planGeneration
		&& authority->commandSlot == a->frameIndex
		&& authority->motionTargetAllocationGeneration ==
			a->targetAllocationGeneration
		&& authority->motionPipelineLayoutAllocationGeneration ==
			a->pipelineLayoutAllocationGeneration
		&& authority->motionMaterializationGeneration ==
			a->materializationGeneration
		&& authority->pipelineTableGeneration == a->pipelineTableGeneration
		&& authority->temporalSegments == activation->temporalSegments
		&& authority->written == activation->written
		&& authority->invalidated == activation->invalidated
		&& authority->drawSequence.lane0 == activation->drawSequence.lane0
		&& authority->drawSequence.lane1 == activation->drawSequence.lane1
		&& authority->drawSequence.count == activation->drawSequence.count
		? qtrue : qfalse;
}

static qboolean RecordInputsValid( const vkTemporalResolveOwner_t *owner,
		const vkTemporalResolveAuthorityReceipt_t *a,
		const vkTemporalResolveProductView_t *p ) {
	uint32_t r;
	if ( !owner || !owner->ready || !owner->allocationGeneration
			|| !KeyValid( &owner->key ) || !OwnerHandlesDistinct( owner )
			|| !a || !a->ready || !p
			|| a->historyReadIndex > 1 || a->historyWriteIndex > 1
			|| a->historyReadIndex == a->historyWriteIndex
			|| a->previousHistorySource.backend != owner->key.backend
			|| !a->previousHistorySource.sourceSceneColor
			|| !a->previousHistorySource.sourcePostprocessGroup
			|| !a->previousHistorySource.sourceHistogramGroup
			|| !a->previousHistorySource.sourceColor
			|| !a->previousHistorySource.sourceColorView
			|| !a->previousHistorySource.postprocessGroup
			|| !a->previousHistorySource.histogramGroup
			|| !a->previousHistorySource.batchToken
			|| a->previousHistorySource.frameId != a->previousFrameId
			|| !a->previousHistorySource.contentSerial
			|| !a->previousHistorySource.frameCount
			|| a->previousHistorySource.frameCount > 4u
			|| a->previousHistorySource.commandSlot >=
				a->previousHistorySource.frameCount
			|| a->previousHistorySource.worldIndex != a->worldIndex
			|| a->previousHistorySource.width != a->width
			|| a->previousHistorySource.height != a->height
			|| a->previousHistorySource.topologyEpoch != a->topologyEpoch
			|| a->previousHistorySource.planGeneration != a->planGeneration
			|| !a->previousHistorySource.sceneColorAttachmentGeneration
			|| !a->previousHistorySource.targetAllocationGeneration
			|| !a->previousHistorySource.storeOwnerAllocationGeneration
			|| a->previousHistorySource.sceneFormat !=
				RAL_FORMAT_R16G16B16A16_SFLOAT
			|| ( a->previousHistorySource.producer ==
				TEMPORAL_HISTORY_WRITE_CURRENT_SEED
				&& a->previousHistorySource.resolveOwnerAllocationGeneration )
			|| ( a->previousHistorySource.producer ==
				TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK
				&& !a->previousHistorySource.resolveOwnerAllocationGeneration )
			|| ( a->previousHistorySource.producer !=
				TEMPORAL_HISTORY_WRITE_CURRENT_SEED
				&& a->previousHistorySource.producer !=
					TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK )
			|| !a->batchToken || !a->frameId
			|| a->previousFrameId == UINT64_MAX
			|| a->previousFrameId + 1u != a->frameId
			|| a->worldIndex < 0 || !a->frameCount
			|| a->frameCount > 4u || a->commandSlot >= a->frameCount
			|| !a->temporalSegments
			|| a->temporalSegments != a->written + a->invalidated
			|| !isfinite( a->currentEffectiveJitterUv[0] )
			|| !isfinite( a->currentEffectiveJitterUv[1] )
			|| !isfinite( a->previousEffectiveJitterUv[0] )
			|| !isfinite( a->previousEffectiveJitterUv[1] )
			|| !isfinite( a->zNear ) || !isfinite( a->zFar )
			|| a->zNear <= 0.0f || a->zFar <= a->zNear
			|| a->width != owner->key.width || a->height != owner->key.height
			|| a->topologyEpoch != owner->key.topologyEpoch
			|| a->sceneColorAttachmentGeneration !=
				owner->key.sceneColorAttachmentGeneration
			|| a->historyAllocationGeneration !=
				owner->key.historyAllocationGeneration
			|| a->motionTargetAllocationGeneration !=
				owner->key.motionTargetAllocationGeneration
			|| a->resolvedTargetAllocationGeneration !=
				owner->key.resolvedTargetAllocationGeneration ) return qfalse;
	r = a->historyReadIndex;
	return owner->pipeline && owner->groups[r]
		&& p->backend == owner->key.backend
		&& p->currentColor == owner->key.currentColor
		&& p->currentDepth == owner->key.currentDepth
		&& p->previousColor == owner->key.historyColor[r]
		&& p->previousColorView == owner->key.historyColorView[r]
		&& p->previousDepth == owner->key.historyDepth[r]
		&& p->previousDepthView == owner->key.historyDepthView[r]
		&& p->velocity == owner->key.velocity
		&& p->velocityView == owner->key.velocityView
		&& p->validity == owner->key.validity
		&& p->validityView == owner->key.validityView
		&& p->resolvedTarget == owner->key.resolvedTarget
		&& p->resolvedTargetView == owner->key.resolvedTargetView
		? qtrue : qfalse;
}

qboolean VK_TemporalResolveRecord( vkTemporalResolveOwner_t *owner,
		ralCommandBuffer_t *commandBuffer,
		const vkTemporalResolveAuthorityReceipt_t *authority,
		const vkTemporalResolveProductView_t *products,
		const vkHdrPostprocessSource_t *current,
		const vkTemporalResolvedHdrReceipt_t *target,
		uint64_t contentSerial, vkTemporalResolveTicket_t *outTicket ) {
	vkTemporalResolvedHdrContentReceipt_t content;
	vkHdrPostprocessSource_t route;
	vkTemporalResolvePush_t push;
	vkTemporalResolveTicket_t ticket;
	uint32_t readIndex;
	if ( !commandBuffer || !outTicket
			|| !RecordInputsValid( owner, authority, products )
			|| !current || current->backend != owner->key.backend
			|| current->attachment != owner->key.currentColor
			|| current->batchToken != authority->batchToken
			|| current->frameId != authority->frameId
			|| current->commandSlot != authority->commandSlot
			|| current->frameCount != authority->frameCount
			|| current->worldIndex != authority->worldIndex
			|| current->width != authority->width
			|| current->height != authority->height
			|| current->topologyEpoch != authority->topologyEpoch
			|| current->planGeneration != authority->planGeneration
			|| current->sceneColorAttachmentGeneration !=
				authority->sceneColorAttachmentGeneration
			|| !target || !target->ready
			|| target->backend != owner->key.backend
			|| target->target != owner->key.resolvedTarget
			|| target->targetView != owner->key.resolvedTargetView
			|| target->currentSceneColor != owner->key.currentColor
			|| target->worldIndex != authority->worldIndex
			|| target->width != authority->width
			|| target->height != authority->height
			|| target->topologyEpoch != authority->topologyEpoch
			|| target->planGeneration != authority->planGeneration
			|| target->sceneColorAttachmentGeneration !=
				authority->sceneColorAttachmentGeneration
			|| target->allocationGeneration !=
				owner->key.resolvedTargetAllocationGeneration
			|| !VK_TemporalResolvedHdrBuildContentReceiptForProducer(
				current, target, contentSerial,
				VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE,
				&content )
			|| !VK_TemporalResolvedHdrRouteSource(
				current, target, &content, &route ) || !route.resolved ) return qfalse;
	readIndex = authority->historyReadIndex;
	memset( &push, 0, sizeof( push ) );
	push.extent[0] = authority->width; push.extent[1] = authority->height;
	push.currentEffectiveJitterUv[0] = authority->currentEffectiveJitterUv[0];
	push.currentEffectiveJitterUv[1] = authority->currentEffectiveJitterUv[1];
	push.previousEffectiveJitterUv[0] = authority->previousEffectiveJitterUv[0];
	push.previousEffectiveJitterUv[1] = authority->previousEffectiveJitterUv[1];
	push.zNear = authority->zNear; push.zFar = authority->zFar;
	push.depthThresholdAbsolute = VK_TEMPORAL_RESOLVE_DEPTH_ABSOLUTE;
	push.depthThresholdRelative = VK_TEMPORAL_RESOLVE_DEPTH_RELATIVE;
	push.historyWeight = VK_TEMPORAL_RESOLVE_HISTORY_WEIGHT;
	push.historyReadIndex = readIndex;
	memset( &ticket, 0, sizeof( ticket ) );
	ticket.authority = *authority;
	ticket.products = *products;
	ticket.content = content;
	ticket.committedWriteExpected.valid = qtrue;
	ticket.committedWriteExpected.frameId = authority->frameId;
	ticket.committedWriteExpected.worldIndex = authority->worldIndex;
	ticket.committedWriteExpected.planGeneration = authority->planGeneration;
	ticket.committedWriteExpected.allocationGeneration =
		authority->historyAllocationGeneration;
	ticket.committedWriteExpected.historyIndex = authority->historyWriteIndex;
	ticket.committedWriteExpected.width = authority->width;
	ticket.committedWriteExpected.height = authority->height;
	ticket.committedWriteExpected.topologyEpoch = authority->topologyEpoch;
	ticket.committedWriteExpected.color =
		owner->key.historyColor[authority->historyWriteIndex];
	ticket.committedWriteExpected.colorView =
		owner->key.historyColorView[authority->historyWriteIndex];
	ticket.committedWriteExpected.depth =
		owner->key.historyDepth[authority->historyWriteIndex];
	ticket.committedWriteExpected.depthView =
		owner->key.historyDepthView[authority->historyWriteIndex];
	ticket.committedWriteExpected.source.backend = content.backend;
	ticket.committedWriteExpected.source.sourceSceneColor =
		content.sourceSceneColor;
	ticket.committedWriteExpected.source.sourcePostprocessGroup =
		content.sourcePostprocessGroup;
	ticket.committedWriteExpected.source.sourceHistogramGroup =
		content.sourceHistogramGroup;
	ticket.committedWriteExpected.source.sourceColor = content.target;
	ticket.committedWriteExpected.source.sourceColorView = content.targetView;
	ticket.committedWriteExpected.source.postprocessGroup =
		content.postprocessGroup;
	ticket.committedWriteExpected.source.histogramGroup =
		content.histogramGroup;
	ticket.committedWriteExpected.source.batchToken = content.batchToken;
	ticket.committedWriteExpected.source.frameId = content.frameId;
	ticket.committedWriteExpected.source.contentSerial = content.contentSerial;
	ticket.committedWriteExpected.source.commandSlot = content.commandSlot;
	ticket.committedWriteExpected.source.frameCount = content.frameCount;
	ticket.committedWriteExpected.source.worldIndex = content.worldIndex;
	ticket.committedWriteExpected.source.width = content.width;
	ticket.committedWriteExpected.source.height = content.height;
	ticket.committedWriteExpected.source.topologyEpoch = content.topologyEpoch;
	ticket.committedWriteExpected.source.planGeneration = content.planGeneration;
	ticket.committedWriteExpected.source.sceneColorAttachmentGeneration =
		content.sceneColorAttachmentGeneration;
	ticket.committedWriteExpected.source.targetAllocationGeneration =
		content.targetAllocationGeneration;
	ticket.committedWriteExpected.source.resolveOwnerAllocationGeneration =
		owner->allocationGeneration;
	/* The Store owner generation is filled by the product's post-fence Store
	 * snapshot before this exact ticket is staged. */
	ticket.committedWriteExpected.source.sceneFormat = content.sceneFormat;
	ticket.committedWriteExpected.source.producer =
		TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK;
	ticket.push = push;
	ticket.ownerAllocationGeneration = owner->allocationGeneration;
	ticket.recorded = qtrue;

	Ral_CmdTransitionTexture( commandBuffer, owner->key.currentColor,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	/* currentDepth is already made compute-visible by the scene-depth copy's
	 * portable COPY_DESTINATION -> SAMPLED_TEXTURE transition. Re-emitting the
	 * legacy native-layout shim here would discard that portable state authority
	 * even though the layout does not change. */
	Ral_CmdTransitionTexture( commandBuffer, owner->key.historyColor[readIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.historyDepth[readIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.velocity,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.validity,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.resolvedTarget,
		RAL_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_GENERAL );
	Ral_CmdBindPipeline( commandBuffer, owner->pipeline );
	Ral_CmdBindBindGroup( commandBuffer, 0, owner->groups[readIndex] );
	Ral_CmdPushConstants( commandBuffer, RAL_STAGE_COMPUTE, 0,
		sizeof( push ), &push );
	Ral_CmdDispatch( commandBuffer, ( authority->width + 7u ) / 8u,
		( authority->height + 7u ) / 8u, 1u );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.velocity,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.validity,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, owner->key.resolvedTarget,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			| RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	*outTicket = ticket;
	return qtrue;
}

qboolean VK_TemporalResolveResolveSubmit(
		const vkTemporalResolveTicket_t *ticket,
		const vkTemporalResolveOwnerReceipt_t *ownerReceipt,
		const vkTemporalMainActivationReceipt_t *acceptedActivation,
		const temporalHistoryCommittedReceipt_t *committedWrite,
		const vkTemporalResolvedHdrReceipt_t *targetReceipt,
		qboolean submitted,
		vkTemporalResolveTicket_t *outSubmittedTicket ) {
	vkTemporalResolveTicket_t promoted;
	vkTemporalResolvedHdrContentReceipt_t submittedContent;
	if ( !ticket || !ticket->recorded || !ticket->ownerAllocationGeneration
			|| ticket->submitted || !ownerReceipt || !ownerReceipt->ready
			|| ticket->content.producer !=
				VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE
			|| ticket->content.batchToken != ticket->authority.batchToken
			|| ticket->content.frameId != ticket->authority.frameId
			|| ticket->content.commandSlot != ticket->authority.commandSlot
			|| ticket->content.frameCount != ticket->authority.frameCount
			|| ticket->content.worldIndex != ticket->authority.worldIndex
			|| ticket->content.width != ticket->authority.width
			|| ticket->content.height != ticket->authority.height
			|| ticket->content.topologyEpoch != ticket->authority.topologyEpoch
			|| ticket->content.planGeneration != ticket->authority.planGeneration
			|| ticket->content.sceneColorAttachmentGeneration !=
				ticket->authority.sceneColorAttachmentGeneration
			|| ticket->content.targetAllocationGeneration !=
				ticket->authority.resolvedTargetAllocationGeneration
			|| ownerReceipt->allocationGeneration !=
				ticket->ownerAllocationGeneration
			|| ownerReceipt->width != ticket->authority.width
			|| ownerReceipt->height != ticket->authority.height
			|| ownerReceipt->topologyEpoch != ticket->authority.topologyEpoch
			|| ownerReceipt->sceneColorAttachmentGeneration !=
				ticket->authority.sceneColorAttachmentGeneration
			|| ownerReceipt->historyAllocationGeneration !=
				ticket->authority.historyAllocationGeneration
			|| ownerReceipt->motionTargetAllocationGeneration !=
				ticket->authority.motionTargetAllocationGeneration
			|| ownerReceipt->resolvedTargetAllocationGeneration !=
				ticket->authority.resolvedTargetAllocationGeneration
			|| !targetReceipt || !targetReceipt->ready
			|| targetReceipt->backend != ticket->content.backend
			|| targetReceipt->currentSceneColor !=
				ticket->content.sourceSceneColor
			|| targetReceipt->currentPostprocessGroup !=
				ticket->content.sourcePostprocessGroup
			|| targetReceipt->currentHistogramGroup !=
				ticket->content.sourceHistogramGroup
			|| targetReceipt->target != ticket->content.target
			|| targetReceipt->targetView != ticket->content.targetView
			|| targetReceipt->postprocessGroup != ticket->content.postprocessGroup
			|| targetReceipt->histogramGroup != ticket->content.histogramGroup
			|| targetReceipt->allocationGeneration !=
				ticket->content.targetAllocationGeneration
			|| targetReceipt->sceneFormat != ticket->content.sceneFormat
			|| targetReceipt->worldIndex != ticket->content.worldIndex
			|| targetReceipt->width != ticket->content.width
			|| targetReceipt->height != ticket->content.height
			|| targetReceipt->topologyEpoch != ticket->content.topologyEpoch
			|| targetReceipt->planGeneration != ticket->content.planGeneration
			|| targetReceipt->sceneColorAttachmentGeneration !=
				ticket->content.sceneColorAttachmentGeneration
			|| !ticket->committedWriteExpected.valid
			|| ticket->committedWriteExpected.frameId != ticket->authority.frameId
			|| ticket->committedWriteExpected.worldIndex !=
				ticket->authority.worldIndex
			|| ticket->committedWriteExpected.planGeneration !=
				ticket->authority.planGeneration
			|| ticket->committedWriteExpected.allocationGeneration !=
				ticket->authority.historyAllocationGeneration
			|| ticket->committedWriteExpected.historyIndex !=
				ticket->authority.historyWriteIndex
			|| ticket->committedWriteExpected.width != ticket->authority.width
			|| ticket->committedWriteExpected.height != ticket->authority.height
			|| ticket->committedWriteExpected.topologyEpoch !=
				ticket->authority.topologyEpoch
			|| !committedWrite || !committedWrite->valid
			|| committedWrite->frameId !=
				ticket->committedWriteExpected.frameId
			|| committedWrite->worldIndex !=
				ticket->committedWriteExpected.worldIndex
			|| committedWrite->planGeneration !=
				ticket->committedWriteExpected.planGeneration
			|| committedWrite->allocationGeneration !=
				ticket->committedWriteExpected.allocationGeneration
			|| committedWrite->historyIndex !=
				ticket->committedWriteExpected.historyIndex
			|| committedWrite->width != ticket->committedWriteExpected.width
			|| committedWrite->height != ticket->committedWriteExpected.height
			|| committedWrite->topologyEpoch !=
				ticket->committedWriteExpected.topologyEpoch
			|| committedWrite->color != ticket->committedWriteExpected.color
			|| committedWrite->colorView !=
				ticket->committedWriteExpected.colorView
			|| committedWrite->depth != ticket->committedWriteExpected.depth
			|| committedWrite->depthView !=
				ticket->committedWriteExpected.depthView
			|| !FeedbackSourceEqualExact(
				&committedWrite->source,
				&ticket->committedWriteExpected.source )
			|| !submitted || !outSubmittedTicket
			|| !AuthorityMatchesActivation( &ticket->authority,
				acceptedActivation ) ) return qfalse;
	if ( !VK_TemporalResolvedHdrResolveContentSubmit(
			&ticket->content, qtrue, &submittedContent ) ) return qfalse;
	promoted = *ticket;
	promoted.content = submittedContent;
	promoted.submitted = qtrue;
	*outSubmittedTicket = promoted;
	return qtrue;
}

qboolean VK_TemporalResolveTicketEqualExact(
		const vkTemporalResolveTicket_t *a,
		const vkTemporalResolveTicket_t *b ) {
	const vkTemporalResolveAuthorityReceipt_t *aa, *ba;
	const vkTemporalResolvedHdrContentReceipt_t *ac, *bc;
	const temporalHistoryCommittedReceipt_t *ah, *bh;
	const vkTemporalResolvePush_t *ap, *bp;
	if ( !a || !b ) return qfalse;
	aa = &a->authority; ba = &b->authority;
	ac = &a->content; bc = &b->content;
	ah = &a->committedWriteExpected; bh = &b->committedWriteExpected;
	ap = &a->push; bp = &b->push;
	return aa->batchToken == ba->batchToken && aa->frameId == ba->frameId
		&& aa->previousFrameId == ba->previousFrameId
		&& aa->worldIndex == ba->worldIndex && aa->width == ba->width
		&& aa->height == ba->height && aa->topologyEpoch == ba->topologyEpoch
		&& aa->planGeneration == ba->planGeneration
		&& aa->commandSlot == ba->commandSlot && aa->frameCount == ba->frameCount
		&& aa->historyAllocationGeneration == ba->historyAllocationGeneration
		&& aa->historyReadIndex == ba->historyReadIndex
		&& aa->historyWriteIndex == ba->historyWriteIndex
		&& FeedbackSourceEqualExact(
			&aa->previousHistorySource, &ba->previousHistorySource )
		&& aa->motionTargetAllocationGeneration == ba->motionTargetAllocationGeneration
		&& aa->motionPipelineLayoutAllocationGeneration == ba->motionPipelineLayoutAllocationGeneration
		&& aa->motionMaterializationGeneration == ba->motionMaterializationGeneration
		&& aa->pipelineTableGeneration == ba->pipelineTableGeneration
		&& aa->sceneColorAttachmentGeneration == ba->sceneColorAttachmentGeneration
		&& aa->resolvedTargetAllocationGeneration == ba->resolvedTargetAllocationGeneration
		&& aa->temporalSegments == ba->temporalSegments
		&& aa->written == ba->written && aa->invalidated == ba->invalidated
		&& aa->currentEffectiveJitterUv[0] == ba->currentEffectiveJitterUv[0]
		&& aa->currentEffectiveJitterUv[1] == ba->currentEffectiveJitterUv[1]
		&& aa->previousEffectiveJitterUv[0] == ba->previousEffectiveJitterUv[0]
		&& aa->previousEffectiveJitterUv[1] == ba->previousEffectiveJitterUv[1]
		&& aa->zNear == ba->zNear && aa->zFar == ba->zFar
		&& aa->drawSequence.lane0 == ba->drawSequence.lane0
		&& aa->drawSequence.lane1 == ba->drawSequence.lane1
		&& aa->drawSequence.count == ba->drawSequence.count
		&& aa->ready == ba->ready
		&& a->products.backend == b->products.backend
		&& a->products.currentColor == b->products.currentColor
		&& a->products.currentDepth == b->products.currentDepth
		&& a->products.previousColor == b->products.previousColor
		&& a->products.previousColorView == b->products.previousColorView
		&& a->products.previousDepth == b->products.previousDepth
		&& a->products.previousDepthView == b->products.previousDepthView
		&& a->products.velocity == b->products.velocity
		&& a->products.velocityView == b->products.velocityView
		&& a->products.validity == b->products.validity
		&& a->products.validityView == b->products.validityView
		&& a->products.resolvedTarget == b->products.resolvedTarget
		&& a->products.resolvedTargetView == b->products.resolvedTargetView
		&& ac->batchToken == bc->batchToken && ac->frameId == bc->frameId
		&& ac->contentSerial == bc->contentSerial
		&& ac->commandSlot == bc->commandSlot && ac->frameCount == bc->frameCount
		&& ac->worldIndex == bc->worldIndex && ac->width == bc->width
		&& ac->height == bc->height && ac->topologyEpoch == bc->topologyEpoch
		&& ac->planGeneration == bc->planGeneration
		&& ac->sceneColorAttachmentGeneration == bc->sceneColorAttachmentGeneration
		&& ac->targetAllocationGeneration == bc->targetAllocationGeneration
		&& ac->backend == bc->backend && ac->sourceSceneColor == bc->sourceSceneColor
		&& ac->sourcePostprocessGroup == bc->sourcePostprocessGroup
		&& ac->sourceHistogramGroup == bc->sourceHistogramGroup
		&& ac->target == bc->target && ac->targetView == bc->targetView
		&& ac->postprocessGroup == bc->postprocessGroup
		&& ac->histogramGroup == bc->histogramGroup
		&& ac->sceneFormat == bc->sceneFormat && ac->producer == bc->producer
		&& ac->submitted == bc->submitted && ac->valid == bc->valid
		&& ah->frameId == bh->frameId && ah->worldIndex == bh->worldIndex
		&& ah->planGeneration == bh->planGeneration
		&& ah->allocationGeneration == bh->allocationGeneration
		&& ah->historyIndex == bh->historyIndex && ah->width == bh->width
		&& ah->height == bh->height && ah->topologyEpoch == bh->topologyEpoch
		&& ah->color == bh->color && ah->colorView == bh->colorView
		&& ah->depth == bh->depth && ah->depthView == bh->depthView
		&& FeedbackSourceEqualExact(
			&ah->source, &bh->source )
		&& ah->valid == bh->valid
		&& ap->extent[0] == bp->extent[0] && ap->extent[1] == bp->extent[1]
		&& ap->currentEffectiveJitterUv[0] == bp->currentEffectiveJitterUv[0]
		&& ap->currentEffectiveJitterUv[1] == bp->currentEffectiveJitterUv[1]
		&& ap->previousEffectiveJitterUv[0] == bp->previousEffectiveJitterUv[0]
		&& ap->previousEffectiveJitterUv[1] == bp->previousEffectiveJitterUv[1]
		&& ap->zNear == bp->zNear && ap->zFar == bp->zFar
		&& ap->depthThresholdAbsolute == bp->depthThresholdAbsolute
		&& ap->depthThresholdRelative == bp->depthThresholdRelative
		&& ap->historyWeight == bp->historyWeight
		&& ap->historyReadIndex == bp->historyReadIndex
		&& a->ownerAllocationGeneration == b->ownerAllocationGeneration
		&& a->recorded == b->recorded && a->submitted == b->submitted
		? qtrue : qfalse;
}

qboolean VK_TemporalResolveTicketValidateRecordedExact(
		const vkTemporalResolveTicket_t *t ) {
	const vkTemporalResolveAuthorityReceipt_t *a;
	const vkTemporalResolveProductView_t *p;
	const vkTemporalResolvedHdrContentReceipt_t *c;
	const vkTemporalResolvePush_t *push;
	if ( !t ) return qfalse;
	a = &t->authority; p = &t->products; c = &t->content; push = &t->push;
	return t->recorded && !t->submitted && !c->submitted && a->ready
		&& c->valid
		&& c->producer == VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE
		&& push->extent[0] == a->width && push->extent[1] == a->height
		&& push->currentEffectiveJitterUv[0] == a->currentEffectiveJitterUv[0]
		&& push->currentEffectiveJitterUv[1] == a->currentEffectiveJitterUv[1]
		&& push->previousEffectiveJitterUv[0] == a->previousEffectiveJitterUv[0]
		&& push->previousEffectiveJitterUv[1] == a->previousEffectiveJitterUv[1]
		&& push->zNear == a->zNear && push->zFar == a->zFar
		&& push->depthThresholdAbsolute == VK_TEMPORAL_RESOLVE_DEPTH_ABSOLUTE
		&& push->depthThresholdRelative == VK_TEMPORAL_RESOLVE_DEPTH_RELATIVE
		&& push->historyWeight == VK_TEMPORAL_RESOLVE_HISTORY_WEIGHT
		&& push->historyReadIndex == a->historyReadIndex
		&& c->batchToken == a->batchToken && c->frameId == a->frameId
		&& c->commandSlot == a->commandSlot && c->frameCount == a->frameCount
		&& c->worldIndex == a->worldIndex && c->width == a->width
		&& c->height == a->height && c->topologyEpoch == a->topologyEpoch
		&& c->planGeneration == a->planGeneration
		&& c->sceneColorAttachmentGeneration == a->sceneColorAttachmentGeneration
		&& c->targetAllocationGeneration == a->resolvedTargetAllocationGeneration
		&& c->backend == p->backend && c->sourceSceneColor == p->currentColor
		&& c->target == p->resolvedTarget && c->targetView == p->resolvedTargetView
		? qtrue : qfalse;
}

qboolean VK_TemporalResolveBindStoreExpected(
		const vkTemporalResolveTicket_t *recorded,
		uint32_t storeOwnerAllocationGeneration,
		vkTemporalResolveTicket_t *outBound ) {
	vkTemporalResolveTicket_t bound;
	if ( !recorded || !storeOwnerAllocationGeneration || !outBound
			|| recorded->committedWriteExpected.source
				.storeOwnerAllocationGeneration
			|| !VK_TemporalResolveTicketValidateRecordedExact( recorded ) )
		return qfalse;
	bound = *recorded;
	bound.committedWriteExpected.source.storeOwnerAllocationGeneration =
		storeOwnerAllocationGeneration;
	*outBound = bound;
	return qtrue;
}

qboolean VK_TemporalResolveHasLive( const vkTemporalResolveOwner_t *owner ) {
	return owner && owner->initialized && ( owner->currentColorView
		|| owner->currentDepthView || owner->nearestSampler
		|| owner->layout
		|| owner->groups[0] || owner->groups[1] || owner->pipeline )
		? qtrue : qfalse;
}

qboolean VK_TemporalResolveReleaseAfterIdle( vkTemporalResolveOwner_t *owner,
		qboolean idleProven ) {
	uint32_t generation;
	if ( !owner || !owner->initialized || !idleProven ) return qfalse;
	generation = owner->allocationGeneration;
	DestroyOwner( owner );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->allocationGeneration = generation;
	return qtrue;
}
