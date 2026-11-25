// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolve_readback.h"
#include "../../../../renderer/tr_temporal_resolve_oracle.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint32_t captureX, captureY, captureWidth, captureHeight;
	uint32_t coreX, coreY, coreWidth, coreHeight;
	uint64_t currentColorOffset, currentDepthOffset;
	uint64_t previousColorOffset, previousDepthOffset;
	uint64_t velocityOffset, validityOffset, resolvedOffset, historyWrittenOffset;
	uint64_t totalBytes;
} vkTemporalResolveReadbackGeometry_t;

#define VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES 14u
#define VK_TEMPORAL_RESOLVE_READBACK_PROTECTED 15u

static qboolean ProductProtectedSet(
		const vkTemporalResolveProductView_t *p, const void **out ) {
	uint32_t i, j;
	if ( !p || !out || !p->backend || !p->currentColor || !p->currentDepth
			|| !p->previousColor || !p->previousColorView || !p->previousDepth
			|| !p->previousDepthView || !p->velocity || !p->velocityView
			|| !p->validity || !p->validityView || !p->resolvedTarget
			|| !p->resolvedTargetView || !p->historyWrittenColor ) return qfalse;
	out[0] = p->backend; out[1] = p->currentColor; out[2] = p->currentDepth;
	out[3] = p->previousColor; out[4] = p->previousColorView;
	out[5] = p->previousDepth; out[6] = p->previousDepthView;
	out[7] = p->velocity; out[8] = p->velocityView;
	out[9] = p->validity; out[10] = p->validityView;
	out[11] = p->resolvedTarget; out[12] = p->resolvedTargetView;
	out[13] = p->historyWrittenColor;
	out[14] = NULL;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++i )
		for ( j = i + 1u; j < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++j )
		if ( out[i] == out[j] ) return qfalse;
	return qtrue;
}

static qboolean ProductExact( const vkTemporalResolveProductView_t *a,
		const vkTemporalResolveProductView_t *b ) {
	return a && b && a->backend == b->backend
		&& a->currentColor == b->currentColor && a->currentDepth == b->currentDepth
		&& a->previousColor == b->previousColor
		&& a->previousColorView == b->previousColorView
		&& a->previousDepth == b->previousDepth
		&& a->previousDepthView == b->previousDepthView
		&& a->velocity == b->velocity && a->velocityView == b->velocityView
		&& a->validity == b->validity && a->validityView == b->validityView
		&& a->resolvedTarget == b->resolvedTarget
		&& a->resolvedTargetView == b->resolvedTargetView
		&& a->historyWrittenColor == b->historyWrittenColor ? qtrue : qfalse;
}

static uint64_t Align16( uint64_t value ) {
	return ( value + 15u ) & ~15ull;
}

static uint32_t DepthBytes(
		vkTemporalResolveReadbackDepthEncoding_t encoding ) {
	switch ( encoding ) {
	case VK_TEMPORAL_RESOLVE_DEPTH_D16:
	case VK_TEMPORAL_RESOLVE_DEPTH_D16_S8: return 2u;
	case VK_TEMPORAL_RESOLVE_DEPTH_X8_D24:
	case VK_TEMPORAL_RESOLVE_DEPTH_D24_S8:
	case VK_TEMPORAL_RESOLVE_DEPTH_D32:
	case VK_TEMPORAL_RESOLVE_DEPTH_D32_S8: return 4u;
	default: return 0u;
	}
}

static qboolean BuildGeometry( uint32_t width, uint32_t height,
		vkTemporalResolveReadbackDepthEncoding_t depthEncoding,
		vkTemporalResolveReadbackGeometry_t *out ) {
	vkTemporalResolveReadbackGeometry_t g;
	uint32_t coreW, coreH, captureW, captureH, depthBytes, apronW, apronH;
	uint64_t pixels, cursor;
	if ( !width || !height || width > INT32_MAX || height > INT32_MAX
			|| !out || !( depthBytes = DepthBytes( depthEncoding ) ) )
		return qfalse;
	coreW = width > VK_TEMPORAL_RESOLVE_READBACK_CORE_MAX
		? VK_TEMPORAL_RESOLVE_READBACK_CORE_MAX : width;
	coreH = height > VK_TEMPORAL_RESOLVE_READBACK_CORE_MAX
		? VK_TEMPORAL_RESOLVE_READBACK_CORE_MAX : height;
	/* The CPU oracle samples the previous frame at motion-reprojected texels.
	 * A fixed four-pixel apron made every pixel of the deterministic IQM witness
	 * unverifiable on HiDPI targets: its valid 0.116-UV motion lands roughly 296
	 * pixels away at 2560-wide.  Capture one quarter of each axis around the core,
	 * clamped to a diagnostic-only 512-pixel bound, so the supported footprint
	 * scales with the render target without becoming an unbounded full-frame
	 * readback. */
	apronW = width / VK_TEMPORAL_RESOLVE_READBACK_APRON_DIVISOR;
	apronH = height / VK_TEMPORAL_RESOLVE_READBACK_APRON_DIVISOR;
	if ( apronW < VK_TEMPORAL_RESOLVE_READBACK_APRON_MIN )
		apronW = VK_TEMPORAL_RESOLVE_READBACK_APRON_MIN;
	if ( apronH < VK_TEMPORAL_RESOLVE_READBACK_APRON_MIN )
		apronH = VK_TEMPORAL_RESOLVE_READBACK_APRON_MIN;
	if ( apronW > VK_TEMPORAL_RESOLVE_READBACK_APRON_MAX )
		apronW = VK_TEMPORAL_RESOLVE_READBACK_APRON_MAX;
	if ( apronH > VK_TEMPORAL_RESOLVE_READBACK_APRON_MAX )
		apronH = VK_TEMPORAL_RESOLVE_READBACK_APRON_MAX;
	captureW = coreW + 2u * apronW;
	captureH = coreH + 2u * apronH;
	if ( captureW > width ) captureW = width;
	if ( captureH > height ) captureH = height;
	memset( &g, 0, sizeof( g ) );
	g.captureWidth = captureW; g.captureHeight = captureH;
	g.captureX = ( width - captureW ) / 2u;
	g.captureY = ( height - captureH ) / 2u;
	g.coreWidth = coreW; g.coreHeight = coreH;
	g.coreX = ( width - coreW ) / 2u;
	g.coreY = ( height - coreH ) / 2u;
	pixels = (uint64_t)captureW * (uint64_t)captureH;
	if ( !pixels || pixels > UINT64_MAX / 64u ) return qfalse;
	cursor = 0u;
	g.currentColorOffset = cursor; cursor = Align16( cursor + pixels * 8u );
	g.currentDepthOffset = cursor; cursor = Align16( cursor + pixels * depthBytes );
	g.previousColorOffset = cursor; cursor = Align16( cursor + pixels * 8u );
	g.previousDepthOffset = cursor; cursor = Align16( cursor + pixels * 4u );
	g.velocityOffset = cursor; cursor = Align16( cursor + pixels * 4u );
	g.validityOffset = cursor; cursor = Align16( cursor + pixels );
	g.resolvedOffset = cursor; cursor = Align16( cursor + pixels * 8u );
	g.historyWrittenOffset = cursor; cursor = Align16( cursor + pixels * 8u );
	g.totalBytes = cursor;
	*out = g;
	return qtrue;
}

