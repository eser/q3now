/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// ui_mapcache.c — enumerates ALL .bsp files, parses .arena data where available
// Provides fast access to map metadata for the map selection UI

#include "ui_local.h"

#define MAX_CACHED_MAPS  1024

typedef struct {
	mapCacheEntry_t maps[MAX_CACHED_MAPS];
	int             numMaps;
	int             filteredIndices[MAX_CACHED_MAPS];
	int             numFiltered;
	qboolean        initialized;
	char            filterText[64];
	int             filterGametype;         // -1 = all, else gametype index
	qboolean        filterArenaOnly;        // qtrue = show only maps with .arena files
} mapCache_t;

static mapCache_t s_mapCache;


/*
=================
MapCache_GametypeBits

Parse gametype string from .arena info into a bitmask.
Duplicated from ui_startserver.c GametypeBits() to avoid cross-file dependency.
=================
*/
static int MapCache_GametypeBits( const char *string ) {
	int         bits;
	const char  *p;
	const char  *token;

	bits = 0;
	p = string;
	while ( 1 ) {
		token = COM_ParseExt( &p, qfalse );
		if ( !token[0] ) {
			break;
		}

		if ( Q_stricmp( token, "ffa" ) == 0 ) {
			bits |= 1 << GT_FFA;
			bits |= 1 << GT_KINGOFTHEHILL;
			bits |= 1 << GT_LASTMANSTANDING;
			continue;
		}

		if ( Q_stricmp( token, "tourney" ) == 0 ) {
			bits |= 1 << GT_TOURNAMENT;
			continue;
		}

		if ( Q_stricmp( token, "team" ) == 0 ) {
			bits |= 1 << GT_TEAM;
			continue;
		}

		if ( Q_stricmp( token, "ctf" ) == 0 ) {
			bits |= 1 << GT_CTF;
			continue;
		}
	}

	return bits;
}


/*
=================
MapCache_SortCompare
=================
*/
static int MapCache_SortCompare( const void *a, const void *b ) {
	const mapCacheEntry_t *ea = (const mapCacheEntry_t *)a;
	const mapCacheEntry_t *eb = (const mapCacheEntry_t *)b;
	return Q_stricmp( ea->mapname, eb->mapname );
}


