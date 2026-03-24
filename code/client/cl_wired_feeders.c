/*
===========================================================================
cl_wired_feeders.c — Wired UI feeder implementations

Provides data for ITEM_TYPE_LISTBOX items. Each feeder reads directly
from client engine data structures — no VM indirection.

Feeders implemented:
  FEEDER_SERVERS  (0x02) — server browser list
  FEEDER_MAPS     (0x01) — maps filtered by game type (single-player)
  FEEDER_ALLMAPS   (0x04) — maps filtered by game type (network)
  FEEDER_DEMOS    (0x0a) — demo file list
  FEEDER_MODS     (0x09) — mod directory list
===========================================================================
*/

#include "client.h"
#include "cl_wired_ui.h"
#include "../ui/menudef.h"

#if FEAT_WIRED_UI

// ── server browser feeder ─────────────────────────────────────────────
// Data source: cls.globalServers[], cls.localServers[], cls.favoriteServers[]
// Controlled by ui_netSource cvar (0=local, 1-5=internet masters, 6=favorites)

// server display list — indices into the source array, sorted/filtered
#define MAX_DISPLAY_SERVERS  4096
static int  wired_serverDisplayList[MAX_DISPLAY_SERVERS];
static int  wired_serverDisplayCount = 0;

// Map ui_netSource cvar (0=local, 1=internet, 6=favorites) to engine arrays
static void WiredFeeder_GetServerList( serverInfo_t **servers, int *count ) {
	int uiSource = Cvar_VariableIntegerValue( "ui_netSource" );

	if ( uiSource == 0 ) {
		*servers = cls.localServers;
		*count = cls.numlocalservers;
	} else if ( uiSource == 6 ) {
		*servers = cls.favoriteServers;
		*count = cls.numfavoriteservers;
	} else {
		*servers = cls.globalServers;
		*count = cls.numglobalservers;
	}
}

static serverInfo_t *WiredFeeder_GetServerPtr( int index ) {
	serverInfo_t *servers;
	int count;

	WiredFeeder_GetServerList( &servers, &count );

	if ( index < 0 || index >= count ) return NULL;
	return &servers[index];
}

static int WiredFeeder_ServerCount( int feederID ) {
	serverInfo_t *servers;
	int count;
	WiredFeeder_GetServerList( &servers, &count );
	return count;
}

static const char *WiredFeeder_ServerItemText( int feederID, int index, int column ) {
	static char buf[256];
	serverInfo_t *s = WiredFeeder_GetServerPtr( index );

	if ( !s ) return "";

	switch ( column ) {
		case 0: return s->hostName;
		case 1: return s->mapName;
		case 2:
			Com_sprintf( buf, sizeof(buf), "%d/%d", s->clients, s->maxClients );
			return buf;
		case 3:
			Com_sprintf( buf, sizeof(buf), "%d", s->ping );
			return buf;
		case 4: return s->game[0] ? s->game : "baseq3";
		default: return "";
	}
}

static int wired_selectedServer = -1;

static void WiredFeeder_ServerSelection( int feederID, int index ) {
	serverInfo_t *s = WiredFeeder_GetServerPtr( index );
	wired_selectedServer = index;
	if ( s ) {
		// store address in cvar so connect action can use it
		Cvar_Set( "ui_selectedServerAddr", NET_AdrToStringwPort( &s->adr ) );
		Cvar_Set( "ui_selectedServerName", s->hostName );
	}
}

// ── demo feeder ───────────────────────────────────────────────────────
// Data source: FS_GetFileList("demos", extension)

#define MAX_WIRED_DEMOS  512
static char  wired_demoList[MAX_WIRED_DEMOS][MAX_QPATH];
static int   wired_demoCount = 0;

void WiredFeeder_LoadDemos( void ) {
	char listBuf[8192];
	int numFiles, i;
	char *namePtr;
	const char *ext;

	wired_demoCount = 0;

	// try current protocol demo extension
	ext = va( ".dm_%d", DEFAULT_PROTOCOL_VERSION );
	numFiles = FS_GetFileList( "demos", ext, listBuf, sizeof( listBuf ) );

	namePtr = listBuf;
	for ( i = 0; i < numFiles && wired_demoCount < MAX_WIRED_DEMOS; i++ ) {
		Q_strncpyz( wired_demoList[wired_demoCount], namePtr, sizeof( wired_demoList[0] ) );
		// strip extension for display
		char *dot = strrchr( wired_demoList[wired_demoCount], '.' );
		if ( dot ) *dot = '\0';
		wired_demoCount++;
		namePtr += strlen( namePtr ) + 1;
	}
}

static int WiredFeeder_DemoCount( int feederID ) {
	return wired_demoCount;
}

