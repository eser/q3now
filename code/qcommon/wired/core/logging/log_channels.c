// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
log_channels.c — Hierarchical log-channel registry (Phase 1)

Storage + linear-scan lookup + lazy registration + dot-hierarchy
inheritance resolution. See log_channels.h for the contract.

Threading model:
  - Registration (Log_GetChannel, Log_FindChannel, Log_ResolveChannel,
    Log_ResolveAllChannels) acquires s_channels_mutex.
  - The eventual hot path in Com_Logv (Phase 2) does a lock-free
    aligned-int read of log_channels[id].effectiveSev. Aligned int
    reads/writes are atomic on every platform we target; readers may
    see either old or new value during a resolve transition, both are
    valid thresholds.

Lazy-init assumption: the first Log_GetChannel call happens on the main
thread before any worker threads are spawned. Engine init is serial up
to that point, so no second thread can race the s_channels_inited flag
ahead of mutex creation.
===========================================================================
*/
#include "q_shared.h"
#include "qcommon.h"
#include "log.h"
#include "log_channels.h"

#include <stdio.h>    // fprintf for overflow / mutex-init warnings
#include <string.h>   // strcmp, strrchr

logChannel_t log_channels[LOG_MAX_CHANNELS];
int          log_channelCount = 0;

static sys_mutex_t s_channels_mutex;
static qboolean    s_channels_inited = qfalse;
static qboolean    s_overflowWarned  = qfalse;

// Linear scan with no locking. Caller holds s_channels_mutex (or is still
// in single-threaded boot before lazy init has handed out the lock).
static int Log_FindChannel_NoLock( const char *name )
{
    int i;
    for ( i = 0; i < log_channelCount; i++ ) {
        if ( strcmp( log_channels[i].name, name ) == 0 )
            return i;
    }
    return -1;
}

// Resolve effectiveSev for one channel by walking dot-hierarchy up to the
// global floor. Caller holds s_channels_mutex. Strictly read-only on every
// other channel's overrideSev/name (fields never mutate after registration);
// only writes the target channel's effectiveSev (aligned-int atomic).
static void Log_ResolveChannel_NoLock( int id )
{
    logChannel_t *ch;
    char          parent[LOG_CHANNEL_NAME_MAX];
    char         *dot;
    int           parentId;

    if ( id < 0 || id >= log_channelCount )
        return;

    ch = &log_channels[id];

    if ( ch->overrideSev >= 0 ) {
        ch->effectiveSev = ch->overrideSev;
        return;
    }

    // Walk up: "renderer.trail" -> "renderer" -> "" (no dot left = bail to global)
    Q_strncpyz( parent, ch->name, sizeof( parent ) );
    for ( ;; ) {
        dot = strrchr( parent, '.' );
        if ( !dot )
            break;
        *dot = '\0';
        parentId = Log_FindChannel_NoLock( parent );
        if ( parentId >= 0 && log_channels[parentId].overrideSev >= 0 ) {
            ch->effectiveSev = log_channels[parentId].overrideSev;
            return;
        }
    }

    // No registered ancestor with an override — inherit the global floor.
    // log_global_severity is the parse-once-on-change cache maintained by
    // Log_OnGlobalSeverityChanged in log.c. It defaults to SEV_INFO before
    // log_severity_cvar exists, then gets refreshed when Log_InitChannels
    // wires the onChange callback (and manually fires it once to capture
    // the initial cvar value). Reading the cached int here avoids re-parsing
    // log_severity_cvar->string on every resolve and removes any chance of
    // a stale/inconsistent read between the cvar string and the resolved
    // threshold seen by the hot path.
    ch->effectiveSev = log_global_severity;
}

