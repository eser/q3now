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

// Phase 7.4c-submit-BC-C-min — adopt a renderer-owned VkFence as a
// ralFence_t without taking ownership. Caller retains responsibility for
// the underlying VkFence lifecycle. The wrapper carries ownsFence=qfalse;
// Ral_DestroyFence on an adopted wrapper frees the wrapper struct but does
// NOT defer-destroy the underlying VkFence. Mirrors A4's Ral_AdoptTexture
// pattern. `externalFence` is the platform-native handle (VkFence on
// Vulkan) cast to void * — the public RAL header stays platform-neutral
// (no vulkan.h include needed). Consumers cast back to VkFence on the
// renderer side.
ralFence_t *Ral_AdoptFence( ralBackend_t *backend,
                            void *externalFence,
                            const char *debugName );

// Accessor for the underlying VkFence (works for both owned and adopted).
// Returns NULL on bad arg or on a preSignaled token-form wrapper. Consumer
// casts back to VkFence.
void *Ral_GetFenceHandle( const ralFence_t *fen );

// ── semaphores ──────────────────────────────────────────────────────────
ralSemaphore_t *Ral_CreateSemaphore ( ralBackend_t *b, ralSemaphoreType_t type );
void            Ral_DestroySemaphore( ralSemaphore_t *s );

// Phase 7.4c-submit-BC-C-min — adopt a renderer-owned VkSemaphore as a
// ralSemaphore_t without taking ownership. Caller retains responsibility
// for the underlying VkSemaphore lifecycle. The wrapper carries
// ownsSemaphore=qfalse; Ral_DestroySemaphore on an adopted wrapper frees
// the wrapper struct but does NOT defer-destroy the underlying VkSemaphore.
// `type` describes the underlying semaphore's type so the wrapper carries
// matching metadata (timeline vs binary) — must match what the caller
// passed to vkCreateSemaphore. `externalSemaphore` is the platform-native
// VkSemaphore cast to void *.
ralSemaphore_t *Ral_AdoptSemaphore( ralBackend_t *backend,
                                    void *externalSemaphore,
                                    ralSemaphoreType_t type,
                                    const char *debugName );

// Accessor for the underlying VkSemaphore. Consumer casts back to VkSemaphore.
void *Ral_GetSemaphoreHandle( const ralSemaphore_t *sem );

// Timeline-semaphore value ops. On a binary semaphore these are no-ops /
// return 0 (consumers should have checked caps.timelineSemaphores first).
uint64_t Ral_GetTimelineValue( ralSemaphore_t *s );
void     Ral_SignalTimeline  ( ralSemaphore_t *s, uint64_t value );
void     Ral_WaitTimeline    ( ralSemaphore_t *s, uint64_t value, uint64_t timeoutNs );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_SYNC_H
