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
 * name:		ai_dmq3.c
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /MissionPack/code/game/ai_dmq3.c $
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
#include "ai_movement.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_team.h"
#include "ai_dodge.h"
#include "ai_aware.h"
#include "ai_itemtime.h"
#include "ai_weapsel.h"
#include "wired/bots/g_bot_scripts.h"
//
#include "chars.h"				//characteristics
#include "inv.h"				//indexes into the inventory
#include "syn.h"				//synonyms
#include "match.h"				//string matching types and vars

// for the voice chats
#include "../qcommon/menudef.h"

// from aasfile.h
#define AREACONTENTS_MOVER				1024
#define AREACONTENTS_MODELNUMSHIFT		24
#define AREACONTENTS_MAXMODELNUM		0xFF
#define AREACONTENTS_MODELNUM			(AREACONTENTS_MAXMODELNUM << AREACONTENTS_MODELNUMSHIFT)

#define IDEAL_ATTACKDIST			140

#define MAX_WAYPOINTS		128
//
bot_waypoint_t botai_waypoints[MAX_WAYPOINTS];
bot_waypoint_t *botai_freewaypoints;

//NOTE: not using a cvars which can be updated because the game should be reloaded anyway
int gametype;		//game type

vmCvar_t bot_grapple;
vmCvar_t bot_rocketjump;
vmCvar_t bot_fastchat;
vmCvar_t bot_nochat;
vmCvar_t bot_testrchat;
vmCvar_t bot_challenge;
vmCvar_t bot_predictobstacles;
vmCvar_t g_spSkill;
vmCvar_t sv_botDirectiveTTL;     /* minimum directive lifetime in ms; default 30000 */

extern vmCvar_t bot_developer;

vec3_t lastteleport_origin;		//last teleport event origin
float lastteleport_time;		//last teleport event time
int max_bspmodelindex;			//maximum BSP model index

//CTF flag goals
bot_goal_t ctf_redflag;
bot_goal_t ctf_blueflag;
bot_goal_t ctf_neutralflag;
bot_goal_t redobelisk;
bot_goal_t blueobelisk;
bot_goal_t neutralobelisk;

#define MAX_ALTROUTEGOALS		32

int altroutegoals_setup;
aas_altroutegoal_t red_altroutegoals[MAX_ALTROUTEGOALS];
int red_numaltroutegoals;
aas_altroutegoal_t blue_altroutegoals[MAX_ALTROUTEGOALS];
int blue_numaltroutegoals;


/*
==================
BotSetUserInfo
==================
*/
void BotSetUserInfo(bot_state_t *bs, const char *key, const char *value) {
	char userinfo[MAX_INFO_STRING];

	trap_GetUserinfo(bs->client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, key, value);
	trap_SetUserinfo(bs->client, userinfo);
	ClientUserinfoChanged( bs->client );
}

/*
==================
BotCTFCarryingFlag
==================
*/
int BotCTFCarryingFlag(bot_state_t *bs) {
	if (gametype != GT_CTF) return CTF_FLAG_NONE;

	if (bs->inventory[INVENTORY_REDFLAG] > 0) return CTF_FLAG_RED;
	else if (bs->inventory[INVENTORY_BLUEFLAG] > 0) return CTF_FLAG_BLUE;
	return CTF_FLAG_NONE;
}

/*
==================
BotTeam
==================
*/
int BotTeam(bot_state_t *bs) {

	if (bs->client < 0 || bs->client >= MAX_CLIENTS) {
		return qfalse;
	}

    if (level.clients[bs->client].sess.sessionTeam == TEAM_RED) {
		return TEAM_RED;
	} else if (level.clients[bs->client].sess.sessionTeam == TEAM_BLUE) {
		return TEAM_BLUE;
	}

	return TEAM_FREE;
}

/*
==================
BotOppositeTeam
==================
*/
int BotOppositeTeam(bot_state_t *bs) {
	switch(BotTeam(bs)) {
		case TEAM_RED: return TEAM_BLUE;
		case TEAM_BLUE: return TEAM_RED;
		default: return TEAM_FREE;
	}
}

/*
==================
BotEnemyFlag
==================
*/
bot_goal_t *BotEnemyFlag(bot_state_t *bs) {
	if (BotTeam(bs) == TEAM_RED) {
		return &ctf_blueflag;
	}
	else {
		return &ctf_redflag;
	}
}

/*
==================
BotTeamFlag
==================
*/
bot_goal_t *BotTeamFlag(bot_state_t *bs) {
	if (BotTeam(bs) == TEAM_RED) {
		return &ctf_redflag;
	}
	else {
		return &ctf_blueflag;
	}
}


/*
==================
EntityIsDead
==================
*/
qboolean EntityIsDead(aas_entityinfo_t *entinfo) {
	playerState_t ps;

	if (entinfo->number >= 0 && entinfo->number < MAX_CLIENTS) {
		//retrieve the current client state
		if (!BotAI_GetClientState(entinfo->number, &ps)) {
			return qfalse;
		}

		if (ps.pm_type != PM_NORMAL) return qtrue;
	}
	return qfalse;
}

/*
==================
EntityCarriesFlag
==================
*/
qboolean EntityCarriesFlag(aas_entityinfo_t *entinfo) {
	if ( entinfo->powerups & ( 1 << PW_REDFLAG ) )
		return qtrue;
	if ( entinfo->powerups & ( 1 << PW_BLUEFLAG ) )
		return qtrue;
	if ( entinfo->powerups & ( 1 << PW_NEUTRALFLAG ) )
		return qtrue;
	return qfalse;
}

/*
==================
EntityIsInvisible
==================
*/
qboolean EntityIsInvisible(aas_entityinfo_t *entinfo) {
	// the flag is always visible
	if (EntityCarriesFlag(entinfo)) {
		return qfalse;
	}
	if (entinfo->powerups & (1 << PW_INVIS)) {
		return qtrue;
	}
	return qfalse;
}

/*
==================
EntityIsShooting
==================
*/
qboolean EntityIsShooting(aas_entityinfo_t *entinfo) {
	if (entinfo->flags & EF_FIRING_PRI || entinfo->flags & EF_FIRING_SEC) {
		return qtrue;
	}

	return qfalse;
}

/*
==================
EntityIsChatting
==================
*/
qboolean EntityIsChatting(aas_entityinfo_t *entinfo) {
	if (entinfo->flags & EF_TALK) {
		return qtrue;
	}
	return qfalse;
}

/*
==================
EntityHasQuad
==================
*/
qboolean EntityHasQuad(aas_entityinfo_t *entinfo) {
	if (entinfo->powerups & (1 << PW_QUAD)) {
		return qtrue;
	}
	return qfalse;
}

/*
==================
EntityHasBerserk
==================
*/
qboolean EntityHasBerserk(aas_entityinfo_t *entinfo) {
	if (entinfo->powerups & (1 << PW_BERSERK)) {
		return qtrue;
	}
	return qfalse;
}

/*
==================
EntityHasKamikze
==================
*/
qboolean EntityHasKamikaze(aas_entityinfo_t *entinfo) {
	if (entinfo->flags & EF_KAMIKAZE) {
		return qtrue;
	}
	return qfalse;
}

/*
==================
Bot1FCTFCarryingFlag
==================
*/
int Bot1FCTFCarryingFlag(bot_state_t *bs) {
	if (gametype != GT_1FCTF) return qfalse;

	if (bs->inventory[INVENTORY_NEUTRALFLAG] > 0) return qtrue;
	return qfalse;
}

#if FEAT_HARVESTER
/*
==================
EntityCarriesCubes
==================
*/
qboolean EntityCarriesCubes(aas_entityinfo_t *entinfo) {
	entityState_t state;

	if (gametype != GT_HARVESTER)
		return qfalse;
	//FIXME: get this info from the aas_entityinfo_t ?
	BotAI_GetEntityState(entinfo->number, &state);
	if (state.generic1 > 0)
		return qtrue;
	return qfalse;
}

/*
==================
BotHarvesterCarryingCubes
==================
*/
int BotHarvesterCarryingCubes(bot_state_t *bs) {
	if (gametype != GT_HARVESTER) return qfalse;

	if (bs->inventory[INVENTORY_REDCUBE] > 0) return qtrue;
	if (bs->inventory[INVENTORY_BLUECUBE] > 0) return qtrue;
	return qfalse;
}
#endif

/*
==================
BotRememberLastOrderedTask
==================
*/
void BotRememberLastOrderedTask(bot_state_t *bs) {
	if (!bs->ordered) {
		return;
	}
	bs->lastgoal_decisionmaker = bs->decisionmaker;
	bs->lastgoal_ltgtype = bs->ltgtype;
	memcpy(&bs->lastgoal_teamgoal, &bs->teamgoal, sizeof(bot_goal_t));
	bs->lastgoal_teammate = bs->teammate;
}

/*
==================
BotSetTeamStatus
==================
*/
void BotSetTeamStatus(bot_state_t *bs) {
	int teamtask;
	aas_entityinfo_t entinfo;

	teamtask = TEAMTASK_PATROL;

	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
			break;
		case LTG_TEAMACCOMPANY:
			BotEntityInfo(bs->teammate, &entinfo);
			if ( ( (gametype == GT_CTF || gametype == GT_1FCTF) && EntityCarriesFlag(&entinfo))
#if FEAT_HARVESTER
				|| ( gametype == GT_HARVESTER && EntityCarriesCubes(&entinfo))
#endif
				) {
				teamtask = TEAMTASK_ESCORT;
			}
			else {
				teamtask = TEAMTASK_FOLLOW;
			}
			break;
		case LTG_DEFENDKEYAREA:
			teamtask = TEAMTASK_DEFENSE;
			break;
		case LTG_GETFLAG:
			teamtask = TEAMTASK_OFFENSE;
			break;
		case LTG_RUSHBASE:
			teamtask = TEAMTASK_DEFENSE;
			break;
		case LTG_RETURNFLAG:
			teamtask = TEAMTASK_RETRIEVE;
			break;
		case LTG_CAMP:
		case LTG_CAMPORDER:
			teamtask = TEAMTASK_CAMP;
			break;
		case LTG_PATROL:
			teamtask = TEAMTASK_PATROL;
			break;
		case LTG_GETITEM:
			teamtask = TEAMTASK_PATROL;
			break;
		case LTG_KILL:
			teamtask = TEAMTASK_PATROL;
			break;
		case LTG_HARVEST:
			teamtask = TEAMTASK_OFFENSE;
			break;
		case LTG_ATTACKENEMYBASE:
			teamtask = TEAMTASK_OFFENSE;
			break;
		default:
			teamtask = TEAMTASK_PATROL;
			break;
	}
	BotSetUserInfo(bs, "teamtask", va("%d", teamtask));
}

/*
==================
BotSetLastOrderedTask
==================
*/
int BotSetLastOrderedTask(bot_state_t *bs) {

	if (gametype == GT_CTF) {
		// don't go back to returning the flag if it's at the base
		if ( bs->lastgoal_ltgtype == LTG_RETURNFLAG ) {
			if ( BotTeam(bs) == TEAM_RED ) {
				if ( bs->redflagstatus == 0 ) {
					bs->lastgoal_ltgtype = 0;
				}
			}
			else {
				if ( bs->blueflagstatus == 0 ) {
					bs->lastgoal_ltgtype = 0;
				}
			}
		}
	}

	if ( bs->lastgoal_ltgtype ) {
		bs->decisionmaker = bs->lastgoal_decisionmaker;
		bs->ordered = qtrue;
		bs->ltgtype = bs->lastgoal_ltgtype;
		memcpy(&bs->teamgoal, &bs->lastgoal_teamgoal, sizeof(bot_goal_t));
		bs->teammate = bs->lastgoal_teammate;
		bs->teamgoal_time = FloatTime() + 300;
		BotSetTeamStatus(bs);
		//
		if ( gametype == GT_CTF ) {
			if ( bs->ltgtype == LTG_GETFLAG ) {
				bot_goal_t *tb, *eb;
				int tt, et;

				tb = BotTeamFlag(bs);
				eb = BotEnemyFlag(bs);
				tt = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, tb->areanum, TFL_DEFAULT);
				et = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, eb->areanum, TFL_DEFAULT);
				// if the travel time towards the enemy base is larger than towards our base
				if (et > tt) {
					//get an alternative route goal towards the enemy base
					BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
				}
			}
		}
		return qtrue;
	}
	return qfalse;
}

/*
==================
BotRefuseOrder
==================
*/
void BotRefuseOrder(bot_state_t *bs) {
	if (!bs->ordered)
		return;
	// if the bot was ordered to do something
	if ( bs->order_time && bs->order_time > FloatTime() - 10 ) {
		trap_EA_Action(bs->client, ACTION_NEGATIVE);
		BotVoiceChat(bs, bs->decisionmaker, VOICECHAT_NO);
		bs->order_time = 0;
	}
}

/*
==================
BotCTFSeekGoals
==================
*/
void BotCTFSeekGoals(bot_state_t *bs) {
	float rnd, l1, l2;
	int flagstatus, c;
	vec3_t dir;
	aas_entityinfo_t entinfo;

	//when carrying a flag in ctf the bot should rush to the base
	if (BotCTFCarryingFlag(bs)) {
		//if not already rushing to the base
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			//
			switch(BotTeam(bs)) {
				case TEAM_RED: VectorSubtract(bs->origin, ctf_blueflag.origin, dir); break;
				case TEAM_BLUE: VectorSubtract(bs->origin, ctf_redflag.origin, dir); break;
				default: VectorSet(dir, 999, 999, 999); break;
			}
			// if the bot picked up the flag very close to the enemy base
			if ( VectorLength(dir) < 128 ) {
				// get an alternative route goal through the enemy base
				BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			} else {
				// don't use any alt route goal, just get the hell out of the base
				bs->altroutegoal.areanum = 0;
			}
			BotSetUserInfo(bs, "teamtask", va("%d", TEAMTASK_OFFENSE));
			BotVoiceChat(bs, -1, VOICECHAT_IHAVEFLAG);
		}
		else if (bs->rushbaseaway_time > FloatTime()) {
			if (BotTeam(bs) == TEAM_RED) flagstatus = bs->redflagstatus;
			else flagstatus = bs->blueflagstatus;
			//if the flag is back
			if (flagstatus == 0) {
				bs->rushbaseaway_time = 0;
			}
		}
		return;
	}
	// if the bot decided to follow someone
	if ( bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered ) {
		// if the team mate being accompanied no longer carries the flag
		BotEntityInfo(bs->teammate, &entinfo);
		if (!EntityCarriesFlag(&entinfo)) {
			bs->ltgtype = 0;
		}
	}
	//
	if (BotTeam(bs) == TEAM_RED) flagstatus = bs->redflagstatus * 2 + bs->blueflagstatus;
	else flagstatus = bs->blueflagstatus * 2 + bs->redflagstatus;
	//if our team has the enemy flag and our flag is at the base
	if (flagstatus == 1) {
		//
		if (bs->owndecision_time < FloatTime()) {
			//if Not defending the base already
			if (!(bs->ltgtype == LTG_DEFENDKEYAREA &&
					(bs->teamgoal.number == ctf_redflag.number ||
					bs->teamgoal.number == ctf_blueflag.number))) {
				//if there is a visible team mate flag carrier
				c = BotTeamFlagCarrierVisible(bs);
				if (c >= 0 &&
						// and not already following the team mate flag carrier
						(bs->ltgtype != LTG_TEAMACCOMPANY || bs->teammate != c)) {
					//
					BotRefuseOrder(bs);
					//follow the flag carrier
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					//the team mate
					bs->teammate = c;
					//last time the team mate was visible
					bs->teammatevisible_time = FloatTime();
					//no message
					bs->teammessage_time = 0;
					//no arrive message
					bs->arrive_time = 1;
					//
					BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
					//get the team goal time
					bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
					bs->ltgtype = LTG_TEAMACCOMPANY;
					bs->formation_dist = 3.5 * 32;		//3.5 meter
					BotSetTeamStatus(bs);
					bs->owndecision_time = FloatTime() + 5;
				}
			}
		}
		return;
	}
	//if the enemy has our flag
	else if (flagstatus == 2) {
		//
		if (bs->owndecision_time < FloatTime()) {
			//if enemy flag carrier is visible
			c = BotEnemyFlagCarrierVisible(bs);
			if (c >= 0) {
				//FIXME: fight enemy flag carrier
			}
			//if not already doing something important
			if (bs->ltgtype != LTG_GETFLAG &&
				bs->ltgtype != LTG_RETURNFLAG &&
				bs->ltgtype != LTG_TEAMHELP &&
				bs->ltgtype != LTG_TEAMACCOMPANY &&
				bs->ltgtype != LTG_CAMPORDER &&
				bs->ltgtype != LTG_PATROL &&
				bs->ltgtype != LTG_GETITEM) {

				BotRefuseOrder(bs);
				bs->decisionmaker = bs->client;
				bs->ordered = qfalse;
				//
				if (random() < 0.5) {
					//go for the enemy flag
					bs->ltgtype = LTG_GETFLAG;
				}
				else {
					bs->ltgtype = LTG_RETURNFLAG;
				}
				//no team message
				bs->teammessage_time = 0;
				//set the time the bot will stop getting the flag
				bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
				//get an alternative route goal towards the enemy base
				BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
				//
				BotSetTeamStatus(bs);
				bs->owndecision_time = FloatTime() + 5;
			}
		}
		return;
	}
	//if both flags Not at their bases
	else if (flagstatus == 3) {
		//
		if (bs->owndecision_time < FloatTime()) {
			// if not trying to return the flag and not following the team flag carrier
			if ( bs->ltgtype != LTG_RETURNFLAG && bs->ltgtype != LTG_TEAMACCOMPANY ) {
				//
				c = BotTeamFlagCarrierVisible(bs);
				// if there is a visible team mate flag carrier
				if (c >= 0) {
					BotRefuseOrder(bs);
					//follow the flag carrier
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					//the team mate
					bs->teammate = c;
					//last time the team mate was visible
					bs->teammatevisible_time = FloatTime();
					//no message
					bs->teammessage_time = 0;
					//no arrive message
					bs->arrive_time = 1;
					//
					BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
					//get the team goal time
					bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
					bs->ltgtype = LTG_TEAMACCOMPANY;
					bs->formation_dist = 3.5 * 32;		//3.5 meter
					//
					BotSetTeamStatus(bs);
					bs->owndecision_time = FloatTime() + 5;
				}
				else {
					BotRefuseOrder(bs);
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					//get the enemy flag
					bs->teammessage_time = FloatTime() + 2 * random();
					//get the flag
					bs->ltgtype = LTG_RETURNFLAG;
					//set the time the bot will stop getting the flag
					bs->teamgoal_time = FloatTime() + CTF_RETURNFLAG_TIME;
					//get an alternative route goal towards the enemy base
					BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
					//
					BotSetTeamStatus(bs);
					bs->owndecision_time = FloatTime() + 5;
				}
			}
		}
		return;
	}
	// don't just do something wait for the bot team leader to give orders
	if (BotTeamLeader(bs)) {
		return;
	}
	// if the bot is ordered to do something
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	// if the bot decided to do something on its own and has a last ordered goal
	if ( !bs->ordered && bs->lastgoal_ltgtype ) {
		bs->ltgtype = 0;
	}
	//if already a CTF or team goal
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_RETURNFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	//
	if (BotSetLastOrderedTask(bs))
		return;
	//
	if (bs->owndecision_time > FloatTime())
		return;;
	//if the bot is roaming
	if (bs->ctfroam_time > FloatTime())
		return;
	//if the bot has enough aggression to decide what to do
	if (BotAggression(bs) < 50)
		return;
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//
	if (bs->directives.preference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->directives.preference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	//get the flag or defend the base
	rnd = random();
	if (rnd < l1 && ctf_redflag.areanum && ctf_blueflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		bs->ltgtype = LTG_GETFLAG;
		//set the time the bot will stop getting the flag
		bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
		//get an alternative route goal towards the enemy base
		BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
		BotSetTeamStatus(bs);
	}
	else if (rnd < l2 && ctf_redflag.areanum && ctf_blueflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		//
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
		//set the ltg type
		bs->ltgtype = LTG_DEFENDKEYAREA;
		//set the time the bot stops defending the base
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		//set the time the bot will stop roaming
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
	bs->owndecision_time = FloatTime() + 5;
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif //DEBUG
}

/*
==================
BotCTFRetreatGoals
==================
*/
void BotCTFRetreatGoals(bot_state_t *bs) {
	//when carrying a flag in ctf the bot should rush to the base
	if (BotCTFCarryingFlag(bs)) {
		//if not already rushing to the base
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotSetTeamStatus(bs);
		}
	}
}

/*
==================
Bot1FCTFSeekGoals
==================
*/
void Bot1FCTFSeekGoals(bot_state_t *bs) {
	aas_entityinfo_t entinfo;
	float rnd, l1, l2;
	int c;

	//when carrying a flag in ctf the bot should rush to the base
	if (Bot1FCTFCarryingFlag(bs)) {
		//if not already rushing to the base
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			//get an alternative route goal towards the enemy base
			BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			//
			BotSetTeamStatus(bs);
			BotVoiceChat(bs, -1, VOICECHAT_IHAVEFLAG);
		}
		return;
	}
	// if the bot decided to follow someone
	if ( bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered ) {
		// if the team mate being accompanied no longer carries the flag
		BotEntityInfo(bs->teammate, &entinfo);
		if (!EntityCarriesFlag(&entinfo)) {
			bs->ltgtype = 0;
		}
	}
	//our team has the flag
	if (bs->neutralflagstatus == 1) {
		if (bs->owndecision_time < FloatTime()) {
			// if not already following someone
			if (bs->ltgtype != LTG_TEAMACCOMPANY) {
				//if there is a visible team mate flag carrier
				c = BotTeamFlagCarrierVisible(bs);
				if (c >= 0) {
					BotRefuseOrder(bs);
					//follow the flag carrier
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					//the team mate
					bs->teammate = c;
					//last time the team mate was visible
					bs->teammatevisible_time = FloatTime();
					//no message
					bs->teammessage_time = 0;
					//no arrive message
					bs->arrive_time = 1;
					//
					BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
					//get the team goal time
					bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
					bs->ltgtype = LTG_TEAMACCOMPANY;
					bs->formation_dist = 3.5 * 32;		//3.5 meter
					BotSetTeamStatus(bs);
					bs->owndecision_time = FloatTime() + 5;
					return;
				}
			}
			//if already a CTF or team goal
			if (bs->ltgtype == LTG_TEAMHELP ||
					bs->ltgtype == LTG_TEAMACCOMPANY ||
					bs->ltgtype == LTG_DEFENDKEYAREA ||
					bs->ltgtype == LTG_GETFLAG ||
					bs->ltgtype == LTG_RUSHBASE ||
					bs->ltgtype == LTG_CAMPORDER ||
					bs->ltgtype == LTG_PATROL ||
					bs->ltgtype == LTG_ATTACKENEMYBASE ||
					bs->ltgtype == LTG_GETITEM ||
					bs->ltgtype == LTG_MAKELOVE_UNDER ||
					bs->ltgtype == LTG_MAKELOVE_ONTOP) {
				return;
			}
			//if not already attacking the enemy base
			if (bs->ltgtype != LTG_ATTACKENEMYBASE) {
				BotRefuseOrder(bs);
				bs->decisionmaker = bs->client;
				bs->ordered = qfalse;
				//
				if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
				else memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
				//set the ltg type
				bs->ltgtype = LTG_ATTACKENEMYBASE;
				//set the time the bot will stop getting the flag
				bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
				BotSetTeamStatus(bs);
				bs->owndecision_time = FloatTime() + 5;
			}
		}
		return;
	}
	//enemy team has the flag
	else if (bs->neutralflagstatus == 2) {
		if (bs->owndecision_time < FloatTime()) {
			c = BotEnemyFlagCarrierVisible(bs);
			if (c >= 0) {
				//FIXME: attack enemy flag carrier
			}
			//if already a CTF or team goal
			if (bs->ltgtype == LTG_TEAMHELP ||
					bs->ltgtype == LTG_TEAMACCOMPANY ||
					bs->ltgtype == LTG_CAMPORDER ||
					bs->ltgtype == LTG_PATROL ||
					bs->ltgtype == LTG_GETITEM) {
				return;
			}
			// if not already defending the base
			if (bs->ltgtype != LTG_DEFENDKEYAREA) {
				BotRefuseOrder(bs);
				bs->decisionmaker = bs->client;
				bs->ordered = qfalse;
				//
				if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
				else memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
				//set the ltg type
				bs->ltgtype = LTG_DEFENDKEYAREA;
				//set the time the bot stops defending the base
				bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
				bs->defendaway_time = 0;
				BotSetTeamStatus(bs);
				bs->owndecision_time = FloatTime() + 5;
			}
		}
		return;
	}
	// don't just do something wait for the bot team leader to give orders
	if (BotTeamLeader(bs)) {
		return;
	}
	// if the bot is ordered to do something
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	// if the bot decided to do something on its own and has a last ordered goal
	if ( !bs->ordered && bs->lastgoal_ltgtype ) {
		bs->ltgtype = 0;
	}
	//if already a CTF or team goal
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_RETURNFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_ATTACKENEMYBASE ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	//
	if (BotSetLastOrderedTask(bs))
		return;
	//
	if (bs->owndecision_time > FloatTime())
		return;;
	//if the bot is roaming
	if (bs->ctfroam_time > FloatTime())
		return;
	//if the bot has enough aggression to decide what to do
	if (BotAggression(bs) < 50)
		return;
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//
	if (bs->directives.preference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->directives.preference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	//get the flag or defend the base
	rnd = random();
	if (rnd < l1 && ctf_neutralflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		bs->ltgtype = LTG_GETFLAG;
		//set the time the bot will stop getting the flag
		bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
		BotSetTeamStatus(bs);
	}
	else if (rnd < l2 && ctf_redflag.areanum && ctf_blueflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		//
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
		//set the ltg type
		bs->ltgtype = LTG_DEFENDKEYAREA;
		//set the time the bot stops defending the base
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		//set the time the bot will stop roaming
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
	bs->owndecision_time = FloatTime() + 5;
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif //DEBUG
}

/*
==================
Bot1FCTFRetreatGoals
==================
*/
void Bot1FCTFRetreatGoals(bot_state_t *bs) {
	//when carrying a flag in ctf the bot should rush to the enemy base
	if (Bot1FCTFCarryingFlag(bs)) {
		//if not already rushing to the base
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			//get an alternative route goal towards the enemy base
			BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			BotSetTeamStatus(bs);
		}
	}
}

