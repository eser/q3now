// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_combat.c

#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

/*
============
ScorePlum
============
*/
void ScorePlum( gentity_t *ent, vec3_t origin, int score ) {
	gentity_t *plum;

	plum = G_TempEntity( origin, EV_SCOREPLUM );
	// only send this temp entity to a single client
	plum->r.svFlags |= SVF_SINGLECLIENT;
	plum->r.singleClient = ent->s.number;
	//
	plum->s.otherEntityNum = ent->s.number;
	plum->s.time = score;
}

#if FEAT_DAMAGE_PLUMS
/*
============
DamagePlum
Sends a floating damage number to the attacker's client.
============
*/
void DamagePlum( gentity_t *attacker, gentity_t *target, int damage ) {
	gentity_t *plum;
	vec3_t    origin;

	if ( !attacker->client ) {
		return;
	}

	VectorCopy( target->r.currentOrigin, origin );
	origin[2] += 2 * target->r.maxs[2];

	plum = G_TempEntity( origin, EV_DAMAGEPLUM );
	plum->r.svFlags |= SVF_SINGLECLIENT;
	plum->r.singleClient = attacker->s.number;
	plum->s.otherEntityNum = attacker->s.number;
	plum->s.time = damage;
}
#endif

/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore( gentity_t *ent, vec3_t origin, int score ) {
	if ( !ent->client ) {
		return;
	}
	// no scoring during pre-match warmup
	if ( level.warmupTime ) {
		return;
	}
#if FEAT_GAME_MEETING
	if ( level.meeting ) {
		return;
	}
#endif
	// show score plum
	ScorePlum(ent, origin, score);
	//
	ent->client->ps.persistant[PERS_SCORE] += score;
	if ( g_gametype.integer == GT_TDM )
		level.teamScores[ ent->client->ps.persistant[PERS_TEAM] ] += score;
	CalculateRanks();
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
void TossClientItems( gentity_t *self ) {
	gitem_t		*item;
	int			weapon;
	float		angle;
	int			i;
	gentity_t	*drop;

	// drop the weapon if not a gauntlet or machinegun
	weapon = self->s.weapon;

	// make a special check to see if they are changing to a new
	// weapon that isn't the mg or gauntlet.  Without this, a client
	// can pick up a weapon, be killed, and not drop the weapon because
	// their weapon change hasn't completed yet and they are still holding the MG.
	if ( weapon == WP_MACHINEGUN ) {
		if ( self->client->ps.weaponstate == WEAPON_DROPPING ) {
			weapon = self->client->pers.cmd.weapon;
		}
		if ( !( self->client->ps.stats[STAT_WEAPONS] & ( 1 << weapon ) ) ) {
			weapon = WP_NONE;
		}
	}

    // if ( weapon > WP_MACHINEGUN &&
    // 	self->client->ps.ammo[ weapon ] ) {
    // 	// find the item type for this weapon
    // 	item = BG_FindItemForWeapon( weapon );

    // 	// spawn the item
    // 	Drop_Item( self, item, 0 );
    // }

    // CPM
    if (g_gametype.integer != GT_KINGOFTHEHILL && weapon >= WP_MACHINEGUN &&
        (self->client->ps.ammo[weapon] || pm_backpacks)) {
        // find the item type for this weapon
        item = BG_FindItemForWeapon(weapon);

        // spawn the item
        drop = Drop_Item(self, item, 0); // CPM

        if (pm_backpacks)
        {
            // set the backpack flag
            drop->s.eFlags |= EF_BACKPACK;

            // fill with information
            drop->damage =
                ((self->client->ps.ammo[WP_MACHINEGUN] & 0x00FF) << 8) +
                (self->client->ps.ammo[WP_SHOTGUN] & 0x00FF);
            drop->health =
                ((self->client->ps.ammo[WP_GRENADE_LAUNCHER] & 0x00FF) << 8) +
                (self->client->ps.ammo[WP_ROCKET_LAUNCHER] & 0x00FF);
            {
                int lgAmmo = self->client->ps.ammo[WP_LIGHTNING_GUN];
                /* Q1 rerelease backpack ammo floor: lightning gun minimum 15 cells */
                if ( lgAmmo > 0 && lgAmmo < 15 ) lgAmmo = 15;
                drop->splashDamage =
                    ((lgAmmo & 0x00FF) << 8) +
                    (self->client->ps.ammo[WP_RAILGUN] & 0x00FF);
            }
            drop->splashRadius =
                ((self->client->ps.ammo[WP_PLASMA_RIFLE] & 0x00FF) << 8);
        }
        else {
            drop->count = self->client->ps.ammo[weapon];
        }
    }

	// drop all the powerups if not in teamplay
	if ( g_gametype.integer != GT_TDM ) {
		angle = 45;
		for ( i = 1 ; i < PW_NUM_POWERUPS ; i++ ) {
			if ( self->client->ps.powerups[ i ] > level.time ) {
				item = BG_FindItemForPowerup( i );
				if ( !item ) {
					continue;
				}
				drop = Drop_Item( self, item, angle );
				// decide how many seconds it has left
				drop->count = ( self->client->ps.powerups[ i ] - level.time ) / 1000;
				if ( drop->count < 1 ) {
					drop->count = 1;
				}
				angle += 45;
			}
		}
	}
}

#if FEAT_HARVESTER

/*
=================
TossClientCubes
=================
*/
extern gentity_t	*neutralObelisk;

void TossClientCubes( gentity_t *self ) {
	gitem_t		*item;
	gentity_t	*drop;
	vec3_t		velocity;
	vec3_t		angles;
	vec3_t		origin;

	self->client->ps.generic1 = 0;

	// this should never happen but we should never
	// get the server to crash due to skull being spawned in
	if (!G_EntitiesFree()) {
		return;
	}

	if( self->client->sess.sessionTeam == TEAM_RED ) {
		item = BG_FindItem( "Red Cube" );
	}
	else {
		item = BG_FindItem( "Blue Cube" );
	}

	angles[YAW] = (float)(level.time % 360);
	angles[PITCH] = 0;	// always forward
	angles[ROLL] = 0;

	AngleVectors( angles, velocity, NULL, NULL );
	VectorScale( velocity, 150, velocity );
	velocity[2] += 200 + crandom() * 50;

	if( neutralObelisk ) {
		VectorCopy( neutralObelisk->s.pos.trBase, origin );
		origin[2] += 44;
	} else {
		VectorClear( origin ) ;
	}

	drop = LaunchItem( item, origin, velocity );

	drop->nextthink = level.time + TA_CUBE_TIMEOUT * 1000;
	drop->think = G_FreeEntity;
	drop->spawnflags = self->client->sess.sessionTeam;
}
#endif


/*
==================
LookAtKiller
==================
*/
void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker ) {
	vec3_t		dir;

	if ( attacker && attacker != self ) {
		VectorSubtract (attacker->s.pos.trBase, self->s.pos.trBase, dir);
	} else if ( inflictor && inflictor != self ) {
		VectorSubtract (inflictor->s.pos.trBase, self->s.pos.trBase, dir);
	} else {
		self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
		return;
	}

	self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw ( dir );
}

