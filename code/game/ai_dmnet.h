/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/
//

/*****************************************************************************
 * name:		ai_dmnet.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_chat.c $
 *
 *****************************************************************************/

#define MAX_NODESWITCHES	50

void AIEnter_Intermission(bot_state_t *bs, char *s);
void AIEnter_Observer(bot_state_t *bs, char *s);
void AIEnter_Respawn(bot_state_t *bs, char *s);
void AIEnter_Stand(bot_state_t *bs, char *s);
void AIEnter_Seek_ActivateEntity(bot_state_t *bs, char *s);
void AIEnter_Seek_NBG(bot_state_t *bs, char *s);
void AIEnter_Seek_LTG(bot_state_t *bs, char *s);
void AIEnter_Seek_Camp(bot_state_t *bs, char *s);
void AIEnter_Battle_Fight(bot_state_t *bs, char *s);
void AIEnter_Battle_Chase(bot_state_t *bs, char *s);
void AIEnter_Battle_Retreat(bot_state_t *bs, char *s);
void AIEnter_Battle_NBG(bot_state_t *bs, char *s);
int AINode_Intermission(bot_state_t *bs);
int AINode_Observer(bot_state_t *bs);
int AINode_Respawn(bot_state_t *bs);
int AINode_Stand(bot_state_t *bs);
int AINode_Seek_ActivateEntity(bot_state_t *bs);
int AINode_Seek_NBG(bot_state_t *bs);
int AINode_Seek_LTG(bot_state_t *bs);
int AINode_Battle_Fight(bot_state_t *bs);
int AINode_Battle_Chase(bot_state_t *bs);
int AINode_Battle_Retreat(bot_state_t *bs);
int AINode_Battle_NBG(bot_state_t *bs);

void BotResetNodeSwitches(void);
void BotDumpNodeSwitches(bot_state_t *bs);
