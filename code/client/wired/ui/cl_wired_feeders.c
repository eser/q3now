// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_feeders.c — Wired UI feeder implementations
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_ui_hud_state.h"
#include "../../../qcommon/menudef.h"
#include "../../../qcommon/maps/meta.h"
#include <inttypes.h>
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

// forward declarations
static int WiredFeeder_ServerSortCompare( const void *a, const void *b );
void WiredFeeder_RebuildServerDisplayList( void );

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
static int  wui_serverListGeneration = 0;
static int  wui_serverSelectionGeneration = 0;

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

static int wui_serverLastRawCount = -1;  // track raw count for rebuild detection
static int wui_serverLastSource = -1;
static unsigned wui_serverLastSignature = 0;
static unsigned wui_serverLastEngineGeneration = 0;
static int wui_selectedServer = -1;
static int wui_selectedServerSource = -1;
static char wui_selectedServerAddress[MAX_STRING_CHARS];
static qboolean wui_serverFixtureActive = qfalse;

static void WiredFeeder_ClearServerSelection( void ) {
	wui_selectedServer = -1;
	wui_selectedServerSource = -1;
	wui_selectedServerAddress[0] = '\0';
	wui_serverSelectionGeneration++;
	WiredFeeder_StateSetString( "ui_selectedServerAddr", "" );
	WiredFeeder_StateSetString( "ui_selectedServerName", "" );
}

static void WiredFeeder_SanitizeRemoteText( const char *input, char *output, size_t outputSize ) {
	size_t used = 0;
	if ( outputSize == 0 ) return;
	for ( const unsigned char *p = (const unsigned char *)( input ? input : "" ); *p && used + 1 < outputSize; p++ ) {
		if ( p[0] == '^' && p[1] && p[1] >= '0' && p[1] <= '9' ) {
			p++;
			continue;
		}
		output[used++] = ( *p < 32 || *p == 127 ) ? ' ' : (char)*p;
	}
	output[used] = '\0';
}

static unsigned WiredFeeder_ServerSignature( const serverInfo_t *servers, int count ) {
	unsigned hash = 2166136261u;
	for ( int i = 0; i < count; i++ ) {
		char address[MAX_STRING_CHARS];
		const unsigned char *p;
		Q_strncpyz( address, NET_AdrToStringwPort( &servers[i].adr ), sizeof( address ) );
		for ( p = (const unsigned char *)address; *p; p++ ) hash = ( hash ^ *p ) * 16777619u;
		for ( p = (const unsigned char *)servers[i].hostName; *p; p++ ) hash = ( hash ^ *p ) * 16777619u;
		for ( p = (const unsigned char *)servers[i].mapName; *p; p++ ) hash = ( hash ^ *p ) * 16777619u;
		hash = ( hash ^ (unsigned)servers[i].clients ) * 16777619u;
		hash = ( hash ^ (unsigned)servers[i].maxClients ) * 16777619u;
		hash = ( hash ^ (unsigned)servers[i].gameType ) * 16777619u;
		hash = ( hash ^ (unsigned)servers[i].ping ) * 16777619u;
	}
	return hash;
}

qboolean WiredFeeder_ServerFixtureActive( void ) {
	return wui_serverFixtureActive;
}

qboolean WiredFeeder_ServerFixtureInstall( int sentinelPort, int targetPort,
	qboolean targetNeedsPassword ) {
	serverInfo_t *target;
	serverInfo_t *sentinel;
	if ( sentinelPort < 1 || sentinelPort > 65535 || targetPort < 1 || targetPort > 65535
	     || sentinelPort == targetPort ) return qfalse;
	memset( cls.localServers, 0, sizeof( cls.localServers ) );
	/* Deliberately make raw order the reverse of host-sort order.  The gate's
	 * display-row 1 must therefore map back to raw row 0. */
	target = &cls.localServers[0];
	target->adr.type = NA_IP;
	target->adr.ipv._4[0] = 127; target->adr.ipv._4[1] = 0;
	target->adr.ipv._4[2] = 0; target->adr.ipv._4[3] = 1;
	target->adr.port = BigShort( (short)targetPort );
	Q_strncpyz( target->hostName, "Z0 WIRED Q0 TARGET", sizeof( target->hostName ) );
	Q_strncpyz( target->mapName, "arena7", sizeof( target->mapName ) );
	Q_strncpyz( target->game, "q3now", sizeof( target->game ) );
	target->clients = 1; target->maxClients = 8; target->gameType = 0;
	target->g_needpass = targetNeedsPassword ? 1 : 0;
	target->ping = 17; target->visible = qtrue;

	sentinel = &cls.localServers[1];
	sentinel->adr.type = NA_IP;
	sentinel->adr.ipv._4[0] = 127; sentinel->adr.ipv._4[1] = 0;
	sentinel->adr.ipv._4[2] = 0; sentinel->adr.ipv._4[3] = 1;
	sentinel->adr.port = BigShort( (short)sentinelPort );
	Q_strncpyz( sentinel->hostName, "A0 WIRED Q0 SENTINEL", sizeof( sentinel->hostName ) );
	Q_strncpyz( sentinel->mapName, "arena1", sizeof( sentinel->mapName ) );
	Q_strncpyz( sentinel->game, "q3now", sizeof( sentinel->game ) );
	sentinel->clients = 0; sentinel->maxClients = 8; sentinel->gameType = 0;
	sentinel->ping = 73; sentinel->visible = qtrue;

	cls.numlocalservers = 2;
	cls.pingUpdateSource = AS_LOCAL;
	WiredFeeder_StateSetString( "ui_netSource", "0" );
	wui_serverFixtureActive = qtrue;
	WiredFeeder_ClearServerSelection();
	WiredFeeder_RebuildServerDisplayList();
	if ( target->g_needpass ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server fixture installed sentinel=127.0.0.1:%d target=127.0.0.1:%d target_needpass=1 raw_order=target,sentinel\n",
			sentinelPort, targetPort );
	} else {
		/* Preserve the established fixture marker for existing composite gates. */
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server fixture installed sentinel=127.0.0.1:%d target=127.0.0.1:%d raw_order=target,sentinel\n",
			sentinelPort, targetPort );
	}
	return qtrue;
}

