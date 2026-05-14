// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_caps.c — fills ralBackend_s::caps after the VkDevice exists.
// Probes the extension set the Phase 7.1 brief calls out (descriptor
// indexing, dynamic rendering, timeline semaphores, fragment shading rate,
// swapchain colorspace) plus separate queue families, and copies the
// VkPhysicalDeviceLimits the renderer will need.

#include "ral_vulkan_internal.h"
#include <string.h>

void ralVk_FillCaps( ralBackend_t *b ) {
	ralCaps_t                    *c = &b->caps;
	const VkPhysicalDeviceLimits *L = &b->physProps.limits;
	uint32_t                      apiV = b->physProps.apiVersion;
	uint32_t                      nDevExt = 0;
	VkExtensionProperties        *devExts = NULL;
	qboolean hasDescriptorIndexing, hasDynamicRendering, hasTimelineSemaphore;
	qboolean hasFragmentShadingRate, hasDrawIndirectCount, hasSwapchainColorspace;

	memset( c, 0, sizeof( *c ) );

	// ── identity ───────────────────────────────────────────────────────
	Q_strncpyz( c->deviceName, b->physProps.deviceName, sizeof( c->deviceName ) );
	Com_sprintf( c->apiVersion, sizeof( c->apiVersion ), "Vulkan %u.%u.%u",
	             VK_API_VERSION_MAJOR( apiV ), VK_API_VERSION_MINOR( apiV ), VK_API_VERSION_PATCH( apiV ) );

	// ── limits straight from VkPhysicalDeviceLimits ────────────────────
	c->maxColorAttachments       = L->maxColorAttachments;
	c->maxComputeWorkgroupSize   = L->maxComputeWorkGroupInvocations;
	c->maxTextureDimension2D     = L->maxImageDimension2D;
	c->maxTextureDimension3D     = L->maxImageDimension3D;
	c->maxTextureArrayLayers     = L->maxImageArrayLayers;
	c->maxPushConstantSize       = L->maxPushConstantsSize;
	c->minUniformBufferAlignment = (uint64_t)L->minUniformBufferOffsetAlignment;
	c->minStorageBufferAlignment = (uint64_t)L->minStorageBufferOffsetAlignment;
	c->timestampPeriodNs         = L->timestampPeriod;
	c->maxSamplerAnisotropy      = b->haveSamplerAnisotropy ? L->maxSamplerAnisotropy : 1.0f;

	// ── device extensions (re-enumerated; cheap) ───────────────────────
	b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, NULL );
	if ( nDevExt ) {
		devExts = (VkExtensionProperties *)malloc( nDevExt * sizeof( *devExts ) );
		b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, devExts );
	}
	hasDescriptorIndexing  = ( apiV >= VK_API_VERSION_1_2 ) || ralVk_HasExtension( devExts, nDevExt, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME );
	hasDynamicRendering    = ( apiV >= VK_API_VERSION_1_3 ) || ralVk_HasExtension( devExts, nDevExt, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME );
	hasTimelineSemaphore   = ( apiV >= VK_API_VERSION_1_2 ) || ralVk_HasExtension( devExts, nDevExt, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME );
	hasFragmentShadingRate = ralVk_HasExtension( devExts, nDevExt, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME );
	hasDrawIndirectCount   = ( apiV >= VK_API_VERSION_1_2 ) || ralVk_HasExtension( devExts, nDevExt, VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME );
	if ( devExts ) free( devExts );

	// ── instance extension: VK_EXT_swapchain_colorspace ────────────────
	hasSwapchainColorspace = qfalse;
	if ( b->vk.EnumerateInstanceExtensionProperties ) {
		uint32_t               nInstExt = 0;
		VkExtensionProperties *instExts = NULL;
		b->vk.EnumerateInstanceExtensionProperties( NULL, &nInstExt, NULL );
		if ( nInstExt ) {
			instExts = (VkExtensionProperties *)malloc( nInstExt * sizeof( *instExts ) );
			b->vk.EnumerateInstanceExtensionProperties( NULL, &nInstExt, instExts );
			hasSwapchainColorspace = ralVk_HasExtension( instExts, nInstExt, "VK_EXT_swapchain_colorspace" );
			free( instExts );
		}
	}

	// ── confirm feature support, not just extension presence ───────────
	{
		VkPhysicalDeviceDescriptorIndexingFeatures di;
		VkPhysicalDeviceTimelineSemaphoreFeatures  ts;
		VkPhysicalDeviceDynamicRenderingFeatures   dr;
		VkPhysicalDeviceFeatures2                  f2;
		void                                      *chain = NULL;

		memset( &di, 0, sizeof( di ) ); di.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		memset( &ts, 0, sizeof( ts ) ); ts.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
		memset( &dr, 0, sizeof( dr ) ); dr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		memset( &f2, 0, sizeof( f2 ) ); f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		if ( hasDescriptorIndexing ) { di.pNext = chain; chain = &di; }
		if ( hasTimelineSemaphore  ) { ts.pNext = chain; chain = &ts; }
		if ( hasDynamicRendering   ) { dr.pNext = chain; chain = &dr; }
		f2.pNext = chain;
		b->vk.GetPhysicalDeviceFeatures2( b->physicalDevice, &f2 );

		// bindlessTextures reflects what was *enabled* at device creation
		// (see Ral_CreateBackend / b->haveDescriptorIndexing), not merely
		// what the device supports — using the binding flags requires the
		// features to be enabled. (`di` is still queried above for the
		// support snapshot; the gate is the enabled flag.)
		(void)di; (void)hasDescriptorIndexing;
		c->bindlessTextures   = b->haveDescriptorIndexing;
		c->timelineSemaphores = b->haveTimelineSemaphore;   // enabled at device creation (required since 7.3); ts/hasTimelineSemaphore are the support snapshot
		(void)hasTimelineSemaphore; (void)ts;
		c->dynamicRendering   = hasDynamicRendering  && dr.dynamicRendering  == VK_TRUE;
	}

	c->variableRateShading = hasFragmentShadingRate;
	// Phase 7.4-pre: drawIndirectCount reflects what was ENABLED at device
	// creation (b->haveDrawIndirectCount), not just version/extension presence
	// — extension visible but feature not enabled would still mean callers
	// can't legally use vkCmdDrawIndexedIndirectCount.
	c->drawIndirectCount   = b->haveDrawIndirectCount;
	(void)hasDrawIndirectCount;
	c->debugUtils          = b->haveDebugUtils;
	c->memoryBudget        = b->haveMemoryBudget;
	// instance ext present → colorspaces are *potentially* presentable; real
	// availability is per-surface and resolved in Phase 7.8b. Best effort here.
	c->hdr10Swapchain      = hasSwapchainColorspace;
	c->scRGBSwapchain      = hasSwapchainColorspace;

	c->asyncCompute  = ( b->computeFamily  != b->graphicsFamily ) ? qtrue : qfalse;
	c->asyncTransfer = ( b->transferFamily != b->graphicsFamily ) ? qtrue : qfalse;

	// ── bindless texture-table size ────────────────────────────────────
	if ( c->bindlessTextures ) {
		VkPhysicalDeviceDescriptorIndexingProperties dip;
		VkPhysicalDeviceProperties2                  p2;
		memset( &dip, 0, sizeof( dip ) ); dip.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
		memset( &p2,  0, sizeof( p2  ) ); p2.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2; p2.pNext = &dip;
		b->vk.GetPhysicalDeviceProperties2( b->physicalDevice, &p2 );
		c->maxBindlessTextures = dip.maxDescriptorSetUpdateAfterBindSampledImages;
	} else {
		c->maxBindlessTextures = L->maxPerStageDescriptorSampledImages;
	}
	if ( c->maxBindlessTextures > ( 1u << 20 ) )   // clamp to something a renderer would actually allocate
		c->maxBindlessTextures = ( 1u << 20 );
	if ( c->maxBindlessTextures == 0 )
		c->maxBindlessTextures = L->maxPerStageDescriptorSampledImages;
}
