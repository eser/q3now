// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_transition.h"

#include <limits.h>
#include <string.h>

static qboolean Ral_QueueTypeValid( ralQueueType_t queue ) {
	return queue == RAL_QUEUE_GRAPHICS
	    || queue == RAL_QUEUE_COMPUTE
	    || queue == RAL_QUEUE_TRANSFER;
}

static qboolean Ral_ShaderStagesValid( uint32_t stages ) {
	return stages != 0 && ( stages & ~RAL_STAGE_ALL ) == 0;
}

static qboolean Ral_StateHasShaderStages( ralResourceUsage_t usage ) {
	return usage == RAL_RESOURCE_USAGE_UNIFORM_BUFFER
	    || usage == RAL_RESOURCE_USAGE_SAMPLED_TEXTURE
	    || usage == RAL_RESOURCE_USAGE_STORAGE_READ
	    || usage == RAL_RESOURCE_USAGE_STORAGE_WRITE
	    || usage == RAL_RESOURCE_USAGE_STORAGE_READ_WRITE;
}

static qboolean Ral_StateShapeValid( const ralResourceState_t *state ) {
	if ( !state || (uint32_t)state->usage >= RAL_RESOURCE_USAGE_COUNT )
		return qfalse;
	if ( Ral_StateHasShaderStages( state->usage ) )
		return Ral_ShaderStagesValid( state->shaderStages );
	return state->shaderStages == 0;
}

qboolean Ral_ResourceStateValidForBuffer( const ralResourceState_t *state ) {
	if ( !Ral_StateShapeValid( state ) )
		return qfalse;
	switch ( state->usage ) {
	case RAL_RESOURCE_USAGE_UNDEFINED:
	case RAL_RESOURCE_USAGE_COPY_SOURCE:
	case RAL_RESOURCE_USAGE_COPY_DESTINATION:
	case RAL_RESOURCE_USAGE_VERTEX_BUFFER:
	case RAL_RESOURCE_USAGE_INDEX_BUFFER:
	case RAL_RESOURCE_USAGE_INDIRECT_BUFFER:
	case RAL_RESOURCE_USAGE_UNIFORM_BUFFER:
	case RAL_RESOURCE_USAGE_STORAGE_READ:
	case RAL_RESOURCE_USAGE_STORAGE_WRITE:
	case RAL_RESOURCE_USAGE_STORAGE_READ_WRITE:
	case RAL_RESOURCE_USAGE_HOST_READ:
	case RAL_RESOURCE_USAGE_HOST_WRITE:
		return qtrue;
	default:
		return qfalse;
	}
}

qboolean Ral_ResourceStateValidForTexture( const ralResourceState_t *state ) {
	if ( !Ral_StateShapeValid( state ) )
		return qfalse;
	switch ( state->usage ) {
	case RAL_RESOURCE_USAGE_UNDEFINED:
	case RAL_RESOURCE_USAGE_COPY_SOURCE:
	case RAL_RESOURCE_USAGE_COPY_DESTINATION:
	case RAL_RESOURCE_USAGE_SAMPLED_TEXTURE:
	case RAL_RESOURCE_USAGE_STORAGE_READ:
	case RAL_RESOURCE_USAGE_STORAGE_WRITE:
	case RAL_RESOURCE_USAGE_STORAGE_READ_WRITE:
	case RAL_RESOURCE_USAGE_COLOR_ATTACHMENT:
	case RAL_RESOURCE_USAGE_DEPTH_STENCIL_READ:
	case RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE:
	case RAL_RESOURCE_USAGE_PRESENT:
		return qtrue;
	default:
		return qfalse;
	}
}

static qboolean Ral_StateEqual( const ralResourceState_t *a,
	                            const ralResourceState_t *b ) {
	return a->usage == b->usage && a->shaderStages == b->shaderStages;
}

