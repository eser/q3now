// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_internal.h — private header shared by the Vulkan RAL backend's
// translation units (ral_vulkan_backend.c / _caps.c / _memory.c / _resource.c
// and the still-stubbed _pipeline / _command / _swapchain / _query TUs).
// Defines the concrete struct ralBackend_s, the per-backend Vulkan
// function-pointer table, the resource handle structs, the suballocator, and
// the helpers.
//
// IMPORTANT: this backend is deliberately *independent* of code/renderervk/.
// It owns its own VkInstance / VkPhysicalDevice / VkDevice / VkQueues — they
// are never shared with the legacy Vulkan renderer (per phase-7-ral-design.md).
// Host loader/surface/log services arrive through ralHostImports_t. The core
// archive has no renderer-global import dependency and can be linked by an
// isolated tool host.

#ifndef WIRED_RAL_VULKAN_INTERNAL_H
#define WIRED_RAL_VULKAN_INTERNAL_H

// The RAL backend loads every Vulkan entry point itself; never let vulkan.h
// emit bare prototypes (would collide conceptually with the engine's qvk*
// loader and pull in the unlinkable loader symbols on Windows).
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../../renderercommon/vulkan/vulkan.h"

#include "../ral/ral.h"                       // q_shared.h + the public RAL surface
#include "../ral/ral_host.h"                  // backend-neutral host contract helpers
#include "ral_vulkan_bridge.h"                // backend-native migration seam (not portable RAL)
#include "ral_vulkan_translate.h"             // exact pure RAL-enum → Vulkan mapping
#include <string.h>
#include <stdlib.h>   // backend internal allocations use stdlib malloc/free (not ri.Malloc) so RAL state survives ri.FreeAll() inside R_InitImages.
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── per-backend Vulkan dispatch table ───────────────────────────────────
// Global-level fns come from host.getProcAddress(NULL, ...); instance-level
// from host.getProcAddress(instance, ...); device-level
// from vkGetDeviceProcAddr(device, ...).
typedef struct {
	// global
	PFN_vkEnumerateInstanceVersion              EnumerateInstanceVersion;            // NULL on a 1.0 loader
	PFN_vkEnumerateInstanceExtensionProperties  EnumerateInstanceExtensionProperties;
	PFN_vkCreateInstance                        CreateInstance;

	// instance
	PFN_vkDestroyInstance                       DestroyInstance;
	PFN_vkEnumeratePhysicalDevices              EnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceProperties           GetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceProperties2          GetPhysicalDeviceProperties2;        // core 1.1
	PFN_vkGetPhysicalDeviceFeatures             GetPhysicalDeviceFeatures;
	PFN_vkGetPhysicalDeviceFeatures2            GetPhysicalDeviceFeatures2;          // core 1.1
	PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties     GetPhysicalDeviceMemoryProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties2    GetPhysicalDeviceMemoryProperties2;  // core 1.1
	PFN_vkGetPhysicalDeviceFormatProperties     GetPhysicalDeviceFormatProperties;
	PFN_vkEnumerateDeviceExtensionProperties    EnumerateDeviceExtensionProperties;
	PFN_vkCreateDevice                          CreateDevice;
	PFN_vkGetDeviceProcAddr                     GetDeviceProcAddr;
	PFN_vkCreateDebugUtilsMessengerEXT          CreateDebugUtilsMessengerEXT;        // NULL unless VK_EXT_debug_utils
	PFN_vkDestroyDebugUtilsMessengerEXT         DestroyDebugUtilsMessengerEXT;
	PFN_vkSetDebugUtilsObjectNameEXT            SetDebugUtilsObjectNameEXT;          // NULL unless VK_EXT_debug_utils

	PFN_vkCmdBeginDebugUtilsLabelEXT            CmdBeginDebugUtilsLabelEXT;          // NULL unless VK_EXT_debug_utils
	PFN_vkCmdEndDebugUtilsLabelEXT              CmdEndDebugUtilsLabelEXT;

	// surface PFNs (instance-level; needed by
	// Ral_CreateBackend's owned-instance path to create + later destroy
	// VkSurfaceKHR via the host surface callback).
	PFN_vkDestroySurfaceKHR                     DestroySurfaceKHR;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR    GetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR    GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormats2KHR   GetPhysicalDeviceSurfaceFormats2KHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;

	// device — core lifecycle
	PFN_vkDestroyDevice                         DestroyDevice;
	PFN_vkGetDeviceQueue                        GetDeviceQueue;
	PFN_vkDeviceWaitIdle                        DeviceWaitIdle;
	PFN_vkQueueWaitIdle                         QueueWaitIdle;   // used by Ral_WaitQueueIdle (vk_queue_wait_idle retarget)
	PFN_vkQueueSubmit                           QueueSubmit;     // v1 (kept; v2 path used by Ral_Submit)
	PFN_vkQueueSubmit2                          QueueSubmit2;    // core 1.3 (synchronization2)

	// device — memory
	PFN_vkAllocateMemory                        AllocateMemory;
	PFN_vkFreeMemory                            FreeMemory;
	PFN_vkMapMemory                             MapMemory;
	PFN_vkUnmapMemory                           UnmapMemory;
	PFN_vkFlushMappedMemoryRanges               FlushMappedMemoryRanges;
	PFN_vkInvalidateMappedMemoryRanges          InvalidateMappedMemoryRanges;

	// device — buffers / images / views / samplers
	PFN_vkCreateBuffer                          CreateBuffer;
	PFN_vkDestroyBuffer                         DestroyBuffer;
	PFN_vkGetBufferMemoryRequirements           GetBufferMemoryRequirements;
	PFN_vkBindBufferMemory                      BindBufferMemory;
	PFN_vkCreateImage                           CreateImage;
	PFN_vkDestroyImage                          DestroyImage;
	PFN_vkGetImageMemoryRequirements            GetImageMemoryRequirements;
	PFN_vkBindImageMemory                       BindImageMemory;
	PFN_vkCreateImageView                       CreateImageView;
	PFN_vkDestroyImageView                      DestroyImageView;
	PFN_vkCreateSampler                         CreateSampler;
	PFN_vkDestroySampler                        DestroySampler;

	// device — descriptors
	PFN_vkCreateDescriptorSetLayout             CreateDescriptorSetLayout;
	PFN_vkDestroyDescriptorSetLayout            DestroyDescriptorSetLayout;
	PFN_vkCreateDescriptorPool                  CreateDescriptorPool;
	PFN_vkDestroyDescriptorPool                 DestroyDescriptorPool;
	PFN_vkResetDescriptorPool                   ResetDescriptorPool;
	PFN_vkAllocateDescriptorSets                AllocateDescriptorSets;
	PFN_vkFreeDescriptorSets                    FreeDescriptorSets;
	PFN_vkUpdateDescriptorSets                  UpdateDescriptorSets;

	// device — commands
	PFN_vkCreateCommandPool                     CreateCommandPool;
	PFN_vkDestroyCommandPool                    DestroyCommandPool;
	PFN_vkResetCommandPool                      ResetCommandPool;
	PFN_vkAllocateCommandBuffers                AllocateCommandBuffers;
	PFN_vkFreeCommandBuffers                    FreeCommandBuffers;
	PFN_vkBeginCommandBuffer                    BeginCommandBuffer;
	PFN_vkEndCommandBuffer                      EndCommandBuffer;
	PFN_vkResetCommandBuffer                    ResetCommandBuffer;
	PFN_vkCmdCopyBuffer                         CmdCopyBuffer;
	PFN_vkCmdCopyBufferToImage                  CmdCopyBufferToImage;
	PFN_vkCmdCopyImageToBuffer                  CmdCopyImageToBuffer;
	PFN_vkCmdBlitImage                          CmdBlitImage;
	PFN_vkCmdFillBuffer                         CmdFillBuffer;
	PFN_vkCmdPipelineBarrier                    CmdPipelineBarrier;
	PFN_vkCmdSetViewport                        CmdSetViewport;
	PFN_vkCmdSetScissor                         CmdSetScissor;
	PFN_vkCmdSetDepthBias                       CmdSetDepthBias;
	PFN_vkCmdWriteTimestamp2                    CmdWriteTimestamp2;   // core 1.3 (synchronization2)
	PFN_vkCmdResetQueryPool                     CmdResetQueryPool;

	// device — fences
	PFN_vkCreateFence                           CreateFence;
	PFN_vkDestroyFence                          DestroyFence;
	PFN_vkGetFenceStatus                        GetFenceStatus;
	PFN_vkWaitForFences                         WaitForFences;
	PFN_vkResetFences                           ResetFences;

	// device — semaphores (binary + timeline; timeline ops are core 1.2)
	PFN_vkCreateSemaphore                       CreateSemaphore;
	PFN_vkDestroySemaphore                      DestroySemaphore;
	PFN_vkGetSemaphoreCounterValue              GetSemaphoreCounterValue;
	PFN_vkSignalSemaphore                       SignalSemaphore;
	PFN_vkWaitSemaphores                        WaitSemaphores;

	// device — query pools
	PFN_vkCreateQueryPool                       CreateQueryPool;
	PFN_vkDestroyQueryPool                      DestroyQueryPool;
	PFN_vkResetQueryPool                        ResetQueryPool;       // host-side reset, core 1.2
	PFN_vkGetQueryPoolResults                   GetQueryPoolResults;

	// device — pipelines
	PFN_vkCreateShaderModule                    CreateShaderModule;
	PFN_vkDestroyShaderModule                   DestroyShaderModule;
	PFN_vkCreatePipelineLayout                  CreatePipelineLayout;
	PFN_vkDestroyPipelineLayout                 DestroyPipelineLayout;
	PFN_vkCreateGraphicsPipelines               CreateGraphicsPipelines;
	PFN_vkCreateComputePipelines                CreateComputePipelines;
	PFN_vkDestroyPipeline                       DestroyPipeline;
	PFN_vkCreatePipelineCache                   CreatePipelineCache;
	PFN_vkDestroyPipelineCache                  DestroyPipelineCache;
	PFN_vkGetPipelineCacheData                  GetPipelineCacheData;

	// device — pipeline-dependent cmd ops
	PFN_vkCmdBindPipeline                       CmdBindPipeline;
	PFN_vkCmdBindDescriptorSets                 CmdBindDescriptorSets;
	PFN_vkCmdBindVertexBuffers                  CmdBindVertexBuffers;
	PFN_vkCmdBindIndexBuffer                    CmdBindIndexBuffer;
	PFN_vkCmdPushConstants                      CmdPushConstants;
	PFN_vkCmdDraw                               CmdDraw;
	PFN_vkCmdDrawIndexed                        CmdDrawIndexed;
	PFN_vkCmdDrawIndexedIndirect                CmdDrawIndexedIndirect;
	PFN_vkCmdDrawIndexedIndirectCount           CmdDrawIndexedIndirectCount;   // gated on caps.drawIndirectCount
	PFN_vkCmdDispatch                           CmdDispatch;
	PFN_vkCmdDispatchIndirect                   CmdDispatchIndirect;
	PFN_vkCmdBeginRendering                     CmdBeginRendering;              // core 1.3 (dynamic rendering)
	PFN_vkCmdEndRendering                       CmdEndRendering;

	// Vk-typed parallel-paths cmd forwarders. The renderer's
	// legacy qvk path still uses VkRenderPass / VkFramebuffer-based render
	// passes (not dynamic rendering), and ClearAttachments / NextSubpass /
	// CopyImage / WriteTimestamp (legacy non-2) are not yet covered by the
	// RAL surface but need parallel-paths support during the migration. Once
	// the legacy path is retired these can either stay (for code that
	// still uses VkRenderPass) or be retired alongside.
	PFN_vkCmdCopyImage                          CmdCopyImage;
	PFN_vkCmdClearAttachments                   CmdClearAttachments;
	PFN_vkCmdWriteTimestamp                     CmdWriteTimestamp;             // legacy non-sync2; matches renderer's qvkCmdWriteTimestamp

	// swapchain + HDR-metadata function
	// pointers. VK_KHR_swapchain device extension is enabled by the renderer
	// at device creation; imported-mode RAL backend inherits its enabled state.
	// SetHdrMetadataEXT is gated on VK_EXT_hdr_metadata presence — NULL when
	// the extension wasn't enabled (caller null-checks before invocation).
	PFN_vkCreateSwapchainKHR                    CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR                   DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR                 GetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR                   AcquireNextImageKHR;
	PFN_vkQueuePresentKHR                       QueuePresentKHR;
	PFN_vkSetHdrMetadataEXT                     SetHdrMetadataEXT;             // NULL unless VK_EXT_hdr_metadata enabled
} ralVkFuncs_t;

