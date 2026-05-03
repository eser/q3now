/*
===========================================================================
log_buffer.c — Per-thread ring buffer implementation

Each thread that calls LogBuffer_Append gets a TLS log_buffer_t allocated
once (lazily) from the heap. The ring overwrites oldest entries when full.
Consecutive identical messages (same FNV-1a body hash) increment a run
counter on the existing slot instead of consuming a new one.

dumpLogBuffer collects a snapshot of all thread rings, sorts by ts_mono,
and writes one JSONL record per slot to the requested file.
===========================================================================
*/
#include "../../../q_shared.h"
#include "../../../qcommon.h"
#include "log.h"
#include "log_buffer.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -------------------------------------------------------------------------
// Global state
// -------------------------------------------------------------------------

static qboolean     s_initialized = qfalse;
static cvar_t      *s_capacity_cvar = NULL;

static sys_mutex_t  s_registry_mutex;
static log_buffer_t *s_registry[LOG_BUFFER_REGISTRY_CAPACITY];
static int          s_registry_count = 0;

// TLS pointer to this thread's ring. NULL until first Append after Init.
static QDECL_TLS log_buffer_t *tls_ring = NULL;

// -------------------------------------------------------------------------
// FNV-1a 32-bit
// -------------------------------------------------------------------------

static uint32_t Fnv1a32( const char *data, uint32_t len )
{
    uint32_t hash = 0x811c9dc5u;
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + len;
    while ( p < end ) {
        hash ^= (uint32_t)*p++;
        hash *= 0x01000193u;
    }
    return hash;
}

// -------------------------------------------------------------------------
// Registry helpers (call with registry_mutex held)
// -------------------------------------------------------------------------

static void Registry_Add( log_buffer_t *ring )
{
    if ( s_registry_count < LOG_BUFFER_REGISTRY_CAPACITY ) {
        s_registry[s_registry_count++] = ring;
    }
    // If the registry is full we lose history for this thread — acceptable.
}

static void Registry_Remove( log_buffer_t *ring )
{
    int i;
    for ( i = 0; i < s_registry_count; i++ ) {
        if ( s_registry[i] == ring ) {
            s_registry[i] = s_registry[--s_registry_count];
            return;
        }
    }
}

// -------------------------------------------------------------------------
// Ring allocation
// -------------------------------------------------------------------------

static log_buffer_t *Ring_Alloc( uint32_t capacity )
{
    log_buffer_t *ring = (log_buffer_t *)calloc( 1, sizeof( log_buffer_t ) );
    if ( !ring ) return NULL;

    ring->slots = (log_buffer_slot_t *)calloc( capacity, sizeof( log_buffer_slot_t ) );
    if ( !ring->slots ) {
        free( ring );
        return NULL;
    }

    if ( !Sys_MutexInit( &ring->mutex ) ) {
        free( ring->slots );
        free( ring );
        return NULL;
    }

    ring->capacity  = capacity;
    ring->write_pos = 0;
    ring->last_hash = 0;
    return ring;
}

static void Ring_Free( log_buffer_t *ring )
{
    Sys_MutexDestroy( &ring->mutex );
    free( ring->slots );
    free( ring );
}

// -------------------------------------------------------------------------
// Lazy per-thread ring init
// -------------------------------------------------------------------------

static log_buffer_t *GetOrCreateRing( void )
{
    uint32_t cap;

    if ( tls_ring ) return tls_ring;
    if ( !s_initialized ) return NULL;

    cap = (uint32_t)( s_capacity_cvar ? s_capacity_cvar->integer : 10000 );
    if ( cap < 16 )   cap = 16;
    if ( cap > 1000000 ) cap = 1000000;

    tls_ring = Ring_Alloc( cap );
    if ( !tls_ring ) return NULL;

    Sys_MutexLock( &s_registry_mutex );
    Registry_Add( tls_ring );
    Sys_MutexUnlock( &s_registry_mutex );

    return tls_ring;
}

// -------------------------------------------------------------------------
// Public: LogBuffer_Append
// -------------------------------------------------------------------------

