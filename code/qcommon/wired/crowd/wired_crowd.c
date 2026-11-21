// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* WiredCrowd implementation. See wired_crowd.h for the design rules. */

#include "wired_crowd.h"

#include <math.h>
#include <string.h>

#define WC_MAX_CELLS 262144u

struct wired_crowd {
	wired_crowd_env_t    env;
	wired_crowd_config_t cfg;
	uint32_t             count;
	uint32_t             tick;

	/* current state (read during a step) */
	float   *px, *py, *pz, *vx, *vy;
	/* next state (written during a step), swapped in at the end */
	float   *qx, *qy, *qz, *qvx, *qvy;
	float   *goalX, *goalY, *goalSpeed;
	float   *nearD2;          /* squared distance to the nearest viewer          */
	uint8_t *tier;
	uint32_t *userId;
	uint32_t *idMap;          /* userId -> index + 1 (0 = empty), open addressing   */
	uint32_t  idMask;

	/* spatial hash over the non-dormant agents, rebuilt every step */
	uint32_t  cellsX, cellsY;
	float     invCell, cellSize;
	uint32_t *cellStart;      /* cellsX*cellsY + 1 */
	uint32_t *cellItems;      /* agent indices, ascending within a cell */
	uint32_t *cellOf;         /* per agent: its cell, or UINT32_MAX when dormant */

	/* promotion candidates of the last step */
	uint32_t *promo;
	float    *promoD2;
	uint32_t  promoCount;

	wired_crowd_report_t report;
};

static float *wc_floats( const wired_crowd_env_t *env, uint32_t n ) {
	float *p = (float *)env->alloc( sizeof( float ) * n );
	if ( p ) memset( p, 0, sizeof( float ) * n );
	return p;
}

wired_crowd_t *wired_crowd_create( const wired_crowd_env_t *env, const wired_crowd_config_t *config )
{
	wired_crowd_t *c;
	uint32_t n;
	if ( !env || !env->alloc || !env->release || !config || !config->capacity ) return NULL;
	if ( !( config->avoidRadius > 0.0f ) || !( config->maxSpeed > 0.0f ) || !( config->maxAccel > 0.0f ) ) return NULL;
	if ( !( config->worldMax[0] > config->worldMin[0] ) || !( config->worldMax[1] > config->worldMin[1] ) ) return NULL;
	c = (wired_crowd_t *)env->alloc( sizeof( *c ) );
	if ( !c ) return NULL;
	memset( c, 0, sizeof( *c ) );
	c->env = *env;
	c->cfg = *config;
	if ( !c->cfg.grain ) c->cfg.grain = 256u;
	if ( !c->cfg.maxNeighbors ) c->cfg.maxNeighbors = 48u;
	n = config->capacity;

	/* The hash cell is at least the avoid radius, and large enough that the grid
	 * stays within WC_MAX_CELLS whatever the world size. */
	c->cellSize = config->cellSize > config->avoidRadius ? config->cellSize : config->avoidRadius;
	for ( ;; ) {
		const float w = config->worldMax[0] - config->worldMin[0];
		const float h = config->worldMax[1] - config->worldMin[1];
		const double cells = ceil( (double)w / c->cellSize ) * ceil( (double)h / c->cellSize );
		if ( cells <= (double)WC_MAX_CELLS ) break;
		c->cellSize *= 1.5f;
	}
	c->invCell = 1.0f / c->cellSize;
	c->cellsX = (uint32_t)ceil( (double)( config->worldMax[0] - config->worldMin[0] ) / c->cellSize );
	c->cellsY = (uint32_t)ceil( (double)( config->worldMax[1] - config->worldMin[1] ) / c->cellSize );
	if ( !c->cellsX ) c->cellsX = 1u;
	if ( !c->cellsY ) c->cellsY = 1u;

	c->px = wc_floats( env, n ); c->py = wc_floats( env, n ); c->pz = wc_floats( env, n );
	c->vx = wc_floats( env, n ); c->vy = wc_floats( env, n );
	c->qx = wc_floats( env, n ); c->qy = wc_floats( env, n ); c->qz = wc_floats( env, n );
	c->qvx = wc_floats( env, n ); c->qvy = wc_floats( env, n );
	c->goalX = wc_floats( env, n ); c->goalY = wc_floats( env, n ); c->goalSpeed = wc_floats( env, n );
	c->nearD2 = wc_floats( env, n );
	c->tier = (uint8_t *)env->alloc( n );
	c->userId = (uint32_t *)env->alloc( sizeof( uint32_t ) * n );
	{
		uint32_t size = 1u;
		while ( size < n * 2u ) size <<= 1;
		c->idMask = size - 1u;
		c->idMap = (uint32_t *)env->alloc( sizeof( uint32_t ) * size );
		if ( c->idMap ) memset( c->idMap, 0, sizeof( uint32_t ) * size );
	}
	c->cellStart = (uint32_t *)env->alloc( sizeof( uint32_t ) * ( (size_t)c->cellsX * c->cellsY + 1u ) );
	c->cellItems = (uint32_t *)env->alloc( sizeof( uint32_t ) * n );
	c->cellOf = (uint32_t *)env->alloc( sizeof( uint32_t ) * n );
	if ( c->cfg.maxPromotions ) {
		c->promo = (uint32_t *)env->alloc( sizeof( uint32_t ) * c->cfg.maxPromotions );
		c->promoD2 = wc_floats( env, c->cfg.maxPromotions );
	}
	if ( !c->px || !c->py || !c->pz || !c->vx || !c->vy || !c->qx || !c->qy || !c->qz || !c->qvx || !c->qvy
			|| !c->goalX || !c->goalY || !c->goalSpeed || !c->nearD2 || !c->tier || !c->userId || !c->idMap
			|| !c->cellStart || !c->cellItems || !c->cellOf
			|| ( c->cfg.maxPromotions && ( !c->promo || !c->promoD2 ) ) ) {
		wired_crowd_destroy( c );
		return NULL;
	}
	memset( c->tier, 0, n );
	memset( c->userId, 0, sizeof( uint32_t ) * n );
	return c;
}

