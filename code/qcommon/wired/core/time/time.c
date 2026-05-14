// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired/core/time/time.c — Wired engine time subsystem

Sources:
  Sys_Microseconds      ← code/qcommon/common.c
  Sys_NanoTime          ← code/qcommon/common.c
  Sys_Milliseconds      ← code/unix/unix_shared.c (POSIX) +
                          code/win32/win_shared.c (Win32), merged via #ifdef
  Com_RealTime          ← code/qcommon/wired/core/logging/log.c
  Com_RealTimeMs        ← code/qcommon/wired/core/logging/log.c (added Cephe B-0b)
  Com_FormatTimestamp   ← code/qcommon/wired/core/logging/log.c (was Log_FormatTimestamp)

sys_timeBase is a non-static global so that Sys_XTimeToSysTime in
code/unix/linux_glimp.c can reference it via `extern unsigned long sys_timeBase`
without modification — Option A ownership (time.c owns all time state) without
requiring a touch on linux_glimp.c.
===========================================================================
*/

#ifndef _WIN32
#  include <sys/time.h>   /* gettimeofday, struct timeval */
#  include <time.h>       /* clock_gettime, CLOCK_MONOTONIC, localtime_r, time() */
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>    /* LARGE_INTEGER, QueryPerformanceCounter/Frequency */
#  undef WIN32_LEAN_AND_MEAN
#  include <sys/timeb.h>  /* _ftime64_s */
#  include <time.h>       /* time(), _localtime64_s, struct tm */
#endif

// NOLINTNEXTLINE(readability-duplicate-include) — distinct from the system <time.h> above
#include "time.h"   /* own public header; pulls in q_shared.h → qtime_t, int64_t */

// -------------------------------------------------------------------------
// sys_timeBase — X11 epoch anchor (owned here, read via extern in linux_glimp.c)
// -------------------------------------------------------------------------
// Wall-clock epoch second, set once at first Sys_Milliseconds call.
// Only relevant on X11 Linux; kept as a regular global so linux_glimp.c's
// existing `extern unsigned long sys_timeBase;` continues to compile unchanged.
unsigned long sys_timeBase = 0;

// -------------------------------------------------------------------------
// Sys_Microseconds
// -------------------------------------------------------------------------
// Moved from code/qcommon/common.c.
// Win32: QueryPerformanceCounter (monotonic). POSIX: gettimeofday (wall-clock).
int64_t Sys_Microseconds( void )
{
#ifdef _WIN32
	static qboolean inited = qfalse;
	static LARGE_INTEGER base;
	static LARGE_INTEGER freq;
	LARGE_INTEGER curr;

	if ( !inited )
	{
		QueryPerformanceFrequency( &freq );
		QueryPerformanceCounter( &base );
		if ( !freq.QuadPart )
		{
			return (int64_t)Sys_Milliseconds() * 1000LL; // fallback
		}
		inited = qtrue;
		return 0;
	}

	QueryPerformanceCounter( &curr );

	return ((curr.QuadPart - base.QuadPart) * 1000000LL) / freq.QuadPart;
#else
	struct timeval curr;
	gettimeofday( &curr, NULL );

	return (int64_t)curr.tv_sec * 1000000LL + (int64_t)curr.tv_usec;
#endif
}