static qboolean TicketMatchesSlot( const vkTemporalResolveReadbackSlot_t *slot,
		uint32_t frameIndex, uint32_t frameCount,
		const vkTemporalResolveReadbackTicket_t *ticket ) {
	vkTemporalResolveReadbackGeometry_t g;
	if ( !slot || !ticket || ticket->commandSlot != frameIndex
			|| ticket->resolve.authority.commandSlot != frameIndex
			|| !ticket->captureSerial
			|| ticket->resolve.authority.frameCount != frameCount
			|| ticket->resolve.content.commandSlot != frameIndex
			|| ticket->resolve.content.frameCount != frameCount
			|| ticket->bufferAllocationGeneration != slot->allocationGeneration
			|| ticket->buffer != slot->buffer
			|| ticket->resolve.authority.width != slot->width
			|| ticket->resolve.authority.height != slot->height
			|| ticket->currentDepthEncoding != slot->depthEncoding
			|| !BuildGeometry( slot->width, slot->height, slot->depthEncoding, &g ) )
		return qfalse;
	return ticket->captureX == g.captureX && ticket->captureY == g.captureY
		&& ticket->captureWidth == g.captureWidth
		&& ticket->captureHeight == g.captureHeight
		&& ticket->coreX == g.coreX && ticket->coreY == g.coreY
		&& ticket->coreWidth == g.coreWidth && ticket->coreHeight == g.coreHeight
		&& ticket->currentColorOffset == g.currentColorOffset
		&& ticket->currentDepthOffset == g.currentDepthOffset
		&& ticket->previousColorOffset == g.previousColorOffset
		&& ticket->previousDepthOffset == g.previousDepthOffset
		&& ticket->velocityOffset == g.velocityOffset
		&& ticket->validityOffset == g.validityOffset
		&& ticket->resolvedOffset == g.resolvedOffset
		&& ticket->historyWrittenOffset == g.historyWrittenOffset
		&& ticket->totalBytes == g.totalBytes && g.totalBytes <= slot->bytes
		? qtrue : qfalse;
}

static qboolean TransitionReadback( ralCommandBuffer_t *commandBuffer,
		ralBuffer_t *buffer, uint64_t bytes, ralResourceUsage_t before,
		ralResourceUsage_t after ) {
	ralBufferTransition_t transition;
	ralResourceTransitionBatch_t batch;
	if ( !commandBuffer || !buffer || !bytes ) return qfalse;
	memset( &transition, 0, sizeof( transition ) );
	transition.buffer = buffer; transition.size = bytes;
	transition.before.usage = before; transition.after.usage = after;
	transition.sourceQueue = transition.destinationQueue = RAL_QUEUE_GRAPHICS;
	memset( &batch, 0, sizeof( batch ) );
	batch.bufferTransitions = &transition; batch.bufferTransitionCount = 1u;
	return Ral_CmdTransitionResources( commandBuffer, &batch ) == ralSuccess
		? qtrue : qfalse;
}

void VK_TemporalResolveReadbackInit( vkTemporalResolveReadbackOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalResolveReadbackArm(
		vkTemporalResolveReadbackOwner_t *owner, uint32_t captureCount ) {
	uint32_t i;
	if ( !owner || !captureCount
			|| captureCount > VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES ) return qfalse;
	if ( !owner->initialized ) VK_TemporalResolveReadbackInit( owner );
	if ( owner->capturesRemaining ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].state == VK_TEMPORAL_RESOLVE_READBACK_RECORDED
				|| owner->slots[i].state == VK_TEMPORAL_RESOLVE_READBACK_SUBMITTED )
			return qfalse;
	owner->capturesRemaining = captureCount;
	return qtrue;
}

qboolean VK_TemporalResolveReadbackIsArmed(
		const vkTemporalResolveReadbackOwner_t *owner ) {
	return owner && owner->initialized && owner->capturesRemaining ? qtrue : qfalse;
}

