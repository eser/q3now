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

// Q1 mover entities (func_door, func_plat)

#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

#define DOOR_START_OPEN   1   /* Q1 spawnflag: spawn at open position */
#define DOOR_DONT_LINK    4   /* Q1 spawnflag: do not link adjacent doors into team */
#define DOOR_GOLD_KEY     8   /* Q1 spawnflag: requires gold key to open */
#define DOOR_SILVER_KEY   16  /* Q1 spawnflag: requires silver key to open */
#define DOOR_TOGGLE       32  /* Q1 spawnflag: stays open until triggered again */
#define DOOR_CRUSHER      64  /* Q1/RR spawnflag: door crushes, does not reverse */

/* func_button spawnflags (rerelease additions) */
#define BUTTON_KEY_ALWAYS_REQUIRED  32  /* RR: key check fires every press (same bit as DOOR_TOGGLE; different entity) */

/* func_door_secret spawnflags (doors.qc fd_secret_spawn) */
#define SECRET_OPEN_ONCE   1   /* stays open after first use */
#define SECRET_1ST_LEFT    2   /* first stage moves left instead of right */
#define SECRET_1ST_DOWN    4   /* first stage moves down (not implemented) */
#define SECRET_NO_SHOOT    8   /* only trigger-opened, not damage (not implemented) */
#define SECRET_YES_SHOOT   16  /* allow targeted door to be shoot-activated (overrides default trigger-only) */

// Functions from g_mover_q3.c used by Q1 movers
void Q3_InitMover( gentity_t *ent );
void Q3_Reached_BinaryMover( gentity_t *ent );
void Q3_Blocked_Door( gentity_t *ent, gentity_t *other );
void Q3_Think_SpawnNewDoorTrigger( gentity_t *ent );
void Q3_Use_BinaryMover( gentity_t *ent, gentity_t *other, gentity_t *activator );
void Q3_Touch_Plat( gentity_t *ent, gentity_t *other, trace_t *trace );
void Q3_Touch_Button( gentity_t *ent, gentity_t *other, trace_t *trace );
void Q3_SpawnPlatTrigger( gentity_t *ent );
void Q3_SetMoverState( gentity_t *ent, moverState_t moverState, int time );
void SP_q3_func_train( gentity_t *ent );
void SP_q3_func_rotating( gentity_t *ent );

/* Forward declaration — Q1_Door_Use is defined below Q1_Door_Touch */
static void Q1_Door_Use( gentity_t *ent, gentity_t *other, gentity_t *activator );

/*
=================
Q1_Blocked_Door

Q1-specific door blocked handler.
When DOOR_CRUSHER (64) is set, deal damage and do NOT reverse.
Otherwise, deal damage and reverse direction.
=================
*/
static void Q1_Blocked_Door( gentity_t *ent, gentity_t *other ) {
	if ( !other->client ) {
		G_FreeEntity( other );
		return;
	}

	if ( ent->damage ) {
		G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH );
	}

	if ( ent->spawnflags & DOOR_CRUSHER ) {
		return;   /* crusher: do not reverse */
	}

	/* reverse direction */
	Q3_Use_BinaryMover( ent, ent, other );
}

/* Key label by worldtype */
static const char * Q1_KeyLabel( int worldtype, qboolean gold ) {
	switch ( worldtype ) {
	case 1: return gold ? "gold runekey"  : "silver runekey";
	case 2: return gold ? "gold keycard"  : "silver keycard";
	default: return gold ? "gold key" : "silver key";
	}
}

/*
=================
Q1_Door_Touch

Key-check touch handler.  Installed when DOOR_SILVER_KEY or DOOR_GOLD_KEY
spawnflag is set.  Checks player holdable bits and either denies (with
centerprint + debounce) or opens the door.
=================
*/
static void Q1_Door_Touch( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) return;

	if ( ent->spawnflags & DOOR_SILVER_KEY ) {
		if ( !( other->client->ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( HI_KEY_SILVER ) ) ) {
			if ( ent->pain_debounce_time > level.time ) return;
			ent->pain_debounce_time = level.time + 2000;
			trap_SendServerCommand( other - g_entities,
				va( "cp \"You need the %s\n\"",
				    Q1_KeyLabel( level.q1_worldtype, qfalse ) ) );
			if ( ent->soundPos1 )
				G_Sound( ent, CHAN_VOICE, ent->soundPos1 );
			return;
		}
	}

	if ( ent->spawnflags & DOOR_GOLD_KEY ) {
		if ( !( other->client->ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( HI_KEY_GOLD ) ) ) {
			if ( ent->pain_debounce_time > level.time ) return;
			ent->pain_debounce_time = level.time + 2000;
			trap_SendServerCommand( other - g_entities,
				va( "cp \"You need the %s\n\"",
				    Q1_KeyLabel( level.q1_worldtype, qtrue ) ) );
			if ( ent->soundPos1 )
				G_Sound( ent, CHAN_VOICE, ent->soundPos1 );
			return;
		}
	}

	/* player has the required key — open the door */
	Q1_Door_Use( ent, other, other );
}

/*
=================
SetMovedirQ1

Q1 angle convention: 0=east(+x), 90=north(+y), -1=up, -2=down.
=================
*/
static void SetMovedirQ1( float angle, vec3_t movedir ) {
	if ( angle == -1.0f ) {
		VectorSet( movedir, 0, 0, 1 );
	} else if ( angle == -2.0f ) {
		VectorSet( movedir, 0, 0, -1 );
	} else {
		float rad = DEG2RAD( angle );
		movedir[0] = cosf( rad );
		movedir[1] = sinf( rad );
		movedir[2] = 0;
	}
}

/*
=================
Q1_Door_Use

Routes a use event to the team master so Q3_MatchTeam propagates the open
to all linked leaves. Installing this on every leaf means Q3_Touch_DoorTrigger
(which calls ent->parent->use) correctly drives the whole team regardless
of which leaf's trigger was touched.
=================
*/
static void Q1_Door_Use( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gentity_t *master = ( ent->flags & FL_TEAMMEMBER ) ? ent->teammaster : ent;
	if ( !master ) master = ent;

	// MOVER_POS2 = door fully open. Q3_Touch_DoorTrigger only skips MOVER_1TO2,
	// so without this guard Q3_Use_BinaryMover gets called every server frame while
	// the player stands in the trigger. Q1 doors don't extend their close timer
	// on repeated touch — suppress those calls here.
	if ( master->moverState == MOVER_POS2 ) {
		return;
	}

	Q3_Use_BinaryMover( master, other, activator );
}

