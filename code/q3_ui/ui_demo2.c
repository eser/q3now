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

DEMOS MENU

=======================================================================
*/


#include "ui_local.h"


#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"	
#define ART_GO0				"menu/art/play_0"
#define ART_GO1				"menu/art/play_1"
#define ART_FRAMEL			"menu/art/frame2_l"
#define ART_FRAMER			"menu/art/frame1_r"
#define ART_ARROWS			"menu/art/arrows_horz_0"
#define ART_ARROWLEFT		"menu/art/arrows_horz_left"
#define ART_ARROWRIGHT		"menu/art/arrows_horz_right"

#define MAX_DEMOS			1024
#define NAMEBUFSIZE			(MAX_DEMOS * 32)

#define ID_BACK				10
#define ID_GO				11
#define ID_LIST				12
#define ID_RIGHT			13
#define ID_LEFT				14

#define ARROWS_WIDTH		128
#define ARROWS_HEIGHT		48


typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menutable_s		table;

	menubitmap_s	arrows;
	menubitmap_s	left;
	menubitmap_s	right;
	menubitmap_s	back;
	menubitmap_s	go;

	int				numDemos;
	char			names[NAMEBUFSIZE];

	char			*demolist[MAX_DEMOS];
} demos_t;

static demos_t	s_demos;


/*
===============
DemoTable_GetColumnText
===============
*/
static const char *DemoTable_GetColumnText( int row, int column )
{
	if ( row < 0 || row >= s_demos.numDemos )
		return "";
	if ( column == 0 )
		return s_demos.demolist[row];
	return "";
}


/*
===============
DemoTable_OnSelect
===============
*/
static void DemoTable_OnSelect( int row )
{
	/* nothing to do */
}


/*
===============
Demos_MenuEvent
===============
*/
static void Demos_MenuEvent( void *ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_GO:
		UI_ForceMenuOff ();
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo %s\n",
								s_demos.demolist[s_demos.table.selectedRow]) );
		break;

	case ID_BACK:
		UI_PopMenu();
		break;

	case ID_LEFT:
		MenuTable_Key( &s_demos.table, K_UPARROW );
		break;

	case ID_RIGHT:
		MenuTable_Key( &s_demos.table, K_DOWNARROW );
		break;
	}
}


