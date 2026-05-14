// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
ai_weapsel.h — DPS-based weapon selection for bot AI improvements
===========================================================================
*/
#ifndef AI_WEAPSEL_H
#define AI_WEAPSEL_H

#include "../qcommon/q_feats.h"

struct bot_state_s;

// update per-weapon accuracy stats after combat (called each frame during combat)
void BotAccuracyUpdate( struct bot_state_s *bs );

// select weapon based on DPS for current combat zone.
// sets bs->best_weapon and bs->weapon_reason.
void BotChooseWeaponDPS( struct bot_state_s *bs );

// return combat zone index (ZONE_NEAR..ZONE_VERYFAR) for a given distance
int BotCombatZone( float dist );

#endif // AI_WEAPSEL_H
