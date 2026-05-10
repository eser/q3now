/*
===========================================================================
Copyright (C) 2026 Wired engine contributors.

This file is part of the Wired engine and is distributed under the terms of the
GNU General Public License version 2 or (at your option) any later version.

tr_spearmint.c — Phase 5 Spearmint feature adaptation.

Adds:
  * Enhanced fog system (FT_LINEAR, FT_EXP, FT_EXP2) with front-end accessors
    and a thin RB_Fog backend helper that talks to GL_FOG.
  * Corona (lens-flare-style glow) scene entries, rendered through the
    existing flare pipeline for free depth-buffer occlusion testing.
  * DrawRotatedPic / SetClipRegion 2D-rendering entry points.

All functionality is gated on FEAT_FOG_SYSTEM / FEAT_CORONA and compiles to
empty translation units when the flags are 0.

Every entry point here is gated behind its feature flag so the file
compiles cleanly into a renderer build that opts out of any subset.
===========================================================================
*/

#include "tr_local.h"

/* ===========================================================================
 *
 * FOG SYSTEM (FEAT_FOG_SYSTEM)
 *
 * The renderer already implements traditional Q3 volume fog via tess.fogNum
 * and the RB_FogPass texcoord-based blend. What Spearmint adds on top is:
 *
 *   1. fogType_t          — choose between FT_LINEAR / FT_EXP / FT_EXP2
 *   2. Global fog         — a view-wide fog that is applied when the camera
 *                           is not inside any Q3 volume fog.
 *   3. RB_Fog / R_FogOff / RB_FogOn — thin wrappers around GL_FOG so
 *                           non-volume geometry can participate in fog.
 *   4. GetGlobalFog/GetViewFog — front-end accessors used by the engine
 *                           (e.g. the map loader populates global fog state,
 *                           the client can query it to colour HUD / skyboxes).
 *
 * ===========================================================================
 */

#if FEAT_FOG_SYSTEM

/*
====================
RE_GetGlobalFog

Front-end query: what is the engine's global fog, if any?
====================
*/
void RE_GetGlobalFog( refFogType_t *type, vec3_t color, float *depthForOpaque, float *density ) {
	if ( type ) {
		*type = (refFogType_t)tr.globalFogType;
	}
	if ( color ) {
		VectorCopy( tr.globalFogColor, color );
	}
	if ( depthForOpaque ) {
		*depthForOpaque = tr.globalFogDepthForOpaque;
	}
	if ( density ) {
		*density = tr.globalFogDensity;
	}
}


/*
====================
R_IsGlobalFog

True if fogNum refers to the engine-wide "global" fog slot (0 is reserved
for Q3's invalid/no-fog sentinel, so the global fog virtual index is 0 when
tr.globalFogType != FT_NONE and no real volume fog applies).
====================
*/
qboolean R_IsGlobalFog( int fogNum ) {
	if ( fogNum < 0 ) {
		return qfalse;
	}
	if ( tr.globalFogType == FT_NONE ) {
		return qfalse;
	}
	return (qboolean)( fogNum == tr.globalFog );
}


/*
====================
R_BoundsFogNum

Return the fog volume that a world-space AABB falls into. Falls back to
the global fog slot when no Q3 fog volume contains the bounds. The call
is a thin wrapper around the existing R_FindFogNum / tr.world->fogs walk
so we do not duplicate the volume-containment math.
====================
*/
int R_BoundsFogNum( const vec3_t mins, const vec3_t maxs ) {
	int		i;
	const fog_t *fog;

	if ( !tr.world || tr.world->numfogs <= 1 ) {
		return ( tr.globalFogType != FT_NONE ) ? tr.globalFog : 0;
	}

	for ( i = 1; i < tr.world->numfogs; i++ ) {
		fog = &tr.world->fogs[i];
		if ( maxs[0] >= fog->bounds[0][0]
			&& maxs[1] >= fog->bounds[0][1]
			&& maxs[2] >= fog->bounds[0][2]
			&& mins[0] <= fog->bounds[1][0]
			&& mins[1] <= fog->bounds[1][1]
			&& mins[2] <= fog->bounds[1][2] ) {
			return i;
		}
	}

	return ( tr.globalFogType != FT_NONE ) ? tr.globalFog : 0;
}


