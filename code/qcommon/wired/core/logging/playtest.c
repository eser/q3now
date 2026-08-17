// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
playtest.c — wired_playtest.jsonl v1 producer ring + flush

The contract this implements is documented in playtest.h; read that first.
Implementation notes that do not belong in the contract:

ONE MUTEX, NOT A LOCK-FREE RING
    log_buffer.c takes the per-thread-ring route because it sits on the
    Com_Logv hot path and must cost nothing. This ring does not: events
    are hundreds per session, not thousands per frame, so a single mutex
    buys a TOTAL order across threads (the ordering guarantee the contract
    states) for a cost that never shows up in a frame time. Trading the
    per-thread rings away is what makes `seq` meaningful.

DROP ACCOUNTING IS THE POINT
    A ring that silently overwrites is the failure mode this whole task
    exists to avoid: it produces an artefact that LOOKS complete. So the
    overwrite path is instrumented rather than implicit, and the counts
    ride out in the session_end record where a consumer cannot miss them.

FLUSH DOES THE I/O, EMIT NEVER DOES
    Emit runs under the mutex and touches only memory, so a producer on
    any thread — including one inside a frame — can call it without
    risking a filesystem stall. All writing happens in Playtest_Flush,
    called at shutdown or on demand.
===========================================================================
*/
#include "../../../q_shared.h"
#include "../../../qcommon.h"
#include "log.h"
#include "playtest.h"
#include "../../wired_build_stamp.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_DECLARE_CHANNEL( ch_playtest, "playtest" );

// -------------------------------------------------------------------------
// Event dictionary
// -------------------------------------------------------------------------

// Index-aligned with playtest_event_t. The contract test walks this table
// and the enum together, so a name added out of order fails loudly rather
// than silently mislabelling every record of that family.
static const char *s_eventNames[PT_EV_COUNT] = {
    "lifecycle.session_begin",
    "lifecycle.session_end",
    "lifecycle.map_load",
    "lifecycle.map_loaded",
    "route.progress",
    "stuck.detected",
    "stuck.recovered",
    "death.player",
    "weapon.fired",
    "ai.decision",
    "net.event",
    "perf.frame_marker"
};

const char *Playtest_EventName( playtest_event_t ev )
{
    if ( (int)ev < 0 || (int)ev >= PT_EV_COUNT )
        return "unknown.unknown";
    return s_eventNames[ev];
}

// -------------------------------------------------------------------------
// Record + ring state
// -------------------------------------------------------------------------

typedef struct {
    uint64_t          seq;
    int64_t           t_ms;                          // ms since session start
    playtest_event_t  ev;
    char              map[MAX_QPATH];                // map at emit time
    char              payload[PLAYTEST_PAYLOAD_MAX]; // JSON object body
} playtest_record_t;

static qboolean            s_initialized     = qfalse;
static qboolean            s_session_opened  = qfalse;
static sys_mutex_t         s_mutex;
static playtest_record_t  *s_ring            = NULL;
static uint32_t            s_capacity        = 0;
static uint64_t            s_write_pos       = 0;   // total accepted records
static unsigned            s_droppedOverwrite = 0;
static unsigned            s_droppedRefused   = 0;

static int64_t             s_session_start_ns = 0;
static char                s_session_id[33]   = "";
static char                s_map[MAX_QPATH]   = "";

static cvar_t             *s_enabled_cvar    = NULL;
static cvar_t             *s_capacity_cvar   = NULL;

// Envelope constants, resolved once at Init.
static char                s_platform[64]    = "";
static char                s_app[16]         = "";
static char                s_head[64]        = "";

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

static qboolean Playtest_Enabled( void )
{
    return ( s_initialized && s_enabled_cvar && s_enabled_cvar->integer );
}