#if FEAT_OVERLOAD
/*
==================
BotObeliskSeekGoals
==================
*/
void BotObeliskSeekGoals(bot_state_t *bs) {
	float rnd, l1, l2;

	// don't just do something wait for the bot team leader to give orders
	if (BotTeamLeader(bs)) {
		return;
	}
	// if the bot is ordered to do something
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	//if already a team goal
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_RETURNFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_ATTACKENEMYBASE ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	//
	if (BotSetLastOrderedTask(bs))
		return;
	//if the bot is roaming
	if (bs->ctfroam_time > FloatTime())
		return;
	//if the bot has enough aggression to decide what to do
	if (BotAggression(bs) < 50)
		return;
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//
	if (bs->directives.preference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->directives.preference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	//get the flag or defend the base
	rnd = random();
	if (rnd < l1 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		//
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
		//set the ltg type
		bs->ltgtype = LTG_ATTACKENEMYBASE;
		//set the time the bot will stop attacking the enemy base
		bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
		//get an alternate route goal towards the enemy base
		BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
		BotSetTeamStatus(bs);
	}
	else if (rnd < l2 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		//
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
		//set the ltg type
		bs->ltgtype = LTG_DEFENDKEYAREA;
		//set the time the bot stops defending the base
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		//set the time the bot will stop roaming
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
}

/*
==================
BotObeliskRetreatGoals
==================
*/
void BotObeliskRetreatGoals(bot_state_t *bs) {
	//nothing special
}
#endif

#if FEAT_HARVESTER
/*
==================
BotGoHarvest
==================
*/
void BotGoHarvest(bot_state_t *bs) {
	//
	if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
	else memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
	//set the ltg type
	bs->ltgtype = LTG_HARVEST;
	//set the time the bot will stop harvesting
	bs->teamgoal_time = FloatTime() + TEAM_HARVEST_TIME;
	bs->harvestaway_time = 0;
	BotSetTeamStatus(bs);
}

/*
==================
BotHarvesterSeekGoals
==================
*/
void BotHarvesterSeekGoals(bot_state_t *bs) {
	aas_entityinfo_t entinfo;
	float rnd, l1, l2;
	int c;

	//when carrying cubes in harvester the bot should rush to the base
	if (BotHarvesterCarryingCubes(bs)) {
		//if not already rushing to the base
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			//get an alternative route goal towards the enemy base
			BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			//
			BotSetTeamStatus(bs);
		}
		return;
	}
	// don't just do something wait for the bot team leader to give orders
	if (BotTeamLeader(bs)) {
		return;
	}
	// if the bot decided to follow someone
	if ( bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered ) {
		// if the team mate being accompanied no longer carries the flag
		BotEntityInfo(bs->teammate, &entinfo);
		if (!EntityCarriesCubes(&entinfo)) {
			bs->ltgtype = 0;
		}
	}
	// if the bot is ordered to do something
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	//if not yet doing something
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_ATTACKENEMYBASE ||
			bs->ltgtype == LTG_HARVEST ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	//
	if (BotSetLastOrderedTask(bs))
		return;
	//if the bot is roaming
	if (bs->ctfroam_time > FloatTime())
		return;
	//if the bot has enough aggression to decide what to do
	if (BotAggression(bs) < 50)
		return;
	//set the time to send a message to the team mates
	bs->teammessage_time = FloatTime() + 2 * random();
	//
	c = BotEnemyCubeCarrierVisible(bs);
	if (c >= 0) {
		//FIXME: attack enemy cube carrier
	}
	if (bs->ltgtype != LTG_TEAMACCOMPANY) {
		//if there is a visible team mate carrying cubes
		c = BotTeamCubeCarrierVisible(bs);
		if (c >= 0) {
			//follow the team mate carrying cubes
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			//the team mate
			bs->teammate = c;
			//last time the team mate was visible
			bs->teammatevisible_time = FloatTime();
			//no message
			bs->teammessage_time = 0;
			//no arrive message
			bs->arrive_time = 1;
			//
			BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
			//get the team goal time
			bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
			bs->ltgtype = LTG_TEAMACCOMPANY;
			bs->formation_dist = 3.5 * 32;		//3.5 meter
			BotSetTeamStatus(bs);
			return;
		}
	}
	//
	if (bs->directives.preference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->directives.preference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	//
	rnd = random();
	if (rnd < l1 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		BotGoHarvest(bs);
	}
	else if (rnd < l2 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		//
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
		//set the ltg type
		bs->ltgtype = LTG_DEFENDKEYAREA;
		//set the time the bot stops defending the base
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		//set the time the bot will stop roaming
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
}

/*
==================
BotHarvesterRetreatGoals
==================
*/
void BotHarvesterRetreatGoals(bot_state_t *bs) {
	//when carrying cubes in harvester the bot should rush to the base
	if (BotHarvesterCarryingCubes(bs)) {
		//if not already rushing to the base
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotSetTeamStatus(bs);
		}
		return;
	}
}
#endif

/*
==================
BotKingOfTheHillSeekGoals
==================
*/
void BotKingOfTheHillSeekGoals(bot_state_t *bs) {
}

/*
==================
BotTeamGoals
==================
*/
void BotTeamGoals(bot_state_t *bs, int retreat) {

	if ( retreat ) {
		if (gametype == GT_CTF) {
			BotCTFRetreatGoals(bs);
		}
		else if (gametype == GT_1FCTF) {
			Bot1FCTFRetreatGoals(bs);
		}
#if FEAT_OVERLOAD
		else if (gametype == GT_OBELISK) {
			BotObeliskRetreatGoals(bs);
		}
#endif
#if FEAT_HARVESTER
		else if (gametype == GT_HARVESTER) {
			BotHarvesterRetreatGoals(bs);
		}
#endif
	}
	else {
		if (gametype == GT_CTF) {
			//decide what to do in CTF mode
			BotCTFSeekGoals(bs);
		}
		else if (gametype == GT_1FCTF) {
			Bot1FCTFSeekGoals(bs);
		}
#if FEAT_OVERLOAD
		else if (gametype == GT_OBELISK) {
			BotObeliskSeekGoals(bs);
		}
#endif
#if FEAT_HARVESTER
		else if (gametype == GT_HARVESTER) {
			BotHarvesterSeekGoals(bs);
		}
#endif

        if (gametype == GT_KINGOFTHEHILL) {
            //decide what to do in CTF mode
            BotKingOfTheHillSeekGoals(bs);
        }
	}
	// reset the order time which is used to see if
	// we decided to refuse an order
	bs->order_time = 0;
}

/*
==================
BotPointAreaNum
==================
*/
int BotPointAreaNum(vec3_t origin) {
	int areanum, numareas, areas[10];
	vec3_t end;

	areanum = trap_AAS_PointAreaNum(origin);
	if (areanum) return areanum;
	VectorCopy(origin, end);
	end[2] += 10;
	numareas = trap_AAS_TraceAreas(origin, end, areas, NULL, 10);
	if (numareas > 0) return areas[0];
	return 0;
}

/*
==================
ClientName
==================
*/
char *ClientName(int client, char *name, int size) {
	char buf[MAX_INFO_STRING];

	if (client < 0 || client >= MAX_CLIENTS) {
		BotAI_Print(PRT_ERROR, "ClientName: client out of range\n");
		return "[client out of range]";
	}
	trap_GetConfigstring(CS_PLAYERS+client, buf, sizeof(buf));
	Q_strncpyz(name, Info_ValueForKey(buf, "n"), size);
	Q_CleanStr( name );
	return name;
}

/*
==================
ClientSkin
==================
*/
char *ClientSkin(int client, char *skin, int size) {
	char buf[MAX_INFO_STRING];

	if (client < 0 || client >= MAX_CLIENTS) {
		BotAI_Print(PRT_ERROR, "ClientSkin: client out of range\n");
		return "[client out of range]";
	}
	trap_GetConfigstring(CS_PLAYERS+client, buf, sizeof(buf));
	Q_strncpyz(skin, Info_ValueForKey(buf, "model"), size);
	return skin;
}

/*
==================
ClientFromName
==================
*/
int ClientFromName(char *name) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < level.maxclients; i++) {
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		Q_CleanStr( buf );
		if (!Q_stricmp(Info_ValueForKey(buf, "n"), name)) return i;
	}
	return -1;
}

/*
==================
ClientOnSameTeamFromName
==================
*/
int ClientOnSameTeamFromName(bot_state_t *bs, char *name) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < level.maxclients; i++) {
		if (!BotSameTeam(bs, i))
			continue;
		trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
		Q_CleanStr( buf );
		if (!Q_stricmp(Info_ValueForKey(buf, "n"), name)) return i;
	}
	return -1;
}

/*
==================
stristr
==================
*/
char *stristr(char *str, char *charset) {
	int i;

	while(*str) {
		for (i = 0; charset[i] && str[i]; i++) {
			if (toupper(charset[i]) != toupper(str[i])) break;
		}
		if (!charset[i]) return str;
		str++;
	}
	return NULL;
}

/*
==================
EasyClientName
==================
*/
char *EasyClientName(int client, char *buf, int size) {
	int i;
	char *str1, *str2, *ptr, c;
	char name[128] = {0};

	ClientName(client, name, sizeof(name));
	
	for (i = 0; name[i]; i++) name[i] &= 127;
	//remove all spaces
	for (ptr = strstr(name, " "); ptr; ptr = strstr(name, " ")) {
		memmove(ptr, ptr+1, strlen(ptr+1)+1);
	}
	//check for [x] and ]x[ clan names
	str1 = strstr(name, "[");
	str2 = strstr(name, "]");
	if (str1 && str2) {
		if (str2 > str1) memmove(str1, str2+1, strlen(str2+1)+1);
		else memmove(str2, str1+1, strlen(str1+1)+1);
	}
	//remove Mr prefix
	if ((name[0] == 'm' || name[0] == 'M') &&
			(name[1] == 'r' || name[1] == 'R')) {
		memmove(name, name+2, strlen(name+2)+1);
	}
	//only allow lower case alphabet characters
	ptr = name;
	while(*ptr) {
		c = *ptr;
		if ((c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '_') {
			ptr++;
		}
		else if (c >= 'A' && c <= 'Z') {
			*ptr += 'a' - 'A';
			ptr++;
		}
		else {
			memmove(ptr, ptr+1, strlen(ptr + 1)+1);
		}
	}
	Q_strncpyz(buf, name, size);
	return buf;
}

/*
==================
BotSynonymContext
==================
*/
int BotSynonymContext(bot_state_t *bs) {
	int context;

	context = CONTEXT_NORMAL|CONTEXT_NEARBYITEM|CONTEXT_NAMES;
	//
	if (gametype == GT_CTF || gametype == GT_1FCTF) {
		if (BotTeam(bs) == TEAM_RED) context |= CONTEXT_CTFREDTEAM;
		else context |= CONTEXT_CTFBLUETEAM;
	}
#if FEAT_OVERLOAD
	else if (gametype == GT_OBELISK) {
		if (BotTeam(bs) == TEAM_RED) context |= CONTEXT_OBELISKREDTEAM;
		else context |= CONTEXT_OBELISKBLUETEAM;
	}
#endif
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		if (BotTeam(bs) == TEAM_RED) context |= CONTEXT_HARVESTERREDTEAM;
		else context |= CONTEXT_HARVESTERBLUETEAM;
	}
#endif
	return context;
}

/*
==================
BotChooseWeapon
==================
*/
void BotChooseWeapon(bot_state_t *bs) {
	int newweaponnum;
	// Save the final weapon chosen last frame BEFORE DPS analysis can clobber bs->weaponnum.
	// BotChooseWeaponDPS writes bs->weaponnum internally, so comparing bs->weaponnum after
	// DPS would always differ from the Lua pick, stamping weaponchange_time every frame.
	int prevweaponnum = bs->weaponnum;

	if (bs->cur_ps.weaponstate == WEAPON_RAISING ||
			bs->cur_ps.weaponstate == WEAPON_DROPPING) {
		trap_EA_SelectWeapon(bs->client, bs->weaponnum);
	}
	else {
		BotAccuracyUpdate(bs);
		BotChooseWeaponDPS(bs);
		newweaponnum = bs->weaponnum;
		if ( bs->wiredBotsActive ) {
			newweaponnum = WiredBots_ChooseWeapon( bs, newweaponnum );
		}
		if (prevweaponnum != newweaponnum) bs->weaponchange_time = FloatTime();
		bs->weaponnum = newweaponnum;
		//BotAI_Print(PRT_MESSAGE, "bs->weaponnum = %d\n", bs->weaponnum);
		trap_EA_SelectWeapon(bs->client, bs->weaponnum);
	}
}

/*
==================
BotSetupForMovement
==================
*/
void BotSetupForMovement(bot_state_t *bs) {
	bot_initmove_t initmove;

	memset(&initmove, 0, sizeof(bot_initmove_t));
	VectorCopy(bs->cur_ps.origin, initmove.origin);
	VectorCopy(bs->cur_ps.velocity, initmove.velocity);
	VectorClear(initmove.viewoffset);
	initmove.viewoffset[2] += bs->cur_ps.viewheight;
	initmove.entitynum = bs->entitynum;
	initmove.client = bs->client;
	initmove.thinktime = bs->thinktime;
	//set the onground flag
	if (bs->cur_ps.groundEntityNum != ENTITYNUM_NONE) initmove.or_moveflags |= MFL_ONGROUND;
	//set the teleported flag
	if ((bs->cur_ps.pm_flags & PMF_TIME_KNOCKBACK) && (bs->cur_ps.pm_time > 0)) {
		initmove.or_moveflags |= MFL_TELEPORTED;
	}
	//set the waterjump flag
	if ((bs->cur_ps.pm_flags & PMF_TIME_WATERJUMP) && (bs->cur_ps.pm_time > 0)) {
		initmove.or_moveflags |= MFL_WATERJUMP;
	}
	//set presence type
	if (bs->cur_ps.pm_flags & PMF_DUCKED) initmove.presencetype = PRESENCE_CROUCH;
	else initmove.presencetype = PRESENCE_NORMAL;
	//
	if (bs->walker > 0.5) initmove.or_moveflags |= MFL_WALK;
	//
	VectorCopy(bs->viewangles, initmove.viewangles);
	//
	trap_BotInitMoveState(bs->ms, &initmove);
}

/*
==================
BotCheckItemPickup
==================
*/
void BotCheckItemPickup(bot_state_t *bs, int *oldinventory) {
	int offence, leader;

	if (gametype <= GT_TDM)
		return;

	offence = -1;
	// go into offence if picked up the kamikaze or invulnerability
	if (!oldinventory[INVENTORY_KAMIKAZE] && bs->inventory[INVENTORY_KAMIKAZE] >= 1) {
		offence = qtrue;
	}
	if (!oldinventory[INVENTORY_INVULNERABILITY] && bs->inventory[INVENTORY_INVULNERABILITY] >= 1) {
		offence = qtrue;
	}

	if (offence >= 0) {
		leader = ClientFromName(bs->directives.teamleader);
		if (offence) {
			if (!(bs->directives.preference & TEAMTP_ATTACKER)) {
				// if we have a bot team leader
				if (BotTeamLeader(bs)) {
					// tell the leader we want to be on offence
					BotVoiceChat(bs, leader, VOICECHAT_WANTONOFFENSE);
					//BotAI_BotInitialChat(bs, "wantoffence", NULL);
					//trap_BotEnterChat(bs->cs, leader, CHAT_TELL);
				}
				else if (g_spSkill.integer <= 3) {
					if ( bs->ltgtype != LTG_GETFLAG &&
						 bs->ltgtype != LTG_ATTACKENEMYBASE &&
						 bs->ltgtype != LTG_HARVEST ) {
						//
						if ((gametype != GT_CTF || (bs->redflagstatus == 0 && bs->blueflagstatus == 0)) &&
							(gametype != GT_1FCTF || bs->neutralflagstatus == 0) ) {
							// tell the leader we want to be on offence
							BotVoiceChat(bs, leader, VOICECHAT_WANTONOFFENSE);
							//BotAI_BotInitialChat(bs, "wantoffence", NULL);
							//trap_BotEnterChat(bs->cs, leader, CHAT_TELL);
						}
					}
				}
				bs->directives.preference |= TEAMTP_ATTACKER;
			}
			bs->directives.preference &= ~TEAMTP_DEFENDER;
		}
		else {
			if (!(bs->directives.preference & TEAMTP_DEFENDER)) {
				// if we have a bot team leader
				if (BotTeamLeader(bs)) {
					// tell the leader we want to be on defense
					BotVoiceChat(bs, -1, VOICECHAT_WANTONDEFENSE);
					//BotAI_BotInitialChat(bs, "wantdefence", NULL);
					//trap_BotEnterChat(bs->cs, leader, CHAT_TELL);
				}
				else if (g_spSkill.integer <= 3) {
					if ( bs->ltgtype != LTG_DEFENDKEYAREA ) {
						//
						if ((gametype != GT_CTF || (bs->redflagstatus == 0 && bs->blueflagstatus == 0)) &&
							(gametype != GT_1FCTF || bs->neutralflagstatus == 0) ) {
							// tell the leader we want to be on defense
							BotVoiceChat(bs, -1, VOICECHAT_WANTONDEFENSE);
							//BotAI_BotInitialChat(bs, "wantdefence", NULL);
							//trap_BotEnterChat(bs->cs, leader, CHAT_TELL);
						}
					}
				}
				bs->directives.preference |= TEAMTP_DEFENDER;
			}
			bs->directives.preference &= ~TEAMTP_ATTACKER;
		}
	}
}

/*
==================
BotUpdateInventory
==================
*/
void BotUpdateInventory(bot_state_t *bs) {
	int oldinventory[MAX_ITEMS];

	memcpy(oldinventory, bs->inventory, sizeof(oldinventory));
	//armor
	bs->inventory[INVENTORY_ARMOR] = bs->cur_ps.stats[STAT_ARMOR];
	//weapons
	bs->inventory[INVENTORY_GAUNTLET] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_GAUNTLET)) != 0;
	bs->inventory[INVENTORY_SHOTGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_SHOTGUN)) != 0;
	bs->inventory[INVENTORY_MACHINEGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_MACHINEGUN)) != 0;
	bs->inventory[INVENTORY_GRENADE_LAUNCHER] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_GRENADE_LAUNCHER)) != 0;
	bs->inventory[INVENTORY_ROCKET_LAUNCHER] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)) != 0;
	bs->inventory[INVENTORY_LIGHTNING_GUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_LIGHTNING_GUN)) != 0;
	bs->inventory[INVENTORY_RAILGUN] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_RAILGUN)) != 0;
	bs->inventory[INVENTORY_PLASMA_RIFLE] = (bs->cur_ps.stats[STAT_WEAPONS] & (1 << WP_PLASMA_RIFLE)) != 0;
	//ammo
	bs->inventory[INVENTORY_SHELLS] = bs->cur_ps.ammo[WP_SHOTGUN];
	bs->inventory[INVENTORY_BULLETS] = bs->cur_ps.ammo[WP_MACHINEGUN];
	bs->inventory[INVENTORY_GRENADES] = bs->cur_ps.ammo[WP_GRENADE_LAUNCHER];
	bs->inventory[INVENTORY_CELLS] = bs->cur_ps.ammo[WP_PLASMA_RIFLE];
	bs->inventory[INVENTORY_LIGHTNING] = bs->cur_ps.ammo[WP_LIGHTNING_GUN];
	bs->inventory[INVENTORY_ROCKETS] = bs->cur_ps.ammo[WP_ROCKET_LAUNCHER];
	bs->inventory[INVENTORY_SLUGS] = bs->cur_ps.ammo[WP_RAILGUN];
	//powerups
	bs->inventory[INVENTORY_HEALTH] = bs->cur_ps.stats[STAT_HEALTH];
	bs->inventory[INVENTORY_TELEPORTER] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_TELEPORTER;
	bs->inventory[INVENTORY_MEDKIT] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_MEDKIT;
	bs->inventory[INVENTORY_KAMIKAZE] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_KAMIKAZE;
	bs->inventory[INVENTORY_PORTAL] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_PORTAL;
	bs->inventory[INVENTORY_INVULNERABILITY] = bs->cur_ps.stats[STAT_HOLDABLE_ITEM] == MODELINDEX_INVULNERABILITY;
	bs->inventory[INVENTORY_QUAD] = bs->cur_ps.powerups[PW_QUAD] != 0;
	bs->inventory[INVENTORY_BERSERK] = bs->cur_ps.powerups[PW_BERSERK] != 0;
	bs->inventory[INVENTORY_ENVIRONMENTSUIT] = bs->cur_ps.powerups[PW_BATTLESUIT] != 0;
	bs->inventory[INVENTORY_HASTE] = bs->cur_ps.powerups[PW_HASTE] != 0;
	bs->inventory[INVENTORY_INVISIBILITY] = bs->cur_ps.powerups[PW_INVIS] != 0;
	bs->inventory[INVENTORY_REGEN] = bs->cur_ps.powerups[PW_REGEN] != 0;
	bs->inventory[INVENTORY_FLIGHT] = bs->cur_ps.powerups[PW_FLIGHT] != 0;
	bs->inventory[INVENTORY_REDFLAG] = bs->cur_ps.powerups[PW_REDFLAG] != 0;
	bs->inventory[INVENTORY_BLUEFLAG] = bs->cur_ps.powerups[PW_BLUEFLAG] != 0;
	bs->inventory[INVENTORY_NEUTRALFLAG] = bs->cur_ps.powerups[PW_NEUTRALFLAG] != 0;
#if FEAT_HARVESTER
	if (BotTeam(bs) == TEAM_RED) {
		bs->inventory[INVENTORY_REDCUBE] = bs->cur_ps.generic1;
		bs->inventory[INVENTORY_BLUECUBE] = 0;
	}
	else {
		bs->inventory[INVENTORY_REDCUBE] = 0;
		bs->inventory[INVENTORY_BLUECUBE] = bs->cur_ps.generic1;
	}
