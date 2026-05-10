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
// cg_event.c -- handle entity events at snapshot or playerstate transitions

#include "cg_local.h"

// for the voice chats
#include "../qcommon/menudef.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );
//==========================================================================

/*
===================
CG_PlaceString

Also called by scoreboard drawing
===================
*/
const char	*CG_PlaceString( int rank ) {
	static char	str[64];
	const char	*s, *t;

	if ( rank & RANK_TIED_FLAG ) {
		rank &= ~RANK_TIED_FLAG;
		t = "Tied for ";
	} else {
		t = "";
	}

	if ( rank == 1 ) {
		s = S_COLOR_BLUE "1st" S_COLOR_WHITE;		// draw in blue
	} else if ( rank == 2 ) {
		s = S_COLOR_RED "2nd" S_COLOR_WHITE;		// draw in red
	} else if ( rank == 3 ) {
		s = S_COLOR_YELLOW "3rd" S_COLOR_WHITE;		// draw in yellow
	} else if ( rank == 11 ) {
		s = "11th";
	} else if ( rank == 12 ) {
		s = "12th";
	} else if ( rank == 13 ) {
		s = "13th";
	} else if ( rank % 10 == 1 ) {
		s = va("%ist", rank);
	} else if ( rank % 10 == 2 ) {
		s = va("%ind", rank);
	} else if ( rank % 10 == 3 ) {
		s = va("%ird", rank);
	} else {
		s = va("%ith", rank);
	}

	Com_sprintf( str, sizeof( str ), "%s%s", t, s );
	return str;
}

/*
===================
CG_ClientName

Returns a team-colored player name from a clientInfo_t.
Uses rotating static buffers (like va) — safe to call twice in one expression.
===================
*/
const char *CG_ClientName( const clientInfo_t *ci ) {
	static char	name[2][64];
	static int	toggle;
	char		*buf = name[toggle & 1];
	const char	*color;

	toggle++;

	if ( !ci || !ci->infoValid ) {
		Q_strncpyz( buf, S_COLOR_GREEN "noname" S_COLOR_WHITE, sizeof(name[0]) );
		return buf;
	}

	color = ( ci->team == TEAM_RED ) ? S_COLOR_RED :
			( ci->team == TEAM_BLUE ) ? S_COLOR_BLUE : S_COLOR_GREEN;

	Com_sprintf( buf, sizeof(name[0]), "%s%s" S_COLOR_WHITE, color, ci->name );
	return buf;
}

/*
===================
CG_ClientNameByNum

Returns a team-colored player name for the given client number.
Convenience wrapper around CG_ClientName.
===================
*/
const char *CG_ClientNameByNum( int clientNum ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return CG_ClientName( NULL );
	}
	return CG_ClientName( &cgs.clientinfo[clientNum] );
}

