// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//

#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );


/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player ) {
	gclient_t	*client;
	float	count;
	vec3_t	angles;

	client = player->client;
	if ( client->ps.pm_type == PM_DEAD ) {
		return;
	}

	// total points of damage shot at the player this frame
	count = client->damage_blood + client->damage_armor;
	if ( count == 0 ) {
		return;		// didn't take any damage
	}

	if ( count > 255 ) {
		count = 255;
	}

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if ( client->damage_fromWorld ) {
		client->ps.damagePitch = 255;
		client->ps.damageYaw = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles( client->damage_from, angles );
		client->ps.damagePitch = angles[PITCH]/360.0 * 256;
		client->ps.damageYaw = angles[YAW]/360.0 * 256;
	}

	// play an appropriate pain sound
	if ( (level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) ) {
		player->pain_debounce_time = level.time + 700;
		G_AddEvent( player, EV_PAIN, player->health );
		client->ps.damageEvent++;
	}


	client->ps.damageCount = count;

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_knockback = 0;
}



/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent ) {
	qboolean	envirosuit;
	int			waterlevel;

	if ( ent->client->noclip ) {
		ent->client->airOutTime = level.time + 12000;	// don't need air
		return;
	}

	waterlevel = ent->waterlevel;

	envirosuit = ent->client->ps.powerups[PW_BATTLESUIT] > level.time;

	//
	// check for drowning
	//
	if ( waterlevel == WATERLEVEL_SUBMERGED ) {
		// envirosuit give air
		if ( envirosuit ) {
			ent->client->airOutTime = level.time + 10000;
		}

		// if out of air, start drowning
		if ( ent->client->airOutTime < level.time) {
			// drown!
			ent->client->airOutTime += 1000;
			if ( ent->health > 0 ) {
				// take more damage the longer underwater
				ent->damage += 2;
				if (ent->damage > 15)
					ent->damage = 15;

				// don't play a normal pain sound
				ent->pain_debounce_time = level.time + 200;

				G_Damage (ent, NULL, NULL, NULL, NULL,
					ent->damage, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	} else {
		ent->client->airOutTime = level.time + 12000;
		ent->damage = 2;
	}

	//
	// check for sizzle damage (move to pmove?)
	//
	if (waterlevel &&
		(ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		if (ent->health > 0
			&& ent->pain_debounce_time <= level.time	) {

			if ( envirosuit ) {
				G_AddEvent( ent, EV_POWERUP_BATTLESUIT, 0 );
			} else {
				if (ent->watertype & CONTENTS_LAVA) {
					G_Damage (ent, NULL, NULL, NULL, NULL,
						30*waterlevel, 0, MOD_LAVA);
				}

				if (ent->watertype & CONTENTS_SLIME) {
					G_Damage (ent, NULL, NULL, NULL, NULL,
						10*waterlevel, 0, MOD_SLIME);
				}
			}
		}
	}
}



/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent ) {
	if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		ent->client->ps.loopSound = level.snd_fry;
	} else {
		ent->client->ps.loopSound = 0;
	}
}



//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( gentity_t *ent, pmove_t *pm ) {
	int		i, j;
	trace_t	trace;
	gentity_t	*other;

	memset( &trace, 0, sizeof( trace ) );
	for (i=0 ; i<pm->numtouch ; i++) {
		for (j=0 ; j<i ; j++) {
			if (pm->touchents[j] == pm->touchents[i] ) {
				break;
			}
		}
		if (j != i) {
			continue;	// duplicated
		}
		other = &g_entities[ pm->touchents[i] ];

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, other, &trace );
		}

		if ( !other->touch ) {
			continue;
		}

		other->touch( other, ent, &trace );
	}

}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void	G_TouchTriggers( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs;
	vec3_t		range = { ITEM_PICKUP_SIZE + 14, ITEM_PICKUP_SIZE + 4, ITEM_PICKUP_SIZE + 16 };

	if ( !ent->client ) {
		return;
	}

	// dead clients don't activate triggers!
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// can't use ent->absmin, because that has a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	for ( i=0 ; i<num ; i++ ) {
		hit = &g_entities[touch[i]];

		if ( !hit->touch && !ent->touch ) {
			continue;
		}
		if ( !( hit->r.contents & CONTENTS_TRIGGER ) ) {
			continue;
		}

		// ignore most entities if a spectator
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER &&
				// this is ugly but adding a new ET_? type will
				// most likely cause network incompatibilities
				hit->touch != Q3_Touch_DoorTrigger) {
				continue;
			}
		}

		// use separate code for determining if an item is picked up
		// so you don't have to actually contact its bounding box
		if ( hit->s.eType == ET_ITEM ) {
			if ( !BG_PlayerTouchesItem( &ent->client->ps, &hit->s, level.time ) ) {
				continue;
			}
		} else {
			if ( !trap_EntityContact( mins, maxs, hit ) ) {
				continue;
			}
		}

		memset( &trace, 0, sizeof(trace) );

		if ( hit->touch ) {
			hit->touch (hit, ent, &trace);
		}

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, hit, &trace );
		}
	}

	// if we didn't touch a jump pad this pmove frame
	if ( ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount ) {
		ent->client->ps.jumppad_frame = 0;
		ent->client->ps.jumppad_ent = 0;
	}
}

