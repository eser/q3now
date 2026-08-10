// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
#include "g_local.h"
LOG_DECLARE_CHANNEL( ch_game, "game" );

//==========================================================

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8)
Gives the activator all the items pointed to.
*/
void Q3_Use_Target_Give( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gentity_t	*t;
	trace_t		trace;

	if ( !activator->client ) {
		return;
	}

	if ( !ent->target ) {
		return;
	}

	memset( &trace, 0, sizeof( trace ) );
	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), ent->target)) != NULL ) {
		if ( !t->item ) {
			continue;
		}
		Touch_Item( t, activator, &trace );

		// make sure it isn't going to respawn or show any events
		t->nextthink = 0;
		trap_UnlinkEntity( t );
	}
}

void SP_q3_target_give( gentity_t *ent ) {
	ent->use = Q3_Use_Target_Give;
}


//==========================================================

/*QUAKED target_remove_powerups (1 0 0) (-8 -8 -8) (8 8 8)
takes away all the activators powerups.
Used to drop flight powerups into death puts.
*/
void Q3_Use_target_remove_powerups( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if( !activator->client ) {
		return;
	}

	if( activator->client->ps.powerups[PW_REDFLAG] ) {
		Team_ReturnFlag( TEAM_RED );
	} else if( activator->client->ps.powerups[PW_BLUEFLAG] ) {
		Team_ReturnFlag( TEAM_BLUE );
	} else if( activator->client->ps.powerups[PW_NEUTRALFLAG] ) {
		Team_ReturnFlag( TEAM_FREE );
	}

	memset( activator->client->ps.powerups, 0, sizeof( activator->client->ps.powerups ) );
}

void SP_q3_target_remove_powerups( gentity_t *ent ) {
	ent->use = Q3_Use_target_remove_powerups;
}


//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8) TOGGLE
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
TOGGLE : while armed, a second trigger cancels the pending delayed fire
instead of re-arming it (acts as an on/off switch for the delayed shot).
*/
void Q3_Think_Target_Delay( gentity_t *ent ) {
	ent->nextthink = 0;
	G_UseTargets( ent, ent->activator );
}

void Q3_Use_Target_Delay( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	// TOGGLE: if a delayed fire is already armed, this trigger disarms it
	if ( ( ent->spawnflags & 1 ) && ent->nextthink ) {
		ent->nextthink = 0;
		return;
	}
	ent->nextthink = level.time + ( ent->wait + ent->random * crandom() ) * 1000;
	ent->think = Q3_Think_Target_Delay;
	ent->activator = activator;
}

void SP_q3_target_delay( gentity_t *ent ) {
	// check delay for backwards compatibility
	if ( !G_SpawnFloat( "delay", "0", &ent->wait ) ) {
		G_SpawnFloat( "wait", "1", &ent->wait );
	}

	if ( !ent->wait ) {
		ent->wait = 1;
	}
	ent->use = Q3_Use_Target_Delay;
}


//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8)
"count" number of points to add, default 1

The activator is given this many points.
*/
void Q3_Use_Target_Score (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	AddScore( activator, ent->r.currentOrigin, ent->count );
}

void SP_q3_target_score( gentity_t *ent ) {
	if ( !ent->count ) {
		ent->count = 1;
	}
	ent->use = Q3_Use_Target_Score;
}


//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) redteam blueteam private
"message"	text to print
If "private", only the activator gets the message.  If no checks, all clients get the message.
*/
void Q3_Use_Target_Print (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	if ( activator->client && ( ent->spawnflags & 4 ) ) {
		trap_SendServerCommand( activator-g_entities, va("cp \"%s\"", ent->message ));
		return;
	}

	if ( ent->spawnflags & 3 ) {
		if ( ent->spawnflags & 1 ) {
			G_TeamCommand( TEAM_RED, va("cp \"%s\"", ent->message) );
		}
		if ( ent->spawnflags & 2 ) {
			G_TeamCommand( TEAM_BLUE, va("cp \"%s\"", ent->message) );
		}
		return;
	}

	trap_SendServerCommand( -1, va("cp \"%s\"", ent->message ));
}

