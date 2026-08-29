// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_SV_ENTITY_EVENTS_H
#define WIRED_SV_ENTITY_EVENTS_H

#include "../qcommon/wired/entity/event.h"

typedef struct lua_State lua_State;

void SV_EntityEvents_LuaRegister( lua_State *L );
void SV_EntityEvents_Shutdown( void );
qboolean SV_EntityEvents_Enqueue( const wiredEntityEvent_t *event );
void SV_EntityEvents_Drain( void );

#endif
