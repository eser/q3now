/*
cl_wired_feeders.c — Wired UI feeder implementations
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_hud.h"
#include "../../../qcommon/menudef.h"
#include "../../../qcommon/maps/meta.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

// forward declarations
static int WiredFeeder_ServerSortCompare( const void *a, const void *b );

#define WiredFeeder_StateSetString WiredUI_StateSetString
#define WiredFeeder_StateSetInt WiredUI_StateSetInt
#define WiredFeeder_StateGetString WiredUI_StateGetString

static int WiredFeeder_StateGetInt( const char *key, int defaultValue ) {
	char buf[64];
	WiredUI_StateGetString( key, buf, sizeof( buf ) );
	if ( !buf[0] ) return defaultValue;
	return atoi( buf );
}

// ── server browser feeder ─────────────────────────────────────────────
// Data source: cls.globalServers[], cls.localServers[], cls.favoriteServers[]
// Controlled by ui_netSource cvar (0=local, 1-5=internet masters, 6=favorites)

// server display list — indices into the source array, sorted/filtered
#define MAX_DISPLAY_SERVERS  4096
static int  wui_serverDisplayList[MAX_DISPLAY_SERVERS];
static int  wui_serverDisplayCount = 0;

// Map ui_netSource cvar (0=local, 1=internet, 6=favorites) to engine arrays
static void WiredFeeder_GetServerList( serverInfo_t **servers, int *count ) {
	int uiSource = WiredFeeder_StateGetInt( "ui_netSource", 0 );

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
	serverInfo_t *servers = NULL;
	int count = 0;

	WiredFeeder_GetServerList( &servers, &count );

	if ( index < 0 || index >= count ) return NULL;
	return &servers[index];
}

static int wui_serverLastRawCount = 0;  // track raw count for rebuild detection

static qboolean WiredFeeder_ServerPassesFilter( serverInfo_t *s ) {
	int filterGameType = WiredFeeder_StateGetInt( "ui_browserGameType", 0 );
	int showFull      = WiredFeeder_StateGetInt( "ui_browserShowFull", 1 );
	int showEmpty     = WiredFeeder_StateGetInt( "ui_browserShowEmpty", 1 );
	int maxPing       = WiredFeeder_StateGetInt( "ui_browserMaxPing", 0 );

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
	int count;
	WiredFeeder_GetServerList( &servers, &count );

	wui_serverDisplayCount = 0;
	for ( int i = 0; i < count && wui_serverDisplayCount < MAX_DISPLAY_SERVERS; i++ ) {
		if ( WiredFeeder_ServerPassesFilter( &servers[i] ) ) {
			wui_serverDisplayList[wui_serverDisplayCount++] = i;
		}
	}

	// apply current sort
	if ( wui_serverDisplayCount > 1 ) {
		qsort( wui_serverDisplayList, wui_serverDisplayCount, sizeof( int ), WiredFeeder_ServerSortCompare );
	}

	wui_serverLastRawCount = count;

	// update status cvar for .menu display
	WiredFeeder_StateSetString( "ui_browserStatus", va( "%d servers (%d total)", wui_serverDisplayCount, count ) );
}

static int WiredFeeder_ServerCount( int feederID ) {
	serverInfo_t *servers = NULL;
	int count = 0;
	WiredFeeder_GetServerList( &servers, &count );

	// rebuild display list when server count changes (new pings arrived)
	if ( count != wui_serverLastRawCount ) {
		WiredFeeder_RebuildServerDisplayList();
	}

	return wui_serverDisplayCount;
}

static const char *WiredFeeder_ServerItemText( int feederID, int index, int column ) {
	static char buf[256];

	// map display index → real server index via sorted display list
	if ( index < 0 || index >= wui_serverDisplayCount ) return "";
	int realIndex = wui_serverDisplayList[index];
	serverInfo_t *s = WiredFeeder_GetServerPtr( realIndex );

	if ( !s ) return "";

	switch ( column ) {
		case 0: return s->hostName;
		case 1: return s->mapName;
		case 2:
			Com_sprintf( buf, sizeof(buf), "%d/%d", s->clients, s->maxClients );
			return buf;
		case 3:
			// game type as short string
			if ( s->gameType >= 0 && s->gameType < 16 /* safe gametype bound */ ) {
				return bg_gametypelist[s->gameType].shortname;
			}
			return "?";
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

