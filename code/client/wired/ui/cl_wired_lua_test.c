// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* Lua-authored automation over the real WiredUI authority. Lua queues intent;
 * execution stays in the native input, WiredStore and Clay paths. */

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_compositor.h"
#include "cl_wired_lua_test.h"
#include "../store/cl_wired_store.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"

#if FEAT_WIRED_UI

#include <lua.h>
#include <lauxlib.h>

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#define WUI_LUA_TEST_MAX_ACTIONS 128

typedef enum {
	WUI_TEST_OPEN,
	WUI_TEST_WAIT,
	WUI_TEST_KEY,
	WUI_TEST_STORE,
	WUI_TEST_ASSERT_STORE,
	WUI_TEST_ASSERT_FOCUS,
	WUI_TEST_ASSERT_RECT,
	WUI_TEST_SCREENSHOT
} wuiTestActionKind_t;

typedef struct {
	wuiTestActionKind_t kind;
	char a[128];
	char b[256];
	int value;
} wuiTestAction_t;

static struct {
	wuiTestAction_t actions[WUI_LUA_TEST_MAX_ACTIONS];
	int count;
	int cursor;
	int waitFrames;
	qboolean active;
	qboolean registered;
	char script[MAX_QPATH];
} wui_test;

static int WUITest_Queue( lua_State *L, wuiTestActionKind_t kind ) {
	wuiTestAction_t *action;
	if ( wui_test.count >= WUI_LUA_TEST_MAX_ACTIONS ) {
		return luaL_error( L, "ui_test action capacity exceeded (%d)", WUI_LUA_TEST_MAX_ACTIONS );
	}
	action = &wui_test.actions[wui_test.count++];
	memset( action, 0, sizeof( *action ) );
	action->kind = kind;
	return 0;
}

static int WUITest_OpenLua( lua_State *L ) {
	int result = WUITest_Queue( L, WUI_TEST_OPEN );
	if ( result ) return result;
	Q_strncpyz( wui_test.actions[wui_test.count - 1].a,
		luaL_checkstring( L, 1 ), sizeof( wui_test.actions[0].a ) );
	return 0;
}

static int WUITest_WaitLua( lua_State *L ) {
	int result = WUITest_Queue( L, WUI_TEST_WAIT );
	if ( result ) return result;
	wui_test.actions[wui_test.count - 1].value = (int)luaL_checkinteger( L, 1 );
	return 0;
}

static int WUITest_KeyLua( lua_State *L ) {
	int result = WUITest_Queue( L, WUI_TEST_KEY );
	if ( result ) return result;
	wui_test.actions[wui_test.count - 1].value = (int)luaL_checkinteger( L, 1 );
	return 0;
}

static int WUITest_TwoStringsLua( lua_State *L, wuiTestActionKind_t kind ) {
	wuiTestAction_t *action;
	int result = WUITest_Queue( L, kind );
	if ( result ) return result;
	action = &wui_test.actions[wui_test.count - 1];
	Q_strncpyz( action->a, luaL_checkstring( L, 1 ), sizeof( action->a ) );
	Q_strncpyz( action->b, luaL_checkstring( L, 2 ), sizeof( action->b ) );
	return 0;
}

static int WUITest_StoreLua( lua_State *L ) { return WUITest_TwoStringsLua( L, WUI_TEST_STORE ); }
static int WUITest_AssertStoreLua( lua_State *L ) { return WUITest_TwoStringsLua( L, WUI_TEST_ASSERT_STORE ); }
static int WUITest_AssertRectLua( lua_State *L ) { return WUITest_TwoStringsLua( L, WUI_TEST_ASSERT_RECT ); }

static int WUITest_AssertFocusLua( lua_State *L ) {
	int result = WUITest_Queue( L, WUI_TEST_ASSERT_FOCUS );
	if ( result ) return result;
	Q_strncpyz( wui_test.actions[wui_test.count - 1].a,
		luaL_checkstring( L, 1 ), sizeof( wui_test.actions[0].a ) );
	return 0;
}

