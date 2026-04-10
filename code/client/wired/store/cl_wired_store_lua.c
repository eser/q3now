#include "../../../qcommon/q_shared.h"
#include "../../../qcommon/qcommon.h"
#include "../../../qcommon/scripting/wired_scripting.h"
#include "cl_wired_store.h"
#include "../ui/cl_wired_ui.h"

#if FEAT_WIRED_UI

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static int WiredStoreLua_Get( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e && e->text[0] ) {
		lua_pushstring( L, e->text );
	} else {
		lua_pushnil( L );
	}
	return 1;
}

static int WiredStoreLua_Set( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Set( key );
	if ( !e ) {
		return luaL_error( L, "store full, cannot set '%s'", key );
	}
	if ( lua_type( L, 2 ) == LUA_TNUMBER ) {
		float val = (float)lua_tonumber( L, 2 );
		char buf[64];
		e->value = val;
		Com_sprintf( buf, sizeof( buf ), "%g", (double)val );
		Q_strncpyz( e->text, buf, sizeof( e->text ) );
	} else {
		const char *val = luaL_checkstring( L, 2 );
		Q_strncpyz( e->text, val, sizeof( e->text ) );
	}
	e->flags |= WUI_STORE_FLAG_DIRTY;
	return 0;
}

static int WiredStoreLua_GetValue( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e ) {
		lua_pushnumber( L, e->value );
	} else {
		lua_pushnil( L );
	}
	return 1;
}

static int WiredStoreLua_GetColor( lua_State *L ) {
	const char *key = luaL_checkstring( L, 1 );
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e ) {
		lua_pushnumber( L, e->color[0] );
		lua_pushnumber( L, e->color[1] );
		lua_pushnumber( L, e->color[2] );
		lua_pushnumber( L, e->color[3] );
		return 4;
	}
	lua_pushnil( L );
	return 1;
}

typedef struct {
	lua_State *L;
	int index;
} storePairsCtx_t;

static void WiredStoreLua_PairsCollect( wuiStoreEntry_t *entry, void *userData ) {
	storePairsCtx_t *ctx = (storePairsCtx_t *)userData;
	lua_State *L = ctx->L;

	ctx->index++;
	lua_pushinteger( L, ctx->index );
	lua_newtable( L );
	lua_pushstring( L, entry->key );
	lua_setfield( L, -2, "key" );
	lua_pushstring( L, entry->text );
	lua_setfield( L, -2, "text" );
	lua_pushnumber( L, entry->value );
	lua_setfield( L, -2, "value" );
	lua_rawset( L, -3 );
}

static int WiredStoreLua_PairsNext( lua_State *L ) {
	int idx;

	idx = (int)lua_tointeger( L, lua_upvalueindex( 2 ) ) + 1;
	lua_pushinteger( L, idx );
	lua_replace( L, lua_upvalueindex( 2 ) );

	lua_rawgeti( L, lua_upvalueindex( 1 ), idx );
	if ( lua_isnil( L, -1 ) ) {
		return 0;
	}

	lua_getfield( L, -1, "key" );
	lua_getfield( L, -2, "text" );
	lua_remove( L, -3 );
	return 2;
}

static int WiredStoreLua_Pairs( lua_State *L ) {
	const char *prefix;
	storePairsCtx_t ctx;

	prefix = luaL_optstring( L, 1, "" );

	lua_newtable( L );
	ctx.L = L;
	ctx.index = 0;
	WiredStore_ForEach( prefix, WiredStoreLua_PairsCollect, &ctx );

	lua_pushinteger( L, 0 );
	lua_pushcclosure( L, WiredStoreLua_PairsNext, 2 );
	return 1;
}

static int WiredStoreLua_SaveState( lua_State *L ) {
	WiredUI_SaveState();
	return 0;
}

static int WiredStoreLua_LoadState( lua_State *L ) {
	WiredUI_LoadState();
	return 0;
}

static const luaL_Reg wiredStoreLib[] = {
	{ "get",      WiredStoreLua_Get },
	{ "set",      WiredStoreLua_Set },
	{ "getvalue", WiredStoreLua_GetValue },
	{ "getcolor", WiredStoreLua_GetColor },
	{ "pairs",    WiredStoreLua_Pairs },
	{ "savestate", WiredStoreLua_SaveState },
	{ "loadstate", WiredStoreLua_LoadState },
	{ NULL, NULL }
};

static void WiredStoreLua_Register( lua_State *L ) {
	luaL_newlib( L, wiredStoreLib );
	lua_setglobal( L, "store" );
	Com_Printf( "WiredScript: store module registered\n" );
}

void WiredStoreLua_Init( void ) {
	WiredScript_RegisterBindings( WiredStoreLua_Register );
}

#endif /* FEAT_WIRED_UI */
