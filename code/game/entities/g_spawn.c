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

#include "g_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

qboolean	G_SpawnString( const char *key, const char *defaultString, char **out ) {
	int		i;

	if ( !level.spawning ) {
		*out = (char *)defaultString;
//		Com_Terminate( TERM_CLIENT_DROP, "G_SpawnString() called while not spawning" );
	}

	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		if ( !Q_stricmp( key, level.spawnVars[i][0] ) ) {
			*out = level.spawnVars[i][1];
			return qtrue;
		}
	}

	*out = (char *)defaultString;
	return qfalse;
}

qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	*out = atof( s );
	return present;
}

qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	*out = atoi( s );
	return present;
}

qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out ) {
	char		*s;
	qboolean	present;

	present = G_SpawnString( key, defaultString, &s );
	sscanf( s, "%f %f %f", &out[0], &out[1], &out[2] );
	return present;
}



//
// fields are needed for spawning from the entity string
//
typedef enum {
	F_INT,
	F_FLOAT,
	F_STRING,
	F_VECTOR,
	F_ANGLEHACK
} fieldtype_t;

typedef struct
{
	char	*name;
	size_t	ofs;
	fieldtype_t	type;
} field_t;

field_t fields[] = {
	{"classname", FOFS(classname), F_STRING},
	{"origin", FOFS(s.origin), F_VECTOR},
	{"model", FOFS(model), F_STRING},
	{"model2", FOFS(model2), F_STRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"speed", FOFS(speed), F_FLOAT},
	{"target", FOFS(target), F_STRING},
	{"targetname", FOFS(targetname), F_STRING},
	{"message", FOFS(message), F_STRING},
	{"team", FOFS(team), F_STRING},
	{"wait", FOFS(wait), F_FLOAT},
	{"random", FOFS(random), F_FLOAT},
	{"count", FOFS(count), F_INT},
	{"health", FOFS(health), F_INT},
	{"dmg", FOFS(damage), F_INT},
	{"angles", FOFS(s.angles), F_VECTOR},
	{"angle", FOFS(s.angles), F_ANGLEHACK},
	{"targetShaderName", FOFS(targetShaderName), F_STRING},
	{"targetShaderNewName", FOFS(targetShaderNewName), F_STRING},

	{NULL}
};


typedef struct {
	char	*name;
	void	(*spawn)(gentity_t *ent);
} spawn_t;

void SP_info_player_start (gentity_t *ent);
void SP_info_player_deathmatch (gentity_t *ent);
void SP_info_player_intermission (gentity_t *ent);

void SP_q3_func_plat (gentity_t *ent);
void SP_q3_func_static (gentity_t *ent);
void SP_q3_func_rotating (gentity_t *ent);
void SP_q3_func_bobbing (gentity_t *ent);
void SP_q3_func_pendulum( gentity_t *ent );
void SP_q3_func_button (gentity_t *ent);
void SP_q3_func_door (gentity_t *ent);
void SP_q3_func_train (gentity_t *ent);
void SP_q3_func_timer (gentity_t *self);

void SP_q3_trigger_always (gentity_t *ent);
void SP_q3_trigger_multiple (gentity_t *ent);
void SP_q3_trigger_push (gentity_t *ent);
void SP_q3_trigger_teleport (gentity_t *ent);
void SP_q3_trigger_hurt (gentity_t *ent);

void SP_q3_target_remove_powerups( gentity_t *ent );
void SP_q3_target_give (gentity_t *ent);
void SP_q3_target_delay (gentity_t *ent);
void SP_q3_target_speaker (gentity_t *ent);
void SP_q3_target_print (gentity_t *ent);
void SP_q3_target_laser (gentity_t *self);
void SP_q3_target_score( gentity_t *ent );
void SP_q3_target_teleporter( gentity_t *ent );
void SP_q3_target_relay (gentity_t *ent);
void SP_q3_target_kill (gentity_t *ent);
void SP_q3_target_position (gentity_t *ent);
void SP_q3_target_location (gentity_t *ent);
void SP_q3_target_push (gentity_t *ent);
#if FEAT_EARTHQUAKE_SYSTEM
void SP_q3_target_earthquake (gentity_t *ent);
#endif

