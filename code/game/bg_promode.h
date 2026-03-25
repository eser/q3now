
void CPM_UpdateSettings(int gametype);

extern float    cpm_pm_jump_z;
extern int      pm_walljumps;

// Physics
extern float	cpm_pm_airstopaccelerate;
extern float	cpm_pm_aircontrol;
extern float	cpm_pm_strafeaccelerate;
extern float	cpm_pm_wishspeed;
extern float	pm_accelerate; // located in bg_pmove.c
extern float	pm_friction; // located in bg_pmove.c

void CPM_PM_Aircontrol ( pmove_t *pm, vec3_t wishdir, float wishspeed );

// Weapon switching
extern float	cpm_weapondrop;
extern float	cpm_weaponraise;
extern float	cpm_outofammodelay;

// Backpacks
extern int		cpm_backpacks;

// Radius Damage Fix
extern int		cpm_radiusdamagefix;

// Respawn delay
extern float	cpm_clientrespawndelay;

// Lava damage
extern float	cpm_lavadamage;
extern float	cpm_slimedamage;
extern float	cpm_lavafrequency;