#define RAL_VK_INVALID_FAMILY  0xFFFFFFFFu

// Round-trip a Vulkan non-dispatchable handle through a uint64_t (used by the
// deferred-destroy ring). On a 64-bit build the handle is a pointer; on a
// 32-bit build it's already a uint64_t typedef.
#if defined( VK_USE_64_BIT_PTR_DEFINES ) && VK_USE_64_BIT_PTR_DEFINES == 1
#  define RAL_VK_H2U( h )         ( (uint64_t)(uintptr_t)(h) )
#  define RAL_VK_U2H( type, u )   ( (type)(uintptr_t)(u) )
#else
#  define RAL_VK_H2U( h )         ( (uint64_t)(h) )
#  define RAL_VK_U2H( type, u )   ( (type)(u) )
#endif

// ── refcount header embedded in every RAL resource handle ───────────────
typedef struct {
	uint32_t refCount;
	uint64_t lastUsedFrame;          // for the deferred-destroy queue
} ralResourceHeader_t;

// ── memory suballocator ─────────────────────────────────────────────────
// Ordinary resources may occupy aligned slices of reusable, per-memory-type
// buffer/image blocks. Large, transient-alias and lazy allocations remain
// dedicated. The backend owns live slice and block lists for accounting.
//
// Contract: the VkDeviceMemory must outlive the VkBuffer/VkImage bound to it.
// Resources destroy themselves (vkDestroyBuffer / vkDestroyImage) and *then*
// ralVk_Free their allocation. ralVkAllocation_t does not back-reference the
// bound resource.
typedef struct {
	VkImageType imageType;
	VkFormat format;
	VkExtent3D extent;
	uint32_t mipLevels;
	uint32_t arrayLayers;
	VkSampleCountFlagBits samples;
	VkImageUsageFlags usage;
	VkSharingMode sharingMode;
	uint32_t queueFamilyIndexCount;
	uint32_t queueFamilyIndices[3];
	uint32_t memoryTypeBits;
} ralVkTransientImageKey_t;

