// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_command.c — Vulkan backend: per-queue command pools + command
// buffers, multi-queue submission (vkQueueSubmit2), the trivially-backend
// pass-through command ops (copy / barrier / timestamp / debug label /
// viewport / scissor / depth-bias), plus the pipeline-dependent cmd ops
// (bind / draw / dispatch / dynamic rendering). See
// docs/phase-7-ral-design.md §3.7, §6.2, §9.3, §10.

#include "ral_vulkan_internal.h"

R_LOG_DECLARE_CHANNEL( rch_ral, "renderer.ral" );

#define RAL_VK_MAX_SUBMIT_CBS   16u
#define RAL_VK_MAX_SUBMIT_SEMS  16u

// ════════════════════════════════════════════════════════════════════════
// queue submission helper (mutex-guarded; vkQueueSubmit2 is not thread-safe)
// ════════════════════════════════════════════════════════════════════════
void ralVk_QueueSubmit2( ralBackend_t *b, ralQueueType_t q, const VkSubmitInfo2 *si2, VkFence fence ) {
	VkResult r;
	ralVk_QueueLock( b, q );
	r = b->vk.QueueSubmit2( b->queues[q], 1, si2, fence );
	ralVk_QueueUnlock( b, q );
	if ( r != VK_SUCCESS )
		R_LOG( rch_ral, SEV_WARN, "vkQueueSubmit2 (queue %d) failed (VkResult %d)\n", (int)q, (int)r );
}

// ════════════════════════════════════════════════════════════════════════
// Ral_WaitQueueIdle.
// Host-side wait on a specific queue's pending work. Replaces the legacy
// qvkQueueWaitIdle(vk.queue) call inside vk_queue_wait_idle (B1 fold —
// retarget done at vk.c:13753-13757). Lock the queue mutex during the
// wait (mirrors ralVk_QueueSubmit2's serialization — the QueueWaitIdle
// call is not thread-safe with respect to other queue ops).
// ════════════════════════════════════════════════════════════════════════
ralResult_t Ral_WaitQueueIdle( ralBackend_t *b, ralQueueType_t q ) {
	VkResult r;
	if ( !b || (uint32_t)q > RAL_QUEUE_TRANSFER ) return ralErrorInvalidArgument;
	ralVk_QueueLock( b, q );
	r = b->vk.QueueWaitIdle( b->queues[q] );
	ralVk_QueueUnlock( b, q );
	if ( r == VK_SUCCESS ) return ralSuccess;
	if ( r == VK_ERROR_DEVICE_LOST ) return ralErrorDeviceLost;
	return ralErrorUnknown;
}

// ════════════════════════════════════════════════════════════════════════
// command pool + command buffer
// ════════════════════════════════════════════════════════════════════════
ralCommandBuffer_t *Ral_AcquireCommandBuffer( ralBackend_t *b, ralQueueType_t q ) {
	VkCommandBufferAllocateInfo ai;
	VkCommandBuffer             vcb = VK_NULL_HANDLE;
	ralCommandBuffer_t         *cb;
	if ( !b || (uint32_t)q > RAL_QUEUE_TRANSFER ) return NULL;
	RAL_ZERO( ai );
	ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool        = b->cmdPools[q];
	ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;
	ralVk_QueueLock( b, q );
	if ( b->vk.AllocateCommandBuffers( b->device, &ai, &vcb ) != VK_SUCCESS ) vcb = VK_NULL_HANDLE;
	ralVk_QueueUnlock( b, q );
	if ( vcb == VK_NULL_HANDLE ) { R_LOG( rch_ral, SEV_WARN, "Ral_AcquireCommandBuffer: vkAllocateCommandBuffers failed (queue %d)\n", (int)q ); return NULL; }
	cb = (ralCommandBuffer_t *)malloc( sizeof( *cb ) );
	if ( !cb ) { ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, &vcb ); ralVk_QueueUnlock( b, q ); return NULL; }
	RAL_ZERO( *cb );
	cb->backend = b; cb->cb = vcb; cb->queue = q; cb->state = RAL_VK_CMD_IDLE; cb->frame = b->currentFrame;
	cb->ownsBuffer = qtrue;   // RAL-allocated; matching Free in Ral_DestroyCommandBuffer
	return cb;
}

// Combined Acquire+Begin helper.
// Allocates a fresh VkCommandBuffer from the backend's pool for the requested
// queue and immediately enters RECORDING state via vkBeginCommandBuffer.
// ownsBuffer=qtrue (inherited from Ral_AcquireCommandBuffer) so the matching
// Ral_SubmitAndDispose / Ral_DestroyCommandBuffer frees the buffer back to
// the pool.
//
// Used by:
//  - vk_begin_frame's per-frame parallel-paths buffer (GRAPHICS).
//  - The 13 one-shot staging / screenshot / shadow-caster sites migrated
//    from the retired legacy Vk one-shot helpers (all
//    GRAPHICS to preserve legacy queue serialization).
//
// The historical "adopt the renderer's VkCommandBuffer" reading is gone —
// it was VUID-unsafe for state-transitioning ops (parallel buffer state
// would conflict with the renderer's own buffer). Naming kept "Adopt" for
// this turn; the D-shim turn renames it.
ralCommandBuffer_t *Ral_AcquireBegunCommandBuffer( ralBackend_t *b, ralQueueType_t q ) {
	ralCommandBuffer_t *cb;
	if ( !b ) return NULL;
	cb = Ral_AcquireCommandBuffer( b, q );
	if ( cb == NULL ) return NULL;
	Ral_BeginCommandBuffer( cb );
	return cb;
}

// End + submit + wait + free in one shot.
// Replaces the retired legacy Vk one-shot helper's "submit to graphics
// queue + vkQueueWaitIdle + vkFreeCommandBuffers" sequence; the wait is
// fence-based via Ral_WaitFence (the established synchronous-completion
// idiom at ralVk_RunAsyncTest's "fence cycle" block above), not a raw
// QueueWaitIdle. Target queue is whichever cmd->queue was acquired on;
// for the 13 callsites that's RAL_QUEUE_GRAPHICS, matching the
// legacy graphics queue used by the prior retired pair.
void Ral_SubmitAndDispose( ralCommandBuffer_t *cb ) {
	ralBackend_t       *b;
	ralFence_t         *fence;
	ralCommandBuffer_t *cbs[ 1 ];
	ralSubmitInfo_t     si;
	if ( !cb ) return;
	b = cb->backend;
	Ral_EndCommandBuffer( cb );
	fence = Ral_CreateFence( b );
	RAL_ZERO( si );
	cbs[ 0 ]              = cb;
	si.commandBuffers     = cbs;
	si.numCommandBuffers  = 1;
	si.signalFence        = fence;
	Ral_Submit( b, cb->queue, &si );
	if ( fence ) {
		Ral_WaitFence   ( fence, ~( uint64_t )0 );
		Ral_DestroyFence( fence );
	}
	// ownsBuffer=qtrue → returns the VkCommandBuffer to the pool + frees wrapper.
	Ral_DestroyCommandBuffer( cb );
}

void Ral_BeginCommandBuffer( ralCommandBuffer_t *cb ) {
	VkCommandBufferBeginInfo bi;
	if ( !cb ) return;
	if ( cb->state != RAL_VK_CMD_IDLE ) { R_LOG( rch_ral, SEV_WARN, "Ral_BeginCommandBuffer: command buffer not IDLE (state %d)\n", (int)cb->state ); return; }
	RAL_ZERO( bi );
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ( cb->backend->vk.BeginCommandBuffer( cb->cb, &bi ) != VK_SUCCESS ) { R_LOG( rch_ral, SEV_WARN, "Ral_BeginCommandBuffer: vkBeginCommandBuffer failed\n" ); return; }
	cb->renderingDebugLabelActive = qfalse;
	cb->debugLabelBeginCount = 0;
	cb->debugLabelEndCount = 0;
	cb->state = RAL_VK_CMD_RECORDING;
}

void Ral_EndCommandBuffer( ralCommandBuffer_t *cb ) {
	if ( !cb ) return;
	if ( cb->state != RAL_VK_CMD_RECORDING ) { R_LOG( rch_ral, SEV_WARN, "Ral_EndCommandBuffer: command buffer not RECORDING (state %d)\n", (int)cb->state ); return; }
	if ( cb->backend->vk.EndCommandBuffer( cb->cb ) != VK_SUCCESS ) { R_LOG( rch_ral, SEV_WARN, "Ral_EndCommandBuffer: vkEndCommandBuffer failed\n" ); return; }
	cb->state = RAL_VK_CMD_PENDING_SUBMIT;
}