qboolean Ral_BufferTransitionValid( const ralBufferTransition_t *transition,
	                                uint64_t bufferSize ) {
	if ( !transition || !transition->buffer || bufferSize == 0
	  || !Ral_ResourceStateValidForBuffer( &transition->before )
	  || !Ral_ResourceStateValidForBuffer( &transition->after )
	  || transition->after.usage == RAL_RESOURCE_USAGE_UNDEFINED
	  || !Ral_QueueTypeValid( transition->sourceQueue )
	  || !Ral_QueueTypeValid( transition->destinationQueue )
	  || transition->size == 0 || transition->offset >= bufferSize
	  || transition->size > bufferSize - transition->offset )
		return qfalse;
	if ( Ral_StateEqual( &transition->before, &transition->after )
	  && transition->sourceQueue == transition->destinationQueue )
		return qfalse;
	return qtrue;
}

static qboolean Ral_TextureUsageValidForAspects( ralResourceUsage_t usage,
	                                             ralTextureAspectFlags_t aspects ) {
	if ( usage == RAL_RESOURCE_USAGE_COLOR_ATTACHMENT
	  || usage == RAL_RESOURCE_USAGE_PRESENT
	  || usage == RAL_RESOURCE_USAGE_STORAGE_READ
	  || usage == RAL_RESOURCE_USAGE_STORAGE_WRITE
	  || usage == RAL_RESOURCE_USAGE_STORAGE_READ_WRITE )
		return aspects == RAL_TEXTURE_ASPECT_COLOR;
	if ( usage == RAL_RESOURCE_USAGE_DEPTH_STENCIL_READ
	  || usage == RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE )
		return ( aspects & RAL_TEXTURE_ASPECT_DEPTH ) != 0
		    && ( aspects & RAL_TEXTURE_ASPECT_COLOR ) == 0;
	return qtrue;
}

qboolean Ral_TextureTransitionValid( const ralTextureTransition_t *transition,
	                                 uint32_t mipLevels,
	                                 uint32_t arrayLayers,
	                                 ralTextureAspectFlags_t availableAspects ) {
	const ralTextureAspectFlags_t allAspects = RAL_TEXTURE_ASPECT_COLOR
		| RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL;
	if ( !transition || !transition->texture || mipLevels == 0 || arrayLayers == 0
	  || !Ral_ResourceStateValidForTexture( &transition->before )
	  || !Ral_ResourceStateValidForTexture( &transition->after )
	  || transition->after.usage == RAL_RESOURCE_USAGE_UNDEFINED
	  || !Ral_QueueTypeValid( transition->sourceQueue )
	  || !Ral_QueueTypeValid( transition->destinationQueue )
	  || transition->aspects == 0 || ( transition->aspects & ~allAspects ) != 0
	  || availableAspects == 0 || ( availableAspects & ~allAspects ) != 0
	  || ( transition->aspects & ~availableAspects ) != 0
	  || transition->mipLevelCount == 0 || transition->baseMipLevel >= mipLevels
	  || transition->mipLevelCount > mipLevels - transition->baseMipLevel
	  || transition->arrayLayerCount == 0 || transition->baseArrayLayer >= arrayLayers
	  || transition->arrayLayerCount > arrayLayers - transition->baseArrayLayer
	  || !Ral_TextureUsageValidForAspects( transition->before.usage, transition->aspects )
	  || !Ral_TextureUsageValidForAspects( transition->after.usage, transition->aspects ) )
		return qfalse;
	if ( Ral_StateEqual( &transition->before, &transition->after )
	  && transition->sourceQueue == transition->destinationQueue )
		return qfalse;
	return qtrue;
}