typedef enum {
	RAL_VK_ALLOC_RESOURCE_BUFFER = 1,
	RAL_VK_ALLOC_RESOURCE_IMAGE,
	RAL_VK_ALLOC_RESOURCE_TRANSIENT
} ralVkAllocationResourceKind_t;

typedef struct ralVkMemoryBlock_s {
	ralBackend_t *backend;
	VkDeviceMemory memory;
	VkDeviceSize size;
	uint32_t memoryTypeIndex;
	VkMemoryPropertyFlags propertyFlags;
	ralVkAllocationResourceKind_t resourceKind;
	uint64_t generation;
	ralSuballocator_t allocator;
	void *mappedBase;
	uint32_t mapCount;
	struct ralVkMemoryBlock_s *next;
} ralVkMemoryBlock_t;

struct ralVkAllocation_s {
	ralBackend_t          *backend;         // owning backend (for device + dispatch)
	VkDeviceMemory         memory;
	VkDeviceSize           size;
	VkDeviceSize           offset;
	uint32_t               memoryTypeIndex;
	VkMemoryPropertyFlags  propertyFlags;   // the type's actual flags (may exceed what was requested)
	void                  *mapped;          // NULL when not mapped (host-visible only)
	ralVkMemoryBlock_t    *block;           // non-NULL for reusable-block slices
	ralSuballocationReceipt_t suballocation;
	ralAllocationReceipt_t receipt;         // native-free placement/pressure authority
	qboolean               transientAlias;  // allocation backs one physical transient slot
	ralVkTransientImageKey_t transientKey;  // exact image cohort, checked again before bind
	struct ralVkAllocation_s *next;         // backend's live-allocation list
};
typedef struct ralVkAllocation_s ralVkAllocation_t;

// ── resource handle structs (forward-declared opaque in ral_types.h) ────
struct ralBuffer_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkBuffer            buffer;
	ralVkAllocation_t  *alloc;
	VkDeviceSize        size;
	ralMemoryType_t     memoryType;
	ralBufferUsage_t    usage;
	qboolean            hostVisible;     // can be mapped
	qboolean            coherent;        // skip flush
	qboolean            ownsBuffer;      // qtrue: Ral_CreateBuffer made the VkBuffer (RAL destroys it).
	                                     // qfalse: adopted via Ral_AdoptBuffer (engine owns it; RAL
	                                     // destroys only the wrapper, never the VkBuffer/memory).
	qboolean            portableStateKnown;
	ralResourceState_t  portableState;
	ralQueueType_t      portableOwnerQueue;
	ralQueueTransferLifecycle_t queueTransfer;
	ralBufferMapLifecycle_t mapLifecycle;
	qboolean            legacyMapped;
};

static inline qboolean ralVk_BufferGpuUseAllowed( const ralBuffer_t *buffer ) {
	return buffer && !buffer->legacyMapped
		&& !buffer->queueTransfer.pending.ready
		&& Ral_BufferMapLifecycleGpuUseAllowed( &buffer->mapLifecycle );
}

struct ralTexture_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkImage             image;
	ralVkAllocation_t  *alloc;
	VkImageView         defaultView;     // full mip + array range, viewType from `type`
	VkFormat            vkFormat;
	ralFormat_t         ralFormat;
	ralTextureType_t    type;
	ralTextureUsage_t   usage;
	uint32_t            width, height, depthOrArrayLayers;
	uint32_t            mipLevels;
	uint32_t            arrayLayers;     // 1 for non-array (cube = 6); resolved layer count for image views
	uint32_t            sampleCount;
	uint64_t            resourceGeneration;
	VkImageAspectFlags  aspect;          // COLOR or DEPTH(+STENCIL)
	VkImageLayout       currentLayout;   // single layout tracked for the whole image (simplification)
	qboolean            ownsImage;       // qtrue if Ral_CreateTexture owns the VkImage + VkImageView + alloc, qfalse if adopted via Ral_AdoptTexture (caller retains lifetime; Ral_DestroyTexture skips defer-destroy of the underlying image / view / memory).
	qboolean            transientCohortOwned; // lifecycle belongs to ralTransientTextureCohort_t; ordinary DestroyTexture must not retire it independently.
	VkMemoryRequirements transientRequirements;
	ralVkTransientImageKey_t transientKey;
	qboolean            concurrentTransfer;   // qtrue when created CONCURRENT graphics+transfer — an upload copy on the transfer queue needs no ownership-transfer barrier.
	qboolean            portableStateKnown;
	ralResourceState_t  portableState;
	ralQueueType_t      portableOwnerQueue;
	ralQueueTransferLifecycle_t queueTransfer;
	// Per-array-layer attachment views. Direct 2D-array attachment textures allocate
	// and own this array; adopted arrays borrow caller views. Ral_BeginRendering
	// selects layerViews[depthAttachmentLayerIndex], while defaultView remains the
	// full-array sampling view.
	const VkImageView  *layerViews;      // NULL = single-layer (bind defaultView)
	uint32_t            numLayerViews;   // 0 = none; else == arrayLayers
};

struct ralTextureView_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkImageView         view;
	const ralTexture_t *texture;
	qboolean            ownsView;
};

struct ralSampler_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkSampler           sampler;
	qboolean            ownsSampler;       // qtrue if Ral_CreateSampler owns the VkSampler, qfalse if adopted via Ral_AdoptSampler (caller retains lifetime; Ral_DestroySampler skips defer-destroy of the underlying sampler).
};

