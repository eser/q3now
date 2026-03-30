/*
===========================================================================
cg_lensflare.c -- Lens flare effects (9A / FEAT_LENS_FLARES)

Ported from BrightArena (JUHOX). Map light flares auto-generated from
BSP light entities. Missile flares per weapon type. Distance culling +
frame-interleaved visibility checks.

CVars: cg_lensFlare, cg_missileFlare
===========================================================================
*/
#include "cg_local.h"

#if FEAT_LENS_FLARES

#define MAX_LENS_FLARE_ENTITIES   256
#define FLARE_DISTANCE            1500.0f
#define FLARES_PER_FRAME          16
#define FLARE_MIN_LIGHT_RADIUS    20		// skip dim fill lights (ambient, pathway)
#define FLARE_MIN_HEIGHT_ABOVE    210		// light must be >= 210 units above ground below it

typedef struct {
	vec3_t		origin;
	float		radius;
	qboolean	visible;
	float		intensity;
} lensFlareEntity_t;

static struct {
	lensFlareEntity_t	entities[MAX_LENS_FLARE_ENTITIES];
	int					numEntities;
	int					checkIndex;	// round-robin index for interleaved visibility

	// original BrightArena shaders
	qhandle_t			glareShader;
	qhandle_t			starShader;
	qhandle_t			discShader;
	qhandle_t			ringShader;
	qhandle_t			lineShader;
	// q3now cinematic shaders
	qhandle_t			coolGlowShader;
	qhandle_t			blueStreakShader;
	qhandle_t			anamorphicShader;
	qhandle_t			powerupGlowShader;
} lf;

/*
==================
CG_InitLensFlares

Called during CG_Init. Parses light entities from the map's entity string
to build the flare entity array.
==================
*/
void CG_InitLensFlares( void ) {
	char		info[MAX_INFO_STRING];
	const char	*p;
	char		*token;
	vec3_t		origin;
	float		light;
	qboolean	inEntity;
	qboolean	isLight;
	char		key[MAX_TOKEN_CHARS];
	char		value[MAX_TOKEN_CHARS];

	memset( &lf, 0, sizeof( lf ) );

	// register shaders — q3now cinematic variants
	lf.glareShader       = trap_R_RegisterShader( "lfWarmGlow" );
	lf.starShader        = trap_R_RegisterShader( "lfStar" );
	lf.discShader        = trap_R_RegisterShader( "lfGhostDisc" );
	lf.ringShader        = trap_R_RegisterShader( "lfGhostRing" );
	lf.lineShader        = trap_R_RegisterShader( "lfAnamorphic" );
	lf.coolGlowShader    = trap_R_RegisterShader( "lfCoolGlow" );
	lf.blueStreakShader   = trap_R_RegisterShader( "lfBlueStreak" );
	lf.anamorphicShader   = trap_R_RegisterShader( "lfAnamorphic" );
	lf.powerupGlowShader = trap_R_RegisterShader( "lfPowerupGlow" );

	// parse the entity string for "light" entities
	// The entity string is available via trap_GetEntityToken
	inEntity = qfalse;
	isLight = qfalse;
	VectorClear( origin );
	light = 0;

	while ( 1 ) {
		if ( !trap_GetEntityToken( key, sizeof( key ) ) ) {
			break;
		}

		if ( key[0] == '{' ) {
			inEntity = qtrue;
			isLight = qfalse;
			VectorClear( origin );
			light = 300;	// default light radius
			continue;
		}

		if ( key[0] == '}' ) {
			if ( inEntity && isLight && lf.numEntities < MAX_LENS_FLARE_ENTITIES
				 && light >= FLARE_MIN_LIGHT_RADIUS ) {
				// check height above ground — skip floor-level fill lights
				trace_t groundTrace;
				vec3_t down;
				VectorCopy( origin, down );
				down[2] -= 4096;
				CG_Trace( &groundTrace, origin, NULL, NULL, down, -1, CONTENTS_SOLID );
				if ( groundTrace.fraction < 1.0f ) {
					float heightAbove = origin[2] - groundTrace.endpos[2];
					if ( heightAbove < FLARE_MIN_HEIGHT_ABOVE ) {
						inEntity = qfalse;
						continue;	// too close to floor — skip this light
					}
				}

				{
					lensFlareEntity_t *e = &lf.entities[lf.numEntities];
					VectorCopy( origin, e->origin );
					e->radius = light;
					e->visible = qfalse;
					e->intensity = 0;
					lf.numEntities++;
				}
			}
			inEntity = qfalse;
			continue;
		}

		if ( !inEntity ) {
			continue;
		}

		// read value
		if ( !trap_GetEntityToken( value, sizeof( value ) ) ) {
			break;
		}

		if ( !Q_stricmp( key, "classname" ) ) {
			if ( !Q_stricmp( value, "light" ) ) {
				isLight = qtrue;
			}
		} else if ( !Q_stricmp( key, "origin" ) ) {
			sscanf( value, "%f %f %f", &origin[0], &origin[1], &origin[2] );
		} else if ( !Q_stricmp( key, "light" ) ) {
			light = atof( value );
		}
	}

	if ( lf.numEntities > 0 ) {
		CG_Printf( "Lens flares: %i light entities found\n", lf.numEntities );
	}
}

