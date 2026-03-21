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
=======================================================================

ADD BOTS MENU

=======================================================================
*/


#include "ui_local.h"


#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"	
#define ART_FIGHT0			"menu/art/accept_0"
#define ART_FIGHT1			"menu/art/accept_1"
#define ART_BACKGROUND		"menu/art/addbotframe"
#define ART_ARROWS			"menu/art/arrows_vert_0"
#define ART_ARROWUP			"menu/art/arrows_vert_top"
#define ART_ARROWDOWN		"menu/art/arrows_vert_bot"

#define ID_BACK				10
#define ID_GO				11
#define ID_LIST				12
#define ID_UP				13
#define ID_DOWN				14
#define ID_SKILL			15
#define ID_TEAM				16
#define ID_SKILLSLIDER		17
#define ID_BOTNAME0			20
#define ID_BOTNAME1			21
#define ID_BOTNAME2			22
#define ID_BOTNAME3			23
#define ID_BOTNAME4			24
#define ID_BOTNAME5			25
#define ID_BOTNAME6			26

#define ART_ICON_HIGHLIGHT	"menu/art/opponents_selected"


typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	background;

	menubitmap_s	arrows;
	menubitmap_s	up;
	menubitmap_s	down;

	menutext_s		bots[7];

	menubitmap_s	iconHighlight;	// highlight overlay behind bot icon
	menubitmap_s	icon;		// bot portrait icon
	menulist_s		skill;
	menuslider_s	skillSlider;
	menulist_s		team;
	menubitmap_s	go;
	menubitmap_s	back;

	int				gametype;
	int				numBots;
	int				delay;
	int				baseBotNum;
	int				selectedBotNum;
	int				sortedBotNums[MAX_BOTS];
	char			botnames[7][32];
	char			boticon[MAX_QPATH];	// path to selected bot's icon
} addBotsMenuInfo_t;

static addBotsMenuInfo_t	addBotsMenuInfo;

static void UI_AddBotsMenu_SetBotNames( void );

/*
=================
AddBots_SetBotIcon

Build the icon path for the currently selected bot and reset the shader
so Bitmap_Draw reloads it.
=================
*/
static void AddBots_SetBotIcon( void )
{
	const char	*info;
	const char	*model;
	char		modelBuf[MAX_QPATH];
	char		*skin;

	info = UI_GetBotInfoByNumber( addBotsMenuInfo.sortedBotNums[addBotsMenuInfo.baseBotNum + addBotsMenuInfo.selectedBotNum] );
	model = Info_ValueForKey( info, "model" );

	Q_strncpyz( modelBuf, model, sizeof(modelBuf) );
	skin = strrchr( modelBuf, '/' );
	if ( skin ) {
		*skin++ = '\0';
	} else {
		skin = "default";
	}

	Com_sprintf( addBotsMenuInfo.boticon, sizeof(addBotsMenuInfo.boticon),
		"models/players/%s/icon_%s", modelBuf, skin );

	// force shader reload
	addBotsMenuInfo.icon.shader = 0;
}


/*
=================
UI_AddBotsMenu_FightEvent
=================
*/
static void UI_AddBotsMenu_FightEvent( void* ptr, int event ) {
	const char	*team;
	int			skill;

	if (event != QM_ACTIVATED) {
		return;
	}

	team = addBotsMenuInfo.team.itemnames[addBotsMenuInfo.team.curvalue];
	skill = addBotsMenuInfo.skill.curvalue + 1;

	trap_Cmd_ExecuteText( EXEC_APPEND, va("addbot %s %i %s %i\n",
		addBotsMenuInfo.botnames[addBotsMenuInfo.selectedBotNum], skill, team, addBotsMenuInfo.delay) );

	addBotsMenuInfo.delay += 1500;
}


/*
=================
UI_AddBotsMenu_SkillSliderEvent
=================
*/
static void UI_AddBotsMenu_SkillSliderEvent( void* ptr, int event ) {
	if (event != QM_ACTIVATED) {
		return;
	}

	addBotsMenuInfo.skill.curvalue = (int)addBotsMenuInfo.skillSlider.curvalue;
}


