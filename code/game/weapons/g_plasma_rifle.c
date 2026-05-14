// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// g_plasma_rifle.c -- Plasma Rifle weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;

#define NUM_PLASMASHOTS 15

void Weapon_PlasmaRifle_Primary (gentity_t *ent) {
    gentity_t	*m;
    int			count;

	if (ent->client) {
		ent->client->accuracy_shots += NUM_PLASMASHOTS;
		ent->client->attackStats[ATT_PLASMA_RIFLE_PRIMARY].shots += NUM_PLASMASHOTS;
	}

    for (count = 0; count < NUM_PLASMASHOTS; count++) {
        m = fire_plasma(ent, muzzle, forward, right, up);
        m->damage *= s_quadFactor;
        m->splashDamage *= s_quadFactor;
    }

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}
