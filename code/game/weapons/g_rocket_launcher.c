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

#define MORTAR_SPEED			650
#define MORTAR_DAMAGE			60
#define MORTAR_SPLASH_DAMAGE	60
#define MORTAR_SPLASH_RADIUS	250

void Attack_RocketLauncher_Mortar (gentity_t *ent) {
	gentity_t	*m;
	vec_t		speed;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_ROCKET_LAUNCHER_MORTAR].shots++;
	}

	// fire single mortar rocket
	m = fire_rocket( ent, muzzle, forward );
	m->methodOfDeath = MOD_ROCKET_MORTAR;
	m->splashMethodOfDeath = MOD_ROCKET_MORTAR_SPLASH;

	// slower projectile speed
	speed = VectorLength( m->s.pos.trDelta );
	if ( speed > 0 ) {
		VectorScale( m->s.pos.trDelta, (float)MORTAR_SPEED / speed, m->s.pos.trDelta );
	}
	SnapVector( m->s.pos.trDelta );

	// mortar gravity arc
	m->s.pos.trType = TR_GRAVITY;

	// override damage: lower direct hit, serious splash
	m->damage = MORTAR_DAMAGE;
	m->splashDamage = MORTAR_SPLASH_DAMAGE;
	m->splashRadius = MORTAR_SPLASH_RADIUS;

	// quad factor
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;
}
