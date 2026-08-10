// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: JuHo X (BrightArena lens flares)
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/traps.h"
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

#if FEAT_LENS_FLARES

#define MAX_LENS_FLARE_ENTITIES   256
#define FLARE_DISTANCE            1500.0f
#define FLARES_PER_FRAME          48  // 16 was too low for Q1 maps (80 lights → 5-frame stale window); 48 covers up to ~128 lights in one pass
#define FLARE_MIN_LIGHT_RADIUS    20		// skip dim fill lights (ambient, pathway)
#define FLARE_MIN_HEIGHT_ABOVE    210		// light must be >= 210 units above ground below it

typedef struct {
	vec3_t		origin;
	float		radius;
	qboolean	visible;
	float		intensity;
	// optional direction-independent halo, set by a light entity's
	// "halo" key (or back-compat "corona" key, content) or implied for the cg_halo demo.
	qboolean	hasHalo;
	vec3_t		haloRgb;		// 0..1
	float		haloScale;
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

	// data-driven aesthetic tables (P5c-1) — built at init from the literal rows
	// below, with the shader handles resolved. Map = 4 layers, powerup = 3.
	lensFlare_t			mapLayers[4];
	lensFlare_t			powerupLayers[3];
} lf;

// Powerup palette (Tier-3): PW_* → rgb. a==0 marks "no flare" (PW_INVIS) so the
// emitter early-returns exactly as the old switch did. default (unlisted) = 200³.
static const lfPowerupColor_t lf_powerupPalette[PW_NUM_POWERUPS] = {
	[PW_QUAD]       = { 100,  80, 255, 255 },
	[PW_BERSERK]    = { 255,   0,   0, 255 },
	[PW_BATTLESUIT] = { 255, 200,  60, 255 },
	[PW_HASTE]      = { 255, 120,  40, 255 },
	[PW_REGEN]      = {  60, 255, 100, 255 },
	[PW_FLIGHT]     = { 200, 200, 255, 255 },
	[PW_INVIS]      = {   0,   0,   0,   0 },   // a==0 → no flare
};

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
		Com_Log( SEV_WARN, LOG_CH(ch_cgame), "lens flare file too large: '%s'\n", path );
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

	// Defaults reproduce today's missile render path exactly: absolute RGB scaled by
	// the missile `scale`, raw (unscaled, clamped) alpha, scale driver, no rotation.
	fl->pos                = 1.0f;
	fl->size               = 1.0f;
	fl->rgba[0] = fl->rgba[1] = fl->rgba[2] = fl->rgba[3] = 255.0f;
	fl->radiusBase         = 0.0f;
	fl->radiusScale        = 0.0f;
	fl->colorMode          = LF_COLOR_ABSOLUTE_SCALED;   // missile RGB rides scale
	fl->alphaMode          = LF_ALPHA_RAW;               // missile alpha is unscaled
	fl->driver             = LF_DRIVE_SCALE;
	fl->colorFactor        = 1.0f;
	fl->gate               = 0.0f;
	fl->rotationOffset     = 0.0f;
	fl->rotationRollFactor = 0.0f;                       // 0 → ent.rotation 0 → no spin (byte-identical)

	while ( 1 ) {
		token = COM_Parse( &parser, p );
		if ( !token[0] ) {
			Com_Log( SEV_WARN, LOG_CH(ch_cgame), "unexpected end in lens flare '%s'\n", effectName );
			return qfalse;
		}
		if ( !Q_stricmp( token, "}" ) ) break;

		if ( !Q_stricmp( token, "shader" ) ) {
			token = COM_Parse( &parser, p );
			if ( token[0] ) fl->shader = trap_R_RegisterShaderNoMip( token );
		} else if ( !Q_stricmp( token, "mode" ) ) {
			// retired field (was reflexion/glare/star, never read at render) — accept
			// and ignore for back-compat with shipped .lfs (e.g. `mode "star"`).
			COM_Parse( &parser, p );
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
			// .lfs ships 4 values (offset yaw pitch roll); yaw/pitch don't map to a
			// 2D billboard roll, so consume-and-discard them. offset + roll are wired
			// to ent.rotation (the sprite spin) in CG_EmitFlareLayer.
			fl->rotationOffset     = Com_Clamp( -360, 360, atof( COM_Parse( &parser, p ) ) );
			COM_Parse( &parser, p );   // yaw  (ignored)
			COM_Parse( &parser, p );   // pitch (ignored)
			fl->rotationRollFactor = atof( COM_Parse( &parser, p ) );
		} else if ( !Q_stricmp( token, "fadeAngleFactor" ) || !Q_stricmp( token, "entityAngleFactor" ) ) {
			// retired fields (parsed but never read) — accept and ignore for back-compat.
			COM_Parse( &parser, p );
		} else if ( !Q_stricmp( token, "intensityThreshold" ) ) {
			fl->intensityThreshold = Com_Clamp( 0, 0.99f, atof( COM_Parse( &parser, p ) ) );
		} else {
			Com_Log( SEV_WARN, LOG_CH(ch_cgame), "unknown token '%s' in flare '%s'\n", token, effectName );
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
		Com_Log( SEV_WARN, LOG_CH(ch_cgame), "expected '{' after effect '%s'\n", eff->name );
		return qfalse;
	}

	eff->range     = 400.0f;
	eff->rangeSqr  = 160000.0f;
	eff->fadeAngle = 20.0f;

	while ( 1 ) {
		token = COM_Parse( &parser, p );
		if ( !token[0] ) {
			Com_Log( SEV_WARN, LOG_CH(ch_cgame), "unexpected end in effect '%s'\n", eff->name );
			return qfalse;
		}
		if ( !Q_stricmp( token, "}" ) ) break;

		if ( !Q_stricmp( token, "{" ) ) {
			if ( eff->numLensFlares >= MAX_LENSFLARES_PER_EFFECT ) {
				Com_Log( SEV_WARN, LOG_CH(ch_cgame), "too many sub-flares in '%s' (max %d)\n",
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
			Com_Log( SEV_WARN, LOG_CH(ch_cgame), "unknown token '%s' in effect '%s'\n", token, eff->name );
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

	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Lens flares: %d missile effects loaded (bfg=%d rl=%d)\n",
	           lf.numMissileLensFlareEffects,
	           lf.lensFlareEffectBFG, lf.lensFlareEffectRocketLauncher );
}

// Time-driven spin rate: ent.rotation (degrees) = rotationOffset + rotationRollFactor
// * cg.time * LF_ROT_SPEED. ~30 deg/s at rollFactor 1. Only used when a row sets a
// non-zero rotationRollFactor (today's tables set it 0 → no spin → byte-identical).
#define LF_ROT_SPEED 0.03f

/*
==================
CG_BuildFlareTables

Populate the map + powerup aesthetic tables from the literal rows, resolving the
shader handles (registered just above). These reproduce the former hard-coded
CG_AddMapFlares / CG_AddPowerupFlare literals byte-identically — every radius,
colour, alpha, and threshold is the same constant the C code used. rotationOffset
and rotationRollFactor are 0 so ent.rotation is exactly 0.0f (no spin) — the
sprite's axis-aligned fast path, identical to today.
==================
*/
static void CG_BuildFlareTables( void ) {
	lensFlare_t *m = lf.mapLayers;
	lensFlare_t *u = lf.powerupLayers;

	memset( lf.mapLayers, 0, sizeof( lf.mapLayers ) );
	memset( lf.powerupLayers, 0, sizeof( lf.powerupLayers ) );

	// ── map flares: 4-layer JJ stack, driven by `scale`, absolute colour ──
	// L1 cool glow (always): radius 20+56*scale, white, alpha 45*scale.
	m[0].shader = lf.coolGlowShader;   m[0].radiusBase = 20; m[0].radiusScale = 56;
	m[0].rgba[0] = 255; m[0].rgba[1] = 255; m[0].rgba[2] = 255; m[0].rgba[3] = 45;
	m[0].colorMode = LF_COLOR_ABSOLUTE; m[0].alphaMode = LF_ALPHA_SCALED; m[0].driver = LF_DRIVE_SCALE; m[0].gate = -1.0f; // always (the scale<0.01 pre-gate already applied)
	// L2 star (scale>0.2): radius 10+32*scale, white*scale, alpha 50*scale.
	m[1].shader = lf.starShader;       m[1].radiusBase = 10; m[1].radiusScale = 32;
	m[1].rgba[0] = 255; m[1].rgba[1] = 255; m[1].rgba[2] = 255; m[1].rgba[3] = 50;
	m[1].colorMode = LF_COLOR_ABSOLUTE_SCALED; m[1].alphaMode = LF_ALPHA_SCALED; m[1].driver = LF_DRIVE_SCALE; m[1].gate = 0.2f;
	// L3 blue streak (scale>0.3): radius 14+40*scale, white, alpha 35*scale.
	m[2].shader = lf.blueStreakShader; m[2].radiusBase = 14; m[2].radiusScale = 40;
	m[2].rgba[0] = 255; m[2].rgba[1] = 255; m[2].rgba[2] = 255; m[2].rgba[3] = 35;
	m[2].colorMode = LF_COLOR_ABSOLUTE; m[2].alphaMode = LF_ALPHA_SCALED; m[2].driver = LF_DRIVE_SCALE; m[2].gate = 0.3f;
	// L4 warm core (scale>0.4): radius 4+8*scale, warm white, alpha 80*scale.
	m[3].shader = lf.warmFlowShader;   m[3].radiusBase = 4;  m[3].radiusScale = 8;
	m[3].rgba[0] = 255; m[3].rgba[1] = 250; m[3].rgba[2] = 240; m[3].rgba[3] = 80;
	m[3].colorMode = LF_COLOR_ABSOLUTE; m[3].alphaMode = LF_ALPHA_SCALED; m[3].driver = LF_DRIVE_SCALE; m[3].gate = 0.4f;

	// ── powerup flares: 3 layers, radius rides `pulse`, colour = palette*factor,
	//    alpha is a fixed const (NO pulse) ──
	// L1 glow: radius 40*pulse, palette*0.5, alpha 50.
	u[0].shader = lf.powerupGlowShader; u[0].radiusBase = 0; u[0].radiusScale = 40;
	u[0].rgba[3] = 50; u[0].colorMode = LF_COLOR_PALETTE; u[0].colorFactor = 0.5f; u[0].alphaMode = LF_ALPHA_CONST; u[0].driver = LF_DRIVE_PULSE;
	// L2 core: radius 14*pulse, palette*1.0, alpha 60.
	u[1].shader = lf.warmFlowShader;    u[1].radiusBase = 0; u[1].radiusScale = 14;
	u[1].rgba[3] = 60; u[1].colorMode = LF_COLOR_PALETTE; u[1].colorFactor = 1.0f; u[1].alphaMode = LF_ALPHA_CONST; u[1].driver = LF_DRIVE_PULSE;
	// L3 star: radius 24*pulse, palette*0.7, alpha 30.
	u[2].shader = lf.starShader;        u[2].radiusBase = 0; u[2].radiusScale = 24;
	u[2].rgba[3] = 30; u[2].colorMode = LF_COLOR_PALETTE; u[2].colorFactor = 0.7f; u[2].alphaMode = LF_ALPHA_CONST; u[2].driver = LF_DRIVE_PULSE;
}

/*
==================
CG_EmitFlareLayer

Compose + submit one RT_SPRITE layer from a config row + a driver value (the map
`scale` or the powerup `pulse`). `pal` is the resolved powerup palette colour (NULL
for non-palette layers). The three selectors reproduce the per-type asymmetries
byte-identically. The caller has already set ent->reType / ent->origin and (for
missiles) ent->radius; this sets radius (when radiusScale != 0), shader, rgba,
rotation, then draws. rotationOffset/RollFactor 0 → ent->rotation 0 → no spin.
==================
*/
static void CG_EmitFlareLayer( refEntity_t *ent, const lensFlare_t *fl, float driver, const byte *pal ) {
	int r, g, b, a;

	if ( fl->radiusScale != 0.0f || fl->radiusBase != 0.0f )
		ent->radius = fl->radiusBase + fl->radiusScale * driver;
	ent->customShader = fl->shader;

	// colour
	switch ( fl->colorMode ) {
	case LF_COLOR_ABSOLUTE_SCALED:
		r = (byte)( fl->rgba[0] * driver );
		g = (byte)( fl->rgba[1] * driver );
		b = (byte)( fl->rgba[2] * driver );
		break;
	case LF_COLOR_PALETTE:
		if ( pal ) {
			r = (byte)( pal[0] * fl->colorFactor );
			g = (byte)( pal[1] * fl->colorFactor );
			b = (byte)( pal[2] * fl->colorFactor );
		} else {
			// Defensive: only the powerup-layer caller supplies a palette;
			// map-layer callers pass pal=NULL and the table assigns them
			// LF_COLOR_ABSOLUTE today. Fall back to the layer's own rgba
			// rather than dereferencing NULL if that coupling ever changes.
			r = (byte)fl->rgba[0];
			g = (byte)fl->rgba[1];
			b = (byte)fl->rgba[2];
		}
		break;
	case LF_COLOR_ABSOLUTE:
	default:
		r = (byte)fl->rgba[0];
		g = (byte)fl->rgba[1];
		b = (byte)fl->rgba[2];
		break;
	}

	// alpha
	switch ( fl->alphaMode ) {
	case LF_ALPHA_RAW:
		a = (byte)Com_Clamp( 0, 255, fl->rgba[3] );
		break;
	case LF_ALPHA_CONST:
		a = (byte)fl->rgba[3];
		break;
	case LF_ALPHA_SCALED:
	default:
		a = (byte)( fl->rgba[3] * driver );
		break;
	}

	ent->shaderRGBA[0] = r;
	ent->shaderRGBA[1] = g;
	ent->shaderRGBA[2] = b;
	ent->shaderRGBA[3] = a;

	ent->rotation = fl->rotationOffset + fl->rotationRollFactor * cg.time * LF_ROT_SPEED;

	trap_R_AddRefEntityToScene( ent );
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
	qboolean	hasHalo;
	vec3_t		haloRgb;
	float		haloScale;

	memset( &lf, 0, sizeof( lf ) );

	// shaders for map + powerup flare rendering
	lf.warmFlowShader    = trap_R_RegisterShader( "lfWarmGlow" );
	lf.starShader        = trap_R_RegisterShader( "lfStar" );
	lf.coolGlowShader    = trap_R_RegisterShader( "lfCoolGlow" );
	lf.blueStreakShader  = trap_R_RegisterShader( "lfBlueStreak" );
	lf.powerupGlowShader = trap_R_RegisterShader( "lfPowerupGlow" );

	CG_BuildFlareTables();

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
			hasHalo = qfalse;
			VectorClear( haloRgb );
			haloScale = 0.0f;
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
					e->hasHalo = hasHalo;
					VectorCopy( haloRgb, e->haloRgb );
					e->haloScale = haloScale;
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
		} else if ( !Q_stricmp( key, "halo" ) || !Q_stricmp( key, "corona" ) ) {
			// "halo" is canonical; "corona" stays as a back-compat content-key alias
			// (existing maps). "r g b scale" (0..1 colour); blank/short → white, scale 1.
			hasHalo = qtrue;
			if ( sscanf( value, "%f %f %f %f", &haloRgb[0], &haloRgb[1], &haloRgb[2], &haloScale ) < 4 ) {
				VectorSet( haloRgb, 1.0f, 1.0f, 1.0f );
				haloScale = 1.0f;
			}
		}
	}

	if ( lf.numEntities > 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Lens flares: %i light entities found\n", lf.numEntities );
	}
}

/*
==================
CG_UpdateMapFlareVisibility

Refresh one map-flare's visibility + drive its intensity ramp. Extracted verbatim
from the former in-loop body so both the oracle-live (all-flares) and oracle-off
(round-robin) iteration shapes share identical per-flare logic. The per-id oracle vs
CG_Trace choice is preserved: a not-yet-warmed id (GetLensVisibility qfalse) still
falls back to CG_Trace for THAT flare — no first-frame flicker.
==================
*/
static void CG_UpdateMapFlareVisibility( int idx ) {
	lensFlareEntity_t *e = &lf.entities[idx];
	vec3_t  dir;
	float   dist;
	float   vis;
	trace_t tr;

	VectorSubtract( e->origin, cg.refdef.vieworg, dir );
	dist = VectorLength( dir );

	if ( dist > FLARE_DISTANCE || dist < 1 ) {
		e->visible = qfalse;
		vis = 0.0f;
	} else if ( trap_R_GetLensVisibility( idx, &vis ) ) {
		// GPU oracle: smooth area visibility (0 = occluded, 1 = clear).
		e->visible = ( vis > 0.0f );
	} else {
		// Fallback (no oracle for this id): the original CPU trace, binary occlusion.
		CG_Trace( &tr, cg.refdef.vieworg, NULL, NULL, e->origin, -1, CONTENTS_SOLID );
		e->visible = ( tr.fraction >= 0.99f );
		vis = e->visible ? 1.0f : 0.0f;
	}

	if ( e->visible ) {
		// smooth intensity ramp (fade in), scaled by the oracle's area factor so
		// partial occlusion settles to a partial intensity (a smoother fade than
		// the binary trace; never drops a layer — scale-gating is unchanged).
		float target = vis;
		if ( e->intensity < target ) {
			e->intensity += cg.frametime * 0.003f;
			if ( e->intensity > target ) e->intensity = target;
		} else if ( e->intensity > target ) {
			e->intensity -= cg.frametime * 0.005f;
			if ( e->intensity < target ) e->intensity = target;
		}
	} else {
		if ( e->intensity > 0 ) {
			e->intensity -= cg.frametime * 0.005f;
			if ( e->intensity < 0 ) e->intensity = 0;
		}
	}
}

/*
==================
CG_AddMapFlares

Renders map light lens flares. Called each frame from CG_DrawActiveFrame.
Uses distance culling + (oracle-live) per-frame all-flare visibility, or
(oracle-off) round-robin interleaved CG_Trace checks.
==================
*/
static void CG_AddMapFlares( void ) {
	int			i, checked;
	float		dist;
	vec3_t		dir;
	refEntity_t	ent;
	float		dot, scale;
	vec3_t		screenDir;

	if ( lf.numEntities == 0 ) {
		return;
	}

	// Register every in-range light with the GPU lens occlusion oracle this frame.
	// The oracle depth-tests them all in one batched pass (no round-robin staleness),
	// and returns a smooth 0..1 visibility next frame. Emit-and-forget: one coarse
	// {origin,radius,id} descriptor per source (the gpu-offload-principle's thin
	// channel — the GPU does the occlusion, cgame keeps the sprite composition).
	for ( i = 0; i < lf.numEntities; i++ ) {
		lensFlareEntity_t *e = &lf.entities[i];
		lensSourceDesc_t   ld;

		VectorSubtract( e->origin, cg.refdef.vieworg, dir );
		dist = VectorLength( dir );
		if ( dist > FLARE_DISTANCE || dist < 1 ) {
			continue;
		}
		memset( &ld, 0, sizeof( ld ) );
		ld.id = i;                  // stable per-map index = the oracle slot key
		VectorCopy( e->origin, ld.origin );
		ld.radius = ( e->radius > 0.0f ) ? e->radius : 16.0f;
		trap_R_AddLensSourceToScene( &ld );
	}

	// Visibility update. The GPU oracle depth-tests EVERY registered source each
	// frame (the dispatch is ⌈count/64⌉ workgroups, unthrottled — the CPU 48-cap
	// never gated it). So when the oracle is live, the per-flare read is just an
	// SSBO fetch of an already-fresh result: iterate ALL flares each frame for
	// zero-staleness visibility (no FLARES_PER_FRAME cap, no round-robin). When the
	// oracle is OFF (r_lens 0 / GL backend / pipeline absent), the CG_Trace fallback
	// is a per-flare BSP raycast — KEEP the FLARES_PER_FRAME round-robin to bound
	// that CPU cost (byte-identical to pre-oracle). Liveness is probed via the
	// per-id GetLensVisibility return (qtrue iff the oracle is live for this backend)
	// — robust to GL where r_lens may be 1 but no pipeline exists (→ qfalse → capped).
	{
		float    probeVis;
		qboolean oracleLive = ( lf.numEntities > 0 ) && trap_R_GetLensVisibility( 0, &probeVis );

		if ( oracleLive ) {
			// Oracle live: refresh ALL flares this frame, no throttle.
			for ( i = 0; i < lf.numEntities; i++ )
				CG_UpdateMapFlareVisibility( i );
		} else {
			// Oracle off: the original FLARES_PER_FRAME=48 round-robin + CG_Trace,
			// exactly as pre-P5c-3 (bounds the per-flare trace cost).
			checked = 0;
			while ( checked < FLARES_PER_FRAME && checked < lf.numEntities ) {
				CG_UpdateMapFlareVisibility( lf.checkIndex );
				lf.checkIndex = ( lf.checkIndex + 1 ) % lf.numEntities;
				checked++;
			}
		}

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

		// 4-layer JJ stack from the config table, gated on `scale` (the gate < 0
		// sentinel = always; the L1 row uses it since the scale<0.01 pre-gate above
		// already applied). Absolute colour, scale-driven radius/alpha — byte-identical.
		{
			int li;
			for ( li = 0; li < 4; li++ ) {
				if ( scale > lf.mapLayers[li].gate )
					CG_EmitFlareLayer( &ent, &lf.mapLayers[li], scale, NULL );
			}
		}

	}

	// Direction-independent halo pass. Separate from the directional lens-
	// flare loop above (no facing cull — a halo looks the same from any angle).
	// Off by default (cg_halo 0 → no emission → byte-identical). When on, emit a
	// halo at each in-range light: content lights ("halo"/"corona" key) use their
	// colour/scale; otherwise the demo shows a neutral white halo. Occlusion comes
	// from the shared lens oracle (register on the halo band, read back the
	// visibility), with a CG_Trace fall-back when no GPU oracle is available.
	if ( cg_halo.integer ) {
		for ( i = 0; i < lf.numEntities; i++ ) {
			lensFlareEntity_t *e = &lf.entities[i];
			lensSourceDesc_t  ld;
			haloDesc_t        cd;
			trace_t           tr;
			float             vis;
			qboolean          visible;
			int               haloId;

			VectorSubtract( e->origin, cg.refdef.vieworg, dir );
			dist = VectorLength( dir );
			if ( dist > FLARE_DISTANCE || dist < 1 )
				continue;

			haloId = LENS_BAND_HALO + ( i % LENS_BAND_ENTSPAN );

			// Register on the halo oracle band, then read last frame's visibility.
			memset( &ld, 0, sizeof( ld ) );
			ld.id = haloId;
			VectorCopy( e->origin, ld.origin );
			ld.radius = ( e->radius > 0.0f ) ? e->radius : 16.0f;
			trap_R_AddLensSourceToScene( &ld );

			if ( trap_R_GetLensVisibility( haloId, &vis ) ) {
				visible = ( vis > 0.0f );
			} else {
				// No GPU oracle (GL / r_lens off): line-of-sight trace fallback.
				CG_Trace( &tr, cg.refdef.vieworg, NULL, NULL, e->origin, -1, CONTENTS_SOLID );
				visible = ( tr.fraction >= 0.99f );
			}

			memset( &cd, 0, sizeof( cd ) );
			cd.id = haloId;
			VectorCopy( e->origin, cd.origin );
			if ( e->hasHalo ) {
				cd.r = e->haloRgb[0]; cd.g = e->haloRgb[1]; cd.b = e->haloRgb[2];
				cd.scale = e->haloScale;
			} else {
				cd.r = cd.g = cd.b = 1.0f;   // demo: neutral white halo
				cd.scale = 1.0f;
			}
			cd.visible = visible;
			trap_R_AddHaloToScene( &cd );
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

	// GPU occlusion gate (lens-glow P5b). Missile flares had NO visibility test —
	// they drew through walls. Register the missile with the shared occlusion oracle
	// and fold its visibility into the flare intensity so a rocket behind geometry
	// stops flaring through it. id is banded into the missile sub-range so it never
	// aliases map-flare / powerup slots. When the oracle is unavailable (r_lens off /
	// GL backend / not-yet-warmed) GetLensVisibility returns qfalse → draw as today
	// (no occlusion = the pre-P5b behaviour, byte-identical). The ghost-chain +
	// sub-flare composition below is UNCHANGED — only `scale` gets the occlusion fold.
	{
		lensSourceDesc_t ld;
		float            vis;
		memset( &ld, 0, sizeof( ld ) );
		ld.id = LENS_BAND_MISSILE + ( cent->currentState.number % LENS_BAND_ENTSPAN );
		VectorCopy( cent->lerpOrigin, ld.origin );
		ld.radius = 24.0f;   // probe disc radius (texels)
		trap_R_AddLensSourceToScene( &ld );
		if ( trap_R_GetLensVisibility( ld.id, &vis ) ) {
			if ( vis <= 0.0f )
				return;          // fully occluded → no flare this frame (the bug-fix)
			scale *= vis;        // partial occlusion → proportional dim (smooth)
		}
	}

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

		// Missile radius + colour stay inline: the missile path Com_Clamps RGB*scale
		// and the radius is its own size*24*scale topology (the map/powerup helper
		// neither clamps nor uses this radius form) — keep it byte-identical here.
		ent.radius        = fl->size * 24.0f * scale;
		if ( ent.radius < 2.0f ) ent.radius = 2.0f;

		ent.customShader  = fl->shader;
		ent.shaderRGBA[0] = (byte)Com_Clamp( 0, 255, fl->rgba[0] * scale );
		ent.shaderRGBA[1] = (byte)Com_Clamp( 0, 255, fl->rgba[1] * scale );
		ent.shaderRGBA[2] = (byte)Com_Clamp( 0, 255, fl->rgba[2] * scale );
		ent.shaderRGBA[3] = (byte)Com_Clamp( 0, 255, fl->rgba[3] );

		// Rotation wire (P5c-1): a .lfs flare with a non-zero rotation spins; the
		// shipped rocket_launcher uses 0 → ent.rotation 0 → axis-aligned (byte-identical).
		ent.rotation = fl->rotationOffset + fl->rotationRollFactor * cg.time * LF_ROT_SPEED;

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
	byte		pal[3];
	float		pulse;

	if ( !cg_powerupFlares.integer ) {
		return;
	}

	// per-powerup colour from the Tier-3 palette. a==0 (PW_INVIS) → no flare; an
	// out-of-range / unlisted tag → the default grey 200,200,200 (matches the old
	// switch's default + PW_INVIS→return exactly).
	if ( powerupTag >= 0 && powerupTag < PW_NUM_POWERUPS && lf_powerupPalette[powerupTag].a != 0 ) {
		pal[0] = lf_powerupPalette[powerupTag].r;
		pal[1] = lf_powerupPalette[powerupTag].g;
		pal[2] = lf_powerupPalette[powerupTag].b;
	} else if ( powerupTag == PW_INVIS ) {
		return;							// invisible = no flare
	} else {
		pal[0] = pal[1] = pal[2] = 200;
	}

	pulse = 0.7f + 0.3f * sin( cg.time * 0.004f + cent->currentState.number * 2.3f );

	// GPU occlusion gate (lens-glow P5b). Powerup flares had NO visibility test —
	// they drew through walls. Register the pickup with the oracle (origin matches
	// the +16Z draw position below) and fold its visibility into the pulse so a
	// powerup behind geometry stops glowing through it. id banded into the powerup
	// sub-range (no alias with map-flare / missile slots). qfalse (oracle off) →
	// draw as today (pre-P5b, byte-identical). The pulse / palette / INVIS-skip /
	// +16Z / 3-layer composition below are UNCHANGED — only `pulse` gets the fold.
	{
		lensSourceDesc_t ld;
		float            vis;
		memset( &ld, 0, sizeof( ld ) );
		ld.id = LENS_BAND_POWERUP + ( cent->currentState.number % LENS_BAND_ENTSPAN );
		VectorCopy( cent->lerpOrigin, ld.origin );
		ld.origin[2] += 16;          // match the draw position
		ld.radius = 24.0f;           // probe disc radius (texels)
		trap_R_AddLensSourceToScene( &ld );
		if ( trap_R_GetLensVisibility( ld.id, &vis ) ) {
			if ( vis <= 0.0f )
				return;              // fully occluded → no flare this frame (the bug-fix)
			pulse *= vis;            // partial occlusion → proportional dim
		}
	}

	memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 16;	// slightly above item center

	// 3-layer pulsing glow from the config table: radius rides `pulse`, colour =
	// palette*factor, alpha is a fixed const (NO pulse). Byte-identical.
	{
		int li;
		for ( li = 0; li < 3; li++ )
			CG_EmitFlareLayer( &ent, &lf.powerupLayers[li], pulse, pal );
	}
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
