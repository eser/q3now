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
/*
=============================================================================

SINGLE PLAYER POSTGAME MENU

=============================================================================
*/

#include "ui_local.h"

#define MAX_SCOREBOARD_CLIENTS		8

#define ART_MENU0		"menu/art/menu_0"
#define ART_MENU1		"menu/art/menu_1"
#define ART_REPLAY0		"menu/art/replay_0"
#define ART_REPLAY1		"menu/art/replay_1"
#define ART_NEXT0		"menu/art/next_0"
#define ART_NEXT1		"menu/art/next_1"

#define ID_AGAIN		10
#define ID_NEXT			11
#define ID_MENU			12

typedef struct {
	menuframework_s	menu;
	menubitmap_s	item_again;
	menubitmap_s	item_next;
	menubitmap_s	item_menu;

	int				serverId;

	int				clientNums[MAX_SCOREBOARD_CLIENTS];
	int				ranks[MAX_SCOREBOARD_CLIENTS];
	int				scores[MAX_SCOREBOARD_CLIENTS];

	char			placeNames[3][64];

	int				level;
	int				numClients;
	int				won;
	qboolean		playedSound[6];
	int				lastTier;
	sfxHandle_t		winnerSound;
} postgameMenuInfo_t;

static postgameMenuInfo_t	postgameMenuInfo;
static char					arenainfo[MAX_INFO_VALUE];


/*
=================
UI_SPPostgameMenu_AgainEvent
=================
*/
static void UI_SPPostgameMenu_AgainEvent( void* ptr, int event )
{
	if (event != QM_ACTIVATED) {
		return;
	}
	UI_PopMenu();
	trap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\n" );
}


/*
=================
UI_SPPostgameMenu_NextEvent
=================
*/
static void UI_SPPostgameMenu_NextEvent( void* ptr, int event ) {
	int			currentSet;
	int			levelSet;
	int			level;
	int			currentLevel;
	const char	*arenaInfo;

	if (event != QM_ACTIVATED) {
		return;
	}
	UI_PopMenu();

	// handle specially if we just won the training map
	if( postgameMenuInfo.won == 0 ) {
		level = 0;
	}
	else {
		level = postgameMenuInfo.level + 1;
	}
	levelSet = level / ARENAS_PER_TIER;

	currentLevel = UI_GetCurrentGame();
	if( currentLevel == -1 ) {
		currentLevel = postgameMenuInfo.level;
	}
	currentSet = currentLevel / ARENAS_PER_TIER;

	if( levelSet > currentSet || levelSet == UI_GetNumSPTiers() ) {
		level = currentLevel;
	}

	arenaInfo = UI_GetArenaInfoByNumber( level );
	if ( !arenaInfo ) {
		return;
	}

	UI_SPArena_Start( arenaInfo );
}


/*
=================
UI_SPPostgameMenu_MenuEvent
=================
*/
static void UI_SPPostgameMenu_MenuEvent( void* ptr, int event )
{
	if (event != QM_ACTIVATED) {
		return;
	}
	UI_PopMenu();
	trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect; levelselect\n" );
}


/*
=================
UI_SPPostgameMenu_MenuKey
=================
*/
static sfxHandle_t UI_SPPostgameMenu_MenuKey( int key ) {
	if( key == K_ESCAPE || key == K_MOUSE2 ) {
		return 0;
	}

	return Menu_DefaultKey( &postgameMenuInfo.menu, key );
}

/*
=================
UI_SPPostgameMenu_MenuDrawScoreLine
=================
*/
static void UI_SPPostgameMenu_MenuDrawScoreLine( int n, int y ) {
	int		rank;
	char	name[64];
	char	info[MAX_INFO_STRING];

	if( n > (postgameMenuInfo.numClients + 1) ) {
		n -= (postgameMenuInfo.numClients + 2);
	}

	if( n >= postgameMenuInfo.numClients ) {
		return;
	}

	rank = postgameMenuInfo.ranks[n];
	if( rank & RANK_TIED_FLAG ) {
		UI_DrawString( 640 - 31 * SMALLCHAR_WIDTH, y, "(tie)", UI_LEFT|UI_SMALLFONT, color_white );
		rank &= ~RANK_TIED_FLAG;
	}
	trap_GetConfigString( CS_PLAYERS + postgameMenuInfo.clientNums[n], info, MAX_INFO_STRING );
	Q_strncpyz( name, Info_ValueForKey( info, "n" ), sizeof(name) );
	Q_CleanStr( name );

	UI_DrawString( 640 - 25 * SMALLCHAR_WIDTH, y, va( "#%i: %-16s %2i", rank + 1, name, postgameMenuInfo.scores[n] ), UI_LEFT|UI_SMALLFONT, color_white );
}


