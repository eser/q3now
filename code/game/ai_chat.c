/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//

/*****************************************************************************
 * name:		ai_chat.c
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /MissionPack/code/game/ai_chat.c $
 *
 *****************************************************************************/

#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "wired/bots/g_bot_scripts.h"
#include "wired/bots/g_wiredbots.h"
//
#include "chars.h"				//characteristics
#include "inv.h"				//indexes into the inventory
#include "syn.h"				//synonyms
#include "match.h"				//string matching types and vars

// for the voice chats
#if FEAT_TA_VOICECHAT
#include "../qcommon/menudef.h"
#endif



/*
==================
BotNumActivePlayers
==================
*/
int BotNumActivePlayers(void) {
	int i, num;
	char buf[MAX_INFO_STRING];

	num = 0;
	for (i = 0; i < level.maxclients; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		num++;
	}
	return num;
}

/*
==================
BotIsFirstInRankings
==================
*/
int BotIsFirstInRankings(bot_state_t *bs) {
	int i, score;
	char buf[MAX_INFO_STRING];
	playerState_t ps;

	score = bs->cur_ps.persistant[PERS_SCORE];
	for (i = 0; i < level.maxclients; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		if (BotAI_GetClientState(i, &ps) && score < ps.persistant[PERS_SCORE]) return qfalse;
	}
	return qtrue;
}

/*
==================
BotIsLastInRankings
==================
*/
int BotIsLastInRankings(bot_state_t *bs) {
	int i, score;
	char buf[MAX_INFO_STRING];
	playerState_t ps;

	score = bs->cur_ps.persistant[PERS_SCORE];
	for (i = 0; i < level.maxclients; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		if (BotAI_GetClientState(i, &ps) && score > ps.persistant[PERS_SCORE]) return qfalse;
	}
	return qtrue;
}

/*
==================
BotFirstClientInRankings
==================
*/
char *BotFirstClientInRankings(void) {
	int i, bestscore, bestclient;
	char buf[MAX_INFO_STRING];
	static char name[32];
	playerState_t ps;

	bestscore = -999999;
	bestclient = 0;
	for (i = 0; i < level.maxclients; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		if (BotAI_GetClientState(i, &ps) && ps.persistant[PERS_SCORE] > bestscore) {
			bestscore = ps.persistant[PERS_SCORE];
			bestclient = i;
		}
	}
	EasyClientName(bestclient, name, 32);
	return name;
}

/*
==================
BotLastClientInRankings
==================
*/
char *BotLastClientInRankings(void) {
	int i, worstscore, bestclient;
	char buf[MAX_INFO_STRING];
	static char name[32];
	playerState_t ps;

	worstscore = 999999;
	bestclient = 0;
	for (i = 0; i < level.maxclients; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//
		if (BotAI_GetClientState(i, &ps) && ps.persistant[PERS_SCORE] < worstscore) {
			worstscore = ps.persistant[PERS_SCORE];
			bestclient = i;
		}
	}
	EasyClientName(bestclient, name, 32);
	return name;
}

/*
==================
BotRandomOpponentName
==================
*/
char *BotRandomOpponentName(bot_state_t *bs) {
	int i, count;
	char buf[MAX_INFO_STRING];
	int opponents[MAX_CLIENTS], numopponents;
	static char name[32];

	numopponents = 0;
	opponents[0] = 0;
	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client) continue;
		//
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "n"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		//skip team mates
		if (BotSameTeam(bs, i)) continue;
		//
		opponents[numopponents] = i;
		numopponents++;
	}
	count = random() * numopponents;
	for (i = 0; i < numopponents; i++) {
		count--;
		if (count <= 0) {
			EasyClientName(opponents[i], name, sizeof(name));
			return name;
		}
	}
	EasyClientName(opponents[0], name, sizeof(name));
	return name;
}

/*
==================
BotMapTitle
==================
*/

char *BotMapTitle(void) {
	char info[1024];
	static char mapname[128];

	trap_GetServerinfo(info, sizeof(info));

	Q_strncpyz(mapname, Info_ValueForKey( info, "mapname" ), sizeof(mapname));

	return mapname;
}

static void WiredBots_InitChatCtx( bot_state_t *bs, wbChatCtx_t *ctx ) {
	if ( !ctx ) {
		return;
	}

	memset( ctx, 0, sizeof( *ctx ) );

	if ( !bs ) {
		return;
	}

	if ( bs->client >= 0 && bs->client < MAX_CLIENTS ) {
		ClientName( bs->client, ctx->sender, sizeof( ctx->sender ) );
		ctx->score = g_entities[bs->client].client ? g_entities[bs->client].client->ps.persistant[PERS_SCORE] : 0;
	}

	Q_strncpyz( ctx->map, BotMapTitle(), sizeof( ctx->map ) );

	switch ( gametype ) {
		case GT_DEATHMATCH: Q_strncpyz( ctx->gametype, "ffa", sizeof( ctx->gametype ) ); break;
		case GT_DUEL: Q_strncpyz( ctx->gametype, "duel", sizeof( ctx->gametype ) ); break;
		case GT_TDM: Q_strncpyz( ctx->gametype, "tdm", sizeof( ctx->gametype ) ); break;
		case GT_CTF: Q_strncpyz( ctx->gametype, "ctf", sizeof( ctx->gametype ) ); break;
		case GT_1FCTF: Q_strncpyz( ctx->gametype, "1fctf", sizeof( ctx->gametype ) ); break;
		default: Q_strncpyz( ctx->gametype, "other", sizeof( ctx->gametype ) ); break;
	}
}


