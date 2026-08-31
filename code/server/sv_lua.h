// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef SV_LUA_H
#define SV_LUA_H

#include "../qcommon/q_shared.h"
#include "../game/wired/bots/g_bot_scripts_shared.h"

void SV_Lua_Init( void );
void SV_Lua_Shutdown( void );
void SV_Lua_EnsureInit( void );

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

// Monster-Lua binding — parallel to the bot binding, keyed by entityNum.
int SV_Lua_MonsterBind( int entityNum, int characterHandle );
void SV_Lua_MonsterUnbind( int entityNum );
float SV_Lua_MonsterProfileField( int entityNum, int field );
int SV_Lua_MonsterDecide( int entityNum, const wbDecideCtx_t *ctx, char *decision, int decisionSize );

void SV_BotVerifyCharacter_f( void );
void SV_BotDebugWeapons_f( void );

qboolean SV_Lua_GetCharacterDisplayName( const char *name, char *out, int outSize );
qboolean SV_Lua_GetCharacterPrimaryModel( const char *name, char *out, int outSize );
qboolean SV_Lua_GetCharacterBBox( const char *name, char *out, int outSize );
qboolean SV_Lua_GetCharacterMovement( const char *name, char *out, int outSize );
qboolean SV_Lua_GetCharacterAttack( const char *name, char *out, int outSize );
qboolean SV_Lua_GetCharacterCanActivate( const char *name, char *out, int outSize );
int      SV_Lua_GetCharacterCount( void );
qboolean SV_Lua_GetCharacterAt( int index, char *out, int outSize );

#endif