void WiredFeeder_ServerFixtureClear( void ) {
	if ( !wui_serverFixtureActive ) return;
	WiredFeeder_ServerStatusCancel();
	WiredFeeder_ClearServerSelection();
	memset( cls.localServers, 0, sizeof( cls.localServers ) );
	cls.numlocalservers = 0;
	wui_serverFixtureActive = qfalse;
	wui_serverLastRawCount = -1;
	wui_serverLastSource = -1;
	wui_serverLastSignature = 0;
	wui_serverLastEngineGeneration = 0;
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: server fixture cleared\n" );
}

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
	int source = WiredFeeder_StateGetInt( "ui_netSource", 0 );
	int remappedSelection = -1;
	WiredFeeder_GetServerList( &servers, &count );

	wui_serverDisplayCount = 0;
	for ( int i = 0; i < count && wui_serverDisplayCount < MAX_DISPLAY_SERVERS; i++ ) {
		/* Wired UI owns this browser surface directly (there is no UI VM call to
		 * LAN_MarkServerVisible), so mark discovered rows for the engine ping queue. */
		servers[i].visible = qtrue;
		/* Master responses contain addresses only.  Preserve raw visibility for
		 * challenge-bound ping scheduling, but never display/select a row before
		 * authoritative metadata has supplied a positive RTT. */
		if ( source > 0 && source < 6 && servers[i].ping <= 0 ) continue;
		if ( WiredFeeder_ServerPassesFilter( &servers[i] ) ) {
			wui_serverDisplayList[wui_serverDisplayCount++] = i;
		}
	}

	// apply current sort
	if ( wui_serverDisplayCount > 1 ) {
		qsort( wui_serverDisplayList, wui_serverDisplayCount, sizeof( int ), WiredFeeder_ServerSortCompare );
	}

	wui_serverLastRawCount = count;
	wui_serverLastSource = source;
	wui_serverLastSignature = WiredFeeder_ServerSignature( servers, count );
	wui_serverLastEngineGeneration = cls.globalServerGeneration;
	wui_serverListGeneration++;
	/* A roster epoch change invalidates any pending physical double-click.
	 * Source changes and explicit sort operations both rebuild here. */
	WiredUI_ResetListboxDoubleClick( "server-roster" );

	/* A selection is transient browser state, not a persisted authority. Keep it
	 * only when the exact address still exists in the same source after a
	 * rebuild; otherwise clear the Store keys so a prior session/source cannot
	 * be consumed by Server Info or Join Server. */
	if ( wui_selectedServerSource == source && wui_selectedServerAddress[0] ) {
		for ( int i = 0; i < wui_serverDisplayCount; i++ ) {
			int raw = wui_serverDisplayList[i];
			char address[MAX_STRING_CHARS];
			Q_strncpyz( address, NET_AdrToStringwPort( &servers[raw].adr ), sizeof( address ) );
			if ( !Q_stricmp( address, wui_selectedServerAddress ) ) {
				remappedSelection = raw;
				break;
			}
		}
	}
	if ( remappedSelection >= 0 ) {
		wui_selectedServer = remappedSelection;
	} else if ( wui_selectedServer >= 0 || wui_selectedServerAddress[0] ) {
		WiredFeeder_ClearServerSelection();
	}

	// update status cvar for .menu display
	WiredFeeder_StateSetString( "ui_browserStatus", va( "%d servers (%d total)", wui_serverDisplayCount, count ) );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server roster generation=%d source=%d displayed=%d raw=%d\n",
		wui_serverListGeneration, source, wui_serverDisplayCount, count );
	for ( int i = 0; i < wui_serverDisplayCount; i++ ) {
		int raw = wui_serverDisplayList[i];
		char address[MAX_STRING_CHARS];
		char name[MAX_HOSTNAME_LENGTH];
		char map[MAX_NAME_LENGTH];
		Q_strncpyz( address, NET_AdrToStringwPort( &servers[raw].adr ), sizeof( address ) );
		WiredFeeder_SanitizeRemoteText( servers[raw].hostName, name, sizeof( name ) );
		WiredFeeder_SanitizeRemoteText( servers[raw].mapName, map, sizeof( map ) );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server roster row=%d raw=%d address=%s name=%s map=%s\n",
			i, raw, address, name, map );
	}
}

static int WiredFeeder_ServerCount( int feederID ) {
	serverInfo_t *servers = NULL;
	int count = 0;
	unsigned signature;
	WiredFeeder_GetServerList( &servers, &count );
	signature = WiredFeeder_ServerSignature( servers, count );

	// rebuild display list when server count changes (new pings arrived)
	if ( count != wui_serverLastRawCount || signature != wui_serverLastSignature
	     || ( WiredFeeder_StateGetInt( "ui_netSource", 0 ) > 0
	       && WiredFeeder_StateGetInt( "ui_netSource", 0 ) < 6
	       && cls.globalServerGeneration != wui_serverLastEngineGeneration )
	     || WiredFeeder_StateGetInt( "ui_netSource", 0 ) != wui_serverLastSource ) {
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
			if ( s->gameType >= 0 && s->gameType < GT_MAX_GAME_TYPE ) {
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

// ── server sort ──────────────────────────────────────────────────────
// Sort columns use SORT_HOST..SORT_PUNKBUSTER from ui_public.h

static int wui_serverSortKey = SORT_PING;
static int wui_serverSortDir = 0;  // 0=ascending, 1=descending

static int WiredFeeder_CompareInt( int a, int b ) {
	return ( a > b ) - ( a < b );
}

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
		case SORT_CLIENTS: result = WiredFeeder_CompareInt( sa->clients, sb->clients ); break;
		case SORT_PING:    result = WiredFeeder_CompareInt( sa->ping, sb->ping ); break;
		case SORT_GAME:    result = WiredFeeder_CompareInt( sa->gameType, sb->gameType ); break;
		default:           result = 0; break;
	}

	return wui_serverSortDir ? -result : result;
}

void WiredFeeder_SortServers( int column ) {
	// toggle direction if clicking same column
	if ( column == wui_serverSortKey ) {
		wui_serverSortDir = !wui_serverSortDir;
	} else {
		wui_serverSortKey = column;
		wui_serverSortDir = 0;
	}

	/* Rebuild through the shared filtered path (applies WiredFeeder_ServerPasses-
	 * Filter, re-sorts, and refreshes wui_serverLastRawCount). Pushing every raw
	 * index here instead would drop the active browser filter — and, by leaving
	 * wui_serverLastRawCount stale, keep the unfiltered list until the raw count
	 * changed. The sort key/dir set above are read by WiredFeeder_ServerSort-
	 * Compare inside the rebuild's qsort. */
	WiredFeeder_RebuildServerDisplayList();

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
	int source = WiredFeeder_StateGetInt( "ui_netSource", 0 );
	int realIndex;
	char address[MAX_STRING_CHARS];
	char safeName[MAX_HOSTNAME_LENGTH];
	char safeMap[MAX_NAME_LENGTH];

	if ( index < 0 || index >= wui_serverDisplayCount ) {
		WiredFeeder_ClearServerSelection();
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: rejected invalid server selection display_row=%d generation=%d\n",
			index, wui_serverListGeneration );
		return;
	}
	realIndex = wui_serverDisplayList[index];
	serverInfo_t *s = WiredFeeder_GetServerPtr( realIndex );
	if ( !s ) {
		WiredFeeder_ClearServerSelection();
		return;
	}

	Q_strncpyz( address, NET_AdrToStringwPort( &s->adr ), sizeof( address ) );
	WiredFeeder_SanitizeRemoteText( s->hostName, safeName, sizeof( safeName ) );
	WiredFeeder_SanitizeRemoteText( s->mapName, safeMap, sizeof( safeMap ) );
	WiredFeeder_ServerStatusCancel();
	wui_selectedServer = realIndex;
	wui_selectedServerSource = source;
	Q_strncpyz( wui_selectedServerAddress, address, sizeof( wui_selectedServerAddress ) );
	wui_serverSelectionGeneration++;
	WiredFeeder_StateSetString( "ui_selectedServerAddr", address );
	WiredFeeder_StateSetString( "ui_selectedServerName", s->hostName );
	WiredFeeder_StateSetString( "ui_mapLevelshot", va( "levelshots/%s", s->mapName ) );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server selection display_row=%d raw=%d source=%d list_generation=%d selection_generation=%d address=%s name=%s map=%s\n",
		index, realIndex, source, wui_serverListGeneration, wui_serverSelectionGeneration,
		address, safeName, safeMap );
}

qboolean WiredFeeder_GetSelectedServerAddress( char *out, int outSize ) {
	char stored[MAX_STRING_CHARS];
	char current[MAX_STRING_CHARS];
	serverInfo_t *server;
	int source = WiredFeeder_StateGetInt( "ui_netSource", 0 );

	if ( out && outSize > 0 ) out[0] = '\0';
	if ( !out || outSize <= 0 || wui_selectedServer < 0
	     || wui_selectedServerSource != source || !wui_selectedServerAddress[0] ) {
		return qfalse;
	}

	server = WiredFeeder_GetServerPtr( wui_selectedServer );
	if ( !server ) {
		WiredFeeder_ClearServerSelection();
		return qfalse;
	}
	Q_strncpyz( current, NET_AdrToStringwPort( &server->adr ), sizeof( current ) );
	WiredFeeder_StateGetString( "ui_selectedServerAddr", stored, sizeof( stored ) );
	if ( Q_stricmp( current, wui_selectedServerAddress )
	     || Q_stricmp( stored, wui_selectedServerAddress ) ) {
		WiredFeeder_ClearServerSelection();
		return qfalse;
	}

	Q_strncpyz( out, wui_selectedServerAddress, outSize );
	return qtrue;
}

