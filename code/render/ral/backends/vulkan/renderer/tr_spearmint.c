// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2010-2019 Zack Middleton & Spearmint contributors
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
tr_spearmint.c — Spearmint feature adaptation (Vulkan renderer).

Adds:
  * Enhanced fog system — fogType_t + portable per-draw UBO state via
    vk_update_fog_uniform.
  * Halo scene entries, rendered through the existing flare pipeline.
  * DrawRotatedPic / SetClipRegion 2D-rendering entry points.

Halo functionality remains gated on FEAT_HALO; enhanced fog is permanent.
*/

#include "tr_local.h"
#include "../../../../frontend/wired_fog_runtime_policy.h"
#ifdef USE_VULKAN
#include "vk.h"
#endif


/* ===========================================================================
 * FOG SYSTEM
 * ===========================================================================
 */

static qboolean R_AdvancedFogRuntimeEnabled( void ) {
	return (qboolean)( r_useGlFog && r_useGlFog->integer );
}


static fogType_t R_ResolveAdvancedFogType( const fogParms_t *parms ) {
	if ( !parms ) {
		return FT_NONE;
	}
	return (fogType_t)wired_fog_runtime_resolve_volume_type(
		R_AdvancedFogRuntimeEnabled(), parms->type,
		r_defaultFogParmsType ? r_defaultFogParmsType->integer : -1 );
}


static fogType_t R_ResolveGlobalFogType( void ) {
	return (fogType_t)wired_fog_runtime_resolve_global_type(
		R_AdvancedFogRuntimeEnabled(), tr.globalFogType );
}

