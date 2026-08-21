// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_transition.c — semantic whole-resource transition recording.
//
// The public descriptors remain backend-neutral. This Vulkan implementation
// lowers them only after the complete batch validates. Shared physical queues
// use one command; distinct families use the paired generation-bound release/
// acquire commands below. WebGPU can consume the same lifecycle as logical
// scheduler validation without exposing native queue-family indices.

#include "ral_vulkan_internal.h"

#define RAL_VK_TRANSITION_MAX_BUFFERS  8u
#define RAL_VK_TRANSITION_MAX_TEXTURES 16u

static qboolean ralVk_StateEqual( const ralResourceState_t *a,
	                              const ralResourceState_t *b ) {
	return a->usage == b->usage && a->shaderStages == b->shaderStages;
}

static ralTextureAspectFlags_t ralVk_PortableAspects( VkImageAspectFlags aspects ) {
	ralTextureAspectFlags_t out = 0;
	if ( aspects & VK_IMAGE_ASPECT_COLOR_BIT ) out |= RAL_TEXTURE_ASPECT_COLOR;
	if ( aspects & VK_IMAGE_ASPECT_DEPTH_BIT ) out |= RAL_TEXTURE_ASPECT_DEPTH;
	if ( aspects & VK_IMAGE_ASPECT_STENCIL_BIT ) out |= RAL_TEXTURE_ASPECT_STENCIL;
	return out;
}

static VkImageAspectFlags ralVk_NativeAspects( ralTextureAspectFlags_t aspects ) {
	VkImageAspectFlags out = 0;
	if ( aspects & RAL_TEXTURE_ASPECT_COLOR ) out |= VK_IMAGE_ASPECT_COLOR_BIT;
	if ( aspects & RAL_TEXTURE_ASPECT_DEPTH ) out |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( aspects & RAL_TEXTURE_ASPECT_STENCIL ) out |= VK_IMAGE_ASPECT_STENCIL_BIT;
	return out;
}

