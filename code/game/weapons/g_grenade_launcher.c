// g_grenade_launcher.c -- Grenade Launcher weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;

void Attack_GrenadeLauncher_Primary (gentity_t *ent) {
	gentity_t	*m;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_GRENADE_LAUNCHER_PRIMARY].shots++;
	}

	// extra vertical velocity
	forward[2] += 0.35f;
	VectorNormalize( forward );

	m = fire_grenade(ent, muzzle, forward, 2500, qtrue);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}