static int wui_selectedServer = -1;

// ── server sort ──────────────────────────────────────────────────────
// Sort columns use SORT_HOST..SORT_PUNKBUSTER from ui_public.h

static int wui_serverSortKey = SORT_PING;
static int wui_serverSortDir = 0;  // 0=ascending, 1=descending

static int WiredFeeder_ServerSortCompare( const void *a, const void *b ) {
	serverInfo_t *servers = NULL;
	int count = 0;
	int ia = *(const int *)a;
	int ib = *(const int *)b;

	WiredFeeder_GetServerList( &servers, &count );
	if ( ia < 0 || ia >= count || ib < 0 || ib >= count ) return 0;
	serverInfo_t *sa = &servers[ia];
	serverInfo_t *sb = &servers[ib];
	int result;

	switch ( wui_serverSortKey ) {
		case SORT_HOST:    result = Q_stricmp( sa->hostName, sb->hostName ); break;
		case SORT_MAP:     result = Q_stricmp( sa->mapName, sb->mapName ); break;
		case SORT_CLIENTS: result = sa->clients - sb->clients; break;
		case SORT_PING:    result = sa->ping - sb->ping; break;
		case SORT_GAME:    result = sa->gameType - sb->gameType; break;
		default:           result = 0; break;
	}

	return wui_serverSortDir ? -result : result;
}

void WiredFeeder_SortServers( int column ) {
	serverInfo_t *servers = NULL;
	int count = 0;

	// toggle direction if clicking same column
	if ( column == wui_serverSortKey ) {
		wui_serverSortDir = !wui_serverSortDir;
	} else {
		wui_serverSortKey = column;
		wui_serverSortDir = 0;
	}

	WiredFeeder_GetServerList( &servers, &count );

	// build display list
	wui_serverDisplayCount = 0;
	for ( int i = 0; i < count && i < MAX_DISPLAY_SERVERS; i++ ) {
		wui_serverDisplayList[wui_serverDisplayCount++] = i;
	}

	// sort display list
	if ( wui_serverDisplayCount > 1 ) {
		qsort( wui_serverDisplayList, wui_serverDisplayCount, sizeof( int ), WiredFeeder_ServerSortCompare );
	}

	// export sort state to cvars for menu sort-direction indicators
	WiredFeeder_StateSetInt( "ui_serverSortKey", wui_serverSortKey );
	WiredFeeder_StateSetInt( "ui_serverSortDir", wui_serverSortDir );

	// set per-column indicator cvars (arrow for active column, empty for others)
	{
		const char *arrow = wui_serverSortDir ? "v" : "^";
		for ( int col = 0; col < 5; col++ ) {
			WiredFeeder_StateSetString( va( "ui_sortInd%d", col ),
				( col == wui_serverSortKey ) ? arrow : "" );
		}
	}
}

// map sort — defined after map data declarations (see below)

static void WiredFeeder_ServerSelection( int feederID, int index ) {
	int realIndex = ( index >= 0 && index < wui_serverDisplayCount ) ? wui_serverDisplayList[index] : index;
	serverInfo_t *s = WiredFeeder_GetServerPtr( realIndex );
	wui_selectedServer = realIndex;
	if ( s ) {
		// store address in cvar so connect action can use it
		WiredFeeder_StateSetString( "ui_selectedServerAddr", NET_AdrToStringwPort( &s->adr ) );
		WiredFeeder_StateSetString( "ui_selectedServerName", s->hostName );
		// update map preview levelshot
		WiredFeeder_StateSetString( "ui_mapLevelshot", va( "levelshots/%s", s->mapName ) );
	}
}