/*
=================
SP_q1_func_door

Q1 sliding door. Opens along the `angle` axis when touched.
Reuses the Q3 binary-mover state machine (MOVER_POS1/POS2/1TO2/2TO1).
=================
*/
void SP_q1_func_door( gentity_t *ent ) {
	float   angle, lip, wait, distance, speed2;
	vec3_t  abs_movedir, size;
	int     sounds;

	trap_SetBrushModel( ent, ent->model );
	ent->r.contents = CONTENTS_SOLID;

	/* Q1 defaults from doors.qc func_door_spawn:
	     speed 100  (doors.qc: "if (!self.speed) self.speed = 100")
	     wait  3    (doors.qc: "if (!self.wait)  self.wait  = 3")
	     lip   8    (doors.qc: "if (!self.lip)   self.lip   = 8")
	     dmg   2    (doors.qc: "if (!self.dmg) self.dmg = 2") */
	G_SpawnFloat( "angle",  "0",   &angle       );
	G_SpawnFloat( "lip",    "8",   &lip         );
	G_SpawnFloat( "speed",  "100", &ent->speed  );
	G_SpawnFloat( "speed2", "0",   &speed2      );   /* RR: separate close speed */
	G_SpawnFloat( "wait",   "3",   &wait        );
	G_SpawnInt(   "sounds", "0",   &sounds      );
	G_SpawnInt(   "dmg",    "2",   &ent->damage );

	/* Store speed2 in ent->physicsBounce (spare float not used by door logic) */
	ent->physicsBounce = speed2;

	/* wait < 0 means "stay open forever". Store -1 as sentinel; Q3_Reached_BinaryMover
	   checks wait < 0 and skips scheduling Q3_ReturnToPos1. */
	ent->wait = ( wait < 0 ) ? -1.0f : wait * 1000.0f;

	/* DOOR_TOGGLE (32): door stays open until triggered again (wait = -1 sentinel).
	   Q1 ref: doors.qc:75 "if (self.spawnflags & DOOR_TOGGLE) return;" in door_fire,
	   meaning the door never schedules auto-close. Q3_Reached_BinaryMover skips
	   scheduling Q3_ReturnToPos1 when wait < 0. */
	if ( ent->spawnflags & DOOR_TOGGLE ) ent->wait = -1.0f;

	/* Q1 door sound sets from doors.qc.
	   noise2 = movement-start (both directions) → sound1to2 / sound2to1
	   noise1 = arrival (both positions)         → soundPos1 / soundPos2 */
	switch ( sounds ) {
	case 1:   /* medieval: doormv1 moving, drclos4 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/doormv1.opus" );
		ent->soundPos1 = ent->soundPos2 = G_SoundIndex( "sound/doors/drclos4.opus" );
		break;
	case 2:   /* hydraulic: hydro1 moving, hydro2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/hydro1.opus" );
		ent->soundPos1 = ent->soundPos2 = G_SoundIndex( "sound/doors/hydro2.opus" );
		break;
	case 3:   /* stone: stndr1 moving, stndr2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/stndr1.opus" );
		ent->soundPos1 = ent->soundPos2 = G_SoundIndex( "sound/doors/stndr2.opus" );
		break;
	case 4:   /* base: ddoor1 moving, ddoor2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/ddoor1.opus" );
		ent->soundPos1 = ent->soundPos2 = G_SoundIndex( "sound/doors/ddoor2.opus" );
		break;
	default:
		break;   /* sounds 0: silent */
	}

	SetMovedirQ1( angle, ent->movedir );

	abs_movedir[0] = fabsf( ent->movedir[0] );
	abs_movedir[1] = fabsf( ent->movedir[1] );
	abs_movedir[2] = fabsf( ent->movedir[2] );
	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	distance = DotProduct( abs_movedir, size ) - lip;

	VectorCopy( ent->s.origin, ent->pos1 );
	VectorMA( ent->pos1, distance, ent->movedir, ent->pos2 );

	if ( ent->spawnflags & DOOR_START_OPEN ) {
		vec3_t temp;
		VectorCopy( ent->pos2, temp );
		VectorCopy( ent->s.origin, ent->pos2 );
		VectorCopy( temp, ent->pos1 );
	}

	ent->blocked = Q1_Blocked_Door;

	Q3_InitMover( ent );
	ent->use = Q1_Door_Use;

	/* Install key-check touch if door requires a key */
	if ( ent->spawnflags & ( DOOR_SILVER_KEY | DOOR_GOLD_KEY ) ) {
		ent->touch = Q1_Door_Touch;
	}

	if ( !ent->targetname || !ent->targetname[0] ) {
		/* Q1 canonical: only un-targeted doors get an auto-spawned proximity trigger.
		 * Targeted doors rely on Q1_Door_Use fired by target chains (set unconditionally above).
		 */
		ent->nextthink = level.time + FRAMETIME;
		ent->think     = Q3_Think_SpawnNewDoorTrigger;
	}
}

/* forward decl — defined after Q1_SecretDoor_StartClose */
static void Q1_SecretDoor_Use( gentity_t *ent, gentity_t *other, gentity_t *activator );

/*
=================
Q1_SecretDoor_Move

Phase movement helper.  Saves/restores the permanent pos1/pos2 values
around the Q3_SetMoverState call (which only reads them once).  Also
recomputes s.pos.trDuration per-phase so speed is uniform across phases.

Permanent slot layout:
  pos1      = fully-closed (spawn origin)
  s.origin2 = intermediate after phase 1 (sideways only)
  pos2      = fully-open after phase 2 (sideways + forward)
=================
*/
static void Q1_SecretDoor_Move( gentity_t *ent, const vec3_t from, const vec3_t to ) {
	vec3_t p1, p2, delta;
	float  len;
	VectorCopy( ent->pos1, p1 );
	VectorCopy( ent->pos2, p2 );
	VectorCopy( from, ent->pos1 );
	VectorCopy( to,   ent->pos2 );
	VectorSubtract( to, from, delta );
	len = VectorLength( delta );
	ent->s.pos.trDuration = ( ent->speed > 0 ) ? (int)( len * 1000.0f / ent->speed ) : 1;
	if ( ent->s.pos.trDuration < 1 ) ent->s.pos.trDuration = 1;
	Q3_SetMoverState( ent, MOVER_1TO2, level.time );
	VectorCopy( p1, ent->pos1 );
	VectorCopy( p2, ent->pos2 );
}