ralResult_t Ral_CmdTransitionResources( ralCommandBuffer_t *cb,
	                                     const ralResourceTransitionBatch_t *batch ) {
	VkBufferMemoryBarrier bufferBarriers[ RAL_VK_TRANSITION_MAX_BUFFERS ];
	VkImageMemoryBarrier imageBarriers[ RAL_VK_TRANSITION_MAX_TEXTURES ];
	VkPipelineStageFlags srcStages = 0;
	VkPipelineStageFlags dstStages = 0;
	uint32_t i, j;

	if ( !cb || !batch || !cb->backend
	  || ( !cb->externalLifecycle && cb->state != RAL_VK_CMD_RECORDING )
	  || cb->renderingActive
	  || !cb->backend->vk.CmdPipelineBarrier
	  || batch->bufferTransitionCount > RAL_VK_TRANSITION_MAX_BUFFERS
	  || batch->textureTransitionCount > RAL_VK_TRANSITION_MAX_TEXTURES
	  || ( batch->bufferTransitionCount == 0 && batch->textureTransitionCount == 0 )
	  || ( batch->bufferTransitionCount != 0 && !batch->bufferTransitions )
	  || ( batch->textureTransitionCount != 0 && !batch->textureTransitions ) )
		return ralErrorInvalidArgument;

	for ( i = 0; i < batch->bufferTransitionCount; ++i ) {
		const ralBufferTransition_t *transition = &batch->bufferTransitions[i];
		const ralBuffer_t *buffer = transition->buffer;
		ralVkResourceStateTranslation_t before, after;
		if ( !buffer || buffer->backend != cb->backend || buffer->buffer == VK_NULL_HANDLE
		  || !buffer->portableStateKnown
		  || buffer->queueTransfer.pending.ready
		  || !ralVk_BufferGpuUseAllowed( buffer )
		  || !Ral_BufferTransitionValid( transition, (uint64_t)buffer->size )
		  || transition->offset != 0 || transition->size != (uint64_t)buffer->size
		  || !ralVk_StateEqual( &buffer->portableState, &transition->before )
		  || ( transition->before.usage != RAL_RESOURCE_USAGE_UNDEFINED
		    && buffer->portableOwnerQueue != transition->sourceQueue )
		  || cb->queue != transition->destinationQueue
		  || !ralVk_TranslateBufferResourceState( &transition->before, &before )
		  || !ralVk_TranslateBufferResourceState( &transition->after, &after ) )
			return ralErrorInvalidArgument;
		if ( cb->backend->queueFamily[transition->sourceQueue]
		  != cb->backend->queueFamily[transition->destinationQueue] )
			return ralUnsupported;
		for ( j = 0; j < i; ++j ) {
			if ( batch->bufferTransitions[j].buffer == buffer
			  || batch->bufferTransitions[j].buffer->buffer == buffer->buffer )
				return ralErrorInvalidArgument;
		}
		RAL_ZERO( bufferBarriers[i] );
		bufferBarriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarriers[i].srcAccessMask = before.access;
		bufferBarriers[i].dstAccessMask = after.access;
		bufferBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarriers[i].buffer = buffer->buffer;
		bufferBarriers[i].offset = 0;
		bufferBarriers[i].size = buffer->size;
		srcStages |= before.stage;
		dstStages |= after.stage;
	}

	for ( i = 0; i < batch->textureTransitionCount; ++i ) {
		const ralTextureTransition_t *transition = &batch->textureTransitions[i];
		const ralTexture_t *texture = transition->texture;
		ralVkResourceStateTranslation_t before, after;
		ralTextureAspectFlags_t availableAspects;
		if ( !texture || texture->backend != cb->backend || texture->image == VK_NULL_HANDLE )
			return ralErrorInvalidArgument;
		availableAspects = ralVk_PortableAspects( texture->aspect );
		if ( !texture->portableStateKnown
		  || texture->queueTransfer.pending.ready
		  || !Ral_TextureTransitionValid( transition, texture->mipLevels,
			texture->arrayLayers, availableAspects )
		  || transition->aspects != availableAspects
		  || transition->baseMipLevel != 0 || transition->mipLevelCount != texture->mipLevels
		  || transition->baseArrayLayer != 0 || transition->arrayLayerCount != texture->arrayLayers
		  || !ralVk_StateEqual( &texture->portableState, &transition->before )
		  || ( transition->before.usage != RAL_RESOURCE_USAGE_UNDEFINED
		    && texture->portableOwnerQueue != transition->sourceQueue )
		  || cb->queue != transition->destinationQueue
		  || !ralVk_TranslateTextureResourceState( &transition->before, &before )
		  || !ralVk_TranslateTextureResourceState( &transition->after, &after )
		  || texture->currentLayout != before.layout )
			return ralErrorInvalidArgument;
		if ( cb->backend->queueFamily[transition->sourceQueue]
		  != cb->backend->queueFamily[transition->destinationQueue] )
			return ralUnsupported;
		for ( j = 0; j < i; ++j ) {
			if ( batch->textureTransitions[j].texture == texture
			  || batch->textureTransitions[j].texture->image == texture->image )
				return ralErrorInvalidArgument;
		}
		RAL_ZERO( imageBarriers[i] );
		imageBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarriers[i].srcAccessMask = before.access;
		imageBarriers[i].dstAccessMask = after.access;
		imageBarriers[i].oldLayout = before.layout;
		imageBarriers[i].newLayout = after.layout;
		imageBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarriers[i].image = texture->image;
		imageBarriers[i].subresourceRange.aspectMask = ralVk_NativeAspects( transition->aspects );
		imageBarriers[i].subresourceRange.baseMipLevel = 0;
		imageBarriers[i].subresourceRange.levelCount = texture->mipLevels;
		imageBarriers[i].subresourceRange.baseArrayLayer = 0;
		imageBarriers[i].subresourceRange.layerCount = texture->arrayLayers;
		srcStages |= before.stage;
		dstStages |= after.stage;
	}

	cb->backend->vk.CmdPipelineBarrier( cb->cb, srcStages, dstStages, 0,
		0, NULL, batch->bufferTransitionCount,
		batch->bufferTransitionCount ? bufferBarriers : NULL,
		batch->textureTransitionCount,
		batch->textureTransitionCount ? imageBarriers : NULL );

	for ( i = 0; i < batch->bufferTransitionCount; ++i ) {
		ralBuffer_t *buffer = batch->bufferTransitions[i].buffer;
		buffer->portableState = batch->bufferTransitions[i].after;
		buffer->portableOwnerQueue = batch->bufferTransitions[i].destinationQueue;
	}
	for ( i = 0; i < batch->textureTransitionCount; ++i ) {
		ralTexture_t *texture = batch->textureTransitions[i].texture;
		ralVkResourceStateTranslation_t after;
		(void)ralVk_TranslateTextureResourceState( &batch->textureTransitions[i].after, &after );
		texture->portableState = batch->textureTransitions[i].after;
		texture->portableOwnerQueue = batch->textureTransitions[i].destinationQueue;
		texture->currentLayout = after.layout;
	}
	return ralSuccess;
}