qboolean VK_TemporalResolveReadbackPrepareAfterFence(
		vkTemporalResolveReadbackOwner_t *owner, ralBackend_t *backend,
		uint32_t frameCount, uint32_t frameIndex, uint32_t width, uint32_t height,
		vkTemporalResolveReadbackDepthEncoding_t depthEncoding,
		const vkTemporalResolveProductView_t *protectedProducts ) {
	vkTemporalResolveReadbackSlot_t *slot;
	vkTemporalResolveReadbackGeometry_t g;
	ralBufferCreateInfo_t bci;
	ralBuffer_t *candidate;
	uint32_t i;
	const void *protectedResources[VK_TEMPORAL_RESOLVE_READBACK_PROTECTED];
	uint32_t j;
	if ( !owner || !owner->initialized || !owner->capturesRemaining || !backend
			|| !frameCount || frameCount > VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES
			|| frameIndex >= frameCount
			|| !BuildGeometry( width, height, depthEncoding, &g )
			|| !protectedProducts || protectedProducts->backend != backend
			|| !ProductProtectedSet( protectedProducts, protectedResources ) )
		return qfalse;
	if ( owner->backend && owner->backend != backend ) return qfalse;
	if ( owner->frameCount && owner->frameCount != frameCount ) return qfalse;
	slot = &owner->slots[frameIndex];
	// Every live readback buffer is protected against every other slot and the
	// complete borrowed H3 product cohort.
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++i ) {
		const void *role = owner->slots[i].buffer;
		if ( role ) {
			for ( j = 0; j < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++j )
				if ( role == protectedResources[j] ) return qfalse;
			for ( uint32_t k = i + 1u;
					k < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++k ) {
				if ( role == (const void *)owner->slots[k].buffer ) return qfalse;
			}
		}
	}
	if ( slot->state == VK_TEMPORAL_RESOLVE_READBACK_RECORDED
			|| slot->state == VK_TEMPORAL_RESOLVE_READBACK_SUBMITTED ) return qfalse;
	if ( slot->buffer && slot->bytes == g.totalBytes
			&& slot->width == width && slot->height == height
			&& slot->depthEncoding == depthEncoding ) {
		for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++i )
			if ( (const void *)slot->buffer == protectedResources[i] ) return qfalse;
		memset( &slot->ticket, 0, sizeof( slot->ticket ) );
		slot->products = *protectedProducts;
		slot->state = VK_TEMPORAL_RESOLVE_READBACK_READY;
		owner->backend = backend; owner->frameCount = frameCount;
		return qtrue;
	}
	if ( slot->allocationGeneration == UINT32_MAX ) return qfalse;
	memset( &bci, 0, sizeof( bci ) );
	bci.size = g.totalBytes;
	bci.usage = RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ;
	bci.memory = RAL_MEMORY_HOST_COHERENT;
	bci.debugName = "wired-temporal-resolve-readback";
	candidate = Ral_CreateBuffer( backend, &bci );
	if ( !candidate ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].buffer == candidate ) {
			// The allocator returned a borrowed live handle.  We do not own it
			// and therefore must neither publish nor destroy it.
			return qfalse;
		}
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++i )
		if ( (const void *)candidate == protectedResources[i] ) return qfalse;
	if ( slot->buffer ) Ral_DestroyBuffer( slot->buffer );
	slot->buffer = candidate; slot->bytes = g.totalBytes;
	slot->width = width; slot->height = height; slot->depthEncoding = depthEncoding;
	slot->products = *protectedProducts;
	slot->allocationGeneration++;
	slot->hostReadable = qfalse;
	memset( &slot->ticket, 0, sizeof( slot->ticket ) );
	slot->state = VK_TEMPORAL_RESOLVE_READBACK_READY;
	owner->backend = backend; owner->frameCount = frameCount;
	return qtrue;
}

static qboolean CopyRect( ralCommandBuffer_t *cb, ralTexture_t *texture,
		ralBuffer_t *buffer, uint64_t offset,
		ralTextureAspectFlags_t aspects,
		const vkTemporalResolveReadbackGeometry_t *g ) {
	ralBufferTextureCopy_t copy;
	if ( !cb || !texture || !buffer || !g
			|| (aspects != RAL_TEXTURE_ASPECT_COLOR
				&& aspects != RAL_TEXTURE_ASPECT_DEPTH) ) return qfalse;
	memset( &copy, 0, sizeof( copy ) );
	copy.bufferOffset = offset;
	copy.aspects = aspects;
	copy.imageRect.x = (int32_t)g->captureX;
	copy.imageRect.y = (int32_t)g->captureY;
	copy.imageRect.width = g->captureWidth;
	copy.imageRect.height = g->captureHeight;
	return Ral_CmdCopyTextureToBuffer( cb, texture, buffer, &copy );
}

