// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_METAL_TIMING_H
#define WIRED_METAL_TIMING_H

#include <math.h>
#include <stdint.h>

typedef struct {
	uint64_t duration, beforeScene, duringScene, afterScene;
} wiredMetalAcquirePartition;

/* Both intervals use mach absolute seconds. This partitions a CPU acquire
 * call against its OWN scene submission, not all GPU work or presentation.
 * Rounding individual spans can differ from duration by up to two ns. */
static inline bool wired_metal_acquire_partition( double begin, double end,
		double sceneBegin, double sceneEnd, wiredMetalAcquirePartition *out ) {
	if ( !out || !isfinite(begin) || !isfinite(end)
			|| !isfinite(sceneBegin) || !isfinite(sceneEnd)
			|| begin <= 0.0 || end < begin || sceneBegin <= 0.0
			|| sceneEnd <= sceneBegin || end - begin >= 86400.0
			|| sceneEnd - sceneBegin >= 86400.0 ) return false;
	wiredMetalAcquirePartition result;
	result.duration = (uint64_t)( (end - begin) * 1e9 + 0.5 );
	result.beforeScene = (uint64_t)( fmax(0.0, fmin(end, sceneBegin) - begin) * 1e9 + 0.5 );
	result.duringScene = (uint64_t)( fmax(0.0, fmin(end, sceneEnd) - fmax(begin, sceneBegin)) * 1e9 + 0.5 );
	result.afterScene = (uint64_t)( fmax(0.0, end - fmax(begin, sceneEnd)) * 1e9 + 0.5 );
	*out = result;
	return true;
}

#endif
