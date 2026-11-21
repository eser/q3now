// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// workload_stats.c -- see workload_stats.h. Owns the com_workloadStats cvar, the
// per-slot windows, the owner-thread binding and the bounded log rows.

#include "../q_shared.h"
#include "../qcommon.h"
#include "../q_platform.h"
#include "workload_stats.h"

LOG_DECLARE_CHANNEL( ch_system, "system" );

int                     wired_workload_enabled = 0;
wired_workload_phase_t  wired_workload_phase   = WIRED_WL_PHASE_OTHER;
wired_workload_window_t wired_workload_windows[WIRED_WL_SLOT_COUNT];

typedef struct {
	const char *name;
	const char *items;
	uint32_t    sampleMask;
	uint64_t    budgetNs;
} wlSlotInfo_t;

static wlSlotInfo_t wlSlots[WIRED_WL_SLOT_COUNT] = {
#define WIRED_WL_INFO( id, name, mask, items ) { name, items, mask, 0u },
	WIRED_WORKLOAD_SLOTS( WIRED_WL_INFO )
#undef WIRED_WL_INFO
};

/* The telemetry measures calls of a microsecond or less, so it needs a clock
 * finer than Sys_NanoTime: on macOS CLOCK_MONOTONIC advances in whole
 * microseconds there, which would floor a 700 ns trace to 0. CLOCK_UPTIME_RAW is
 * the Mach absolute-time clock in nanoseconds (about 40 ns tick on Apple
 * Silicon). Only differences of two readings are used, so the epoch is free. */
#if defined( __APPLE__ )
#  include <time.h>
static int64_t WL_Now( void ) { return (int64_t)clock_gettime_nsec_np( CLOCK_UPTIME_RAW ); }
#else
static int64_t WL_Now( void ) { return Sys_NanoTime(); }
#endif

/* Only the thread that called wired_workload_bind_owner records. */
static QDECL_TLS int wlOwnerThread;

static cvar_t  *wlCvar;
static int      wlWasEnabled;
static unsigned wlFrames;
static unsigned wlBucket;

void wired_workload_register( void )
{
	static const cvarDesc_t d = CVAR_BOOL( "com_workloadStats", "0", 0,
		"Diagnostic: per-workload CPU telemetry (snapshot build/write, traces, "
		"game tick, ...) logged as 'workload v1' rows every 200 frames on the "
		"system channel. Counts are exact, per-call hot paths are time-sampled. "
		"Default 0." );
	wlCvar = Cvar_Register( &d );
}

void wired_workload_bind_owner( void )
{
	wlOwnerThread = 1;
}

int64_t wired_workload_clock_slow( void )
{
	if ( !wlOwnerThread ) return WIRED_WL_OFF;
	return WL_Now();
}

int64_t wired_workload_begin_slow( wired_workload_slot_t slot )
{
	if ( !wlOwnerThread ) return WIRED_WL_OFF;
	/* calls counts completed calls, so the first call of a window is timed. */
	if ( ( wired_workload_windows[slot].calls & wlSlots[slot].sampleMask ) != 0u )
		return WIRED_WL_UNTIMED;
	return WL_Now();
}

void wired_workload_record_slow( wired_workload_slot_t slot, int64_t token, uint64_t items )
{
	wired_workload_window_t *w;
	if ( !wlOwnerThread ) return;
	w = &wired_workload_windows[slot];
	wired_workload_add_call( w, items );
	if ( token > 0 ) {
		const int64_t now = WL_Now();
		/* A clock that went backwards (or a token from a previous epoch) is not
		 * a measurement; keep the exact count and drop the sample. */
		if ( now >= token )
			wired_workload_add_sample( w, (uint64_t)( now - token ), items,
				wlSlots[slot].budgetNs );
	}
}

void wired_workload_set_budget_ns( wired_workload_slot_t slot, uint64_t budgetNs )
{
	if ( wired_workload_enabled && wlOwnerThread ) wlSlots[slot].budgetNs = budgetNs;
}

static void WL_ClearAll( void )
{
	for ( unsigned i = 0; i < WIRED_WL_SLOT_COUNT; ++i )
		wired_workload_clear( &wired_workload_windows[i] );
}

/* One bounded row per active slot plus one context row, never per frame. The
 * histogram is written sparse (index:count) so a row stays well under the log
 * line limit however spread the tail is. */
static void WL_Flush( void )
{
	int clients = 0, bots = 0, entities = 0;
	if ( !wlFrames ) return;
	++wlBucket;
	SV_WorkloadContext( &clients, &bots, &entities );
	Com_Log( SEV_INFO, LOG_CH( ch_system ),
		"workload context v1: bucket=%u frames=%u clients=%d bots=%d entities=%d\n",
		wlBucket, wlFrames, clients, bots, entities );
	for ( unsigned s = 0; s < WIRED_WL_SLOT_COUNT; ++s ) {
		const wired_workload_window_t *w = &wired_workload_windows[s];
		char bins[WIRED_WL_BINS * 12u];
		size_t used = 0;
		if ( !w->calls ) continue;
		bins[0] = '\0';
		for ( unsigned b = 0; b < WIRED_WL_BINS; ++b ) {
			if ( !w->bins[b] ) continue;
			used += (size_t)snprintf( bins + used, sizeof( bins ) - used, "%s%u:%u",
				used ? "," : "", b, w->bins[b] );
		}
		Com_Log( SEV_INFO, LOG_CH( ch_system ),
			"workload v1: bucket=%u frames=%u slot=%s items_are=%s calls=%llu items=%llu "
			"samples=%llu sample_ns=%llu sample_items=%llu max_ns=%llu over=%u budget_ns=%llu "
			"p50_ns=%llu p95_ns=%llu p99_ns=%llu bins=%s\n",
			wlBucket, wlFrames, wlSlots[s].name, wlSlots[s].items,
			(unsigned long long)w->calls, (unsigned long long)w->items,
			(unsigned long long)w->samples, (unsigned long long)w->sampleNs,
			(unsigned long long)w->sampleItems, (unsigned long long)w->maxNs,
			w->overBudget, (unsigned long long)wlSlots[s].budgetNs,
			(unsigned long long)wired_workload_quantile_ns( w, 50, 100 ),
			(unsigned long long)wired_workload_quantile_ns( w, 95, 100 ),
			(unsigned long long)wired_workload_quantile_ns( w, 99, 100 ),
			bins );
	}
	WL_ClearAll();
	wlFrames = 0;
}

void wired_workload_frame_begin( void )
{
	const int on = ( wlCvar && wlCvar->integer ) ? 1 : 0;
	wired_workload_bind_owner();
	/* The phase is a per-frame attribution. An ERR_DROP longjmps out of the
	 * server frame without running the matching restore, so start every frame
	 * from a known state instead of trusting the previous one. */
	wired_workload_phase = WIRED_WL_PHASE_OTHER;
	if ( on && !wlWasEnabled ) {
		/* A new epoch: the first window is whatever follows the switch. */
		WL_ClearAll();
		wlFrames = 0;
	} else if ( !on && wlWasEnabled ) {
		WL_Flush();   /* do not lose the final partial window */
	}
	wlWasEnabled = on;
	wired_workload_enabled = on;
}

void wired_workload_frame_end( unsigned windowFrames )
{
	if ( !wired_workload_enabled ) return;
	if ( ++wlFrames >= windowFrames ) WL_Flush();
}