static void RestoreCopiedProducts( ralCommandBuffer_t *commandBuffer,
		const vkTemporalResolveProductView_t *products ) {
	Ral_CmdTransitionTexture( commandBuffer, products->currentColor,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->previousColor,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->previousDepth,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->velocity,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->validity,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->resolvedTarget,
		RAL_PIPELINE_STAGE_TRANSFER_BIT,
		RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	// The resolve leaves the history it wrote in GENERAL; the next frame's
	// resolve transitions it for sampling from there.
	Ral_CmdTransitionTexture( commandBuffer, products->historyWrittenColor,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_GENERAL );
}

qboolean VK_TemporalResolveReadbackRecord(
		vkTemporalResolveReadbackOwner_t *owner, ralCommandBuffer_t *commandBuffer,
		uint32_t frameIndex, const vkTemporalResolveTicket_t *recordedResolve,
		const vkTemporalResolveProductView_t *products,
		vkTemporalResolveReadbackDepthCopyFn depthCopy, void *depthCopyUser ) {
	vkTemporalResolveReadbackSlot_t *slot;
	vkTemporalResolveReadbackGeometry_t g;
	vkTemporalResolveReadbackTicket_t ticket;
	ralMemoryBarrier_t memory;
	ralPipelineBarrierInfo_t barrier;
	uint32_t readIndex;
	const void *protectedResources[VK_TEMPORAL_RESOLVE_READBACK_PROTECTED];
	uint32_t i, j;
	if ( !owner || !owner->initialized || !owner->capturesRemaining
			|| !commandBuffer || frameIndex >= owner->frameCount
			|| !recordedResolve
			|| !VK_TemporalResolveTicketValidateRecordedExact( recordedResolve )
			|| !products || !depthCopy
			|| recordedResolve->authority.commandSlot != frameIndex
			|| recordedResolve->push.extent[0] != recordedResolve->authority.width
			|| recordedResolve->push.extent[1] != recordedResolve->authority.height
			|| recordedResolve->push.historyReadIndex !=
				recordedResolve->authority.historyReadIndex
			|| !ProductExact( products, &recordedResolve->products )
			|| !ProductProtectedSet( products, protectedResources )
			|| recordedResolve->content.backend != products->backend
			|| recordedResolve->content.sourceSceneColor != products->currentColor
			|| recordedResolve->content.target != products->resolvedTarget
			|| recordedResolve->content.targetView != products->resolvedTargetView )
		return qfalse;
	readIndex = recordedResolve->authority.historyReadIndex;
	if ( readIndex > 1u || products->currentColor == products->currentDepth
			|| products->previousColor == products->previousDepth
			|| products->velocity == products->validity
			|| products->resolvedTarget == products->currentColor ) return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_RESOLVE_READBACK_READY || !slot->buffer
			|| owner->nextCaptureSerial == UINT32_MAX
			|| !BuildGeometry( slot->width, slot->height,
				slot->depthEncoding, &g )
			|| slot->width != recordedResolve->authority.width
			|| slot->height != recordedResolve->authority.height
			|| !ProductExact( products, &slot->products ) ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++i )
		if ( protectedResources[i] == (const void *)slot->buffer ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++i )
		for ( j = i + 1u; j < VK_TEMPORAL_RESOLVE_READBACK_PRODUCT_ROLES; ++j )
		if ( protectedResources[i] == protectedResources[j] ) return qfalse;
	memset( &ticket, 0, sizeof( ticket ) );
	ticket.resolve = *recordedResolve;
	ticket.commandSlot = frameIndex;
	ticket.captureSerial = owner->nextCaptureSerial + 1u;
	ticket.bufferAllocationGeneration = slot->allocationGeneration;
	ticket.buffer = slot->buffer;
	ticket.captureX = g.captureX; ticket.captureY = g.captureY;
	ticket.captureWidth = g.captureWidth; ticket.captureHeight = g.captureHeight;
	ticket.coreX = g.coreX; ticket.coreY = g.coreY;
	ticket.coreWidth = g.coreWidth; ticket.coreHeight = g.coreHeight;
	ticket.currentDepthEncoding = slot->depthEncoding;
	ticket.currentColorOffset = g.currentColorOffset;
	ticket.currentDepthOffset = g.currentDepthOffset;
	ticket.previousColorOffset = g.previousColorOffset;
	ticket.previousDepthOffset = g.previousDepthOffset;
	ticket.velocityOffset = g.velocityOffset;
	ticket.validityOffset = g.validityOffset;
	ticket.resolvedOffset = g.resolvedOffset;
	ticket.historyWrittenOffset = g.historyWrittenOffset;
	ticket.totalBytes = g.totalBytes;

	// The destination must enter COPY_DESTINATION before the platform records
	// any depth/image copy. The callback still precedes mutations of the six
	// borrowed typed textures and restores its own depth source.
	if ( !TransitionReadback( commandBuffer, slot->buffer, slot->bytes,
			slot->hostReadable ? RAL_RESOURCE_USAGE_HOST_READ
			                   : RAL_RESOURCE_USAGE_UNDEFINED,
			RAL_RESOURCE_USAGE_COPY_DESTINATION ) ) return qfalse;
	if ( !depthCopy( commandBuffer, products->currentDepth, slot->buffer,
			g.currentDepthOffset, g.captureX, g.captureY,
			g.captureWidth, g.captureHeight, slot->depthEncoding,
			depthCopyUser ) ) {
		TransitionReadback( commandBuffer, slot->buffer, slot->bytes,
			RAL_RESOURCE_USAGE_COPY_DESTINATION,
			slot->hostReadable ? RAL_RESOURCE_USAGE_HOST_READ
			                   : RAL_RESOURCE_USAGE_UNDEFINED );
		return qfalse;
	}
	Ral_CmdTransitionTexture( commandBuffer, products->currentColor,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RAL_PIPELINE_STAGE_TRANSFER_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->previousColor,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RAL_PIPELINE_STAGE_TRANSFER_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->previousDepth,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RAL_PIPELINE_STAGE_TRANSFER_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->velocity,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, RAL_PIPELINE_STAGE_TRANSFER_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->validity,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, RAL_PIPELINE_STAGE_TRANSFER_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->resolvedTarget,
		RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, products->historyWrittenColor,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RAL_PIPELINE_STAGE_TRANSFER_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	if ( !CopyRect( commandBuffer, products->currentColor, slot->buffer,
			g.currentColorOffset, RAL_TEXTURE_ASPECT_COLOR, &g )
			|| !CopyRect( commandBuffer, products->previousColor, slot->buffer,
				g.previousColorOffset, RAL_TEXTURE_ASPECT_COLOR, &g )
			|| !CopyRect( commandBuffer, products->previousDepth, slot->buffer,
				g.previousDepthOffset, RAL_TEXTURE_ASPECT_COLOR, &g )
			|| !CopyRect( commandBuffer, products->velocity, slot->buffer,
				g.velocityOffset, RAL_TEXTURE_ASPECT_COLOR, &g )
			|| !CopyRect( commandBuffer, products->validity, slot->buffer,
				g.validityOffset, RAL_TEXTURE_ASPECT_COLOR, &g )
			|| !CopyRect( commandBuffer, products->resolvedTarget, slot->buffer,
				g.resolvedOffset, RAL_TEXTURE_ASPECT_COLOR, &g )
			|| !CopyRect( commandBuffer, products->historyWrittenColor, slot->buffer,
				g.historyWrittenOffset, RAL_TEXTURE_ASPECT_COLOR, &g ) ) {
		RestoreCopiedProducts( commandBuffer, products );
		(void)TransitionReadback( commandBuffer, slot->buffer, slot->bytes,
			RAL_RESOURCE_USAGE_COPY_DESTINATION,
			slot->hostReadable ? RAL_RESOURCE_USAGE_HOST_READ
			                   : RAL_RESOURCE_USAGE_UNDEFINED );
		return qfalse;
	}
	memset( &memory, 0, sizeof( memory ) );
	memory.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memory.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	memset( &barrier, 0, sizeof( barrier ) );
	barrier.srcStageMask = RAL_PIPELINE_STAGE_TRANSFER_BIT;
	barrier.dstStageMask = RAL_PIPELINE_STAGE_HOST_BIT;
	barrier.memoryBarrierCount = 1; barrier.memoryBarriers = &memory;
	Ral_CmdPipelineBarrierFull( commandBuffer, &barrier );
	if ( !TransitionReadback( commandBuffer, slot->buffer, slot->bytes,
			RAL_RESOURCE_USAGE_COPY_DESTINATION,
			RAL_RESOURCE_USAGE_HOST_READ ) ) {
		RestoreCopiedProducts( commandBuffer, products );
		return qfalse;
	}
	RestoreCopiedProducts( commandBuffer, products );
	owner->nextCaptureSerial = ticket.captureSerial;
	slot->ticket = ticket; slot->hostReadable = qtrue;
	slot->state = VK_TEMPORAL_RESOLVE_READBACK_RECORDED;
	return qtrue;
}

qboolean VK_TemporalResolveReadbackBindStoreExpected(
		vkTemporalResolveReadbackOwner_t *owner, uint32_t frameIndex,
		const vkTemporalResolveTicket_t *boundResolve ) {
	vkTemporalResolveReadbackSlot_t *slot;
	vkTemporalResolveTicket_t expected;
	uint32_t storeGeneration;
	if ( !owner || !owner->initialized || frameIndex >= owner->frameCount
			|| !boundResolve || !boundResolve->recorded
			|| boundResolve->submitted || boundResolve->content.submitted
			|| !VK_TemporalResolveTicketValidateRecordedExact( boundResolve ) )
		return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_RESOLVE_READBACK_RECORDED
			|| !TicketMatchesSlot( slot, frameIndex, owner->frameCount,
				&slot->ticket )
			|| slot->ticket.resolve.committedWriteExpected.source.
				storeOwnerAllocationGeneration ) return qfalse;
	storeGeneration = boundResolve->committedWriteExpected.source.
		storeOwnerAllocationGeneration;
	if ( !storeGeneration ) return qfalse;
	expected = slot->ticket.resolve;
	expected.committedWriteExpected.source.storeOwnerAllocationGeneration =
		storeGeneration;
	if ( !VK_TemporalResolveTicketEqualExact( &expected, boundResolve ) )
		return qfalse;
	slot->ticket.resolve = *boundResolve;
	return qtrue;
}