/*
=================
UI_AddBotsMenu_RecolorBots

Recolor all bot name items based on team selection and gametype.
The currently selected bot stays white.
=================
*/
static void UI_AddBotsMenu_RecolorBots( void ) {
	int		n;
	float	*teamColor;

	if ( addBotsMenuInfo.gametype >= GT_TEAM ) {
		teamColor = ( addBotsMenuInfo.team.curvalue == 0 ) ? color_red : color_blue;
	} else {
		teamColor = color_orange;
	}

	for ( n = 0; n < 7; n++ ) {
		if ( n == addBotsMenuInfo.selectedBotNum ) {
			addBotsMenuInfo.bots[n].color = color_white;
		} else {
			addBotsMenuInfo.bots[n].color = teamColor;
		}
	}
}


/*
=================
UI_AddBotsMenu_TeamEvent
=================
*/
static void UI_AddBotsMenu_TeamEvent( void* ptr, int event ) {
	if (event != QM_ACTIVATED) {
		return;
	}

	UI_AddBotsMenu_RecolorBots();
}


/*
=================
UI_AddBotsMenu_BotEvent
=================
*/
static void UI_AddBotsMenu_BotEvent( void* ptr, int event ) {
	int		prev;
	float	*teamColor;

	if (event != QM_ACTIVATED) {
		return;
	}

	prev = addBotsMenuInfo.selectedBotNum;

	/* deselect previous — use team-aware color */
	if ( addBotsMenuInfo.gametype >= GT_TEAM ) {
		teamColor = ( addBotsMenuInfo.team.curvalue == 0 ) ? color_red : color_blue;
	} else {
		teamColor = color_orange;
	}
	addBotsMenuInfo.bots[prev].color = teamColor;

	addBotsMenuInfo.selectedBotNum = ((menucommon_s*)ptr)->id - ID_BOTNAME0;
	addBotsMenuInfo.bots[addBotsMenuInfo.selectedBotNum].color = color_white;

	AddBots_SetBotIcon();
}


/*
=================
UI_AddBotsMenu_Key
=================
*/
static sfxHandle_t UI_AddBotsMenu_Key( int key ) {
	switch ( key ) {
	case K_MWHEELUP:
		if ( addBotsMenuInfo.baseBotNum > 0 ) {
			addBotsMenuInfo.baseBotNum--;
			UI_AddBotsMenu_SetBotNames();
			return menu_move_sound;
		}
		return menu_buzz_sound;

	case K_MWHEELDOWN:
		if ( addBotsMenuInfo.baseBotNum + 7 < addBotsMenuInfo.numBots ) {
			addBotsMenuInfo.baseBotNum++;
			UI_AddBotsMenu_SetBotNames();
			return menu_move_sound;
		}
		return menu_buzz_sound;
	}

	return Menu_DefaultKey( &addBotsMenuInfo.menu, key );
}


/*
=================
UI_AddBotsMenu_BackEvent
=================
*/
static void UI_AddBotsMenu_BackEvent( void* ptr, int event ) {
	if (event != QM_ACTIVATED) {
		return;
	}
	UI_PopMenu();
}


/*
=================
UI_AddBotsMenu_SetBotNames
=================
*/
static void UI_AddBotsMenu_SetBotNames( void ) {
	int			n;
	const char	*info;

	for ( n = 0; n < 7; n++ ) {
		info = UI_GetBotInfoByNumber( addBotsMenuInfo.sortedBotNums[addBotsMenuInfo.baseBotNum + n] );
		Q_strncpyz( addBotsMenuInfo.botnames[n], Info_ValueForKey( info, "name" ), sizeof(addBotsMenuInfo.botnames[n]) );
	}

}


/*
=================
UI_AddBotsMenu_UpEvent
=================
*/
static void UI_AddBotsMenu_UpEvent( void* ptr, int event ) {
	if (event != QM_ACTIVATED) {
		return;
	}

	if( addBotsMenuInfo.baseBotNum > 0 ) {
		addBotsMenuInfo.baseBotNum--;
		UI_AddBotsMenu_SetBotNames();
	}
}


/*
=================
UI_AddBotsMenu_DownEvent
=================
*/
static void UI_AddBotsMenu_DownEvent( void* ptr, int event ) {
	if (event != QM_ACTIVATED) {
		return;
	}

	if( addBotsMenuInfo.baseBotNum + 7 < addBotsMenuInfo.numBots ) {
		addBotsMenuInfo.baseBotNum++;
		UI_AddBotsMenu_SetBotNames();
	}
}


