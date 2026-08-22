// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_atmospheric_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; };
struct ralFence_s { int id; };
struct ralSemaphore_s { int id; };

#define POOL_BYTES 128u
#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

static struct ralBuffer_s buffers[2];
static struct ralFence_s fences[2];
static struct ralSemaphore_s semaphores[2];
static ralAllocationReceipt_t allocations[2];
static unsigned creates, destroys, begins, completes, acquires, receiptGets;
static unsigned waits, destroysFence, destroysSemaphore;
static unsigned failPhase, requireAcquire, aliasSecond, receiptMutation;
static char destroyOrder[4];

static void MockAbort( const char *where ) {
	fprintf( stderr, "mock invariant failed: %s\n", where );
	exit( 2 );
}

static int BufferIndex( const ralBuffer_t *buffer ) {
	if ( buffer == &buffers[0] ) return 0;
	if ( buffer == &buffers[1] ) return 1;
	return -1;
}

static void BuildAllocation( int index ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset( &request, 0, sizeof( request ) );
	memset( &facts, 0, sizeof( facts ) );
	request.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = POOL_BYTES;
	request.alignment = 256u;
	request.ownerIdentity = (uintptr_t)&buffers[index];
	request.ownerGeneration = 3u + (uint64_t)index;
	facts.backendType = RAL_BACKEND_VULKAN;
	facts.placement = RAL_ALLOCATION_PLACEMENT_SUBALLOCATED;
	facts.committedSize = 256u;
	facts.actualAlignment = 256u;
	facts.allocationGeneration = 10u + (uint64_t)index;
	facts.deviceLocal = qtrue;
	if ( !Ral_AllocationReceiptBuild( &request, &facts,
			&allocations[index] ) ) MockAbort( "allocation build" );
}

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *info ) {
	unsigned index = creates++;
	if ( !backend || !info || index > 1u
			|| info->size != POOL_BYTES
			|| info->usage != ( RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST )
			|| info->memory != RAL_MEMORY_DEVICE_LOCAL
			|| failPhase == 1u + index ) return NULL;
	buffers[index].id = (int)index + 1;
	if ( index == 1u && aliasSecond ) return &buffers[0];
	return &buffers[index];
}

void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	int index = BufferIndex( buffer );
	if ( index < 0 ) MockAbort( "destroy buffer identity" );
	destroyOrder[destroys++] = (char)( '0' + index );
}

qboolean Ral_BufferGetAllocationReceipt( const ralBuffer_t *buffer,
		ralAllocationReceipt_t *out ) {
	int index = BufferIndex( buffer );
	if ( index < 0 || !out || failPhase == 3u + (unsigned)index ) return qfalse;
	BuildAllocation( index );
	*out = allocations[index];
	return qtrue;
}

ralBufferUploadTicket_t Ral_BufferUploadBegin( ralBuffer_t *buffer,
		uint64_t offset, const void *data, uint64_t size ) {
	ralBufferUploadTicket_t ticket;
	ralTransferRequest_t request;
	ralTransferReceipt_t prepared;
	const unsigned char *bytes = (const unsigned char *)data;
	int index = BufferIndex( buffer );
	uint64_t i;
	memset( &ticket, 0, sizeof( ticket ) );
	begins++;
	if ( index < 0 || offset != 0u || size != POOL_BYTES || !bytes
			|| failPhase == 5u + (unsigned)index ) return ticket;
	for ( i = 0u; i < size; i++ )
		if ( bytes[i] != 0u ) MockAbort( "nonzero seed" );
	memset( &request, 0, sizeof( request ) );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_UPLOAD;
	request.resourceKind = RAL_TRANSFER_BUFFER;
	request.resourceIdentity = (uintptr_t)buffer;
	request.resourceGeneration = allocations[index].allocationGeneration;
	request.byteSize = size;
	request.byteBudget = allocations[index].committedSize;
	request.queue = requireAcquire ? RAL_QUEUE_TRANSFER : RAL_QUEUE_GRAPHICS;
	if ( !Ral_TransferPrepare( &request, 20u + (uint64_t)index, &prepared )
			|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
				20u + (uint64_t)index, &ticket.transfer ) )
		MockAbort( "transfer build" );
	fences[index].id = index + 1;
	semaphores[index].id = index + 1;
	ticket.fence = &fences[index];
	ticket.readySemaphore = requireAcquire ? &semaphores[index] : NULL;
	ticket.buffer = buffer;
	ticket.offset = 0u;
	ticket.size = size;
	ticket.graphicsAcquireRequired = requireAcquire ? qtrue : qfalse;
	ticket.graphicsAcquired = requireAcquire ? qfalse : qtrue;
	return ticket;
}