/*
=================
SpectatorThink
=================
*/
#if FEAT_SCREENSHOT_TOOLS
static const float timeScaleFromMode[16] = {
	1.00f, 0.75f, 0.50f, 0.33f, 0.25f,
	0.20f, 0.15f, 0.10f, 0.08f, 0.05f,
	1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f
};
#endif

void SpectatorThink( gentity_t *ent, usercmd_t *ucmd ) {
	pmove_t	pm;
	gclient_t	*client;

	client = ent->client;

	if ( client->sess.spectatorState != SPECTATOR_FOLLOW || !( client->ps.pm_flags & PMF_FOLLOW ) ) {
		if ( client->sess.spectatorState == SPECTATOR_FREE ) {
			if ( client->noclip ) {
				client->ps.pm_type = PM_NOCLIP;
			} else {
				client->ps.pm_type = PM_SPECTATOR;
			}
		} else {
			client->ps.pm_type = PM_FREEZE;
		}

		client->ps.speed = DEFAULT_MOVESPEED_SPECTATOR;
#if FEAT_SCREENSHOT_TOOLS
		if ( ent->s.number == 0 ) client->ps.speed = (int)( client->ps.speed / timeScaleFromMode[level.timeFreezeMode] );
#endif

		// set up for pmove
		memset (&pm, 0, sizeof(pm));
		pm.ps = &client->ps;
		pm.cmd = *ucmd;
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;	// spectators can fly through bodies
		pm.trace = trap_Trace;
		pm.pointcontents = trap_PointContents;

		// perform a pmove
		Pmove (&pm);
		// save results of pmove
		VectorCopy( client->ps.origin, ent->s.origin );

		G_TouchTriggers( ent );
		trap_UnlinkEntity( ent );
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;

#if !FEAT_SCREENSHOT_TOOLS
	// attack button cycles through spectators
	if ( ( client->buttons & BUTTON_ATTACK_PRI ) && ! ( client->oldbuttons & BUTTON_ATTACK_PRI ) ) {
		Cmd_FollowCycle_f( ent, 1 );
	}
#else
	if ( ( client->buttons & BUTTON_ATTACK_PRI ) && ! ( client->oldbuttons & BUTTON_ATTACK_PRI ) ) {
		Cmd_Stop_f( ent );
	}
	if ( ( client->buttons & BUTTON_USE_HOLDABLE ) && ! ( client->oldbuttons & BUTTON_USE_HOLDABLE ) ) {
		trap_SendServerCommand( client->ps.clientNum, "screenshot\n" );
	}
	if ( ent->s.number == 0 && level.time > client->respawnTime + 2000 ) {
		level.timeFreezeMode = ucmd->weapon;
		if ( level.timeFreezeMode < 0 ) level.timeFreezeMode = 0;
		if ( level.timeFreezeMode > 9 ) level.timeFreezeMode = 9;
		trap_Cvar_Set( "timescale", va( "%f", timeScaleFromMode[level.timeFreezeMode] ) );
	}
#endif
}



/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped
=================
*/
qboolean ClientInactivityTimer( gclient_t *client ) {
	if ( ! g_inactivity.integer ) {
		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked
		client->inactivityTime = level.time + 60 * 1000;
		client->inactivityWarning = qfalse;
	} else if ( client->pers.cmd.forwardmove ||
		client->pers.cmd.rightmove ||
		client->pers.cmd.upmove ||
		(client->pers.cmd.buttons & BUTTON_ATTACK_PRI) ||
		(client->pers.cmd.buttons & BUTTON_ATTACK_SEC)
	) {
		client->inactivityTime = level.time + g_inactivity.integer * 1000;
		client->inactivityWarning = qfalse;
	} else if ( !client->pers.localClient ) {
		if ( level.time > client->inactivityTime ) {
			trap_DropClient( client - level.clients, "Dropped due to inactivity" );
			return qfalse;
		}
		if ( level.time > client->inactivityTime - 10000 && !client->inactivityWarning ) {
			client->inactivityWarning = qtrue;
			trap_SendServerCommand( client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"" );
		}
	}
	return qtrue;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
void ClientTimerActions( gentity_t *ent, int msec ) {
	gclient_t	*client;

	client = ent->client;
	client->timeResidual += msec;

	while ( client->timeResidual >= 1000 ) {
		client->timeResidual -= 1000;

		// regenerate
        if (client->ps.powerups[PW_REGEN] || (g_gametype.integer == GT_KINGOFTHEHILL && client->ps.powerups[PW_KING])) {
            int diff = level.time - client->lasthurt_time;
            if (diff > 10000) {
                diff = 10000;
            }

            if (ent->health < MAX_HEALTH) {
				ent->health += diff / 1000;
                if (ent->health > MAX_HEALTH) {
                    ent->health = MAX_HEALTH;
                }

                G_AddEvent( ent, EV_POWERUP_REGEN, 0 );
			}
		} else {
			// count down health when over max
			if ( ent->health > MAX_HEALTH ) {
				ent->health--;
			}
		}
	}
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client ) {
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~(EF_FIRING_PRI | EF_FIRING_SEC);
    client->ps.eFlags &= ~EF_GRAPPLE;

	// the level will exit when everyone wants to or after timeouts

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = client->pers.cmd.buttons;
	if ( client->buttons & ( BUTTON_ATTACK_PRI | BUTTON_ATTACK_SEC | BUTTON_USE_HOLDABLE ) & ( client->oldbuttons ^ client->buttons ) ) {
		// this used to be an ^1 but once a player says ready, it should stick
		client->readyToExit = 1;
	}
}


void G_HoldableUpdateSelectedAfterChange( gentity_t *ent ) {
	playerState_t *ps = &ent->client->ps;
	int bits = ps->stats[STAT_HOLDABLE_BITS];
	int cur  = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;

	if ( cur != HI_NONE && ( bits & BG_HOLDABLE_BIT( cur ) ) && BG_HoldableIsSelectable( (holdable_t)cur ) )
		return;

	ps->stats[STAT_HOLDABLE_ITEM] = 0;
	for ( int hi = 1; hi < HI_NUM_HOLDABLE; hi++ ) {
		if ( ( bits & BG_HOLDABLE_BIT( hi ) ) && BG_HoldableIsSelectable( (holdable_t)hi ) ) {
			ps->stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( (holdable_t)hi ) - bg_itemlist;
			break;
		}
	}
}

void G_HoldableAdvanceSelected( gentity_t *ent, int dir ) {
	playerState_t *ps = &ent->client->ps;
	int bits  = ps->stats[STAT_HOLDABLE_BITS];
	int cur   = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	int start = ( cur == HI_NONE ) ? ( dir > 0 ? HI_NONE : HI_NUM_HOLDABLE ) : cur;
	int hi;

	for ( hi = start + dir; hi > HI_NONE && hi < HI_NUM_HOLDABLE; hi += dir ) {
		if ( ( bits & BG_HOLDABLE_BIT( hi ) ) && BG_HoldableIsSelectable( (holdable_t)hi ) ) {
			ps->stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( (holdable_t)hi ) - bg_itemlist;
			return;
		}
	}
	start = ( dir > 0 ) ? HI_NONE : HI_NUM_HOLDABLE;
	for ( hi = start + dir; hi > HI_NONE && hi < HI_NUM_HOLDABLE; hi += dir ) {
		if ( hi == cur ) break;
		if ( ( bits & BG_HOLDABLE_BIT( hi ) ) && BG_HoldableIsSelectable( (holdable_t)hi ) ) {
			ps->stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( (holdable_t)hi ) - bg_itemlist;
			return;
		}
	}
}

/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
void ClientEvents( gentity_t *ent, int oldEventSequence ) {
	int		i, j;
	int		event;
	gclient_t *client;
	int		damage;
	vec3_t	origin, angles;
//	qboolean	fired;
	gitem_t *item;
	gentity_t *drop;

	client = ent->client;

	if ( oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS ) {
		oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
	}
	for ( i = oldEventSequence ; i < client->ps.eventSequence ; i++ ) {
		event = client->ps.events[ i & (MAX_PS_EVENTS-1) ];

		switch ( event ) {
		case EV_FALL_MEDIUM:
		case EV_FALL_FAR:
			if ( ent->s.eType != ET_PLAYER ) {
				break;		// not in the player model
			}
			if ( event == EV_FALL_FAR ) {
				damage = 8;
			} else {
				damage = 2;
			}
			trap_WCE_EmitEvent( WCE_PLAYER_FALL,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    event == EV_FALL_FAR ? 2 : 1,
			                    0, 0.0f, NULL );
			ent->pain_debounce_time = level.time + 200;	// no normal pain sound
            G_Damage(ent, NULL, NULL, NULL, NULL, damage, DAMAGE_NO_ARMOR, MOD_FALLING);
			break;

		case EV_FIRE_WEAPON_PRI:
			FireWeapon( ent, 0 );
			trap_WCE_EmitEvent( WCE_PLAYER_ATTACK_PRIMARY,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    ent->client->ps.weapon,
			                    0, 0.0f, NULL );
			break;

		case EV_FIRE_WEAPON_SEC:
			FireWeapon( ent, 1 );
			trap_WCE_EmitEvent( WCE_PLAYER_ATTACK_SECONDARY,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    ent->client->ps.weapon,
			                    0, 0.0f, NULL );
			break;

		case EV_USE_ITEM1:		// teleporter
			// drop flags in CTF
			item = NULL;
			j = 0;

			if ( ent->client->ps.powerups[ PW_REDFLAG ] ) {
				item = BG_FindItemForPowerup( PW_REDFLAG );
				j = PW_REDFLAG;
			} else if ( ent->client->ps.powerups[ PW_BLUEFLAG ] ) {
				item = BG_FindItemForPowerup( PW_BLUEFLAG );
				j = PW_BLUEFLAG;
			} else if ( ent->client->ps.powerups[ PW_NEUTRALFLAG ] ) {
				item = BG_FindItemForPowerup( PW_NEUTRALFLAG );
				j = PW_NEUTRALFLAG;
			}

			if ( item ) {
				drop = Drop_Item( ent, item, 0 );
				// decide how many seconds it has left
				drop->count = ( ent->client->ps.powerups[ j ] - level.time ) / 1000;
				if ( drop->count < 1 ) {
					drop->count = 1;
				}

				ent->client->ps.powerups[ j ] = 0;
			}

#if FEAT_HARVESTER
			if ( g_gametype.integer == GT_HARVESTER ) {
				if ( ent->client->ps.generic1 > 0 ) {
					if ( ent->client->sess.sessionTeam == TEAM_RED ) {
						item = BG_FindItem( "Blue Cube" );
					} else {
						item = BG_FindItem( "Red Cube" );
					}
					if ( item ) {
						for ( j = 0; j < ent->client->ps.generic1; j++ ) {
							drop = Drop_Item( ent, item, 0 );
							if ( ent->client->sess.sessionTeam == TEAM_RED ) {
								drop->spawnflags = TEAM_BLUE;
							} else {
								drop->spawnflags = TEAM_RED;
							}
						}
					}
					ent->client->ps.generic1 = 0;
				}
			}
#endif
			SelectSpawnPoint( ent->client->ps.origin, origin, angles, qfalse );
			TeleportPlayer( ent, origin, angles, 400 );
			G_HoldableUpdateSelectedAfterChange( ent );
			break;

		case EV_USE_ITEM2:		// medkit
			ent->health = MAX_HEALTH;
			G_HoldableUpdateSelectedAfterChange( ent );
			break;

		case EV_USE_ITEM3:		// kamikaze
			// make sure the deflector is off
			ent->client->deflectorTime = 0;
			ent->client->ps.powerups[PW_DEFLECTOR] = 0;

			// start the kamikze
			G_StartKamikaze( ent );
			G_HoldableUpdateSelectedAfterChange( ent );
			break;

#if FEAT_PW_PORTAL
		case EV_USE_ITEM4:		// portal
			if( ent->client->portalID ) {
				Q3_DropPortalSource( ent );
			}
			else {
				Q3_DropPortalDestination( ent );
			}
			G_HoldableUpdateSelectedAfterChange( ent );
			break;
#endif

		case EV_USE_ITEM5:		// deflector
			ent->client->deflectorTime = level.time + 10000;
			ent->client->ps.powerups[PW_DEFLECTOR] = ent->client->deflectorTime;
			G_HoldableUpdateSelectedAfterChange( ent );
			break;

		case EV_FOOTSTEP:
			trap_WCE_EmitEvent( WCE_PLAYER_FOOTSTEP,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_FOOTSTEP_METAL:
			trap_WCE_EmitEvent( WCE_PLAYER_FOOTSTEP,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    1, 0, 0.0f, NULL );
			break;
		case EV_FALL_SHORT:
			trap_WCE_EmitEvent( WCE_PLAYER_FALL,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_SWIM:
			trap_WCE_EmitEvent( WCE_PLAYER_SWIM,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_JUMP:
			trap_WCE_EmitEvent( WCE_PLAYER_JUMP,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_WATER_TOUCH:
			trap_WCE_EmitEvent( WCE_PLAYER_WATER_ENTER,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_WATER_LEAVE:
			trap_WCE_EmitEvent( WCE_PLAYER_WATER_EXIT,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_WATER_UNDER:
			trap_WCE_EmitEvent( WCE_PLAYER_WATER_SUBMERGE,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_WATER_CLEAR:
			trap_WCE_EmitEvent( WCE_PLAYER_WATER_EXIT,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;
		case EV_TAUNT:
		case EV_TAUNT_YES:
		case EV_TAUNT_NO:
		case EV_TAUNT_FOLLOWME:
		case EV_TAUNT_GETFLAG:
		case EV_TAUNT_GUARDBASE:
		case EV_TAUNT_PATROL:
			trap_WCE_EmitEvent( WCE_PLAYER_TAUNT,
			                    ent->s.number, ent->s.number,
			                    ent->client->ps.origin,
			                    0, 0, 0.0f, NULL );
			break;

		default:
			break;
		}
	}

}

/*
==============
StuckInOtherClient
==============
*/
static int StuckInOtherClient(gentity_t *ent) {
	gentity_t	*ent2;

	ent2 = &g_entities[0];
	for ( int i = 0; i < MAX_CLIENTS; i++, ent2++ ) {
		if ( ent2 == ent ) {
			continue;
		}
		if ( !ent2->inuse ) {
			continue;
		}
		if ( !ent2->client ) {
			continue;
		}
		if ( ent2->health <= 0 ) {
			continue;
		}
		//
		if (ent2->r.absmin[0] > ent->r.absmax[0])
			continue;
		if (ent2->r.absmin[1] > ent->r.absmax[1])
			continue;
		if (ent2->r.absmin[2] > ent->r.absmax[2])
			continue;
		if (ent2->r.absmax[0] < ent->r.absmin[0])
			continue;
		if (ent2->r.absmax[1] < ent->r.absmin[1])
			continue;
		if (ent2->r.absmax[2] < ent->r.absmin[2])
			continue;
		return qtrue;
	}
	return qfalse;
}

void BotTestSolid(vec3_t origin);

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps ) {
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	// if there are still events pending
	if ( ps->entityEventSequence < ps->eventSequence ) {
		// create a temporary entity for this event which is sent to everyone
		// except the client who generated the event
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		// set external event to zero before calling BG_PlayerStateToEntityState
		extEvent = ps->externalEvent;
		ps->externalEvent = 0;
		// create temporary entity for event
		t = G_TempEntity( ps->origin, event );
		number = t->s.number;
		BG_PlayerStateToEntityState( ps, &t->s, qtrue );
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		// send to everyone except the client who generated the event
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		// set back external event
		ps->externalEvent = extEvent;
	}
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void ClientThink_real( gentity_t *ent ) {
	gclient_t	*client;
	pmove_t		pm;
	int			oldEventSequence;
	int			msec;
	usercmd_t	*ucmd;

	client = ent->client;

	// don't think if the client is not yet connected (and thus not yet spawned in)
	if (client->pers.connected != CON_CONNECTED) {
		return;
	}
	// mark the time, so the connection sprite can be removed
	ucmd = &ent->client->pers.cmd;

	// sanity check the command time to prevent speedup cheating
	if ( ucmd->serverTime > level.time + 200 ) {
		ucmd->serverTime = level.time + 200;
//		Com_Log( SEV_INFO, LOG_CH(ch_game), "serverTime <<<<<\n" );
	}
	if ( ucmd->serverTime < level.time - 1000 ) {
		ucmd->serverTime = level.time - 1000;
//		Com_Log( SEV_INFO, LOG_CH(ch_game), "serverTime >>>>>\n" );
	}

	msec = ucmd->serverTime - client->ps.commandTime;
	// following others may result in bad times, but we still want
	// to check for follow toggles
	if ( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		return;
	}
	if ( msec > 200 ) {
		msec = 200;
	}

	if ( pmove_msec.integer < 8 ) {
		trap_Cvar_Set("pmove_msec", "8");
		trap_Cvar_Update(&pmove_msec);
	}
	else if (pmove_msec.integer > 33) {
		trap_Cvar_Set("pmove_msec", "33");
		trap_Cvar_Update(&pmove_msec);
	}

	if ( pmove_fixed.integer || client->pers.pmoveFixed ) {
		ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer-1) / pmove_msec.integer) * pmove_msec.integer;
		//if (ucmd->serverTime - client->ps.commandTime <= 0)
		//	return;
	}

	//
	// check for exiting intermission
	//
	if ( level.intermissiontime ) {
		ClientIntermissionThink( client );
		return;
	}
#if FEAT_GAME_MEETING
	if ( level.meeting && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		ClientIntermissionThink( client );
		return;
	}
#endif

	// spectators don't do much
	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			return;
		}
		SpectatorThink( ent, ucmd );
		return;
	}

	// check for inactivity timer, but never drop the local client of a non-dedicated server
	if ( !ClientInactivityTimer( client ) ) {
		return;
	}

	// clear the rewards if time
	if ( level.time > client->rewardTime ) {
		client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
	}

	if ( client->noclip ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		client->ps.pm_type = PM_DEAD;
	} else {
		client->ps.pm_type = PM_NORMAL;
	}
#if FEAT_GAME_MEETING
	if ( level.meeting && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		client->ps.pm_type = PM_MEETING;
	}
#endif

	client->ps.gravity = g_envGravity.value;

	// set speed
	if (g_excessive.integer) {
		client->ps.speed = DEFAULT_MOVESPEED_PLAYER * 2;
	}
	else {
		client->ps.speed = DEFAULT_MOVESPEED_PLAYER;
	}

	// eser - weapon weights
	if ( client->ps.weapon > WP_NONE && client->ps.weapon < WP_NUM_WEAPONS ) {
		client->ps.speed *= bg_weaponlist[client->ps.weapon].weight;
	}
	// eser - weapon weights

	if ( client->ps.powerups[PW_HASTE] ) {
		client->ps.speed *= 1.3;
	}

	// set up for pmove
	oldEventSequence = client->ps.eventSequence;

	memset (&pm, 0, sizeof(pm));

	// check for the hit-scan gauntlet, don't let the action
	// go through as an attack unless it actually hits something
	if ( client->ps.weapon == WP_GAUNTLET && !( ucmd->buttons & BUTTON_TALK ) &&
		( ucmd->buttons & BUTTON_ATTACK_PRI || ucmd->buttons & BUTTON_ATTACK_SEC ) && client->ps.weaponTime <= 0 ) {
		pm.gauntletHit = CheckGauntletAttack( ent );
	}

	if ( ent->flags & FL_FORCE_GESTURE ) {
		ent->flags &= ~FL_FORCE_GESTURE;
		ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
	}

	// check for deflector expansion before doing the Pmove
	if ( client->deflectorTime && !(client->ps.pm_flags & PMF_DEFLECTOR_EXPAND) ) {
		vec3_t mins = { -INVUL_RADIUS, -INVUL_RADIUS, -INVUL_RADIUS };
		vec3_t maxs = { INVUL_RADIUS, INVUL_RADIUS, INVUL_RADIUS };
		vec3_t oldmins, oldmaxs;

		VectorCopy (ent->r.mins, oldmins);
		VectorCopy (ent->r.maxs, oldmaxs);
		// expand
		VectorCopy (mins, ent->r.mins);
		VectorCopy (maxs, ent->r.maxs);
		trap_LinkEntity(ent);
		// check if this would get anyone stuck in this player
		if ( !StuckInOtherClient(ent) ) {
			// set flag so the expanded size will be set in PM_CheckDuck
			client->ps.pm_flags |= PMF_DEFLECTOR_EXPAND;
		}
		// set back
		VectorCopy (oldmins, ent->r.mins);
		VectorCopy (oldmaxs, ent->r.maxs);
		trap_LinkEntity(ent);
	}

	pm.ps = &client->ps;
	pm.cmd = *ucmd;
	if ( pm.ps->pm_type == PM_DEAD ) {
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	}
	else if ( ent->r.svFlags & SVF_BOT ) {
		pm.tracemask = MASK_PLAYERSOLID | CONTENTS_BOTCLIP;
	}
	else {
		pm.tracemask = MASK_PLAYERSOLID;
	}
	pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = g_debugMove.integer;
	pm.stepDebugLevel = pm_step_debug.integer;
	pm.noFootsteps = g_noFootsteps.integer;

	pm.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
	pm.pmove_msec = pmove_msec.integer;
	// build pmove_flags bitmask from feature cvars
	pm.pmove_flags = 0;


#if FEAT_FAST_WEAPON_SWITCH
	// fast weapon switch (5A): map cvar value to 2-bit pmove_flags
	if ( g_fastWeaponSwitch.integer == 2 )      pm.pmove_flags |= PMF_FAST_SWITCH_INSTANT;
	else if ( g_fastWeaponSwitch.integer == 1 )  pm.pmove_flags |= PMF_FAST_SWITCH_SKIP_DROP;
#endif

	VectorCopy( client->ps.origin, client->oldOrigin );

	if (level.intermissionQueued != 0 && (g_gameflags.integer & GF_CAMPAIGN)) {
		if ( level.time - level.intermissionQueued >= 1000  ) {
			pm.cmd.buttons = 0;
			pm.cmd.forwardmove = 0;
			pm.cmd.rightmove = 0;
			pm.cmd.upmove = 0;
			if ( level.time - level.intermissionQueued >= 2000 && level.time - level.intermissionQueued <= 2500 ) {
				trap_SendConsoleCommand( EXEC_APPEND, "centerview\n");
			}
			ent->client->ps.pm_type = PM_INTERMISSION;
		}
	}
	Pmove (&pm);

	// save results of pmove
	if ( ent->client->ps.eventSequence != oldEventSequence ) {
		ent->eventTime = level.time;
	}
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue );
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
	}
	SendPendingPredictableEvents( &ent->client->ps );

    // eser - offhand grapple
    if (g_grapple.integer) {
        if ((pm.cmd.buttons & BUTTON_AFFIRMATIVE) && ent->client->ps.pm_type != PM_DEAD && !ent->client->hookhasbeenfired) {
            Offhand_Grapple_Fire(ent);
        }
        if (!(pm.cmd.buttons & BUTTON_AFFIRMATIVE) && ent->client->ps.pm_type != PM_DEAD && ent->client->hookhasbeenfired && ent->client->fireHeld) {
            ent->client->fireHeld = qfalse;
            ent->client->hookhasbeenfired = qfalse;
        }
        if (client->hook && client->fireHeld == qfalse) {
            Offhand_Grapple_Free(client->hook);
        }
    }
    // offhand grapple

	// use the snapped origin for linking so it matches client predicted versions
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

	VectorCopy (pm.mins, ent->r.mins);
	VectorCopy (pm.maxs, ent->r.maxs);

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;

	// execute client events
	ClientEvents( ent, oldEventSequence );

	// link entity now, after any teleporters have been used
	trap_LinkEntity (ent);
	if ( !ent->client->noclip ) {
		G_TouchTriggers( ent );
	}

	// NOTE: now copy the exact origin over otherwise clients can be snapped into solid
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

	// unlagged: record position for lag compensation
#if FEAT_UNLAGGED
	ent->client->frameOffset = trap_Milliseconds() - level.frameStartTime;
	ent->client->attackTime = ent->client->ps.commandTime;
#endif
	G_StoreHistory( ent );

	//test for solid areas in the AAS file
	BotTestAAS(ent->r.currentOrigin);

	// touch other objects
	ClientImpacts( ent, &pm );

	// save results of triggers and client events
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// check for respawning
	if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
#if FEAT_ELIMINATION
		// elimination (10B): don't allow respawn during a live round
		if ( g_elimination.integer && level.roundState == ROUND_LIVE ) {
			return;
		}
#endif
		if (g_gametype.integer == GT_LASTMANSTANDING && client->ps.persistant[PERS_SCORE] < 1) {
			SetTeam(ent, "s");
			return;
		}
		// wait for the attack button to be pressed
		if ( level.time > client->respawnTime ) {
			// forcerespawn is to prevent users from waiting out powerups
			if ( g_forceRespawn.integer > 0 && ( level.time - client->respawnTime ) > g_forceRespawn.integer * 1000 ) {
				ClientRespawn( ent );
				return;
			}

			// pressing attack or use is the normal respawn method
			if ( ucmd->buttons & ( BUTTON_ATTACK_PRI | BUTTON_ATTACK_SEC | BUTTON_USE_HOLDABLE ) ) {
				ClientRespawn( ent );
			}
		}
		return;
	}

	// camp detection (11C): gradual dark screen punishment
	if ( g_campDetectionTime.integer > 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		float dist;
		vec3_t delta;
		VectorSubtract( ent->r.currentOrigin, client->campOrigin, delta );
		dist = VectorLength( delta );
		if ( dist > g_campDetectionRadius.value ) {
			// moved far enough — clear camper status and reset timer
			VectorCopy( ent->r.currentOrigin, client->campOrigin );
			client->campTime = level.time;
			client->ps.stats[STAT_CAMPER] = 0;
		} else if ( level.time - client->campTime > g_campDetectionTime.integer * 1000 ) {
			// camping too long — ramp darkness every second
			if ( ent->health > 0 ) {
				client->campTime = level.time - ( g_campDetectionTime.integer * 1000 ) + 1000;
				if ( client->ps.stats[STAT_CAMPER] < 255 ) {
					client->ps.stats[STAT_CAMPER] += 50;
					if ( client->ps.stats[STAT_CAMPER] > 255 ) {
						client->ps.stats[STAT_CAMPER] = 255;
					}
				}
			}
		}
	}

	// perform once-a-second actions
	ClientTimerActions( ent, msec );
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink( int clientNum ) {
	gentity_t *ent;

	ent = g_entities + clientNum;
	trap_GetUsercmd( clientNum, &ent->client->pers.cmd );

	// mark the time we got info, so we can display the
	// phone jack if they don't get any for a while
	ent->client->lastCmdTime = level.time;

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		ClientThink_real( ent );
	}
}


void G_RunClient( gentity_t *ent ) {
	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		return;
	}
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink_real( ent );
}


/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent ) {
	gclient_t	*cl;

	// if we are doing a chase cam or a remote view, grab the latest info
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		int		clientNum, flags;

		clientNum = ent->client->sess.spectatorClient;

		// team follow1 and team follow2 go to whatever clients are playing
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}
		if ( clientNum >= 0 ) {
			cl = &level.clients[ clientNum ];
			if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR ) {
				flags = (cl->ps.eFlags & ~(EF_VOTED | EF_TEAMVOTED)) | (ent->client->ps.eFlags & (EF_VOTED | EF_TEAMVOTED));
				ent->client->ps = cl->ps;
				ent->client->ps.pm_flags |= PMF_FOLLOW;
				ent->client->ps.eFlags = flags;
#if FEAT_MOVEMENT_KEYS
				{
					int keys = 0;
					usercmd_t *cmd = &cl->pers.cmd;
					if ( cmd->forwardmove > 0 )            keys |= KEYS_FORWARD;
					if ( cmd->forwardmove < 0 )            keys |= KEYS_BACK;
					if ( cmd->rightmove < 0 )              keys |= KEYS_LEFT;
					if ( cmd->rightmove > 0 )              keys |= KEYS_RIGHT;
					if ( cmd->upmove > 0 )                 keys |= KEYS_JUMP;
					if ( cmd->upmove < 0 )                 keys |= KEYS_CROUCH;
					if ( cmd->buttons & BUTTON_ATTACK_PRI )    keys |= KEYS_ATTACK_PRI;
					if ( cmd->buttons & BUTTON_ATTACK_SEC )    keys |= KEYS_ATTACK_SEC;
					if ( cmd->buttons & BUTTON_USE_HOLDABLE )  keys |= KEYS_USE;
					if ( cmd->buttons & BUTTON_WALKING )   keys |= KEYS_WALK;
					if ( cmd->buttons & BUTTON_GESTURE )   keys |= KEYS_GESTURE;
					ent->client->ps.stats[STAT_KEYS] = keys;
				}
#endif
				return;
			}
		}

		if ( ent->client->ps.pm_flags & PMF_FOLLOW ) {
			// drop them to free spectators unless they are dedicated camera followers
			if ( ent->client->sess.spectatorClient >= 0 ) {
				ent->client->sess.spectatorState = SPECTATOR_FREE;
			}

			ClientBegin( ent->client - level.clients );
		}
	}

	if ( ent->client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		ent->client->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		ent->client->ps.pm_flags &= ~PMF_SCOREBOARD;
	}
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame( gentity_t *ent ) {
	int			i;

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		SpectatorClientEndFrame( ent );
		return;
	}

	// turn off any expired powerups
    for (i = 0; i < PW_REDFLAG; i++) {
		if ( ent->client->ps.powerups[ i ] < level.time ) {
			ent->client->ps.powerups[ i ] = 0;
		}
	}

