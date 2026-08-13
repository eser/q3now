// SPDX-License-Identifier: GPL-3.0-or-later

#include "ral_residency.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expr) do { \
	if ( !( expr ) ) { \
		fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		failures++; \
	} \
} while ( 0 )

static ralResidencyCandidate_t candidate( ralResidencyClass_t classId,
		uint32_t resource, ralResidencyTier_t tier, uint32_t error,
		uint32_t age, uint64_t cost ) {
	ralResidencyCandidate_t c;
	memset( &c, 0, sizeof( c ) );
	c.id.classId = (uint8_t)classId;
	c.id.resource = resource;
	c.id.planeMask = 7; // coherent base/normal/ORM group
	c.state = RAL_RESIDENCY_REQUESTED;
	c.tier = tier;
	c.screenErrorQ8 = error;
	c.ageFrames = age;
	c.costBytes = cost;
	c.fallbackReady = 1;
	return c;
}

static void testLifecycle( void ) {
	CHECK( Ral_ResidencyCanTransition( RAL_RESIDENCY_ABSENT, RAL_RESIDENCY_REQUESTED ) );
	CHECK( Ral_ResidencyCanTransition( RAL_RESIDENCY_REQUESTED, RAL_RESIDENCY_IN_FLIGHT ) );
	CHECK( Ral_ResidencyCanTransition( RAL_RESIDENCY_IN_FLIGHT, RAL_RESIDENCY_RESIDENT ) );
	CHECK( Ral_ResidencyCanTransition( RAL_RESIDENCY_RESIDENT, RAL_RESIDENCY_STALE ) );
	CHECK( Ral_ResidencyCanTransition( RAL_RESIDENCY_STALE, RAL_RESIDENCY_EVICTING ) );
	CHECK( Ral_ResidencyCanTransition( RAL_RESIDENCY_EVICTING, RAL_RESIDENCY_ABSENT ) );
	CHECK( !Ral_ResidencyCanTransition( RAL_RESIDENCY_ABSENT, RAL_RESIDENCY_RESIDENT ) );
	CHECK( !Ral_ResidencyCanTransition( RAL_RESIDENCY_IN_FLIGHT, RAL_RESIDENCY_EVICTING ) );
	CHECK( !Ral_ResidencyCanTransition( RAL_RESIDENCY_EVICTING, RAL_RESIDENCY_REQUESTED ) );
}

static void testScoreAndRank( void ) {
	ralResidencyCandidate_t safety = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 0,
		RAL_RESIDENCY_TIER_SAFETY, 1, 0, 1 );
	ralResidencyCandidate_t oldBackground = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 1,
		RAL_RESIDENCY_TIER_BACKGROUND, 100000, 99999, 1 );
	ralResidencyCandidate_t young = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 2,
		RAL_RESIDENCY_TIER_VISIBLE, 256, 0, 64 * 1024 );
	ralResidencyCandidate_t old = young;
	ralResidencyScore_t score;
	old.id.resource = 3;
	old.ageFrames = 99999;

	CHECK( Ral_ResidencyCompareRequest( &safety, &oldBackground, NULL ) < 0 );
	CHECK( Ral_ResidencyCompareRequest( &old, &young, NULL ) < 0 );
	score = Ral_ResidencyScore( &old, NULL );
	CHECK( score.screenAge == 1280 ); // age capped at 120: 256*(30+120)/30
	CHECK( score.cost == 1 );
	CHECK( score.total == 1279 );

	young.waitFrames = ralResidencyDefaultPolicy.starvationFrames;
	old.waitFrames = 0;
	CHECK( Ral_ResidencyCompareRequest( &young, &old, NULL ) < 0 );
}

static void testEviction( void ) {
	ralResidencyCandidate_t visible = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 0,
		RAL_RESIDENCY_TIER_VISIBLE, 256, 1, 4096 );
	ralResidencyCandidate_t old = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 1,
		RAL_RESIDENCY_TIER_BACKGROUND, 0, 100, 4096 );
	ralResidencyCandidate_t newer = old;

	visible.state = RAL_RESIDENCY_RESIDENT;
	old.state = RAL_RESIDENCY_RESIDENT;
	newer.state = RAL_RESIDENCY_RESIDENT;
	newer.id.resource = 2;
	newer.ageFrames = 10;

	CHECK( !Ral_ResidencyEvictionEligible( &visible ) );
	CHECK( Ral_ResidencyEvictionEligible( &old ) );
	CHECK( Ral_ResidencyCompareEviction( &old, &newer, NULL ) < 0 );
	old.pinned = 1;
	CHECK( !Ral_ResidencyEvictionEligible( &old ) );
	old.pinned = 0;
	old.fallbackReady = 0;
	CHECK( !Ral_ResidencyEvictionEligible( &old ) );
}

static void testAtomicPromotionAndFallback( void ) {
	ralResidencyCandidate_t page = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 7,
		RAL_RESIDENCY_TIER_VISIBLE, 256, 0, 4096 );
	page.state = RAL_RESIDENCY_IN_FLIGHT;
	page.id.level = 2;
	page.id.planeMask = 7; // base + normal + ORM must publish together

	CHECK( !Ral_ResidencyPromotionReady( &page, 3, 1 ) );
	CHECK( !Ral_ResidencyPromotionReady( &page, 7, 0 ) );
	CHECK( Ral_ResidencyPromotionReady( &page, 7, 1 ) );
	page.id.level = 0;
	CHECK( Ral_ResidencyPromotionReady( &page, 7, 0 ) );
	page.state = RAL_RESIDENCY_REQUESTED;
	CHECK( !Ral_ResidencyPromotionReady( &page, 7, 1 ) );
}

