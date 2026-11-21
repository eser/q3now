// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
workload_stats.h -- per-workload CPU telemetry for the runtime parallelization
decision (backlog TASK-130, capacity roadmap doc-1 Phase 0).

What it answers: for a named workload (snapshot build, snapshot write, world
trace, entity broadphase, game tick, ...) how many times it ran, how many items
it touched, how long it took, and how bad the tail is -- per window of frames,
with the numbers a go/no-go threshold needs (count, on-CPU time, p50/p95/p99,
deadline overruns).

Design rules
  * Opt-in. Everything is behind one global flag; a disabled hook is a single
    predictable branch and no clock read.
  * Counts are exact. Time is exact for coarse workloads (sample mask 0) and
    SAMPLED for per-call hot paths such as traces (every Nth call is timed), so
    the instrument does not distort what it measures. Sampled totals carry their
    own sample count and item count so consumers scale them without bias.
  * Tail latency comes from a log-linear histogram (four sub-buckets per octave,
    so a quantile is within one quarter octave of the truth).
  * Single-owner contract. Windows are plain integers with no locks: only the
    thread that bound itself as owner (the main thread) records; a call from any
    other thread behaves as if the facility were off. Collision traces run on the
    navmesh bake worker as well, which is why the hooks sit on main-thread call
    sites and never inside the shared collision code. Workers that join the
    picture in Phase 1 need per-worker windows merged at the flush; until then
    there is deliberately no shared mutable state to contend on, so the
    "contention" column of the baseline is zero by construction.

The pure window arithmetic below has no globals and is unit-tested on its own
(tests/workload_stats_test.c). The registry, the enable flag and the log rows live
in workload_stats.c.
===========================================================================
*/

#ifndef WIRED_WORKLOAD_STATS_H
#define WIRED_WORKLOAD_STATS_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- pure window arithmetic ------------------------------------------- */

/* Bin 0 holds everything below 2^WIRED_WL_MIN_EXP ns (128 ns). Above that each
 * octave is split into 2^WIRED_WL_SUB_BITS sub-buckets; the last bin is the
 * overflow for anything at or above 128 ns * 2^WIRED_WL_OCTAVES (about 2.1 s). */
#define WIRED_WL_MIN_EXP   7u
#define WIRED_WL_SUB_BITS  2u
#define WIRED_WL_SUBS      (1u << WIRED_WL_SUB_BITS)
#define WIRED_WL_OCTAVES   24u
#define WIRED_WL_BINS      (1u + WIRED_WL_OCTAVES * WIRED_WL_SUBS + 1u)

typedef struct {
	uint64_t calls;        /* exact number of completed calls                       */
	uint64_t items;        /* exact sum of the caller-defined item counts           */
	uint64_t samples;      /* calls that were timed                                 */
	uint64_t sampleNs;     /* total time of the timed calls                         */
	uint64_t sampleItems;  /* items summed over the timed calls only                */
	uint64_t maxNs;        /* longest timed call                                    */
	uint32_t overBudget;   /* timed calls longer than the slot budget               */
	uint32_t bins[WIRED_WL_BINS];
} wired_workload_window_t;

static inline unsigned wired_wl_floor_log2( uint64_t v ) {
	unsigned e = 0;
#if defined( __GNUC__ ) || defined( __clang__ )
	if ( v ) e = 63u - (unsigned)__builtin_clzll( v );
#else
	while ( v >>= 1 ) ++e;
#endif
	return e;
}

static inline unsigned wired_wl_bin_for( uint64_t ns ) {
	unsigned e, octave, sub;
	if ( ns < ( UINT64_C( 1 ) << WIRED_WL_MIN_EXP ) ) return 0u;
	e = wired_wl_floor_log2( ns );
	octave = e - WIRED_WL_MIN_EXP;
	if ( octave >= WIRED_WL_OCTAVES ) return WIRED_WL_BINS - 1u;
	sub = (unsigned)( ( ns >> ( e - WIRED_WL_SUB_BITS ) ) & ( WIRED_WL_SUBS - 1u ) );
	return 1u + octave * WIRED_WL_SUBS + sub;
}

/* Smallest time that falls in the bin, in ns (bin 0 starts at 0). */
static inline uint64_t wired_wl_bin_lower_ns( unsigned bin ) {
	unsigned octave, sub;
	if ( bin == 0u ) return 0u;
	if ( bin >= WIRED_WL_BINS - 1u )
		return UINT64_C( 1 ) << ( WIRED_WL_MIN_EXP + WIRED_WL_OCTAVES );
	octave = ( bin - 1u ) / WIRED_WL_SUBS;
	sub = ( bin - 1u ) % WIRED_WL_SUBS;
	return (uint64_t)( WIRED_WL_SUBS + sub ) << ( WIRED_WL_MIN_EXP + octave - WIRED_WL_SUB_BITS );
}

