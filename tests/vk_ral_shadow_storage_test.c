// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_ral_shadow_storage.h"

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
static unsigned char gpuBytes[2][256];
static uint64_t createdSizes[2], writeOffsets[32], writeSizes[32];
static ralBufferUsage_t createdUsage[2];
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
			|| info->size == 0u || info->size > sizeof( gpuBytes[index] )
			|| ( info->usage != ( RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST
				| RAL_BUFFER_INDIRECT )
				&& info->usage != ( RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST
					| RAL_BUFFER_INDIRECT | RAL_BUFFER_TRANSFER_SRC ) )
			|| info->memory != RAL_MEMORY_HOST_COHERENT
			|| !info->debugName || !info->debugName[0] ) return NULL;
	if ( index == 1u && aliasSecond ) return &buffers[0];
	createdSizes[index] = info->size;
	createdUsage[index] = info->usage;
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
	memset( createdUsage, 0, sizeof( createdUsage ) );
	memset( gpuBytes, 0x7c, sizeof( gpuBytes ) );
	memset( writeOffsets, 0, sizeof( writeOffsets ) );
	memset( writeSizes, 0, sizeof( writeSizes ) );
	memset( destroyOrder, 0, sizeof( destroyOrder ) );
}

static int FailureMatrix( ralBackend_t *backend,
		const vkRalShadowStorageConfig_t *config ) {
	vkRalShadowStorageOwner_t owner, before;
	unsigned phase;
	for ( phase = 1u; phase <= 5u; phase++ ) {
		ResetMocks();
		VK_RalShadowStorageInit( &owner );
		before = owner;
		if ( phase <= 2u ) failCreate = phase;
		else if ( phase <= 4u ) failAllocation = phase - 2u;
		else aliasSecond = 1u;
		CHECK( !VK_RalShadowStorageEnsure( &owner, backend, config ) );
		CHECK( !memcmp( &owner, &before, sizeof( owner ) ) );
		CHECK( !VK_RalShadowStorageHasLive( &owner ) );
	}
	return 0;
}

