// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_query.h — GPU query pools (timestamps, occlusion, pipeline stats).
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.7).
//
// Timestamps pair with Ral_WriteTimestamp() in ral_command.h. Convert raw
// tick deltas to nanoseconds with caps.timestampPeriodNs (== 0 → timestamps
// unsupported on this backend).

#ifndef WIRED_RAL_QUERY_H
#define WIRED_RAL_QUERY_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RAL_QUERY_TIMESTAMP,
	RAL_QUERY_OCCLUSION,
	RAL_QUERY_PIPELINE_STATISTICS
} ralQueryType_t;

typedef struct {
	ralQueryType_t type;
	uint32_t       count;        // number of queries in the pool
	const char    *debugName;
} ralQueryPoolCreateInfo_t;

ralQueryPool_t *Ral_CreateQueryPool ( ralBackend_t *b, const ralQueryPoolCreateInfo_t *ci );
void            Ral_DestroyQueryPool( ralQueryPool_t *pool );

// Must be issued (CPU-side, or via the backend's reset path) before reuse.
void Ral_ResetQueryPool( ralQueryPool_t *pool, uint32_t firstQuery, uint32_t queryCount );

// Reads `queryCount` results into `out` (one uint64 each). With wait == qtrue
// the call blocks until the results are available; with qfalse it returns
// qfalse if any requested result isn't ready yet.
qboolean Ral_GetQueryResults( ralQueryPool_t *pool, uint32_t firstQuery, uint32_t queryCount,
                              uint64_t *out, qboolean wait );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_QUERY_H
