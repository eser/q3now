// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_trigger_q1.c -- Q1 trigger entity spawn functions

#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

void Q3_InitTrigger( gentity_t *self );

/* trigger_multiple spawnflags */
#define Q1_TMUL_NOTOUCH   1   /* fire on use instead of touch */
/* bit 2: unallocated (no QC reference; player-only is always implicit) */
#define Q1_TMUL_SILENT    4   /* suppress sounds */

/* trigger_counter spawnflags */
#define COUNTER_NOMESSAGE  1   /* suppress hint messages */
#define COUNTER_LOOPS      2   /* reset counter after firing instead of removing self */

/* trigger_push spawnflags (rerelease additions) */
#define Q1_PUSH_ONCE      1   /* fire once then remove */
#define Q1_PUSH_ADDITIVE  2   /* accumulate velocity instead of replace */
#define Q1_PUSH_START_OFF 4   /* disabled until fired */

/* trigger_hurt spawnflags (rerelease bit layout) */
#define Q1_HURT_START_OFF    1   /* disabled until fired */
#define Q1_HURT_SILENT       4   /* no sound */
#define Q1_HURT_MONSTER_ONLY 8   /* only damages entities with FL_Q1MONSTER flag */
#define Q1_HURT_SLOW         16  /* 1-second rate instead of per-frame */

/* trigger_teleport spawnflags (rerelease) */
#define Q1_TELE_PLAYER_ONLY   1   /* only teleport players, not monsters */
#define Q1_TELE_SILENT        2   /* suppress teleport sound */
#define Q1_TELE_IGNORE_TNAME  4   /* find any info_teleport_destination */

/* Q1 monster flag for MONSTER_ONLY check */
#define FL_Q1MONSTER  0x00000080


/*
===================
SP_q1_info_teleport_destination

Positional marker for Q1 trigger_teleport targets.
No geometry — just provides an origin for teleportation.
===================
*/
void SP_q1_info_teleport_destination( gentity_t *ent ) {
	VectorCopy( ent->s.angles, ent->movedir );
	VectorClear( ent->s.angles );
	ent->s.origin[2] += 27.0f;
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_once

Fires targets exactly once, then removes itself.
sounds=1: secret.opus, sounds=2: talk.opus, sounds=3: trigger1.opus
===================
*/
static void q1_trigger_once_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) {
		return;
	}

	/* Direction check: if the trigger has an angle, only fire when the player
	   is facing toward the trigger's movedir (within 90 degrees). Mirrors Q1
	   multi_touch angle check using makevectors(other.angles)*movedir. */
	if ( !VectorCompare( self->movedir, vec3_origin ) ) {
		vec3_t forward;
		AngleVectors( other->client->ps.viewangles, forward, NULL, NULL );
		if ( DotProduct( forward, self->movedir ) < 0 ) {
			return;
		}
	}

	G_UseTargets( self, other );

	if ( self->noise_index ) {
		G_Sound( self, CHAN_AUTO, self->noise_index );
	}

	self->touch    = NULL;
	self->nextthink = level.time + FRAMETIME;
	self->think    = G_FreeEntity;
}

static void q1_trigger_once_pain( gentity_t *self, gentity_t *attacker, int damage ) {
	/* Q1 trigger_once with health > 0: shoot-to-activate, fires once then removes.
	 * Q1 ref: trigger_once delegates to trigger_multiple with wait=-1;
	 *         multi_killed fires targets then removes self. */
	G_UseTargets( self, attacker );
	if ( self->noise_index ) {
		G_Sound( self, CHAN_AUTO, self->noise_index );
	}
	self->takedamage = qfalse;
	self->pain       = NULL;
	self->nextthink  = level.time + FRAMETIME;
	self->think      = G_FreeEntity;
}

