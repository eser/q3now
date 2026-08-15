// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_motion_sequence.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

static vkTemporalGenericPipelineReceipt_t Receipt( void ) {
	vkTemporalGenericPipelineReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.tableGeneration = 1; r.slot = 2; r.entryGeneration = 3;
	r.recipeOwnerEpoch = 4; r.recipeEntryGeneration = 5;
	r.factoryAllocationGeneration = 6; r.catalogId = 7;
	return r;
}

static vkTemporalMotionDrawSequence_t One( uint32_t absoluteSlot,
		uint32_t pipelineSlot, temporalMotionOutcome_t outcome,
		const vkTemporalGenericPipelineReceipt_t *receipt ) {
	vkTemporalMotionDrawSequence_t s;
	memset( &s, 0, sizeof( s ) );
	(void)VK_TemporalMotionDrawSequenceAppend( &s, absoluteSlot,
		pipelineSlot, outcome, receipt );
	return s;
}

int main( void ) {
	vkTemporalGenericPipelineReceipt_t baseReceipt = Receipt(), changed;
	vkTemporalMotionDrawSequence_t base, candidate, before, orderedA, orderedB;
	base = One( 9, 2, TEMPORAL_MOTION_WRITE_VALID, &baseReceipt );
	CHECK( base.count == 1 );
#define DIFFERENT(expr) do { candidate = (expr); \
	CHECK( !VK_TemporalMotionDrawSequenceEqual( &base, &candidate ) ); \
} while ( 0 )
	DIFFERENT( One( 10, 2, TEMPORAL_MOTION_WRITE_VALID, &baseReceipt ) );
	DIFFERENT( One( 9, 3, TEMPORAL_MOTION_PRESERVE, NULL ) );
	changed = baseReceipt; changed.slot = 8;
	DIFFERENT( One( 9, 8, TEMPORAL_MOTION_WRITE_VALID, &changed ) );
	DIFFERENT( One( 9, 2, TEMPORAL_MOTION_INVALIDATE_OPAQUE, &baseReceipt ) );
#define RECEIPT_FIELD(field) do { changed = baseReceipt; changed.field++; \
	DIFFERENT( One( 9, 2, TEMPORAL_MOTION_WRITE_VALID, &changed ) ); \
} while ( 0 )
	RECEIPT_FIELD( tableGeneration );
	RECEIPT_FIELD( entryGeneration );
	RECEIPT_FIELD( recipeOwnerEpoch );
	RECEIPT_FIELD( recipeEntryGeneration );
	RECEIPT_FIELD( factoryAllocationGeneration );
	RECEIPT_FIELD( catalogId );
#undef RECEIPT_FIELD
	memset( &orderedA, 0, sizeof( orderedA ) );
	memset( &orderedB, 0, sizeof( orderedB ) );
	CHECK( VK_TemporalMotionDrawSequenceAppend( &orderedA, 9, 2,
		TEMPORAL_MOTION_WRITE_VALID, &baseReceipt ) );
	CHECK( VK_TemporalMotionDrawSequenceAppend( &orderedA, 10, 2,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, &baseReceipt ) );
	CHECK( VK_TemporalMotionDrawSequenceAppend( &orderedB, 10, 2,
		TEMPORAL_MOTION_INVALIDATE_OPAQUE, &baseReceipt ) );
	CHECK( VK_TemporalMotionDrawSequenceAppend( &orderedB, 9, 2,
		TEMPORAL_MOTION_WRITE_VALID, &baseReceipt ) );
	CHECK( !VK_TemporalMotionDrawSequenceEqual( &orderedA, &orderedB ) );
	CHECK( orderedA.count == 2 && orderedB.count == 2 );

	before = base; candidate = before; changed = baseReceipt;
	changed.entryGeneration = 0;
	CHECK( !VK_TemporalMotionDrawSequenceAppend( &candidate, 9, 2,
		TEMPORAL_MOTION_WRITE_VALID, &changed ) );
	CHECK( memcmp( &candidate, &before, sizeof( candidate ) ) == 0 );
	CHECK( !VK_TemporalMotionDrawSequenceAppend( &candidate, 9, 2,
		TEMPORAL_MOTION_PRESERVE, &baseReceipt ) );
	CHECK( memcmp( &candidate, &before, sizeof( candidate ) ) == 0 );
	candidate = before; candidate.count = UINT32_MAX; before = candidate;
	CHECK( !VK_TemporalMotionDrawSequenceAppend( &candidate, 9, 2,
		TEMPORAL_MOTION_WRITE_VALID, &baseReceipt ) );
	CHECK( memcmp( &candidate, &before, sizeof( candidate ) ) == 0 );
#undef DIFFERENT
	puts( "vk_temporal_motion_sequence_test: PASS" );
	return 0;
}
