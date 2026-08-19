// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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
LOG_DECLARE_CHANNEL( ch_system, "system" );

#define ARENA_MAX_REGISTERED 32
#define ARENA_GUARD_MAGIC    0xA2A2A2A2U

struct arena_s {
    char   name[64];
    byte  *base;         /* start of OS block */
    byte  *ptr;          /* next free byte (bump pointer) */
    byte  *end;          /* one past last byte of block */
    size_t peak;         /* high-water mark */
    size_t blockBytes;   /* header + data, as passed to mmap (munmap needs it) */
    qboolean mapped;     /* block came from mmap, so free it with munmap */
    uint32_t magic;
#ifdef HUNK_DEBUG
    qboolean locked;
#endif
};

/* Global registry for /memstats */
static arena_t *s_registry[ARENA_MAX_REGISTERED];
static int      s_registryCount;


/*
===========================================================================
Low-address backing

Some consumers cannot use memory from just anywhere in the address space.
LuaJIT is the concrete case: it packs GC pointers into tagged values and
refuses any allocation above 47 bits outright (lj_def.h checkptr47, enforced
at lj_state.c:266). A block from plain malloc is therefore accepted or refused
depending on where the platform's heap happens to sit — measured working on
macOS arm64 and failing in a Linux aarch64 container with the same binary.
Nothing about that is specific to Lua; it is a property any address-sensitive
consumer can have, so the guarantee belongs here rather than in one subsystem.

The strategy mirrors LuaJIT's own (lj_alloc.c mmap_probe), because no portable
flag asks for a low address — MAP_32BIT is Linux/x86-64 only, and far below a
47-bit window anyway. So: ask, check what came back, keep it if it fits,
release and retry with a higher hint if not.

Failure is deliberately NOT fatal. Falling back to malloc leaves every arena
that does not care behaving exactly as before, including on platforms where
probing is unavailable; only an address-sensitive consumer would notice, and
Arena_IsLowAddress lets it check rather than assume.
===========================================================================
*/
#if defined( _WIN32 )
#	define ARENA_HAS_MMAP 0
#else
#	define ARENA_HAS_MMAP 1
#	include <sys/mman.h>
#	include <errno.h>
#endif

/* LuaJIT's limit in GC64 mode (lj_alloc.c LJ_ALLOC_MBITS). It is the strictest
   real consumer here; a tighter bound would reject usable addresses. */
#define ARENA_ADDR_BITS      47
#define ARENA_PROBE_ATTEMPTS 32
/* Start above the lowest pages so a mapping never lands where a NULL
   dereference is meant to fault. Mirrors LJ_ALLOC_MMAP_PROBE_LOWER. */
#define ARENA_PROBE_LOWEST   ( (uintptr_t)0x10000 )

static qboolean Arena_AddrFits( const void *p, size_t size )
{
    uintptr_t a = (uintptr_t)p;
    return ( ( a >> ARENA_ADDR_BITS ) == 0
          && ( ( a + size ) >> ARENA_ADDR_BITS ) == 0 ) ? qtrue : qfalse;
}

/* Returns a block whose whole extent fits below 2^ARENA_ADDR_BITS when it can,
   otherwise whatever malloc gives. *outMapped reports whether the result came
   from mmap, and so must be munmap'd rather than freed. */
static void *Arena_AllocBlock( size_t total, qboolean *outMapped )
{
    *outMapped = qfalse;

#if ARENA_HAS_MMAP
    {
        uintptr_t hint = ARENA_PROBE_LOWEST;
        int       attempt;

        for ( attempt = 0; attempt < ARENA_PROBE_ATTEMPTS; attempt++ ) {
            void *p = mmap( (void *)hint, total, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );

            if ( p == MAP_FAILED ) {
                /* Genuinely out of memory: stop probing and let the malloc path
                   below produce one clear failure. A rejected hint is not an
                   error — the kernel may place the mapping wherever it likes. */
                if ( errno == ENOMEM ) {
                    break;
                }
                hint += 0x1000000;
                continue;
            }
            if ( Arena_AddrFits( p, total ) ) {
                *outMapped = qtrue;
                return p;
            }
            /* Unusable: hand it straight back. Leaking here would cost the
               process a full arena's worth of address space per attempt. */
            munmap( p, total );
            hint += 0x1000000;
            if ( ( ( hint + total ) >> ARENA_ADDR_BITS ) != 0 ) {
                hint = ARENA_PROBE_LOWEST;   /* walked past the window; restart */
            }
        }
    }
#endif

    return malloc( total );
}

