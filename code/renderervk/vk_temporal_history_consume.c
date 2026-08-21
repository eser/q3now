// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_history_consume.h"

#include <limits.h>
#include <math.h>
#include <string.h>
#include "../renderercommon/vulkan/vulkan.h"

typedef struct {
	uint32_t extent[2];
	uint32_t frameLo, frameHi;
	uint32_t worldIndex;
	uint32_t planGeneration;
	uint32_t allocationGeneration;
	uint32_t historyValid;
	uint32_t readIndex;
	uint32_t writeIndex;
	float zNear;
	float zFar;
} vkTemporalHistoryConsumePush_t;

static qboolean PackedHalf2Finite( uint32_t packed ) {
	return ( packed & 0x7c00u ) != 0x7c00u
		&& ( ( packed >> 16 ) & 0x7c00u ) != 0x7c00u ? qtrue : qfalse;
}

static qboolean FloatWordFinitePositive( uint32_t bits ) {
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return isfinite( value ) && value > 0.0f ? qtrue : qfalse;
}

static qboolean KeyValid( const vkTemporalHistoryConsumeKey_t *key ) {
	uint32_t i, j;
	const void *identities[10];
	if ( !key || !key->backend || key->worldIndex < 0 || !key->width || !key->height
			|| !key->topologyEpoch || !key->historyAllocationGeneration
			|| !key->currentColor || !key->currentDepth
			|| key->currentColor == key->currentDepth ) return qfalse;
	identities[0] = key->currentColor; identities[1] = key->currentDepth;
	for ( i = 0; i < 2; ++i ) {
		if ( !key->historyColor[i] || !key->historyColorView[i]
				|| !key->historyDepth[i] || !key->historyDepthView[i] ) return qfalse;
		identities[2 + i * 4] = key->historyColor[i];
		identities[3 + i * 4] = key->historyColorView[i];
		identities[4 + i * 4] = key->historyDepth[i];
		identities[5 + i * 4] = key->historyDepthView[i];
	}
	for ( i = 0; i < 10; ++i ) for ( j = i + 1; j < 10; ++j )
		if ( identities[i] == identities[j] ) return qfalse;
	return qtrue;
}

static qboolean KeyEqual( const vkTemporalHistoryConsumeKey_t *a,
		const vkTemporalHistoryConsumeKey_t *b ) {
	return a && b && a->backend == b->backend && a->worldIndex == b->worldIndex
		&& a->width == b->width && a->height == b->height
		&& a->topologyEpoch == b->topologyEpoch
		&& a->historyAllocationGeneration == b->historyAllocationGeneration
		&& a->currentColor == b->currentColor && a->currentDepth == b->currentDepth
		&& a->historyColor[0] == b->historyColor[0]
		&& a->historyColor[1] == b->historyColor[1]
		&& a->historyColorView[0] == b->historyColorView[0]
		&& a->historyColorView[1] == b->historyColorView[1]
		&& a->historyDepth[0] == b->historyDepth[0]
		&& a->historyDepth[1] == b->historyDepth[1]
		&& a->historyDepthView[0] == b->historyDepthView[0]
		&& a->historyDepthView[1] == b->historyDepthView[1] ? qtrue : qfalse;
}

static qboolean TicketMatchesOwner( const vkTemporalHistoryConsumeOwner_t *owner,
		uint32_t frameIndex, const vkTemporalHistoryConsumeTicket_t *ticket ) {
	const temporalHistoryCommittedReceipt_t *prior;
	if ( !owner || !owner->ready || !ticket || frameIndex >= owner->frameCount
			|| !ticket->frameId || !ticket->planGeneration
			|| ticket->worldIndex != owner->key.worldIndex
			|| ticket->historyAllocationGeneration != owner->key.historyAllocationGeneration
			|| ticket->historyReadIndex > 1 || ticket->historyWriteIndex > 1
			|| ticket->historyReadIndex == ticket->historyWriteIndex
			|| ticket->width != owner->key.width || ticket->height != owner->key.height
			|| ticket->topologyEpoch != owner->key.topologyEpoch
			|| ticket->commandSlot != frameIndex || !ticket->captureSerial
			|| !ticket->bufferAllocationGeneration
			|| ticket->bufferAllocationGeneration != owner->slots[frameIndex].allocationGeneration
			|| !ticket->historyValid ) return qfalse;
	prior = &ticket->previousCommitted;
	return prior->valid && prior->frameId != UINT64_MAX
		&& prior->frameId + 1u == ticket->frameId
		&& prior->worldIndex == ticket->worldIndex
		&& prior->planGeneration == ticket->planGeneration
		&& prior->allocationGeneration == ticket->historyAllocationGeneration
		&& prior->historyIndex == ticket->historyReadIndex
		&& prior->width == ticket->width && prior->height == ticket->height
		&& prior->topologyEpoch == ticket->topologyEpoch
		&& prior->color == owner->key.historyColor[ticket->historyReadIndex]
		&& prior->colorView == owner->key.historyColorView[ticket->historyReadIndex]
		&& prior->depth == owner->key.historyDepth[ticket->historyReadIndex]
		&& prior->depthView == owner->key.historyDepthView[ticket->historyReadIndex]
		? qtrue : qfalse;
}