// ── demo feeder ───────────────────────────────────────────────────────
// Data source: FS_GetFileList("demos", extension)

#define MAX_WIRED_DEMOS  512
static char  wui_demoList[MAX_WIRED_DEMOS][MAX_QPATH];
static int   wui_demoCount = 0;

void WiredFeeder_LoadDemos( void ) {
	char listBuf[8192];

	wui_demoCount = 0;

	// try current protocol demo extension
	const char *ext = va( ".dm_%d", PROTOCOL_VERSION );
	int numFiles = FS_GetFileList( "demos", ext, listBuf, sizeof( listBuf ) );

	char *namePtr = listBuf;
	for ( int i = 0; i < numFiles && wui_demoCount < MAX_WIRED_DEMOS; i++ ) {
		Q_strncpyz( wui_demoList[wui_demoCount], namePtr, sizeof( wui_demoList[0] ) );
		// strip extension for display
		char *dot = strrchr( wui_demoList[wui_demoCount], '.' );
		if ( dot ) *dot = '\0';
		wui_demoCount++;
		namePtr += strlen( namePtr ) + 1;
	}
}

static int WiredFeeder_DemoCount( int feederID ) {
	return wui_demoCount;
}

static const char *WiredFeeder_DemoItemText( int feederID, int index, int column ) {
	if ( index < 0 || index >= wui_demoCount ) return "";
	return wui_demoList[index];
}

static int wui_selectedDemo = -1;

static void WiredFeeder_DemoSelection( int feederID, int index ) {
	wui_selectedDemo = index;
	if ( index >= 0 && index < wui_demoCount ) {
		WiredFeeder_StateSetString( "ui_selectedDemo", wui_demoList[index] );
	}
}

// ── mod feeder ────────────────────────────────────────────────────────
// Data source: FS_GetFileList("$modlist", "")

#define MAX_WIRED_MODS  64
static char  wui_modList[MAX_WIRED_MODS][MAX_QPATH];
static char  wui_modDesc[MAX_WIRED_MODS][256];
static int   wui_modCount = 0;

void WiredFeeder_LoadMods( void ) {
	char listBuf[4096];

	wui_modCount = 0;

	// first entry is always the base game; description sources the product
	// name from cl_gamename (set by the cgame at init via GAMENAME_FOR_MASTER).
	// Engine code stays product-agnostic — falls back to "Wired" before init.
	Q_strncpyz( wui_modList[0], "baseq3", sizeof( wui_modList[0] ) );
	{
		const char *gamename = Cvar_VariableString( "cl_gamename" );
		if ( gamename && *gamename ) {
			Com_sprintf( wui_modDesc[0], sizeof( wui_modDesc[0] ), "%s (base game)", gamename );
		} else {
			Q_strncpyz( wui_modDesc[0], "Wired (base game)", sizeof( wui_modDesc[0] ) );
		}
	}
	wui_modCount = 1;

	int numDirs = FS_GetFileList( "$modlist", "", listBuf, sizeof( listBuf ) );

	char *dirPtr = listBuf;
	for ( int i = 0; i < numDirs && wui_modCount < MAX_WIRED_MODS; i++ ) {
		if ( dirPtr[0] && Q_stricmp( dirPtr, "baseq3" ) ) {
			Q_strncpyz( wui_modList[wui_modCount], dirPtr, sizeof( wui_modList[0] ) );
			// try to read description.txt
			wui_modDesc[wui_modCount][0] = '\0';
			{
				fileHandle_t f;
				int len = FS_FOpenFileRead( va( "%s/description.txt", dirPtr ), &f, qfalse );
				if ( len > 0 && f != FS_INVALID_HANDLE ) {
					if ( len >= (int)sizeof( wui_modDesc[0] ) ) len = sizeof( wui_modDesc[0] ) - 1;
					FS_Read( wui_modDesc[wui_modCount], len, f );
					wui_modDesc[wui_modCount][len] = '\0';
					FS_FCloseFile( f );
				} else {
					Q_strncpyz( wui_modDesc[wui_modCount], dirPtr, sizeof( wui_modDesc[0] ) );
					if ( f != FS_INVALID_HANDLE ) FS_FCloseFile( f );
				}
			}
			wui_modCount++;
		}
		dirPtr += strlen( dirPtr ) + 1;
	}
}