static int WUITest_ScreenshotLua( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	int result;
	for ( const char *p = name; *p; p++ ) {
		if ( !( *p == '_' || *p == '-' || ( *p >= '0' && *p <= '9' )
		     || ( *p >= 'A' && *p <= 'Z' ) || ( *p >= 'a' && *p <= 'z' ) ) ) {
			return luaL_error( L, "ui_test screenshot name must be alnum/_/-" );
		}
	}
	result = WUITest_Queue( L, WUI_TEST_SCREENSHOT );
	if ( result ) return result;
	Q_strncpyz( wui_test.actions[wui_test.count - 1].a, name,
		sizeof( wui_test.actions[0].a ) );
	return 0;
}

static const luaL_Reg wuiTestLib[] = {
	{ "open", WUITest_OpenLua }, { "wait", WUITest_WaitLua },
	{ "key", WUITest_KeyLua }, { "store", WUITest_StoreLua },
	{ "assert_store", WUITest_AssertStoreLua },
	{ "assert_focus", WUITest_AssertFocusLua },
	{ "assert_rect", WUITest_AssertRectLua },
	{ "screenshot", WUITest_ScreenshotLua }, { NULL, NULL }
};

static void WUITest_RegisterLua( lua_State *L ) {
	luaL_newlib( L, wuiTestLib );
	lua_setglobal( L, "ui_test" );
}

void WiredUITest_LuaInit( void ) { WiredScript_RegisterBindings( WUITest_RegisterLua ); }

static const wiredItemDef_t *WUITest_FindInItem( const wiredItemDef_t *item, const char *name ) {
	const wiredItemDef_t *found;
	if ( !item ) return NULL;
	if ( !Q_stricmp( item->name, name ) ) return item;
	for ( int i = 0; i < item->childCount; i++ ) {
		found = WUITest_FindInItem( item->children[i], name );
		if ( found ) return found;
	}
	if ( item->repeatBlock && item->repeatBlock->templateItem ) {
		found = WUITest_FindInItem( item->repeatBlock->templateItem, name );
		if ( found ) return found;
	}
	if ( item->ifBlock ) {
		for ( int i = 0; i < item->ifBlock->childCount; i++ ) {
			found = WUITest_FindInItem( item->ifBlock->children[i], name );
			if ( found ) return found;
		}
	}
	return NULL;
}

static const wiredItemDef_t *WUITest_FindItem( const wiredMenuDef_t *menu, const char *name ) {
	const wiredItemDef_t *found;
	if ( !menu ) return NULL;
	for ( int i = 0; i < menu->itemCount; i++ ) {
		found = WUITest_FindInItem( menu->items[i], name );
		if ( found ) return found;
	}
	return NULL;
}

static void WUITest_Fail( const char *reason, const char *a, const char *b ) {
	Com_Log( SEV_ERROR, LOG_CH(ch_ui),
		"WiredUI Lua harness: FAIL script=%s action=%d reason=%s a=%s b=%s\n",
		wui_test.script, wui_test.cursor, reason, a ? a : "", b ? b : "" );
	wui_test.active = qfalse;
}

static void WUITest_Run_f( void ) {
	if ( !com_automated || !com_automated->integer ) return;
	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "usage: wui_test_run <script-qpath>\n" );
		return;
	}
	memset( wui_test.actions, 0, sizeof( wui_test.actions ) );
	wui_test.count = wui_test.cursor = wui_test.waitFrames = 0;
	wui_test.active = qfalse;
	Q_strncpyz( wui_test.script, Cmd_Argv( 1 ), sizeof( wui_test.script ) );
	if ( !WiredScript_TryExecFile( wui_test.script ) || wui_test.count == 0 ) {
		WUITest_Fail( "script-load-or-empty", wui_test.script, "" );
		return;
	}
	wui_test.active = qtrue;
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI Lua harness: RUN script=%s actions=%d\n",
		wui_test.script, wui_test.count );
}

