// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolved_hdr.h"

#include <limits.h>
#include <string.h>

#define TEMPORAL_RESOLVED_HDR_USAGE ((ralTextureUsage_t)( \
	RAL_TEXTURE_USAGE_STORAGE | RAL_TEXTURE_USAGE_SAMPLED | \
	RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_TRANSFER_DST | \
	RAL_TEXTURE_USAGE_TRANSFER_SRC ))
#define TEMPORAL_RESOLVED_HDR_MAX_COMMAND_SLOTS 4u

static qboolean InputValid( const vkTemporalResolvedHdrInput_t *input ) {
	const void *borrowed[7];
	uint32_t i, j;
	if ( !input || !input->backend || !input->currentSceneColor
			|| !input->currentPostprocessGroup || !input->currentHistogramGroup
			|| !input->postprocessLayout || !input->histogramLayout
			|| !input->histogramBuffer || input->worldIndex < 0
			|| !input->width || !input->height || !input->topologyEpoch
			|| !input->planGeneration || !input->sceneColorAttachmentGeneration
			|| input->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT
			|| ( input->filter != RAL_FILTER_NEAREST
				&& input->filter != RAL_FILTER_LINEAR ) ) return qfalse;
	borrowed[0] = input->backend;
	borrowed[1] = input->currentSceneColor;
	borrowed[2] = input->currentPostprocessGroup;
	borrowed[3] = input->currentHistogramGroup;
	borrowed[4] = input->postprocessLayout;
	borrowed[5] = input->histogramLayout;
	borrowed[6] = input->histogramBuffer;
	for ( i = 0; i < 7; ++i ) for ( j = i + 1; j < 7; ++j )
		if ( borrowed[i] == borrowed[j] ) return qfalse;
	return qtrue;
}

static qboolean ResourceKeyEqual( const vkTemporalResolvedHdrOwner_t *owner,
		const vkTemporalResolvedHdrInput_t *input ) {
	return owner && owner->ready && input
		&& owner->key.backend == input->backend
		&& owner->key.currentSceneColor == input->currentSceneColor
		&& owner->key.currentPostprocessGroup == input->currentPostprocessGroup
		&& owner->key.currentHistogramGroup == input->currentHistogramGroup
		&& owner->key.postprocessLayout == input->postprocessLayout
		&& owner->key.histogramLayout == input->histogramLayout
		&& owner->key.histogramBuffer == input->histogramBuffer
		&& owner->key.width == input->width
		&& owner->key.height == input->height
		&& owner->key.topologyEpoch == input->topologyEpoch
		&& owner->key.sceneColorAttachmentGeneration ==
			input->sceneColorAttachmentGeneration
		&& owner->key.sceneFormat == input->sceneFormat
		&& owner->key.filter == input->filter ? qtrue : qfalse;
}

static qboolean ExactKeyEqual( const vkTemporalResolvedHdrOwner_t *owner,
		const vkTemporalResolvedHdrInput_t *input ) {
	return ResourceKeyEqual( owner, input )
		&& owner->key.worldIndex == input->worldIndex
		&& owner->key.planGeneration == input->planGeneration ? qtrue : qfalse;
}

static qboolean PointerInSet( const void *value, const void *const *set,
		uint32_t count ) {
	uint32_t i;
	if ( !value ) return qfalse;
	for ( i = 0; i < count; ++i ) {
		if ( value == set[i] ) return qtrue;
	}
	return qfalse;
}

static uint32_t ProtectedPointers( const vkTemporalResolvedHdrOwner_t *live,
		const vkTemporalResolvedHdrInput_t *input, const void **out ) {
	uint32_t count = 0;
	if ( input ) {
		out[count++] = (const void *)input->backend;
		out[count++] = (const void *)input->currentSceneColor;
		out[count++] = (const void *)input->currentPostprocessGroup;
		out[count++] = (const void *)input->currentHistogramGroup;
		out[count++] = (const void *)input->postprocessLayout;
		out[count++] = (const void *)input->histogramLayout;
		out[count++] = (const void *)input->histogramBuffer;
	}
	if ( live ) {
		out[count++] = (const void *)live->target;
		out[count++] = (const void *)live->targetView;
		out[count++] = (const void *)live->sampler;
		out[count++] = (const void *)live->postprocessGroup;
		out[count++] = (const void *)live->histogramGroup;
	}
	return count;
}

