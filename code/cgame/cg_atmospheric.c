/*
===========================================================================
cg_atmospheric.c -- Rain & snow atmospheric effects (3B / FEAT_ATMOSPHERIC)

Simplified port from Spearmint/mint-arena. Uses tracemap for sky/ground
height detection. Weather is server-controlled via g_weather cvar.

g_weather "rain"  — rain particles
g_weather "snow"  — snow particles
g_weather ""      — disabled (default)

Players can callvote to change weather mid-game.
Client can locally disable with cg_atmosphericEffects 0.

Based on Spearmint Source Code (GPL v3).
===========================================================================
*/
#include "cg_local.h"

#if FEAT_ATMOSPHERIC

#define MAX_ATM_PARTICLES   8000
#define ATM_DISTANCE        1500
#define ATM_RAIN_SPEED      ( 1.1f * DEFAULT_GRAVITY )
#define ATM_SNOW_SPEED      ( 0.2f * DEFAULT_GRAVITY )
#define ATM_RAIN_HEIGHT     150
#define ATM_SNOW_HEIGHT     8

typedef enum {
	ATM_NONE,
	ATM_RAIN,
	ATM_SNOW
} atmType_t;

typedef enum {
	PART_INACTIVE,
	PART_FALLING
} partActive_t;

typedef struct {
	vec3_t		pos;
	vec3_t		vel;
	float		height;
	partActive_t active;
	int			startTime;
	float		weight;
} atmParticle_t;

static struct {
	atmParticle_t	particles[MAX_ATM_PARTICLES];
	atmType_t		type;
	int				numActive;
	qhandle_t		shader;
	qboolean		tracemapGenerated;
} atm;

/*
==================
CG_AtmosphericInit

Idempotent — called from CG_ParseServerinfo whenever serverinfo updates.
Reads cgs.weather (set by server's g_weather cvar).
Generates tracemap on first call, reinitializes particles on weather change.
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

	// clear particles for weather change
	memset( atm.particles, 0, sizeof( atm.particles ) );
	atm.numActive = 0;
	atm.type = newType;

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
CG_GenerateParticle

Spawns a new particle at a random position around the viewer.
Uses tracemap for O(1) sky height lookup instead of per-particle traces.
==================
*/
static void CG_GenerateParticle( atmParticle_t *p ) {
	float	dist, skyHeight, groundHeight;

	// random position around player within ATM_DISTANCE
	dist = ATM_DISTANCE * 0.5f;
	p->pos[0] = cg.refdef.vieworg[0] + crandom() * dist;
	p->pos[1] = cg.refdef.vieworg[1] + crandom() * dist;

	// use tracemap for sky and ground height
	skyHeight = BG_GetSkyHeightAtPoint( p->pos );
	if ( skyHeight >= ( 128 * 1024 ) ) {
		// no sky above this position (indoors)
		p->active = PART_INACTIVE;
		return;
	}

	groundHeight = BG_GetGroundHeightAtPoint( p->pos );
	if ( groundHeight <= ( -128 * 1024 ) ) {
		// no ground below (void) — skip to avoid particles in empty space
		p->active = PART_INACTIVE;
		return;
	}

	// spawn at random height between sky and ground for natural distribution
	p->pos[2] = groundHeight + random() * ( skyHeight - groundHeight );

	if ( atm.type == ATM_RAIN ) {
		p->vel[0] = crandom() * 30;
		p->vel[1] = crandom() * 30;
		p->vel[2] = -ATM_RAIN_SPEED;
		p->height = ATM_RAIN_HEIGHT;
	} else {
		p->vel[0] = crandom() * 20;
		p->vel[1] = crandom() * 20;
		p->vel[2] = -ATM_SNOW_SPEED;
		p->height = ATM_SNOW_HEIGHT;
		p->weight = 0.3f + random() * 0.5f;
	}

	p->active = PART_FALLING;
	p->startTime = cg.time;
}

