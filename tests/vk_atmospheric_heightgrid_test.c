// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_atmospheric_heightgrid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ralBackend_s { int id; };
struct ralTexture_s { int id; };
struct ralSampler_s { int id; };
struct ralFence_s { int id; };
struct ralSemaphore_s { int id; };

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

static struct ralTexture_s textureStorage;
static struct ralSampler_s samplerStorage;
static struct ralFence_s fenceStorage;
static struct ralSemaphore_s semaphoreStorage;
static ralAllocationReceipt_t allocation;
static ralTransferReceipt_t transfer;
static unsigned createsTexture, createsSampler, destroysTexture, destroysSampler;
static unsigned begins, waits, completes, acquires, receiptGets, destroysFence, destroysSemaphore;
static unsigned failCreateTexture, failCreateSampler, failAllocation, failBegin;
static unsigned failComplete, failAcquire, failGet;
static unsigned requireAcquire;
static unsigned receiptMutation;
static char destroyOrder[8];
static unsigned destroyOrderCount;
static uint64_t nextTransferGeneration = 10u;

static void BuildAllocation( ralTexture_t *texture ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset( &request, 0, sizeof( request ) );
	memset( &facts, 0, sizeof( facts ) );
	request.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = VK_ATMOSPHERIC_HEIGHTGRID_BYTES;
	request.alignment = 256u;
	request.ownerIdentity = (uintptr_t)texture;
	request.ownerGeneration = 3u;
	facts.backendType = RAL_BACKEND_VULKAN;
	facts.placement = RAL_ALLOCATION_PLACEMENT_SUBALLOCATED;
	facts.committedSize = VK_ATMOSPHERIC_HEIGHTGRID_BYTES;
	facts.actualAlignment = 256u;
	facts.allocationGeneration = 7u;
	facts.deviceLocal = qtrue;
	if ( !Ral_AllocationReceiptBuild( &request, &facts, &allocation ) ) abort();
}

ralTexture_t *Ral_CreateTexture( ralBackend_t *backend,
	const ralTextureCreateInfo_t *info ) {
	createsTexture++;
	if ( failCreateTexture || !backend || !info
			|| info->type != RAL_TEXTURE_2D
			|| info->format != RAL_FORMAT_R32_SFLOAT
			|| info->width != VK_ATMOSPHERIC_HEIGHTGRID_SIZE
			|| info->height != VK_ATMOSPHERIC_HEIGHTGRID_SIZE
			|| info->depthOrArrayLayers != 1u || info->mipLevels != 1u
			|| info->sampleCount != 1u
			|| info->usage != ( RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST )
			|| info->memory != RAL_MEMORY_DEVICE_LOCAL ) return NULL;
	textureStorage.id = 1;
	return &textureStorage;
}

void Ral_DestroyTexture( ralTexture_t *texture ) {
	if ( texture ) { destroysTexture++; destroyOrder[destroyOrderCount++] = 'T'; }
}

ralSampler_t *Ral_CreateSampler( ralBackend_t *backend,
	const ralSamplerCreateInfo_t *info ) {
	createsSampler++;
	if ( failCreateSampler || !backend || !info
			|| info->minFilter != RAL_FILTER_LINEAR
			|| info->magFilter != RAL_FILTER_LINEAR
			|| info->mipmapMode != RAL_MIPMAP_NEAREST
			|| info->addressU != RAL_ADDRESS_CLAMP_TO_EDGE
			|| info->addressV != RAL_ADDRESS_CLAMP_TO_EDGE
			|| info->addressW != RAL_ADDRESS_CLAMP_TO_EDGE
			|| info->maxAnisotropy != 1.0f
			|| info->minLod != 0.0f || info->maxLod != 1.0f ) return NULL;
	samplerStorage.id = 2;
	return &samplerStorage;
}

void Ral_DestroySampler( ralSampler_t *sampler ) {
	if ( sampler ) { destroysSampler++; destroyOrder[destroyOrderCount++] = 'S'; }
}

qboolean Ral_TextureGetAllocationReceipt( const ralTexture_t *texture,
	ralAllocationReceipt_t *out ) {
	if ( failAllocation || texture != &textureStorage || !out ) return qfalse;
	BuildAllocation( (ralTexture_t *)texture );
	*out = allocation;
	return qtrue;
}

