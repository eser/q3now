// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_motion_readback.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static uint64_t HashBytes( const unsigned char *p, uint64_t bytes ) {
	uint64_t hash = 1469598103934665603ull;
	uint64_t i;
	for ( i = 0; i < bytes; ++i ) {
		hash ^= p[i];
		hash *= 1099511628211ull;
	}
	return hash;
}

static qboolean SnapshotIqmPayload(
		const vkTemporalMainActivationReceipt_t *activation,
		vkTemporalMotionReadbackTicket_t *ticket ) {
	const vkTemporalIqmPayloadReceipt_t *payload;
	const unsigned char *mapped;
	uint64_t current = 1469598103934665603ull;
	uint64_t previous = 1469598103934665603ull;
	uint32_t i;
	if ( !activation || !ticket ) return qfalse;
	if ( !activation->iqm.ready ) return qtrue;
	payload = &activation->iqm.content.payload;
	if ( !activation->iqm.entityCount
			|| activation->iqm.entityCount > TEMPORAL_IQM_MAX_RECORDS
			|| activation->iqm.content.recordCount
				!= activation->iqm.entityCount
			|| !payload->ready || !payload->mappedIdentity
			|| payload->recordBytes != TEMPORAL_IQM_RECORD_SIZE
			|| payload->recordCapacity != TEMPORAL_IQM_MAX_RECORDS
			|| payload->descriptorRange < (uint64_t)activation->iqm.entityCount
				* TEMPORAL_IQM_RECORD_SIZE ) return qfalse;
	mapped = (const unsigned char *)payload->mappedIdentity;
	for ( i = 0; i < activation->iqm.entityCount; ++i ) {
		const unsigned char *record = mapped
			+ (uint64_t)i * TEMPORAL_IQM_RECORD_SIZE;
		current = HashBytes( record + TEMPORAL_IQM_CURRENT_BONES_OFFSET,
			TEMPORAL_IQM_PREVIOUS_BONES_OFFSET
				- TEMPORAL_IQM_CURRENT_BONES_OFFSET ) ^ ( current << 1 );
		previous = HashBytes( record + TEMPORAL_IQM_PREVIOUS_BONES_OFFSET,
			TEMPORAL_IQM_RASTER_MVP_OFFSET
				- TEMPORAL_IQM_PREVIOUS_BONES_OFFSET ) ^ ( previous << 1 );
	}
	ticket->iqmCurrentPaletteHash = current ? current : 1u;
	ticket->iqmPreviousPaletteHash = previous ? previous : 1u;
	memcpy( ticket->iqmRasterMvpBits,
		mapped + TEMPORAL_IQM_RASTER_MVP_OFFSET,
		sizeof( ticket->iqmRasterMvpBits ) );
	ticket->iqmRecordCount = activation->iqm.entityCount;
	return qtrue;
}

static float HalfToFloat( uint16_t h ) {
	const uint32_t sign = (uint32_t)( h & 0x8000u ) << 16;
	uint32_t exp = ( h >> 10 ) & 0x1fu;
	uint32_t mant = h & 0x03ffu;
	uint32_t bits;
	float value;
	if ( exp == 0 ) {
		if ( mant == 0 ) bits = sign;
		else {
			int shift = 0;
			while ( ( mant & 0x0400u ) == 0 ) { mant <<= 1; ++shift; }
			mant &= 0x03ffu;
			bits = sign | (uint32_t)( 113 - shift ) << 23 | mant << 13;
		}
	} else if ( exp == 31 ) {
		bits = sign | 0x7f800000u | mant << 13;
	} else {
		bits = sign | ( exp + 112u ) << 23 | mant << 13;
	}
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}

static qboolean TicketGeometry( uint32_t width, uint32_t height,
		uint32_t *outW, uint32_t *outH, uint64_t *outVelocity,
		uint64_t *outValidityOffset, uint64_t *outTotal ) {
	uint32_t w, h;
	uint64_t pixels, velocity, validityOffset, total;
	if ( !width || !height || !outW || !outH || !outVelocity
			|| !outValidityOffset || !outTotal ) return qfalse;
	w = width > VK_TEMPORAL_READBACK_MAX_ROI
		? VK_TEMPORAL_READBACK_MAX_ROI : width;
	h = height > VK_TEMPORAL_READBACK_MAX_ROI
		? VK_TEMPORAL_READBACK_MAX_ROI : height;
	pixels = (uint64_t)w * (uint64_t)h;
	if ( !pixels || pixels > UINT64_MAX / 5u ) return qfalse;
	velocity = pixels * 4u;
	validityOffset = ( velocity + 3u ) & ~3ull;
	total = validityOffset + pixels;
	*outW = w; *outH = h; *outVelocity = velocity;
	*outValidityOffset = validityOffset; *outTotal = total;
	return qtrue;
}

