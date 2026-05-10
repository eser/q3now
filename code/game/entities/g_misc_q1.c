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

// Q1 misc / ambient entities

#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

/* worldtype constants matching Q1/RR */
#define WORLDTYPE_MEDIEVAL  0
#define WORLDTYPE_METAL     1
#define WORLDTYPE_BASE      2
#define WORLDTYPE_HUB       3

/* trap_spikeshooter / trap_shooter spawnflags (Q1 ref: rerelease misc.qc:341, FTEQW misc.qc:280) */
#define Q1_TRAP_SUPERSPIKE  1   /* fires 18-dmg super-spike instead of 9-dmg spike */
#define Q1_TRAP_LASER       2   /* fires laser projectile (15 dmg, 600 speed) */

/*
=================
SP_q1_light_fluoro

Q1 fluorescent light fixture — emits a looping ambient hum.
Reference: FTEQW misc.qc:63–76 — ambientsound "ambience/fl_hum1.wav".
=================
*/
void SP_q1_light_fluoro( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/fl_hum1.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_light_fluorospark

Q1 flickering fluorescent light — emits a looping electrical buzz.
=================
*/
void SP_q1_light_fluorospark( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/buzz1.wav" );
	ent->s.eType = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_comp_hum

Q1 computer / machinery ambient — continuous electronic hum.
=================
*/
void SP_q1_ambient_comp_hum( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/comp1.wav" );
	ent->s.eType = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_drone

Q1 deep ambient drone — used for e.g. dungeon atmosphere.
=================
*/
void SP_q1_ambient_drone( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/drone6.wav" );
	ent->s.eType = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_suck_wind

Q1 wind ambient — lava/wind suction sound (e.g. e1m5 lava pit).
=================
*/
void SP_q1_ambient_suck_wind( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/suck1.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_drip

Q1 dripping water ambient — used in dungeon areas (e.g. e1m1).
=================
*/
void SP_q1_ambient_drip( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/drip1.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_thunder

Q1 outdoor thunder ambient — used in exterior areas (e.g. e1m4).
=================
*/
void SP_q1_ambient_thunder( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/thunder1.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_light_buzz

Q1 electrical buzz ambient — used in lab/tech areas (e.g. e1m3).
Same sound asset as light_fluoro (fl_hum1.wav); separate entity for
distinct map authoring placement semantics.
=================
*/
void SP_q1_ambient_light_buzz( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/fl_hum1.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_swamp1

Q1 swamp ambient variant 1 — used in outdoor swamp/water areas (e.g. e1m4).
=================
*/
void SP_q1_ambient_swamp1( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/swamp1.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_ambient_swamp2

Q1 swamp ambient variant 2 — used in outdoor swamp/water areas (e.g. e1m4).
=================
*/
void SP_q1_ambient_swamp2( gentity_t *ent ) {
	ent->noise_index = G_SoundIndex( "sound/ambience/swamp2.wav" );
	ent->s.eType     = ET_SPEAKER;
	ent->s.loopSound = ent->noise_index;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_info_intermission

Q1 intermission camera point — records position; no think or physics.
=================
*/
void SP_q1_info_intermission( gentity_t *ent ) {
	char *mangle;

	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	/* Q1 uses "mangle" for camera pitch/roll/yaw (format: "pitch roll yaw").
	   The standard spawn system only reads "angles" and "angle" keys; mangle
	   must be parsed explicitly as a fallback when angles would be zero. */
	if ( G_SpawnString( "mangle", "", &mangle ) && mangle[0] ) {
		sscanf( mangle, "%f %f %f",
		        &ent->s.angles[PITCH],
		        &ent->s.angles[ROLL],
		        &ent->s.angles[YAW] );
	}

	/* FindIntermissionPoint searches only for "info_player_intermission".
	   Reclassify so Q1 maps work with the shared intermission system. */
	ent->classname = "info_player_intermission";
}

/*
=================
misc_explobox_die

Exploding crate death callback — radius damage, then remove.
=================
*/
static void q1_misc_explobox_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                                   int damage, int mod ) {
	gentity_t *te;
	vec3_t     up = { 0, 0, 1 };

	G_RadiusDamage( self->s.origin, attacker, 160.0f, 160.0f, self, MOD_ROCKET_SPLASH, qfalse );

	/* Q1 ref: misc.qc barrel_explode plays r_exp3.wav and shows a particle explosion.
	   G_Sound delivers the correct Q1 audio; G_TempEntity adds the rocket-explosion visual.
	   WP_NONE keeps cgame from playing a second weapon-specific sound. */
	G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/weapons/r_exp3.wav" ) );
	te = G_TempEntity( self->s.origin, EV_MISSILE_MISS );
	te->s.eventParm = DirToByte( up );
	te->s.weapon    = WP_NONE;
	te->s.pType     = PROJ_NONE;

	G_UseTargets( self, attacker );
	G_FreeEntity( self );
}

/*
=================
SP_q1_misc_explobox

Q1 explosive crate — takes damage, explodes on death.

Q1 ref: misc.qc:157–183. In vanilla Q1, setmodel(self, "maps/b_explob.bsp") loads
a standalone BSP file as both the visual mesh and collision hull. q3now uses
G_ModelIndex to register the same path; if the Q1 PAK is mounted and the renderer
supports standalone Q1 BSP props, the barrel will render. If G_ModelIndex returns 0
(asset unreachable), a warning is logged — the entity retains its collision box so
the gameplay bug (invisible obstacle) is surfaced but not silently lost.

Bbox kept explicit: misc_explobox is a point entity (no inline BSP brush, no
trap_SetBrushModel), so mins/maxs must be set manually from the QUAKED bounds.
=================
*/
void SP_q1_misc_explobox( gentity_t *ent ) {
	ent->s.modelindex = G_ModelIndex( "maps/b_explob.bsp" );
	if ( !ent->s.modelindex ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
		         "SP_q1_misc_explobox: G_ModelIndex(\"maps/b_explob.bsp\") returned 0 — barrel will be invisible. "
		         "Ensure the Q1 PAK is mounted and the renderer supports standalone Q1 BSP props.\n" );
	}

	ent->s.eType      = ET_MOVER;
	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );

	ent->health     = 20;
	ent->takedamage = qtrue;
	ent->die        = q1_misc_explobox_die;
	ent->r.contents = CONTENTS_SOLID;
	/* Q1 canonical bbox from QUAKED comment: (0 0 0) to (32 32 64) — floor-resting.
	   Cannot use trap_SetBrushModel: misc_explobox is a point entity, not an inline brush. */
	VectorSet( ent->r.mins,  0.0f,  0.0f,  0.0f );
	VectorSet( ent->r.maxs, 32.0f, 32.0f, 64.0f );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );
}

/*
=================
SP_q1_misc_explobox2

Q1 smaller explosive barrel variant (registered-only in vanilla Q1).
Identical behaviour to misc_explobox: health=20, radius damage 160 on death.
Q1 ref: misc.qc:192–196 — uses "maps/b_exbox2.bsp" as the model.
FTEQW ref: misc.qc:245–273 — uses bbox 0,0,0 → 32,32,32 (asymmetric, floor-resting).
=================
*/
void SP_q1_misc_explobox2( gentity_t *ent ) {
	ent->s.modelindex = G_ModelIndex( "maps/b_exbox2.bsp" );
	if ( !ent->s.modelindex ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
		         "SP_q1_misc_explobox2: G_ModelIndex(\"maps/b_exbox2.bsp\") returned 0 — barrel will be invisible. "
		         "Ensure the Q1 PAK is mounted and the renderer supports standalone Q1 BSP props.\n" );
	}

	ent->s.eType      = ET_MOVER;
	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );

	ent->health     = 20;
	ent->takedamage = qtrue;
	ent->die        = q1_misc_explobox_die;
	ent->r.contents = CONTENTS_SOLID;
	/* Q1 canonical bbox: same as explobox1, (0 0 0) to (32 32 64). */
	VectorSet( ent->r.mins,  0.0f,  0.0f,  0.0f );
	VectorSet( ent->r.maxs, 32.0f, 32.0f, 64.0f );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );
}

/*QUAKED q1_air_bubbles (1 .5 0) (-8 -8 -8) (8 8 8)
Q1 vanilla map-positioned bubble emitter. q3now uses player-driven CG_BubblePuffs
instead, so this entity is silently removed at spawn.
*/
void SP_q1_air_bubbles( gentity_t *ent ) {
	G_FreeEntity( ent );
}

/*QUAKED q1_misc_teleporttrain (1 .5 0) (-8 -8 -8) (8 8 8)
DEFERRED — depends on Shub-Niggurath boss fight (not yet implemented).
Q1 vanilla: spinning telefrag platform used in e4m8 to kill Shub-Niggurath.
Follows path_corner waypoints; kills Shub when player rides it through her location.
See docs/deferred_q1_workstreams.md.
*/
void SP_q1_misc_teleporttrain( gentity_t *ent ) {
	G_FreeEntity( ent );
}

/*
=================
SP_q1_func_wall

Q1 static brush entity — solid, visible, immovable.
=================
*/
void SP_q1_func_wall( gentity_t *ent ) {
	trap_SetBrushModel( ent, ent->model );

	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "SP_q1_func_wall [after SetBrushModel]:"
	         " model='%s' s.modelindex=%d"
	         " r.mins=(%.0f,%.0f,%.0f) r.maxs=(%.0f,%.0f,%.0f)"
	         " s.origin=(%.0f,%.0f,%.0f)"
	         " r.bmodel=%d s.eType=%d s.solid=0x%x\n",
	         ent->model ? ent->model : "<null>",
	         ent->s.modelindex,
	         ent->r.mins[0], ent->r.mins[1], ent->r.mins[2],
	         ent->r.maxs[0], ent->r.maxs[1], ent->r.maxs[2],
	         ent->s.origin[0], ent->s.origin[1], ent->s.origin[2],
	         (int)ent->r.bmodel,
	         (int)ent->s.eType,
	         (unsigned)ent->s.solid );

	ent->s.eType          = ET_MOVER;
	ent->s.pos.trType     = TR_STATIONARY;
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	ent->r.contents       = CONTENTS_SOLID;
	ent->r.svFlags       |= SVF_USE_CURRENT_ORIGIN;
	ent->r.svFlags       &= ~SVF_NOCLIENT;
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );

	Com_Log( SEV_TRACE, LOG_CH(ch_game),
	         "SP_q1_func_wall [after LinkEntity]:"
	         " r.linked=%d"
	         " r.absmin=(%.0f,%.0f,%.0f) r.absmax=(%.0f,%.0f,%.0f)"
	         " s.eType=%d s.solid=0x%x\n",
	         (int)ent->r.linked,
	         ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2],
	         ent->r.absmax[0], ent->r.absmax[1], ent->r.absmax[2],
	         (int)ent->s.eType,
	         (unsigned)ent->s.solid );
}