#endif
	BotCheckItemPickup(bs, oldinventory);
}

/*
==================
BotUpdateBattleInventory
==================
*/
void BotUpdateBattleInventory(bot_state_t *bs, int enemy) {
	vec3_t dir;
	aas_entityinfo_t entinfo;

	BotEntityInfo(enemy, &entinfo);
	VectorSubtract(entinfo.origin, bs->origin, dir);
	bs->inventory[ENEMY_HEIGHT] = (int) dir[2];
	dir[2] = 0;
	bs->inventory[ENEMY_HORIZONTAL_DIST] = (int) VectorLength(dir);
	//FIXME: add num visible enemies and num visible team mates to the inventory
}

/*
==================
BotUseKamikaze
==================
*/
#define KAMIKAZE_DIST		1024

void BotUseKamikaze(bot_state_t *bs) {
	int c, teammates, enemies;
	aas_entityinfo_t entinfo;
	vec3_t dir, target;
	bot_goal_t *goal;
	bsp_trace_t trace;

	//if the bot has no kamikaze
	if (bs->inventory[INVENTORY_KAMIKAZE] <= 0)
		return;
	if (bs->kamikaze_time > FloatTime())
		return;
	bs->kamikaze_time = FloatTime() + 0.2;
	if (gametype == GT_CTF) {
		//never use kamikaze if the team flag carrier is visible
		if (BotCTFCarryingFlag(bs))
			return;
		c = BotTeamFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
				return;
		}
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_1FCTF) {
		//never use kamikaze if the team flag carrier is visible
		if (Bot1FCTFCarryingFlag(bs))
			return;
		c = BotTeamFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
				return;
		}
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
#if FEAT_OVERLOAD
	else if (gametype == GT_OBELISK) {
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &blueobelisk; break;
			default: goal = &redobelisk; break;
		}
		//if the obelisk is visible
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST * 0.9)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
#endif
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		//
		if (BotHarvesterCarryingCubes(bs))
			return;
		//never use kamikaze if a team mate carrying cubes is visible
		c = BotTeamCubeCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
				return;
		}
		c = BotEnemyCubeCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
#endif
	//
	BotVisibleTeamMatesAndEnemies(bs, &teammates, &enemies, KAMIKAZE_DIST);
	//
	if (enemies > 2 && enemies > teammates+1) {
		trap_EA_Use(bs->client);
		return;
	}
}

/*
==================
BotUseInvulnerability
==================
*/
void BotUseInvulnerability(bot_state_t *bs) {
	int c;
	vec3_t dir, target;
	bot_goal_t *goal;
	bsp_trace_t trace;

	//if the bot has no invulnerability
	if (bs->inventory[INVENTORY_INVULNERABILITY] <= 0)
		return;
	if (bs->invulnerability_time > FloatTime())
		return;
	bs->invulnerability_time = FloatTime() + 0.2;
	if (gametype == GT_CTF) {
		//never use kamikaze if the team flag carrier is visible
		if (BotCTFCarryingFlag(bs))
			return;
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0)
			return;
		//if near enemy flag and the flag is visible
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &ctf_blueflag; break;
			default: goal = &ctf_redflag; break;
		}
		//if the obelisk is visible
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(200)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_1FCTF) {
		//never use kamikaze if the team flag carrier is visible
		if (Bot1FCTFCarryingFlag(bs))
			return;
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0)
			return;
		//if near enemy flag and the flag is visible
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &ctf_blueflag; break;
			default: goal = &ctf_redflag; break;
		}
		//if the obelisk is visible
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(200)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_OBELISK) {
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &blueobelisk; break;
			default: goal = &redobelisk; break;
		}
		//if the obelisk is visible
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(300)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		//
		if (BotHarvesterCarryingCubes(bs))
			return;
		c = BotEnemyCubeCarrierVisible(bs);
		if (c >= 0)
			return;
		//if near enemy base and enemy base is visible
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &blueobelisk; break;
			default: goal = &redobelisk; break;
		}
		//if the obelisk is visible
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(200)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				trap_EA_Use(bs->client);
				return;
			}
		}
	}
#endif
}

/*
==================
BotBattleUseItems
==================
*/
void BotBattleUseItems(bot_state_t *bs) {
	if (bs->inventory[INVENTORY_HEALTH] < 40) {
		if (bs->inventory[INVENTORY_TELEPORTER] > 0) {
			if (!BotCTFCarryingFlag(bs) && !Bot1FCTFCarryingFlag(bs)
#if FEAT_HARVESTER
				&& !BotHarvesterCarryingCubes(bs)
#endif
				) {
				trap_EA_Use(bs->client);
			}
		}
	}
	if (bs->inventory[INVENTORY_HEALTH] < 60) {
		if (bs->inventory[INVENTORY_MEDKIT] > 0) {
			trap_EA_Use(bs->client);
		}
	}
	BotUseKamikaze(bs);
	BotUseInvulnerability(bs);
}

/*
==================
BotSetTeleportTime
==================
*/
void BotSetTeleportTime(bot_state_t *bs) {
	if ((bs->cur_ps.eFlags ^ bs->last_eFlags) & EF_TELEPORT_BIT) {
		bs->teleport_time = FloatTime();
	}
	bs->last_eFlags = bs->cur_ps.eFlags;
}

/*
==================
BotIsDead
==================
*/
qboolean BotIsDead(bot_state_t *bs) {
	return (bs->cur_ps.pm_type == PM_DEAD);
}

/*
==================
BotIsObserver
==================
*/
qboolean BotIsObserver(bot_state_t *bs) {
	char buf[MAX_INFO_STRING];
	if (bs->cur_ps.pm_type == PM_SPECTATOR) return qtrue;
	trap_GetConfigstring(CS_PLAYERS+bs->client, buf, sizeof(buf));
	if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) return qtrue;
	return qfalse;
}

/*
==================
BotIntermission
==================
*/
qboolean BotIntermission(bot_state_t *bs) {
	//NOTE: we shouldn't be looking at the game code...
	if (level.intermissiontime) return qtrue;
	return (bs->cur_ps.pm_type == PM_FREEZE || bs->cur_ps.pm_type == PM_INTERMISSION);
}

/*
==================
BotInLavaOrSlime
==================
*/
qboolean BotInLavaOrSlime(bot_state_t *bs) {
	vec3_t feet;

	VectorCopy(bs->origin, feet);
	feet[2] -= 23;
	return (trap_AAS_PointContents(feet) & (CONTENTS_LAVA|CONTENTS_SLIME));
}

/*
==================
BotCreateWayPoint
==================
*/
bot_waypoint_t *BotCreateWayPoint(char *name, vec3_t origin, int areanum) {
	bot_waypoint_t *wp;
	vec3_t waypointmins = {-8, -8, -8}, waypointmaxs = {8, 8, 8};

	wp = botai_freewaypoints;
	if ( !wp ) {
		BotAI_Print( PRT_WARNING, "BotCreateWayPoint: Out of waypoints\n" );
		return NULL;
	}
	botai_freewaypoints = botai_freewaypoints->next;

	Q_strncpyz( wp->name, name, sizeof(wp->name) );
	VectorCopy(origin, wp->goal.origin);
	VectorCopy(waypointmins, wp->goal.mins);
	VectorCopy(waypointmaxs, wp->goal.maxs);
	wp->goal.areanum = areanum;
	wp->next = NULL;
	wp->prev = NULL;
	return wp;
}

/*
==================
BotFindWayPoint
==================
*/
bot_waypoint_t *BotFindWayPoint(bot_waypoint_t *waypoints, char *name) {
	bot_waypoint_t *wp;

	for (wp = waypoints; wp; wp = wp->next) {
		if (!Q_stricmp(wp->name, name)) return wp;
	}
	return NULL;
}

/*
==================
BotFreeWaypoints
==================
*/
void BotFreeWaypoints(bot_waypoint_t *wp) {
	bot_waypoint_t *nextwp;

	for (; wp; wp = nextwp) {
		nextwp = wp->next;
		wp->next = botai_freewaypoints;
		botai_freewaypoints = wp;
	}
}

/*
==================
BotInitWaypoints
==================
*/
void BotInitWaypoints(void) {
	int i;

	botai_freewaypoints = NULL;
	for (i = 0; i < MAX_WAYPOINTS; i++) {
		botai_waypoints[i].next = botai_freewaypoints;
		botai_freewaypoints = &botai_waypoints[i];
	}
}

/*
==================
TeamPlayIsOn
==================
*/
int TeamPlayIsOn(void) {
	return g_gametypeIsTeamGame;
}

/*
==================
BotAggression
==================
*/
float BotAggression(bot_state_t *bs) {
	if ( bs->wiredBotsActive ) {
		return WiredBots_Aggression( bs );
	}

	//if the bot has quad
	if (bs->inventory[INVENTORY_QUAD]) {
		//if the bot is not holding the gauntlet or the enemy is really nearby
		if (bs->weaponnum != WP_GAUNTLET ||
			bs->inventory[ENEMY_HORIZONTAL_DIST] < 80) {
			return 70;
		}
	}
	//if the enemy is located way higher than the bot
	if (bs->inventory[ENEMY_HEIGHT] > 200) return 0;
	//if the bot is very low on health
	if (bs->inventory[INVENTORY_HEALTH] < 60) return 0;
	//if the bot is low on health
	if (bs->inventory[INVENTORY_HEALTH] < 80) {
		//if the bot has insufficient armor
		if (bs->inventory[INVENTORY_ARMOR] < 40) return 0;
	}
	//if the bot can use the railgun
	if (bs->inventory[INVENTORY_RAILGUN] > 0 &&
			bs->inventory[INVENTORY_SLUGS] > 5) return 95;
	//if the bot can use the lightning gun
	if (bs->inventory[INVENTORY_LIGHTNING_GUN] > 0 &&
			bs->inventory[INVENTORY_LIGHTNING] > 50) return 90;
	//if the bot can use the rocketlauncher
	if (bs->inventory[INVENTORY_ROCKET_LAUNCHER] > 0 &&
			bs->inventory[INVENTORY_ROCKETS] > 5) return 90;
	//if the bot can use the plasmagun
	if (bs->inventory[INVENTORY_PLASMA_RIFLE] > 0 &&
			bs->inventory[INVENTORY_CELLS] > 40) return 85;
	//if the bot can use the grenade launcher
	if (bs->inventory[INVENTORY_GRENADE_LAUNCHER] > 0 &&
			bs->inventory[INVENTORY_GRENADES] > 10) return 80;
	//if the bot can use the shotgun
	if (bs->inventory[INVENTORY_SHOTGUN] > 0 &&
			bs->inventory[INVENTORY_SHELLS] > 10) return 50;
	//otherwise the bot is not feeling too good
	return 0;
}

/*
==================
BotFeelingBad
==================
*/
float BotFeelingBad(bot_state_t *bs) {
	if (bs->weaponnum == WP_GAUNTLET) {
		return 100;
	}
	if (bs->inventory[INVENTORY_HEALTH] < 40) {
		return 100;
	}
	if (bs->weaponnum == WP_MACHINEGUN) {
		return 90;
	}
	if (bs->inventory[INVENTORY_HEALTH] < 60) {
		return 80;
	}
	return 0;
}

/*
==================
BotWantsToRetreat
==================
*/
int BotWantsToRetreat(bot_state_t *bs) {
	aas_entityinfo_t entinfo;

	/* WiredBots tactic: RETREAT forces the bot to always fall back */
	if ( bs->directives.tactic == TACTIC_RETREAT && bs->directives.tactic_active ) {
		return qtrue;
	}

	if ( bs->wiredBotsActive ) {
		return WiredBots_WantsToRetreat( bs );
	}

	if (gametype == GT_CTF) {
		//always retreat when carrying a CTF flag
		if (BotCTFCarryingFlag(bs))
			return qtrue;
	}
	else if (gametype == GT_1FCTF) {
		//if carrying the flag then always retreat
		if (Bot1FCTFCarryingFlag(bs))
			return qtrue;
	}
#if FEAT_OVERLOAD
	else if (gametype == GT_OBELISK) {
		//the bots should be dedicated to attacking the enemy obelisk
		if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
			if (bs->enemy != redobelisk.entitynum &&
						bs->enemy != blueobelisk.entitynum) {
				return qtrue;
			}
		}
		if (BotFeelingBad(bs) > 50) {
			return qtrue;
		}
		return qfalse;
	}
#endif
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		//if carrying cubes then always retreat
		if (BotHarvesterCarryingCubes(bs)) return qtrue;
	}
#endif
	//
	if (bs->enemy >= 0) {
		BotEntityInfo(bs->enemy, &entinfo);
		// if the enemy is carrying a flag
		if (EntityCarriesFlag(&entinfo)) return qfalse;
#if FEAT_HARVESTER
		// if the enemy is carrying cubes
		if (EntityCarriesCubes(&entinfo)) return qfalse;
#endif
	}
	//if the bot is getting the flag
	if (bs->ltgtype == LTG_GETFLAG)
		return qtrue;
	//
	if (BotAggression(bs) < 50)
		return qtrue;
	return qfalse;
}

/*
==================
BotWantsToChase
==================
*/
int BotWantsToChase(bot_state_t *bs) {
	aas_entityinfo_t entinfo;

	if ( bs->wiredBotsActive ) {
		return WiredBots_WantsToChase( bs );
	}

	if (gametype == GT_CTF) {
		//never chase when carrying a CTF flag
		if (BotCTFCarryingFlag(bs))
			return qfalse;
		//always chase if the enemy is carrying a flag
		BotEntityInfo(bs->enemy, &entinfo);
		if (EntityCarriesFlag(&entinfo))
			return qtrue;
	}
	else if (gametype == GT_1FCTF) {
		//never chase if carrying the flag
		if (Bot1FCTFCarryingFlag(bs))
			return qfalse;
		//always chase if the enemy is carrying a flag
		BotEntityInfo(bs->enemy, &entinfo);
		if (EntityCarriesFlag(&entinfo))
			return qtrue;
	}
#if FEAT_OVERLOAD
	else if (gametype == GT_OBELISK) {
		//the bots should be dedicated to attacking the enemy obelisk
		if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
			if (bs->enemy != redobelisk.entitynum &&
						bs->enemy != blueobelisk.entitynum) {
				return qfalse;
			}
		}
	}
#endif
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		//never chase if carrying cubes
		if (BotHarvesterCarryingCubes(bs)) return qfalse;

		BotEntityInfo(bs->enemy, &entinfo);
		// always chase if the enemy is carrying cubes
		if (EntityCarriesCubes(&entinfo)) return qtrue;
	}
#endif
	//if the bot is getting the flag
	if (bs->ltgtype == LTG_GETFLAG)
		return qfalse;
	//
	if (BotAggression(bs) > 50)
		return qtrue;
	return qfalse;
}

/*
==================
BotWantsToHelp
==================
*/
int BotWantsToHelp(bot_state_t *bs) {
	return qtrue;
}

/*
==================
BotCanAndWantsToRocketJump
==================
*/
int BotCanAndWantsToRocketJump(bot_state_t *bs) {
	float rocketjumper;

	//if rocket jumping is disabled
	if (!bot_rocketjump.integer) return qfalse;
	//if no rocket launcher
	if (bs->inventory[INVENTORY_ROCKET_LAUNCHER] <= 0) return qfalse;
	//if low on rockets
	if (bs->inventory[INVENTORY_ROCKETS] < 3) return qfalse;
	//never rocket jump with the Quad
	if (bs->inventory[INVENTORY_QUAD]) return qfalse;
	//if low on health
	if (bs->inventory[INVENTORY_HEALTH] < 60) return qfalse;
	//if not full health
	if (bs->inventory[INVENTORY_HEALTH] < 90) {
		//if the bot has insufficient armor
		if (bs->inventory[INVENTORY_ARMOR] < 40) return qfalse;
	}
	if ( bs->wiredBotsActive ) {
		rocketjumper = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ROCKET_JUMP, 0.0f ) > 0.5f ? 1.0f : 0.0f;
	} else {
		rocketjumper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_WEAPONJUMPING, 0, 1);
	}
	if (rocketjumper < 0.5) return qfalse;
	return qtrue;
}

/*
==================
BotHasPersistantPowerupAndWeapon
==================
*/
int BotHasPersistantPowerupAndWeapon(bot_state_t *bs) {
	//if the bot is very low on health
	if (bs->inventory[INVENTORY_HEALTH] < 60) return qfalse;
	//if the bot is low on health
	if (bs->inventory[INVENTORY_HEALTH] < 80) {
		//if the bot has insufficient armor
		if (bs->inventory[INVENTORY_ARMOR] < 40) return qfalse;
	}
	//if the bot can use the railgun
	if (bs->inventory[INVENTORY_RAILGUN] > 0 &&
			bs->inventory[INVENTORY_SLUGS] > 5) return qtrue;
	//if the bot can use the lightning gun
	if (bs->inventory[INVENTORY_LIGHTNING_GUN] > 0 &&
			bs->inventory[INVENTORY_LIGHTNING] > 50) return qtrue;
	//if the bot can use the rocketlauncher
	if (bs->inventory[INVENTORY_ROCKET_LAUNCHER] > 0 &&
			bs->inventory[INVENTORY_ROCKETS] > 5) return qtrue;
	//if the bot can use the plasmagun
	if (bs->inventory[INVENTORY_PLASMA_RIFLE] > 0 &&
			bs->inventory[INVENTORY_CELLS] > 20) return qtrue;
	return qfalse;
}

/*
==================
BotGoCamp
==================
*/
void BotGoCamp(bot_state_t *bs, bot_goal_t *goal) {
	float camper;

	bs->decisionmaker = bs->client;
	//set message time to zero so bot will NOT show any message
	bs->teammessage_time = 0;
	//set the ltg type
	bs->ltgtype = LTG_CAMP;
	//set the team goal
	memcpy(&bs->teamgoal, goal, sizeof(bot_goal_t));
	//get the team goal time
	if ( bs->wiredBotsActive ) {
		camper = WiredBots_ProfileFieldOr( bs, WB_PROFILE_CAMP_TENDENCY, 0.5f );
	} else {
		camper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
	}
	if (camper > 0.99) bs->teamgoal_time = FloatTime() + 99999;
	else bs->teamgoal_time = FloatTime() + 120 + 180 * camper + random() * 15;
	//set the last time the bot started camping
	bs->camp_time = FloatTime();
	//the teammate that requested the camping
	bs->teammate = 0;
	//do NOT type arrive message
	bs->arrive_time = 1;
}

/*
==================
BotWantsToCamp
==================
*/
int BotWantsToCamp(bot_state_t *bs) {
	float camper;
	int cs, traveltime, besttraveltime;
	bot_goal_t goal, bestgoal;

	if ( bs->wiredBotsActive ) {
		camper = WiredBots_ProfileFieldOr( bs, WB_PROFILE_CAMP_TENDENCY, 0.5f );
	} else {
		camper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
	}
	if (camper < 0.1) return qfalse;
	//if the bot has a team goal
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_CAMP ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL) {
		return qfalse;
	}
	//if camped recently
	if (bs->camp_time > FloatTime() - 60 + 300 * (1-camper)) return qfalse;
	//
	if (random() > camper) {
		bs->camp_time = FloatTime();
		return qfalse;
	}
	//if the bot isn't healthy enough
	if (BotAggression(bs) < 50) return qfalse;
	//the bot should have at least have the rocket launcher or the railgun with some ammo
	if ((bs->inventory[INVENTORY_ROCKET_LAUNCHER] <= 0 || bs->inventory[INVENTORY_ROCKETS] < 10) &&
		(bs->inventory[INVENTORY_RAILGUN] <= 0 || bs->inventory[INVENTORY_SLUGS] < 10)) {
		return qfalse;
	}
	//find the closest camp spot
	besttraveltime = 99999;
	for (cs = trap_BotGetNextCampSpotGoal(0, &goal); cs; cs = trap_BotGetNextCampSpotGoal(cs, &goal)) {
		traveltime = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, goal.areanum, TFL_DEFAULT);
		if (traveltime && traveltime < besttraveltime) {
			besttraveltime = traveltime;
			memcpy(&bestgoal, &goal, sizeof(bot_goal_t));
		}
	}
	if (besttraveltime > 150) return qfalse;
	//ok found a camp spot, go camp there
	BotGoCamp(bs, &bestgoal);
	bs->ordered = qfalse;
	//
	return qtrue;
}

/*
==================
BotDontAvoid
==================
*/
void BotDontAvoid(bot_state_t *bs, char *itemname) {
	bot_goal_t goal;
	int num;

	num = trap_BotGetLevelItemGoal(-1, itemname, &goal);
	while(num >= 0) {
		trap_BotRemoveFromAvoidGoals(bs->gs, goal.number);
		num = trap_BotGetLevelItemGoal(num, itemname, &goal);
	}
}

/*
==================
BotGoForPowerups
==================
*/
void BotGoForPowerups(bot_state_t *bs) {

	//don't avoid any of the powerups anymore
	BotDontAvoid(bs, "Quad Damage");
	BotDontAvoid(bs, "Regeneration");
	BotDontAvoid(bs, "Battle Suit");
	BotDontAvoid(bs, "Haste");
	BotDontAvoid(bs, "Invisibility");
	//BotDontAvoid(bs, "Flight");
	//reset the long term goal time so the bot will go for the powerup
	//NOTE: the long term goal type doesn't change
	bs->ltg_time = 0;
}

/*
==================
BotRoamGoal
==================
*/
void BotRoamGoal(bot_state_t *bs, vec3_t goal) {
	int pc, i;
	float len, rnd;
	vec3_t dir, bestorg, belowbestorg;
	bsp_trace_t trace;

	for (i = 0; i < 10; i++) {
		//start at the bot origin
		VectorCopy(bs->origin, bestorg);
		rnd = random();
		if (rnd > 0.25) {
			//add a random value to the x-coordinate
			if (random() < 0.5) bestorg[0] -= 800 * random() + 100;
			else bestorg[0] += 800 * random() + 100;
		}
		if (rnd < 0.75) {
			//add a random value to the y-coordinate
			if (random() < 0.5) bestorg[1] -= 800 * random() + 100;
			else bestorg[1] += 800 * random() + 100;
		}
		//add a random value to the z-coordinate (NOTE: 48 = maxjump?)
		bestorg[2] += 2 * 48 * crandom();
		//trace a line from the origin to the roam target
		BotAI_Trace(&trace, bs->origin, NULL, NULL, bestorg, bs->entitynum, MASK_SOLID);
		//direction and length towards the roam target
		VectorSubtract(trace.endpos, bs->origin, dir);
		len = VectorNormalize(dir);
		//if the roam target is far away enough
		if (len > 200) {
			//the roam target is in the given direction before walls
			VectorScale(dir, len * trace.fraction - 40, dir);
			VectorAdd(bs->origin, dir, bestorg);
			//get the coordinates of the floor below the roam target
			belowbestorg[0] = bestorg[0];
			belowbestorg[1] = bestorg[1];
			belowbestorg[2] = bestorg[2] - 800;
			BotAI_Trace(&trace, bestorg, NULL, NULL, belowbestorg, bs->entitynum, MASK_SOLID);
			//
			if (!trace.startsolid) {
				trace.endpos[2]++;
				pc = trap_PointContents(trace.endpos, bs->entitynum);
				if (!(pc & (CONTENTS_LAVA | CONTENTS_SLIME))) {
					VectorCopy(bestorg, goal);
					return;
				}
			}
		}
	}
	VectorCopy(bestorg, goal);
}