#define RAL_VK_MAX_LAYOUT_ENTRIES 16
#define RAL_VK_MAX_DYNAMIC_OFFSETS 16u
#define RAL_VK_MAX_TRACKED_BIND_GROUP_BUFFERS 64u
#define RAL_VK_MAX_TRACKED_VERTEX_BUFFERS     16u
#define RAL_VK_MAX_TRACKED_BIND_GROUPS         8u

typedef struct {
	uint32_t          binding;
	VkDescriptorType  vkType;
	uint32_t          count;             // as supplied (0 = unbounded bindless array)
	uint32_t          effectiveCount;    // resolved (caps.maxBindlessTextures for the unbounded one)
	qboolean          dynamicOffset;
} ralVkBindEntry_t;

struct ralBindGroupArena_s {
	ralBackend_t *backend;
	VkDescriptorPool pool;
	ralBindGroupArenaLifecycle_t lifecycle;
};

struct ralBindGroupLayout_s {
	ralResourceHeader_t   header;
	ralBackend_t         *backend;
	VkDescriptorSetLayout layout;
	qboolean              bindless;
	qboolean              ownsLayout;        // qtrue if Ral_CreateBindGroupLayout owns the VkDescriptorSetLayout, qfalse if adopted via Ral_AdoptBindGroupLayout (caller retains ownership; Ral_DestroyBindGroupLayout skips vkDestroyDescriptorSetLayout).
	uint32_t              numEntries;
	uint32_t              dynamicOffsetCount;
	ralVkBindEntry_t      entries[RAL_VK_MAX_LAYOUT_ENTRIES];
};

typedef struct {
	uint32_t           binding;
	VkDescriptorType   vkType;
	const ralBuffer_t *buffer;
	uint64_t           baseOffset;
	uint64_t           range;
	qboolean           registered;
} ralVkDynamicBufferBinding_t;

struct ralBindGroup_s {
	ralResourceHeader_t         header;
	ralBackend_t               *backend;
	VkDescriptorSet             set;             // freed via vkFreeDescriptorSets (pool has FREE_DESCRIPTOR_SET_BIT) on deferred-destroy
	const ralBindGroupLayout_t *layout;
	qboolean                    ownsSet;         // qtrue if Ral_CreateBindGroup owns the VkDescriptorSet, qfalse if adopted via Ral_AdoptBindGroup (caller's pool retains ownership; Ral_DestroyBindGroup skips vkFreeDescriptorSets).
	const ralBindGroupArena_t  *arena;           // non-NULL for generation-bound arena allocations
	ralBindGroupArenaReceipt_t  arenaReceipt;
	// Weak references used only to enforce WebGPU's "mapped buffers are not
	// available to GPU commands" rule at bind and draw/dispatch time. Native
	// RAL groups publish a complete inventory. Adopted compatibility groups do
	// not have descriptor introspection, so their migration must explicitly
	// register buffers before this flag can become true.
	qboolean                    bufferTrackingComplete;
	uint32_t                    bufferCount;
	const ralBuffer_t          *buffers[ RAL_VK_MAX_TRACKED_BIND_GROUP_BUFFERS ];
	uint32_t                    dynamicOffsetCount;
	ralVkDynamicBufferBinding_t dynamicBindings[ RAL_VK_MAX_DYNAMIC_OFFSETS ];
};

static inline qboolean ralVk_BindGroupArenaLive( const ralBindGroup_t *group ) {
	ralBindGroupArenaReceipt_t current;
	if ( !group ) return qfalse;
	if ( !group->arena ) return qtrue;
	return Ral_GetBindGroupArenaReceipt( group->arena, &current ) == ralSuccess
		&& Ral_BindGroupArenaReceiptExact( &current, &group->arenaReceipt );
}

static inline qboolean ralVk_BindGroupBuffersGpuUseAllowed( const ralBindGroup_t *group ) {
	uint32_t i;
	if ( !group ) return qfalse;
	// Compatibility bridge only. The legacy Vulkan descriptor owner remains
	// authoritative until each adopted set publishes its exact buffer list.
	if ( !group->bufferTrackingComplete ) return qtrue;
	for ( i = 0; i < group->bufferCount; ++i ) {
		if ( !ralVk_BufferGpuUseAllowed( group->buffers[i] ) ) return qfalse;
	}
	return qtrue;
}

qboolean ralVk_CmdBindBindGroupDynamic( ralCommandBuffer_t *cb, uint32_t setIndex,
ralBindGroup_t *group, const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount );
qboolean ralVk_CmdBindBindGroupDynamicExact( ralCommandBuffer_t *cb, uint32_t setIndex,
ralBindGroup_t *group, const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount );
qboolean ralVk_ValidateBindGroupDynamicExact( const ralCommandBuffer_t *cb,
const ralPipeline_t *pipeline, uint32_t setIndex, const ralBindGroup_t *group,
const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount );

struct ralFence_s {
	ralBackend_t *backend;
	VkFence       fence;          // VK_NULL_HANDLE when preSignaled
	qboolean      preSignaled;    // legacy token form; later uploads return real (already-signaled) fences
	qboolean      ownsFence;      // qtrue if Ral_CreateFence owns the VkFence, qfalse if adopted via Ral_AdoptFence (caller retains lifetime; Ral_DestroyFence skips defer-destroy of the underlying fence).
};

struct ralSemaphore_s {
	ralBackend_t      *backend;
	VkSemaphore        sem;
	ralSemaphoreType_t type;      // BINARY or TIMELINE — drives VkSemaphoreSubmitInfo.value handling
	qboolean           ownsSemaphore; // qtrue if Ral_CreateSemaphore owns the VkSemaphore, qfalse if adopted via Ral_AdoptSemaphore (caller retains lifetime; Ral_DestroySemaphore skips defer-destroy of the underlying semaphore).
};

struct ralQueryPool_s {
	ralBackend_t  *backend;
	VkQueryPool    pool;
	ralQueryType_t type;
	uint32_t       count;
};

// ── pipeline + pipeline-layout cache ────────────────────────────────────
// Pipelines own a VkPipeline + a refcounted VkPipelineLayout drawn from the
// backend's small layout cache (one VkPipelineLayout per distinct combination
// of {bindGroupLayouts[], pushConstantSize, pushConstantStages}). Caches help
// the renderer migration avoid re-creating identical layouts when
// every shader variant for the same bind-set lineage gets its own pipeline.
#define RAL_VK_MAX_PIPELINE_SETS   8u    // per-pipeline VkDescriptorSetLayouts (Vulkan min maxBoundDescriptorSets=4; 8 is generous)
#define RAL_VK_LAYOUT_CACHE_MAX  256u    // distinct (set-layouts × push-constants) tuples cached in ralBackend_s.layoutCache
#define RAL_VK_MAX_EXTERNAL_PUSH_RANGES 4u

