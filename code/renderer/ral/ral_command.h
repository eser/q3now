// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_command.h — command-buffer recording, submission, dynamic rendering.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.7, §6.2, §9.3).
//
// Queue selection is per-command-buffer (graphics / async-compute / async-
// transfer). Recording covers draw, dispatch, copy, barrier, push-constant,
// bind, indirect, timestamp and debug-label ops. The renderer stays
// imperative (§2.3) — this is a recording surface, not a frame graph.

#ifndef WIRED_RAL_COMMAND_H
#define WIRED_RAL_COMMAND_H

#include "ral_types.h"
#include "ral_resource.h"   // ralFilter_t, ralPipelineLayout_t-forward references for the typed cmd surface

#ifdef __cplusplus
extern "C" {
#endif

// ── per-frame lifecycle ─────────────────────────────────────────────────
// Ral_BeginFrame advances the frame counter, waits the frame fence being
// reused, drains the deferred-destroy queue (resources whose destroy frame is
// at least MAX_FRAMES_IN_FLIGHT behind), and resets that fence. Ral_EndFrame
// closes the frame (signals the frame fence). Consumers that don't run a
// per-frame render loop (Phase 7.3 has none) may skip these; the resource
// layer falls back to wait-idle-on-shutdown either way.
void Ral_BeginFrame( ralBackend_t *b );
void Ral_EndFrame  ( ralBackend_t *b );

// ── command buffer lifecycle ────────────────────────────────────────────
ralCommandBuffer_t *Ral_AcquireCommandBuffer ( ralBackend_t *b, ralQueueType_t q );
void                Ral_BeginCommandBuffer   ( ralCommandBuffer_t *cb );
void                Ral_EndCommandBuffer     ( ralCommandBuffer_t *cb );
void                Ral_DestroyCommandBuffer ( ralCommandBuffer_t *cb );    // usually superseded by Ral_PoolReset
void                Ral_PoolReset            ( ralBackend_t *b, ralQueueType_t q );   // vkResetCommandPool for the queue's pool

// Phase 7.4c-cmd / Phase 7.4c-submit-BC-B — combined Acquire+Begin helper for
// one-shot command buffers. Acquires a fresh VkCommandBuffer from RAL's pool
// for the given queue, immediately begins it in
// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT mode, and returns the wrapper
// in RECORDING state. Caller must call Ral_EndCommandBuffer before submission
// and Ral_SubmitAndDispose (or manual Submit + Ral_DestroyCommandBuffer) when
// finished. ownsBuffer=qtrue → underlying VkCommandBuffer returned to pool on
// Destroy.
//
// Used by:
//  - vk_begin_frame's per-frame parallel-paths buffer (7.4c-cmd, vk.c:~19321
//    with RAL_QUEUE_GRAPHICS).
//  - The 13 one-shot staging / screenshot / shadow-caster sites that 7.4c-
//    submit-BC-B migrated from the retired legacy Vk one-shot helpers (all
//    RAL_QUEUE_GRAPHICS to preserve the legacy graphics-queue serialization
//    with rendering work; RAL_QUEUE_TRANSFER promotion is Phase 7.15 scope).
//
// Phase 7.4c-submit-D-shim — renamed to reflect actual Acquire+Begin
// semantics. The prior misleading "Adopt"-named identifier was a leftover
// from an earlier parallel-paths model where this function once wrapped a
// renderer-owned VkCommandBuffer; that adoption path migrated to
// Ral_WrapCommandBuffer (BC-C-final), which is the true "wrap an external
// buffer" surface today.
ralCommandBuffer_t *Ral_AcquireBegunCommandBuffer( ralBackend_t *b, ralQueueType_t q );

// Phase 7.4c-submit-BC-C-final — wrap an EXTERNALLY-allocated VkCommandBuffer
// in a ralCommandBuffer_t with ownsBuffer=qfalse. Unlike Ral_AcquireCommandBuffer
// (allocates fresh from RAL's pool) and Ral_AcquireBegunCommandBuffer (allocates
// fresh + begins), this wraps a buffer the caller already owns + already begun +
// already recorded. Ral_DestroyCommandBuffer on the wrapper frees only the
// wrapper struct, not the underlying VkCommandBuffer — the caller retains
// alloc/reset/begin/end/free lifecycle. Used by the per-frame submit migration
// where the renderer's legacy vk.cmd->command_buffer keeps its lifecycle
// intact but Ral_Submit replaces the qvkQueueSubmit call site.
ralCommandBuffer_t *Ral_WrapCommandBuffer( ralBackend_t  *b,
                                           void          *externalCommandBuffer, /* VkCommandBuffer on Vulkan */
                                           ralQueueType_t q );

// Phase 7.4c-submit-BC-B — end + submit + queue-idle + free in a single
// call. Mirrors the retired legacy Vk one-shot helper's semantics
// (synchronous-completion-before-return). Safe to call with NULL (no-op).
// `cmd` MUST NOT be used after this call. The submit target queue is read
// from cmd->queue (set by Ral_AcquireBegunCommandBuffer at acquisition time).
//
// Internally: Ral_EndCommandBuffer → Ral_Submit with a temp signalFence →
// Ral_WaitFence(infinite) → Ral_DestroyCommandBuffer (frees the buffer back
// to the pool because ownsBuffer=qtrue). The fence-based wait matches the
// established RAL "submit and wait synchronously" idiom at
// ral_vulkan_command.c's RunAsyncTest tests; explicit queue-wait-idle is
// not exposed on the RAL surface.
void Ral_SubmitAndDispose( ralCommandBuffer_t *cmd );

// Phase 7.4c-cmd — return the backend-native cmd buffer handle (VkCommandBuffer
// on Vulkan) for a ralCommandBuffer_t. NULL-safe. Used by the parallel-paths
// renderer to feed Ral_CmdBindBindGroups (and a few other void*-handle entry
// points) the RAL-allocated parallel cmd buffer instead of the renderer's
// legacy VkCommandBuffer. Mirrors Ral_GetBindGroupHandle.
void *Ral_GetCommandBufferHandle( const ralCommandBuffer_t *cb );

// ── submission ──────────────────────────────────────────────────────────
typedef struct {
	ralCommandBuffer_t **commandBuffers;
	uint32_t             numCommandBuffers;
	ralSemaphore_t     **waitSemaphores;     // binary or timeline
	uint32_t             numWaitSemaphores;
	const uint64_t      *waitValues;         // timeline wait values (parallel to waitSemaphores; ignored for binary)
	ralSemaphore_t     **signalSemaphores;
	uint32_t             numSignalSemaphores;
	const uint64_t      *signalValues;       // timeline signal values (parallel to signalSemaphores)
	ralFence_t          *signalFence;        // optional
} ralSubmitInfo_t;

void Ral_Submit( ralBackend_t *b, ralQueueType_t q, const ralSubmitInfo_t *si );

// Phase 7.4c-submit-followup-present-1 — host-side wait until all work
// previously submitted on the specified queue completes. Equivalent to
// vkQueueWaitIdle on Vulkan. Returns ralSuccess / ralErrorDeviceLost.
// Used by the renderer's vk_queue_wait_idle helper after the BC-B
// retargeting; called from tr_backend.c at end-of-frame-batch and
// shutdown paths where the renderer needs a synchronization point.
ralResult_t Ral_WaitQueueIdle( ralBackend_t *b, ralQueueType_t q );

// ── dynamic rendering (§6.2) ────────────────────────────────────────────
typedef struct {
	ralTexture_t   *colorAttachments[RAL_MAX_COLOR_ATTACHMENTS];
	ralLoadOp_t     colorLoadOps   [RAL_MAX_COLOR_ATTACHMENTS];
	ralStoreOp_t    colorStoreOps  [RAL_MAX_COLOR_ATTACHMENTS];
	ralClearValue_t colorClears    [RAL_MAX_COLOR_ATTACHMENTS];
	uint32_t        numColorAttachments;
	ralTexture_t   *depthAttachment;         // NULL → no depth
	ralLoadOp_t     depthLoadOp;
	ralStoreOp_t    depthStoreOp;
	float           depthClear;
	ralTexture_t   *resolveAttachments[RAL_MAX_COLOR_ATTACHMENTS];  // MSAA resolve targets (NULL = none)
	ralRect_t       renderArea;
} ralRenderingInfo_t;

void Ral_BeginRendering( ralCommandBuffer_t *cb, const ralRenderingInfo_t *ri );
void Ral_EndRendering  ( ralCommandBuffer_t *cb );

// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit Cluster A — typed RAL cmd API surface.
//
// Each enum below uses the same numeric values as the matching Vulkan enum
// so the backend can cast directly (zero-cost), while the public API stays
// platform-neutral. The struct types (ralRect2D_t, ralBufferCopy_t,
// ralImageCopy_t, …) match the Vk layout field-for-field so a
// `*(const VkXxx *)( &ralXxx )` cast at the backend boundary is sound.
// ════════════════════════════════════════════════════════════════════════

typedef enum {
	RAL_BIND_POINT_GRAPHICS = 0,    // == VK_PIPELINE_BIND_POINT_GRAPHICS
	RAL_BIND_POINT_COMPUTE  = 1     // == VK_PIPELINE_BIND_POINT_COMPUTE
} ralBindPoint_t;

typedef enum {
	RAL_SUBPASS_CONTENTS_INLINE                    = 0,
	RAL_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = 1
} ralSubpassContents_t;

// Pipeline stage flag bits — same numeric values as VK_PIPELINE_STAGE_*_BIT.
// The renderer's parallel-paths era only uses a subset; the rest are reserved
// for future RAL clients.
typedef enum {
	RAL_PIPELINE_STAGE_TOP_OF_PIPE_BIT                    = 0x00000001,
	RAL_PIPELINE_STAGE_DRAW_INDIRECT_BIT                  = 0x00000002,
	RAL_PIPELINE_STAGE_VERTEX_INPUT_BIT                   = 0x00000004,
	RAL_PIPELINE_STAGE_VERTEX_SHADER_BIT                  = 0x00000008,
	RAL_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT    = 0x00000010,
	RAL_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
	RAL_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                = 0x00000040,
	RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                = 0x00000080,
	RAL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT           = 0x00000100,
	RAL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT            = 0x00000200,
	RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT        = 0x00000400,
	RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT                 = 0x00000800,
	RAL_PIPELINE_STAGE_TRANSFER_BIT                       = 0x00001000,
	RAL_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                 = 0x00002000,
	RAL_PIPELINE_STAGE_HOST_BIT                           = 0x00004000,
	RAL_PIPELINE_STAGE_ALL_GRAPHICS_BIT                   = 0x00008000,
	RAL_PIPELINE_STAGE_ALL_COMMANDS_BIT                   = 0x00010000
} ralPipelineStageFlagBits_t;
typedef uint32_t ralPipelineStageFlags_t;

// ralRect2D_t — alias for ralRect_t. Same memory layout as VkRect2D
// (int32_t x,y; uint32_t width,height — see ral_types.h:174-177), so the
// backend casts (const VkRect2D *)( &ralRect2D ) at the boundary.
typedef ralRect_t ralRect2D_t;

// Buffer ↔ buffer copy region. Same layout as VkBufferCopy.
// (existing ralBufferCopy_t in §3.7 below is the same — kept consistent.)

// Image subresource layers descriptor (alias-compatible with VkImageSubresourceLayers).
typedef struct {
	uint32_t aspectMask;        // RAL_TEXTURE_ASPECT_* equivalents == VK_IMAGE_ASPECT_*
	uint32_t mipLevel;
	uint32_t baseArrayLayer;
	uint32_t layerCount;
} ralImageSubresourceLayers_t;

// 3D offset / extent — Vk-layout compatible (matches VkOffset3D / VkExtent3D).
typedef struct { int32_t  x, y, z; }     ralOffset3D_t;
typedef struct { uint32_t width, height, depth; } ralExtentVk3D_t;

// Image ↔ image copy region. Same layout as VkImageCopy.
typedef struct {
	ralImageSubresourceLayers_t srcSubresource;
	ralOffset3D_t               srcOffset;
	ralImageSubresourceLayers_t dstSubresource;
	ralOffset3D_t               dstOffset;
	ralExtentVk3D_t             extent;
} ralImageCopy_t;

// Buffer → image (or image → buffer) copy region. Same layout as VkBufferImageCopy.
typedef struct {
	uint64_t                    bufferOffset;
	uint32_t                    bufferRowLength;
	uint32_t                    bufferImageHeight;
	ralImageSubresourceLayers_t imageSubresource;
	ralOffset3D_t               imageOffset;
	ralExtentVk3D_t             imageExtent;
} ralBufferImageCopy_t;

// Image blit region. Same layout as VkImageBlit (srcOffsets[2] + dstOffsets[2]).
typedef struct {
	ralImageSubresourceLayers_t srcSubresource;
	ralOffset3D_t               srcOffsets[2];
	ralImageSubresourceLayers_t dstSubresource;
	ralOffset3D_t               dstOffsets[2];
} ralImageBlit_t;

// Clear attachment / clear rect — Vk-layout-compatible.
typedef struct {
	uint32_t        aspectMask;        // == VkImageAspectFlags bits
	uint32_t        colorAttachment;
	ralClearValue_t clearValue;
} ralClearAttachment_t;

typedef struct {
	ralRect_t rect;
	uint32_t  baseArrayLayer;
	uint32_t  layerCount;
} ralClearRect_t;

// Memory barrier — Vk-layout-compatible with VkMemoryBarrier (sType+pNext
// stripped; the backend reconstructs them).
typedef struct {
	uint32_t srcAccessMask;
	uint32_t dstAccessMask;
} ralMemoryBarrier_t;

// Buffer memory barrier — Vk-layout-compatible.
typedef struct {
	uint32_t      srcAccessMask;
	uint32_t      dstAccessMask;
	uint32_t      srcQueueFamilyIndex;
	uint32_t      dstQueueFamilyIndex;
	ralBuffer_t  *buffer;
	uint64_t      offset;
	uint64_t      size;
} ralBufferMemoryBarrier_t;

// Image memory barrier — slightly Vk-layout-divergent because it carries a
// ralTexture_t pointer (the backend unwraps to VkImage) instead of the raw
// VkImage. The Vulkan backend has a thin scratch path that converts at the
// boundary.
typedef struct {
	uint32_t      srcAccessMask;
	uint32_t      dstAccessMask;
	uint32_t      oldLayout;             // == VkImageLayout
	uint32_t      newLayout;             // == VkImageLayout
	uint32_t      srcQueueFamilyIndex;
	uint32_t      dstQueueFamilyIndex;
	ralTexture_t *texture;               // ralTexture_t → VkImage at backend boundary
	uint32_t      aspectMask;            // == VkImageAspectFlags
	uint32_t      baseMipLevel;
	uint32_t      levelCount;
	uint32_t      baseArrayLayer;
	uint32_t      layerCount;
} ralImageMemoryBarrier_t;

// Pipeline barrier info — bundles every argument vkCmdPipelineBarrier needs.
typedef struct {
	ralPipelineStageFlags_t           srcStageMask;
	ralPipelineStageFlags_t           dstStageMask;
	uint32_t                          dependencyFlags;      // == VkDependencyFlags
	uint32_t                          memoryBarrierCount;
	const ralMemoryBarrier_t         *memoryBarriers;
	uint32_t                          bufferMemoryBarrierCount;
	const ralBufferMemoryBarrier_t   *bufferMemoryBarriers;
	uint32_t                          imageMemoryBarrierCount;
	const ralImageMemoryBarrier_t    *imageMemoryBarriers;
} ralPipelineBarrierInfo_t;

// ── bind / state ────────────────────────────────────────────────────────
typedef enum { RAL_INDEX_UINT16, RAL_INDEX_UINT32 } ralIndexType_t;

void Ral_CmdBindPipeline    ( ralCommandBuffer_t *cb, ralPipeline_t *p );
void Ral_CmdBindBindGroup   ( ralCommandBuffer_t *cb, uint32_t setIndex, ralBindGroup_t *g );

// Phase 7.4c-bindgroup — parallel-paths bind. Records vkCmdBindDescriptorSets
// (or backend equivalent) for `count` bind groups starting at `firstSet`,
// against an EXTERNALLY-supplied pipeline layout + cmd handle. Unlike
// Ral_CmdBindBindGroup (singular) this does not require a prior Ral_Cmd-
// BindPipeline — the caller passes layout + bindPoint explicitly. Used by
// the parallel-paths era where the legacy renderer drives both the cmd
// buffer and the pipeline layout; the RAL parallel call records the same
// bind onto the same cmd buffer (idempotent — re-binding the same set is
// a legal no-op behaviorally; CPU cost is the parallel-paths overhead
// until 7.4c-submit retires the legacy path).
//
// TODO_7.4c-cmd: `cmdHandle` is currently a raw VkCommandBuffer cast to
// void *, and `pipelineLayout` is a raw VkPipelineLayout cast to void *.
// 7.4c-cmd introduces ralCommandBuffer_t threading + ralPipelineLayout_t
// and tightens this signature. `bindPoint` is a VkPipelineBindPoint value
// (0 = GRAPHICS, 1 = COMPUTE) — same TODO_7.4c-cmd treatment.
void Ral_CmdBindBindGroups  ( ralBackend_t *b,
                              void *cmdHandle,                /* TODO_7.4c-cmd: VkCommandBuffer → ralCommandBuffer_t * */
                              int bindPoint,                  /* TODO_7.4c-cmd: VkPipelineBindPoint → ralBindPoint_t */
                              void *pipelineLayout,           /* TODO_7.4c-cmd: VkPipelineLayout → ralPipelineLayout_t * */
                              uint32_t firstSet,
                              uint32_t count,
                              ralBindGroup_t *const *bindGroups,
                              uint32_t dynamicOffsetCount,
                              const uint32_t *dynamicOffsets );
void Ral_CmdBindVertexBuffer( ralCommandBuffer_t *cb, uint32_t binding, ralBuffer_t *buf, uint64_t offset );
void Ral_CmdBindIndexBuffer ( ralCommandBuffer_t *cb, ralBuffer_t *buf, uint64_t offset, ralIndexType_t type );
void Ral_CmdSetViewport     ( ralCommandBuffer_t *cb, const ralViewport_t *vp );
void Ral_CmdSetScissor      ( ralCommandBuffer_t *cb, const ralRect_t *rect );
void Ral_CmdSetDepthBias    ( ralCommandBuffer_t *cb, float constant, float clamp, float slope );
void Ral_CmdPushConstants   ( ralCommandBuffer_t *cb, uint32_t stageFlags, uint32_t offset, uint32_t size, const void *data );

// ── draw / dispatch ─────────────────────────────────────────────────────
void Ral_CmdDraw                  ( ralCommandBuffer_t *cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance );
void Ral_CmdDrawIndexed           ( ralCommandBuffer_t *cb, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance );
void Ral_CmdDrawIndexedIndirect   ( ralCommandBuffer_t *cb, ralBuffer_t *argBuf, uint64_t offset, uint32_t drawCount, uint32_t stride );
void Ral_CmdDrawIndexedIndirectCount( ralCommandBuffer_t *cb, ralBuffer_t *argBuf, uint64_t argOffset,
                                      ralBuffer_t *countBuf, uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride );  // §9.3
void Ral_CmdDispatch              ( ralCommandBuffer_t *cb, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ );
void Ral_CmdDispatchIndirect      ( ralCommandBuffer_t *cb, ralBuffer_t *argBuf, uint64_t offset );

// ── transfers ───────────────────────────────────────────────────────────
typedef struct { uint64_t srcOffset, dstOffset, size; } ralBufferCopy_t;
typedef struct {
	uint64_t  bufferOffset;
	uint32_t  mipLevel;
	uint32_t  arrayLayer;
	ralRect_t imageRect;     // x/y/width/height of the destination texel region
} ralBufferTextureCopy_t;

void Ral_CmdCopyBuffer          ( ralCommandBuffer_t *cb, ralBuffer_t *src, ralBuffer_t *dst, const ralBufferCopy_t *region );
void Ral_CmdCopyBufferToTexture ( ralCommandBuffer_t *cb, ralBuffer_t *src, ralTexture_t *dst, const ralBufferTextureCopy_t *region );
// Readback path — caller must first transition `src` to TRANSFER_SRC_OPTIMAL
// via a barrier op (the RAL test does this directly today; the renderer
// migration in Phase 7.4 will route through the same coarse barriers).
void Ral_CmdCopyTextureToBuffer ( ralCommandBuffer_t *cb, ralTexture_t *src, ralBuffer_t *dst, const ralBufferTextureCopy_t *region );

// ── barriers ────────────────────────────────────────────────────────────
// v1 keeps barriers coarse — named transitions covering the renderer's and
// the GPU-driven path's needs (§9). Fine-grained per-resource barriers are a
// later refinement.
typedef enum {
	RAL_BARRIER_ALL,                    // full pipeline barrier
	RAL_BARRIER_COMPUTE_TO_GRAPHICS,    // SSBO/UAV written by compute, read by graphics
	RAL_BARRIER_GRAPHICS_TO_COMPUTE,
	RAL_BARRIER_TRANSFER_TO_GRAPHICS,   // upload finished, sampled by graphics
	RAL_BARRIER_INDIRECT                // buffer written by compute, consumed as indirect-draw args
} ralBarrierScope_t;

void Ral_CmdPipelineBarrier( ralCommandBuffer_t *cb, ralBarrierScope_t scope );

// ── GPU timestamps + debug labels (v1 primitives) ───────────────────────
void Ral_WriteTimestamp ( ralCommandBuffer_t *cb, ralQueryPool_t *pool, uint32_t query );
void Ral_BeginDebugLabel( ralCommandBuffer_t *cb, const char *label, const float color[4] );  // color NULL → default
void Ral_EndDebugLabel  ( ralCommandBuffer_t *cb );

// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A2 — typed RAL cmd surface (additions).
//
// These typed entry points cover the call sites that the 7.4c-cmd `_Raw`
// shims handled with raw Vk handles. Renderer migration: each _Raw caller
// finds the matching typed wrapper from this surface and passes RAL-typed
// arguments looked up via vk_ral_lookup_buffer / vk_ral_lookup_pipeline /
// vk_ral_lookup_pipeline_layout / vk_ral_lookup_bindgroup. NULL-fallthrough:
// each typed body null-guards every typed-pointer arg, so a missing-wrapper
// lookup cleanly skips the underlying vkCmd* call (parallel-paths-era
// invariant — legacy qvkCmd* stays authoritative; the parallel buffer
// records only what's adopted).
//
// Functions whose name collides with an already-typed entry point above
// (Ral_CmdBindPipeline, Ral_CmdSetViewport, Ral_CmdSetScissor,
// Ral_CmdSetDepthBias, Ral_CmdDraw, Ral_CmdDrawIndexed, Ral_CmdDispatch,
// Ral_CmdCopyBuffer, Ral_CmdBindIndexBuffer) are NOT redeclared here — the
// renderer call sites use the existing typed signatures directly.
// Distinct-name additions: PushConstants takes an explicit ralPipelineLayout_t
// (existing Ral_CmdPushConstants uses cb->currentLayout from Ral_CmdBindPipeline,
// which the renderer's parallel-paths buffer doesn't necessarily go through);
// PipelineBarrierFull takes the full Vk-style barrier info struct (existing
// Ral_CmdPipelineBarrier is coarse RAL_BARRIER_* scope enum only);
// BindVertexBuffers takes an array (existing singular is one-binding only).

// Bind vertex-buffer array (plural — multi-binding case). Renderer's vbo /
// tess paths bind up to 8 attribute streams at once.
void Ral_CmdBindVertexBuffers( ralCommandBuffer_t *cb, uint32_t firstBinding,
                               uint32_t bindingCount,
                               ralBuffer_t *const *buffers,
                               const uint64_t *offsets );

// Push constants with explicit layout (renderer parallel-paths path — the
// parallel cmd buffer doesn't track cb->currentLayout via Ral_CmdBindPipeline
// because legacy qvkCmdBindPipeline + RAL parallel pipeline bind take
// separate paths).
void Ral_CmdPushConstantsLayout( ralCommandBuffer_t *cb,
                                 ralPipelineLayout_t *layout,
                                 uint32_t stageFlags,        // RAL_STAGE_* bitmask
                                 uint32_t offset,
                                 uint32_t size,
                                 const void *data );

// Legacy VkRenderPass / VkFramebuffer based render pass (the renderer's
// existing render passes are not on dynamic rendering yet — that's a
// post-7.4d migration; this surface bridges the legacy-pass model into the
// typed cmd buffer).
void Ral_CmdBeginRenderPass( ralCommandBuffer_t *cb,
                             ralRenderPass_t *renderPass,
                             ralFramebuffer_t *framebuffer,
                             const ralRect_t *renderArea,
                             uint32_t clearValueCount,
                             const ralClearValue_t *clearValues,
                             ralSubpassContents_t contents );
void Ral_CmdEndRenderPass  ( ralCommandBuffer_t *cb );
void Ral_CmdNextSubpass    ( ralCommandBuffer_t *cb, ralSubpassContents_t contents );

// Full pipeline barrier (Vk-style — granular per-resource barriers, not the
// coarse RAL_BARRIER_* scope of Ral_CmdPipelineBarrier).
void Ral_CmdPipelineBarrierFull( ralCommandBuffer_t *cb,
                                 const ralPipelineBarrierInfo_t *info );

// Image-to-image / blit / buffer-to-image multi-region transfers.
void Ral_CmdCopyImage         ( ralCommandBuffer_t *cb, ralTexture_t *src, ralTexture_t *dst,
                                uint32_t regionCount, const ralImageCopy_t *regions );
void Ral_CmdCopyBufferToImage ( ralCommandBuffer_t *cb, ralBuffer_t *src, ralTexture_t *dst,
                                uint32_t regionCount, const ralBufferImageCopy_t *regions );
void Ral_CmdBlitImage         ( ralCommandBuffer_t *cb, ralTexture_t *src, ralTexture_t *dst,
                                uint32_t regionCount, const ralImageBlit_t *regions,
                                ralFilter_t filter );

// Clear attachments mid-render-pass.
void Ral_CmdClearAttachments( ralCommandBuffer_t *cb, uint32_t attachmentCount,
                              const ralClearAttachment_t *attachments,
                              uint32_t rectCount, const ralClearRect_t *rects );

// Cmd-buffer-side query-pool reset (host-side reset is Ral_ResetQueryPool).
void Ral_CmdResetQueryPool( ralCommandBuffer_t *cb, ralQueryPool_t *pool,
                            uint32_t firstQuery, uint32_t queryCount );

// Cmd-buffer write-timestamp (vs host-side Ral_WriteTimestamp / pool reset).
// Different name from Ral_WriteTimestamp to avoid collision.
void Ral_CmdWriteTimestamp( ralCommandBuffer_t *cb, uint32_t pipelineStageBits,
                            ralQueryPool_t *pool, uint32_t query );

// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A4 — parallel-paths void-handle shim retirement complete.
//
// The four pre-A4 void*-handle parallel-paths cmd forwarders (PipelineBarrier,
// CopyImage, ResetQueryPool, WriteTimestamp) have been retired; their renderer
// callsites migrated to the typed Ral_Cmd{PipelineBarrierFull,CopyImage,
// ResetQueryPool,WriteTimestamp} surface above via
// vk_ral_lookup_texture / vk_ral_lookup_query_pool reverse-lookups.
// The remaining parallel-paths-era void*-handle entry point is
// Ral_CmdBindBindGroups (lines 272-280 above) — its typed-handle migration
// (ralCommandBuffer_t / ralPipelineLayout_t) is a TODO_7.4d-map-arena
// follow-up that requires per-frame rotating-set adoption.

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_COMMAND_H
