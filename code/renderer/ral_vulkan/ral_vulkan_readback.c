// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

typedef struct {
	ralBackend_t *backend;
	ralTransferResourceKind_t kind;
	union { ralBuffer_t *buffer; ralTexture_t *texture; } source;
	ralTextureReadbackRegion_t textureRegion;
	uint64_t bufferOffset;
	uint64_t bytes;
	uint64_t generation;
	ralResourceState_t sourceState;
	ralQueueType_t sourceQueue;
	ralCommandBuffer_t *command;
	ralFence_t *fence;
	ralBuffer_t *staging;
	qboolean submitted;
} ralVkReadbackContext_t;

static ralTextureAspectFlags_t ReadbackAspects( VkImageAspectFlags aspects ) {
	ralTextureAspectFlags_t out = 0;
	if ( aspects & VK_IMAGE_ASPECT_COLOR_BIT ) out |= RAL_TEXTURE_ASPECT_COLOR;
	if ( aspects & VK_IMAGE_ASPECT_DEPTH_BIT ) out |= RAL_TEXTURE_ASPECT_DEPTH;
	if ( aspects & VK_IMAGE_ASPECT_STENCIL_BIT ) out |= RAL_TEXTURE_ASPECT_STENCIL;
	return out;
}

static void RestorePortableState( ralVkReadbackContext_t *context ) {
	if ( context->kind == RAL_TRANSFER_BUFFER ) {
		context->source.buffer->portableState = context->sourceState;
		context->source.buffer->portableOwnerQueue = context->sourceQueue;
		context->source.buffer->portableStateKnown = qtrue;
	} else {
		ralVkResourceStateTranslation_t native;
		context->source.texture->portableState = context->sourceState;
		context->source.texture->portableOwnerQueue = context->sourceQueue;
		context->source.texture->portableStateKnown = qtrue;
		if ( ralVk_TranslateTextureResourceState( &context->sourceState, &native ) )
			context->source.texture->currentLayout = native.layout;
	}
}

static void CancelCommand( ralVkReadbackContext_t *context ) {
	ralCommandReceipt_t receipt;
	if ( !context->command ) return;
	if ( Ral_GetCommandBufferReceipt( context->command, &receipt ) == ralSuccess
			&& ( receipt.state == RAL_COMMAND_RECORDING
				|| receipt.state == RAL_COMMAND_EXECUTABLE ) )
		(void)Ral_CancelCommandBuffer( context->command, &receipt );
	Ral_DestroyCommandBuffer( context->command );
	context->command = NULL;
	RestorePortableState( context );
}

static qboolean CreateStaging( void *opaque, uint64_t bytes,
		ralBuffer_t **outBuffer, ralAllocationReceipt_t *outAllocation ) {
	ralVkReadbackContext_t *context = (ralVkReadbackContext_t *)opaque;
	ralBufferCreateInfo_t createInfo;
	ralBuffer_t *candidate;
	if ( !context || !outBuffer || !outAllocation || bytes != context->bytes ) return qfalse;
	RAL_ZERO( createInfo );
	createInfo.size = bytes;
	createInfo.usage = RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ;
	createInfo.memory = RAL_MEMORY_HOST_VISIBLE;
	createInfo.debugName = "ral-readback-staging";
	candidate = Ral_CreateBuffer( context->backend, &createInfo );
	if ( !candidate ) return qfalse;
	if ( !Ral_BufferGetAllocationReceipt( candidate, outAllocation ) ) {
		Ral_DestroyBuffer( candidate );
		return qfalse;
	}
	context->staging = candidate;
	*outBuffer = candidate;
	return qtrue;
}

