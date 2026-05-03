// g_grappling_hook.c -- Grappling Hook weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;
extern void SnapVectorTowards( vec3_t v, vec3_t to );

void Offhand_Grapple_Hook_Think (gentity_t *ent)
{
	if (ent->enemy) {
		vec3_t v, oldorigin;

		VectorCopy(ent->r.currentOrigin, oldorigin);
		v[0] = ent->enemy->r.currentOrigin[0] + (ent->enemy->r.mins[0] + ent->enemy->r.maxs[0]) * 0.5;
		v[1] = ent->enemy->r.currentOrigin[1] + (ent->enemy->r.mins[1] + ent->enemy->r.maxs[1]) * 0.5;
		v[2] = ent->enemy->r.currentOrigin[2] + (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5;
		SnapVectorTowards( v, oldorigin );	// save net bandwidth

		G_SetOrigin( ent, v );
	}

	VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);

#if FEAT_GRAPPLE_DAMAGE
	// grapple damage (5D): deal damage while hooked to an enemy
	if ( ent->enemy && ent->enemy->client ) {
		if ( level.time - ent->pain_debounce_time >= 100 ) {
			int dmg = 30;
			if ( ent->parent->client->ps.powerups[PW_QUAD] ) {
				dmg *= QUAD_FACTOR;
			}
			if ( ent->enemy->client && ent->enemy->client->deflectorTime > level.time ) {
				vec3_t grappleDir, invulImpact, invulBounce;
				VectorSubtract( ent->enemy->r.currentOrigin, ent->r.currentOrigin, grappleDir );
				VectorNormalize( grappleDir );
				G_DeflectorEffect( ent->enemy, grappleDir, ent->r.currentOrigin,
					invulImpact, invulBounce );
			} else {
				G_Damage( ent->enemy, ent, ent->parent, NULL, NULL, dmg, 0, MOD_GRAPPLE );
			}
			ent->pain_debounce_time = level.time;
		}
	}
#endif
}

/*
===============
Offhand_Grapple_Fire
===============
*/
void Offhand_Grapple_Fire(gentity_t *ent)
{
    AngleVectors(ent->client->ps.viewangles, forward, right, up);
    CalcMuzzlePoint(ent, forward, right, up, muzzle);

    if (!ent->client->fireHeld && !ent->client->hook)
        fire_grapple(ent, muzzle, forward);

    ent->client->hookhasbeenfired = qtrue;
    ent->client->fireHeld = qtrue;
}

/*
===============
Offhand_Grapple_Free
===============
*/
void Offhand_Grapple_Free(gentity_t *ent)
{
    ent->parent->client->hook = NULL;
    ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
    G_FreeEntity(ent);
}

/*
===============
Offhand_Grapple_Think
===============
*/
void Offhand_Grapple_Think(gentity_t *ent)
{
    if (ent->enemy) {
        vec3_t v, oldorigin;

        VectorCopy(ent->r.currentOrigin, oldorigin);
        v[0] = ent->enemy->r.currentOrigin[0] + (ent->enemy->r.mins[0] + ent->enemy->r.maxs[0]) * 0.5;
        v[1] = ent->enemy->r.currentOrigin[1] + (ent->enemy->r.mins[1] + ent->enemy->r.maxs[1]) * 0.5;
        v[2] = ent->enemy->r.currentOrigin[2] + (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5;
        SnapVectorTowards(v, oldorigin);	// save net bandwidth

        G_SetOrigin(ent, v);
    }

    VectorCopy(ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);
}
