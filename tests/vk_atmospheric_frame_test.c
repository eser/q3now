// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_atmospheric_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ralBackend_s { int id; uint64_t writeGeneration; };
struct ralBuffer_s { int id; };

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

static struct ralBuffer_s buffers[2];
static ralAllocationReceipt_t allocations[2];
static unsigned char gpuBytes[2][VK_ATMOSPHERIC_FRAME_BYTE_SIZE];
static unsigned creates, destroys, allocationGets, writes;
static unsigned failCreate, failAllocation, failWrite, aliasSecond;
static uint64_t writeOffsets[8], writeSizes[8];
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
	request.size = VK_ATMOSPHERIC_FRAME_BYTE_SIZE;
	request.alignment = 16u;
	request.ownerIdentity = (uintptr_t)&buffers[index];
	request.ownerGeneration = 3u + (uint64_t)index;
	facts.backendType = RAL_BACKEND_VULKAN;
	facts.placement = RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize = 256u;
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
			|| info->size != VK_ATMOSPHERIC_FRAME_BYTE_SIZE
			|| info->usage != ( RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST )
			|| info->memory != RAL_MEMORY_HOST_COHERENT ) return NULL;
	if ( index == 1u && aliasSecond ) return &buffers[0];
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
	allocationGets++;
	if ( index < 0 || !out || failAllocation == (unsigned)index + 1u )
		return qfalse;
	BuildAllocation( index );
	*out = allocations[index];
	return qtrue;
}

qboolean Ral_BufferWriteImmediate( ralBuffer_t *buffer, uint64_t offset,
		const void *data, uint64_t size,
		ralBufferUploadReceipt_t *outReceipt ) {
	struct ralBackend_s *backend;
	ralTransferRequest_t request;
	ralTransferReceipt_t prepared, completed;
	ralBufferUploadReceipt_t candidate;
	int index = BufferIndex( buffer );
	if ( index < 0 || !data || !outReceipt || size == 0u
			|| offset > VK_ATMOSPHERIC_FRAME_BYTE_SIZE
			|| size > VK_ATMOSPHERIC_FRAME_BYTE_SIZE - offset
			|| failWrite == writes + 1u ) return qfalse;
	backend = (struct ralBackend_s *)(uintptr_t)1u;
	(void)backend;
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
	creates = destroys = allocationGets = writes = 0u;
	failCreate = failAllocation = failWrite = aliasSecond = 0u;
	memset( gpuBytes, 0, sizeof( gpuBytes ) );
	memset( writeOffsets, 0, sizeof( writeOffsets ) );
	memset( writeSizes, 0, sizeof( writeSizes ) );
	memset( destroyOrder, 0, sizeof( destroyOrder ) );
}

static int EnsureFailureMatrix( ralBackend_t *backend ) {
	vkAtmosphericFrameOwner_t owner, before;
	unsigned phase;
	for ( phase = 1u; phase <= 5u; phase++ ) {
		ResetMocks();
		VK_AtmosphericFrameInit( &owner );
		before = owner;
		if ( phase <= 2u ) failCreate = phase;
		else if ( phase <= 4u ) failAllocation = phase - 2u;
		else aliasSecond = 1u;
		CHECK( !VK_AtmosphericFrameEnsure( &owner, backend ) );
		CHECK( !memcmp( &owner, &before, sizeof( owner ) ) );
		CHECK( !VK_AtmosphericFrameHasLive( &owner ) );
	}
	return 0;
}

