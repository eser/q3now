// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_atmospheric_heightgrid.h"

#include <limits.h>
#include <string.h>

#define VK_ATMOSPHERIC_HEIGHTGRID_RECEIPT_SCHEMA 1u

static qboolean AllocationValid( const ralAllocationReceipt_t *allocation,
	const ralTexture_t *texture ) {
	return allocation
		&& Ral_AllocationReceiptExact( allocation, allocation )
		&& allocation->ready
		&& allocation->memoryClass == RAL_ALLOCATION_DEVICE_LOCAL
		&& allocation->residency == RAL_ALLOCATION_RESIDENCY_PERMANENT
		&& allocation->ownerIdentity == (uintptr_t)texture
		&& allocation->allocationGeneration > 0u
		&& allocation->allocationGeneration < UINT64_MAX
		&& allocation->committedSize >= VK_ATMOSPHERIC_HEIGHTGRID_BYTES;
}

static qboolean OwnerValid( const vkAtmosphericHeightgridOwner_t *owner ) {
	return owner && owner->initialized == qtrue && owner->ready == qtrue
		&& owner->backend && owner->texture && owner->sampler
		&& AllocationValid( &owner->allocation, owner->texture )
		&& owner->uploadSerial < UINT64_MAX;
}

static qboolean TransferValid( const ralTransferReceipt_t *transfer,
	const vkAtmosphericHeightgridOwner_t *owner ) {
	return transfer && owner
		&& Ral_TransferReceiptExact( transfer, transfer )
		&& transfer->ready
		&& transfer->state == RAL_TRANSFER_COMPLETED
		&& transfer->request.direction == RAL_TRANSFER_UPLOAD
		&& transfer->request.resourceKind == RAL_TRANSFER_TEXTURE
		&& transfer->request.resourceIdentity == (uintptr_t)owner->texture
		&& transfer->request.resourceGeneration
			== owner->allocation.allocationGeneration
		&& transfer->request.byteSize == VK_ATMOSPHERIC_HEIGHTGRID_BYTES
		&& transfer->request.byteBudget == owner->allocation.committedSize
		&& transfer->request.mipLevel == 0u
		&& transfer->request.arrayLayer == 0u
		&& transfer->request.offsetX == 0u
		&& transfer->request.offsetY == 0u
		&& transfer->request.width == VK_ATMOSPHERIC_HEIGHTGRID_SIZE
		&& transfer->request.height == VK_ATMOSPHERIC_HEIGHTGRID_SIZE
		&& transfer->request.depth == 1u
		&& transfer->request.queue == RAL_QUEUE_GRAPHICS
		&& transfer->transferGeneration > 0u
		&& transfer->transferGeneration < UINT64_MAX
		&& transfer->submissionGeneration > 0u
		&& transfer->submissionGeneration < UINT64_MAX
		&& transfer->completionGeneration > 0u
		&& transfer->completionGeneration < UINT64_MAX;
}

static qboolean ReceiptValid( const vkAtmosphericHeightgridReceipt_t *receipt ) {
	vkAtmosphericHeightgridOwner_t owner;
	if ( !receipt ) return qfalse;
	memset( &owner, 0, sizeof( owner ) );
	owner.initialized = qtrue;
	owner.backend = receipt->backend;
	owner.texture = receipt->texture;
	owner.sampler = receipt->sampler;
	owner.allocation = receipt->allocation;
	owner.transfer = receipt->transfer;
	owner.uploadSerial = receipt->uploadSerial;
	owner.ready = receipt->ready;
	return receipt
		&& receipt->schemaVersion == VK_ATMOSPHERIC_HEIGHTGRID_RECEIPT_SCHEMA
		&& receipt->backend && receipt->texture && receipt->sampler
		&& AllocationValid( &receipt->allocation, receipt->texture )
		&& TransferValid( &receipt->transfer, &owner )
		&& receipt->uploadSerial > 0u && receipt->uploadSerial < UINT64_MAX
		&& receipt->width == VK_ATMOSPHERIC_HEIGHTGRID_SIZE
		&& receipt->height == VK_ATMOSPHERIC_HEIGHTGRID_SIZE
		&& receipt->byteSize == VK_ATMOSPHERIC_HEIGHTGRID_BYTES
		&& receipt->ready == qtrue;
}

