// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "server.h"
#include "sv_entity_events.h"
#if !defined(WASM_MODULE)
#include "../qcommon/wired/core/scripting/user_vm.h"

#include <lua.h>
#include <lauxlib.h>
#endif
#include <string.h>

LOG_DECLARE_CHANNEL( ch_server, "server" );

#define SV_ENTITY_EVENT_MAX_LUA_HANDLERS 64

#if !defined(WASM_MODULE)
typedef struct {
	uint32_t handle;
	uint32_t eventId;
	int callbackRef;
} svEntityEventLuaHandler_t;
#endif

static wiredEntityEventQueue_t s_queue;
#if !defined(WASM_MODULE)
static svEntityEventLuaHandler_t s_handlers[SV_ENTITY_EVENT_MAX_LUA_HANDLERS];
static uint32_t s_nextHandle = 1u;

static int LuaOn( lua_State *L )
{
	const char *name = luaL_checkstring( L, 1 );
	uint32_t eventId;
	int index;
	if ( !lua_isfunction( L, 2 ) ) return luaL_error( L, "wired.entity.on(name, callback)" );
	eventId = !strcmp( name, "*" ) ? 0u : WiredEntityEvent_NameId( name );
	if ( !eventId && strcmp( name, "*" ) ) return luaL_error( L, "invalid entity event name" );
	for ( index = 0; index < SV_ENTITY_EVENT_MAX_LUA_HANDLERS; ++index ) {
		if ( s_handlers[index].handle ) continue;
		lua_pushvalue( L, 2 );
		s_handlers[index].callbackRef = luaL_ref( L, LUA_REGISTRYINDEX );
		s_handlers[index].handle = s_nextHandle++;
		if ( !s_nextHandle ) s_nextHandle = 1u;
		s_handlers[index].eventId = eventId;
		lua_pushinteger( L, (lua_Integer)s_handlers[index].handle );
		return 1;
	}
	return luaL_error( L, "wired.entity handler table full" );
}

static int LuaOff( lua_State *L )
{
	uint32_t handle = (uint32_t)luaL_checkinteger( L, 1 );
	int index;
	for ( index = 0; index < SV_ENTITY_EVENT_MAX_LUA_HANDLERS; ++index ) {
		if ( s_handlers[index].handle != handle ) continue;
		luaL_unref( L, LUA_REGISTRYINDEX, s_handlers[index].callbackRef );
		memset( &s_handlers[index], 0, sizeof( s_handlers[index] ) );
		lua_pushboolean( L, 1 );
		return 1;
	}
	lua_pushboolean( L, 0 );
	return 1;
}

static int LuaStats( lua_State *L )
{
	lua_newtable( L );
	lua_pushinteger( L, (lua_Integer)s_queue.count ); lua_setfield( L, -2, "queued" );
	lua_pushnumber( L, (lua_Number)s_queue.acceptedCount ); lua_setfield( L, -2, "accepted" );
	lua_pushnumber( L, (lua_Number)s_queue.droppedCount ); lua_setfield( L, -2, "dropped" );
	return 1;
}
#endif

void SV_EntityEvents_Shutdown( void )
{
#if !defined(WASM_MODULE)
	lua_State *L = UserVM_GetState();
	int index;
	if ( L ) {
		for ( index = 0; index < SV_ENTITY_EVENT_MAX_LUA_HANDLERS; ++index )
			if ( s_handlers[index].handle )
				luaL_unref( L, LUA_REGISTRYINDEX, s_handlers[index].callbackRef );
	}
	memset( s_handlers, 0, sizeof( s_handlers ) );
	s_nextHandle = 1u;
#endif
	WiredEntityEventQueue_Init( &s_queue );
}

void SV_EntityEvents_LuaRegister( lua_State *L )
{
	SV_EntityEvents_Shutdown();
#if defined(WASM_MODULE)
	(void)L;
#else
	if ( !L || !lua_istable( L, -1 ) ) return;
	lua_newtable( L );
	lua_pushcfunction( L, LuaOn ); lua_setfield( L, -2, "on" );
	lua_pushcfunction( L, LuaOff ); lua_setfield( L, -2, "off" );
	lua_pushcfunction( L, LuaStats ); lua_setfield( L, -2, "stats" );
	lua_pushinteger( L, WIRED_ENTITY_EVENT_VALUE_NONE ); lua_setfield( L, -2, "NONE" );
	lua_pushinteger( L, WIRED_ENTITY_EVENT_VALUE_INT ); lua_setfield( L, -2, "INT" );
	lua_pushinteger( L, WIRED_ENTITY_EVENT_VALUE_FLOAT ); lua_setfield( L, -2, "FLOAT" );
	lua_pushinteger( L, WIRED_ENTITY_EVENT_VALUE_STRING ); lua_setfield( L, -2, "STRING" );
	lua_pushinteger( L, WIRED_ENTITY_EVENT_VALUE_VEC3 ); lua_setfield( L, -2, "VEC3" );
	lua_setfield( L, -2, "entity" );
#endif
}

