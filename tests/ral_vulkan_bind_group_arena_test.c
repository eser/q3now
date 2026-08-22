// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; } } while ( 0 )

static uint32_t createCalls, resetCalls, destroyCalls;
static uint32_t allocateCalls, updateCalls, freeCalls;
static VkResult createResult = VK_SUCCESS, resetResult = VK_SUCCESS;
static VkDescriptorPoolCreateInfo capturedInfo;
static VkDescriptorPoolSize capturedSizes[ RAL_MAX_BIND_GROUP_ARENA_ENTRIES ];
static VkDescriptorPool capturedAllocationPool;
static VkDescriptorType capturedWriteType;

static VKAPI_ATTR VkResult VKAPI_CALL CaptureCreateDescriptorPool(
		VkDevice device, const VkDescriptorPoolCreateInfo *info,
		const VkAllocationCallbacks *allocator, VkDescriptorPool *pool ) {
	uint32_t i;
	(void)device; (void)allocator;
	createCalls++;
	if ( !info || !pool ) return VK_ERROR_INITIALIZATION_FAILED;
	capturedInfo = *info;
	for ( i = 0u; i < info->poolSizeCount; ++i )
		capturedSizes[i] = info->pPoolSizes[i];
	capturedInfo.pPoolSizes = capturedSizes;
	if ( createResult == VK_SUCCESS )
		*pool = (VkDescriptorPool)(uintptr_t)0x55u;
	return createResult;
}

static VKAPI_ATTR VkResult VKAPI_CALL CaptureResetDescriptorPool(
		VkDevice device, VkDescriptorPool pool,
		VkDescriptorPoolResetFlags flags ) {
	(void)device;
	resetCalls++;
	return pool == (VkDescriptorPool)(uintptr_t)0x55u && flags == 0u
		? resetResult : VK_ERROR_INITIALIZATION_FAILED;
}

static VKAPI_ATTR void VKAPI_CALL CaptureDestroyDescriptorPool(
		VkDevice device, VkDescriptorPool pool,
		const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator;
	if ( pool == (VkDescriptorPool)(uintptr_t)0x55u ) destroyCalls++;
}