int main( void ) {
	struct ralBackend_s backend = { 1 }, backend2 = { 2 };
	vkRalShadowStorageConfig_t pair = { 2u, 16u, 8u, "shadow-pair", 0 };
	vkRalShadowStorageConfig_t single = { 1u, 8u, 4u, "shadow-single", 0 };
	vkRalShadowStorageConfig_t bad;
	vkRalShadowStorageOwner_t owner, before;
	vkRalShadowStorageResourcesReceipt_t resources, badResources;
	vkRalShadowStorageFlushReceipt_t initial0, initial1, receipt, receipt2;
	vkRalShadowStorageFlushReceipt_t badReceipt, sentinel;
	unsigned char value[16], value2[16], untouched[16];
	const void *read;
	unsigned writesBefore;

	CHECK( FailureMatrix( &backend, &pair ) == 0 );
	VK_RalShadowStorageInit( &owner );
	bad = pair; bad.bufferCount = 0u;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	bad = pair; bad.bufferCount = 3u;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	bad = pair; bad.elementSize = 0u;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	bad = pair; bad.elementCount = VK_RAL_SHADOW_STORAGE_MAX_ELEMENTS + 1u;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	bad = pair; bad.elementSize = VK_RAL_SHADOW_STORAGE_MAX_BYTES;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	bad = pair; bad.debugName = NULL;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	bad = pair; bad.extraUsage = RAL_BUFFER_VERTEX;
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );

	ResetMocks();
	CHECK( VK_RalShadowStorageEnsure( &owner, &backend, &pair ) );
	CHECK( VK_RalShadowStorageEnsure( &owner, &backend, &pair ) );
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend2, &pair ) );
	CHECK( !VK_RalShadowStorageEnsure( &owner, &backend, &single ) );
	CHECK( VK_RalShadowStorageGetResources( &owner, &resources ) );
	CHECK( VK_RalShadowStorageResourcesReceiptExact( &resources, &resources ) );
	CHECK( resources.bufferCount == 2u && resources.elementSize == 16u
		&& resources.elementCount == 8u && resources.byteSize == 128u );
	badResources = resources; badResources.elementCount--;
	CHECK( !VK_RalShadowStorageResourcesReceiptExact( &resources,
		&badResources ) );

	// Each shadow begins dirty and seeds only its own GPU buffer.
	CHECK( VK_RalShadowStorageHasDirty( &owner, 0u ) );
	CHECK( VK_RalShadowStorageHasDirty( &owner, 1u ) );
	CHECK( VK_RalShadowStorageFlush( &owner, 0u, &initial0 )
		&& initial0.writeCount == 1u && initial0.dirtyElementCount == 8u
		&& writeOffsets[0] == 0u && writeSizes[0] == 128u );
	CHECK( VK_RalShadowStorageFlush( &owner, 1u, &initial1 )
		&& writeOffsets[1] == 0u && writeSizes[1] == 128u );
	CHECK( !memcmp( gpuBytes[0], gpuBytes[1], 128u ) );
	CHECK( VK_RalShadowStorageGetFlushReceipt( &owner, 0u, &receipt )
		&& VK_RalShadowStorageFlushReceiptExact( &initial0, &receipt ) );

	memset( value, 0x11, sizeof( value ) );
	memset( value2, 0x22, sizeof( value2 ) );
	memset( untouched, 0x7c, sizeof( untouched ) );
	CHECK( VK_RalShadowStorageWriteElement( &owner, 0u, 1u,
		value, sizeof( value ) ) );
	CHECK( VK_RalShadowStorageWriteElement( &owner, 0u, 2u,
		value2, sizeof( value2 ) ) );
	CHECK( VK_RalShadowStorageWriteElement( &owner, 0u, 5u,
		value, sizeof( value ) ) );
	CHECK( VK_RalShadowStorageReadElement( &owner, 0u, 2u, &read )
		&& !memcmp( read, value2, sizeof( value2 ) ) );
	CHECK( VK_RalShadowStorageReadElement( &owner, 1u, 2u, &read )
		&& memcmp( read, value2, sizeof( value2 ) ) );
	memcpy( gpuBytes[0] + 48u, untouched, sizeof( untouched ) );
	writesBefore = writes;
	CHECK( VK_RalShadowStorageFlush( &owner, 0u, &receipt )
		&& receipt.dirtyElementCount == 3u && receipt.writeCount == 2u
		&& writeOffsets[writesBefore] == 16u
		&& writeSizes[writesBefore] == 32u
		&& writeOffsets[writesBefore + 1u] == 80u
		&& writeSizes[writesBefore + 1u] == 16u
		&& !memcmp( gpuBytes[0] + 48u, untouched, sizeof( untouched ) ) );

	// A later run failure publishes neither receipt nor dirty-state change.
	CHECK( VK_RalShadowStorageWriteElement( &owner, 0u, 0u,
		value2, sizeof( value2 ) ) );
	CHECK( VK_RalShadowStorageWriteElement( &owner, 0u, 7u,
		value2, sizeof( value2 ) ) );
	before = owner;
	memset( &sentinel, 0xa5, sizeof( sentinel ) );
	receipt2 = sentinel;
	failWrite = writes + 2u;
	CHECK( !VK_RalShadowStorageFlush( &owner, 0u, &receipt2 )
		&& !memcmp( &receipt2, &sentinel, sizeof( receipt2 ) )
		&& !memcmp( owner.dirty[0], before.dirty[0], owner.elementCount )
		&& owner.flushGenerations[0] == before.flushGenerations[0] );
	failWrite = 0u;
	CHECK( VK_RalShadowStorageFlush( &owner, 0u, &receipt2 )
		&& receipt2.writeCount == 2u && receipt2.dirtyElementCount == 2u );

	// A clean flush is an exact generation-bound no-op receipt.
	writesBefore = writes;
	CHECK( VK_RalShadowStorageFlush( &owner, 0u, &receipt )
		&& receipt.wrote == qfalse && receipt.writeCount == 0u
		&& receipt.firstDirtyElement == UINT32_MAX
		&& receipt.lastDirtyElement == UINT32_MAX && writes == writesBefore );
	CHECK( VK_RalShadowStorageGetFlushReceipt( &owner, 0u, &receipt2 )
		&& VK_RalShadowStorageFlushReceiptExact( &receipt, &receipt2 ) );
	badReceipt = receipt2; badReceipt.flushGeneration++;
	CHECK( !VK_RalShadowStorageFlushReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = receipt2; badReceipt.wrote = qtrue;
	CHECK( !VK_RalShadowStorageFlushReceiptExact( &receipt2, &badReceipt ) );
	badReceipt = initial0; badReceipt.firstWrite.transfer.request.byteOffset++;
	CHECK( !VK_RalShadowStorageFlushReceiptExact( &initial0, &badReceipt ) );

	owner.shadowGenerations[0] = UINT64_MAX - 1u;
	before = owner;
	CHECK( !VK_RalShadowStorageWriteElement( &owner, 0u, 0u,
		value, sizeof( value ) ) && !memcmp( &owner, &before, sizeof( owner ) ) );
	owner.shadowGenerations[0] = receipt.shadowGeneration;
	owner.flushGenerations[0] = UINT64_MAX - 1u;
	before = owner;
	receipt2 = sentinel;
	CHECK( !VK_RalShadowStorageFlush( &owner, 0u, &receipt2 )
		&& !memcmp( &owner, &before, sizeof( owner ) )
		&& !memcmp( &receipt2, &sentinel, sizeof( receipt2 ) ) );

	VK_RalShadowStorageRelease( &owner );
	CHECK( destroys == 2u && destroyOrder[0] == '1'
		&& destroyOrder[1] == '0' && !VK_RalShadowStorageHasLive( &owner ) );

	// Readback-capable owners opt into transfer-source usage explicitly.
	ResetMocks();
	VK_RalShadowStorageInit( &owner );
	bad = pair; bad.extraUsage = RAL_BUFFER_TRANSFER_SRC;
	CHECK( VK_RalShadowStorageEnsure( &owner, &backend, &bad ) );
	CHECK( owner.extraUsage == RAL_BUFFER_TRANSFER_SRC
		&& ( createdUsage[0] & RAL_BUFFER_TRANSFER_SRC ) != 0
		&& ( createdUsage[1] & RAL_BUFFER_TRANSFER_SRC ) != 0 );
	VK_RalShadowStorageRelease( &owner );

	ResetMocks();
	VK_RalShadowStorageInit( &owner );
	CHECK( VK_RalShadowStorageEnsure( &owner, &backend, &single ) );
	CHECK( VK_RalShadowStorageGetResources( &owner, &resources )
		&& VK_RalShadowStorageResourcesReceiptExact( &resources, &resources )
		&& resources.buffers[1] == NULL );
	CHECK( VK_RalShadowStorageFlush( &owner, 0u, &receipt )
		&& receipt.bufferIndex == 0u && receipt.elementCount == 4u );
	CHECK( !VK_RalShadowStorageFlush( &owner, 1u, &receipt2 ) );
	VK_RalShadowStorageRelease( &owner );

	puts( "PASS RAL shadow-storage owner" );
	return 0;
}
