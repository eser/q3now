// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_APP_MEMORY_H
#define WIRED_APP_MEMORY_H

#include "arena.h"

/*
 * Explicit Engine/App/Level lifetime owner.
 *
 * The container is deliberately independent of vm_t, clientApp_t and renderer
 * types.  Those subsystems embed or reference one of these contexts; allocation
 * never discovers an owner through a process-global "active app" singleton.
 */
typedef struct appMemory_s {
	arena_t *appArena;
	arena_t *levelArena;
	char     name[48];
	size_t   appCapacity;
	size_t   levelCapacity;
	uint64_t levelGeneration;
} appMemory_t;

void AppMemory_Init( appMemory_t *memory, const char *name,
	size_t appCapacity, size_t levelCapacity );
void AppMemory_Destroy( appMemory_t *memory );

void *App_Alloc( appMemory_t *memory, size_t size, size_t alignment );
void *Level_Alloc( appMemory_t *memory, size_t size, size_t alignment );

/* Retires only map-owned allocations.  App allocations and their addresses
 * remain valid until AppMemory_Destroy. */
void Level_Reset( appMemory_t *memory );

qboolean AppMemory_IsInitialized( const appMemory_t *memory );
uint64_t AppMemory_LevelGeneration( const appMemory_t *memory );

#define App_AllocType(memory, type) \
	((type *)App_Alloc((memory), sizeof(type), _Alignof(type)))
#define App_AllocArray(memory, type, count) \
	((type *)App_Alloc((memory), sizeof(type) * (count), _Alignof(type)))
#define Level_AllocType(memory, type) \
	((type *)Level_Alloc((memory), sizeof(type), _Alignof(type)))
#define Level_AllocArray(memory, type, count) \
	((type *)Level_Alloc((memory), sizeof(type) * (count), _Alignof(type)))

#endif /* WIRED_APP_MEMORY_H */