void RE_GetGlobalFog( refFogType_t *type, vec3_t color, float *depthForOpaque, float *density ) {
	if ( type ) {
		*type = (refFogType_t)R_ResolveGlobalFogType();
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
	if ( R_ResolveGlobalFogType() == FT_NONE ) {
		return qfalse;
	}
	return (qboolean)( fogNum == tr.globalFog );
}


int R_BoundsFogNum( const vec3_t mins, const vec3_t maxs ) {
	const fog_t *fog;

	if ( !tr.world || tr.world->numfogs <= 1 ) {
		return ( R_ResolveGlobalFogType() != FT_NONE ) ? tr.globalFog : 0;
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

	return ( R_ResolveGlobalFogType() != FT_NONE ) ? tr.globalFog : 0;
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
			*type = (refFogType_t)R_ResolveAdvancedFogType( parms );
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
			*type = (refFogType_t)R_ResolveGlobalFogType();
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
		vk_update_fog_uniform( zeroColor, FT_NONE, 0.0f, 0.0f, qfalse );
	}
#endif
}


void RB_FogOn( void ) {
	if ( !R_AdvancedFogRuntimeEnabled() || tr.fogTypeCurrent == FT_NONE ) {
		return;
	}
	tr.fogEnabled = qtrue;
#ifdef USE_VULKAN
	if ( vk.active && vk.cmd ) {
		vec4_t color;
		VectorCopy( tr.globalFogColor, color );
		color[3] = 1.0f;
		vk_update_fog_uniform( color, (int)tr.fogTypeCurrent,
			tr.globalFogDensity, tr.globalFogDepthForOpaque, qtrue );
	}
#endif
}


/*
====================
RB_Fog (Vulkan)

Publish fog parameters through the per-draw set-0 UBO
(see vk_update_fog_uniform). The existing texcoord-based fog pipeline is
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

	if ( !R_AdvancedFogRuntimeEnabled() ) {
		R_FogOff();
		return;
	}

	if ( fogNum <= 0 ) {
		type = R_ResolveGlobalFogType();
		if ( type == FT_NONE ) {
			R_FogOff();
			return;
		}
		VectorCopy( tr.globalFogColor, color );
		depthForOpaque = tr.globalFogDepthForOpaque;
		density = tr.globalFogDensity;
		farClip = depthForOpaque;
	} else if ( tr.world && fogNum < tr.world->numfogs ) {
		const fogParms_t *parms = &tr.world->fogs[fogNum].parms;
		type = R_ResolveAdvancedFogType( parms );
		if ( type == FT_NONE ) {
			R_FogOff();
			return;
		}
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
		vk_update_fog_uniform( color, (int)type, density, farClip, qtrue );
	}
#endif

	(void)depthForOpaque;
}

/* ===========================================================================
 * HALOS
 * ===========================================================================
 */

#if FEAT_HALO

static int r_numhalos;
static int r_firstSceneHalo;


void R_ClearHalos( void ) {
	r_numhalos = 0;
	r_firstSceneHalo = 0;
}


void RE_AddHaloToScene( const vec3_t org, float r, float g, float b,
	float scale, int id, qboolean visible )
{
	halo_t *cor;

	if ( !tr.registered ) {
		return;
	}

	if ( r_numhalos >= MAX_HALOS ) {
		return;
	}

	cor = &backEndData->halos[ r_numhalos + r_firstSceneHalo ];
	VectorCopy( org, cor->origin );
	cor->color[0] = r;
	cor->color[1] = g;
	cor->color[2] = b;
	cor->scale = scale;
	cor->id = id;
	cor->visible = visible;
	cor->shader = NULL;
	r_numhalos++;
}


void RB_AddHaloFlares( void ) {
	halo_t *cor;

	// halos are their own primitive; either the surface-flare master (r_flares)
	// or the dedicated halo toggle (r_halos) is enough to draw them.
	if ( ( !r_flares || !r_flares->integer ) && ( !r_halos || !r_halos->integer ) ) {
		return;
	}
	if ( !backEndData ) {
		return;
	}

	cor = backEndData->halos + r_firstSceneHalo;
	for ( int i = 0; i < r_numhalos; i++, cor++ ) {
		vec3_t scaledColor;

		if ( !cor->visible ) {
			continue;
		}

		VectorScale( cor->color, cor->scale, scaledColor );

		int fogNum = 0;
		if ( tr.world && tr.world->numfogs > 1 ) {
			for ( int j = 1; j < tr.world->numfogs; j++ ) {
				const fog_t *fv = &tr.world->fogs[j];
				int k;
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

#endif // FEAT_HALO


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
		float corners[4][2] = {
			{ region[0], region[1] }, { region[0] + region[2], region[1] },
			{ region[0] + region[2], region[1] + region[3] },
			{ region[0], region[1] + region[3] }
		};
		float minX, minY, maxX, maxY;
		for ( int corner = 0; corner < 4; corner++ )
			RE_TransformUiPoint( &corners[corner][0], &corners[corner][1] );
		minX = maxX = corners[0][0]; minY = maxY = corners[0][1];
		for ( int corner = 1; corner < 4; corner++ ) {
			if ( corners[corner][0] < minX ) minX = corners[corner][0];
			if ( corners[corner][0] > maxX ) maxX = corners[corner][0];
			if ( corners[corner][1] < minY ) minY = corners[corner][1];
			if ( corners[corner][1] > maxY ) maxY = corners[corner][1];
		}
		cmd->hasRegion = qtrue;
		cmd->x = minX; cmd->y = minY;
		cmd->w = maxX - minX; cmd->h = maxY - minY;
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
	{
		float centerX = x + w * 0.5f, centerY = y + h * 0.5f;
		float radians = angle * ( (float)M_PI / 180.0f );
		float cosine = cosf( radians ), sine = sinf( radians );
		static const float signs[4][2] = {
			{ -1.0f, -1.0f }, { 1.0f, -1.0f },
			{ 1.0f, 1.0f }, { -1.0f, 1.0f }
		};
		for ( int corner = 0; corner < 4; corner++ ) {
			float localX = signs[corner][0] * w * 0.5f;
			float localY = signs[corner][1] * h * 0.5f;
			cmd->positions[corner][0] = centerX + localX * cosine - localY * sine;
			cmd->positions[corner][1] = centerY + localX * sine + localY * cosine;
			RE_TransformUiPoint( &cmd->positions[corner][0], &cmd->positions[corner][1] );
		}
	}
}
