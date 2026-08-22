// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_sync.h — fences (CPU↔GPU) and semaphores (GPU↔GPU, binary + timeline).
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.8, §16.3).
//
// Timeline semaphores are value-based wait/signal. On backends without them
// (caps.timelineSemaphores == qfalse — WebGPU, some older Metal) the timeline
// API is emulated with binary semaphores + extra serialization; consumers
// branch on the caps flag rather than assuming availability.

#ifndef WIRED_RAL_SYNC_H
#define WIRED_RAL_SYNC_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TIMEOUT_INFINITE  ((uint64_t)~0ull)

typedef enum {
	RAL_SEMAPHORE_BINARY,
	RAL_SEMAPHORE_TIMELINE       // requires caps.timelineSemaphores
} ralSemaphoreType_t;

// ── fences ──────────────────────────────────────────────────────────────
ralFence_t *Ral_CreateFence ( ralBackend_t *b );
void        Ral_DestroyFence( ralFence_t *f );

void        Ral_WaitFence    ( ralFence_t *f, uint64_t timeoutNs );  // RAL_TIMEOUT_INFINITE to block
void        Ral_ResetFence   ( ralFence_t *f );
qboolean    Ral_FenceSignaled( ralFence_t *f );
ralResult_t Ral_WaitFenceExact ( ralFence_t *f, uint64_t timeoutNs );
ralResult_t Ral_ResetFenceExact( ralFence_t *f );

// ── semaphores ──────────────────────────────────────────────────────────
ralSemaphore_t *Ral_CreateSemaphore ( ralBackend_t *b, ralSemaphoreType_t type );
void            Ral_DestroySemaphore( ralSemaphore_t *s );

// Timeline-semaphore value ops. On a binary semaphore these are no-ops /
// return 0 (consumers should have checked caps.timelineSemaphores first).
uint64_t Ral_GetTimelineValue( ralSemaphore_t *s );
void     Ral_SignalTimeline  ( ralSemaphore_t *s, uint64_t value );
void     Ral_WaitTimeline    ( ralSemaphore_t *s, uint64_t value, uint64_t timeoutNs );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_SYNC_H
