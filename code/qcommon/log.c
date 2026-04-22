/*
===========================================================================
log.c — Unified print pipeline V1

Format → short-circuit → dispatch. One 64 KB stack buffer per call.
FIFO sink registry under a single mutex (held only for snapshot copy).
LIFO redirect stack, caller-owned frames, main-thread only.

FUTURE (V2): replace Log_Dispatch body with
    Event_Emit(EV_LOG, rec, sizeof(*rec));
===========================================================================
*/
#include "q_shared.h"
#include "qcommon.h"
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>     // time(), gmtime_r / _gmtime64_s

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <io.h>       // _write
#define STDERR_FILENO 2
#define log_write_stderr(buf, len)  _write( STDERR_FILENO, (buf), (unsigned int)(len) )
#else
#include <unistd.h>
#define log_write_stderr(buf, len)  write( STDERR_FILENO, (buf), (size_t)(len) )
#endif

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static sys_mutex_t      s_sinks_mutex;
static log_sink_t      *s_sinks_head     = NULL;
static log_sink_t      *s_sinks_tail     = NULL;
static log_severity_t   s_min_severity   = SEV_INFO;

// Thread-local stamp: which sink is currently running its emit callback.
// NULL outside of dispatch. Used to stamp rec->origin_sink so a sink can't
// re-dispatch to itself (self-origin filter in Log_Dispatch).
// V1 main-thread-only invariant: s_redirect_top is read pre-mutex at the
// top of Log_Dispatch; cross-thread Com_Log concurrent with main-thread Pop
// would race on a caller-owned stack frame. V2: make _Atomic.
static QDECL_TLS log_sink_t *s_current_sink = NULL;

static log_redirect_frame_t *s_redirect_top = NULL;

// Set on first SEV_FATAL dispatch; never cleared. Prevents a broken sink
// from recursing back through Log_Dispatch during fatal handling.
static qboolean s_errorEntered = qfalse;

// Failure counter for the file sink (see log_sink_file.c).
// Exposed as info-only cvar log_file_failures (CVAR_ROM).
static int s_fileSinkFailures = 0;

// -------------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------------

static void Log_RecomputeMinSeverity( void )
{
    log_sink_t     *s;
    log_severity_t  min = SEV_FATAL;

    Sys_MutexLock( &s_sinks_mutex );
    for ( s = s_sinks_head; s; s = s->next ) {
        if ( s->min_severity < min )
            min = s->min_severity;
    }
    Sys_MutexUnlock( &s_sinks_mutex );
    s_min_severity = min;
}

static void Log_DispatchToRedirect( const log_record_t *rec )
{
    log_redirect_frame_t *f = s_redirect_top;
    int                   avail;
    int                   to_copy;

    if ( !f || !f->buf )
        return;

    avail   = f->buf_size - f->used - 1; // leave room for NUL
    to_copy = (int)rec->body_len;
    if ( to_copy > avail )
        to_copy = avail;
    if ( to_copy <= 0 )
        return;

    memcpy( f->buf + f->used, rec->body, to_copy );
    f->used += to_copy;
    f->buf[f->used] = '\0';
}

static void Log_Dispatch( const log_record_t *rec )
{
    // FUTURE (V2): replace with Event_Emit(EV_LOG, rec, sizeof(*rec));

    // Recursion guard for SEV_FATAL: if a sink's emit calls back into
    // Com_Log(SEV_FATAL,...), bypass sinks and write direct to stderr.
    // We gate here (after format, before dispatch) so the recursion's body
    // is still available to write — the inner message often IS the root cause.
    // s_errorEntered is never cleared: once fatal, the process is walking dead.
    if ( rec->severity == SEV_FATAL ) {
        if ( s_errorEntered ) {
            log_write_stderr( rec->body, rec->body_len );
            log_write_stderr( "\n", 1 );
            return;
        }
        s_errorEntered = qtrue;
    }

    // Redirect stack: LIFO, caller-owned frames, main-thread only.
    if ( s_redirect_top ) {
        Log_DispatchToRedirect( rec );
        // SEV_FATAL always escapes EXCLUSIVE so forensics reach the file sink
        // even when an rcon / Lua console handler is capturing output.
        if ( ( s_redirect_top->flags & LOG_REDIRECT_EXCLUSIVE )
             && rec->severity != SEV_FATAL ) {
            return;
        }
    }

    // Snapshot-and-release: hold the mutex only long enough to copy the
    // active sink pointers. Emit runs outside the lock so sinks can call
    // Com_Log safely without deadlock.
    log_sink_t *snapshot[LOG_MAX_SINKS];
    int         snapshot_count = 0;

    Sys_MutexLock( &s_sinks_mutex );
    {
        log_sink_t *s;
        for ( s = s_sinks_head;
              s && snapshot_count < LOG_MAX_SINKS;
              s = s->next ) {
            if ( rec->severity >= s->min_severity ) {
                s->active_dispatches++;
                snapshot[snapshot_count++] = s;
            }
        }
    }
    Sys_MutexUnlock( &s_sinks_mutex );

    for ( int i = 0; i < snapshot_count; i++ ) {
        log_sink_t *s = snapshot[i];
        // Self-origin filter: skip the sink that produced this record to
        // prevent infinite loops when a sink calls Com_Log from its emit.
        if ( rec->origin_sink == s ) {
            s->active_dispatches--;
            continue;
        }
        s_current_sink = s;
        s->emit( rec, s->ctx );
        s_current_sink = NULL;
        s->active_dispatches--;
    }
}