/*
=================
MapCache_Init

Enumerates all .bsp files and cross-references with .arena data.
=================
*/
void MapCache_Init( void ) {
	char        listbuf[16384];
	char        *listptr;
	int         listCount;
	int         i, j;
	int         numArenas;
	const char  *info;
	const char  *mapname;
	char        aasfile[MAX_QPATH];
	fileHandle_t f;
	int         len;
	qboolean    found;

	if ( s_mapCache.initialized ) {
		return;
	}

	memset( &s_mapCache, 0, sizeof( s_mapCache ) );
	s_mapCache.filterGametype = -1;

	// Step 1: Get all .bsp files from maps/
	listCount = trap_FS_GetFileList( "maps", ".bsp", listbuf, sizeof( listbuf ) );
	trap_Print( va( "MapCache: .bsp scan found %d maps\n", listCount ) );
	listptr = listbuf;

	for ( i = 0; i < listCount && s_mapCache.numMaps < MAX_CACHED_MAPS; i++ ) {
		int nameLen = strlen( listptr );
		if ( nameLen > 0 ) {
			mapCacheEntry_t *entry = &s_mapCache.maps[s_mapCache.numMaps];

			// Strip .bsp extension
			Q_strncpyz( entry->mapname, listptr, MAPNAME_LEN );
			len = strlen( entry->mapname );
			if ( len > 4 && !Q_stricmp( entry->mapname + len - 4, ".bsp" ) ) {
				entry->mapname[len - 4] = '\0';
			}

			// Default values — will be overridden if .arena data exists
			Q_strncpyz( entry->longname, entry->mapname, MAPNAME_LEN );
			entry->gametypeBits = (1 << GT_FFA) | (1 << GT_TOURNAMENT) | (1 << GT_TEAM) |
			                      (1 << GT_CTF) | (1 << GT_KINGOFTHEHILL) | (1 << GT_LASTMANSTANDING);
			entry->hasArena = qfalse;
			entry->hasAAS = qfalse;
			entry->levelshot = 0;

			// Check for .aas file (bot navigation)
			Com_sprintf( aasfile, sizeof( aasfile ), "maps/%s.aas", entry->mapname );
			len = trap_FS_FOpenFile( aasfile, &f, FS_READ );
			if ( f ) {
				entry->hasAAS = qtrue;
				trap_FS_FCloseFile( f );
			}

			s_mapCache.numMaps++;
		}
		listptr += nameLen + 1;
	}

	// Step 2: Cross-reference with arena data for longname, gametype, bots, fraglimit
	numArenas = UI_GetNumArenas();
	for ( i = 0; i < numArenas; i++ ) {
		info = UI_GetArenaInfoByNumber( i );
		if ( !info || !info[0] ) {
			continue;
		}

		mapname = Info_ValueForKey( info, "map" );
		if ( !mapname[0] ) {
			continue;
		}

		// Find matching cache entry from .bsp scan
		for ( j = 0; j < s_mapCache.numMaps; j++ ) {
			if ( Q_stricmp( s_mapCache.maps[j].mapname, mapname ) == 0 ) {
				/*
				 * IMPORTANT: Info_ValueForKey uses 2 rotating static buffers.
				 * Calling it 4 times means buffer 0 and 2 alias, buffer 1 and 3 alias.
				 * We must copy values to local buffers before using them.
				 */
				char localLongname[MAPNAME_LEN];
				char localType[64];
				char localBots[256];
				char localFraglimit[8];

				Q_strncpyz( localLongname, Info_ValueForKey( info, "longname" ), sizeof( localLongname ) );
				Q_strncpyz( localType, Info_ValueForKey( info, "type" ), sizeof( localType ) );
				Q_strncpyz( localBots, Info_ValueForKey( info, "bots" ), sizeof( localBots ) );
				Q_strncpyz( localFraglimit, Info_ValueForKey( info, "fraglimit" ), sizeof( localFraglimit ) );

				if ( localLongname[0] ) {
					Q_strncpyz( s_mapCache.maps[j].longname, localLongname, MAPNAME_LEN );
				}
				if ( localType[0] ) {
					s_mapCache.maps[j].gametypeBits = MapCache_GametypeBits( localType );
				}
				if ( localBots[0] ) {
					Q_strncpyz( s_mapCache.maps[j].bots, localBots, sizeof( s_mapCache.maps[j].bots ) );
				}
				if ( localFraglimit[0] ) {
					s_mapCache.maps[j].fraglimit = atoi( localFraglimit );
				}
				s_mapCache.maps[j].hasArena = qtrue;
				break;
			}
		}
		// Arena entries with no matching .bsp are silently skipped (bot tiers etc.)
	}

	trap_Print( va( "MapCache: %d maps cached after arena cross-ref\n", s_mapCache.numMaps ) );

	// Step 3: Sort alphabetically
	qsort( s_mapCache.maps, s_mapCache.numMaps, sizeof( mapCacheEntry_t ), MapCache_SortCompare );

	// Step 4: Initialize filtered list to show everything
	s_mapCache.numFiltered = s_mapCache.numMaps;
	for ( i = 0; i < s_mapCache.numMaps; i++ ) {
		s_mapCache.filteredIndices[i] = i;
	}

	s_mapCache.initialized = qtrue;
}


/*
=================
MapCache_StrStr

Case-insensitive substring search.
=================
*/
static qboolean MapCache_StrStr( const char *haystack, const char *needle ) {
	char lowerHay[MAPNAME_LEN];
	char lowerNeedle[64];
	int i;

	if ( !needle[0] ) {
		return qtrue;
	}

	Q_strncpyz( lowerHay, haystack, sizeof( lowerHay ) );
	Q_strncpyz( lowerNeedle, needle, sizeof( lowerNeedle ) );

	for ( i = 0; lowerHay[i]; i++ ) {
		if ( lowerHay[i] >= 'A' && lowerHay[i] <= 'Z' ) {
			lowerHay[i] += 'a' - 'A';
		}
	}
	for ( i = 0; lowerNeedle[i]; i++ ) {
		if ( lowerNeedle[i] >= 'A' && lowerNeedle[i] <= 'Z' ) {
			lowerNeedle[i] += 'a' - 'A';
		}
	}

	return strstr( lowerHay, lowerNeedle ) != NULL;
}


