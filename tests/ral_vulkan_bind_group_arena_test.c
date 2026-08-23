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
static uint32_t capturedWriteBinding, capturedWriteElement;
static uint32_t capturedDescriptorCount;
static VkSampler capturedFirstSampler, capturedLastSampler;
static uint32_t createLayoutCalls, destroyLayoutCalls;
static VkResult createLayoutResult = VK_SUCCESS;
static VkDescriptorSetLayoutCreateInfo capturedLayoutInfo;
static VkDescriptorSetLayoutBinding capturedLayoutBindings[4];

static VKAPI_ATTR VkResult VKAPI_CALL CaptureCreateDescriptorSetLayout(
		VkDevice device, const VkDescriptorSetLayoutCreateInfo *info,
		const VkAllocationCallbacks *allocator, VkDescriptorSetLayout *layout ) {
	uint32_t i;
	(void)device; (void)allocator;
	createLayoutCalls++;
	if ( !info || !layout || info->bindingCount > 4u )
		return VK_ERROR_INITIALIZATION_FAILED;
	capturedLayoutInfo = *info;
	for ( i = 0u; i < info->bindingCount; ++i )
		capturedLayoutBindings[i] = info->pBindings[i];
	capturedLayoutInfo.pBindings = capturedLayoutBindings;
	if ( createLayoutResult == VK_SUCCESS )
		*layout = (VkDescriptorSetLayout)(uintptr_t)0x88u;
	return createLayoutResult;
}

