// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_memory.c — Vulkan backend:
//   * reusable aligned VkDeviceMemory blocks for ordinary buffers/images,
//     with dedicated allocations for large, transient and lazy resources;
//   * Ral_QueryMemoryBudget — VK_EXT_memory_budget when available, plus the
//     RAL's own tracked footprint;
//   * Ral_SetPressureCallback + the 1 Hz polling thread (phase-7-ral-design.md §12).

#include "ral_vulkan_internal.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#  include <time.h>
#endif

// ════════════════════════════════════════════════════════════════════════
// per-queue OS mutexes (guard command-pool alloc/free/reset + vkQueueSubmit2
// for that queue — Vulkan does not make those thread-safe)
//
// These intentionally do NOT reuse the engine's Sys_Mutex* abstraction: the RAL
// Vulkan backend is deliberately engine-independent (see ral_vulkan_internal.h
// header note — it depends only on the refimport_t `ri`, which does not expose a
// mutex primitive; Sys_Mutex* lives in code/win32 + code/unix, outside that
// import surface). Pulling Sys_Mutex* in would create a new dependency on engine
// OS internals that the `ri` indirection exists to avoid, so the small
// hand-rolled CRITICAL_SECTION / pthread_mutex_t wrappers stay local by design.
// ════════════════════════════════════════════════════════════════════════
#ifdef _WIN32
typedef CRITICAL_SECTION ralVkMutex_t;
static ralVkMutex_t *ralVk_MutexNew( void )      { ralVkMutex_t *m = (ralVkMutex_t *)malloc( sizeof( *m ) ); if ( m ) InitializeCriticalSection( m ); return m; }
static void          ralVk_MutexFree( ralVkMutex_t *m ) { if ( m ) { DeleteCriticalSection( m ); free( m ); } }
static void          ralVk_MutexLock( ralVkMutex_t *m ) { if ( m ) EnterCriticalSection( m ); }
static void          ralVk_MutexUnlock( ralVkMutex_t *m ) { if ( m ) LeaveCriticalSection( m ); }
#else
typedef pthread_mutex_t ralVkMutex_t;
static ralVkMutex_t *ralVk_MutexNew( void )      { ralVkMutex_t *m = (ralVkMutex_t *)malloc( sizeof( *m ) ); if ( m ) pthread_mutex_init( m, NULL ); return m; }
static void          ralVk_MutexFree( ralVkMutex_t *m ) { if ( m ) { pthread_mutex_destroy( m ); free( m ); } }
static void          ralVk_MutexLock( ralVkMutex_t *m ) { if ( m ) pthread_mutex_lock( m ); }
static void          ralVk_MutexUnlock( ralVkMutex_t *m ) { if ( m ) pthread_mutex_unlock( m ); }
#endif

qboolean ralVk_InitQueueMutexes( ralBackend_t *b ) {
	uint32_t q;
	for ( q = 0; q < 3; q++ ) {
		b->queueMutex[q] = (void *)ralVk_MutexNew();
		if ( !b->queueMutex[q] ) return qfalse;
	}
	return qtrue;
}
void ralVk_DestroyQueueMutexes( ralBackend_t *b ) {
	uint32_t q;
	for ( q = 0; q < 3; q++ ) { ralVk_MutexFree( (ralVkMutex_t *)b->queueMutex[q] ); b->queueMutex[q] = NULL; }
}
void ralVk_QueueLock  ( ralBackend_t *b, ralQueueType_t q ) { ralVk_MutexLock  ( (ralVkMutex_t *)b->queueMutex[q] ); }
void ralVk_QueueUnlock( ralBackend_t *b, ralQueueType_t q ) { ralVk_MutexUnlock( (ralVkMutex_t *)b->queueMutex[q] ); }