/*
==================
IsGibMOD
==================
*/
qboolean IsGibMOD( int meansOfDeath ) {
    switch (meansOfDeath) {
        case MOD_UNKNOWN:
        case MOD_GRENADE:
        case MOD_ROCKET:
        case MOD_RAILGUN:
        case MOD_CRUSH:
        case MOD_TELEFRAG:
        case MOD_FALLING:
        case MOD_SUICIDE:
        case MOD_KAMIKAZE:
            return qtrue;
        default:
            return qfalse;
    }
}

/*
==================
IsInstantGibMOD
==================
*/
qboolean IsInstantGibMOD( int meansOfDeath ) {
    switch (meansOfDeath) {
        case MOD_RAILGUN:
        case MOD_CRUSH:
        case MOD_TELEFRAG:
        case MOD_FALLING:
        case MOD_SUICIDE:
        case MOD_KAMIKAZE:
            return qtrue;
        default:
            return qfalse;
    }
}

/*
==================
GibEntity
==================
*/
void GibEntity( gentity_t *self, int killer ) {
	gentity_t *ent;

	//if this entity still has kamikaze
	if (self->s.eFlags & EF_KAMIKAZE) {
		// check if there is a kamikaze timer around for this owner
		for (int i = 0; i < level.num_entities; i++) {
			ent = &g_entities[i];
			if (!ent->inuse)
				continue;
			if (ent->activator != self)
				continue;
			if ( strcmp( ent->classname, "kamikaze timer" ) != 0 )
				continue;
			G_FreeEntity(ent);
			break;
		}
	}
	G_AddEvent( self, EV_GIB_PLAYER, killer );
	self->takedamage = qfalse;
	self->s.eType = ET_INVISIBLE;
	self->r.contents = 0;
}

/*
==================
body_die
==================
*/
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	if ( self->health > GIB_HEALTH ) {
		return;
	}
	if ( !g_blood.integer || !IsGibMOD( meansOfDeath ) ) {
		self->health = GIB_HEALTH+1;
		return;
	}

	GibEntity( self, 0 );
}


// these are just for logging, the client prints its own messages
char	*modNames[] = {
	"MOD_UNKNOWN",
	"MOD_SHOTGUN",
	"MOD_GAUNTLET",
	"MOD_MACHINEGUN",
	"MOD_GRENADE",
	"MOD_GRENADE_SPLASH",
	"MOD_ROCKET",
	"MOD_ROCKET_SPLASH",
	"MOD_PLASMA",
	"MOD_RAILGUN",
	"MOD_LIGHTNING",
// eser - lightning discharge
    "MOD_LIGHTNING_DISCHARGE",
// eser - lightning discharge
	// alt-fire means of death
	"MOD_GAUNTLET_LUNGE",
	"MOD_MACHINEGUN_BURST",
	"MOD_SHOTGUN_DOUBLE_BLAST",
	"MOD_ROCKET_MORTAR",
	"MOD_ROCKET_MORTAR_SPLASH",
	"MOD_LIGHTNING_CHAIN_ARC",
	"MOD_WATER",
	"MOD_SLIME",
	"MOD_LAVA",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TARGET_LASER",
	"MOD_TRIGGER_HURT",
	"MOD_KAMIKAZE",
	"MOD_GRAPPLE",
	"MOD_LAVABALL",
	"MOD_NAIL"
};

