// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// bg_misc.c -- both games misc functions, all completely stateless

#include "../qcommon/q_shared.h"
#include "bg_public.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );


float BG_GetArmorProtection( int armorClass ) {
	switch (armorClass) {
		case ARM_HEAVY:
			return 0.75f;

		case ARM_COMBAT:
			return 0.66f;

		case ARM_JACKET:
			return 0.50f;
	}

	return 0.00f;
}

int BG_GetEffectiveHealth( int health, int armorClass, int armor ) {
	float protection = BG_GetArmorProtection(armorClass);

	return MIN( health / ( 1 - protection ), health + armor );
}

void BG_GetColorForAmount( int amount, vec4_t hcolor ) {
	float t = Com_Clamp(0, 100, amount) / 100.0f;

	hcolor[0] = 1;
	hcolor[1] = MIN(t * 2, 1.0);
	hcolor[2] = MAX(t * 2 - 1.0, 0);
	hcolor[3] = 1;
}

/*QUAKED item_***** ( 0 0 0 ) (-16 -16 -16) (16 16 16) suspended
DO NOT USE THIS CLASS, IT JUST HOLDS GENERAL INFORMATION.
The suspended flag will allow items to hang in the air, otherwise they are dropped to the next surface.

If an item is the target of another entity, it will not spawn in until fired.

An item fires all of its targets when it is picked up.  If the toucher can't carry it, the targets won't be fired.

"notfree" if set to 1, don't spawn in free for all games
"notteam" if set to 1, don't spawn in team games
"notsingle" if set to 1, don't spawn in single player games
"wait"	override the default wait before respawning.  -1 = never respawn automatically, which can be used with targeted spawning.
"random" random number of plus or minus seconds varied from the respawn time
"count" override quantity or duration on most items.
*/

#define Q1_ITEM_Z	24.0f	/* Z offset applied when spawning from a Q1 BSP */