/*
==================
BotLuaWeaponKeyForMOD
Returns a short Lua-dispatch key for a means-of-death.
Non-weapon deaths use descriptive keys; weapons use shortnames.
==================
*/
static const char *BotLuaWeaponKeyForMOD(int mod) {
	int att, wp;

	switch (mod) {
		case MOD_WATER: case MOD_SLIME: return "slime";
		case MOD_LAVA:                  return "lava";
		case MOD_FALLING:               return "fall";
		case MOD_TELEFRAG:              return "telefrag";
		case MOD_CRUSH:                 return "crush";
		case MOD_SUICIDE:
		case MOD_TRIGGER_HURT:          return "suicide";
		case MOD_KAMIKAZE:              return "kamikaze";
		default: break;
	}

	att = G_AttackFromMOD(mod);
	if (att > ATT_NONE && att < ATT_NUM_ATTACKS) {
		wp = bg_attacklist[att].weapon;
		if (wp > WP_NONE && wp < WP_NUM_WEAPONS && bg_weaponlist[wp].shortname) {
			return bg_weaponlist[wp].shortname;
		}
	}
	return "";
}

/*
==================
BotWeaponNameForMeansOfDeath
==================
*/

char *BotWeaponNameForMeansOfDeath(int mod) {
	int att = G_AttackFromMOD( mod );
	if ( att > ATT_NONE && att < ATT_NUM_ATTACKS ) {
		int wp = bg_attacklist[att].weapon;
		if ( wp >= 0 && wp < WP_NUM_WEAPONS ) {
			return bg_weaponlist[wp].name;
		}
	}
	// fallback for non-weapon MODs
	switch(mod) {
		case MOD_KAMIKAZE: return "Kamikaze";
		case MOD_GRAPPLE: return "Grapple";
		default: return "[unknown weapon]";
	}
}

/*
==================
BotRandomWeaponName
==================
*/
char *BotRandomWeaponName(void) {
	int rnd;

	rnd = random() * 8.9;

	switch(rnd) {
		case 0: return "Gauntlet";
		case 1: return "Shotgun";
		case 2: return "Machinegun";
		case 3: return "Grenade Launcher";
		case 4: return "Rocket Launcher";
		case 5: return "Plasma Rifle";
		case 6: return "Railgun";
        default: return "Lightning Gun";
	}
}

/*
==================
BotVisibleEnemies
==================
*/
int BotVisibleEnemies(bot_state_t *bs) {
	float vis;
	aas_entityinfo_t entinfo;

	for (int i = 0; i < MAX_CLIENTS; i++) {

		if (i == bs->client) continue;
		//
		BotEntityInfo(i, &entinfo);
		//
		if (!entinfo.valid) continue;
		//if the enemy isn't dead and the enemy isn't the bot self
		if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum) continue;
		//if the enemy is invisible and not shooting
		if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
			continue;
		}
		//if on the same team
		if (BotSameTeam(bs, i)) continue;
		//check if the enemy is visible
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis > 0) return qtrue;
	}
	return qfalse;
}

/*
==================
BotValidChatPosition
==================
*/
int BotValidChatPosition(bot_state_t *bs) {
	vec3_t point, start, end, mins, maxs;
	bsp_trace_t trace;

	//if the bot is dead all positions are valid
	if (BotIsDead(bs)) return qtrue;
	//never start chatting with a powerup
	if (bs->inventory[INVENTORY_QUAD] ||
		bs->inventory[INVENTORY_BERSERK] ||
		bs->inventory[INVENTORY_ENVIRONMENTSUIT] ||
		bs->inventory[INVENTORY_HASTE] ||
		bs->inventory[INVENTORY_INVISIBILITY] ||
		bs->inventory[INVENTORY_REGEN] ||
		bs->inventory[INVENTORY_FLIGHT]) return qfalse;
	//must be on the ground
	//if (bs->cur_ps.groundEntityNum != ENTITYNUM_NONE) return qfalse;
	//do not chat if in lava or slime
	VectorCopy(bs->origin, point);
	point[2] -= 24;
	if (trap_PointContents(point,bs->entitynum) & (CONTENTS_LAVA|CONTENTS_SLIME)) return qfalse;
	//do not chat if under water
	VectorCopy(bs->origin, point);
	point[2] += 32;
	if (trap_PointContents(point,bs->entitynum) & MASK_WATER) return qfalse;
	//must be standing on the world entity
	VectorCopy(bs->origin, start);
	VectorCopy(bs->origin, end);
	start[2] += 1;
	end[2] -= 10;
	trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, mins, maxs);
	BotAI_Trace(&trace, start, mins, maxs, end, bs->client, MASK_SOLID);
	if (trace.ent != ENTITYNUM_WORLD) return qfalse;
	//the bot is in a position where it can chat
	return qtrue;
}

