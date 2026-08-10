// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
log_sink_console.c — In-game console sink (V1, #ifndef HEADLESS)

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
#ifndef HEADLESS

#include <assert.h>
#include "q_shared.h"
#include "qcommon.h"
#include "log.h"
#include "../console/con_public.h"   /* Con_PrintSeverity */

// -------------------------------------------------------------------------
// Sink state
// -------------------------------------------------------------------------

// TURN 3 V-20 (severity-as-data): the console sink no longer builds a
// "[SEV] " text prefix or injects a color escape into the buffer. It tags the
// line with rec->severity via Con_PrintSeverity; the in-game console colors
// each line from that severity at draw time (elements/console.c). The
// ConsoleSevBracket helper and the atLineStart header-building block are gone.
// Optional HH:MM:SS timestamping is still owned by the console UI cvar
// (con_timestamp) — this sink no longer prepends a timestamp either; the
// console renders its own clock/timestamp presentation.

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

    // Severity-as-data: tag the line with rec->severity and let the console
    // color it at draw time. No text bracket, no color escape, no timestamp
    // are injected into the buffer here (severity-as-data replaces the
    // id-model text-prefix approach). The atLineStart bookkeeping is kept so
    // the suffix logic below still tracks whether a newline closed the line.
    Con_PrintSeverity( rec->severity, rec->body );

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

#endif // !HEADLESS
