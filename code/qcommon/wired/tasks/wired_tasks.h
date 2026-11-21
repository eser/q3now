// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
WiredTasks -- the engine's task / job module (public header).

This is the ONLY header callers include. Everything else in this directory is
private to the module, so the implementation can be rewritten or swapped (a
different scheduler, a platform backend such as libdispatch, a tuned work
stealer) without touching a single caller.

What callers get
  * parallel_for over an index range, with a deterministic partition: chunk i
    always covers [i*grain, min(count, (i+1)*grain)), whatever the number of
    workers. A caller that writes per-chunk results and merges them in chunk
    order gets bit-identical output with 0, 1 or N workers.
  * async submit/wait for a single task, so a piece of read-only work can
    overlap work on the calling thread (the world trace running while the entity
    broadphase gathers candidates).
  * Per-worker counters for the workload telemetry.

What callers never see: threads, queues, locks, wake-ups. They hand over work
and a read/write contract; the module decides how to run it.

Contract (the rules that make the results independent of worker count)
  1. A task body only READS shared state, or writes memory that no other task of
     the same call writes (its own chunk's output slots). Anything that mutates
     shared state is applied by the calling thread, in a fixed order, after the
     call returns.
  2. Task bodies do not call back into the scheduler except to run a nested
     parallel_for, which is then executed serially on the same thread.
  3. Task bodies do not allocate from the engine zone allocator, log, or touch
     cvars; none of those are thread-safe.
  4. The scheduler is driven from one thread (the owner: the main thread).
     Workers only execute tasks.
  5. A task body must not raise a non-local exit. The engine's fatal-error path
     (Com_Error with ERR_DROP) is a longjmp: taken on the owner thread inside a
     chunk it would leave the shared descriptor pointing at a dead stack frame,
     taken on a worker it is undefined. Consumers are audited for it before they
     are handed to the module, and a body that can fail reports it through its
     own output slot for the owner to act on after the call.
  6. Every handle returned by wired_tasks_submit is waited exactly once. A handle
     that is never waited keeps its slot for the life of the scheduler.

Backends (chosen by the worker count at creation)
  * 0 workers: the inline backend. Every call runs on the calling thread, in
    order. This is the determinism reference, the single-thread path (the
    browser client may have no threads) and the behaviour the engine has today.
  * N workers: a pool of N threads plus the calling thread, which helps while it
    waits. Work is claimed from one shared descriptor with an atomic counter, so
    there is no per-task allocation and nothing that can grow without bound; the
    async slots are a fixed array.

Dependencies injected at creation (the module links against nothing of the
engine): an allocator, an optional monotonic clock. It is used and unit-tested
standalone.
===========================================================================
*/

#ifndef WIRED_TASKS_H
#define WIRED_TASKS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wired_tasks wired_tasks_t;

/* Body of one chunk of a parallel_for. `chunk` is the chunk index (the slot a
 * body writes its output to), [begin, end) the index range it covers. */
typedef void ( *wired_tasks_range_fn )( void *ctx, uint32_t chunk, uint32_t begin, uint32_t end );

/* Body of one async task. */
typedef void ( *wired_tasks_fn )( void *ctx );

typedef struct {
	void *( *alloc )( size_t bytes );   /* required; called at create/destroy only */
	void  ( *release )( void *memory ); /* required                                  */
	uint64_t ( *clock_ns )( void );     /* optional; enables the busy-time counters  */
} wired_tasks_env_t;

typedef struct {
	unsigned workers;      /* 0 selects the inline backend                         */
	unsigned asyncSlots;   /* capacity of the async slot array (0 = default 64)    */
	const char *name;      /* thread name prefix, for profilers                    */
} wired_tasks_config_t;

/* Create / destroy. create returns NULL when the environment is unusable or the
 * workers cannot be started; the caller then keeps working without a scheduler
 * (every wired_tasks_* entry point also accepts a NULL scheduler and runs
 * inline, so callers need no special case). */
wired_tasks_t *wired_tasks_create( const wired_tasks_env_t *env, const wired_tasks_config_t *config );
void           wired_tasks_destroy( wired_tasks_t *tasks );

/* Number of worker threads (0 for the inline backend or a NULL scheduler). */
unsigned wired_tasks_workers( const wired_tasks_t *tasks );

/* Number of chunks parallel_for will use: ceil(count / grain). */
uint32_t wired_tasks_chunk_count( uint32_t count, uint32_t grain );

/* Run fn over [0, count) in chunks of `grain` indices (grain 0 is treated as 1)
 * and return when every chunk has finished. The calling thread takes part. */
void wired_tasks_parallel_for( wired_tasks_t *tasks, uint32_t count, uint32_t grain,
		wired_tasks_range_fn fn, void *ctx );

/* Handle of one async task. A zero handle is "already complete". */
typedef struct {
	uint32_t slot;
	uint32_t generation;
} wired_tasks_handle_t;

/* Start fn(ctx). When no slot is free, or there are no workers, the task runs
 * inline before submit returns and the handle is already complete: submit never
 * blocks and never grows a queue. */
wired_tasks_handle_t wired_tasks_submit( wired_tasks_t *tasks, wired_tasks_fn fn, void *ctx );

/* Return when the task has finished; runs it on the calling thread if no worker
 * has started it yet. Waiting twice, or on a zero handle, is a no-op. */
void wired_tasks_wait( wired_tasks_t *tasks, wired_tasks_handle_t handle );

/* Counters, for the workload telemetry and the tests. All are totals since
 * create; per-worker windows are the caller's business. */
typedef struct {
	uint64_t parallelFors;     /* parallel_for calls that used workers          */
	uint64_t serialFors;       /* calls that ran serially (inline, nested, tiny) */
	uint64_t chunksByCaller;   /* chunks the calling thread ran                  */
	uint64_t chunksByWorkers;  /* chunks the workers ran                         */
	uint64_t asyncSubmitted;   /* tasks handed to a worker slot                  */
	uint64_t asyncInline;      /* tasks run inline (no slot, no workers)         */
	uint64_t asyncStolenBack;  /* submitted tasks the waiter ran itself          */
	uint64_t workerParks;      /* times a worker went to sleep for lack of work  */
	uint64_t workerBusyNs;     /* summed worker run time (needs clock_ns)        */
} wired_tasks_stats_t;

void wired_tasks_stats( const wired_tasks_t *tasks, wired_tasks_stats_t *out );

#ifdef __cplusplus
}
#endif

#endif