void SP_q3_light (gentity_t *self);
void SP_q3_info_null (gentity_t *self);
void SP_q3_info_notnull (gentity_t *self);
void SP_q3_info_camp (gentity_t *self);
void SP_q3_path_corner (gentity_t *self);

void SP_q3_misc_teleporter_dest (gentity_t *self);
void SP_q3_misc_model(gentity_t *ent);
void SP_q3_misc_portal_camera(gentity_t *ent);
void SP_q3_misc_portal_surface(gentity_t *ent);

// Q1 entities
void SP_q1_light_fluoro (gentity_t *ent);
void SP_q1_light_fluorospark (gentity_t *ent);
void SP_q1_ambient_comp_hum (gentity_t *ent);
void SP_q1_ambient_drone (gentity_t *ent);
void SP_q1_ambient_suck_wind (gentity_t *ent);
void SP_q1_ambient_drip (gentity_t *ent);
void SP_q1_ambient_thunder (gentity_t *ent);
void SP_q1_ambient_light_buzz (gentity_t *ent);
void SP_q1_ambient_swamp1 (gentity_t *ent);
void SP_q1_ambient_swamp2 (gentity_t *ent);
void SP_q1_info_intermission (gentity_t *ent);
void SP_q1_misc_explobox (gentity_t *ent);
void SP_q1_misc_explobox2 (gentity_t *ent);
void SP_q1_misc_fireball (gentity_t *ent);
void SP_q1_air_bubbles (gentity_t *ent);
void SP_q1_misc_teleporttrain (gentity_t *ent);
void SP_q1_func_wall (gentity_t *ent);
void SP_q1_func_illusionary (gentity_t *ent);
void SP_q1_func_door (gentity_t *ent);
void SP_q1_func_door_secret (gentity_t *ent);
void SP_q1_func_plat (gentity_t *ent);
void SP_q1_func_button (gentity_t *ent);
void SP_q1_func_train (gentity_t *ent);
void SP_q1_func_rotating (gentity_t *ent);
void SP_q1_func_episodegate (gentity_t *ent);
void SP_q1_func_bossgate (gentity_t *ent);
void SP_q1_info_teleport_destination (gentity_t *ent);
void SP_q1_trigger_once (gentity_t *ent);
void SP_q1_trigger_multiple (gentity_t *ent);
void SP_q1_trigger_counter (gentity_t *ent);
void SP_q1_trigger_counter_timed (gentity_t *ent);
void SP_q1_trigger_secret (gentity_t *ent);
void SP_q1_trigger_changelevel (gentity_t *ent);
void SP_q1_trigger_relay (gentity_t *ent);
void SP_q1_trigger_push (gentity_t *ent);
void SP_q1_trigger_hurt (gentity_t *ent);
void SP_q1_trigger_teleport (gentity_t *ent);
void SP_q1_trigger_setskill (gentity_t *ent);
void SP_q1_trigger_monsterjump (gentity_t *ent);
void SP_q1_trigger_onlyregistered (gentity_t *ent);
void SP_q1_trap_spikeshooter (gentity_t *ent);
void SP_q1_trap_shooter (gentity_t *ent);
void SP_q1_worldspawn (gentity_t *ent);
void SP_q1_item_key1 (gentity_t *ent);
void SP_q1_item_key2 (gentity_t *ent);

void SP_misc_lightstyle (gentity_t *ent);

void SP_q3_shooter_rocket( gentity_t *ent );
void SP_q3_shooter_plasma( gentity_t *ent );
void SP_q3_shooter_grenade( gentity_t *ent );

void SP_team_CTF_redplayer( gentity_t *ent );
void SP_team_CTF_blueplayer( gentity_t *ent );