void wired_crowd_destroy( wired_crowd_t *c )
{
	void ( *rel )( void * );
	void *blocks[25];
	unsigned i = 0;
	if ( !c ) return;
	rel = c->env.release;
	blocks[i++] = c->px; blocks[i++] = c->py; blocks[i++] = c->pz; blocks[i++] = c->vx; blocks[i++] = c->vy;
	blocks[i++] = c->qx; blocks[i++] = c->qy; blocks[i++] = c->qz; blocks[i++] = c->qvx; blocks[i++] = c->qvy;
	blocks[i++] = c->goalX; blocks[i++] = c->goalY; blocks[i++] = c->goalSpeed; blocks[i++] = c->nearD2;
	blocks[i++] = c->tier; blocks[i++] = c->userId; blocks[i++] = c->cellStart; blocks[i++] = c->cellItems;
	blocks[i++] = c->cellOf; blocks[i++] = c->promo; blocks[i++] = c->promoD2; blocks[i++] = c->idMap;
	for ( unsigned b = 0; b < i; ++b ) if ( blocks[b] ) rel( blocks[b] );
	rel( c );
}

static uint32_t wc_id_hash( uint32_t id, uint32_t mask ) { return ( id * 2654435761u >> 7 ) & mask; }

uint32_t wired_crowd_index_of( const wired_crowd_t *c, uint32_t userId )
{
	uint32_t slot;
	if ( !c ) return UINT32_MAX;
	slot = wc_id_hash( userId, c->idMask );
	while ( c->idMap[slot] ) {
		const uint32_t index = c->idMap[slot] - 1u;
		if ( c->userId[index] == userId ) return index;
		slot = ( slot + 1u ) & c->idMask;
	}
	return UINT32_MAX;
}

static void wc_id_set( wired_crowd_t *c, uint32_t userId, uint32_t index )
{
	uint32_t slot = wc_id_hash( userId, c->idMask );
	while ( c->idMap[slot] && c->userId[c->idMap[slot] - 1u] != userId ) slot = ( slot + 1u ) & c->idMask;
	c->idMap[slot] = index + 1u;
}

/* Linear-probing deletion by backward shift (no tombstones). The id being erased
 * must still be readable through userId[] of its own index. */