void SP_q3_target_print( gentity_t *ent ) {
	ent->use = Q3_Use_Target_Print;
}


//==========================================================


/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off global activator
"noise"		wav file to play

A global sound will play full volume throughout the level.
Activator sounds will play on the player that activated the target.
Global and activator sounds can't be combined with looping.
Normal sounds play each time the target is used.
Looped sounds will be toggled by use functions.
Multiple identical looping sounds will just increase volume without any speed cost.
"wait" : Seconds between auto triggerings, 0 = don't auto trigger
"random"	wait variance, default is 0
*/
void Q3_Use_Target_Speaker (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	if (ent->spawnflags & 3) {	// looping sound toggles
		if (ent->s.loopSound)
			ent->s.loopSound = 0;	// turn it off
		else
			ent->s.loopSound = ent->noise_index;	// start it
	}else {	// normal sound
		if ( ent->spawnflags & 8 ) {
			G_AddEvent( activator, EV_GENERAL_SOUND, ent->noise_index );
		} else if (ent->spawnflags & 4) {
			G_AddEvent( ent, EV_GLOBAL_SOUND, ent->noise_index );
		} else {
			G_AddEvent( ent, EV_GENERAL_SOUND, ent->noise_index );
		}
	}
}

void SP_q3_target_speaker( gentity_t *ent ) {
	char	buffer[MAX_QPATH];
	char	*s;

	G_SpawnFloat( "wait", "0", &ent->wait );
	G_SpawnFloat( "random", "0", &ent->random );

	if ( !G_SpawnString( "noise", "NOSOUND", &s ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "target_speaker without a noise key at %s", vtos( ent->s.origin ) );
	}

	// force all client relative sounds to be "activator" speakers that
	// play on the entity that activates it
	if ( s[0] == '*' ) {
		ent->spawnflags |= 8;
	}

	// FIXME(@eser) it's for backward compatibility for old Q3A/Q3TA maps
	if ( !COM_GetExtension( s )[0] ) {
		char mapAssetProfile[16];
		int mapVersion;
		const char *defaultExt;

		trap_Cvar_VariableStringBuffer( "com_mapAssetProfile", mapAssetProfile, sizeof( mapAssetProfile ) );
		mapVersion = trap_Cvar_VariableIntegerValue( "com_mapBspVersion" );
		if ( !Q_stricmp( mapAssetProfile, "legacy" ) || ( mapVersion > 0 && ( mapVersion <= 46 || mapVersion == 68 ) ) ) {
			defaultExt = "wav";
		} else {
			defaultExt = "opus";
		}

		Com_sprintf( buffer, sizeof( buffer ), "%s.%s", s, defaultExt );
	} else {
		Q_strncpyz( buffer, s, sizeof(buffer) );
	}
	ent->noise_index = G_SoundIndex(buffer);

	// a repeating speaker can be done completely client side
	ent->s.eType = ET_SPEAKER;
	ent->s.eventParm = ent->noise_index;
	ent->s.frame = ent->wait * 10;
	ent->s.clientNum = ent->random * 10;


	// check for prestarted looping sound
	if ( ent->spawnflags & 1 ) {
		ent->s.loopSound = ent->noise_index;
	}

	ent->use = Q3_Use_Target_Speaker;

	if (ent->spawnflags & 4) {
		ent->r.svFlags |= SVF_BROADCAST;
	}

	VectorCopy( ent->s.origin, ent->s.pos.trBase );

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	trap_LinkEntity( ent );
}