/*
=================
MapCache_Filter

Filters cached maps by text (substring match on mapname/longname),
gametype (bitmask check), and arena-only flag.
=================
*/
void MapCache_Filter( const char *text, int gametype, qboolean arenaOnly ) {
	int i;
	int matchbits;

	if ( !s_mapCache.initialized ) {
		return;
	}

	// Store filter state
	if ( text ) {
		Q_strncpyz( s_mapCache.filterText, text, sizeof( s_mapCache.filterText ) );
	} else {
		s_mapCache.filterText[0] = '\0';
	}
	s_mapCache.filterGametype = gametype;
	s_mapCache.filterArenaOnly = arenaOnly;

	// Build gametype match bits
	if ( gametype >= 0 ) {
		matchbits = 1 << gametype;
	} else {
		matchbits = ~0;  // match all
	}

	s_mapCache.numFiltered = 0;
	for ( i = 0; i < s_mapCache.numMaps; i++ ) {
		mapCacheEntry_t *entry = &s_mapCache.maps[i];

		// Gametype filter
		if ( !( entry->gametypeBits & matchbits ) ) {
			continue;
		}

		// Arena-only filter
		if ( arenaOnly && !entry->hasArena ) {
			continue;
		}

		// Text filter — match against mapname or longname
		if ( s_mapCache.filterText[0] ) {
			if ( !MapCache_StrStr( entry->mapname, s_mapCache.filterText ) &&
			     !MapCache_StrStr( entry->longname, s_mapCache.filterText ) ) {
				continue;
			}
		}

		s_mapCache.filteredIndices[s_mapCache.numFiltered] = i;
		s_mapCache.numFiltered++;
	}
}


/*
=================
MapCache_GetCount
=================
*/
int MapCache_GetCount( void ) {
	return s_mapCache.numFiltered;
}


/*
=================
MapCache_GetEntry
=================
*/
const mapCacheEntry_t *MapCache_GetEntry( int filteredIndex ) {
	if ( filteredIndex < 0 || filteredIndex >= s_mapCache.numFiltered ) {
		return NULL;
	}
	return &s_mapCache.maps[s_mapCache.filteredIndices[filteredIndex]];
}


/*
=================
MapCache_GetLevelshot

Lazy-loads the levelshot shader on first access.
=================
*/
qhandle_t MapCache_GetLevelshot( int filteredIndex ) {
	mapCacheEntry_t *entry;
	char picname[MAX_QPATH];

	if ( filteredIndex < 0 || filteredIndex >= s_mapCache.numFiltered ) {
		return 0;
	}

	entry = &s_mapCache.maps[s_mapCache.filteredIndices[filteredIndex]];

	if ( !entry->levelshot ) {
		Com_sprintf( picname, sizeof( picname ), "levelshots/%s", entry->mapname );
		entry->levelshot = trap_R_RegisterShaderNoMip( picname );
		if ( !entry->levelshot ) {
			entry->levelshot = trap_R_RegisterShaderNoMip( "menu/art/unknownmap" );
		}
	}

	return entry->levelshot;
}


/*
=================
MapCache_FindByName

Find a cache entry by exact mapname match (case-insensitive).
Returns NULL if not found or if name is NULL/empty.
=================
*/
const mapCacheEntry_t *MapCache_FindByName( const char *mapname ) {
	int i;

	if ( !mapname || !mapname[0] ) {
		return NULL;
	}

	for ( i = 0; i < s_mapCache.numMaps; i++ ) {
		if ( Q_stricmp( s_mapCache.maps[i].mapname, mapname ) == 0 ) {
			return &s_mapCache.maps[i];
		}
	}

	return NULL;
}