void *Ral_GetCommandBufferHandle( const ralCommandBuffer_t *cb ) {
	return cb ? (void *)cb->cb : NULL;
}

void Ral_DestroyCommandBuffer( ralCommandBuffer_t *cb ) {
	ralBackend_t *b;
	if ( !cb ) return;
	b = cb->backend;
	// Adopted wrappers (ownsBuffer = qfalse) skip the FreeCommandBuffers
	// call — the renderer's existing pool owns the underlying VkCommandBuffer lifetime.
	if ( cb->ownsBuffer ) {
		// Command buffers in flight are always fence/timeline-waited before
		// being freed by the current consumers; freed immediately (the deferred
		// path for cmd buffers lands with the renderer migration later).
		ralVk_QueueLock( b, cb->queue );
		b->vk.FreeCommandBuffers( b->device, b->cmdPools[cb->queue], 1, &cb->cb );
		ralVk_QueueUnlock( b, cb->queue );
	}
	free( cb );
}

void Ral_PoolReset( ralBackend_t *b, ralQueueType_t q ) {
	if ( !b || (uint32_t)q > RAL_QUEUE_TRANSFER ) return;
	// Resets the pool; outstanding ralCommandBuffer_t wrappers for this queue
	// become invalid (consumer must not reuse them). Wrapper bookkeeping is a
	// follow-up.
	ralVk_QueueLock( b, q );
	b->vk.ResetCommandPool( b->device, b->cmdPools[q], 0 );
	ralVk_QueueUnlock( b, q );
}

