// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "q_shared.h"
#include "qcommon.h"
#include "app_memory.h"

static void AppMemory_ArenaName( char *out, size_t outSize,
	const char *appName, const char *layer )
{
	Com_sprintf( out, outSize, "%s:%s", layer, appName && appName[0] ? appName : "unnamed" );
}

void AppMemory_Init( appMemory_t *memory, const char *name,
	size_t appCapacity, size_t levelCapacity )
{
	char arenaName[64];

	if ( !memory || !name || !name[0] || !appCapacity || !levelCapacity ) {
		Com_Terminate( TERM_UNRECOVERABLE, "AppMemory_Init: bad parameters" );
	}
	if ( memory->appArena || memory->levelArena ) {
		Com_Terminate( TERM_UNRECOVERABLE,
			"AppMemory_Init: owner '%s' already initialized", memory->name );
	}

	memset( memory, 0, sizeof( *memory ) );
	Q_strncpyz( memory->name, name, sizeof( memory->name ) );
	memory->appCapacity = appCapacity;
	memory->levelCapacity = levelCapacity;
	memory->levelGeneration = 1;

	AppMemory_ArenaName( arenaName, sizeof( arenaName ), name, "App" );
	memory->appArena = Arena_Create( arenaName, appCapacity );
	AppMemory_ArenaName( arenaName, sizeof( arenaName ), name, "Level" );
	memory->levelArena = Arena_Create( arenaName, levelCapacity );
}

void AppMemory_Destroy( appMemory_t *memory )
{
	if ( !memory ) {
		return;
	}
	Arena_Destroy( memory->levelArena );
	Arena_Destroy( memory->appArena );
	memset( memory, 0, sizeof( *memory ) );
}

void *App_Alloc( appMemory_t *memory, size_t size, size_t alignment )
{
	if ( !memory || !memory->appArena ) {
		Com_Terminate( TERM_UNRECOVERABLE, "App_Alloc: uninitialized owner" );
	}
	return Arena_Alloc( memory->appArena, size, alignment );
}

void *Level_Alloc( appMemory_t *memory, size_t size, size_t alignment )
{
	if ( !memory || !memory->levelArena ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Level_Alloc: uninitialized owner" );
	}
	return Arena_Alloc( memory->levelArena, size, alignment );
}

void Level_Reset( appMemory_t *memory )
{
	if ( !memory || !memory->levelArena ) {
		return;
	}
	Arena_Reset( memory->levelArena );
	memory->levelGeneration++;
	if ( !memory->levelGeneration ) {
		memory->levelGeneration = 1;
	}
}

qboolean AppMemory_IsInitialized( const appMemory_t *memory )
{
	return memory && memory->appArena && memory->levelArena ? qtrue : qfalse;
}

uint64_t AppMemory_LevelGeneration( const appMemory_t *memory )
{
	return memory ? memory->levelGeneration : 0;
}