/*
=================
Q1_SecretDoor_StartClose

Think callback: fired after `wait` ms to begin the 2-phase close sequence.
Phase 3 moves open → intermediate; phase 4 moves intermediate → closed.
=================
*/
static void Q1_SecretDoor_StartClose( gentity_t *ent ) {
	if ( ent->count != 3 ) return;
	if ( ent->sound2to1 )
		G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound2to1 );
	ent->count = 4;
	Q1_SecretDoor_Move( ent, ent->pos2, ent->s.origin2 );   /* open → mid */
}

/*
=================
Q1_DoorSecret_StartPhase2 / Q1_DoorSecret_StartPhase5

Think callbacks fired after the 1-second inter-phase pause.
Matches Q1 fd_secret_move1 (open pause) and its closing counterpart.
=================
*/
static void Q1_DoorSecret_StartPhase2( gentity_t *ent ) {
	ent->count = 2;
	Q1_SecretDoor_Move( ent, ent->s.origin2, ent->pos2 );   /* mid → open */
}

static void Q1_DoorSecret_StartPhase5( gentity_t *ent ) {
	ent->count = 5;
	Q1_SecretDoor_Move( ent, ent->s.origin2, ent->pos1 );   /* mid → closed */
}

/*
=================
Q1_DoorSecret_Reached

4-phase open/close sequence.  count encoding:
  1 = phase 1 (closed→mid) in progress  → arrival: pause 1 s then start phase 2
  2 = phase 2 (mid→open) in progress    → arrival: door fully open
  3 = open/idle (waiting to close)
  4 = phase 3 (open→mid) in progress    → arrival: pause 1 s then start phase 5
  5 = phase 4 (mid→closed) in progress  → arrival: door fully closed
=================
*/
static void Q1_DoorSecret_Reached( gentity_t *ent ) {
	/* DIAG Bug 3 — remove after fix */
	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "q1_func_door_secret reached: id=%i count=%i moverState=%i origin=(%.1f %.1f %.1f) "
	         "absmin=(%.1f %.1f %.1f) absmax=(%.1f %.1f %.1f) time=%i\n",
	         ent->s.number, ent->count, ent->moverState,
	         ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
	         ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2],
	         ent->r.absmax[0], ent->r.absmax[1], ent->r.absmax[2],
	         level.time );
	switch ( ent->count ) {
	case 1:   /* sideways phase done → 1 s pause before forward phase (Q1 fd_secret_move1) */
		/* Stop the mover loop: TR_LINEAR_STOP keeps firing reached every frame,
		   which would push nextthink forward indefinitely and prevent phase 2 from starting.
		   Set TR_STATIONARY so G_MoverTeam does not call reached again until phase 2 begins. */
		ent->s.pos.trType = TR_STATIONARY;
		VectorCopy( ent->s.origin2, ent->s.pos.trBase );   /* rest at mid position */
		VectorCopy( ent->s.origin2, ent->r.currentOrigin );
		trap_LinkEntity( ent );
		ent->nextthink = level.time + 1000;
		ent->think     = Q1_DoorSecret_StartPhase2;
		break;
	case 2:   /* forward phase done → fully open */
		/* Use Q3_SetMoverState to mark stationary at pos2 — stops mover loop. */
		Q3_SetMoverState( ent, MOVER_POS2, level.time );
		if ( ent->soundPos2 )
			G_AddEvent( ent, EV_GENERAL_SOUND, ent->soundPos2 );
		ent->count = 3;
		if ( ent->wait >= 0 ) {
			ent->nextthink = level.time + (int)ent->wait;
			ent->think     = Q1_SecretDoor_StartClose;
		}
		if ( ent->spawnflags & SECRET_OPEN_ONCE ) {
			ent->use   = NULL;
			ent->touch = NULL;
		}
		break;
	case 4:   /* backward phase done → 1 s pause before return phase (symmetric with open) */
		/* Same fix as case 1: stop mover loop so nextthink is not pushed out every frame. */
		ent->s.pos.trType = TR_STATIONARY;
		VectorCopy( ent->s.origin2, ent->s.pos.trBase );   /* rest at mid position */
		VectorCopy( ent->s.origin2, ent->r.currentOrigin );
		trap_LinkEntity( ent );
		ent->nextthink = level.time + 1000;
		ent->think     = Q1_DoorSecret_StartPhase5;
		break;
	case 5:   /* return phase done → fully closed */
		/* Use Q3_SetMoverState to mark stationary at pos1 — stops mover loop. */
		Q3_SetMoverState( ent, MOVER_POS1, level.time );
		if ( ent->soundPos1 )
			G_AddEvent( ent, EV_GENERAL_SOUND, ent->soundPos1 );
		ent->count = 0;
		/* Re-arm shootable state using the same condition as spawn:
		   non-targeted doors and YES_SHOOT targeted doors get pain re-enabled.
		   Q1 ref: fd_secret_use resets "self.health = 10000" on each trigger call. */
		{
			qboolean canShoot = ( !ent->targetname || !ent->targetname[0] ) ||
			                    ( ent->spawnflags & SECRET_YES_SHOOT );
			if ( !( ent->spawnflags & SECRET_NO_SHOOT ) && canShoot ) {
				ent->takedamage = qtrue;
				ent->health     = 10000;
			}
		}
		break;
	}
}

/*
=================
Q1_DoorSecret_Die

Safety-net die callback: fires if something reduces health to ≤ 0 despite the
10000-sentinel. Normal operation routes through Q1_DoorSecret_Pain (any-damage).
=================
*/
static void Q1_DoorSecret_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	if ( self->use ) self->use( self, attacker, attacker );
}