#if FEAT_SPAWN_PROTECTION
	// expire spawn protection by timer
	if ( ent->client->spawnprotected &&
		 level.time > ent->client->respawnTime + ( g_spawnProtect.integer * 1000 ) ) {
		ent->client->spawnprotected = qfalse;
		ent->client->ps.eFlags &= ~EF_SPAWN_PROTECT;
	}
#endif

#if FEAT_FREEZETAG
	// freezetag thaw (7A): nearby teammates thaw frozen players
	if ( g_freeze.integer && ent->client->ps.stats[STAT_FROZENSTATE] == FROZENSTATE_FROZEN ) {
		int j;
		qboolean teammateNearby = qfalse;
		for ( j = 0; j < level.maxclients; j++ ) {
			gentity_t *other = &g_entities[j];
			if ( !other->client || other == ent ) continue;
			if ( other->client->pers.connected != CON_CONNECTED ) continue;
			if ( other->client->sess.sessionTeam != ent->client->sess.sessionTeam ) continue;
			if ( other->client->ps.stats[STAT_FROZENSTATE] != FROZENSTATE_NORMAL ) continue;
			if ( other->health <= 0 ) continue;
			if ( Distance( ent->r.currentOrigin, other->r.currentOrigin ) <= g_thawRadius.integer ) {
				teammateNearby = qtrue;
				break;
			}
		}
		if ( teammateNearby ) {
			ent->client->thawProgress += level.time - level.previousTime;
			ent->client->ps.stats[STAT_FROZENSTATE] = FROZENSTATE_THAWING;
			if ( ent->client->thawProgress >= g_thawTime.integer ) {
				// thaw complete — respawn in place
				ent->client->ps.pm_type = PM_NORMAL;
				ent->client->ps.stats[STAT_FROZENSTATE] = FROZENSTATE_NORMAL;
				ent->client->ps.eFlags &= ~EF_FROZEN;
				ent->client->ps.stats[STAT_HEALTH] = ent->health = MAX_HEALTH;
				ent->takedamage = qtrue;
				ent->client->thawProgress = 0;
				trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " was thawed!\n\"",
					ent->client->pers.netname ) );
			}
		} else {
			ent->client->thawProgress = 0;
			ent->client->ps.stats[STAT_FROZENSTATE] = FROZENSTATE_FROZEN;
		}
	}