void Ral_WaitFence( ralFence_t *fence, uint64_t timeoutNs ) {
	if ( !fence || timeoutNs != RAL_TIMEOUT_INFINITE )
		MockAbort( "wait fence" );
	waits++;
}

qboolean Ral_BufferUploadTicketComplete( ralBufferUploadTicket_t *ticket ) {
	ralTransferReceipt_t completed;
	int index = ticket ? BufferIndex( ticket->buffer ) : -1;
	completes++;
	if ( index < 0 || failPhase == 7u + (unsigned)index ) return qfalse;
	if ( ticket->transfer.state == RAL_TRANSFER_COMPLETED ) return qtrue;
	if ( !Ral_TransferComplete( &ticket->transfer,
			ticket->transfer.submissionGeneration, qtrue, &completed ) ) return qfalse;
	ticket->transfer = completed;
	return qtrue;
}

qboolean Ral_BufferAcquireBatchToGraphics( ralBackend_t *backend,
		ralBufferUploadTicket_t *tickets, uint32_t count ) {
	uint32_t i;
	acquires++;
	if ( !backend || !tickets || count != 2u || failPhase == 9u ) return qfalse;
	for ( i = 0u; i < count; i++ ) {
		if ( tickets[i].transfer.state != RAL_TRANSFER_COMPLETED ) return qfalse;
		tickets[i].graphicsAcquired = qtrue;
	}
	return qtrue;
}

qboolean Ral_BufferUploadTicketGetReceipt(
		const ralBufferUploadTicket_t *ticket,
		ralBufferUploadReceipt_t *out ) {
	int index = ticket ? BufferIndex( ticket->buffer ) : -1;
	receiptGets++;
	if ( index < 0 || !out || ticket->graphicsAcquired != qtrue
			|| failPhase == 10u + (unsigned)index
			|| !Ral_BufferUploadReceiptBuild( &ticket->transfer,
				ticket->transfer.completionGeneration, out ) ) return qfalse;
	if ( receiptMutation == 1u && index == 0 ) out->transfer.request.byteSize--;
	if ( receiptMutation == 2u && index == 1 )
		out->transfer.request.resourceGeneration++;
	return qtrue;
}

void Ral_DestroyFence( ralFence_t *fence ) {
	if ( !fence ) MockAbort( "destroy fence" );
	destroysFence++;
}

void Ral_DestroySemaphore( ralSemaphore_t *semaphore ) {
	if ( !semaphore ) MockAbort( "destroy semaphore" );
	destroysSemaphore++;
}

static void ResetMocks( void ) {
	creates = destroys = begins = completes = acquires = receiptGets = 0u;
	waits = destroysFence = destroysSemaphore = 0u;
	failPhase = requireAcquire = aliasSecond = receiptMutation = 0u;
	memset( destroyOrder, 0, sizeof( destroyOrder ) );
}