// -------------------------------------------------------------------------
// Fallback stderr sink (registered before Cvar_Init)
// -------------------------------------------------------------------------

static void FallbackSink_Emit( const log_record_t *rec, void *ctx )
{
    (void)ctx;
    log_write_stderr( rec->body, rec->body_len );
    if ( rec->body_len == 0 || ((const char *)rec->body)[rec->body_len - 1] != '\n' )
        log_write_stderr( "\n", 1 );
}

static log_sink_t s_fallbackSink = {
    "fallback_stderr",
    FallbackSink_Emit,
    NULL,
    SEV_TRACE,  // accepts everything; no cvar filter at this stage
    NULL,       // no cvar
    0,
    NULL
};

void Log_UnregisterFallbackStderrSink( void )
{
    Log_UnregisterSink( &s_fallbackSink );
}

qboolean Log_RegisterFallbackStderrSink( void )
{
    if ( Sys_MutexInit( &s_sinks_mutex ) == qfalse ) {
        // Caller (Com_Init) must Com_Error ERR_FATAL on qfalse.
        return qfalse;
    }

    s_fallbackSink.next              = NULL;
    s_fallbackSink.active_dispatches = 0;
    s_fallbackSink.min_severity      = SEV_TRACE;

    Sys_MutexLock( &s_sinks_mutex );
    s_sinks_head = &s_fallbackSink;
    s_sinks_tail = &s_fallbackSink;
    Sys_MutexUnlock( &s_sinks_mutex );

    s_min_severity = SEV_TRACE;
    return qtrue;
}

// -------------------------------------------------------------------------
// Sink registration
// -------------------------------------------------------------------------

log_sink_t *Log_RegisterSink( log_sink_t *sink )
{
    if ( !sink ) return NULL;

    Sys_MutexLock( &s_sinks_mutex );
    {
        // Count existing sinks
        int count = 0;
        log_sink_t *s;
        for ( s = s_sinks_head; s; s = s->next )
            count++;

        if ( count >= LOG_MAX_SINKS ) {
            Sys_MutexUnlock( &s_sinks_mutex );
            Com_Log( SEV_ERROR, "Log_RegisterSink: registry full (%d sinks)", LOG_MAX_SINKS );
            return NULL;
        }

        // Refresh min_severity cache from severity_cvar if available
        if ( sink->severity_cvar ) {
            log_severity_t sv = Log_ParseSeverity( sink->severity_cvar->string );
            sink->min_severity = sv;
        }

        // FIFO: append to tail
        sink->next = NULL;
        sink->active_dispatches = 0;
        if ( s_sinks_tail ) {
            s_sinks_tail->next = sink;
        } else {
            s_sinks_head = sink;
        }
        s_sinks_tail = sink;
    }
    Sys_MutexUnlock( &s_sinks_mutex );

    Log_RecomputeMinSeverity();
    return sink;
}

void Log_UnregisterSink( log_sink_t *sink )
{
    if ( !sink ) return;

    Sys_MutexLock( &s_sinks_mutex );
    {
        log_sink_t **pp = &s_sinks_head;
        while ( *pp && *pp != sink )
            pp = &( *pp )->next;
        if ( *pp ) {
            *pp = sink->next;
            // Fix tail pointer if we removed the last element
            if ( s_sinks_tail == sink ) {
                // Walk list to find new tail
                log_sink_t *s = s_sinks_head;
                s_sinks_tail  = NULL;
                while ( s ) { s_sinks_tail = s; s = s->next; }
            }
        }
    }
    Sys_MutexUnlock( &s_sinks_mutex );

    Log_RecomputeMinSeverity();

    // Drain-before-destroy: spin until all in-flight dispatches that
    // captured this sink pointer complete.
    // Contract: emit returns in <100µs. If a sink violates that, we hang
    // here rather than force-unlink — forcing would re-introduce the
    // use-after-free class this counter exists to eliminate. A hung
    // shutdown is louder than a silent memory corruption.
    while ( sink->active_dispatches > 0 ) {
        Sys_Sleep( 0 );
    }
}

// -------------------------------------------------------------------------
// Redirect stack
// -------------------------------------------------------------------------

void Log_PushRedirectSink( log_redirect_frame_t *frame,
                            char *buf, int buf_size,
                            void (*flush)(void *ctx), void *ctx,
                            int flags )
{
    if ( !frame ) return;
    frame->buf      = buf;
    frame->buf_size = buf_size;
    frame->used     = 0;
    frame->flush    = flush;
    frame->ctx      = ctx;
    frame->flags    = flags;
    frame->prev     = s_redirect_top;
    s_redirect_top  = frame;

    if ( buf && buf_size > 0 )
        buf[0] = '\0';
}

