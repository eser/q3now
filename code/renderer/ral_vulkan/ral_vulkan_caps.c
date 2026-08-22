// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_caps.c — fills ralBackend_s::caps after the VkDevice exists.
// Probes the extension set the brief calls out (descriptor
// indexing, dynamic rendering, timeline semaphores, fragment shading rate,
// swapchain colorspace) plus separate queue families, and copies the
// VkPhysicalDeviceLimits the renderer will need.

#include "ral_vulkan_internal.h"

static const char *ralVk_VendorName( uint32_t vendorId ) {
	switch ( vendorId ) {
		case 0x1002: return "Advanced Micro Devices, Inc.";
		case 0x106B: return "Apple Inc.";
		case 0x10DE: return "NVIDIA";
		case 0x14E4: return "Broadcom Inc.";
		case 0x1AE0: return "Google Inc.";
		case 0x8086: return "Intel Corporation";
		case VK_VENDOR_ID_MESA: return "MESA";
		default: return NULL;
	}
}

static ralAdapterType_t ralVk_AdapterType( VkPhysicalDeviceType type ) {
	switch ( type ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return RAL_ADAPTER_TYPE_INTEGRATED;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return RAL_ADAPTER_TYPE_DISCRETE;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return RAL_ADAPTER_TYPE_VIRTUAL;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return RAL_ADAPTER_TYPE_CPU;
		default: return RAL_ADAPTER_TYPE_UNKNOWN;
	}
}

static void ralVk_FillDriverIdentity( const VkPhysicalDeviceProperties *p,
		ralCaps_t *c ) {
	const char *vendorName = ralVk_VendorName( p->vendorID );
	c->adapterType = ralVk_AdapterType( p->deviceType );
	c->vendorId = p->vendorID;
	c->deviceId = p->deviceID;
	c->offscreenPresentation = p->vendorID == 0x10DE ? qfalse : qtrue;
	if ( vendorName ) {
		snprintf( c->vendorName, sizeof( c->vendorName ), "%s", vendorName );
	} else {
		snprintf( c->vendorName, sizeof( c->vendorName ),
			"VendorID: %04x", p->vendorID );
	}

	if ( p->vendorID == 0x10DE ) {
		c->driverVersionMajor = ( p->driverVersion >> 22 ) & 0x3FFu;
		c->driverVersionMinor = ( p->driverVersion >> 14 ) & 0x0FFu;
		c->driverVersionPatch = ( p->driverVersion >> 6 ) & 0x0FFu;
		c->driverVersionBuild = p->driverVersion & 0x03Fu;
		snprintf( c->driverVersion, sizeof( c->driverVersion ), "%u.%u.%u.%u",
			c->driverVersionMajor, c->driverVersionMinor,
			c->driverVersionPatch, c->driverVersionBuild );
#ifdef _WIN32
	} else if ( p->vendorID == 0x8086 ) {
		c->driverVersionMajor = p->driverVersion >> 14;
		c->driverVersionMinor = p->driverVersion & 0x3FFFu;
		snprintf( c->driverVersion, sizeof( c->driverVersion ), "%u.%u",
			c->driverVersionMajor, c->driverVersionMinor );
#endif
	} else {
		c->driverVersionMajor = p->driverVersion >> 22;
		c->driverVersionMinor = ( p->driverVersion >> 12 ) & 0x3FFu;
		c->driverVersionPatch = p->driverVersion & 0xFFFu;
		snprintf( c->driverVersion, sizeof( c->driverVersion ), "%u.%u.%u",
			c->driverVersionMajor, c->driverVersionMinor,
			c->driverVersionPatch );
	}
}

static void ralVk_FillMemoryCapacity(
		const VkPhysicalDeviceMemoryProperties *memory,
		ralCaps_t *caps ) {
	uint32_t i;
	for ( i = 0u; i < memory->memoryTypeCount; i++ ) {
		const VkMemoryPropertyFlags flags = memory->memoryTypes[i].propertyFlags;
		const uint32_t heapIndex = memory->memoryTypes[i].heapIndex;
		uint64_t bytes;
		if ( heapIndex >= memory->memoryHeapCount ) continue;
		bytes = (uint64_t)memory->memoryHeaps[heapIndex].size;
		if ( ( flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) != 0u
				&& bytes > caps->deviceLocalMemoryBytes ) {
			caps->deviceLocalMemoryBytes = bytes;
		}
		if ( ( flags & ( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
					| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) )
				== ( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
					| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT )
				&& bytes > caps->hostVisibleDeviceLocalMemoryBytes ) {
			caps->hostVisibleDeviceLocalMemoryBytes = bytes;
		}
	}
}
#include <string.h>