qboolean VK_TemporalResolveReadbackResolveSubmit(
		vkTemporalResolveReadbackOwner_t *owner, uint32_t frameIndex,
		qboolean submitted, const vkTemporalResolveTicket_t *submittedResolve,
		vkTemporalResolveReadbackTicket_t *outTicket ) {
	vkTemporalResolveReadbackSlot_t *slot;
	vkTemporalResolveTicket_t expected;
	if ( !owner || !owner->initialized || frameIndex >= owner->frameCount )
		return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_RESOLVE_READBACK_RECORDED ) return qfalse;
	if ( !TicketMatchesSlot( slot, frameIndex, owner->frameCount,
			&slot->ticket ) ) {
		memset( &slot->ticket, 0, sizeof( slot->ticket ) );
		slot->state = VK_TEMPORAL_RESOLVE_READBACK_READY;
		return qfalse;
	}
	expected = slot->ticket.resolve;
	expected.content.submitted = qtrue;
	expected.submitted = qtrue;
	if ( !submitted ) {
		memset( &slot->ticket, 0, sizeof( slot->ticket ) );
		slot->state = VK_TEMPORAL_RESOLVE_READBACK_READY;
		return qtrue;
	}
	if ( !submittedResolve
			|| !VK_TemporalResolveTicketEqualExact( &expected, submittedResolve ) ) {
		memset( &slot->ticket, 0, sizeof( slot->ticket ) );
		slot->state = VK_TEMPORAL_RESOLVE_READBACK_READY;
		return qfalse;
	}
	slot->ticket.resolve = *submittedResolve;
	slot->ticket.submitted = qtrue;
	slot->state = VK_TEMPORAL_RESOLVE_READBACK_SUBMITTED;
	if ( owner->capturesRemaining ) owner->capturesRemaining--;
	if ( outTicket ) *outTicket = slot->ticket;
	return qtrue;
}

static uint64_t HashBytes( const unsigned char *p, uint64_t bytes ) {
	uint64_t hash = 1469598103934665603ull, i;
	for ( i = 0; i < bytes; ++i ) { hash ^= p[i]; hash *= 1099511628211ull; }
	return hash;
}

static float HalfToFloat( uint16_t h ) {
	const uint32_t sign = (uint32_t)( h & 0x8000u ) << 16;
	uint32_t exp = ( h >> 10 ) & 0x1fu, mant = h & 0x03ffu, bits;
	float value;
	if ( exp == 0 ) {
		if ( mant == 0 ) bits = sign;
		else {
			int shift = 0;
			while ( ( mant & 0x0400u ) == 0 ) { mant <<= 1; ++shift; }
			mant &= 0x03ffu;
			bits = sign | (uint32_t)( 113 - shift ) << 23 | mant << 13;
		}
	} else if ( exp == 31 ) bits = sign | 0x7f800000u | mant << 13;
	else bits = sign | ( exp + 112u ) << 23 | mant << 13;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}

float VK_TemporalResolveReadbackDecodeDepth( const void *bytes,
		vkTemporalResolveReadbackDepthEncoding_t encoding ) {
	const unsigned char *p = (const unsigned char *)bytes;
	uint16_t u16;
	uint32_t u32;
	float f;
	switch ( encoding ) {
	case VK_TEMPORAL_RESOLVE_DEPTH_D16:
	case VK_TEMPORAL_RESOLVE_DEPTH_D16_S8:
		memcpy( &u16, p, sizeof( u16 ) ); return (float)u16 / 65535.0f;
	case VK_TEMPORAL_RESOLVE_DEPTH_X8_D24:
	case VK_TEMPORAL_RESOLVE_DEPTH_D24_S8:
		memcpy( &u32, p, sizeof( u32 ) ); return (float)( u32 & 0x00ffffffu ) / 16777215.0f;
	case VK_TEMPORAL_RESOLVE_DEPTH_D32:
	case VK_TEMPORAL_RESOLVE_DEPTH_D32_S8:
		memcpy( &f, p, sizeof( f ) ); return f;
	default: return NAN;
	}
}

static qboolean LocalIndex( const vkTemporalResolveReadbackTicket_t *t,
		int32_t x, int32_t y, uint32_t *out ) {
	if ( !t || !out || x < (int32_t)t->captureX || y < (int32_t)t->captureY
			|| x >= (int32_t)( t->captureX + t->captureWidth )
			|| y >= (int32_t)( t->captureY + t->captureHeight ) ) return qfalse;
	*out = (uint32_t)( y - (int32_t)t->captureY ) * t->captureWidth
		+ (uint32_t)( x - (int32_t)t->captureX );
	return qtrue;
}

static void ReadHalf4( const unsigned char *base, uint32_t index,
		float out[4], uint16_t bits[4] ) {
	uint32_t c;
	for ( c = 0; c < 4; ++c ) {
		memcpy( &bits[c], base + ( (uint64_t)index * 4u + c ) * 2u, 2u );
		out[c] = HalfToFloat( bits[c] );
	}
}

