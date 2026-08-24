// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_atmospheric_pool.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static qboolean AllocationValid( const ralAllocationReceipt_t *allocation,
		const ralBuffer_t *buffer, uint64_t byteSize ) {
	return allocation && buffer && byteSize > 0u
		&& Ral_AllocationReceiptExact( allocation, allocation )
		&& allocation->ready == qtrue
		&& allocation->memoryClass == RAL_ALLOCATION_DEVICE_LOCAL
		&& allocation->residency == RAL_ALLOCATION_RESIDENCY_PERMANENT
		&& allocation->ownerIdentity == (uintptr_t)buffer
		&& allocation->allocationGeneration > 0u
		&& allocation->allocationGeneration < UINT64_MAX
		&& allocation->requestedSize == byteSize
		&& allocation->committedSize >= byteSize;
}

static qboolean UploadValid( const ralBufferUploadReceipt_t *upload,
		const ralBuffer_t *buffer,
		const ralAllocationReceipt_t *allocation, uint64_t byteSize ) {
	const ralTransferRequest_t *request;
	if ( !upload || !buffer || !allocation
			|| !Ral_BufferUploadReceiptExact( upload, upload ) ) return qfalse;
	request = &upload->transfer.request;
	return request->direction == RAL_TRANSFER_UPLOAD
		&& request->resourceKind == RAL_TRANSFER_BUFFER
		&& request->resourceIdentity == (uintptr_t)buffer
		&& request->resourceGeneration == allocation->allocationGeneration
		&& request->byteOffset == 0u && request->byteSize == byteSize
		&& request->byteBudget == allocation->committedSize
		&& ( request->queue == RAL_QUEUE_GRAPHICS
			|| request->queue == RAL_QUEUE_TRANSFER )
		&& upload->transfer.state == RAL_TRANSFER_COMPLETED
		&& upload->graphicsVisibilityGeneration
			== upload->transfer.completionGeneration
		&& upload->ready == qtrue;
}

static qboolean OwnerValid( const vkAtmosphericPoolOwner_t *owner ) {
	uint32_t i;
	if ( !owner || owner->initialized != qtrue || owner->ready != qtrue
			|| !owner->backend || owner->byteSize == 0u
			|| owner->poolGeneration == 0u
			|| owner->poolGeneration == UINT64_MAX
			|| !owner->buffers[0] || !owner->buffers[1]
			|| owner->buffers[0] == owner->buffers[1] ) return qfalse;
	for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ ) {
		if ( !AllocationValid( &owner->allocations[i], owner->buffers[i],
				owner->byteSize )
				|| !UploadValid( &owner->uploads[i], owner->buffers[i],
					&owner->allocations[i], owner->byteSize ) ) return qfalse;
	}
	return qtrue;
}

static qboolean ReceiptValid( const vkAtmosphericPoolReceipt_t *receipt ) {
	vkAtmosphericPoolOwner_t owner;
	if ( !receipt || receipt->schemaVersion
			!= VK_ATMOSPHERIC_POOL_RECEIPT_SCHEMA ) return qfalse;
	memset( &owner, 0, sizeof( owner ) );
	owner.initialized = qtrue;
	owner.backend = receipt->backend;
	memcpy( owner.buffers, receipt->buffers, sizeof( owner.buffers ) );
	memcpy( owner.allocations, receipt->allocations,
		sizeof( owner.allocations ) );
	memcpy( owner.uploads, receipt->uploads, sizeof( owner.uploads ) );
	owner.poolGeneration = receipt->poolGeneration;
	owner.byteSize = receipt->byteSize;
	owner.ready = receipt->ready;
	return OwnerValid( &owner );
}

static void BuildReceipt( const vkAtmosphericPoolOwner_t *owner,
		vkAtmosphericPoolReceipt_t *outReceipt ) {
	vkAtmosphericPoolReceipt_t candidate;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = VK_ATMOSPHERIC_POOL_RECEIPT_SCHEMA;
	candidate.backend = owner->backend;
	memcpy( candidate.buffers, owner->buffers, sizeof( candidate.buffers ) );
	memcpy( candidate.allocations, owner->allocations,
		sizeof( candidate.allocations ) );
	memcpy( candidate.uploads, owner->uploads, sizeof( candidate.uploads ) );
	candidate.poolGeneration = owner->poolGeneration;
	candidate.byteSize = owner->byteSize;
	candidate.ready = qtrue;
	*outReceipt = candidate;
}

static void RetireTicket( ralBufferUploadTicket_t *ticket ) {
	if ( !ticket ) return;
	if ( ticket->fence ) {
		Ral_WaitFence( ticket->fence, RAL_TIMEOUT_INFINITE );
		(void)Ral_BufferUploadTicketComplete( ticket );
		Ral_DestroyFence( ticket->fence );
	}
	if ( ticket->readySemaphore )
		Ral_DestroySemaphore( ticket->readySemaphore );
	memset( ticket, 0, sizeof( *ticket ) );
}

static void DestroyCandidate( vkAtmosphericPoolOwner_t *candidate,
		ralBufferUploadTicket_t *tickets ) {
	uint32_t i;
	if ( tickets ) {
		for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ )
			RetireTicket( &tickets[i] );
	}
	if ( candidate ) {
		if ( candidate->buffers[1]
				&& candidate->buffers[1] != candidate->buffers[0] )
			Ral_DestroyBuffer( candidate->buffers[1] );
		if ( candidate->buffers[0] ) Ral_DestroyBuffer( candidate->buffers[0] );
		candidate->buffers[0] = candidate->buffers[1] = NULL;
	}
}

