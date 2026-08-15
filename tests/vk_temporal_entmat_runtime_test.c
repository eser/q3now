// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_entmat_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL entmat runtime line %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; void *native; byte *bytes; uint64_t size; qboolean adopted; };
struct ralBindGroupLayout_s { int id; };
struct ralBindGroup_s { int id; };

static struct ralBuffer_s buffers[32], adopted[16];
static struct ralBindGroupLayout_s layouts[8];
static struct ralBindGroup_s groups[32];
static ralBackend_t *lastBackend;
static int creates, adopts, maps, groupsCreated, layoutsCreated;
static int bufferDestroys, groupDestroys, layoutDestroys, unmaps;
static int failCreateAt, failMapAt, failGroupAt, failAdoptAt;
static char events[128]; static int eventCount;

ralBuffer_t *Ral_AdoptBuffer( ralBackend_t *backend, void *nativeBuffer,
		size_t size, const char *debugName ) {
	int call = ++adopts; (void)debugName; lastBackend = backend;
	if ( call == failAdoptAt ) return NULL;
	adopted[call].id = 100 + call; adopted[call].native = nativeBuffer;
	adopted[call].size = size; adopted[call].adopted = qtrue;
	return &adopted[call];
}
void *Ral_GetBufferHandle( const ralBuffer_t *buffer ) {
	return buffer ? ((const struct ralBuffer_s *)buffer)->native : NULL;
}
ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *ci ) {
	int call = ++creates; lastBackend = backend;
	if ( call == failCreateAt ) return NULL;
	buffers[call].id = call; buffers[call].size = ci->size;
	buffers[call].bytes = (byte *)calloc( 1, (size_t)ci->size );
	return buffers[call].bytes ? &buffers[call] : NULL;
}
void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	struct ralBuffer_s *b = (struct ralBuffer_s *)buffer;
	bufferDestroys++; events[eventCount++] = b->adopted ? 'A' : 'B';
	if ( !b->adopted ) { free( b->bytes ); b->bytes = NULL; }
}
void *Ral_MapBuffer( ralBuffer_t *buffer ) {
	int call = ++maps;
	return call == failMapAt ? NULL : ((struct ralBuffer_s *)buffer)->bytes;
}
void Ral_UnmapBuffer( ralBuffer_t *buffer ) { (void)buffer; unmaps++; events[eventCount++] = 'U'; }
void Ral_FlushBuffer( ralBuffer_t *buffer, uint64_t offset, uint64_t size ) { (void)buffer; (void)offset; (void)size; }
ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *backend,
		const ralBindGroupLayoutCreateInfo_t *ci ) {
	int call = ++layoutsCreated; lastBackend = backend;
	if ( ci->numEntries != 2 ) abort(); layouts[call].id = call; return &layouts[call];
}
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout ) { (void)layout; layoutDestroys++; events[eventCount++] = 'L'; }
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *backend,
		const ralBindGroupCreateInfo_t *ci ) {
	int call = ++groupsCreated; lastBackend = backend;
	if ( call == failGroupAt ) return NULL;
	if ( ci->numValues != 2 || !ci->values[0].buffer || !ci->values[1].buffer ) abort();
	groups[call].id = call; return &groups[call];
}
void Ral_DestroyBindGroup( ralBindGroup_t *group ) { (void)group; groupDestroys++; events[eventCount++] = 'G'; }

