// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2010-2019 Zack Middleton & Spearmint contributors
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
tr_spearmint.c — Spearmint feature adaptation (Vulkan renderer).

Adds:
  * Enhanced fog system — fogType_t + real push-constant upload via
    vk_update_fog_push. The main pipeline layout reserves a fragment-stage
    push constant range (offset 64, size 32) for this purpose.
  * Corona scene entries, rendered through the existing flare pipeline.
  * DrawRotatedPic / SetClipRegion 2D-rendering entry points.

All functionality is gated on FEAT_FOG_SYSTEM / FEAT_CORONA.
*/

#include "tr_local.h"
#ifdef USE_VULKAN
#include "vk.h"
#endif


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
	const fog_t *fog;

	if ( !tr.world || tr.world->numfogs <= 1 ) {
		return ( tr.globalFogType != FT_NONE ) ? tr.globalFog : 0;
	}

	for ( int i = 1; i < tr.world->numfogs; i++ ) {
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
	const fog_t *fog;
	const fogParms_t *parms = NULL;
	qboolean insideVolume = qfalse;

	if ( tr.world ) {
		for ( int i = 1; i < tr.world->numfogs; i++ ) {
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
#ifdef USE_VULKAN
	if ( vk.active && vk.cmd ) {
		vec4_t zeroColor = { 0, 0, 0, 1 };
		vk_update_fog_push( zeroColor, FT_NONE, 0.0f, 0.0f, qfalse );
	}
#endif
}


void RB_FogOn( void ) {
	if ( tr.fogTypeCurrent == FT_NONE ) {
		return;
	}
	tr.fogEnabled = qtrue;
#ifdef USE_VULKAN
	if ( vk.active && vk.cmd ) {
		vec4_t color;
		VectorCopy( tr.globalFogColor, color );
		color[3] = 1.0f;
		vk_update_fog_push( color, (int)tr.fogTypeCurrent,
			tr.globalFogDensity, tr.globalFogDepthForOpaque, qtrue );
	}
#endif
}


/*
====================
RB_Fog (Vulkan)

Upload fog parameters to the GPU via a real fragment-stage push constant
(see vk_update_fog_push). The existing texcoord-based fog pipeline is
unchanged — this call sits alongside it so shaders that want to read the
enhanced fog state can do so.
====================
*/
void RB_Fog( int fogNum ) {
	fogType_t type;
	vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
	float depthForOpaque = 1.0f;
	float density = 0.0f;
	float farClip = 0.0f;

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
		const fogParms_t *parms = &tr.world->fogs[fogNum].parms;
		type = ( parms->type != FT_NONE ) ? parms->type : FT_LINEAR;
		VectorCopy( parms->color, color );
		depthForOpaque = parms->depthForOpaque;
		density = parms->density;
		farClip = ( parms->farClip > 0.0f ) ? parms->farClip : depthForOpaque;
	} else {
		R_FogOff();
		return;
	}

	tr.fogTypeCurrent = type;
	tr.fogEnabled = qtrue;
	color[3] = 1.0f;

#ifdef USE_VULKAN
	if ( vk.active && vk.cmd ) {
		vk_update_fog_push( color, (int)type, density, farClip, qtrue );
	}
#endif

	(void)depthForOpaque;
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
	corona_t *cor;

	if ( !r_flares || !r_flares->integer ) {
		return;
	}
	if ( !backEndData ) {
		return;
	}

	cor = backEndData->coronas + r_firstSceneCorona;
	for ( int i = 0; i < r_numcoronas; i++, cor++ ) {
		vec3_t scaledColor;

		if ( !cor->visible ) {
			continue;
		}

		VectorScale( cor->color, cor->scale, scaledColor );

		int fogNum = 0;
		if ( tr.world && tr.world->numfogs > 1 ) {
			for ( int j = 1; j < tr.world->numfogs; j++ ) {
				const fog_t *fv = &tr.world->fogs[j];
				for ( int k = 0; k < 3; k++ ) {
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