/*
=================
UI_AddBotsMenu_GetSortedBotNums
=================
*/
static int QDECL UI_AddBotsMenu_SortCompare( const void *arg1, const void *arg2 ) {
	int			num1, num2;
	const char	*info1, *info2;
	const char	*name1, *name2;

	num1 = *(int *)arg1;
	num2 = *(int *)arg2;

	info1 = UI_GetBotInfoByNumber( num1 );
	info2 = UI_GetBotInfoByNumber( num2 );

	name1 = Info_ValueForKey( info1, "name" );
	name2 = Info_ValueForKey( info2, "name" );

	return Q_stricmp( name1, name2 );
}

static void UI_AddBotsMenu_GetSortedBotNums( void ) {
	int		n;

	// initialize the array
	for( n = 0; n < addBotsMenuInfo.numBots; n++ ) {
		addBotsMenuInfo.sortedBotNums[n] = n;
	}

	qsort( addBotsMenuInfo.sortedBotNums, addBotsMenuInfo.numBots, sizeof(addBotsMenuInfo.sortedBotNums[0]), UI_AddBotsMenu_SortCompare );
}

/*
=================
UI_AddBotsMenu_Init
=================
*/
static const char *skillNames[] = {
	"I Can Win",
	"Bring It On",
	"Hurt Me Plenty",
	"Hardcore",
	"Nightmare!",
	NULL
};

static const char *teamNames1[] = {
	"Free",
	NULL
};

static const char *teamNames2[] = {
	"Red",
	"Blue",
	NULL
};

