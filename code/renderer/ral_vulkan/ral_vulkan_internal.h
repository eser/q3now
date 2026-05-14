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
// are never shared with the legacy Vulkan renderer (per phase-7-ral-design.md
// and the Phase 7.x briefs). It does reuse the engine's platform Vulkan loader
// entry point (ri.VK_GetInstanceProcAddr) and the engine's logging / allocation
// imports (the refimport_t `ri`), because the backend is statically linked into
// the same renderer DLL.

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
#include "../../renderercommon/tr_public.h"   // refimport_t, extern refimport_t ri

#include <string.h>
#include <stdlib.h>   // Phase 7.4c-pre: backend internal allocations use stdlib malloc/free (not ri.Malloc) so RAL state survives ri.FreeAll() inside R_InitImages.

#ifdef __cplusplus
extern "C" {
#endif

// ── per-backend Vulkan dispatch table ───────────────────────────────────
// Global-level fns come from ri.VK_GetInstanceProcAddr(VK_NULL_HANDLE, ...);
// instance-level from ri.VK_GetInstanceProcAddr(instance, ...); device-level
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

	// device — core lifecycle
	PFN_vkDestroyDevice                         DestroyDevice;
	PFN_vkGetDeviceQueue                        GetDeviceQueue;
	PFN_vkDeviceWaitIdle                        DeviceWaitIdle;
	PFN_vkQueueWaitIdle                         QueueWaitIdle;   // Phase 7.4c-submit-followup-present-1: used by Ral_WaitQueueIdle (BC-B vk_queue_wait_idle retarget)
	PFN_vkQueueSubmit                           QueueSubmit;     // v1 (kept; v2 path used by Ral_Submit)
	PFN_vkQueueSubmit2                          QueueSubmit2;    // core 1.3 (synchronization2)

	// device — memory
	PFN_vkAllocateMemory                        AllocateMemory;
	PFN_vkFreeMemory                            FreeMemory;
	PFN_vkMapMemory                             MapMemory;
	PFN_vkUnmapMemory                           UnmapMemory;
	PFN_vkFlushMappedMemoryRanges               FlushMappedMemoryRanges;

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

	// device — pipelines (Phase 7.3c)
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

	// device — pipeline-dependent cmd ops (Phase 7.3c)
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

	// Phase 7.4c-cmd — Vk-typed parallel-paths cmd forwarders. The renderer's
	// legacy qvk path still uses VkRenderPass / VkFramebuffer-based render
	// passes (not dynamic rendering), and ClearAttachments / NextSubpass /
	// CopyImage / WriteTimestamp (legacy non-2) are not yet covered by the
	// RAL surface but need parallel-paths support during the migration. Once
	// 7.4c-submit retires the legacy path these can either stay (for code that
	// still uses VkRenderPass) or be retired alongside.
	PFN_vkCmdBeginRenderPass                    CmdBeginRenderPass;
	PFN_vkCmdEndRenderPass                      CmdEndRenderPass;
	PFN_vkCmdNextSubpass                        CmdNextSubpass;
	PFN_vkCmdCopyImage                          CmdCopyImage;
	PFN_vkCmdClearAttachments                   CmdClearAttachments;
	PFN_vkCmdWriteTimestamp                     CmdWriteTimestamp;             // legacy non-sync2; matches renderer's qvkCmdWriteTimestamp

	// Phase 7.4c-submit-followup-present-1 — swapchain + HDR-metadata function
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

// ── refcount header embedded in every RAL resource handle (§7.1) ────────
typedef struct {
	uint32_t refCount;
	uint64_t lastUsedFrame;          // for the deferred-destroy queue (Phase 7.3)
} ralResourceHeader_t;

// ── memory suballocator ─────────────────────────────────────────────────
// 7.2: one VkDeviceMemory per resource (replaceable later; the API is what
// matters). The backend owns a linked list of live allocations for memory-
// budget accounting + leak detection at shutdown.
//
// Contract: the VkDeviceMemory must outlive the VkBuffer/VkImage bound to it.
// Resources destroy themselves (vkDestroyBuffer / vkDestroyImage) and *then*
// ralVk_Free their allocation. ralVkAllocation_t does not back-reference the
// bound resource.
struct ralVkAllocation_s {
	ralBackend_t          *backend;         // owning backend (for device + dispatch)
	VkDeviceMemory         memory;
	VkDeviceSize           size;
	uint32_t               memoryTypeIndex;
	VkMemoryPropertyFlags  propertyFlags;   // the type's actual flags (may exceed what was requested)
	void                  *mapped;          // NULL when not mapped (host-visible only)
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
	qboolean            hostVisible;     // can be mapped
	qboolean            coherent;        // skip flush
};

struct ralTexture_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkImage             image;
	ralVkAllocation_t  *alloc;
	VkImageView         defaultView;     // full mip + array range, viewType from `type`
	VkFormat            vkFormat;
	ralFormat_t         ralFormat;
	ralTextureType_t    type;
	uint32_t            width, height, depthOrArrayLayers;
	uint32_t            mipLevels;
	uint32_t            arrayLayers;     // 1 for non-array (cube = 6); resolved layer count for image views
	uint32_t            sampleCount;
	VkImageAspectFlags  aspect;          // COLOR or DEPTH(+STENCIL)
	VkImageLayout       currentLayout;   // single layout tracked for the whole image (7.2 simplification)
	qboolean            ownsImage;       // Phase 7.4c-submit-A4: qtrue if Ral_CreateTexture owns the VkImage + VkImageView + alloc, qfalse if adopted via Ral_AdoptTexture (caller retains lifetime; Ral_DestroyTexture skips defer-destroy of the underlying image / view / memory).
};