/*
=============
CG_Obituary
=============
*/
static void CG_Obituary( entityState_t *ent ) {
	int			mod;
	int			target, attacker;
	char		*message;
	char		*message2;
	clientInfo_t	*ci;

	target = ent->otherEntityNum;
	if ( target < 0 || target >= MAX_CLIENTS ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_Obituary: target out of range" );
	}

	attacker = ent->otherEntityNum2;
	if ( attacker < 0 || attacker >= MAX_CLIENTS ) {
		attacker = ENTITYNUM_WORLD;
	}

	message2 = "";

	// check for single client messages
	mod = ent->eventParm;

	switch( mod ) {
	case MOD_SUICIDE:
		message = "suicides";
		break;
	case MOD_FALLING:
		message = "cratered";
		break;
	case MOD_CRUSH:
		message = "was squished";
		break;
	case MOD_WATER:
		message = "sank like a rock";
		break;
	case MOD_SLIME:
		message = "melted";
		break;
	case MOD_LAVA:
		message = "does a back flip into the lava";
		break;
	case MOD_TARGET_LASER:
		message = "saw the light";
		break;
	case MOD_TRIGGER_HURT:
		message = "was in the wrong place";
		break;
	case MOD_LAVABALL:
		message = "was incinerated by a lavaball";
		break;
	case MOD_NAIL:
		message = "was nailed";
		break;
	default:
		message = NULL;
		break;
	}

	ci = &cgs.clientinfo[target];

	if (attacker == target) {
		switch (mod) {
		case MOD_KAMIKAZE:
			message = "goes out with a bang";
			break;
		case MOD_GRENADE_SPLASH:
			message = "tripped on their own grenade";
			break;
		case MOD_ROCKET_SPLASH:
		case MOD_ROCKET_MORTAR_SPLASH:
			message = "blew themself up";
			break;
// eser - lightning discharge
        case MOD_LIGHTNING_DISCHARGE:
			message = "discharged themself";
            break;
// eser - lightning discharge
        default:
			message = "killed themself";
			break;
		}
	}

	if (message) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%s %s.\n", CG_ClientNameByNum( target ), message);
#if FEAT_WIRED_UI
		trap_WiredUI_PushEvent( WIRED_EVENT_OBITUARY,
			va( "%d|%d|%d|%d", attacker, target, mod, 0 ) );
#endif
		return;
	}

	// check for kill messages from the current clientNum
	if ( attacker == cg.snap->ps.clientNum ) {
		const char	*s;

		if ( !cgs.gametypeIsTeamGame ) {
			s = va("You fragged %s\n%s place with %i", CG_ClientNameByNum( target ),
				CG_PlaceString( cg.snap->ps.persistant[PERS_RANK] + 1 ),
				cg.snap->ps.persistant[PERS_SCORE] );
		} else {
			s = va("You fragged %s", CG_ClientNameByNum( target ) );
		}

		if (!cg_cameraOrbit.integer) {
#if FEAT_WIRED_UI
			if ( !cgs.gametypeIsTeamGame ) {
				trap_WiredUI_PushEvent( WIRED_EVENT_FRAG_RANK,
					va( "You fragged %s|%s place with %i",
						CG_ClientNameByNum( target ),
						CG_PlaceString( cg.snap->ps.persistant[PERS_RANK] + 1 ),
						cg.snap->ps.persistant[PERS_SCORE] ) );
			} else {
				trap_WiredUI_PushEvent( WIRED_EVENT_FRAG_RANK,
					va( "You fragged %s", CG_ClientNameByNum( target ) ) );
			}
#else
			CG_CenterPrint( s, 144, BIGCHAR_WIDTH );
#endif
		}
	}

	if ( attacker != ENTITYNUM_WORLD ) {
		// check for kill messages about the current clientNum
		if ( target == cg.snap->ps.clientNum ) {
			Q_strncpyz( cg.killerName, CG_ClientNameByNum( attacker ), sizeof( cg.killerName ) );
#if FEAT_FOLLOW_KILLER
			cg.killerClientNum = attacker;
			cg.followKillerPending = qtrue;
#endif
		}

		switch (mod) {
		case MOD_GRAPPLE:
			message = "was caught by";
			break;
		case MOD_GAUNTLET:
		case MOD_GAUNTLET_LUNGE:
			message = "was pummeled by";
			break;
		case MOD_MACHINEGUN:
		case MOD_MACHINEGUN_BURST:
			message = "was machinegunned by";
			break;
		case MOD_SHOTGUN:
		case MOD_SHOTGUN_DOUBLE_BLAST:
			message = "was gunned down by";
			break;
		case MOD_GRENADE:
			message = "ate";
			message2 = "'s grenade";
			break;
		case MOD_GRENADE_SPLASH:
			message = "ate";
			message2 = "'s shrapnel";
			break;
		case MOD_ROCKET:
		case MOD_ROCKET_MORTAR:
			message = "rides";
			message2 = "'s rocket";
			break;
		case MOD_ROCKET_SPLASH:
		case MOD_ROCKET_MORTAR_SPLASH:
			message = "almost dodged";
			message2 = "'s rocket";
			break;
		case MOD_PLASMA:
			message = "was melted by";
			message2 = "'s plasma rifle";
			break;
		case MOD_RAILGUN:
			message = "was slugged by";
			break;
		case MOD_LIGHTNING:
			message = "was electrocuted by";
			break;
		case MOD_LIGHTNING_CHAIN_ARC:
			message = "was arc'd by";
			break;
// eser - lightning discharge
        case MOD_LIGHTNING_DISCHARGE:
            message = "was discharged by";
            break;
// eser - lightning discharge
		case MOD_KAMIKAZE:
			message = "falls to";
			message2 = "'s Kamikaze blast";
			break;
		case MOD_TELEFRAG:
			message = "was telefragged by";
			break;
		default:
			message = "was killed by";
			break;
		}

		if (message) {
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%s %s %s%s\n",
				CG_ClientNameByNum( target ), message, CG_ClientNameByNum( attacker ), message2);
#if FEAT_WIRED_UI
			trap_WiredUI_PushEvent( WIRED_EVENT_OBITUARY,
				va( "%d|%d|%d|%d", attacker, target, mod, 0 ) );
#endif
			return;
		}
	}

	// we don't know what it was
	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%s died.\n", CG_ClientNameByNum( target ) );
#if FEAT_WIRED_UI
	trap_WiredUI_PushEvent( WIRED_EVENT_OBITUARY,
		va( "%d|%d|%d|%d", ENTITYNUM_WORLD, target, mod, 0 ) );
#endif
}

//==========================================================================

/*
===============
CG_UseItem
===============
*/
static void CG_UseItem( centity_t *cent ) {
	clientInfo_t *ci;
	int			itemNum, clientNum;
	gitem_t		*item;
	entityState_t *es;

	es = &cent->currentState;

	itemNum = (es->event & ~EV_EVENT_BITS) - EV_USE_ITEM0;
	if ( itemNum < 0 || itemNum > HI_NUM_HOLDABLE ) {
		itemNum = 0;
	}

	// print a message if the local player
	if ( es->number == cg.snap->ps.clientNum ) {
		if ( !itemNum ) {
			CG_CenterPrint( "No item to use", 144, BIGCHAR_WIDTH );
		} else {
			item = BG_FindItemForHoldable( itemNum );
			CG_CenterPrint( va("Use %s", item->pickup_name), 144, BIGCHAR_WIDTH );
		}
	}

	switch ( itemNum ) {
	default:
	case HI_NONE:
		trap_S_StartSound (NULL, es->number, CHAN_BODY, cgs.media.useNothingSound );
		break;

	case HI_TELEPORTER:
		break;

	case HI_MEDKIT:
		clientNum = cent->currentState.clientNum;
		if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
			ci = &cgs.clientinfo[ clientNum ];
			ci->medkitUsageTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_BODY, cgs.media.medkitSound );
		break;

	case HI_KAMIKAZE:
		break;

#if FEAT_PW_PORTAL
	case HI_PORTAL:
		break;
#endif

	case HI_DEFLECTOR:
		trap_S_StartSound (NULL, es->number, CHAN_BODY, cgs.media.useDeflectorSound );
		break;
	}

}

