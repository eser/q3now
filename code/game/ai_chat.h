// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//

/*****************************************************************************
 * name:		ai_chat.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_chat.c $
 *
 *****************************************************************************/

typedef struct bot_state_s bot_state_t;

#define TIME_BETWEENCHATTING	25

//
int BotChat_EnterGame(bot_state_t *bs);
//
int BotChat_ExitGame(bot_state_t *bs);
//
int BotChat_StartLevel(bot_state_t *bs);
//
int BotChat_EndLevel(bot_state_t *bs);
//
int BotChat_HitTalking(bot_state_t *bs);
//
int BotChat_HitNoDeath(bot_state_t *bs);
//
int BotChat_HitNoKill(bot_state_t *bs);
//
int BotChat_Death(bot_state_t *bs);
//
int BotChat_Kill(bot_state_t *bs);
//
int BotChat_EnemySuicide(bot_state_t *bs);
//
int BotChat_Random(bot_state_t *bs);
// time the selected chat takes to type in
float BotChatTime(bot_state_t *bs);
// returns true if the bot can chat at the current position
int BotValidChatPosition(bot_state_t *bs);
// test the initial bot chats
void BotChatTest(bot_state_t *bs);
// current map title
char *BotMapTitle(void);
// CTF event chat helpers — called from g_team.c
void BotCTFChatEvent( int clientNum, const char *eventName );
void BotCTFChatBroadcast( int excludeClient, int teamNum, const char *eventName );
