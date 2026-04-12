/*
===========================================================================
ai_weapsel.c — DPS-based weapon selection for bot AI improvements

Replaces stock fuzzy-logic weapon weights with empirical accuracy
tracking and DPS-based selection per combat zone.

Combat zones by distance:
  ZONE_NEAR     < 300 units
  ZONE_MID      < 700 units
  ZONE_FAR      < 1200 units
  ZONE_VERYFAR  >= 1200 units

DPS formula: (hit_rate * damage_per_hit * 1000.0f) / fire_interval_ms

Skill scaling: lower skill blends fuzzy weights + tracked accuracy.
===========================================================================
*/

#include "g_local.h"
#include "../qcommon/q_shared.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_weapsel.h"
#include "inv.h"

// static weapon stats table — mirrors g_weapon.c / g_missile.c values
typedef struct {
	int		damage;			// direct hit damage
	int		splashDamage;	// splash damage
	int		splashRadius;	// splash radius
	int		fireInterval;	// ms between shots
	float	speed;			// projectile speed (0 = hitscan)
	float	baseAccuracy;	// default accuracy when no data exists [0-1]
} weapstat_t;

static const weapstat_t weapStats[WP_NUM_WEAPONS] = {
	// WP_NONE
	{ 0, 0, 0, 0, 0, 0 },
	// WP_GAUNTLET: 80 damage, melee, 400ms
	{ 80, 0, 0, 400, 0, 0.5f },
	// WP_MACHINEGUN: 8 damage, hitscan, 100ms
	{ 8, 0, 0, 100, 0, 0.3f },
	// WP_SHOTGUN: 10*11=110 max, hitscan, 1000ms
	{ 110, 0, 0, 1000, 0, 0.4f },
	// WP_GRENADE_LAUNCHER: 120 splash, 160 radius, 800ms, 700 speed
	{ 100, 120, 160, 800, 700, 0.15f },
	// WP_ROCKET_LAUNCHER: 100 direct + 120 splash, 120 radius, 800ms, 900 speed
	{ 100, 120, 120, 800, 900, 0.25f },
	// WP_LIGHTNING_GUN: 8 damage, hitscan, 50ms
	{ 8, 0, 0, 50, 0, 0.35f },
	// WP_RAILGUN: 80 damage, hitscan, 1500ms
	{ 80, 0, 0, 1500, 0, 0.4f },
	// WP_PLASMA_RIFLE: 20 damage, 15 splash, 20 radius, 100ms, 2000 speed
	{ 20, 15, 20, 100, 2000, 0.25f },
};

// inventory indices for weapon ammo (from inv.h)
static const int weapAmmoInv[WP_NUM_WEAPONS] = {
	0,							// WP_NONE
	0,							// WP_GAUNTLET (no ammo)
	INVENTORY_BULLETS,			// WP_MACHINEGUN
	INVENTORY_SHELLS,			// WP_SHOTGUN
	INVENTORY_GRENADES,			// WP_GRENADE_LAUNCHER
	INVENTORY_ROCKETS,			// WP_ROCKET_LAUNCHER
	INVENTORY_LIGHTNING,	// WP_LIGHTNING_GUN
	INVENTORY_SLUGS,			// WP_RAILGUN
	INVENTORY_CELLS,			// WP_PLASMA_RIFLE
};

static const int weapInv[WP_NUM_WEAPONS] = {
	0,							// WP_NONE
	INVENTORY_GAUNTLET,			// WP_GAUNTLET
	INVENTORY_MACHINEGUN,		// WP_MACHINEGUN
	INVENTORY_SHOTGUN,			// WP_SHOTGUN
	INVENTORY_GRENADE_LAUNCHER,	// WP_GRENADE_LAUNCHER
	INVENTORY_ROCKET_LAUNCHER,	// WP_ROCKET_LAUNCHER
	INVENTORY_LIGHTNING_GUN,		// WP_LIGHTNING_GUN
	INVENTORY_RAILGUN,			// WP_RAILGUN
	INVENTORY_PLASMA_RIFLE,		// WP_PLASMA_RIFLE
};

/*
==================
BotCombatZone
==================
*/
int BotCombatZone( float dist )
{
	if ( dist < 300.0f ) return ZONE_NEAR;
	if ( dist < 700.0f ) return ZONE_MID;
	if ( dist < 1200.0f ) return ZONE_FAR;
	return ZONE_VERYFAR;
}

/*
==================
BotAccuracyUpdate

Track shots/hits per weapon. Called from combat each frame.
Currently updates based on whether the bot landed a hit this frame.
==================
*/
void BotAccuracyUpdate( struct bot_state_s *bs )
{
	int wp, zone;
	int hitcount;
	float enemyDist;
	vec3_t delta;

	if ( bs->enemy < 0 ) return;

	wp = bs->weaponnum;
	if ( wp <= WP_NONE || wp >= WP_NUM_WEAPONS ) return;

	// compute distance to enemy
	VectorSubtract( g_entities[bs->enemy].r.currentOrigin, bs->origin, delta );
	enemyDist = VectorLength( delta );
	zone = BotCombatZone( enemyDist );

	// check if bot hit something this frame (via persistent hit counter)
	hitcount = bs->cur_ps.persistant[PERS_HITS];
	if ( hitcount > bs->lasthitcount ) {
		bs->accuracy[wp][zone].hits += ( hitcount - bs->lasthitcount );
		bs->accuracy[wp][zone].damage += weapStats[wp].damage * ( hitcount - bs->lasthitcount );
	}

	// if the bot was attacking, count it as a shot
	if ( bs->flags & BFL_ATTACKED ) {
		bs->accuracy[wp][zone].shots++;
	}
}