/*
==================
BotAttackMove
==================
*/
bot_moveresult_t BotAttackMove(bot_state_t *bs, int tfl) {
	int movetype, i, attackentity;
	float attack_skill, jumper, croucher, dist, strafechange_time;
	float attack_dist, attack_range;
	vec3_t forward, backward, sideward, hordir, up = {0, 0, 1};
	aas_entityinfo_t entinfo;
	bot_moveresult_t moveresult;
	bot_goal_t goal;

	attackentity = bs->enemy;
	//
	if (bs->attackchase_time > FloatTime()) {
		//create the chase goal
		goal.entitynum = attackentity;
		goal.areanum = bs->lastenemyareanum;
		VectorCopy(bs->lastenemyorigin, goal.origin);
		VectorSet(goal.mins, -8, -8, -8);
		VectorSet(goal.maxs, 8, 8, 8);
		//initialize the movement state
		BotSetupForMovement(bs);
		//move towards the goal
		trap_BotMoveToGoal(&moveresult, bs->ms, &goal, tfl);
		BotMovementThink(bs, &moveresult);
		return moveresult;
	}
	//
	memset(&moveresult, 0, sizeof(bot_moveresult_t));
	//
	if ( bs->wiredBotsActive ) {
		attack_skill = WiredBots_ProfileFieldOr( bs, WB_PROFILE_AGGRESSION, 0.5f );
		jumper = WiredBots_ProfileFieldOr( bs, WB_PROFILE_BUNNY_HOP, 0.3f );
		croucher = WiredBots_ProfileFieldOr( bs, WB_PROFILE_DODGE_ON_FIRE, 0.2f );
	} else {
		attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
		jumper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_JUMPER, 0, 1);
		croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
	}
	//if the bot is really stupid
	if (attack_skill < 0.2) return moveresult;
	//initialize the movement state
	BotSetupForMovement(bs);
	//get the enemy entity info
	BotEntityInfo(attackentity, &entinfo);
	//direction towards the enemy
	VectorSubtract(entinfo.origin, bs->origin, forward);
	//the distance towards the enemy
	dist = VectorNormalize(forward);
	VectorNegate(forward, backward);
	//walk, crouch or jump
	movetype = MOVE_WALK;
	//
	if (bs->attackcrouch_time < FloatTime() - 1) {
		if (random() < jumper) {
			movetype = MOVE_JUMP;
		}
		//wait at least one second before crouching again
		else if (bs->attackcrouch_time < FloatTime() - 1 && random() < croucher) {
			bs->attackcrouch_time = FloatTime() + croucher * 5;
		}
	}
	if (bs->attackcrouch_time > FloatTime()) movetype = MOVE_CROUCH;
	//if the bot should jump
	if (movetype == MOVE_JUMP) {
		//if jumped last frame
		if (bs->attackjump_time > FloatTime()) {
			movetype = MOVE_WALK;
		}
		else {
			bs->attackjump_time = FloatTime() + 1;
		}
	}
	if (bs->cur_ps.weapon == WP_GAUNTLET) {
		attack_dist = 0;
		attack_range = 0;
	}
	else {
		attack_dist = IDEAL_ATTACKDIST;
		attack_range = 40;
	}
	//if the bot is stupid
	if (attack_skill <= 0.4) {
		//just walk to or away from the enemy
		if (dist > attack_dist + attack_range) {
			if (trap_BotMoveInDirection(bs->ms, forward, 400, movetype)) return moveresult;
		}
		if (dist < attack_dist - attack_range) {
			if (trap_BotMoveInDirection(bs->ms, backward, 400, movetype)) return moveresult;
		}
		return moveresult;
	}
	//increase the strafe time
	bs->attackstrafe_time += bs->thinktime;
	//get the strafe change time
	strafechange_time = 0.4 + (1 - attack_skill) * 0.2;
	if (attack_skill > 0.7) strafechange_time += crandom() * 0.2;
	//if the strafe direction should be changed
	if (bs->attackstrafe_time > strafechange_time) {
		//some magic number :)
		if (random() > 0.935) {
			//flip the strafe direction
			bs->flags ^= BFL_STRAFERIGHT;
			bs->attackstrafe_time = 0;
		}
	}
	//
	for (i = 0; i < 2; i++) {
		hordir[0] = forward[0];
		hordir[1] = forward[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		//get the sideward vector
		CrossProduct(hordir, up, sideward);
		//reverse the vector depending on the strafe direction
		if (bs->flags & BFL_STRAFERIGHT) VectorNegate(sideward, sideward);
		//randomly go back a little
		if (random() > 0.9) {
			VectorAdd(sideward, backward, sideward);
		}
		else {
			//walk forward or backward to get at the ideal attack distance
			if (dist > attack_dist + attack_range) {
				VectorAdd(sideward, forward, sideward);
			}
			else if (dist < attack_dist - attack_range) {
				VectorAdd(sideward, backward, sideward);
			}
		}
		//perform the movement
		if (trap_BotMoveInDirection(bs->ms, sideward, 400, movetype))
			return moveresult;
		//movement failed, flip the strafe direction
		bs->flags ^= BFL_STRAFERIGHT;
		bs->attackstrafe_time = 0;
	}
	//bot couldn't do any useful movement
//	bs->attackchase_time = AAS_Time() + 6;
	return moveresult;
}

/*
==================
BotSameTeam
==================
*/
int BotSameTeam(bot_state_t *bs, int entnum) {

	if (bs->client < 0 || bs->client >= MAX_CLIENTS) {
		return qfalse;
	}

	if (entnum < 0 || entnum >= MAX_CLIENTS) {
		return qfalse;
	}

    if (gametype == GT_KINGOFTHEHILL) {
        playerState_t myPs;
        playerState_t ps;

        BotAI_GetClientState(bs->client, &myPs);
        BotAI_GetClientState(entnum, &ps);

        if (myPs.powerups[PW_KING]) {
            return qfalse;
        }
        if (ps.powerups[PW_KING]) {
            return qfalse;
        }

        return qtrue;
    }

    if (g_gametypeIsTeamGame) {
        if (level.clients[bs->client].sess.sessionTeam == level.clients[entnum].sess.sessionTeam) {
            return qtrue;
        }
	}

	return qfalse;
}

/*
==================
InFieldOfVision
==================
*/
qboolean InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles)
{
	int i;
	float diff, angle;

	for (i = 0; i < 2; i++) {
		angle = AngleMod(viewangles[i]);
		angles[i] = AngleMod(angles[i]);
		diff = angles[i] - angle;
		if (angles[i] > angle) {
			if (diff > 180.0) diff -= 360.0;
		}
		else {
			if (diff < -180.0) diff += 360.0;
		}
		if (diff > 0) {
			if (diff > fov * 0.5) return qfalse;
		}
		else {
			if (diff < -fov * 0.5) return qfalse;
		}
	}
	return qtrue;
}

/*
==================
BotEntityVisible

returns visibility in the range [0, 1] taking fog and water surfaces into account
==================
*/
float BotEntityVisible(int viewer, vec3_t eye, vec3_t viewangles, float fov, int ent) {
	int i, contents_mask, passent, hitent, infog, inwater, otherinfog, pc;
	float squaredfogdist, waterfactor, vis, bestvis;
	bsp_trace_t trace;
	aas_entityinfo_t entinfo;
	vec3_t dir, entangles, start, end, middle;

	BotEntityInfo(ent, &entinfo);
	if (!entinfo.valid) {
		return 0;
	}

	//calculate middle of bounding box
	VectorAdd(entinfo.mins, entinfo.maxs, middle);
	VectorScale(middle, 0.5, middle);
	VectorAdd(entinfo.origin, middle, middle);
	//check if entity is within field of vision
	VectorSubtract(middle, eye, dir);
	vectoangles(dir, entangles);
	if (!InFieldOfVision(viewangles, fov, entangles)) return 0;
	//
	pc = trap_AAS_PointContents(eye);
	infog = (pc & CONTENTS_FOG);
	inwater = (pc & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER));
	//
	bestvis = 0;
	for (i = 0; i < 3; i++) {
		//if the point is not in potential visible sight
		//if (!AAS_inPVS(eye, middle)) continue;
		//
		// Use MASK_OPAQUE (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA) for LOS checks.
		// CONTENTS_PLAYERCLIP (MASK_DEADSOLID) blocks invisible movement-barrier brushes
		// that are placed by mappers to seal ledges/pits — these must NOT block visibility.
		contents_mask = MASK_OPAQUE;
		passent = viewer;
		hitent = ent;
		VectorCopy(eye, start);
		VectorCopy(middle, end);
		//if the entity is in water, lava or slime
		if (trap_AAS_PointContents(middle) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER)) {
			contents_mask |= CONTENTS_WATER;
		}
		//if eye is in water, lava or slime
		if (inwater) {
			if (!(contents_mask & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))) {
				passent = ent;
				hitent = viewer;
				VectorCopy(middle, start);
				VectorCopy(eye, end);
			}
			contents_mask ^= (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
		}
		//trace from start to end
		BotAI_Trace(&trace, start, NULL, NULL, end, passent, contents_mask);
		//if water was hit
		waterfactor = 1.0;
		//note: trace.contents is always 0, see BotAI_Trace
		if (trace.contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER)) {
			//if the water surface is translucent
			if (1) {
				//trace through the water
				contents_mask &= ~(CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
				BotAI_Trace(&trace, trace.endpos, NULL, NULL, end, passent, contents_mask);
				waterfactor = 0.5;
			}
		}
		//if a full trace or the hitent was hit
		if (trace.fraction >= 1 || trace.ent == hitent) {
			//check for fog, assuming there's only one fog brush where
			//either the viewer or the entity is in or both are in
			otherinfog = (trap_AAS_PointContents(middle) & CONTENTS_FOG);
			if (infog && otherinfog) {
				VectorSubtract(trace.endpos, eye, dir);
				squaredfogdist = VectorLengthSquared(dir);
			}
			else if (infog) {
				VectorCopy(trace.endpos, start);
				BotAI_Trace(&trace, start, NULL, NULL, eye, viewer, CONTENTS_FOG);
				VectorSubtract(eye, trace.endpos, dir);
				squaredfogdist = VectorLengthSquared(dir);
			}
			else if (otherinfog) {
				VectorCopy(trace.endpos, end);
				BotAI_Trace(&trace, eye, NULL, NULL, end, viewer, CONTENTS_FOG);
				VectorSubtract(end, trace.endpos, dir);
				squaredfogdist = VectorLengthSquared(dir);
			}
			else {
				//if the entity and the viewer are not in fog assume there's no fog in between
				squaredfogdist = 0;
			}
			//decrease visibility with the view distance through fog
			vis = 1 / ((squaredfogdist * 0.001) < 1 ? 1 : (squaredfogdist * 0.001));
			//if entering water visibility is reduced
			vis *= waterfactor;
			//
			if (vis > bestvis) bestvis = vis;
			//if pretty much no fog
			if (bestvis >= 0.95) return bestvis;
		}
		//check eye-level and top of bounding box as well
		// NOTE: iteration 0 ends at bbox center (origin+4).
		// Previously iteration 1 used mins[2] offset (-24) which went underground
		// (origin-20) and was always trace-blocked. Now we trace to approximate
		// eye level (origin+26) and top of head (origin+maxs[2]) instead.
		if (i == 0) middle[2] = entinfo.origin[2] + 26;         // approx eye height
		else if (i == 1) middle[2] = entinfo.origin[2] + entinfo.maxs[2]; // top of head
	}
	return bestvis;
}

/*
==================
BotFindEnemy
==================
*/
int BotFindEnemy(bot_state_t *bs, int curenemy) {
	int i, healthdecrease;
	float f, alertness, easyfragger, vis;
	float squaredist, cursquaredist;
	aas_entityinfo_t entinfo, curenemyinfo;
	vec3_t dir, angles;

	if ( bs->wiredBotsActive ) {
		alertness = WiredBots_ProfileFieldOr( bs, WB_PROFILE_TRACKING, 0.5f );
		easyfragger = WiredBots_ProfileFieldOr( bs, WB_PROFILE_AGGRESSION, 0.5f );
	} else {
		alertness = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ALERTNESS, 0, 1);
		easyfragger = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_EASY_FRAGGER, 0, 1);
	}

	if ( trap_Cvar_VariableIntegerValue( "sv_botDebugDecide" ) ) {
		static float s_fovLog[MAX_CLIENTS];
		if ( FloatTime() - s_fovLog[bs->client] > 5.0f ) {
			s_fovLog[bs->client] = FloatTime();
			float viewfactor = bs->wiredBotsActive ? alertness :
				trap_Characteristic_BFloat( bs->character, CHARACTERISTIC_VIEW_FACTOR, 0, 1 );
			G_Printf( "^5[BotFOV] cl=%d alertness=%.2f viewfactor=%.2f alertFov=%.0f (lua=%d)\n",
				bs->client, alertness, viewfactor, 90.0f + alertness * 90.0f,
				bs->wiredBotsActive );
		}
	}
	//check if the health decreased
	healthdecrease = bs->lasthealth > bs->inventory[INVENTORY_HEALTH];
	//remember the current health value
	bs->lasthealth = bs->inventory[INVENTORY_HEALTH];
	//
	if (curenemy >= 0) {
		BotEntityInfo(curenemy, &curenemyinfo);
		if (EntityCarriesFlag(&curenemyinfo)) return qfalse;
		VectorSubtract(curenemyinfo.origin, bs->origin, dir);
		cursquaredist = VectorLengthSquared(dir);
	}
	else {
		cursquaredist = 0;
	}

	/* WiredBots tactic: HUNT — if a specific target is visible, engage them
	   first regardless of distance, bypassing the normal candidate loop. */
	if ( bs->directives.tactic == TACTIC_HUNT && bs->directives.tactic_active ) {
		int hunt_target = bs->directives.tactic_target;
		if ( hunt_target >= 0 && hunt_target < MAX_CLIENTS &&
		     hunt_target != bs->client &&
		     level.clients[hunt_target].pers.connected == CON_CONNECTED &&
		     !BotSameTeam( bs, hunt_target ) ) {
			aas_entityinfo_t huntinfo;
			BotEntityInfo( hunt_target, &huntinfo );
			if ( huntinfo.valid && !EntityIsDead( &huntinfo ) ) {
				float vis = BotEntityVisible( bs->entitynum, bs->eye,
				                              bs->viewangles, 360, hunt_target );
				if ( vis > 0 ) {
					bs->enemy             = hunt_target;
					bs->enemysight_time   = FloatTime();
					bs->enemysuicide      = qfalse;
					bs->enemydeath_time   = 0;
					bs->enemyvisible_time = FloatTime();
					return qtrue;
				}
			}
		}
	}

#if FEAT_OVERLOAD
	if (gametype == GT_OBELISK) {
		vec3_t target;
		bot_goal_t *goal;
		bsp_trace_t trace;

		if (BotTeam(bs) == TEAM_RED)
			goal = &blueobelisk;
		else
			goal = &redobelisk;
		//if the obelisk is visible
		VectorCopy(goal->origin, target);
		target[2] += 1;
		BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
		if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
			if (goal->entitynum == bs->enemy) {
				return qfalse;
			}
			bs->enemy = goal->entitynum;
			bs->enemysight_time = FloatTime();
			bs->enemysuicide = qfalse;
			bs->enemydeath_time = 0;
			bs->enemyvisible_time = FloatTime();
			return qtrue;
		}
	}
#endif
	//
	for (i = 0; i < level.maxclients; i++) {

		if (i == bs->client) continue;
		//if it's the current enemy
		if (i == curenemy) continue;
		//if the enemy has targeting disabled
		if (g_entities[i].flags & FL_NOTARGET) {
			continue;
		}
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
		//if not an easy fragger don't shoot at chatting players
		if (easyfragger < 0.5 && EntityIsChatting(&entinfo)) continue;
		//
		if (lastteleport_time > FloatTime() - 3) {
			VectorSubtract(entinfo.origin, lastteleport_origin, dir);
			if (VectorLengthSquared(dir) < Square(70)) continue;
		}
		//calculate the distance towards the enemy
		VectorSubtract(entinfo.origin, bs->origin, dir);
		squaredist = VectorLengthSquared(dir);
		//if this entity is not carrying a flag
		if (!EntityCarriesFlag(&entinfo))
		{
			//if this enemy is further away than the current one
			if (curenemy >= 0 && squaredist > cursquaredist) continue;
		} //end if
		//if the bot has no
		if (squaredist > Square(900.0 + alertness * 4000.0)) continue;
		//if on the same team
		if (BotSameTeam(bs, i)) continue;
		//stateless clients have no game presence — never a valid enemy
		if ( level.clients[i].sess.isStatelessClient ) continue;
		/* WiredBots tactic: AVOID — skip the designated target unless it is
		   actively hurting us (health decrease = we must engage to survive). */
		if ( bs->directives.tactic == TACTIC_AVOID && bs->directives.tactic_active &&
		     i == bs->directives.tactic_target && !healthdecrease ) {
			continue;
		}
		//if the bot's health decreased, the enemy is shooting, or the enemy is
		// within hearing range (~350 units — simulates footstep/breathing awareness)
		if (curenemy < 0 && (healthdecrease || EntityIsShooting(&entinfo) || squaredist < Square(350.0f)))
			f = 360;
		else {
			// Distance-based FOV grows from 90° (close) to 180° (810+ units).
			// alertness provides a floor so aware bots don't miss point-blank enemies.
			// alertness=0.5 (default) → floor=135°; alertness=0.8 → floor=162°.
			float distFov  = 90.0f + ( squaredist > Square( 810.0f ) ? Square( 810.0f ) : squaredist ) / ( 810.0f * 9.0f );
			float alertFov = 90.0f + alertness * 90.0f;
			f = distFov > alertFov ? distFov : alertFov;
		}
		//check if the enemy is visible
		// During proactive scanning (no current enemy), remove the view-direction gate so
		// any in-LOS enemy is detectable regardless of which way the bot faces.
		// The FOV gate only applies in combat (curenemy >= 0) to simulate tunnel-vision.
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, curenemy < 0 ? 360.0f : f, i);
		if (vis <= 0) {
			if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugDecide" ) ) {
				static float s_visLog[MAX_CLIENTS];
				if ( FloatTime() - s_visLog[bs->client] > 1.5f ) {
					s_visLog[bs->client] = FloatTime();
					vec3_t toEnemy, entAng;
					VectorSubtract( entinfo.origin, bs->eye, toEnemy );
					vectoangles( toEnemy, entAng );
					qboolean fovResult = InFieldOfVision( bs->viewangles, f, entAng );
					// Compute the 3 trace targets (same formula as BotEntityVisible)
					vec3_t eMid, eCenter, eEyeLevel, eTop;
					VectorAdd( entinfo.mins, entinfo.maxs, eMid );
					VectorScale( eMid, 0.5f, eMid );
					VectorAdd( entinfo.origin, eMid, eCenter );
					VectorCopy( eCenter, eEyeLevel ); eEyeLevel[2] = entinfo.origin[2] + 26;
					VectorCopy( eCenter, eTop ); eTop[2] = entinfo.origin[2] + entinfo.maxs[2];
					// Run the same 3 traces to get concrete fraction/ent results
					bsp_trace_t t0, t1, t2;
					int cmask = MASK_OPAQUE; // must match BotEntityVisible
					BotAI_Trace( &t0, bs->eye, NULL, NULL, eCenter,    bs->entitynum, cmask );
					BotAI_Trace( &t1, bs->eye, NULL, NULL, eEyeLevel,  bs->entitynum, cmask );
					BotAI_Trace( &t2, bs->eye, NULL, NULL, eTop,       bs->entitynum, cmask );
					G_Printf( "^3[FindEnemy] cl=%d cand=%d VIS_FAIL f=%.0f dist=%.0f fov=%s\n"
						"  shooter  eye=(%.1f %.1f %.1f) "
						"ps.orig=(%.1f %.1f %.1f) r.cur=(%.1f %.1f %.1f) viewht=%d\n"
						"  enemy    entinfo.orig=(%.1f %.1f %.1f) "
						"enemy_dir=(%.0f %.0f)\n"
						"  t0 center  ->(%.1f %.1f %.1f) frac=%.3f ent=%d\n"
						"  t1 eyelevel->(%.1f %.1f %.1f) frac=%.3f ent=%d\n"
						"  t2 top     ->(%.1f %.1f %.1f) frac=%.3f ent=%d\n",
						bs->client, i, f, sqrtf( squaredist ),
						fovResult ? "pass" : "FAIL",
						bs->eye[0], bs->eye[1], bs->eye[2],
						bs->cur_ps.origin[0], bs->cur_ps.origin[1], bs->cur_ps.origin[2],
						g_entities[bs->client].r.currentOrigin[0],
						g_entities[bs->client].r.currentOrigin[1],
						g_entities[bs->client].r.currentOrigin[2],
						bs->cur_ps.viewheight,
						entinfo.origin[0], entinfo.origin[1], entinfo.origin[2],
						entAng[PITCH], entAng[YAW],
						eCenter[0],   eCenter[1],   eCenter[2],   t0.fraction, t0.ent,
						eEyeLevel[0], eEyeLevel[1], eEyeLevel[2], t1.fraction, t1.ent,
						eTop[0],      eTop[1],      eTop[2],      t2.fraction, t2.ent );
				}
			}
			continue;
		}
		//if the enemy is quite far away, not shooting and the bot is not damaged
		if (curenemy < 0 && squaredist > Square(100) && !healthdecrease && !EntityIsShooting(&entinfo))
		{
			//check if we can avoid this enemy
			VectorSubtract(bs->origin, entinfo.origin, dir);
			vectoangles(dir, angles);
			//if the bot isn't in the fov of the enemy
			if (!InFieldOfVision(entinfo.angles, 90, angles)) {
				//update some stuff for this enemy
				BotUpdateBattleInventory(bs, i);
				//if the bot doesn't really want to fight
				if (BotWantsToRetreat(bs)) {
					if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugDecide" ) ) {
						G_Printf( "^1[FindEnemy] cl=%d cand=%d SKIP:retreat dist=%.0f\n",
							bs->client, i, sqrtf( squaredist ) );
					}
					continue;
				}
			}
		}
		//found an enemy
		bs->enemy = entinfo.number;
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugDecide" ) ) {
			G_Printf( "^2[FindEnemy] cl=%d FOUND enemy=%d dist=%.0f hpdec=%d f=%.0f\n",
				bs->client, i, sqrtf( squaredist ), healthdecrease, f );
		}
		if (curenemy >= 0) bs->enemysight_time = FloatTime() - 2;
		else bs->enemysight_time = FloatTime();
		bs->enemysuicide = qfalse;
		bs->enemydeath_time = 0;
		bs->enemyvisible_time = FloatTime();
		return qtrue;
	}
	return qfalse;
}

/*
==================
BotTeamFlagCarrierVisible
==================
*/
int BotTeamFlagCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client)
			continue;
		//
		BotEntityInfo(i, &entinfo);
		//if this player is active
		if (!entinfo.valid)
			continue;
		//if this player is carrying a flag
		if (!EntityCarriesFlag(&entinfo))
			continue;
		//if the flag carrier is not on the same team
		if (!BotSameTeam(bs, i))
			continue;
		//if the flag carrier is not visible
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		//
		return i;
	}
	return -1;
}

/*
==================
BotTeamFlagCarrier
==================
*/
int BotTeamFlagCarrier(bot_state_t *bs) {
	int i;
	aas_entityinfo_t entinfo;

	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client)
			continue;
		//
		BotEntityInfo(i, &entinfo);
		//if this player is active
		if (!entinfo.valid)
			continue;
		//if this player is carrying a flag
		if (!EntityCarriesFlag(&entinfo))
			continue;
		//if the flag carrier is not on the same team
		if (!BotSameTeam(bs, i))
			continue;
		//
		return i;
	}
	return -1;
}

/*
==================
BotEnemyFlagCarrierVisible
==================
*/
int BotEnemyFlagCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client)
			continue;
		//
		BotEntityInfo(i, &entinfo);
		//if this player is active
		if (!entinfo.valid)
			continue;
		//if this player is carrying a flag
		if (!EntityCarriesFlag(&entinfo))
			continue;
		//if the flag carrier is on the same team
		if (BotSameTeam(bs, i))
			continue;
		//if the flag carrier is not visible
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		//
		return i;
	}
	return -1;
}

/*
==================
BotVisibleTeamMatesAndEnemies
==================
*/
void BotVisibleTeamMatesAndEnemies(bot_state_t *bs, int *teammates, int *enemies, float range) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;
	vec3_t dir;

	if (teammates)
		*teammates = 0;
	if (enemies)
		*enemies = 0;
	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client)
			continue;
		//
		BotEntityInfo(i, &entinfo);
		//if this player is active
		if (!entinfo.valid)
			continue;
		//if this player is carrying a flag
		if (!EntityCarriesFlag(&entinfo))
			continue;
		//if not within range
		VectorSubtract(entinfo.origin, bs->origin, dir);
		if (VectorLengthSquared(dir) > Square(range))
			continue;
		//if the flag carrier is not visible
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		//if the flag carrier is on the same team
		if (BotSameTeam(bs, i)) {
			if (teammates)
				(*teammates)++;
		}
		else {
			if (enemies)
				(*enemies)++;
		}
	}
}

