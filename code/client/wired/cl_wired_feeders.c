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

#include "../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_hud.h"
#include "../../ui/menudef.h"

#if FEAT_WIRED_UI

// forward declarations
static int WiredFeeder_ServerSortCompare( const void *a, const void *b );

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

static int wired_serverLastRawCount = 0;  // track raw count for rebuild detection

static qboolean WiredFeeder_ServerPassesFilter( serverInfo_t *s ) {
	int filterGameType = Cvar_VariableIntegerValue( "ui_browserGameType" );
	int showFull      = Cvar_VariableIntegerValue( "ui_browserShowFull" );
	int showEmpty     = Cvar_VariableIntegerValue( "ui_browserShowEmpty" );
	int maxPing       = Cvar_VariableIntegerValue( "ui_browserMaxPing" );

	// game type filter (0 = all)
	if ( filterGameType > 0 && s->gameType != ( filterGameType - 1 ) ) return qfalse;
	// full server filter
	if ( !showFull && s->clients >= s->maxClients && s->maxClients > 0 ) return qfalse;
	// empty server filter
	if ( !showEmpty && s->clients == 0 ) return qfalse;
	// ping filter (0 = no limit)
	if ( maxPing > 0 && s->ping > maxPing ) return qfalse;

	return qtrue;
}

void WiredFeeder_RebuildServerDisplayList( void ) {
	serverInfo_t *servers;
	int count, i;
	WiredFeeder_GetServerList( &servers, &count );

	wired_serverDisplayCount = 0;
	for ( i = 0; i < count && wired_serverDisplayCount < MAX_DISPLAY_SERVERS; i++ ) {
		if ( WiredFeeder_ServerPassesFilter( &servers[i] ) ) {
			wired_serverDisplayList[wired_serverDisplayCount++] = i;
		}
	}

	// apply current sort
	if ( wired_serverDisplayCount > 1 ) {
		qsort( wired_serverDisplayList, wired_serverDisplayCount, sizeof( int ), WiredFeeder_ServerSortCompare );
	}

	wired_serverLastRawCount = count;

	// update status cvar for .menu display
	Cvar_Set( "ui_browserStatus", va( "%d servers (%d total)", wired_serverDisplayCount, count ) );
}

static int WiredFeeder_ServerCount( int feederID ) {
	serverInfo_t *servers;
	int count;
	WiredFeeder_GetServerList( &servers, &count );

	// rebuild display list when server count changes (new pings arrived)
	if ( count != wired_serverLastRawCount ) {
		WiredFeeder_RebuildServerDisplayList();
	}

	return wired_serverDisplayCount;
}

static const char *WiredFeeder_ServerItemText( int feederID, int index, int column ) {
	static char buf[256];
	int realIndex;
	serverInfo_t *s;

	// map display index → real server index via sorted display list
	if ( index < 0 || index >= wired_serverDisplayCount ) return "";
	realIndex = wired_serverDisplayList[index];
	s = WiredFeeder_GetServerPtr( realIndex );

	if ( !s ) return "";

	switch ( column ) {
		case 0: return s->hostName;
		case 1: return s->mapName;
		case 2:
			Com_sprintf( buf, sizeof(buf), "%d/%d", s->clients, s->maxClients );
			return buf;
		case 3:
			// game type as short string
			switch ( s->gameType ) {
				case 0:  return "FFA";
				case 1:  return "1v1";
				case 3:  return "KotH";
				case 4:  return "LMS";
				case 5:  return "TDM";
				case 6:  return "CTF";
				default: return "?";
			}
		case 4:
			// color-coded ping: green <200, yellow <400, red >=400
			if ( s->ping < 200 )
				Com_sprintf( buf, sizeof(buf), "^2%d", s->ping );
			else if ( s->ping < 400 )
				Com_sprintf( buf, sizeof(buf), "^3%d", s->ping );
			else
				Com_sprintf( buf, sizeof(buf), "^1%d", s->ping );
			return buf;
		default: return "";
	}
}

static int wired_selectedServer = -1;

// ── server sort ──────────────────────────────────────────────────────
// Sort columns use SORT_HOST..SORT_PUNKBUSTER from ui_public.h

static int wired_serverSortKey = SORT_PING;
static int wired_serverSortDir = 0;  // 0=ascending, 1=descending