static qboolean IsBorrowedHistoryView( const vkTemporalHistoryConsumeKey_t *key,
		const ralTextureView_t *view ) {
	return key && view && ( view == key->historyColorView[0]
		|| view == key->historyColorView[1] || view == key->historyDepthView[0]
		|| view == key->historyDepthView[1] ) ? qtrue : qfalse;
}

static qboolean AliasesLiveView( const vkTemporalHistoryConsumeOwner_t *live,
		const ralTextureView_t *view ) {
	return live && view && ( view == live->currentColorView
		|| view == live->currentDepthView ) ? qtrue : qfalse;
}

static qboolean AliasesLiveBuffer( const vkTemporalHistoryConsumeOwner_t *live,
		const ralBuffer_t *buffer ) {
	if ( !live || !buffer ) return qfalse;
	for ( uint32_t i=0; i<VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i )
		if ( buffer == live->slots[i].gpuBuffer
				|| buffer == live->slots[i].readbackBuffer ) return qtrue;
	return qfalse;
}

static qboolean AliasesLiveGroup( const vkTemporalHistoryConsumeOwner_t *live,
		const ralBindGroup_t *group ) {
	if ( !live || !group ) return qfalse;
	for ( uint32_t i=0; i<VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i )
		for ( uint32_t r=0; r<2; ++r ) if ( group == live->slots[i].groups[r] ) return qtrue;
	return qfalse;
}

static void SanitizeCandidateAliases( vkTemporalHistoryConsumeOwner_t *candidate,
		const vkTemporalHistoryConsumeOwner_t *live,
		const vkTemporalHistoryConsumeKey_t *key ) {
	if ( AliasesLiveView(live,candidate->currentColorView)
			|| IsBorrowedHistoryView(key,candidate->currentColorView) ) candidate->currentColorView=NULL;
	if ( AliasesLiveView(live,candidate->currentDepthView)
			|| IsBorrowedHistoryView(key,candidate->currentDepthView) ) candidate->currentDepthView=NULL;
	if ( live && candidate->sampler == live->sampler ) candidate->sampler=NULL;
	if ( live && candidate->layout == live->layout ) candidate->layout=NULL;
	if ( live && candidate->pipeline == live->pipeline ) candidate->pipeline=NULL;
	for(uint32_t i=0;i<VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES;++i){
		if(AliasesLiveBuffer(live,candidate->slots[i].gpuBuffer))
			candidate->slots[i].gpuBuffer=NULL;
		if(AliasesLiveBuffer(live,candidate->slots[i].readbackBuffer))
			candidate->slots[i].readbackBuffer=NULL;
		for(uint32_t r=0;r<2;++r) if(AliasesLiveGroup(live,candidate->slots[i].groups[r]))
			candidate->slots[i].groups[r]=NULL;
		for(uint32_t j=0;j<i;++j){
			if(candidate->slots[i].gpuBuffer==candidate->slots[j].gpuBuffer)
				candidate->slots[i].gpuBuffer=NULL;
			if(candidate->slots[i].readbackBuffer==candidate->slots[j].readbackBuffer)
				candidate->slots[i].readbackBuffer=NULL;
			for(uint32_t r=0;r<2;++r) for(uint32_t q=0;q<2;++q)
				if(candidate->slots[i].groups[r]
						&& candidate->slots[i].groups[r]==candidate->slots[j].groups[q])
					candidate->slots[i].groups[r]=NULL;
		}
		if(candidate->slots[i].groups[1]==candidate->slots[i].groups[0])
			candidate->slots[i].groups[1]=NULL;
	}
}