void SP_team_CTF_redspawn( gentity_t *ent );
void SP_team_CTF_bluespawn( gentity_t *ent );

#if FEAT_OVERLOAD
void SP_team_blueobelisk( gentity_t *ent );
void SP_team_redobelisk( gentity_t *ent );
void SP_team_neutralobelisk( gentity_t *ent );
#endif
void SP_item_botroam( gentity_t *ent ) { }

spawn_t	spawns[] = {
	// info entities don't do anything at all, but provide positional
	// information for things controlled by other processes
	{"info_player_start", SP_info_player_start},
	{"info_player_deathmatch", SP_info_player_deathmatch},
	{"info_player_intermission", SP_info_player_intermission},
	{"info_null", SP_q3_info_null},
	{"info_notnull", SP_q3_info_notnull},		// use target_position instead
	{"info_camp", SP_q3_info_camp},

	{"func_plat", SP_q3_func_plat},
	{"func_button", SP_q3_func_button},
	{"func_door", SP_q3_func_door},
	{"func_static", SP_q3_func_static},
	{"func_rotating", SP_q3_func_rotating},
	{"func_bobbing", SP_q3_func_bobbing},
	{"func_pendulum", SP_q3_func_pendulum},
	{"func_train", SP_q3_func_train},
	{"func_group", SP_q3_info_null},
	{"func_timer", SP_q3_func_timer},			// rename trigger_timer?

	// Triggers are brush objects that cause an effect when contacted
	// by a living player, usually involving firing targets.
	// While almost everything could be done with
	// a single trigger class and different targets, triggered effects
	// could not be client side predicted (push and teleport).
	{"trigger_always", SP_q3_trigger_always},
	{"trigger_multiple", SP_q3_trigger_multiple},
	{"trigger_push", SP_q3_trigger_push},
	{"trigger_teleport", SP_q3_trigger_teleport},
	{"trigger_hurt", SP_q3_trigger_hurt},

	// targets perform no action by themselves, but must be triggered
	// by another entity
	{"target_give", SP_q3_target_give},
	{"target_remove_powerups", SP_q3_target_remove_powerups},
	{"target_delay", SP_q3_target_delay},
	{"target_speaker", SP_q3_target_speaker},
	{"target_print", SP_q3_target_print},
	{"target_laser", SP_q3_target_laser},
	{"target_score", SP_q3_target_score},
	{"target_teleporter", SP_q3_target_teleporter},
	{"target_relay", SP_q3_target_relay},
	{"target_kill", SP_q3_target_kill},
	{"target_position", SP_q3_target_position},
	{"target_location", SP_q3_target_location},
	{"target_push", SP_q3_target_push},
#if FEAT_EARTHQUAKE_SYSTEM
	{"target_earthquake", SP_q3_target_earthquake},
#endif

	{"light", SP_q3_light},
	{"path_corner", SP_q3_path_corner},

	{"misc_teleporter_dest", SP_q3_misc_teleporter_dest},
	{"misc_model", SP_q3_misc_model},
	{"misc_portal_surface", SP_q3_misc_portal_surface},
	{"misc_portal_camera", SP_q3_misc_portal_camera},
	{"misc_lightstyle",     SP_misc_lightstyle},

	{"shooter_rocket", SP_q3_shooter_rocket},
	{"shooter_grenade", SP_q3_shooter_grenade},
	{"shooter_plasma", SP_q3_shooter_plasma},

	{"team_CTF_redplayer", SP_team_CTF_redplayer},
	{"team_CTF_blueplayer", SP_team_CTF_blueplayer},

	{"team_CTF_redspawn", SP_team_CTF_redspawn},
	{"team_CTF_bluespawn", SP_team_CTF_bluespawn},

#if FEAT_OVERLOAD
	{"team_redobelisk", SP_team_redobelisk},
	{"team_blueobelisk", SP_team_blueobelisk},
	{"team_neutralobelisk", SP_team_neutralobelisk},
#endif
	{"item_botroam", SP_item_botroam},

	// Q1 entities (classnames prefixed at BSP parse time by BSP_Q1_PrefixClassnames)
	{"q1_worldspawn",              SP_q1_worldspawn},
	{"q1_info_player_start",       SP_info_player_start},
	{"q1_info_player_deathmatch",  SP_info_player_deathmatch},
	{"q1_info_player_intermission",SP_info_player_intermission},
	{"q1_func_plat",               SP_q1_func_plat},
	{"q1_func_button",             SP_q1_func_button},
	{"q1_func_door",               SP_q1_func_door},
	{"q1_func_door_secret",        SP_q1_func_door_secret},
	{"q1_func_wall",               SP_q1_func_wall},
	{"q1_func_illusionary",        SP_q1_func_illusionary},
	{"q1_func_static",             SP_q3_func_static},
	{"q1_func_rotating",           SP_q1_func_rotating},
	{"q1_func_episodegate",        SP_q1_func_episodegate},
	{"q1_func_bossgate",           SP_q1_func_bossgate},
	{"q1_func_train",              SP_q1_func_train},
	{"q1_trigger_always",          SP_q3_trigger_always},
	{"q1_trigger_multiple",        SP_q1_trigger_multiple},
	{"q1_trigger_push",            SP_q1_trigger_push},
	{"q1_trigger_teleport",        SP_q1_trigger_teleport},
	{"q1_trigger_hurt",            SP_q1_trigger_hurt},
	{"q1_trigger_relay",           SP_q1_trigger_relay},
	{"q1_trigger_setskill",        SP_q1_trigger_setskill},
	{"q1_trigger_monsterjump",     SP_q1_trigger_monsterjump},
	{"q1_trigger_onlyregistered",  SP_q1_trigger_onlyregistered},
	{"q1_trigger_counter_timed",   SP_q1_trigger_counter_timed},
	{"q1_light",                   SP_q3_light},
	{"q1_lightstyle",              SP_misc_lightstyle},
	{"q1_path_corner",             SP_q3_path_corner},
	{"q1_light_fluoro",            SP_q1_light_fluoro},
	{"q1_light_fluorospark",       SP_q1_light_fluorospark},
	{"q1_ambient_comp_hum",        SP_q1_ambient_comp_hum},
	{"q1_ambient_drone",           SP_q1_ambient_drone},
	{"q1_ambient_suck_wind",       SP_q1_ambient_suck_wind},
	{"q1_ambient_drip",            SP_q1_ambient_drip},
	{"q1_ambient_thunder",         SP_q1_ambient_thunder},
	{"q1_ambient_light_buzz",      SP_q1_ambient_light_buzz},
	{"q1_ambient_swamp1",          SP_q1_ambient_swamp1},
	{"q1_ambient_swamp2",          SP_q1_ambient_swamp2},
	{"q1_info_intermission",       SP_q1_info_intermission},
	{"q1_info_null",               SP_q3_info_null},
	{"q1_info_notnull",            SP_q3_info_notnull},
	{"q1_misc_explobox",           SP_q1_misc_explobox},
	{"q1_misc_explobox2",          SP_q1_misc_explobox2},
	{"q1_misc_fireball",           SP_q1_misc_fireball},
	{"q1_air_bubbles",             SP_q1_air_bubbles},
	{"q1_misc_teleporttrain",      SP_q1_misc_teleporttrain},
	{"q1_trap_spikeshooter",       SP_q1_trap_spikeshooter},
	{"q1_trap_shooter",            SP_q1_trap_shooter},
	{"q1_info_teleport_destination", SP_q1_info_teleport_destination},
	{"q1_trigger_once",            SP_q1_trigger_once},
	{"q1_trigger_counter",         SP_q1_trigger_counter},
	{"q1_trigger_secret",          SP_q1_trigger_secret},
	{"q1_trigger_changelevel",     SP_q1_trigger_changelevel},
	{"q1_item_key1",               SP_q1_item_key1},
	{"q1_item_key2",               SP_q1_item_key2},

	{NULL, 0}
};

