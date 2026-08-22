// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_ral_frame_uniform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; };

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

static struct ralBuffer_s buffers[2];
static ralAllocationReceipt_t allocations[2];
static unsigned char gpuBytes[2][VK_RAL_FRAME_UNIFORM_MAX_BYTES];
static uint64_t createdSizes[2], writeOffsets[8], writeSizes[8];
static unsigned creates, destroys, writes;
static unsigned failCreate, failAllocation, failWrite, aliasSecond;
static char destroyOrder[4];

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
	request.memoryClass = RAL_ALLOCATION_UPLOAD;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = createdSizes[index];
	request.alignment = 16u;
	request.ownerIdentity = (uintptr_t)&buffers[index];
	request.ownerGeneration = 3u + (uint64_t)index;
	facts.backendType = RAL_BACKEND_VULKAN;
	facts.placement = RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize = ( createdSizes[index] + 255u ) & ~UINT64_C( 255 );
	facts.actualAlignment = 16u;
	facts.allocationGeneration = 11u + (uint64_t)index;
	facts.hostVisible = qtrue;
	facts.hostCoherent = qtrue;
	if ( !Ral_AllocationReceiptBuild( &request, &facts,
			&allocations[index] ) ) abort();
}

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *info ) {
	unsigned index = creates++;
	if ( !backend || !info || index >= 2u || failCreate == index + 1u
			|| info->size == 0u || info->size > VK_RAL_FRAME_UNIFORM_MAX_BYTES
			|| info->usage != ( RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST )
			|| info->memory != RAL_MEMORY_HOST_COHERENT
			|| !info->debugName || !info->debugName[0] ) return NULL;
	if ( index == 1u && aliasSecond ) return &buffers[0];
	createdSizes[index] = info->size;
	buffers[index].id = (int)index + 1;
	return &buffers[index];
}

void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	int index = BufferIndex( buffer );
	if ( index < 0 ) abort();
	destroyOrder[destroys++] = (char)( '0' + index );
}

qboolean Ral_BufferGetAllocationReceipt( const ralBuffer_t *buffer,
		ralAllocationReceipt_t *out ) {
	int index = BufferIndex( buffer );
	if ( index < 0 || !out || failAllocation == (unsigned)index + 1u )
		return qfalse;
	BuildAllocation( index );
	*out = allocations[index];
	return qtrue;
}

qboolean Ral_BufferWriteImmediate( ralBuffer_t *buffer, uint64_t offset,
		const void *data, uint64_t size,
		ralBufferUploadReceipt_t *outReceipt ) {
	ralTransferRequest_t request;
	ralTransferReceipt_t prepared, completed;
	ralBufferUploadReceipt_t candidate;
	int index = BufferIndex( buffer );
	if ( index < 0 || !data || !outReceipt || size == 0u
			|| offset > createdSizes[index]
			|| size > createdSizes[index] - offset
			|| failWrite == writes + 1u ) return qfalse;
	writeOffsets[writes] = offset;
	writeSizes[writes] = size;
	writes++;
	memset( &request, 0, sizeof( request ) );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_UPLOAD;
	request.resourceKind = RAL_TRANSFER_BUFFER;
	request.resourceIdentity = (uintptr_t)buffer;
	request.resourceGeneration = allocations[index].allocationGeneration;
	request.byteOffset = offset;
	request.byteSize = size;
	request.byteBudget = allocations[index].committedSize;
	request.queue = RAL_QUEUE_GRAPHICS;
	if ( !Ral_TransferPrepare( &request, (uint64_t)writes, &prepared )
			|| !Ral_TransferPublish( &prepared,
				RAL_TRANSFER_OUTCOME_SYNCHRONOUS, (uint64_t)writes,
				&completed )
			|| !Ral_BufferUploadReceiptBuild( &completed, (uint64_t)writes,
				&candidate ) ) abort();
	memcpy( gpuBytes[index] + offset, data, (size_t)size );
	*outReceipt = candidate;
	return qtrue;
}

