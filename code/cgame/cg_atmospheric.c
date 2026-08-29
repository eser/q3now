// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2010-2019 Zack Middleton & Spearmint contributors
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cg_atmospheric.c -- app-authored unified atmosphere (FEAT_ATMOSPHERIC)

Environment state is server-controlled via g_envWeather.  The cgame resolves
content presets into one semantic AtmosphereFrameState and contextual emitter
stream; the renderer remains unaware of game rules and owns all per-particle,
froxel, surface-climate and composition work.  The small immutable state is
published each frame so its authoritative timeline advances, while the large
collision heightgrid is uploaded only when the preset/world changes.

Supported authored presets are rain, snow, sleet, hail, dust/ash, cold, fog,
storm and clean/empty.  Visual particles are deterministic derivatives of the
state timeline + emitter seeds; they are not network entities.

Players can callvote to change weather mid-game.
Client can locally disable with cg_atmosphericEffects 0.

Based on Spearmint Source Code (GPL v3).
===========================================================================
*/
#include "cg_local.h"
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/traps.h"

#if FEAT_ATMOSPHERIC

#define ATM_DISTANCE 1500

typedef enum {
	ATM_NONE	 = ATMOSPHERE_PRECIP_NONE,
	ATM_RAIN	 = ATMOSPHERE_PRECIP_RAIN,
	ATM_SNOW	 = ATMOSPHERE_PRECIP_SNOW,
	ATM_SLEET	 = ATMOSPHERE_PRECIP_SLEET,
	ATM_HAIL	 = ATMOSPHERE_PRECIP_HAIL,
	ATM_DUST_ASH = ATMOSPHERE_PRECIP_DUST_ASH,
	ATM_COLD,
	ATM_FOG,
	ATM_STORM
} atmPreset_t;

static struct {
	atmPreset_t preset;
	qhandle_t	shader;
	qboolean	tracemapGenerated;
	qboolean	heightgridEmitted;
	qboolean	offEmitted;
} atm;

/*
==================
CG_AtmosphericInit

Idempotent — called from CG_ParseServerinfo whenever serverinfo updates.
Reads cgs.weather (set by server's g_envWeather cvar). Marks the weather
descriptor stale on a type change so the next frame re-emits it to the pool.
==================
*/
void CG_AtmosphericInit( void )
{
	atmPreset_t newPreset;

	// determine weather type from server setting
	if ( !Q_stricmp( cgs.weather, "rain" ) ) {
		newPreset = ATM_RAIN;
	} else if ( !Q_stricmp( cgs.weather, "snow" ) ) {
		newPreset = ATM_SNOW;
	} else if ( !Q_stricmp( cgs.weather, "sleet" ) ) {
		newPreset = ATM_SLEET;
	} else if ( !Q_stricmp( cgs.weather, "hail" ) ) {
		newPreset = ATM_HAIL;
	} else if ( !Q_stricmp( cgs.weather, "dust" ) || !Q_stricmp( cgs.weather, "ash" ) ) {
		newPreset = ATM_DUST_ASH;
	} else if ( !Q_stricmp( cgs.weather, "cold" ) ) {
		newPreset = ATM_COLD;
	} else if ( !Q_stricmp( cgs.weather, "fog" ) ) {
		newPreset = ATM_FOG;
	} else if ( !Q_stricmp( cgs.weather, "storm" ) ) {
		newPreset = ATM_STORM;
	} else {
		newPreset = ATM_NONE;
	}

	// no change — skip reinit
	if ( newPreset == atm.preset && ( newPreset == ATM_NONE || atm.shader ) ) {
		return;
	}

	atm.preset			  = newPreset;
	atm.heightgridEmitted = qfalse;
	atm.offEmitted		  = qfalse;

	if ( newPreset == ATM_RAIN || newPreset == ATM_SLEET || newPreset == ATM_HAIL || newPreset == ATM_STORM ) {
		atm.shader = trap_R_RegisterShader( "gfx/misc/raindrop" );
		if ( !atm.shader ) {
			atm.shader = trap_R_RegisterShader( "white" );
		}
	} else if ( newPreset == ATM_SNOW ) {
		atm.shader = trap_R_RegisterShader( "gfx/misc/snow" );
		if ( !atm.shader ) {
			atm.shader = trap_R_RegisterShader( "white" );
		}
	} else if ( newPreset != ATM_NONE ) {
		atm.shader = trap_R_RegisterShader( "*white" );
	} else {
		atm.shader = 0;
	}
}