static qboolean ralVk_BufferTransferValid( const ralCommandBuffer_t *cb,
		const ralBufferTransition_t *transition, qboolean release ) {
	const ralBuffer_t *buffer = transition ? transition->buffer : NULL;
	if ( !cb || !transition || !buffer || !cb->backend
			|| ( !cb->externalLifecycle && cb->state != RAL_VK_CMD_RECORDING )
			|| cb->renderingActive || !cb->backend->vk.CmdPipelineBarrier
			|| buffer->backend != cb->backend || buffer->buffer == VK_NULL_HANDLE
			|| !buffer->portableStateKnown || buffer->legacyMapped
			|| !Ral_BufferMapLifecycleGpuUseAllowed( &buffer->mapLifecycle )
			|| !Ral_BufferTransitionValid( transition, (uint64_t)buffer->size )
			|| transition->offset != 0 || transition->size != (uint64_t)buffer->size
			|| transition->sourceQueue == transition->destinationQueue
			|| cb->backend->queueFamily[transition->sourceQueue]
				== cb->backend->queueFamily[transition->destinationQueue]
			|| !ralVk_StateEqual( &buffer->portableState, &transition->before )
			|| buffer->portableOwnerQueue != transition->sourceQueue
			|| cb->queue != ( release ? transition->sourceQueue
				: transition->destinationQueue ) ) return qfalse;
	return qtrue;
}

static qboolean ralVk_TextureTransferValid( const ralCommandBuffer_t *cb,
		const ralTextureTransition_t *transition, qboolean release ) {
	const ralTexture_t *texture = transition ? transition->texture : NULL;
	if ( !cb || !transition || !texture || !cb->backend
			|| ( !cb->externalLifecycle && cb->state != RAL_VK_CMD_RECORDING )
			|| cb->renderingActive || !cb->backend->vk.CmdPipelineBarrier
			|| texture->backend != cb->backend || texture->image == VK_NULL_HANDLE
			|| !texture->portableStateKnown
			|| !Ral_TextureTransitionValid( transition, texture->mipLevels,
				texture->arrayLayers, ralVk_PortableAspects( texture->aspect ) )
			|| transition->aspects != ralVk_PortableAspects( texture->aspect )
			|| transition->baseMipLevel != 0
			|| transition->mipLevelCount != texture->mipLevels
			|| transition->baseArrayLayer != 0
			|| transition->arrayLayerCount != texture->arrayLayers
			|| transition->sourceQueue == transition->destinationQueue
			|| cb->backend->queueFamily[transition->sourceQueue]
				== cb->backend->queueFamily[transition->destinationQueue]
			|| !ralVk_StateEqual( &texture->portableState, &transition->before )
			|| texture->portableOwnerQueue != transition->sourceQueue
			|| cb->queue != ( release ? transition->sourceQueue
				: transition->destinationQueue ) ) return qfalse;
	return qtrue;
}