typedef struct {
	uint32_t              numSetLayouts;
	VkDescriptorSetLayout setLayouts[ RAL_VK_MAX_PIPELINE_SETS ];   // raw VkDescriptorSetLayout handles (key)
	uint32_t              pushConstantSize;                          // bytes (key)
	uint32_t              pushConstantStages;                        // VkShaderStageFlags (key)
	uint64_t              hash;                                      // fast-reject prefilter
	VkPipelineLayout      layout;                                    // value
	uint32_t              refCount;                                  // pipelines using this entry; 0 → defer-destroy
} ralVkLayoutCacheEntry_t;

struct ralPipeline_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkPipeline          pipeline;
	VkPipelineLayout    layout;             // borrowed from layoutCache[layoutCacheIndex] (don't destroy directly)
	uint32_t            layoutCacheIndex;   // index into ralBackend_s.layoutCache[]; ~0u if no cache entry (shouldn't happen)
	VkPipelineBindPoint bindPoint;          // VK_PIPELINE_BIND_POINT_GRAPHICS / _COMPUTE
	uint32_t            pushConstantOffset; // exact portable range base
	uint32_t            pushConstantSize;   // bytes (host-side, for validation in Ral_CmdPushConstants)
	uint32_t            pushConstantStages; // VkShaderStageFlags
	qboolean            bindGroupLayoutsRegistered;
	uint32_t            numSetLayouts;
	VkDescriptorSetLayout setLayouts[ RAL_VK_MAX_PIPELINE_SETS ];
	uint32_t            optionalBindGroupMask;
	qboolean            hasSemanticKey;
	ralShaderPipelineKey_t semanticKey;
	char                debugName[64];
};

// typed wrappers around renderer-owned VkPipelineLayout /
// VkRenderPass / VkFramebuffer. ownsHandle=qfalse on all wrappers created by
// the renderer's adoption helpers — Ral_Destroy* frees only the wrapper struct,
// the underlying Vk handle's lifetime stays with vk.c's existing teardown path.
struct ralPipelineLayout_s {
	ralBackend_t     *backend;
	VkPipelineLayout  vkHandle;
	qboolean          ownsHandle;
	qboolean          portableShapeKnown;
	uint32_t          numBindGroupLayouts;
	VkDescriptorSetLayout bindGroupLayouts[ RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS ];
	uint32_t          pushConstantSize;
	uint32_t          pushConstantStages;
	uint32_t          externalPushRangeCount;
	struct {
		uint32_t stageFlags; // portable RAL_STAGE_* authority
		uint32_t offset;
		uint32_t size;
	} externalPushRanges[ RAL_VK_MAX_EXTERNAL_PUSH_RANGES ];
};

// ── command-buffer wrapper ──────────────────────────────────────────────
typedef enum {
	RAL_VK_CMD_IDLE,            // freshly acquired (or pool-reset)
	RAL_VK_CMD_RECORDING,       // between Ral_BeginCommandBuffer and Ral_EndCommandBuffer
	RAL_VK_CMD_PENDING_SUBMIT,  // recorded, not yet submitted
	RAL_VK_CMD_SUBMITTED        // handed to a queue
} ralVkCmdState_t;

// RAL-side swapchain wrapper. References the backend-owned surface and owns its
// VkSwapchainKHR, dynamic image/view arrays, and canonical borrowed texture
// wrappers. HDR
// metadata cached in hdrMetadata + hasHdrMetadata flag (set by
// Ral_SetSwapchainHdrMetadata; replayed on swapchain recreate if needed).
typedef enum {
	RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE = 0,
	RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED,
	RAL_VK_SWAPCHAIN_IMAGE_PREPARED
} ralVkSwapchainImageState_t;

struct ralSwapchain_s {
	ralBackend_t    *backend;
	uint64_t         generation;
	VkSurfaceKHR     surface;          // borrowed from backend; backend owns its lifetime
	VkSwapchainKHR   swapchain;        // RAL-owned; created via b->vk.CreateSwapchainKHR; destroyed via b->vk.DestroySwapchainKHR
	VkFormat         vkFormat;
	ralFormat_t      format;
	VkColorSpaceKHR  vkColorSpace;
	ralColorSpace_t  colorSpace;
	VkPresentModeKHR vkPresentMode;
	ralPresentMode_t presentMode;
	VkExtent2D       extent;
	ralTextureUsage_t usage;
	uint32_t         requestedImageCount;
	uint32_t         imageCount;
	VkImage         *images;
	VkImageView     *imageViews;
	ralTexture_t   **adoptedImages;   // canonical wrappers; image/view remain swapchain-owned
	uint8_t          *imageStates;     // 0 available, 1 acquired, 2 prepared for present
	qboolean         hasHdrMetadata;
	VkHdrMetadataEXT hdrMetadata;
};

struct ralCommandBuffer_s {
	ralBackend_t       *backend;
	VkCommandBuffer     cb;
	ralQueueType_t      queue;             // which b->cmdPools[]/queues[] this came from
	ralVkCmdState_t     state;
	ralCommandLifecycle_t lifecycle;        // native-free generation-bound authority
	uint64_t            frame;             // currentFrame at submit time (for deferred-destroy association)
	// last Ral_CmdBindPipeline target — owns the VkPipelineLayout that bind-bind-group / push-constants / draw need.
	ralPipeline_t      *currentPipeline;   // weak ref (caller guarantees lifetime through Submit)
	VkPipelineLayout    currentLayout;     // mirror of currentPipeline->layout (also a weak ref)
	VkPipelineBindPoint currentBindPoint;  // mirror of currentPipeline->bindPoint
	// Weak refs mirror the current command-buffer binding state. Draw and
	// dispatch revalidate them immediately before native command emission so a
	// buffer mapped after an earlier bind cannot escape WebGPU exclusion.
	ralBuffer_t         *boundVertexBuffers[ RAL_VK_MAX_TRACKED_VERTEX_BUFFERS ];
	ralBuffer_t         *boundIndexBuffer;
	ralBindGroup_t      *boundBindGroups[ RAL_VK_MAX_TRACKED_BIND_GROUPS ];
	// Dynamic-rendering debug-label scope. The counters are reset for each
	// recording and increment only after real vkCmd*DebugUtilsLabelEXT calls.
	qboolean            renderingDebugLabelActive;
	qboolean            renderingActive;       // semantic transition commands are encoder/pass-boundary only
	uint32_t            debugLabelBeginCount;
	uint32_t            debugLabelEndCount;
	// parallel-paths adoption. When ownsBuffer == qfalse the
	// wrapper was created by Ral_AcquireBegunCommandBuffer around a renderer-owned
	// VkCommandBuffer; Ral_DestroyCommandBuffer skips vkFreeCommandBuffers
	// (the renderer's existing pool owns lifetime). Wrappers created by
	// Ral_AcquireCommandBuffer have ownsBuffer == qtrue (legacy RAL path).
	qboolean            ownsBuffer;
};