/* Per-map accumulator for classnames that lack a spawn function. */
typedef struct { const char *name; int count; } missingSpawn_t;
static missingSpawn_t g_missingSpawns[ 64 ];
static int            g_missingSpawnCount = 0;

static void G_TrackMissingSpawn( const char *classname ) {
	int i;
	for ( i = 0; i < g_missingSpawnCount; i++ ) {
		if ( !strcmp( g_missingSpawns[i].name, classname ) ) {
			g_missingSpawns[i].count++;
			return;
		}
	}
	if ( g_missingSpawnCount < (int)ARRAY_LEN( g_missingSpawns ) ) {
		g_missingSpawns[ g_missingSpawnCount ].name  = classname;
		g_missingSpawns[ g_missingSpawnCount ].count = 1;
		g_missingSpawnCount++;
	}
}

/*
===============
G_CallSpawn

Finds the spawn function for the entity and calls it,
returning qfalse if not found
===============
*/
qboolean G_CallSpawn( gentity_t *ent ) {
	spawn_t	*s;
	gitem_t	*item;

	if ( !ent->classname ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "G_CallSpawn: NULL classname\n");
		return qfalse;
	}

	// check item spawn functions
	for ( item=bg_itemlist+1 ; item->classname ; item++ ) {
		if (item->classname && !strcmp(item->classname, ent->classname)) {
			G_SpawnItem(ent, item);
			return qtrue;
		}
	}

	// check normal spawn functions
	for ( s=spawns ; s->name ; s++ ) {
		if ( !strcmp(s->name, ent->classname) ) {
			// found it
			s->spawn(ent);
			return qtrue;
		}
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_game), "%s doesn't have a spawn function\n", ent->classname);
	G_TrackMissingSpawn( ent->classname );
	return qfalse;
}

