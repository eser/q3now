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
// Category names
// -------------------------------------------------------------------------

const char *logCategoryNames[LOG_CAT_COUNT] = {
    "general", "system", "filesystem", "network", "server",
    "client", "renderer", "sound", "botlib", "nav",
    "game", "cgame", "ui", "scripting", "mcp",
    "collision", "physics", "loading"
};

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static sys_mutex_t      s_sinks_mutex;
static log_sink_t      *s_sinks_head     = NULL;
static log_sink_t      *s_sinks_tail     = NULL;
static log_severity_t   s_min_severity   = SEV_TRACE; // only used in Log_RecomputeMinSeverity

// Global severity gate. NULL before Cvar_Init; cvar registered in Com_Init.
cvar_t *log_severity_cvar = NULL;

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

// Per-category severity cvars (registered by Log_InitCategories after Cvar_Init).
// NULL entries = not yet registered; cache falls back to global s_min_severity.
static cvar_t         *log_cat_cvars[LOG_CAT_COUNT];
// Effective severity threshold per category. Zero-initialized: 0 < SEV_TRACE=1,
// so everything passes at boot before the cvars are registered.
static log_severity_t  log_cat_cache[LOG_CAT_COUNT];

// -------------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------------

static void Log_RecomputeMinSeverity( void )
{
    if ( log_severity_cvar ) {
        s_min_severity = Log_ParseSeverity( log_severity_cvar->string );
    } else {
        s_min_severity = SEV_TRACE;
    }
}

static void Log_UpdateCategoryCache( void )
{
    int ci;
    Log_RecomputeMinSeverity();
    for ( ci = 0; ci < LOG_CAT_COUNT; ci++ ) {
        if ( log_cat_cvars[ci] && log_cat_cvars[ci]->string[0] != '\0' ) {
            log_cat_cache[ci] = Log_ParseSeverity( log_cat_cvars[ci]->string );
        } else {
            log_cat_cache[ci] = s_min_severity;
        }
    }
}

static void Log_OnAnyCategoryChanged( cvar_t *self )
{
    (void)self;
    Log_UpdateCategoryCache();
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
            Com_Log( SEV_ERROR, LOG_CAT_SYSTEM, "Log_RegisterSink: registry full (%d sinks)", LOG_MAX_SINKS );
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
        Com_Log( SEV_DEBUG, LOG_CAT_SYSTEM, "Log_PopRedirectSink: stack was empty" );
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

void Com_Logv( log_severity_t sev, logCategory_t cat, const char *fmt, va_list ap )
{
    char          buf[LOG_FORMAT_BUFFER_SIZE];
    log_record_t  rec;
    int           ret;

    ret = vsnprintf( buf, sizeof(buf), fmt, ap );

    rec.severity        = sev;
    rec.category        = cat;
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

    if ( sev < log_cat_cache[cat] && !s_redirect_top )
        return;

    Log_Dispatch( &rec );
}

void Com_Log_Impl( log_severity_t sev, logCategory_t cat, const char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    Com_Logv( sev, cat, fmt, ap );
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
// Per-category severity cvars
// -------------------------------------------------------------------------

static void Log_CatList_f( void ) {
    int i;
    const char *global_str = log_severity_cvar ? log_severity_cvar->string : "TRACE";

    Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "Per-category log severity (global: %s):\n", global_str );
    for ( i = 0; i < LOG_CAT_COUNT; i++ ) {
        qboolean has_explicit = ( log_cat_cvars[i] && log_cat_cvars[i]->string[0] != '\0' );
        if ( has_explicit ) {
            Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  %-12s %s\n",
                     logCategoryNames[i], log_cat_cvars[i]->string );
        } else {
            Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "  %-12s (inherited: %s)\n",
                     logCategoryNames[i], global_str );
        }
    }
}

static void Log_CatAll_f( void ) {
    int        i;
    const char *sev_str;
    char       cvar_name[64];

    if ( Cmd_Argc() < 2 ) {
        Com_Log( SEV_INFO, LOG_CAT_SYSTEM,
                 "Usage: log_cat_all <severity>\n"
                 "  Severities: TRACE DEBUG INFO WARN ERROR FATAL\n"
                 "  Empty string clears per-category override (inherits log_severity)\n" );
        return;
    }

    sev_str = Cmd_Argv( 1 );
    for ( i = 0; i < LOG_CAT_COUNT; i++ ) {
        Com_sprintf( cvar_name, sizeof(cvar_name), "log_cat_%s", logCategoryNames[i] );
        Cvar_Set( cvar_name, sev_str );
    }
    Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "All log categories set to: %s\n",
             *sev_str ? sev_str : "(inherit)" );
}

void Log_InitCategories( void ) {
    int  i;
    char cvar_name[64];

    for ( i = 0; i < LOG_CAT_COUNT; i++ ) {
        Com_sprintf( cvar_name, sizeof(cvar_name), "log_cat_%s", logCategoryNames[i] );
        log_cat_cvars[i] = Cvar_Get( cvar_name, "", CVAR_ARCHIVE );
        log_cat_cvars[i]->onChange = Log_OnAnyCategoryChanged;
    }

    if ( log_severity_cvar )
        log_severity_cvar->onChange = Log_OnAnyCategoryChanged;

    Log_UpdateCategoryCache();

    Cmd_AddCommand( "log_cat_list", Log_CatList_f );
    Cmd_AddCommand( "log_cat_all",  Log_CatAll_f  );
    Com_Log( SEV_INFO, LOG_CAT_SYSTEM, "Log categories initialized (%d categories)\n", LOG_CAT_COUNT );
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
	COM_ERROR( LOG_CAT_SYSTEM, "Error: %s\n", com_errorMessage );
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
		Com_Log( logSev, LOG_CAT_SYSTEM, "%s", com_errorMessage );
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

		Q_longjmp( abortframe, 1 );
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

		Q_longjmp( abortframe, 1 );
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