/*
=================
Q1_DoorSecret_Pain

Pain (any-damage) callback for shoot-to-activate secret doors.
Mirrors Q1 fd_secret_use which is installed as th_pain — fires on the first damage
hit regardless of amount, since health is kept as a 10000 sentinel (never depleted).

Q1 ref: doors.qc fd_secret_use:
  "self.health = 10000"   ← re-arm after each trigger call
  "if (self.origin != self.oldorigin) return"  ← ignore if already moving
=================
*/
static void Q1_DoorSecret_Pain( gentity_t *self, gentity_t *attacker, int damage ) {
	self->health = 10000;   /* re-arm sentinel: pain must stay callable after this returns */
	if ( self->use ) {
		self->use( self, attacker, attacker );   /* Q1_SecretDoor_Use guards count != 0 */
	}
}

/*
=================
Q1_SecretDoor_Touch

Direct player-contact handler for func_door_secret.
Mirrors Q1 doors.qc fd_secret_touch: fires Use on the door when a
player physically walks into the brush panel.  A 2-second debounce
(`pain_debounce_time`) prevents rapid re-fire while standing in contact.

Only installed when SECRET_OPEN_ONCE is NOT set — a one-shot secret door
opens via trigger/damage only and never re-triggers on touch.
=================
*/
static void Q1_SecretDoor_Touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) return;
	if ( self->targetname && self->targetname[0] ) return;
	if ( self->pain_debounce_time > level.time ) return;
	self->pain_debounce_time = level.time + 2000;
	if ( self->use ) self->use( self, other, other );
}

/*
=================
Q1_SecretDoor_Blocked

Q1 fd_secret_blocked: damage the blocker every 0.5 s but never reverse.
Reversing a secret door mid-phase would corrupt the 4-phase state machine
(Q1_DoorSecret_Reached would fire at the wrong count for the wrong position).
Generic Q3_Blocked_Door calls Q3_Use_BinaryMover to reverse — wrong for secret doors.
=================
*/
static void Q1_SecretDoor_Blocked( gentity_t *ent, gentity_t *other ) {
	if ( !other->client ) {
		G_FreeEntity( other );
		return;
	}
	/* fly_sound_debounce_time is unused by doors; repurpose as damage-rate limiter (Q1: 0.5 s) */
	if ( ent->fly_sound_debounce_time > level.time ) return;
	ent->fly_sound_debounce_time = level.time + 500;
	if ( ent->damage ) {
		G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH );
	}
	/* no reversal — door continues moving through the blocker (Q1 behavior) */
}

/*
=================
Q1_SecretDoor_Use

Use handler.  Only triggers when door is fully closed (count == 0).
Starts the 2-phase opening sequence: sideways first, then forward.
=================
*/
static void Q1_SecretDoor_Use( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if ( ent->teammaster && ent->teammaster != ent ) {
		Q1_SecretDoor_Use( ent->teammaster, other, activator );
		return;
	}
	if ( ent->count != 0 ) return;
	ent->activator = activator;
	G_UseTargets( ent, activator );
	if ( ent->sound1to2 )
		G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound1to2 );
	ent->count = 1;
	Q1_SecretDoor_Move( ent, ent->pos1, ent->s.origin2 );   /* closed → mid */
}

