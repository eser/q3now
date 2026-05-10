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
#include "log_buffer.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>     // time(), localtime_r / _localtime64_s

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <io.h>       // _write
#include <sys/timeb.h> // _ftime64_s
#define STDERR_FILENO 2
#define log_write_stderr(buf, len)  _write( STDERR_FILENO, (buf), (unsigned int)(len) )
#if defined(_DEBUG)
#include "../../../win32/win_local.h"
#endif
#else
#include <unistd.h>
#include <sys/time.h>  // gettimeofday
#define log_write_stderr(buf, len)  write( STDERR_FILENO, (buf), (size_t)(len) )
#endif

// -------------------------------------------------------------------------
// Channels owned by the logging subsystem itself
// -------------------------------------------------------------------------

LOG_DECLARE_CHANNEL( ch_system, "system" );
LOG_DECLARE_CHANNEL( ch_log,    "log" );

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static sys_mutex_t      s_sinks_mutex;
static log_sink_t      *s_sinks_head     = NULL;
static log_sink_t      *s_sinks_tail     = NULL;

// Global severity gate. NULL before Cvar_Init; cvar registered in Com_Init.
cvar_t *log_severity_cvar = NULL;

// Cached parsed severity. Channel resolver reads this directly — no string
// parsing on the resolve hot path. Default SEV_INFO so the pre-Cvar_Init
// window doesn't flood with TRACE/DEBUG before Log_InitChannels seeds the
// cache from log_severity_cvar->string.
int log_global_severity = (int)SEV_INFO;

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

