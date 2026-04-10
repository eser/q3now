#ifndef G_BOT_LUA_H
#define G_BOT_LUA_H

#include "g_bot_lua_shared.h"

typedef struct bot_state_s bot_state_t;
typedef struct bot_goal_s bot_goal_t;

int BotLua_ChooseWeapon( bot_state_t *bs, int fallbackWeapon );
float BotLua_Aggression( bot_state_t *bs );
int BotLua_WantsToRetreat( bot_state_t *bs );
int BotLua_WantsToChase( bot_state_t *bs );

int BotLua_ChooseLTGItem( bot_state_t *bs, int tfl );
int BotLua_ChooseNBGItem( bot_state_t *bs, int tfl, bot_goal_t *ltg, float range );
int BotLua_Chat( bot_state_t *bs, const char *eventName, const botLuaChatCtx_t *ctx );

float BotLua_ProfileFieldOr( bot_state_t *bs, int field, float fallback );

#endif
