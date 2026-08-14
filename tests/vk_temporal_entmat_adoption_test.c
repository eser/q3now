// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_entmat_adoption.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL temporal entMat adoption line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; void *native; size_t size; };

typedef struct {
	struct ralBuffer_s wrappers[32];
	const ralBuffer_t *bound[VK_TEMPORAL_ENTMAT_MAX_SLOTS];
	const ralBuffer_t *lastDetached[VK_TEMPORAL_ENTMAT_MAX_SLOTS];
	uint32_t boundGeneration[VK_TEMPORAL_ENTMAT_MAX_SLOTS];
	uint32_t lastDetachedGeneration[VK_TEMPORAL_ENTMAT_MAX_SLOTS];
	int adoptCalls, destroyCalls, rebindCalls, beginCalls, detachCalls;
	int failAdoptAt, failRebindAt, failBeginAt, failDetachAt;
	const ralBuffer_t *aliasAdopt;
	int destroyOrder[32];
} fakeContext_t;

static ralBuffer_t *FakeAdopt( void *userData, ralBackend_t *backend,
		void *nativeBuffer, size_t size, const char *debugName ) {
	fakeContext_t *ctx = (fakeContext_t *)userData;
	int call = ++ctx->adoptCalls;
	(void)backend;
	if ( !debugName || strcmp( debugName, "wired-temporal-entmat-adopted" ) != 0 )
		return NULL;
	if ( call == ctx->failAdoptAt ) return NULL;
	if ( ctx->aliasAdopt ) {
		const ralBuffer_t *alias = ctx->aliasAdopt;
		ctx->aliasAdopt = NULL;
		return (ralBuffer_t *)alias;
	}
	ctx->wrappers[call].id = call;
	ctx->wrappers[call].native = nativeBuffer;
	ctx->wrappers[call].size = size;
	return &ctx->wrappers[call];
}

static void FakeDestroy( void *userData, ralBuffer_t *adopted ) {
	fakeContext_t *ctx = (fakeContext_t *)userData;
	struct ralBuffer_s *wrapper = (struct ralBuffer_s *)adopted;
	ctx->destroyOrder[ctx->destroyCalls++] = wrapper->id;
}

static qboolean FakeRebind( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration ) {
	fakeContext_t *ctx = (fakeContext_t *)userData;
	int call = ++ctx->rebindCalls;
	if ( call == ctx->failRebindAt ) return qfalse;
	ctx->bound[frameIndex] = adopted;
	ctx->boundGeneration[frameIndex] = allocationGeneration;
	return qtrue;
}

static qboolean FakeBegin( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration ) {
	fakeContext_t *ctx = (fakeContext_t *)userData;
	int call = ++ctx->beginCalls;
	if ( call == ctx->failBeginAt ) return qfalse;
	return ctx->bound[frameIndex] == adopted
		&& ctx->boundGeneration[frameIndex] == allocationGeneration;
}

static qboolean FakeDetach( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration ) {
	fakeContext_t *ctx = (fakeContext_t *)userData;
	int call = ++ctx->detachCalls;
	if ( call == ctx->failDetachAt ) return qfalse;
	if ( ctx->bound[frameIndex] == adopted
			&& ctx->boundGeneration[frameIndex] == allocationGeneration ) {
		ctx->bound[frameIndex] = NULL;
		ctx->boundGeneration[frameIndex] = 0;
		ctx->lastDetached[frameIndex] = adopted;
		ctx->lastDetachedGeneration[frameIndex] = allocationGeneration;
		return qtrue;
	}
	return ctx->bound[frameIndex] == NULL
		&& ctx->lastDetached[frameIndex] == adopted
		&& ctx->lastDetachedGeneration[frameIndex] == allocationGeneration;
}

