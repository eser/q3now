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

/*
Multi-frame fairness under sustained oversubscription.

testBoundedFairSelection proves one selection is bounded and ordered. It cannot
prove fairness: fairness is a property of the sequence, not of a single drain.
This drives repeated drains with a candidate set whose demand permanently
exceeds the per-frame budget, advancing waitFrames on everything not served,
which is what the production drain does between frames.

Asserted: (1) no frame ever exceeds maxPages or maxBytes; (2) every class is
served, so a class with permanently low screen error is not starved out by
high-error textures; (3) the starvation ceiling actually fires — the worst
observed wait stays bounded rather than growing without limit.
*/
static void testMultiFrameFairnessAndStarvationCeiling( void ) {
	enum { NUM_CANDIDATES = 8, NUM_FRAMES = 400 };
	ralResidencyCandidate_t c[NUM_CANDIDATES];
	ralResidencyBudget_t    budget;
	ralResidencyPolicy_t    policy = ralResidencyDefaultPolicy;
	size_t                  servedPerClass[RAL_RESIDENCY_CLASS_COUNT];
	uint32_t                worstWait = 0;
	size_t                  i, frame;

	// Demand outstrips supply every frame: four classes compete, textures carry
	// far higher screen error than the rest, so without a fairness rule the
	// other three classes would never be selected.
	c[0] = candidate( RAL_RESIDENCY_CLASS_TEXTURE,     0, RAL_RESIDENCY_TIER_VISIBLE,    900, 20, 64 );
	c[1] = candidate( RAL_RESIDENCY_CLASS_TEXTURE,     1, RAL_RESIDENCY_TIER_VISIBLE,    880, 20, 64 );
	c[2] = candidate( RAL_RESIDENCY_CLASS_TEXTURE,     2, RAL_RESIDENCY_TIER_VISIBLE,    860, 20, 64 );
	c[3] = candidate( RAL_RESIDENCY_CLASS_TEXTURE,     3, RAL_RESIDENCY_TIER_VISIBLE,    840, 20, 64 );
	c[4] = candidate( RAL_RESIDENCY_CLASS_SHADOW,      0, RAL_RESIDENCY_TIER_PREDICTED,   40, 20, 64 );
	c[5] = candidate( RAL_RESIDENCY_CLASS_SHADOW,      1, RAL_RESIDENCY_TIER_PREDICTED,   30, 20, 64 );
	c[6] = candidate( RAL_RESIDENCY_CLASS_IRRADIANCE,  0, RAL_RESIDENCY_TIER_BACKGROUND,  10, 20, 64 );
	c[7] = candidate( RAL_RESIDENCY_CLASS_ASSET_CHUNK, 0, RAL_RESIDENCY_TIER_BACKGROUND,   1, 20, 64 );

	// Fairness is carried by the per-class quota pass in
	// Ral_ResidencySelectRequests ("first honor bounded per-class fairness"),
	// not by the comparator: the comparator orders by tier before it looks at
	// the starvation ceiling, so a BACKGROUND page can never out-rank a VISIBLE
	// one on wait alone. Reserving one slot per class is what lets the low-error
	// classes progress at all.
	memset( &budget, 0, sizeof( budget ) );
	budget.maxPages = 4;   // eight candidates, four slots: permanent contention
	budget.maxBytes = 256; // exactly four 64-byte pages
	for ( i = 0; i < RAL_RESIDENCY_CLASS_COUNT; i++ ) {
		budget.minPerClass[i] = 1;
	}

	memset( servedPerClass, 0, sizeof( servedPerClass ) );

	for ( frame = 0; frame < NUM_FRAMES; frame++ ) {
		size_t   out[NUM_CANDIDATES];
		uint8_t  scratch[NUM_CANDIDATES];
		uint64_t frameBytes = 0;
		size_t   n = Ral_ResidencySelectRequests( c, NUM_CANDIDATES, &policy,
			&budget, out, NUM_CANDIDATES, scratch );

		// (1) bounded update: the drain never overruns either cap.
		CHECK( n <= budget.maxPages );
		for ( i = 0; i < n; i++ ) {
			frameBytes += c[out[i]].costBytes;
		}
		CHECK( frameBytes <= budget.maxBytes );

		// Advance the queue the way the production drain does: selected entries
		// are dispatched (wait resets), everything else waits one more frame.
		for ( i = 0; i < NUM_CANDIDATES; i++ ) {
			size_t k;
			int    selected = 0;
			for ( k = 0; k < n; k++ ) {
				if ( out[k] == i ) { selected = 1; break; }
			}
			if ( selected ) {
				servedPerClass[c[i].id.classId]++;
				c[i].waitFrames = 0;
			} else {
				c[i].waitFrames++;
				if ( c[i].waitFrames > worstWait ) {
					worstWait = c[i].waitFrames;
				}
			}
		}
	}

	// (2) no class is starved out, including the permanently lowest-error one.
	for ( i = 0; i < RAL_RESIDENCY_CLASS_COUNT; i++ ) {
		CHECK( servedPerClass[i] > 0 );
	}

	// (1b) page cap and byte cap must each bind on their own. The loop above
	// sizes maxBytes to exactly maxPages worth of pages, so the byte check
	// alone could satisfy it and a broken page cap would go unnoticed. Re-run
	// one drain with the byte budget deliberately slack so only maxPages can
	// stop the selection.
	{
		size_t  out[NUM_CANDIDATES];
		uint8_t scratch[NUM_CANDIDATES];
		size_t  n;
		ralResidencyBudget_t pageBound = budget;

		pageBound.maxPages = 3;
		pageBound.maxBytes = 1u << 20; // far above total candidate cost
		for ( i = 0; i < NUM_CANDIDATES; i++ ) {
			c[i].waitFrames = 0;
		}
		n = Ral_ResidencySelectRequests( c, NUM_CANDIDATES, &policy, &pageBound,
			out, NUM_CANDIDATES, scratch );
		CHECK( n == pageBound.maxPages );
	}

	// (1c) and the mirror case: byte cap binding while the page cap is slack.
	{
		size_t  out[NUM_CANDIDATES];
		uint8_t scratch[NUM_CANDIDATES];
		size_t  n, k;
		uint64_t bytes = 0;
		ralResidencyBudget_t byteBound = budget;

		byteBound.maxPages = NUM_CANDIDATES; // never the limiting factor
		byteBound.maxBytes = 128;            // two 64-byte pages
		for ( i = 0; i < NUM_CANDIDATES; i++ ) {
			c[i].waitFrames = 0;
		}
		n = Ral_ResidencySelectRequests( c, NUM_CANDIDATES, &policy, &byteBound,
			out, NUM_CANDIDATES, scratch );
		for ( k = 0; k < n; k++ ) {
			bytes += c[out[k]].costBytes;
		}
		CHECK( bytes <= byteBound.maxBytes );
		CHECK( n == 2 );
	}

	// (3) the ceiling bounds the worst case instead of letting it grow with the
	// run length. Allow one full ceiling plus a drain margin for the backlog.
	CHECK( policy.starvationFrames > 0 );
	CHECK( worstWait < policy.starvationFrames + NUM_CANDIDATES );
	CHECK( worstWait < NUM_FRAMES ); // guards against "never served" passing (2)
}

int main( void ) {
	testLifecycle();
	testScoreAndRank();
	testEviction();
	testAtomicPromotionAndFallback();
	testPersistentPageRecord();
	testBoundedFairSelection();
	testMultiFrameFairnessAndStarvationCeiling();
	if ( failures ) return 1;
	puts( "ral residency policy contract: PASS" );
	return 0;
}
