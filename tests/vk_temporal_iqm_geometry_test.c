// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_geometry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

static int s_adopts, s_destroys, s_failAt, s_aliasAt, s_wrongNativeAt;
static void *s_alias;

struct ralBuffer_s { void *native; size_t bytes; };

static ralBuffer_t *Adopt( ralBackend_t *backend, void *native,
		size_t bytes, const char *name ) {
	(void)backend; (void)native; (void)bytes; (void)name;
	s_adopts++;
	if ( s_adopts == s_failAt ) return NULL;
	if ( s_adopts == s_aliasAt ) return (ralBuffer_t *)s_alias;
	{
		ralBuffer_t *buffer = (ralBuffer_t *)malloc( sizeof( *buffer ) );
		if ( buffer ) {
			buffer->native = s_adopts == s_wrongNativeAt ? (void *)0xfeedu : native;
			buffer->bytes = bytes;
		}
		return buffer;
	}
}

static void Destroy( ralBuffer_t *buffer ) {
	if ( !buffer || buffer == (ralBuffer_t *)s_alias ) return;
	s_destroys++;
	free( buffer );
}

static qboolean Owned( ralBuffer_t *candidate, const void *context ) {
	return candidate != context ? qtrue : qfalse;
}

static qboolean Matches( const ralBuffer_t *candidate, const void *native,
		size_t bytes ) {
	return candidate && candidate->native == native && candidate->bytes == bytes
		? qtrue : qfalse;
}

static vkTemporalIqmGeometryKey_t Key( uintptr_t base ) {
	vkTemporalIqmGeometryKey_t key;
	memset( &key, 0, sizeof( key ) );
	key.backend = (ralBackend_t *)( base + 1u );
	key.nativeVertexBuffer = (void *)( base + 2u );
	key.nativeIndexBuffer = (void *)( base + 3u );
	key.vertexBytes = 680;
	key.indexBytes = 120;
	key.modelAllocationGeneration = 4;
	key.geometryGeneration = 5;
	key.contentDigest = 6;
	return key;
}

static void Reset( void ) {
	s_adopts = s_destroys = s_failAt = s_aliasAt = s_wrongNativeAt = 0;
	s_alias = NULL;
}

