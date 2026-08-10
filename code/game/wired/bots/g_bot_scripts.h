// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_BOT_SCRIPTS_H
#define G_BOT_SCRIPTS_H

#include "g_bot_scripts_shared.h"

typedef struct bot_state_s bot_state_t;
typedef struct bot_goal_s bot_goal_t;

int WiredIntel_ChooseWeapon( bot_state_t *bs, int fallbackWeapon );
float WiredIntel_Aggression( bot_state_t *bs );
int WiredIntel_WantsToRetreat( bot_state_t *bs );
int WiredIntel_WantsToChase( bot_state_t *bs );

int WiredIntel_ChooseLTGItem( bot_state_t *bs, int tfl );
int WiredIntel_ChooseNBGItem( bot_state_t *bs, int tfl, bot_goal_t *ltg, float range );
int WiredIntel_Chat( bot_state_t *bs, const char *eventName, const wbChatCtx_t *ctx );

float WiredIntel_ProfileFieldOr( bot_state_t *bs, int field, float fallback );
float WiredIntel_GetCurrentAttackAimHeight( bot_state_t *bs );

float WiredIntel_EffectiveSkill( bot_state_t *bs );
float WiredIntel_SkillFraction( bot_state_t *bs );
float WiredIntel_ResolveAbility( bot_state_t *bs, float min, float max );
float WiredIntel_AttackAccuracy( bot_state_t *bs, int weapon, int slot );

#endif
