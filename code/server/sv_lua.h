#ifndef SV_LUA_H
#define SV_LUA_H

#include "../qcommon/q_shared.h"
#include "../game/wired/bots/g_bot_scripts_shared.h"

void SV_Lua_Init( void );
void SV_Lua_Shutdown( void );

int SV_Lua_LoadCharacter( const char *characterName, float skillNormalized );
void SV_Lua_FreeCharacter( int characterHandle );
float SV_Lua_CharacteristicBFloat( int characterHandle, int index, float min, float max );
void SV_Lua_CharacteristicString( int characterHandle, int index, char *buf, int size );

int SV_Lua_BindBot( int clientNum, int characterHandle );
int SV_Lua_BotThink( int clientNum, float thinktime );
float SV_Lua_BotProfileField( int clientNum, int field );
int SV_Lua_BotPickWeapon( int clientNum, const wbCombatCtx_t *ctx, char *weaponKey, int weaponKeySize );
float SV_Lua_BotGetAttackAimHeight( int clientNum, int weaponNum );
int SV_Lua_BotEvalItem( int clientNum, const wbItemEvalCtx_t *ctx );
int SV_Lua_BotDecide( int clientNum, const wbDecideCtx_t *ctx, char *decision, int decisionSize );
int SV_Lua_BotOnChat( int clientNum, const char *eventName, const wbChatCtx_t *ctx, char *outChat, int outChatSize );
void SV_BotVerifyCharacter_f( void );
void SV_BotDebugWeapons_f( void );

qboolean SV_Lua_GetCharacterDisplayName( const char *name, char *out, int outSize );
int      SV_Lua_GetCharacterCount( void );
qboolean SV_Lua_GetCharacterAt( int index, char *out, int outSize );

#endif
