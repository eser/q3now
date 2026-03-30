// bg_weapons.c -- weapon and attack definition tables (shared by cgame, game, ui)

#include "../qcommon/q_shared.h"
#include "bg_public.h"

// Attack types — indexed by attackType_t
gattack_t	bg_attacklist[] =
{
	// ATT_NONE
	{
		/* name                 */ "None",
		/* weapon				*/ WP_NONE,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 0,
		/* selfKnockbackScale   */ 0,
		/* recoilKick           */ 0,
		/* reloadTime           */ 0,
	},

	// ATT_GAUNTLET_PRIMARY
	{
		/* name                 */ "Gauntlet Primary",
		/* weapon				*/ WP_GAUNTLET,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 2.5f,
		/* selfKnockbackScale   */ 2.5f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 400,
	},

	// ATT_MACHINEGUN_PRIMARY
	{
		/* name                 */ "Machinegun Primary",
		/* weapon				*/ WP_MACHINEGUN,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 6.75f,
		/* selfKnockbackScale   */ 6.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 100,
	},

	// ATT_SHOTGUN_PRIMARY
	{
		/* name                 */ "Shotgun Primary",
		/* weapon				*/ WP_SHOTGUN,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 6.75f,
		/* selfKnockbackScale   */ 6.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 1000,
	},

	// ATT_GRENADE_LAUNCHER_PRIMARY
	{
		/* name                 */ "Grenade Launcher Primary",
		/* weapon				*/ WP_GRENADE_LAUNCHER,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 6.0f,
		/* selfKnockbackScale   */ 5.0f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 600,
	},

	// ATT_ROCKET_LAUNCHER_PRIMARY
	{
		/* name                 */ "Rocket Launcher Primary",
		/* weapon				*/ WP_ROCKET_LAUNCHER,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 6.0f,
		/* selfKnockbackScale   */ 5.0f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 800,
	},

	// ATT_LIGHTNING_GUN_PRIMARY
	{
		/* name                 */ "Lightning Gun Primary",
		/* weapon				*/ WP_LIGHTNING_GUN,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 7.75f,
		/* selfKnockbackScale   */ 7.75f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 50,
	},

	// ATT_RAILGUN_PRIMARY
	{
		/* name                 */ "Railgun Primary",
		/* weapon				*/ WP_RAILGUN,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 7.75f,
		/* selfKnockbackScale   */ 7.75f,
		/* recoilKick           */ 200.0f,
		/* reloadTime           */ 1000,
	},

	// ATT_PLASMA_RIFLE_PRIMARY
	{
		/* name                 */ "Plasma Rifle Primary",
		/* weapon				*/ WP_PLASMA_RIFLE,
		/* maxDamageDistance    */ 0,
		/* knockbackScale       */ 2.5f,
		/* selfKnockbackScale   */ 2.5f,
		/* recoilKick           */ 0.0f,
		/* reloadTime           */ 1000,
	},
};

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
		/* attackAlt            */ ATT_NONE,

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
		/* attackAlt            */ ATT_GRENADE_LAUNCHER_PRIMARY,

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
		/* attackAlt            */ ATT_NONE,

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
		/* attackAlt            */ ATT_NONE,

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
		/* attackAlt            */ ATT_NONE,

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