// -------------------------------------------------------------------------
// Sys_NanoTime
// -------------------------------------------------------------------------
// Moved from code/qcommon/common.c.
// Contract-infallible: POSIX guarantees CLOCK_MONOTONIC; Win32 QPC on XP+.
// Worst case on failure is a single malformed timestamp in one log line.
int64_t Sys_NanoTime( void )
{
#ifdef _WIN32
	static qboolean     inited = qfalse;
	static LARGE_INTEGER base;
	static LARGE_INTEGER freq;
	LARGE_INTEGER        curr;

	if ( !inited )
	{
		QueryPerformanceFrequency( &freq );
		QueryPerformanceCounter( &base );
		inited = qtrue;
		return 0;
	}

	QueryPerformanceCounter( &curr );
	// multiply before divide to keep precision; freq is ~10MHz on modern HW
	return ((curr.QuadPart - base.QuadPart) * 1000000000LL) / freq.QuadPart;
#else
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

// -------------------------------------------------------------------------
// Sys_Milliseconds
// -------------------------------------------------------------------------
// Merged from code/unix/unix_shared.c (POSIX) and code/win32/win_shared.c.
// Both paths now derive from Sys_NanoTime (fixed in Cephe B-0a).
// POSIX path also sets sys_timeBase once for Sys_XTimeToSysTime.
int Sys_Milliseconds( void )
{
	static int64_t base = 0;
	int64_t        now  = Sys_NanoTime();

	if ( base == 0 )
	{
#ifndef _WIN32
		struct timeval tp;
		gettimeofday( &tp, NULL );
		sys_timeBase = tp.tv_sec;  /* wall-clock anchor for Sys_XTimeToSysTime */
		base         = now;
		return (int)( tp.tv_usec / 1000 );
#else
		base = now;
		return 0;
#endif
	}

	return (int)( ( now - base ) / 1000000LL );
}

// -------------------------------------------------------------------------
// Com_RealTime
// -------------------------------------------------------------------------
// Moved from code/qcommon/wired/core/logging/log.c.
int Com_RealTime( qtime_t *qtime ) {
	time_t t = time( NULL );
	if ( !qtime )
		return t;
	struct tm *tms = localtime( &t );
	if ( tms ) {
		qtime->tm_sec   = tms->tm_sec;
		qtime->tm_min   = tms->tm_min;
		qtime->tm_hour  = tms->tm_hour;
		qtime->tm_mday  = tms->tm_mday;
		qtime->tm_mon   = tms->tm_mon;
		qtime->tm_year  = tms->tm_year;
		qtime->tm_wday  = tms->tm_wday;
		qtime->tm_yday  = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}
	return t;
}

// -------------------------------------------------------------------------
// Com_RealTimeMs
// -------------------------------------------------------------------------
// Added in Cephe B-0b; moved here from log.c.
int64_t Com_RealTimeMs( qtime_t *qtime ) {
#ifdef _WIN32
	struct __timeb64 tb;
	time_t           sec;
	_ftime64_s( &tb );
	sec = (time_t)tb.time;
	if ( qtime ) {
		struct tm tms;
		_localtime64_s( &tms, &sec );
		qtime->tm_sec   = tms.tm_sec;
		qtime->tm_min   = tms.tm_min;
		qtime->tm_hour  = tms.tm_hour;
		qtime->tm_mday  = tms.tm_mday;
		qtime->tm_mon   = tms.tm_mon;
		qtime->tm_year  = tms.tm_year;
		qtime->tm_wday  = tms.tm_wday;
		qtime->tm_yday  = tms.tm_yday;
		qtime->tm_isdst = tms.tm_isdst;
	}
	return (int64_t)tb.time * 1000 + (int64_t)tb.millitm;
#else
	struct timeval tv;
	gettimeofday( &tv, NULL );
	if ( qtime ) {
		struct tm tms;
		localtime_r( &tv.tv_sec, &tms );
		qtime->tm_sec   = tms.tm_sec;
		qtime->tm_min   = tms.tm_min;
		qtime->tm_hour  = tms.tm_hour;
		qtime->tm_mday  = tms.tm_mday;
		qtime->tm_mon   = tms.tm_mon;
		qtime->tm_year  = tms.tm_year;
		qtime->tm_wday  = tms.tm_wday;
		qtime->tm_yday  = tms.tm_yday;
		qtime->tm_isdst = tms.tm_isdst;
	}
	return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
#endif
}

// -------------------------------------------------------------------------
// Com_FormatTimestamp
// -------------------------------------------------------------------------
// Moved from code/qcommon/wired/core/logging/log.c (was Log_FormatTimestamp,
// renamed to Com_FormatTimestamp in Cephe B-0b).
// All fields derived from a single clock read so seconds and ms never mismatch.
// Timezone offset derived from the process's system TZ (TZ env / /etc/localtime).
//
//   fmt 0  off (writes nothing, returns 0)
//   fmt 1  "HH:MM:SS"                          — short, for in-game console
//   fmt 2  "HH:MM:SS.mmm+HH:MM"                — with ms + TZ, for TTY
//   fmt 3  "YYYY-MM-DDTHH:MM:SS.mmm+HH:MM"     — full ISO 8601, for file
//
// Returns bytes written (no NUL counted). buf is NUL-terminated on success.
int Com_FormatTimestamp( char *buf, int buflen, int fmt )
{
    struct tm tm;
    int       ms;
    long      gmtoff;
    int       off_sign, off_h, off_m;
    int       written;

    if ( fmt == 0 || !buf || buflen <= 0 )
        return 0;

#ifdef _WIN32
    {
        struct __timeb64 tb;
        time_t sec;
        _ftime64_s( &tb );
        sec = (time_t)tb.time;
        ms  = (int)tb.millitm;
        _localtime64_s( &tm, &sec );
        // tm_gmtoff not in MSVC struct tm; derive from _timezone (seconds west UTC).
        _tzset();
        gmtoff = -_timezone;
        if ( tm.tm_isdst > 0 ) gmtoff += 3600;
    }
#else
    {
        struct timeval tv;
        gettimeofday( &tv, NULL );
        ms = (int)( tv.tv_usec / 1000 );
        localtime_r( &tv.tv_sec, &tm );
        gmtoff = tm.tm_gmtoff; // seconds east of UTC, provided by POSIX
    }
#endif

    off_sign = ( gmtoff >= 0 ) ? '+' : '-';
    off_h    = (int)( labs( gmtoff ) / 3600 );
    off_m    = (int)( ( labs( gmtoff ) % 3600 ) / 60 );

    switch ( fmt ) {
    case 1:
        // Short local time: "HH:MM:SS"
        written = snprintf( buf, buflen, "%02d:%02d:%02d",
            tm.tm_hour, tm.tm_min, tm.tm_sec );
        break;
    case 2:
        // Local time + ms + TZ offset: "HH:MM:SS.mmm+HH:MM"
        written = snprintf( buf, buflen, "%02d:%02d:%02d.%03d%c%02d:%02d",
            tm.tm_hour, tm.tm_min, tm.tm_sec, ms,
            off_sign, off_h, off_m );
        break;
    case 3:
        // Full ISO 8601 local with offset: "YYYY-MM-DDTHH:MM:SS.mmm+HH:MM"
        written = snprintf( buf, buflen,
            "%04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, ms,
            off_sign, off_h, off_m );
        break;
    default:
        return 0;
    }

    return written < 0 ? 0 : written;
}
