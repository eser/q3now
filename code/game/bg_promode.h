
void CPM_UpdateSettings(int num);

#define CS_PRO_MODE 16

extern float cpm_pm_jump_z;

// Physics
extern float	cpm_pm_airstopaccelerate;
extern float	cpm_pm_aircontrol;
extern float	cpm_pm_strafeaccelerate;
extern float	cpm_pm_wishspeed;
extern float	pm_accelerate; // located in bg_pmove.c
extern float	pm_friction; // located in bg_pmove.c

void CPM_PM_Aircontrol ( pmove_t *pm, vec3_t wishdir, float wishspeed );

// Gauntlet
extern float	cpm_Gauntletknockback;

// Machinegun
extern float	cpm_MGbox;
extern int		cpm_MGweapon;

// Shotgun
extern float	cpm_SSGknockback;
extern int		cpm_SSGmaxammo;

// Grenade Launcher
extern float	cpm_GLreload;
extern int		cpm_GLmaxammo;

// Rocket Launcher
extern int		cpm_RLmaxammo;
extern float	cpm_RLsplashknockback;
extern float	cpm_RLknockback;

// Lightning Gun
extern float	cpm_LGknockback;

// Railgun
extern int		cpm_RGmaxammo;
extern int		cpm_RGbox;
extern float	cpm_RGchange;

// Plasma Gun
extern float	cpm_PGknockback;

// Weapon switching
extern float	cpm_weapondrop;
extern float	cpm_weaponraise;
extern float	cpm_outofammodelay;

// Armor system
#define CPM_YAPROTECTION 0.60
#define CPM_RAPROTECTION 0.75
#define CPM_RABREAKPOINT 120	// this is the point where a player with RA can pickup an YA
								// calculated through YAPROT / RAPROT = 0.60 / 0.75 = 0.8
								// then MAXYA * 0.8 = 150 * 0.8 = 120
#define CPM_RAMULTIPLIER 1.25   // If a player has little RA and picks up an YA, multiply his
								// RA value with this. (0.75 / 0.60 = 1.25)
// Battle Suit
extern float	cpm_BSprotection;

// Backpacks
extern int		cpm_backpacks;

// Radius Damage Fix
extern int		cpm_radiusdamagefix;

// Z Knockback
extern float	cpm_knockback_z;

// Respawn Times
extern int		cpm_itemrespawnhealth;
extern int		cpm_itemrespawnpowerup;
extern int		cpm_itemrespawnBS;
extern int		cpm_itemrespawnammo;
extern int		cpm_startpowerups;

// Megahealth
extern int		cpm_megastyle;
#define CPM_MEGARESPAWNDELAY 20

// Respawn delay
extern float	cpm_clientrespawndelay;

// Hit tones
extern int		cpm_hittones;

// Item size
extern int		cpm_itemsize;

// Lava damage
extern float	cpm_lavadamage;
extern float	cpm_slimedamage;
extern float	cpm_lavafrequency;