/*
==================
CG_AddMapFlares

Renders map light lens flares. Called each frame from CG_DrawActiveFrame.
Uses distance culling + round-robin interleaved visibility checks.
==================
*/
static void CG_AddMapFlares( void ) {
	int			i, checked;
	float		dist;
	vec3_t		dir;
	trace_t		tr;
	refEntity_t	ent;
	float		dot, scale;
	vec3_t		screenDir;

	if ( lf.numEntities == 0 ) {
		return;
	}

	// round-robin visibility update: check FLARES_PER_FRAME lights per frame
	checked = 0;
	while ( checked < FLARES_PER_FRAME && checked < lf.numEntities ) {
		lensFlareEntity_t *e = &lf.entities[lf.checkIndex];

		VectorSubtract( e->origin, cg.refdef.vieworg, dir );
		dist = VectorLength( dir );

		if ( dist > FLARE_DISTANCE || dist < 1 ) {
			e->visible = qfalse;
		} else {
			// trace for occlusion
			CG_Trace( &tr, cg.refdef.vieworg, NULL, NULL, e->origin, -1, CONTENTS_SOLID );
			e->visible = ( tr.fraction >= 0.99f );
		}

		if ( e->visible ) {
			// smooth intensity ramp (fade in/out)
			if ( e->intensity < 1.0f ) {
				e->intensity += cg.frametime * 0.003f;
				if ( e->intensity > 1.0f ) e->intensity = 1.0f;
			}
		} else {
			if ( e->intensity > 0 ) {
				e->intensity -= cg.frametime * 0.005f;
				if ( e->intensity < 0 ) e->intensity = 0;
			}
		}

		lf.checkIndex = ( lf.checkIndex + 1 ) % lf.numEntities;
		checked++;
	}

	// render all flares with intensity > 0
	for ( i = 0; i < lf.numEntities; i++ ) {
		lensFlareEntity_t *e = &lf.entities[i];

		if ( e->intensity <= 0 ) {
			continue;
		}

		VectorSubtract( e->origin, cg.refdef.vieworg, dir );
		dist = VectorLength( dir );
		if ( dist > FLARE_DISTANCE || dist < 1 ) {
			continue;
		}

		// facing check: only render if light is roughly in front of camera
		VectorNormalize2( dir, screenDir );
		dot = DotProduct( screenDir, cg.refdef.viewaxis[0] );
		if ( dot < 0.1f ) {
			continue;
		}

		// scale by distance and angle
		scale = e->intensity * dot * ( 1.0f - dist / FLARE_DISTANCE );
		if ( scale < 0.01f ) {
			continue;
		}

		memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_SPRITE;
		VectorCopy( e->origin, ent.origin );

		// Layer 1: blue-white soft glow (JJ Abrams cool tone)
		ent.radius = 20 + scale * 56;
		ent.customShader = lf.coolGlowShader;
		ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
		ent.shaderRGBA[3] = (byte)( 45 * scale );
		trap_R_AddRefEntityToScene( &ent );

		// Layer 2: star spike — subtle white
		if ( scale > 0.2f ) {
			ent.radius = 10 + scale * 32;
			ent.customShader = lf.starShader;
			ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = (byte)( 255 * scale );
			ent.shaderRGBA[3] = (byte)( 50 * scale );
			trap_R_AddRefEntityToScene( &ent );
		}

		// Layer 3: horizontal anamorphic streak (JJ signature)
		if ( scale > 0.3f ) {
			ent.radius = 14 + scale * 40;
			ent.customShader = lf.blueStreakShader;
			ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
			ent.shaderRGBA[3] = (byte)( 35 * scale );
			trap_R_AddRefEntityToScene( &ent );
		}

		// Layer 4: small bright core — soft glow, warm white
		if ( scale > 0.4f ) {
			ent.radius = 4 + scale * 8;
			ent.customShader = lf.glareShader;
			ent.shaderRGBA[0] = 255;
			ent.shaderRGBA[1] = 250;
			ent.shaderRGBA[2] = 240;
			ent.shaderRGBA[3] = (byte)( 80 * scale );
			trap_R_AddRefEntityToScene( &ent );
		}
	}
}