static void ResetMocks( void ) {
	creates = destroys = writes = 0u;
	failCreate = failAllocation = failWrite = aliasSecond = 0u;
	memset( createdSizes, 0, sizeof( createdSizes ) );
	memset( gpuBytes, 0, sizeof( gpuBytes ) );
	memset( writeOffsets, 0, sizeof( writeOffsets ) );
	memset( writeSizes, 0, sizeof( writeSizes ) );
	memset( destroyOrder, 0, sizeof( destroyOrder ) );
}

static int FailureMatrix( ralBackend_t *backend,
		const vkRalFrameUniformConfig_t *config ) {
	vkRalFrameUniformOwner_t owner, before;
	unsigned phase;
	for ( phase = 1u; phase <= 5u; phase++ ) {
		ResetMocks();
		VK_RalFrameUniformInit( &owner );
		before = owner;
		if ( phase <= 2u ) failCreate = phase;
		else if ( phase <= 4u ) failAllocation = phase - 2u;
		else aliasSecond = 1u;
		CHECK( !VK_RalFrameUniformEnsure( &owner, backend, config ) );
		CHECK( !memcmp( &owner, &before, sizeof( owner ) ) );
		CHECK( !VK_RalFrameUniformHasLive( &owner ) );
	}
	return 0;
}

