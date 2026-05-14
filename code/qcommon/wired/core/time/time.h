// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired/core/time/time.h — Wired engine time subsystem public header

All time-related primitives for the engine. Included transitively via
qcommon.h so existing consumers do not need to include this directly.

Function name prefixes (Sys_*, Com_*) are preserved for now; renaming to
Wired_* is a separate, incremental effort across all call sites.
===========================================================================
*/
#ifndef WIRED_CORE_TIME_H
#define WIRED_CORE_TIME_H

#include "../../../q_shared.h"   /* qtime_t, int64_t, qboolean */

/* ── Monotonic clocks ──────────────────────────────────────────────────── */

/* Microseconds since an arbitrary epoch. Win32: QPC. POSIX: gettimeofday
 * (wall-clock-based; monotonicity is a separate future fix). */
int64_t Sys_Microseconds( void );

/* Nanoseconds since an arbitrary epoch.
 * POSIX: CLOCK_MONOTONIC. Win32: QueryPerformanceCounter. */
int64_t Sys_NanoTime( void );

/* Milliseconds since first call. POSIX: derived from Sys_NanoTime.
 * Win32: derived from Sys_NanoTime. (Fixed in Cephe B-0a.) */
int Sys_Milliseconds( void );

/* ── Wall-clock ─────────────────────────────────────────────────────────── */

/* Unix epoch seconds. Optionally fills *tm with broken-down local time. */
int Com_RealTime( qtime_t *tm );

/* Unix epoch milliseconds. Optionally fills *tm with broken-down local time.
 * POSIX: gettimeofday. Win32: _ftime64_s. */
int64_t Com_RealTimeMs( qtime_t *tm );

/* ── Formatting ─────────────────────────────────────────────────────────── */

/* Format current wall-clock time into buf[0..buflen-1].
 * fmt 1: "HH:MM:SS"
 * fmt 2: "HH:MM:SS.mmm+HH:MM"
 * fmt 3: "YYYY-MM-DDTHH:MM:SS.mmm+HH:MM"  (ISO 8601)
 * Returns bytes written (no NUL counted). */
int Com_FormatTimestamp( char *buf, int buflen, int fmt );

#endif /* WIRED_CORE_TIME_H */
