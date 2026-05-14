// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_backend.c — Vulkan RAL backend: instance / physical-device /
// device / queue creation + teardown, the availability probe, and the
// "\ral_dump" developer command entry point. Phase 7.1 skeleton: no command
// buffers / pipelines / resources yet (those are stubs in the sibling TUs).
//
// This backend owns its own VkInstance / VkDevice / VkQueues — it never
// shares them with code/renderervk/. It loads its own Vulkan entry points via
// the engine's platform loader (ri.VK_GetInstanceProcAddr) and vkGetDeviceProcAddr.

#include "ral_vulkan_internal.h"

#include <string.h>

// ── extension-list lookup (declared in ral_vulkan_internal.h) ───────────
qboolean ralVk_HasExtension( const VkExtensionProperties *exts, uint32_t count, const char *name ) {
	uint32_t i;
	if ( !exts || !name ) return qfalse;
	for ( i = 0; i < count; i++ ) {
		if ( strcmp( exts[i].extensionName, name ) == 0 )
			return qtrue;
	}
	return qfalse;
}

// ── validation message sink ─────────────────────────────────────────────
static VKAPI_ATTR VkBool32 VKAPI_CALL ralVk_DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
		VkDebugUtilsMessageTypeFlagsEXT             types,
		const VkDebugUtilsMessengerCallbackDataEXT *data,
		void                                       *user ) {
	const char *msg = ( data && data->pMessage ) ? data->pMessage : "(null)";
	ri.Log( ( severity & ( VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) )
	            ? SEV_WARN : SEV_DEBUG,
	        "[RAL][VK] %s\n", msg );
	(void)types; (void)user;
	return VK_FALSE;   // don't abort the offending call
}

// ── teardown (shared by Ral_DestroyBackend and the failure paths) ───────
// Imported-mode (b->ownsHandles == qfalse): RAL still owns frame/resource/
// pipeline layer state allocated against the caller's VkDevice, so we still
// drain & shut those down. We do NOT call vkDestroyDevice/Instance — the
// caller (renderervk) owns them. We skip DeviceWaitIdle too: the caller will
// already have called it before its own shutdown, and double-call is fine
// but redundant; gating on ownsHandles makes the intent explicit.
static void ralVk_DestroyBackendInternal( ralBackend_t *b ) {
	if ( !b ) return;
	// Drain GPU work on this device before destroying our cmd pools / fences
	// / descriptor pool. Safe in both modes: the call is idempotent and
	// vkDeviceWaitIdle on a shared device only blocks until all of RAL's +
	// the renderer's submissions complete — the renderer will also issue
	// its own wait in vk_shutdown, but a redundant pair is cheap.
	if ( b->device != VK_NULL_HANDLE && b->vk.DeviceWaitIdle )
		b->vk.DeviceWaitIdle( b->device );
	ralVk_DrainPendingDestroy( b, ~0ull );   // drain everything still queued
	ralVk_ShutdownPipelineLayer( b );        // VkPipelineCache + layout cache (Phase 7.3c)
	ralVk_ShutdownResourceLayer( b );        // descriptor pool / live allocations
	ralVk_ShutdownFrameLayer( b );           // cmd pools / queue mutexes / frame fences / pending-destroy ring
	if ( b->ownsHandles ) {
		if ( b->device != VK_NULL_HANDLE && b->vk.DestroyDevice )
			b->vk.DestroyDevice( b->device, NULL );
		if ( b->debugMessenger != VK_NULL_HANDLE && b->vk.DestroyDebugUtilsMessengerEXT )
			b->vk.DestroyDebugUtilsMessengerEXT( b->instance, b->debugMessenger, NULL );
		if ( b->instance != VK_NULL_HANDLE && b->vk.DestroyInstance )
			b->vk.DestroyInstance( b->instance, NULL );
	}
	free( b );
}

// ── instance-level entry-point loader ───────────────────────────────────
#define RAL_GIPA( inst, name )  ri.VK_GetInstanceProcAddr( (inst), (name) )

