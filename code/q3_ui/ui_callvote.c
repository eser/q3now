/*
===========================================================================
ui_callvote.c -- GUI callvote menu (6C / FEAT_CALLVOTE_MENU)

Accessible from the in-game menu (ESC → Call Vote).
Provides buttons for common vote types without needing console commands.
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

#define SPACING          28

typedef struct {
	menuframework_s		menu;
	menubitmap_s		frame;
	menutext_s			banner;
	menutext_s			restart;
	menutext_s			nextmap;
	menutext_s			shuffle;
	menutext_s			kick;
	menutext_s			timelimit;
	menutext_s			fraglimit;
	menubitmap_s		back;
} callvoteMenu_t;

static callvoteMenu_t	s_callvote;

/*
==================
CallVote_Event
==================
*/
static void CallVote_Event( void *ptr, int notification ) {
	if ( notification != QM_ACTIVATED ) {
		return;
	}

	switch ( ((menucommon_s*)ptr)->id ) {
	case ID_RESTART:
		trap_Cmd_ExecuteText( EXEC_APPEND, "callvote map_restart\n" );
		UI_PopMenu();
		UI_PopMenu();	// close in-game menu too
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
		// kick requires a player name — for now, print hint
		trap_Cmd_ExecuteText( EXEC_APPEND, "echo \"Usage: /callvote kick <playername>\"\n" );
		UI_PopMenu();
		break;
	case ID_TIMELIMIT:
		trap_Cmd_ExecuteText( EXEC_APPEND, "callvote timelimit 15\n" );
		UI_PopMenu();
		UI_PopMenu();
		break;
	case ID_FRAGLIMIT:
		trap_Cmd_ExecuteText( EXEC_APPEND, "callvote fraglimit 30\n" );
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

	memset( &s_callvote, 0, sizeof( callvoteMenu_t ) );

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

	s_callvote.shuffle.generic.type		= MTYPE_PTEXT;
	s_callvote.shuffle.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.shuffle.generic.x		= 320;
	s_callvote.shuffle.generic.y		= y;
	s_callvote.shuffle.generic.id		= ID_SHUFFLE;
	s_callvote.shuffle.generic.callback	= CallVote_Event;
	s_callvote.shuffle.string			= "Shuffle Teams";
	s_callvote.shuffle.style			= UI_CENTER | UI_SMALLFONT;
	s_callvote.shuffle.color			= color_red;
	y += SPACING;

	s_callvote.timelimit.generic.type		= MTYPE_PTEXT;
	s_callvote.timelimit.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.timelimit.generic.x		= 320;
	s_callvote.timelimit.generic.y		= y;
	s_callvote.timelimit.generic.id		= ID_TIMELIMIT;
	s_callvote.timelimit.generic.callback = CallVote_Event;
	s_callvote.timelimit.string			= "Timelimit 15";
	s_callvote.timelimit.style			= UI_CENTER | UI_SMALLFONT;
	s_callvote.timelimit.color			= color_red;
	y += SPACING;

	s_callvote.fraglimit.generic.type		= MTYPE_PTEXT;
	s_callvote.fraglimit.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_callvote.fraglimit.generic.x		= 320;
	s_callvote.fraglimit.generic.y		= y;
	s_callvote.fraglimit.generic.id		= ID_FRAGLIMIT;
	s_callvote.fraglimit.generic.callback = CallVote_Event;
	s_callvote.fraglimit.string			= "Fraglimit 30";
	s_callvote.fraglimit.style			= UI_CENTER | UI_SMALLFONT;
	s_callvote.fraglimit.color			= color_red;
	y += SPACING + 8;

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
