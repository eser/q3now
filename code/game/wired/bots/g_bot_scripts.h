#ifndef G_BOT_SCRIPTS_H
#define G_BOT_SCRIPTS_H

#include "g_bot_scripts_shared.h"

typedef struct bot_state_s bot_state_t;
typedef struct bot_goal_s bot_goal_t;

int WiredBots_ChooseWeapon( bot_state_t *bs, int fallbackWeapon );
float WiredBots_Aggression( bot_state_t *bs );
int WiredBots_WantsToRetreat( bot_state_t *bs );
int WiredBots_WantsToChase( bot_state_t *bs );

int WiredBots_ChooseLTGItem( bot_state_t *bs, int tfl );
int WiredBots_ChooseNBGItem( bot_state_t *bs, int tfl, bot_goal_t *ltg, float range );
int WiredBots_Chat( bot_state_t *bs, const char *eventName, const wbChatCtx_t *ctx );

float WiredBots_ProfileFieldOr( bot_state_t *bs, int field, float fallback );
float WiredBots_GetCurrentAttackAimHeight( bot_state_t *bs );

#endif
