// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

static ralResourceUsage_t ralVk_MapUsage( ralBufferMapMode_t mode ) {
	return mode == RAL_MAP_READ
		? RAL_RESOURCE_USAGE_HOST_READ : RAL_RESOURCE_USAGE_HOST_WRITE;
}

ralResult_t Ral_BufferMapBegin( ralBuffer_t *buf,
	                            const ralBufferMapRequest_t *request,
	                            ralBufferMapTicket_t *outTicket ) {
	void *base;
	void *mappedRange;
	ralResourceUsage_t usage;
	ralResult_t result;
	if ( !buf || !request || !outTicket || !buf->backend || !buf->alloc
	  || !buf->hostVisible || buf->immediateMapped
	  || buf->legacyMapped || buf->alloc->mapped
	  || !Ral_BufferMapLifecycleGpuUseAllowed( &buf->mapLifecycle )
	  || buf->mapLifecycle.generation >= UINT64_MAX - 1u
	  || !Ral_BufferMapRequestValid( request, (uint64_t)buf->size )
	  || !buf->portableStateKnown )
		return ralErrorInvalidArgument;
	if ( ( request->mode == RAL_MAP_READ && !( buf->usage & RAL_BUFFER_MAP_READ ) )
	  || ( request->mode == RAL_MAP_WRITE && !( buf->usage & RAL_BUFFER_MAP_WRITE ) ) )
		return ralErrorInvalidArgument;
	usage = ralVk_MapUsage( request->mode );
	if ( buf->portableState.usage != RAL_RESOURCE_USAGE_UNDEFINED
	  && ( buf->portableState.usage != usage || buf->portableState.shaderStages != 0 ) )
		return ralErrorInvalidArgument;
	base = ralVk_Map( buf->alloc );
	if ( !base ) return ralErrorUnknown;
	if ( request->mode == RAL_MAP_READ )
		ralVk_Invalidate( buf->alloc, (VkDeviceSize)request->offset, (VkDeviceSize)request->size );
	mappedRange = (void *)( (unsigned char *)base + request->offset );
	result = Ral_BufferMapLifecyclePublishBegin( &buf->mapLifecycle, buf,
		(uint64_t)buf->size, request, qtrue, mappedRange, outTicket );
	if ( result != ralSuccess ) {
		ralVk_Unmap( buf->alloc );
		return result;
	}
	buf->portableState.usage = usage;
	buf->portableState.shaderStages = 0;
	buf->backend->gpuExcludedBufferMapCount++;
	return ralSuccess;
}

ralResult_t Ral_BufferMapPoll( ralBuffer_t *buf,
	                           const ralBufferMapTicket_t *authority,
	                           ralBufferMapTicket_t *outTicket ) {
	if ( !buf || !authority || authority->bufferIdentity != buf )
		return ralErrorInvalidArgument;
	return Ral_BufferMapLifecyclePoll( &buf->mapLifecycle, authority, outTicket );
}

ralResult_t Ral_BufferMapUnmap( ralBuffer_t *buf,
	                            const ralBufferMapTicket_t *readyTicket ) {
	ralBufferMapTicket_t current;
	ralResult_t result;
	if ( !buf || !readyTicket || readyTicket->bufferIdentity != buf
	  || Ral_BufferMapLifecyclePoll( &buf->mapLifecycle, readyTicket, &current ) != ralSuccess
	  || !Ral_BufferMapTicketExact( &current, readyTicket )
	  || readyTicket->status != RAL_BUFFER_MAP_READY )
		return ralErrorInvalidArgument;
	if ( readyTicket->request.mode == RAL_MAP_WRITE )
		ralVk_Flush( buf->alloc, (VkDeviceSize)readyTicket->request.offset,
			(VkDeviceSize)readyTicket->request.size );
	ralVk_Unmap( buf->alloc );
	result = Ral_BufferMapLifecycleUnmap( &buf->mapLifecycle, readyTicket );
	if ( result == ralSuccess && buf->backend->gpuExcludedBufferMapCount > 0u )
		buf->backend->gpuExcludedBufferMapCount--;
	return result;
}

ralResult_t Ral_BufferMapCancel( ralBuffer_t *buf,
	                             const ralBufferMapTicket_t *pendingTicket ) {
	if ( !buf || !pendingTicket || pendingTicket->bufferIdentity != buf )
		return ralErrorInvalidArgument;
	// Vulkan publishes READY synchronously, so it never owns a cancellable
	// pending native map. WebGPU lowering consumes this shared Cancel contract.
	return Ral_BufferMapLifecycleCancel( &buf->mapLifecycle, pendingTicket );
}