static qboolean TransitionForCopy( ralVkReadbackContext_t *context,
		ralBuffer_t *staging, qboolean restore ) {
	ralBufferTransition_t buffers[2];
	ralTextureTransition_t texture;
	ralResourceTransitionBatch_t batch;
	ralResourceState_t copySource = { RAL_RESOURCE_USAGE_COPY_SOURCE, 0 };
	ralResourceState_t copyDestination = { RAL_RESOURCE_USAGE_COPY_DESTINATION, 0 };
	ralResourceState_t hostRead = { RAL_RESOURCE_USAGE_HOST_READ, 0 };
	RAL_ZERO( buffers ); RAL_ZERO( texture ); RAL_ZERO( batch );
	if ( context->kind == RAL_TRANSFER_BUFFER ) {
		buffers[0].buffer = context->source.buffer;
		buffers[0].size = (uint64_t)context->source.buffer->size;
		buffers[0].before = restore ? copySource : context->sourceState;
		buffers[0].after = restore ? context->sourceState : copySource;
		buffers[0].sourceQueue = RAL_QUEUE_GRAPHICS;
		buffers[0].destinationQueue = RAL_QUEUE_GRAPHICS;
		batch.bufferTransitions = buffers;
		batch.bufferTransitionCount = 1;
	} else {
		texture.texture = context->source.texture;
		texture.aspects = ReadbackAspects( context->source.texture->aspect );
		texture.mipLevelCount = context->source.texture->mipLevels;
		texture.arrayLayerCount = context->source.texture->arrayLayers;
		texture.before = restore ? copySource : context->sourceState;
		texture.after = restore ? context->sourceState : copySource;
		texture.sourceQueue = RAL_QUEUE_GRAPHICS;
		texture.destinationQueue = RAL_QUEUE_GRAPHICS;
		batch.textureTransitions = &texture;
		batch.textureTransitionCount = 1;
	}
	if ( Ral_CmdTransitionResources( context->command, &batch ) != ralSuccess ) return qfalse;
	RAL_ZERO( batch ); RAL_ZERO( buffers[1] );
	buffers[1].buffer = staging;
	buffers[1].size = (uint64_t)staging->size;
	buffers[1].before = restore ? copyDestination
		: (ralResourceState_t){ RAL_RESOURCE_USAGE_UNDEFINED, 0 };
	buffers[1].after = restore ? hostRead : copyDestination;
	buffers[1].sourceQueue = RAL_QUEUE_GRAPHICS;
	buffers[1].destinationQueue = RAL_QUEUE_GRAPHICS;
	batch.bufferTransitions = &buffers[1];
	batch.bufferTransitionCount = 1;
	return Ral_CmdTransitionResources( context->command, &batch ) == ralSuccess ? qtrue : qfalse;
}

static qboolean SubmitCopy( void *opaque, const ralTransferRequest_t *request,
		ralBuffer_t *staging, ralTransferOutcome_t *outOutcome,
		uintptr_t *outSubmissionIdentity, uint64_t *outSubmissionGeneration ) {
	ralVkReadbackContext_t *context = (ralVkReadbackContext_t *)opaque;
	ralCommandReceipt_t recording, executable;
	ralSubmissionReceipt_t submission;
	ralSubmitInfo_t submitInfo;
	ralCommandBuffer_t *commands[1];
	if ( !context || !request || !staging || staging != context->staging
			|| !outOutcome || !outSubmissionIdentity || !outSubmissionGeneration
			|| request->resourceKind != context->kind || request->byteSize != context->bytes
			|| request->resourceGeneration == 0 || request->queue != RAL_QUEUE_GRAPHICS ) return qfalse;
	context->command = Ral_AcquireBegunCommandBuffer( context->backend, RAL_QUEUE_GRAPHICS );
	if ( !context->command
			|| Ral_GetCommandBufferReceipt( context->command, &recording ) != ralSuccess
			|| !TransitionForCopy( context, staging, qfalse ) ) goto fail;
	if ( context->kind == RAL_TRANSFER_BUFFER ) {
		ralBufferCopy_t copy = { context->bufferOffset, 0, context->bytes };
		Ral_CmdCopyBuffer( context->command, context->source.buffer, staging, &copy );
	} else {
		ralBufferTextureCopy_t copy;
		RAL_ZERO( copy );
		copy.mipLevel = context->textureRegion.mipLevel;
		copy.arrayLayer = context->textureRegion.arrayLayer;
		copy.imageRect.x = (int32_t)context->textureRegion.x;
		copy.imageRect.y = (int32_t)context->textureRegion.y;
		copy.imageRect.width = context->textureRegion.width;
		copy.imageRect.height = context->textureRegion.height;
		Ral_CmdCopyTextureToBuffer( context->command, context->source.texture, staging, &copy );
	}
	if ( !TransitionForCopy( context, staging, qtrue )
			|| Ral_EndCommandBufferExact( context->command, &recording, &executable ) != ralSuccess )
		goto fail;
	context->fence = Ral_CreateFence( context->backend );
	if ( !context->fence ) goto fail;
	RAL_ZERO( submitInfo ); RAL_ZERO( submission );
	commands[0] = context->command;
	submitInfo.commandBuffers = commands;
	submitInfo.numCommandBuffers = 1;
	submitInfo.signalFence = context->fence;
	if ( Ral_SubmitExact( context->backend, RAL_QUEUE_GRAPHICS, &submitInfo,
			&executable, &submission ) != ralSuccess ) goto fail;
	context->submitted = qtrue;
	*outOutcome = RAL_TRANSFER_OUTCOME_NATIVE_ASYNC;
	*outSubmissionIdentity = (uintptr_t)context->fence;
	*outSubmissionGeneration = context->generation;
	return qtrue;
fail:
	if ( context->fence ) { Ral_DestroyFence( context->fence ); context->fence = NULL; }
	CancelCommand( context );
	return qfalse;
}

