// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
//
// Backend-neutral semantic profiling accumulation. A sampling epoch may only
// combine frames with the same ordered lane taxonomy; otherwise durations from
// unrelated passes would be reported under the last frame's labels.

#ifndef WIRED_RAL_PROFILE_H
#define WIRED_RAL_PROFILE_H

#include <stdint.h>

#define RAL_PROFILE_MAX_LANES 16
#define RAL_PROFILE_HISTORY_CAPACITY 120

typedef struct {
	const char *labels[ RAL_PROFILE_MAX_LANES ];
	double      latestMs[ RAL_PROFILE_MAX_LANES ];
	double      averageMs[ RAL_PROFILE_MAX_LANES ];
	double      minimumMs[ RAL_PROFILE_MAX_LANES ];
	double      maximumMs[ RAL_PROFILE_MAX_LANES ];
	uint32_t    laneCount;
	uint32_t    sampleCount;
	uint32_t    topologyEpoch;
} ralProfileSnapshot_t;

typedef struct {
	const char *labels[ RAL_PROFILE_MAX_LANES ];
	double      totalMs[ RAL_PROFILE_MAX_LANES ];
	double      historyMs[ RAL_PROFILE_HISTORY_CAPACITY ][ RAL_PROFILE_MAX_LANES ];
	uint32_t    laneCount;
	uint32_t    sampleFrames;
	uint32_t    topologyEpoch;
	uint32_t    historyWrite;
	uint32_t    historyCount;
} ralProfileAccumulator_t;

void Ral_ProfileAccumulatorInit( ralProfileAccumulator_t *accumulator );
void Ral_ProfileAccumulatorClearSamples( ralProfileAccumulator_t *accumulator );

// Returns 0 when appended to the current topology, 1 when a changed topology
// started a fresh epoch, and -1 for invalid input. Invalid input is
// output-atomic. Label strings must have static/owner-stable lifetime.
int Ral_ProfileAccumulatorAdd( ralProfileAccumulator_t *accumulator,
		const char *const *labels, const double *durationMs, uint32_t laneCount );

// Copies a bounded, oldest-to-newest history reduction into a tool-neutral
// snapshot. Returns 1 when samples exist, 0 otherwise. A failed read leaves
// the caller's output untouched.
int Ral_ProfileAccumulatorSnapshot( const ralProfileAccumulator_t *accumulator,
		ralProfileSnapshot_t *snapshot );

// Copies up to capacityFrames newest samples, oldest-to-newest, into a
// frame-major array whose stride is RAL_PROFILE_MAX_LANES. Returns the number
// of copied frames; invalid/empty requests return zero without touching output.
uint32_t Ral_ProfileAccumulatorCopyHistory( const ralProfileAccumulator_t *accumulator,
		double *historyMs, uint32_t capacityFrames );

#endif // WIRED_RAL_PROFILE_H
