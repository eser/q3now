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

// -------------------------------------------------------------------------
// Record
// -------------------------------------------------------------------------

// Forward declaration for origin_sink field.
typedef struct log_sink_s log_sink_t;

typedef struct {
    int64_t          timestamp_ns;   // from Sys_NanoTime()
    log_severity_t   severity;
    const char      *body;           // NOT null-terminated past body_len
    uint32_t         body_len;       // use this, never strlen(body)
    qboolean         truncated;      // qtrue if message exceeded 64KB-1
    uint32_t         truncated_bytes;// bytes that were dropped
    log_sink_t      *origin_sink;    // NULL for normal callers; set by dispatch
                                     // to skip self-dispatch in a sink's emit
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
    log_severity_t  min_severity;    // cached from severity_cvar at registration
    cvar_t         *severity_cvar;   // pointer cached once at Log_RegisterSink time
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
void FORMAT_PRINTF(2, 3) Com_Log ( log_severity_t sev, const char *fmt, ... );
void                     Com_Logv( log_severity_t sev, const char *fmt, va_list ap );

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

// Log_ParseSeverity: case-insensitive "TRACE"/"DEBUG"/"INFO"/"WARN"/"ERROR"/"FATAL".
// Returns SEV_INFO on unrecognised input.
log_severity_t Log_ParseSeverity( const char *name );

// Log_FormatTimestamp: formats local wall-clock time into buf[0..buflen-1].
//   fmt 0  off (writes nothing, returns 0)
//   fmt 1  "HH:MM:SS"                       — short, for in-game console
//   fmt 2  "HH:MM:SS.mmm+HH:MM"             — ms + TZ offset, for TTY
//   fmt 3  "YYYY-MM-DDTHH:MM:SS.mmm+HH:MM"  — full ISO 8601, for file
// Returns bytes written (no NUL counted). buf is NUL-terminated on success.
int Log_FormatTimestamp( int64_t ns, char *buf, int buflen, int fmt );

// Log_SeverityBracket: returns static literal e.g. "[INFO]", "[WARN]".
const char *Log_SeverityBracket( log_severity_t sev );

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

// File sink — qconsole.log. Returns NULL if com_logfile == 0.
log_sink_t *Log_RegisterFileSink     ( void );
void        Log_UnregisterFileSink   ( void );

// -------------------------------------------------------------------------
// Convenience macros
// -------------------------------------------------------------------------

#define COM_TRACE(...)  Com_Log(SEV_TRACE,  __VA_ARGS__)
#define COM_DEBUG(...)  Com_Log(SEV_DEBUG,  __VA_ARGS__)
#define COM_INFO(...)   Com_Log(SEV_INFO,   __VA_ARGS__)
#define COM_WARN(...)   Com_Log(SEV_WARN,   __VA_ARGS__)
#define COM_ERROR(...)  Com_Log(SEV_ERROR,  __VA_ARGS__)
#define COM_FATAL(...)  Com_Log(SEV_FATAL,  __VA_ARGS__)

// -------------------------------------------------------------------------
// Format buffer size (64 KB)
// -------------------------------------------------------------------------
#define LOG_FORMAT_BUFFER_SIZE  65536