/*
=================
SP_q1_func_door_secret

Q1 hidden wall panel — 2-phase motion matching original Q1 behaviour:
  Phase 1: slide sideways (perpendicular to angle) by t_width
  Phase 2: slide backward (along angle) by t_length
Closing is the same two phases in reverse.

Sound sets from doors.qc fd_secret_spawn (lines 778–803):
  1 = medieval: noise2=winch2 (move), noise1=latch2 (arrival)
  2 = metal:    noise2=airdoor1 (move), noise1=airdoor2 (arrival)
  3 = base:     noise2=basesec1 (move), noise1=basesec2 (arrival)
Sounds 0 defaults to 3 per Q1 source.
=================
*/
void SP_q1_func_door_secret( gentity_t *ent ) {
	float    angle, kw, kl, t_width, t_length, sideways_sign;
	int      sounds;
	vec3_t   angles_vec, forward, right, up;
	vec3_t   size;

	trap_SetBrushModel( ent, ent->model );
	ent->r.contents = CONTENTS_SOLID;

	G_SpawnFloat( "angle",  "0", &angle );
	G_SpawnInt(   "dmg",    "2", &ent->damage );
	G_SpawnInt(   "sounds", "0", &sounds );
	if ( sounds == 0 ) sounds = 3;   /* Q1: fd_secret_spawn defaults to base */

	if ( !ent->speed ) ent->speed = 50;
	if ( !ent->wait  ) ent->wait  = 5;
	ent->wait *= 1000;
	if ( ent->spawnflags & SECRET_OPEN_ONCE )
		ent->wait = -1;   /* stay open; binary mover skips auto-return on negative wait */

	/* Sound sets — noise2 = movement (sound1to2/sound2to1), noise1 = arrival (soundPos) */
	switch ( sounds ) {
	case 1:   /* medieval: winch2 moving, latch2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/winch2.opus" );
		ent->soundPos1 = ent->soundPos2  = G_SoundIndex( "sound/doors/latch2.opus" );
		break;
	case 2:   /* metal: airdoor1 moving, airdoor2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/airdoor1.opus" );
		ent->soundPos1 = ent->soundPos2  = G_SoundIndex( "sound/doors/airdoor2.opus" );
		break;
	case 3:   /* base: basesec1 moving, basesec2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/doors/basesec1.opus" );
		ent->soundPos1 = ent->soundPos2  = G_SoundIndex( "sound/doors/basesec2.opus" );
		break;
	default:
		break;   /* silent */
	}

	/* Direction computation mirrors fd_secret_use in doors.qc:
	     1ST_DOWN: t_width = fabs(v_up · size), dest1 = origin - v_up * t_width
	     default:  t_width = fabs(v_right · size), dest1 = origin + v_right * (t_width * ±1)
	     dest2 = dest1 + v_forward * t_length  (both phases collapsed into single target) */
	VectorSet( angles_vec, 0, angle, 0 );
	AngleVectors( angles_vec, forward, right, up );

	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	if ( ent->spawnflags & SECRET_1ST_DOWN )
		t_width = fabsf( DotProduct( size, up ) );
	else
		t_width = fabsf( DotProduct( size, right ) );
	t_length = fabsf( DotProduct( size, forward ) );

	if ( G_SpawnFloat( "t_width",  "0", &kw ) && kw > 0 ) t_width  = kw;
	if ( G_SpawnFloat( "t_length", "0", &kl ) && kl > 0 ) t_length = kl;

	VectorCopy( ent->s.origin, ent->pos1 );
	VectorCopy( ent->pos1, ent->pos2 );
	if ( ent->spawnflags & SECRET_1ST_DOWN ) {
		VectorMA( ent->pos2, -t_width, up, ent->pos2 );
	} else {
		sideways_sign = ( ent->spawnflags & SECRET_1ST_LEFT ) ? -1.0f : 1.0f;
		VectorMA( ent->pos2, t_width * sideways_sign, right, ent->pos2 );
	}
	/* pos2 is now pos_mid (end of phase 1); cache it before adding the forward offset */
	VectorCopy( ent->pos2, ent->s.origin2 );
	VectorMA( ent->pos2, t_length, forward, ent->pos2 );
	/* pos2 is now pos_open (end of phase 2) */

	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "SP_q1_func_door_secret: model='%s' spawnflags=%d sounds=%d"
	         " angle=%.0f t_width=%.0f t_length=%.0f"
	         " pos1=(%.0f,%.0f,%.0f) pos2=(%.0f,%.0f,%.0f)\n",
	         ent->model ? ent->model : "<null>",
	         ent->spawnflags, sounds, angle, t_width, t_length,
	         ent->pos1[0], ent->pos1[1], ent->pos1[2],
	         ent->pos2[0], ent->pos2[1], ent->pos2[2] );

	ent->blocked = Q1_SecretDoor_Blocked;  /* damage only, no reversal — preserves 4-phase state machine */

	Q3_InitMover( ent );
	ent->reached = Q1_DoorSecret_Reached;
	ent->use     = Q1_SecretDoor_Use;   /* override Q3_Use_BinaryMover from Q3_InitMover */

	/* Suppress the proximity walk-in trigger — secret doors must not open
	   just by standing near them.  They open on direct touch, targeted use,
	   or damage (see below). */
	ent->nextthink = 0;
	ent->think     = NULL;

	/* Q1 canonical (FTEQW doors.qc func_door_secret):
	   "if (!self.targetname || self.spawnflags & SECRET_YES_SHOOT) { health=10000; th_pain=fd_secret_use; }"
	   Non-targeted doors are always shootable; targeted doors require YES_SHOOT to opt in.
	   SECRET_NO_SHOOT overrides both — trigger-only regardless of targetname. */
	{
		qboolean canShoot = ( !ent->targetname || !ent->targetname[0] ) ||
		                    ( ent->spawnflags & SECRET_YES_SHOOT );
		if ( ( ent->spawnflags & SECRET_NO_SHOOT ) || !canShoot ) {
			ent->takedamage = qfalse;
			ent->pain       = NULL;
			ent->die        = NULL;
		} else {
			ent->takedamage = qtrue;
			ent->pain       = Q1_DoorSecret_Pain;   /* fires on any hit — Q1 th_pain semantics */
			ent->die        = Q1_DoorSecret_Die;     /* safety-net if health somehow reaches 0 */
			ent->health     = 10000;                 /* sentinel: pain always fires, die unreachable normally */
		}
	}

	/* Touch-to-open: mirrors Q1 fd_secret_touch.  Disabled by SECRET_OPEN_ONCE
	   because that flag means the door only opens once (trigger/damage only). */
	if ( !( ent->spawnflags & SECRET_OPEN_ONCE ) )
		ent->touch = Q1_SecretDoor_Touch;

	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "SP_q1_func_door_secret [post-Q3_InitMover]: model='%s'"
	         " s.origin=(%.0f,%.0f,%.0f)"
	         " r.currentOrigin=(%.0f,%.0f,%.0f)"
	         " pos1=(%.0f,%.0f,%.0f) pos2=(%.0f,%.0f,%.0f)"
	         " trBase=(%.0f,%.0f,%.0f) trType=%d"
	         " trDelta=(%.2f,%.2f,%.2f) s.solid=0x%x r.linked=%d\n",
	         ent->model ? ent->model : "<null>",
	         ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
	         ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
	         ent->pos1[0], ent->pos1[1], ent->pos1[2],
	         ent->pos2[0], ent->pos2[1], ent->pos2[2],
	         ent->s.pos.trBase[0], ent->s.pos.trBase[1], ent->s.pos.trBase[2],
	         (int)ent->s.pos.trType,
	         ent->s.pos.trDelta[0], ent->s.pos.trDelta[1], ent->s.pos.trDelta[2],
	         (unsigned)ent->s.solid, (int)ent->r.linked );
	/* DIAG Bug 3 — remove after fix */
	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "q1_func_door_secret spawn: id=%i origin=(%.1f %.1f %.1f) angle=%.1f "
	         "pos1=(%.1f %.1f %.1f) pos2=(%.1f %.1f %.1f) origin2=(%.1f %.1f %.1f) "
	         "absmin=(%.1f %.1f %.1f) absmax=(%.1f %.1f %.1f) "
	         "spawnflags=%i targetname=%s\n",
	         ent->s.number,
	         ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
	         ent->s.angles[YAW],
	         ent->pos1[0], ent->pos1[1], ent->pos1[2],
	         ent->pos2[0], ent->pos2[1], ent->pos2[2],
	         ent->s.origin2[0], ent->s.origin2[1], ent->s.origin2[2],
	         ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2],
	         ent->r.absmax[0], ent->r.absmax[1], ent->r.absmax[2],
	         ent->spawnflags,
	         ent->targetname ? ent->targetname : "(null)" );
}

/*
=================
Reached_Q1Plat

Q1 plat_hit_top / plat_hit_bottom both fire targets (SUB_UseTargets in QuakeC).
Q3_Reached_BinaryMover only fires G_UseTargets at pos2 (bottom). This wrapper
additionally fires targets when the plat arrives at pos1 (top), matching Q1 behavior.
=================
*/
static void Reached_Q1Plat( gentity_t *ent ) {
	/* With Q1 pos1=BOTTOM and pos2=TOP, arriving at TOP means moverState was MOVER_1TO2.
	   Q1's plat_hit_top fires SUB_UseTargets; we match that by firing targets here. */
	qboolean arrivedAtTop = ( ent->moverState == MOVER_1TO2 );
	Q3_Reached_BinaryMover( ent );
	if ( arrivedAtTop ) {
		/* DIAG Bug 3 — remove after fix */
		Com_Log( SEV_TRACE, LOG_CH(ch_game),
		         "q1_func_plat reached top: id=%i origin=(%.1f %.1f %.1f) time=%i\n",
		         ent->s.number,
		         ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
		         level.time );
		G_UseTargets( ent, ent->activator ? ent->activator : ent );
	}
}