int main( void ) {
	struct ralBackend_s backendA = { 1 }, backendB = { 2 };
	vkTemporalEntMatAdoptionOwner_t owner, before, other;
	fakeContext_t ctx;
	vkTemporalEntMatAdoptionOps_t ops;
	unsigned char nativeA, nativeB;
	const ralBuffer_t *wrapper0, *wrapper1;
	uint32_t generation;
	int adopts, destroys, rebinds, begins, detaches;

	memset( &ctx, 0, sizeof( ctx ) );
	memset( &ops, 0, sizeof( ops ) );
	ops.adopt = FakeAdopt;
	ops.destroy = FakeDestroy;
	ops.consumerRebind = FakeRebind;
	ops.consumerBeginFrame = FakeBegin;
	ops.consumerDetach = FakeDetach;
	ops.userData = &ctx;
	VK_TemporalEntMatAdoptionInit( &owner );
	CHECK( !owner.configured && owner.backend == NULL );
	CHECK( ctx.adoptCalls == 0 && ctx.destroyCalls == 0 );

	VK_TemporalEntMatAdoptionInit( &other );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( NULL, 2, 0 ) );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( &other, 0, 0 ) );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( &other,
		VK_TEMPORAL_ENTMAT_MAX_SLOTS + 1u, 0 ) );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( &other, 2, 2 ) );
	CHECK( !other.configured );

	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 1, &ops ) );
	CHECK( ctx.adoptCalls == 0 );
	CHECK( VK_TemporalEntMatAdoptionResetAfterFence( &owner, 2, 0 ) );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( &owner, 3, 0 ) );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( &owner, 2, 2 ) );
	before = owner;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, NULL, 0,
		&nativeA, 4096, 1, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		NULL, 4096, 1, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 0, 1, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 0, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, UINT32_MAX, &ops ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );

	CHECK( VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 1, &ops ) );
	CHECK( ctx.adoptCalls == 1 && ctx.rebindCalls == 1 && ctx.destroyCalls == 0 );
	generation = 77;
	wrapper0 = VK_TemporalEntMatAdoptionGet( &owner, 0, &generation );
	CHECK( wrapper0 == &ctx.wrappers[1] && generation == 1 );
	generation = 77;
	CHECK( VK_TemporalEntMatAdoptionGet( &owner, 1, &generation ) == NULL
		&& generation == 77 );

	adopts = ctx.adoptCalls; destroys = ctx.destroyCalls; rebinds = ctx.rebindCalls;
	CHECK( VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 1, &ops ) );
	CHECK( ctx.adoptCalls == adopts && ctx.destroyCalls == destroys
		&& ctx.rebindCalls == rebinds + 1 );
	before = owner;
	adopts = ctx.adoptCalls; destroys = ctx.destroyCalls; rebinds = ctx.rebindCalls;
	ctx.failRebindAt = ctx.rebindCalls + 1;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 1, &ops ) );
	CHECK( ctx.adoptCalls == adopts && ctx.destroyCalls == destroys
		&& ctx.rebindCalls == rebinds + 1 );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0
		&& owner.slots[0].resetAfterFence );
	ctx.failRebindAt = 0;
	CHECK( VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 1, &ops ) );
	before = owner;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeB, 4096, 1, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 8192, 1, &ops ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );

	ctx.failBeginAt = ctx.beginCalls + 1;
	CHECK( !VK_TemporalEntMatAdoptionBeginFrame( &owner, 0, &ops ) );
	CHECK( owner.slots[0].resetAfterFence && !owner.slots[0].begun );
	ctx.failBeginAt = 0;
	CHECK( VK_TemporalEntMatAdoptionBeginFrame( &owner, 0, &ops ) );
	CHECK( owner.slots[0].begun && !owner.slots[0].resetAfterFence );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 2, &ops ) );
	CHECK( VK_TemporalEntMatAdoptionResetAfterFence( &owner, 2, 0 ) );

	before = owner;
	ctx.failAdoptAt = ctx.adoptCalls + 1;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 2, &ops ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	ctx.failAdoptAt = 0;
	ctx.failRebindAt = ctx.rebindCalls + 1;
	destroys = ctx.destroyCalls;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 2, &ops ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	CHECK( ctx.destroyCalls == destroys + 1 );
	ctx.failRebindAt = 0;

	destroys = ctx.destroyCalls;
	CHECK( VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 2, &ops ) );
	CHECK( owner.slots[0].allocationGeneration == 2
		&& owner.slots[0].nativeBuffer == &nativeA );
	CHECK( ctx.destroyCalls == destroys + 1 );
	wrapper0 = owner.slots[0].adopted;
	before = owner;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 1, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, UINT32_MAX, &ops ) );
	CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendB, 0,
		&nativeA, 4096, 3, &ops ) );

	CHECK( VK_TemporalEntMatAdoptionResetAfterFence( &owner, 2, 1 ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 1,
		&nativeA, 4096, 1, &ops ) );
	ctx.aliasAdopt = (const ralBuffer_t *)&nativeB;
	destroys = ctx.destroyCalls;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 1,
		&nativeB, 4096, 1, &ops ) );
	CHECK( ctx.destroyCalls == destroys && !owner.slots[1].ready );
	ctx.aliasAdopt = wrapper0;
	destroys = ctx.destroyCalls;
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 1,
		&nativeB, 4096, 1, &ops ) );
	CHECK( ctx.destroyCalls == destroys );
	CHECK( owner.slots[0].adopted == wrapper0 && !owner.slots[1].ready );
	CHECK( VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 1,
		&nativeB, 4096, 1, &ops ) );
	wrapper1 = owner.slots[1].adopted;
	CHECK( wrapper1 != wrapper0 );

	// A consumer-begin failure is output-atomic for the owner grant.
	ctx.failBeginAt = ctx.beginCalls + 1;
	before = owner; begins = ctx.beginCalls;
	CHECK( !VK_TemporalEntMatAdoptionBeginFrame( &owner, 1, &ops ) );
	CHECK( ctx.beginCalls == begins + 1 && memcmp( &owner, &before, sizeof( owner ) ) == 0 );
	ctx.failBeginAt = 0;
	CHECK( VK_TemporalEntMatAdoptionBeginFrame( &owner, 1, &ops ) );

	CHECK( !VK_TemporalEntMatAdoptionReleaseAfterIdle( &owner, qfalse, &ops ) );
	CHECK( owner.slots[0].ready && owner.slots[1].ready );
	ctx.failDetachAt = ctx.detachCalls + 2;
	destroys = ctx.destroyCalls; detaches = ctx.detachCalls;
	CHECK( !VK_TemporalEntMatAdoptionReleaseAfterIdle( &owner, qtrue, &ops ) );
	CHECK( ctx.detachCalls == detaches + 2 && ctx.destroyCalls == destroys );
	CHECK( owner.slots[0].ready && owner.slots[1].ready );
	CHECK( owner.releasing && owner.slots[0].consumerDetached
		&& !owner.slots[1].consumerDetached );
	CHECK( ctx.bound[0] == NULL && ctx.lastDetached[0] == wrapper0 );
	CHECK( ctx.bound[1] == wrapper1 && ctx.lastDetached[1] == NULL );
	generation = 91;
	CHECK( VK_TemporalEntMatAdoptionGet( &owner, 0, &generation ) == NULL
		&& generation == 91 );
	CHECK( VK_TemporalEntMatAdoptionGet( &owner, 1, &generation ) == NULL
		&& generation == 91 );
	CHECK( !VK_TemporalEntMatAdoptionResetAfterFence( &owner, 2, 0 ) );
	CHECK( !VK_TemporalEntMatAdoptionEnsure( &owner, &backendA, 0,
		&nativeA, 4096, 3, &ops ) );
	CHECK( !VK_TemporalEntMatAdoptionBeginFrame( &owner, 0, &ops ) );
	ctx.failDetachAt = 0;
	detaches = ctx.detachCalls;
	CHECK( VK_TemporalEntMatAdoptionReleaseAfterIdle( &owner, qtrue, &ops ) );
	CHECK( ctx.detachCalls == detaches + 1 );
	CHECK( ctx.destroyCalls == destroys + 2 );
	CHECK( ctx.destroyOrder[destroys] == ((const struct ralBuffer_s *)wrapper1)->id );
	CHECK( ctx.destroyOrder[destroys + 1] == ((const struct ralBuffer_s *)wrapper0)->id );
	CHECK( !owner.configured && owner.backend == NULL );
	destroys = ctx.destroyCalls;
	CHECK( VK_TemporalEntMatAdoptionReleaseAfterIdle( &owner, qtrue, &ops ) );
	CHECK( ctx.destroyCalls == destroys );

	// A fresh owner can use the full bounded slot count without allocation.
	VK_TemporalEntMatAdoptionInit( &other );
	adopts = ctx.adoptCalls;
	CHECK( VK_TemporalEntMatAdoptionResetAfterFence( &other,
		VK_TEMPORAL_ENTMAT_MAX_SLOTS, VK_TEMPORAL_ENTMAT_MAX_SLOTS - 1u ) );
	CHECK( other.frameCount == VK_TEMPORAL_ENTMAT_MAX_SLOTS
		&& ctx.adoptCalls == adopts );

	puts( "temporal entMat adoption owner contract: ok" );
	return 0;
}