gitem_t	bg_itemlist[] =
{
	{
        NULL,
		NULL,
		{ NULL,
		NULL,
		NULL, NULL} ,
	0.0f,
/* icon */		NULL,
/* pickup */	NULL,
		0,
		0,
		0,
/* precache */ "",
/* sounds */ ""
	},	// leave index 0 alone

	//
	// ARMOR
	//

/*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_armor_jacket",
		"sound/misc/ar2_pkup.opus",
        { "models/powerups/armor/armor_grn.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"icons/iconr_green",
/* pickup */	"Jacket Armor",
		100,
		IT_ARMOR,
        ARM_JACKET,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_armor_combat",
		"sound/misc/ar2_pkup.opus",
        { "models/powerups/armor/armor_yel.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"icons/iconr_yellow",
/* pickup */	"Combat Armor",
		150,
		IT_ARMOR,
        ARM_COMBAT,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_armor_body",
		"sound/misc/ar2_pkup.opus",
        { "models/powerups/armor/armor_red.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"icons/iconr_red",
/* pickup */	"Heavy Armor",
        200,
		IT_ARMOR,
        ARM_HEAVY,
/* precache */ "",
/* sounds */ ""
	},

	//
	// health
	//
/*QUAKED item_health_5 (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_health_5",
		"sound/items/s_health.opus",
        { "models/powerups/health/small_cross.md3",
		"models/powerups/health/small_sphere.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"items/health_5/icon",
/* pickup */	"5 Health",
		5,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_health_25 (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_health_25",
		"sound/items/n_health.opus",
        { "models/powerups/health/medium_cross.md3",
		"models/powerups/health/medium_sphere.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"items/health_25/icon",
/* pickup */	"25 Health",
		25,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_health_50 (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_health_50",
		"sound/items/l_health.opus",
        { "models/powerups/health/large_cross.md3",
		"models/powerups/health/large_sphere.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"items/health_50/icon",
/* pickup */	"50 Health",
		50,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},

	//
	// WEAPONS
	//

/*QUAKED weapon_gauntlet (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_gauntlet",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/gauntlet/gauntlet.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/gauntlet/icon",
/* pickup */	"Gauntlet",
		0,
		IT_WEAPON,
		WP_GAUNTLET,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_shotgun",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/shotgun/shotgun.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/shotgun/icon",
/* pickup */	"Shotgun",
		10,
		IT_WEAPON,
		WP_SHOTGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_machinegun",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/machinegun/machinegun.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/machinegun/icon",
/* pickup */	"Machinegun",
		40,
		IT_WEAPON,
		WP_MACHINEGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_grenadelauncher",
		"sound/misc/w_pkup.opus",
        // { "models/weapons/proxmine/proxmine.md3",
		{ "models/weapons2/grenadel/grenadel.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/grenade_launcher/icon",
/* pickup */	"Grenade Launcher",
		10,
		IT_WEAPON,
		WP_GRENADE_LAUNCHER,
/* precache */ "",
/* sounds */ "sound/weapons/grenade/hgrenb1a.opus sound/weapons/grenade/hgrenb2a.opus"
	},

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_rocketlauncher",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/rocketl/rocketl.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/rocket_launcher/icon",
/* pickup */	"Rocket Launcher",
		10,
		IT_WEAPON,
		WP_ROCKET_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_lightning",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/lightning/lightning.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/lightning_gun/icon",
/* pickup */	"Lightning Gun",
		100,
		IT_WEAPON,
		WP_LIGHTNING_GUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_railgun",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/railgun/railgun.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/railgun/icon",
/* pickup */	"Railgun",
		10,
		IT_WEAPON,
		WP_RAILGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "weapon_plasmagun",
		"sound/misc/w_pkup.opus",
        { "models/weapons2/plasma/plasma.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/plasma_rifle/icon",
/* pickup */	"Plasma Rifle",
		50,
		IT_WEAPON,
		WP_PLASMA_RIFLE,
/* precache */ "",
/* sounds */ ""
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_shells",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/shotgunam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/shotgun/icon",
/* pickup */	"Shells",
		10,
		IT_AMMO,
		WP_SHOTGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_bullets",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/machinegunam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/machinegun/icon",
/* pickup */	"Bullets",
		50,
		IT_AMMO,
		WP_MACHINEGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_grenades",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/grenadeam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/grenade_launcher/icon",
/* pickup */	"Grenades",
		5,
		IT_AMMO,
		WP_GRENADE_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_cells",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/plasmaam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/plasma_rifle/icon",
/* pickup */	"Plasma Cells",
		30,
		IT_AMMO,
		WP_PLASMA_RIFLE,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_lightning",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/lightningam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/lightning_gun/icon",
/* pickup */	"Lightning Cells",
		60,
		IT_AMMO,
		WP_LIGHTNING_GUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_rockets",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/rocketam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/rocket_launcher/icon",
/* pickup */	"Rockets",
		5,
		IT_AMMO,
		WP_ROCKET_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "ammo_slugs",
		"sound/misc/am_pkup.opus",
        { "models/powerups/ammo/railgunam.md3",
		NULL, NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"weapons/railgun/icon",
/* pickup */	"Slugs",
		10,
		IT_AMMO,
		WP_RAILGUN,
/* precache */ "",
/* sounds */ ""
	},

	//
	// HOLDABLE ITEMS
	//
/*QUAKED holdable_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "holdable_teleporter",
		"sound/items/holdable.opus",
        { "models/powerups/holdable/teleporter.md3",
		NULL, NULL, NULL},
	0.0f,
/* icon */		"icons/teleporter",
/* pickup */	"Teleporter",
		60,
		IT_HOLDABLE,
		HI_TELEPORTER,
/* precache */ "",
/* sounds */ ""
	},
/*QUAKED holdable_medkit (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "holdable_medkit",
		"sound/items/holdable.opus",
        {
		"models/powerups/holdable/medkit.md3",
		"models/powerups/holdable/medkit_sphere.md3",
		NULL, NULL},
	Q1_ITEM_Z,
/* icon */		"icons/medkit",
/* pickup */	"Medkit",
		60,
		IT_HOLDABLE,
		HI_MEDKIT,
/* precache */ "",
/* sounds */ "sound/items/use_medkit.opus"
	},

	//
	// POWERUP ITEMS
	//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_quad",
		"sound/items/quaddamage.opus",
        { "models/powerups/instant/quad.md3",
        "models/powerups/instant/quad_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"icons/quad",
/* pickup */	"Quad Damage",
		30,
		IT_POWERUP,
		PW_QUAD,
/* precache */ "",
/* sounds */ "sound/items/damage2.opus sound/items/damage3.opus"
	},

/*QUAKED item_berserk (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_berserk",
		"sound/items/protect.opus",
		{ "models/powerups/instant/quad.md3",
		"models/powerups/instant/quad_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
	/* icon */		"icons/quad",
	/* pickup */	"Berserk",
		30,
		IT_POWERUP,
		PW_BERSERK,
	/* precache */ "",
	/* sounds */ "sound/items/protect3.opus"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_enviro",
		"sound/items/protect.opus",
        { "models/powerups/instant/enviro.md3",
		"models/powerups/instant/enviro_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"icons/envirosuit",
/* pickup */	"Battle Suit",
		30,
		IT_POWERUP,
		PW_BATTLESUIT,
/* precache */ "",
/* sounds */ "sound/items/airout.opus sound/items/protect3.opus"
	},

/*QUAKED item_haste (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_haste",
		"sound/items/haste.opus",
        { "models/powerups/instant/haste.md3",
		"models/powerups/instant/haste_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"icons/haste",
/* pickup */	"Haste",
		30,
		IT_POWERUP,
		PW_HASTE,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_invis (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_invis",
		"sound/items/invisibility.opus",
        { "models/powerups/instant/invis.md3",
		"models/powerups/instant/invis_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"icons/invis",
/* pickup */	"Invisibility",
		30,
		IT_POWERUP,
		PW_INVIS,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_regen",
		"sound/items/regeneration.opus",
        { "models/powerups/instant/regen.md3",
		"models/powerups/instant/regen_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"icons/regen",
/* pickup */	"Regeneration",
		30,
		IT_POWERUP,
		PW_REGEN,
/* precache */ "",
/* sounds */ "sound/items/regen.opus"
	},

/*QUAKED item_flight (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "item_flight",
		"sound/items/flight.opus",
        { "models/powerups/instant/flight.md3",
		"models/powerups/instant/flight_ring.md3",
		NULL, NULL },
	Q1_ITEM_Z,
/* icon */		"icons/flight",
/* pickup */	"Flight",
		60,
		IT_POWERUP,
		PW_FLIGHT,
/* precache */ "",
/* sounds */ "sound/items/flight.opus"
	},

/*QUAKED team_CTF_redflag (1 0 0) (-16 -16 -16) (16 16 16)
Only in CTF games
*/
	{
        "team_CTF_redflag",
		NULL,
        { "models/flags/r_flag.md3",
		NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/iconf_red1",
/* pickup */	"Red Flag",
		0,
		IT_TEAM,
		PW_REDFLAG,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED team_CTF_blueflag (0 0 1) (-16 -16 -16) (16 16 16)
Only in CTF games
*/
	{
        "team_CTF_blueflag",
		NULL,
        { "models/flags/b_flag.md3",
		NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/iconf_blu1",
/* pickup */	"Blue Flag",
		0,
		IT_TEAM,
		PW_BLUEFLAG,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED holdable_kamikaze (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "holdable_kamikaze",
		"sound/items/holdable.opus",
        { "models/powerups/kamikazi.md3",
		NULL, NULL, NULL},
	0.0f,
/* icon */		"icons/kamikaze",
/* pickup */	"Kamikaze",
		60,
		IT_HOLDABLE,
		HI_KAMIKAZE,
/* precache */ "",
/* sounds */ "sound/items/kamikazerespawn.opus"
	},

#if FEAT_PW_PORTAL
/*QUAKED holdable_portal (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "holdable_portal",
		"sound/items/holdable.opus",
        { "models/powerups/holdable/porter.md3",
		NULL, NULL, NULL},
	0.0f,
/* icon */		"icons/portal",
/* pickup */	"Portal",
		60,
		IT_HOLDABLE,
		HI_PORTAL,
/* precache */ "",
/* sounds */ ""
	},
#endif

/*QUAKED holdable_deflector (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
        "holdable_deflector",
		"sound/items/holdable.opus",
        { "models/powerups/holdable/invulnerability.md3",
		NULL, NULL, NULL},
	0.0f,
/* icon */		"icons/invulnerability",
/* pickup */	"Deflector",
		60,
		IT_HOLDABLE,
		HI_DEFLECTOR,
/* precache */ "",
/* sounds */ ""
	},

	/*QUAKED team_CTF_neutralflag (0 0 1) (-16 -16 -16) (16 16 16)
Only in One Flag CTF games
*/
	{
        "team_CTF_neutralflag",
		NULL,
        { "models/flags/n_flag.md3",
		NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/iconf_neutral1",
/* pickup */	"Neutral Flag",
		0,
		IT_TEAM,
		PW_NEUTRALFLAG,
/* precache */ "",
/* sounds */ ""
	},

#if FEAT_HARVESTER
	{
        "item_redcube",
		"sound/misc/am_pkup.opus",
        { "models/powerups/orb/r_orb.md3",
		NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/iconh_rorb",
/* pickup */	"Red Cube",
		0,
		IT_TEAM,
		0,
/* precache */ "",
/* sounds */ ""
	},

	{
        "item_bluecube",
		"sound/misc/am_pkup.opus",
        { "models/powerups/orb/b_orb.md3",
		NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/iconh_borb",
/* pickup */	"Blue Cube",
		0,
		IT_TEAM,
		0,
/* precache */ "",
/* sounds */ ""
	},
#endif

	// Q1 key items (holdable, not usable — persist in inventory)
	{
        "q1_item_key1",
		"sound/misc/medkitrespawn.opus",
        { "progs/key1.mdl", NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/key_silver",
/* pickup */	"Silver Key",
		0,
		IT_HOLDABLE,
		HI_KEY_SILVER,
/* precache */ "",
/* sounds */ ""
	},

	{
        "q1_item_key2",
		"sound/misc/medkitrespawn.opus",
        { "progs/key2.mdl", NULL, NULL, NULL },
	0.0f,
/* icon */		"icons/key_gold",
/* pickup */	"Gold Key",
		0,
		IT_HOLDABLE,
		HI_KEY_GOLD,
/* precache */ "",
/* sounds */ ""
	},

	// end of list marker
	{NULL}
};

int		bg_numItems = ARRAY_LEN( bg_itemlist ) - 1;

// bg_weaponlist moved to bg_weapons.c

/*
==============
BG_FindItemForPowerup
==============
*/
gitem_t	*BG_FindItemForPowerup( powerup_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( (bg_itemlist[i].giType == IT_POWERUP ||
					bg_itemlist[i].giType == IT_TEAM) &&
			bg_itemlist[i].giTag == pw ) {
			return &bg_itemlist[i];
		}
	}

	return NULL;
}


/*
==============
BG_FindItemForHoldable
==============
*/
gitem_t	*BG_FindItemForHoldable( holdable_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == pw ) {
			return &bg_itemlist[i];
		}
	}

	Com_Terminate( TERM_CLIENT_DROP, "HoldableItem not found" );

	return NULL;
}

qboolean BG_HoldableIsSelectable( holdable_t h ) {
	switch ( h ) {
	case HI_TELEPORTER:
	case HI_MEDKIT:
	case HI_KAMIKAZE:
	case HI_PORTAL:
	case HI_DEFLECTOR:
		return qtrue;
	default:
		return qfalse;
	}
}


/*
===============
BG_FindItemForWeapon

===============
*/
gitem_t	*BG_FindItemForWeapon( weapon_t weapon ) {
	gitem_t	*it;

	for ( it = bg_itemlist + 1 ; it->classname ; it++) {
		if ( it->giType == IT_WEAPON && it->giTag == weapon ) {
			return it;
		}
	}

	Com_Terminate( TERM_CLIENT_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}

/*
===============
BG_FindItem

===============
*/
gitem_t	*BG_FindItem( const char *pickupName ) {
	gitem_t	*it;

	for ( it = bg_itemlist + 1 ; it->classname ; it++ ) {
		if ( !Q_stricmp( it->pickup_name, pickupName ) )
			return it;
	}

	return NULL;
}

/*
============
BG_PlayerTouchesItem

Items can be picked up without actually touching their physical bounds to make
grabbing them easier
============
*/
qboolean	BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime ) {
	vec3_t		origin;

	BG_EvaluateTrajectory( &item->pos, atTime, origin );

	// we are ignoring ducked differences here
	if ( ps->origin[0] - origin[0] > ITEM_PICKUP_SIZE + 8
		|| ps->origin[0] - origin[0] < -(ITEM_PICKUP_SIZE + 14)
		|| ps->origin[1] - origin[1] > ITEM_PICKUP_SIZE
		|| ps->origin[1] - origin[1] < -ITEM_PICKUP_SIZE
		|| ps->origin[2] - origin[2] > ITEM_PICKUP_SIZE
		|| ps->origin[2] - origin[2] < -ITEM_PICKUP_SIZE ) {
		return qfalse;
	}

	return qtrue;
}



/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.
================
*/
qboolean BG_CanItemBeGrabbed( int gametype, const entityState_t *ent, const playerState_t *ps ) {
	gitem_t	*item;

	if ( ent->modelindex < 1 || ent->modelindex >= bg_numItems ) {
		Com_Terminate( TERM_CLIENT_DROP, "BG_CanItemBeGrabbed: index out of range" );
	}

	item = &bg_itemlist[ent->modelindex];

	switch( item->giType ) {
	case IT_WEAPON:
        if (gametype == GT_KINGOFTHEHILL) {
            if (!ps->powerups[PW_KING] && item->giTag > WP_ROCKET_LAUNCHER) {
                return qfalse;
            }
        }

        // always pick up dropped weapons
        if (ent->eFlags & EF_DROPPED_ITEM) {
            return qtrue;
        }

        // if (ps->stats[STAT_WEAPONS] & (1 << item->giTag) && ps->ammo[item->giTag] >= item->quantity) {
        if (ps->stats[STAT_WEAPONS] & (1 << item->giTag) && ps->ammo[item->giTag] >= bg_weaponlist[item->giTag].minAmmunition) {
            return qfalse;
        }

        return qtrue;

	case IT_AMMO:
        if (gametype == GT_KINGOFTHEHILL) {
            if (!ps->powerups[PW_KING] && item->giTag > WP_ROCKET_LAUNCHER) {
                return qfalse;
            }
        }

        if (ps->ammo[item->giTag] >= bg_weaponlist[item->giTag].maxAmmunition) {
            return qfalse;
        }

        return qtrue;

	case IT_ARMOR:
        if (gametype == GT_KINGOFTHEHILL) {
            if (ps->powerups[PW_KING]) {
                return qfalse;
            }
        }

        if (ps->stats[STAT_ARMORCLASS] == ARM_NONE && item->giTag == ARM_NONE) {
            return qfalse;
        }

        if (item->giTag != ARM_NONE && ps->stats[STAT_ARMORCLASS] >= item->giTag && ps->stats[STAT_ARMOR] >= item->quantity) {
            return qfalse;
        }

        if (ps->stats[STAT_ARMOR] >= MAX_ARMOR) {
            return qfalse;
        }

        return qtrue;

	case IT_HEALTH:
		if ( ps->stats[STAT_HEALTH] >= MAX_HEALTH ) {
			return qfalse;
		}

		return qtrue;

	case IT_POWERUP:
		return qtrue;	// powerups are always picked up

	case IT_TEAM: // team items, such as flags
		if( gametype == GT_1FCTF ) {
			// neutral flag can always be picked up
			if( item->giTag == PW_NEUTRALFLAG ) {
				return qtrue;
			}
			if (ps->persistant[PERS_TEAM] == TEAM_RED) {
				if (item->giTag == PW_BLUEFLAG  && ps->powerups[PW_NEUTRALFLAG] ) {
					return qtrue;
				}
			} else if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
				if (item->giTag == PW_REDFLAG  && ps->powerups[PW_NEUTRALFLAG] ) {
					return qtrue;
				}
			}
		}
		if( gametype == GT_CTF ) {
			// ent->modelindex2 is non-zero on items if they are dropped
			// we need to know this because we can pick up our dropped flag (and return it)
			// but we can't pick up our flag at base
			if (ps->persistant[PERS_TEAM] == TEAM_RED) {
				if (item->giTag == PW_BLUEFLAG ||
					(item->giTag == PW_REDFLAG && ent->modelindex2) ||
					(item->giTag == PW_REDFLAG && ps->powerups[PW_BLUEFLAG]) )
					return qtrue;
			} else if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
				if (item->giTag == PW_REDFLAG ||
					(item->giTag == PW_BLUEFLAG && ent->modelindex2) ||
					(item->giTag == PW_BLUEFLAG && ps->powerups[PW_REDFLAG]) )
					return qtrue;
			}
		}

#if FEAT_HARVESTER
		if( gametype == GT_HARVESTER ) {
			return qtrue;
		}
#endif
		return qfalse;

	case IT_HOLDABLE:
		if ( ps->stats[STAT_HOLDABLE_BITS] & BG_HOLDABLE_BIT(item->giTag) ) {
			return qfalse;
		}
		return qtrue;

        case IT_BAD:
            Com_Terminate( TERM_CLIENT_DROP, "BG_CanItemBeGrabbed: IT_BAD" );
        default:
#ifndef NDEBUG
          Com_Log( SEV_INFO, LOG_CH(ch_game), "BG_CanItemBeGrabbed: unknown enum %d\n", item->giType );
#endif
         break;
	}

	return qfalse;
}

//======================================================================

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result ) {
	float		deltaTime;
	float		phase;
    vec3_t		dir;
    vec3_t		friction;
    float		angle;

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorCopy( tr->trBase, result );
		break;
	case TR_LINEAR:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		deltaTime = fmod( deltaTime, 1.0f );  // prevent float precision loss on long uptimes
		phase = sin( deltaTime * M_PI * 2 );
		VectorMA( tr->trBase, phase, tr->trDelta, result );
		break;
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		if ( deltaTime < 0 ) {
			deltaTime = 0;
		}
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
		break;
    case TR_GRAVITY_DOUBLE:
        deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        result[2] -= 0.25 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
        break;
    case TR_ACCELERATE:
        // time since missile fired in seconds
        deltaTime = (atTime - tr->trTime) * 0.001;

        // the .5*a*t^2 part. trDuration = acceleration,
        // phase gives the magnitude of the distance
        // we need to move
        phase = (tr->trDuration / 2) * (deltaTime * deltaTime);

        // Make dir equal to the velocity of the object
        VectorCopy(tr->trDelta, dir);

        // Sets the magnitude of vector dir to 1
        VectorNormalize(dir);

        // Move a distance "phase" in the direction "dir"
        // from our starting point
        VectorMA(tr->trBase, phase, dir, result);

        // The u*t part. Adds the velocity of the object
        // multiplied by the time to the last result.
        VectorMA(result, deltaTime, tr->trDelta, result);
        break;
        // eser - accelerate
        //NT - added small gravity trajectory type
    case TR_SMALL_GRAVITY:
        deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        VectorScale(result, 0.02 * deltaTime * deltaTime, friction);
        VectorSubtract(result, friction, result);
        result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
        break;
        //NT - added orbital trajectory type
    case TR_ORBITAL:
        deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);

        angle = (float)((atTime + tr->trDuration) % 360) * M_PI / 180;
        result[0] += cos(angle) * 48;
        result[1] += sin(angle) * 48;

        angle = (float)((atTime / 6 + tr->trDuration) % 360) * M_PI / 180;

        result[2] += sin(angle) * 32;
        break;
	default:
		Com_Terminate( TERM_CLIENT_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trType );
		break;
	}
}

/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result ) {
	float	deltaTime;
	float	phase;
    vec3_t	dir;

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear( result );
		break;
	case TR_LINEAR:
		VectorCopy( tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		deltaTime = fmod( deltaTime, 1.0f );  // prevent float precision loss on long uptimes
		phase = cos( deltaTime * M_PI * 2 );	// derivative of sin = cos
		phase *= 0.5;
		VectorScale( tr->trDelta, phase, result );
		break;
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			VectorClear( result );
			return;
		}
		VectorCopy( tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorCopy( tr->trDelta, result );
		result[2] -= DEFAULT_GRAVITY * deltaTime;		// FIXME: local gravity...
		break;
    case TR_GRAVITY_DOUBLE:
        deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
        VectorCopy(tr->trDelta, result);
        result[2] -= (DEFAULT_GRAVITY / 2) * deltaTime;		// FIXME: local gravity...
        break;
    case TR_ACCELERATE:
        // time since missile fired in seconds
        deltaTime = (atTime - tr->trTime) * 0.001;

        // Turn magnitude of acceleration into a vector
        VectorCopy(tr->trDelta, dir);
        VectorNormalize(dir);
        VectorScale(dir, tr->trDuration, dir);

        // u + t * a = v
        VectorMA(tr->trDelta, deltaTime, dir, result);
        break;
    case TR_SMALL_GRAVITY:
        deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
        VectorCopy(tr->trDelta, result);
        result[2] -= DEFAULT_GRAVITY * deltaTime;		// FIXME: local gravity...
        break;
    case TR_ORBITAL:
        // I really don't care to calculate angular velocity
        VectorCopy(tr->trDelta, result);
        break;
	default:
		Com_Terminate( TERM_CLIENT_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trType );
		break;
	}
}

char *eventnames[] = {
	"EV_NONE",

	"EV_FOOTSTEP",
	"EV_FOOTSTEP_METAL",
	"EV_FOOTSPLASH",
	"EV_FOOTWADE",
	"EV_SWIM",

	"EV_STEP_4",
	"EV_STEP_8",
	"EV_STEP_12",
	"EV_STEP_16",

	"EV_FALL_SHORT",
	"EV_FALL_MEDIUM",
	"EV_FALL_FAR",

	"EV_JUMP_PAD",			// boing sound at origin", jump sound on player

	"EV_JUMP",
	"EV_WATER_TOUCH",	// foot touches
	"EV_WATER_LEAVE",	// foot leaves
	"EV_WATER_UNDER",	// head touches
	"EV_WATER_CLEAR",	// head leaves

	"EV_ITEM_PICKUP",			// normal item pickups are predictable
	"EV_GLOBAL_ITEM_PICKUP",	// powerup / team sounds are broadcast to everyone

	"EV_NOAMMO",
	"EV_CHANGE_WEAPON",
	"EV_FIRE_WEAPON_PRI",
	"EV_FIRE_WEAPON_SEC",

	"EV_USE_ITEM0",
	"EV_USE_ITEM1",
	"EV_USE_ITEM2",
	"EV_USE_ITEM3",
	"EV_USE_ITEM4",
	"EV_USE_ITEM5",
	"EV_USE_ITEM6",
	"EV_USE_ITEM7",
	"EV_USE_ITEM8",
	"EV_USE_ITEM9",
	"EV_USE_ITEM10",
	"EV_USE_ITEM11",
	"EV_USE_ITEM12",
	"EV_USE_ITEM13",
	"EV_USE_ITEM14",
	"EV_USE_ITEM15",

	"EV_ITEM_RESPAWN",
	"EV_ITEM_POP",
	"EV_PLAYER_TELEPORT_IN",
	"EV_PLAYER_TELEPORT_OUT",

	"EV_GRENADE_BOUNCE",		// eventParm will be the soundindex

	"EV_GENERAL_SOUND",
	"EV_GLOBAL_SOUND",		// no attenuation
	"EV_GLOBAL_TEAM_SOUND",

	"EV_BULLET_HIT_FLESH",
	"EV_BULLET_HIT_WALL",

	"EV_MISSILE_HIT",
	"EV_MISSILE_MISS",
	"EV_MISSILE_MISS_METAL",
	"EV_RAILTRAIL",
	"EV_SHOTGUN",
	"EV_SHOTGUN_WIDE",
	"EV_BULLET",				// otherEntity is the shooter
// eser - lightning discharge
	"EV_LIGHTNING_DISCHARGE",
// eser - lightning discharge

	"EV_PAIN",
	"EV_DEATH1",
	"EV_DEATH2",
	"EV_DEATH3",
	"EV_OBITUARY",

	"EV_POWERUP_QUAD",
	"EV_POWERUP_BERSERK",
	"EV_POWERUP_BATTLESUIT",
	"EV_POWERUP_REGEN",

	"EV_GIB_PLAYER",			// gib a previously living player
	"EV_SCOREPLUM",			// score plum
#if FEAT_DAMAGE_PLUMS
	"EV_DAMAGEPLUM",			// floating damage number (attacker-only)
#endif
#if FEAT_PING_LOCATION
	"EV_PING_LOCATION",			// team coordination ping (4G)
#endif
#if FEAT_FREEZETAG
	"EV_FREEZE",				// player frozen (7A)
#endif

//#ifdef MISSIONPACK
	"EV_KAMIKAZE",				// kamikaze explodes
	"EV_OBELISKEXPLODE",		// obelisk explodes
	"EV_OBELISKPAIN",			// obelisk pain
	"EV_DEFLECTOR_IMPACT",		// deflector sphere impact
	"EV_DEFLECTOR_JUICED",		// deflector juiced effect
	"EV_LIGHTNINGBOLT",		// lightning bolt bounced of deflector sphere
	"EV_LIGHTNING_ARC",		// chain arc beam from primary target to secondary target
//#endif

	"EV_DEBUG_LINE",
	"EV_STOPLOOPINGSOUND",
	"EV_TAUNT",
	"EV_TAUNT_YES",
	"EV_TAUNT_NO",
	"EV_TAUNT_FOLLOWME",
	"EV_TAUNT_GETFLAG",
	"EV_TAUNT_GUARDBASE",
	"EV_TAUNT_PATROL"

#if FEAT_EARTHQUAKE_SYSTEM
	,"EV_EARTHQUAKE"
#endif
};

// A3: compile-time check — event name table must match entity_event_t enum
typedef char _event_table_size_check[
	(sizeof(eventnames) / sizeof(eventnames[0]) == EV_NUM_ENTITY_EVENTS) ? 1 : -1
];

/*
===============
BG_AddPredictableEventToPlayerstate

Handles the sequence numbers
===============
*/

void	trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

void BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps ) {

#ifdef _DEBUG
	{
		char buf[256];
		trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));
		if ( atof(buf) != 0 ) {
#ifdef QAGAME
			Com_Log( SEV_INFO, LOG_CH(ch_game), " game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#else
			Com_Log( SEV_INFO, LOG_CH(ch_game), "Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#endif
		}
	}
#endif
	ps->events[ps->eventSequence & (MAX_PS_EVENTS-1)] = newEvent;
	ps->eventParms[ps->eventSequence & (MAX_PS_EVENTS-1)] = eventParm;
	ps->eventSequence++;
}