int main( void ) {
	struct ralBackend_s backend = { 1 }, backend2 = { 2 };
	vkAtmosphericPoolOwner_t owner, before;
	vkAtmosphericPoolReceipt_t receipt, receipt2, bad, sentinel;
	unsigned phase, mutation;
	memset( &sentinel, 0xa5, sizeof( sentinel ) );

	for ( phase = 1u; phase <= 13u; phase++ ) {
		ResetMocks(); VK_AtmosphericPoolInit( &owner ); before = owner;
		if ( phase <= 11u ) failPhase = phase;
		if ( phase == 12u ) receiptMutation = 1u;
		if ( phase == 13u ) receiptMutation = 2u;
		CHECK( !VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
		CHECK( !memcmp( &owner, &before, sizeof( owner ) ) );
		CHECK( destroys <= 2u && destroysFence <= 2u
			&& destroysSemaphore <= 2u );
	}

	ResetMocks(); VK_AtmosphericPoolInit( &owner ); aliasSecond = 1u;
	before = owner;
	CHECK( !VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
	CHECK( !memcmp( &owner, &before, sizeof( owner ) ) && destroys == 1u );

	ResetMocks(); VK_AtmosphericPoolInit( &owner ); requireAcquire = 1u;
	CHECK( VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
	CHECK( VK_AtmosphericPoolHasLive( &owner ) );
	CHECK( creates == 2u && begins == 2u && acquires == 1u
		&& destroysFence == 2u && destroysSemaphore == 2u );
	CHECK( VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
	CHECK( creates == 2u );
	CHECK( !VK_AtmosphericPoolEnsure( &owner, &backend2, POOL_BYTES ) );
	CHECK( !VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES * 2u ) );
	receipt = sentinel;
	CHECK( VK_AtmosphericPoolGetReceipt( &owner, &receipt ) );
	CHECK( VK_AtmosphericPoolReceiptExact( &receipt, &receipt ) );
	CHECK( receipt.poolGeneration == 1u && receipt.byteSize == POOL_BYTES );

	for ( mutation = 0u; mutation < 10u; mutation++ ) {
		bad = receipt;
		switch ( mutation ) {
		case 0: bad.schemaVersion++; break;
		case 1: bad.backend = &backend2; break;
		case 2: bad.buffers[0] = bad.buffers[1]; break;
		case 3: bad.allocations[0].requestedSize++; break;
		case 4: bad.allocations[0].allocationGeneration++; break;
		case 5: bad.uploads[0].transfer.request.byteSize--; break;
		case 6: bad.uploads[1].graphicsVisibilityGeneration++; break;
		case 7: bad.poolGeneration++; break;
		case 8: bad.byteSize++; break;
		default: bad.ready = qfalse; break;
		}
		CHECK( !VK_AtmosphericPoolReceiptExact( &receipt, &bad ) );
	}

	VK_AtmosphericPoolRelease( &owner );
	CHECK( !VK_AtmosphericPoolHasLive( &owner ) );
	CHECK( destroys == 2u && destroyOrder[0] == '1'
		&& destroyOrder[1] == '0' );
	VK_AtmosphericPoolRelease( &owner );
	CHECK( destroys == 2u );
	ResetMocks(); requireAcquire = 1u;
	CHECK( VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
	CHECK( VK_AtmosphericPoolGetReceipt( &owner, &receipt2 ) );
	CHECK( receipt2.poolGeneration == 2u );
	VK_AtmosphericPoolRelease( &owner );
	owner.poolGeneration = UINT64_MAX - 1u;
	before = owner;
	CHECK( !VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
	CHECK( !memcmp( &owner, &before, sizeof( owner ) ) );

	ResetMocks(); VK_AtmosphericPoolInit( &owner );
	CHECK( VK_AtmosphericPoolEnsure( &owner, &backend, POOL_BYTES ) );
	CHECK( VK_AtmosphericPoolGetReceipt( &owner, &receipt2 ) );
	CHECK( receipt2.poolGeneration == 1u );
	VK_AtmosphericPoolRelease( &owner );
	puts( "PASS atmospheric pool RAL owner" );
	return 0;
}
