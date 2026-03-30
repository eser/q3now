/*
===========================================================================
ui_callvote.c -- GUI callvote menu (6C / FEAT_CALLVOTE_MENU)

Accessible from the in-game menu (ESC → Call Vote).
Provides buttons for common vote types without needing console commands.
Enhanced: kick player selection, timelimit/fraglimit spincontrols.
===========================================================================
*/
#include "ui_local.h"

#if FEAT_CALLVOTE_MENU

#define CALLVOTE_FRAME      "menu/art/addbotframe"
#define CALLVOTE_BACK       "menu/art/back_0"
#define CALLVOTE_BACK_FOCUS  "menu/art/back_1"

#define ID_BACK          10
#define ID_RESTART       11
#define ID_NEXTMAP       12
#define ID_SHUFFLE       13
#define ID_KICK          14
#define ID_TIMELIMIT     15
#define ID_FRAGLIMIT     16
#define ID_KICKPLAYER    17
#define ID_UNLAGGED      18

#define SPACING          28

#define MAX_VOTEPLAYERS  16

// timelimit presets
static const char *timelimitNames[] = {
	"No Limit",
	"5 minutes",
	"10 minutes",
	"15 minutes",
	"20 minutes",
	"30 minutes",
	NULL
};
static int timelimitValues[] = { 0, 5, 10, 15, 20, 30 };

// fraglimit presets
static const char *fraglimitNames[] = {
	"No Limit",
	"10 frags",
	"15 frags",
	"20 frags",
	"25 frags",
	"30 frags",
	"50 frags",
	NULL
};
static int fraglimitValues[] = { 0, 10, 15, 20, 25, 30, 50 };

typedef struct {
	menuframework_s		menu;
	menubitmap_s		frame;
	menutext_s			banner;
	menutext_s			restart;
	menutext_s			nextmap;
	menutext_s			shuffle;
	menulist_s			kickPlayer;
	menutext_s			kick;
	menutext_s			unlagged;
	menulist_s			timelimit;
	menulist_s			fraglimit;
	menubitmap_s		back;

	// kick player data
	int					numPlayers;
	int					playerClientNums[MAX_VOTEPLAYERS];
	char				playerNames[MAX_VOTEPLAYERS][32];
	const char			*playerNamePtrs[MAX_VOTEPLAYERS + 1];	// NULL-terminated
} callvoteMenu_t;

static callvoteMenu_t	s_callvote;


/*
==================
CallVote_BuildPlayerList

Scans CS_PLAYERS for connected clients and builds a list for the kick dropdown.
==================
*/
static void CallVote_BuildPlayerList( void ) {
	int		n;
	int		numPlayers;
	char	info[MAX_INFO_STRING];
	char	serverinfo[MAX_INFO_STRING];
	const char	*name;

	trap_GetConfigString( CS_SERVERINFO, serverinfo, sizeof(serverinfo) );
	numPlayers = atoi( Info_ValueForKey( serverinfo, "sv_maxclients" ) );

	s_callvote.numPlayers = 0;

	for ( n = 0; n < numPlayers && s_callvote.numPlayers < MAX_VOTEPLAYERS; n++ ) {
		trap_GetConfigString( CS_PLAYERS + n, info, MAX_INFO_STRING );
		name = Info_ValueForKey( info, "n" );
		if ( !name[0] ) {
			continue;
		}

		s_callvote.playerClientNums[s_callvote.numPlayers] = n;
		Q_strncpyz( s_callvote.playerNames[s_callvote.numPlayers], name, sizeof(s_callvote.playerNames[0]) );
		Q_CleanStr( s_callvote.playerNames[s_callvote.numPlayers] );
		s_callvote.playerNamePtrs[s_callvote.numPlayers] = s_callvote.playerNames[s_callvote.numPlayers];
		s_callvote.numPlayers++;
	}

	s_callvote.playerNamePtrs[s_callvote.numPlayers] = NULL;
}