#endif

	// save network bandwidth
#if 0
	if ( !g_synchronousClients->integer && ent->client->ps.pm_type == PM_NORMAL ) {
		// FIXME: this must change eventually for non-sync demo recording
		VectorClear( ent->client->ps.viewangles );
	}
#endif

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if ( level.intermissiontime ) {
		return;
	}

	// burn from lava, etc
	P_WorldEffects (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	if ( ent->flags & FL_CLOAK ) {
		ent->client->ps.eFlags |= EF_CLOAK;
	} else {
		ent->client->ps.eFlags &= ~EF_CLOAK;
	}

	// add the EF_CONNECTION flag if we haven't gotten commands recently
	if ( level.time - ent->client->lastCmdTime > 1000 ) {
		ent->client->ps.eFlags |= EF_CONNECTION;
	} else {
		ent->client->ps.eFlags &= ~EF_CONNECTION;
	}

	ent->client->ps.stats[STAT_HEALTH] = ent->health;	// FIXME: get rid of ent->health...

	G_SetClientSound (ent);

	// set the latest infor
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue );
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
	}
	SendPendingPredictableEvents( &ent->client->ps );

	// sync aggregate stats to persistant slots for observer/web UI (wn_http.c reads these)
	{
		gclient_t *cl = ent->client;
		int totalDamage = 0, att;
		for ( att = ATT_NONE + 1; att < ATT_NUM_ATTACKS; att++ )
			totalDamage += cl->attackStats[att].damage;
		cl->ps.persistant[PERS_TOTAL_SHOTS]  = cl->accuracy_shots;
		cl->ps.persistant[PERS_TOTAL_HITS]   = cl->accuracy_hits;
		cl->ps.persistant[PERS_TOTAL_DAMAGE] = totalDamage;
	}

	// set the bit for the reachability area the client is currently in
//	i = trap_AAS_PointReachabilityAreaIndex( ent->client->ps.origin );
//	ent->client->areabits[i >> 3] |= 1 << (i & 7);
}