/*
=============
G_NewString

Builds a copy of the string, translating \n to real linefeeds
so message texts can be multi-line
=============
*/
char *G_NewString( const char *string ) {
	char	*newb, *new_p;
	int		i,l;

	l = strlen(string) + 1;

	newb = G_Alloc( l );

	new_p = newb;

	// turn \n into a real linefeed
	for ( i=0 ; i< l ; i++ ) {
		if (string[i] == '\\' && i < l-1) {
			i++;
			if (string[i] == 'n') {
				*new_p++ = '\n';
			} else {
				*new_p++ = '\\';
			}
		} else {
			*new_p++ = string[i];
		}
	}

	return newb;
}




/*
===============
G_ParseField

Takes a key/value pair and sets the binary values
in a gentity
===============
*/
void G_ParseField( const char *key, const char *value, gentity_t *ent ) {
	field_t	*f;
	byte	*b;
	float	v;
	vec3_t	vec;

	for ( f=fields ; f->name ; f++ ) {
		if ( !Q_stricmp(f->name, key) ) {
			// found it
			b = (byte *)ent;

			switch( f->type ) {
			case F_STRING:
				*(char **)(b+f->ofs) = G_NewString (value);
				break;
			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
				break;
			case F_INT:
				*(int *)(b+f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *)(b+f->ofs) = atof(value);
				break;
			case F_ANGLEHACK:
				v = atof(value);
				((float *)(b+f->ofs))[0] = 0;
				((float *)(b+f->ofs))[1] = v;
				((float *)(b+f->ofs))[2] = 0;
				break;
			}
			return;
		}
	}
}

#define ADJUST_AREAPORTAL() \
	if(ent->s.eType == ET_MOVER) \
	{ \
		trap_LinkEntity(ent); \
		trap_AdjustAreaPortalState(ent, qtrue); \
	}

/*
===================
G_RemapEntity

Replace entity classnames before spawning. Allows programmatic item
substitution without modifying map files.

  BSP entity lump -> parse -> G_RemapEntity -> G_CallSpawn
  e.g. "item_health_mega" becomes "holdable_medkit"
===================
*/
typedef struct {
	const char *from;
	const char *to;
} entityRemap_t;