/*
==================
BotChooseWeaponDPS

Select weapon with highest effective DPS for the current combat zone.
Sets bs->weaponnum and bs->weapon_reason.
==================
*/
void BotChooseWeaponDPS( struct bot_state_s *bs )
{
	int wp, zone;
	float enemyDist, bestDps, dps, hitRate, skill;
	vec3_t delta;
	const weapstat_t *ws;

	if ( bs->enemy < 0 ) return;

	// compute distance to enemy
	VectorSubtract( g_entities[bs->enemy].r.currentOrigin, bs->origin, delta );
	enemyDist = VectorLength( delta );
	zone = BotCombatZone( enemyDist );

	skill = bs->autoskill > 0 ? bs->autoskill : bs->settings.skill;

	bestDps = -1;
	bs->weapon_reason = 0;

	for ( wp = WP_GAUNTLET; wp < WP_NUM_WEAPONS; wp++ ) {
		ws = &weapStats[wp];
		if ( ws->fireInterval < 1 ) continue;

		// must have the weapon
		if ( !bs->inventory[weapInv[wp]] ) continue;

		// must have ammo (gauntlet has infinite)
		if ( wp != WP_GAUNTLET && weapAmmoInv[wp] > 0 ) {
			if ( bs->inventory[weapAmmoInv[wp]] <= 0 ) continue;
		}

		// gauntlet only viable at close range (lunge extends effective range)
		if ( wp == WP_GAUNTLET && enemyDist > 128.0f ) continue;

		// railgun not great at close range
		if ( wp == WP_RAILGUN && enemyDist < 200.0f && bestDps > 0 ) continue;

		// compute hit rate from tracked accuracy or base accuracy
		if ( bs->accuracy[wp][zone].shots > 10 ) {
			hitRate = (float)bs->accuracy[wp][zone].hits / (float)bs->accuracy[wp][zone].shots;
		} else {
			hitRate = ws->baseAccuracy;
			// lower skill = worse base accuracy
			hitRate *= (skill / 5.0f);
		}

		if ( hitRate < 0.01f ) hitRate = 0.01f;

		// compute effective DPS = (hit_rate * damage * 1000) / fire_interval_ms
		dps = (hitRate * (float)ws->damage * 1000.0f) / (float)ws->fireInterval;

		// add splash damage contribution for explosives
		if ( ws->splashDamage > 0 && ws->speed > 0 ) {
			// splash contribution: lower accuracy requirement, but distance-dependent
			float splashRate = hitRate * 0.5f; // splash hits more often
			dps += (splashRate * (float)ws->splashDamage * 1000.0f) / (float)ws->fireInterval;
		}

		// machinegun burst mode: boost DPS at range due to tighter spread
		if ( wp == WP_MACHINEGUN && enemyDist > 500.0f ) {
			// at range, burst (10 dmg, tighter spread) is more effective
			// simulate burst accuracy improvement: 50% better hit rate at range
			float burstHitRate = hitRate * 1.5f;
			if ( burstHitRate > 0.95f ) burstHitRate = 0.95f;
			float burstDps = (burstHitRate * 10.0f * 1000.0f) / 400.0f;  // 10 dmg, 400ms effective rate (3 rounds per burst)
			if ( burstDps > dps ) {
				dps = burstDps;
			}
		}

		// shotgun double-blast: boost DPS at close range due to wide spread
		if ( wp == WP_SHOTGUN && enemyDist < 128.0f ) {
			// at close range, double-blast (2x pellets in rapid succession) is devastating
			// double the DPS since both blasts land at close range
			dps *= 2.0f;
		}

		// rocket launcher mortar: boost effective DPS at close range
		// mortar launches targets into the air for combo follow-ups
		if ( wp == WP_ROCKET_LAUNCHER && enemyDist < 300.0f ) {
			// mortar: 60 dmg + 60 splash, 800ms reload, 250 splash radius, huge knockback
			float mortarSplashRate = hitRate * 0.9f;  // 250 radius = near-guaranteed splash at close range
			float mortarDps = (mortarSplashRate * 120.0f * 1000.0f) / 800.0f;  // 60 direct + 60 splash
			if ( mortarDps > dps ) {
				dps = mortarDps;
			}
		}

		// gauntlet lunge: boost DPS at close range due to gap-close ability
		if ( wp == WP_GAUNTLET && enemyDist <= 128.0f && enemyDist > 40.0f ) {
			// lunge does 100 damage with gap-close, making gauntlet viable beyond normal melee
			float lungeDps = (hitRate * 100.0f * 1000.0f) / 1500.0f;  // 100 dmg, 1.5s cycle
			if ( lungeDps > dps ) {
				dps = lungeDps;
			}
		}

		// penalize weapon switch (0.4s lost)
		if ( wp != bs->weaponnum && bestDps > 0 ) {
			dps *= 0.85f; // ~15% penalty for switching
		}

		if ( dps > bestDps ) {
			bestDps = dps;
			bs->best_weapon = wp;
			bs->weapon_reason = 1; // DPS-based
		}
	}

	if ( bs->best_weapon > WP_NONE && bs->best_weapon < WP_NUM_WEAPONS ) {
		bs->weaponnum = bs->best_weapon;
	}
}