static qboolean TicketMatchesSlot(
		const vkTemporalMotionReadbackSlot_t *slot, uint32_t frameIndex,
		const vkTemporalMotionReadbackTicket_t *ticket ) {
	uint32_t roiW, roiH;
	uint64_t velocityBytes, validityOffset, totalBytes;
	if ( !slot || !ticket || ticket->commandSlot != frameIndex
			|| ticket->activation.authority.frameIndex != frameIndex
			|| ticket->bufferAllocationGeneration != slot->allocationGeneration
			|| ticket->activation.authority.width != slot->width
			|| ticket->activation.authority.height != slot->height
			|| !TicketGeometry( slot->width, slot->height, &roiW, &roiH,
				&velocityBytes, &validityOffset, &totalBytes ) ) return qfalse;
	if ( ticket->activation.iqm.ready ) {
		vkTemporalMotionReadbackTicket_t snapshot = *ticket;
		snapshot.iqmCurrentPaletteHash = 0;
		snapshot.iqmPreviousPaletteHash = 0;
		memset( snapshot.iqmRasterMvpBits, 0,
			sizeof( snapshot.iqmRasterMvpBits ) );
		snapshot.iqmRecordCount = 0;
		if ( !SnapshotIqmPayload( &ticket->activation, &snapshot )
				|| snapshot.iqmRecordCount != ticket->iqmRecordCount
				|| snapshot.iqmCurrentPaletteHash
					!= ticket->iqmCurrentPaletteHash
				|| snapshot.iqmPreviousPaletteHash
					!= ticket->iqmPreviousPaletteHash
				|| memcmp( snapshot.iqmRasterMvpBits,
					ticket->iqmRasterMvpBits,
					sizeof( ticket->iqmRasterMvpBits ) ) != 0 ) return qfalse;
	} else if ( ticket->iqmRecordCount || ticket->iqmCurrentPaletteHash
			|| ticket->iqmPreviousPaletteHash ) return qfalse;
	return ticket->roiWidth == roiW && ticket->roiHeight == roiH
		&& ticket->roiX == ( slot->width - roiW ) / 2u
		&& ticket->roiY == ( slot->height - roiH ) / 2u
		&& ticket->velocityBytes == velocityBytes
		&& ticket->validityOffset == validityOffset
		&& ticket->totalBytes == totalBytes && totalBytes <= slot->bytes
		&& validityOffset <= totalBytes
		&& (uint64_t)roiW * (uint64_t)roiH <= totalBytes - validityOffset
		? qtrue : qfalse;
}

void VK_TemporalMotionReadbackInit( vkTemporalMotionReadbackOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalMotionReadbackArm(
		vkTemporalMotionReadbackOwner_t *owner, uint32_t captureCount ) {
	uint32_t i;
	if ( !owner || !captureCount || captureCount > VK_TEMPORAL_READBACK_MAX_FRAMES )
		return qfalse;
	if ( !owner->initialized ) VK_TemporalMotionReadbackInit( owner );
	if ( owner->capturesRemaining ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].state == VK_TEMPORAL_READBACK_RECORDED
				|| owner->slots[i].state == VK_TEMPORAL_READBACK_SUBMITTED )
			return qfalse;
	owner->capturesRemaining = captureCount;
	return qtrue;
}

qboolean VK_TemporalMotionReadbackIsArmed(
		const vkTemporalMotionReadbackOwner_t *owner ) {
	return owner && owner->initialized && owner->capturesRemaining ? qtrue : qfalse;
}

