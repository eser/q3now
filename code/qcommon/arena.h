// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
arena.h — Per-subsystem arena allocators

Each persistent subsystem that must survive Hunk_ClearLevel() gets its own
arena.  Arenas are bump-pointer allocators backed by a single OS allocation.
Fragmentation is zero.  Subsystem shutdown = Arena_Destroy = one free().

There is no per-object Arena_Free.  If a subsystem needs dynamic object
reclamation, it maintains its own free-list within the arena block.

Usage:
    arena_t *myArena = Arena_Create( "MySubsystem", 4 * 1024 * 1024 );
    MyState *s = Arena_Alloc( myArena, sizeof(MyState), 16 );
    ...
    Arena_Destroy( myArena );   // on engine shutdown

See: docs/hunk-audit.md for the full subsystem classification.
===========================================================================
*/
#ifndef ARENA_H
#define ARENA_H

#include "q_shared.h"

typedef struct arena_s arena_t;

/* Lifecycle */
arena_t *Arena_Create( const char *name, size_t size );
void     Arena_Destroy( arena_t *arena );

/* Allocation */
void    *Arena_Alloc( arena_t *arena, size_t size, size_t alignment );
#define  Arena_AllocType(arena, type) \
    ((type *)Arena_Alloc((arena), sizeof(type), _Alignof(type)))
#define  Arena_AllocArray(arena, type, count) \
    ((type *)Arena_Alloc((arena), sizeof(type) * (count), _Alignof(type)))

/* State */
void     Arena_Reset( arena_t *arena );        /* reset bump ptr, keep block */
size_t   Arena_Used( const arena_t *arena );   /* bytes consumed */
size_t   Arena_Size( const arena_t *arena );   /* block capacity */
size_t   Arena_Peak( const arena_t *arena );   /* high-water mark */

/* Debug lock (used by Test 3.5 — assert no access during CL_ShutdownLevel) */
#ifdef HUNK_DEBUG
void     Arena_Lock( arena_t *arena );         /* assert on alloc while locked */
void     Arena_Unlock( arena_t *arena );
#else
#define  Arena_Lock(a)   ((void)0)
#define  Arena_Unlock(a) ((void)0)
#endif

/* Registry (for /memstats reporting) */
void     Arena_Register( arena_t *arena );
void     Arena_Unregister( arena_t *arena );
void     Arena_PrintStats( void );             /* called by /memstats command */

#endif /* ARENA_H */
