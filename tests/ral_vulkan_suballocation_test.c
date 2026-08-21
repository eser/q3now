// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	void *bytes;
	VkDeviceSize size;
} fakeMemory_t;

static uint32_t allocateCalls;
static uint32_t freeCalls;
static uint32_t mapCalls;
static uint32_t unmapCalls;
static uint32_t failNextAllocation;

static VKAPI_ATTR VkResult VKAPI_CALL FakeAllocateMemory( VkDevice device,
		const VkMemoryAllocateInfo *info, const VkAllocationCallbacks *callbacks,
		VkDeviceMemory *outMemory ) {
	fakeMemory_t *memory;
	(void)device;
	(void)callbacks;
	if ( !info || !outMemory || !info->allocationSize ) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	if ( failNextAllocation ) { failNextAllocation--; return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
	memory = (fakeMemory_t *)malloc( sizeof( *memory ) );
	if ( !memory ) return VK_ERROR_OUT_OF_HOST_MEMORY;
	memory->bytes = calloc( 1u, (size_t)info->allocationSize );
	if ( !memory->bytes ) { free( memory ); return VK_ERROR_OUT_OF_HOST_MEMORY; }
	memory->size = info->allocationSize;
	*outMemory = (VkDeviceMemory)(uintptr_t)memory;
	allocateCalls++;
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL FakeFreeMemory( VkDevice device,
		VkDeviceMemory handle, const VkAllocationCallbacks *callbacks ) {
	fakeMemory_t *memory = (fakeMemory_t *)(uintptr_t)handle;
	(void)device;
	(void)callbacks;
	if ( !memory ) return;
	free( memory->bytes );
	free( memory );
	freeCalls++;
}

static VKAPI_ATTR VkResult VKAPI_CALL FakeMapMemory( VkDevice device,
		VkDeviceMemory handle, VkDeviceSize offset, VkDeviceSize size,
		VkMemoryMapFlags flags, void **outData ) {
	fakeMemory_t *memory = (fakeMemory_t *)(uintptr_t)handle;
	(void)device;
	(void)flags;
	if ( !memory || !outData || offset > memory->size
			|| ( size != VK_WHOLE_SIZE && size > memory->size - offset ) ) return VK_ERROR_MEMORY_MAP_FAILED;
	*outData = (byte *)memory->bytes + offset;
	mapCalls++;
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL FakeUnmapMemory( VkDevice device, VkDeviceMemory memory ) {
	(void)device;
	(void)memory;
	unmapCalls++;
}

void ralVk_Logf( const ralBackend_t *backend, ralLogSeverity_t severity,
		const char *format, ... ) {
	(void)backend;
	(void)severity;
	(void)format;
}

static void InitBackend( ralBackend_t *backend ) {
	memset( backend, 0, sizeof( *backend ) );
	backend->device = (VkDevice)(uintptr_t)0x1u;
	backend->memProps.memoryHeapCount = 2u;
	backend->memProps.memoryHeaps[0].size = 64u * 1024u * 1024u;
	backend->memProps.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
	backend->memProps.memoryHeaps[1].size = 64u * 1024u * 1024u;
	backend->memProps.memoryTypeCount = 2u;
	backend->memProps.memoryTypes[0].heapIndex = 0u;
	backend->memProps.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	backend->memProps.memoryTypes[1].heapIndex = 1u;
	backend->memProps.memoryTypes[1].propertyFlags =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	backend->vk.AllocateMemory = FakeAllocateMemory;
	backend->vk.FreeMemory = FakeFreeMemory;
	backend->vk.MapMemory = FakeMapMemory;
	backend->vk.UnmapMemory = FakeUnmapMemory;
}

int main( void ) {
	ralBackend_t backend;
	ralVkAllocation_t *bufferA, *bufferB, *imageA, *hostA, *hostB, *large, *transient;
	ralVkAllocation_t *fallback, *resizeOld, *resizeNew;
	ralSuballocationReceipt_t receipt;
	VkMemoryRequirements requirements;
	void *mapA, *mapB;
	InitBackend( &backend );
	memset( &requirements, 0, sizeof( requirements ) );
	requirements.memoryTypeBits = 1u;
	requirements.size = 128u * 1024u;
	requirements.alignment = 256u;
	bufferA = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_DEVICE_LOCAL, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x1000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	bufferB = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_DEVICE_LOCAL, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x2000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	CHECK( bufferA && bufferB && bufferA->block == bufferB->block );
	CHECK( bufferA->memory == bufferB->memory && bufferA->offset == 0u
		&& bufferB->offset == requirements.size && allocateCalls == 1u );
	CHECK( bufferA->receipt.placement == RAL_ALLOCATION_PLACEMENT_SUBALLOCATED );
	CHECK( Ral_SuballocationReceiptExact( &bufferA->suballocation, &bufferA->suballocation ) );
	{
		ralBuffer_t wrapper;
		memset( &wrapper, 0, sizeof( wrapper ) );
		wrapper.ownsBuffer = qtrue;
		wrapper.alloc = bufferA;
		CHECK( Ral_BufferGetSuballocationReceipt( &wrapper, &receipt ) );
		CHECK( Ral_SuballocationReceiptExact( &receipt, &bufferA->suballocation ) );
	}

	imageA = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_DEVICE_LOCAL, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x3000u, RAL_VK_ALLOC_RESOURCE_IMAGE );
	CHECK( imageA && imageA->block != bufferA->block && allocateCalls == 2u );

	requirements.memoryTypeBits = 2u;
	requirements.size = 64u * 1024u;
	hostA = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		RAL_ALLOCATION_UPLOAD, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x4000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	hostB = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		RAL_ALLOCATION_UPLOAD, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x5000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	CHECK( hostA && hostB && hostA->block == hostB->block );
	mapA = ralVk_Map( hostA );
	mapB = ralVk_Map( hostB );
	CHECK( mapA && mapB && mapA != mapB && mapCalls == 1u );
	ralVk_Unmap( hostA );
	CHECK( unmapCalls == 0u );
	ralVk_Unmap( hostB );
	CHECK( unmapCalls == 1u );

	requirements.memoryTypeBits = 1u;
	requirements.size = 5u * 1024u * 1024u;
	large = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_DEVICE_LOCAL, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x6000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	CHECK( large && !large->block && large->offset == 0u
		&& large->receipt.placement == RAL_ALLOCATION_PLACEMENT_DEDICATED );
	requirements.size = 64u * 1024u;
	transient = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_TRANSIENT, RAL_ALLOCATION_RESIDENCY_TRANSIENT,
		(uintptr_t)0x7000u, RAL_VK_ALLOC_RESOURCE_TRANSIENT );
	CHECK( transient && !transient->block
		&& transient->receipt.placement == RAL_ALLOCATION_PLACEMENT_DEDICATED );

	// A failed preferred-block allocation falls back to a dedicated allocation;
	// existing live parents remain untouched.
	requirements.memoryTypeBits = 2u;
	requirements.size = 32u * 1024u;
	failNextAllocation = 1u;
	fallback = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		RAL_ALLOCATION_UPLOAD, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x8000u, RAL_VK_ALLOC_RESOURCE_IMAGE );
	CHECK( fallback && !fallback->block
		&& fallback->receipt.placement == RAL_ALLOCATION_PLACEMENT_DEDICATED );

	// Resize is candidate-first: the larger replacement is live before the old
	// dedicated parent is retired.
	requirements.memoryTypeBits = 1u;
	requirements.size = 6u * 1024u * 1024u;
	resizeOld = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_DEVICE_LOCAL, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0x9000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	requirements.size = 7u * 1024u * 1024u;
	resizeNew = ralVk_Alloc( &backend, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		RAL_ALLOCATION_DEVICE_LOCAL, RAL_ALLOCATION_RESIDENCY_PERMANENT,
		(uintptr_t)0xa000u, RAL_VK_ALLOC_RESOURCE_BUFFER );
	CHECK( resizeOld && resizeNew && resizeOld->memory != resizeNew->memory );

	CHECK( freeCalls == 0u );
	ralVk_Free( &backend, bufferA );
	CHECK( freeCalls == 0u );
	ralVk_Free( &backend, bufferB );
	CHECK( freeCalls == 1u );
	ralVk_Free( &backend, imageA );
	ralVk_Free( &backend, hostA );
	CHECK( freeCalls == 2u );
	ralVk_Free( &backend, hostB );
	ralVk_Free( &backend, large );
	ralVk_Free( &backend, transient );
	ralVk_Free( &backend, fallback );
	ralVk_Free( &backend, resizeOld );
	CHECK( resizeNew->receipt.ready == qtrue );
	ralVk_Free( &backend, resizeNew );
	CHECK( freeCalls == allocateCalls && backend.allocations == NULL
		&& backend.memoryBlocks == NULL && backend.numAllocations == 0u
		&& backend.ralDeviceLocalBytes == 0u && backend.ralHostVisibleBytes == 0u );
	puts( "ral Vulkan suballocation: PASS" );
	return 0;
}