// Allocates the ring on first use, at the capacity the cvars have by then
// settled on. CALLER MUST HOLD s_mutex. Returns qfalse if allocation failed,
// in which case the caller counts a refusal rather than touching a null ring.
static qboolean Playtest_EnsureRing_Locked( void )
{
    uint32_t cap;

    if ( s_ring )
        return qtrue;

    cap = (uint32_t)( s_capacity_cvar ? s_capacity_cvar->integer : 4096 );
    if ( cap < 64 )     cap = 64;
    if ( cap > 262144 ) cap = 262144;

    s_ring = (playtest_record_t *)calloc( cap, sizeof( playtest_record_t ) );
    if ( !s_ring )
        return qfalse;

    s_capacity = cap;
    return qtrue;
}

// Session id: monotonic clock + pid, hex. Unique per boot, carries no
// personal data — deliberately not a hostname or username.
static void Playtest_MintSessionId( void )
{
    int64_t ns = Sys_NanoTime();
    Com_sprintf( s_session_id, sizeof( s_session_id ), "%08x%08x",
                 (unsigned)( (uint64_t)ns >> 32 ),
                 (unsigned)( (uint64_t)ns & 0xffffffffu ) );
}

// JSON-escape a token destined for a string field. Producers pass short
// enum-like tokens, but a map name arrives from content and is not ours
// to trust, so it goes through the same escaper the file sink uses.
static void Playtest_EscapeToken( const char *in, char *out, int outsize )
{
    int n;
    if ( !in ) { out[0] = '\0'; return; }
    n = JsonEscapeBody( in, (int)strlen( in ), out, outsize );
    if ( n < 0 ) {
        /* truncated; JsonEscapeBody already NUL-terminated what fit */
    }
}

// -------------------------------------------------------------------------
// Emit
// -------------------------------------------------------------------------

// Writes one record into the ring. CALLER MUST HOLD s_mutex. Split out of
// Playtest_Emit so the lazy session_begin can be written from inside the same
// critical section as the event that triggered it — re-entering Playtest_Emit
// there would deadlock on a non-recursive mutex, and dropping the lock to
// recurse would let another thread take seq 0 out from under the opening
// record.
static void Playtest_Record_Locked( playtest_event_t ev, const char *payload_json )
{
    playtest_record_t *rec;
    uint32_t           idx;
    size_t             plen = strlen( payload_json );

    if ( s_write_pos >= s_capacity )
        s_droppedOverwrite++;

    idx = (uint32_t)( s_write_pos % s_capacity );
    rec = &s_ring[idx];

    rec->seq  = s_write_pos;
    rec->t_ms = ( Sys_NanoTime() - s_session_start_ns ) / 1000000;
    rec->ev   = ev;
    Q_strncpyz( rec->map, s_map, sizeof( rec->map ) );
    memcpy( rec->payload, payload_json, plen );
    rec->payload[plen] = '\0';

    s_write_pos++;
}

void Playtest_Emit( playtest_event_t ev, const char *payload_json )
{
    size_t plen;

    if ( !Playtest_Enabled() )
        return;

    if ( (int)ev < 0 || (int)ev >= PT_EV_COUNT ) {
        Sys_MutexLock( &s_mutex );
        s_droppedRefused++;
        Sys_MutexUnlock( &s_mutex );
        return;
    }

    if ( !payload_json )
        payload_json = "";

    plen = strlen( payload_json );
    if ( plen >= PLAYTEST_PAYLOAD_MAX ) {
        // Refuse rather than truncate: a half-written JSON object would
        // make the whole line unparseable, which is worse than a counted
        // absence. The count surfaces in session_end.
        Sys_MutexLock( &s_mutex );
        s_droppedRefused++;
        Sys_MutexUnlock( &s_mutex );
        return;
    }

    Sys_MutexLock( &s_mutex );

    if ( !Playtest_EnsureRing_Locked() ) {
        s_droppedRefused++;
        Sys_MutexUnlock( &s_mutex );
        return;
    }

    // Open the session lazily, on the first event actually accepted.
    //
    // Emitting session_begin from Playtest_Init does not work: Init runs
    // early in Com_Init, BEFORE the command line's `+set playtest_enabled 1`
    // has been applied, so the enabled-check rejected it and every artefact
    // came out with no opening record — the one record that establishes when
    // t=0 was. Doing it here means the session is opened by whichever event
    // arrives first, whenever collection actually became live.
    if ( !s_session_opened ) {
        s_session_opened = qtrue;
        Playtest_Record_Locked( PT_EV_SESSION_BEGIN, "" );
    }

    // Wrap accounting lives in Playtest_Record_Locked: once the ring is full
    // every accepted record buries exactly one older record, and counting it
    // at write time (rather than inferring it at flush) keeps the number
    // correct across multiple flushes.
    Playtest_Record_Locked( ev, payload_json );

    Sys_MutexUnlock( &s_mutex );
}