/*
================
CG_ItemPickup

A new item was picked up this frame
================
*/
static void CG_ItemPickup( int itemNum ) {
	cg.itemPickup = itemNum;
	cg.itemPickupTime = cg.time;
	cg.itemPickupBlendTime = cg.time;

	// see if it should be the grabbed weapon
	if ( bg_itemlist[itemNum].giType == IT_WEAPON ) {
		cg.lastGrabbedWeapon = bg_itemlist[itemNum].giTag;
		// select it immediately
		if ( cg_autoswitch.integer && bg_itemlist[itemNum].giTag != WP_MACHINEGUN ) {
			cg.weaponSelectTime = cg.time;
			cg.weaponSelect = bg_itemlist[itemNum].giTag;
		}
	}

}

/*
================
CG_WaterLevel

Returns waterlevel for entity origin
================
*/
int CG_WaterLevel(centity_t *cent) {
	vec3_t point;
	int contents, sample1, sample2, anim, waterlevel;
	int viewheight;

	anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;

	if (anim == LEGS_WALKCR || anim == LEGS_IDLECR) {
		viewheight = CROUCH_VIEWHEIGHT;
	} else {
		viewheight = DEFAULT_VIEWHEIGHT;
	}

	//
	// get waterlevel, accounting for ducking
	//
	waterlevel = WATERLEVEL_NONE;

	point[0] = cent->lerpOrigin[0];
	point[1] = cent->lerpOrigin[1];
	point[2] = cent->lerpOrigin[2] + MINS_Z + 1;
	contents = CG_PointContents(point, -1);

	if (contents & MASK_WATER) {
		sample2 = viewheight - MINS_Z;
		sample1 = sample2 / 2;
		waterlevel = WATERLEVEL_FEET;
		point[2] = cent->lerpOrigin[2] + MINS_Z + sample1;
		contents = CG_PointContents(point, -1);

		if (contents & MASK_WATER) {
			waterlevel = WATERLEVEL_HALFWAY;
			point[2] = cent->lerpOrigin[2] + MINS_Z + sample2;
			contents = CG_PointContents(point, -1);

			if (contents & MASK_WATER) {
				waterlevel = WATERLEVEL_SUBMERGED;
			}
		}
	}

	return waterlevel;
}

/*
================
CG_PainEvent

Also called by playerstate transition
================
*/
void CG_PainEvent( centity_t *cent, int health ) {
	int slot;
	clientInfo_t *ci = &cgs.clientinfo[cent->currentState.number < MAX_CLIENTS ? cent->currentState.number : 0];

	// don't do more than two pain sounds a second
	if ( cg.time - cent->pe.painTime < 500 ) {
		return;
	}

	if ( health < 25 )      slot = CSOUND_PAIN25;
	else if ( health < 50 ) slot = CSOUND_PAIN50;
	else if ( health < 75 ) slot = CSOUND_PAIN75;
	else                    slot = CSOUND_PAIN100;

	// play a gurp sound instead of a normal pain sound
	if (CG_WaterLevel(cent) == WATERLEVEL_SUBMERGED) {
		trap_S_StartSound(NULL, cent->currentState.number, CHAN_VOICE,
			cgs.media.gurpSound[rand() & 1]);
	} else {
		trap_S_StartSound(NULL, cent->currentState.number, CHAN_VOICE, ci->sounds[slot]);
	}
	// save pain time for programitic twitch animation
	cent->pe.painTime = cg.time;
	cent->pe.painDirection ^= 1;
}