static void ReleaseTicket( ralUploadTicket_t *ticket ) {
	if ( !ticket ) return;
	if ( ticket->fence ) Ral_DestroyFence( ticket->fence );
	if ( ticket->readySemaphore ) Ral_DestroySemaphore( ticket->readySemaphore );
	memset( ticket, 0, sizeof( *ticket ) );
}

static void BuildReceipt( const vkAtmosphericHeightgridOwner_t *owner,
	vkAtmosphericHeightgridReceipt_t *outReceipt ) {
	vkAtmosphericHeightgridReceipt_t candidate;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = VK_ATMOSPHERIC_HEIGHTGRID_RECEIPT_SCHEMA;
	candidate.backend = owner->backend;
	candidate.texture = owner->texture;
	candidate.sampler = owner->sampler;
	candidate.allocation = owner->allocation;
	candidate.transfer = owner->transfer;
	candidate.uploadSerial = owner->uploadSerial;
	candidate.width = VK_ATMOSPHERIC_HEIGHTGRID_SIZE;
	candidate.height = VK_ATMOSPHERIC_HEIGHTGRID_SIZE;
	candidate.byteSize = VK_ATMOSPHERIC_HEIGHTGRID_BYTES;
	candidate.ready = qtrue;
	*outReceipt = candidate;
}