void Playtest_EmitFmt( playtest_event_t ev, const char *fmt, ... )
{
    char    buf[PLAYTEST_PAYLOAD_MAX];
    va_list ap;

    if ( !Playtest_Enabled() )
        return;

    va_start( ap, fmt );
    vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    Playtest_Emit( ev, buf );
}

void Playtest_SetMap( const char *mapname )
{
    if ( !s_initialized )
        return;

    Sys_MutexLock( &s_mutex );
    Q_strncpyz( s_map, mapname ? mapname : "", sizeof( s_map ) );
    Sys_MutexUnlock( &s_mutex );
}

// -------------------------------------------------------------------------
// Flush
// -------------------------------------------------------------------------

// Writes one envelope + payload line. Shared by the ring walk and the
// trailing session_end record so both are guaranteed identical in shape —
// a session_end that did not match the envelope would defeat the point of
// having a contract.
static void Playtest_WriteRecord( fileHandle_t fh,
                                  uint64_t seq, int64_t t_ms,
                                  playtest_event_t ev, const char *map,
                                  const char *payload )
{
    char line[PLAYTEST_PAYLOAD_MAX + 512];
    char mapEsc[MAX_QPATH * 2];
    int  len;

    Playtest_EscapeToken( map, mapEsc, sizeof( mapEsc ) );

    len = Com_sprintf( line, sizeof( line ),
        "{\"v\":%d,\"seq\":%u,\"t\":%d,\"sid\":\"%s\",\"build\":\"%s\","
        "\"head\":\"%s\",\"plat\":\"%s\",\"app\":\"%s\",\"map\":\"%s\","
        "\"ev\":\"%s\"%s%s}\n",
        PLAYTEST_SCHEMA_VERSION,
        (unsigned)seq,
        (int)t_ms,
        s_session_id,
        WIRED_BUILD_ID_STR,
        s_head,
        s_platform,
        s_app,
        mapEsc,
        Playtest_EventName( ev ),
        ( payload && payload[0] ) ? "," : "",
        ( payload && payload[0] ) ? payload : "" );

    FS_Write( line, len, fh );
}

