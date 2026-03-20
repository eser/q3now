/*
===========================================================================
bg_tracemap.c -- Height map system (3A / FEAT_ATMOSPHERIC)

Simplified port from Spearmint/mint-arena. Provides sky/ground height
lookups for atmospheric effects and other position-aware features.

Shared between game and cgame modules.

Based on Spearmint Source Code (GPL v3).
===========================================================================
*/
#include "../qcommon/q_shared.h"
#include "bg_public.h"

#if FEAT_ATMOSPHERIC

#define TRACEMAP_SIZE       256
#define MAX_WORLD_HEIGHT    ( 128 * 1024 )
#define MIN_WORLD_HEIGHT    ( -128 * 1024 )

typedef struct {
	qboolean	loaded;
	float		sky[TRACEMAP_SIZE][TRACEMAP_SIZE];
	float		ground[TRACEMAP_SIZE][TRACEMAP_SIZE];
	vec2_t		world_mins;
	vec2_t		world_maxs;
	float		one_over_mapgrid_factor[2];
} tracemap_t;

static tracemap_t tracemap;

/*
==================
BG_ClampPointToTracemapExtends
==================
*/
static void BG_ClampPointToTracemapExtends( vec3_t point, int *x, int *y ) {
	if ( point[0] < tracemap.world_mins[0] ) {
		point[0] = tracemap.world_mins[0];
	} else if ( point[0] > tracemap.world_maxs[0] ) {
		point[0] = tracemap.world_maxs[0];
	}
	if ( point[1] < tracemap.world_mins[1] ) {
		point[1] = tracemap.world_mins[1];
	} else if ( point[1] > tracemap.world_maxs[1] ) {
		point[1] = tracemap.world_maxs[1];
	}

	*x = (int)( ( point[0] - tracemap.world_mins[0] ) * tracemap.one_over_mapgrid_factor[0] );
	*y = (int)( ( point[1] - tracemap.world_mins[1] ) * tracemap.one_over_mapgrid_factor[1] );

	if ( *x < 0 ) *x = 0;
	if ( *x >= TRACEMAP_SIZE ) *x = TRACEMAP_SIZE - 1;
	if ( *y < 0 ) *y = 0;
	if ( *y >= TRACEMAP_SIZE ) *y = TRACEMAP_SIZE - 1;
}

/*
==================
BG_GetSkyHeightAtPoint

Returns the sky height at a world position, or MAX_WORLD_HEIGHT if no tracemap.
Used by atmospheric effects to know where rain/snow originates.
==================
*/
float BG_GetSkyHeightAtPoint( vec3_t pos ) {
	int i, j;
	vec3_t p;

	if ( !tracemap.loaded ) {
		return MAX_WORLD_HEIGHT;
	}

	VectorCopy( pos, p );
	BG_ClampPointToTracemapExtends( p, &i, &j );
	return tracemap.sky[j][i];
}

/*
==================
BG_GetGroundHeightAtPoint

Returns the ground height at a world position, or MIN_WORLD_HEIGHT if no tracemap.
==================
*/
float BG_GetGroundHeightAtPoint( vec3_t pos ) {
	int i, j;
	vec3_t p;

	if ( !tracemap.loaded ) {
		return MIN_WORLD_HEIGHT;
	}

	VectorCopy( pos, p );
	BG_ClampPointToTracemapExtends( p, &i, &j );
	return tracemap.ground[j][i];
}

/*
==================
BG_GenerateTracemap

Builds the tracemap by sampling sky/ground heights across the map.
Called during map load. Requires trace callbacks via the gen parameter.
==================
*/
void BG_GenerateTracemap( vec3_t world_mins, vec3_t world_maxs, void (*trace)( trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int, int ) ) {
	int i, j;
	vec3_t start, end;
	trace_t tr;
	float stepx, stepy;

	memset( &tracemap, 0, sizeof( tracemap ) );

	tracemap.world_mins[0] = world_mins[0];
	tracemap.world_mins[1] = world_mins[1];
	tracemap.world_maxs[0] = world_maxs[0];
	tracemap.world_maxs[1] = world_maxs[1];

	stepx = ( world_maxs[0] - world_mins[0] ) / TRACEMAP_SIZE;
	stepy = ( world_maxs[1] - world_mins[1] ) / TRACEMAP_SIZE;

	if ( stepx < 1 ) stepx = 1;
	if ( stepy < 1 ) stepy = 1;

	tracemap.one_over_mapgrid_factor[0] = 1.0f / stepx;
	tracemap.one_over_mapgrid_factor[1] = 1.0f / stepy;

	for ( j = 0; j < TRACEMAP_SIZE; j++ ) {
		for ( i = 0; i < TRACEMAP_SIZE; i++ ) {
			start[0] = world_mins[0] + ( i + 0.5f ) * stepx;
			start[1] = world_mins[1] + ( j + 0.5f ) * stepy;
			end[0] = start[0];
			end[1] = start[1];

			// trace down from above to find the first surface (sky or ground)
			start[2] = MAX_WORLD_HEIGHT;
			end[2] = MIN_WORLD_HEIGHT;

			trace( &tr, start, NULL, NULL, end, -1, CONTENTS_SOLID );

			if ( tr.fraction == 1.0f ) {
				// nothing hit — void
				tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
				tracemap.ground[j][i] = MIN_WORLD_HEIGHT;
				continue;
			}

			// first hit: is it sky?
			if ( tr.surfaceFlags & SURF_SKY ) {
				tracemap.sky[j][i] = tr.endpos[2];

				// continue tracing downward from below the sky to find ground
				start[2] = tr.endpos[2] - 1;
				trace( &tr, start, NULL, NULL, end, -1, CONTENTS_SOLID );

				if ( tr.fraction < 1.0f ) {
					tracemap.ground[j][i] = tr.endpos[2];
				} else {
					tracemap.ground[j][i] = MIN_WORLD_HEIGHT;
				}
			} else {
				// first hit is solid (roof/ceiling, no sky above)
				tracemap.sky[j][i] = MAX_WORLD_HEIGHT;
				tracemap.ground[j][i] = tr.endpos[2];
			}
		}
	}

	tracemap.loaded = qtrue;
}

/*
==================
BG_TracemapLoaded
==================
*/
qboolean BG_TracemapLoaded( void ) {
	return tracemap.loaded;
}

#endif // FEAT_ATMOSPHERIC
