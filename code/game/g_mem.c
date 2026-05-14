// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
//
// g_mem.c
//


#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );


#define POOLSIZE	(256 * 1024)

static char		memoryPool[POOLSIZE];
static int		allocPoint;

void *G_Alloc( int size ) {
	char	*p;

	if ( g_debugAlloc.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "G_Alloc of %i bytes (%i left)\n", size, POOLSIZE - allocPoint - ( ( size + 31 ) & ~31 ) );
	}

	if ( allocPoint + size > POOLSIZE ) {
	  Com_Terminate( TERM_CLIENT_DROP, "G_Alloc: failed on allocation of %i bytes", size );
		return NULL;
	}

	p = &memoryPool[allocPoint];

	allocPoint += ( size + 31 ) & ~31;

	return p;
}

void G_InitMemory( void ) {
	allocPoint = 0;
}

void Svcmd_GameMem_f( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_game), "Game memory status: %i out of %i bytes allocated\n", allocPoint, POOLSIZE );
}