uint16_t VK_TemporalResolveReadbackFloatToHalfRne( float value ) {
	uint32_t bits, sign, exponent, mantissa, halfMantissa, remainder, halfway;
	int32_t halfExponent, shift;
	memcpy( &bits, &value, sizeof( bits ) );
	sign = ( bits >> 16 ) & 0x8000u;
	exponent = ( bits >> 23 ) & 0xffu;
	mantissa = bits & 0x7fffffu;
	if ( exponent == 0xffu )
		return (uint16_t)( sign | ( mantissa ? 0x7e00u : 0x7c00u ) );
	halfExponent = (int32_t)exponent - 127 + 15;
	if ( halfExponent >= 31 ) return (uint16_t)( sign | 0x7c00u );
	if ( halfExponent <= 0 ) {
		if ( halfExponent < -10 ) return (uint16_t)sign;
		mantissa |= 0x800000u;
		shift = 14 - halfExponent;
		halfMantissa = mantissa >> shift;
		remainder = mantissa & ( ( 1u << shift ) - 1u );
		halfway = 1u << ( shift - 1 );
		if ( remainder > halfway
				|| ( remainder == halfway && ( halfMantissa & 1u ) ) )
			halfMantissa++;
		return (uint16_t)( sign | halfMantissa );
	}
	halfMantissa = mantissa >> 13;
	remainder = mantissa & 0x1fffu;
	if ( remainder > 0x1000u
			|| ( remainder == 0x1000u && ( halfMantissa & 1u ) ) ) {
		halfMantissa++;
		if ( halfMantissa == 0x400u ) {
			halfMantissa = 0; halfExponent++;
			if ( halfExponent >= 31 ) return (uint16_t)( sign | 0x7c00u );
		}
	}
	return (uint16_t)( sign | (uint32_t)halfExponent << 10 | halfMantissa );
}

qboolean VK_TemporalResolveReadbackHalfWithinOneStep(
		uint16_t actualBits, float expected ) {
	uint16_t expectedBits;
	uint32_t actualOrdered, expectedOrdered, distance;
	if ( !isfinite( expected ) || ( actualBits & 0x7c00u ) == 0x7c00u )
		return qfalse;
	expectedBits = VK_TemporalResolveReadbackFloatToHalfRne( expected );
	actualOrdered = ( actualBits & 0x8000u )
		? 0x8000u - ( actualBits & 0x7fffu ) : 0x8000u + actualBits;
	expectedOrdered = ( expectedBits & 0x8000u )
		? 0x8000u - ( expectedBits & 0x7fffu ) : 0x8000u + expectedBits;
	distance = actualOrdered > expectedOrdered
		? actualOrdered - expectedOrdered : expectedOrdered - actualOrdered;
	return distance <= 1u ? qtrue : qfalse;
}

static uint32_t HalfOrderedDistance( uint16_t a, uint16_t b ) {
	uint32_t ao = ( a & 0x8000u )
		? 0x8000u - ( a & 0x7fffu ) : 0x8000u + a;
	uint32_t bo = ( b & 0x8000u )
		? 0x8000u - ( b & 0x7fffu ) : 0x8000u + b;
	return ao > bo ? ao - bo : bo - ao;
}

static void VerifyZeroFallback( vkTemporalResolveReadbackContentReceipt_t *r,
		const uint16_t actual[4] ) {
	static const uint16_t zero[4] = { 0u, 0u, 0u, 0u };
	r->zeroFallbackExpected++;
	if ( memcmp( zero, actual, sizeof( zero ) ) == 0 ) r->zeroFallbackExact++;
	else r->mismatches++;
}

void VK_TemporalResolveReadbackPreviousTexel(
		const uint32_t pixel[2], const uint32_t extent[2], const float velocity[2],
		const float previousJitter[2], const float currentJitter[2], float out[2] ) {
	volatile float jitterDelta[2], jitterTexel[2], motionTexel[2];
	volatile float unjittered[2], result[2];
	if ( !pixel || !extent || !velocity || !previousJitter || !currentJitter || !out )
		return;
	jitterDelta[0] = previousJitter[0] - currentJitter[0];
	jitterDelta[1] = previousJitter[1] - currentJitter[1];
	jitterTexel[0] = jitterDelta[0] * (float)extent[0];
	jitterTexel[1] = jitterDelta[1] * (float)extent[1];
	motionTexel[0] = velocity[0] * (float)extent[0];
	motionTexel[1] = velocity[1] * (float)extent[1];
	unjittered[0] = (float)pixel[0] - motionTexel[0];
	unjittered[1] = (float)pixel[1] - motionTexel[1];
	result[0] = unjittered[0] + jitterTexel[0];
	result[1] = unjittered[1] + jitterTexel[1];
	out[0] = result[0]; out[1] = result[1];
}

