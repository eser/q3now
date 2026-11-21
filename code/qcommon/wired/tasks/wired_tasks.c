// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
WiredTasks scheduler. See wired_tasks.h for the contract.

Concurrency model, in one paragraph. One thread (the owner) drives the scheduler.
parallel_for publishes a descriptor that lives on the owner's stack; the owner and
any awake worker claim chunk indices from it with an atomic counter, so no
per-task memory is allocated and nothing queues. The descriptor is handed to
workers under the mutex (a worker registers itself as a helper while holding it)
and withdrawn under the mutex, after which the owner waits for the helper count
to drain: a worker can therefore never touch a descriptor that has gone out of
scope. Async tasks use a fixed slot array; a slot is claimed with a CAS on its
state, by a worker or by the waiter itself, so a task always runs exactly once.
Workers sleep on one condition variable and are woken by a broadcast issued after
the publishing store, with the predicate evaluated under the mutex, so a wake-up
cannot be lost.
*/

#include "wired_tasks.h"
#include "wired_tasks_platform.h"

#include <string.h>

#define WT_DEFAULT_SLOTS 64u

enum {
	WT_SLOT_FREE = 0,
	WT_SLOT_QUEUED,
	WT_SLOT_RUNNING,
	WT_SLOT_DONE
};

typedef struct {
	uint32_t        state;        /* atomic */
	uint32_t        generation;   /* owner-only; bumped when the slot is recycled */
	wired_tasks_fn  fn;
	void           *ctx;
} wtSlot_t;

typedef struct {
	wired_tasks_range_fn fn;
	void                *ctx;
	uint32_t             count;
	uint32_t             grain;
	uint32_t             chunks;
	uint32_t             next;     /* atomic: next chunk index to claim        */
	uint32_t             done;     /* atomic: chunks finished                  */
	uint32_t             helpers;  /* atomic: workers inside this descriptor   */
} wtFor_t;

typedef struct wtWorker_s {
	struct wired_tasks  *owner;
	wt_thread_t          thread;
	wt_thread_start_t    start;
	int                  started;
	char                 name[24];
} wtWorker_t;

struct wired_tasks {
	wired_tasks_env_t env;
	unsigned          workers;
	unsigned          slotCount;
	wt_mutex_t        mutex;
	wt_cond_t         wake;
	int               syncReady;
	wtFor_t          *current;     /* guarded by mutex                         */
	int               stopping;    /* guarded by mutex                         */
	uint32_t          queued;      /* atomic: slots in QUEUED state            */
	wtSlot_t         *slots;
	wtWorker_t       *workerData;

	/* statistics (relaxed atomics) */
	uint64_t parallelFors, serialFors, chunksByCaller, chunksByWorkers;
	uint64_t asyncSubmitted, asyncInline, asyncStolenBack, workerParks, workerBusyNs;
};

/* True while this thread is inside a task body: scheduler calls made from there
 * run inline (contract rule 2), which also rules out a worker waiting on itself. */
static
#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L && !defined( __STDC_NO_THREADS__ )
_Thread_local
#else
__thread
#endif
int wtInTask;

/* ---- chunk arithmetic (shared by every backend) ------------------------ */

uint32_t wired_tasks_chunk_count( uint32_t count, uint32_t grain )
{
	if ( !grain ) grain = 1u;
	return count / grain + ( count % grain ? 1u : 0u );
}

static void wt_run_chunk( const wtFor_t *f, uint32_t chunk )
{
	const uint64_t begin = (uint64_t)chunk * f->grain;
	uint64_t end = begin + f->grain;
	if ( end > f->count ) end = f->count;
	f->fn( f->ctx, chunk, (uint32_t)begin, (uint32_t)end );
}

/* Claim and run chunks until none are left. Returns how many this thread ran. */
static uint32_t wt_help_for( wtFor_t *f )
{
	uint32_t ran = 0;
	const int wasInTask = wtInTask;
	wtInTask = 1;
	for ( ;; ) {
		const uint32_t chunk = WT_ADD( &f->next, 1u );
		if ( chunk >= f->chunks ) break;
		wt_run_chunk( f, chunk );
		WT_ADD( &f->done, 1u );
		++ran;
	}
	wtInTask = wasInTask;
	return ran;
}

