// bg_weapons.c -- weapon and attack definition tables (shared by cgame, game, ui)

#include "../qcommon/q_shared.h"
#include "bg_public.h"

// Attack types — indexed by attackType_t
gattack_t	bg_attacklist[] =
{
	// ATT_NONE
	{
		/* name                 */ "None",
		/* shortname            */ "",
		/* weapon				*/ WP_NONE,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 0,
		/* selfKnockbackScale   */ 0,
		/* recoilKick           */ 0,
		/* reloadTime           */ 0,
		/* meansOfDeath         */ MOD_UNKNOWN,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_GAUNTLET_PRIMARY
	{
		/* name                 */ "Gauntlet Primary",
		/* shortname            */ "g1",
		/* weapon				*/ WP_GAUNTLET,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 2.5f,
		/* selfKnockbackScale   */ 2.5f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 400,
		/* meansOfDeath         */ MOD_GAUNTLET,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_GAUNTLET_LUNGE
	{
		/* name                 */ "Gauntlet Lunge",
		/* shortname            */ "g2",
		/* weapon				*/ WP_GAUNTLET,
		/* maxDamageDistance    */ 80.0f,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 2.5f,
		/* selfKnockbackScale   */ 2.5f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 1500,
		/* meansOfDeath         */ MOD_GAUNTLET_LUNGE,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
#if defined(CGAME) || defined(QAGAME)
		/* onAltFireStart       */ PM_Gauntlet_Lunge_Start,
		/* onAltFireThink       */ PM_Gauntlet_Lunge_Think,
		/* onAltFireRelease     */ PM_Gauntlet_Lunge_Release,
#else
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
#endif
	},

	// ATT_MACHINEGUN_PRIMARY
	{
		/* name                 */ "Machinegun Primary",
		/* shortname            */ "mg1",
		/* weapon				*/ WP_MACHINEGUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 6.75f,
		/* selfKnockbackScale   */ 6.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 100,
		/* meansOfDeath         */ MOD_MACHINEGUN,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_MACHINEGUN_BURST
	{
		/* name                 */ "Machinegun Burst",
		/* shortname            */ "mg2",
		/* weapon				*/ WP_MACHINEGUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 6.75f,
		/* selfKnockbackScale   */ 6.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 400,
		/* meansOfDeath         */ MOD_MACHINEGUN_BURST,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
#if defined(CGAME) || defined(QAGAME)
		/* onAltFireStart       */ PM_MG_Burst_Start,
		/* onAltFireThink       */ PM_MG_Burst_Think,
		/* onAltFireRelease     */ PM_MG_Burst_Release,
#else
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
#endif
	},

	// ATT_SHOTGUN_PRIMARY
	{
		/* name                 */ "Shotgun Primary",
		/* shortname            */ "sg1",
		/* weapon				*/ WP_SHOTGUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 6.75f,
		/* selfKnockbackScale   */ 6.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 1000,
		/* meansOfDeath         */ MOD_SHOTGUN,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_SHOTGUN_DOUBLE_BLAST
	{
		/* name                 */ "Shotgun Double Blast",
		/* shortname            */ "sg2",
		/* weapon				*/ WP_SHOTGUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 6.75f,
		/* selfKnockbackScale   */ 6.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 1500,
		/* meansOfDeath         */ MOD_SHOTGUN_DOUBLE_BLAST,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
#if defined(CGAME) || defined(QAGAME)
		/* onAltFireStart       */ PM_SG_DoubleBlast_Start,
		/* onAltFireThink       */ PM_SG_DoubleBlast_Think,
		/* onAltFireRelease     */ PM_SG_DoubleBlast_Release,
#else
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
#endif
	},

	// ATT_GRENADE_LAUNCHER_PRIMARY
	{
		/* name                 */ "Grenade Launcher Primary",
		/* shortname            */ "gl1",
		/* weapon				*/ WP_GRENADE_LAUNCHER,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 6.0f,
		/* selfKnockbackScale   */ 5.0f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 600,
		/* meansOfDeath         */ MOD_GRENADE,
		/* splashMeansOfDeath   */ MOD_GRENADE_SPLASH,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_ROCKET_LAUNCHER_PRIMARY
	{
		/* name                 */ "Rocket Launcher Primary",
		/* shortname            */ "rl1",
		/* weapon				*/ WP_ROCKET_LAUNCHER,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 6.0f,
		/* selfKnockbackScale   */ 5.0f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 800,
		/* meansOfDeath         */ MOD_ROCKET,
		/* splashMeansOfDeath   */ MOD_ROCKET_SPLASH,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_ROCKET_LAUNCHER_MORTAR
	{
		/* name                 */ "Rocket Launcher Mortar",
		/* shortname            */ "rl2",
		/* weapon				*/ WP_ROCKET_LAUNCHER,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 10.0f,
		/* selfKnockbackScale   */ 8.0f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 800,
		/* meansOfDeath         */ MOD_ROCKET_MORTAR,
		/* splashMeansOfDeath   */ MOD_ROCKET_MORTAR_SPLASH,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_LIGHTNING_GUN_PRIMARY
	{
		/* name                 */ "Lightning Gun Primary",
		/* shortname            */ "lg1",
		/* weapon				*/ WP_LIGHTNING_GUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 7.75f,
		/* selfKnockbackScale   */ 7.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 50,
		/* meansOfDeath         */ MOD_LIGHTNING,
		/* splashMeansOfDeath   */ MOD_LIGHTNING_DISCHARGE,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_LIGHTNING_GUN_CHAIN_ARC
	{
		/* name                 */ "Lightning Gun Chain Arc",
		/* shortname            */ "lg2",
		/* weapon				*/ WP_LIGHTNING_GUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 3.0f,
		/* selfKnockbackScale   */ 0.0f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 50,
		/* meansOfDeath         */ MOD_LIGHTNING_CHAIN_ARC,
		/* splashMeansOfDeath   */ 0,
#if defined(CGAME) || defined(QAGAME)
		/* onAltFireStart       */ PM_LG_ChainArc_Start,
		/* onAltFireThink       */ PM_LG_ChainArc_Think,
		/* onAltFireRelease     */ PM_LG_ChainArc_Release,
#else
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
#endif
	},

	// ATT_RAILGUN_PRIMARY
	{
		/* name                 */ "Railgun Primary",
		/* shortname            */ "rg1",
		/* weapon				*/ WP_RAILGUN,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qtrue,
		/* knockbackScale       */ 7.75f,
		/* selfKnockbackScale   */ 7.75f,
		/* recoilKick           */ 200.0f,
		/* reloadTime           */ 1000,
		/* meansOfDeath         */ MOD_RAILGUN,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},

	// ATT_PLASMA_RIFLE_PRIMARY
	{
		/* name                 */ "Plasma Rifle Primary",
		/* shortname            */ "pr1",
		/* weapon				*/ WP_PLASMA_RIFLE,
		/* maxDamageDistance    */ 0,
		/* armorPiercing        */ qfalse,
		/* knockbackScale       */ 2.5f,
		/* selfKnockbackScale   */ 2.5f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 1000,
		/* meansOfDeath         */ MOD_PLASMA,
		/* splashMeansOfDeath   */ MOD_UNKNOWN,
		/* onAltFireStart       */ NULL,
		/* onAltFireThink       */ NULL,
		/* onAltFireRelease     */ NULL,
	},
};

int BG_AttackByShortname( const char *shortname ) {
	int count = (int)( sizeof( bg_attacklist ) / sizeof( bg_attacklist[0] ) );
	for ( int i = 0; i < count; i++ ) {
		if ( bg_attacklist[i].shortname[0] && Q_stricmp( bg_attacklist[i].shortname, shortname ) == 0 ) {
			return i;
		}
	}
	return ATT_NONE;
}

// Weapon definitions
gweapon_t	bg_weaponlist[] =
{
	// WP_NONE
	{
		/* name                 */ "None",
		/* shortname            */ "n",

		/* color                */ colorBlack,
		/* switchOnCycle        */ qfalse,
		/* switchOnOutOfAmmo    */ qfalse,

		/* tossOnDeath          */ qfalse,

		/* ammoBox              */ 0,
		/* minAmmunition        */ -1,
		/* maxAmmunition        */ -1,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ -1,

		/* attack               */ ATT_NONE,
		/* attackAlt            */ ATT_NONE,

		/* weight               */ 1.0f
	},

	// WP_GAUNTLET
	{
		/* name                 */ "Gauntlet",
		/* shortname            */ "g",

		/* color                */ colorOrange,
		/* switchOnCycle        */ qfalse,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qfalse,

		/* ammoBox              */ 0,
		/* minAmmunition        */ -1,
		/* maxAmmunition        */ -1,

		/* spawnWeapon          */ qtrue,
		/* spawnAmmunition      */ -1,

		/* attack               */ ATT_GAUNTLET_PRIMARY,
		/* attackAlt            */ ATT_GAUNTLET_LUNGE,

		/* weight               */ 1.0f
	},

	// WP_MACHINEGUN
	{
		/* name                 */ "Machinegun",
		/* shortname            */ "mg",

		/* color                */ colorYellow,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 50,
		/* minAmmunition        */ 50,
		/* maxAmmunition        */ 200,

		/* spawnWeapon          */ qtrue,
		/* spawnAmmunition      */ 100,

		/* attack               */ ATT_MACHINEGUN_PRIMARY,
		/* attackAlt            */ ATT_MACHINEGUN_BURST,

		/* weight               */ 1.0f
	},

	// WP_SHOTGUN
	{
		/* name                 */ "Shotgun",
		/* shortname            */ "sg",

		/* color                */ colorRed,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 20,
		/* minAmmunition        */ 20,
		/* maxAmmunition        */ 80,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ 0,

		/* attack               */ ATT_SHOTGUN_PRIMARY,
		/* attackAlt            */ ATT_SHOTGUN_DOUBLE_BLAST,

		/* weight               */ 1.0f
	},

	// WP_GRENADE_LAUNCHER
	{
		/* name                 */ "Grenade Launcher",
		/* shortname            */ "gl",

		/* color                */ colorGreen,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 10,
		/* minAmmunition        */ 10,
		/* maxAmmunition        */ 50,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ 0,

		/* attack               */ ATT_GRENADE_LAUNCHER_PRIMARY,
		/* attackAlt            */ ATT_NONE,

		/* weight               */ 1.0f
	},

	// WP_ROCKET_LAUNCHER
	{
		/* name                 */ "Rocket Launcher",
		/* shortname            */ "rl",

		/* color                */ colorMdGrey,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 10,
		/* minAmmunition        */ 10,
		/* maxAmmunition        */ 50,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ 0,

		/* attack               */ ATT_ROCKET_LAUNCHER_PRIMARY,
		/* attackAlt            */ ATT_ROCKET_LAUNCHER_MORTAR,

		/* weight               */ 1.0f
	},

	// WP_LIGHTNING_GUN
	{
		/* name                 */ "Lightning Gun",
		/* shortname            */ "lg",

		/* color                */ colorWhite,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 50,
		/* minAmmunition        */ 50,
		/* maxAmmunition        */ 250,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ 0,

		/* attack               */ ATT_LIGHTNING_GUN_PRIMARY,
		/* attackAlt            */ ATT_LIGHTNING_GUN_CHAIN_ARC,

		/* weight               */ 1.0f
	},

	// WP_RAILGUN
	{
		/* name                 */ "Railgun",
		/* shortname            */ "rg",

		/* color                */ colorCyan,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 5,
		/* minAmmunition        */ 5,
		/* maxAmmunition        */ 25,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ 0,

		/* attack               */ ATT_RAILGUN_PRIMARY,
		/* attackAlt            */ ATT_NONE,

		/* weight               */ 1.0f
	},

	// WP_PLASMA_RIFLE
	{
		/* name                 */ "Plasma Rifle",
		/* shortname            */ "pr",

		/* color                */ colorMagenta,
		/* switchOnCycle        */ qtrue,
		/* switchOnOutOfAmmo    */ qtrue,

		/* tossOnDeath          */ qtrue,

		/* ammoBox              */ 5,
		/* minAmmunition        */ 5,
		/* maxAmmunition        */ 25,

		/* spawnWeapon          */ qfalse,
		/* spawnAmmunition      */ 0,

		/* attack               */ ATT_PLASMA_RIFLE_PRIMARY,
		/* attackAlt            */ ATT_NONE,

		/* weight               */ 1.0f
	}
};


/*
====================
BG_ModShortName

Return a short ASCII weapon tag for a means-of-death value.
Used for event encoding and coaching tools.
====================
*/
const char *BG_ModShortName( meansOfDeath_t mod )
{
	switch ( mod ) {
	case MOD_SHOTGUN:               return "sg";
	case MOD_SHOTGUN_DOUBLE_BLAST:  return "sg";
	case MOD_GAUNTLET:              return "gauntlet";
	case MOD_GAUNTLET_LUNGE:        return "gauntlet";
	case MOD_MACHINEGUN:            return "mg";
	case MOD_MACHINEGUN_BURST:      return "mg";
	case MOD_GRENADE:               return "gl";
	case MOD_GRENADE_SPLASH:        return "gl";
	case MOD_ROCKET:                return "rl";
	case MOD_ROCKET_SPLASH:         return "rl";
	case MOD_ROCKET_MORTAR:         return "rl";
	case MOD_ROCKET_MORTAR_SPLASH:  return "rl";
	case MOD_PLASMA:                return "pg";
	case MOD_RAILGUN:               return "rg";
	case MOD_LIGHTNING:             return "lg";
	case MOD_LIGHTNING_DISCHARGE:   return "lg";
	case MOD_LIGHTNING_CHAIN_ARC:   return "lg";
	case MOD_GRAPPLE:               return "grapple";
	case MOD_TELEFRAG:              return "telefrag";
	case MOD_FALLING:               return "falling";
	case MOD_SUICIDE:               return "suicide";
	case MOD_WATER:                 return "water";
	case MOD_SLIME:                 return "slime";
	case MOD_LAVA:                  return "lava";
	case MOD_CRUSH:                 return "crush";
	case MOD_TARGET_LASER:          return "laser";
	case MOD_TRIGGER_HURT:          return "hurt";
	case MOD_KAMIKAZE:              return "kamikaze";
	case MOD_LAVABALL:              return "lavaball";
	case MOD_NAIL:                  return "nail";
	default:                        return "world";
	}
}