static inline void wired_workload_clear( wired_workload_window_t *w ) {
	memset( w, 0, sizeof( *w ) );
}

/* An untimed call: exact count and items only. */
static inline void wired_workload_add_call( wired_workload_window_t *w, uint64_t items ) {
	++w->calls;
	w->items += items;
}

/* A timed call. budgetNs 0 means the slot has no deadline. */
static inline void wired_workload_add_sample( wired_workload_window_t *w,
		uint64_t ns, uint64_t items, uint64_t budgetNs ) {
	++w->samples;
	w->sampleNs += ns;
	w->sampleItems += items;
	if ( ns > w->maxNs ) w->maxNs = ns;
	if ( budgetNs && ns > budgetNs ) ++w->overBudget;
	++w->bins[wired_wl_bin_for( ns )];
}

/* Upper edge of the bin holding the num/den quantile of the timed calls, so a
 * reported p99 never understates the tail. Zero when nothing was timed. The
 * overflow bin reports the observed maximum. */
static inline uint64_t wired_workload_quantile_ns( const wired_workload_window_t *w,
		uint32_t num, uint32_t den ) {
	uint64_t target, seen = 0;
	unsigned bin;
	if ( !w->samples || !den ) return 0u;
	target = ( w->samples * num + den - 1u ) / den;   /* ceil */
	if ( !target ) target = 1u;
	for ( bin = 0; bin < WIRED_WL_BINS; ++bin ) {
		seen += w->bins[bin];
		if ( seen >= target ) {
			if ( bin >= WIRED_WL_BINS - 1u ) return w->maxNs;
			return wired_wl_bin_lower_ns( bin + 1u );
		}
	}
	return w->maxNs;
}

/* ---- registry ---------------------------------------------------------- */

/* One row per slot. Columns: id, log name, sample mask (time every call whose
 * running count has all mask bits clear; 0 times every call, 15 one in 16),
 * what an "item" counts. Names are part of the log format and the report. */
#define WIRED_WORKLOAD_SLOTS( X ) \
	X( SV_FRAME_TICK,     "sv_frame_tick",     0u,  "entities"   ) \
	X( SV_FRAME_IDLE,     "sv_frame_idle",     0u,  "entities"   ) \
	X( SV_TICK,           "sv_tick",           0u,  "ticks"      ) \
	X( SV_BOT_FRAME,      "sv_bot_frame",      0u,  "none"       ) \
	X( SV_USERCMDS,       "sv_usercmds",       0u,  "none"       ) \
	X( SV_SEND_MESSAGES,  "sv_send_messages",  0u,  "none"       ) \
	X( SV_SNAP_BUILD,     "sv_snap_build",     0u,  "entities"   ) \
	X( SV_SNAP_WRITE,     "sv_snap_write",     0u,  "entities"   ) \
	X( SV_SNAP_BYTES,     "sv_snap_bytes",     0u,  "bytes"      ) \
	X( SV_TRACE_BOT,      "sv_trace_bot",      15u, "none"       ) \
	X( SV_TRACE_TICK,     "sv_trace_tick",     15u, "none"       ) \
	X( SV_TRACE_USERCMD,  "sv_trace_usercmd",  15u, "none"       ) \
	X( SV_TRACE_OTHER,    "sv_trace_other",    15u, "none"       ) \
	X( SV_TRACE_WORLD,    "sv_trace_world",    15u, "none"       ) \
	X( SV_CLIP_ENTITIES,  "sv_clip_entities",  15u, "candidates" ) \
	X( SV_AREA_ENTITIES,  "sv_area_entities",  15u, "candidates" ) \
	X( SV_CROWD_STEP,     "sv_crowd_step",     0u,  "agents"     ) \
	X( SV_CROWD_QUANTISE, "sv_crowd_quantise", 0u,  "agents"     ) \
	X( SV_CROWD_ENCODE,   "sv_crowd_encode",   0u,  "agents"     ) \
	X( CL_PARSE_SNAPSHOT, "cl_parse_snapshot", 0u,  "entities"   ) \
	X( CL_TRACE,          "cl_trace",          15u, "none"       ) \
	X( CL_CROWD_SCENE,    "cl_crowd_scene",    0u,  "drawn"      )