static int WiredFeeder_ServerSortCompare( const void *a, const void *b ) {
	serverInfo_t *servers;
	int count;
	int ia = *(const int *)a;
	int ib = *(const int *)b;
	serverInfo_t *sa, *sb;
	int result;

	WiredFeeder_GetServerList( &servers, &count );
	if ( ia < 0 || ia >= count || ib < 0 || ib >= count ) return 0;
	sa = &servers[ia];
	sb = &servers[ib];

	switch ( wired_serverSortKey ) {
		case SORT_HOST:    result = Q_stricmp( sa->hostName, sb->hostName ); break;
		case SORT_MAP:     result = Q_stricmp( sa->mapName, sb->mapName ); break;
		case SORT_CLIENTS: result = sa->clients - sb->clients; break;
		case SORT_PING:    result = sa->ping - sb->ping; break;
		case SORT_GAME:    result = sa->gameType - sb->gameType; break;
		default:           result = 0; break;
	}

	return wired_serverSortDir ? -result : result;
}

void WiredFeeder_SortServers( int column ) {
	serverInfo_t *servers;
	int count, i;

	// toggle direction if clicking same column
	if ( column == wired_serverSortKey ) {
		wired_serverSortDir = !wired_serverSortDir;
	} else {
		wired_serverSortKey = column;
		wired_serverSortDir = 0;
	}

	WiredFeeder_GetServerList( &servers, &count );

	// build display list
	wired_serverDisplayCount = 0;
	for ( i = 0; i < count && i < MAX_DISPLAY_SERVERS; i++ ) {
		wired_serverDisplayList[wired_serverDisplayCount++] = i;
	}

	// sort display list
	if ( wired_serverDisplayCount > 1 ) {
		qsort( wired_serverDisplayList, wired_serverDisplayCount, sizeof( int ), WiredFeeder_ServerSortCompare );
	}
}

// map sort — defined after map data declarations (see below)

static void WiredFeeder_ServerSelection( int feederID, int index ) {
	int realIndex = ( index >= 0 && index < wired_serverDisplayCount ) ? wired_serverDisplayList[index] : index;
	serverInfo_t *s = WiredFeeder_GetServerPtr( realIndex );
	wired_selectedServer = realIndex;
	if ( s ) {
		// store address in cvar so connect action can use it
		Cvar_Set( "ui_selectedServerAddr", NET_AdrToStringwPort( &s->adr ) );
		Cvar_Set( "ui_selectedServerName", s->hostName );
		// update map preview levelshot
		Cvar_Set( "ui_mapLevelshot", va( "levelshots/%s", s->mapName ) );
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
	ext = va( ".dm_%d", PROTOCOL_VERSION );
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

// helper: check if a map name is in a space-separated list (used by pool + favorites)
qboolean WiredFeeder_IsMapInList( const char *list, const char *mapName ) {
	char token[MAX_QPATH];
	const char *p = list;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && i < (int)sizeof(token) - 1 ) token[i++] = *p++;
		token[i] = '\0';
		if ( !Q_stricmp( token, mapName ) ) return qtrue;
	}
	return qfalse;
}

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

	// select first map by default and set levelshot
	if ( wired_mapCount > 0 ) {
		Cvar_Set( "ui_selectedMap", wired_maps[0].mapLoadName );
		Cvar_Set( "ui_mapLevelshot", va( "levelshots/%s", wired_maps[0].mapLoadName ) );
		Cvar_Set( "ui_currentNetMap", "0" );
	}

	// ensure cvars are initialized
	if ( !Cvar_VariableString( "ui_netGameType" )[0] ) {
		Cvar_Set( "ui_netGameType", "0" );
	}
	if ( !Cvar_VariableString( "g_maprotation" )[0] ) {
		Cvar_Set( "g_maprotation", "" );
	}
	Cvar_Set( "ui_mapPoolStatus", "Single map (no rotation)" );
	Cvar_Set( "ui_mapPoolAction", "Add to Pool" );
	Cvar_Set( "ui_favMapAction", "Favorite" );
	// ensure ui_favoriteMaps exists (ARCHIVE so it persists)
	Cvar_Get( "ui_favoriteMaps", "", CVAR_ARCHIVE );

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

static int wired_lastFilterGameType = -1;

static int WiredFeeder_MapCount( int feederID ) {
	// only re-filter when game type changes (preserves sort order)
	int gt = Cvar_VariableIntegerValue( "ui_netGameType" );
	if ( gt != wired_lastFilterGameType ) {
		WiredFeeder_FilterMaps();
		wired_lastFilterGameType = gt;
	}
	return wired_filteredMapCount;
}