static void DestroyOwner( vkTemporalHistoryConsumeOwner_t *owner ) {
	uint32_t i, r;
	if ( !owner ) return;
	if ( owner->pipeline ) Ral_DestroyPipeline( owner->pipeline );
	for ( i = 0; i < VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i ) {
		for ( r = 0; r < 2; ++r ) if ( owner->slots[i].groups[r] )
			Ral_DestroyBindGroup( owner->slots[i].groups[r] );
	}
	if ( owner->layout ) Ral_DestroyBindGroupLayout( owner->layout );
	if ( owner->sampler ) Ral_DestroySampler( owner->sampler );
	if ( owner->currentColorView ) Ral_DestroyTextureView( owner->currentColorView );
	if ( owner->currentDepthView && owner->currentDepthView != owner->currentColorView )
		Ral_DestroyTextureView( owner->currentDepthView );
	for ( i = 0; i < VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i ) {
		if ( owner->slots[i].readbackBuffer )
			Ral_DestroyBuffer( owner->slots[i].readbackBuffer );
		if ( owner->slots[i].gpuBuffer )
			Ral_DestroyBuffer( owner->slots[i].gpuBuffer );
	}
}

static qboolean TransitionBuffer( ralCommandBuffer_t *commandBuffer,
		ralBuffer_t *buffer, ralResourceUsage_t before,
		ralResourceUsage_t after, uint32_t beforeStages,
		uint32_t afterStages ) {
	ralBufferTransition_t transition;
	ralResourceTransitionBatch_t batch;
	if ( !commandBuffer || !buffer ) return qfalse;
	memset( &transition, 0, sizeof( transition ) );
	transition.buffer = buffer; transition.size = VK_TEMPORAL_HISTORY_WITNESS_BYTES;
	transition.before.usage = before; transition.before.shaderStages = beforeStages;
	transition.after.usage = after; transition.after.shaderStages = afterStages;
	transition.sourceQueue = transition.destinationQueue = RAL_QUEUE_GRAPHICS;
	memset( &batch, 0, sizeof( batch ) );
	batch.bufferTransitions = &transition; batch.bufferTransitionCount = 1;
	return Ral_CmdTransitionResources( commandBuffer, &batch ) == ralSuccess
		? qtrue : qfalse;
}