#if FEAT_HARVESTER
/*
==================
BotTeamCubeCarrierVisible
==================
*/
int BotTeamCubeCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client) continue;
		//
		BotEntityInfo(i, &entinfo);
		//if this player is active
		if (!entinfo.valid) continue;
		//if this player is carrying a flag
		if (!EntityCarriesCubes(&entinfo)) continue;
		//if the flag carrier is not on the same team
		if (!BotSameTeam(bs, i)) continue;
		//if the flag carrier is not visible
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0) continue;
		//
		return i;
	}
	return -1;
}

/*
==================
BotEnemyCubeCarrierVisible
==================
*/
int BotEnemyCubeCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < level.maxclients; i++) {
		if (i == bs->client)
			continue;
		//
		BotEntityInfo(i, &entinfo);
		//if this player is active
		if (!entinfo.valid)
			continue;
		//if this player is carrying a flag
		if (!EntityCarriesCubes(&entinfo)) continue;
		//if the flag carrier is on the same team
		if (BotSameTeam(bs, i))
			continue;
		//if the flag carrier is not visible
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		//
		return i;
	}
	return -1;
}
#endif

/*
==================
BotAimAtEnemy
==================
*/
void BotAimAtEnemy(bot_state_t *bs) {
	int i, enemyvisible;
	float dist, f, aim_skill, aim_accuracy, aimHeight, speed, reactiontime;
	vec3_t dir, bestorigin, end, start, groundtarget, cmdmove, enemyvelocity;
	vec3_t mins = {-4,-4,-4}, maxs = {4, 4, 4};
	weaponinfo_t wi;
	aas_entityinfo_t entinfo;
	bot_goal_t goal;
	bsp_trace_t trace;
	vec3_t target;

	//if the bot has no enemy
	if (bs->enemy < 0) {
		return;
	}
	//get the enemy entity information
	BotEntityInfo(bs->enemy, &entinfo);
	//if this is not a player (should be an obelisk)
	if (bs->enemy >= MAX_CLIENTS) {
		//if the obelisk is visible
		VectorCopy(entinfo.origin, target);
#if FEAT_OVERLOAD
		// if attacking an obelisk
		if ( bs->enemy == redobelisk.entitynum ||
			bs->enemy == blueobelisk.entitynum ) {
			target[2] += 32;
		}
#endif
		//aim at the obelisk
		VectorSubtract(target, bs->eye, dir);
		vectoangles(dir, bs->ideal_viewangles);
		//set the aim target before trying to attack
		VectorCopy(target, bs->aimtarget);
		return;
	}
	//
	//BotAI_Print(PRT_MESSAGE, "client %d: aiming at client %d\n", bs->entitynum, bs->enemy);
	//
	if ( bs->wiredBotsActive ) {
		aim_skill = WiredBots_ProfileFieldOr( bs, WB_PROFILE_LEAD_SKILL, 0.5f );
		aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
		aimHeight = WiredBots_GetCurrentAttackAimHeight( bs );
	} else {
		aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL, 0, 1);
		aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
		aimHeight = 28.0f;
	}
	//
	if (aim_skill > 0.95) {
		//don't aim too early
		if ( bs->wiredBotsActive ) {
			reactiontime = 0.5f * WiredBots_ProfileFieldOr( bs, WB_PROFILE_REACTION_TIME, 0.5f );
		} else {
			reactiontime = 0.5f * trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
		}
		if (bs->enemysight_time > FloatTime() - reactiontime) return;
		if (bs->teleport_time > FloatTime() - reactiontime) return;
	}

	//get the weapon information
	trap_BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
	//get the weapon specific aim accuracy and or aim skill
	if (wi.number == WP_MACHINEGUN) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_MACHINEGUN, 0, 1);
		}
	}
	else if (wi.number == WP_SHOTGUN) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_SHOTGUN, 0, 1);
		}
	}
	else if (wi.number == WP_GRENADE_LAUNCHER) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
			aim_skill = WiredBots_ProfileFieldOr( bs, WB_PROFILE_LEAD_SKILL, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_GRENADELAUNCHER, 0, 1);
			aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_GRENADELAUNCHER, 0, 1);
		}
	}
	else if (wi.number == WP_ROCKET_LAUNCHER) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
			aim_skill = WiredBots_ProfileFieldOr( bs, WB_PROFILE_LEAD_SKILL, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_ROCKETLAUNCHER, 0, 1);
			aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_ROCKETLAUNCHER, 0, 1);
		}
	}
	else if (wi.number == WP_LIGHTNING_GUN) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_LIGHTNING, 0, 1);
		}
	}
	else if (wi.number == WP_RAILGUN) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_RAILGUN, 0, 1);
		}
	}
	else if (wi.number == WP_PLASMA_RIFLE) {
		if ( bs->wiredBotsActive ) {
			aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
			aim_skill = WiredBots_ProfileFieldOr( bs, WB_PROFILE_LEAD_SKILL, 0.5f );
		} else {
			aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_PLASMAGUN, 0, 1);
			aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_PLASMAGUN, 0, 1);
		}
	}
	//
	if (aim_accuracy <= 0) aim_accuracy = 0.0001f;
	//get the enemy entity information
	BotEntityInfo(bs->enemy, &entinfo);
	//if the enemy is invisible then shoot crappy most of the time
	if (EntityIsInvisible(&entinfo)) {
		if (random() > 0.1) aim_accuracy *= 0.4f;
	}
	//
	VectorSubtract(entinfo.origin, entinfo.lastvisorigin, enemyvelocity);
	VectorScale(enemyvelocity, 1 / entinfo.update_time, enemyvelocity);
	//enemy origin and velocity is remembered every 0.5 seconds
	if (bs->enemyposition_time < FloatTime()) {
		//
		bs->enemyposition_time = FloatTime() + 0.5;
		VectorCopy(enemyvelocity, bs->enemyvelocity);
		VectorCopy(entinfo.origin, bs->enemyorigin);
	}
	//if not extremely skilled
	if (aim_skill < 0.9) {
		VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
		//if the enemy moved a bit
		if (VectorLengthSquared(dir) > Square(48)) {
			//if the enemy changed direction
			if (DotProduct(bs->enemyvelocity, enemyvelocity) < 0) {
				//aim accuracy should be worse now
				aim_accuracy *= 0.7f;
			}
		}
	}
	//check visibility of enemy
	enemyvisible = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy);
	//if the enemy is visible
	if (enemyvisible) {
		//
		VectorCopy(entinfo.origin, bestorigin);
		bestorigin[2] += aimHeight; // per-attack aim height (0=feet/splash, 28=center mass, 36=upper body)
		//get the start point shooting from
		//NOTE: the x and y projectile start offsets are ignored
		VectorCopy(bs->origin, start);
		start[2] += bs->cur_ps.viewheight;
		start[2] += wi.offset[2];
		//
		BotAI_Trace(&trace, start, mins, maxs, bestorigin, bs->entitynum, MASK_SHOT);
		//if the enemy is NOT hit (something obstructed the path)
		if (trace.fraction < 1 && trace.ent != entinfo.number) {
			bestorigin[2] += 16;
		}
		//if it is not an instant hit weapon the bot might want to predict the enemy
		if (wi.speed) {
			//
			VectorSubtract(bestorigin, bs->origin, dir);
			dist = VectorLength(dir);
			VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
			//if the enemy is NOT pretty far away and strafing just small steps left and right
			if (!(dist > 100 && VectorLengthSquared(dir) < Square(32))) {
				//if skilled enough do exact prediction
				if (aim_skill > 0.8 &&
						//if the weapon is ready to fire
						bs->cur_ps.weaponstate == WEAPON_READY) {
					aas_clientmove_t move;
					vec3_t origin;

					VectorSubtract(entinfo.origin, bs->origin, dir);
					//distance towards the enemy
					dist = VectorLength(dir);
					//direction the enemy is moving in
					VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
					//
					VectorScale(dir, 1 / entinfo.update_time, dir);
					//
					VectorCopy(entinfo.origin, origin);
					origin[2] += 1;
					//
					VectorClear(cmdmove);
					//AAS_ClearShownDebugLines();
					trap_AAS_PredictClientMovement(&move, bs->enemy, origin,
														PRESENCE_CROUCH, qfalse,
														dir, cmdmove, 0,
														dist * 10 / wi.speed, 0.1f, 0, 0, qfalse);
					VectorCopy(move.endpos, bestorigin);
					bestorigin[2] += aimHeight; // restore per-attack aim height after prediction overwrites Z
					//BotAI_Print(PRT_MESSAGE, "%1.1f predicted speed = %f, frames = %f\n", FloatTime(), VectorLength(dir), dist * 10 / wi.speed);
				}
				//if not that skilled do linear prediction
				else if (aim_skill > 0.4) {
					VectorSubtract(entinfo.origin, bs->origin, dir);
					//distance towards the enemy
					dist = VectorLength(dir);
					//direction the enemy is moving in
					VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
					dir[2] = 0;
					//
					speed = VectorNormalize(dir) / entinfo.update_time;
					//botimport.Print(PRT_MESSAGE, "speed = %f, wi->speed = %f\n", speed, wi->speed);
					//best spot to aim at (apply per-attack aim height after prediction overwrites Z)
					VectorMA(entinfo.origin, (dist / wi.speed) * speed, dir, bestorigin);
					bestorigin[2] += aimHeight;
				}
			}
		}
		//if the projectile does radial damage
		if (aim_skill > 0.6 && wi.proj.damagetype & DAMAGETYPE_RADIAL) {
			//if the enemy isn't standing significantly higher than the bot
			if (entinfo.origin[2] < bs->origin[2] + 16) {
				//try to aim at the ground in front of the enemy
				VectorCopy(entinfo.origin, end);
				end[2] -= 64;
				BotAI_Trace(&trace, entinfo.origin, NULL, NULL, end, entinfo.number, MASK_SHOT);
				//
				VectorCopy(bestorigin, groundtarget);
				if (trace.startsolid) groundtarget[2] = entinfo.origin[2] - 16;
				else groundtarget[2] = trace.endpos[2] - 8;
				//trace a line from projectile start to ground target
				BotAI_Trace(&trace, start, NULL, NULL, groundtarget, bs->entitynum, MASK_SHOT);
				//if hitpoint is not vertically too far from the ground target
				if (fabs(trace.endpos[2] - groundtarget[2]) < 50) {
					VectorSubtract(trace.endpos, groundtarget, dir);
					//if the hitpoint is near enough the ground target
					if (VectorLengthSquared(dir) < Square(60)) {
						VectorSubtract(trace.endpos, start, dir);
						//if the hitpoint is far enough from the bot
						if (VectorLengthSquared(dir) > Square(100)) {
							//check if the bot is visible from the ground target
							trace.endpos[2] += 1;
							BotAI_Trace(&trace, trace.endpos, NULL, NULL, entinfo.origin, entinfo.number, MASK_SHOT);
							if (trace.fraction >= 1) {
								//botimport.Print(PRT_MESSAGE, "%1.1f aiming at ground\n", AAS_Time());
								VectorCopy(groundtarget, bestorigin);
							}
						}
					}
				}
			}
		}
		bestorigin[0] += 20 * crandom() * (1 - aim_accuracy);
		bestorigin[1] += 20 * crandom() * (1 - aim_accuracy);
		bestorigin[2] += 10 * crandom() * (1 - aim_accuracy);
	}
	else {
		//
		VectorCopy(bs->lastenemyorigin, bestorigin);
		bestorigin[2] += aimHeight; // per-attack aim height
		//if the bot is skilled enough
		if (aim_skill > 0.5) {
			//do prediction shots around corners
			if (wi.number == WP_ROCKET_LAUNCHER ||
				wi.number == WP_GRENADE_LAUNCHER) {
				//create the chase goal
				goal.entitynum = bs->client;
				goal.areanum = bs->areanum;
				VectorCopy(bs->eye, goal.origin);
				VectorSet(goal.mins, -8, -8, -8);
				VectorSet(goal.maxs, 8, 8, 8);
				//
				if (trap_BotPredictVisiblePosition(bs->lastenemyorigin, bs->lastenemyareanum, &goal, TFL_DEFAULT, target)) {
					VectorSubtract(target, bs->eye, dir);
					if (VectorLengthSquared(dir) > Square(80)) {
						VectorCopy(target, bestorigin);
						bestorigin[2] -= 20;
					}
				}
				aim_accuracy = 1;
			}
		}
	}
	//
	if (enemyvisible) {
		BotAI_Trace(&trace, bs->eye, NULL, NULL, bestorigin, bs->entitynum, MASK_SHOT);
		VectorCopy(trace.endpos, bs->aimtarget);
	}
	else {
		VectorCopy(bestorigin, bs->aimtarget);
	}
	//get aim direction
	VectorSubtract(bestorigin, bs->eye, dir);
	//
	if (wi.number == WP_MACHINEGUN ||
		wi.number == WP_SHOTGUN ||
		wi.number == WP_LIGHTNING_GUN ||
		wi.number == WP_RAILGUN) {
		//distance towards the enemy
		dist = VectorLength(dir);
		if (dist > 150) dist = 150;
		f = 0.6 + dist / 150 * 0.4;
		aim_accuracy *= f;
	}
	//add some random stuff to the aim direction depending on the aim accuracy
	if (aim_accuracy < 0.8) {
		VectorNormalize(dir);
		for (i = 0; i < 3; i++) dir[i] += 0.3 * crandom() * (1 - aim_accuracy);
	}
	//set the ideal view angles
	vectoangles(dir, bs->ideal_viewangles);
	if ( trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
		static int s_aimLogTick[MAX_CLIENTS];
		if ( ++s_aimLogTick[bs->client] % 30 == 0 ) {
			G_Printf( "^6[BotAim] client=%d lua=%d aim_skill=%.3f aim_accuracy=%.3f "
				"visible=%d bestorigin=(%.1f %.1f %.1f) ideal=(%.1f %.1f)\n",
				bs->client, bs->wiredBotsActive,
				aim_skill, aim_accuracy,
				enemyvisible,
				bestorigin[0], bestorigin[1], bestorigin[2],
				bs->ideal_viewangles[PITCH], bs->ideal_viewangles[YAW] );
		}
	}
	//take the weapon spread into account for lower skilled bots
	// Lua bots: skip vspread/hspread noise — g_syscalls.c sets these to game-world
	// units (e.g. 600 for shotgun) not botlib-style "degrees from middle", so the
	// formula would inject ±1700° of noise per frame. Direction-vector noise on the
	// lines above already handles inaccuracy for Lua bots.
	if ( !bs->wiredBotsActive ) {
		bs->ideal_viewangles[PITCH] += 6 * wi.vspread * crandom() * (1 - aim_accuracy);
		bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
		bs->ideal_viewangles[YAW] += 6 * wi.hspread * crandom() * (1 - aim_accuracy);
		bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
	}
	//if the bots should be really challenging
	if (bot_challenge.integer) {
		//if the bot is really accurate and has the enemy in view for some time
		if (aim_accuracy > 0.9 && bs->enemysight_time < FloatTime() - 1) {
			//set the view angles directly
			if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
			VectorCopy(bs->ideal_viewangles, bs->viewangles);
			trap_EA_View(bs->client, bs->viewangles);
		}
	}
}

/*
==================
BotCheckAttack
==================
*/
void BotCheckAttack(bot_state_t *bs) {
	float points, reactiontime, fov, firethrottle;
	int attackentity;
	bsp_trace_t bsptrace;
	//float selfpreservation;
	vec3_t forward, right, start, end, dir, angles;
	weaponinfo_t wi;
	bsp_trace_t trace;
	aas_entityinfo_t entinfo;
	vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

	attackentity = bs->enemy;
	if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
		static int s_caLogTick[MAX_CLIENTS];
		if ( ++s_caLogTick[bs->client] % 30 == 0 ) {
			G_Printf( "^5[CheckAttack] cl=%d enemy=%d wpn=%d ammo=%d "
				"sight_dt=%.2f tele_dt=%.2f wpnchg_dt=%.2f ftwait=%.2f ftshoot=%.2f\n",
				bs->client, attackentity, bs->weaponnum,
				(bs->weaponnum >= 0 && bs->weaponnum < MAX_WEAPONS)
					? bs->cur_ps.ammo[bs->weaponnum] : -1,
				FloatTime() - bs->enemysight_time,
				FloatTime() - bs->teleport_time,
				FloatTime() - bs->weaponchange_time,
				bs->firethrottlewait_time - FloatTime(),
				bs->firethrottleshoot_time - FloatTime() );
		}
	}
	//
	BotEntityInfo(attackentity, &entinfo);
	// if not attacking a player
	if (attackentity >= MAX_CLIENTS) {
#if FEAT_OVERLOAD
		// if attacking an obelisk
		if ( entinfo.number == redobelisk.entitynum ||
			entinfo.number == blueobelisk.entitynum ) {
			// if obelisk is respawning return
			if ( g_entities[entinfo.number].activator &&
				g_entities[entinfo.number].activator->s.frame == 2 ) {
				return;
			}
		}
#endif
	}
	//
	if ( bs->wiredBotsActive ) {
		reactiontime = WiredBots_ProfileFieldOr( bs, WB_PROFILE_REACTION_TIME, 0.5f );
	} else {
		reactiontime = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
	}
	if (bs->enemysight_time > FloatTime() - reactiontime) {
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
			static int s_rLog[MAX_CLIENTS];
			if ( ++s_rLog[bs->client] % 30 == 0 )
				G_Printf( "^1[CheckAttack] cl=%d BLOCKED:reaction rt=%.2f sdt=%.2f\n",
					bs->client, reactiontime, FloatTime() - bs->enemysight_time );
		}
		return;
	}
	if (bs->teleport_time > FloatTime() - reactiontime) {
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
			static int s_tpLog[MAX_CLIENTS];
			if ( ++s_tpLog[bs->client] % 30 == 0 )
				G_Printf( "^1[CheckAttack] cl=%d BLOCKED:teleport tdt=%.2f\n",
					bs->client, FloatTime() - bs->teleport_time );
		}
		return;
	}
	//if changing weapons
	if (bs->weaponchange_time > FloatTime() - 0.1) {
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
			static int s_wcLog[MAX_CLIENTS];
			if ( ++s_wcLog[bs->client] % 30 == 0 )
				G_Printf( "^1[CheckAttack] cl=%d BLOCKED:wpnchange wdt=%.2f\n",
					bs->client, FloatTime() - bs->weaponchange_time );
		}
		return;
	}
	//check fire throttle characteristic
	if (bs->firethrottlewait_time > FloatTime()) {
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
			static int s_ftLog[MAX_CLIENTS];
			if ( ++s_ftLog[bs->client] % 30 == 0 )
				G_Printf( "^1[CheckAttack] cl=%d BLOCKED:firethrottle wait=%.2f\n",
					bs->client, bs->firethrottlewait_time - FloatTime() );
		}
		return;
	}
	if ( bs->wiredBotsActive ) {
		firethrottle = WiredBots_ProfileFieldOr( bs, WB_PROFILE_FIRETHROTTLE, 0.5f );
	} else {
		firethrottle = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_FIRETHROTTLE, 0, 1);
	}
	if (bs->firethrottleshoot_time < FloatTime()) {
		if (random() > firethrottle) {
			bs->firethrottlewait_time = FloatTime() + firethrottle;
			bs->firethrottleshoot_time = 0;
		}
		else {
			bs->firethrottleshoot_time = FloatTime() + 1 - firethrottle;
			bs->firethrottlewait_time = 0;
		}
	}
	//
	//
	VectorSubtract(bs->aimtarget, bs->eye, dir);
	//
	if (bs->weaponnum == WP_GAUNTLET) {
		if (VectorLengthSquared(dir) > Square(60)) {
			if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
				static int s_gLog[MAX_CLIENTS];
				if ( ++s_gLog[bs->client] % 30 == 0 )
					G_Printf( "^1[CheckAttack] cl=%d BLOCKED:gauntlet dist=%.0f\n",
						bs->client, VectorLength(dir) );
			}
			return;
		}
	}
	if (VectorLengthSquared(dir) < Square(100))
		fov = 120;
	else
		fov = 50;
	//
	vectoangles(dir, angles);
	if (!InFieldOfVision(bs->viewangles, fov, angles)) {
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
			static int s_fovMissLog[MAX_CLIENTS];
			if ( ++s_fovMissLog[bs->client] % 30 == 0 ) {
				G_Printf( "^1[Attack] cl=%d FOV_MISS fov=%.0f view=(%.1f %.1f) aim=(%.1f %.1f)\n",
					bs->client, fov,
					bs->viewangles[PITCH], bs->viewangles[YAW],
					angles[PITCH], angles[YAW] );
			}
		}
		return;
	}
	// MASK_OPAQUE: only solid/lava/slime surfaces block shots — playerclip (invisible)
	// movement barriers must not gate firing; they are not real walls.
	BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->aimtarget, bs->client, MASK_OPAQUE);
	if (bsptrace.fraction < 1 && bsptrace.ent != attackentity) {
		if ( bs->wiredBotsActive && trap_Cvar_VariableIntegerValue( "sv_botDebugAim" ) ) {
			static int s_fgLog[MAX_CLIENTS];
			if ( ++s_fgLog[bs->client] % 30 == 0 ) {
				G_Printf( "^1[Attack] cl=%d FIREGATE_BLOCKED "
					"eye=(%.1f %.1f %.1f) aim=(%.1f %.1f %.1f) "
					"frac=%.3f ent=%d (enemy=%d)\n",
					bs->client,
					bs->eye[0], bs->eye[1], bs->eye[2],
					bs->aimtarget[0], bs->aimtarget[1], bs->aimtarget[2],
					bsptrace.fraction, bsptrace.ent, attackentity );
			}
		}
		return;
	}

	//get the weapon info
	trap_BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
	//get the start point shooting from
	VectorCopy(bs->origin, start);
	start[2] += bs->cur_ps.viewheight;
	AngleVectors(bs->viewangles, forward, right, NULL);
	start[0] += forward[0] * wi.offset[0] + right[0] * wi.offset[1];
	start[1] += forward[1] * wi.offset[0] + right[1] * wi.offset[1];
	start[2] += forward[2] * wi.offset[0] + right[2] * wi.offset[1] + wi.offset[2];
	//end point aiming at
	VectorMA(start, 1000, forward, end);
	//a little back to make sure not inside a very close enemy
	VectorMA(start, -12, forward, start);
	BotAI_Trace(&trace, start, mins, maxs, end, bs->entitynum, MASK_SHOT);
	//if the entity is a client
	if (trace.ent >= 0 && trace.ent < MAX_CLIENTS) {
		if (trace.ent != attackentity) {
			//if a teammate is hit
			if (BotSameTeam(bs, trace.ent))
				return;
		}
	}
	//if won't hit the enemy or not attacking a player (obelisk)
	if (trace.ent != attackentity || attackentity >= MAX_CLIENTS) {
		//if the projectile does radial damage
		if (wi.proj.damagetype & DAMAGETYPE_RADIAL) {
			if (trace.fraction * 1000 < wi.proj.radius) {
				points = (wi.proj.damage - 0.5 * trace.fraction * 1000) * 0.5;
				if (points > 0) {
					return;
				}
			}
			//FIXME: check if a teammate gets radial damage
		}
	}
	//if fire has to be release to activate weapon
	if (wi.flags & WFL_FIRERELEASED) {
		if (bs->flags & BFL_ATTACKED) {
			trap_EA_Attack(bs->client);
		}
	}
	else {
		float distSq = VectorLengthSquared(dir);
		qboolean useAlt = qfalse;

		// shotgun double-blast: use alt-fire at close range
		if ( bs->cur_ps.weapon == WP_SHOTGUN && distSq < Square(128) ) {
			useAlt = qtrue;
		}
		// machinegun: always use primary — burst alt-fire requires the button to be held
		// continuously across pmove frames, which bots cannot reliably do; primary fire
		// gives a consistent 100ms-per-shot cadence that is more effective for bots.
		// rocket launcher mortar: use alt-fire at close range to launch targets
		else if ( bs->cur_ps.weapon == WP_ROCKET_LAUNCHER && distSq < Square(300) ) {
			useAlt = qtrue;
		}
		// lightning gun chain arc: use alt-fire when 2+ enemies are clustered
		// chain arc hits primary target + arcs to a nearby secondary within 192u
		else if ( bs->cur_ps.weapon == WP_LIGHTNING_GUN && distSq < Square(LIGHTNING_RANGE) ) {
			// check if any other visible enemy is within arc range of the primary target
			int k;
			aas_entityinfo_t enemyInfo;
			vec3_t enemyOrigin;

			BotEntityInfo(attackentity, &enemyInfo);
			VectorCopy(enemyInfo.origin, enemyOrigin);

			for ( k = 0; k < level.maxclients; k++ ) {
				aas_entityinfo_t candidateInfo;
				vec3_t diff;
				float candDistSq;
				bsp_trace_t losTrace;

				if ( k == bs->client ) continue;       // skip self
				if ( k == attackentity ) continue;      // skip primary target

				BotEntityInfo(k, &candidateInfo);
				if ( !candidateInfo.valid ) continue;
				if ( EntityIsDead(&candidateInfo) ) continue;
				if ( BotSameTeam(bs, k) ) continue;

				// candidate must be within arc range of primary target
				VectorSubtract(candidateInfo.origin, enemyOrigin, diff);
				candDistSq = VectorLengthSquared(diff);
				if ( candDistSq > Square(LG_CHAIN_ARC_RANGE) ) continue;

				// LOS check: primary target to candidate
				BotAI_Trace(&losTrace, enemyOrigin, NULL, NULL,
					candidateInfo.origin, attackentity, CONTENTS_SOLID);
				if ( losTrace.ent != k ) continue;

				// found a valid arc target — use chain arc alt-fire
				useAlt = qtrue;
				break;
			}
		}

		if ( useAlt ) {
			trap_EA_Action(bs->client, ACTION_ATTACK_SEC);
		} else {
			trap_EA_Attack(bs->client);
		}
	}
	bs->flags ^= BFL_ATTACKED;
}

