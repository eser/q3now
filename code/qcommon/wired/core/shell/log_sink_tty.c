/*
===========================================================================
log_sink_tty.c — TTY / terminal sink (V1)

Severity filter from con_severity cvar (shared with console sink).
Prefix format per logical line: "[INFO ] body" — always present.
With con_timestamp != 0: "HH:MM:SS.mmm+TZ [INFO ] body".

Severity bracket is inner-padded to 7 chars: [TRACE] [DEBUG] [INFO ]
[WARN ] [ERROR] [FATAL]. One space after the closing bracket.

atLineStart: prefix is emitted only when the previous emit ended with '\n'
(or on first emit). Prevents mid-line injection when Com_Printf is called
in multiple pieces without a trailing newline.

V1 invariant: Com_Log is called from the main thread only. No interleave
risk. V2 migration note: cross-thread sinks must revisit atomicity.

Delegates to Sys_Print (unix: ANSI colorize + write(2);
win32: Conbuf_AppendText).
===========================================================================
*/
#include <assert.h>
#include "q_shared.h"
#include "qcommon.h"
#include "log.h"

// -------------------------------------------------------------------------
// Sink state
// -------------------------------------------------------------------------

// Header: "HH:MM:SS.mmm+HH:MM " (19) + "[INFO ] " (8) + NUL = 28 bytes max.
// 128 bytes gives large margin.
#define TTY_HEADER_SIZE 128

typedef struct {
    cvar_t   *severity_cvar;
    cvar_t   *timestamp_cvar;
    qboolean  atLineStart;
} tty_sink_ctx_t;

static tty_sink_ctx_t s_ttySinkCtx;

static log_sink_t s_ttySink = {
    "tty",
    NULL,
    &s_ttySinkCtx,
    SEV_INFO,
    NULL,
    0,
    NULL
};

// -------------------------------------------------------------------------
// Emit
// -------------------------------------------------------------------------

static void TtySink_Emit( const log_record_t *rec, void *ctx )
{
    tty_sink_ctx_t *c = (tty_sink_ctx_t *)ctx;
    log_severity_t  min;

    min = c->severity_cvar
        ? Log_ParseSeverity( c->severity_cvar->string )
        : SEV_INFO;
    if ( rec->severity < min )
        return;

    // Emit prefix only at the start of a new line.
    if ( c->atLineStart ) {
        char        header[TTY_HEADER_SIZE];
        int         hlen = 0;
        const char *bracket;
        int         blen;

        // Optional timestamp (fmt=2: "HH:MM:SS.mmm+HH:MM"), gated by con_timestamp.
        if ( c->timestamp_cvar && c->timestamp_cvar->integer ) {
            hlen = Log_FormatTimestamp( rec->timestamp_ns, header,
                                        sizeof( header ) - 16, 2 );
            // Max timestamp len is 18; 16-byte guard leaves room for bracket+NUL.
            assert( hlen < (int)sizeof( header ) - 14 );
            header[hlen++] = ' ';
            header[hlen]   = '\0';
        }

        // Severity bracket — always present, inner-padded, 7 chars.
        bracket = Log_SeverityBracket( rec->severity );
        blen    = 7;  // Log_SeverityBracket always returns 7 chars
        assert( hlen + blen + 2 < (int)sizeof( header ) );
        memcpy( header + hlen, bracket, blen );
        hlen += blen;
        header[hlen++] = ' ';  // one space after closing bracket
        header[hlen]   = '\0';

        Sys_Print( header );
    }

    Sys_Print( rec->body );

    // Update atLineStart: did this emit end on a newline?
    c->atLineStart = ( rec->body_len > 0 &&
                       rec->body[rec->body_len - 1] == '\n' );
}

// -------------------------------------------------------------------------
// Registration
// -------------------------------------------------------------------------

log_sink_t *Log_RegisterTtySink( void )
{
    // con_severity and con_timestamp are shared with the console sink.
    // Cvar_Get is idempotent. Descriptions are set here so dedicated builds
    // (which skip the console sink) still get help text; in client builds the
    // console sink sets them first and these calls are harmless repeats.
    s_ttySinkCtx.severity_cvar  = Cvar_Get( "con_severity",  "INFO", CVAR_ARCHIVE );
    s_ttySinkCtx.timestamp_cvar = Cvar_Get( "con_timestamp", "1",    CVAR_ARCHIVE );
    s_ttySinkCtx.atLineStart    = qtrue;

    Cvar_SetDescription( s_ttySinkCtx.severity_cvar,
        "Minimum severity shown in console and TTY: TRACE DEBUG INFO WARN ERROR FATAL" );
    Cvar_SetDescription( s_ttySinkCtx.timestamp_cvar,
        "Timestamp prefix for console and TTY: 0=off, 1=on. "
        "Console shows HH:MM:SS; TTY shows HH:MM:SS.mmm+TZ." );

    s_ttySink.emit            = TtySink_Emit;
    s_ttySink.ctx             = &s_ttySinkCtx;
    s_ttySink.severity_cvar   = s_ttySinkCtx.severity_cvar;
    s_ttySink.min_severity    = Log_ParseSeverity(
                                    s_ttySinkCtx.severity_cvar->string );

    return Log_RegisterSink( &s_ttySink );
}

void Log_UnregisterTtySink( void )
{
    Log_UnregisterSink( &s_ttySink );
}