int main( void ) {
	struct ralBackend_s backend = { 7 };
	vkTemporalEntMatRuntime_t runtime, before, ceilingRuntime;
	vkTemporalEntMatRuntimeFrameReceipt_t frameReceipt, receiptBefore;
	vkTemporalEntMatRuntimeFrameBinding_t frameBinding, frameBindingBefore;
	temporalMotionMatrices_t matrices;
	uint32_t writtenSlot;
	int native0, native1, native2;
	int a, c, g, b, u, gd, ld, base;

	VK_TemporalEntMatRuntimeInit( &runtime );
	before = runtime;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, NULL, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, UINT32_MAX, 32 ) );
	CHECK( memcmp( &runtime, &before, sizeof( runtime ) ) == 0 );
	VK_TemporalEntMatRuntimeInit( &ceilingRuntime );
	failCreateAt = creates + 1; c = creates;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &ceilingRuntime,
		&backend, 4, 0, &native0, 4096, 1,
		TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS ) );
	CHECK( creates == c + 1 && !VK_TemporalEntMatRuntimeHasLive( &ceilingRuntime ) );
	failCreateAt = 0; c = creates;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &ceilingRuntime,
		&backend, 4, 0, &native0, 4096, 1,
		TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS + 1u ) );
	CHECK( creates == c );
	failAdoptAt = adopts + 1; c = creates;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( creates == c && lastBackend == &backend
		&& !VK_TemporalEntMatRuntimeHasLive( &runtime ) ); failAdoptAt = 0;
	failCreateAt = creates + 1;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( !runtime.adoption.slots[0].ready ); failCreateAt = 0;
	failMapAt = maps + 1;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( !runtime.adoption.slots[0].ready ); failMapAt = 0;
	failGroupAt = groupsCreated + 1; u = unmaps;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( !runtime.adoption.slots[0].ready && unmaps == u + 1 ); failGroupAt = 0;

	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( lastBackend == &backend && runtime.payload.backend == &backend );
	CHECK( runtime.adoption.slots[0].ready
		&& runtime.adoption.slots[0].nativeBuffer == &native0
		&& runtime.payload.frames[0].entityBuffer
			== runtime.adoption.slots[0].adopted );
	CHECK( !runtime.payload.frames[0].begun );
	CHECK( VK_TemporalEntMatRuntimePeekFrameReceipt( &runtime, 0, &frameReceipt ) );
	CHECK( VK_TemporalEntMatRuntimeBeginFrame( &runtime, 0, &frameReceipt ) );
	CHECK( runtime.payload.frames[0].begun
		&& !runtime.payload.frames[0].resetAfterFence
		&& !runtime.adoption.slots[0].resetAfterFence );
	CHECK( VK_TemporalEntMatRuntimeGetFrameBinding(
		&runtime, &frameReceipt, &frameBinding ) );
	CHECK( frameBinding.compositeGroup == runtime.payload.frames[0].bindGroup
		&& frameBinding.receipt.payloadAllocationGeneration
			== frameReceipt.payloadAllocationGeneration );
	frameBindingBefore = frameBinding; frameReceipt.payloadLayoutGeneration++;
	CHECK( !VK_TemporalEntMatRuntimeGetFrameBinding(
		&runtime, &frameReceipt, &frameBinding )
		&& memcmp( &frameBinding, &frameBindingBefore,
			sizeof( frameBinding ) ) == 0 );
	frameReceipt.payloadLayoutGeneration--;
	CHECK( !VK_TemporalEntMatRuntimeBeginFrame( &runtime, 0, &frameReceipt ) );

	before = runtime; a = adopts; c = creates; g = groupsCreated;
	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 1, 32 ) );
	CHECK( adopts == a && creates == c && groupsCreated == g );
	CHECK( runtime.adoption.slots[0].resetAfterFence
		&& runtime.payload.frames[0].resetAfterFence );

	// Same native allocation at a new generation rebuilds identity; a changed
	// native pointer at the same generation rejects before adoption.
	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 2, 32 ) );
	a = adopts; before = runtime;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native2, 4096, 2, 32 ) );
	CHECK( adopts == a && runtime.adoption.slots[0].nativeBuffer == &native0 );
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native0, 4096, 3, TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS + 1u ) );

	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 1,
		&native1, 4096, 1, 32 ) );
	CHECK( runtime.adoption.slots[1].ready
		&& runtime.payload.frames[1].buffer != runtime.payload.frames[0].buffer );
	before = runtime; a = adopts; c = creates; g = groupsCreated;
	failGroupAt = groupsCreated + 1;
	CHECK( !VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native2, 4096, 3, 32 ) );
	CHECK( runtime.adoption.slots[0].nativeBuffer == &native0
		&& runtime.adoption.slots[0].allocationGeneration == 2
		&& runtime.payload.frames[0].entityAllocationGeneration == 2
		&& adopts == a + 1 && creates == c && groupsCreated == g + 1 );
	failGroupAt = 0;
	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 0,
		&native2, 4096, 3, 32 ) );
	CHECK( runtime.adoption.slots[0].nativeBuffer == &native2
		&& runtime.payload.frames[0].entityAllocationGeneration == 3 );
	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 2,
		(void *)( (char *)&native2 + 1 ), 4096, 1, 32 ) );
	CHECK( VK_TemporalEntMatRuntimeEnsureAfterFence( &runtime, &backend, 4, 3,
		(void *)( (char *)&native2 + 2 ), 4096, 1, 32 ) );
	CHECK( runtime.adoption.slots[0].ready && runtime.adoption.slots[1].ready
		&& runtime.adoption.slots[2].ready && runtime.adoption.slots[3].ready );

	// A2c3 generation-bound begin/append: preflight is non-consuming and
	// output-atomic, Begin consumes the fence grant, and absolute slots are exact.
	memset( &receiptBefore, 0x5a, sizeof( receiptBefore ) );
	frameReceipt = receiptBefore;
	CHECK( !VK_TemporalEntMatRuntimePeekFrameReceipt( &runtime, 4, &frameReceipt ) );
	CHECK( memcmp( &frameReceipt, &receiptBefore, sizeof( frameReceipt ) ) == 0 );
	CHECK( VK_TemporalEntMatRuntimePeekFrameReceipt( &runtime, 0, &frameReceipt ) );
	CHECK( frameReceipt.frameIndex == 0 && frameReceipt.capacity == 32
		&& frameReceipt.entityAllocationGeneration == 3
		&& frameReceipt.payloadAllocationGeneration
		&& frameReceipt.payloadLayoutGeneration );
	CHECK( VK_TemporalEntMatRuntimeBeginFrame( &runtime, 0, &frameReceipt ) );
	memset( &matrices, 0, sizeof( matrices ) );
	for ( int i = 0; i < 16; ++i )
		matrices.currentMvp[i] = matrices.previousMvp[i] = ( i % 5 == 0 ) ? 1.0f : 0.0f;
	writtenSlot = UINT32_MAX;
	CHECK( VK_TemporalEntMatRuntimeAppendAt( &runtime, &frameReceipt, 7,
		TEMPORAL_MOTION_WRITE_VALID, &matrices, &writtenSlot ) );
	CHECK( writtenSlot == 7 );
	{ vkTemporalEntMatRuntimeFrameReceipt_t stale = frameReceipt;
		stale.payloadAllocationGeneration++;
		writtenSlot = 123;
		CHECK( !VK_TemporalEntMatRuntimeAppendAt( &runtime, &stale, 8,
			TEMPORAL_MOTION_WRITE_VALID, &matrices, &writtenSlot ) );
		CHECK( writtenSlot == 123 ); }

	// Release requires explicit idle proof, resets every payload frame, detaches
	// groups before adopted wrappers, then releases temporal buffers/layout.
	before = runtime; CHECK( !VK_TemporalEntMatRuntimeReleaseAfterIdle( &runtime, qfalse ) );
	CHECK( memcmp( &runtime, &before, sizeof( runtime ) ) == 0 );
	base = eventCount; b = bufferDestroys; u = unmaps;
	gd = groupDestroys; ld = layoutDestroys;
	{
		ralBindGroup_t *saved = runtime.payload.frames[1].bindGroup;
		int groupsBeforeRetry;
		runtime.payload.frames[1].bindGroup = NULL;
		CHECK( !VK_TemporalEntMatRuntimeReleaseAfterIdle( &runtime, qtrue ) );
		CHECK( runtime.adoption.releasing
			&& runtime.adoption.slots[0].consumerDetached );
		groupsBeforeRetry = groupDestroys;
		runtime.payload.frames[1].bindGroup = saved;
		CHECK( VK_TemporalEntMatRuntimeReleaseAfterIdle( &runtime, qtrue ) );
		CHECK( groupDestroys >= groupsBeforeRetry );
	}
	CHECK( !VK_TemporalEntMatRuntimeHasLive( &runtime ) );
	CHECK( groupDestroys == gd + 4 && bufferDestroys == b + 8 && unmaps == u + 4
		&& layoutDestroys == ld + 1 );
	for ( int i = base; i < eventCount; ++i ) {
		if ( events[i] == 'A' ) {
			for ( int j = i + 1; j < eventCount; ++j ) CHECK( events[j] != 'G' );
		}
	}
	CHECK( VK_TemporalEntMatRuntimeReleaseAfterIdle( &runtime, qtrue ) );
	puts( "temporal entMat runtime contract: PASS" );
	return 0;
}