/*
==================
BotMapScripts
==================
*/
void BotMapScripts(bot_state_t *bs) {
	char info[1024];
	char mapname[128];
	int i, shootbutton;
	float aim_accuracy;
	aas_entityinfo_t entinfo;
	vec3_t dir;

	trap_GetServerinfo(info, sizeof(info));

	Q_strncpyz(mapname, Info_ValueForKey( info, "mapname" ), sizeof(mapname));

	if (!Q_stricmp(mapname, "q3tourney6") || !Q_stricmp(mapname, "q3tourney6_ctf") || !Q_stricmp(mapname, "mpq3tourney6")) {
		vec3_t mins = {694, 200, 480}, maxs = {968, 472, 680};
		vec3_t buttonorg = {304, 352, 920};
		//NOTE: NEVER use the func_bobbing in q3tourney6
		bs->tfl &= ~TFL_FUNCBOB;
		//crush area is higher in mpq3tourney6
		if (!Q_stricmp(mapname, "mpq3tourney6")) {
			mins[2] += 64;
			maxs[2] += 64;
		}
		//if the bot is in the bounding box of the crush area
		if (bs->origin[0] > mins[0] && bs->origin[0] < maxs[0]) {
			if (bs->origin[1] > mins[1] && bs->origin[1] < maxs[1]) {
				if (bs->origin[2] > mins[2] && bs->origin[2] < maxs[2]) {
					return;
				}
			}
		}
		shootbutton = qfalse;
		//if an enemy is in the bounding box then shoot the button
		for (i = 0; i < level.maxclients; i++) {

			if (i == bs->client) continue;
			//
			BotEntityInfo(i, &entinfo);
			//
			if (!entinfo.valid) continue;
			//if the enemy isn't dead and the enemy isn't the bot self
			if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum) continue;
			//
			if (entinfo.origin[0] > mins[0] && entinfo.origin[0] < maxs[0]) {
				if (entinfo.origin[1] > mins[1] && entinfo.origin[1] < maxs[1]) {
					if (entinfo.origin[2] > mins[2] && entinfo.origin[2] < maxs[2]) {
						//if there's a team mate below the crusher
						if (BotSameTeam(bs, i)) {
							shootbutton = qfalse;
							break;
						}
						else if (gametype < GT_CTF || bs->enemy == i) {
							shootbutton = qtrue;
						}
					}
				}
			}
		}
		if (shootbutton) {
			bs->flags |= BFL_IDEALVIEWSET;
			VectorSubtract(buttonorg, bs->eye, dir);
			vectoangles(dir, bs->ideal_viewangles);
			if ( bs->wiredBotsActive ) {
				aim_accuracy = WiredBots_ProfileFieldOr( bs, WB_PROFILE_ACCURACY, 0.5f );
			} else {
				aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
			}
			bs->ideal_viewangles[PITCH] += 8 * crandom() * (1 - aim_accuracy);
			bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
			bs->ideal_viewangles[YAW] += 8 * crandom() * (1 - aim_accuracy);
			bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
			//
			if (InFieldOfVision(bs->viewangles, 20, bs->ideal_viewangles)) {
				trap_EA_Attack(bs->client);
			}
		}
	}
}

/*
==================
BotSetMovedir
==================
*/
static vec3_t VEC_UP		= {0, -1,  0};
static vec3_t MOVEDIR_UP	= {0,  0,  1};
static vec3_t VEC_DOWN		= {0, -2,  0};
static vec3_t MOVEDIR_DOWN	= {0,  0, -1};

void BotSetMovedir(vec3_t angles, vec3_t movedir) {
	if (VectorCompare(angles, VEC_UP)) {
		VectorCopy(MOVEDIR_UP, movedir);
	}
	else if (VectorCompare(angles, VEC_DOWN)) {
		VectorCopy(MOVEDIR_DOWN, movedir);
	}
	else {
		AngleVectors(angles, movedir, NULL, NULL);
	}
}

/*
==================
BotModelMinsMaxs

this is ugly
==================
*/
int BotModelMinsMaxs(int modelindex, int eType, int contents, vec3_t mins, vec3_t maxs) {
	gentity_t *ent;
	int i;

	ent = &g_entities[0];
	for (i = 0; i < level.num_entities; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( eType && ent->s.eType != eType) {
			continue;
		}
		if ( contents && ent->r.contents != contents) {
			continue;
		}
		if (ent->s.modelindex == modelindex) {
			if (mins)
				VectorAdd(ent->r.currentOrigin, ent->r.mins, mins);
			if (maxs)
				VectorAdd(ent->r.currentOrigin, ent->r.maxs, maxs);
			return i;
		}
	}
	if (mins)
		VectorClear(mins);
	if (maxs)
		VectorClear(maxs);
	return 0;
}

/*
==================
BotFuncButtonGoal
==================
*/
int BotFuncButtonActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
	int i, areas[10], numareas, modelindex, entitynum;
	char model[128];
	float lip, dist, health, angle;
	vec3_t size, start, end, mins, maxs, angles, points[10];
	vec3_t movedir, origin, goalorigin, bboxmins, bboxmaxs;
	vec3_t extramins = {1, 1, 1}, extramaxs = {-1, -1, -1};
	bsp_trace_t bsptrace;

	activategoal->shoot = qfalse;
	VectorClear(activategoal->target);
	//create a bot goal towards the button
	trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
	if (!*model)
		return qfalse;
	modelindex = atoi(model+1);
	if (!modelindex)
		return qfalse;
	entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
	//get the lip of the button
	trap_AAS_FloatForBSPEpairKey(bspent, "lip", &lip);
	if (!lip) lip = 4;
	//get the move direction from the angle
	trap_AAS_FloatForBSPEpairKey(bspent, "angle", &angle);
	VectorSet(angles, 0, angle, 0);
	BotSetMovedir(angles, movedir);
	//button size
	VectorSubtract(maxs, mins, size);
	//button origin
	VectorAdd(mins, maxs, origin);
	VectorScale(origin, 0.5, origin);
	//touch distance of the button
	dist = fabs(movedir[0]) * size[0] + fabs(movedir[1]) * size[1] + fabs(movedir[2]) * size[2];
	dist *= 0.5;
	//
	trap_AAS_FloatForBSPEpairKey(bspent, "health", &health);
	//if the button is shootable
	if (health) {
		//calculate the shoot target
		VectorMA(origin, -dist, movedir, goalorigin);
		//
		VectorCopy(goalorigin, activategoal->target);
		activategoal->shoot = qtrue;
		//
		BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, goalorigin, bs->entitynum, MASK_SHOT);
		// if the button is visible from the current position
		if (bsptrace.fraction >= 1.0 || bsptrace.ent == entitynum) {
			//
			activategoal->goal.entitynum = entitynum; //NOTE: this is the entity number of the shootable button
			activategoal->goal.number = 0;
			activategoal->goal.flags = 0;
			VectorCopy(bs->origin, activategoal->goal.origin);
			activategoal->goal.areanum = bs->areanum;
			VectorSet(activategoal->goal.mins, -8, -8, -8);
			VectorSet(activategoal->goal.maxs, 8, 8, 8);
			//
			return qtrue;
		}
		else {
			//create a goal from where the button is visible and shoot at the button from there
			//add bounding box size to the dist
			trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
			for (i = 0; i < 3; i++) {
				if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
				else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
			}
			//calculate the goal origin
			VectorMA(origin, -dist, movedir, goalorigin);
			//
			VectorCopy(goalorigin, start);
			start[2] += 24;
			VectorCopy(start, end);
			end[2] -= 512;
			numareas = trap_AAS_TraceAreas(start, end, areas, points, 10);
			//
			for (i = numareas-1; i >= 0; i--) {
				if (trap_AAS_AreaReachability(areas[i])) {
					break;
				}
			}
			if (i < 0) {
				// FIXME: trace forward and maybe in other directions to find a valid area
			}
			if (i >= 0) {
				//
				VectorCopy(points[i], activategoal->goal.origin);
				activategoal->goal.areanum = areas[i];
				VectorSet(activategoal->goal.mins, 8, 8, 8);
				VectorSet(activategoal->goal.maxs, -8, -8, -8);
				//
				for (i = 0; i < 3; i++)
				{
					if (movedir[i] < 0) activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
					else activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
				} //end for
				//
				activategoal->goal.entitynum = entitynum;
				activategoal->goal.number = 0;
				activategoal->goal.flags = 0;
				return qtrue;
			}
		}
		return qfalse;
	}
	else {
		//add bounding box size to the dist
		trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
		for (i = 0; i < 3; i++) {
			if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
			else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
		}
		//calculate the goal origin
		VectorMA(origin, -dist, movedir, goalorigin);
		//
		VectorCopy(goalorigin, start);
		start[2] += 24;
		VectorCopy(start, end);
		end[2] -= 100;
		numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
		//
		for (i = 0; i < numareas; i++) {
			if (trap_AAS_AreaReachability(areas[i])) {
				break;
			}
		}
		if (i < numareas) {
			//
			VectorCopy(origin, activategoal->goal.origin);
			activategoal->goal.areanum = areas[i];
			VectorSubtract(mins, origin, activategoal->goal.mins);
			VectorSubtract(maxs, origin, activategoal->goal.maxs);
			//
			for (i = 0; i < 3; i++)
			{
				if (movedir[i] < 0) activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
				else activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
			} //end for
			//
			activategoal->goal.entitynum = entitynum;
			activategoal->goal.number = 0;
			activategoal->goal.flags = 0;
			return qtrue;
		}
	}
	return qfalse;
}

/*
==================
BotFuncDoorGoal
==================
*/
int BotFuncDoorActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
	int modelindex, entitynum;
	char model[MAX_INFO_STRING];
	vec3_t mins, maxs, origin;

	//shoot at the shootable door
	trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
	if (!*model)
		return qfalse;
	modelindex = atoi(model+1);
	if (!modelindex)
		return qfalse;
	entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
	//door origin
	VectorAdd(mins, maxs, origin);
	VectorScale(origin, 0.5, origin);
	VectorCopy(origin, activategoal->target);
	activategoal->shoot = qtrue;
	//
	activategoal->goal.entitynum = entitynum; //NOTE: this is the entity number of the shootable door
	activategoal->goal.number = 0;
	activategoal->goal.flags = 0;
	VectorCopy(bs->origin, activategoal->goal.origin);
	activategoal->goal.areanum = bs->areanum;
	VectorSet(activategoal->goal.mins, -8, -8, -8);
	VectorSet(activategoal->goal.maxs, 8, 8, 8);
	return qtrue;
}

/*
==================
BotTriggerMultipleGoal
==================
*/
int BotTriggerMultipleActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
	int i, areas[10], numareas, modelindex, entitynum;
	char model[128];
	vec3_t start, end, mins, maxs;
	vec3_t origin, goalorigin;

	activategoal->shoot = qfalse;
	VectorClear(activategoal->target);
	//create a bot goal towards the trigger
	trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
	if (!*model)
		return qfalse;
	modelindex = atoi(model+1);
	if (!modelindex)
		return qfalse;
	entitynum = BotModelMinsMaxs(modelindex, 0, CONTENTS_TRIGGER, mins, maxs);
	//trigger origin
	VectorAdd(mins, maxs, origin);
	VectorScale(origin, 0.5, origin);
	VectorCopy(origin, goalorigin);
	//
	VectorCopy(goalorigin, start);
	start[2] += 24;
	VectorCopy(start, end);
	end[2] -= 100;
	numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
	//
	for (i = 0; i < numareas; i++) {
		if (trap_AAS_AreaReachability(areas[i])) {
			break;
		}
	}
	if (i < numareas) {
		VectorCopy(origin, activategoal->goal.origin);
		activategoal->goal.areanum = areas[i];
		VectorSubtract(mins, origin, activategoal->goal.mins);
		VectorSubtract(maxs, origin, activategoal->goal.maxs);
		//
		activategoal->goal.entitynum = entitynum;
		activategoal->goal.number = 0;
		activategoal->goal.flags = 0;
		return qtrue;
	}
	return qfalse;
}

/*
==================
BotPopFromActivateGoalStack
==================
*/
int BotPopFromActivateGoalStack(bot_state_t *bs) {
	if (!bs->activatestack)
		return qfalse;
	BotEnableActivateGoalAreas(bs->activatestack, qtrue);
	bs->activatestack->inuse = qfalse;
	bs->activatestack->justused_time = FloatTime();
	bs->activatestack = bs->activatestack->next;
	return qtrue;
}

/*
==================
BotPushOntoActivateGoalStack
==================
*/
int BotPushOntoActivateGoalStack(bot_state_t *bs, bot_activategoal_t *activategoal) {
	int i, best;
	float besttime;

	best = -1;
	besttime = FloatTime() + 9999;
	//
	for (i = 0; i < MAX_ACTIVATESTACK; i++) {
		if (!bs->activategoalheap[i].inuse) {
			if (bs->activategoalheap[i].justused_time < besttime) {
				besttime = bs->activategoalheap[i].justused_time;
				best = i;
			}
		}
	}
	if (best != -1) {
		memcpy(&bs->activategoalheap[best], activategoal, sizeof(bot_activategoal_t));
		bs->activategoalheap[best].inuse = qtrue;
		bs->activategoalheap[best].next = bs->activatestack;
		bs->activatestack = &bs->activategoalheap[best];
		return qtrue;
	}
	return qfalse;
}

/*
==================
BotClearActivateGoalStack
==================
*/
void BotClearActivateGoalStack(bot_state_t *bs) {
	while(bs->activatestack)
		BotPopFromActivateGoalStack(bs);
}

/*
==================
BotEnableActivateGoalAreas
==================
*/
void BotEnableActivateGoalAreas(bot_activategoal_t *activategoal, int enable) {
	int i;

	if (activategoal->areasdisabled == !enable)
		return;
	for (i = 0; i < activategoal->numareas; i++)
		trap_AAS_EnableRoutingArea( activategoal->areas[i], enable );
	activategoal->areasdisabled = !enable;
}

/*
==================
BotIsGoingToActivateEntity
==================
*/
int BotIsGoingToActivateEntity(bot_state_t *bs, int entitynum) {
	bot_activategoal_t *a;
	int i;

	for (a = bs->activatestack; a; a = a->next) {
		if (a->time < FloatTime())
			continue;
		if (a->goal.entitynum == entitynum)
			return qtrue;
	}
	for (i = 0; i < MAX_ACTIVATESTACK; i++) {
		if (bs->activategoalheap[i].inuse)
			continue;
		//
		if (bs->activategoalheap[i].goal.entitynum == entitynum) {
			// if the bot went for this goal less than 2 seconds ago
			if (bs->activategoalheap[i].justused_time > FloatTime() - 2)
				return qtrue;
		}
	}
	return qfalse;
}

/*
==================
BotGetActivateGoal

  returns the number of the bsp entity to activate
  goal->entitynum will be set to the game entity to activate
==================
*/
//#define OBSTACLEDEBUG

int BotGetActivateGoal(bot_state_t *bs, int entitynum, bot_activategoal_t *activategoal) {
	int i, ent, cur_entities[10], spawnflags, modelindex, areas[MAX_ACTIVATEAREAS*2], numareas, t;
	char model[MAX_INFO_STRING], tmpmodel[128];
	char target[128], classname[128];
	float health;
	char targetname[10][128];
	aas_entityinfo_t entinfo;
	aas_areainfo_t areainfo;
	vec3_t origin, absmins, absmaxs;

	memset(activategoal, 0, sizeof(bot_activategoal_t));
	BotEntityInfo(entitynum, &entinfo);
	Com_sprintf(model, sizeof( model ), "*%d", entinfo.modelindex);
	for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
		if (!trap_AAS_ValueForBSPEpairKey(ent, "model", tmpmodel, sizeof(tmpmodel))) continue;
		if (!strcmp(model, tmpmodel)) break;
	}
	if (!ent) {
		BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity found with model %s\n", model);
		return 0;
	}
	trap_AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname));
	if (!*classname) {
		BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model %s has no classname\n", model);
		return 0;
	}
	//if it is a door
	if (!strcmp(classname, "func_door")) {
		if (trap_AAS_FloatForBSPEpairKey(ent, "health", &health)) {
			//if the door has health then the door must be shot to open
			if (health) {
				BotFuncDoorActivateGoal(bs, ent, activategoal);
				return ent;
			}
		}
		//
		trap_AAS_IntForBSPEpairKey(ent, "spawnflags", &spawnflags);
		// if the door starts open then just wait for the door to return
		if ( spawnflags & 1 )
			return 0;
		//get the door origin
		if (!trap_AAS_VectorForBSPEpairKey(ent, "origin", origin)) {
			VectorClear(origin);
		}
		//if the door is open or opening already
		if (!VectorCompare(origin, entinfo.origin))
			return 0;
		// store all the areas the door is in
		trap_AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model));
		if (*model) {
			modelindex = atoi(model+1);
			if (modelindex) {
				BotModelMinsMaxs(modelindex, ET_MOVER, 0, absmins, absmaxs);
				//
				numareas = trap_AAS_BBoxAreas(absmins, absmaxs, areas, MAX_ACTIVATEAREAS*2);
				// store the areas with reachabilities first
				for (i = 0; i < numareas; i++) {
					if (activategoal->numareas >= MAX_ACTIVATEAREAS)
						break;
					if ( !trap_AAS_AreaReachability(areas[i]) ) {
						continue;
					}
					trap_AAS_AreaInfo(areas[i], &areainfo);
					if (areainfo.contents & AREACONTENTS_MOVER) {
						activategoal->areas[activategoal->numareas++] = areas[i];
					}
				}
				// store any remaining areas
				for (i = 0; i < numareas; i++) {
					if (activategoal->numareas >= MAX_ACTIVATEAREAS)
						break;
					if ( trap_AAS_AreaReachability(areas[i]) ) {
						continue;
					}
					trap_AAS_AreaInfo(areas[i], &areainfo);
					if (areainfo.contents & AREACONTENTS_MOVER) {
						activategoal->areas[activategoal->numareas++] = areas[i];
					}
				}
			}
		}
	}
	// if the bot is blocked by or standing on top of a button
	if (!strcmp(classname, "func_button")) {
		return 0;
	}
	// get the targetname so we can find an entity with a matching target
	if (!trap_AAS_ValueForBSPEpairKey(ent, "targetname", targetname[0], sizeof(targetname[0]))) {
		if (bot_developer.integer) {
			BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model \"%s\" has no targetname\n", model);
		}
		return 0;
	}
	// allow tree-like activation
	cur_entities[0] = trap_AAS_NextBSPEntity(0);
	for (i = 0; i >= 0 && i < 10;) {
		for (ent = cur_entities[i]; ent; ent = trap_AAS_NextBSPEntity(ent)) {
			if (!trap_AAS_ValueForBSPEpairKey(ent, "target", target, sizeof(target))) continue;
			if (!strcmp(targetname[i], target)) {
				cur_entities[i] = trap_AAS_NextBSPEntity(ent);
				break;
			}
		}
		if (!ent) {
			if (bot_developer.integer) {
				BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity with target \"%s\"\n", targetname[i]);
			}
			i--;
			continue;
		}
		if (!trap_AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname))) {
			if (bot_developer.integer) {
				BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with target \"%s\" has no classname\n", targetname[i]);
			}
			continue;
		}
		// BSP button model
		if (!strcmp(classname, "func_button")) {
			//
			if (!BotFuncButtonActivateGoal(bs, ent, activategoal))
				continue;
			// if the bot tries to activate this button already
			if ( bs->activatestack && bs->activatestack->inuse &&
				 bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
				 bs->activatestack->time > FloatTime() &&
				 bs->activatestack->start_time < FloatTime() - 2)
				continue;
			// if the bot is in a reachability area
			if ( trap_AAS_AreaReachability(bs->areanum) ) {
				// disable all areas the blocking entity is in
				BotEnableActivateGoalAreas( activategoal, qfalse );
				//
				t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
				// if the button is not reachable
				if (!t) {
					continue;
				}
				activategoal->time = FloatTime() + t * 0.01 + 5;
			}
			return ent;
		}
		// invisible trigger multiple box
		else if (!strcmp(classname, "trigger_multiple")) {
			//
			if (!BotTriggerMultipleActivateGoal(bs, ent, activategoal))
				continue;
			// if the bot tries to activate this trigger already
			if ( bs->activatestack && bs->activatestack->inuse &&
				 bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
				 bs->activatestack->time > FloatTime() &&
				 bs->activatestack->start_time < FloatTime() - 2)
				continue;
			// if the bot is in a reachability area
			if ( trap_AAS_AreaReachability(bs->areanum) ) {
				// disable all areas the blocking entity is in
				BotEnableActivateGoalAreas( activategoal, qfalse );
				//
				t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
				// if the trigger is not reachable
				if (!t) {
					continue;
				}
				activategoal->time = FloatTime() + t * 0.01 + 5;
			}
			return ent;
		}
		else if (!strcmp(classname, "func_timer")) {
			// just skip the func_timer
			continue;
		}
		// the actual button or trigger might be linked through a target_relay or target_delay
		else if (!strcmp(classname, "target_relay") || !strcmp(classname, "target_delay")) {
			if (trap_AAS_ValueForBSPEpairKey(ent, "targetname", targetname[i+1], sizeof(targetname[0]))) {
				i++;
				cur_entities[i] = trap_AAS_NextBSPEntity(0);
			}
		}
	}
#ifdef OBSTACLEDEBUG
	BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no valid activator for entity with target \"%s\"\n", targetname[0]);
#endif
	return 0;
}

/*
==================
BotGoForActivateGoal
==================
*/
int BotGoForActivateGoal(bot_state_t *bs, bot_activategoal_t *activategoal) {
	aas_entityinfo_t activateinfo;

	activategoal->inuse = qtrue;
	if (!activategoal->time)
		activategoal->time = FloatTime() + 10;
	activategoal->start_time = FloatTime();
	BotEntityInfo(activategoal->goal.entitynum, &activateinfo);
	VectorCopy(activateinfo.origin, activategoal->origin);
	//
	if (BotPushOntoActivateGoalStack(bs, activategoal)) {
		// enter the activate entity AI node
		AIEnter_Seek_ActivateEntity(bs, "BotGoForActivateGoal");
		return qtrue;
	}
	else {
		// enable any routing areas that were disabled
		BotEnableActivateGoalAreas(activategoal, qtrue);
		return qfalse;
	}
}

