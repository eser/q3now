// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_motion_payload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL temporal payload line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; byte *bytes; uint64_t size; qboolean external; };
struct ralBindGroupLayout_s { int id; };
struct ralBindGroup_s { int id; };

static struct ralBuffer_s buffers[32];
static struct ralBindGroupLayout_s layouts[8];
static struct ralBindGroup_s groups[32];
static ralBufferCreateInfo_t bufferInfos[32];
static ralBindEntry_t capturedEntries[2];
static ralBindGroupLayoutCreateInfo_t capturedLayout;
static ralBindingValue_t groupValues[32][2];
static ralBindGroupCreateInfo_t groupInfos[32];
static int layoutCalls, bufferCalls, mapCalls, groupCalls;
static int layoutDestroys, bufferDestroys, unmaps, groupDestroys;
static int flushCalls;
static uint64_t flushOffset, flushSize;
static int failLayoutAt, failBufferAt, failMapAt, failGroupAt;
static ralBuffer_t *aliasBufferAt;
static ralBindGroup_t *aliasGroupAt;
static char destroyEvents[128];
static int destroyEventCount;

ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *backend,
		const ralBindGroupLayoutCreateInfo_t *ci ) {
	int call = ++layoutCalls;
	(void)backend;
	if ( call == failLayoutAt ) return NULL;
	capturedLayout = *ci;
	if ( ci->numEntries != 2 ) abort();
	memcpy( capturedEntries, ci->entries, sizeof( capturedEntries ) );
	capturedLayout.entries = capturedEntries;
	layouts[call].id = call;
	return &layouts[call];
}

void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout ) {
	(void)layout;
	layoutDestroys++;
	destroyEvents[destroyEventCount++] = 'L';
}

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *ci ) {
	int call = ++bufferCalls;
	(void)backend;
	bufferInfos[call] = *ci;
	if ( call == failBufferAt ) return NULL;
	if ( aliasBufferAt ) {
		ralBuffer_t *result = aliasBufferAt;
		aliasBufferAt = NULL;
		return result;
	}
	buffers[call].id = call;
	buffers[call].size = ci->size;
	buffers[call].bytes = (byte *)malloc( (size_t)ci->size );
	if ( !buffers[call].bytes ) return NULL;
	memset( buffers[call].bytes, 0xa5, (size_t)ci->size );
	return &buffers[call];
}

void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	struct ralBuffer_s *fake = (struct ralBuffer_s *)buffer;
	bufferDestroys++;
	destroyEvents[destroyEventCount++] = 'B';
	if ( !fake->external ) {
		free( fake->bytes );
		fake->bytes = NULL;
	}
}

void *Ral_MapBuffer( ralBuffer_t *buffer ) {
	int call = ++mapCalls;
	if ( call == failMapAt ) return NULL;
	return ((struct ralBuffer_s *)buffer)->bytes;
}

void Ral_UnmapBuffer( ralBuffer_t *buffer ) {
	(void)buffer;
	unmaps++;
	destroyEvents[destroyEventCount++] = 'U';
}

void Ral_FlushBuffer( ralBuffer_t *buffer, uint64_t offset, uint64_t size ) {
	(void)buffer;
	flushCalls++;
	flushOffset = offset;
	flushSize = size;
}

ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *backend,
		const ralBindGroupCreateInfo_t *ci ) {
	int call = ++groupCalls;
	(void)backend;
	if ( call == failGroupAt ) return NULL;
	if ( aliasGroupAt ) {
		ralBindGroup_t *result = aliasGroupAt;
		aliasGroupAt = NULL;
		return result;
	}
	groupInfos[call] = *ci;
	if ( ci->numValues != 2 ) abort();
	memcpy( groupValues[call], ci->values, sizeof( groupValues[call] ) );
	groupInfos[call].values = groupValues[call];
	groups[call].id = call;
	return &groups[call];
}

void Ral_DestroyBindGroup( ralBindGroup_t *group ) {
	(void)group;
	groupDestroys++;
	destroyEvents[destroyEventCount++] = 'G';
}

static void FillMatrices( temporalMotionMatrices_t *m, float base ) {
	uint32_t i;
	for ( i = 0; i < 16; ++i ) {
		m->currentMvp[i] = base + (float)i;
		m->previousMvp[i] = base + 32.0f + (float)i;
	}
}