static qboolean CG_AtmosphericBuildState( atmosphericDesc_t *desc, const vec2_t mins2, const vec2_t maxs2,
										  int gridSize )
{
	if ( !desc || atm.preset == ATM_NONE )
		return qfalse;
	memset( desc, 0, sizeof( *desc ) );
	desc->schemaVersion		= WIRED_ATMOSPHERE_SCHEMA_VERSION;
	desc->flags				= ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SKY_LIGHTING;
	desc->qualityTier		= ATMOSPHERE_QUALITY_WEATHER;
	desc->seed				= 216u + (uint32_t)atm.preset;
	desc->timelineSeconds	= (float)cg.time * 0.001f;
	desc->transitionSeconds = 2.0f;
	desc->temperatureC		= 12.0f;
	desc->humidity			= 0.75f;
	desc->indoorExposure	= 1.0f;
	desc->distance			= ATM_DISTANCE;
	desc->visibility		= ATM_DISTANCE;
	desc->ambientColor[0]	= 0.16f;
	desc->ambientColor[1]	= 0.18f;
	desc->ambientColor[2]	= 0.22f;
	desc->sunDirection[0]	= 0.30f;
	desc->sunDirection[1]	= 0.20f;
	desc->sunDirection[2]	= 0.93f;
	desc->sunIntensity		= 0.9f;
	desc->moonDirection[0]	= -0.30f;
	desc->moonDirection[1]	= -0.20f;
	desc->moonDirection[2]	= 0.93f;
	desc->moonIntensity		= 0.08f;
	desc->bounds[0]			= mins2[0];
	desc->bounds[1]			= mins2[1];
	desc->bounds[2]			= -( 32 * 1024 );
	desc->bounds[3]			= maxs2[0];
	desc->bounds[4]			= maxs2[1];
	desc->bounds[5]			= 32 * 1024;
	desc->worldMins[0]		= mins2[0];
	desc->worldMins[1]		= mins2[1];
	desc->worldMaxs[0]		= maxs2[0];
	desc->worldMaxs[1]		= maxs2[1];
	desc->gridSize			= gridSize;

	switch ( atm.preset ) {
	case ATM_RAIN:
		desc->type			   = ATMOSPHERE_PRECIP_RAIN;
		desc->precipitation[0] = 1.0f;
		desc->surfaceWetness   = 1.0f;
		desc->cloudCover	   = 0.75f;
		desc->cloudShadow	   = 0.45f;
		break;
	case ATM_SNOW:
		desc->type			   = ATMOSPHERE_PRECIP_SNOW;
		desc->temperatureC	   = -7.0f;
		desc->humidity		   = 0.9f;
		desc->precipitation[1] = 1.0f;
		desc->surfaceFrost	   = 0.75f;
		desc->snowAccumulation = 1.0f;
		desc->cloudCover	   = 0.82f;
		desc->cloudShadow	   = 0.55f;
		break;
	case ATM_SLEET:
		desc->type			   = ATMOSPHERE_PRECIP_SLEET;
		desc->temperatureC	   = 0.0f;
		desc->humidity		   = 0.95f;
		desc->precipitation[0] = 0.25f;
		desc->precipitation[2] = 0.75f;
		desc->surfaceWetness   = 0.85f;
		desc->surfaceFrost	   = 0.35f;
		desc->cloudCover	   = 0.9f;
		desc->cloudShadow	   = 0.65f;
		break;
	case ATM_HAIL:
		desc->type			   = ATMOSPHERE_PRECIP_HAIL;
		desc->temperatureC	   = 2.0f;
		desc->humidity		   = 0.9f;
		desc->precipitation[0] = 0.2f;
		desc->precipitation[3] = 0.8f;
		desc->surfaceWetness   = 0.8f;
		desc->wind[0]		   = 70.0f;
		desc->wind[1]		   = 25.0f;
		desc->gustStrength	   = 45.0f;
		desc->cloudCover	   = 0.95f;
		desc->cloudShadow	   = 0.75f;
		break;
	case ATM_DUST_ASH:
		desc->type		  = ATMOSPHERE_PRECIP_DUST_ASH;
		desc->qualityTier = ATMOSPHERE_QUALITY_FULL;
		desc->flags |= ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		desc->temperatureC		 = 24.0f;
		desc->humidity			 = 0.15f;
		desc->precipitation[4]	 = 1.0f;
		desc->visibility		 = 1050.0f;
		desc->mediaDensity		 = 0.0009f;
		desc->mediaHeightFalloff = 0.0005f;
		desc->wind[0]			 = 95.0f;
		desc->wind[1]			 = 35.0f;
		desc->gustStrength		 = 70.0f;
		desc->cloudCover		 = 0.55f;
		desc->cloudShadow		 = 0.5f;
		break;
	case ATM_COLD:
		desc->temperatureC = -12.0f;
		desc->humidity	   = 0.8f;
		desc->surfaceFrost = 0.9f;
		desc->visibility   = 2600.0f;
		desc->cloudCover   = 0.35f;
		desc->cloudShadow  = 0.2f;
		break;
	case ATM_FOG:
		desc->qualityTier = ATMOSPHERE_QUALITY_FULL;
		desc->flags |= ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		desc->temperatureC		 = 6.0f;
		desc->humidity			 = 0.98f;
		desc->visibility		 = 850.0f;
		desc->mediaDensity		 = 0.0015f;
		desc->mediaHeightFalloff = 0.0012f;
		desc->surfaceWetness	 = 0.35f;
		desc->cloudCover		 = 0.7f;
		desc->cloudShadow		 = 0.45f;
		break;
	case ATM_STORM:
		desc->type		  = ATMOSPHERE_PRECIP_RAIN;
		desc->qualityTier = ATMOSPHERE_QUALITY_FULL;
		desc->flags |= ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		desc->temperatureC		 = 5.0f;
		desc->humidity			 = 1.0f;
		desc->precipitation[0]	 = 0.65f;
		desc->precipitation[2]	 = 0.15f;
		desc->precipitation[3]	 = 0.20f;
		desc->surfaceWetness	 = 1.0f;
		desc->visibility		 = 1200.0f;
		desc->mediaDensity		 = 0.0007f;
		desc->mediaHeightFalloff = 0.0007f;
		desc->wind[0]			 = 140.0f;
		desc->wind[1]			 = 55.0f;
		desc->gustStrength		 = 95.0f;
		desc->cloudCover		 = 1.0f;
		desc->cloudShadow		 = 0.85f;
		desc->lightning			 = 0.35f;
		break;
	default:
		return qfalse;
	}

	if ( atm.preset == ATM_RAIN || atm.preset == ATM_SNOW || atm.preset == ATM_SLEET || atm.preset == ATM_HAIL ||
		 atm.preset == ATM_STORM )
		desc->flags |= ATMOSPHERE_FLAG_HEIGHTGRID;
	if ( atm.preset != ATM_DUST_ASH && atm.preset != ATM_FOG )
		desc->flags |= ATMOSPHERE_FLAG_SURFACE_CLIMATE;
	return qtrue;
}