/*
==================
BotChat_EnterGame
==================
*/
int BotChat_EnterGame(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		return WiredBots_Chat( bs, "game_enter", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	//don't chat in teamplay
	if (TeamPlayIsOn()) return qfalse;
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_ENTEREXITGAME, 0, 1);
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (BotNumActivePlayers() <= 1) return qfalse;
	if (!BotValidChatPosition(bs)) return qfalse;
	BotAI_BotInitialChat(bs, "game_enter",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				"[invalid var]",						// 2
				"[invalid var]",						// 3
				BotMapTitle(),							// 4
				NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_ExitGame
==================
*/
int BotChat_ExitGame(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		return WiredBots_Chat( bs, "game_exit", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	//don't chat in teamplay
	if (TeamPlayIsOn()) return qfalse;
	// don't chat in duel mode
    if (gametype == GT_DUEL) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_ENTEREXITGAME, 0, 1);
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (BotNumActivePlayers() <= 1) return qfalse;
	//
	BotAI_BotInitialChat(bs, "game_exit",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				"[invalid var]",						// 2
				"[invalid var]",						// 3
				BotMapTitle(),							// 4
				NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_StartLevel
==================
*/
int BotChat_StartLevel(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		if ( TeamPlayIsOn() ) WiredBots_Announce( bs, WB_TAUNT_GENERIC, NULL );
		return WiredBots_Chat( bs, "level_start", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (BotIsObserver(bs)) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	//don't chat in teamplay
	if (TeamPlayIsOn()) {
	    return qfalse;
	}
	// don't chat in duel mode
    if (gametype == GT_DUEL || gametype == GT_LASTMANSTANDING) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_STARTENDLEVEL, 0, 1);
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (BotNumActivePlayers() <= 1) return qfalse;
	BotAI_BotInitialChat(bs, "level_start",
				EasyClientName(bs->client, name, 32),	// 0
				NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_EndLevel
==================
*/
int BotChat_EndLevel(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		ctx.won = BotIsFirstInRankings( bs ) ? 1 : ( BotIsLastInRankings( bs ) ? -1 : 0 );
		if ( TeamPlayIsOn() && BotIsFirstInRankings( bs ) ) WiredBots_Announce( bs, WB_TAUNT_PRAISE, NULL );
		if ( ( gametype == GT_LASTMANSTANDING || gametype == GT_DUEL )
		     && !BotIsFirstInRankings( bs ) ) {
			return WiredBots_Chat( bs, "level_end_eliminated", &ctx );
		}
		return WiredBots_Chat( bs, "level_end", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (BotIsObserver(bs)) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	// teamplay
	if (TeamPlayIsOn())
	{
		return qtrue;
	}
	// don't chat in duel mode
    if (gametype == GT_DUEL || gametype == GT_LASTMANSTANDING) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_STARTENDLEVEL, 0, 1);
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (BotNumActivePlayers() <= 1) return qfalse;
	//
	if (BotIsFirstInRankings(bs)) {
		BotAI_BotInitialChat(bs, "level_end_victory",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				"[invalid var]",						// 2
				BotLastClientInRankings(),				// 3
				BotMapTitle(),							// 4
				NULL);
	}
	else if (BotIsLastInRankings(bs)) {
		BotAI_BotInitialChat(bs, "level_end_lose",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				BotFirstClientInRankings(),				// 2
				"[invalid var]",						// 3
				BotMapTitle(),							// 4
				NULL);
	}
	else {
		BotAI_BotInitialChat(bs, "level_end",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				BotFirstClientInRankings(),				// 2
				BotLastClientInRankings(),				// 3
				BotMapTitle(),							// 4
				NULL);
	}
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_Death
==================
*/
int BotChat_Death(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		ctx.team = ( TeamPlayIsOn() && BotSameTeam( bs, bs->lastkilledby ) ) ? 1 : 0;
		if ( bs->lastkilledby >= 0 && bs->lastkilledby < MAX_CLIENTS ) {
			ClientName( bs->lastkilledby, ctx.killer, sizeof( ctx.killer ) );
		}
		Q_strncpyz( ctx.weapon, BotLuaWeaponKeyForMOD( bs->botdeathtype ), sizeof( ctx.weapon ) );
		if ( TeamPlayIsOn() ) WiredBots_Announce( bs, WB_TAUNT_DEATH, NULL );
		{
			float insultRate = WiredBots_ProfileFieldOr( bs, WB_PROFILE_CHAT_INSULT, 0.0f );
			if ( insultRate > 0.0f && random() < insultRate ) {
				if ( WiredBots_Chat( bs, "death_insult", &ctx ) ) return qtrue;
			}
		}
		return WiredBots_Chat( bs, "death", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_DEATH, 0, 1);
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//if fast chatting is off
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (BotNumActivePlayers() <= 1) return qfalse;
	//
	if (bs->lastkilledby >= 0 && bs->lastkilledby < MAX_CLIENTS)
		EasyClientName(bs->lastkilledby, name, 32);
	else
		strcpy(name, "[world]");
	//
	if (TeamPlayIsOn() && BotSameTeam(bs, bs->lastkilledby)) {
		if (bs->lastkilledby == bs->client) return qfalse;
		BotAI_BotInitialChat(bs, "death_teammate", name, NULL);
		bs->chatto = CHAT_TEAM;
	}
	else
	{
		//teamplay
		if (TeamPlayIsOn()) {
			return qtrue;
		}
		//
		if (bs->botdeathtype == MOD_WATER)
			BotAI_BotInitialChat(bs, "death_drown", BotRandomOpponentName(bs), NULL);
		else if (bs->botdeathtype == MOD_SLIME)
			BotAI_BotInitialChat(bs, "death_slime", BotRandomOpponentName(bs), NULL);
		else if (bs->botdeathtype == MOD_LAVA)
			BotAI_BotInitialChat(bs, "death_lava", BotRandomOpponentName(bs), NULL);
		else if (bs->botdeathtype == MOD_FALLING)
			BotAI_BotInitialChat(bs, "death_cratered", BotRandomOpponentName(bs), NULL);
		else if (bs->botsuicide || //all other suicides by own weapon
				bs->botdeathtype == MOD_CRUSH ||
				bs->botdeathtype == MOD_SUICIDE ||
				bs->botdeathtype == MOD_TARGET_LASER ||
				bs->botdeathtype == MOD_TRIGGER_HURT ||
				bs->botdeathtype == MOD_UNKNOWN)
			BotAI_BotInitialChat(bs, "death_suicide", BotRandomOpponentName(bs), NULL);
		else if (bs->botdeathtype == MOD_TELEFRAG)
			BotAI_BotInitialChat(bs, "death_telefrag", name, NULL);
		else if (bs->botdeathtype == MOD_KAMIKAZE && trap_BotNumInitialChats(bs->cs, "death_kamikaze"))
			BotAI_BotInitialChat(bs, "death_kamikaze", name, NULL);
		else {
			if ((bs->botdeathtype == MOD_GAUNTLET || bs->botdeathtype == MOD_GAUNTLET_LUNGE ||
				bs->botdeathtype == MOD_RAILGUN) && random() < 0.5) {

				if (bs->botdeathtype == MOD_GAUNTLET || bs->botdeathtype == MOD_GAUNTLET_LUNGE)
					BotAI_BotInitialChat(bs, "death_gauntlet",
							name,												// 0
							BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
							NULL);
				else
					BotAI_BotInitialChat(bs, "death_rail",
							name,												// 0
							BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
							NULL);
			}
			//choose between insult and praise
			else if (random() < trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_INSULT, 0, 1)) {
				BotAI_BotInitialChat(bs, "death_insult",
							name,												// 0
							BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
							NULL);
			}
			else {
				BotAI_BotInitialChat(bs, "death_praise",
							name,												// 0
							BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
							NULL);
			}
		}
		bs->chatto = CHAT_ALL;
	}
	bs->lastchat_time = FloatTime();
	return qtrue;
}

/*
==================
BotGetCurrentRank
Returns 0-based rank of the bot in level.sortedClients (0 = first place).
Returns -1 if not found.
==================
*/
static int BotGetCurrentRank( bot_state_t *bs ) {
	int i, max;
	max = g_maxclients.integer;
	for ( i = 0; i < max; i++ ) {
		if ( level.sortedClients[i] == bs->client ) return i;
	}
	return -1;
}

/*
==================
BotChat_Kill
==================
*/
int BotChat_Kill(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		int curRank;
		WiredBots_InitChatCtx( bs, &ctx );
		ctx.team = ( TeamPlayIsOn() && BotSameTeam( bs, bs->lastkilledplayer ) ) ? 1 : 0;
		if ( bs->lastkilledplayer >= 0 && bs->lastkilledplayer < MAX_CLIENTS ) {
			ClientName( bs->lastkilledplayer, ctx.victim, sizeof( ctx.victim ) );
		}
		Q_strncpyz( ctx.weapon, BotLuaWeaponKeyForMOD( bs->enemydeathtype ), sizeof( ctx.weapon ) );
		if ( TeamPlayIsOn() ) WiredBots_Announce( bs, WB_TAUNT_KILL, NULL );

		// Score milestone checks — fire instead of regular kill chat when applicable.
		// prev_rank == -1 means "not yet initialized"; skip first kill to avoid false positives.
		if ( bs->prev_rank < 0 ) bs->prev_rank = BotGetCurrentRank( bs );
		curRank = BotGetCurrentRank( bs );
		ctx.score = bs->num_kills;
		if ( curRank >= 0 && bs->prev_rank >= 0 ) {
			// took first place
			if ( curRank == 0 && bs->prev_rank != 0 ) {
				bs->prev_rank = curRank;
				if ( WiredBots_Chat( bs, "score_first_place", &ctx ) ) return qtrue;
			// lost first place
			} else if ( curRank > 0 && bs->prev_rank == 0 ) {
				bs->prev_rank = curRank;
				if ( WiredBots_Chat( bs, "score_falling_back", &ctx ) ) return qtrue;
			// at last place
			} else if ( curRank == g_maxclients.integer - 1 ) {
				bs->prev_rank = curRank;
				if ( WiredBots_Chat( bs, "score_last_place", &ctx ) ) return qtrue;
			}
		}
		if ( curRank >= 0 ) bs->prev_rank = curRank;
		if ( bs->num_kills > 0 && bs->num_kills % 5 == 0 ) {
			ctx.count = bs->num_kills;
			if ( WiredBots_Chat( bs, "score_frag_milestone", &ctx ) ) return qtrue;
		}

		// Streak events — fire instead of regular kill chat.
		ctx.count = bs->current_streak;
		if ( bs->current_streak >= 2 && bs->last_streak_ack > 0.0f &&
		     (bs->last_kill_time - bs->last_streak_ack) < 2.0f ) {
			if ( WiredBots_Chat( bs, "kill_double", &ctx ) ) return qtrue;
		}
		if ( bs->current_streak == 5 ) {
			if ( WiredBots_Chat( bs, "kill_streak_5", &ctx ) ) return qtrue;
		}
		if ( bs->current_streak == 10 ) {
			if ( WiredBots_Chat( bs, "kill_streak_10", &ctx ) ) return qtrue;
		}
		if ( bs->current_streak >= 15 ) {
			if ( WiredBots_Chat( bs, "kill_rampage", &ctx ) ) return qtrue;
		}

		{
			float insultRate = WiredBots_ProfileFieldOr( bs, WB_PROFILE_CHAT_INSULT, 0.0f );
			if ( insultRate > 0.0f && random() < insultRate ) {
				if ( WiredBots_Chat( bs, "kill_insult", &ctx ) ) return qtrue;
			}
		}
		return WiredBots_Chat( bs, "kill", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_KILL, 0, 1);
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//if fast chat is off
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (bs->lastkilledplayer == bs->client) return qfalse;
	if (BotNumActivePlayers() <= 1) return qfalse;
	if (!BotValidChatPosition(bs)) return qfalse;
	//
	if (BotVisibleEnemies(bs)) return qfalse;
	//
	EasyClientName(bs->lastkilledplayer, name, 32);
	//
	bs->chatto = CHAT_ALL;
	if (TeamPlayIsOn() && BotSameTeam(bs, bs->lastkilledplayer)) {
		BotAI_BotInitialChat(bs, "kill_teammate", name, NULL);
		bs->chatto = CHAT_TEAM;
	}
	else
	{
		//don't chat in teamplay
		if (TeamPlayIsOn()) {
			return qfalse;			// don't wait
		}
		//
		if (bs->enemydeathtype == MOD_GAUNTLET || bs->enemydeathtype == MOD_GAUNTLET_LUNGE) {
			BotAI_BotInitialChat(bs, "kill_gauntlet", name, NULL);
		}
		else if (bs->enemydeathtype == MOD_RAILGUN) {
			BotAI_BotInitialChat(bs, "kill_rail", name, NULL);
		}
		else if (bs->enemydeathtype == MOD_TELEFRAG) {
			BotAI_BotInitialChat(bs, "kill_telefrag", name, NULL);
		}
		else if (bs->botdeathtype == MOD_KAMIKAZE && trap_BotNumInitialChats(bs->cs, "kill_kamikaze"))
			BotAI_BotInitialChat(bs, "kill_kamikaze", name, NULL);
		//choose between insult and praise
		else if (random() < trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_INSULT, 0, 1)) {
			BotAI_BotInitialChat(bs, "kill_insult", name, NULL);
		}
		else {
			BotAI_BotInitialChat(bs, "kill_praise", name, NULL);
		}
	}
	bs->lastchat_time = FloatTime();
	return qtrue;
}

/*
==================
BotChat_EnemySuicide
==================
*/
int BotChat_EnemySuicide(bot_state_t *bs) {
	char name[32];
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		if ( bs->enemy >= 0 && bs->enemy < MAX_CLIENTS ) {
			ClientName( bs->enemy, ctx.victim, sizeof( ctx.victim ) );
		}
		return WiredBots_Chat( bs, "enemy_suicide", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	if (BotNumActivePlayers() <= 1) return qfalse;
	//
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_ENEMYSUICIDE, 0, 1);
	//don't chat in teamplay
	if (TeamPlayIsOn()) return qfalse;
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//if fast chat is off
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
	}
	if (!BotValidChatPosition(bs)) return qfalse;
	//
	if (BotVisibleEnemies(bs)) return qfalse;
	//
	if (bs->enemy >= 0) EasyClientName(bs->enemy, name, 32);
	else strcpy(name, "");
	BotAI_BotInitialChat(bs, "enemy_suicide", name, NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_HitTalking
==================
*/
int BotChat_HitTalking(bot_state_t *bs) {
	char name[32], *weap;
	int lasthurt_client;
	float rnd;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		return WiredBots_Chat( bs, "hit_talking", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	if (BotNumActivePlayers() <= 1) return qfalse;
	lasthurt_client = g_entities[bs->client].client->lasthurt_client;
	if (!lasthurt_client) return qfalse;
	if (lasthurt_client == bs->client) return qfalse;
	//
	if (lasthurt_client < 0 || lasthurt_client >= MAX_CLIENTS) return qfalse;
	//
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_HITTALKING, 0, 1);
	//don't chat in teamplay
	if (TeamPlayIsOn()) return qfalse;
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//if fast chat is off
	if (!bot_fastchat.integer) {
		if (random() > rnd * 0.5) return qfalse;
	}
	if (!BotValidChatPosition(bs)) return qfalse;
	//
	ClientName(g_entities[bs->client].client->lasthurt_client, name, sizeof(name));
	weap = BotWeaponNameForMeansOfDeath(g_entities[bs->client].client->lasthurt_mod);
	//
	BotAI_BotInitialChat(bs, "hit_talking", name, weap, NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_HitNoDeath
==================
*/
int BotChat_HitNoDeath(bot_state_t *bs) {
	char name[32], *weap;
	float rnd;
	int lasthurt_client;
	aas_entityinfo_t entinfo;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		return WiredBots_Chat( bs, "hit_nodeath", &ctx );
	}

	lasthurt_client = g_entities[bs->client].client->lasthurt_client;
	if (!lasthurt_client) return qfalse;
	if (lasthurt_client == bs->client) return qfalse;
	//
	if (lasthurt_client < 0 || lasthurt_client >= MAX_CLIENTS) return qfalse;
	//
	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	if (BotNumActivePlayers() <= 1) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_HITNODEATH, 0, 1);
	//don't chat in teamplay
	if (TeamPlayIsOn()) return qfalse;
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//if fast chat is off
	if (!bot_fastchat.integer) {
		if (random() > rnd * 0.5) return qfalse;
	}
	if (!BotValidChatPosition(bs)) return qfalse;
	//
	if (BotVisibleEnemies(bs)) return qfalse;
	//
	BotEntityInfo(bs->enemy, &entinfo);
	if (EntityIsShooting(&entinfo)) return qfalse;
	//
	ClientName(lasthurt_client, name, sizeof(name));
	weap = BotWeaponNameForMeansOfDeath(g_entities[bs->client].client->lasthurt_mod);
	//
	BotAI_BotInitialChat(bs, "hit_nodeath", name, weap, NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_HitNoKill
==================
*/
int BotChat_HitNoKill(bot_state_t *bs) {
	char name[32], *weap;
	float rnd;
	aas_entityinfo_t entinfo;

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		return WiredBots_Chat( bs, "hit_nokill", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	if (BotNumActivePlayers() <= 1) return qfalse;
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_HITNOKILL, 0, 1);
	//don't chat in teamplay
	if (TeamPlayIsOn()) return qfalse;
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//if fast chat is off
	if (!bot_fastchat.integer) {
		if (random() > rnd * 0.5) return qfalse;
	}
	if (!BotValidChatPosition(bs)) return qfalse;
	//
	if (BotVisibleEnemies(bs)) return qfalse;
	//
	BotEntityInfo(bs->enemy, &entinfo);
	if (EntityIsShooting(&entinfo)) return qfalse;
	//
	ClientName(bs->enemy, name, sizeof(name));
	weap = BotWeaponNameForMeansOfDeath(g_entities[bs->enemy].client->lasthurt_mod);
	//
	BotAI_BotInitialChat(bs, "hit_nokill", name, weap, NULL);
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotChat_Random
==================
*/
int BotChat_Random(bot_state_t *bs) {
	float rnd;
	char name[32];

	if ( bs->wiredBotsActive ) {
		wbChatCtx_t ctx;
		WiredBots_InitChatCtx( bs, &ctx );
		if ( TeamPlayIsOn() ) {
			WiredBots_Announce( bs, WB_TAUNT_GENERIC, NULL );
			ctx.team = 1;
			if ( bs->lasthealth < 30 ) {
				if ( WiredBots_Chat( bs, "team_need_health", &ctx ) ) return qtrue;
			}
			{
				int weapons = bs->cur_ps.stats[STAT_WEAPONS];
				if ( ( weapons & ~( (1 << WP_GAUNTLET) | (1 << WP_MACHINEGUN) ) ) == 0 ) {
					if ( WiredBots_Chat( bs, "team_need_weapon", &ctx ) ) return qtrue;
				}
			}
			if ( gametype == GT_CTF && BotCTFCarryingFlag( bs ) ) {
				if ( WiredBots_Chat( bs, "team_got_flag_need_support", &ctx ) ) return qtrue;
			}
			if ( bs->ltgtype == LTG_ATTACKENEMYBASE ) {
				if ( WiredBots_Chat( bs, "team_enemy_base_attack", &ctx ) ) return qtrue;
			}
			if ( bs->ltgtype == LTG_DEFENDKEYAREA ) {
				if ( WiredBots_Chat( bs, "team_defending_base", &ctx ) ) return qtrue;
			}
			if ( bs->ltgtype == LTG_RUSHBASE && gametype == GT_CTF && BotCTFCarryingFlag( bs ) ) {
				if ( WiredBots_Chat( bs, "team_follow_me", &ctx ) ) return qtrue;
			}
			if ( bs->enemy >= 0 && bs->lasthealth < 50 ) {
				if ( WiredBots_Chat( bs, "team_cover_me", &ctx ) ) return qtrue;
			}
			ctx.team = 0;
		}
		return WiredBots_Chat( bs, "random", &ctx );
	}

	if (bot_nochat.integer) return qfalse;
	if (BotIsObserver(bs)) return qfalse;
	if (bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING) return qfalse;
	// don't chat in duel mode
	if (gametype == GT_DUEL) return qfalse;
	//don't chat when doing something important :)
	if (bs->ltgtype == LTG_TEAMHELP ||
		bs->ltgtype == LTG_TEAMACCOMPANY ||
		bs->ltgtype == LTG_RUSHBASE) return qfalse;
	//
	rnd = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_RANDOM, 0, 1);
	if (random() > bs->thinktime * 0.1) return qfalse;
	if (!bot_fastchat.integer) {
		if (random() > rnd) return qfalse;
		if (random() > 0.25) return qfalse;
	}
	if (BotNumActivePlayers() <= 1) return qfalse;
	//
	if (!BotValidChatPosition(bs)) return qfalse;
	//
	if (BotVisibleEnemies(bs)) return qfalse;
	//
	if (bs->lastkilledplayer == bs->client) {
		strcpy(name, BotRandomOpponentName(bs));
	}
	else {
		EasyClientName(bs->lastkilledplayer, name, sizeof(name));
	}
	if (TeamPlayIsOn()) {
		return qfalse;			// don't wait
	}
	//
	if (random() < trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_MISC, 0, 1)) {
		BotAI_BotInitialChat(bs, "random_misc",
					BotRandomOpponentName(bs),	// 0
					name,						// 1
					"[invalid var]",			// 2
					"[invalid var]",			// 3
					BotMapTitle(),				// 4
					BotRandomWeaponName(),		// 5
					NULL);
	}
	else {
		BotAI_BotInitialChat(bs, "random_insult",
					BotRandomOpponentName(bs),	// 0
					name,						// 1
					"[invalid var]",			// 2
					"[invalid var]",			// 3
					BotMapTitle(),				// 4
					BotRandomWeaponName(),		// 5
					NULL);
	}
	bs->lastchat_time = FloatTime();
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotCTFChatEvent
Fire a CTF chat event for one specific bot client.
Called from g_team.c; returns without action for non-WiredBots.
==================
*/
void BotCTFChatEvent( int clientNum, const char *eventName ) {
	bot_state_t *bs;
	wbChatCtx_t ctx;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return;
	bs = botstates[clientNum];
	if ( !bs || !bs->inuse || !bs->wiredBotsActive ) return;

	WiredBots_InitChatCtx( bs, &ctx );
	ctx.team = 1;
	WiredBots_Chat( bs, eventName, &ctx );
}

/*
==================
BotCTFChatBroadcast
Fire a CTF chat event for every active WiredBots bot on teamNum.
Pass excludeClient = -1 to broadcast to all bots on the team.
Pass TEAM_FREE to broadcast to all bots regardless of team.
==================
*/
void BotCTFChatBroadcast( int excludeClient, int teamNum, const char *eventName ) {
	int i;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		bot_state_t *bs = botstates[i];
		if ( !bs || !bs->inuse || !bs->wiredBotsActive ) continue;
		if ( i == excludeClient ) continue;
		if ( teamNum != TEAM_FREE ) {
			if ( !g_entities[i].client ) continue;
			if ( g_entities[i].client->sess.sessionTeam != teamNum ) continue;
		}
		BotCTFChatEvent( i, eventName );
	}
}

/*
==================
BotChatTime
==================
*/
float BotChatTime(bot_state_t *bs) {
	//int cpm;

	if ( bs->wiredBotsActive ) return 0.0f;

	//cpm = trap_Characteristic_BInteger(bs->character, CHARACTERISTIC_CHAT_CPM, 1, 4000);

	return 2.0;	//(float) trap_BotChatLength(bs->cs) * 30 / cpm;
}

/*
==================
BotChatTest
==================
*/
void BotChatTest(bot_state_t *bs) {

	char name[32];
	char *weap;
	int num, i;

	if ( bs->wiredBotsActive ) return;

	num = trap_BotNumInitialChats(bs->cs, "game_enter");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "game_enter",
					EasyClientName(bs->client, name, 32),	// 0
					BotRandomOpponentName(bs),				// 1
					"[invalid var]",						// 2
					"[invalid var]",						// 3
					BotMapTitle(),							// 4
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "game_exit");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "game_exit",
					EasyClientName(bs->client, name, 32),	// 0
					BotRandomOpponentName(bs),				// 1
					"[invalid var]",						// 2
					"[invalid var]",						// 3
					BotMapTitle(),							// 4
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "level_start");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "level_start",
					EasyClientName(bs->client, name, 32),	// 0
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "level_end_victory");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "level_end_victory",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				BotFirstClientInRankings(),				// 2
				BotLastClientInRankings(),				// 3
				BotMapTitle(),							// 4
				NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "level_end_lose");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "level_end_lose",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				BotFirstClientInRankings(),				// 2
				BotLastClientInRankings(),				// 3
				BotMapTitle(),							// 4
				NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "level_end");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "level_end",
				EasyClientName(bs->client, name, 32),	// 0
				BotRandomOpponentName(bs),				// 1
				BotFirstClientInRankings(),				// 2
				BotLastClientInRankings(),				// 3
				BotMapTitle(),							// 4
				NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	EasyClientName(bs->lastkilledby, name, sizeof(name));
	num = trap_BotNumInitialChats(bs->cs, "death_drown");
	for (i = 0; i < num; i++)
	{
		//
		BotAI_BotInitialChat(bs, "death_drown", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_slime");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_slime", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_lava");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_lava", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_cratered");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_cratered", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_suicide");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_suicide", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_telefrag");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_telefrag", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_gauntlet");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_gauntlet",
				name,												// 0
				BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
				NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_rail");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_rail",
				name,												// 0
				BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
				NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_insult");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_insult",
					name,												// 0
					BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "death_praise");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "death_praise",
					name,												// 0
					BotWeaponNameForMeansOfDeath(bs->botdeathtype),		// 1
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	//
	EasyClientName(bs->lastkilledplayer, name, 32);
	//
	num = trap_BotNumInitialChats(bs->cs, "kill_gauntlet");
	for (i = 0; i < num; i++)
	{
		//
		BotAI_BotInitialChat(bs, "kill_gauntlet", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "kill_rail");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "kill_rail", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "kill_telefrag");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "kill_telefrag", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "kill_insult");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "kill_insult", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "kill_praise");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "kill_praise", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "enemy_suicide");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "enemy_suicide", name, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	ClientName(g_entities[bs->client].client->lasthurt_client, name, sizeof(name));
	weap = BotWeaponNameForMeansOfDeath(g_entities[bs->client].client->lasthurt_client);
	num = trap_BotNumInitialChats(bs->cs, "hit_talking");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "hit_talking", name, weap, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "hit_nodeath");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "hit_nodeath", name, weap, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "hit_nokill");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "hit_nokill", name, weap, NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	//
	if (bs->lastkilledplayer == bs->client) {
		strcpy(name, BotRandomOpponentName(bs));
	}
	else {
		EasyClientName(bs->lastkilledplayer, name, sizeof(name));
	}
	//
	num = trap_BotNumInitialChats(bs->cs, "random_misc");
	for (i = 0; i < num; i++)
	{
		//
		BotAI_BotInitialChat(bs, "random_misc",
					BotRandomOpponentName(bs),	// 0
					name,						// 1
					"[invalid var]",			// 2
					"[invalid var]",			// 3
					BotMapTitle(),				// 4
					BotRandomWeaponName(),		// 5
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
	num = trap_BotNumInitialChats(bs->cs, "random_insult");
	for (i = 0; i < num; i++)
	{
		BotAI_BotInitialChat(bs, "random_insult",
					BotRandomOpponentName(bs),	// 0
					name,						// 1
					"[invalid var]",			// 2
					"[invalid var]",			// 3
					BotMapTitle(),				// 4
					BotRandomWeaponName(),		// 5
					NULL);
		trap_BotEnterChat(bs->cs, 0, CHAT_ALL);
	}
}