ralUploadTicket_t Ral_TextureUploadBegin( ralTexture_t *texture,
	const ralTextureUploadDesc_t *upload ) {
	ralUploadTicket_t ticket;
	ralTransferRequest_t request;
	ralTransferReceipt_t prepared;
	memset( &ticket, 0, sizeof( ticket ) );
	begins++;
	if ( failBegin || texture != &textureStorage || !upload || !upload->data
			|| upload->dataSize != VK_ATMOSPHERIC_HEIGHTGRID_BYTES
			|| upload->mipLevel != 0u || upload->arrayLayer != 0u
			|| upload->suppressMipGeneration != qtrue ) return ticket;
	memset( &request, 0, sizeof( request ) );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_UPLOAD;
	request.resourceKind = RAL_TRANSFER_TEXTURE;
	request.resourceIdentity = (uintptr_t)texture;
	request.resourceGeneration = allocation.allocationGeneration;
	request.byteSize = VK_ATMOSPHERIC_HEIGHTGRID_BYTES;
	request.byteBudget = allocation.committedSize;
	request.width = VK_ATMOSPHERIC_HEIGHTGRID_SIZE;
	request.height = VK_ATMOSPHERIC_HEIGHTGRID_SIZE;
	request.depth = 1u;
	request.queue = RAL_QUEUE_GRAPHICS;
	if ( !Ral_TransferPrepare( &request, nextTransferGeneration, &prepared )
			|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_SYNCHRONOUS,
				nextTransferGeneration, &transfer ) ) abort();
	nextTransferGeneration++;
	ticket.fence = &fenceStorage;
	if ( requireAcquire ) {
		ticket.readySemaphore = &semaphoreStorage;
		ticket.graphicsAcquireRequired = qtrue;
	}
	ticket.texture = texture;
	ticket.synchronous = qtrue;
	ticket.baseMipLevel = 0u;
	ticket.mipLevelCount = 1u;
	ticket.arrayLayerCount = 1u;
	ticket.transfer = transfer;
	return ticket;
}

void Ral_WaitFence( ralFence_t *fence, uint64_t timeoutNs ) {
	if ( fence != &fenceStorage || timeoutNs != RAL_TIMEOUT_INFINITE ) abort();
	waits++;
}

qboolean Ral_TextureUploadTicketComplete( ralUploadTicket_t *ticket ) {
	completes++;
	return !failComplete && ticket && ticket->fence == &fenceStorage;
}

qboolean Ral_TextureAcquireBatchToGraphics( ralBackend_t *backend,
	const ralUploadTicket_t *tickets, uint32_t count ) {
	(void)backend; (void)tickets; (void)count;
	acquires++;
	return failAcquire ? qfalse : qtrue;
}

qboolean Ral_TextureUploadTicketGetReceipt( const ralUploadTicket_t *ticket,
	ralTransferReceipt_t *out ) {
	ralTransferReceipt_t candidate;
	receiptGets++;
	if ( failGet || !ticket || !out ) return qfalse;
	candidate = transfer;
	switch ( receiptMutation ) {
	case 1: candidate.request.byteSize--; break;
	case 2: candidate.request.resourceGeneration++; break;
	case 3: candidate.request.queue = RAL_QUEUE_TRANSFER; break;
	case 4: candidate.request.width--; break;
	case 5: candidate.state = RAL_TRANSFER_SUBMITTED;
		candidate.completionGeneration = 0u; break;
	default: break;
	}
	*out = candidate;
	return qtrue;
}

void Ral_DestroyFence( ralFence_t *fence ) {
	if ( fence != &fenceStorage ) abort();
	destroysFence++;
}

void Ral_DestroySemaphore( ralSemaphore_t *semaphore ) {
	if ( semaphore != &semaphoreStorage ) abort();
	destroysSemaphore++;
}

static void ResetMocks( void ) {
	createsTexture = createsSampler = destroysTexture = destroysSampler = 0u;
	begins = waits = completes = acquires = receiptGets = 0u;
	destroysFence = destroysSemaphore = 0u;
	failCreateTexture = failCreateSampler = failAllocation = failBegin = 0u;
	failComplete = failAcquire = failGet = requireAcquire = receiptMutation = 0u;
	destroyOrderCount = 0u;
	memset( destroyOrder, 0, sizeof( destroyOrder ) );
	nextTransferGeneration = 10u;
}