void SP_q1_trigger_once( gentity_t *ent ) {
	int sounds;

	G_SpawnInt( "sounds", "0", &sounds );
	switch ( sounds ) {
	case 1: ent->noise_index = G_SoundIndex( "sound/misc/secret.opus"  ); break;
	case 2: ent->noise_index = G_SoundIndex( "sound/misc/talk.opus"    ); break;
	case 3: ent->noise_index = G_SoundIndex( "sound/misc/trigger1.opus"); break;
	default: break;
	}

	if ( ent->health ) {
		/* Q1 shoot-to-activate: fires on any hit, then removes itself.
		 * Touch is mutually exclusive — Q1 ref: triggers.qc:131-148. */
		ent->takedamage = qtrue;
		ent->pain       = q1_trigger_once_pain;
		ent->health     = 10000;   /* sentinel; original value irrelevant */
	} else {
		ent->touch = q1_trigger_once_touch;
	}

	Q3_InitTrigger( ent );
	if ( ent->pain ) {
		/* Shootable: override CONTENTS_TRIGGER (set by Q3_InitTrigger) to CONTENTS_CORPSE.
		 * CONTENTS_CORPSE is in MASK_SHOT but not MASK_PLAYERSOLID — bullets hit it,
		 * player walks through. Q1 ref: triggers.qc:138 — SOLID_BBOX, not a trigger vol. */
		ent->r.contents = CONTENTS_CORPSE;
	}
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_counter

Fires targets after being touched `count` times (default 2).
Removes itself after firing.
===================
*/
/* Q1 counter_use is a use-activated entity, not a touch volume.
   It is always targeted by buttons/triggers — never directly walked through. */
static void q1_trigger_counter_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	self->count--;

	if ( self->count > 0 ) {
		if ( !( self->spawnflags & COUNTER_NOMESSAGE ) && self->count < 4 ) {
			trap_SendServerCommand( -1, va( "cp \"%d more to go\n\"", self->count ) );
		}
		return;
	}

	if ( !( self->spawnflags & COUNTER_NOMESSAGE ) ) {
		trap_SendServerCommand( -1, "cp \"Sequence complete!\n\"" );
	}

	G_UseTargets( self, activator );

	if ( self->spawnflags & COUNTER_LOOPS ) {
		/* Reset to original count (stored in health at spawn) */
		self->count = self->health;
	} else {
		self->use      = NULL;
		self->nextthink = level.time + FRAMETIME;
		self->think    = G_FreeEntity;
	}
}

void SP_q1_trigger_counter( gentity_t *ent ) {
	G_SpawnInt( "count", "2", &ent->count );

	/* Store original count for COUNTER_LOOPS reset */
	ent->health = ent->count;

	/* counter_use is activated by other entities via G_UseTargets — no spatial volume */
	ent->use = q1_trigger_counter_use;
}


/*
===================
SP_q1_trigger_secret

One-shot secret trigger: plays secret sound and fires targets.
===================
*/
static void q1_trigger_secret_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) {
		return;
	}

	level.q1_found_secrets++;

	G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/misc/secret.opus" ) );

	/* Broadcast "secret found" to all clients */
	trap_SendServerCommand( -1, "cp \"You found a secret area!\n\"" );

	G_UseTargets( self, other );

	Com_Log( SEV_INFO, LOG_CH(ch_game), "Secret [%d/%d]\n",
	         level.q1_found_secrets, level.q1_total_secrets );

	self->touch    = NULL;
	self->nextthink = level.time + FRAMETIME;
	self->think    = G_FreeEntity;
}

void SP_q1_trigger_secret( gentity_t *ent ) {
	ent->touch = q1_trigger_secret_touch;

	Q3_InitTrigger( ent );
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_changelevel

Changes the map when touched.
"map" key specifies the destination map name.
===================
*/
static void q1_trigger_changelevel_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) {
		return;
	}

	self->touch    = NULL;
	self->nextthink = level.time + FRAMETIME;
	self->think    = G_FreeEntity;

	trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", self->target ) );
}