/*
==================
CallVote_Event
==================
*/
static void CallVote_Event( void *ptr, int notification ) {
	int idx;

	if ( notification != QM_ACTIVATED ) {
		return;
	}

	switch ( ((menucommon_s*)ptr)->id ) {
	case ID_RESTART:
		trap_Cmd_ExecuteText( EXEC_APPEND, "callvote map_restart\n" );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_NEXTMAP:
		trap_Cmd_ExecuteText( EXEC_APPEND, "callvote nextmap\n" );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_SHUFFLE:
		trap_Cmd_ExecuteText( EXEC_APPEND, "callvote shuffle\n" );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_KICK:
		idx = s_callvote.kickPlayer.curvalue;
		if ( idx >= 0 && idx < s_callvote.numPlayers ) {
			trap_Cmd_ExecuteText( EXEC_APPEND,
				va("callvote clientkick %i\n", s_callvote.playerClientNums[idx]) );
		}
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_UNLAGGED:
		// toggle: if currently enabled, vote to disable, and vice versa
		idx = (int)trap_Cvar_VariableValue( "g_unlagged" );
		trap_Cmd_ExecuteText( EXEC_APPEND,
			va("callvote g_unlagged %i\n", idx ? 0 : 1) );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_TIMELIMIT:
		idx = s_callvote.timelimit.curvalue;
		trap_Cmd_ExecuteText( EXEC_APPEND,
			va("callvote timelimit %i\n", timelimitValues[idx]) );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_FRAGLIMIT:
		idx = s_callvote.fraglimit.curvalue;
		trap_Cmd_ExecuteText( EXEC_APPEND,
			va("callvote fraglimit %i\n", fraglimitValues[idx]) );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
==================
CallVote_MenuInit
==================
*/
static void CallVote_MenuInit( void ) {
	int y;
	int gametype;
	char info[MAX_INFO_STRING];

	memset( &s_callvote, 0, sizeof( callvoteMenu_t ) );

	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) );
	gametype = atoi( Info_ValueForKey( info, "g_gametype" ) );

	CallVote_BuildPlayerList();

	s_callvote.menu.wrapAround = qtrue;
	s_callvote.menu.fullscreen = qfalse;

	s_callvote.frame.generic.type		= MTYPE_BITMAP;
	s_callvote.frame.generic.flags		= QMF_INACTIVE;
	s_callvote.frame.generic.name		= CALLVOTE_FRAME;
	s_callvote.frame.generic.x			= 320 - 233;
	s_callvote.frame.generic.y			= 240 - 166;
	s_callvote.frame.width				= 466;
	s_callvote.frame.height				= 332;

	y = 88;

	s_callvote.banner.generic.type		= MTYPE_PTEXT;
	s_callvote.banner.generic.flags		= QMF_CENTER_JUSTIFY | QMF_INACTIVE;
	s_callvote.banner.generic.x			= 320;
	s_callvote.banner.generic.y			= y;
	s_callvote.banner.string			= "CALL VOTE";
	s_callvote.banner.style				= UI_CENTER | UI_SMALLFONT;
	s_callvote.banner.color				= color_white;
	y += SPACING;

	// --- simple vote buttons ---

	s_callvote.restart.generic.type		= MTYPE_PTEXT;
	s_callvote.restart.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.restart.generic.x		= 320;
	s_callvote.restart.generic.y		= y;
	s_callvote.restart.generic.id		= ID_RESTART;
	s_callvote.restart.generic.callback	= CallVote_Event;
	s_callvote.restart.string			= "Map Restart";
	s_callvote.restart.style			= UI_CENTER | UI_SMALLFONT;
	s_callvote.restart.color			= color_red;
	y += SPACING;

	s_callvote.nextmap.generic.type		= MTYPE_PTEXT;
	s_callvote.nextmap.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.nextmap.generic.x		= 320;
	s_callvote.nextmap.generic.y		= y;
	s_callvote.nextmap.generic.id		= ID_NEXTMAP;
	s_callvote.nextmap.generic.callback	= CallVote_Event;
	s_callvote.nextmap.string			= "Next Map";
	s_callvote.nextmap.style			= UI_CENTER | UI_SMALLFONT;
	s_callvote.nextmap.color			= color_red;
	y += SPACING;

	// shuffle only visible in team games
	s_callvote.shuffle.generic.type		= MTYPE_PTEXT;
	s_callvote.shuffle.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.shuffle.generic.x		= 320;
	s_callvote.shuffle.generic.y		= y;
	s_callvote.shuffle.generic.id		= ID_SHUFFLE;
	s_callvote.shuffle.generic.callback	= CallVote_Event;
	s_callvote.shuffle.string			= "Shuffle Teams";
	s_callvote.shuffle.style			= UI_CENTER | UI_SMALLFONT;
	s_callvote.shuffle.color			= color_red;
	if ( gametype < GT_TEAM ) {
		s_callvote.shuffle.generic.flags |= QMF_GRAYED;
	}
	y += SPACING;

	// --- unlagged toggle ---

	s_callvote.unlagged.generic.type		= MTYPE_PTEXT;
	s_callvote.unlagged.generic.flags		= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.unlagged.generic.x			= 320;
	s_callvote.unlagged.generic.y			= y;
	s_callvote.unlagged.generic.id			= ID_UNLAGGED;
	s_callvote.unlagged.generic.callback	= CallVote_Event;
	s_callvote.unlagged.string				= (int)trap_Cvar_VariableValue( "g_unlagged" )
												? "Disable Unlagged"
												: "Enable Unlagged";
	s_callvote.unlagged.style				= UI_CENTER | UI_SMALLFONT;
	s_callvote.unlagged.color				= color_red;
	y += SPACING;

	// --- kick player: dropdown + button ---

	s_callvote.kickPlayer.generic.type		= MTYPE_SPINCONTROL;
	s_callvote.kickPlayer.generic.flags		= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_callvote.kickPlayer.generic.x			= 320;
	s_callvote.kickPlayer.generic.y			= y;
	s_callvote.kickPlayer.generic.name		= "Player:";
	s_callvote.kickPlayer.generic.id		= ID_KICKPLAYER;
	s_callvote.kickPlayer.itemnames			= s_callvote.playerNamePtrs;
	if ( s_callvote.numPlayers == 0 ) {
		s_callvote.kickPlayer.generic.flags |= QMF_GRAYED;
	}
	y += SMALLCHAR_HEIGHT + 2;

	s_callvote.kick.generic.type		= MTYPE_PTEXT;
	s_callvote.kick.generic.flags		= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.kick.generic.x			= 320;
	s_callvote.kick.generic.y			= y;
	s_callvote.kick.generic.id			= ID_KICK;
	s_callvote.kick.generic.callback	= CallVote_Event;
	s_callvote.kick.string				= "Vote Kick";
	s_callvote.kick.style				= UI_CENTER | UI_SMALLFONT;
	s_callvote.kick.color				= color_red;
	if ( s_callvote.numPlayers == 0 ) {
		s_callvote.kick.generic.flags |= QMF_GRAYED;
	}
	y += SPACING;

	// --- timelimit: dropdown + vote button ---

	s_callvote.timelimit.generic.type		= MTYPE_SPINCONTROL;
	s_callvote.timelimit.generic.flags		= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_callvote.timelimit.generic.x			= 320;
	s_callvote.timelimit.generic.y			= y;
	s_callvote.timelimit.generic.name		= "Timelimit:";
	s_callvote.timelimit.generic.id			= ID_TIMELIMIT;
	s_callvote.timelimit.generic.callback	= CallVote_Event;
	s_callvote.timelimit.itemnames			= timelimitNames;
	s_callvote.timelimit.curvalue			= 3;	// default: 15 minutes
	y += SMALLCHAR_HEIGHT + 4;

	// --- fraglimit: dropdown + vote button ---

	s_callvote.fraglimit.generic.type		= MTYPE_SPINCONTROL;
	s_callvote.fraglimit.generic.flags		= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_callvote.fraglimit.generic.x			= 320;
	s_callvote.fraglimit.generic.y			= y;
	s_callvote.fraglimit.generic.name		= "Fraglimit:";
	s_callvote.fraglimit.generic.id			= ID_FRAGLIMIT;
	s_callvote.fraglimit.generic.callback	= CallVote_Event;
	s_callvote.fraglimit.itemnames			= fraglimitNames;
	s_callvote.fraglimit.curvalue			= 5;	// default: 30 frags
	y += SPACING + 8;

	// --- back button ---

	s_callvote.back.generic.type		= MTYPE_BITMAP;
	s_callvote.back.generic.name		= CALLVOTE_BACK;
	s_callvote.back.generic.flags		= QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.back.generic.callback	= CallVote_Event;
	s_callvote.back.generic.id			= ID_BACK;
	s_callvote.back.generic.x			= 320 - 128;
	s_callvote.back.generic.y			= y;
	s_callvote.back.width				= 128;
	s_callvote.back.height				= 64;
	s_callvote.back.focuspic			= CALLVOTE_BACK_FOCUS;

	Menu_AddItem( &s_callvote.menu, &s_callvote.frame );
	Menu_AddItem( &s_callvote.menu, &s_callvote.banner );
	Menu_AddItem( &s_callvote.menu, &s_callvote.restart );
	Menu_AddItem( &s_callvote.menu, &s_callvote.nextmap );
	Menu_AddItem( &s_callvote.menu, &s_callvote.shuffle );
	Menu_AddItem( &s_callvote.menu, &s_callvote.unlagged );
	Menu_AddItem( &s_callvote.menu, &s_callvote.kickPlayer );
	Menu_AddItem( &s_callvote.menu, &s_callvote.kick );
	Menu_AddItem( &s_callvote.menu, &s_callvote.timelimit );
	Menu_AddItem( &s_callvote.menu, &s_callvote.fraglimit );
	Menu_AddItem( &s_callvote.menu, &s_callvote.back );
}

/*
==================
UI_CallVoteMenu
==================
*/
void UI_CallVoteMenu( void ) {
	CallVote_MenuInit();
	UI_PushMenu( &s_callvote.menu );
}

#endif // FEAT_CALLVOTE_MENU
