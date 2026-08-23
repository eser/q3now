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
#include "ral_command_lifecycle.h" // generation-bound recording/submission receipts
#include "ral_resource.h"   // ralFilter_t, ralPipelineLayout_t-forward references for the typed cmd surface
#include "ral_transition.h" // semantic resource-state transitions; native lowering stays in each backend

#ifdef __cplusplus
extern "C" {
#endif

// ── per-frame lifecycle ─────────────────────────────────────────────────
// Ral_BeginFrame advances the frame counter, waits the frame fence being
// reused, drains the deferred-destroy queue (resources whose destroy frame is
// at least MAX_FRAMES_IN_FLIGHT behind), and resets that fence. Ral_EndFrame
// closes the frame (signals the frame fence). Consumers that don't run a
// per-frame render loop may skip these; the resource
// layer falls back to wait-idle-on-shutdown either way.
void Ral_BeginFrame( ralBackend_t *b );
void Ral_EndFrame  ( ralBackend_t *b );

// Ral_DrainDeferred — advance the deferred-destroy frame counter and reclaim
// every resource queued for destroy that is now safely past the frames-in-
// flight window, WITHOUT touching RAL's internal frame fences or issuing the
// empty stand-in submit that Ral_BeginFrame/Ral_EndFrame do. For consumers
// that run their own render loop (the renderer advances vk.frame_count and
// owns its own per-frame fence wait) and so cannot use Ral_BeginFrame's
// fence/submit cycle, but still need the deferred-destroy ring to drain on a
// real per-frame cadence instead of only on the 4096-entry overflow. Call it
// once per rendered frame at the renderer's frame boundary, AFTER the host has
// already waited the frame fence guaranteeing the work that referenced those
// resources is complete. NULL-safe / no-op until the frame layer is up.
void Ral_DrainDeferred( ralBackend_t *b );

// Ral_WaitIdleAndDrainDeferred — synchronously wait for every queue on the
// backend device to become idle, then reclaim the complete deferred-destroy
// queue before returning.  This is deliberately stronger than the normal
// per-frame drain above: use it only at a topology boundary where native
// parent objects are about to be destroyed and every deferred RAL child must
// already be gone (for example, attachment-generation replacement).  The
// queue is left untouched if the device-idle wait fails.
ralResult_t Ral_WaitIdleAndDrainDeferred( ralBackend_t *b );

// ── command buffer lifecycle ────────────────────────────────────────────
ralCommandBuffer_t *Ral_AcquireCommandBuffer ( ralBackend_t *b, ralQueueType_t q );
ralResult_t          Ral_BeginCommandBufferExact( ralCommandBuffer_t *cb,
	                                              ralCommandReceipt_t *outRecording );
ralResult_t          Ral_EndCommandBufferExact( ralCommandBuffer_t *cb,
	                                            const ralCommandReceipt_t *recording,
	                                            ralCommandReceipt_t *outExecutable );
ralResult_t          Ral_GetCommandBufferReceipt( const ralCommandBuffer_t *cb,
	                                              ralCommandReceipt_t *outReceipt );
ralResult_t          Ral_CancelCommandBuffer( ralCommandBuffer_t *cb,
	                                          const ralCommandReceipt_t *authority );
// Recycle one completed submitted generation back to IDLE. The caller must
// first prove completion of the submission carrying `submitted` (for example,
// an exact fence wait). This shape maps to vkResetCommandBuffer on Vulkan and
// dropping the finished command buffer/encoder generation on WebGPU.
ralResult_t          Ral_RecycleCommandBufferExact( ralCommandBuffer_t *cb,
	                                                 const ralCommandReceipt_t *submitted );
void                Ral_BeginCommandBuffer   ( ralCommandBuffer_t *cb );
void                Ral_EndCommandBuffer     ( ralCommandBuffer_t *cb );
void                Ral_DestroyCommandBuffer ( ralCommandBuffer_t *cb );    // usually superseded by Ral_PoolReset
void                Ral_PoolReset            ( ralBackend_t *b, ralQueueType_t q );   // vkResetCommandPool for the queue's pool

// Combined Acquire+Begin helper for
// one-shot command buffers. Acquires a fresh VkCommandBuffer from RAL's pool
// for the given queue, immediately begins it in
// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT mode, and returns the wrapper
// in RECORDING state. Caller must call Ral_EndCommandBuffer before submission
// and Ral_SubmitAndDispose (or manual Submit + Ral_DestroyCommandBuffer) when
// finished. ownsBuffer=qtrue → underlying VkCommandBuffer returned to pool on
// Destroy.
//
// Used by:
//  - vk_begin_frame's per-frame parallel-paths buffer (vk.c:~19321
//    with RAL_QUEUE_GRAPHICS).
//  - The 13 one-shot staging / screenshot / shadow-caster sites migrated
//    from the retired legacy Vk one-shot helpers (all
//    RAL_QUEUE_GRAPHICS to preserve the legacy graphics-queue serialization
//    with rendering work; RAL_QUEUE_TRANSFER promotion is a later scope).
//
// Renamed to reflect actual Acquire+Begin
// semantics. The prior "Adopt"-named identifier was a leftover from an
// earlier parallel-paths model; the retired Ral_WrapCommandBuffer
// sibling has been removed by cleanup-final.
ralCommandBuffer_t *Ral_AcquireBegunCommandBuffer( ralBackend_t *b, ralQueueType_t q );

// End + submit + queue-idle + free in a single
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

// Checked submission. The command buffers enter SUBMITTED state only after
// the backend queue call succeeds. Callers must not wait on signal objects or
// present a frame when this returns anything other than ralSuccess.
ralResult_t Ral_Submit( ralBackend_t *b, ralQueueType_t q, const ralSubmitInfo_t *si );
ralResult_t Ral_SubmitExact( ralBackend_t *b, ralQueueType_t q,
	                         const ralSubmitInfo_t *si,
	                         const ralCommandReceipt_t *executableReceipts,
	                         ralSubmissionReceipt_t *outReceipt );

// Host-side wait until all work
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
	// Array layer to render into when depthAttachment is an array image (adopted
	// via Ral_AdoptArrayTexture, which carries per-layer views); 0 for single-layer
	// (the default). VkRenderingInfo has no baseArrayLayer — the layer offset lives
	// entirely in the bound attachment imageView's subresourceRange, so the backend
	// selects depthAttachment->layerViews[depthAttachmentLayerIndex] as the depth
	// imageView. Zero-init (every current single-layer caller) → layerViews unused,
	// defaultView bound, byte-identical to before.
	uint32_t        depthAttachmentLayerIndex;
	// Stencil: the stencil attachment is the depthAttachment itself (combined
	// depth+stencil format → one image/view). A stencil attachment is bound only
	// when depthAttachment's format carries a stencil aspect; these fields are
	// ignored otherwise. Zero-init (color-only / depth-only callers) → no stencil.
	ralLoadOp_t     stencilLoadOp;
	ralStoreOp_t    stencilStoreOp;
	uint32_t        stencilClear;
	ralTexture_t   *resolveAttachments[RAL_MAX_COLOR_ATTACHMENTS];  // MSAA resolve targets (NULL = none)
	ralRect_t       renderArea;
	// Optional semantic GPU pass label. When the backend exposes debug-utils,
	// Ral_BeginRendering/Ral_EndRendering wrap this dynamic-rendering scope in
	// one balanced debug-label pair. NULL keeps the path byte-for-byte inert.
	const char     *debugName;
} ralRenderingInfo_t;