void SP_q1_trigger_changelevel( gentity_t *ent ) {
	char *mapName;

	G_SpawnString( "map", "", &mapName );
	ent->target = G_NewString( mapName );

	ent->touch = q1_trigger_changelevel_touch;

	Q3_InitTrigger( ent );
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_relay

Simple relay — fires targets when used. No touch.
Common in Q1 maps for chained activation sequences.
===================
*/
static void q1_trigger_relay_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	G_UseTargets( self, activator );
}

void SP_q1_trigger_relay( gentity_t *ent ) {
	ent->use = q1_trigger_relay_use;
}


/*
===================
SP_q1_trigger_push  (rerelease additions)

Adds ADDITIVE_PUSH (2) and START_OFF (4) spawnflags.
ADDITIVE_PUSH: per-tick velocity accumulation instead of replace (uses BG_TouchJumpPad via Q3 path;
for additive, we track in a direct velocity add via a custom touch).
START_OFF: disabled until fired via trigger_push_use.
===================
*/
static void q1_trigger_push_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( self->r.linked ) {
		trap_UnlinkEntity( self );
	} else {
		trap_LinkEntity( self );
	}
}

static void q1_trigger_push_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	vec3_t push;
	float  speed;

	if ( !other->client ) return;

	if ( self->spawnflags & Q1_PUSH_ADDITIVE ) {
		/* Additive push: accumulate velocity per tick */
		speed = self->speed ? self->speed : 300.0f;
		VectorScale( self->movedir, speed, push );
		/* Clear FL_ONGROUND for monsters */
		if ( other->flags & FL_Q1MONSTER )
			other->flags &= ~FL_Q1MONSTER;  /* FL_ONGROUND not tracked same as Q1 */
		VectorAdd( other->client->ps.velocity, push, other->client->ps.velocity );
	} else {
		/* Standard push: use Q3 jump pad mechanism */
		BG_TouchJumpPad( &other->client->ps, &self->s );
	}

	if ( self->spawnflags & Q1_PUSH_ONCE ) {
		self->touch    = NULL;
		self->nextthink = level.time + FRAMETIME;
		self->think    = G_FreeEntity;
	}
}

void SP_q1_trigger_push( gentity_t *ent ) {
	void Q3_AimAtTarget( gentity_t *self );

	Q3_InitTrigger( ent );

	ent->r.svFlags &= ~SVF_NOCLIENT;
	G_SoundIndex( "sound/world/jumppad.opus" );

	if ( ent->spawnflags & Q1_PUSH_ADDITIVE ) {
		/* Additive push: custom touch with direct velocity add */
		ent->s.eType = ET_GENERAL;
		ent->touch   = q1_trigger_push_touch;
		ent->use     = q1_trigger_push_use;
		if ( ent->spawnflags & Q1_PUSH_START_OFF )
			trap_UnlinkEntity( ent );
		else
			trap_LinkEntity( ent );
	} else {
		/* Standard Q3 jump pad path for non-additive push */
		ent->s.eType = ET_PUSH_TRIGGER;
		ent->touch   = q1_trigger_push_touch;
		ent->use     = q1_trigger_push_use;
		if ( ent->target && ent->target[0] ) {
			/* Target entity present: use Q3's ballistic trajectory calculator */
			ent->think     = Q3_AimAtTarget;
			ent->nextthink = level.time + FRAMETIME;
		} else {
			/* No target (vanilla Q1): pre-compute push velocity from movedir * speed
			   so BG_TouchJumpPad reads a valid origin2 and the trigger persists. */
			float speed = ent->speed > 0 ? ent->speed : 300.0f;
			VectorScale( ent->movedir, speed, ent->s.origin2 );
		}
		if ( ent->spawnflags & Q1_PUSH_START_OFF )
			trap_UnlinkEntity( ent );
		else
			trap_LinkEntity( ent );
	}
}


