// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
log_channels.h — Hierarchical log-channel registry

Free-form, dot-separated channel names that are filtered by prefix. Adding
a channel requires no header changes — a translation unit declares its
channels at file scope with LOG_DECLARE_CHANNEL and resolves them on first
LOG_CH() use. The mutex guards registration only; the hot-path read in
Com_Logv is a single bounds check + array load + integer compare.

Severity inheritance walks up the dot hierarchy: a channel without an
explicit override searches "renderer.trail" -> "renderer" -> ""; the
first registered ancestor with an override wins. If none is found,
effectiveSev falls back to the global log_severity cvar.
===========================================================================
*/
#pragma once

// q_shared.h must be included before this header (for qboolean, sys_mutex_t).
// log.h must be included before this header (for log_severity_t / SEV_*).

#define LOG_MAX_CHANNELS       256
#define LOG_CHANNEL_NAME_MAX    64

typedef struct {
    char  name[LOG_CHANNEL_NAME_MAX];
    int   nameLen;        // strlen(name), cached for prefix matching
    int   overrideSev;    // -1 = inherit; >= SEV_TRACE = explicit; SEV_FATAL+1 = off
    int   effectiveSev;   // resolved threshold consulted by Com_Logv hot path
} logChannel_t;

extern logChannel_t log_channels[LOG_MAX_CHANNELS];
extern int          log_channelCount;

// Lazy-registers if the name is not already present. Thread-safe.
// On overflow, returns 0 (the "general" fallback) and emits a one-shot
// warning to stderr.
int  Log_GetChannel  ( const char *name );

// Pure lookup. Returns -1 if not registered. Thread-safe.
int  Log_FindChannel ( const char *name );

// Re-resolves effectiveSev for the given channel by walking dot-hierarchy
// up to the global log_severity floor. Cheap; called when overrides change.
void Log_ResolveChannel    ( int id );

// Re-resolves every registered channel. Called on log_severity cvar change
// and after batch override commands.
void Log_ResolveAllChannels( void );

// Each translation unit declares its channels at file scope. The first
// LOG_CH(var) call hits Log_GetChannel under the registry mutex; every
// subsequent call reads the cached static int directly.
//
// WASM/VM modules don't have access to the engine's channel registry —
// their Com_Log_Impl stubs ignore the channel argument and forward the
// payload through the VM-syscall bridge to the engine, which assigns the
// emitting category at the bridge site. Compile LOG_CH(...) to a constant
// 0 in those builds so wasm-ld doesn't need Log_GetChannel.
#ifdef WASM_MODULE

#define LOG_DECLARE_CHANNEL(var, name_str)                                  \
    typedef int _log_ch_unused_##var

#define LOG_CH(var)  (0)

#else

#define LOG_DECLARE_CHANNEL(var, name_str)                                  \
    static int        _log_ch_##var      = -1;                              \
    static const char *_log_ch_name_##var = (name_str)

#define LOG_CH(var)                                                         \
    ( _log_ch_##var >= 0                                                    \
        ? _log_ch_##var                                                     \
        : ( _log_ch_##var = Log_GetChannel( _log_ch_name_##var ) ) )

#endif