static int WiredFeeder_ModCount( int feederID ) {
	return wui_modCount;
}

static const char *WiredFeeder_ModItemText( int feederID, int index, int column ) {
	if ( index < 0 || index >= wui_modCount ) return "";
	switch ( column ) {
		case 0:  return wui_modList[index];
		case 1:  return wui_modDesc[index];
		default: return wui_modList[index];
	}
}

static int wui_selectedMod = -1;

static void WiredFeeder_ModSelection( int feederID, int index ) {
	wui_selectedMod = index;
	if ( index >= 0 && index < wui_modCount ) {
		WiredFeeder_StateSetString( "ui_selectedMod", wui_modList[index] );
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

typedef struct {
	char    mapLoadName[MAX_QPATH];
	char    mapName[64];
	int     typeBits;                   /* bitmask of supported gametype indices */
} wiredMapInfo_t;

static wiredMapInfo_t  wui_maps[MAX_WIRED_MAPS];
static int             wui_mapCount = 0;
static int             wui_filteredMaps[MAX_WIRED_MAPS];
static int             wui_filteredMapCount = 0;

// Phase 5 (q3now meta migration): WiredFeeder_LoadArenaFile and
// WiredFeeder_ParseArenaBuffer were removed when the UI map roster
// was migrated to the .meta-driven maps_list[] global. The legacy
// .arena fallback still happens — but inside Maps_LoadMetaFor, not
// here. UI consumers only ever see normalized map_meta_t, projected
// into wui_maps[] for the existing widget surface.

static int WiredFeeder_MapSortCompare( const void *a, const void *b ) {
	const wiredMapInfo_t *ma = (const wiredMapInfo_t *)a;
	const wiredMapInfo_t *mb = (const wiredMapInfo_t *)b;
	return Q_stricmp( ma->mapLoadName, mb->mapLoadName );
}

void WiredFeeder_LoadMaps( void ) {
	wui_mapCount = 0;

	// Source of truth: maps_list[] (owned by code/qcommon/maps).
	// Already populated at engine init and on every FS_Restart; we
	// trigger a refresh here as well so the UI reflects content
	// changes that happened while the menu was hidden.
	Maps_ScanAll();

	const int n = ( maps_count < MAX_WIRED_MAPS ) ? maps_count : MAX_WIRED_MAPS;
	for ( int i = 0; i < n; i++ ) {
		const map_meta_t *m = &maps_list[i];
		wiredMapInfo_t   *o = &wui_maps[i];

		Q_strncpyz( o->mapLoadName, m->mapname, sizeof( o->mapLoadName ) );

		// Defensive consumer rule: empty longname → display the load name.
		Q_strncpyz( o->mapName,
			m->longname[0] ? m->longname : m->mapname,
			sizeof( o->mapName ) );

		// Empty type list → permissive fallback: support every
		// gametype (per the file-optionality invariant table).
		// Otherwise OR in the bits each canonical token resolves to.
		// BG_GametypeBits accepts both legacy ("ffa", "tourney") and
		// canonical ("dm", "duel") tokens, so the canonical tokens
		// from m->type.tokens go through unchanged.
		if ( m->type.count == 0 ) {
			o->typeBits = (1 << 0) | (1 << 1) | (1 << 2) |
			              (1 << 3) | (1 << 4) | (1 << 5);
		} else {
			int bits = 0;
			for ( int t = 0; t < m->type.count; t++ ) {
				bits |= BG_GametypeBits( m->type.tokens[t] );
			}
			o->typeBits = bits;
		}
	}
	wui_mapCount = n;

	qsort( wui_maps, wui_mapCount, sizeof( wiredMapInfo_t ), WiredFeeder_MapSortCompare );

	wui_filteredMapCount = wui_mapCount;
	for ( int i = 0; i < wui_mapCount; i++ ) {
		wui_filteredMaps[i] = i;
	}

	if ( wui_mapCount > 0 ) {
		WiredFeeder_StateSetString( "ui_selectedMap", wui_maps[0].mapLoadName );
		WiredFeeder_StateSetString( "ui_mapLevelshot", va( "levelshots/%s", wui_maps[0].mapLoadName ) );
		WiredFeeder_StateSetString( "ui_currentNetMap", "0" );
	}

	if ( WiredFeeder_StateGetInt( "ui_netGameType", -1 ) < 0 ) {
		WiredFeeder_StateSetString( "ui_netGameType", "0" );
	}
	if ( !Cvar_VariableString( "g_maprotation" )[0] ) {
		Cvar_Set( "g_maprotation", "" );
	}
	WiredFeeder_StateSetString( "ui_mapPoolStatus", "Single map (no rotation)" );
	WiredFeeder_StateSetString( "ui_mapPoolAction", "Add to Pool" );
	WiredFeeder_StateSetString( "ui_favMapAction", "Favorite" );
	WiredFeeder_StateSetString( "ui_favoriteMaps", "" );

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: %d maps loaded from maps_list[]\n", wui_mapCount );
}

static void WiredFeeder_FilterMaps( void ) {
	int gameType = WiredFeeder_StateGetInt( "ui_netGameType", 0 );
	int typeBit = ( 1 << gameType );

	wui_filteredMapCount = 0;
	for ( int i = 0; i < wui_mapCount; i++ ) {
		if ( wui_maps[i].typeBits & typeBit ) {
			wui_filteredMaps[wui_filteredMapCount++] = i;
		}
	}
}

static int wui_lastFilterGameType = -1;

static int WiredFeeder_MapCount( int feederID ) {
	// only re-filter when game type changes (preserves sort order)
	int gt = WiredFeeder_StateGetInt( "ui_netGameType", 0 );
	if ( gt != wui_lastFilterGameType ) {
		WiredFeeder_FilterMaps();
		wui_lastFilterGameType = gt;
	}
	return wui_filteredMapCount;
}

static const char *WiredFeeder_MapItemText( int feederID, int index, int column ) {
	static char buf[128];
	if ( index < 0 || index >= wui_filteredMapCount ) return "";
	int mapIdx = wui_filteredMaps[index];
	switch ( column ) {
		case 0: {
			// pool queue number
			char rotation[1024];
			char token[MAX_QPATH];
			const char *p;
			int pos = 0;

			WiredUI_GetMapRotation( rotation, sizeof( rotation ) );
			p = rotation;
			while ( *p ) {
				int ti = 0;
				while ( *p == ' ' ) p++;
				if ( !*p ) break;
				while ( *p && *p != ' ' && ti < (int)sizeof(token) - 1 ) token[ti++] = *p++;
				token[ti] = '\0';
				pos++;
				if ( !Q_stricmp( token, wui_maps[mapIdx].mapLoadName ) ) {
					Com_sprintf( buf, sizeof(buf), "^2%d", pos );
					return buf;
				}
			}
			return "";
		}
		case 1:  return wui_maps[mapIdx].mapLoadName;
		case 2:
			if ( Q_stricmp( wui_maps[mapIdx].mapName, wui_maps[mapIdx].mapLoadName ) != 0 ) {
				return wui_maps[mapIdx].mapName;
			}
			return "";
		case 3: {
			// favorite star
			char favs[2048];
			WiredFeeder_StateGetString( "ui_favoriteMaps", favs, sizeof( favs ) );
			if ( WiredFeeder_IsMapInList( favs, wui_maps[mapIdx].mapLoadName ) ) {
				return "^3*";
			}
			return "";
		}
		default: return "";
	}
}

static int wui_selectedMap = -1;

static void WiredFeeder_MapSelection( int feederID, int index ) {
	if ( index >= 0 && index < wui_filteredMapCount ) {
		int mapIdx = wui_filteredMaps[index];
		wui_selectedMap = mapIdx;
		WiredFeeder_StateSetString( "ui_selectedMap", wui_maps[mapIdx].mapLoadName );
		WiredFeeder_StateSetString( "ui_currentNetMap", va( "%d", mapIdx ) );

		// set levelshot path for map preview (menu reads this cvar)
		WiredFeeder_StateSetString( "ui_mapLevelshot", va( "levelshots/%s", wui_maps[mapIdx].mapLoadName ) );

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

static int wui_mapSortKey = 0;   // 0=name, 1=longname
static int wui_mapSortDir = 0;

static int WiredFeeder_MapSortByColumn( const void *a, const void *b ) {
	int ia = *(const int *)a;
	int ib = *(const int *)b;
	int result = 0;

	switch ( wui_mapSortKey ) {
		case 0: {
			// sort by pool position (pooled maps first, by position)
			char rotation[1024];
			int pa = 999, pb = 999, pos = 0;
			char token[MAX_QPATH];
			const char *p;
			WiredUI_GetMapRotation( rotation, sizeof( rotation ) );
			p = rotation;
			while ( *p ) {
				int ti = 0;
				while ( *p == ' ' ) p++;
				if ( !*p ) break;
				while ( *p && *p != ' ' && ti < (int)sizeof(token) - 1 ) token[ti++] = *p++;
				token[ti] = '\0';
				pos++;
				if ( !Q_stricmp( token, wui_maps[ia].mapLoadName ) ) pa = pos;
				if ( !Q_stricmp( token, wui_maps[ib].mapLoadName ) ) pb = pos;
			}
			result = pa - pb;
			break;
		}
		case 1:  result = Q_stricmp( wui_maps[ia].mapLoadName, wui_maps[ib].mapLoadName ); break;
		case 2:  result = Q_stricmp( wui_maps[ia].mapName, wui_maps[ib].mapName ); break;
		case 3: {
			// sort by favorite status (favorites first)
			char favs[2048];
			qboolean fa, fb;
			WiredFeeder_StateGetString( "ui_favoriteMaps", favs, sizeof( favs ) );
			fa = WiredFeeder_IsMapInList( favs, wui_maps[ia].mapLoadName );
			fb = WiredFeeder_IsMapInList( favs, wui_maps[ib].mapLoadName );
			result = (int)fb - (int)fa;
			break;
		}
		default: result = Q_stricmp( wui_maps[ia].mapLoadName, wui_maps[ib].mapLoadName ); break;
	}

	return wui_mapSortDir ? -result : result;
}

void WiredFeeder_SortMaps( int column ) {
	if ( column == wui_mapSortKey ) {
		wui_mapSortDir = !wui_mapSortDir;
	} else {
		wui_mapSortKey = column;
		wui_mapSortDir = 0;
	}

	if ( wui_filteredMapCount > 1 ) {
		qsort( wui_filteredMaps, wui_filteredMapCount, sizeof( int ), WiredFeeder_MapSortByColumn );
	}
}

// ── scoreboard feeder ─────────────────────────────────────────────────
// Data source: wiredHud->scores[] (pre-sorted by server rank order)
// FEEDER_SCOREBOARD = all players, FEEDER_REDTEAM/BLUETEAM = team-filtered

static int WiredFeeder_ScoreCount( int feederID ) {
	int count = 0;
	int teamFilter = -1;

	if ( feederID == 0x05 /* red team feeder */ )  teamFilter = 1 /* red */;
	if ( feederID == 0x06 /* blue team feeder */ ) teamFilter = 2 /* blue */;

	if ( !wiredHud || !wiredHud->valid ) return 0;

	for ( int i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( teamFilter >= 0 && wiredHud->scores[i].team != teamFilter )
			continue;
		count++;
	}
	return count;
}

static const char *WiredFeeder_ScoreItemText( int feederID, int index, int column ) {
	static char buf[128];
	int count = 0;
	int teamFilter = -1;

	if ( feederID == 0x05 /* red team feeder */ )  teamFilter = 1 /* red */;
	if ( feederID == 0x06 /* blue team feeder */ ) teamFilter = 2 /* blue */;

	if ( !wiredHud || !wiredHud->valid ) return "";

	// find the Nth matching entry
	int i;
	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( teamFilter >= 0 && wiredHud->scores[i].team != teamFilter )
			continue;
		if ( count == index ) break;
		count++;
	}
	if ( i >= wiredHud->numScores || i >= WIRED_HUD_MAX_SCORES ) return "";
	wiredHudScore_t *sc = &wiredHud->scores[i];

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

static int wui_selectedScore = -1;

static void WiredFeeder_ScoreSelection( int feederID, int index ) {
	wui_selectedScore = index;
}

// ── character feeder (FEEDER_CHARACTERS) ──────────────────────────────

static int  wui_charSelected = -1;
static int  wui_skinSelected = -1;

// Pick a skin index for the given character, preserving the current "skin" cvar
// if it still matches one of the character's skins; otherwise fall back to the
// first available skin. Returns -1 only if the character has no skins at all.
static int WiredFeeder_PickSkinForChar( const clCharacterEntry_t *e ) {
	char skinBuf[CM_SKIN_NAME_LEN];
	int i;

	if ( !e || e->manifest.numSkins <= 0 ) return -1;

	Cvar_VariableStringBuffer( "skin", skinBuf, sizeof( skinBuf ) );
	if ( skinBuf[0] ) {
		for ( i = 0; i < e->manifest.numSkins; i++ ) {
			if ( !Q_stricmp( e->manifest.skins[i].name, skinBuf ) ) {
				return i;
			}
		}
	}
	return 0;
}

void WiredFeeder_LoadCharacters( void ) {
	int count = CL_Characters_Count();

	// match current char cvar to selection
	char charBuf[64];
	Cvar_VariableStringBuffer( "char", charBuf, sizeof( charBuf ) );
	wui_charSelected = -1;
	for ( int i = 0; i < count; i++ ) {
		const clCharacterEntry_t *e = CL_Characters_At( i );
		if ( e && !Q_stricmp( e->dirname, charBuf ) ) {
			wui_charSelected = i;
			break;
		}
	}

	// align the skin selection with whatever the current skin cvar is
	wui_skinSelected = WiredFeeder_PickSkinForChar( CL_Characters_At( wui_charSelected ) );

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: found %d characters\n", count );
}

static int WiredFeeder_CharactersCount( int feederID ) {
	return CL_Characters_Count();
}

static const char *WiredFeeder_CharactersItemText( int feederID, int index, int column ) {
	const clCharacterEntry_t *e = CL_Characters_At( index );
	if ( !e ) return "";
	return e->manifest.displayName[0] ? e->manifest.displayName : e->dirname;
}

static qhandle_t WiredFeeder_CharactersItemIcon( int feederID, int index ) {
	const clCharacterEntry_t *e = CL_Characters_At( index );
	return e ? e->iconHandle : 0;
}

static void WiredFeeder_CharactersSelection( int feederID, int index ) {
	const clCharacterEntry_t *e = CL_Characters_At( index );
	int skinIdx;
	if ( !e ) return;
	wui_charSelected = index;
	Cvar_Set( "char", e->dirname );

	skinIdx = WiredFeeder_PickSkinForChar( e );
	if ( skinIdx >= 0 ) {
		wui_skinSelected = skinIdx;
		Cvar_Set( "skin", e->manifest.skins[skinIdx].name );
	} else {
		wui_skinSelected = -1;
		Cvar_Set( "skin", "default" );
	}
}

// ── skin feeder (FEEDER_SKINS) ───────────────────────────────────────
// Lists the skins of the currently selected character. Reflects wui_charSelected,
// which is updated by the characters feeder; if no character is selected the list is empty.

static int WiredFeeder_SkinsCount( int feederID ) {
	const clCharacterEntry_t *e = CL_Characters_At( wui_charSelected );
	return e ? e->manifest.numSkins : 0;
}

static const char *WiredFeeder_SkinsItemText( int feederID, int index, int column ) {
	const clCharacterEntry_t *e = CL_Characters_At( wui_charSelected );
	if ( !e || index < 0 || index >= e->manifest.numSkins ) return "";
	return e->manifest.skins[index].name;
}

static void WiredFeeder_SkinsSelection( int feederID, int index ) {
	const clCharacterEntry_t *e = CL_Characters_At( wui_charSelected );
	if ( !e || index < 0 || index >= e->manifest.numSkins ) return;
	wui_skinSelected = index;
	Cvar_Set( "skin", e->manifest.skins[index].name );
}

int WiredFeeder_GetCharacterCount( void ) { return CL_Characters_Count(); }
int WiredFeeder_GetCharacterSelected( void ) { return wui_charSelected; }
const char *WiredFeeder_GetCharacterName( int index ) {
	const clCharacterEntry_t *e = CL_Characters_At( index );
	return e ? e->dirname : NULL;
}
void WiredFeeder_SetCharacterSelected( int index ) {
	WiredFeeder_CharactersSelection( FEEDER_CHARACTERS, index );
}

// ── feeder registration ───────────────────────────────────────────────

void WiredUI_RegisterCoreFeeders( void ) {
	WiredUI_RegisterFeeder( FEEDER_SERVERS, "servers", WiredFeeder_ServerCount,
		WiredFeeder_ServerItemText, WiredFeeder_ServerSelection );
	WiredUI_RegisterFeeder( FEEDER_MAPS, "maps", WiredFeeder_MapCount,
		WiredFeeder_MapItemText, WiredFeeder_MapSelection );
	WiredUI_RegisterFeeder( FEEDER_ALLMAPS, "allmaps", WiredFeeder_MapCount,
		WiredFeeder_MapItemText, WiredFeeder_MapSelection );
	WiredUI_RegisterFeeder( FEEDER_DEMOS, "demos", WiredFeeder_DemoCount,
		WiredFeeder_DemoItemText, WiredFeeder_DemoSelection );
	WiredUI_RegisterFeeder( FEEDER_MODS, "mods", WiredFeeder_ModCount,
		WiredFeeder_ModItemText, WiredFeeder_ModSelection );
	WiredUI_RegisterFeeder( FEEDER_SCOREBOARD, "scoreboard", WiredFeeder_ScoreCount,
		WiredFeeder_ScoreItemText, WiredFeeder_ScoreSelection );
	WiredUI_RegisterFeeder( FEEDER_REDTEAM_LIST, "players_red_team", WiredFeeder_ScoreCount,
		WiredFeeder_ScoreItemText, WiredFeeder_ScoreSelection );
	WiredUI_RegisterFeeder( FEEDER_BLUETEAM_LIST, "players_blue_team", WiredFeeder_ScoreCount,
		WiredFeeder_ScoreItemText, WiredFeeder_ScoreSelection );
	WiredUI_RegisterFeeder( FEEDER_CHARACTERS, "characters", WiredFeeder_CharactersCount,
		WiredFeeder_CharactersItemText, WiredFeeder_CharactersSelection );
	WiredUI_RegisterFeederIcon( FEEDER_CHARACTERS, WiredFeeder_CharactersItemIcon );
	WiredUI_RegisterFeeder( FEEDER_SKINS, "skins", WiredFeeder_SkinsCount,
		WiredFeeder_SkinsItemText, WiredFeeder_SkinsSelection );

	// load initial data
	WiredFeeder_LoadMaps();
	WiredFeeder_LoadDemos();
	WiredFeeder_LoadMods();
	WiredFeeder_LoadCharacters();

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: feeders registered (demos=%d, mods=%d)\n",
		wui_demoCount, wui_modCount );
}

#endif // FEAT_WIRED_UI