static qboolean Ral_QueueTransferReceiptValid(
		const ralQueueTransferReceipt_t *receipt ) {
	if ( !receipt || receipt->ready != qtrue || !receipt->resourceIdentity
			|| !receipt->generation || receipt->generation == UINT64_MAX
			|| ( receipt->resourceType != RAL_QUEUE_TRANSFER_RESOURCE_BUFFER
				&& receipt->resourceType != RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE )
			|| !Ral_QueueTypeValid( receipt->sourceQueue )
			|| !Ral_QueueTypeValid( receipt->destinationQueue )
			|| receipt->sourceQueue == receipt->destinationQueue ) return qfalse;
	return receipt->resourceType == RAL_QUEUE_TRANSFER_RESOURCE_BUFFER
		? ( Ral_ResourceStateValidForBuffer( &receipt->before )
			&& Ral_ResourceStateValidForBuffer( &receipt->after )
			&& receipt->after.usage != RAL_RESOURCE_USAGE_UNDEFINED )
		: ( Ral_ResourceStateValidForTexture( &receipt->before )
			&& Ral_ResourceStateValidForTexture( &receipt->after )
			&& receipt->after.usage != RAL_RESOURCE_USAGE_UNDEFINED );
}

void Ral_QueueTransferLifecycleInit( ralQueueTransferLifecycle_t *lifecycle ) {
	if ( lifecycle ) memset( lifecycle, 0, sizeof( *lifecycle ) );
}

qboolean Ral_QueueTransferReceiptExact( const ralQueueTransferReceipt_t *a,
		const ralQueueTransferReceipt_t *b ) {
	return Ral_QueueTransferReceiptValid( a )
		&& Ral_QueueTransferReceiptValid( b )
		&& a->resourceIdentity == b->resourceIdentity
		&& a->generation == b->generation && a->resourceType == b->resourceType
		&& Ral_StateEqual( &a->before, &b->before )
		&& Ral_StateEqual( &a->after, &b->after )
		&& a->sourceQueue == b->sourceQueue
		&& a->destinationQueue == b->destinationQueue && a->ready == b->ready
		? qtrue : qfalse;
}

ralResult_t Ral_QueueTransferLifecycleRelease(
		ralQueueTransferLifecycle_t *lifecycle, const void *resourceIdentity,
		ralQueueTransferResourceType_t resourceType,
		const ralResourceState_t *before, const ralResourceState_t *after,
		ralQueueType_t sourceQueue, ralQueueType_t destinationQueue,
		ralQueueTransferReceipt_t *outReceipt ) {
	ralQueueTransferReceipt_t candidate;
	ralQueueTransferReceipt_t empty;
	memset( &empty, 0, sizeof( empty ) );
	if ( !lifecycle || !outReceipt
			|| memcmp( &lifecycle->pending, &empty, sizeof( empty ) ) != 0
			|| lifecycle->nextGeneration >= UINT64_MAX - 1u )
		return ralErrorInvalidArgument;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.resourceIdentity = resourceIdentity;
	candidate.generation = lifecycle->nextGeneration + 1u;
	candidate.resourceType = resourceType;
	if ( before ) candidate.before = *before;
	if ( after ) candidate.after = *after;
	candidate.sourceQueue = sourceQueue;
	candidate.destinationQueue = destinationQueue;
	candidate.ready = qtrue;
	if ( !Ral_QueueTransferReceiptValid( &candidate ) )
		return ralErrorInvalidArgument;
	lifecycle->nextGeneration = candidate.generation;
	lifecycle->pending = candidate;
	*outReceipt = candidate;
	return ralSuccess;
}

static ralResult_t Ral_QueueTransferLifecycleFinish(
		ralQueueTransferLifecycle_t *lifecycle,
		const ralQueueTransferReceipt_t *receipt ) {
	if ( !lifecycle || !receipt
			|| !Ral_QueueTransferReceiptExact( &lifecycle->pending, receipt ) )
		return ralErrorInvalidArgument;
	memset( &lifecycle->pending, 0, sizeof( lifecycle->pending ) );
	return ralSuccess;
}

ralResult_t Ral_QueueTransferLifecycleAcquire(
		ralQueueTransferLifecycle_t *lifecycle,
		const ralQueueTransferReceipt_t *receipt ) {
	return Ral_QueueTransferLifecycleFinish( lifecycle, receipt );
}

ralResult_t Ral_QueueTransferLifecycleCancel(
		ralQueueTransferLifecycle_t *lifecycle,
		const ralQueueTransferReceipt_t *receipt ) {
	return Ral_QueueTransferLifecycleFinish( lifecycle, receipt );
}