struct ralTextureView_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkImageView         view;
	const ralTexture_t *texture;
};

struct ralSampler_s {
	ralResourceHeader_t header;
	ralBackend_t       *backend;
	VkSampler           sampler;
};

#define RAL_VK_MAX_LAYOUT_ENTRIES 16

typedef struct {
	uint32_t          binding;
	VkDescriptorType  vkType;
	uint32_t          count;             // as supplied (0 = unbounded bindless array)
	uint32_t          effectiveCount;    // resolved (caps.maxBindlessTextures for the unbounded one)
} ralVkBindEntry_t;

struct ralBindGroupLayout_s {
	ralResourceHeader_t   header;
	ralBackend_t         *backend;
	VkDescriptorSetLayout layout;
	qboolean              bindless;
	qboolean              ownsLayout;        // Phase 7.4c-bindgroup-pre: qtrue if Ral_CreateBindGroupLayout owns the VkDescriptorSetLayout, qfalse if adopted via Ral_AdoptBindGroupLayout (caller retains ownership; Ral_DestroyBindGroupLayout skips vkDestroyDescriptorSetLayout).
	uint32_t              numEntries;
	ralVkBindEntry_t      entries[RAL_VK_MAX_LAYOUT_ENTRIES];
};

struct ralBindGroup_s {
	ralResourceHeader_t         header;
	ralBackend_t               *backend;
	VkDescriptorSet             set;             // freed via vkFreeDescriptorSets (pool has FREE_DESCRIPTOR_SET_BIT) on deferred-destroy
	const ralBindGroupLayout_t *layout;
	qboolean                    ownsSet;         // Phase 7.4c-bindgroup: qtrue if Ral_CreateBindGroup owns the VkDescriptorSet, qfalse if adopted via Ral_AdoptBindGroup (caller's pool retains ownership; Ral_DestroyBindGroup skips vkFreeDescriptorSets).
};

struct ralFence_s {
	ralBackend_t *backend;
	VkFence       fence;          // VK_NULL_HANDLE when preSignaled
	qboolean      preSignaled;    // legacy 7.2 token form; 7.3 uploads return real (already-signaled) fences
	qboolean      ownsFence;      // Phase 7.4c-submit-BC-C-min: qtrue if Ral_CreateFence owns the VkFence, qfalse if adopted via Ral_AdoptFence (caller retains lifetime; Ral_DestroyFence skips defer-destroy of the underlying fence).
};

struct ralSemaphore_s {
	ralBackend_t      *backend;
	VkSemaphore        sem;
	ralSemaphoreType_t type;      // BINARY or TIMELINE — drives VkSemaphoreSubmitInfo.value handling
	qboolean           ownsSemaphore; // Phase 7.4c-submit-BC-C-min: qtrue if Ral_CreateSemaphore owns the VkSemaphore, qfalse if adopted via Ral_AdoptSemaphore (caller retains lifetime; Ral_DestroySemaphore skips defer-destroy of the underlying semaphore).
};

struct ralQueryPool_s {
	ralBackend_t  *backend;
	VkQueryPool    pool;
	ralQueryType_t type;
	uint32_t       count;
	qboolean       ownsPool;             // Phase 7.4c-submit-A4: qtrue if Ral_CreateQueryPool owns the VkQueryPool, qfalse if adopted via Ral_AdoptQueryPool (caller retains lifetime; Ral_DestroyQueryPool skips defer-destroy of the underlying pool).
};

