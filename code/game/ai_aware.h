// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
ai_aware.h — entity/sound awareness for bot AI improvements
===========================================================================
*/
#ifndef AI_AWARE_H
#define AI_AWARE_H

#include "../qcommon/q_feats.h"

struct bot_state_s;

// process events and update awareness list each frame
void BotAwareUpdate( struct bot_state_s *bs );

// track an entity in the awareness list (if enemy and within radius)
void BotAwareTrackEntity( struct bot_state_s *bs, int entnum, float radius );

// return entity number of the best aware-but-not-visible enemy, or -1
int BotBestAwareEnemy( struct bot_state_s *bs );

#endif // AI_AWARE_H