/*
========================
BG_TouchJumpPad
========================
*/
void BG_TouchJumpPad( playerState_t *ps, entityState_t *jumppad ) {
	vec3_t	angles;
	float p;
	int effectNum;

	// spectators don't use jump pads
	if ( ps->pm_type != PM_NORMAL ) {
		return;
	}

	// flying characters don't hit bounce pads
	if ( ps->powerups[PW_FLIGHT] ) {
		return;
	}

	// if we didn't hit this same jumppad the previous frame
	// then don't play the event sound again if we are in a fat trigger
	if ( ps->jumppad_ent != jumppad->number ) {

		vectoangles( jumppad->origin2, angles);
		p = fabs( AngleNormalize180( angles[PITCH] ) );
		if( p < 45 ) {
			effectNum = 0;
		} else {
			effectNum = 1;
		}
		BG_AddPredictableEventToPlayerstate( EV_JUMP_PAD, effectNum, ps );
	}
	// remember hitting this jumppad this frame
	ps->jumppad_ent = jumppad->number;
	ps->jumppad_frame = ps->pmove_framecount;
	// give the player the velocity from the jumppad
	VectorCopy( jumppad->origin2, ps->velocity );
}

/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap ) {
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR ) {
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction
	VectorCopy( ps->velocity, s->pos.trDelta );

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = PW_NONE + 1 ; i < PW_NUM_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->loopSound = ps->loopSound;
	s->generic1 = ps->generic1;
}

/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap ) {
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR ) {
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_LINEAR_STOP;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction and linear prediction
	VectorCopy( ps->velocity, s->pos.trDelta );
	// set the time for linear prediction
	s->pos.trTime = time;
	// set maximum extra polation time
	s->pos.trDuration = 50; // 1000 / sv_fps (default = 20)

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = PW_NONE + 1 ; i < PW_NUM_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->loopSound = ps->loopSound;
	s->generic1 = ps->generic1;
}