static qboolean ralVk_ReceiptMatchesTransition(
		const ralQueueTransferReceipt_t *receipt, const void *identity,
		ralQueueTransferResourceType_t type, const ralResourceState_t *before,
		const ralResourceState_t *after, ralQueueType_t source,
		ralQueueType_t destination ) {
	ralQueueTransferReceipt_t expected;
	if ( !receipt ) return qfalse;
	expected = *receipt; expected.resourceIdentity = identity;
	expected.resourceType = type; expected.before = *before; expected.after = *after;
	expected.sourceQueue = source; expected.destinationQueue = destination;
	return Ral_QueueTransferReceiptExact( receipt, &expected );
}

ralResult_t Ral_CmdReleaseBufferOwnership( ralCommandBuffer_t *cb,
		const ralBufferTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt ) {
	ralBuffer_t *buffer = transition ? transition->buffer : NULL;
	ralVkResourceStateTranslation_t before;
	VkBufferMemoryBarrier barrier;
	if ( !outReceipt || !ralVk_BufferTransferValid( cb, transition, qtrue )
			|| buffer->queueTransfer.pending.ready
			|| !ralVk_TranslateBufferResourceState( &transition->before, &before ) )
		return ralErrorInvalidArgument;
	if ( Ral_QueueTransferLifecycleRelease( &buffer->queueTransfer, buffer,
			RAL_QUEUE_TRANSFER_RESOURCE_BUFFER, &transition->before,
			&transition->after, transition->sourceQueue,
			transition->destinationQueue, outReceipt ) != ralSuccess )
		return ralErrorInvalidArgument;
	RAL_ZERO( barrier ); barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = before.access; barrier.dstAccessMask = 0;
	barrier.srcQueueFamilyIndex = cb->backend->queueFamily[transition->sourceQueue];
	barrier.dstQueueFamilyIndex = cb->backend->queueFamily[transition->destinationQueue];
	barrier.buffer = buffer->buffer; barrier.size = buffer->size;
	cb->backend->vk.CmdPipelineBarrier( cb->cb, before.stage,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 1, &barrier, 0, NULL );
	return ralSuccess;
}

ralResult_t Ral_CmdAcquireBufferOwnership( ralCommandBuffer_t *cb,
		const ralBufferTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt ) {
	ralBuffer_t *buffer = transition ? transition->buffer : NULL;
	ralVkResourceStateTranslation_t after;
	VkBufferMemoryBarrier barrier;
	if ( !ralVk_BufferTransferValid( cb, transition, qfalse )
			|| !ralVk_ReceiptMatchesTransition( receipt, buffer,
				RAL_QUEUE_TRANSFER_RESOURCE_BUFFER, &transition->before,
				&transition->after, transition->sourceQueue,
				transition->destinationQueue )
			|| !Ral_QueueTransferReceiptExact( &buffer->queueTransfer.pending, receipt )
			|| !ralVk_TranslateBufferResourceState( &transition->after, &after ) )
		return ralErrorInvalidArgument;
	RAL_ZERO( barrier ); barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = 0; barrier.dstAccessMask = after.access;
	barrier.srcQueueFamilyIndex = cb->backend->queueFamily[transition->sourceQueue];
	barrier.dstQueueFamilyIndex = cb->backend->queueFamily[transition->destinationQueue];
	barrier.buffer = buffer->buffer; barrier.size = buffer->size;
	cb->backend->vk.CmdPipelineBarrier( cb->cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		after.stage, 0, 0, NULL, 1, &barrier, 0, NULL );
	(void)Ral_QueueTransferLifecycleAcquire( &buffer->queueTransfer, receipt );
	buffer->portableState = transition->after;
	buffer->portableOwnerQueue = transition->destinationQueue;
	return ralSuccess;
}

ralResult_t Ral_CancelBufferOwnershipTransfer( ralBuffer_t *buffer,
		const ralQueueTransferReceipt_t *receipt ) {
	if ( !buffer || !receipt || receipt->resourceIdentity != buffer
			|| receipt->resourceType != RAL_QUEUE_TRANSFER_RESOURCE_BUFFER )
		return ralErrorInvalidArgument;
	return Ral_QueueTransferLifecycleCancel( &buffer->queueTransfer, receipt );
}

