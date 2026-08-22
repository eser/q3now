// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

static unsigned char storage[256];
static uint32_t mapCalls, unmapCalls;

static VKAPI_ATTR VkResult VKAPI_CALL MapMemory( VkDevice device,
	VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
	VkMemoryMapFlags flags, void **out ) {
	(void)device; (void)memory; (void)offset; (void)size; (void)flags;
	mapCalls++;
	*out = storage;
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL UnmapMemory( VkDevice device,
	VkDeviceMemory memory ) {
	(void)device; (void)memory;
	unmapCalls++;
}

static int BuildAllocation( ralBuffer_t *buffer, ralVkAllocation_t *allocation ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset( &request, 0, sizeof( request ) );
	request.memoryClass = RAL_ALLOCATION_UPLOAD;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = sizeof( storage );
	request.alignment = 16u;
	request.ownerIdentity = (uintptr_t)buffer;
	request.ownerGeneration = 1u;
	request.allowFallback = qfalse;
	memset( &facts, 0, sizeof( facts ) );
	facts.backendType = RAL_BACKEND_VULKAN;
	facts.placement = RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.allocationGeneration = 3u;
	facts.committedSize = sizeof( storage );
	facts.actualAlignment = 16u;
	facts.hostVisible = qtrue;
	facts.hostCoherent = qtrue;
	return Ral_AllocationReceiptBuild( &request, &facts,
		&allocation->receipt ) ? 0 : 1;
}

int main( void ) {
	ralBackend_t backend;
	ralBuffer_t buffer;
	ralVkAllocation_t allocation;
	ralBufferUploadReceipt_t receipt, sentinel, out;
	const unsigned char first[] = { 1u, 2u, 3u, 4u };
	const unsigned char second[] = { 9u, 8u };
	uint64_t generation;

	memset( &backend, 0, sizeof( backend ) );
	backend.device = (VkDevice)(uintptr_t)0x10u;
	backend.vk.MapMemory = MapMemory;
	backend.vk.UnmapMemory = UnmapMemory;
	memset( &buffer, 0, sizeof( buffer ) );
	buffer.backend = &backend;
	buffer.buffer = (VkBuffer)(uintptr_t)0x20u;
	buffer.size = sizeof( storage );
	buffer.memoryType = RAL_MEMORY_HOST_COHERENT;
	buffer.usage = RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST;
	buffer.hostVisible = qtrue;
	buffer.coherent = qtrue;
	buffer.ownsBuffer = qtrue;
	buffer.portableStateKnown = qtrue;
	Ral_BufferMapLifecycleInit( &buffer.mapLifecycle );
	memset( &allocation, 0, sizeof( allocation ) );
	allocation.backend = &backend;
	allocation.memory = (VkDeviceMemory)(uintptr_t)0x30u;
	allocation.size = sizeof( storage );
	allocation.propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	CHECK( BuildAllocation( &buffer, &allocation ) == 0 );
	buffer.alloc = &allocation;

	memset( storage, 0, sizeof( storage ) );
	memset( &receipt, 0, sizeof( receipt ) );
	CHECK( Ral_BufferWriteImmediate( &buffer, 8u, first, sizeof( first ),
		&receipt ) );
	CHECK( mapCalls == 1u && unmapCalls == 1u );
	CHECK( memcmp( storage + 8u, first, sizeof( first ) ) == 0 );
	CHECK( receipt.ready == qtrue
		&& receipt.transfer.state == RAL_TRANSFER_COMPLETED
		&& receipt.transfer.outcome == RAL_TRANSFER_OUTCOME_SYNCHRONOUS
		&& receipt.transfer.request.backendType == RAL_BACKEND_VULKAN
		&& receipt.transfer.request.resourceIdentity == (uintptr_t)&buffer
		&& receipt.transfer.request.resourceGeneration == 3u
		&& receipt.transfer.request.byteOffset == 8u
		&& receipt.transfer.request.byteSize == sizeof( first )
		&& receipt.transfer.request.byteBudget == sizeof( storage )
		&& receipt.transfer.request.queue == RAL_QUEUE_GRAPHICS
		&& receipt.graphicsVisibilityGeneration
			== receipt.transfer.completionGeneration );
	CHECK( Ral_BufferUploadReceiptExact( &receipt, &receipt ) );
	CHECK( backend.nextTransferGeneration == 1u );

	// A second write is a new generation and may update a disjoint range.
	CHECK( Ral_BufferWriteImmediate( &buffer, 32u, second, sizeof( second ),
		&out ) );
	CHECK( backend.nextTransferGeneration == 2u
		&& out.transfer.transferGeneration == 2u
		&& memcmp( storage + 32u, second, sizeof( second ) ) == 0 );

	// Every invalid request is output-atomic and consumes neither a generation
	// nor a native map call.
	sentinel = receipt;
	generation = backend.nextTransferGeneration;
	out = sentinel;
	CHECK( !Ral_BufferWriteImmediate( &buffer, sizeof( storage ) - 1u,
		first, sizeof( first ), &out ) );
	CHECK( Ral_BufferUploadReceiptExact( &out, &sentinel )
		&& backend.nextTransferGeneration == generation && mapCalls == 2u );
	buffer.usage = RAL_BUFFER_UNIFORM;
	CHECK( !Ral_BufferWriteImmediate( &buffer, 0u, first, sizeof( first ), &out ) );
	buffer.usage |= RAL_BUFFER_TRANSFER_DST;
	buffer.hostVisible = qfalse;
	CHECK( !Ral_BufferWriteImmediate( &buffer, 0u, first, sizeof( first ), &out ) );
	buffer.hostVisible = qtrue;
	buffer.legacyMapped = qtrue;
	CHECK( !Ral_BufferWriteImmediate( &buffer, 0u, first, sizeof( first ), &out ) );
	buffer.legacyMapped = qfalse;
	buffer.portableStateKnown = qfalse;
	CHECK( !Ral_BufferWriteImmediate( &buffer, 0u, first, sizeof( first ), &out ) );
	buffer.portableStateKnown = qtrue;
	backend.nextTransferGeneration = UINT64_MAX - 1u;
	CHECK( !Ral_BufferWriteImmediate( &buffer, 0u, first, sizeof( first ), &out ) );
	CHECK( Ral_BufferUploadReceiptExact( &out, &sentinel ) && mapCalls == 2u );

	puts( "PASS Vulkan immediate buffer write receipt" );
	return 0;
}