static qboolean SubmissionCompleted( void *opaque, uintptr_t identity, qboolean *out ) {
	ralVkReadbackContext_t *context = (ralVkReadbackContext_t *)opaque;
	if ( !context || !out || !context->submitted || identity != (uintptr_t)context->fence )
		return qfalse;
	*out = Ral_FenceSignaled( context->fence );
	return qtrue;
}

static ralResult_t MapBegin( void *opaque, ralBuffer_t *staging,
		const ralBufferMapRequest_t *request, ralBufferMapTicket_t *out ) {
	(void)opaque; return Ral_BufferMapBegin( staging, request, out );
}
static ralResult_t MapPoll( void *opaque, ralBuffer_t *staging,
		const ralBufferMapTicket_t *authority, ralBufferMapTicket_t *out ) {
	(void)opaque; return Ral_BufferMapPoll( staging, authority, out );
}
static ralResult_t MapUnmap( void *opaque, ralBuffer_t *staging,
		const ralBufferMapTicket_t *ticket ) {
	(void)opaque; return Ral_BufferMapUnmap( staging, ticket );
}
static ralResult_t MapCancel( void *opaque, ralBuffer_t *staging,
		const ralBufferMapTicket_t *ticket ) {
	(void)opaque; return Ral_BufferMapCancel( staging, ticket );
}

static qboolean CandidateAllowed( void *opaque, ralReadbackOwnedRole_t role,
		uintptr_t identity ) {
	ralVkReadbackContext_t *context = (ralVkReadbackContext_t *)opaque;
	if ( !context || !identity ) return qfalse;
	if ( role == RAL_READBACK_ROLE_STAGING ) {
		if ( identity != (uintptr_t)context->staging ) return qfalse;
		return context->kind != RAL_TRANSFER_BUFFER
			|| identity != (uintptr_t)context->source.buffer;
	}
	return role == RAL_READBACK_ROLE_SUBMISSION
		&& identity == (uintptr_t)context->fence;
}

static void RetireStaging( void *opaque, ralBuffer_t *staging ) {
	ralVkReadbackContext_t *context = (ralVkReadbackContext_t *)opaque;
	if ( context && staging == context->staging ) {
		Ral_DestroyBuffer( staging );
		context->staging = NULL;
	}
}

static void RetireSubmission( void *opaque, uintptr_t identity ) {
	ralVkReadbackContext_t *context = (ralVkReadbackContext_t *)opaque;
	if ( !context || identity != (uintptr_t)context->fence ) return;
	if ( context->submitted && !Ral_FenceSignaled( context->fence ) )
		Ral_WaitFence( context->fence, RAL_TIMEOUT_INFINITE );
	if ( context->command ) { Ral_DestroyCommandBuffer( context->command ); context->command = NULL; }
	Ral_DestroyFence( context->fence );
	context->fence = NULL;
}

static void DestroyContext( void *opaque ) { free( opaque ); }

static const ralReadbackOps_t readbackOps = {
	CreateStaging, SubmitCopy, SubmissionCompleted, MapBegin, MapPoll,
	MapUnmap, MapCancel, CandidateAllowed, RetireStaging, RetireSubmission,
	DestroyContext
};

static qboolean Begin( ralVkReadbackContext_t *context,
		ralTransferRequest_t *request, ralReadbackOwner_t **outOwner ) {
	ralReadbackCreateInfo_t createInfo;
	ralBackend_t *backend = context->backend;
	if ( backend->nextTransferGeneration >= UINT64_MAX - 1u ) { free( context ); return qfalse; }
	context->generation = backend->nextTransferGeneration + 1u;
	RAL_ZERO( createInfo );
	createInfo.transfer = *request;
	createInfo.transferGeneration = context->generation;
	createInfo.context = context;
	createInfo.ops = &readbackOps;
	if ( !Ral_ReadbackCreate( &createInfo, outOwner ) ) return qfalse;
	backend->nextTransferGeneration = context->generation;
	return qtrue;
}