//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON
When triggered, fires a laser.  You can either set a target or a direction.
*/
void Q3_target_laser_think (gentity_t *self) {
	vec3_t	end;
	trace_t	tr;
	vec3_t	point;

	// if pointed at another entity, set movedir to point at it
	if ( self->enemy ) {
		VectorMA (self->enemy->s.origin, 0.5, self->enemy->r.mins, point);
		VectorMA (point, 0.5, self->enemy->r.maxs, point);
		VectorSubtract (point, self->s.origin, self->movedir);
		VectorNormalize (self->movedir);
	}

	// fire forward and see what we hit
	VectorMA (self->s.origin, 2048, self->movedir, end);

	trap_Trace( &tr, self->s.origin, NULL, NULL, end, self->s.number, CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE);

	// ENTITYNUM_NONE (1023) is truthy, and client 0 is falsy — so `if (tr.entityNum)`
	// both damages a non-existent entity on a clean miss AND skips client 0 on a hit.
	if ( tr.entityNum != ENTITYNUM_NONE ) {
		// hurt it if we can
		G_Damage ( &g_entities[tr.entityNum], self, self->activator, self->movedir,
			tr.endpos, self->damage, DAMAGE_NO_KNOCKBACK, MOD_TARGET_LASER);
	}

	VectorCopy (tr.endpos, self->s.origin2);

	trap_LinkEntity( self );
	self->nextthink = level.time + FRAMETIME;
}

void Q3_target_laser_on (gentity_t *self)
{
	if (!self->activator)
		self->activator = self;
	Q3_target_laser_think (self);
}

void Q3_target_laser_off (gentity_t *self)
{
	trap_UnlinkEntity( self );
	self->nextthink = 0;
}

void Q3_target_laser_use (gentity_t *self, gentity_t *other, gentity_t *activator)
{
	self->activator = activator;
	if ( self->nextthink > 0 )
		Q3_target_laser_off (self);
	else
		Q3_target_laser_on (self);
}

void Q3_target_laser_start (gentity_t *self)
{
	gentity_t *ent;

	self->s.eType = ET_BEAM;

	if (self->target) {
		ent = G_Find (NULL, FOFS(targetname), self->target);
		if (!ent) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
		}
		self->enemy = ent;
	} else {
		G_SetMovedir (self->s.angles, self->movedir);
	}

	self->use = Q3_target_laser_use;
	self->think = Q3_target_laser_think;

	if ( !self->damage ) {
		self->damage = 1;
	}

	if (self->spawnflags & 1)
		Q3_target_laser_on (self);
	else
		Q3_target_laser_off (self);
}

void SP_q3_target_laser (gentity_t *self)
{
	// let everything else get spawned before we start firing
	self->think = Q3_target_laser_start;
	self->nextthink = level.time + FRAMETIME;
}


//==========================================================

void Q3_target_teleporter_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	gentity_t	*dest;

	if (!activator->client)
		return;
	dest = 	G_PickTarget( self->target );
	if (!dest) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( activator, dest->s.origin, dest->s.angles, 400 );
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8)
The activator will be teleported away.
*/
void SP_q3_target_teleporter( gentity_t *self ) {
	if (!self->targetname)
		Com_Log( SEV_INFO, LOG_CH(ch_game), "untargeted %s at %s\n", self->classname, vtos(self->s.origin));

	self->use = Q3_target_teleporter_use;
}

//==========================================================


/*QUAKED target_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM ONCE
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them.
"count" fire only once every N triggers (a counter; default 1 = every trigger).
"target2" a second target group fired alongside "target".
ONCE : free this relay after it fires (one-shot).
*/
void Q3_target_relay_use (gentity_t *self, gentity_t *other, gentity_t *activator) {
	gentity_t	*ent;

	if ( ( self->spawnflags & 1 ) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_RED ) {
		return;
	}
	if ( ( self->spawnflags & 2 ) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}
	if ( self->spawnflags & 4 ) {
		ent = G_PickTarget( self->target );
		// guard ent != self: a relay whose own targetname is in its target list
		// would otherwise re-enter this function forever (stack overflow). Mirrors
		// the self-skip in G_UseTargets.
		if ( ent && ent != self && ent->use ) {
			ent->use( ent, self, activator );
		}
		if ( self->target2 ) {
			ent = G_PickTarget( self->target2 );
			if ( ent && ent != self && ent->use ) {
				ent->use( ent, self, activator );
			}
		}
		return;
	}

	// count gate: only fire once N triggers have accumulated. damage holds the
	// running trigger count, count holds the threshold (set in spawn, >= 1).
	self->damage++;
	if ( self->damage < self->count ) {
		return;
	}

	G_UseTargets( self, activator );

	// fire the secondary target group (every entity named by target2)
	if ( self->target2 ) {
		ent = NULL;
		while ( ( ent = G_Find( ent, FOFS( targetname ), self->target2 ) ) != NULL ) {
			// guard ent != self: a relay whose own targetname is in its target2
			// group would re-enter this use fn forever (direct self-recursion /
			// stack overflow). Mirrors the self-skip in G_UseTargets (g_utils.c)
			// and the RANDOM path above. NOTE (known residual, out of scope): mutual
			// recursion A->B->A is a deeper class needing a depth counter.
			if ( ent->use && ent != self ) {
				ent->use( ent, self, activator );
			}
		}
	}

	// ONCE: self-destruct after firing; otherwise re-arm the counter
	if ( self->spawnflags & 8 ) {
		G_FreeEntity( self );
	} else {
		self->damage = 0;
	}
}

