// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
log_sink_file.c — JSONL file sink (V1)

Writes to qconsole.jsonl, one JSON object per line:
  {"ts":"2026-04-22T15:28:40.123","sev":"INFO","cat":"NETWORK","msg":"body text"}
  {"ts":"...","sev":"WARN","cat":"SERVER","msg":"...","truncated":true,"truncated_bytes":N}
  {"ts":"...","sev":"ERROR","cat":"SYSTEM","msg":"...","escape_truncated":true}

Timestamp is always present as a structured UTC field.

msg field: color codes (^N) stripped; all JSON special chars escaped
(", \, \n, \r, \t, control chars as \uXXXX).

Cvar surface (all log_* prefix):
  log_file_enabled   0|1, ARCHIVE         — enable/disable file logging
  log_file_mode      string enum, ARCHIVE — overwrite_buffered (default)
                                       overwrite_synced
                                       append_buffered
                                       append_synced
  log_file_severity  string, ARCHIVE      — minimum severity (default INFO)
  log_file_path      ROM                  — display-only; fs_homepath/qconsole.jsonl
  log_file_failures  ROM                  — write-failure counter

do_append and do_sync are cached at registration time; changing log_mode
at runtime has no effect until engine restart.
===========================================================================
*/
#include "q_shared.h"
#include "qcommon.h"
#include "log.h"

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

LOG_DECLARE_CHANNEL( ch_system, "system" );

static int     s_fileSinkFailures = 0;
static cvar_t *s_failuresCvar     = NULL;

typedef struct {
    fileHandle_t  fh;
    cvar_t       *severity_cvar;
    qboolean      do_sync;   // cached from log_mode at registration time
} file_sink_ctx_t;

static file_sink_ctx_t s_fileSinkCtx;

static log_sink_t s_fileSink = {
    "file",
    NULL,
    &s_fileSinkCtx,
    SEV_TRACE,
    NULL,   // severity_cvar — set by Log_RegisterFileSink
    -1,     // last_cvar_mod
    0,      // active_dispatches
    NULL    // next
};

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/* Log_SeverityName — implemented above (non-static, declared in log.h) */

// Parses log_mode string into do_append / do_sync. Returns qfalse on unknown.
static qboolean ParseLogMode( const char *mode,
                               qboolean *do_append, qboolean *do_sync )
{
    if ( Q_stricmp( mode, "overwrite_buffered" ) == 0 ) {
        *do_append = qfalse; *do_sync = qfalse; return qtrue;
    }
    if ( Q_stricmp( mode, "overwrite_synced" ) == 0 ) {
        *do_append = qfalse; *do_sync = qtrue;  return qtrue;
    }
    if ( Q_stricmp( mode, "append_buffered" ) == 0 ) {
        *do_append = qtrue;  *do_sync = qfalse; return qtrue;
    }
    if ( Q_stricmp( mode, "append_synced" ) == 0 ) {
        *do_append = qtrue;  *do_sync = qtrue;  return qtrue;
    }
    return qfalse;
}

// Hex digits for \uXXXX encoding.
static const char s_hexdig[] = "0123456789abcdef";

// JsonEscapeBody: strips ^N color codes, JSON-escapes all special chars.
// Writes into out[0..outsize-1] and NUL-terminates. Returns bytes written.
// Returns a negative value if output was truncated; absolute value is bytes
// written (not counting NUL).
const char *Log_SeverityName( log_severity_t sev )
{
    switch ( sev ) {
    case SEV_TRACE: return "TRACE";
    case SEV_DEBUG: return "DEBUG";
    case SEV_INFO:  return "INFO";
    case SEV_WARN:  return "WARN";
    case SEV_ERROR: return "ERROR";
    case SEV_FATAL: return "FATAL";
    default:        return "UNKNOWN";
    }
}

int JsonEscapeBody( const char *body, int body_len,
                    char *out, int outsize )
{
    const char   *src    = body;
    const char   *end    = body + body_len;
    char         *dst    = out;
    // Reserve 7 bytes: worst-case single-char output is \uXXXX (6) + guard.
    char         *dstend = out + outsize - 7;
    qboolean      trunc  = qfalse;

    while ( src < end ) {
        if ( dst > dstend ) {
            trunc = qtrue;
            break;
        }

        if ( Q_IsColorString( src ) ) {
            src += 2;
            continue;
        }

        {
            unsigned char c = (unsigned char)*src++;
            switch ( c ) {
            case '"':  *dst++ = '\\'; *dst++ = '"';  break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n';  break;
            case '\r': *dst++ = '\\'; *dst++ = 'r';  break;
            case '\t': *dst++ = '\\'; *dst++ = 't';  break;
            default:
                if ( c < 0x20 ) {
                    *dst++ = '\\';
                    *dst++ = 'u';
                    *dst++ = '0';
                    *dst++ = '0';
                    *dst++ = s_hexdig[( c >> 4 ) & 0xf];
                    *dst++ = s_hexdig[c & 0xf];
                } else {
                    *dst++ = (char)c;
                }
            }
        }
    }

    *dst = '\0';
    {
        int written = (int)( dst - out );
        return trunc ? -written : written;
    }
}

// -------------------------------------------------------------------------
// Emit
// -------------------------------------------------------------------------

static void FileSink_Emit( const log_record_t *rec, void *ctx )
{
    file_sink_ctx_t *c = (file_sink_ctx_t *)ctx;
    char    header[80];
    char    msg[LOG_FORMAT_BUFFER_SIZE + 128];
    char    footer[80];
    int     header_len, msg_len, footer_len;
    int     written;
    qboolean msg_trunc;

    if ( c->fh == FS_INVALID_HANDLE )
        return;

    // Build JSON header. ts fmt=3: "YYYY-MM-DDTHH:MM:SS.mmm+HH:MM" = 29 chars max.
    {
        char ts[36];
        Com_FormatTimestamp( ts, sizeof( ts ), 3 );
        header_len = Com_sprintf( header, sizeof( header ),
            "{\"ts\":\"%s\",\"sev\":\"%s\",\"cat\":\"%s\",\"msg\":\"",
            ts, Log_SeverityName( rec->severity ),
            ( rec->channel >= 0 && rec->channel < log_channelCount )
                ? log_channels[ rec->channel ].name
                : "general" );
    }

    // JSON-escape body (strips color codes).
    msg_len   = JsonEscapeBody( rec->body, (int)rec->body_len,
                                msg, sizeof( msg ) );
    msg_trunc = ( msg_len < 0 );
    if ( msg_trunc ) msg_len = -msg_len;

    // Build JSON footer. rec->truncated and msg_trunc are distinct:
    // rec->truncated  = format buffer overflowed at Com_Logv (body cut at 64KB-1).
    // msg_trunc       = JSON-escape buffer overflowed (pathological control-char body).
    // Each gets its own JSON field so consumers can distinguish the stage.
    if ( rec->truncated && msg_trunc ) {
        footer_len = Com_sprintf( footer, sizeof( footer ),
            "\",\"truncated\":true,\"truncated_bytes\":%u,"
            "\"escape_truncated\":true}\n",
            (unsigned)rec->truncated_bytes );
    } else if ( rec->truncated ) {
        footer_len = Com_sprintf( footer, sizeof( footer ),
            "\",\"truncated\":true,\"truncated_bytes\":%u}\n",
            (unsigned)rec->truncated_bytes );
    } else if ( msg_trunc ) {
        footer_len = Com_sprintf( footer, sizeof( footer ),
            "\",\"escape_truncated\":true}\n" );
    } else {
        footer_len = Com_sprintf( footer, sizeof( footer ), "\"}\n" );
    }

    // Three-part write. Footer is a single FS_Write so the closing delimiter
    // and any truncation fields are always atomic.
    written = FS_Write( header, header_len, c->fh );
    if ( written != header_len ) goto write_fail;

    written = FS_Write( msg, msg_len, c->fh );
    if ( written != msg_len ) goto write_fail;

    written = FS_Write( footer, footer_len, c->fh );
    if ( written != footer_len ) goto write_fail;

    if ( c->do_sync )
        FS_ForceFlush( c->fh );
    return;

write_fail:
    s_fileSinkFailures++;
    if ( s_failuresCvar )
        Cvar_SetValue( "log_file_failures", (float)s_fileSinkFailures );
}

// -------------------------------------------------------------------------
// Registration / unregistration
// -------------------------------------------------------------------------

log_sink_t *Log_RegisterFileSink( void )
{
    cvar_t     *enabled_cvar;
    cvar_t     *mode_cvar;
    cvar_t     *fs_homepath;
    qboolean    do_append, do_sync;
    char        logPath[MAX_OSPATH];
    const char *logName = "qconsole.jsonl";

    {
        static const cvarDesc_t ds = CVAR_STRING( "log_file_severity", "", CVAR_ARCHIVE,
            "Minimum severity written to qconsole.jsonl: [EMPTY] TRACE DEBUG INFO WARN ERROR FATAL" );
        s_fileSinkCtx.severity_cvar = Cvar_Register( &ds );
    }
    {
        static const cvarDesc_t de = CVAR_BOOL( "log_file_enabled", "1", CVAR_ARCHIVE,
            "Enable writing to qconsole.jsonl. 0=disabled, 1=enabled." );
        enabled_cvar = Cvar_Register( &de );
    }
    {
        static const cvarDesc_t dm = CVAR_STRING( "log_file_mode", "overwrite_buffered", CVAR_ARCHIVE,
            "Log file write mode: overwrite_buffered (default) | overwrite_synced | "
            "append_buffered | append_synced. Changes take effect on engine restart." );
        mode_cvar = Cvar_Register( &dm );
    }
    {
        static const cvarDesc_t df = CVAR_INT( "log_file_failures", "0", CVAR_ROM,
            "Number of write failures encountered by the file log sink.", 0, 0 );
        s_failuresCvar = Cvar_Register( &df );
    }

    if ( !enabled_cvar->integer )
        return NULL;

    if ( !ParseLogMode( mode_cvar->string, &do_append, &do_sync ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_system), "log_sink_file: unknown log_mode '%s', using overwrite_buffered\n",
                    mode_cvar->string );
        do_append = qfalse;
        do_sync   = qfalse;
    }

    if ( do_append )
        s_fileSinkCtx.fh = FS_SV_FOpenFileAppend( logName );
    else
        s_fileSinkCtx.fh = FS_SV_FOpenFileWrite( logName );

    if ( s_fileSinkCtx.fh == FS_INVALID_HANDLE )
        return NULL;

    s_fileSinkCtx.do_sync = do_sync;

    // log_path: ROM display-only introspection showing where the file lives.
    fs_homepath = Cvar_Get( "fs_homepath", "", 0 );
    Com_sprintf( logPath, sizeof( logPath ), "%s/%s",
                 fs_homepath->string, logName );
    Cvar_Get( "log_file_path", logPath, CVAR_ROM );

    s_fileSink.emit          = FileSink_Emit;
    s_fileSink.ctx           = &s_fileSinkCtx;
    s_fileSink.severity_cvar = s_fileSinkCtx.severity_cvar;
    s_fileSink.min_severity  = Log_ParseSeverity(
                                   s_fileSinkCtx.severity_cvar->string );

    return Log_RegisterSink( &s_fileSink );
}

void Log_UnregisterFileSink( void )
{
    Log_UnregisterSink( &s_fileSink );

    if ( s_fileSinkCtx.fh != FS_INVALID_HANDLE ) {
        FS_FCloseFile( s_fileSinkCtx.fh );
        s_fileSinkCtx.fh = FS_INVALID_HANDLE;
    }
}