int main( void ) {
	struct ralBackend_s backend = { 1 }, backend2 = { 2 };
	vkAtmosphericHeightgridOwner_t owner, before;
	vkAtmosphericHeightgridReceipt_t receipt, receipt2, sentinel, bad;
	float *grid = (float *)calloc( VK_ATMOSPHERIC_HEIGHTGRID_TEXELS, sizeof( float ) );
	unsigned mutation;
	CHECK( grid != NULL );
	memset( &sentinel, 0xa5, sizeof( sentinel ) );

	for ( mutation = 0u; mutation < 3u; mutation++ ) {
		ResetMocks(); VK_AtmosphericHeightgridInit( &owner ); before = owner;
		failCreateTexture = mutation == 0u;
		failCreateSampler = mutation == 1u;
		failAllocation = mutation == 2u;
		CHECK( !VK_AtmosphericHeightgridEnsure( &owner, &backend ) );
		CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
		CHECK( destroysSampler <= 1u && destroysTexture <= 1u );
	}

	ResetMocks(); VK_AtmosphericHeightgridInit( &owner );
	CHECK( VK_AtmosphericHeightgridEnsure( &owner, &backend ) );
	CHECK( VK_AtmosphericHeightgridHasLive( &owner ) );
	CHECK( VK_AtmosphericHeightgridEnsure( &owner, &backend ) );
	CHECK( createsTexture == 1u && createsSampler == 1u );
	CHECK( !VK_AtmosphericHeightgridEnsure( &owner, &backend2 ) );

	before = owner; receipt = sentinel;
	CHECK( !VK_AtmosphericHeightgridUpload( &owner, grid, -1, &receipt ) );
	CHECK( !memcmp( &owner, &before, sizeof( owner ) )
		&& !memcmp( &receipt, &sentinel, sizeof( receipt ) ) && begins == 0u );
	CHECK( !VK_AtmosphericHeightgridUpload( &owner, grid,
		(int)VK_ATMOSPHERIC_HEIGHTGRID_TEXELS - 1, &receipt ) );
	CHECK( begins == 0u );

	for ( mutation = 0u; mutation < 9u; mutation++ ) {
		before = owner; receipt = sentinel;
		failBegin = mutation == 0u; failComplete = mutation == 1u;
		failAcquire = mutation == 2u; requireAcquire = mutation == 2u;
		failGet = mutation == 3u; receiptMutation = mutation >= 4u ? mutation - 3u : 0u;
		CHECK( !VK_AtmosphericHeightgridUpload( &owner, grid,
			(int)VK_ATMOSPHERIC_HEIGHTGRID_TEXELS, &receipt ) );
		CHECK( !memcmp( &owner, &before, sizeof( owner ) ) );
		CHECK( !memcmp( &receipt, &sentinel, sizeof( receipt ) ) );
		failBegin = failComplete = failAcquire = failGet = requireAcquire = receiptMutation = 0u;
	}

	CHECK( Ral_AllocationReceiptExact( &owner.allocation, &owner.allocation ) );
	CHECK( Ral_TransferReceiptExact( &transfer, &transfer ) );
	CHECK( VK_AtmosphericHeightgridUpload( &owner, grid,
		(int)VK_ATMOSPHERIC_HEIGHTGRID_TEXELS, &receipt ) );
	CHECK( receipt.uploadSerial == 1u && receipt.ready );
	CHECK( VK_AtmosphericHeightgridGetReceipt( &owner, &receipt2 ) );
	CHECK( VK_AtmosphericHeightgridReceiptExact( &receipt, &receipt2 ) );
	CHECK( VK_AtmosphericHeightgridUpload( &owner, grid,
		(int)VK_ATMOSPHERIC_HEIGHTGRID_TEXELS, &receipt2 ) );
	CHECK( receipt2.uploadSerial == 2u );
	CHECK( !VK_AtmosphericHeightgridReceiptExact( &receipt, &receipt2 ) );
	CHECK( destroysFence == begins - 1u );

	for ( mutation = 0u; mutation < 10u; mutation++ ) {
		bad = receipt2;
		switch ( mutation ) {
		case 0: bad.schemaVersion++; break;
		case 1: bad.backend = &backend2; break;
		case 2: bad.texture = NULL; break;
		case 3: bad.sampler = NULL; break;
		case 4: bad.allocation.allocationGeneration++; break;
		case 5: bad.transfer.request.width--; break;
		case 6: bad.uploadSerial++; break;
		case 7: bad.width--; break;
		case 8: bad.byteSize--; break;
		default: bad.ready = qfalse; break;
		}
		CHECK( !VK_AtmosphericHeightgridReceiptExact( &bad, &receipt2 ) );
	}

	VK_AtmosphericHeightgridRelease( &owner );
	CHECK( !VK_AtmosphericHeightgridHasLive( &owner ) );
	CHECK( destroyOrderCount == 2u && destroyOrder[0] == 'S'
		&& destroyOrder[1] == 'T' );
	VK_AtmosphericHeightgridRelease( &owner );
	CHECK( destroyOrderCount == 2u );
	free( grid );
	puts( "PASS atmospheric heightgrid RAL owner" );
	return 0;
}