qboolean VK_TemporalResolveReadbackCompleteAfterFence(
		vkTemporalResolveReadbackOwner_t *owner, uint32_t frameIndex,
		qboolean fenceProven, vkTemporalResolveReadbackContentReceipt_t *outReceipt ) {
	vkTemporalResolveReadbackSlot_t *slot;
	vkTemporalResolveReadbackContentReceipt_t r;
	const vkTemporalResolveReadbackTicket_t *t;
	vkTemporalResolveTicket_t canonicalRecorded;
	const unsigned char *bytes, *current, *depth, *previous, *previousDepth;
	const unsigned char *velocity, *validity, *resolved, *historyWritten;
	ralBufferMapRequest_t mapRequest;
	ralBufferMapTicket_t mapTicket;
	uint64_t pixels;
	uint32_t x, y, depthBytes;
	if ( !owner || !owner->initialized || !fenceProven
			|| frameIndex >= owner->frameCount ) return qfalse;
	slot = &owner->slots[frameIndex]; t = &slot->ticket;
	if ( slot->state != VK_TEMPORAL_RESOLVE_READBACK_SUBMITTED || !slot->hostReadable
			|| !t->submitted || !t->resolve.submitted
			|| !t->resolve.content.submitted
			|| !TicketMatchesSlot( slot, frameIndex, owner->frameCount, t ) )
		return qfalse;
	canonicalRecorded = t->resolve;
	canonicalRecorded.submitted = qfalse;
	canonicalRecorded.content.submitted = qfalse;
	if ( !VK_TemporalResolveTicketValidateRecordedExact( &canonicalRecorded )
			|| !ProductExact( &t->resolve.products, &slot->products )
			|| owner->backend != slot->products.backend ) return qfalse;
	memset( &mapRequest, 0, sizeof( mapRequest ) );
	mapRequest.mode = RAL_MAP_READ; mapRequest.size = slot->bytes;
	if ( Ral_BufferMapBegin( slot->buffer, &mapRequest, &mapTicket ) != ralSuccess
			|| !mapTicket.mappedRange ) return qfalse;
	memset( &r, 0, sizeof( r ) ); r.ticket = *t;
	bytes = (const unsigned char *)mapTicket.mappedRange;
	current = bytes + t->currentColorOffset; depth = bytes + t->currentDepthOffset;
	previous = bytes + t->previousColorOffset;
	previousDepth = bytes + t->previousDepthOffset;
	velocity = bytes + t->velocityOffset; validity = bytes + t->validityOffset;
	resolved = bytes + t->resolvedOffset;
	historyWritten = bytes + t->historyWrittenOffset;
	pixels = (uint64_t)t->captureWidth * t->captureHeight;
	depthBytes = DepthBytes( t->currentDepthEncoding );
	r.currentColorHash = HashBytes( current, pixels * 8u );
	r.currentDepthHash = HashBytes( depth, pixels * depthBytes );
	r.previousColorHash = HashBytes( previous, pixels * 8u );
	r.previousDepthHash = HashBytes( previousDepth, pixels * 4u );
	r.velocityHash = HashBytes( velocity, pixels * 4u );
	r.validityHash = HashBytes( validity, pixels );
	r.resolvedHash = HashBytes( resolved, pixels * 8u );
	r.historyWrittenHash = HashBytes( historyWritten, pixels * 8u );
	// The exact ticket, full-plane copy command inventory, submitted receipt and
	// completed slot fence are the population authority. Content validity remains
	// independently fail-closed below; zero is a legitimate value for every plane.
	r.planesPopulated = qtrue;
	r.centerX = t->resolve.authority.width / 2u;
	r.centerY = t->resolve.authority.height / 2u;
	{
		uint32_t centerIndex;
		if ( !LocalIndex( t, (int32_t)r.centerX, (int32_t)r.centerY,
				&centerIndex ) ) {
			Ral_BufferMapUnmap( slot->buffer, &mapTicket );
			return qfalse;
		}
		memcpy( r.centerCurrentColor,
			current + (uint64_t)centerIndex * 8u,
			sizeof( r.centerCurrentColor ) );
		memcpy( r.centerResolvedColor,
			resolved + (uint64_t)centerIndex * 8u,
			sizeof( r.centerResolvedColor ) );
		memcpy( r.centerVelocity,
			velocity + (uint64_t)centerIndex * 4u,
			sizeof( r.centerVelocity ) );
		memcpy( &r.centerDepthRaw,
			depth + (uint64_t)centerIndex * depthBytes, depthBytes );
		r.centerValidity = validity[centerIndex];
	}
	for ( uint64_t vi = 0; vi < pixels; ++vi ) {
		if ( validity[vi] == 0u ) r.validityZero++;
		else if ( validity[vi] == 255u ) r.validityFull++;
		else r.validityOther++;
	}
	r.corePixels = t->coreWidth * t->coreHeight;
	{
		// The current-resolve oracle (tr_temporal_resolve_oracle): every core pixel's displayed colour and written
		// history must lie in the interval the CPU model of temporal_resolve.comp predicts. A pixel the model cannot
		// decide (a read outside the capture, or a branch that flips within the hardware filter's error) counts as
		// unsupported; it is never a match.
		temporalResolveOraclePlanes_t planes;
		temporalResolveOracleParams_t params;
		float *depthPlane = (float *)malloc( (size_t)pixels * sizeof( float ) );
		float *previousDepthPlane = (float *)malloc( (size_t)pixels * sizeof( float ) );
		if ( !depthPlane || !previousDepthPlane ) {
			free( depthPlane ); free( previousDepthPlane );
			Ral_BufferMapUnmap( slot->buffer, &mapTicket );
			return qfalse;
		}
		for ( uint64_t vi = 0; vi < pixels; ++vi ) {
			depthPlane[vi] = VK_TemporalResolveReadbackDecodeDepth(
				depth + vi * depthBytes, t->currentDepthEncoding );
			memcpy( &previousDepthPlane[vi], previousDepth + vi * 4u, 4u );
		}
		memset( &planes, 0, sizeof( planes ) );
		planes.width = t->captureWidth; planes.height = t->captureHeight;
		planes.originX = t->captureX; planes.originY = t->captureY;
		planes.currentColor = (const uint16_t *)current;
		planes.currentDepth = depthPlane;
		planes.previousColor = (const uint16_t *)previous;
		planes.previousLinearDepth = previousDepthPlane;
		planes.velocity = (const uint16_t *)velocity;
		planes.validity = validity;
		memset( &params, 0, sizeof( params ) );
		params.extent[0] = t->resolve.push.extent[0];
		params.extent[1] = t->resolve.push.extent[1];
		params.currentEffectiveJitterUv[0] = t->resolve.push.currentEffectiveJitterUv[0];
		params.currentEffectiveJitterUv[1] = t->resolve.push.currentEffectiveJitterUv[1];
		params.zNear = t->resolve.push.zNear; params.zFar = t->resolve.push.zFar;
		params.depthThresholdAbsolute = t->resolve.push.depthThresholdAbsolute;
		params.depthThresholdRelative = t->resolve.push.depthThresholdRelative;
		params.historyWeight = t->resolve.push.historyWeight;
		params.qualityFlags = t->resolve.push.qualityFlags;
		params.filterWeightError = VK_TEMPORAL_RESOLVE_READBACK_FILTER_WEIGHT_ERROR;
		for ( y = t->coreY; y < t->coreY + t->coreHeight; ++y ) {
			for ( x = t->coreX; x < t->coreX + t->coreWidth; ++x ) {
				temporalResolveOracleResult_t expected;
				uint32_t i, c;
				uint16_t cb[4], rb[4], hb[4], vb[2];
				float cur[4], actual[4], written[4];
				qboolean currentFinite = qtrue, match, influenced = qfalse;
				if ( !LocalIndex( t, (int32_t)x, (int32_t)y, &i ) ) continue;
				ReadHalf4( current, i, cur, cb ); ReadHalf4( resolved, i, actual, rb );
				ReadHalf4( historyWritten, i, written, hb );
				memcpy( vb, velocity + (uint64_t)i * 4u, sizeof( vb ) );
				for ( c = 0; c < 4; ++c ) if ( !isfinite( cur[c] ) ) currentFinite = qfalse;
				if ( !R_TemporalResolveOraclePixel( &planes, &params, (int32_t)x, (int32_t)y, &expected )
						|| expected.pixelClass == TEMPORAL_RESOLVE_ORACLE_UNSUPPORTED || expected.ambiguous ) {
					r.unsupportedFootprint++;
					if ( expected.ambiguous ) r.oracleAmbiguous++;
					continue;
				}
				match = R_TemporalResolveOracleHalf4Within( rb, expected.displayedLo, expected.displayedHi )
					&& R_TemporalResolveOracleHalf4Within( hb, expected.historyLo, expected.historyHi )
					? qtrue : qfalse;
				if ( !currentFinite ) {
					// The resolve writes zero for a non-finite current colour.
					r.nonfiniteInputs++;
					VerifyZeroFallback( &r, rb );
					continue;
				}
				if ( expected.pixelClass != TEMPORAL_RESOLVE_ORACLE_HISTORY ) {
					qboolean invalid = validity[i] == 0u ? qtrue : qfalse;
					if ( expected.pixelClass == TEMPORAL_RESOLVE_ORACLE_SPATIAL ) r.oracleSpatial++;
					r.fallbackExpected++;
					if ( invalid ) r.invalidFallbackExpected++;
					if ( match ) {
						r.fallbackExact++;
						if ( invalid ) r.invalidFallbackExact++;
						continue;
					}
				} else {
					r.oracleEligible++; r.accepted++;
					if ( expected.moving ) r.oracleMoving++;
					if ( ( vb[0] & 0x7fffu ) != 0u || ( vb[1] & 0x7fffu ) != 0u ) r.acceptedNonzeroVelocity++;
					for ( c = 0; c < 3; ++c ) if ( rb[c] != cb[c] ) influenced = qtrue;
					if ( influenced ) r.acceptedInfluence++;
					if ( match ) { r.acceptedMatches++; continue; }
				}
				r.mismatches++;
				if ( memcmp( rb, cb, sizeof( rb ) ) == 0 ) r.mismatchGpuCurrentFallback++;
				else r.mismatchBlended++;
				if ( r.mismatchSampleCount < VK_TEMPORAL_RESOLVE_READBACK_MISMATCH_SAMPLES ) {
					vkTemporalResolveReadbackMismatchSample_t *sample =
						&r.mismatchSamples[r.mismatchSampleCount++];
					uint32_t channel = 0u;
					for ( c = 0; c < 4; ++c ) {
						float v = actual[c];
						if ( v < expected.displayedLo[c] || v > expected.displayedHi[c] ) { channel = c; break; }
					}
					sample->x = x; sample->y = y; sample->channel = (uint8_t)channel;
					sample->expectedBits = VK_TemporalResolveReadbackFloatToHalfRne(
						0.5f * ( expected.displayedLo[channel] + expected.displayedHi[channel] ) );
					sample->actualBits = rb[channel];
					sample->currentBits = cb[channel];
					sample->halfDistance = HalfOrderedDistance( rb[channel], sample->expectedBits );
					if ( sample->halfDistance > r.mismatchMaxHalfDistance )
						r.mismatchMaxHalfDistance = sample->halfDistance;
					sample->gpuCurrentFallback = memcmp( rb, cb, sizeof( rb ) ) == 0 ? 1u : 0u;
				}
			}
		}
		free( depthPlane ); free( previousDepthPlane );
	}
	r.fenceComplete = qtrue;
	r.ready = r.planesPopulated && r.accepted > 0u
		&& r.acceptedMatches == r.accepted
		&& r.acceptedNonzeroVelocity > 0u
		&& r.fallbackExact == r.fallbackExpected
		&& r.invalidFallbackExact == r.invalidFallbackExpected
		&& r.zeroFallbackExact == r.zeroFallbackExpected
		&& r.nonfiniteInputs == 0u ? qtrue : qfalse;
	if ( Ral_BufferMapUnmap( slot->buffer, &mapTicket ) != ralSuccess ) return qfalse;
	owner->latest = r;
	memset( &slot->ticket, 0, sizeof( slot->ticket ) );
	slot->state = VK_TEMPORAL_RESOLVE_READBACK_READY;
	if ( outReceipt ) *outReceipt = r;
	return qtrue;
}

