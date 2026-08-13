// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_profile.h"

#include <string.h>

void Ral_ProfileAccumulatorInit( ralProfileAccumulator_t *accumulator ) {
	if ( accumulator )
		memset( accumulator, 0, sizeof( *accumulator ) );
}

void Ral_ProfileAccumulatorClearSamples( ralProfileAccumulator_t *accumulator ) {
	if ( !accumulator )
		return;
	memset( accumulator->totalMs, 0, sizeof( accumulator->totalMs ) );
	accumulator->sampleFrames = 0;
}

int Ral_ProfileAccumulatorAdd( ralProfileAccumulator_t *accumulator,
		const char *const *labels, const double *durationMs, uint32_t laneCount ) {
	uint32_t i;
	int topologyChanged = 0;

	if ( !accumulator || !labels || !durationMs || laneCount == 0
		|| laneCount > RAL_PROFILE_MAX_LANES )
		return -1;
	for ( i = 0; i < laneCount; ++i ) {
		if ( !labels[i] || !labels[i][0] || !( durationMs[i] >= 0.0 ) )
			return -1;
	}

	if ( accumulator->laneCount ) {
		if ( accumulator->laneCount != laneCount ) {
			topologyChanged = 1;
		} else {
			for ( i = 0; i < laneCount; ++i ) {
				if ( !accumulator->labels[i]
					|| strcmp( accumulator->labels[i], labels[i] ) != 0 ) {
					topologyChanged = 1;
					break;
				}
			}
		}
	}

	if ( topologyChanged ) {
		uint32_t nextEpoch = accumulator->topologyEpoch + 1;
		Ral_ProfileAccumulatorInit( accumulator );
		accumulator->topologyEpoch = nextEpoch;
	} else if ( !accumulator->topologyEpoch ) {
		accumulator->topologyEpoch = 1;
	}

	if ( !accumulator->laneCount ) {
		accumulator->laneCount = laneCount;
		for ( i = 0; i < laneCount; ++i )
			accumulator->labels[i] = labels[i];
	}
	for ( i = 0; i < laneCount; ++i )
		accumulator->totalMs[i] += durationMs[i];
	memcpy( accumulator->historyMs[ accumulator->historyWrite ], durationMs,
		laneCount * sizeof( durationMs[0] ) );
	accumulator->historyWrite = ( accumulator->historyWrite + 1 ) % RAL_PROFILE_HISTORY_CAPACITY;
	if ( accumulator->historyCount < RAL_PROFILE_HISTORY_CAPACITY )
		accumulator->historyCount++;
	accumulator->sampleFrames++;
	return topologyChanged;
}

int Ral_ProfileAccumulatorSnapshot( const ralProfileAccumulator_t *accumulator,
		ralProfileSnapshot_t *snapshot ) {
	ralProfileSnapshot_t result;
	uint32_t first;
	uint32_t sample;
	uint32_t lane;

	if ( !accumulator || !snapshot || !accumulator->laneCount
		|| !accumulator->historyCount )
		return 0;

	memset( &result, 0, sizeof( result ) );
	result.laneCount = accumulator->laneCount;
	result.sampleCount = accumulator->historyCount;
	result.topologyEpoch = accumulator->topologyEpoch;
	first = ( accumulator->historyWrite + RAL_PROFILE_HISTORY_CAPACITY
		- accumulator->historyCount ) % RAL_PROFILE_HISTORY_CAPACITY;

	for ( lane = 0; lane < result.laneCount; ++lane ) {
		double total = 0.0;
		double value = accumulator->historyMs[ first ][ lane ];
		result.labels[lane] = accumulator->labels[lane];
		result.minimumMs[lane] = value;
		result.maximumMs[lane] = value;
		for ( sample = 0; sample < result.sampleCount; ++sample ) {
			uint32_t index = ( first + sample ) % RAL_PROFILE_HISTORY_CAPACITY;
			value = accumulator->historyMs[ index ][ lane ];
			total += value;
			if ( value < result.minimumMs[lane] )
				result.minimumMs[lane] = value;
			if ( value > result.maximumMs[lane] )
				result.maximumMs[lane] = value;
		}
		result.latestMs[lane] = accumulator->historyMs[
			( accumulator->historyWrite + RAL_PROFILE_HISTORY_CAPACITY - 1 )
			% RAL_PROFILE_HISTORY_CAPACITY ][lane];
		result.averageMs[lane] = total / (double)result.sampleCount;
	}

	*snapshot = result;
	return 1;
}

uint32_t Ral_ProfileAccumulatorCopyHistory( const ralProfileAccumulator_t *accumulator,
		double *historyMs, uint32_t capacityFrames ) {
	uint32_t copyCount;
	uint32_t first;
	uint32_t sample;

	if ( !accumulator || !historyMs || !capacityFrames || !accumulator->laneCount
		|| !accumulator->historyCount )
		return 0;

	copyCount = accumulator->historyCount < capacityFrames
		? accumulator->historyCount : capacityFrames;
	first = ( accumulator->historyWrite + RAL_PROFILE_HISTORY_CAPACITY - copyCount )
		% RAL_PROFILE_HISTORY_CAPACITY;
	for ( sample = 0; sample < copyCount; ++sample ) {
		uint32_t index = ( first + sample ) % RAL_PROFILE_HISTORY_CAPACITY;
		memcpy( historyMs + sample * RAL_PROFILE_MAX_LANES,
			accumulator->historyMs[index],
			accumulator->laneCount * sizeof( historyMs[0] ) );
	}
	return copyCount;
}