int Playtest_Flush( const char *filename )
{
    fileHandle_t       fh;
    playtest_record_t *snapshot = NULL;
    uint32_t           count    = 0;
    uint64_t           start;
    unsigned           overwritten, refused;
    uint64_t           total;
    uint32_t           i;
    int64_t            t_now;

    if ( !s_initialized )
        return -1;

    if ( !filename || !filename[0] )
        filename = PLAYTEST_ARTEFACT_NAME;

    // Snapshot under the mutex, write outside it: a producer on another
    // thread must not block on this file I/O.
    Sys_MutexLock( &s_mutex );

    total       = s_write_pos;
    overwritten = s_droppedOverwrite;
    refused     = s_droppedRefused;
    /* s_capacity is 0 until the first accepted event allocates the ring; a
     * session that emitted nothing still flushes a session_end so the
     * artefact records that the session happened and collected nothing. */
    start       = ( s_capacity && total >= s_capacity ) ? ( total - s_capacity ) : 0;
    count       = (uint32_t)( total - start );

    if ( count > 0 && s_ring ) {
        snapshot = (playtest_record_t *)calloc( count, sizeof( playtest_record_t ) );
        if ( snapshot ) {
            for ( i = 0; i < count; i++ )
                snapshot[i] = s_ring[( start + i ) % s_capacity];
        }
    }

    t_now = ( Sys_NanoTime() - s_session_start_ns ) / 1000000;

    Sys_MutexUnlock( &s_mutex );

    if ( count > 0 && !snapshot )
        return -1;


    fh = FS_SV_FOpenFileWrite( filename );
    if ( fh == FS_INVALID_HANDLE ) {
        free( snapshot );
        return -1;
    }

    // Ring contents, already in seq order — the ring is walked from the
    // oldest surviving slot, and seq was assigned under the mutex, so no
    // sort is needed or wanted here.
    for ( i = 0; i < count; i++ ) {
        const playtest_record_t *r = &snapshot[i];
        Playtest_WriteRecord( fh, r->seq, r->t_ms, r->ev, r->map, r->payload );
    }

    // Trailing session_end: the artefact's self-report. A consumer reading
    // ONLY this file learns from this record whether it holds the whole
    // session, and if not, exactly how much is missing and where.
    {
        char payload[PLAYTEST_PAYLOAD_MAX];
        Com_sprintf( payload, sizeof( payload ),
            "\"records_written\":%u,\"seq_first\":%u,\"seq_last\":%u,"
            "\"dropped_overwritten\":%u,\"dropped_refused\":%u,"
            "\"ring_capacity\":%u,\"complete\":%s",
            (unsigned)count,
            (unsigned)start,
            (unsigned)( total > 0 ? total - 1 : 0 ),
            overwritten, refused, (unsigned)s_capacity,
            ( overwritten == 0 && refused == 0 ) ? "true" : "false" );

        Playtest_WriteRecord( fh, total, t_now, PT_EV_SESSION_END, s_map, payload );
    }

    FS_ForceFlush( fh );
    FS_FCloseFile( fh );
    free( snapshot );

    COM_INFO( LOG_CH(ch_playtest),
        "playtest: wrote %u records to '%s' (overwritten %u, refused %u)\n",
        (unsigned)count, filename, overwritten, refused );

    return (int)count;
}

// -------------------------------------------------------------------------
// Introspection
// -------------------------------------------------------------------------

unsigned Playtest_DroppedOverwritten( void ) { return s_droppedOverwrite; }
unsigned Playtest_DroppedRefused    ( void ) { return s_droppedRefused; }
unsigned Playtest_Recorded          ( void ) { return (unsigned)s_write_pos; }

// -------------------------------------------------------------------------
// Commands
// -------------------------------------------------------------------------

static void Playtest_Flush_f( void )
{
    const char *name = ( Cmd_Argc() >= 2 ) ? Cmd_Argv( 1 ) : NULL;
    int         n    = Playtest_Flush( name );

    if ( n < 0 )
        COM_WARN( LOG_CH(ch_playtest), "playtestFlush: flush failed\n" );
}

static void Playtest_Status_f( void )
{
    COM_INFO( LOG_CH(ch_playtest),
        "playtest: enabled=%d session=%s recorded=%u overwritten=%u refused=%u capacity=%u\n",
        Playtest_Enabled() ? 1 : 0, s_session_id,
        (unsigned)s_write_pos, s_droppedOverwrite, s_droppedRefused,
        (unsigned)s_capacity );
}

// -------------------------------------------------------------------------
// Init / Shutdown
// -------------------------------------------------------------------------