static void CG_AtmosphericEmitContextual( const atmosphericDesc_t *atmosphere )
{
	atmosphereEmitter_t emitter;
	if ( !atmosphere )
		return;
	if ( atmosphere->temperatureC <= 5.0f && cgs.media.atmosphereBreathClass > 0 ) {
		memset( &emitter, 0, sizeof( emitter ) );
		emitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
		emitter.kind		  = ATMOSPHERE_EMITTER_BREATH;
		emitter.id			  = 0x10000u + (uint32_t)cg.clientNum;
		emitter.seed		  = atmosphere->seed ^ emitter.id;
		emitter.profile		  = CG_ATMOSPHERE_PROFILE_BREATH;
		VectorMA( cg.refdef.vieworg, 8.0f, cg.refdef.viewaxis[0], emitter.origin );
		VectorCopy( cg.refdef.viewaxis[0], emitter.direction );
		emitter.intensity	 = 1.0f;
		emitter.radius		 = 12.0f;
		emitter.temperatureC = 37.0f;
		emitter.humidity	 = 1.0f;
		trap_R_AddAtmosphereEmitter( &emitter );
	}
	if ( ( atm.preset == ATM_FOG || atm.preset == ATM_COLD || atm.preset == ATM_SNOW ) &&
		 cgs.media.atmosphereMistClass > 0 ) {
		memset( &emitter, 0, sizeof( emitter ) );
		emitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
		emitter.kind		  = ATMOSPHERE_EMITTER_GROUND_MIST;
		emitter.id			  = 0x20000u + (uint32_t)cg.clientNum;
		emitter.seed		  = atmosphere->seed ^ emitter.id;
		emitter.profile		  = CG_ATMOSPHERE_PROFILE_MIST;
		VectorCopy( cg.refdef.vieworg, emitter.origin );
		emitter.origin[2] -= 36.0f;
		emitter.direction[2] = 1.0f;
		emitter.intensity	 = 0.7f;
		emitter.radius		 = 128.0f;
		emitter.temperatureC = atmosphere->temperatureC;
		emitter.humidity	 = atmosphere->humidity;
		trap_R_AddAtmosphereEmitter( &emitter );
	}
	if ( ( atm.preset == ATM_STORM || atm.preset == ATM_DUST_ASH ) && cgs.media.atmosphereDebrisClass > 0 ) {
		memset( &emitter, 0, sizeof( emitter ) );
		emitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
		emitter.kind		  = ATMOSPHERE_EMITTER_DEBRIS;
		emitter.id			  = 0x30000u + (uint32_t)cg.clientNum;
		emitter.seed		  = atmosphere->seed ^ emitter.id;
		emitter.profile		  = CG_ATMOSPHERE_PROFILE_DEBRIS;
		VectorCopy( cg.refdef.vieworg, emitter.origin );
		VectorCopy( atmosphere->wind, emitter.direction );
		if ( VectorNormalize( emitter.direction ) == 0.0f )
			emitter.direction[0] = 1.0f;
		emitter.intensity	 = 1.0f;
		emitter.radius		 = 180.0f;
		emitter.temperatureC = atmosphere->temperatureC;
		emitter.humidity	 = atmosphere->humidity;
		trap_R_AddAtmosphereEmitter( &emitter );
	}
}

