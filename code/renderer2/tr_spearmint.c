/*
===========================================================================
Copyright (C) 2026 q3now contributors.

This file is part of q3now and is distributed under the terms of the
GNU General Public License version 2 or (at your option) any later version.

tr_spearmint.c — Phase 5 Spearmint feature adaptation (GL2 renderer).

Adds:
  * Enhanced fog system — fogType_t + global fog state reachable by shaders
    (GL2 already pushes fog data as uniforms, so RB_Fog is a state hook).
  * Corona scene entries, rendered through the existing flare pipeline.
  * DrawRotatedPic / SetClipRegion 2D-rendering entry points.

All functionality is gated on FEAT_FOG_SYSTEM / FEAT_CORONA.
===========================================================================
*/

#include "tr_local.h"


/* ===========================================================================
 * FOG SYSTEM
 * ===========================================================================
 */

#if FEAT_FOG_SYSTEM

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


qboolean R_IsGlobalFog( int fogNum ) {
	if ( fogNum < 0 ) {
		return qfalse;
	}
	if ( tr.globalFogType == FT_NONE ) {
		return qfalse;
	}
	return (qboolean)( fogNum == tr.globalFog );
}


int R_BoundsFogNum( const vec3_t mins, const vec3_t maxs ) {
	int i;
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


void R_FogOff( void ) {
	tr.fogEnabled = qfalse;
}


void RB_FogOn( void ) {
	if ( tr.fogTypeCurrent == FT_NONE ) {
		return;
	}
	tr.fogEnabled = qtrue;
}


/*
====================
RB_Fog (GL2)

GL2 pushes fog parameters to shaders as uniforms (u_FogDistance etc.) so the
RB_Fog hook here just tracks the current fog so GLSL programs can bind the
right uniforms on the next draw. The actual uniform upload happens in
tr_shade.c / tr_glsl.c.
====================
*/
void RB_Fog( int fogNum ) {
	if ( fogNum <= 0 ) {
		if ( tr.globalFogType == FT_NONE ) {
			tr.fogEnabled = qfalse;
			return;
		}
		tr.fogTypeCurrent = tr.globalFogType;
		tr.fogEnabled = qtrue;
		return;
	}

	if ( tr.world && fogNum < tr.world->numfogs ) {
		const fogParms_t *parms = &tr.world->fogs[fogNum].parms;
		tr.fogTypeCurrent = ( parms->type != FT_NONE ) ? parms->type : FT_LINEAR;
		tr.fogEnabled = qtrue;
		return;
	}

	tr.fogEnabled = qfalse;
}

#endif // FEAT_FOG_SYSTEM


/* ===========================================================================
 * CORONAS
 * ===========================================================================
 */

#if FEAT_CORONA

static int r_numcoronas;
static int r_firstSceneCorona;


void R_ClearCoronas( void ) {
	r_numcoronas = 0;
	r_firstSceneCorona = 0;
}


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


void RB_AddCoronaFlares( void ) {
	int i;
	corona_t *cor;
	int fogNum;

	if ( !r_flares || !r_flares->integer ) {
		return;
	}
	if ( !backEndData ) {
		return;
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

		RB_AddFlare( (void *)( uintptr_t )( 0x7F000000 | (unsigned)cor->id ),
			fogNum, cor->origin, scaledColor, NULL );
	}
}

#endif // FEAT_CORONA


/* ===========================================================================
 * DrawRotatedPic / SetClipRegion
 * ===========================================================================
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
