// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/* Browser-only probes and input adapters for the canonical native WiredUI
 * parser/compositor. Authored layout remains owned by client/wired/ui. */

#include "web_ui_compat.h"
#include "web_authored_content.h"
#include "../client/wired/hud/cl_wired_crosshair.h"
#include "../client/wired/store/cl_wired_store.h"

#include <string.h>

static float s_pointerX;
static float s_pointerY;
static qboolean s_pointerValid;
static qboolean s_loadingFixture;
static qboolean s_serverScrolled;
static qboolean s_serverClicked;

static wiredItemDef_t *WiredWebUi_FindItem( wiredItemDef_t *item, const char *name ) {
	int index;
	if ( !item || !name ) return NULL;
	if ( item->name[0] && !strcmp( item->name, name ) ) return item;
	for ( index = 0; index < item->childCount; ++index ) {
		wiredItemDef_t *found = WiredWebUi_FindItem( item->children[index], name );
		if ( found ) return found;
	}
	return NULL;
}

void WiredWebUi_ResetBridge( void ) {
	s_pointerX = 0.0f;
	s_pointerY = 0.0f;
	s_pointerValid = qfalse;
	s_loadingFixture = qfalse;
	s_serverScrolled = qfalse;
	s_serverClicked = qfalse;
}

static uint32_t WiredWebUi_NameReceipt( const char *name ) {
	uint32_t hash = 2166136261u;
	const unsigned char *cursor = (const unsigned char *)( name ? name : "" );
	while ( *cursor ) hash = ( hash ^ *cursor++ ) * 16777619u;
	return hash & 0x7fffu;
}

void WiredWebUi_ReceiveHudState( const wiredHudState_t *state ) { (void)state; }
void WiredWebUi_ReceiveStoreBatch( const wuiStagedEntry_t *entries, int count ) {
	(void)entries; (void)count;
}

uint32_t WiredWebUi_TextReceipt( void ) {
	uint32_t flags = 0u;
	if ( MSDF_GetFontCount() >= 3 ) flags |= 0x1u;
	if ( MSDF_GetRenderableFontCount() >= 3 ) flags |= 0x2u;
	if ( Text_Measure( "WIRED", FONT_DISPLAY_BOLD, 32.0f ) > 0.0f ) flags |= 0x4u;
	if ( FS_ReadFile( "fonts/sansman-regular.json", NULL ) > 0 ) flags |= 0x10u;
	return flags;
}

uint32_t WiredWebUi_MenuSemanticReceipt( void ) {
	const wiredItemDef_t *focused = WiredUI_GetFocusedItem();
	return focused && focused->name[0]
		? 0x8000u | WiredWebUi_NameReceipt( focused->name ) : 0u;
}

int WiredWebUi_MenuActive( void ) {
	return WiredUI_GetActiveMenu() ? 1 : 0;
}

uint32_t WiredWebUi_HudReceipt( void ) {
	cgCrosshairDrawSpec_t crosshair;
	uint32_t receipt = WiredStore_Get( "player.health.value" ) ? 0x3u : 0u;
	if ( WiredCrosshair_Eval( &crosshair ) && crosshair.visible ) receipt |= 0x4u;
	return receipt;
}

uint32_t WiredWebUi_HudExtent( void ) {
	if ( !WiredStore_Get( "player.health.value" ) ) return 0u;
	return ( (uint32_t)cls.glconfig.vidWidth << 16 )
		| ( (uint32_t)cls.glconfig.vidHeight & 0xffffu );
}

void WiredWebUi_EnableServerFixture( int count ) {
	if ( count <= 0 ) {
		WiredFeeder_ServerFixtureClear();
		return;
	}
	s_serverScrolled = qfalse;
	s_serverClicked = qfalse;
	WiredFeeder_ServerFixtureInstall( 27961, 27960, qfalse );
}