static void testPersistentPageRecord( void ) {
	ralResidencyPageId_t id;
	ralResidencyPageRecord_t record;
	ralResidencyPageRecord_t before;

	memset( &id, 0, sizeof( id ) );
	id.classId = RAL_RESIDENCY_CLASS_TEXTURE;
	id.resource = 23;
	id.level = 2;
	id.x = 4;
	id.y = 5;
	id.planeMask = 7;
	CHECK( Ral_ResidencyPageRecordInit( &record, &id,
		RAL_RESIDENCY_RESIDENT, 10 ) );
	CHECK( Ral_ResidencyPageIdCompare( &record.id, &id ) == 0 );
	CHECK( record.completedPlaneMask == 7 );
	CHECK( Ral_ResidencyPageRecordTransition( &record,
		RAL_RESIDENCY_STALE, 0, 1, 11 ) );
	CHECK( Ral_ResidencyPageRecordTransition( &record,
		RAL_RESIDENCY_IN_FLIGHT, 0, 1, 12 ) );
	before = record;
	CHECK( !Ral_ResidencyPageRecordTransition( &record,
		RAL_RESIDENCY_RESIDENT, 3, 1, 13 ) );
	CHECK( memcmp( &record, &before, sizeof( record ) ) == 0 );
	CHECK( !Ral_ResidencyPageRecordTransition( &record,
		RAL_RESIDENCY_RESIDENT, 7, 0, 13 ) );
	CHECK( memcmp( &record, &before, sizeof( record ) ) == 0 );
	CHECK( !Ral_ResidencyPageRecordTransition( &record,
		RAL_RESIDENCY_RESIDENT, 7, 1, 9 ) );
	CHECK( memcmp( &record, &before, sizeof( record ) ) == 0 );
	CHECK( Ral_ResidencyPageRecordTransition( &record,
		RAL_RESIDENCY_RESIDENT, 7, 1, 13 ) );
	CHECK( record.state == RAL_RESIDENCY_RESIDENT );
	CHECK( record.transitionSerial == 13 );

	id.planeMask = 0;
	CHECK( !Ral_ResidencyPageRecordInit( &record, &id,
		RAL_RESIDENCY_ABSENT, 0 ) );
}

static void testBoundedFairSelection( void ) {
	ralResidencyCandidate_t c[6];
	ralResidencyBudget_t budget;
	size_t out[6];
	uint8_t scratch[6];
	size_t n;

	c[0] = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 0, RAL_RESIDENCY_TIER_VISIBLE, 900, 20, 64 );
	c[1] = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 1, RAL_RESIDENCY_TIER_VISIBLE, 800, 20, 64 );
	c[2] = candidate( RAL_RESIDENCY_CLASS_SHADOW, 0, RAL_RESIDENCY_TIER_PREDICTED, 50, 20, 64 );
	c[3] = candidate( RAL_RESIDENCY_CLASS_IRRADIANCE, 0, RAL_RESIDENCY_TIER_BACKGROUND, 10, 20, 64 );
	c[4] = candidate( RAL_RESIDENCY_CLASS_ASSET_CHUNK, 0, RAL_RESIDENCY_TIER_BACKGROUND, 1, 20, 1024 );
	c[5] = candidate( RAL_RESIDENCY_CLASS_TEXTURE, 2, RAL_RESIDENCY_TIER_VISIBLE, 700, 20, 64 );

	memset( &budget, 0, sizeof( budget ) );
	budget.maxPages = 4;
	budget.maxBytes = 256;
	budget.minPerClass[RAL_RESIDENCY_CLASS_TEXTURE] = 1;
	budget.minPerClass[RAL_RESIDENCY_CLASS_SHADOW] = 1;
	budget.minPerClass[RAL_RESIDENCY_CLASS_IRRADIANCE] = 1;
	budget.minPerClass[RAL_RESIDENCY_CLASS_ASSET_CHUNK] = 1;

	n = Ral_ResidencySelectRequests( c, 6, NULL, &budget, out, 6, scratch );
	CHECK( n == 4 );
	CHECK( out[0] == 0 );
	CHECK( out[1] == 2 );
	CHECK( out[2] == 3 );
	CHECK( out[3] == 1 ); // asset chunk cannot fit; global texture request fills

	// Replay stability: same trace yields the exact same selection.
	{
		size_t replay[6];
		uint8_t replayScratch[6];
		size_t replayN = Ral_ResidencySelectRequests( c, 6, NULL, &budget,
			replay, 6, replayScratch );
		CHECK( replayN == n );
		CHECK( memcmp( replay, out, n * sizeof( out[0] ) ) == 0 );
	}
}

int main( void ) {
	testLifecycle();
	testScoreAndRank();
	testEviction();
	testAtomicPromotionAndFallback();
	testPersistentPageRecord();
	testBoundedFairSelection();
	if ( failures ) return 1;
	puts( "ral residency policy contract: PASS" );
	return 0;
}