static const char *WiredFeeder_MapItemText( int feederID, int index, int column ) {
	static char buf[128];
	int mapIdx;
	if ( index < 0 || index >= wired_filteredMapCount ) return "";
	mapIdx = wired_filteredMaps[index];
	switch ( column ) {
		case 0: {
			// pool queue number
			char rotation[1024];
			char token[MAX_QPATH];
			const char *p;
			int pos = 0;

			Cvar_VariableStringBuffer( "g_maprotation", rotation, sizeof( rotation ) );
			p = rotation;
			while ( *p ) {
				int ti = 0;
				while ( *p == ' ' ) p++;
				if ( !*p ) break;
				while ( *p && *p != ' ' && ti < (int)sizeof(token) - 1 ) token[ti++] = *p++;
				token[ti] = '\0';
				pos++;
				if ( !Q_stricmp( token, wired_maps[mapIdx].mapLoadName ) ) {
					Com_sprintf( buf, sizeof(buf), "^2%d", pos );
					return buf;
				}
			}
			return "";
		}
		case 1:  return wired_maps[mapIdx].mapLoadName;
		case 2:
			if ( Q_stricmp( wired_maps[mapIdx].mapName, wired_maps[mapIdx].mapLoadName ) != 0 ) {
				return wired_maps[mapIdx].mapName;
			}
			return "";
		case 3: {
			// favorite star
			char favs[2048];
			Cvar_VariableStringBuffer( "ui_favoriteMaps", favs, sizeof( favs ) );
			if ( WiredFeeder_IsMapInList( favs, wired_maps[mapIdx].mapLoadName ) ) {
				return "^3*";
			}
			return "";
		}
		default: return "";
	}
}

static int wired_selectedMap = -1;

static void WiredFeeder_MapSelection( int feederID, int index ) {
	if ( index >= 0 && index < wired_filteredMapCount ) {
		int mapIdx = wired_filteredMaps[index];
		wired_selectedMap = mapIdx;
		Cvar_Set( "ui_selectedMap", wired_maps[mapIdx].mapLoadName );
		Cvar_Set( "ui_currentNetMap", va( "%d", mapIdx ) );

		// set levelshot path for map preview (menu reads this cvar)
		Cvar_Set( "ui_mapLevelshot", va( "levelshots/%s", wired_maps[mapIdx].mapLoadName ) );

		// update pool + favorite button states
		{
			extern void WiredUI_UpdateMapPoolButton( void );
			extern void WiredUI_UpdateFavoriteButton( void );
			WiredUI_UpdateMapPoolButton();
			WiredUI_UpdateFavoriteButton();
		}
	}
}

// ── map sort (by column) ─────────────────────────────────────────────

static int wired_mapSortKey = 0;   // 0=name, 1=longname
static int wired_mapSortDir = 0;

static int WiredFeeder_MapSortByColumn( const void *a, const void *b ) {
	int ia = *(const int *)a;
	int ib = *(const int *)b;
	int result;

	switch ( wired_mapSortKey ) {
		case 0: {
			// sort by pool position (pooled maps first, by position)
			char rotation[1024];
			int pa = 999, pb = 999, pos = 0;
			char token[MAX_QPATH];
			const char *p;
			Cvar_VariableStringBuffer( "g_maprotation", rotation, sizeof( rotation ) );
			p = rotation;
			while ( *p ) {
				int ti = 0;
				while ( *p == ' ' ) p++;
				if ( !*p ) break;
				while ( *p && *p != ' ' && ti < (int)sizeof(token) - 1 ) token[ti++] = *p++;
				token[ti] = '\0';
				pos++;
				if ( !Q_stricmp( token, wired_maps[ia].mapLoadName ) ) pa = pos;
				if ( !Q_stricmp( token, wired_maps[ib].mapLoadName ) ) pb = pos;
			}
			result = pa - pb;
			break;
		}
		case 1:  result = Q_stricmp( wired_maps[ia].mapLoadName, wired_maps[ib].mapLoadName ); break;
		case 2:  result = Q_stricmp( wired_maps[ia].mapName, wired_maps[ib].mapName ); break;
		case 3: {
			// sort by favorite status (favorites first)
			char favs[2048];
			qboolean fa, fb;
			Cvar_VariableStringBuffer( "ui_favoriteMaps", favs, sizeof( favs ) );
			fa = WiredFeeder_IsMapInList( favs, wired_maps[ia].mapLoadName );
			fb = WiredFeeder_IsMapInList( favs, wired_maps[ib].mapLoadName );
			result = (int)fb - (int)fa;
			break;
		}
		default: result = Q_stricmp( wired_maps[ia].mapLoadName, wired_maps[ib].mapLoadName ); break;
	}

	return wired_mapSortDir ? -result : result;
}