/*
==============
CG_EntityEvent

An entity has an event value
also called by CG_CheckPlayerstateEvents
==============
*/
#define	DEBUGNAME(x) if(cg_debugEvents.integer){Com_Log( SEV_INFO, LOG_CH(ch_cgame), x"\n");}
void CG_EntityEvent( centity_t *cent, vec3_t position ) {
	entityState_t	*es;
	int				event;
	vec3_t			dir;
	const char		*s;
	int				clientNum;
	clientInfo_t	*ci;
	qboolean        hasSound;

	es = &cent->currentState;
	event = es->event & ~EV_EVENT_BITS;

	if ( cg_debugEvents.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "ent:%3i  event:%3i ", es->number, event );
	}

	if ( !event ) {
		DEBUGNAME("ZEROEVENT");
		return;
	}

	clientNum = es->clientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		clientNum = 0;
	}
	ci = &cgs.clientinfo[ clientNum ];

	hasSound = !CG_IsPlayerInvisible(cent);

	switch ( event ) {
	//
	// movement generated events
	//
	case EV_FOOTSTEP:
		DEBUGNAME("EV_FOOTSTEP");
		if (cg_footsteps.integer && hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_BODY,
				ci->footstepSounds[ FOOTSTEP_NORMAL ][rand()&3] );
		}
		break;
	case EV_FOOTSTEP_METAL:
		DEBUGNAME("EV_FOOTSTEP_METAL");
		if (cg_footsteps.integer && hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_BODY,
				ci->footstepSounds[ FOOTSTEP_METAL ][rand()&3] );
		}
		break;
	case EV_FOOTSPLASH:
		DEBUGNAME("EV_FOOTSPLASH");
		if (cg_footsteps.integer && hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_BODY,
				ci->footstepSounds[ FOOTSTEP_SPLASH ][rand()&3] );
		}
		break;
	case EV_FOOTWADE:
		DEBUGNAME("EV_FOOTWADE");
		if (cg_footsteps.integer && hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_BODY,
				ci->footstepSounds[ FOOTSTEP_SPLASH ][rand()&3] );
		}
		break;
	case EV_SWIM:
		DEBUGNAME("EV_SWIM");
		if (cg_footsteps.integer && hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_BODY,
				ci->footstepSounds[ FOOTSTEP_SPLASH ][rand()&3] );
		}
		break;


	case EV_FALL_SHORT:
		DEBUGNAME("EV_FALL_SHORT");
		if (hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, ci->effects.landSound );
		}
		if ( clientNum == cg.predictedPlayerState.clientNum ) {
			// smooth landing z changes
			cg.landChange = -8;
			cg.landTime = cg.time;
		}
		break;
	case EV_FALL_MEDIUM:
		DEBUGNAME("EV_FALL_MEDIUM");
		if (hasSound) {
			// use normal pain sound
			clientInfo_t *ci_fall = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			trap_S_StartSound( NULL, es->number, CHAN_VOICE, ci_fall->sounds[CSOUND_PAIN100] );
		}
		if ( clientNum == cg.predictedPlayerState.clientNum ) {
			// smooth landing z changes
			cg.landChange = -16;
			cg.landTime = cg.time;
		}
		break;
	case EV_FALL_FAR:
		DEBUGNAME("EV_FALL_FAR");
		if (hasSound) {
			clientInfo_t *ci_fall2 = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, ci_fall2->sounds[CSOUND_FALL] );
		}
		cent->pe.painTime = cg.time;	// don't play a pain sound right after this
		if ( clientNum == cg.predictedPlayerState.clientNum ) {
			// smooth landing z changes
			cg.landChange = -24;
			cg.landTime = cg.time;
		}
		break;

	case EV_STEP_4:
	case EV_STEP_8:
	case EV_STEP_12:
	case EV_STEP_16:		// smooth out step up transitions
		DEBUGNAME("EV_STEP");
	{
		float	oldStep;
		int		delta;
		int		step;

		if ( clientNum != cg.predictedPlayerState.clientNum ) {
			break;
		}
		// if we are interpolating, we don't need to smooth steps
		if ( cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) ||
			cg_nopredict.integer || cg_synchronousClients.integer ) {
			break;
		}
		// check for stepping up before a previous step is completed
		delta = cg.time - cg.stepTime;
		if (delta < STEP_TIME) {
			oldStep = cg.stepChange * (STEP_TIME - delta) / STEP_TIME;
		} else {
			oldStep = 0;
		}

		// add this amount
		step = 4 * (event - EV_STEP_4 + 1 );
		cg.stepChange = oldStep + step;
		if ( cg.stepChange > MAX_STEP_CHANGE ) {
			cg.stepChange = MAX_STEP_CHANGE;
		}
		cg.stepTime = cg.time;
		break;
	}

	case EV_JUMP_PAD:
		DEBUGNAME("EV_JUMP_PAD");