/*
=================
SP_q1_func_illusionary

Q1 non-solid visible brush.  Used for decorative geometry like torch flames,
fake floors, and hidden passage disguises.  Contents = 0 so nothing collides.
=================
*/
void SP_q1_func_illusionary( gentity_t *ent ) {
	trap_SetBrushModel( ent, ent->model );
	ent->r.contents  = 0;           /* non-solid */
	ent->s.eType     = ET_GENERAL;  /* visible */
	ent->r.svFlags  &= ~SVF_NOCLIENT;
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );
}


/*
=================
SP_q1_worldspawn

Q1 worldspawn.  Reads Q1-specific keys and stores them in level state.
The regular Q3 worldspawn (g_spawn.c SP_worldspawn) handles gravity and music
for Q3 maps; this handler mirrors that for Q1-prefixed maps.
=================
*/
static void Q1_CountSecrets( gentity_t *self ) {
	int i;
	level.q1_total_secrets = 0;
	for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
		gentity_t *e = &g_entities[i];
		if ( !e->inuse || !e->classname ) continue;
		if ( !Q_stricmp( e->classname, "q1_trigger_secret" ) )
			level.q1_total_secrets++;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_game), "Q1 secrets: %d total\n", level.q1_total_secrets );
}

void SP_q1_worldspawn( gentity_t *ent ) {
	char *s;
	int   worldtype, gravity, sounds, light;

	G_SpawnInt( "worldtype", "0", &worldtype );
	level.q1_worldtype = worldtype;

	G_SpawnInt( "gravity", "800", &gravity );
	if ( gravity > 0 ) {
		trap_Cvar_Set( "g_envGravity", va( "%d", gravity ) );
	}

	if ( G_SpawnString( "sky", "", &s ) && s[0] ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "Q1 worldspawn: sky='%s' (renderer integration deferred)\n", s );
	}

	if ( G_SpawnInt( "sounds", "0", &sounds ) && sounds ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "Q1 worldspawn: sounds=%d (music mapping deferred)\n", sounds );
	}

	if ( G_SpawnInt( "light", "0", &light ) && light ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
		         "Q1 worldspawn: light=%d (ambient deferred)\n", light );
	}

	/* Count secrets after all entities are spawned.
	   Entities are spawned sequentially; worldspawn is typically first but
	   other entities exist by the time G_InitGame calls Q1_LinkDoors.
	   Schedule a think to count secrets after the current spawn pass. */
	ent->think     = Q1_CountSecrets;
	ent->nextthink = level.time + FRAMETIME;

	Com_Log( SEV_INFO, LOG_CH(ch_game),
	         "SP_q1_worldspawn: worldtype=%d gravity=%d\n", worldtype, gravity );
}


