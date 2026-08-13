// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_residency.h"

#include <limits.h>
#include <string.h>

const ralResidencyPolicy_t ralResidencyDefaultPolicy = {
	120,       // cap age contribution at two seconds at 60 Hz
	30,        // one urgency half-life at 60 Hz
	64,        // one sample contributes 0.25 in Q8 units
	64 * 1024, // subtract one Q8 unit per 64 KiB of producer cost
	240        // four-second fairness ceiling at 60 Hz
};

static uint64_t ralResidencyAddSaturated( uint64_t a, uint64_t b ) {
	return ( UINT64_MAX - a < b ) ? UINT64_MAX : a + b;
}

static int ralResidencyCompareU64Descending( uint64_t a, uint64_t b ) {
	return ( a > b ) ? -1 : ( a < b ) ? 1 : 0;
}

static int ralResidencyCompareU32Descending( uint32_t a, uint32_t b ) {
	return ( a > b ) ? -1 : ( a < b ) ? 1 : 0;
}

int Ral_ResidencyPageIdCompare( const ralResidencyPageId_t *a,
		const ralResidencyPageId_t *b ) {
	if ( !a || !b ) return a ? -1 : b ? 1 : 0;
	if ( a->classId != b->classId ) return a->classId < b->classId ? -1 : 1;
	if ( a->resource != b->resource ) return a->resource < b->resource ? -1 : 1;
	if ( a->level != b->level ) return a->level < b->level ? -1 : 1;
	if ( a->x != b->x ) return a->x < b->x ? -1 : 1;
	if ( a->y != b->y ) return a->y < b->y ? -1 : 1;
	if ( a->planeMask != b->planeMask ) return a->planeMask < b->planeMask ? -1 : 1;
	return 0;
}

int Ral_ResidencyCanTransition( ralResidencyState_t from,
		ralResidencyState_t to ) {
	if ( from < 0 || from >= RAL_RESIDENCY_STATE_COUNT ||
	     to < 0 || to >= RAL_RESIDENCY_STATE_COUNT ) return 0;
	if ( from == to ) return 1;

	switch ( from ) {
	case RAL_RESIDENCY_ABSENT:
		return to == RAL_RESIDENCY_REQUESTED;
	case RAL_RESIDENCY_REQUESTED:
		return to == RAL_RESIDENCY_IN_FLIGHT || to == RAL_RESIDENCY_ABSENT;
	case RAL_RESIDENCY_IN_FLIGHT:
		return to == RAL_RESIDENCY_RESIDENT ||
		       to == RAL_RESIDENCY_REQUESTED || to == RAL_RESIDENCY_ABSENT;
	case RAL_RESIDENCY_RESIDENT:
		return to == RAL_RESIDENCY_STALE || to == RAL_RESIDENCY_EVICTING;
	case RAL_RESIDENCY_STALE:
		return to == RAL_RESIDENCY_REQUESTED ||
		       to == RAL_RESIDENCY_IN_FLIGHT ||
		       to == RAL_RESIDENCY_RESIDENT ||
		       to == RAL_RESIDENCY_EVICTING;
	case RAL_RESIDENCY_EVICTING:
		return to == RAL_RESIDENCY_ABSENT;
	default:
		return 0;
	}
}

int Ral_ResidencyRequestEligible( const ralResidencyCandidate_t *candidate ) {
	if ( !candidate || candidate->id.classId >= RAL_RESIDENCY_CLASS_COUNT ||
	     candidate->tier >= RAL_RESIDENCY_TIER_COUNT ||
	     candidate->id.planeMask == 0 ) return 0;
	return candidate->state == RAL_RESIDENCY_REQUESTED ||
	       candidate->state == RAL_RESIDENCY_STALE;
}