ralResult_t Ral_CmdReleaseTextureOwnership( ralCommandBuffer_t *cb,
		const ralTextureTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt ) {
	ralTexture_t *texture = transition ? transition->texture : NULL;
	ralVkResourceStateTranslation_t before, after;
	VkImageMemoryBarrier barrier;
	if ( !outReceipt || !ralVk_TextureTransferValid( cb, transition, qtrue )
			|| texture->queueTransfer.pending.ready
			|| texture->currentLayout == VK_IMAGE_LAYOUT_UNDEFINED
			|| !ralVk_TranslateTextureResourceState( &transition->before, &before )
			|| !ralVk_TranslateTextureResourceState( &transition->after, &after )
			|| texture->currentLayout != before.layout ) return ralErrorInvalidArgument;
	if ( Ral_QueueTransferLifecycleRelease( &texture->queueTransfer, texture,
			RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE, &transition->before,
			&transition->after, transition->sourceQueue,
			transition->destinationQueue, outReceipt ) != ralSuccess )
		return ralErrorInvalidArgument;
	RAL_ZERO( barrier ); barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = before.access; barrier.oldLayout = before.layout;
	barrier.newLayout = after.layout;
	barrier.srcQueueFamilyIndex = cb->backend->queueFamily[transition->sourceQueue];
	barrier.dstQueueFamilyIndex = cb->backend->queueFamily[transition->destinationQueue];
	barrier.image = texture->image;
	barrier.subresourceRange.aspectMask = ralVk_NativeAspects( transition->aspects );
	barrier.subresourceRange.levelCount = texture->mipLevels;
	barrier.subresourceRange.layerCount = texture->arrayLayers;
	cb->backend->vk.CmdPipelineBarrier( cb->cb, before.stage,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );
	return ralSuccess;
}

ralResult_t Ral_CmdAcquireTextureOwnership( ralCommandBuffer_t *cb,
		const ralTextureTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt ) {
	ralTexture_t *texture = transition ? transition->texture : NULL;
	ralVkResourceStateTranslation_t before, after;
	VkImageMemoryBarrier barrier;
	if ( !ralVk_TextureTransferValid( cb, transition, qfalse )
			|| !ralVk_ReceiptMatchesTransition( receipt, texture,
				RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE, &transition->before,
				&transition->after, transition->sourceQueue,
				transition->destinationQueue )
			|| !Ral_QueueTransferReceiptExact( &texture->queueTransfer.pending, receipt )
			|| !ralVk_TranslateTextureResourceState( &transition->before, &before )
			|| !ralVk_TranslateTextureResourceState( &transition->after, &after )
			|| texture->currentLayout != before.layout ) return ralErrorInvalidArgument;
	RAL_ZERO( barrier ); barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.dstAccessMask = after.access; barrier.oldLayout = before.layout;
	barrier.newLayout = after.layout;
	barrier.srcQueueFamilyIndex = cb->backend->queueFamily[transition->sourceQueue];
	barrier.dstQueueFamilyIndex = cb->backend->queueFamily[transition->destinationQueue];
	barrier.image = texture->image;
	barrier.subresourceRange.aspectMask = ralVk_NativeAspects( transition->aspects );
	barrier.subresourceRange.levelCount = texture->mipLevels;
	barrier.subresourceRange.layerCount = texture->arrayLayers;
	cb->backend->vk.CmdPipelineBarrier( cb->cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		after.stage, 0, 0, NULL, 0, NULL, 1, &barrier );
	(void)Ral_QueueTransferLifecycleAcquire( &texture->queueTransfer, receipt );
	texture->portableState = transition->after;
	texture->portableOwnerQueue = transition->destinationQueue;
	texture->currentLayout = after.layout;
	return ralSuccess;
}

ralResult_t Ral_CancelTextureOwnershipTransfer( ralTexture_t *texture,
		const ralQueueTransferReceipt_t *receipt ) {
	if ( !texture || !receipt || receipt->resourceIdentity != texture
			|| receipt->resourceType != RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE )
		return ralErrorInvalidArgument;
	return Ral_QueueTransferLifecycleCancel( &texture->queueTransfer, receipt );
}