qboolean WiredFeeder_GetSelectedServerConnection( char *address, int addressSize,
	char *displayName, int displayNameSize, qboolean *needPassword,
	int *selectionGeneration ) {
	serverInfo_t *server;

	if ( address && addressSize > 0 ) address[0] = '\0';
	if ( displayName && displayNameSize > 0 ) displayName[0] = '\0';
	if ( needPassword ) *needPassword = qfalse;
	if ( selectionGeneration ) *selectionGeneration = -1;
	if ( !WiredFeeder_GetSelectedServerAddress( address, addressSize ) ) return qfalse;

	server = WiredFeeder_GetServerPtr( wui_selectedServer );
	if ( !server ) {
		WiredFeeder_ClearServerSelection();
		if ( address && addressSize > 0 ) address[0] = '\0';
		return qfalse;
	}
	if ( displayName && displayNameSize > 0 ) {
		WiredFeeder_SanitizeRemoteText( server->hostName, displayName, displayNameSize );
	}
	if ( needPassword ) *needPassword = server->g_needpass ? qtrue : qfalse;
	if ( selectionGeneration ) *selectionGeneration = wui_serverSelectionGeneration;
	return qtrue;
}

int WiredFeeder_ServerDisplayGeneration( void ) {
	return wui_serverListGeneration;
}

qboolean WiredFeeder_GetSelectedServerIdentity( int displayRow, int *rawIndex,
	int *source, int *listGeneration, int *selectionGeneration ) {
	char address[MAX_STRING_CHARS];
	int currentSource = WiredFeeder_StateGetInt( "ui_netSource", 0 );

	if ( rawIndex ) *rawIndex = -1;
	if ( source ) *source = -1;
	if ( listGeneration ) *listGeneration = -1;
	if ( selectionGeneration ) *selectionGeneration = -1;
	if ( displayRow < 0 || displayRow >= wui_serverDisplayCount
	     || !WiredFeeder_GetSelectedServerAddress( address, sizeof( address ) )
	     || wui_serverDisplayList[displayRow] != wui_selectedServer ) {
		return qfalse;
	}

	if ( rawIndex ) *rawIndex = wui_selectedServer;
	if ( source ) *source = currentSource;
	if ( listGeneration ) *listGeneration = wui_serverListGeneration;
	if ( selectionGeneration ) *selectionGeneration = wui_serverSelectionGeneration;
	return qtrue;
}

// ── selected-server status feeder ────────────────────────────────────

#define MAX_WIRED_SERVERSTATUS_ROWS (8 + MAX_CLIENTS)
#define MAX_WIRED_SERVERSTATUS_KEY 64
#define MAX_WIRED_SERVERSTATUS_VALUE 256
#define WIRED_SERVERSTATUS_TIMEOUT_MS 4000u

typedef struct {
	char key[MAX_WIRED_SERVERSTATUS_KEY];
	char value[MAX_WIRED_SERVERSTATUS_VALUE];
} wiredServerStatusRow_t;

static wiredServerStatusRow_t wui_serverStatusRows[MAX_WIRED_SERVERSTATUS_ROWS];
static int wui_serverStatusRowCount = 0;
static int wui_serverStatusGeneration = 0;
static int wui_serverStatusSelectionGeneration = -1;
typedef enum {
	WIRED_SERVERSTATUS_IDLE,
	WIRED_SERVERSTATUS_PENDING,
	WIRED_SERVERSTATUS_READY,
	WIRED_SERVERSTATUS_NO_RESPONSE,
	WIRED_SERVERSTATUS_MALFORMED
} wiredServerStatusState_t;
static wiredServerStatusState_t wui_serverStatusState = WIRED_SERVERSTATUS_IDLE;
static unsigned int wui_serverStatusStartTime = 0;
static char wui_serverStatusAddress[MAX_STRING_CHARS];
typedef enum {
	WIRED_SERVERSTATUS_ORIGIN_NONE = 0,
	WIRED_SERVERSTATUS_ORIGIN_BROWSER,
	WIRED_SERVERSTATUS_ORIGIN_CONNECTED
} wiredServerStatusOrigin_t;
static wiredServerStatusOrigin_t wui_serverStatusOrigin = WIRED_SERVERSTATUS_ORIGIN_NONE;
static uint64_t wui_serverStatusOwnerGeneration = 0;
static netadr_t wui_serverStatusConnectedAddress;
static conn_handle_t wui_serverStatusConnectedHandle = CONN_INVALID;

static void WiredFeeder_AddStatusRow( const char *key, const char *value ) {
	wiredServerStatusRow_t *row;
	if ( wui_serverStatusRowCount >= MAX_WIRED_SERVERSTATUS_ROWS ) return;
	row = &wui_serverStatusRows[wui_serverStatusRowCount++];
	WiredFeeder_SanitizeRemoteText( key, row->key, sizeof( row->key ) );
	WiredFeeder_SanitizeRemoteText( value, row->value, sizeof( row->value ) );
}

typedef struct {
	char name[MAX_WIRED_SERVERSTATUS_VALUE];
	long score;
	long ping;
} wiredServerStatusPlayer_t;

static qboolean WiredFeeder_ParseStatusUnsigned( const char *text, unsigned int minValue,
	unsigned int maxValue, unsigned int *value ) {
	unsigned int parsed = 0;
	const unsigned char *p = (const unsigned char *)text;
	if ( !p || !*p ) return qfalse;
	while ( *p ) {
		unsigned int digit;
		if ( *p < '0' || *p > '9' ) return qfalse;
		digit = (unsigned int)(*p - '0');
		if ( parsed > ( maxValue - digit ) / 10u ) return qfalse;
		parsed = parsed * 10u + digit;
		p++;
	}
	if ( parsed < minValue || parsed > maxValue ) return qfalse;
	if ( value ) *value = parsed;
	return qtrue;
}

static qboolean WiredFeeder_CopyStatusToken( const char *start, size_t length,
	char *out, size_t outSize ) {
	char raw[MAX_WIRED_SERVERSTATUS_VALUE];
	if ( !start || !out || outSize == 0 || length == 0 || length >= sizeof( raw ) ) return qfalse;
	memcpy( raw, start, length );
	raw[length] = '\0';
	WiredFeeder_SanitizeRemoteText( raw, out, outSize );
	return out[0] != '\0';
}