static entityRemap_t s_entityRemaps[] = {
	/* q3 map remaps */
	{ "item_health_small",					"item_health_5" },
	{ "item_health",						"item_health_25" },
	{ "item_health_large",					"item_health_50" },
	{ "item_health_mega",               	"holdable_medkit" },              /* mega health -> medkit */
	{ "item_armor_shard",               	"item_health_5" },           	  /* armor shard -> 5 health */
	{ "item_invulnerability",				"item_deflector" },

	{ "weapon_chaingun",					"weapon_shotgun" },
	{ "weapon_prox_launcher",				"weapon_grenadelauncher" },
	{ "weapon_bfg",							"weapon_plasmagun" },
	{ "weapon_nailgun",						"weapon_lightning" },

	{ "ammo_belt",							"ammo_shells" },
	{ "ammo_mines",							"ammo_grenades" },
	{ "ammo_nails",							"ammo_lightning" },
	{ "ammo_bfg",							"ammo_cells" },

	/* q1 map remaps */
	{ "q1_item_artifact_super_damage",		"item_quad" },                    /* q1 quad damage -> quad damage */
	{ "q1_item_artifact_invulnerability",	"item_enviro" },                  /* q1 invulnerability -> environment suit */
	{ "q1_item_artifact_envirosuit",     	"item_enviro" },                  /* q1 environment suit armor -> environment suit */

	{ "q1_item_armor1",                  	"item_armor_jacket" },            /* q1 green armor -> jacket armor */
	{ "q1_item_armor2",                  	"item_armor_combat" },            /* q1 yellow armor -> combat armor */

	{ "q1_item_shells",                 	 "ammo_shells" },                 /* q1 shells -> shells ammo */
	{ "q1_item_spikes",                  	 "ammo_shells" },                 /* q1 spikes/nails -> shells ammo */
	{ "q1_item_rockets",                 	 "ammo_rockets" },                /* q1 rockets -> rockets ammo */

	{ "q1_weapon_supershotgun",          	 "weapon_shotgun" },              /* q1 super shotgun -> shotgun */
	{ "q1_weapon_supernailgun",          	 "weapon_shotgun" },              /* q1 super nailgun -> shotgun */
	{ "q1_weapon_nailgun",               	 "weapon_shotgun" },              /* q1 nailgun -> shotgun */
	{ "q1_weapon_grenadelauncher",       	 "weapon_grenadelauncher" },      /* q1 grenade launcher -> grenade launcher */
	{ "q1_weapon_rocketlauncher",        	 "weapon_rocketlauncher" },       /* q1 rocket launcher -> rocket launcher */

	{ "q1_info_player_coop",             	 "info_player_deathmatch" },      /* q1 coop spawn -> deathmatch spawn */

	{ "q1_item_health",                  	 "item_health_25" },              /* q1 base health -> health */
	{ "q1_item_health_mega",             	 "holdable_medkit" },             /* q1 megahealth -> own spawn (250 cap) */

	{ NULL, NULL }
};

static void G_RemapEntity( gentity_t *ent ) {
	entityRemap_t *r;

	if ( !ent->classname ) {
		return;
	}

	for ( r = s_entityRemaps; r->from; r++ ) {
		if ( !Q_stricmp( ent->classname, r->from ) ) {
			Com_Log( SEV_TRACE, LOG_CH(ch_game), "Entity remap: %s -> %s\n", r->from, r->to );
			ent->classname = (char *)r->to;
			return;
		}
	}
}