/*
===================
SP_q1_trigger_hurt  (rerelease addition: MONSTER_ONLY)

Spawnflags (rerelease layout): START_OFF=1, SILENT=4, MONSTER_ONLY=8, SLOW=16.
MONSTER_ONLY (8): only damage entities with FL_Q1MONSTER flag.
===================
*/
static void q1_hurt_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->takedamage ) return;

	/* MONSTER_ONLY: skip non-monsters */
	if ( ( self->spawnflags & Q1_HURT_MONSTER_ONLY ) && !( other->flags & FL_Q1MONSTER ) )
		return;

	if ( self->timestamp > level.time ) return;

	{
		int interval;
		if ( self->spawnflags & Q1_HURT_SLOW ) {
			interval = 1000;   /* SLOW forces 1 s regardless of wait */
		} else {
			interval = (int)( self->wait * 1000.0f );
			if ( interval < FRAMETIME )
				interval = FRAMETIME;   /* floor at one tick */
		}
		self->timestamp = level.time + interval;
	}

	if ( !( self->spawnflags & Q1_HURT_SILENT ) )
		G_Sound( other, CHAN_AUTO, self->noise_index );

	G_Damage( other, self, self, NULL, NULL, self->damage, 0, MOD_TRIGGER_HURT );
}

static void q1_hurt_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( self->r.linked )
		trap_UnlinkEntity( self );
	else
		trap_LinkEntity( self );
}

void SP_q1_trigger_hurt( gentity_t *ent ) {
	Q3_InitTrigger( ent );

	ent->noise_index = G_SoundIndex( "sound/world/electro.opus" );
	ent->touch = q1_hurt_touch;

	if ( !ent->damage ) ent->damage = 5;
	G_SpawnFloat( "wait", "1", &ent->wait );   /* damage rate; rerelease triggers.qc:747-748 */

	ent->use = q1_hurt_use;

	if ( ent->spawnflags & Q1_HURT_START_OFF )
		trap_UnlinkEntity( ent );
	else
		trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_counter_timed

Counts activations within a `wait`-second window.
Fires targets when count is reached within window; resets if window expires.
===================
*/
static void q1_trigger_counter_timed_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	/* Check if window has expired */
	if ( self->timestamp > 0 && level.time > self->timestamp ) {
		/* Window expired: reset */
		self->count    = self->health;
		self->timestamp = 0;
	}

	if ( self->timestamp == 0 ) {
		/* First activation: open window */
		self->timestamp = level.time + (int)( self->wait * 1000.0f );
	}

	self->count--;

	if ( self->count > 0 ) return;

	/* Count reached within window: fire */
	G_UseTargets( self, activator );
	self->count    = self->health;
	self->timestamp = 0;
}

void SP_q1_trigger_counter_timed( gentity_t *ent ) {
	G_SpawnInt(   "count", "2", &ent->count );
	G_SpawnFloat( "wait",  "2", &ent->wait  );

	ent->health    = ent->count;
	ent->timestamp = 0;
	ent->use       = q1_trigger_counter_timed_use;
}


/*
===================
SP_q1_trigger_multiple

Repeating touch trigger.  Re-arms after `wait` seconds (default 0.2).
Spawnflags: NOTOUCH=1 (use-activated only), SILENT=4.
Player-only is always enforced unconditionally (if (!other->client) return).
Sounds: 1=secret.opus, 2=talk.opus, 3=trigger1.opus.
===================
*/
static void q1_trigger_multiple_rearm( gentity_t *self ) {
	self->nextthink = 0;
	if ( self->pain ) {
		/* Shootable trigger: re-arm so the next hit fires again. */
		self->takedamage = qtrue;
	}
}