int Ral_ResidencyEvictionEligible( const ralResidencyCandidate_t *candidate ) {
	if ( !candidate || candidate->id.classId >= RAL_RESIDENCY_CLASS_COUNT ||
	     candidate->tier >= RAL_RESIDENCY_TIER_COUNT ||
	     candidate->id.planeMask == 0 ) return 0;
	if ( candidate->state != RAL_RESIDENCY_RESIDENT &&
	     candidate->state != RAL_RESIDENCY_STALE ) return 0;
	if ( candidate->pinned || !candidate->fallbackReady ) return 0;
	return candidate->tier == RAL_RESIDENCY_TIER_PREDICTED ||
	       candidate->tier == RAL_RESIDENCY_TIER_BACKGROUND;
}

int Ral_ResidencyPromotionReady( const ralResidencyCandidate_t *candidate,
		uint8_t completedPlaneMask, int parentReady ) {
	if ( !candidate || candidate->id.classId >= RAL_RESIDENCY_CLASS_COUNT ||
	     candidate->id.planeMask == 0 ||
	     candidate->state != RAL_RESIDENCY_IN_FLIGHT ) return 0;
	if ( ( completedPlaneMask & candidate->id.planeMask ) != candidate->id.planeMask )
		return 0;
	if ( candidate->id.level != 0 && !parentReady ) return 0;
	return 1;
}

int Ral_ResidencyPageRecordInit( ralResidencyPageRecord_t *record,
		const ralResidencyPageId_t *id, ralResidencyState_t initialState,
		uint32_t transitionSerial ) {
	if ( !record || !id || id->classId >= RAL_RESIDENCY_CLASS_COUNT ||
	     id->planeMask == 0 || initialState < 0 ||
	     initialState >= RAL_RESIDENCY_STATE_COUNT ) return 0;
	memset( record, 0, sizeof( *record ) );
	record->id = *id;
	record->state = initialState;
	record->completedPlaneMask = initialState == RAL_RESIDENCY_RESIDENT
		? id->planeMask : 0;
	record->parentReady = id->level == 0 ? 1 : 0;
	record->transitionSerial = transitionSerial;
	return 1;
}

int Ral_ResidencyPageRecordTransition( ralResidencyPageRecord_t *record,
		ralResidencyState_t nextState, uint8_t completedPlaneMask,
		int parentReady, uint32_t transitionSerial ) {
	ralResidencyCandidate_t candidate;
	if ( !record || record->id.classId >= RAL_RESIDENCY_CLASS_COUNT ||
	     record->id.planeMask == 0 ||
	     transitionSerial < record->transitionSerial ||
	     !Ral_ResidencyCanTransition( record->state, nextState ) ) return 0;
	if ( record->state == RAL_RESIDENCY_IN_FLIGHT &&
	     nextState == RAL_RESIDENCY_RESIDENT ) {
		memset( &candidate, 0, sizeof( candidate ) );
		candidate.id = record->id;
		candidate.state = record->state;
		if ( !Ral_ResidencyPromotionReady( &candidate, completedPlaneMask,
		                                  parentReady ) ) return 0;
	}
	record->state = nextState;
	record->completedPlaneMask = completedPlaneMask;
	record->parentReady = parentReady ? 1 : 0;
	record->transitionSerial = transitionSerial;
	return 1;
}

ralResidencyScore_t Ral_ResidencyScore( const ralResidencyCandidate_t *candidate,
		const ralResidencyPolicy_t *policy ) {
	ralResidencyScore_t result = { 0 };
	uint64_t age, ageScale, halfLife;

	if ( !candidate ) return result;
	if ( !policy ) policy = &ralResidencyDefaultPolicy;

	age = candidate->ageFrames;
	if ( policy->ageCapFrames && age > policy->ageCapFrames )
		age = policy->ageCapFrames;
	halfLife = policy->halfLifeFrames ? policy->halfLifeFrames : 1;
	ageScale = halfLife + age;

	if ( candidate->screenErrorQ8 && ageScale > UINT64_MAX / candidate->screenErrorQ8 )
		result.screenAge = UINT64_MAX;
	else
		result.screenAge = ( (uint64_t)candidate->screenErrorQ8 * ageScale ) / halfLife;

	result.samples = (uint64_t)candidate->sampleCount * policy->sampleWeightQ8;
	result.motion = candidate->motionQ8;
	result.explicitBoost = candidate->explicitBoostQ8;
	result.cost = policy->costDivisorBytes
		? candidate->costBytes / policy->costDivisorBytes : 0;

	result.total = ralResidencyAddSaturated( result.screenAge, result.samples );
	result.total = ralResidencyAddSaturated( result.total, result.motion );
	result.total = ralResidencyAddSaturated( result.total, result.explicitBoost );
	result.total = result.total > result.cost ? result.total - result.cost : 0;
	return result;
}

