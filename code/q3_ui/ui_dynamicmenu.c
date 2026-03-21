/*
===========================================================================
ui_dynamicmenu.c -- Dynamic hierarchical menu system (6A / FEAT_DYNAMIC_MENU)

Ported from UIE12. Provides a callback-driven menu where items can be
added at runtime and submenus can be stacked via a depth system.

Usage:
  DynamicMenu_MenuInit( qtrue, qtrue );
  DynamicMenu_AddItem( "Fraglimit", ID_FRAGLIMIT, FragLimit_SubMenu, NULL );
  DynamicMenu_AddItem( "Timelimit", ID_TIMELIMIT, TimeLimit_SubMenu, NULL );
  UI_PushMenu( DynamicMenu_Ref() );
===========================================================================
*/
#include "ui_local.h"

#if FEAT_DYNAMIC_MENU

#define MAX_DYNAMIC_ITEMS   32
#define MAX_DYNAMIC_DEPTH   4
#define DYNAMIC_SPACING     24

typedef struct {
	char			label[64];
	int				id;
	dynamicCreateHandler create;
	dynamicEventHandler  event;
} dynamicItem_t;

typedef struct {
	menuframework_s	menu;
	menutext_s		items[MAX_DYNAMIC_ITEMS];
	menutext_s		banner;
	menutext_s		back;

	dynamicItem_t	data[MAX_DYNAMIC_ITEMS];
	int				numItems;
	int				depth;
	char			bannerText[64];

	// depth stack for submenus
	int				savedNumItems[MAX_DYNAMIC_DEPTH];
} dynamicMenu_t;

static dynamicMenu_t s_dynmenu;

/*
==================
DynamicMenu_Event
==================
*/
static void DynamicMenu_Event( void *ptr, int notification ) {
	int index;
	if ( notification != QM_ACTIVATED ) return;

	index = ((menucommon_s*)ptr)->id;

	if ( index == -1 ) {
		// back button
		if ( s_dynmenu.depth > 0 ) {
			s_dynmenu.depth--;
			s_dynmenu.numItems = s_dynmenu.savedNumItems[s_dynmenu.depth];
		} else {
			UI_PopMenu();
		}
		return;
	}

	if ( index >= 0 && index < s_dynmenu.numItems ) {
		// if item has a create handler, it opens a submenu
		if ( s_dynmenu.data[index].create ) {
			s_dynmenu.savedNumItems[s_dynmenu.depth] = s_dynmenu.numItems;
			s_dynmenu.depth++;
			s_dynmenu.numItems = 0;
			s_dynmenu.data[index].create();
			return;
		}
		// if item has an event handler, call it
		if ( s_dynmenu.data[index].event ) {
			s_dynmenu.data[index].event( s_dynmenu.data[index].id );
		}
	}
}

/*
==================
DynamicMenu_Draw
==================
*/
static void DynamicMenu_Draw( void ) {
	int i, y;

	// draw banner
	UI_DrawProportionalString( 320, 88,
		s_dynmenu.bannerText[0] ? s_dynmenu.bannerText : "MENU",
		UI_CENTER | UI_SMALLFONT, color_white );

	// draw items
	y = 120;
	for ( i = 0; i < s_dynmenu.numItems; i++ ) {
		vec4_t *color = ( s_dynmenu.menu.cursor == i ) ? &color_yellow : &color_red;
		UI_DrawProportionalString( 320, y, s_dynmenu.data[i].label,
			UI_CENTER | UI_SMALLFONT, *color );
		y += DYNAMIC_SPACING;
	}

	// draw back
	y += 8;
	{
		vec4_t *color = ( s_dynmenu.menu.cursor == s_dynmenu.numItems ) ? &color_yellow : &color_red;
		UI_DrawProportionalString( 320, y, "BACK", UI_CENTER | UI_SMALLFONT, *color );
	}
}

/*
==================
DynamicMenu_Key
==================
*/
static sfxHandle_t DynamicMenu_Key( int key ) {
	int total = s_dynmenu.numItems + 1; // items + back

	switch ( key ) {
	case K_MWHEELUP:
	case K_UPARROW:
	case K_KP_UPARROW:
		s_dynmenu.menu.cursor--;
		if ( s_dynmenu.menu.cursor < 0 ) s_dynmenu.menu.cursor = total - 1;
		return menu_move_sound;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		s_dynmenu.menu.cursor++;
		if ( s_dynmenu.menu.cursor >= total ) s_dynmenu.menu.cursor = 0;
		return menu_move_sound;

	case K_ENTER:
	case K_KP_ENTER:
	case K_MOUSE1:
		{
			menucommon_s fake;
			if ( s_dynmenu.menu.cursor == s_dynmenu.numItems ) {
				fake.id = -1;
			} else {
				fake.id = s_dynmenu.menu.cursor;
			}
			DynamicMenu_Event( &fake, QM_ACTIVATED );
		}
		return menu_in_sound;

	case K_ESCAPE:
		if ( s_dynmenu.depth > 0 ) {
			s_dynmenu.depth--;
			s_dynmenu.numItems = s_dynmenu.savedNumItems[s_dynmenu.depth];
			return menu_out_sound;
		}
		UI_PopMenu();
		return menu_out_sound;
	}
	return 0;
}

/*
==================
DynamicMenu_MenuInit
==================
*/
void DynamicMenu_MenuInit( qboolean fullscreen, qboolean wraparound ) {
	memset( &s_dynmenu, 0, sizeof( dynamicMenu_t ) );
	s_dynmenu.menu.fullscreen = fullscreen;
	s_dynmenu.menu.wrapAround = wraparound;
	s_dynmenu.menu.draw = DynamicMenu_Draw;
	s_dynmenu.menu.key = DynamicMenu_Key;
	s_dynmenu.depth = 0;
}

/*
==================
DynamicMenu_AddItem
==================
*/
qboolean DynamicMenu_AddItem( const char *label, int id, dynamicCreateHandler create, dynamicEventHandler event ) {
	dynamicItem_t *item;

	if ( s_dynmenu.numItems >= MAX_DYNAMIC_ITEMS ) {
		return qfalse;
	}

	item = &s_dynmenu.data[s_dynmenu.numItems];
	Q_strncpyz( item->label, label, sizeof( item->label ) );
	item->id = id;
	item->create = create;
	item->event = event;
	s_dynmenu.numItems++;
	return qtrue;
}

/*
==================
DynamicMenu_Ref

Returns a pointer to the menu framework for UI_PushMenu.
==================
*/
menuframework_s *DynamicMenu_Ref( void ) {
	return &s_dynmenu.menu;
}

void DynamicMenu_SetBanner( const char *text ) {
	if ( text ) {
		Q_strncpyz( s_dynmenu.bannerText, text, sizeof( s_dynmenu.bannerText ) );
	} else {
		s_dynmenu.bannerText[0] = '\0';
	}
}

#endif // FEAT_DYNAMIC_MENU
