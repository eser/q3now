// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../code/render/ral/backends/vulkan/renderer/vk_ral_buffer_shadow.h"

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; };

static int failures;
static struct ralBuffer_s fakeBuffer;
static uint64_t createdSize;
static ralBufferUsage_t createdUsage;
static ralAllocationReceipt_t allocation;
static uint64_t nextTransferGeneration;
static unsigned char gpuBytes[128];

#define CHECK( expression ) do { if ( !( expression ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #expression ); \
	failures++; } } while ( 0 )

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *createInfo ) {
	if ( !backend || !createInfo ) return NULL;
	fakeBuffer.id = 1;
	createdSize = createInfo->size;
	createdUsage = createInfo->usage;
	return &fakeBuffer;
}

void Ral_DestroyBuffer( ralBuffer_t *buffer ) { (void)buffer; }

qboolean Ral_BufferGetAllocationReceipt( const ralBuffer_t *buffer,
		ralAllocationReceipt_t *outReceipt ) {
	if ( buffer != &fakeBuffer || !outReceipt ) return qfalse;
	*outReceipt = allocation;
	return qtrue;
}

qboolean Ral_BufferWriteImmediate( ralBuffer_t *buffer, uint64_t offset,
		const void *data, uint64_t size, ralBufferUploadReceipt_t *outReceipt ) {
	ralTransferRequest_t request;
	ralTransferReceipt_t prepared, completed;
	if ( buffer != &fakeBuffer || !data || !outReceipt || size == 0u
			|| offset + size > sizeof( gpuBytes ) ) return qfalse;
	memcpy( gpuBytes + offset, data, (size_t)size );
	memset( &request, 0, sizeof( request ) );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_UPLOAD;
	request.resourceKind = RAL_TRANSFER_BUFFER;
	request.resourceIdentity = (uintptr_t)buffer;
	request.resourceGeneration = allocation.allocationGeneration;
	request.byteOffset = offset;
	request.byteSize = size;
	request.byteBudget = allocation.committedSize;
	request.queue = RAL_QUEUE_GRAPHICS;
	nextTransferGeneration++;
	return Ral_TransferPrepare( &request, nextTransferGeneration, &prepared )
		&& Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_SYNCHRONOUS,
			nextTransferGeneration, &completed )
		&& Ral_BufferUploadReceiptBuild( &completed,
			nextTransferGeneration, outReceipt );
}

int main( void ) {
	vkRalBufferShadow_t shadow;
	vkRalBufferShadowWriteReceipt_t receipt, copy;
	struct ralBackend_s backendValue = { 1 };
	ralBackend_t *backend = &backendValue;
	ralAllocationRequest_t allocationRequest;
	ralAllocationFacts_t allocationFacts;
	const unsigned char data[] = { 1u, 2u, 3u, 4u };

	VK_RalBufferShadowInit( &shadow );
	memset( &allocationRequest, 0, sizeof( allocationRequest ) );
	memset( &allocationFacts, 0, sizeof( allocationFacts ) );
	allocationRequest.memoryClass = RAL_ALLOCATION_UPLOAD;
	allocationRequest.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	allocationRequest.size = 128u;
	allocationRequest.alignment = 16u;
	allocationRequest.ownerIdentity = (uintptr_t)&fakeBuffer;
	allocationRequest.ownerGeneration = 1u;
	allocationFacts.backendType = RAL_BACKEND_VULKAN;
	allocationFacts.placement = RAL_ALLOCATION_PLACEMENT_DEDICATED;
	allocationFacts.committedSize = 128u;
	allocationFacts.actualAlignment = 16u;
	allocationFacts.allocationGeneration = 1u;
	allocationFacts.hostVisible = qtrue;
	allocationFacts.hostCoherent = qtrue;
	CHECK( Ral_AllocationReceiptBuild( &allocationRequest, &allocationFacts,
		&allocation ) );
	CHECK( !VK_RalBufferShadowHasLive( &shadow ) );
	CHECK( VK_RalBufferShadowEnsure( &shadow, backend, 128u,
		(ralBufferUsage_t)( RAL_BUFFER_VERTEX | RAL_BUFFER_UNIFORM ),
		"shadow-test" ) );
	CHECK( createdSize == 128u );
	CHECK( createdUsage == ( RAL_BUFFER_VERTEX | RAL_BUFFER_UNIFORM
		| RAL_BUFFER_TRANSFER_DST ) );
	CHECK( VK_RalBufferShadowHasLive( &shadow ) );
	CHECK( VK_RalBufferShadowWrite( &shadow, 12u, data, sizeof( data ) ) );
	CHECK( VK_RalBufferShadowMarkWritten( &shadow, 12u, sizeof( data ) ) );
	CHECK( !VK_RalBufferShadowPublish( &shadow, 12u, 0u, &receipt ) );
	CHECK( VK_RalBufferShadowPublish( &shadow, 12u, sizeof( data ), &receipt ) );
	CHECK( memcmp( gpuBytes + 12u, data, sizeof( data ) ) == 0 );
	copy = receipt;
	CHECK( VK_RalBufferShadowWriteReceiptExact( &receipt, &copy ) );
	CHECK( !VK_RalBufferShadowWrite( &shadow, 127u, data, sizeof( data ) ) );
	VK_RalBufferShadowRelease( &shadow );
	CHECK( !VK_RalBufferShadowHasLive( &shadow ) );
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
