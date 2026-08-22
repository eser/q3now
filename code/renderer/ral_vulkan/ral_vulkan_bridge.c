// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#define RAL_VK_SPIRV_MAGIC 0x07230203u

qboolean RalVulkan_CreateShaderModule( ralBackend_t *b,
		const uint8_t *bytes, uint32_t byteCount, void **outIdentity ) {
	VkShaderModuleCreateInfo info;
	ralVkLegacyShaderModuleNode_t *node;
	VkShaderModule candidate = VK_NULL_HANDLE;
	uint32_t magic = 0u;
	if ( !b || b->type != RAL_BACKEND_VULKAN || b->device == VK_NULL_HANDLE
			|| !b->vk.CreateShaderModule || !b->vk.DestroyShaderModule
			|| !bytes || byteCount < 20u || ( byteCount & 3u ) != 0u
			|| !outIdentity ) return qfalse;
	memcpy( &magic, bytes, sizeof( magic ) );
	if ( magic != RAL_VK_SPIRV_MAGIC ) return qfalse;
	node = (ralVkLegacyShaderModuleNode_t *)calloc( 1, sizeof( *node ) );
	if ( !node ) return qfalse;
	memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = byteCount;
	info.pCode = (const uint32_t *)(const void *)bytes;
	if ( b->vk.CreateShaderModule( b->device, &info, NULL, &candidate ) != VK_SUCCESS
			|| candidate == VK_NULL_HANDLE ) {
		if ( candidate != VK_NULL_HANDLE )
			b->vk.DestroyShaderModule( b->device, candidate, NULL );
		free( node );
		return qfalse;
	}
	node->module = candidate;
	node->next = b->legacyShaderModules;
	b->legacyShaderModules = node;
	*outIdentity = (void *)(uintptr_t)RAL_VK_H2U( candidate );
	return qtrue;
}

qboolean RalVulkan_DestroyShaderModule( ralBackend_t *b, void *identity ) {
	ralVkLegacyShaderModuleNode_t **link;
	VkShaderModule module = RAL_VK_U2H( VkShaderModule, (uint64_t)(uintptr_t)identity );
	if ( !b || b->type != RAL_BACKEND_VULKAN || b->device == VK_NULL_HANDLE
			|| !b->vk.DestroyShaderModule || module == VK_NULL_HANDLE ) return qfalse;
	for ( link = &b->legacyShaderModules; *link; link = &( *link )->next ) {
		if ( ( *link )->module == module ) {
			ralVkLegacyShaderModuleNode_t *owned = *link;
			*link = owned->next;
			b->vk.DestroyShaderModule( b->device, module, NULL );
			free( owned );
			return qtrue;
		}
	}
	return qfalse;
}

void ralVk_DestroyLegacyShaderModules( ralBackend_t *b ) {
	if ( !b ) return;
	while ( b->legacyShaderModules ) {
		ralVkLegacyShaderModuleNode_t *owned = b->legacyShaderModules;
		b->legacyShaderModules = owned->next;
		if ( b->device != VK_NULL_HANDLE && b->vk.DestroyShaderModule
				&& owned->module != VK_NULL_HANDLE )
			b->vk.DestroyShaderModule( b->device, owned->module, NULL );
		free( owned );
	}
}

qboolean RalVulkan_RecordLegacyImageTransition( ralBackend_t *b,
		void *commandIdentity, void *imageIdentity, uint32_t aspectMask,
		uint32_t oldLayout, uint32_t newLayout,
		uint32_t srcStageOverride, uint32_t dstStageOverride ) {
	VkImageMemoryBarrier barrier;
	VkCommandBuffer commandBuffer = (VkCommandBuffer)commandIdentity;
	VkImage image = RAL_VK_U2H( VkImage, (uint64_t)(uintptr_t)imageIdentity );
	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;
	const uint32_t supportedAspects = VK_IMAGE_ASPECT_COLOR_BIT
		| VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	if ( !b || b->type != RAL_BACKEND_VULKAN || b->device == VK_NULL_HANDLE
			|| !b->vk.CmdPipelineBarrier || commandBuffer == VK_NULL_HANDLE
			|| image == VK_NULL_HANDLE || aspectMask == 0u
			|| ( aspectMask & ~supportedAspects ) != 0u
			|| ( ( aspectMask & VK_IMAGE_ASPECT_COLOR_BIT ) != 0u
				&& ( aspectMask & ( VK_IMAGE_ASPECT_DEPTH_BIT
					| VK_IMAGE_ASPECT_STENCIL_BIT ) ) != 0u )
			|| dstStageOverride != 0u
			|| ( srcStageOverride != 0u
				&& srcStageOverride != VK_PIPELINE_STAGE_HOST_BIT ) ) return qfalse;

	memset( &barrier, 0, sizeof( barrier ) );
	switch ( (VkImageLayout)oldLayout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			srcStage = srcStageOverride != 0u
				? (VkPipelineStageFlags)srcStageOverride
				: VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			if ( srcStageOverride != 0u ) return qfalse;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			if ( srcStageOverride != 0u ) return qfalse;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			if ( srcStageOverride != 0u ) return qfalse;
			srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			if ( srcStageOverride != 0u ) return qfalse;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		default: return qfalse;
	}

	switch ( (VkImageLayout)newLayout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
				| VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default: return qfalse;
	}

	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = (VkImageLayout)oldLayout;
	barrier.newLayout = (VkImageLayout)newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = (VkImageAspectFlags)aspectMask;
	barrier.subresourceRange.baseMipLevel = 0u;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0u;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	b->vk.CmdPipelineBarrier( commandBuffer, srcStage, dstStage, 0u,
		0u, NULL, 0u, NULL, 1u, &barrier );
	return qtrue;
}