void SP_q3_target_relay (gentity_t *self) {
	// count is the trigger threshold; default and clamp to fire every trigger
	if ( self->count < 1 ) {
		self->count = 1;
	}
	self->damage = 0;
	self->use = Q3_target_relay_use;
}


//==========================================================

/*QUAKED target_logic (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM STAY_ON
A boolean AND-gate: fires its targets only once EVERY entity that targets it
(up to MAX_LOGIC_ENTITIES inputs) has triggered it. The activator can be forced
to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them.
STAY_ON : a triggered input stays latched until the gate fires (and resets),
instead of toggling off when triggered again.
*/
void Q3_target_logic_reset( gentity_t *self ) {
	int i;
	for ( i = 0; i < MAX_LOGIC_ENTITIES; i++ ) {
		self->logicEntities[i] = 0;
	}
}

void Q3_target_logic_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	int			i;
	int			triggerCount;	// entities that target this gate
	int			triggeredCount;	// inputs that have fired so far
	qboolean	found;
	gentity_t	*t;

	if ( ( self->spawnflags & 1 ) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_RED ) {
		return;
	}
	if ( ( self->spawnflags & 2 ) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}

	// count how many entities target this gate (its inputs)
	triggerCount = 0;
	t = NULL;
	while ( ( t = G_Find( t, FOFS( target ), self->targetname ) ) != NULL ) {
		if ( t != self ) {
			triggerCount++;
		}
	}

	// Slots store (entity number + 1) so 0 stays a distinct "empty" sentinel —
	// otherwise an input whose s.number is 0 (the world/first entity) would be
	// indistinguishable from an empty slot and never latch.
	int otherSlot = other->s.number + 1;

	// tally already-fired inputs; if THIS input is already latched, toggle it off
	// unless STAY_ON keeps it latched
	found = qfalse;
	triggeredCount = 0;
	for ( i = 0; i < MAX_LOGIC_ENTITIES; i++ ) {
		if ( self->logicEntities[i] ) {
			triggeredCount++;
			if ( self->logicEntities[i] == otherSlot ) {
				found = qtrue;
				if ( !( self->spawnflags & 8 ) ) {
					self->logicEntities[i] = 0;
					triggeredCount--;
				}
			}
		}
	}

	// new input — record it in the first free slot
	if ( !found ) {
		qboolean stored = qfalse;
		for ( i = 0; i < MAX_LOGIC_ENTITIES; i++ ) {
			if ( !self->logicEntities[i] ) {
				self->logicEntities[i] = otherSlot;
				triggeredCount++;
				stored = qtrue;
				break;
			}
		}
		// More than MAX_LOGIC_ENTITIES distinct inputs target this gate: the
		// fixed slot array is full, so triggeredCount can never reach
		// triggerCount and the AND-gate would silently never fire. Warn the
		// mapper instead of failing silently (raise MAX_LOGIC_ENTITIES to
		// support wider gates).
		if ( !stored ) {
			trap_Print( va( S_COLOR_YELLOW "WARNING: target_logic '%s' has more than "
				"%d inputs; extra inputs are ignored and the gate will never fire.\n",
				self->targetname ? self->targetname : "(unnamed)", MAX_LOGIC_ENTITIES ) );
		}
	}

	// all inputs satisfied → fire and reset the gate
	if ( triggerCount > 0 && triggerCount == triggeredCount ) {
		Q3_target_logic_reset( self );

		if ( self->spawnflags & 4 ) {
			gentity_t	*ent;

			ent = G_PickTarget( self->target );
			if ( ent && ent->use ) {
				ent->use( ent, self, activator );
			}
			return;
		}
		G_UseTargets( self, activator );
	}
}