static qboolean WiredFeeder_ParseServerStatus( const char *raw, const char **failureReason ) {
	char bounded[BIG_INFO_STRING];
	char *players;
	char server[MAX_WIRED_SERVERSTATUS_VALUE] = "";
	char map[MAX_WIRED_SERVERSTATUS_VALUE] = "";
	char maxClients[32] = "";
	char gameType[64] = "";
	char game[64] = "";
	char protocol[32] = "";
	char version[MAX_WIRED_SERVERSTATUS_VALUE] = "";
	wiredServerStatusPlayer_t parsedPlayers[MAX_CLIENTS];
	unsigned int parsedMaxClients = 0;
	unsigned int parsedGameType = 0;
	unsigned int parsedProtocol = 0;
	int playerCount = 0;
	qboolean haveServer = qfalse, haveMap = qfalse, haveMaxClients = qfalse;
	qboolean haveGameType = qfalse, haveGame = qfalse, haveProtocol = qfalse;
	unsigned int gameKeysSeen = 0;
	int gamePriority = 0;
	char *p;

	#define STATUS_FAIL(reason) do { if ( failureReason ) *failureReason = (reason); return qfalse; } while ( 0 )
	if ( failureReason ) *failureReason = "malformed";
	if ( !raw || strlen( raw ) >= sizeof( bounded ) ) STATUS_FAIL( "overlong" );
	Q_strncpyz( bounded, raw ? raw : "", sizeof( bounded ) );
	players = strstr( bounded, "\\\\" );
	if ( !players ) STATUS_FAIL( "missing-player-delimiter" );
	*players = '\0';
	players += 2;

	p = bounded;
	while ( *p ) {
		char key[MAX_WIRED_SERVERSTATUS_KEY];
		char value[MAX_WIRED_SERVERSTATUS_VALUE];
		char *keyStart, *valueStart;
		size_t keyLength, valueLength;
		if ( *p++ != '\\' ) STATUS_FAIL( "invalid-info-envelope" );
		keyStart = p;
		while ( *p && *p != '\\' ) p++;
		if ( !*p ) STATUS_FAIL( "unterminated-info-key" );
		keyLength = (size_t)( p - keyStart );
		if ( keyLength == 0 || keyLength >= sizeof( key ) ) STATUS_FAIL( "invalid-info-key" );
		memcpy( key, keyStart, keyLength );
		key[keyLength] = '\0';
		p++;
		valueStart = p;
		while ( *p && *p != '\\' ) p++;
		valueLength = (size_t)( p - valueStart );
		if ( valueLength >= sizeof( value ) ) STATUS_FAIL( "invalid-info-value" );
		memcpy( value, valueStart, valueLength );
		value[valueLength] = '\0';

		if ( !Q_stricmp( key, "sv_hostname" ) || !Q_stricmp( key, "hostname" ) ) {
			char candidate[MAX_WIRED_SERVERSTATUS_VALUE];
			if ( !WiredFeeder_CopyStatusToken( value, valueLength, candidate, sizeof( candidate ) ) ) STATUS_FAIL( "invalid-hostname" );
			if ( haveServer && Q_stricmp( server, candidate ) ) STATUS_FAIL( "conflicting-hostname" );
			if ( !haveServer ) Q_strncpyz( server, candidate, sizeof( server ) );
			haveServer = qtrue;
		} else if ( !Q_stricmp( key, "mapname" ) ) {
			if ( haveMap || !WiredFeeder_CopyStatusToken( value, valueLength, map, sizeof( map ) ) ) STATUS_FAIL( "invalid-mapname" );
			haveMap = qtrue;
		} else if ( !Q_stricmp( key, "sv_maxclients" ) ) {
			if ( haveMaxClients || !WiredFeeder_ParseStatusUnsigned( value, 1, MAX_CLIENTS, &parsedMaxClients ) ) STATUS_FAIL( "invalid-sv_maxclients" );
			Q_strncpyz( maxClients, value, sizeof( maxClients ) );
			haveMaxClients = qtrue;
		} else if ( !Q_stricmp( key, "g_gametype" ) || !Q_stricmp( key, "gametype" ) ) {
			unsigned int candidate;
			if ( !WiredFeeder_ParseStatusUnsigned( value, 0, GT_MAX_GAME_TYPE - 1, &candidate ) ) STATUS_FAIL( "invalid-gametype" );
			if ( haveGameType && parsedGameType != candidate ) STATUS_FAIL( "conflicting-gametype" );
			parsedGameType = candidate;
			if ( !haveGameType ) Q_strncpyz( gameType, value, sizeof( gameType ) );
			haveGameType = qtrue;
		} else if ( !Q_stricmp( key, "sv_gamename" ) || !Q_stricmp( key, "gamename" ) || !Q_stricmp( key, "game" ) ) {
			char candidate[sizeof( game )];
			unsigned int keyBit;
			int priority;
			if ( !Q_stricmp( key, "sv_gamename" ) ) {
				keyBit = 1u;
				priority = 3;
			} else if ( !Q_stricmp( key, "gamename" ) ) {
				keyBit = 2u;
				priority = 2;
			} else {
				keyBit = 4u;
				priority = 1;
			}
			if ( gameKeysSeen & keyBit ) STATUS_FAIL( "duplicate-game-key" );
			gameKeysSeen |= keyBit;
			if ( !WiredFeeder_CopyStatusToken( value, valueLength, candidate, sizeof( candidate ) ) ) STATUS_FAIL( "invalid-game" );
			/* q3now status carries both product identity (`sv_gamename`/`gamename`)
			 * and the filesystem mod key (`game`). They are distinct legacy aliases,
			 * not conflicting duplicates; display the most specific product key. */
			if ( priority > gamePriority ) {
				Q_strncpyz( game, candidate, sizeof( game ) );
				gamePriority = priority;
			}
			haveGame = qtrue;
		} else if ( !Q_stricmp( key, "protocol" ) ) {
			if ( haveProtocol || !WiredFeeder_ParseStatusUnsigned( value, 1, 1000000u, &parsedProtocol ) ) STATUS_FAIL( "invalid-protocol" );
			Q_strncpyz( protocol, value, sizeof( protocol ) );
			haveProtocol = qtrue;
		} else if ( !Q_stricmp( key, "version" ) ) {
			if ( version[0] || !WiredFeeder_CopyStatusToken( value, valueLength, version, sizeof( version ) ) ) STATUS_FAIL( "invalid-version" );
		}
	}
	if ( !haveServer ) STATUS_FAIL( "missing-hostname" );
	if ( !haveMap ) STATUS_FAIL( "missing-mapname" );
	if ( !haveMaxClients ) STATUS_FAIL( "missing-sv_maxclients" );
	if ( !haveGameType ) STATUS_FAIL( "missing-gametype" );
	if ( !haveGame ) STATUS_FAIL( "missing-game" );
	if ( !haveProtocol ) STATUS_FAIL( "missing-protocol" );
	(void)parsedGameType;
	(void)parsedProtocol;

	p = players;
	while ( *p ) {
		char *recordEnd = strchr( p, '\\' );
		char *cursor = p;
		char *scoreEnd, *pingEnd, *quote;
		long score, ping;
		if ( playerCount >= MAX_CLIENTS ) STATUS_FAIL( "too-many-players" );
		if ( recordEnd ) *recordEnd = '\0';
		if ( !*cursor ) STATUS_FAIL( "empty-player-record" );
		while ( *cursor == ' ' || *cursor == '\t' ) cursor++;
		score = strtol( cursor, &scoreEnd, 10 );
		if ( scoreEnd == cursor || score < -999999L || score > 999999L ) STATUS_FAIL( "invalid-player-score" );
		cursor = scoreEnd;
		if ( *cursor != ' ' && *cursor != '\t' ) STATUS_FAIL( "invalid-player-separator" );
		while ( *cursor == ' ' || *cursor == '\t' ) cursor++;
		ping = strtol( cursor, &pingEnd, 10 );
		if ( pingEnd == cursor || ping < 0 || ping > 999999L ) STATUS_FAIL( "invalid-player-ping" );
		cursor = pingEnd;
		if ( *cursor != ' ' && *cursor != '\t' ) STATUS_FAIL( "invalid-player-separator" );
		while ( *cursor == ' ' || *cursor == '\t' ) cursor++;
		if ( *cursor++ != '"' ) STATUS_FAIL( "invalid-player-name" );
		quote = strchr( cursor, '"' );
		if ( !quote ) STATUS_FAIL( "unterminated-player-name" );
		for ( char *suffix = quote + 1; *suffix; suffix++ ) {
			if ( *suffix != ' ' && *suffix != '\t' ) STATUS_FAIL( "invalid-player-suffix" );
		}
		if ( !WiredFeeder_CopyStatusToken( cursor, (size_t)( quote - cursor ),
			parsedPlayers[playerCount].name, sizeof( parsedPlayers[playerCount].name ) ) ) STATUS_FAIL( "invalid-player-name" );
		parsedPlayers[playerCount].score = score;
		parsedPlayers[playerCount].ping = ping;
		playerCount++;
		if ( !recordEnd ) break;
		p = recordEnd + 1;
	}
	if ( (unsigned int)playerCount > parsedMaxClients ) STATUS_FAIL( "players-exceed-max" );

	wui_serverStatusRowCount = 0;
	WiredFeeder_AddStatusRow( "Address", wui_serverStatusAddress );
	WiredFeeder_AddStatusRow( "Server", server );
	WiredFeeder_AddStatusRow( "Map", map );
	WiredFeeder_AddStatusRow( "Players", va( "%d/%s", playerCount, maxClients[0] ? maxClients : "0" ) );
	WiredFeeder_AddStatusRow( "Game type", gameType );
	WiredFeeder_AddStatusRow( "Game", game );
	WiredFeeder_AddStatusRow( "Protocol", protocol );
	WiredFeeder_AddStatusRow( "Version", version );

	for ( int i = 0; i < playerCount; i++ ) {
		char value[MAX_WIRED_SERVERSTATUS_VALUE];
		char key[32];
		Com_sprintf( key, sizeof( key ), "Player %d", i + 1 );
		Com_sprintf( value, sizeof( value ), "%s — score %ld, ping %ld",
			parsedPlayers[i].name, parsedPlayers[i].score, parsedPlayers[i].ping );
		WiredFeeder_AddStatusRow( key, value );
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server status loaded generation=%d selection_generation=%d address=%s rows=%d\n",
		wui_serverStatusGeneration, wui_serverStatusSelectionGeneration,
		wui_serverStatusAddress, wui_serverStatusRowCount );
	for ( int i = 0; i < wui_serverStatusRowCount; i++ ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server status row=%d key=%s value=%s\n",
			i, wui_serverStatusRows[i].key, wui_serverStatusRows[i].value );
	}
	#undef STATUS_FAIL
	return qtrue;
}