void Ral_BeginRendering( ralCommandBuffer_t *cb, const ralRenderingInfo_t *ri );
void Ral_EndRendering  ( ralCommandBuffer_t *cb );

// ════════════════════════════════════════════════════════════════════════
// Typed RAL cmd API surface.
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

// Image blit region. Same layout as VkImageBlit (srcOffsets[2] + dstOffsets[2]).
typedef struct {
	ralImageSubresourceLayers_t srcSubresource;
	ralOffset3D_t               srcOffsets[2];
	ralImageSubresourceLayers_t dstSubresource;
	ralOffset3D_t               dstOffsets[2];
} ralImageBlit_t;

// Clear attachment / clear rect. Aspect bits are portable RAL_TEXTURE_ASPECT_*
// values; each backend performs its own native lowering.
typedef struct {
	ralTextureAspectFlags_t aspectMask;
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
// Binds one group with the exact dynamic-buffer offset vector declared by its
// layout. Offsets are uint32/WebGPU-shaped and ordered by ascending binding.
// Returns qfalse without emitting a backend command when count, alignment,
// registered range, backend ownership or current-pipeline authority is wrong.
qboolean Ral_CmdBindBindGroupDynamic( ralCommandBuffer_t *cb, uint32_t setIndex,
	ralBindGroup_t *g, const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount );
// Exact migration form. In addition to the dynamic-buffer checks above, this
// requires authoritative set-layout metadata on the currently bound pipeline
// and rejects a group whose layout is not the pipeline's layout at setIndex.
qboolean Ral_CmdBindBindGroupDynamicExact( ralCommandBuffer_t *cb, uint32_t setIndex,
	ralBindGroup_t *g, const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount );
// Non-emitting counterpart used by pass-boundary transactions. It validates
// the same exact layout/backend/lifecycle/dynamic-range authority against an
// explicit target pipeline without changing command state or recording a
// backend command. A later exact bind is therefore infallible provided the
// validated cohort remains alive and unchanged.
qboolean Ral_ValidateBindGroupDynamicExact( const ralCommandBuffer_t *cb,
	const ralPipeline_t *pipeline, uint32_t setIndex, const ralBindGroup_t *g,
	const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount );

void Ral_CmdBindVertexBuffer( ralCommandBuffer_t *cb, uint32_t binding, ralBuffer_t *buf, uint64_t offset );
void Ral_CmdBindIndexBuffer ( ralCommandBuffer_t *cb, ralBuffer_t *buf, uint64_t offset, ralIndexType_t type );
// Exact, WebGPU-shaped buffer-binding authority. These variants reject an
// invalid backend/usage/range/alignment/map/lifecycle cohort before recording
// or changing the command buffer's tracked bindings.
qboolean Ral_CmdBindVertexBufferExact( ralCommandBuffer_t *cb, uint32_t binding,
	ralBuffer_t *buf, uint64_t offset );
qboolean Ral_CmdBindVertexBuffersExact( ralCommandBuffer_t *cb,
	uint32_t firstBinding, uint32_t bindingCount,
	ralBuffer_t *const *buffers, const uint64_t *offsets );
qboolean Ral_CmdBindIndexBufferExact( ralCommandBuffer_t *cb, ralBuffer_t *buf,
	uint64_t offset, ralIndexType_t type );
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
	// Zero preserves the legacy tightly-packed Vulkan upload path. Portable
	// readbacks provide an explicit texel-row pitch; Vulkan lowers it to
	// bufferRowLength/bufferImageHeight and WebGPU consumes it directly.
	uint32_t  bytesPerRow;
	uint32_t  rowsPerImage;
	uint32_t  mipLevel;
	uint32_t  arrayLayer;
	// Portable texture plane selection, matching GPUImageCopyTexture.aspect.
	// Zero means "all available aspects" and is valid only when that resolves
	// to one copyable plane (ordinary color/depth-only resources). Combined
	// depth-stencil resources must select DEPTH or STENCIL explicitly.
	ralTextureAspectFlags_t aspects;
	ralRect_t imageRect;     // x/y/width/height of the destination texel region
	// Appended zero-default fields preserve existing initializers while mapping
	// the complete GPUImageCopyTexture / GPUExtent3D vocabulary. A zero count or
	// depth means one. Non-3D textures use array layers and require z=0/depth=1;
	// 3D textures use z/depth and require arrayLayer=0/arrayLayerCount=1.
	uint32_t  arrayLayerCount;
	uint32_t  imageZ;
	uint32_t  imageDepth;
} ralBufferTextureCopy_t;