static inline qboolean ralVk_CommandBoundBuffersGpuUseAllowed( const ralCommandBuffer_t *cb ) {
	uint32_t i;
	if ( !cb ) return qfalse;
	for ( i = 0; i < RAL_VK_MAX_TRACKED_VERTEX_BUFFERS; ++i ) {
		if ( cb->boundVertexBuffers[i]
		  && !ralVk_BufferGpuUseAllowed( cb->boundVertexBuffers[i] ) ) return qfalse;
	}
	if ( cb->boundIndexBuffer && !ralVk_BufferGpuUseAllowed( cb->boundIndexBuffer ) ) return qfalse;
	for ( i = 0; i < RAL_VK_MAX_TRACKED_BIND_GROUPS; ++i ) {
		if ( cb->boundBindGroups[i]
		  && !ralVk_BindGroupBuffersGpuUseAllowed( cb->boundBindGroups[i] ) ) return qfalse;
	}
	return qtrue;
}

static inline ralResult_t ralVk_TransitionWholeBuffer( ralCommandBuffer_t *cb,
	                                                    ralBuffer_t *buffer,
	                                                    ralResourceUsage_t before,
	                                                    ralResourceUsage_t after ) {
	ralBufferTransition_t transition;
	ralResourceTransitionBatch_t batch;
	if ( !cb || !buffer ) return ralErrorInvalidArgument;
	memset( &transition, 0, sizeof( transition ) );
	transition.buffer = buffer;
	transition.size = (uint64_t)buffer->size;
	transition.before.usage = before;
	transition.after.usage = after;
	transition.sourceQueue = cb->queue;
	transition.destinationQueue = cb->queue;
	memset( &batch, 0, sizeof( batch ) );
	batch.bufferTransitions = &transition;
	batch.bufferTransitionCount = 1u;
	return Ral_CmdTransitionResources( cb, &batch );
}

static inline void *ralVk_MapReadbackBuffer( ralBuffer_t *buffer,
	                                         ralBufferMapTicket_t *ticket ) {
	ralBufferMapRequest_t request;
	if ( !buffer || !ticket ) return NULL;
	memset( &request, 0, sizeof( request ) );
	request.mode = RAL_MAP_READ;
	request.size = (uint64_t)buffer->size;
	return Ral_BufferMapBegin( buffer, &request, ticket ) == ralSuccess
	     ? ticket->mappedRange : NULL;
}

// ── deferred-destroy queue (lifecycle) ──────────────────────────────────
#define RAL_VK_MAX_FRAMES_IN_FLIGHT  2
#define RAL_VK_PENDING_DESTROY_MAX   4096   // ring capacity; overflow forces a synchronous drain

typedef enum {
	RAL_RES_BUFFER,           // h1 = VkBuffer;            alloc freed
	RAL_RES_IMAGE_AND_VIEW,   // h1 = VkImage, h2 = VkImageView (default view); alloc freed
	RAL_RES_IMAGE_VIEW,       // h1 = VkImageView
	RAL_RES_SAMPLER,          // h1 = VkSampler
	RAL_RES_DESC_SET_LAYOUT,  // h1 = VkDescriptorSetLayout
	RAL_RES_DESC_SET,         // h1 = VkDescriptorSet (freed back to b->descriptorPool)
	RAL_RES_FENCE,            // h1 = VkFence
	RAL_RES_SEMAPHORE,        // h1 = VkSemaphore
	RAL_RES_QUERY_POOL,       // h1 = VkQueryPool
	RAL_RES_PIPELINE,         // h1 = VkPipeline
	RAL_RES_PIPELINE_LAYOUT,  // h1 = VkPipelineLayout     (layout cache refcount → 0)
	RAL_RES_CMD_BUFFER,       // h1 = VkCommandBuffer, h2 = ralQueueType_t (which cmdPool to free from)
	RAL_RES_ALLOCATION_ONLY   // no handle; alloc freed after aliased child images retire
} ralResourceKind_t;

typedef struct {
	ralResourceKind_t  kind;
	uint64_t           h1;            // primary backend handle (non-dispatchable → fits in u64 on 32/64-bit)
	uint64_t           h2;            // secondary handle (VkImageView for IMAGE_AND_VIEW), else 0
	ralVkAllocation_t *alloc;         // memory to free, or NULL
	uint64_t           destroyedAtFrame;
} ralVkPendingDestroy_t;

typedef struct ralVkLegacyShaderModuleNode_s {
	VkShaderModule module;
	struct ralVkLegacyShaderModuleNode_s *next;
} ralVkLegacyShaderModuleNode_t;

// ── concrete backend object ─────────────────────────────────────────────
struct ralBackend_s {
	ralBackendType_t  type;            // always RAL_BACKEND_VULKAN for this implementation
	uint32_t          flags;           // RAL_FLAG_DEBUG_LABELS
	uint32_t          instanceApiVersion;   // version the VkInstance was created at
	ralHostImports_t  host;            // value-copied host contract; never renderer-global
	void             *platformHandle;
	qboolean          allowAsyncTextureUploads;

	ralVkFuncs_t      vk;

	VkInstance        instance;
	VkPhysicalDevice  physicalDevice;
	VkDevice          device;
	VkPhysicalDeviceProperties       physProps;
	VkPhysicalDeviceMemoryProperties memProps;

	uint32_t          graphicsFamily;
	uint32_t          computeFamily;   // == graphicsFamily if no dedicated async-compute family
	uint32_t          transferFamily;  // == graphicsFamily if no dedicated async-transfer family
	VkQueue           graphicsQueue;
	VkQueue           computeQueue;     // == graphicsQueue if shared
	VkQueue           transferQueue;    // == graphicsQueue if shared

	VkDebugUtilsMessengerEXT debugMessenger;   // VK_NULL_HANDLE unless validation requested + available

	// RAL-owned surface (created via ralHostImports_t::createSurface
	// in Ral_CreateBackend's owned-instance path; VK_NULL_HANDLE in imported
	// mode where the renderer retains surface lifecycle).
	VkSurfaceKHR      surface;

	// fine-grained ownership flags replace the prior
	// monolithic ownsHandles. ownsInstance gates teardown of instance +
	// messenger + surface; ownsDevice gates teardown of device. Imported
	// mode sets both qfalse; standalone sets both qtrue; the owned-
	// instance/imported-device hybrid sets ownsInstance=qtrue + ownsDevice=
	// qfalse until a later change flips ownsDevice.
	qboolean          ownsInstance;
	qboolean          ownsDevice;