static void q1_trigger_multiple_fire( gentity_t *self, gentity_t *activator ) {
	if ( self->nextthink ) return;   /* waiting to re-arm */
	G_UseTargets( self, activator );
	if ( self->wait > 0 ) {
		self->nextthink = level.time + (int)( self->wait * 1000.0f );
		self->think     = q1_trigger_multiple_rearm;
	} else {
		self->touch     = NULL;
		self->use       = NULL;
		self->nextthink = level.time + FRAMETIME;
		self->think     = G_FreeEntity;
	}
}

static void q1_trigger_multiple_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) return;   /* PLAYER_ONLY is implicit: only clients reach here */
	if ( !VectorCompare( self->movedir, vec3_origin ) ) {
		vec3_t forward;
		AngleVectors( other->client->ps.viewangles, forward, NULL, NULL );
		if ( DotProduct( forward, self->movedir ) < 0 ) return;
	}
	if ( self->noise_index && !( self->spawnflags & Q1_TMUL_SILENT ) ) {
		G_Sound( self, CHAN_AUTO, self->noise_index );
	}
	q1_trigger_multiple_fire( self, other );
}

static void q1_trigger_multiple_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	q1_trigger_multiple_fire( self, activator );
}

static void q1_trigger_multiple_pain( gentity_t *self, gentity_t *attacker, int damage ) {
	/* Q1 trigger_multiple with health > 0: shoot-to-activate.
	 * Any hit fires targets; re-arms after wait via q1_trigger_multiple_rearm.
	 * Q1 ref: triggers.qc:131-148 (th_die=multi_killed, DAMAGE_YES, no touch). */
	if ( self->nextthink ) {
		return;   /* already in wait/re-arm cycle */
	}
	self->health     = 10000;  /* restore sentinel — G_Damage subtracted from it */
	self->takedamage = qfalse; /* lock out until rearm */
	if ( self->noise_index && !( self->spawnflags & Q1_TMUL_SILENT ) ) {
		G_Sound( self, CHAN_AUTO, self->noise_index );
	}
	q1_trigger_multiple_fire( self, attacker );
}

void SP_q1_trigger_multiple( gentity_t *ent ) {
	int   sounds;
	float wait;

	G_SpawnInt(   "sounds", "0",   &sounds );
	G_SpawnFloat( "wait",   "0.2", &wait   );
	ent->wait = wait;

	switch ( sounds ) {
	case 1: ent->noise_index = G_SoundIndex( "sound/misc/secret.opus"  ); break;
	case 2: ent->noise_index = G_SoundIndex( "sound/misc/talk.opus"    ); break;
	case 3: ent->noise_index = G_SoundIndex( "sound/misc/trigger1.opus"); break;
	default: break;
	}

	ent->use = q1_trigger_multiple_use;

	if ( ent->health ) {
		/* Q1 shoot-to-activate: touch and shootable are mutually exclusive.
		 * Q1 ref: triggers.qc:131-148 — health branch skips touch installation. */
		ent->takedamage = qtrue;
		ent->pain       = q1_trigger_multiple_pain;
		ent->health     = 10000;   /* sentinel; original health value irrelevant */
		/* do NOT install touch handler */
	} else {
		if ( !( ent->spawnflags & Q1_TMUL_NOTOUCH ) ) {
			ent->touch = q1_trigger_multiple_touch;
		}
	}

	Q3_InitTrigger( ent );
	if ( ent->pain ) {
		/* Shootable: override CONTENTS_TRIGGER to CONTENTS_CORPSE.
		 * CONTENTS_CORPSE is in MASK_SHOT but not MASK_PLAYERSOLID — bullets hit it,
		 * player walks through. Q1 ref: triggers.qc:138 — SOLID_BBOX, not a trigger vol. */
		ent->r.contents = CONTENTS_CORPSE;
	}
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_teleport

Touch-activated teleport.  Finds `info_teleport_destination` by targetname match.
Spawnflags: PLAYER_ONLY=1, SILENT=2, IGNORE_TARGETNAME=4 (find any destination).
200 ms debounce via `timestamp` prevents re-trigger during telefrag window.
===================
*/
static void q1_trigger_teleport_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	gentity_t *dest;

	if ( !other->client ) return;
	if ( other->client->ps.pm_type == PM_DEAD ) return;
	if ( self->timestamp > level.time ) return;   /* debounce */

	if ( self->spawnflags & Q1_TELE_IGNORE_TNAME ) {
		dest = G_Find( NULL, FOFS(classname), "q1_info_teleport_destination" );
	} else {
		dest = G_Find( NULL, FOFS(targetname), self->target );
	}
	if ( !dest ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "q1_trigger_teleport: no destination for target '%s'\n",
		         self->target ? self->target : "<null>" );
		return;
	}

	if ( !( self->spawnflags & Q1_TELE_SILENT ) ) {
		G_Sound( other, CHAN_AUTO, G_SoundIndex( "sound/misc/tele1.opus" ) );
	}

	TeleportPlayer( other, dest->s.origin, dest->movedir, (int)self->speed );
	self->timestamp = level.time + 200;
}