void VK_AtmosphericHeightgridInit( vkAtmosphericHeightgridOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_AtmosphericHeightgridEnsure( vkAtmosphericHeightgridOwner_t *owner,
	ralBackend_t *backend ) {
	ralTextureCreateInfo_t textureInfo;
	ralSamplerCreateInfo_t samplerInfo;
	ralTexture_t *texture;
	ralSampler_t *sampler;
	ralAllocationReceipt_t allocation;
	if ( !owner || owner->initialized != qtrue || !backend ) return qfalse;
	if ( owner->ready ) return OwnerValid( owner ) && owner->backend == backend;
	if ( owner->backend || owner->texture || owner->sampler ) return qfalse;

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.type = RAL_TEXTURE_2D;
	textureInfo.format = RAL_FORMAT_R32_SFLOAT;
	textureInfo.width = VK_ATMOSPHERIC_HEIGHTGRID_SIZE;
	textureInfo.height = VK_ATMOSPHERIC_HEIGHTGRID_SIZE;
	textureInfo.depthOrArrayLayers = 1u;
	textureInfo.mipLevels = 1u;
	textureInfo.sampleCount = 1u;
	textureInfo.usage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST;
	textureInfo.memory = RAL_MEMORY_DEVICE_LOCAL;
	textureInfo.debugName = "wired-atmospheric-heightgrid";
	texture = Ral_CreateTexture( backend, &textureInfo );
	if ( !texture ) return qfalse;

	memset( &samplerInfo, 0, sizeof( samplerInfo ) );
	samplerInfo.minFilter = RAL_FILTER_LINEAR;
	samplerInfo.magFilter = RAL_FILTER_LINEAR;
	samplerInfo.mipmapMode = RAL_MIPMAP_NEAREST;
	samplerInfo.addressU = RAL_ADDRESS_CLAMP_TO_EDGE;
	samplerInfo.addressV = RAL_ADDRESS_CLAMP_TO_EDGE;
	samplerInfo.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.debugName = "wired-atmospheric-heightgrid-sampler";
	sampler = Ral_CreateSampler( backend, &samplerInfo );
	if ( !sampler ) {
		Ral_DestroyTexture( texture );
		return qfalse;
	}
	memset( &allocation, 0, sizeof( allocation ) );
	if ( !Ral_TextureGetAllocationReceipt( texture, &allocation )
			|| !AllocationValid( &allocation, texture ) ) {
		Ral_DestroySampler( sampler );
		Ral_DestroyTexture( texture );
		return qfalse;
	}
	owner->backend = backend;
	owner->texture = texture;
	owner->sampler = sampler;
	owner->allocation = allocation;
	owner->ready = qtrue;
	return qtrue;
}

qboolean VK_AtmosphericHeightgridUpload( vkAtmosphericHeightgridOwner_t *owner,
	const float *grid, int count, vkAtmosphericHeightgridReceipt_t *outReceipt ) {
	ralTextureUploadDesc_t upload;
	ralUploadTicket_t ticket;
	ralTransferReceipt_t transfer;
	vkAtmosphericHeightgridOwner_t ownerCandidate;
	vkAtmosphericHeightgridReceipt_t candidate;
	if ( !OwnerValid( owner ) || !grid || !outReceipt
			|| count < 0 || (uint32_t)count < VK_ATMOSPHERIC_HEIGHTGRID_TEXELS
			|| owner->uploadSerial >= UINT64_MAX - 1u ) return qfalse;
	memset( &upload, 0, sizeof( upload ) );
	upload.data = grid;
	upload.dataSize = VK_ATMOSPHERIC_HEIGHTGRID_BYTES;
	upload.mipLevel = 0u;
	upload.arrayLayer = 0u;
	// Forces the portable graphics-queue upload path: no transfer-family
	// ownership is left pending before the compute sampler can consume it.
	upload.suppressMipGeneration = qtrue;
	ticket = Ral_TextureUploadBegin( owner->texture, &upload );
	if ( !ticket.fence ) {
		ReleaseTicket( &ticket );
		return qfalse;
	}
	Ral_WaitFence( ticket.fence, RAL_TIMEOUT_INFINITE );
	if ( !Ral_TextureUploadTicketComplete( &ticket )
			|| ( ticket.graphicsAcquireRequired
				&& !Ral_TextureAcquireBatchToGraphics( owner->backend, &ticket, 1u ) )
			|| !Ral_TextureUploadTicketGetReceipt( &ticket, &transfer )
			|| !TransferValid( &transfer, owner ) ) {
		ReleaseTicket( &ticket );
		return qfalse;
	}
	ReleaseTicket( &ticket );
	ownerCandidate = *owner;
	ownerCandidate.transfer = transfer;
	ownerCandidate.uploadSerial++;
	BuildReceipt( &ownerCandidate, &candidate );
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*owner = ownerCandidate;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_AtmosphericHeightgridGetReceipt(
	const vkAtmosphericHeightgridOwner_t *owner,
	vkAtmosphericHeightgridReceipt_t *outReceipt ) {
	vkAtmosphericHeightgridReceipt_t candidate;
	if ( !OwnerValid( owner ) || !outReceipt || !owner->uploadSerial
			|| !TransferValid( &owner->transfer, owner ) ) return qfalse;
	BuildReceipt( owner, &candidate );
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_AtmosphericHeightgridReceiptExact(
	const vkAtmosphericHeightgridReceipt_t *a,
	const vkAtmosphericHeightgridReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->backend == b->backend
		&& a->texture == b->texture
		&& a->sampler == b->sampler
		&& Ral_AllocationReceiptExact( &a->allocation, &b->allocation )
		&& Ral_TransferReceiptExact( &a->transfer, &b->transfer )
		&& a->uploadSerial == b->uploadSerial
		&& a->width == b->width && a->height == b->height
		&& a->byteSize == b->byteSize && a->ready == b->ready;
}

qboolean VK_AtmosphericHeightgridHasLive(
	const vkAtmosphericHeightgridOwner_t *owner ) {
	return ( owner && ( owner->backend || owner->texture || owner->sampler
		|| owner->ready ) ) ? qtrue : qfalse;
}

void VK_AtmosphericHeightgridRelease( vkAtmosphericHeightgridOwner_t *owner ) {
	uint64_t uploadSerial;
	if ( !owner || owner->initialized != qtrue ) return;
	uploadSerial = owner->uploadSerial;
	if ( owner->sampler ) Ral_DestroySampler( owner->sampler );
	if ( owner->texture ) Ral_DestroyTexture( owner->texture );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->uploadSerial = uploadSerial;
}