/*
=================
q1_key_touch

Shared pickup callback for Q1 key items.
Sets the appropriate holdable bit and sends a centerprint to the picker.
Keys are never removed (single player: key persists in inventory).
=================
*/
static void q1_key_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	int      tag;
	qboolean isGold;
	const char *label;

	if ( !other->client ) return;

	tag    = self->item->giTag;
	isGold = ( tag == HI_KEY_GOLD );

	/* Already have the key? */
	if ( other->client->ps.stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT( tag ) )
		return;

	other->client->ps.stats[STAT_HOLDABLE_BITS] |= BG_HOLDABLE_BIT( tag );

	/* Key label by worldtype */
	switch ( level.q1_worldtype ) {
	case WORLDTYPE_METAL:
		label = isGold ? "Gold Runekey" : "Silver Runekey"; break;
	case WORLDTYPE_BASE:
		label = isGold ? "Gold Keycard" : "Silver Keycard"; break;
	default:
		label = isGold ? "Gold Key" : "Silver Key"; break;
	}

	trap_SendServerCommand( other - g_entities, va( "cp \"Picked up %s\n\"", label ) );

	if ( self->noise_index )
		G_Sound( other, CHAN_AUTO, self->noise_index );

	/* Fire key's own targets (Q1 ref: key_touch calls SUB_UseTargets after granting items).
	   Must be called before G_FreeEntity so self is still valid. */
	G_UseTargets( self, other );

	/* Remove key entity (single-player: key exists once in world) */
	G_FreeEntity( self );
}