/*
==================
CG_RenderParticle

Renders a single rain/snow particle as a poly triangle.
==================
*/
static void CG_RenderParticle( atmParticle_t *p ) {
	polyVert_t	verts[3];
	vec3_t		forward, right;
	float		size;

	if ( atm.type == ATM_RAIN ) {
		size = 1.0f;
		// rain streak: elongated vertical triangle
		VectorSet( forward, 0, 0, -p->height * 0.02f );
		VectorSet( right, size, 0, 0 );
	} else {
		size = p->height * p->weight;
		VectorSet( forward, 0, 0, size );
		VectorSet( right, size, 0, 0 );
	}

	VectorCopy( p->pos, verts[0].xyz );
	VectorAdd( p->pos, forward, verts[1].xyz );
	VectorAdd( p->pos, right, verts[2].xyz );

	verts[0].st[0] = 0; verts[0].st[1] = 0;
	verts[1].st[0] = 1; verts[1].st[1] = 0;
	verts[2].st[0] = 0; verts[2].st[1] = 1;

	if ( atm.type == ATM_RAIN ) {
		verts[0].modulate.rgba[0] = verts[0].modulate.rgba[1] = verts[0].modulate.rgba[2] = 128;
		verts[1].modulate.rgba[0] = verts[1].modulate.rgba[1] = verts[1].modulate.rgba[2] = 128;
		verts[2].modulate.rgba[0] = verts[2].modulate.rgba[1] = verts[2].modulate.rgba[2] = 128;
	} else {
		verts[0].modulate.rgba[0] = verts[0].modulate.rgba[1] = verts[0].modulate.rgba[2] = 255;
		verts[1].modulate.rgba[0] = verts[1].modulate.rgba[1] = verts[1].modulate.rgba[2] = 255;
		verts[2].modulate.rgba[0] = verts[2].modulate.rgba[1] = verts[2].modulate.rgba[2] = 255;
	}
	verts[0].modulate.rgba[3] = verts[1].modulate.rgba[3] = verts[2].modulate.rgba[3] = 200;

	trap_R_AddPolyToScene( atm.shader, 3, verts );
}

/*
==================
CG_AddAtmosphericEffects

Called once per frame from cg_view.c to add rain/snow particles.
==================
*/
void CG_AddAtmosphericEffects( void ) {
	int		i, max;
	float	deltaTime;
	vec3_t	groundEnd;
	trace_t	tr;

	if ( atm.type == ATM_NONE ) {
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

	deltaTime = ( cg.frametime ) * 0.001f;

	// spawn new particles (up to 100 per frame)
	max = 100;
	for ( i = 0; i < MAX_ATM_PARTICLES && max > 0; i++ ) {
		if ( atm.particles[i].active == PART_INACTIVE ) {
			CG_GenerateParticle( &atm.particles[i] );
			if ( atm.particles[i].active == PART_FALLING ) {
				max--;
			}
		}
	}

	// update and render active particles
	atm.numActive = 0;
	for ( i = 0; i < MAX_ATM_PARTICLES; i++ ) {
		atmParticle_t *p = &atm.particles[i];
		if ( p->active != PART_FALLING ) continue;

		// move
		VectorMA( p->pos, deltaTime, p->vel, p->pos );

		// snow: add slight wind drift
		if ( atm.type == ATM_SNOW ) {
			p->pos[0] += sin( cg.time * 0.001f + i ) * 0.5f;
			p->pos[1] += cos( cg.time * 0.0013f + i ) * 0.5f;
		}

		// check ground collision (short trace — cheap and precise; ignore sky surfaces)
		VectorCopy( p->pos, groundEnd );
		groundEnd[2] -= 4;
		CG_Trace( &tr, p->pos, NULL, NULL, groundEnd, -1, CONTENTS_SOLID | CONTENTS_WATER );
		if ( tr.fraction < 1.0f && !( tr.surfaceFlags & SURF_SKY ) ) {
			p->active = PART_INACTIVE;
			continue;
		}

		// cull if too far from viewer (XY distance only — vertical doesn't matter for falling particles)
		{
			float dx = p->pos[0] - cg.refdef.vieworg[0];
			float dy = p->pos[1] - cg.refdef.vieworg[1];
			if ( dx * dx + dy * dy > ATM_DISTANCE * ATM_DISTANCE ) {
				p->active = PART_INACTIVE;
				continue;
			}
		}

		// kill if below viewer by too much
		if ( p->pos[2] < cg.refdef.vieworg[2] - 500 ) {
			p->active = PART_INACTIVE;
			continue;
		}

		CG_RenderParticle( p );
		atm.numActive++;
	}
}

#endif // FEAT_ATMOSPHERIC