/*
==================
Kamikaze_DeathActivate
==================
*/
void Kamikaze_DeathActivate( gentity_t *ent ) {
	G_StartKamikaze(ent);
	G_FreeEntity(ent);
}

/*
==================
Kamikaze_DeathTimer
==================
*/
void Kamikaze_DeathTimer( gentity_t *self ) {
	gentity_t *ent;

	ent = G_Spawn();
	ent->classname = "kamikaze timer";
	VectorCopy(self->s.pos.trBase, ent->s.pos.trBase);
	ent->r.svFlags |= SVF_NOCLIENT;
	ent->think = Kamikaze_DeathActivate;
	ent->nextthink = level.time + 5 * 1000;

	ent->activator = self;
}

/*
==================
CheckAlmostCapture
==================
*/
void CheckAlmostCapture( gentity_t *self, gentity_t *attacker ) {
	gentity_t	*ent;
	vec3_t		dir;
	char		*classname;

	// if this player was carrying a flag
	if ( self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG] ||
		self->client->ps.powerups[PW_NEUTRALFLAG] ) {
		// get the goal flag this player should have been going for
		if ( g_gametype.integer == GT_CTF ) {
			if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
				classname = "team_CTF_blueflag";
			}
			else {
				classname = "team_CTF_redflag";
			}
		}
		else {
			if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
				classname = "team_CTF_redflag";
			}
			else {
				classname = "team_CTF_blueflag";
			}
		}
		ent = NULL;
		do
		{
			ent = G_Find(ent, FOFS(classname), classname);
        } while (ent && (ent->s.eFlags & EF_DROPPED_ITEM));
		// if we found the destination flag and it's not picked up
		if (ent && !(ent->r.svFlags & SVF_NOCLIENT) ) {
			// if the player was *very* close
			VectorSubtract( self->client->ps.origin, ent->s.origin, dir );
			if ( VectorLength(dir) < 200 ) {
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				if ( attacker->client ) {
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
}

/*
==================
CheckAlmostScored
==================
*/
void CheckAlmostScored( gentity_t *self, gentity_t *attacker ) {
	gentity_t	*ent;
	vec3_t		dir;
	char		*classname;

	// if the player was carrying cubes
	if ( self->client->ps.generic1 ) {
		if ( self->client->sess.sessionTeam == TEAM_BLUE ) {
			classname = "team_redobelisk";
		}
		else {
			classname = "team_blueobelisk";
		}
		ent = G_Find(NULL, FOFS(classname), classname);
		// if we found the destination obelisk
		if ( ent ) {
			// if the player was *very* close
			VectorSubtract( self->client->ps.origin, ent->s.origin, dir );
			if ( VectorLength(dir) < 200 ) {
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				if ( attacker->client ) {
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
}

/*
==================
player_die
==================
*/
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	gentity_t	*ent;
	int			anim;
	int			contents;
	int			killer;
	int			i;
	char		*killerName, *obit;
	int			attIdx;

	if ( self->client->ps.pm_type == PM_DEAD ) {
		return;
	}

	if ( level.intermissiontime ) {
		return;
	}

	// check for an almost capture
	CheckAlmostCapture( self, attacker );
	// check for a player that almost brought in cubes
	CheckAlmostScored( self, attacker );

    if (self->client && self->client->hook) {
        Offhand_Grapple_Free(self->client->hook);
    }

#if FEAT_FREEZETAG
	// freezetag (7A): freeze instead of dying in team games
	if ( g_freeze.integer && g_gametype.integer >= GT_TDM ) {
		self->client->ps.pm_type = PM_FREEZE;
		self->client->ps.stats[STAT_FROZENSTATE] = FROZENSTATE_FROZEN;
		self->client->ps.eFlags |= EF_FROZEN;
		self->client->frozenTime = level.time;
		self->client->thawProgress = 0;
		self->takedamage = qfalse;
		G_AddEvent( self, EV_FREEZE, 0 );
		// don't actually die — skip the rest of player_die
		// award the kill to the attacker
		if ( attacker && attacker->client && attacker != self ) {
			attacker->client->ps.persistant[PERS_SCORE] += 1;
		}
		return;
	}
#endif
	self->client->ps.pm_type = PM_DEAD;

	if ( attacker ) {
		killer = attacker->s.number;
		if ( attacker->client ) {
			killerName = attacker->client->pers.netname;
		} else {
			killerName = "<non-client>";
		}
	} else {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ( killer < 0 || killer >= MAX_CLIENTS ) {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ( meansOfDeath < 0 || meansOfDeath >= ARRAY_LEN( modNames ) ) {
		obit = "<bad obituary>";
	} else {
		obit = modNames[meansOfDeath];
	}

	G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n",
		killer, self->s.number, meansOfDeath, killerName,
		self->client->pers.netname, obit );

#if FEAT_UNLAGGED
	G_UnTimeShiftClient( self );
#endif

#if FEAT_WIREDNET_OBSERVER
	// QUIC event: kill with positions for spatial-aware coaching
	{
		vec3_t att_pos = {0, 0, 0};
		if ( attacker && attacker->client )
			VectorCopy( attacker->client->ps.origin, att_pos );
		trap_WiredNet_EmitKill( killer, self->s.number, meansOfDeath, att_pos, self->client->ps.origin );
	}
#endif
	trap_WCE_EmitEvent( WCE_PLAYER_DEATH,
	                    self->s.number, self->s.number,
	                    self->client->ps.origin,
	                    meansOfDeath,
	                    killer,
	                    0.0f,
	                    obit );

	// broadcast the death event to everyone
	ent = G_TempEntity( self->r.currentOrigin, EV_OBITUARY );
	ent->s.eventParm = meansOfDeath;
	ent->s.otherEntityNum = self->s.number;
	ent->s.otherEntityNum2 = killer;
	ent->r.svFlags = SVF_BROADCAST;	// send to everyone

	self->enemy = attacker;

#if FEAT_TEAM_LEADERSHIP
	// PTL (11M): if the dead player was the leader (carrying team flag),
	// transfer the flag to the killer
	if ( g_ptl.integer && g_gametype.integer >= GT_TDM && attacker && attacker->client && attacker != self ) {
		if ( self->client->ps.powerups[PW_REDFLAG] ) {
			self->client->ps.powerups[PW_REDFLAG] = 0;
			attacker->client->ps.powerups[PW_REDFLAG] = INT_MAX;
			trap_SendServerCommand( -1, va( "cp \"" S_COLOR_RED "%s" S_COLOR_WHITE " is now the Red Leader!\"",
				attacker->client->pers.netname ) );
			AddScore( attacker, self->r.currentOrigin, 2 );
		}
		if ( self->client->ps.powerups[PW_BLUEFLAG] ) {
			self->client->ps.powerups[PW_BLUEFLAG] = 0;
			attacker->client->ps.powerups[PW_BLUEFLAG] = INT_MAX;
			trap_SendServerCommand( -1, va( "cp \"" S_COLOR_BLUE "%s" S_COLOR_WHITE " is now the Blue Leader!\"",
				attacker->client->pers.netname ) );
			AddScore( attacker, self->r.currentOrigin, 2 );
		}
	}
#endif

	self->client->ps.persistant[PERS_KILLED]++;

	// eser - kill streaks
	self->client->consecutiveKills = 0;
	// eser - kill streaks

	if (attacker && attacker->client) {
		attacker->client->lastkilled_client = self->s.number;

		if ( attacker == self || OnSameTeam (self, attacker ) ) {
            if (g_gametype.integer == GT_KINGOFTHEHILL && self->client->ps.powerups[PW_KING]) {
                AssignAKing(NULL);
            }

			AddScore( attacker, self->r.currentOrigin, -1 );
		} else {
#if FEAT_SPAWN_PROTECTION
			// spawn protection: no score for killing a protected player
			if ( self->client->spawnprotected ) {
				trap_SendServerCommand( attacker->s.number,
					"print \"No score — target was spawn protected.\n\"" );
			} else
#endif
            if (g_gametype.integer == GT_KINGOFTHEHILL && self->client->ps.powerups[PW_KING]) {
                AddScore(attacker, self->r.currentOrigin, 5);
                AssignAKing(attacker);
            }
            else if (g_gametype.integer == GT_LASTMANSTANDING) {
                AddScore(self, self->r.currentOrigin, -1);
            }
            else {
                AddScore(attacker, self->r.currentOrigin, 1);
            }

			if( meansOfDeath == MOD_GAUNTLET || meansOfDeath == MOD_GAUNTLET_LUNGE ) {

				// play humiliation on player
				attacker->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;

				// add the sprite over the player's head
				attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
				attacker->client->ps.eFlags |= EF_AWARD_GAUNTLET;
				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

				// also play humiliation on target
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
			}

			// check for two kills in a short amount of time
			// if this is close enough to the last kill, give a reward sound
			if ( level.time - attacker->client->lastKillTime < CARNAGE_REWARD_TIME ) {
				// play excellent on player
				attacker->client->ps.persistant[PERS_EXCELLENT_COUNT]++;

				// add the sprite over the player's head
				attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
				attacker->client->ps.eFlags |= EF_AWARD_EXCELLENT;
				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
			attacker->client->lastKillTime = level.time;

			// eser - kill streaks
			if ( attacker != self && !OnSameTeam( self, attacker ) ) {
				attacker->client->consecutiveKills++;
				switch ( attacker->client->consecutiveKills ) {
				case 5:
					attacker->client->ps.persistant[PERS_KILLING_SPREE_COUNT]++;
					trap_SendServerCommand( -1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is on a Killing Spree!\n\"", attacker->client->pers.netname ) );
					break;
				case 10:
					attacker->client->ps.persistant[PERS_RAMPAGE_COUNT]++;
					trap_SendServerCommand( -1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is on a Rampage!\n\"", attacker->client->pers.netname ) );
					break;
				case 15:
					attacker->client->ps.persistant[PERS_MASSACRE_COUNT]++;
					trap_SendServerCommand( -1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is on a Massacre!\n\"", attacker->client->pers.netname ) );
					break;
				case 20:
					attacker->client->ps.persistant[PERS_UNSTOPPABLE_COUNT]++;
					trap_SendServerCommand( -1, va( "cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is Unstoppable!\n\"", attacker->client->pers.netname ) );
					break;
				default:
					break;
				}
			}
			// eser - kill streaks

		}
	} else {
        if (g_gametype.integer == GT_KINGOFTHEHILL && self->client->ps.powerups[PW_KING]) {
            AssignAKing(NULL);
        }

		AddScore( self, self->r.currentOrigin, -1 );
	}

	// per-attack kill/death tracking
	attIdx = G_AttackFromMOD( meansOfDeath );
	if ( attIdx > ATT_NONE && attIdx < ATT_NUM_ATTACKS ) {
		if ( attacker && attacker->client && attacker != self ) {
			attacker->client->attackStats[attIdx].kills++;
		}
		self->client->attackStats[attIdx].deaths++;
	}

	// Add team bonuses
	Team_FragBonuses(self, inflictor, attacker);

	// if I committed suicide, the flag does not fall, it returns.
	if (meansOfDeath == MOD_SUICIDE) {
		if ( self->client->ps.powerups[PW_NEUTRALFLAG] ) {		// only happens in One Flag CTF
			Team_ReturnFlag( TEAM_FREE );
			self->client->ps.powerups[PW_NEUTRALFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_REDFLAG] ) {		// only happens in standard CTF
			Team_ReturnFlag( TEAM_RED );
			self->client->ps.powerups[PW_REDFLAG] = 0;
		}
		else if ( self->client->ps.powerups[PW_BLUEFLAG] ) {	// only happens in standard CTF
			Team_ReturnFlag( TEAM_BLUE );
			self->client->ps.powerups[PW_BLUEFLAG] = 0;
		}
	}

	TossClientItems( self );
#if FEAT_HARVESTER
	if( g_gametype.integer == GT_HARVESTER ) {
		TossClientCubes( self );
	}
#endif

	Cmd_Score_f( self );		// show scores
	// send updated scores to any clients that are following this one,
	// or they would get stale scoreboards
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t	*client;

		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if ( client->sess.spectatorClient == self->s.number ) {
			Cmd_Score_f( g_entities + i );
		}
	}

	self->takedamage = qtrue;	// can still be gibbed

	self->s.weapon = WP_NONE;
	self->s.powerups = 0;
	self->r.contents = CONTENTS_CORPSE;

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;
	LookAtKiller (self, inflictor, attacker);

	VectorCopy( self->s.angles, self->client->ps.viewangles );

	self->s.loopSound = 0;

	self->r.maxs[2] = DEAD_MAXS_Z;

	// don't allow respawn until the death anim is done
	// g_forcerespawn may force spawning at some later time
    self->client->respawnTime = level.time + pm_clientrespawndelay;

	// remove powerups
	memset( self->client->ps.powerups, 0, sizeof(self->client->ps.powerups) );

	// never gib in a nodrop
	contents = trap_PointContents( self->r.currentOrigin, -1 );

	if ( (self->health <= GIB_HEALTH && !(contents & CONTENTS_NODROP) && g_blood.integer && IsGibMOD( meansOfDeath )) || IsInstantGibMOD( meansOfDeath ) ) {
		// gib death
		GibEntity( self, killer );
	} else {
		// normal death
		static int lastDeath;

		switch ( lastDeath ) {
		case 0:
			anim = BOTH_DEATH1;
			break;
		case 1:
			anim = BOTH_DEATH2;
			break;
		case 2:
		default:
			anim = BOTH_DEATH3;
			break;
		}

		// for the no-blood option, we need to prevent the health
		// from going to gib level
		if ( self->health <= GIB_HEALTH ) {
			self->health = GIB_HEALTH+1;
		}

		self->client->ps.legsAnim =
			( ( self->client->ps.legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;
		self->client->ps.torsoAnim =
			( ( self->client->ps.torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;

		G_AddEvent( self, EV_DEATH1 + lastDeath, killer );

		// the body can still be gibbed
		self->die = body_die;

		// globally cycle through the different death animations
		lastDeath = ( lastDeath + 1 ) % 3;

		if (self->s.eFlags & EF_KAMIKAZE) {
			Kamikaze_DeathTimer( self );
		}
	}

	trap_LinkEntity (self);

}


/*
================
CheckArmor
================
*/
int CheckArmor (gentity_t *ent, int damage, int dflags)
{
	gclient_t	*client;
	int			save;
	int			count;
    float	    protection;

	if (!damage)
		return 0;

	client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	// armor
	count = client->ps.stats[STAT_ARMOR];
    if (count <= 0) {
        return 0;
    }

	protection = BG_GetArmorProtection( client->ps.stats[STAT_ARMORCLASS] );

    save = ceil(damage * protection);
	if (save >= count)
		save = count;

	if (!save)
		return 0;

    if (g_gametype.integer != GT_KINGOFTHEHILL || !ent->client->ps.powerups[PW_KING]) {
        client->ps.stats[STAT_ARMOR] -= save;
        if (client->ps.stats[STAT_ARMOR] <= 0) {
            client->ps.stats[STAT_ARMORCLASS] = ARM_NONE;
        }
    }

	return save;
}

/*
================
RaySphereIntersections
================
*/
int RaySphereIntersections( vec3_t origin, float radius, vec3_t point, vec3_t dir, vec3_t intersections[2] ) {
	float b, c, d, t;

	//	| origin - (point + t * dir) | = radius
	//	a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	//	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	//	c = (point[0] - origin[0])^2 + (point[1] - origin[1])^2 + (point[2] - origin[2])^2 - radius^2;

	// normalize dir so a = 1
	VectorNormalize(dir);
	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	c = (point[0] - origin[0]) * (point[0] - origin[0]) +
		(point[1] - origin[1]) * (point[1] - origin[1]) +
		(point[2] - origin[2]) * (point[2] - origin[2]) -
		radius * radius;

	d = b * b - 4 * c;
	if (d > 0) {
		t = (- b + sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[0]);
		t = (- b - sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[1]);
		return 2;
	}
	if ( d == 0 ) {
		t = ( -b ) / 2;
		VectorMA( point, t, dir, intersections[0] );
		return 1;
	}
	return 0;
}

/*
================
G_DeflectorEffect
================
*/
int G_DeflectorEffect( gentity_t *targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir ) {
	gentity_t	*impact;
	vec3_t		intersections[2], vec;
	int			n;

	if ( !targ->client ) {
		return qfalse;
	}
	VectorCopy(dir, vec);
	VectorInverse(vec);
	n = RaySphereIntersections( targ->client->ps.origin, INVUL_RADIUS, point, vec, intersections);
	if (n > 0) {
		impact = G_TempEntity( targ->client->ps.origin, EV_DEFLECTOR_IMPACT );
		VectorSubtract(intersections[0], targ->client->ps.origin, vec);
		vectoangles(vec, impact->s.angles);
		impact->s.angles[0] += 90;
		if (impact->s.angles[0] > 360)
			impact->s.angles[0] -= 360;
		if ( impactpoint ) {
			VectorCopy( intersections[0], impactpoint );
		}
		if ( bouncedir ) {
			VectorCopy( vec, bouncedir );
			VectorNormalize( bouncedir );
		}
		return qtrue;
	}
	return qfalse;
}

/*
============
G_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/

/*
============
G_AttackFromMOD
Maps a means-of-death constant to the corresponding attack index.
============
*/
int G_AttackFromMOD( int mod ) {
	for ( int i = 1; i < ATT_NUM_ATTACKS; i++ ) {
		if ( bg_attacklist[i].meansOfDeath == mod || bg_attacklist[i].splashMeansOfDeath == mod ) {
			return i;
		}
	}
	return ATT_NONE;
}

void G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
			   vec3_t dir, vec3_t point, int damage, int dflags, int mod ) {
	int			take;
	int			asave;
	int			knockback;
	int			max;
	int			wp;
	vec3_t		bouncedir, impactpoint;
	int			attIdx;

	if (!targ->takedamage) {
		return;
	}

	// the intermission has already been qualified for, so don't
	// allow any extra scoring
	if ( level.intermissionQueued ) {
		return;
	}

	attIdx = G_AttackFromMOD( mod );

	if ( targ->client ) {
		if ( targ->client->deflectorTime > level.time) {
			if ( dir && point ) {
				G_DeflectorEffect( targ, dir, point, impactpoint, bouncedir );
			}
			return;
		}
	}

	if ( !inflictor ) {
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if ( !attacker ) {
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	// shootable doors / buttons don't actually have any health
	if ( targ->s.eType == ET_MOVER ) {
		if ( targ->use && targ->moverState == MOVER_POS1 ) {
			targ->use( targ, inflictor, attacker );
		}
		return;
	}
#if FEAT_OVERLOAD
	if( g_gametype.integer == GT_OBELISK && CheckObeliskAttack( targ, attacker ) ) {
		return;
	}
#endif
	// reduce damage by the attacker's handicap value
	// unless they are rocket jumping
	if ( attacker->client && attacker != targ ) {
		max = MAX_HEALTH;
		damage = damage * max / 100;
	}

	if ( targ->client && targ->client->noclip ) {
		return;
	}

	if ( !dir ) {
		dflags |= DAMAGE_NO_KNOCKBACK;
	} else {
		VectorNormalize(dir);
	}

	knockback = damage;

	// no knockback cap: skip the 200 limit for extreme physics
	if ( !g_excessive.integer && knockback > 200 ) {
		knockback = 200;
	}

	if ( targ->flags & FL_NO_KNOCKBACK ) {
		knockback = 0;
	}
	if ( dflags & DAMAGE_NO_KNOCKBACK ) {
		knockback = 0;
	}

	if ( targ->client ) {
		// figure momentum add, even if the damage won't be taken
		if ( knockback ) {
			vec3_t	kvel;
			float   knockbackScale;

			if (targ != attacker) {
				if (g_instagib.integer) {
					switch (mod) {
					case MOD_GAUNTLET:
					case MOD_GAUNTLET_LUNGE:
					case MOD_GRENADE:
					case MOD_ROCKET:
					case MOD_ROCKET_MORTAR:
					case MOD_RAILGUN:
						damage = INFINITE;
					}
				}
			}

			knockbackScale = ( targ == attacker ) ?
				bg_attacklist[attIdx].selfKnockbackScale :
				bg_attacklist[attIdx].knockbackScale;

			if ( g_excessive.integer && targ != attacker ) {
				knockbackScale *= 2;
			}

			VectorScale( dir, (float)knockback * knockbackScale, kvel );
			VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );

			// set the timer so that the other client can't cancel
			// out the movement immediately
			if ( !targ->client->ps.pm_time ) {
				int		t;

				t = knockback * 2;
				// no knockback boundaries
				// if ( t < 50 ) {
				// 	t = 50;
				// }
				// if ( t > 200 ) {
				// 	t = 200;
				// }
				targ->client->ps.pm_time = t;
				targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
			}
		}

		// attacker hook release
		if (targ->client->hook && targ->client->hook->enemy == attacker) {
			Offhand_Grapple_Free(targ->client->hook);
		}
	}

	// check for completely getting out of the damage
	if ( !(dflags & DAMAGE_NO_PROTECTION) ) {

		// if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
		// if the attacker was on the same team
		if ( targ != attacker && !(dflags & DAMAGE_NO_TEAM_PROTECTION) && OnSameTeam (targ, attacker)  ) {
			if ( !g_friendlyFire.integer ) {
				return;
			}
		}

		// check for godmode
		if ( targ->flags & FL_GODMODE ) {
			return;
		}

#if FEAT_SPAWN_PROTECTION
		// spawn protection (2B): block damage for g_spawnProtect * 1000ms after respawn
		if ( g_spawnProtect.integer > 0 && targ->client && targ->client->spawnprotected ) {
			if ( level.time > targ->client->respawnTime + ( g_spawnProtect.integer * 1000 ) ) {
				targ->client->spawnprotected = qfalse;
				targ->client->ps.eFlags &= ~EF_SPAWN_PROTECT;
			} else {
				// FIXME(@eser) absorb damage (let it through) but prevent score award
				//              attacker gets no points for spawnkills
				//              so damage continues through to reduce health, but if it kills:
				//              player_die will check spawnprotected and skip AddScore
				//              so we shouldn't `return;` here for now
			}
		}
#endif
	}

	// battlesuit protects from all radius damage (but takes knockback)
	// and protects 75% against all damage
	if ( targ->client && targ->client->ps.powerups[PW_BATTLESUIT] ) {
		G_AddEvent( targ, EV_POWERUP_BATTLESUIT, 0 );
		if ( ( dflags & DAMAGE_RADIUS ) || ( mod == MOD_FALLING ) ) {
			return;
		}

		damage *= 0.25f;
	}

	// add to the attacker's hit counter (if the target isn't a general entity like a prox mine)
	if ( attacker->client && targ->client
			&& targ != attacker && targ->health > 0
			&& targ->s.eType != ET_MISSILE
			&& targ->s.eType != ET_GENERAL) {
		if ( OnSameTeam( targ, attacker ) ) {
			attacker->client->ps.persistant[PERS_HITS]--;
		} else {
			attacker->client->ps.persistant[PERS_HITS]++;
		}
	}

	// always give quarter damage if hurting self
	// calculated after knockback, so rocket jumping works
	if (targ == attacker) {
		damage *= 0.25f;
	}

	if ( damage < 1 ) {
		damage = 1;
	}
	take = damage;

	if ( bg_attacklist[attIdx].armorPiercing ) {
		// armor piercing
		asave = 0;
	} else {
		// save some from armor
		asave = CheckArmor(targ, take, dflags);
		take -= asave;
	}

	if ( g_debugDamage.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "%i: client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number,
			targ->health, take, asave );
	}

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( targ->client ) {
		if ( attacker ) {
			targ->client->ps.persistant[PERS_LAST_ATTACKER] = attacker->s.number;
		} else {
			targ->client->ps.persistant[PERS_LAST_ATTACKER] = ENTITYNUM_WORLD;
		}
		targ->client->damage_armor += asave;
		targ->client->damage_blood += take;
		targ->client->damage_knockback += knockback;
		if ( dir ) {
			VectorCopy ( dir, targ->client->damage_from );
			targ->client->damage_fromWorld = qfalse;
		} else {
			VectorCopy ( targ->r.currentOrigin, targ->client->damage_from );
			targ->client->damage_fromWorld = qtrue;
		}
	}

	// See if it's the player hurting the emeny flag carrier
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF ) {
		Team_CheckHurtCarrier(targ, attacker);
	}

	if (targ->client) {
		// set the last client who damaged the target
		targ->client->lasthurt_client = attacker->s.number;
		targ->client->lasthurt_mod = mod;
        targ->client->lasthurt_time = level.time;
	}

	if ( take <= 0 ) {
		return;
	}

#if FEAT_DAMAGE_PLUMS
	// damage plums (2A): show floating number to attacker
	if ( targ->client && attacker != targ && attacker->client ) {
		DamagePlum( attacker, targ, take );
	}
#endif

	if ( attacker && attacker->client ) {
		// berserk
		if ( attacker->client->ps.powerups[PW_BERSERK]
			&& attacker != targ
			&& attacker->client && targ->client && attacker->health > 0
			&& !OnSameTeam( targ, attacker ) ) {
			int heal = ( take < targ->health ) ? take : targ->health;
			attacker->health += heal;
			if ( attacker->health > MAX_HEALTH ) {
				attacker->health = MAX_HEALTH;
			}
			attacker->client->ps.stats[STAT_HEALTH] = attacker->health;
		}

		// per-attack damage tracking
		if ( attIdx > ATT_NONE && attIdx < ATT_NUM_ATTACKS ) {
			attacker->client->attackStats[attIdx].damage += take;
		}
	}

	// do the damage
	targ->health = targ->health - take;
	if ( targ->client ) {
		targ->client->ps.stats[STAT_HEALTH] = targ->health;
	}

#if FEAT_WIREDNET_OBSERVER
	// QUIC event: damage with positions
	if ( take > 0 && attacker ) {
		vec3_t att_pos = {0, 0, 0};
		if ( attacker->client )
			VectorCopy( attacker->client->ps.origin, att_pos );
		trap_WiredNet_EmitDamage( attacker->s.number, targ->s.number, take, mod, att_pos, targ->r.currentOrigin );
	}
#endif
	if ( take > 0 && targ->client ) {
		trap_WCE_EmitEvent( WCE_PLAYER_TAKE_DAMAGE,
		                    targ->s.number, targ->s.number,
		                    targ->r.currentOrigin,
		                    take,
		                    attacker ? attacker->s.number : -1,
		                    0.0f, NULL );
	}

	if ( targ->health <= 0 ) {
		if ( targ->client )
			targ->flags |= FL_NO_KNOCKBACK;

		if (targ->health < -999)
			targ->health = -999;

		targ->enemy = attacker;
		targ->die (targ, inflictor, attacker, take, mod);
		return;
	}
	if ( targ->pain ) {
		targ->pain( targ, attacker, take );
	}
}


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage (gentity_t *targ, vec3_t origin) {
	vec3_t	dest;
	trace_t	tr;
	vec3_t	midpoint;
	vec3_t	offsetmins = {-15, -15, -15};
	vec3_t	offsetmaxs = {15, 15, 15};

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin is 0,0,0
	VectorAdd (targ->r.absmin, targ->r.absmax, midpoint);
	VectorScale (midpoint, 0.5, midpoint);

	VectorCopy(midpoint, dest);
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0 || tr.entityNum == targ->s.number)
		return qtrue;

	// this should probably check in the plane of projection,
	// rather than in world coordinate
	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0)
		return qtrue;

	return qfalse;
}


/*
============
G_RadiusDamage
============
*/
qboolean G_RadiusDamage ( vec3_t origin, gentity_t *attacker, float damage, float radius,
    gentity_t *ignore, int mod, qboolean underWater) {
	float		points, dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;
	qboolean	hitClient = qfalse;

// eser - lightning discharge
    // if we're not underwater, forget it!
    if (underWater && !(trap_PointContents(origin, -1) & MASK_WATER)) {
        return qfalse;
    }
// eser - lightning discharge

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

		points = damage * ( 1.0 - dist / radius );

		if( CanDamage (ent, origin) ) {
			if( LogAccuracyHit( ent, attacker ) ) {
				hitClient = qtrue;
			}
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			// dir[2] += 24;

            // PM: Add some extra knockback
            if (ent == attacker) { // rjumps are same as in normal Q3A
                dir[2] += 24;
            }
            else {
                dir[2] += 40; // additional vertical velocity for combo potential
            }

			if (mod == MOD_ROCKET_MORTAR || mod == MOD_ROCKET_MORTAR_SPLASH) {
				dir[2] += 40;
			}

			G_Damage (ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, mod);
		}
	}

	return hitClient;
}

// New radius damage function (called if radiusDamageFix is enabled)
qboolean PM_RadiusDamage(vec3_t origin, gentity_t *attacker, float damage, float radius,
    gentity_t *ignore, int mod, vec3_t viewpoint) {
    float		points, dist;
    gentity_t	*ent;
    int			entityList[MAX_GENTITIES];
    int			numListedEntities;
    vec3_t		mins, maxs;
    vec3_t		v;
    vec3_t		dir;
    int			i, e;
    qboolean	hitClient = qfalse;
    trace_t		tr;

    if (radius < 1) {
        radius = 1;
    }

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        if (ent == ignore)
            continue;
        if (!ent->takedamage)
            continue;

        // find the distance from the edge of the bounding box
        for (i = 0; i < 3; i++) {
            if (origin[i] < ent->r.absmin[i]) {
                v[i] = ent->r.absmin[i] - origin[i];
            }
            else if (origin[i] > ent->r.absmax[i]) {
                v[i] = origin[i] - ent->r.absmax[i];
            }
            else {
                v[i] = 0;
            }
        }

        dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }

        // Make sure the ent is visible from the viewpoint! // KHAILE
        trap_Trace(&tr, viewpoint, NULL, NULL, ent->r.currentOrigin, ENTITYNUM_NONE, MASK_SOLID);
        if (tr.fraction != 1.0) {
            continue;		// we hit something solid enough to stop the blast
        }
        // !KHAILE

        points = damage * (1.0 - dist / radius);

        if (CanDamage(ent, origin)) {
            if (LogAccuracyHit(ent, attacker)) {
                hitClient = qtrue;
            }
            VectorSubtract(ent->r.currentOrigin, origin, dir);
            // push the center of mass higher than the origin so players
            // get knocked into the air more
            // PM: Add some extra knockback
            if (ent == attacker) { // rjumps are same as in normal Q3A
                dir[2] += 24;
            }
            else {
                dir[2] += 40; // additional vertical velocity for combo potential
            }

			G_Damage(ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, mod);
        }
    }

    return hitClient;
}