void SP_q1_trigger_teleport( gentity_t *ent ) {
	G_SpawnFloat( "speed", "300", &ent->speed );
	if ( ent->speed <= 0 ) ent->speed = 300.0f;

	ent->touch = q1_trigger_teleport_touch;

	Q3_InitTrigger( ent );
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_setskill

Sets the game skill cvar when touched (rerelease start-map difficulty selector).
Reads the `message` field for the skill value (e.g., "0", "1", "2").
One-shot; removes self after first touch.
===================
*/
static void q1_trigger_setskill_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) return;
	if ( self->message ) {
		trap_Cvar_Set( "g_skill", self->message );
	}
	self->touch     = NULL;
	self->nextthink = level.time + FRAMETIME;
	self->think     = G_FreeEntity;
}

void SP_q1_trigger_setskill( gentity_t *ent ) {
	ent->touch = q1_trigger_setskill_touch;

	Q3_InitTrigger( ent );
	trap_LinkEntity( ent );
}


/*
===================
SP_q1_trigger_monsterjump

Monster jump pad — stub pending monster AI implementation.
Registers a spatial volume so the spawn loop does not warn about a missing classname.
===================
*/
void SP_q1_trigger_monsterjump( gentity_t *ent ) {
	Q3_InitTrigger( ent );
	trap_LinkEntity( ent );
}


/*
===================
q1_trigger_onlyregistered_touch / SP_q1_trigger_onlyregistered

Fires targets only if the "registered" cvar is non-zero.
In q3now (non-registered), shows the centerprint message and plays a
talk sound instead, mirroring Q1 vanilla behaviour for unregistered copies.

Q1 ref: rerelease triggers.qc:546–580; FTEQW triggers.qc:491–523.
===================
*/
static void q1_trigger_onlyregistered_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) return;

	if ( self->pain_debounce_time > level.time ) return;
	self->pain_debounce_time = level.time + 2000;

	if ( trap_Cvar_VariableIntegerValue( "registered" ) ) {
		/* Registered: clear message, fire targets, remove self */
		self->message = NULL;
		G_UseTargets( self, other );
		G_FreeEntity( self );
	} else {
		/* Non-registered: show centerprint + talk sound (Q1 "shareware nag") */
		if ( self->message && self->message[0] ) {
			trap_SendServerCommand( other - g_entities,
			                        va( "cp \"%s\"", self->message ) );
		}
		G_Sound( other, CHAN_AUTO, G_SoundIndex( "sound/misc/talk.opus" ) );
	}
}

void SP_q1_trigger_onlyregistered( gentity_t *ent ) {
	G_SoundIndex( "sound/misc/talk.opus" );   /* precache */
	ent->touch = q1_trigger_onlyregistered_touch;
	Q3_InitTrigger( ent );
	trap_LinkEntity( ent );
}