//		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "EV_JUMP_PAD w/effect #%i\n", es->eventParm );
		{
			vec3_t			up = {0, 0, 1};


			CG_SmokePuff( cent->lerpOrigin, up,
						  32,
						  1, 1, 1, 0.33f,
						  1000,
						  cg.time, 0,
						  LEF_PUFF_DONT_SCALE,
						  cgs.media.smokePuffShader );
		}

		// boing sound at origin, jump sound on player
		trap_S_StartSound ( cent->lerpOrigin, -1, CHAN_VOICE, cgs.media.jumpPadSound );
		{
			clientInfo_t *ci_jp = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			trap_S_StartSound (NULL, es->number, CHAN_VOICE, ci_jp->sounds[CSOUND_JUMP] );
		}

		// Phase 5T: PTRAIL_PUSH trigger for jumppad launch. Trail is
		// anchored behind the launched player along their negative
		// velocity vector by CG_RenderOnePlayerTrail; no pad origin
		// needed here. `es->number` is the launched player's
		// clientNum.
		//
		// Speed source: predicted velocity for the local player
		// (post-bounce velocity is already applied client-side in
		// the predicted state); for remote players the snapped
		// trDelta still holds the pre-bounce vector at this moment
		// (post-bounce arrives next snap), so the < 100 fallback
		// gives a sensible default duration.
		{
			float pushSpeed = 0.0f;
			float t;
			int   durationMs;

			if ( es->number == cg.predictedPlayerState.clientNum ) {
				pushSpeed = VectorLength( cg.predictedPlayerState.velocity );
			} else if ( es->number >= 0 && es->number < MAX_CLIENTS ) {
				pushSpeed = VectorLength(
					cg_entities[es->number].currentState.pos.trDelta );
			}
			if ( pushSpeed < 100.0f ) pushSpeed = 800.0f;

			t = pushSpeed / PTRAIL_JUMPPAD_SPEED_NORM;
			if ( t > 1.0f ) t = 1.0f;
			durationMs = PTRAIL_JUMPPAD_DURATION_MIN_MS +
				(int)( t * ( PTRAIL_JUMPPAD_DURATION_MAX_MS -
				             PTRAIL_JUMPPAD_DURATION_MIN_MS ) );

			CG_TriggerPlayerTrail( es->number, PTRAIL_PUSH, durationMs );
		}
		break;

	case EV_JUMP:
		DEBUGNAME("EV_JUMP");
		if (hasSound) {
			clientInfo_t *ci_jump = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			trap_S_StartSound (NULL, es->number, CHAN_VOICE, ci_jump->sounds[CSOUND_JUMP] );
		}
		break;
	case EV_TAUNT:
		DEBUGNAME("EV_TAUNT");
		{
			clientInfo_t *ci_taunt = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			trap_S_StartSound (NULL, es->number, CHAN_VOICE, ci_taunt->sounds[CSOUND_TAUNT] );
		}
		break;
	case EV_TAUNT_YES:
		DEBUGNAME("EV_TAUNT_YES");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_YES);
		break;
	case EV_TAUNT_NO:
		DEBUGNAME("EV_TAUNT_NO");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_NO);
		break;
	case EV_TAUNT_FOLLOWME:
		DEBUGNAME("EV_TAUNT_FOLLOWME");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_FOLLOWME);
		break;
	case EV_TAUNT_GETFLAG:
		DEBUGNAME("EV_TAUNT_GETFLAG");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_ONGETFLAG);
		break;
	case EV_TAUNT_GUARDBASE:
		DEBUGNAME("EV_TAUNT_GUARDBASE");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_ONDEFENSE);
		break;
	case EV_TAUNT_PATROL:
		DEBUGNAME("EV_TAUNT_PATROL");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_ONPATROL);
		break;
	case EV_WATER_TOUCH:
		DEBUGNAME("EV_WATER_TOUCH");
		if (hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, ci->effects.watrInSound );
		}
		break;
	case EV_WATER_LEAVE:
		DEBUGNAME("EV_WATER_LEAVE");
		if (hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, ci->effects.watrOutSound );
		}
		break;
	case EV_WATER_UNDER:
		DEBUGNAME("EV_WATER_UNDER");
		if (hasSound) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, ci->effects.watrUnSound );
		}
		break;
	case EV_WATER_CLEAR:
		DEBUGNAME("EV_WATER_CLEAR");
		if (hasSound) {
			clientInfo_t *ci_gasp = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, ci_gasp->sounds[CSOUND_GASP] );
		}
		break;

	case EV_ITEM_PICKUP:
		DEBUGNAME("EV_ITEM_PICKUP");
		{
			gitem_t	*item;
			int		index;

			index = es->eventParm;		// player predicted

			if ( index < 1 || index >= bg_numItems ) {
				break;
			}
			item = &bg_itemlist[ index ];

			// powerups and team items will have a separate global sound, this one
			// will be played at prediction time
			if ( item->giType == IT_POWERUP || item->giType == IT_TEAM) {
				trap_S_StartSound (NULL, es->number, CHAN_AUTO,	cgs.media.n_healthSound );
			} else {
				trap_S_StartSound (NULL, es->number, CHAN_AUTO,	trap_S_RegisterSound( item->pickup_sound, qfalse ) );
			}

			// show icon and name on status bar
			if ( es->number == cg.snap->ps.clientNum ) {
				CG_ItemPickup( index );
			}
		}
		break;

	case EV_GLOBAL_ITEM_PICKUP:
		DEBUGNAME("EV_GLOBAL_ITEM_PICKUP");
		{
			gitem_t	*item;
			int		index;

			index = es->eventParm;		// player predicted

			if ( index < 1 || index >= bg_numItems ) {
				break;
			}
			item = &bg_itemlist[ index ];
			// powerup pickups are global
			if( item->pickup_sound ) {
				trap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, trap_S_RegisterSound( item->pickup_sound, qfalse ) );
			}

			// show icon and name on status bar
			if ( es->number == cg.snap->ps.clientNum ) {
				CG_ItemPickup( index );
			}
		}
		break;

	//
	// weapon events
	//
	case EV_NOAMMO:
		DEBUGNAME("EV_NOAMMO");
//		trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.noAmmoSound );
		if ( es->number == cg.snap->ps.clientNum ) {
			CG_OutOfAmmoChange();
		}
		break;
	case EV_CHANGE_WEAPON:
		DEBUGNAME("EV_CHANGE_WEAPON");
		trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.selectSound );
		break;
	case EV_FIRE_WEAPON_PRI:
		DEBUGNAME("EV_FIRE_WEAPON_PRI");
		CG_FireWeapon( cent );
		break;

	case EV_FIRE_WEAPON_SEC:
		DEBUGNAME("EV_FIRE_WEAPON_SEC");
		CG_FireWeapon( cent );
		// gauntlet lunge: play whoosh sound
		if ( cent->currentState.weapon == WP_GAUNTLET ) {
			trap_S_StartSound( NULL, cent->currentState.number, CHAN_WEAPON,
				trap_S_RegisterSound( "sound/weapons/melee/fstatck.opus", qfalse ) );
		}
		// shotgun double-blast: play sawed-off blast sound
		if ( cent->currentState.weapon == WP_SHOTGUN ) {
			// use existing shotgun sound as placeholder — distinct sound is v2
			trap_S_StartSound( NULL, cent->currentState.number, CHAN_WEAPON,
				trap_S_RegisterSound( "sound/weapons/shotgun/sshotf1b.opus", qfalse ) );
			// trigger screen shake for local player
			if ( cent->currentState.number == cg.snap->ps.clientNum ) {
				cg.doubleBlastKickTime = cg.time;
			}
		}
		break;

	case EV_USE_ITEM0:
		DEBUGNAME("EV_USE_ITEM0");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM1:
		DEBUGNAME("EV_USE_ITEM1");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM2:
		DEBUGNAME("EV_USE_ITEM2");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM3:
		DEBUGNAME("EV_USE_ITEM3");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM4:
		DEBUGNAME("EV_USE_ITEM4");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM5:
		DEBUGNAME("EV_USE_ITEM5");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM6:
		DEBUGNAME("EV_USE_ITEM6");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM7:
		DEBUGNAME("EV_USE_ITEM7");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM8:
		DEBUGNAME("EV_USE_ITEM8");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM9:
		DEBUGNAME("EV_USE_ITEM9");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM10:
		DEBUGNAME("EV_USE_ITEM10");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM11:
		DEBUGNAME("EV_USE_ITEM11");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM12:
		DEBUGNAME("EV_USE_ITEM12");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM13:
		DEBUGNAME("EV_USE_ITEM13");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM14:
		DEBUGNAME("EV_USE_ITEM14");
		CG_UseItem( cent );
		break;
	case EV_USE_ITEM15:
		DEBUGNAME("EV_USE_ITEM15");
		CG_UseItem( cent );
		break;

	//=================================================================

	//
	// other events
	//
	case EV_PLAYER_TELEPORT_IN:
		DEBUGNAME("EV_PLAYER_TELEPORT_IN");
		trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.teleInSound );
		CG_SpawnEffect( position);
		break;

	case EV_PLAYER_TELEPORT_OUT:
		DEBUGNAME("EV_PLAYER_TELEPORT_OUT");
		trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.teleOutSound );
		CG_SpawnEffect(  position);
		break;

	case EV_ITEM_POP:
		DEBUGNAME("EV_ITEM_POP");
		trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.respawnSound );
		break;
	case EV_ITEM_RESPAWN:
		DEBUGNAME("EV_ITEM_RESPAWN");
		cent->miscTime = cg.time;	// scale up from this
		trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.respawnSound );
		break;

	case EV_GRENADE_BOUNCE:
		DEBUGNAME("EV_GRENADE_BOUNCE");
		if ( rand() & 1 ) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.hgrenb1aSound );
		} else {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.hgrenb2aSound );
		}
		break;

	case EV_KAMIKAZE:
		DEBUGNAME("EV_KAMIKAZE");
		CG_KamikazeEffect( cent->lerpOrigin );
		break;
