// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "tr_local.h"
#include "vk.h"
#include "vk_ral_textures.h"   // Phase 7.4a — parallel-paths RAL texture migration lifecycle
#include "../renderer/ral/ral.h"   // Phase 7.4c-pipeline — Ral_CreateGraphics/ComputePipeline + Ral_DestroyPipeline
#ifndef _WIN32
#include <pthread.h>
#endif
#include "smaa_area_texture.h"
#include "smaa_search_texture.h"
#include "../qcommon/q_feats.h"

#if FEAT_IQM
// Bone UBO size: 128 joints * 3 vec4 rows = 6144 bytes
// Must match IQM_MAX_JOINTS (128) from iqm.h and the shader's boneMats[128*3]
#define IQM_GPU_MAX_JOINTS 128
#define IQM_BONE_UBO_SIZE (IQM_GPU_MAX_JOINTS * 3 * sizeof(vec4_t))
#endif

#if defined (_DEBUG)
#if defined (_WIN32)
#define USE_VK_VALIDATION
#endif
#endif

#if defined( _WIN32 )
#include <windows.h> // win32 debug callback + Phase 6B3'-d8 HMONITOR / MonitorFromWindow
#include <dxgi1_6.h> // Phase 6B3'-d8 delta 3: IDXGIOutput6::GetDesc1 — read-only OS HDR-state diagnostic (no DXGI rendering interop)
// Phase 6B3'-d8: VK_EXT_full_screen_exclusive — the renderer DLL does not
// define VK_USE_PLATFORM_WIN32_KHR, so the vendored vulkan_win32.h (which
// carries these EXT structs) is not pulled in. Declare the few pieces we
// need here; stable ABI, and the VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_*
// enum values are already present in the vendored vulkan_core.h. Needed so
// vkGetPhysicalDeviceSurfaceFormats2KHR / vkCreateSwapchainKHR accept the
// FSE pNext chain on NVIDIA/Windows, which is what makes the HDR10 swapchain
// colorspace appear on the engine's window surface.
#ifndef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
#define VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME "VK_EXT_full_screen_exclusive"
typedef enum VkFullScreenExclusiveEXT {
	VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT = 0,
	VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT = 1,
	VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT = 2,
	VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT = 3,
} VkFullScreenExclusiveEXT;
typedef struct VkSurfaceFullScreenExclusiveInfoEXT {
	VkStructureType             sType;
	void                       *pNext;
	VkFullScreenExclusiveEXT    fullScreenExclusive;
} VkSurfaceFullScreenExclusiveInfoEXT;
typedef struct VkSurfaceFullScreenExclusiveWin32InfoEXT {
	VkStructureType    sType;
	const void        *pNext;
	HMONITOR           hmonitor;
} VkSurfaceFullScreenExclusiveWin32InfoEXT;
typedef VkResult (VKAPI_PTR *PFN_vkAcquireFullScreenExclusiveModeEXT)(VkDevice device, VkSwapchainKHR swapchain);
typedef VkResult (VKAPI_PTR *PFN_vkReleaseFullScreenExclusiveModeEXT)(VkDevice device, VkSwapchainKHR swapchain);
#endif // VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
#endif // _WIN32

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#if defined( _WIN32 )
// Phase 6B3'-d8: VK_EXT_full_screen_exclusive enabled at device creation?
// Gates whether the FSE pNext chain is attached to the surface-format query
// and to swapchain creation (needed to expose the HDR10 colorspace on the
// engine's win32 surface). qfalse on platforms / drivers without the ext —
// the SDR path is unaffected either way.
static qboolean vk_fse_ext_enabled = qfalse;
// Primary monitor handle, resolved at surface-query time. Used as the
// VkSurfaceFullScreenExclusiveWin32InfoEXT.hmonitor hint.
static HMONITOR vk_hmonitor = NULL;
#endif

#ifdef USE_VK_VALIDATION
static VkDebugUtilsMessengerEXT vk_debug_messenger = VK_NULL_HANDLE;
#endif

static int vk_diag_fence_ms, vk_diag_submit_ms, vk_diag_present_ms, vk_diag_acquire_ms, vk_diag_frames;
static int vk_diag_ft_fence_ms; // background fence thread: accumulated vkWaitForFences duration
static int vk_diag_drawcalls, vk_diag_pipebinds, vk_diag_msdf_draws, vk_diag_msdf_binds;
static qboolean vk_diag_msdf_active;

// Phase 7.4c-cmd — lifetime tallies for the per-frame adopted ralCommandBuffer_t
// and the per-frame rotating ralBindGroup_t wrappers. Tracked so the
// vk_ral_cmd_diag_dump helper (fires periodically and at shutdown) can confirm
// adoption is firing without needing a new diagnostic cvar.
static uint64_t vk_ral_cmd_adopt_count;        // ++ at every Ral_AcquireBegunCommandBuffer
static uint64_t vk_ral_descset_adopt_count;    // ++ at every per-frame ring Ral_AdoptBindGroup
static uint32_t vk_ral_cmd_last_dump_frame;    // throttling for the periodic SEV_DEBUG line

// Phase 7.4c-pipeline-followup-5 PART 3+4 — cold/warm boot timing markers.
// START captured at Ral_LoadPipelineCache call site (vk_initialize tail).
// END captured at frame-1 marker site (vk_end_frame's first-frame guard).
// Bracketed delta covers cache load → all init-time pipeline creation →
// first frame submit. SEV_INFO log lines stay permanently — durable
// observability of cold-vs-warm boot characteristics per §17.9 projection.
//
// RAL_CACHE_WARM_MIN_BYTES: anything below this size is treated as cold.
// Driver header alone is ~32 bytes; a meaningfully warm cache contains
// driver header + at least a few KB of compiled pipeline data. PART 1
// observed a real warm cache at ~700 KB on RTX 4070 Ti; the threshold is
// generously conservative at 4 KB so a freshly-created near-empty cache
// (e.g., aborted boot) doesn't get misread as warm.
#define RAL_CACHE_WARM_MIN_BYTES   4096u
static int      vk_pipeline_cache_load_start_ms = 0;
static qboolean vk_pipeline_cache_was_cold      = qtrue;
static uint64_t vk_pipeline_cache_size_at_load  = 0;

// Per-frame µs timestamps for r_frameSpikeUs profiler (single-threaded, main thread only).
static int64_t vk_frame_t_start;
static int64_t vk_frame_t_after_fence;
static int64_t vk_frame_t_after_acquire;
static int64_t vk_frame_t_after_begincb;
static int64_t vk_frame_t_rec_start;
static int64_t vk_frame_t_rec_end;
static int64_t vk_frame_t_submit_start;
static int64_t vk_frame_t_after_submit;
static int64_t vk_frame_t_present_start;
static int64_t vk_frame_t_after_present;
static qboolean vk_frame_present_done;

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFeatures2					qvkGetPhysicalDeviceFeatures2;  // Phase 7.4c-pre: 1.1 core
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormats2KHR		qvkGetPhysicalDeviceSurfaceFormats2KHR;	// Phase 6B3'-d8 — NULL unless VK_KHR_get_surface_capabilities2 enabled; needed to enumerate HDR colorspaces on NVIDIA/Windows
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugUtilsMessengerEXT				qvkCreateDebugUtilsMessengerEXT;
static PFN_vkDestroyDebugUtilsMessengerEXT				qvkDestroyDebugUtilsMessengerEXT;
#endif
static PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory								qvkAllocateMemory;
static PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory							qvkBindBufferMemory;
static PFN_vkBindImageMemory							qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage								qvkCmdBlitImage;
static PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
static PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage								qvkCmdCopyImage;
static PFN_vkCmdDispatch								qvkCmdDispatch;
static PFN_vkCmdDraw									qvkCmdDraw;
static PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
static PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants							qvkCmdPushConstants;
static PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
static PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor								qvkCmdSetScissor;
static PFN_vkCmdSetViewport								qvkCmdSetViewport;
static PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
static PFN_vkCreateBuffer								qvkCreateBuffer;
static PFN_vkCreateCommandPool							qvkCreateCommandPool;
static PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence								qvkCreateFence;
static PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
static PFN_vkCreateComputePipelines						qvkCreateComputePipelines;
static PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
static PFN_vkCreateImage								qvkCreateImage;
static PFN_vkCreateImageView							qvkCreateImageView;
static PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
static PFN_vkCreateQueryPool							qvkCreateQueryPool;
static PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
static PFN_vkCreateRenderPass							qvkCreateRenderPass;
static PFN_vkCreateSampler								qvkCreateSampler;
static PFN_vkCreateSemaphore							qvkCreateSemaphore;
static PFN_vkCreateShaderModule							qvkCreateShaderModule;
static PFN_vkDestroyBuffer								qvkDestroyBuffer;
static PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice								qvkDestroyDevice;
static PFN_vkDestroyFence								qvkDestroyFence;
static PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
static PFN_vkDestroyImage								qvkDestroyImage;
static PFN_vkDestroyImageView							qvkDestroyImageView;
static PFN_vkDestroyPipeline							qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
static PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
static PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
static PFN_vkDestroySampler								qvkDestroySampler;
static PFN_vkDestroySemaphore							qvkDestroySemaphore;
static PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
static PFN_vkFreeMemory									qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
static PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
static PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
static PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory									qvkMapMemory;
static PFN_vkQueueSubmit								qvkQueueSubmit;
static PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
static PFN_vkResetFences								qvkResetFences;
static PFN_vkUnmapMemory								qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
static PFN_vkWaitForFences								qvkWaitForFences;
static PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR							qvkQueuePresentKHR;
static PFN_vkSetHdrMetadataEXT							qvkSetHdrMetadataEXT;	// Phase 6B3'-d8 — NULL unless VK_EXT_hdr_metadata enabled

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );

// Phase 7.4c-pipeline-followup-2 — special-case pipeline helper.
// The struct definition lives here so the 15 special-case create sites
// throughout this file (the first is vk_init_beam at ~line 4900) can
// instantiate it on the stack. The function body is further down near
// the centralized create_pipeline() helper, just forward-declared here.
struct vk_ral_special_pipeline_params_s {
	VkShaderModule              vs_module;
	VkShaderModule              fs_module;

	ralPolygonMode_t            polygonMode;
	ralCullMode_t               cullMode;
	ralFrontFace_t              frontFace;
	qboolean                    depthBiasEnable;
	float                       depthBiasConstant;
	float                       depthBiasSlope;
	float                       lineWidth;

	qboolean                    depthTestEnable;
	qboolean                    depthWriteEnable;
	ralCompareOp_t              depthCompareOp;

	qboolean                    blendEnable;
	ralBlendFactor_t            srcColor, dstColor;
	ralBlendOp_t                blendOp;
	uint32_t                    colorWriteMask;

	ralPrimitiveTopology_t      topology;
	uint32_t                    sampleCount;

	uint32_t                    pushConstantSize;
	uint32_t                    pushConstantStages;
	const ralBindGroupLayout_t *bgls[ 8 ];
	uint32_t                    numBgls;

	const ralVertexBinding_t   *vbinds;
	uint32_t                    numVbinds;
	const ralVertexAttribute_t *vattrs;
	uint32_t                    numVattrs;

	ralFormat_t                 colorFormat;
	ralFormat_t                 depthFormat;
	uint32_t                    numColorAttachments;
	qboolean                    depthOnly;   // Phase 7.4c-submit-A3 — explicit "0 color attachments" signal for shadow caster (overrides the helper's default-to-1 fallback).

	// Phase 7.4c-pipeline-followup-4 — VkSpecializationInfo support.
	// Existing call sites zero-initialise via memset; numSpecConstants = 0
	// means the helper threads NULL/0 into the RAL create info, no-op.
	// Used by particle render (PIPELINE_BLEND_MASK), blur (3-float offsets),
	// post-process variants (gamma/tonemap/lottes/grading/HDR knobs).
	const ralSpecConstant_t    *specConstants;
	uint32_t                    numSpecConstants;

	// Phase 7.4c-submit-A3 — caller-provided pipeline layout. Threaded into
	// ralGraphicsPipelineCreateInfo_t.externalLayout so the RAL sibling
	// pipeline identity-shares its VkPipelineLayout with the renderer's
	// matching vk.<subsystem>.pipeline_layout legacy handle. NULL falls back
	// to RAL's layoutCache (legacy A2 behavior — produces layout-incompatible
	// bind/draw pairs on the parallel buffer; do not leave NULL when the
	// matching sibling exists).
	ralPipelineLayout_t        *externalLayout;

	// Phase 7.4c-submit-A3 — caller-provided render pass + subpass. Threaded
	// into ralGraphicsPipelineCreateInfo_t.externalRenderPass so the sibling
	// pipeline is created with the SAME VkRenderPass that the renderer's
	// vkCmdBeginRenderPass uses (rather than the RAL default of dynamic
	// rendering). Required for parallel-buffer pipeline-bind compatibility
	// inside legacy render pass instances.
	ralRenderPass_t            *externalRenderPass;
	uint32_t                    externalSubpass;        // 0 by default

	const char                 *debugName;
};
typedef struct vk_ral_special_pipeline_params_s vk_ral_special_pipeline_params_t;
static ralPipeline_t *vk_ral_create_special_pipeline( const vk_ral_special_pipeline_params_t *p );

qboolean vk_shader_blob_lookup( VkShaderModule handle, const uint8_t **out_bytes, uint32_t *out_size );

// Phase 7.4c-bindgroup — parallel-paths bind-side helper used at each of the
// ~25 qvkCmdBindDescriptorSets call sites. Looks up the adopted ralBindGroup_t
// for each VkDescriptorSet via vk_ral_lookup_bindgroup(), then records the
// same bind onto the same VkCommandBuffer via Ral_CmdBindBindGroups. The
// double-record is intentional: same descriptor set on same set index is an
// idempotent vkCmdBindDescriptorSets call. The parallel path stays inert until
// 7.4c-submit retires the legacy bind.
//
// Returns early (skip parallel record) when ANY set in vkSets[] isn't adopted,
// e.g. per-shader-type rotating descriptors in vk.cmd->descriptor_set.current[]
// — those need per-frame re-adoption tied to the cmd-buffer ring that
// 7.4c-cmd introduces. TODO_7.4c-cmd: rotating-set adoption + drop this guard.
static void vk_ral_parallel_bind_descriptor_sets( VkCommandBuffer cb,
                                                  VkPipelineBindPoint bindPoint,
                                                  VkPipelineLayout layout,
                                                  uint32_t firstSet,
                                                  uint32_t count,
                                                  const VkDescriptorSet *vkSets,
                                                  uint32_t dynOffCount,
                                                  const uint32_t *dynOffs )
{
	ralBackend_t   *b;
	ralBindGroup_t *rbg[ VK_DESC_COUNT ];
	void           *parallelCb;
	uint32_t        i;
	(void)cb;   // Phase 7.4c-cmd: legacy cmd buffer no longer used here — we
	            // record onto the RAL-allocated parallel cmd buffer instead.
	if ( layout == VK_NULL_HANDLE || count == 0 || count > VK_DESC_COUNT ) return;
	b = vk_ral_get_backend();
	if ( !b ) return;
	if ( !vk.cmd ) return;
	parallelCb = Ral_GetCommandBufferHandle( vk.cmd->ral_cmd );
	if ( !parallelCb ) return;
	for ( i = 0; i < count; i++ ) {
		rbg[i] = vk_ral_lookup_bindgroup( vkSets[i] );
		if ( !rbg[i] ) return;
	}
	Ral_CmdBindBindGroups( b, parallelCb, (int)bindPoint, (void *)layout,
	                       firstSet, count, rbg, dynOffCount, dynOffs );
}


// Phase 7.4c-submit-A3 — typed-BeginRenderPass dispatcher for the renderer's
// parallel buffer. Unpacks a VkRenderPassBeginInfo into the typed Ral_CmdBeginRenderPass
// args via the vk_ral_lookup_render_pass / vk_ral_lookup_framebuffer reverse
// lookups + a stack ralRect_t. If either lookup misses (renderpass / framebuffer
// not yet adopted), the call is silently skipped — legacy qvkCmdBeginRenderPass
// on the renderer's own buffer remains authoritative.
static void vk_ral_parallel_begin_render_pass( const VkRenderPassBeginInfo *bi )
{
	struct ralRenderPass_s  *rp;
	struct ralFramebuffer_s *fb;
	ralRect_t                area;
	if ( !vk.cmd || !vk.cmd->ral_cmd || !bi ) return;
	rp = vk_ral_lookup_render_pass ( bi->renderPass  );
	fb = vk_ral_lookup_framebuffer ( bi->framebuffer );
	if ( !rp || !fb ) return;
	area.x       = bi->renderArea.offset.x;
	area.y       = bi->renderArea.offset.y;
	area.width   = bi->renderArea.extent.width;
	area.height  = bi->renderArea.extent.height;
	// ralClearValue_t is layout-compatible with VkClearValue (color[4] / depthStencil union).
	Ral_CmdBeginRenderPass( vk.cmd->ral_cmd, rp, fb, &area,
		bi->clearValueCount, (const ralClearValue_t *)bi->pClearValues,
		RAL_SUBPASS_CONTENTS_INLINE );
}


// Phase 7.4c-submit-A4 — typed-PipelineBarrier dispatchers for the renderer's
// parallel buffer. Wrap the renderer's existing qvkCmdPipelineBarrier-style
// arrays of VkImageMemoryBarrier / VkBufferMemoryBarrier into the typed
// ralPipelineBarrierInfo_t + ralImageMemoryBarrier_t / ralBufferMemoryBarrier_t
// surface via vk_ral_lookup_texture / vk_ral_lookup_buffer. If ANY lookup
// misses (the texture/buffer wasn't adopted by Phase 7.4c-submit-A4's
// internal-texture sweep — depthFade.image is the known miss-by-design case)
// the typed call is skipped wholesale; legacy qvkCmdPipelineBarrier on the
// renderer's own buffer remains authoritative. SEV_WARN logging is gated by
// a once-per-callsite static flag to avoid spamming each frame.
//
// Bounds: VK_RAL_PARALLEL_MAX_BARRIERS caps the on-stack scratch — the
// renderer's existing callsite set uses at most 2 image barriers + 1 buffer
// barrier per call, so 8 covers each kind comfortably with growth headroom.
#define VK_RAL_PARALLEL_MAX_BARRIERS 8

static void vk_ral_parallel_pipeline_barrier_image( VkPipelineStageFlags srcStage,
                                                    VkPipelineStageFlags dstStage,
                                                    uint32_t count,
                                                    const VkImageMemoryBarrier *bs,
                                                    qboolean *warnedOnce,
                                                    const char *callsite )
{
	ralTexture_t            *rt[ VK_RAL_PARALLEL_MAX_BARRIERS ];
	ralImageMemoryBarrier_t  rb[ VK_RAL_PARALLEL_MAX_BARRIERS ];
	ralPipelineBarrierInfo_t info;
	uint32_t                 i;
	if ( !vk.cmd || !vk.cmd->ral_cmd || count == 0 || count > VK_RAL_PARALLEL_MAX_BARRIERS || !bs ) return;
	for ( i = 0; i < count; i++ ) {
		rt[i] = vk_ral_lookup_texture( bs[i].image );
		if ( !rt[i] ) {
			if ( warnedOnce && !*warnedOnce ) {
				*warnedOnce = qtrue;
				ri.Log( SEV_WARN, "[VK->RAL] vk_ral_lookup_texture miss at %s (image %u/%u not adopted); skipping typed PipelineBarrier (legacy qvkCmd path remains authoritative)\n",
				        callsite, i, count );
			}
			return;
		}
	}
	memset( rb, 0, sizeof( rb ) );
	for ( i = 0; i < count; i++ ) {
		rb[i].srcAccessMask       = bs[i].srcAccessMask;
		rb[i].dstAccessMask       = bs[i].dstAccessMask;
		rb[i].oldLayout           = (uint32_t)bs[i].oldLayout;
		rb[i].newLayout           = (uint32_t)bs[i].newLayout;
		rb[i].srcQueueFamilyIndex = bs[i].srcQueueFamilyIndex;
		rb[i].dstQueueFamilyIndex = bs[i].dstQueueFamilyIndex;
		rb[i].texture             = rt[i];
		rb[i].aspectMask          = (uint32_t)bs[i].subresourceRange.aspectMask;
		rb[i].baseMipLevel        = bs[i].subresourceRange.baseMipLevel;
		rb[i].levelCount          = bs[i].subresourceRange.levelCount;
		rb[i].baseArrayLayer      = bs[i].subresourceRange.baseArrayLayer;
		rb[i].layerCount          = bs[i].subresourceRange.layerCount;
	}
	memset( &info, 0, sizeof( info ) );
	info.srcStageMask            = (ralPipelineStageFlags_t)srcStage;
	info.dstStageMask            = (ralPipelineStageFlags_t)dstStage;
	info.imageMemoryBarrierCount = count;
	info.imageMemoryBarriers     = rb;
	Ral_CmdPipelineBarrierFull( vk.cmd->ral_cmd, &info );
}

static void vk_ral_parallel_pipeline_barrier_buffer( VkPipelineStageFlags srcStage,
                                                     VkPipelineStageFlags dstStage,
                                                     const VkBufferMemoryBarrier *bm,
                                                     qboolean *warnedOnce,
                                                     const char *callsite )
{
	ralBuffer_t              *rb;
	ralBufferMemoryBarrier_t  rbm;
	ralPipelineBarrierInfo_t  info;
	if ( !vk.cmd || !vk.cmd->ral_cmd || !bm ) return;
	rb = vk_ral_lookup_buffer( bm->buffer );
	if ( !rb ) {
		if ( warnedOnce && !*warnedOnce ) {
			*warnedOnce = qtrue;
			ri.Log( SEV_WARN, "[VK->RAL] vk_ral_lookup_buffer miss at %s; skipping typed PipelineBarrier (legacy qvkCmd path remains authoritative)\n", callsite );
		}
		return;
	}
	memset( &rbm, 0, sizeof( rbm ) );
	rbm.srcAccessMask       = bm->srcAccessMask;
	rbm.dstAccessMask       = bm->dstAccessMask;
	rbm.srcQueueFamilyIndex = bm->srcQueueFamilyIndex;
	rbm.dstQueueFamilyIndex = bm->dstQueueFamilyIndex;
	rbm.buffer              = rb;
	rbm.offset              = bm->offset;
	rbm.size                = bm->size;
	memset( &info, 0, sizeof( info ) );
	info.srcStageMask             = (ralPipelineStageFlags_t)srcStage;
	info.dstStageMask             = (ralPipelineStageFlags_t)dstStage;
	info.bufferMemoryBarrierCount = 1;
	info.bufferMemoryBarriers     = &rbm;
	Ral_CmdPipelineBarrierFull( vk.cmd->ral_cmd, &info );
}

// Phase 7.4c-submit-A4 — VkImageCopy region array is binary-compatible with
// ralImageCopy_t (matching offset/extent/subresource fields) so the cast is
// sound; the src/dst layouts are passed implicitly by Ral_CmdCopyImage which
// hard-codes TRANSFER_SRC_OPTIMAL / TRANSFER_DST_OPTIMAL (matching every
// renderer callsite — verified at 18278 and 18424).
static void vk_ral_parallel_copy_image( VkImage src, VkImage dst,
                                        uint32_t regionCount, const VkImageCopy *regions,
                                        qboolean *warnedOnce,
                                        const char *callsite )
{
	ralTexture_t *rsrc, *rdst;
	if ( !vk.cmd || !vk.cmd->ral_cmd || regionCount == 0 || !regions ) return;
	rsrc = vk_ral_lookup_texture( src );
	rdst = vk_ral_lookup_texture( dst );
	if ( !rsrc || !rdst ) {
		if ( warnedOnce && !*warnedOnce ) {
			*warnedOnce = qtrue;
			ri.Log( SEV_WARN, "[VK->RAL] vk_ral_lookup_texture miss at %s (src=%p dst=%p); skipping typed CopyImage\n",
			        callsite, (void *)rsrc, (void *)rdst );
		}
		return;
	}
	Ral_CmdCopyImage( vk.cmd->ral_cmd, rsrc, rdst, regionCount, (const ralImageCopy_t *)regions );
}

static void vk_ral_parallel_write_timestamp( VkPipelineStageFlags stage,
                                             VkQueryPool pool, uint32_t query,
                                             qboolean *warnedOnce,
                                             const char *callsite )
{
	ralQueryPool_t *rp;
	if ( !vk.cmd || !vk.cmd->ral_cmd ) return;
	rp = vk_ral_lookup_query_pool( pool );
	if ( !rp ) {
		if ( warnedOnce && !*warnedOnce ) {
			*warnedOnce = qtrue;
			ri.Log( SEV_WARN, "[VK->RAL] vk_ral_lookup_query_pool miss at %s; skipping typed WriteTimestamp\n", callsite );
		}
		return;
	}
	Ral_CmdWriteTimestamp( vk.cmd->ral_cmd, (uint32_t)stage, rp, query );
}

static void vk_ral_parallel_reset_query_pool( VkQueryPool pool,
                                              uint32_t firstQuery, uint32_t queryCount,
                                              qboolean *warnedOnce,
                                              const char *callsite )
{
	ralQueryPool_t *rp;
	if ( !vk.cmd || !vk.cmd->ral_cmd || queryCount == 0 ) return;
	rp = vk_ral_lookup_query_pool( pool );
	if ( !rp ) {
		if ( warnedOnce && !*warnedOnce ) {
			*warnedOnce = qtrue;
			ri.Log( SEV_WARN, "[VK->RAL] vk_ral_lookup_query_pool miss at %s; skipping typed ResetQueryPool\n", callsite );
		}
		return;
	}
	Ral_CmdResetQueryPool( vk.cmd->ral_cmd, rp, firstQuery, queryCount );
}


static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: failed to find matching memory type with requested properties" );
	return ~0U;
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}


static const char *pmode_to_str( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: sprintf( buf, "mode#%x", mode ); return buf;
	};
}


#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_R16G16B16A16_SFLOAT );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


static const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR


// Phase 9C: HDR pipeline state snapshot, captured during
// setup_surface_formats() and printed on demand by the `gfxinfo`
// console command (see vk_hdr_state_print() below). This replaces
// the per-vid_restart bloom diagnostic added in 9B — state-keeping
// + on-demand report instead of build-flag log spam.
typedef struct {
	qboolean valid;
	VkFormat base_format;
	VkFormat color_format;
	VkFormat bloom_format;
	VkFormat capture_format;
	VkFormat present_format;
	VkFormat depth_format;
	int      r_fbo_value;
	int      r_hdr_value;        // post-downgrade
	int      r_hdr_requested;    // pre-downgrade
	qboolean sfloat_supported;
	// Phase 6B3'-d: sRGB swapchain capability + active state. The
	// hardware does the linear->sRGB encode on present when active;
	// gamma.frag's encode math is gated by the srgb_swapchain spec
	// constant to avoid double-encoding.
	qboolean srgb_swapchain_capable;
	qboolean srgb_swapchain;
	// Phase 6B3'-d8: HDR10 display output. hdr_display_requested mirrors
	// r_hdrDisplay; hdr_display_active is qtrue only once an HDR colorspace
	// was actually negotiated on the surface (else SDR fallback, with a
	// warning). present_colorspace is the VkColorSpaceKHR actually chosen.
	// peak/min nits are the cvar values surfaced for `gfxinfo` and the
	// mastering metadata.
	qboolean         hdr_display_requested;
	qboolean         hdr_display_active;
	VkColorSpaceKHR  present_colorspace;
	float            hdr_peak_nits;
	float            hdr_min_nits;
	int      bloom_enabled;
	int      bloom_passes_active;
	int      bloom_mip_max;
} hdrPipelineState_t;

static hdrPipelineState_t vk_hdr_state;

void vk_hdr_state_print( void )
{
	if ( !vk_hdr_state.valid ) {
		ri.Log( SEV_INFO, "HDR pipeline: not yet initialized\n" );
		return;
	}

	ri.Log( SEV_INFO, "HDR pipeline:\n" );
	ri.Log( SEV_INFO, "  r_fbo            : %d\n", vk_hdr_state.r_fbo_value );
	if ( vk_hdr_state.r_hdr_value != vk_hdr_state.r_hdr_requested ) {
		ri.Log( SEV_INFO, "  r_hdr            : %d (requested %d)\n",
			vk_hdr_state.r_hdr_value, vk_hdr_state.r_hdr_requested );
	} else {
		ri.Log( SEV_INFO, "  r_hdr            : %d\n", vk_hdr_state.r_hdr_value );
	}
	if ( vk_hdr_state.r_hdr_requested == 1 ) {
		ri.Log( SEV_INFO, "  SFLOAT supported : %s\n",
			vk_hdr_state.sfloat_supported ? "yes" : "no" );
	} else {
		// SFLOAT capability is only queried when r_hdr 1 is requested;
		// for r_hdr 0 / 2 the GPU's actual SFLOAT support is unknown.
		ri.Log( SEV_INFO, "  SFLOAT supported : not queried\n" );
	}
	ri.Log( SEV_INFO, "  base format      : %s\n", vk_format_string( vk_hdr_state.base_format ) );
	ri.Log( SEV_INFO, "  color (main FBO) : %s\n", vk_format_string( vk_hdr_state.color_format ) );
	ri.Log( SEV_INFO, "  bloom            : %s\n", vk_format_string( vk_hdr_state.bloom_format ) );
	ri.Log( SEV_INFO, "  capture          : %s\n", vk_format_string( vk_hdr_state.capture_format ) );
	ri.Log( SEV_INFO, "  present          : %s\n", vk_format_string( vk_hdr_state.present_format ) );
	ri.Log( SEV_INFO, "  sRGB swapchain   : %s%s\n",
		vk_hdr_state.srgb_swapchain ? "yes" : "no",
		vk_hdr_state.srgb_swapchain ? "" : ( vk_hdr_state.srgb_swapchain_capable ? " (capable, but inactive)" : " (not capable)" ) );
	{
		const char *cs;
		switch ( vk_hdr_state.present_colorspace ) {
			case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:        cs = "SRGB_NONLINEAR (SDR)"; break;
			case VK_COLOR_SPACE_HDR10_ST2084_EXT:          cs = "HDR10_ST2084 (BT.2020 + PQ)"; break;
			case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:  cs = "EXTENDED_SRGB_LINEAR (scRGB)"; break;
			case VK_COLOR_SPACE_BT2020_LINEAR_EXT:         cs = "BT2020_LINEAR"; break;
			default:                                       cs = "(other)"; break;
		}
		ri.Log( SEV_INFO, "  colorspace       : %s\n", cs );
	}
	ri.Log( SEV_INFO, "  HDR10 display    : %s%s\n",
		vk_hdr_state.hdr_display_active ? "active" : ( vk_hdr_state.hdr_display_requested ? "requested (SDR fallback — no HDR colorspace on surface)" : "off" ),
		vk_hdr_state.hdr_display_active ? va( " — peak %.0f nits, min %.4f nits", vk_hdr_state.hdr_peak_nits, vk_hdr_state.hdr_min_nits ) : "" );
	ri.Log( SEV_INFO, "  depth            : %s\n", vk_format_string( vk_hdr_state.depth_format ) );
	ri.Log( SEV_INFO, "  bloom passes     : %d / %d max\n",
		vk_hdr_state.bloom_passes_active, vk_hdr_state.bloom_mip_max );
	ri.Log( SEV_INFO, "  bloom enabled    : %d\n", vk_hdr_state.bloom_enabled );

	if ( vk_hdr_state.r_hdr_requested != vk_hdr_state.r_hdr_value ) {
		ri.Log( SEV_INFO, "  r_hdr DOWNGRADED %d -> %d (GPU lacks SFLOAT)\n",
			vk_hdr_state.r_hdr_requested, vk_hdr_state.r_hdr_value );
	}
}


#define VK_CHECK( function_call ) { \
	VkResult res = function_call; \
	if ( res < 0 ) { \
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: %s returned %s", #function_call, vk_result_string( res ) ); \
	} \
}


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for ( int i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


/* Legacy one-shot Vk one-shot helpers retired in 7.4c-submit-BC-B. Use
   Ral_AcquireBegunCommandBuffer(RAL_QUEUE_GRAPHICS) + Ral_SubmitAndDispose instead.
   The 13 callsites that consumed the retired pair are now backed by the
   RAL backend's graphics-queue command pool. The legacy USE_UPLOAD_QUEUE
   branch that consumed vk.rendering_finished for cross-frame sync is
   preserved implicitly: all submits stay on the GRAPHICS queue so the
   same queue serialization holds. */


static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override ) {
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			if ( src_stage_override != 0 )
				src_stage = src_stage_override;
			else
				src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}


	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	//barrier.srcAccessMask = src_access_flags;
	//barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

static void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}


// Phase 6B3'-d8: push the HDR10 mastering metadata for the current
// swapchain. No-op unless an HDR colorspace was negotiated and
// VK_EXT_hdr_metadata is available. Called once per swapchain creation
// and again whenever r_hdrPeakLuminance / r_hdrMinLuminance change
// (the values are static — the spec advises against per-frame updates).
static void vk_apply_hdr_metadata( void )
{
	// Phase 7.4c-submit-followup-present-2 — Cluster H. Retired the legacy
	// qvkSetHdrMetadataEXT call in favor of Ral_SetSwapchainHdrMetadata.
	// ralHdrMetadata_t shape is 1:1 with VkHdrMetadataEXT (primaries +
	// luminance + MaxCLL + MaxFALL); the RAL backend caches the data and
	// forwards to qvkSetHdrMetadataEXT internally when VK_EXT_hdr_metadata
	// is available (PFN_vkSetHdrMetadataEXT loaded via LOAD_DEV_OPT in
	// present-1). NULL function pointer is the gate — same legacy behavior.
	ralHdrMetadata_t md;

	if ( !vk_hdr_state.hdr_display_active || vk.ral_swapchain == NULL )
		return;

	memset( &md, 0, sizeof( md ) );
	// BT.2020 / Rec. ITU-R BT.2100 mastering display primaries, D65 white.
	md.displayPrimaryRed  [0] = 0.708f;  md.displayPrimaryRed  [1] = 0.292f;
	md.displayPrimaryGreen[0] = 0.170f;  md.displayPrimaryGreen[1] = 0.797f;
	md.displayPrimaryBlue [0] = 0.131f;  md.displayPrimaryBlue [1] = 0.046f;
	md.whitePoint         [0] = 0.3127f; md.whitePoint         [1] = 0.3290f;
	md.maxLuminance              = (float)r_hdrPeakLuminance->integer;       // nits
	md.minLuminance              = r_hdrMinLuminance->value;                 // nits
	md.maxContentLightLevel      = (float)r_hdrPeakLuminance->integer;       // MaxCLL hint (static)
	md.maxFrameAverageLightLevel = (float)r_hdrPeakLuminance->integer * 0.4f; // MaxFALL hint (rough static estimate)

	Ral_SetSwapchainHdrMetadata( vk.ral_swapchain, &md );

	vk_hdr_state.hdr_peak_nits = (float)r_hdrPeakLuminance->integer;
	vk_hdr_state.hdr_min_nits  = r_hdrMinLuminance->value;
}


// Phase 7.4c-submit-followup-present-2 — VkFormat / VkColorSpaceKHR /
// VkPresentModeKHR → ralX_t reverse mapping helpers. Used at the
// vk_create_swapchain tail to populate ralSwapchainCreateInfo_t from the
// renderer's already-resolved vk.present_format + picked present_mode.
//
// Phase 7.4c-submit-followup-present-2-fix1 — default cases were SILENT
// fall-throughs returning RAL_FORMAT_UNDEFINED / SRGB_NONLINEAR / FIFO,
// which produces invalid swapchain create info that vkCreateSwapchainKHR
// rejects without a recoverable log path (the renderer's ri.Terminate
// doesn't flush logs reliably from inside vkCreate*-failure callstacks).
// Replaced with SEV_FATAL + ri.Terminate so an unmapped format/colorspace/
// present-mode surfaces as a readable log line including the numeric Vk
// value — future-extend cases at this site when the log fires.
//
// TODO_log_flush_on_fatal: even with SEV_FATAL, the Wired logging sink may
// have async-flush behavior that drops the line if ri.Terminate exits
// immediately. Investigated at log.c:144-164 — SEV_FATAL has a recursion
// guard and direct-to-stderr write, so the message should reach stderr
// even on broken sinks. File-sink durability across abrupt exit is a
// separate concern (out of scope for this fix turn).
static ralFormat_t Vk_to_RalFormat( VkFormat f ) {
	switch ( f ) {
	case VK_FORMAT_B8G8R8A8_UNORM:           return RAL_FORMAT_B8G8R8A8_UNORM;
	case VK_FORMAT_B8G8R8A8_SRGB:            return RAL_FORMAT_B8G8R8A8_SRGB;
	case VK_FORMAT_R8G8B8A8_UNORM:           return RAL_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_R8G8B8A8_SRGB:            return RAL_FORMAT_R8G8B8A8_SRGB;
	// Phase 7.4c-submit-followup-present-2-fix1 — added these per the
	// brief's "boot-log evidence" guidance. VK_FORMAT_A8B8G8R8_UNORM_PACK32
	// (51) and VK_FORMAT_A8B8G8R8_SRGB_PACK32 (57) are memory-equivalent
	// to R8G8B8A8 variants (byte order R G B A in memory; the PACK32 form
	// just declares the component layout differently). NVIDIA Windows
	// drivers sometimes expose these as the first candidate in
	// vkGetPhysicalDeviceSurfaceFormatsKHR's result list, which would
	// fall through vk_select_surface_format's candidates[0] fallback and
	// reach the renderer's vk.present_format.format with no SRGB upgrade
	// available (the upgrade switch at vk.c:2459-2461 only handles
	// B8G8R8A8/R8G8B8A8 forms, not the PACK32 variants).
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:    return RAL_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32:     return RAL_FORMAT_R8G8B8A8_SRGB;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return RAL_FORMAT_A2B10G10R10_UNORM;
	case VK_FORMAT_R16G16B16A16_SFLOAT:      return RAL_FORMAT_R16G16B16A16_SFLOAT;
	default:
		ri.Log( SEV_FATAL, "[VK->RAL] Vk_to_RalFormat: unmapped VkFormat=%d "
			"(vk_select_surface_format picked a format Vk_to_RalFormat doesn't recognize). "
			"Extend the helper at this site to add the missing case.\n", (int)f );
		ri.Terminate( TERM_UNRECOVERABLE, "Vk_to_RalFormat unmapped format %d", (int)f );
		return RAL_FORMAT_UNDEFINED;   /* unreached; satisfies compiler */
	}
}

static ralColorSpace_t Vk_to_RalColorSpace( VkColorSpaceKHR cs ) {
	switch ( cs ) {
	case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:       return RAL_COLORSPACE_SRGB_NONLINEAR;
	case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return RAL_COLORSPACE_EXTENDED_SRGB_LINEAR;
	case VK_COLOR_SPACE_HDR10_ST2084_EXT:         return RAL_COLORSPACE_HDR10_ST2084;
	case VK_COLOR_SPACE_HDR10_HLG_EXT:            return RAL_COLORSPACE_HDR10_HLG;
	case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return RAL_COLORSPACE_DISPLAY_P3;
	default:
		ri.Log( SEV_FATAL, "[VK->RAL] Vk_to_RalColorSpace: unmapped VkColorSpaceKHR=%d "
			"(vk_select_surface_format negotiated a colorspace Vk_to_RalColorSpace doesn't recognize). "
			"Extend the helper at this site to add the missing case.\n", (int)cs );
		ri.Terminate( TERM_UNRECOVERABLE, "Vk_to_RalColorSpace unmapped colorSpace %d", (int)cs );
		return RAL_COLORSPACE_SRGB_NONLINEAR;   /* unreached */
	}
}

static ralPresentMode_t Vk_to_RalPresentMode( VkPresentModeKHR pm ) {
	switch ( pm ) {
	case VK_PRESENT_MODE_MAILBOX_KHR:           return RAL_PRESENT_MAILBOX;
	case VK_PRESENT_MODE_IMMEDIATE_KHR:         return RAL_PRESENT_IMMEDIATE;
	case VK_PRESENT_MODE_FIFO_KHR:              return RAL_PRESENT_FIFO;
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR:      return RAL_PRESENT_FIFO;   // RAL lacks FIFO_RELAXED; maps to FIFO (legal degradation per Vulkan spec)
	// Phase 7.4c-submit-followup-present-2-fix1 — VK_PRESENT_MODE_FIFO_LATEST_READY_EXT
	// is picked by vk_create_swapchain when r_swapInterval > 0 + the extension
	// is supported (vk.c:1101). Map to RAL_PRESENT_FIFO (FIFO with low-latency
	// drain semantics is conceptually closest to plain FIFO).
	case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return RAL_PRESENT_FIFO;
	default:
		ri.Log( SEV_FATAL, "[VK->RAL] Vk_to_RalPresentMode: unmapped VkPresentModeKHR=%d "
			"(vk_create_swapchain picked a present mode Vk_to_RalPresentMode doesn't recognize). "
			"Extend the helper at this site to add the missing case.\n", (int)pm );
		ri.Terminate( TERM_UNRECOVERABLE, "Vk_to_RalPresentMode unmapped present mode %d", (int)pm );
		return RAL_PRESENT_FIFO;   /* unreached */
	}
}

static void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose ) {
	VkImageViewCreateInfo view;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	qboolean fifo_latest_ready_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	vk.clearAttachment = qtrue;

	if ( !vk.fboActive ) {
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			vk.clearAttachment = qfalse;
			ri.Log( SEV_WARN, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain, \\r_clear might not work\n" );
		}
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
			ri.Terminate( TERM_UNRECOVERABLE, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL));

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Log( SEV_INFO, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Log( SEV_INFO, " %s", pmode_to_str( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_LATEST_READY_EXT )
			fifo_latest_ready_supported = qtrue;
	}
	if ( verbose ) {
		ri.Log( SEV_INFO, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_latest_ready_supported ) {
			/* macOS/MoltenVK: presents most-recently-completed frame at vblank,
			   avoiding the 16ms FIFO stall while remaining tear-free */
			present_mode = VK_PRESENT_MODE_FIFO_LATEST_READY_EXT;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_LATEST_READY, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Log( SEV_INFO, "...selected presentation mode: %s, image count: %i\n", pmode_to_str( present_mode ), image_count );
	}

	// create swap chain
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( !vk.fboActive ) {
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	//desc.compositeAlpha = get_composite_alpha( surface_caps.supportedCompositeAlpha );
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

#if defined( _WIN32 )
	// Phase 6B3'-d8: when the negotiated swapchain colorspace is HDR10, chain
	// the FSE info (allowed + target HMONITOR) into swapchain creation too —
	// matching the surface-format query above. ALLOWED mode means the driver
	// only enters exclusive fullscreen when it makes sense (covers-the-screen
	// borderless), so this does not change a windowed/SDR session. Phase
	// 7.4c-submit-followup-present-2: the fseInfo chain is now passed through
	// ralSwapchainCreateInfo_t.backendExtensionChain into RAL's
	// VkSwapchainCreateInfoKHR.pNext, preserving the legacy Win32 HDR FSE
	// behavior under RAL ownership.
	static VkSurfaceFullScreenExclusiveWin32InfoEXT fseWin32;
	static VkSurfaceFullScreenExclusiveInfoEXT      fseInfo;
	const void *fseExtensionChain = NULL;
	if ( vk_fse_ext_enabled && vk_hdr_state.hdr_display_active ) {
		memset( &fseWin32, 0, sizeof( fseWin32 ) );
		fseWin32.sType    = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
		fseWin32.hmonitor = vk_hmonitor;
		memset( &fseInfo, 0, sizeof( fseInfo ) );
		fseInfo.sType              = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
		fseInfo.pNext              = &fseWin32;
		fseInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
		fseExtensionChain = &fseInfo;
	}
#endif

	// Phase 7.4c-submit-followup-present-2 — Cluster E. Retired the legacy
	// qvkCreateSwapchainKHR(device, &desc, NULL, swapchain) call (and the
	// qvkGetSwapchainImagesKHR pair below it that referenced the now-
	// unused-as-direct-create-target `desc` struct). The RAL backend's
	// Ral_CreateSwapchain owns the VkSwapchainKHR; vk.swapchain field is
	// repurposed as an alias of the RAL-owned handle so the 100+ existing
	// vk.swapchain references in vk.c continue to work unchanged.
	{
		ralSwapchain_t *oldRalSc = vk.ral_swapchain;
		VkSwapchainKHR  oldRalVk = ( oldRalSc != NULL ) ? (VkSwapchainKHR)Ral_GetSwapchainHandle( oldRalSc ) : VK_NULL_HANDLE;
		ralSwapchainCreateInfo_t sci;
		memset( &sci, 0, sizeof( sci ) );
		sci.width                   = image_extent.width;
		sci.height                  = image_extent.height;
		sci.format                  = Vk_to_RalFormat    ( surface_format.format     );
		sci.colorSpace              = Vk_to_RalColorSpace( surface_format.colorSpace );
		sci.presentMode             = Vk_to_RalPresentMode( present_mode );
		sci.minImageCount           = image_count;
		sci.externalSurface         = (void *)surface;
		sci.oldExternalSwapchain    = (void *)oldRalVk;
#if defined( _WIN32 )
		sci.backendExtensionChain   = fseExtensionChain;
#else
		sci.backendExtensionChain   = NULL;
#endif
		vk.ral_swapchain = Ral_CreateSwapchain( vk_ral_get_backend(), &sci );
		if ( vk.ral_swapchain == NULL ) {
			ri.Terminate( TERM_UNRECOVERABLE, "vk_create_swapchain: Ral_CreateSwapchain returned NULL" );
		}
		// Destroy the OLD wrapper AFTER successful new create. The
		// oldSwapchain handoff retired the old VkSwapchainKHR; the wrapper
		// destroy invokes vkDestroySwapchainKHR on the retired handle —
		// safe per Vulkan spec. On boot (oldRalSc == NULL) this branch
		// skips.
		if ( oldRalSc != NULL ) {
			Ral_DestroySwapchain( oldRalSc );
		}
		// Alias the legacy field — preserves the 100+ existing
		// vk.swapchain references in vk.c. The output param `swapchain`
		// (typically &vk.swapchain) is also written for caller compat.
		vk.swapchain = (VkSwapchainKHR)Ral_GetSwapchainHandle( vk.ral_swapchain );
		*swapchain = vk.swapchain;
	}

	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );

	// Phase 6B3'-d8: attach the HDR10 mastering metadata to the freshly-
	// created swapchain (no-op unless an HDR colorspace was negotiated).
	vk_apply_hdr_metadata();

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.flags = 0;
		view.image = vk.swapchain_images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = vk.present_format.format;
		view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.swapchain_image_views[i] ) );

		SET_OBJECT_NAME( vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.swapchain_image_views[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		SET_OBJECT_NAME( vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
		// Phase 7.4c-submit-followup-present-1 — per-swapchain-image adoption.
		// Consumed by -2 as the ralPresentInfo_t.waitSemaphores[0] entry at
		// the migrated Ral_Present site. Re-runs on every swapchain recreate
		// (this loop fires from both initial vk_create_swapchain and the 2
		// recreate paths that call it).
		vk.ral_swapchain_rendering_finished[i] = Ral_AdoptSemaphore( vk_ral_get_backend(),
			(void *)vk.swapchain_rendering_finished[i], RAL_SEMAPHORE_BINARY,
			"wired-swapchain-rendering-finished" );
	}

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping swapchain layout transition\n", __func__ );
		} else {
			VkCommandBuffer command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );

			for ( i = 0; i < vk.swapchain_image_count; i++ ) {
				record_image_layout_transition( command_buffer, vk.swapchain_images[i],
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
			}

			Ral_SubmitAndDispose( rcmd );
		}
	}

	// Phase 7.4c-submit-followup-present-2 — Ral_CreateSwapchain delivered
	// above (see the Cluster E block before the swapchain-image enumeration).
	// vk.swapchain is now an alias of vk.ral_swapchain->swapchain via
	// Ral_GetSwapchainHandle; the legacy qvkCreateSwapchainKHR/Destroy
	// pair is retired. Recreate paths (vk_restart_swapchain,
	// vk_rebuild_for_fbo_change) call vk_destroy_swapchain(qtrue) +
	// vk_create_swapchain → the latter passes the old VkSwapchainKHR
	// through oldExternalSwapchain for atomic handoff.
}


static void vk_create_render_passes( void )
{
	VkAttachmentDescription attachments[2]; // color | depth
	VkAttachmentReference colorRef0;
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	if ( r_fbo->integer == 0 )
	{
		// presentation
		attachments[0].flags = 0;
		attachments[0].format = vk.present_format.format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for presentation
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = vk.initSwapchainLayout;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

#ifdef USE_BUFFER_CLEAR
		// Always CLEAR the color/resolve target — DONT_CARE leaves
		// garbage (pink) on TBDR GPUs (Apple Silicon via MoltenVK)
		// because MTLLoadActionDontCare truly discards tile contents.
		attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[ 0 ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif

		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	// depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	if ( r_bloom->integer || vk.depthFade.active ) {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep it for post-bloom/depth-fade pass
		attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	} else {
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;

	desc.subpassCount = 1;
	desc.attachmentCount = 2;

	// subpass dependencies

	memset( &deps, 0, sizeof( deps ) );

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[2].dependencyFlags = 0;

	if ( r_fbo->integer == 0 )
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		return;
	}

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;						// What access scopes are influence the dependency
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Fragment data has been written
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// Don't start shading until data is available
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// Waiting for color data to be written
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						// Don't read things from the shader before ready
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;					// Only need the current fragment (or tile) synchronized, not the whole framebuffer

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	ri.Log( SEV_DEBUG, "[FBO_DEBUG] Main render pass created:\n" );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   color format=%d  loadOp=%d  initialLayout=%d  finalLayout=%d\n",
		attachments[0].format, attachments[0].loadOp, attachments[0].initialLayout, attachments[0].finalLayout );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   depth format=%d  loadOp=%d\n",
		attachments[1].format, attachments[1].loadOp );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   msaaActive=%d  r_fbo=%d\n", vk.msaaActive, r_fbo->integer );

	// depth fade pass: loads color+depth from main pass, renders soft transparents
	if ( vk.depthFade.active ) {
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.depth_fade ) );
		SET_OBJECT_NAME( vk.render_pass.depth_fade, "render pass - depth_fade", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// restore main pass settings for subsequent render pass creation
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		if ( r_bloom->integer || vk.depthFade.active ) {
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].stencilStoreOp = glConfig.stencilBits ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		} else {
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
	}

	if ( r_bloom->integer ) {

		// post-bloom pass
		// color buffer
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // load from previous pass
		 // depth buffer
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = 0;
		attachments[0].format = vk.bloom_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		// DONT_CARE leaves pink on TBDR
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.bloom_extract ) );
		SET_OBJECT_NAME( vk.render_pass.bloom_extract, "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ )
		{
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - blur %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}
	}

	// SMAA render passes
	if ( vk.smaa.active )
	{
		desc.attachmentCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = &deps[0];

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		// SMAA edge detection pass: R8G8_UNORM, clear
		attachments[0].flags = 0;
		attachments[0].format = VK_FORMAT_R8G8_UNORM;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.smaa_edge ) );
		SET_OBJECT_NAME( vk.render_pass.smaa_edge, "render pass - smaa_edge", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// SMAA blend weight pass: R8G8B8A8_UNORM, clear
		attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.smaa_blend ) );
		SET_OBJECT_NAME( vk.render_pass.smaa_blend, "render pass - smaa_blend", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// SMAA resolve pass: color_format, clear to black (fullscreen triangle covers all, but safety)
		attachments[0].format = vk.color_format;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.smaa_resolve ) );
		SET_OBJECT_NAME( vk.render_pass.smaa_resolve, "render pass - smaa_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	// capture render pass
	if ( vk.capture.image )
	{
		memset( &subpass, 0, sizeof( subpass ) );

		attachments[0].flags = 0;
		attachments[0].format = vk.capture_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // DONT_CARE leaves pink on TBDR GPUs
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.capture ) );
		SET_OBJECT_NAME( vk.render_pass.capture, "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	desc.attachmentCount = 1;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// Phase 6B3'-c1: tonemap pass — reads vk.color_image (HDR scene
	// + bloom composition), writes vk.tonemapped_image (LDR-range, still
	// linear). Single attachment, full-screen post-process, no MSAA.
	// Phase 6B3'-d4-Block-5a: format follows vk.color_format (matches
	// vk.tonemapped_image's allocation; see vk_create_attachments) — the
	// former hardcoded R8G8B8A8_UNORM quantized sub-1/255 linear values to
	// zero, crushing dark UI/scene content before the gamma pass.
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // tonemap writes every pixel
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.tonemap ) );
	SET_OBJECT_NAME( vk.render_pass.tonemap, "render pass - tonemap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// Phase 6B3'-d4-Block-5b: UI compositing pass. 2D draws now land here
	// (in vk.tonemapped_image / img 265) instead of vk.color_image, so the
	// PBR Neutral shoulder + bloom blur don't apply to UI. Color LOADs
	// from the tonemap output, depth is a DONT_CARE stub for pipeline
	// compat with main pass. Pipeline-compatible with render_pass.main
	// per Vulkan §8.2 (same 2 attachments, R16F color + D24S8 depth,
	// single-sample) — no separate pipeline variant set needed after
	// Block-5b-prereq retired MSAA.
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	desc.attachmentCount = 2;
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0]; // EXTERNAL→0: SHADER_READ → COLOR_ATTACHMENT_R/W; 0→EXTERNAL: COLOR_WRITE → SHADER_READ

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ui ) );
	SET_OBJECT_NAME( vk.render_pass.ui, "render pass - ui", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// Block 8 (Delta 2): render_pass.ui_clear — identical to render_pass.ui
	// but the colour attachment loadOp is CLEAR instead of LOAD. Pure-2D
	// frames (menu / loading screen) skip the tonemap pass entirely and
	// draw 2D straight into a freshly-cleared img 265. Render-pass-compatible
	// with render_pass.ui (and render_pass.main) per Vulkan §8.2 — only
	// loadOp differs, which is not part of compatibility — so the same 2D
	// pipelines bind to it. (deps, subpass, attachment[1] depth stub all
	// carry over from the render_pass.ui setup above.)
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.ui_clear ) );
	SET_OBJECT_NAME( vk.render_pass.ui_clear, "render pass - ui_clear", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// Restore single-attachment shape for the gamma pass that follows —
	// must also clear pDepthStencilAttachment from the UI subpass struct,
	// otherwise the gamma pass (1 attachment) inherits a dangling
	// depthRef pointing at attachment 1 (out of range).
	desc.attachmentCount = 1;
	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// gamma post-processing
	attachments[0].flags = 0;
	attachments[0].format = vk.present_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // needed for presentation
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = vk.initSwapchainLayout;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
	SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// screenmap
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].flags = 0;
	attachments[0].format = vk.color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
	// Always CLEAR — DONT_CARE leaves pink on TBDR GPUs (Apple Silicon)
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// screenmap depth buffer
	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = vk.screenMapSamples;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorRef0.attachment = 0;
	colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthRef0.attachment = 1;
	depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.attachmentCount = 2;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

	SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
}


static void allocate_and_bind_image_memory(VkImage image) {
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;

	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if ( memory_requirements.size > vk.image_chunk_size ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
			(int)(memory_requirements.size/1024) );
	}

	chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for ( int i = 0; i < vk_world.num_image_chunks; i++ ) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: image chunk limit has been reached" );
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static void vk_clean_staging_buffer( void )
{
	if ( vk.staging_buffer.handle != VK_NULL_HANDLE ) {
		vk_ral_unregister_buffer( vk.staging_buffer.handle );
		qvkDestroyBuffer( vk.device, vk.staging_buffer.handle, NULL );
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	//if ( vk.staging_buffer.ptr != NULL )
	//	qvkUnmapMemory( vk.device, vk.staging_buffer.memory ) {
	//	vk.staging_buffer.ptr = NULL;
	//}

	if ( vk.staging_buffer.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.staging_buffer.memory, NULL );
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = NULL;
	vk.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
}


#ifdef USE_UPLOAD_QUEUE
static qboolean vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait ) {
		// Phase 7.4c-submit-followup-staging — replace qvkWaitForFences with
		// Ral_WaitFence on the adopted sibling. The 5s timeout is preserved
		// by Ral_WaitFence honoring its timeout arg; using RAL_TIMEOUT_INFINITE
		// (~0ull) here would match the BC-B Ral_SubmitAndDispose pattern, but
		// the legacy 5s bound is a deliberate "hang detector" — preserve it.
		Ral_WaitFence( vk.ral_aux_fence, 5 * 1000000000ULL );
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk.staging_buffer.offset = 0; // FIXME: is this correct?
		vk.aux_fence_wait = qfalse;
		return qtrue;
	}
	return qfalse;
}


static void vk_flush_staging_buffer( qboolean final )
{
	ralSubmitInfo_t si;
	ralSemaphore_t *waitSem[1];
	ralSemaphore_t *signalSem[1];

	if ( vk.staging_buffer.offset == 0 ) {
		return;
	}

	//ri.Log( SEV_WARN, S_COLOR_CYAN ">>> flush %i bytes (final=%i)<<<\n", (int)vk_world.staging_buffer_offset, final );

	vk.staging_buffer.offset = 0;

	// Phase 7.4c-submit-followup-staging — keep using qvkEndCommandBuffer (NOT
	// Ral_EndCommandBuffer) on the staging cb: the RAL state-tracker on
	// ral_staging_cmd would need to be in RECORDING state for Ral_EndCommandBuffer
	// to succeed, but the persistent staging cb's begin/reset cycle bypasses
	// the RAL state machine entirely (no Ral_Begin/Ral_Reset; this is the
	// brief's III.3 explicit decision to avoid double-tracking). qvkEnd on
	// the underlying VkCommandBuffer (= staging_command_buffer alias) works
	// regardless of the wrapper's state field.
	VK_CHECK( qvkEndCommandBuffer( vk.staging_command_buffer ) );

	// Phase 7.4c-submit-followup-staging — build ralSubmitInfo_t in place of
	// VkSubmitInfo. Semaphore + fence args use the adopted RAL siblings; the
	// dynamic alias logic (image_uploaded ↔ image_uploaded2, rendering_finished
	// ↔ vk.cmd->rendering_finished2) is preserved with parallel RAL-side
	// alias flips so a future per-frame submit migration (BC-C-final) can
	// consume vk.ral_image_uploaded directly.
	memset( &si, 0, sizeof( si ) );
	si.commandBuffers     = &vk.ral_staging_cmd;
	si.numCommandBuffers  = 1;

	if ( vk.ral_rendering_finished != NULL ) {
		// first call after previous queue submission?
		waitSem[0] = vk.ral_rendering_finished;
		si.waitSemaphores      = waitSem;
		si.numWaitSemaphores   = 1;
		// legacy alias also clears here; both must flip in lockstep
		vk.rendering_finished     = VK_NULL_HANDLE;
		vk.ral_rendering_finished = NULL;
	}

	if ( vk.image_uploaded != VK_NULL_HANDLE ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: incorrect state during image upload" );
	}

	si.signalFence = vk.ral_aux_fence;

	if ( final ) {
		// final submission before recording — signal image_uploaded2 so the
		// next per-frame render submit waits on this batch's completion at
		// the GPU side (no host wait here — fire and forget).
		signalSem[0] = vk.ral_image_uploaded2;
		si.signalSemaphores    = signalSem;
		si.numSignalSemaphores = 1;
		Ral_Submit( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS, &si );
		// Alias flip: both legacy and RAL aliases now point at their
		// respective sibling, signaling "the next render must wait for this
		// upload batch's GPU-side completion".
		vk.image_uploaded     = vk.image_uploaded2;
		vk.ral_image_uploaded = vk.ral_image_uploaded2;
		vk.aux_fence_wait     = qtrue;
	} else {
		// intermediate flush — submit, host-wait on the fence, reset
		// everything so the same staging cb can be re-recorded immediately.
		Ral_Submit( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS, &si );
		Ral_WaitFence( vk.ral_aux_fence, 5 * 1000000000ULL );
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
	}
}
#endif // USE_UPLOAD_QUEUE


static void vk_alloc_staging_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk.staging_buffer.size = PAD( vk.staging_buffer.size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements( vk.device, vk.staging_buffer.handle, &memory_requirements );

	memory_type = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));
	vk_ral_register_buffer( vk.staging_buffer.handle, vk.staging_buffer.size, buffer_desc.usage,
	                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                        "vk.staging_buffer" );

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
	SET_OBJECT_NAME( vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VK_VALIDATION
/* Routes validation-layer messages to the engine log (cat=renderer) instead
 * of spawning a modal MessageBox per error.  ri.Log → Com_Logv internally,
 * so output lands in qconsole.jsonl with the renderer category. */
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT       severity_bits,
	VkDebugUtilsMessageTypeFlagsEXT              type_flags,
	const VkDebugUtilsMessengerCallbackDataEXT  *data,
	void                                        *user_data )
{
	log_severity_t sev;
	const char *msg;

	(void)type_flags;
	(void)user_data;

	if ( !data || !data->pMessage )
		return VK_FALSE;
	msg = data->pMessage;

	if ( severity_bits & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT )
		sev = SEV_ERROR;
	else if ( severity_bits & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
		sev = SEV_WARN;
	else if ( severity_bits & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT )
		sev = SEV_INFO;
	else
		sev = SEV_DEBUG;

	ri.Log( sev, "Vulkan: %s\n", msg );
	return VK_FALSE;
}
#endif


static qboolean used_instance_extension( const char *ext )
{
	const char *u;

	// allow all VK_*_surface extensions
	u = strrchr( ext, '_' );
	if ( u && Q_stricmp( u + 1, "surface" ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME ) == 0 )
		return qtrue; // needed for KMSDRM instances/devices?

	if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
		return qtrue;

	// Phase 6B3'-d8: enables the non-sRGB swapchain colorspaces
	// (VK_COLOR_SPACE_HDR10_ST2084_EXT etc.) in
	// vkGetPhysicalDeviceSurfaceFormatsKHR. Harmless when present but
	// the display is SDR (the format list just stays sRGB-only); when
	// absent, the HDR10 negotiation below simply finds no HDR colorspace
	// and falls back to SDR. Always-enable when available.
	if ( Q_stricmp( ext, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	return qfalse;
}


static void create_instance( void )
{
#ifdef USE_VK_VALIDATION
	const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	const char* validation_layer_name2 = "VK_LAYER_KHRONOS_validation";
#endif
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !used_instance_extension( ext ) ) {
			continue;
		}

		// search for duplicates
		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}
	}

	for ( i = 0; i < extension_count; i++ )
		ri.Log( SEV_DEBUG, "[VK] instance extension enabled: %s\n", extension_names[i] );

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL; // WIRED_ENGINE_VERSION;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
	// Phase 7.4c-pre (Option A): renderervk shares its VkInstance/VkDevice
	// with the RAL Vulkan backend, so the instance apiVersion gates which
	// core entry points vkGetDeviceProcAddr will dispatch. RAL requires
	// vkQueueSubmit2 / vkCmdBeginRendering (1.3 core), so the instance
	// must be created at 1.3 — a 1.2 instance on a 1.3-class GPU still
	// fails to resolve those entry points. The descriptorIndexing-suite +
	// timelineSemaphore + hostQueryReset + drawIndirectCount features come
	// in through VkPhysicalDeviceVulkan12Features (still valid at 1.3).
	{
		PFN_vkEnumerateInstanceVersion qvkEnumerateInstanceVersion =
			( PFN_vkEnumerateInstanceVersion )ri.VK_GetInstanceProcAddr( VK_NULL_HANDLE, "vkEnumerateInstanceVersion" );
		uint32_t loaderVer = VK_API_VERSION_1_0;
		if ( qvkEnumerateInstanceVersion ) {
			qvkEnumerateInstanceVersion( &loaderVer );
		}
		if ( VK_API_VERSION_MAJOR( loaderVer ) < 1u
		  || ( VK_API_VERSION_MAJOR( loaderVer ) == 1u && VK_API_VERSION_MINOR( loaderVer ) < 3u ) ) {
			ri.Terminate( TERM_UNRECOVERABLE,
				"Vulkan: loader reports %u.%u; Wired requires Vulkan 1.3+ (update GPU driver / install the Vulkan SDK runtime)",
				VK_API_VERSION_MAJOR( loaderVer ), VK_API_VERSION_MINOR( loaderVer ) );
		}
		vk.instance_api_version = loaderVer;
	}
	appInfo.apiVersion = VK_API_VERSION_1_3;

	// create instance
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );

	if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

			ri.Log( SEV_WARN, "...validation layer is not available\n" );

			// try without validation layer
			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			res = qvkCreateInstance( &desc, NULL, &vk_instance );
		}
	}
#else
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}
}


static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( int i = 0; i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Terminate( TERM_UNRECOVERABLE, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static VkFormat get_hdr_format( VkFormat base_format )
{
	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	switch ( r_hdr->integer ) {
		case 1:  return VK_FORMAT_R16G16B16A16_SFLOAT;  // true HDR, default
		case 2:  return VK_FORMAT_R16G16B16A16_UNORM;   // clamped HDR fallback
		default: return base_format;                    // r_hdr 0 — LDR
	}
}

typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	//{12, VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16},
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb ) {
	const present_format_t *pf, *sel;

	sel = NULL;
	pf = present_formats;
	for ( int i = 0; i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits  ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}


#if defined( _WIN32 )
// Phase 6B3'-d8 delta 3: read-only OS HDR-state diagnostic. Walks the DXGI
// adapter/output tree and logs each output's DXGI_OUTPUT_DESC1.ColorSpace —
// DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 (== 12) means Windows HDR is
// ACTIVE for that display; DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 (== 0)
// means it is in SDR mode. Also logs the reported luminance envelope. This is
// purely informational: no DXGI device/swapchain is created and nothing is
// rendered through DXGI — dxgi.dll is loaded at runtime so the renderer DLL
// keeps zero link-time DXGI deps (no dxgi.lib / dxguid.lib). If `target`
// matches an output's HMONITOR that line is flagged "<== engine surface
// monitor", so we can see whether the monitor the Vulkan surface lives on is
// in HDR mode at the OS level — i.e. whether a "no HDR10 colorspace" result
// is an engine/driver limitation or just "Windows HDR isn't actually on".
static void vk_log_dxgi_hdr_state( HMONITOR target )
{
	typedef HRESULT( WINAPI *PFN_CreateDXGIFactory1 )( REFIID riid, void **ppFactory );
	// Local GUID literals — MinGW's DEFINE_GUID(IID_*) only emits an extern
	// declaration (the symbols live in libdxguid); defining our own avoids
	// having to link it.
	static const GUID kIID_IDXGIFactory1 = { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
	static const GUID kIID_IDXGIOutput6  = { 0x068346e8, 0xaaec, 0x4b84, { 0xad, 0xd7, 0x13, 0x7f, 0x51, 0x3f, 0x77, 0xa1 } };

	HMODULE                dxgi;
	PFN_CreateDXGIFactory1 pCreateDXGIFactory1;
	IDXGIFactory1         *factory = NULL;
	HRESULT                hr;
	UINT                   ai;

	dxgi = LoadLibraryA( "dxgi.dll" );
	if ( dxgi == NULL ) {
		ri.Log( SEV_DEBUG, "[VK] DXGI HDR diag: dxgi.dll not available — skipping\n" );
		return;
	}
	pCreateDXGIFactory1 = (PFN_CreateDXGIFactory1)(void *)GetProcAddress( dxgi, "CreateDXGIFactory1" );
	if ( pCreateDXGIFactory1 == NULL ) {
		ri.Log( SEV_DEBUG, "[VK] DXGI HDR diag: CreateDXGIFactory1 not found — skipping\n" );
		FreeLibrary( dxgi );
		return;
	}
	hr = pCreateDXGIFactory1( &kIID_IDXGIFactory1, (void **)&factory );
	if ( FAILED( hr ) || factory == NULL ) {
		ri.Log( SEV_DEBUG, "[VK] DXGI HDR diag: CreateDXGIFactory1 failed (0x%08lx)\n", (unsigned long)hr );
		FreeLibrary( dxgi );
		return;
	}

	ri.Log( SEV_INFO, "[VK] DXGI HDR diag (OS-side display colorspace):\n" );
	for ( ai = 0;; ai++ ) {
		IDXGIAdapter1 *adapter = NULL;
		UINT           oi;
		hr = factory->lpVtbl->EnumAdapters1( factory, ai, &adapter );
		if ( hr == DXGI_ERROR_NOT_FOUND || FAILED( hr ) || adapter == NULL )
			break;
		for ( oi = 0;; oi++ ) {
			IDXGIOutput  *output  = NULL;
			IDXGIOutput6 *output6 = NULL;
			hr = adapter->lpVtbl->EnumOutputs( adapter, oi, &output );
			if ( hr == DXGI_ERROR_NOT_FOUND || FAILED( hr ) || output == NULL )
				break;
			hr = output->lpVtbl->QueryInterface( output, &kIID_IDXGIOutput6, (void **)&output6 );
			if ( SUCCEEDED( hr ) && output6 != NULL ) {
				DXGI_OUTPUT_DESC1 d1;
				memset( &d1, 0, sizeof( d1 ) );
				hr = output6->lpVtbl->GetDesc1( output6, &d1 );
				if ( SUCCEEDED( hr ) ) {
					const char *cs =
						( d1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ) ? "HDR10 (G2084/P2020) — Windows HDR ACTIVE"
						: ( d1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 )   ? "SDR (G22/P709) — Windows HDR OFF"
						                                                                : "other";
					ri.Log( SEV_INFO, "[VK]   adapter %u output %u: ColorSpace=%d [%s] bpc=%u maxLum=%.0f minLum=%.4f maxFFL=%.0f%s\n",
						ai, oi, (int)d1.ColorSpace, cs, d1.BitsPerColor,
						d1.MaxLuminance, d1.MinLuminance, d1.MaxFullFrameLuminance,
						( target != NULL && d1.Monitor == target ) ? "  <== engine surface monitor" : "" );
				} else {
					ri.Log( SEV_INFO, "[VK]   adapter %u output %u: GetDesc1 failed (0x%08lx)\n", ai, oi, (unsigned long)hr );
				}
				output6->lpVtbl->Release( output6 );
			} else {
				ri.Log( SEV_INFO, "[VK]   adapter %u output %u: IDXGIOutput6 unavailable (pre-1703 DXGI) — cannot read ColorSpace\n", ai, oi );
			}
			output->lpVtbl->Release( output );
		}
		adapter->lpVtbl->Release( adapter );
	}
	factory->lpVtbl->Release( factory );
	FreeLibrary( dxgi );
}
#endif // _WIN32


static qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Log( SEV_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Log( SEV_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		// special case that means we can choose any format
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		uint32_t i;
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == base_bgr || candidates[i].format == base_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.base_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.base_format = candidates[0];
		}
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == ext_bgr || candidates[i].format == ext_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.present_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.present_format = vk.base_format;
		}
	}

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	// Phase 6B3'-d: prefer an sRGB swapchain variant when one matches the
	// chosen UNORM present format. The hardware does the linear -> sRGB
	// encode on present, replacing the shader pow() inside gamma.frag.
	// Only the 24-bit candidates (B8G8R8A8 / R8G8B8A8) have sRGB pairs in
	// Vulkan; B5G6R5 (16-bit) and A2B10G10R10 (30-bit) don't, so they
	// stay on the UNORM path. r_fbo 0 keeps the legacy UNORM swapchain
	// regardless.
	vk_hdr_state.srgb_swapchain_capable = qfalse;
	vk_hdr_state.srgb_swapchain         = qfalse;

	if ( r_fbo->integer && format_count > 0 && !( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) ) {
		VkFormat srgb_match = VK_FORMAT_UNDEFINED;
		switch ( vk.present_format.format ) {
			case VK_FORMAT_B8G8R8A8_UNORM: srgb_match = VK_FORMAT_B8G8R8A8_SRGB; break;
			case VK_FORMAT_R8G8B8A8_UNORM: srgb_match = VK_FORMAT_R8G8B8A8_SRGB; break;
			default:                       srgb_match = VK_FORMAT_UNDEFINED;     break;
		}
		if ( srgb_match != VK_FORMAT_UNDEFINED ) {
			uint32_t i;
			for ( i = 0; i < format_count; i++ ) {
				if ( candidates[i].format == srgb_match
				  && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
					vk_hdr_state.srgb_swapchain_capable = qtrue;
					vk.present_format = candidates[i];
					vk_hdr_state.srgb_swapchain = qtrue;
					break;
				}
			}
		}
	}

	// Phase 6B3'-d8: HDR10 swapchain negotiation. When r_hdrDisplay 1 (and
	// r_fbo 1 — the HDR scene buffer + tonemap + gamma encode chain), look
	// for VK_FORMAT_A2B10G10R10_UNORM_PACK32 + VK_COLOR_SPACE_HDR10_ST2084_EXT
	// (BT.2020 + PQ — the gamma pass PQ-encodes). On NVIDIA/Windows the HDR
	// colorspaces are ONLY reported by vkGetPhysicalDeviceSurfaceFormats2KHR
	// (the legacy vkGetPhysicalDeviceSurfaceFormatsKHR used for `candidates`
	// above stays sRGB-only even with VK_EXT_swapchain_colorspace enabled),
	// so re-enumerate via the 2KHR query here. If HDR10 isn't found, stay SDR
	// with a warning (the scRGB EXTENDED_SRGB_LINEAR fallback would need a
	// separate gamma encode path — not implemented this turn). When HDR10 is
	// active the hardware does NOT do an sRGB encode, so clear srgb_swapchain
	// — gamma.frag's hdr_mode==1 branch owns the encode.
	vk_hdr_state.hdr_display_requested = ( r_hdrDisplay->integer != 0 );
	vk_hdr_state.hdr_display_active    = qfalse;
	vk_hdr_state.present_colorspace    = vk.present_format.colorSpace;
	vk_hdr_state.hdr_peak_nits         = (float)r_hdrPeakLuminance->integer;
	vk_hdr_state.hdr_min_nits          = r_hdrMinLuminance->value;

	if ( r_fbo->integer && r_hdrDisplay->integer ) {
		if ( qvkGetPhysicalDeviceSurfaceFormats2KHR == NULL ) {
			ri.Log( SEV_WARN, "r_hdrDisplay 1: VK_KHR_get_surface_capabilities2 unavailable — cannot query HDR colorspaces; staying SDR\n" );
		} else {
			VkPhysicalDeviceSurfaceInfo2KHR surfInfo2;
			VkSurfaceFormat2KHR *fmts2 = NULL;
			uint32_t fc2 = 0;
			VkResult r2;
#if defined( _WIN32 )
			// Phase 6B3'-d8: on NVIDIA/Windows the HDR10_ST2084 colorspace is
			// only enumerated for a surface when the query carries the
			// VK_EXT_full_screen_exclusive pNext chain (FSE allowed + the
			// target HMONITOR). Delta 1 (windowed-HDR10 attempt): resolve the
			// monitor from the actual game window (the renderer DLL runs on the
			// main thread that created it, so GetActiveWindow/GetForegroundWindow
			// returns it) rather than the desktop primary — some drivers tie HDR10
			// enumeration to the game window's monitor specifically. Falls back
			// to the primary monitor if no window handle is available yet.
			VkSurfaceFullScreenExclusiveWin32InfoEXT fseWin32;
			VkSurfaceFullScreenExclusiveInfoEXT      fseInfo;
			{
				HWND hwnd = GetActiveWindow();
				if ( hwnd == NULL )
					hwnd = GetForegroundWindow();
				vk_hmonitor = hwnd ? MonitorFromWindow( hwnd, MONITOR_DEFAULTTOPRIMARY )
				                   : MonitorFromPoint( (POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY );
				ri.Log( SEV_DEBUG, "[VK] HDR10 query: hwnd=%p hmonitor=%p (from %s)\n",
					(void*)hwnd, (void*)vk_hmonitor, hwnd ? "GetActiveWindow/Foreground" : "MonitorFromPoint(primary)" );
				// Delta 3: log the OS-level HDR state of every display, flagging
				// the one our surface lives on — tells us whether a "no HDR10
				// colorspace" enumeration result is an engine/driver limitation
				// or simply that Windows HDR isn't actually on for this monitor.
				vk_log_dxgi_hdr_state( vk_hmonitor );
			}
#endif

			// Delta-3 follow-up diagnostic: enumerate WITHOUT the FSE pNext
			// chain, to settle whether attaching VkSurfaceFullScreenExclusiveInfoEXT
			// is what hides the HDR10 colorspace (some drivers narrow the format
			// list when an FSE mode is requested). If this list contained HDR10
			// but the FSE-chained one below didn't, we'd drop the FSE pNext from
			// the format query. (Empirically on NVIDIA/Windows it is sRGB-only
			// either way — the engine's win32 surface never exposes HDR10.)
			{
				VkPhysicalDeviceSurfaceInfo2KHR si2;
				VkSurfaceFormat2KHR            *nfmts = NULL;
				uint32_t                        nf = 0, i;
				memset( &si2, 0, sizeof( si2 ) );
				si2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
				si2.surface = surface;
				if ( qvkGetPhysicalDeviceSurfaceFormats2KHR( physical_device, &si2, &nf, NULL ) >= 0 && nf > 0 ) {
					nfmts = (VkSurfaceFormat2KHR*)ri.Malloc( nf * sizeof( VkSurfaceFormat2KHR ) );
					memset( nfmts, 0, nf * sizeof( VkSurfaceFormat2KHR ) );
					for ( i = 0; i < nf; i++ )
						nfmts[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
					if ( qvkGetPhysicalDeviceSurfaceFormats2KHR( physical_device, &si2, &nf, nfmts ) >= 0 ) {
						ri.Log( SEV_DEBUG, "[VK] surface formats WITHOUT FSE pNext: %u format(s)\n", nf );
						for ( i = 0; i < nf; i++ )
							ri.Log( SEV_DEBUG, "[VK]   [%u] fmt=%s colorSpace=%d\n",
								i, vk_format_string( nfmts[i].surfaceFormat.format ), (int)nfmts[i].surfaceFormat.colorSpace );
					}
					ri.Free( nfmts );
				}
			}

			memset( &surfInfo2, 0, sizeof( surfInfo2 ) );
			surfInfo2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
			surfInfo2.surface = surface;
#if defined( _WIN32 )
			if ( vk_fse_ext_enabled ) {
				memset( &fseWin32, 0, sizeof( fseWin32 ) );
				fseWin32.sType    = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
				fseWin32.hmonitor = vk_hmonitor;
				memset( &fseInfo, 0, sizeof( fseInfo ) );
				fseInfo.sType              = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
				fseInfo.pNext              = &fseWin32;
				fseInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
				surfInfo2.pNext = &fseInfo;
			}
#endif

			r2 = qvkGetPhysicalDeviceSurfaceFormats2KHR( physical_device, &surfInfo2, &fc2, NULL );
			if ( r2 >= 0 && fc2 > 0 ) {
				uint32_t i;
				int hdrIdx = -1;
				fmts2 = (VkSurfaceFormat2KHR*)ri.Malloc( fc2 * sizeof( VkSurfaceFormat2KHR ) );
				memset( fmts2, 0, fc2 * sizeof( VkSurfaceFormat2KHR ) );
				for ( i = 0; i < fc2; i++ )
					fmts2[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;

				r2 = qvkGetPhysicalDeviceSurfaceFormats2KHR( physical_device, &surfInfo2, &fc2, fmts2 );
				if ( r2 >= 0 ) {
					ri.Log( SEV_DEBUG, "[VK] vkGetPhysicalDeviceSurfaceFormats2KHR: %u format(s) on the engine surface\n", fc2 );
					for ( i = 0; i < fc2; i++ ) {
						ri.Log( SEV_DEBUG, "[VK]   [%u] fmt=%s colorSpace=%d\n",
							i, vk_format_string( fmts2[i].surfaceFormat.format ), (int)fmts2[i].surfaceFormat.colorSpace );
						if ( fmts2[i].surfaceFormat.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32
						  && fmts2[i].surfaceFormat.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ) {
							hdrIdx = (int)i;
						}
					}
				}
				if ( hdrIdx >= 0 ) {
					vk.present_format                   = fmts2[hdrIdx].surfaceFormat;
					vk_hdr_state.hdr_display_active     = qtrue;
					vk_hdr_state.present_colorspace     = fmts2[hdrIdx].surfaceFormat.colorSpace;
					vk_hdr_state.srgb_swapchain         = qfalse;  // PQ encode is in-shader, not HW sRGB
					vk_hdr_state.srgb_swapchain_capable = qfalse;
					ri.Log( SEV_INFO, "r_hdrDisplay 1: HDR10 swapchain negotiated — %s + HDR10_ST2084 (peak %d nits)\n",
						vk_format_string( vk.present_format.format ), r_hdrPeakLuminance->integer );
				} else {
					ri.Log( SEV_WARN, "r_hdrDisplay 1: no VK_COLOR_SPACE_HDR10_ST2084_EXT on this surface (display not in HDR mode, or output not HDR10-capable) — staying SDR (%s + SRGB_NONLINEAR)\n",
						vk_format_string( vk.present_format.format ) );
				}
				ri.Free( fmts2 );
			} else {
				ri.Log( SEV_WARN, "r_hdrDisplay 1: vkGetPhysicalDeviceSurfaceFormats2KHR returned %s — staying SDR\n", vk_result_string( r2 ) );
			}
		}
	}

	ri.Free( candidates );

	return qtrue;
}


static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	// Phase 9C: seed the HDR state snapshot with what the user asked
	// for. The r_hdr==1 path below overwrites r_hdr_requested with
	// the same value plus a real sfloat_supported probe; for r_hdr
	// 0/2 these defaults remain (sfloat_supported "unknown until
	// SFLOAT path runs" — surfaced as "not queried" in the report).
	vk_hdr_state.r_hdr_requested = r_hdr->integer;
	vk_hdr_state.sfloat_supported = qfalse;

	// Phase 9A: validate SFLOAT support before get_hdr_format()
	// reads r_hdr. If the chosen GPU lacks the required usage flags
	// for VK_FORMAT_R16G16B16A16_SFLOAT, downgrade r_hdr 1 → 2
	// silently. UNORM blends correctly; SFLOAT just adds the
	// out-of-[0,1] range that future phases will exploit.
	if ( r_hdr->integer == 1 ) {
		VkFormatProperties props;
		const VkFormatFeatureFlags required =
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

		vk_hdr_state.r_hdr_requested = r_hdr->integer;

		qvkGetPhysicalDeviceFormatProperties( physical_device,
			VK_FORMAT_R16G16B16A16_SFLOAT, &props );

		vk_hdr_state.sfloat_supported =
			( ( props.optimalTilingFeatures & required ) == required )
				? qtrue : qfalse;

		if ( ( props.optimalTilingFeatures & required ) != required ) {
			ri.Log( SEV_WARN,
				"r_hdr 1 (SFLOAT) not supported by GPU "
				"(optimalTilingFeatures = 0x%08x, required = 0x%08x); "
				"falling back to r_hdr 2 (UNORM)\n",
				props.optimalTilingFeatures, required );
			ri.Cvar_Set( "r_hdr", "2" );
		}
	}

	vk.color_format = get_hdr_format( vk.base_format.format );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	// Phase 6.5: probe BCn sampling support per VkFormat. Tables match
	// the DDS loader's DXGI / FourCC dispatch in tr_image_dds.c; an
	// unsupported format causes R_LoadDDS to log and return NULL so the
	// caller can fall back (typically to the engine's missing-texture
	// checkerboard). All BC* formats are 4x4 block-compressed; the
	// sampler requirements (filter-linear, sampled-image) match the
	// existing optimalTilingFeatures probe pattern.
	{
		const VkFormatFeatureFlags bc_required =
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
		VkFormatProperties bcProps;

#define VK_BC_PROBE( vkfmt, slot ) \
	do { \
		qvkGetPhysicalDeviceFormatProperties( physical_device, (vkfmt), &bcProps ); \
		vk.bc_formats_supported.slot = ( ( bcProps.optimalTilingFeatures & bc_required ) == bc_required ) ? qtrue : qfalse; \
	} while ( 0 )

		VK_BC_PROBE( VK_FORMAT_BC1_RGB_UNORM_BLOCK,  bc1_unorm   );
		VK_BC_PROBE( VK_FORMAT_BC1_RGB_SRGB_BLOCK,   bc1_srgb    );
		VK_BC_PROBE( VK_FORMAT_BC2_UNORM_BLOCK,      bc2_unorm   );
		VK_BC_PROBE( VK_FORMAT_BC2_SRGB_BLOCK,       bc2_srgb    );
		VK_BC_PROBE( VK_FORMAT_BC3_UNORM_BLOCK,      bc3_unorm   );
		VK_BC_PROBE( VK_FORMAT_BC3_SRGB_BLOCK,       bc3_srgb    );
		VK_BC_PROBE( VK_FORMAT_BC4_UNORM_BLOCK,      bc4_unorm   );
		VK_BC_PROBE( VK_FORMAT_BC4_SNORM_BLOCK,      bc4_snorm   );
		VK_BC_PROBE( VK_FORMAT_BC5_UNORM_BLOCK,      bc5_unorm   );
		VK_BC_PROBE( VK_FORMAT_BC5_SNORM_BLOCK,      bc5_snorm   );
		VK_BC_PROBE( VK_FORMAT_BC6H_UFLOAT_BLOCK,    bc6h_ufloat );
		VK_BC_PROBE( VK_FORMAT_BC6H_SFLOAT_BLOCK,    bc6h_sfloat );
		VK_BC_PROBE( VK_FORMAT_BC7_UNORM_BLOCK,      bc7_unorm   );
		VK_BC_PROBE( VK_FORMAT_BC7_SRGB_BLOCK,       bc7_srgb    );

#undef VK_BC_PROBE

		// Phase 6.5.1: surface the probe so the DDS cubemap/volume support
		// (which can carry BC6H HDR cubemaps for IBL) is visible in -developer
		// logs. BC6H + BC7 are mandatory on D3D11-feature-level-11 hardware,
		// so on any modern discrete GPU all of these read 1.
		ri.Log( SEV_DEBUG, "[VK] BC sampled-format support: BC1=%d BC2=%d BC3=%d BC4=%d BC5=%d BC6H(uf/sf)=%d/%d BC7=%d\n",
			vk.bc_formats_supported.bc1_unorm, vk.bc_formats_supported.bc2_unorm,
			vk.bc_formats_supported.bc3_unorm, vk.bc_formats_supported.bc4_unorm,
			vk.bc_formats_supported.bc5_unorm, vk.bc_formats_supported.bc6h_ufloat,
			vk.bc_formats_supported.bc6h_sfloat, vk.bc_formats_supported.bc7_unorm );
	}

	// Phase 9B: bloom intermediates inherit the HDR-aware color
	// attachment format so the bloom mip chain can carry the same
	// dynamic range as the main pass. Under r_hdr 1 (SFLOAT) this
	// removes the 8-bit clamp that previously broke HDR highlight
	// bloom at extract time.
	vk.bloom_format = vk.color_format;

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled )
	{
		vk.capture_format = vk.color_format;
	}

	ri.Log( SEV_DEBUG, "[FBO_DEBUG] Format selection:\n" );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   r_fbo=%d r_hdr=%d r_presentBits=%d\n", r_fbo->integer, r_hdr->integer, r_presentBits->integer );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   base_format=%d  color_format=%d  present_format=%d\n", vk.base_format.format, vk.color_format, vk.present_format.format );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   bloom_format=%d  capture_format=%d  depth_format=%d\n", vk.bloom_format, vk.capture_format, vk.depth_format );
	ri.Log( SEV_DEBUG, "[FBO_DEBUG]   blitEnabled=%d\n", vk.blitEnabled );

	// Phase 9C: finalise the HDR state snapshot now that every format
	// decision is settled (including any r_hdr 1 → 2 downgrade from
	// the SFLOAT probe above). Bloom-side fields are filled later at
	// vk_create_attachments time. valid=qtrue last so a partially-
	// populated snapshot never leaks through to vk_hdr_state_print.
	vk_hdr_state.r_fbo_value     = r_fbo->integer;
	vk_hdr_state.r_hdr_value     = r_hdr->integer;
	vk_hdr_state.base_format     = vk.base_format.format;
	vk_hdr_state.color_format    = vk.color_format;
	vk_hdr_state.bloom_format    = vk.bloom_format;
	vk_hdr_state.capture_format  = vk.capture_format;
	vk_hdr_state.present_format  = vk.present_format.format;
	vk_hdr_state.depth_format    = vk.depth_format;
	vk_hdr_state.valid           = qtrue;

	// Phase 9A: r_hdr requested but FBO disabled — every HDR format
	// option above is gated by the r_fbo == 0 short-circuit in
	// get_hdr_format(), so the user's r_hdr setting has no effect.
	// One-shot diagnostic for confused users.
	if ( r_hdr->integer != 0 && r_fbo->integer == 0 ) {
		ri.Log( SEV_WARN,
			"r_hdr %d has no effect: r_fbo is 0\n",
			r_hdr->integer );
	}
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


static qboolean vk_create_device( VkPhysicalDevice physical_device, int device_index ) {

	// Phase 7.4c-pre: feature chain (Vulkan 1.2 umbrella + 1.3 sync2 + 1.3
	// dynamic rendering). Spec VUID-VkDeviceCreateInfo-pNext-02831 forbids
	// chaining the individual VkPhysicalDevice{TimelineSemaphore,Vulkan
	// MemoryModel,BufferDeviceAddress,8BitStorage,…}Features structs once
	// the v12 umbrella is present, so we drive every 1.2-promoted feature
	// through v12 exclusively. Synchronization2 and dynamicRendering remain
	// individual structs (1.3) until/unless we adopt v13 too.
	VkPhysicalDeviceFeatures2                f2query, f2enable;
	VkPhysicalDeviceVulkan12Features         v12support, v12enable;
	VkPhysicalDeviceSynchronization2Features s2support, s2enable;
	VkPhysicalDeviceDynamicRenderingFeatures drsupport, drenable;

	ri.Log( SEV_INFO, "...selected physical device: %i\n", device_index );

	// select surface format
	if ( !vk_select_surface_format( physical_device, vk_surface ) ) {
		return qfalse;
	}

	setup_surface_formats( physical_device );

	// Phase 7.4c-pre: select up to 3 queue families (graphics+presentation,
	// dedicated async-compute, dedicated async-transfer). The RAL backend
	// will adopt the same vk.device, so it needs all three resolved here;
	// existing renderer code only ever uses vk.queue_family_index / vk.queue,
	// so the dedicated families sit idle on the renderer side and become
	// available to RAL once descriptor binding migrates in 7.4c proper.
	{
		VkQueueFamilyProperties *queue_families;
		uint32_t queue_family_count = 0;
		uint32_t i;

		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, NULL );
		queue_families = (VkQueueFamilyProperties*)ri.Malloc( queue_family_count * sizeof( VkQueueFamilyProperties ) );
		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

		// (1) graphics+presentation — required
		vk.queue_family_index    = ~0U;
		vk.queue_family_compute  = ~0U;
		vk.queue_family_transfer = ~0U;
		for (i = 0; i < queue_family_count; i++) {
			VkBool32 presentation_supported;
			VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, vk_surface, &presentation_supported ) );

			if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				vk.queue_family_index = i;
				break;
			}
		}

		if ( vk.queue_family_index == ~0U ) {
			ri.Free( queue_families );
			ri.Log( SEV_ERROR, "...failed to find graphics queue family\n" );
			return qfalse;
		}

		// (2) dedicated async-compute (compute bit, NO graphics bit)
		for (i = 0; i < queue_family_count; i++) {
			VkQueueFlags f = queue_families[i].queueFlags;
			if ( (f & VK_QUEUE_COMPUTE_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT) ) {
				vk.queue_family_compute = i;
				break;
			}
		}
		// (3) dedicated async-transfer (transfer bit, NO graphics/compute)
		for (i = 0; i < queue_family_count; i++) {
			VkQueueFlags f = queue_families[i].queueFlags;
			if ( (f & VK_QUEUE_TRANSFER_BIT) && !(f & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ) {
				vk.queue_family_transfer = i;
				break;
			}
		}
		// alias fallback to graphics when no dedicated family was found
		if ( vk.queue_family_compute  == ~0U ) vk.queue_family_compute  = vk.queue_family_index;
		if ( vk.queue_family_transfer == ~0U ) vk.queue_family_transfer = vk.queue_family_index;

		ri.Log( SEV_INFO, "...queue families: graphics=%u compute=%u transfer=%u%s%s\n",
			vk.queue_family_index, vk.queue_family_compute, vk.queue_family_transfer,
			vk.queue_family_compute  == vk.queue_family_index ? " (compute aliases graphics)"  : "",
			vk.queue_family_transfer == vk.queue_family_index ? " (transfer aliases graphics)" : "" );

		ri.Free( queue_families );
	}

	// create VkDevice
	{
		const char *device_extension_list[10];
		uint32_t device_extension_count;
		const char *ext, *end;
		char *str;
		const float priority = 1.0;
		VkExtensionProperties *extension_properties;
		VkPhysicalDeviceFeatures device_features;   // back-fill from f2query.features
		VkDeviceCreateInfo device_desc;
		VkResult res;
		qboolean swapchainSupported = qfalse;
		qboolean dedicatedAllocation = qfalse;
		qboolean memoryRequirements2 = qfalse;
		qboolean debugMarker = qfalse;
		qboolean hdrMetadataSupported = qfalse;  // Phase 6B3'-d8
		qboolean fseSupported = qfalse;          // Phase 6B3'-d8 (VK_EXT_full_screen_exclusive)
		qboolean memoryBudgetSupported = qfalse; // Phase 7.4c-pipeline — VK_EXT_memory_budget for RAL's Ral_QueryMemoryBudget
		uint32_t i, len, count = 0;

		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) );
		extension_properties = (VkExtensionProperties*)ri.Malloc( count * sizeof( VkExtensionProperties ) );
		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extension_properties ) );

		// fill glConfig.extensions_string
		str = glConfig.extensions_string; *str = '\0';
		end = &glConfig.extensions_string[ sizeof( glConfig.extensions_string ) - 1];

		for ( i = 0; i < count; i++ ) {
			ext = extension_properties[i].extensionName;
			if ( strcmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 ) {
				swapchainSupported = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME ) == 0 ) {
				dedicatedAllocation = qtrue;
			} else if ( strcmp( ext, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME ) == 0 ) {
				memoryRequirements2 = qtrue;
			} else if ( strcmp( ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				debugMarker = qtrue;
			} else if ( strcmp( ext, VK_EXT_HDR_METADATA_EXTENSION_NAME ) == 0 ) {
				hdrMetadataSupported = qtrue;  // Phase 6B3'-d8
			} else if ( strcmp( ext, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME ) == 0 ) {
				memoryBudgetSupported = qtrue; // Phase 7.4c-pipeline
#if defined( _WIN32 )
			} else if ( strcmp( ext, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME ) == 0 ) {
				fseSupported = qtrue;          // Phase 6B3'-d8
#endif
			}
			// Phase 7.4c-pre: 1.2-promoted KHR extensions (timeline_semaphore,
			// vulkan_memory_model, buffer_device_address, 8bit_storage) are
			// no longer scanned/enabled here — the corresponding features are
			// queried/enabled via VkPhysicalDeviceVulkan12Features below.
			// add this device extension to glConfig
			if ( i != 0 ) {
				if ( str + 1 >= end )
					continue;
				str = Q_stradd( str, " " );
			}
			len = (uint32_t)strlen( ext );
			if ( str + len >= end )
				continue;
			str = Q_stradd( str, ext );
		}

		ri.Free( extension_properties );

		device_extension_count = 0;

		if ( !swapchainSupported ) {
			ri.Log( SEV_ERROR, "...required device extension is not available: %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
			return qfalse;
		}

		if ( !memoryRequirements2 )
			dedicatedAllocation = qfalse;
		else
			vk.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk.dedicatedAllocation = qfalse;
#endif

		device_extension_list[ device_extension_count++ ] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		// Phase 6B3'-d8: VK_EXT_hdr_metadata — request whenever available
		// (harmless if r_hdrDisplay 0; without it qvkSetHdrMetadataEXT
		// stays NULL and the HDR10 path is skipped). vkGetDeviceProcAddr
		// for vkSetHdrMetadataEXT only resolves when this is enabled.
		if ( hdrMetadataSupported ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_HDR_METADATA_EXTENSION_NAME;
		}

		// Phase 7.4c-pipeline: VK_EXT_memory_budget — enables real-numbers
		// device-local / host-visible usage reporting via VK_KHR_memory_budget
		// in VkPhysicalDeviceMemoryProperties2. The shared backend's
		// Ral_QueryMemoryBudget reads this; without the extension it falls
		// back to estimates. Pure query-side extension — no runtime cost.
		if ( memoryBudgetSupported ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;
		}

#if defined( _WIN32 )
		// Phase 6B3'-d8: VK_EXT_full_screen_exclusive — enabling it (when
		// available) lets the FSE pNext chain be attached to the surface-
		// format query and to swapchain creation, which is what makes the
		// HDR10_ST2084 colorspace appear on the engine's win32 window
		// surface on NVIDIA/Windows. With VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT
		// the driver only enters exclusive fullscreen when conditions are
		// right; a windowed SDR session is unaffected.
		if ( fseSupported ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME;
			vk_fse_ext_enabled = qtrue;
		}
#endif

		if ( vk.dedicatedAllocation ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
		}

		if ( debugMarker ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			vk.debugMarkers = qtrue;
		}

		// ── Phase 7.4c-pre: chained Features2 query ────────────────────
		memset( &v12support, 0, sizeof( v12support ) ); v12support.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		memset( &s2support,  0, sizeof( s2support  ) ); s2support.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		memset( &drsupport,  0, sizeof( drsupport  ) ); drsupport.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		v12support.pNext = &s2support;  s2support.pNext = &drsupport;  drsupport.pNext = NULL;
		memset( &f2query, 0, sizeof( f2query ) );  f2query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;  f2query.pNext = &v12support;
		qvkGetPhysicalDeviceFeatures2( physical_device, &f2query );
		device_features = f2query.features;   // back-fill the legacy struct used below

		if ( device_features.fillModeNonSolid == VK_FALSE ) {
			ri.Log( SEV_ERROR, "...fillModeNonSolid feature is not supported\n" );
			return qfalse;
		}

		// synchronization2 + timelineSemaphore + dynamicRendering — required to share with RAL.
		if ( s2support.synchronization2 != VK_TRUE
		  || v12support.timelineSemaphore != VK_TRUE
		  || drsupport.dynamicRendering != VK_TRUE ) {
			ri.Log( SEV_ERROR,
				"...device lacks synchronization2 (%d) / timelineSemaphore (%d) / dynamicRendering (%d) — Wired requires a Vulkan 1.3-class GPU\n",
				(int)s2support.synchronization2, (int)v12support.timelineSemaphore, (int)drsupport.dynamicRendering );
			return qfalse;
		}

		// ── queue create infos: one per distinct family we resolved ────
		{
			VkDeviceQueueCreateInfo queue_descs[3];
			uint32_t                queue_desc_count = 0;
			memset( queue_descs, 0, sizeof( queue_descs ) );
			queue_descs[ queue_desc_count ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_descs[ queue_desc_count ].queueFamilyIndex = vk.queue_family_index;
			queue_descs[ queue_desc_count ].queueCount       = 1;
			queue_descs[ queue_desc_count ].pQueuePriorities = &priority;
			queue_desc_count++;
			if ( vk.queue_family_compute != vk.queue_family_index ) {
				queue_descs[ queue_desc_count ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_descs[ queue_desc_count ].queueFamilyIndex = vk.queue_family_compute;
				queue_descs[ queue_desc_count ].queueCount       = 1;
				queue_descs[ queue_desc_count ].pQueuePriorities = &priority;
				queue_desc_count++;
			}
			if ( vk.queue_family_transfer != vk.queue_family_index
			  && vk.queue_family_transfer != vk.queue_family_compute ) {
				queue_descs[ queue_desc_count ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_descs[ queue_desc_count ].queueFamilyIndex = vk.queue_family_transfer;
				queue_descs[ queue_desc_count ].queueCount       = 1;
				queue_descs[ queue_desc_count ].pQueuePriorities = &priority;
				queue_desc_count++;
			}

			// ── enable chain: f2enable → v12enable → s2enable → drenable ──
			memset( &f2enable,  0, sizeof( f2enable  ) ); f2enable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			memset( &v12enable, 0, sizeof( v12enable ) ); v12enable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			memset( &s2enable,  0, sizeof( s2enable  ) ); s2enable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
			memset( &drenable,  0, sizeof( drenable  ) ); drenable.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

			// classic features (back-filled into f2enable.features)
			f2enable.features.fillModeNonSolid = VK_TRUE;
			if ( device_features.shaderInt64 ) {
				f2enable.features.shaderInt64 = VK_TRUE;
			}
			if ( device_features.wideLines ) { // RB_SurfaceAxis
				f2enable.features.wideLines = VK_TRUE;
				vk.wideLines = qtrue;
			}
			if ( device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics ) {
				f2enable.features.vertexPipelineStoresAndAtomics = VK_TRUE;
				f2enable.features.fragmentStoresAndAtomics       = VK_TRUE;
				vk.fragmentStores = qtrue;
			}
			if ( r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy ) {
				f2enable.features.samplerAnisotropy = VK_TRUE;
				vk.samplerAnisotropy = qtrue;
			}

			// 1.2 features — descriptorIndexing suite (bindless), timeline,
			// hostQueryReset, drawIndirectCount; mirror device support.
			if ( v12support.shaderSampledImageArrayNonUniformIndexing
			  && v12support.runtimeDescriptorArray
			  && v12support.descriptorBindingPartiallyBound
			  && v12support.descriptorBindingSampledImageUpdateAfterBind ) {
				v12enable.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
				v12enable.runtimeDescriptorArray                       = VK_TRUE;
				v12enable.descriptorBindingPartiallyBound              = VK_TRUE;
				v12enable.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
				v12enable.descriptorBindingUpdateUnusedWhilePending    = v12support.descriptorBindingUpdateUnusedWhilePending;
				v12enable.descriptorBindingVariableDescriptorCount     = v12support.descriptorBindingVariableDescriptorCount;
			}
			v12enable.timelineSemaphore = VK_TRUE;
			if ( v12support.hostQueryReset    ) v12enable.hostQueryReset    = VK_TRUE;
			if ( v12support.drawIndirectCount ) v12enable.drawIndirectCount = VK_TRUE;
			// the formerly _DEBUG-only features (now 1.2 core)
			if ( v12support.vulkanMemoryModel )       v12enable.vulkanMemoryModel       = VK_TRUE;
			if ( v12support.vulkanMemoryModelDeviceScope ) v12enable.vulkanMemoryModelDeviceScope = VK_TRUE;
			if ( v12support.bufferDeviceAddress )     v12enable.bufferDeviceAddress     = VK_TRUE;
			if ( v12support.storageBuffer8BitAccess ) v12enable.storageBuffer8BitAccess = VK_TRUE;
			if ( v12support.uniformAndStorageBuffer8BitAccess ) v12enable.uniformAndStorageBuffer8BitAccess = VK_TRUE;

			s2enable.synchronization2 = VK_TRUE;
			drenable.dynamicRendering = VK_TRUE;

			f2enable.pNext = &v12enable;  v12enable.pNext = &s2enable;  s2enable.pNext = &drenable;  drenable.pNext = NULL;

			device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			device_desc.pNext = &f2enable;
			device_desc.flags = 0;
			device_desc.queueCreateInfoCount = queue_desc_count;
			device_desc.pQueueCreateInfos = queue_descs;
			device_desc.enabledLayerCount = 0;
			device_desc.ppEnabledLayerNames = NULL;
			device_desc.enabledExtensionCount = device_extension_count;
			device_desc.ppEnabledExtensionNames = device_extension_list;
			device_desc.pEnabledFeatures = NULL;   // features are passed through f2enable.features

			res = qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );
			if ( res < 0 ) {
				ri.Log( SEV_ERROR, "vkCreateDevice returned %s\n", vk_result_string( res ) );
				return qfalse;
			}
		}
	}

	return qtrue;
}


#define INIT_INSTANCE_FUNCTION(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func); \
	if (q##func == NULL) {											\
		ri.Terminate( TERM_UNRECOVERABLE, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func);


#define INIT_DEVICE_FUNCTION(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);\
	if (q##func == NULL) {											\
		ri.Terminate( TERM_UNRECOVERABLE, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);


static void vk_destroy_instance( void ) {
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if ( vk_debug_messenger != VK_NULL_HANDLE ) {
		if ( qvkDestroyDebugUtilsMessengerEXT != NULL ) {
			qvkDestroyDebugUtilsMessengerEXT( vk_instance, vk_debug_messenger, NULL );
		}
		vk_debug_messenger = VK_NULL_HANDLE;
	}
#endif

	if ( vk_instance != VK_NULL_HANDLE ) {
		if ( qvkDestroyInstance ) {
			qvkDestroyInstance( vk_instance, NULL );
		}
		vk_instance = VK_NULL_HANDLE;
	}
}


static void init_vulkan_library( void )
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice *physical_devices;
	uint32_t device_count;
	int device_index, i;
	VkResult res;

	memset( &vk, 0, sizeof( vk ) );

	if ( vk_instance == VK_NULL_HANDLE ) {

		// force cleanup
		vk_destroy_instance();

		// Get functions that do not depend on VkInstance (vk_instance == nullptr at this point).
		INIT_INSTANCE_FUNCTION( vkCreateInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION( vkCreateDevice )
		INIT_INSTANCE_FUNCTION( vkDestroyInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
		INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures2 )   // Phase 7.4c-pre: 1.1 core, used for v12/sync2/dr query
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceSurfaceFormats2KHR )	// Phase 6B3'-d8 — NULL if VK_KHR_get_surface_capabilities2 not enabled
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugUtilsMessengerEXT )
		INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugUtilsMessengerEXT )

		/* Wire validation-layer output into the engine log.  Skipped silently
		 * when the loader didn't expose VK_EXT_debug_utils — same behavior as
		 * before for drivers that lack the extension. */
		if ( qvkCreateDebugUtilsMessengerEXT && qvkDestroyDebugUtilsMessengerEXT ) {
			VkDebugUtilsMessengerCreateInfoEXT desc;
			desc.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			desc.pNext           = NULL;
			desc.flags           = 0;
			desc.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			                     | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			desc.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			desc.pfnUserCallback = debug_utils_callback;
			desc.pUserData       = NULL;

			VK_CHECK( qvkCreateDebugUtilsMessengerEXT( vk_instance, &desc, NULL, &vk_debug_messenger ) );
		}
#endif

		// create surface
		if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
			ri.Terminate( TERM_UNRECOVERABLE, "Error creating Vulkan surface" );
			return;
		}
	} // vk_instance == VK_NULL_HANDLE

	res = qvkEnumeratePhysicalDevices( vk_instance, &device_count, NULL );
	if ( device_count == 0 ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: no physical devices found" );
		return;
	}
	if ( res < 0 ) {
		ri.Terminate( TERM_UNRECOVERABLE, "vkEnumeratePhysicalDevices returned %s", vk_result_string( res ) );
		return;
	}

	physical_devices = (VkPhysicalDevice*)ri.Malloc( device_count * sizeof( VkPhysicalDevice ) );
	VK_CHECK( qvkEnumeratePhysicalDevices( vk_instance, &device_count, physical_devices ) );

	// initial physical device index
	device_index = r_device->integer;

	ri.Log( SEV_INFO, ".......................\nAvailable physical devices:\n" );
	for ( i = 0; i < device_count; i++ ) {
		qvkGetPhysicalDeviceProperties( physical_devices[ i ], &props );
		ri.Log( SEV_INFO, " %i: %s\n", i, renderer_name( &props ) );
		if ( device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			device_index = i;
		} else if ( device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			device_index = i;
		}
	}
	ri.Log( SEV_INFO, ".......................\n" );

	vk.physical_device = VK_NULL_HANDLE;
	for ( i = 0; i < device_count; i++, device_index++ ) {
		if ( device_index >= device_count || device_index < 0 ) {
			device_index = 0;
		}
		if ( vk_create_device( physical_devices[ device_index ], device_index ) ) {
			vk.physical_device = physical_devices[ device_index ];
			break;
		}
	}

	ri.Free( physical_devices );

	if ( vk.physical_device == VK_NULL_HANDLE ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: unable to find any suitable physical device" );
		return;
	}

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDispatch)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateComputePipelines)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	// Phase 6B3'-d8: VK_EXT_hdr_metadata — resolves only if the extension
	// was enabled at device creation (gated by hdrMetadataSupported above).
	// NULL = HDR10 metadata not settable; the HDR10 swapchain still works,
	// it just won't carry mastering metadata to the display.
	INIT_DEVICE_FUNCTION_EXT(vkSetHdrMetadataEXT)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFeatures2 = NULL;  // Phase 7.4c-pre
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormats2KHR = NULL;	// Phase 6B3'-d8
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugUtilsMessengerEXT = NULL;
	qvkDestroyDebugUtilsMessengerEXT = NULL;
#endif
}


static void deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdDispatch								= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdResetQueryPool						= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCmdWriteTimestamp						= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateComputePipelines					= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateQueryPool							= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyQueryPool							= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkGetQueryPoolResults						= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;
	qvkAcquireNextImageKHR						= NULL;
	qvkCreateSwapchainKHR						= NULL;
	qvkDestroySwapchainKHR						= NULL;
	qvkGetSwapchainImagesKHR					= NULL;
	qvkQueuePresentKHR							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
}


// Phase 7.4c-pipeline — side table mapping VkShaderModule → its source SPV
// bytes so that create_pipeline can hand the same blobs to
// Ral_CreateGraphicsPipeline. Reset at the top of vk_create_shader_modules
// (which runs at vid_init and again on every vid_restart). Sized for the
// renderervk shader module count (~138 modules; 256 leaves headroom).
#define VK_SHADER_BLOB_CAPACITY 256
typedef struct {
	VkShaderModule  handle;
	const uint8_t  *bytes;
	uint32_t        size;
} vk_shader_blob_record_t;
static vk_shader_blob_record_t vk_shader_blob_table[ VK_SHADER_BLOB_CAPACITY ];
static uint32_t                vk_shader_blob_count;
static qboolean                vk_shader_blob_full_warned;

static void vk_shader_blob_reset( void ) {
	memset( vk_shader_blob_table, 0, sizeof( vk_shader_blob_table ) );
	vk_shader_blob_count = 0;
	vk_shader_blob_full_warned = qfalse;
}

static void vk_shader_blob_record( VkShaderModule handle, const uint8_t *bytes, uint32_t size ) {
	if ( vk_shader_blob_count >= VK_SHADER_BLOB_CAPACITY ) {
		if ( !vk_shader_blob_full_warned ) {
			vk_shader_blob_full_warned = qtrue;
			ri.Log( SEV_WARN, "[VK] vk_shader_blob_table overflow (%u); raise VK_SHADER_BLOB_CAPACITY — RAL pipelines using this module will fall back to bytes=NULL\n", VK_SHADER_BLOB_CAPACITY );
		}
		return;
	}
	vk_shader_blob_table[ vk_shader_blob_count ].handle = handle;
	vk_shader_blob_table[ vk_shader_blob_count ].bytes  = bytes;
	vk_shader_blob_table[ vk_shader_blob_count ].size   = size;
	vk_shader_blob_count++;
}

// Public to create_pipeline (and the 16 special-case sites): given a
// VkShaderModule the renderer is using, return the SPV bytes + size that
// originally produced it, so the same SPV can feed Ral_CreateGraphics*Pipeline.
// Returns qfalse if the module isn't in the table.
qboolean vk_shader_blob_lookup( VkShaderModule handle, const uint8_t **out_bytes, uint32_t *out_size ) {
	uint32_t i;
	for ( i = 0; i < vk_shader_blob_count; i++ ) {
		if ( vk_shader_blob_table[i].handle == handle ) {
			if ( out_bytes ) *out_bytes = vk_shader_blob_table[i].bytes;
			if ( out_size  ) *out_size  = vk_shader_blob_table[i].size;
			return qtrue;
		}
	}
	return qfalse;
}

static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	// Phase 7.4c-pipeline: record (handle → bytes, size) so RAL pipelines can
	// re-use the same SPV blobs without an extra shader-binary lookup table.
	vk_shader_blob_record( module, bytes, (uint32_t)count );

	return module;
}


static void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bind;
	VkDescriptorSetLayoutCreateInfo desc;

	bind.binding = binding;
	bind.descriptorType = type;
	bind.descriptorCount = 1;
	bind.stageFlags = flags;
	bind.pImmutableSamplers = NULL;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = 1;
	desc.pBindings = &bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}


void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;

	info.buffer = buffer;
	info.offset = 0;
	info.range = sizeof( vkUniform_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;

	// Look for sampler among existing samplers.
	for ( int i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	// Create new sampler.
	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Terminate( TERM_CLIENT_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->gl_mag_filter == GL_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Terminate( TERM_UNRECOVERABLE, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Terminate( TERM_UNRECOVERABLE, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;

	if ( def->noAnisotropy || mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || mag_filter == VK_FILTER_NEAREST ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[ vk.samplers.count ] = *def;
	vk.samplers.handle[ vk.samplers.count ] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	for ( int i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		memset( &vk.samplers.def[i], 0x0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}


void vk_update_attachment_descriptors( void ) {

	ri.Log( SEV_TRACE, "[FBO_TRACE] vk_update_attachment_descriptors: color_image_view=%p fboActive=%d\n",
		(void*)vk.color_image_view, vk.fboActive );

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = vk.blitFilter;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.color_image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = vk.color_descriptor;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// Phase 6B3'-c1: tonemap output sampler used by gamma + capture
		// passes downstream.
		if ( vk.tonemapped_image_view != VK_NULL_HANDLE )
		{
			info.imageView = vk.tonemapped_image_view;
			desc.dstSet = vk.tonemapped_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t i;
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				info.imageView = vk.bloom_image_view[i];
				desc.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

		// depth fade copy image
		if ( vk.depthFade.active && vk.depthFade.image != VK_NULL_HANDLE )
		{
			info.sampler = vk.depthFade.sampler;
			info.imageView = vk.depthFade.view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.depthFade.descriptor;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;

			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

#if FEAT_SHADOW_MAPPING
		// shadow map depth image
		if ( vk.shadowMap.active && vk.shadowMap.image != VK_NULL_HANDLE )
		{
			info.sampler = vk.shadowMap.sampler;
			info.imageView = vk.shadowMap.view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.shadowMap.descriptor;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}
#endif

		// SMAA descriptors
		if ( vk.smaa.active && vk.smaa.edges_image != VK_NULL_HANDLE )
		{
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstBinding = 0;
			desc.dstArrayElement = 0;
			desc.descriptorCount = 1;

			// edges: point sampler
			info.sampler = vk.smaa.point_sampler;
			info.imageView = vk.smaa.edges_view;
			desc.dstSet = vk.smaa.edges_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// blend weights: linear sampler
			info.sampler = vk.smaa.linear_sampler;
			info.imageView = vk.smaa.blend_view;
			desc.dstSet = vk.smaa.blend_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// input (color copy): linear sampler
			info.sampler = vk.smaa.linear_sampler;
			info.imageView = vk.smaa.input_view;
			desc.dstSet = vk.smaa.input_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// area LUT: linear sampler
			info.sampler = vk.smaa.linear_sampler;
			info.imageView = vk.smaa.area_view;
			desc.dstSet = vk.smaa.area_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// search LUT: point sampler
			info.sampler = vk.smaa.point_sampler;
			info.imageView = vk.smaa.search_view;
			desc.dstSet = vk.smaa.search_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}
	}
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor ) );

		// Phase 6B3'-c1: LDR-linear tonemap output, sampled by the
		// gamma + capture passes downstream.
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tonemapped_descriptor ) );

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

		if ( vk.depthFade.active ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.depthFade.descriptor ) );
		}

#if FEAT_SHADOW_MAPPING
		// vk.shadowMap.descriptor — the single alloc site for the shadow sampler
		// SET (Phase 6.5.4 Part B-refactor; mirrors the SMAA block right below).
		// Allocated whenever the FBO is on, not just when r_shadowMapping is set,
		// so a cold r_shadowMapping 0 boot followed by a live 0->1 toggle already
		// has a valid dstSet waiting (the +1 /* vk.shadowMap.descriptor */ pool
		// reservation in vk_initialize covers it). The SET persists across
		// vk_shadow_release_resources / vk_shadow_alloc_resources cycles (the FBO
		// rebuild / r_shadowMapping toggle don't re-run this function and don't
		// reset the pool); vk_update_attachment_descriptors rebinds it to the
		// fresh vk.shadowMap.view, and the dispatch gate (!vk.shadowMap.active in
		// vk_render_shadow_map / the lit pass) keeps anything from reading it when
		// the shadow image is gone. The pool itself is reset by vk_release_
		// resources() (RE_Shutdown path, via qvkResetDescriptorPool — flags == 0,
		// so a full reset is the only reclaim path); on REF_KEEP_CONTEXT restarts
		// vk_initialize is skipped, so this re-alloc is the post-reset
		// repopulation, which is why it's unconditional under `if (vk.fboActive)`.
		if ( vk.fboActive ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.shadowMap.descriptor ) );
		}
#endif

		// Phase 6B3'-f split-A3: allocate SMAA descriptor sets whenever
		// the FBO is on, not just when SMAA is currently active. The pool
		// slots are reserved up-front (see pool sizing in vk_initialize),
		// and live r_smaa toggling re-binds these descriptors to fresh
		// image views in vk_update_attachment_descriptors after each
		// alloc cycle. Allocating eagerly avoids a "first 0->N toggle
		// has no descriptors" startup race.
		if ( vk.fboActive ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.edges_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.blend_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.input_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.area_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.search_descriptor ) );
		}

		vk_update_attachment_descriptors();
	}

#if FEAT_IQM
	// re-allocate IQM bone UBO descriptors after descriptor pool reset
	if ( vk.iqmGpu.available ) {
		VkDescriptorSetAllocateInfo boneAlloc;
		VkDescriptorBufferInfo boneBufDesc;
		VkWriteDescriptorSet boneWriteDesc;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			memset( &boneAlloc, 0, sizeof( boneAlloc ) );
			boneAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			boneAlloc.descriptorPool = vk.descriptor_pool;
			boneAlloc.descriptorSetCount = 1;
			boneAlloc.pSetLayouts = &vk.iqmGpu.set_layout_bones;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &boneAlloc, &vk.iqmGpu.bone_descriptor[j] ) );

			memset( &boneBufDesc, 0, sizeof( boneBufDesc ) );
			boneBufDesc.buffer = vk.iqmGpu.bone_buffer[j];
			boneBufDesc.offset = 0;
			boneBufDesc.range = IQM_BONE_UBO_SIZE;

			memset( &boneWriteDesc, 0, sizeof( boneWriteDesc ) );
			boneWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			boneWriteDesc.dstSet = vk.iqmGpu.bone_descriptor[j];
			boneWriteDesc.dstBinding = 0;
			boneWriteDesc.dstArrayElement = 0;
			boneWriteDesc.descriptorCount = 1;
			boneWriteDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			boneWriteDesc.pBufferInfo = &boneBufDesc;

			qvkUpdateDescriptorSets( vk.device, 1, &boneWriteDesc, 0, NULL );
		}
	}
#endif

	// re-allocate ribbon SSBO descriptors after descriptor pool reset.
	// The buffers (points, headers) and the set/pipeline layouts survive
	// the reset; only the descriptor sets need recreation + rewrite.
	if ( vk.ribbon.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[2];
		VkDescriptorBufferInfo bufInfos[2];
		const uint32_t pointsBytes  = RIBBON_POINTS_PER_FRAME  * RIBBON_POINT_BYTES;
		const uint32_t headersBytes = RIBBON_HEADERS_PER_FRAME * RIBBON_HEADER_BYTES;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.ribbon.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.ribbon.descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.ribbon.points_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = pointsBytes;
			bufInfos[1].buffer = vk.ribbon.headers_buffer[j];
			bufInfos[1].offset = 0;
			bufInfos[1].range  = headersBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.ribbon.descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];
			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.ribbon.descriptor[j];
			writes[1].dstBinding      = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];

			qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
		}
	}

	// re-allocate sprite SSBO descriptor after descriptor pool reset.
	// The buffer (headers) and the set/pipeline layouts survive the
	// reset; only the descriptor sets need recreation + rewrite.
	if ( vk.sprite.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[1];
		VkDescriptorBufferInfo bufInfos[1];
		const uint32_t headersBytes = SPRITES_PER_FRAME * SPRITE_HEADER_BYTES;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.sprite.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.sprite.descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.sprite.headers_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = headersBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.sprite.descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];

			qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );
		}
	}

	// re-allocate beam SSBO descriptor after descriptor pool reset.
	// The header buffers and set/pipeline layouts survive the reset;
	// only the descriptor sets need recreation + rewrite of the
	// header SSBO binding (binding 0). Binding 1 (the sampler array)
	// is rewritten later by vk_init_primitive_shader_images, which
	// runs from R_Init AFTER this function. Without this block,
	// vk.beam.descriptor[j] holds stale handles after vid_restart
	// and the first call into the sampler-array write site fires
	// "Invalid VkDescriptorSet Object" validation errors.
	if ( vk.beam.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[3];
		VkDescriptorBufferInfo bufInfos[3];
		const uint32_t headerBytes = BEAM_POOL_MAX * BEAM_HEADER_BYTES;
		uint32_t j;

		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			uint32_t writeCount;

			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.beam.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.beam.descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.beam.header_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = headerBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.beam.descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];

			writeCount = 1;

			// Phase 5G: bindings 2 (per-stage data) and 3 (stage counts)
			// must be re-written here too. The buffers themselves
			// survive vid_restart (allocated in
			// vk_init_primitive_shader_stages, which runs after this
			// from R_Init's R_InitImages → vk_init_primitive_shader_images
			// path); skip their writes if the buffers don't exist yet
			// — vk_init_primitive_shader_images will handle the late
			// case via its own descriptor write or the sampler-array
			// write site (which fires after R_Init).
			if ( vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
				bufInfos[1].buffer = vk.primitive_stages_buffer;
				bufInfos[1].offset = 0;
				bufInfos[1].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;

				writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[1].dstSet          = vk.beam.descriptor[j];
				writes[1].dstBinding      = 2;
				writes[1].descriptorCount = 1;
				writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				writes[1].pBufferInfo     = &bufInfos[1];

				bufInfos[2].buffer = vk.primitive_stage_counts_buffer;
				bufInfos[2].offset = 0;
				bufInfos[2].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* sizeof( uint32_t );

				writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[2].dstSet          = vk.beam.descriptor[j];
				writes[2].dstBinding      = 3;
				writes[2].descriptorCount = 1;
				writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				writes[2].pBufferInfo     = &bufInfos[2];

				writeCount = 3;
			}

			qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
		}

		// Pool slots' active flags survived the descriptor pool reset
		// in host memory, but the underlying GPU resources (descriptor
		// sets) got invalidated. Any in-flight transient or persistent
		// beam from before vid_restart is now decoupled from valid
		// state — drop them all so RB_DrawBeams starts fresh.
		memset( vk.beam.active,    0, sizeof( vk.beam.active ) );
		memset( vk.beam.spawnTime, 0, sizeof( vk.beam.spawnTime ) );
		memset( vk.beam.duration,  0, sizeof( vk.beam.duration ) );
		memset( vk.beam.fadeIn,    0, sizeof( vk.beam.fadeIn ) );
		memset( vk.beam.fadeOut,   0, sizeof( vk.beam.fadeOut ) );
		vk.beam.drawCount = 0;
	}

	// re-allocate particle compute + render descriptor sets after
	// descriptor pool reset. The buffers (pool ping-pong, classes
	// shadow, per-frame UBOs) and the set/pipeline layouts survive
	// the reset; only the descriptor sets need recreation + rewrite.
	// Without this block, particle bindings hold stale handles after
	// vk_release_resources / vid_restart and validation fires every
	// frame (Family 2 / Family 3).
	if ( vk.particle.available ) {
		VkDescriptorSetAllocateInfo dsAlloc;
		VkWriteDescriptorSet writes[4];
		VkDescriptorBufferInfo bufInfos[4];
		const uint32_t poolBytes    = PARTICLES_PER_POOL * PARTICLE_BYTES;
		const uint32_t classesBytes = MAX_PARTICLE_CLASSES * PARTICLE_CLASS_GPU_BYTES;
		const uint32_t frameBytes   = sizeof( particleFrame_t );
		uint32_t j;

		// compute descriptor sets — same layout as the freshly-init
		// pattern in vk_init_particle: UBO at 0, read pool at 1,
		// write pool at 2, class shadow at 3. Index i reads pool[i],
		// writes pool[1-i].
		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			uint32_t readPool  = j;
			uint32_t writePool = 1 - j;

			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.particle.compute_set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.compute_descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.particle.frame_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = frameBytes;
			bufInfos[1].buffer = vk.particle.pool_buffer[readPool];
			bufInfos[1].offset = 0;
			bufInfos[1].range  = poolBytes;
			bufInfos[2].buffer = vk.particle.pool_buffer[writePool];
			bufInfos[2].offset = 0;
			bufInfos[2].range  = poolBytes;
			bufInfos[3].buffer = vk.particle.classes_buffer;
			bufInfos[3].offset = 0;
			bufInfos[3].range  = classesBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.particle.compute_descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];
			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.particle.compute_descriptor[j];
			writes[1].dstBinding      = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];
			writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet          = vk.particle.compute_descriptor[j];
			writes[2].dstBinding      = 2;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2].pBufferInfo     = &bufInfos[2];
			writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[3].dstSet          = vk.particle.compute_descriptor[j];
			writes[3].dstBinding      = 3;
			writes[3].descriptorCount = 1;
			writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[3].pBufferInfo     = &bufInfos[3];

			qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
		}

		// render descriptor sets — UBO at 0, read pool at 1, classes
		// at 2. Index i reads pool[1-i] (the post-compute output when
		// compute_descriptor[i] just ran).
		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			uint32_t renderPool = 1 - j;

			memset( &dsAlloc, 0, sizeof( dsAlloc ) );
			dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			dsAlloc.descriptorPool     = vk.descriptor_pool;
			dsAlloc.descriptorSetCount = 1;
			dsAlloc.pSetLayouts        = &vk.particle.render_set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.render_descriptor[j] ) );

			memset( bufInfos, 0, sizeof( bufInfos ) );
			bufInfos[0].buffer = vk.particle.frame_buffer[j];
			bufInfos[0].offset = 0;
			bufInfos[0].range  = frameBytes;
			bufInfos[1].buffer = vk.particle.pool_buffer[renderPool];
			bufInfos[1].offset = 0;
			bufInfos[1].range  = poolBytes;
			bufInfos[2].buffer = vk.particle.classes_buffer;
			bufInfos[2].offset = 0;
			bufInfos[2].range  = classesBytes;

			memset( writes, 0, sizeof( writes ) );
			writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet          = vk.particle.render_descriptor[j];
			writes[0].dstBinding      = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].pBufferInfo     = &bufInfos[0];
			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.particle.render_descriptor[j];
			writes[1].dstBinding      = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];
			writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet          = vk.particle.render_descriptor[j];
			writes[2].dstBinding      = 2;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2].pBufferInfo     = &bufInfos[2];

			qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

			// Note on binding 3 (per-class sampler array): population
			// is deferred — vk_init_descriptors runs from InitOpenGL
			// BEFORE R_InitImages re-creates tr.whiteImage on a
			// vid_restart, so we cannot read tr.whiteImage->view
			// here. R_Init calls vk_init_particle_textures AFTER
			// R_InitImages to populate this binding eagerly against
			// the now-fresh tr.whiteImage.
		}
		// Pool reset invalidated the previously-populated sampler
		// array; R_DeleteTextures + R_InitImages also invalidate the
		// cached image_t pointers in classImages[] (the old image_t
		// structs were freed; new ones live at different addresses).
		// Clear both. R_Init's call to vk_init_particle_textures
		// (after R_InitImages re-creates tr.whiteImage) repopulates
		// the array; cgame re-registration later writes each class's
		// own slot in classImages[].
		memset( vk.particle.classImages, 0, sizeof( vk.particle.classImages ) );
		vk.particle.numClasses = 0;
	}

	// Phase 7.4c-bindgroup — adopt every allocate-once VkDescriptorSet into
	// a ralBindGroup_t with ownsSet=qfalse so 7.4c-cmd can pass them through
	// Ral_CmdBindBindGroup at the bind sites. Idempotent: re-init (vid_restart,
	// REF_KEEP_CONTEXT-then-recover, descriptor-pool reset) tears the registry
	// down and re-wraps the now-fresh handles. Logs the adoption count.
	// Per-draw rotating sets (vk.cmd->descriptor_set.current[]) are NOT
	// adopted here — they need per-frame re-adoption tied to the cmd-buffer
	// ring that 7.4c-cmd introduces.
	vk_ral_adopt_static_bindgroups();
}


static void vk_release_geometry_buffers( void )
{
	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk_ral_unregister_buffer( vk.tess[i].vertex_buffer );
		qvkDestroyBuffer( vk.device, vk.tess[i].vertex_buffer, NULL );
		vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	qvkFreeMemory( vk.device, vk.geometry_buffer_memory, NULL );
	vk.geometry_buffer_memory = VK_NULL_HANDLE;
}


static void vk_create_geometry_buffers( VkDeviceSize size )
{
	VkMemoryRequirements vb_memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	memset( &vb_memory_requirements, 0, sizeof( vb_memory_requirements ) );

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		desc.size = size;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.tess[i].vertex_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements );
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );

	vertex_buffer_offset = 0;

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkBindBufferMemory( vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset );
		vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
		vk.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;
		vk_ral_register_buffer( vk.tess[i].vertex_buffer, size,
		                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.tess.vertex_buffer" );

		SET_OBJECT_NAME( vk.tess[i].vertex_buffer, va( "geometry buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	}

	SET_OBJECT_NAME( vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	vk.geometry_buffer_size = vb_memory_requirements.size;

	memset( &vk.stats, 0, sizeof( vk.stats ) );
}


static void vk_create_storage_buffer( uint32_t size )
{
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	memset( &memory_requirements, 0, sizeof( memory_requirements ) );

	desc.size = size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.storage.buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.storage.buffer, &memory_requirements );

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.storage.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr ) );

	memset( vk.storage.buffer_ptr, 0, memory_requirements.size );

	qvkBindBufferMemory( vk.device, vk.storage.buffer, vk.storage.memory, 0 );
	vk_ral_register_buffer( vk.storage.buffer, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                        "vk.storage" );

	SET_OBJECT_NAME( vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	SET_OBJECT_NAME( vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


/*
===============
Primitive ribbon — self-contained pipeline.

Cgame submits world-space ribbons of N control points each via
RE_AddRibbonToScene. Each call appends `numPoints` GPU RibbonPoint
records to the per-frame points SSBO and one GPU RibbonHeader to
the per-frame headers SSBO. RB_DrawRibbons (called from
RB_DrawSurfs after world translucents) issues one direct
vkCmdDraw per submitted ribbon, with `firstInstance = headerIdx`
so the vertex shader can read its header via gl_InstanceIndex.

Push range layout (vertex stage only, 80 bytes):
    bytes  0..63  mat4  mvp        — world MVP, Y-flipped for Vulkan
    bytes 64..79  vec4  eyeWorld   — .xyz = world-space camera origin

Descriptor set 0:
    binding 0  STORAGE_BUFFER  RibbonPoint  points[]
    binding 1  STORAGE_BUFFER  RibbonHeader headers[]

Two pipeline variants (alpha and additive) selected per ribbon
from header.flags.
===============
*/

void vk_init_ribbon( void )
{
	VkDescriptorSetLayoutBinding binds[3];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[2];
	VkDescriptorBufferInfo bufInfos[2];
	uint32_t pointsBytes;
	uint32_t headersBytes;
	int i;

	// Phase 5M: anti-drift checks on the ribbon SSBO byte-layout
	// constants. The point assert is real layout protection — host
	// `ribbonPoint_t` is the std430-mirror of GPU `RibbonPoint`, so a
	// drift in either side fires the assert. The header assert is a
	// documentation tautology (no host typedef to take sizeof of —
	// SSBO writes go through raw float/uint pointers); kept for parity
	// with BEAM_HEADER_BYTES's assert and to flag bumps to the constant.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( sizeof( ribbonPoint_t ) == RIBBON_POINT_BYTES,
		"ribbonPoint_t (host) and GPU RibbonPoint must agree on layout: "
		"vec3 pos + float width + vec4 rgba + vec3 normal + float pad = 48 B" );
	_Static_assert( RIBBON_HEADER_BYTES == 24,
		"RIBBON_HEADER_BYTES must be 24 bytes for std430 RibbonHeader "
		"(4 uint header + vec2 uvScroll = 24 B; struct alignment 8 B)" );
#endif

	memset( &vk.ribbon, 0, sizeof( vk.ribbon ) );

	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	// binding 2: per-shader-handle texture array. Populated host-side
	// from vk_primitive_shader_images[] by vk_init_primitive_shader_images
	// (called from R_Init AFTER tr.whiteImage exists; deferred because
	// vk_init_ribbon runs from InitOpenGL before R_InitImages). Fragment
	// stage only — only ribbon.frag samples it.
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
	binds[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.ribbon.set_layout ) );

	memset( &pushRange, 0, sizeof( pushRange ) );
	// VERTEX | FRAGMENT: the trailing vec4 frameParams stays in the push
	// range for layout invariance with the vertex stage. frameParams.x
	// was the legacy identityLight halving factor (dropped Phase 6B3'-a;
	// the named field removed in the Block 9 sweep) — ribbon.frag never
	// reads it, but the word stays so the push range matches across the
	// primitive shaders. frameParams.y carries currentTime.
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	                     | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset     = 0;
	pushRange.size       = 96; // mat4 mvp + vec4 eyeWorld + vec4 frameParams

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount         = 1;
	pipeLayoutInfo.pSetLayouts            = &vk.ribbon.set_layout;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges    = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.ribbon.pipeline_layout ) );

	pointsBytes  = RIBBON_POINTS_PER_FRAME  * RIBBON_POINT_BYTES;
	headersBytes = RIBBON_HEADERS_PER_FRAME * RIBBON_HEADER_BYTES;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		// points buffer (host-coherent, mapped)
		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = pointsBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.ribbon.points_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.ribbon.points_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.ribbon.points_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.ribbon.points_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.ribbon.points_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.ribbon.points_buffer[i], vk.ribbon.points_memory[i], 0 );
		vk_ral_register_buffer( vk.ribbon.points_buffer[i], pointsBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.ribbon.points" );

		// headers buffer (host-coherent, mapped)
		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = headersBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.ribbon.headers_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.ribbon.headers_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.ribbon.headers_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.ribbon.headers_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.ribbon.headers_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.ribbon.headers_buffer[i], vk.ribbon.headers_memory[i], 0 );
		vk_ral_register_buffer( vk.ribbon.headers_buffer[i], headersBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.ribbon.headers" );
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.ribbon.set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.ribbon.descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.ribbon.points_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = pointsBytes;
		bufInfos[1].buffer = vk.ribbon.headers_buffer[i];
		bufInfos[1].offset = 0;
		bufInfos[1].range  = headersBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.ribbon.descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];
		writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet          = vk.ribbon.descriptor[i];
		writes[1].dstBinding      = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo     = &bufInfos[1];

		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
	}

	// Two graphics pipeline variants (alpha and additive). Everything
	// matches the existing translucent pattern: depthTest=LESS_OR_EQUAL,
	// depthWrite=OFF, cull=NONE, blend enabled. Variant differs only in
	// the destination blend factor.
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		int variant;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.ribbon_vs;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.ribbon_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;            // two-sided
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.ribbon.pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		// 0 = alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA),
		// 1 = additive (SRC_ALPHA / ONE).
		for ( variant = 0; variant < 2; variant++ ) {
			memset( &blendAttach, 0, sizeof( blendAttach ) );
			blendAttach.blendEnable         = VK_TRUE;
			blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttach.dstColorBlendFactor = (variant == 0)
				? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
				: VK_BLEND_FACTOR_ONE;
			blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
			blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
			blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
				(variant == 0) ? &vk.ribbon.pipeline_alpha : &vk.ribbon.pipeline_additive ) );

			// Phase 7.4c-pipeline-followup-3 — ribbon (vk.ribbon.pipeline_layout).
			// Binding contract from vk_init_ribbon (vk.c:~4226) + ribbon.{vert,frag}:
			// push Push = mat4 mvp + 2 × vec4 = 96 bytes VERTEX|FRAGMENT (the
			// trailing vec4 frameParams stays in the push range for byte-layout
			// invariance between vs/fs even though ribbon.frag no longer reads it).
			// 1 BGL = vk.ribbon.set_layout (2 SSBOs + 1 sampler-array binding).
			// Two pipelines per session: variant 0 = SRC_ALPHA/ONE_MINUS_SRC_ALPHA,
			// variant 1 = SRC_ALPHA/ONE (additive). Topology = TRIANGLE_LIST.
			if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
				vk_ral_special_pipeline_params_t p;
				memset( &p, 0, sizeof( p ) );
				p.vs_module          = vk.modules.ribbon_vs;
				p.fs_module          = vk.modules.ribbon_fs;
				p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
				p.cullMode           = RAL_CULL_NONE;
				p.depthTestEnable    = qtrue;
				p.depthWriteEnable   = qfalse;
				p.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
				p.blendEnable        = qtrue;
				p.srcColor           = RAL_BLEND_SRC_ALPHA;
				p.dstColor           = ( variant == 0 ) ? RAL_BLEND_ONE_MINUS_SRC_ALPHA : RAL_BLEND_ONE;
				p.blendOp            = RAL_BLEND_OP_ADD;
				p.pushConstantSize   = 96;
				p.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
				if ( vk.ribbon.ral_bgl ) { p.bgls[ p.numBgls++ ] = vk.ribbon.ral_bgl; }
				p.debugName          = ( variant == 0 ) ? "ral-ribbon-alpha" : "ral-ribbon-additive";
				p.externalLayout     = vk.ribbon.ral_pipeline_layout;   // Phase 7.4c-submit-A3 — share renderer's VkPipelineLayout.
				p.externalRenderPass = vk.ral_render_pass.main;          // ribbon draws inside the main pass.
				if ( variant == 0 ) vk.ribbon.ral_pipeline_alpha    = vk_ral_create_special_pipeline( &p );
				else                vk.ribbon.ral_pipeline_additive = vk_ral_create_special_pipeline( &p );
			}
		}
	}

	vk.ribbon.available = qtrue;
	ri.Log( SEV_INFO, "Vulkan ribbon primitive pipeline initialized (%u points / %u headers per frame)\n",
		(unsigned)RIBBON_POINTS_PER_FRAME, (unsigned)RIBBON_HEADERS_PER_FRAME );
}


void vk_shutdown_ribbon( void )
{
	int i;

	if ( vk.ribbon.pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ribbon.pipeline_alpha, NULL );
		vk.ribbon.pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ribbon.pipeline_additive, NULL );
		vk.ribbon.pipeline_additive = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.ral_pipeline_alpha    ) { Ral_DestroyPipeline( vk.ribbon.ral_pipeline_alpha    ); vk.ribbon.ral_pipeline_alpha    = NULL; }
	if ( vk.ribbon.ral_pipeline_additive ) { Ral_DestroyPipeline( vk.ribbon.ral_pipeline_additive ); vk.ribbon.ral_pipeline_additive = NULL; }

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.ribbon.points_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.ribbon.points_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.ribbon.points_buffer[i], NULL );
			vk.ribbon.points_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.ribbon.points_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.ribbon.points_memory[i], NULL );
			vk.ribbon.points_memory[i] = VK_NULL_HANDLE;
			vk.ribbon.points_ptr[i] = NULL;
		}
		if ( vk.ribbon.headers_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.ribbon.headers_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.ribbon.headers_buffer[i], NULL );
			vk.ribbon.headers_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.ribbon.headers_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.ribbon.headers_memory[i], NULL );
			vk.ribbon.headers_memory[i] = VK_NULL_HANDLE;
			vk.ribbon.headers_ptr[i] = NULL;
		}
	}

	if ( vk.ribbon.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.ribbon.pipeline_layout, NULL );
		vk.ribbon.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.ral_bgl ) { Ral_DestroyBindGroupLayout( vk.ribbon.ral_bgl ); vk.ribbon.ral_bgl = NULL; }
	if ( vk.ribbon.set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.ribbon.set_layout, NULL );
		vk.ribbon.set_layout = VK_NULL_HANDLE;
	}

	vk.ribbon.available = qfalse;
}


void RB_DrawRibbons( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	VkPipeline lastPipeline;
	float pushBuf[24]; // mat4 mvp + vec4 eyeWorld + vec4 frameParams
	float mvp[16];
	const float *p;
	float proj[16];
	uint32_t i;

	// Pipelines were created against vk.render_pass.main only — skip if
	// we're being called inside the screenmap pass.
	if ( !vk.ribbon.available
	  || vk.ribbon.numHeadersThisFrame == 0
	  || vk.renderPassIndex != RENDER_PASS_MAIN )
		return;

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// World MVP — same construction the standard 3D path uses
	// (get_mvp_transform): copy the projection, flip column 1
	// for Vulkan clip-space, then myGlMultMatrix( modelView, proj ).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	memcpy( pushBuf,      mvp,                          64 );
	memcpy( pushBuf + 16, backEnd.viewParms.or.origin,  sizeof( vec3_t ) );
	pushBuf[19] = 0.0f;
	// frameParams: .x = reserved/unused — was the legacy identityLight
	// halving factor (dropped Phase 6B3'-a; field removed in the Block 9
	// sweep). The vec4 word stays for push-range byte-compat with the
	// other primitive shaders; ribbon.frag never reads it. .y =
	// currentTime (consumed by ribbon.vert to drive uvScroll age); .zw
	// reserved.
	pushBuf[20] = 0.0f;
	pushBuf[21] = (float)backEnd.refdef.floatTime;
	pushBuf[22] = 0.0f;
	pushBuf[23] = 0.0f;

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
		// Phase 7.4c-cmd — parallel-paths cmd-record (viewport/scissor).
		Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);
		Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.ribbon.pipeline_layout, 0, 1, &vk.ribbon.descriptor[frameIdx], 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record.
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.ribbon.pipeline_layout, 0, 1, &vk.ribbon.descriptor[frameIdx], 0, NULL );

	qvkCmdPushConstants( cmd, vk.ribbon.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pushBuf ), pushBuf );
	// Phase 7.4c-cmd — parallel-paths push-constants.
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.ribbon.pipeline_layout),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pushBuf ), pushBuf );

	lastPipeline = VK_NULL_HANDLE;
	for ( i = 0; i < vk.ribbon.numHeadersThisFrame; i++ ) {
		const uint32_t *hdr = (const uint32_t *)
			(vk.ribbon.headers_ptr[frameIdx] + i * RIBBON_HEADER_BYTES);
		uint32_t pointCount = hdr[1];
		uint32_t flags      = hdr[3];
		VkPipeline pipe = (flags & PRIM_FLAG_ADDITIVE)
			? vk.ribbon.pipeline_additive : vk.ribbon.pipeline_alpha;

		if ( pointCount < 2 )
			continue;

		if ( pipe != lastPipeline ) {
			qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe );
			// Phase 7.4c-cmd — parallel-paths bind-pipeline.
			Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(pipe ));
			lastPipeline = pipe;
		}

		// 6 verts per segment × (pointCount - 1) segments. firstInstance = i
		// so the vertex shader sees this header at gl_InstanceIndex.
		qvkCmdDraw( cmd, ( pointCount - 1 ) * 6, 1, 0, i );
		// Phase 7.4c-cmd — parallel-paths draw.
		Ral_CmdDraw( vk.cmd->ral_cmd, ( pointCount - 1 ) * 6, 1, 0, i );
	}

	// Invalidate cached pipeline / descriptor / depth-range so the next
	// standard draw rebinds correctly. Same cleanup pattern the IQM
	// draw uses.
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


/*
===============
Primitive beam — self-contained pipeline.

Cgame submits two-endpoint camera-facing quads via RE_AddBeamToScene.
The renderer maintains a small mixed-mode pool (BEAM_POOL_MAX = 128
slots) holding both transient (one-frame) and persistent (lifetime
+ fade) beams. Each frame, RB_DrawBeams walks the pool, resolves
entity-attached endpoints into world space, computes fade alpha for
persistent beams, writes a compacted run of beamHeaderGPU_t entries
to the per-frame SSBO, and issues a single vkCmdDraw with
vertexCount = 6 * BEAM_AXIAL_MAX, instanceCount = drawCount.

The vertex shader (beam.vert) expands each instance into
axialCopies × 6 vertices, emitting clip-space-behind degenerate
output for axial copies above the beam's `axialCopies` field
(GPU rasterizer discards). axialCopies > 1 produces a "cross"
pattern of multiple camera-facing quads rotated around the beam
axis at equal angular intervals.

Push range layout (96 bytes):
    bytes  0..63   mat4  mvp           — world MVP, Y-flipped for Vulkan
    bytes 64..79   vec4  eyeWorld      — .xyz = camera world origin (axis-facing math)
    bytes 80..95   vec4  frameParams   — reserved (matches ribbon layout)

Descriptor set 0:
    binding 0  STORAGE_BUFFER          BeamHeader headers[]
    binding 1  COMBINED_IMAGE_SAMPLER  shaderImages[PRIMITIVE_SHADER_IMAGE_MAX]

Lifetime semantics:
    desc.duration == 0  → transient. Drawn this frame; slot freed
                          at the end of RB_DrawBeams. Cgame must
                          re-submit each frame to keep visible.
    desc.duration  > 0  → persistent. spawnTime captured at
                          RE_AddBeamToScene. Engine fades in/out
                          and frees the slot when age >= duration.

Entity attachment is translate-only: cachedStart =
tr.refdef.entities[startEntityNum].e.origin + startOffset.
Rotation is NOT applied (kept simple for the LG-style migration
sites that pass world coords directly with startEntityNum = -1).
===============
*/

void vk_init_beam( void )
{
	VkDescriptorSetLayoutBinding binds[4];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[3];
	VkDescriptorBufferInfo bufInfos[3];
	uint32_t headerBytes;
	int i;

	memset( &vk.beam, 0, sizeof( vk.beam ) );

	// Sanity check on the GPU header layout. The layout is described
	// in vk.h's BEAM_HEADER_BYTES comment and must match beam.vert's
	// BeamHeader struct.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( BEAM_HEADER_BYTES == 96,
		"BEAM_HEADER_BYTES must be 96 bytes for std430 BeamHeader "
		"(4 vec4 = 64 B (start, end, startColor, endColor); "
		"+ vec2 uvScroll + 2 float widths + float spawnTime + 3 uint "
		"= 32 B trailer; total 96 B, naturally 16-aligned)" );
	// Phase 5K: VK_PRIM_QHANDLE_MAX (parallel constant in vk.h, lives
	// outside the tr_local.h include order) must match MAX_SHADERS.
	_Static_assert( VK_PRIM_QHANDLE_MAX == MAX_SHADERS,
		"VK_PRIM_QHANDLE_MAX must equal MAX_SHADERS — qhandle→primitive-slot "
		"indirection table is sized by VK_PRIM_QHANDLE_MAX in vk.h but "
		"MAX_SHADERS is the actual qhandle bound" );
#endif

	// ── Descriptor set layout ────────────────────────────────────
	// Phase 5G: 4 bindings.
	//   0: header SSBO (per-instance beam pool, vertex stage)
	//   1: image array (sampler array, fragment stage)
	//   2: per-stage SSBO (multi-stage shader data, vertex+fragment)
	//   3: stage counts SSBO (PRIMITIVE_SHADER_IMAGE_MAX uints,
	//      vertex stage — for cheap per-stage cull)
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
	binds[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[3].binding         = 3;
	binds[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.beam.set_layout ) );

	memset( &pushRange, 0, sizeof( pushRange ) );
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset     = 0;
	// Phase 5G: 112 B = mat4 mvp (64) + vec4 eyeWorld (16) + vec4
	// frameParams (16) + vec4 stageParams (16). stageParams.x carries
	// the per-draw stageIdx (cast from float in the shader).
	pushRange.size       = 112;

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount         = 1;
	pipeLayoutInfo.pSetLayouts            = &vk.beam.set_layout;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges    = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.beam.pipeline_layout ) );

	// ── Per-frame header SSBOs (host-coherent, mapped) ──────────
	headerBytes = BEAM_POOL_MAX * BEAM_HEADER_BYTES;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = headerBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.beam.header_buffer[i] ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.beam.header_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.beam.header_memory[i] ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.beam.header_buffer[i], vk.beam.header_memory[i], 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.beam.header_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.beam.header_ptr[i] ) );
		vk_ral_register_buffer( vk.beam.header_buffer[i], headerBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.beam.header" );
	}

	// ── Allocate descriptor sets and write bindings 0, 2, 3 ─────
	// Binding 1 (sampler array) is left unwritten here; it's
	// populated by vk_init_primitive_shader_images from R_Init,
	// AFTER tr.whiteImage exists and vk_init_particle has created
	// vk.particle.sampler.
	//
	// Bindings 2 and 3 (per-stage data, stage counts) come from
	// vk.primitive_stages_buffer / vk.primitive_stage_counts_buffer
	// allocated by vk_init_primitive_shader_stages, which is called
	// from vk_init_primitive_shader_images. The buffers may not
	// exist yet at vk_init_beam time (vk_init_beam runs before
	// R_Init); skip the writes here and re-emit at vid_restart re-
	// alloc once the buffers exist (vk_init_descriptors handles it).
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t writeCount;

		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.beam.set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.beam.descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.beam.header_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = headerBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.beam.descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];

		writeCount = 1;

		if ( vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
			bufInfos[1].buffer = vk.primitive_stages_buffer;
			bufInfos[1].offset = 0;
			bufInfos[1].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
				* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;

			writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet          = vk.beam.descriptor[i];
			writes[1].dstBinding      = 2;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pBufferInfo     = &bufInfos[1];

			bufInfos[2].buffer = vk.primitive_stage_counts_buffer;
			bufInfos[2].offset = 0;
			bufInfos[2].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
				* sizeof( uint32_t );

			writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet          = vk.beam.descriptor[i];
			writes[2].dstBinding      = 3;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2].pBufferInfo     = &bufInfos[2];

			writeCount = 3;
		}

		qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
	}

	// ── Phase 5J: dedicated REPEAT-mode sampler for binding 1 ───
	// Beam UV scrolling can produce arbitrarily large out-of-range
	// UVs over a long match (-1.8 UV/sec × hundreds of seconds).
	// CLAMP_TO_EDGE saturates and kills animation; REPEAT wraps
	// natively, restoring the legacy CPU pipeline's texture-tiling
	// behaviour. Beam-only — ribbon/sprite/particle keep
	// vk.particle.sampler (CLAMP_TO_EDGE).
	{
		VkSamplerCreateInfo samplerInfo;
		memset( &samplerInfo, 0, sizeof( samplerInfo ) );
		samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter     = VK_FILTER_LINEAR;
		samplerInfo.minFilter     = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerInfo.minLod        = 0.0f;
		samplerInfo.maxLod        = VK_LOD_CLAMP_NONE;
		VK_CHECK( qvkCreateSampler( vk.device, &samplerInfo, NULL,
			&vk.beam.sampler_repeat ) );
		SET_OBJECT_NAME( vk.beam.sampler_repeat,
			"sampler - beam REPEAT",
			VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );
	}

	// ── Graphics pipeline ───────────────────────────────────────
	{
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineViewportStateCreateInfo viewportState;
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.beam_vs;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.beam_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		// No vertex attributes; vertex shader pulls from the SSBO
		// via gl_VertexIndex / gl_InstanceIndex (same pattern as
		// ribbon and particle).

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;            // two-sided
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable         = VK_TRUE;
		// Phase 5G: ONE/ONE (pure additive) — matches `blendfunc add`
		// in q3 shader.script which every beam-consuming shader
		// (lightningBolt, lightningArc) uses. Source contribution is
		// `texel * fragColor.rgb`; src.alpha is ignored under ONE/ONE
		// (different from the prior SRC_ALPHA/ONE pipeline which
		// modulated the contribution by vertex alpha).
		blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.beam.pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL, &vk.beam.pipeline ) );

		// Phase 7.4c-pipeline-followup-3 — beam (vk.beam.pipeline_layout).
		// Binding contract from vk_init_beam (vk.c:~4717) + beam.{vert,frag}:
		// push Push = mat4 mvp + 3 × vec4 = 112 bytes VERTEX|FRAGMENT.
		// 1 BGL = vk.beam.set_layout (4 bindings: headers SSBO + sampler
		// array + per-stage SSBO + counts SSBO).
		// Topology = TRIANGLE_LIST — beam.vert emits 6 vertices per axial
		// copy expanding to two triangles (a camera-facing quad).
		if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
			vk_ral_special_pipeline_params_t p;
			memset( &p, 0, sizeof( p ) );
			p.vs_module          = vk.modules.beam_vs;
			p.fs_module          = vk.modules.beam_fs;
			p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
			p.cullMode           = RAL_CULL_NONE;
			p.depthTestEnable    = qtrue;
			p.depthWriteEnable   = qfalse;
			p.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
			p.blendEnable        = qtrue;
			p.srcColor           = RAL_BLEND_ONE;
			p.dstColor           = RAL_BLEND_ONE;
			p.blendOp            = RAL_BLEND_OP_ADD;
			p.pushConstantSize   = 112;
			p.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
			if ( vk.beam.ral_bgl ) { p.bgls[ p.numBgls++ ] = vk.beam.ral_bgl; }
			p.debugName          = "ral-beam";
			p.externalLayout     = vk.beam.ral_pipeline_layout;     // Phase 7.4c-submit-A3
			p.externalRenderPass = vk.ral_render_pass.main;          // beam draws inside main pass
			vk.beam.ral_pipeline = vk_ral_create_special_pipeline( &p );
		}
	}

	vk.beam.available = qtrue;
	ri.Log( SEV_INFO, "Vulkan beam primitive pipeline initialized (%u pool slots, %u axial copies max)\n",
		(unsigned)BEAM_POOL_MAX, (unsigned)BEAM_AXIAL_MAX );
}


void vk_shutdown_beam( void )
{
	int i;

	if ( vk.beam.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.beam.pipeline, NULL );
		vk.beam.pipeline = VK_NULL_HANDLE;
	}
	// Phase 7.4c-pipeline-followup-2 — sibling RAL pipeline teardown.
	if ( vk.beam.ral_pipeline ) { Ral_DestroyPipeline( vk.beam.ral_pipeline ); vk.beam.ral_pipeline = NULL; }

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.beam.header_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.beam.header_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.beam.header_buffer[i], NULL );
			vk.beam.header_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.beam.header_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.beam.header_memory[i], NULL );
			vk.beam.header_memory[i] = VK_NULL_HANDLE;
			vk.beam.header_ptr[i] = NULL;
		}
	}

	if ( vk.beam.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.beam.pipeline_layout, NULL );
		vk.beam.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.beam.set_layout != VK_NULL_HANDLE ) {
		if ( vk.beam.ral_bgl ) { Ral_DestroyBindGroupLayout( vk.beam.ral_bgl ); vk.beam.ral_bgl = NULL; }
		qvkDestroyDescriptorSetLayout( vk.device, vk.beam.set_layout, NULL );
		vk.beam.set_layout = VK_NULL_HANDLE;
	}
	if ( vk.beam.sampler_repeat != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, vk.beam.sampler_repeat, NULL );
		vk.beam.sampler_repeat = VK_NULL_HANDLE;
	}

	vk.beam.available = qfalse;
}


void RB_DrawBeams( void )
{
	VkCommandBuffer cmd;
	int             frameIdx;
	// Phase 5G: 28 floats = 112 B push range = mat4 mvp + vec4 eyeWorld
	// + vec4 frameParams + vec4 stageParams (.x = stageIdx).
	float           pushBuf[28];
	float           mvp[16];
	const float    *p;
	float           proj[16];
	float           currentTime;
	uint32_t        i;
	uint32_t        stageIdx;
	byte           *headerBase;

	if ( !vk.beam.available
	  || vk.renderPassIndex != RENDER_PASS_MAIN ) {
		// Even if we skip the draw this frame, transient slots from
		// prior frames should still expire so they don't leak. Walk
		// the pool and free any transient slots; persistent slots
		// keep their state for the next viable frame.
		for ( i = 0; i < BEAM_POOL_MAX; i++ ) {
			if ( vk.beam.active[i] && vk.beam.duration[i] == 0.0f ) {
				vk.beam.active[i] = qfalse;
			}
		}
		vk.beam.drawCount = 0;
		return;
	}

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;
	currentTime = (float)backEnd.refdef.floatTime;

	headerBase = vk.beam.header_ptr[frameIdx];
	vk.beam.drawCount = 0;

	// ── Pool walk: resolve, fade, write SSBO, and free expired slots
	for ( i = 0; i < BEAM_POOL_MAX; i++ ) {
		const beamDesc_t *desc;
		float    age;
		float    fadeAlpha;
		vec3_t   resolvedStart;
		vec3_t   resolvedEnd;
		uint32_t copies;
		float   *fdst;
		uint32_t *udst;
		byte    *slot;

		if ( !vk.beam.active[i] ) continue;

		desc = &vk.beam.desc[i];

		// Lifetime check.
		if ( vk.beam.duration[i] > 0.0f ) {
			age = currentTime - vk.beam.spawnTime[i];
			if ( age >= vk.beam.duration[i] ) {
				// Expired persistent beam — free the slot, skip drawing.
				vk.beam.active[i] = qfalse;
				continue;
			}

			// Compute fade alpha. fadeIn / fadeOut clamp to non-negative.
			fadeAlpha = 1.0f;
			if ( vk.beam.fadeIn[i] > 0.0f && age < vk.beam.fadeIn[i] ) {
				fadeAlpha = age / vk.beam.fadeIn[i];
			}
			if ( vk.beam.fadeOut[i] > 0.0f && age > vk.beam.duration[i] - vk.beam.fadeOut[i] ) {
				float fadeOutAge = age - ( vk.beam.duration[i] - vk.beam.fadeOut[i] );
				float k = 1.0f - fadeOutAge / vk.beam.fadeOut[i];
				if ( k < 0.0f ) k = 0.0f;
				if ( k < fadeAlpha ) fadeAlpha = k;
			}
		} else {
			// Transient: full opacity, single-frame.
			fadeAlpha = 1.0f;
		}

		// ── Resolve world-space start/end (entity attachment is
		// translate-only; entity rotation is intentionally not
		// applied — see header doc-comment).
		if ( desc->startEntityNum >= 0 && desc->startEntityNum < tr.refdef.num_entities ) {
			const trRefEntity_t *ent = &tr.refdef.entities[desc->startEntityNum];
			VectorAdd( ent->e.origin, desc->startOffset, resolvedStart );
		} else {
			VectorCopy( desc->start, resolvedStart );
		}
		if ( desc->endEntityNum >= 0 && desc->endEntityNum < tr.refdef.num_entities ) {
			const trRefEntity_t *ent = &tr.refdef.entities[desc->endEntityNum];
			VectorAdd( ent->e.origin, desc->endOffset, resolvedEnd );
		} else {
			VectorCopy( desc->end, resolvedEnd );
		}

		// Clamp axialCopies (defensive — RE_AddBeamToScene already
		// clamps, but a corrupt slot or future caller wouldn't have).
		copies = (uint32_t)desc->axialCopies;
		if ( copies < 1 )              copies = 1;
		if ( copies > BEAM_AXIAL_MAX ) copies = BEAM_AXIAL_MAX;

		// ── Write the GPU header at SSBO slot vk.beam.drawCount.
		// std430 layout (96 B):
		//   bytes  0..15  vec4  start         (.xyz, .w pad)
		//   bytes 16..31  vec4  end           (.xyz, .w pad)
		//   bytes 32..47  vec4  startColor    (alpha pre-multiplied with fade)
		//   bytes 48..63  vec4  endColor      (alpha pre-multiplied with fade)
		//   bytes 64..71  vec2  uvScroll      (8-byte aligned at offset 64)
		//   bytes 72..75  float startWidth
		//   bytes 76..79  float endWidth
		//   bytes 80..83  float spawnTime
		//   bytes 84..87  uint  shaderHandle
		//   bytes 88..91  uint  axialCopies
		//   bytes 92..95  uint  flags
		slot = headerBase + vk.beam.drawCount * BEAM_HEADER_BYTES;
		fdst = (float    *)slot;
		udst = (uint32_t *)slot;

		fdst[0] = resolvedStart[0];
		fdst[1] = resolvedStart[1];
		fdst[2] = resolvedStart[2];
		fdst[3] = 0.0f; // pad

		fdst[4] = resolvedEnd[0];
		fdst[5] = resolvedEnd[1];
		fdst[6] = resolvedEnd[2];
		fdst[7] = 0.0f; // pad

		// Premultiply fade into BOTH endpoints' alpha channels so the
		// linear-interpolated per-fragment alpha fades uniformly. RGB
		// stays as-is; the fragment shader does texel * fragColor and
		// the alpha modulation propagates correctly through the
		// additive blend.
		fdst[8]  = desc->startColor[0];
		fdst[9]  = desc->startColor[1];
		fdst[10] = desc->startColor[2];
		fdst[11] = desc->startColor[3] * fadeAlpha;

		fdst[12] = desc->endColor[0];
		fdst[13] = desc->endColor[1];
		fdst[14] = desc->endColor[2];
		fdst[15] = desc->endColor[3] * fadeAlpha;

		// Animation trailer (offsets 64..95). vk.beam.spawnTime[i]
		// was captured at RE_AddBeamToScene time; transient beams
		// (PRIM_FLAG_TRANSIENT in flags) ignore it shader-side and
		// use frameParams.y directly, persistent beams use
		// (frameParams.y - spawnTime).
		fdst[16] = desc->uvScroll[0];
		fdst[17] = desc->uvScroll[1];
		fdst[18] = desc->startWidth;
		fdst[19] = desc->endWidth;
		fdst[20] = vk.beam.spawnTime[i];
		// Phase 5K: translate cgame qhandle → primitive registry slot.
		// Out-of-range / unregistered qhandles map to slot 0 (whiteImage)
		// rather than producing OOB SSBO reads on the GPU.
		udst[21] = vk_qhandle_to_prim_slot( desc->shader );
		udst[22] = copies;
		udst[23] = (uint32_t)desc->flags;

		vk.beam.drawCount++;

		// Transient slots free immediately after the SSBO write so
		// the next frame requires re-submission.
		if ( vk.beam.duration[i] == 0.0f ) {
			vk.beam.active[i] = qfalse;
		}
	}

	if ( vk.beam.drawCount == 0 ) return;

	// ── World MVP push (same construction as ribbon/sprite).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	memcpy( pushBuf,      mvp,                          64 );
	memcpy( pushBuf + 16, backEnd.viewParms.or.origin,  sizeof( vec3_t ) );
	pushBuf[19] = 0.0f;
	// frameParams: .x = reserved/unused — was the legacy identityLight
	// halving factor (dropped Phase 6B3'-a; field removed in the Block 9
	// sweep). The vec4 word stays for push-range byte-compat with the
	// other primitive shaders; beam.frag never reads it. .y = currentTime
	// (consumed by beam.vert to drive uvScroll age), .zw reserved.
	pushBuf[20] = 0.0f;
	pushBuf[21] = (float)backEnd.refdef.floatTime;
	pushBuf[22] = 0.0f;
	pushBuf[23] = 0.0f;
	// stageParams (.x = stageIdx) — set per dispatch in the loop below.
	pushBuf[24] = 0.0f;
	pushBuf[25] = 0.0f;
	pushBuf[26] = 0.0f;
	pushBuf[27] = 0.0f;

	// Phase 5G: refresh the stage-counts SSBO from the host-side
	// mirror. Cheap (256 B) and decouples shader-registration timing
	// from the descriptor write — the beam pipeline always reads the
	// current count array regardless of when shaders were registered.
	if ( vk.primitive_stage_counts_mapped != NULL ) {
		uint32_t *countDst = (uint32_t *)vk.primitive_stage_counts_mapped;
		for ( i = 0; i < PRIMITIVE_SHADER_IMAGE_MAX; i++ ) {
			countDst[i] = (uint32_t)vk.primitive_shader_stage_counts[i];
		}
	}

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
		// Phase 7.4c-cmd — parallel-paths cmd-record (beam viewport/scissor).
		Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);
		Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);
	}

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.beam.pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline.
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.beam.pipeline ));
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.beam.pipeline_layout, 0, 1, &vk.beam.descriptor[frameIdx], 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record.
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.beam.pipeline_layout, 0, 1, &vk.beam.descriptor[frameIdx], 0, NULL );

	// Phase 5G: outer loop over stage index. Each iteration is one
	// instanced draw covering all beams in the pool; the vertex
	// shader culls beams whose registered stageCount <= stageIdx
	// (their fragments never rasterize). For homogeneous-shader
	// frames (e.g. only LG primary, stageCount=2) we issue 2 real
	// dispatches and 2 culled-empty dispatches; the empty ones
	// cost only a vertex-shader degenerate write per vertex.
	//
	// All stages share the single ONE/ONE additive pipeline (the
	// only blend used by every primitive shader in the current
	// asset base — `blendfunc add`). When mixed-blend primitive
	// shaders appear, extend with a per-stage pipeline cache.
	for ( stageIdx = 0; stageIdx < PRIMITIVE_STAGE_MAX; stageIdx++ ) {
		pushBuf[24] = (float)stageIdx;
		qvkCmdPushConstants( cmd, vk.beam.pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof( pushBuf ), pushBuf );
		// Phase 7.4c-cmd — parallel-paths push-constants (beam).
		Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.beam.pipeline_layout),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof( pushBuf ), pushBuf );

		// 6 verts × BEAM_AXIAL_MAX slots per instance × drawCount
		// instances. Vertex shader gates axialCopies > limit AND
		// stageIdx >= stageCount to degenerate output.
		qvkCmdDraw( cmd, 6 * BEAM_AXIAL_MAX, vk.beam.drawCount, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths draw (beam).
		Ral_CmdDraw( vk.cmd->ral_cmd, 6 * BEAM_AXIAL_MAX, vk.beam.drawCount, 0, 0 );
	}

	// Invalidate cached binding state (same cleanup ribbon does).
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


/*
===============
Primitive sprite — self-contained pipeline.

Cgame submits world-space billboard sprites via RE_AddSpriteToScene.
Each call appends one GPU SpriteHeader to the per-frame headers
SSBO. RB_DrawSprites (called from RB_DrawSurfs after RB_DrawRibbons)
sorts the headers into two blend groups (alpha and additive) and
issues at most one direct vkCmdDraw per group, with vertexCount=6
and instanceCount=N, so the vertex shader can read its header via
gl_InstanceIndex.

Push range layout (vertex stage only, 96 bytes):
    bytes  0..63   mat4  mvp        — world MVP, Y-flipped for Vulkan
    bytes 64..79   vec4  viewLeft   — .xyz = camera-left in world
    bytes 80..95   vec4  viewUp     — .xyz = camera-up   in world

Descriptor set 0:
    binding 0  STORAGE_BUFFER  SpriteHeader sprites[]

Two pipeline variants (alpha and additive) selected per submission
from header.flags.
===============
*/

void vk_init_sprite( void )
{
	VkDescriptorSetLayoutBinding binds[1];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[1];
	VkDescriptorBufferInfo bufInfos[1];
	uint32_t headersBytes;
	int i;

	memset( &vk.sprite, 0, sizeof( vk.sprite ) );

	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.sprite.set_layout ) );

	memset( &pushRange, 0, sizeof( pushRange ) );
	// VERTEX | FRAGMENT: trailing vec4 frameParams retained for vertex-
	// stage layout invariance. frameParams.x was the legacy identityLight
	// halving factor (dropped Phase 6B3'-a; the named field removed in
	// the Block 9 sweep) — sprite.frag never reads it, but the word stays
	// so the push range matches across the primitive shaders.
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	                     | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset     = 0;
	pushRange.size       = 112; // mat4 mvp + vec4 viewLeft + vec4 viewUp + vec4 frameParams

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount         = 1;
	pipeLayoutInfo.pSetLayouts            = &vk.sprite.set_layout;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges    = &pushRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.sprite.pipeline_layout ) );

	headersBytes = SPRITES_PER_FRAME * SPRITE_HEADER_BYTES;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		// headers buffer (host-coherent, mapped)
		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = headersBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.sprite.headers_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.sprite.headers_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.sprite.headers_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.sprite.headers_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.sprite.headers_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.sprite.headers_buffer[i], vk.sprite.headers_memory[i], 0 );
		vk_ral_register_buffer( vk.sprite.headers_buffer[i], headersBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.sprite.headers" );
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.sprite.set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.sprite.descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.sprite.headers_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = headersBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.sprite.descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];

		qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );
	}

	// Two graphics pipeline variants (alpha and additive). Same state
	// as the ribbon pipeline (depthTest enabled, depthWrite off, cull
	// none, blend enabled). Only the destination blend factor differs.
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		int variant;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.sprite_vs;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.sprite_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;            // two-sided
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.sprite.pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		// 0 = alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA),
		// 1 = additive (SRC_ALPHA / ONE).
		for ( variant = 0; variant < 2; variant++ ) {
			memset( &blendAttach, 0, sizeof( blendAttach ) );
			blendAttach.blendEnable         = VK_TRUE;
			blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttach.dstColorBlendFactor = (variant == 0)
				? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
				: VK_BLEND_FACTOR_ONE;
			blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
			blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
			blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
				(variant == 0) ? &vk.sprite.pipeline_alpha : &vk.sprite.pipeline_additive ) );

			// Phase 7.4c-pipeline-followup-3 — sprite (vk.sprite.pipeline_layout).
			// Binding contract from vk_init_sprite (vk.c:~5378) + sprite.{vert,frag}:
			// push Push = mat4 mvp + 3 × vec4 = 112 bytes VERTEX|FRAGMENT.
			// 1 BGL = vk.sprite.set_layout (1 SSBO: headers).
			// Two pipelines per session: variant 0 = SRC_ALPHA/ONE_MINUS_SRC_ALPHA,
			// variant 1 = SRC_ALPHA/ONE (additive). Topology = TRIANGLE_LIST
			// — sprite.vert emits 6 vertices per quad (two triangles).
			if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
				vk_ral_special_pipeline_params_t p;
				memset( &p, 0, sizeof( p ) );
				p.vs_module          = vk.modules.sprite_vs;
				p.fs_module          = vk.modules.sprite_fs;
				p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
				p.cullMode           = RAL_CULL_NONE;
				p.depthTestEnable    = qtrue;
				p.depthWriteEnable   = qfalse;
				p.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
				p.blendEnable        = qtrue;
				p.srcColor           = RAL_BLEND_SRC_ALPHA;
				p.dstColor           = ( variant == 0 ) ? RAL_BLEND_ONE_MINUS_SRC_ALPHA : RAL_BLEND_ONE;
				p.blendOp            = RAL_BLEND_OP_ADD;
				p.pushConstantSize   = 112;
				p.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
				if ( vk.sprite.ral_bgl ) { p.bgls[ p.numBgls++ ] = vk.sprite.ral_bgl; }
				p.debugName          = ( variant == 0 ) ? "ral-sprite-alpha" : "ral-sprite-additive";
				p.externalLayout     = vk.sprite.ral_pipeline_layout;   // Phase 7.4c-submit-A3
				p.externalRenderPass = vk.ral_render_pass.main;
				if ( variant == 0 ) vk.sprite.ral_pipeline_alpha    = vk_ral_create_special_pipeline( &p );
				else                vk.sprite.ral_pipeline_additive = vk_ral_create_special_pipeline( &p );
			}
		}
	}

	vk.sprite.available = qtrue;
	ri.Log( SEV_INFO, "Vulkan sprite primitive pipeline initialized (%u headers per frame)\n",
		(unsigned)SPRITES_PER_FRAME );
}


void vk_shutdown_sprite( void )
{
	int i;

	if ( vk.sprite.pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.sprite.pipeline_alpha, NULL );
		vk.sprite.pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.sprite.pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.sprite.pipeline_additive, NULL );
		vk.sprite.pipeline_additive = VK_NULL_HANDLE;
	}
	// Phase 7.4c-pipeline-followup-3 — sibling RAL pipeline teardown.
	if ( vk.sprite.ral_pipeline_alpha    ) { Ral_DestroyPipeline( vk.sprite.ral_pipeline_alpha    ); vk.sprite.ral_pipeline_alpha    = NULL; }
	if ( vk.sprite.ral_pipeline_additive ) { Ral_DestroyPipeline( vk.sprite.ral_pipeline_additive ); vk.sprite.ral_pipeline_additive = NULL; }

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.sprite.headers_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.sprite.headers_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.sprite.headers_buffer[i], NULL );
			vk.sprite.headers_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.sprite.headers_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.sprite.headers_memory[i], NULL );
			vk.sprite.headers_memory[i] = VK_NULL_HANDLE;
			vk.sprite.headers_ptr[i] = NULL;
		}
	}

	if ( vk.sprite.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.sprite.pipeline_layout, NULL );
		vk.sprite.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.sprite.set_layout != VK_NULL_HANDLE ) {
		if ( vk.sprite.ral_bgl ) { Ral_DestroyBindGroupLayout( vk.sprite.ral_bgl ); vk.sprite.ral_bgl = NULL; }
		qvkDestroyDescriptorSetLayout( vk.device, vk.sprite.set_layout, NULL );
		vk.sprite.set_layout = VK_NULL_HANDLE;
	}

	vk.sprite.available = qfalse;
}


void RB_DrawSprites( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	float pushBuf[28]; // mat4 mvp + vec4 viewLeft + vec4 viewUp + vec4 frameParams
	float mvp[16];
	const float *p;
	float proj[16];
	uint32_t firstAdditive, additiveCount, alphaCount;

	// Pipelines were created against vk.render_pass.main only — skip if
	// we're being called inside the screenmap pass.
	if ( !vk.sprite.available
	  || vk.sprite.numThisFrame == 0
	  || vk.renderPassIndex != RENDER_PASS_MAIN )
		return;

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// World MVP — same construction the standard 3D path uses
	// (get_mvp_transform): copy the projection, flip column 1
	// for Vulkan clip-space, then myGlMultMatrix( modelView, proj ).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	memcpy( pushBuf,      mvp,                          64 );
	memcpy( pushBuf + 16, backEnd.viewParms.or.axis[1], sizeof( vec3_t ) );
	pushBuf[19] = 0.0f;
	memcpy( pushBuf + 20, backEnd.viewParms.or.axis[2], sizeof( vec3_t ) );
	pushBuf[23] = 0.0f;
	// frameParams: .x = reserved/unused — was the legacy identityLight
	// halving factor (dropped Phase 6B3'-a; field removed in the Block 9
	// sweep). The vec4 word stays for push-range byte-compat with the
	// other primitive shaders; sprite.frag never reads it. .y =
	// currentTime (sprite shader doesn't currently consume frameParams.y,
	// but the word is populated for symmetry with ribbon/beam — a future
	// textured sprite consumer with UV scroll could opt in without
	// further host-side changes); .zw reserved.
	pushBuf[24] = 0.0f;
	pushBuf[25] = (float)backEnd.refdef.floatTime;
	pushBuf[26] = 0.0f;
	pushBuf[27] = 0.0f;

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
		// Phase 7.4c-cmd — parallel-paths cmd-record (sprite viewport/scissor).
		Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);
		Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.sprite.pipeline_layout, 0, 1, &vk.sprite.descriptor[frameIdx], 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record.
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.sprite.pipeline_layout, 0, 1, &vk.sprite.descriptor[frameIdx], 0, NULL );

	qvkCmdPushConstants( cmd, vk.sprite.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pushBuf ), pushBuf );
	// Phase 7.4c-cmd — parallel-paths push-constants (sprite).
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.sprite.pipeline_layout),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pushBuf ), pushBuf );

	// Sort headers into two contiguous groups by blend mode, in place
	// in the SSBO — alpha first, additive second. Adjacent additive
	// entries get bubbled to the tail with a single linear pass; this
	// keeps the GPU draw to at most two vkCmdDraws per frame
	// (one per blend variant) without giving up the gl_InstanceIndex
	// indexing pattern.
	//
	// Two-pointer partition: scan from start, anything additive gets
	// swapped with the next-from-end alpha. O(N) headers, all in the
	// host-coherent ring slot we own this frame.
	{
		byte *base = vk.sprite.headers_ptr[frameIdx];
		uint32_t lo = 0;
		uint32_t hi = vk.sprite.numThisFrame;
		// hdrFlagsOf: read flags field at offset 36 in a header
		while ( lo < hi ) {
			uint32_t loFlags = *(const uint32_t *)( base + lo * SPRITE_HEADER_BYTES + 36 );
			if ( ( loFlags & PRIM_FLAG_ADDITIVE ) == 0 ) {
				lo++;
				continue;
			}
			// lo is additive — find the previous alpha at hi-1 and swap
			hi--;
			if ( lo >= hi )
				break;
			{
				uint32_t hiFlags = *(const uint32_t *)( base + hi * SPRITE_HEADER_BYTES + 36 );
				if ( ( hiFlags & PRIM_FLAG_ADDITIVE ) != 0 )
					continue; // already additive at the tail; keep shrinking hi
			}
			{
				byte tmp[SPRITE_HEADER_BYTES];
				byte *aSlot = base + lo * SPRITE_HEADER_BYTES;
				byte *bSlot = base + hi * SPRITE_HEADER_BYTES;
				memcpy( tmp,   aSlot, SPRITE_HEADER_BYTES );
				memcpy( aSlot, bSlot, SPRITE_HEADER_BYTES );
				memcpy( bSlot, tmp,   SPRITE_HEADER_BYTES );
			}
			lo++;
		}
		// After the partition: [0..lo) is alpha, [lo..numThisFrame) is additive.
		alphaCount    = lo;
		firstAdditive = lo;
		additiveCount = vk.sprite.numThisFrame - lo;
	}

	if ( alphaCount > 0 ) {
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.sprite.pipeline_alpha );
		qvkCmdDraw( cmd, 6, alphaCount, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline + draw (sprite alpha).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.sprite.pipeline_alpha ));
		Ral_CmdDraw        ( vk.cmd->ral_cmd, 6, alphaCount, 0, 0 );
	}
	if ( additiveCount > 0 ) {
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.sprite.pipeline_additive );
		qvkCmdDraw( cmd, 6, additiveCount, 0, firstAdditive );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline + draw (sprite additive).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.sprite.pipeline_additive ));
		Ral_CmdDraw        ( vk.cmd->ral_cmd, 6, additiveCount, 0, firstAdditive );
	}

	// Invalidate cached pipeline / descriptor / depth-range so the next
	// standard draw rebinds correctly. Same cleanup pattern the IQM
	// and ribbon draws use.
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


/*
===============
Primitive particle — compute-driven GPU pool + billboard render.

Pool: PARTICLES_PER_POOL particles in 2 ping-pong SSBOs. Each frame's
compute pass reads from one pool, integrates physics + age, writes to
the other. Render pass reads from the just-written pool.

Per-frame uniform layout (std140, 128 B):
    bytes  0..63   mat4  mvp
    bytes 64..79   vec4  viewLeft   (.xyz from backEnd.viewParms.or.axis[1])
    bytes 80..95   vec4  viewUp     (.xyz from backEnd.viewParms.or.axis[2])
    bytes 96..111  vec4  eyeWorld   (.xyz from backEnd.viewParms.or.origin)
    bytes 112..127 float dt + uint poolSize + uint numClasses + uint pingPongRead

Compute descriptor set (set 0, 4 bindings):
    binding 0  UNIFORM_BUFFER  ParticleFrame
    binding 1  STORAGE_BUFFER  Particle pool (read)
    binding 2  STORAGE_BUFFER  Particle pool (write)
    binding 3  STORAGE_BUFFER  ParticleClassGPU classes[]

Render descriptor set (set 0, 3 bindings):
    binding 0  UNIFORM_BUFFER  ParticleFrame      (same UBO)
    binding 1  STORAGE_BUFFER  Particle pool (read — post-compute)
    binding 2  STORAGE_BUFFER  ParticleClassGPU classes[]

Two graphics pipeline variants (alpha and additive). The vertex shader
reads class.renderFlags and emits a degenerate triangle for particles
that don't match the bound pipeline's blend variant. The variant flag
is supplied via specialization constant 0 (PIPELINE_BLEND_MASK).
===============
*/

void vk_init_particle( void )
{
	VkDescriptorSetLayoutBinding binds[4];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkBufferCreateInfo bufInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkDescriptorSetAllocateInfo dsAlloc;
	VkWriteDescriptorSet writes[4];
	VkDescriptorBufferInfo bufInfos[4];
	const uint32_t poolBytes    = PARTICLES_PER_POOL * PARTICLE_BYTES;
	const uint32_t classesBytes = MAX_PARTICLE_CLASSES * PARTICLE_CLASS_GPU_BYTES;
	const uint32_t frameBytes   = sizeof( particleFrame_t );
	int i;

	// Layout sanity. If a future edit drifts the C struct out of sync
	// with std430, these fire at compile time, not as silent SSBO
	// misreads.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( sizeof( particleClassGPU_t ) == PARTICLE_CLASS_GPU_BYTES,
		"particleClassGPU_t must be 400 bytes to match GLSL std430 stride" );
	_Static_assert( sizeof( particleGPU_t ) == PARTICLE_BYTES,
		"particleGPU_t must be 64 bytes to match GLSL std430 stride" );
	_Static_assert( sizeof( particleFrame_t ) == 144,
		"particleFrame_t must be 144 bytes to match GLSL std140 layout" );
#else
	if ( sizeof( particleClassGPU_t ) != PARTICLE_CLASS_GPU_BYTES ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"particleClassGPU_t size mismatch: C=%u, std430=%u",
			(unsigned)sizeof( particleClassGPU_t ),
			(unsigned)PARTICLE_CLASS_GPU_BYTES );
	}
	if ( sizeof( particleGPU_t ) != PARTICLE_BYTES ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"particleGPU_t size mismatch: C=%u, std430=%u",
			(unsigned)sizeof( particleGPU_t ),
			(unsigned)PARTICLE_BYTES );
	}
	if ( sizeof( particleFrame_t ) != 144 ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"particleFrame_t size mismatch: C=%u, expected=144",
			(unsigned)sizeof( particleFrame_t ) );
	}
#endif

	memset( &vk.particle, 0, sizeof( vk.particle ) );

	// ── Compute descriptor set layout (4 bindings) ───────────────
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[3].binding         = 3;
	binds[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.particle.compute_set_layout ) );

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 1;
	pipeLayoutInfo.pSetLayouts    = &vk.particle.compute_set_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.particle.compute_pipeline_layout ) );

	// ── Render descriptor set layout (4 bindings) ────────────────
	// binding 0  UBO         — ParticleFrame
	// binding 1  STORAGE     — Particle pool (post-compute output)
	// binding 2  STORAGE     — ParticleClassGPU classes shadow
	// binding 3  IMAGE_SAMPLER × MAX_PARTICLE_CLASSES — per-class
	//            texture array, indexed in fragment shader by
	//            classHandle - 1. Phase 5.
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[2].binding         = 2;
	binds[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	binds[3].binding         = 3;
	binds[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].descriptorCount = MAX_PARTICLE_CLASSES;
	binds[3].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[3].pImmutableSamplers = NULL;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings    = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.particle.render_set_layout ) );

	// Shared sampler for the per-class texture array. Linear filter,
	// clamp-to-edge, no anisotropy — appropriate for billboard
	// particles regardless of class. vk_find_sampler deduplicates
	// against existing samplers so this may return an already-built
	// instance.
	{
		Vk_Sampler_Def sd;
		memset( &sd, 0, sizeof( sd ) );
		sd.address_mode  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.gl_mag_filter = GL_LINEAR;
		sd.gl_min_filter = GL_LINEAR;
		sd.noAnisotropy  = qtrue;
		vk.particle.sampler = vk_find_sampler( &sd );
	}

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 1;
	pipeLayoutInfo.pSetLayouts    = &vk.particle.render_set_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.particle.render_pipeline_layout ) );

	// ── Particle pool buffers (ping-pong, host-coherent) ─────────
	// Phase 5P: the ping-pong index math at the readPool/writePool
	// sites assumes NUM_COMMAND_BUFFERS == 2 (cmd buffer i ⇔ pool
	// (1-i)). Bumping NUM_COMMAND_BUFFERS for triple buffering would
	// silently underflow `1 - i` for i >= 2 and OOB-index pool_buffer.
	// Catch it at compile time; the RAL refactor will abstract this
	// via frame-in-flight, after which the assert can come out.
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
	_Static_assert( NUM_COMMAND_BUFFERS == 2,
		"Particle ping-pong assumes double-buffering. Bumping "
		"NUM_COMMAND_BUFFERS requires reworking the readPool/writePool "
		"logic and pool_buffer[N] array sizing." );
#endif
	for ( i = 0; i < 2; i++ ) {
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = poolBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.particle.pool_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.particle.pool_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.particle.pool_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.particle.pool_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.particle.pool_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.particle.pool_buffer[i], vk.particle.pool_memory[i], 0 );
		vk_ral_register_buffer( vk.particle.pool_buffer[i], poolBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.particle.pool" );

		// Initialize all slots to dead (classHandle = 0).
		memset( vk.particle.pool_ptr[i], 0, poolBytes );
	}

	// ── Class shadow SSBO (host-coherent, mapped) ────────────────
	{
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = classesBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.particle.classes_buffer ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.particle.classes_buffer, &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.particle.classes_memory ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.particle.classes_memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.particle.classes_ptr ) );
		qvkBindBufferMemory( vk.device, vk.particle.classes_buffer, vk.particle.classes_memory, 0 );
		vk_ral_register_buffer( vk.particle.classes_buffer, classesBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.particle.classes" );

		memset( vk.particle.classes_ptr, 0, classesBytes );
	}

	// ── Per-frame uniform buffer (host-coherent, one per cmd_index) ─
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t memType;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = frameBytes;
		bufInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.particle.frame_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.particle.frame_buffer[i], &memReqs );
		memType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = memType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.particle.frame_memory[i] ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.particle.frame_memory[i], 0, VK_WHOLE_SIZE, 0, (void**)&vk.particle.frame_ptr[i] ) );
		qvkBindBufferMemory( vk.device, vk.particle.frame_buffer[i], vk.particle.frame_memory[i], 0 );
		vk_ral_register_buffer( vk.particle.frame_buffer[i], frameBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.particle.frame" );

		memset( vk.particle.frame_ptr[i], 0, frameBytes );
	}

	// ── Allocate compute descriptor sets (NUM_COMMAND_BUFFERS slots) ─
	// compute_descriptor[i] reads pool[i], writes pool[1-i].
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t readPool  = (uint32_t)i;            // 0 or 1
		uint32_t writePool = (uint32_t)(1 - i);      // 1 or 0

		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.particle.compute_set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.compute_descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.particle.frame_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = frameBytes;
		bufInfos[1].buffer = vk.particle.pool_buffer[readPool];
		bufInfos[1].offset = 0;
		bufInfos[1].range  = poolBytes;
		bufInfos[2].buffer = vk.particle.pool_buffer[writePool];
		bufInfos[2].offset = 0;
		bufInfos[2].range  = poolBytes;
		bufInfos[3].buffer = vk.particle.classes_buffer;
		bufInfos[3].offset = 0;
		bufInfos[3].range  = classesBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.particle.compute_descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];
		writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet          = vk.particle.compute_descriptor[i];
		writes[1].dstBinding      = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo     = &bufInfos[1];
		writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet          = vk.particle.compute_descriptor[i];
		writes[2].dstBinding      = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].pBufferInfo     = &bufInfos[2];
		writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet          = vk.particle.compute_descriptor[i];
		writes[3].dstBinding      = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[3].pBufferInfo     = &bufInfos[3];

		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	}

	// ── Allocate render descriptor sets (NUM_COMMAND_BUFFERS slots) ─
	// render_descriptor[i] reads pool[1-i] (the post-compute output
	// when compute_descriptor[i] just ran).
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		uint32_t renderPool = (uint32_t)(1 - i);     // 1 or 0

		memset( &dsAlloc, 0, sizeof( dsAlloc ) );
		dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsAlloc.descriptorPool     = vk.descriptor_pool;
		dsAlloc.descriptorSetCount = 1;
		dsAlloc.pSetLayouts        = &vk.particle.render_set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.render_descriptor[i] ) );

		memset( bufInfos, 0, sizeof( bufInfos ) );
		bufInfos[0].buffer = vk.particle.frame_buffer[i];
		bufInfos[0].offset = 0;
		bufInfos[0].range  = frameBytes;
		bufInfos[1].buffer = vk.particle.pool_buffer[renderPool];
		bufInfos[1].offset = 0;
		bufInfos[1].range  = poolBytes;
		bufInfos[2].buffer = vk.particle.classes_buffer;
		bufInfos[2].offset = 0;
		bufInfos[2].range  = classesBytes;

		memset( writes, 0, sizeof( writes ) );
		writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet          = vk.particle.render_descriptor[i];
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo     = &bufInfos[0];
		writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet          = vk.particle.render_descriptor[i];
		writes[1].dstBinding      = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo     = &bufInfos[1];
		writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet          = vk.particle.render_descriptor[i];
		writes[2].dstBinding      = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].pBufferInfo     = &bufInfos[2];

		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

		// Note on binding 3 (per-class sampler array): population is
		// deferred. vk_init_particle runs from InitOpenGL →
		// vk_initialize, BEFORE R_InitImages creates tr.whiteImage.
		// R_Init calls vk_init_particle_textures AFTER R_InitImages
		// to populate this binding eagerly against a valid
		// tr.whiteImage.
	}

	// ── Compute pipeline ──────────────────────────────────────────
	{
		VkComputePipelineCreateInfo cpInfo;
		memset( &cpInfo, 0, sizeof( cpInfo ) );
		cpInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		cpInfo.layout = vk.particle.compute_pipeline_layout;
		cpInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		cpInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
		cpInfo.stage.module = vk.modules.particle_integrate_cs;
		cpInfo.stage.pName  = "main";
		VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &cpInfo, NULL, &vk.particle.compute_pipeline ) );

		// Phase 7.4c-pipeline (PART F) — parallel RAL compute pipeline.
		// Same SPV blob via the side table; numBindGroupLayouts = 0 per the
		// parallel-paths era (VUID-VkComputePipelineCreateInfo-layout-07988
		// will fire under validation; harmless — rendering still uses legacy).
		if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
			ralBackend_t *backend = vk_ral_get_backend();
			const uint8_t *cs_bytes = NULL;
			uint32_t       cs_size  = 0;
			if ( backend && vk_shader_blob_lookup( vk.modules.particle_integrate_cs, &cs_bytes, &cs_size ) ) {
				ralComputePipelineCreateInfo_t cci;
				memset( &cci, 0, sizeof( cci ) );
				cci.computeSpirv      = (const uint32_t *)cs_bytes;
				cci.computeSpirvSize  = cs_size;
				cci.pushConstantSize  = 16;            // particle compute push-constants: time deltas
				cci.numBindGroupLayouts = 0;           // see PART F note above
				cci.externalLayout    = vk.particle.ral_compute_pipeline_layout;   // Phase 7.4c-submit-A3
				cci.debugName         = "ral-particle-integrate-cs";
				vk.particle.ral_compute_pipeline = Ral_CreateComputePipeline( backend, &cci );
				if ( !vk.particle.ral_compute_pipeline ) {
					ri.Log( SEV_DEBUG, "[VK->RAL] particle compute Ral_CreateComputePipeline returned NULL\n" );
				}
			}
		}
	}

	// ── Render pipelines (alpha + additive variants) ──────────────
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		VkSpecializationMapEntry specMap;
		VkSpecializationInfo specInfo;
		uint32_t blendMaskValue;
		int variant;

		// Specialization: vertex stage gets PIPELINE_BLEND_MASK = 0
		// (alpha) or 1 (additive). Filled per-variant inside the loop.
		memset( &specMap, 0, sizeof( specMap ) );
		specMap.constantID = 0;
		specMap.offset     = 0;
		specMap.size       = sizeof( uint32_t );
		memset( &specInfo, 0, sizeof( specInfo ) );
		specInfo.mapEntryCount = 1;
		specInfo.pMapEntries   = &specMap;
		specInfo.dataSize      = sizeof( uint32_t );
		specInfo.pData         = &blendMaskValue;

		memset( stages, 0, sizeof( stages ) );
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.particle_vs;
		stages[0].pName  = "main";
		stages[0].pSpecializationInfo = &specInfo;
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.particle_fs;
		stages[1].pName  = "main";

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates    = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width    = (float)vk.renderWidth;
		viewport.height   = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width  = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports    = &viewport;
		viewportState.scissorCount  = 1;
		viewportState.pScissors     = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = VK_CULL_MODE_NONE;
		rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = VK_TRUE;
		depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments    = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount          = 2;
		gpInfo.pStages             = stages;
		gpInfo.pVertexInputState   = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState      = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState   = &multisampling;
		gpInfo.pDepthStencilState  = &depthStencil;
		gpInfo.pColorBlendState    = &colorBlend;
		gpInfo.pDynamicState       = &dynamicState;
		gpInfo.layout              = vk.particle.render_pipeline_layout;
		gpInfo.renderPass          = vk.render_pass.main;
		gpInfo.subpass             = 0;

		// 0 = alpha (SRC_ALPHA / ONE_MINUS_SRC_ALPHA),
		// 1 = additive (SRC_ALPHA / ONE).
		for ( variant = 0; variant < 2; variant++ ) {
			blendMaskValue = (uint32_t)variant;

			memset( &blendAttach, 0, sizeof( blendAttach ) );
			blendAttach.blendEnable         = VK_TRUE;
			blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttach.dstColorBlendFactor = (variant == 0)
				? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
				: VK_BLEND_FACTOR_ONE;
			blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
			blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
			blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
			blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
				(variant == 0) ? &vk.particle.render_pipeline_alpha : &vk.particle.render_pipeline_additive ) );

			// Phase 7.4c-pipeline-followup-4 — particle render (vk.particle.render_pipeline_layout).
			// Binding contract from vk_init_particle (vk.c:~5935) + particle.{vert,frag}:
			// NO push constant; 1 BGL = vk.particle.render_set_layout (4 bindings: UBO + 2 SSBOs +
			// sampler array, set 0 bindings 0/1/2/3). Topology TRIANGLE_LIST — particle.vert
			// emits 6 vertices per quad via gl_VertexIndex.
			// Spec constant: layout(constant_id = 0) const uint PIPELINE_BLEND_MASK; variant 0
			// (alpha) sends value 0, variant 1 (additive) sends value 1.
			if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
				vk_ral_special_pipeline_params_t p;
				ralSpecConstant_t specs[1];
				memset( &p,    0, sizeof( p    ) );
				memset( specs, 0, sizeof( specs ) );
				p.vs_module          = vk.modules.particle_vs;
				p.fs_module          = vk.modules.particle_fs;
				p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
				p.cullMode           = RAL_CULL_NONE;
				p.depthTestEnable    = qtrue;
				p.depthWriteEnable   = qfalse;
				p.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
				p.blendEnable        = qtrue;
				p.srcColor           = RAL_BLEND_SRC_ALPHA;
				p.dstColor           = ( variant == 0 ) ? RAL_BLEND_ONE_MINUS_SRC_ALPHA : RAL_BLEND_ONE;
				p.blendOp            = RAL_BLEND_OP_ADD;
				if ( vk.particle.ral_bgl_render ) { p.bgls[ p.numBgls++ ] = vk.particle.ral_bgl_render; }
				specs[0].constantId = 0;
				specs[0].value      = (uint32_t)variant;
				p.specConstants     = specs;
				p.numSpecConstants  = 1;
				p.debugName         = ( variant == 0 ) ? "ral-particle-render-alpha" : "ral-particle-render-additive";
				p.externalLayout    = vk.particle.ral_render_pipeline_layout;   // Phase 7.4c-submit-A3
				p.externalRenderPass = vk.ral_render_pass.main;
				if ( variant == 0 ) vk.particle.ral_render_pipeline_alpha    = vk_ral_create_special_pipeline( &p );
				else                vk.particle.ral_render_pipeline_additive = vk_ral_create_special_pipeline( &p );
			}
		}
	}

	vk.particle.pingPongRead  = 0;
	vk.particle.prevSceneTime = 0.0f;
	vk.particle.numClasses    = 0;
	vk.particle.nextSlot      = 0;
	vk.particle.available     = qtrue;

	ri.Log( SEV_INFO, "Vulkan particle subsystem initialized (pool=%u particles, classes=%u, %u KB pool memory)\n",
		(unsigned)PARTICLES_PER_POOL,
		(unsigned)MAX_PARTICLE_CLASSES,
		(unsigned)( ( poolBytes * 2 + classesBytes ) / 1024 ) );
}


// Phase 5 eager texture init: populate the per-class sampler array
// (binding 3) on every per-frame render descriptor set. Walks
// classImages[] (mostly NULL at engine init) and falls back to
// tr.whiteImage for unregistered slots. Called once from R_Init
// AFTER R_InitImages has created tr.whiteImage; cannot run earlier
// because vk_init_particle and vk_init_descriptors both execute
// from InitOpenGL, before R_InitImages.
//
// On vid_restart, vk_init_descriptors's particle re-alloc clears
// classImages[] and numClasses; R_InitImages re-creates
// tr.whiteImage at a fresh address; this function then re-runs
// from the same R_Init call site to repopulate against the fresh
// pointers. RE_RegisterParticleClass overwrites per-class slots
// with their resolved shader images later, when cgame loads.
//
// The fatal-on-NULL guard converts a wrong-phase call into a clear
// startup failure rather than a silent unbound-texture issue.
void vk_init_particle_textures( void )
{
	VkDescriptorImageInfo imgInfos[MAX_PARTICLE_CLASSES];
	VkWriteDescriptorSet  imgWrite;
	uint32_t j, k;

	if ( !vk.particle.available ) return;
	if ( tr.whiteImage == NULL ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"vk_init_particle_textures: tr.whiteImage is NULL — "
			"called before R_InitImages?" );
		return;
	}

	for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
		for ( k = 0; k < MAX_PARTICLE_CLASSES; k++ ) {
			image_t *img = vk.particle.classImages[k]
			             ? vk.particle.classImages[k]
			             : tr.whiteImage;
			memset( &imgInfos[k], 0, sizeof( imgInfos[k] ) );
			imgInfos[k].imageView   = img->view;
			imgInfos[k].sampler     = vk.particle.sampler;
			imgInfos[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		memset( &imgWrite, 0, sizeof( imgWrite ) );
		imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imgWrite.dstSet          = vk.particle.render_descriptor[j];
		imgWrite.dstBinding      = 3;
		imgWrite.dstArrayElement = 0;
		imgWrite.descriptorCount = MAX_PARTICLE_CLASSES;
		imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imgWrite.pImageInfo      = imgInfos;
		qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
	}
}


void vk_particle_set_class_image( int handle, image_t *image )
{
	VkDescriptorImageInfo imgInfo;
	VkWriteDescriptorSet  imgWrite;
	uint32_t              j;

	// Phase 5: encapsulate the per-class sampler-array slot update
	// so RE_RegisterParticleClass (in tr_scene.c) doesn't need
	// access to the static qvk* function pointers in this TU.
	// Writes the same (image->view, vk.particle.sampler) pair into
	// slot (handle - 1) of binding 3 on every per-frame render
	// descriptor set. The other 63 slots were populated with
	// tr.whiteImage by vk_init_particle_textures (called from
	// R_Init after R_InitImages); this function only touches the
	// registered class's slot.
	if ( !vk.particle.available ) return;
	if ( handle < 1 || handle > MAX_PARTICLE_CLASSES ) return;
	if ( image == NULL ) return;

	memset( &imgInfo, 0, sizeof( imgInfo ) );
	imgInfo.imageView   = image->view;
	imgInfo.sampler     = vk.particle.sampler;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
		memset( &imgWrite, 0, sizeof( imgWrite ) );
		imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imgWrite.dstSet          = vk.particle.render_descriptor[j];
		imgWrite.dstBinding      = 3;
		imgWrite.dstArrayElement = (uint32_t)( handle - 1 );
		imgWrite.descriptorCount = 1;
		imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imgWrite.pImageInfo      = &imgInfo;
		qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
	}
}


// ── shared primitive shader image registry ──────────────────────────
// One global registry, indexed by qhandle_t. Today consumed only by
// ribbon (descriptor binding 2). Future beam pipeline will add itself
// to vk_register_primitive_shader_image's update broadcast in turn 5B.
// See vk.h for the registry contract.
image_t *vk_primitive_shader_images[PRIMITIVE_SHADER_IMAGE_MAX];


/*
================
vk_init_primitive_shader_images

Eager init of the primitive shader image registry + ribbon's binding-2
sampler array. Called from R_Init AFTER R_InitImages creates
tr.whiteImage; cannot run earlier because the descriptor write needs
a valid image_t. Pattern mirrors vk_init_particle_textures: walk all
NUM_COMMAND_BUFFERS descriptor sets and populate every slot with
either the registered image or tr.whiteImage as fallback.

Re-runs on every R_Init, so vid_restart correctly repopulates against
the freshly-recreated tr.whiteImage and any previously-registered
image pointers (those become dangling after R_DeleteTextures and must
be reset; this function clears the registry to tr.whiteImage as the
fresh-start state, and any consumer that wants a textured shader
re-registers via RE_RegisterPrimitiveShader).
================
*/
static void vk_init_primitive_shader_stages( void );

void vk_init_primitive_shader_images( void )
{
	int i;

	if ( tr.whiteImage == NULL ) {
		ri.Terminate( TERM_UNRECOVERABLE,
			"vk_init_primitive_shader_images: tr.whiteImage is NULL — "
			"called before R_InitImages?" );
		return;
	}

	// Reset the registry to a known fresh-start state. Any
	// previously-cached pointers from before vid_restart are now
	// dangling. Per-shader registrations re-run from cgame init
	// after R_Init returns and overwrite the slots they care about.
	for ( i = 0; i < PRIMITIVE_SHADER_IMAGE_MAX; i++ ) {
		vk_primitive_shader_images[i] = tr.whiteImage;
	}

	// Phase 5F: allocate (first call) and re-zero (every call) the
	// per-stage SSBO. cgame re-registers shaders post-vid_restart
	// via RE_RegisterPrimitiveShader, which re-populates entries.
	vk_init_primitive_shader_stages();

	// Sampler reuse: vk.particle.sampler is a shared linear/clamp/
	// no-anisotropy sampler created in vk_init_particle. Used by
	// ribbon binding 2 (helix has uvScroll=0 so CLAMP_TO_EDGE is
	// fine). Beam binding 1 uses vk.beam.sampler_repeat instead —
	// see Phase 5J: scrolling lightning shaders need REPEAT to
	// wrap large UVs without saturating to the texture edge.
	{
		VkDescriptorImageInfo imgInfosClamp[PRIMITIVE_SHADER_IMAGE_MAX];
		VkDescriptorImageInfo imgInfosRepeat[PRIMITIVE_SHADER_IMAGE_MAX];
		VkWriteDescriptorSet  imgWrite;
		uint32_t              j, k;

		// Build two parallel imageInfo arrays — same images, two
		// samplers. Cheap (each is 16 entries × NUM_COMMAND_BUFFERS
		// frames; rebuilt once per init/vid_restart, not per frame).
		for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
			for ( k = 0; k < PRIMITIVE_SHADER_IMAGE_MAX; k++ ) {
				image_t *img = vk_primitive_shader_images[k];
				memset( &imgInfosClamp[k], 0, sizeof( imgInfosClamp[k] ) );
				imgInfosClamp[k].imageView   = img->view;
				imgInfosClamp[k].sampler     = vk.particle.sampler;
				imgInfosClamp[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				memset( &imgInfosRepeat[k], 0, sizeof( imgInfosRepeat[k] ) );
				imgInfosRepeat[k].imageView   = img->view;
				imgInfosRepeat[k].sampler     = vk.beam.sampler_repeat;
				imgInfosRepeat[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			if ( vk.ribbon.available ) {
				memset( &imgWrite, 0, sizeof( imgWrite ) );
				imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				imgWrite.dstSet          = vk.ribbon.descriptor[j];
				imgWrite.dstBinding      = 2;
				imgWrite.dstArrayElement = 0;
				imgWrite.descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
				imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				imgWrite.pImageInfo      = imgInfosClamp;
				qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
			}

			if ( vk.beam.available ) {
				memset( &imgWrite, 0, sizeof( imgWrite ) );
				imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				imgWrite.dstSet          = vk.beam.descriptor[j];
				imgWrite.dstBinding      = 1;
				imgWrite.dstArrayElement = 0;
				imgWrite.descriptorCount = PRIMITIVE_SHADER_IMAGE_MAX;
				imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				imgWrite.pImageInfo      = imgInfosRepeat;
				qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
			}

			// Phase 5G: bind the multi-stage SSBOs to the beam set's
			// bindings 2 and 3. vk_init_primitive_shader_stages
			// (called above) just allocated these buffers, so this
			// is the earliest time the writes are valid. On vid_restart
			// the descriptor pool reset has invalidated the previous
			// writes; re-emitting here keeps the set healthy.
			if ( vk.beam.available
			  && vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
				VkDescriptorBufferInfo bufInfos[2];
				VkWriteDescriptorSet   bufWrites[2];

				memset( bufInfos, 0, sizeof( bufInfos ) );
				bufInfos[0].buffer = vk.primitive_stages_buffer;
				bufInfos[0].offset = 0;
				bufInfos[0].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;

				bufInfos[1].buffer = vk.primitive_stage_counts_buffer;
				bufInfos[1].offset = 0;
				bufInfos[1].range  = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
					* sizeof( uint32_t );

				memset( bufWrites, 0, sizeof( bufWrites ) );
				bufWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				bufWrites[0].dstSet          = vk.beam.descriptor[j];
				bufWrites[0].dstBinding      = 2;
				bufWrites[0].descriptorCount = 1;
				bufWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				bufWrites[0].pBufferInfo     = &bufInfos[0];

				bufWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				bufWrites[1].dstSet          = vk.beam.descriptor[j];
				bufWrites[1].dstBinding      = 3;
				bufWrites[1].descriptorCount = 1;
				bufWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				bufWrites[1].pBufferInfo     = &bufInfos[1];

				qvkUpdateDescriptorSets( vk.device, 2, bufWrites, 0, NULL );
			}
		}
	}
}


/*
================
vk_register_primitive_shader_image

Idempotently register an image_t at registry slot `handle` and push
the change to every primitive descriptor set that consumes the
registry. Today: ribbon only. Beam (turn 5B) will append a second
update call here.

Out-of-range handles (>= PRIMITIVE_SHADER_IMAGE_MAX) are silently
ignored — the shader-side slot clamp at the fragment shader
(`slot = handle < 64u ? handle : 0u`) renders such submissions
through slot 0 (tr.whiteImage), so they appear untextured rather
than crashing.
================
*/
void vk_register_primitive_shader_image( int slot, image_t *image )
{
	VkDescriptorImageInfo imgInfoClamp;
	VkDescriptorImageInfo imgInfoRepeat;
	VkWriteDescriptorSet  imgWrite;
	uint32_t              j;

	// Phase 5K: parameter is now a primitive registry SLOT, not a qhandle.
	// Slot 0 reserved for tr.whiteImage; usable slots [1, PRIMITIVE_SHADER_IMAGE_MAX).
	if ( slot <= 0 || slot >= PRIMITIVE_SHADER_IMAGE_MAX ) return;
	if ( image == NULL ) return;
	if ( vk_primitive_shader_images[slot] == image ) return; // idempotent

	vk_primitive_shader_images[slot] = image;

	// Phase 5J: ribbon binding 2 uses vk.particle.sampler (CLAMP_TO_EDGE);
	// beam binding 1 uses vk.beam.sampler_repeat (REPEAT). Same image,
	// different sampler.
	memset( &imgInfoClamp, 0, sizeof( imgInfoClamp ) );
	imgInfoClamp.imageView   = image->view;
	imgInfoClamp.sampler     = vk.particle.sampler;
	imgInfoClamp.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	memset( &imgInfoRepeat, 0, sizeof( imgInfoRepeat ) );
	imgInfoRepeat.imageView   = image->view;
	imgInfoRepeat.sampler     = vk.beam.sampler_repeat;
	imgInfoRepeat.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	for ( j = 0; j < NUM_COMMAND_BUFFERS; j++ ) {
		if ( vk.ribbon.available ) {
			memset( &imgWrite, 0, sizeof( imgWrite ) );
			imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imgWrite.dstSet          = vk.ribbon.descriptor[j];
			imgWrite.dstBinding      = 2;
			imgWrite.dstArrayElement = (uint32_t)slot;
			imgWrite.descriptorCount = 1;
			imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			imgWrite.pImageInfo      = &imgInfoClamp;
			qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
		}
		if ( vk.beam.available ) {
			memset( &imgWrite, 0, sizeof( imgWrite ) );
			imgWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imgWrite.dstSet          = vk.beam.descriptor[j];
			imgWrite.dstBinding      = 1;
			imgWrite.dstArrayElement = (uint32_t)slot;
			imgWrite.descriptorCount = 1;
			imgWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			imgWrite.pImageInfo      = &imgInfoRepeat;
			qvkUpdateDescriptorSets( vk.device, 1, &imgWrite, 0, NULL );
		}
	}
}


/*
================
vk_alloc_primitive_shader_image_slot

Phase 5F support: allocate (or reuse) a registry slot for a stage>0
image whose qhandle slot is already taken by stage 0's image of the
same shader. Linear scan returns an existing slot when the image is
already registered; otherwise writes into the first slot still
holding tr.whiteImage and broadcasts the descriptor write via
vk_register_primitive_shader_image.

Slots 0 (reserved for tr.whiteImage) and the qhandle-occupied slots
1..N are skipped during the free-slot search. Returns -1 if no free
slot remains.

The collision case (a future shader's qhandle landing on a slot
allocated here for a stage>0 image) is documented but unhandled in
this turn — most multi-stage shaders reuse the same image across
stages (the LG case), so the linear-scan reuse path covers them
without needing a new slot.
================
*/
int vk_alloc_primitive_shader_image_slot( image_t *image )
{
	int i;

	if ( image == NULL ) return -1;

	// Reuse path: if `image` is already registered anywhere, return
	// its slot. Includes slot 0 (tr.whiteImage) — but caller filters
	// that case before getting here for non-trivial images.
	for ( i = 1; i < PRIMITIVE_SHADER_IMAGE_MAX; i++ ) {
		if ( vk_primitive_shader_images[i] == image ) {
			return i;
		}
	}

	// Allocate path: scan from the end backward to minimize collision
	// with future qhandle assignments (which start low). First slot
	// still holding tr.whiteImage wins.
	for ( i = PRIMITIVE_SHADER_IMAGE_MAX - 1; i >= 1; i-- ) {
		if ( vk_primitive_shader_images[i] == tr.whiteImage ) {
			vk_register_primitive_shader_image( i, image );
			return i;
		}
	}

	return -1; // exhausted
}


/*
================
vk_qhandle_to_prim_slot

Phase 5K: translate a cgame qhandle into the engine-internal primitive
registry slot used by GPU SSBOs. Reads the indirection table populated
by RE_RegisterPrimitiveShader. Used at SSBO write sites to pack a
small (≤63) slot index into the GPU header where the previous design
had relied on (qhandle == slot), an assumption that broke as soon as
the qhandle counter exceeded PRIMITIVE_SHADER_IMAGE_MAX.

Out-of-range or unregistered qhandles return slot 0 (whiteImage),
rendering as untextured rather than producing OOB SSBO reads on the
GPU.
================
*/
unsigned int vk_qhandle_to_prim_slot( qhandle_t h )
{
	uint8_t slot;

	if ( h <= 0 || h >= VK_PRIM_QHANDLE_MAX ) return 0;

	slot = vk.qhandle_to_prim_slot[h];
	if ( slot == PRIMITIVE_SLOT_INVALID ) return 0;

	return (unsigned int)slot;
}


/*
================
vk_init_primitive_shader_stages

Phase 5F: lazy allocation of the per-stage SSBO that backs multi-
stage primitive shaders. Buffer is allocated on first call and
persists across vid_restart (it lives outside the descriptor pool,
so the pool reset doesn't invalidate it). Contents are zeroed on
every call so vid_restart sees a clean slate before cgame
re-registers shaders via RE_RegisterPrimitiveShader.

The buffer holds PRIMITIVE_SHADER_IMAGE_MAX × PRIMITIVE_STAGE_MAX
entries of VK_PRIMITIVE_STAGE_BYTES each = 64 × 4 × 32 = 8 KB.
Indexed [shaderHandle * PRIMITIVE_STAGE_MAX + stageNumber].

Bound to the beam pipeline by Phase 5G (this turn allocates only).
================
*/
static void vk_init_primitive_shader_stages( void )
{
	const VkDeviceSize stagesSize = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
		* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;
	const VkDeviceSize countsSize = (VkDeviceSize)PRIMITIVE_SHADER_IMAGE_MAX
		* sizeof( uint32_t );

	if ( vk.primitive_stages_buffer == VK_NULL_HANDLE ) {
		VkBufferCreateInfo bufInfo;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = stagesSize;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL,
			&vk.primitive_stages_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device,
			vk.primitive_stages_buffer, &memReqs );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL,
			&vk.primitive_stages_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.primitive_stages_buffer,
			vk.primitive_stages_memory, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.primitive_stages_memory,
			0, stagesSize, 0, &vk.primitive_stages_mapped ) );
		vk_ral_register_buffer( vk.primitive_stages_buffer, stagesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.primitive_stages" );
	}

	// Phase 5G: stage-counts companion buffer. 256 B; uploaded each
	// frame in RB_DrawBeams from the host-side count array. Decoupled
	// from the stages buffer so the vertex shader can cheaply cull
	// per-stage draws without reading every stage entry.
	if ( vk.primitive_stage_counts_buffer == VK_NULL_HANDLE ) {
		VkBufferCreateInfo bufInfo;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo;

		memset( &bufInfo, 0, sizeof( bufInfo ) );
		bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size        = countsSize;
		bufInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL,
			&vk.primitive_stage_counts_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device,
			vk.primitive_stage_counts_buffer, &memReqs );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL,
			&vk.primitive_stage_counts_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.primitive_stage_counts_buffer,
			vk.primitive_stage_counts_memory, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.primitive_stage_counts_memory,
			0, countsSize, 0, &vk.primitive_stage_counts_mapped ) );
		vk_ral_register_buffer( vk.primitive_stage_counts_buffer, countsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.primitive_stage_counts" );
	}

	// Zero-init: every call (vid_restart included). Trailing stages
	// (stageNumber >= stageCount[handle]) stay zero; consumers gate
	// per-stage draws on the count array.
	if ( vk.primitive_stages_mapped != NULL ) {
		memset( vk.primitive_stages_mapped, 0, (size_t)stagesSize );
	}
	if ( vk.primitive_stage_counts_mapped != NULL ) {
		memset( vk.primitive_stage_counts_mapped, 0, (size_t)countsSize );
	}
	memset( vk.primitive_shader_stage_counts, 0,
		sizeof( vk.primitive_shader_stage_counts ) );

	// Phase 5K: reset qhandle→primitive-slot lookup. 0xFF byte fill
	// means every entry is PRIMITIVE_SLOT_INVALID. Re-runs cleanly
	// on vid_restart; cgame re-registers primitive shaders post-restart
	// and repopulates entries through the slot allocator.
	memset( vk.qhandle_to_prim_slot, PRIMITIVE_SLOT_INVALID,
		sizeof( vk.qhandle_to_prim_slot ) );
}


/*
================
vk_shutdown_primitive_stages

Phase 5F: destroy the per-stage SSBO. Called from vk_shutdown only
(NOT from vk_release_resources / vid_restart — buffer survives
those because it's outside the descriptor pool).
================
*/
void vk_shutdown_primitive_stages( void )
{
	if ( vk.primitive_stages_buffer != VK_NULL_HANDLE ) {
		if ( vk.primitive_stages_mapped != NULL ) {
			qvkUnmapMemory( vk.device, vk.primitive_stages_memory );
			vk.primitive_stages_mapped = NULL;
		}
		vk_ral_unregister_buffer( vk.primitive_stages_buffer );
		qvkDestroyBuffer( vk.device, vk.primitive_stages_buffer, NULL );
		vk.primitive_stages_buffer = VK_NULL_HANDLE;
	}
	if ( vk.primitive_stages_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.primitive_stages_memory, NULL );
		vk.primitive_stages_memory = VK_NULL_HANDLE;
	}
	if ( vk.primitive_stage_counts_buffer != VK_NULL_HANDLE ) {
		if ( vk.primitive_stage_counts_mapped != NULL ) {
			qvkUnmapMemory( vk.device, vk.primitive_stage_counts_memory );
			vk.primitive_stage_counts_mapped = NULL;
		}
		vk_ral_unregister_buffer( vk.primitive_stage_counts_buffer );
		qvkDestroyBuffer( vk.device, vk.primitive_stage_counts_buffer, NULL );
		vk.primitive_stage_counts_buffer = VK_NULL_HANDLE;
	}
	if ( vk.primitive_stage_counts_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.primitive_stage_counts_memory, NULL );
		vk.primitive_stage_counts_memory = VK_NULL_HANDLE;
	}
	memset( vk.primitive_shader_stage_counts, 0,
		sizeof( vk.primitive_shader_stage_counts ) );
}


void vk_shutdown_particle( void )
{
	int i;

	if ( vk.particle.render_pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.render_pipeline_alpha, NULL );
		vk.particle.render_pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.particle.render_pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.render_pipeline_additive, NULL );
		vk.particle.render_pipeline_additive = VK_NULL_HANDLE;
	}
	if ( vk.particle.ral_render_pipeline_alpha    ) { Ral_DestroyPipeline( vk.particle.ral_render_pipeline_alpha    ); vk.particle.ral_render_pipeline_alpha    = NULL; }
	if ( vk.particle.ral_render_pipeline_additive ) { Ral_DestroyPipeline( vk.particle.ral_render_pipeline_additive ); vk.particle.ral_render_pipeline_additive = NULL; }
	if ( vk.particle.compute_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.compute_pipeline, NULL );
		vk.particle.compute_pipeline = VK_NULL_HANDLE;
	}
	// Phase 7.4c-pipeline PART F — sibling RAL compute pipeline teardown.
	if ( vk.particle.ral_compute_pipeline != NULL ) {
		Ral_DestroyPipeline( vk.particle.ral_compute_pipeline );
		vk.particle.ral_compute_pipeline = NULL;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.particle.frame_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.particle.frame_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.particle.frame_buffer[i], NULL );
			vk.particle.frame_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.particle.frame_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.particle.frame_memory[i], NULL );
			vk.particle.frame_memory[i] = VK_NULL_HANDLE;
			vk.particle.frame_ptr[i] = NULL;
		}
	}

	if ( vk.particle.classes_buffer != VK_NULL_HANDLE ) {
		vk_ral_unregister_buffer( vk.particle.classes_buffer );
		qvkDestroyBuffer( vk.device, vk.particle.classes_buffer, NULL );
		vk.particle.classes_buffer = VK_NULL_HANDLE;
	}
	if ( vk.particle.classes_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.particle.classes_memory, NULL );
		vk.particle.classes_memory = VK_NULL_HANDLE;
		vk.particle.classes_ptr = NULL;
	}

	for ( i = 0; i < 2; i++ ) {
		if ( vk.particle.pool_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.particle.pool_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.particle.pool_buffer[i], NULL );
			vk.particle.pool_buffer[i] = VK_NULL_HANDLE;
		}
		if ( vk.particle.pool_memory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.particle.pool_memory[i], NULL );
			vk.particle.pool_memory[i] = VK_NULL_HANDLE;
			vk.particle.pool_ptr[i] = NULL;
		}
	}

	if ( vk.particle.render_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.particle.render_pipeline_layout, NULL );
		vk.particle.render_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.particle.compute_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.particle.compute_pipeline_layout, NULL );
		vk.particle.compute_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.particle.ral_bgl_render  ) { Ral_DestroyBindGroupLayout( vk.particle.ral_bgl_render  ); vk.particle.ral_bgl_render  = NULL; }
	if ( vk.particle.ral_bgl_compute ) { Ral_DestroyBindGroupLayout( vk.particle.ral_bgl_compute ); vk.particle.ral_bgl_compute = NULL; }
	if ( vk.particle.render_set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.particle.render_set_layout, NULL );
		vk.particle.render_set_layout = VK_NULL_HANDLE;
	}
	if ( vk.particle.compute_set_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.particle.compute_set_layout, NULL );
		vk.particle.compute_set_layout = VK_NULL_HANDLE;
	}

	vk.particle.available = qfalse;
}


void RB_RunParticleCompute( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	float currentSceneTime, dt;
	particleFrame_t *frameDst;
	VkBufferMemoryBarrier barrier;
	uint32_t pingRead;

	if ( !vk.particle.available )
		return;

	// IMPORTANT: this function is called from vk_begin_frame, BEFORE
	// the main render pass opens. vkCmdDispatch is spec-forbidden
	// inside a render pass instance (VUID-vkCmdDispatch-renderpass);
	// keeping the call out here is the entire point of phase 2's
	// Hypothesis-A fix. Do NOT call this from inside RB_DrawSurfs.
	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// backEnd.refdef.floatTime here holds the PREVIOUS frame's value
	// — refdef isn't updated for the current frame until
	// RB_BeginDrawingView runs (later, inside the main render pass).
	// The dt computed here is therefore the delta from frame N-2 to
	// frame N-1, applied to particles being integrated for frame N's
	// render. One frame of integration latency. Acceptable for
	// particles; not a bug; do not "fix" by reading time elsewhere.
	//
	// dt clamp: covers map-change resets (negative delta, clamp to 0)
	// and long pauses / breakpoints (huge delta, clamp to 100ms so
	// particles skip a frame instead of teleporting).
	currentSceneTime = (float)backEnd.refdef.floatTime;
	dt = currentSceneTime - vk.particle.prevSceneTime;
	if ( dt < 0.0f ) dt = 0.0f;
	if ( dt > 0.1f ) dt = 0.1f;
	vk.particle.prevSceneTime = currentSceneTime;

	// Update the COMPUTE region of the per-frame UBO (bytes 112..127)
	// and the SHARED region (bytes 128..143). The render region
	// (bytes 0..111: mvp, viewLeft, viewUp, eyeWorld) is filled later
	// by RB_DrawParticles when backEnd.viewParms is valid. Field-by-
	// field assignment leaves the render region untouched.
	//
	// Block 9 sweep: the old particleFrame_t.identityLight slot is now
	// plain reserved padding (pad0..pad3) — no shader consumes it in the
	// linear pipeline. Zero pad0 deterministically; pad1..pad3 are never
	// read so they're left as-is.
	frameDst = (particleFrame_t *)vk.particle.frame_ptr[frameIdx];
	frameDst->dt            = dt;
	frameDst->poolSize      = PARTICLES_PER_POOL;
	frameDst->numClasses    = vk.particle.numClasses;
	frameDst->pingPongRead  = vk.particle.pingPongRead;
	frameDst->pad0          = 0.0f;

	pingRead = vk.particle.pingPongRead;

	// Bind compute pipeline + descriptor set, dispatch.
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.particle.compute_pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline (particle compute).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.particle.compute_pipeline ));
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.particle.compute_pipeline_layout, 0, 1,
		&vk.particle.compute_descriptor[pingRead], 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record.
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.particle.compute_pipeline_layout, 0, 1,
		&vk.particle.compute_descriptor[pingRead], 0, NULL );

	// Dispatch ceil(PARTICLES_PER_POOL / 64). Workgroup size = 64.
	qvkCmdDispatch( cmd, ( PARTICLES_PER_POOL + 63 ) / 64, 1, 1 );
	// Phase 7.4c-cmd — parallel-paths dispatch (particle compute).
	Ral_CmdDispatch( vk.cmd->ral_cmd, ( PARTICLES_PER_POOL + 63 ) / 64, 1, 1 );

	// Barrier: compute writes to pool[1-pingRead] must be visible to
	// the next vertex shader read in RB_DrawParticles. Now outside any
	// render pass, so no self-dependency machinery is needed.
	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer              = vk.particle.pool_buffer[1 - pingRead];
	barrier.offset              = 0;
	barrier.size                = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		0,
		0, NULL,
		1, &barrier,
		0, NULL );
	// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier (particle
	// compute→vertex). The pool_buffer VkBuffer is registered with the RAL
	// buffer tracker at vk_initialize time, so vk_ral_lookup_buffer succeeds.
	{
		static qboolean warned;
		vk_ral_parallel_pipeline_barrier_buffer(
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			&barrier, &warned, "particle-compute-to-vertex" );
	}

	// Flip ping-pong for next frame.
	vk.particle.pingPongRead = 1 - vk.particle.pingPongRead;
}


void RB_DrawParticles( void )
{
	VkCommandBuffer cmd;
	int frameIdx;
	uint32_t renderIdx;
	particleFrame_t *frameDst;
	const float *p;
	float proj[16];
	float mvp[16];

	if ( !vk.particle.available
	  || vk.renderPassIndex != RENDER_PASS_MAIN )
		return;

	cmd      = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// Update the RENDER region of the per-frame UBO (bytes 0..111).
	// The compute region (bytes 112..127) was filled earlier in
	// RB_RunParticleCompute (called from vk_begin_frame). Field-by-
	// field assignment leaves the compute region untouched.
	//
	// MVP construction — same pattern as ribbon / sprite (proj y-flip
	// for Vulkan clip space, world.modelMatrix already has the q3
	// z-up → gl y-up flip baked in).
	p = backEnd.viewParms.projectionMatrix;
	memcpy( proj, p, sizeof( proj ) );
	proj[5] = -p[5];
	myGlMultMatrix( backEnd.viewParms.world.modelMatrix, proj, mvp );

	frameDst = (particleFrame_t *)vk.particle.frame_ptr[frameIdx];
	memcpy( frameDst->mvp,      mvp,                          64 );
	memcpy( frameDst->viewLeft, backEnd.viewParms.or.axis[1], sizeof( vec3_t ) );
	frameDst->viewLeft[3] = 0.0f;
	memcpy( frameDst->viewUp,   backEnd.viewParms.or.axis[2], sizeof( vec3_t ) );
	frameDst->viewUp[3] = 0.0f;
	memcpy( frameDst->eyeWorld, backEnd.viewParms.or.origin,  sizeof( vec3_t ) );
	frameDst->eyeWorld[3] = 0.0f;

	// renderIdx selects the descriptor set whose particle pool binding
	// is the post-compute output. compute_descriptor[i] writes pool[1-i],
	// so render_descriptor[i] reads pool[1-i] — index i matches.
	// RB_RunParticleCompute flipped pingPongRead at its tail, so the
	// PRE-flip value (= the index used by compute this frame) is
	// (vk.particle.pingPongRead ^ 1).
	renderIdx = vk.particle.pingPongRead ^ 1u;

	{
		VkViewport viewport;
		VkRect2D   scissor;
		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x        = (float)backEnd.viewParms.viewportX;
		viewport.y        = (float)backEnd.viewParms.viewportY;
		viewport.width    = (float)backEnd.viewParms.viewportWidth;
		viewport.height   = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;
		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x      = backEnd.viewParms.viewportX;
		scissor.offset.y      = backEnd.viewParms.viewportY;
		scissor.extent.width  = backEnd.viewParms.viewportWidth;
		scissor.extent.height = backEnd.viewParms.viewportHeight;
		qvkCmdSetViewport( cmd, 0, 1, &viewport );
		qvkCmdSetScissor ( cmd, 0, 1, &scissor );
		// Phase 7.4c-cmd — parallel-paths cmd-record (particle viewport/scissor).
		Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);
		Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);
	}

	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.particle.render_pipeline_layout, 0, 1,
		&vk.particle.render_descriptor[renderIdx], 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record.
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.particle.render_pipeline_layout, 0, 1,
		&vk.particle.render_descriptor[renderIdx], 0, NULL );

	// Two passes: alpha-blend particles, then additive. Each draw
	// dispatches the full pool (16K instances of 6 vertices); the
	// vertex shader emits degenerate triangles for slots whose
	// blend variant doesn't match the bound pipeline (and for dead
	// slots). 16K of small fully-culled quads is cheap on the GPU.
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.particle.render_pipeline_alpha );
	qvkCmdDraw( cmd, 6, PARTICLES_PER_POOL, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline + draw (particle alpha).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.particle.render_pipeline_alpha ));
	Ral_CmdDraw        ( vk.cmd->ral_cmd, 6, PARTICLES_PER_POOL, 0, 0 );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.particle.render_pipeline_additive );
	qvkCmdDraw( cmd, 6, PARTICLES_PER_POOL, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline + draw (particle additive).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.particle.render_pipeline_additive ));
	Ral_CmdDraw        ( vk.cmd->ral_cmd, 6, PARTICLES_PER_POOL, 0, 0 );

	// Invalidate cached pipeline / descriptor / depth-range so the
	// next standard draw rebinds correctly. Same cleanup pattern the
	// IQM, ribbon, and sprite draws use.
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range   = DEPTH_RANGE_COUNT;
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end   = 0;
}


#if FEAT_IQM
/*
===============
IQM GPU skinning — self-contained pipeline for skeletal IQM models
===============
*/

/*
===============
vk_init_iqm_gpu_skinning — create descriptor set layout, pipeline layout,
bone UBOs, descriptors, and the graphics pipeline
===============
*/
void vk_init_iqm_gpu_skinning( void )
{
	VkDescriptorSetLayoutBinding binds[1];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo pipeLayoutInfo;
	VkDescriptorSetLayout setLayouts[2];
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkBufferCreateInfo bufInfo;
	VkDescriptorSetAllocateInfo allocDesc;
	VkDescriptorBufferInfo bufDesc;
	VkWriteDescriptorSet writeDesc;

	vk.iqmGpu.available = qfalse;

	// descriptor set layout for bone matrices (set 0, binding 0, UBO)
	memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	binds[0].pImmutableSamplers = NULL;

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = binds;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.iqmGpu.set_layout_bones ) );

	// pipeline layout: push constants (MVP 64 bytes) + set 0 (bones) + set 1 (texture sampler)
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushRange.offset = 0;
	pushRange.size = 64; // mat4 MVP

	setLayouts[0] = vk.iqmGpu.set_layout_bones;
	setLayouts[1] = vk.set_layout_sampler;

	memset( &pipeLayoutInfo, 0, sizeof( pipeLayoutInfo ) );
	pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutInfo.setLayoutCount = 2;
	pipeLayoutInfo.pSetLayouts = setLayouts;
	pipeLayoutInfo.pushConstantRangeCount = 1;
	pipeLayoutInfo.pPushConstantRanges = &pushRange;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipeLayoutInfo, NULL, &vk.iqmGpu.pipeline_layout ) );

	// per-frame bone UBOs (host-visible, persistently mapped)
	memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = IQM_BONE_UBO_SIZE;
	bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		VK_CHECK( qvkCreateBuffer( vk.device, &bufInfo, NULL, &vk.iqmGpu.bone_buffer[i] ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.iqmGpu.bone_buffer[i], &memReqs );

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.iqmGpu.bone_memory[i] ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.iqmGpu.bone_buffer[i], vk.iqmGpu.bone_memory[i], 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.iqmGpu.bone_memory[i], 0, IQM_BONE_UBO_SIZE, 0, (void **)&vk.iqmGpu.bone_ptr[i] ) );
		vk_ral_register_buffer( vk.iqmGpu.bone_buffer[i], (uint64_t)bufInfo.size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                        "vk.iqmGpu.bone" );

		// allocate descriptor set
		memset( &allocDesc, 0, sizeof( allocDesc ) );
		allocDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocDesc.descriptorPool = vk.descriptor_pool;
		allocDesc.descriptorSetCount = 1;
		allocDesc.pSetLayouts = &vk.iqmGpu.set_layout_bones;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocDesc, &vk.iqmGpu.bone_descriptor[i] ) );

		// update descriptor to point at bone UBO
		memset( &bufDesc, 0, sizeof( bufDesc ) );
		bufDesc.buffer = vk.iqmGpu.bone_buffer[i];
		bufDesc.offset = 0;
		bufDesc.range = IQM_BONE_UBO_SIZE;

		memset( &writeDesc, 0, sizeof( writeDesc ) );
		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.dstSet = vk.iqmGpu.bone_descriptor[i];
		writeDesc.dstBinding = 0;
		writeDesc.dstArrayElement = 0;
		writeDesc.descriptorCount = 1;
		writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDesc.pBufferInfo = &bufDesc;

		qvkUpdateDescriptorSets( vk.device, 1, &writeDesc, 0, NULL );
	}

	// create the graphics pipeline
	{
		VkGraphicsPipelineCreateInfo gpInfo;
		VkPipelineShaderStageCreateInfo stages[2];
		VkPipelineVertexInputStateCreateInfo vertexInput;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		VkPipelineViewportStateCreateInfo viewportState;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState blendAttach;
		VkPipelineColorBlendStateCreateInfo colorBlend;
		VkPipelineDynamicStateCreateInfo dynamicState;
		VkDynamicState dynStates[2];
		VkViewport viewport;
		VkRect2D scissor;
		VkVertexInputBindingDescription iqmBindings[1];
		VkVertexInputAttributeDescription iqmAttribs[6];

		// interleaved vertex layout: pos(3f) + normal(3f) + texcoord(2f) + tangent(4f) + weights(4f) + indices(4u8)
		// stride = 3*4 + 3*4 + 2*4 + 4*4 + 4*4 + 4*1 = 12+12+8+16+16+4 = 68 bytes
		uint32_t stride = 68;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		memset( stages, 0, sizeof( stages ) );

		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk.modules.iqm_skinning_vs;
		stages[0].pName = "main";

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk.modules.iqm_skinning_fs;
		stages[1].pName = "main";

		// single interleaved binding
		iqmBindings[0].binding = 0;
		iqmBindings[0].stride = stride;
		iqmBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// location 0: position (vec3)
		iqmAttribs[0].location = 0;
		iqmAttribs[0].binding = 0;
		iqmAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		iqmAttribs[0].offset = 0;

		// location 1: normal (vec3)
		iqmAttribs[1].location = 1;
		iqmAttribs[1].binding = 0;
		iqmAttribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		iqmAttribs[1].offset = 12;

		// location 2: texcoord (vec2)
		iqmAttribs[2].location = 2;
		iqmAttribs[2].binding = 0;
		iqmAttribs[2].format = VK_FORMAT_R32G32_SFLOAT;
		iqmAttribs[2].offset = 24;

		// location 3: tangent (vec4) — xyz tangent + w bitangent sign
		iqmAttribs[3].location = 3;
		iqmAttribs[3].binding = 0;
		iqmAttribs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		iqmAttribs[3].offset = 32;

		// location 4: bone weights (vec4)
		iqmAttribs[4].location = 4;
		iqmAttribs[4].binding = 0;
		iqmAttribs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		iqmAttribs[4].offset = 48;

		// location 5: bone indices (uvec4, stored as R8G8B8A8_UINT)
		iqmAttribs[5].location = 5;
		iqmAttribs[5].binding = 0;
		iqmAttribs[5].format = VK_FORMAT_R8G8B8A8_UINT;
		iqmAttribs[5].offset = 64;

		memset( &vertexInput, 0, sizeof( vertexInput ) );
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.pVertexBindingDescriptions = iqmBindings;
		vertexInput.vertexAttributeDescriptionCount = 6;
		vertexInput.pVertexAttributeDescriptions = iqmAttribs;

		memset( &inputAssembly, 0, sizeof( inputAssembly ) );
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;

		memset( &dynamicState, 0, sizeof( dynamicState ) );
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynStates;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width = (float)vk.renderWidth;
		viewport.height = (float)vk.renderHeight;
		viewport.maxDepth = 1.0f;

		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width = vk.renderWidth;
		scissor.extent.height = vk.renderHeight;

		memset( &viewportState, 0, sizeof( viewportState ) );
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		memset( &rasterizer, 0, sizeof( rasterizer ) );
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

		memset( &multisampling, 0, sizeof( multisampling ) );
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		memset( &depthStencil, 0, sizeof( depthStencil ) );
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
#ifdef USE_REVERSED_DEPTH
		depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable = VK_FALSE;
		blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		memset( &colorBlend, 0, sizeof( colorBlend ) );
		colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments = &blendAttach;

		memset( &gpInfo, 0, sizeof( gpInfo ) );
		gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gpInfo.stageCount = 2;
		gpInfo.pStages = stages;
		gpInfo.pVertexInputState = &vertexInput;
		gpInfo.pInputAssemblyState = &inputAssembly;
		gpInfo.pViewportState = &viewportState;
		gpInfo.pRasterizationState = &rasterizer;
		gpInfo.pMultisampleState = &multisampling;
		gpInfo.pDepthStencilState = &depthStencil;
		gpInfo.pColorBlendState = &colorBlend;
		gpInfo.pDynamicState = &dynamicState;
		gpInfo.layout = vk.iqmGpu.pipeline_layout;
		gpInfo.renderPass = vk.render_pass.main;
		gpInfo.subpass = 0;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL, &vk.iqmGpu.pipeline ) );

		// Phase 7.4c-pipeline-followup-3 — IQM Q3 (pipeline_layout_iqm).
		// Binding contract per iqm_skinning.{vert,frag}: push_constant
		// Transform mat4 mvp = 64 bytes VERTEX only. Set 0 binding 0 =
		// BoneMatrices UBO (128 joints × 3 vec4 rows = 6144 bytes). Set 1
		// binding 0 = sampler2D texture0. 2 BGLs.
		if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
			vk_ral_special_pipeline_params_t p;
			memset( &p, 0, sizeof( p ) );
			p.vs_module          = vk.modules.iqm_skinning_vs;
			p.fs_module          = vk.modules.iqm_skinning_fs;
			p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
			p.cullMode           = RAL_CULL_BACK;
			p.depthTestEnable    = qtrue;
			p.depthWriteEnable   = qtrue;
			p.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
			p.blendEnable        = qfalse;
			p.pushConstantSize   = 64;
			p.pushConstantStages = RAL_STAGE_VERTEX;
			if ( vk.iqmGpu.ral_bgl_bones ) { p.bgls[ p.numBgls++ ] = vk.iqmGpu.ral_bgl_bones; }
			if ( vk.ral_bgl_sampler      ) { p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler; }
			p.debugName          = "ral-iqm-q3";
			p.externalLayout     = vk.iqmGpu.ral_pipeline_layout;   // Phase 7.4c-submit-A3
			p.externalRenderPass = vk.ral_render_pass.main;
			vk.iqmGpu.ral_pipeline = vk_ral_create_special_pipeline( &p );
		}
	}

	vk.iqmGpu.available = qtrue;

	ri.Log( SEV_INFO, "IQM GPU skinning initialized (bone UBO %d bytes)\n",
		(int)IQM_BONE_UBO_SIZE );
}


/*
===============
vk_shutdown_iqm_gpu_skinning
===============
*/
void vk_shutdown_iqm_gpu_skinning( void )
{
	if ( vk.iqmGpu.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.iqmGpu.pipeline, NULL );
		vk.iqmGpu.pipeline = VK_NULL_HANDLE;
	}
	// Phase 7.4c-pipeline-followup-3 — sibling RAL pipeline teardown.
	if ( vk.iqmGpu.ral_pipeline ) { Ral_DestroyPipeline( vk.iqmGpu.ral_pipeline ); vk.iqmGpu.ral_pipeline = NULL; }

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.iqmGpu.bone_buffer[i] != VK_NULL_HANDLE ) {
			vk_ral_unregister_buffer( vk.iqmGpu.bone_buffer[i] );
			qvkDestroyBuffer( vk.device, vk.iqmGpu.bone_buffer[i], NULL );
			if ( vk.iqmGpu.bone_memory[i] != VK_NULL_HANDLE ) {
				qvkFreeMemory( vk.device, vk.iqmGpu.bone_memory[i], NULL );
			}
			vk.iqmGpu.bone_buffer[i] = VK_NULL_HANDLE;
			vk.iqmGpu.bone_memory[i] = VK_NULL_HANDLE;
			vk.iqmGpu.bone_ptr[i] = NULL;
		}
	}

	if ( vk.iqmGpu.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.iqmGpu.pipeline_layout, NULL );
		vk.iqmGpu.pipeline_layout = VK_NULL_HANDLE;
	}

	if ( vk.iqmGpu.set_layout_bones != VK_NULL_HANDLE ) {
		if ( vk.iqmGpu.ral_bgl_bones ) { Ral_DestroyBindGroupLayout( vk.iqmGpu.ral_bgl_bones ); vk.iqmGpu.ral_bgl_bones = NULL; }
		qvkDestroyDescriptorSetLayout( vk.device, vk.iqmGpu.set_layout_bones, NULL );
		vk.iqmGpu.set_layout_bones = VK_NULL_HANDLE;
	}

	vk.iqmGpu.available = qfalse;
}


/*
===============
vk_create_iqm_vbo — create device-local vertex + index buffers for an IQM model
Uses the staging buffer to upload data.
===============
*/
qboolean vk_create_iqm_vbo( VkBuffer *outVertBuf, VkDeviceMemory *outVertMem,
	VkBuffer *outIdxBuf, VkDeviceMemory *outIdxMem,
	const byte *vertData, int vertSize,
	const byte *idxData, int idxSize )
{
	VkBufferCreateInfo bufDesc;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkCommandBuffer cmdBuf;
	VkBufferCopy copyRegion;

	if ( !vk.iqmGpu.available )
		return qfalse;

	// check that the staging buffer can hold the data
	// NOLINTNEXTLINE(bugprone-misplaced-widening-cast) — small int sum widened to VkDeviceSize for the comparison; both inputs are bounded, no precision loss
	if ( (VkDeviceSize)(vertSize + idxSize) > vk.staging_buffer.size ) {
		ri.Log( SEV_WARN, "vk_create_iqm_vbo: data too large for staging buffer (%d > %d)\n",
			vertSize + idxSize, (int)vk.staging_buffer.size );
		return qfalse;
	}

	// create device-local vertex buffer
	memset( &bufDesc, 0, sizeof( bufDesc ) );
	bufDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufDesc.size = vertSize;
	bufDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bufDesc, NULL, outVertBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outVertBuf, &memReqs );
	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, outVertMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outVertBuf, *outVertMem, 0 ) );
	vk_ral_register_buffer( *outVertBuf, (uint64_t)vertSize,
	                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vk.iqm.vert" );

	// create device-local index buffer
	bufDesc.size = idxSize;
	bufDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bufDesc, NULL, outIdxBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outIdxBuf, &memReqs );
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, outIdxMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outIdxBuf, *outIdxMem, 0 ) );
	vk_ral_register_buffer( *outIdxBuf, (uint64_t)idxSize,
	                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vk.iqm.idx" );

	// upload vertex data via staging buffer
	// Phase 7.4c-submit-followup-IQM — migrated from inline raw-Vk one-shot
	// pattern (cmdAlloc + qvkAllocateCommandBuffers + qvkCreateFence + Begin +
	// Submit + WaitForFences + DestroyFence + FreeCommandBuffers) to BC-B's
	// Ral_AcquireBegunCommandBuffer(GRAPHICS) + Ral_SubmitAndDispose pair. BC-B's
	// grep missed this site because it bypassed the legacy one-shot helpers
	// and inlined the same lifecycle directly. RAL_QUEUE_GRAPHICS matches
	// the legacy vk.queue submit semantics exactly.
	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping IQM VBO+IDX upload\n", __func__ );
			return qfalse;
		}
		cmdBuf = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );

		// copy vertex data to staging, then staging to device
		memcpy( vk.staging_buffer.ptr, vertData, vertSize );
		memset( &copyRegion, 0, sizeof( copyRegion ) );
		copyRegion.size = vertSize;
		qvkCmdCopyBuffer( cmdBuf, vk.staging_buffer.handle, *outVertBuf, 1, &copyRegion );

		// copy index data to staging, then staging to device
		memcpy( vk.staging_buffer.ptr + vertSize, idxData, idxSize );
		copyRegion.srcOffset = vertSize;
		copyRegion.dstOffset = 0;
		copyRegion.size = idxSize;
		qvkCmdCopyBuffer( cmdBuf, vk.staging_buffer.handle, *outIdxBuf, 1, &copyRegion );

		Ral_SubmitAndDispose( rcmd );
	}

	return qtrue;
}


/*
===============
vk_destroy_iqm_vbo — release per-model VBO resources
===============
*/
void vk_destroy_iqm_vbo( VkBuffer *vertBuf, VkDeviceMemory *vertMem,
	VkBuffer *idxBuf, VkDeviceMemory *idxMem )
{
	if ( *vertBuf != VK_NULL_HANDLE ) {
		vk_ral_unregister_buffer( *vertBuf );
		qvkDestroyBuffer( vk.device, *vertBuf, NULL );
		*vertBuf = VK_NULL_HANDLE;
	}
	if ( *vertMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *vertMem, NULL );
		*vertMem = VK_NULL_HANDLE;
	}
	if ( *idxBuf != VK_NULL_HANDLE ) {
		vk_ral_unregister_buffer( *idxBuf );
		qvkDestroyBuffer( vk.device, *idxBuf, NULL );
		*idxBuf = VK_NULL_HANDLE;
	}
	if ( *idxMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *idxMem, NULL );
		*idxMem = VK_NULL_HANDLE;
	}
}


/*
===============
vk_draw_iqm_gpu — issue a GPU-skinned IQM draw call

Binds the IQM pipeline, uploads bone matrices, binds the per-model VBO,
binds the texture descriptor, and issues a drawIndexed call.
Must be called during an active render pass.
===============
*/
void vk_draw_iqm_gpu( VkBuffer vertBuffer, VkBuffer idxBuffer,
	int firstIndex, int numIndexes,
	const float *boneMats, int numBones,
	VkDescriptorSet textureDescriptor,
	const float *mvp )
{
	VkCommandBuffer cmd;
	int frameIdx;
	VkDeviceSize vertOffset;
	VkViewport viewport;
	VkRect2D scissor;

	if ( !vk.iqmGpu.available )
		return;

	if ( vk.geometry_buffer_size_new )
		return; // geometry buffer overflow this frame

	cmd = vk.cmd->command_buffer;
	frameIdx = vk.cmd_index;

	// upload bone matrices to per-frame UBO
	// each bone is 3 * vec4 = 48 bytes
	memcpy( vk.iqmGpu.bone_ptr[frameIdx], boneMats, numBones * 3 * sizeof( vec4_t ) );

	// bind IQM skinning pipeline
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.iqmGpu.pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline (IQM).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.iqmGpu.pipeline ));

	// push MVP matrix
	qvkCmdPushConstants( cmd, vk.iqmGpu.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp );
	// Phase 7.4c-cmd — parallel-paths push-constants (IQM MVP).
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.iqmGpu.pipeline_layout),
		VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp );

	// bind descriptor set 0: bone matrices
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.iqmGpu.pipeline_layout, 0, 1,
		&vk.iqmGpu.bone_descriptor[frameIdx], 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record (bones set 0).
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.iqmGpu.pipeline_layout, 0, 1,
		&vk.iqmGpu.bone_descriptor[frameIdx], 0, NULL );

	// bind descriptor set 1: texture
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.iqmGpu.pipeline_layout, 1, 1,
		&textureDescriptor, 0, NULL );
	// Phase 7.4c-bindgroup — TODO_7.4c-cmd: `textureDescriptor` is a per-shader-type
	// rotating set passed in by the caller (vk_alloc_descriptor_set / shader
	// stage descriptor pool); not in the boot-time adoption registry. The
	// parallel helper bails on unadopted lookup until 7.4c-cmd introduces
	// per-frame rotating-set adoption tied to the cmd-buffer ring.
	vk_ral_parallel_bind_descriptor_sets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.iqmGpu.pipeline_layout, 1, 1,
		&textureDescriptor, 0, NULL );

	// bind vertex buffer
	vertOffset = 0;
	qvkCmdBindVertexBuffers( cmd, 0, 1, &vertBuffer, &vertOffset );
	// Phase 7.4c-submit-A2 — typed parallel-paths bind-vertex-buffer (IQM).
	{ ralBuffer_t *r = vk_ral_lookup_buffer( vertBuffer );
	  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, 0, 1, &r, (const uint64_t *)&vertOffset ); }

	// bind index buffer
	qvkCmdBindIndexBuffer( cmd, idxBuffer, 0, VK_INDEX_TYPE_UINT32 );
	// Phase 7.4c-cmd — parallel-paths bind-index-buffer (IQM).
	Ral_CmdBindIndexBuffer(vk.cmd->ral_cmd, vk_ral_lookup_buffer(idxBuffer), 0, RAL_INDEX_UINT32);

	// set viewport and scissor
	memset( &viewport, 0, sizeof( viewport ) );
	viewport.x = (float)backEnd.viewParms.viewportX;
	viewport.y = (float)backEnd.viewParms.viewportY;
	viewport.width = (float)backEnd.viewParms.viewportWidth;
	viewport.height = (float)backEnd.viewParms.viewportHeight;
	viewport.maxDepth = 1.0f;
	qvkCmdSetViewport( cmd, 0, 1, &viewport );
	// Phase 7.4c-cmd — parallel-paths viewport (IQM).
	Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);

	memset( &scissor, 0, sizeof( scissor ) );
	scissor.offset.x = backEnd.viewParms.viewportX;
	scissor.offset.y = backEnd.viewParms.viewportY;
	scissor.extent.width = backEnd.viewParms.viewportWidth;
	scissor.extent.height = backEnd.viewParms.viewportHeight;
	qvkCmdSetScissor( cmd, 0, 1, &scissor );
	// Phase 7.4c-cmd — parallel-paths scissor (IQM).
	Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);

	// draw
	qvkCmdDrawIndexed( cmd, numIndexes, 1, firstIndex, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths draw-indexed (IQM).
	Ral_CmdDrawIndexed( vk.cmd->ral_cmd, numIndexes, 1, firstIndex, 0, 0 );

	// invalidate pipeline and descriptor state so the next standard draw
	// rebinds correctly (we bound a different pipeline layout)
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT; // force viewport/scissor update
	memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
}
#endif // FEAT_IQM


#ifdef USE_VBO
void vk_release_vbo( void )
{
	if ( vk.vbo.vertex_buffer ) {
		vk_ral_unregister_buffer( vk.vbo.vertex_buffer );
		qvkDestroyBuffer( vk.device, vk.vbo.vertex_buffer, NULL );
	}
	vk.vbo.vertex_buffer = VK_NULL_HANDLE;

	if ( vk.vbo.buffer_memory )
		qvkFreeMemory( vk.device, vk.vbo.buffer_memory, NULL );
	vk.vbo.buffer_memory = VK_NULL_HANDLE;
}


qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size )
{
	VkMemoryRequirements vb_mem_reqs;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize allocationSize;
	uint32_t memory_type_bits;
	VkCommandBuffer command_buffer;
	VkBufferCopy copyRegion[1];
	VkDeviceSize uploadDone;

	vk_release_vbo();

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	// device-local buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.vbo.vertex_buffer ) );

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory ) );
	qvkBindBufferMemory( vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset );
	vk_ral_register_buffer( vk.vbo.vertex_buffer, vbo_size,
	                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vk.vbo.vertex_buffer" );

	// staging buffers

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	// utilize existing staging buffer
	uploadDone = 0;
	while ( uploadDone < vbo_size ) {
		VkDeviceSize uploadSize = vk.staging_buffer.size;
		if ( uploadDone + uploadSize > vbo_size ) {
			uploadSize = vbo_size - uploadDone;
		}
		memcpy(vk.staging_buffer.ptr + 0, vbo_data + uploadDone, uploadSize);
		{
			ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
			if ( rcmd == NULL ) {
				ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping VBO upload chunk (offset=%llu, size=%llu)\n",
				        __func__, (unsigned long long)uploadDone, (unsigned long long)uploadSize );
			} else {
				command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
				copyRegion[0].srcOffset = 0;
				copyRegion[0].dstOffset = uploadDone;
				copyRegion[0].size = uploadSize;
				qvkCmdCopyBuffer( command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, &copyRegion[0] );
				Ral_SubmitAndDispose( rcmd );
			}
		}
		uploadDone += uploadSize;
	}

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

static void vk_create_shader_modules( void )
{
	int i, j, k, l;

	// Phase 7.4c-pipeline: reset the (VkShaderModule → SPV bytes) side table
	// for the new vid_init / vid_restart cycle. Every SHADER_MODULE call
	// below re-registers a fresh handle.
	vk_shader_blob_reset();

	vk.modules.vert.gen[0][0][0][0] = SHADER_MODULE( vert_tx0 );
	vk.modules.vert.gen[0][0][0][1] = SHADER_MODULE( vert_tx0_fog );
	vk.modules.vert.gen[0][0][1][0] = SHADER_MODULE( vert_tx0_env );
	vk.modules.vert.gen[0][0][1][1] = SHADER_MODULE( vert_tx0_env_fog );

	vk.modules.vert.gen[1][0][0][0] = SHADER_MODULE( vert_tx1 );
	vk.modules.vert.gen[1][0][0][1] = SHADER_MODULE( vert_tx1_fog );
	vk.modules.vert.gen[1][0][1][0] = SHADER_MODULE( vert_tx1_env );
	vk.modules.vert.gen[1][0][1][1] = SHADER_MODULE( vert_tx1_env_fog );

	vk.modules.vert.gen[1][1][0][0] = SHADER_MODULE( vert_tx1_cl );
	vk.modules.vert.gen[1][1][0][1] = SHADER_MODULE( vert_tx1_cl_fog );
	vk.modules.vert.gen[1][1][1][0] = SHADER_MODULE( vert_tx1_cl_env );
	vk.modules.vert.gen[1][1][1][1] = SHADER_MODULE( vert_tx1_cl_env_fog );

	vk.modules.vert.gen[2][0][0][0] = SHADER_MODULE( vert_tx2 );
	vk.modules.vert.gen[2][0][0][1] = SHADER_MODULE( vert_tx2_fog );
	vk.modules.vert.gen[2][0][1][0] = SHADER_MODULE( vert_tx2_env );
	vk.modules.vert.gen[2][0][1][1] = SHADER_MODULE( vert_tx2_env_fog );

	vk.modules.vert.gen[2][1][0][0] = SHADER_MODULE( vert_tx2_cl );
	vk.modules.vert.gen[2][1][0][1] = SHADER_MODULE( vert_tx2_cl_fog );
	vk.modules.vert.gen[2][1][1][0] = SHADER_MODULE( vert_tx2_cl_env );
	vk.modules.vert.gen[2][1][1][1] = SHADER_MODULE( vert_tx2_cl_env_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l] );
					SET_OBJECT_NAME( vk.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}

	// specialized depth-fragment shader
	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	// fixed-color (1.0) shader modules
	vk.modules.vert.ident1[0][0][0] = SHADER_MODULE( vert_tx0_ident1 );
	vk.modules.vert.ident1[0][0][1] = SHADER_MODULE( vert_tx0_ident1_fog );
	vk.modules.vert.ident1[0][1][0] = SHADER_MODULE( vert_tx0_ident1_env );
	vk.modules.vert.ident1[0][1][1] = SHADER_MODULE( vert_tx0_ident1_env_fog );
	vk.modules.vert.ident1[1][0][0] = SHADER_MODULE( vert_tx1_ident1 );
	vk.modules.vert.ident1[1][0][1] = SHADER_MODULE( vert_tx1_ident1_fog );
	vk.modules.vert.ident1[1][1][0] = SHADER_MODULE( vert_tx1_ident1_env );
	vk.modules.vert.ident1[1][1][1] = SHADER_MODULE( vert_tx1_ident1_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ident1[0][0] = SHADER_MODULE( frag_tx0_ident1 );
	vk.modules.frag.ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_fog );
	vk.modules.frag.ident1[1][0] = SHADER_MODULE( frag_tx1_ident1 );
	vk.modules.frag.ident1[1][1] = SHADER_MODULE( frag_tx1_ident1_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture identity%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ident1[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.vert.fixed[0][0][0] = SHADER_MODULE( vert_tx0_fixed );
	vk.modules.vert.fixed[0][0][1] = SHADER_MODULE( vert_tx0_fixed_fog );
	vk.modules.vert.fixed[0][1][0] = SHADER_MODULE( vert_tx0_fixed_env );
	vk.modules.vert.fixed[0][1][1] = SHADER_MODULE( vert_tx0_fixed_env_fog );
	vk.modules.vert.fixed[1][0][0] = SHADER_MODULE( vert_tx1_fixed );
	vk.modules.vert.fixed[1][0][1] = SHADER_MODULE( vert_tx1_fixed_fog );
	vk.modules.vert.fixed[1][1][0] = SHADER_MODULE( vert_tx1_fixed_env );
	vk.modules.vert.fixed[1][1][1] = SHADER_MODULE( vert_tx1_fixed_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.fixed[0][0] = SHADER_MODULE( frag_tx0_fixed );
	vk.modules.frag.fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_fog );
	vk.modules.frag.fixed[1][0] = SHADER_MODULE( frag_tx1_fixed );
	vk.modules.frag.fixed[1][1] = SHADER_MODULE( frag_tx1_fixed_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture fixed-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.fixed[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.ent[0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	//vk.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	//vk.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );
	for ( i = 0; i < 1; i++ ) {
		const char *tx[] = { "single" /*, "double" */};
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture entity-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ent[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.gen[0][0][0] = SHADER_MODULE( frag_tx0 );
	vk.modules.frag.gen[0][0][1] = SHADER_MODULE( frag_tx0_fog );

	vk.modules.frag.gen[1][0][0] = SHADER_MODULE( frag_tx1 );
	vk.modules.frag.gen[1][0][1] = SHADER_MODULE( frag_tx1_fog );

	vk.modules.frag.gen[1][1][0] = SHADER_MODULE( frag_tx1_cl );
	vk.modules.frag.gen[1][1][1] = SHADER_MODULE( frag_tx1_cl_fog );

	vk.modules.frag.gen[2][0][0] = SHADER_MODULE( frag_tx2 );
	vk.modules.frag.gen[2][0][1] = SHADER_MODULE( frag_tx2_fog );

	vk.modules.frag.gen[2][1][0] = SHADER_MODULE( frag_tx2_cl );
	vk.modules.frag.gen[2][1][1] = SHADER_MODULE( frag_tx2_cl_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture%s%s fragment module", tx[i], cl[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.frag.gen[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}


	// depth fade fragment shader variants (single-texture only)
	if ( vk.depthFade.active ) {
		vk.modules.frag.dfade_gen[0][0] = SHADER_MODULE( frag_tx0_dfade );
		vk.modules.frag.dfade_gen[0][1] = SHADER_MODULE( frag_tx0_dfade_fog );
		vk.modules.frag.dfade_ident1[0][0] = SHADER_MODULE( frag_tx0_ident1_dfade );
		vk.modules.frag.dfade_ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_dfade_fog );
		vk.modules.frag.dfade_fixed[0][0] = SHADER_MODULE( frag_tx0_fixed_dfade );
		vk.modules.frag.dfade_fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_dfade_fog );
		vk.modules.frag.dfade_ent[0][0] = SHADER_MODULE( frag_tx0_ent_dfade );
		vk.modules.frag.dfade_ent[0][1] = SHADER_MODULE( frag_tx0_ent_dfade_fog );
	}

	// SMAA shader modules. Loaded unconditionally: vk.smaa.active is the
	// "intermediate images allocated" flag, owned by vk_smaa_alloc_resources
	// / vk_smaa_release_resources, and is still qfalse here — vk_create_attachments
	// (which calls vk_smaa_alloc_resources, flipping it to qtrue) runs *after*
	// this function. Gating module creation on vk.smaa.active left the handles
	// VK_NULL_HANDLE while vk_update_post_process_pipelines still saw .active and
	// fed them to vk_create_smaa_pipelines (VUID-VkGraphicsPipelineCreateInfo
	// triple-NULL at boot when r_smaa > 0). r_smaa is also live-toggleable
	// (CVG_RENDERER), so the modules must exist even if SMAA is off at boot. The
	// six SMAA shaders are tiny — always-present is cheaper than re-introducing an
	// ordering dependency. Destroyed unconditionally (NULL-guarded) in vk_shutdown.
	vk.modules.smaa_edge_vs    = SHADER_MODULE( smaa_edge_vert_spv );
	vk.modules.smaa_edge_fs    = SHADER_MODULE( smaa_edge_frag_spv );
	vk.modules.smaa_blend_vs   = SHADER_MODULE( smaa_blend_vert_spv );
	vk.modules.smaa_blend_fs   = SHADER_MODULE( smaa_blend_frag_spv );
	vk.modules.smaa_resolve_vs = SHADER_MODULE( smaa_resolve_vert_spv );
	vk.modules.smaa_resolve_fs = SHADER_MODULE( smaa_resolve_frag_spv );

	vk.modules.vert.light[0] = SHADER_MODULE( vert_light );
	vk.modules.vert.light[1] = SHADER_MODULE( vert_light_fog );
	SET_OBJECT_NAME( vk.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.light[0][0] = SHADER_MODULE( frag_light );
	vk.modules.frag.light[0][1] = SHADER_MODULE( frag_light_fog );
	vk.modules.frag.light[1][0] = SHADER_MODULE( frag_light_line );
	vk.modules.frag.light[1][1] = SHADER_MODULE( frag_light_line_fog );
	SET_OBJECT_NAME( vk.modules.frag.light[0][0], "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[0][1], "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][0], "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][1], "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#if FEAT_PARALLAX_MAPPING
	vk.modules.vert.light_parallax[0] = SHADER_MODULE( vert_light_parallax );
	vk.modules.vert.light_parallax[1] = SHADER_MODULE( vert_light_parallax_fog );
	vk.modules.frag.light_parallax[0][0] = SHADER_MODULE( frag_light_parallax );
	vk.modules.frag.light_parallax[0][1] = SHADER_MODULE( frag_light_parallax_fog );
	vk.modules.frag.light_parallax[1][0] = SHADER_MODULE( frag_light_parallax_line );
	vk.modules.frag.light_parallax[1][1] = SHADER_MODULE( frag_light_parallax_line_fog );
#endif

	vk.modules.color_fs = SHADER_MODULE( color_frag_spv );
	vk.modules.color_vs = SHADER_MODULE( color_vert_spv );

	SET_OBJECT_NAME( vk.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.fog_vs = SHADER_MODULE( fog_vert_spv );
	vk.modules.fog_fs = SHADER_MODULE( fog_frag_spv );

	SET_OBJECT_NAME( vk.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.dot_vs = SHADER_MODULE( dot_vert_spv );
	vk.modules.dot_fs = SHADER_MODULE( dot_frag_spv );

	SET_OBJECT_NAME( vk.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.bloom_fs = SHADER_MODULE( bloom_frag_spv );
	vk.modules.blur_fs = SHADER_MODULE( blur_frag_spv );
	vk.modules.blend_fs = SHADER_MODULE( blend_frag_spv );

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#if FEAT_ADVANCED_WATER
	vk.modules.water_fs = SHADER_MODULE( water_frag_spv );
#endif

#if FEAT_SHADOW_MAPPING
	vk.modules.shadow_depth_vs = SHADER_MODULE( shadow_depth_vert_spv );
	vk.modules.shadow_depth_fs = SHADER_MODULE( shadow_depth_frag_spv );
	vk.modules.light_shadow[0] = SHADER_MODULE( vert_light_shadow );
	vk.modules.light_shadow[1] = SHADER_MODULE( vert_light_shadow_fog );
	vk.modules.light_shadow_frag[0][0] = SHADER_MODULE( frag_light_shadow );
	vk.modules.light_shadow_frag[0][1] = SHADER_MODULE( frag_light_shadow_fog );
	vk.modules.light_shadow_frag[1][0] = SHADER_MODULE( frag_light_shadow_line );
	vk.modules.light_shadow_frag[1][1] = SHADER_MODULE( frag_light_shadow_line_fog );
#endif

#if FEAT_PBR
	vk.modules.light_pbr_frag[0][0] = SHADER_MODULE( frag_light_pbr );
	vk.modules.light_pbr_frag[0][1] = SHADER_MODULE( frag_light_pbr_fog );
	vk.modules.light_pbr_frag[1][0] = SHADER_MODULE( frag_light_pbr_line );
	vk.modules.light_pbr_frag[1][1] = SHADER_MODULE( frag_light_pbr_line_fog );
#endif

	vk.modules.gamma_fs   = SHADER_MODULE( gamma_frag_spv );
	vk.modules.gamma_vs   = SHADER_MODULE( gamma_vert_spv );
	vk.modules.tonemap_fs = SHADER_MODULE( tonemap_frag_spv );

	// Phase 6B3'-c1: variant shaders moved from gamma.frag to tonemap.frag.
	// Bit layout (TONEMAP_VAR_*) preserved; shader source/output names changed.
	memset( vk.tonemap_variant_fs, 0, sizeof( vk.tonemap_variant_fs ) );
#if FEAT_SSAO
	vk.tonemap_variant_fs[ TONEMAP_VAR_SSAO ] = SHADER_MODULE( tonemap_ssao_frag_spv );
#endif
#if FEAT_TONEMAP
	vk.tonemap_variant_fs[ TONEMAP_VAR_BASE ] = SHADER_MODULE( tonemap_tonemap_frag_spv );
#endif
#if FEAT_COLOR_GRADING
	vk.tonemap_variant_fs[ TONEMAP_VAR_CG ] = SHADER_MODULE( tonemap_colorgrade_frag_spv );
#endif
	// Phase 6B3'-e: FXAA variants removed (SMAA replaces it).
#if FEAT_SSAO && FEAT_TONEMAP
	vk.tonemap_variant_fs[ TONEMAP_VAR_SSAO | TONEMAP_VAR_BASE ] = SHADER_MODULE( tonemap_ssao_tonemap_frag_spv );
#endif
#if FEAT_TONEMAP && FEAT_COLOR_GRADING
	vk.tonemap_variant_fs[ TONEMAP_VAR_BASE | TONEMAP_VAR_CG ] = SHADER_MODULE( tonemap_tonemap_cg_frag_spv );
#endif
#if FEAT_SSAO && FEAT_TONEMAP && FEAT_COLOR_GRADING
	vk.tonemap_variant_fs[ TONEMAP_VAR_SSAO | TONEMAP_VAR_BASE | TONEMAP_VAR_CG ] = SHADER_MODULE( tonemap_full_frag_spv );
#endif
#if FEAT_GODRAYS
	vk.tonemap_variant_fs[ TONEMAP_VAR_GODRAYS ] = SHADER_MODULE( tonemap_godrays_frag_spv );
#endif
#if FEAT_GODRAYS && FEAT_SSAO
	vk.tonemap_variant_fs[ TONEMAP_VAR_SSAO | TONEMAP_VAR_GODRAYS ] = SHADER_MODULE( tonemap_ssao_godrays_frag_spv );
#endif
#if FEAT_GODRAYS && FEAT_SSAO && FEAT_TONEMAP
	vk.tonemap_variant_fs[ TONEMAP_VAR_SSAO | TONEMAP_VAR_GODRAYS | TONEMAP_VAR_BASE ] = SHADER_MODULE( tonemap_ssao_godrays_tm_frag_spv );
#endif
#if FEAT_GODRAYS && FEAT_SSAO && FEAT_TONEMAP && FEAT_COLOR_GRADING
	// Phase 6B3'-e: "ultimate" full-stack variant (SSAO + TONEMAP + CG + GODRAYS).
	// Was FXAA + all four pre-6B3'-e; FXAA removal demoted it to 4 features.
	vk.tonemap_variant_fs[ TONEMAP_VAR_SSAO | TONEMAP_VAR_BASE | TONEMAP_VAR_CG | TONEMAP_VAR_GODRAYS ] = SHADER_MODULE( tonemap_ultimate_frag_spv );
#endif

	SET_OBJECT_NAME( vk.modules.gamma_fs,   "gamma post-processing fragment module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs,   "gamma post-processing vertex module",     VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.tonemap_fs, "tonemap post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

#if FEAT_IQM
	vk.modules.iqm_skinning_vs = SHADER_MODULE( iqm_skinning_vert_spv );
	vk.modules.iqm_skinning_fs = SHADER_MODULE( iqm_skinning_frag_spv );
	SET_OBJECT_NAME( vk.modules.iqm_skinning_vs, "IQM skinning vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.iqm_skinning_fs, "IQM skinning fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
#endif

	vk.modules.msdf_vs = SHADER_MODULE( msdf_vert_spv );
	vk.modules.msdf_fs = SHADER_MODULE( msdf_frag_spv );
	SET_OBJECT_NAME( vk.modules.msdf_vs, "MSDF text vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.msdf_fs, "MSDF text fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.ribbon_vs = SHADER_MODULE( ribbon_vert_spv );
	vk.modules.ribbon_fs = SHADER_MODULE( ribbon_frag_spv );
	SET_OBJECT_NAME( vk.modules.ribbon_vs, "ribbon vertex module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.ribbon_fs, "ribbon fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.sprite_vs = SHADER_MODULE( sprite_vert_spv );
	vk.modules.sprite_fs = SHADER_MODULE( sprite_frag_spv );
	SET_OBJECT_NAME( vk.modules.sprite_vs, "sprite vertex module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.sprite_fs, "sprite fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.beam_vs = SHADER_MODULE( beam_vert_spv );
	vk.modules.beam_fs = SHADER_MODULE( beam_frag_spv );
	SET_OBJECT_NAME( vk.modules.beam_vs, "beam vertex module",   VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.beam_fs, "beam fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.particle_integrate_cs = SHADER_MODULE( particle_integrate_comp_spv );
	vk.modules.particle_vs           = SHADER_MODULE( particle_vert_spv );
	vk.modules.particle_fs           = SHADER_MODULE( particle_frag_spv );
	SET_OBJECT_NAME( vk.modules.particle_integrate_cs, "particle integrate compute module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.particle_vs,           "particle vertex module",            VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.particle_fs,           "particle fragment module",          VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.q1_ls_vs       = SHADER_MODULE( q1_ls_vert_spv );
	vk.modules.q1_ls_fs       = SHADER_MODULE( q1_ls_frag_spv );
	vk.modules.q1_ls_array_fs = SHADER_MODULE( q1_ls_array_frag_spv );
	SET_OBJECT_NAME( vk.modules.q1_ls_vs,       "lightstyle vertex module",         VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.q1_ls_fs,       "lightstyle fragment module",       VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.q1_ls_array_fs, "lightstyle array fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
}


static void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// skybox
	{
		memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SINGLE_TEXTURE_FIXED_COLOR;
		/* Phase 6B3'-a: identityLightByte → 255 (linear). */
		def.color.rgb = 255;
		def.color.alpha = 255;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		//def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test dot
	if ( vk.fragmentStores )
	{
		memset( &def, 0, sizeof( def ) );
		//def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// MSDF text rendering pipeline
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_MSDF;
		vk.msdf_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// Q1 4-style lightmap blend pipeline (animChain lerp via set=6)
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_FRONT_SIDED;
		def.shader_type = TYPE_LIGHTSTYLES;
		vk.q1ls_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// Q1 4-style lightmap blend pipeline (GPU texture array animation)
	{
		memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_FRONT_SIDED;
		def.shader_type = TYPE_LIGHTSTYLES_ARRAY;
		vk.q1ls_array_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass );
static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry);

static void vk_create_smaa_pipelines( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;

	// Quality presets: Low(1), Medium(2), High(3), Ultra(4). Threshold
	// values come from the Jorge Jimenez et al. SMAA paper — kept as
	// authored. Phase 6B4 made smaa_edge.frag's threshold check use
	// relative luma delta, so these values are now HDR-magnitude-
	// independent (same edge sensitivity at r_hdr 0 / 1 / 2).
	static const float thresholds[] = { 0.0f, 0.15f, 0.10f, 0.10f, 0.05f };
	static const int searchSteps[] = { 0, 4, 8, 16, 32 };
	static const int searchStepsDiag[] = { 0, 0, 0, 8, 16 };
	static const int cornerRounding[] = { 0, 0, 0, 25, 25 };

	// Phase 6B4: r_smaa is live (CVG_RENDERER), so re-read it at every
	// rebuild instead of reusing the init-time cache. vk.smaa.quality
	// stays in sync for the SEV_INFO log path elsewhere.
	int q = r_smaa->integer;
	vk.smaa.quality = q;

	// specialization constants for edge detection fragment shader
	VkSpecializationMapEntry edge_spec_entry;
	VkSpecializationInfo edge_spec_info;
	float edge_threshold;

	// specialization constants for blend weight calculation
	VkSpecializationMapEntry blend_vert_spec_entry;
	VkSpecializationInfo blend_vert_spec_info;
	int blend_vert_max_search;

	struct SmaaBlendFragSpec {
		int max_search_steps;
		int max_search_steps_diag;
		int corner_rounding;
	} blend_frag_spec_data;
	VkSpecializationMapEntry blend_frag_spec_entries[3];
	VkSpecializationInfo blend_frag_spec_info;

	if ( !vk.smaa.active )
		return;

	// Phase 6B4: r_smaa toggled off live. Tear down any live SMAA
	// pipelines so the dispatch gate in vk_end_frame (which checks
	// vk.smaa_edge_pipeline != VK_NULL_HANDLE alongside r_smaa) is
	// consistent. Resources stay allocated per the always-alloc
	// design — only pipeline objects come and go with r_smaa.
	if ( q == 0 ) {
		if ( vk.smaa_edge_pipeline != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.smaa_edge_pipeline, NULL );
			vk.smaa_edge_pipeline = VK_NULL_HANDLE;
		}
		if ( vk.smaa_blend_pipeline != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.smaa_blend_pipeline, NULL );
			vk.smaa_blend_pipeline = VK_NULL_HANDLE;
		}
		if ( vk.smaa_resolve_pipeline != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.smaa_resolve_pipeline, NULL );
			vk.smaa_resolve_pipeline = VK_NULL_HANDLE;
		}
		// Phase 7.4c-pipeline-followup-5 PART 2.6-v2 — sibling RAL pipeline
		// teardown on the r_smaa=0 bail. Mirrors the legacy destroys above.
		// Without this, an SMAA→off→on cvar cycle orphans the RAL siblings
		// (their handles linger on vk.device past Ral_DestroyBackend at
		// process exit → VUID-vkDestroyDevice-device-05137).
		if ( vk.ral_smaa_edge_pipeline ) {
			Ral_DestroyPipeline( vk.ral_smaa_edge_pipeline );
			vk.ral_smaa_edge_pipeline = NULL;
		}
		if ( vk.ral_smaa_blend_pipeline ) {
			Ral_DestroyPipeline( vk.ral_smaa_blend_pipeline );
			vk.ral_smaa_blend_pipeline = NULL;
		}
		if ( vk.ral_smaa_resolve_pipeline ) {
			Ral_DestroyPipeline( vk.ral_smaa_resolve_pipeline );
			vk.ral_smaa_resolve_pipeline = NULL;
		}
		return;
	}

	// shared pipeline state
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexAttributeDescriptions = NULL;

	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)glConfig.vidWidth;
	viewport.height = (float)glConfig.vidHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = glConfig.vidWidth;
	scissor.extent.height = glConfig.vidHeight;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment_blend_state.blendEnable = VK_FALSE;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.stencilTestEnable = VK_FALSE;

	memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_smaa;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	// --- Edge detection pipeline ---
	// Phase 6B4: r_smaa_threshold > 0 overrides the quality preset.
	// Default value 0 means "use r_smaa preset" so existing user
	// configs continue to behave as before. The override only swaps
	// the threshold constant; searchSteps / searchStepsDiag /
	// cornerRounding stay preset-keyed (they're quality knobs, not
	// sensitivity knobs).
	if ( r_smaa_threshold->value > 0.0f ) {
		edge_threshold = r_smaa_threshold->value;
	} else {
		edge_threshold = thresholds[q];
	}
	edge_spec_entry.constantID = 0;
	edge_spec_entry.offset = 0;
	edge_spec_entry.size = sizeof( float );
	edge_spec_info.mapEntryCount = 1;
	edge_spec_info.pMapEntries = &edge_spec_entry;
	edge_spec_info.dataSize = sizeof( float );
	edge_spec_info.pData = &edge_threshold;

	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.smaa_edge_vs, "main" );
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.smaa_edge_fs, "main" );
	shader_stages[1].pSpecializationInfo = &edge_spec_info;

	create_info.renderPass = vk.render_pass.smaa_edge;

	if ( vk.smaa_edge_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_edge_pipeline, NULL );
		vk.smaa_edge_pipeline = VK_NULL_HANDLE;
	}
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.smaa_edge_pipeline ) );
	SET_OBJECT_NAME( vk.smaa_edge_pipeline, "SMAA edge detection pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// Phase 7.4c-pipeline-followup-3 — SMAA cluster (shared pipeline_layout_smaa).
	// Binding contract per smaa_edge.{vert,frag}: push_constant `vec4 rtMetrics` =
	// 16 bytes used in BOTH vertex + fragment stages. Set 0 binding 0 = sampler2D
	// colorTex. The shared pipeline_layout_smaa declares 3 sampler sets to cover
	// the larger blend/resolve variants; declaring extra sets is allowed (shader
	// only consumes set 0 here).
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		vk_ral_special_pipeline_params_t p;
		memset( &p, 0, sizeof( p ) );
		p.vs_module          = vk.modules.smaa_edge_vs;
		p.fs_module          = vk.modules.smaa_edge_fs;
		p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
		p.cullMode           = RAL_CULL_NONE;
		p.depthTestEnable    = qfalse;
		p.depthWriteEnable   = qfalse;
		p.blendEnable        = qfalse;
		p.pushConstantSize   = 16;
		p.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
		if ( vk.ral_bgl_sampler ) {
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
		}
		p.debugName          = "ral-smaa-edge";
		p.externalLayout     = vk.ral_pipeline_layout_smaa;     // Phase 7.4c-submit-A3
		// Phase 7.4c-pipeline-followup-5 PART 2.6-v2 — destroy-then-create
		// guard. vk_create_smaa_pipelines is called multiple times per
		// session (r_smaa toggle, framebuffer resize, map transition with
		// SMAA active rebuild); without this guard the predecessor RAL
		// pipeline is orphaned on rebuild and trips
		// VUID-vkDestroyDevice-device-05137 at process exit.
		if ( vk.ral_smaa_edge_pipeline ) {
			Ral_DestroyPipeline( vk.ral_smaa_edge_pipeline );
			vk.ral_smaa_edge_pipeline = NULL;
		}
		p.externalRenderPass = vk.ral_render_pass.smaa_edge;   // Phase 7.4c-submit-A3
		vk.ral_smaa_edge_pipeline = vk_ral_create_special_pipeline( &p );
	}

	// --- Blend weight calculation pipeline ---
	blend_vert_max_search = searchSteps[q];
	blend_vert_spec_entry.constantID = 0;
	blend_vert_spec_entry.offset = 0;
	blend_vert_spec_entry.size = sizeof( int );
	blend_vert_spec_info.mapEntryCount = 1;
	blend_vert_spec_info.pMapEntries = &blend_vert_spec_entry;
	blend_vert_spec_info.dataSize = sizeof( int );
	blend_vert_spec_info.pData = &blend_vert_max_search;

	blend_frag_spec_data.max_search_steps = searchSteps[q];
	blend_frag_spec_data.max_search_steps_diag = searchStepsDiag[q];
	blend_frag_spec_data.corner_rounding = cornerRounding[q];

	blend_frag_spec_entries[0].constantID = 0;
	blend_frag_spec_entries[0].offset = offsetof( struct SmaaBlendFragSpec, max_search_steps );
	blend_frag_spec_entries[0].size = sizeof( int );
	blend_frag_spec_entries[1].constantID = 1;
	blend_frag_spec_entries[1].offset = offsetof( struct SmaaBlendFragSpec, max_search_steps_diag );
	blend_frag_spec_entries[1].size = sizeof( int );
	blend_frag_spec_entries[2].constantID = 2;
	blend_frag_spec_entries[2].offset = offsetof( struct SmaaBlendFragSpec, corner_rounding );
	blend_frag_spec_entries[2].size = sizeof( int );

	blend_frag_spec_info.mapEntryCount = 3;
	blend_frag_spec_info.pMapEntries = blend_frag_spec_entries;
	blend_frag_spec_info.dataSize = sizeof( blend_frag_spec_data );
	blend_frag_spec_info.pData = &blend_frag_spec_data;

	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.smaa_blend_vs, "main" );
	shader_stages[0].pSpecializationInfo = &blend_vert_spec_info;
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.smaa_blend_fs, "main" );
	shader_stages[1].pSpecializationInfo = &blend_frag_spec_info;

	create_info.renderPass = vk.render_pass.smaa_blend;

	if ( vk.smaa_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_blend_pipeline, NULL );
		vk.smaa_blend_pipeline = VK_NULL_HANDLE;
	}
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.smaa_blend_pipeline ) );
	SET_OBJECT_NAME( vk.smaa_blend_pipeline, "SMAA blend weight pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// Phase 7.4c-pipeline-followup-3 — SMAA blend (same pipeline_layout_smaa).
	// Contract: 16B push V+F; 3 sampler sets (edges + area LUT + search LUT).
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		vk_ral_special_pipeline_params_t p;
		memset( &p, 0, sizeof( p ) );
		p.vs_module          = vk.modules.smaa_blend_vs;
		p.fs_module          = vk.modules.smaa_blend_fs;
		p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
		p.cullMode           = RAL_CULL_NONE;
		p.depthTestEnable    = qfalse;
		p.depthWriteEnable   = qfalse;
		p.blendEnable        = qfalse;
		p.pushConstantSize   = 16;
		p.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
		if ( vk.ral_bgl_sampler ) {
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
		}
		p.debugName          = "ral-smaa-blend";
		p.externalLayout     = vk.ral_pipeline_layout_smaa;     // Phase 7.4c-submit-A3
		// Phase 7.4c-pipeline-followup-5 PART 2.6-v2 — destroy-then-create
		// guard (see ral-smaa-edge site above).
		if ( vk.ral_smaa_blend_pipeline ) {
			Ral_DestroyPipeline( vk.ral_smaa_blend_pipeline );
			vk.ral_smaa_blend_pipeline = NULL;
		}
		p.externalRenderPass = vk.ral_render_pass.smaa_blend;   // Phase 7.4c-submit-A3
		vk.ral_smaa_blend_pipeline = vk_ral_create_special_pipeline( &p );
	}

	// --- Resolve (neighborhood blending) pipeline ---
	set_shader_stage_desc( shader_stages + 0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.smaa_resolve_vs, "main" );
	shader_stages[0].pSpecializationInfo = NULL;
	set_shader_stage_desc( shader_stages + 1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.smaa_resolve_fs, "main" );
	shader_stages[1].pSpecializationInfo = NULL;

	create_info.renderPass = vk.render_pass.smaa_resolve;

	if ( vk.smaa_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_resolve_pipeline, NULL );
		vk.smaa_resolve_pipeline = VK_NULL_HANDLE;
	}
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.smaa_resolve_pipeline ) );
	SET_OBJECT_NAME( vk.smaa_resolve_pipeline, "SMAA resolve pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// Phase 7.4c-pipeline-followup-3 — SMAA resolve (same pipeline_layout_smaa).
	// Contract: 16B push V+F; 2 sampler sets used (colorTex + blendTex);
	// layout declares 3 (allowed superset).
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		vk_ral_special_pipeline_params_t p;
		memset( &p, 0, sizeof( p ) );
		p.vs_module          = vk.modules.smaa_resolve_vs;
		p.fs_module          = vk.modules.smaa_resolve_fs;
		p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
		p.cullMode           = RAL_CULL_NONE;
		p.depthTestEnable    = qfalse;
		p.depthWriteEnable   = qfalse;
		p.blendEnable        = qfalse;
		p.pushConstantSize   = 16;
		p.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
		if ( vk.ral_bgl_sampler ) {
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
			p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler;
		}
		p.debugName          = "ral-smaa-resolve";
		p.externalLayout     = vk.ral_pipeline_layout_smaa;     // Phase 7.4c-submit-A3
		// Phase 7.4c-pipeline-followup-5 PART 2.6-v2 — destroy-then-create
		// guard (see ral-smaa-edge site above).
		if ( vk.ral_smaa_resolve_pipeline ) {
			Ral_DestroyPipeline( vk.ral_smaa_resolve_pipeline );
			vk.ral_smaa_resolve_pipeline = NULL;
		}
		p.externalRenderPass = vk.ral_render_pass.smaa_resolve;   // Phase 7.4c-submit-A3
		vk.ral_smaa_resolve_pipeline = vk_ral_create_special_pipeline( &p );
	}
}


// Forward declarations for the static helpers vk_rebuild_fbo_for_hdr_change
// and vk_rebuild_for_fbo_change reach across. Definitions live further down
// in this file; vk_initialize and vk_restart_swapchain use the same set.
static void vk_create_render_passes( void );
static void vk_alloc_attachments( void );
static void vk_create_attachments( void );
static void vk_create_framebuffers( void );
static void vk_destroy_framebuffers( void );
static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );
static void setup_surface_formats( VkPhysicalDevice physical_device );
// Phase 6B3'-f: r_fbo live conversion reaches across the swapchain helpers
// too — the FBO toggle changes sRGB swapchain selection and present format.
static void vk_create_sync_primitives( void );
static void vk_destroy_sync_primitives( void );
static void vk_destroy_swapchain( qboolean preserveRal );  // Phase 7.4c-submit-followup-present-2: preserveRal=qtrue preserves vk.ral_swapchain for atomic-handoff recreate; qfalse destroys it (fresh teardown).
// Phase 6B3'-f split-A3: SMAA option (ii) live release / re-alloc.
// Bodies live near vk_create_attachments so all of SMAA's lifecycle
// is together. Called from vk_create_attachments / vk_destroy_attachments
// for cold start + r_fbo flip, and from vk_update_post_process_pipelines
// for r_smaa live toggle.
static void vk_smaa_alloc_resources( void );
static void vk_smaa_release_resources( void );
// Phase 7.4c-pipeline-followup-5 PART 2.7: framebuffers that reference SMAA-
// owned VkImageViews (smaa_edge attaches edges_view, smaa_blend attaches
// blend_view). Their lifecycle MUST track the views — destroy before the
// view destroy in vk_smaa_release_resources, recreate after the view create
// in vk_smaa_alloc_resources. Cold-start vk_create_framebuffers also calls
// the create helper; idempotent NULL-check makes both call orders safe.
// (smaa_resolve framebuffer attaches vk.tonemapped_image_view — a non-SMAA-
// owned view that survives r_smaa toggle — so it stays managed inline by
// vk_create_framebuffers / vk_destroy_framebuffers.)
static void vk_smaa_create_framebuffers ( void );
static void vk_smaa_destroy_framebuffers( void );
// Phase 6.5.4 Part B-refactor: shadow-map resource lifecycle, same shape as
// SMAA's. Bodies live near vk_create_attachments. Called from vk_create_
// attachments / vk_destroy_attachments (cold start + r_fbo / r_hdr flip) and
// from vk_update_post_process_pipelines for the live r_shadowMapping /
// r_shadowMapSize toggle. The shadow sampler descriptor SET is owned by
// vk_init_descriptors (allocated whenever vk.fboActive, like the SMAA SETs),
// not by these helpers.
#if FEAT_SHADOW_MAPPING
static void vk_shadow_alloc_resources( void );
static void vk_shadow_release_resources( void );
// Phase 6.5.4d2-followup: destroys each command-buffer slot's deformed-mesh
// snapshot buffer + frees the shadowMeshSnap* CPU arrays. Defined near
// vk_shadow_capture_mesh (where it can see both the vk.tess[].shadowSnap*
// fields and the static CPU arrays). Called once from vk_shutdown.
static void vk_shutdown_shadow_snap( void );
#endif


/*
================
vk_recreate_main_pass_primitive_pipelines

Phase 6B3'-d1: the ribbon / beam / sprite / particle / iqmGpu primitive
systems each create their own graphics pipeline(s) outside vk.pipelines[]
and the static post-process pipeline list (gamma / tonemap / capture /
bloom / SMAA). Those primitive pipelines bind directly to
vk.render_pass.main (see the gpInfo.renderPass assignments in
vk_init_ribbon, vk_init_beam, vk_init_sprite, vk_init_particle, and
vk_init_iqm_gpu_skinning), so when r_hdr live-flips and
vk_rebuild_fbo_for_hdr_change destroys + recreates the main render pass
with a different colour-attachment format the primitive pipelines retain
their stale render-pass reference and validation fires
VUID-vkCmdDraw-renderPass-02684 on next draw:

	pSubpasses[0].pColorAttachments[0].attachment is incompatible between
	VkRenderPass (current) and VkRenderPass (from VkPipeline).

vk_destroy_pipelines() handles dynamic vk.pipelines[] handles plus the
static post-process pipelines; vk_update_post_process_pipelines() then
rebuilds the post-process side. Nothing touches the standalone primitive
pipelines — this helper fills that gap by destroying + recreating each
one using the same create_info as the source vk_init_<prim>.

MAINTENANCE: each block here mirrors a pipeline block in the
corresponding vk_init_<prim>. If you change a primitive's pipeline
create_info, mirror the change in both places. The init function is the
cold-boot path; this helper is only reached on r_hdr live conversion.
The duplication is accepted as the path-of-least-risk to avoid
refactoring the proven primitive init functions while the descriptor
pool budget is too tight to use a full shutdown + init cycle (~22 SSBO
descriptor sets would leak per toggle against a 64-slot budget; see the
pool sizing block in vk_initialize).
================
*/

static int vk_recreate_ribbon_pipelines( void )
{
	VkGraphicsPipelineCreateInfo gpInfo;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blendAttach;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkDynamicState dynStates[2];
	VkViewport viewport;
	VkRect2D scissor;
	int variant;

	if ( !vk.ribbon.available || vk.ribbon.pipeline_layout == VK_NULL_HANDLE )
		return 0;

	if ( vk.ribbon.pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ribbon.pipeline_alpha, NULL );
		vk.ribbon.pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.ribbon.pipeline_additive, NULL );
		vk.ribbon.pipeline_additive = VK_NULL_HANDLE;
	}
	if ( vk.ribbon.ral_pipeline_alpha    ) { Ral_DestroyPipeline( vk.ribbon.ral_pipeline_alpha    ); vk.ribbon.ral_pipeline_alpha    = NULL; }
	if ( vk.ribbon.ral_pipeline_additive ) { Ral_DestroyPipeline( vk.ribbon.ral_pipeline_additive ); vk.ribbon.ral_pipeline_additive = NULL; }

	memset( stages, 0, sizeof( stages ) );
	stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.ribbon_vs;
	stages[0].pName  = "main";
	stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.ribbon_fs;
	stages[1].pName  = "main";

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates    = dynStates;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.width    = (float)vk.renderWidth;
	viewport.height   = (float)vk.renderHeight;
	viewport.maxDepth = 1.0f;
	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width  = vk.renderWidth;
	scissor.extent.height = vk.renderHeight;

	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports    = &viewport;
	viewportState.scissorCount  = 1;
	viewportState.pScissors     = &scissor;

	memset( &rasterizer, 0, sizeof( rasterizer ) );
	rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth   = 1.0f;
	rasterizer.cullMode    = VK_CULL_MODE_NONE;
	rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

	memset( &multisampling, 0, sizeof( multisampling ) );
	multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable  = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	memset( &colorBlend, 0, sizeof( colorBlend ) );
	colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments    = &blendAttach;

	memset( &gpInfo, 0, sizeof( gpInfo ) );
	gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpInfo.stageCount          = 2;
	gpInfo.pStages             = stages;
	gpInfo.pVertexInputState   = &vertexInput;
	gpInfo.pInputAssemblyState = &inputAssembly;
	gpInfo.pViewportState      = &viewportState;
	gpInfo.pRasterizationState = &rasterizer;
	gpInfo.pMultisampleState   = &multisampling;
	gpInfo.pDepthStencilState  = &depthStencil;
	gpInfo.pColorBlendState    = &colorBlend;
	gpInfo.pDynamicState       = &dynamicState;
	gpInfo.layout              = vk.ribbon.pipeline_layout;
	gpInfo.renderPass          = vk.render_pass.main;
	gpInfo.subpass             = 0;

	for ( variant = 0; variant < 2; variant++ ) {
		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable         = VK_TRUE;
		blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttach.dstColorBlendFactor = (variant == 0)
			? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
			: VK_BLEND_FACTOR_ONE;
		blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
		blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
		blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
			(variant == 0) ? &vk.ribbon.pipeline_alpha : &vk.ribbon.pipeline_additive ) );

		// Phase 7.4c-pipeline-followup-3 — Q1 ribbon re-init mirror of Q3 wiring
		// in vk_init_ribbon. Binding contract unchanged across Q3/Q1 (push 96B
		// V|F + 1 BGL); rendering state identical (the Q1 path differs only in
		// the depth attachment format inside the render pass). Parallel RAL
		// pipeline is built when r_useRALPipelines != 0 so the legacy path stays
		// bit-identical.
		if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
			vk_ral_special_pipeline_params_t pr;
			memset( &pr, 0, sizeof( pr ) );
			pr.vs_module          = vk.modules.ribbon_vs;
			pr.fs_module          = vk.modules.ribbon_fs;
			pr.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
			pr.cullMode           = RAL_CULL_NONE;
			pr.depthTestEnable    = qtrue;
			pr.depthWriteEnable   = qfalse;
			pr.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
			pr.blendEnable        = qtrue;
			pr.srcColor           = RAL_BLEND_SRC_ALPHA;
			pr.dstColor           = ( variant == 0 ) ? RAL_BLEND_ONE_MINUS_SRC_ALPHA : RAL_BLEND_ONE;
			pr.blendOp            = RAL_BLEND_OP_ADD;
			pr.pushConstantSize   = 96;
			pr.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
			if ( vk.ribbon.ral_bgl ) { pr.bgls[ pr.numBgls++ ] = vk.ribbon.ral_bgl; }
			pr.debugName          = ( variant == 0 ) ? "ral-ribbon-alpha-q1" : "ral-ribbon-additive-q1";
			pr.externalLayout     = vk.ribbon.ral_pipeline_layout;  // Phase 7.4c-submit-A3
			pr.externalRenderPass = vk.ral_render_pass.main;
			if ( variant == 0 ) vk.ribbon.ral_pipeline_alpha    = vk_ral_create_special_pipeline( &pr );
			else                vk.ribbon.ral_pipeline_additive = vk_ral_create_special_pipeline( &pr );
		}
	}

	return 2;
}


static int vk_recreate_beam_pipeline( void )
{
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineColorBlendAttachmentState blendAttach;
	VkPipelineViewportStateCreateInfo viewportState;
	VkGraphicsPipelineCreateInfo gpInfo;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkDynamicState dynStates[2];
	VkViewport viewport;
	VkRect2D scissor;

	if ( !vk.beam.available || vk.beam.pipeline_layout == VK_NULL_HANDLE )
		return 0;

	if ( vk.beam.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.beam.pipeline, NULL );
		vk.beam.pipeline = VK_NULL_HANDLE;
	}
	if ( vk.beam.ral_pipeline ) { Ral_DestroyPipeline( vk.beam.ral_pipeline ); vk.beam.ral_pipeline = NULL; }

	memset( stages, 0, sizeof( stages ) );
	stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.beam_vs;
	stages[0].pName  = "main";
	stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.beam_fs;
	stages[1].pName  = "main";

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates    = dynStates;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.width    = (float)vk.renderWidth;
	viewport.height   = (float)vk.renderHeight;
	viewport.maxDepth = 1.0f;
	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width  = vk.renderWidth;
	scissor.extent.height = vk.renderHeight;

	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports    = &viewport;
	viewportState.scissorCount  = 1;
	viewportState.pScissors     = &scissor;

	memset( &rasterizer, 0, sizeof( rasterizer ) );
	rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth   = 1.0f;
	rasterizer.cullMode    = VK_CULL_MODE_NONE;
	rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

	memset( &multisampling, 0, sizeof( multisampling ) );
	multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable  = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	memset( &blendAttach, 0, sizeof( blendAttach ) );
	blendAttach.blendEnable         = VK_TRUE;
	blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
	blendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
	blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
	                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	memset( &colorBlend, 0, sizeof( colorBlend ) );
	colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments    = &blendAttach;

	memset( &gpInfo, 0, sizeof( gpInfo ) );
	gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpInfo.stageCount          = 2;
	gpInfo.pStages             = stages;
	gpInfo.pVertexInputState   = &vertexInput;
	gpInfo.pInputAssemblyState = &inputAssembly;
	gpInfo.pViewportState      = &viewportState;
	gpInfo.pRasterizationState = &rasterizer;
	gpInfo.pMultisampleState   = &multisampling;
	gpInfo.pDepthStencilState  = &depthStencil;
	gpInfo.pColorBlendState    = &colorBlend;
	gpInfo.pDynamicState       = &dynamicState;
	gpInfo.layout              = vk.beam.pipeline_layout;
	gpInfo.renderPass          = vk.render_pass.main;
	gpInfo.subpass             = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL, &vk.beam.pipeline ) );

	// Phase 7.4c-pipeline-followup-3 — Q1 beam re-init mirror of Q3 wiring
	// in vk_init_beam. Binding contract unchanged across Q3/Q1 (push 112B V|F
	// + 1 BGL = vk.beam.ral_bgl); rendering state matches the legacy ONE/ONE
	// additive pipeline.
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		vk_ral_special_pipeline_params_t pr;
		memset( &pr, 0, sizeof( pr ) );
		pr.vs_module          = vk.modules.beam_vs;
		pr.fs_module          = vk.modules.beam_fs;
		pr.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
		pr.cullMode           = RAL_CULL_NONE;
		pr.depthTestEnable    = qtrue;
		pr.depthWriteEnable   = qfalse;
		pr.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
		pr.blendEnable        = qtrue;
		pr.srcColor           = RAL_BLEND_ONE;
		pr.dstColor           = RAL_BLEND_ONE;
		pr.blendOp            = RAL_BLEND_OP_ADD;
		pr.pushConstantSize   = 112;
		pr.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
		if ( vk.beam.ral_bgl ) { pr.bgls[ pr.numBgls++ ] = vk.beam.ral_bgl; }
		pr.debugName          = "ral-beam-q1";
		pr.externalLayout     = vk.beam.ral_pipeline_layout;    // Phase 7.4c-submit-A3
		pr.externalRenderPass = vk.ral_render_pass.main;
		vk.beam.ral_pipeline  = vk_ral_create_special_pipeline( &pr );
	}

	return 1;
}


static int vk_recreate_sprite_pipelines( void )
{
	VkGraphicsPipelineCreateInfo gpInfo;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blendAttach;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkDynamicState dynStates[2];
	VkViewport viewport;
	VkRect2D scissor;
	int variant;

	if ( !vk.sprite.available || vk.sprite.pipeline_layout == VK_NULL_HANDLE )
		return 0;

	if ( vk.sprite.pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.sprite.pipeline_alpha, NULL );
		vk.sprite.pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.sprite.pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.sprite.pipeline_additive, NULL );
		vk.sprite.pipeline_additive = VK_NULL_HANDLE;
	}
	if ( vk.sprite.ral_pipeline_alpha    ) { Ral_DestroyPipeline( vk.sprite.ral_pipeline_alpha    ); vk.sprite.ral_pipeline_alpha    = NULL; }
	if ( vk.sprite.ral_pipeline_additive ) { Ral_DestroyPipeline( vk.sprite.ral_pipeline_additive ); vk.sprite.ral_pipeline_additive = NULL; }

	memset( stages, 0, sizeof( stages ) );
	stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.sprite_vs;
	stages[0].pName  = "main";
	stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.sprite_fs;
	stages[1].pName  = "main";

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates    = dynStates;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.width    = (float)vk.renderWidth;
	viewport.height   = (float)vk.renderHeight;
	viewport.maxDepth = 1.0f;
	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width  = vk.renderWidth;
	scissor.extent.height = vk.renderHeight;

	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports    = &viewport;
	viewportState.scissorCount  = 1;
	viewportState.pScissors     = &scissor;

	memset( &rasterizer, 0, sizeof( rasterizer ) );
	rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth   = 1.0f;
	rasterizer.cullMode    = VK_CULL_MODE_NONE;
	rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

	memset( &multisampling, 0, sizeof( multisampling ) );
	multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable  = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	memset( &colorBlend, 0, sizeof( colorBlend ) );
	colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments    = &blendAttach;

	memset( &gpInfo, 0, sizeof( gpInfo ) );
	gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpInfo.stageCount          = 2;
	gpInfo.pStages             = stages;
	gpInfo.pVertexInputState   = &vertexInput;
	gpInfo.pInputAssemblyState = &inputAssembly;
	gpInfo.pViewportState      = &viewportState;
	gpInfo.pRasterizationState = &rasterizer;
	gpInfo.pMultisampleState   = &multisampling;
	gpInfo.pDepthStencilState  = &depthStencil;
	gpInfo.pColorBlendState    = &colorBlend;
	gpInfo.pDynamicState       = &dynamicState;
	gpInfo.layout              = vk.sprite.pipeline_layout;
	gpInfo.renderPass          = vk.render_pass.main;
	gpInfo.subpass             = 0;

	for ( variant = 0; variant < 2; variant++ ) {
		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable         = VK_TRUE;
		blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttach.dstColorBlendFactor = (variant == 0)
			? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
			: VK_BLEND_FACTOR_ONE;
		blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
		blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
		blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
			(variant == 0) ? &vk.sprite.pipeline_alpha : &vk.sprite.pipeline_additive ) );

		// Phase 7.4c-pipeline-followup-3 — Q1 sprite re-init mirror of Q3 wiring
		// in vk_init_sprite. Binding contract unchanged across Q3/Q1 (push 112B
		// V|F + 1 BGL = vk.sprite.ral_bgl). Topology = TRIANGLE_LIST — sprite.vert
		// emits 6 vertices per quad.
		if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
			vk_ral_special_pipeline_params_t pr;
			memset( &pr, 0, sizeof( pr ) );
			pr.vs_module          = vk.modules.sprite_vs;
			pr.fs_module          = vk.modules.sprite_fs;
			pr.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
			pr.cullMode           = RAL_CULL_NONE;
			pr.depthTestEnable    = qtrue;
			pr.depthWriteEnable   = qfalse;
			pr.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
			pr.blendEnable        = qtrue;
			pr.srcColor           = RAL_BLEND_SRC_ALPHA;
			pr.dstColor           = ( variant == 0 ) ? RAL_BLEND_ONE_MINUS_SRC_ALPHA : RAL_BLEND_ONE;
			pr.blendOp            = RAL_BLEND_OP_ADD;
			pr.pushConstantSize   = 112;
			pr.pushConstantStages = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
			if ( vk.sprite.ral_bgl ) { pr.bgls[ pr.numBgls++ ] = vk.sprite.ral_bgl; }
			pr.debugName          = ( variant == 0 ) ? "ral-sprite-alpha-q1" : "ral-sprite-additive-q1";
			pr.externalLayout     = vk.sprite.ral_pipeline_layout;  // Phase 7.4c-submit-A3
			pr.externalRenderPass = vk.ral_render_pass.main;
			if ( variant == 0 ) vk.sprite.ral_pipeline_alpha    = vk_ral_create_special_pipeline( &pr );
			else                vk.sprite.ral_pipeline_additive = vk_ral_create_special_pipeline( &pr );
		}
	}

	return 2;
}


static int vk_recreate_particle_render_pipelines( void )
{
	VkGraphicsPipelineCreateInfo gpInfo;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blendAttach;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkDynamicState dynStates[2];
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry specMap;
	VkSpecializationInfo specInfo;
	uint32_t blendMaskValue;
	int variant;

	if ( !vk.particle.available || vk.particle.render_pipeline_layout == VK_NULL_HANDLE )
		return 0;

	if ( vk.particle.render_pipeline_alpha != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.render_pipeline_alpha, NULL );
		vk.particle.render_pipeline_alpha = VK_NULL_HANDLE;
	}
	if ( vk.particle.render_pipeline_additive != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.particle.render_pipeline_additive, NULL );
		vk.particle.render_pipeline_additive = VK_NULL_HANDLE;
	}
	if ( vk.particle.ral_render_pipeline_alpha    ) { Ral_DestroyPipeline( vk.particle.ral_render_pipeline_alpha    ); vk.particle.ral_render_pipeline_alpha    = NULL; }
	if ( vk.particle.ral_render_pipeline_additive ) { Ral_DestroyPipeline( vk.particle.ral_render_pipeline_additive ); vk.particle.ral_render_pipeline_additive = NULL; }

	memset( &specMap, 0, sizeof( specMap ) );
	specMap.constantID = 0;
	specMap.offset     = 0;
	specMap.size       = sizeof( uint32_t );
	memset( &specInfo, 0, sizeof( specInfo ) );
	specInfo.mapEntryCount = 1;
	specInfo.pMapEntries   = &specMap;
	specInfo.dataSize      = sizeof( uint32_t );
	specInfo.pData         = &blendMaskValue;

	memset( stages, 0, sizeof( stages ) );
	stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.particle_vs;
	stages[0].pName  = "main";
	stages[0].pSpecializationInfo = &specInfo;
	stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.particle_fs;
	stages[1].pName  = "main";

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates    = dynStates;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.width    = (float)vk.renderWidth;
	viewport.height   = (float)vk.renderHeight;
	viewport.maxDepth = 1.0f;
	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width  = vk.renderWidth;
	scissor.extent.height = vk.renderHeight;

	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports    = &viewport;
	viewportState.scissorCount  = 1;
	viewportState.pScissors     = &scissor;

	memset( &rasterizer, 0, sizeof( rasterizer ) );
	rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth   = 1.0f;
	rasterizer.cullMode    = VK_CULL_MODE_NONE;
	rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;

	memset( &multisampling, 0, sizeof( multisampling ) );
	multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable  = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	memset( &colorBlend, 0, sizeof( colorBlend ) );
	colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments    = &blendAttach;

	memset( &gpInfo, 0, sizeof( gpInfo ) );
	gpInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpInfo.stageCount          = 2;
	gpInfo.pStages             = stages;
	gpInfo.pVertexInputState   = &vertexInput;
	gpInfo.pInputAssemblyState = &inputAssembly;
	gpInfo.pViewportState      = &viewportState;
	gpInfo.pRasterizationState = &rasterizer;
	gpInfo.pMultisampleState   = &multisampling;
	gpInfo.pDepthStencilState  = &depthStencil;
	gpInfo.pColorBlendState    = &colorBlend;
	gpInfo.pDynamicState       = &dynamicState;
	gpInfo.layout              = vk.particle.render_pipeline_layout;
	gpInfo.renderPass          = vk.render_pass.main;
	gpInfo.subpass             = 0;

	for ( variant = 0; variant < 2; variant++ ) {
		blendMaskValue = (uint32_t)variant;

		memset( &blendAttach, 0, sizeof( blendAttach ) );
		blendAttach.blendEnable         = VK_TRUE;
		blendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttach.dstColorBlendFactor = (variant == 0)
			? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
			: VK_BLEND_FACTOR_ONE;
		blendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.srcAlphaBlendFactor = blendAttach.srcColorBlendFactor;
		blendAttach.dstAlphaBlendFactor = blendAttach.dstColorBlendFactor;
		blendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
		blendAttach.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL,
			(variant == 0) ? &vk.particle.render_pipeline_alpha : &vk.particle.render_pipeline_additive ) );

		// Phase 7.4c-pipeline-followup-4 — Q1 particle render mirror of Q3 wiring
		// in vk_init_particle. Binding contract unchanged (no push, 1 BGL =
		// vk.particle.ral_bgl_render); spec constant PIPELINE_BLEND_MASK = variant.
		if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
			vk_ral_special_pipeline_params_t pr;
			ralSpecConstant_t specs[1];
			memset( &pr,   0, sizeof( pr   ) );
			memset( specs, 0, sizeof( specs ) );
			pr.vs_module          = vk.modules.particle_vs;
			pr.fs_module          = vk.modules.particle_fs;
			pr.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
			pr.cullMode           = RAL_CULL_NONE;
			pr.depthTestEnable    = qtrue;
			pr.depthWriteEnable   = qfalse;
			pr.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
			pr.blendEnable        = qtrue;
			pr.srcColor           = RAL_BLEND_SRC_ALPHA;
			pr.dstColor           = ( variant == 0 ) ? RAL_BLEND_ONE_MINUS_SRC_ALPHA : RAL_BLEND_ONE;
			pr.blendOp            = RAL_BLEND_OP_ADD;
			if ( vk.particle.ral_bgl_render ) { pr.bgls[ pr.numBgls++ ] = vk.particle.ral_bgl_render; }
			specs[0].constantId = 0;
			specs[0].value      = (uint32_t)variant;
			pr.specConstants    = specs;
			pr.numSpecConstants = 1;
			pr.debugName        = ( variant == 0 ) ? "ral-particle-render-alpha-q1" : "ral-particle-render-additive-q1";
			pr.externalLayout   = vk.particle.ral_render_pipeline_layout;   // Phase 7.4c-submit-A3
			pr.externalRenderPass = vk.ral_render_pass.main;
			if ( variant == 0 ) vk.particle.ral_render_pipeline_alpha    = vk_ral_create_special_pipeline( &pr );
			else                vk.particle.ral_render_pipeline_additive = vk_ral_create_special_pipeline( &pr );
		}
	}

	return 2;
}


static int vk_recreate_iqm_gpu_pipeline( void )
{
	VkGraphicsPipelineCreateInfo gpInfo;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blendAttach;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkDynamicState dynStates[2];
	VkViewport viewport;
	VkRect2D scissor;
	VkVertexInputBindingDescription iqmBindings[1];
	VkVertexInputAttributeDescription iqmAttribs[6];
	uint32_t stride = 68;

	if ( !vk.iqmGpu.available || vk.iqmGpu.pipeline_layout == VK_NULL_HANDLE )
		return 0;

	if ( vk.iqmGpu.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.iqmGpu.pipeline, NULL );
		vk.iqmGpu.pipeline = VK_NULL_HANDLE;
	}

	memset( &gpInfo, 0, sizeof( gpInfo ) );
	memset( stages, 0, sizeof( stages ) );

	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.iqm_skinning_vs;
	stages[0].pName = "main";

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.iqm_skinning_fs;
	stages[1].pName = "main";

	iqmBindings[0].binding = 0;
	iqmBindings[0].stride = stride;
	iqmBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	iqmAttribs[0].location = 0;
	iqmAttribs[0].binding = 0;
	iqmAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	iqmAttribs[0].offset = 0;

	iqmAttribs[1].location = 1;
	iqmAttribs[1].binding = 0;
	iqmAttribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	iqmAttribs[1].offset = 12;

	iqmAttribs[2].location = 2;
	iqmAttribs[2].binding = 0;
	iqmAttribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	iqmAttribs[2].offset = 24;

	iqmAttribs[3].location = 3;
	iqmAttribs[3].binding = 0;
	iqmAttribs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	iqmAttribs[3].offset = 32;

	iqmAttribs[4].location = 4;
	iqmAttribs[4].binding = 0;
	iqmAttribs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	iqmAttribs[4].offset = 48;

	iqmAttribs[5].location = 5;
	iqmAttribs[5].binding = 0;
	iqmAttribs[5].format = VK_FORMAT_R8G8B8A8_UINT;
	iqmAttribs[5].offset = 64;

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = iqmBindings;
	vertexInput.vertexAttributeDescriptionCount = 6;
	vertexInput.pVertexAttributeDescriptions = iqmAttribs;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynStates;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.width = (float)vk.renderWidth;
	viewport.height = (float)vk.renderHeight;
	viewport.maxDepth = 1.0f;

	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width = vk.renderWidth;
	scissor.extent.height = vk.renderHeight;

	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	memset( &rasterizer, 0, sizeof( rasterizer ) );
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	memset( &multisampling, 0, sizeof( multisampling ) );
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
#ifdef USE_REVERSED_DEPTH
	depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	memset( &blendAttach, 0, sizeof( blendAttach ) );
	blendAttach.blendEnable = VK_FALSE;
	blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	memset( &colorBlend, 0, sizeof( colorBlend ) );
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &blendAttach;

	gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpInfo.stageCount = 2;
	gpInfo.pStages = stages;
	gpInfo.pVertexInputState = &vertexInput;
	gpInfo.pInputAssemblyState = &inputAssembly;
	gpInfo.pViewportState = &viewportState;
	gpInfo.pRasterizationState = &rasterizer;
	gpInfo.pMultisampleState = &multisampling;
	gpInfo.pDepthStencilState = &depthStencil;
	gpInfo.pColorBlendState = &colorBlend;
	gpInfo.pDynamicState = &dynamicState;
	gpInfo.layout = vk.iqmGpu.pipeline_layout;
	gpInfo.renderPass = vk.render_pass.main;
	gpInfo.subpass = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gpInfo, NULL, &vk.iqmGpu.pipeline ) );

	// Phase 7.4c-pipeline-followup-3 — IQM re-init path (e.g. r_fbo toggle).
	// Same contract as primary site at vk.c:~7367; rebuild the sibling RAL
	// pipeline so a stale handle from the previous device generation isn't
	// kept alive across the rebuild.
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		if ( vk.iqmGpu.ral_pipeline ) { Ral_DestroyPipeline( vk.iqmGpu.ral_pipeline ); vk.iqmGpu.ral_pipeline = NULL; }
		vk_ral_special_pipeline_params_t p;
		memset( &p, 0, sizeof( p ) );
		p.vs_module          = vk.modules.iqm_skinning_vs;
		p.fs_module          = vk.modules.iqm_skinning_fs;
		p.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
		p.cullMode           = RAL_CULL_BACK;
		p.depthTestEnable    = qtrue;
		p.depthWriteEnable   = qtrue;
		p.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
		p.blendEnable        = qfalse;
		p.pushConstantSize   = 64;
		p.pushConstantStages = RAL_STAGE_VERTEX;
		if ( vk.iqmGpu.ral_bgl_bones ) { p.bgls[ p.numBgls++ ] = vk.iqmGpu.ral_bgl_bones; }
		if ( vk.ral_bgl_sampler      ) { p.bgls[ p.numBgls++ ] = vk.ral_bgl_sampler; }
		p.debugName          = "ral-iqm-recreate";
		p.externalLayout     = vk.iqmGpu.ral_pipeline_layout;   // Phase 7.4c-submit-A3
		p.externalRenderPass = vk.ral_render_pass.main;
		vk.iqmGpu.ral_pipeline = vk_ral_create_special_pipeline( &p );
	}

	return 1;
}


static int vk_recreate_main_pass_primitive_pipelines( void )
{
	int n = 0;
	n += vk_recreate_ribbon_pipelines();
	n += vk_recreate_beam_pipeline();
	n += vk_recreate_sprite_pipelines();
	n += vk_recreate_particle_render_pipelines();
	n += vk_recreate_iqm_gpu_pipeline();
	return n;
}


/*
================
vk_rebuild_fbo_for_hdr_change

Phase 6B3'-d: tear down + recreate the FBO color attachment chain
when r_hdr live-flips. Re-runs setup_surface_formats to recompute
vk.color_format (including the SFLOAT capability re-probe). If the
effective format is unchanged (e.g. user toggled 1 <-> 2 on a GPU
that lacks SFLOAT and got auto-downgraded both times), bails out
before any teardown.

Render-pass compatibility (VkAttachmentDescription.format) means
the main / post_bloom / bloom_extract render passes — and every
pipeline that targets them — must be destroyed and recreated when
the color attachment format changes. Three pipeline cohorts exist:

  1. Dynamic vk.pipelines[] (main / screenmap / post_bloom slots):
     destroyed by vk_destroy_pipelines(qfalse), rebuilt lazily at
     next draw via vk_gen_pipeline's null-handle check.
  2. Static post-process pipelines (gamma / tonemap / tonemap_variants
     / capture / bloom_extract / bloom_blend / blur / SMAA):
     destroyed by vk_destroy_pipelines(qfalse), rebuilt eagerly by
     vk_update_post_process_pipelines() after this returns.
  3. Standalone main-pass primitive pipelines (ribbon / beam / sprite
     / particle render / iqmGpu): NOT touched by vk_destroy_pipelines.
     Phase 6B3'-d1 fix — rebuilt here explicitly via
     vk_recreate_main_pass_primitive_pipelines(). Without this the
     stale pipelines retain a reference to the destroyed render pass
     and fire VUID-vkCmdDraw-renderPass-02684 on next draw.

The swapchain itself is unaffected — r_hdr only flips the offscreen
color attachment, not the present format.
================
*/
static void vk_rebuild_fbo_for_hdr_change( void )
{
	uint32_t i;
	int rebuilt;
	VkFormat old_color_format = vk.color_format;

	// Re-probe SFLOAT capability + recompute vk.color_format. This
	// may re-enter Cvar_Set if the GPU lacks SFLOAT and r_hdr 1 was
	// requested (auto-downgrade to 2).
	setup_surface_formats( vk.physical_device );

	if ( vk.color_format == old_color_format ) {
		// Effective format unchanged. No teardown needed.
		return;
	}

	ri.Log( SEV_INFO, "r_hdr live: color_format %s -> %s, rebuilding FBO chain\n",
		vk_format_string( old_color_format ),
		vk_format_string( vk.color_format ) );

	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
	}
#ifdef USE_UPLOAD_QUEUE
	// Flush any pending staging-upload batch (texture layout transitions + copies
	// already recorded into vk.staging_command_buffer, offset != 0) BEFORE
	// resetting the cb: a bare qvkResetCommandBuffer would DISCARD those recorded
	// commands, leaving the partially-uploaded images stuck in
	// VK_IMAGE_LAYOUT_UNDEFINED (VUID-vkCmdDraw-None-09600 on the next sample) and
	// desyncing the "offset != 0 ⇒ staging cb is recording" invariant that
	// vk_upload_image relies on (VUID-vkCmdPipelineBarrier-commandBuffer-recording).
	// vk_flush_staging_buffer( qfalse ) submits the batch, waits, resets the cb,
	// and zeroes offset; it is a no-op when nothing is pending. The caller already
	// did vk_wait_idle(), so the submit + fence wait inside it are legal.
	vk_flush_staging_buffer( qfalse );
	qvkResetCommandBuffer( vk.staging_command_buffer, 0 );
#endif

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();

	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	// Phase 6B3'-d1: standalone main-pass primitive pipelines aren't
	// in vk.pipelines[] and aren't in the static post-process list, so
	// vk_destroy_pipelines() above leaves them dangling against the
	// destroyed vk.render_pass.main. Rebuild them eagerly here against
	// the freshly-recreated render pass. Static post-process pipelines
	// and dynamic vk.pipelines[] handles are rebuilt by the caller
	// (vk_update_post_process_pipelines) and by vk_gen_pipeline
	// respectively.
	rebuilt = vk_recreate_main_pass_primitive_pipelines();
	ri.Log( SEV_INFO, "r_hdr live: rebuilt %d main-pass primitive pipeline(s)\n", rebuilt );
}


/*
================
vk_rebuild_for_fbo_change

Phase 6B3'-f: tear down + recreate the swapchain + FBO chain when
r_fbo live-flips. Wider scope than vk_rebuild_fbo_for_hdr_change
because flipping r_fbo:

  - changes vk.fboActive (gates the existence of color_image,
    tonemapped_image, post-process render passes/pipelines)
  - changes swapchain format selection (sRGB-capable swapchain is
    only chosen when r_fbo is true, per Phase 6B3'-d)
  - changes present_format selection (under r_fbo 0 the swapchain
    inherits base_format; under r_fbo 1 it may switch to the
    r_presentBits-selected format)

So the rebuild path matches vk_restart_swapchain — destroy the
swapchain itself, re-run vk_select_surface_format +
setup_surface_formats, recreate. Inline here rather than calling
vk_restart_swapchain to avoid the recursive vk_update_post_process_pipelines
call at the tail of that helper.

Error handling: VK_CHECK in the create helpers terminates on
device-lost / out-of-memory per the engine convention. A "soft revert"
on swapchain-create failure would require keeping the old swapchain
alive throughout, which is incompatible with the strict
destroy-then-create pattern used elsewhere. Documented as a known
limitation; a r_fbo flip on a marginal GPU may abort the engine
rather than fall back to the prior configuration.
================
*/
static void vk_rebuild_for_fbo_change( void )
{
	uint32_t i;
	int rebuilt;
	qboolean new_fbo_active = r_fbo->integer ? qtrue : qfalse;

	if ( new_fbo_active == vk.fboActive ) {
		// No-op: r_fbo modificationCount bumped but effective value
		// unchanged (e.g. clamp into range, set-to-same).
		return;
	}

	ri.Log( SEV_INFO, "r_fbo live: %d -> %d, rebuilding swapchain + FBO chain\n",
		vk.fboActive ? 1 : 0, new_fbo_active ? 1 : 0 );

	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
	}
#ifdef USE_UPLOAD_QUEUE
	// Flush any pending staging-upload batch (texture layout transitions + copies
	// already recorded into vk.staging_command_buffer, offset != 0) BEFORE
	// resetting the cb: a bare qvkResetCommandBuffer would DISCARD those recorded
	// commands, leaving the partially-uploaded images stuck in
	// VK_IMAGE_LAYOUT_UNDEFINED (VUID-vkCmdDraw-None-09600 on the next sample) and
	// desyncing the "offset != 0 ⇒ staging cb is recording" invariant that
	// vk_upload_image relies on (VUID-vkCmdPipelineBarrier-commandBuffer-recording).
	// vk_flush_staging_buffer( qfalse ) submits the batch, waits, resets the cb,
	// and zeroes offset; it is a no-op when nothing is pending. The caller already
	// did vk_wait_idle(), so the submit + fence wait inside it are legal.
	vk_flush_staging_buffer( qfalse );
	qvkResetCommandBuffer( vk.staging_command_buffer, 0 );
#endif

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain( qtrue );   // Phase 7.4c-submit-followup-present-2: preserveRal=qtrue — recreate path; next vk_create_swapchain passes the surviving vk.ral_swapchain through oldExternalSwapchain for atomic handoff.
	vk_destroy_sync_primitives();

	// Update FBO state + dependent flags BEFORE rebuilding so the
	// create paths see the new values. Mirrors the block in
	// vk_initialize (around the existing r_fbo / r_ext_multisample
	// gate) that originally set these at boot.
	vk.fboActive = new_fbo_active;
	if ( !vk.fboActive ) {
		vk.msaaActive = qfalse;
	}
	vk.depthFade.active = ( vk.fboActive && ( r_depthFade->integer
#if FEAT_SSAO
		|| r_ssao->integer
#endif
#if FEAT_GODRAYS
		|| r_godRays->integer
#endif
	) ) ? qtrue : qfalse;

#if FEAT_SHADOW_MAPPING
	{
		cvar_t *r_shadowMapping_local = ri.Cvar_Get( "r_shadowMapping", "0", 0 );
		vk.shadowMap.active = ( r_shadowMapping_local->integer && vk.fboActive ) ? qtrue : qfalse;
	}
#endif

	// Re-run surface-format selection. r_fbo gates the sRGB swapchain
	// preference and switches between base_format / r_presentBits-
	// selected present_format. setup_surface_formats then derives
	// vk.color_format from the new r_fbo + r_hdr combination.
	vk_select_surface_format( vk.physical_device, vk_surface );
	setup_surface_formats( vk.physical_device );

	vk_create_sync_primitives();
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface,
		vk.present_format, &vk.swapchain, qfalse );
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	// Phase 6B3'-f r_fbo-fix: render_pass.main was destroyed +
	// recreated above. The 8 standalone primitive pipelines
	// (ribbon × 2, beam, sprite × 2, particle × 2, IQM) bind to
	// render_pass.main but aren't tracked in vk.pipelines[] or
	// vk_destroy_pipelines's static list — they retain stale
	// renderpass references and fire VUID-vkCmdDraw-renderPass-02684
	// on next draw. Rebuild via the same aggregator r_hdr live uses.
	rebuilt = vk_recreate_main_pass_primitive_pipelines();
	ri.Log( SEV_INFO, "r_fbo live: rebuilt %d main-pass primitive pipeline(s)\n", rebuilt );

	// Static post-process pipelines were destroyed above; the caller
	// (vk_update_post_process_pipelines) recreates them next if
	// vk.fboActive is now true. Under r_fbo 0 the post-process
	// pipelines simply aren't rebuilt; dynamic main-pass pipelines
	// regenerate lazily on first draw via vk_gen_pipeline.
}


void vk_update_post_process_pipelines( void )
{
	// Phase 6B3'-d / 6B3'-f: r_fbo and r_hdr live conversion. The r_fbo
	// check runs first because flipping r_fbo subsumes r_hdr's rebuild
	// scope (the swapchain itself is recreated, picking up the new
	// color_format en route). After an r_fbo flip the r_hdr cache is
	// reseeded so the subsequent r_hdr check is a no-op until the next
	// real r_hdr change.
	static int s_r_fbo_value = -1;
	static int s_r_hdr_value = -1;
	// Phase 6B3'-f split-A3: r_smaa live alloc / release.
	static int s_r_smaa_value = -1;
	// Phase 6B3'-d8: r_hdrDisplay live conversion + HDR10 mastering-metadata
	// re-issue on r_hdrPeakLuminance / r_hdrMinLuminance change.
	static int   s_r_hdrDisplay_value = -1;
	static int   s_hdr_peak_nits      = -1;
	static float s_hdr_min_nits       = -1.0f;
#if FEAT_SHADOW_MAPPING
	// Phase 6.5.4 Part B: r_shadowMapping / r_shadowMapSize live conversion.
	static int   s_r_shadowMapping_value = -1;
	static int   s_r_shadowMapSize_value = -1;
#endif

#if FEAT_SHADOW_MAPPING
	// Phase 6.5.4 Part B-refactor: r_shadowMapping / r_shadowMapSize live toggle.
	// Runs before the r_fbo / r_hdr checks below — if r_fbo or r_hdr ALSO changed
	// this frame, their rebuild paths re-run vk_shadow_release_resources /
	// vk_shadow_alloc_resources internally (via vk_destroy/create_attachments),
	// so this stays correct (at worst one redundant rebuild for that rare combo).
	// Only the shadow resources are rebuilt — not the whole FBO chain: the shadow
	// image / views / render pass / framebuffers / depth pipeline live on their
	// own dedicated memory + render pass and are sampled via a separate descriptor
	// SET, so nothing in the main framebuffers / render passes / pipelines refers
	// to them. The descriptor SET was allocated by vk_init_descriptors (whenever
	// vk.fboActive) and persists across release/realloc; vk_update_attachment_
	// descriptors rebinds it to the fresh vk.shadowMap.view. (Mirrors the r_smaa
	// live hook below.)
	{
		cvar_t *c_sm   = ri.Cvar_Get( "r_shadowMapping", "0", 0 );
		cvar_t *c_size = ri.Cvar_Get( "r_shadowMapSize", "2048", 0 );
		if ( s_r_shadowMapping_value == -1 ) {
			s_r_shadowMapping_value = c_sm->integer;
			s_r_shadowMapSize_value = c_size->integer;
		} else if ( s_r_shadowMapping_value != c_sm->integer || s_r_shadowMapSize_value != c_size->integer ) {
			vk_wait_idle();
			vk_shadow_release_resources();
			vk_shadow_alloc_resources();
			vk_update_attachment_descriptors();
			ri.Log( SEV_INFO, "r_shadowMapping live: mapping=%d size=%d -> shadow %s\n",
				c_sm->integer, c_size->integer, vk.shadowMap.active ? "(re)allocated" : "released" );
			s_r_shadowMapping_value = c_sm->integer;
			s_r_shadowMapSize_value = c_size->integer;
		}
	}
#endif

	if ( s_r_fbo_value == -1 ) {
		s_r_fbo_value = r_fbo->integer;
	} else if ( s_r_fbo_value != r_fbo->integer ) {
		vk_rebuild_for_fbo_change();
		s_r_fbo_value = r_fbo->integer;
		// Reseed the r_hdr cache: vk.fboActive may have flipped, and
		// the FBO chain (including color_format) was rebuilt against
		// the current r_hdr value during the r_fbo rebuild path.
		s_r_hdr_value = r_hdr->integer;
		// Reseed the r_smaa cache too: vk_create_attachments inside
		// the rebuild already called vk_smaa_alloc_resources iff
		// (vk.fboActive && r_smaa > 0), so smaa.active is now in
		// agreement with r_smaa for whatever fbo state we landed in.
		s_r_smaa_value = r_smaa->integer;
		// Reseed r_hdrDisplay too — the rebuild re-ran vk_select_surface_
		// format, which re-negotiated the swapchain colorspace.
		s_r_hdrDisplay_value = r_hdrDisplay->integer;
	}

	// Phase 6B3'-d8: r_hdrDisplay live conversion. Changing the swapchain
	// colorspace (sRGB <-> HDR10/PQ) needs the same full swapchain teardown +
	// recreate that an r_fbo flip does — vk_rebuild_for_fbo_change re-runs
	// vk_select_surface_format (re-negotiates the colorspace per the new
	// r_hdrDisplay) and rebuilds the post-process pipelines (picking up the
	// new hdr_mode / hdr_peak_norm spec consts). Runs after the r_fbo check
	// (which already reseeds this cache when it fires).
	if ( s_r_hdrDisplay_value == -1 ) {
		s_r_hdrDisplay_value = r_hdrDisplay->integer;
	} else if ( s_r_hdrDisplay_value != r_hdrDisplay->integer ) {
		vk_rebuild_for_fbo_change();
		s_r_hdrDisplay_value = r_hdrDisplay->integer;
		s_r_hdr_value  = r_hdr->integer;   // reseed — rebuilt against current r_hdr
		s_r_smaa_value = r_smaa->integer;  // reseed
	}

	if ( vk.fboActive ) {
		if ( s_r_hdr_value == -1 ) {
			s_r_hdr_value = r_hdr->integer;
		} else if ( s_r_hdr_value != r_hdr->integer ) {
			vk_rebuild_fbo_for_hdr_change();
			// Re-read after rebuild: setup_surface_formats may have
			// auto-downgraded r_hdr 1 -> 2 if the GPU lacks SFLOAT.
			s_r_hdr_value = r_hdr->integer;
		}
	}

	// Phase 6B3'-d8: r_hdrPeakLuminance / r_hdrMinLuminance changed — the
	// hdr_peak_norm spec const is picked up by the post-process pipeline
	// rebuild below; here we re-issue the swapchain HDR10 mastering metadata
	// (no-op unless an HDR colorspace is active). Sentinel-gated so it fires
	// only on an actual cvar change, not per frame.
	if ( s_hdr_peak_nits == -1 ) {
		s_hdr_peak_nits = r_hdrPeakLuminance->integer;
		s_hdr_min_nits  = r_hdrMinLuminance->value;
	} else if ( s_hdr_peak_nits != r_hdrPeakLuminance->integer || s_hdr_min_nits != r_hdrMinLuminance->value ) {
		vk_apply_hdr_metadata();
		s_hdr_peak_nits = r_hdrPeakLuminance->integer;
		s_hdr_min_nits  = r_hdrMinLuminance->value;
	}

	// r_smaa live alloc / release. Runs after the r_fbo/r_hdr rebuilds
	// above so we see the post-rebuild vk.fboActive state. Crossing the
	// 0/non-0 boundary either frees ~tens of MB of SMAA images (drop to
	// 0) or reallocates them (rise from 0). Same-side bumps (e.g. 1->4)
	// don't change resource lifetime — only pipeline-side spec constants
	// react, which the existing vk_create_smaa_pipelines path handles.
	if ( vk.fboActive ) {
		if ( s_r_smaa_value == -1 ) {
			s_r_smaa_value = r_smaa->integer;
		} else if ( ( s_r_smaa_value == 0 ) != ( r_smaa->integer == 0 ) ) {
			vk_wait_idle();
			if ( r_smaa->integer == 0 ) {
				vk_smaa_release_resources();
				ri.Log( SEV_INFO, "r_smaa live: 0, released SMAA resources\n" );
			} else {
				vk_smaa_alloc_resources();
				// Rebind the SMAA descriptors to the freshly-allocated
				// image views. The descriptor sets themselves persist
				// across release/realloc cycles (allocated eagerly in
				// vk_init_descriptors when vk.fboActive).
				vk_update_attachment_descriptors();
				ri.Log( SEV_INFO, "r_smaa live: %d, allocated SMAA resources\n", r_smaa->integer );
			}
			s_r_smaa_value = r_smaa->integer;
		} else {
			s_r_smaa_value = r_smaa->integer;
		}
	}

	if ( vk.fboActive ) {
		// update gamma shader (thin display-encode pass — gamma + dither only)
		vk_create_post_process_pipeline( 0, 0, 0 );
		// Phase 6B3'-c1: tonemap default pipeline (no feature variants).
		// Always built; bound when varIdx == 0 (no scene-radiance effects
		// active). The shader still applies exposure_bias and saturation,
		// so r_brightness and r_saturation work even with r_tonemap == 0.
		vk_create_post_process_pipeline( 6, 0, 0 );
		if ( vk.capture.image ) {
			// update capture pipeline
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( r_bloom->integer ) {
			// update bloom shaders
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;

			vk_create_post_process_pipeline( 1, width, height ); // bloom extraction

			for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i += 2 ) {
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline( i + 0, width, height, qtrue ); // horizontal
				vk_create_blur_pipeline( i + 1, width, height, qfalse ); // vertical
			}

			vk_create_post_process_pipeline( 2, glConfig.vidWidth, glConfig.vidHeight ); // bloom blending
		}
		if ( vk.smaa.active ) {
			vk_create_smaa_pipelines();
		}
		// Tear down any previously-active tonemap variant pipelines.
		// Variant slots in vk.tonemap_variants[] are indexed by varIdx
		// bitmask; toggling a feature cvar (r_tonemap, r_ssao, etc.)
		// changes the active index, so the previous slot's pipeline
		// must be explicitly freed. The per-create destroy guard inside
		// vk_create_post_process_pipeline only checks the CURRENT slot,
		// so it would leak any other slot's pipeline. Walking the whole
		// array here keeps "exactly one variant pipeline live at a
		// time" — the same invariant the variant-bind path at draw
		// time assumes. Cost: one vk_wait_idle if any variant is alive
		// (rare, only fires when CVG_RENDERER cvars change), otherwise
		// a no-op loop.
		{
			int i;
			for ( i = 0; i < ARRAY_LEN( vk.tonemap_variants ); i++ ) {
				if ( vk.tonemap_variants[i] != VK_NULL_HANDLE ) {
					vk_wait_idle();
					qvkDestroyPipeline( vk.device, vk.tonemap_variants[i], NULL );
					vk.tonemap_variants[i] = VK_NULL_HANDLE;
				}
			}
		}

		// Create tonemap variant pipeline if any post-process features are active
		{
			int varIdx = 0;
#if FEAT_SSAO
			if ( r_ssao->integer ) varIdx |= TONEMAP_VAR_SSAO;
#endif
#if FEAT_TONEMAP
			if ( r_tonemap->integer ) varIdx |= TONEMAP_VAR_BASE;
#endif
#if FEAT_COLOR_GRADING
			if ( r_colorGrading->integer ) varIdx |= TONEMAP_VAR_CG;
#endif
#if FEAT_GODRAYS
			if ( r_godRays->integer ) varIdx |= TONEMAP_VAR_GODRAYS;
#endif
			if ( varIdx ) {
				vk_create_post_process_pipeline( 5, 0, 0 ); // tonemap variant
			}
		}
	}
}


// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding) — fields ordered by semantics, not packing
typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Terminate( TERM_CLIENT_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Log( SEV_INFO, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Log( SEV_INFO, "memory type bits: %04x\n", memoryTypeBits );
	ri.Log( SEV_INFO, "memory type index: %04x\n", memoryTypeIndex );
	ri.Log( SEV_INFO, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );

		// create color image view
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.pNext = NULL;
		view_desc.flags = 0;
		view_desc.image = attachments[ i ].descriptor;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = attachments[ i ].image_format;
		view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.subresourceRange.aspectMask = attachments[ i ].aspect_flags;
		view_desc.subresourceRange.baseMipLevel = 0;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, attachments[ i ].image_view ) );
	}

	// perform layout transition
	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping attachment layout transition\n", __func__ );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
			for ( i = 0; i < num_attachments; i++ ) {
				record_image_layout_transition( command_buffer,
					attachments[i].descriptor,
					attachments[i].aspect_flags,
					VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
					attachments[i].image_layout,
					0, 0 );
			}
			Ral_SubmitAndDispose( rcmd );
		}
	}

	num_attachments = 0;
}


static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout )
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Terminate( TERM_UNRECOVERABLE, "Attachments array overflow" );
	} else {
		attachments[ num_attachments ].descriptor = desc;
		attachments[ num_attachments ].image_view = image_view;
		attachments[ num_attachments ].usage = usage;
		attachments[ num_attachments ].reqs = *reqs;
		attachments[ num_attachments ].aspect_flags = aspect_flags;
		attachments[ num_attachments ].image_layout = image_layout;
		attachments[ num_attachments ].image_format = image_format;
		attachments[ num_attachments ].memory_offset = 0;
		num_attachments++;
	}
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static void create_color_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout );
}


static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;

	// create depth image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( vk.depthFade.active ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	} else if ( allowTransient ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
}


/*
================
vk_smaa_create_image_dedicated

Helper for vk_smaa_alloc_resources. Creates one SMAA intermediate
image on dedicated VkDeviceMemory (not the attachment pool). The
pool-based create_color_attachment() helper above is unsuitable
because its memory is co-allocated with the rest of the FBO chain
and cannot be selectively released on r_smaa toggle.
================
*/
static void vk_smaa_create_image_dedicated( uint32_t width, uint32_t height, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, VkImageView *view, VkDeviceMemory *memory )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkImageViewCreateInfo view_desc;

	memset( &create_desc, 0, sizeof( create_desc ) );
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	qvkGetImageMemoryRequirements( vk.device, *image, &memory_requirements );
	memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, *image, *memory, 0 ) );

	memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = *image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = format;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, view ) );
}


/*
================
vk_smaa_alloc_resources

Phase 6B3'-f split-A3. Allocates every SMAA-owned Vulkan resource:
  - edges_image (R8G8) + dedicated memory + view
  - blend_image (RGBA8) + dedicated memory + view
  - input_image (vk.color_format) + dedicated memory + view
  - area_image (R8G8 LUT 160x560) + dedicated memory + view
  - search_image (R8 LUT 64x16) + dedicated memory + view
  - point_sampler / linear_sampler
  - one-shot LUT byte upload via a temporary staging buffer

Idempotent (early-out if already allocated). Does not allocate
descriptor sets — those live in vk_init_descriptors and are reused
across release/realloc cycles (their pool slots are reserved up-front
in the pool sizing).

Sets vk.smaa.active = qtrue on success. Callers are responsible
for invoking vk_update_attachment_descriptors() after this returns
so the descriptor sets pick up the fresh image-view handles.

Called from:
  - vk_create_attachments (cold start + r_fbo flip rebuild)
  - vk_update_post_process_pipelines via the r_smaa live hook
================
*/
static void vk_smaa_alloc_resources( void )
{
	VkSamplerCreateInfo sampler_desc;
	VkCommandBuffer command_buffer;
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkBuffer staging_buf;
	VkDeviceMemory staging_mem;
	VkDeviceSize staging_size;
	VkBufferImageCopy region;
	byte *mapped;

	if ( vk.smaa.area_image != VK_NULL_HANDLE ) {
		// already allocated
		return;
	}

	// ── Intermediate attachment images (resolution-dependent) ──────
	// Dedicated memory per image (NOT the shared attachment pool) so
	// the release helper can free them without rebuilding the pool.
	vk_smaa_create_image_dedicated( glConfig.vidWidth, glConfig.vidHeight,
		VK_FORMAT_R8G8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		&vk.smaa.edges_image, &vk.smaa.edges_view, &vk.smaa.edges_memory );

	vk_smaa_create_image_dedicated( glConfig.vidWidth, glConfig.vidHeight,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		&vk.smaa.blend_image, &vk.smaa.blend_view, &vk.smaa.blend_memory );

	vk_smaa_create_image_dedicated( glConfig.vidWidth, glConfig.vidHeight,
		vk.color_format,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		&vk.smaa.input_image, &vk.smaa.input_view, &vk.smaa.input_memory );

	// ── LUT textures (resolution-independent, static once uploaded) ─
	vk_smaa_create_image_dedicated( AREATEX_WIDTH, AREATEX_HEIGHT,
		VK_FORMAT_R8G8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		&vk.smaa.area_image, &vk.smaa.area_view, &vk.smaa.area_memory );

	vk_smaa_create_image_dedicated( SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT,
		VK_FORMAT_R8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		&vk.smaa.search_image, &vk.smaa.search_view, &vk.smaa.search_memory );

	// ── Samplers ────────────────────────────────────────────────────
	memset( &sampler_desc, 0, sizeof( sampler_desc ) );
	sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.magFilter = VK_FILTER_NEAREST;
	sampler_desc.minFilter = VK_FILTER_NEAREST;
	VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.smaa.point_sampler ) );
	sampler_desc.magFilter = VK_FILTER_LINEAR;
	sampler_desc.minFilter = VK_FILTER_LINEAR;
	VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.smaa.linear_sampler ) );

	// ── LUT data upload via staging buffer ──────────────────────────
	staging_size = AREATEX_SIZE + SEARCHTEX_SIZE;
	memset( &buf_desc, 0, sizeof( buf_desc ) );
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.size = staging_size;
	buf_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &staging_buf ) );

	qvkGetBufferMemoryRequirements( vk.device, staging_buf, &mem_req );
	memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type( mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &staging_mem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, staging_buf, staging_mem, 0 ) );
	vk_ral_register_buffer( staging_buf, staging_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                        "vk.smaa.staging" );

	VK_CHECK( qvkMapMemory( vk.device, staging_mem, 0, VK_WHOLE_SIZE, 0, (void **)&mapped ) );
	memcpy( mapped, areaTexBytes, AREATEX_SIZE );
	memcpy( mapped + AREATEX_SIZE, searchTexBytes, SEARCHTEX_SIZE );
	qvkUnmapMemory( vk.device, staging_mem );

	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping SMAA LUT upload + layout seeding\n", __func__ );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );

			record_image_layout_transition( command_buffer, vk.smaa.area_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
			record_image_layout_transition( command_buffer, vk.smaa.search_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

			memset( &region, 0, sizeof( region ) );
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.depth = 1;

			region.bufferOffset = 0;
			region.imageExtent.width = AREATEX_WIDTH;
			region.imageExtent.height = AREATEX_HEIGHT;
			qvkCmdCopyBufferToImage( command_buffer, staging_buf, vk.smaa.area_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

			region.bufferOffset = AREATEX_SIZE;
			region.imageExtent.width = SEARCHTEX_WIDTH;
			region.imageExtent.height = SEARCHTEX_HEIGHT;
			qvkCmdCopyBufferToImage( command_buffer, staging_buf, vk.smaa.search_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

			record_image_layout_transition( command_buffer, vk.smaa.area_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			record_image_layout_transition( command_buffer, vk.smaa.search_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

			// Phase 6B3'-smaa-layout-transition-fix: the edges/blend/input
			// intermediate images are created with VK_IMAGE_LAYOUT_UNDEFINED.
			// The per-frame SMAA passes expect them already in
			// SHADER_READ_ONLY_OPTIMAL on entry (each frame's barriers declare
			// that as the old layout), so without seeding the layout here the
			// first frame's vkQueueSubmit fires one validation error per image
			// (UNDEFINED vs SHADER_READ_ONLY_OPTIMAL). Seed it once on alloc;
			// this runs on the boot path, the r_fbo flip rebuild, and the
			// r_smaa 0->N live realloc — all three reach vk_smaa_alloc_resources.
			record_image_layout_transition( command_buffer, vk.smaa.edges_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			record_image_layout_transition( command_buffer, vk.smaa.blend_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			record_image_layout_transition( command_buffer, vk.smaa.input_image,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

			Ral_SubmitAndDispose( rcmd );
		}
	}

	vk_ral_unregister_buffer( staging_buf );
	qvkDestroyBuffer( vk.device, staging_buf, NULL );
	qvkFreeMemory( vk.device, staging_mem, NULL );

	SET_OBJECT_NAME( vk.smaa.area_image,   "SMAA area LUT",        VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.smaa.area_view,    "SMAA area LUT view",   VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.smaa.search_image, "SMAA search LUT",      VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.smaa.search_view,  "SMAA search LUT view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.smaa.edges_image,  "SMAA edges image",     VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.smaa.blend_image,  "SMAA blend image",     VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.smaa.input_image,  "SMAA input copy",      VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	vk.smaa.active = qtrue;

	// Phase 7.4c-pipeline-followup-5 PART 2.7 — pair SMAA framebuffer
	// lifecycle with view lifecycle. Idempotent: on cold-start the helper
	// early-outs because vk.render_pass.smaa_edge hasn't been built yet
	// (vk_create_attachments runs vk_smaa_alloc_resources BEFORE
	// vk_create_render_passes); the later vk_create_framebuffers call
	// builds them via the same helper. On the live r_smaa 0→1 toggle the
	// render passes are already live, so the FBs build here.
	vk_smaa_create_framebuffers();
}


/*
================
vk_smaa_release_resources

Phase 6B3'-f split-A3. Inverse of vk_smaa_alloc_resources: destroys
every SMAA-owned image, view, memory, and sampler. Idempotent.

Does NOT free descriptor sets — those persist across release/realloc
cycles and are rebound to fresh image views via
vk_update_attachment_descriptors after the next alloc. Until then,
the dispatch gate (!vk.smaa.active in RB_SMAA / the post-process
chain) prevents any reference to the stale descriptors.

Callers must vk_wait_idle() before entering this function so no
in-flight command buffer is still referencing the resources.

Called from:
  - vk_destroy_attachments (full FBO chain teardown)
  - vk_update_post_process_pipelines via the r_smaa live hook
================
*/
static void vk_smaa_release_resources( void )
{
	if ( vk.smaa.area_image == VK_NULL_HANDLE ) {
		// already released
		return;
	}

	// Phase 7.4c-pipeline-followup-5 PART 2.7 — tear down the SMAA-view-
	// dependent framebuffers BEFORE the views they reference. Vulkan does
	// not require this ordering for vkDestroyFramebuffer correctness, but
	// it makes the dependency chain explicit and matches the destroy-then-
	// create idiom used elsewhere in the codebase. The next vk_smaa_alloc_
	// resources call recreates them via vk_smaa_create_framebuffers.
	vk_smaa_destroy_framebuffers();

	if ( vk.smaa.edges_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.smaa.edges_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.edges_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.edges_memory, NULL );
		vk.smaa.edges_image  = VK_NULL_HANDLE;
		vk.smaa.edges_view   = VK_NULL_HANDLE;
		vk.smaa.edges_memory = VK_NULL_HANDLE;
	}
	if ( vk.smaa.blend_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.smaa.blend_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.blend_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.blend_memory, NULL );
		vk.smaa.blend_image  = VK_NULL_HANDLE;
		vk.smaa.blend_view   = VK_NULL_HANDLE;
		vk.smaa.blend_memory = VK_NULL_HANDLE;
	}
	if ( vk.smaa.input_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.smaa.input_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.input_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.input_memory, NULL );
		vk.smaa.input_image  = VK_NULL_HANDLE;
		vk.smaa.input_view   = VK_NULL_HANDLE;
		vk.smaa.input_memory = VK_NULL_HANDLE;
	}

	if ( vk.smaa.area_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.smaa.area_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.area_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.area_memory, NULL );
		vk.smaa.area_image  = VK_NULL_HANDLE;
		vk.smaa.area_view   = VK_NULL_HANDLE;
		vk.smaa.area_memory = VK_NULL_HANDLE;
	}
	if ( vk.smaa.search_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.smaa.search_image, NULL );
		qvkDestroyImageView( vk.device, vk.smaa.search_view, NULL );
		qvkFreeMemory( vk.device, vk.smaa.search_memory, NULL );
		vk.smaa.search_image  = VK_NULL_HANDLE;
		vk.smaa.search_view   = VK_NULL_HANDLE;
		vk.smaa.search_memory = VK_NULL_HANDLE;
	}

	if ( vk.smaa.point_sampler != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, vk.smaa.point_sampler, NULL );
		vk.smaa.point_sampler = VK_NULL_HANDLE;
	}
	if ( vk.smaa.linear_sampler != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, vk.smaa.linear_sampler, NULL );
		vk.smaa.linear_sampler = VK_NULL_HANDLE;
	}

	vk.smaa.active = qfalse;
}


/*
================
vk_smaa_create_framebuffers / vk_smaa_destroy_framebuffers

Phase 7.4c-pipeline-followup-5 PART 2.7. Lifecycle of the two SMAA
framebuffers whose attachments reference SMAA-owned VkImageViews:

  vk.framebuffers.smaa_edge  attaches vk.smaa.edges_view
  vk.framebuffers.smaa_blend attaches vk.smaa.blend_view

Both views are destroyed by vk_smaa_release_resources on r_smaa 1→0
toggle. Without these helpers, the framebuffers retained the stale
view handles and tripped VUID-VkRenderPassBeginInfo-framebuffer-
parameter at the next vkCmdBeginRenderPass after the user toggled
r_smaa back to 1.

vk.framebuffers.smaa_resolve attaches vk.tonemapped_image_view — a
non-SMAA-owned view that survives the toggle — so it stays managed
inline by vk_create_framebuffers / vk_destroy_framebuffers.

Idempotency: vk_smaa_create_framebuffers early-outs if the FBs already
exist or if the render passes haven't been built yet (vk_create_
attachments runs vk_smaa_alloc_resources BEFORE vk_create_render_passes
on cold start; the later vk_create_framebuffers call picks them up via
this same helper).
================
*/
static void vk_smaa_create_framebuffers( void )
{
	VkFramebufferCreateInfo desc;
	VkImageView             attachments[1];

	if ( vk.framebuffers.smaa_edge != VK_NULL_HANDLE ) return;   // already created
	if ( vk.render_pass.smaa_edge  == VK_NULL_HANDLE ) return;   // render passes not built yet (cold-start ordering)
	if ( vk.smaa.edges_view        == VK_NULL_HANDLE ) return;   // views not allocated yet

	memset( &desc, 0, sizeof( desc ) );
	desc.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.attachmentCount = 1;
	desc.pAttachments    = attachments;
	desc.width           = glConfig.vidWidth;
	desc.height          = glConfig.vidHeight;
	desc.layers          = 1;

	attachments[0]  = vk.smaa.edges_view;
	desc.renderPass = vk.render_pass.smaa_edge;
	VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_edge ) );
	SET_OBJECT_NAME( vk.framebuffers.smaa_edge, "framebuffer - smaa_edge", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

	attachments[0]  = vk.smaa.blend_view;
	desc.renderPass = vk.render_pass.smaa_blend;
	VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_blend ) );
	SET_OBJECT_NAME( vk.framebuffers.smaa_blend, "framebuffer - smaa_blend", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
}


static void vk_smaa_destroy_framebuffers( void )
{
	if ( vk.framebuffers.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_edge, NULL );
		vk.framebuffers.smaa_edge = VK_NULL_HANDLE;
	}
	if ( vk.framebuffers.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_blend, NULL );
		vk.framebuffers.smaa_blend = VK_NULL_HANDLE;
	}
}


#if FEAT_SHADOW_MAPPING
/*
================
vk_shadow_alloc_resources

Phase 6.5.4 Part B-refactor. Creates the CSM depth target: a 4-layer 2D-array
D32 image on dedicated memory, its 2D_ARRAY sampling view + 4 per-cascade 2D
views, the border-clamp NEAREST sampler, the depth-only render pass, 4
framebuffers (one per cascade layer), and the depth-only caster pipeline + its
push-constant pipeline layout. Idempotent (no-op if vk.shadowMap.image is
already live). Re-derives vk.shadowMap.active / .size from r_shadowMapping /
r_shadowMapSize at call time; if shadows are off or the FBO is inactive it
leaves .active == qfalse and creates nothing (the if ( active ) gate below).

The shadow sampler descriptor SET is NOT allocated here -- vk_init_descriptors
owns it (allocated whenever vk.fboActive, like the SMAA SETs); this helper only
creates the image / views / RP / FB / pipeline, and vk_update_attachment_
descriptors binds the SET to vk.shadowMap.view afterwards. Mirrors
vk_smaa_alloc_resources.

Callers must vk_wait_idle() before re-entry on a live toggle. Reached from
vk_create_attachments (cold start + r_fbo / r_hdr rebuild) and from
vk_update_post_process_pipelines (live r_shadowMapping / r_shadowMapSize).
================
*/
static void vk_shadow_alloc_resources( void )
{
	if ( vk.shadowMap.image != VK_NULL_HANDLE )
		return; // already allocated (idempotent)

	{
		cvar_t *c_sm   = ri.Cvar_Get( "r_shadowMapping", "0", 0 );
		cvar_t *c_size = ri.Cvar_Get( "r_shadowMapSize", "2048", 0 );
		vk.shadowMap.active = ( c_sm->integer && vk.fboActive ) ? qtrue : qfalse;
		vk.shadowMap.size = c_size->integer;
		if ( vk.shadowMap.size < 512 )  vk.shadowMap.size = 512;
		if ( vk.shadowMap.size > 4096 ) vk.shadowMap.size = 4096;
	}
	if ( vk.shadowMap.active ) {
		VkImageCreateInfo img_desc;
		VkImageViewCreateInfo view_desc;
		VkSamplerCreateInfo sampler_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;
		VkAttachmentDescription att;
		VkAttachmentReference depthRef;
		VkSubpassDescription subpass;
		VkRenderPassCreateInfo rpDesc;
		VkFramebufferCreateInfo fbDesc;
		VkPushConstantRange pushRange;
		VkPipelineLayoutCreateInfo plDesc;
		uint32_t mapSize = vk.shadowMap.size;
		uint32_t mem_type;
		int casc;

		// depth image — 4-layer 2D array (one layer per CSM cascade)
		memset( &img_desc, 0, sizeof( img_desc ) );
		img_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_desc.imageType = VK_IMAGE_TYPE_2D;
		img_desc.format = vk.depth_format;
		img_desc.extent.width = mapSize;
		img_desc.extent.height = mapSize;
		img_desc.extent.depth = 1;
		img_desc.mipLevels = 1;
		img_desc.arrayLayers = SHADOWMAP_MAX_CASCADES;
		img_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		img_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		img_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		img_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &img_desc, NULL, &vk.shadowMap.image ) );
		qvkGetImageMemoryRequirements( vk.device, vk.shadowMap.image, &mem_req );

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = mem_req.size;
		mem_type = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		alloc_info.memoryTypeIndex = mem_type;
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.shadowMap.memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.shadowMap.image, vk.shadowMap.memory, 0 ) );

		// sampling view — 2D_ARRAY, all cascades (bound to descriptor set 3)
		memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.shadowMap.image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view_desc.format = vk.depth_format;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = SHADOWMAP_MAX_CASCADES;
		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.shadowMap.view ) );

		// per-cascade 2D views — framebuffer attachments
		for ( casc = 0; casc < SHADOWMAP_MAX_CASCADES; casc++ ) {
			view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view_desc.subresourceRange.baseArrayLayer = casc;
			view_desc.subresourceRange.layerCount = 1;
			VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.shadowMap.layerView[casc] ) );
		}

		// nearest sampler with border clamp (outside shadow map = lit)
		memset( &sampler_desc, 0, sizeof( sampler_desc ) );
		sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_desc.magFilter = VK_FILTER_NEAREST;
		sampler_desc.minFilter = VK_FILTER_NEAREST;
		sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_desc.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // out-of-bounds = lit (depth=1.0)
		VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.shadowMap.sampler ) );

		// render pass (depth-only, clear on begin) — reused for every cascade fb
		memset( &att, 0, sizeof( att ) );
		att.format = vk.depth_format;
		att.samples = VK_SAMPLE_COUNT_1_BIT;
		att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		depthRef.attachment = 0;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pDepthStencilAttachment = &depthRef;

		memset( &rpDesc, 0, sizeof( rpDesc ) );
		rpDesc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpDesc.attachmentCount = 1;
		rpDesc.pAttachments = &att;
		rpDesc.subpassCount = 1;
		rpDesc.pSubpasses = &subpass;
		VK_CHECK( qvkCreateRenderPass( vk.device, &rpDesc, NULL, &vk.shadowMap.renderPass ) );

		// one framebuffer per cascade (single-layer 2D view)
		for ( casc = 0; casc < SHADOWMAP_MAX_CASCADES; casc++ ) {
			memset( &fbDesc, 0, sizeof( fbDesc ) );
			fbDesc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbDesc.renderPass = vk.shadowMap.renderPass;
			fbDesc.attachmentCount = 1;
			fbDesc.pAttachments = &vk.shadowMap.layerView[casc];
			fbDesc.width = mapSize;
			fbDesc.height = mapSize;
			fbDesc.layers = 1;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &fbDesc, NULL, &vk.shadowMap.framebuffer[casc] ) );
		}

		// pipeline layout: 128 B of push constants — mat4 cascadeMVP @ 0,
		// mat4 modelMatrix @ 64. Phase 6.5.4d2: modelMatrix is the caster's
		// model->world transform (identity for the worldspawn batch, the
		// entity's [axis|origin] for inline brush-model casters). 128 B fits
		// the Vulkan guaranteed minimum (maxPushConstantsSize >= 128) exactly.
		pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushRange.offset = 0;
		pushRange.size = 128; // mat4 cascadeMVP + mat4 modelMatrix

		memset( &plDesc, 0, sizeof( plDesc ) );
		plDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		plDesc.pushConstantRangeCount = 1;
		plDesc.pPushConstantRanges = &pushRange;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &plDesc, NULL, &vk.shadowMap.depthLayout ) );

		// depth-only caster pipeline (created against vk.shadowMap.renderPass —
		// depth-only render passes are not compatible with the main color pass,
		// so the standard pipeline cache can't build this; do it by hand).
		{
			VkPipelineShaderStageCreateInfo            stages[2];
			VkVertexInputBindingDescription            vibd;
			VkVertexInputAttributeDescription          viad;
			VkPipelineVertexInputStateCreateInfo       vis;
			VkPipelineInputAssemblyStateCreateInfo     ias;
			VkPipelineViewportStateCreateInfo          vps;
			VkPipelineRasterizationStateCreateInfo     rs;
			VkPipelineMultisampleStateCreateInfo       ms;
			VkPipelineDepthStencilStateCreateInfo      ds;
			VkPipelineColorBlendStateCreateInfo        cb;
			VkPipelineDynamicStateCreateInfo           dyn;
			VkDynamicState                             dynStates[3];
			VkGraphicsPipelineCreateInfo               gp;

			memset( stages, 0, sizeof( stages ) );
			stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
			stages[0].module = vk.modules.shadow_depth_vs;
			stages[0].pName  = "main";
			stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
			stages[1].module = vk.modules.shadow_depth_fs;
			stages[1].pName  = "main";

			// binding 0 = position only, as vec4 (shader reads vec3, ignores w)
			vibd.binding   = 0;
			vibd.stride    = sizeof( vec4_t );
			vibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			viad.location  = 0;
			viad.binding   = 0;
			viad.format    = VK_FORMAT_R32G32B32A32_SFLOAT;
			viad.offset    = 0;
			memset( &vis, 0, sizeof( vis ) );
			vis.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vis.vertexBindingDescriptionCount   = 1;
			vis.pVertexBindingDescriptions      = &vibd;
			vis.vertexAttributeDescriptionCount = 1;
			vis.pVertexAttributeDescriptions    = &viad;

			memset( &ias, 0, sizeof( ias ) );
			ias.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			memset( &vps, 0, sizeof( vps ) );
			vps.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			vps.viewportCount = 1;
			vps.scissorCount  = 1; // both dynamic — see dynStates

			memset( &rs, 0, sizeof( rs ) );
			rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rs.polygonMode             = VK_POLYGON_MODE_FILL;
			// Phase 6.5.4d1: VK_CULL_MODE_NONE (was FRONT_BIT in 6.5.4b/c). World
			// BSP brushfaces are single-sided — with front-culling a face that
			// faces the light writes nothing → the wall stops blocking light →
			// leak. NONE renders both sides so every face contributes; acne is
			// held by the slope-scaled depth bias below (now r_csmBias-driven and
			// dynamic, so it's tunable at runtime without a pipeline rebuild).
			rs.cullMode                = VK_CULL_MODE_NONE;
			rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rs.depthBiasEnable         = VK_TRUE;   // factors are dynamic — see vkCmdSetDepthBias in vk_render_shadow_map
			rs.depthBiasConstantFactor = 0.0f;
			rs.depthBiasSlopeFactor    = 0.0f;
			rs.depthBiasClamp          = 0.0f;
			rs.lineWidth               = 1.0f;

			memset( &ms, 0, sizeof( ms ) );
			ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			memset( &ds, 0, sizeof( ds ) );
			ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			ds.depthTestEnable  = VK_TRUE;
			ds.depthWriteEnable = VK_TRUE;
			ds.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

			memset( &cb, 0, sizeof( cb ) );
			cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			cb.attachmentCount = 0; // depth-only render pass — no color attachments
			cb.pAttachments    = NULL;

			dynStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			dynStates[1] = VK_DYNAMIC_STATE_SCISSOR;
			dynStates[2] = VK_DYNAMIC_STATE_DEPTH_BIAS; // set per pass from r_csmBias
			memset( &dyn, 0, sizeof( dyn ) );
			dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dyn.dynamicStateCount = 3;
			dyn.pDynamicStates    = dynStates;

			memset( &gp, 0, sizeof( gp ) );
			gp.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			gp.stageCount          = 2;
			gp.pStages             = stages;
			gp.pVertexInputState   = &vis;
			gp.pInputAssemblyState = &ias;
			gp.pViewportState      = &vps;
			gp.pRasterizationState = &rs;
			gp.pMultisampleState   = &ms;
			gp.pDepthStencilState  = &ds;
			gp.pColorBlendState    = &cb;
			gp.pDynamicState       = &dyn;
			gp.layout              = vk.shadowMap.depthLayout;
			gp.renderPass          = vk.shadowMap.renderPass;
			gp.subpass             = 0;
			gp.basePipelineIndex   = -1;
			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &gp, NULL, &vk.shadowMap.depthPipeline ) );

			// Phase 7.4c-pipeline-followup-3 — shadow-depth (depthLayout, no
			// descriptor sets, push-constants only).
			// Binding contract per shadow_depth.vert: push_constant `Transform`
			// = 2 × mat4 = 128 bytes, VERTEX only (cascadeMVP + modelMatrix per
			// 6.5.4d2 plumbing). shadow_depth.frag has no resources. 0 BGLs.
			// Dynamic depth bias is handled by RAL's implicit dynamic-state
			// set widening (Phase 7.3c already declares depthBias dynamic
			// when raster.depthBiasEnable=qtrue — see §17.8 gap #5).
			if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
				vk_ral_special_pipeline_params_t pr;
				memset( &pr, 0, sizeof( pr ) );
				pr.vs_module          = vk.modules.shadow_depth_vs;
				pr.fs_module          = vk.modules.shadow_depth_fs;
				pr.topology           = RAL_TOPOLOGY_TRIANGLE_LIST;
				pr.cullMode           = RAL_CULL_NONE;
				pr.frontFace          = RAL_FRONT_FACE_CCW;   // §17.6.c — BSP single-sided face leak fix
				pr.depthBiasEnable    = qtrue;                // values set per pass via Ral_CmdSetDepthBias
				pr.depthTestEnable    = qtrue;
				pr.depthWriteEnable   = qtrue;
				pr.depthCompareOp     = RAL_COMPARE_LESS_EQUAL;
				pr.blendEnable        = qfalse;
				pr.numBgls            = 0;                    // shadow caster uses no descriptor sets
				pr.numColorAttachments = 0;                   // depth-only
				pr.depthOnly           = qtrue;               // Phase 7.4c-submit-A3 — explicit signal
				pr.pushConstantSize   = 128;                  // cascadeMVP + modelMatrix
				pr.pushConstantStages = RAL_STAGE_VERTEX;
				pr.debugName          = "ral-shadow-depth";
				pr.externalLayout     = vk.shadowMap.ral_depthLayout;   // Phase 7.4c-submit-A3
				pr.externalRenderPass = vk.shadowMap.ral_renderPass;     // Phase 7.4c-submit-A3
				vk.shadowMap.ral_depthPipeline = vk_ral_create_special_pipeline( &pr );
			}
		}

		vk.shadowMap.casterBuiltSurfaces = NULL; // force lazy rebuild for the current world

		SET_OBJECT_NAME( vk.shadowMap.image, "shadow map depth array (4 cascades)", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.shadowMap.renderPass, "render pass - shadow depth", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		SET_OBJECT_NAME( vk.shadowMap.depthPipeline, "pipeline - shadow caster depth", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		SET_OBJECT_NAME( vk.shadowMap.descriptor, "descriptor - shadow map sampler2DArray", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}
}
#endif


#if FEAT_SHADOW_MAPPING
/*
================
vk_shadow_release_resources

Phase 6.5.4 Part B-refactor. Inverse of vk_shadow_alloc_resources: destroys
every shadow-owned image / view / framebuffer / sampler / memory / render pass
/ pipeline / pipeline-layout / caster buffer, then nulls each field individually
(NOT memset( &vk.shadowMap, 0, ... )). Idempotent (no-op if vk.shadowMap.image
is already gone).

Does NOT free vk.shadowMap.descriptor -- that SET is owned by vk_init_descriptors
(allocated from a pool this teardown does not reset), persists across release /
realloc cycles, and is rebound to the fresh view by vk_update_attachment_
descriptors after the next alloc; the dispatch gate (!vk.shadowMap.active in
vk_render_shadow_map / the lit pass) keeps anything from reading it meanwhile.
Mirrors vk_smaa_release_resources. Callers must vk_wait_idle() first.
================
*/
static void vk_shadow_release_resources( void )
{
	if ( vk.shadowMap.image ) {
		int casc;
		qvkDestroyImage( vk.device, vk.shadowMap.image, NULL );
		qvkDestroyImageView( vk.device, vk.shadowMap.view, NULL );
		for ( casc = 0; casc < SHADOWMAP_MAX_CASCADES; casc++ ) {
			if ( vk.shadowMap.layerView[casc] )
				qvkDestroyImageView( vk.device, vk.shadowMap.layerView[casc], NULL );
			if ( vk.shadowMap.framebuffer[casc] )
				qvkDestroyFramebuffer( vk.device, vk.shadowMap.framebuffer[casc], NULL );
		}
		qvkDestroySampler( vk.device, vk.shadowMap.sampler, NULL );
		qvkFreeMemory( vk.device, vk.shadowMap.memory, NULL );
		qvkDestroyRenderPass( vk.device, vk.shadowMap.renderPass, NULL );
		qvkDestroyPipelineLayout( vk.device, vk.shadowMap.depthLayout, NULL );
		if ( vk.shadowMap.depthPipeline )
			qvkDestroyPipeline( vk.device, vk.shadowMap.depthPipeline, NULL );
		// Phase 7.4c-pipeline-followup-2 — sibling RAL pipeline teardown.
		if ( vk.shadowMap.ral_depthPipeline ) { Ral_DestroyPipeline( vk.shadowMap.ral_depthPipeline ); vk.shadowMap.ral_depthPipeline = NULL; }
		if ( vk.shadowMap.casterBuf ) {
			vk_ral_unregister_buffer( vk.shadowMap.casterBuf );
			qvkDestroyBuffer( vk.device, vk.shadowMap.casterBuf, NULL );
		}
		if ( vk.shadowMap.casterMem )
			qvkFreeMemory( vk.device, vk.shadowMap.casterMem, NULL );
		if ( vk.shadowMap.casterBmodelBuf ) {
			vk_ral_unregister_buffer( vk.shadowMap.casterBmodelBuf );
			qvkDestroyBuffer( vk.device, vk.shadowMap.casterBmodelBuf, NULL );
		}
		if ( vk.shadowMap.casterBmodelMem )
			qvkFreeMemory( vk.device, vk.shadowMap.casterBmodelMem, NULL );
		if ( vk.shadowMap.bmodelRanges )
			ri.Free( vk.shadowMap.bmodelRanges );
		vk.shadowMap.image            = VK_NULL_HANDLE;
		vk.shadowMap.view             = VK_NULL_HANDLE;
		for ( casc = 0; casc < SHADOWMAP_MAX_CASCADES; casc++ ) {
			vk.shadowMap.layerView[casc]   = VK_NULL_HANDLE;
			vk.shadowMap.framebuffer[casc] = VK_NULL_HANDLE;
		}
		vk.shadowMap.memory           = VK_NULL_HANDLE;
		vk.shadowMap.sampler          = VK_NULL_HANDLE;
		vk.shadowMap.renderPass       = VK_NULL_HANDLE;
		vk.shadowMap.depthLayout      = VK_NULL_HANDLE;
		vk.shadowMap.depthPipeline    = VK_NULL_HANDLE;
		vk.shadowMap.size             = 0;
		vk.shadowMap.casterBuf        = VK_NULL_HANDLE;
		vk.shadowMap.casterMem        = VK_NULL_HANDLE;
		vk.shadowMap.casterVtxBytes   = 0;
		vk.shadowMap.casterIndexCount = 0;
		vk.shadowMap.casterBmodelBuf      = VK_NULL_HANDLE;
		vk.shadowMap.casterBmodelMem      = VK_NULL_HANDLE;
		vk.shadowMap.casterBmodelVtxBytes = 0;
		vk.shadowMap.bmodelRanges         = NULL;
		vk.shadowMap.numBmodelRanges      = 0;
		vk.shadowMap.casterBuiltSurfaces = NULL;
	}
	vk.shadowMap.active = qfalse;
}
#endif


static void vk_create_attachments( void )
{
	uint32_t i;

	vk_clear_attachment_pool();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	if ( vk.fboActive ) {

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
				usage, &vk.bloom_image[0], &vk.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

			// Phase 9C: capture bloom-side state for the `gfxinfo`
			// HDR pipeline report. Replaces the FBO_DEBUG per-restart
			// log from 9B with on-demand reporting.
			vk_hdr_state.bloom_enabled       = r_bloom->integer;
			vk_hdr_state.bloom_passes_active = r_bloomPasses->integer;
			vk_hdr_state.bloom_mip_max       = VK_NUM_BLOOM_PASSES;

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
		}

		// post-processing/msaa-resolve
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// Phase 6B3'-c1: tonemap output (LDR-range, but still linear-light).
		// Same dimensions as color_image (full vid resolution). Phase
		// 6B3'-d4-Block-5a: format is vk.color_format (R16F under r_hdr 1,
		// R16_UNORM under r_hdr 2, base 8-bit when r_hdr 0 / r_fbo 0) —
		// matches color_image. An 8-bit UNORM intermediate quantized linear
		// values below ~1/255 to zero (dark UI fills, dark scene radiance);
		// the wider format preserves them into the gamma pass. Also the
		// d8/HDR10 prerequisite. Read by the gamma and capture passes
		// downstream (sampling a wider format is transparent to them).
		// Block 8: TRANSFER_SRC_BIT — vk_smaa() copies this image into
		// vk.smaa.input_image before edge/blend/resolve (resolve writes
		// back here via vk.framebuffers.smaa_resolve).
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			&vk.tonemapped_image, &vk.tonemapped_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		ri.Log( SEV_TRACE, "[FBO_TRACE] FBO color attachment created:\n" );
		ri.Log( SEV_TRACE, "[FBO_TRACE]   size=%dx%d  format=%d  layout=SHADER_READ_ONLY\n",
			glConfig.vidWidth, glConfig.vidHeight, vk.color_format );
		ri.Log( SEV_TRACE, "[FBO_TRACE]   usage=%d (COLOR|SAMPLED|TRANSFER_SRC)\n",
			(int)(usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) );

		// screenmap color
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// screenmap depth
		create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, &vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue );

		if ( r_ext_supersample->integer ) {
			// capture buffer
			usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,
				usage, &vk.capture.image, &vk.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse );
		}
	} // if ( vk.fboActive )

	//vk_alloc_attachments();

	create_depth_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, &vk.depth_image, &vk.depth_image_view,
		(vk.fboActive && (r_bloom->integer || vk.depthFade.active)) ? qfalse : qtrue );

	// depth fade: create a non-MSAA depth copy image for sampling in fragment shaders
	if ( vk.depthFade.active ) {
		VkImageCreateInfo img_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;
		VkImageViewCreateInfo view_desc;
		VkSamplerCreateInfo sampler_desc;
		uint32_t mem_type;

		// depth copy image (transfer dst + sampled)
		memset( &img_desc, 0, sizeof( img_desc ) );
		img_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		img_desc.imageType = VK_IMAGE_TYPE_2D;
		img_desc.format = vk.depth_format;
		img_desc.extent.width = glConfig.vidWidth;
		img_desc.extent.height = glConfig.vidHeight;
		img_desc.extent.depth = 1;
		img_desc.mipLevels = 1;
		img_desc.arrayLayers = 1;
		img_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		img_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		img_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		img_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		img_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &img_desc, NULL, &vk.depthFade.image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.depthFade.image, &mem_req );

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = mem_req.size;
		mem_type = find_memory_type( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		alloc_info.memoryTypeIndex = mem_type;

		// allocate dedicated memory for this image
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.depthFade.memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.depthFade.image, vk.depthFade.memory, 0 ) );

		// image view (depth-only aspect)
		memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.depthFade.image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = vk.depth_format;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.depthFade.view ) );

		// nearest-neighbor sampler
		memset( &sampler_desc, 0, sizeof( sampler_desc ) );
		sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_desc.magFilter = VK_FILTER_NEAREST;
		sampler_desc.minFilter = VK_FILTER_NEAREST;
		sampler_desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &vk.depthFade.sampler ) );

		SET_OBJECT_NAME( vk.depthFade.image, "depth fade copy image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.depthFade.view, "depth fade copy view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

#if FEAT_SHADOW_MAPPING
	vk_shadow_alloc_resources();
#endif

	// Phase 6B3'-f split-A3: SMAA intermediates were here. They now live
	// on dedicated VkDeviceMemory via vk_smaa_alloc_resources, called
	// below after vk_alloc_attachments. Pulling them out of the
	// attachment pool is what makes live r_smaa release possible.

	vk_alloc_attachments();

	for ( i = 0; i < vk.image_memory_count; i++ )
	{
		SET_OBJECT_NAME( vk.image_memory[i], va( "framebuffer memory chunk %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	}

	SET_OBJECT_NAME( vk.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.depth_image_view, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ )
	{
		SET_OBJECT_NAME( vk.bloom_image[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.bloom_image_view[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	// Phase 6B3'-f split-A3: SMAA images / LUTs / samplers + LUT upload
	// all live in vk_smaa_alloc_resources now. Allocate when the FBO is
	// on and the user wants SMAA; the helper is idempotent so calling
	// it twice (e.g. cold start + r_fbo rebuild while already allocated)
	// is safe.
	if ( vk.fboActive && r_smaa->integer > 0 ) {
		vk_smaa_alloc_resources();
	}
}


static void vk_create_framebuffers( void )
{
	VkImageView attachments[3];
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.layers = 1;

	for ( n = 0; n < vk.swapchain_image_count; n++ )
	{
		desc.renderPass = vk.render_pass.main;
		desc.attachmentCount = 2;
		if ( r_fbo->integer == 0 )
		{
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			attachments[0] = vk.swapchain_image_views[n];
			attachments[1] = vk.depth_image_view;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.main[n], va( "framebuffer - main %i", n ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
		else
		{
			// same framebuffer configuration for main and post-bloom render passes
			if ( n == 0 )
			{
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				attachments[0] = vk.color_image_view;
				attachments[1] = vk.depth_image_view;
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main[n] ) );
				SET_OBJECT_NAME( vk.framebuffers.main[n], "framebuffer - main", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
			else
			{
				vk.framebuffers.main[n] = vk.framebuffers.main[0];
			}

			// gamma correction
			desc.renderPass = vk.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			attachments[0] = vk.swapchain_image_views[n];
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.gamma[n] ) );

			SET_OBJECT_NAME( vk.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			// Phase 6B3'-c1: tonemap framebuffer is a single image (not
			// per-swapchain) — create once on the first iteration.
			if ( n == 0 && vk.tonemapped_image_view != VK_NULL_HANDLE )
			{
				desc.renderPass = vk.render_pass.tonemap;
				desc.attachmentCount = 1;
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				attachments[0] = vk.tonemapped_image_view;
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.tonemap ) );
				SET_OBJECT_NAME( vk.framebuffers.tonemap, "framebuffer - tonemap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

				// Phase 6B3'-d4-Block-5b: UI compositing framebuffer.
				// Color = tonemapped_image (img 265, LOADed from tonemap
				// output); depth = main depth_image stub (DONT_CARE on
				// both ends, for render-pass pipeline compatibility).
				desc.renderPass = vk.render_pass.ui;
				desc.attachmentCount = 2;
				attachments[0] = vk.tonemapped_image_view;
				attachments[1] = vk.depth_image_view;
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ui ) );
				SET_OBJECT_NAME( vk.framebuffers.ui, "framebuffer - ui", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

				// Block 8 (Delta 2): same attachments, render_pass.ui_clear.
				desc.renderPass = vk.render_pass.ui_clear;
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.ui_clear ) );
				SET_OBJECT_NAME( vk.framebuffers.ui_clear, "framebuffer - ui_clear", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}
	}

	if ( vk.fboActive )
	{
		// screenmap
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		attachments[0] = vk.screenMap.color_image_view;
		attachments[1] = vk.screenMap.depth_image_view;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		if ( vk.capture.image != VK_NULL_HANDLE )
		{
			attachments[0] = vk.capture.image_view;

			desc.renderPass = vk.render_pass.capture;
			desc.pAttachments = attachments;
			desc.attachmentCount = 1;
			desc.width = gls.captureWidth;
			desc.height = gls.captureHeight;

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.capture ) );
			SET_OBJECT_NAME( vk.framebuffers.capture, "framebuffer - capture", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}

		if ( r_bloom->integer )
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			// bloom color extraction
			desc.renderPass = vk.render_pass.bloom_extract;
			desc.width = width;
			desc.height = height;

			desc.attachmentCount = 1;
			attachments[0] = vk.bloom_image_view[0];

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.bloom_extract ) );

			SET_OBJECT_NAME( vk.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
			{
				width /= 2;
				height /= 2;

				desc.renderPass = vk.render_pass.blur[n];
				desc.width = width;
				desc.height = height;

				desc.attachmentCount = 1;

				attachments[0] = vk.bloom_image_view[n+0+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

				attachments[0] = vk.bloom_image_view[n+1+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

				SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
				SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}

		// SMAA framebuffers
		if ( vk.smaa.active ) {
			// Phase 7.4c-pipeline-followup-5 PART 2.7 — edge/blend FB
			// creation moved into vk_smaa_create_framebuffers() so the
			// live r_smaa 0→1 toggle path can rebuild them after
			// vk_smaa_alloc_resources recreates the underlying views.
			// Idempotent helper: on this cold-start call, render passes
			// are now live (vk_create_render_passes ran before us) so the
			// helper actually builds the FBs.
			vk_smaa_create_framebuffers();

			// smaa_resolve attaches vk.tonemapped_image_view — a non-
			// SMAA-owned view that survives r_smaa toggle (its lifecycle
			// follows r_hdr / r_fbo, not r_smaa). Kept inline here.
			// Block 8: SMAA resolve writes the tonemapped image (img 265)
			// instead of the pre-tonemap HDR scene (img 264).
			// render_pass.smaa_resolve is created with vk.color_format,
			// which == tonemapped_image's format, and initial/final
			// layout SHADER_READ_ONLY_OPTIMAL — matching render_pass.ui's
			// initialLayout for the UI pass that follows.
			desc.attachmentCount = 1;
			desc.width = glConfig.vidWidth;
			desc.height = glConfig.vidHeight;
			attachments[0] = vk.tonemapped_image_view;
			desc.renderPass = vk.render_pass.smaa_resolve;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.smaa_resolve ) );
			SET_OBJECT_NAME( vk.framebuffers.smaa_resolve, "framebuffer - smaa_resolve", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}
}


static void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.image_uploaded2 ) );
	// Phase 7.4c-submit-followup-staging — adopt the just-created VkSemaphore
	// into a ralSemaphore_t sibling so the vk_flush_staging_buffer submit can
	// pass it through ralSubmitInfo_t.signalSemaphores. ownsSemaphore=qfalse
	// on the wrapper; the existing qvkDestroySemaphore in vk_destroy_sync_
	// primitives retains lifetime ownership of the underlying handle.
	vk.ral_image_uploaded2 = Ral_AdoptSemaphore( vk_ral_get_backend(),
		(void *)vk.image_uploaded2, RAL_SEMAPHORE_BINARY, "wired-image-uploaded2" );
#endif

	// all commands submitted
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		// swapchain image acquired
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );
		// Phase 7.4c-submit-followup-present-1 — per-frame ring adoption of
		// image_acquired. Consumed by -2's atomic switchover as the signalSem
		// arg to Ral_AcquireNextImage. Dormant this turn (renderer still
		// calls qvkAcquireNextImageKHR with the legacy VkSemaphore at
		// vk.c:19312); the adopted wrapper exists so the next turn's call-
		// site flip is a 1-line edit.
		vk.tess[i].ral_image_acquired = Ral_AdoptSemaphore( vk_ral_get_backend(),
			(void *)vk.tess[i].image_acquired, RAL_SEMAPHORE_BINARY,
			"wired-frame-image-acquired" );

#ifdef USE_UPLOAD_QUEUE
		// second semaphore to synchronize additional tasks (e.g. image upload)
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].rendering_finished2 ) );
		// Phase 7.4c-submit-followup-staging — per-frame ring adoption of the
		// rendering_finished2 semaphore. The vk.rendering_finished alias flip
		// at the per-frame render submit site (vk.c:19613) sets
		// vk.ral_rendering_finished = vk.tess[cmd_index].ral_rendering_finished2
		// alongside the legacy alias.
		vk.tess[i].ral_rendering_finished2 = Ral_AdoptSemaphore( vk_ral_get_backend(),
			(void *)vk.tess[i].rendering_finished2, RAL_SEMAPHORE_BINARY,
			"wired-frame-rendering-finished2" );
#endif
		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		//fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so it can be used to start rendering
		fence_desc.flags = 0; // non-signalled state

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		// Phase 7.4c-submit-BC-C-final — adopt the per-frame render fence so
		// the migrated Ral_Submit at vk_end_frame can pass it via
		// ralSubmitInfo_t.signalFence. ownsFence=qfalse → underlying VkFence
		// stays owned by the existing qvkCreateFence/qvkDestroyFence pair.
		vk.tess[i].ral_rendering_finished_fence = Ral_AdoptFence( vk_ral_get_backend(),
			(void *)vk.tess[i].rendering_finished_fence, "wired-frame-rendering-finished-fence" );
		vk.tess[i].waitForFence = qfalse;

		SET_OBJECT_NAME( vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME( vk.tess[i].rendering_finished2, va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME( vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.aux_fence ) );
	SET_OBJECT_NAME( vk.aux_fence, "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	// Phase 7.4c-submit-followup-staging — adopt aux_fence into a ralFence_t
	// sibling for Ral_Submit's signalFence + Ral_WaitFence in vk_wait_staging_
	// buffer / vk_flush_staging_buffer's intermediate-flush branch.
	vk.ral_aux_fence = Ral_AdoptFence( vk_ral_get_backend(), (void *)vk.aux_fence, "wired-aux-fence" );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.ral_rendering_finished = NULL;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.ral_image_uploaded = NULL;
	vk.aux_fence_wait = qfalse;
#endif
}


static void vk_destroy_sync_primitives( void  ) {
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	// Phase 7.4c-submit-followup-staging — destroy adopted RAL wrappers BEFORE
	// the underlying VkSemaphore/VkFence. ownsX=qfalse short-circuits the
	// underlying destroy (BC-C-min Cluster I behavior), so only the wrapper
	// struct is freed here; the qvkDestroySemaphore/qvkDestroyFence below
	// retains lifetime ownership of the native handle.
	if ( vk.ral_image_uploaded2 ) { Ral_DestroySemaphore( vk.ral_image_uploaded2 ); vk.ral_image_uploaded2 = NULL; }
	qvkDestroySemaphore( vk.device, vk.image_uploaded2, NULL );
#endif

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		// Phase 7.4c-submit-followup-present-1 — destroy adopted RAL wrapper
		// BEFORE the underlying VkSemaphore. ownsSemaphore=qfalse on the
		// adopted wrapper, so only the wrapper struct is freed here.
		if ( vk.tess[i].ral_image_acquired ) { Ral_DestroySemaphore( vk.tess[i].ral_image_acquired ); vk.tess[i].ral_image_acquired = NULL; }
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
#ifdef USE_UPLOAD_QUEUE
		if ( vk.tess[i].ral_rendering_finished2 ) { Ral_DestroySemaphore( vk.tess[i].ral_rendering_finished2 ); vk.tess[i].ral_rendering_finished2 = NULL; }
		qvkDestroySemaphore( vk.device, vk.tess[i].rendering_finished2, NULL );
#endif
		// Phase 7.4c-submit-BC-C-final — destroy adopted RAL wrapper BEFORE
		// the underlying VkFence. ownsFence=qfalse on the adopted wrapper,
		// so this is wrapper-only free; the qvkDestroyFence below retains
		// lifetime ownership of the native handle.
		if ( vk.tess[i].ral_rendering_finished_fence ) {
			Ral_DestroyFence( vk.tess[i].ral_rendering_finished_fence );
			vk.tess[i].ral_rendering_finished_fence = NULL;
		}
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk.ral_aux_fence ) { Ral_DestroyFence( vk.ral_aux_fence ); vk.ral_aux_fence = NULL; }
	qvkDestroyFence( vk.device, vk.aux_fence, NULL );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.ral_rendering_finished = NULL;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.ral_image_uploaded = NULL;
#endif
}


static void vk_destroy_framebuffers( void ) {
	uint32_t n;

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.main[n] != VK_NULL_HANDLE ) {
			if ( !vk.fboActive || n == 0 ) {
				qvkDestroyFramebuffer( vk.device, vk.framebuffers.main[n], NULL );
			}
			vk.framebuffers.main[n] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.tonemap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.tonemap, NULL );
		vk.framebuffers.tonemap = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ui != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ui, NULL );
		vk.framebuffers.ui = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.ui_clear != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.ui_clear, NULL );
		vk.framebuffers.ui_clear = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.bloom_extract, NULL );
		vk.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.capture != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.capture, NULL );
		vk.framebuffers.capture = VK_NULL_HANDLE;
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

	// Phase 7.4c-pipeline-followup-5 PART 2.7 — edge/blend FB destruction
	// moved into vk_smaa_destroy_framebuffers() so the live r_smaa 1→0
	// toggle path can tear them down alongside the views they reference.
	// Idempotent: vk_smaa_release_resources runs the helper first on
	// live toggle, so by the time vk_destroy_framebuffers runs at full
	// teardown the helper is a NULL-check no-op.
	vk_smaa_destroy_framebuffers();
	if ( vk.framebuffers.smaa_resolve != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.smaa_resolve, NULL );
		vk.framebuffers.smaa_resolve = VK_NULL_HANDLE;
	}
}


static void vk_destroy_swapchain( qboolean preserveRal ) {
	uint32_t i;

	// Phase 7.4c-submit-followup-present-2 — Cluster F. preserveRal=qtrue
	// keeps vk.ral_swapchain alive across the destroy so the next
	// vk_create_swapchain can pass its VkSwapchainKHR through
	// oldExternalSwapchain for atomic handoff (recreate paths:
	// vk_restart_swapchain, vk_rebuild_for_fbo_change). preserveRal=qfalse
	// is the fresh-teardown path (vk_shutdown): destroy the wrapper here.
	// vk.swapchain itself is just an alias of the RAL-owned handle — no
	// separate qvkDestroySwapchainKHR call: Ral_DestroySwapchain owns
	// destruction when preserveRal=qfalse; the new vkCreateSwapchainKHR
	// inside Ral_CreateSwapchain handles the retired-via-oldSwapchain case
	// when preserveRal=qtrue.
	if ( !preserveRal && vk.ral_swapchain ) {
		Ral_DestroySwapchain( vk.ral_swapchain );
		vk.ral_swapchain = NULL;
	}

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		if ( vk.swapchain_image_views[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.swapchain_image_views[i], NULL );
			vk.swapchain_image_views[i] = VK_NULL_HANDLE;
		}
		// Phase 7.4c-submit-followup-present-1 — adopted wrapper goes
		// BEFORE underlying VkSemaphore. ownsSemaphore=qfalse → wrapper-
		// only free; the qvkDestroySemaphore below owns the native handle.
		if ( vk.ral_swapchain_rendering_finished[i] ) {
			Ral_DestroySemaphore( vk.ral_swapchain_rendering_finished[i] );
			vk.ral_swapchain_rendering_finished[i] = NULL;
		}
		if ( vk.swapchain_rendering_finished[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.swapchain_rendering_finished[i], NULL );
			vk.swapchain_rendering_finished[i] = VK_NULL_HANDLE;
		}
	}

	// Phase 7.4c-submit-followup-present-2 — qvkDestroySwapchainKHR retired.
	// vk.swapchain is an alias of vk.ral_swapchain->swapchain (populated by
	// vk_create_swapchain via Ral_GetSwapchainHandle). When preserveRal=qfalse
	// the Ral_DestroySwapchain above destroyed both the wrapper AND the
	// underlying VkSwapchainKHR; we just clear the alias here. When
	// preserveRal=qtrue the next Ral_CreateSwapchain in vk_create_swapchain
	// will retire this swapchain via oldExternalSwapchain handoff — DO NOT
	// destroy the handle here in that case.
	vk.swapchain = VK_NULL_HANDLE;
	memset( vk.swapchain_images, 0, sizeof( vk.swapchain_images ) );
	vk.swapchain_image_count = 0;
}

static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );

static void vk_restart_swapchain( const char *funcname, VkResult res )
{
	uint32_t i;

#ifdef _DEBUG
	ri.Log( SEV_WARN, "%s(%s): restarting swapchain...\n", funcname, vk_result_string( res ) );
#else
	ri.Log( SEV_WARN, "%s(): restarting swapchain...\n", funcname );
#endif

	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
	}

#ifdef USE_UPLOAD_QUEUE
	// Flush any pending staging-upload batch (texture layout transitions + copies
	// already recorded into vk.staging_command_buffer, offset != 0) BEFORE
	// resetting the cb: a bare qvkResetCommandBuffer would DISCARD those recorded
	// commands, leaving the partially-uploaded images stuck in
	// VK_IMAGE_LAYOUT_UNDEFINED (VUID-vkCmdDraw-None-09600 on the next sample) and
	// desyncing the "offset != 0 ⇒ staging cb is recording" invariant that
	// vk_upload_image relies on (VUID-vkCmdPipelineBarrier-commandBuffer-recording).
	// vk_flush_staging_buffer( qfalse ) submits the batch, waits, resets the cb,
	// and zeroes offset; it is a no-op when nothing is pending. The caller already
	// did vk_wait_idle(), so the submit + fence wait inside it are legal.
	vk_flush_staging_buffer( qfalse );
	qvkResetCommandBuffer( vk.staging_command_buffer, 0 );
#endif

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain( qtrue );   // Phase 7.4c-submit-followup-present-2: preserveRal=qtrue — out-of-date recovery recreate path; atomic-handoff via oldExternalSwapchain at next create.
	vk_destroy_sync_primitives();

	vk_select_surface_format( vk.physical_device, vk_surface );
	setup_surface_formats( vk.physical_device );

	vk_create_sync_primitives();
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qfalse );
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	vk_update_post_process_pipelines();
}


static void vk_set_render_scale( void )
{
	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect )
				{
					float scale = (float)gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					vk.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					vk.blitY0 += bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				vk.blitFilter = GL_LINEAR;
			else
				vk.blitFilter = GL_NEAREST;
		}

		vk.windowAdjusted = qtrue;
	}

	if ( r_fbo->integer && r_ext_supersample->integer && !r_renderScale->integer )
	{
		vk.blitFilter = GL_LINEAR;
	}
}


static void vk_fence_thread_start( void );
static void vk_fence_thread_stop( void );
static void vk_gpu_ts_init( void );
static void vk_gpu_ts_shutdown( void );
static void vk_gpu_ts_write( const char *label );

void vk_initialize( void )
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	VkPhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t maxSize;
	uint32_t i;

	init_vulkan_library();

	vk.instance = vk_instance;   // Phase 7.4c-pre: expose the file-static VkInstance to vk_ral_textures.c (imported-mode bringup)
	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );

	// Phase 7.4c-submit-followup-present-2-fix2 — hoist RAL backend bringup
	// ahead of every consumer. present-2 turned vk_create_swapchain (called
	// later in this function at the swapchain block) into a hard RAL
	// consumer: Ral_CreateSwapchain dereferences the backend pointer, and
	// the previous late vk_ral_textures_init position left s_ral_backend
	// NULL at swapchain-create time → silent boot crash on every config.
	// Earlier consumers (vk_create_sync_primitives's per-frame Ral_Adopt*
	// calls) also pick up real wrappers now instead of silently-NULL ones.
	if ( !vk_ral_backend_init() ) {
		ri.Terminate( TERM_UNRECOVERABLE, "vk_initialize: RAL backend bringup failed" );
	}

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.cmd = vk.tess + 0;
	vk.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	vk.uniform_item_size = PAD( (uint32_t)sizeof( vkUniform_t ), vk.uniform_alignment );

	// for flare visibility tests
	vk.storage_alignment = MAX( props.limits.minStorageBufferOffsetAlignment, sizeof( uint32_t ) );

	vk.maxAnisotropy = props.limits.maxSamplerAnisotropy;

	vk.timestampPeriodNs  = props.limits.timestampPeriod;
	vk.timestampSupported = ( props.limits.timestampComputeAndGraphics && vk.timestampPeriodNs > 0.0f ) ? qtrue : qfalse;

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;

	vk_set_render_scale();

	if ( r_fbo->integer ) {
		vk.fboActive = qtrue;
	} else {
		vk.fboActive = qfalse;
	}
	// Phase 6B3'-d4-Block-5b-prereq: MSAA retired (SMAA + Phase 7 TAA cover
	// AA needs; r_ext_multisample cvar removed). vk.msaaActive stays at its
	// initialized qfalse permanently — the dead-but-present branches below
	// (render_pass.main 3-attachment-with-resolve variant, vk.msaa_image
	// allocation, etc.) are deleted alongside this retire.

	// depth fade requires FBO for the depth copy — also needed for SSAO and god rays
	vk.depthFade.active = ( vk.fboActive && ( r_depthFade->integer
#if FEAT_SSAO
		|| r_ssao->integer
#endif
#if FEAT_GODRAYS
		|| r_godRays->integer
#endif
	) ) ? qtrue : qfalse;

#if FEAT_SHADOW_MAPPING
	{
		cvar_t *r_shadowMapping = ri.Cvar_Get( "r_shadowMapping", "0", 0 );
		cvar_t *r_shadowMapSize = ri.Cvar_Get( "r_shadowMapSize", "2048", 0 );
		vk.shadowMap.active = ( r_shadowMapping->integer && vk.fboActive ) ? qtrue : qfalse;
		vk.shadowMap.size = r_shadowMapSize->integer;
		if ( vk.shadowMap.size < 512 )  vk.shadowMap.size = 512;
		if ( vk.shadowMap.size > 4096 ) vk.shadowMap.size = 4096;
	}
#endif

	// Phase 6B3'-f split-A3: option (ii) memory-release SMAA. vk.smaa.active
	// is owned by vk_smaa_alloc_resources (sets qtrue) and
	// vk_smaa_release_resources (sets qfalse). r_smaa = 0 at boot leaves
	// .active false and no SMAA Vulkan resources allocated; r_smaa > 0
	// at boot has vk_create_attachments call alloc, which flips .active
	// to qtrue. Live toggling routes through vk_update_post_process_pipelines.
	vk.smaa.active = qfalse;
	vk.smaa.quality = r_smaa->integer;
	if ( vk.fboActive && r_smaa->integer > 0 ) {
		const char *names[] = { "", "Low", "Medium", "High", "Ultra" };
		ri.Log( SEV_INFO, "...SMAA: %s quality\n", names[vk.smaa.quality] );
	}

	// multisampling — Phase 6B3'-d4-Block-5b-prereq: MSAA retired. vkSamples
	// and vk.screenMapSamples are now constant VK_SAMPLE_COUNT_1_BIT. The
	// vkMaxSamples query is dead but cheap — kept for diagnostic completeness.

	vkMaxSamples = MIN( props.limits.sampledImageColorSampleCounts, props.limits.sampledImageDepthSampleCounts );

	vkSamples = VK_SAMPLE_COUNT_1_BIT;

	vk.screenMapSamples = VK_SAMPLE_COUNT_1_BIT;

	vk.screenMapWidth = (float) glConfig.vidWidth / 16.0;
	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	vk.screenMapHeight = (float) glConfig.vidHeight / 16.0;
	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

	vk.defaults.geometry_size = VERTEX_BUFFER_SIZE;
	vk.defaults.staging_size = STAGING_BUFFER_SIZE;

	// get memory size & defaults
	{
		VkPhysicalDeviceMemoryProperties props;
		VkDeviceSize maxDedicatedSize = 0;
		VkDeviceSize maxBARSize = 0;
		qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &props );
		for ( i = 0; i < props.memoryTypeCount; i++ ) {
			if ( props.memoryTypes[i].propertyFlags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				if ( maxDedicatedSize == 0 || props.memoryHeaps[props.memoryTypes[i].heapIndex].size > maxDedicatedSize ) {
					maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
			if ( props.memoryTypes[i].propertyFlags == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				if ( maxBARSize == 0 ) {
					maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
		}

		if ( maxDedicatedSize != 0 ) {
			ri.Log( SEV_INFO, "...device memory size: %iMB\n", (int)((maxDedicatedSize + (1024 * 1024) - 1) / (1024 * 1024)) );
		}
		if ( maxBARSize != 0 ) {
			if ( maxBARSize >= 128 * 1024 * 1024 ) {
				// user larger buffers to avoid potential reallocations
				vk.defaults.geometry_size = VERTEX_BUFFER_SIZE_HI;
				vk.defaults.staging_size = STAGING_BUFFER_SIZE_HI;
			}
#ifdef _DEBUG
			ri.Log( SEV_INFO, "...BAR memory size: %iMB\n", (int)((maxBARSize + (1024 * 1024) - 1) / (1024 * 1024)) );
#endif
		}
	}

	// fill glConfig information

	// maxTextureSize must not exceed IMAGE_CHUNK_SIZE
	maxSize = sqrtf( IMAGE_CHUNK_SIZE / 4 );
	// round down to next power of 2
	glConfig.maxTextureSize = MIN( props.limits.maxImageDimension2D, log2pad( maxSize, 0 ) );

	if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk.maxLod = 1 + Q_log2( glConfig.maxTextureSize );

	if ( props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF )
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if ( glConfig.numTextureUnits > MAX_TEXTURE_UNITS )
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = qtrue;
	else
		glConfig.textureEnvAddAvailable = qfalse;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch ( props.vendorID ) {
		case 0x10DE: // NVidia
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i.%i",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
#ifdef _WIN32
		case 0x8086: // Intel
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i",
				(props.driverVersion >> 14),
				(props.driverVersion >> 0) & 0x3FFF );
			break;
#endif
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
	}

	Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ), "API: %i.%i.%i, Driver: %s",
		major, minor, patch, driver_version );

#ifdef _WIN32
	// Intel iGPU drivers from 101.5333 to 101.6737 have a known bug that causes
	// VK_ERROR_DEVICE_LOST during vkQueueSubmit, see https://github.com/ec-/Quake3e/issues/312
	if ( props.vendorID == 0x8086 ) {
		uint32_t drvMajor = props.driverVersion >> 14;
		uint32_t drvMinor = props.driverVersion & 0x3FFF;
		if ( drvMajor == 101 && drvMinor >= 5333 && drvMinor <= 6737 ) {
			Com_sprintf( vk.driverNote, sizeof( vk.driverNote ), S_COLOR_WARNING
				"\nWARNING: Intel driver %i.%i is known to cause Vulkan crashes.\n"
				"Consider updating to driver >= 101.6790 or downgrading to <= 101.5186.\n",
				drvMajor, drvMinor );
		}
	}
#endif

	vk.offscreenRender = qtrue;

	if ( props.vendorID == 0x1002 ) {
		vendor_name = "Advanced Micro Devices, Inc.";
	} else if ( props.vendorID == 0x106B ) {
		vendor_name = "Apple Inc.";
	} else if ( props.vendorID == 0x10DE ) {
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk.offscreenRender = qfalse;
		vendor_name = "NVIDIA";
	} else if ( props.vendorID == 0x14E4 ) {
		vendor_name = "Broadcom Inc.";
	} else if ( props.vendorID == 0x1AE0 ) {
		vendor_name = "Google Inc.";
	} else if ( props.vendorID == 0x8086 ) {
		vendor_name = "Intel Corporation";
	} else if ( props.vendorID == VK_VENDOR_ID_MESA ) {
		vendor_name = "MESA";
	} else {
		Com_sprintf( buf, sizeof( buf ), "VendorID: %04x", props.vendorID );
		vendor_name = buf;
	}

	Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, renderer_name( &props ), sizeof( glConfig.renderer_string ) );

	SET_OBJECT_NAME( (intptr_t)vk.device, glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT );

	// do early texture mode setup to avoid redundant descriptor updates in GL_SetDefaultState()
	vk.samplers.filter_min = -1;
	vk.samplers.filter_max = -1;
	GL_TextureMode( r_textureMode->string );

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, &vk.command_pool ) );

		SET_OBJECT_NAME( vk.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	// Phase 7.4c-submit-followup-staging — persistent staging command buffer
	// acquired once via the RAL backend's graphics pool. Returned in IDLE
	// state (Ral_AcquireCommandBuffer does NOT auto-begin — Ral_AcquireBegunCommandBuffer
	// is the begin-on-acquire variant, used for one-shot buffers only).
	// ownsBuffer=qtrue so Ral_DestroyCommandBuffer at teardown frees the
	// underlying VkCommandBuffer back to the RAL pool. The legacy
	// vk.staging_command_buffer VkCommandBuffer field is repurposed as an
	// alias to the RAL-allocated buffer's handle — the 30+ existing
	// qvkBegin/End/Reset/Cmd* sites on this field operate transparently
	// (state-tracking on ral_staging_cmd stays as RAL_VK_CMD_IDLE/SUBMITTED;
	// we deliberately bypass Ral_Begin/End/Reset to avoid double-tracking).
	{
		vk.ral_staging_cmd = Ral_AcquireCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( vk.ral_staging_cmd == NULL ) {
			ri.Terminate( TERM_UNRECOVERABLE, "vk_initialize: Ral_AcquireCommandBuffer(GRAPHICS) returned NULL for staging cb; RAL backend must be up (r_useRALTextures/r_useRALBuffers/r_useRALPipelines default to 1)" );
		}
		vk.staging_command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( vk.ral_staging_cmd );
		SET_OBJECT_NAME( vk.staging_command_buffer, "staging command buffer (RAL)", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}
#endif

	//
	// Command buffers and color attachments.
	//
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].command_buffer ) );

		//SET_OBJECT_NAME( vk.tess[i].command_buffer, va( "command buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

	//
	// Descriptor pool.
	//
	{
		VkDescriptorPoolSize pool_size[5];
		VkDescriptorPoolCreateInfo desc;
		uint32_t i, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		// MAX_DRAWIMAGES per-image samplers + 9 post-process / FBO-side
		// descriptors + 384 for the three primitive sampler arrays
		// (particle binding 3, ribbon binding 2, beam binding 1 — each
		// 64 slots × NUM_COMMAND_BUFFERS (= 2) descriptor sets = 128
		// entries, three primitives = 384). Without the explicit primitive
		// budget those arrays would silently eat into MAX_DRAWIMAGES
		// headroom and pool exhaustion could fire on heavy maps that load
		// close to MAX_DRAWIMAGES images.
		//
		// The 9 post-process descriptors map to vkAllocateDescriptorSets
		// calls in the post-process descriptor block (~line 2771): each
		// `+ 1` term below corresponds to one sampler descriptor for one
		// VkImageView, allocated unconditionally or under the matching
		// FEAT_* / boolean guard at the alloc site.
		pool_size[0].descriptorCount = MAX_DRAWIMAGES
		                             + 1 /* vk.color_descriptor — main scene HDR sampler */
		                             + 1 /* vk.tonemapped_descriptor — LDR-linear scene (Phase 6B3'-c1) */
		                             + 1 /* vk.screenMap.color_descriptor — screenmap (env capture) */
		                             + 1 /* vk.depthFade.descriptor — depth-fade post pass */
		                             + VK_NUM_BLOOM_PASSES * 2 /* vk.bloom_image_descriptor[i] — ping-pong per pass */
		                             + 1 /* vk.shadowMap.descriptor — FEAT_SHADOW_MAPPING */
		                             + 5 /* vk.smaa.{edges,blend,input,area,search}_descriptor */
		                             + 384;

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS;

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1 + NUM_COMMAND_BUFFERS; // generic storage + reserved per-frame slots

		pool_size[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		// First-launch consumption (ribbon, sprite, particle, beam
		// each double-allocate — once in vk_init_*, once in
		// vk_init_descriptors's re-alloc block; a vid_restart skips
		// the first allocation since vk.active stays true and
		// vk_initialize doesn't re-run):
		//   ribbon:   2 SSBOs × 2 cmd × 2 alloc =  8
		//   sprite:   1 SSBO  × 2 cmd × 2 alloc =  4
		//   particle: ~10 SSBOs across compute+render × 2 alloc ≈ 20
		//   beam:     3 SSBOs (header + stages + counts) × 2 cmd × 2 alloc = 12
		//   total: ≈ 44.
		// vid_restart-only path: ≈ 22 (single allocation).
		// 64-slot pool gives ~31% headroom (44/64) post-Phase 5G; the
		// underlying double-alloc waste is tracked in docs/health.md
		// and deferred to the RAL refactor that will rebuild
		// descriptor management entirely.
		pool_size[3].descriptorCount = 64;

		// IQM GPU skinning: per-frame bone matrix UBOs (non-dynamic uniform buffers)
		// Particle subsystem also uses non-dynamic UBOs for per-frame
		// state (NUM_COMMAND_BUFFERS slots × 2 descriptor sets [compute
		// and render] = 4). Total: NUM_COMMAND_BUFFERS for IQM + 4 for
		// particle, with headroom.
		pool_size[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size[4].descriptorCount = NUM_COMMAND_BUFFERS + 8;

		for ( i = 0, maxSets = 0; i < ARRAY_LEN( pool_size ); i++ ) {
			maxSets += pool_size[i].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	//
	// Descriptor set layout.
	//
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_sampler );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_uniform );
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_storage );
	//vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_input );
	// Phase 7.4c-bindgroup-pre: RAL adoption of these three layouts lives at
	// the END of vk_initialize, AFTER vk_ral_textures_init brings up the
	// imported-mode backend. See vk_ral_adopt_descriptor_layouts call below.

	//
	// Pipeline layouts.
	//
	{
		VkDescriptorSetLayout set_layouts[8]; // sized for potential parallax set 6
		VkPipelineLayoutCreateInfo desc;
		VkPushConstantRange push_range;
#if FEAT_FOG_SYSTEM
		VkPushConstantRange push_ranges[2];
#endif

		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = 64; // 16 floats

#if FEAT_FOG_SYSTEM
		// Range 0: MVP matrix (vertex stage, bytes 0-63)
		push_ranges[0] = push_range;
		// Range 1: Enhanced fog parameters (fragment stage, bytes 64-95).
		// Consumed by vk_update_fog_push; reserved even if no shader
		// currently reads it so the layout is forward-compatible.
		push_ranges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_ranges[1].offset = 64;
		push_ranges[1].size = 32;
#endif

		// standard pipelines
		set_layouts[0] = vk.set_layout_uniform; // fog/dlight parameters
		set_layouts[1] = vk.set_layout_sampler; // diffuse
		set_layouts[2] = vk.set_layout_sampler; // lightmap / fog-only
		set_layouts[3] = vk.set_layout_sampler; // blend
		set_layouts[4] = vk.set_layout_sampler; // collapsed fog texture
		set_layouts[5] = vk.set_layout_sampler; // depth fade texture
		set_layouts[6] = vk.set_layout_sampler; // normalmap (parallax) or Q1 anim next-frame
		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		{
			int layoutCount = (vk.maxBoundDescriptorSets >= VK_DESC_COUNT) ? VK_DESC_COUNT : 4;
			if ( vk.maxBoundDescriptorSets >= 7 )
				layoutCount = 7; // set=6: normalmap (parallax) or Q1 anim next-frame sampler
			desc.setLayoutCount = layoutCount;
		}
		desc.pSetLayouts = set_layouts;
#if FEAT_FOG_SYSTEM
		desc.pushConstantRangeCount = 2;
		desc.pPushConstantRanges = push_ranges;
#else
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;
#endif

		VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));

		// MSDF pipeline layout: 128 bytes (64 MVP + 64 outline/glow/shadow params), VERTEX|FRAGMENT
		{
			VkPushConstantRange msdfPush;
			msdfPush.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			msdfPush.offset = 0;
			msdfPush.size = 128; // 64 (mat4 mvp) + 4 (outlineWidth) + 4 (glowWidth) + 8 (shadowOffset) + 16 (outlineColor) + 16 (glowColor) + 16 (shadowColor)

			VkPipelineLayoutCreateInfo msdfLayoutInfo;
			memset(&msdfLayoutInfo, 0, sizeof(msdfLayoutInfo));
			msdfLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			msdfLayoutInfo.setLayoutCount = desc.setLayoutCount;
			msdfLayoutInfo.pSetLayouts = set_layouts;
			msdfLayoutInfo.pushConstantRangeCount = 1;
			msdfLayoutInfo.pPushConstantRanges = &msdfPush;

			VK_CHECK(qvkCreatePipelineLayout(vk.device, &msdfLayoutInfo, NULL, &vk.pipeline_layout_msdf));
			SET_OBJECT_NAME(vk.pipeline_layout_msdf, "pipeline layout: MSDF text", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		}

		// flare test pipeline
		set_layouts[0] = vk.set_layout_storage; // dynamic storage buffer

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_storage ) );

		// post-processing pipeline
		set_layouts[0] = vk.set_layout_sampler; // sampler
		set_layouts[1] = vk.set_layout_sampler; // sampler
		set_layouts[2] = vk.set_layout_sampler; // sampler
		set_layouts[3] = vk.set_layout_sampler; // sampler

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_post_process ) );

		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_blend ) );

#if FEAT_SSAO
		// SSAO pipeline layout: 2 sampler sets (color + depth)
		desc.setLayoutCount = 2;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_ssao ) );
#endif

#if FEAT_GODRAYS
		{
			// Godrays pipeline layout: 2 samplers + push constants for sun position
			VkPushConstantRange godrayPush;
			godrayPush.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			godrayPush.offset = 0;
			godrayPush.size = 16; // vec2 sunScreenPos + float intensity + float decay
			desc.setLayoutCount = 2;
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &godrayPush;
			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_godrays ) );
			desc.pushConstantRangeCount = 0;
			desc.pPushConstantRanges = NULL;
		}
#endif

		// SMAA pipeline layout: 3 sampler sets + push constants (vec4 rtMetrics).
		// Created unconditionally for the same reason the SMAA shader modules are
		// (see vk_create_shader_modules): vk.smaa.active is still qfalse at this
		// point in vk_initialize, and r_smaa is live-toggleable, so the layout has
		// to exist whenever vk_create_smaa_pipelines might run. Destroyed
		// NULL-guarded in vk_shutdown.
		{
			push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			push_range.offset = 0;
			push_range.size = 16; // vec4: 1/w, 1/h, w, h

			set_layouts[0] = vk.set_layout_sampler;
			set_layouts[1] = vk.set_layout_sampler;
			set_layouts[2] = vk.set_layout_sampler;

			desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			desc.pNext = NULL;
			desc.flags = 0;
			desc.setLayoutCount = 3;
			desc.pSetLayouts = set_layouts;
			desc.pushConstantRangeCount = 1;
			desc.pPushConstantRanges = &push_range;

			VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_smaa ) );
			SET_OBJECT_NAME( vk.pipeline_layout_smaa, "pipeline layout - smaa", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		}

		SET_OBJECT_NAME( vk.pipeline_layout, "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_post_process, "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_blend, "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	}

	vk.geometry_buffer_size_new = vk.defaults.geometry_size;
	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	vk_create_storage_buffer( MAX_FLARES * vk.storage_alignment );

	vk_create_shader_modules();

	{
		VkPipelineCacheCreateInfo ci;
		memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
	}

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//vk.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qtrue );

	// color/depth attachments
	vk_create_attachments();

	// renderpasses — must run before vk_init_iqm_gpu_skinning() because it
	// creates a graphics pipeline that reads vk.render_pass.main; with the
	// previous ordering that field was still VK_NULL_HANDLE, tripping
	// VUID-VkGraphicsPipelineCreateInfo-dynamicRendering-06576 on every
	// Debug startup.
	vk_create_render_passes();

	// Primitive ribbon — must be after shader modules, descriptor pool, and
	// render passes (graphics pipelines read vk.render_pass.main).
	vk_init_ribbon();

	// Primitive sprite — same prerequisites as ribbon.
	vk_init_sprite();

	// Primitive particle — same prerequisites as ribbon/sprite, plus
	// uses compute pipeline. Must be after vk_init_sprite() so the
	// descriptor pool sizing already accounts for sprite's slots.
	vk_init_particle();

	// Primitive beam — must be after vk_init_particle() because
	// beam reuses vk.particle.sampler for the binding-1 sampler
	// array. Beam's binding-1 descriptor write is deferred to
	// vk_init_primitive_shader_images (called from R_Init), so the
	// sampler reuse happens at that later point — ordering here is
	// for documentation rather than strict dependency.
	vk_init_beam();

#if FEAT_IQM
	// IQM GPU skinning — must be after shader modules, descriptor pool, AND
	// render passes (graphics pipeline reads vk.render_pass.main).
	vk_init_iqm_gpu_skinning();
#endif

	// framebuffers for each swapchain image
	vk_create_framebuffers();

	// preallocate staging buffer
	if ( vk.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk.defaults.staging_size );
	}

	vk_fence_thread_start();
	vk_gpu_ts_init();

	vk.active = qtrue;

	// Phase 7.4c-pre (Option A): bring up the RAL Vulkan backend in
	// imported mode — it adopts vk.instance/vk.device/queue families so any
	// RAL-allocated resources are usable in the same descriptor writes the
	// renderer already issues. Backend internal allocations now use stdlib
	// malloc/free (PART C), so R_InitImages's ri.FreeAll() can no longer
	// wipe them — initialising here (END of vk_initialize) is safe.
	// vk_ral_textures_init also flushes any vk_initialize-time buffer
	// registrations queued in s_buf_pending earlier in this function.
	vk_ral_textures_init();

	// Phase 7.4c-bindgroup-pre — adopt the three core descriptor-set
	// layouts as ralBindGroupLayout_t so vk_ral_create_pipeline_from_def
	// can declare matching VkPipelineLayouts. Closes the 10 layout-07988
	// VUIDs that 7.4c-pipeline left behind. Gated on r_useRALPipelines +
	// the backend actually being up.
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		ralBackend_t *backend = vk_ral_get_backend();
		if ( backend ) {
			ralBindEntry_t e;
			memset( &e, 0, sizeof( e ) );
			e.binding    = 0;
			e.count      = 1;

			e.type       = RAL_BIND_SAMPLED_TEXTURE;
			e.stageFlags = RAL_STAGE_FRAGMENT;
			vk.ral_bgl_sampler = Ral_AdoptBindGroupLayout( backend, vk.set_layout_sampler, 1, &e, "wired-set-layout-sampler-adopted" );

			e.type       = RAL_BIND_UNIFORM_BUFFER;
			e.stageFlags = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
			vk.ral_bgl_uniform = Ral_AdoptBindGroupLayout( backend, vk.set_layout_uniform, 1, &e, "wired-set-layout-uniform-adopted" );

			e.type       = RAL_BIND_STORAGE_BUFFER;
			e.stageFlags = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
			vk.ral_bgl_storage = Ral_AdoptBindGroupLayout( backend, vk.set_layout_storage, 1, &e, "wired-set-layout-storage-adopted" );

			if ( vk.ral_bgl_sampler && vk.ral_bgl_uniform && vk.ral_bgl_storage ) {
				ri.Log( SEV_INFO, "[VK->RAL] adopted 3 descriptor set layouts as ralBindGroupLayout_t (sampler/uniform/storage)\n" );
			} else {
				ri.Log( SEV_WARN, "[VK->RAL] Ral_AdoptBindGroupLayout partial failure (sampler=%p uniform=%p storage=%p)\n",
				        (void *)vk.ral_bgl_sampler, (void *)vk.ral_bgl_uniform, (void *)vk.ral_bgl_storage );
			}

			// Phase 7.4c-pipeline-followup — adopt the 6 per-subsystem
			// descriptor layouts so the 15 special-case pipeline sites can
			// declare matching VkPipelineLayouts. Entries[] is informational
			// (RAL pipeline-layout creation only needs the VkDescriptorSetLayout
			// handle); we pass a single placeholder STORAGE_BUFFER entry where
			// the exact shape doesn't affect this turn's create-only path.
			{
				ralBindEntry_t ee;
				memset( &ee, 0, sizeof( ee ) );
				ee.binding    = 0;
				ee.count      = 1;
				ee.type       = RAL_BIND_STORAGE_BUFFER;
				ee.stageFlags = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;

				if ( vk.ribbon.set_layout != VK_NULL_HANDLE )
					vk.ribbon.ral_bgl = Ral_AdoptBindGroupLayout( backend, vk.ribbon.set_layout, 1, &ee, "wired-ribbon-set-layout-adopted" );
				if ( vk.beam.set_layout != VK_NULL_HANDLE )
					vk.beam.ral_bgl = Ral_AdoptBindGroupLayout( backend, vk.beam.set_layout, 1, &ee, "wired-beam-set-layout-adopted" );
				if ( vk.sprite.set_layout != VK_NULL_HANDLE )
					vk.sprite.ral_bgl = Ral_AdoptBindGroupLayout( backend, vk.sprite.set_layout, 1, &ee, "wired-sprite-set-layout-adopted" );
				if ( vk.particle.compute_set_layout != VK_NULL_HANDLE )
					vk.particle.ral_bgl_compute = Ral_AdoptBindGroupLayout( backend, vk.particle.compute_set_layout, 1, &ee, "wired-particle-compute-set-layout-adopted" );
				if ( vk.particle.render_set_layout != VK_NULL_HANDLE )
					vk.particle.ral_bgl_render = Ral_AdoptBindGroupLayout( backend, vk.particle.render_set_layout, 1, &ee, "wired-particle-render-set-layout-adopted" );
#if FEAT_IQM
				ee.type = RAL_BIND_UNIFORM_BUFFER;
				if ( vk.iqmGpu.set_layout_bones != VK_NULL_HANDLE )
					vk.iqmGpu.ral_bgl_bones = Ral_AdoptBindGroupLayout( backend, vk.iqmGpu.set_layout_bones, 1, &ee, "wired-iqm-bones-set-layout-adopted" );
#endif
				ri.Log( SEV_INFO, "[VK->RAL] adopted 6 per-subsystem descriptor set layouts (ribbon/beam/sprite/particle-compute/particle-render/iqm-bones)\n" );
			}

			// Phase 7.4c-pipeline-followup — pipeline cache disk load.
			// Path follows §16.7 versioned-filename convention:
			//   <fs_homepath>/<fs_basegame>/pipelinecache_v1_vulkan.bin
			// If the file doesn't exist, Ral_LoadPipelineCache logs SEV_INFO
			// and the backend's empty VkPipelineCache regenerates from scratch
			// during this session. On subsequent boots, the same path seeds
			// the cache for faster create_pipeline warm-up.
			{
				char     cachePath[ MAX_OSPATH ];
				FILE    *probe;
				uint64_t probeSize = 0;
				qboolean probeExists = qfalse;
				const char *home = ri.Cvar_VariableString( "fs_homepath" );
				const char *base = ri.Cvar_VariableString( "fs_basegame" );
				if ( !base || !*base ) base = "baseq3";
				Com_sprintf( cachePath, sizeof( cachePath ), "%s/%s/pipelinecache_v1_vulkan.bin",
				             ( home && *home ) ? home : ".", base );
				// Phase 7.4c-pipeline-followup-5 PART 3+4 — START timing marker.
				// Probe the cache file size BEFORE Ral_LoadPipelineCache so cold-vs-
				// warm determination reflects on-disk reality, not post-load state.
				probe = fopen( cachePath, "rb" );
				if ( probe ) {
					probeExists = qtrue;
					if ( fseek( probe, 0, SEEK_END ) == 0 ) {
						long pos = ftell( probe );
						if ( pos > 0 ) probeSize = (uint64_t)pos;
					}
					fclose( probe );
				}
				vk_pipeline_cache_was_cold     = ( !probeExists || probeSize < RAL_CACHE_WARM_MIN_BYTES );
				vk_pipeline_cache_size_at_load = probeSize;
				vk_pipeline_cache_load_start_ms = ri.Milliseconds();
				ri.Log( SEV_INFO,
					"[7.4c-fu5] pipeline cache load: cold=%d (cache file %s, %llu bytes)\n",
					vk_pipeline_cache_was_cold ? 1 : 0,
					probeExists ? "found" : "absent",
					(unsigned long long)probeSize );
				Ral_LoadPipelineCache( backend, cachePath );
			}
		}
	}
}


void vk_create_pipelines( void )
{
	vk_alloc_persistent_pipelines();

	vk.pipelines_world_base = vk.pipelines_count;
}


static void vk_destroy_attachments( void )
{
	uint32_t i;

	if ( vk.bloom_image[0] ) {
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ ) {
			qvkDestroyImage( vk.device, vk.bloom_image[i], NULL );
			qvkDestroyImageView( vk.device, vk.bloom_image_view[i], NULL );
			vk.bloom_image[i] = VK_NULL_HANDLE;
			vk.bloom_image_view[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.tonemapped_image ) {
		qvkDestroyImage( vk.device, vk.tonemapped_image, NULL );
		qvkDestroyImageView( vk.device, vk.tonemapped_image_view, NULL );
		vk.tonemapped_image = VK_NULL_HANDLE;
		vk.tonemapped_image_view = VK_NULL_HANDLE;
	}


	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
	vk.depth_image = VK_NULL_HANDLE;
	vk.depth_image_view = VK_NULL_HANDLE;

	if ( vk.depthFade.image ) {
		qvkDestroyImage( vk.device, vk.depthFade.image, NULL );
		qvkDestroyImageView( vk.device, vk.depthFade.view, NULL );
		qvkDestroySampler( vk.device, vk.depthFade.sampler, NULL );
		qvkFreeMemory( vk.device, vk.depthFade.memory, NULL );
		vk.depthFade.image = VK_NULL_HANDLE;
		vk.depthFade.view = VK_NULL_HANDLE;
		vk.depthFade.sampler = VK_NULL_HANDLE;
		vk.depthFade.memory = VK_NULL_HANDLE;
	}

#if FEAT_SHADOW_MAPPING
	vk_shadow_release_resources();
#endif

	// Phase 6B3'-f split-A3: every SMAA resource (images / views /
	// memory / samplers) lives on dedicated allocations now. The
	// release helper is idempotent so the cold-path (full FBO teardown)
	// and the live-path (r_smaa flips to 0) share this single call.
	vk_smaa_release_resources();

	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}


	if ( vk.screenMap.depth_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.depth_image_view, NULL );
		vk.screenMap.depth_image = VK_NULL_HANDLE;
		vk.screenMap.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.capture.image ) {
		qvkDestroyImage( vk.device, vk.capture.image, NULL );
		qvkDestroyImageView( vk.device, vk.capture.image_view, NULL );
		vk.capture.image = VK_NULL_HANDLE;
		vk.capture.image_view = VK_NULL_HANDLE;
	}

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
}


static void vk_destroy_render_passes( void )
{
	uint32_t i;

	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.bloom_extract, NULL );
		vk.render_pass.bloom_extract = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		if ( vk.render_pass.blur[i] != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.blur[i], NULL );
			vk.render_pass.blur[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.post_bloom, NULL );
		vk.render_pass.post_bloom = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.screenmap, NULL );
		vk.render_pass.screenmap = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.tonemap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.tonemap, NULL );
		vk.render_pass.tonemap = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.ui != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ui, NULL );
		vk.render_pass.ui = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.ui_clear != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.ui_clear, NULL );
		vk.render_pass.ui_clear = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.capture != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.capture, NULL );
		vk.render_pass.capture = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.depth_fade != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.depth_fade, NULL );
		vk.render_pass.depth_fade = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.smaa_edge != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_edge, NULL );
		vk.render_pass.smaa_edge = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.smaa_blend != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_blend, NULL );
		vk.render_pass.smaa_blend = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.smaa_resolve != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.smaa_resolve, NULL );
		vk.render_pass.smaa_resolve = VK_NULL_HANDLE;
	}
}


static void vk_destroy_pipelines( qboolean resetCounter )
{
	uint32_t i, j;

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
			// Phase 7.4c-pipeline — sibling RAL pipeline (NULL when
			// r_useRALPipelines = 0, or when RAL creation failed for this
			// variant). Ral_DestroyPipeline tolerates NULL → no-op.
			if ( vk.pipelines[i].ral_handle[j] != NULL ) {
				Ral_DestroyPipeline( vk.pipelines[i].ral_handle[j] );
				vk.pipelines[i].ral_handle[j] = NULL;
			}
		}
	}

	if ( resetCounter ) {
		memset( &vk.pipelines, 0, sizeof( vk.pipelines ) );
		vk.pipelines_count = 0;
	}

	if ( vk.gamma_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_gamma_pipeline ) { Ral_DestroyPipeline( vk.ral_gamma_pipeline ); vk.ral_gamma_pipeline = NULL; }

	// Phase 6B3'-c1: tonemap default + variant pipelines.
	if ( vk.tonemap_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.tonemap_pipeline, NULL );
		vk.tonemap_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_tonemap_pipeline ) { Ral_DestroyPipeline( vk.ral_tonemap_pipeline ); vk.ral_tonemap_pipeline = NULL; }
	for ( i = 0; i < ARRAY_LEN( vk.tonemap_variants ); i++ ) {
		if ( vk.tonemap_variants[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.tonemap_variants[i], NULL );
			vk.tonemap_variants[i] = VK_NULL_HANDLE;
		}
		if ( vk.ral_tonemap_variants[i] ) { Ral_DestroyPipeline( vk.ral_tonemap_variants[i] ); vk.ral_tonemap_variants[i] = NULL; }
	}

	if ( vk.capture_pipeline ) {
		qvkDestroyPipeline( vk.device, vk.capture_pipeline, NULL );
		vk.capture_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_capture_pipeline ) { Ral_DestroyPipeline( vk.ral_capture_pipeline ); vk.ral_capture_pipeline = NULL; }

	if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
		vk.bloom_extract_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_bloom_extract_pipeline ) { Ral_DestroyPipeline( vk.ral_bloom_extract_pipeline ); vk.ral_bloom_extract_pipeline = NULL; }

	if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
		vk.bloom_blend_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_bloom_blend_pipeline ) { Ral_DestroyPipeline( vk.ral_bloom_blend_pipeline ); vk.ral_bloom_blend_pipeline = NULL; }

	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
		if ( vk.ral_blur_pipeline[i] ) {
			Ral_DestroyPipeline( vk.ral_blur_pipeline[i] );
			vk.ral_blur_pipeline[i] = NULL;
		}
	}

	if ( vk.smaa_edge_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_edge_pipeline, NULL );
		vk.smaa_edge_pipeline = VK_NULL_HANDLE;
	}
	// Phase 7.4c-pipeline-followup-2 — sibling RAL pipeline teardown.
	if ( vk.ral_smaa_edge_pipeline ) { Ral_DestroyPipeline( vk.ral_smaa_edge_pipeline ); vk.ral_smaa_edge_pipeline = NULL; }
	if ( vk.smaa_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_blend_pipeline, NULL );
		vk.smaa_blend_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_smaa_blend_pipeline ) { Ral_DestroyPipeline( vk.ral_smaa_blend_pipeline ); vk.ral_smaa_blend_pipeline = NULL; }
	if ( vk.smaa_resolve_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.smaa_resolve_pipeline, NULL );
		vk.smaa_resolve_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.ral_smaa_resolve_pipeline ) { Ral_DestroyPipeline( vk.ral_smaa_resolve_pipeline ); vk.ral_smaa_resolve_pipeline = NULL; }
}


void vk_shutdown( refShutdownCode_t code )
{
	int i, j, k, l;

	// Phase 9C: invalidate the HDR pipeline state snapshot so a
	// post-shutdown `gfxinfo` prints "not yet initialized" rather
	// than stale formats from the previous device. Runs before the
	// early-return below so it fires even when shutdown is called
	// on a partially-initialized instance.
	memset( &vk_hdr_state, 0, sizeof( vk_hdr_state ) );

	if ( qvkQueuePresentKHR == NULL ) { // not fully initialized
		goto __cleanup;
	}

	// Drain GPU work so all pending fences are signaled, then stop the fence
	// thread before any Vulkan resources (fences, device) are destroyed.
	// Phase 7.4c-submit-BC-C-final — retargeted from qvkQueueWaitIdle(vk.queue)
	// to Ral_WaitQueueIdle. The RAL backend owns the queue mutex + dispatches
	// vkQueueWaitIdle internally; functional equivalent of the legacy call.
	// Completes the qvkQueueWaitIdle retirement (vk_queue_wait_idle's BC-
	// followup-present-1 retarget was the other site; this is vk_shutdown's
	// direct call).
	Ral_WaitQueueIdle( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
	vk_fence_thread_stop();
	vk_gpu_ts_shutdown();

	vk_destroy_framebuffers();

	vk_destroy_pipelines( qtrue ); // reset counter

	vk_destroy_render_passes();

	vk_destroy_attachments();

	vk_destroy_swapchain( qfalse );   // Phase 7.4c-submit-followup-present-2: preserveRal=qfalse — fresh shutdown teardown; Ral_DestroySwapchain destroys both wrapper AND underlying VkSwapchainKHR.

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}

#ifdef USE_UPLOAD_QUEUE
	// Phase 7.4c-submit-followup-staging — free the RAL-owned staging cb
	// BEFORE the legacy vk.command_pool destroy. ral_staging_cmd has
	// ownsBuffer=qtrue (acquired from RAL's graphics pool, not vk.command_pool),
	// so Ral_DestroyCommandBuffer returns the underlying VkCommandBuffer to
	// RAL's pool — not vk.command_pool. After this turn vk.command_pool
	// has one fewer buffer to free (the per-frame ring at vk.tess[i].command_
	// buffer still lives in it; that retires in BC-C-final).
	if ( vk.ral_staging_cmd ) {
		Ral_DestroyCommandBuffer( vk.ral_staging_cmd );
		vk.ral_staging_cmd = NULL;
		vk.staging_command_buffer = VK_NULL_HANDLE;
	}
#endif

	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);

	// Phase 7.4c-bindgroup-pre — free the RAL BGL wrappers before destroying
	// the underlying VkDescriptorSetLayouts. Ral_DestroyBindGroupLayout on an
	// adopted layout (ownsLayout=qfalse) just frees the wrapper struct; the
	// VkDescriptorSetLayout is destroyed by the legacy calls below.
	if ( vk.ral_bgl_sampler ) { Ral_DestroyBindGroupLayout( vk.ral_bgl_sampler ); vk.ral_bgl_sampler = NULL; }
	if ( vk.ral_bgl_uniform ) { Ral_DestroyBindGroupLayout( vk.ral_bgl_uniform ); vk.ral_bgl_uniform = NULL; }
	if ( vk.ral_bgl_storage ) { Ral_DestroyBindGroupLayout( vk.ral_bgl_storage ); vk.ral_bgl_storage = NULL; }

	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_sampler, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_uniform, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_storage, NULL);

	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
	if ( vk.pipeline_layout_msdf != VK_NULL_HANDLE )
		qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_msdf, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_storage, NULL);
	vk_shutdown_ribbon();
	vk_shutdown_sprite();
	vk_shutdown_beam();
	vk_shutdown_particle();
	vk_shutdown_primitive_stages();
#if FEAT_IQM
	vk_shutdown_iqm_gpu_skinning();
#endif
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_post_process, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_blend, NULL);
	if ( vk.pipeline_layout_smaa != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_smaa, NULL );
	}
#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

#if FEAT_SHADOW_MAPPING
	vk_shutdown_shadow_snap(); // Phase 6.5.4d2-followup: per-cmd-slot snapshot buffers + CPU arrays
#endif

	vk_destroy_samplers();

	vk_destroy_sync_primitives();

	vk_ral_unregister_buffer( vk.storage.buffer );
	qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
	qvkFreeMemory( vk.device, vk.storage.memory, NULL );

	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.vert.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.vert.gen[i][j][k][l], NULL );
						vk.modules.vert.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				if ( vk.modules.frag.gen[i][j][k] != VK_NULL_HANDLE ) {
					qvkDestroyShaderModule( vk.device, vk.modules.frag.gen[i][j][k], NULL );
					vk.modules.frag.gen[i][j][k] = VK_NULL_HANDLE;
				}
			}
		}
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.light[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.light[i], NULL );
			vk.modules.vert.light[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			if ( vk.modules.frag.light[i][j] != VK_NULL_HANDLE ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.light[i][j], NULL );
				vk.modules.frag.light[i][j] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k], NULL );
				vk.modules.vert.ident1[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j], NULL );
			vk.modules.frag.ident1[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k], NULL );
				vk.modules.vert.fixed[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j], NULL );
			vk.modules.frag.fixed[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 1; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j], NULL );
			vk.modules.frag.ent[i][j] = VK_NULL_HANDLE;
		}
	}

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );

	// depth fade shader modules
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.frag.dfade_gen[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_gen[0][i], NULL );
		if ( vk.modules.frag.dfade_ident1[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_ident1[0][i], NULL );
		if ( vk.modules.frag.dfade_fixed[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_fixed[0][i], NULL );
		if ( vk.modules.frag.dfade_ent[0][i] )
			qvkDestroyShaderModule( vk.device, vk.modules.frag.dfade_ent[0][i], NULL );
	}

	if ( vk.modules.msdf_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.msdf_fs, NULL );
	if ( vk.modules.msdf_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.msdf_vs, NULL );

	if ( vk.modules.ribbon_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.ribbon_fs, NULL );
	if ( vk.modules.ribbon_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.ribbon_vs, NULL );

	if ( vk.modules.sprite_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.sprite_fs, NULL );
	if ( vk.modules.sprite_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.sprite_vs, NULL );

	if ( vk.modules.beam_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.beam_fs, NULL );
	if ( vk.modules.beam_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.beam_vs, NULL );

	if ( vk.modules.particle_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.particle_fs, NULL );
	if ( vk.modules.particle_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.particle_vs, NULL );
	if ( vk.modules.particle_integrate_cs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.particle_integrate_cs, NULL );

	if ( vk.modules.q1_ls_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.q1_ls_fs, NULL );
	if ( vk.modules.q1_ls_array_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.q1_ls_array_fs, NULL );
	if ( vk.modules.q1_ls_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.q1_ls_vs, NULL );

	qvkDestroyShaderModule( vk.device, vk.modules.color_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.color_vs, NULL );

	qvkDestroyShaderModule(vk.device, vk.modules.fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fog_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.dot_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.dot_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.bloom_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blend_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.gamma_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.tonemap_fs, NULL);

	// SMAA shader modules
	if ( vk.modules.smaa_edge_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_edge_vs, NULL );
	if ( vk.modules.smaa_edge_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_edge_fs, NULL );
	if ( vk.modules.smaa_blend_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_blend_vs, NULL );
	if ( vk.modules.smaa_blend_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_blend_fs, NULL );
	if ( vk.modules.smaa_resolve_vs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_resolve_vs, NULL );
	if ( vk.modules.smaa_resolve_fs != VK_NULL_HANDLE )
		qvkDestroyShaderModule( vk.device, vk.modules.smaa_resolve_fs, NULL );

#define DESTROY_SM(m) \
	do { if ( (m) != VK_NULL_HANDLE ) { \
		qvkDestroyShaderModule( vk.device, (m), NULL ); \
		(m) = VK_NULL_HANDLE; } } while ( 0 )

#if FEAT_PARALLAX_MAPPING
	for ( i = 0; i < 2; i++ ) {
		DESTROY_SM( vk.modules.vert.light_parallax[i] );
		for ( j = 0; j < 2; j++ )
			DESTROY_SM( vk.modules.frag.light_parallax[i][j] );
	}
#endif

#if FEAT_ADVANCED_WATER
	DESTROY_SM( vk.modules.water_fs );
#endif

#if FEAT_SHADOW_MAPPING
	DESTROY_SM( vk.modules.shadow_depth_vs );
	DESTROY_SM( vk.modules.shadow_depth_fs );
	for ( i = 0; i < 2; i++ ) {
		DESTROY_SM( vk.modules.light_shadow[i] );
		for ( j = 0; j < 2; j++ )
			DESTROY_SM( vk.modules.light_shadow_frag[i][j] );
	}
#endif

#if FEAT_PBR
	for ( i = 0; i < 2; i++ )
		for ( j = 0; j < 2; j++ )
			DESTROY_SM( vk.modules.light_pbr_frag[i][j] );
#endif

#if FEAT_IQM
	/* vk_shutdown_iqm_gpu_skinning above only destroys the pipeline/buffers/layout. */
	DESTROY_SM( vk.modules.iqm_skinning_vs );
	DESTROY_SM( vk.modules.iqm_skinning_fs );
#endif

	/* tonemap_variant_fs is a sparse array indexed by feature-flag bitmask. */
	for ( i = 0; i < TONEMAP_VAR_COUNT; i++ )
		DESTROY_SM( vk.tonemap_variant_fs[i] );

#undef DESTROY_SM

	// Phase 7.4a RAL teardown (texture/buffer wrappers + bindless set +
	// owned RAL pipelines) lives in RE_Shutdown:vk_ral_textures_shutdown —
	// runs BEFORE vk_shutdown on REF_UNLOAD_DLL paths so the renderer's
	// RAL-pipeline destroy fires while the backend is still alive, AND
	// runs on REF_KEEP_CONTEXT paths where vk_shutdown is skipped.
	//
	// Phase 7.4c-submit-followup-present-2-fix3 — Ral_DestroyBackend itself
	// is the LAST RAL touch: it has to outlive all the RAL-wrapper
	// destruction in vk_shutdown above (Ral_DestroyCommandBuffer of
	// vk.ral_staging_cmd, Ral_DestroySwapchain inside vk_destroy_swapchain,
	// vk_destroy_sync_primitives' Ral_DestroySemaphore wrappers,
	// vk_ral_unregister_buffer on vk.storage.buffer). Drives backend
	// shutdown HERE so b->cmdPools[] / b->device are still valid for
	// those callers, then qvkDestroyDevice retires the imported VkDevice
	// below.
	vk_ral_backend_shutdown();

__cleanup:
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDestroyDevice( vk.device, NULL );
	}

	deinit_device_functions();

	memset( &vk, 0, sizeof( vk ) );
	memset( &vk_world, 0, sizeof( vk_world ) );

	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		deinit_instance_functions();
	}
}


void vk_wait_idle( void )
{
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}


void vk_queue_wait_idle( void )
{
	// Phase 7.4c-submit-followup-present-1 (B1 fold) — retargeted from
	// VK_CHECK(qvkQueueWaitIdle(vk.queue)) to Ral_WaitQueueIdle. Functional
	// equivalent: idle-waits the graphics queue via the RAL backend's
	// queue mutex + vkQueueWaitIdle. Callers (tr_backend.c:527, 2117) are
	// unchanged. After BC-C-final retires the per-frame submit, vk.queue
	// has only the init/shutdown consumers; this helper becomes the last
	// renderer-side "wait queue idle" surface and is delivered through RAL.
	Ral_WaitQueueIdle( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
}


void vk_release_resources( void ) {
	int i, j;

	vk_wait_idle();

#if FEAT_IQM
	// destroy per-model IQM GPU skinning VBOs before hunk is reset
	if ( vk.iqmGpu.available ) {
		for ( i = 0; i < tr.numModels; i++ ) {
			model_t *mod = tr.models[i];
			if ( mod && mod->type == MOD_IQM && mod->modelData ) {
				iqmData_t *data = (iqmData_t *)mod->modelData;
				if ( data->vk_gpu_skinning ) {
					vk_destroy_iqm_vbo( &data->vk_vertex_buffer, &data->vk_vertex_memory,
						&data->vk_index_buffer, &data->vk_index_memory );
					data->vk_gpu_skinning = qfalse;
				}
			}
		}
	}
#endif

	for (i = 0; i < vk_world.num_image_chunks; i++)
		qvkFreeMemory(vk.device, vk_world.image_chunks[i].memory, NULL);

	vk_clean_staging_buffer();

	// vk_destroy_samplers();

	for ( i = vk.pipelines_world_base; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
			// Phase 7.4c-pipeline — sibling RAL pipeline cleanup for per-map
			// destroy. NULL-tolerant.
			if ( vk.pipelines[i].ral_handle[j] != NULL ) {
				Ral_DestroyPipeline( vk.pipelines[i].ral_handle[j] );
				vk.pipelines[i].ral_handle[j] = NULL;
			}
		}
		memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}
#if 0 // do not reduce chunk size
	else if ( vk_world.num_image_chunks == 1 ) {
		// otherwise set to default if used less than a half
		if ( vk_world.image_chunks[0].used < ( IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10) ) ) {
			vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}
#endif

	memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );

	memset( &vk.stats, 0, sizeof( vk.stats ) );
}

#if 0
static void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access) {

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = offset;
	barrier.size = size;

	qvkCmdPipelineBarrier( cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL );
}
#endif

void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat format = image->internalFormat;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	// Phase 6.5.1: dimensionality. CUBE → 2D image with 6 array layers and
	// the cube-compatible flag (so a VK_IMAGE_VIEW_TYPE_CUBE view is legal);
	// 3D → a true 3D image with extent.depth slices and arrayLayers 1; else
	// the legacy 2D / 2D-array path. A zero-initialised image_t is TEXTYPE_2D
	// with layerCount/depth 0, which the clamps below normalise to 1.
	uint32_t cube_layers  = ( image->layerCount > 1 ) ? image->layerCount : 6;
	uint32_t array_layers = ( image->layerCount > 1 ) ? image->layerCount : 1;
	uint32_t vol_depth    = ( image->depth > 1 ) ? (uint32_t)image->depth : 1;

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = array_layers;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( image->texType == TEXTYPE_CUBE ) {
			desc.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			desc.arrayLayers = cube_layers;
		} else if ( image->texType == TEXTYPE_3D ) {
			desc.imageType = VK_IMAGE_TYPE_3D;
			desc.extent.depth = vol_depth;
			desc.arrayLayers = 1;
		}

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = ( array_layers > 1 ) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = array_layers;

		if ( image->texType == TEXTYPE_CUBE ) {
			desc.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			desc.subresourceRange.layerCount = cube_layers;
		} else if ( image->texType == TEXTYPE_3D ) {
			desc.viewType = VK_IMAGE_VIEW_TYPE_3D;
			desc.subresourceRange.layerCount = 1;
		}

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );

	if ( image->descriptor == VK_NULL_HANDLE || image->view == VK_NULL_HANDLE || image->handle == VK_NULL_HANDLE ) {
		ri.Log( SEV_DEBUG, "[FBO_DEBUG] INVALID image: '%s' handle=%p view=%p descriptor=%p\n",
			image->imgName, (void*)image->handle, (void*)image->view, (void*)image->descriptor );
	}
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update, uint32_t baseArrayLayer ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = baseArrayLayer;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, buf, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	//ri.Log( SEV_WARN, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk.staging_buffer.offset += buffer_size;

	command_buffer = vk.staging_command_buffer;

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping image upload '%s'\n", __func__, image->imgName );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
			// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
			if ( update ) {
				record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
			} else {
				record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
			}
			qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
			record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			Ral_SubmitAndDispose( rcmd );
		}
	}
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}


// Phase 6.5: BC* per-block byte size (all BC formats use 4x4 blocks).
// BC1, BC4 = 8 bytes/block. BC2, BC3, BC5, BC6H, BC7 = 16 bytes/block.
static int vk_bc_block_bytes( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case VK_FORMAT_BC4_UNORM_BLOCK:
		case VK_FORMAT_BC4_SNORM_BLOCK:
			return 8;
		case VK_FORMAT_BC2_UNORM_BLOCK:
		case VK_FORMAT_BC2_SRGB_BLOCK:
		case VK_FORMAT_BC3_UNORM_BLOCK:
		case VK_FORMAT_BC3_SRGB_BLOCK:
		case VK_FORMAT_BC5_UNORM_BLOCK:
		case VK_FORMAT_BC5_SNORM_BLOCK:
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:
		case VK_FORMAT_BC7_UNORM_BLOCK:
		case VK_FORMAT_BC7_SRGB_BLOCK:
			return 16;
		default:
			return 0;  // not a recognised BC format
	}
}


qboolean vk_bc_format_supported( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:  return vk.bc_formats_supported.bc1_unorm;
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:   return vk.bc_formats_supported.bc1_srgb;
		case VK_FORMAT_BC2_UNORM_BLOCK:      return vk.bc_formats_supported.bc2_unorm;
		case VK_FORMAT_BC2_SRGB_BLOCK:       return vk.bc_formats_supported.bc2_srgb;
		case VK_FORMAT_BC3_UNORM_BLOCK:      return vk.bc_formats_supported.bc3_unorm;
		case VK_FORMAT_BC3_SRGB_BLOCK:       return vk.bc_formats_supported.bc3_srgb;
		case VK_FORMAT_BC4_UNORM_BLOCK:      return vk.bc_formats_supported.bc4_unorm;
		case VK_FORMAT_BC4_SNORM_BLOCK:      return vk.bc_formats_supported.bc4_snorm;
		case VK_FORMAT_BC5_UNORM_BLOCK:      return vk.bc_formats_supported.bc5_unorm;
		case VK_FORMAT_BC5_SNORM_BLOCK:      return vk.bc_formats_supported.bc5_snorm;
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:    return vk.bc_formats_supported.bc6h_ufloat;
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:    return vk.bc_formats_supported.bc6h_sfloat;
		case VK_FORMAT_BC7_UNORM_BLOCK:      return vk.bc_formats_supported.bc7_unorm;
		case VK_FORMAT_BC7_SRGB_BLOCK:       return vk.bc_formats_supported.bc7_srgb;
		default:                             return qfalse;
	}
}


/*
================
vk_upload_image_data_compressed

Phase 6.5: upload a BC* compressed mip chain to image->handle. The
data buffer is the contiguous concatenation of mip 0 through
mip (mipLevels-1) as the DDS file format lays them out. Per-mip
byte size is computed from the BC block byte count + block-padded
dimensions (every BC format uses 4x4 blocks).

Phase 6.5.1: when image->texType == TEXTYPE_CUBE the buffer is six
such mip chains laid out face-major (the DDS spec order +X,-X,+Y,
-Y,+Z,-Z, which equals Vulkan cube array layers 0..5), and the
upload writes all 6 array layers.

Mirrors vk_upload_image_data's staging-buffer + vkCmdCopyBufferToImage
flow, minus the resample_image_data step (compressed data has no
per-pixel layout to convert) and minus the byte-per-pixel math.
================
*/
void vk_upload_image_data_compressed( image_t *image, int width, int height, int mipLevels, byte *data, int dataSize ) {
	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[6 * 16];
	int n;
	int num_regions = 0;
	int buffer_offset = 0;
	int blockBytes;
	int faces, face;
	int mipsPerFace;

	blockBytes = vk_bc_block_bytes( image->internalFormat );
	if ( blockBytes == 0 ) {
		ri.Log( SEV_ERROR, "vk_upload_image_data_compressed: %s — internalFormat %d is not a BCn format\n",
			image->imgName, (int)image->internalFormat );
		return;
	}

	faces = ( image->texType == TEXTYPE_CUBE )
	      ? ( ( image->layerCount > 1 ) ? (int)image->layerCount : 6 )
	      : 1;

	mipsPerFace = mipLevels;
	if ( mipsPerFace > 16 )
		mipsPerFace = 16;
	if ( mipsPerFace * faces > (int)ARRAY_LEN( regions ) )
		mipsPerFace = (int)ARRAY_LEN( regions ) / faces;

	for ( face = 0; face < faces; face++ ) {
		int mipW = width;
		int mipH = height;
		for ( n = 0; n < mipsPerFace; n++ ) {
			int blocksW = ( mipW + 3 ) / 4;
			int blocksH = ( mipH + 3 ) / 4;
			int mipBytes = blocksW * blocksH * blockBytes;

			if ( buffer_offset + mipBytes > dataSize ) {
				ri.Log( SEV_WARN, "vk_upload_image_data_compressed: %s — face %d mip %d truncated (have %d bytes, need %d)\n",
					image->imgName, face, n, dataSize - buffer_offset, mipBytes );
				face = faces;  // stop the outer loop too
				break;
			}

			memset( &regions[num_regions], 0, sizeof( regions[num_regions] ) );
			regions[num_regions].bufferOffset = buffer_offset;
			regions[num_regions].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			regions[num_regions].imageSubresource.mipLevel = n;
			regions[num_regions].imageSubresource.baseArrayLayer = face;
			regions[num_regions].imageSubresource.layerCount = 1;
			regions[num_regions].imageExtent.width = mipW;
			regions[num_regions].imageExtent.height = mipH;
			regions[num_regions].imageExtent.depth = 1;
			num_regions++;

			buffer_offset += mipBytes;

			if ( mipW == 1 && mipH == 1 )
				break;

			mipW >>= 1; if ( mipW < 1 ) mipW = 1;
			mipH >>= 1; if ( mipH < 1 ) mipH = 1;
		}
	}

	if ( num_regions == 0 )
		return;

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < (uint32_t)buffer_offset ) {
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size < (uint32_t)buffer_offset ) {
		vk_alloc_staging_buffer( buffer_offset );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, data, buffer_offset );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	vk.staging_buffer.offset += buffer_offset;

	command_buffer = vk.staging_command_buffer;

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < (uint32_t)buffer_offset ) {
		vk_alloc_staging_buffer( buffer_offset );
	}

	memcpy( vk.staging_buffer.ptr, data, buffer_offset );

	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping image upload '%s'\n", __func__, image->imgName );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
			record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
			qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
			record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			Ral_SubmitAndDispose( rcmd );
		}
	}
#endif
}


// Phase 6.5.1: bytes-per-pixel for the (few) uncompressed VkFormats that
// R_CreateImageDDS / vk_upload_image_data_3d will accept directly from a
// .dds file. Returns 0 for anything else.
static int vk_uncompressed_dds_bpp( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:       return 4;
		case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
		default:                            return 0;
	}
}


qboolean vk_dds_format_uploadable( VkFormat format ) {
	if ( vk_bc_block_bytes( format ) != 0 )
		return vk_bc_format_supported( format );
	return vk_uncompressed_dds_bpp( format ) != 0 ? qtrue : qfalse;
}


/*
================
vk_upload_image_data_3d

Phase 6.5.1: upload an uncompressed volume (3D) mip chain. The DDS
file lays a 3D texture out as, per mip level, a contiguous slab of
max(1, depth>>level) z-slices, each (width>>level) x (height>>level)
pixels. One vkCmdCopyBufferToImage region per mip, extent.depth set
to the slice count. image->internalFormat must be one of the formats
vk_uncompressed_dds_bpp accepts; image->texType must be TEXTYPE_3D
and image->depth the base slice count, set before vk_create_image.
================
*/
void vk_upload_image_data_3d( image_t *image, int width, int height, int depth, int mipLevels, byte *data, int dataSize ) {
	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	int n;
	int num_regions = 0;
	int buffer_offset = 0;
	int bpp;
	int mipW, mipH, mipD;

	bpp = vk_uncompressed_dds_bpp( image->internalFormat );
	if ( bpp == 0 ) {
		ri.Log( SEV_ERROR, "vk_upload_image_data_3d: %s — internalFormat %d is not a supported uncompressed format\n",
			image->imgName, (int)image->internalFormat );
		return;
	}

	if ( mipLevels > (int)ARRAY_LEN( regions ) )
		mipLevels = (int)ARRAY_LEN( regions );
	if ( depth < 1 )
		depth = 1;

	mipW = width;
	mipH = height;
	mipD = depth;
	for ( n = 0; n < mipLevels; n++ ) {
		int mipBytes = mipW * mipH * mipD * bpp;

		if ( buffer_offset + mipBytes > dataSize ) {
			ri.Log( SEV_WARN, "vk_upload_image_data_3d: %s — mip %d truncated (have %d bytes, need %d)\n",
				image->imgName, n, dataSize - buffer_offset, mipBytes );
			break;
		}

		memset( &regions[num_regions], 0, sizeof( regions[num_regions] ) );
		regions[num_regions].bufferOffset = buffer_offset;
		regions[num_regions].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		regions[num_regions].imageSubresource.mipLevel = n;
		regions[num_regions].imageSubresource.baseArrayLayer = 0;
		regions[num_regions].imageSubresource.layerCount = 1;
		regions[num_regions].imageExtent.width = mipW;
		regions[num_regions].imageExtent.height = mipH;
		regions[num_regions].imageExtent.depth = mipD;
		num_regions++;

		buffer_offset += mipBytes;

		if ( mipW == 1 && mipH == 1 && mipD == 1 )
			break;

		mipW >>= 1; if ( mipW < 1 ) mipW = 1;
		mipH >>= 1; if ( mipH < 1 ) mipH = 1;
		mipD >>= 1; if ( mipD < 1 ) mipD = 1;
	}

	if ( num_regions == 0 )
		return;

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < (uint32_t)buffer_offset ) {
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size < (uint32_t)buffer_offset ) {
		vk_alloc_staging_buffer( buffer_offset );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, data, buffer_offset );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	vk.staging_buffer.offset += buffer_offset;

	command_buffer = vk.staging_command_buffer;

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < (uint32_t)buffer_offset ) {
		vk_alloc_staging_buffer( buffer_offset );
	}

	memcpy( vk.staging_buffer.ptr, data, buffer_offset );

	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping image upload '%s'\n", __func__, image->imgName );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
			record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
			qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
			record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
			Ral_SubmitAndDispose( rcmd );
		}
	}
#endif
}


/*
================
vk_upload_dds_image_data

Phase 6.5.1: dispatch a freshly-created DDS image to the right upload
path based on its format / texType:
  - TEXTYPE_3D                       -> vk_upload_image_data_3d
  - BC* block-compressed             -> vk_upload_image_data_compressed
                                        (handles 6-face cubemaps too)
  - uncompressed RGBA, plain 2D      -> vk_upload_image_data
An uncompressed cubemap (RGBA faces, no BC) is the one combination not
yet handled — those are vanishingly rare in practice; warn and upload
only face 0 so the engine still has *something* sampleable.
================
*/
void vk_upload_dds_image_data( image_t *image, int width, int height, int depth, int mipLevels, byte *data, int dataSize ) {
	if ( image->texType == TEXTYPE_3D ) {
		vk_upload_image_data_3d( image, width, height, depth, mipLevels, data, dataSize );
		return;
	}
	if ( vk_bc_block_bytes( image->internalFormat ) != 0 ) {
		vk_upload_image_data_compressed( image, width, height, mipLevels, data, dataSize );
		return;
	}
	if ( image->texType == TEXTYPE_CUBE ) {
		ri.Log( SEV_WARN, "vk_upload_dds_image_data: %s — uncompressed cubemap upload is not implemented; only face +X will be visible\n",
			image->imgName );
	}
	vk_upload_image_data( image, 0, 0, width, height, mipLevels, data, dataSize, qfalse, 0 );
}


void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			// MoltenVK guard: swapchain images are owned by the swapchain and must
			// not be passed to vkDestroyImage — doing so crashes via
			// destroyPresentableSwapchainImage when handle aliasing occurs.
			qboolean is_swapchain = qfalse;
			uint32_t sci;
			for ( sci = 0; sci < vk.swapchain_image_count; sci++ ) {
				if ( *image == vk.swapchain_images[ sci ] ) {
					is_swapchain = qtrue;
					break;
				}
			}
			if ( !is_swapchain ) {
				qvkDestroyImage( vk.device, *image, NULL );
			}
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}


/*
================
vk_is_full_window_pipeline

Returns qtrue for vk_create_post_process_pipeline program_index values
whose render pass writes the full window (gamma, tonemap variant,
tonemap default). The (0, 0) sentinel from those call sites is
interpreted as "use the full window extent" — every other program
gets a normal viewport derived from the supplied width/height.

Add a new full-window case here when introducing one; the viewport
branch in vk_create_post_process_pipeline reads only this predicate
so a missing case here will silently produce a 0x0 viewport and
VUID-VkViewport-width-01770. Slot 4 (boost) was removed in
6B3'-f-boost-only; case 5 was added by Phase 6B3'-c1 (tonemap
variant); case 6 was added by the c1 hotfix (tonemap default).
================
*/
static qboolean vk_is_full_window_pipeline( int program_index )
{
	return ( program_index == 0    /* gamma */
	      || program_index == 5    /* tonemap variant */
	      || program_index == 6 )  /* tonemap default */
		? qtrue : qfalse;
}


void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[26];  // post-process: 13 gamma+tonemap + 5 lottes + 1 srgb_swapchain (Phase 6B3'-d) + 5 colour grading (Phase 6B3'-e) + 2 HDR10 (Phase 6B3'-d8)
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;
	VkShaderModule fsmodule;
	VkRenderPass renderpass;
	VkPipelineLayout layout;
	VkSampleCountFlagBits samples;
	const char *pipeline_name;
	qboolean blend;

	struct FragSpecData {
		float gamma;
		// Phase 6B3'-a: was `overbright` (= 2^tr.overbrightBits in
		// the legacy LDR pipeline). constant_id 1 is now a
		// pre-tonemap exposure_bias fed directly from
		// r_brightness->value — no log/round/clamp transformation.
		// Default 1.0 = linear identity (no boost).
		float exposure_bias;
		float saturation;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
		// Tonemap fields wire tonemap.frag spec constants 17/18.
		// They are present in the struct unconditionally even when
		// FEAT_TONEMAP isn't compiled — the spec_entries below always
		// include them, and the driver ignores spec entries whose
		// constant IDs aren't declared in the bound shader. Keeping
		// the struct shape stable simplifies offsetof maintenance.
		int tonemap_mode;
		float tonemap_exposure;

		// Lottes (tonemap_mode == 3) configurable parameters wired to
		// tonemap.frag spec constants 28-32. Same write-unconditionally
		// pattern as the tonemap fields above.
		float lottes_contrast;
		float lottes_shoulder;
		float lottes_mid_in;
		float lottes_mid_out;
		float lottes_hdr_max;
		// Phase 6B3'-d: gamma.frag spec constant 11 — gates the display-
		// encode pow() when the swapchain image format is VK_FORMAT_*_SRGB
		// (hardware does the linear -> sRGB encode on present). Written
		// for every post-process pipeline; tonemap variants ignore the
		// ID because they don't declare it.
		int srgb_swapchain;
		// Phase 6B3'-e: colour-grading knobs wired to tonemap.frag spec
		// constants 19..23 (USE_COLOR_GRADING variant). Live via the
		// five r_grade_* cvars; ignored by every variant that lacks
		// USE_COLOR_GRADING. Slot allocation table:
		//   id 11      srgb_swapchain          (Phase 6B3'-d, gamma.frag)
		//   id 17..18  tonemap_mode/exposure   (c1, tonemap.frag)
		//   id 19..23  cg_tint_r/g/b/sat/con   (this phase, tonemap.frag)
		//   id 24..25  FREE (was FXAA, removed 6B3'-e)
		//   id 26..27  godrays_samples/density (tonemap.frag)
		//   id 28..32  lottes_*                (rebrand, tonemap.frag)
		float cg_tint_r;
		float cg_tint_g;
		float cg_tint_b;
		float cg_saturation;
		float cg_contrast;
		// Phase 6B3'-d8: HDR10 display output. hdr_mode (gamma.frag +
		// tonemap.frag spec constant 12): 0 = sRGB SDR encode, 1 = HDR10
		// PQ encode (gamma.frag) / HDR tonemap shoulder (tonemap.frag).
		// hdr_peak_norm (id 13): display peak in "graphics-white" units
		// (= r_hdrPeakLuminance / 100), the value a fully-bright scene
		// element tonemaps to; the gamma pass then scales graphics-white
		// (1.0) to ~100 nits and the peak to r_hdrPeakLuminance nits.
		// Written for every post-process pipeline; variants that don't
		// declare ids 12/13 ignore the entries.
		int   hdr_mode;
		float hdr_peak_norm;
	} frag_spec_data;

	switch ( program_index ) {
		case 1: // bloom extraction
			pipeline = &vk.bloom_extract_pipeline;
			fsmodule = vk.modules.bloom_fs;
			renderpass = vk.render_pass.bloom_extract;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom extraction pipeline";
			blend = qfalse;
			break;
		case 2: // final bloom blend
			pipeline = &vk.bloom_blend_pipeline;
			fsmodule = vk.modules.blend_fs;
			renderpass = vk.render_pass.post_bloom;
			layout = vk.pipeline_layout_blend;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "bloom blend pipeline";
			blend = qtrue;
			break;
		case 3: // capture buffer extraction
			pipeline = &vk.capture_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.capture;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "capture buffer pipeline";
			blend = qfalse;
			break;
		case 6: // Phase 6B3'-c1: tonemap default (no feature variants set)
			pipeline = &vk.tonemap_pipeline;
			fsmodule = vk.modules.tonemap_fs;
			renderpass = vk.render_pass.tonemap;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "tonemap default pipeline";
			blend = qfalse;
			break;
		default: // gamma correction
			pipeline = &vk.gamma_pipeline;
			fsmodule = vk.modules.gamma_fs;
			renderpass = vk.render_pass.gamma;
			layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "gamma-correction pipeline";
			blend = qfalse;
			break;
		case 5: { // Phase 6B3'-c1: tonemap variant (SSAO/tonemap/colorgrade/godrays combo)
			int varIdx = 0;
#if FEAT_SSAO
			if ( r_ssao->integer )     varIdx |= TONEMAP_VAR_SSAO;
#endif
#if FEAT_TONEMAP
			if ( r_tonemap->integer )  varIdx |= TONEMAP_VAR_BASE;
#endif
#if FEAT_COLOR_GRADING
			if ( r_colorGrading->integer ) varIdx |= TONEMAP_VAR_CG;
#endif
#if FEAT_GODRAYS
			if ( r_godRays->integer )  varIdx |= TONEMAP_VAR_GODRAYS;
#endif
			pipeline = &vk.tonemap_variants[ varIdx ];
			fsmodule = vk.tonemap_variant_fs[ varIdx ];
			renderpass = vk.render_pass.tonemap;
			// Select pipeline layout: godrays needs push constants, SSAO/godrays need depth sampler
			if ( varIdx & TONEMAP_VAR_GODRAYS )
				layout = vk.pipeline_layout_godrays;
			else if ( varIdx & TONEMAP_VAR_SSAO )
				layout = vk.pipeline_layout_ssao;
			else
				layout = vk.pipeline_layout_post_process;
			samples = VK_SAMPLE_COUNT_1_BIT;
			pipeline_name = "tonemap-variant pipeline";
			blend = qfalse;
			break;
		}
	}

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, fsmodule, "main" );

	frag_spec_data.gamma = 1.0 / (r_gamma->value);
	frag_spec_data.exposure_bias = r_brightness->value;
	frag_spec_data.saturation = r_saturation->value;
	frag_spec_data.bloom_threshold = r_bloomThreshold->value;
	frag_spec_data.bloom_intensity = r_bloomIntensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloomThresholdMode->integer;
	// Block 3 (colour closure): r_bloomModulate is gone — bloom.frag's
	// soft knee replaced the modulate post-tweak and dropped its
	// constant_id 6. The FragSpecData field + spec_entries[6] mapping
	// below are retained as a dead, zero-valued slot purely so the
	// struct offsets and spec_entries[] indices of every following
	// post-process constant stay byte-for-byte stable (no renumber);
	// constant id 6 is no longer declared by any post-process shader,
	// so the driver ignores the entry. Reclaim in a later cleanup.
	frag_spec_data.bloom_modulate = 0;
	frag_spec_data.dither = r_dither->integer;
#if FEAT_TONEMAP
	frag_spec_data.tonemap_mode = r_tonemap->integer;
	frag_spec_data.tonemap_exposure = r_tonemapExposure->value;
	frag_spec_data.lottes_contrast  = r_lottes_contrast->value;
	frag_spec_data.lottes_shoulder  = r_lottes_shoulder->value;
	frag_spec_data.lottes_mid_in    = r_lottes_mid_in->value;
	frag_spec_data.lottes_mid_out   = r_lottes_mid_out->value;
	frag_spec_data.lottes_hdr_max   = r_lottes_hdr_max->value;
#else
	frag_spec_data.tonemap_mode = 1;       // PBR Neutral if compiled-in path runs without the cvar
	frag_spec_data.tonemap_exposure = 1.0f;
	frag_spec_data.lottes_contrast  = 1.6f;
	frag_spec_data.lottes_shoulder  = 0.977f;
	frag_spec_data.lottes_mid_in    = 0.18f;
	frag_spec_data.lottes_mid_out   = 0.267f;
	frag_spec_data.lottes_hdr_max   = 8.0f;
#endif

	// Phase 6B3'-e: colour-grading host fill. Always written so the
	// USE_COLOR_GRADING variant always sees current values; non-CG
	// variants ignore IDs 19..23 because they don't declare them.
#if FEAT_COLOR_GRADING
	frag_spec_data.cg_tint_r     = r_grade_tint_r->value;
	frag_spec_data.cg_tint_g     = r_grade_tint_g->value;
	frag_spec_data.cg_tint_b     = r_grade_tint_b->value;
	frag_spec_data.cg_saturation = r_grade_saturation->value;
	frag_spec_data.cg_contrast   = r_grade_contrast->value;
#else
	frag_spec_data.cg_tint_r     = 1.0f;
	frag_spec_data.cg_tint_g     = 1.0f;
	frag_spec_data.cg_tint_b     = 1.0f;
	frag_spec_data.cg_saturation = 1.0f;
	frag_spec_data.cg_contrast   = 1.0f;
#endif

	if ( !vk_surface_format_color_depth( vk.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) )
		ri.Log( SEV_INFO, "Format %s not recognized, dither to assume 8bpc\n", vk_format_string( vk.base_format.format ) );

	// Phase 6B3'-d: surface the sRGB swapchain state into gamma.frag's
	// constant_id 11. vk_hdr_state.srgb_swapchain is recomputed by
	// vk_select_surface_format on every swapchain (re)creation.
	frag_spec_data.srgb_swapchain = vk_hdr_state.srgb_swapchain ? 1 : 0;

	// Block 2 (colour closure): the capture pipeline (program_index 3)
	// reuses gamma.frag but writes vk.capture_image (VK_FORMAT_R8G8B8A8_
	// UNORM) — there is no hardware sRGB encode on that attachment, so
	// the shader must run its own linear→sRGB encode path (the
	// srgb_swapchain == 0 branch). Without this the capture inherits the
	// swapchain's sRGB capability bit and writes raw linear radiance
	// bytes into the UNORM image — screenshots / RDoc readbacks come out
	// far too dark. Force the bit off for the capture program regardless
	// of the (swapchain-only) hardware capability.
	if ( program_index == 3 )
		frag_spec_data.srgb_swapchain = 0;

	// Phase 6B3'-d8: HDR10 spec constants. hdr_mode == 1 only when an HDR
	// colorspace was actually negotiated on the swapchain (vk_select_surface_
	// format); the capture pipeline (program_index 3) always writes an
	// SDR UNORM image, so force hdr_mode 0 there (mirrors srgb_swapchain
	// above). hdr_peak_norm = display peak in graphics-white units.
	frag_spec_data.hdr_mode      = ( vk_hdr_state.hdr_display_active && program_index != 3 ) ? 1 : 0;
	frag_spec_data.hdr_peak_norm = (float)r_hdrPeakLuminance->integer / 100.0f;

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct FragSpecData, exposure_bias );
	spec_entries[1].size = sizeof( frag_spec_data.exposure_bias );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct FragSpecData, saturation );
	spec_entries[2].size = sizeof( frag_spec_data.saturation );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof(struct FragSpecData, depth_g);
	spec_entries[9].size = sizeof(frag_spec_data.depth_g);

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof(struct FragSpecData, depth_b);
	spec_entries[10].size = sizeof(frag_spec_data.depth_b);

	// Tonemap spec constants in gamma.frag — IDs 17 and 18 per the
	// shader declaration. Pipelines without USE_TONEMAP defined
	// (the non-tonemap gamma variants) never declare these IDs;
	// the driver silently ignores spec entries whose IDs aren't
	// referenced by the bound shader, so it's safe to write the
	// entries unconditionally.
	spec_entries[11].constantID = 17;
	spec_entries[11].offset = offsetof(struct FragSpecData, tonemap_mode);
	spec_entries[11].size = sizeof(frag_spec_data.tonemap_mode);

	spec_entries[12].constantID = 18;
	spec_entries[12].offset = offsetof(struct FragSpecData, tonemap_exposure);
	spec_entries[12].size = sizeof(frag_spec_data.tonemap_exposure);

	// Lottes spec constants — IDs 28-32 (skip past color grading 19-23,
	// FXAA 24-25, godrays 26-27, which is the lowest free range).
	spec_entries[13].constantID = 28;
	spec_entries[13].offset = offsetof(struct FragSpecData, lottes_contrast);
	spec_entries[13].size = sizeof(frag_spec_data.lottes_contrast);

	spec_entries[14].constantID = 29;
	spec_entries[14].offset = offsetof(struct FragSpecData, lottes_shoulder);
	spec_entries[14].size = sizeof(frag_spec_data.lottes_shoulder);

	spec_entries[15].constantID = 30;
	spec_entries[15].offset = offsetof(struct FragSpecData, lottes_mid_in);
	spec_entries[15].size = sizeof(frag_spec_data.lottes_mid_in);

	spec_entries[16].constantID = 31;
	spec_entries[16].offset = offsetof(struct FragSpecData, lottes_mid_out);
	spec_entries[16].size = sizeof(frag_spec_data.lottes_mid_out);

	spec_entries[17].constantID = 32;
	spec_entries[17].offset = offsetof(struct FragSpecData, lottes_hdr_max);
	spec_entries[17].size = sizeof(frag_spec_data.lottes_hdr_max);

	// Phase 6B3'-d: gamma.frag srgb_swapchain spec constant (id 11).
	// Tonemap variants don't declare id 11 so the driver ignores this
	// entry for those pipelines.
	spec_entries[18].constantID = 11;
	spec_entries[18].offset = offsetof(struct FragSpecData, srgb_swapchain);
	spec_entries[18].size = sizeof(frag_spec_data.srgb_swapchain);

	// Phase 6B3'-e: colour-grading spec constants (IDs 19..23). Only
	// declared by tonemap.frag's USE_COLOR_GRADING variant; gamma.frag
	// and the non-CG tonemap variants silently ignore these entries.
	spec_entries[19].constantID = 19;
	spec_entries[19].offset = offsetof(struct FragSpecData, cg_tint_r);
	spec_entries[19].size = sizeof(frag_spec_data.cg_tint_r);

	spec_entries[20].constantID = 20;
	spec_entries[20].offset = offsetof(struct FragSpecData, cg_tint_g);
	spec_entries[20].size = sizeof(frag_spec_data.cg_tint_g);

	spec_entries[21].constantID = 21;
	spec_entries[21].offset = offsetof(struct FragSpecData, cg_tint_b);
	spec_entries[21].size = sizeof(frag_spec_data.cg_tint_b);

	spec_entries[22].constantID = 22;
	spec_entries[22].offset = offsetof(struct FragSpecData, cg_saturation);
	spec_entries[22].size = sizeof(frag_spec_data.cg_saturation);

	spec_entries[23].constantID = 23;
	spec_entries[23].offset = offsetof(struct FragSpecData, cg_contrast);
	spec_entries[23].size = sizeof(frag_spec_data.cg_contrast);

	// Phase 6B3'-d8: HDR10 spec constants — id 12 (hdr_mode, int) and id 13
	// (hdr_peak_norm, float). Declared by gamma.frag (the PQ encode branch)
	// and tonemap.frag (the HDR shoulder); other variants ignore them.
	spec_entries[24].constantID = 12;
	spec_entries[24].offset = offsetof(struct FragSpecData, hdr_mode);
	spec_entries[24].size = sizeof(frag_spec_data.hdr_mode);

	spec_entries[25].constantID = 13;
	spec_entries[25].offset = offsetof(struct FragSpecData, hdr_peak_norm);
	spec_entries[25].size = sizeof(frag_spec_data.hdr_peak_norm);

	frag_spec_info.mapEntryCount = 26;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	if ( vk_is_full_window_pipeline( program_index ) ) {
		// Full-window post-process pipelines (gamma, tonemap variant,
		// tonemap default) all pass (0, 0) for width/height as a
		// sentinel meaning "use the window extent". Without this
		// branch the pipeline would fall into the else and produce a
		// 0×0 viewport, which vkCreateGraphicsPipelines rejects with
		// the "pViewports[0].width (0.000000) is not greater than
		// zero" validation error (VUID-VkViewport-width-01770).
		// The blitX0/blitY0 offsets are the windowed-mode blit insets;
		// every full-window post-process pass shares the same insets
		// because their attachments are sized to the gamma pass's
		// full-window dimensions.
		viewport.x = 0.0 + vk.blitX0;
		viewport.y = 0.0 + vk.blitY0;
		viewport.width = gls.windowWidth - vk.blitX0 * 2;
		viewport.height = gls.windowHeight - vk.blitY0 * 2;
	} else {
		// other post-processing
		viewport.x = 0.0;
		viewport.y = 0.0;
		viewport.width = width;
		viewport.height = height;
	}

	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
#if FEAT_DEPTH_CLAMP
	rasterization_state.depthClampEnable = r_depthClamp && r_depthClamp->integer ? VK_TRUE : VK_FALSE;
#else
	rasterization_state.depthClampEnable = VK_FALSE;
#endif
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = samples;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( blend ) {
		attachment_blend_state.blendEnable = VK_TRUE;
		attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	} else {
		attachment_blend_state.blendEnable = VK_FALSE;
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = (program_index == 2) ? &depth_stencil_state : NULL;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = layout;
	create_info.renderPass = renderpass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// Phase 7.4c-pipeline-followup-4 — post-process parallel RAL pipeline.
	// Binding contract from vk_initialize (vk.c:~12276) pipeline layout creation:
	//   pipeline_layout_post_process : 1 sampler set, 0 push constants
	//   pipeline_layout_blend        : VK_NUM_BLOOM_PASSES (=4) sampler sets, 0 push
	//   pipeline_layout_ssao         : 2 sampler sets, 0 push (FEAT_SSAO)
	//   pipeline_layout_godrays      : 2 sampler sets, 16B FRAGMENT push (FEAT_GODRAYS)
	// Topology TRIANGLE_STRIP — gamma.vert emits 4 vertices from a const array
	// indexed by gl_VertexIndex. Cull NONE. No depth test/write. Blend on for
	// bloom_blend only. Spec constants: 26 entries (gamma/tonemap/lottes/grading/HDR
	// knobs) packed into frag_spec_data — copy into ralSpecConstant_t[].
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		ralPipeline_t **ral_target = NULL;
		vk_ral_special_pipeline_params_t pr;
		ralSpecConstant_t              specs[26];
		uint32_t                       ns = 0, i;
		memset( &pr,   0, sizeof( pr   ) );
		memset( specs, 0, sizeof( specs ) );

		// Match the sibling sl ral_*_pipeline field per program_index. The
		// switch's varIdx for case 5 is computed above and visible here.
		switch ( program_index ) {
			case 1: ral_target = &vk.ral_bloom_extract_pipeline; break;
			case 2: ral_target = &vk.ral_bloom_blend_pipeline;   break;
			case 3: ral_target = &vk.ral_capture_pipeline;       break;
			case 6: ral_target = &vk.ral_tonemap_pipeline;       break;
			case 5: {
				int varIdxLocal = 0;
#if FEAT_SSAO
				if ( r_ssao->integer )         varIdxLocal |= TONEMAP_VAR_SSAO;
#endif
#if FEAT_TONEMAP
				if ( r_tonemap->integer )      varIdxLocal |= TONEMAP_VAR_BASE;
#endif
#if FEAT_COLOR_GRADING
				if ( r_colorGrading->integer ) varIdxLocal |= TONEMAP_VAR_CG;
#endif
#if FEAT_GODRAYS
				if ( r_godRays->integer )      varIdxLocal |= TONEMAP_VAR_GODRAYS;
#endif
				ral_target = &vk.ral_tonemap_variants[ varIdxLocal ];
				break;
			}
			default: ral_target = &vk.ral_gamma_pipeline;        break;
		}

		pr.vs_module        = vk.modules.gamma_vs;
		pr.fs_module        = fsmodule;
		pr.topology         = RAL_TOPOLOGY_TRIANGLE_STRIP;
		pr.cullMode         = RAL_CULL_NONE;
		pr.depthTestEnable  = qfalse;
		pr.depthWriteEnable = qfalse;
		pr.blendEnable      = blend;
		if ( blend ) {
			// Match bloom_blend's blend setup (set later in legacy code path):
			// SRC_ALPHA / ONE_MINUS_SRC_ALPHA additive. Conservative; if the
			// legacy attachment_blend_state ends up different, the parallel
			// pipeline still validates and bind-flip will recompute correctly
			// in followup-cmd.
			pr.srcColor = RAL_BLEND_SRC_ALPHA;
			pr.dstColor = RAL_BLEND_ONE_MINUS_SRC_ALPHA;
			pr.blendOp  = RAL_BLEND_OP_ADD;
		}

		// BGLs + push range derived from `layout` (the legacy VkPipelineLayout
		// already selected by the switch above).
		if ( layout == vk.pipeline_layout_post_process ) {
			if ( vk.ral_bgl_sampler ) { pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler; }
		} else if ( layout == vk.pipeline_layout_blend ) {
			if ( vk.ral_bgl_sampler ) {
				int j;
				for ( j = 0; j < VK_NUM_BLOOM_PASSES && pr.numBgls < 8u; j++ )
					pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler;
			}
#if FEAT_SSAO
		} else if ( layout == vk.pipeline_layout_ssao ) {
			if ( vk.ral_bgl_sampler ) {
				pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler;
				pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler;
			}
#endif
#if FEAT_GODRAYS
		} else if ( layout == vk.pipeline_layout_godrays ) {
			if ( vk.ral_bgl_sampler ) {
				pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler;
				pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler;
			}
			pr.pushConstantSize   = 16;
			pr.pushConstantStages = RAL_STAGE_FRAGMENT;
#endif
		}

		// Pack the 26 frag_spec_entries by copying 32-bit values. Each entry
		// is either int or float; for both the bit-pattern is the right 32-bit
		// value to feed RAL. We read out of frag_spec_data using each entry's
		// offset+size.
		for ( i = 0; i < (uint32_t)frag_spec_info.mapEntryCount && ns < 26u; i++ ) {
			uint32_t v = 0;
			if ( spec_entries[i].size == sizeof( uint32_t ) ) {
				memcpy( &v, (const uint8_t *)frag_spec_info.pData + spec_entries[i].offset, sizeof( uint32_t ) );
				specs[ns].constantId = spec_entries[i].constantID;
				specs[ns].value      = v;
				ns++;
			}
		}
		pr.specConstants    = specs;
		pr.numSpecConstants = ns;
		pr.debugName        = pipeline_name;

		// Phase 7.4c-submit-A3 — thread the matching ralPipelineLayout_t sibling
		// based on the legacy VkPipelineLayout the post-process site selected.
		pr.externalLayout   = vk_ral_lookup_pipeline_layout( layout );

		// Phase 7.4c-submit-A3 — pick the matching render-pass sibling per
		// program_index. The dispatcher branches above already selected `target`
		// (the legacy VkPipeline *); the program_index encodes which pass.
		// 0 = gamma; 1 = bloom_extract; 2 = bloom_blend (post_bloom); 3 = capture;
		// 5 = tonemap variant (renders into render_pass.tonemap); 6 = depth_fade.
		// blur uses its own per-index pass (handled in vk_create_blur_pipeline
		// directly).
		switch ( program_index ) {
		case 0:  pr.externalRenderPass = vk.ral_render_pass.gamma;          break;
		case 1:  pr.externalRenderPass = vk.ral_render_pass.bloom_extract;  break;
		case 2:  pr.externalRenderPass = vk.ral_render_pass.post_bloom;     break;
		case 3:  pr.externalRenderPass = vk.ral_render_pass.capture;        break;
		case 5:  pr.externalRenderPass = vk.ral_render_pass.tonemap;        break;
		case 6:  pr.externalRenderPass = vk.ral_render_pass.depth_fade;     break;
		default: pr.externalRenderPass = NULL;                              break;
		}

		if ( *ral_target ) { Ral_DestroyPipeline( *ral_target ); *ral_target = NULL; }
		*ral_target = vk_ral_create_special_pipeline( &pr );
	}
}


void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	float frag_spec_data[3]; // x-offset, y-offset, correction
	VkSpecializationMapEntry spec_entries[3];
	VkSpecializationInfo frag_spec_info;
	VkPipeline *pipeline;

	pipeline = &vk.blur_pipeline[ index ];

	if ( *pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, *pipeline, NULL );
		*pipeline = VK_NULL_HANDLE;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;

	// shaders
	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );

	frag_spec_data[0] = 1.2 / (float) width; // x offset
	frag_spec_data[1] = 1.2 / (float) height; // y offset
	frag_spec_data[2] = 1.0; // intensity?

	if ( horizontal_pass ) {
		frag_spec_data[1] = 0.0;
	} else {
		frag_spec_data[0] = 0.0;
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = 0 * sizeof( float );
	spec_entries[0].size = sizeof( float );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = 1 * sizeof( float );
	spec_entries[1].size = sizeof( float );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = 2 * sizeof( float );
	spec_entries[2].size = sizeof( float );

	frag_spec_info.mapEntryCount = 3;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = 3 * sizeof( float );
	frag_spec_info.pData = &frag_spec_data[0];

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	viewport.x = 0.0;
	viewport.y = 0.0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	scissor.offset.x = viewport.x;
	scissor.offset.y = viewport.y;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
#if FEAT_DEPTH_CLAMP
	rasterization_state.depthClampEnable = r_depthClamp && r_depthClamp->integer ? VK_TRUE : VK_FALSE;
#else
	rasterization_state.depthClampEnable = VK_FALSE;
#endif
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	//rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT; // VK_CULL_MODE_NONE;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = NULL;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_post_process; // one input attachment
	create_info.renderPass = vk.render_pass.blur[ index ];
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pipeline ) );

	SET_OBJECT_NAME( *pipeline, va( "%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// Phase 7.4c-pipeline-followup-4 — bloom blur parallel RAL pipeline.
	// Binding contract: NO push constant; 1 BGL = vk.ral_bgl_sampler
	// (pipeline_layout_post_process declares setLayoutCount=1 over set_layout_sampler).
	// Spec constants: ids 0/1/2 are float x_offset, y_offset, intensity per the
	// existing VkSpecializationMapEntry array above. Topology TRIANGLE_STRIP —
	// gamma.vert emits 4 vertices from a constant array indexed by gl_VertexIndex.
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0 ) {
		vk_ral_special_pipeline_params_t pr;
		ralSpecConstant_t specs[3];
		floatint_t fi0, fi1, fi2;
		memset( &pr,   0, sizeof( pr   ) );
		memset( specs, 0, sizeof( specs ) );
		pr.vs_module          = vk.modules.gamma_vs;
		pr.fs_module          = vk.modules.blur_fs;
		pr.topology           = RAL_TOPOLOGY_TRIANGLE_STRIP;
		pr.cullMode           = RAL_CULL_NONE;
		pr.depthTestEnable    = qfalse;
		pr.depthWriteEnable   = qfalse;
		pr.blendEnable        = qfalse;
		if ( vk.ral_bgl_sampler ) { pr.bgls[ pr.numBgls++ ] = vk.ral_bgl_sampler; }
		fi0.f = frag_spec_data[0];
		fi1.f = frag_spec_data[1];
		fi2.f = frag_spec_data[2];
		specs[0].constantId = 0; specs[0].value = fi0.u;
		specs[1].constantId = 1; specs[1].value = fi1.u;
		specs[2].constantId = 2; specs[2].value = fi2.u;
		pr.specConstants    = specs;
		pr.numSpecConstants = 3;
		pr.debugName        = va( "ral-blur-%s-%u", horizontal_pass ? "h" : "v", index );
		pr.externalLayout   = vk.ral_pipeline_layout_post_process;  // Phase 7.4c-submit-A3
		pr.externalRenderPass = vk.ral_render_pass.blur[ index ];    // blur uses its own per-index pass
		if ( vk.ral_blur_pipeline[ index ] ) { Ral_DestroyPipeline( vk.ral_blur_pipeline[ index ] ); vk.ral_blur_pipeline[ index ] = NULL; }
		vk.ral_blur_pipeline[ index ] = vk_ral_create_special_pipeline( &pr );
	}
}


static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


// ───────────────────────────────────────────────────────────────────────
// Phase 7.4c-pipeline — parallel RAL pipeline construction
//
// Mirrors the legacy create_pipeline()'s state translation onto
// ralGraphicsPipelineCreateInfo_t per docs/phase7-ral-design.md §17.
// Called from create_pipeline() *after* the legacy qvkCreateGraphicsPipelines
// succeeds. Failure here is non-fatal: rendering still uses the legacy
// VkPipeline, the ral_handle slot stays NULL, and a SEV_DEBUG line records
// the cause.
//
// Pipeline-layout caveat (intentional in this turn): numBindGroupLayouts = 0
// — the renderer's VkDescriptorSetLayouts aren't yet exposed as
// ralBindGroupLayout_t*. The Vulkan driver tolerates create-time shader/
// layout mismatch under most builds (Validation will complain via VUID
// -VkGraphicsPipelineCreateInfo-layout-07991). 7.4c-bindgroup-pre will
// adopt the renderer descriptor layouts as ralBindGroupLayout_t and
// flow them through here. Until then, the RAL pipeline is creation-
// validated only — not bind-compatible. That matches the parallel-paths
// invariant (rendering stays on legacy).
// ───────────────────────────────────────────────────────────────────────

static ralFormat_t vk_ral_translate_vk_format( VkFormat f ) {
	switch ( f ) {
	case VK_FORMAT_R8G8B8A8_UNORM:        return RAL_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_R8G8B8A8_SRGB:         return RAL_FORMAT_R8G8B8A8_SRGB;
	case VK_FORMAT_B8G8R8A8_UNORM:        return RAL_FORMAT_B8G8R8A8_UNORM;
	case VK_FORMAT_B8G8R8A8_SRGB:         return RAL_FORMAT_B8G8R8A8_SRGB;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return RAL_FORMAT_A2B10G10R10_UNORM;
	case VK_FORMAT_R16G16B16A16_SFLOAT:   return RAL_FORMAT_R16G16B16A16_SFLOAT;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return RAL_FORMAT_R11G11B10_UFLOAT;
	case VK_FORMAT_D16_UNORM:             return RAL_FORMAT_D16_UNORM;
	case VK_FORMAT_D24_UNORM_S8_UINT:     return RAL_FORMAT_D24_UNORM_S8_UINT;
	case VK_FORMAT_D32_SFLOAT:            return RAL_FORMAT_D32_SFLOAT;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:    return RAL_FORMAT_D32_SFLOAT_S8_UINT;
	default:                              return RAL_FORMAT_UNDEFINED;
	}
}

// Translate state_bits → ralColorBlendAttachment_t. Mirrors create_pipeline's
// switch on GLS_SRCBLEND_BITS / GLS_DSTBLEND_BITS per design doc §17.2.
static ralBlendFactor_t vk_ral_translate_src_blend( unsigned int state_bits ) {
	switch ( state_bits & GLS_SRCBLEND_BITS ) {
	case GLS_SRCBLEND_ZERO:                return RAL_BLEND_ZERO;
	case GLS_SRCBLEND_ONE:                 return RAL_BLEND_ONE;
	case GLS_SRCBLEND_DST_COLOR:           return RAL_BLEND_DST_COLOR;
	case GLS_SRCBLEND_ONE_MINUS_DST_COLOR: return RAL_BLEND_ONE_MINUS_DST_COLOR;
	case GLS_SRCBLEND_SRC_ALPHA:           return RAL_BLEND_SRC_ALPHA;
	case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA: return RAL_BLEND_ONE_MINUS_SRC_ALPHA;
	case GLS_SRCBLEND_DST_ALPHA:           return RAL_BLEND_DST_ALPHA;
	case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA: return RAL_BLEND_ONE_MINUS_DST_ALPHA;
	case GLS_SRCBLEND_ALPHA_SATURATE:      return RAL_BLEND_SRC_ALPHA_SATURATE;
	default:                               return RAL_BLEND_ONE;
	}
}
static ralBlendFactor_t vk_ral_translate_dst_blend( unsigned int state_bits ) {
	switch ( state_bits & GLS_DSTBLEND_BITS ) {
	case GLS_DSTBLEND_ZERO:                return RAL_BLEND_ZERO;
	case GLS_DSTBLEND_ONE:                 return RAL_BLEND_ONE;
	case GLS_DSTBLEND_SRC_COLOR:           return RAL_BLEND_SRC_COLOR;
	case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR: return RAL_BLEND_ONE_MINUS_SRC_COLOR;
	case GLS_DSTBLEND_SRC_ALPHA:           return RAL_BLEND_SRC_ALPHA;
	case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA: return RAL_BLEND_ONE_MINUS_SRC_ALPHA;
	case GLS_DSTBLEND_DST_ALPHA:           return RAL_BLEND_DST_ALPHA;
	case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA: return RAL_BLEND_ONE_MINUS_DST_ALPHA;
	default:                               return RAL_BLEND_ZERO;
	}
}

// Derive (colorFormats[], depthFormat) for a given renderPassIndex. RAL uses
// dynamic rendering — the format list lives on the pipeline. We mirror what
// the legacy VkRenderPass declares so the RAL pipeline can plausibly be
// migrated to dynamic rendering later (7.4c-cmd).
static void vk_ral_renderpass_formats( renderPass_t pass,
                                       ralFormat_t *outColorFormats, uint32_t *outNumColor,
                                       ralFormat_t *outDepthFormat ) {
	// For the parallel-paths era we just need plausible, build-valid format
	// IDs — the RAL pipeline isn't actually bound. Use the main present
	// format and the depth format negotiated at vk_create_attachments.
	*outNumColor = 1;
	outColorFormats[0] = vk_ral_translate_vk_format( vk.color_format );
	if ( outColorFormats[0] == RAL_FORMAT_UNDEFINED )
		outColorFormats[0] = RAL_FORMAT_B8G8R8A8_UNORM;
	*outDepthFormat = vk_ral_translate_vk_format( vk.depth_format );
	if ( *outDepthFormat == RAL_FORMAT_UNDEFINED )
		*outDepthFormat = RAL_FORMAT_D32_SFLOAT;
	(void)pass;   // every renderervk render pass uses the same attachment shape for our purposes
}

// ralFormat translator for the small set of VkFormat values create_pipeline's
// vertex input switch uses (R32G32_SFLOAT, R32G32B32A32_SFLOAT, R8G8B8A8_UNORM).
// Returns RAL_FORMAT_UNDEFINED for unrecognised values — the RAL create then
// fails fast (VUID would catch unset locations anyway).
static ralFormat_t vk_ral_translate_vertex_format( VkFormat f ) {
	switch ( f ) {
	case VK_FORMAT_R32G32_SFLOAT:       return RAL_FORMAT_R32_SFLOAT;          // 2-comp doesn't have a 1:1 RAL match; the legacy uses it for ST coords. Approximate with R32_SFLOAT (caps the storage; shader still reads vec2 correctly because Vulkan auto-clamps to declared size).
	case VK_FORMAT_R32G32B32A32_SFLOAT: return RAL_FORMAT_R32G32B32A32_SFLOAT;
	case VK_FORMAT_R8G8B8A8_UNORM:      return RAL_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_R8G8B8A8_UINT:       return RAL_FORMAT_R8G8B8A8_UNORM;       // approximate — RAL v1 has no UINT 8888 variant; the IQM-skinning bone-index attribute reads via integer cast in the shader.
	default:                            return RAL_FORMAT_UNDEFINED;
	}
}

static ralPipeline_t *vk_ral_create_pipeline_from_def( const Vk_Pipeline_Def *def,
                                                       renderPass_t renderPassIndex,
                                                       uint32_t def_index,
                                                       VkShaderModule vs_module,
                                                       VkShaderModule fs_module,
                                                       unsigned int effective_state_bits ) {
	ralBackend_t *backend = vk_ral_get_backend();
	if ( !backend ) return NULL;

	const uint8_t *vs_bytes = NULL;
	const uint8_t *fs_bytes = NULL;
	uint32_t       vs_size  = 0;
	uint32_t       fs_size  = 0;
	if ( !vk_shader_blob_lookup( vs_module, &vs_bytes, &vs_size )
	  || !vk_shader_blob_lookup( fs_module, &fs_bytes, &fs_size ) ) {
		// Module that didn't go through SHADER_MODULE (shouldn't happen for the
		// centralized helper). Skip; rendering stays on legacy.
		return NULL;
	}

	ralGraphicsPipelineCreateInfo_t gci;
	ralColorBlendAttachment_t       cb;
	ralFormat_t                     colorFormats[ RAL_MAX_COLOR_ATTACHMENTS ];
	uint32_t                        numColorFormats;
	ralFormat_t                     depthFormat;
	ralSpecConstant_t               specConstants[ 14 ];
	uint32_t                        numSpecConstants = 0;
	char                            debugNameBuf[ 96 ];

	memset( &gci, 0, sizeof( gci ) );
	memset( &cb,  0, sizeof( cb  ) );

	gci.vertexSpirv      = (const uint32_t *)vs_bytes;
	gci.vertexSpirvSize  = vs_size;
	gci.fragmentSpirv    = (const uint32_t *)fs_bytes;
	gci.fragmentSpirvSize = fs_size;
	// vertexEntry / fragmentEntry default to "main"

	// ── topology (§17.4 row "primitives") ──────────────────────────────
	switch ( def->primitives ) {
	case LINE_LIST:      gci.topology = RAL_TOPOLOGY_LINE_LIST;      break;
	case POINT_LIST:     gci.topology = RAL_TOPOLOGY_POINT_LIST;     break;
	case TRIANGLE_STRIP: gci.topology = RAL_TOPOLOGY_TRIANGLE_STRIP; break;
	default:             gci.topology = RAL_TOPOLOGY_TRIANGLE_LIST;  break;
	}

	// ── raster (§17.2 GLS_POLYMODE_LINE / §17.3 faceCulling × mirror / §17.4 polygon_offset + line_width) ──
	if ( def->shader_type == TYPE_DOT ) {
		gci.raster.polygonMode = RAL_POLYGON_POINT;
	} else {
		gci.raster.polygonMode = ( effective_state_bits & GLS_POLYMODE_LINE )
		                       ? RAL_POLYGON_LINE : RAL_POLYGON_FILL;
	}
	switch ( def->face_culling ) {
	case CT_TWO_SIDED:
		gci.raster.cullMode = RAL_CULL_NONE;
		break;
	case CT_FRONT_SIDED:
		gci.raster.cullMode = def->mirror ? RAL_CULL_FRONT : RAL_CULL_BACK;
		break;
	case CT_BACK_SIDED:
		gci.raster.cullMode = def->mirror ? RAL_CULL_BACK : RAL_CULL_FRONT;
		break;
	default:
		gci.raster.cullMode = RAL_CULL_NONE;
		break;
	}
	gci.raster.frontFace        = RAL_FRONT_FACE_CW;   // Q3 vertex winding convention (§17.3)
	if ( def->polygon_offset ) {
		gci.raster.depthBiasEnable   = qtrue;
#ifdef USE_REVERSED_DEPTH
		gci.raster.depthBiasConstant = -r_offsetUnits->value;
		gci.raster.depthBiasSlope    = -r_offsetFactor->value;
#else
		gci.raster.depthBiasConstant = r_offsetUnits->value;
		gci.raster.depthBiasSlope    = r_offsetFactor->value;
#endif
		gci.raster.depthBiasClamp    = 0.0f;
	}
	gci.raster.lineWidth = ( def->line_width > 0 ) ? (float)def->line_width : 1.0f;
#if FEAT_DEPTH_CLAMP
	gci.raster.depthClampEnable = ( r_depthClamp && r_depthClamp->integer ) ? qtrue : qfalse;
#endif

	// ── depth/stencil (§17.2 GLS_DEPTH* / §17.4 shadow_phase) ─────────
	gci.depthStencil.depthTestEnable  = ( effective_state_bits & GLS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	gci.depthStencil.depthWriteEnable = ( effective_state_bits & GLS_DEPTHMASK_TRUE )    ? qtrue  : qfalse;
#ifdef USE_REVERSED_DEPTH
	gci.depthStencil.depthCompareOp = ( effective_state_bits & GLS_DEPTHFUNC_EQUAL )
	                                  ? RAL_COMPARE_EQUAL : RAL_COMPARE_GREATER_EQUAL;
#else
	gci.depthStencil.depthCompareOp = ( effective_state_bits & GLS_DEPTHFUNC_EQUAL )
	                                  ? RAL_COMPARE_EQUAL : RAL_COMPARE_LESS_EQUAL;
#endif
	gci.depthStencil.stencilTestEnable = ( def->shadow_phase != SHADOW_DISABLED ) ? qtrue : qfalse;
	if ( def->shadow_phase == SHADOW_EDGES ) {
		gci.depthStencil.stencilFront.failOp      = RAL_STENCIL_OP_KEEP;
		gci.depthStencil.stencilFront.passOp      = ( def->face_culling == CT_FRONT_SIDED )
		                                          ? RAL_STENCIL_OP_INCREMENT_AND_CLAMP
		                                          : RAL_STENCIL_OP_DECREMENT_AND_CLAMP;
		gci.depthStencil.stencilFront.depthFailOp = RAL_STENCIL_OP_KEEP;
		gci.depthStencil.stencilFront.compareOp   = RAL_COMPARE_ALWAYS;
		gci.depthStencil.stencilFront.compareMask = 255;
		gci.depthStencil.stencilFront.writeMask   = 255;
		gci.depthStencil.stencilFront.reference   = 0;
		gci.depthStencil.stencilBack = gci.depthStencil.stencilFront;
	} else if ( def->shadow_phase == SHADOW_FS_QUAD ) {
		gci.depthStencil.stencilFront.failOp      = RAL_STENCIL_OP_KEEP;
		gci.depthStencil.stencilFront.passOp      = RAL_STENCIL_OP_KEEP;
		gci.depthStencil.stencilFront.depthFailOp = RAL_STENCIL_OP_KEEP;
		gci.depthStencil.stencilFront.compareOp   = RAL_COMPARE_NOT_EQUAL;
		gci.depthStencil.stencilFront.compareMask = 255;
		gci.depthStencil.stencilFront.writeMask   = 255;
		gci.depthStencil.stencilFront.reference   = 0;
		gci.depthStencil.stencilBack = gci.depthStencil.stencilFront;
	}

	// ── blend (§17.2 GLS_SRCBLEND_* / GLS_DSTBLEND_*) ─────────────────
	cb.blendEnable = ( effective_state_bits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ? qtrue : qfalse;
	if ( cb.blendEnable ) {
		cb.srcColor = vk_ral_translate_src_blend( effective_state_bits );
		cb.dstColor = vk_ral_translate_dst_blend( effective_state_bits );
		cb.srcAlpha = cb.srcColor;   // alpha mirrors color (Q3 quirk; §17.2 note)
		cb.dstAlpha = cb.dstColor;
		cb.colorOp  = RAL_BLEND_OP_ADD;
		cb.alphaOp  = RAL_BLEND_OP_ADD;
	}
	cb.writeMask = ( def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SINGLE_TEXTURE_DF )
	             ? 0u : RAL_COLOR_WRITE_ALL;
	gci.colorBlends    = &cb;
	gci.numColorBlends = 1;

	// ── attachment formats (renderPassIndex → dynamic-rendering formats) ──
	vk_ral_renderpass_formats( renderPassIndex, colorFormats, &numColorFormats, &depthFormat );
	memcpy( gci.colorFormats, colorFormats, sizeof( gci.colorFormats ) );
	gci.numColorFormats = numColorFormats;
	gci.depthFormat     = depthFormat;
	gci.sampleCount     = 1;   // MSAA retired post-Phase 6B3'-d4

	// ── spec constants (§17.5; subset for parallel-paths era) ─────────
	// alpha_test_func / alpha_test_value, depth_fragment threshold,
	// depth_fade_scale. The full set would also include tex_domain (id 4)
	// + normal_format (id 15) etc. — those carry shader-side state that
	// matters at draw time; for create-time validation the above subset
	// is sufficient.
	{
		floatint_t fi;
		unsigned int atest = effective_state_bits & GLS_ATEST_BITS;
		specConstants[ numSpecConstants ].constantId = 0;
		switch ( atest ) {
		case GLS_ATEST_GT_0:   specConstants[ numSpecConstants ].value = 1; break;
		case GLS_ATEST_LT_80:  specConstants[ numSpecConstants ].value = 2; break;
		case GLS_ATEST_GE_80:  specConstants[ numSpecConstants ].value = 3; break;
		default:               specConstants[ numSpecConstants ].value = 0; break;
		}
		numSpecConstants++;
		specConstants[ numSpecConstants ].constantId = 1;
		switch ( atest ) {
		case GLS_ATEST_GT_0:   fi.f = 0.0f; break;
		case GLS_ATEST_LT_80:  fi.f = 0.5f; break;
		case GLS_ATEST_GE_80:  fi.f = 0.5f; break;
		default:               fi.f = 0.0f; break;
		}
		specConstants[ numSpecConstants ].value = fi.u;
		numSpecConstants++;
		specConstants[ numSpecConstants ].constantId = 2;
		{ floatint_t f2; f2.f = 0.85f; specConstants[ numSpecConstants ].value = f2.u; }
		numSpecConstants++;
		specConstants[ numSpecConstants ].constantId = 11;
		{ floatint_t f3; f3.f = 2.0f; specConstants[ numSpecConstants ].value = f3.u; }
		numSpecConstants++;
	}
	gci.specConstants    = specConstants;
	gci.numSpecConstants = numSpecConstants;

	// ── vertex input — mirror the legacy file-static bindings[]/attribs[] ──
	// (populated by the switch on def->shader_type earlier in create_pipeline).
	// Reduces VUID-VkGraphicsPipelineCreateInfo-Input-07904 noise.
	ralVertexBinding_t   rvbinds[8];
	ralVertexAttribute_t rvattrs[8];
	uint32_t i;
	for ( i = 0; i < num_binds && i < 8u; i++ ) {
		rvbinds[i].binding   = bindings[i].binding;
		rvbinds[i].stride    = bindings[i].stride;
		rvbinds[i].inputRate = RAL_VERTEX_INPUT_PER_VERTEX;
	}
	for ( i = 0; i < num_attrs && i < 8u; i++ ) {
		rvattrs[i].location = attribs[i].location;
		rvattrs[i].binding  = attribs[i].binding;
		rvattrs[i].format   = vk_ral_translate_vertex_format( attribs[i].format );
		rvattrs[i].offset   = attribs[i].offset;
	}
	gci.vertexBindings      = rvbinds;
	gci.numVertexBindings   = num_binds;
	gci.vertexAttributes    = rvattrs;
	gci.numVertexAttributes = num_attrs;

	// ── push constants — mirror renderervk pipeline_layout. 96 bytes covers
	// the main layout's vertex (64 MVP + 32 fog/dlight params), and the MSDF
	// 128-byte layout; we use the larger of the two to keep the RAL pipeline
	// layout compatible across shader_types without per-type branching.
	gci.pushConstantSize    = 128;
	gci.pushConstantStages  = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;

	// ── BindGroupLayouts (Phase 7.4c-bindgroup-pre) ───────────────────
	// Mirror the legacy pipeline_layout's set composition. The renderer
	// builds three pipeline_layouts (vk.c:11850, 11867, 11882) that the
	// centralized helper picks between based on def->shader_type:
	//   TYPE_DOT  → vk.pipeline_layout_storage  = [storage]
	//   TYPE_MSDF → vk.pipeline_layout_msdf     = [uniform, sampler×(N-1)]
	//   else      → vk.pipeline_layout          = [uniform, sampler×(N-1)]
	// N = layoutCount derived from vk.maxBoundDescriptorSets at
	// vk_initialize-time (4-7 depending on hardware). The adopted RAL BGLs
	// land in vk_initialize's tail; if they didn't (RAL not up, or
	// r_useRALPipelines was off at boot) we leave numBindGroupLayouts = 0
	// and accept the layout-07988 VUIDs.
	const ralBindGroupLayout_t *bgls[ 8 ];
	uint32_t numBgls = 0;
	if ( def->shader_type == TYPE_DOT ) {
		if ( vk.ral_bgl_storage ) {
			bgls[ numBgls++ ] = vk.ral_bgl_storage;
		}
	} else {
		if ( vk.ral_bgl_uniform && vk.ral_bgl_sampler ) {
			// Match the layoutCount logic at vk.c:11836-11839 exactly.
			int layoutCount = ( vk.maxBoundDescriptorSets >= VK_DESC_COUNT ) ? VK_DESC_COUNT : 4;
			if ( vk.maxBoundDescriptorSets >= 7 )
				layoutCount = 7;
			bgls[ numBgls++ ] = vk.ral_bgl_uniform;   // set 0
			while ( numBgls < (uint32_t)layoutCount && numBgls < 8u )
				bgls[ numBgls++ ] = vk.ral_bgl_sampler;   // sets 1..N-1
		}
	}
	gci.numBindGroupLayouts = numBgls;
	gci.bindGroupLayouts    = numBgls ? bgls : NULL;

	Com_sprintf( debugNameBuf, sizeof( debugNameBuf ), "ral-pipeline-def#%u-pass#%d", def_index, (int)renderPassIndex );
	gci.debugName = debugNameBuf;

	// Phase 7.4c-submit-A3 — share the renderer's VkPipelineLayout. Matches the
	// shader-type → layout selection inside this function above (TYPE_DOT uses
	// the storage layout, TYPE_MSDF uses the msdf layout, others use the main
	// layout). Layout sharing makes the RAL sibling pipeline layout-compatible
	// with the renderer's vkCmdBindDescriptorSets recorded on the same parallel
	// cmd buffer.
	if ( def->shader_type == TYPE_DOT )
		gci.externalLayout = vk.ral_pipeline_layout_storage;
	else if ( def->shader_type == TYPE_MSDF )
		gci.externalLayout = vk.ral_pipeline_layout_msdf;
	else
		gci.externalLayout = vk.ral_pipeline_layout;

	// Phase 7.4c-submit-A3 — pick the matching ralRenderPass_t per renderPassIndex.
	switch ( renderPassIndex ) {
	case RENDER_PASS_MAIN:       gci.externalRenderPass = vk.ral_render_pass.main;       break;
	case RENDER_PASS_SCREENMAP:  gci.externalRenderPass = vk.ral_render_pass.screenmap;  break;
	case RENDER_PASS_POST_BLOOM: gci.externalRenderPass = vk.ral_render_pass.post_bloom; break;
	default:                     gci.externalRenderPass = NULL;                          break;
	}

	return Ral_CreateGraphicsPipeline( backend, &gci );
}


// ───────────────────────────────────────────────────────────────────────
// Phase 7.4c-pipeline-followup — generic special-case pipeline helper.
//
// The 15 graphics special-case sites (beam Q3/Q1, sprite Q3/Q1, particle
// render alpha/additive Q3/Q1, IQM Q3/Q1, ribbon, SMAA edge/blend/resolve,
// shadow depth, post-process helper, blur helper) each have fixed state.
// We capture the differing values via this params struct + helper so each
// site shrinks to a small "fill struct + call helper" block.
//
// Per §17.7's parallel-paths invariant, the RAL pipeline is allocated but
// not consumed — rendering still uses the legacy VkPipeline. 7.4c-cmd
// flips the bind sites.
// ───────────────────────────────────────────────────────────────────────

// (Struct definition + forward declaration moved to the top of this file
// near the qvk* statics, so the 15 special-case create sites earlier in
// the file can stack-allocate the params struct.)
static ralPipeline_t *vk_ral_create_special_pipeline( const vk_ral_special_pipeline_params_t *p ) {
	ralBackend_t *backend = vk_ral_get_backend();
	if ( !backend ) return NULL;

	const uint8_t *vs_bytes = NULL;
	const uint8_t *fs_bytes = NULL;
	uint32_t       vs_size  = 0;
	uint32_t       fs_size  = 0;
	if ( !vk_shader_blob_lookup( p->vs_module, &vs_bytes, &vs_size )
	  || !vk_shader_blob_lookup( p->fs_module, &fs_bytes, &fs_size ) ) {
		return NULL;   // shader module didn't pass through SHADER_MODULE(); skip
	}

	ralGraphicsPipelineCreateInfo_t gci;
	ralColorBlendAttachment_t       cb;
	memset( &gci, 0, sizeof( gci ) );
	memset( &cb,  0, sizeof( cb  ) );

	gci.vertexSpirv       = (const uint32_t *)vs_bytes;
	gci.vertexSpirvSize   = vs_size;
	gci.fragmentSpirv     = (const uint32_t *)fs_bytes;
	gci.fragmentSpirvSize = fs_size;

	gci.topology = p->topology;

	gci.raster.polygonMode        = p->polygonMode;
	gci.raster.cullMode           = p->cullMode;
	gci.raster.frontFace          = ( p->frontFace == 0 ) ? RAL_FRONT_FACE_CW : p->frontFace;
	gci.raster.depthBiasEnable    = p->depthBiasEnable;
	gci.raster.depthBiasConstant  = p->depthBiasConstant;
	gci.raster.depthBiasSlope     = p->depthBiasSlope;
	gci.raster.lineWidth          = ( p->lineWidth > 0.0f ) ? p->lineWidth : 1.0f;

	gci.depthStencil.depthTestEnable  = p->depthTestEnable;
	gci.depthStencil.depthWriteEnable = p->depthWriteEnable;
	gci.depthStencil.depthCompareOp   = ( p->depthCompareOp == RAL_COMPARE_NEVER )
	                                  ? RAL_COMPARE_LESS_EQUAL : p->depthCompareOp;
	gci.depthStencil.stencilTestEnable = qfalse;

	cb.blendEnable = p->blendEnable;
	if ( p->blendEnable ) {
		cb.srcColor = p->srcColor;
		cb.dstColor = p->dstColor;
		cb.colorOp  = p->blendOp;
		cb.srcAlpha = p->srcColor;
		cb.dstAlpha = p->dstColor;
		cb.alphaOp  = p->blendOp;
	}
	cb.writeMask = p->colorWriteMask ? p->colorWriteMask : RAL_COLOR_WRITE_ALL;
	gci.colorBlends    = &cb;
	gci.numColorBlends = 1;

	// Attachment formats. For depth-only sites (shadow caster), numColorAttachments
	// is explicitly 0 to match the legacy renderpass shape.
	uint32_t numColor = ( p->numColorAttachments == 0 && p->colorFormat == RAL_FORMAT_UNDEFINED )
	                  ? 1u
	                  : p->numColorAttachments;
	// Phase 7.4c-submit-A3: explicit depth-only signal — shadow caster sets
	// p->depthOnly=qtrue so the helper produces a 0-color-attachment pipeline
	// (otherwise post-process pipelines that share the same "numColorAttachments=0
	// + colorFormat=UNDEFINED" defaults would also collapse to 0 — wrong, they're
	// truly 1-attachment).
	if ( p->depthOnly ) {
		gci.numColorBlends = 0;
		gci.colorBlends    = NULL;
		numColor           = 0;
	}
	if ( numColor > 0 ) {
		gci.colorFormats[0] = ( p->colorFormat != RAL_FORMAT_UNDEFINED )
		                    ? p->colorFormat
		                    : vk_ral_translate_vk_format( vk.color_format );
		if ( gci.colorFormats[0] == RAL_FORMAT_UNDEFINED )
			gci.colorFormats[0] = RAL_FORMAT_B8G8R8A8_UNORM;
	}
	gci.numColorFormats = numColor;
	gci.depthFormat     = ( p->depthFormat != RAL_FORMAT_UNDEFINED )
	                    ? p->depthFormat
	                    : vk_ral_translate_vk_format( vk.depth_format );
	if ( gci.depthFormat == RAL_FORMAT_UNDEFINED )
		gci.depthFormat = RAL_FORMAT_D32_SFLOAT;
	gci.sampleCount     = p->sampleCount ? p->sampleCount : 1u;

	gci.vertexBindings      = p->vbinds;
	gci.numVertexBindings   = p->numVbinds;
	gci.vertexAttributes    = p->vattrs;
	gci.numVertexAttributes = p->numVattrs;

	gci.pushConstantSize   = p->pushConstantSize;
	gci.pushConstantStages = p->pushConstantStages;

	gci.bindGroupLayouts    = p->numBgls ? p->bgls : NULL;
	gci.numBindGroupLayouts = p->numBgls;

	// Phase 7.4c-pipeline-followup-4 — VkSpecializationInfo support.
	// Sites that don't use spec constants zero-init numSpecConstants;
	// the helper threads NULL/0 through (RAL backend skips the
	// VkSpecializationInfo build).
	gci.specConstants    = p->numSpecConstants ? p->specConstants : NULL;
	gci.numSpecConstants = p->numSpecConstants;

	// Phase 7.4c-submit-A3 — thread external layout + render pass (or NULL → fallback path).
	gci.externalLayout      = p->externalLayout;
	gci.externalRenderPass  = p->externalRenderPass;
	gci.externalSubpass     = p->externalSubpass;

	gci.debugName = p->debugName ? p->debugName : "ral-special-pipeline";

	return Ral_CreateGraphicsPipeline( backend, &gci );
}


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[13]; // blob slots: 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8:ident.color, 9:ident.alpha, 10:acff, 11:depth_fade_scale, 12:normal_format (Phase 6.5.2 — light_frag.tmpl USE_PARALLAX, constant id 15; the A3 srgb that used to occupy id 15 is gone)
	VkSpecializationMapEntry spec_entries[14]; // [0] = vertex clip_plane (disabled), [1..12] = fragment constant IDs 0..11, [13] = fragment constant id 15 (normal_format)
	//VkSpecializationInfo vert_spec_info;
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	unsigned int atest_bits;
	unsigned int state_bits = def->state_bits;

	switch ( def->shader_type ) {

		case TYPE_SINGLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][0];
			break;

#if FEAT_PARALLAX_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX:
			vs_module = &vk.modules.vert.light_parallax[0];
			fs_module = &vk.modules.frag.light_parallax[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR:
			vs_module = &vk.modules.vert.light_parallax[0];
			fs_module = &vk.modules.frag.light_parallax[1][0];
			break;
#endif

#if FEAT_ADVANCED_WATER
		case TYPE_WATER:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.water_fs;
			break;
#endif

#if FEAT_SHADOW_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW:
			vs_module = &vk.modules.light_shadow[0];
			fs_module = &vk.modules.light_shadow_frag[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR:
			vs_module = &vk.modules.light_shadow[0];
			fs_module = &vk.modules.light_shadow_frag[1][0];
			break;

		case TYPE_SHADOW_DEPTH:
			vs_module = &vk.modules.shadow_depth_vs;
			fs_module = &vk.modules.shadow_depth_fs;
			state_bits |= GLS_DEPTHMASK_TRUE;
			break;
#endif

#if FEAT_PBR
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.light_pbr_frag[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.light_pbr_frag[1][0];
			break;
#endif

		case TYPE_SINGLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = &vk.modules.vert.gen[0][0][0][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[0][0][1][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[0][1][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[1][0][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[1][1][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[1][0][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[1][1][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[1][0][0][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[1][0][1][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[2][0][0][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[2][0][1][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[1][1][0][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[1][1][1][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[2][1][0][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[2][1][1][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = &vk.modules.dot_fs;
			break;

		case TYPE_MSDF:
			vs_module = &vk.modules.msdf_vs;
			fs_module = &vk.modules.msdf_fs;
			break;

		case TYPE_LIGHTSTYLES:
			vs_module = &vk.modules.q1_ls_vs;
			fs_module = &vk.modules.q1_ls_fs;
			break;

		case TYPE_LIGHTSTYLES_ARRAY:
			vs_module = &vk.modules.q1_ls_vs;       // vertex stage is identical to TYPE_LIGHTSTYLES
			fs_module = &vk.modules.q1_ls_array_fs;
			break;

		default:
			ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_MSDF:
			case TYPE_LIGHTSTYLES:
			case TYPE_LIGHTSTYLES_ARRAY:
			case TYPE_SINGLE_TEXTURE_DF:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module++;
				fs_module++;
				break;
		}
	}

	// depth fade: swap fragment module for single-texture blended surfaces
	if ( vk.depthFade.active && (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ) {
		int fogIdx = def->fog_stage ? 1 : 0;
		switch ( def->shader_type ) {
			case TYPE_SIGNLE_TEXTURE:
			case TYPE_SINGLE_TEXTURE_ENV:
				fs_module = &vk.modules.frag.dfade_gen[0][fogIdx];
				break;
			case TYPE_SINGLE_TEXTURE_IDENTITY:
			case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
				fs_module = &vk.modules.frag.dfade_ident1[0][fogIdx];
				break;
			case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
			case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
				fs_module = &vk.modules.frag.dfade_fixed[0][fogIdx];
				break;
			case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
				fs_module = &vk.modules.frag.dfade_ent[0][fogIdx];
				break;
			default:
				break; // no depth fade variant for multi-texture
		}
	}

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

	//memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	memset( frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
		case GLS_ATEST_GT_0:
			frag_spec_data[0].i = 1; // not equal
			frag_spec_data[1].f = 0.0f;
			break;
		case GLS_ATEST_LT_80:
			frag_spec_data[0].i = 2; // less than
			frag_spec_data[1].f = 0.5f;
			break;
		case GLS_ATEST_GE_80:
			frag_spec_data[0].i = 3; // greater or equal
			frag_spec_data[1].f = 0.5f;
			break;
		default:
			frag_spec_data[0].i = 0;
			frag_spec_data[1].f = 0.0f;
			break;
	};

	// depth fragment threshold
	frag_spec_data[2].f = 0.85f;

	// depth fade scale (soft particles)
	frag_spec_data[11].f = 2.0f;

	// Phase 6.5.2: tangent-space normal-map encoding for light_frag.tmpl's
	// USE_PARALLAX variants (constant id 15). 0 for every non-parallax def
	// (memset-zeroed at the def-build site), and those shaders don't declare
	// id 15 so the value is ignored regardless.
	frag_spec_data[12].i = def->normal_format;

#if 0
	if ( r_ext_alpha_to_coverage->integer && vkSamples != VK_SAMPLE_COUNT_1_BIT && frag_spec_data[0].i ) {
		frag_spec_data[3].i = 1;
		alphaToCoverage = VK_TRUE;
	}
#endif

	// spec constant id 4. For the solid-colour shader (color.frag) it is
	// `color_mode` (white/green/red). For every other shader type — which
	// declares id 4 as `tex_domain` (gen_frag.tmpl / light_frag.tmpl /
	// water.frag, Block 5d) — it carries the per-slot colour-domain bitmask
	// (bit N = texture slot N is CD_LINEAR → raw fetch). A pipeline is one
	// shader type, so the slot is unambiguous; types that declare neither id 4
	// (e.g. depth-fragment) ignore the value harmlessly.
	switch ( def->shader_type ) {
		default:               frag_spec_data[4].i = def->tex_domain; break;
		case TYPE_COLOR_WHITE: frag_spec_data[4].i = 1; break;
		case TYPE_COLOR_GREEN: frag_spec_data[4].i = 2; break;
		case TYPE_COLOR_RED:   frag_spec_data[4].i = 3; break;
	}

	// abs lighting
	switch ( def->shader_type ) {
		case TYPE_SINGLE_TEXTURE_LIGHTING:
		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
#if FEAT_PARALLAX_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR:
#endif
#if FEAT_ADVANCED_WATER
		case TYPE_WATER:
#endif
#if FEAT_SHADOW_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW:
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR:
#endif
#if FEAT_PBR
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR:
#endif
			frag_spec_data[5].i = def->abs_light ? 1 : 0;
		default:
			break;
	}

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data[6].i = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data[6].i = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data[6].i = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data[6].i = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data[6].i = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data[6].i = 7;
			break;

		default:
			break;
	}

	frag_spec_data[8].f = ((float)def->color.rgb) / 255.0;
	frag_spec_data[9].f = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data[10].i = def->acff;
	} else {
		frag_spec_data[10].i = 0;
	}

	// Phase 6B3'-d4-m_final (Block 1): the A3/A4 `srgb` blob slot
	// (spec-constant id 15) is retired — colour-texel sRGB->linear
	// decode is unconditional in gen_frag.tmpl / light_frag.tmpl now.
	// Id 15 is left a hole (spec-constant ids need not be contiguous).

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof( int32_t );
	spec_entries[0].size = sizeof( int32_t );

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof( int32_t );
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//

	spec_entries[1].constantID = 0;  // alpha-test-function
	spec_entries[1].offset = 0 * sizeof( int32_t );
	spec_entries[1].size = sizeof( int32_t );

	spec_entries[2].constantID = 1; // alpha-test-value
	spec_entries[2].offset = 1 * sizeof( int32_t );
	spec_entries[2].size = sizeof( float );

	spec_entries[3].constantID = 2; // depth-fragment
	spec_entries[3].offset = 2 * sizeof( int32_t );
	spec_entries[3].size = sizeof( float );

	spec_entries[4].constantID = 3; // alpha-to-coverage
	spec_entries[4].offset = 3 * sizeof( int32_t );
	spec_entries[4].size = sizeof( int32_t );

	spec_entries[5].constantID = 4; // color_mode
	spec_entries[5].offset = 4 * sizeof( int32_t );
	spec_entries[5].size = sizeof( int32_t );

	spec_entries[6].constantID = 5; // abs_light
	spec_entries[6].offset = 5 * sizeof( int32_t );
	spec_entries[6].size = sizeof( int32_t );

	spec_entries[7].constantID = 6; // multitexture mode
	spec_entries[7].offset = 6 * sizeof( int32_t );
	spec_entries[7].size = sizeof( int32_t );

	spec_entries[8].constantID = 7; // discard mode
	spec_entries[8].offset = 7 * sizeof( int32_t );
	spec_entries[8].size = sizeof( int32_t );

	spec_entries[9].constantID = 8; // fixed color
	spec_entries[9].offset = 8 * sizeof( int32_t );
	spec_entries[9].size = sizeof( float );

	spec_entries[10].constantID = 9; // fixed alpha
	spec_entries[10].offset = 9 * sizeof( int32_t );
	spec_entries[10].size = sizeof( float );

	spec_entries[11].constantID = 10; // acff
	spec_entries[11].offset = 10 * sizeof( int32_t );
	spec_entries[11].size = sizeof( int32_t );

	spec_entries[12].constantID = 11; // depth_fade_scale
	spec_entries[12].offset = 11 * sizeof( int32_t );
	spec_entries[12].size = sizeof( float );

	// Phase 6.5.2: blob slot 12 -> fragment constant id 15 = normal_format
	// (light_frag.tmpl USE_PARALLAX). Reuses the id-15 hole the A3/A4 srgb
	// constant left behind; harmlessly ignored by every shader that doesn't
	// declare id 15.
	spec_entries[13].constantID = 15; // normal_format
	spec_entries[13].offset = 12 * sizeof( int32_t );
	spec_entries[13].size = sizeof( int32_t );

	frag_spec_info.mapEntryCount = 13;
	frag_spec_info.pMapEntries = spec_entries + 1;
	frag_spec_info.dataSize = sizeof( int32_t ) * 13;
	frag_spec_info.pData = &frag_spec_data[0];

	// MSDF fragment shader has its own specialization layout (constant_id=0 is
	// msdf_distance_range, not alpha-test-function). Shader compiled-in default
	// is 8.0, matching all shipped atlases (pxrange 8). Pass NULL to use default.
	if ( def->shader_type == TYPE_MSDF || def->shader_type == TYPE_LIGHTSTYLES
	  || def->shader_type == TYPE_LIGHTSTYLES_ARRAY )
		shader_stages[1].pSpecializationInfo = NULL;
	else
		shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch ( def->shader_type ) {

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_DF:
		case TYPE_SINGLE_TEXTURE_IDENTITY:
		case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
		case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MSDF:
		case TYPE_SIGNLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING:
		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
#if FEAT_PARALLAX_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR:
#endif
#if FEAT_ADVANCED_WATER
		case TYPE_WATER:
#endif
#if FEAT_SHADOW_MAPPING
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW:
		case TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR:
#endif
#if FEAT_PBR
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR:
		case TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR:
#endif
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

#if FEAT_SHADOW_MAPPING
		case TYPE_SHADOW_DEPTH:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array only
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;
#endif

		case TYPE_LIGHTSTYLES:
		case TYPE_LIGHTSTYLES_ARRAY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		default:
			ri.Terminate( TERM_CLIENT_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
#if FEAT_DEPTH_CLAMP
	rasterization_state.depthClampEnable = r_depthClamp && r_depthClamp->integer ? VK_TRUE : VK_FALSE;
#else
	rasterization_state.depthClampEnable = VK_FALSE;
#endif
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	if ( def->shader_type == TYPE_DOT ) {
		rasterization_state.polygonMode = VK_POLYGON_MODE_POINT;
	} else {
		rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SINGLE_TEXTURE_DF)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Terminate( TERM_CLIENT_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
	dynamic_state.pDynamicStates = dynamic_state_array;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;

	if ( def->shader_type == TYPE_DOT )
		create_info.layout = vk.pipeline_layout_storage;
	else if ( def->shader_type == TYPE_MSDF )
		create_info.layout = vk.pipeline_layout_msdf;
	else
		create_info.layout = vk.pipeline_layout;

	if ( renderPassIndex == RENDER_PASS_SCREENMAP )
		create_info.renderPass = vk.render_pass.screenmap;
	else
		create_info.renderPass = vk.render_pass.main;

	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline ) );

	SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	// Phase 7.4c-pipeline — parallel RAL pipeline construction. Gated on
	// r_useRALPipelines; failure is non-fatal (slot stays NULL, legacy
	// VkPipeline still drives rendering). def_index points into vk.pipelines[].
	if ( r_useRALPipelines && r_useRALPipelines->integer != 0
	  && def_index < vk.pipelines_count
	  && vs_module && fs_module ) {
		ralPipeline_t *ral = vk_ral_create_pipeline_from_def( def, renderPassIndex, def_index,
		                                                     *vs_module, *fs_module, state_bits );
		vk.pipelines[ def_index ].ral_handle[ renderPassIndex ] = ral;
		if ( !ral ) {
			ri.Log( SEV_DEBUG, "[VK->RAL] parallel ralPipeline_t creation returned NULL (def#%u, pass#%d, shader_type=%d)\n",
			        def_index, (int)renderPassIndex, (int)def->shader_type );
		}
	}

	return pipeline;
}


static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def ) {
	VK_Pipeline_t *pipeline;
	if ( vk.pipelines_count >= MAX_VK_PIPELINES ) {
		ri.Terminate( TERM_CLIENT_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached" );
		return 0;
	}
	int j;
	pipeline	  = &vk.pipelines[vk.pipelines_count];
	pipeline->def = *def;
	for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
		pipeline->handle[j] = VK_NULL_HANDLE;
	}
	return vk.pipelines_count++;
}


VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = create_pipeline( &pipeline->def, pass, index );
		}
		return pipeline->handle[ pass ];
	}
	ri.Terminate( TERM_UNRECOVERABLE, "%s(%i): NULL pipeline", __func__, index );
	return VK_NULL_HANDLE;
}


uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use ) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for ( index = base; index < vk.pipelines_count; index++ ) {
		cur_def = &vk.pipelines[ index ].def;
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			goto found;
		}
	}

	index = vk_alloc_pipeline( def );
found:

	if ( use )
		vk_gen_pipeline( index );

	return index;
}


void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def ) {
	if ( pipeline >= vk.pipelines_count ) {
		memset( def, 0, sizeof( *def ) );
	} else {
		memcpy( def, &vk.pipelines[ pipeline ].def, sizeof( *def ) );
	}
}


static void get_viewport_rect(VkRect2D *r)
{
	if ( backEnd.projection2D )
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if (r->offset.x + r->extent.width > glConfig.vidWidth)
			r->extent.width = glConfig.vidWidth - r->offset.x;
		if (r->offset.y + r->extent.height > glConfig.vidHeight)
			r->extent.height = glConfig.vidHeight - r->offset.y;
	}
}


static void get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D )
	{
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
#else
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
#endif
	}
	else
	{
		const float *p = backEnd.viewParms.projectionMatrix;
		float proj[16];
		memcpy( proj, p, 64 );

		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		proj[5] = -p[5];
		//proj[10] = ( p[10] - 1.0f ) / 2.0f;
		//proj[14] = p[14] / 2.0f;
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}


void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
	// Phase 7.4c-cmd — parallel-paths clear-attachments (color).
	Ral_CmdClearAttachments( vk.cmd->ral_cmd, 1, (const ralClearAttachment_t *)&attachment, 1, (const ralClearRect_t *)&clear_rect );
}


void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
	// Phase 7.4c-cmd — parallel-paths clear-attachments (depth).
	Ral_CmdClearAttachments( vk.cmd->ral_cmd, 1, (const ralClearAttachment_t *)&attachment, 1, (const ralClearRect_t *)clear_rect );
}


void vk_update_mvp( const float *m ) {
	float push_constants[16]; // mvp transform

	//
	// Specify push constants.
	//
	if ( m )
		memcpy( push_constants, m, sizeof( push_constants ) );
	else
		get_mvp_transform( push_constants );

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );
	// Phase 7.4c-cmd — parallel-paths push-constants (MVP).
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );

	vk.stats.push_size += sizeof( push_constants );
}


/*
====================
vk_set_2d_scissor

Sets a scissor rect on the current command buffer. Accepts a 4-int array
{x, y, w, h} in pixel coordinates, or NULL to restore the full-screen
scissor. Used by RE_SetClipRegion to clamp 2D drawing to a rectangular
region. Wraps the static qvkCmdSetScissor binding so non-vk.c callers can
drive it.
====================
*/
void vk_set_2d_scissor( const int *rect ) {
	VkRect2D scissor;
	if ( !vk.active || !vk.cmd ) {
		return;
	}
	if ( rect ) {
		int x = rect[0];
		int y = rect[1];
		int w = rect[2];
		int h = rect[3];
		if ( x < 0 ) x = 0;
		if ( y < 0 ) y = 0;
		if ( w < 0 ) w = 0;
		if ( h < 0 ) h = 0;
		scissor.offset.x = x;
		scissor.offset.y = y;
		scissor.extent.width = (uint32_t)w;
		scissor.extent.height = (uint32_t)h;
	} else {
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = glConfig.vidWidth;
		scissor.extent.height = glConfig.vidHeight;
	}
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
	// Phase 7.4c-cmd — parallel-paths scissor (2D clip).
	Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);
}


#if FEAT_FOG_SYSTEM
/*
====================
vk_update_fog_push

Push the enhanced-fog parameters as a 32-byte fragment-stage push constant
at offset 64 (right after the 64-byte vertex MVP). This is a REAL push
constant upload via qvkCmdPushConstants; the pipeline layout has been
extended to reserve this range so any future fog fragment shader that wants
to read it can do so without further layout changes.

Layout (std430, 32 bytes):
  float fogColorR;    // offset 64
  float fogColorG;    // offset 68
  float fogColorB;    // offset 72
  float density;      // offset 76
  int   fogType;      // offset 80   (0=none, 1=linear, 2=exp, 3=exp2)
  float farClip;      // offset 84
  int   enabled;      // offset 88
  float _pad;         // offset 92
====================
*/
void vk_update_fog_push( const vec4_t color, int fogType, float density, float farClip, qboolean enabled ) {
	struct {
		float r, g, b, density;
		int   type;
		float farClip;
		int   enabled;
		float pad;
	} push;

	push.r       = color[0];
	push.g       = color[1];
	push.b       = color[2];
	push.density = density;
	push.type    = fogType;
	push.farClip = farClip;
	push.enabled = enabled ? 1 : 0;
	push.pad     = 0.0f;

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof( push ), &push );
	// Phase 7.4c-cmd — parallel-paths push-constants (fog).
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout),
		VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof( push ), &push );

	vk.stats.push_size += sizeof( push );
}
#endif // FEAT_FOG_SYSTEM


void vk_update_msdf_outline( float outlineWidth, const float *outlineColor,
                              float glowWidth, const float *glowColor,
                              const float *shadowOffset, const float *shadowColor )
{
	// Re-push MVP (bytes 0-63) via the MSDF layout so push constants are valid
	// for the currently bound MSDF pipeline.
	float mvp[16];
	get_mvp_transform( mvp );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_msdf,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( mvp ), mvp );
	// Phase 7.4c-cmd — parallel-paths push-constants (MSDF MVP).
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout_msdf),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( mvp ), mvp );

	// Push outline/glow/shadow params at offset 64 (64 bytes, std430 aligned)
	// Layout: outlineWidth(4) + glowWidth(4) + shadowOffset(8) + outlineColor(16) + glowColor(16) + shadowColor(16)
	struct {
		float outlineWidth;
		float glowWidth;
		float shadowOffset[2];
		float outlineColor[4];
		float glowColor[4];
		float shadowColor[4];
	} params;

	static const float zero4[4] = { 0, 0, 0, 0 };
	static const float zero2[2] = { 0, 0 };

	params.outlineWidth    = outlineWidth;
	params.glowWidth       = glowWidth;
	memcpy( params.shadowOffset, shadowOffset ? shadowOffset : zero2, sizeof( params.shadowOffset ) );

	// Phase 6B3'-d4-m2: decode the colour push constants to linear domain.
	// msdf.frag composites the layers (col = col*(1-a) + layer*a) in linear;
	// callers supply sRGB-encoded display values. Decoded once here per fill
	// rather than per-fragment in the shader. Alpha is not sRGB-encoded — copied
	// verbatim. (The vertex-stream text colour is decoded shader-side instead,
	// since it arrives as a normalised byte attribute.)
	{
		const float *oc = outlineColor ? outlineColor : zero4;
		const float *gc = glowColor    ? glowColor    : zero4;
		const float *sc = shadowColor  ? shadowColor  : zero4;
		params.outlineColor[0] = R_SRGBToLinear( oc[0] );
		params.outlineColor[1] = R_SRGBToLinear( oc[1] );
		params.outlineColor[2] = R_SRGBToLinear( oc[2] );
		params.outlineColor[3] = oc[3];
		params.glowColor[0] = R_SRGBToLinear( gc[0] );
		params.glowColor[1] = R_SRGBToLinear( gc[1] );
		params.glowColor[2] = R_SRGBToLinear( gc[2] );
		params.glowColor[3] = gc[3];
		params.shadowColor[0] = R_SRGBToLinear( sc[0] );
		params.shadowColor[1] = R_SRGBToLinear( sc[1] );
		params.shadowColor[2] = R_SRGBToLinear( sc[2] );
		params.shadowColor[3] = sc[3];
	}

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_msdf,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		64, sizeof( params ), &params );
	// Phase 7.4c-cmd — parallel-paths push-constants (MSDF style params).
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout_msdf),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		64, sizeof( params ), &params );

	vk.stats.push_size += sizeof( mvp ) + sizeof( params );
}


static VkBuffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	}
	memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
	vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	return offset;
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset ) {
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );
		// Phase 7.4c-cmd — parallel-paths bind-index-buffer.
		Ral_CmdBindIndexBuffer(vk.cmd->ral_cmd, vk_ral_lookup_buffer(buffer), offset, RAL_INDEX_UINT32);
	}

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths draw-indexed (VBO path).
	Ral_CmdDrawIndexed( vk.cmd->ral_cmd, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );
		// Phase 7.4c-submit-A2 — typed parallel-paths bind-vertex-buffers (VBO geometry).
		{ ralBuffer_t *rbufs[8]; int rb_i;
		  for (rb_i = 0; rb_i < bind_count; rb_i++) rbufs[rb_i] = vk_ral_lookup_buffer( shade_bufs[bind_base + rb_i] );
		  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, bind_base, bind_count, rbufs, (const uint64_t *)( vk.cmd->vbo_offset + bind_base ) ); }

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
		// Phase 7.4c-submit-A2 — typed parallel-paths bind-vertex-buffers (CPU-streamed geometry).
		{ ralBuffer_t *rbufs[8]; int rb_i;
		  for (rb_i = 0; rb_i < bind_count; rb_i++) rbufs[rb_i] = vk_ral_lookup_buffer( shade_bufs[bind_base + rb_i] );
		  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, bind_base, bind_count, rbufs, (const uint64_t *)( vk.cmd->buf_offset + bind_base ) ); }
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );
		// Phase 7.4c-submit-A2 — typed parallel-paths bind-vertex-buffers (lighting VBO).
		{ ralBuffer_t *rbufs[3] = { vk_ral_lookup_buffer(shade_bufs[0]), vk_ral_lookup_buffer(shade_bufs[1]), vk_ral_lookup_buffer(shade_bufs[2]) };
		  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, 0, 3, rbufs, (const uint64_t *)( vk.cmd->vbo_offset + 0 ) ); }

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
		// Phase 7.4c-submit-A2 — typed parallel-paths bind-vertex-buffers (lighting CPU-streamed).
		{ ralBuffer_t *rbufs[8]; int rb_i;
		  for (rb_i = 0; rb_i < bind_count; rb_i++) rbufs[rb_i] = vk_ral_lookup_buffer( shade_bufs[bind_base + rb_i] );
		  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, bind_base, bind_count, rbufs, (const uint64_t *)( vk.cmd->buf_offset + bind_base ) ); }
	}
}


// Phase 7.4c-cmd — per-frame ring adoption helper. Walks the boot-time
// adoption registry first (so the common case of binding a long-lived
// VkDescriptorSet doesn't allocate a fresh wrapper every update); falls
// back to creating a transient Ral_AdoptBindGroup wrapper for per-image
// descriptors (allocated at R_CreateImage time, not yet in the registry).
// `layoutHint` is used for the transient adoption — when NULL or the RAL
// backend isn't up, skips the wrapper creation (Ral_Cmd*Vk consumers
// silently no-op via the NULL guard at lookup time).
static void vk_ral_adopt_ring_slot( int index, VkDescriptorSet vkSet )
{
	ralBackend_t          *b;
	ralBindGroup_t        *existing;
	struct ralBindGroupLayout_s *layoutHint;

	// Free any wrapper currently parked in this slot — about to be replaced.
	if ( vk.cmd->descriptor_set.current_ral[ index ] != NULL ) {
		Ral_DestroyBindGroup( vk.cmd->descriptor_set.current_ral[ index ] );
		vk.cmd->descriptor_set.current_ral[ index ] = NULL;
	}

	if ( vkSet == VK_NULL_HANDLE )
		return;

	// Fast path: boot-time adoption registry already wraps this set.
	existing = vk_ral_lookup_bindgroup( vkSet );
	if ( existing != NULL ) {
		// We do NOT take ownership of registry entries here — those are owned
		// by s_adopted_bgs[] for the program lifetime. Leaving slot NULL means
		// vk_ral_parallel_bind_descriptor_sets falls back to the registry
		// scan and finds the same wrapper. Wrapper refcount stays at 1.
		return;
	}

	// Slow path: per-image descriptor not yet adopted. Adopt with the sampler
	// BGL (per-image sets all use vk.set_layout_sampler). Skip when the BGL
	// hasn't been adopted yet (RAL backend not up, or pre-vk_init_descriptors).
	b = vk_ral_get_backend();
	if ( b == NULL )
		return;
	layoutHint = vk.ral_bgl_sampler;
	if ( layoutHint == NULL )
		return;
	vk.cmd->descriptor_set.current_ral[ index ] =
		Ral_AdoptBindGroup( b, (void *)vkSet, layoutHint, "wired-frame-ring-bg" );
	if ( vk.cmd->descriptor_set.current_ral[ index ] != NULL )
		vk_ral_descset_adopt_count++;
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
	// Phase 7.4c-cmd: clear the matching ring wrapper.
	if ( vk.cmd->descriptor_set.current_ral[ index ] != NULL ) {
		Ral_DestroyBindGroup( vk.cmd->descriptor_set.current_ral[ index ] );
		vk.cmd->descriptor_set.current_ral[ index ] = NULL;
	}
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( index < vk.cmd->descriptor_set.start ) ? index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( index > vk.cmd->descriptor_set.end ) ? index : vk.cmd->descriptor_set.end;
		// Phase 7.4c-cmd: rotate the per-frame ring wrapper alongside the
		// VkDescriptorSet slot. Only fires on actual change (the surrounding
		// `if` already filters duplicate writes).
		vk_ral_adopt_ring_slot( index, descriptor );
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}


void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[2], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	end = vk.cmd->descriptor_set.end;

	offset_count = 0;
	if ( /*start == VK_DESC_STORAGE || */ start == VK_DESC_UNIFORM ) { // uniform offset or storage offset
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[ start ];
	}

	count = end - start + 1;

	// fill NULL descriptor gaps
	for ( i = start + 1; i < end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	/* Bind via the layout the currently-bound pipeline was created with
	 * (set by vk_bind_pipeline). Falls back to vk.pipeline_layout when no
	 * pipeline is bound — matches the historical hardcoded behavior for
	 * that pre-bind window and after callers that reset last_pipeline =
	 * VK_NULL_HANDLE (IQM / bloom / etc.) to force a rebind. */
	{
		VkPipelineLayout layout = vk.cmd->last_pipeline != VK_NULL_HANDLE
		                          ? vk.cmd->last_pipeline_layout
		                          : vk.pipeline_layout;
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record. The
		// per-frame rotating sets in vk.cmd->descriptor_set.current[] are NOT
		// adopted in 7.4c-bindgroup (deferred to 7.4c-cmd's per-frame adoption
		// pass); vk_ral_lookup_bindgroup returns NULL for them and the helper
		// is a no-op, so this call is effectively skipped here today. The
		// rare static-set hits (storage / color / depthFade if routed via
		// vk_bind_descriptor_sets) still record correctly. TODO_7.4c-cmd:
		// remove this conditional skip once per-frame sets are adopted.
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );
	}

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	vkpipe = vk_gen_pipeline( pipeline );

	if ( vkpipe != vk.cmd->last_pipeline ) {
		/* Track which layout this pipeline was created with so
		 * vk_bind_descriptor_sets binds via a compatible layout. Mirrors the
		 * layout selection inside vk_create_pipeline (TYPE_DOT/TYPE_MSDF/default). */
		const int shader_type = vk.pipelines[ pipeline ].def.shader_type;
		if ( shader_type == TYPE_DOT )
			vk.cmd->last_pipeline_layout = vk.pipeline_layout_storage;
		else if ( shader_type == TYPE_MSDF )
			vk.cmd->last_pipeline_layout = vk.pipeline_layout_msdf;
		else
			vk.cmd->last_pipeline_layout = vk.pipeline_layout;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline (vk_bind_pipeline).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vkpipe ));
		vk.cmd->last_pipeline = vkpipe;
		vk_diag_pipebinds++;
		vk_diag_msdf_active = ( pipeline == vk.msdf_pipeline );
		if ( vk_diag_msdf_active )
			vk_diag_msdf_binds++;
	}

	vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			// Phase 7.4c-cmd — parallel-paths scissor (depth-range update).
			Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor_rect);
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
		// Phase 7.4c-cmd — parallel-paths viewport (depth-range update).
		Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths draw-indexed (vk_draw_geometry).
		Ral_CmdDrawIndexed( vk.cmd->ral_cmd, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths draw (vk_draw_geometry).
		Ral_CmdDraw( vk.cmd->ral_cmd, tess.numVertexes, 1, 0, 0 );
	}
	// NOLINTNEXTLINE(readability-misleading-indentation) — Q3 split-else-if / preprocessor-conditional idiom; statement is at correct enclosing scope
	vk_diag_drawcalls++;
	if ( vk_diag_msdf_active )
		vk_diag_msdf_draws++;
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record (storage SSBO).
	vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths draw (vk_draw_dot).
	Ral_CmdDraw( vk.cmd->ral_cmd, tess.numVertexes, 1, 0, 0 );
}


static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	// Phase 7.4c-cmd — parallel-paths begin-render-pass.
	vk_ral_parallel_begin_render_pass( &render_pass_begin_info );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


/* Single-attachment variant for post-process passes (bloom_extract, blur,
 * capture, gamma).  Each of these passes was created with
 * VK_ATTACHMENT_LOAD_OP_CLEAR on its sole color attachment, so the begin
 * info must supply exactly one VkClearValue or VUID-clearValueCount-00902
 * fires.  vk_begin_render_pass()'s clearValueCount=2-or-3 shape is for
 * color+depth(+msaa) and doesn't fit here. */
static void vk_begin_render_pass_clear1( VkRenderPass renderPass, VkFramebuffer frameBuffer, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_value;

	memset( &clear_value, 0, sizeof( clear_value ) );

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount = 1;
	render_pass_begin_info.pClearValues = &clear_value;

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	// Phase 7.4c-cmd — parallel-paths begin-render-pass (single-attachment variant).
	vk_ral_parallel_begin_render_pass( &render_pass_begin_info );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.main, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_begin_post_bloom_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_POST_BLOOM;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


/*
================
vk_tonemap  (Block 8 / Delta 2 — UI-agnostic)

Scene-radiance post-process pass. Reads vk.color_image (img 264 — HDR
scene + bloom composition), writes vk.tonemapped_image (img 265 — LDR,
still linear-light). Variant selection (SSAO, tonemap operator, colour
grading, godrays) happens at draw time.

This is now purely the tonemap step: it ends whatever scene pass is open
(render_pass.main, or post_bloom from vk_bloom), runs the fullscreen
tonemap into img 265, then ends render_pass.tonemap. On return: no render
pass is open and img 265 is in SHADER_READ_ONLY_OPTIMAL (render_pass.tonemap's
finalLayout). It knows nothing about UI — vk_open_ui_pass() is a separate,
independent function — and it does no once-guarding (the 3D→2D transition
orchestrator in tr_backend.c, gated by backEnd.doneUIPass, decides when to
call it).

No-op during the screenmap pass (mirror/portal views don't tonemap) and
when !fboActive.
================
*/
void vk_tonemap( void )
{
	int varIdx = 0;
	qboolean needsDepth = qfalse;
	VkPipeline tonePipeline = VK_NULL_HANDLE;
	VkPipelineLayout pLayout = vk.pipeline_layout_post_process;

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
		return;
	if ( !vk.fboActive )
		return;

#if FEAT_SSAO
	if ( r_ssao->integer && vk.depthFade.active ) { varIdx |= TONEMAP_VAR_SSAO; needsDepth = qtrue; }
#endif
#if FEAT_TONEMAP
	if ( r_tonemap->integer ) varIdx |= TONEMAP_VAR_BASE;
#endif
#if FEAT_COLOR_GRADING
	if ( r_colorGrading->integer ) varIdx |= TONEMAP_VAR_CG;
#endif
#if FEAT_GODRAYS
	if ( r_godRays->integer && vk.depthFade.active ) { varIdx |= TONEMAP_VAR_GODRAYS; needsDepth = qtrue; }
#endif

	if ( varIdx && vk.tonemap_variants[ varIdx ] != VK_NULL_HANDLE ) {
		tonePipeline = vk.tonemap_variants[ varIdx ];
		if ( varIdx & TONEMAP_VAR_GODRAYS )
			pLayout = vk.pipeline_layout_godrays;
		else if ( varIdx & TONEMAP_VAR_SSAO )
			pLayout = vk.pipeline_layout_ssao;
		else
			pLayout = vk.pipeline_layout_post_process;
	} else {
		tonePipeline = vk.tonemap_pipeline;
		pLayout = vk.pipeline_layout_post_process;
		varIdx = 0;
		needsDepth = qfalse;
	}

	// End the open scene pass (post_bloom from vk_bloom, or render_pass.main
	// from vk_begin_frame when bloom is off).
	vk_end_render_pass();

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_clear1( vk.render_pass.tonemap, vk.framebuffers.tonemap, vk.renderWidth, vk.renderHeight );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonePipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline (tonemap).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(tonePipeline ));
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 0, 1, &vk.color_descriptor, 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record (color sampler).
	vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 0, 1, &vk.color_descriptor, 0, NULL );
	if ( needsDepth ) {
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 1, 1, &vk.depthFade.descriptor, 0, NULL );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record (depthFade sampler).
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 1, 1, &vk.depthFade.descriptor, 0, NULL );
	}
#if FEAT_GODRAYS
	if ( varIdx & TONEMAP_VAR_GODRAYS ) {
		float pushData[4];
		pushData[0] = r_sunX->value;
		pushData[1] = r_sunY->value;
		pushData[2] = 0.5f;
		pushData[3] = 0.97f;
		qvkCmdPushConstants( vk.cmd->command_buffer, pLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, pushData );
		// Phase 7.4c-cmd — parallel-paths push-constants (godrays).
		Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(pLayout), VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, pushData );
	}
#endif

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths draw (tonemap).
	Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );

	// End the tonemap pass. img 265 → SHADER_READ_ONLY_OPTIMAL. No pass open.
	vk_end_render_pass();
}


/*
================
vk_open_ui_pass  (Block 8 / Delta 2)

Open the 2D compositing pass on img 265. Knows nothing about tonemap.

  clear == qtrue  (pure-2D frames — menu / loading screen): tonemap was
    never called this frame, so render_pass.main (img 264) is still open
    from vk_begin_frame. End it, then begin render_pass.ui_clear on img
    265 with loadOp=CLEAR (img 264's main-pass clear is discarded — nothing
    of value was drawn there). 2D draws land in a fresh black img 265 with
    NO tonemap operator applied.

  clear == qfalse (gameplay frames): the orchestrator already ran
    vk_tonemap() (and vk_smaa() if r_smaa>0), which left a clean state —
    no render pass open. Begin render_pass.ui on img 265 with loadOp=LOAD
    so the HUD blends on top of the tonemapped (+ SMAA'd) scene.

On return: a UI pass is open on img 265; renderPassIndex = RENDER_PASS_MAIN
(render_pass.ui / ui_clear are pipeline-compatible with render_pass.main per
Vulkan §8.2 — same 2 attachments: R16F colour + D24S8 depth, single-sample —
so 2D pipelines built against render_pass.main bind here unchanged).
================
*/
void vk_open_ui_pass( qboolean clear )
{
	if ( !vk.fboActive )
		return;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	if ( clear ) {
		// Pure-2D path: end render_pass.main (img 264) first, then begin the
		// CLEAR-mode UI pass on img 265. render_pass.ui_clear's colour
		// attachment is loadOp=CLEAR (idx 0) and depth is DONT_CARE (idx 1),
		// so clearValueCount=1 (vk_begin_render_pass_clear1) satisfies
		// VUID-clearValueCount-00902.
		vk_end_render_pass();
		vk_begin_render_pass_clear1( vk.render_pass.ui_clear, vk.framebuffers.ui_clear, vk.renderWidth, vk.renderHeight );
	} else {
		// Gameplay path: no render pass open (vk_tonemap()/vk_smaa() left
		// a clean state) — just begin the LOAD-mode UI pass.
		vk_begin_render_pass( vk.render_pass.ui, vk.framebuffers.ui, qfalse, vk.renderWidth, vk.renderHeight );
	}
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


/*
================
vk_depth_fade_copy

End current render pass, copy the depth buffer to the sampable depth copy image,
then begin the depth fade render pass (which loads color+depth without clearing).
This is called at the opaque->transparent transition to enable soft particle rendering.
================
*/
void vk_depth_fade_copy( void )
{
	VkImageMemoryBarrier barriers[2];
	VkFramebuffer frameBuffer;

	if ( !vk.depthFade.active || vk.depthFade.copied )
		return;

	// end the current render pass
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	// Phase 7.4c-cmd — parallel-paths end-render-pass (depthFade copy).
	Ral_CmdEndRenderPass( vk.cmd->ral_cmd );

	// barrier: depth attachment -> transfer src
	memset( barriers, 0, sizeof( barriers ) );

	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = vk.depth_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		barriers[0].subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;

	// barrier: depth copy image -> transfer dst
	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = vk.depthFade.image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
	// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier (depthFade
	// pre-copy). Phase 7.4c-submit-BC-Pre extended the internal-texture sweep
	// to adopt depthFade.image alongside depth/color/tonemapped/smaa — the
	// lookup miss is now closed; the typed call fires normally.
	{
		static qboolean warned;
		vk_ral_parallel_pipeline_barrier_image(
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			2, barriers, &warned, "depthFade-pre-copy" );
	}

	// copy depth — single-sample only after MSAA retire
	{
		VkImageCopy region;
		memset( &region, 0, sizeof( region ) );
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.dstSubresource.layerCount = 1;
		region.extent.width = glConfig.vidWidth;
		region.extent.height = glConfig.vidHeight;
		region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer,
			vk.depth_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.depthFade.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region );
		// Phase 7.4c-submit-A4 — typed parallel-paths copy-image (depthFade
		// depth->copy). Phase 7.4c-submit-BC-Pre — depthFade.image now adopted;
		// the typed call fires normally.
		{
			static qboolean warned;
			vk_ral_parallel_copy_image( vk.depth_image, vk.depthFade.image,
				1, &region, &warned, "depthFade-depth-to-copy" );
		}
	}

	// barrier: depth back to attachment, depth copy to shader read
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
	// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier (depthFade
	// post-copy). Phase 7.4c-submit-BC-Pre — depthFade.image now adopted;
	// the typed call fires normally.
	{
		static qboolean warned;
		vk_ral_parallel_pipeline_barrier_image(
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			2, barriers, &warned, "depthFade-post-copy" );
	}

	// begin depth fade render pass (loads color+depth)
	frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN; // still in "main" context for pipeline compatibility
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.depth_fade, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );

	// bind the depth copy descriptor to set 5
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout, VK_DESC_DEPTH_FADE, 1, &vk.depthFade.descriptor, 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record (depthFade copy).
	vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout, VK_DESC_DEPTH_FADE, 1, &vk.depthFade.descriptor, 0, NULL );

	vk.depthFade.copied = qtrue;
}


/*
================
vk_smaa

Perform SMAA anti-aliasing as a post-process. Block 8: the source/target
is the tonemapped image (img 265) — post-tonemap and pre-UI — instead of
the pre-tonemap HDR scene (img 264, whose old SMAA output had no
downstream reader). Called only from the 3D→2D transition gameplay branch
(tr_backend.c), between vk_tonemap() and vk_open_ui_pass(qfalse).

Precondition: no render pass open (vk_tonemap() ended render_pass.tonemap).
Postcondition: no render pass open (the caller opens render_pass.ui next).

  1. Copy tonemapped_image -> input_image
  2. Edge detection pass (input -> edges)
  3. Blend weight calculation (edges + area + search -> blend)
  4. Neighborhood blending / resolve (input + blend -> tonemapped_image)
================
*/
void vk_smaa( void )
{
	VkImageMemoryBarrier barriers[2];
	VkImageCopy copy_region;
	float rtMetrics[4];
	VkClearValue clear_value;

	// Phase 6B4: vk.smaa.active is the resource-alloc gate. Dispatch
	// also requires r_smaa->integer > 0 AND the live pipelines to be
	// built (vk_create_smaa_pipelines tears them down when r_smaa hits
	// 0). The caller (the transition gameplay branch) is itself once-
	// guarded by backEnd.doneUIPass, so this runs at most once per frame;
	// this defensive guard handles r_smaa = 0 and the live-toggle /
	// acquire-failed corner cases.
	if ( !vk.smaa.active || r_smaa->integer == 0 || vk.smaa_edge_pipeline == VK_NULL_HANDLE )
		return;

	rtMetrics[0] = 1.0f / (float)glConfig.vidWidth;
	rtMetrics[1] = 1.0f / (float)glConfig.vidHeight;
	rtMetrics[2] = (float)glConfig.vidWidth;
	rtMetrics[3] = (float)glConfig.vidHeight;

	// No render pass open here — vk_tonemap() ended render_pass.tonemap
	// before the caller invoked us.
	vk_gpu_ts_write( "smaa" ); // outside render pass — MoltenVK timestamps only resolve at encoder boundaries

	// --- Step 0: Copy tonemapped_image -> input_image ---

	// barrier: tonemapped_image SHADER_READ -> TRANSFER_SRC, input_image SHADER_READ -> TRANSFER_DST
	memset( barriers, 0, sizeof( barriers ) );

	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = vk.tonemapped_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = vk.smaa.input_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
	// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier (SMAA
	// pre-copy). Both barriers reference adopted textures (tonemapped + smaa.input),
	// the SMAA siblings are gated by vk.fboActive at adoption time — matching
	// the vk.smaa.active gate at the top of vk_smaa(). Typed call fires normally.
	{
		static qboolean warned;
		vk_ral_parallel_pipeline_barrier_image(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			2, barriers, &warned, "smaa-pre-copy" );
	}

	// copy
	memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.srcSubresource.layerCount = 1;
	copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.dstSubresource.layerCount = 1;
	copy_region.extent.width = glConfig.vidWidth;
	copy_region.extent.height = glConfig.vidHeight;
	copy_region.extent.depth = 1;

	qvkCmdCopyImage( vk.cmd->command_buffer,
		vk.tonemapped_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.smaa.input_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy_region );
	// Phase 7.4c-submit-A4 — typed parallel-paths copy-image (SMAA tonemap->input).
	// Both textures adopted under vk.fboActive at boot.
	{
		static qboolean warned;
		vk_ral_parallel_copy_image( vk.tonemapped_image, vk.smaa.input_image,
			1, &copy_region, &warned, "smaa-tonemap-to-input" );
	}

	// barrier: both back to SHADER_READ_ONLY
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
	// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier (SMAA post-copy).
	{
		static qboolean warned;
		vk_ral_parallel_pipeline_barrier_image(
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			2, barriers, &warned, "smaa-post-copy" );
	}

	// --- Pass 1: Edge detection ---
	memset( &clear_value, 0, sizeof( clear_value ) );
	{
		VkRenderPassBeginInfo rp_info;
		memset( &rp_info, 0, sizeof( rp_info ) );
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.renderPass = vk.render_pass.smaa_edge;
		rp_info.framebuffer = vk.framebuffers.smaa_edge;
		rp_info.renderArea.extent.width = glConfig.vidWidth;
		rp_info.renderArea.extent.height = glConfig.vidHeight;
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &clear_value;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE );
		// Phase 7.4c-cmd — parallel-paths begin-render-pass (SMAA edge).
		vk_ral_parallel_begin_render_pass( &rp_info );
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.smaa_edge_pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline + push-constants (SMAA edge).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.smaa_edge_pipeline ));
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout_smaa),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );

	// set 0: input image
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_smaa, 0, 1, &vk.smaa.input_descriptor, 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record (SMAA edge pass).
	vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_smaa, 0, 1, &vk.smaa.input_descriptor, 0, NULL );

	qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	// Phase 7.4c-cmd — parallel-paths draw + end-render-pass (SMAA edge).
	Ral_CmdDraw         ( vk.cmd->ral_cmd, 3, 1, 0, 0 );
	Ral_CmdEndRenderPass( vk.cmd->ral_cmd );

	// --- Pass 2: Blend weight calculation ---
	{
		VkRenderPassBeginInfo rp_info;
		memset( &rp_info, 0, sizeof( rp_info ) );
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.renderPass = vk.render_pass.smaa_blend;
		rp_info.framebuffer = vk.framebuffers.smaa_blend;
		rp_info.renderArea.extent.width = glConfig.vidWidth;
		rp_info.renderArea.extent.height = glConfig.vidHeight;
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &clear_value;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE );
		// Phase 7.4c-cmd — parallel-paths begin-render-pass (SMAA blend).
		vk_ral_parallel_begin_render_pass( &rp_info );
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.smaa_blend_pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline + push-constants (SMAA blend).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.smaa_blend_pipeline ));
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout_smaa),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );

	// set 0: edges, set 1: area LUT, set 2: search LUT
	{
		VkDescriptorSet sets[3];
		sets[0] = vk.smaa.edges_descriptor;
		sets[1] = vk.smaa.area_descriptor;
		sets[2] = vk.smaa.search_descriptor;
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_smaa, 0, 3, sets, 0, NULL );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record (SMAA blend pass).
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_smaa, 0, 3, sets, 0, NULL );
	}

	qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	// Phase 7.4c-cmd — parallel-paths draw + end-render-pass (SMAA blend).
	Ral_CmdDraw         ( vk.cmd->ral_cmd, 3, 1, 0, 0 );
	Ral_CmdEndRenderPass( vk.cmd->ral_cmd );

	// --- Pass 3: Neighborhood blending / resolve ---
	{
		VkRenderPassBeginInfo rp_info;
		VkClearValue resolve_clear;
		memset( &resolve_clear, 0, sizeof( resolve_clear ) );
		memset( &rp_info, 0, sizeof( rp_info ) );
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_info.renderPass = vk.render_pass.smaa_resolve;
		rp_info.framebuffer = vk.framebuffers.smaa_resolve;
		rp_info.renderArea.extent.width = glConfig.vidWidth;
		rp_info.renderArea.extent.height = glConfig.vidHeight;
		rp_info.clearValueCount = 1;
		rp_info.pClearValues = &resolve_clear;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE );
		// Phase 7.4c-cmd — parallel-paths begin-render-pass (SMAA resolve).
		vk_ral_parallel_begin_render_pass( &rp_info );
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.smaa_resolve_pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline + push-constants (SMAA resolve).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.smaa_resolve_pipeline ));
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );
	Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.pipeline_layout_smaa),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( rtMetrics ), rtMetrics );

	// set 0: input (original color), set 1: blend weights
	{
		VkDescriptorSet sets[3];
		sets[0] = vk.smaa.input_descriptor;
		sets[1] = vk.smaa.blend_descriptor;
		sets[2] = vk.smaa.blend_descriptor; // padding for layout compatibility (3 sets)
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_smaa, 0, 3, sets, 0, NULL );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record (SMAA resolve pass).
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_smaa, 0, 3, sets, 0, NULL );
	}

	qvkCmdDraw( vk.cmd->command_buffer, 3, 1, 0, 0 );
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	// Phase 7.4c-cmd — parallel-paths draw + end-render-pass (SMAA resolve).
	Ral_CmdDraw         ( vk.cmd->ral_cmd, 3, 1, 0, 0 );
	Ral_CmdEndRenderPass( vk.cmd->ral_cmd );
}


void vk_begin_bloom_extract_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.bloom_extract;

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth;
	vk.renderHeight = gls.captureHeight;

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_clear1( vk.render_pass.bloom_extract, frameBuffer, vk.renderWidth, vk.renderHeight );
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_clear1( vk.render_pass.blur[ index ], frameBuffer, vk.renderWidth, vk.renderHeight );
}


static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_end_render_pass( void )
{
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	// Phase 7.4c-cmd — parallel-paths end-render-pass (centralized).
	Ral_CmdEndRenderPass( vk.cmd->ral_cmd );

//	vk.renderPassIndex = RENDER_PASS_MAIN;
}


static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}


// ---------------------------------------------------------------------------
// Background fence-wait thread
// Moves vkWaitForFences + vkResetFences off the main thread so game simulation
// and command recording are not blocked by Metal's drawable-release latency.
// With NUM_COMMAND_BUFFERS=3 at 100 FPS (10ms frames), each slot is reused after
// 30ms. Metal releases the drawable after ~16.67ms. The thread finishes its wait
// ~13ms before the main thread ever needs the slot — making the main-thread wait
// effectively 0ms in steady state.
// ---------------------------------------------------------------------------
#ifndef _WIN32
typedef struct {
	int     slot;
	VkFence fence;
} vk_fence_work_t;

static pthread_t         vk_ft_thread;
static pthread_mutex_t   vk_ft_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t    vk_ft_cwork  = PTHREAD_COND_INITIALIZER;  // main -> thread: new fence
static pthread_cond_t    vk_ft_cready = PTHREAD_COND_INITIALIZER;  // thread -> main: slot free
static qboolean          vk_ft_running;
static qboolean          vk_slot_ready[ NUM_COMMAND_BUFFERS ];
static vk_fence_work_t   vk_ft_queue[ NUM_COMMAND_BUFFERS * 2 ];
static int               vk_ft_head;
static int               vk_ft_tail;

static void *vk_fence_worker( void *arg )
{
	int     slot;
	VkFence fen;

	pthread_mutex_lock( &vk_ft_mutex );
	while ( vk_ft_running || vk_ft_head != vk_ft_tail ) {
		while ( vk_ft_head == vk_ft_tail && vk_ft_running )
			pthread_cond_wait( &vk_ft_cwork, &vk_ft_mutex );

		if ( vk_ft_head == vk_ft_tail )
			break;

		slot = vk_ft_queue[ vk_ft_head ].slot;
		fen  = vk_ft_queue[ vk_ft_head ].fence;
		vk_ft_head = ( vk_ft_head + 1 ) % ( NUM_COMMAND_BUFFERS * 2 );
		pthread_mutex_unlock( &vk_ft_mutex );

		{
			int t0 = ri.Milliseconds();
			qvkWaitForFences( vk.device, 1, &fen, VK_FALSE, (uint64_t)10000000000ULL );
			qvkResetFences( vk.device, 1, &fen );
			pthread_mutex_lock( &vk_ft_mutex );
			vk_diag_ft_fence_ms += ri.Milliseconds() - t0;
		}
		vk_slot_ready[ slot ] = qtrue;
		pthread_cond_broadcast( &vk_ft_cready );
	}
	pthread_mutex_unlock( &vk_ft_mutex );
	return NULL;
}

static void vk_fence_thread_start( void )
{
	pthread_attr_t attr;
	vk_ft_head = vk_ft_tail = 0;
	vk_ft_running = qtrue;
	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_slot_ready[ i ] = qfalse; // set to qtrue by fence thread after each vkResetFences
	pthread_attr_init( &attr );
#ifdef __APPLE__
	// Boost to user-interactive QoS so macOS schedules this thread on a performance
	// core and preempts it promptly — prevents the main thread from arriving at
	// vk_slot_wait before the fence thread has had a chance to run vkWaitForFences.
	pthread_attr_set_qos_class_np( &attr, QOS_CLASS_USER_INTERACTIVE, 0 );
#endif
	pthread_create( &vk_ft_thread, &attr, vk_fence_worker, NULL );
	pthread_attr_destroy( &attr );
}

static void vk_fence_thread_stop( void )
{
	pthread_mutex_lock( &vk_ft_mutex );
	vk_ft_running = qfalse;
	pthread_cond_signal( &vk_ft_cwork );
	pthread_mutex_unlock( &vk_ft_mutex );
	pthread_join( vk_ft_thread, NULL );
}

// Called after vkQueueSubmit: hand fence to background thread.
static void vk_fence_submit( int slot, VkFence fence )
{
	pthread_mutex_lock( &vk_ft_mutex );
	vk_ft_queue[ vk_ft_tail ].slot  = slot;
	vk_ft_queue[ vk_ft_tail ].fence = fence;
	vk_ft_tail = ( vk_ft_tail + 1 ) % ( NUM_COMMAND_BUFFERS * 2 );
	pthread_cond_signal( &vk_ft_cwork );
	pthread_mutex_unlock( &vk_ft_mutex );
}

// Called at start of vk_begin_frame: wait (usually instant) for slot to be free.
static void vk_slot_wait( int slot )
{
	pthread_mutex_lock( &vk_ft_mutex );
	while ( !vk_slot_ready[ slot ] )
		pthread_cond_wait( &vk_ft_cready, &vk_ft_mutex );
	vk_slot_ready[ slot ] = qfalse;
	pthread_mutex_unlock( &vk_ft_mutex );
}

#else
// Windows: no pthread — the thread functions are stubs; fence wait happens
// synchronously in vk_begin_frame via the original vkWaitForFences path.
static void vk_fence_thread_start( void ) {}
static void vk_fence_thread_stop( void )  {}
static void vk_fence_submit( int slot, VkFence fence ) { (void)slot; (void)fence; }
static void vk_slot_wait( int slot ) { (void)slot; }
#endif  // !_WIN32

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

// ---------------------------------------------------------------------------
// GPU per-pass timestamp module
// Usage: r_gpuSpeeds 0=off 1=200f-avg N>=2=per-frame when total>=N ms
// ---------------------------------------------------------------------------

#define VK_GPU_TS_MAX 16

static qboolean    vk_gpu_ts_active;
// Phase 7.4c-submit-A4: dropped `static` so vk_ral_textures.c's
// vk_ral_lookup_query_pool can compare against the live VkQueryPool handle.
// The ral sibling below holds the adopted wrapper; both fields are
// effectively module-private (single translation-unit consumer outside
// vk.c is vk_ral_textures.c, via the lookup helper).
VkQueryPool        vk_gpu_ts_pool;
struct ralQueryPool_s *vk_gpu_ts_ral_pool;  // Phase 7.4c-submit-A4 — adopted parallel-paths sibling for typed Ral_Cmd{ResetQueryPool,WriteTimestamp}
static uint32_t    vk_gpu_ts_count;
static const char *vk_gpu_ts_labels[ VK_GPU_TS_MAX ];

static struct {
	qboolean    pending;
	uint32_t    base;
	uint32_t    count;
	const char *labels[ VK_GPU_TS_MAX ];
} vk_gpu_ts_inflight[ NUM_COMMAND_BUFFERS ];

static double      vk_gpu_ts_accum_ms[ VK_GPU_TS_MAX ];
static const char *vk_gpu_ts_accum_labels[ VK_GPU_TS_MAX ];
static uint32_t    vk_gpu_ts_accum_slots;
static uint32_t    vk_gpu_ts_accum_frames;

static void vk_gpu_ts_init( void )
{
	VkQueryPoolCreateInfo info;

	vk_gpu_ts_active = qfalse;
	vk_gpu_ts_pool = VK_NULL_HANDLE;
	vk_gpu_ts_count = 0;
	vk_gpu_ts_accum_slots = 0;
	vk_gpu_ts_accum_frames = 0;
	memset( vk_gpu_ts_inflight, 0, sizeof( vk_gpu_ts_inflight ) );
	memset( vk_gpu_ts_accum_ms, 0, sizeof( vk_gpu_ts_accum_ms ) );

	if ( !vk.timestampSupported ) {
		ri.Log( SEV_DEBUG, "r_gpuSpeeds: device lacks timestampComputeAndGraphics, disabled\n" );
		return;
	}

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk_gpu_ts_inflight[ i ].base = i * VK_GPU_TS_MAX;
	}

	memset( &info, 0, sizeof( info ) );
	info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = VK_GPU_TS_MAX * NUM_COMMAND_BUFFERS;

	if ( qvkCreateQueryPool( vk.device, &info, NULL, &vk_gpu_ts_pool ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "r_gpuSpeeds: vkCreateQueryPool failed\n" );
		return;
	}

	vk_gpu_ts_active = qtrue;

	// Phase 7.4c-submit-A4: adopt the freshly-created VkQueryPool into the
	// parallel-paths typed RAL wrapper. Runs AFTER the internal-texture adoption
	// sweep (vk_ral_adopt_static_internal_textures fires from
	// vk_ral_adopt_static_bindgroups's tail during vk_init_descriptors — which
	// runs strictly before vk_gpu_ts_init in vk_initialize). ownsPool=qfalse
	// so Ral_DestroyQueryPool from vk_gpu_ts_shutdown only frees the wrapper —
	// the underlying VkQueryPool stays owned by the qvkCreateQueryPool /
	// qvkDestroyQueryPool pair here. Guarded by vk_ral_get_backend() so the
	// adoption skips cleanly when neither r_useRALTextures nor r_useRALBuffers
	// is set (the RAL backend doesn't come up).
	{
		struct ralBackend_s *ralBackend = vk_ral_get_backend();
		if ( ralBackend ) {
			if ( vk_gpu_ts_ral_pool ) { Ral_DestroyQueryPool( vk_gpu_ts_ral_pool ); vk_gpu_ts_ral_pool = NULL; }
			vk_gpu_ts_ral_pool = Ral_AdoptQueryPool( ralBackend, (void *)vk_gpu_ts_pool, RAL_QUERY_TIMESTAMP, info.queryCount, "wired-qp-gpu-ts" );
			if ( vk_gpu_ts_ral_pool )
				ri.Log( SEV_INFO, "[VK->RAL] adopted vk_gpu_ts_pool as ralQueryPool_t (queryCount=%u)\n", info.queryCount );
		}
	}
}

static void vk_gpu_ts_shutdown( void )
{
	// Phase 7.4c-submit-A4: destroy the adopted RAL wrapper BEFORE the
	// underlying VkQueryPool. ownsPool=qfalse on the wrapper so only the
	// wrapper struct is freed; the qvkDestroyQueryPool below owns the
	// VkQueryPool lifetime. Also covers the REF_KEEP_CONTEXT case where
	// vk_ral_textures_shutdown's full-teardown branch is skipped — this
	// site keeps the wrapper-pool pairing consistent across map transitions.
	if ( vk_gpu_ts_ral_pool ) {
		Ral_DestroyQueryPool( vk_gpu_ts_ral_pool );
		vk_gpu_ts_ral_pool = NULL;
	}
	if ( vk_gpu_ts_pool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, vk_gpu_ts_pool, NULL );
		vk_gpu_ts_pool = VK_NULL_HANDLE;
	}
	vk_gpu_ts_active = qfalse;
}

static void vk_gpu_ts_frame_begin( void )
{
	// host-side readback of the previous frame's results for this command-buffer slot
	uint64_t results[ 2 * VK_GPU_TS_MAX ];  // (value, availability) pairs
	int slot;
	double total_ms;
	uint32_t i;
	int gate;

	if ( !vk_gpu_ts_active )
		return;

	if ( !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;

	gate = r_gpuSpeeds->integer;
	slot = vk.cmd_index;

	if ( vk_gpu_ts_inflight[ slot ].pending && vk_gpu_ts_inflight[ slot ].count >= 2 ) {
		memset( results, 0, sizeof( results ) );
		qvkGetQueryPoolResults( vk.device, vk_gpu_ts_pool,
			vk_gpu_ts_inflight[ slot ].base,
			vk_gpu_ts_inflight[ slot ].count,
			vk_gpu_ts_inflight[ slot ].count * 2 * sizeof( uint64_t ), results,
			2 * sizeof( uint64_t ),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT );

		// accumulate inter-timestamp deltas: label[i] covers [i-1 .. i]
		// Only update slots/labels on first valid readback so we never print null labels.
		for ( i = 1; i < vk_gpu_ts_inflight[ slot ].count; i++ ) {
			if ( results[ 2*i + 1 ] && results[ 2*(i-1) + 1 ] ) {
				double delta_ns = (double)( results[ 2*i ] - results[ 2*(i-1) ] ) * (double)vk.timestampPeriodNs;
				vk_gpu_ts_accum_ms[ i - 1 ] += delta_ns * 1e-6;
				vk_gpu_ts_accum_labels[ i - 1 ] = vk_gpu_ts_inflight[ slot ].labels[ i ];
				if ( i - 1 >= vk_gpu_ts_accum_slots )
					vk_gpu_ts_accum_slots = i;  // = i, not i-1, since slots is 1-based count
			}
		}
	}

	vk_gpu_ts_inflight[ slot ].pending = qfalse;

	++vk_gpu_ts_accum_frames;

	// print in averaged mode (gate==1) or threshold mode (gate>=2, per-frame)
	if ( gate == 1 && vk_gpu_ts_accum_frames < 200 )
		return;

	total_ms = 0.0;
	for ( i = 0; i < vk_gpu_ts_accum_slots; i++ )
		total_ms += vk_gpu_ts_accum_ms[ i ];

	if ( gate >= 2 ) {
		// per-frame threshold: only print if total >= gate ms
		if ( vk_gpu_ts_accum_frames < 1 || total_ms < (double)gate )
			goto reset_accum;
		ri.Log( SEV_DEBUG, "gpu (ms):" );
		for ( i = 0; i < vk_gpu_ts_accum_slots; i++ )
			ri.Log( SEV_DEBUG, "  %s=%.2f", vk_gpu_ts_accum_labels[ i ], vk_gpu_ts_accum_ms[ i ] );
		ri.Log( SEV_DEBUG, "  total=%.2f\n", total_ms );
	} else {
		// averaged mode
		double n = (double)vk_gpu_ts_accum_frames;
		ri.Log( SEV_DEBUG, "gpu (%df avg, ms):", vk_gpu_ts_accum_frames );
		for ( i = 0; i < vk_gpu_ts_accum_slots; i++ )
			ri.Log( SEV_DEBUG, "  %s=%.2f", vk_gpu_ts_accum_labels[ i ], vk_gpu_ts_accum_ms[ i ] / n );
		ri.Log( SEV_DEBUG, "  total=%.2f\n", total_ms / n );
	}

reset_accum:
	memset( vk_gpu_ts_accum_ms, 0, sizeof( vk_gpu_ts_accum_ms ) );
	vk_gpu_ts_accum_slots = 0;
	vk_gpu_ts_accum_frames = 0;
}

static void vk_gpu_ts_pool_reset( void )
{
	// CB-side reset + first "acquire" timestamp — must run after qvkBeginCommandBuffer
	if ( !vk_gpu_ts_active || !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;

	qvkCmdResetQueryPool( vk.cmd->command_buffer, vk_gpu_ts_pool,
		vk_gpu_ts_inflight[ vk.cmd_index ].base, VK_GPU_TS_MAX );
	// Phase 7.4c-submit-A4 — typed parallel-paths reset-query-pool.
	{
		static qboolean warned;
		vk_ral_parallel_reset_query_pool( vk_gpu_ts_pool,
			vk_gpu_ts_inflight[ vk.cmd_index ].base, VK_GPU_TS_MAX,
			&warned, "gpu-ts-pool-reset" );
	}

	vk_gpu_ts_count = 0;
	qvkCmdWriteTimestamp( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		vk_gpu_ts_pool,
		vk_gpu_ts_inflight[ vk.cmd_index ].base + vk_gpu_ts_count );
	// Phase 7.4c-submit-A4 — typed parallel-paths write-timestamp (acquire).
	{
		static qboolean warned;
		vk_ral_parallel_write_timestamp( VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			vk_gpu_ts_pool,
			vk_gpu_ts_inflight[ vk.cmd_index ].base + vk_gpu_ts_count,
			&warned, "gpu-ts-acquire" );
	}
	vk_gpu_ts_labels[ vk_gpu_ts_count ] = "acquire";
	vk_gpu_ts_count++;
}

static void vk_gpu_ts_write( const char *label )
{
	if ( !vk_gpu_ts_active || !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;
	if ( vk_gpu_ts_count >= VK_GPU_TS_MAX )
		return;

	qvkCmdWriteTimestamp( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		vk_gpu_ts_pool,
		vk_gpu_ts_inflight[ vk.cmd_index ].base + vk_gpu_ts_count );
	// Phase 7.4c-submit-A4 — typed parallel-paths write-timestamp (named label).
	{
		static qboolean warned;
		vk_ral_parallel_write_timestamp( VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			vk_gpu_ts_pool,
			vk_gpu_ts_inflight[ vk.cmd_index ].base + vk_gpu_ts_count,
			&warned, "gpu-ts-named-label" );
	}
	vk_gpu_ts_labels[ vk_gpu_ts_count ] = label;
	vk_gpu_ts_count++;
}

static void vk_gpu_ts_frame_end( void )
{
	uint32_t i;

	if ( !vk_gpu_ts_active || !r_gpuSpeeds || !r_gpuSpeeds->integer )
		return;

	vk_gpu_ts_inflight[ vk.cmd_index ].count   = vk_gpu_ts_count;
	vk_gpu_ts_inflight[ vk.cmd_index ].pending  = qtrue;
	for ( i = 0; i < vk_gpu_ts_count; i++ )
		vk_gpu_ts_inflight[ vk.cmd_index ].labels[ i ] = vk_gpu_ts_labels[ i ];

	vk_gpu_ts_count = 0;
}

// ---------------------------------------------------------------------------

void vk_begin_frame( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkResult res;

	if ( vk.frame_count++ ) // might happen during stereo rendering
		return;

	vk_frame_t_start = ri.Microseconds();
	vk_frame_present_done = qfalse;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	vk.cmd = &vk.tess[ vk.cmd_index ];

	{
		int t_diag = ri.Milliseconds();
		if ( vk.cmd->waitForFence ) {
			vk.cmd->waitForFence = qfalse;
#ifdef _WIN32
			// Windows: no background fence thread — wait synchronously
			res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
			if ( res != VK_SUCCESS ) {
				if ( res == VK_ERROR_DEVICE_LOST ) {
					ri.Log( SEV_WARN, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
				} else {
					ri.Terminate( TERM_UNRECOVERABLE, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
				}
			}
			VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
#else
			// Background fence thread already waited + reset the fence.
			// This call is nearly instant: with 3 buffers, the slot was freed
			// ~13ms before we need it again.
			vk_slot_wait( vk.cmd_index );
#endif
		}
		{
			int fence_ms = ri.Milliseconds() - t_diag;
			vk_diag_fence_ms += fence_ms;
			if ( r_gpuSpeeds && r_gpuSpeeds->integer >= 2 && fence_ms >= r_gpuSpeeds->integer )
				ri.Log( SEV_DEBUG, "fence spike: %dms\n", fence_ms );
		}
		vk_frame_t_after_fence = ri.Microseconds();
		if ( ++vk_diag_frames >= 200 ) {
			if ( r_vkDebugTiming && r_vkDebugTiming->integer )
				ri.Log( SEV_DEBUG, "vk timing (200f avg): fence=%dms/f  ft_fence=%dms/f  acquire=%dms/f  submit=%dms/f  present=%dms/f  draws=%d/f(msdf=%d)  pipebinds=%d/f(msdf=%d)\n",
					vk_diag_fence_ms / 200, vk_diag_ft_fence_ms / 200,
					vk_diag_acquire_ms / 200, vk_diag_submit_ms / 200, vk_diag_present_ms / 200,
					vk_diag_drawcalls / 200, vk_diag_msdf_draws / 200,
					vk_diag_pipebinds / 200, vk_diag_msdf_binds / 200 );
			vk_diag_fence_ms = vk_diag_ft_fence_ms = vk_diag_submit_ms = vk_diag_present_ms = vk_diag_acquire_ms = vk_diag_frames = 0;
			vk_diag_drawcalls = vk_diag_pipebinds = vk_diag_msdf_draws = vk_diag_msdf_binds = 0;
		}
	}

	// GPU timestamp readback: fence above guarantees this slot's GPU work is done.
	vk_gpu_ts_frame_begin();

	if ( !ri.CL_IsMinimized() && !vk.cmd->swapchain_image_acquired ) {
		int t_acquire = ri.Milliseconds();
		qboolean retry = qfalse;
_retry:
		{
			// Phase 7.4c-submit-followup-present-2 — Cluster G.1. Retired
			// qvkAcquireNextImageKHR; vk.cmd->ral_image_acquired (per-frame
			// ring adopted at sync_primitives init) is signaled by RAL on
			// success. The returned ralTexture_t is unused beyond this call —
			// downstream code reads vk.swapchain_image_index directly to
			// index vk.swapchain_images[] / vk.framebuffers.main/gamma[].
			ralTexture_t *acquiredImage = NULL;
			ralResult_t  ralRes = Ral_AcquireNextImage( vk.ral_swapchain,
				1ULL * 1000000000ULL,
				vk.cmd->ral_image_acquired,
				&vk.cmd->swapchain_image_index,
				&acquiredImage );
			(void)acquiredImage;
			if ( ralRes == ralOutOfDate || ralRes == ralSuboptimal ) {
				if ( retry == qfalse ) {
					retry = qtrue;
					// Map ral-result back to VkResult for vk_restart_swapchain's
					// existing signature; preserves the legacy log shape.
					vk_restart_swapchain( __func__, ( ralRes == ralSuboptimal ) ? VK_SUBOPTIMAL_KHR : VK_ERROR_OUT_OF_DATE_KHR );
					goto _retry;
				}
				ri.Terminate( TERM_UNRECOVERABLE, "Ral_AcquireNextImage returned ralOutOfDate twice" );
			}
			if ( ralRes != ralSuccess ) {
				ri.Terminate( TERM_UNRECOVERABLE, "Ral_AcquireNextImage returned %d", (int)ralRes );
			}
			res = VK_SUCCESS;
		}
		vk.cmd->swapchain_image_acquired = qtrue;
		vk_diag_acquire_ms += ri.Milliseconds() - t_acquire;
	}
	vk_frame_t_after_acquire = ri.Microseconds();

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk_frame_t_after_begincb = ri.Microseconds();

	// Phase 7.4c-cmd — adopt the per-frame VkCommandBuffer into a
	// ralCommandBuffer_t wrapper (ownsBuffer = qfalse). Used by every
	// Ral_Cmd*Vk parallel-paths call site this frame. Destroyed at frame
	// end after qvkEndCommandBuffer. NULL when RAL backend isn't up.
	{
		ralBackend_t *b = vk_ral_get_backend();
		if ( b != NULL ) {
			vk.cmd->ral_cmd = Ral_AcquireBegunCommandBuffer( b, RAL_QUEUE_GRAPHICS );
			vk_ral_cmd_adopt_count++;

			// Phase 7.4c-cmd: confirm-adoption-fires line. Cadence = first
			// frame (so short smokes / map loads see it immediately), then
			// every 1000 frames after that (matches the brief's guidance
			// "once per 1000 frames or once per map load"). No new cvars
			// introduced; existing log channels carry it.
			#define VK_RAL_CMD_DUMP_CADENCE  1000u
			{
				const uint32_t framesSinceLastDump = (uint32_t)vk_ral_cmd_adopt_count - vk_ral_cmd_last_dump_frame;
				const qboolean firstAdoption       = ( vk_ral_cmd_adopt_count == 1 ) ? qtrue : qfalse;
				if ( firstAdoption || framesSinceLastDump >= VK_RAL_CMD_DUMP_CADENCE ) {
					vk_ral_cmd_last_dump_frame = (uint32_t)vk_ral_cmd_adopt_count;
					ri.Log( SEV_INFO,
						"[VK->RAL] cmd buffer adopted per-frame (cadence=%u; lifetime cmd adoptions=%llu; ring adoptions=%llu)\n",
						(unsigned)VK_RAL_CMD_DUMP_CADENCE,
						(unsigned long long)vk_ral_cmd_adopt_count,
						(unsigned long long)vk_ral_descset_adopt_count );
				}
			}
			#undef VK_RAL_CMD_DUMP_CADENCE
		}
	}

	vk_gpu_ts_pool_reset();

	// Ensure visibility of geometry buffers writes.
	//record_buffer_memory_barrier( vk.cmd->command_buffer, vk.cmd->vertex_buffer, vk.geometry_buffer_size, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	// add explicit layout transition dependency
	if ( vk.fboActive ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( vk.cmd->command_buffer, vk.swapchain_images[ vk.swapchain_image_index ], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );
	}
#endif

	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}

	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	backEnd.screenMapDone = qfalse;
	vk.depthFade.copied = qfalse;

	// Particle compute pass — runs OUTSIDE any render pass, before the
	// main / screenmap pass begins this frame. vkCmdDispatch is
	// spec-forbidden inside a render pass instance; this is the only
	// safe location in the frame to record the dispatch + the
	// compute→vertex-shader buffer barrier. RB_RunParticleCompute
	// updates only the compute region of its per-frame UBO; the render
	// region is filled later by RB_DrawParticles when
	// backEnd.viewParms is valid for this frame.
	RB_RunParticleCompute();

#if FEAT_SHADOW_MAPPING
	// Sun shadow cascades — a per-frame PRE-PASS, recorded here (like the
	// particle compute above) while no render pass is open and before the
	// main / screenmap pass begins, so the PMLIGHT lit pass can sample a
	// complete shadow array. It fits the cascades to backEnd.viewParms, which
	// at this point still holds the PREVIOUS frame's main view (this frame's
	// scene hasn't been processed yet — same as RB_RunParticleCompute's render
	// region note): a one-frame fit lag, imperceptible given the cascades
	// over-cover (bounding-sphere fit + the +1024u near-plane pullback). This
	// replaces the 6.5.4a-d1 mid-frame call from RB_LightingPass, which had to
	// vk_end_render_pass()/vk_begin_main_render_pass() — and the re-begin
	// re-clears the main FBO (the d1 black-screen-on-dlight regression).
	if ( vk.shadowMap.active )
		vk_render_shadow_map();
#endif

	if ( vk_find_screenmap_drawsurfs() ) {
		if ( vk.frame_count <= 3 )
			ri.Log( SEV_TRACE, "[FBO_TRACE] Frame %d: screenmap pass selected\n", vk.frame_count );
		vk_begin_screenmap_render_pass();
	} else {
		if ( vk.frame_count <= 3 )
			ri.Log( SEV_TRACE, "[FBO_TRACE] Frame %d: main pass (no screenmap)\n", vk.frame_count );
		vk_begin_main_render_pass();
	}

	// dynamic vertex buffer layout
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;

	memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;
	//vk.cmd->descriptor_set.end = 0;

	memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// other stats
	vk.stats.push_size = 0;
	vk_frame_t_rec_start = ri.Microseconds();
}


static void vk_resize_geometry_buffer( void )
{
	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( int i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

	ri.Log( SEV_DEBUG, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}


void vk_end_frame( void )
{
	// Phase 7.4c-submit-BC-C-final — per-frame submit migrated to Ral_Submit.
	// Legacy VkSubmitInfo / waits / signals / wait_dst_stage_mask decls retired
	// (Ral_Submit's ralSubmitInfo_t carries semaphore arrays in RAL-typed
	// pointers; the backend internally builds VkSubmitInfo2 with stageMask =
	// VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT — coarser than the legacy COLOR_
	// ATTACHMENT_OUTPUT / FRAGMENT_SHADER masks but semantically valid given
	// the renderer's actual barrier coverage, which inserts explicit pipeline
	// barriers around every read of the swapchain image / staging-upload data).
	ralSemaphore_t     *ralWaits  [2];
	ralSemaphore_t     *ralSignals[2];
	ralSubmitInfo_t     ralSubmit;
	ralCommandBuffer_t *frameRalCb;
	ralCommandBuffer_t *ralCbs[1];

	if ( vk.frame_count == 0 )
		return;

	vk.frame_count = 0;
	vk_frame_t_rec_end = ri.Microseconds();

	if ( vk.geometry_buffer_size_new )
	{
		vk_resize_geometry_buffer();
		// issue: one frame may be lost during video recording
		// solution: re-record all commands again? (might be complicated though)
		return;
	}

	if ( vk.fboActive )
	{
		if ( vk.frame_count == 0 ) {
			ri.Log( SEV_TRACE, "[FBO_TRACE] vk_end_frame: fboActive=1 bloom=%d r_smaa=%d\n", r_bloom->integer, r_smaa->integer );
			ri.Log( SEV_TRACE, "[FBO_TRACE]   color_descriptor=%p gamma_pipeline=%p\n", (void*)vk.color_descriptor, (void*)vk.gamma_pipeline );
			ri.Log( SEV_TRACE, "[FBO_TRACE]   renderPassIndex=%d doneSurfaces=%d doneUIPass=%d\n", vk.renderPassIndex, backEnd.doneSurfaces, backEnd.doneUIPass );
		}
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		// Block 8 (Delta 2): the 3D→2D transition orchestrator (tr_backend.c)
		// owns bloom / tonemap / SMAA / UI-pass sequencing now, gated by
		// backEnd.doneUIPass. We only reach here with !doneUIPass when the
		// frame had NO 2D draws at all:
		//   - gameplay-with-no-2D (e.g. cg_draw2D 0): the 3D scene is in
		//     render_pass.main (img 264). Tonemap it into img 265 so gamma
		//     can read it. (SMAA is not run here — it is dispatched only
		//     from the transition gameplay branch; a fully-2D-less frame is
		//     exotic and gets no AA, which is acceptable.)
		//   - nothing-drawn frame: render_pass.main holds only its CLEAR;
		//     tonemap(black) → img 265 ≈ black, gamma reads black.
		// When doneUIPass is set, render_pass.ui (gameplay) or
		// render_pass.ui_clear (pure-2D) is open on img 265 with the
		// composited frame — leave it for the pass-closing chain below.
		if ( !backEnd.doneUIPass )
		{
			if ( r_bloom->integer && backEnd.doneSurfaces )
				vk_bloom();
			vk_tonemap(); // ends main/post_bloom + does tonemap + ends render_pass.tonemap → no pass open
		}

		// Render-pass state here:
		//   doneUIPass  → render_pass.ui / ui_clear open on img 265
		//   !doneUIPass → no render pass open (vk_tonemap left a clean state)

		if ( backEnd.screenshotMask && vk.capture.image )
		{
			if ( backEnd.doneUIPass )
				vk_end_render_pass(); // end the ui / ui_clear pass

			// render to capture FBO — sample img 265 (post-tonemap +,
			// gameplay path, SMAA + HUD) so captured screenshots represent
			// the composited output.
			vk_begin_render_pass_clear1( vk.render_pass.capture, vk.framebuffers.capture, gls.captureWidth, gls.captureHeight );
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.capture_pipeline );
			// Phase 7.4c-cmd — parallel-paths bind-pipeline (capture).
			Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.capture_pipeline ));
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.tonemapped_descriptor, 0, NULL );
			// Phase 7.4c-bindgroup — parallel-paths bind-side record (capture pass).
			vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.tonemapped_descriptor, 0, NULL );

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			// Phase 7.4c-cmd — parallel-paths draw (capture).
			Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );
		}

		if ( !ri.CL_IsMinimized() )
		{
			// End the capture pass (if it ran) or the ui / ui_clear pass
			// (if doneUIPass and no capture). If !doneUIPass and no capture
			// there is no pass open — skip.
			if ( ( backEnd.screenshotMask && vk.capture.image ) || backEnd.doneUIPass )
				vk_end_render_pass();
			vk.renderWidth = gls.windowWidth;
			vk.renderHeight = gls.windowHeight;

			vk.renderScaleX = 1.0;
			vk.renderScaleY = 1.0;

#ifdef __APPLE__
			// MoltenVK/TBDR: flush tile cache so gamma pass sees writes
			// of the upstream tonemap pass. Phase 6B3'-c1: target image
			// changed from vk.color_image to vk.tonemapped_image since
			// the gamma pass now samples the tonemap output.
			// Gate under r_vkApplePinkBarrier to measure FPS impact; set
			// to 0 to test, 1 to restore pink-glitch fix.
			if ( vk.fboActive && r_vkApplePinkBarrier->integer ) {
				VkImageMemoryBarrier b;
				memset( &b, 0, sizeof( b ) );
				b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.image = vk.tonemapped_image;
				b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				b.subresourceRange.levelCount = 1;
				b.subresourceRange.layerCount = 1;
				qvkCmdPipelineBarrier( vk.cmd->command_buffer,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, NULL, 0, NULL, 1, &b );
				// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier
				// (Apple TBDR pink fix; tonemapped_image is adopted at boot).
				{
					static qboolean warned;
					vk_ral_parallel_pipeline_barrier_image(
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						1, &b, &warned, "apple-tbdr-pink-tonemapped" );
				}
			}
#endif

			vk_begin_render_pass_clear1( vk.render_pass.gamma, vk.framebuffers.gamma[ vk.cmd->swapchain_image_index ], vk.renderWidth, vk.renderHeight );

			// Phase 6B3'-c1: gamma is now a thin display-encode pass — no
			// variant selection. Reads vk.tonemapped_descriptor (LDR linear),
			// applies r_gamma encoding + framebuffer-bit-depth dither, writes
			// to the swapchain image. All scene-radiance variants moved to
			// vk_tonemap upstream.
			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.gamma_pipeline );
			// Phase 7.4c-cmd — parallel-paths bind-pipeline (gamma).
			Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.gamma_pipeline ));
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.tonemapped_descriptor, 0, NULL );
			// Phase 7.4c-bindgroup — parallel-paths bind-side record (gamma display-encode pass).
			vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.tonemapped_descriptor, 0, NULL );

			qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
			// Phase 7.4c-cmd — parallel-paths draw (gamma).
			Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );
		}
	}

	// End whatever render pass is active. Skip when no render pass is open:
	//   (a) FBO + not minimized + swapchain acquire failed — the gamma pass
	//       was never started, and the prior pass was already ended in the
	//       `if ( !minimized )` block above.
	//   (b) FBO + minimized + !doneUIPass + no capture pass — vk_tonemap()
	//       above left a clean state, and minimized skips the gamma pass.
	if ( !( vk.fboActive && !vk.cmd->swapchain_image_acquired && !ri.CL_IsMinimized() ) &&
	     !( vk.fboActive && ri.CL_IsMinimized() && !backEnd.doneUIPass && !( backEnd.screenshotMask && vk.capture.image ) ) ) {
		vk_end_render_pass();
	}

	vk_gpu_ts_write( "present_prep" ); // must be before EndCommandBuffer; render passes are all closed above
	vk_gpu_ts_frame_end();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	// Phase 7.4c-cmd — release the parallel-paths RAL command buffer + the
	// per-frame ring-adoption descriptor wrappers. Ral_AcquireBegunCommandBuffer
	// (despite its name) allocated a SEPARATE VkCommandBuffer from RAL's
	// graphics pool and began it; we end + free it here. The legacy
	// vk.cmd->command_buffer was already ended above and is what the
	// renderer's existing submit_info uses — RAL parallel buffer is recorded
	// every frame but never submitted (7.4c-submit will move submission to
	// it and retire legacy).
	{
		uint32_t i;
		for ( i = 0; i < ARRAY_LEN( vk.cmd->descriptor_set.current_ral ); i++ ) {
			if ( vk.cmd->descriptor_set.current_ral[i] ) {
				Ral_DestroyBindGroup( vk.cmd->descriptor_set.current_ral[i] );
				vk.cmd->descriptor_set.current_ral[i] = NULL;
			}
		}
		if ( vk.cmd->ral_cmd ) {
			Ral_EndCommandBuffer    ( vk.cmd->ral_cmd );
			Ral_DestroyCommandBuffer( vk.cmd->ral_cmd );
			vk.cmd->ral_cmd = NULL;
		}
	}

	// Phase 7.4c-submit-BC-C-final — per-frame submit migrated from
	// qvkQueueSubmit to Ral_Submit. The legacy vk.cmd->command_buffer lifecycle
	// (alloc / reset / begin / record / end) stays intact — it's wrapped here
	// via Ral_WrapCommandBuffer (ownsBuffer=qfalse) just long enough to feed
	// ralSubmitInfo_t.commandBuffers[]. All wait/signal semaphores + the
	// signal fence reference adopted RAL siblings populated at
	// vk_create_sync_primitives + vk_create_swapchain (BC-followup-staging /
	// BC-followup-present-1 / BC-C-final's ral_rendering_finished_fence).
	frameRalCb = Ral_WrapCommandBuffer( vk_ral_get_backend(),
		(void *)vk.cmd->command_buffer, RAL_QUEUE_GRAPHICS );
	ralCbs[0] = frameRalCb;

	memset( &ralSubmit, 0, sizeof( ralSubmit ) );
	ralSubmit.commandBuffers     = ralCbs;
	ralSubmit.numCommandBuffers  = 1;
	ralSubmit.signalFence        = vk.cmd->ral_rendering_finished_fence;
	if ( !ri.CL_IsMinimized() && vk.cmd->swapchain_image_acquired ) {
#ifdef USE_UPLOAD_QUEUE
		if ( vk.image_uploaded != VK_NULL_HANDLE ) {
			ralWaits[0] = vk.cmd->ral_image_acquired;
			ralWaits[1] = vk.ral_image_uploaded;
			ralSubmit.waitSemaphores    = ralWaits;
			ralSubmit.numWaitSemaphores = 2;
			ralSignals[0] = vk.ral_swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			ralSignals[1] = vk.cmd->ral_rendering_finished2;
			ralSubmit.signalSemaphores    = ralSignals;
			ralSubmit.numSignalSemaphores = 2;

			vk.rendering_finished     = vk.cmd->rendering_finished2;
			vk.ral_rendering_finished = vk.cmd->ral_rendering_finished2;
			vk.image_uploaded     = VK_NULL_HANDLE;
			vk.ral_image_uploaded = NULL;
		} else {
			ralWaits[0] = vk.cmd->ral_image_acquired;
			ralSubmit.waitSemaphores    = ralWaits;
			ralSubmit.numWaitSemaphores = 1;
			ralSignals[0] = vk.ral_swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
			ralSubmit.signalSemaphores    = ralSignals;
			ralSubmit.numSignalSemaphores = 1;
		}
#else
		ralWaits[0] = vk.cmd->ral_image_acquired;
		ralSubmit.waitSemaphores    = ralWaits;
		ralSubmit.numWaitSemaphores = 1;
		ralSignals[0] = vk.ral_swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
		ralSubmit.signalSemaphores    = ralSignals;
		ralSubmit.numSignalSemaphores = 1;
#endif
	}
	/* else: no waits/signals — minimised or acquire failed; the submit still
	   carries the signalFence so the per-frame fence-wait at vk_begin_frame
	   sees a signaled fence. */

	{
		int t_submit = ri.Milliseconds();
		vk_frame_t_submit_start = ri.Microseconds();
		Ral_Submit( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS, &ralSubmit );
		vk_diag_submit_ms += ri.Milliseconds() - t_submit;
		vk_frame_t_after_submit = ri.Microseconds();
	}

	// Phase 7.4c-submit-BC-C-final — wrapper retire (ownsBuffer=qfalse → no
	// underlying-buffer destroy; the legacy vk.cmd->command_buffer stays in
	// its existing alloc/reset/begin/end lifecycle for the next frame).
	Ral_DestroyCommandBuffer( frameRalCb );

	// Phase 7.4c-pipeline-followup-5 PART 0 — first-frame submit marker.
	// Stays permanently as durable observability: proves a frame actually
	// rendered (i.e. we didn't abort during map-load shutdown before reaching
	// SCR_UpdateScreen → vk_end_frame). Printed once per process lifetime; the
	// followup-5 smoke gate (item 4) requires this line to appear.
	//
	// Phase 7.4c-pipeline-followup-5 PART 2.5 — Lesson 18 watchpoint: the
	// "backend=" tail reads vk_ral_get_backend() directly so the marker
	// reflects ACTUAL runtime state, not just cvar intent. P/T/B keep
	// reading cvar state (= user's flag intent at boot); backend=live/down
	// reports whether the RAL backend pointer is actually non-NULL at
	// frame-1 time, which is what would silently false-positive if a
	// REF_KEEP_CONTEXT teardown nulled the backend before the first frame.
	//
	// Phase 7.4c-pipeline-followup-5 PART 3+4 — END timing marker fires
	// alongside the frame-1 marker. delta_ms covers cache load → all
	// init-time pipeline creation → first frame submit, bracketed by the
	// START marker at the Ral_LoadPipelineCache call site in vk_initialize.
	// Both markers single-fire-per-process, stay permanently as durable
	// observability.
	{
		static qboolean s_logged_frame1 = qfalse;
		if ( !s_logged_frame1 ) {
			ralBackend_t *backend = vk_ral_get_backend();
			s_logged_frame1 = qtrue;
			ri.Log( SEV_INFO, "[7.4c-fu5] frame 1 rendered (RAL paths active: P=%d T=%d B=%d, backend=%s)\n",
				( r_useRALPipelines && r_useRALPipelines->integer ) ? 1 : 0,
				( r_useRALTextures  && r_useRALTextures->integer  ) ? 1 : 0,
				( r_useRALBuffers   && r_useRALBuffers->integer   ) ? 1 : 0,
				( backend != NULL ) ? "live" : "down" );
			if ( vk_pipeline_cache_load_start_ms != 0 ) {
				int delta_ms = ri.Milliseconds() - vk_pipeline_cache_load_start_ms;
				uint32_t slotCount = backend ? Ral_GetPipelineLayoutCacheSlotCount( backend ) : 0u;
				ri.Log( SEV_INFO,
					"[7.4c-fu5] pipeline creation done: %d ms (cold=%d, layout slots used: %u, cache loaded: %llu bytes)\n",
					delta_ms,
					vk_pipeline_cache_was_cold ? 1 : 0,
					(unsigned)slotCount,
					(unsigned long long)vk_pipeline_cache_size_at_load );
			}
		}
	}

	// Hand this slot's fence to the background thread: it will vkWaitForFences +
	// vkResetFences off the main thread, then signal vk_slot_ready[cmd_index].
	vk_fence_submit( vk.cmd_index, vk.cmd->rendering_finished_fence );
	vk.cmd->waitForFence = qtrue;

	// presentation may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	vk.renderPassIndex = RENDER_PASS_MAIN;
}


void vk_present_frame( void )
{
	VkPresentInfoKHR present_info;
	VkResult res;

	if ( ri.CL_IsMinimized() || !vk.cmd->swapchain_image_acquired ) {
		return;
	}

	if ( !vk.cmd->waitForFence ) {
		// nothing has been submitted this frame due to geometry buffer overflow?
		return;
	}

	// Phase 7.4c-submit-followup-present-2 — Cluster G.2. Retired
	// qvkQueuePresentKHR; built typed ralPresentInfo_t over the per-
	// swapchain-image adopted semaphore (ral_swapchain_rendering_finished[idx])
	// and pass through Ral_Present. Result-code mapping preserves the legacy
	// error-handling shape: ralOutOfDate/ralSuboptimal → vk_restart_swapchain;
	// ralErrorDeviceLost → debug log + break; other errors → terminate.
	vk.cmd->swapchain_image_acquired = qfalse;

	{
		ralPresentInfo_t pi;
		ralResult_t      ralRes;
		int              t_present;
		memset( &pi, 0, sizeof( pi ) );
		pi.swapchains         = &vk.ral_swapchain;
		pi.numSwapchains      = 1;
		pi.imageIndices       = &vk.cmd->swapchain_image_index;
		pi.waitSemaphores     = &vk.ral_swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
		pi.numWaitSemaphores  = 1;

		t_present = ri.Milliseconds();
		vk_frame_t_present_start = ri.Microseconds();
		ralRes = Ral_Present( vk_ral_get_backend(), &pi );
		vk_diag_present_ms += ri.Milliseconds() - t_present;
		vk_frame_t_after_present = ri.Microseconds();
		vk_frame_present_done = qtrue;

		if ( ralRes == ralOutOfDate || ralRes == ralSuboptimal ) {
			// swapchain re-creation needed
			vk_restart_swapchain( __func__, ( ralRes == ralSuboptimal ) ? VK_SUBOPTIMAL_KHR : VK_ERROR_OUT_OF_DATE_KHR );
			return;
		}
		if ( ralRes == ralErrorDeviceLost ) {
			ri.Log( SEV_DEBUG, "Ral_Present: device lost\n" );
		} else if ( ralRes != ralSuccess ) {
			ri.Terminate( TERM_UNRECOVERABLE, "Ral_Present returned %d", (int)ralRes );
		}
	}

	// pickup next command buffer for rendering
	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];

	if ( vk_frame_present_done && r_frameSpikeUs && r_frameSpikeUs->integer > 0 ) {
		int64_t total = vk_frame_t_after_present - vk_frame_t_start;
		if ( total >= (int64_t)r_frameSpikeUs->integer ) {
			ri.Log( SEV_DEBUG,
				"spike total=%lldus  fence=%lld  acquire=%lld  cb_setup=%lld  "
				"begin_passes=%lld  record=%lld  post_passes=%lld  submit=%lld  present=%lld\n",
				(long long)total,
				(long long)( vk_frame_t_after_fence    - vk_frame_t_start ),
				(long long)( vk_frame_t_after_acquire  - vk_frame_t_after_fence ),
				(long long)( vk_frame_t_after_begincb  - vk_frame_t_after_acquire ),
				(long long)( vk_frame_t_rec_start      - vk_frame_t_after_begincb ),
				(long long)( vk_frame_t_rec_end        - vk_frame_t_rec_start ),
				(long long)( vk_frame_t_submit_start   - vk_frame_t_rec_end ),
				(long long)( vk_frame_t_after_submit   - vk_frame_t_submit_start ),
				(long long)( vk_frame_t_after_present  - vk_frame_t_present_start ) );
		}
	}
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


void vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.fboActive ) {
		if ( vk.capture.image ) {
			// dedicated capture buffer
			srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			srcImage = vk.capture.image;
		} else {
			srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			srcImage = vk.color_image;
		}
	} else {
		srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		srcImage = vk.swapchain_images[ vk.cmd->swapchain_image_index ];
	}

	memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) );

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0 ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Terminate( TERM_UNRECOVERABLE, "%s(): failed to find matching memory type for image capture", __func__ );
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	{
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping screenshot blit/copy\n", __func__ );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );

			if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
				record_image_layout_transition( command_buffer, srcImage,
					VK_IMAGE_ASPECT_COLOR_BIT,
					srcImageLayout,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					0, 0);
			}

			record_image_layout_transition( command_buffer, dstImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

			if ( vk.blitEnabled ) {
				VkImageBlit region;

				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.mipLevel = 0;
				region.srcSubresource.baseArrayLayer = 0;
				region.srcSubresource.layerCount = 1;
				region.srcOffsets[0].x = 0;
				region.srcOffsets[0].y = 0;
				region.srcOffsets[0].z = 0;
				region.srcOffsets[1].x = width;
				region.srcOffsets[1].y = height;
				region.srcOffsets[1].z = 1;
				region.dstSubresource = region.srcSubresource;
				region.dstOffsets[0] = region.srcOffsets[0];
				region.dstOffsets[1] = region.srcOffsets[1];

				qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

			} else {
				VkImageCopy region;

				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.srcSubresource.mipLevel = 0;
				region.srcSubresource.baseArrayLayer = 0;
				region.srcSubresource.layerCount = 1;
				region.srcOffset.x = 0;
				region.srcOffset.y = 0;
				region.srcOffset.z = 0;
				region.dstSubresource = region.srcSubresource;
				region.dstOffset = region.srcOffset;
				region.extent.width = width;
				region.extent.height = height;
				region.extent.depth = 1;

				qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
			}

			Ral_SubmitAndDispose( rcmd );
		}
	}

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
		if ( rcmd == NULL ) {
			ri.Log( SEV_ERROR, "[VK->RAL] %s: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping restore-layout transition\n", __func__ );
		} else {
			command_buffer = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );

			record_image_layout_transition( command_buffer, srcImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				srcImageLayout, 0, 0 );

			Ral_SubmitAndDispose( rcmd );
		}
	}
}


#if FEAT_SHADOW_MAPPING

// A BSP surface is an opaque shadow caster iff: not sky, no SURF_SKY/NODRAW,
// sort == SS_OPAQUE, has a stage[0], and that stage isn't ATEST / blended.
// (ATEST cutout casters arrive in 6.5.4d3.)
#define SHADOW_CASTER_OK(sh) ( (sh) && !(sh)->isSky && !((sh)->surfaceFlags & (SURF_SKY|SURF_NODRAW)) \
	&& (sh)->sort == SS_OPAQUE && (sh)->stages[0] \
	&& !((sh)->stages[0]->stateBits & (GLS_ATEST_BITS | GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) )

// Phase 6.5.4d2-followup: prev-frame deformed-mesh shadow casters. The main pass
// deforms each MD3 / MDL-as-MD3 / CPU-skinned-IQM surface into tess.xyz; vk_shadow_
// capture_mesh() snapshots that (model-space verts + 0-based indices + the entity's
// [axis|origin]) into these CPU arrays; the NEXT frame's vk_render_shadow_map()
// memcpy's them into vk.cmd->shadowSnapBuf (host-coherent, per-frame slot) and draws
// them with the same depthPipeline / 128-byte push the bmodel casters use. 1-frame
// lag, composing with the existing cascade-fit lag. (GPU-skinned IQM is d2-followup-2.)
typedef struct {
	uint32_t firstIndex;     // first index of this slice within shadowMeshSnapIndices[]
	uint32_t indexCount;     // 3 * triangles
	int32_t  vertexOffset;   // first vert of this slice within shadowMeshSnapVerts[] (added to each index at draw)
	float    modelMatrix[16];// column-major model->world ([axis|origin]; bottom row 0,0,0,1) captured this slice
} shadowMeshSlice_t;

static vec4_t            *shadowMeshSnapVerts;     // ri.Malloc'd, lazy-grown (double-on-overflow)
static glIndex_t         *shadowMeshSnapIndices;
static shadowMeshSlice_t *shadowMeshSnapSlices;
static int  shadowMeshSnapVertCount,  shadowMeshSnapVertCap;
static int  shadowMeshSnapIndexCount, shadowMeshSnapIndexCap;
static int  shadowMeshSnapSliceCount, shadowMeshSnapSliceCap;

// Pass 1: accumulate this surface range's opaque caster vertex / index counts
// into *addVerts / *addIdx. SF_FACE / SF_TRIANGLES / SF_GRID (control net,
// 2 tris/cell — coarse but conservative). Returns the caster-surface count.
static int vk_shadow_caster_count( const msurface_t *surfs, int n, uint32_t *addVerts, uint32_t *addIdx ) {
	int i, found = 0;
	for ( i = 0; i < n; i++ ) {
		const msurface_t *surf = &surfs[i];
		if ( !surf->data || !SHADOW_CASTER_OK( surf->shader ) ) continue;
		switch ( *surf->data ) {
			case SF_FACE:      { const srfSurfaceFace_t *f = (const srfSurfaceFace_t *)surf->data; *addVerts += f->numPoints; *addIdx += f->numIndices; found++; break; }
			case SF_TRIANGLES: { const srfTriangles_t  *t = (const srfTriangles_t  *)surf->data; *addVerts += t->numVerts;  *addIdx += t->numIndexes; found++; break; }
			case SF_GRID:      { const srfGridMesh_t   *g = (const srfGridMesh_t   *)surf->data; if ( g->width >= 2 && g->height >= 2 ) { *addVerts += g->width * g->height; *addIdx += (g->width-1) * (g->height-1) * 6; found++; } break; }
			default: break;
		}
	}
	return found;
}

// Pass 2: append this surface range's opaque caster geometry to verts[] / idx[]
// at the running offsets *vCap / *iCap (advanced in place). Verts are xyz with
// w=1; indices are absolute into verts[] (so a single bind + vertexOffset == 0
// can draw any sub-range). Mirrors vk_shadow_caster_count's predicate, so the
// counts pre-computed there match exactly.
static void vk_shadow_caster_fill( const msurface_t *surfs, int n, vec4_t *verts, uint32_t *vCap, uint32_t *idx, uint32_t *iCap ) {
	int i, v;
	for ( i = 0; i < n; i++ ) {
		const msurface_t *surf = &surfs[i];
		uint32_t base = *vCap;
		if ( !surf->data || !SHADOW_CASTER_OK( surf->shader ) ) continue;
		switch ( *surf->data ) {
			case SF_FACE: {
				const srfSurfaceFace_t *f = (const srfSurfaceFace_t *)surf->data;
				const int *tri = (const int *)( (const byte *)f + f->ofsIndices );
				for ( v = 0; v < f->numPoints; v++ ) { verts[*vCap][0] = f->points[v][0]; verts[*vCap][1] = f->points[v][1]; verts[*vCap][2] = f->points[v][2]; verts[*vCap][3] = 1.0f; (*vCap)++; }
				for ( v = 0; v < f->numIndices; v++ ) idx[(*iCap)++] = base + (uint32_t)tri[v];
				break;
			}
			case SF_TRIANGLES: {
				const srfTriangles_t *t = (const srfTriangles_t *)surf->data;
				for ( v = 0; v < t->numVerts; v++ ) { verts[*vCap][0] = t->verts[v].xyz[0]; verts[*vCap][1] = t->verts[v].xyz[1]; verts[*vCap][2] = t->verts[v].xyz[2]; verts[*vCap][3] = 1.0f; (*vCap)++; }
				for ( v = 0; v < t->numIndexes; v++ ) idx[(*iCap)++] = base + (uint32_t)t->indexes[v];
				break;
			}
			case SF_GRID: {
				const srfGridMesh_t *g = (const srfGridMesh_t *)surf->data;
				int r, c;
				if ( g->width < 2 || g->height < 2 ) break;
				for ( v = 0; v < g->width * g->height; v++ ) { verts[*vCap][0] = g->verts[v].xyz[0]; verts[*vCap][1] = g->verts[v].xyz[1]; verts[*vCap][2] = g->verts[v].xyz[2]; verts[*vCap][3] = 1.0f; (*vCap)++; }
				for ( r = 0; r < g->height-1; r++ ) for ( c = 0; c < g->width-1; c++ ) {
					uint32_t i00 = base + (uint32_t)( r*g->width + c );
					uint32_t i01 = i00 + 1u;
					uint32_t i10 = i00 + (uint32_t)g->width;
					uint32_t i11 = i10 + 1u;
					idx[(*iCap)++] = i00; idx[(*iCap)++] = i10; idx[(*iCap)++] = i01;
					idx[(*iCap)++] = i01; idx[(*iCap)++] = i10; idx[(*iCap)++] = i11;
				}
				break;
			}
			default: break;
		}
	}
}


/*
===================
vk_build_bmodel_casters

Phase 6.5.4d2: one shared device-local buffer ([vec4 positions][uint32 indices],
absolute indices into the vertex region) holding every inline brush model's
opaque caster geometry concatenated, plus a per-bmodel slice table
(vk.shadowMap.bmodelRanges[k] matches tr.world->bmodels[k]; slot 0 = worldspawn
is unused — those casters live in vk.shadowMap.casterBuf). Built lazily by
vk_build_shadow_caster on the same map-load gate (bmodels are static for a map's
lifetime). vk_render_shadow_map draws each visible MOD_BRUSH entity into every
cascade with that entity's [axis|origin] model matrix, so moved/rotated doors,
platforms etc. cast their shadow at the current position.
===================
*/
static void vk_build_bmodel_casters( void ) {
	const world_t *w = tr.world;
	int k, numB, withGeom = 0;
	uint32_t numVerts = 0, numIdx = 0;
	uint32_t vCap = 0, iCap = 0;
	vec4_t *verts = NULL;
	uint32_t *idx = NULL;
	VkDeviceSize vBytes, iBytes, bufSize, uploadDone;
	VkBufferCreateInfo bdesc;
	VkMemoryRequirements mreq;
	VkMemoryAllocateInfo binfo;
	VkCommandBuffer cb;
	VkBufferCopy region;

	if ( !w || w->bmodels == NULL || w->numBModels < 2 )
		return; // no inline submodels beyond worldspawn

	numB = w->numBModels;

	// pass 1 — count (bmodels 1..numB-1; bmodel 0 = worldspawn → vk.shadowMap.casterBuf)
	for ( k = 1; k < numB; k++ )
		vk_shadow_caster_count( w->bmodels[k].firstSurface, w->bmodels[k].numSurfaces, &numVerts, &numIdx );

	// allocate the slice table unconditionally (cheap; lets vk_render_shadow_map
	// index it by bmodel index without bounds-vs-NULL gymnastics) — zeroed, so
	// bmodels with no opaque casters have indexCount == 0 and get skipped.
	vk.shadowMap.bmodelRanges    = ri.Malloc( (size_t)numB * sizeof( vkBmodelCasterRange_t ) );
	memset( vk.shadowMap.bmodelRanges, 0, (size_t)numB * sizeof( vkBmodelCasterRange_t ) );
	vk.shadowMap.numBmodelRanges = numB;

	if ( numVerts == 0 || numIdx == 0 )
		return; // no opaque bmodel casters at all
	if ( numVerts > 4u*1024u*1024u || numIdx > 16u*1024u*1024u ) {
		ri.Log( SEV_WARN, "[VK] bmodel shadow caster geometry too large (%u verts / %u idx) — skipping\n", numVerts, numIdx );
		return;
	}

	verts = ri.Hunk_AllocateTempMemory( numVerts * sizeof( vec4_t ) );
	idx   = ri.Hunk_AllocateTempMemory( numIdx * sizeof( uint32_t ) );

	// pass 2 — fill, recording each bmodel's index slice into the shared buffer
	for ( k = 1; k < numB; k++ ) {
		vk.shadowMap.bmodelRanges[k].firstIndex = iCap;
		vk_shadow_caster_fill( w->bmodels[k].firstSurface, w->bmodels[k].numSurfaces, verts, &vCap, idx, &iCap );
		vk.shadowMap.bmodelRanges[k].indexCount = iCap - vk.shadowMap.bmodelRanges[k].firstIndex;
		if ( vk.shadowMap.bmodelRanges[k].indexCount ) withGeom++;
	}

	vBytes  = (VkDeviceSize)numVerts * sizeof( vec4_t );
	iBytes  = (VkDeviceSize)numIdx   * sizeof( uint32_t );
	bufSize = vBytes + iBytes;
	vk.shadowMap.casterBmodelVtxBytes = (uint32_t)vBytes;

	memset( &bdesc, 0, sizeof( bdesc ) );
	bdesc.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bdesc.size        = bufSize;
	bdesc.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	bdesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bdesc, NULL, &vk.shadowMap.casterBmodelBuf ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.shadowMap.casterBmodelBuf, &mreq );
	memset( &binfo, 0, sizeof( binfo ) );
	binfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	binfo.allocationSize  = mreq.size;
	binfo.memoryTypeIndex = find_memory_type( mreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &binfo, NULL, &vk.shadowMap.casterBmodelMem ) );
	qvkBindBufferMemory( vk.device, vk.shadowMap.casterBmodelBuf, vk.shadowMap.casterBmodelMem, 0 );
	vk_ral_register_buffer( vk.shadowMap.casterBmodelBuf, bufSize,
	                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vk.shadowMap.casterBmodelBuf" );

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	uploadDone = 0; // vertices
	while ( uploadDone < vBytes ) {
		VkDeviceSize chunk = vk.staging_buffer.size;
		if ( uploadDone + chunk > vBytes ) chunk = vBytes - uploadDone;
		memcpy( vk.staging_buffer.ptr, (const byte *)verts + uploadDone, chunk );
		{
			ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
			if ( rcmd == NULL ) {
				ri.Log( SEV_ERROR, "[VK->RAL] bmodel shadow caster verts: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping vertex chunk\n" );
			} else {
				cb = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
				region.srcOffset = 0; region.dstOffset = uploadDone; region.size = chunk;
				qvkCmdCopyBuffer( cb, vk.staging_buffer.handle, vk.shadowMap.casterBmodelBuf, 1, &region );
				Ral_SubmitAndDispose( rcmd );
			}
		}
		uploadDone += chunk;
	}
	uploadDone = 0; // indices
	while ( uploadDone < iBytes ) {
		VkDeviceSize chunk = vk.staging_buffer.size;
		if ( uploadDone + chunk > iBytes ) chunk = iBytes - uploadDone;
		memcpy( vk.staging_buffer.ptr, (const byte *)idx + uploadDone, chunk );
		{
			ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
			if ( rcmd == NULL ) {
				ri.Log( SEV_ERROR, "[VK->RAL] bmodel shadow caster idx: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping index chunk\n" );
			} else {
				cb = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
				region.srcOffset = 0; region.dstOffset = vBytes + uploadDone; region.size = chunk;
				qvkCmdCopyBuffer( cb, vk.staging_buffer.handle, vk.shadowMap.casterBmodelBuf, 1, &region );
				Ral_SubmitAndDispose( rcmd );
			}
		}
		uploadDone += chunk;
	}

	ri.Hunk_FreeTempMemory( idx );
	ri.Hunk_FreeTempMemory( verts );

	SET_OBJECT_NAME( vk.shadowMap.casterBmodelBuf, "shadow caster geometry (inline brush models)", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	ri.Log( SEV_INFO, "[VK] shadow caster: %u verts / %u tris from %d inline brush models\n", numVerts, numIdx/3, withGeom );
}


// Manual grow for the shadowMeshSnap* CPU arrays (refimport has Malloc/Free, no
// Realloc): doubles capacity until it fits `needed` elements, copies the old
// contents, frees the old buffer. Returns the (possibly new) buffer; *cap updated.
static void *vk_shadow_snap_grow( void *arr, int *cap, int needed, size_t elemSize ) {
	int   nc;
	void *nb;
	if ( needed <= *cap )
		return arr;
	nc = *cap ? *cap : 4096;
	while ( nc < needed )
		nc *= 2;
	nb = ri.Malloc( (size_t)nc * elemSize );
	if ( arr ) {
		if ( *cap > 0 )
			memcpy( nb, arr, (size_t)*cap * elemSize );
		ri.Free( arr );
	}
	*cap = nc;
	return nb;
}


/*
===================
vk_shadow_capture_mesh

Phase 6.5.4d2-followup. Hooked into RB_SurfaceMesh / RB_IQMSurfaceAnim (CPU path)
right after the main pass deforms a mesh surface into tess.xyz[firstVert..numVerts]
/ tess.indexes[firstIdx..numIdx]. Snapshots that surface's model-space verts
(xyz, w=1), its indices rebased to 0, and the current entity's model->world
transform ([backEnd.or.axis | backEnd.or.origin]) into the shadowMeshSnap* CPU
arrays. The NEXT frame's vk_render_shadow_map() consumes the snapshot. Cheap
early-out when shadow mapping is off / the surface isn't an opaque caster / the
entity is a viewmodel / depth-hacked / RF_NOSHADOW. (GPU-skinned IQM never reaches
this — the GPU branch returns before tess.xyz is touched; that's d2-followup-2.)
===================
*/
void vk_shadow_capture_mesh( int firstVert, int numVerts, int firstIdx, int numIdx ) {
	const trRefEntity_t *ent = backEnd.currentEntity;
	const orientationr_t *o;
	shadowMeshSlice_t *sl;
	int v;

	if ( !vk.shadowMap.active ) return;
	if ( ent == NULL || ent == &tr.worldEntity ) return; // worldspawn → bmodels[0] caster buffer
	if ( ent->e.reType != RT_MODEL ) return;
	if ( ent->e.renderfx & ( RF_FIRST_PERSON | RF_DEPTHHACK | RF_NOSHADOW ) ) return;
	if ( !SHADOW_CASTER_OK( tess.shader ) ) return;
	if ( numVerts <= 0 || numIdx <= 0 ) return;
	if ( numVerts > 1024*1024 || numIdx > 4*1024*1024 ) return;          // a single absurd surface
	if ( shadowMeshSnapVertCount + numVerts > 8*1024*1024 ) return;       // ~128 MB of verts — give up gracefully

	shadowMeshSnapVerts   = vk_shadow_snap_grow( shadowMeshSnapVerts,   &shadowMeshSnapVertCap,  shadowMeshSnapVertCount  + numVerts, sizeof( vec4_t ) );
	shadowMeshSnapIndices = vk_shadow_snap_grow( shadowMeshSnapIndices, &shadowMeshSnapIndexCap, shadowMeshSnapIndexCount + numIdx,   sizeof( glIndex_t ) );
	shadowMeshSnapSlices  = vk_shadow_snap_grow( shadowMeshSnapSlices,  &shadowMeshSnapSliceCap, shadowMeshSnapSliceCount + 1,        sizeof( shadowMeshSlice_t ) );

	for ( v = 0; v < numVerts; v++ ) {
		shadowMeshSnapVerts[ shadowMeshSnapVertCount + v ][0] = tess.xyz[ firstVert + v ][0];
		shadowMeshSnapVerts[ shadowMeshSnapVertCount + v ][1] = tess.xyz[ firstVert + v ][1];
		shadowMeshSnapVerts[ shadowMeshSnapVertCount + v ][2] = tess.xyz[ firstVert + v ][2];
		shadowMeshSnapVerts[ shadowMeshSnapVertCount + v ][3] = 1.0f;
	}
	for ( v = 0; v < numIdx; v++ )
		shadowMeshSnapIndices[ shadowMeshSnapIndexCount + v ] = tess.indexes[ firstIdx + v ] - (glIndex_t)firstVert; // rebase to 0; vertexOffset handles the absolute base

	o  = &backEnd.or;
	sl = &shadowMeshSnapSlices[ shadowMeshSnapSliceCount ];
	sl->firstIndex   = (uint32_t)shadowMeshSnapIndexCount;
	sl->indexCount   = (uint32_t)numIdx;
	sl->vertexOffset = shadowMeshSnapVertCount;
	sl->modelMatrix[ 0] = o->axis[0][0]; sl->modelMatrix[ 4] = o->axis[1][0]; sl->modelMatrix[ 8] = o->axis[2][0]; sl->modelMatrix[12] = o->origin[0];
	sl->modelMatrix[ 1] = o->axis[0][1]; sl->modelMatrix[ 5] = o->axis[1][1]; sl->modelMatrix[ 9] = o->axis[2][1]; sl->modelMatrix[13] = o->origin[1];
	sl->modelMatrix[ 2] = o->axis[0][2]; sl->modelMatrix[ 6] = o->axis[1][2]; sl->modelMatrix[10] = o->axis[2][2]; sl->modelMatrix[14] = o->origin[2];
	sl->modelMatrix[ 3] = 0.0f;          sl->modelMatrix[ 7] = 0.0f;          sl->modelMatrix[11] = 0.0f;          sl->modelMatrix[15] = 1.0f;

	shadowMeshSnapVertCount  += numVerts;
	shadowMeshSnapIndexCount += numIdx;
	shadowMeshSnapSliceCount += 1;
}


/*
===================
vk_shutdown_shadow_snap

Phase 6.5.4d2-followup. Called once from vk_shutdown (device teardown). Destroys
each command-buffer slot's host-coherent deformed-mesh-snapshot buffer (the one
vk_render_shadow_map lazily (re)creates) and frees the shadowMeshSnap* CPU
arrays. Idempotent — every field/pointer is NULL-guarded. Not needed on
vk_release_resources (vid_restart keeps the device, so the buffers stay valid).
===================
*/
static void vk_shutdown_shadow_snap( void ) {
	int i;
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.tess[i].shadowSnapMapped ) { qvkUnmapMemory( vk.device, vk.tess[i].shadowSnapMem ); vk.tess[i].shadowSnapMapped = NULL; }
		if ( vk.tess[i].shadowSnapBuf )    { vk_ral_unregister_buffer( vk.tess[i].shadowSnapBuf ); qvkDestroyBuffer( vk.device, vk.tess[i].shadowSnapBuf, NULL ); vk.tess[i].shadowSnapBuf = VK_NULL_HANDLE; }
		if ( vk.tess[i].shadowSnapMem )    { qvkFreeMemory( vk.device, vk.tess[i].shadowSnapMem, NULL );    vk.tess[i].shadowSnapMem = VK_NULL_HANDLE; }
		vk.tess[i].shadowSnapSize = 0;
	}
	if ( shadowMeshSnapVerts )   { ri.Free( shadowMeshSnapVerts );   shadowMeshSnapVerts   = NULL; }
	if ( shadowMeshSnapIndices ) { ri.Free( shadowMeshSnapIndices ); shadowMeshSnapIndices = NULL; }
	if ( shadowMeshSnapSlices )  { ri.Free( shadowMeshSnapSlices );  shadowMeshSnapSlices  = NULL; }
	shadowMeshSnapVertCount  = shadowMeshSnapVertCap  = 0;
	shadowMeshSnapIndexCount = shadowMeshSnapIndexCap = 0;
	shadowMeshSnapSliceCount = shadowMeshSnapSliceCap = 0;
}


/*
===================
vk_build_shadow_caster

Phase 6.5.4b: builds a position-only worldspawn-geometry buffer for the shadow
depth pass, lazily, once per map load (keyed on tr.world->surfaces). Casters
are model-0 BSP surfaces only — SF_FACE / SF_GRID / SF_TRIANGLES with an opaque
material (SHADOW_CASTER_OK). Inline brush models (doors / platforms / ...) are
6.5.4d2's vk_build_bmodel_casters, called from here; MD3/MDL/IQM mesh entities
and ATEST cutouts are still deferred.

The buffer is one device-local allocation laid out as
  [ vec4 positions (xyz, w=1) ][ uint32 indices ]
drawn with a single qvkCmdDrawIndexed. Vertices are not deduplicated across
surfaces — for a depth pass that's fine. Grid (patch) surfaces use their
control net (2 tris/cell) — a coarse but conservative silhouette.

This drives a one-shot queue submit + wait via Ral_AcquireBegunCommandBuffer +
Ral_SubmitAndDispose (same upload path vk_alloc_vbo uses); doing it here means a brief one-time GPU stall
on the first frame after a map load. (Moving it to RE_LoadWorldMap time, like
the world VBO, is a clean follow-up — kept in vk.c for now.)
===================
*/
void vk_build_shadow_caster( void ) {
	const world_t *w = tr.world;
	uint32_t numVerts = 0, numIdx = 0;
	uint32_t vCap = 0, iCap = 0;
	vec4_t *verts = NULL;
	uint32_t *idx = NULL;
	int numCasterSurfs = 0;
	const msurface_t *worldSurfs;
	int worldNumSurfs;
	VkDeviceSize vBytes, iBytes, bufSize, uploadDone;
	VkBufferCreateInfo bdesc;
	VkMemoryRequirements mreq;
	VkMemoryAllocateInfo binfo;
	VkCommandBuffer cb;
	VkBufferCopy region;

	if ( !vk.shadowMap.active || vk.shadowMap.image == VK_NULL_HANDLE ) {
		vk.shadowMap.casterIndexCount = 0;
		return; // shadow mapping off / not set up — nothing to build
	}
	if ( !w || w->surfaces == NULL || w->numsurfaces <= 0 ) {
		vk.shadowMap.casterIndexCount = 0;
		return;
	}
	if ( vk.shadowMap.casterBuiltSurfaces == (const void *)w->surfaces )
		return; // already built for this map

	// drop any stale buffers from a previous map (worldspawn + per-bmodel)
	if ( vk.shadowMap.casterBuf )       { vk_ral_unregister_buffer( vk.shadowMap.casterBuf );       qvkDestroyBuffer( vk.device, vk.shadowMap.casterBuf, NULL );       vk.shadowMap.casterBuf = VK_NULL_HANDLE; }
	if ( vk.shadowMap.casterMem )       { qvkFreeMemory( vk.device, vk.shadowMap.casterMem, NULL );          vk.shadowMap.casterMem = VK_NULL_HANDLE; }
	if ( vk.shadowMap.casterBmodelBuf ) { vk_ral_unregister_buffer( vk.shadowMap.casterBmodelBuf ); qvkDestroyBuffer( vk.device, vk.shadowMap.casterBmodelBuf, NULL ); vk.shadowMap.casterBmodelBuf = VK_NULL_HANDLE; }
	if ( vk.shadowMap.casterBmodelMem ) { qvkFreeMemory( vk.device, vk.shadowMap.casterBmodelMem, NULL );    vk.shadowMap.casterBmodelMem = VK_NULL_HANDLE; }
	if ( vk.shadowMap.bmodelRanges )    { ri.Free( vk.shadowMap.bmodelRanges );                              vk.shadowMap.bmodelRanges = NULL; }
	vk.shadowMap.casterIndexCount = 0;
	vk.shadowMap.casterVtxBytes = 0;
	vk.shadowMap.casterBmodelVtxBytes = 0;
	vk.shadowMap.numBmodelRanges = 0;
	vk.shadowMap.casterBuiltSurfaces = (const void *)w->surfaces; // mark attempted — don't retry every frame on failure

	// inline brush-model casters (doors / platforms / func_rotating / ...) — built
	// on the same map-load gate; drawn per entity with the entity's transform.
	vk_build_bmodel_casters();

	// worldspawn (model 0) caster geometry only — bmodel surfaces (which also
	// live in w->surfaces[], past the worldspawn range) are owned by
	// vk_build_bmodel_casters above.
	if ( w->bmodels && w->numBModels > 0 ) {
		worldSurfs    = w->bmodels[0].firstSurface;
		worldNumSurfs = w->bmodels[0].numSurfaces;
	} else {
		worldSurfs    = w->surfaces;
		worldNumSurfs = w->numsurfaces;
	}

	// pass 1 — count
	numCasterSurfs = vk_shadow_caster_count( worldSurfs, worldNumSurfs, &numVerts, &numIdx );
	if ( numVerts == 0 || numIdx == 0 ) return; // nothing to cast → depth stays cleared
	if ( numVerts > 4u*1024u*1024u || numIdx > 16u*1024u*1024u ) {
		ri.Log( SEV_WARN, "[VK] shadow caster geometry too large (%u verts / %u idx) — skipping\n", numVerts, numIdx );
		return;
	}

	verts = ri.Hunk_AllocateTempMemory( numVerts * sizeof( vec4_t ) );
	idx   = ri.Hunk_AllocateTempMemory( numIdx * sizeof( uint32_t ) );

	// pass 2 — fill
	vk_shadow_caster_fill( worldSurfs, worldNumSurfs, verts, &vCap, idx, &iCap );
	// pass-1 and pass-2 walk the same predicate, so vCap==numVerts && iCap==numIdx

	// device-local buffer: [vec4 verts][uint32 idx]
	vBytes  = (VkDeviceSize)numVerts * sizeof( vec4_t );
	iBytes  = (VkDeviceSize)numIdx   * sizeof( uint32_t );
	bufSize = vBytes + iBytes;
	vk.shadowMap.casterVtxBytes   = (uint32_t)vBytes;
	vk.shadowMap.casterIndexCount = numIdx;

	memset( &bdesc, 0, sizeof( bdesc ) );
	bdesc.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bdesc.size        = bufSize;
	bdesc.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	bdesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bdesc, NULL, &vk.shadowMap.casterBuf ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.shadowMap.casterBuf, &mreq );
	memset( &binfo, 0, sizeof( binfo ) );
	binfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	binfo.allocationSize  = mreq.size;
	binfo.memoryTypeIndex = find_memory_type( mreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &binfo, NULL, &vk.shadowMap.casterMem ) );
	qvkBindBufferMemory( vk.device, vk.shadowMap.casterBuf, vk.shadowMap.casterMem, 0 );
	vk_ral_register_buffer( vk.shadowMap.casterBuf, bufSize,
	                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vk.shadowMap.casterBuf" );

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	uploadDone = 0; // vertices
	while ( uploadDone < vBytes ) {
		VkDeviceSize chunk = vk.staging_buffer.size;
		if ( uploadDone + chunk > vBytes ) chunk = vBytes - uploadDone;
		memcpy( vk.staging_buffer.ptr, (const byte *)verts + uploadDone, chunk );
		{
			ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
			if ( rcmd == NULL ) {
				ri.Log( SEV_ERROR, "[VK->RAL] shadow caster verts: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping vertex chunk\n" );
			} else {
				cb = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
				region.srcOffset = 0; region.dstOffset = uploadDone; region.size = chunk;
				qvkCmdCopyBuffer( cb, vk.staging_buffer.handle, vk.shadowMap.casterBuf, 1, &region );
				Ral_SubmitAndDispose( rcmd );
			}
		}
		uploadDone += chunk;
	}
	uploadDone = 0; // indices
	while ( uploadDone < iBytes ) {
		VkDeviceSize chunk = vk.staging_buffer.size;
		if ( uploadDone + chunk > iBytes ) chunk = iBytes - uploadDone;
		memcpy( vk.staging_buffer.ptr, (const byte *)idx + uploadDone, chunk );
		{
			ralCommandBuffer_t *rcmd = Ral_AcquireBegunCommandBuffer( vk_ral_get_backend(), RAL_QUEUE_GRAPHICS );
			if ( rcmd == NULL ) {
				ri.Log( SEV_ERROR, "[VK->RAL] shadow caster idx: Ral_AcquireBegunCommandBuffer(GRAPHICS) returned NULL; skipping index chunk\n" );
			} else {
				cb = (VkCommandBuffer)Ral_GetCommandBufferHandle( rcmd );
				region.srcOffset = 0; region.dstOffset = vBytes + uploadDone; region.size = chunk;
				qvkCmdCopyBuffer( cb, vk.staging_buffer.handle, vk.shadowMap.casterBuf, 1, &region );
				Ral_SubmitAndDispose( rcmd );
			}
		}
		uploadDone += chunk;
	}

	ri.Hunk_FreeTempMemory( idx );
	ri.Hunk_FreeTempMemory( verts );

	SET_OBJECT_NAME( vk.shadowMap.casterBuf, "shadow caster geometry", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	ri.Log( SEV_INFO, "[VK] shadow caster: %u verts / %u tris from %d world surfaces\n", numVerts, numIdx/3, numCasterSurfs );
}


/*
===================
vk_render_shadow_map

Phase 6.5.4c: Cascaded Shadow Maps — renders the world caster geometry depth
from the SUN's perspective into a 4-layer 2D-array, one cascade per layer.
Called once per RB_LightingPass. Light direction = tr.sunDirection. The view
frustum [near, far] (near = 4, far = min(view zFar, 4096)) is split into 4
cascades by the Practical Split Scheme (lambda = 0.5, log/uniform blend); each
cascade's sub-frustum gets a bounding-sphere fit (rotation-stable) in sun
light-space, a stable texel-grid snap (kills shimmer on camera rotation), and
its own light view-projection matrix. The 4 cascade MVPs + the 4 split
distances are stashed in vk.shadowMap.{cascadeMVP,cascadeSplits}; the PMLIGHT
lit pass (VK_LightingPass) copies them into the lighting UBO so light_frag.tmpl
picks the cascade from the fragment's view depth.

Each cascade gets a fresh depth render-pass instance (clear=1.0, then the
caster geometry drawn with the depth pipeline) — so every array slice lands in
SHADER_READ_ONLY_OPTIMAL before the lit pass samples it. NOT yet: per-cascade
frustum culling (every caster is drawn into every cascade — wasteful but
correct), cascade boundary soft-blend, slope-bias / r_csm* cvars, FRONT_BIT
cull-mode revisit, entity / ATEST casters — all 6.5.4d.
===================
*/
void vk_render_shadow_map( void ) {
	VkRenderPassBeginInfo rpBegin;
	VkClearValue clearVal;
	VkViewport viewport;
	VkRect2D scissor;
	vec3_t sunDir, fwd, lft, vup;        // sun dir + this-frame view basis
	vec3_t xL, yL, zL;                    // light-space basis (zL = sunDir = toward the light)
	float  near0, far0;
	float  splitDist[SHADOWMAP_MAX_CASCADES + 1];
	float  tanX, tanY;
	float  lambda, biasScale;             // Phase 6.5.4d1: r_csmLambda, r_csmBias
	int    numCascades;                   // Phase 6.5.4d1: r_csmCascades (live, 1..4)
	uint32_t mapSize = vk.shadowMap.size;
	int i, casc;
	// Phase 6.5.4d2-followup: claim the prev-frame deformed-mesh snapshot. The
	// shadowMeshSnap* arrays still hold the data; we just take the counts and zero
	// the globals here, at the very top, so every exit path leaves them at 0 — an
	// early-return frame ran no shadow pre-pass, so its captures are correctly
	// discarded (vs leaking stale slices into the next frame). The next frame's main
	// pass re-fills the arrays from index 0 (after this consume).
	int snapVerts  = shadowMeshSnapVertCount;
	int snapIdx    = shadowMeshSnapIndexCount;
	int snapSlices = shadowMeshSnapSliceCount;
	shadowMeshSnapVertCount = shadowMeshSnapIndexCount = shadowMeshSnapSliceCount = 0;

	// Phase 6.5.4d2-followup part 1: prove the capture path is live. Once-per-
	// second SEV_DEBUG line (quiet in normal play; `+set developer 1` to see it),
	// plus a high-watermark since map start. The captured data is staged below
	// but not drawn yet — the per-cascade animated-mesh draw loop is part 2.
	{
		static int diagLast = 0, diagHiV = 0, diagHiI = 0, diagHiS = 0;
		int diagNow = ri.Milliseconds();
		if ( snapVerts  > diagHiV ) diagHiV = snapVerts;
		if ( snapIdx    > diagHiI ) diagHiI = snapIdx;
		if ( snapSlices > diagHiS ) diagHiS = snapSlices;
		if ( ( snapVerts || snapIdx || snapSlices ) && diagNow - diagLast >= 1000 ) {
			diagLast = diagNow;
			ri.Log( SEV_DEBUG, "[VK-DIAG-SNAP] %d verts / %d idx / %d slices captured this frame "
				"(hi-watermark since map start: %d / %d / %d)\n",
				snapVerts, snapIdx, snapSlices, diagHiV, diagHiI, diagHiS );
		}
	}

	if ( vk.shadowMap.image == VK_NULL_HANDLE || vk.shadowMap.renderPass == VK_NULL_HANDLE )
		return;

	// build the worldspawn + inline-bmodel caster geometry once per map (lazy)
	vk_build_shadow_caster();

	// stage the prev-frame deformed-mesh snapshot into this command-buffer slot's
	// host-coherent buffer (lazily (re)created on growth — rare; one-time stall).
	if ( snapVerts > 0 && snapIdx > 0 && snapSlices > 0 ) {
		VkDeviceSize need = (VkDeviceSize)snapVerts * sizeof( vec4_t ) + (VkDeviceSize)snapIdx * sizeof( glIndex_t );
		if ( need > 32u*1024u*1024u ) {
			snapVerts = snapIdx = snapSlices = 0; // pathological — skip the mesh casters this frame
		} else {
			if ( vk.cmd->shadowSnapBuf == VK_NULL_HANDLE || vk.cmd->shadowSnapSize < need ) {
				VkBufferCreateInfo   bd;
				VkMemoryRequirements mr;
				VkMemoryAllocateInfo ma;
				VkDeviceSize         sz = 65536;
				while ( sz < need ) sz <<= 1;
				vk_wait_idle();
				if ( vk.cmd->shadowSnapMapped ) { qvkUnmapMemory( vk.device, vk.cmd->shadowSnapMem ); vk.cmd->shadowSnapMapped = NULL; }
				if ( vk.cmd->shadowSnapBuf )    { vk_ral_unregister_buffer( vk.cmd->shadowSnapBuf ); qvkDestroyBuffer( vk.device, vk.cmd->shadowSnapBuf, NULL ); vk.cmd->shadowSnapBuf = VK_NULL_HANDLE; }
				if ( vk.cmd->shadowSnapMem )    { qvkFreeMemory( vk.device, vk.cmd->shadowSnapMem, NULL );    vk.cmd->shadowSnapMem = VK_NULL_HANDLE; }
				memset( &bd, 0, sizeof( bd ) );
				bd.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bd.size = sz;
				bd.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT; bd.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				VK_CHECK( qvkCreateBuffer( vk.device, &bd, NULL, &vk.cmd->shadowSnapBuf ) );
				qvkGetBufferMemoryRequirements( vk.device, vk.cmd->shadowSnapBuf, &mr );
				memset( &ma, 0, sizeof( ma ) );
				ma.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; ma.allocationSize = mr.size;
				ma.memoryTypeIndex = find_memory_type( mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
				VK_CHECK( qvkAllocateMemory( vk.device, &ma, NULL, &vk.cmd->shadowSnapMem ) );
				qvkBindBufferMemory( vk.device, vk.cmd->shadowSnapBuf, vk.cmd->shadowSnapMem, 0 );
				VK_CHECK( qvkMapMemory( vk.device, vk.cmd->shadowSnapMem, 0, VK_WHOLE_SIZE, 0, &vk.cmd->shadowSnapMapped ) );
				vk.cmd->shadowSnapSize = sz;
				vk_ral_register_buffer( vk.cmd->shadowSnapBuf, sz,
				                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				                        "vk.cmd.shadowSnapBuf" );
				SET_OBJECT_NAME( vk.cmd->shadowSnapBuf, "shadow caster snapshot (animated meshes)", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
			}
			memcpy( (byte *)vk.cmd->shadowSnapMapped, shadowMeshSnapVerts, (size_t)snapVerts * sizeof( vec4_t ) );
			memcpy( (byte *)vk.cmd->shadowSnapMapped + (size_t)snapVerts * sizeof( vec4_t ), shadowMeshSnapIndices, (size_t)snapIdx * sizeof( glIndex_t ) );
		}
	} else {
		snapSlices = 0; // nothing captured last frame
	}

	// --- Phase 6.5.4d1 live cvars -------------------------------------------
	lambda      = ri.Cvar_Get( "r_csmLambda", "0.5", 0 )->value;
	if ( lambda < 0.0f ) lambda = 0.0f; else if ( lambda > 1.0f ) lambda = 1.0f;
	biasScale   = ri.Cvar_Get( "r_csmBias", "0.005", 0 )->value;
	if ( biasScale < 0.0f ) biasScale = 0.0f; else if ( biasScale > 0.1f ) biasScale = 0.1f;
	numCascades = ri.Cvar_Get( "r_csmCascades", "1", 0 )->integer;
	if ( numCascades < 1 ) numCascades = 1; else if ( numCascades > SHADOWMAP_MAX_CASCADES ) numCascades = SHADOWMAP_MAX_CASCADES;

	// --- sun direction (toward the sun) -------------------------------------
	VectorCopy( tr.sunDirection, sunDir );
	if ( VectorNormalize( sunDir ) < 0.01f ) {
		VectorSet( sunDir, 0.45f, 0.3f, 0.9f );   // tr_bsp.c default
		VectorNormalize( sunDir );
	}

	// --- this frame's view basis + Practical Split distances ----------------
	VectorCopy( backEnd.viewParms.or.axis[0], fwd );
	VectorCopy( backEnd.viewParms.or.axis[1], lft );
	VectorCopy( backEnd.viewParms.or.axis[2], vup );
	tanX = tanf( DEG2RAD( backEnd.viewParms.fovX ) * 0.5f );
	tanY = tanf( DEG2RAD( backEnd.viewParms.fovY ) * 0.5f );

	near0 = 4.0f;
	far0  = backEnd.viewParms.zFar;
	{
		float maxd = ri.Cvar_Get( "r_csmMaxDistance", "4096", 0 )->value;
		if ( maxd < 256.0f ) maxd = 256.0f; else if ( maxd > 16384.0f ) maxd = 16384.0f;
		if ( far0 <= near0 || far0 > maxd ) far0 = maxd;
	}

	splitDist[0] = near0;
	splitDist[SHADOWMAP_MAX_CASCADES] = far0;
	for ( i = 1; i < SHADOWMAP_MAX_CASCADES; i++ ) {
		float p   = (float)i / (float)SHADOWMAP_MAX_CASCADES;
		float uni = near0 + (far0 - near0) * p;
		float lg  = near0 * powf( far0 / near0, p );
		splitDist[i] = lambda * lg + (1.0f - lambda) * uni;
	}
	{
		// log split distances once per (map, cvar-combo) change
		static const void *loggedFor = NULL;
		static float lastL = -1.0f, lastF = -1.0f; static int lastN = -1;
		if ( tr.world && ( loggedFor != (const void *)tr.world->surfaces || lastL != lambda || lastF != far0 || lastN != numCascades ) ) {
			loggedFor = (const void *)tr.world->surfaces; lastL = lambda; lastF = far0; lastN = numCascades;
			ri.Log( SEV_INFO, "[VK] CSM splits (near=%.0f far=%.0f lambda=%.2f cascades=%d): %.1f %.1f %.1f %.1f %.1f\n",
				near0, far0, lambda, numCascades, splitDist[0], splitDist[1], splitDist[2], splitDist[3], splitDist[4] );
		}
	}

	// --- light-space basis (zL = toward the light = sunDir) -----------------
	VectorCopy( sunDir, zL );
	if ( fabs( zL[2] ) > 0.95f )
		VectorSet( yL, 0, 1, 0 );
	else
		VectorSet( yL, 0, 0, 1 );
	CrossProduct( yL, zL, xL );  VectorNormalize( xL );
	CrossProduct( zL, xL, yL );  VectorNormalize( yL );

	// --- depth render passes — one per cascade ------------------------------
	// Phase 6.5.4d1-fix-A: this runs as a per-frame PRE-PASS from vk_begin_frame
	// (right after the particle compute, before any render pass opens) — so no
	// render pass is open on entry, and none is left open on exit. It used to be
	// a mid-frame call from RB_LightingPass that bracketed itself with
	// vk_end_render_pass() / vk_begin_main_render_pass(); the latter re-clears
	// the main FBO (the d1 black-screen-on-dlight regression). No more.
	memset( &clearVal, 0, sizeof( clearVal ) );
	clearVal.depthStencil.depth = 1.0f;
	viewport.x = 0; viewport.y = 0;
	viewport.width = (float)mapSize; viewport.height = (float)mapSize;
	viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
	scissor.offset.x = 0; scissor.offset.y = 0;
	scissor.extent.width = mapSize; scissor.extent.height = mapSize;

	for ( casc = 0; casc < SHADOWMAP_MAX_CASCADES; casc++ ) {
		vec3_t corners[8], centerW, cn, cf;
		float  cnDist = splitDist[casc], cfDist = splitDist[casc + 1];
		float  radiusW, centerLx, centerLy, centerLz, texelSize, zNearL, zFarL, dz;
		float  *M = vk.shadowMap.cascadeMVP[casc];
		float  mm[16];  // Phase 6.5.4d2: caster model->world push constant (offset 64)

		// sub-frustum corners (world space) for [cnDist, cfDist]
		VectorMA( backEnd.viewParms.or.origin, cnDist, fwd, cn );
		VectorMA( backEnd.viewParms.or.origin, cfDist, fwd, cf );
		{
			float hwn = cnDist * tanX, hhn = cnDist * tanY;
			float hwf = cfDist * tanX, hhf = cfDist * tanY;
			VectorMA( cn,  hwn, lft, corners[0] ); VectorMA( corners[0],  hhn, vup, corners[0] );
			VectorMA( cn,  hwn, lft, corners[1] ); VectorMA( corners[1], -hhn, vup, corners[1] );
			VectorMA( cn, -hwn, lft, corners[2] ); VectorMA( corners[2],  hhn, vup, corners[2] );
			VectorMA( cn, -hwn, lft, corners[3] ); VectorMA( corners[3], -hhn, vup, corners[3] );
			VectorMA( cf,  hwf, lft, corners[4] ); VectorMA( corners[4],  hhf, vup, corners[4] );
			VectorMA( cf,  hwf, lft, corners[5] ); VectorMA( corners[5], -hhf, vup, corners[5] );
			VectorMA( cf, -hwf, lft, corners[6] ); VectorMA( corners[6],  hhf, vup, corners[6] );
			VectorMA( cf, -hwf, lft, corners[7] ); VectorMA( corners[7], -hhf, vup, corners[7] );
		}

		// bounding sphere — centre = mean of corners, radius = max distance.
		// (radius is rotation-invariant — the basis for stable cascade snap.)
		VectorClear( centerW );
		for ( i = 0; i < 8; i++ ) VectorAdd( centerW, corners[i], centerW );
		VectorScale( centerW, 1.0f / 8.0f, centerW );
		radiusW = 0.0f;
		for ( i = 0; i < 8; i++ ) {
			vec3_t d; float l;
			VectorSubtract( corners[i], centerW, d );
			l = VectorLength( d );
			if ( l > radiusW ) radiusW = l;
		}
		if ( radiusW < 1.0f ) radiusW = 1.0f;

		// light-space sphere centre + stable texel-grid snap (XY only — Z snap
		// would just bias depth, no shimmer benefit).
		centerLx = DotProduct( xL, centerW );
		centerLy = DotProduct( yL, centerW );
		centerLz = DotProduct( zL, centerW );
		texelSize = ( 2.0f * radiusW ) / (float)mapSize;
		centerLx = floorf( centerLx / texelSize ) * texelSize;
		centerLy = floorf( centerLy / texelSize ) * texelSize;

		// light-space depth bounds. zL increases toward the sun: the near plane
		// (toward the sun) is pulled back +1024u beyond the cascade sphere so
		// casters between the sun and the cascade still write depth; the far
		// plane is the cascade's deep edge.
		zNearL = centerLz + radiusW + 1024.0f;
		zFarL  = centerLz - radiusW;
		dz = zNearL - zFarL;
		if ( dz < 1.0f ) dz = 1.0f;

		// build M directly (column-major). NDC x/y in [-1,1] over the snapped
		// [centreL ± radiusW] window; depth in [0,1] with 0 = near the light,
		// 1 = far (agrees with the LESS_OR_EQUAL depth pipeline + clear = 1.0).
		M[0]  =  xL[0] / radiusW; M[4]  =  xL[1] / radiusW; M[8]  =  xL[2] / radiusW; M[12] = -centerLx / radiusW;
		M[1]  =  yL[0] / radiusW; M[5]  =  yL[1] / radiusW; M[9]  =  yL[2] / radiusW; M[13] = -centerLy / radiusW;
		M[2]  = -zL[0] / dz;      M[6]  = -zL[1] / dz;      M[10] = -zL[2] / dz;      M[14] =  zNearL / dz;
		M[3]  = 0.0f;             M[7]  = 0.0f;             M[11] = 0.0f;             M[15] = 1.0f;

		// view-space split distance (planar depth) for the shader's selector
		vk.shadowMap.cascadeSplits[casc] = cfDist;

		// render casters into this cascade's layer
		rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBegin.pNext = NULL;
		rpBegin.renderPass = vk.shadowMap.renderPass;
		rpBegin.framebuffer = vk.shadowMap.framebuffer[casc];
		rpBegin.renderArea.offset.x = 0; rpBegin.renderArea.offset.y = 0;
		rpBegin.renderArea.extent.width = mapSize; rpBegin.renderArea.extent.height = mapSize;
		rpBegin.clearValueCount = 1; rpBegin.pClearValues = &clearVal;

		qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
		qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
		// Phase 7.4c-cmd — parallel-paths begin-render-pass + viewport/scissor (shadow cascade).
		vk_ral_parallel_begin_render_pass( &rpBegin );
		Ral_CmdSetViewport(vk.cmd->ral_cmd, (const ralViewport_t *)&viewport);
		Ral_CmdSetScissor(vk.cmd->ral_cmd, (const ralRect_t *)&scissor);
		// cascadeMVP (push offset 0) is constant across this cascade's draws.
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.shadowMap.depthLayout,
			VK_SHADER_STAGE_VERTEX_BIT, 0, 64, M );
		// Phase 7.4c-cmd — parallel-paths push-constants (shadow cascade MVP).
		Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.shadowMap.depthLayout),
			VK_SHADER_STAGE_VERTEX_BIT, 0, 64, M );

		// Cascades [numCascades .. 3] are left at the clear value (depth 1.0 ⇒
		// the shader's step(receiverDepth-bias, 1.0) == 1 ⇒ fully lit). Only the
		// enabled cascades carry caster geometry.
		if ( casc < numCascades && vk.shadowMap.depthPipeline != VK_NULL_HANDLE ) {
			const qboolean haveWorld  = ( vk.shadowMap.casterBuf != VK_NULL_HANDLE && vk.shadowMap.casterIndexCount > 0 ) ? qtrue : qfalse;
			const qboolean haveBmodel = ( vk.shadowMap.casterBmodelBuf != VK_NULL_HANDLE && vk.shadowMap.bmodelRanges != NULL ) ? qtrue : qfalse;
			// Phase 6.5.4d2-followup part 2: prev-frame deformed-mesh casters,
			// staged into vk.cmd->shadowSnapBuf at the top of this function
			// (verts at offset 0, then 0-based indices). snapVerts / snapSlices
			// are the locals claimed by the consume block at function start.
			const qboolean haveSnap   = ( snapSlices > 0 && vk.cmd->shadowSnapBuf != VK_NULL_HANDLE ) ? qtrue : qfalse;
			VkDeviceSize voff = 0;

			if ( haveWorld || haveBmodel || haveSnap ) {
				qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.shadowMap.depthPipeline );
				// Phase 7.4c-cmd — parallel-paths bind-pipeline (shadow depth).
				Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.shadowMap.depthPipeline ));
				// dynamic slope-scaled depth bias from r_csmBias (default 0.005 ⇒ the
				// prior fixed conservative constant/slope of 2.0/3.0).
				qvkCmdSetDepthBias( vk.cmd->command_buffer, biasScale * 400.0f, 0.0f, biasScale * 600.0f );
				// Phase 7.4c-cmd — parallel-paths set-depth-bias (shadow).
				Ral_CmdSetDepthBias( vk.cmd->ral_cmd, biasScale * 400.0f, 0.0f, biasScale * 600.0f );
			}

			// worldspawn casters — identity model->world (their verts are world-space)
			if ( haveWorld ) {
				memset( mm, 0, sizeof( mm ) ); mm[0] = mm[5] = mm[10] = mm[15] = 1.0f;
				qvkCmdPushConstants( vk.cmd->command_buffer, vk.shadowMap.depthLayout, VK_SHADER_STAGE_VERTEX_BIT, 64, 64, mm );
				qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1, &vk.shadowMap.casterBuf, &voff );
				qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.shadowMap.casterBuf, vk.shadowMap.casterVtxBytes, VK_INDEX_TYPE_UINT32 );
				qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.shadowMap.casterIndexCount, 1, 0, 0, 0 );
				// Phase 7.4c-cmd — parallel-paths (shadow worldspawn casters).
				Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.shadowMap.depthLayout), VK_SHADER_STAGE_VERTEX_BIT, 64, 64, mm );
				{ ralBuffer_t *rb = vk_ral_lookup_buffer( vk.shadowMap.casterBuf );
				  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, 0, 1, &rb, (const uint64_t *)&voff ); }
				Ral_CmdBindIndexBuffer(vk.cmd->ral_cmd, vk_ral_lookup_buffer(vk.shadowMap.casterBuf), vk.shadowMap.casterVtxBytes, RAL_INDEX_UINT32);
				Ral_CmdDrawIndexed      ( vk.cmd->ral_cmd, vk.shadowMap.casterIndexCount, 1, 0, 0, 0 );
			}

			// Phase 6.5.4d2: inline brush-model casters — one draw per visible
			// MOD_BRUSH entity, each with that entity's [axis|origin] model->world
			// (so a moved door / rotated platform casts its shadow at the current
			// position). Uses backEnd.refdef from the PREVIOUS frame, same 1-frame
			// lag as the cascade fit above — imperceptible at frame motion scale.
			if ( haveBmodel && backEnd.refdef.num_entities > 0 ) {
				int ei;
				qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1, &vk.shadowMap.casterBmodelBuf, &voff );
				qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.shadowMap.casterBmodelBuf, vk.shadowMap.casterBmodelVtxBytes, VK_INDEX_TYPE_UINT32 );
				// Phase 7.4c-cmd — parallel-paths (shadow bmodel cast bind).
				{ ralBuffer_t *rb = vk_ral_lookup_buffer( vk.shadowMap.casterBmodelBuf );
				  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, 0, 1, &rb, (const uint64_t *)&voff ); }
				Ral_CmdBindIndexBuffer(vk.cmd->ral_cmd, vk_ral_lookup_buffer(vk.shadowMap.casterBmodelBuf), vk.shadowMap.casterBmodelVtxBytes, RAL_INDEX_UINT32);
				for ( ei = 0; ei < backEnd.refdef.num_entities; ei++ ) {
					const trRefEntity_t *ent = &backEnd.refdef.entities[ei];
					const model_t *mod;
					const vkBmodelCasterRange_t *rng;
					int bmIdx;
					if ( ent->e.reType != RT_MODEL ) continue;
					if ( ent->e.renderfx & ( RF_FIRST_PERSON | RF_DEPTHHACK | RF_NOSHADOW ) ) continue;
					mod = R_GetModelByHandle( ent->e.hModel );
					if ( mod == NULL || mod->type != MOD_BRUSH || mod->bmodel == NULL ) continue;
					if ( tr.world == NULL || tr.world->bmodels == NULL ) continue;
					bmIdx = (int)( mod->bmodel - tr.world->bmodels );
					// slot 0 == worldspawn (already drawn); out of range == a separate-BSP
					// brush model (not in this world's caster set this turn) — skip both.
					if ( bmIdx < 1 || bmIdx >= vk.shadowMap.numBmodelRanges ) continue;
					rng = &vk.shadowMap.bmodelRanges[bmIdx];
					if ( rng->indexCount == 0 ) continue;
					// model->world = [axis | origin] (column-major) into push offset 64
					mm[0] = ent->e.axis[0][0]; mm[4] = ent->e.axis[1][0]; mm[ 8] = ent->e.axis[2][0]; mm[12] = ent->e.origin[0];
					mm[1] = ent->e.axis[0][1]; mm[5] = ent->e.axis[1][1]; mm[ 9] = ent->e.axis[2][1]; mm[13] = ent->e.origin[1];
					mm[2] = ent->e.axis[0][2]; mm[6] = ent->e.axis[1][2]; mm[10] = ent->e.axis[2][2]; mm[14] = ent->e.origin[2];
					mm[3] = 0.0f;              mm[7] = 0.0f;              mm[11] = 0.0f;              mm[15] = 1.0f;
					qvkCmdPushConstants( vk.cmd->command_buffer, vk.shadowMap.depthLayout, VK_SHADER_STAGE_VERTEX_BIT, 64, 64, mm );
					qvkCmdDrawIndexed( vk.cmd->command_buffer, rng->indexCount, 1, rng->firstIndex, 0, 0 );
					// Phase 7.4c-cmd — parallel-paths (shadow bmodel per-entity).
					Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.shadowMap.depthLayout), VK_SHADER_STAGE_VERTEX_BIT, 64, 64, mm );
					Ral_CmdDrawIndexed  ( vk.cmd->ral_cmd, rng->indexCount, 1, rng->firstIndex, 0, 0 );
				}
			}

			// Phase 6.5.4d2-followup part 2: deformed-mesh casters — MD3 (RB_SurfaceMesh),
			// MDL-as-MD3, and CPU-skinned IQM (RB_IQMSurfaceAnim CPU path) — snapshotted
			// last frame by vk_shadow_capture_mesh and staged into vk.cmd->shadowSnapBuf at
			// the top of this function. One draw per captured slice, each with that slice's
			// [axis|origin] model->world pushed at offset 64 (cascadeMVP @ 0 from the cascade
			// setup is still in effect; the depth pipeline + dynamic depth bias were bound by
			// the haveWorld/haveBmodel/haveSnap block above). Same 1-frame lag as the cascade
			// fit / bmodel casters — imperceptible at frame-motion scale. The CPU arrays
			// rebuild from index 0 every frame, so a despawned entity leaves no stale shadow
			// and a freshly-spawned one is shadowless for one frame (both acceptable).
			// GPU-skinned IQM never reaches the capture hook (it returns before tess.xyz is
			// touched) — that path is d2-followup-2.
			if ( haveSnap ) {
				VkDeviceSize snapVertOff = 0;
				VkDeviceSize snapIdxOff  = (VkDeviceSize)snapVerts * sizeof( vec4_t );
				int si;
				qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1, &vk.cmd->shadowSnapBuf, &snapVertOff );
				qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.cmd->shadowSnapBuf, snapIdxOff, VK_INDEX_TYPE_UINT32 );
				// Phase 7.4c-cmd — parallel-paths (shadow snap bind).
				{ ralBuffer_t *rb = vk_ral_lookup_buffer( vk.cmd->shadowSnapBuf );
				  Ral_CmdBindVertexBuffers( vk.cmd->ral_cmd, 0, 1, &rb, (const uint64_t *)&snapVertOff ); }
				Ral_CmdBindIndexBuffer(vk.cmd->ral_cmd, vk_ral_lookup_buffer(vk.cmd->shadowSnapBuf), snapIdxOff, RAL_INDEX_UINT32);
				for ( si = 0; si < snapSlices; si++ ) {
					const shadowMeshSlice_t *sl = &shadowMeshSnapSlices[si];
					// modelMatrix is already a full 16-float column-major model->world
					// (vk_shadow_capture_mesh fills [axis|origin] + bottom row {0,0,0,1}).
					qvkCmdPushConstants( vk.cmd->command_buffer, vk.shadowMap.depthLayout, VK_SHADER_STAGE_VERTEX_BIT, 64, 64, sl->modelMatrix );
					qvkCmdDrawIndexed( vk.cmd->command_buffer, sl->indexCount, 1, sl->firstIndex, sl->vertexOffset, 0 );
					// Phase 7.4c-cmd — parallel-paths (shadow snap per-slice draw).
					Ral_CmdPushConstantsLayout( vk.cmd->ral_cmd, vk_ral_lookup_pipeline_layout(vk.shadowMap.depthLayout), VK_SHADER_STAGE_VERTEX_BIT, 64, 64, sl->modelMatrix );
					Ral_CmdDrawIndexed  ( vk.cmd->ral_cmd, sl->indexCount, 1, sl->firstIndex, sl->vertexOffset, 0 );
				}
			}
		}

		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		// Phase 7.4c-cmd — parallel-paths end-render-pass (shadow cascade).
		Ral_CmdEndRenderPass( vk.cmd->ral_cmd );
	}

	// On exit no render pass is open. The caller (vk_begin_frame) opens the
	// main / screenmap pass next, and vk_begin_render_pass() there resets
	// last_pipeline / depth_range. The shadow pass binds only a push-constant-
	// only pipeline (no descriptor sets), so the descriptor cache is untouched —
	// nothing to invalidate here.
}
#endif // FEAT_SHADOW_MAPPING


qboolean vk_bloom( void )
{
	uint32_t i;

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces || !vk.fboActive )
	{
		return qfalse;
	}

	vk_end_render_pass(); // end main
	vk_gpu_ts_write( "world_done" ); // outside render pass — MoltenVK timestamps only resolve at encoder boundaries

#ifdef __APPLE__
	// MoltenVK/TBDR: subpass external deps alone don't flush tile cache on Apple Silicon.
	// Explicit barrier makes main-pass writes to color_image visible to bloom_extract's sampler.
	if ( r_vkApplePinkBarrier->integer ) {
		VkImageMemoryBarrier b;
		memset( &b, 0, sizeof( b ) );
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = vk.color_image;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.levelCount = 1;
		b.subresourceRange.layerCount = 1;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &b );
		// Phase 7.4c-submit-A4 — typed parallel-paths pipeline-barrier (Apple
		// TBDR bloom path; color_image is adopted at boot).
		{
			static qboolean warned;
			vk_ral_parallel_pipeline_barrier_image(
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				1, &b, &warned, "apple-tbdr-bloom-color" );
		}
	}
#endif

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_extract_pipeline );
	// Phase 7.4c-cmd — parallel-paths bind-pipeline (bloom extract).
	Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.bloom_extract_pipeline ));
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
	// Phase 7.4c-bindgroup — parallel-paths bind-side record (bloom extract).
	vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	// Phase 7.4c-cmd — parallel-paths draw (bloom extract).
	Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );
	vk_end_render_pass();

	{
	const int num_bloom_passes = Com_Clamp( 1, VK_NUM_BLOOM_PASSES, r_bloomPasses->integer );

	for ( i = 0; i < num_bloom_passes*2; i+=2 ) {
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline (bloom blur H).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.blur_pipeline[i+0] ));
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record (bloom blur H).
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths draw (bloom blur H).
		Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline (bloom blur V).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.blur_pipeline[i+1] ));
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record (bloom blur V).
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths draw (bloom blur V).
		Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );
		vk_end_render_pass();
#if 0
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+2], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#endif
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			// pad unused slots with the last active (most diffuse) blur level
			int src = (i < num_bloom_passes) ? i : (num_bloom_passes - 1);
			dset[i] = vk.bloom_image_descriptor[(src+1)*2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.bloom_blend_pipeline );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline (bloom blend).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.bloom_blend_pipeline ));
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		// Phase 7.4c-bindgroup — parallel-paths bind-side record (bloom_blend N-set bundle).
		vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		// Phase 7.4c-cmd — parallel-paths draw (bloom blend).
		Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );
	}
	} // num_bloom_passes scope

	// invalidate pipeline state cache
	//vk.cmd->last_pipeline = VK_NULL_HANDLE;

	if ( vk.cmd->last_pipeline != VK_NULL_HANDLE )
	{
		// restore last pipeline
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cmd->last_pipeline );
		// Phase 7.4c-cmd — parallel-paths bind-pipeline (post-bloom restore).
		Ral_CmdBindPipeline(vk.cmd->ral_cmd, vk_ral_lookup_pipeline(vk.cmd->last_pipeline ));

		vk_update_mvp( NULL );

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/ ) {
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 1, &vk.cmd->descriptor_set.offset[i] );
					// Phase 7.4c-bindgroup — parallel-paths bind-side record (post-bloom rebind, uniform set). TODO_7.4c-cmd: per-frame rotating set unadopted, helper auto-skips via NULL lookup today.
					vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 1, &vk.cmd->descriptor_set.offset[i] );
				} else {
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
					// Phase 7.4c-bindgroup — parallel-paths bind-side record (post-bloom rebind, non-uniform sets). TODO_7.4c-cmd: per-frame rotating texture set unadopted, helper auto-skips via NULL lookup today.
					vk_ral_parallel_bind_descriptor_sets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
				}
			}
		}
	}

	backEnd.doneBloom = qtrue;

	return qtrue;
}