static VKAPI_ATTR VkResult VKAPI_CALL CaptureAllocateDescriptorSets(
		VkDevice device, const VkDescriptorSetAllocateInfo *info,
		VkDescriptorSet *sets ) {
	(void)device;
	allocateCalls++;
	if ( !info || !sets || info->descriptorSetCount != 1u )
		return VK_ERROR_INITIALIZATION_FAILED;
	capturedAllocationPool = info->descriptorPool;
	sets[0] = (VkDescriptorSet)(uintptr_t)0x66u;
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL CaptureUpdateDescriptorSets(
		VkDevice device, uint32_t writeCount,
		const VkWriteDescriptorSet *writes, uint32_t copyCount,
		const VkCopyDescriptorSet *copies ) {
	(void)device; (void)copyCount; (void)copies;
	updateCalls += writeCount;
	if ( writeCount == 1u && writes ) capturedWriteType = writes[0].descriptorType;
}

static VKAPI_ATTR VkResult VKAPI_CALL CaptureFreeDescriptorSets(
		VkDevice device, VkDescriptorPool pool, uint32_t count,
		const VkDescriptorSet *sets ) {
	(void)device; (void)pool; (void)count; (void)sets;
	freeCalls++;
	return VK_SUCCESS;
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralBindGroupArenaEntry_t entries[5];
	ralBindGroupArenaCreateInfo_t info;
	ralBindGroupArenaReceipt_t first, second, stale, sentinel, untouched;
	ralBindGroupArena_t *arena;
	ralBindGroupLayout_t layout;
	ralBuffer_t buffer;
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t groupInfo;
	ralBindGroup_t *group;

	memset( &backend, 0, sizeof( backend ) );
	memset( &otherBackend, 0, sizeof( otherBackend ) );
	backend.device = (VkDevice)(uintptr_t)0x10u;
	backend.vk.CreateDescriptorPool = CaptureCreateDescriptorPool;
	backend.vk.ResetDescriptorPool = CaptureResetDescriptorPool;
	backend.vk.DestroyDescriptorPool = CaptureDestroyDescriptorPool;
	backend.vk.AllocateDescriptorSets = CaptureAllocateDescriptorSets;
	backend.vk.UpdateDescriptorSets = CaptureUpdateDescriptorSets;
	backend.vk.FreeDescriptorSets = CaptureFreeDescriptorSets;
	backend.descriptorPool = (VkDescriptorPool)(uintptr_t)0x77u;
	memset( entries, 0, sizeof( entries ) );
	entries[0] = (ralBindGroupArenaEntry_t){ RAL_BIND_COMBINED_TEXTURE_SAMPLER, 100u, qfalse };
	entries[1] = (ralBindGroupArenaEntry_t){ RAL_BIND_UNIFORM_BUFFER, 20u, qtrue };
	entries[2] = (ralBindGroupArenaEntry_t){ RAL_BIND_STORAGE_BUFFER, 10u, qtrue };
	entries[3] = (ralBindGroupArenaEntry_t){ RAL_BIND_STORAGE_BUFFER, 30u, qfalse };
	entries[4] = (ralBindGroupArenaEntry_t){ RAL_BIND_UNIFORM_BUFFER, 40u, qfalse };
	info = (ralBindGroupArenaCreateInfo_t){ entries, 5u, 200u, "fixture" };
	memset( &sentinel, 0x5a, sizeof( sentinel ) );
	untouched = sentinel;
	arena = Ral_CreateBindGroupArena( &backend, &info, &first );
	CHECK( arena != NULL && createCalls == 1u
		&& Ral_BindGroupArenaReceiptValid( &first )
		&& first.backendIdentity == &backend && first.arenaIdentity == arena
		&& first.generation == 1u && first.maxGroups == 200u
		&& Ral_GetBindGroupArenaHandle( arena ) == (void *)(uintptr_t)0x55u );
	CHECK( capturedInfo.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO
		&& capturedInfo.flags == 0u && capturedInfo.maxSets == 200u
		&& capturedInfo.poolSizeCount == 5u
		&& capturedSizes[0].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		&& capturedSizes[1].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
		&& capturedSizes[2].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
		&& capturedSizes[3].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		&& capturedSizes[4].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	CHECK( Ral_GetBindGroupArenaReceipt( arena, &second ) == ralSuccess
		&& Ral_BindGroupArenaReceiptExact( &first, &second ) );

	memset( &layout, 0, sizeof( layout ) );
	layout.backend = &backend;
	layout.layout = (VkDescriptorSetLayout)(uintptr_t)0x44u;
	layout.numEntries = 1u;
	layout.dynamicOffsetCount = 1u;
	layout.entries[0].binding = 0u;
	layout.entries[0].vkType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	layout.entries[0].count = 1u;
	layout.entries[0].effectiveCount = 1u;
	layout.entries[0].dynamicOffset = qtrue;
	memset( &buffer, 0, sizeof( buffer ) );
	buffer.backend = &backend;
	buffer.buffer = (VkBuffer)(uintptr_t)0x33u;
	buffer.size = 256u;
	memset( &value, 0, sizeof( value ) );
	value.binding = 0u;
	value.type = RAL_BIND_UNIFORM_BUFFER;
	value.buffer = &buffer;
	value.bufferRange = 64u;
	memset( &groupInfo, 0, sizeof( groupInfo ) );
	groupInfo.layout = &layout;
	groupInfo.values = &value;
	groupInfo.numValues = 1u;
	groupInfo.arena = arena;
	groupInfo.arenaReceipt = &first;
	group = Ral_CreateBindGroup( &backend, &groupInfo );
	CHECK( group != NULL && group->arena == arena
		&& Ral_BindGroupArenaReceiptExact( &group->arenaReceipt, &first )
		&& ralVk_BindGroupArenaLive( group )
		&& capturedAllocationPool == (VkDescriptorPool)(uintptr_t)0x55u
		&& allocateCalls == 1u && updateCalls == 1u
		&& capturedWriteType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC );
	stale = first; stale.generation++;
	groupInfo.arenaReceipt = &stale;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 1u );
	groupInfo.arenaReceipt = NULL;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 1u );
	groupInfo.arena = NULL;
	groupInfo.arenaReceipt = &first;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 1u );

	stale = first; stale.generation++;
	CHECK( Ral_ResetBindGroupArenaExact( arena, &stale, &untouched )
		== ralErrorInvalidArgument && resetCalls == 0u
		&& memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	resetResult = VK_ERROR_DEVICE_LOST;
	CHECK( Ral_ResetBindGroupArenaExact( arena, &first, &untouched )
		== ralErrorDeviceLost && resetCalls == 1u );
	CHECK( Ral_GetBindGroupArenaReceipt( arena, &second ) == ralSuccess
		&& Ral_BindGroupArenaReceiptExact( &first, &second ) );
	resetResult = VK_ERROR_UNKNOWN;
	untouched = sentinel;
	CHECK( Ral_ResetBindGroupArenaExact( arena, &first, &untouched )
		== ralErrorUnknown && resetCalls == 2u
		&& memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	CHECK( Ral_GetBindGroupArenaReceipt( arena, &second ) == ralSuccess
		&& Ral_BindGroupArenaReceiptExact( &first, &second ) );
	resetResult = VK_SUCCESS;
	CHECK( Ral_ResetBindGroupArenaExact( arena, &first, &second ) == ralSuccess
		&& resetCalls == 3u && second.generation == 2u
		&& !Ral_BindGroupArenaReceiptExact( &first, &second ) );
	CHECK( !ralVk_BindGroupArenaLive( group ) );
	Ral_DestroyBindGroup( group );
	CHECK( freeCalls == 0u );
	arena->lifecycle.generation = UINT64_MAX - 1u;
	CHECK( Ral_GetBindGroupArenaReceipt( arena, &stale ) == ralSuccess );
	CHECK( Ral_ResetBindGroupArenaExact( arena, &stale, &untouched )
		== ralErrorInvalidArgument && resetCalls == 3u );
	arena->lifecycle.generation = second.generation;
	Ral_DestroyBindGroupArena( arena );
	CHECK( destroyCalls == 1u );

#define REJECT_CREATE(statement) do { \
	uint32_t before = createCalls; untouched = sentinel; statement; \
	CHECK( Ral_CreateBindGroupArena( &backend, &info, &untouched ) == NULL \
		&& createCalls == before \
		&& memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 ); \
} while ( 0 )
	REJECT_CREATE( info.maxGroups = 0u ); info.maxGroups = 200u;
	REJECT_CREATE( info.numEntries = 0u ); info.numEntries = 5u;
	REJECT_CREATE( entries[0].count = 0u ); entries[0].count = 100u;
	REJECT_CREATE( entries[0].dynamicOffset = qtrue ); entries[0].dynamicOffset = qfalse;
	REJECT_CREATE( entries[4] = entries[1] );
	entries[4] = (ralBindGroupArenaEntry_t){ RAL_BIND_UNIFORM_BUFFER, 40u, qfalse };
#undef REJECT_CREATE
	otherBackend = backend;
	otherBackend.vk.ResetDescriptorPool = NULL;
	untouched = sentinel;
	CHECK( Ral_CreateBindGroupArena( &otherBackend, &info, &untouched ) == NULL
		&& createCalls == 1u
		&& memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	createResult = VK_ERROR_OUT_OF_HOST_MEMORY;
	untouched = sentinel;
	CHECK( Ral_CreateBindGroupArena( &backend, &info, &untouched ) == NULL
		&& createCalls == 2u
		&& memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	return 0;
}