#if FEAT_OVERLOAD
	case EV_OBELISKEXPLODE:
		DEBUGNAME("EV_OBELISKEXPLODE");
		CG_ObeliskExplode( cent->lerpOrigin, es->eventParm );
		break;
	case EV_OBELISKPAIN:
		DEBUGNAME("EV_OBELISKPAIN");
		CG_ObeliskPain( cent->lerpOrigin );
		break;
#endif
	case EV_DEFLECTOR_IMPACT:
		DEBUGNAME("EV_DEFLECTOR_IMPACT");
		CG_DeflectorImpact( cent->lerpOrigin, cent->currentState.angles );
		break;
	case EV_DEFLECTOR_JUICED:
		DEBUGNAME("EV_DEFLECTOR_JUICED");
		CG_DeflectorJuiced( cent->lerpOrigin );
		break;
	case EV_LIGHTNINGBOLT:
		DEBUGNAME("EV_LIGHTNINGBOLT");
		CG_LightningBoltBeam(es->origin2, es->pos.trBase);
		break;
	case EV_LIGHTNING_ARC:
		DEBUGNAME("EV_LIGHTNING_ARC");
		CG_LightningArcBeam( es->origin, es->origin2 );
		trap_S_StartSound( es->origin, es->number, CHAN_AUTO, cgs.media.sfx_lightningArcLoop );
		// Screen shake on new arc connection
		if ( es->otherEntityNum != cg.lastArcTarget ) {
			cg.lastArcTarget = es->otherEntityNum;
			cg.lastArcTime = cg.time;
		}
		break;
	case EV_SCOREPLUM:
		DEBUGNAME("EV_SCOREPLUM");
		CG_ScorePlum( cent->currentState.otherEntityNum, cent->lerpOrigin, cent->currentState.time );
		break;

#if FEAT_DAMAGE_PLUMS
	case EV_DAMAGEPLUM:
		DEBUGNAME("EV_DAMAGEPLUM");
		CG_DamagePlum( cent->currentState.otherEntityNum, cent->lerpOrigin, cent->currentState.time );
		break;
#endif
#if FEAT_PING_LOCATION
	case EV_PING_LOCATION:
		DEBUGNAME("EV_PING_LOCATION");
		CG_PingLocation( cent );
		break;
#endif

#if FEAT_FREEZETAG
	case EV_FREEZE:
		DEBUGNAME("EV_FREEZE");
		trap_S_StartSound( NULL, es->number, CHAN_BODY, cgs.media.teleInSound );
		break;
#endif

	//
	// missile impacts
	//
	case EV_MISSILE_HIT:
		DEBUGNAME("EV_MISSILE_HIT");
		ByteToDir( es->eventParm, dir );
		CG_MissileHitPlayer( es->pType, position, dir, es->otherEntityNum );
		break;

	case EV_MISSILE_MISS:
		DEBUGNAME("EV_MISSILE_MISS");
		ByteToDir( es->eventParm, dir );
		CG_MissileHitWall( es->pType, 0, position, dir, IMPACTSOUND_DEFAULT, es->number );
		break;

	case EV_MISSILE_MISS_METAL:
		DEBUGNAME("EV_MISSILE_MISS_METAL");
		ByteToDir( es->eventParm, dir );
		CG_MissileHitWall( es->pType, 0, position, dir, IMPACTSOUND_METAL, es->number );
		break;

	case EV_RAILTRAIL:
		DEBUGNAME("EV_RAILTRAIL");
		cent->currentState.weapon = WP_RAILGUN;

        if (((cg_drawGun.integer == 2) || (cg_drawGun.integer == 3)) && es->clientNum == cg.snap->ps.clientNum && !cg.renderingThirdPerson) {
            int pos;
            vec3_t railtrail_origin;

            if (cg_drawGun.integer == 2) {
                pos = -4;
            }
            else {
                pos = -8;
            }

            AngleVectors(cg.snap->ps.viewangles, NULL, railtrail_origin, NULL);
            VectorMA(es->origin2, pos, railtrail_origin, railtrail_origin);

            CG_RailTrail(ci, railtrail_origin, es->pos.trBase);
        }
        else {
            CG_RailTrail(ci, es->origin2, es->pos.trBase);
        }

		// if the end was on a nomark surface, don't make an explosion
		if ( es->eventParm != 255 ) {
			ByteToDir( es->eventParm, dir );
			CG_MissileHitWall( PROJ_RAILGUN, es->clientNum, position, dir, IMPACTSOUND_DEFAULT, es->number );
		}
		break;

	case EV_BULLET_HIT_WALL:
		DEBUGNAME("EV_BULLET_HIT_WALL");
		ByteToDir( es->eventParm, dir );
		CG_Bullet( es->pos.trBase, es->otherEntityNum, dir, qfalse, ENTITYNUM_WORLD );
		break;

	case EV_BULLET_HIT_FLESH:
		DEBUGNAME("EV_BULLET_HIT_FLESH");
		CG_Bullet( es->pos.trBase, es->otherEntityNum, dir, qtrue, es->eventParm );
		break;

	case EV_SHOTGUN:
		DEBUGNAME("EV_SHOTGUN");
		CG_ShotgunFire( es );
		break;

	case EV_SHOTGUN_WIDE:
		DEBUGNAME("EV_SHOTGUN_WIDE");
		CG_ShotgunFireWide( es );
		break;

