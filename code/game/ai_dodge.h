/*
===========================================================================
ai_dodge.h — missile avoidance for bot AI improvements
===========================================================================
*/
#ifndef AI_DODGE_H
#define AI_DODGE_H

#include "q_feats.h"

#if FEAT_BOT_IMPROVEMENTS

// forward declaration
struct bot_state_s;

// scan g_entities[] for incoming missiles, populate bs->missile_dodge[]
void BotScanMissiles( struct bot_state_s *bs );

// evaluate dodge directions, set bs->dodge_dir and bs->dodge_active
void BotDodgeMovement( struct bot_state_s *bs );

// predict closest approach distance between two linear trajectories
// over a lookahead window. Returns squared distance for efficiency.
float TrajectoryClosestDistSq( vec3_t p1, vec3_t v1,
                               vec3_t p2, vec3_t v2,
                               float lookahead );

#endif // FEAT_BOT_IMPROVEMENTS
#endif // AI_DODGE_H
