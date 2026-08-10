// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired/core/thread/thread.h — Wired engine threading primitives (public header)

A minimal portable thread + atomic layer that links into BOTH the client
(qcommon) and the headless server (qcommon_ded) — unlike SDL threads, which
only reach the client through the window system. Used by the background
navmesh bake (server-side, runs in both builds).

Deliberately tiny: one worker + a single-producer/single-consumer atomic
handoff is all the current consumer needs. Not a general thread pool.

Function-name prefixes (Sys_*) follow the rest of the sys layer.
===========================================================================
*/
#ifndef WIRED_CORE_THREAD_H
#define WIRED_CORE_THREAD_H

#include "../../../q_shared.h"   /* qboolean */

/* C linkage for C++ consumers (nav_impl.cpp — the background bake — is C++). */
#ifdef __cplusplus
extern "C" {
#endif

/* Opaque thread handle (a heap-allocated platform handle; NULL on failure). */
typedef struct sysThread_s sysThread_t;

/* The worker entry point. Runs on the new thread; its return value is ignored
 * (the handoff is done through caller-owned state + the atomics below). */
typedef void (*sysThreadFn_t)( void *arg );

/* Spawn a thread running fn(arg). Returns a handle to join later, or NULL if
 * the thread could not be created (the caller should then run fn inline). */
sysThread_t *Sys_CreateThread( sysThreadFn_t fn, void *arg );

/* Block until the thread has finished, then free the handle. NULL is a no-op. */
void         Sys_JoinThread( sysThread_t *thread );

/* ── Atomic 32-bit flag (the SPSC handoff) ─────────────────────────────────
 * Sys_AtomicStore writes with RELEASE ordering (all prior writes by this
 * thread are visible to a thread that then reads the flag with ACQUIRE);
 * Sys_AtomicLoad reads with ACQUIRE ordering. One writer publishes a result
 * (pointer + state) then Sys_AtomicStore(&flag, DONE); the reader spins on
 * Sys_AtomicLoad(&flag) and, once it sees DONE, is guaranteed to see the
 * result writes. This is the memory-barrier guarantee the bake handoff needs. */
typedef volatile int sysAtomic32_t;

void Sys_AtomicStore( sysAtomic32_t *a, int value );   /* release */
int  Sys_AtomicLoad ( const sysAtomic32_t *a );        /* acquire */

#ifdef __cplusplus
}
#endif

#endif /* WIRED_CORE_THREAD_H */
