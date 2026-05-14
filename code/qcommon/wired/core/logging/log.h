// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
log.h — Unified print pipeline V1

Severity-aware dispatch with a dynamic sink registry and LIFO redirect
stack. Every print goes through one format → short-circuit → dispatch
pipeline with one 64 KB stack buffer, one timestamp source, and one
color-escape policy per sink.

OTel SeverityNumber mapping:
    TRACE=1, DEBUG=5, INFO=9, WARN=13, ERROR=17, FATAL=21
===========================================================================
*/
#pragma once

// q_shared.h must be included by the translation unit before this header.
// Q3 convention: sub-headers rely on the caller having established the base
// types (qboolean, cvar_t, int64_t, etc.) rather than re-including from
// a path that may not resolve in all build contexts.

// -------------------------------------------------------------------------
// Severity
// -------------------------------------------------------------------------

typedef enum {
    SEV_TRACE = 1,
    SEV_DEBUG = 5,
    SEV_INFO  = 9,
    SEV_WARN  = 13,
    SEV_ERROR = 17,
    SEV_FATAL = 21
} log_severity_t;

/*
 * LOG_COMPILE_SEVERITY: messages strictly below this level are compiled out.
 * In release builds (NDEBUG defined), TRACE calls become ((void)0) — no
 * argument evaluation, no call, no overhead.  Dev builds keep everything
 * so that runtime cvar filtering controls visibility.
 *
 * Can be overridden per-TU before including this header:
 *   #define LOG_COMPILE_SEVERITY SEV_INFO
 *   #include "qcommon.h"
 */
#ifndef LOG_COMPILE_SEVERITY
  #ifdef NDEBUG
    #define LOG_COMPILE_SEVERITY SEV_DEBUG
  #else
    #define LOG_COMPILE_SEVERITY SEV_TRACE
  #endif
#endif

// -------------------------------------------------------------------------
// Record
// -------------------------------------------------------------------------

// Forward declaration for origin_sink field.
typedef struct log_sink_s log_sink_t;

typedef struct {
    log_severity_t   severity;
    int              channel;        // index into log_channels[] (Phase 2+)
    const char      *body;           // NOT null-terminated past body_len
    uint32_t         body_len;       // use this, never strlen(body)
    qboolean         truncated;      // qtrue if message exceeded 64KB-1
    uint32_t         truncated_bytes;// bytes that were dropped
    log_sink_t      *origin_sink;    // NULL for normal callers; set by dispatch
                                     // to skip self-dispatch in a sink's emit
    int64_t          ts_mono;        // Sys_NanoTime() at emit time
    char             ts_wall[40];    // Com_FormatTimestamp output (fmt=3)
} log_record_t;

// -------------------------------------------------------------------------
// Sink
// -------------------------------------------------------------------------

// Emit contract:
//   - MUST return in <100µs
//   - MUST NOT retain rec->body pointer past return
//   - MUST use rec->body_len, never strlen(rec->body)
//   - MUST NOT longjmp() out (would leak active_dispatches, hang unregister)
//   - MUST NOT be called from signal handlers (not async-signal-safe)
typedef void (*log_sink_fn)(const log_record_t *rec, void *ctx);

#define LOG_MAX_SINKS 32

struct log_sink_s {
    const char     *name;
    log_sink_fn     emit;
    void           *ctx;             // per-sink context (sink resolves its cvars here)
    log_severity_t  min_severity;    // cached threshold, refreshed by last_cvar_mod check
    cvar_t         *severity_cvar;   // pointer cached once at Log_RegisterSink time
    int             last_cvar_mod;   // cached cvar->modificationCount, -1 = uninitialized
    int             active_dispatches; // drain counter for Log_UnregisterSink
    log_sink_t     *next;            // intrusive FIFO list (tail-appended)
};

// -------------------------------------------------------------------------
// Redirect stack
// -------------------------------------------------------------------------

// Log_PushRedirectSink / Log_PopRedirectSink are MAIN-THREAD-ONLY.
// s_redirect_top is read pre-mutex; a cross-thread call racing a Pop would
// access a freed stack frame. V1 call sites are all main-thread command
// handlers — matches legacy redirect callers (rcon, status).
#define LOG_REDIRECT_EXCLUSIVE  0x0001  // suppress normal sinks while active;
                                         // SEV_FATAL always bypasses regardless

// Caller allocates this on their C stack and keeps it alive until the
// matching Log_PopRedirectSink call. Zero-heap-allocation on print path.
typedef struct log_redirect_frame_s {
    char                        *buf;
    int                          buf_size;
    int                          used;
    void                       (*flush)(void *ctx);
    void                        *ctx;
    int                          flags;
    struct log_redirect_frame_s *prev;
} log_redirect_frame_t;

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

// FORMAT_PRINTF is declared in q_shared.h:122 — compile-time CWE-134 defence.
// MUST be on the declaration so every translation unit gets -Wformat coverage.
//
// Com_Log_Impl is the real function. Com_Log is a macro that compiles out
// calls below LOG_COMPILE_SEVERITY entirely, preventing argument evaluation.
// Code that needs a raw function pointer (e.g. rimp.Log = Com_Log_Impl) must
// reference Com_Log_Impl directly.
//
// The second parameter is a channel id obtained from LOG_CH(<var>); the
// declaration of <var> uses LOG_DECLARE_CHANNEL(<var>, "<name>") at file
// scope and resolves through the channel registry on first use.
void FORMAT_PRINTF(3, 4) Com_Log_Impl ( log_severity_t sev, int channel, const char *fmt, ... );
void                     Com_Logv     ( log_severity_t sev, int channel, const char *fmt, va_list ap );