// Refresh the cached global severity and re-resolve every channel.
//
// Wired to log_severity_cvar's onChange. Also called manually from
// Log_InitChannels so the initial cvar value (applied by Cvar_Register
// without firing onChange) lands in the cache before any channel resolves.
//
// Parse-once-on-change pattern: the channel hot path (Com_Logv → effectiveSev
// compare) and the resolve path (Log_ResolveChannel_NoLock) both read the
// cached int directly. The cvar's string field is only touched here.
//
// Empty/missing string: keep the last good value. The cache is already
// primed with a valid threshold (either the static initializer at log.c
// file scope, a previous successful parse, or the boot-time seed in
// Log_InitChannels). No constant fallback — the gate always reflects the
// most recent meaningful instruction, never an arbitrary default chosen
// at the call site.
//
// Why guard empty before parse: Log_ParseSeverity returns SEV_TRACE for an
// empty input string. That's the right semantic for sink-side
// log_*_severity cvars (empty = "show everything"), but the wrong semantic
// for the global gate, where empty must mean "no instruction received".
static void Log_OnGlobalSeverityChanged( cvar_t *self )
{
    if ( !self || !self->string || !self->string[0] )
        return;

    log_global_severity = (int) Log_ParseSeverity( self->string );
    Log_ResolveAllChannels();
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
            if ( s->severity_cvar &&
                 s->severity_cvar->modificationCount != s->last_cvar_mod ) {
                s->min_severity  = Log_ParseSeverity( s->severity_cvar->string );
                s->last_cvar_mod = s->severity_cvar->modificationCount;
            }
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
    NULL,       // no cvar — last_cvar_mod never read when severity_cvar is NULL
    -1,         // last_cvar_mod
    0,          // active_dispatches
    NULL        // next
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
            Com_Log( SEV_ERROR, LOG_CH(ch_system), "Log_RegisterSink: registry full (%d sinks)", LOG_MAX_SINKS );
            return NULL;
        }

        // Seed min_severity cache and sync modificationCount so first dispatch
        // skips the recompute (the value was just set here).
        if ( sink->severity_cvar ) {
            sink->min_severity = Log_ParseSeverity( sink->severity_cvar->string );
            sink->last_cvar_mod = sink->severity_cvar->modificationCount;
        } else {
            sink->last_cvar_mod = -1;
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
        Com_Log( SEV_DEBUG, LOG_CH(ch_system), "Log_PopRedirectSink: stack was empty" );
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

void Com_Logv( log_severity_t sev, int channel, const char *fmt, va_list ap )
{
    char          buf[LOG_FORMAT_BUFFER_SIZE];
    log_record_t  rec;
    int           ret;
    int           gate;

    // Bound the channel index. Out-of-range or unregistered channels route
    // to id 0 ("general") so the JSONL emitter and severity gate both have
    // a valid slot to consult. log_channelCount is read lock-free; the
    // publish-after-write pattern in Log_GetChannel guarantees that a
    // visible counter always has a fully-populated slot below it.
    if ( channel < 0 || channel >= log_channelCount )
        channel = 0;

    ret = vsnprintf( buf, sizeof(buf), fmt, ap );

    rec.severity        = sev;
    rec.channel         = channel;
    rec.body            = buf;
    rec.truncated       = ( ret >= (int)sizeof(buf) ) ? qtrue : qfalse;
    rec.body_len        = rec.truncated
                        ? (uint32_t)( sizeof(buf) - 1 )
                        : (uint32_t)( ret < 0 ? 0 : ret );
    rec.truncated_bytes = rec.truncated
                        ? (uint32_t)( ret - (int)( sizeof(buf) - 1 ) )
                        : 0;
    rec.origin_sink     = s_current_sink;
    rec.ts_mono         = Sys_NanoTime();
    Com_FormatTimestamp( rec.ts_wall, (int)sizeof( rec.ts_wall ), 3 );

    // Capture into ring before severity gate — buffer sees everything.
    LogBuffer_Append( &rec );

    // Hot-path gate: aligned-int read of the channel's resolved threshold.
    // No lock — see threading note in log_channels.c.
    gate = log_channels[channel].effectiveSev;
    if ( (int)sev < gate && !s_redirect_top )
        return;

    Log_Dispatch( &rec );
}

void Com_Log_Impl( log_severity_t sev, int channel, const char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    Com_Logv( sev, channel, fmt, ap );
    va_end( ap );
}

// -------------------------------------------------------------------------
// Shared helpers
// -------------------------------------------------------------------------

log_severity_t Log_ParseSeverity( const char *name )
{
    if ( !name || !*name ) {
        return SEV_TRACE;
    }

    if ( Q_stricmp( name, "TRACE" ) == 0 ) return SEV_TRACE;
    if ( Q_stricmp( name, "DEBUG" ) == 0 ) return SEV_DEBUG;
    if ( Q_stricmp( name, "INFO"  ) == 0 ) return SEV_INFO;
    if ( Q_stricmp( name, "WARN"  ) == 0 ) return SEV_WARN;
    if ( Q_stricmp( name, "ERROR" ) == 0 ) return SEV_ERROR;
    if ( Q_stricmp( name, "FATAL" ) == 0 ) return SEV_FATAL;

    return SEV_INFO;
}

// -------------------------------------------------------------------------
// Unified `log` console command — channel-based severity control
// -------------------------------------------------------------------------
//
// Forms:
//   log                            list active overrides + global severity
//   log channels                   list every registered channel (hierarchical)
//   log <prefix> <severity>        set severity for all channels matching prefix
//   log <prefix> off               suppress (overrideSev = SEV_FATAL+1)
//   log <prefix>                   clear override on matching channels
//   log all <severity>             apply to every channel
//   log all                        clear every override
//
// Prefix match: `prefix` matches a channel iff name == prefix OR name starts
// with prefix + "." (so "renderer" matches "renderer" and "renderer.trail"
// but NOT "renderer2"). Case-insensitive. The literal "all" matches every
// channel regardless of name.

static const char *Log_OverrideSevName( int sev ) {
    if ( sev < 0 )                return "(inherit)";
    if ( sev > (int)SEV_FATAL )   return "OFF";
    return Log_SeverityName( (log_severity_t)sev );
}

static qboolean Log_PrefixMatch( const char *name, const char *prefix ) {
    int plen;
    if ( Q_stricmp( prefix, "all" ) == 0 ) return qtrue;
    plen = (int)strlen( prefix );
    if ( Q_stricmpn( name, prefix, plen ) != 0 ) return qfalse;
    return ( name[plen] == '\0' || name[plen] == '.' ) ? qtrue : qfalse;
}

// Channel-name enumerator for tab completion (also injects "all" + "channels").
static void Log_ChannelEnumerator( void (*cb)( const char *s ) ) {
    int i;
    cb( "all" );
    cb( "channels" );
    for ( i = 0; i < log_channelCount; i++ ) {
        cb( log_channels[i].name );
    }
}

// Severity-name enumerator (incl. "off") for tab completion at arg position 3.
static void Log_SevEnumerator( void (*cb)( const char *s ) ) {
    cb( "trace" );
    cb( "debug" );
    cb( "info" );
    cb( "warn" );
    cb( "error" );
    cb( "fatal" );
    cb( "off" );
}

static void Log_Cmd_Complete( const char *args, int argNum ) {
    (void)args;
    if ( argNum == 2 ) {
        Field_CompleteList( Log_ChannelEnumerator );
    } else if ( argNum == 3 ) {
        Field_CompleteList( Log_SevEnumerator );
    }
}

// Sort comparator for channel ids — alphabetical by name, case-insensitive.
// The hierarchy ("renderer" before "renderer.trail") emerges naturally because
// the dot character (0x2E) sorts before any alphanumeric.
static int Log_ChannelIdCmp( const void *pa, const void *pb ) {
    int a = *(const int *)pa;
    int b = *(const int *)pb;
    return Q_stricmp( log_channels[a].name, log_channels[b].name );
}

static void Log_PrintActiveOverrides( void ) {
    int  ids[LOG_MAX_CHANNELS];
    int  count = 0;
    int  overrides = 0;
    int  i;
    // Display the gate threshold the way the hot path actually sees it:
    // map the cached log_global_severity int back to a name. No raw cvar
    // string read, no fallback constant — Log_OverrideSevName always
    // returns a meaningful label for a value in [SEV_TRACE..SEV_FATAL].
    const char *global_str = Log_OverrideSevName( log_global_severity );

    for ( i = 0; i < log_channelCount; i++ ) {
        ids[count++] = i;
        if ( log_channels[i].overrideSev >= 0 ) overrides++;
    }
    qsort( ids, count, sizeof( ids[0] ), Log_ChannelIdCmp );

    if ( overrides == 0 ) {
        Com_Log( SEV_INFO, LOG_CH( ch_log ),
            "No active log overrides (%d channels, global: %s)\n",
            count, global_str );
    } else {
        Com_Log( SEV_INFO, LOG_CH( ch_log ),
            "Active log overrides (%d of %d channels):\n",
            overrides, count );
        for ( i = 0; i < count; i++ ) {
            const logChannel_t *ch = &log_channels[ids[i]];
            if ( ch->overrideSev < 0 ) continue;
            Com_Log( SEV_INFO, LOG_CH( ch_log ),
                "  %-24s %s\n", ch->name, Log_OverrideSevName( ch->overrideSev ) );
        }
        Com_Log( SEV_INFO, LOG_CH( ch_log ),
            "Global severity: %s\n", global_str );
    }
    Com_Log( SEV_INFO, LOG_CH( ch_log ),
        "Use 'log channels' to list all channels.\n" );
}

static void Log_PrintAllChannels( void ) {
    int  ids[LOG_MAX_CHANNELS];
    int  count = 0;
    int  i;
    const char *global_str = Log_OverrideSevName( log_global_severity );

    for ( i = 0; i < log_channelCount; i++ )
        ids[count++] = i;
    qsort( ids, count, sizeof( ids[0] ), Log_ChannelIdCmp );

    Com_Log( SEV_INFO, LOG_CH( ch_log ),
        "All registered log channels (%d):\n", count );

    for ( i = 0; i < count; i++ ) {
        const logChannel_t *ch = &log_channels[ids[i]];
        int   indent = 0;
        const char *p;
        char  pad[32];
        // Indent two spaces per dot-depth.
        for ( p = ch->name; *p; p++ ) if ( *p == '.' ) indent++;
        if ( indent * 2 >= (int)sizeof( pad ) ) indent = ( (int)sizeof( pad ) - 1 ) / 2;
        memset( pad, ' ', indent * 2 );
        pad[indent * 2] = '\0';

        if ( ch->overrideSev >= 0 ) {
            Com_Log( SEV_INFO, LOG_CH( ch_log ),
                "  %s%-26s  %-9s  (override)\n",
                pad, ch->name, Log_OverrideSevName( ch->overrideSev ) );
        } else {
            Com_Log( SEV_INFO, LOG_CH( ch_log ),
                "  %s%-26s  (%s, inherited)\n",
                pad, ch->name,
                Log_OverrideSevName( ch->effectiveSev ) );
        }
    }
    Com_Log( SEV_INFO, LOG_CH( ch_log ),
        "Global severity: %s\n", global_str );
}

static void Log_Cmd_f( void ) {
    int          argc = Cmd_Argc();
    const char  *prefix;
    const char  *sev_str;
    int          new_override;   // -1 = clear, >=0 = set
    int          matched = 0;
    int          i;

    if ( argc <= 1 ) {
        Log_PrintActiveOverrides();
        return;
    }

    prefix = Cmd_Argv( 1 );

    if ( argc == 2 && Q_stricmp( prefix, "channels" ) == 0 ) {
        Log_PrintAllChannels();
        return;
    }

    if ( argc == 2 ) {
        // Clear override on prefix-match (or all).
        new_override = -1;
    } else {
        sev_str = Cmd_Argv( 2 );
        if ( Q_stricmp( sev_str, "off" ) == 0 ) {
            new_override = (int)SEV_FATAL + 1;
        } else if ( !*sev_str ) {
            new_override = -1;
        } else {
            new_override = (int)Log_ParseSeverity( sev_str );
        }
    }

    // Apply to every channel matching prefix. Writing overrideSev is an
    // aligned-int store; we don't take the registry mutex here. Resolve
    // happens below under the mutex.
    for ( i = 0; i < log_channelCount; i++ ) {
        if ( Log_PrefixMatch( log_channels[i].name, prefix ) ) {
            log_channels[i].overrideSev = new_override;
            matched++;
        }
    }
    Log_ResolveAllChannels();

    if ( matched == 0 ) {
        Com_Log( SEV_WARN, LOG_CH( ch_log ),
            "log: no channels match prefix '%s'\n", prefix );
        return;
    }

    if ( new_override < 0 ) {
        Com_Log( SEV_INFO, LOG_CH( ch_log ),
            "log: cleared override on %d channel(s) matching '%s' (now inherits global)\n",
            matched, prefix );
    } else {
        Com_Log( SEV_INFO, LOG_CH( ch_log ),
            "log: set %d channel(s) matching '%s' to %s\n",
            matched, prefix, Log_OverrideSevName( new_override ) );
    }
}

void Log_InitChannels( void ) {
    // Wire global severity changes through to channel resolution. Whenever
    // log_severity changes, every channel without an explicit override
    // re-inherits the new floor.
    //
    // Cvar_Register does NOT fire onChange for the initial default value —
    // the callback only triggers on subsequent value changes. So we wire the
    // callback first, then invoke it manually with the current cvar to seed
    // log_global_severity from log_severity_cvar->string. After this point,
    // every channel resolve reads the cached int directly.
    if ( log_severity_cvar ) {
        log_severity_cvar->onChange = Log_OnGlobalSeverityChanged;
        Log_OnGlobalSeverityChanged( log_severity_cvar );
    }

    // Touch the well-known channels owned by this subsystem so they appear
    // in `log channels` before the first message routes through them.
    (void)LOG_CH( ch_system );
    (void)LOG_CH( ch_log );

    // Apply the current log_severity floor to every already-registered
    // channel (any LOG_CH(...) call site reached during boot prior to
    // log_severity being read). Log_OnGlobalSeverityChanged above already
    // ran Log_ResolveAllChannels for any channels that existed at that
    // moment; this redundant call covers ch_system / ch_log just registered
    // above and any other late additions.
    Log_ResolveAllChannels();

    Cmd_AddCommand( "log", Log_Cmd_f );
    Cmd_SetCommandCompletionFunc( "log", Log_Cmd_Complete );
}

/* Com_FormatTimestamp → wired/core/time/time.c */

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

// -------------------------------------------------------------------------
// Com_LastError API
// -------------------------------------------------------------------------

void FORMAT_PRINTF(1, 2) QDECL Com_SetLastError( const char *fmt, ... ) {
	va_list argptr;

	va_start( argptr, fmt );
	vsnprintf( com_errorMessage, sizeof( com_errorMessage ), fmt, argptr );
	va_end( argptr );

	Cvar_Set( "com_errorMessage", com_errorMessage );
	COM_ERROR( LOG_CH(ch_system), "Error: %s\n", com_errorMessage );
}

const char *Com_GetLastError( void ) {
	return com_errorMessage;
}

qboolean Com_HasLastError( void ) {
	return com_errorMessage[0] != '\0';
}

void Com_ClearLastError( void ) {
	com_errorMessage[0] = '\0';
	Cvar_Set( "com_errorMessage", "" );
}

// -------------------------------------------------------------------------
// Com_Terminate
// -------------------------------------------------------------------------

void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Terminate( terminationReason_t reason, const char *fmt, ... ) {
	va_list		argptr;
	static int	lastErrorTime;
	static int	errorCount;
	static qboolean	calledSysError = qfalse;

#if defined(_WIN32) && defined(_DEBUG)
	if ( reason != TERM_CLIENT_LEAVE ) {
		if ( !com_noErrorInterrupt->integer ) {
			ShowWindow( g_wv.hWnd, SW_MINIMIZE );
			DebugBreak();
		}
	}
#endif

	if ( com_errorEntered ) {
		if ( !calledSysError ) {
			calledSysError = qtrue;
			Sys_Error( "recursive error after: %s", com_errorMessage );
		}
	}

	com_errorEntered = qtrue;

	Cvar_SetIntegerValue( "com_errorCode", (int)reason );

	// solid stream of TERM_CLIENT_DROP → escalate to TERM_UNRECOVERABLE
	int currentTime = Sys_Milliseconds();
	if ( currentTime - lastErrorTime < 100 ) {
		if ( ++errorCount > 3 ) {
			reason = TERM_UNRECOVERABLE;
		}
	} else {
		errorCount = 0;
	}
	lastErrorTime = currentTime;

	va_start( argptr, fmt );
	vsnprintf( com_errorMessage, sizeof( com_errorMessage ), fmt, argptr );
	va_end( argptr );

	if ( reason == TERM_CLIENT_DROP ) {
		// we can't recover from TERM_UNRECOVERABLE so there are no recipients
		// also if TERM_UNRECOVERABLE was called from S_Malloc - CopyString for a
		// long (2+ chars) text will trigger recursive error without proper shutdown
		Cvar_Set( "com_errorMessage", com_errorMessage );
	}

	// Emit log at severity appropriate to reason, before any longjmp/abort.
	{
		log_severity_t logSev;
		switch ( reason ) {
			case TERM_UNRECOVERABLE: logSev = SEV_FATAL; break;
			case TERM_CLIENT_DROP:   logSev = SEV_ERROR; break;
			case TERM_SERVER_KICK:   logSev = SEV_WARN;  break;
			case TERM_CLIENT_LEAVE:
			default:                 logSev = SEV_INFO;  break;
		}
		Com_Log( logSev, LOG_CH(ch_system), "%s", com_errorMessage );
	}

	Cbuf_Init();

	if ( reason == TERM_CLIENT_LEAVE || reason == TERM_SERVER_KICK ) {
		VM_Forced_Unload_Start();
		SV_Shutdown( "Server disconnected" );
		Log_PopRedirectSink();
#ifndef DEDICATED
		CL_Disconnect( qfalse );
		CL_FlushMemory();
#endif
		VM_Forced_Unload_Done();

		// make sure we can get at our local stuff
		FS_PureServerSetLoadedPaks( "", "" );
		com_errorEntered = qfalse;

		// Q_longjmp maps to __builtin_longjmp on MinGW which expects void**;
		// jmp_buf decays compatibly under GCC but clang-tidy's frontend rejects
		// the implicit conversion. The (void **)abortframe cast is a no-op at
		// runtime and matches the existing Q3 setjmp/longjmp pattern.
		Q_longjmp( (void **)abortframe, 1 );
	} else if ( reason == TERM_CLIENT_DROP ) {
#ifndef DEDICATED
		// Capture whether a demo was playing BEFORE CL_Disconnect clears
		// the flag so we can honor the "nextdemo" cvar below.
		const qboolean wasDemoPlaying = ( com_cl_running && com_cl_running->integer &&
			CL_DemoPlaying() );
#endif
		VM_Forced_Unload_Start();
		SV_Shutdown( va( "Server crashed: %s", com_errorMessage ) );
		Log_PopRedirectSink();
#ifndef DEDICATED
		CL_Disconnect( qfalse );
		CL_FlushMemory();
		if ( wasDemoPlaying ) {
			char next[ MAX_CVAR_VALUE_STRING ];
			Cvar_VariableStringBuffer( "nextdemo", next, sizeof( next ) );
			if ( next[0] != '\0' ) {
				Cvar_Set( "nextdemo", "" );
				Cbuf_AddText( next );
				Cbuf_AddText( "\n" );
			}
		}
#endif
		VM_Forced_Unload_Done();

		FS_PureServerSetLoadedPaks( "", "" );
		com_errorEntered = qfalse;

		// Q_longjmp maps to __builtin_longjmp on MinGW which expects void**;
		// jmp_buf decays compatibly under GCC but clang-tidy's frontend rejects
		// the implicit conversion. The (void **)abortframe cast is a no-op at
		// runtime and matches the existing Q3 setjmp/longjmp pattern.
		Q_longjmp( (void **)abortframe, 1 );
	} else {
		// TERM_UNRECOVERABLE
		VM_Forced_Unload_Start();
#ifndef DEDICATED
		CL_Shutdown( va( "Server fatal crashed: %s", com_errorMessage ), qtrue );
#endif
		SV_Shutdown( va( "Server fatal crashed: %s", com_errorMessage ) );
		Log_PopRedirectSink();
		VM_Forced_Unload_Done();
	}

	Com_Shutdown();

	calledSysError = qtrue;
	Sys_Error( "%s", com_errorMessage );
}

// -------------------------------------------------------------------------
// Com_Error_f — test command
// -------------------------------------------------------------------------

void NORETURN Com_Error_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Com_Terminate( TERM_CLIENT_DROP, "Testing drop error" );
	} else {
		Com_Terminate( TERM_UNRECOVERABLE, "Testing fatal error" );
	}
}

/* Com_RealTime, Com_RealTimeMs → wired/core/time/time.c */