	// flat list of device extensions actually
	// enabled at vkCreateDevice time. Pointers refer to either the
	// caller-supplied string literals (bci.platformDeviceExtensions[])
	// or RAL-internal static literals (VK_KHR_swapchain,
	// VK_EXT_memory_budget). Owned: the *array of pointers* is malloc'd
	// in the owned-device branch and freed in ralVk_DestroyBackendInternal;
	// the strings themselves are NOT owned (string-literal storage).
	const char      **enabledDeviceExtensions;
	uint32_t          enabledDeviceExtensionCount;

	qboolean          haveDebugUtils;        // VK_EXT_debug_utils instance extension present + entry points loaded
	qboolean          haveMemoryBudget;      // VK_EXT_memory_budget device extension enabled
	qboolean          haveDescriptorIndexing;  // descriptor-indexing features enabled at device creation → bindless usable
	qboolean          haveSync2;             // synchronization2 feature enabled (required for vkQueueSubmit2, vkCmdWriteTimestamp2)
	qboolean          haveTimelineSemaphore;  // timelineSemaphore feature enabled (required)
	qboolean          haveSamplerAnisotropy;  // samplerAnisotropy core feature enabled
	qboolean          haveHostQueryReset;    // hostQueryReset feature enabled → vkResetQueryPool usable
	qboolean          haveDrawIndirectCount; // drawIndirectCount feature enabled → vkCmdDrawIndexedIndirectCount usable
	qboolean          haveFragmentShadingRate; // VK_KHR_fragment_shading_rate enabled AND pipelineFragmentShadingRate feature on → pipeline-static VRS legal
	qboolean          haveDepthClamp;        // depthClamp core feature enabled → pipeline depthClampEnable legal (read into caps.depthClamp by ralVk_FillCaps, which survives the caps memset)
	qboolean          haveWideLines;         // wideLines core feature enabled → pipeline lineWidth != 1.0 legal (read into caps.wideLines by ralVk_FillCaps)
	qboolean          haveVertexFragmentStores; // vertexPipelineStoresAndAtomics + fragmentStoresAndAtomics both enabled → shader image/SSBO stores legal (read into caps.vertexFragmentStores by ralVk_FillCaps)
	qboolean          haveIndependentBlend; // independentBlend core feature enabled; imported mode stays false without caller proof
	ralVkLegacyShaderModuleNode_t *legacyShaderModules;

	ralCaps_t         caps;

	// ── per-queue command pools + queues + serialization ──
	VkQueue           queues[3];             // indexed by ralQueueType_t; compute/transfer alias graphics if no dedicated family
	uint32_t          queueFamily[3];        // family index per queue type
	VkCommandPool     cmdPools[3];           // one per queue type, RESET_COMMAND_BUFFER_BIT; also used for one-shot upload/readback cmds
	void             *queueMutex[3];         // boxed CRITICAL_SECTION/pthread_mutex_t — guards pool alloc/free/reset + vkQueueSubmit2 for that queue
	ralSubmissionLifecycle_t submissionLifecycle[3]; // one monotonic submit authority per logical queue

	// ── per-frame lifecycle + deferred destroy ──
	uint64_t          currentFrame;          // advanced by Ral_BeginFrame
	uint64_t          nextSwapchainGeneration; // monotonically assigned to each fully materialized swapchain
	uint64_t          nextTextureGeneration; // monotonically assigned imported texture wrappers
	uint64_t          nextAllocationGeneration; // monotonic owned allocation receipts
	uint64_t          nextMemoryBlockGeneration;
	uint64_t          nextTransferGeneration; // monotonic staging upload/readback receipts
	ralMemoryFailureLedger_t memoryFailures;
	VkFence           frameFences[ RAL_VK_MAX_FRAMES_IN_FLIGHT ];   // signaled by Ral_EndFrame's empty submit; waited by Ral_BeginFrame
	ralVkPendingDestroy_t *pendingDestroy;   // malloc'd ring of RAL_VK_PENDING_DESTROY_MAX entries
	uint32_t          numPendingDestroy;

	// ── resource layer ──
	VkDescriptorPool  descriptorPool;        // one big pool, UPDATE_AFTER_BIND | FREE_DESCRIPTOR_SET
	ralVkAllocation_t *allocations;          // live-allocation list
	ralVkMemoryBlock_t *memoryBlocks;         // reusable buffer/image blocks
	uint32_t          numAllocations;
	VkDeviceSize      ralDeviceLocalBytes;   // sum of device-local allocation sizes (RAL's own footprint)
	VkDeviceSize      ralHostVisibleBytes;   // sum of host-visible allocation sizes
	uint8_t           formatBlitGen[ RAL_FORMAT_COUNT ];  // 1 if optimal-tiling format supports BLIT_SRC|BLIT_DST|SAMPLED → GPU mip gen ok

	// ── pipeline layer ──
	VkPipelineCache   pipelineCache;         // backend-wide VkPipelineCache; seeds VkPipeline creation, persisted via Ral_{Save,Load}PipelineCache
	ralVkLayoutCacheEntry_t *layoutCache;    // malloc'd array of RAL_VK_LAYOUT_CACHE_MAX entries
	uint32_t          numLayoutCache;        // live entries (entries with refCount > 0 OR not yet defer-destroyed)

	// ── memory-pressure polling ──
	ralPressureCallback_t pressureCb;
	void                 *pressureUser;
	ralPressureLevel_t    lastPressureLevel;
	void                 *pollThread;        // OS thread handle (HANDLE / boxed pthread_t); NULL when not running
	volatile int          pollThreadStop;
};

// Bounded, backend-owned log formatter.  A NULL sink is intentionally silent;
// backend correctness must never depend on diagnostic output being present.
void ralVk_Logf( const ralBackend_t *b, ralLogSeverity_t severity,
	             const char *fmt, ... ) FORMAT_PRINTF( 3, 4 );

// Keep call sites compact while every message is routed through the backend
// value-copy of ralHostImports_t.  Unlike renderercommon's R_LOG this macro
// has no global import/channel dependency; `b` is deliberately the explicit
// backend variable in each Vulkan RAL function.
#define RAL_VK_LOG( severity, ... ) ralVk_Logf( b, (ralLogSeverity_t)(severity), __VA_ARGS__ )
#define RAL_VK_LOG_ON( backend, severity, ... ) \
	ralVk_Logf( (backend), (ralLogSeverity_t)(severity), __VA_ARGS__ )