void WiredUITest_Init( void ) {
	if ( wui_test.registered ) return;
	Cmd_AddCommand( "wui_test_run", WUITest_Run_f );
	wui_test.registered = qtrue;
}

void WiredUITest_Shutdown( void ) {
	if ( wui_test.registered ) Cmd_RemoveCommand( "wui_test_run" );
	memset( &wui_test, 0, sizeof( wui_test ) );
}

void WiredUITest_Tick( void ) {
	int budget = 32;
	if ( !wui_test.active ) return;
	if ( wui_test.waitFrames > 0 ) { wui_test.waitFrames--; return; }
	while ( wui_test.active && budget-- > 0 ) {
		wuiTestAction_t *action;
		if ( wui_test.cursor >= wui_test.count ) {
			Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI Lua harness: PASS script=%s actions=%d\n",
				wui_test.script, wui_test.count );
			wui_test.active = qfalse;
			return;
		}
		action = &wui_test.actions[wui_test.cursor++];
		switch ( action->kind ) {
		case WUI_TEST_OPEN:
			if ( !WiredUI_FindMenu( action->a ) ) { WUITest_Fail( "menu-not-found", action->a, "" ); return; }
			WiredUI_PushMenu( action->a );
			break;
		case WUI_TEST_WAIT:
			wui_test.waitFrames = MAX( 0, action->value ); return;
		case WUI_TEST_KEY:
			CL_KeyEvent( action->value, qtrue, cls.realtime );
			CL_KeyEvent( action->value, qfalse, cls.realtime );
			break;
		case WUI_TEST_STORE: {
			wuiStoreEntry_t *entry = WiredStore_Set( action->a );
			if ( !entry ) { WUITest_Fail( "store-full", action->a, "" ); return; }
			Q_strncpyz( entry->text, action->b, sizeof( entry->text ) );
			entry->flags |= WUI_STORE_FLAG_DIRTY;
			break;
		}
		case WUI_TEST_ASSERT_STORE: {
			const wuiStoreEntry_t *entry = WiredStore_Get( action->a );
			if ( !entry || Q_stricmp( entry->text, action->b ) ) {
				WUITest_Fail( "store-mismatch", action->a, action->b ); return;
			}
			break;
		}
		case WUI_TEST_ASSERT_FOCUS: {
			const wiredItemDef_t *focused = WiredUI_GetFocusedItem();
			if ( !focused || Q_stricmp( focused->name, action->a ) ) {
				WUITest_Fail( "focus-mismatch", action->a, focused ? focused->name : "<none>" ); return;
			}
			break;
		}
		case WUI_TEST_ASSERT_RECT: {
			const wiredMenuDef_t *menu = WiredUI_FindMenu( action->a );
			const wiredItemDef_t *item = WUITest_FindItem( menu, action->b );
			wuiPixelRect_t rect;
			if ( !item || !WiredUI_ClayItemRenderedRect( menu, item, &rect ) || rect.w <= 0.0f || rect.h <= 0.0f ) {
				WUITest_Fail( "missing-live-clay-rect", action->a, action->b ); return;
			}
			break;
		}
		case WUI_TEST_SCREENSHOT:
			/* Backend capture is consumed at end-of-frame. Insert a wait with
			 * the command so a following quit can never tear the renderer down
			 * in the same command-buffer pass. */
			Cbuf_InsertText( va( "screenshot %s png silent\nwait 5\n", action->a ) );
			wui_test.waitFrames = 2; return;
		}
	}
}

#else
void WiredUITest_LuaInit( void ) {}
void WiredUITest_Init( void ) {}
void WiredUITest_Shutdown( void ) {}
void WiredUITest_Tick( void ) {}
#endif