/*
=================
SP_q1_func_plat

Q1 elevator platform. Spawns at bottom (pos1 = floor), rises to top (pos2)
when a player steps into the shaft trigger, then descends after `wait` ms.

pos1 = BOTTOM (where the plat starts and the trigger sits).
pos2 = TOP    (where the plat carries the player, as placed in the editor).
Q3_InitMover starts the entity at pos1; Q3_SpawnPlatTrigger places the trigger at pos1.
=================
*/
#define PLAT_LOW_TRIGGER  1   /* Q1 spawnflag: trigger sits at bottom (floor level) not at top */

/*
=================
Q1_Plat_TargetedUse

Q1 plat_use semantic: the targeted plat rests at TOP (pos2 in q3now convention),
waits for a trigger/button to descend once, then hands off to the shaft trigger.
One-shot: clears its own use handler after first activation.
=================
*/
static void Q1_Plat_TargetedUse( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	/* Only valid from rest at top; silently ignore if already moving or at bottom. */
	if ( ent->moverState != MOVER_POS2 ) {
		return;
	}
	Q3_Use_BinaryMover( ent, other, activator );   /* descend pos2 → pos1 */
	Q3_SpawnPlatTrigger( ent );                    /* shaft trigger handles future cycles */
	ent->use = NULL;                               /* one-shot complete */
}

void SP_q1_func_plat( gentity_t *ent ) {
	float lip, height;
	int   sounds;

	trap_SetBrushModel( ent, ent->model );
	ent->r.contents = CONTENTS_SOLID;
	VectorClear( ent->s.angles );

	G_SpawnFloat( "lip",    "8",  &lip    );
	G_SpawnInt(   "sounds", "0",  &sounds );
	G_SpawnInt(   "dmg",    "2",  &ent->damage );

	if ( !ent->speed ) ent->speed = 150;
	ent->wait = 3000;   /* 3 s dwell (matches Q1 canonical wait at top) */

	/* Q1 plat sound sets from plats.qc.
	   noise  = movement-start (both directions) → sound1to2 / sound2to1
	   noise1 = arrival (both positions)         → soundPos1 / soundPos2 */
	switch ( sounds ) {
	case 1:   /* plat1 moving, plat2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/plats/plat1.opus" );
		ent->soundPos1 = ent->soundPos2 = G_SoundIndex( "sound/plats/plat2.opus" );
		break;
	case 2:   /* medplat1 moving, medplat2 arrival */
		ent->sound1to2 = ent->sound2to1 = G_SoundIndex( "sound/plats/medplat1.opus" );
		ent->soundPos1 = ent->soundPos2 = G_SoundIndex( "sound/plats/medplat2.opus" );
		break;
	default:
		break;   /* sounds 0: silent */
	}

	/* height: explicit key overrides brush-extent computation */
	if ( !G_SpawnFloat( "height", "0", &height ) ) {
		height = fabsf( ent->r.maxs[2] - ent->r.mins[2] ) - lip;
	}

	/* Q1: pos1 = bottom (start), pos2 = top (destination).
	   The entity is placed in the editor at its top position (s.origin = top).
	   Q3_InitMover starts the plat at pos1 and places the player-trigger at pos1,
	   so both the spawn origin and the trigger shaft are at floor level (bottom). */
	VectorCopy( ent->s.origin, ent->pos2 );   /* pos2 = top (editor origin) */
	VectorCopy( ent->pos2,     ent->pos1 );
	ent->pos1[2] -= height;                   /* pos1 = bottom */

	ent->blocked = Q3_Blocked_Door;
	ent->touch   = Q3_Touch_Plat;
	ent->parent  = ent;

	Q3_InitMover( ent );
	ent->reached = Reached_Q1Plat;   /* fire targets at top (pos2) arrival */

	if ( ent->targetname && ent->targetname[0] ) {
		/* Q1 targeted plat: rests at TOP waiting for a trigger/button.
		   Q3_InitMover placed it at pos1 (BOTTOM); reposition to pos2 (TOP). */
		VectorCopy( ent->pos2, ent->r.currentOrigin );
		VectorCopy( ent->pos2, ent->s.pos.trBase );
		ent->s.pos.trType = TR_STATIONARY;
		ent->s.pos.trTime = 0;
		ent->moverState   = MOVER_POS2;
		ent->use          = Q1_Plat_TargetedUse;
		trap_LinkEntity( ent );
	} else {
		/* Q3_SpawnPlatTrigger uses pos1 — always bottom after the swap above.
		   PLAT_LOW_TRIGGER=1 is a legacy no-op (bottom IS the Q1 default). */
		Q3_SpawnPlatTrigger( ent );
	}
	/* DIAG Bug 3 — remove after fix */
	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "q1_func_plat spawn: id=%i origin=(%.1f %.1f %.1f) "
	         "pos1=(%.1f %.1f %.1f) pos2=(%.1f %.1f %.1f) "
	         "absmin=(%.1f %.1f %.1f) absmax=(%.1f %.1f %.1f)\n",
	         ent->s.number,
	         ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
	         ent->pos1[0], ent->pos1[1], ent->pos1[2],
	         ent->pos2[0], ent->pos2[1], ent->pos2[2],
	         ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2],
	         ent->r.absmax[0], ent->r.absmax[1], ent->r.absmax[2] );
}