qboolean SV_EntityEvents_Enqueue( const wiredEntityEvent_t *event )
{
	qboolean accepted = WiredEntityEventQueue_Enqueue( &s_queue, event );
	if ( !accepted && s_queue.count >= WIRED_ENTITY_EVENT_QUEUE_CAPACITY )
		Com_Log( SEV_WARN, LOG_CH(ch_server),
			"entity event queue full; dropped event %s for entity %d\n",
			event ? event->name : "<invalid>", event ? event->entityNum : -1 );
	return accepted;
}

#if !defined(WASM_MODULE)
static void PushEvent( lua_State *L, const wiredEntityEvent_t *event )
{
	lua_newtable( L );
	lua_pushnumber( L, (lua_Number)event->sequence ); lua_setfield( L, -2, "sequence" );
	lua_pushinteger( L, event->gameTime ); lua_setfield( L, -2, "game_time" );
	lua_pushinteger( L, event->entityNum ); lua_setfield( L, -2, "entity" );
	lua_pushinteger( L, event->sourceEntityNum ); lua_setfield( L, -2, "source_entity" );
	lua_pushinteger( L, event->stableEventId ); lua_setfield( L, -2, "event_id" );
	lua_pushinteger( L, event->stableFieldId ); lua_setfield( L, -2, "field_id" );
	lua_pushinteger( L, event->valueType ); lua_setfield( L, -2, "value_type" );
	lua_pushstring( L, event->name ); lua_setfield( L, -2, "name" );
	switch ( event->valueType ) {
	case WIRED_ENTITY_EVENT_VALUE_INT: lua_pushinteger( L, event->intValue ); break;
	case WIRED_ENTITY_EVENT_VALUE_FLOAT: lua_pushnumber( L, event->floatValue ); break;
	case WIRED_ENTITY_EVENT_VALUE_STRING: lua_pushstring( L, event->textValue ); break;
	case WIRED_ENTITY_EVENT_VALUE_VEC3:
		lua_newtable( L );
		lua_pushnumber( L, event->vectorValue[0] ); lua_rawseti( L, -2, 1 );
		lua_pushnumber( L, event->vectorValue[1] ); lua_rawseti( L, -2, 2 );
		lua_pushnumber( L, event->vectorValue[2] ); lua_rawseti( L, -2, 3 );
		break;
	default: lua_pushnil( L ); break;
	}
	lua_setfield( L, -2, "value" );
}
#endif

void SV_EntityEvents_Drain( void )
{
#if defined(WASM_MODULE)
	wiredEntityEvent_t event;
	while ( WiredEntityEventQueue_Pop( &s_queue, &event ) ) {}
#else
	lua_State *L = UserVM_GetState();
	uint32_t remaining = s_queue.count;
	wiredEntityEvent_t event;
	while ( remaining-- && WiredEntityEventQueue_Pop( &s_queue, &event ) ) {
		int index;
		if ( !L ) continue;
		for ( index = 0; index < SV_ENTITY_EVENT_MAX_LUA_HANDLERS; ++index ) {
			const char *error;
			if ( !s_handlers[index].handle ||
				( s_handlers[index].eventId && s_handlers[index].eventId != event.stableEventId ) )
				continue;
			lua_rawgeti( L, LUA_REGISTRYINDEX, s_handlers[index].callbackRef );
			PushEvent( L, &event );
			if ( lua_pcall( L, 1, 0, 0 ) == 0 ) continue;
			error = lua_tostring( L, -1 );
			Com_Log( SEV_WARN, LOG_CH(ch_server), "wired.entity callback failed: %s\n",
				error ? error : "unknown Lua error" );
			lua_pop( L, 1 );
		}
	}
#endif
}