static qboolean CandidateHasAlias( const vkTemporalResolvedHdrOwner_t *live,
		const vkTemporalResolvedHdrOwner_t *candidate,
		const vkTemporalResolvedHdrInput_t *input ) {
	const void *protectedSet[12];
	const void *roles[5];
	uint32_t protectedCount, i, j;
	if ( !candidate ) return qtrue;
	protectedCount = ProtectedPointers( live, input, protectedSet );
	roles[0] = (const void *)candidate->target;
	roles[1] = (const void *)candidate->targetView;
	roles[2] = (const void *)candidate->sampler;
	roles[3] = (const void *)candidate->postprocessGroup;
	roles[4] = (const void *)candidate->histogramGroup;
	for ( i = 0; i < (uint32_t)( sizeof( roles ) / sizeof( roles[0] ) ); ++i ) {
		if ( !roles[i] ) continue;
		if ( PointerInSet( roles[i], protectedSet, protectedCount ) ) return qtrue;
		for ( j = 0; j < i; ++j ) {
			if ( roles[i] == roles[j] ) return qtrue;
		}
	}
	return qfalse;
}

static void DestroyHandles( vkTemporalResolvedHdrOwner_t *owner ) {
	if ( !owner ) return;
	if ( owner->histogramGroup
			&& owner->histogramGroup != owner->postprocessGroup )
		Ral_DestroyBindGroup( owner->histogramGroup );
	if ( owner->postprocessGroup ) Ral_DestroyBindGroup( owner->postprocessGroup );
	if ( owner->sampler ) Ral_DestroySampler( owner->sampler );
	if ( owner->targetView ) Ral_DestroyTextureView( owner->targetView );
	if ( owner->target ) Ral_DestroyTexture( owner->target );
}

static void DestroyCandidate( vkTemporalResolvedHdrOwner_t *candidate,
		const vkTemporalResolvedHdrOwner_t *live,
		const vkTemporalResolvedHdrInput_t *input ) {
	const void *protectedSet[12];
	const void *target, *view, *sampler, *post, *histogram;
	uint32_t protectedCount;
	if ( !candidate ) return;
	protectedCount = ProtectedPointers( live, input, protectedSet );
	target = (const void *)candidate->target;
	view = (const void *)candidate->targetView;
	sampler = (const void *)candidate->sampler;
	post = (const void *)candidate->postprocessGroup;
	histogram = (const void *)candidate->histogramGroup;
	if ( PointerInSet( target, protectedSet, protectedCount ) ) candidate->target = NULL;
	if ( PointerInSet( view, protectedSet, protectedCount ) || view == target )
		candidate->targetView = NULL;
	if ( PointerInSet( sampler, protectedSet, protectedCount )
			|| sampler == target || sampler == view ) candidate->sampler = NULL;
	if ( PointerInSet( post, protectedSet, protectedCount )
			|| post == target || post == view || post == sampler )
		candidate->postprocessGroup = NULL;
	if ( PointerInSet( histogram, protectedSet, protectedCount )
			|| histogram == target || histogram == view || histogram == sampler
			|| histogram == post ) candidate->histogramGroup = NULL;
	DestroyHandles( candidate );
}