void Playtest_Init( const char *app_identity )
{
    if ( s_initialized )
        return;

    {
        // OPT-IN by construction: a build collects nothing until a run asks.
        // Provisional default (criterion #6 is Eser's call) — chosen
        // conservative so no artefact is produced without an explicit ask.
        static const cvarDesc_t de = CVAR_BOOL( "playtest_enabled", "0", CVAR_ARCHIVE,
            "Collect wired_playtest.jsonl playtest evidence. 0=off (default), 1=on." );
        s_enabled_cvar = Cvar_Register( &de );
    }
    {
        static const cvarDesc_t dc = CVAR_INT( "playtest_ring_capacity", "4096", CVAR_ARCHIVE,
            "Bounded playtest breadcrumb ring size in records.", 64, 262144 );
        s_capacity_cvar = Cvar_Register( &dc );
    }

    if ( !Sys_MutexInit( &s_mutex ) ) {
        COM_ERROR( LOG_CH(ch_playtest), "Playtest_Init: mutex init failed\n" );
        return;
    }

    /* The ring is NOT allocated here. Init runs early in Com_Init, before the
     * command line's `+set playtest_ring_capacity N` has been applied, so
     * sizing it now would silently ignore the operator's request and always
     * produce the default — measured: a run asking for 64 still reported
     * ring_capacity 4096. It is allocated lazily on the first accepted event
     * instead, by which point the cvars have settled. */
    s_ring             = NULL;
    s_capacity         = 0;
    s_write_pos        = 0;
    s_droppedOverwrite = 0;
    s_droppedRefused   = 0;
    s_session_start_ns = Sys_NanoTime();
    s_map[0]           = '\0';

    Playtest_MintSessionId();

    Com_sprintf( s_platform, sizeof( s_platform ), "%s-%s", OS_STRING, ARCH_STRING );

    // `app` is supplied by the CALLER rather than decided with #ifdef HEADLESS
    // here. This TU routes to qcommon_engine_shared (it is deliberately absent
    // from the QCOMMON_VARIANT_ALLOWLIST at CMakeLists.txt:107-127), so it is
    // compiled exactly ONCE and that single object is linked into BOTH wired
    // and wired-headless. A compile-time branch in this file would stamp one
    // value into both binaries — the "invisible in the other build config"
    // trap GAME-DATA.md §4 documents. common.c IS on the variant allowlist, so
    // it is compiled per-binary and its HEADLESS define is the real answer.
    Q_strncpyz( s_app, ( app_identity && app_identity[0] ) ? app_identity : "unknown",
                sizeof( s_app ) );
    /* wired_build_stamp.h always defines this (falling back to "unknown"
     * when the generated header is absent), so no #ifdef is needed here. */
    Q_strncpyz( s_head, WIRED_SOURCE_REVISION, sizeof( s_head ) );

    Cmd_AddCommand( "playtestFlush",  Playtest_Flush_f );
    Cmd_AddCommand( "playtestStatus", Playtest_Status_f );

    s_session_opened = qfalse;
    s_initialized    = qtrue;

    // session_begin is NOT emitted here. Init runs before the command line's
    // `+set playtest_enabled 1` is applied, so an emit at this point is
    // rejected by the enabled-check and the artefact loses the very record
    // that establishes when t=0 was. It is opened lazily instead, by the
    // first event that is actually accepted (see Playtest_Emit).
}

void Playtest_Shutdown( void )
{
    if ( !s_initialized )
        return;

    // Orderly shutdown flush: the artefact is written while the VFS is
    // still up. A session that ends cleanly therefore always leaves one.
    if ( Playtest_Enabled() )
        Playtest_Flush( NULL );

    Cmd_RemoveCommand( "playtestFlush" );
    Cmd_RemoveCommand( "playtestStatus" );

    Sys_MutexLock( &s_mutex );
    free( s_ring );
    s_ring     = NULL;
    s_capacity = 0;
    Sys_MutexUnlock( &s_mutex );

    Sys_MutexDestroy( &s_mutex );

    s_initialized    = qfalse;
    s_session_opened = qfalse;
}