static void wc_id_erase( wired_crowd_t *c, uint32_t userId )
{
	uint32_t i = wc_id_hash( userId, c->idMask ), j;
	while ( c->idMap[i] && c->userId[c->idMap[i] - 1u] != userId ) i = ( i + 1u ) & c->idMask;
	if ( !c->idMap[i] ) return;
	j = i;
	for ( ;; ) {
		uint32_t home;
		j = ( j + 1u ) & c->idMask;
		if ( !c->idMap[j] ) break;
		home = wc_id_hash( c->userId[c->idMap[j] - 1u], c->idMask );
		if ( ( i <= j ) ? ( i < home && home <= j ) : ( i < home || home <= j ) ) continue;
		c->idMap[i] = c->idMap[j];
		i = j;
	}
	c->idMap[i] = 0u;
}

uint32_t wired_crowd_add( wired_crowd_t *c, wired_crowd_vec3_t p, uint32_t userId )
{
	uint32_t i;
	if ( !c || c->count >= c->cfg.capacity ) return UINT32_MAX;
	if ( wired_crowd_index_of( c, userId ) != UINT32_MAX ) return UINT32_MAX;   /* ids are unique */
	i = c->count++;
	c->px[i] = p.x; c->py[i] = p.y; c->pz[i] = p.z;
	c->vx[i] = c->vy[i] = 0.0f;
	c->goalX[i] = p.x; c->goalY[i] = p.y; c->goalSpeed[i] = 0.0f;
	c->tier[i] = (uint8_t)WIRED_CROWD_TIER_FULL;
	c->userId[i] = userId;
	wc_id_set( c, userId, i );
	return i;
}

void wired_crowd_remove( wired_crowd_t *c, uint32_t i )
{
	uint32_t last;
	if ( !c || i >= c->count ) return;
	wc_id_erase( c, c->userId[i] );
	last = --c->count;
	if ( i == last ) return;
	c->px[i] = c->px[last]; c->py[i] = c->py[last]; c->pz[i] = c->pz[last];
	c->vx[i] = c->vx[last]; c->vy[i] = c->vy[last];
	c->goalX[i] = c->goalX[last]; c->goalY[i] = c->goalY[last]; c->goalSpeed[i] = c->goalSpeed[last];
	c->tier[i] = c->tier[last]; c->userId[i] = c->userId[last];
	wc_id_set( c, c->userId[i], i );          /* the agent that moved into the gap */
}

int wired_crowd_take( wired_crowd_t *c, uint32_t userId, wired_crowd_agent_t *out )
{
	const uint32_t i = wired_crowd_index_of( c, userId );
	if ( i == UINT32_MAX ) return 0;
	if ( out ) {
		out->userId = userId;
		out->position.x = c->px[i]; out->position.y = c->py[i]; out->position.z = c->pz[i];
		out->vx = c->vx[i]; out->vy = c->vy[i];
		out->goal.x = c->goalX[i]; out->goal.y = c->goalY[i]; out->goal.speed = c->goalSpeed[i];
	}
	wired_crowd_remove( c, i );
	return 1;
}

uint32_t wired_crowd_put( wired_crowd_t *c, const wired_crowd_agent_t *a )
{
	uint32_t i;
	if ( !c || !a ) return UINT32_MAX;
	i = wired_crowd_add( c, a->position, a->userId );
	if ( i == UINT32_MAX ) return UINT32_MAX;
	c->vx[i] = a->vx; c->vy[i] = a->vy;
	c->goalX[i] = a->goal.x; c->goalY[i] = a->goal.y; c->goalSpeed[i] = a->goal.speed;
	return i;
}

uint32_t wired_crowd_count( const wired_crowd_t *c ) { return c ? c->count : 0u; }

void wired_crowd_set_goal( wired_crowd_t *c, uint32_t i, wired_crowd_goal_t g )
{
	if ( !c || i >= c->count ) return;
	c->goalX[i] = g.x; c->goalY[i] = g.y; c->goalSpeed[i] = g.speed;
}

/* ---- step -------------------------------------------------------------- */

typedef struct {
	wired_crowd_t            *c;
	float                     dt;
	const wired_crowd_vec3_t *viewers;
	uint32_t                  viewerCount;
} wcStepCtx_t;