/*
==================
BotPrintActivateGoalInfo
==================
*/
void BotPrintActivateGoalInfo(bot_state_t *bs, bot_activategoal_t *activategoal, int bspent) {
	char netname[MAX_NETNAME];
	char classname[128];
	char buf[128];

	ClientName(bs->client, netname, sizeof(netname));
	trap_AAS_ValueForBSPEpairKey(bspent, "classname", classname, sizeof(classname));
	if (activategoal->shoot) {
		Com_sprintf(buf, sizeof(buf), "%s: I have to shoot at a %s from %1.1f %1.1f %1.1f in area %d\n",
						netname, classname,
						activategoal->goal.origin[0],
						activategoal->goal.origin[1],
						activategoal->goal.origin[2],
						activategoal->goal.areanum);
	}
	else {
		Com_sprintf(buf, sizeof(buf), "%s: I have to activate a %s at %1.1f %1.1f %1.1f in area %d\n",
						netname, classname,
						activategoal->goal.origin[0],
						activategoal->goal.origin[1],
						activategoal->goal.origin[2],
						activategoal->goal.areanum);
	}
	trap_EA_Say(bs->client, buf);
}

/*
==================
BotRandomMove
==================
*/
void BotRandomMove(bot_state_t *bs, bot_moveresult_t *moveresult) {
	vec3_t dir, angles;

	angles[0] = 0;
	angles[1] = random() * 360;
	angles[2] = 0;
	AngleVectors(angles, dir, NULL, NULL);

	trap_BotMoveInDirection(bs->ms, dir, 400, MOVE_WALK);

	moveresult->failure = qfalse;
	VectorCopy(dir, moveresult->movedir);
}

/*
==================
BotAIBlocked

Very basic handling of bots being blocked by other entities.
Check what kind of entity is blocking the bot and try to activate
it. If that's not an option then try to walk around or over the entity.
Before the bot ends in this part of the AI it should predict which doors to
open, which buttons to activate etc.
==================
*/
void BotAIBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate) {
#ifdef OBSTACLEDEBUG
	char netname[MAX_NETNAME];
#endif
	int movetype, bspent;
	vec3_t hordir, sideward, angles, up = {0, 0, 1};
	//vec3_t start, end, mins, maxs;
	aas_entityinfo_t entinfo;
	bot_activategoal_t activategoal;

	// if the bot is not blocked by anything
	if (!moveresult->blocked) {
		bs->notblocked_time = FloatTime();
		return;
	}
	// if stuck in a solid area
	if ( moveresult->type == RESULTTYPE_INSOLIDAREA ) {
		// move in a random direction in the hope to get out
		BotRandomMove(bs, moveresult);
		//
		return;
	}
	// get info for the entity that is blocking the bot
	BotEntityInfo(moveresult->blockentity, &entinfo);
#ifdef OBSTACLEDEBUG
	ClientName(bs->client, netname, sizeof(netname));
	BotAI_Print(PRT_MESSAGE, "%s: I'm blocked by model %d\n", netname, entinfo.modelindex);
#endif // OBSTACLEDEBUG
	// if blocked by a bsp model and the bot wants to activate it
	if (activate && entinfo.modelindex > 0 && entinfo.modelindex <= max_bspmodelindex) {
		// find the bsp entity which should be activated in order to get the blocking entity out of the way
		bspent = BotGetActivateGoal(bs, entinfo.number, &activategoal);
		if (bspent) {
			//
			if (bs->activatestack && !bs->activatestack->inuse)
				bs->activatestack = NULL;
			// if not already trying to activate this entity
			if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
				//
				BotGoForActivateGoal(bs, &activategoal);
			}
			// if ontop of an obstacle or
			// if the bot is not in a reachability area it'll still
			// need some dynamic obstacle avoidance, otherwise return
			if (!(moveresult->flags & MOVERESULT_ONTOPOFOBSTACLE) &&
				trap_AAS_AreaReachability(bs->areanum))
				return;
		}
		else {
			// enable any routing areas that were disabled
			BotEnableActivateGoalAreas(&activategoal, qtrue);
		}
	}
	// just some basic dynamic obstacle avoidance code
	hordir[0] = moveresult->movedir[0];
	hordir[1] = moveresult->movedir[1];
	hordir[2] = 0;
	// if no direction just take a random direction
	if (VectorNormalize(hordir) < 0.1) {
		VectorSet(angles, 0, 360 * random(), 0);
		AngleVectors(angles, hordir, NULL, NULL);
	}
	//
	//if (moveresult->flags & MOVERESULT_ONTOPOFOBSTACLE) movetype = MOVE_JUMP;
	//else
	movetype = MOVE_WALK;
	// if there's an obstacle at the bot's feet and head then
	// the bot might be able to crouch through
	//VectorCopy(bs->origin, start);
	//start[2] += 18;
	//VectorMA(start, 5, hordir, end);
	//VectorSet(mins, -16, -16, -24);
	//VectorSet(maxs, 16, 16, 4);
	//
	//bsptrace = AAS_Trace(start, mins, maxs, end, bs->entitynum, MASK_PLAYERSOLID);
	//if (bsptrace.fraction >= 1) movetype = MOVE_CROUCH;
	// get the sideward vector
	CrossProduct(hordir, up, sideward);
	//
	if (bs->flags & BFL_AVOIDRIGHT) VectorNegate(sideward, sideward);
	// try to crouch straight forward?
	if (movetype != MOVE_CROUCH || !trap_BotMoveInDirection(bs->ms, hordir, 400, movetype)) {
		// perform the movement
		if (!trap_BotMoveInDirection(bs->ms, sideward, 400, movetype)) {
			// flip the avoid direction flag
			bs->flags ^= BFL_AVOIDRIGHT;
			// flip the direction
			// VectorNegate(sideward, sideward);
			VectorMA(sideward, -1, hordir, sideward);
			// move in the other direction
			trap_BotMoveInDirection(bs->ms, sideward, 400, movetype);
		}
	}
	//
	if (bs->notblocked_time < FloatTime() - 0.4) {
		// just reset goals and hope the bot will go into another direction?
		// is this still needed??
		if (bs->ainode == AINode_Seek_NBG) bs->nbg_time = 0;
		else if (bs->ainode == AINode_Seek_LTG) bs->ltg_time = 0;
	}
}

/*
==================
BotAIPredictObstacles

Predict the route towards the goal and check if the bot
will be blocked by certain obstacles. When the bot has obstacles
on its path the bot should figure out if they can be removed
by activating certain entities.
==================
*/
int BotAIPredictObstacles(bot_state_t *bs, bot_goal_t *goal) {
	int modelnum, entitynum, bspent;
	bot_activategoal_t activategoal;
	aas_predictroute_t route;

	if (!bot_predictobstacles.integer)
		return qfalse;

	// always predict when the goal change or at regular intervals
	// eser - (4J) more aggressive recomputation — 0.5s vs stock 6s
	if (bs->predictobstacles_goalareanum == goal->areanum &&
		bs->predictobstacles_time > FloatTime() - 0.5f) {
		return qfalse;
	}
	bs->predictobstacles_goalareanum = goal->areanum;
	bs->predictobstacles_time = FloatTime();

	// predict at most 100 areas or 1 second ahead
	trap_AAS_PredictRoute(&route, bs->areanum, bs->origin,
							goal->areanum, bs->tfl, 100, 1000,
							RSE_USETRAVELTYPE|RSE_ENTERCONTENTS,
							AREACONTENTS_MOVER, TFL_BRIDGE, 0);
	// if bot has to travel through an area with a mover
	if (route.stopevent & RSE_ENTERCONTENTS) {
		// if the bot will run into a mover
		if (route.endcontents & AREACONTENTS_MOVER) {
			//NOTE: this only works with bspc 2.1 or higher
			modelnum = (route.endcontents & AREACONTENTS_MODELNUM) >> AREACONTENTS_MODELNUMSHIFT;
			if (modelnum) {
				//
				entitynum = BotModelMinsMaxs(modelnum, ET_MOVER, 0, NULL, NULL);
				if (entitynum) {
					//NOTE: BotGetActivateGoal already checks if the door is open or not
					bspent = BotGetActivateGoal(bs, entitynum, &activategoal);
					if (bspent) {
						//
						if (bs->activatestack && !bs->activatestack->inuse)
							bs->activatestack = NULL;
						// if not already trying to activate this entity
						if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
							//
							//BotAI_Print(PRT_MESSAGE, "blocked by mover model %d, entity %d ?\n", modelnum, entitynum);
							//
							BotGoForActivateGoal(bs, &activategoal);
							return qtrue;
						}
						else {
							// enable any routing areas that were disabled
							BotEnableActivateGoalAreas(&activategoal, qtrue);
						}
					}
				}
			}
		}
	}
	else if (route.stopevent & RSE_USETRAVELTYPE) {
		if (route.endtravelflags & TFL_BRIDGE) {
			//FIXME: check if the bridge is available to travel over
		}
	}
	return qfalse;
}

/*
==================
BotCheckConsoleMessages
==================
*/
void BotCheckConsoleMessages(bot_state_t *bs) {
	char botname[MAX_NETNAME], message[MAX_MESSAGE_SIZE], netname[MAX_NETNAME], *ptr;
	float chat_reply;
	int context, handle;
	bot_consolemessage_t m;
	bot_match_t match;

	if ( bs->wiredBotsActive ) {
		while((handle = trap_BotNextConsoleMessage(bs->cs, &m)) != 0) {
			ptr = m.message;
			if (m.type == CMS_CHAT) {
				if (trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
					ptr = m.message + match.variables[MESSAGE].offset;
				}
				trap_UnifyWhiteSpaces(ptr);
				context = BotSynonymContext(bs);
				trap_BotReplaceSynonyms(ptr, context);

				if (trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
					wbChatCtx_t chatCtx;
					int fromClient;

					Com_Memset( &chatCtx, 0, sizeof( chatCtx ) );
					trap_BotMatchVariable(&match, NETNAME, netname, sizeof(netname));
					trap_BotMatchVariable(&match, MESSAGE, message, sizeof(message));

					fromClient = ClientFromName( netname );
					if ( fromClient >= 0 && fromClient < MAX_CLIENTS ) {
						ClientName( fromClient, chatCtx.sender, sizeof( chatCtx.sender ) );
						chatCtx.team = (m.type == CMS_CHAT && (match.subtype & ST_TEAM)) ? 1 : 0;
					}
					Q_strncpyz( chatCtx.text, message, sizeof( chatCtx.text ) );
					Q_strncpyz( chatCtx.map, BotMapTitle(), sizeof( chatCtx.map ) );

					switch ( gametype ) {
						case GT_DEATHMATCH: Q_strncpyz( chatCtx.gametype, "ffa", sizeof( chatCtx.gametype ) ); break;
						case GT_DUEL: Q_strncpyz( chatCtx.gametype, "duel", sizeof( chatCtx.gametype ) ); break;
						case GT_TDM: Q_strncpyz( chatCtx.gametype, "tdm", sizeof( chatCtx.gametype ) ); break;
						case GT_CTF: Q_strncpyz( chatCtx.gametype, "ctf", sizeof( chatCtx.gametype ) ); break;
						case GT_1FCTF: Q_strncpyz( chatCtx.gametype, "1fctf", sizeof( chatCtx.gametype ) ); break;
						default: Q_strncpyz( chatCtx.gametype, "other", sizeof( chatCtx.gametype ) ); break;
					}

					WiredBots_Chat( bs, "message", &chatCtx );
				}
			}

			trap_BotRemoveConsoleMessage(bs->cs, handle);
		}
		return;
	}

	//the name of this bot
	ClientName(bs->client, botname, sizeof(botname));
	//
	while((handle = trap_BotNextConsoleMessage(bs->cs, &m)) != 0) {
		//if the chat state is flooded with messages the bot will read them quickly
		if (trap_BotNumConsoleMessages(bs->cs) < 10) {
			//if it is a chat message the bot needs some time to read it
			if (m.type == CMS_CHAT && m.time > FloatTime() - (1 + random())) break;
		}
		//
		ptr = m.message;
		//if it is a chat message then don't unify white spaces and don't
		//replace synonyms in the netname
		if (m.type == CMS_CHAT) {
			//
			if (trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
				ptr = m.message + match.variables[MESSAGE].offset;
			}
		}
		//unify the white spaces in the message
		trap_UnifyWhiteSpaces(ptr);
		//replace synonyms in the right context
		context = BotSynonymContext(bs);
		trap_BotReplaceSynonyms(ptr, context);
		//if there's no match
		if (!BotMatchMessage(bs, m.message)) {
			if (m.type == CMS_CHAT) {
				int talker = -1;
				if (trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
					trap_BotMatchVariable(&match, NETNAME, netname, sizeof(netname));
					talker = ClientOnSameTeamFromName(bs, netname);
				}
				BotDirective_ParseChatOrder(bs, talker, m.message);
			}
			//if it is a chat message
			if (m.type == CMS_CHAT && !bot_nochat.integer) {
				//
				if (!trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
					trap_BotRemoveConsoleMessage(bs->cs, handle);
					continue;
				}
				//don't use eliza chats with team messages
				if (match.subtype & ST_TEAM) {
					trap_BotRemoveConsoleMessage(bs->cs, handle);
					continue;
				}
				//
				trap_BotMatchVariable(&match, NETNAME, netname, sizeof(netname));
				trap_BotMatchVariable(&match, MESSAGE, message, sizeof(message));
				//if this is a message from the bot self
				if (bs->client == ClientFromName(netname)) {
					trap_BotRemoveConsoleMessage(bs->cs, handle);
					continue;
				}
				//unify the message
				trap_UnifyWhiteSpaces(message);
				//
				trap_Cvar_Update(&bot_testrchat);
				if (bot_testrchat.integer) {
					//
					trap_BotLibVarSet("bot_testrchat", "1");
					//if bot replies with a chat message
					if (trap_BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
															NULL, NULL,
															NULL, NULL,
															NULL, NULL,
															botname, netname)) {
						BotAI_Print(PRT_MESSAGE, "------------------------\n");
					}
					else {
						BotAI_Print(PRT_MESSAGE, "**** no valid reply ****\n");
					}
				}
				//if at a valid chat position and not chatting already and not in teamplay
				else if (bs->ainode != AINode_Stand && BotValidChatPosition(bs) && !TeamPlayIsOn()) {
					chat_reply = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_REPLY, 0, 1);
					if (random() < 1.5 / (NumBots()+1) && random() < chat_reply) {
						//if bot replies with a chat message
						if (trap_BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
																NULL, NULL,
																NULL, NULL,
																NULL, NULL,
																botname, netname)) {
							//remove the console message
							trap_BotRemoveConsoleMessage(bs->cs, handle);
							bs->stand_time = FloatTime() + BotChatTime(bs);
							AIEnter_Stand(bs, "BotCheckConsoleMessages: reply chat");
							//EA_Say(bs->client, bs->cs.chatmessage);
							break;
						}
					}
				}
			}
		}
		//remove the console message
		trap_BotRemoveConsoleMessage(bs->cs, handle);
	}
}

/*
==================
BotCheckEvents
==================
*/
void BotCheckForGrenades(bot_state_t *bs, entityState_t *state) {
	// if this is not a grenade
	if (state->eType != ET_MISSILE || state->weapon != WP_GRENADE_LAUNCHER)
		return;
	// try to avoid the grenade
	trap_BotAddAvoidSpot(bs->ms, state->pos.trBase, 160, AVOID_ALWAYS);
}

/*
==================
BotCheckForKamikazeBody
==================
*/
void BotCheckForKamikazeBody(bot_state_t *bs, entityState_t *state) {
	// if this entity is not wearing the kamikaze
	if (!(state->eFlags & EF_KAMIKAZE))
		return;
	// if this entity isn't dead
	if (!(state->eFlags & EF_DEAD))
		return;
	//remember this kamikaze body
	bs->kamikazebody = state->number;
}

/*
==================
BotCheckEvents
==================
*/
void BotCheckEvents(bot_state_t *bs, entityState_t *state) {
	int event;
	char buf[128];
	aas_entityinfo_t entinfo;

	//NOTE: this sucks, we're accessing the gentity_t directly
	//but there's no other fast way to do it right now
	if (bs->entityeventTime[state->number] == g_entities[state->number].eventTime) {
		return;
	}
	bs->entityeventTime[state->number] = g_entities[state->number].eventTime;
	//if it's an event only entity
	if (state->eType > ET_EVENTS) {
		event = (state->eType - ET_EVENTS) & ~EV_EVENT_BITS;
	}
	else {
		event = state->event & ~EV_EVENT_BITS;
	}
	//
	switch(event) {
		//client obituary event
		case EV_OBITUARY:
		{
			int target, attacker, mod;

			target = state->otherEntityNum;
			attacker = state->otherEntityNum2;
			mod = state->eventParm;
			//
			if (target == bs->client) {
				bs->botdeathtype = mod;
				bs->lastkilledby = attacker;
				//
				if (target == attacker ||
					target == ENTITYNUM_NONE ||
					target == ENTITYNUM_WORLD) bs->botsuicide = qtrue;
				else bs->botsuicide = qfalse;
				//
				bs->num_deaths++;
			}
			//else if this client was killed by the bot
			else if (attacker == bs->client) {
				bs->enemydeathtype = mod;
				bs->lastkilledplayer = target;
				bs->killedenemy_time = FloatTime();
				//
				bs->num_kills++;
			}
			else if (attacker == bs->enemy && target == attacker) {
				bs->enemysuicide = qtrue;
			}
			//
			if (gametype == GT_1FCTF) {
				//
				BotEntityInfo(target, &entinfo);
				if ( entinfo.powerups & ( 1 << PW_NEUTRALFLAG ) ) {
					if (!BotSameTeam(bs, target)) {
						bs->neutralflagstatus = 3;	//enemy dropped the flag
						bs->flagstatuschanged = qtrue;
					}
				}
			}
			break;
		}
		case EV_GLOBAL_SOUND:
		{
			if (state->eventParm < 0 || state->eventParm >= MAX_SOUNDS) {
				BotAI_Print(PRT_ERROR, "EV_GLOBAL_SOUND: eventParm (%d) out of range\n", state->eventParm);
				break;
			}
			trap_GetConfigstring(CS_SOUNDS + state->eventParm, buf, sizeof(buf));
			/*
			if (!strcmp(buf, "sound/teamplay/flagret_red.opus")) {
				//red flag is returned
				bs->redflagstatus = 0;
				bs->flagstatuschanged = qtrue;
			}
			else if (!strcmp(buf, "sound/teamplay/flagret_blu.opus")) {
				//blue flag is returned
				bs->blueflagstatus = 0;
				bs->flagstatuschanged = qtrue;
			}
			else*/
			if (!strcmp(buf, "sound/items/kamikazerespawn.opus" )) {
				//the kamikaze respawned so don't avoid it
				BotDontAvoid(bs, "Kamikaze");
			}
			else if (!strcmp(buf, "sound/items/poweruprespawn.opus")) {
				//powerup respawned... go get it
				BotGoForPowerups(bs);
			}
			break;
		}
		case EV_GLOBAL_TEAM_SOUND:
		{
			if (gametype == GT_CTF) {
				switch(state->eventParm) {
					case GTS_RED_CAPTURE:
						bs->blueflagstatus = 0;
						bs->redflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break; //see BotMatch_CTF
					case GTS_BLUE_CAPTURE:
						bs->blueflagstatus = 0;
						bs->redflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break; //see BotMatch_CTF
					case GTS_RED_RETURN:
						//blue flag is returned
						bs->blueflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_BLUE_RETURN:
						//red flag is returned
						bs->redflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_RED_TAKEN:
						//blue flag is taken
						bs->blueflagstatus = 1;
						bs->flagstatuschanged = qtrue;
						break; //see BotMatch_CTF
					case GTS_BLUE_TAKEN:
						//red flag is taken
						bs->redflagstatus = 1;
						bs->flagstatuschanged = qtrue;
						break; //see BotMatch_CTF
				}
			}
			else if (gametype == GT_1FCTF) {
				switch(state->eventParm) {
					case GTS_RED_CAPTURE:
						bs->neutralflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_BLUE_CAPTURE:
						bs->neutralflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_RED_RETURN:
						//flag has returned
						bs->neutralflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_BLUE_RETURN:
						//flag has returned
						bs->neutralflagstatus = 0;
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_RED_TAKEN:
						bs->neutralflagstatus = BotTeam(bs) == TEAM_RED ? 2 : 1; //FIXME: check Team_TakeFlagSound in g_team.c
						bs->flagstatuschanged = qtrue;
						break;
					case GTS_BLUE_TAKEN:
						bs->neutralflagstatus = BotTeam(bs) == TEAM_BLUE ? 2 : 1; //FIXME: check Team_TakeFlagSound in g_team.c
						bs->flagstatuschanged = qtrue;
						break;
				}
			}
			break;
		}
		case EV_PLAYER_TELEPORT_IN:
		{
			VectorCopy(state->origin, lastteleport_origin);
			lastteleport_time = FloatTime();
			break;
		}
		case EV_GENERAL_SOUND:
		{
			//if this sound is played on the bot
			if (state->number == bs->client) {
				if (state->eventParm < 0 || state->eventParm >= MAX_SOUNDS) {
					BotAI_Print(PRT_ERROR, "EV_GENERAL_SOUND: eventParm (%d) out of range\n", state->eventParm);
					break;
				}
				//check out the sound
				trap_GetConfigstring(CS_SOUNDS + state->eventParm, buf, sizeof(buf));
				//if falling into a death pit
				if (!strcmp(buf, "*falling1.opus")) {
					//if the bot has a personal teleporter
					if (bs->inventory[INVENTORY_TELEPORTER] > 0) {
						//use the holdable item
						trap_EA_Use(bs->client);
					}
				}
			}
			break;
		}
		case EV_FOOTSTEP:
		case EV_FOOTSTEP_METAL:
		case EV_FOOTSPLASH:
		case EV_FOOTWADE:
		case EV_SWIM:
		case EV_FALL_SHORT:
		case EV_FALL_MEDIUM:
		case EV_FALL_FAR:
		case EV_STEP_4:
		case EV_STEP_8:
		case EV_STEP_12:
		case EV_STEP_16:
		case EV_JUMP_PAD:
		case EV_JUMP:
		case EV_TAUNT:
		case EV_WATER_TOUCH:
		case EV_WATER_LEAVE:
		case EV_WATER_UNDER:
		case EV_WATER_CLEAR:
		case EV_ITEM_PICKUP:
		case EV_GLOBAL_ITEM_PICKUP:
		case EV_NOAMMO:
		case EV_CHANGE_WEAPON:
		case EV_FIRE_WEAPON_PRI:
		case EV_FIRE_WEAPON_SEC:
			//FIXME: either add to sound queue or mark player as someone making noise
			break;
		case EV_USE_ITEM0:
		case EV_USE_ITEM1:
		case EV_USE_ITEM2:
		case EV_USE_ITEM3:
		case EV_USE_ITEM4:
		case EV_USE_ITEM5:
		case EV_USE_ITEM6:
		case EV_USE_ITEM7:
		case EV_USE_ITEM8:
		case EV_USE_ITEM9:
		case EV_USE_ITEM10:
		case EV_USE_ITEM11:
		case EV_USE_ITEM12:
		case EV_USE_ITEM13:
		case EV_USE_ITEM14:
		case EV_USE_ITEM15:
			break;
	}
}