void LogBuffer_Append( const log_record_t *rec )
{
    log_buffer_t       *ring;
    log_buffer_slot_t  *slot;
    uint64_t            idx;
    uint32_t            hash;
    uint32_t            copy_len;

    ring = GetOrCreateRing();
    if ( !ring ) return;

    hash = Fnv1a32( rec->body, rec->body_len );

    Sys_MutexLock( &ring->mutex );

    // Consecutive dedup: if same hash as last slot, bump times and update lastOccurance.
    if ( hash == ring->last_hash && ring->write_pos > 0 ) {
        idx  = ( ring->write_pos - 1 ) % ring->capacity;
        slot = &ring->slots[idx];
        slot->times++;
        Q_strncpyz( slot->ts_wall_last, rec->ts_wall, sizeof( slot->ts_wall_last ) );
        Sys_MutexUnlock( &ring->mutex );
        return;
    }

    idx  = ring->write_pos % ring->capacity;
    slot = &ring->slots[idx];

    slot->ts_mono   = rec->ts_mono;
    Q_strncpyz( slot->ts_wall,      rec->ts_wall, sizeof( slot->ts_wall ) );
    Q_strncpyz( slot->ts_wall_last, rec->ts_wall, sizeof( slot->ts_wall_last ) );
    slot->severity  = rec->severity;
    slot->category  = rec->category;
    slot->fnv_hash  = hash;
    slot->times     = 1;

    copy_len = rec->body_len < LOG_BUFFER_SLOT_MSG_SIZE
             ? rec->body_len
             : LOG_BUFFER_SLOT_MSG_SIZE - 1;
    memcpy( slot->body, rec->body, copy_len );
    slot->body[copy_len] = '\0';
    slot->body_len       = copy_len;

    if ( rec->body_len > LOG_BUFFER_SLOT_MSG_SIZE - 1 ) {
        slot->truncated       = qtrue;
        slot->truncated_bytes = rec->body_len - ( LOG_BUFFER_SLOT_MSG_SIZE - 1 );
    } else {
        slot->truncated       = rec->truncated;
        slot->truncated_bytes = rec->truncated_bytes;
    }

    ring->write_pos++;
    ring->last_hash = hash;

    Sys_MutexUnlock( &ring->mutex );
}

// -------------------------------------------------------------------------
// Dump: flat snapshot + qsort merge
// -------------------------------------------------------------------------

typedef struct {
    int64_t         ts_mono;
    char            ts_wall[40];
    char            ts_wall_last[40];
    log_severity_t  severity;
    logCategory_t   category;
    char            body[LOG_BUFFER_SLOT_MSG_SIZE];
    uint32_t        body_len;
    qboolean        truncated;
    uint32_t        truncated_bytes;
    uint32_t        times;
} dump_entry_t;

static int DumpEntry_Cmp( const void *a, const void *b )
{
    const dump_entry_t *ea = (const dump_entry_t *)a;
    const dump_entry_t *eb = (const dump_entry_t *)b;
    if ( ea->ts_mono < eb->ts_mono ) return -1;
    if ( ea->ts_mono > eb->ts_mono ) return  1;
    return 0;
}

