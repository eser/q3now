#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "bg_promode.h"

float   cpm_pm_jump_z;
int     pm_walljumps;

// Physics
float	cpm_pm_airstopaccelerate = 1;
float	cpm_pm_aircontrol = 0;
float	cpm_pm_strafeaccelerate = 1;
float	cpm_pm_wishspeed = 400;

// Weapon switching
float	cpm_weapondrop = 0; // 200;
float	cpm_weaponraise = 250;
float	cpm_outofammodelay = 500;

// Backpacks
int		cpm_backpacks = 0;

// Radius Damage Fix
int		cpm_radiusdamagefix = 0;

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

void CPM_UpdateSettings(int gametype)
{
	// num = 0: normal quake 3
	// num = 1: pro mode

	cpm_pm_jump_z = 0; // turn off double-jump in vq3
    pm_walljumps = 0;

	// Physics
	cpm_pm_airstopaccelerate = 1;
	cpm_pm_aircontrol = 0;
	cpm_pm_strafeaccelerate = 1;
	cpm_pm_wishspeed = 400;
	pm_accelerate = 10;
	pm_friction = 6;

	// vq3 Weapon switching
    cpm_weapondrop = 0; // 200;
	cpm_weaponraise = 250;
	cpm_outofammodelay = 500;

	// Backpacks
	cpm_backpacks = 0;

	// Radius Damage Fix
	cpm_radiusdamagefix = 0;

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

    if (1) // pro_physics
    {
        cpm_pm_jump_z = 100; // enable double-jump
        pm_walljumps = 1;

        // Physics
        cpm_pm_airstopaccelerate = 2.5;
        cpm_pm_aircontrol = 150;
        cpm_pm_strafeaccelerate = 70;
        cpm_pm_wishspeed = 30;
        pm_accelerate = 15;
        pm_friction = 8;
    }

	if (0) // pro_mode
	{
		// Weapon switching
		cpm_weapondrop = 0;
		cpm_weaponraise = 0;
		cpm_outofammodelay = 100;

		// Backpacks
		cpm_backpacks = 0;

		// Radius Damage Fix
		cpm_radiusdamagefix = 1;

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