static void WiredFeeder_ServerStatusFail( wiredServerStatusState_t state,
	const char *reason, const char *message ) {
	wui_serverStatusState = state;
	wui_serverStatusRowCount = 0;
	WiredFeeder_AddStatusRow( "Address", wui_serverStatusAddress );
	WiredFeeder_AddStatusRow( "Status", message );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server status failed generation=%d selection_generation=%d address=%s state=%s reason=%s rows=2\n",
		wui_serverStatusGeneration, wui_serverStatusSelectionGeneration,
		wui_serverStatusAddress,
		state == WIRED_SERVERSTATUS_NO_RESPONSE ? "no-response" : "malformed",
		reason ? reason : "unknown" );
}

static int WiredFeeder_ServerStatusCount( int feederID ) {
	return wui_serverStatusRowCount;
}

static const char *WiredFeeder_ServerStatusItemText( int feederID, int index, int column ) {
	if ( index < 0 || index >= wui_serverStatusRowCount ) return "";
	if ( column == 0 ) return wui_serverStatusRows[index].key;
	if ( column == 1 ) return wui_serverStatusRows[index].value;
	return "";
}

static qboolean WiredFeeder_ServerStatusStart( const char *currentAddress ) {
	char response[BIG_INFO_STRING];
	if ( wui_serverStatusAddress[0] ) {
		CL_ServerStatus( wui_serverStatusAddress, NULL, 0 );
	}
	CL_ServerStatus( currentAddress, NULL, 0 );
	wui_serverStatusGeneration++;
	wui_serverStatusRowCount = 0;
	wui_serverStatusState = WIRED_SERVERSTATUS_PENDING;
	wui_serverStatusStartTime = (unsigned int)Sys_Milliseconds();
	Q_strncpyz( wui_serverStatusAddress, currentAddress, sizeof( wui_serverStatusAddress ) );
	WiredFeeder_AddStatusRow( "Status", "Contacting server..." );
	(void) CL_ServerStatus( wui_serverStatusAddress, response, sizeof( response ) );
	if ( wui_serverStatusOrigin == WIRED_SERVERSTATUS_ORIGIN_CONNECTED ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: connected server status request generation=%d owner_generation=%" PRIu64 " address=%s\n",
			wui_serverStatusGeneration, wui_serverStatusOwnerGeneration,
			wui_serverStatusAddress );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: connected server status state=pending generation=%d owner_generation=%" PRIu64 " address=%s rows=1\n",
			wui_serverStatusGeneration, wui_serverStatusOwnerGeneration,
			wui_serverStatusAddress );
	} else {
		/* Browser-origin marker ABI is consumed by the existing browser gates. */
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server status request generation=%d selection_generation=%d address=%s\n",
			wui_serverStatusGeneration, wui_serverStatusSelectionGeneration, wui_serverStatusAddress );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server status state=pending generation=%d selection_generation=%d address=%s rows=1\n",
			wui_serverStatusGeneration, wui_serverStatusSelectionGeneration, wui_serverStatusAddress );
	}
	return qtrue;
}

qboolean WiredFeeder_ServerStatusBegin( void ) {
	char currentAddress[MAX_STRING_CHARS];
	if ( !WiredFeeder_GetSelectedServerAddress( currentAddress, sizeof( currentAddress ) ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: server status refused without current selection\n" );
		return qfalse;
	}
	wui_serverStatusOrigin = WIRED_SERVERSTATUS_ORIGIN_BROWSER;
	wui_serverStatusSelectionGeneration = wui_serverSelectionGeneration;
	wui_serverStatusOwnerGeneration = 0;
	memset( &wui_serverStatusConnectedAddress, 0, sizeof( wui_serverStatusConnectedAddress ) );
	wui_serverStatusConnectedHandle = CONN_INVALID;
	return WiredFeeder_ServerStatusStart( currentAddress );
}

qboolean WiredFeeder_ServerStatusBeginConnected( void ) {
	const netadr_t *activeAddress;
	char currentAddress[MAX_STRING_CHARS];

	if ( !clientActiveApp || clientActiveApp->state != CA_ACTIVE
	     || clientActiveApp->clc.demoplaying
	     || clientActiveApp->clc.serverAddress.type == NA_BAD
	     || clientActiveApp->clc.quic_conn == CONN_INVALID
	     || clientActiveApp->connectionGeneration == 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: connected server status refused without active network connection\n" );
		return qfalse;
	}
	activeAddress = &clientActiveApp->clc.serverAddress;
	Q_strncpyz( currentAddress, NET_AdrToStringwPort( activeAddress ), sizeof( currentAddress ) );
	if ( !currentAddress[0] ) return qfalse;

	wui_serverStatusOrigin = WIRED_SERVERSTATUS_ORIGIN_CONNECTED;
	wui_serverStatusOwnerGeneration = clientActiveApp->connectionGeneration;
	wui_serverStatusConnectedAddress = *activeAddress;
	wui_serverStatusConnectedHandle = clientActiveApp->clc.quic_conn;
	wui_serverStatusSelectionGeneration = -1;
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: connected server status open owner_generation=%" PRIu64 " address=%s\n",
		wui_serverStatusOwnerGeneration, currentAddress );
	return WiredFeeder_ServerStatusStart( currentAddress );
}

qboolean WiredFeeder_ServerStatusRetry( void ) {
	if ( wui_serverStatusOrigin == WIRED_SERVERSTATUS_ORIGIN_CONNECTED ) {
		char currentAddress[MAX_STRING_CHARS];
		if ( !clientActiveApp || clientActiveApp->state != CA_ACTIVE
		     || clientActiveApp->clc.demoplaying
		     || clientActiveApp->clc.quic_conn == CONN_INVALID
		     || clientActiveApp->clc.quic_conn != wui_serverStatusConnectedHandle
		     || clientActiveApp->connectionGeneration != wui_serverStatusOwnerGeneration
		     || !NET_CompareAdr( &wui_serverStatusConnectedAddress,
			&clientActiveApp->clc.serverAddress ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI: connected server status retry refused owner_generation=%" PRIu64 " reason=stale-connection\n",
				wui_serverStatusOwnerGeneration );
			WiredFeeder_ServerStatusCancel();
			WiredUI_PopMenu();
			return qfalse;
		}
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: connected server status retry owner_generation=%" PRIu64 " prior_generation=%d address=%s\n",
			wui_serverStatusOwnerGeneration, wui_serverStatusGeneration,
			wui_serverStatusAddress );
		Q_strncpyz( currentAddress, wui_serverStatusAddress, sizeof( currentAddress ) );
		return WiredFeeder_ServerStatusStart( currentAddress );
	}
	return WiredFeeder_ServerStatusBegin();
}