// Exact form returns whether one copy command was emitted. It validates the
// portable COPY_SRC/COPY_DST capabilities, bounds, non-overlap for same-buffer
// copies, backend cohort, map state and command recording lifecycle first.
qboolean Ral_CmdCopyBufferExact( ralCommandBuffer_t *cb, ralBuffer_t *src,
	                          ralBuffer_t *dst, const ralBufferCopy_t *region );
void Ral_CmdCopyBuffer          ( ralCommandBuffer_t *cb, ralBuffer_t *src, ralBuffer_t *dst, const ralBufferCopy_t *region );
// Validate the complete batch before emitting one native copy command. This
// is the portable upload primitive: regions express texel/block layout rather
// than VkBufferImageCopy, and zero row pitches select backend-managed tightly
// packed input. No region is emitted when any member is invalid.
qboolean Ral_CmdCopyBufferToTextureRegionsExact( ralCommandBuffer_t *cb,
											  ralBuffer_t *src, ralTexture_t *dst,
											  uint32_t regionCount,
											  const ralBufferTextureCopy_t *regions );
void Ral_CmdCopyBufferToTexture ( ralCommandBuffer_t *cb, ralBuffer_t *src, ralTexture_t *dst, const ralBufferTextureCopy_t *region );
// Readback path — caller must first transition `src` to TRANSFER_SRC_OPTIMAL
// via a barrier op (the RAL test does this directly today; the renderer
// migration will route through the same coarse barriers).
qboolean Ral_CmdCopyTextureToBuffer( ralCommandBuffer_t *cb,
	ralTexture_t *src, ralBuffer_t *dst,
	const ralBufferTextureCopy_t *region );