// ════════════════════════════════════════════════════════════════════════
// suballocator
// ════════════════════════════════════════════════════════════════════════
// Pick a memory type from `typeBits` whose flags include every bit of
// `required` (and, secondarily, prefer the fewest extra bits). Returns
// UINT32_MAX if none qualifies.
static uint32_t ralVk_PickMemoryType( const VkPhysicalDeviceMemoryProperties *mp,
                                      uint32_t typeBits, VkMemoryPropertyFlags required ) {
	uint32_t i, best = 0xFFFFFFFFu, bestExtra = 0xFFFFFFFFu;
	for ( i = 0; i < mp->memoryTypeCount; i++ ) {
		VkMemoryPropertyFlags f = mp->memoryTypes[i].propertyFlags;
		if ( !( typeBits & ( 1u << i ) ) )       continue;
		if ( ( f & required ) != required )      continue;
		{
			// popcount of the extra bits — fewer is "purer"
			VkMemoryPropertyFlags extra = f & ~required;
			uint32_t n = 0;
			while ( extra ) { extra &= ( extra - 1 ); n++; }
			if ( n < bestExtra ) { bestExtra = n; best = i; }
		}
	}
	return best;
}

static uint64_t ralVk_TrackedHeapBytes( const ralBackend_t *b, uint32_t heapIndex ) {
	const ralVkAllocation_t *allocation;
	uint64_t total = 0u;
	for ( allocation=b->allocations; allocation; allocation=allocation->next ) {
		const uint32_t allocationHeap = b->memProps.memoryTypes[allocation->memoryTypeIndex].heapIndex;
		if ( allocationHeap != heapIndex || allocation->size > UINT64_MAX - total ) continue;
		total += allocation->size;
	}
	return total;
}

static ralMemoryCriticality_t ralVk_MemoryCriticality(
		ralAllocationClass_t memoryClass, ralAllocationResidency_t residency ) {
	if ( memoryClass == RAL_ALLOCATION_TRANSIENT
			|| residency == RAL_ALLOCATION_RESIDENCY_TRANSIENT )
		return RAL_MEMORY_CRITICALITY_OPTIONAL;
	if ( residency == RAL_ALLOCATION_RESIDENCY_STREAMED )
		return RAL_MEMORY_CRITICALITY_STREAMING;
	return RAL_MEMORY_CRITICALITY_REQUIRED;
}

static void ralVk_PublishMemoryFailure( ralBackend_t *b,
		ralMemoryFailureCause_t cause, ralAllocationClass_t memoryClass,
		ralAllocationResidency_t residency, uint64_t requestedBytes,
		uint32_t typeIndex ) {
	ralMemoryFailureEvent_t event;
	uint32_t heapIndex = 0;
	if ( !b || requestedBytes == 0 ) return;
	RAL_ZERO( event );
	event.backendType = RAL_BACKEND_VULKAN;
	event.cause = cause;
	event.memoryClass = memoryClass;
	event.residency = residency;
	event.criticality = ralVk_MemoryCriticality( memoryClass, residency );
	event.requestedBytes = requestedBytes;
	event.attempt = 1;
	event.maxAttempts = cause == RAL_MEMORY_FAILURE_DEVICE_LOST ? 1u : 2u;
	event.liveParent = qfalse;
	if ( typeIndex < b->memProps.memoryTypeCount ) {
		heapIndex = b->memProps.memoryTypes[typeIndex].heapIndex;
		event.budgetKnown = qtrue;
		event.budgetBytes = b->memProps.memoryHeaps[heapIndex].size;
		event.usedBytes = ralVk_TrackedHeapBytes( b, heapIndex );
	}
	(void)Ral_MemoryFailureLedgerPublish( &b->memoryFailures, &event );
}

qboolean Ral_GetLastMemoryFailure( const ralBackend_t *backend,
		ralMemoryFailureReceipt_t *outReceipt ) {
	return backend ? Ral_MemoryFailureLedgerGet( &backend->memoryFailures, outReceipt ) : qfalse;
}

#define RAL_VK_DEVICE_BLOCK_BYTES ( 8u * 1024u * 1024u )
#define RAL_VK_HOST_BLOCK_BYTES   ( 2u * 1024u * 1024u )

static VkDeviceSize ralVk_BlockBytes( VkMemoryPropertyFlags properties ) {
	return ( properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT )
		? (VkDeviceSize)RAL_VK_HOST_BLOCK_BYTES : (VkDeviceSize)RAL_VK_DEVICE_BLOCK_BYTES;
}

