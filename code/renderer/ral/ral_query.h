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

// Phase 7.4c-submit-A4 — adopt-style wrapper around an existing backend
// VkQueryPool (caller's qvkCreateQueryPool retains lifetime ownership).
// Mirrors Ral_AdoptPipelineLayout / Ral_AdoptTexture: ownsPool=qfalse so
// Ral_DestroyQueryPool frees only the wrapper struct, the underlying
// VkQueryPool stays alive past the wrapper. `type` describes the underlying
// pool's query type (TIMESTAMP / OCCLUSION / PIPELINE_STATISTICS) so the
// wrapper's metadata matches a native Ral_CreateQueryPool result. `count`
// is the pool's queryCount — used for the existing bounds checks in
// Ral_ResetQueryPool / Ral_GetQueryResults.
//
// Used by the renderer's GPU-timestamp pool (`vk_gpu_ts_pool`): the adoption
// wrapper backs vk_ral_lookup_query_pool() so the typed Ral_CmdWriteTimestamp /
// Ral_CmdResetQueryPool entry points can replace the parallel-paths-era
// Ral_Cmd*Legacy shims at the gpu-timestamp callsites.
ralQueryPool_t *Ral_AdoptQueryPool( ralBackend_t *b,
                                    void *externalPool,
                                    ralQueryType_t type,
                                    uint32_t count,
                                    const char *debugName );

// Phase 7.4c-submit-A4: read the backend-native VkQueryPool from an adopted
// (or owned) ralQueryPool_t. Mirrors Ral_GetTextureImageHandle. Returns NULL
// on bad arg. Consumers cast back to VkQueryPool.
void *Ral_GetQueryPoolHandle( const ralQueryPool_t *pool );

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
