#ifndef SV_LUA_H
#define SV_LUA_H

#include "../qcommon/q_shared.h"
#include "../game/g_bot_lua_shared.h"

void SV_Lua_Init( void );
void SV_Lua_Shutdown( void );

int SV_Lua_LoadCharacter( const char *characterName, float skillNormalized );
void SV_Lua_FreeCharacter( int characterHandle );
float SV_Lua_CharacteristicBFloat( int characterHandle, int index, float min, float max );
void SV_Lua_CharacteristicString( int characterHandle, int index, char *buf, int size );

int SV_Lua_BindBot( int clientNum, int characterHandle );
int SV_Lua_BotThink( int clientNum, float thinktime );
float SV_Lua_BotProfileField( int clientNum, int field );
int SV_Lua_BotPickWeapon( int clientNum, const botLuaCombatCtx_t *ctx, char *weaponKey, int weaponKeySize );
int SV_Lua_BotEvalItem( int clientNum, const botLuaItemEvalCtx_t *ctx );
int SV_Lua_BotDecide( int clientNum, const botLuaDecideCtx_t *ctx, char *decision, int decisionSize );
int SV_Lua_BotOnChat( int clientNum, const char *eventName, const botLuaChatCtx_t *ctx, char *outChat, int outChatSize );
void SV_BotVerifyCharacter_f( void );
void SV_BotDebugWeapons_f( void );

#endif
