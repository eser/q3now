#ifndef SV_WIRED_RCON_H
#define SV_WIRED_RCON_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

void SV_RconAuth( const netadr_t *from );
void SV_RconVerify( const netadr_t *from, const char *hmacHex );
qboolean SV_RconAuthorized( const netadr_t *from );
void SV_RconCleanupSessions( void );
void SV_RconExecute( const netadr_t *from, const char *luaCode );

#endif