static qboolean ralVk_SampledFormat( ralBackend_t *b, VkFormat format ) {
	VkFormatProperties properties;
	memset(&properties,0,sizeof(properties));
	b->vk.GetPhysicalDeviceFormatProperties(b->physicalDevice,format,&properties);
	return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0
		? qtrue : qfalse;
}

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
	snprintf( c->deviceName, sizeof( c->deviceName ), "%s", b->physProps.deviceName );
	snprintf( c->apiVersion, sizeof( c->apiVersion ), "Vulkan %u.%u.%u",
	             VK_API_VERSION_MAJOR( apiV ), VK_API_VERSION_MINOR( apiV ), VK_API_VERSION_PATCH( apiV ) );
	ralVk_FillDriverIdentity( &b->physProps, c );
	ralVk_FillMemoryCapacity( &b->memProps, c );

	// ── limits straight from VkPhysicalDeviceLimits ────────────────────
	c->maxColorAttachments       = L->maxColorAttachments;
	c->maxComputeWorkgroupSize   = L->maxComputeWorkGroupInvocations;
	c->maxTextureDimension2D     = L->maxImageDimension2D;
	c->maxTextureDimension3D     = L->maxImageDimension3D;
	c->maxTextureArrayLayers     = L->maxImageArrayLayers;
	c->maxPushConstantSize       = L->maxPushConstantsSize;
	c->minUniformBufferAlignment = (uint64_t)L->minUniformBufferOffsetAlignment;
	c->minStorageBufferAlignment = (uint64_t)L->minStorageBufferOffsetAlignment;
	c->maxStorageBufferRange     = (uint64_t)L->maxStorageBufferRange;
	c->timestampPeriodNs         = L->timestampComputeAndGraphics
		? L->timestampPeriod : 0.0f;
	c->maxSamplerAnisotropy      = b->haveSamplerAnisotropy ? L->maxSamplerAnisotropy : 1.0f;
	c->maxSampledTexturesPerShaderStage = L->maxPerStageDescriptorSamplers;
	c->maxBindGroups             = L->maxBoundDescriptorSets;

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
		c->timelineSemaphores = b->haveTimelineSemaphore;   // enabled at device creation (required); ts/hasTimelineSemaphore are the support snapshot
		(void)hasTimelineSemaphore; (void)ts;
		c->dynamicRendering   = hasDynamicRendering  && dr.dynamicRendering  == VK_TRUE;
	}

	// variableRateShading reflects what was ENABLED at device creation
	// (b->haveFragmentShadingRate: extension enabled AND pipelineFragmentShadingRate
	// feature on), not merely extension presence — a visible-but-unenabled extension
	// would make a pipeline VRS state / vkCmdSetFragmentShadingRateKHR a validation
	// error. false on MoltenVK / unsupported HW.
	c->variableRateShading = b->haveFragmentShadingRate;
	(void)hasFragmentShadingRate;
	// depthClamp reflects what was ENABLED at device creation (b->haveDepthClamp:
	// the renderer requested it AND the device reported VkPhysicalDeviceFeatures.
	// depthClamp), not mere support — a pipeline depthClampEnable=VK_TRUE without
	// the enabled feature is a validation error (VUID-...-depthClampEnable-00782).
	// false on backends without native depth-clamp → renderer takes the projection
	// near-plane-shrink fallback.
	c->depthClamp          = b->haveDepthClamp;
	// wideLines / vertexFragmentStores / samplerAnisotropyEnabled reflect what was
	// ENABLED at device creation (b->have* flags), restored here after the memset
	// above. Without these restores they read false in the renderer regardless of
	// device support — the latent bug that silently disabled the halo/flare
	// occlusion-probe (vk_flares.c gates R_ClearFlares + the dot-pipeline on
	// vk.fragmentStores = caps.vertexFragmentStores).
	c->wideLines                = b->haveWideLines;
	c->vertexFragmentStores     = b->haveVertexFragmentStores;
	c->samplerAnisotropyEnabled = b->haveSamplerAnisotropy;
	c->independentBlend         = b->haveIndependentBlend;
	// drawIndirectCount reflects what was ENABLED at device
	// creation (b->haveDrawIndirectCount), not just version/extension presence
	// — extension visible but feature not enabled would still mean callers
	// can't legally use vkCmdDrawIndexedIndirectCount.
	c->drawIndirectCount   = b->haveDrawIndirectCount;
	(void)hasDrawIndirectCount;
	c->debugUtils          = b->haveDebugUtils;
	c->memoryBudget        = b->haveMemoryBudget;
	// instance ext present → colorspaces are *potentially* presentable; real
	// availability is per-surface and resolved later. Best effort here.
	c->hdr10Swapchain      = hasSwapchainColorspace;
	c->scRGBSwapchain      = hasSwapchainColorspace;

	c->asyncCompute  = ( b->computeFamily  != b->graphicsFamily ) ? qtrue : qfalse;
	c->asyncTransfer = ( b->transferFamily != b->graphicsFamily ) ? qtrue : qfalse;
	c->textureCompressionBC = ralVk_SampledFormat(b,VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
		&& ralVk_SampledFormat(b,VK_FORMAT_BC3_UNORM_BLOCK)
		&& ralVk_SampledFormat(b,VK_FORMAT_BC5_UNORM_BLOCK)
		&& ralVk_SampledFormat(b,VK_FORMAT_BC7_UNORM_BLOCK);
	c->textureCompressionASTC = ralVk_SampledFormat(b,VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
	c->textureCompressionETC2 = ralVk_SampledFormat(b,VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);

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