// Zero one aligned range of a STORAGE|TRANSFER_DST buffer and publish the
// result for immediate compute storage read/write. This deliberately exposes
// no arbitrary fill pattern: WebGPU's portable counterpart is
// GPUCommandEncoder.clearBuffer, which is zero-only.
qboolean Ral_CmdClearStorageBuffer( ralCommandBuffer_t *cb,
	ralBuffer_t *buffer, uint64_t offset, uint64_t size );

// ── barriers ────────────────────────────────────────────────────────────
// v1 keeps barriers coarse — named transitions covering the renderer's and
// the GPU-driven path's needs (§9). Fine-grained per-resource barriers are a
// later refinement.
typedef enum {
	RAL_BARRIER_ALL,                    // full pipeline barrier
	RAL_BARRIER_COMPUTE_TO_GRAPHICS,    // SSBO/UAV written by compute, read by graphics
	RAL_BARRIER_COMPUTE_TO_COMPUTE,     // SSBO/UAV written by compute, read/written by compute
	RAL_BARRIER_COMPUTE_TO_TRANSFER,    // SSBO/UAV written by compute, copied by transfer
	RAL_BARRIER_GRAPHICS_TO_COMPUTE,
	RAL_BARRIER_TRANSFER_TO_GRAPHICS,   // upload finished, sampled by graphics
	RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT, // attachment write visible to fragment sampling
	RAL_BARRIER_INDIRECT                // buffer written by compute, consumed as indirect-draw args
} ralBarrierScope_t;

void Ral_CmdPipelineBarrier( ralCommandBuffer_t *cb, ralBarrierScope_t scope );

// Records one output-atomic batch of semantic resource transitions. This
// initial portable command supports whole-resource ranges whose logical source
// and destination queues map to one physical queue. Vulkan emits explicit
// barriers; WebGPU can implement the same contract as pass-boundary validation
// plus implicit usage transitions on its single GPU queue. Distinct physical
// queues use the paired generation-bound release/acquire surface below.
ralResult_t Ral_CmdTransitionResources( ralCommandBuffer_t *cb,
	                                     const ralResourceTransitionBatch_t *batch );
ralResult_t Ral_CmdReleaseBufferOwnership( ralCommandBuffer_t *cb,
	const ralBufferTransition_t *transition,
	ralQueueTransferReceipt_t *outReceipt );
ralResult_t Ral_CmdAcquireBufferOwnership( ralCommandBuffer_t *cb,
	const ralBufferTransition_t *transition,
	const ralQueueTransferReceipt_t *receipt );
ralResult_t Ral_CancelBufferOwnershipTransfer( ralBuffer_t *buffer,
	const ralQueueTransferReceipt_t *receipt );
ralResult_t Ral_CmdReleaseTextureOwnership( ralCommandBuffer_t *cb,
	const ralTextureTransition_t *transition,
	ralQueueTransferReceipt_t *outReceipt );
ralResult_t Ral_CmdAcquireTextureOwnership( ralCommandBuffer_t *cb,
	const ralTextureTransition_t *transition,
	const ralQueueTransferReceipt_t *receipt );
ralResult_t Ral_CancelTextureOwnershipTransfer( ralTexture_t *texture,
	const ralQueueTransferReceipt_t *receipt );

// ── GPU timestamps + debug labels (v1 primitives) ───────────────────────
void Ral_WriteTimestamp ( ralCommandBuffer_t *cb, ralQueryPool_t *pool, uint32_t query );
// Return qtrue only when the backend command was actually emitted. This lets
// diagnostics distinguish a requested label from a real GPU-capture marker.
qboolean Ral_BeginDebugLabel( ralCommandBuffer_t *cb, const char *label, const float color[4] );  // color NULL → default
qboolean Ral_EndDebugLabel  ( ralCommandBuffer_t *cb );