void VK_TemporalHistoryConsumeInit( vkTemporalHistoryConsumeOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalHistoryConsumeArm(
		vkTemporalHistoryConsumeOwner_t *owner, uint32_t captureCount ) {
	uint32_t i;
	if ( !owner || !captureCount || captureCount > VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES )
		return qfalse;
	if ( !owner->initialized ) VK_TemporalHistoryConsumeInit( owner );
	if ( owner->capturesRemaining ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i )
		if ( owner->slots[i].state == VK_TEMPORAL_HISTORY_CONSUME_RECORDED
				|| owner->slots[i].state == VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED ) return qfalse;
	owner->capturesRemaining = captureCount;
	return qtrue;
}

qboolean VK_TemporalHistoryConsumeIsArmed(
		const vkTemporalHistoryConsumeOwner_t *owner ) {
	return owner && owner->initialized && owner->capturesRemaining ? qtrue : qfalse;
}

qboolean VK_TemporalHistoryConsumeEnsureAfterFence(
		vkTemporalHistoryConsumeOwner_t *owner,
		const vkTemporalHistoryConsumeKey_t *key, uint32_t frameCount,
		uint32_t frameIndex, const uint32_t *computeSpirv, size_t computeSpirvSize ) {
	vkTemporalHistoryConsumeOwner_t candidate;
	ralTextureViewCreateInfo_t vci;
	ralSamplerCreateInfo_t sci;
	ralBindEntry_t entries[6];
	ralBindGroupLayoutCreateInfo_t lci;
	ralComputePipelineCreateInfo_t pci;
	const ralBindGroupLayout_t *layouts[1];
	uint32_t i, r;
	if ( !owner || !owner->initialized || !owner->capturesRemaining
			|| !KeyValid( key ) || !frameCount
			|| frameCount > VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES
			|| frameIndex >= frameCount || !computeSpirv || !computeSpirvSize ) return qfalse;
	if ( owner->ready && owner->frameCount == frameCount && KeyEqual( &owner->key, key ) ) {
		if ( owner->slots[frameIndex].state != VK_TEMPORAL_HISTORY_CONSUME_EMPTY
				&& owner->slots[frameIndex].state != VK_TEMPORAL_HISTORY_CONSUME_READY )
			return qfalse;
		memset( &owner->slots[frameIndex].ticket, 0,
			sizeof( owner->slots[frameIndex].ticket ) );
		owner->slots[frameIndex].state = VK_TEMPORAL_HISTORY_CONSUME_READY;
		return qtrue;
	}
	// Replacement needs device/global-idle authority because the existing views,
	// descriptors and per-slot buffers may still be referenced by another slot.
	// Product parent teardown releases this owner before changing any exact key.
	if ( owner->ready ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i )
		if ( owner->slots[i].state == VK_TEMPORAL_HISTORY_CONSUME_RECORDED
				|| owner->slots[i].state == VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.initialized = qtrue; candidate.key = *key; candidate.frameCount = frameCount;
	candidate.capturesRemaining = owner->capturesRemaining;
	candidate.nextCaptureSerial = owner->nextCaptureSerial;
	memset( &vci, 0, sizeof( vci ) ); vci.viewType = RAL_TEXTURE_2D;
	vci.texture = key->currentColor;
	candidate.currentColorView = Ral_CreateTextureView( key->backend, &vci );
	if ( AliasesLiveView(owner,candidate.currentColorView)
			|| IsBorrowedHistoryView(key,candidate.currentColorView) ) goto fail;
	vci.texture = key->currentDepth;
	candidate.currentDepthView = Ral_CreateTextureView( key->backend, &vci );
	if ( !candidate.currentColorView || !candidate.currentDepthView
			|| candidate.currentColorView == candidate.currentDepthView
			|| AliasesLiveView(owner,candidate.currentDepthView)
			|| IsBorrowedHistoryView(key,candidate.currentDepthView) ) goto fail;
	memset( &sci, 0, sizeof( sci ) ); sci.minFilter = sci.magFilter = RAL_FILTER_NEAREST;
	sci.mipmapMode = RAL_MIPMAP_NEAREST;
	sci.addressU = sci.addressV = sci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
	sci.maxAnisotropy = 1.0f; sci.debugName = "wired-temporal-history-consume-sampler";
	candidate.sampler = Ral_CreateSampler( key->backend, &sci );
	if ( candidate.sampler == owner->sampler ) goto fail;
	memset( entries, 0, sizeof( entries ) );
	for ( i = 0; i < 4; ++i ) entries[i] = (ralBindEntry_t){ i, RAL_BIND_SAMPLED_TEXTURE, 1, RAL_STAGE_COMPUTE };
	entries[4] = (ralBindEntry_t){ 4, RAL_BIND_SAMPLER, 1, RAL_STAGE_COMPUTE };
	entries[5] = (ralBindEntry_t){ 5, RAL_BIND_STORAGE_BUFFER, 1, RAL_STAGE_COMPUTE };
	memset( &lci, 0, sizeof( lci ) ); lci.entries = entries; lci.numEntries = 6;
	lci.debugName = "wired-temporal-history-consume-bgl";
	candidate.layout = Ral_CreateBindGroupLayout( key->backend, &lci );
	if ( !candidate.sampler || !candidate.layout || candidate.layout == owner->layout ) goto fail;
	for ( i = 0; i < frameCount; ++i ) {
		ralBufferCreateInfo_t bci;
		memset( &bci, 0, sizeof( bci ) ); bci.size = VK_TEMPORAL_HISTORY_WITNESS_BYTES;
		bci.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_SRC;
		bci.memory = RAL_MEMORY_DEVICE_LOCAL;
		bci.debugName = "wired-temporal-history-witness";
		candidate.slots[i].gpuBuffer = Ral_CreateBuffer( key->backend, &bci );
		if ( !candidate.slots[i].gpuBuffer
				|| AliasesLiveBuffer(owner,candidate.slots[i].gpuBuffer) ) goto fail;
		for ( uint32_t j = 0; j < i; ++j )
			if ( candidate.slots[i].gpuBuffer == candidate.slots[j].gpuBuffer ) goto fail;
		bci.usage = RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ;
		bci.memory = RAL_MEMORY_HOST_COHERENT;
		bci.debugName = "wired-temporal-history-witness-readback";
		candidate.slots[i].readbackBuffer = Ral_CreateBuffer( key->backend, &bci );
		if ( !candidate.slots[i].readbackBuffer
				|| candidate.slots[i].readbackBuffer == candidate.slots[i].gpuBuffer
				|| AliasesLiveBuffer(owner,candidate.slots[i].readbackBuffer) ) goto fail;
		for ( uint32_t j = 0; j < i; ++j )
			if ( candidate.slots[i].readbackBuffer
					== candidate.slots[j].readbackBuffer ) goto fail;
		candidate.slots[i].allocationGeneration = owner->slots[i].allocationGeneration + 1u;
		if ( !candidate.slots[i].allocationGeneration ) goto fail;
		for ( r = 0; r < 2; ++r ) {
			ralBindingValue_t values[6]; ralBindGroupCreateInfo_t gci;
			memset( values, 0, sizeof( values ) );
			values[0] = (ralBindingValue_t){ .binding=0, .type=RAL_BIND_SAMPLED_TEXTURE, .textureView=candidate.currentColorView };
			values[1] = (ralBindingValue_t){ .binding=1, .type=RAL_BIND_SAMPLED_TEXTURE, .textureView=candidate.currentDepthView };
			values[2] = (ralBindingValue_t){ .binding=2, .type=RAL_BIND_SAMPLED_TEXTURE, .textureView=key->historyColorView[r] };
			values[3] = (ralBindingValue_t){ .binding=3, .type=RAL_BIND_SAMPLED_TEXTURE, .textureView=key->historyDepthView[r] };
			values[4] = (ralBindingValue_t){ .binding=4, .type=RAL_BIND_SAMPLER, .sampler=candidate.sampler };
			values[5] = (ralBindingValue_t){ .binding=5, .type=RAL_BIND_STORAGE_BUFFER, .buffer=candidate.slots[i].gpuBuffer, .bufferRange=VK_TEMPORAL_HISTORY_WITNESS_BYTES };
			memset( &gci, 0, sizeof( gci ) ); gci.layout = candidate.layout;
			gci.values = values; gci.numValues = 6;
			gci.debugName = "wired-temporal-history-consume-bg";
			candidate.slots[i].groups[r] = Ral_CreateBindGroup( key->backend, &gci );
			if ( !candidate.slots[i].groups[r]
					|| AliasesLiveGroup(owner,candidate.slots[i].groups[r]) ) goto fail;
			for(uint32_t j=0;j<=i;++j) for(uint32_t q=0;q<2;++q)
				if((j!=i||q!=r) && candidate.slots[i].groups[r]==candidate.slots[j].groups[q]) goto fail;
		}
	}
	layouts[0] = candidate.layout; memset( &pci, 0, sizeof( pci ) );
	pci.computeSpirv = computeSpirv; pci.computeSpirvSize = computeSpirvSize;
	pci.bindGroupLayouts = layouts; pci.numBindGroupLayouts = 1;
	pci.pushConstantSize = sizeof( vkTemporalHistoryConsumePush_t );
	pci.debugName = "wired-temporal-history-consume-cs";
	candidate.pipeline = Ral_CreateComputePipeline( key->backend, &pci );
	if ( !candidate.pipeline || candidate.pipeline == owner->pipeline ) goto fail;
	candidate.ready = qtrue;
	DestroyOwner( owner );
	*owner = candidate;
	owner->slots[frameIndex].state = VK_TEMPORAL_HISTORY_CONSUME_READY;
	return qtrue;
fail:
	SanitizeCandidateAliases( &candidate, owner, key );
	DestroyOwner( &candidate );
	return qfalse;
}

qboolean VK_TemporalHistoryConsumeRecord(
	vkTemporalHistoryConsumeOwner_t *owner, ralCommandBuffer_t *commandBuffer,
		uint32_t frameIndex, const ralTemporalFramePlan_t *plan,
		const temporalHistoryFrameView_t *view, float zNear, float zFar ) {
	vkTemporalHistoryConsumeSlot_t *slot;
	vkTemporalHistoryConsumePush_t push;
	ralBufferCopy_t copy;
	uint32_t readIndex;
	if ( !owner || !owner->ready || !owner->capturesRemaining || !commandBuffer
			|| frameIndex >= owner->frameCount || !plan || !view
			|| !plan->enabled || !plan->frameId || !plan->generation
			|| plan->historyReadIndex > 1 || plan->historyWriteIndex > 1
			|| view->historyValid != ( plan->historyValid ? qtrue : qfalse )
			|| !plan->historyValid || !view->committed.valid
			|| view->readIndex != plan->historyReadIndex
			|| view->writeIndex != plan->historyWriteIndex
			|| !isfinite( zNear ) || !isfinite( zFar ) || zNear <= 0.0f || zFar <= zNear
			|| owner->nextCaptureSerial == UINT32_MAX ) return qfalse;
	readIndex = plan->historyReadIndex;
	if ( plan->historyValid && plan->historyReadIndex == plan->historyWriteIndex ) return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_HISTORY_CONSUME_READY || !slot->gpuBuffer
			|| !slot->readbackBuffer || !slot->groups[readIndex] ) return qfalse;
	memset( &push, 0, sizeof( push ) ); push.extent[0]=owner->key.width; push.extent[1]=owner->key.height;
	push.frameLo=(uint32_t)plan->frameId; push.frameHi=(uint32_t)(plan->frameId>>32);
	push.worldIndex=(uint32_t)owner->key.worldIndex; push.planGeneration=plan->generation;
	push.allocationGeneration=owner->key.historyAllocationGeneration;
	push.historyValid=plan->historyValid; push.readIndex=readIndex; push.writeIndex=plan->historyWriteIndex;
	push.zNear=zNear; push.zFar=zFar;
	if ( !slot->gpuWritable ) {
		if ( !TransitionBuffer( commandBuffer, slot->gpuBuffer,
				RAL_RESOURCE_USAGE_UNDEFINED, RAL_RESOURCE_USAGE_STORAGE_WRITE,
				0, RAL_STAGE_COMPUTE ) ) return qfalse;
		slot->gpuWritable = qtrue;
	}
	Ral_CmdBindPipeline( commandBuffer, owner->pipeline );
	Ral_CmdBindBindGroup( commandBuffer, 0, slot->groups[readIndex] );
	Ral_CmdPushConstants( commandBuffer, RAL_STAGE_COMPUTE, 0, sizeof( push ), &push );
	Ral_CmdDispatch( commandBuffer, 1, 1, 1 );
	if ( !TransitionBuffer( commandBuffer, slot->gpuBuffer,
			RAL_RESOURCE_USAGE_STORAGE_WRITE, RAL_RESOURCE_USAGE_COPY_SOURCE,
			RAL_STAGE_COMPUTE, 0 )
			|| !TransitionBuffer( commandBuffer, slot->readbackBuffer,
				slot->hostReadable ? RAL_RESOURCE_USAGE_HOST_READ
					: RAL_RESOURCE_USAGE_UNDEFINED,
				RAL_RESOURCE_USAGE_COPY_DESTINATION, 0, 0 ) ) return qfalse;
	memset( &copy, 0, sizeof( copy ) ); copy.size = VK_TEMPORAL_HISTORY_WITNESS_BYTES;
	Ral_CmdCopyBuffer( commandBuffer, slot->gpuBuffer, slot->readbackBuffer, &copy );
	if ( !TransitionBuffer( commandBuffer, slot->readbackBuffer,
			RAL_RESOURCE_USAGE_COPY_DESTINATION, RAL_RESOURCE_USAGE_HOST_READ, 0, 0 )
			|| !TransitionBuffer( commandBuffer, slot->gpuBuffer,
				RAL_RESOURCE_USAGE_COPY_SOURCE, RAL_RESOURCE_USAGE_STORAGE_WRITE,
				0, RAL_STAGE_COMPUTE ) ) return qfalse;
	slot->hostReadable = qtrue;
	memset( &slot->ticket, 0, sizeof( slot->ticket ) );
	slot->ticket.frameId=plan->frameId; slot->ticket.worldIndex=owner->key.worldIndex;
	slot->ticket.planGeneration=plan->generation; slot->ticket.historyAllocationGeneration=owner->key.historyAllocationGeneration;
	slot->ticket.historyReadIndex=plan->historyReadIndex; slot->ticket.historyWriteIndex=plan->historyWriteIndex;
	slot->ticket.width=owner->key.width; slot->ticket.height=owner->key.height; slot->ticket.topologyEpoch=owner->key.topologyEpoch;
	slot->ticket.commandSlot=frameIndex; slot->ticket.captureSerial=owner->nextCaptureSerial+1u;
	slot->ticket.bufferAllocationGeneration=slot->allocationGeneration; slot->ticket.historyValid=plan->historyValid?qtrue:qfalse;
	slot->ticket.previousCommitted=view->committed;
	owner->nextCaptureSerial=slot->ticket.captureSerial; slot->state=VK_TEMPORAL_HISTORY_CONSUME_RECORDED;
	return qtrue;
}

qboolean VK_TemporalHistoryConsumeAcceptStore(
		vkTemporalHistoryConsumeOwner_t *owner, uint32_t frameIndex,
		qboolean storeRecorded ) {
	vkTemporalHistoryConsumeSlot_t *slot;
	if ( !owner || !owner->ready || frameIndex >= owner->frameCount ) return qfalse;
	slot=&owner->slots[frameIndex]; if ( slot->state != VK_TEMPORAL_HISTORY_CONSUME_RECORDED ) return qfalse;
	if ( !TicketMatchesOwner( owner, frameIndex, &slot->ticket ) ) {
		memset(&slot->ticket,0,sizeof(slot->ticket)); slot->state=VK_TEMPORAL_HISTORY_CONSUME_READY;
		return qfalse;
	}
	if ( !storeRecorded ) { memset(&slot->ticket,0,sizeof(slot->ticket)); slot->state=VK_TEMPORAL_HISTORY_CONSUME_READY; return qfalse; }
	slot->ticket.storeAccepted=qtrue; return qtrue;
}

qboolean VK_TemporalHistoryConsumeResolveSubmit(
		vkTemporalHistoryConsumeOwner_t *owner, uint32_t frameIndex,
		qboolean submitted, const temporalHistoryCommittedReceipt_t committed[2],
		vkTemporalHistoryConsumeTicket_t *outTicket ) {
	vkTemporalHistoryConsumeSlot_t *slot;
	const temporalHistoryCommittedReceipt_t *published;
	if ( !owner || !owner->ready || frameIndex >= owner->frameCount ) return qfalse;
	slot=&owner->slots[frameIndex]; if ( slot->state != VK_TEMPORAL_HISTORY_CONSUME_RECORDED ) return qfalse;
	if ( !TicketMatchesOwner( owner, frameIndex, &slot->ticket ) ) {
		memset(&slot->ticket,0,sizeof(slot->ticket)); slot->state=VK_TEMPORAL_HISTORY_CONSUME_READY;
		return qfalse;
	}
	published = committed && slot->ticket.historyWriteIndex < 2
		? &committed[slot->ticket.historyWriteIndex] : NULL;
	if ( !submitted || !slot->ticket.storeAccepted || !published || !published->valid
			|| published->worldIndex != slot->ticket.worldIndex || published->frameId != slot->ticket.frameId
			|| published->planGeneration != slot->ticket.planGeneration
			|| published->allocationGeneration != slot->ticket.historyAllocationGeneration
			|| published->historyIndex != slot->ticket.historyWriteIndex
			|| published->width != slot->ticket.width || published->height != slot->ticket.height
			|| published->topologyEpoch != slot->ticket.topologyEpoch
			|| published->color != owner->key.historyColor[slot->ticket.historyWriteIndex]
			|| published->colorView != owner->key.historyColorView[slot->ticket.historyWriteIndex]
			|| published->depth != owner->key.historyDepth[slot->ticket.historyWriteIndex]
			|| published->depthView != owner->key.historyDepthView[slot->ticket.historyWriteIndex] ) {
		memset(&slot->ticket,0,sizeof(slot->ticket)); slot->state=VK_TEMPORAL_HISTORY_CONSUME_READY; return qfalse;
	}
	slot->ticket.submitted=qtrue; slot->state=VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED;
	if ( owner->capturesRemaining ) owner->capturesRemaining--;
	if ( outTicket ) *outTicket=slot->ticket; return qtrue;
}

qboolean VK_TemporalHistoryConsumeCompleteAfterFence(
		vkTemporalHistoryConsumeOwner_t *owner, uint32_t frameIndex,
		qboolean fenceProven, vkTemporalHistoryConsumeReceipt_t *outReceipt ) {
	vkTemporalHistoryConsumeSlot_t *slot; vkTemporalHistoryConsumeReceipt_t receipt;
	const uint32_t *words;
	ralBufferMapRequest_t mapRequest;
	ralBufferMapTicket_t mapTicket;
	if ( !owner || !owner->ready || !fenceProven || frameIndex >= owner->frameCount ) return qfalse;
	slot=&owner->slots[frameIndex]; if ( slot->state != VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED
			|| !slot->readbackBuffer || !slot->hostReadable || !slot->ticket.submitted
			|| !TicketMatchesOwner( owner, frameIndex, &slot->ticket ) ) return qfalse;
	memset( &mapRequest, 0, sizeof( mapRequest ) );
	mapRequest.mode = RAL_MAP_READ; mapRequest.size = VK_TEMPORAL_HISTORY_WITNESS_BYTES;
	if ( Ral_BufferMapBegin( slot->readbackBuffer, &mapRequest, &mapTicket )
			!= ralSuccess || !mapTicket.mappedRange ) return qfalse;
	words=(const uint32_t *)mapTicket.mappedRange; memset(&receipt,0,sizeof(receipt)); receipt.ticket=slot->ticket;
	receipt.currentColorRG=words[11]; receipt.currentColorBA=words[12]; receipt.currentDepth=words[13];
	receipt.previousColorRG=words[14]; receipt.previousColorBA=words[15]; receipt.previousDepth=words[16];
	receipt.fenceComplete=qtrue;
	receipt.ready = words[0]==VK_TEMPORAL_HISTORY_WITNESS_MAGIC
		&& words[1]==(slot->ticket.historyValid?1u:0u)
		&& words[2]==(uint32_t)slot->ticket.frameId && words[3]==(uint32_t)(slot->ticket.frameId>>32)
		&& words[4]==(uint32_t)slot->ticket.worldIndex && words[5]==slot->ticket.planGeneration
		&& words[6]==slot->ticket.historyAllocationGeneration
		&& words[7]==slot->ticket.historyReadIndex && words[8]==slot->ticket.historyWriteIndex
		&& words[9]==slot->ticket.width && words[10]==slot->ticket.height
		&& (words[11]!=0x7f7f7f7fu || words[12]!=0x7f7f7f7fu || words[13]!=0x7f7f7f7fu)
		&& PackedHalf2Finite(words[11]) && PackedHalf2Finite(words[12])
		&& FloatWordFinitePositive(words[13])
		&& ( !slot->ticket.historyValid || (words[14]!=0x7f7f7f7fu || words[15]!=0x7f7f7f7fu || words[16]!=0x7f7f7f7fu) ) ? qtrue:qfalse;
	if ( receipt.ready && slot->ticket.historyValid )
		receipt.ready = PackedHalf2Finite(words[14])
			&& PackedHalf2Finite(words[15]) && FloatWordFinitePositive(words[16]);
	if ( Ral_BufferMapUnmap( slot->readbackBuffer, &mapTicket ) != ralSuccess )
		return qfalse;
	owner->latest=receipt; memset(&slot->ticket,0,sizeof(slot->ticket)); slot->state=VK_TEMPORAL_HISTORY_CONSUME_READY;
	if ( outReceipt ) *outReceipt=receipt; return qtrue;
}

qboolean VK_TemporalHistoryConsumeHasLive( const vkTemporalHistoryConsumeOwner_t *owner ) {
	if ( !owner || !owner->initialized ) return qfalse;
	if ( owner->capturesRemaining || owner->ready || owner->latest.fenceComplete
			|| owner->currentColorView || owner->currentDepthView || owner->sampler
			|| owner->layout || owner->pipeline ) return qtrue;
	for ( uint32_t i=0; i<VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES; ++i )
		if ( owner->slots[i].gpuBuffer || owner->slots[i].readbackBuffer
				|| owner->slots[i].groups[0] || owner->slots[i].groups[1] ) return qtrue;
	return qfalse;
}

qboolean VK_TemporalHistoryConsumeReleaseAfterIdle(
		vkTemporalHistoryConsumeOwner_t *owner, qboolean idleProven ) {
	uint32_t serial;
	if ( !owner || !owner->initialized || !idleProven ) return qfalse;
	for ( uint32_t i=0;i<VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES;++i )
		if ( owner->slots[i].state==VK_TEMPORAL_HISTORY_CONSUME_RECORDED
				|| owner->slots[i].state==VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED ) return qfalse;
	serial=owner->nextCaptureSerial; DestroyOwner(owner); memset(owner,0,sizeof(*owner)); owner->initialized=qtrue; owner->nextCaptureSerial=serial; return qtrue;
}