uint32_t WiredWebUi_ServerReceipt( void ) {
	const char *top = WiredUI_GetMenuStackTop();
	uint32_t receipt = 0u;
	if ( top && !strcmp( top, "servers" ) ) {
		receipt |= 0x1u;
		if ( WiredWebAuthored_Receipt() & WIRED_WEB_AUTHORED_RECEIPT_MENU_RENDERED ) receipt |= 0x2u;
	}
	if ( s_serverScrolled ) receipt |= 0x4u;
	if ( s_serverClicked ) receipt |= 0x8u;
	if ( WiredFeeder_ServerFixtureActive() ) receipt |= 0x10u;
	return receipt;
}

uint32_t WiredWebUi_ServerRowCenter( void ) {
	wiredMenuDef_t *menu = WiredUI_GetActiveMenu();
	wiredItemDef_t *listbox = NULL;
	wiredItemDef_t *frame = NULL;
	wuiPixelRect_t rect;
	float dpi, charSize, rowHeight, rowFloor, headerHeight, x, y;
	int index;
	if ( !menu ) return 0u;
	for ( index = 0; index < menu->itemCount && ( !listbox || !frame ); ++index ) {
		if ( !listbox ) listbox = WiredWebUi_FindItem( menu->items[index], "serverlist" );
		if ( !frame ) frame = WiredWebUi_FindItem( menu->items[index], "serverlist_frame" );
	}
	if ( !listbox || !frame ) {
		menu = WiredUI_FindMenu( "servers" );
		listbox = frame = NULL;
		if ( !menu ) return 0u;
		for ( index = 0; index < menu->itemCount && ( !listbox || !frame ); ++index ) {
			if ( !listbox ) listbox = WiredWebUi_FindItem( menu->items[index], "serverlist" );
			if ( !frame ) frame = WiredWebUi_FindItem( menu->items[index], "serverlist_frame" );
		}
	}
	if ( !listbox || !frame ) return 0u;
	if ( listbox->type != ITEM_TYPE_LISTBOX ) return 0u;
	if ( WiredUI_FeederCount( (int)listbox->feeder ) <= 0 )
		return 0u;
	/* The listbox is a floating Clay child and may intentionally own no
	 * persistent parent-layout element. resolvedRect is synchronized from the
	 * completed Clay frame; its grow-1 frame is the occupied fallback. */
	if ( !WiredUI_ClayItemRenderedRect( menu, listbox, &rect ) ) {
		rect = listbox->resolvedRect;
		if ( rect.w <= 0.0f || rect.h <= 0.0f ) rect = frame->resolvedRect;
	}
	if ( rect.w <= 0.0f || rect.h <= 0.0f )
		return 0u;
	dpi = WiredUI_GetDpiScale();
	charSize = listbox->fontPointSize > 0.0f
		? listbox->fontPointSize : WUI_DEFAULT_FONT_SIZE;
	rowFloor = charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
	rowHeight = listbox->elementheight > 0.0f
		? listbox->elementheight * dpi : rowFloor;
	if ( rowHeight < rowFloor ) rowHeight = rowFloor;
	headerHeight = WiredUI_ListboxHeaderHeight( listbox );
	if ( headerHeight + rowHeight > rect.h ) return 0u;
	x = rect.x + rect.w * 0.5f;
	y = rect.y + headerHeight + rowHeight * 0.5f;
	if ( x < 0.0f || y < 0.0f || x > 65535.0f || y > 65535.0f ) return 0u;
	return ( (uint32_t)( x + 0.5f ) << 16 ) | (uint32_t)( y + 0.5f );
}

void WiredWebUi_ActivateLayerFixture( int kind ) {
	s_loadingFixture = kind == 1 ? qtrue : qfalse;
	WiredUI_SetLoadingMenu( s_loadingFixture ? "ui/loading_screen.wui" : NULL );
}

uint32_t WiredWebUi_LayerReceipt( void ) {
	uint32_t receipt = 0u;
	if ( WiredUI_GetLoadingMenuPath()[0] ) receipt |= 0x3u;
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) receipt |= 0xcu;
	if ( s_loadingFixture ) receipt |= 0x10u;
	return receipt;
}