qboolean Ral_BufferReadbackBegin( ralBuffer_t *source, uint64_t offset,
		uint64_t size, ralReadbackOwner_t **outOwner ) {
	ralVkReadbackContext_t *context;
	ralAllocationReceipt_t allocation;
	ralTransferRequest_t request;
	if ( !source || !outOwner || *outOwner || !source->backend || !source->alloc
			|| !( source->usage & RAL_BUFFER_TRANSFER_SRC ) || !source->portableStateKnown
			|| source->portableState.usage == RAL_RESOURCE_USAGE_UNDEFINED
			|| source->portableOwnerQueue != RAL_QUEUE_GRAPHICS
			|| !ralVk_BufferGpuUseAllowed( source ) || size == 0
			|| offset > (uint64_t)source->size || size > (uint64_t)source->size - offset
			|| !Ral_BufferGetAllocationReceipt( source, &allocation ) ) return qfalse;
	context = (ralVkReadbackContext_t *)calloc( 1, sizeof( *context ) );
	if ( !context ) return qfalse;
	context->backend = source->backend;
	context->kind = RAL_TRANSFER_BUFFER;
	context->source.buffer = source;
	context->bufferOffset = offset;
	context->bytes = size;
	context->sourceState = source->portableState;
	context->sourceQueue = source->portableOwnerQueue;
	RAL_ZERO( request );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_READBACK;
	request.resourceKind = RAL_TRANSFER_BUFFER;
	request.resourceIdentity = (uintptr_t)source;
	request.resourceGeneration = allocation.allocationGeneration;
	request.byteOffset = offset;
	request.byteSize = size;
	request.byteBudget = allocation.committedSize;
	request.queue = RAL_QUEUE_GRAPHICS;
	return Begin( context, &request, outOwner );
}

qboolean Ral_TextureReadbackBegin( ralTexture_t *source,
		const ralTextureReadbackRegion_t *region, ralReadbackOwner_t **outOwner ) {
	ralVkReadbackContext_t *context;
	ralAllocationReceipt_t allocation;
	ralTransferRequest_t request;
	uint32_t mipWidth, mipHeight, bytesPerPixel;
	uint64_t requiredBytes;
	if ( !source || !region || !outOwner || *outOwner || !source->backend || !source->alloc
			|| !( source->usage & RAL_TEXTURE_USAGE_TRANSFER_SRC )
			|| !source->portableStateKnown
			|| source->portableState.usage == RAL_RESOURCE_USAGE_UNDEFINED
			|| source->portableOwnerQueue != RAL_QUEUE_GRAPHICS
			|| region->mipLevel >= source->mipLevels
			|| region->arrayLayer >= source->arrayLayers || region->byteSize == 0
			|| !Ral_TextureGetAllocationReceipt( source, &allocation ) ) return qfalse;
	mipWidth = source->width >> region->mipLevel; if ( mipWidth == 0 ) mipWidth = 1;
	mipHeight = source->height >> region->mipLevel; if ( mipHeight == 0 ) mipHeight = 1;
	bytesPerPixel = ralVk_FormatBPP( source->ralFormat );
	if ( region->width == 0 || region->height == 0 || region->x > mipWidth
			|| region->width > mipWidth - region->x || region->y > mipHeight
			|| region->height > mipHeight - region->y || bytesPerPixel == 0
			|| region->width > UINT64_MAX / bytesPerPixel
			|| (uint64_t)region->width * bytesPerPixel > UINT64_MAX / region->height ) return qfalse;
	requiredBytes = (uint64_t)region->width * region->height * bytesPerPixel;
	if ( region->byteSize != requiredBytes ) return qfalse;
	context = (ralVkReadbackContext_t *)calloc( 1, sizeof( *context ) );
	if ( !context ) return qfalse;
	context->backend = source->backend;
	context->kind = RAL_TRANSFER_TEXTURE;
	context->source.texture = source;
	context->textureRegion = *region;
	context->bytes = region->byteSize;
	context->sourceState = source->portableState;
	context->sourceQueue = source->portableOwnerQueue;
	RAL_ZERO( request );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_READBACK;
	request.resourceKind = RAL_TRANSFER_TEXTURE;
	request.resourceIdentity = (uintptr_t)source;
	request.resourceGeneration = allocation.allocationGeneration;
	request.byteSize = region->byteSize;
	request.byteBudget = allocation.committedSize;
	request.mipLevel = region->mipLevel;
	request.arrayLayer = region->arrayLayer;
	request.offsetX = region->x;
	request.offsetY = region->y;
	request.width = region->width;
	request.height = region->height;
	request.depth = 1;
	request.queue = RAL_QUEUE_GRAPHICS;
	return Begin( context, &request, outOwner );
}