static const char *WiredFeeder_DemoItemText( int feederID, int index, int column ) {
	if ( index < 0 || index >= wired_demoCount ) return "";
	return wired_demoList[index];
}

static int wired_selectedDemo = -1;

static void WiredFeeder_DemoSelection( int feederID, int index ) {
	wired_selectedDemo = index;
	if ( index >= 0 && index < wired_demoCount ) {
		Cvar_Set( "ui_selectedDemo", wired_demoList[index] );
	}
}

// ── mod feeder ────────────────────────────────────────────────────────
// Data source: FS_GetFileList("$modlist", "")

#define MAX_WIRED_MODS  64
static char  wired_modList[MAX_WIRED_MODS][MAX_QPATH];
static char  wired_modDesc[MAX_WIRED_MODS][256];
static int   wired_modCount = 0;

void WiredFeeder_LoadMods( void ) {
	char listBuf[4096];
	int numDirs, i;
	char *dirPtr;

	wired_modCount = 0;

	// first entry is always the base game
	Q_strncpyz( wired_modList[0], "baseq3", sizeof( wired_modList[0] ) );
	Q_strncpyz( wired_modDesc[0], "q3now (base game)", sizeof( wired_modDesc[0] ) );
	wired_modCount = 1;

	numDirs = FS_GetFileList( "$modlist", "", listBuf, sizeof( listBuf ) );

	dirPtr = listBuf;
	for ( i = 0; i < numDirs && wired_modCount < MAX_WIRED_MODS; i++ ) {
		if ( dirPtr[0] && Q_stricmp( dirPtr, "baseq3" ) ) {
			Q_strncpyz( wired_modList[wired_modCount], dirPtr, sizeof( wired_modList[0] ) );
			// try to read description.txt
			wired_modDesc[wired_modCount][0] = '\0';
			{
				fileHandle_t f;
				int len = FS_FOpenFileRead( va( "%s/description.txt", dirPtr ), &f, qfalse );
				if ( len > 0 && f != FS_INVALID_HANDLE ) {
					if ( len >= (int)sizeof( wired_modDesc[0] ) ) len = sizeof( wired_modDesc[0] ) - 1;
					FS_Read( wired_modDesc[wired_modCount], len, f );
					wired_modDesc[wired_modCount][len] = '\0';
					FS_FCloseFile( f );
				} else {
					Q_strncpyz( wired_modDesc[wired_modCount], dirPtr, sizeof( wired_modDesc[0] ) );
					if ( f != FS_INVALID_HANDLE ) FS_FCloseFile( f );
				}
			}
			wired_modCount++;
		}
		dirPtr += strlen( dirPtr ) + 1;
	}
}

static int WiredFeeder_ModCount( int feederID ) {
	return wired_modCount;
}

static const char *WiredFeeder_ModItemText( int feederID, int index, int column ) {
	if ( index < 0 || index >= wired_modCount ) return "";
	switch ( column ) {
		case 0:  return wired_modList[index];
		case 1:  return wired_modDesc[index];
		default: return wired_modList[index];
	}
}

static int wired_selectedMod = -1;

static void WiredFeeder_ModSelection( int feederID, int index ) {
	wired_selectedMod = index;
	if ( index >= 0 && index < wired_modCount ) {
		Cvar_Set( "ui_selectedMod", wired_modList[index] );
	}
}

// ── map feeder ────────────────────────────────────────────────────────
// Data source: .bsp scan + .arena cross-reference from scripts/ directory
// Maps are filtered by game type bits

#define MAX_WIRED_MAPS  1024
#define MAX_ARENA_TEXT  8192

typedef struct {
	char    mapLoadName[MAX_QPATH];
	char    mapName[64];
	int     typeBits;                   // bitmask: (1 << GT_FFA) | (1 << GT_TEAM) etc.
} wiredMapInfo_t;

static wiredMapInfo_t  wired_maps[MAX_WIRED_MAPS];
static int             wired_mapCount = 0;
static int             wired_filteredMaps[MAX_WIRED_MAPS];
static int             wired_filteredMapCount = 0;

// parse "ffa tourney team ctf" → bitmask
// mirrors MapCache_GametypeBits (ui_mapcache.c) — adapted for client-side use
static int WiredFeeder_GametypeBits( const char *string ) {
	int         bits = 0;
	const char  *p = string;
	const char  *token;

	while ( 1 ) {
		token = COM_ParseExt( &p, qfalse );
		if ( !token[0] ) break;

		if ( Q_stricmp( token, "ffa" ) == 0 ) {
			bits |= 1 << GT_FFA;
			bits |= 1 << GT_KINGOFTHEHILL;
			bits |= 1 << GT_LASTMANSTANDING;
		} else if ( Q_stricmp( token, "tourney" ) == 0 ) {
			bits |= 1 << GT_TOURNAMENT;
		} else if ( Q_stricmp( token, "team" ) == 0 ) {
			bits |= 1 << GT_TEAM;
		} else if ( Q_stricmp( token, "ctf" ) == 0 ) {
			bits |= 1 << GT_CTF;
		}
	}

	return bits;
}

