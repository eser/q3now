// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
//
// ral_residency.h — backend-neutral residency scheduling policy.
//
// This module owns deterministic identity, lifecycle, request ranking and
// bounded selection.  Producers (texture IO, future shadow pages, irradiance
// bricks, asset chunks) own execution and fences; they must not grow private
// ranking semantics beside this contract.

#ifndef WIRED_RAL_RESIDENCY_H
#define WIRED_RAL_RESIDENCY_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
	RAL_RESIDENCY_CLASS_TEXTURE = 0,
	RAL_RESIDENCY_CLASS_SHADOW,
	RAL_RESIDENCY_CLASS_IRRADIANCE,
	RAL_RESIDENCY_CLASS_ASSET_CHUNK,
	RAL_RESIDENCY_CLASS_COUNT
} ralResidencyClass_t;

typedef enum {
	RAL_RESIDENCY_ABSENT = 0,
	RAL_RESIDENCY_REQUESTED,
	RAL_RESIDENCY_IN_FLIGHT,
	RAL_RESIDENCY_RESIDENT,
	RAL_RESIDENCY_STALE,
	RAL_RESIDENCY_EVICTING,
	RAL_RESIDENCY_STATE_COUNT
} ralResidencyState_t;

// Lower value is more urgent.  Tiers are strict: score never lets an old
// background request jump a missing safety ancestor or a current-visible page.
typedef enum {
	RAL_RESIDENCY_TIER_SAFETY = 0,
	RAL_RESIDENCY_TIER_VISIBLE,
	RAL_RESIDENCY_TIER_PREDICTED,
	RAL_RESIDENCY_TIER_BACKGROUND,
	RAL_RESIDENCY_TIER_COUNT
} ralResidencyTier_t;

// One ID names one atomically promoted coherence group.  planeMask therefore
// represents all material planes that must become visible together; producers
// must not split a group into independently published descriptors.
typedef struct {
	uint32_t resource;
	uint16_t level;
	uint16_t x;
	uint16_t y;
	uint8_t  classId;
	uint8_t  planeMask;
} ralResidencyPageId_t;

typedef struct {
	ralResidencyPageId_t id;
	ralResidencyState_t  state;
	ralResidencyTier_t   tier;
	uint32_t screenErrorQ8;
	uint32_t ageFrames;
	uint32_t sampleCount;
	uint32_t motionQ8;
	uint32_t explicitBoostQ8;
	uint32_t waitFrames;
	uint64_t costBytes;
	uint8_t  pinned;
	uint8_t  fallbackReady;
} ralResidencyCandidate_t;

// Persistent owner-side state for one addressed residency page.  The record
// keeps identity and lifecycle together so an adapter cannot accidentally
// publish a completion against a different mip/chunk.  transitionSerial is an
// owner-provided monotonic frame/generation value; equal serials are allowed
// for request->in-flight transitions issued at one safe boundary.
typedef struct {
	ralResidencyPageId_t id;
	ralResidencyState_t  state;
	uint8_t              completedPlaneMask;
	uint8_t              parentReady;
	uint16_t             reserved;
	uint32_t             transitionSerial;
} ralResidencyPageRecord_t;

typedef struct {
	uint32_t ageCapFrames;
	uint32_t halfLifeFrames;
	uint32_t sampleWeightQ8;
	uint64_t costDivisorBytes;
	uint32_t starvationFrames;
} ralResidencyPolicy_t;

typedef struct {
	uint64_t screenAge;
	uint64_t samples;
	uint64_t motion;
	uint64_t explicitBoost;
	uint64_t cost;
	uint64_t total;
} ralResidencyScore_t;

typedef struct {
	uint32_t maxPages;
	uint64_t maxBytes;
	uint32_t minPerClass[RAL_RESIDENCY_CLASS_COUNT];
} ralResidencyBudget_t;

// The stable default is deliberately integer-only: trace replays are bit-exact
// across architectures and do not inherit host floating-point behavior.
extern const ralResidencyPolicy_t ralResidencyDefaultPolicy;

int Ral_ResidencyPageIdCompare( const ralResidencyPageId_t *a,
		const ralResidencyPageId_t *b );
int Ral_ResidencyCanTransition( ralResidencyState_t from,
		ralResidencyState_t to );
int Ral_ResidencyRequestEligible( const ralResidencyCandidate_t *candidate );
int Ral_ResidencyEvictionEligible( const ralResidencyCandidate_t *candidate );
// Promotion is atomic for every bit in pageId.planeMask.  Non-root pages also
// require a resident parent/coarser fallback before their descriptor/page-table
// entry may become visible.
int Ral_ResidencyPromotionReady( const ralResidencyCandidate_t *candidate,
		uint8_t completedPlaneMask, int parentReady );
int Ral_ResidencyPageRecordInit( ralResidencyPageRecord_t *record,
		const ralResidencyPageId_t *id, ralResidencyState_t initialState,
		uint32_t transitionSerial );
int Ral_ResidencyPageRecordTransition( ralResidencyPageRecord_t *record,
		ralResidencyState_t nextState, uint8_t completedPlaneMask,
		int parentReady, uint32_t transitionSerial );

ralResidencyScore_t Ral_ResidencyScore( const ralResidencyCandidate_t *candidate,
		const ralResidencyPolicy_t *policy );

// Negative means `a` wins (is selected first), positive means `b` wins.
int Ral_ResidencyCompareRequest( const ralResidencyCandidate_t *a,
		const ralResidencyCandidate_t *b,
		const ralResidencyPolicy_t *policy );
int Ral_ResidencyCompareEviction( const ralResidencyCandidate_t *a,
		const ralResidencyCandidate_t *b,
		const ralResidencyPolicy_t *policy );

// Select request indices without allocation.  selectedScratch must contain at
// least `count` bytes and is cleared by the function.  Per-class minima are
// attempted first, then the remaining global budget is filled by strict rank;
// both page and byte caps remain hard.
size_t Ral_ResidencySelectRequests( const ralResidencyCandidate_t *candidates,
		size_t count, const ralResidencyPolicy_t *policy,
		const ralResidencyBudget_t *budget, size_t *outIndices,
		size_t outCapacity, uint8_t *selectedScratch );

const char *Ral_ResidencyClassName( ralResidencyClass_t classId );
const char *Ral_ResidencyStateName( ralResidencyState_t state );

#endif // WIRED_RAL_RESIDENCY_H