/*
=============
Arena_Create
=============
*/
arena_t *Arena_Create( const char *name, size_t size )
{
    qboolean mapped;

    if ( !name || size == 0 ) {
        Com_Terminate( TERM_UNRECOVERABLE, "Arena_Create: bad parameters" );
    }

    /* Allocate the arena header and the data block together for locality */
    byte *block = (byte *)Arena_AllocBlock( sizeof(arena_t) + size, &mapped );
    if ( !block ) {
        Com_Terminate( TERM_UNRECOVERABLE, "Arena_Create: failed to allocate %zu bytes for '%s'", size, name );
    }

    arena_t *a = (arena_t *)block;
    memset( a, 0, sizeof(arena_t) );
    Q_strncpyz( a->name, name, sizeof(a->name) );
    a->base  = block + sizeof(arena_t);
    a->ptr   = a->base;
    a->end   = a->base + size;
    a->peak  = 0;
    a->blockBytes = sizeof(arena_t) + size;
    a->mapped     = mapped;
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
    {
        /* Read both out before clearing magic: the release below invalidates
           the header, so nothing may be read from it afterwards. mmap and
           malloc blocks must be released by their matching call — mixing them
           is undefined behaviour, not merely untidy. */
        qboolean mapped = arena->mapped;
        size_t   bytes  = arena->blockBytes;

        arena->magic = 0;
#if ARENA_HAS_MMAP
        if ( mapped ) {
            munmap( arena, bytes );
            return;
        }
#else
        (void)mapped;
        (void)bytes;
#endif
        free( arena );   /* frees header + data block together */
    }
}


/*
=============
Arena_Alloc
=============
*/
void *Arena_Alloc( arena_t *arena, size_t size, size_t alignment )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        Com_Terminate( TERM_UNRECOVERABLE, "Arena_Alloc: invalid arena" );
    }
    if ( size == 0 ) {
        return NULL;
    }
    if ( alignment == 0 ) {
        alignment = sizeof(void*);
    }

#ifdef HUNK_DEBUG
    if ( arena->locked ) {
        Com_Terminate( TERM_UNRECOVERABLE, "Arena_Alloc: arena '%s' is locked (CL_ShutdownLevel test guard)", arena->name );
    }
#endif

    /* Align the bump pointer */
    size_t pad = (alignment - ((size_t)(arena->ptr) & (alignment - 1))) & (alignment - 1);
    byte *aligned = arena->ptr + pad;

    if ( aligned + size > arena->end ) {
        Com_Terminate( TERM_UNRECOVERABLE, "Arena_Alloc: arena '%s' out of space (used %zu / %zu, requested %zu)",
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


/*
=============
Arena_IsLowAddress

Re-checks the real extent rather than trusting the mapped flag: a malloc block
can land low by luck, and that is just as usable to a caller who only cares
about the address. Answering from the flag would refuse a perfectly good block
purely because of how it was obtained.
=============
*/
qboolean Arena_IsLowAddress( const arena_t *arena )
{
    if ( !arena || arena->magic != ARENA_GUARD_MAGIC ) {
        return qfalse;
    }
    return Arena_AddrFits( arena, arena->blockBytes );
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
        Com_Log( SEV_INFO, LOG_CH(ch_system), "Arena_Register: registry full, '%s' won't appear in /memstats\n", arena->name );
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
        Com_Log( SEV_INFO, LOG_CH(ch_system), "  (no persistent arenas registered)\n" );
        return;
    }

    Com_Log( SEV_INFO, LOG_CH(ch_system), "%-24s %8s %8s %8s  %s\n", "Arena", "Size", "Used", "Peak", "Pct" );
    Com_Log( SEV_INFO, LOG_CH(ch_system), "%-24s %8s %8s %8s  %s\n",
        "------------------------",
        "--------", "--------", "--------", "---" );

    for ( int i = 0; i < s_registryCount; i++ ) {
        arena_t *a = s_registry[i];
        size_t size = Arena_Size( a );
        size_t used = Arena_Used( a );
        size_t peak = Arena_Peak( a );
        int    pct  = size > 0 ? (int)( used * 100 / size ) : 0;

        Com_Log( SEV_INFO, LOG_CH(ch_system), "%-24s %7zuK %7zuK %7zuK  %d%%\n",
            a->name,
            size / 1024,
            used / 1024,
            peak / 1024,
            pct );

        totalUsed += used;
        totalSize += size;
    }

    Com_Log( SEV_INFO, LOG_CH(ch_system), "%-24s %7zuK %7zuK\n",
        "TOTAL",
        totalSize / 1024,
        totalUsed / 1024 );
}