typedef struct {
	qboolean supported;
	qboolean renderingLabelActive;
	uint32_t beginCount;
	uint32_t endCount;
} ralDebugLabelStats_t;

// Per-recording command-buffer receipt. Counts reset in Ral_BeginCommandBuffer.
qboolean Ral_GetDebugLabelStats( const ralCommandBuffer_t *cb, ralDebugLabelStats_t *out );
void Ral_ResetDebugLabelStats( ralCommandBuffer_t *cb );

// ════════════════════════════════════════════════════════════════════════
// Typed RAL cmd surface (additions).
//
// These typed entry points cover the call sites that the earlier `_Raw`
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

// Push constants with an explicit layout. External/adopted layouts must first
// publish their exact portable stage/range authority through the backend
// migration bridge; the command then rejects lifecycle, backend, alignment,
// stage and range drift before emission. stageFlags use RAL_STAGE_* on every
// backend — never native Vulkan stage bits.
qboolean Ral_CmdPushConstantsLayoutExact( ralCommandBuffer_t *cb,
                                 ralPipelineLayout_t *layout,
                                 uint32_t stageFlags,
                                 uint32_t offset,
                                 uint32_t size,
                                 const void *data );
void Ral_CmdPushConstantsLayout( ralCommandBuffer_t *cb,
                                 ralPipelineLayout_t *layout,
                                 uint32_t stageFlags,
                                 uint32_t offset,
                                 uint32_t size,
                                 const void *data );

// Full pipeline barrier (Vk-style — granular per-resource barriers, not the
// coarse RAL_BARRIER_* scope of Ral_CmdPipelineBarrier).
void Ral_CmdPipelineBarrierFull( ralCommandBuffer_t *cb,
                                 const ralPipelineBarrierInfo_t *info );

// Transition a single texture between layouts AND record the matching image
// barrier in one call: it both issues the VkImageMemoryBarrier (src/dst stage
// from the args; access masks derived from the stages) AND updates the
// texture's tracked currentLayout — so layout state and the GPU barrier can
// never desync (unlike the state-only Ral_SetTextureLayout, which records no
// barrier). Used e.g. to make a render target readable by a later compute pass.
void Ral_CmdTransitionTexture( ralCommandBuffer_t *cb, ralTexture_t *tex,
                               ralPipelineStageFlags_t srcStage,
                               ralPipelineStageFlags_t dstStage,
                               uint32_t newVkLayout );

// Transition one canonical swapchain-image wrapper to the backend's present
// layout.  The typed index is validated against the live swapchain, and the
// tracked texture layout is updated atomically with the recorded barrier.
ralResult_t Ral_PrepareSwapchainImageForPresent( ralCommandBuffer_t *cb,
	                                             ralSwapchain_t *swapchain,
	                                             uint32_t imageIndex );

// Image-to-image / blit transfers. Buffer-to-texture copies use the portable
// ralBufferTextureCopy_t exact batch above, never a Vk-layout alias.
void Ral_CmdCopyImage         ( ralCommandBuffer_t *cb, ralTexture_t *src, ralTexture_t *dst,
                                uint32_t regionCount, const ralImageCopy_t *regions );
void Ral_CmdBlitImage         ( ralCommandBuffer_t *cb, ralTexture_t *src, ralTexture_t *dst,
                                uint32_t regionCount, const ralImageBlit_t *regions,
                                ralFilter_t filter );

// Clear attachments mid-render-pass. The exact form validates the complete
// command before emission; the compatibility wrapper intentionally discards
// that result.
qboolean Ral_CmdClearAttachmentsExact( ralCommandBuffer_t *cb,
	uint32_t attachmentCount, const ralClearAttachment_t *attachments,
	uint32_t rectCount, const ralClearRect_t *rects );
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
// Parallel-paths void-handle shim retirement complete.
//
// The four earlier void*-handle parallel-paths cmd forwarders (PipelineBarrier,
// CopyImage, ResetQueryPool, WriteTimestamp) have been retired; their renderer
// callsites migrated to the typed Ral_Cmd{PipelineBarrierFull,CopyImage,
// ResetQueryPool,WriteTimestamp} surface above. Query pools are now created
// natively by RAL; texture adoption remains a Vulkan-migration detail.
// No raw void*-handle command forwarder remains in the public command API.

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_COMMAND_H
