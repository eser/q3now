// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_motion_sequence.h"

#include <limits.h>

static uint64_t Mix( uint64_t state, uint64_t value ) {
	state ^= value + UINT64_C( 0x9e3779b97f4a7c15 ) + ( state << 6 )
		+ ( state >> 2 );
	state ^= state >> 30;
	state *= UINT64_C( 0xbf58476d1ce4e5b9 );
	state ^= state >> 27;
	state *= UINT64_C( 0x94d049bb133111eb );
	return state ^ ( state >> 31 );
}

static qboolean ExactReceiptValid(
		const vkTemporalGenericPipelineReceipt_t *r,
		uint32_t pipelineSlot ) {
	return r && r->slot == pipelineSlot && r->tableGeneration
		&& r->entryGeneration && r->recipeOwnerEpoch
		&& r->recipeEntryGeneration && r->factoryAllocationGeneration
		&& r->catalogId ? qtrue : qfalse;
}

qboolean VK_TemporalMotionDrawSequenceAppend(
		vkTemporalMotionDrawSequence_t *sequence,
		uint32_t absoluteEntMatSlot, uint32_t pipelineSlot,
		temporalMotionOutcome_t outcome,
		const vkTemporalGenericPipelineReceipt_t *pipelineReceipt ) {
	vkTemporalMotionDrawSequence_t candidate;
	uint64_t words[10];
	uint32_t i;
	if ( !sequence || sequence->count == UINT32_MAX ) return qfalse;
	if ( outcome == TEMPORAL_MOTION_PRESERVE ) {
		if ( pipelineReceipt ) return qfalse;
	} else if ( outcome == TEMPORAL_MOTION_WRITE_VALID
			|| outcome == TEMPORAL_MOTION_INVALIDATE_OPAQUE ) {
		if ( !ExactReceiptValid( pipelineReceipt, pipelineSlot ) ) return qfalse;
	} else {
		return qfalse;
	}
	words[0] = absoluteEntMatSlot;
	words[1] = pipelineSlot;
	words[2] = (uint32_t)outcome;
	words[3] = pipelineReceipt ? pipelineReceipt->tableGeneration : 0;
	words[4] = pipelineReceipt ? pipelineReceipt->entryGeneration : 0;
	words[5] = pipelineReceipt ? pipelineReceipt->recipeOwnerEpoch : 0;
	words[6] = pipelineReceipt ? pipelineReceipt->recipeEntryGeneration : 0;
	words[7] = pipelineReceipt ? pipelineReceipt->factoryAllocationGeneration : 0;
	words[8] = pipelineReceipt ? pipelineReceipt->catalogId : 0;
	words[9] = sequence->count + 1u;
	candidate = *sequence;
	if ( candidate.count == 0 ) {
		candidate.lane0 = UINT64_C( 0x243f6a8885a308d3 );
		candidate.lane1 = UINT64_C( 0x13198a2e03707344 );
	}
	for ( i = 0; i < 10; ++i ) {
		candidate.lane0 = Mix( candidate.lane0, words[i] );
		candidate.lane1 = Mix( candidate.lane1,
			words[9u - i] ^ UINT64_C( 0xa4093822299f31d0 ) );
	}
	candidate.count++;
	*sequence = candidate;
	return qtrue;
}

qboolean VK_TemporalMotionDrawSequenceEqual(
		const vkTemporalMotionDrawSequence_t *a,
		const vkTemporalMotionDrawSequence_t *b ) {
	return a && b && a->count == b->count && a->lane0 == b->lane0
		&& a->lane1 == b->lane1 ? qtrue : qfalse;
}