/*
==================
CG_AddMissileFlare

Adds a lens flare to a missile entity. Called from cg_ents.c or cg_weapons.c
for each visible missile.
==================
*/
void CG_AddMissileFlare( centity_t *cent ) {
	refEntity_t	ent;
	byte		r, g, b;
	float		pulse;

	if ( !cg_missileFlare.integer ) {
		return;
	}

	// per-weapon color — vivid, saturated for JJ Abrams look
	switch ( cent->currentState.weapon ) {
	case WP_ROCKET_LAUNCHER:
		r = 255; g = 180; b = 60;		// hot orange
		break;
	case WP_PLASMA_RIFLE:
		r = 100; g = 255; b = 120;		// neon green
		break;
	case WP_RAILGUN:
		r = 100; g = 140; b = 255;		// electric blue
		break;
	case WP_GRENADE_LAUNCHER:
		r = 240; g = 220; b = 80;		// bright yellow
		break;
	case WP_LIGHTNING_GUN:
		r = 200; g = 220; b = 255;		// white-blue
		break;
	default:
		r = g = b = 220;				// bright white
		break;
	}

	// subtle pulse based on time + entity number (each missile pulses differently)
	pulse = 0.85f + 0.15f * sin( cg.time * 0.008f + cent->currentState.number * 1.7f );

	memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;
	VectorCopy( cent->lerpOrigin, ent.origin );

	// ── Layer 1: large soft glow ──
	ent.radius = 48 * pulse;
	ent.customShader = lf.glareShader;
	ent.shaderRGBA[0] = (byte)( r * 0.5f );
	ent.shaderRGBA[1] = (byte)( g * 0.5f );
	ent.shaderRGBA[2] = (byte)( b * 0.5f );
	ent.shaderRGBA[3] = 40;
	trap_R_AddRefEntityToScene( &ent );

	// ── Layer 2: medium core ──
	ent.radius = 20 * pulse;
	ent.customShader = lf.glareShader;
	ent.shaderRGBA[0] = r;
	ent.shaderRGBA[1] = g;
	ent.shaderRGBA[2] = b;
	ent.shaderRGBA[3] = 60;
	trap_R_AddRefEntityToScene( &ent );

	// ── Layer 3: bright center — glare (soft) not disc (hard circle) ──
	ent.radius = 8 * pulse;
	ent.customShader = lf.glareShader;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 80;
	trap_R_AddRefEntityToScene( &ent );

	// ── Layer 4: star spike — low alpha, large, ethereal ──
	ent.radius = 32 * pulse;
	ent.customShader = lf.starShader;
	ent.shaderRGBA[0] = (byte)( r * 0.7f );
	ent.shaderRGBA[1] = (byte)( g * 0.7f );
	ent.shaderRGBA[2] = (byte)( b * 0.7f );
	ent.shaderRGBA[3] = 35;
	trap_R_AddRefEntityToScene( &ent );

	// ── Layer 5: outer ring halo ──
	if ( cent->currentState.weapon == WP_ROCKET_LAUNCHER ||
		 cent->currentState.weapon == WP_RAILGUN ) {
		ent.radius = 56 * pulse;
		ent.customShader = lf.ringShader;
		ent.shaderRGBA[0] = (byte)( r * 0.3f );
		ent.shaderRGBA[1] = (byte)( g * 0.3f );
		ent.shaderRGBA[2] = (byte)( b * 0.3f );
		ent.shaderRGBA[3] = 25;
		trap_R_AddRefEntityToScene( &ent );
	}
}

