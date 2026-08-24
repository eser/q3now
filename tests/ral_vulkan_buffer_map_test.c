// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

static unsigned char storage[128];
static qboolean mapSucceeds;
static uint32_t mapCalls, unmapCalls, flushCalls, invalidateCalls;

void *ralVk_Map( ralVkAllocation_t *allocation ) {
	mapCalls++;
	if ( !allocation || !mapSucceeds ) return NULL;
	allocation->mapped = storage;
	return storage;
}

void ralVk_Unmap( ralVkAllocation_t *allocation ) {
	unmapCalls++;
	if ( allocation ) allocation->mapped = NULL;
}

void ralVk_Flush( ralVkAllocation_t *allocation, VkDeviceSize offset, VkDeviceSize size ) {
	(void)allocation; (void)offset; (void)size;
	flushCalls++;
}

void ralVk_Invalidate( ralVkAllocation_t *allocation, VkDeviceSize offset, VkDeviceSize size ) {
	(void)allocation; (void)offset; (void)size;
	invalidateCalls++;
}

static void SetupBuffer( ralBuffer_t *buffer, ralBackend_t *backend,
	                     ralVkAllocation_t *allocation ) {
	memset( buffer, 0, sizeof( *buffer ) );
	memset( backend, 0, sizeof( *backend ) );
	memset( allocation, 0, sizeof( *allocation ) );
	buffer->backend = backend;
	buffer->alloc = allocation;
	buffer->buffer = (VkBuffer)(uintptr_t)0x100u;
	buffer->size = sizeof( storage );
	buffer->hostVisible = qtrue;
	buffer->usage = RAL_BUFFER_MAP_READ | RAL_BUFFER_MAP_WRITE;
	buffer->portableStateKnown = qtrue;
	buffer->portableState.usage = RAL_RESOURCE_USAGE_UNDEFINED;
	buffer->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	Ral_BufferMapLifecycleInit( &buffer->mapLifecycle );
}

int main( void ) {
	ralBackend_t backend;
	ralVkAllocation_t allocation;
	ralBuffer_t buffer;
	ralBindGroup_t bindGroup;
	ralCommandBuffer_t commandBuffer;
	ralBufferMapRequest_t request = { RAL_MAP_WRITE, 16, 32 };
	ralBufferMapTicket_t ticket, current, stale, output, unchanged;

	SetupBuffer( &buffer, &backend, &allocation );
	mapSucceeds = qtrue;
	CHECK( ralVk_BufferGpuUseAllowed( &buffer ) );
	CHECK( Ral_BufferMapBegin( &buffer, &request, &ticket ) == ralSuccess );
	CHECK( backend.gpuExcludedBufferMapCount == 1u );
	CHECK( !ralVk_BufferGpuUseAllowed( &buffer ) );
	memset( &bindGroup, 0, sizeof( bindGroup ) );
	bindGroup.bufferTrackingComplete = qtrue;
	bindGroup.bufferCount = 1;
	bindGroup.buffers[0] = &buffer;
	memset( &commandBuffer, 0, sizeof( commandBuffer ) );
	commandBuffer.backend = &backend;
	commandBuffer.boundVertexBuffers[0] = &buffer;
	commandBuffer.boundIndexBuffer = &buffer;
	commandBuffer.boundBindGroups[0] = &bindGroup;
	CHECK( !ralVk_BindGroupBuffersGpuUseAllowed( &bindGroup ) );
	CHECK( !ralVk_CommandBoundBuffersGpuUseAllowed( &commandBuffer ) );
	CHECK( ticket.status == RAL_BUFFER_MAP_READY );
	CHECK( ticket.mappedRange == storage + 16 );
	CHECK( buffer.portableState.usage == RAL_RESOURCE_USAGE_HOST_WRITE );
	CHECK( mapCalls == 1 && invalidateCalls == 0 );
	CHECK( Ral_BufferMapPoll( &buffer, &ticket, &current ) == ralSuccess );
	CHECK( Ral_BufferMapTicketExact( &ticket, &current ) );
	memset( ticket.mappedRange, 0x4d, (size_t)ticket.request.size );
	CHECK( storage[15] == 0 && storage[16] == 0x4d && storage[47] == 0x4d && storage[48] == 0 );
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	CHECK( Ral_BufferMapCancel( &buffer, &ticket ) == ralErrorInvalidArgument );
	stale = ticket; stale.generation++;
	CHECK( Ral_BufferMapUnmap( &buffer, &stale ) == ralErrorInvalidArgument );
	CHECK( flushCalls == 0 && unmapCalls == 0 );
	CHECK( Ral_BufferMapUnmap( &buffer, &ticket ) == ralSuccess );
	CHECK( backend.gpuExcludedBufferMapCount == 0u );
	CHECK( flushCalls == 1 && unmapCalls == 1 );
	CHECK( Ral_BufferMapLifecycleGpuUseAllowed( &buffer.mapLifecycle ) );
	CHECK( ralVk_BufferGpuUseAllowed( &buffer ) );
	CHECK( ralVk_BindGroupBuffersGpuUseAllowed( &bindGroup ) );
	CHECK( ralVk_CommandBoundBuffersGpuUseAllowed( &commandBuffer ) );

	// READ makes noncoherent device writes visible before returning the pointer.
	request.mode = RAL_MAP_READ;
	buffer.portableState.usage = RAL_RESOURCE_USAGE_HOST_READ;
	storage[16] = 0x7a;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &ticket ) == ralSuccess );
	CHECK( invalidateCalls == 1 );
	CHECK( *(const unsigned char *)ticket.mappedRange == 0x7a );
	CHECK( Ral_BufferMapUnmap( &buffer, &ticket ) == ralSuccess );
	CHECK( flushCalls == 1 && unmapCalls == 2 );

	// Wrong semantic state, legacy/external mapping and map failure are atomic.
	buffer.portableState.usage = RAL_RESOURCE_USAGE_STORAGE_READ;
	buffer.portableState.shaderStages = RAL_STAGE_COMPUTE;
	mapCalls = 0;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	CHECK( mapCalls == 0 );
	buffer.portableState.usage = RAL_RESOURCE_USAGE_HOST_READ;
	buffer.portableState.shaderStages = 0;
	buffer.legacyMapped = qtrue;
	CHECK( !ralVk_BufferGpuUseAllowed( &buffer ) );
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	buffer.legacyMapped = qfalse;
	allocation.mapped = storage;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	allocation.mapped = NULL;
	mapSucceeds = qfalse;
	memset( &output, 0x6b, sizeof( output ) );
	unchanged = output;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorUnknown );
	CHECK( memcmp( &output, &unchanged, sizeof( output ) ) == 0 );
	mapSucceeds = qtrue;
	buffer.hostVisible = qfalse;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	buffer.hostVisible = qtrue;
	buffer.usage = 0;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	buffer.usage = RAL_BUFFER_MAP_READ | RAL_BUFFER_MAP_WRITE;
	buffer.mapLifecycle.generation = UINT64_MAX - 1u;
	CHECK( Ral_BufferMapBegin( &buffer, &request, &output ) == ralErrorInvalidArgument );
	CHECK( allocation.mapped == NULL );

	puts( "PASS Vulkan typed buffer-map lowering" );
	return 0;
}