static void wt_serial_for( uint32_t count, uint32_t grain, wired_tasks_range_fn fn, void *ctx )
{
	wtFor_t f;
	memset( &f, 0, sizeof( f ) );
	f.fn = fn; f.ctx = ctx; f.count = count; f.grain = grain;
	f.chunks = wired_tasks_chunk_count( count, grain );
	for ( uint32_t chunk = 0; chunk < f.chunks; ++chunk ) wt_run_chunk( &f, chunk );
}

/* ---- async slots ------------------------------------------------------- */

static int wt_try_run_slot( struct wired_tasks *t, wtSlot_t *s )
{
	uint32_t expected = WT_SLOT_QUEUED;
	if ( !WT_CAS( &s->state, &expected, (uint32_t)WT_SLOT_RUNNING ) ) return 0;
	WT_SUB( &t->queued, 1u );
	{
		const int wasInTask = wtInTask;
		wtInTask = 1;
		s->fn( s->ctx );
		wtInTask = wasInTask;
	}
	WT_STORE( &s->state, (uint32_t)WT_SLOT_DONE );
	return 1;
}

/* ---- workers ----------------------------------------------------------- */

static void wt_worker_main( void *arg )
{
	wtWorker_t *w = (wtWorker_t *)arg;
	struct wired_tasks *t = w->owner;
	wt_thread_init_self( w->name );
	for ( ;;) {
		wtFor_t *f = NULL;
		wt_mutex_lock( &t->mutex );
		for ( ;; ) {
			if ( t->stopping ) { wt_mutex_unlock( &t->mutex ); return; }
			f = t->current;
			if ( f && WT_LOAD( &f->next ) < f->chunks ) {
				WT_ADD( &f->helpers, 1u );   /* registered while the owner cannot withdraw it */
				break;
			}
			f = NULL;
			if ( WT_LOAD( &t->queued ) > 0u ) break;
			WT_ADD_RELAXED( &t->workerParks, (uint64_t)1 );
			wt_cond_wait( &t->wake, &t->mutex );
		}
		wt_mutex_unlock( &t->mutex );

		{
			const uint64_t t0 = t->env.clock_ns ? t->env.clock_ns() : 0u;
			if ( f ) {
				const uint32_t ran = wt_help_for( f );
				WT_ADD_RELAXED( &t->chunksByWorkers, (uint64_t)ran );
				WT_SUB( &f->helpers, 1u );
			} else {
				for ( unsigned i = 0; i < t->slotCount; ++i ) wt_try_run_slot( t, &t->slots[i] );
			}
			if ( t->env.clock_ns ) {
				const uint64_t t1 = t->env.clock_ns();
				if ( t1 >= t0 ) WT_ADD_RELAXED( &t->workerBusyNs, t1 - t0 );
			}
		}
	}
}

/* ---- create / destroy -------------------------------------------------- */