/*
=================
Q1_LinkDoors

Link adjacent q1_func_door entities into mover teams so they open together.
G_FindTeams already handles doors sharing an explicit "team" key; this pass
handles the implicit Q1 convention: touching bboxes without a shared key.
Called from G_InitGame after G_FindTeams, Q1 BSP maps only.
=================
*/
void Q1_LinkDoors( void ) {
	gentity_t  *ent, *other, *member, *tail;
	int         i, j, doorCount, teamCount;
	qboolean    found;

	doorCount = 0;
	teamCount = 0;

	for ( i = 0; i < level.num_entities; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse ) continue;
		if ( !ent->classname ) continue;
		if ( Q_stricmp( ent->classname, "q1_func_door" ) != 0 ) continue;
		doorCount++;
		if ( ent->teammaster ) continue;   /* already in a team from G_FindTeams */
		if ( ent->spawnflags & DOOR_DONT_LINK ) continue;  /* Q1 ref: doors.qc DOOR_DONT_LINK=4 */

		ent->teammaster = ent;
		teamCount++;

		/* Iteratively expand the team: keep scanning until no new door is added. */
		do {
			found = qfalse;
			for ( j = 0; j < level.num_entities; j++ ) {
				other = &g_entities[j];
				if ( !other->inuse ) continue;
				if ( !other->classname ) continue;
				if ( Q_stricmp( other->classname, "q1_func_door" ) != 0 ) continue;
				if ( other->teammaster ) continue;
				if ( other->spawnflags & DOOR_DONT_LINK ) continue;

				for ( member = ent; member; member = member->teamchain ) {
					if ( member->r.absmax[0] >= other->r.absmin[0] - 1 &&
					     member->r.absmax[1] >= other->r.absmin[1] - 1 &&
					     member->r.absmax[2] >= other->r.absmin[2] - 1 &&
					     other->r.absmax[0]  >= member->r.absmin[0] - 1 &&
					     other->r.absmax[1]  >= member->r.absmin[1] - 1 &&
					     other->r.absmax[2]  >= member->r.absmin[2] - 1 ) {
						other->teammaster = ent;
						other->flags |= FL_TEAMMEMBER;
						tail = ent;
						while ( tail->teamchain ) tail = tail->teamchain;
						tail->teamchain = other;
						found = qtrue;
						break;
					}
				}
			}
		} while ( found );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_game), "Q1_LinkDoors: %d doors, %d teams formed\n", doorCount, teamCount );
}

/*
=================
Q1_Button_Touch

Custom touch handler for func_button.  Plays the activation sound via
G_Sound (a temp-entity event, guaranteed to reach nearby clients) then
drives the binary mover.  Mirrors Q1 buttons.qc button_touch.
=================
*/
static void Q1_Button_Touch( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) return;
	if ( ent->moverState != MOVER_POS1 ) return;

	/* Key requirements (mirrors rerelease button_touch in buttons.qc) */
	if ( ent->spawnflags & DOOR_SILVER_KEY ) {
		if ( !( other->client->ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( HI_KEY_SILVER ) ) ) {
			if ( ent->pain_debounce_time > level.time ) return;
			ent->pain_debounce_time = level.time + 2000;
			trap_SendServerCommand( other - g_entities,
				va( "cp \"You need the %s\n\"",
				    Q1_KeyLabel( level.q1_worldtype, qfalse ) ) );
			return;
		}
	}
	if ( ent->spawnflags & DOOR_GOLD_KEY ) {
		if ( !( other->client->ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( HI_KEY_GOLD ) ) ) {
			if ( ent->pain_debounce_time > level.time ) return;
			ent->pain_debounce_time = level.time + 2000;
			trap_SendServerCommand( other - g_entities,
				va( "cp \"You need the %s\n\"",
				    Q1_KeyLabel( level.q1_worldtype, qtrue ) ) );
			return;
		}
	}

	if ( ent->noise_index )
		G_Sound( ent, CHAN_VOICE, ent->noise_index );
	Q3_Use_BinaryMover( ent, other, other );
}

/*
=================
Q1_Button_Die

Die handler for shoot-to-activate buttons (health > 0).
Mirrors Q1 buttons.qc button_killed — three canonical steps:
  1. Guard: ignore if already moving (Q1: "if state == STATE_UP || STATE_TOP return")
  2. Reset health to MAX_HEALTH — re-arms for next cycle (Q1: "self.health = self.max_health")
  3. Disable takedamage during travel — prevents concurrent fires (Q1: "self.takedamage = DAMAGE_NO")
Re-arm (step 4: "if self.health self.takedamage = DAMAGE_YES") is done in Q1_Button_Reached.
=================
*/
static void Q1_Button_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	if ( self->moverState != MOVER_POS1 ) {
		return;   /* already activating — ignore concurrent fire */
	}
	self->health     = MAX_HEALTH;         /* reset so next cycle starts from full health */
	self->takedamage = qfalse;             /* lock out during travel */
	if ( self->noise_index )
		G_Sound( self, CHAN_VOICE, self->noise_index );
	Q3_Use_BinaryMover( self, attacker, attacker );
}

/*
=================
Q1_Button_Reached

Called when the button mover finishes a move leg (wraps Q3_Reached_BinaryMover).
Re-arms takedamage when the button returns to pos1.
Mirrors Q1 button_return: "if (self.health) self.takedamage = DAMAGE_YES"
=================
*/
static void Q1_Button_Reached( gentity_t *ent ) {
	qboolean wasReturning = ( ent->moverState == MOVER_2TO1 );
	Q3_Reached_BinaryMover( ent );
	ent->s.frame = wasReturning ? 0 : 1;   /* Q1 button_return/button_wait: frame=0 rest, frame=1 pressed */
	if ( wasReturning && ent->die ) {
		ent->takedamage = qtrue;   /* re-arm: button is back at rest, ready to be shot again */
	}
}