int main( void ) {
	struct ralBackend_s backend = { 1, 0 }, backend2 = { 2, 0 };
	vkAtmosphericFrameOwner_t owner, before;
	vkAtmosphericFrameResourcesReceipt_t resources, resources2, badResources;
	vkAtmosphericFrameReceipt_t receipt, receipt2, badReceipt, sentinel;
	unsigned char *shadow;
	void *shadowOut;
	uint64_t slotGeneration;

	CHECK( EnsureFailureMatrix( &backend ) == 0 );
	ResetMocks();
	VK_AtmosphericFrameInit( &owner );
	CHECK( VK_AtmosphericFrameEnsure( &owner, &backend ) );
	CHECK( creates == 2u && allocationGets == 2u
		&& VK_AtmosphericFrameHasLive( &owner ) );
	CHECK( VK_AtmosphericFrameEnsure( &owner, &backend ) );
	CHECK( !VK_AtmosphericFrameEnsure( &owner, &backend2 ) );
	CHECK( VK_AtmosphericFrameGetResources( &owner, &resources ) );
	CHECK( VK_AtmosphericFrameResourcesReceiptExact( &resources, &resources )
		&& resources.ownerGeneration == 1u
		&& resources.byteSize == VK_ATMOSPHERIC_FRAME_BYTE_SIZE );
	badResources = resources;
	badResources.allocations[1].allocationGeneration++;
	CHECK( !VK_AtmosphericFrameResourcesReceiptExact( &resources,
		&badResources ) );

	// A never-submitted first slot needs no fence. The compute owner publishes
	// only bytes 96..195; a final receipt is unavailable until render publishes
	// the complete shadow.
	CHECK( VK_AtmosphericFrameBeginSlot( &owner, 0u, qfalse, qfalse ) );
	CHECK( VK_AtmosphericFrameGetShadow( &owner, 0u, &shadowOut ) );
	shadow = (unsigned char *)shadowOut;
	memset( shadow + VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET, 0x5a,
		VK_ATMOSPHERIC_FRAME_COMPUTE_SIZE );
	CHECK( VK_AtmosphericFramePublishCompute( &owner, 0u ) );
	memset( &sentinel, 0xa5, sizeof( sentinel ) );
	receipt = sentinel;
	CHECK( !VK_AtmosphericFrameGetReceipt( &owner, 0u, &receipt )
		&& !memcmp( &receipt, &sentinel, sizeof( receipt ) ) );
	CHECK( writeOffsets[0] == VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET
		&& writeSizes[0] == VK_ATMOSPHERIC_FRAME_COMPUTE_SIZE );
	CHECK( VK_AtmosphericFrameGetShadow( &owner, 0u, &shadowOut ) );
	memset( shadow, 0x31, VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET );
	memset( shadow + 196u, 0x32, 12u );
	CHECK( VK_AtmosphericFramePublishFinal( &owner, 0u, &receipt ) );
	CHECK( writeOffsets[1] == 0u
		&& writeSizes[1] == VK_ATMOSPHERIC_FRAME_BYTE_SIZE
		&& !memcmp( gpuBytes[0], shadow, sizeof( gpuBytes[0] ) ) );
	CHECK( VK_AtmosphericFrameGetReceipt( &owner, 0u, &receipt2 )
		&& VK_AtmosphericFrameReceiptExact( &receipt, &receipt2 )
		&& receipt.commandSlot == 0u && receipt.slotGeneration == 1u
		&& receipt.fenceRequired == qfalse
		&& receipt.fenceCompleted == qfalse
		&& receipt.partialWrite.transfer.request.byteOffset == 96u
		&& receipt.partialWrite.transfer.request.byteSize == 100u
		&& receipt.finalWrite.transfer.request.byteOffset == 0u
		&& receipt.finalWrite.transfer.request.byteSize == 208u );
	CHECK( VK_AtmosphericFrameGetShadow( &owner, 0u, &shadowOut ) );
	shadow[1] ^= 1u;
	CHECK( VK_AtmosphericFramePublishFinal( &owner, 0u, &receipt2 )
		&& receipt2.finalWrite.transfer.transferGeneration
			> receipt.finalWrite.transfer.transferGeneration );
	receipt = receipt2;

	// Shadow mutation after finalization invalidates the current receipt.
	shadow[0] ^= 1u;
	receipt2 = sentinel;
	CHECK( !VK_AtmosphericFrameGetReceipt( &owner, 0u, &receipt2 )
		&& !memcmp( &receipt2, &sentinel, sizeof( receipt2 ) ) );
	shadow[0] ^= 1u;

	// Submitted-slot reuse is exact-fence gated and output-atomic on rejection.
	before = owner;
	CHECK( !VK_AtmosphericFrameBeginSlot( &owner, 0u, qtrue, qfalse )
		&& !memcmp( &owner, &before, sizeof( owner ) ) );
	CHECK( VK_AtmosphericFrameBeginSlot( &owner, 0u, qtrue, qtrue ) );
	CHECK( owner.slotGenerations[0] == 2u );
	CHECK( VK_AtmosphericFrameBeginSlot( &owner, 1u, qfalse, qfalse ) );
	CHECK( owner.slotGenerations[1] == 1u );

	// Write failure consumes no publication state. A compute-region mutation
	// after its publication prevents render from finalizing stale mixed bytes.
	CHECK( VK_AtmosphericFrameGetShadow( &owner, 1u, &shadowOut ) );
	shadow = (unsigned char *)shadowOut;
	memset( shadow + 96u, 0x77, 100u );
	failWrite = writes + 1u;
	before = owner;
	CHECK( !VK_AtmosphericFramePublishCompute( &owner, 1u )
		&& !memcmp( &owner, &before, sizeof( owner ) ) );
	failWrite = 0u;
	CHECK( VK_AtmosphericFramePublishCompute( &owner, 1u ) );
	shadow[96] ^= 1u;
	receipt2 = sentinel;
	CHECK( !VK_AtmosphericFramePublishFinal( &owner, 1u, &receipt2 )
		&& !memcmp( &receipt2, &sentinel, sizeof( receipt2 ) ) );
	shadow[96] ^= 1u;
	CHECK( VK_AtmosphericFramePublishFinal( &owner, 1u, &receipt2 ) );

	// Every receipt field is exact, including backend/allocation/write/fence/hash.
	badReceipt = receipt2; badReceipt.backend = &backend2;
	CHECK( !VK_AtmosphericFrameReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.allocation.allocationGeneration++;
	CHECK( !VK_AtmosphericFrameReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.finalWrite.graphicsVisibilityGeneration++;
	CHECK( !VK_AtmosphericFrameReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.slotGeneration++;
	CHECK( !VK_AtmosphericFrameReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.contentHash++;
	CHECK( !VK_AtmosphericFrameReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.fenceRequired = qtrue;
	CHECK( !VK_AtmosphericFrameReceiptExact( &receipt2, &badReceipt ) );

	// Persistent generation ceilings reject before mutation. Release is child-
	// complete, reverse-slot ordered, and recreate advances owner identity.
	slotGeneration = owner.slotGenerations[1];
	owner.slotGenerations[1] = UINT64_MAX - 1u;
	before = owner;
	CHECK( !VK_AtmosphericFrameBeginSlot( &owner, 1u, qtrue, qtrue )
		&& !memcmp( &owner, &before, sizeof( owner ) ) );
	owner.slotGenerations[1] = slotGeneration;
	VK_AtmosphericFrameRelease( &owner );
	CHECK( destroys == 2u && destroyOrder[0] == '1'
		&& destroyOrder[1] == '0'
		&& !VK_AtmosphericFrameHasLive( &owner ) );
	ResetMocks();
	CHECK( VK_AtmosphericFrameEnsure( &owner, &backend ) );
	CHECK( VK_AtmosphericFrameGetResources( &owner, &resources2 )
		&& resources2.ownerGeneration == 2u
		&& !VK_AtmosphericFrameResourcesReceiptExact( &resources,
			&resources2 ) );
	VK_AtmosphericFrameRelease( &owner );

	puts( "PASS atmospheric RAL frame-uniform owner" );
	return 0;
}