void SP_q3_target_logic( gentity_t *self ) {
	self->use = Q3_target_logic_use;
	Q3_target_logic_reset( self );
}


//==========================================================

/*QUAKED target_gravity (.5 .5 .5) (-8 -8 -8) (8 8 8) GLOBAL
Sets the activator's gravity to the "count" value (default = the map's
environmental gravity). GLOBAL applies the change to every connected player.
The override is persistent — it survives the per-frame gravity recompute in
ClientEndFrame (a one-shot ps.gravity write would be overwritten next frame).
*/
void Q3_target_gravity_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	int gravity;
	int i;

	gravity = self->count ? self->count : (int)g_envGravity.value;

	if ( self->spawnflags & 1 ) {
		for ( i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected == CON_CONNECTED ) {
				level.clients[i].gravityOverride = gravity;
			}
		}
	} else if ( activator && activator->client ) {
		activator->client->gravityOverride = gravity;
	}
}

void SP_q3_target_gravity( gentity_t *self ) {
	self->use = Q3_target_gravity_use;
}


//==========================================================

/*QUAKED target_playerspeed (.5 .5 .5) (-8 -8 -8) (8 8 8) GLOBAL
Sets the activator's movement speed to the "speed" value (default =
DEFAULT_MOVESPEED_PLAYER). speed == -1 freezes the player (PM_FREEZE); a later
trigger with speed >= 0 unfreezes and restores movement. GLOBAL applies to every
connected player. The override is persistent — it is re-applied as the final word
in the per-frame speed recompute in ClientEndFrame.
*/
void Q3_target_playerspeed_apply( gclient_t *client, int speed ) {
	if ( speed == -1 ) {
		client->speedOverride = -1;
	} else {
		// speed >= 0: clear any freeze and set (0 restores the engine default)
		if ( client->speedOverride == -1 && client->ps.pm_type == PM_FREEZE ) {
			client->ps.pm_type = PM_NORMAL;
		}
		client->speedOverride = speed;
	}
}

void Q3_target_playerspeed_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	int speed;
	int i;

	// self->speed is a float key; -1 means freeze, 0 means "default" (clear override)
	speed = (int)self->speed;

	if ( self->spawnflags & 1 ) {
		for ( i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected == CON_CONNECTED ) {
				Q3_target_playerspeed_apply( &level.clients[i], speed );
			}
		}
	} else if ( activator && activator->client ) {
		Q3_target_playerspeed_apply( activator->client, speed );
	}
}

void SP_q3_target_playerspeed( gentity_t *self ) {
	self->use = Q3_target_playerspeed_use;
}


//==========================================================

/*QUAKED target_playerstats (.5 .5 .5) (-8 -8 -8) (8 8 8) ONLY_WHEN_LOWER NO_HEALTH NO_ARMOR
Instantly sets the activator's health ("health" key) and armor ("armor" key).
ONLY_WHEN_LOWER only raises a stat (never lowers it). NO_HEALTH skips health,
NO_ARMOR skips armor. Health/armor are clamped to MAX_HEALTH / MAX_ARMOR. Bots
are ignored. This is an instant set — health/armor are not recomputed per frame,
so no persistent override is needed.
*/
void Q3_target_playerstats_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	gclient_t	*client;
	int			value;

	if ( !activator || !activator->client ) {
		return;
	}
	// do not let bots trigger this entity
	if ( activator->r.svFlags & SVF_BOT ) {
		return;
	}
	client = activator->client;

	// health: skip if NO_HEALTH (2); with ONLY_WHEN_LOWER (1) only raise
	if ( !( self->spawnflags & 2 ) ) {
		if ( !( self->spawnflags & 1 ) || client->ps.stats[STAT_HEALTH] < self->health ) {
			value = self->health;
			if ( value > MAX_HEALTH ) {
				value = MAX_HEALTH;
			}
			client->ps.stats[STAT_HEALTH] = value;
			activator->health = value;
		}
	}

	// armor: skip if NO_ARMOR (4); with ONLY_WHEN_LOWER (1) only raise
	if ( !( self->spawnflags & 4 ) ) {
		if ( !( self->spawnflags & 1 ) || client->ps.stats[STAT_ARMOR] < self->armor ) {
			value = self->armor;
			if ( value > MAX_ARMOR ) {
				value = MAX_ARMOR;
			}
			client->ps.stats[STAT_ARMOR] = value;
			activator->armor = value;
		}
	}
}