/*
=================
SP_q1_func_button

Q1 button: moves along `angle` when touched (or shot if health is set),
fires targets, then returns after `wait` seconds.
=================
*/
void SP_q1_func_button( gentity_t *ent ) {
	float   angle, lip, distance;
	vec3_t  abs_movedir, size;
	int     sounds;

	G_SpawnFloat( "angle",  "0", &angle );
	G_SpawnFloat( "lip",    "4", &lip   );
	G_SpawnInt(   "sounds", "0", &sounds );

	if ( !ent->speed ) ent->speed = 40;
	if ( !ent->wait )  ent->wait  = 1;
	ent->wait *= 1000;

	/* Q1 button sound sets from buttons.qc (sounds 0/1=airbut1, 2=switch21, 3=switch02, 4=switch04) */
	switch ( sounds ) {
	case 0:  /* default: airbut1 (Q1 canonical: sounds 0 → same as sounds 1) */
	case 1:  ent->noise_index = G_SoundIndex( "sound/buttons/airbut1.opus"  ); break;
	case 2:  ent->noise_index = G_SoundIndex( "sound/buttons/switch21.opus" ); break;
	case 3:  ent->noise_index = G_SoundIndex( "sound/buttons/switch02.opus" ); break;
	case 4:  ent->noise_index = G_SoundIndex( "sound/buttons/switch04.opus" ); break;
	default: break;
	}

	VectorCopy( ent->s.origin, ent->pos1 );

	trap_SetBrushModel( ent, ent->model );
	/* CONTENTS_TRIGGER required so G_TouchTriggers (g_active.c:262) dispatches
	 * Q1_Button_Touch when the player contacts the button brush. */
	ent->r.contents = CONTENTS_SOLID | CONTENTS_TRIGGER;

	SetMovedirQ1( angle, ent->movedir );

	abs_movedir[0] = fabsf( ent->movedir[0] );
	abs_movedir[1] = fabsf( ent->movedir[1] );
	abs_movedir[2] = fabsf( ent->movedir[2] );
	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	distance = DotProduct( abs_movedir, size ) - lip;
	VectorMA( ent->pos1, distance, ent->movedir, ent->pos2 );

	if ( ent->health ) {
		ent->takedamage = qtrue;
		ent->die        = Q1_Button_Die;
	} else {
		ent->touch = Q1_Button_Touch;
	}

	Q3_InitMover( ent );
	ent->reached = Q1_Button_Reached;   /* always: updates s.frame; also re-arms takedamage for shoot buttons */
}


/*
=================
Q1_Train_Blocked

Q1 train blocked handler: damage the blocker every 500 ms without stopping.
Q1 trains push through obstacles (unlike Q3 TRAIN_BLOCK_STOPS which halts).
=================
*/
#define FUNC_TRAIN_NONSOLID  1   /* Q1/RR spawnflag: train has no collision */

static void Q1_Train_Blocked( gentity_t *ent, gentity_t *other ) {
	if ( !other->client ) {
		G_FreeEntity( other );
		return;
	}
	/* fly_sound_debounce_time: unused by trains, repurposed as damage rate-limiter */
	if ( ent->fly_sound_debounce_time > level.time ) return;
	ent->fly_sound_debounce_time = level.time + 500;
	if ( ent->damage ) {
		G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH );
	}
}

/*
=================
SP_q1_func_train

Q1 path-corner-following mover. Wraps Q3's SP_q3_func_train with:
  - FUNC_TRAIN_NONSOLID=1 (spawnflag): passthrough geometry
  - Q1 blocked semantics: damage without stopping (no TRAIN_BLOCK_STOPS)
  Q1's FUNC_TRAIN_NONSOLID=1 has the same bit as Q3's TRAIN_START_ON=1;
  the bit is cleared before delegating so Q3 doesn't misread it.
=================
*/
void SP_q1_func_train( gentity_t *ent ) {
	int q1flags = ent->spawnflags;

	/* Clear FUNC_TRAIN_NONSOLID before delegating — Q3 interprets bit 1 as TRAIN_START_ON */
	ent->spawnflags &= ~FUNC_TRAIN_NONSOLID;

	SP_q3_func_train( ent );

	/* Override blocked handler: Q1 trains damage instead of stopping */
	ent->blocked = Q1_Train_Blocked;

	if ( q1flags & FUNC_TRAIN_NONSOLID ) {
		ent->r.contents = 0;
		trap_LinkEntity( ent );
	}
}


/*
=================
SP_q1_func_rotating

Q1 continuously-rotating solid brush. Wraps Q3's SP_q3_func_rotating with Q1
spawnflag translation:
  Q1 STOP_ROTATING=2: start stopped (no angular velocity)
  Q1 X_AXIS=4        → Q3 bit 4  → apos.trDelta[2] (roll / X-axis spin)
  Q1 Y_AXIS=8        → Q3 bit 8  → apos.trDelta[0] (pitch / Y-axis spin)
  Q1 default (Z axis) → Q3 default (no axis bits) → apos.trDelta[1] (yaw / Z-axis spin)

Q1 and Q3 use identical axis-bit positions; pass through directly.
=================
*/
void SP_q1_func_rotating( gentity_t *ent ) {
	int q1flags = ent->spawnflags;

	/* Pass Q1 axis bits through — they are identical to Q3's encoding */
	ent->spawnflags &= ~( 2 | 4 | 8 );
	if ( q1flags & 4 ) {
		ent->spawnflags |= 4;   /* Q1 X_AXIS → Q3 bit 4 → trDelta[2] (roll) */
	} else if ( q1flags & 8 ) {
		ent->spawnflags |= 8;   /* Q1 Y_AXIS → Q3 bit 8 → trDelta[0] (pitch) */
	}
	/* Q1 default → Q3 default (no bits) → trDelta[1] = yaw (Z-axis / ceiling fan) */

	SP_q3_func_rotating( ent );

	if ( q1flags & 2 ) {   /* STOP_ROTATING: suppress angular velocity until triggered */
		ent->s.apos.trType = TR_STATIONARY;
		VectorClear( ent->s.apos.trDelta );
		trap_LinkEntity( ent );
	}
}

/*QUAKED q1_func_episodegate (0 .5 .8) ?
DEFERRED — depends on g_q1ServerFlags + sigil collection system (not yet implemented).
Q1 vanilla: gate is solid only after the matching episode sigil is collected
(spawnflag bit: E1=1, E2=2, E3=4, E4=8). Used in start.bsp hub progression.
See docs/deferred_q1_workstreams.md.
*/
void SP_q1_func_episodegate( gentity_t *ent ) {
	G_FreeEntity( ent );
}

/*QUAKED q1_func_bossgate (0 .5 .8) ?
DEFERRED — depends on g_q1ServerFlags + sigil collection system (not yet implemented).
Q1 vanilla: gate is solid until all 4 episode sigils are collected
((serverflags & 15) == 15), then opens for the Shub-Niggurath fight.
See docs/deferred_q1_workstreams.md.
*/
void SP_q1_func_bossgate( gentity_t *ent ) {
	G_FreeEntity( ent );
}