static uint32_t wc_period( uint8_t tier )
{
	switch ( tier ) {
	case WIRED_CROWD_TIER_FULL: return 1u;
	case WIRED_CROWD_TIER_HALF: return 2u;
	case WIRED_CROWD_TIER_FAR:  return 8u;
	default:                    return 0u;   /* dormant: never */
	}
}

static uint32_t wc_cell_of( const wired_crowd_t *c, float x, float y )
{
	int ix = (int)( ( x - c->cfg.worldMin[0] ) * c->invCell );
	int iy = (int)( ( y - c->cfg.worldMin[1] ) * c->invCell );
	if ( ix < 0 ) ix = 0; else if ( ix >= (int)c->cellsX ) ix = (int)c->cellsX - 1;
	if ( iy < 0 ) iy = 0; else if ( iy >= (int)c->cellsY ) iy = (int)c->cellsY - 1;
	return (uint32_t)iy * c->cellsX + (uint32_t)ix;
}

/* Phase 1: level of detail from the nearest viewer. Writes only its own agents. */
static void wc_tier_body( void *p, uint32_t chunk, uint32_t begin, uint32_t end )
{
	const wcStepCtx_t *s = (const wcStepCtx_t *)p;
	wired_crowd_t *c = s->c;
	const float r0 = c->cfg.tierRadius[0] * c->cfg.tierRadius[0];
	const float r1 = c->cfg.tierRadius[1] * c->cfg.tierRadius[1];
	const float r2 = c->cfg.tierRadius[2] * c->cfg.tierRadius[2];
	(void)chunk;
	for ( uint32_t i = begin; i < end; ++i ) {
		float best = INFINITY;
		for ( uint32_t v = 0; v < s->viewerCount; ++v ) {
			const float dx = c->px[i] - s->viewers[v].x;
			const float dy = c->py[i] - s->viewers[v].y;
			const float d2 = dx * dx + dy * dy;
			if ( d2 < best ) best = d2;
		}
		c->nearD2[i] = best;
		c->tier[i] = (uint8_t)( s->viewerCount == 0u ? WIRED_CROWD_TIER_DORMANT
			: best <= r0 ? WIRED_CROWD_TIER_FULL
			: best <= r1 ? WIRED_CROWD_TIER_HALF
			: best <= r2 ? WIRED_CROWD_TIER_FAR : WIRED_CROWD_TIER_DORMANT );
	}
}

