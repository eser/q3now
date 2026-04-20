/*
===========================================================================
arena.c — Per-subsystem arena allocators

Bump-pointer allocator backed by one malloc()'d block.  Zero fragmentation.
No per-object free — reset the whole arena or destroy it.

See arena.h for the public API contract.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "arena.h"

#define ARENA_MAX_REGISTERED 32
#define ARENA_GUARD_MAGIC    0xA2A2A2A2U

struct arena_s {
    char   name[64];
    byte  *base;         /* start of OS block */
    byte  *ptr;          /* next free byte (bump pointer) */
    byte  *end;          /* one past last byte of block */
    size_t peak;         /* high-water mark */
    uint32_t magic;
#ifdef HUNK_DEBUG
    qboolean locked;
#endif
};

/* Global registry for /memstats */
static arena_t *s_registry[ARENA_MAX_REGISTERED];
static int      s_registryCount;


/*
=============
Arena_Create
=============
*/
arena_t *Arena_Create( const char *name, size_t size )
{
    if ( !name || size == 0 ) {
        Com_Error( ERR_FATAL, "Arena_Create: bad parameters" );
    }

    /* Allocate the arena header and the data block together for locality */
    byte *block = (byte *)malloc( sizeof(arena_t) + size );
    if ( !block ) {
        Com_Error( ERR_FATAL, "Arena_Create: failed to allocate %zu bytes for '%s'", size, name );
    }

    arena_t *a = (arena_t *)block;
    memset( a, 0, sizeof(arena_t) );
    Q_strncpyz( a->name, name, sizeof(a->name) );
    a->base  = block + sizeof(arena_t);
    a->ptr   = a->base;
    a->end   = a->base + size;
    a->peak  = 0;
    a->magic = ARENA_GUARD_MAGIC;

    Arena_Register( a );
    return a;
}


/*
=============
Arena_Destroy
=============
*/
void Arena_Destroy( arena_t *arena )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        return;
    }
    Arena_Unregister( arena );
    arena->magic = 0;
    free( arena );   /* frees header + data block together */
}


/*
=============
Arena_Alloc
=============
*/
void *Arena_Alloc( arena_t *arena, size_t size, size_t alignment )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        Com_Error( ERR_FATAL, "Arena_Alloc: invalid arena" );
    }
    if ( size == 0 ) {
        return NULL;
    }
    if ( alignment == 0 ) {
        alignment = sizeof(void*);
    }

#ifdef HUNK_DEBUG
    if ( arena->locked ) {
        Com_Error( ERR_FATAL, "Arena_Alloc: arena '%s' is locked (CL_ShutdownLevel test guard)", arena->name );
    }
#endif

    /* Align the bump pointer */
    size_t pad = (alignment - ((size_t)(arena->ptr) & (alignment - 1))) & (alignment - 1);
    byte *aligned = arena->ptr + pad;

    if ( aligned + size > arena->end ) {
        Com_Error( ERR_FATAL, "Arena_Alloc: arena '%s' out of space (used %zu / %zu, requested %zu)",
            arena->name,
            Arena_Used( arena ),
            Arena_Size( arena ),
            size );
    }

    arena->ptr = aligned + size;

    /* Track high-water mark */
    {
        size_t used = (size_t)(arena->ptr - arena->base);
        if ( used > arena->peak ) {
            arena->peak = used;
        }
    }

    return aligned;
}


/*
=============
Arena_Reset
=============
*/
void Arena_Reset( arena_t *arena )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        return;
    }
    arena->ptr = arena->base;
    /* peak is intentionally NOT reset — it's a lifetime high-water mark */
}


/*
=============
Arena_Used
=============
*/
size_t Arena_Used( const arena_t *arena )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        return 0;
    }
    return (size_t)(arena->ptr - arena->base);
}


/*
=============
Arena_Size
=============
*/
size_t Arena_Size( const arena_t *arena )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        return 0;
    }
    return (size_t)(arena->end - arena->base);
}


/*
=============
Arena_Peak
=============
*/
size_t Arena_Peak( const arena_t *arena )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        return 0;
    }
    return arena->peak;
}


#ifdef HUNK_DEBUG
/*
=============
Arena_Lock / Arena_Unlock
Used by Test 3.5 to assert CL_ShutdownLevel doesn't touch persistent arenas.
=============
*/
void Arena_Lock( arena_t *arena )
{
    if ( arena && arena->magic == ARENA_GUARD_MAGIC ) {
        arena->locked = qtrue;
    }
}

void Arena_Unlock( arena_t *arena )
{
    if ( arena && arena->magic == ARENA_GUARD_MAGIC ) {
        arena->locked = qfalse;
    }
}
#endif


/*
=============
Arena_Register / Arena_Unregister
=============
*/
void Arena_Register( arena_t *arena )
{
    if ( s_registryCount >= ARENA_MAX_REGISTERED ) {
        /* Log but don't fatal — arena still works, just won't appear in /memstats */
        Com_Printf( "Arena_Register: registry full, '%s' won't appear in /memstats\n", arena->name );
        return;
    }
    s_registry[s_registryCount++] = arena;
}

void Arena_Unregister( arena_t *arena )
{
    for ( int i = 0; i < s_registryCount; i++ ) {
        if ( s_registry[i] == arena ) {
            s_registry[i] = s_registry[--s_registryCount];
            s_registry[s_registryCount] = NULL;
            return;
        }
    }
}


/*
=============
Arena_PrintStats
Called by /memstats console command.
=============
*/
void Arena_PrintStats( void )
{
    size_t totalUsed = 0, totalSize = 0;

    if ( s_registryCount == 0 ) {
        Com_Printf( "  (no persistent arenas registered)\n" );
        return;
    }

    Com_Printf( "%-24s %8s %8s %8s  %s\n", "Arena", "Size", "Used", "Peak", "Pct" );
    Com_Printf( "%-24s %8s %8s %8s  %s\n",
        "------------------------",
        "--------", "--------", "--------", "---" );

    for ( int i = 0; i < s_registryCount; i++ ) {
        arena_t *a = s_registry[i];
        size_t size = Arena_Size( a );
        size_t used = Arena_Used( a );
        size_t peak = Arena_Peak( a );
        int    pct  = size > 0 ? (int)( used * 100 / size ) : 0;

        Com_Printf( "%-24s %7zuK %7zuK %7zuK  %d%%\n",
            a->name,
            size / 1024,
            used / 1024,
            peak / 1024,
            pct );

        totalUsed += used;
        totalSize += size;
    }

    Com_Printf( "%-24s %7zuK %7zuK\n",
        "TOTAL",
        totalSize / 1024,
        totalUsed / 1024 );
}