#define Com_Log( severity, channel, ... ) \
    do { \
        if ( (severity) >= LOG_COMPILE_SEVERITY ) { \
            Com_Log_Impl( (severity), (channel), __VA_ARGS__ ); \
        } \
    } while (0)

// Test command handler registered as "/error" in Com_Init.
void NORETURN Com_Error_f( void );

// Log_RegisterSink returns NULL + SEV_ERROR log when registry is full.
log_sink_t *Log_RegisterSink  ( log_sink_t *sink );
void        Log_UnregisterSink ( log_sink_t *sink );

// Fallback sink: writes raw to fd 2, no timestamps, no cvars.
// Must be the very first call in Com_Init (before Cvar_Init).
// Returns qfalse if Sys_MutexInit fails; caller must Com_Error ERR_FATAL.
qboolean    Log_RegisterFallbackStderrSink  ( void );
void        Log_UnregisterFallbackStderrSink( void );

// Redirect stack. Caller-owned frames, main thread only.
void     Log_PushRedirectSink( log_redirect_frame_t *frame,
                                char *buf, int buf_size,
                                void (*flush)(void *ctx), void *ctx,
                                int flags );
qboolean Log_PopRedirectSink ( void );   // qtrue=popped, qfalse=was empty

// -------------------------------------------------------------------------
// Shared helpers (implemented in log.c, used by all built-in sinks)
// -------------------------------------------------------------------------

// Global severity gate cvar. Registered by Com_Init after Cvar_Init.
// NULL during the pre-Cvar_Init fallback window (early boot drops nothing).
extern cvar_t *log_severity_cvar;

// Cached parsed value of log_severity. Refreshed by the cvar's onChange
// callback (Log_OnGlobalSeverityChanged) whenever the string changes. The
// channel resolver reads this directly instead of re-parsing the cvar's
// string on every call. Default SEV_INFO is the safe pre-Cvar_Init floor.
extern int log_global_severity;

// Log_InitChannels: register the `log` console command + completion, wire the
// log_severity onChange callback to recompute every channel's effectiveSev,
// and pre-register the "log" channel itself. Call after Cvar_Init and after
// log_severity_cvar is registered.
void Log_InitChannels( void );

// Log_ParseSeverity: case-insensitive [EMPTY]/"TRACE"/"DEBUG"/"INFO"/"WARN"/"ERROR"/"FATAL".
// Returns SEV_INFO on unrecognised input.
log_severity_t Log_ParseSeverity( const char *name );

/* Com_FormatTimestamp declared in wired/core/time/time.h (via qcommon.h). */

// Log_SeverityBracket: returns static literal e.g. "[INFO]", "[WARN]".
const char *Log_SeverityBracket( log_severity_t sev );

// Log_SeverityName: bare name without brackets, e.g. "INFO", "WARN".
// Implemented in log_sink_file.c (shared by file sink and log buffer).
const char *Log_SeverityName( log_severity_t sev );

// JsonEscapeBody: strips ^N color codes, JSON-escapes special chars.
// Implemented in log_sink_file.c (shared by file sink and log buffer).
int JsonEscapeBody( const char *body, int body_len, char *out, int outsize );

// -------------------------------------------------------------------------
// Built-in sink registration (implemented in log_sink_*.c)
// -------------------------------------------------------------------------

// Console sink — in-game ring buffer. Compiled out in dedicated builds.
#ifndef DEDICATED
log_sink_t *Log_RegisterConsoleSink  ( void );
void        Log_UnregisterConsoleSink( void );
#endif

// TTY sink — terminal output via Sys_Print.
log_sink_t *Log_RegisterTtySink      ( void );
void        Log_UnregisterTtySink    ( void );

// File sink — log_file_path cvar.
log_sink_t *Log_RegisterFileSink     ( void );
void        Log_UnregisterFileSink   ( void );

// -------------------------------------------------------------------------
// Convenience macros
// -------------------------------------------------------------------------

#define COM_TRACE( channel, ... )  Com_Log( SEV_TRACE,  (channel), __VA_ARGS__ )
#define COM_DEBUG( channel, ... )  Com_Log( SEV_DEBUG,  (channel), __VA_ARGS__ )
#define COM_INFO( channel, ... )   Com_Log( SEV_INFO,   (channel), __VA_ARGS__ )
#define COM_WARN( channel, ... )   Com_Log( SEV_WARN,   (channel), __VA_ARGS__ )
#define COM_ERROR( channel, ... )  Com_Log( SEV_ERROR,  (channel), __VA_ARGS__ )
#define COM_FATAL( channel, ... )  Com_Log( SEV_FATAL,  (channel), __VA_ARGS__ )

// -------------------------------------------------------------------------
// Channel registry — pull in the public API for hierarchical channel-based
// filtering. Every TU that emits a Com_Log declares its channels at file
// scope via LOG_DECLARE_CHANNEL(<var>, "<name>") and uses LOG_CH(<var>) at
// the call site to resolve the channel id.
// -------------------------------------------------------------------------
#include "log_channels.h"

// -------------------------------------------------------------------------
// Format buffer size (64 KB)
// -------------------------------------------------------------------------
#define LOG_FORMAT_BUFFER_SIZE  65536