static void Log_LazyInit( void )
{
    if ( s_channels_inited )
        return;
    s_channels_inited = qtrue;

    if ( Sys_MutexInit( &s_channels_mutex ) == qfalse ) {
        // No good recovery — registration races would corrupt the registry.
        // Note the failure on stderr; subsequent registrations run unlocked
        // (still safe in the single-threaded boot phase, racy thereafter).
        fprintf( stderr, "log_channels: Sys_MutexInit failed; registry "
                 "running without thread safety\n" );
    }

    // Pre-register the "general" fallback at id 0. Anyone who passes a
    // missing/empty name lands here, and the overflow path also routes
    // here. effectiveSev is seeded from log_global_severity (which defaults
    // to SEV_INFO via its static initializer in log.c, so pre-Cvar_Init
    // resolution gets a safe floor that suppresses TRACE/DEBUG).
    // Log_InitChannels later refreshes log_global_severity from the cvar
    // and re-resolves every registered slot, so this seed is just a holding
    // value for the brief boot window before that runs.
    Q_strncpyz( log_channels[0].name, "general", sizeof( log_channels[0].name ) );
    log_channels[0].nameLen     = (int) strlen( log_channels[0].name );
    log_channels[0].overrideSev = -1;
    log_channelCount            = 1;
    Log_ResolveChannel_NoLock( 0 );
}

int Log_FindChannel( const char *name )
{
    int id;
    if ( !name || !*name )
        return -1;
    Log_LazyInit();
    Sys_MutexLock( &s_channels_mutex );
    id = Log_FindChannel_NoLock( name );
    Sys_MutexUnlock( &s_channels_mutex );
    return id;
}

int Log_GetChannel( const char *name )
{
    int           id;
    logChannel_t *ch;

    if ( !name || !*name )
        return 0;  // route to the "general" fallback

    Log_LazyInit();
    Sys_MutexLock( &s_channels_mutex );

    id = Log_FindChannel_NoLock( name );
    if ( id >= 0 ) {
        Sys_MutexUnlock( &s_channels_mutex );
        return id;
    }

    if ( log_channelCount >= LOG_MAX_CHANNELS ) {
        if ( !s_overflowWarned ) {
            s_overflowWarned = qtrue;
            fprintf( stderr,
                "log_channels: registry full (%d channels); routing '%s' "
                "and any future channels to 'general' (id 0)\n",
                LOG_MAX_CHANNELS, name );
        }
        Sys_MutexUnlock( &s_channels_mutex );
        return 0;
    }

    // Populate the slot FIRST, then publish by incrementing the counter.
    // Hot-path readers (Com_Logv in log.c) sample log_channelCount lock-free;
    // readers must not see a slot index that points at half-written state.
    //
    // effectiveSev MUST be written before the counter increment. Calling
    // Log_ResolveChannel_NoLock here would early-return on its bounds guard
    // (id == log_channelCount at this moment, so id >= log_channelCount is
    // true) and leave effectiveSev at its BSS-zero value — opening the gate
    // to every severity. The fix is a direct inline write of the global
    // floor before publishing; once the counter is incremented we re-resolve
    // properly so the dot-hierarchy parent walk gets a chance to override.
    id = log_channelCount;
    ch = &log_channels[id];
    Q_strncpyz( ch->name, name, sizeof( ch->name ) );
    ch->nameLen     = (int) strlen( ch->name );
    ch->overrideSev = -1;
    ch->effectiveSev = log_global_severity;
    log_channelCount = id + 1;
    Log_ResolveChannel_NoLock( id );

    Sys_MutexUnlock( &s_channels_mutex );
    return id;
}

void Log_ResolveChannel( int id )
{
    Log_LazyInit();
    Sys_MutexLock( &s_channels_mutex );
    Log_ResolveChannel_NoLock( id );
    Sys_MutexUnlock( &s_channels_mutex );
}

void Log_ResolveAllChannels( void )
{
    int i;
    Log_LazyInit();
    Sys_MutexLock( &s_channels_mutex );
    for ( i = 0; i < log_channelCount; i++ ) {
        Log_ResolveChannel_NoLock( i );
    }
    Sys_MutexUnlock( &s_channels_mutex );
}
