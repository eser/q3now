#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "bg_promode.h"

float cpm_pm_jump_z;

// Physics
float	cpm_pm_airstopaccelerate = 1;
float	cpm_pm_aircontrol = 0;
float	cpm_pm_strafeaccelerate = 1;
float	cpm_pm_wishspeed = 400;

// Gauntlet
float	cpm_Gauntletknockback = 1;

// Machinegun
float	cpm_MGbox = 50;
float	cpm_MGdmg = 7;
float	cpm_MGdmg_tp = 5;
int		cpm_MGweapon = 100;

// Shotgun
float	cpm_SSGdmg = 10;
float	cpm_SSGspread = 700;
int		cpm_SSGcount = 11;
float	cpm_SSGknockback = 1;
int		cpm_SSGmaxammo = 200;

// Grenade Launcher
float	cpm_GLreload = 800;
int		cpm_GLmaxammo = 200;

// Rocket Launcher
float	cpm_RLspeed = 900;
int		cpm_RLmaxammo = 200;
float	cpm_RLsplashknockback = 1;
float	cpm_RLknockback = 1;

// Lightning Gun
float	cpm_LGknockback = 1;

// Railgun
int		cpm_RGmaxammo = 200;
int		cpm_RGbox = 10;
float	cpm_RGchange;

// Plasma Gun
float	cpm_PGdmg = 20;
float	cpm_PGknockback = 1;

// Weapon switching
float	cpm_weapondrop = 200;
float	cpm_weaponraise = 250;
float	cpm_outofammodelay = 500;

// Armor system
int		cpm_armorsystem = 0;

// Battle Suit
float	cpm_BSprotection = 0.5;

// Respawn Health
float	cpm_respawnhealth = 25;

// Backpacks
int		cpm_backpacks = 0;

// Radius Damage Fix
int		cpm_radiusdamagefix = 0;

// Z Knockback
float	cpm_knockback_z = 24;

// Respawn Times
int		cpm_itemrespawnhealth = 35;
int		cpm_itemrespawnpowerup = 120;
int		cpm_itemrespawnammo = 40;
int		cpm_startpowerups = 0;
int		cpm_itemrespawnBS = 120;

// Megahealth
int		cpm_megastyle = 0;

// Respawn delay
float	cpm_clientrespawndelay = 1700;

// Hit tones
int		cpm_hittones = 0;

// Item size
int		cpm_itemsize = 36;

// Lava damage
float	cpm_lavadamage = 30;
float	cpm_slimedamage = 10;
float	cpm_lavafrequency = 700;

void CPM_UpdateSettings(int num)
{
	// num = 0: normal quake 3
	// num = 1: pro mode

	cpm_pm_jump_z = 0; // turn off double-jump in vq3

	// Physics
	cpm_pm_airstopaccelerate = 1;
	cpm_pm_aircontrol = 0;
	cpm_pm_strafeaccelerate = 1;
	cpm_pm_wishspeed = 400;
	pm_accelerate = 10;
	pm_friction = 6;

	// vq3 Gauntlet
	cpm_Gauntletknockback = 1;

	// vq3 machine gun settings
	cpm_MGbox = 50;
	cpm_MGdmg = 7;
	cpm_MGdmg_tp = 5;
	cpm_MGweapon = 100;

	// vq3 ssg settings
	cpm_SSGdmg = 10;
	cpm_SSGspread = 700;
	cpm_SSGcount = 11;
	cpm_SSGknockback = 1;
	cpm_SSGmaxammo = 200;

	// vq3 GL settings
	cpm_GLreload = 800;
	cpm_GLmaxammo = 200;

	// vq3 Rocket Launcher
	cpm_RLspeed = 900;
	cpm_RLmaxammo = 200;
	cpm_RLsplashknockback = 1;
	cpm_RLknockback = 1;

	// vq3 Lightning Gun
	cpm_LGknockback = 1;

	// vq3 Railgun
	cpm_RGmaxammo = 200;
	cpm_RGbox = 10;
	cpm_RGchange = 1500;

	// vq3 Plasma Gun
	cpm_PGdmg = 20;
	cpm_PGknockback = 1;

	// vq3 Weapon switching
	cpm_weapondrop = 200;
	cpm_weaponraise = 250;
	cpm_outofammodelay = 500;

	// Armor system
	cpm_armorsystem = 0;

	// vq3 Battle Suit
	cpm_BSprotection = 0.5;

	// vq3 Respawn Health
	cpm_respawnhealth = 25;

	// Backpacks
	cpm_backpacks = 0;

	// Radius Damage Fix
	cpm_radiusdamagefix = 0;

	// Z Knockback
	cpm_knockback_z = 24;

	// Respawn Times
	cpm_itemrespawnhealth = 35;
	cpm_itemrespawnpowerup = 120;
	cpm_itemrespawnammo = 40;
	cpm_startpowerups = 0;
	cpm_itemrespawnBS = 120;

	// Megahealth
	cpm_megastyle = 0;

	// Respawn delay
	cpm_clientrespawndelay = 1700;

	// Hit tones
	cpm_hittones = 0;

	// Item size
	cpm_itemsize = 36;

	// Lava damage
	cpm_lavadamage = 30;
	cpm_slimedamage = 10;
	cpm_lavafrequency = 700;

	if (num)
	{
		cpm_pm_jump_z = 100; // enable double-jump

		// Physics
		cpm_pm_airstopaccelerate = 2.5;
		cpm_pm_aircontrol = 150;
		cpm_pm_strafeaccelerate = 70;
		cpm_pm_wishspeed = 30;
		pm_accelerate = 15;
		pm_friction = 8;

		// Gauntlet
		cpm_Gauntletknockback = 0.5;

		// machinegun
		cpm_MGbox = 25;
		cpm_MGdmg = 5;
		cpm_MGdmg_tp = 5;
		cpm_MGweapon = 50;

		// Shotgun
		cpm_SSGdmg = 7;
		cpm_SSGspread = 900;
		cpm_SSGcount = 16;
		cpm_SSGknockback = 1.35;
		cpm_SSGmaxammo = 100;

		// Grenade Launcher
		cpm_GLreload = 600;
		cpm_GLmaxammo = 100;

		// Rocket Launcher
		cpm_RLspeed = 1000;
		cpm_RLmaxammo = 100;
		cpm_RLsplashknockback = 1.2;
		cpm_RLknockback = 1.2;

		// Lightning Gun
		cpm_LGknockback = 1.55;

		// Railgun
		cpm_RGmaxammo = 100;
		cpm_RGbox = 5;
		cpm_RGchange = 1000; // allow users to switch weapon before reload

		// Plasma Gun
		cpm_PGdmg = 15;
		cpm_PGknockback = 0.5;

		// Weapon switching
		cpm_weapondrop = 0;
		cpm_weaponraise = 0;
		cpm_outofammodelay = 100;

		// Armor system
		cpm_armorsystem = 1;

		// Battle Suit
		cpm_BSprotection = 0.25; // ie 75% protection

		// Respawn Health
		cpm_respawnhealth = 0;

		// Backpacks
		cpm_backpacks = 1;

		// Radius Damage Fix
		cpm_radiusdamagefix = 1;

		// Z Knockback
		cpm_knockback_z = 40;

		// Respawn Times
		cpm_itemrespawnhealth = 30;
		cpm_itemrespawnpowerup = 60;
		cpm_itemrespawnammo = 30;
		cpm_startpowerups = 1;
		cpm_itemrespawnBS = 120;

		// Megahealth
		cpm_megastyle = 1;

		// Respawn delay
		cpm_clientrespawndelay = 500;

		// Hit tones
		cpm_hittones = 1;

		// Item size
		cpm_itemsize = 66; // easier to get items in pro mode

		// Lava damage
		cpm_lavadamage = 4;
		cpm_slimedamage = 1.3;
		cpm_lavafrequency = 100;

	}
}