/*
===============
Demos_MenuInit
===============
*/
static void Demos_MenuInit( void ) {
	int		i, j;
	int		len;
	char	*demoname, extension[32];
	int	protocol, protocolLegacy;

	memset( &s_demos, 0 ,sizeof(demos_t) );

	Demos_Cache();

	s_demos.menu.fullscreen = qtrue;
	s_demos.menu.wrapAround = qtrue;

	s_demos.banner.generic.type		= MTYPE_BTEXT;
	s_demos.banner.generic.x		= 320;
	s_demos.banner.generic.y		= 16;
	s_demos.banner.string			= "DEMOS";
	s_demos.banner.color			= color_white;
	s_demos.banner.style			= UI_CENTER;

	s_demos.framel.generic.type		= MTYPE_BITMAP;
	s_demos.framel.generic.name		= ART_FRAMEL;
	s_demos.framel.generic.flags	= QMF_INACTIVE;
	s_demos.framel.generic.x		= 0;  
	s_demos.framel.generic.y		= 78;
	s_demos.framel.width			= 256;
	s_demos.framel.height			= 329;

	s_demos.framer.generic.type		= MTYPE_BITMAP;
	s_demos.framer.generic.name		= ART_FRAMER;
	s_demos.framer.generic.flags	= QMF_INACTIVE;
	s_demos.framer.generic.x		= 376;
	s_demos.framer.generic.y		= 76;
	s_demos.framer.width			= 256;
	s_demos.framer.height			= 334;

	s_demos.arrows.generic.type		= MTYPE_BITMAP;
	s_demos.arrows.generic.name		= ART_ARROWS;
	s_demos.arrows.generic.flags	= QMF_INACTIVE;
	s_demos.arrows.generic.x		= 320-ARROWS_WIDTH/2;
	s_demos.arrows.generic.y		= 400;
	s_demos.arrows.width			= ARROWS_WIDTH;
	s_demos.arrows.height			= ARROWS_HEIGHT;

	s_demos.left.generic.type		= MTYPE_BITMAP;
	s_demos.left.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_MOUSEONLY;
	s_demos.left.generic.x			= 320-ARROWS_WIDTH/2;
	s_demos.left.generic.y			= 400;
	s_demos.left.generic.id			= ID_LEFT;
	s_demos.left.generic.callback	= Demos_MenuEvent;
	s_demos.left.width				= ARROWS_WIDTH/2;
	s_demos.left.height				= ARROWS_HEIGHT;
	s_demos.left.focuspic			= ART_ARROWLEFT;

	s_demos.right.generic.type		= MTYPE_BITMAP;
	s_demos.right.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_MOUSEONLY;
	s_demos.right.generic.x			= 320;
	s_demos.right.generic.y			= 400;
	s_demos.right.generic.id		= ID_RIGHT;
	s_demos.right.generic.callback	= Demos_MenuEvent;
	s_demos.right.width				= ARROWS_WIDTH/2;
	s_demos.right.height			= ARROWS_HEIGHT;
	s_demos.right.focuspic			= ART_ARROWRIGHT;

	s_demos.back.generic.type		= MTYPE_BITMAP;
	s_demos.back.generic.name		= ART_BACK0;
	s_demos.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.back.generic.id			= ID_BACK;
	s_demos.back.generic.callback	= Demos_MenuEvent;
	s_demos.back.generic.x			= 0;
	s_demos.back.generic.y			= 480-64;
	s_demos.back.width				= 128;
	s_demos.back.height				= 64;
	s_demos.back.focuspic			= ART_BACK1;

	s_demos.go.generic.type			= MTYPE_BITMAP;
	s_demos.go.generic.name			= ART_GO0;
	s_demos.go.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_demos.go.generic.id			= ID_GO;
	s_demos.go.generic.callback		= Demos_MenuEvent;
	s_demos.go.generic.x			= 640;
	s_demos.go.generic.y			= 480-64;
	s_demos.go.width				= 128;
	s_demos.go.height				= 64;
	s_demos.go.focuspic				= ART_GO1;

	s_demos.table.generic.type			= MTYPE_TABLE;
	s_demos.table.generic.flags			= QMF_PULSEIFFOCUS;
	s_demos.table.generic.callback		= Demos_MenuEvent;
	s_demos.table.generic.id			= ID_LIST;
	s_demos.table.generic.x			= 118;
	s_demos.table.generic.y			= 130;
	s_demos.table.numColumns			= 1;
	s_demos.table.columns[0].header		= "Demo Name";
	s_demos.table.columns[0].width		= 40;
	s_demos.table.columns[0].align		= UI_LEFT;
	s_demos.table.visibleRows			= 14;
	s_demos.table.selectedRow			= 0;
	s_demos.table.emptyText				= "No demos found";
	s_demos.table.getColumnText			= DemoTable_GetColumnText;
	s_demos.table.onSelect				= DemoTable_OnSelect;

	protocolLegacy = trap_Cvar_VariableValue("com_legacyprotocol");
	protocol = trap_Cvar_VariableValue("com_protocol");

	if(!protocol)
		protocol = trap_Cvar_VariableValue("protocol");
	if(protocolLegacy == protocol)
		protocolLegacy = 0;

	Com_sprintf(extension, sizeof(extension), ".%s%d", DEMOEXT, protocol);
	s_demos.numDemos = trap_FS_GetFileList("demos", extension, s_demos.names, ARRAY_LEN(s_demos.names));

	demoname = s_demos.names;
	i = 0;

	for(j = 0; j < 2; j++)
	{
		if(s_demos.numDemos > MAX_DEMOS)
			s_demos.numDemos = MAX_DEMOS;

		for(; i < s_demos.numDemos; i++)
		{
			s_demos.demolist[i] = demoname;
		
			len = strlen(demoname);

			demoname += len + 1;
		}

		if(!j)
		{
			if(protocolLegacy > 0 && s_demos.numDemos < MAX_DEMOS)
			{
				Com_sprintf(extension, sizeof(extension), ".%s%d", DEMOEXT, protocolLegacy);
				s_demos.numDemos += trap_FS_GetFileList("demos", extension, demoname,
									ARRAY_LEN(s_demos.names) - (demoname - s_demos.names));
			}
			else
				break;
		}
	}

	s_demos.table.numRows = s_demos.numDemos;

	if(!s_demos.numDemos)
	{
		//degenerate case, not selectable
		s_demos.go.generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
	}

	Menu_AddItem( &s_demos.menu, &s_demos.banner );
	Menu_AddItem( &s_demos.menu, &s_demos.framel );
	Menu_AddItem( &s_demos.menu, &s_demos.framer );
	Menu_AddItem( &s_demos.menu, &s_demos.table );
	Menu_AddItem( &s_demos.menu, &s_demos.arrows );
	Menu_AddItem( &s_demos.menu, &s_demos.left );
	Menu_AddItem( &s_demos.menu, &s_demos.right );
	Menu_AddItem( &s_demos.menu, &s_demos.back );
	Menu_AddItem( &s_demos.menu, &s_demos.go );
}

/*
=================
Demos_Cache
=================
*/
void Demos_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
	trap_R_RegisterShaderNoMip( ART_GO0 );
	trap_R_RegisterShaderNoMip( ART_GO1 );
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_ARROWS );
	trap_R_RegisterShaderNoMip( ART_ARROWLEFT );
	trap_R_RegisterShaderNoMip( ART_ARROWRIGHT );
}

/*
===============
UI_DemosMenu
===============
*/
void UI_DemosMenu( void ) {
	Demos_MenuInit();
	UI_PushMenu( &s_demos.menu );
}