/*
==================
CG_AddAtmosphericEffects

Publishes one immutable semantic state per frame and contextual camera-near
emitters.  The heightgrid remains an infrequent world/preset upload; all dense
simulation, collision, effect-graph expansion and drawing remains GPU-owned.
==================
*/
void CG_AddAtmosphericEffects( void )
{
	if ( atm.preset == ATM_NONE || !cg_atmosphericEffects.integer ) {
		if ( !atm.offEmitted ) {
			atmosphericDesc_t desc;
			memset( &desc, 0, sizeof( desc ) );
			desc.schemaVersion	 = WIRED_ATMOSPHERE_SCHEMA_VERSION;
			desc.qualityTier	 = ATMOSPHERE_QUALITY_OFF;
			desc.timelineSeconds = (float)cg.time * 0.001f;
			desc.temperatureC	 = 15.0f;
			desc.humidity		 = 0.5f;
			desc.indoorExposure	 = 1.0f;
			trap_R_SetAtmosphere( &desc );
			atm.offEmitted = qtrue;
		}
		return;
	}
	atm.offEmitted = qfalse;

	// generate tracemap on first frame (deferred — collision map must be loaded first)
	if ( !atm.tracemapGenerated ) {
		vec3_t	  mins, maxs;
		qhandle_t worldModel = trap_R_RegisterModel( "*0" );
		trap_R_ModelBounds( worldModel, mins, maxs );
		if ( mins[0] >= maxs[0] || mins[1] >= maxs[1] ) {
			atm.tracemapGenerated = qtrue;
			return;
		}
		BG_GenerateTracemap( mins, maxs, CG_Trace );
		atm.tracemapGenerated = qtrue;
	}

	{
		atmosphericDesc_t desc;
		const float		 *ground;
		vec2_t			  mins2, maxs2;
		int				  gridSize;
		ground = BG_GetTracemapGround( mins2, maxs2, &gridSize );
		if ( !ground || !CG_AtmosphericBuildState( &desc, mins2, maxs2, gridSize ) )
			return;
		trap_R_SetAtmosphere( &desc );
		if ( ( desc.flags & ATMOSPHERE_FLAG_HEIGHTGRID ) && !atm.heightgridEmitted ) {
			trap_R_SetAtmosphereHeightgrid( ground, gridSize * gridSize );
			atm.heightgridEmitted = qtrue;
		}
		CG_AtmosphericEmitContextual( &desc );
	}
}

#endif // FEAT_ATMOSPHERIC
