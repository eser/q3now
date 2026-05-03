/*
===========================================================================
log_sink_console.c — In-game console sink (V1, #ifndef DEDICATED)

Severity filter from con_severity cvar (default INFO).
Prefix format per logical line: "[SEV]  body" — always present.
With con_timestamp != 0: "HH:MM:SS [SEV]  body".

Severity bracket is outer-padded so body starts at the same column
regardless of severity name length:
  [TRACE]  [DEBUG]  [INFO]   [WARN]   [ERROR]  [FATAL]
         ^^      ^^ (extra spaces on short names)

atLineStart: prefix is emitted only when the previous emit ended with '\n'
(or on first emit). Prevents mid-line injection when Com_Log is called
in multiple pieces without a trailing newline.

Delegates to CL_ConsolePrint for ring-buffer storage and color parsing.
===========================================================================
*/
#ifndef DEDICATED

#include <assert.h>
#include "q_shared.h"
#include "qcommon.h"
#include "log.h"

// -------------------------------------------------------------------------
// Sink state
// -------------------------------------------------------------------------

// Outer-padded bracket: body always starts at column 9 (8-char bracket + space
// already included). All entries are exactly 8 chars.
static const char *ConsoleSevBracket( log_severity_t sev )
{
    switch ( sev ) {
    case SEV_TRACE: return "[TRACE] ";
    case SEV_DEBUG: return "[DEBUG] ";
    case SEV_INFO:  return "[INFO]  ";
    case SEV_WARN:  return "[WARN]  ";
    case SEV_ERROR: return "[ERROR] ";
    case SEV_FATAL: return "[FATAL] ";
    default:        return "[?????] ";
    }
}

// Header: "HH:MM:SS " (9) + "[TRACE] " (8) + NUL = 18 bytes max.
// 64 bytes gives large margin; assert guards against future growth.
#define CON_HEADER_SIZE 64

typedef struct {
    cvar_t   *severity_cvar;
    cvar_t   *timestamp_cvar;
    qboolean  atLineStart;
} console_sink_ctx_t;

static console_sink_ctx_t s_consoleSinkCtx;

static log_sink_t s_consoleSink = {
    "console",
    NULL,
    &s_consoleSinkCtx,
    SEV_TRACE,
    NULL,   // severity_cvar — set by Log_RegisterConsoleSink
    -1,     // last_cvar_mod
    0,      // active_dispatches
    NULL    // next
};

// -------------------------------------------------------------------------
// Emit
// -------------------------------------------------------------------------

static void ConsoleSink_Emit( const log_record_t *rec, void *ctx )
{
    console_sink_ctx_t *c = (console_sink_ctx_t *)ctx;

    // Emit prefix only at the start of a new line.
    if ( c->atLineStart ) {
        char        header[CON_HEADER_SIZE];
        int         hlen = 0;
        const char *bracket;
        int         blen;

        // Optional timestamp (fmt=1: "HH:MM:SS"), gated by con_timestamp.
        if ( c->timestamp_cvar && c->timestamp_cvar->integer ) {
            hlen = Com_FormatTimestamp( header, sizeof( header ) - 12, 1 );
            // Max timestamp len is 8; 12 bytes of guard leaves room for bracket+NUL.
            assert( hlen < (int)sizeof( header ) - 10 );
            header[hlen++] = ' ';
            header[hlen]   = '\0';
        }

        // Severity bracket — always present, outer-padded to 8 chars.
        bracket = ConsoleSevBracket( rec->severity );
        blen    = 8;  // ConsoleSevBracket always returns 8 chars
        assert( hlen + blen + 1 < (int)sizeof( header ) );
        memcpy( header + hlen, bracket, blen );
        hlen += blen;
        header[hlen] = '\0';

        CL_ConsolePrint( header );
    }

    CL_ConsolePrint( rec->body );

    // Update atLineStart: did this emit end on a newline?
    c->atLineStart = ( rec->body_len > 0 &&
                       rec->body[rec->body_len - 1] == '\n' );

    if ( rec->truncated ) {
        char suffix[64];
        Com_sprintf( suffix, sizeof( suffix ),
            "[truncated %u bytes]\n", (unsigned)rec->truncated_bytes );
        CL_ConsolePrint( suffix );
        c->atLineStart = qtrue;  // suffix always ends with '\n'
    }
}

// -------------------------------------------------------------------------
// Registration
// -------------------------------------------------------------------------

log_sink_t *Log_RegisterConsoleSink( void )
{
    {
        static const cvarDesc_t ds = CVAR_STRING( "log_con_severity", "", CVAR_ARCHIVE,
            "Minimum severity shown in console: [EMPTY] TRACE DEBUG INFO WARN ERROR FATAL" );
        s_consoleSinkCtx.severity_cvar = Cvar_Register( &ds );
    }
    {
        static const cvarDesc_t dt = CVAR_BOOL( "log_con_timestamp", "1", CVAR_ARCHIVE,
            "Timestamp prefix for console: 0=off, 1=on. "
            "Console shows timestamps as HH:MM:SS." );
        s_consoleSinkCtx.timestamp_cvar = Cvar_Register( &dt );
    }
    s_consoleSinkCtx.atLineStart    = qtrue;

    s_consoleSink.emit            = ConsoleSink_Emit;
    s_consoleSink.ctx             = &s_consoleSinkCtx;
    s_consoleSink.severity_cvar   = s_consoleSinkCtx.severity_cvar;
    s_consoleSink.min_severity    = Log_ParseSeverity(
                                        s_consoleSinkCtx.severity_cvar->string );

    return Log_RegisterSink( &s_consoleSink );
}

void Log_UnregisterConsoleSink( void )
{
    Log_UnregisterSink( &s_consoleSink );
}

#endif // !DEDICATED