int main( void ) {
	struct ralBackend_s backendA = { 1 }, backendB = { 2 };
	struct ralBuffer_s entityA = { 100, NULL, 4096, qtrue };
	struct ralBuffer_s entityB = { 101, NULL, 4096, qtrue };
	struct ralBuffer_s entityC = { 102, NULL, 4096, qtrue };
	temporalMotionPayloadOwner_t owner, beforeOwner;
	temporalMotionMatrices_t matrices, matrixBefore;
	temporalMotionGpuPayload_t *records;
	byte recordBefore[sizeof( temporalMotionGpuPayload_t )];
	uint32_t out, sentinel = 0xdeadbeefu;
	uint32_t layoutGeneration = 0xfeedbeefu;
	const ralBindGroupLayout_t *publishedLayout = (const ralBindGroupLayout_t *)0x1;
	int lc, bc, mc, gc, gd, bd, baseEvent;

	CHECK( sizeof( temporalMotionGpuPayload_t ) == 144 );
	CHECK( offsetof( temporalMotionGpuPayload_t, previousMvp ) == 64 );
	CHECK( offsetof( temporalMotionGpuPayload_t, outcome ) == 128 );
	CHECK( TEMPORAL_MOTION_WRITE_VALID == 1 );
	CHECK( TEMPORAL_MOTION_INVALIDATE_OPAQUE == 2 );
	R_TemporalMotionPayloadInit( &owner );
	CHECK( !R_TemporalMotionPayloadGetLayout( &owner, &publishedLayout,
		&layoutGeneration ) );
	CHECK( publishedLayout == (const ralBindGroupLayout_t *)0x1
		&& layoutGeneration == 0xfeedbeefu );
	beforeOwner = owner;
	CHECK( !R_TemporalMotionPayloadEnsure( NULL, &backendA, 2, 0, 4, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, NULL, 2, 0, 4, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 0, 0, 4, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 5, 0, 4, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 2, 4, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 0, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, NULL, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityA, 0 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0,
		TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS + 1u, &entityA, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );

	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityA, 1 ) );
	CHECK( owner.ready && owner.backend == &backendA && owner.frameCount == 2 );
	CHECK( R_TemporalMotionPayloadGetLayout( &owner, &publishedLayout,
		&layoutGeneration ) );
	CHECK( publishedLayout == owner.layout && layoutGeneration == 1
		&& owner.layoutAllocationGeneration == 1 );
	CHECK( owner.frames[0].ready && owner.frames[0].capacity == 4
		&& owner.frames[0].allocationGeneration == 1
		&& owner.frames[0].entityBuffer == &entityA );
	CHECK( layoutCalls == 1 && bufferCalls == 1 && mapCalls == 1 && groupCalls == 1 );
	CHECK( capturedLayout.numEntries == 2 && !capturedLayout.bindless );
	CHECK( capturedEntries[0].binding == 0
		&& capturedEntries[0].type == RAL_BIND_STORAGE_BUFFER
		&& capturedEntries[0].count == 1
		&& capturedEntries[0].stageFlags == RAL_STAGE_VERTEX );
	CHECK( capturedEntries[1].binding == 1
		&& capturedEntries[1].type == RAL_BIND_STORAGE_BUFFER
		&& capturedEntries[1].count == 1
		&& capturedEntries[1].stageFlags == RAL_STAGE_VERTEX );
	CHECK( bufferInfos[1].size == 4u * sizeof( temporalMotionGpuPayload_t )
		&& bufferInfos[1].usage == RAL_BUFFER_STORAGE
		&& bufferInfos[1].memory == RAL_MEMORY_HOST_COHERENT );
	CHECK( groupValues[1][0].binding == 0
		&& groupValues[1][0].buffer == &entityA
		&& groupValues[1][0].type == RAL_BIND_STORAGE_BUFFER );
	CHECK( groupValues[1][1].binding == 1
		&& groupValues[1][1].buffer == owner.frames[0].buffer
		&& groupValues[1][1].type == RAL_BIND_STORAGE_BUFFER );
	CHECK( R_TemporalMotionPayloadGetBindGroup( &owner, 0 ) == owner.frames[0].bindGroup );

	beforeOwner = owner; lc = layoutCalls; bc = bufferCalls; mc = mapCalls; gc = groupCalls;
	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 3, &entityA, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );
	CHECK( layoutCalls == lc && bufferCalls == bc && mapCalls == mc && groupCalls == gc );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendB, 2, 0, 4, &entityA, 1 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 3, 0, 4, &entityA, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );

	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 1, 3, &entityC, 1 ) );
	CHECK( owner.frames[1].allocationGeneration == 1
		&& owner.frames[1].buffer != owner.frames[0].buffer
		&& owner.frames[1].bindGroup != owner.frames[0].bindGroup );
	// First materialization is authored at the completed-fence seam and now
	// carries the same one-shot reset authority as a reused frame.
	CHECK( owner.frames[1].resetAfterFence && !owner.frames[1].begun );

	// Borrowed identity is allocation pointer + generation. Reusing the same
	// wrapper at a new generation rebuilds only the composite group.
	CHECK( R_TemporalMotionPayloadResetAfterFence( &owner, 1 ) );
	beforeOwner = owner; bc = bufferCalls; gc = groupCalls; gd = groupDestroys;
	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 1, 3, &entityC, 2 ) );
	CHECK( owner.frames[1].buffer == beforeOwner.frames[1].buffer
		&& owner.frames[1].entityBuffer == &entityC
		&& owner.frames[1].entityAllocationGeneration == 2
		&& owner.frames[1].allocationGeneration == beforeOwner.frames[1].allocationGeneration + 1
		&& bufferCalls == bc && groupCalls == gc + 1 && groupDestroys == gd + 1 );
	CHECK( R_TemporalMotionPayloadBeginFrame( &owner, 1 ) );
	beforeOwner = owner; gd = groupDestroys;
	CHECK( !R_TemporalMotionPayloadDetachEntityBuffer( &owner, 1, &entityC, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& groupDestroys == gd );

	// Borrowed entity buffers cannot alias this frame's temporal buffer, another
	// frame's temporal buffer, or another frame's borrowed entity allocation.
	beforeOwner = owner; bc = bufferCalls; gc = groupCalls;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4,
		owner.frames[0].buffer, 3 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4,
		owner.frames[1].buffer, 3 ) );
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4,
		&entityC, 3 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferCalls == bc && groupCalls == gc );
	CHECK( R_TemporalMotionPayloadBeginFrame( &owner, 0 ) );
	CHECK( !R_TemporalMotionPayloadBeginFrame( &owner, 0 ) );
	CHECK( R_TemporalMotionPayloadResetAfterFence( &owner, 0 ) );
	CHECK( R_TemporalMotionPayloadBeginFrame( &owner, 0 ) );
	CHECK( !R_TemporalMotionPayloadBeginFrame( &owner, 0 ) );
	FillMatrices( &matrices, 10.0f );
	matrixBefore = matrices;
	out = sentinel;
	CHECK( R_TemporalMotionPayloadAppendAt( &owner, 0, 1,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) );
	CHECK( out == 1 && memcmp( &matrices, &matrixBefore, sizeof( matrices ) ) == 0 );
	CHECK( flushCalls == 1 && flushOffset == sizeof( temporalMotionGpuPayload_t )
		&& flushSize == sizeof( temporalMotionGpuPayload_t ) );
	records = (temporalMotionGpuPayload_t *)owner.frames[0].mapped;
	CHECK( memcmp( records[1].currentMvp, matrices.currentMvp, 64 ) == 0
		&& memcmp( records[1].previousMvp, matrices.previousMvp, 64 ) == 0
		&& records[1].outcome == TEMPORAL_MOTION_WRITE_VALID
		&& records[1].reserved[0] == 0 && records[1].reserved[1] == 0
		&& records[1].reserved[2] == 0 );
	out = sentinel;
	CHECK( R_TemporalMotionPayloadAppendAt( &owner, 0, 3,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) && out == 3 );
	memcpy( recordBefore, &records[2], sizeof( recordBefore ) );
	beforeOwner = owner; out = sentinel;
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 3,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) );
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 2,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) );
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 4,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) );
	CHECK( out == sentinel && memcmp( &records[2], recordBefore, sizeof( recordBefore ) ) == 0
		&& memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );

	CHECK( R_TemporalMotionPayloadResetAfterFence( &owner, 0 ) );
	CHECK( R_TemporalMotionPayloadBeginFrame( &owner, 0 ) );
	memcpy( recordBefore, &records[0], sizeof( recordBefore ) );
	matrices.currentMvp[7] = NAN; out = sentinel; beforeOwner = owner;
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 0,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) );
	CHECK( out == sentinel && memcmp( &records[0], recordBefore, sizeof( recordBefore ) ) == 0
		&& memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );
	matrices.currentMvp[7] = INFINITY;
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 0,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &out ) );
	FillMatrices( &matrices, 20.0f );
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 0,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, &matrices, &out ) );
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 0,
		TEMPORAL_MOTION_PRESERVE, NULL, &out ) );
	CHECK( !R_TemporalMotionPayloadAppendAt( &owner, 0, 0,
		TEMPORAL_MOTION_DEFER_ATEST, NULL, &out ) );
	CHECK( R_TemporalMotionPayloadAppendAt( &owner, 0, 0,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, NULL, &out ) && out == 0 );
	CHECK( records[0].outcome == TEMPORAL_MOTION_INVALIDATE_OPAQUE );
	for ( size_t i = 0; i < 16; ++i )
		CHECK( records[0].currentMvp[i] == 0.0f && !signbit( records[0].currentMvp[i] )
			&& records[0].previousMvp[i] == 0.0f && !signbit( records[0].previousMvp[i] ) );

	// Replacement while begun/in-flight is mechanically rejected. After the
	// frame fence, the same temporal buffer can rebuild only its composite group.
	beforeOwner = owner; bc = bufferCalls; mc = mapCalls; gc = groupCalls; gd = groupDestroys;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferCalls == bc && groupCalls == gc );
	CHECK( R_TemporalMotionPayloadResetAfterFence( &owner, 0 ) );
	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityB, 2 ) );
	CHECK( bufferCalls == bc && mapCalls == mc && groupCalls == gc + 1
		&& groupDestroys == gd + 1 );
	CHECK( owner.frames[0].buffer == beforeOwner.frames[0].buffer
		&& owner.frames[0].entityBuffer == &entityB
		&& owner.frames[0].allocationGeneration == beforeOwner.frames[0].allocationGeneration + 1 );
	beforeOwner = owner; failGroupAt = groupCalls + 1;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityA, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );
	failGroupAt = 0;
	aliasGroupAt = owner.frames[0].bindGroup;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityA, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );
	aliasGroupAt = owner.frames[1].bindGroup;
	gd = groupDestroys;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 4, &entityA, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& groupDestroys == gd );

	// Growth replaces group then unmaps/destroys the old buffer.
	baseEvent = destroyEventCount; beforeOwner = owner;
	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 8, &entityB, 2 ) );
	CHECK( owner.frames[0].capacity == 8
		&& owner.frames[0].allocationGeneration == beforeOwner.frames[0].allocationGeneration + 1 );
	CHECK( destroyEvents[baseEvent] == 'G'
		&& destroyEvents[baseEvent + 1] == 'U'
		&& destroyEvents[baseEvent + 2] == 'B' );
	beforeOwner = owner; bd = bufferDestroys; gd = groupDestroys;
	failMapAt = mapCalls + 1;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferDestroys == bd + 1 && groupDestroys == gd );
	failMapAt = 0;
	bd = bufferDestroys; gd = groupDestroys; failGroupAt = groupCalls + 1;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferDestroys == bd + 1 && groupDestroys == gd );
	failGroupAt = 0;
	aliasBufferAt = owner.frames[0].buffer;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );
	aliasBufferAt = &entityB;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0 );
	aliasBufferAt = owner.frames[1].buffer;
	bd = bufferDestroys;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferDestroys == bd );
	aliasBufferAt = &entityC;
	bd = bufferDestroys;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferDestroys == bd );

	// Detach is exact and fence-authorized. It destroys the group before the
	// borrowed allocation can die, retains the temporal buffer, then permits a
	// generation-safe group-only rebuild.
	CHECK( R_TemporalMotionPayloadResetAfterFence( &owner, 1 ) );
	beforeOwner = owner; gd = groupDestroys;
	CHECK( !R_TemporalMotionPayloadDetachEntityBuffer( &owner, 1, &entityA, 2 ) );
	CHECK( !R_TemporalMotionPayloadDetachEntityBuffer( &owner, 1, &entityC, 1 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& groupDestroys == gd );
	CHECK( R_TemporalMotionPayloadDetachEntityBuffer( &owner, 1, &entityC, 2 ) );
	CHECK( owner.frames[1].buffer == beforeOwner.frames[1].buffer
		&& owner.frames[1].bindGroup == NULL
		&& owner.frames[1].entityBuffer == NULL
		&& owner.frames[1].entityAllocationGeneration == 0
		&& groupDestroys == gd + 1 );
	bc = bufferCalls; gc = groupCalls;
	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 1, 3, &entityC, 3 ) );
	CHECK( owner.frames[1].buffer == beforeOwner.frames[1].buffer
		&& owner.frames[1].entityAllocationGeneration == 3
		&& bufferCalls == bc && groupCalls == gc + 1 );
	owner.frames[0].allocationGeneration = UINT32_MAX;
	beforeOwner = owner; bc = bufferCalls; gc = groupCalls;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 2, 0, 16, &entityB, 2 ) );
	CHECK( memcmp( &owner, &beforeOwner, sizeof( owner ) ) == 0
		&& bufferCalls == bc && groupCalls == gc );

	// Fresh-owner failure matrix: no partial publication and safe cleanup.
	{
		temporalMotionPayloadOwner_t fresh, zero;
		R_TemporalMotionPayloadInit( &fresh ); zero = fresh;
		failLayoutAt = layoutCalls + 1;
		CHECK( !R_TemporalMotionPayloadEnsure( &fresh, &backendA, 1, 0, 2, &entityA, 1 ) );
		CHECK( memcmp( &fresh, &zero, sizeof( fresh ) ) == 0 ); failLayoutAt = 0;
		failBufferAt = bufferCalls + 1;
		CHECK( !R_TemporalMotionPayloadEnsure( &fresh, &backendA, 1, 0, 2, &entityA, 1 ) );
		CHECK( memcmp( &fresh, &zero, sizeof( fresh ) ) == 0 ); failBufferAt = 0;
		failMapAt = mapCalls + 1;
		CHECK( !R_TemporalMotionPayloadEnsure( &fresh, &backendA, 1, 0, 2, &entityA, 1 ) );
		CHECK( memcmp( &fresh, &zero, sizeof( fresh ) ) == 0 ); failMapAt = 0;
		failGroupAt = groupCalls + 1;
		CHECK( !R_TemporalMotionPayloadEnsure( &fresh, &backendA, 1, 0, 2, &entityA, 1 ) );
		CHECK( memcmp( &fresh, &zero, sizeof( fresh ) ) == 0 ); failGroupAt = 0;
		aliasBufferAt = &entityA;
		CHECK( !R_TemporalMotionPayloadEnsure( &fresh, &backendA, 1, 0, 2, &entityA, 1 ) );
		CHECK( memcmp( &fresh, &zero, sizeof( fresh ) ) == 0 );
	}

	baseEvent = destroyEventCount;
	R_TemporalMotionPayloadRelease( &owner );
	CHECK( !owner.ready && owner.layout == NULL && owner.backend == NULL
		&& owner.layoutAllocationGeneration == 1 );
	// Both groups are released before either buffer, then the shared layout.
	CHECK( destroyEvents[baseEvent] == 'G' && destroyEvents[baseEvent + 1] == 'G' );
	CHECK( destroyEvents[destroyEventCount - 1] == 'L' );
	lc = layoutDestroys; bc = bufferDestroys; gc = groupDestroys;
	R_TemporalMotionPayloadRelease( &owner );
	CHECK( layoutDestroys == lc && bufferDestroys == bc && groupDestroys == gc );
	CHECK( R_TemporalMotionPayloadGetBindGroup( &owner, 0 ) == NULL );
	publishedLayout = (const ralBindGroupLayout_t *)0x1;
	layoutGeneration = 0xfeedbeefu;
	CHECK( !R_TemporalMotionPayloadGetLayout( &owner, &publishedLayout,
		&layoutGeneration ) );
	CHECK( publishedLayout == (const ralBindGroupLayout_t *)0x1
		&& layoutGeneration == 0xfeedbeefu );
	CHECK( R_TemporalMotionPayloadEnsure( &owner, &backendA, 1, 0, 2,
		&entityA, 4 ) );
	CHECK( R_TemporalMotionPayloadGetLayout( &owner, &publishedLayout,
		&layoutGeneration ) && layoutGeneration == 2 );
	R_TemporalMotionPayloadRelease( &owner );
	CHECK( owner.layoutAllocationGeneration == 2 );
	owner.layoutAllocationGeneration = UINT32_MAX;
	lc = layoutCalls;
	CHECK( !R_TemporalMotionPayloadEnsure( &owner, &backendA, 1, 0, 2,
		&entityA, 5 ) && layoutCalls == lc );

	puts( "PASS active-only temporal motion payload contract" );
	return 0;
}