int Ral_ResidencyCompareRequest( const ralResidencyCandidate_t *a,
		const ralResidencyCandidate_t *b,
		const ralResidencyPolicy_t *policy ) {
	ralResidencyScore_t as, bs;
	int cmp;

	if ( !a || !b ) return a ? -1 : b ? 1 : 0;
	if ( a->tier != b->tier ) return a->tier < b->tier ? -1 : 1;

	if ( !policy ) policy = &ralResidencyDefaultPolicy;
	if ( policy->starvationFrames ) {
		const int aStarved = a->waitFrames >= policy->starvationFrames;
		const int bStarved = b->waitFrames >= policy->starvationFrames;
		if ( aStarved != bStarved ) return aStarved ? -1 : 1;
		if ( aStarved ) {
			cmp = ralResidencyCompareU32Descending( a->waitFrames, b->waitFrames );
			if ( cmp ) return cmp;
		}
	}

	as = Ral_ResidencyScore( a, policy );
	bs = Ral_ResidencyScore( b, policy );
	cmp = ralResidencyCompareU64Descending( as.total, bs.total );
	if ( cmp ) return cmp;
	cmp = ralResidencyCompareU32Descending( a->sampleCount, b->sampleCount );
	if ( cmp ) return cmp;
	cmp = ralResidencyCompareU32Descending( a->motionQ8, b->motionQ8 );
	if ( cmp ) return cmp;
	if ( a->costBytes != b->costBytes ) return a->costBytes < b->costBytes ? -1 : 1;
	return Ral_ResidencyPageIdCompare( &a->id, &b->id );
}

int Ral_ResidencyCompareEviction( const ralResidencyCandidate_t *a,
		const ralResidencyCandidate_t *b,
		const ralResidencyPolicy_t *policy ) {
	ralResidencyScore_t as, bs;
	int cmp;

	if ( !a || !b ) return a ? -1 : b ? 1 : 0;
	if ( a->tier != b->tier ) return a->tier > b->tier ? -1 : 1;
	cmp = ralResidencyCompareU32Descending( a->ageFrames, b->ageFrames );
	if ( cmp ) return cmp;
	as = Ral_ResidencyScore( a, policy );
	bs = Ral_ResidencyScore( b, policy );
	if ( as.total != bs.total ) return as.total < bs.total ? -1 : 1;
	if ( a->costBytes != b->costBytes ) return a->costBytes > b->costBytes ? -1 : 1;
	return Ral_ResidencyPageIdCompare( &a->id, &b->id );
}

static size_t ralResidencyBestRequest( const ralResidencyCandidate_t *candidates,
		size_t count, const ralResidencyPolicy_t *policy,
		const uint8_t *selected, int classFilter ) {
	size_t i, best = count;
	for ( i = 0; i < count; ++i ) {
		if ( selected[i] || !Ral_ResidencyRequestEligible( &candidates[i] ) ) continue;
		if ( classFilter >= 0 && candidates[i].id.classId != (uint8_t)classFilter ) continue;
		if ( best == count ||
		     Ral_ResidencyCompareRequest( &candidates[i], &candidates[best], policy ) < 0 )
			best = i;
	}
	return best;
}

