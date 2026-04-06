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
#define MORTAR_DAMAGE			50
#define MORTAR_SPLASH_DAMAGE	50
#define MORTAR_SPLASH_RADIUS	300
// #define MORTAR_HELIX_OFFSET		10.0f

static void Mortar_SetupRocket( gentity_t *m, float quadFactor ) {
	vec_t speed;

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
	m->damage = MORTAR_DAMAGE * quadFactor;
	m->splashDamage = MORTAR_SPLASH_DAMAGE * quadFactor;
	m->splashRadius = MORTAR_SPLASH_RADIUS;
}

void Attack_RocketLauncher_Mortar (gentity_t *ent) {
	// gentity_t	*m1, *m2;
	gentity_t	*m1;
	// vec3_t		offset;
	// qboolean	dualRocket;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_ROCKET_LAUNCHER_MORTAR].shots++;
	}

	// // pmove already consumed 1 ammo; consume a second if available
	// dualRocket = qtrue;
	// if ( ent->client && ent->client->ps.ammo[WP_ROCKET_LAUNCHER] != -1 ) {
	// 	if ( ent->client->ps.ammo[WP_ROCKET_LAUNCHER] > 0 ) {
	// 		ent->client->ps.ammo[WP_ROCKET_LAUNCHER]--;
	// 	} else {
	// 		dualRocket = qfalse;  // only 1 ammo total — single mortar
	// 	}
	// }

	// if ( dualRocket ) {
	// 	// spawn two rockets with lateral offsets for helix pairing
	// 	VectorScale( right, MORTAR_HELIX_OFFSET, offset );

	// 	VectorAdd( muzzle, offset, offset );
	// 	m1 = fire_rocket( ent, offset, forward );
	// 	Mortar_SetupRocket( m1, s_quadFactor );

	// 	VectorScale( right, -MORTAR_HELIX_OFFSET, offset );
	// 	VectorAdd( muzzle, offset, offset );
	// 	m2 = fire_rocket( ent, offset, forward );
	// 	Mortar_SetupRocket( m2, s_quadFactor );

	// 	// link pair for collision exclusion
	// 	m1->helixPairEntity = m2;
	// 	m2->helixPairEntity = m1;
	// } else {
		// 1-ammo edge case: single mortar rocket, no helix
		m1 = fire_rocket( ent, muzzle, forward );
		Mortar_SetupRocket( m1, s_quadFactor );
	// }
}