qboolean VK_TemporalResolveReadbackGetLatest(
		const vkTemporalResolveReadbackOwner_t *owner,
		vkTemporalResolveReadbackContentReceipt_t *outReceipt ) {
	if ( !owner || !owner->initialized || !outReceipt
			|| !owner->latest.fenceComplete ) return qfalse;
	*outReceipt = owner->latest; return qtrue;
}

qboolean VK_TemporalResolveReadbackHasLive(
		const vkTemporalResolveReadbackOwner_t *owner ) {
	uint32_t i;
	if ( !owner || !owner->initialized ) return qfalse;
	if ( owner->capturesRemaining || owner->latest.fenceComplete ) return qtrue;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].buffer
				|| owner->slots[i].state != VK_TEMPORAL_RESOLVE_READBACK_EMPTY )
			return qtrue;
	return qfalse;
}

qboolean VK_TemporalResolveReadbackReleaseAfterIdle(
		vkTemporalResolveReadbackOwner_t *owner, qboolean idleProven ) {
	uint32_t i, serial;
	if ( !owner || !owner->initialized || !idleProven ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].state == VK_TEMPORAL_RESOLVE_READBACK_RECORDED
				|| owner->slots[i].state == VK_TEMPORAL_RESOLVE_READBACK_SUBMITTED )
			return qfalse;
	serial = owner->nextCaptureSerial;
	for ( i = 0; i < VK_TEMPORAL_RESOLVE_READBACK_MAX_FRAMES; ++i ) {
		vkTemporalResolveReadbackSlot_t *slot = &owner->slots[i];
		if ( slot->buffer ) Ral_DestroyBuffer( slot->buffer );
	}
	memset( owner, 0, sizeof( *owner ) ); owner->initialized = qtrue;
	owner->nextCaptureSerial = serial;
	return qtrue;
}
