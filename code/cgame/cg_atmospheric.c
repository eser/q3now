// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2010-2019 Zack Middleton & Spearmint contributors
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cg_atmospheric.c -- Rain & snow atmospheric effects (3B / FEAT_ATMOSPHERIC)

Weather is server-controlled via the g_envWeather cvar; the client resolves
it to a GPU-resident atmospheric pool that self-spawns, integrates, collides
against the world heightgrid, and draws every frame. cgame's job is to emit
the weather descriptor + collision heightgrid ONCE per weather change (after
the tracemap is built) — the renderer owns the per-frame lifecycle.

g_envWeather "rain"  — rain particles
g_envWeather "snow"  — snow particles
g_envWeather ""      — disabled (default)

Players can callvote to change weather mid-game.
Client can locally disable with cg_atmosphericEffects 0.

Based on Spearmint Source Code (GPL v3).
===========================================================================
*/
#include "cg_local.h"
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/traps.h"

#if FEAT_ATMOSPHERIC

#define ATM_DISTANCE        1500

typedef enum {
	ATM_NONE,
	ATM_RAIN,
	ATM_SNOW
} atmType_t;

static struct {
	atmType_t		type;
	qhandle_t		shader;
	qboolean		tracemapGenerated;
	qboolean		gpuEmitted;        // GPU descriptor + heightgrid pushed for the current weather
} atm;

/*
==================
CG_AtmosphericInit

Idempotent — called from CG_ParseServerinfo whenever serverinfo updates.
Reads cgs.weather (set by server's g_envWeather cvar). Marks the weather
descriptor stale on a type change so the next frame re-emits it to the pool.
==================
*/
void CG_AtmosphericInit( void ) {
	atmType_t newType;

	// determine weather type from server setting
	if ( !Q_stricmp( cgs.weather, "rain" ) ) {
		newType = ATM_RAIN;
	} else if ( !Q_stricmp( cgs.weather, "snow" ) ) {
		newType = ATM_SNOW;
	} else {
		newType = ATM_NONE;
	}

	// no change — skip reinit
	if ( newType == atm.type && ( newType == ATM_NONE || atm.shader ) ) {
		return;
	}

	atm.type = newType;
	// Re-push the GPU weather descriptor on the next frame the tracemap is ready
	// (the new type / spawn volume must reach the renderer pool).
	atm.gpuEmitted = qfalse;

	if ( newType == ATM_RAIN ) {
		atm.shader = trap_R_RegisterShader( "gfx/misc/raindrop" );
		if ( !atm.shader ) {
			atm.shader = trap_R_RegisterShader( "white" );
		}
	} else if ( newType == ATM_SNOW ) {
		atm.shader = trap_R_RegisterShader( "gfx/misc/snow" );
		if ( !atm.shader ) {
			atm.shader = trap_R_RegisterShader( "white" );
		}
	} else {
		atm.shader = 0;
	}
}

/*
==================
CG_AtmosphericEmitGPU

Push the current weather descriptor + collision heightgrid to the renderer's
GPU-resident atmospheric pool. Called ONCE per weather change (the GPU pool
then self-spawns / integrates / collides / draws every frame). The spawn
volume is the world xy bounds plus a generous z range; the GPU distance-culls
around the eye each frame, so emit-once is sufficient. Requires the tracemap.
==================
*/
static void CG_AtmosphericEmitGPU( void ) {
	atmosphericDesc_t	desc;
	const float			*ground;
	vec2_t				mins2, maxs2;
	int					gridSize;

	ground = BG_GetTracemapGround( mins2, maxs2, &gridSize );
	if ( ground == NULL ) {
		return;   // tracemap not ready yet — retry next frame
	}

	memset( &desc, 0, sizeof( desc ) );
	desc.type = (int)atm.type;   // ATM_NONE/RAIN/SNOW order matches the renderer

	// Spawn volume = world xy bounds, z from the lowest ground up through a
	// generous sky margin. The GPU distance-culls to the eye, so a world-wide
	// volume is fine for an emit-once descriptor.
	desc.bounds[0] = mins2[0];
	desc.bounds[1] = mins2[1];
	desc.bounds[2] = -( 32 * 1024 );          // floor (clamped against the heightgrid on the GPU)
	desc.bounds[3] = maxs2[0];
	desc.bounds[4] = maxs2[1];
	desc.bounds[5] =  ( 32 * 1024 );          // sky margin

	desc.distance     = ATM_DISTANCE;
	desc.worldMins[0] = mins2[0];
	desc.worldMins[1] = mins2[1];
	desc.worldMaxs[0] = maxs2[0];
	desc.worldMaxs[1] = maxs2[1];
	desc.gridSize     = gridSize;

	trap_R_SetAtmosphere( &desc );
	// Heightgrid ships separately (a struct-nested pointer can't cross the VM
	// boundary). gridSize² floats, row-major ground[y][x].
	trap_R_SetAtmosphereHeightgrid( ground, gridSize * gridSize );

	atm.gpuEmitted = qtrue;
}

/*
==================
CG_AddAtmosphericEffects

Called once per frame from cg_view.c. Builds the collision tracemap on the
first frame (deferred — the collision model must be loaded first), then emits
the weather descriptor + heightgrid to the GPU pool once per weather change.
The renderer's GPU-resident pool owns spawn/integrate/collide/draw thereafter.
==================
*/
void CG_AddAtmosphericEffects( void ) {
	if ( atm.type == ATM_NONE ) {
		// Weather just turned off — tell the pool to go inert (type 0) so it
		// stops drawing.
		if ( atm.tracemapGenerated && !atm.gpuEmitted ) {
			atmosphericDesc_t desc;
			memset( &desc, 0, sizeof( desc ) );
			desc.type = 0;
			trap_R_SetAtmosphere( &desc );
			atm.gpuEmitted = qtrue;
		}
		return;
	}

	if ( !cg_atmosphericEffects.integer ) {
		return;
	}

	// generate tracemap on first frame (deferred — collision map must be loaded first)
	if ( !atm.tracemapGenerated ) {
		vec3_t mins, maxs;
		qhandle_t worldModel = trap_R_RegisterModel( "*0" );
		trap_R_ModelBounds( worldModel, mins, maxs );
		if ( mins[0] >= maxs[0] || mins[1] >= maxs[1] ) {
			atm.tracemapGenerated = qtrue;
			return;
		}
		BG_GenerateTracemap( mins, maxs, CG_Trace );
		atm.tracemapGenerated = qtrue;
	}

	// Emit the weather descriptor + heightgrid once, then let the renderer pool
	// own everything (spawn / integrate / collide / draw).
	if ( !atm.gpuEmitted ) {
		CG_AtmosphericEmitGPU();
	}
}

#endif // FEAT_ATMOSPHERIC