/*
=================
SP_q1_item_key1  (silver key)
SP_q1_item_key2  (gold key)

Spawns key item with correct model/sound for current worldtype.
=================
*/
void SP_q1_item_key1( gentity_t *ent ) {
	gitem_t *it;
	for ( it = bg_itemlist + 1; it->classname; it++ ) {
		if ( it->giTag == HI_KEY_SILVER ) {
			G_SpawnItem( ent, it );
			ent->touch = q1_key_touch;
			if ( it->pickup_sound )
				ent->noise_index = G_SoundIndex( (char *)it->pickup_sound );
			return;
		}
	}
	G_FreeEntity( ent );
}

void SP_q1_item_key2( gentity_t *ent ) {
	gitem_t *it;
	for ( it = bg_itemlist + 1; it->classname; it++ ) {
		if ( it->giTag == HI_KEY_GOLD ) {
			G_SpawnItem( ent, it );
			ent->touch = q1_key_touch;
			if ( it->pickup_sound )
				ent->noise_index = G_SoundIndex( (char *)it->pickup_sound );
			return;
		}
	}
	G_FreeEntity( ent );
}


/*
=================
q1_megahealth_touch

Q1 megahealth pickup.  Rerelease: hard cap at 250 instead of max_health + 150.
Grants 100 health, caps at 250.
=================
*/
static void q1_megahealth_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
#define Q1_MEGAHEALTH_CAP  250
	if ( !other->client ) return;
	if ( other->client->ps.stats[STAT_HEALTH] <= 0 ) return;
	/* Q1 ref: item_health touch — if already at cap, don't consume the item (return early). */
	if ( other->health >= Q1_MEGAHEALTH_CAP ) return;

	other->health += 100;
	if ( other->health > Q1_MEGAHEALTH_CAP )
		other->health = Q1_MEGAHEALTH_CAP;
	other->client->ps.stats[STAT_HEALTH] = other->health;

	trap_SendServerCommand( other - g_entities, "cp \"Megahealth!\n\"" );

	G_Sound( other, CHAN_AUTO, G_SoundIndex( "sound/items/l_health.opus" ) );

	G_FreeEntity( self );
}

