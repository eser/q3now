// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_backend.c — Vulkan RAL backend: instance / physical-device /
// device / queue creation + teardown, the availability probe, and the
// "\ral_dump" developer command entry point. Skeleton: no command
// buffers / pipelines / resources yet (those are stubs in the sibling TUs).
//
// This backend owns its own VkInstance / VkDevice / VkQueues — it never
// shares them with code/renderervk/. It loads its own Vulkan entry points via
// the engine's platform loader (ri.VK_GetInstanceProcAddr) and vkGetDeviceProcAddr.

#include "ral_vulkan_internal.h"

R_LOG_DECLARE_CHANNEL( rch_ral, "renderer.ral" );

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
	R_LOG( rch_ral,( severity & ( VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) )
	            ? SEV_WARN : SEV_DEBUG,
	        "%s\n", msg );
	(void)types; (void)user;
	return VK_FALSE;   // don't abort the offending call
}

// ── teardown (shared by Ral_DestroyBackend and the failure paths) ───────
// Fine-grained ownership: ownsDevice gates device
// teardown, ownsInstance gates instance + messenger + surface teardown.
// Surface destroy MUST run before instance destroy (uses instance handle).
// Imported-mode (both flags qfalse): RAL still owns frame/resource/pipeline
// layer state allocated against the caller's VkDevice, so it still drains
// + shuts those down.
static void ralVk_DestroyBackendInternal( ralBackend_t *b ) {
	if ( !b ) return;
	// Drain GPU work on this device before destroying cmd pools / fences /
	// descriptor pool. Idempotent and cheap.
	if ( b->device != VK_NULL_HANDLE && b->vk.DeviceWaitIdle )
		b->vk.DeviceWaitIdle( b->device );
	ralVk_DrainPendingDestroy( b, ~0ull );   // drain everything still queued
	ralVk_ShutdownPipelineLayer( b );        // VkPipelineCache + layout cache
	ralVk_ShutdownResourceLayer( b );        // descriptor pool / live allocations
	ralVk_ShutdownFrameLayer( b );           // cmd pools / queue mutexes / frame fences / pending-destroy ring
	if ( b->ownsDevice ) {
		if ( b->device != VK_NULL_HANDLE && b->vk.DestroyDevice )
			b->vk.DestroyDevice( b->device, NULL );
	}
	if ( b->ownsInstance ) {
		if ( b->surface != VK_NULL_HANDLE && b->vk.DestroySurfaceKHR )
			b->vk.DestroySurfaceKHR( b->instance, b->surface, NULL );
		if ( b->debugMessenger != VK_NULL_HANDLE && b->vk.DestroyDebugUtilsMessengerEXT )
			b->vk.DestroyDebugUtilsMessengerEXT( b->instance, b->debugMessenger, NULL );
		if ( b->instance != VK_NULL_HANDLE && b->vk.DestroyInstance )
			b->vk.DestroyInstance( b->instance, NULL );
	}
	// Free the enabled-ext pointer array (strings
	// themselves are caller-owned literals; we only free our pointer table).
	if ( b->enabledDeviceExtensions ) {
		free( (void *)b->enabledDeviceExtensions );
		b->enabledDeviceExtensions     = NULL;
		b->enabledDeviceExtensionCount = 0;
	}
	free( b );
}

// ── handle accessors ─────────────────────
void *Ral_GetInstanceHandle      ( const ralBackend_t *b ) { return b ? (void *)b->instance       : NULL; }
void *Ral_GetPhysicalDeviceHandle( const ralBackend_t *b ) { return b ? (void *)b->physicalDevice : NULL; }
void *Ral_GetSurfaceHandle       ( const ralBackend_t *b ) { return b ? (void *)b->surface        : NULL; }
void *Ral_GetDeviceHandle        ( const ralBackend_t *b ) { return b ? (void *)b->device         : NULL; }

void *Ral_GetQueueHandle( const ralBackend_t *b, ralQueueType_t q ) {
	if ( !b ) return NULL;
	switch ( q ) {
		case RAL_QUEUE_GRAPHICS: return (void *)b->graphicsQueue;
		case RAL_QUEUE_COMPUTE:  return (void *)b->computeQueue;
		case RAL_QUEUE_TRANSFER: return (void *)b->transferQueue;
	}
	return NULL;
}

uint32_t Ral_GetQueueFamily( const ralBackend_t *b, ralQueueType_t q ) {
	if ( !b ) return UINT32_MAX;
	switch ( q ) {
		case RAL_QUEUE_GRAPHICS: return b->graphicsFamily;
		case RAL_QUEUE_COMPUTE:  return b->computeFamily;
		case RAL_QUEUE_TRANSFER: return b->transferFamily;
	}
	return UINT32_MAX;
}

void Ral_GetEnabledDeviceExtensions( const ralBackend_t *b,
                                     const char *const **out,
                                     uint32_t           *outCount ) {
	if ( out      ) *out      = b ? (const char *const *)b->enabledDeviceExtensions : NULL;
	if ( outCount ) *outCount = b ? b->enabledDeviceExtensionCount : 0u;
}

// ── instance-level entry-point loader ───────────────────────────────────
#define RAL_GIPA( inst, name )  ri.VK_GetInstanceProcAddr( (inst), (name) )

