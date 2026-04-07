#ifndef SV_WIRED_RCON_LUA_H
#define SV_WIRED_RCON_LUA_H

#include "../qcommon/q_shared.h"

void SV_RconLua_Init( void );
void SV_RconLua_Shutdown( void );
qboolean SV_RconLua_Execute( const char *code, char *output, int outputLen );

#endif