void SP_q3_target_playerstats( gentity_t *self ) {
	self->use = Q3_target_playerstats_use;
}


//==========================================================

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator.
*/
void Q3_target_kill_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	G_Damage ( activator, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void SP_q3_target_kill( gentity_t *self ) {
	self->use = Q3_target_kill_use;
}

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
*/
void SP_q3_target_position( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
}

static void Q3_target_location_linkup(gentity_t *ent)
{
	int i;
	int n;

	if (level.locationLinked)
		return;

	level.locationLinked = qtrue;

	level.locationHead = NULL;

	trap_SetConfigstring( CS_LOCATIONS, "unknown" );

	for (i = 0, ent = g_entities, n = 1;
			i < level.num_entities;
			i++, ent++) {
		if (ent->classname && !Q_stricmp(ent->classname, "target_location")) {
			// lets overload some variables!
			if (n >= MAX_LOCATIONS)
				break;	// don't write configstrings past the CS_LOCATIONS region
			ent->health = n; // use for location marking
			trap_SetConfigstring( CS_LOCATIONS + n, ent->message );
			n++;
			ent->nextTrain = level.locationHead;
			level.locationHead = ent;
		}
	}

	// All linked together now
}

/*QUAKED target_location (0 0.5 0) (-8 -8 -8) (8 8 8)
Set "message" to the name of this location.
Set "count" to 0-7 for color.
0:white 1:red 2:green 3:yellow 4:blue 5:cyan 6:magenta 7:white

Closest target_location in sight used for the location, if none
in site, closest in distance
*/
void SP_q3_target_location( gentity_t *self ){
	self->think = Q3_target_location_linkup;
	self->nextthink = level.time + 200;  // Let them all spawn first

	G_SetOrigin( self, self->s.origin );
}

#if FEAT_EARTHQUAKE_SYSTEM
/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8)
Triggers a local or global view shake.
-------- KEYS --------
duration:  total shake time in seconds (default 10)
fadein:    fade-in time in seconds (default 1)
fadeout:   fade-out time in seconds (default 1)
amplitude: shake intensity 0-100 (default 100; 100 = maximum)
radius:    attenuation radius in units; use -1 for global (default -1)
*/
static void Q3_Use_target_earthquake( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gentity_t *event = G_TempEntity( ent->s.origin, EV_EARTHQUAKE );
	if ( event ) {
		VectorCopy( ent->s.angles,  event->s.angles );
		VectorCopy( ent->s.angles2, event->s.angles2 );
	}
}

void SP_q3_target_earthquake( gentity_t *ent ) {
	G_SpawnFloat( "duration",  "10",   &ent->s.angles[0] );
	G_SpawnFloat( "fadein",    "1",    &ent->s.angles[1] );
	G_SpawnFloat( "fadeout",   "1",    &ent->s.angles[2] );
	G_SpawnFloat( "amplitude", "100",  &ent->s.angles2[0] );
	G_SpawnFloat( "radius",    "-1",   &ent->s.angles2[1] );

	if ( ent->s.angles[0] < ent->s.angles[1] + ent->s.angles[2] ) {
		ent->s.angles[0] = ent->s.angles[1] + ent->s.angles[2];
	}

	ent->use = Q3_Use_target_earthquake;
}
#endif