// ── pipeline + pipeline-layout cache (Phase 7.3c) ───────────────────────
// Pipelines own a VkPipeline + a refcounted VkPipelineLayout drawn from the
// backend's small layout cache (one VkPipelineLayout per distinct combination
// of {bindGroupLayouts[], pushConstantSize, pushConstantStages}). Caches help
// the renderer migration (Phase 7.4+) avoid re-creating identical layouts when
// every shader variant for the same bind-set lineage gets its own pipeline.
#define RAL_VK_MAX_PIPELINE_SETS   8u    // per-pipeline VkDescriptorSetLayouts (Vulkan min maxBoundDescriptorSets=4; 8 is generous)
#define RAL_VK_LAYOUT_CACHE_MAX  256u    // distinct (set-layouts × push-constants) tuples cached in ralBackend_s.layoutCache

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
	uint32_t            pushConstantSize;   // bytes (host-side, for validation in Ral_CmdPushConstants)
	uint32_t            pushConstantStages; // VkShaderStageFlags
};

// Phase 7.4c-submit-A2 — typed wrappers around renderer-owned VkPipelineLayout /
// VkRenderPass / VkFramebuffer. ownsHandle=qfalse on all wrappers created by
// the renderer's adoption helpers — Ral_Destroy* frees only the wrapper struct,
// the underlying Vk handle's lifetime stays with vk.c's existing teardown path.
struct ralPipelineLayout_s {
	ralBackend_t     *backend;
	VkPipelineLayout  vkHandle;
	qboolean          ownsHandle;
};

struct ralRenderPass_s {
	ralBackend_t  *backend;
	VkRenderPass   vkHandle;
	qboolean       ownsHandle;
};

struct ralFramebuffer_s {
	ralBackend_t   *backend;
	VkFramebuffer   vkHandle;
	qboolean        ownsHandle;
};

// ── command-buffer wrapper ──────────────────────────────────────────────
typedef enum {
	RAL_VK_CMD_IDLE,            // freshly acquired (or pool-reset)
	RAL_VK_CMD_RECORDING,       // between Ral_BeginCommandBuffer and Ral_EndCommandBuffer
	RAL_VK_CMD_PENDING_SUBMIT,  // recorded, not yet submitted
	RAL_VK_CMD_SUBMITTED        // handed to a queue
} ralVkCmdState_t;

// Phase 7.4c-submit-followup-present-1 — RAL-side swapchain wrapper. Adopts an
// externally-created VkSurfaceKHR (renderer-owned via ri.VK_CreateSurface;
// ownsSurface=qfalse) and owns its own VkSwapchainKHR + image array. Each
// swapchain image is wrapped in an adopted ralTexture_t (ownsImage=qfalse) so
// renderer-side reverse-lookups (vk_ral_lookup_texture) find them. HDR
// metadata cached in hdrMetadata + hasHdrMetadata flag (set by
// Ral_SetSwapchainHdrMetadata; replayed on swapchain recreate if needed).
#define MAX_RAL_SWAPCHAIN_IMAGES 8u   // matches renderer-side MAX_SWAPCHAIN_IMAGES (vk.h:11)

struct ralSwapchain_s {
	ralBackend_t    *backend;
	VkSurfaceKHR     surface;          // adopted from renderer; lifecycle stays with renderer's ri.VK_CreateSurface / qvkDestroySurfaceKHR
	qboolean         ownsSurface;      // qfalse on adoption — Ral_DestroySwapchain does NOT call qvkDestroySurfaceKHR
	VkSwapchainKHR   swapchain;        // RAL-owned; created via b->vk.CreateSwapchainKHR; destroyed via b->vk.DestroySwapchainKHR
	VkFormat         vkFormat;
	VkColorSpaceKHR  vkColorSpace;
	VkPresentModeKHR vkPresentMode;
	VkExtent2D       extent;
	uint32_t         imageCount;
	VkImage          images[ MAX_RAL_SWAPCHAIN_IMAGES ];
	ralTexture_t    *adoptedImages[ MAX_RAL_SWAPCHAIN_IMAGES ];   // Ral_AdoptTexture wrappers (ownsImage=qfalse)
	qboolean         hasHdrMetadata;
	VkHdrMetadataEXT hdrMetadata;
};