/*
==================
CG_AddPowerupFlare

Adds a pulsing glow flare to a powerup pickup item.
Called from CG_Item() in cg_ents.c for IT_POWERUP items.
==================
*/
void CG_AddPowerupFlare( centity_t *cent, int powerupTag ) {
	refEntity_t	ent;
	byte		r, g, b;
	float		pulse;

	if ( !cg_powerupFlares.integer ) {
		return;
	}

	// per-powerup color
	switch ( powerupTag ) {
	case PW_QUAD:
		r = 100; g = 80; b = 255;		// blue-purple
		break;
	case PW_BERSERK:
		r = 255; g = 0; b = 0;		    // red
		break;
	case PW_BATTLESUIT:
		r = 255; g = 200; b = 60;		// gold
		break;
	case PW_HASTE:
		r = 255; g = 120; b = 40;		// orange-red
		break;
	case PW_REGEN:
		r = 60;  g = 255; b = 100;		// green
		break;
	case PW_FLIGHT:
		r = 200; g = 200; b = 255;		// light blue
		break;
	case PW_INVIS:
		return;							// invisible = no flare
	default:
		r = g = b = 200;
		break;
	}

	pulse = 0.7f + 0.3f * sin( cg.time * 0.004f + cent->currentState.number * 2.3f );

	memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 16;	// slightly above item center

	// Layer 1: pulsing colored glow
	ent.radius = 40 * pulse;
	ent.customShader = lf.powerupGlowShader;
	ent.shaderRGBA[0] = (byte)( r * 0.5f );
	ent.shaderRGBA[1] = (byte)( g * 0.5f );
	ent.shaderRGBA[2] = (byte)( b * 0.5f );
	ent.shaderRGBA[3] = 50;
	trap_R_AddRefEntityToScene( &ent );

	// Layer 2: bright core
	ent.radius = 14 * pulse;
	ent.customShader = lf.glareShader;
	ent.shaderRGBA[0] = r;
	ent.shaderRGBA[1] = g;
	ent.shaderRGBA[2] = b;
	ent.shaderRGBA[3] = 60;
	trap_R_AddRefEntityToScene( &ent );

	// Layer 3: star spike
	ent.radius = 24 * pulse;
	ent.customShader = lf.starShader;
	ent.shaderRGBA[0] = (byte)( r * 0.7f );
	ent.shaderRGBA[1] = (byte)( g * 0.7f );
	ent.shaderRGBA[2] = (byte)( b * 0.7f );
	ent.shaderRGBA[3] = 30;
	trap_R_AddRefEntityToScene( &ent );
}

/*
==================
CG_AddLensFlares

Main entry point. Called from cg_view.c render loop.
==================
*/
void CG_AddLensFlares( void ) {
	if ( !cg_lensFlare.integer ) {
		return;
	}
	CG_AddMapFlares();
}

#endif // FEAT_LENS_FLARES