// ── once-per-method-per-process stub log ────────────────────────────────
// rilog-channel-mechanism Turn B — route through R_LOG on the renderer.ral
// channel; every TU that expands these macros must (and does) declare a
// file-scope rch_ral via R_LOG_DECLARE_CHANNEL.
#define RAL_STUB_ONCE( fnname, phase ) \
	do { \
		static qboolean ral_stub_logged_ = qfalse; \
		if ( !ral_stub_logged_ ) { \
			ral_stub_logged_ = qtrue; \
			RAL_VK_LOG( SEV_DEBUG, "stub: %s -- TODO Phase %s\n", (fnname), (phase) ); \
		} \
	} while ( 0 )

// once-per-call-site informational note (used for "running synchronously" etc.)
#define RAL_NOTE_ONCE( ... ) \
	do { \
		static qboolean ral_note_logged_ = qfalse; \
		if ( !ral_note_logged_ ) { ral_note_logged_ = qtrue; RAL_VK_LOG( SEV_DEBUG, __VA_ARGS__ ); } \
	} while ( 0 )

#define RAL_NOTE_ONCE_ON( backend, ... ) \
	do { \
		static qboolean ral_note_logged_ = qfalse; \
		if ( !ral_note_logged_ ) { ral_note_logged_ = qtrue; RAL_VK_LOG_ON( (backend), SEV_DEBUG, __VA_ARGS__ ); } \
	} while ( 0 )

#define RAL_ZERO( x )  memset( &(x), 0, sizeof( x ) )

// ── cross-TU internals ──────────────────────────────────────────────────
// ral_vulkan_caps.c
void     ralVk_FillCaps( ralBackend_t *b );

// ral_vulkan_memory.c — suballocator, pressure polling, OS mutexes
ralVkAllocation_t *ralVk_Alloc ( ralBackend_t *b, VkMemoryRequirements req,
	VkMemoryPropertyFlags props, ralAllocationClass_t memoryClass,
	ralAllocationResidency_t residency, uintptr_t ownerIdentity,
	ralVkAllocationResourceKind_t resourceKind );
void               ralVk_Free  ( ralBackend_t *b, ralVkAllocation_t *a );
void              *ralVk_Map   ( ralVkAllocation_t *a );
void               ralVk_Unmap ( ralVkAllocation_t *a );
void               ralVk_Flush ( ralVkAllocation_t *a, VkDeviceSize offset, VkDeviceSize size );
void               ralVk_Invalidate( ralVkAllocation_t *a, VkDeviceSize offset, VkDeviceSize size );
void               ralVk_StopPollThread( ralBackend_t *b );
qboolean           ralVk_InitQueueMutexes   ( ralBackend_t *b );   // creates queueMutex[0..2]
void               ralVk_DestroyQueueMutexes( ralBackend_t *b );
void               ralVk_QueueLock  ( ralBackend_t *b, ralQueueType_t q );
void               ralVk_QueueUnlock( ralBackend_t *b, ralQueueType_t q );

// ral_vulkan_resource.c — resource-layer init/shutdown + the \ral_dump resource test
qboolean ralVk_InitResourceLayer    ( ralBackend_t *b );
void     ralVk_ShutdownResourceLayer( ralBackend_t *b );
void     ralVk_RunResourceTest      ( ralBackend_t *b );
VkImageUsageFlags ralVk_TextureUsage( ralTextureUsage_t u );
uint32_t ralVk_FormatBPP( ralFormat_t f );
qboolean ralVk_FormatCopyFootprint( ralFormat_t format,
	uint32_t *blockWidth, uint32_t *blockHeight, uint32_t *bytesPerBlock );
VkFormatFeatureFlags ralVk_TextureUsageFormatFeatures( ralTextureUsage_t u );
VkFormatFeatureFlags ralVk_TextureFormatFeatures( ralTextureFormatFeatures_t features );
VkColorComponentFlags ralVk_ColorWriteMask( const ralColorBlendAttachment_t *blend );
qboolean ralVk_ColorBlendStatesSupported( const ralGraphicsPipelineCreateInfo_t *ci,
	                                       qboolean independentBlend );
qboolean ralVk_IndependentBlendEnabled( qboolean backendOwnsDevice,
	                                     qboolean requested, VkBool32 supported );

// ral_vulkan_command.c — queue submission helper + the \ral_dump async test
ralResult_t ralVk_QueueSubmit2( ralBackend_t *b, ralQueueType_t q, const VkSubmitInfo2 *si2, VkFence fence );
void     ralVk_RunAsyncTest ( ralBackend_t *b );

// ral_vulkan_pipeline.c — pipeline layout cache + the \ral_dump pipeline test
qboolean ralVk_InitPipelineLayer    ( ralBackend_t *b );
void     ralVk_ShutdownPipelineLayer( ralBackend_t *b );
// returns ~0u on failure; on success increments refCount for the matched/new entry.
uint32_t ralVk_GetOrCreatePipelineLayout( ralBackend_t *b,
                                          const VkDescriptorSetLayout *setLayouts, uint32_t numSetLayouts,
                                          uint32_t pushConstantSize, uint32_t pushConstantStages,
                                          VkPipelineLayout *outLayout );
void     ralVk_ReleasePipelineLayout    ( ralBackend_t *b, uint32_t layoutCacheIndex );  // refCount-- ; defer-destroy at 0
void     ralVk_RunPipelineTest          ( ralBackend_t *b );

// ral_vulkan_backend.c — frame lifecycle / deferred destroy / shared helpers
qboolean ralVk_InitFrameLayer     ( ralBackend_t *b );
void     ralVk_ShutdownFrameLayer ( ralBackend_t *b );
void     ralVk_DeferDestroy       ( ralBackend_t *b, ralResourceKind_t kind, uint64_t h1, uint64_t h2, ralVkAllocation_t *alloc );
void     ralVk_DrainPendingDestroy( ralBackend_t *b, uint64_t drainBeforeFrame );   // ~0ull → drain everything
qboolean ralVk_HasExtension       ( const VkExtensionProperties *exts, uint32_t count, const char *name );
void     ralVk_SetObjectName      ( ralBackend_t *b, uint64_t handle, VkObjectType type, const char *name );
void     ralVk_DestroyLegacyShaderModules( ralBackend_t *b );

// ── interop bridge (renderer migration) ─────────────────────────────────
// Renderervk needs raw VkImage / VkImageView / VkDevice handles for the
// parallel-paths migration model. These accessors are backend-internal —
// they let renderervk peek at RAL-managed Vulkan objects without round-
// tripping through the public RAL surface. Used by renderervk to populate
// diagnostic dumps (\ral_textures), and to bridge into the new
// descriptor-binding path. Not part of the v1 RAL surface.
VkImage     ralVk_GetTextureNativeImage    ( const ralTexture_t *tex );
VkImageView ralVk_GetTextureNativeImageView( const ralTexture_t *tex );
VkDevice    ralVk_GetBackendNativeDevice   ( const ralBackend_t *b   );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_VULKAN_INTERNAL_H