//==========================================================

/*QUAKED target_unlink (1 0 0) (-8 -8 -8) (8 8 8) - - ALWAYS_UNLINK ALWAYS_LINK IMMEDIATELY
Toggles whether its targeted entities are linked into the world. An unlinked
trigger is no longer touchable (disabled); re-linking re-enables it — a phase
gate for turning triggers / brushes on and off at runtime.
ALWAYS_UNLINK : the targets are always unlinked (disabled), never re-linked.
ALWAYS_LINK   : the targets are always linked (enabled), never unlinked.
IMMEDIATELY   : act once at spawn (a few frames in, after the targets exist)
                instead of waiting for a trigger.
Default (no flag) : toggle each target's current linked state.
Also registered as "target_disable" for older maps (same behaviour).
*/

// spawnflag bits (match the QUAKED order above)
#define UNLINK_ALWAYS_UNLINK	4
#define UNLINK_ALWAYS_LINK		8
#define UNLINK_IMMEDIATELY		16

// Apply the link/unlink decision to one targeted entity. Reads the entity's
// actual r.linked state for the toggle, so re-linking works. A func_bobbing has
// no meaningful unlinked state (it is a constantly-moving mover) — for it,
// "unlinked" is expressed by flipping its s.eType to ET_INVISIBLE instead, the
// same dodge the source uses.
static void Q3_SetEntityLinked( gentity_t *t, int spawnflags ) {
	qboolean isBobbing = ( t->classname && !strcmp( t->classname, "func_bobbing" ) );

	if ( spawnflags & UNLINK_ALWAYS_UNLINK ) {
		if ( isBobbing ) {
			if ( t->s.eType == ET_MOVER ) t->s.eType = ET_INVISIBLE;
		} else if ( t->r.linked ) {
			trap_UnlinkEntity( t );
		}
	} else if ( spawnflags & UNLINK_ALWAYS_LINK ) {
		if ( isBobbing ) {
			if ( t->s.eType == ET_INVISIBLE ) t->s.eType = ET_MOVER;
		} else if ( !t->r.linked ) {
			trap_LinkEntity( t );
		}
	} else {
		// toggle on the current state
		if ( isBobbing ) {
			t->s.eType = ( t->s.eType == ET_MOVER ) ? ET_INVISIBLE : ET_MOVER;
		} else if ( t->r.linked ) {
			trap_UnlinkEntity( t );
		} else {
			trap_LinkEntity( t );
		}
	}
}

static void Q3_ToggleTargetsLinked( gentity_t *self ) {
	gentity_t	*t;

	if ( !self->target ) {
		return;
	}
	t = NULL;
	while ( ( t = G_Find( t, FOFS(targetname), self->target ) ) != NULL ) {
		if ( t == self ) {
			continue;
		}
		Q3_SetEntityLinked( t, self->spawnflags );
	}
}

void Q3_Use_target_unlink( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	Q3_ToggleTargetsLinked( self );
}

void Q3_target_unlink_think( gentity_t *self ) {
	self->nextthink = 0;
	Q3_ToggleTargetsLinked( self );
}

void SP_q3_target_unlink( gentity_t *self ) {
	self->use = Q3_Use_target_unlink;

	if ( self->spawnflags & UNLINK_IMMEDIATELY ) {
		// act a few frames in so the targeted entities have spawned first
		self->think = Q3_target_unlink_think;
		self->nextthink = level.time + FRAMETIME * 3;
	}
}


//==========================================================