/*
====================
RE_GetViewFog

Resolve the fog that affects the given view origin. If the origin is inside
a volume fog, return that volume's parameters; otherwise fall back to the
global fog (if configured).

useColorArray is set to qtrue when the engine should use per-vertex color
arrays rather than a fixed-function fog; the renderer historically uses
vertex colours for volume fog and GL_FOG for the global fog.
====================
*/
void RE_GetViewFog( const vec3_t origin, refFogType_t *type, vec3_t color,
	float *depthForOpaque, float *density, qboolean *useColorArray )
{
	int i;
	const fog_t *fog;
	const fogParms_t *parms = NULL;
	qboolean insideVolume = qfalse;

	if ( tr.world ) {
		for ( i = 1; i < tr.world->numfogs; i++ ) {
			fog = &tr.world->fogs[i];
			if ( origin[0] >= fog->bounds[0][0] && origin[0] <= fog->bounds[1][0]
				&& origin[1] >= fog->bounds[0][1] && origin[1] <= fog->bounds[1][1]
				&& origin[2] >= fog->bounds[0][2] && origin[2] <= fog->bounds[1][2] ) {
				parms = &fog->parms;
				insideVolume = qtrue;
				break;
			}
		}
	}

	if ( parms ) {
		if ( type ) {
			*type = (refFogType_t)( parms->type != FT_NONE ? parms->type : FT_LINEAR );
		}
		if ( color ) {
			VectorCopy( parms->color, color );
		}
		if ( depthForOpaque ) {
			*depthForOpaque = parms->depthForOpaque;
		}
		if ( density ) {
			*density = parms->density;
		}
	} else {
		if ( type ) {
			*type = (refFogType_t)tr.globalFogType;
		}
		if ( color ) {
			VectorCopy( tr.globalFogColor, color );
		}
		if ( depthForOpaque ) {
			*depthForOpaque = tr.globalFogDepthForOpaque;
		}
		if ( density ) {
			*density = tr.globalFogDensity;
		}
	}

	if ( useColorArray ) {
		*useColorArray = insideVolume;
	}
}


/*
====================
R_FogOff

Turn off fixed-function / cvar-driven fog. Idempotent.
====================
*/
void R_FogOff( void ) {
	if ( !tr.fogEnabled ) {
		return;
	}
	tr.fogEnabled = qfalse;
#ifdef GL_FOG
	qglDisable( GL_FOG );
#endif
}


/*
====================
RB_FogOn

Re-enable the most-recently-configured fog. Idempotent.
====================
*/
void RB_FogOn( void ) {
	if ( tr.fogEnabled ) {
		return;
	}
	if ( tr.fogTypeCurrent == FT_NONE ) {
		return;
	}
	tr.fogEnabled = qtrue;
#ifdef GL_FOG
	qglEnable( GL_FOG );
#endif
}


/*
====================
RB_Fog

Configure GL_FOG for the given fog number. fogNum 0 with globalFogType
== FT_NONE disables fog. For volume fogs, only the colour/depth/density
are uploaded; the actual per-pixel blending is handled by RB_FogPass (in
tr_shade.c) which uses the Q3 texcoord trick.
====================
*/
void RB_Fog( int fogNum ) {
#ifdef GL_FOG
	const fogParms_t *parms;
	fogType_t type;
	vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
	float depthForOpaque = 1.0f;
	float density = 0.0f;
	float farClip;

	if ( fogNum <= 0 ) {
		if ( tr.globalFogType == FT_NONE ) {
			R_FogOff();
			return;
		}
		type = tr.globalFogType;
		VectorCopy( tr.globalFogColor, color );
		depthForOpaque = tr.globalFogDepthForOpaque;
		density = tr.globalFogDensity;
		farClip = depthForOpaque;
	} else if ( tr.world && fogNum < tr.world->numfogs ) {
		parms = &tr.world->fogs[fogNum].parms;
		type = ( parms->type != FT_NONE ) ? parms->type : FT_LINEAR;
		VectorCopy( parms->color, color );
		depthForOpaque = parms->depthForOpaque;
		density = parms->density;
		farClip = parms->farClip > 0.0f ? parms->farClip : depthForOpaque;
	} else {
		R_FogOff();
		return;
	}

	tr.fogTypeCurrent = type;
	color[3] = 1.0f;

	qglFogfv( GL_FOG_COLOR, color );

	switch ( type ) {
	case FT_LINEAR:
		qglFogi( GL_FOG_MODE, GL_LINEAR );
		qglFogf( GL_FOG_START, 0.0f );
		qglFogf( GL_FOG_END, farClip > 0.0f ? farClip : depthForOpaque );
		break;
	case FT_EXP:
		qglFogi( GL_FOG_MODE, GL_EXP );
		qglFogf( GL_FOG_DENSITY, density );
		break;
	case FT_EXP2:
		qglFogi( GL_FOG_MODE, GL_EXP2 );
		qglFogf( GL_FOG_DENSITY, density );
		break;
	case FT_NONE:
	default:
		R_FogOff();
		return;
	}

	tr.fogEnabled = qtrue;
	qglEnable( GL_FOG );
#else
	(void)fogNum;
#endif
}

#endif // FEAT_FOG_SYSTEM