void WiredFeeder_ServerStatusPoll( void ) {
	char currentAddress[MAX_STRING_CHARS];
	char response[BIG_INFO_STRING];
	const char *failureReason = NULL;
	if ( wui_serverStatusState != WIRED_SERVERSTATUS_PENDING ) return;
	if ( wui_serverStatusOrigin == WIRED_SERVERSTATUS_ORIGIN_CONNECTED ) {
		if ( !clientActiveApp || clientActiveApp->state != CA_ACTIVE
		     || clientActiveApp->clc.demoplaying
		     || clientActiveApp->clc.quic_conn == CONN_INVALID
		     || clientActiveApp->clc.quic_conn != wui_serverStatusConnectedHandle
		     || clientActiveApp->connectionGeneration != wui_serverStatusOwnerGeneration
		     || !NET_CompareAdr( &wui_serverStatusConnectedAddress,
			&clientActiveApp->clc.serverAddress ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI: connected server status invalidated owner_generation=%" PRIu64 " reason=stale-connection\n",
				wui_serverStatusOwnerGeneration );
			WiredFeeder_ServerStatusCancel();
			WiredUI_PopMenu();
			return;
		}
	} else if ( !WiredFeeder_GetSelectedServerAddress( currentAddress, sizeof( currentAddress ) )
	            || wui_serverStatusSelectionGeneration != wui_serverSelectionGeneration
	            || Q_stricmp( wui_serverStatusAddress, currentAddress ) ) {
		WiredFeeder_ServerStatusCancel();
		return;
	}
	if ( CL_ServerStatus( wui_serverStatusAddress, response, sizeof( response ) ) ) {
		if ( WiredFeeder_ParseServerStatus( response, &failureReason ) ) {
			wui_serverStatusState = WIRED_SERVERSTATUS_READY;
			if ( wui_serverStatusOrigin == WIRED_SERVERSTATUS_ORIGIN_CONNECTED ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
					"WiredUI: connected server status ready generation=%d owner_generation=%" PRIu64 " address=%s rows=%d\n",
					wui_serverStatusGeneration, wui_serverStatusOwnerGeneration,
					wui_serverStatusAddress, wui_serverStatusRowCount );
			}
		} else {
			CL_ServerStatus( wui_serverStatusAddress, NULL, 0 );
			WiredFeeder_ServerStatusFail( WIRED_SERVERSTATUS_MALFORMED,
				failureReason, "Invalid server response." );
		}
		return;
	}
	if ( (unsigned int)Sys_Milliseconds() - wui_serverStatusStartTime >= WIRED_SERVERSTATUS_TIMEOUT_MS ) {
		CL_ServerStatus( wui_serverStatusAddress, NULL, 0 );
		WiredFeeder_ServerStatusFail( WIRED_SERVERSTATUS_NO_RESPONSE,
			"timeout", "No response from server." );
	}
}

void WiredFeeder_ServerStatusCancel( void ) {
	if ( wui_serverStatusOrigin == WIRED_SERVERSTATUS_ORIGIN_CONNECTED ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: connected server status cancel owner_generation=%" PRIu64 " generation=%d address=%s\n",
			wui_serverStatusOwnerGeneration, wui_serverStatusGeneration,
			wui_serverStatusAddress[0] ? wui_serverStatusAddress : "none" );
	}
	if ( wui_serverStatusAddress[0] ) {
		CL_ServerStatus( wui_serverStatusAddress, NULL, 0 );
	}
	wui_serverStatusGeneration++;
	wui_serverStatusState = WIRED_SERVERSTATUS_IDLE;
	wui_serverStatusStartTime = 0;
	wui_serverStatusRowCount = 0;
	wui_serverStatusSelectionGeneration = -1;
	wui_serverStatusAddress[0] = '\0';
	wui_serverStatusOrigin = WIRED_SERVERSTATUS_ORIGIN_NONE;
	wui_serverStatusOwnerGeneration = 0;
	memset( &wui_serverStatusConnectedAddress, 0, sizeof( wui_serverStatusConnectedAddress ) );
	wui_serverStatusConnectedHandle = CONN_INVALID;
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server status cancelled generation=%d rows=0\n", wui_serverStatusGeneration );
}

void WiredFeeder_ServerStatusCancelForCloseAll( void ) {
	const char *state;

	switch ( wui_serverStatusState ) {
	case WIRED_SERVERSTATUS_PENDING: state = "pending"; break;
	case WIRED_SERVERSTATUS_READY: state = "ready"; break;
	case WIRED_SERVERSTATUS_NO_RESPONSE: state = "no-response"; break;
	case WIRED_SERVERSTATUS_MALFORMED: state = "malformed"; break;
	case WIRED_SERVERSTATUS_IDLE:
	default: state = "idle"; break;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server status dispose reason=close-all prior_state=%s generation=%d selection_generation=%d address=%s\n",
		state, wui_serverStatusGeneration, wui_serverStatusSelectionGeneration,
		wui_serverStatusAddress[0] ? wui_serverStatusAddress : "none" );
	WiredFeeder_ServerStatusCancel();
}

void WiredFeeder_ServerStatusTrace( void ) {
	const char *state;

	switch ( wui_serverStatusState ) {
	case WIRED_SERVERSTATUS_PENDING: state = "pending"; break;
	case WIRED_SERVERSTATUS_READY: state = "ready"; break;
	case WIRED_SERVERSTATUS_NO_RESPONSE: state = "no-response"; break;
	case WIRED_SERVERSTATUS_MALFORMED: state = "malformed"; break;
	case WIRED_SERVERSTATUS_IDLE:
	default: state = "idle"; break;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server status snapshot state=%s generation=%d selection_generation=%d address=%s count=%d\n",
		state, wui_serverStatusGeneration, wui_serverStatusSelectionGeneration,
		wui_serverStatusAddress[0] ? wui_serverStatusAddress : "none",
		wui_serverStatusRowCount );
}

// ── demo feeder ───────────────────────────────────────────────────────
// Data source: FS_GetFileList("demos", extension)

#define MAX_WIRED_DEMOS  512
static char  wui_demoList[MAX_WIRED_DEMOS][MAX_QPATH];
static int   wui_demoCount = 0;
static unsigned int wui_demoGeneration = 0;
static unsigned int wui_demoSelectionGeneration = 0;
static int   wui_selectedDemo = -1;
static char  wui_selectedDemoName[MAX_QPATH];

static int WiredFeeder_DemoSortCompare( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, (const char *)b );
}

void WiredFeeder_LoadDemos( void ) {
	char listBuf[8192];

	wui_demoCount = 0;
	wui_selectedDemo = -1;
	wui_selectedDemoName[0] = '\0';
	wui_demoSelectionGeneration = 0;
	wui_demoGeneration++;
	WiredFeeder_StateSetString( "ui_selectedDemo", "" );

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
	if ( wui_demoCount > 1 ) {
		qsort( wui_demoList, (size_t)wui_demoCount, sizeof( wui_demoList[0] ),
			WiredFeeder_DemoSortCompare );
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: demos loaded protocol=%d count=%d generation=%u\n",
		PROTOCOL_VERSION, wui_demoCount, wui_demoGeneration );
	for ( int i = 0; i < wui_demoCount; i++ ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: demo feeder row=%d name=%s generation=%u\n",
			i, wui_demoList[i], wui_demoGeneration );
	}
}

static int WiredFeeder_DemoCount( int feederID ) {
	return wui_demoCount;
}

static const char *WiredFeeder_DemoItemText( int feederID, int index, int column ) {
	if ( index < 0 || index >= wui_demoCount ) return "";
	return wui_demoList[index];
}

static void WiredFeeder_DemoSelection( int feederID, int index ) {
	if ( index < 0 || index >= wui_demoCount ) {
		wui_selectedDemo = -1;
		wui_selectedDemoName[0] = '\0';
		wui_demoSelectionGeneration = 0;
		WiredFeeder_StateSetString( "ui_selectedDemo", "" );
		return;
	}
	wui_selectedDemo = index;
	wui_demoSelectionGeneration = wui_demoGeneration;
	Q_strncpyz( wui_selectedDemoName, wui_demoList[index], sizeof( wui_selectedDemoName ) );
	WiredFeeder_StateSetString( "ui_selectedDemo", wui_selectedDemoName );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: demo feeder selection row=%d name=%s generation=%u\n",
		index, wui_selectedDemoName, wui_demoSelectionGeneration );
}

qboolean WiredFeeder_GetSelectedDemo( char *name, size_t nameSize ) {
	char stored[MAX_QPATH];
	if ( !name || nameSize < 1 ) return qfalse;
	name[0] = '\0';
	if ( wui_selectedDemo < 0 || wui_selectedDemo >= wui_demoCount
	  || wui_demoSelectionGeneration != wui_demoGeneration
	  || Q_stricmp( wui_selectedDemoName, wui_demoList[wui_selectedDemo] ) ) {
		return qfalse;
	}
	WiredFeeder_StateGetString( "ui_selectedDemo", stored, sizeof( stored ) );
	if ( Q_stricmp( stored, wui_selectedDemoName ) ) return qfalse;
	Q_strncpyz( name, wui_selectedDemoName, nameSize );
	return qtrue;
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
	Q_strncpyz( wui_modList[0], BASEGAME, sizeof( wui_modList[0] ) );
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
		char *descPtr = dirPtr + strlen( dirPtr ) + 1;

		if ( dirPtr[0] && Q_stricmp( dirPtr, BASEGAME ) ) {
			Q_strncpyz( wui_modList[wui_modCount], dirPtr, sizeof( wui_modList[0] ) );
			// $modlist is a sequence of directory/description NUL pairs.  The
			// description was already resolved by FS_GetModList, including its
			// directory-name fallback, so consuming it directly also preserves the
			// exact pairing contract when multiple mods are installed.
			Q_strncpyz( wui_modDesc[wui_modCount], descPtr, sizeof( wui_modDesc[0] ) );
			wui_modCount++;
		}
		dirPtr = descPtr + strlen( descPtr ) + 1;
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
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: mod feeder selection row=%d dir=%s description=%s\n",
			index, wui_modList[index], wui_modDesc[index] );
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