wired_tasks_t *wired_tasks_create( const wired_tasks_env_t *env, const wired_tasks_config_t *config )
{
	struct wired_tasks *t;
	unsigned slots, started = 0;
	if ( !env || !env->alloc || !env->release || !config ) return NULL;
	slots = config->asyncSlots ? config->asyncSlots : WT_DEFAULT_SLOTS;
	t = (struct wired_tasks *)env->alloc( sizeof( *t ) );
	if ( !t ) return NULL;
	memset( t, 0, sizeof( *t ) );
	t->env = *env;
	t->workers = config->workers;
	t->slotCount = slots;
	t->slots = (wtSlot_t *)env->alloc( sizeof( wtSlot_t ) * slots );
	if ( !t->slots ) { env->release( t ); return NULL; }
	memset( t->slots, 0, sizeof( wtSlot_t ) * slots );
	if ( !t->workers ) return t;            /* inline backend: no threads, no sync objects */

	t->workerData = (wtWorker_t *)env->alloc( sizeof( wtWorker_t ) * t->workers );
	if ( !t->workerData ) { env->release( t->slots ); env->release( t ); return NULL; }
	memset( t->workerData, 0, sizeof( wtWorker_t ) * t->workers );
	wt_mutex_init( &t->mutex );
	wt_cond_init( &t->wake );
	t->syncReady = 1;
	for ( ; started < t->workers; ++started ) {
		wtWorker_t *w = &t->workerData[started];
		w->owner = t;
		w->start.fn = wt_worker_main;
		w->start.arg = w;
		{
			const char *prefix = config->name ? config->name : "wtasks";
			size_t n = strlen( prefix );
			if ( n > sizeof( w->name ) - 4 ) n = sizeof( w->name ) - 4;
			memcpy( w->name, prefix, n );
			w->name[n] = '\0';
			w->name[n] = (char)( '0' + started % 10u );
			w->name[n + 1] = '\0';
		}
		if ( !wt_thread_create_rec( &w->thread, &w->start ) ) break;
		w->started = 1;
	}
	if ( started != t->workers ) {
		/* Could not start every worker: stop the ones that did and report failure,
		 * so the caller keeps the inline path instead of a half-built pool. */
		t->workers = started;
		wired_tasks_destroy( t );
		return NULL;
	}
	return t;
}

void wired_tasks_destroy( wired_tasks_t *t )
{
	if ( !t ) return;
	/* Outstanding async tasks finish first; nothing is abandoned. */
	for ( unsigned i = 0; i < t->slotCount; ++i ) {
		wtSlot_t *s = &t->slots[i];
		uint32_t state;
		while ( ( state = WT_LOAD( &s->state ) ) != WT_SLOT_FREE && state != WT_SLOT_DONE ) {
			if ( !wt_try_run_slot( t, s ) ) wt_yield();
		}
	}
	if ( t->syncReady ) {
		wt_mutex_lock( &t->mutex );
		t->stopping = 1;
		wt_cond_broadcast( &t->wake );
		wt_mutex_unlock( &t->mutex );
		for ( unsigned i = 0; i < t->workers; ++i )
			if ( t->workerData[i].started ) wt_thread_join( t->workerData[i].thread );
		wt_cond_destroy( &t->wake );
		wt_mutex_destroy( &t->mutex );
	}
	{
		void ( *release )( void * ) = t->env.release;
		if ( t->workerData ) release( t->workerData );
		release( t->slots );
		release( t );
	}
}

unsigned wired_tasks_workers( const wired_tasks_t *t )
{
	return t ? t->workers : 0u;
}

/* ---- parallel_for ------------------------------------------------------ */

void wired_tasks_parallel_for( wired_tasks_t *t, uint32_t count, uint32_t grain,
		wired_tasks_range_fn fn, void *ctx )
{
	wtFor_t f;
	if ( !count || !fn ) return;
	if ( !grain ) grain = 1u;
	if ( !t || !t->workers || wtInTask || wired_tasks_chunk_count( count, grain ) < 2u ) {
		if ( t ) WT_ADD_RELAXED( &t->serialFors, (uint64_t)1 );
		wt_serial_for( count, grain, fn, ctx );
		return;
	}

	memset( &f, 0, sizeof( f ) );
	f.fn = fn; f.ctx = ctx; f.count = count; f.grain = grain;
	f.chunks = wired_tasks_chunk_count( count, grain );

	wt_mutex_lock( &t->mutex );
	t->current = &f;
	wt_cond_broadcast( &t->wake );
	wt_mutex_unlock( &t->mutex );

	{
		const uint32_t ran = wt_help_for( &f );
		WT_ADD_RELAXED( &t->chunksByCaller, (uint64_t)ran );
	}
	while ( WT_LOAD( &f.done ) != f.chunks ) wt_yield();

	/* Withdraw the descriptor, then wait for any worker that registered before the
	 * withdrawal to leave it. After this nothing can reference `f`. */
	wt_mutex_lock( &t->mutex );
	t->current = NULL;
	wt_mutex_unlock( &t->mutex );
	while ( WT_LOAD( &f.helpers ) != 0u ) wt_yield();
	WT_ADD_RELAXED( &t->parallelFors, (uint64_t)1 );
}