void VK_AtmosphericPoolInit( vkAtmosphericPoolOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_AtmosphericPoolEnsure( vkAtmosphericPoolOwner_t *owner,
		ralBackend_t *backend, uint64_t byteSize ) {
	vkAtmosphericPoolOwner_t candidate;
	ralBufferUploadTicket_t tickets[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	ralBufferCreateInfo_t createInfo;
	unsigned char *zeroSeed;
	uint32_t i;
	if ( !owner || owner->initialized != qtrue || !backend || byteSize == 0u
			|| byteSize > (uint64_t)SIZE_MAX ) return qfalse;
	if ( owner->ready ) return OwnerValid( owner )
		&& owner->backend == backend && owner->byteSize == byteSize;
	if ( owner->backend || owner->buffers[0] || owner->buffers[1]
			|| owner->byteSize || owner->ready
			|| owner->poolGeneration >= UINT64_MAX - 1u ) return qfalse;

	candidate = *owner;
	memset( tickets, 0, sizeof( tickets ) );
	zeroSeed = (unsigned char *)calloc( 1u, (size_t)byteSize );
	if ( !zeroSeed ) return qfalse;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.size = byteSize;
	createInfo.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;
	createInfo.memory = RAL_MEMORY_DEVICE_LOCAL;
	createInfo.debugName = "wired-atmospheric-pool";
	for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ ) {
		candidate.buffers[i] = Ral_CreateBuffer( backend, &createInfo );
		if ( !candidate.buffers[i]
				|| ( i > 0u && candidate.buffers[i] == candidate.buffers[0] )
				|| !Ral_BufferGetAllocationReceipt( candidate.buffers[i],
					&candidate.allocations[i] )
				|| !AllocationValid( &candidate.allocations[i],
					candidate.buffers[i], byteSize ) ) goto fail;
		tickets[i] = Ral_BufferUploadBegin( candidate.buffers[i], 0u,
			zeroSeed, byteSize );
		if ( !tickets[i].fence ) goto fail;
	}
	free( zeroSeed );
	zeroSeed = NULL;
	for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ ) {
		Ral_WaitFence( tickets[i].fence, RAL_TIMEOUT_INFINITE );
		if ( !Ral_BufferUploadTicketComplete( &tickets[i] ) ) goto fail;
	}
	if ( !Ral_BufferAcquireBatchToGraphics( backend, tickets,
			VK_ATMOSPHERIC_POOL_BUFFER_COUNT ) ) goto fail;
	for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ ) {
		if ( !Ral_BufferUploadTicketGetReceipt( &tickets[i],
				&candidate.uploads[i] )
				|| !UploadValid( &candidate.uploads[i], candidate.buffers[i],
					&candidate.allocations[i], byteSize ) ) goto fail;
	}
	for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ )
		RetireTicket( &tickets[i] );
	candidate.backend = backend;
	candidate.poolGeneration++;
	candidate.byteSize = byteSize;
	candidate.ready = qtrue;
	if ( !OwnerValid( &candidate ) ) goto fail;
	*owner = candidate;
	return qtrue;

fail:
	free( zeroSeed );
	DestroyCandidate( &candidate, tickets );
	return qfalse;
}

qboolean VK_AtmosphericPoolGetReceipt(
		const vkAtmosphericPoolOwner_t *owner,
		vkAtmosphericPoolReceipt_t *outReceipt ) {
	vkAtmosphericPoolReceipt_t candidate;
	if ( !OwnerValid( owner ) || !outReceipt ) return qfalse;
	BuildReceipt( owner, &candidate );
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_AtmosphericPoolReceiptExact(
		const vkAtmosphericPoolReceipt_t *a,
		const vkAtmosphericPoolReceipt_t *b ) {
	uint32_t i;
	if ( !ReceiptValid( a ) || !ReceiptValid( b )
			|| a->schemaVersion != b->schemaVersion
			|| a->backend != b->backend
			|| a->poolGeneration != b->poolGeneration
			|| a->byteSize != b->byteSize || a->ready != b->ready ) return qfalse;
	for ( i = 0u; i < VK_ATMOSPHERIC_POOL_BUFFER_COUNT; i++ ) {
		if ( a->buffers[i] != b->buffers[i]
				|| !Ral_AllocationReceiptExact( &a->allocations[i],
					&b->allocations[i] )
				|| !Ral_BufferUploadReceiptExact( &a->uploads[i],
					&b->uploads[i] ) ) return qfalse;
	}
	return qtrue;
}

qboolean VK_AtmosphericPoolHasLive(
		const vkAtmosphericPoolOwner_t *owner ) {
	return owner && ( owner->backend || owner->buffers[0] || owner->buffers[1]
		|| owner->byteSize || owner->ready ) ? qtrue : qfalse;
}

void VK_AtmosphericPoolRelease( vkAtmosphericPoolOwner_t *owner ) {
	uint64_t poolGeneration;
	if ( !owner || owner->initialized != qtrue ) return;
	poolGeneration = owner->poolGeneration;
	if ( owner->buffers[1] && owner->buffers[1] != owner->buffers[0] )
		Ral_DestroyBuffer( owner->buffers[1] );
	if ( owner->buffers[0] ) Ral_DestroyBuffer( owner->buffers[0] );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->poolGeneration = poolGeneration;
}