struct ralCommandBuffer_s {
	ralBackend_t       *backend;
	VkCommandBuffer     cb;
	ralQueueType_t      queue;             // which b->cmdPools[]/queues[] this came from
	ralVkCmdState_t     state;
	uint64_t            frame;             // currentFrame at submit time (for deferred-destroy association)
	// last Ral_CmdBindPipeline target — owns the VkPipelineLayout that bind-bind-group / push-constants / draw need.
	ralPipeline_t      *currentPipeline;   // weak ref (caller guarantees lifetime through Submit)
	VkPipelineLayout    currentLayout;     // mirror of currentPipeline->layout (also a weak ref)
	VkPipelineBindPoint currentBindPoint;  // mirror of currentPipeline->bindPoint
	// Phase 7.4c-cmd: parallel-paths adoption. When ownsBuffer == qfalse the
	// wrapper was created by Ral_AcquireBegunCommandBuffer around a renderer-owned
	// VkCommandBuffer; Ral_DestroyCommandBuffer skips vkFreeCommandBuffers
	// (the renderer's existing pool owns lifetime). Wrappers created by
	// Ral_AcquireCommandBuffer have ownsBuffer == qtrue (legacy RAL path).
	qboolean            ownsBuffer;
	// Phase 7.4c-submit-A3: tracks whether a Ral_CmdBeginRenderPass succeeded.
	// Ral_CmdEndRenderPass / vkCmdEndRenderPass bails if false (matches the
	// NULL-fallthrough contract — when the parallel buffer's render-pass /
	// framebuffer lookup misses, Begin silently skips and End must too).
	qboolean            inRenderPass;
};

// ── deferred-destroy queue (§7.2 lifecycle) ─────────────────────────────
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
	RAL_RES_PIPELINE,         // h1 = VkPipeline           (Phase 7.3c)
	RAL_RES_PIPELINE_LAYOUT   // h1 = VkPipelineLayout     (Phase 7.3c — layout cache refcount → 0)
} ralResourceKind_t;

typedef struct {
	ralResourceKind_t  kind;
	uint64_t           h1;            // primary backend handle (non-dispatchable → fits in u64 on 32/64-bit)
	uint64_t           h2;            // secondary handle (VkImageView for IMAGE_AND_VIEW), else 0
	ralVkAllocation_t *alloc;         // memory to free, or NULL
	uint64_t           destroyedAtFrame;
} ralVkPendingDestroy_t;

// ── concrete backend object ─────────────────────────────────────────────
struct ralBackend_s {
	ralBackendType_t  type;            // always RAL_BACKEND_VULKAN for this implementation
	uint32_t          flags;           // RAL_FLAG_VALIDATION | RAL_FLAG_DEBUG_LABELS
	uint32_t          instanceApiVersion;   // version the VkInstance was created at

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
	qboolean          ownsHandles;           // Phase 7.4c-pre: qtrue → Ral_DestroyBackend tears down instance/device/debugMessenger; qfalse → caller owns them (imported mode)
	qboolean          haveDebugUtils;        // VK_EXT_debug_utils instance extension present + entry points loaded
	qboolean          haveMemoryBudget;      // VK_EXT_memory_budget device extension enabled
	qboolean          haveDescriptorIndexing;  // descriptor-indexing features enabled at device creation → bindless usable
	qboolean          haveSync2;             // synchronization2 feature enabled (required by 7.3+: vkQueueSubmit2, vkCmdWriteTimestamp2)
	qboolean          haveTimelineSemaphore;  // timelineSemaphore feature enabled (required by 7.3+)
	qboolean          haveSamplerAnisotropy;  // samplerAnisotropy core feature enabled
	qboolean          haveHostQueryReset;    // hostQueryReset feature enabled → vkResetQueryPool usable
	qboolean          haveDrawIndirectCount; // drawIndirectCount feature enabled → vkCmdDrawIndexedIndirectCount usable (Phase 7.4-pre)

	ralCaps_t         caps;

	// ── per-queue command pools + queues + serialization (Phase 7.3) ──
	VkQueue           queues[3];             // indexed by ralQueueType_t; compute/transfer alias graphics if no dedicated family
	uint32_t          queueFamily[3];        // family index per queue type
	VkCommandPool     cmdPools[3];           // one per queue type, RESET_COMMAND_BUFFER_BIT; also used for one-shot upload/readback cmds
	void             *queueMutex[3];         // boxed CRITICAL_SECTION/pthread_mutex_t — guards pool alloc/free/reset + vkQueueSubmit2 for that queue

	// ── per-frame lifecycle + deferred destroy (Phase 7.3) ──
	uint64_t          currentFrame;          // advanced by Ral_BeginFrame
	VkFence           frameFences[ RAL_VK_MAX_FRAMES_IN_FLIGHT ];   // signaled by Ral_EndFrame's empty submit; waited by Ral_BeginFrame
	ralVkPendingDestroy_t *pendingDestroy;   // malloc'd ring of RAL_VK_PENDING_DESTROY_MAX entries
	uint32_t          numPendingDestroy;