static int ralResidencyFits( const ralResidencyCandidate_t *candidate,
		const ralResidencyBudget_t *budget, size_t selectedCount,
		uint64_t selectedBytes, size_t outCapacity ) {
	if ( selectedCount >= outCapacity || selectedCount >= budget->maxPages ) return 0;
	if ( candidate->costBytes > budget->maxBytes - selectedBytes ) return 0;
	return 1;
}

size_t Ral_ResidencySelectRequests( const ralResidencyCandidate_t *candidates,
		size_t count, const ralResidencyPolicy_t *policy,
		const ralResidencyBudget_t *budget, size_t *outIndices,
		size_t outCapacity, uint8_t *selectedScratch ) {
	size_t selectedCount = 0;
	uint64_t selectedBytes = 0;
	int classId;

	if ( !candidates || !budget || !outIndices || !selectedScratch ||
	     count == 0 || outCapacity == 0 || budget->maxPages == 0 ) return 0;
	memset( selectedScratch, 0, count );

	// First honor bounded per-class fairness.  A candidate that cannot fit does
	// not block a cheaper candidate in the same class.
	for ( classId = 0; classId < RAL_RESIDENCY_CLASS_COUNT; ++classId ) {
		uint32_t quota = budget->minPerClass[classId];
		while ( quota-- > 0 ) {
			size_t best = ralResidencyBestRequest( candidates, count, policy,
					selectedScratch, classId );
			while ( best != count && !ralResidencyFits( &candidates[best], budget,
					selectedCount, selectedBytes, outCapacity ) ) {
				selectedScratch[best] = 2; // considered but too expensive
				best = ralResidencyBestRequest( candidates, count, policy,
						selectedScratch, classId );
			}
			if ( best == count ) break;
			selectedScratch[best] = 1;
			outIndices[selectedCount++] = best;
			selectedBytes += candidates[best].costBytes;
		}
	}

	// A class-quota miss does not globally ban an over-budget candidate forever:
	// reset only the temporary `2` marks before filling the remaining budget.
	{
		size_t i;
		for ( i = 0; i < count; ++i ) if ( selectedScratch[i] == 2 ) selectedScratch[i] = 0;
	}

	while ( selectedCount < outCapacity && selectedCount < budget->maxPages ) {
		size_t best = ralResidencyBestRequest( candidates, count, policy,
				selectedScratch, -1 );
		while ( best != count && !ralResidencyFits( &candidates[best], budget,
				selectedCount, selectedBytes, outCapacity ) ) {
			selectedScratch[best] = 2;
			best = ralResidencyBestRequest( candidates, count, policy,
					selectedScratch, -1 );
		}
		if ( best == count ) break;
		selectedScratch[best] = 1;
		outIndices[selectedCount++] = best;
		selectedBytes += candidates[best].costBytes;
	}
	return selectedCount;
}

const char *Ral_ResidencyClassName( ralResidencyClass_t classId ) {
	switch ( classId ) {
	case RAL_RESIDENCY_CLASS_TEXTURE: return "texture";
	case RAL_RESIDENCY_CLASS_SHADOW: return "shadow";
	case RAL_RESIDENCY_CLASS_IRRADIANCE: return "irradiance";
	case RAL_RESIDENCY_CLASS_ASSET_CHUNK: return "asset-chunk";
	default: return "invalid";
	}
}

const char *Ral_ResidencyStateName( ralResidencyState_t state ) {
	switch ( state ) {
	case RAL_RESIDENCY_ABSENT: return "absent";
	case RAL_RESIDENCY_REQUESTED: return "requested";
	case RAL_RESIDENCY_IN_FLIGHT: return "in-flight";
	case RAL_RESIDENCY_RESIDENT: return "resident";
	case RAL_RESIDENCY_STALE: return "stale";
	case RAL_RESIDENCY_EVICTING: return "evicting";
	default: return "invalid";
	}
}