void VK_TemporalResolvedHdrInit( vkTemporalResolvedHdrOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalResolvedHdrNeedsIdle(
		const vkTemporalResolvedHdrOwner_t *owner,
		const vkTemporalResolvedHdrInput_t *input ) {
	if ( !owner || !owner->initialized || !InputValid( input ) ) return qfalse;
	return VK_TemporalResolvedHdrHasLive( owner )
		&& !ResourceKeyEqual( owner, input ) ? qtrue : qfalse;
}

qboolean VK_TemporalResolvedHdrEnsureAfterFence(
		vkTemporalResolvedHdrOwner_t *owner,
		const vkTemporalResolvedHdrInput_t *input, qboolean idleProven ) {
	vkTemporalResolvedHdrOwner_t candidate;
	ralTextureCreateInfo_t tci;
	ralTextureViewCreateInfo_t vci;
	ralSamplerCreateInfo_t sci;
	ralBindingValue_t sampleValue;
	ralBindingValue_t histogramValues[3];
	ralBindGroupCreateInfo_t gci;
	const ralCaps_t *caps;
	uint32_t generation;

	if ( !owner || !owner->initialized ) return qfalse;
	if ( !InputValid( input ) ) return qfalse;
	if ( ExactKeyEqual( owner, input ) ) return qtrue;
	if ( ResourceKeyEqual( owner, input ) ) {
		owner->key.worldIndex = input->worldIndex;
		owner->key.planGeneration = input->planGeneration;
		return qtrue;
	}
	if ( VK_TemporalResolvedHdrHasLive( owner ) && !idleProven ) return qfalse;
	if ( owner->allocationGeneration == UINT32_MAX ) return qfalse;
	caps = Ral_GetCaps( input->backend );
	if ( !caps || !caps->maxTextureDimension2D
			|| input->width > caps->maxTextureDimension2D
			|| input->height > caps->maxTextureDimension2D
			|| !Ral_TextureFormatSupports( input->backend,
				input->sceneFormat,
				TEMPORAL_RESOLVED_HDR_USAGE ) ) return qfalse;
	generation = owner->allocationGeneration + 1u;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.initialized = qtrue;
	candidate.key = *input;
	candidate.allocationGeneration = generation;

	memset( &tci, 0, sizeof( tci ) );
	tci.type = RAL_TEXTURE_2D;
	tci.width = input->width;
	tci.height = input->height;
	tci.depthOrArrayLayers = 1;
	tci.mipLevels = 1;
	tci.sampleCount = 1;
	tci.format = input->sceneFormat;
	tci.usage = TEMPORAL_RESOLVED_HDR_USAGE;
	tci.memory = RAL_MEMORY_DEVICE_LOCAL;
	tci.debugName = "wired-temporal-resolved-hdr";
	candidate.target = Ral_CreateTexture( input->backend, &tci );
	if ( !candidate.target || CandidateHasAlias( owner, &candidate, input ) ) goto fail;
	memset( &vci, 0, sizeof( vci ) );
	vci.texture = candidate.target;
	vci.viewType = RAL_TEXTURE_2D;
	vci.format = RAL_FORMAT_UNDEFINED;
	candidate.targetView = Ral_CreateTextureView( input->backend, &vci );
	if ( !candidate.targetView
			|| CandidateHasAlias( owner, &candidate, input ) ) goto fail;
	memset( &sci, 0, sizeof( sci ) );
	sci.minFilter = input->filter;
	sci.magFilter = input->filter;
	sci.mipmapMode = RAL_MIPMAP_NEAREST;
	sci.addressU = sci.addressV = sci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
	sci.maxAnisotropy = 1.0f;
	sci.debugName = "wired-temporal-resolved-hdr-sampler";
	candidate.sampler = Ral_CreateSampler( input->backend, &sci );
	if ( !candidate.sampler
			|| CandidateHasAlias( owner, &candidate, input ) ) goto fail;

	memset( &sampleValue, 0, sizeof( sampleValue ) );
	sampleValue.binding = 0;
	sampleValue.type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;
	sampleValue.textureView = candidate.targetView;
	sampleValue.sampler = candidate.sampler;
	memset( &gci, 0, sizeof( gci ) );
	gci.layout = input->postprocessLayout;
	gci.values = &sampleValue;
	gci.numValues = 1;
	gci.debugName = "wired-temporal-resolved-hdr-postprocess-bg";
	candidate.postprocessGroup = Ral_CreateBindGroup( input->backend, &gci );
	if ( !candidate.postprocessGroup
			|| CandidateHasAlias( owner, &candidate, input ) ) goto fail;

	memset( histogramValues, 0, sizeof( histogramValues ) );
	histogramValues[0].binding = 0;
	histogramValues[0].type = RAL_BIND_SAMPLED_TEXTURE;
	histogramValues[0].textureView = candidate.targetView;
	histogramValues[1].binding = 1;
	histogramValues[1].type = RAL_BIND_SAMPLER;
	histogramValues[1].sampler = candidate.sampler;
	histogramValues[2].binding = 2;
	histogramValues[2].type = RAL_BIND_STORAGE_BUFFER;
	histogramValues[2].buffer = input->histogramBuffer;
	memset( &gci, 0, sizeof( gci ) );
	gci.layout = input->histogramLayout;
	gci.values = histogramValues;
	gci.numValues = 3;
	gci.debugName = "wired-temporal-resolved-hdr-histogram-bg";
	candidate.histogramGroup = Ral_CreateBindGroup( input->backend, &gci );
	if ( !candidate.histogramGroup
			|| CandidateHasAlias( owner, &candidate, input ) ) goto fail;

	candidate.ready = qtrue;
	DestroyHandles( owner );
	*owner = candidate;
	return qtrue;

fail:
	DestroyCandidate( &candidate, owner, input );
	return qfalse;
}

qboolean VK_TemporalResolvedHdrGetReceipt(
		const vkTemporalResolvedHdrOwner_t *owner,
		vkTemporalResolvedHdrReceipt_t *outReceipt ) {
	vkTemporalResolvedHdrReceipt_t receipt;
	if ( !owner || !owner->initialized || !owner->ready || !outReceipt
			|| !owner->allocationGeneration || !owner->target || !owner->targetView
			|| !owner->postprocessGroup || !owner->histogramGroup
			|| owner->postprocessGroup == owner->histogramGroup ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.backend = owner->key.backend;
	receipt.target = owner->target;
	receipt.targetView = owner->targetView;
	receipt.postprocessGroup = owner->postprocessGroup;
	receipt.histogramGroup = owner->histogramGroup;
	receipt.currentSceneColor = owner->key.currentSceneColor;
	receipt.currentPostprocessGroup = owner->key.currentPostprocessGroup;
	receipt.currentHistogramGroup = owner->key.currentHistogramGroup;
	receipt.worldIndex = owner->key.worldIndex;
	receipt.width = owner->key.width;
	receipt.height = owner->key.height;
	receipt.topologyEpoch = owner->key.topologyEpoch;
	receipt.planGeneration = owner->key.planGeneration;
	receipt.sceneColorAttachmentGeneration =
		owner->key.sceneColorAttachmentGeneration;
	receipt.allocationGeneration = owner->allocationGeneration;
	receipt.sceneFormat = owner->key.sceneFormat;
	receipt.ready = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalResolvedHdrHasLive(
		const vkTemporalResolvedHdrOwner_t *owner ) {
	return owner && owner->initialized && ( owner->target || owner->targetView
		|| owner->sampler || owner->postprocessGroup || owner->histogramGroup )
		? qtrue : qfalse;
}

qboolean VK_TemporalResolvedHdrReleaseAfterIdle(
		vkTemporalResolvedHdrOwner_t *owner, qboolean idleProven ) {
	uint32_t generation;
	if ( !owner || !owner->initialized || !idleProven ) return qfalse;
	generation = owner->allocationGeneration;
	DestroyHandles( owner );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->allocationGeneration = generation;
	return qtrue;
}

static qboolean CurrentValid( const vkHdrPostprocessSource_t *current ) {
	const void *identities[4];
	uint32_t i, j;
	if ( !current || !current->backend || !current->attachment
			|| !current->postprocessGroup || !current->histogramGroup
			|| !current->batchToken || !current->frameId || !current->frameCount
			|| current->frameCount > TEMPORAL_RESOLVED_HDR_MAX_COMMAND_SLOTS
			|| current->commandSlot >= current->frameCount
			|| current->worldIndex < 0 || !current->width || !current->height
			|| !current->topologyEpoch || !current->planGeneration
			|| !current->sceneColorAttachmentGeneration
			|| current->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT
			|| current->targetAllocationGeneration || current->resolved ) return qfalse;
	identities[0] = current->backend;
	identities[1] = current->attachment;
	identities[2] = current->postprocessGroup;
	identities[3] = current->histogramGroup;
	for ( i = 0; i < 4; ++i ) for ( j = i + 1; j < 4; ++j )
		if ( identities[i] == identities[j] ) return qfalse;
	return qtrue;
}

static qboolean ContentMatches( const vkHdrPostprocessSource_t *current,
		const vkTemporalResolvedHdrReceipt_t *target,
		const vkTemporalResolvedHdrContentReceipt_t *content ) {
	const void *identities[8];
	uint32_t i, j;
	if ( !( current && target && target->ready && target->backend
		&& target->target && target->targetView && target->currentSceneColor
		&& target->postprocessGroup && target->histogramGroup
		&& target->worldIndex == current->worldIndex
		&& target->width == current->width && target->height == current->height
		&& target->topologyEpoch == current->topologyEpoch
		&& target->planGeneration == current->planGeneration
		&& target->sceneColorAttachmentGeneration ==
			current->sceneColorAttachmentGeneration
		&& target->sceneFormat == current->sceneFormat
		&& target->backend == current->backend
		&& target->currentSceneColor == current->attachment
		&& target->currentPostprocessGroup == current->postprocessGroup
		&& target->currentHistogramGroup == current->histogramGroup
		&& target->allocationGeneration
		&& content && content->valid && !content->submitted
		&& content->batchToken == current->batchToken
		&& content->frameId == current->frameId && content->contentSerial
		&& content->frameCount == current->frameCount
		&& content->commandSlot == current->commandSlot
		&& content->commandSlot < content->frameCount
		&& content->worldIndex == target->worldIndex
		&& content->width == target->width && content->height == target->height
		&& content->topologyEpoch == target->topologyEpoch
		&& content->planGeneration == target->planGeneration
		&& content->sceneColorAttachmentGeneration ==
			target->sceneColorAttachmentGeneration
		&& content->targetAllocationGeneration == target->allocationGeneration
		&& content->backend == target->backend
		&& content->sourceSceneColor == target->currentSceneColor
		&& content->sourcePostprocessGroup == target->currentPostprocessGroup
		&& content->sourceHistogramGroup == target->currentHistogramGroup
		&& content->target == target->target
		&& content->targetView == target->targetView
		&& content->postprocessGroup == target->postprocessGroup
		&& content->histogramGroup == target->histogramGroup
		&& content->sceneFormat == target->sceneFormat
		&& ( content->producer == VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY
			|| content->producer ==
				VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE ) ) ) return qfalse;
	identities[0] = target->backend;
	identities[1] = target->currentSceneColor;
	identities[2] = target->currentPostprocessGroup;
	identities[3] = target->currentHistogramGroup;
	identities[4] = target->target;
	identities[5] = target->targetView;
	identities[6] = target->postprocessGroup;
	identities[7] = target->histogramGroup;
	for ( i = 0; i < 8; ++i ) for ( j = i + 1; j < 8; ++j )
		if ( identities[i] == identities[j] ) return qfalse;
	return qtrue;
}

qboolean VK_TemporalResolvedHdrBuildContentReceipt(
		const vkHdrPostprocessSource_t *current,
		const vkTemporalResolvedHdrReceipt_t *target,
		uint64_t contentSerial,
		vkTemporalResolvedHdrContentReceipt_t *outContent ) {
	return VK_TemporalResolvedHdrBuildContentReceiptForProducer(
		current, target, contentSerial,
		VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY, outContent );
}

qboolean VK_TemporalResolvedHdrBuildContentReceiptForProducer(
		const vkHdrPostprocessSource_t *current,
		const vkTemporalResolvedHdrReceipt_t *target,
		uint64_t contentSerial, vkTemporalResolvedHdrProducer_t producer,
		vkTemporalResolvedHdrContentReceipt_t *outContent ) {
	vkTemporalResolvedHdrContentReceipt_t content;
	if ( !CurrentValid( current ) || !target || !outContent || !contentSerial )
		return qfalse;
	if ( producer != VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY
			&& producer != VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE )
		return qfalse;
	memset( &content, 0, sizeof( content ) );
	content.batchToken = current->batchToken;
	content.frameId = current->frameId;
	content.contentSerial = contentSerial;
	content.commandSlot = current->commandSlot;
	content.frameCount = current->frameCount;
	content.worldIndex = current->worldIndex;
	content.width = current->width;
	content.height = current->height;
	content.topologyEpoch = current->topologyEpoch;
	content.planGeneration = current->planGeneration;
	content.sceneColorAttachmentGeneration =
		current->sceneColorAttachmentGeneration;
	content.targetAllocationGeneration = target->allocationGeneration;
	content.backend = current->backend;
	content.sourceSceneColor = current->attachment;
	content.sourcePostprocessGroup = current->postprocessGroup;
	content.sourceHistogramGroup = current->histogramGroup;
	content.target = target->target;
	content.targetView = target->targetView;
	content.postprocessGroup = target->postprocessGroup;
	content.histogramGroup = target->histogramGroup;
	content.sceneFormat = current->sceneFormat;
	content.producer = producer;
	content.valid = qtrue;
	if ( !ContentMatches( current, target, &content ) ) return qfalse;
	*outContent = content;
	return qtrue;
}

qboolean VK_TemporalResolvedHdrResolveContentSubmit(
		const vkTemporalResolvedHdrContentReceipt_t *recorded,
		qboolean submitted,
		vkTemporalResolvedHdrContentReceipt_t *outSubmitted ) {
	vkTemporalResolvedHdrContentReceipt_t receipt;
	const void *identities[8];
	uint32_t i, j;
	if ( !recorded || !submitted || !outSubmitted || !recorded->valid
			|| recorded->submitted || !recorded->batchToken || !recorded->frameId
			|| !recorded->contentSerial || !recorded->frameCount
			|| recorded->frameCount > TEMPORAL_RESOLVED_HDR_MAX_COMMAND_SLOTS
			|| recorded->commandSlot >= recorded->frameCount
			|| recorded->worldIndex < 0 || !recorded->width || !recorded->height
			|| !recorded->topologyEpoch || !recorded->planGeneration
			|| !recorded->sceneColorAttachmentGeneration
			|| !recorded->targetAllocationGeneration || !recorded->backend
			|| !recorded->sourceSceneColor || !recorded->sourcePostprocessGroup
			|| !recorded->sourceHistogramGroup || !recorded->target
			|| !recorded->targetView || !recorded->postprocessGroup
			|| !recorded->histogramGroup
			|| ( recorded->producer != VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY
				&& recorded->producer !=
					VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE )
			|| recorded->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT )
		return qfalse;
	identities[0] = recorded->backend;
	identities[1] = recorded->sourceSceneColor;
	identities[2] = recorded->sourcePostprocessGroup;
	identities[3] = recorded->sourceHistogramGroup;
	identities[4] = recorded->target;
	identities[5] = recorded->targetView;
	identities[6] = recorded->postprocessGroup;
	identities[7] = recorded->histogramGroup;
	for ( i = 0; i < 8; ++i ) for ( j = i + 1; j < 8; ++j )
		if ( identities[i] == identities[j] ) return qfalse;
	receipt = *recorded;
	receipt.submitted = qtrue;
	*outSubmitted = receipt;
	return qtrue;
}

qboolean VK_TemporalResolvedHdrRouteSource(
		const vkHdrPostprocessSource_t *current,
		const vkTemporalResolvedHdrReceipt_t *target,
		const vkTemporalResolvedHdrContentReceipt_t *content,
		vkHdrPostprocessSource_t *outSource ) {
	vkHdrPostprocessSource_t source;
	if ( !CurrentValid( current ) || !outSource ) return qfalse;
	source = *current;
	if ( ContentMatches( current, target, content ) ) {
		source.backend = target->backend;
		source.attachment = target->target;
		source.postprocessGroup = target->postprocessGroup;
		source.histogramGroup = target->histogramGroup;
		source.width = target->width;
		source.height = target->height;
		source.sceneColorAttachmentGeneration =
			target->sceneColorAttachmentGeneration;
		source.targetAllocationGeneration = target->allocationGeneration;
		source.sceneFormat = target->sceneFormat;
		source.resolved = qtrue;
	}
	*outSource = source;
	return qtrue;
}