static qboolean ralVk_LoadInstanceFuncs( ralBackend_t *b ) {
	#define LOAD_REQ( field, sym ) \
		b->vk.field = (PFN_##sym)RAL_GIPA( b->instance, #sym ); \
		if ( !b->vk.field ) { R_LOG( rch_ral, SEV_WARN, "Vulkan: missing instance entry point %s\n", #sym ); return qfalse; }
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
	// Surface entry points are optional for a deliberately offscreen backend.
	// The owned present-capable path validates both before creating its surface;
	// imported/offscreen diagnostics must not fail merely because the instance
	// did not enable VK_KHR_surface.
	LOAD_OPT( DestroySurfaceKHR,                      vkDestroySurfaceKHR )
	LOAD_OPT( GetPhysicalDeviceSurfaceSupportKHR,     vkGetPhysicalDeviceSurfaceSupportKHR )
	return qtrue;
	#undef LOAD_REQ
	#undef LOAD_OPT
}

static qboolean ralVk_LoadDeviceFuncs( ralBackend_t *b ) {
	#define LOAD_DEV( field, sym ) \
		b->vk.field = (PFN_##sym)b->vk.GetDeviceProcAddr( b->device, #sym ); \
		if ( !b->vk.field ) { R_LOG( rch_ral, SEV_WARN, "Vulkan: missing device entry point %s\n", #sym ); return qfalse; }
	#define LOAD_DEV_OPT( field, sym ) \
		b->vk.field = (PFN_##sym)b->vk.GetDeviceProcAddr( b->device, #sym );
	// core lifecycle
	LOAD_DEV( DestroyDevice,                  vkDestroyDevice )
	LOAD_DEV( GetDeviceQueue,                 vkGetDeviceQueue )
	LOAD_DEV( DeviceWaitIdle,                 vkDeviceWaitIdle )
	LOAD_DEV( QueueWaitIdle,                  vkQueueWaitIdle )     // Ral_WaitQueueIdle body
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
	// pipelines
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
	// pipeline-dependent cmd ops
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

	// Vk-typed parallel-paths cmd forwarders (see header).
	LOAD_DEV( CmdCopyImage,                   vkCmdCopyImage )
	LOAD_DEV( CmdClearAttachments,            vkCmdClearAttachments )
	LOAD_DEV( CmdWriteTimestamp,              vkCmdWriteTimestamp )

	// swapchain + HDR-metadata function
	// pointers. VK_KHR_swapchain device extension already enabled by renderer
	// (imported-mode RAL inherits). VK_EXT_hdr_metadata is optional —
	// SetHdrMetadataEXT loaded via LOAD_DEV_OPT (NULL OK on drivers lacking
	// the extension; caller null-checks before invocation).
	//
	// Guard the swapchain function group only for a backend that actually owns
	// a surface. vkGetDeviceProcAddr returns
	// NULL for these when VK_KHR_swapchain was NOT enabled on the device
	// (owned-device path HARD-REQUIRES it; imported mode inherits the renderer's
	// enable). Load them as OPT and, if ANY is NULL, decline with a PRECISE
	// reason (device cannot present) instead of either the generic "missing
	// device entry point" warning OR — worse — storing a NULL PFN that a later
	// swapchain create/acquire/present would deref. This makes the no-swapchain
	// case a graceful backend-creation decline (caller's R_DeclineInit-style
	// fallback), never a null call.
	LOAD_DEV_OPT( CreateSwapchainKHR,         vkCreateSwapchainKHR )
	LOAD_DEV_OPT( DestroySwapchainKHR,        vkDestroySwapchainKHR )
	LOAD_DEV_OPT( GetSwapchainImagesKHR,      vkGetSwapchainImagesKHR )
	LOAD_DEV_OPT( AcquireNextImageKHR,        vkAcquireNextImageKHR )
	LOAD_DEV_OPT( QueuePresentKHR,            vkQueuePresentKHR )
	if ( b->surface != VK_NULL_HANDLE
	  && ( !b->vk.CreateSwapchainKHR || !b->vk.DestroySwapchainKHR || !b->vk.GetSwapchainImagesKHR
	  || !b->vk.AcquireNextImageKHR || !b->vk.QueuePresentKHR ) ) {
		R_LOG( rch_ral, SEV_WARN, "Vulkan: VK_KHR_swapchain device functions unavailable (extension not enabled on this device) — device cannot present\n" );
		return qfalse;
	}
	LOAD_DEV_OPT( SetHdrMetadataEXT,          vkSetHdrMetadataEXT )   // VK_EXT_hdr_metadata; NULL when extension not enabled
	return qtrue;
	#undef LOAD_DEV
	#undef LOAD_DEV_OPT
}

// multi-device retry predicate. Returns qtrue iff `pd` advertises at
// least one queue family that supports BOTH graphics and presentation to
// b->surface. This is the same predicate the legacy vk.c multi-device retry
// loop (origin/main vk.c:2286-2295) used to skip compute-only / non-present
// adapters enumerated ahead of the real GPU on hybrid systems. b->instance
// + b->surface must be valid (they are by the time the device picker runs).
static qboolean ralVk_DeviceCanPresent( ralBackend_t *b, VkPhysicalDevice pd ) {
	VkQueueFamilyProperties *qfp;
	uint32_t                 nqf = 0, i;
	qboolean                 ok = qfalse;

	if ( pd == VK_NULL_HANDLE || b->surface == VK_NULL_HANDLE )
		return qfalse;

	b->vk.GetPhysicalDeviceQueueFamilyProperties( pd, &nqf, NULL );
	if ( nqf == 0 )
		return qfalse;
	qfp = (VkQueueFamilyProperties *)malloc( nqf * sizeof( *qfp ) );
	if ( !qfp )
		return qfalse;
	b->vk.GetPhysicalDeviceQueueFamilyProperties( pd, &nqf, qfp );

	for ( i = 0; i < nqf; i++ ) {
		VkBool32 presentOK = VK_FALSE;
		if ( ( qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) == 0 )
			continue;
		b->vk.GetPhysicalDeviceSurfaceSupportKHR( pd, i, b->surface, &presentOK );
		if ( presentOK ) { ok = qtrue; break; }
	}
	free( qfp );
	return ok;
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
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: only RAL_BACKEND_VULKAN implemented in this build (requested %d)\n", (int)ci->type );
		return NULL;
	}
	if ( !ri.VK_GetInstanceProcAddr ) {
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: platform Vulkan loader (ri.VK_GetInstanceProcAddr) unavailable\n" );
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
	// Fine-grained ownership flags. Default-true for
	// the standalone path (creates everything). Imported branch flips both
	// to qfalse. Owned-instance branch sets ownsInstance=qtrue + ownsDevice=
	// qfalse (RAL owns instance/messenger/surface; renderer's vk_create_
	// device + Ral_AdoptDeviceAndQueues handle the device portion).
	b->ownsInstance    = qtrue;
	b->ownsDevice      = qtrue;

	// ── global entry points ────────────────────────────────────────────
	b->vk.EnumerateInstanceVersion             = (PFN_vkEnumerateInstanceVersion)            RAL_GIPA( VK_NULL_HANDLE, "vkEnumerateInstanceVersion" );
	b->vk.EnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)RAL_GIPA( VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties" );
	b->vk.CreateInstance                       = (PFN_vkCreateInstance)                      RAL_GIPA( VK_NULL_HANDLE, "vkCreateInstance" );
	if ( !b->vk.CreateInstance || !b->vk.EnumerateInstanceExtensionProperties ) {
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: Vulkan loader did not yield vkCreateInstance\n" );
		free( b );
		return NULL;
	}
	if ( b->vk.EnumerateInstanceVersion )
		b->vk.EnumerateInstanceVersion( &loaderVer );
	if ( VK_API_VERSION_MAJOR( loaderVer ) == 1 && VK_API_VERSION_MINOR( loaderVer ) < 1 ) {
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: Vulkan loader reports %u.%u; RAL requires 1.1+\n",
		        VK_API_VERSION_MAJOR( loaderVer ), VK_API_VERSION_MINOR( loaderVer ) );
		free( b );
		return NULL;
	}
	b->instanceApiVersion = loaderVer;

	// ── owned-instance (+ owned-device) branch ──
	// RAL creates VkInstance + debug messenger + VkSurfaceKHR + picks
	// physical device internally. When letBackendOwnDevice=qtrue
	// it also resolves queue families, builds the device-extension
	// allow-list, enables requested features, creates the VkDevice, and
	// brings up the frame/resource/pipeline layers.
	if ( ci->letBackendOwnInstance ) {
		// Default to owned-device; flipped qfalse at end of branch only when
		// letBackendOwnDevice=qfalse (legacy instance-only path, no current caller).
		b->ownsDevice = ci->letBackendOwnDevice ? qtrue : qfalse;

		// Vulkan 1.3+ loader check (mirrors vk.c create_instance:1969-1974).
		if ( VK_API_VERSION_MAJOR( loaderVer ) < 1u
		  || ( VK_API_VERSION_MAJOR( loaderVer ) == 1u && VK_API_VERSION_MINOR( loaderVer ) < 3u ) ) {
			R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: Vulkan loader reports %u.%u; Wired requires 1.3+\n",
			        VK_API_VERSION_MAJOR( loaderVer ), VK_API_VERSION_MINOR( loaderVer ) );
			free( b );
			return NULL;
		}

		// Build instance extension allow-list (mirrors renderer's
		// used_instance_extension at vk.c:1858-1895).
		{
			VkExtensionProperties *extProps;
			const char           **enabledExts;
			uint32_t               nExt = 0, nEnabled = 0, i;
			VkInstanceCreateFlags  iFlags = 0;
			VkApplicationInfo      appInfo;
			VkInstanceCreateInfo   ici;
			VkResult               r2;
			static const char     *kLayerLunarg  = "VK_LAYER_LUNARG_standard_validation";
			static const char     *kLayerKhronos = "VK_LAYER_KHRONOS_validation";

			b->vk.EnumerateInstanceExtensionProperties( NULL, &nExt, NULL );
			extProps    = nExt ? (VkExtensionProperties *)malloc( nExt * sizeof( *extProps ) ) : NULL;
			enabledExts = nExt ? (const char **)malloc( nExt * sizeof( *enabledExts ) )         : NULL;
			if ( nExt && ( !extProps || !enabledExts ) ) {
				if ( extProps ) free( extProps );
				if ( enabledExts ) free( (void *)enabledExts );
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: oom enumerating instance extensions\n" );
				free( b );
				return NULL;
			}
			if ( nExt )
				b->vk.EnumerateInstanceExtensionProperties( NULL, &nExt, extProps );
			for ( i = 0; i < nExt; i++ ) {
				const char *ext = extProps[i].extensionName;
				const char *u   = strrchr( ext, '_' );
				qboolean    use = qfalse;
				if ( u && Q_stricmp( u + 1, "surface" ) == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME )                       == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME )                     == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME )                   == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME )       == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME )         == 0 ) use = qtrue;
				else if ( Q_stricmp( ext, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME )    == 0 ) use = qtrue;
				if ( !use ) continue;
				enabledExts[ nEnabled++ ] = ext;
				if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
					iFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			}
			b->haveDebugUtils = qfalse;
			for ( i = 0; i < nEnabled; i++ ) {
				if ( Q_stricmp( enabledExts[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 ) { b->haveDebugUtils = qtrue; break; }
			}

			memset( &appInfo, 0, sizeof( appInfo ) );
			appInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.apiVersion = VK_API_VERSION_1_3;

			memset( &ici, 0, sizeof( ici ) );
			ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			ici.flags                   = iFlags;
			ici.pApplicationInfo        = &appInfo;
			ici.enabledExtensionCount   = nEnabled;
			ici.ppEnabledExtensionNames = nEnabled ? enabledExts : NULL;

			// Validation-layer fallback chain (mirrors vk.c:1987-2016).
			r2 = VK_ERROR_LAYER_NOT_PRESENT;
			if ( ci->enableValidation ) {
				ici.enabledLayerCount   = 1;
				ici.ppEnabledLayerNames = &kLayerLunarg;
				r2 = b->vk.CreateInstance( &ici, NULL, &b->instance );
				if ( r2 == VK_ERROR_LAYER_NOT_PRESENT ) {
					ici.ppEnabledLayerNames = &kLayerKhronos;
					r2 = b->vk.CreateInstance( &ici, NULL, &b->instance );
				}
				if ( r2 == VK_ERROR_LAYER_NOT_PRESENT ) {
					R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: validation layer not available\n" );
					ici.enabledLayerCount   = 0;
					ici.ppEnabledLayerNames = NULL;
					r2 = b->vk.CreateInstance( &ici, NULL, &b->instance );
				}
			} else {
				ici.enabledLayerCount   = 0;
				ici.ppEnabledLayerNames = NULL;
				r2 = b->vk.CreateInstance( &ici, NULL, &b->instance );
			}

			if ( enabledExts ) free( (void *)enabledExts );
			if ( extProps    ) free( extProps );

			if ( r2 != VK_SUCCESS || b->instance == VK_NULL_HANDLE ) {
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: vkCreateInstance failed (VkResult %d)\n", (int)r2 );
				free( b );
				return NULL;
			}
		}

		// Load instance-level entry points (now that b->instance exists).
		if ( !ralVk_LoadInstanceFuncs( b ) ) {
			b->vk.DestroyInstance( b->instance, NULL );
			b->instance = VK_NULL_HANDLE;
			free( b );
			return NULL;
		}

		// Only create a messenger if BOTH create and destroy are resolved —
		// otherwise teardown (ralVk_DestroyBackendInternal) can't destroy it and
		// it leaks across device lifetime (validation reports at vkDestroyInstance).
		// Mirrors the hardening on the other instance-create path.
		if ( !b->vk.CreateDebugUtilsMessengerEXT || !b->vk.DestroyDebugUtilsMessengerEXT )
			b->haveDebugUtils = qfalse;

		// Debug messenger (best effort; honor enableValidation gate).
		if ( ci->enableValidation && b->haveDebugUtils && b->vk.CreateDebugUtilsMessengerEXT ) {
			VkDebugUtilsMessengerCreateInfoEXT dci;
			memset( &dci, 0, sizeof( dci ) );
			dci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			dci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			dci.pfnUserCallback = ralVk_DebugCallback;
			dci.pUserData       = b;
			if ( b->vk.CreateDebugUtilsMessengerEXT( b->instance, &dci, NULL, &b->debugMessenger ) != VK_SUCCESS )
				b->debugMessenger = VK_NULL_HANDLE;   // non-fatal
		}

		// Platform surface (via engine's existing ri.VK_CreateSurface callback;
		// works on win32/linux/SDL transparently — same surface contract).
		if ( !b->vk.DestroySurfaceKHR || !b->vk.GetPhysicalDeviceSurfaceSupportKHR ) {
			R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: VK_KHR_surface entry points unavailable\n" );
			goto fail_after_instance;
		}
		if ( !ri.VK_CreateSurface ) {
			R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: ri.VK_CreateSurface unavailable\n" );
			goto fail_after_instance;
		}
		{
			VkSurfaceKHR surfaceLocal = VK_NULL_HANDLE;
			if ( !ri.VK_CreateSurface( b->instance, &surfaceLocal ) || surfaceLocal == VK_NULL_HANDLE ) {
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: ri.VK_CreateSurface failed\n" );
				goto fail_after_instance;
			}
			b->surface = surfaceLocal;
		}

		// Physical device enumeration + r_device-style picker.
		//
		// multi-device retry. The legacy vk.c device pick (origin/main
		// vk.c:2286-2295) iterated EVERY physical device until one could
		// present to the surface and complete device bringup; the single-device
		// rewrite dropped that, so on hybrid-GPU systems (PRIME offload, a
		// compute-only adapter enumerated first) or older drivers the renderer
		// hard-failed where it used to fall back. Restored here: we build an
		// ORDERED candidate list (preferred device first, then the rest), all
		// filtered to those that advertise both a graphics queue family AND
		// presentation to b->surface, then attempt owned-device bringup against
		// each in turn, falling through to the next on a device-specific
		// failure. preferredDeviceIndex is honoured as the first entry.
		uint32_t          nCand = 0;
		VkPhysicalDevice  candList[ 16 ];
		uint32_t          candIdx;
		{
			uint32_t          nDev = 0;
			VkPhysicalDevice *devs;
			int               devIdx = ci->preferredDeviceIndex;
			uint32_t          i;
			int               preferredPick = -1;

			b->vk.EnumeratePhysicalDevices( b->instance, &nDev, NULL );
			if ( nDev == 0 ) {
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: no Vulkan physical devices\n" );
				goto fail_after_instance;
			}
			devs = (VkPhysicalDevice *)malloc( nDev * sizeof( *devs ) );
			if ( !devs ) goto fail_after_instance;
			b->vk.EnumeratePhysicalDevices( b->instance, &nDev, devs );

			// preferredDeviceIndex semantics: -1 = first DISCRETE_GPU (or fall
			// through to first available); -2 = first INTEGRATED_GPU; N ≥ 0 =
			// explicit index. Matches r_device cvar at vk.c:3296+.
			if ( devIdx == -1 ) {
				for ( i = 0; i < nDev; i++ ) {
					VkPhysicalDeviceProperties p;
					b->vk.GetPhysicalDeviceProperties( devs[i], &p );
					if ( p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) { preferredPick = (int)i; break; }
				}
			} else if ( devIdx == -2 ) {
				for ( i = 0; i < nDev; i++ ) {
					VkPhysicalDeviceProperties p;
					b->vk.GetPhysicalDeviceProperties( devs[i], &p );
					if ( p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) { preferredPick = (int)i; break; }
				}
			} else if ( devIdx >= 0 && (uint32_t)devIdx < nDev ) {
				preferredPick = devIdx;
			}

			// Build the ordered candidate list: the preferred device first (if
			// it presents), then every other present-capable device in
			// enumeration order. ralVk_DeviceCanPresent returns qtrue only when
			// the device has a graphics queue family that supports presentation
			// to b->surface — exactly the legacy retry-loop predicate.
			if ( preferredPick >= 0 && ralVk_DeviceCanPresent( b, devs[ preferredPick ] ) )
				candList[ nCand++ ] = devs[ preferredPick ];
			for ( i = 0; i < nDev && nCand < (uint32_t)ARRAY_LEN( candList ); i++ ) {
				if ( (int)i == preferredPick ) continue;
				if ( ralVk_DeviceCanPresent( b, devs[i] ) )
					candList[ nCand++ ] = devs[i];
			}
			// Last-resort: if NO device advertised present support (e.g. a
			// headless / loader quirk), fall back to the preferred (or first)
			// device so behaviour is no worse than the single-device pick.
			if ( nCand == 0 ) {
				candList[ nCand++ ] = devs[ ( preferredPick >= 0 ) ? (uint32_t)preferredPick : 0u ];
			}
			free( devs );
		}

		// owned-instance-only contract: return here
		// with device == VK_NULL_HANDLE. No current caller uses this path;
		// preserved for hypothetical re-use. (Single device — the
		// retry loop below is owned-device-only.)
		if ( !ci->letBackendOwnDevice ) {
			b->physicalDevice = candList[ 0 ];
			b->vk.GetPhysicalDeviceProperties( b->physicalDevice, &b->physProps );
			b->vk.GetPhysicalDeviceMemoryProperties( b->physicalDevice, &b->memProps );
			return b;
		}

		// ── owned-device bringup ────────────────────
		// Absorbs vk.c::vk_create_device sections B-F (queue family resolve,
		// device-extension allow-list, feature query, paired-bundle enable,
		// vkCreateDevice). Surface invariant: b->surface populated above
		// before queue picker runs.
		//
		// retry each candidate device until one completes bringup. The
		// device-specific failure labels (fail_after_device / fail_after_devexts)
		// now fall through to the end of this loop body, advancing to the next
		// candidate; only after every candidate fails do we fall to
		// fail_after_instance below the loop.
		for ( candIdx = 0; candIdx < nCand; candIdx++ )
		{
			b->physicalDevice = candList[ candIdx ];
			b->vk.GetPhysicalDeviceProperties( b->physicalDevice, &b->physProps );
			b->vk.GetPhysicalDeviceMemoryProperties( b->physicalDevice, &b->memProps );
			VkPhysicalDeviceFeatures2                 f2support, f2enable;
			VkPhysicalDeviceVulkan12Features          v12support, v12enable;
			VkPhysicalDeviceSynchronization2Features  s2support,  s2enable;
			VkPhysicalDeviceDynamicRenderingFeatures  drsupport,  drenable;
			VkPhysicalDeviceFragmentShadingRateFeaturesKHR vrssupport, vrsenable;
			qboolean                                  fragmentShadingRateExtEnabled = qfalse;
			VkExtensionProperties                    *devExts = NULL;
			uint32_t                                  nDevExts = 0, i;
			VkDeviceQueueCreateInfo                   queueInfos[3];
			uint32_t                                  nQueueInfos = 0;
			const float                               priority = 1.0f;
			VkDeviceCreateInfo                        dci;
			VkResult                                  r2;

			// ── queue family resolve (Section B) ────────────────────────────
			{
				VkQueueFamilyProperties *qfp;
				uint32_t                 nqf = 0;

				b->vk.GetPhysicalDeviceQueueFamilyProperties( b->physicalDevice, &nqf, NULL );
				if ( nqf == 0 ) {
					R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: device reports zero queue families\n" );
					goto fail_after_instance;
				}
				qfp = (VkQueueFamilyProperties *)malloc( nqf * sizeof( *qfp ) );
				if ( !qfp ) goto fail_after_instance;
				b->vk.GetPhysicalDeviceQueueFamilyProperties( b->physicalDevice, &nqf, qfp );

				b->graphicsFamily = ~0U;
				b->computeFamily  = ~0U;
				b->transferFamily = ~0U;
				for ( i = 0; i < nqf; i++ ) {
					VkBool32 presentOK = VK_FALSE;
					b->vk.GetPhysicalDeviceSurfaceSupportKHR( b->physicalDevice, i, b->surface, &presentOK );
					if ( presentOK && ( qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0 ) {
						b->graphicsFamily = i;
						break;
					}
				}
				if ( b->graphicsFamily == ~0U ) {
					free( qfp );
					R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: no graphics+present queue family found\n" );
					goto fail_after_instance;
				}
				for ( i = 0; i < nqf; i++ ) {
					VkQueueFlags f = qfp[i].queueFlags;
					if ( ( f & VK_QUEUE_COMPUTE_BIT ) && !( f & VK_QUEUE_GRAPHICS_BIT ) ) {
						b->computeFamily = i; break;
					}
				}
				for ( i = 0; i < nqf; i++ ) {
					VkQueueFlags f = qfp[i].queueFlags;
					if ( ( f & VK_QUEUE_TRANSFER_BIT ) && !( f & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ) ) ) {
						b->transferFamily = i; break;
					}
				}
				if ( b->computeFamily  == ~0U ) b->computeFamily  = b->graphicsFamily;
				if ( b->transferFamily == ~0U ) b->transferFamily = b->graphicsFamily;
				free( qfp );
				R_LOG( rch_ral, SEV_INFO, "queue families: graphics=%u compute=%u transfer=%u%s%s\n",
				        b->graphicsFamily, b->computeFamily, b->transferFamily,
				        b->computeFamily  == b->graphicsFamily ? " (compute aliases graphics)"  : "",
				        b->transferFamily == b->graphicsFamily ? " (transfer aliases graphics)" : "" );
			}

			// ── device extension allow-list (Sections C+D absorbed) ─────────
			b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExts, NULL );
			if ( nDevExts ) {
				devExts = (VkExtensionProperties *)malloc( nDevExts * sizeof( *devExts ) );
				if ( !devExts ) goto fail_after_instance;
				b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExts, devExts );
			}

			if ( !ralVk_HasExtension( devExts, nDevExts, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) ) {
				if ( devExts ) free( devExts );
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: VK_KHR_swapchain not advertised — HARD-REQUIRED\n" );
				goto fail_after_instance;
			}

			// Build enabled list: caller-supplied (intersected with availability) + RAL-internal additions.
			// Max size = bci.platformDeviceExtensionCount + 2 (swapchain + memory_budget).
			{
				uint32_t  cap = ci->platformDeviceExtensionCount + 2u;
				uint32_t  k;
				static const char *const kSwap   = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
				static const char *const kMemBud = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;

				b->enabledDeviceExtensions      = (const char **)malloc( cap * sizeof( *b->enabledDeviceExtensions ) );
				b->enabledDeviceExtensionCount  = 0;
				if ( !b->enabledDeviceExtensions ) { free( devExts ); goto fail_after_instance; }

				b->enabledDeviceExtensions[ b->enabledDeviceExtensionCount++ ] = kSwap;
				b->haveMemoryBudget = ralVk_HasExtension( devExts, nDevExts, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME );
				if ( b->haveMemoryBudget )
					b->enabledDeviceExtensions[ b->enabledDeviceExtensionCount++ ] = kMemBud;
				for ( k = 0; k < ci->platformDeviceExtensionCount; k++ ) {
					const char *want = ci->platformDeviceExtensions[k];
					if ( !want ) continue;
					if ( Q_stricmp( want, VK_KHR_SWAPCHAIN_EXTENSION_NAME )      == 0 ) continue;   // already added
					if ( Q_stricmp( want, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME )  == 0 ) continue;   // already added
					if ( !ralVk_HasExtension( devExts, nDevExts, want ) )            continue;
					// Note whether the fragment-shading-rate extension made it into the
					// enabled list — the feature can only be enabled (below) when the
					// extension is actually being enabled on this device.
					if ( Q_stricmp( want, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME ) == 0 )
						fragmentShadingRateExtEnabled = qtrue;
					b->enabledDeviceExtensions[ b->enabledDeviceExtensionCount++ ] = want;
				}
			}
			if ( devExts ) { free( devExts ); devExts = NULL; }

			// ── features query (Section E start) ────────────────────────────
			memset( &v12support, 0, sizeof( v12support ) ); v12support.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			memset( &s2support,  0, sizeof( s2support  ) ); s2support.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
			memset( &drsupport,  0, sizeof( drsupport  ) ); drsupport.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
			memset( &vrssupport, 0, sizeof( vrssupport ) ); vrssupport.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
			v12support.pNext = &s2support;  s2support.pNext = &drsupport;
			// Only chain the VRS query when the extension is actually being enabled —
			// querying a struct whose extension isn't enabled is harmless but pointless,
			// and keeping it off the chain when absent avoids a confusing all-zero report.
			if ( fragmentShadingRateExtEnabled ) { drsupport.pNext = &vrssupport; vrssupport.pNext = NULL; }
			else                                 { drsupport.pNext = NULL; }
			memset( &f2support, 0, sizeof( f2support ) );  f2support.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			f2support.pNext = &v12support;
			b->vk.GetPhysicalDeviceFeatures2( b->physicalDevice, &f2support );

			// HARD-REQUIRED enforcement (K15).
			if ( f2support.features.fillModeNonSolid != VK_TRUE ) {
				R_LOG( rch_ral, SEV_ERROR, "device lacks HARD-REQUIRED feature fillModeNonSolid\n" );
				goto fail_after_devexts;
			}
			if ( s2support.synchronization2 != VK_TRUE
			  || v12support.timelineSemaphore != VK_TRUE
			  || drsupport.dynamicRendering   != VK_TRUE ) {
				R_LOG( rch_ral, SEV_ERROR, "device lacks HARD-REQUIRED features: sync2=%d timelineSemaphore=%d dynamicRendering=%d — Wired requires a Vulkan 1.3-class GPU\n",
				        (int)s2support.synchronization2, (int)v12support.timelineSemaphore, (int)drsupport.dynamicRendering );
				goto fail_after_devexts;
			}

			// ── feature enable chain (K14 paired-bundle AND-gate logic) ────
			memset( &f2enable,  0, sizeof( f2enable  ) ); f2enable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			memset( &v12enable, 0, sizeof( v12enable ) ); v12enable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			memset( &s2enable,  0, sizeof( s2enable  ) ); s2enable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
			memset( &drenable,  0, sizeof( drenable  ) ); drenable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
			memset( &vrsenable, 0, sizeof( vrsenable ) ); vrsenable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
			f2enable.pNext = &v12enable;  v12enable.pNext = &s2enable;  s2enable.pNext = &drenable;  drenable.pNext = NULL;

			// HARD-REQUIRED enables (always-on after the gate above).
			f2enable.features.fillModeNonSolid = VK_TRUE;
			s2enable.synchronization2          = VK_TRUE;
			v12enable.timelineSemaphore        = VK_TRUE;
			drenable.dynamicRendering          = VK_TRUE;

			// Variable-rate shading (pipeline rate): enable ONLY when the renderer
			// requested it, the extension is being enabled, and the device reports the
			// pipelineFragmentShadingRate feature. b->haveFragmentShadingRate (mirrored
			// below) is the single source of truth the caps query reads — so the
			// variableRateShading cap is true ONLY when vkCreateGraphicsPipelines with a
			// VkPipelineFragmentShadingRateStateCreateInfoKHR is legal. (Chained onto
			// drenable so it reaches vkCreateDevice's pNext.)
			b->haveFragmentShadingRate = qfalse;
			if ( ci->requestFeatures.wantFragmentShadingRate
			  && fragmentShadingRateExtEnabled
			  && vrssupport.pipelineFragmentShadingRate == VK_TRUE ) {
				vrsenable.pipelineFragmentShadingRate = VK_TRUE;
				drenable.pNext = &vrsenable;  vrsenable.pNext = NULL;
				b->haveFragmentShadingRate = qtrue;
			}

			// RENDERER-WANTS-IF-AVAILABLE single-bit wants.
			if ( ci->requestFeatures.wantShaderInt64 && f2support.features.shaderInt64 )
				f2enable.features.shaderInt64 = VK_TRUE;
			if ( ci->requestFeatures.wantWideLines && f2support.features.wideLines )
				f2enable.features.wideLines = VK_TRUE;
			if ( ci->requestFeatures.wantDepthClamp && f2support.features.depthClamp )
				f2enable.features.depthClamp = VK_TRUE;
			if ( ci->requestFeatures.wantSamplerAnisotropy && f2support.features.samplerAnisotropy )
				f2enable.features.samplerAnisotropy = VK_TRUE;
			if ( ci->requestFeatures.wantHostQueryReset && v12support.hostQueryReset )
				v12enable.hostQueryReset = VK_TRUE;
			if ( ci->requestFeatures.wantDrawIndirectCount && v12support.drawIndirectCount )
				v12enable.drawIndirectCount = VK_TRUE;
			if ( ci->requestFeatures.wantBufferDeviceAddress && v12support.bufferDeviceAddress )
				v12enable.bufferDeviceAddress = VK_TRUE;

			// PAIRED: vertex+fragment stores
			if ( ci->requestFeatures.wantVertexFragmentStores
			  && f2support.features.vertexPipelineStoresAndAtomics
			  && f2support.features.fragmentStoresAndAtomics ) {
				f2enable.features.vertexPipelineStoresAndAtomics = VK_TRUE;
				f2enable.features.fragmentStoresAndAtomics       = VK_TRUE;
			}
			// PAIRED 4-bundle + 2 mirror: descriptor indexing
			if ( ci->requestFeatures.wantDescriptorIndexing
			  && v12support.shaderSampledImageArrayNonUniformIndexing
			  && v12support.runtimeDescriptorArray
			  && v12support.descriptorBindingPartiallyBound
			  && v12support.descriptorBindingSampledImageUpdateAfterBind ) {
				v12enable.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
				v12enable.runtimeDescriptorArray                       = VK_TRUE;
				v12enable.descriptorBindingPartiallyBound              = VK_TRUE;
				v12enable.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
				if ( v12support.descriptorBindingUpdateUnusedWhilePending )
					v12enable.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
				if ( v12support.descriptorBindingVariableDescriptorCount )
					v12enable.descriptorBindingVariableDescriptorCount = VK_TRUE;
			}
			// PAIRED: vulkan memory model + scope
			if ( ci->requestFeatures.wantVulkanMemoryModel
			  && v12support.vulkanMemoryModel
			  && v12support.vulkanMemoryModelDeviceScope ) {
				v12enable.vulkanMemoryModel            = VK_TRUE;
				v12enable.vulkanMemoryModelDeviceScope = VK_TRUE;
			}
			// PAIRED: 8-bit storage
			if ( ci->requestFeatures.want8BitStorage
			  && v12support.storageBuffer8BitAccess
			  && v12support.uniformAndStorageBuffer8BitAccess ) {
				v12enable.storageBuffer8BitAccess            = VK_TRUE;
				v12enable.uniformAndStorageBuffer8BitAccess  = VK_TRUE;
			}

			// Mirror enabled state into b->have* flags + ralCaps_t feature mirrors.
			b->haveSync2              = qtrue;
			b->haveTimelineSemaphore  = qtrue;
			b->haveSamplerAnisotropy  = ( f2enable.features.samplerAnisotropy == VK_TRUE ) ? qtrue : qfalse;
			b->haveHostQueryReset     = ( v12enable.hostQueryReset    == VK_TRUE ) ? qtrue : qfalse;
			b->haveDrawIndirectCount  = ( v12enable.drawIndirectCount == VK_TRUE ) ? qtrue : qfalse;
			b->haveDescriptorIndexing = ( v12enable.shaderSampledImageArrayNonUniformIndexing == VK_TRUE
			                           && v12enable.runtimeDescriptorArray                    == VK_TRUE
			                           && v12enable.descriptorBindingPartiallyBound           == VK_TRUE
			                           && v12enable.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE ) ? qtrue : qfalse;
			// Feature caps go through b->have* flags, NOT b->caps.* directly:
			// ralVk_FillCaps memset-clears the whole caps struct AFTER this block and
			// then copies each have-flag back into c->* — so a direct b->caps.* write
			// here would be silently wiped (the latent bug that left wideLines /
			// vertexFragmentStores / samplerAnisotropyEnabled reading false). The
			// have-flag is what survives to the renderer's caps read.
			b->haveWideLines                  = ( f2enable.features.wideLines == VK_TRUE ) ? qtrue : qfalse;
			b->haveDepthClamp                 = ( f2enable.features.depthClamp == VK_TRUE ) ? qtrue : qfalse;
			b->haveVertexFragmentStores       = ( f2enable.features.vertexPipelineStoresAndAtomics == VK_TRUE
			                                  &&  f2enable.features.fragmentStoresAndAtomics       == VK_TRUE ) ? qtrue : qfalse;
			// (b->haveSamplerAnisotropy is already set above from f2enable.samplerAnisotropy.)

			// ── queue create infos ──────────────────────────────────────────
			memset( queueInfos, 0, sizeof( queueInfos ) );
			queueInfos[ nQueueInfos ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfos[ nQueueInfos ].queueFamilyIndex = b->graphicsFamily;
			queueInfos[ nQueueInfos ].queueCount       = 1;
			queueInfos[ nQueueInfos ].pQueuePriorities = &priority;
			nQueueInfos++;
			if ( b->computeFamily != b->graphicsFamily ) {
				queueInfos[ nQueueInfos ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfos[ nQueueInfos ].queueFamilyIndex = b->computeFamily;
				queueInfos[ nQueueInfos ].queueCount       = 1;
				queueInfos[ nQueueInfos ].pQueuePriorities = &priority;
				nQueueInfos++;
			}
			if ( b->transferFamily != b->graphicsFamily && b->transferFamily != b->computeFamily ) {
				queueInfos[ nQueueInfos ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfos[ nQueueInfos ].queueFamilyIndex = b->transferFamily;
				queueInfos[ nQueueInfos ].queueCount       = 1;
				queueInfos[ nQueueInfos ].pQueuePriorities = &priority;
				nQueueInfos++;
			}

			// ── vkCreateDevice (Section F) ──────────────────────────────────
			memset( &dci, 0, sizeof( dci ) );
			dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			dci.pNext                   = &f2enable;
			dci.queueCreateInfoCount    = nQueueInfos;
			dci.pQueueCreateInfos       = queueInfos;
			dci.enabledExtensionCount   = b->enabledDeviceExtensionCount;
			dci.ppEnabledExtensionNames = b->enabledDeviceExtensions;
			dci.pEnabledFeatures        = NULL;   // features passed via f2enable.features

			r2 = b->vk.CreateDevice( b->physicalDevice, &dci, NULL, &b->device );
			if ( r2 != VK_SUCCESS || b->device == VK_NULL_HANDLE ) {
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: vkCreateDevice failed (VkResult %d)\n", (int)r2 );
				goto fail_after_devexts;
			}

			if ( ci->externalApiVersion ) {
				b->instanceApiVersion = ci->externalApiVersion;
			} else {
				b->instanceApiVersion = VK_API_VERSION_1_3;
			}

			if ( !ralVk_LoadDeviceFuncs( b ) ) {
				R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: ralVk_LoadDeviceFuncs failed\n" );
				goto fail_after_device;
			}

			goto initSharedLayers;   // success — exits the candidate loop

		fail_after_device:
			if ( b->device != VK_NULL_HANDLE && b->vk.DestroyDevice )
				b->vk.DestroyDevice( b->device, NULL );
			b->device = VK_NULL_HANDLE;
		fail_after_devexts:
			if ( b->enabledDeviceExtensions ) {
				free( (void *)b->enabledDeviceExtensions );
				b->enabledDeviceExtensions     = NULL;
				b->enabledDeviceExtensionCount = 0;
			}
			if ( devExts ) { free( devExts ); devExts = NULL; }
			// device-specific bringup failure: reset the per-device
			// feature/queue state and retry the NEXT candidate device instead
			// of tearing down the instance. b->graphics/compute/transferFamily
			// are recomputed at the top of the next iteration's queue resolve.
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: device candidate %u/%u failed bringup — trying next\n",
			        candIdx + 1u, nCand );
		}   // end candidate-device retry loop

		// Every present-capable candidate device failed owned-device bringup.
		R_LOG( rch_ral, SEV_ERROR, "Ral_CreateBackend: no candidate device completed owned-device bringup (%u tried)\n", nCand );
		goto fail_after_instance;

	fail_after_instance:
		if ( b->surface != VK_NULL_HANDLE )           b->vk.DestroySurfaceKHR( b->instance, b->surface, NULL );
		if ( b->debugMessenger != VK_NULL_HANDLE && b->vk.DestroyDebugUtilsMessengerEXT )
			b->vk.DestroyDebugUtilsMessengerEXT( b->instance, b->debugMessenger, NULL );
		b->vk.DestroyInstance( b->instance, NULL );
		free( b );
		return NULL;
	}

	// ── imported-mode branch ────────────────────────────
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

		b->ownsInstance   = qfalse;
		b->ownsDevice     = qfalse;
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
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend (imported): caller passed NULL instance/physicalDevice/device handle\n" );
			free( b );
			return NULL;
		}
		if ( !ralVk_LoadInstanceFuncs( b ) ) {
			free( b );
			return NULL;
		}
		// Caller may or may not have set up debug-utils on their instance; we
		// don't create our own messenger in imported mode (would double-fire).
		// Infer haveDebugUtils from entry-point
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
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend (imported): caller's device lacks synchronization2 (%d) / timelineSemaphore (%d) / dynamicRendering (%d) — Wired requires a Vulkan 1.3-class GPU\n",
			        (int)s2s.synchronization2, (int)v12s.timelineSemaphore, (int)drs.dynamicRendering );
			free( b );
			return NULL;
		}
		b->haveSync2              = qtrue;
		b->haveTimelineSemaphore  = qtrue;
		b->haveSamplerAnisotropy  = ( f2q.features.samplerAnisotropy == VK_TRUE ) ? qtrue : qfalse;
		// Imported-adopt path: we don't create the device, only observe what the
		// caller enabled. Report wideLines / vertex+fragment-stores from the device
		// feature query so the have-flags (→ caps via ralVk_FillCaps) are accurate.
		b->haveWideLines          = ( f2q.features.wideLines == VK_TRUE ) ? qtrue : qfalse;
		b->haveVertexFragmentStores = ( f2q.features.vertexPipelineStoresAndAtomics == VK_TRUE
		                            &&  f2q.features.fragmentStoresAndAtomics       == VK_TRUE ) ? qtrue : qfalse;
		b->haveHostQueryReset     = ( v12s.hostQueryReset == VK_TRUE ) ? qtrue : qfalse;
		b->haveDrawIndirectCount  = ( v12s.drawIndirectCount == VK_TRUE ) ? qtrue : qfalse;
		b->haveDescriptorIndexing = ( v12s.shaderSampledImageArrayNonUniformIndexing == VK_TRUE
		                           && v12s.runtimeDescriptorArray                    == VK_TRUE
		                           && v12s.descriptorBindingPartiallyBound           == VK_TRUE
		                           && v12s.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE ) ? qtrue : qfalse;
		// renderervk now enables VK_EXT_memory_budget
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
				if ( devExtProps ) {
					b->vk.EnumerateDeviceExtensionProperties( b->physicalDevice, NULL, &nDevExt, devExtProps );
				} else {
					// malloc failed — don't hand a NULL buffer to the driver
					// (it would write nDevExt entries into it). Treat as "no
					// extensions enumerated" for the budget probe.
					nDevExt = 0;
				}
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
		qboolean                wantDebugUtils = ci->enableValidation || ( b->flags & RAL_FLAG_DEBUG_LABELS ) != 0;
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
		if ( ci->enableValidation )
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
			R_LOG( rch_ral, SEV_DEBUG, "vkCreateInstance with layers/extensions failed (VkResult %d); retrying bare\n", (int)r );
			b->haveDebugUtils       = qfalse;
			ici.enabledLayerCount   = 0; ici.ppEnabledLayerNames     = NULL;
			ici.enabledExtensionCount = 0; ici.ppEnabledExtensionNames = NULL;
			r = b->vk.CreateInstance( &ici, NULL, &b->instance );
		}
		if ( instExtProps ) free( instExtProps );
		if ( r != VK_SUCCESS || b->instance == VK_NULL_HANDLE ) {
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: vkCreateInstance failed (VkResult %d)\n", (int)r );
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
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: no Vulkan physical devices\n" );
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
		if ( nq == 0 ) { R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: device has no queue families\n" ); goto fail; }
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
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: device has no graphics queue family\n" );
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
		// The 1.2-promoted features (descriptor indexing, timeline
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

		// synchronization2 + timelineSemaphore + dynamicRendering are required:
		// vkQueueSubmit2, vkCmdWriteTimestamp2, timeline ops, vkCmdBeginRendering.
		if ( s2Support.synchronization2 != VK_TRUE || v12support.timelineSemaphore != VK_TRUE || drSupport.dynamicRendering != VK_TRUE ) {
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: device lacks synchronization2 (%d) / timelineSemaphore (%d) / dynamicRendering (%d) — required since Phase 7.3\n",
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
		v12enable.drawIndirectCount = b->haveDrawIndirectCount ? VK_TRUE : VK_FALSE;
		s2Enable.synchronization2   = VK_TRUE;
		drEnable.dynamicRendering   = VK_TRUE;
		if ( b->haveSamplerAnisotropy ) f2enable.features.samplerAnisotropy = VK_TRUE;
		// The imported path has no want-bits, so enable these raster/shader features
		// from the device query directly (mirrors samplerAnisotropy). Each have-flag is
		// copied into caps.* by ralVk_FillCaps so a pipeline built on this backend may
		// legally use the feature.
		if ( f2query.features.depthClamp == VK_TRUE ) {
			f2enable.features.depthClamp = VK_TRUE;
			b->haveDepthClamp = qtrue;
		}
		if ( f2query.features.wideLines == VK_TRUE ) {
			f2enable.features.wideLines = VK_TRUE;
			b->haveWideLines = qtrue;
		}
		if ( f2query.features.vertexPipelineStoresAndAtomics == VK_TRUE
		  && f2query.features.fragmentStoresAndAtomics       == VK_TRUE ) {
			f2enable.features.vertexPipelineStoresAndAtomics = VK_TRUE;
			f2enable.features.fragmentStoresAndAtomics       = VK_TRUE;
			b->haveVertexFragmentStores = qtrue;
		}
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
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: vkCreateDevice failed (VkResult %d)\n", (int)r );
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
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: frame layer init failed\n" );
		goto fail;
	}
	if ( !ralVk_InitResourceLayer( b ) ) {  // descriptor pool, format-blit cache
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: resource layer init failed\n" );
		goto fail;
	}
	if ( !ralVk_InitPipelineLayer( b ) ) {  // empty VkPipelineCache + layout cache
		R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend: pipeline layer init failed\n" );
		goto fail;
	}

	R_LOG( rch_ral, SEV_INFO, "Vulkan backend ready: %s [%s] (queue families gfx/cmp/xfer = %u/%u/%u; debugUtils=%s memBudget=%s descriptorIndexing=%s sync2=%s timeline=%s drawIndirectCount=%s anisotropy=%.0fx)\n",
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

// Ral_AdoptDeviceAndQueues retired. Its body
// folded into Ral_CreateBackend's owned-device branch (gated by
// letBackendOwnDevice=qtrue). The renderer no longer needs the two-
// step bringup contract that earlier split instance and device creation.

const ralCaps_t *Ral_GetCaps( ralBackend_t *b ) {
	return b ? &b->caps : NULL;
}

// ════════════════════════════════════════════════════════════════════════
// Frame layer: per-queue command pools, queue mutexes, frame fences,
// deferred-destroy ring.
// ════════════════════════════════════════════════════════════════════════
qboolean ralVk_InitFrameLayer( ralBackend_t *b ) {
	VkCommandPoolCreateInfo cpi;
	VkFenceCreateInfo       fi;
	uint32_t                q, i;

	if ( !ralVk_InitQueueMutexes( b ) ) {
		R_LOG( rch_ral, SEV_WARN, "ralVk_InitFrameLayer: queue mutex creation failed\n" );
		return qfalse;
	}
	for ( q = 0; q < 3; q++ ) {
		RAL_ZERO( cpi );
		cpi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpi.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cpi.queueFamilyIndex = b->queueFamily[q];
		if ( b->vk.CreateCommandPool( b->device, &cpi, NULL, &b->cmdPools[q] ) != VK_SUCCESS ) {
			R_LOG( rch_ral, SEV_WARN, "ralVk_InitFrameLayer: vkCreateCommandPool (queue %u) failed\n", q );
			return qfalse;
		}
	}
	RAL_ZERO( fi );
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // first Ral_BeginFrame must not block
	for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT; i++ )
		if ( b->vk.CreateFence( b->device, &fi, NULL, &b->frameFences[i] ) != VK_SUCCESS ) {
			R_LOG( rch_ral, SEV_WARN, "ralVk_InitFrameLayer: frame fence %u creation failed\n", i );
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
	case RAL_RES_CMD_BUFFER:      { VkCommandBuffer cb = RAL_VK_U2H( VkCommandBuffer, e->h1 ); ralQueueType_t q = (ralQueueType_t)e->h2;
	                                ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, &cb ); ralVk_QueueUnlock( b, q ); break; }
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

// Ral_DrainDeferred — renderer-driven per-frame drain (see ral_command.h). The
// renderer owns its own render loop + per-frame fence wait, so it cannot use
// Ral_BeginFrame's fence-wait / Ral_EndFrame's empty stand-in submit (those
// would collide with the renderer's real swapchain submission). This advances
// the deferred-destroy frame counter and reclaims entries past the in-flight
// window using the SAME drainBeforeFrame arithmetic as Ral_BeginFrame, but
// touches neither b->frameFences[] nor the queue. Resources queued at frame F
// are freed once currentFrame has advanced more than RAL_VK_MAX_FRAMES_IN_
// FLIGHT past F — the renderer's caller must have already waited the matching
// frame fence so that work is complete.
void Ral_DrainDeferred( ralBackend_t *b ) {
	if ( !b || !b->pendingDestroy ) return;
	b->currentFrame++;
	ralVk_DrainPendingDestroy( b, ( b->currentFrame > RAL_VK_MAX_FRAMES_IN_FLIGHT )
	                              ? ( b->currentFrame - RAL_VK_MAX_FRAMES_IN_FLIGHT + 1 ) : 1ull );
}

void Ral_EndFrame( ralBackend_t *b ) {
	uint32_t idx;
	VkSubmitInfo2 si2;
	if ( !b || !b->pendingDestroy ) return;
	idx = (uint32_t)( b->currentFrame % RAL_VK_MAX_FRAMES_IN_FLIGHT );
	// Empty submit on the graphics queue just to signal the frame fence (in a
	// full render loop the frame's real submission carries the fence; the dump
	// path has no render loop, so this stand-in keeps the BeginFrame wait/reset cycle
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
// Ral_RunDiagnostic — shared body for the generic "\ral_dump" command and
// the dedicated "\ral_pipeline_test" compatibility command. A forced
// subcommand keeps the latter independent of Cmd_Argv state while exercising
// the exact same offscreen backend path as "\ral_dump pipeline".
// ════════════════════════════════════════════════════════════════════════
static void Ral_RunDiagnostic( const char *forcedSubcommand ) {
	{
		ralBackendAvailability_t avail[4];
		ralBackendCreateInfo_t   ci;
		ralBackend_t            *b;
		const ralCaps_t         *c;
		ralMemoryBudget_t        mb;
		uint32_t                 n, i;

		R_LOG( rch_ral, SEV_INFO, "===== RAL dump (Phase 7.3c Vulkan: instance/device/resources/async/pipeline) =====\n" );

		n = Ral_ProbeBackends( avail, 4 );
		R_LOG( rch_ral, SEV_INFO, "Ral_ProbeBackends -> %u backend(s)\n", n );
		for ( i = 0; i < n; i++ ) {
			R_LOG( rch_ral, SEV_INFO, "  [%u] %-12s available=%-3s device=\"%s\"%s%s\n",
			        i, avail[i].name ? avail[i].name : "?",
			        avail[i].available ? "yes" : "no",
			        avail[i].deviceName ? avail[i].deviceName : "-",
			        avail[i].reason ? "  reason=" : "", avail[i].reason ? avail[i].reason : "" );
		}

		memset( &ci, 0, sizeof( ci ) );
		ci.type             = RAL_BACKEND_VULKAN;
		ci.platformHandle   = NULL;                     // offscreen — no swapchain in the dump path
		ci.flags            = RAL_FLAG_DEBUG_LABELS;
		ci.enableValidation = ( ri.Cvar_VariableIntegerValue( "r_vkValidate" ) != 0 ) ? qtrue : qfalse;
		b = Ral_CreateBackend( &ci );
		if ( !b ) {
			R_LOG( rch_ral, SEV_WARN, "Ral_CreateBackend failed (see [RAL] warnings above)\n" );
			R_LOG( rch_ral, SEV_INFO, "===== end RAL dump =====\n" );
			return;
		}

		c = Ral_GetCaps( b );
		R_LOG( rch_ral, SEV_INFO, "Ral_GetCaps:\n" );
		R_LOG( rch_ral, SEV_INFO, "  device                   : %s\n", c->deviceName );
		R_LOG( rch_ral, SEV_INFO, "  apiVersion               : %s\n", c->apiVersion );
		R_LOG( rch_ral, SEV_INFO, "  bindlessTextures         : %s (max %u)\n", c->bindlessTextures ? "yes" : "no", c->maxBindlessTextures );
		R_LOG( rch_ral, SEV_INFO, "  dynamicRendering         : %s\n", c->dynamicRendering ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  timelineSemaphores       : %s\n", c->timelineSemaphores ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  asyncCompute             : %s\n", c->asyncCompute ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  asyncTransfer            : %s\n", c->asyncTransfer ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  variableRateShading      : %s\n", c->variableRateShading ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  hdr10Swapchain           : %s\n", c->hdr10Swapchain ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  scRGBSwapchain           : %s\n", c->scRGBSwapchain ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  debugUtils               : %s\n", c->debugUtils ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  memoryBudget             : %s\n", c->memoryBudget ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  drawIndirectCount        : %s\n", c->drawIndirectCount ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  maxColorAttachments      : %u\n", c->maxColorAttachments );
		R_LOG( rch_ral, SEV_INFO, "  maxComputeWorkgroupSize  : %u\n", c->maxComputeWorkgroupSize );
		R_LOG( rch_ral, SEV_INFO, "  maxTextureDimension2D/3D : %u / %u\n", c->maxTextureDimension2D, c->maxTextureDimension3D );
		R_LOG( rch_ral, SEV_INFO, "  maxTextureArrayLayers    : %u\n", c->maxTextureArrayLayers );
		R_LOG( rch_ral, SEV_INFO, "  maxPushConstantSize      : %u bytes\n", c->maxPushConstantSize );
		R_LOG( rch_ral, SEV_INFO, "  minUBO / minSSBO align   : %u / %u bytes\n", (unsigned)c->minUniformBufferAlignment, (unsigned)c->minStorageBufferAlignment );
		R_LOG( rch_ral, SEV_INFO, "  timestampPeriod          : %.3f ns/tick\n", c->timestampPeriodNs );

		Ral_QueryMemoryBudget( b, &mb );
		R_LOG( rch_ral, SEV_INFO, "Ral_QueryMemoryBudget:\n" );
		R_LOG( rch_ral, SEV_INFO, "  device-local : %u / %u MiB used\n", (unsigned)( mb.deviceLocalUsed >> 20 ), (unsigned)( mb.deviceLocalBudget >> 20 ) );
		R_LOG( rch_ral, SEV_INFO, "  host-visible : %u / %u MiB used\n", (unsigned)( mb.hostVisibleUsed >> 20 ), (unsigned)( mb.hostVisibleBudget >> 20 ) );
		R_LOG( rch_ral, SEV_INFO, "  underPressure: %s\n", mb.underPressure ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  RAL footprint: %u KiB device-local / %u KiB host-visible across %u allocation(s)\n",
		        (unsigned)( b->ralDeviceLocalBytes >> 10 ), (unsigned)( b->ralHostVisibleBytes >> 10 ), b->numAllocations );

		// "\ral_dump <sub>" runs an extra exercise on the live backend:
		//   resource | test  → resource layer (texture/upload/mip-gen/bindless/budget/teardown)
		//   async            → queue/cmd/sync/query/deferred-destroy
		//   pipeline         → pipeline/cache/layout-cache/draw/dispatch
		//   all              → all of the above
		{
			const char *sub = forcedSubcommand;
			if ( ( !sub || !sub[0] ) && ri.Cmd_Argc() > 1 ) sub = ri.Cmd_Argv( 1 );
			if ( !sub || !sub[0] ) goto diagnostic_done;
			if ( Q_stricmp( sub, "resource" ) == 0 || Q_stricmp( sub, "test" ) == 0 )      ralVk_RunResourceTest( b );
			else if ( Q_stricmp( sub, "async" ) == 0 )                                     ralVk_RunAsyncTest( b );
			else if ( Q_stricmp( sub, "pipeline" ) == 0 )                                  ralVk_RunPipelineTest( b );
			else if ( Q_stricmp( sub, "all" ) == 0 )                                     { ralVk_RunResourceTest( b ); ralVk_RunAsyncTest( b ); ralVk_RunPipelineTest( b ); }
			else R_LOG( rch_ral, SEV_INFO, "  (unknown \\ral_dump subcommand \"%s\" — try: resource | async | pipeline | all)\n", sub );
		}

	diagnostic_done:
		Ral_DestroyBackend( b );
		R_LOG( rch_ral, SEV_INFO, "Ral_DestroyBackend: ok\n" );
		R_LOG( rch_ral, SEV_INFO, "===== end RAL dump =====\n" );
	}
}

Q_EXPORT void Ral_Dump( void ) {
	Ral_RunDiagnostic( NULL );
}

void Ral_RunPipelineDiagnostic( void ) {
	Ral_RunDiagnostic( "pipeline" );
}