qboolean VK_TemporalMotionReadbackPrepareAfterFence(
		vkTemporalMotionReadbackOwner_t *owner, ralBackend_t *backend,
		uint32_t frameCount, uint32_t frameIndex, uint32_t width, uint32_t height ) {
	vkTemporalMotionReadbackSlot_t *slot;
	ralBufferCreateInfo_t bci;
	ralBuffer_t *candidate;
	void *mapped;
	uint32_t roiW, roiH;
	uint64_t velocityBytes, validityOffset, totalBytes;
	if ( !owner || !owner->initialized || !owner->capturesRemaining || !backend
			|| !frameCount || frameCount > VK_TEMPORAL_READBACK_MAX_FRAMES
			|| frameIndex >= frameCount || !TicketGeometry( width, height,
				&roiW, &roiH, &velocityBytes, &validityOffset, &totalBytes ) )
		return qfalse;
	if ( owner->backend && owner->backend != backend ) return qfalse;
	if ( owner->frameCount && owner->frameCount != frameCount ) return qfalse;
	owner->backend = backend;
	owner->frameCount = frameCount;
	slot = &owner->slots[frameIndex];
	if ( slot->state == VK_TEMPORAL_READBACK_RECORDED
			|| slot->state == VK_TEMPORAL_READBACK_SUBMITTED ) return qfalse;
	if ( slot->buffer && slot->mapped && slot->bytes == totalBytes
			&& slot->width == width && slot->height == height ) {
		memset( slot->mapped, 0x7f, (size_t)totalBytes );
		memset( &slot->ticket, 0, sizeof( slot->ticket ) );
		slot->state = VK_TEMPORAL_READBACK_READY;
		return qtrue;
	}
	if ( slot->allocationGeneration == UINT32_MAX ) return qfalse;
	memset( &bci, 0, sizeof( bci ) );
	bci.size = totalBytes;
	bci.usage = RAL_BUFFER_TRANSFER_DST;
	bci.memory = RAL_MEMORY_HOST_COHERENT;
	bci.debugName = "wired-temporal-motion-readback";
	candidate = Ral_CreateBuffer( backend, &bci );
	if ( !candidate ) return qfalse;
	{
		uint32_t i;
		for ( i = 0; i < VK_TEMPORAL_READBACK_MAX_FRAMES; ++i )
			if ( owner->slots[i].buffer == candidate ) return qfalse;
	}
	mapped = Ral_MapBuffer( candidate );
	if ( !mapped ) { Ral_DestroyBuffer( candidate ); return qfalse; }
	{
		uint32_t i;
		for ( i = 0; i < VK_TEMPORAL_READBACK_MAX_FRAMES; ++i ) {
			if ( owner->slots[i].mapped == mapped ) {
				Ral_UnmapBuffer( candidate );
				Ral_DestroyBuffer( candidate );
				return qfalse;
			}
		}
	}
	if ( slot->mapped ) Ral_UnmapBuffer( slot->buffer );
	if ( slot->buffer ) Ral_DestroyBuffer( slot->buffer );
	slot->buffer = candidate;
	slot->mapped = mapped;
	slot->bytes = totalBytes;
	slot->width = width;
	slot->height = height;
	slot->allocationGeneration++;
	memset( slot->mapped, 0x7f, (size_t)totalBytes );
	slot->state = VK_TEMPORAL_READBACK_READY;
	memset( &slot->ticket, 0, sizeof( slot->ticket ) );
	return qtrue;
}