/*
==================
BotCheckSnapshot
==================
*/
void BotCheckSnapshot(bot_state_t *bs) {
	int ent;
	entityState_t state;

	//remove all avoid spots
	{ vec3_t clearOrigin = { 0, 0, 0 }; trap_BotAddAvoidSpot(bs->ms, clearOrigin, 0, AVOID_CLEAR); }
	//reset kamikaze body
	bs->kamikazebody = 0;
	//
	ent = 0;
	while( ( ent = BotAI_GetSnapshotEntity( bs->client, ent, &state ) ) != -1 ) {
		//check the entity state for events
		BotCheckEvents(bs, &state);
		//check for grenades the bot should avoid
		BotCheckForGrenades(bs, &state);
		//
		//check for dead bodies with the kamikaze effect which should be gibbed
		BotCheckForKamikazeBody(bs, &state);
	}
	//check the player state for events
	BotAI_GetEntityState(bs->client, &state);
	//copy the player state events to the entity state
	state.event = bs->cur_ps.externalEvent;
	state.eventParm = bs->cur_ps.externalEventParm;
	//
	BotCheckEvents(bs, &state);
}

/*
==================
BotCheckAir
==================
*/
void BotCheckAir(bot_state_t *bs) {
	if (bs->inventory[INVENTORY_ENVIRONMENTSUIT] <= 0) {
		if (trap_AAS_PointContents(bs->eye) & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA)) {
			return;
		}
	}
	bs->lastair_time = FloatTime();
}

/*
==================
BotAlternateRoute
==================
*/
bot_goal_t *BotAlternateRoute(bot_state_t *bs, bot_goal_t *goal) {
	int t;

	// if the bot has an alternative route goal
	if (bs->altroutegoal.areanum) {
		//
		if (bs->reachedaltroutegoal_time)
			return goal;
		// travel time towards alternative route goal
		t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, bs->altroutegoal.areanum, bs->tfl);
		if (t && t < 20) {
			//BotAI_Print(PRT_MESSAGE, "reached alternate route goal\n");
			bs->reachedaltroutegoal_time = FloatTime();
		}
		memcpy(goal, &bs->altroutegoal, sizeof(bot_goal_t));
		return &bs->altroutegoal;
	}
	return goal;
}

/*
==================
BotGetAlternateRouteGoal
==================
*/
int BotGetAlternateRouteGoal(bot_state_t *bs, int base) {
	aas_altroutegoal_t *altroutegoals;
	bot_goal_t *goal;
	int numaltroutegoals, rnd;

	if (base == TEAM_RED) {
		altroutegoals = red_altroutegoals;
		numaltroutegoals = red_numaltroutegoals;
	}
	else {
		altroutegoals = blue_altroutegoals;
		numaltroutegoals = blue_numaltroutegoals;
	}
	if (!numaltroutegoals)
		return qfalse;
	rnd = (float) random() * numaltroutegoals;
	if (rnd >= numaltroutegoals)
		rnd = numaltroutegoals-1;
	goal = &bs->altroutegoal;
	goal->areanum = altroutegoals[rnd].areanum;
	VectorCopy(altroutegoals[rnd].origin, goal->origin);
	VectorSet(goal->mins, -8, -8, -8);
	VectorSet(goal->maxs, 8, 8, 8);
	goal->entitynum = 0;
	goal->iteminfo = 0;
	goal->number = 0;
	goal->flags = 0;
	//
	bs->reachedaltroutegoal_time = 0;
	return qtrue;
}

/*
==================
BotSetupAlternateRouteGoals
==================
*/
void BotSetupAlternativeRouteGoals(void) {

	if (altroutegoals_setup)
		return;
	if (gametype == GT_CTF) {
		if (trap_BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
			BotAI_Print(PRT_WARNING, "No alt routes without Neutral Flag\n");
		if (ctf_neutralflag.areanum) {
			//
			red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
										ctf_neutralflag.origin, ctf_neutralflag.areanum,
										ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
										red_altroutegoals, MAX_ALTROUTEGOALS,
										ALTROUTEGOAL_CLUSTERPORTALS|
										ALTROUTEGOAL_VIEWPORTALS);
			blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
										ctf_neutralflag.origin, ctf_neutralflag.areanum,
										ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
										blue_altroutegoals, MAX_ALTROUTEGOALS,
										ALTROUTEGOAL_CLUSTERPORTALS|
										ALTROUTEGOAL_VIEWPORTALS);
		}
	}
	else if (gametype == GT_1FCTF) {
		if (trap_BotGetLevelItemGoal(-1, "Neutral Flag", &neutralobelisk) < 0)
			BotAI_Print(PRT_WARNING, "One Flag CTF without Neutral Flag\n");
		red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
									ctf_neutralflag.origin, ctf_neutralflag.areanum,
									ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
									red_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
		blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
									ctf_neutralflag.origin, ctf_neutralflag.areanum,
									ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
									blue_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
	}
#if FEAT_OVERLOAD
	else if (gametype == GT_OBELISK) {
		if (trap_BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
			BotAI_Print(PRT_WARNING, "No alt routes without Neutral Obelisk\n");
		//
		red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
									red_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
		blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
									blue_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
	}
#endif
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		if (trap_BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without Neutral Obelisk\n");
		red_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
									red_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
		blue_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
									blue_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
	}
#endif
	altroutegoals_setup = qtrue;
}

vmCvar_t bot_autoskill;

/*
==================
BotAutoCalibrate

Per-bot skill adjustment based on K/D ratio vs human players.
Evaluates every 60 seconds using a 5-minute sliding window.
Adjusts by ±0.3f per evaluation, clamped to [1.0, 5.0].
==================
*/
static void BotAutoCalibrate( bot_state_t *bs )
{
	float kd, adjustment;

	if ( !bot_autoskill.integer ) return;

	// initialize autoskill from base skill on first call
	if ( bs->autoskill <= 0 ) {
		bs->autoskill = bs->settings.skill;
		bs->autoskill_time = floattime + 60.0f;
		bs->autoskill_window_start = floattime;
		bs->kills_vs_humans = 0;
		bs->deaths_vs_humans = 0;
		return;
	}

	// reset window every 5 minutes
	if ( floattime - bs->autoskill_window_start > 300.0f ) {
		bs->kills_vs_humans = 0;
		bs->deaths_vs_humans = 0;
		bs->autoskill_window_start = floattime;
	}

	// evaluate every 60 seconds
	if ( floattime < bs->autoskill_time ) return;
	bs->autoskill_time = floattime + 60.0f;

	// need some data to calibrate
	if ( bs->kills_vs_humans + bs->deaths_vs_humans < 3 ) return;

	// compute K/D ratio
	if ( bs->deaths_vs_humans > 0 ) {
		kd = (float)bs->kills_vs_humans / (float)bs->deaths_vs_humans;
	} else {
		kd = (float)bs->kills_vs_humans + 1.0f; // no deaths = very dominant
	}

	// adjust: if bot is dominating (K/D > 2.0), make it easier
	//         if bot is getting crushed (K/D < 0.5), make it harder
	adjustment = 0;
	if ( kd > 2.0f ) {
		adjustment = -0.3f; // bot too strong, lower skill
	} else if ( kd < 0.5f ) {
		adjustment = 0.3f;  // bot too weak, raise skill
	}

	if ( adjustment != 0 ) {
		bs->autoskill += adjustment;
		if ( bs->autoskill < 1.0f ) bs->autoskill = 1.0f;
		if ( bs->autoskill > 5.0f ) bs->autoskill = 5.0f;
	}
}

/*
==================
BotAutoCalibrate_RecordKill

Called from game code when a kill occurs. Tracks bot kills/deaths
vs human players for the auto-calibration sliding window.
==================
*/
void BotAutoCalibrate_RecordKill( int attacker, int victim )
{
	bot_state_t *bs;
	gentity_t *ent;
	int i;

	if ( !bot_autoskill.integer ) return;

	// check if attacker is a bot who killed a human
	ent = &g_entities[attacker];
	if ( attacker >= 0 && attacker < MAX_CLIENTS && (ent->r.svFlags & SVF_BOT) ) {
		// attacker is a bot — check if victim is human
		ent = &g_entities[victim];
		if ( victim >= 0 && victim < MAX_CLIENTS && !(ent->r.svFlags & SVF_BOT) ) {
			for ( i = 0; i < MAX_CLIENTS; i++ ) {
				bs = botstates[i];
				if ( bs && bs->entitynum == attacker ) {
					bs->kills_vs_humans++;
					break;
				}
			}
		}
	}

	// check if victim is a bot who was killed by a human
	ent = &g_entities[victim];
	if ( victim >= 0 && victim < MAX_CLIENTS && (ent->r.svFlags & SVF_BOT) ) {
		ent = &g_entities[attacker];
		if ( attacker >= 0 && attacker < MAX_CLIENTS && !(ent->r.svFlags & SVF_BOT) ) {
			for ( i = 0; i < MAX_CLIENTS; i++ ) {
				bs = botstates[i];
				if ( bs && bs->entitynum == victim ) {
					bs->deaths_vs_humans++;
					break;
				}
			}
		}
	}
}

/*
==================
BotTeamCallout

Issue tactical voice commands based on awareness state.
Global team cooldown: max 3 callouts per team per 5 seconds.
==================
*/
static int teamCalloutCount[TEAM_NUM_TEAMS];
static float teamCalloutReset[TEAM_NUM_TEAMS];

static void BotTeamCallout( bot_state_t *bs )
{
	int team;

	// only in team games
	if ( !g_gametypeIsTeamGame ) return;

	team = bs->cur_ps.persistant[PERS_TEAM];
	if ( team < 0 || team >= TEAM_NUM_TEAMS ) return;

	// global team cooldown
	if ( floattime > teamCalloutReset[team] ) {
		teamCalloutCount[team] = 0;
		teamCalloutReset[team] = floattime + 5.0f;
	}
	if ( teamCalloutCount[team] >= 3 ) return;

	// "enemy spotted" — when aware of an enemy not visually confirmed
	if ( bs->num_aware > 0 && !bs->aware[0].visual ) {
		trap_EA_Command( bs->client, "vsay_team enemyspot" );
		teamCalloutCount[team]++;
		return;
	}

	// "need backup" — low health + enemy nearby + teammate nearby
	if ( bs->cur_ps.stats[STAT_HEALTH] < 50 && bs->enemy >= 0 ) {
		trap_EA_Command( bs->client, "vsay_team needbackup" );
		teamCalloutCount[team]++;
		return;
	}
}

/*
==================
BotStrafeJumpCheck

Check if the bot should strafejump during navigation. Requirements:
  - On ground with forward momentum
  - Route ahead is straight (no tight turns)
  - Skill >= 3
  - Not in combat (bs->enemy == -1)
  - Not dodging missiles

If eligible, overrides bs->ideal_viewangles with strafejump angles
and issues jump commands. The Q3 air acceleration physics handle
the speed gain automatically.
==================
*/
void BotStrafeJumpCheck( bot_state_t *bs, bot_moveresult_t *moveresult )
{
	float skill, speed, angle;
	vec3_t flatvel, moveDir, forward;
	vec3_t sjAngles;

	bs->strafejump_active = qfalse;

	// must have an enemy-free context and no active dodge
	if ( bs->enemy >= 0 ) return;
	if ( bs->dodge_active ) return;

	skill = bs->autoskill > 0 ? bs->autoskill : bs->settings.skill;
	if ( skill < 3.0f ) return;

	// ── IN AIR ──────────────────────────────────────────────────────
	// With air control, view angle IS the steering input. Just press
	// forward with the strafejump angles — no direction vector needed.
	if ( bs->cur_ps.groundEntityNum == ENTITYNUM_NONE ) {
		if ( bs->strafejump_side != 0 ) {
			bs->strafejump_active = qtrue;
			bs->strafejump_landed = qfalse;
			VectorCopy( bs->strafejump_angles, bs->ideal_viewangles );
			trap_EA_MoveForward( bs->client );
			trap_EA_Jump( bs->client );
		}
		return;
	}

	// ── ON GROUND ───────────────────────────────────────────────────

	VectorCopy( bs->cur_ps.velocity, flatvel );
	flatvel[2] = 0;
	speed = VectorLength( flatvel );

	if ( speed < 200.0f ) {
		bs->strafejump_side = 0;
		return;
	}

	// if nav has no direction this frame (reachability transition),
	// bridge the gap: keep pressing forward with last strafejump angles
	if ( VectorLength( moveresult->movedir ) < 0.1f ) {
		if ( bs->strafejump_side != 0 ) {
			bs->strafejump_active = qtrue;
			VectorCopy( bs->strafejump_angles, bs->ideal_viewangles );
			trap_EA_MoveForward( bs->client );
			trap_EA_Jump( bs->client );
		}
		return;
	}

	// check route straightness
	VectorCopy( moveresult->movedir, moveDir );
	moveDir[2] = 0;
	VectorNormalize( moveDir );
	VectorNormalize2( flatvel, forward );

	if ( DotProduct( moveDir, forward ) < 0.85f ) {
		bs->strafejump_side = 0;
		return;
	}

	// compute optimal strafejump angle (speed-dependent)
	angle = atan2f( 320.0f * 0.033f, speed ) * (180.0f / M_PI);
	if ( angle < 3.0f ) angle = 3.0f;
	if ( angle > 30.0f ) angle = 30.0f;

	// alternate sides once per landing (not every grounded frame)
	if ( bs->strafejump_side == 0 ) {
		bs->strafejump_side = 1;
		bs->strafejump_landed = qtrue;
	} else if ( !bs->strafejump_landed ) {
		bs->strafejump_side = -bs->strafejump_side;
		bs->strafejump_landed = qtrue;
	}

	// compute strafejump view angles: yaw offset from movement direction
	vectoangles( moveDir, sjAngles );
	sjAngles[YAW] += angle * bs->strafejump_side;
	sjAngles[PITCH] = 0;

	VectorCopy( sjAngles, bs->strafejump_angles );
	VectorCopy( sjAngles, bs->ideal_viewangles );
	bs->strafejump_active = qtrue;

	// press forward — view angle offset handles the strafe component
	trap_EA_MoveForward( bs->client );
	trap_EA_Jump( bs->client );

#if FEAT_WIREDNET_OBSERVER
	trap_WiredNet_EmitBotEvent( bs->entitynum, "strafejump", 1, (int)speed, bs->origin );
#endif
}

/*
==================
BotDeathmatchAI
==================
*/
void BotDeathmatchAI(bot_state_t *bs, float thinktime) {
	char name[144];
	char userinfo[MAX_INFO_STRING];
	char buf[MAX_INFO_STRING];
	int i;

	//if the bot has just been setup
	if (bs->setupcount > 0) {
		bs->setupcount--;
		if (bs->setupcount > 0) return;
		//set the chat name
		ClientName(bs->client, name, sizeof(name));
		trap_BotSetChatName(bs->cs, name, bs->client);
		//
		bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];
		bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
		//
		bs->setupcount = 0;
		//
		BotSetupAlternativeRouteGoals();
	}
	//no ideal view set
	bs->flags &= ~BFL_IDEALVIEWSET;
	//
	if (!BotIntermission(bs)) {
		//set the teleport time
		BotSetTeleportTime(bs);
		//update some inventory values
		BotUpdateInventory(bs);
		//check out the snapshot
		BotCheckSnapshot(bs);
		//check for air
		BotCheckAir(bs);
		//scan for incoming missiles
		BotScanMissiles(bs);
		//update entity awareness (tracks enemies via missiles, teleports, sounds)
		BotAwareUpdate(bs);
		//evaluate dodge direction if missiles are incoming
		BotDodgeMovement(bs);
		//update item respawn timing
		BotItemTimeUpdate(bs);
		//per-bot skill auto-calibration vs human players
		BotAutoCalibrate(bs);
		//tactical team voice callouts
		BotTeamCallout(bs);
	}
	//check the console messages
	BotCheckConsoleMessages(bs);
	//if not in the intermission and not in observer mode
	if (!BotIntermission(bs) && !BotIsObserver(bs)) {
		//do team AI
		BotTeamAI(bs);
	}
	//if the bot has no ai node
	if (!bs->ainode) {
		AIEnter_Seek_LTG(bs, "BotDeathmatchAI: no ai node");
	}
	//if the bot entered the game less than 8 seconds ago
	if (!bs->entergamechat && bs->entergame_time > FloatTime() - 8) {
		if (BotChat_EnterGame(bs)) {
			if ( !bs->wiredBotsActive ) {
				bs->stand_time = FloatTime() + BotChatTime(bs);
				AIEnter_Stand(bs, "BotDeathmatchAI: chat enter game");
			}
		}
		bs->entergamechat = qtrue;
	}
	//reset the node switches from the previous frame
	BotResetNodeSwitches();
	//execute AI nodes
	for (i = 0; i < MAX_NODESWITCHES; i++) {
		if (bs->ainode(bs)) break;
	}
	//if the bot removed itself :)
	if (!bs->inuse) return;
	//if the bot executed too many AI nodes
	if (i >= MAX_NODESWITCHES) {
		trap_BotDumpGoalStack(bs->gs);
		trap_BotDumpAvoidGoals(bs->gs);
		BotDumpNodeSwitches(bs);
		ClientName(bs->client, name, sizeof(name));
		BotAI_Print(PRT_ERROR, "%s at %1.1f switched more than %d AI nodes\n", name, FloatTime(), MAX_NODESWITCHES);
	}
	//
	if ( trap_Cvar_VariableIntegerValue( "sv_botDebugMove" ) ) {
		static float s_moveLogTime[MAX_CLIENTS];
		if ( FloatTime() - s_moveLogTime[bs->client] > 5.0f ) {
			s_moveLogTime[bs->client] = FloatTime();
			float speed = VectorLength( bs->cur_ps.velocity );
			G_Printf( "^3[BotMove] client=%d origin=(%.0f %.0f %.0f) speed=%.1f enemy=%d wp=%d hp=%d\n",
				bs->client,
				bs->origin[0], bs->origin[1], bs->origin[2],
				speed,
				bs->enemy,
				bs->weaponnum,
				bs->inventory[INVENTORY_HEALTH] );
		}
	}
	//
	bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];
	bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
}

/*
==================
BotSetEntityNumForGoalWithModel
==================
*/
void BotSetEntityNumForGoalWithModel(bot_goal_t *goal, int eType, char *modelname) {
	gentity_t *ent;
	int i, modelindex;
	vec3_t dir;

	modelindex = G_ModelIndex( modelname );
	ent = &g_entities[0];
	for (i = 0; i < level.num_entities; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( eType && ent->s.eType != eType) {
			continue;
		}
		if (ent->s.modelindex != modelindex) {
			continue;
		}
		VectorSubtract(goal->origin, ent->s.origin, dir);
		if (VectorLengthSquared(dir) < Square(10)) {
			goal->entitynum = i;
			return;
		}
	}
}

/*
==================
BotSetEntityNumForGoal
==================
*/
void BotSetEntityNumForGoal(bot_goal_t *goal, char *classname) {
	gentity_t *ent;
	int i;
	vec3_t dir;

	ent = &g_entities[0];
	for (i = 0; i < level.num_entities; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( Q_stricmp(ent->classname, classname) != 0 ) {
			continue;
		}
		VectorSubtract(goal->origin, ent->s.origin, dir);
		if (VectorLengthSquared(dir) < Square(10)) {
			goal->entitynum = i;
			return;
		}
	}
}

/*
==================
BotSetEntityNumForGoalWithActivator
==================
*/
void BotSetEntityNumForGoalWithActivator(bot_goal_t *goal, char *classname) {
	gentity_t *ent;
	int i;
	vec3_t dir;

	ent = &g_entities[0];
	for (i = 0; i < level.num_entities; i++, ent++) {
		if ( !ent->inuse || !ent->activator ) {
			continue;
		}
		if ( Q_stricmp(ent->activator->classname, classname) != 0 ) {
			continue;
		}
		VectorSubtract(goal->origin, ent->s.origin, dir);
		if (VectorLengthSquared(dir) < Square(10)) {
			goal->entitynum = i;
			return;
		}
	}
}

/*
==================
BotGoalForBSPEntity
==================
*/
int BotGoalForBSPEntity( char *classname, bot_goal_t *goal ) {
	char value[MAX_INFO_STRING];
	vec3_t origin, start, end;
	int ent, numareas, areas[10];

	memset(goal, 0, sizeof(bot_goal_t));
	for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
		if (!trap_AAS_ValueForBSPEpairKey(ent, "classname", value, sizeof(value)))
			continue;
		if (!strcmp(value, classname)) {
			if (!trap_AAS_VectorForBSPEpairKey(ent, "origin", origin))
				return qfalse;
			VectorCopy(origin, goal->origin);
			VectorCopy(origin, start);
			start[2] -= 32;
			VectorCopy(origin, end);
			end[2] += 32;
			numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
			if (!numareas)
				return qfalse;
			goal->areanum = areas[0];
			return qtrue;
		}
	}
	return qfalse;
}

/*
==================
BotSetupDeathmatchAI
==================
*/
void BotSetupDeathmatchAI(void) {
	int ent, modelnum;
	char model[128];

	gametype = trap_Cvar_VariableIntegerValue("g_gametype");

	trap_Cvar_Register(&bot_rocketjump, "bot_rocketjump", "1", 0);
	trap_Cvar_Register(&bot_grapple, "bot_grapple", "0", 0);
	trap_Cvar_Register(&bot_fastchat, "bot_fastchat", "0", 0);
	trap_Cvar_Register(&bot_nochat, "bot_nochat", "0", 0);
	trap_Cvar_Register(&bot_testrchat, "bot_testrchat", "0", 0);
	trap_Cvar_Register(&bot_challenge, "bot_challenge", "0", 0);
	trap_Cvar_Register(&bot_predictobstacles, "bot_predictobstacles", "1", 0);
	trap_Cvar_Register(&bot_autoskill, "bot_autoskill", "0", 0);
	trap_Cvar_Register(&g_spSkill, "g_spSkill", "3", 0);
	trap_Cvar_Register(&sv_botDirectiveTTL, "sv_botDirectiveTTL", "30000", CVAR_ARCHIVE);
	//
	if (gametype == GT_CTF) {
		if (trap_BotGetLevelItemGoal(-1, "Red Flag", &ctf_redflag) < 0)
			BotAI_Print(PRT_WARNING, "CTF without Red Flag\n");
		if (trap_BotGetLevelItemGoal(-1, "Blue Flag", &ctf_blueflag) < 0)
			BotAI_Print(PRT_WARNING, "CTF without Blue Flag\n");
	}
	else if (gametype == GT_1FCTF) {
		if (trap_BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
			BotAI_Print(PRT_WARNING, "One Flag CTF without Neutral Flag\n");
		if (trap_BotGetLevelItemGoal(-1, "Red Flag", &ctf_redflag) < 0)
			BotAI_Print(PRT_WARNING, "One Flag CTF without Red Flag\n");
		if (trap_BotGetLevelItemGoal(-1, "Blue Flag", &ctf_blueflag) < 0)
			BotAI_Print(PRT_WARNING, "One Flag CTF without Blue Flag\n");
	}
#if FEAT_OVERLOAD
	else if (gametype == GT_OBELISK) {
		if (trap_BotGetLevelItemGoal(-1, "Red Obelisk", &redobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Overload without Red Obelisk\n");
		BotSetEntityNumForGoalWithActivator(&redobelisk, "team_redobelisk");
		if (trap_BotGetLevelItemGoal(-1, "Blue Obelisk", &blueobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Overload without Blue Obelisk\n");
		BotSetEntityNumForGoalWithActivator(&blueobelisk, "team_blueobelisk");
	}
#endif
#if FEAT_HARVESTER
	else if (gametype == GT_HARVESTER) {
		if (trap_BotGetLevelItemGoal(-1, "Red Obelisk", &redobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without Red Obelisk\n");
		BotSetEntityNumForGoalWithActivator(&redobelisk, "team_redobelisk");
		if (trap_BotGetLevelItemGoal(-1, "Blue Obelisk", &blueobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without Blue Obelisk\n");
		BotSetEntityNumForGoalWithActivator(&blueobelisk, "team_blueobelisk");
		if (trap_BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without Neutral Obelisk\n");
		BotSetEntityNumForGoalWithActivator(&neutralobelisk, "team_neutralobelisk");
	}
#endif

	max_bspmodelindex = 0;
	for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
		if (!trap_AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model))) continue;
		if (model[0] == '*') {
			modelnum = atoi(model+1);
			if (modelnum > max_bspmodelindex)
				max_bspmodelindex = modelnum;
		}
	}
	//initialize the waypoint heap
	BotInitWaypoints();
}

/*
==================
BotShutdownDeathmatchAI
==================
*/
void BotShutdownDeathmatchAI(void) {
	altroutegoals_setup = qfalse;
}
