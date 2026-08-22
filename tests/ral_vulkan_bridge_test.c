// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static uint32_t nameCalls;
static VkObjectType capturedType;
static uint64_t capturedIdentity;
static char capturedName[64];
static VkResult nextResult = VK_SUCCESS;
static uint32_t shaderCreateCalls;
static uint32_t shaderDestroyCalls;
static size_t capturedShaderBytes;
static VkResult nextShaderCreateResult = VK_SUCCESS;
static uint32_t barrierCalls;
static VkCommandBuffer capturedCommandBuffer;
static VkPipelineStageFlags capturedSrcStage;
static VkPipelineStageFlags capturedDstStage;
static VkImageMemoryBarrier capturedImageBarrier;
static uint32_t barrierArgumentFailure;

VkFormat ralVk_TranslateFormat( ralFormat_t format ) {
	return format == RAL_FORMAT_B8G8R8A8_UNORM
		? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_UNDEFINED;
}

VkColorSpaceKHR ralVk_TranslateColorSpace( ralColorSpace_t colorSpace ) {
	return colorSpace == RAL_COLORSPACE_HDR10_ST2084
		? VK_COLOR_SPACE_HDR10_ST2084_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

static VKAPI_ATTR VkResult VKAPI_CALL FakeCreateShaderModule(
		VkDevice device, const VkShaderModuleCreateInfo *info,
		const VkAllocationCallbacks *allocator, VkShaderModule *outModule ) {
	(void)device; (void)allocator;
	shaderCreateCalls++;
	if ( !info || info->sType != VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
			|| !info->pCode ) return VK_ERROR_UNKNOWN;
	capturedShaderBytes = info->codeSize;
	if ( nextShaderCreateResult != VK_SUCCESS ) {
		*outModule = (VkShaderModule)(uintptr_t)0x7FFu;
		return nextShaderCreateResult;
	}
	*outModule = (VkShaderModule)(uintptr_t)( 0x700u + shaderCreateCalls );
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL FakeDestroyShaderModule(
		VkDevice device, VkShaderModule module,
		const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator;
	if ( module != VK_NULL_HANDLE ) shaderDestroyCalls++;
}

static VKAPI_ATTR void VKAPI_CALL FakeCmdPipelineBarrier(
		VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
		uint32_t memoryBarrierCount, const VkMemoryBarrier *memoryBarriers,
		uint32_t bufferMemoryBarrierCount,
		const VkBufferMemoryBarrier *bufferMemoryBarriers,
		uint32_t imageMemoryBarrierCount,
		const VkImageMemoryBarrier *imageMemoryBarriers ) {
	if ( dependencyFlags != 0u
			|| memoryBarrierCount != 0u || memoryBarriers != NULL
			|| bufferMemoryBarrierCount != 0u || bufferMemoryBarriers != NULL
			|| imageMemoryBarrierCount != 1u || imageMemoryBarriers == NULL ) {
		barrierArgumentFailure++;
		return;
	}
	barrierCalls++;
	capturedCommandBuffer = commandBuffer;
	capturedSrcStage = srcStageMask;
	capturedDstStage = dstStageMask;
	capturedImageBarrier = imageMemoryBarriers[0];
}

static VKAPI_ATTR VkResult VKAPI_CALL FakeSetDebugUtilsObjectNameEXT(
		VkDevice device, const VkDebugUtilsObjectNameInfoEXT *info ) {
	(void)device;
	if ( !info || info->sType != VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT
			|| !info->pObjectName ) return VK_ERROR_UNKNOWN;
	nameCalls++;
	capturedType = info->objectType;
	capturedIdentity = info->objectHandle;
	strncpy( capturedName, info->pObjectName, sizeof( capturedName ) - 1u );
	capturedName[ sizeof( capturedName ) - 1u ] = '\0';
	return nextResult;
}

int main( void ) {
	static const struct {
		ralVulkanObjectRole_t role;
		VkObjectType nativeType;
	} mappings[] = {
		{ RAL_VULKAN_OBJECT_ROLE_DEVICE, VK_OBJECT_TYPE_DEVICE },
		{ RAL_VULKAN_OBJECT_ROLE_DEVICE_MEMORY, VK_OBJECT_TYPE_DEVICE_MEMORY },
		{ RAL_VULKAN_OBJECT_ROLE_FENCE, VK_OBJECT_TYPE_FENCE },
		{ RAL_VULKAN_OBJECT_ROLE_SEMAPHORE, VK_OBJECT_TYPE_SEMAPHORE },
		{ RAL_VULKAN_OBJECT_ROLE_BUFFER, VK_OBJECT_TYPE_BUFFER },
		{ RAL_VULKAN_OBJECT_ROLE_IMAGE, VK_OBJECT_TYPE_IMAGE },
		{ RAL_VULKAN_OBJECT_ROLE_IMAGE_VIEW, VK_OBJECT_TYPE_IMAGE_VIEW },
		{ RAL_VULKAN_OBJECT_ROLE_SHADER_MODULE, VK_OBJECT_TYPE_SHADER_MODULE },
		{ RAL_VULKAN_OBJECT_ROLE_PIPELINE_LAYOUT, VK_OBJECT_TYPE_PIPELINE_LAYOUT },
		{ RAL_VULKAN_OBJECT_ROLE_SAMPLER, VK_OBJECT_TYPE_SAMPLER },
		{ RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET_LAYOUT, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT },
		{ RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET, VK_OBJECT_TYPE_DESCRIPTOR_SET },
		{ RAL_VULKAN_OBJECT_ROLE_COMMAND_BUFFER, VK_OBJECT_TYPE_COMMAND_BUFFER }
	};
	ralBackend_t backend;
	uint32_t i;

	memset( &backend, 0, sizeof( backend ) );
	backend.type = RAL_BACKEND_VULKAN;
	backend.device = (VkDevice)(uintptr_t)0x123u;
	backend.haveDebugUtils = qtrue;
	backend.vk.SetDebugUtilsObjectNameEXT = FakeSetDebugUtilsObjectNameEXT;
	backend.vk.CreateShaderModule = FakeCreateShaderModule;
	backend.vk.DestroyShaderModule = FakeDestroyShaderModule;
	backend.vk.CmdPipelineBarrier = FakeCmdPipelineBarrier;

	{
		void *commandIdentity = (void *)(uintptr_t)0x456u;
		void *imageIdentity = (void *)(uintptr_t)0x789u;
		PFN_vkCmdPipelineBarrier savedDispatch = backend.vk.CmdPipelineBarrier;
		VkDevice savedDevice = backend.device;
		CHECK( !RalVulkan_RecordLegacyImageTransition( NULL,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		backend.type = RAL_BACKEND_WEBGPU;
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		backend.type = RAL_BACKEND_VULKAN;
		backend.device = VK_NULL_HANDLE;
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		backend.device = savedDevice;
		backend.vk.CmdPipelineBarrier = NULL;
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		backend.vk.CmdPipelineBarrier = savedDispatch;
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			NULL, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, NULL, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, 0u,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity,
			VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED, 0u, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_HOST_BIT, 0u ) );
		CHECK( !RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, VK_PIPELINE_STAGE_TRANSFER_BIT ) );
		CHECK( barrierCalls == 0u );

		CHECK( RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_HOST_BIT, 0u ) );
		CHECK( barrierCalls == 1u && barrierArgumentFailure == 0u
			&& capturedCommandBuffer == (VkCommandBuffer)commandIdentity );
		CHECK( capturedSrcStage == VK_PIPELINE_STAGE_HOST_BIT
			&& capturedDstStage == VK_PIPELINE_STAGE_TRANSFER_BIT );
		CHECK( capturedImageBarrier.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER
			&& capturedImageBarrier.srcAccessMask == VK_ACCESS_NONE
			&& capturedImageBarrier.dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT
			&& capturedImageBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
			&& capturedImageBarrier.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			&& capturedImageBarrier.image == (VkImage)(uintptr_t)imageIdentity
			&& capturedImageBarrier.subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
			&& capturedImageBarrier.subresourceRange.levelCount == VK_REMAINING_MIP_LEVELS
			&& capturedImageBarrier.subresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS );

		CHECK( RalVulkan_RecordLegacyImageTransition( &backend,
			commandIdentity, imageIdentity, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0u, 0u ) );
		CHECK( barrierCalls == 2u
			&& capturedSrcStage == VK_PIPELINE_STAGE_TRANSFER_BIT
			&& capturedDstStage == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			&& capturedImageBarrier.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT
			&& capturedImageBarrier.dstAccessMask
				== ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ) );
	}

	{
		uint32_t spirv[5] = { 0x07230203u, 0x00010000u, 0u, 1u, 0u };
		uint32_t invalidSpirv[5] = { 0u, 0x00010000u, 0u, 1u, 0u };
		void *identity = (void *)(uintptr_t)0xBADu;
		CHECK( !RalVulkan_CreateShaderModule( &backend,
			(const uint8_t *)invalidSpirv, sizeof( invalidSpirv ), &identity ) );
		CHECK( identity == (void *)(uintptr_t)0xBADu && shaderCreateCalls == 0u );
		CHECK( !RalVulkan_CreateShaderModule( &backend,
			(const uint8_t *)spirv, sizeof( spirv ) - 1u, &identity ) );
		CHECK( identity == (void *)(uintptr_t)0xBADu && shaderCreateCalls == 0u );
		nextShaderCreateResult = VK_ERROR_DEVICE_LOST;
		CHECK( !RalVulkan_CreateShaderModule( &backend,
			(const uint8_t *)spirv, sizeof( spirv ), &identity ) );
		CHECK( identity == (void *)(uintptr_t)0xBADu && shaderCreateCalls == 1u
			&& shaderDestroyCalls == 1u );
		nextShaderCreateResult = VK_SUCCESS;
		CHECK( RalVulkan_CreateShaderModule( &backend,
			(const uint8_t *)spirv, sizeof( spirv ), &identity ) );
		CHECK( identity == (void *)(uintptr_t)0x702u
			&& capturedShaderBytes == sizeof( spirv )
			&& backend.legacyShaderModules != NULL );
		CHECK( !RalVulkan_DestroyShaderModule( &backend, (void *)(uintptr_t)0x999u ) );
		CHECK( shaderDestroyCalls == 1u );
		CHECK( RalVulkan_DestroyShaderModule( &backend, identity ) );
		CHECK( shaderDestroyCalls == 2u && backend.legacyShaderModules == NULL );
		CHECK( !RalVulkan_DestroyShaderModule( &backend, identity ) );
		CHECK( RalVulkan_CreateShaderModule( &backend,
			(const uint8_t *)spirv, sizeof( spirv ), &identity ) );
		ralVk_DestroyLegacyShaderModules( &backend );
		CHECK( shaderDestroyCalls == 3u && backend.legacyShaderModules == NULL );
	}

	for ( i = 0u; i < (uint32_t)( sizeof( mappings ) / sizeof( mappings[0] ) ); ++i ) {
		CHECK( RalVulkan_SetObjectName( &backend, 0x1000u + i,
			mappings[i].role, "wired-object" ) );
		CHECK( capturedType == mappings[i].nativeType );
		CHECK( capturedIdentity == 0x1000u + i );
		CHECK( strcmp( capturedName, "wired-object" ) == 0 );
	}
	CHECK( nameCalls == RAL_VULKAN_OBJECT_ROLE_COUNT );

	CHECK( !RalVulkan_SetObjectName( NULL, 1u,
		RAL_VULKAN_OBJECT_ROLE_BUFFER, "bad" ) );
	backend.type = RAL_BACKEND_WEBGPU;
	CHECK( !RalVulkan_SetObjectName( &backend, 1u,
		RAL_VULKAN_OBJECT_ROLE_BUFFER, "bad" ) );
	backend.type = RAL_BACKEND_VULKAN;
	CHECK( !RalVulkan_SetObjectName( &backend, 0u,
		RAL_VULKAN_OBJECT_ROLE_BUFFER, "bad" ) );
	CHECK( !RalVulkan_SetObjectName( &backend, 1u,
		RAL_VULKAN_OBJECT_ROLE_BUFFER, NULL ) );
	CHECK( !RalVulkan_SetObjectName( &backend, 1u,
		RAL_VULKAN_OBJECT_ROLE_BUFFER, "" ) );
	CHECK( !RalVulkan_SetObjectName( &backend, 1u,
		RAL_VULKAN_OBJECT_ROLE_COUNT, "bad" ) );
	CHECK( nameCalls == RAL_VULKAN_OBJECT_ROLE_COUNT );

	backend.haveDebugUtils = qfalse;
	backend.device = VK_NULL_HANDLE;
	backend.vk.SetDebugUtilsObjectNameEXT = NULL;
	CHECK( RalVulkan_SetObjectName( &backend, 2u,
		RAL_VULKAN_OBJECT_ROLE_IMAGE, "optional-noop" ) );
	CHECK( nameCalls == RAL_VULKAN_OBJECT_ROLE_COUNT );

	backend.haveDebugUtils = qtrue;
	CHECK( !RalVulkan_SetObjectName( &backend, 2u,
		RAL_VULKAN_OBJECT_ROLE_IMAGE, "missing-device" ) );
	backend.device = (VkDevice)(uintptr_t)0x123u;
	CHECK( !RalVulkan_SetObjectName( &backend, 2u,
		RAL_VULKAN_OBJECT_ROLE_IMAGE, "missing-dispatch" ) );
	backend.vk.SetDebugUtilsObjectNameEXT = FakeSetDebugUtilsObjectNameEXT;
	nextResult = VK_ERROR_DEVICE_LOST;
	CHECK( !RalVulkan_SetObjectName( &backend, 2u,
		RAL_VULKAN_OBJECT_ROLE_IMAGE, "dispatch-failure" ) );
	CHECK( nameCalls == RAL_VULKAN_OBJECT_ROLE_COUNT + 1u );

	puts( "ral_vulkan_bridge_test: PASS" );
	return 0;
}