/*QUAKED target_modify (1 0 0) (-8 -8 -8) (8 8 8)
Edits one field of its targeted entities at runtime.
"key"	which field to change. Supported (game-side, network-safe) fields:
        spawnflags, wait, speed, health, count, dmg, target, target2, message.
"value"	the new value (a number for numeric fields, text for string fields).
Networked display fields (light/color) are deliberately not editable here, to
avoid client desync.
*/
void Q3_Use_target_modify( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	gentity_t	*t;

	if ( !self->key || !self->target ) {
		return;
	}

	t = NULL;
	while ( ( t = G_Find( t, FOFS(targetname), self->target ) ) != NULL ) {
		const char	*v = self->value ? self->value : "";

		if ( t == self ) {
			continue;
		}

		// Whitelist of arena-relevant, GAME-PRIVATE gentity fields only. Nothing
		// here writes an s.* (networked entityState) field, so a modify can never
		// desync clients. Unknown / unsupported keys are ignored.
		//
		// String fields share self->value directly rather than G_NewString()'ing a
		// fresh copy per fire: self->value is already a permanent string from the
		// spawn-field pool (G_NewString at spawn), the map-author value never
		// changes, and pool strings are never freed — so the shared pointer is
		// safe and, crucially, allocates nothing at runtime. (A per-fire
		// G_NewString would grow the never-freed 256 KB pool on every trigger and
		// could exhaust it in a long match — a needless DoS vector.)
		if ( !Q_stricmp( self->key, "spawnflags" ) ) {
			t->spawnflags = atoi( v );
		} else if ( !Q_stricmp( self->key, "wait" ) ) {
			t->wait = atof( v );
		} else if ( !Q_stricmp( self->key, "speed" ) ) {
			t->speed = atof( v );
		} else if ( !Q_stricmp( self->key, "health" ) ) {
			t->health = atoi( v );
		} else if ( !Q_stricmp( self->key, "count" ) ) {
			t->count = atoi( v );
		} else if ( !Q_stricmp( self->key, "dmg" ) || !Q_stricmp( self->key, "damage" ) ) {
			t->damage = atoi( v );
		} else if ( !Q_stricmp( self->key, "target" ) ) {
			t->target = self->value;
		} else if ( !Q_stricmp( self->key, "target2" ) ) {
			t->target2 = self->value;
		} else if ( !Q_stricmp( self->key, "message" ) ) {
			t->message = self->value;
		}
	}
}

void SP_q3_target_modify( gentity_t *self ) {
	self->use = Q3_Use_target_modify;
}

//==========================================================

/*QUAKED target_music (0 .7 .7) (-8 -8 -8) (8 8 8)
When triggered, switches the background music track for all clients.
"music"  the music track to play (e.g. "music/level.opus"); empty = stop music.

Zero new ABI: CS_MUSIC (configstring slot 2) already exists and the client
already restarts the background track whenever that configstring changes
(cg_servercmds.c CG_StartMusic on CS_MUSIC). The worldspawn sets CS_MUSIC once
at level load (g_spawn.c); this entity is the runtime, mapper-triggerable switch.
Adapted from EntityPlus (QIIIA/id-derived) target_music — see THIRD_PARTY_LICENSES.
*/
void Q3_Use_target_music( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	// self->message holds the track name, copied once at spawn into the permanent
	// level pool (G_NewString). A NULL/empty track stops the music — both
	// behaviours ride the existing CS_MUSIC path, so no new configstring, event,
	// or netfield is introduced.
	trap_SetConfigstring( CS_MUSIC, self->message ? self->message : "" );
}

void SP_q3_target_music( gentity_t *self ) {
	char *track;

	// G_SpawnString returns a pointer into level.spawnVarChars, which is reused
	// per-entity during parsing (G_ParseSpawnVars resets numSpawnVarChars=0), so
	// it must be copied into the permanent level pool before the next entity
	// spawns. G_NewString does this exactly once at spawn — no per-fire alloc.
	G_SpawnString( "music", "", &track );
	self->message = G_NewString( track );

	self->use = Q3_Use_target_music;
}

// ── savegame callback registry — TIER 2 file-local sub-list ──────────────────
// The file-static callbacks defined above, published through SG_Register_g_target_q3().
// Q3_Use_target_earthquake only exists under FEAT_EARTHQUAKE_SYSTEM; the list
// (SG_LOCAL_CB_g_target_q3 in g_save_localcbs.h) guards it identically. See
// g_save_funcs.h.
#include "g_save_funcs.h"
#include "g_save_localcbs.h"

SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_target_q3, SG_Register_g_target_q3 )