/*
=================
UI_SPPostgameMenu_MenuDraw
=================
*/
static void UI_SPPostgameMenu_MenuDraw( void ) {
	int		serverId;
	char	info[MAX_INFO_STRING];

	trap_GetConfigString( CS_SYSTEMINFO, info, sizeof(info) );
	serverId = atoi( Info_ValueForKey( info, "sv_serverid" ) );
	if( serverId != postgameMenuInfo.serverId ) {
		UI_PopMenu();
		return;
	}

	if( uis.demoversion ) {
		if( postgameMenuInfo.won == 1 && UI_ShowTierVideo( 8 )) {
			trap_Cvar_Set( "nextmap", "" );
			trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect; cinematic demoEnd.RoQ\n" );
			return;
		}
	}
	else if( postgameMenuInfo.won > -1 && UI_ShowTierVideo( postgameMenuInfo.won + 1 )) {
		if( postgameMenuInfo.won == postgameMenuInfo.lastTier ) {
			trap_Cvar_Set( "nextmap", "" );
			trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect; cinematic end.RoQ\n" );
			return;
		}

		trap_Cvar_SetValue( "ui_spSelection", postgameMenuInfo.won * ARENAS_PER_TIER );
		trap_Cvar_Set( "nextmap", "levelselect" );
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "disconnect; cinematic tier%i.RoQ\n", postgameMenuInfo.won + 1 ) );
		return;
	}

	postgameMenuInfo.item_again.generic.flags &= ~QMF_INACTIVE;
	postgameMenuInfo.item_next.generic.flags &= ~QMF_INACTIVE;
	postgameMenuInfo.item_menu.generic.flags &= ~QMF_INACTIVE;

	Menu_Draw( &postgameMenuInfo.menu );
}


/*
=================
UI_SPPostgameMenu_Cache
=================
*/
void UI_SPPostgameMenu_Cache( void ) {
	qboolean	buildscript;

	buildscript = trap_Cvar_VariableValue("com_buildscript");

	trap_R_RegisterShaderNoMip( ART_MENU0 );
	trap_R_RegisterShaderNoMip( ART_MENU1 );
	trap_R_RegisterShaderNoMip( ART_REPLAY0 );
	trap_R_RegisterShaderNoMip( ART_REPLAY1 );
	trap_R_RegisterShaderNoMip( ART_NEXT0 );
	trap_R_RegisterShaderNoMip( ART_NEXT1 );

	if( buildscript ) {
		trap_S_RegisterSound( "music/loss.wav", qfalse );
		trap_S_RegisterSound( "music/win.wav", qfalse );
		trap_S_RegisterSound( "sound/player/announce/youwin.wav", qfalse );
	}
}


/*
=================
UI_SPPostgameMenu_Init
=================
*/
static void UI_SPPostgameMenu_Init( void ) {
	postgameMenuInfo.menu.wrapAround	= qtrue;
	postgameMenuInfo.menu.key			= UI_SPPostgameMenu_MenuKey;
	postgameMenuInfo.menu.draw			= UI_SPPostgameMenu_MenuDraw;

	UI_SPPostgameMenu_Cache();

	postgameMenuInfo.item_menu.generic.type			= MTYPE_BITMAP;
	postgameMenuInfo.item_menu.generic.name			= ART_MENU0;
	postgameMenuInfo.item_menu.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_INACTIVE;
	postgameMenuInfo.item_menu.generic.x			= 0;
	postgameMenuInfo.item_menu.generic.y			= 480-64;
	postgameMenuInfo.item_menu.generic.callback		= UI_SPPostgameMenu_MenuEvent;
	postgameMenuInfo.item_menu.generic.id			= ID_MENU;
	postgameMenuInfo.item_menu.width				= 128;
	postgameMenuInfo.item_menu.height				= 64;
	postgameMenuInfo.item_menu.focuspic				= ART_MENU1;

	postgameMenuInfo.item_again.generic.type		= MTYPE_BITMAP;
	postgameMenuInfo.item_again.generic.name		= ART_REPLAY0;
	postgameMenuInfo.item_again.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_INACTIVE;
	postgameMenuInfo.item_again.generic.x			= 320;
	postgameMenuInfo.item_again.generic.y			= 480-64;
	postgameMenuInfo.item_again.generic.callback	= UI_SPPostgameMenu_AgainEvent;
	postgameMenuInfo.item_again.generic.id			= ID_AGAIN;
	postgameMenuInfo.item_again.width				= 128;
	postgameMenuInfo.item_again.height				= 64;
	postgameMenuInfo.item_again.focuspic			= ART_REPLAY1;

	postgameMenuInfo.item_next.generic.type			= MTYPE_BITMAP;
	postgameMenuInfo.item_next.generic.name			= ART_NEXT0;
	postgameMenuInfo.item_next.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_INACTIVE;
	postgameMenuInfo.item_next.generic.x			= 640;
	postgameMenuInfo.item_next.generic.y			= 480-64;
	postgameMenuInfo.item_next.generic.callback		= UI_SPPostgameMenu_NextEvent;
	postgameMenuInfo.item_next.generic.id			= ID_NEXT;
	postgameMenuInfo.item_next.width				= 128;
	postgameMenuInfo.item_next.height				= 64;
	postgameMenuInfo.item_next.focuspic				= ART_NEXT1;

	Menu_AddItem( &postgameMenuInfo.menu, ( void * )&postgameMenuInfo.item_menu );
	Menu_AddItem( &postgameMenuInfo.menu, ( void * )&postgameMenuInfo.item_again );
	Menu_AddItem( &postgameMenuInfo.menu, ( void * )&postgameMenuInfo.item_next );
}