/*
===================
G_SpawnGEntityFromSpawnVars

Spawn an entity and fill in all of the level fields from
level.spawnVars[], then call the class specific spawn function
===================
*/
void G_SpawnGEntityFromSpawnVars( void ) {
	int			i;
	gentity_t	*ent;
	char		*s, *value;

	// get the next free entity
	ent = G_Spawn();

	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], ent );
	}

	// check for "notsingle" flag
	if ( g_gameflags.integer & GF_CAMPAIGN ) {
		G_SpawnInt( "notsingle", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	}
	// check for "notteam" flag (GT_DEATHMATCH, GT_DUEL, GT_KINGOFTHEHILL)
	if ( g_gametype.integer >= GT_TDM ) {
		G_SpawnInt( "notteam", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	} else {
		G_SpawnInt( "notfree", "0", &i );
		if ( i ) {
			ADJUST_AREAPORTAL();
			G_FreeEntity( ent );
			return;
		}
	}

// #if FEAT_TA_UI
// 	G_SpawnInt( "notta", "0", &i );
// 	if ( i ) {
// 		ADJUST_AREAPORTAL();
// 		G_FreeEntity( ent );
// 		return;
// 	}
// #else
// 	G_SpawnInt( "notq3a", "0", &i );
// 	if ( i ) {
// 		ADJUST_AREAPORTAL();
// 		G_FreeEntity( ent );
// 		return;
// 	}
// #endif

	if( G_SpawnString( "gametype", NULL, &value ) ) {
		if( g_gametype.integer >= GT_DEATHMATCH && g_gametype.integer < GT_MAX_GAME_TYPE ) {
			s = strstr( value, bg_gametypelist[g_gametype.integer].shortname );
			if( !s ) {
				ADJUST_AREAPORTAL();
				G_FreeEntity( ent );
				return;
			}
		}
	}

	// move editor origin to pos
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	// entity remap: replace items before spawning
	G_RemapEntity( ent );

	// if we didn't get a classname, don't bother spawning anything
	if ( !G_CallSpawn( ent ) ) {
		G_FreeEntity( ent );
	}
}



/*
====================
G_AddSpawnVarToken
====================
*/
char *G_AddSpawnVarToken( const char *string ) {
	int		l;
	char	*dest;

	l = strlen( string );
	if ( level.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS ) {
		Com_Terminate( TERM_CLIENT_DROP, "G_AddSpawnVarToken: MAX_SPAWN_VARS_CHARS" );
	}

	dest = level.spawnVarChars + level.numSpawnVarChars;
	memcpy( dest, string, l+1 );

	level.numSpawnVarChars += l + 1;

	return dest;
}

/*
====================
G_ParseSpawnVars

Parses a brace bounded set of key / value pairs out of the
level's entity strings into level.spawnVars[]

This does not actually spawn an entity.
====================
*/
qboolean G_ParseSpawnVars( void ) {
	char		keyname[MAX_TOKEN_CHARS];
	char		com_token[MAX_TOKEN_CHARS];

	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;

	// parse the opening brace
	if ( !trap_GetEntityToken( com_token, sizeof( com_token ) ) ) {
		// end of spawn string
		return qfalse;
	}
	if ( com_token[0] != '{' ) {
		Com_Terminate( TERM_CLIENT_DROP, "G_ParseSpawnVars: found %s when expecting {",com_token );
	}

	// go through all the key / value pairs
	while ( 1 ) {
		// parse key
		if ( !trap_GetEntityToken( keyname, sizeof( keyname ) ) ) {
			Com_Terminate( TERM_CLIENT_DROP, "G_ParseSpawnVars: EOF without closing brace" );
		}

		if ( keyname[0] == '}' ) {
			break;
		}

		// parse value
		if ( !trap_GetEntityToken( com_token, sizeof( com_token ) ) ) {
			Com_Terminate( TERM_CLIENT_DROP, "G_ParseSpawnVars: EOF without closing brace" );
		}

		if ( com_token[0] == '}' ) {
			Com_Terminate( TERM_CLIENT_DROP, "G_ParseSpawnVars: closing brace without data" );
		}
		if ( level.numSpawnVars == MAX_SPAWN_VARS ) {
			Com_Terminate( TERM_CLIENT_DROP, "G_ParseSpawnVars: MAX_SPAWN_VARS" );
		}
		level.spawnVars[ level.numSpawnVars ][0] = G_AddSpawnVarToken( keyname );
		level.spawnVars[ level.numSpawnVars ][1] = G_AddSpawnVarToken( com_token );
		level.numSpawnVars++;
	}

	return qtrue;
}



/*QUAKED worldspawn (0 0 0) ?

Every map should have exactly one worldspawn.
"music"		music wav file
"gravity"	800 is default gravity
"message"	Text to print during connection process
*/
void SP_worldspawn( void ) {
	char	*s;

	G_SpawnString( "classname", "", &s );
	if ( Q_stricmp( s, "worldspawn" ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "SP_worldspawn: The first entity isn't 'worldspawn'" );
	}

	// make some data visible to connecting client
	trap_SetConfigstring( CS_GAME_VERSION, GAME_VERSION );

	trap_SetConfigstring( CS_LEVEL_START_TIME, va("%i", level.startTime ) );

	G_SpawnString( "music", "", &s );
	trap_SetConfigstring( CS_MUSIC, s );

	G_SpawnString( "message", "", &s );
	trap_SetConfigstring( CS_MESSAGE, s );				// map specific message

	trap_SetConfigstring( CS_MOTD, g_motd.string );		// message of the day

	G_SpawnString( "gravity", "800", &s );
	trap_Cvar_Set( "g_envGravity", s );

	G_SpawnString( "weather", "", &s );
	trap_Cvar_Set( "g_envWeather", s );

	G_SpawnString( "temperature", "20", &s );
	trap_Cvar_Set( "g_envTemperature", s );

	G_SpawnString( "enableDust", "0", &s );
	trap_Cvar_Set( "g_envGroundDusty", s );

	G_SpawnString( "enableBreath", "0", &s );
	if ( atoi ( s ) ) {
		trap_Cvar_Set( "g_envTemperature", "0" );
	}

	g_entities[ENTITYNUM_WORLD].s.number = ENTITYNUM_WORLD;
	g_entities[ENTITYNUM_WORLD].r.ownerNum = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_WORLD].classname = "worldspawn";

	g_entities[ENTITYNUM_NONE].s.number = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_NONE].r.ownerNum = ENTITYNUM_NONE;
	g_entities[ENTITYNUM_NONE].classname = "nothing";

	// see if we want a warmup time
	trap_SetConfigstring( CS_WARMUP, "" );
	if ( g_restarted.integer ) {
		trap_Cvar_Set( "g_restarted", "0" );
		level.warmupTime = 0;
	} else if ( g_minPlayers.integer ) { // Turn it on
		level.warmupTime = -1;
		trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
		G_LogPrintf( "Warmup:\n" );
	}

}