typedef enum {
#define WIRED_WL_ENUM( id, name, mask, items ) WIRED_WL_##id,
	WIRED_WORKLOAD_SLOTS( WIRED_WL_ENUM )
#undef WIRED_WL_ENUM
	WIRED_WL_SLOT_COUNT
} wired_workload_slot_t;

/* The server phase a trace is attributed to. Set by the server frame around the
 * work it dispatches (bot AI, the game tick, queued user commands); a trace
 * issued outside any of them is OTHER. Attribution answers who issues the
 * traces without instrumenting call sites inside game code. */
typedef enum {
	WIRED_WL_PHASE_OTHER = 0,
	WIRED_WL_PHASE_BOT,
	WIRED_WL_PHASE_TICK,
	WIRED_WL_PHASE_USERCMD,
	WIRED_WL_PHASE_COUNT
} wired_workload_phase_t;

extern int                     wired_workload_enabled;   /* from com_workloadStats */
extern wired_workload_phase_t  wired_workload_phase;
extern wired_workload_window_t wired_workload_windows[WIRED_WL_SLOT_COUNT];

/* Token values returned by the clock/begin calls. */
#define WIRED_WL_OFF        ( (int64_t)-1 )   /* disabled or not the owner thread */
#define WIRED_WL_UNTIMED    ( (int64_t)0 )    /* count it, skip the clock          */

/* Slow paths, in workload_stats.c. */
int64_t wired_workload_clock_slow( void );
int64_t wired_workload_begin_slow( wired_workload_slot_t slot );
void    wired_workload_record_slow( wired_workload_slot_t slot, int64_t token, uint64_t items );
void    wired_workload_set_budget_ns( wired_workload_slot_t slot, uint64_t budgetNs );

/* The calling thread is the one that records. Called once per frame by the main
 * loop; every other thread keeps seeing the facility as off. */
void    wired_workload_bind_owner( void );

/* Registers com_workloadStats. Called once at startup. */
void    wired_workload_register( void );

/* Per-frame driver for the main loop: binds the owner thread, applies the cvar
 * (a switch-on starts a fresh epoch, a switch-off flushes the partial window),
 * counts frames, and flushes one row per active slot every windowFrames frames. */
void    wired_workload_frame_begin( void );
void    wired_workload_frame_end( unsigned windowFrames );

/* A timestamp, or WIRED_WL_OFF when the facility is off. For callers that pick
 * the slot only after the work is done (the server frame: tick or idle). */
static inline int64_t wired_workload_clock( void ) {
	if ( !wired_workload_enabled ) return WIRED_WL_OFF;
	return wired_workload_clock_slow();
}

/* Start of a workload on a sampled slot: WIRED_WL_OFF when disabled,
 * WIRED_WL_UNTIMED when this call is not one of the sampled ones, else a
 * timestamp. Pass the result to wired_workload_end. */
static inline int64_t wired_workload_begin( wired_workload_slot_t slot ) {
	if ( !wired_workload_enabled ) return WIRED_WL_OFF;
	return wired_workload_begin_slow( slot );
}

static inline void wired_workload_end( wired_workload_slot_t slot, int64_t token,
		uint64_t items ) {
	if ( token == WIRED_WL_OFF ) return;
	wired_workload_record_slow( slot, token, items );
}

/* Scoped phase attribution: returns the previous phase for the matching restore. */
static inline wired_workload_phase_t wired_workload_phase_enter( wired_workload_phase_t p ) {
	wired_workload_phase_t previous = wired_workload_phase;
	if ( wired_workload_enabled ) wired_workload_phase = p;
	return previous;
}
static inline void wired_workload_phase_leave( wired_workload_phase_t previous ) {
	if ( wired_workload_enabled ) wired_workload_phase = previous;
}

/* The trace slot a server trace belongs to under the current phase. */
static inline wired_workload_slot_t wired_workload_trace_slot( void ) {
	switch ( wired_workload_phase ) {
	case WIRED_WL_PHASE_BOT:     return WIRED_WL_SV_TRACE_BOT;
	case WIRED_WL_PHASE_TICK:    return WIRED_WL_SV_TRACE_TICK;
	case WIRED_WL_PHASE_USERCMD: return WIRED_WL_SV_TRACE_USERCMD;
	default:                     return WIRED_WL_SV_TRACE_OTHER;
	}
}

#ifdef __cplusplus
}
#endif

#endif