void WiredWebUi_Pointer( float x, float y, int down ) {
	const char *top = WiredUI_GetMenuStackTop();
	char selected[256];
	if ( !s_pointerValid ) {
		float nx = 0.0f, ny = 0.0f;
		WiredUI_GetCursorNorm( &nx, &ny );
		s_pointerX = ( nx + 1.0f ) * 0.5f * (float)cls.glconfig.vidWidth;
		s_pointerY = ( ny + 1.0f ) * 0.5f * (float)cls.glconfig.vidHeight;
	}
	WiredUI_MouseEvent( x - s_pointerX, y - s_pointerY );
	s_pointerX = x; s_pointerY = y; s_pointerValid = qtrue;
	WiredUI_KeyEvent( K_MOUSE1, down ? qtrue : qfalse );
	if ( !down && top && !strcmp( top, "servers" )
		&& WiredFeeder_GetSelectedServerAddress( selected, sizeof( selected ) ) )
		s_serverClicked = qtrue;
}

void WiredWebUi_Wheel( float deltaY ) {
	int key;
	if ( deltaY == 0.0f ) return;
	key = deltaY > 0.0f ? K_MWHEELDOWN : K_MWHEELUP;
	WiredUI_KeyEvent( key, qtrue );
	WiredUI_KeyEvent( key, qfalse );
	if ( !strcmp( WiredUI_GetMenuStackTop(), "servers" ) ) s_serverScrolled = qtrue;
}

int WiredWebUi_RootCount( void ) {
	const wiredWebAuthoredCatalog_t *catalog = WiredWebAuthored_Catalog();
	return catalog ? catalog->rootCount : 0;
}

int WiredWebUi_ActivateRoot( int index ) {
	const wiredWebAuthoredCatalog_t *catalog = WiredWebAuthored_Catalog();
	const wiredWebAuthoredRoot_t *root;
	while ( WiredUI_HasActivePopup() ) WiredUI_DismissPopup();
	WiredUI_CloseAllMenus();
	if ( index == -1 ) {
		WiredUI_SetActiveMenu( UIMENU_NONE );
		return 1;
	}
	if ( !catalog || index < 0 || index >= catalog->rootCount ) return 0;
	root = &catalog->roots[index];
	if ( root->layer == WIRED_WEB_AUTHORED_ROOT_MENU ) {
		WiredUI_PushMenu( root->menuName );
	} else if ( root->layer == WIRED_WEB_AUTHORED_ROOT_POPUP ) {
		WiredUI_PushMenu( "main" );
		WiredUI_PushPopup( root->menuName );
	} else {
		return 0;
	}
	return 1;
}

uint32_t WiredWebUi_RootReceipt( int index ) {
	const wiredWebAuthoredCatalog_t *catalog = WiredWebAuthored_Catalog();
	const wiredWebAuthoredRoot_t *root;
	const char *active;
	uint32_t receipt = 0u;
	if ( !catalog || index < 0 || index >= catalog->rootCount ) return 0u;
	root = &catalog->roots[index];
	active = root->layer == WIRED_WEB_AUTHORED_ROOT_POPUP
		? WiredUI_FrontPopupName() : WiredUI_GetMenuStackTop();
	if ( active && !strcmp( active, root->menuName ) ) receipt |= 0x1u;
	if ( WiredUI_IsHealthy() ) receipt |= 0x2u;
	if ( WiredWebAuthored_Receipt() & WIRED_WEB_AUTHORED_RECEIPT_MENU_RENDERED ) receipt |= 0x4u;
	return receipt | ( (uint32_t)root->layer << 8 ) | ( (uint32_t)( index + 1 ) << 16 );
}

uint32_t WiredWebClay_Receipt( void ) {
	return WiredUI_IsHealthy() ? 1u : 0u;
}

uint32_t WiredWebClay_ServerLayoutP99Micros( void ) { return 0u; }