static qboolean ralVk_BlockEligible( VkMemoryRequirements requirements,
		VkMemoryPropertyFlags properties, ralAllocationClass_t memoryClass,
		ralAllocationResidency_t residency, ralVkAllocationResourceKind_t resourceKind ) {
	const VkDeviceSize blockBytes = ralVk_BlockBytes( properties );
	return ( resourceKind == RAL_VK_ALLOC_RESOURCE_BUFFER
			|| resourceKind == RAL_VK_ALLOC_RESOURCE_IMAGE )
		&& memoryClass != RAL_ALLOCATION_TRANSIENT
		&& residency != RAL_ALLOCATION_RESIDENCY_TRANSIENT
		&& !( properties & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT )
		&& requirements.size <= blockBytes / 2u
		&& requirements.alignment != 0u;
}

static void ralVk_AddPhysicalBytes( ralBackend_t *b,
		VkMemoryPropertyFlags properties, VkDeviceSize bytes ) {
	if ( properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) b->ralDeviceLocalBytes += bytes;
	else b->ralHostVisibleBytes += bytes;
}

static void ralVk_SubtractPhysicalBytes( ralBackend_t *b,
		VkMemoryPropertyFlags properties, VkDeviceSize bytes ) {
	VkDeviceSize *tracked = ( properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
		? &b->ralDeviceLocalBytes : &b->ralHostVisibleBytes;
	*tracked = *tracked >= bytes ? *tracked - bytes : 0u;
}

static qboolean ralVk_AllocateDeviceMemory( ralBackend_t *b, VkDeviceSize bytes,
		uint32_t typeIndex, VkDeviceMemory *outMemory, VkResult *outResult ) {
	VkMemoryAllocateInfo allocateInfo;
	VkResult result;
	if ( !b || !outMemory || !outResult || bytes == 0u ) return qfalse;
	RAL_ZERO( allocateInfo );
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = bytes;
	allocateInfo.memoryTypeIndex = typeIndex;
	result = b->vk.AllocateMemory( b->device, &allocateInfo, NULL, outMemory );
	*outResult = result;
	return result == VK_SUCCESS ? qtrue : qfalse;
}

static ralVkMemoryBlock_t *ralVk_FindBlock( ralBackend_t *b, uint32_t typeIndex,
		ralVkAllocationResourceKind_t resourceKind, uintptr_t ownerIdentity,
		uint64_t ownerGeneration, VkMemoryRequirements requirements,
		ralSuballocationReceipt_t *outReceipt ) {
	ralVkMemoryBlock_t *block;
	for ( block = b->memoryBlocks; block; block = block->next ) {
		if ( block->memoryTypeIndex != typeIndex || block->resourceKind != resourceKind ) continue;
		if ( Ral_SuballocatorAllocate( &block->allocator, ownerIdentity, ownerGeneration,
				requirements.size, requirements.alignment, outReceipt ) ) return block;
	}
	return NULL;
}

static ralVkMemoryBlock_t *ralVk_CreateBlock( ralBackend_t *b, uint32_t typeIndex,
		VkMemoryPropertyFlags properties, ralVkAllocationResourceKind_t resourceKind,
		uintptr_t ownerIdentity, uint64_t ownerGeneration,
		VkMemoryRequirements requirements, ralSuballocationReceipt_t *outReceipt ) {
	ralVkMemoryBlock_t *block;
	VkResult allocationResult = VK_SUCCESS;
	const VkDeviceSize blockBytes = ralVk_BlockBytes( properties );
	const uint64_t generation = b->nextMemoryBlockGeneration + 1u;
	if ( b->nextMemoryBlockGeneration >= UINT64_MAX - 1u ) return NULL;
	block = (ralVkMemoryBlock_t *)malloc( sizeof( *block ) );
	if ( !block ) return NULL;
	RAL_ZERO( *block );
	block->backend = b;
	block->size = blockBytes;
	block->memoryTypeIndex = typeIndex;
	block->propertyFlags = properties;
	block->resourceKind = resourceKind;
	block->generation = generation;
	if ( !ralVk_AllocateDeviceMemory( b, blockBytes, typeIndex, &block->memory, &allocationResult )
			|| !Ral_SuballocatorInit( &block->allocator, RAL_BACKEND_VULKAN,
				(uintptr_t)block, generation, blockBytes )
			|| !Ral_SuballocatorAllocate( &block->allocator, ownerIdentity, ownerGeneration,
				requirements.size, requirements.alignment, outReceipt ) ) {
		if ( block->memory != VK_NULL_HANDLE ) b->vk.FreeMemory( b->device, block->memory, NULL );
		free( block );
		return NULL;
	}
	block->next = b->memoryBlocks;
	b->memoryBlocks = block;
	b->nextMemoryBlockGeneration = generation;
	ralVk_AddPhysicalBytes( b, properties, blockBytes );
	return block;
}

static void ralVk_DestroyEmptyBlock( ralBackend_t *b, ralVkMemoryBlock_t *block ) {
	ralVkMemoryBlock_t **link;
	ralSuballocationStats_t stats;
	if ( !b || !block || !Ral_SuballocatorGetStats( &block->allocator, &stats )
			|| !stats.empty || block->mapCount != 0u ) return;
	for ( link = &b->memoryBlocks; *link; link = &( *link )->next ) {
		if ( *link == block ) { *link = block->next; break; }
	}
	if ( block->mappedBase ) b->vk.UnmapMemory( b->device, block->memory );
	b->vk.FreeMemory( b->device, block->memory, NULL );
	ralVk_SubtractPhysicalBytes( b, block->propertyFlags, block->size );
	free( block );
}

static ralMemoryFailureCause_t ralVk_FailureCause( VkResult result ) {
	if ( result == VK_ERROR_OUT_OF_HOST_MEMORY ) return RAL_MEMORY_FAILURE_HOST_OOM;
	if ( result == VK_ERROR_DEVICE_LOST ) return RAL_MEMORY_FAILURE_DEVICE_LOST;
	if ( result == VK_ERROR_FRAGMENTED_POOL ) return RAL_MEMORY_FAILURE_FRAGMENTED;
	return RAL_MEMORY_FAILURE_DEVICE_OOM;
}

ralVkAllocation_t *ralVk_Alloc( ralBackend_t *b, VkMemoryRequirements req,
		VkMemoryPropertyFlags props, ralAllocationClass_t memoryClass,
		ralAllocationResidency_t residency, uintptr_t ownerIdentity,
		ralVkAllocationResourceKind_t resourceKind ) {
	ralVkAllocation_t    *a;
	uint32_t              typeIndex;
	uint32_t              heapIndex;
	uint64_t              generation;
	VkResult              allocationResult = VK_SUCCESS;
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	VkMemoryPropertyFlags want = props;
	if ( !b || ownerIdentity == (uintptr_t)0 || req.size == 0u || req.alignment == 0u
			|| resourceKind < RAL_VK_ALLOC_RESOURCE_BUFFER
			|| resourceKind > RAL_VK_ALLOC_RESOURCE_TRANSIENT
			|| b->nextAllocationGeneration >= UINT64_MAX - 1u ) return NULL;

	// LAZILY_ALLOCATED is a tile-GPU optimisation; fall back to DEVICE_LOCAL
	// where it isn't offered.
	typeIndex = ralVk_PickMemoryType( &b->memProps, req.memoryTypeBits, want );
	if ( typeIndex == 0xFFFFFFFFu && ( want & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT ) ) {
		want = ( want & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT ) | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		typeIndex = ralVk_PickMemoryType( &b->memProps, req.memoryTypeBits, want );
	}
	if ( typeIndex == 0xFFFFFFFFu ) {
		// last resort: any compatible type at all
		uint32_t i;
		for ( i = 0; i < b->memProps.memoryTypeCount; i++ )
			if ( req.memoryTypeBits & ( 1u << i ) ) { typeIndex = i; break; }
	}
	if ( typeIndex == 0xFFFFFFFFu ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_Alloc: no compatible memory type (typeBits=0x%x, props=0x%x)\n",
		        req.memoryTypeBits, (unsigned)props );
		ralVk_PublishMemoryFailure( b, RAL_MEMORY_FAILURE_NO_COMPATIBLE_TYPE,
			memoryClass, residency, req.size, UINT32_MAX );
		return NULL;
	}

	a = (ralVkAllocation_t *)malloc( sizeof( *a ) );
	if ( !a ) {
		ralVk_PublishMemoryFailure( b, RAL_MEMORY_FAILURE_HOST_OOM,
			memoryClass, residency, req.size, typeIndex );
		return NULL;
	}
	RAL_ZERO( *a );
	a->backend         = b;
	a->memoryTypeIndex = typeIndex;
	a->propertyFlags   = b->memProps.memoryTypes[ typeIndex ].propertyFlags;
	a->mapped          = NULL;
	heapIndex = b->memProps.memoryTypes[typeIndex].heapIndex;
	generation = b->nextAllocationGeneration + 1u;
	if ( ralVk_BlockEligible( req, a->propertyFlags, memoryClass, residency, resourceKind ) ) {
		a->block = ralVk_FindBlock( b, typeIndex, resourceKind, ownerIdentity,
			generation, req, &a->suballocation );
		if ( !a->block ) a->block = ralVk_CreateBlock( b, typeIndex, a->propertyFlags,
			resourceKind, ownerIdentity, generation, req, &a->suballocation );
		if ( a->block ) {
			a->memory = a->block->memory;
			a->offset = a->suballocation.offset;
			a->size = a->suballocation.committedSize;
		}
	}
	if ( !a->block ) {
		if ( !ralVk_AllocateDeviceMemory( b, req.size, typeIndex, &a->memory, &allocationResult ) ) {
			RAL_VK_LOG( SEV_WARN, "ralVk_Alloc: vkAllocateMemory failed (%llu bytes, type %u)\n",
				(unsigned long long)req.size, typeIndex );
			ralVk_PublishMemoryFailure( b, ralVk_FailureCause( allocationResult ),
				memoryClass, residency, req.size, typeIndex );
			free( a );
			return NULL;
		}
		a->size = req.size;
		a->offset = 0u;
		ralVk_AddPhysicalBytes( b, a->propertyFlags, a->size );
	}
	RAL_ZERO( request );
	request.memoryClass = memoryClass;
	request.residency = residency;
	request.size = req.size;
	request.alignment = req.alignment;
	request.ownerIdentity = ownerIdentity;
	request.ownerGeneration = generation;
	request.allowFallback = memoryClass == RAL_ALLOCATION_TRANSIENT ? qtrue : qfalse;
	RAL_ZERO( facts );
	facts.backendType = RAL_BACKEND_VULKAN;
	facts.placement = a->block ? RAL_ALLOCATION_PLACEMENT_SUBALLOCATED
		: RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize = a->size;
	facts.actualAlignment = req.alignment;
	facts.allocationGeneration = generation;
	facts.deviceLocal = (a->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? qtrue : qfalse;
	facts.hostVisible = (a->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? qtrue : qfalse;
	facts.hostCoherent = (a->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? qtrue : qfalse;
	facts.lazy = (a->propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) ? qtrue : qfalse;
	facts.budgetKnown = qtrue;
	facts.budgetBytes = b->memProps.memoryHeaps[heapIndex].size;
	facts.usedBytesBefore = ralVk_TrackedHeapBytes(b,heapIndex);
	if ( !Ral_AllocationReceiptBuild(&request,&facts,&a->receipt) ) {
		if ( a->block ) {
			ralVkMemoryBlock_t *block = a->block;
			(void)Ral_SuballocatorFree( &block->allocator, &a->suballocation );
			ralVk_DestroyEmptyBlock( b, block );
		} else {
			b->vk.FreeMemory(b->device,a->memory,NULL);
			ralVk_SubtractPhysicalBytes( b, a->propertyFlags, a->size );
		}
		ralVk_PublishMemoryFailure( b, RAL_MEMORY_FAILURE_PRESSURE,
			memoryClass, residency, req.size, typeIndex );
		free(a);
		return NULL;
	}
	b->nextAllocationGeneration = generation;

	a->next = b->allocations;
	b->allocations = a;
	b->numAllocations++;
	return a;
}

void ralVk_Free( ralBackend_t *b, ralVkAllocation_t *a ) {
	ralVkAllocation_t **pp;
	ralSuballocationStats_t stats;
	if ( !a ) return;
	if ( a->mapped ) ralVk_Unmap( a );
	if ( a->block && ( !Ral_SuballocatorFree( &a->block->allocator, &a->suballocation )
			|| !Ral_SuballocatorGetStats( &a->block->allocator, &stats ) ) ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_Free: invalid reusable-block slice receipt\n" );
		return;
	}
	for ( pp = &b->allocations; *pp; pp = &( *pp )->next ) {
		if ( *pp == a ) { *pp = a->next; b->numAllocations--; break; }
	}
	if ( a->block ) {
		ralVkMemoryBlock_t *block = a->block;
		free( a );
		if ( !stats.empty ) return;
		ralVk_DestroyEmptyBlock( b, block );
		return;
	}
	ralVk_SubtractPhysicalBytes( b, a->propertyFlags, a->size );
	b->vk.FreeMemory( b->device, a->memory, NULL );
	free( a );
}

qboolean Ral_BufferGetAllocationReceipt( const ralBuffer_t *buffer,
		ralAllocationReceipt_t *out ) {
	ralAllocationReceipt_t candidate;
	if ( !buffer || !out || !buffer->ownsBuffer || !buffer->alloc
			|| !Ral_AllocationReceiptExact(&buffer->alloc->receipt,&buffer->alloc->receipt) ) return qfalse;
	candidate = buffer->alloc->receipt;
	*out = candidate;
	return qtrue;
}

qboolean Ral_TextureGetAllocationReceipt( const ralTexture_t *texture,
		ralAllocationReceipt_t *out ) {
	ralAllocationReceipt_t candidate;
	if ( !texture || !out || !texture->ownsImage || !texture->alloc
			|| !Ral_AllocationReceiptExact(&texture->alloc->receipt,&texture->alloc->receipt) ) return qfalse;
	candidate = texture->alloc->receipt;
	*out = candidate;
	return qtrue;
}

qboolean Ral_BufferGetSuballocationReceipt( const ralBuffer_t *buffer,
		ralSuballocationReceipt_t *outReceipt ) {
	ralSuballocationReceipt_t candidate;
	if ( !buffer || !outReceipt || !buffer->ownsBuffer || !buffer->alloc
			|| !buffer->alloc->block
			|| !Ral_SuballocationReceiptExact( &buffer->alloc->suballocation,
				&buffer->alloc->suballocation ) ) return qfalse;
	candidate = buffer->alloc->suballocation;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_TextureGetSuballocationReceipt( const ralTexture_t *texture,
		ralSuballocationReceipt_t *outReceipt ) {
	ralSuballocationReceipt_t candidate;
	if ( !texture || !outReceipt || !texture->ownsImage || !texture->alloc
			|| !texture->alloc->block
			|| !Ral_SuballocationReceiptExact( &texture->alloc->suballocation,
				&texture->alloc->suballocation ) ) return qfalse;
	candidate = texture->alloc->suballocation;
	*outReceipt = candidate;
	return qtrue;
}

void *ralVk_Map( ralVkAllocation_t *a ) {
	ralBackend_t *b;
	void *base = NULL;
	// `a` must be a HOST_VISIBLE allocation. Persistent map.
	if ( !a ) return NULL;
	b = a->backend;
	if ( a->mapped ) return a->mapped;
	if ( !( a->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_Map: allocation is not host-visible\n" );
		return NULL;
	}
	if ( a->block ) {
		if ( !a->block->mappedBase
				&& b->vk.MapMemory( b->device, a->memory, 0, VK_WHOLE_SIZE, 0,
					&a->block->mappedBase ) != VK_SUCCESS ) return NULL;
		a->mapped = (void *)( (byte *)a->block->mappedBase + a->offset );
		a->block->mapCount++;
		return a->mapped;
	}
	if ( b->vk.MapMemory( b->device, a->memory, 0, VK_WHOLE_SIZE, 0, &base ) == VK_SUCCESS )
		a->mapped = base;
	return a->mapped;
}

void ralVk_Unmap( ralVkAllocation_t *a ) {
	if ( !a || !a->mapped ) return;
	if ( a->block ) {
		a->mapped = NULL;
		if ( a->block->mapCount ) a->block->mapCount--;
		if ( a->block->mapCount == 0u && a->block->mappedBase ) {
			a->backend->vk.UnmapMemory( a->backend->device, a->memory );
			a->block->mappedBase = NULL;
		}
		return;
	}
	a->backend->vk.UnmapMemory( a->backend->device, a->memory );
	a->mapped = NULL;
}

void ralVk_Flush( ralVkAllocation_t *a, VkDeviceSize offset, VkDeviceSize size ) {
	VkMappedMemoryRange r;
	(void)offset; (void)size;   // 7.2: always flush the whole allocation (avoids nonCoherentAtomSize alignment juggling)
	if ( !a ) return;
	if ( a->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) return;   // no flush needed
	RAL_ZERO( r );
	r.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	r.memory = a->memory;
	r.offset = 0;
	r.size   = VK_WHOLE_SIZE;
	a->backend->vk.FlushMappedMemoryRanges( a->backend->device, 1, &r );
}

void ralVk_Invalidate( ralVkAllocation_t *a, VkDeviceSize offset, VkDeviceSize size ) {
	VkMappedMemoryRange r;
	(void)offset; (void)size;
	if ( !a || ( a->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) ) return;
	RAL_ZERO( r );
	r.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	r.memory = a->memory;
	r.offset = 0;
	r.size   = VK_WHOLE_SIZE;
	a->backend->vk.InvalidateMappedMemoryRanges( a->backend->device, 1, &r );
}

// ════════════════════════════════════════════════════════════════════════
// memory budget
// ════════════════════════════════════════════════════════════════════════
static void ralVk_RawBudget( ralBackend_t *b, ralMemoryBudget_t *out ) {
	VkPhysicalDeviceMemoryBudgetPropertiesEXT bp;
	VkPhysicalDeviceMemoryProperties2         mp2;
	uint32_t                                  i;
	qboolean                                  dl, hv;

	RAL_ZERO( *out );
	RAL_ZERO( bp );  bp.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
	RAL_ZERO( mp2 ); mp2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	mp2.pNext = b->haveMemoryBudget ? (void *)&bp : NULL;
	b->vk.GetPhysicalDeviceMemoryProperties2( b->physicalDevice, &mp2 );

	for ( i = 0; i < mp2.memoryProperties.memoryHeapCount; i++ ) {
		VkDeviceSize size   = mp2.memoryProperties.memoryHeaps[i].size;
		VkDeviceSize used   = b->haveMemoryBudget ? bp.heapUsage[i]  : 0;
		VkDeviceSize budget = b->haveMemoryBudget ? bp.heapBudget[i] : size;
		if ( budget == 0 ) budget = size;
		if ( mp2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
			out->deviceLocalUsed   += used;
			out->deviceLocalBudget += budget;
		} else {
			out->hostVisibleUsed   += used;
			out->hostVisibleBudget += budget;
		}
	}
	// If the driver gives no real usage (no VK_EXT_memory_budget), at least
	// surface the RAL's own footprint so the number isn't a flat zero.
	if ( !b->haveMemoryBudget ) {
		out->deviceLocalUsed = b->ralDeviceLocalBytes;
		out->hostVisibleUsed = b->ralHostVisibleBytes;
	}
	dl = ( out->deviceLocalBudget > 0 ) && ( out->deviceLocalUsed * 100ull > out->deviceLocalBudget * 85ull );
	hv = ( out->hostVisibleBudget > 0 ) && ( out->hostVisibleUsed * 100ull > out->hostVisibleBudget * 85ull );
	out->underPressure = ( dl || hv ) ? qtrue : qfalse;
}

void Ral_QueryMemoryBudget( ralBackend_t *b, ralMemoryBudget_t *out ) {
	if ( !out ) return;
	RAL_ZERO( *out );
	if ( !b || b->physicalDevice == VK_NULL_HANDLE || !b->vk.GetPhysicalDeviceMemoryProperties2 )
		return;
	ralVk_RawBudget( b, out );
}

// ════════════════════════════════════════════════════════════════════════
// pressure polling — §12.4 thresholds: WARNING at 75 %, CRITICAL at 90 %
// ════════════════════════════════════════════════════════════════════════
static ralPressureLevel_t ralVk_LevelOf( const ralMemoryBudget_t *mb ) {
	uint32_t dlPm = ( mb->deviceLocalBudget > 0 ) ? (uint32_t)( ( mb->deviceLocalUsed * 1000ull ) / mb->deviceLocalBudget ) : 0;
	uint32_t hvPm = ( mb->hostVisibleBudget > 0 ) ? (uint32_t)( ( mb->hostVisibleUsed * 1000ull ) / mb->hostVisibleBudget ) : 0;
	uint32_t pm   = ( dlPm > hvPm ) ? dlPm : hvPm;
	if ( pm >= 900 ) return RAL_PRESSURE_CRITICAL;
	if ( pm >= 750 ) return RAL_PRESSURE_WARNING;
	return RAL_PRESSURE_NORMAL;
}

#ifdef _WIN32
static DWORD WINAPI ralVk_PollThreadProc( LPVOID param )
#else
static void *ralVk_PollThreadProc( void *param )
#endif
{
	ralBackend_t *b = (ralBackend_t *)param;
	while ( !b->pollThreadStop ) {
#ifdef _WIN32
		Sleep( 1000 );
#else
		{ struct timespec ts; ts.tv_sec = 1; ts.tv_nsec = 0; nanosleep( &ts, NULL ); }
#endif
		if ( b->pollThreadStop ) break;
		{
			ralMemoryBudget_t  mb;
			ralPressureLevel_t lvl;
			Ral_QueryMemoryBudget( b, &mb );
			lvl = ralVk_LevelOf( &mb );
			if ( lvl != b->lastPressureLevel ) {
				ralPressureCallback_t cb   = b->pressureCb;
				void                 *user = b->pressureUser;
				b->lastPressureLevel = lvl;
				if ( cb )
					cb( b, lvl, &mb, user );
			}
		}
	}
#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

void ralVk_StopPollThread( ralBackend_t *b ) {
	if ( !b || !b->pollThread ) return;
	b->pollThreadStop = 1;
#ifdef _WIN32
	{
		HANDLE h = (HANDLE)b->pollThread;
		WaitForSingleObject( h, 5000 );   // thread checks the flag every ~1 s
		CloseHandle( h );
	}
#else
	{
		pthread_t *pt = (pthread_t *)b->pollThread;
		pthread_join( *pt, NULL );
		free( pt );
	}
#endif
	b->pollThread = NULL;
}

void Ral_SetPressureCallback( ralBackend_t *b, ralPressureCallback_t cb, void *user ) {
	if ( !b ) return;
	ralVk_StopPollThread( b );          // drop any existing poller
	b->pressureCb        = cb;
	b->pressureUser      = user;
	b->lastPressureLevel = RAL_PRESSURE_NORMAL;
	if ( !cb ) return;                  // de-registration

	b->pollThreadStop = 0;
#ifdef _WIN32
	b->pollThread = (void *)CreateThread( NULL, 0, ralVk_PollThreadProc, b, 0, NULL );
#else
	{
		pthread_t *pt = (pthread_t *)malloc( sizeof( pthread_t ) );
		if ( pt && pthread_create( pt, NULL, ralVk_PollThreadProc, b ) == 0 )
			b->pollThread = (void *)pt;
		else {
			if ( pt ) free( pt );
			b->pollThread = NULL;
		}
	}
#endif
	if ( !b->pollThread )
		RAL_VK_LOG( SEV_WARN, "Ral_SetPressureCallback: could not start polling thread; pressure events disabled\n" );
}