qboolean CPM_CanGrabAmmo(const gitem_t *item, const playerState_t *ps)
{
    int		quantity;

    switch (item->giTag)
    {
    case WP_SHOTGUN:
        quantity = cpm_SSGmaxammo;
        break;
    case WP_GRENADE_LAUNCHER:
        quantity = cpm_GLmaxammo;
        break;
    case WP_ROCKET_LAUNCHER:
        quantity = cpm_RLmaxammo;
        break;
    case WP_RAILGUN:
        quantity = cpm_RGmaxammo;
        break;
    default:
        quantity = 200;
    }

    if (ps->ammo[item->giTag] < quantity) {
        return qtrue;
    }

    return qfalse;
}

qboolean CPM_CanGrabArmor(const gitem_t *item, const playerState_t *ps)
{
    int upperBound;
    qboolean hasGuard;

#ifdef MISSIONPACK
    if (bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT) {
        return qfalse;
    }

    // we also clamp armor to the maxhealth for handicapping
    if (bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD) {
        upperBound = ps->stats[STAT_MAX_HEALTH];
        hasGuard = qtrue;
    }
    else {
        upperBound = ps->stats[STAT_MAX_HEALTH] * 2;
        hasGuard = qfalse;
    }
#else
    upperBound = ps->stats[STAT_MAX_HEALTH] * 2;
    hasGuard = qfalse;
#endif

    // vq3 system
    if (!cpm_armorsystem)
    {
        // notice that vq3 armor system is cap'ed for handicaps
        return (ps->stats[STAT_ARMOR] >= upperBound) ? qfalse : qtrue;
    }

    // pro mode system
    if (item->quantity == 100) // RA
    {
        return (hasGuard || ps->stats[STAT_ARMOR] >= 200) ? qfalse : qtrue;
    }
    if (item->quantity == 50) // YA
    {
        if (ps->stats[STAT_ARMORTYPE] <= 1)
        {
            return (ps->stats[STAT_ARMOR] >= 150) ? qfalse : qtrue;
        }
        return (ps->stats[STAT_ARMOR] > CPM_RABREAKPOINT) ? qfalse : qtrue;
    }

    return qtrue; // you can _always_ get shards in cpm
}

void CPM_PM_Aircontrol(pmove_t *pm, vec3_t wishdir, float wishspeed)
{
    float	zspeed, speed, dot, k;
    int		i;

    if ((pm->ps->movementDir && pm->ps->movementDir != 4) || wishspeed == 0.0)
        return; // can't control movement if not moveing forward or backward

    zspeed = pm->ps->velocity[2];
    pm->ps->velocity[2] = 0;
    speed = VectorNormalize(pm->ps->velocity);

    dot = DotProduct(pm->ps->velocity, wishdir);
    k = 32;
    k *= cpm_pm_aircontrol*dot*dot*pml.frametime;


    if (dot > 0) {	// we can't change direction while slowing down
        for (i = 0; i < 2; i++)
            pm->ps->velocity[i] = pm->ps->velocity[i] * speed + wishdir[i] * k;
        VectorNormalize(pm->ps->velocity);
    }

    for (i = 0; i < 2; i++)
        pm->ps->velocity[i] *= speed;

    pm->ps->velocity[2] = zspeed;
}