qboolean VK_TemporalMotionReadbackRecord(
		vkTemporalMotionReadbackOwner_t *owner, ralCommandBuffer_t *commandBuffer,
		uint32_t frameIndex,
		const vkTemporalMainActivationReceipt_t *pendingActivation,
		const vkTemporalMotionMaterializationProductView_t *view ) {
	vkTemporalMotionReadbackSlot_t *slot;
	vkTemporalMotionReadbackTicket_t ticket;
	ralBufferTextureCopy_t copy;
	ralMemoryBarrier_t memory;
	ralPipelineBarrierInfo_t barrier;
	uint32_t roiW, roiH;
	uint64_t velocityBytes, validityOffset, totalBytes;
	if ( !owner || !owner->initialized || !owner->capturesRemaining
			|| !commandBuffer || frameIndex >= owner->frameCount
			|| !pendingActivation || !pendingActivation->ready || !view
			|| pendingActivation->authority.frameIndex != frameIndex
			|| !view->velocity || !view->validity
			|| view->velocity == view->validity
			|| pendingActivation->authority.targetAllocationGeneration !=
				view->targetAllocationGeneration
			|| pendingActivation->authority.materializationGeneration !=
				view->allocationGeneration
			|| pendingActivation->authority.pipelineLayoutAllocationGeneration !=
				view->pipelineLayoutAllocationGeneration
			|| !pendingActivation->temporalSegments
			|| pendingActivation->temporalSegments !=
				pendingActivation->written + pendingActivation->invalidated
			|| pendingActivation->prepared !=
				pendingActivation->temporalSegments + pendingActivation->preserved
			|| pendingActivation->drawSequence.count != pendingActivation->prepared
			|| !pendingActivation->auxiliaryCleared
			|| !pendingActivation->depthStoreRequired
			|| !pendingActivation->stencilStoreRequired
			|| !TicketGeometry( pendingActivation->authority.width,
				pendingActivation->authority.height, &roiW, &roiH,
				&velocityBytes, &validityOffset, &totalBytes ) ) return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_READBACK_READY || !slot->buffer
			|| !slot->mapped || slot->bytes != totalBytes
			|| slot->width != pendingActivation->authority.width
			|| slot->height != pendingActivation->authority.height
			|| owner->nextCaptureSerial == UINT32_MAX ) return qfalse;
	memset( &ticket, 0, sizeof( ticket ) );
	ticket.activation = *pendingActivation;
	ticket.commandSlot = frameIndex;
	ticket.captureSerial = owner->nextCaptureSerial + 1u;
	ticket.bufferAllocationGeneration = slot->allocationGeneration;
	ticket.roiWidth = roiW; ticket.roiHeight = roiH;
	ticket.roiX = ( slot->width - roiW ) / 2u;
	ticket.roiY = ( slot->height - roiH ) / 2u;
	ticket.velocityBytes = velocityBytes;
	ticket.validityOffset = validityOffset;
	ticket.totalBytes = totalBytes;
	if ( !SnapshotIqmPayload( pendingActivation, &ticket ) ) return qfalse;

	Ral_CmdTransitionTexture( commandBuffer, view->velocity,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, view->validity,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		RAL_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	memset( &copy, 0, sizeof( copy ) );
	copy.imageRect.x = (int32_t)ticket.roiX;
	copy.imageRect.y = (int32_t)ticket.roiY;
	copy.imageRect.width = roiW; copy.imageRect.height = roiH;
	Ral_CmdCopyTextureToBuffer( commandBuffer, view->velocity, slot->buffer, &copy );
	copy.bufferOffset = validityOffset;
	Ral_CmdCopyTextureToBuffer( commandBuffer, view->validity, slot->buffer, &copy );
	memset( &memory, 0, sizeof( memory ) );
	memory.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memory.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	memset( &barrier, 0, sizeof( barrier ) );
	barrier.srcStageMask = RAL_PIPELINE_STAGE_TRANSFER_BIT;
	barrier.dstStageMask = RAL_PIPELINE_STAGE_HOST_BIT;
	barrier.memoryBarrierCount = 1;
	barrier.memoryBarriers = &memory;
	Ral_CmdPipelineBarrierFull( commandBuffer, &barrier );
	Ral_CmdTransitionTexture( commandBuffer, view->velocity,
		RAL_PIPELINE_STAGE_TRANSFER_BIT,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	Ral_CmdTransitionTexture( commandBuffer, view->validity,
		RAL_PIPELINE_STAGE_TRANSFER_BIT,
		RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	owner->nextCaptureSerial = ticket.captureSerial;
	slot->ticket = ticket;
	slot->state = VK_TEMPORAL_READBACK_RECORDED;
	return qtrue;
}

qboolean VK_TemporalMotionReadbackResolveSubmit(
		vkTemporalMotionReadbackOwner_t *owner, uint32_t frameIndex,
		qboolean submitted,
		const vkTemporalMainActivationReceipt_t *resolvedActivation,
		vkTemporalMotionReadbackTicket_t *outTicket ) {
	vkTemporalMotionReadbackSlot_t *slot;
	if ( !owner || !owner->initialized || frameIndex >= owner->frameCount )
		return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_READBACK_RECORDED ) return qfalse;
	if ( !TicketMatchesSlot( slot, frameIndex, &slot->ticket ) ) return qfalse;
	if ( !submitted || !resolvedActivation
			|| !VK_TemporalMainActivationReceiptExact(
				&slot->ticket.activation, resolvedActivation ) ) {
		memset( &slot->ticket, 0, sizeof( slot->ticket ) );
		slot->state = VK_TEMPORAL_READBACK_READY;
		return qfalse;
	}
	slot->ticket.submitted = qtrue;
	slot->state = VK_TEMPORAL_READBACK_SUBMITTED;
	if ( owner->capturesRemaining ) owner->capturesRemaining--;
	if ( outTicket ) *outTicket = slot->ticket;
	return qtrue;
}

qboolean VK_TemporalMotionReadbackCompleteAfterFence(
		vkTemporalMotionReadbackOwner_t *owner, uint32_t frameIndex,
		qboolean fenceProven,
		vkTemporalMotionReadbackContentReceipt_t *outReceipt ) {
	vkTemporalMotionReadbackSlot_t *slot;
	vkTemporalMotionReadbackContentReceipt_t receipt;
	const unsigned char *velocity, *validity;
	const uint16_t *halfs;
	uint32_t i;
	if ( !owner || !owner->initialized || !fenceProven
			|| frameIndex >= owner->frameCount )
		return qfalse;
	slot = &owner->slots[frameIndex];
	if ( slot->state != VK_TEMPORAL_READBACK_SUBMITTED || !slot->mapped
			|| !slot->ticket.submitted
			|| !TicketMatchesSlot( slot, frameIndex, &slot->ticket ) ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.ticket = slot->ticket;
	receipt.pixels = slot->ticket.roiWidth * slot->ticket.roiHeight;
	velocity = (const unsigned char *)slot->mapped;
	validity = velocity + slot->ticket.validityOffset;
	halfs = (const uint16_t *)velocity;
	receipt.velocityHash = HashBytes( velocity, slot->ticket.velocityBytes );
	receipt.validityHash = HashBytes( validity, receipt.pixels );
	for ( i = 0; i < receipt.pixels; ++i ) {
		const uint16_t hx = halfs[i * 2u], hy = halfs[i * 2u + 1u];
		const float x = HalfToFloat( hx ), y = HalfToFloat( hy );
		const qboolean finite = isfinite( x ) && isfinite( y ) ? qtrue : qfalse;
		const qboolean nonzero = ( ( hx & 0x7fffu ) != 0
			|| ( hy & 0x7fffu ) != 0 ) ? qtrue : qfalse;
		if ( validity[i] == 0 ) receipt.validityZero++;
		else if ( validity[i] == 255 ) receipt.validityFull++;
		else receipt.validityOther++;
		if ( finite ) receipt.finiteVelocity++;
		else receipt.nonfiniteVelocity++;
		if ( validity[i] == 255 && finite && nonzero )
			receipt.nonzeroValidVelocity++;
		if ( validity[i] == 0 && nonzero ) receipt.nonzeroInvalidVelocity++;
	}
	receipt.fenceComplete = qtrue;
	receipt.ready = receipt.validityOther == 0
		&& receipt.nonfiniteVelocity == 0
		&& receipt.validityFull > 0
		&& receipt.nonzeroValidVelocity > 0
		&& receipt.nonzeroInvalidVelocity == 0 ? qtrue : qfalse;
	owner->latest = receipt;
	memset( &slot->ticket, 0, sizeof( slot->ticket ) );
	slot->state = VK_TEMPORAL_READBACK_READY;
	if ( outReceipt ) *outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalMotionReadbackGetLatest(
		const vkTemporalMotionReadbackOwner_t *owner,
		vkTemporalMotionReadbackContentReceipt_t *outReceipt ) {
	if ( !owner || !owner->initialized || !outReceipt
			|| !owner->latest.fenceComplete ) return qfalse;
	*outReceipt = owner->latest;
	return qtrue;
}

qboolean VK_TemporalMotionReadbackHasLive(
		const vkTemporalMotionReadbackOwner_t *owner ) {
	uint32_t i;
	if ( !owner || !owner->initialized ) return qfalse;
	if ( owner->capturesRemaining ) return qtrue;
	for ( i = 0; i < VK_TEMPORAL_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].buffer
				|| owner->slots[i].state != VK_TEMPORAL_READBACK_EMPTY )
			return qtrue;
	if ( owner->latest.fenceComplete ) return qtrue;
	return qfalse;
}

qboolean VK_TemporalMotionReadbackReleaseAfterIdle(
		vkTemporalMotionReadbackOwner_t *owner, qboolean idleProven ) {
	uint32_t i;
	uint32_t serial;
	if ( !owner || !owner->initialized || !idleProven ) return qfalse;
	for ( i = 0; i < VK_TEMPORAL_READBACK_MAX_FRAMES; ++i )
		if ( owner->slots[i].state == VK_TEMPORAL_READBACK_RECORDED
				|| owner->slots[i].state == VK_TEMPORAL_READBACK_SUBMITTED )
			return qfalse;
	serial = owner->nextCaptureSerial;
	for ( i = 0; i < VK_TEMPORAL_READBACK_MAX_FRAMES; ++i ) {
		vkTemporalMotionReadbackSlot_t *slot = &owner->slots[i];
		if ( slot->mapped ) Ral_UnmapBuffer( slot->buffer );
		if ( slot->buffer ) Ral_DestroyBuffer( slot->buffer );
	}
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->nextCaptureSerial = serial;
	return qtrue;
}