// q3now meta migration: WiredFeeder_LoadArenaFile and
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
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: map feeder selection filtered_row=%d map=%s\n",
			index, wui_maps[mapIdx].mapLoadName );

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

	// Export sort state so the engine-drawn header band can draw the
	// active-column ^/v indicator (parity with WiredFeeder_SortServers).
	WiredFeeder_StateSetInt( "ui_mapSortKey", wui_mapSortKey );
	WiredFeeder_StateSetInt( "ui_mapSortDir", wui_mapSortDir );
}

/* Generic "which column is this feeder currently sorted by, and which
 * direction" accessor, used by the engine header band (cl_wired_clay.c) to
 * draw the ^/v sort indicator on the active column. Returns qtrue and fills
 * *col / *dir (dir: 0=asc, 1=desc) when the feeder has a known sort; qfalse
 * otherwise (no indicator drawn). */
qboolean WiredFeeder_ActiveSort( int feederID, int *col, int *dir ) {
	switch ( feederID ) {
		case FEEDER_MAPS:
		case FEEDER_ALLMAPS:
			if ( col ) *col = wui_mapSortKey;
			if ( dir ) *dir = wui_mapSortDir;
			return qtrue;
		case FEEDER_SERVERS:
			if ( col ) *col = wui_serverSortKey;
			if ( dir ) *dir = wui_serverSortDir;
			return qtrue;
		default:
			return qfalse;
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

	if ( !wiredHud || !wiredHud_state_valid ) return 0;

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

	if ( !wiredHud || !wiredHud_state_valid ) return "";

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

// ── player-list feeder (FEEDER_PLAYER_LIST) ───────────────────────────
// Backs the kick / leader listboxes in callvote.wui and the kick listbox in
// removebots.wui. Data source is the same as the scoreboard feeder
// (wiredHud->scores[] + wiredHud->clients[]); spectators (team 3) are excluded
// since they can't be kicked/led as active players. The selection callback
// deposits the client NUMBER (not name) into state key ui_selectedPlayerNum,
// which the vote/kick handlers read for the numeric console path
// (clientkick <n> / callteamvote leader <n>).

static int WiredFeeder_PlayerCount( int feederID ) {
	int count = 0;
	if ( !wiredHud || !wiredHud_state_valid ) return 0;
	for ( int i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( wiredHud->scores[i].team == 3 /* spectator */ ) continue;
		count++;
	}
	return count;
}

static const char *WiredFeeder_PlayerItemText( int feederID, int index, int column ) {
	int count = 0;
	int i;
	if ( !wiredHud || !wiredHud_state_valid ) return "";
	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( wiredHud->scores[i].team == 3 /* spectator */ ) continue;
		if ( count == index ) break;
		count++;
	}
	if ( i >= wiredHud->numScores || i >= WIRED_HUD_MAX_SCORES ) return "";
	wiredHudScore_t *sc = &wiredHud->scores[i];
	if ( sc->client >= 0 && sc->client < WIRED_HUD_MAX_CLIENTS
		 && wiredHud->clients[sc->client].infoValid )
		return wiredHud->clients[sc->client].name;
	return "???";
}

static void WiredFeeder_PlayerSelection( int feederID, int index ) {
	int count = 0;
	int i;
	if ( !wiredHud || !wiredHud_state_valid ) return;
	for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
		if ( wiredHud->scores[i].team == 3 /* spectator */ ) continue;
		if ( count == index ) break;
		count++;
	}
	if ( i >= wiredHud->numScores || i >= WIRED_HUD_MAX_SCORES ) return;
	WiredFeeder_StateSetString( "ui_selectedPlayerNum",
		va( "%d", wiredHud->scores[i].client ) );
}

// ── bot-only feeder (FEEDER_BOTS) ────────────────────────────────────
// Presentation authority starts with the game-owned CS_PLAYERS `skill` key,
// then binds the row to one atomic server snapshot. Final removal authority is
// the server's execution-time NA_BOT + allocation-id recheck in `botkick`.

typedef struct {
	int      clientNum;
	uint64_t allocationId;
	char     name[MAX_NAME_LENGTH];
} wuiBotRow_t;

static wuiBotRow_t wui_botRows[MAX_CLIENTS];
static int         wui_botRowCount;
static int         wui_botRosterGeneration;
static int         wui_botSelectedClient = -1;
static uint64_t    wui_botSelectedAllocationId;
static int         wui_botSelectionGeneration = -1;
static char        wui_botSelectedName[MAX_NAME_LENGTH];

static qboolean WiredFeeder_ClientBotInfo( int clientNum, wuiBotRow_t *row ) {
	int ofs;
	const char *info;
	const char *skill;
	const char *displayName;
	svBotIdentitySnapshot_t serverIdentity = { 0 };
	wuiBotRow_t verified = { 0 };
	float skillValue;

	if ( !row || !clientActiveApp || clientActiveApp->state < CA_PRIMED ) return qfalse;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return qfalse;
	ofs = clientActiveApp->cl.gameState.stringOffsets[CS_PLAYERS + clientNum];
	if ( ofs <= 0 ) return qfalse;
	info = clientActiveApp->cl.gameState.stringData + ofs;
	skill = Info_ValueForKey( info, "skill" );
	if ( !skill || !skill[0] ) return qfalse;
	skillValue = (float)atof( skill );
	if ( skillValue < 1.0f || skillValue > 5.0f ) return qfalse;

	displayName = Info_ValueForKey( info, "n" );
	if ( !displayName || !displayName[0]
	  || !SV_BotIdentityForClient( clientNum, &serverIdentity )
	  || serverIdentity.clientNum != clientNum
	  || Q_stricmp( displayName, serverIdentity.name ) != 0 ) return qfalse;
	verified.clientNum = clientNum;
	verified.allocationId = serverIdentity.allocationId;
	Q_strncpyz( verified.name, displayName, sizeof( verified.name ) );
	*row = verified;
	return qtrue;
}

static void WiredFeeder_RefreshBotRows( void ) {
	wuiBotRow_t next[MAX_CLIENTS];
	int nextCount = 0;
	qboolean changed;
	memset( next, 0, sizeof( next ) );

	for ( int clientNum = 0; clientNum < MAX_CLIENTS; clientNum++ ) {
		wuiBotRow_t verified = { 0 };
		if ( !WiredFeeder_ClientBotInfo( clientNum, &verified ) ) continue;
		next[nextCount] = verified;
		nextCount++;
	}

	changed = ( nextCount != wui_botRowCount );
	for ( int i = 0; !changed && i < nextCount; i++ ) {
		changed = next[i].clientNum != wui_botRows[i].clientNum
			|| next[i].allocationId != wui_botRows[i].allocationId
			|| Q_stricmp( next[i].name, wui_botRows[i].name ) != 0;
	}
	if ( !changed ) return;

	memcpy( wui_botRows, next, (size_t)nextCount * sizeof( next[0] ) );
	wui_botRowCount = nextCount;
	wui_botRosterGeneration++;
	wui_botSelectedClient = -1;
	wui_botSelectedAllocationId = 0;
	wui_botSelectionGeneration = -1;
	wui_botSelectedName[0] = '\0';
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: bot feeder roster generation=%d count=%d\n",
		wui_botRosterGeneration, wui_botRowCount );
}