int main( void ) {
	struct ralBackend_s backend = { 1 }, backend2 = { 2 };
	vkRalFrameUniformConfig_t particle = {
		144u, 112u, 16u, qtrue, "wired-particle-frame"
	};
	vkRalFrameUniformConfig_t decal = {
		160u, 0u, 0u, qfalse, "wired-decal-frame"
	};
	vkRalFrameUniformConfig_t bad;
	vkRalFrameUniformOwner_t owner, before;
	vkRalFrameUniformResourcesReceipt_t resources, badResources;
	vkRalFrameUniformReceipt_t receipt, receipt2, badReceipt, sentinel;
	unsigned char *shadow;
	void *shadowOut;

	CHECK( FailureMatrix( &backend, &particle ) == 0 );
	VK_RalFrameUniformInit( &owner );
	bad = particle; bad.byteSize = 0u;
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend, &bad ) );
	bad = particle; bad.byteSize = VK_RAL_FRAME_UNIFORM_MAX_BYTES + 1u;
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend, &bad ) );
	bad = particle; bad.partialSize = 0u;
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend, &bad ) );
	bad = particle; bad.partialOffset = 140u; bad.partialSize = 16u;
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend, &bad ) );
	bad = decal; bad.partialOffset = 1u;
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend, &bad ) );

	ResetMocks();
	CHECK( VK_RalFrameUniformEnsure( &owner, &backend, &particle ) );
	CHECK( VK_RalFrameUniformEnsure( &owner, &backend, &particle ) );
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend2, &particle ) );
	CHECK( !VK_RalFrameUniformEnsure( &owner, &backend, &decal ) );
	CHECK( VK_RalFrameUniformGetResources( &owner, &resources )
		&& VK_RalFrameUniformResourcesReceiptExact( &resources, &resources )
		&& resources.byteSize == 144u && resources.partialOffset == 112u
		&& resources.partialSize == 16u
		&& resources.partialRequired == qtrue );
	badResources = resources; badResources.partialSize++;
	CHECK( !VK_RalFrameUniformResourcesReceiptExact( &resources,
		&badResources ) );

	CHECK( VK_RalFrameUniformBeginSlot( &owner, 0u, qfalse, qfalse ) );
	CHECK( VK_RalFrameUniformGetShadow( &owner, 0u, &shadowOut ) );
	shadow = (unsigned char *)shadowOut;
	memset( shadow + 112u, 0x5a, 16u );
	CHECK( VK_RalFrameUniformPublishPartial( &owner, 0u )
		&& writeOffsets[0] == 112u && writeSizes[0] == 16u );
	memset( &sentinel, 0xa5, sizeof( sentinel ) );
	receipt = sentinel;
	CHECK( !VK_RalFrameUniformGetReceipt( &owner, 0u, &receipt )
		&& !memcmp( &receipt, &sentinel, sizeof( receipt ) ) );
	memset( shadow, 0x31, 112u );
	memset( shadow + 128u, 0x32, 16u );
	CHECK( VK_RalFrameUniformPublishFinal( &owner, 0u, &receipt )
		&& writeOffsets[1] == 0u && writeSizes[1] == 144u
		&& !memcmp( gpuBytes[0], shadow, 144u ) );
	CHECK( VK_RalFrameUniformGetReceipt( &owner, 0u, &receipt2 )
		&& VK_RalFrameUniformReceiptExact( &receipt, &receipt2 )
		&& receipt.partialRequired == qtrue
		&& receipt.partialWrite.transfer.request.byteOffset == 112u
		&& receipt.finalWrite.transfer.request.byteSize == 144u );
	shadow[112] ^= 1u;
	receipt2 = sentinel;
	CHECK( !VK_RalFrameUniformPublishFinal( &owner, 0u, &receipt2 )
		&& !memcmp( &receipt2, &sentinel, sizeof( receipt2 ) ) );
	shadow[112] ^= 1u;
	CHECK( VK_RalFrameUniformPublishFinal( &owner, 0u, &receipt2 ) );
	before = owner;
	CHECK( !VK_RalFrameUniformBeginSlot( &owner, 0u, qtrue, qfalse )
		&& !memcmp( &owner, &before, sizeof( owner ) ) );
	CHECK( VK_RalFrameUniformBeginSlot( &owner, 0u, qtrue, qtrue ) );

	badReceipt = receipt2; badReceipt.partialOffset++;
	CHECK( !VK_RalFrameUniformReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.partialHash++;
	CHECK( !VK_RalFrameUniformReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.finalWrite.graphicsVisibilityGeneration++;
	CHECK( !VK_RalFrameUniformReceiptExact( &receipt2, &badReceipt ) );

	VK_RalFrameUniformRelease( &owner );
	CHECK( destroys == 2u && destroyOrder[0] == '1'
		&& destroyOrder[1] == '0' && !VK_RalFrameUniformHasLive( &owner ) );

	// A final-only consumer has no forgeable partial receipt and can finalize
	// directly from its shadow.
	ResetMocks();
	CHECK( VK_RalFrameUniformEnsure( &owner, &backend, &decal ) );
	CHECK( VK_RalFrameUniformBeginSlot( &owner, 1u, qfalse, qfalse ) );
	CHECK( VK_RalFrameUniformGetShadow( &owner, 1u, &shadowOut ) );
	shadow = (unsigned char *)shadowOut;
	memset( shadow, 0x77, 160u );
	CHECK( !VK_RalFrameUniformPublishPartial( &owner, 1u ) );
	CHECK( VK_RalFrameUniformPublishFinal( &owner, 1u, &receipt )
		&& receipt.partialRequired == qfalse
		&& receipt.partialHash == 0u
		&& receipt.partialWrite.ready == qfalse
		&& receipt.finalWrite.transfer.request.byteSize == 160u );
	badReceipt = receipt; badReceipt.partialRequired = qtrue;
	CHECK( !VK_RalFrameUniformReceiptExact( &receipt, &badReceipt ) );
	owner.slotGenerations[1] = UINT64_MAX - 1u;
	before = owner;
	CHECK( !VK_RalFrameUniformBeginSlot( &owner, 1u, qtrue, qtrue )
		&& !memcmp( &owner, &before, sizeof( owner ) ) );
	VK_RalFrameUniformRelease( &owner );

	puts( "PASS shared RAL frame-uniform owner" );
	return 0;
}
