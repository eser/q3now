// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_memory.c — Vulkan backend:
//   * memory suballocator (ralVk_Alloc/Free/Map/Unmap/Flush) — Phase 7.2,
//     one VkDeviceMemory per resource; the API is what matters, the backing
//     strategy is replaceable;
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

ralVkAllocation_t *ralVk_Alloc( ralBackend_t *b, VkMemoryRequirements req, VkMemoryPropertyFlags props ) {
	VkMemoryAllocateInfo  ai;
	ralVkAllocation_t    *a;
	uint32_t              typeIndex;
	VkMemoryPropertyFlags want = props;

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
		ri.Log( SEV_WARN, "[RAL] ralVk_Alloc: no compatible memory type (typeBits=0x%x, props=0x%x)\n",
		        req.memoryTypeBits, (unsigned)props );
		return NULL;
	}

	a = (ralVkAllocation_t *)malloc( sizeof( *a ) );
	if ( !a ) return NULL;
	RAL_ZERO( *a );
	RAL_ZERO( ai );
	ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize  = req.size;
	ai.memoryTypeIndex = typeIndex;
	if ( b->vk.AllocateMemory( b->device, &ai, NULL, &a->memory ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] ralVk_Alloc: vkAllocateMemory failed (%llu bytes, type %u)\n",
		        (unsigned long long)req.size, typeIndex );
		free( a );
		return NULL;
	}
	a->backend         = b;
	a->size            = req.size;
	a->memoryTypeIndex = typeIndex;
	a->propertyFlags   = b->memProps.memoryTypes[ typeIndex ].propertyFlags;
	a->mapped          = NULL;

	a->next = b->allocations;
	b->allocations = a;
	b->numAllocations++;
	if ( a->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) b->ralDeviceLocalBytes += a->size;
	else                                                          b->ralHostVisibleBytes += a->size;
	return a;
}

void ralVk_Free( ralBackend_t *b, ralVkAllocation_t *a ) {
	ralVkAllocation_t **pp;
	if ( !a ) return;
	if ( a->mapped ) { b->vk.UnmapMemory( b->device, a->memory ); a->mapped = NULL; }
	for ( pp = &b->allocations; *pp; pp = &( *pp )->next ) {
		if ( *pp == a ) { *pp = a->next; b->numAllocations--; break; }
	}
	if ( a->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) {
		if ( b->ralDeviceLocalBytes >= a->size ) b->ralDeviceLocalBytes -= a->size; else b->ralDeviceLocalBytes = 0;
	} else {
		if ( b->ralHostVisibleBytes >= a->size ) b->ralHostVisibleBytes -= a->size; else b->ralHostVisibleBytes = 0;
	}
	b->vk.FreeMemory( b->device, a->memory, NULL );
	free( a );
}

void *ralVk_Map( ralVkAllocation_t *a ) {
	// `a` must be a HOST_VISIBLE allocation. Persistent map.
	if ( !a ) return NULL;
	if ( a->mapped ) return a->mapped;
	if ( !( a->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) ) {
		ri.Log( SEV_WARN, "[RAL] ralVk_Map: allocation is not host-visible\n" );
		return NULL;
	}
	if ( a->backend->vk.MapMemory( a->backend->device, a->memory, 0, VK_WHOLE_SIZE, 0, &a->mapped ) != VK_SUCCESS )
		a->mapped = NULL;
	return a->mapped;
}

void ralVk_Unmap( ralVkAllocation_t *a ) {
	if ( !a || !a->mapped ) return;
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
		ri.Log( SEV_WARN, "[RAL] Ral_SetPressureCallback: could not start polling thread; pressure events disabled\n" );
}