	// ── resource layer (Phase 7.2) ──
	VkDescriptorPool  descriptorPool;        // one big pool, UPDATE_AFTER_BIND | FREE_DESCRIPTOR_SET
	ralVkAllocation_t *allocations;          // live-allocation list
	uint32_t          numAllocations;
	VkDeviceSize      ralDeviceLocalBytes;   // sum of device-local allocation sizes (RAL's own footprint)
	VkDeviceSize      ralHostVisibleBytes;   // sum of host-visible allocation sizes
	uint8_t           formatBlitGen[ RAL_FORMAT_COUNT ];  // 1 if optimal-tiling format supports BLIT_SRC|BLIT_DST|SAMPLED → GPU mip gen ok

	// ── pipeline layer (Phase 7.3c) ──
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

// ── once-per-method-per-process stub log ────────────────────────────────
#define RAL_STUB_ONCE( fnname, phase ) \
	do { \
		static qboolean ral_stub_logged_ = qfalse; \
		if ( !ral_stub_logged_ ) { \
			ral_stub_logged_ = qtrue; \
			ri.Log( SEV_DEBUG, "[RAL] stub: %s -- TODO Phase %s\n", (fnname), (phase) ); \
		} \
	} while ( 0 )

// once-per-call-site informational note (used for "running synchronously" etc.)
#define RAL_NOTE_ONCE( ... ) \
	do { \
		static qboolean ral_note_logged_ = qfalse; \
		if ( !ral_note_logged_ ) { ral_note_logged_ = qtrue; ri.Log( SEV_DEBUG, __VA_ARGS__ ); } \
	} while ( 0 )

#define RAL_ZERO( x )  memset( &(x), 0, sizeof( x ) )

// ── cross-TU internals ──────────────────────────────────────────────────
// ral_vulkan_caps.c
void     ralVk_FillCaps( ralBackend_t *b );

// ral_vulkan_memory.c — suballocator, pressure polling, OS mutexes
ralVkAllocation_t *ralVk_Alloc ( ralBackend_t *b, VkMemoryRequirements req, VkMemoryPropertyFlags props );
void               ralVk_Free  ( ralBackend_t *b, ralVkAllocation_t *a );
void              *ralVk_Map   ( ralVkAllocation_t *a );
void               ralVk_Unmap ( ralVkAllocation_t *a );
void               ralVk_Flush ( ralVkAllocation_t *a, VkDeviceSize offset, VkDeviceSize size );
void               ralVk_StopPollThread( ralBackend_t *b );
qboolean           ralVk_InitQueueMutexes   ( ralBackend_t *b );   // creates queueMutex[0..2]
void               ralVk_DestroyQueueMutexes( ralBackend_t *b );
void               ralVk_QueueLock  ( ralBackend_t *b, ralQueueType_t q );
void               ralVk_QueueUnlock( ralBackend_t *b, ralQueueType_t q );

// ral_vulkan_resource.c — resource-layer init/shutdown + the \ral_dump resource test
qboolean ralVk_InitResourceLayer    ( ralBackend_t *b );
void     ralVk_ShutdownResourceLayer( ralBackend_t *b );
void     ralVk_RunResourceTest      ( ralBackend_t *b );

// ral_vulkan_command.c — queue submission helper + the \ral_dump async test
void     ralVk_QueueSubmit2 ( ralBackend_t *b, ralQueueType_t q, const VkSubmitInfo2 *si2, VkFence fence );
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
VkFormat ralVk_TranslateFormat    ( ralFormat_t f );

// ── Phase 7.4a interop bridge (renderer migration) ──────────────────────
// Renderervk needs raw VkImage / VkImageView / VkDevice handles for the
// parallel-paths migration model. These accessors are backend-internal —
// they let renderervk peek at RAL-managed Vulkan objects without round-
// tripping through the public RAL surface. Used by renderervk to populate
// diagnostic dumps (\ral_textures), and (in 7.4c+) to bridge into the new
// descriptor-binding path. Not part of the v1 RAL surface.
VkImage     ralVk_GetTextureNativeImage    ( const ralTexture_t *tex );
VkImageView ralVk_GetTextureNativeImageView( const ralTexture_t *tex );
VkDevice    ralVk_GetBackendNativeDevice   ( const ralBackend_t *b   );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_VULKAN_INTERNAL_H