// find a map entry by load name (case-insensitive)
static wiredMapInfo_t *WiredFeeder_FindMap( const char *mapname ) {
	int i;
	for ( i = 0; i < wired_mapCount; i++ ) {
		if ( Q_stricmp( wired_maps[i].mapLoadName, mapname ) == 0 ) {
			return &wired_maps[i];
		}
	}
	return NULL;
}

// parse { key value } arena blocks from a buffer — same format as UI_ParseInfos
// but stores directly into wired_maps entries instead of Info strings
static void WiredFeeder_ParseArenaBuffer( const char *buf ) {
	const char  *token;
	const char  *p = buf;
	char        key[MAX_TOKEN_CHARS];
	char        arenaMap[MAX_QPATH];
	char        arenaLongname[64];
	char        arenaType[64];
	wiredMapInfo_t *entry;

	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) break;
		if ( strcmp( token, "{" ) != 0 ) break;

		arenaMap[0] = '\0';
		arenaLongname[0] = '\0';
		arenaType[0] = '\0';

		// parse key-value pairs until closing brace
		while ( 1 ) {
			token = COM_ParseExt( &p, qtrue );
			if ( !token[0] || !strcmp( token, "}" ) ) break;

			Q_strncpyz( key, token, sizeof( key ) );
			token = COM_ParseExt( &p, qfalse );

			if ( Q_stricmp( key, "map" ) == 0 ) {
				Q_strncpyz( arenaMap, token, sizeof( arenaMap ) );
			} else if ( Q_stricmp( key, "longname" ) == 0 ) {
				Q_strncpyz( arenaLongname, token, sizeof( arenaLongname ) );
			} else if ( Q_stricmp( key, "type" ) == 0 ) {
				Q_strncpyz( arenaType, token, sizeof( arenaType ) );
			}
		}

		if ( !arenaMap[0] ) continue;

		// cross-reference: find the BSP entry and enrich it
		entry = WiredFeeder_FindMap( arenaMap );
		if ( !entry ) continue;

		if ( arenaLongname[0] ) {
			Q_strncpyz( entry->mapName, arenaLongname, sizeof( entry->mapName ) );
		}
		if ( arenaType[0] ) {
			entry->typeBits = WiredFeeder_GametypeBits( arenaType );
		}
	}
}

// load a single .arena file and cross-reference with map entries
static void WiredFeeder_LoadArenaFile( const char *filename ) {
	fileHandle_t f;
	int len;
	char buf[MAX_ARENA_TEXT];

	len = FS_FOpenFileRead( filename, &f, qfalse );
	if ( !f || len <= 0 ) {
		if ( f != FS_INVALID_HANDLE ) FS_FCloseFile( f );
		return;
	}
	if ( len >= (int)sizeof( buf ) ) {
		FS_FCloseFile( f );
		return;
	}

	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

	WiredFeeder_ParseArenaBuffer( buf );
}

static int WiredFeeder_MapSortCompare( const void *a, const void *b ) {
	const wiredMapInfo_t *ma = (const wiredMapInfo_t *)a;
	const wiredMapInfo_t *mb = (const wiredMapInfo_t *)b;
	return Q_stricmp( ma->mapLoadName, mb->mapLoadName );
}