// ════════════════════════════════════════════════════════════════════════
// Ral_Submit — VkSubmitInfo2 with command-buffer + wait/signal semaphore arrays
// ════════════════════════════════════════════════════════════════════════
void Ral_Submit( ralBackend_t *b, ralQueueType_t q, const ralSubmitInfo_t *si ) {
	VkCommandBufferSubmitInfo cbis[ RAL_VK_MAX_SUBMIT_CBS ];
	VkSemaphoreSubmitInfo     waitInfos[ RAL_VK_MAX_SUBMIT_SEMS ], signalInfos[ RAL_VK_MAX_SUBMIT_SEMS ];
	VkSubmitInfo2             si2;
	uint32_t                  i, nCb, nWait, nSig;
	VkFence                   fence;
	if ( !b || !si || (uint32_t)q > RAL_QUEUE_TRANSFER ) return;

	// The per-submit fixed-capacity scratch arrays are intentional (16 is
	// generous for every renderer submission path). Refuse — rather than
	// silently record a truncated prefix — if a caller exceeds them: a partial
	// submit (dropped command buffers / un-waited or un-signalled semaphores)
	// would corrupt synchronization in a way that's far harder to diagnose than
	// a hard fail here. Same "no silent degrade" idiom as Ral_CmdDrawIndexedIndirectCount.
	if ( si->numCommandBuffers  > RAL_VK_MAX_SUBMIT_CBS  ||
	     si->numWaitSemaphores   > RAL_VK_MAX_SUBMIT_SEMS ||
	     si->numSignalSemaphores > RAL_VK_MAX_SUBMIT_SEMS ) {
		R_LOG( rch_ral, SEV_ERROR, "Ral_Submit: over-capacity (cbs %u/%u, wait %u/%u, signal %u/%u) — refusing submit (would truncate; raise RAL_VK_MAX_SUBMIT_* if this is legitimate)\n",
		        si->numCommandBuffers, RAL_VK_MAX_SUBMIT_CBS,
		        si->numWaitSemaphores, RAL_VK_MAX_SUBMIT_SEMS,
		        si->numSignalSemaphores, RAL_VK_MAX_SUBMIT_SEMS );
		return;
	}

	nCb = si->numCommandBuffers;
	for ( i = 0; i < nCb; i++ ) {
		ralCommandBuffer_t *cb = si->commandBuffers[i];
		RAL_ZERO( cbis[i] );
		cbis[i].sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cbis[i].commandBuffer = cb ? cb->cb : VK_NULL_HANDLE;
		if ( cb ) { cb->state = RAL_VK_CMD_SUBMITTED; cb->frame = b->currentFrame; }
	}
	nWait = si->numWaitSemaphores;
	for ( i = 0; i < nWait; i++ ) {
		ralSemaphore_t *s = si->waitSemaphores[i];
		RAL_ZERO( waitInfos[i] );
		waitInfos[i].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitInfos[i].semaphore = s ? s->sem : VK_NULL_HANDLE;
		waitInfos[i].value     = ( s && s->type == RAL_SEMAPHORE_TIMELINE && si->waitValues ) ? si->waitValues[i] : 0;
		waitInfos[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	nSig = si->numSignalSemaphores;
	for ( i = 0; i < nSig; i++ ) {
		ralSemaphore_t *s = si->signalSemaphores[i];
		RAL_ZERO( signalInfos[i] );
		signalInfos[i].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalInfos[i].semaphore = s ? s->sem : VK_NULL_HANDLE;
		signalInfos[i].value     = ( s && s->type == RAL_SEMAPHORE_TIMELINE && si->signalValues ) ? si->signalValues[i] : 0;
		signalInfos[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	fence = ( si->signalFence && si->signalFence->fence != VK_NULL_HANDLE ) ? si->signalFence->fence : VK_NULL_HANDLE;

	RAL_ZERO( si2 );
	si2.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	si2.waitSemaphoreInfoCount   = nWait;  si2.pWaitSemaphoreInfos   = nWait ? waitInfos   : NULL;
	si2.commandBufferInfoCount   = nCb;    si2.pCommandBufferInfos   = nCb   ? cbis        : NULL;
	si2.signalSemaphoreInfoCount = nSig;   si2.pSignalSemaphoreInfos = nSig  ? signalInfos : NULL;
	ralVk_QueueSubmit2( b, q, &si2, fence );
}

// ════════════════════════════════════════════════════════════════════════
// trivially-backend command ops
// ════════════════════════════════════════════════════════════════════════
void Ral_CmdCopyBuffer( ralCommandBuffer_t *cb, ralBuffer_t *src, ralBuffer_t *dst, const ralBufferCopy_t *region ) {
	VkBufferCopy r;
	if ( !cb || !src || !dst || !region ) return;
	RAL_ZERO( r ); r.srcOffset = region->srcOffset; r.dstOffset = region->dstOffset; r.size = region->size;
	cb->backend->vk.CmdCopyBuffer( cb->cb, src->buffer, dst->buffer, 1, &r );
}

void Ral_CmdCopyBufferToTexture( ralCommandBuffer_t *cb, ralBuffer_t *src, ralTexture_t *dst, const ralBufferTextureCopy_t *region ) {
	VkBufferImageCopy bic;
	if ( !cb || !src || !dst || !region ) return;
	RAL_ZERO( bic );
	bic.bufferOffset                    = region->bufferOffset;
	bic.imageSubresource.aspectMask     = dst->aspect;
	bic.imageSubresource.mipLevel       = region->mipLevel;
	bic.imageSubresource.baseArrayLayer = region->arrayLayer;
	bic.imageSubresource.layerCount     = 1;
	bic.imageOffset.x                   = region->imageRect.x;
	bic.imageOffset.y                   = region->imageRect.y;
	bic.imageExtent.width               = region->imageRect.width;
	bic.imageExtent.height              = region->imageRect.height;
	bic.imageExtent.depth               = 1;
	// consumer must have transitioned `dst` to TRANSFER_DST_OPTIMAL via Ral_CmdPipelineBarrier first
	cb->backend->vk.CmdCopyBufferToImage( cb->cb, src->buffer, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic );
}

// Mirror of Ral_CmdCopyBufferToTexture for readback (the early
// RAL test went through the raw VK PFN; with this in place renderer
// readbacks / screenshot paths can stay on the RAL surface).
void Ral_CmdCopyTextureToBuffer( ralCommandBuffer_t *cb, ralTexture_t *src, ralBuffer_t *dst, const ralBufferTextureCopy_t *region ) {
	VkBufferImageCopy bic;
	if ( !cb || !src || !dst || !region ) return;
	RAL_ZERO( bic );
	bic.bufferOffset                    = region->bufferOffset;
	bic.imageSubresource.aspectMask     = src->aspect;
	bic.imageSubresource.mipLevel       = region->mipLevel;
	bic.imageSubresource.baseArrayLayer = region->arrayLayer;
	bic.imageSubresource.layerCount     = 1;
	bic.imageOffset.x                   = region->imageRect.x;
	bic.imageOffset.y                   = region->imageRect.y;
	bic.imageExtent.width               = region->imageRect.width;
	bic.imageExtent.height              = region->imageRect.height;
	bic.imageExtent.depth               = 1;
	// consumer must have transitioned `src` to TRANSFER_SRC_OPTIMAL first
	cb->backend->vk.CmdCopyImageToBuffer( cb->cb, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->buffer, 1, &bic );
}

void Ral_CmdPipelineBarrier( ralCommandBuffer_t *cb, ralBarrierScope_t scope ) {
	VkMemoryBarrier mb;
	ralVkBarrierTranslation_t barrier;
	if ( !cb ) return;
	barrier = ralVk_TranslateBarrierScope( scope );
	RAL_ZERO( mb );
	mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	mb.srcAccessMask = barrier.srcAccess;
	mb.dstAccessMask = barrier.dstAccess;
	cb->backend->vk.CmdPipelineBarrier( cb->cb, barrier.srcStage, barrier.dstStage, 0, 1, &mb, 0, NULL, 0, NULL );
}

void Ral_WriteTimestamp( ralCommandBuffer_t *cb, ralQueryPool_t *pool, uint32_t query ) {
	if ( !cb || !pool || query >= pool->count ) return;
	cb->backend->vk.CmdWriteTimestamp2( cb->cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pool->pool, query );
}

qboolean Ral_BeginDebugLabel( ralCommandBuffer_t *cb, const char *label, const float color[4] ) {
	VkDebugUtilsLabelEXT li;
	if ( !cb || !label || !cb->backend->haveDebugUtils || !cb->backend->vk.CmdBeginDebugUtilsLabelEXT ) return qfalse;
	RAL_ZERO( li );
	li.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	li.pLabelName = label;
	if ( color ) { li.color[0] = color[0]; li.color[1] = color[1]; li.color[2] = color[2]; li.color[3] = color[3]; }
	cb->backend->vk.CmdBeginDebugUtilsLabelEXT( cb->cb, &li );
	cb->debugLabelBeginCount++;
	return qtrue;
}
qboolean Ral_EndDebugLabel( ralCommandBuffer_t *cb ) {
	if ( !cb || !cb->backend->haveDebugUtils || !cb->backend->vk.CmdEndDebugUtilsLabelEXT ) return qfalse;
	cb->backend->vk.CmdEndDebugUtilsLabelEXT( cb->cb );
	cb->debugLabelEndCount++;
	return qtrue;
}

qboolean Ral_GetDebugLabelStats( const ralCommandBuffer_t *cb, ralDebugLabelStats_t *out ) {
	if ( !cb || !out ) return qfalse;
	RAL_ZERO( *out );
	out->supported = ( cb->backend->haveDebugUtils
		&& cb->backend->vk.CmdBeginDebugUtilsLabelEXT
		&& cb->backend->vk.CmdEndDebugUtilsLabelEXT ) ? qtrue : qfalse;
	out->renderingLabelActive = cb->renderingDebugLabelActive;
	out->beginCount = cb->debugLabelBeginCount;
	out->endCount = cb->debugLabelEndCount;
	return qtrue;
}

void Ral_ResetDebugLabelStats( ralCommandBuffer_t *cb ) {
	if ( !cb ) return;
	cb->renderingDebugLabelActive = qfalse;
	cb->debugLabelBeginCount = 0;
	cb->debugLabelEndCount = 0;
}

void Ral_CmdSetViewport( ralCommandBuffer_t *cb, const ralViewport_t *vp ) {
	VkViewport v;
	if ( !cb || !vp ) return;
	v.x = vp->x; v.y = vp->y; v.width = vp->width; v.height = vp->height; v.minDepth = vp->minDepth; v.maxDepth = vp->maxDepth;
	cb->backend->vk.CmdSetViewport( cb->cb, 0, 1, &v );
}
void Ral_CmdSetScissor( ralCommandBuffer_t *cb, const ralRect_t *rect ) {
	VkRect2D r;
	if ( !cb || !rect ) return;
	r.offset.x = rect->x; r.offset.y = rect->y; r.extent.width = rect->width; r.extent.height = rect->height;
	cb->backend->vk.CmdSetScissor( cb->cb, 0, 1, &r );
}
void Ral_CmdSetDepthBias( ralCommandBuffer_t *cb, float constant, float clamp, float slope ) {
	if ( !cb ) return;
	cb->backend->vk.CmdSetDepthBias( cb->cb, constant, clamp, slope );
}

// ════════════════════════════════════════════════════════════════════════
// pipeline-dependent ops
// ════════════════════════════════════════════════════════════════════════
//
// vkCmdBindPipeline records the pipeline + caches its layout/bindPoint on the
// ralCommandBuffer_t wrapper — subsequent Ral_CmdBindBindGroup / Ral_CmdPush-
// Constants / Ral_CmdDraw* need both. The wrapper holds weak refs (caller
// guarantees the pipeline outlives Submit completion via the deferred-destroy
// queue's frame-fence delay).
void Ral_CmdBindPipeline( ralCommandBuffer_t *cb, ralPipeline_t *p ) {
	if ( !cb ) return;
	if ( !p ) {
		// NULL-fallthrough contract: clear cb's current-
		// pipeline state so subsequent Ral_CmdDraw / DrawIndexed / Dispatch on
		// this cmd buffer bail (they check cb->currentPipeline). Avoids
		// VUID-vkCmdDraw-None-08606 / -vkCmdDispatch-None-08606 when the
		// renderer's vk_ral_lookup_pipeline returns NULL for a VkPipeline that
		// has no RAL sibling yet (parallel-paths era — those calls skip
		// recording on the parallel buffer; legacy qvkCmd* still authoritative).
		cb->currentPipeline = NULL;
		return;
	}
	cb->backend->vk.CmdBindPipeline( cb->cb, p->bindPoint, p->pipeline );
	cb->currentPipeline  = p;
	cb->currentLayout    = p->layout;
	cb->currentBindPoint = p->bindPoint;
}

void Ral_CmdBindBindGroup( ralCommandBuffer_t *cb, uint32_t setIndex, ralBindGroup_t *g ) {
	if ( !cb || !g || cb->currentLayout == VK_NULL_HANDLE ) {
		if ( cb && cb->currentLayout == VK_NULL_HANDLE )
			R_LOG( rch_ral, SEV_WARN, "Ral_CmdBindBindGroup: no pipeline bound (call Ral_CmdBindPipeline first)\n" );
		return;
	}
	cb->backend->vk.CmdBindDescriptorSets( cb->cb, cb->currentBindPoint, cb->currentLayout,
	                                       setIndex, 1, &g->set, 0, NULL );
}


void Ral_CmdBindVertexBuffer( ralCommandBuffer_t *cb, uint32_t binding, ralBuffer_t *buf, uint64_t offset ) {
	VkDeviceSize off;
	if ( !cb || !buf ) return;
	off = (VkDeviceSize)offset;
	cb->backend->vk.CmdBindVertexBuffers( cb->cb, binding, 1, &buf->buffer, &off );
}

void Ral_CmdBindIndexBuffer( ralCommandBuffer_t *cb, ralBuffer_t *buf, uint64_t offset, ralIndexType_t type ) {
	if ( !cb || !buf ) return;
	cb->backend->vk.CmdBindIndexBuffer( cb->cb, buf->buffer, (VkDeviceSize)offset,
	                                    ( type == RAL_INDEX_UINT32 ) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16 );
}

// Push constants: vkCmdPushConstants takes stage flags + offset + size + data.
// RAL exposes per-call stageFlags (RAL_STAGE_*) instead of forcing the
// pipeline's declared stages — lets the caller push to a subset.
void Ral_CmdPushConstants( ralCommandBuffer_t *cb, uint32_t stageFlags, uint32_t offset, uint32_t size, const void *data ) {
	VkShaderStageFlags vkStages = 0;
	if ( !cb || !data || size == 0 || cb->currentLayout == VK_NULL_HANDLE ) {
		if ( cb && cb->currentLayout == VK_NULL_HANDLE )
			R_LOG( rch_ral, SEV_WARN, "Ral_CmdPushConstants: no pipeline bound\n" );
		return;
	}
	// Require a bound pipeline so we have an authoritative pushConstantSize to
	// validate against, then range-check WITHOUT the `offset + size` add (which
	// is uint32 and wraps — e.g. offset=0xFFFFFFF0,size=0x20 → 0x10, slipping a
	// huge offset past the bound and recording an out-of-range vkCmdPushConstants).
	if ( !cb->currentPipeline ) {
		R_LOG( rch_ral, SEV_WARN, "Ral_CmdPushConstants: no pipeline bound (cannot bound-check range)\n" );
		return;
	}
	{
		uint32_t limit = cb->currentPipeline->pushConstantSize;
		if ( size > limit || offset > limit - size ) {
			R_LOG( rch_ral, SEV_WARN, "Ral_CmdPushConstants: range [offset=%u size=%u] exceeds pipeline's pushConstantSize %u\n",
			        offset, size, limit );
			return;
		}
	}
	if ( stageFlags & RAL_STAGE_VERTEX   ) vkStages |= VK_SHADER_STAGE_VERTEX_BIT;
	if ( stageFlags & RAL_STAGE_FRAGMENT ) vkStages |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if ( stageFlags & RAL_STAGE_COMPUTE  ) vkStages |= VK_SHADER_STAGE_COMPUTE_BIT;
	if ( vkStages == 0 ) vkStages = cb->currentPipeline ? cb->currentPipeline->pushConstantStages : VK_SHADER_STAGE_ALL_GRAPHICS;
	cb->backend->vk.CmdPushConstants( cb->cb, cb->currentLayout, vkStages, offset, size, data );
}

void Ral_CmdDraw( ralCommandBuffer_t *cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance ) {
	if ( !cb ) return;
	// Bail when no pipeline is currently bound. The
	// renderer's parallel-paths era can hit this when vk_ral_lookup_pipeline
	// returned NULL for a VkPipeline that has no RAL sibling yet (the matching
	// Ral_CmdBindPipeline cleared cb->currentPipeline). Legacy qvkCmdDraw on
	// the renderer's own cmd buffer remains authoritative.
	if ( !cb->currentPipeline ) return;
	cb->backend->vk.CmdDraw( cb->cb, vertexCount, instanceCount ? instanceCount : 1, firstVertex, firstInstance );
}

void Ral_CmdDrawIndexed( ralCommandBuffer_t *cb, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance ) {
	if ( !cb ) return;
	if ( !cb->currentPipeline ) return;   // Same NULL-fallthrough rationale as Ral_CmdDraw above.
	cb->backend->vk.CmdDrawIndexed( cb->cb, indexCount, instanceCount ? instanceCount : 1, firstIndex, vertexOffset, firstInstance );
}

void Ral_CmdDrawIndexedIndirect( ralCommandBuffer_t *cb, ralBuffer_t *argBuf, uint64_t offset, uint32_t drawCount, uint32_t stride ) {
	if ( !cb || !argBuf ) return;
	cb->backend->vk.CmdDrawIndexedIndirect( cb->cb, argBuf->buffer, (VkDeviceSize)offset, drawCount, stride );
}

void Ral_CmdDrawIndexedIndirectCount( ralCommandBuffer_t *cb, ralBuffer_t *argBuf, uint64_t argOffset,
                                      ralBuffer_t *countBuf, uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride ) {
	if ( !cb || !argBuf || !countBuf ) return;
	// Refuse to silently degrade. The earlier fallback substituted
	// vkCmdDrawIndexedIndirect with maxDrawCount, which ignores the count
	// buffer and renders *wrong* output (always maxDrawCount draws regardless
	// of what compute culling produced). Consumers must branch on
	// caps.drawIndirectCount and provide a non-indirect-count fallback path
	// when the device lacks it.
	if ( !cb->backend->caps.drawIndirectCount || !cb->backend->vk.CmdDrawIndexedIndirectCount ) {
		RAL_NOTE_ONCE( "Ral_CmdDrawIndexedIndirectCount called but caps.drawIndirectCount=false; consumer must branch on caps before calling. Call is a no-op (no degraded substitute).\n" );
		R_LOG( rch_ral, SEV_ERROR, "Ral_CmdDrawIndexedIndirectCount: caps.drawIndirectCount=false but called anyway — refusing to substitute vkCmdDrawIndexedIndirect (would render the wrong primitive count). Consumer must cap-branch.\n" );
		return;
	}
	cb->backend->vk.CmdDrawIndexedIndirectCount( cb->cb, argBuf->buffer, (VkDeviceSize)argOffset,
	                                             countBuf->buffer, (VkDeviceSize)countOffset, maxDrawCount, stride );
}

void Ral_CmdDispatch( ralCommandBuffer_t *cb, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ ) {
	if ( !cb ) return;
	if ( !cb->currentPipeline ) return;   // NULL-fallthrough (see Ral_CmdDraw).
	cb->backend->vk.CmdDispatch( cb->cb, groupCountX ? groupCountX : 1, groupCountY ? groupCountY : 1, groupCountZ ? groupCountZ : 1 );
}

void Ral_CmdDispatchIndirect( ralCommandBuffer_t *cb, ralBuffer_t *argBuf, uint64_t offset ) {
	if ( !cb || !argBuf ) return;
	cb->backend->vk.CmdDispatchIndirect( cb->cb, argBuf->buffer, (VkDeviceSize)offset );
}

// ════════════════════════════════════════════════════════════════════════
// `_Raw` shim retirement.
//
// Parallel-path raw-handle entry points are gone. The remaining command
// surface below accepts typed RAL resources and command buffers only.
// ════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════
// Typed RAL cmd surface additions (8 new functions).
// See ral_command.h docblock for the migration rationale + NULL-fallthrough
// contract. Bodies unwrap typed args via the struct .vkHandle / .pipeline /
// .buffer / .image accessors and call the backend's loaded vkCmd*.
// ════════════════════════════════════════════════════════════════════════

void Ral_CmdBindVertexBuffers( ralCommandBuffer_t *cb, uint32_t firstBinding,
                               uint32_t bindingCount,
                               ralBuffer_t *const *buffers,
                               const uint64_t *offsets )
{
	VkBuffer scratchBufs[ RAL_VK_MAX_PIPELINE_SETS ];   // reuse the per-bind small-stack constant
	uint32_t i;
	if ( !cb || bindingCount == 0 || !buffers ) return;
	if ( bindingCount > RAL_VK_MAX_PIPELINE_SETS ) return;
	for ( i = 0; i < bindingCount; i++ ) {
		if ( !buffers[i] ) return;   // null-fallthrough: missing wrapper → skip
		scratchBufs[i] = buffers[i]->buffer;
	}
	cb->backend->vk.CmdBindVertexBuffers( cb->cb, firstBinding, bindingCount,
	                                       scratchBufs, (const VkDeviceSize *)offsets );
}

void Ral_CmdPushConstantsLayout( ralCommandBuffer_t *cb,
                                 ralPipelineLayout_t *layout,
                                 uint32_t stageFlags,         // VkShaderStageFlags directly — parallel-paths-era variant
                                 uint32_t offset,
                                 uint32_t size,
                                 const void *data )
{
	if ( !cb || !layout || size == 0 || !data ) return;
	// `stageFlags` is passed through as VkShaderStageFlags
	// (the renderer's call sites use VK_SHADER_STAGE_*_BIT). The existing
	// Ral_CmdPushConstants (above) uses the RAL_STAGE_* convention for typed
	// callers. Both coexist during the parallel-paths era.
	cb->backend->vk.CmdPushConstants( cb->cb, layout->vkHandle,
	                                   (VkShaderStageFlags)stageFlags, offset, size, data );
}

void Ral_CmdPipelineBarrierFull( ralCommandBuffer_t *cb, const ralPipelineBarrierInfo_t *info )
{
	VkMemoryBarrier       memBarriers   [ 8 ];
	VkBufferMemoryBarrier bufferBarriers[ 8 ];
	VkImageMemoryBarrier  imageBarriers [ 16 ];
	uint32_t              i, m, b, im;
	if ( !cb || !info ) return;
	m  = info->memoryBarrierCount;
	b  = info->bufferMemoryBarrierCount;
	im = info->imageMemoryBarrierCount;
	// The barrier scratch arrays are intentionally fixed-capacity (8/8/16 covers
	// every renderer barrier batch). Recording only a prefix would silently drop
	// barriers — leaving resources in the wrong layout / unsynchronized — which is
	// a far worse failure than refusing the call. Hard-fail with a log (same
	// "no silent degrade" idiom as Ral_Submit / Ral_CmdDrawIndexedIndirectCount).
	if ( m  > ARRAY_LEN( memBarriers    ) ||
	     b  > ARRAY_LEN( bufferBarriers ) ||
	     im > ARRAY_LEN( imageBarriers  ) ) {
		R_LOG( rch_ral, SEV_ERROR, "Ral_CmdPipelineBarrierFull: over-capacity (mem %u/%u, buffer %u/%u, image %u/%u) — refusing barrier (would truncate; split the batch or raise the scratch caps)\n",
		        m,  (uint32_t)ARRAY_LEN( memBarriers    ),
		        b,  (uint32_t)ARRAY_LEN( bufferBarriers ),
		        im, (uint32_t)ARRAY_LEN( imageBarriers  ) );
		return;
	}
	for ( i = 0; i < m; i++ ) {
		RAL_ZERO( memBarriers[i] );
		memBarriers[i].sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memBarriers[i].srcAccessMask = info->memoryBarriers[i].srcAccessMask;
		memBarriers[i].dstAccessMask = info->memoryBarriers[i].dstAccessMask;
	}
	for ( i = 0; i < b; i++ ) {
		RAL_ZERO( bufferBarriers[i] );
		bufferBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarriers[i].srcAccessMask       = info->bufferMemoryBarriers[i].srcAccessMask;
		bufferBarriers[i].dstAccessMask       = info->bufferMemoryBarriers[i].dstAccessMask;
		bufferBarriers[i].srcQueueFamilyIndex = info->bufferMemoryBarriers[i].srcQueueFamilyIndex;
		bufferBarriers[i].dstQueueFamilyIndex = info->bufferMemoryBarriers[i].dstQueueFamilyIndex;
		bufferBarriers[i].buffer              = info->bufferMemoryBarriers[i].buffer ? info->bufferMemoryBarriers[i].buffer->buffer : VK_NULL_HANDLE;
		bufferBarriers[i].offset              = info->bufferMemoryBarriers[i].offset;
		bufferBarriers[i].size                = info->bufferMemoryBarriers[i].size;
	}
	for ( i = 0; i < im; i++ ) {
		const ralImageMemoryBarrier_t *r = &info->imageMemoryBarriers[i];
		RAL_ZERO( imageBarriers[i] );
		imageBarriers[i].sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarriers[i].srcAccessMask               = r->srcAccessMask;
		imageBarriers[i].dstAccessMask               = r->dstAccessMask;
		imageBarriers[i].oldLayout                   = (VkImageLayout)r->oldLayout;
		imageBarriers[i].newLayout                   = (VkImageLayout)r->newLayout;
		imageBarriers[i].srcQueueFamilyIndex         = r->srcQueueFamilyIndex;
		imageBarriers[i].dstQueueFamilyIndex         = r->dstQueueFamilyIndex;
		imageBarriers[i].image                       = r->texture ? r->texture->image : VK_NULL_HANDLE;
		imageBarriers[i].subresourceRange.aspectMask = r->aspectMask;
		imageBarriers[i].subresourceRange.baseMipLevel = r->baseMipLevel;
		imageBarriers[i].subresourceRange.levelCount   = r->levelCount;
		imageBarriers[i].subresourceRange.baseArrayLayer = r->baseArrayLayer;
		imageBarriers[i].subresourceRange.layerCount     = r->layerCount;
	}
	cb->backend->vk.CmdPipelineBarrier( cb->cb,
	                                     (VkPipelineStageFlags)info->srcStageMask,
	                                     (VkPipelineStageFlags)info->dstStageMask,
	                                     (VkDependencyFlags)info->dependencyFlags,
	                                     m,  m  ? memBarriers    : NULL,
	                                     b,  b  ? bufferBarriers : NULL,
	                                     im, im ? imageBarriers  : NULL );
}

// Derive the VkAccessFlags a stage uses, for a single-texture transition.
// Conservative: a producer stage gets its write access, a consumer stage its
// read access; the shader/transfer stages get read|write to cover either role.
void Ral_CmdTransitionTexture( ralCommandBuffer_t *cb, ralTexture_t *tex,
                               ralPipelineStageFlags_t srcStage,
                               ralPipelineStageFlags_t dstStage,
                               uint32_t newVkLayout ) {
	VkImageMemoryBarrier ib;
	if ( !cb || !tex ) return;
	RAL_ZERO( ib );
	ib.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	ib.srcAccessMask                   = ralVk_TranslateStageAccess( srcStage );
	ib.dstAccessMask                   = ralVk_TranslateStageAccess( dstStage );
	ib.oldLayout                       = tex->currentLayout;
	ib.newLayout                       = (VkImageLayout)newVkLayout;
	ib.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	ib.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	ib.image                           = tex->image;
	ib.subresourceRange.aspectMask     = tex->aspect;
	ib.subresourceRange.baseMipLevel   = 0;
	ib.subresourceRange.levelCount     = tex->mipLevels;
	ib.subresourceRange.baseArrayLayer = 0;
	ib.subresourceRange.layerCount     = tex->arrayLayers;
	cb->backend->vk.CmdPipelineBarrier( cb->cb,
	                                    (VkPipelineStageFlags)srcStage,
	                                    (VkPipelineStageFlags)dstStage,
	                                    0, 0, NULL, 0, NULL, 1, &ib );
	// Atomic with the barrier: tracked layout can never desync from the GPU.
	tex->currentLayout = (VkImageLayout)newVkLayout;
}

void Ral_CmdCopyImage( ralCommandBuffer_t *cb, ralTexture_t *src, ralTexture_t *dst,
                       uint32_t regionCount, const ralImageCopy_t *regions )
{
	if ( !cb || !src || !dst || regionCount == 0 || !regions ) return;
	// ralImageCopy_t is binary-compat with VkImageCopy (matching field order).
	cb->backend->vk.CmdCopyImage( cb->cb,
	                               src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	                               dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                               regionCount, (const VkImageCopy *)regions );
}

void Ral_CmdCopyBufferToImage( ralCommandBuffer_t *cb, ralBuffer_t *src, ralTexture_t *dst,
                               uint32_t regionCount, const ralBufferImageCopy_t *regions )
{
	if ( !cb || !src || !dst || regionCount == 0 || !regions ) return;
	cb->backend->vk.CmdCopyBufferToImage( cb->cb,
	                                       src->buffer,
	                                       dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                                       regionCount, (const VkBufferImageCopy *)regions );
}

void Ral_CmdBlitImage( ralCommandBuffer_t *cb, ralTexture_t *src, ralTexture_t *dst,
                       uint32_t regionCount, const ralImageBlit_t *regions,
                       ralFilter_t filter )
{
	if ( !cb || !src || !dst || regionCount == 0 || !regions ) return;
	cb->backend->vk.CmdBlitImage( cb->cb,
	                               src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	                               dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                               regionCount, (const VkImageBlit *)regions,
	                               ( filter == RAL_FILTER_LINEAR ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST );
}

void Ral_CmdClearAttachments( ralCommandBuffer_t *cb, uint32_t attachmentCount,
                              const ralClearAttachment_t *attachments,
                              uint32_t rectCount, const ralClearRect_t *rects )
{
	if ( !cb || attachmentCount == 0 || !attachments || rectCount == 0 || !rects ) return;
	cb->backend->vk.CmdClearAttachments( cb->cb,
	                                      attachmentCount, (const VkClearAttachment *)attachments,
	                                      rectCount,       (const VkClearRect *)rects );
}

void Ral_CmdResetQueryPool( ralCommandBuffer_t *cb, ralQueryPool_t *pool,
                            uint32_t firstQuery, uint32_t queryCount )
{
	if ( !cb || !pool || queryCount == 0 ) return;
	cb->backend->vk.CmdResetQueryPool( cb->cb, pool->pool, firstQuery, queryCount );
}

void Ral_CmdWriteTimestamp( ralCommandBuffer_t *cb, uint32_t pipelineStageBits,
                            ralQueryPool_t *pool, uint32_t query )
{
	if ( !cb || !pool ) return;
	cb->backend->vk.CmdWriteTimestamp( cb->cb, (VkPipelineStageFlagBits)pipelineStageBits,
	                                    pool->pool, query );
}

// 4 void*-handle parallel-paths shims retired here
// (PipelineBarrier / CopyImage / ResetQueryPool / WriteTimestamp). The
// renderer's callsites migrated to the typed Ral_Cmd{PipelineBarrierFull,
// CopyImage,ResetQueryPool,WriteTimestamp} surface above. Query pools are
// native RAL resources; texture adoption remains a Vulkan-migration detail.

// ─── dynamic rendering ──────────────────────────────────────────────────
// Ral_BeginRendering transitions every attachment from its current layout to
// the matching ATTACHMENT_OPTIMAL layout (UNDEFINED tex on first use → fine
// when the loadOp is CLEAR or DONT_CARE; LOAD on UNDEFINED is the caller's
// bug). The post-render layout stays ATTACHMENT_OPTIMAL — consumers needing
// SHADER_READ_ONLY (e.g., to sample the colour target afterwards) emit their
// own transition via the renderer migration's barrier code.
// Transition `tex` from `tex->currentLayout` to `newLayout` if different;
// updates tex->currentLayout. Coarse stage/access masks — sufficient for the
// RAL test; the renderer migration tightens these per use case.
static void ralVk_RenderTargetTransition( ralCommandBuffer_t *cb, ralTexture_t *tex, VkImageLayout newLayout ) {
	VkImageMemoryBarrier bar;
	ralVkLayoutTranslation_t src, dst;
	if ( !tex || tex->currentLayout == newLayout ) return;
	src = ralVk_TranslateSourceLayout( tex->currentLayout );
	dst = ralVk_TranslateDestinationLayout( newLayout );
	RAL_ZERO( bar );
	bar.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	bar.srcAccessMask                   = src.access;
	bar.dstAccessMask                   = dst.access;
	bar.oldLayout                       = tex->currentLayout;
	bar.newLayout                       = newLayout;
	bar.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	bar.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	bar.image                           = tex->image;
	// An image-layout transition is WHOLE-IMAGE: for a combined depth+stencil
	// format both aspects move together, so the barrier subresourceRange must
	// include the stencil aspect even when the texture was adopted DEPTH-only
	// (e.g. the cascaded shadow map, rendered depth-only into a D24S8 image)
	// — without it: VUID-VkImageMemoryBarrier-image-03320, and the never-
	// transitioned stencil plane trips VUID-vkCmdDraw-None-09600 at draw. This is
	// distinct from the ATTACHMENT aspect (which stays depth-only — the stencil
	// auto-bind in Ral_BeginRendering still keys on tex->aspect, so no stencil
	// attachment is bound). Purely additive: depth-only (D32) + color textures
	// keep tex->aspect unchanged.
	bar.subresourceRange.aspectMask     = tex->aspect;
	if ( tex->vkFormat == VK_FORMAT_D24_UNORM_S8_UINT
	  || tex->vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT
	  || tex->vkFormat == VK_FORMAT_D16_UNORM_S8_UINT )
		bar.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	bar.subresourceRange.baseMipLevel   = 0;
	bar.subresourceRange.levelCount     = tex->mipLevels;
	bar.subresourceRange.baseArrayLayer = 0;
	bar.subresourceRange.layerCount     = tex->arrayLayers;
	cb->backend->vk.CmdPipelineBarrier( cb->cb, src.stage, dst.stage, 0, 0, NULL, 0, NULL, 1, &bar );
	tex->currentLayout = newLayout;
}

void Ral_BeginRendering( ralCommandBuffer_t *cb, const ralRenderingInfo_t *ri_ ) {
	VkRenderingAttachmentInfo  colorAtt[ RAL_MAX_COLOR_ATTACHMENTS ];
	VkRenderingAttachmentInfo  depthAtt;
	VkRenderingAttachmentInfo  stencilAtt;
	qboolean                   haveStencil = qfalse;
	VkRenderingInfo            info;
	uint32_t                   i;
	if ( !cb || !ri_ ) return;
	if ( ri_->numColorAttachments > RAL_MAX_COLOR_ATTACHMENTS ) return;
	if ( cb->renderingDebugLabelActive ) {
		R_LOG( rch_ral, SEV_WARN, "Ral_BeginRendering: prior debug-label scope still active\n" );
		Ral_EndDebugLabel( cb );
		cb->renderingDebugLabelActive = qfalse;
	}

	// transitions first — every attachment needs the right layout before vkCmdBeginRendering
	for ( i = 0; i < ri_->numColorAttachments; i++ )
		ralVk_RenderTargetTransition( cb, ri_->colorAttachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	if ( ri_->depthAttachment )
		ralVk_RenderTargetTransition( cb, ri_->depthAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );

	for ( i = 0; i < ri_->numColorAttachments; i++ ) {
		RAL_ZERO( colorAtt[i] );
		colorAtt[i].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAtt[i].imageView   = ri_->colorAttachments[i] ? ri_->colorAttachments[i]->defaultView : VK_NULL_HANDLE;
		colorAtt[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAtt[i].loadOp      = ralVk_TranslateLoadOp ( ri_->colorLoadOps [i] );
		colorAtt[i].storeOp     = ralVk_TranslateStoreOp( ri_->colorStoreOps[i] );
		colorAtt[i].clearValue.color.float32[0] = ri_->colorClears[i].color[0];
		colorAtt[i].clearValue.color.float32[1] = ri_->colorClears[i].color[1];
		colorAtt[i].clearValue.color.float32[2] = ri_->colorClears[i].color[2];
		colorAtt[i].clearValue.color.float32[3] = ri_->colorClears[i].color[3];
		if ( ri_->resolveAttachments[i] ) {
			colorAtt[i].resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
			colorAtt[i].resolveImageView   = ri_->resolveAttachments[i]->defaultView;
			colorAtt[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}
	if ( ri_->depthAttachment ) {
		const ralTexture_t *dtex = ri_->depthAttachment;
		// Array depth image (adopted via Ral_AdoptArrayTexture): the layer offset
		// can only live in the bound attachment view (VkRenderingInfo has no
		// baseArrayLayer), so pick the per-layer view for depthAttachmentLayerIndex.
		// Single-layer textures (every current caller: no layerViews, index 0) bind
		// defaultView exactly as before — byte-identical.
		VkImageView depthView = dtex->defaultView;
		if ( dtex->layerViews && dtex->numLayerViews > 0 ) {
			uint32_t li = ( ri_->depthAttachmentLayerIndex < dtex->numLayerViews )
			            ? ri_->depthAttachmentLayerIndex : 0;
			depthView = dtex->layerViews[ li ];
		}
		RAL_ZERO( depthAtt );
		depthAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAtt.imageView   = depthView;
		depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAtt.loadOp      = ralVk_TranslateLoadOp ( ri_->depthLoadOp  );
		depthAtt.storeOp     = ralVk_TranslateStoreOp( ri_->depthStoreOp );
		depthAtt.clearValue.depthStencil.depth   = ri_->depthClear;
		depthAtt.clearValue.depthStencil.stencil = 0;

		// A combined depth+stencil format (e.g. D24_UNORM_S8_UINT) carries a
		// stencil aspect on the same image/view; bind it as the stencil
		// attachment too. Depth-only formats (no stencil aspect) leave
		// pStencilAttachment NULL. The depthAttachment transition above already
		// targets DEPTH_STENCIL_ATTACHMENT_OPTIMAL, valid for both aspects. Use the
		// same per-layer view the depth aspect resolved to (depthView) so a future
		// depth+stencil array image binds the right layer for both aspects; for the
		// single-layer callers depthView == defaultView (byte-identical). The
		// current array consumer (shadow) is DEPTH-only so this branch stays off.
		if ( ri_->depthAttachment->aspect & VK_IMAGE_ASPECT_STENCIL_BIT ) {
			haveStencil = qtrue;
			RAL_ZERO( stencilAtt );
			stencilAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			stencilAtt.imageView   = depthView;
			stencilAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			stencilAtt.loadOp      = ralVk_TranslateLoadOp ( ri_->stencilLoadOp  );
			stencilAtt.storeOp     = ralVk_TranslateStoreOp( ri_->stencilStoreOp );
			stencilAtt.clearValue.depthStencil.depth   = ri_->depthClear;
			stencilAtt.clearValue.depthStencil.stencil = ri_->stencilClear;
		}
	}

	RAL_ZERO( info );
	info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.renderArea.offset.x  = ri_->renderArea.x;
	info.renderArea.offset.y  = ri_->renderArea.y;
	info.renderArea.extent.width  = ri_->renderArea.width;
	info.renderArea.extent.height = ri_->renderArea.height;
	info.layerCount           = 1;       // v1: single-layer rendering; multi-view + array layers land with stereoscopic / cubemap work
	info.viewMask             = 0;
	info.colorAttachmentCount = ri_->numColorAttachments;
	info.pColorAttachments    = ri_->numColorAttachments ? colorAtt : NULL;
	info.pDepthAttachment     = ri_->depthAttachment ? &depthAtt : NULL;
	info.pStencilAttachment   = haveStencil ? &stencilAtt : NULL;   // stencil attachment when the depth format carries a stencil aspect
	cb->renderingDebugLabelActive = ri_->debugName
		? Ral_BeginDebugLabel( cb, ri_->debugName, NULL ) : qfalse;
	cb->backend->vk.CmdBeginRendering( cb->cb, &info );
}

void Ral_EndRendering( ralCommandBuffer_t *cb ) {
	if ( !cb ) return;
	cb->backend->vk.CmdEndRendering( cb->cb );
	if ( cb->renderingDebugLabelActive ) {
		if ( !Ral_EndDebugLabel( cb ) )
			R_LOG( rch_ral, SEV_WARN, "Ral_EndRendering: active debug-label scope could not close\n" );
		cb->renderingDebugLabelActive = qfalse;
	}
}

// ════════════════════════════════════════════════════════════════════════
// \ral_dump async — exercise queues / submit / sync / query / deferred destroy
// ════════════════════════════════════════════════════════════════════════
static void ralVk_BufQfo( ralBackend_t *b, VkCommandBuffer cb, VkBuffer buf, qboolean release ) {
	// queue-ownership transfer barrier on `buf` (transfer family → graphics family).
	// release == qtrue: recorded on the transfer queue; qfalse: the paired acquire on graphics.
	VkBufferMemoryBarrier bar;
	RAL_ZERO( bar );
	bar.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bar.srcAccessMask       = release ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
	bar.dstAccessMask       = release ? 0 : ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT );
	bar.srcQueueFamilyIndex = b->queueFamily[ RAL_QUEUE_TRANSFER ];
	bar.dstQueueFamilyIndex = b->queueFamily[ RAL_QUEUE_GRAPHICS ];
	bar.buffer              = buf;
	bar.offset              = 0;
	bar.size                = VK_WHOLE_SIZE;
	if ( release )
		b->vk.CmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 1, &bar, 0, NULL );
	else
		b->vk.CmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 1, &bar, 0, NULL );
}

void ralVk_RunAsyncTest( ralBackend_t *b ) {
	const uint32_t        BIG = 1u << 20;   // 1 MiB
	ralBufferCreateInfo_t bci;
	ralBuffer_t          *buf = NULL, *readback = NULL, *tsBuf = NULL;
	ralSemaphore_t       *timeline = NULL;
	ralQueryPool_t       *qpool = NULL;
	uint32_t              i;

	R_LOG( rch_ral, SEV_INFO, "===== RAL async test (Phase 7.3) =====\n" );
	R_LOG( rch_ral, SEV_INFO, "  queues: graphics fam %u, compute fam %u (%s), transfer fam %u (%s)\n",
	        b->queueFamily[RAL_QUEUE_GRAPHICS],
	        b->queueFamily[RAL_QUEUE_COMPUTE],  b->caps.asyncCompute  ? "dedicated" : "shared-with-graphics",
	        b->queueFamily[RAL_QUEUE_TRANSFER], b->caps.asyncTransfer ? "dedicated" : "shared-with-graphics" );

	// ── (1) fence cycle ────────────────────────────────────────────────
	{
		ralFence_t         *f  = Ral_CreateFence( b );
		ralCommandBuffer_t *cb = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
		if ( f && cb ) {
			ralCommandBuffer_t *cbs[1]; ralSubmitInfo_t si; qboolean pre, post;
			Ral_BeginCommandBuffer( cb );
			Ral_CmdPipelineBarrier( cb, RAL_BARRIER_ALL );
			Ral_EndCommandBuffer( cb );
			cbs[0] = cb; RAL_ZERO( si ); si.commandBuffers = cbs; si.numCommandBuffers = 1; si.signalFence = f;
			pre = Ral_FenceSignaled( f );
			Ral_Submit( b, RAL_QUEUE_GRAPHICS, &si );
			Ral_WaitFence( f, ~0ull );
			post = Ral_FenceSignaled( f );
			R_LOG( rch_ral, SEV_INFO, "  fence cycle: pre-wait signaled=%s, post-wait signaled=%s  (expect post=yes)\n", pre ? "yes" : "no", post ? "yes" : "no" );
		} else R_LOG( rch_ral, SEV_WARN, "  fence cycle: setup failed\n" );
		if ( cb ) Ral_DestroyCommandBuffer( cb );
		if ( f )  Ral_DestroyFence( f );
	}

	// ── (2) async buffer upload (1 MiB device-local) ───────────────────
	RAL_ZERO( bci ); bci.size = BIG; bci.usage = RAL_BUFFER_STORAGE; bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-async-buf";
	buf = Ral_CreateBuffer( b, &bci );
	RAL_ZERO( bci ); bci.size = 256u; bci.usage = RAL_BUFFER_TRANSFER_DST; bci.memory = RAL_MEMORY_HOST_COHERENT; bci.debugName = "ral-async-readback";
	readback = Ral_CreateBuffer( b, &bci );
	if ( buf ) {
		byte *data = (byte *)malloc( BIG );
		ralFence_t *uf;
		if ( !data ) {
			R_LOG( rch_ral, SEV_WARN, "  async buffer upload: malloc(%u) failed — skipping\n", BIG );
		} else {
		for ( i = 0; i < BIG; i++ ) data[i] = (byte)( i & 0xFF );
		uf = Ral_BufferUploadAsync( buf, 0, data, BIG );
		if ( uf ) {
			R_LOG( rch_ral, SEV_INFO, "  Ral_BufferUploadAsync: 1 MiB on %s queue; fence signaled=%s (synchronous internal wait — async streaming is Phase 7.15)\n",
			        b->caps.asyncTransfer ? "transfer" : "graphics", Ral_FenceSignaled( uf ) ? "yes" : "no" );
			Ral_WaitFence( uf, ~0ull );
			Ral_DestroyFence( uf );
		} else R_LOG( rch_ral, SEV_WARN, "  Ral_BufferUploadAsync failed\n" );
		free( data );
		}
	}

	// ── (3) timeline semaphore cross-queue: transfer → graphics → compute ──
	timeline = Ral_CreateSemaphore( b, RAL_SEMAPHORE_TIMELINE );
	if ( timeline && buf && readback ) {
		ralCommandBuffer_t *cbT = Ral_AcquireCommandBuffer( b, RAL_QUEUE_TRANSFER );
		ralCommandBuffer_t *cbG = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
		ralCommandBuffer_t *cbC = Ral_AcquireCommandBuffer( b, RAL_QUEUE_COMPUTE );
		if ( cbT && cbG && cbC ) {
			ralCommandBuffer_t *one[1];
			ralSemaphore_t     *waitSem[1], *sigSem[1];
			uint64_t            waitVal[1], sigVal[1];
			ralSubmitInfo_t     si;
			ralBufferCopy_t     copy; byte *m; uint64_t tv;
			waitSem[0] = sigSem[0] = timeline;
			// transfer queue: barrier (upload visibility) + copy buf→readback + release `buf` to graphics; signal timeline@1
			Ral_BeginCommandBuffer( cbT );
			Ral_CmdPipelineBarrier( cbT, RAL_BARRIER_ALL );
			RAL_ZERO( copy ); copy.size = 256u;
			Ral_CmdCopyBuffer( cbT, buf, readback, &copy );
			ralVk_BufQfo( b, cbT->cb, buf->buffer, qtrue /*release*/ );
			Ral_EndCommandBuffer( cbT );
			one[0] = cbT; RAL_ZERO( si ); si.commandBuffers = one; si.numCommandBuffers = 1;
			sigVal[0] = 1; si.signalSemaphores = sigSem; si.signalValues = sigVal; si.numSignalSemaphores = 1;
			Ral_Submit( b, RAL_QUEUE_TRANSFER, &si );
			// graphics queue: paired acquire of `buf`; wait timeline@1, signal timeline@2
			Ral_BeginCommandBuffer( cbG );
			ralVk_BufQfo( b, cbG->cb, buf->buffer, qfalse /*acquire*/ );
			Ral_EndCommandBuffer( cbG );
			one[0] = cbG; RAL_ZERO( si ); si.commandBuffers = one; si.numCommandBuffers = 1;
			waitVal[0] = 1; si.waitSemaphores = waitSem; si.waitValues = waitVal; si.numWaitSemaphores = 1;
			sigVal[0]  = 2; si.signalSemaphores = sigSem; si.signalValues = sigVal; si.numSignalSemaphores = 1;
			Ral_Submit( b, RAL_QUEUE_GRAPHICS, &si );
			// compute queue: no-op; wait timeline@2, signal timeline@3 — primes the async-compute submission path
			Ral_BeginCommandBuffer( cbC );
			Ral_EndCommandBuffer( cbC );
			one[0] = cbC; RAL_ZERO( si ); si.commandBuffers = one; si.numCommandBuffers = 1;
			waitVal[0] = 2; si.waitSemaphores = waitSem; si.waitValues = waitVal; si.numWaitSemaphores = 1;
			sigVal[0]  = 3; si.signalSemaphores = sigSem; si.signalValues = sigVal; si.numSignalSemaphores = 1;
			Ral_Submit( b, RAL_QUEUE_COMPUTE, &si );
			// host wait until value ≥ 3 (compute completion), then host-side signal to 4 (exercises vkSignalSemaphore)
			Ral_WaitTimeline( timeline, 3, ~0ull );
			Ral_SignalTimeline( timeline, 4 );
			b->vk.DeviceWaitIdle( b->device );   // guarantee the transfer-queue copy is host-visible before mapping
			tv = Ral_GetTimelineValue( timeline );
			m = (byte *)Ral_MapBuffer( readback );
			R_LOG( rch_ral, SEV_INFO, "  cross-queue (transfer→graphics→compute): timeline value=%llu (expect ≥4 after host signal); queue-ownership barriers (transfer fam %u → graphics fam %u) recorded\n",
			        (unsigned long long)tv, b->queueFamily[RAL_QUEUE_TRANSFER], b->queueFamily[RAL_QUEUE_GRAPHICS] );
			R_LOG( rch_ral, SEV_INFO, "  readback after transfer-queue copy: byte[0..3] = %u %u %u %u  (expect 0 1 2 3)\n", m ? m[0] : 255, m ? m[1] : 255, m ? m[2] : 255, m ? m[3] : 255 );
			Ral_UnmapBuffer( readback );
		} else R_LOG( rch_ral, SEV_WARN, "  cross-queue: command buffer acquisition failed\n" );
		if ( cbC ) Ral_DestroyCommandBuffer( cbC );
		if ( cbG ) Ral_DestroyCommandBuffer( cbG );
		if ( cbT ) Ral_DestroyCommandBuffer( cbT );
	} else R_LOG( rch_ral, SEV_WARN, "  cross-queue: timeline semaphore creation failed\n" );

	// ── (4) GPU timestamps ─────────────────────────────────────────────
	{
		ralQueryPoolCreateInfo_t qci;
		RAL_ZERO( qci ); qci.type = RAL_QUERY_TIMESTAMP; qci.count = 4; qci.debugName = "ral-async-ts";
		qpool = Ral_CreateQueryPool( b, &qci );
	}
	RAL_ZERO( bci ); bci.size = 1u * 1024u * 1024u; bci.usage = RAL_BUFFER_STORAGE; bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-async-tsbuf";
	tsBuf = Ral_CreateBuffer( b, &bci );
	if ( qpool && tsBuf && b->caps.timestampPeriodNs > 0.0f ) {
		ralFence_t         *f  = Ral_CreateFence( b );
		ralCommandBuffer_t *cb = Ral_AcquireCommandBuffer( b, RAL_QUEUE_GRAPHICS );
		if ( f && cb ) {
			ralCommandBuffer_t *cbs[1]; ralSubmitInfo_t si; ralBufferCopy_t copy; uint64_t res[4]; double d10, d30;
			RAL_ZERO( copy ); copy.srcOffset = 0; copy.dstOffset = 512u * 1024u; copy.size = 512u * 1024u;   // non-overlapping halves of tsBuf
			Ral_BeginCommandBuffer( cb );
			b->vk.CmdResetQueryPool( cb->cb, qpool->pool, 0, 4 );      // cmd-buffer-side reset
			Ral_WriteTimestamp( cb, qpool, 0 );
			Ral_CmdCopyBuffer( cb, tsBuf, tsBuf, &copy );              // 512 KiB copy (lower → upper half)
			Ral_WriteTimestamp( cb, qpool, 1 );
			Ral_CmdPipelineBarrier( cb, RAL_BARRIER_ALL );
			Ral_CmdPipelineBarrier( cb, RAL_BARRIER_ALL );
			Ral_WriteTimestamp( cb, qpool, 2 );
			Ral_CmdCopyBuffer( cb, tsBuf, tsBuf, &copy );
			Ral_WriteTimestamp( cb, qpool, 3 );
			Ral_EndCommandBuffer( cb );
			cbs[0] = cb; RAL_ZERO( si ); si.commandBuffers = cbs; si.numCommandBuffers = 1; si.signalFence = f;
			Ral_Submit( b, RAL_QUEUE_GRAPHICS, &si );
			Ral_WaitFence( f, ~0ull );
			if ( Ral_GetQueryResults( qpool, 0, 4, res, qtrue ) ) {
				d10 = (double)( res[1] - res[0] ) * (double)b->caps.timestampPeriodNs / 1000.0;   // µs
				d30 = (double)( res[3] - res[0] ) * (double)b->caps.timestampPeriodNs / 1000.0;
				R_LOG( rch_ral, SEV_INFO, "  timestamps: ts1-ts0 = %.3f us, ts3-ts0 = %.3f us  (timestampPeriod = %.3f ns/tick; expect microsecond-range)\n", d10, d30, (double)b->caps.timestampPeriodNs );
			} else R_LOG( rch_ral, SEV_WARN, "  timestamps: Ral_GetQueryResults returned not-ready\n" );
			Ral_ResetQueryPool( qpool, 0, 4 );   // host-side reset (exercise the public API)
		} else R_LOG( rch_ral, SEV_WARN, "  timestamps: setup failed\n" );
		if ( cb ) Ral_DestroyCommandBuffer( cb );
		if ( f )  Ral_DestroyFence( f );
	} else R_LOG( rch_ral, SEV_INFO, "  timestamps: skipped (timestampPeriod=%.3f)\n", (double)b->caps.timestampPeriodNs );

	// ── (5) deferred-destroy stress: 100 frames × create+destroy a 256 KiB buffer ──
	{
		uint32_t pending0 = b->numPendingDestroy;
		for ( i = 0; i < 100; i++ ) {
			ralBuffer_t *bb;
			Ral_BeginFrame( b );
			RAL_ZERO( bci ); bci.size = 256u * 1024u; bci.usage = RAL_BUFFER_STORAGE; bci.memory = RAL_MEMORY_DEVICE_LOCAL; bci.debugName = "ral-async-loopbuf";
			bb = Ral_CreateBuffer( b, &bci );
			if ( bb ) Ral_DestroyBuffer( bb );
			Ral_EndFrame( b );
		}
		for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT + 1u; i++ ) { Ral_BeginFrame( b ); Ral_EndFrame( b ); }   // drain the tail
		R_LOG( rch_ral, SEV_INFO, "  deferred destroy: 100 frames × (create+destroy 256 KiB buffer); pending was %u, now %u; RAL footprint %u KiB / %u allocations  (no OOM — drain works)\n",
		        pending0, b->numPendingDestroy, (unsigned)( b->ralDeviceLocalBytes >> 10 ), b->numAllocations );
	}

	// ── (6) BindGroup free-path stress: 1000 × create+destroy ──────────
	{
		ralBindEntry_t be; ralBindGroupLayoutCreateInfo_t lci; ralBindGroupLayout_t *bgl;
		RAL_ZERO( be ); be.binding = 0; be.type = RAL_BIND_UNIFORM_BUFFER; be.count = 1; be.stageFlags = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
		RAL_ZERO( lci ); lci.entries = &be; lci.numEntries = 1; lci.bindless = qfalse; lci.debugName = "ral-async-bgl";
		bgl = Ral_CreateBindGroupLayout( b, &lci );
		if ( bgl ) {
			uint32_t failed = 0, peakPending;
			for ( i = 0; i < 1000; i++ ) {
				ralBindGroupCreateInfo_t bci2; ralBindGroup_t *bg;
				RAL_ZERO( bci2 ); bci2.layout = bgl; bci2.numValues = 0; bci2.debugName = "ral-async-bg";
				bg = Ral_CreateBindGroup( b, &bci2 );
				if ( !bg ) failed++;
				else Ral_DestroyBindGroup( bg );   // queued; freed when the deferred-destroy queue drains
			}
			peakPending = b->numPendingDestroy;
			for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT + 1u; i++ ) { Ral_BeginFrame( b ); Ral_EndFrame( b ); }   // drain all 1000
			R_LOG( rch_ral, SEV_INFO, "  BindGroup cycle: 1000 × (create+destroy 1-UBO bind group); %u alloc failures (expect 0); peak pending-destroy %u; after drain %u  (descriptor-set free path works, no pool exhaustion)\n",
			        failed, peakPending, b->numPendingDestroy );
			Ral_DestroyBindGroupLayout( bgl );
		} else R_LOG( rch_ral, SEV_WARN, "  BindGroup cycle: layout creation failed\n" );
	}

	// ── teardown ───────────────────────────────────────────────────────
	if ( tsBuf )    Ral_DestroyBuffer( tsBuf );
	if ( qpool )    Ral_DestroyQueryPool( qpool );
	if ( timeline ) Ral_DestroySemaphore( timeline );
	if ( readback ) Ral_DestroyBuffer( readback );
	if ( buf )      Ral_DestroyBuffer( buf );
	for ( i = 0; i < RAL_VK_MAX_FRAMES_IN_FLIGHT + 1u; i++ ) { Ral_BeginFrame( b ); Ral_EndFrame( b ); }
	R_LOG( rch_ral, SEV_INFO, "  teardown: %u pending destroys, %u live allocations, RAL footprint %u KiB device-local  (expect ~0)\n",
	        b->numPendingDestroy, b->numAllocations, (unsigned)( b->ralDeviceLocalBytes >> 10 ) );
	R_LOG( rch_ral, SEV_INFO, "===== end RAL async test =====\n" );
}