int main( void ) {
	vkTemporalIqmGeometryOwner_t owner, snapshot;
	vkTemporalIqmGeometryReceipt_t receipt;
	vkTemporalIqmGeometryKey_t key = Key( 0x1000u ), replacement = Key( 0x2000u );
	vkTemporalIqmGeometryOps_t ops = { Adopt, Destroy, Owned, Matches, NULL };
	VK_TemporalIqmGeometryInit( &owner );
	Reset();
	s_failAt = 1;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( !owner.receipt.ready && s_destroys == 0 );
	Reset(); s_failAt = 2;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( !owner.receipt.ready && s_destroys == 1 );
	Reset();
	CHECK( VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( VK_TemporalIqmGeometryGetReceipt( &owner, &receipt, &ops ) );
	CHECK( receipt.allocationGeneration == 1 && receipt.vertex != receipt.index );
	CHECK( !VK_TemporalIqmGeometryNeedsIdle( &owner, &key, &ops ) );
	CHECK( VK_TemporalIqmGeometryNeedsIdle( &owner, &replacement, &ops ) );
	snapshot = owner;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &replacement,
		qfalse, &ops ) );
	CHECK( memcmp( &owner, &snapshot, sizeof( owner ) ) == 0 );
	Reset(); s_failAt = 2;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &replacement,
		qtrue, &ops ) );
	CHECK( memcmp( &owner, &snapshot, sizeof( owner ) ) == 0 && s_destroys == 1 );
	Reset();
	CHECK( VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &replacement,
		qtrue, &ops ) );
	CHECK( owner.receipt.allocationGeneration == 2 && s_destroys == 2 );
	for ( int mutation = 0; mutation < 8; ++mutation ) {
		vkTemporalIqmGeometryKey_t changed = owner.receipt.key;
		uint32_t priorGeneration = owner.receipt.allocationGeneration;
		switch ( mutation ) {
		case 0: changed.vertexBytes++; break;
		case 1: changed.indexBytes++; break;
		case 2: changed.modelAllocationGeneration++; break;
		case 3: changed.geometryGeneration++; break;
		case 4: changed.contentDigest++; break;
		case 5: changed.nativeVertexBuffer = (void *)0xaaaau; break;
		case 6: changed.nativeIndexBuffer = (void *)0xbbbbu; break;
		default: changed.backend = (ralBackend_t *)0xccccu; break;
		}
		CHECK( VK_TemporalIqmGeometryNeedsIdle( &owner, &changed, &ops ) );
		snapshot = owner;
		CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &changed,
			qfalse, &ops ) );
		CHECK( memcmp( &owner, &snapshot, sizeof( owner ) ) == 0 );
		Reset();
		CHECK( VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &changed,
			qtrue, &ops ) );
		CHECK( owner.receipt.allocationGeneration == priorGeneration + 1u
			&& s_destroys == 2 );
	}
	{
		void *native = owner.receipt.vertex->native;
		size_t bytes = owner.receipt.vertex->bytes;
		owner.receipt.vertex->native = (void *)0xccccu;
		CHECK( !VK_TemporalIqmGeometryGetReceipt( &owner, &receipt, &ops ) );
		owner.receipt.vertex->native = native;
		owner.receipt.vertex->bytes = bytes + 1u;
		CHECK( !VK_TemporalIqmGeometryGetReceipt( &owner, &receipt, &ops ) );
		owner.receipt.vertex->bytes = bytes;
		native = owner.receipt.index->native;
		bytes = owner.receipt.index->bytes;
		owner.receipt.index->native = (void *)0xddddu;
		CHECK( !VK_TemporalIqmGeometryGetReceipt( &owner, &receipt, &ops ) );
		owner.receipt.index->native = native;
		owner.receipt.index->bytes = bytes + 1u;
		CHECK( !VK_TemporalIqmGeometryGetReceipt( &owner, &receipt, &ops ) );
		owner.receipt.index->bytes = bytes;
	}
	CHECK( !VK_TemporalIqmGeometryReleaseAfterIdle( &owner, qfalse, &ops ) );
	Reset();
	CHECK( VK_TemporalIqmGeometryReleaseAfterIdle( &owner, qtrue, &ops ) );
	CHECK( !owner.receipt.ready && s_destroys == 2 );

	// Borrowed/native aliases are rejected and never destroyed.
	VK_TemporalIqmGeometryInit( &owner ); Reset();
	s_alias = key.nativeVertexBuffer; s_aliasAt = 1;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( s_destroys == 0 );
	Reset(); s_wrongNativeAt = 1;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( s_destroys == 1 && !owner.receipt.ready );
	Reset(); s_alias = (void *)0x8888u; s_aliasAt = 1;
	ops.candidateContext = s_alias;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( s_destroys == 0 && !owner.receipt.ready );
	ops.candidateContext = NULL;
	{
		vkTemporalIqmGeometryKey_t invalid = key;
		invalid.geometryGeneration = UINT32_MAX;
		CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &invalid,
			qfalse, &ops ) );
		invalid = key;
		invalid.modelAllocationGeneration = UINT32_MAX;
		CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &invalid,
			qfalse, &ops ) );
	}
	Reset(); s_alias = (void *)0x7777u; s_aliasAt = 2;
	ops.candidateContext = s_alias;
	CHECK( !VK_TemporalIqmGeometryEnsureAfterIdle( &owner, &key, qfalse, &ops ) );
	CHECK( s_destroys == 1 );
	puts( "vk_temporal_iqm_geometry_test: PASS" );
	return 0;
}