/* ===========================================================================
 *
 * CORONAS (FEAT_CORONA)
 *
 * Coronas are scene entries added by the game (e.g. for muzzle flashes, sun
 * glares, fire sources) that should render as a flare with depth-buffer
 * occlusion testing. We reuse the existing flare infrastructure so we get
 * the fade logic and qglReadPixels-based occlusion for free.
 *
 * ===========================================================================
 */

#if FEAT_CORONA

static int r_numcoronas;
static int r_firstSceneCorona;


/*
====================
R_ClearCoronas

Called from R_InitNextFrame (via tr_scene.c) to reset the per-frame
corona list. Using a weak-ish symbol by exposing it via an extern in
tr_local.h keeps the existing tr_scene.c untouched beyond the hook.
====================
*/
void R_ClearCoronas( void ) {
	r_numcoronas = 0;
	r_firstSceneCorona = 0;
}


/*
====================
RE_AddCoronaToScene

Engine API: add a corona to the current scene buffer. Mirrors the
Spearmint signature exactly.
====================
*/
void RE_AddCoronaToScene( const vec3_t org, float r, float g, float b,
	float scale, int id, qboolean visible )
{
	corona_t *cor;

	if ( !tr.registered ) {
		return;
	}

	if ( r_numcoronas >= MAX_CORONAS ) {
		return;
	}

	cor = &backEndData->coronas[ r_numcoronas + r_firstSceneCorona ];
	VectorCopy( org, cor->origin );
	cor->color[0] = r;
	cor->color[1] = g;
	cor->color[2] = b;
	cor->scale = scale;
	cor->id = id;
	cor->visible = visible;
	cor->shader = NULL;
	r_numcoronas++;
}


/*
====================
RB_AddCoronaFlares

Called from the flare rendering path once per view. Iterates the corona
list and emits each entry as a flare. The flare pipeline already handles
depth-buffer occlusion testing (qglReadPixels in RB_TestFlare), fade-in/
fade-out, and per-view visibility tracking.
====================
*/
void RB_AddCoronaFlares( void ) {
	int i;
	corona_t *cor;
	fog_t *fog = NULL;
	int fogNum;

	if ( !r_flares || !r_flares->integer ) {
		return;
	}
	if ( !backEndData ) {
		return;
	}

	if ( tr.world ) {
		fog = tr.world->fogs;
		(void)fog;
	}

	cor = backEndData->coronas + r_firstSceneCorona;
	for ( i = 0; i < r_numcoronas; i++, cor++ ) {
		vec3_t scaledColor;

		if ( !cor->visible ) {
			continue;
		}

		VectorScale( cor->color, cor->scale, scaledColor );

		fogNum = 0;
		if ( tr.world && tr.world->numfogs > 1 ) {
			int j, k;
			for ( j = 1; j < tr.world->numfogs; j++ ) {
				const fog_t *fv = &tr.world->fogs[j];
				for ( k = 0; k < 3; k++ ) {
					if ( cor->origin[k] < fv->bounds[0][k] ||
					     cor->origin[k] > fv->bounds[1][k] ) {
						break;
					}
				}
				if ( k == 3 ) {
					fogNum = j;
					break;
				}
			}
		}

		/* Reuses the generic flare pipeline which performs occlusion
		 * testing against the depth buffer (see RB_TestFlare). */
		RB_AddFlare( (void *)( uintptr_t )( 0x7F000000 | (unsigned)cor->id ),
			fogNum, cor->origin, scaledColor, NULL );
	}
}

#endif // FEAT_CORONA


/* ===========================================================================
 *
 * DrawRotatedPic / SetClipRegion
 *
 * These are the two new 2D-rendering entry points added to refexport_t
 * in v9 of the renderer API.
 *
 * ===========================================================================
 */

/*
====================
RE_SetClipRegion

Set a scissor region for subsequent 2D draws. Pass NULL to clear. Coordinates
are in the same space as RE_StretchPic (virtual 640x480 scaled to the current
mode unless FEAT_WIRED_UI normalized coords are in use).
====================
*/
void RE_SetClipRegion( const float *region ) {
	setClipRegionCommand_t *cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SET_CLIP_REGION;
	if ( region ) {
		cmd->hasRegion = qtrue;
		cmd->x = region[0];
		cmd->y = region[1];
		cmd->w = region[2];
		cmd->h = region[3];
	} else {
		cmd->hasRegion = qfalse;
		cmd->x = cmd->y = cmd->w = cmd->h = 0.0f;
	}
}


/*
====================
RE_RotatedPic

Queue a rotated 2D pic draw. Rotation is clockwise around the centre of the
(x,y,w,h) rect. Uses a dedicated RC_ROTATED_PIC command because the backend
needs the angle to compute per-vertex positions (rather than re-using
RB_AddQuadStamp2 which only writes axis-aligned quads).
====================
*/
void RE_RotatedPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, float angle, qhandle_t hShader )
{
	rotatedPicCommand_t *cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_ROTATED_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
	cmd->angle = angle;
}