static VKAPI_ATTR void VKAPI_CALL CaptureDestroyDescriptorSetLayout(
		VkDevice device, VkDescriptorSetLayout layout,
		const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator;
	if ( layout == (VkDescriptorSetLayout)(uintptr_t)0x88u )
		destroyLayoutCalls++;
}

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
	if ( writeCount == 1u && writes ) {
		capturedWriteType = writes[0].descriptorType;
		capturedWriteBinding = writes[0].dstBinding;
		capturedWriteElement = writes[0].dstArrayElement;
		capturedDescriptorCount = writes[0].descriptorCount;
		if ( writes[0].pImageInfo && writes[0].descriptorCount > 0u ) {
			capturedFirstSampler = writes[0].pImageInfo[0].sampler;
			capturedLastSampler = writes[0].pImageInfo[writes[0].descriptorCount - 1u].sampler;
		}
	}
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
	ralBindGroup_t imageGroup;
	ralBindGroupLayout_t imageLayout;
	ralTexture_t imageTexture;
	ralTextureView_t imageView;
	ralSampler_t imageSampler;
	ralTextureView_t arrayViews[3];
	const ralTextureView_t *arrayViewPtrs[3];
	ralSampler_t arraySampler;
	ralBindGroupLayout_t arrayLayout;
	ralBindGroup_t *arrayGroup;
	ralBindEntry_t effectEntries[4];
	ralBindGroupLayoutCreateInfo_t effectInfo;
	ralBindGroupLayout_t *effectLayout;

	memset( &backend, 0, sizeof( backend ) );
	memset( &otherBackend, 0, sizeof( otherBackend ) );
	backend.device = (VkDevice)(uintptr_t)0x10u;
	backend.vk.CreateDescriptorPool = CaptureCreateDescriptorPool;
	backend.vk.ResetDescriptorPool = CaptureResetDescriptorPool;
	backend.vk.DestroyDescriptorPool = CaptureDestroyDescriptorPool;
	backend.vk.AllocateDescriptorSets = CaptureAllocateDescriptorSets;
	backend.vk.UpdateDescriptorSets = CaptureUpdateDescriptorSets;
	backend.vk.FreeDescriptorSets = CaptureFreeDescriptorSets;
	backend.vk.CreateDescriptorSetLayout = CaptureCreateDescriptorSetLayout;
	backend.vk.DestroyDescriptorSetLayout = CaptureDestroyDescriptorSetLayout;
	backend.descriptorPool = (VkDescriptorPool)(uintptr_t)0x77u;
	memset( effectEntries, 0, sizeof( effectEntries ) );
	effectEntries[0] = (ralBindEntry_t){ 0u, RAL_BIND_STORAGE_BUFFER, 1u,
		RAL_STAGE_VERTEX, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED, qfalse };
	effectEntries[1] = (ralBindEntry_t){ 1u, RAL_BIND_STORAGE_BUFFER, 1u,
		RAL_STAGE_VERTEX, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED, qfalse };
	effectEntries[2] = (ralBindEntry_t){ 2u, RAL_BIND_TEXTURE_ARRAY, 64u,
		RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_2D, qfalse };
	effectEntries[3] = (ralBindEntry_t){ 3u, RAL_BIND_SAMPLER, 1u,
		RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED, qfalse };
	effectInfo = (ralBindGroupLayoutCreateInfo_t){ effectEntries, 4u, qfalse,
		"effect-layout-fixture" };
	effectLayout = Ral_CreateBindGroupLayout( &backend, &effectInfo );
	CHECK( effectLayout != NULL && createLayoutCalls == 1u
		&& effectLayout->ownsLayout == qtrue
		&& effectLayout->backend == &backend
		&& Ral_GetBindGroupLayoutHandle( effectLayout )
			== (void *)(uintptr_t)0x88u );
	CHECK( capturedLayoutInfo.sType
			== VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
		&& capturedLayoutInfo.bindingCount == 4u
		&& capturedLayoutBindings[0].descriptorType
			== VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		&& capturedLayoutBindings[0].stageFlags == VK_SHADER_STAGE_VERTEX_BIT
		&& capturedLayoutBindings[1].descriptorType
			== VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		&& capturedLayoutBindings[2].descriptorType
			== VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
		&& capturedLayoutBindings[2].descriptorCount == 64u
		&& capturedLayoutBindings[2].stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT
		&& capturedLayoutBindings[3].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER );
	Ral_DestroyBindGroupLayout( effectLayout );
	CHECK( destroyLayoutCalls == 1u );
	effectEntries[1].binding = 0u;
	CHECK( Ral_CreateBindGroupLayout( &backend, &effectInfo ) == NULL
		&& createLayoutCalls == 1u && destroyLayoutCalls == 1u );
	effectEntries[1].binding = 1u;
	effectEntries[2].dynamicOffset = qtrue;
	CHECK( Ral_CreateBindGroupLayout( &backend, &effectInfo ) == NULL
		&& createLayoutCalls == 1u && destroyLayoutCalls == 1u );
	effectEntries[2].dynamicOffset = qfalse;
	createLayoutResult = VK_ERROR_OUT_OF_HOST_MEMORY;
	CHECK( Ral_CreateBindGroupLayout( &backend, &effectInfo ) == NULL
		&& createLayoutCalls == 2u && destroyLayoutCalls == 1u );
	createLayoutResult = VK_SUCCESS;
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
	memset( &imageLayout, 0, sizeof( imageLayout ) );
	imageLayout.backend = &backend;
	imageLayout.numEntries = 3u;
	imageLayout.entries[0].binding = 0u;
	imageLayout.entries[0].vkType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	imageLayout.entries[0].effectiveCount = 4096u;
	imageLayout.entries[1].binding = 2u;
	imageLayout.entries[1].vkType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	imageLayout.entries[1].effectiveCount = 256u;
	imageLayout.entries[2].binding = 1u;
	imageLayout.entries[2].vkType = VK_DESCRIPTOR_TYPE_SAMPLER;
	imageLayout.entries[2].effectiveCount = 32u;
	memset( &imageGroup, 0, sizeof( imageGroup ) );
	imageGroup.backend = &backend;
	imageGroup.layout = &imageLayout;
	imageGroup.set = (VkDescriptorSet)(uintptr_t)0x68u;
	imageGroup.arena = arena;
	imageGroup.arenaReceipt = first;
	memset( &imageView, 0, sizeof( imageView ) );
	imageView.backend = &backend;
	imageView.view = (VkImageView)(uintptr_t)0x69u;
	memset( &imageTexture, 0, sizeof( imageTexture ) );
	imageTexture.backend = &backend;
	imageTexture.defaultView = imageView.view;
	memset( &imageSampler, 0, sizeof( imageSampler ) );
	imageSampler.backend = &backend;
	imageSampler.sampler = (VkSampler)(uintptr_t)0x6au;
	CHECK( Ral_BindGroupSetTextureViewAtBinding( &imageGroup, 2u, 17u, &imageView )
		&& updateCalls == 2u && capturedWriteBinding == 2u
		&& capturedWriteElement == 17u
		&& capturedWriteType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
	CHECK( !Ral_BindGroupSetTextureViewAtBinding( &imageGroup, 1u, 17u, &imageView )
		&& !Ral_BindGroupSetTextureViewAtBinding( &imageGroup, 2u, 256u, &imageView )
		&& updateCalls == 2u );
	CHECK( Ral_BindGroupSetTextureViewAt( &imageGroup, 4095u, &imageView )
		&& updateCalls == 3u && capturedWriteBinding == 0u
		&& capturedWriteElement == 4095u );
	CHECK( Ral_BindGroupSetTextureAt( &imageGroup, 23u, &imageTexture )
		&& updateCalls == 4u && capturedWriteBinding == 0u
		&& capturedWriteElement == 23u );
	CHECK( Ral_BindGroupSetSamplerAt( &imageGroup, 31u, &imageSampler )
		&& updateCalls == 5u && capturedWriteBinding == 1u
		&& capturedWriteElement == 31u
		&& capturedWriteType == VK_DESCRIPTOR_TYPE_SAMPLER );
	CHECK( Ral_BindGroupSetTextureAt( &imageGroup, 24u, NULL )
		&& Ral_BindGroupSetSamplerAt( &imageGroup, 24u, NULL )
		&& updateCalls == 5u );
	imageView.backend = &otherBackend;
	imageTexture.backend = &otherBackend;
	imageSampler.backend = &otherBackend;
	CHECK( !Ral_BindGroupSetTextureViewAt( &imageGroup, 0u, &imageView )
		&& !Ral_BindGroupSetTextureAt( &imageGroup, 0u, &imageTexture )
		&& !Ral_BindGroupSetSamplerAt( &imageGroup, 0u, &imageSampler )
		&& !Ral_BindGroupSetTextureViewAt( &imageGroup, 4096u, NULL )
		&& !Ral_BindGroupSetTextureAt( &imageGroup, 4096u, NULL )
		&& !Ral_BindGroupSetSamplerAt( &imageGroup, 32u, NULL )
		&& updateCalls == 5u );
	imageView.backend = &backend;
	imageTexture.backend = &backend;
	imageSampler.backend = &backend;
	memset( &arrayLayout, 0, sizeof( arrayLayout ) );
	arrayLayout.backend = &backend;
	arrayLayout.layout = (VkDescriptorSetLayout)(uintptr_t)0x70u;
	arrayLayout.numEntries = 1u;
	arrayLayout.entries[0].binding = 3u;
	arrayLayout.entries[0].vkType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	arrayLayout.entries[0].count = 3u;
	arrayLayout.entries[0].effectiveCount = 3u;
	memset( arrayViews, 0, sizeof( arrayViews ) );
	for ( uint32_t arrayIndex = 0u; arrayIndex < 3u; ++arrayIndex ) {
		arrayViews[arrayIndex].backend = &backend;
		arrayViews[arrayIndex].view = (VkImageView)(uintptr_t)(0x71u + arrayIndex);
		arrayViewPtrs[arrayIndex] = &arrayViews[arrayIndex];
	}
	memset( &arraySampler, 0, sizeof( arraySampler ) );
	arraySampler.backend = &backend;
	arraySampler.sampler = (VkSampler)(uintptr_t)0x75u;
	memset( &value, 0, sizeof( value ) );
	value.binding = 3u;
	value.type = RAL_BIND_TEXTURE_ARRAY;
	value.textureArray = arrayViewPtrs;
	value.textureArrayCount = 3u;
	value.sampler = &arraySampler;
	groupInfo.layout = &arrayLayout;
	groupInfo.values = &value;
	groupInfo.numValues = 1u;
	groupInfo.arena = arena;
	groupInfo.arenaReceipt = &first;
	arrayGroup = Ral_CreateBindGroup( &backend, &groupInfo );
	CHECK( arrayGroup != NULL && updateCalls == 6u
		&& capturedWriteBinding == 3u && capturedDescriptorCount == 3u
		&& capturedWriteType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		&& capturedFirstSampler == arraySampler.sampler
		&& capturedLastSampler == arraySampler.sampler );
	Ral_DestroyBindGroup( arrayGroup );
	value.sampler = NULL;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 2u && updateCalls == 6u );
	value.sampler = &arraySampler;
	stale = first; stale.generation++;
	groupInfo.arenaReceipt = &stale;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 2u );
	groupInfo.arenaReceipt = NULL;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 2u );
	groupInfo.arena = NULL;
	groupInfo.arenaReceipt = &first;
	CHECK( Ral_CreateBindGroup( &backend, &groupInfo ) == NULL
		&& allocateCalls == 2u );

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