static qboolean ralVk_LoadInstanceFuncs( ralBackend_t *b ) {
	#define LOAD_REQ( field, sym ) \
		b->vk.field = (PFN_##sym)RAL_GIPA( b->instance, #sym ); \
		if ( !b->vk.field ) { ri.Log( SEV_WARN, "[RAL] Vulkan: missing instance entry point %s\n", #sym ); return qfalse; }
	#define LOAD_OPT( field, sym ) \
		b->vk.field = (PFN_##sym)RAL_GIPA( b->instance, #sym );

	LOAD_REQ( DestroyInstance,                        vkDestroyInstance )
	LOAD_REQ( EnumeratePhysicalDevices,               vkEnumeratePhysicalDevices )
	LOAD_REQ( GetPhysicalDeviceProperties,            vkGetPhysicalDeviceProperties )
	LOAD_REQ( GetPhysicalDeviceProperties2,           vkGetPhysicalDeviceProperties2 )
	LOAD_REQ( GetPhysicalDeviceFeatures,              vkGetPhysicalDeviceFeatures )
	LOAD_REQ( GetPhysicalDeviceFeatures2,             vkGetPhysicalDeviceFeatures2 )
	LOAD_REQ( GetPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties )
	LOAD_REQ( GetPhysicalDeviceMemoryProperties,      vkGetPhysicalDeviceMemoryProperties )
	LOAD_REQ( GetPhysicalDeviceMemoryProperties2,     vkGetPhysicalDeviceMemoryProperties2 )
	LOAD_REQ( EnumerateDeviceExtensionProperties,     vkEnumerateDeviceExtensionProperties )
	LOAD_REQ( CreateDevice,                           vkCreateDevice )
	LOAD_REQ( GetDeviceProcAddr,                      vkGetDeviceProcAddr )
	LOAD_OPT( GetPhysicalDeviceFormatProperties,      vkGetPhysicalDeviceFormatProperties )
	LOAD_OPT( CreateDebugUtilsMessengerEXT,           vkCreateDebugUtilsMessengerEXT )
	LOAD_OPT( DestroyDebugUtilsMessengerEXT,          vkDestroyDebugUtilsMessengerEXT )
	LOAD_OPT( SetDebugUtilsObjectNameEXT,             vkSetDebugUtilsObjectNameEXT )
	LOAD_OPT( CmdBeginDebugUtilsLabelEXT,             vkCmdBeginDebugUtilsLabelEXT )
	LOAD_OPT( CmdEndDebugUtilsLabelEXT,               vkCmdEndDebugUtilsLabelEXT )
	return qtrue;
	#undef LOAD_REQ
	#undef LOAD_OPT
}

static qboolean ralVk_LoadDeviceFuncs( ralBackend_t *b ) {
	#define LOAD_DEV( field, sym ) \
		b->vk.field = (PFN_##sym)b->vk.GetDeviceProcAddr( b->device, #sym ); \
		if ( !b->vk.field ) { ri.Log( SEV_WARN, "[RAL] Vulkan: missing device entry point %s\n", #sym ); return qfalse; }
	#define LOAD_DEV_OPT( field, sym ) \
		b->vk.field = (PFN_##sym)b->vk.GetDeviceProcAddr( b->device, #sym );
	// core lifecycle
	LOAD_DEV( DestroyDevice,                  vkDestroyDevice )
	LOAD_DEV( GetDeviceQueue,                 vkGetDeviceQueue )
	LOAD_DEV( DeviceWaitIdle,                 vkDeviceWaitIdle )
	LOAD_DEV( QueueWaitIdle,                  vkQueueWaitIdle )     // Phase 7.4c-submit-followup-present-1: Ral_WaitQueueIdle body
	LOAD_DEV( QueueSubmit,                    vkQueueSubmit )
	LOAD_DEV( QueueSubmit2,                   vkQueueSubmit2 )      // synchronization2 (core 1.3)
	// memory
	LOAD_DEV( AllocateMemory,                 vkAllocateMemory )
	LOAD_DEV( FreeMemory,                     vkFreeMemory )
	LOAD_DEV( MapMemory,                      vkMapMemory )
	LOAD_DEV( UnmapMemory,                    vkUnmapMemory )
	LOAD_DEV( FlushMappedMemoryRanges,        vkFlushMappedMemoryRanges )
	// buffers / images / views / samplers
	LOAD_DEV( CreateBuffer,                   vkCreateBuffer )
	LOAD_DEV( DestroyBuffer,                  vkDestroyBuffer )
	LOAD_DEV( GetBufferMemoryRequirements,    vkGetBufferMemoryRequirements )
	LOAD_DEV( BindBufferMemory,               vkBindBufferMemory )
	LOAD_DEV( CreateImage,                    vkCreateImage )
	LOAD_DEV( DestroyImage,                   vkDestroyImage )
	LOAD_DEV( GetImageMemoryRequirements,     vkGetImageMemoryRequirements )
	LOAD_DEV( BindImageMemory,                vkBindImageMemory )
	LOAD_DEV( CreateImageView,                vkCreateImageView )
	LOAD_DEV( DestroyImageView,               vkDestroyImageView )
	LOAD_DEV( CreateSampler,                  vkCreateSampler )
	LOAD_DEV( DestroySampler,                 vkDestroySampler )
	// descriptors
	LOAD_DEV( CreateDescriptorSetLayout,      vkCreateDescriptorSetLayout )
	LOAD_DEV( DestroyDescriptorSetLayout,     vkDestroyDescriptorSetLayout )
	LOAD_DEV( CreateDescriptorPool,           vkCreateDescriptorPool )
	LOAD_DEV( DestroyDescriptorPool,          vkDestroyDescriptorPool )
	LOAD_DEV( AllocateDescriptorSets,         vkAllocateDescriptorSets )
	LOAD_DEV( FreeDescriptorSets,             vkFreeDescriptorSets )
	LOAD_DEV( UpdateDescriptorSets,           vkUpdateDescriptorSets )
	// commands
	LOAD_DEV( CreateCommandPool,              vkCreateCommandPool )
	LOAD_DEV( DestroyCommandPool,             vkDestroyCommandPool )
	LOAD_DEV( ResetCommandPool,               vkResetCommandPool )
	LOAD_DEV( AllocateCommandBuffers,         vkAllocateCommandBuffers )
	LOAD_DEV( FreeCommandBuffers,             vkFreeCommandBuffers )
	LOAD_DEV( BeginCommandBuffer,             vkBeginCommandBuffer )
	LOAD_DEV( EndCommandBuffer,               vkEndCommandBuffer )
	LOAD_DEV( ResetCommandBuffer,             vkResetCommandBuffer )
	LOAD_DEV( CmdCopyBuffer,                  vkCmdCopyBuffer )
	LOAD_DEV( CmdCopyBufferToImage,           vkCmdCopyBufferToImage )
	LOAD_DEV( CmdCopyImageToBuffer,           vkCmdCopyImageToBuffer )
	LOAD_DEV( CmdBlitImage,                   vkCmdBlitImage )
	LOAD_DEV( CmdPipelineBarrier,             vkCmdPipelineBarrier )
	LOAD_DEV( CmdSetViewport,                 vkCmdSetViewport )
	LOAD_DEV( CmdSetScissor,                  vkCmdSetScissor )
	LOAD_DEV( CmdSetDepthBias,                vkCmdSetDepthBias )
	LOAD_DEV( CmdWriteTimestamp2,             vkCmdWriteTimestamp2 )   // synchronization2 (core 1.3)
	LOAD_DEV( CmdResetQueryPool,              vkCmdResetQueryPool )
	// fences
	LOAD_DEV( CreateFence,                    vkCreateFence )
	LOAD_DEV( DestroyFence,                   vkDestroyFence )
	LOAD_DEV( GetFenceStatus,                 vkGetFenceStatus )
	LOAD_DEV( WaitForFences,                  vkWaitForFences )
	LOAD_DEV( ResetFences,                    vkResetFences )
	// semaphores (binary + timeline; timeline ops core 1.2)
	LOAD_DEV( CreateSemaphore,                vkCreateSemaphore )
	LOAD_DEV( DestroySemaphore,               vkDestroySemaphore )
	LOAD_DEV( GetSemaphoreCounterValue,       vkGetSemaphoreCounterValue )
	LOAD_DEV( SignalSemaphore,                vkSignalSemaphore )
	LOAD_DEV( WaitSemaphores,                 vkWaitSemaphores )
	// query pools
	LOAD_DEV( CreateQueryPool,                vkCreateQueryPool )
	LOAD_DEV( DestroyQueryPool,               vkDestroyQueryPool )
	LOAD_DEV( ResetQueryPool,                 vkResetQueryPool )       // host-side reset (core 1.2)
	LOAD_DEV( GetQueryPoolResults,            vkGetQueryPoolResults )
	// pipelines (Phase 7.3c)
	LOAD_DEV( CreateShaderModule,             vkCreateShaderModule )
	LOAD_DEV( DestroyShaderModule,            vkDestroyShaderModule )
	LOAD_DEV( CreatePipelineLayout,           vkCreatePipelineLayout )
	LOAD_DEV( DestroyPipelineLayout,          vkDestroyPipelineLayout )
	LOAD_DEV( CreateGraphicsPipelines,        vkCreateGraphicsPipelines )
	LOAD_DEV( CreateComputePipelines,         vkCreateComputePipelines )
	LOAD_DEV( DestroyPipeline,                vkDestroyPipeline )
	LOAD_DEV( CreatePipelineCache,            vkCreatePipelineCache )
	LOAD_DEV( DestroyPipelineCache,           vkDestroyPipelineCache )
	LOAD_DEV( GetPipelineCacheData,           vkGetPipelineCacheData )
	// pipeline-dependent cmd ops (Phase 7.3c)
	LOAD_DEV( CmdBindPipeline,                vkCmdBindPipeline )
	LOAD_DEV( CmdBindDescriptorSets,          vkCmdBindDescriptorSets )
	LOAD_DEV( CmdBindVertexBuffers,           vkCmdBindVertexBuffers )
	LOAD_DEV( CmdBindIndexBuffer,             vkCmdBindIndexBuffer )
	LOAD_DEV( CmdPushConstants,               vkCmdPushConstants )
	LOAD_DEV( CmdDraw,                        vkCmdDraw )
	LOAD_DEV( CmdDrawIndexed,                 vkCmdDrawIndexed )
	LOAD_DEV( CmdDrawIndexedIndirect,         vkCmdDrawIndexedIndirect )
	LOAD_DEV_OPT( CmdDrawIndexedIndirectCount, vkCmdDrawIndexedIndirectCount )  // core 1.2 sym; usable iff drawIndirectCount feature enabled (caller-gated)
	LOAD_DEV( CmdDispatch,                    vkCmdDispatch )
	LOAD_DEV( CmdDispatchIndirect,            vkCmdDispatchIndirect )
	LOAD_DEV( CmdBeginRendering,              vkCmdBeginRendering )    // core 1.3 (dynamic rendering)
	LOAD_DEV( CmdEndRendering,                vkCmdEndRendering )

	// Phase 7.4c-cmd — Vk-typed parallel-paths cmd forwarders (see header).
	LOAD_DEV( CmdBeginRenderPass,             vkCmdBeginRenderPass )
	LOAD_DEV( CmdEndRenderPass,               vkCmdEndRenderPass )
	LOAD_DEV( CmdNextSubpass,                 vkCmdNextSubpass )
	LOAD_DEV( CmdCopyImage,                   vkCmdCopyImage )
	LOAD_DEV( CmdClearAttachments,            vkCmdClearAttachments )
	LOAD_DEV( CmdWriteTimestamp,              vkCmdWriteTimestamp )

	// Phase 7.4c-submit-followup-present-1 — swapchain + HDR-metadata function
	// pointers. VK_KHR_swapchain device extension already enabled by renderer
	// (imported-mode RAL inherits). VK_EXT_hdr_metadata is optional —
	// SetHdrMetadataEXT loaded via LOAD_DEV_OPT (NULL OK on drivers lacking
	// the extension; caller null-checks before invocation).
	LOAD_DEV( CreateSwapchainKHR,             vkCreateSwapchainKHR )
	LOAD_DEV( DestroySwapchainKHR,            vkDestroySwapchainKHR )
	LOAD_DEV( GetSwapchainImagesKHR,          vkGetSwapchainImagesKHR )
	LOAD_DEV( AcquireNextImageKHR,            vkAcquireNextImageKHR )
	LOAD_DEV( QueuePresentKHR,                vkQueuePresentKHR )
	LOAD_DEV_OPT( SetHdrMetadataEXT,          vkSetHdrMetadataEXT )   // VK_EXT_hdr_metadata; NULL when extension not enabled
	return qtrue;
	#undef LOAD_DEV
	#undef LOAD_DEV_OPT
}

// ════════════════════════════════════════════════════════════════════════
// Ral_CreateBackend
// ════════════════════════════════════════════════════════════════════════
ralBackend_t *Ral_CreateBackend( const ralBackendCreateInfo_t *ci ) {
	ralBackend_t *b;
	uint32_t      loaderVer = VK_API_VERSION_1_0;
	uint32_t      i;
	VkResult      r;

	if ( !ci ) return NULL;
	if ( ci->type != RAL_BACKEND_VULKAN ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: only RAL_BACKEND_VULKAN implemented in this build (requested %d)\n", (int)ci->type );
		return NULL;
	}
	if ( !ri.VK_GetInstanceProcAddr ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: platform Vulkan loader (ri.VK_GetInstanceProcAddr) unavailable\n" );
		return NULL;
	}

	b = (ralBackend_t *)malloc( sizeof( *b ) );
	if ( !b ) return NULL;
	memset( b, 0, sizeof( *b ) );
	b->type            = RAL_BACKEND_VULKAN;
	b->flags           = ci->flags;
	b->instance        = VK_NULL_HANDLE;
	b->physicalDevice  = VK_NULL_HANDLE;
	b->device          = VK_NULL_HANDLE;
	b->debugMessenger  = VK_NULL_HANDLE;
	b->graphicsFamily  = RAL_VK_INVALID_FAMILY;
	b->computeFamily   = RAL_VK_INVALID_FAMILY;
	b->transferFamily  = RAL_VK_INVALID_FAMILY;
	b->lastPressureLevel = RAL_PRESSURE_NORMAL;
	b->ownsHandles     = qtrue;   // default: standalone mode owns instance/device; imported branch flips this to qfalse

	// ── global entry points ────────────────────────────────────────────
	b->vk.EnumerateInstanceVersion             = (PFN_vkEnumerateInstanceVersion)            RAL_GIPA( VK_NULL_HANDLE, "vkEnumerateInstanceVersion" );
	b->vk.EnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)RAL_GIPA( VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties" );
	b->vk.CreateInstance                       = (PFN_vkCreateInstance)                      RAL_GIPA( VK_NULL_HANDLE, "vkCreateInstance" );
	if ( !b->vk.CreateInstance || !b->vk.EnumerateInstanceExtensionProperties ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: Vulkan loader did not yield vkCreateInstance\n" );
		free( b );
		return NULL;
	}
	if ( b->vk.EnumerateInstanceVersion )
		b->vk.EnumerateInstanceVersion( &loaderVer );
	if ( VK_API_VERSION_MAJOR( loaderVer ) == 1 && VK_API_VERSION_MINOR( loaderVer ) < 1 ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: Vulkan loader reports %u.%u; RAL requires 1.1+\n",
		        VK_API_VERSION_MAJOR( loaderVer ), VK_API_VERSION_MINOR( loaderVer ) );
		free( b );
		return NULL;
	}
	b->instanceApiVersion = loaderVer;

	// ── Phase 7.4c-pre: imported-mode branch ────────────────────────────
	// Caller created its own VkInstance/VkPhysicalDevice/VkDevice (renderervk
	// owns vk.instance / vk.device after vk_initialize) and hands them to RAL
	// here. Skip our own instance/device/queue creation; load entry points
	// against the adopted handles + re-query physical-device features so caps
	// flags (haveSync2 / haveDescriptorIndexing / …) reflect what the caller
	// actually enabled.
	if ( ci->externalInstance != NULL ) {
		VkPhysicalDeviceFeatures2                 f2q;
		VkPhysicalDeviceVulkan12Features          v12s;
		VkPhysicalDeviceSynchronization2Features  s2s;
		VkPhysicalDeviceDynamicRenderingFeatures  drs;

		b->ownsHandles    = qfalse;
		b->instance       = (VkInstance)        ci->externalInstance;
		b->physicalDevice = (VkPhysicalDevice)  ci->externalPhysicalDevice;
		b->device         = (VkDevice)          ci->externalDevice;
		b->graphicsFamily = ci->externalQueueFamilies[ RAL_QUEUE_GRAPHICS ];
		b->computeFamily  = ci->externalQueueFamilies[ RAL_QUEUE_COMPUTE  ];
		b->transferFamily = ci->externalQueueFamilies[ RAL_QUEUE_TRANSFER ];
		if ( ci->externalApiVersion ) {
			b->instanceApiVersion = ci->externalApiVersion;
		}
		if ( b->instance == VK_NULL_HANDLE || b->physicalDevice == VK_NULL_HANDLE || b->device == VK_NULL_HANDLE ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend (imported): caller passed NULL instance/physicalDevice/device handle\n" );
			free( b );
			return NULL;
		}
		if ( !ralVk_LoadInstanceFuncs( b ) ) {
			free( b );
			return NULL;
		}
		// Caller may or may not have set up debug-utils on their instance; we
		// don't create our own messenger in imported mode (would double-fire).
		// Phase 7.4c-pipeline fix: infer haveDebugUtils from entry-point
		// availability — renderervk now enables VK_EXT_debug_utils
		// unconditionally so the function pointers resolve, and RAL pipeline
		// debug labels (via SetDebugUtilsObjectNameEXT etc.) work.
		b->haveDebugUtils = ( b->vk.CreateDebugUtilsMessengerEXT != NULL
		                  &&  b->vk.DestroyDebugUtilsMessengerEXT != NULL ) ? qtrue : qfalse;

		b->vk.GetPhysicalDeviceProperties( b->physicalDevice, &b->physProps );
		b->vk.GetPhysicalDeviceMemoryProperties( b->physicalDevice, &b->memProps );

		memset( &v12s, 0, sizeof( v12s ) ); v12s.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		memset( &s2s,  0, sizeof( s2s  ) ); s2s.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		memset( &drs,  0, sizeof( drs  ) ); drs.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		v12s.pNext = &s2s;  s2s.pNext = &drs;  drs.pNext = NULL;
		memset( &f2q, 0, sizeof( f2q ) );  f2q.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;  f2q.pNext = &v12s;
		b->vk.GetPhysicalDeviceFeatures2( b->physicalDevice, &f2q );

		if ( s2s.synchronization2 != VK_TRUE || v12s.timelineSemaphore != VK_TRUE || drs.dynamicRendering != VK_TRUE ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend (imported): caller's device lacks synchronization2 (%d) / timelineSemaphore (%d) / dynamicRendering (%d) — Wired requires a Vulkan 1.3-class GPU\n",
			        (int)s2s.synchronization2, (int)v12s.timelineSemaphore, (int)drs.dynamicRendering );
			free( b );
			return NULL;
		}
		b->haveSync2              = qtrue;
		b->haveTimelineSemaphore  = qtrue;
		b->haveSamplerAnisotropy  = ( f2q.features.samplerAnisotropy == VK_TRUE ) ? qtrue : qfalse;
		b->haveHostQueryReset     = ( v12s.hostQueryReset == VK_TRUE ) ? qtrue : qfalse;
		b->haveDrawIndirectCount  = ( v12s.drawIndirectCount == VK_TRUE ) ? qtrue : qfalse;
		b->haveDescriptorIndexing = ( v12s.shaderSampledImageArrayNonUniformIndexing == VK_TRUE
		                           && v12s.runtimeDescriptorArray                    == VK_TRUE
		                           && v12s.descriptorBindingPartiallyBound           == VK_TRUE
		                           && v12s.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE ) ? qtrue : qfalse;
		// Phase 7.4c-pipeline: renderervk now enables VK_EXT_memory_budget
		// when the device supports it. We can't know directly which device
		// extensions the caller enabled, so scan the device's supported set
		// — if it advertises the extension, renderervk's vk_create_device
		// will have enabled it. (Mismatch is benign: the worst case is
		// querying with the extension flag set when it wasn't enabled, which
		// just makes the budget chain return estimates instead of real
		// numbers.)
		{
			uint32_t nDevExt = 0;
			VkExtensionProperties *devExtProps = NULL;
			b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, NULL );
			if ( nDevExt ) {
				devExtProps = (VkExtensionProperties *)malloc( nDevExt * sizeof( *devExtProps ) );
				b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, devExtProps );
			}
			b->haveMemoryBudget = ralVk_HasExtension( devExtProps, nDevExt, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME );
			if ( devExtProps ) free( devExtProps );
		}

		if ( !ralVk_LoadDeviceFuncs( b ) ) {
			free( b );
			return NULL;
		}
		goto initSharedLayers;
	}

	// ── instance extensions / layers ───────────────────────────────────
	{
		uint32_t                nInstExt = 0;
		VkExtensionProperties  *instExtProps = NULL;
		qboolean                wantDebugUtils = ( b->flags & ( RAL_FLAG_VALIDATION | RAL_FLAG_DEBUG_LABELS ) ) != 0;
		const char             *enabledExts[4];
		uint32_t                nEnabledExts = 0;
		const char             *enabledLayers[2];
		uint32_t                nEnabledLayers = 0;
		VkApplicationInfo       ai;
		VkInstanceCreateInfo    ici;

		b->vk.EnumerateInstanceExtensionProperties( NULL, &nInstExt, NULL );
		if ( nInstExt ) {
			instExtProps = (VkExtensionProperties *)malloc( nInstExt * sizeof( *instExtProps ) );
			b->vk.EnumerateInstanceExtensionProperties( NULL, &nInstExt, instExtProps );
		}
		b->haveDebugUtils = ralVk_HasExtension( instExtProps, nInstExt, VK_EXT_DEBUG_UTILS_EXTENSION_NAME );

		if ( wantDebugUtils && b->haveDebugUtils )
			enabledExts[ nEnabledExts++ ] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		if ( ( b->flags & RAL_FLAG_VALIDATION ) )
			enabledLayers[ nEnabledLayers++ ] = "VK_LAYER_KHRONOS_validation";

		memset( &ai, 0, sizeof( ai ) );
		ai.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		ai.pApplicationName   = "Wired (RAL)";
		ai.applicationVersion = 1;
		ai.pEngineName        = "Wired";
		ai.engineVersion      = 1;
		ai.apiVersion         = loaderVer;

		memset( &ici, 0, sizeof( ici ) );
		ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		ici.pApplicationInfo        = &ai;
		ici.enabledLayerCount       = nEnabledLayers;
		ici.ppEnabledLayerNames     = nEnabledLayers ? enabledLayers : NULL;
		ici.enabledExtensionCount   = nEnabledExts;
		ici.ppEnabledExtensionNames = nEnabledExts ? enabledExts : NULL;

		r = b->vk.CreateInstance( &ici, NULL, &b->instance );
		if ( r != VK_SUCCESS ) {
			// Retry with a bare instance (no layers, no extensions) — handles
			// missing validation layer / debug-utils gracefully.
			ri.Log( SEV_DEBUG, "[RAL] vkCreateInstance with layers/extensions failed (VkResult %d); retrying bare\n", (int)r );
			b->haveDebugUtils       = qfalse;
			ici.enabledLayerCount   = 0; ici.ppEnabledLayerNames     = NULL;
			ici.enabledExtensionCount = 0; ici.ppEnabledExtensionNames = NULL;
			r = b->vk.CreateInstance( &ici, NULL, &b->instance );
		}
		if ( instExtProps ) free( instExtProps );
		if ( r != VK_SUCCESS || b->instance == VK_NULL_HANDLE ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: vkCreateInstance failed (VkResult %d)\n", (int)r );
			free( b );
			return NULL;
		}
	}

	// ── instance-level entry points ────────────────────────────────────
	if ( !ralVk_LoadInstanceFuncs( b ) )
		goto fail;
	if ( !b->vk.CreateDebugUtilsMessengerEXT || !b->vk.DestroyDebugUtilsMessengerEXT )
		b->haveDebugUtils = qfalse;

	// ── validation messenger (best effort) ─────────────────────────────
	if ( b->haveDebugUtils && b->vk.CreateDebugUtilsMessengerEXT ) {
		VkDebugUtilsMessengerCreateInfoEXT dci;
		memset( &dci, 0, sizeof( dci ) );
		dci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dci.pfnUserCallback = ralVk_DebugCallback;
		dci.pUserData       = b;
		if ( b->vk.CreateDebugUtilsMessengerEXT( b->instance, &dci, NULL, &b->debugMessenger ) != VK_SUCCESS )
			b->debugMessenger = VK_NULL_HANDLE;   // non-fatal
	}

	// ── pick a physical device ─────────────────────────────────────────
	{
		uint32_t          nDev = 0;
		VkPhysicalDevice *devs;
		b->vk.EnumeratePhysicalDevices( b->instance, &nDev, NULL );
		if ( nDev == 0 ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: no Vulkan physical devices\n" );
			goto fail;
		}
		devs = (VkPhysicalDevice *)malloc( nDev * sizeof( *devs ) );
		b->vk.EnumeratePhysicalDevices( b->instance, &nDev, devs );
		b->physicalDevice = devs[0];
		for ( i = 0; i < nDev; i++ ) {
			VkPhysicalDeviceProperties p;
			b->vk.GetPhysicalDeviceProperties( devs[i], &p );
			if ( p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) { b->physicalDevice = devs[i]; break; }
		}
		free( devs );
	}
	b->vk.GetPhysicalDeviceProperties( b->physicalDevice, &b->physProps );
	b->vk.GetPhysicalDeviceMemoryProperties( b->physicalDevice, &b->memProps );

	// ── resolve queue families ─────────────────────────────────────────
	{
		uint32_t                 nq = 0;
		VkQueueFamilyProperties *qf;
		b->vk.GetPhysicalDeviceQueueFamilyProperties( b->physicalDevice, &nq, NULL );
		if ( nq == 0 ) { ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: device has no queue families\n" ); goto fail; }
		qf = (VkQueueFamilyProperties *)malloc( nq * sizeof( *qf ) );
		b->vk.GetPhysicalDeviceQueueFamilyProperties( b->physicalDevice, &nq, qf );
		for ( i = 0; i < nq; i++ ) {
			VkQueueFlags fl = qf[i].queueFlags;
			if ( ( fl & VK_QUEUE_GRAPHICS_BIT ) && b->graphicsFamily == RAL_VK_INVALID_FAMILY )
				b->graphicsFamily = i;
			if ( ( fl & VK_QUEUE_COMPUTE_BIT ) && !( fl & VK_QUEUE_GRAPHICS_BIT ) && b->computeFamily == RAL_VK_INVALID_FAMILY )
				b->computeFamily = i;   // dedicated async-compute family
			if ( ( fl & VK_QUEUE_TRANSFER_BIT ) && !( fl & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ) ) && b->transferFamily == RAL_VK_INVALID_FAMILY )
				b->transferFamily = i;  // dedicated DMA family
		}
		free( qf );
		if ( b->graphicsFamily == RAL_VK_INVALID_FAMILY ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: device has no graphics queue family\n" );
			goto fail;
		}
		if ( b->computeFamily  == RAL_VK_INVALID_FAMILY ) b->computeFamily  = b->graphicsFamily;
		if ( b->transferFamily == RAL_VK_INVALID_FAMILY ) b->transferFamily = b->graphicsFamily;
	}

	// ── enabled device extensions + features ───────────────────────────
	{
		uint32_t                nDevExt = 0;
		VkExtensionProperties  *devExtProps = NULL;
		const char             *enabledDevExts[4];
		uint32_t                nEnabledDevExts = 0;
		float                   prio = 1.0f;
		VkDeviceQueueCreateInfo qcis[3];
		uint32_t                nQci = 0;
		VkDeviceCreateInfo      dci;
		// Phase 7.4-pre: the 1.2-promoted features (descriptor indexing, timeline
		// semaphore, host query reset, drawIndirectCount, …) live in the
		// VkPhysicalDeviceVulkan12Features umbrella struct. The spec
		// (VUID-VkDeviceCreateInfo-pNext-02831) forbids passing the individual
		// VkPhysicalDeviceXxxFeatures structs alongside it, so we use the
		// umbrella exclusively for all 1.2 gates. Synchronization2 + dynamic
		// rendering are 1.3 features → still individual structs (until/unless
		// we adopt VkPhysicalDeviceVulkan13Features, which would force the same
		// consolidation for those).
		VkPhysicalDeviceFeatures2                  f2query, f2enable;
		VkPhysicalDeviceVulkan12Features           v12support, v12enable;
		VkPhysicalDeviceSynchronization2Features   s2Support, s2Enable;
		VkPhysicalDeviceDynamicRenderingFeatures   drSupport, drEnable;

		b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, NULL );
		if ( nDevExt ) {
			devExtProps = (VkExtensionProperties *)malloc( nDevExt * sizeof( *devExtProps ) );
			b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, devExtProps );
		}
		b->haveMemoryBudget = ralVk_HasExtension( devExtProps, nDevExt, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME );
		if ( b->haveMemoryBudget )
			enabledDevExts[ nEnabledDevExts++ ] = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;
		if ( devExtProps ) free( devExtProps );

		// ── query the feature support snapshot (one chained call) ──
		memset( &v12support, 0, sizeof( v12support ) ); v12support.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		memset( &s2Support,  0, sizeof( s2Support  ) ); s2Support.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		memset( &drSupport,  0, sizeof( drSupport  ) ); drSupport.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		v12support.pNext = &s2Support;  s2Support.pNext = &drSupport;  drSupport.pNext = NULL;
		memset( &f2query, 0, sizeof( f2query ) );  f2query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;  f2query.pNext = &v12support;
		b->vk.GetPhysicalDeviceFeatures2( b->physicalDevice, &f2query );

		// synchronization2 + timelineSemaphore + dynamicRendering are required by 7.3+:
		// vkQueueSubmit2, vkCmdWriteTimestamp2, timeline ops, vkCmdBeginRendering.
		if ( s2Support.synchronization2 != VK_TRUE || v12support.timelineSemaphore != VK_TRUE || drSupport.dynamicRendering != VK_TRUE ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: device lacks synchronization2 (%d) / timelineSemaphore (%d) / dynamicRendering (%d) — required since Phase 7.3\n",
			        (int)s2Support.synchronization2, (int)v12support.timelineSemaphore, (int)drSupport.dynamicRendering );
			goto fail;
		}
		b->haveSync2              = qtrue;
		b->haveTimelineSemaphore  = qtrue;
		b->haveSamplerAnisotropy  = ( f2query.features.samplerAnisotropy == VK_TRUE ) ? qtrue : qfalse;
		b->haveHostQueryReset     = ( v12support.hostQueryReset == VK_TRUE ) ? qtrue : qfalse;
		b->haveDrawIndirectCount  = ( v12support.drawIndirectCount == VK_TRUE ) ? qtrue : qfalse;
		b->haveDescriptorIndexing = ( v12support.shaderSampledImageArrayNonUniformIndexing == VK_TRUE
		                           && v12support.runtimeDescriptorArray                    == VK_TRUE
		                           && v12support.descriptorBindingPartiallyBound           == VK_TRUE
		                           && v12support.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE ) ? qtrue : qfalse;

		// ── build the enable chain: f2enable → v12enable → s2Enable → drEnable ──
		memset( &v12enable, 0, sizeof( v12enable ) ); v12enable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		memset( &s2Enable,  0, sizeof( s2Enable  ) ); s2Enable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		memset( &drEnable,  0, sizeof( drEnable  ) ); drEnable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		memset( &f2enable,  0, sizeof( f2enable  ) ); f2enable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		if ( b->haveDescriptorIndexing ) {
			v12enable.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
			v12enable.runtimeDescriptorArray                       = VK_TRUE;
			v12enable.descriptorBindingPartiallyBound              = VK_TRUE;
			v12enable.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
			v12enable.descriptorBindingUpdateUnusedWhilePending    = v12support.descriptorBindingUpdateUnusedWhilePending;
			v12enable.descriptorBindingVariableDescriptorCount     = v12support.descriptorBindingVariableDescriptorCount;
		}
		v12enable.timelineSemaphore = VK_TRUE;
		v12enable.hostQueryReset    = b->haveHostQueryReset    ? VK_TRUE : VK_FALSE;
		v12enable.drawIndirectCount = b->haveDrawIndirectCount ? VK_TRUE : VK_FALSE;   // Phase 7.4-pre
		s2Enable.synchronization2   = VK_TRUE;
		drEnable.dynamicRendering   = VK_TRUE;
		if ( b->haveSamplerAnisotropy ) f2enable.features.samplerAnisotropy = VK_TRUE;
		f2enable.pNext = &v12enable;  v12enable.pNext = &s2Enable;  s2Enable.pNext = &drEnable;  drEnable.pNext = NULL;

		// one queue per distinct family we resolved
		memset( qcis, 0, sizeof( qcis ) );
		qcis[ nQci ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qcis[ nQci ].queueFamilyIndex = b->graphicsFamily;
		qcis[ nQci ].queueCount       = 1;
		qcis[ nQci ].pQueuePriorities = &prio;
		nQci++;
		if ( b->computeFamily != b->graphicsFamily ) {
			qcis[ nQci ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qcis[ nQci ].queueFamilyIndex = b->computeFamily;
			qcis[ nQci ].queueCount       = 1;
			qcis[ nQci ].pQueuePriorities = &prio;
			nQci++;
		}
		if ( b->transferFamily != b->graphicsFamily && b->transferFamily != b->computeFamily ) {
			qcis[ nQci ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qcis[ nQci ].queueFamilyIndex = b->transferFamily;
			qcis[ nQci ].queueCount       = 1;
			qcis[ nQci ].pQueuePriorities = &prio;
			nQci++;
		}

		memset( &dci, 0, sizeof( dci ) );
		dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		dci.pNext                   = &f2enable;   // VkPhysicalDeviceFeatures2 chain (core + descriptor-indexing + timeline + sync2); pEnabledFeatures stays NULL
		dci.queueCreateInfoCount    = nQci;
		dci.pQueueCreateInfos       = qcis;
		dci.enabledExtensionCount   = nEnabledDevExts;
		dci.ppEnabledExtensionNames = nEnabledDevExts ? enabledDevExts : NULL;
		dci.pEnabledFeatures        = NULL;

		r = b->vk.CreateDevice( b->physicalDevice, &dci, NULL, &b->device );
		if ( r != VK_SUCCESS || b->device == VK_NULL_HANDLE ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: vkCreateDevice failed (VkResult %d)\n", (int)r );
			b->device = VK_NULL_HANDLE;
			goto fail;
		}
	}

	if ( !ralVk_LoadDeviceFuncs( b ) )
		goto fail;

initSharedLayers:
	// queues (compute/transfer alias graphics if no dedicated family)
	b->vk.GetDeviceQueue( b->device, b->graphicsFamily, 0, &b->graphicsQueue );
	b->computeQueue  = b->graphicsQueue;
	b->transferQueue = b->graphicsQueue;
	if ( b->computeFamily  != b->graphicsFamily ) b->vk.GetDeviceQueue( b->device, b->computeFamily,  0, &b->computeQueue );
	if ( b->transferFamily != b->graphicsFamily && b->transferFamily != b->computeFamily )
		b->vk.GetDeviceQueue( b->device, b->transferFamily, 0, &b->transferQueue );
	b->queues[ RAL_QUEUE_GRAPHICS ] = b->graphicsQueue;  b->queueFamily[ RAL_QUEUE_GRAPHICS ] = b->graphicsFamily;
	b->queues[ RAL_QUEUE_COMPUTE  ] = b->computeQueue;   b->queueFamily[ RAL_QUEUE_COMPUTE  ] = b->computeFamily;
	b->queues[ RAL_QUEUE_TRANSFER ] = b->transferQueue;  b->queueFamily[ RAL_QUEUE_TRANSFER ] = b->transferFamily;

	ralVk_FillCaps( b );

	if ( !ralVk_InitFrameLayer( b ) ) {     // per-queue cmd pools, queue mutexes, frame fences, deferred-destroy ring
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: frame layer init failed\n" );
		goto fail;
	}
	if ( !ralVk_InitResourceLayer( b ) ) {  // descriptor pool, format-blit cache
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: resource layer init failed\n" );
		goto fail;
	}
	if ( !ralVk_InitPipelineLayer( b ) ) {  // empty VkPipelineCache + layout cache (Phase 7.3c)
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBackend: pipeline layer init failed\n" );
		goto fail;
	}

	ri.Log( SEV_INFO, "[RAL] Vulkan backend ready: %s [%s] (queue families gfx/cmp/xfer = %u/%u/%u; debugUtils=%s memBudget=%s descriptorIndexing=%s sync2=%s timeline=%s drawIndirectCount=%s anisotropy=%.0fx)\n",
	        b->physProps.deviceName, b->caps.apiVersion,
	        b->graphicsFamily, b->computeFamily, b->transferFamily,
	        b->haveDebugUtils ? "yes" : "no", b->haveMemoryBudget ? "yes" : "no", b->haveDescriptorIndexing ? "yes" : "no",
	        b->haveSync2 ? "yes" : "no", b->haveTimelineSemaphore ? "yes" : "no", b->haveDrawIndirectCount ? "yes" : "no", (double)b->caps.maxSamplerAnisotropy );
	return b;

fail:
	ralVk_DestroyBackendInternal( b );
	return NULL;
}

#undef RAL_GIPA

// ════════════════════════════════════════════════════════════════════════
// Ral_DestroyBackend / Ral_GetCaps
// ════════════════════════════════════════════════════════════════════════
void Ral_DestroyBackend( ralBackend_t *b ) {
	if ( !b ) return;
	ralVk_StopPollThread( b );
	if ( b->device != VK_NULL_HANDLE && b->vk.DeviceWaitIdle )
		b->vk.DeviceWaitIdle( b->device );
	ralVk_DestroyBackendInternal( b );
}

const ralCaps_t *Ral_GetCaps( ralBackend_t *b ) {
	return b ? &b->caps : NULL;
}

// ════════════════════════════════════════════════════════════════════════
// Frame layer: per-queue command pools, queue mutexes, frame fences,
// deferred-destroy ring. (Phase 7.3)
// ════════════════════════════════════════════════════════════════════════
qboolean ralVk_InitFrameLayer( ralBackend_t *b ) {
	VkCommandPoolCreateInfo cpi;
	VkFenceCreateInfo       fi;
	uint32_t                q, i;

	if ( !ralVk_InitQueueMutexes( b ) ) {
		ri.Log( SEV_WARN, "[RAL] ralVk_InitFrameLayer: queue mutex creation failed\n" );
		return qfalse;
	}
	for ( q = 0; q < 3; q++ ) {
		RAL_ZERO( cpi );
		cpi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpi.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cpi.queueFamilyIndex = b->queueFamily[q];
		if ( b->vk.CreateCommandPool( b->device, &cpi, NULL, &b->cmdPools[q] ) != VK_SUCCESS ) {
			ri.Log( SEV_WARN, "[RAL] ralVk_InitFrameLayer: vkCreateCommandPool (queue %u) failed\n", q );
			return qfalse;
		}
	}
	RAL_ZERO( fi );
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // first Ral_BeginFrame must not block
	for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT; i++ )
		if ( b->vk.CreateFence( b->device, &fi, NULL, &b->frameFences[i] ) != VK_SUCCESS ) {
			ri.Log( SEV_WARN, "[RAL] ralVk_InitFrameLayer: frame fence %u creation failed\n", i );
			return qfalse;
		}
	b->pendingDestroy = (ralVkPendingDestroy_t *)malloc( RAL_VK_PENDING_DESTROY_MAX * sizeof( ralVkPendingDestroy_t ) );
	if ( !b->pendingDestroy ) return qfalse;
	memset( b->pendingDestroy, 0, RAL_VK_PENDING_DESTROY_MAX * sizeof( ralVkPendingDestroy_t ) );
	b->numPendingDestroy = 0;
	b->currentFrame      = 0;
	return qtrue;
}

void ralVk_ShutdownFrameLayer( ralBackend_t *b ) {
	uint32_t q, i;
	if ( b->device != VK_NULL_HANDLE ) {
		for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT; i++ )
			if ( b->frameFences[i] != VK_NULL_HANDLE ) { b->vk.DestroyFence( b->device, b->frameFences[i], NULL ); b->frameFences[i] = VK_NULL_HANDLE; }
		for ( q = 0; q < 3; q++ )
			if ( b->cmdPools[q] != VK_NULL_HANDLE ) { b->vk.DestroyCommandPool( b->device, b->cmdPools[q], NULL ); b->cmdPools[q] = VK_NULL_HANDLE; }
	}
	ralVk_DestroyQueueMutexes( b );
	if ( b->pendingDestroy ) { free( b->pendingDestroy ); b->pendingDestroy = NULL; b->numPendingDestroy = 0; }
}

// ── deferred destroy ────────────────────────────────────────────────────
static void ralVk_DoDestroyEntry( ralBackend_t *b, const ralVkPendingDestroy_t *e ) {
	switch ( e->kind ) {
	case RAL_RES_BUFFER:          b->vk.DestroyBuffer( b->device, RAL_VK_U2H( VkBuffer, e->h1 ), NULL ); break;
	case RAL_RES_IMAGE_AND_VIEW:  if ( e->h2 ) b->vk.DestroyImageView( b->device, RAL_VK_U2H( VkImageView, e->h2 ), NULL );
	                              b->vk.DestroyImage( b->device, RAL_VK_U2H( VkImage, e->h1 ), NULL ); break;
	case RAL_RES_IMAGE_VIEW:      b->vk.DestroyImageView( b->device, RAL_VK_U2H( VkImageView, e->h1 ), NULL ); break;
	case RAL_RES_SAMPLER:         b->vk.DestroySampler( b->device, RAL_VK_U2H( VkSampler, e->h1 ), NULL ); break;
	case RAL_RES_DESC_SET_LAYOUT: b->vk.DestroyDescriptorSetLayout( b->device, RAL_VK_U2H( VkDescriptorSetLayout, e->h1 ), NULL ); break;
	case RAL_RES_DESC_SET:        { VkDescriptorSet ds = RAL_VK_U2H( VkDescriptorSet, e->h1 ); if ( b->descriptorPool != VK_NULL_HANDLE ) b->vk.FreeDescriptorSets( b->device, b->descriptorPool, 1, &ds ); break; }
	case RAL_RES_FENCE:           b->vk.DestroyFence( b->device, RAL_VK_U2H( VkFence, e->h1 ), NULL ); break;
	case RAL_RES_SEMAPHORE:       b->vk.DestroySemaphore( b->device, RAL_VK_U2H( VkSemaphore, e->h1 ), NULL ); break;
	case RAL_RES_QUERY_POOL:      b->vk.DestroyQueryPool( b->device, RAL_VK_U2H( VkQueryPool, e->h1 ), NULL ); break;
	case RAL_RES_PIPELINE:        b->vk.DestroyPipeline( b->device, RAL_VK_U2H( VkPipeline, e->h1 ), NULL ); break;
	case RAL_RES_PIPELINE_LAYOUT: b->vk.DestroyPipelineLayout( b->device, RAL_VK_U2H( VkPipelineLayout, e->h1 ), NULL ); break;
	default: break;
	}
	if ( e->alloc ) ralVk_Free( b, e->alloc );
}

void ralVk_DrainPendingDestroy( ralBackend_t *b, uint64_t drainBeforeFrame ) {
	uint32_t i, kept = 0;
	if ( !b->pendingDestroy ) return;
	for ( i = 0; i < b->numPendingDestroy; i++ ) {
		ralVkPendingDestroy_t *e = &b->pendingDestroy[i];
		if ( e->destroyedAtFrame < drainBeforeFrame ) {
			ralVk_DoDestroyEntry( b, e );
		} else {
			if ( kept != i ) b->pendingDestroy[kept] = *e;
			kept++;
		}
	}
	b->numPendingDestroy = kept;
}

void ralVk_DeferDestroy( ralBackend_t *b, ralResourceKind_t kind, uint64_t h1, uint64_t h2, ralVkAllocation_t *alloc ) {
	ralVkPendingDestroy_t e;
	e.kind = kind; e.h1 = h1; e.h2 = h2; e.alloc = alloc;
	if ( !b->pendingDestroy ) {                 // frame layer not up — destroy now (drain the GPU first)
		if ( b->device != VK_NULL_HANDLE && b->vk.DeviceWaitIdle ) b->vk.DeviceWaitIdle( b->device );
		e.destroyedAtFrame = 0;
		ralVk_DoDestroyEntry( b, &e );
		return;
	}
	if ( b->numPendingDestroy >= RAL_VK_PENDING_DESTROY_MAX ) {   // ring full — force a full synchronous drain
		if ( b->vk.DeviceWaitIdle ) b->vk.DeviceWaitIdle( b->device );
		ralVk_DrainPendingDestroy( b, ~0ull );
	}
	e.destroyedAtFrame = b->currentFrame;
	b->pendingDestroy[ b->numPendingDestroy++ ] = e;
}

// ── per-frame lifecycle ─────────────────────────────────────────────────
void Ral_BeginFrame( ralBackend_t *b ) {
	uint32_t idx;
	if ( !b || !b->pendingDestroy ) return;
	b->currentFrame++;
	idx = (uint32_t)( b->currentFrame % RAL_VK_MAX_FRAMES_IN_FLIGHT );
	if ( b->frameFences[idx] != VK_NULL_HANDLE ) {
		b->vk.WaitForFences( b->device, 1, &b->frameFences[idx], VK_TRUE, ~0ull );
		b->vk.ResetFences( b->device, 1, &b->frameFences[idx] );
	}
	// drain destroys queued at or before (currentFrame - MAX_FRAMES_IN_FLIGHT)
	ralVk_DrainPendingDestroy( b, ( b->currentFrame > RAL_VK_MAX_FRAMES_IN_FLIGHT )
	                              ? ( b->currentFrame - RAL_VK_MAX_FRAMES_IN_FLIGHT + 1 ) : 1ull );
}

void Ral_EndFrame( ralBackend_t *b ) {
	uint32_t idx;
	VkSubmitInfo2 si2;
	if ( !b || !b->pendingDestroy ) return;
	idx = (uint32_t)( b->currentFrame % RAL_VK_MAX_FRAMES_IN_FLIGHT );
	// Empty submit on the graphics queue just to signal the frame fence (in a
	// full render loop the frame's real submission carries the fence; 7.3 has
	// no render loop, so this stand-in keeps the BeginFrame wait/reset cycle
	// from deadlocking). Full integration with swapchain present is 7.8b.
	RAL_ZERO( si2 );
	si2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	ralVk_QueueSubmit2( b, RAL_QUEUE_GRAPHICS, &si2, ( b->frameFences[idx] != VK_NULL_HANDLE ) ? b->frameFences[idx] : VK_NULL_HANDLE );
}

// ════════════════════════════════════════════════════════════════════════
// Ral_ProbeBackends — cheap availability check (instance + PD enumeration only)
// ════════════════════════════════════════════════════════════════════════
uint32_t Ral_ProbeBackends( ralBackendAvailability_t *out, uint32_t maxOut ) {
	static char nameBuf[64];
	static char devNameBuf[ VK_MAX_PHYSICAL_DEVICE_NAME_SIZE ];

	PFN_vkEnumerateInstanceVersion    pEIV;
	PFN_vkCreateInstance              pCI;
	PFN_vkDestroyInstance             pDI;
	PFN_vkEnumeratePhysicalDevices    pEPD;
	PFN_vkGetPhysicalDeviceProperties pGPP;
	uint32_t                          ver = VK_API_VERSION_1_0;
	uint32_t                          nDev = 0, i;
	VkApplicationInfo                 ai;
	VkInstanceCreateInfo              ici;
	VkInstance                        inst = VK_NULL_HANDLE;
	VkPhysicalDevice                 *devs, chosen;
	VkPhysicalDeviceProperties        p;

	if ( !out || maxOut < 1 ) return 0;
	memset( &out[0], 0, sizeof( out[0] ) );
	out[0].type = RAL_BACKEND_VULKAN;
	out[0].name = "Vulkan";

	if ( !ri.VK_GetInstanceProcAddr ) { out[0].reason = "Vulkan loader unavailable"; return 1; }

	pEIV = (PFN_vkEnumerateInstanceVersion)ri.VK_GetInstanceProcAddr( VK_NULL_HANDLE, "vkEnumerateInstanceVersion" );
	pCI  = (PFN_vkCreateInstance)          ri.VK_GetInstanceProcAddr( VK_NULL_HANDLE, "vkCreateInstance" );
	if ( !pCI ) { out[0].reason = "vkCreateInstance unavailable"; return 1; }
	if ( pEIV ) pEIV( &ver );
	Com_sprintf( nameBuf, sizeof( nameBuf ), "Vulkan %u.%u", VK_API_VERSION_MAJOR( ver ), VK_API_VERSION_MINOR( ver ) );
	out[0].name = nameBuf;
	if ( VK_API_VERSION_MAJOR( ver ) == 1 && VK_API_VERSION_MINOR( ver ) < 1 ) { out[0].reason = "requires Vulkan 1.1+"; return 1; }

	memset( &ai,  0, sizeof( ai  ) );  ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; ai.pEngineName = "Wired"; ai.apiVersion = ver;
	memset( &ici, 0, sizeof( ici ) );  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.pApplicationInfo = &ai;
	if ( pCI( &ici, NULL, &inst ) != VK_SUCCESS || inst == VK_NULL_HANDLE ) { out[0].reason = "vkCreateInstance failed"; return 1; }

	pDI  = (PFN_vkDestroyInstance)            ri.VK_GetInstanceProcAddr( inst, "vkDestroyInstance" );
	pEPD = (PFN_vkEnumeratePhysicalDevices)   ri.VK_GetInstanceProcAddr( inst, "vkEnumeratePhysicalDevices" );
	pGPP = (PFN_vkGetPhysicalDeviceProperties)ri.VK_GetInstanceProcAddr( inst, "vkGetPhysicalDeviceProperties" );
	if ( pEPD && pGPP ) pEPD( inst, &nDev, NULL );
	if ( nDev == 0 ) {
		if ( pDI ) pDI( inst, NULL );
		out[0].reason = "no Vulkan physical devices";
		return 1;
	}
	devs = (VkPhysicalDevice *)malloc( nDev * sizeof( *devs ) );
	pEPD( inst, &nDev, devs );
	chosen = devs[0];
	for ( i = 0; i < nDev; i++ ) {
		pGPP( devs[i], &p );
		if ( p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) { chosen = devs[i]; break; }
	}
	pGPP( chosen, &p );
	Q_strncpyz( devNameBuf, p.deviceName, sizeof( devNameBuf ) );
	free( devs );
	if ( pDI ) pDI( inst, NULL );

	out[0].available  = qtrue;
	out[0].deviceName = devNameBuf;
	out[0].reason     = NULL;
	return 1;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_Dump — "\ral_dump" developer command body. Exported from the renderer
// DLL; the client's ral_dump command resolves it with Sys_LoadFunction.
// ════════════════════════════════════════════════════════════════════════
Q_EXPORT void Ral_Dump( void ) {
#if !defined( FEAT_RAL ) || !FEAT_RAL
	if ( ri.Cvar_VariableIntegerValue( "developer" ) <= 0 ) {
		ri.Log( SEV_INFO, "ral_dump: needs 'developer 1' (or a FEAT_RAL build).\n" );
		return;
	}
#endif
	{
		ralBackendAvailability_t avail[4];
		ralBackendCreateInfo_t   ci;
		ralBackend_t            *b;
		const ralCaps_t         *c;
		ralMemoryBudget_t        mb;
		uint32_t                 n, i;

		ri.Log( SEV_INFO, "===== RAL dump (Phase 7.3c Vulkan: instance/device/resources/async/pipeline) =====\n" );

		n = Ral_ProbeBackends( avail, 4 );
		ri.Log( SEV_INFO, "Ral_ProbeBackends -> %u backend(s)\n", n );
		for ( i = 0; i < n; i++ ) {
			ri.Log( SEV_INFO, "  [%u] %-12s available=%-3s device=\"%s\"%s%s\n",
			        i, avail[i].name ? avail[i].name : "?",
			        avail[i].available ? "yes" : "no",
			        avail[i].deviceName ? avail[i].deviceName : "-",
			        avail[i].reason ? "  reason=" : "", avail[i].reason ? avail[i].reason : "" );
		}

		memset( &ci, 0, sizeof( ci ) );
		ci.type          = RAL_BACKEND_VULKAN;
		ci.platformHandle = NULL;                       // offscreen — no swapchain in 7.1
		ci.flags          = RAL_FLAG_DEBUG_LABELS | ( ri.Cvar_VariableIntegerValue( "developer" ) >= 2 ? RAL_FLAG_VALIDATION : 0u );
		b = Ral_CreateBackend( &ci );
		if ( !b ) {
			ri.Log( SEV_WARN, "Ral_CreateBackend failed (see [RAL] warnings above)\n" );
			ri.Log( SEV_INFO, "===== end RAL dump =====\n" );
			return;
		}

		c = Ral_GetCaps( b );
		ri.Log( SEV_INFO, "Ral_GetCaps:\n" );
		ri.Log( SEV_INFO, "  device                   : %s\n", c->deviceName );
		ri.Log( SEV_INFO, "  apiVersion               : %s\n", c->apiVersion );
		ri.Log( SEV_INFO, "  bindlessTextures         : %s (max %u)\n", c->bindlessTextures ? "yes" : "no", c->maxBindlessTextures );
		ri.Log( SEV_INFO, "  dynamicRendering         : %s\n", c->dynamicRendering ? "yes" : "no" );
		ri.Log( SEV_INFO, "  timelineSemaphores       : %s\n", c->timelineSemaphores ? "yes" : "no" );
		ri.Log( SEV_INFO, "  asyncCompute             : %s\n", c->asyncCompute ? "yes" : "no" );
		ri.Log( SEV_INFO, "  asyncTransfer            : %s\n", c->asyncTransfer ? "yes" : "no" );
		ri.Log( SEV_INFO, "  variableRateShading      : %s\n", c->variableRateShading ? "yes" : "no" );
		ri.Log( SEV_INFO, "  hdr10Swapchain           : %s\n", c->hdr10Swapchain ? "yes" : "no" );
		ri.Log( SEV_INFO, "  scRGBSwapchain           : %s\n", c->scRGBSwapchain ? "yes" : "no" );
		ri.Log( SEV_INFO, "  debugUtils               : %s\n", c->debugUtils ? "yes" : "no" );
		ri.Log( SEV_INFO, "  memoryBudget             : %s\n", c->memoryBudget ? "yes" : "no" );
		ri.Log( SEV_INFO, "  drawIndirectCount        : %s\n", c->drawIndirectCount ? "yes" : "no" );
		ri.Log( SEV_INFO, "  maxColorAttachments      : %u\n", c->maxColorAttachments );
		ri.Log( SEV_INFO, "  maxComputeWorkgroupSize  : %u\n", c->maxComputeWorkgroupSize );
		ri.Log( SEV_INFO, "  maxTextureDimension2D/3D : %u / %u\n", c->maxTextureDimension2D, c->maxTextureDimension3D );
		ri.Log( SEV_INFO, "  maxTextureArrayLayers    : %u\n", c->maxTextureArrayLayers );
		ri.Log( SEV_INFO, "  maxPushConstantSize      : %u bytes\n", c->maxPushConstantSize );
		ri.Log( SEV_INFO, "  minUBO / minSSBO align   : %u / %u bytes\n", (unsigned)c->minUniformBufferAlignment, (unsigned)c->minStorageBufferAlignment );
		ri.Log( SEV_INFO, "  timestampPeriod          : %.3f ns/tick\n", c->timestampPeriodNs );

		Ral_QueryMemoryBudget( b, &mb );
		ri.Log( SEV_INFO, "Ral_QueryMemoryBudget:\n" );
		ri.Log( SEV_INFO, "  device-local : %u / %u MiB used\n", (unsigned)( mb.deviceLocalUsed >> 20 ), (unsigned)( mb.deviceLocalBudget >> 20 ) );
		ri.Log( SEV_INFO, "  host-visible : %u / %u MiB used\n", (unsigned)( mb.hostVisibleUsed >> 20 ), (unsigned)( mb.hostVisibleBudget >> 20 ) );
		ri.Log( SEV_INFO, "  underPressure: %s\n", mb.underPressure ? "yes" : "no" );
		ri.Log( SEV_INFO, "  RAL footprint: %u KiB device-local / %u KiB host-visible across %u allocation(s)\n",
		        (unsigned)( b->ralDeviceLocalBytes >> 10 ), (unsigned)( b->ralHostVisibleBytes >> 10 ), b->numAllocations );

		// "\ral_dump <sub>" runs an extra exercise on the live backend:
		//   resource | test  → Phase 7.2 resource layer (texture/upload/mip-gen/bindless/budget/teardown)
		//   async            → Phase 7.3 queue/cmd/sync/query/deferred-destroy
		//   pipeline         → Phase 7.3c pipeline/cache/layout-cache/draw/dispatch
		//   all              → all of the above
		if ( ri.Cmd_Argc() > 1 ) {
			const char *sub = ri.Cmd_Argv( 1 );
			if ( Q_stricmp( sub, "resource" ) == 0 || Q_stricmp( sub, "test" ) == 0 )      ralVk_RunResourceTest( b );
			else if ( Q_stricmp( sub, "async" ) == 0 )                                     ralVk_RunAsyncTest( b );
			else if ( Q_stricmp( sub, "pipeline" ) == 0 )                                  ralVk_RunPipelineTest( b );
			else if ( Q_stricmp( sub, "all" ) == 0 )                                     { ralVk_RunResourceTest( b ); ralVk_RunAsyncTest( b ); ralVk_RunPipelineTest( b ); }
			else ri.Log( SEV_INFO, "  (unknown \\ral_dump subcommand \"%s\" — try: resource | async | pipeline | all)\n", sub );
		}

		Ral_DestroyBackend( b );
		ri.Log( SEV_INFO, "Ral_DestroyBackend: ok\n" );
		ri.Log( SEV_INFO, "===== end RAL dump =====\n" );
	}
}
