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
	// BSP-discovered map lights
	lensFlareEntity_t	entities[MAX_LENS_FLARE_ENTITIES];
	int					numEntities;
	int					checkIndex;

	// .lfs missile flare effects (loaded from scripts/missiles.lfs)
	lensFlareEffect_t	missileLensFlareEffects[MAX_MISSILE_LENSFLARE_EFFECTS];
	int					numMissileLensFlareEffects;
	int					lensFlareEffectBFG;
	int					lensFlareEffectRocketLauncher;

	// shaders for map + powerup flare rendering
	qhandle_t			warmFlowShader;
	qhandle_t			starShader;
	qhandle_t			coolGlowShader;
	qhandle_t			blueStreakShader;
	qhandle_t			powerupGlowShader;
} lf;

/*
==================
CG_LoadFileToBuffer — flat .lfs file loader, no include stack
==================
*/
static qboolean CG_LoadFileToBuffer( const char *path, char *buf, int bufSize ) {
	fileHandle_t	f;
	int				len;

	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if ( len <= 0 ) {
		if ( f ) trap_FS_FCloseFile( f );
		return qfalse;
	}
	if ( len >= bufSize ) {
		CG_Printf( S_COLOR_YELLOW "lens flare file too large: '%s'\n", path );
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = '\0';
	trap_FS_FCloseFile( f );
	return qtrue;
}

/*
==================
CG_FindMissileLensFlareEffect
==================
*/
static int CG_FindMissileLensFlareEffect( const char *name ) {
	for ( int i = 0; i < lf.numMissileLensFlareEffects; i++ ) {
		if ( !Q_stricmp( lf.missileLensFlareEffects[i].name, name ) )
			return i;
	}
	return -1;
}

/*
==================
CG_ParseLensFlare — parse one { ... } sub-flare block
Ported from BrightArena cg_main.c (JUHOX)
==================
*/
static qboolean CG_ParseLensFlare( const char **p, lensFlare_t *fl, const char *effectName ) {
	ComParser       parser = { 0 };
	const char      *token;

	fl->pos                = 1.0f;
	fl->size               = 1.0f;
	fl->rgba[0] = fl->rgba[1] = fl->rgba[2] = fl->rgba[3] = 255.0f;
	fl->fadeAngleFactor    = 1.0f;
	fl->entityAngleFactor  = 1.0f;
	fl->rotationRollFactor = 1.0f;

	while ( 1 ) {
		token = COM_Parse( &parser, p );
		if ( !token[0] ) {
			CG_Printf( S_COLOR_YELLOW "unexpected end in lens flare '%s'\n", effectName );
			return qfalse;
		}
		if ( !Q_stricmp( token, "}" ) ) break;

		if ( !Q_stricmp( token, "shader" ) ) {
			token = COM_Parse( &parser, p );
			if ( token[0] ) fl->shader = trap_R_RegisterShaderNoMip( token );
		} else if ( !Q_stricmp( token, "mode" ) ) {
			token = COM_Parse( &parser, p );
			if      ( !Q_stricmp( token, "reflexion" ) ) fl->mode = LFM_reflexion;
			else if ( !Q_stricmp( token, "glare"     ) ) fl->mode = LFM_glare;
			else if ( !Q_stricmp( token, "star"      ) ) fl->mode = LFM_star;
			else {
				CG_Printf( S_COLOR_YELLOW "unknown flare mode '%s' in '%s'\n", token, effectName );
				return qfalse;
			}
		} else if ( !Q_stricmp( token, "pos" ) ) {
			fl->pos  = atof( COM_Parse( &parser, p ) );
		} else if ( !Q_stricmp( token, "size" ) ) {
			fl->size = atof( COM_Parse( &parser, p ) );
		} else if ( !Q_stricmp( token, "color" ) ) {
			fl->rgba[0] = 255.0f * Com_Clamp( 0, 1, atof( COM_Parse( &parser, p ) ) );
			fl->rgba[1] = 255.0f * Com_Clamp( 0, 1, atof( COM_Parse( &parser, p ) ) );
			fl->rgba[2] = 255.0f * Com_Clamp( 0, 1, atof( COM_Parse( &parser, p ) ) );
		} else if ( !Q_stricmp( token, "alpha" ) ) {
			fl->rgba[3] = 255.0f * Com_Clamp( 0, 1, atof( COM_Parse( &parser, p ) ) );
		} else if ( !Q_stricmp( token, "rotation" ) ) {
			fl->rotationOffset      = Com_Clamp( -360, 360, atof( COM_Parse( &parser, p ) ) );
			fl->rotationYawFactor   = atof( COM_Parse( &parser, p ) );
			fl->rotationPitchFactor = atof( COM_Parse( &parser, p ) );
			fl->rotationRollFactor  = atof( COM_Parse( &parser, p ) );
		} else if ( !Q_stricmp( token, "fadeAngleFactor" ) ) {
			fl->fadeAngleFactor = atof( COM_Parse( &parser, p ) );
			if ( fl->fadeAngleFactor < 0 ) fl->fadeAngleFactor = 0;
		} else if ( !Q_stricmp( token, "entityAngleFactor" ) ) {
			fl->entityAngleFactor = atof( COM_Parse( &parser, p ) );
			if ( fl->entityAngleFactor < 0 ) fl->entityAngleFactor = 0;
		} else if ( !Q_stricmp( token, "intensityThreshold" ) ) {
			fl->intensityThreshold = Com_Clamp( 0, 0.99f, atof( COM_Parse( &parser, p ) ) );
		} else {
			CG_Printf( S_COLOR_YELLOW "unknown token '%s' in flare '%s'\n", token, effectName );
			return qfalse;
		}
	}
	return qtrue;
}

/*
==================
CG_ParseLensFlareEffect — parse one named effect block
Ported from BrightArena cg_main.c (JUHOX). Simplified: no import/sunparm/file-stack.
==================
*/
static qboolean CG_ParseLensFlareEffect( const char **p, lensFlareEffect_t *eff ) {
	ComParser       parser = { 0 };
	const char      *token;

	token = COM_Parse( &parser, p );
	if ( !token[0] ) return qfalse;
	Q_strncpyz( eff->name, token, sizeof( eff->name ) );

	token = COM_Parse( &parser, p );
	if ( Q_stricmp( token, "{" ) ) {
		CG_Printf( S_COLOR_YELLOW "expected '{' after effect '%s'\n", eff->name );
		return qfalse;
	}

	eff->range     = 400.0f;
	eff->rangeSqr  = 160000.0f;
	eff->fadeAngle = 20.0f;

	while ( 1 ) {
		token = COM_Parse( &parser, p );
		if ( !token[0] ) {
			CG_Printf( S_COLOR_YELLOW "unexpected end in effect '%s'\n", eff->name );
			return qfalse;
		}
		if ( !Q_stricmp( token, "}" ) ) break;

		if ( !Q_stricmp( token, "{" ) ) {
			if ( eff->numLensFlares >= MAX_LENSFLARES_PER_EFFECT ) {
				CG_Printf( S_COLOR_YELLOW "too many sub-flares in '%s' (max %d)\n",
				           eff->name, MAX_LENSFLARES_PER_EFFECT );
				return qfalse;
			}
			if ( !CG_ParseLensFlare( p, &eff->lensFlares[eff->numLensFlares], eff->name ) )
				return qfalse;
			eff->numLensFlares++;
		} else if ( !Q_stricmp( token, "range" ) ) {
			eff->range    = atof( COM_Parse( &parser, p ) );
			eff->rangeSqr = eff->range * eff->range;
		} else if ( !Q_stricmp( token, "fadeAngle" ) ) {
			eff->fadeAngle = Com_Clamp( 0, 180, atof( COM_Parse( &parser, p ) ) );
		} else {
			CG_Printf( S_COLOR_YELLOW "unknown token '%s' in effect '%s'\n", token, eff->name );
			return qfalse;
		}
	}
	return qtrue;
}

/*
==================
CG_LoadMissileLensFlares — loads scripts/missiles.lfs
==================
*/
static void CG_LoadMissileLensFlares( void ) {
	static char	buf[65536];
	const char	*p;

	lf.numMissileLensFlareEffects    = 0;
	lf.lensFlareEffectBFG            = -1;
	lf.lensFlareEffectRocketLauncher = -1;

	if ( !CG_LoadFileToBuffer( "scripts/missiles.lfs", buf, sizeof( buf ) ) )
		return;

	p = buf;
	while ( lf.numMissileLensFlareEffects < MAX_MISSILE_LENSFLARE_EFFECTS ) {
		if ( !CG_ParseLensFlareEffect( &p, &lf.missileLensFlareEffects[lf.numMissileLensFlareEffects] ) )
			break;
		lf.numMissileLensFlareEffects++;
	}

	lf.lensFlareEffectBFG            = CG_FindMissileLensFlareEffect( "missile_bfg" );
	lf.lensFlareEffectRocketLauncher = CG_FindMissileLensFlareEffect( "missile_rocket_launcher" );

	CG_Printf( "Lens flares: %d missile effects loaded (bfg=%d rl=%d)\n",
	           lf.numMissileLensFlareEffects,
	           lf.lensFlareEffectBFG, lf.lensFlareEffectRocketLauncher );
}

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

	// shaders for map + powerup flare rendering
	lf.warmFlowShader    = trap_R_RegisterShader( "lfWarmGlow" );
	lf.starShader        = trap_R_RegisterShader( "lfStar" );
	lf.coolGlowShader    = trap_R_RegisterShader( "lfCoolGlow" );
	lf.blueStreakShader  = trap_R_RegisterShader( "lfBlueStreak" );
	lf.powerupGlowShader = trap_R_RegisterShader( "lfPowerupGlow" );

	// load missile flare effects from data file
	CG_LoadMissileLensFlares();

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
			ent.customShader = lf.warmFlowShader;
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

Adds lens flare to a missile entity using the loaded .lfs effect data.
Called from cg_ents.c for each visible missile each frame.
==================
*/
void CG_AddMissileFlare( centity_t *cent ) {
	lensFlareEffect_t	*eff;
	int					effIdx;
	int					i;
	refEntity_t			ent;
	vec3_t				toLight, screenPt, delta, elemOrigin;
	float				dist, depth, scale;

	if ( !cg_missileFlare.integer ) return;

	switch ( cent->currentState.weapon ) {
	case WP_ROCKET_LAUNCHER:	effIdx = lf.lensFlareEffectRocketLauncher;	break;
	default:					return;
	}
	if ( effIdx < 0 ) return;

	eff = &lf.missileLensFlareEffects[effIdx];

	VectorSubtract( cent->lerpOrigin, cg.refdef.vieworg, toLight );
	dist = VectorLength( toLight );
	if ( dist < 1.0f || dist > eff->range ) return;

	depth = DotProduct( toLight, cg.refdef.viewaxis[0] );
	if ( depth <= 0 ) return;

	scale = 1.0f - dist / eff->range;

	// screen-axis pivot: point on forward axis at missile depth
	VectorMA( cg.refdef.vieworg, depth, cg.refdef.viewaxis[0], screenPt );
	// delta drives ghost element positioning (pos != 1.0)
	VectorSubtract( cent->lerpOrigin, screenPt, delta );

	memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;

	for ( i = 0; i < eff->numLensFlares; i++ ) {
		lensFlare_t *fl = &eff->lensFlares[i];
		if ( !fl->shader ) continue;
		if ( fl->intensityThreshold > scale ) continue;

		// pos=1 → missile origin, pos=0 → screen axis pt, pos<0 → mirrored
		VectorMA( screenPt, fl->pos, delta, elemOrigin );
		VectorCopy( elemOrigin, ent.origin );

		ent.radius        = fl->size * 24.0f * scale;
		if ( ent.radius < 2.0f ) ent.radius = 2.0f;

		ent.customShader  = fl->shader;
		ent.shaderRGBA[0] = (byte)Com_Clamp( 0, 255, fl->rgba[0] * scale );
		ent.shaderRGBA[1] = (byte)Com_Clamp( 0, 255, fl->rgba[1] * scale );
		ent.shaderRGBA[2] = (byte)Com_Clamp( 0, 255, fl->rgba[2] * scale );
		ent.shaderRGBA[3] = (byte)Com_Clamp( 0, 255, fl->rgba[3] );

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
	ent.customShader = lf.warmFlowShader;
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