static void LogBuffer_Dump( const char *filename )
{
    // Snapshot all rings while holding their individual mutexes.
    // We build a flat array from all rings, then sort by ts_mono.

    dump_entry_t   *entries  = NULL;
    int             total    = 0;
    int             capacity = 0;
    fileHandle_t    fh;
    char            escaped[LOG_BUFFER_SLOT_MSG_SIZE * 6 + 8];
    int             i, j;

    Sys_MutexLock( &s_registry_mutex );

    // Calculate upper bound for allocation.
    for ( i = 0; i < s_registry_count; i++ ) {
        capacity += (int)s_registry[i]->capacity;
    }

    if ( capacity > 0 ) {
        entries = (dump_entry_t *)calloc( capacity, sizeof( dump_entry_t ) );
    }

    if ( entries ) {
        for ( i = 0; i < s_registry_count; i++ ) {
            log_buffer_t      *ring  = s_registry[i];
            uint64_t           wp;
            uint64_t           start;

            Sys_MutexLock( &ring->mutex );
            wp = ring->write_pos;
            start = ( wp >= ring->capacity ) ? ( wp - ring->capacity ) : 0;

            for ( j = (int)start; j < (int)wp && total < capacity; j++ ) {
                const log_buffer_slot_t *s = &ring->slots[j % ring->capacity];
                dump_entry_t *e = &entries[total++];

                e->ts_mono        = s->ts_mono;
                Q_strncpyz( e->ts_wall,      s->ts_wall,      sizeof( e->ts_wall ) );
                Q_strncpyz( e->ts_wall_last, s->ts_wall_last, sizeof( e->ts_wall_last ) );
                e->severity       = s->severity;
                e->category       = s->category;
                e->body_len       = s->body_len;
                e->truncated      = s->truncated;
                e->truncated_bytes= s->truncated_bytes;
                e->times          = s->times;
                memcpy( e->body, s->body, s->body_len );
                e->body[s->body_len] = '\0';
            }
            Sys_MutexUnlock( &ring->mutex );
        }
    }

    Sys_MutexUnlock( &s_registry_mutex );

    if ( !entries || total == 0 ) {
        COM_INFO( LOG_CAT_SYSTEM, "dumpLogBuffer: nothing captured yet\n" );
        free( entries );
        return;
    }

    qsort( entries, (size_t)total, sizeof( dump_entry_t ), DumpEntry_Cmp );

    fh = FS_SV_FOpenFileWrite( filename );
    if ( fh == FS_INVALID_HANDLE ) {
        COM_ERROR( LOG_CAT_SYSTEM, "dumpLogBuffer: cannot open '%s' for write\n", filename );
        free( entries );
        return;
    }

    for ( i = 0; i < total; i++ ) {
        dump_entry_t *e = &entries[i];
        char     header[128];
        char     footer[128];
        int      header_len, msg_len, footer_len;
        uint32_t trim_len;

        // Trim trailing newline — qconsole.jsonl never embeds \n in msg.
        trim_len = e->body_len;
        while ( trim_len > 0 &&
                ( e->body[trim_len - 1] == '\n' || e->body[trim_len - 1] == '\r' ) )
            trim_len--;

        header_len = Com_sprintf( header, sizeof( header ),
            "{\"ts\":\"%s\",\"sev\":\"%s\",\"cat\":\"%s\",\"msg\":\"",
            e->ts_wall,
            Log_SeverityName( e->severity ),
            logCategoryNames[ e->category ] );

        msg_len = JsonEscapeBody( e->body, (int)trim_len,
                                  escaped, (int)sizeof( escaped ) );
        if ( msg_len < 0 ) msg_len = -msg_len;

        if ( e->truncated ) {
            footer_len = Com_sprintf( footer, sizeof( footer ),
                "\",\"times\":%u,\"lastOccurance\":\"%s\","
                "\"truncated\":true,\"truncated_bytes\":%u}\n",
                e->times, e->ts_wall_last, e->truncated_bytes );
        } else {
            footer_len = Com_sprintf( footer, sizeof( footer ),
                "\",\"times\":%u,\"lastOccurance\":\"%s\"}\n",
                e->times, e->ts_wall_last );
        }

        FS_Write( header,  header_len, fh );
        FS_Write( escaped, msg_len,    fh );
        FS_Write( footer,  footer_len, fh );
    }

    FS_FCloseFile( fh );
    COM_INFO( LOG_CAT_SYSTEM, "dumpLogBuffer: wrote %d entries to '%s'\n", total, filename );

    free( entries );
}

// -------------------------------------------------------------------------
// Console command handler
// -------------------------------------------------------------------------

void LogBuffer_DumpCmd_f( void )
{
    const char *filename;

    if ( Cmd_Argc() < 2 ) {
        filename = "logbuffer.jsonl";
    } else {
        filename = Cmd_Argv( 1 );
    }

    LogBuffer_Dump( filename );
}

// -------------------------------------------------------------------------
// Init / Shutdown
// -------------------------------------------------------------------------

void LogBuffer_Init( void )
{
    static const cvarDesc_t cap_desc = CVAR_INT(
        "log_buffer_capacity", "10000", CVAR_ARCHIVE, NULL, 16, 1000000 );

    if ( s_initialized ) return;

    if ( !Sys_MutexInit( &s_registry_mutex ) ) {
        COM_ERROR( LOG_CAT_SYSTEM, "LogBuffer_Init: failed to create registry mutex\n" );
        return;
    }

    s_capacity_cvar = Cvar_Register( &cap_desc );
    Cmd_AddCommand( "dumpLogBuffer", LogBuffer_DumpCmd_f );

    s_initialized = qtrue;
}

void LogBuffer_Shutdown( void )
{
    int i;

    if ( !s_initialized ) return;

    Cmd_RemoveCommand( "dumpLogBuffer" );

    Sys_MutexLock( &s_registry_mutex );
    for ( i = 0; i < s_registry_count; i++ ) {
        Ring_Free( s_registry[i] );
        s_registry[i] = NULL;
    }
    s_registry_count = 0;
    Sys_MutexUnlock( &s_registry_mutex );

    Sys_MutexDestroy( &s_registry_mutex );

    tls_ring      = NULL;
    s_initialized = qfalse;
}