qboolean Log_PopRedirectSink( void )
{
    log_redirect_frame_t *f = s_redirect_top;

    if ( !f ) {
        Com_Log( SEV_DEBUG, "Log_PopRedirectSink: stack was empty" );
        return qfalse;
    }

    s_redirect_top = f->prev;

    if ( f->flush )
        f->flush( f->ctx );

    return qtrue;
}

// -------------------------------------------------------------------------
// Core format path
// -------------------------------------------------------------------------

void Com_Logv( log_severity_t sev, const char *fmt, va_list ap )
{
    char          buf[LOG_FORMAT_BUFFER_SIZE];
    log_record_t  rec;
    int           ret;

    // Fast-path short-circuit: skip format entirely when no sink can
    // receive this severity AND no redirect is active.
    if ( sev < s_min_severity && !s_redirect_top )
        return;

    ret = vsnprintf( buf, sizeof(buf), fmt, ap );

    rec.timestamp_ns    = Sys_NanoTime();
    rec.severity        = sev;
    rec.body            = buf;
    rec.truncated       = ( ret >= (int)sizeof(buf) ) ? qtrue : qfalse;
    rec.body_len        = rec.truncated
                        ? (uint32_t)( sizeof(buf) - 1 )
                        : (uint32_t)( ret < 0 ? 0 : ret );
    rec.truncated_bytes = rec.truncated
                        ? (uint32_t)( ret - (int)( sizeof(buf) - 1 ) )
                        : 0;
    rec.origin_sink     = s_current_sink;

    Log_Dispatch( &rec );
}

void Com_Log( log_severity_t sev, const char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    Com_Logv( sev, fmt, ap );
    va_end( ap );
}

// -------------------------------------------------------------------------
// Shared helpers
// -------------------------------------------------------------------------

log_severity_t Log_ParseSeverity( const char *name )
{
    if ( !name ) return SEV_INFO;

    if ( Q_stricmp( name, "TRACE" ) == 0 ) return SEV_TRACE;
    if ( Q_stricmp( name, "DEBUG" ) == 0 ) return SEV_DEBUG;
    if ( Q_stricmp( name, "INFO"  ) == 0 ) return SEV_INFO;
    if ( Q_stricmp( name, "WARN"  ) == 0 ) return SEV_WARN;
    if ( Q_stricmp( name, "ERROR" ) == 0 ) return SEV_ERROR;
    if ( Q_stricmp( name, "FATAL" ) == 0 ) return SEV_FATAL;

    return SEV_INFO;
}

// Log_FormatTimestamp: formats local wall-clock time.
// The ns parameter carries the record's monotonic timestamp; its sub-second
// part (ns % 1e9 / 1e6) is used for millisecond precision in the output.
// Timezone offset derived from the process's system TZ (TZ env / /etc/localtime).
//
//   fmt 0  off (writes nothing, returns 0)
//   fmt 1  "HH:MM:SS"                          — short, for in-game console
//   fmt 2  "HH:MM:SS.mmm+HH:MM"                — with ms + TZ, for TTY
//   fmt 3  "YYYY-MM-DDTHH:MM:SS.mmm+HH:MM"     — full ISO 8601, for file
//
// Returns bytes written (no NUL counted). buf is NUL-terminated on success.
int Log_FormatTimestamp( int64_t ns, char *buf, int buflen, int fmt )
{
    time_t    sec;
    struct tm tm;
    int       ms;
    long      gmtoff;
    int       off_sign, off_h, off_m;
    int       written;

    if ( fmt == 0 || !buf || buflen <= 0 )
        return 0;

    // Wall-clock seconds for calendar part; monotonic sub-second for .NNN.
    sec = time( NULL );
    ms  = (int)( ( ns % 1000000000LL ) / 1000000LL );
    if ( ms < 0 ) ms = 0;

#ifdef _WIN32
    _localtime64_s( &tm, &sec );
    // tm_gmtoff not in MSVC struct tm; derive from _timezone (seconds west UTC).
    _tzset();
    gmtoff = -_timezone;
    if ( tm.tm_isdst > 0 ) gmtoff += 3600;
#else
    localtime_r( &sec, &tm );
    gmtoff = tm.tm_gmtoff;   // seconds east of UTC, provided by POSIX
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

// Log_SeverityBracket: inner-padded to 7 chars so all brackets are uniform width.
// Space on INFO/WARN is inside the bracket to preserve alignment when brackets
// are followed by a single space separator (TTY format).
const char *Log_SeverityBracket( log_severity_t sev )
{
    switch ( sev ) {
    case SEV_TRACE: return "[TRACE]";
    case SEV_DEBUG: return "[DEBUG]";
    case SEV_INFO:  return "[INFO ]";
    case SEV_WARN:  return "[WARN ]";
    case SEV_ERROR: return "[ERROR]";
    case SEV_FATAL: return "[FATAL]";
    default:        return "[?????]";
    }
}