/*
==============
G_SpawnEntitiesFromString

Parses textual entity definitions out of an entstring and spawns gentities.
==============
*/
void G_SpawnEntitiesFromString( void ) {
	// allow calls to G_Spawn*()
	level.spawning = qtrue;
	level.numSpawnVars = 0;

	// the worldspawn is not an actual entity, but it still
	// has a "spawn" function to perform any global setup
	// needed by a level (setting configstrings or cvars, etc)
	if ( !G_ParseSpawnVars() ) {
		Com_Terminate( TERM_CLIENT_DROP, "SpawnEntities: no entities" );
	}
	SP_worldspawn();

	// parse ents
	while( G_ParseSpawnVars() ) {
		G_SpawnGEntityFromSpawnVars();
	}

	if ( g_missingSpawnCount > 0 ) {
		int i;
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Missing spawn functions (%d unique classnames):\n", g_missingSpawnCount );
		for ( i = 0; i < g_missingSpawnCount; i++ ) {
			Com_Log( SEV_INFO, LOG_CH(ch_game), "  %-40s  count=%d\n",
			         g_missingSpawns[i].name, g_missingSpawns[i].count );
		}
		g_missingSpawnCount = 0;
	}

	level.spawning = qfalse;			// any future calls to G_Spawn*() will be errors
}