// eser - lightning discharge
    case EV_LIGHTNING_DISCHARGE:
        DEBUGNAME("EV_LIGHTNING_DISCHARGE");
        CG_Lightning_Discharge(position, es->eventParm);	// eventParm is duration/size
        break;
// eser - lightning discharge

	case EV_GENERAL_SOUND:
		DEBUGNAME("EV_GENERAL_SOUND");
		if ( cgs.gameSounds[ es->eventParm ] ) {
			trap_S_StartSound (NULL, es->number, CHAN_VOICE, cgs.gameSounds[ es->eventParm ] );
		} else {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			trap_S_StartSound (NULL, es->number, CHAN_VOICE, trap_S_RegisterSound( s, qfalse ) );
		}
		break;

	case EV_GLOBAL_SOUND:	// play from the player's head so it never diminishes
		DEBUGNAME("EV_GLOBAL_SOUND");
		if ( cgs.gameSounds[ es->eventParm ] ) {
			trap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[ es->eventParm ] );
		} else {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			trap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, trap_S_RegisterSound( s, qfalse ) );
		}
		break;

	case EV_GLOBAL_TEAM_SOUND:	// play from the player's head so it never diminishes
		{
			DEBUGNAME("EV_GLOBAL_TEAM_SOUND");
			switch( es->eventParm ) {
				case GTS_RED_CAPTURE: // CTF: red team captured the blue flag, 1FCTF: red team captured the neutral flag
					if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED )
						CG_AddBufferedSound( cgs.media.captureYourTeamSound );
					else
						CG_AddBufferedSound( cgs.media.captureOpponentSound );
					break;
				case GTS_BLUE_CAPTURE: // CTF: blue team captured the red flag, 1FCTF: blue team captured the neutral flag
					if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE )
						CG_AddBufferedSound( cgs.media.captureYourTeamSound );
					else
						CG_AddBufferedSound( cgs.media.captureOpponentSound );
					break;
				case GTS_RED_RETURN: // CTF: blue flag returned, 1FCTF: never used
					if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED )
						CG_AddBufferedSound( cgs.media.returnYourTeamSound );
					else
						CG_AddBufferedSound( cgs.media.returnOpponentSound );
					//
					CG_AddBufferedSound( cgs.media.blueFlagReturnedSound );
					break;
				case GTS_BLUE_RETURN: // CTF red flag returned, 1FCTF: neutral flag returned
					if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE )
						CG_AddBufferedSound( cgs.media.returnYourTeamSound );
					else
						CG_AddBufferedSound( cgs.media.returnOpponentSound );
					//
					CG_AddBufferedSound( cgs.media.redFlagReturnedSound );
					break;

				case GTS_RED_TAKEN: // CTF: red team took blue flag, 1FCTF: blue team took the neutral flag
					// if this player picked up the flag then a sound is played in CG_CheckLocalSounds
					if (cg.snap->ps.powerups[PW_BLUEFLAG] || cg.snap->ps.powerups[PW_NEUTRALFLAG]) {
					}
					else {
						if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
							if (cgs.gametype == GT_1FCTF)
								CG_AddBufferedSound( cgs.media.yourTeamTookTheFlagSound );
							else
								CG_AddBufferedSound( cgs.media.enemyTookYourFlagSound );
						}
						else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
							if (cgs.gametype == GT_1FCTF)
								CG_AddBufferedSound( cgs.media.enemyTookTheFlagSound );
							else
	 							CG_AddBufferedSound( cgs.media.yourTeamTookEnemyFlagSound );
						}
					}
					break;
				case GTS_BLUE_TAKEN: // CTF: blue team took the red flag, 1FCTF red team took the neutral flag
					// if this player picked up the flag then a sound is played in CG_CheckLocalSounds
					if (cg.snap->ps.powerups[PW_REDFLAG] || cg.snap->ps.powerups[PW_NEUTRALFLAG]) {
					}
					else {
						if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
							if (cgs.gametype == GT_1FCTF)
								CG_AddBufferedSound( cgs.media.yourTeamTookTheFlagSound );
							else
								CG_AddBufferedSound( cgs.media.enemyTookYourFlagSound );
						}
						else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
							if (cgs.gametype == GT_1FCTF)
								CG_AddBufferedSound( cgs.media.enemyTookTheFlagSound );
							else
								CG_AddBufferedSound( cgs.media.yourTeamTookEnemyFlagSound );
						}
					}
					break;