qboolean RalVulkan_SurfaceFormatToNative( ralSurfaceFormat_t format,
		uint32_t *outFormat, uint32_t *outColorSpace ) {
	VkFormat nativeFormat;
	if ( !outFormat || !outColorSpace ) return qfalse;
	nativeFormat = ralVk_TranslateFormat( format.format );
	if ( nativeFormat == VK_FORMAT_UNDEFINED ) return qfalse;
	*outFormat = (uint32_t)nativeFormat;
	*outColorSpace = (uint32_t)ralVk_TranslateColorSpace( format.colorSpace );
	return qtrue;
}

static qboolean ralVk_ObjectRoleToNative( ralVulkanObjectRole_t role,
		VkObjectType *outType ) {
	VkObjectType candidate;
	if ( !outType ) return qfalse;
	switch ( role ) {
		case RAL_VULKAN_OBJECT_ROLE_DEVICE: candidate = VK_OBJECT_TYPE_DEVICE; break;
		case RAL_VULKAN_OBJECT_ROLE_DEVICE_MEMORY: candidate = VK_OBJECT_TYPE_DEVICE_MEMORY; break;
		case RAL_VULKAN_OBJECT_ROLE_FENCE: candidate = VK_OBJECT_TYPE_FENCE; break;
		case RAL_VULKAN_OBJECT_ROLE_SEMAPHORE: candidate = VK_OBJECT_TYPE_SEMAPHORE; break;
		case RAL_VULKAN_OBJECT_ROLE_BUFFER: candidate = VK_OBJECT_TYPE_BUFFER; break;
		case RAL_VULKAN_OBJECT_ROLE_IMAGE: candidate = VK_OBJECT_TYPE_IMAGE; break;
		case RAL_VULKAN_OBJECT_ROLE_IMAGE_VIEW: candidate = VK_OBJECT_TYPE_IMAGE_VIEW; break;
		case RAL_VULKAN_OBJECT_ROLE_SHADER_MODULE: candidate = VK_OBJECT_TYPE_SHADER_MODULE; break;
		case RAL_VULKAN_OBJECT_ROLE_PIPELINE_LAYOUT: candidate = VK_OBJECT_TYPE_PIPELINE_LAYOUT; break;
		case RAL_VULKAN_OBJECT_ROLE_SAMPLER: candidate = VK_OBJECT_TYPE_SAMPLER; break;
		case RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET_LAYOUT: candidate = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT; break;
		case RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET: candidate = VK_OBJECT_TYPE_DESCRIPTOR_SET; break;
		case RAL_VULKAN_OBJECT_ROLE_COMMAND_BUFFER: candidate = VK_OBJECT_TYPE_COMMAND_BUFFER; break;
		default: return qfalse;
	}
	*outType = candidate;
	return qtrue;
}

qboolean RalVulkan_SetObjectName( ralBackend_t *b, uint64_t objectIdentity,
		ralVulkanObjectRole_t role, const char *debugName ) {
	VkDebugUtilsObjectNameInfoEXT info;
	VkObjectType nativeType;

	if ( !b || b->type != RAL_BACKEND_VULKAN || objectIdentity == 0u
			|| !debugName || debugName[0] == '\0'
			|| !ralVk_ObjectRoleToNative( role, &nativeType ) ) return qfalse;
	if ( !b->haveDebugUtils ) return qtrue;
	if ( !b->device || !b->vk.SetDebugUtilsObjectNameEXT ) return qfalse;

	memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	info.objectType = nativeType;
	info.objectHandle = objectIdentity;
	info.pObjectName = debugName;
	return b->vk.SetDebugUtilsObjectNameEXT( b->device, &info ) == VK_SUCCESS
		? qtrue : qfalse;
}