/* ---- async submit / wait ----------------------------------------------- */

wired_tasks_handle_t wired_tasks_submit( wired_tasks_t *t, wired_tasks_fn fn, void *ctx )
{
	wired_tasks_handle_t none = { 0u, 0u };
	if ( !fn ) return none;
	if ( !t || !t->workers || wtInTask ) {
		if ( t ) WT_ADD_RELAXED( &t->asyncInline, (uint64_t)1 );
		{
			const int wasInTask = wtInTask;
			wtInTask = 1;
			fn( ctx );
			wtInTask = wasInTask;
		}
		return none;
	}
	for ( unsigned i = 0; i < t->slotCount; ++i ) {
		wtSlot_t *s = &t->slots[i];
		if ( WT_LOAD( &s->state ) != WT_SLOT_FREE ) continue;
		s->fn = fn;
		s->ctx = ctx;
		{
			wired_tasks_handle_t h;
			h.slot = i + 1u;
			h.generation = s->generation;
			WT_STORE( &s->state, (uint32_t)WT_SLOT_QUEUED );
			WT_ADD( &t->queued, 1u );
			wt_mutex_lock( &t->mutex );
			wt_cond_broadcast( &t->wake );
			wt_mutex_unlock( &t->mutex );
			WT_ADD_RELAXED( &t->asyncSubmitted, (uint64_t)1 );
			return h;
		}
	}
	/* No free slot: run it here rather than grow or block. */
	WT_ADD_RELAXED( &t->asyncInline, (uint64_t)1 );
	{
		const int wasInTask = wtInTask;
		wtInTask = 1;
		fn( ctx );
		wtInTask = wasInTask;
	}
	return none;
}

void wired_tasks_wait( wired_tasks_t *t, wired_tasks_handle_t h )
{
	wtSlot_t *s;
	if ( !t || !h.slot || h.slot > t->slotCount ) return;
	s = &t->slots[h.slot - 1u];
	if ( s->generation != h.generation ) return;   /* already waited for */
	for ( ;; ) {
		const uint32_t state = WT_LOAD( &s->state );
		if ( state == WT_SLOT_DONE ) break;
		if ( state == WT_SLOT_QUEUED ) {
			if ( wt_try_run_slot( t, s ) ) {
				WT_ADD_RELAXED( &t->asyncStolenBack, (uint64_t)1 );
				break;
			}
			continue;
		}
		wt_yield();
	}
	s->fn = NULL;
	s->ctx = NULL;
	++s->generation;
	WT_STORE( &s->state, (uint32_t)WT_SLOT_FREE );
}

void wired_tasks_stats( const wired_tasks_t *t, wired_tasks_stats_t *out )
{
	memset( out, 0, sizeof( *out ) );
	if ( !t ) return;
	out->parallelFors = __atomic_load_n( &t->parallelFors, __ATOMIC_RELAXED );
	out->serialFors = __atomic_load_n( &t->serialFors, __ATOMIC_RELAXED );
	out->chunksByCaller = __atomic_load_n( &t->chunksByCaller, __ATOMIC_RELAXED );
	out->chunksByWorkers = __atomic_load_n( &t->chunksByWorkers, __ATOMIC_RELAXED );
	out->asyncSubmitted = __atomic_load_n( &t->asyncSubmitted, __ATOMIC_RELAXED );
	out->asyncInline = __atomic_load_n( &t->asyncInline, __ATOMIC_RELAXED );
	out->asyncStolenBack = __atomic_load_n( &t->asyncStolenBack, __ATOMIC_RELAXED );
	out->workerParks = __atomic_load_n( &t->workerParks, __ATOMIC_RELAXED );
	out->workerBusyNs = __atomic_load_n( &t->workerBusyNs, __ATOMIC_RELAXED );
}