#if FEAT_OVERLOAD
				case GTS_REDOBELISK_ATTACKED: // Overload: red obelisk is being attacked
					if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
						CG_AddBufferedSound( cgs.media.yourBaseIsUnderAttackSound );
					}
					break;
				case GTS_BLUEOBELISK_ATTACKED: // Overload: blue obelisk is being attacked
					if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
						CG_AddBufferedSound( cgs.media.yourBaseIsUnderAttackSound );
					}
					break;
#endif

				case GTS_REDTEAM_SCORED:
					CG_AddBufferedSound(cgs.media.redScoredSound);
					break;
				case GTS_BLUETEAM_SCORED:
					CG_AddBufferedSound(cgs.media.blueScoredSound);
					break;
				case GTS_REDTEAM_TOOK_LEAD:
					CG_AddBufferedSound(cgs.media.redLeadsSound);
					break;
				case GTS_BLUETEAM_TOOK_LEAD:
					CG_AddBufferedSound(cgs.media.blueLeadsSound);
					break;
				case GTS_TEAMS_ARE_TIED:
					CG_AddBufferedSound( cgs.media.teamsTiedSound );
					break;
				case GTS_KAMIKAZE:
					trap_S_StartLocalSound(cgs.media.kamikazeFarSound, CHAN_ANNOUNCER);
					break;
				default:
					break;
			}
			break;
		}

	case EV_PAIN:
		// local player sounds are triggered in CG_CheckLocalSounds,
		// so ignore events on the player
		DEBUGNAME("EV_PAIN");
		if ( cent->currentState.number != cg.snap->ps.clientNum ) {
			CG_PainEvent( cent, es->eventParm );
		}
		break;

	case EV_DEATH1:
	case EV_DEATH2:
	case EV_DEATH3:
		DEBUGNAME("EV_DEATHx");
		{
			clientInfo_t *ci_death = &cgs.clientinfo[es->number < MAX_CLIENTS ? es->number : 0];
			if (CG_WaterLevel(cent) == WATERLEVEL_SUBMERGED) {
				trap_S_StartSound(NULL, es->number, CHAN_VOICE, ci_death->sounds[CSOUND_DROWN]);
			} else {
				trap_S_StartSound(NULL, es->number, CHAN_VOICE,
					ci_death->sounds[CSOUND_DEATH1 + (event - EV_DEATH1)]);
			}
		}
		break;


	case EV_OBITUARY:
		DEBUGNAME("EV_OBITUARY");
		CG_Obituary( es );
		break;

	//
	// powerup events
	//
	case EV_POWERUP_QUAD:
		DEBUGNAME("EV_POWERUP_QUAD");
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_QUAD;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.quadSound );
		break;
	case EV_POWERUP_BERSERK:
		DEBUGNAME("EV_POWERUP_BERSERK");
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_BERSERK;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.berserkSound );
		break;
	case EV_POWERUP_BATTLESUIT:
		DEBUGNAME("EV_POWERUP_BATTLESUIT");
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_BATTLESUIT;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.protectSound );
		break;
	case EV_POWERUP_REGEN:
		DEBUGNAME("EV_POWERUP_REGEN");
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_REGEN;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.regenSound );
		break;

	case EV_GIB_PLAYER:
		DEBUGNAME("EV_GIB_PLAYER");
		// don't play gib sound when using the kamikaze because it interferes
		// with the kamikaze sound, downside is that the gib sound will also
		// not be played when someone is gibbed while just carrying the kamikaze
		if ( !(es->eFlags & EF_KAMIKAZE) ) {
			trap_S_StartSound( NULL, es->number, CHAN_BODY, ci->effects.gibSound );
		}
		CG_GibPlayer( cent->lerpOrigin );
		break;

	case EV_STOPLOOPINGSOUND:
		DEBUGNAME("EV_STOPLOOPINGSOUND");
		trap_S_StopLoopingSound( es->number );
		es->loopSound = 0;
		break;

	case EV_DEBUG_LINE:
		DEBUGNAME("EV_DEBUG_LINE");
		CG_Beam( cent );
		break;

#if FEAT_EARTHQUAKE_SYSTEM
	case EV_EARTHQUAKE:
		DEBUGNAME("EV_EARTHQUAKE");
		CG_AddEarthquake(
			es->origin,
			es->angles2[1],
			es->angles[0],
			es->angles[1],
			es->angles[2],
			es->angles2[0]
		);
		break;
#endif

	default:
		DEBUGNAME("UNKNOWN");
		Com_Terminate( TERM_CLIENT_DROP, "Unknown event: %i", event );
		break;
	}

}


/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents( centity_t *cent ) {
	// check for event-only entities
	if ( cent->currentState.eType > ET_EVENTS ) {
		if ( cent->previousEvent ) {
			return;	// already fired
		}
		// if this is a player event set the entity number of the client entity number
		if ( cent->currentState.eFlags & EF_PLAYER_EVENT ) {
			cent->currentState.number = cent->currentState.otherEntityNum;
		}

		cent->previousEvent = 1;

		cent->currentState.event = cent->currentState.eType - ET_EVENTS;
	} else {
		// check for events riding with another entity
		if ( cent->currentState.event == cent->previousEvent ) {
			return;
		}
		cent->previousEvent = cent->currentState.event;
		if ( ( cent->currentState.event & ~EV_EVENT_BITS ) == 0 ) {
			return;
		}
	}

	// calculate the position at exactly the frame time
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
	CG_SetEntitySoundPosition( cent );

	CG_EntityEvent( cent, cent->lerpOrigin );
}