void WiredFeeder_SortMaps( int column ) {
	if ( column == wired_mapSortKey ) {
		wired_mapSortDir = !wired_mapSortDir;
	} else {
		wired_mapSortKey = column;
		wired_mapSortDir = 0;
	}

	if ( wired_filteredMapCount > 1 ) {
		qsort( wired_filteredMaps, wired_filteredMapCount, sizeof( int ), WiredFeeder_MapSortByColumn );
	}
}

// ── scoreboard feeder ─────────────────────────────────────────────────
// Data source: wiredHud->scores[] (pre-sorted by server rank order)
// FEEDER_SCOREBOARD = all players, FEEDER_REDTEAM/BLUETEAM = team-filtered

static int WiredFeeder_ScoreCount( int feederID ) {
	int i, count = 0;
	int teamFilter = -1;

	if ( feederID == FEEDER_REDTEAM_LIST )  teamFilter = TEAM_RED;
	if ( feederID == FEEDER_BLUETEAM_LIST ) teamFilter = TEAM_BLUE;

	if ( !wiredHud || !wiredHud->valid ) return 0;

	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( teamFilter >= 0 && wiredHud->scores[i].team != teamFilter )
			continue;
		count++;
	}
	return count;
}

static const char *WiredFeeder_ScoreItemText( int feederID, int index, int column ) {
	static char buf[128];
	int i, count = 0;
	int teamFilter = -1;
	wiredHudScore_t *sc;

	if ( feederID == FEEDER_REDTEAM_LIST )  teamFilter = TEAM_RED;
	if ( feederID == FEEDER_BLUETEAM_LIST ) teamFilter = TEAM_BLUE;

	if ( !wiredHud || !wiredHud->valid ) return "";

	// find the Nth matching entry
	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( teamFilter >= 0 && wiredHud->scores[i].team != teamFilter )
			continue;
		if ( count == index ) break;
		count++;
	}
	if ( i >= wiredHud->numScores || i >= WIRED_HUD_MAX_SCORES ) return "";
	sc = &wiredHud->scores[i];

	switch ( column ) {
		case 0: // name
			if ( sc->client >= 0 && sc->client < WIRED_HUD_MAX_CLIENTS
				 && wiredHud->clients[sc->client].infoValid )
				return wiredHud->clients[sc->client].name;
			return "???";
		case 1: // score
			Com_sprintf( buf, sizeof(buf), "%d", sc->score );
			return buf;
		case 2: // ping
			if ( sc->ping == -1 ) return "...";
			Com_sprintf( buf, sizeof(buf), "%d", sc->ping );
			return buf;
		case 3: // time
			Com_sprintf( buf, sizeof(buf), "%d", sc->time );
			return buf;
		case 4: // accuracy
			Com_sprintf( buf, sizeof(buf), "%d%%", sc->accuracy );
			return buf;
		default: return "";
	}
}

static int wired_selectedScore = -1;

static void WiredFeeder_ScoreSelection( int feederID, int index ) {
	wired_selectedScore = index;
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
	WiredUI_RegisterFeeder( FEEDER_SCOREBOARD, WiredFeeder_ScoreCount,
		WiredFeeder_ScoreItemText, WiredFeeder_ScoreSelection );
	WiredUI_RegisterFeeder( FEEDER_REDTEAM_LIST, WiredFeeder_ScoreCount,
		WiredFeeder_ScoreItemText, WiredFeeder_ScoreSelection );
	WiredUI_RegisterFeeder( FEEDER_BLUETEAM_LIST, WiredFeeder_ScoreCount,
		WiredFeeder_ScoreItemText, WiredFeeder_ScoreSelection );

	// load initial data
	WiredFeeder_LoadMaps();
	WiredFeeder_LoadDemos();
	WiredFeeder_LoadMods();

	Com_Printf( "WiredUI: feeders registered (demos=%d, mods=%d)\n",
		wired_demoCount, wired_modCount );
}

#endif // FEAT_WIRED_UI