static void UI_AddBotsMenu_Init( void ) {
	int		n;
	int		y;
	int		gametype;
	int		count;
	char	info[MAX_INFO_STRING];

	trap_GetConfigString(CS_SERVERINFO, info, MAX_INFO_STRING);   
	gametype = atoi( Info_ValueForKey( info,"g_gametype" ) );

	memset( &addBotsMenuInfo, 0 ,sizeof(addBotsMenuInfo) );
	addBotsMenuInfo.menu.fullscreen = qfalse;
	addBotsMenuInfo.menu.wrapAround = qtrue;
	addBotsMenuInfo.menu.key        = UI_AddBotsMenu_Key;
	addBotsMenuInfo.delay = 1000;
	addBotsMenuInfo.gametype = gametype;

	UI_AddBots_Cache();

	addBotsMenuInfo.numBots = UI_GetNumBots();
	count = addBotsMenuInfo.numBots < 7 ? addBotsMenuInfo.numBots : 7;

	addBotsMenuInfo.banner.generic.type			= MTYPE_BTEXT;
	addBotsMenuInfo.banner.generic.x			= 320;
	addBotsMenuInfo.banner.generic.y			= 16;
	addBotsMenuInfo.banner.string				= "ADD BOTS";
	addBotsMenuInfo.banner.color				= color_white;
	addBotsMenuInfo.banner.style				= UI_CENTER;

	addBotsMenuInfo.background.generic.type		= MTYPE_BITMAP;
	addBotsMenuInfo.background.generic.name		= ART_BACKGROUND;
	addBotsMenuInfo.background.generic.flags	= QMF_INACTIVE;
	addBotsMenuInfo.background.generic.x		= 320-233;
	addBotsMenuInfo.background.generic.y		= 240-166;
	addBotsMenuInfo.background.width			= 466;
	addBotsMenuInfo.background.height			= 332;

	addBotsMenuInfo.arrows.generic.type  = MTYPE_BITMAP;
	addBotsMenuInfo.arrows.generic.name  = ART_ARROWS;
	addBotsMenuInfo.arrows.generic.flags = QMF_INACTIVE;
	addBotsMenuInfo.arrows.generic.x	 = 200;
	addBotsMenuInfo.arrows.generic.y	 = 128;
	addBotsMenuInfo.arrows.width  	     = 64;
	addBotsMenuInfo.arrows.height  	     = 128;

	addBotsMenuInfo.up.generic.type	    = MTYPE_BITMAP;
	addBotsMenuInfo.up.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	addBotsMenuInfo.up.generic.x		= 200;
	addBotsMenuInfo.up.generic.y		= 128;
	addBotsMenuInfo.up.generic.id	    = ID_UP;
	addBotsMenuInfo.up.generic.callback = UI_AddBotsMenu_UpEvent;
	addBotsMenuInfo.up.width  		    = 64;
	addBotsMenuInfo.up.height  		    = 64;
	addBotsMenuInfo.up.focuspic         = ART_ARROWUP;

	addBotsMenuInfo.down.generic.type	  = MTYPE_BITMAP;
	addBotsMenuInfo.down.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	addBotsMenuInfo.down.generic.x		  = 200;
	addBotsMenuInfo.down.generic.y		  = 128+64;
	addBotsMenuInfo.down.generic.id	      = ID_DOWN;
	addBotsMenuInfo.down.generic.callback = UI_AddBotsMenu_DownEvent;
	addBotsMenuInfo.down.width  		  = 64;
	addBotsMenuInfo.down.height  		  = 64;
	addBotsMenuInfo.down.focuspic         = ART_ARROWDOWN;

	for( n = 0, y = 120; n < count; n++, y += 20 ) {
		addBotsMenuInfo.bots[n].generic.type		= MTYPE_PTEXT;
		addBotsMenuInfo.bots[n].generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
		addBotsMenuInfo.bots[n].generic.id			= ID_BOTNAME0 + n;
		addBotsMenuInfo.bots[n].generic.x			= 320 - 56;
		addBotsMenuInfo.bots[n].generic.y			= y;
		addBotsMenuInfo.bots[n].generic.callback	= UI_AddBotsMenu_BotEvent;
		addBotsMenuInfo.bots[n].string				= addBotsMenuInfo.botnames[n];
		if ( gametype >= GT_TEAM ) {
			addBotsMenuInfo.bots[n].color			= color_red;
		} else {
			addBotsMenuInfo.bots[n].color			= color_orange;
		}
		addBotsMenuInfo.bots[n].style				= UI_LEFT|UI_SMALLFONT;
	}

	y += 12;
	addBotsMenuInfo.skill.generic.type		= MTYPE_SPINCONTROL;
	addBotsMenuInfo.skill.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	addBotsMenuInfo.skill.generic.x			= 320;
	addBotsMenuInfo.skill.generic.y			= y;
	addBotsMenuInfo.skill.generic.name		= "Skill:";
	addBotsMenuInfo.skill.generic.id		= ID_SKILL;
	addBotsMenuInfo.skill.itemnames			= skillNames;
	addBotsMenuInfo.skill.curvalue			= Com_Clamp( 0, 4, (int)trap_Cvar_VariableValue( "g_spSkill" ) - 1 );

	y += SMALLCHAR_HEIGHT;
	addBotsMenuInfo.skillSlider.generic.type		= MTYPE_SLIDER;
	addBotsMenuInfo.skillSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	addBotsMenuInfo.skillSlider.generic.x			= 320;
	addBotsMenuInfo.skillSlider.generic.y			= y;
	addBotsMenuInfo.skillSlider.generic.name		= "Skill:";
	addBotsMenuInfo.skillSlider.generic.id			= ID_SKILLSLIDER;
	addBotsMenuInfo.skillSlider.generic.callback	= UI_AddBotsMenu_SkillSliderEvent;
	addBotsMenuInfo.skillSlider.minvalue			= 0;
	addBotsMenuInfo.skillSlider.maxvalue			= 4;
	addBotsMenuInfo.skillSlider.curvalue			= addBotsMenuInfo.skill.curvalue;

	y += SMALLCHAR_HEIGHT;
	addBotsMenuInfo.team.generic.type		= MTYPE_SPINCONTROL;
	addBotsMenuInfo.team.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	addBotsMenuInfo.team.generic.x			= 320;
	addBotsMenuInfo.team.generic.y			= y;
	addBotsMenuInfo.team.generic.name		= "Team: ";
	addBotsMenuInfo.team.generic.id			= ID_TEAM;
	addBotsMenuInfo.team.generic.callback	= UI_AddBotsMenu_TeamEvent;
	if( gametype >= GT_TEAM ) {
		addBotsMenuInfo.team.itemnames		= teamNames2;
	}
	else {
		addBotsMenuInfo.team.itemnames		= teamNames1;
		addBotsMenuInfo.team.generic.flags	= QMF_GRAYED;
	}

	// icon highlight overlay — slightly larger, behind the bot icon
	addBotsMenuInfo.iconHighlight.generic.type		= MTYPE_BITMAP;
	addBotsMenuInfo.iconHighlight.generic.flags		= QMF_LEFT_JUSTIFY|QMF_INACTIVE|QMF_HIGHLIGHT;
	addBotsMenuInfo.iconHighlight.generic.x			= 128 - 4;
	addBotsMenuInfo.iconHighlight.generic.y			= 168 - 4;
	addBotsMenuInfo.iconHighlight.width				= 72;
	addBotsMenuInfo.iconHighlight.height			= 72;
	addBotsMenuInfo.iconHighlight.focuspic			= ART_ICON_HIGHLIGHT;
	addBotsMenuInfo.iconHighlight.focuscolor		= colorRed;

	// bot portrait icon — positioned left of the bot list
	addBotsMenuInfo.icon.generic.type		= MTYPE_BITMAP;
	addBotsMenuInfo.icon.generic.flags		= QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	addBotsMenuInfo.icon.generic.name		= addBotsMenuInfo.boticon;
	addBotsMenuInfo.icon.generic.x			= 128;
	addBotsMenuInfo.icon.generic.y			= 168;
	addBotsMenuInfo.icon.width				= 64;
	addBotsMenuInfo.icon.height				= 64;

	addBotsMenuInfo.go.generic.type			= MTYPE_BITMAP;
	addBotsMenuInfo.go.generic.name			= ART_FIGHT0;
	addBotsMenuInfo.go.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	addBotsMenuInfo.go.generic.id			= ID_GO;
	addBotsMenuInfo.go.generic.callback		= UI_AddBotsMenu_FightEvent;
	addBotsMenuInfo.go.generic.x			= 320+128-128;
	addBotsMenuInfo.go.generic.y			= 256+128-64;
	addBotsMenuInfo.go.width  				= 128;
	addBotsMenuInfo.go.height  				= 64;
	addBotsMenuInfo.go.focuspic				= ART_FIGHT1;

	addBotsMenuInfo.back.generic.type		= MTYPE_BITMAP;
	addBotsMenuInfo.back.generic.name		= ART_BACK0;
	addBotsMenuInfo.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	addBotsMenuInfo.back.generic.id			= ID_BACK;
	addBotsMenuInfo.back.generic.callback	= UI_AddBotsMenu_BackEvent;
	addBotsMenuInfo.back.generic.x			= 320-128;
	addBotsMenuInfo.back.generic.y			= 256+128-64;
	addBotsMenuInfo.back.width				= 128;
	addBotsMenuInfo.back.height				= 64;
	addBotsMenuInfo.back.focuspic			= ART_BACK1;

	addBotsMenuInfo.baseBotNum = 0;
	addBotsMenuInfo.selectedBotNum = 0;
	addBotsMenuInfo.bots[0].color = color_white;

	UI_AddBotsMenu_GetSortedBotNums();
	UI_AddBotsMenu_SetBotNames();
	AddBots_SetBotIcon();

	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.background );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.banner );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.arrows );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.up );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.down );
	for( n = 0; n < count; n++ ) {
		Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.bots[n] );
	}
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.iconHighlight );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.icon );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.skill );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.skillSlider );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.team );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.go );
	Menu_AddItem( &addBotsMenuInfo.menu, &addBotsMenuInfo.back );
}


/*
=================
UI_AddBots_Cache
=================
*/
void UI_AddBots_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
	trap_R_RegisterShaderNoMip( ART_FIGHT0 );
	trap_R_RegisterShaderNoMip( ART_FIGHT1 );
	trap_R_RegisterShaderNoMip( ART_BACKGROUND );
	trap_R_RegisterShaderNoMip( ART_ARROWS );
	trap_R_RegisterShaderNoMip( ART_ARROWUP );
	trap_R_RegisterShaderNoMip( ART_ARROWDOWN );
	trap_R_RegisterShaderNoMip( ART_ICON_HIGHLIGHT );
}


/*
=================
UI_AddBotsMenu
=================
*/
void UI_AddBotsMenu( void ) {
	UI_AddBotsMenu_Init();
	UI_PushMenu( &addBotsMenuInfo.menu );
}