void WiredFeeder_LoadMaps( void ) {
	char listBuf[16384];
	int numFiles, i;
	char *namePtr;
	int numArenaFiles;
	char arenaListBuf[4096];
	char *arenaPtr;
	int arenaLen;
	char filename[128];
	int arenaCount = 0;

	wired_mapCount = 0;

	// ── Step 1: scan all .bsp files ──────────────────────────────────
	numFiles = FS_GetFileList( "maps", ".bsp", listBuf, sizeof( listBuf ) );

	namePtr = listBuf;
	for ( i = 0; i < numFiles && wired_mapCount < MAX_WIRED_MAPS; i++ ) {
		char *dot;
		Q_strncpyz( wired_maps[wired_mapCount].mapLoadName, namePtr, sizeof( wired_maps[0].mapLoadName ) );

		// strip .bsp extension
		dot = strrchr( wired_maps[wired_mapCount].mapLoadName, '.' );
		if ( dot ) *dot = '\0';

		// defaults: display name = load name, all game types
		Q_strncpyz( wired_maps[wired_mapCount].mapName,
			wired_maps[wired_mapCount].mapLoadName, sizeof( wired_maps[0].mapName ) );
		wired_maps[wired_mapCount].typeBits =
			(1 << GT_FFA) | (1 << GT_TOURNAMENT) | (1 << GT_TEAM) |
			(1 << GT_CTF) | (1 << GT_KINGOFTHEHILL) | (1 << GT_LASTMANSTANDING);

		wired_mapCount++;
		namePtr += strlen( namePtr ) + 1;
	}

	// ── Step 2: parse .arena files to enrich map entries ─────────────
	// first try arenas.txt (bulk arena definitions)
	WiredFeeder_LoadArenaFile( "scripts/arenas.txt" );

	// then scan individual .arena files
	numArenaFiles = FS_GetFileList( "scripts", ".arena", arenaListBuf, sizeof( arenaListBuf ) );
	arenaPtr = arenaListBuf;
	for ( i = 0; i < numArenaFiles; i++ ) {
		arenaLen = strlen( arenaPtr );
		Com_sprintf( filename, sizeof( filename ), "scripts/%s", arenaPtr );
		WiredFeeder_LoadArenaFile( filename );
		arenaCount++;
		arenaPtr += arenaLen + 1;
	}

	// ── Step 3: sort alphabetically by display name ──────────────────
	qsort( wired_maps, wired_mapCount, sizeof( wiredMapInfo_t ), WiredFeeder_MapSortCompare );

	// ── Step 4: initialize filtered list ─────────────────────────────
	wired_filteredMapCount = wired_mapCount;
	for ( i = 0; i < wired_mapCount; i++ ) {
		wired_filteredMaps[i] = i;
	}

	Com_Printf( "WiredUI: %d maps loaded, %d arena files parsed\n", wired_mapCount, arenaCount );
}

static void WiredFeeder_FilterMaps( void ) {
	int i;
	int gameType = Cvar_VariableIntegerValue( "ui_netGameType" );
	int typeBit = ( 1 << gameType );

	wired_filteredMapCount = 0;
	for ( i = 0; i < wired_mapCount; i++ ) {
		if ( wired_maps[i].typeBits & typeBit ) {
			wired_filteredMaps[wired_filteredMapCount++] = i;
		}
	}
}

static int WiredFeeder_MapCount( int feederID ) {
	WiredFeeder_FilterMaps();
	return wired_filteredMapCount;
}

static const char *WiredFeeder_MapItemText( int feederID, int index, int column ) {
	static char buf[128];
	int mapIdx;
	if ( index < 0 || index >= wired_filteredMapCount ) return "";
	mapIdx = wired_filteredMaps[index];
	switch ( column ) {
		case 0:  return wired_maps[mapIdx].mapLoadName;
		case 1:
			// show longname if different from shortname, otherwise just shortname
			if ( Q_stricmp( wired_maps[mapIdx].mapName, wired_maps[mapIdx].mapLoadName ) != 0 ) {
				return wired_maps[mapIdx].mapName;
			}
			return "";
		default: return wired_maps[mapIdx].mapLoadName;
	}
}

static int wired_selectedMap = -1;

static void WiredFeeder_MapSelection( int feederID, int index ) {
	if ( index >= 0 && index < wired_filteredMapCount ) {
		int mapIdx = wired_filteredMaps[index];
		wired_selectedMap = mapIdx;
		Cvar_Set( "ui_selectedMap", wired_maps[mapIdx].mapLoadName );
		Cvar_Set( "ui_currentNetMap", va( "%d", mapIdx ) );
	}
}

// ── feeder registration ───────────────────────────────────────────────

void WiredUI_RegisterCoreFeeders( void ) {
	WiredUI_RegisterFeeder( FEEDER_SERVERS, WiredFeeder_ServerCount,
		WiredFeeder_ServerItemText, WiredFeeder_ServerSelection );
	WiredUI_RegisterFeeder( FEEDER_MAPS, WiredFeeder_MapCount,
		WiredFeeder_MapItemText, WiredFeeder_MapSelection );
	WiredUI_RegisterFeeder( FEEDER_ALLMAPS, WiredFeeder_MapCount,
		WiredFeeder_MapItemText, WiredFeeder_MapSelection );
	WiredUI_RegisterFeeder( FEEDER_DEMOS, WiredFeeder_DemoCount,
		WiredFeeder_DemoItemText, WiredFeeder_DemoSelection );
	WiredUI_RegisterFeeder( FEEDER_MODS, WiredFeeder_ModCount,
		WiredFeeder_ModItemText, WiredFeeder_ModSelection );

	// load initial data
	WiredFeeder_LoadMaps();
	WiredFeeder_LoadDemos();
	WiredFeeder_LoadMods();

	Com_Printf( "WiredUI: feeders registered (demos=%d, mods=%d)\n",
		wired_demoCount, wired_modCount );
}

#endif // FEAT_WIRED_UI