/*
=================
q1_trap_spikeshooter_use / SP_q1_trap_spikeshooter

Q1 trap that fires a spike, super-spike, or laser on each use/trigger event.
SUPERSPIKE (1): 18-dmg super-spike.  LASER (2): 15-dmg laser at 600 speed.
Default: 9-dmg normal spike at 1000 speed.

Q1 ref: rerelease misc.qc:315–362; FTEQW misc.qc:280–350.
=================
*/
static void q1_trap_spikeshooter_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( self->spawnflags & Q1_TRAP_LASER ) {
		fire_q1_laser( self, self->s.origin, self->movedir );
	} else if ( self->spawnflags & Q1_TRAP_SUPERSPIKE ) {
		/* Q1 ref: misc.qc spikeshooter_use — trap speed is 500, not player nailgun speed 1000 */
		fire_q1_spike( self, self->s.origin, self->movedir, 18, 500 );
	} else {
		fire_q1_spike( self, self->s.origin, self->movedir, 9, 500 );
	}
}

void SP_q1_trap_spikeshooter( gentity_t *ent ) {
	G_SetMovedir( ent->s.angles, ent->movedir );
	ent->use = q1_trap_spikeshooter_use;
	trap_LinkEntity( ent );
}

/*
=================
q1_trap_shooter_think / SP_q1_trap_shooter

Continuous-fire variant of trap_spikeshooter.
Fires every "wait" seconds (default 1, minimum 0.1).

Q1 ref: rerelease misc.qc:333–379; FTEQW misc.qc:324–366.
=================
*/
static void q1_trap_shooter_think( gentity_t *self ) {
	q1_trap_spikeshooter_use( self, self, self );
	self->nextthink = level.time + (int)( self->wait * 1000.0f );
}

void SP_q1_trap_shooter( gentity_t *ent ) {
	SP_q1_trap_spikeshooter( ent );

	G_SpawnFloat( "wait", "1", &ent->wait );
	if ( ent->wait < 0.1f ) ent->wait = 0.1f;

	ent->think     = q1_trap_shooter_think;
	ent->nextthink = level.time + (int)( ent->wait * 1000.0f );
}


/*
=================
q1_misc_fireball_think / SP_q1_misc_fireball

Periodic lavaball emitter.  Spawns an upward-flying lavaball every 3–8 s.
XY jitter ±≈60, Z = speed (default 1000) + random*200.

Q1 ref: rerelease misc.qc:186–236 (fire_fly); FTEQW misc.qc:166–195.
=================
*/
static void q1_misc_fireball_think( gentity_t *self ) {
	vec3_t dir;
	float  jitter_x, jitter_y, vz, magnitude;

	/* Q1 fire_fly XY jitter and upward Z velocity */
	jitter_x = ( crandom() * 10.0f ) + ( ( random() * 2.0f - 1.0f ) * 50.0f );
	jitter_y = ( crandom() * 10.0f ) + ( ( random() * 2.0f - 1.0f ) * 50.0f );
	vz       = self->speed + random() * 200.0f;

	dir[0] = jitter_x;
	dir[1] = jitter_y;
	dir[2] = vz;

	magnitude = VectorLength( dir );
	if ( magnitude > 0.001f ) {
		VectorScale( dir, 1.0f / magnitude, dir );
		fire_q1_lavaball( self, self->s.origin, dir, magnitude );
	}

	/* re-arm: 3–8 seconds between shots (Q1: random()*5 + 3) */
	self->nextthink = level.time + (int)( ( random() * 5.0f + 3.0f ) * 1000.0f );
}

void SP_q1_misc_fireball( gentity_t *ent ) {
	if ( ent->speed <= 0.0f ) ent->speed = 1000.0f;

	ent->think     = q1_misc_fireball_think;
	/* stagger first shot 0–5 s so simultaneous emitters don't fire in sync */
	ent->nextthink = level.time + (int)( random() * 5000.0f );
}
