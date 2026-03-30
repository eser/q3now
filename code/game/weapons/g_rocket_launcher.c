// g_rocket_launcher.c -- Rocket Launcher weapon implementation
#include "../g_local.h"
#include "../bg_promode.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;

void Attack_RocketLauncher_Primary (gentity_t *ent) {
	gentity_t	*m;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_ROCKET_LAUNCHER_PRIMARY].shots++;
	}

	m = fire_rocket (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}