/* Phase 2: one agent's next state from the previous state. */
static void wc_move_body( void *p, uint32_t chunk, uint32_t begin, uint32_t end )
{
	const wcStepCtx_t *s = (const wcStepCtx_t *)p;
	wired_crowd_t *c = s->c;
	const float r = c->cfg.avoidRadius, r2 = r * r;
	(void)chunk;
	for ( uint32_t i = begin; i < end; ++i ) {
		const uint32_t period = wc_period( c->tier[i] );
		float x = c->px[i], y = c->py[i], z = c->pz[i], vx = c->vx[i], vy = c->vy[i];
		if ( period && ( ( c->tick + c->userId[i] ) % period ) == 0u ) {
			const float dt = s->dt * (float)period;
			/* desired velocity toward the goal */
			float dvx = 0.0f, dvy = 0.0f;
			{
				const float gx = c->goalX[i] - x, gy = c->goalY[i] - y;
				const float g2 = gx * gx + gy * gy;
				if ( c->goalSpeed[i] > 0.0f && g2 > 1.0f ) {
					const float inv = c->goalSpeed[i] / sqrtf( g2 );
					dvx = gx * inv; dvy = gy * inv;
				}
			}
			/* separation from the neighbours in the 3x3 cells around, in cell
			 * order and ascending agent index, so the sum is order-fixed */
			{
				const uint32_t cell = c->cellOf[i];
				const int cx = (int)( cell % c->cellsX ), cy = (int)( cell / c->cellsX );
				float px = 0.0f, py = 0.0f;
				uint32_t examined = 0;            /* candidates looked at, in or out of range */
				/* Own cell first, then the ring: when the neighbour cap bites in a packed
				 * crowd it is the far cells that are skipped, never the agents standing
				 * right next to this one. */
				static const int8_t ring[9][2] = { {0,0}, {-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {1,-1}, {-1,1}, {1,1} };
				for ( int r9 = 0; r9 < 9 && examined < c->cfg.maxNeighbors; ++r9 ) {
					const int xx = cx + ring[r9][0], yy = cy + ring[r9][1];
					uint32_t k, kEnd, cellIndex;
					if ( xx < 0 || xx >= (int)c->cellsX || yy < 0 || yy >= (int)c->cellsY ) continue;
					cellIndex = (uint32_t)yy * c->cellsX + (uint32_t)xx;
					kEnd = c->cellStart[cellIndex + 1u];
					for ( k = c->cellStart[cellIndex]; k < kEnd && examined < c->cfg.maxNeighbors; ++k ) {
						const uint32_t j = c->cellItems[k];
						float ddx, ddy, d2;
						++examined;
						if ( j == i ) continue;
						ddx = x - c->px[j]; ddy = y - c->py[j];
						d2 = ddx * ddx + ddy * ddy;
						if ( d2 >= r2 ) continue;
						if ( d2 < 1e-8f ) {            /* coincident: a fixed push, no division by zero */
							ddx = ( i < j ) ? 1.0f : -1.0f; ddy = 0.0f; d2 = 1.0f;
						}
						{
							const float d = sqrtf( d2 );
							const float w = ( r - d ) / r / d;
							px += ddx * w; py += ddy * w;
						}
					}
				}
				dvx += px * c->cfg.maxSpeed; dvy += py * c->cfg.maxSpeed;
			}
			/* clamp the desired speed, then limit the change of velocity */
			{
				const float s2 = dvx * dvx + dvy * dvy;
				if ( s2 > c->cfg.maxSpeed * c->cfg.maxSpeed ) {
					const float k = c->cfg.maxSpeed / sqrtf( s2 );
					dvx *= k; dvy *= k;
				}
			}
			{
				float ax = dvx - vx, ay = dvy - vy;
				const float a2 = ax * ax + ay * ay;
				const float maxDv = c->cfg.maxAccel * dt;
				if ( a2 > maxDv * maxDv ) {
					const float k = maxDv / sqrtf( a2 );
					ax *= k; ay *= k;
				}
				vx += ax; vy += ay;
			}
			{
				const float nx = x + vx * dt, ny = y + vy * dt;
				float nz = z;
				if ( c->env.ground_z ) nz = c->env.ground_z( c->env.ground_ctx, nx, ny, z );
				if ( nz != nz ) {                 /* blocked: stay put, lose the velocity */
					vx = vy = 0.0f;
				} else {
					x = nx; y = ny; z = nz;
				}
			}
		}
		c->qx[i] = x; c->qy[i] = y; c->qz[i] = z; c->qvx[i] = vx; c->qvy[i] = vy;
	}
}

static void wc_build_hash( wired_crowd_t *c )
{
	const size_t cells = (size_t)c->cellsX * c->cellsY;
	memset( c->cellStart, 0, sizeof( uint32_t ) * ( cells + 1u ) );
	for ( uint32_t i = 0; i < c->count; ++i ) {
		if ( c->tier[i] == WIRED_CROWD_TIER_DORMANT ) { c->cellOf[i] = UINT32_MAX; continue; }
		c->cellOf[i] = wc_cell_of( c, c->px[i], c->py[i] );
		++c->cellStart[c->cellOf[i] + 1u];
	}
	for ( size_t k = 0; k < cells; ++k ) c->cellStart[k + 1u] += c->cellStart[k];
	/* Stable fill: agents are visited in ascending index, so each cell ends up
	 * sorted by index. The fill uses cellStart itself as the write cursor; once
	 * every cell has advanced to the next cell's begin offset, shifting the array
	 * one slot restores the begin offsets without a scratch array. */
	for ( uint32_t i = 0; i < c->count; ++i ) {
		if ( c->cellOf[i] == UINT32_MAX ) continue;
		c->cellItems[c->cellStart[c->cellOf[i]]++] = i;
	}
	for ( size_t k = cells; k > 0; --k ) c->cellStart[k] = c->cellStart[k - 1u];
	c->cellStart[0] = 0u;
}

static void wc_select_promotions( wired_crowd_t *c )
{
	const float limit = c->cfg.promoteRadius * c->cfg.promoteRadius;
	c->promoCount = 0;
	if ( !c->cfg.maxPromotions || !( c->cfg.promoteRadius > 0.0f ) ) return;
	for ( uint32_t i = 0; i < c->count; ++i ) {
		uint32_t pos;
		const float d2 = c->nearD2[i];
		if ( c->tier[i] != WIRED_CROWD_TIER_FULL || d2 > limit ) continue;
		if ( c->promoCount == c->cfg.maxPromotions ) {
			const float worst = c->promoD2[c->promoCount - 1u];
			if ( d2 >= worst ) continue;       /* ties keep the lower index already in */
			--c->promoCount;
		}
		/* insertion into the ascending (distance, index) list */
		pos = c->promoCount++;
		while ( pos > 0u && c->promoD2[pos - 1u] > d2 ) {
			c->promoD2[pos] = c->promoD2[pos - 1u];
			c->promo[pos] = c->promo[pos - 1u];
			--pos;
		}
		c->promoD2[pos] = d2;
		c->promo[pos] = i;
	}
}

static void wc_swap( float **a, float **b ) { float *t = *a; *a = *b; *b = t; }

void wired_crowd_step( wired_crowd_t *c, float dt, const wired_crowd_vec3_t *viewers, uint32_t viewerCount )
{
	wcStepCtx_t s;
	if ( !c || !( dt > 0.0f ) ) return;
	s.c = c; s.dt = dt; s.viewers = viewers; s.viewerCount = viewers ? viewerCount : 0u;
	memset( &c->report, 0, sizeof( c->report ) );

	wired_tasks_parallel_for( c->env.tasks, c->count, c->cfg.grain, wc_tier_body, &s );
	wc_build_hash( c );
	wired_tasks_parallel_for( c->env.tasks, c->count, c->cfg.grain, wc_move_body, &s );

	wc_swap( &c->px, &c->qx ); wc_swap( &c->py, &c->qy ); wc_swap( &c->pz, &c->qz );
	wc_swap( &c->vx, &c->qvx ); wc_swap( &c->vy, &c->qvy );

	for ( uint32_t i = 0; i < c->count; ++i ) {
		++c->report.tierCount[c->tier[i]];
		if ( wc_period( c->tier[i] ) && ( ( c->tick + c->userId[i] ) % wc_period( c->tier[i] ) ) == 0u )
			++c->report.simulated;
	}
	wc_select_promotions( c );
	c->report.promotionCount = c->promoCount;
	++c->tick;
}

/* ---- read access ------------------------------------------------------- */

const float    *wired_crowd_pos_x( const wired_crowd_t *c ) { return c->px; }
const float    *wired_crowd_pos_y( const wired_crowd_t *c ) { return c->py; }
const float    *wired_crowd_pos_z( const wired_crowd_t *c ) { return c->pz; }
const float    *wired_crowd_vel_x( const wired_crowd_t *c ) { return c->vx; }
const float    *wired_crowd_vel_y( const wired_crowd_t *c ) { return c->vy; }
const uint8_t  *wired_crowd_tier( const wired_crowd_t *c ) { return c->tier; }
const uint32_t *wired_crowd_user_id( const wired_crowd_t *c ) { return c->userId; }

const uint32_t *wired_crowd_promotions( const wired_crowd_t *c, uint32_t *count )
{
	if ( count ) *count = c->promoCount;
	return c->promo;
}

static uint64_t wc_fnv( uint64_t h, const void *data, size_t n )
{
	const unsigned char *p = (const unsigned char *)data;
	for ( size_t i = 0; i < n; ++i ) { h ^= p[i]; h *= 1099511628211ull; }
	return h;
}

void wired_crowd_report( const wired_crowd_t *c, wired_crowd_report_t *out )
{
	uint64_t h = 14695981039346656037ull;
	*out = c->report;
	for ( uint32_t i = 0; i < c->count; ++i ) {
		h = wc_fnv( h, &c->px[i], sizeof( float ) );
		h = wc_fnv( h, &c->py[i], sizeof( float ) );
		h = wc_fnv( h, &c->pz[i], sizeof( float ) );
		h = wc_fnv( h, &c->vx[i], sizeof( float ) );
		h = wc_fnv( h, &c->vy[i], sizeof( float ) );
		h = wc_fnv( h, &c->tier[i], 1u );
	}
	out->stateHash = h;
}