void WiredFeeder_ClearBotSelection( void ) {
	wui_botSelectedClient = -1;
	wui_botSelectedAllocationId = 0;
	wui_botSelectionGeneration = -1;
	wui_botSelectedName[0] = '\0';
}

static int WiredFeeder_BotCount( int feederID ) {
	WiredFeeder_RefreshBotRows();
	return wui_botRowCount;
}

static const char *WiredFeeder_BotItemText( int feederID, int index, int column ) {
	WiredFeeder_RefreshBotRows();
	if ( index < 0 || index >= wui_botRowCount ) return "";
	return wui_botRows[index].name;
}

static void WiredFeeder_BotSelection( int feederID, int index ) {
	char cleanName[MAX_NAME_LENGTH];
	WiredFeeder_RefreshBotRows();
	if ( index < 0 || index >= wui_botRowCount ) {
		WiredFeeder_ClearBotSelection();
		return;
	}
	wui_botSelectedClient = wui_botRows[index].clientNum;
	wui_botSelectedAllocationId = wui_botRows[index].allocationId;
	wui_botSelectionGeneration = wui_botRosterGeneration;
	Q_strncpyz( wui_botSelectedName, wui_botRows[index].name, sizeof( wui_botSelectedName ) );
	Q_strncpyz( cleanName, wui_botSelectedName, sizeof( cleanName ) );
	Q_CleanStr( cleanName );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: bot feeder selection row=%d client=%d allocation=%" PRIu64 " generation=%d name=%s\n",
		index, wui_botSelectedClient, wui_botSelectedAllocationId,
		wui_botSelectionGeneration, cleanName );
}

qboolean WiredFeeder_GetSelectedBotIdentity( wuiBotSelection_t *selection ) {
	svBotIdentitySnapshot_t currentIdentity = { 0 };
	wuiBotSelection_t verified = { 0 };
	WiredFeeder_RefreshBotRows();
	if ( !selection || wui_botSelectedClient < 0
	  || wui_botSelectedAllocationId == 0
	  || wui_botSelectionGeneration != wui_botRosterGeneration ) return qfalse;
	if ( !SV_BotIdentityForClient( wui_botSelectedClient, &currentIdentity )
	  || currentIdentity.allocationId != wui_botSelectedAllocationId
	  || Q_stricmp( currentIdentity.name, wui_botSelectedName ) != 0 ) return qfalse;
	for ( int i = 0; i < wui_botRowCount; i++ ) {
		if ( wui_botRows[i].clientNum != wui_botSelectedClient ) continue;
		if ( wui_botRows[i].allocationId != wui_botSelectedAllocationId ) return qfalse;
		if ( Q_stricmp( wui_botRows[i].name, wui_botSelectedName ) != 0 ) return qfalse;
		verified.clientNum = wui_botSelectedClient;
		verified.allocationId = wui_botSelectedAllocationId;
		verified.rosterGeneration = wui_botSelectionGeneration;
		Q_strncpyz( verified.name, wui_botSelectedName, sizeof( verified.name ) );
		*selection = verified;
		return qtrue;
	}
	return qfalse;
}

void WiredFeeder_BotTrace( void ) {
	WiredFeeder_RefreshBotRows();
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: bot feeder trace generation=%d count=%d selected_client=%d selected_allocation=%" PRIu64 "\n",
		wui_botRosterGeneration, wui_botRowCount, wui_botSelectedClient,
		wui_botSelectedAllocationId );
	for ( int i = 0; i < wui_botRowCount; i++ ) {
		char cleanName[MAX_NAME_LENGTH];
		Q_strncpyz( cleanName, wui_botRows[i].name, sizeof( cleanName ) );
		Q_CleanStr( cleanName );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: bot feeder row=%d client=%d allocation=%" PRIu64 " name=%s\n",
			i, wui_botRows[i].clientNum, wui_botRows[i].allocationId, cleanName );
	}
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

// The character feeder enumerates the SELECTABLE-only subset. Every entry point
// below (Count / ItemText / ItemIcon / Selection / this loader) indexes through
// CL_Characters_SelectableAt, and wui_charSelected is a selectable-subset index —
// so the skins feeder, which reads CL_Characters_SelectableAt(wui_charSelected),
// stays in the same index space. No raw registry index reaches the feeder.
void WiredFeeder_LoadCharacters( void ) {
	int count = CL_Characters_SelectableCount();

	// match current char cvar to selection (in the selectable subset)
	char charBuf[64];
	Cvar_VariableStringBuffer( "char", charBuf, sizeof( charBuf ) );
	wui_charSelected = -1;
	for ( int i = 0; i < count; i++ ) {
		const clCharacterEntry_t *e = CL_Characters_SelectableAt( i );
		if ( e && !Q_stricmp( e->dirname, charBuf ) ) {
			wui_charSelected = i;
			break;
		}
	}

	// align the skin selection with whatever the current skin cvar is
	wui_skinSelected = WiredFeeder_PickSkinForChar( CL_Characters_SelectableAt( wui_charSelected ) );

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: found %d selectable characters\n", count );
}

static int WiredFeeder_CharactersCount( int feederID ) {
	return CL_Characters_SelectableCount();
}

static const char *WiredFeeder_CharactersItemText( int feederID, int index, int column ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( index );
	if ( !e ) return "";
	return e->manifest.displayName[0] ? e->manifest.displayName : e->dirname;
}

static qhandle_t WiredFeeder_CharactersItemIcon( int feederID, int index ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( index );
	return e ? e->iconHandle : 0;
}

static void WiredFeeder_CharactersSelection( int feederID, int index ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( index );
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

// Skins reflect the currently selected character. wui_charSelected is a
// selectable-subset index (set by the characters feeder), so this resolves the
// character through the same selectable view — keeping both feeders in one index
// space.
static int WiredFeeder_SkinsCount( int feederID ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( wui_charSelected );
	return e ? e->manifest.numSkins : 0;
}

static const char *WiredFeeder_SkinsItemText( int feederID, int index, int column ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( wui_charSelected );
	if ( !e || index < 0 || index >= e->manifest.numSkins ) return "";
	return e->manifest.skins[index].name;
}

static void WiredFeeder_SkinsSelection( int feederID, int index ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( wui_charSelected );
	if ( !e || index < 0 || index >= e->manifest.numSkins ) return;
	wui_skinSelected = index;
	Cvar_Set( "skin", e->manifest.skins[index].name );
}

// Public character helpers — the select surface (keyboard/gamepad character
// cycling, the 3D preview's selection sync) operates on the same selectable-only
// index space as the feeder, so these route through the selectable view too.
int WiredFeeder_GetCharacterCount( void ) { return CL_Characters_SelectableCount(); }
int WiredFeeder_GetCharacterSelected( void ) { return wui_charSelected; }
const char *WiredFeeder_GetCharacterName( int index ) {
	const clCharacterEntry_t *e = CL_Characters_SelectableAt( index );
	return e ? e->dirname : NULL;
}
void WiredFeeder_SetCharacterSelected( int index ) {
	WiredFeeder_CharactersSelection( FEEDER_CHARACTERS, index );
}

// ── feeder registration ───────────────────────────────────────────────

void WiredUI_RegisterCoreFeeders( void ) {
	WiredUI_RegisterFeeder( FEEDER_SERVERS, "servers", WiredFeeder_ServerCount,
		WiredFeeder_ServerItemText, WiredFeeder_ServerSelection );
	WiredUI_RegisterFeeder( FEEDER_SERVERSTATUS, "serverstatus", WiredFeeder_ServerStatusCount,
		WiredFeeder_ServerStatusItemText, NULL );
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
	WiredUI_RegisterFeeder( FEEDER_PLAYER_LIST, "players", WiredFeeder_PlayerCount,
		WiredFeeder_PlayerItemText, WiredFeeder_PlayerSelection );
	WiredUI_RegisterFeeder( FEEDER_BOTS, "bots", WiredFeeder_BotCount,
		WiredFeeder_BotItemText, WiredFeeder_BotSelection );
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
	WiredFeeder_ClearServerSelection();
	WiredFeeder_ServerStatusCancel();

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: feeders registered (demos=%d, mods=%d)\n",
		wui_demoCount, wui_modCount );
}

#endif // FEAT_WIRED_UI
