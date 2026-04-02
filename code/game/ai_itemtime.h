/*
===========================================================================
ai_itemtime.h — item respawn timing for bot AI improvements
===========================================================================
*/
#ifndef AI_ITEMTIME_H
#define AI_ITEMTIME_H

#include "../qcommon/q_feats.h"

#if FEAT_BOT_IMPROVEMENTS

struct bot_state_s;

// initialize item registry at level start (scan g_entities[])
void BotItemTimeInit( struct bot_state_s *bs );

// update item tracking each frame (detect pickups, compute ETAs)
void BotItemTimeUpdate( struct bot_state_s *bs );

// override nearby goal if a high-value item is about to respawn.
// returns qtrue if an item timing goal was set.
qboolean BotItemTimingGoal( struct bot_state_s *bs );

#endif // FEAT_BOT_IMPROVEMENTS
#endif // AI_ITEMTIME_H