static void Prepname( int index ) {
	int		len;
	char	name[64];
	char	info[MAX_INFO_STRING];

	trap_GetConfigString( CS_PLAYERS + postgameMenuInfo.clientNums[index], info, MAX_INFO_STRING );
	Q_strncpyz( name, Info_ValueForKey( info, "n" ), sizeof(name) );
	Q_CleanStr( name );
	len = strlen( name );

	while( len && UI_ProportionalStringWidth( name ) > 256 ) {
		len--;
		name[len] = 0;
	}

	Q_strncpyz( postgameMenuInfo.placeNames[index], name, sizeof(postgameMenuInfo.placeNames[index]) );
}


/*
=================
UI_SPPostgameMenu_f
=================
*/
void UI_SPPostgameMenu_f( void ) {
	int			playerGameRank;
	int			playerClientNum;
	int			n;
	const char	*arena;
	int			awardValues[6];
	char		map[MAX_QPATH];
	char		info[MAX_INFO_STRING];

	memset( &postgameMenuInfo, 0, sizeof(postgameMenuInfo) );

	trap_GetConfigString( CS_SYSTEMINFO, info, sizeof(info) );
	postgameMenuInfo.serverId = atoi( Info_ValueForKey( info, "sv_serverid" ) );

	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) );
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof(map) );
	arena = UI_GetArenaInfoByMap( map );
	if ( !arena ) {
		return;
	}
	Q_strncpyz( arenainfo, arena, sizeof(arenainfo) );

	postgameMenuInfo.level = atoi( Info_ValueForKey( arenainfo, "num" ) );

	postgameMenuInfo.numClients = atoi( UI_Argv( 1 ) );
	playerClientNum = atoi( UI_Argv( 2 ) );
	playerGameRank = 8;		// in case they ended game as a spectator

	if( postgameMenuInfo.numClients > MAX_SCOREBOARD_CLIENTS ) {
		postgameMenuInfo.numClients = MAX_SCOREBOARD_CLIENTS;
	}

	for( n = 0; n < postgameMenuInfo.numClients; n++ ) {
		postgameMenuInfo.clientNums[n] = atoi( UI_Argv( 8 + n * 3 + 1 ) );
		postgameMenuInfo.ranks[n] = atoi( UI_Argv( 8 + n * 3 + 2 ) );
		postgameMenuInfo.scores[n] = atoi( UI_Argv( 8 + n * 3 + 3 ) );

		if( postgameMenuInfo.clientNums[n] == playerClientNum ) {
			playerGameRank = (postgameMenuInfo.ranks[n] & ~RANK_TIED_FLAG) + 1;
		}
	}

	UI_SetBestScore( postgameMenuInfo.level, playerGameRank );

	// process award stats and prepare presentation data
	awardValues[AWARD_ACCURACY] = atoi( UI_Argv( 3 ) );
	awardValues[AWARD_IMPRESSIVE] = atoi( UI_Argv( 4 ) );
	awardValues[AWARD_EXCELLENT] = atoi( UI_Argv( 5 ) );
	awardValues[AWARD_GAUNTLET] = atoi( UI_Argv( 6 ) );
	awardValues[AWARD_FRAGS] = atoi( UI_Argv( 7 ) );
	awardValues[AWARD_PERFECT] = atoi( UI_Argv( 8 ) );

	if ( playerGameRank == 1 ) {
		postgameMenuInfo.won = UI_TierCompleted( postgameMenuInfo.level );
	}
	else {
		postgameMenuInfo.won = -1;
	}

	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;

	UI_SPPostgameMenu_Init();
	UI_PushMenu( &postgameMenuInfo.menu );

	if ( playerGameRank == 1 ) {
		Menu_SetCursorToItem( &postgameMenuInfo.menu, &postgameMenuInfo.item_next );
	}
	else {
		Menu_SetCursorToItem( &postgameMenuInfo.menu, &postgameMenuInfo.item_again );
	}

	Prepname( 0 );
	Prepname( 1 );
	Prepname( 2 );

	if ( playerGameRank != 1 ) {
		postgameMenuInfo.winnerSound = trap_S_RegisterSound( va( "sound/player/announce/%s_wins.wav", postgameMenuInfo.placeNames[0] ), qfalse );
		trap_Cmd_ExecuteText( EXEC_APPEND, "music music/loss\n" );
	}
	else {
		postgameMenuInfo.winnerSound = trap_S_RegisterSound( "sound/player/announce/youwin.wav", qfalse );
		trap_Cmd_ExecuteText( EXEC_APPEND, "music music/win\n" );
	}

	postgameMenuInfo.lastTier = UI_GetNumSPTiers();
	if ( UI_GetSpecialArenaInfo( "final" ) ) {
		postgameMenuInfo.lastTier++;
	}
}
