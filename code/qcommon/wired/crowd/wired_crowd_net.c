// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* WiredCrowdNet implementation. See wired_crowd_net.h for the design. */

#include "wired_crowd_net.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WCN_MARKER  0xC0u
#define WCN_RESYNC  0x01u
#define WCN_BUCKETS 256u
/* An agent the client has never been sent counts as this many ticks old: it ranks
 * ahead of known agents at the same distance, but still nearest first among its
 * own kind. */
#define WCN_UNKNOWN_AGE 32u
#define WCN_HEADER_RESERVE 16u
/* A resync packet carries the quantisation box and the speed range, so a client
 * needs no side channel to learn how to read the records: 8 floats, little endian. */
#define WCN_CONFIG_BYTES 32u

#define WCN_PI 3.14159265358979323846f

typedef struct { uint16_t x, y, z; uint8_t h, s; } wcnRec_t;

/* ---- helpers ------------------------------------------------------------ */

static uint32_t wcn_pow2_at_least( uint32_t n ) {
	uint32_t p = 1u;
	while ( p < n ) p <<= 1;
	return p;
}

static uint32_t wcn_hash( uint32_t id, uint32_t mask ) {
	return ( id * 2654435761u >> 7 ) & mask;
}

static uint16_t wcn_quant( float v, float lo, float hi ) {
	float t = ( v - lo ) / ( hi - lo );
	if ( !( t > 0.0f ) ) t = 0.0f;
	if ( t > 1.0f ) t = 1.0f;
	return (uint16_t)( t * 65535.0f + 0.5f );
}

static float wcn_dequant( uint16_t q, float lo, float hi ) {
	return lo + (float)q * ( 1.0f / 65535.0f ) * ( hi - lo );
}

static wcnRec_t wcn_make_rec( const wired_crowd_net_config_t *cfg, float x, float y, float z, float vx, float vy ) {
	wcnRec_t r;
	float angle = atan2f( vy, vx ), speed = sqrtf( vx * vx + vy * vy ) / cfg->maxSpeed;
	if ( angle < 0.0f ) angle += 2.0f * WCN_PI;
	if ( speed > 1.0f ) speed = 1.0f;
	r.x = wcn_quant( x, cfg->worldMin[0], cfg->worldMax[0] );
	r.y = wcn_quant( y, cfg->worldMin[1], cfg->worldMax[1] );
	r.z = wcn_quant( z, cfg->worldMin[2], cfg->worldMax[2] );
	r.h = (uint8_t)( (uint32_t)( angle / ( 2.0f * WCN_PI ) * 256.0f + 0.5f ) & 255u );
	r.s = (uint8_t)( speed * 255.0f + 0.5f );
	return r;
}

static int wcn_rec_equal( const wcnRec_t *a, const wcnRec_t *b ) {
	return a->x == b->x && a->y == b->y && a->z == b->z && a->h == b->h && a->s == b->s;
}

static size_t wcn_put_varint( uint8_t *out, uint32_t v ) {
	size_t n = 0;
	while ( v >= 0x80u ) { out[n++] = (uint8_t)( v | 0x80u ); v >>= 7; }
	out[n++] = (uint8_t)v;
	return n;
}

static size_t wcn_varint_size( uint32_t v ) {
	size_t n = 1;
	while ( v >= 0x80u ) { ++n; v >>= 7; }
	return n;
}

/* Returns bytes consumed, 0 on truncation or overflow. */
static size_t wcn_get_varint( const uint8_t *in, size_t length, uint32_t *v ) {
	uint32_t result = 0;
	unsigned shift = 0;
	for ( size_t i = 0; i < length && i < 5u; ++i ) {
		result |= (uint32_t)( in[i] & 0x7fu ) << shift;
		if ( !( in[i] & 0x80u ) ) { *v = result; return i + 1u; }
		shift += 7;
	}
	return 0;
}

/* ---- server ------------------------------------------------------------- */

/* A uniform grid over the agents, built once per tick and shared by every client's
 * interest query: a client visits the cells its radius covers instead of scanning
 * the whole population (Phase 5: the interest set is cached per cell). */
#define WCN_GRID_CELL  256.0f
#define WCN_GRID_MAX   65536u

struct wired_crowd_net_server {
	wired_crowd_net_env_t    env;
	wired_crowd_net_config_t cfg;
	uint32_t                 tick, count;
	uint32_t                *id;
	wcnRec_t                *rec;
	float                   *fx, *fy;
	uint32_t                 gridX, gridY;
	float                    gridCell, gridInv;
	uint32_t                *gridStart, *gridItems, *gridCellOf;
};

wired_crowd_net_server_t *wired_crowd_net_server_create( const wired_crowd_net_env_t *env, const wired_crowd_net_config_t *cfg )
{
	wired_crowd_net_server_t *s;
	uint32_t n;
	if ( !env || !env->alloc || !env->release || !cfg || !cfg->capacity || !( cfg->maxSpeed > 0.0f ) || !( cfg->tickMsec > 0.0f ) ) return NULL;
	for ( int a = 0; a < 3; ++a ) if ( !( cfg->worldMax[a] > cfg->worldMin[a] ) ) return NULL;
	for ( int b = 0; b < 3; ++b ) if ( !cfg->rateEvery[b] ) return NULL;
	s = (wired_crowd_net_server_t *)env->alloc( sizeof( *s ) );
	if ( !s ) return NULL;
	memset( s, 0, sizeof( *s ) );
	s->env = *env; s->cfg = *cfg;
	n = cfg->capacity;
	s->id = (uint32_t *)env->alloc( sizeof( uint32_t ) * n );
	s->rec = (wcnRec_t *)env->alloc( sizeof( wcnRec_t ) * n );
	s->fx = (float *)env->alloc( sizeof( float ) * n );
	s->fy = (float *)env->alloc( sizeof( float ) * n );
	/* grid dimensions: cells of WCN_GRID_CELL, coarsened if the world is very large */
	s->gridCell = WCN_GRID_CELL;
	for ( ;; ) {
		const double w = ceil( (double)( cfg->worldMax[0] - cfg->worldMin[0] ) / s->gridCell );
		const double h = ceil( (double)( cfg->worldMax[1] - cfg->worldMin[1] ) / s->gridCell );
		if ( w * h <= (double)WCN_GRID_MAX ) { s->gridX = (uint32_t)( w < 1.0 ? 1.0 : w ); s->gridY = (uint32_t)( h < 1.0 ? 1.0 : h ); break; }
		s->gridCell *= 1.5f;
	}
	s->gridInv = 1.0f / s->gridCell;
	s->gridStart = (uint32_t *)env->alloc( sizeof( uint32_t ) * ( (size_t)s->gridX * s->gridY + 1u ) );
	s->gridItems = (uint32_t *)env->alloc( sizeof( uint32_t ) * n );
	s->gridCellOf = (uint32_t *)env->alloc( sizeof( uint32_t ) * n );
	if ( !s->id || !s->rec || !s->fx || !s->fy || !s->gridStart || !s->gridItems || !s->gridCellOf ) { wired_crowd_net_server_destroy( s ); return NULL; }
	return s;
}

void wired_crowd_net_server_destroy( wired_crowd_net_server_t *s )
{
	void ( *rel )( void * );
	if ( !s ) return;
	rel = s->env.release;
	if ( s->id ) rel( s->id );
	if ( s->rec ) rel( s->rec );
	if ( s->fx ) rel( s->fx );
	if ( s->fy ) rel( s->fy );
	if ( s->gridStart ) rel( s->gridStart );
	if ( s->gridItems ) rel( s->gridItems );
	if ( s->gridCellOf ) rel( s->gridCellOf );
	rel( s );
}

void wired_crowd_net_server_tick( wired_crowd_net_server_t *s, uint32_t serverTick, uint32_t count,
		const uint32_t *userId, const float *x, const float *y, const float *z, const float *vx, const float *vy )
{
	if ( !s ) return;
	if ( count > s->cfg.capacity ) count = s->cfg.capacity;
	s->tick = serverTick;
	s->count = count;
	/* the one quantisation of the tick: every client's packet is assembled from this */
	for ( uint32_t i = 0; i < count; ++i ) {
		s->id[i] = userId[i];
		s->fx[i] = x[i]; s->fy[i] = y[i];
		s->rec[i] = wcn_make_rec( &s->cfg, x[i], y[i], z[i], vx[i], vy[i] );
	}
	/* the shared interest grid: a stable counting sort by cell, so agents stay in
	 * ascending index order inside a cell */
	{
		const size_t cells = (size_t)s->gridX * s->gridY;
		memset( s->gridStart, 0, sizeof( uint32_t ) * ( cells + 1u ) );
		for ( uint32_t i = 0; i < count; ++i ) {
			int ix = (int)( ( x[i] - s->cfg.worldMin[0] ) * s->gridInv );
			int iy = (int)( ( y[i] - s->cfg.worldMin[1] ) * s->gridInv );
			if ( ix < 0 ) ix = 0; else if ( ix >= (int)s->gridX ) ix = (int)s->gridX - 1;
			if ( iy < 0 ) iy = 0; else if ( iy >= (int)s->gridY ) iy = (int)s->gridY - 1;
			s->gridCellOf[i] = (uint32_t)iy * s->gridX + (uint32_t)ix;
			++s->gridStart[s->gridCellOf[i] + 1u];
		}
		for ( size_t k = 0; k < cells; ++k ) s->gridStart[k + 1u] += s->gridStart[k];
		for ( uint32_t i = 0; i < count; ++i ) s->gridItems[s->gridStart[s->gridCellOf[i]]++] = i;
		for ( size_t k = cells; k > 0; --k ) s->gridStart[k] = s->gridStart[k - 1u];
		s->gridStart[0] = 0u;
	}
}

/* ---- peer (one client's baseline) --------------------------------------- */

typedef struct { uint32_t id; wcnRec_t rec; uint32_t lastTick; uint8_t used; uint8_t seen; } wcnPeerEntry_t;

struct wired_crowd_net_peer {
	wired_crowd_net_env_t env;
	uint32_t              capacity, mask;
	uint32_t              known;        /* agents the client currently holds         */
	uint32_t              viewCap;      /* what the client can hold                  */
	wcnPeerEntry_t       *table;
	uint16_t              seq;
	int                   needResync;
	/* scratch for one encode */
	uint32_t *cand, *candBucket, *order, *sel, *remove;
	uint64_t *selKey;
};

wired_crowd_net_peer_t *wired_crowd_net_peer_create( const wired_crowd_net_env_t *env, const wired_crowd_net_config_t *cfg )
{
	wired_crowd_net_peer_t *p;
	uint32_t size;
	if ( !env || !env->alloc || !env->release || !cfg || !cfg->capacity ) return NULL;
	p = (wired_crowd_net_peer_t *)env->alloc( sizeof( *p ) );
	if ( !p ) return NULL;
	memset( p, 0, sizeof( *p ) );
	p->env = *env;
	p->capacity = cfg->capacity;
	p->viewCap = cfg->viewCapacity ? cfg->viewCapacity : WIRED_CROWD_NET_VIEW_CAPACITY;
	size = wcn_pow2_at_least( cfg->capacity * 2u );
	p->mask = size - 1u;
	p->table = (wcnPeerEntry_t *)env->alloc( sizeof( wcnPeerEntry_t ) * size );
	p->cand = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	p->candBucket = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	p->order = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	p->sel = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	p->remove = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	p->selKey = (uint64_t *)env->alloc( sizeof( uint64_t ) * cfg->capacity );
	if ( !p->selKey || !p->table || !p->cand || !p->candBucket || !p->order || !p->sel || !p->remove ) {
		wired_crowd_net_peer_destroy( p );
		return NULL;
	}
	memset( p->table, 0, sizeof( wcnPeerEntry_t ) * size );
	p->needResync = 1;
	return p;
}

void wired_crowd_net_peer_destroy( wired_crowd_net_peer_t *p )
{
	void ( *rel )( void * );
	if ( !p ) return;
	rel = p->env.release;
	if ( p->table ) rel( p->table );
	if ( p->cand ) rel( p->cand );
	if ( p->candBucket ) rel( p->candBucket );
	if ( p->order ) rel( p->order );
	if ( p->sel ) rel( p->sel );
	if ( p->remove ) rel( p->remove );
	if ( p->selKey ) rel( p->selKey );
	rel( p );
}

void wired_crowd_net_peer_resync( wired_crowd_net_peer_t *p )
{
	if ( !p ) return;
	memset( p->table, 0, sizeof( wcnPeerEntry_t ) * ( (size_t)p->mask + 1u ) );
	p->known = 0;
	p->needResync = 1;
}

static wcnPeerEntry_t *wcn_peer_find( wired_crowd_net_peer_t *p, uint32_t id )
{
	uint32_t slot = wcn_hash( id, p->mask );
	while ( p->table[slot].used ) {
		if ( p->table[slot].id == id ) return &p->table[slot];
		slot = ( slot + 1u ) & p->mask;
	}
	return NULL;
}

static wcnPeerEntry_t *wcn_peer_insert( wired_crowd_net_peer_t *p, uint32_t id )
{
	uint32_t slot = wcn_hash( id, p->mask );
	while ( p->table[slot].used ) {
		if ( p->table[slot].id == id ) return &p->table[slot];
		slot = ( slot + 1u ) & p->mask;
	}
	p->table[slot].used = 1; p->table[slot].id = id; p->table[slot].seen = 0;
	++p->known;
	return &p->table[slot];
}

/* Linear-probing deletion by backward shift, so no tombstones accumulate. */
static void wcn_peer_erase( wired_crowd_net_peer_t *p, uint32_t id )
{
	uint32_t i = wcn_hash( id, p->mask ), j;
	while ( p->table[i].used && p->table[i].id != id ) i = ( i + 1u ) & p->mask;
	if ( !p->table[i].used ) return;
	j = i;
	for ( ;; ) {
		uint32_t home;
		j = ( j + 1u ) & p->mask;
		if ( !p->table[j].used ) break;
		home = wcn_hash( p->table[j].id, p->mask );
		if ( ( i <= j ) ? ( i < home && home <= j ) : ( i < home || home <= j ) ) continue;
		p->table[i] = p->table[j];
		i = j;
	}
	memset( &p->table[i], 0, sizeof( p->table[i] ) );
	--p->known;
}

/* Sort keys are (userId << 32 | index): ordering by the key orders by userId and
 * needs no shared state, so several clients can be encoded at once. */
static int wcn_cmp_u64( const void *a, const void *b ) {
	const uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return x < y ? -1 : x > y ? 1 : 0;
}
static int wcn_cmp_u32( const void *a, const void *b ) {
	const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : x > y ? 1 : 0;
}

/* Delta against a baseline fits when every axis moved by at most +-127 units. */
static int wcn_delta_fits( const wcnRec_t *a, const wcnRec_t *base ) {
	const int dx = (int)a->x - (int)base->x, dy = (int)a->y - (int)base->y, dz = (int)a->z - (int)base->z;
	return dx >= -127 && dx <= 127 && dy >= -127 && dy <= 127 && dz >= -127 && dz <= 127;
}

size_t wired_crowd_net_encode( wired_crowd_net_server_t *s, wired_crowd_net_peer_t *p,
		float viewerX, float viewerY, float radius, size_t budget,
		uint8_t *out, size_t capacity, wired_crowd_net_encode_stats_t *stats )
{
	wired_crowd_net_encode_stats_t st;
	const float r2 = radius * radius;
	const float b0 = s->cfg.bandRadius[0] * s->cfg.bandRadius[0];
	const float b1 = s->cfg.bandRadius[1] * s->cfg.bandRadius[1];
	uint32_t candCount = 0, selCount = 0, removeCount = 0;
	uint32_t bucketCounts[WCN_BUCKETS + 1u];
	size_t used, tickBytes;
	const int resync = p->needResync;
	size_t pos;
	uint32_t prevId;

	memset( &st, 0, sizeof( st ) );
	if ( stats ) memset( stats, 0, sizeof( *stats ) );
	if ( capacity < WCN_HEADER_RESERVE ) return (size_t)-1;
	if ( budget > capacity ) budget = capacity;
	if ( budget < WCN_HEADER_RESERVE ) budget = WCN_HEADER_RESERVE;

	for ( uint32_t k = 0; k <= p->mask; ++k ) p->table[k].seen = 0;

	/* 1. who is in range and due: walk only the grid cells the radius covers */
	{
		int x0 = (int)( ( viewerX - radius - s->cfg.worldMin[0] ) * s->gridInv );
		int x1 = (int)( ( viewerX + radius - s->cfg.worldMin[0] ) * s->gridInv );
		int y0 = (int)( ( viewerY - radius - s->cfg.worldMin[1] ) * s->gridInv );
		int y1 = (int)( ( viewerY + radius - s->cfg.worldMin[1] ) * s->gridInv );
		if ( x0 < 0 ) x0 = 0;
		if ( y0 < 0 ) y0 = 0;
		if ( x1 >= (int)s->gridX ) x1 = (int)s->gridX - 1;
		if ( y1 >= (int)s->gridY ) y1 = (int)s->gridY - 1;
		for ( int cy = y0; cy <= y1; ++cy ) {
			for ( int cx = x0; cx <= x1; ++cx ) {
				const uint32_t cell = (uint32_t)cy * s->gridX + (uint32_t)cx;
				for ( uint32_t k = s->gridStart[cell]; k < s->gridStart[cell + 1u]; ++k ) {
					const uint32_t i = s->gridItems[k];
					const float dx = s->fx[i] - viewerX, dy = s->fy[i] - viewerY;
					const float d2 = dx * dx + dy * dy;
					wcnPeerEntry_t *e;
					uint32_t band, rate, age;
					int due;
					if ( d2 > r2 ) continue;
					++st.agentsConsidered;
					e = wcn_peer_find( p, s->id[i] );
					if ( e ) e->seen = 1;
					band = d2 < b0 ? 0u : d2 < b1 ? 1u : 2u;
					rate = s->cfg.rateEvery[band];
					/* Due when the client has never had it, or when it has waited a full
					 * period for this band and the record changed. Age, not a fixed phase,
					 * so an agent that lost out on budget stays due until it is sent. */
					age = e ? s->tick - e->lastTick : WCN_UNKNOWN_AGE;
					due = !e || ( age >= rate && !wcn_rec_equal( &s->rec[i], &e->rec ) );
					if ( !due ) continue;
					p->cand[candCount] = i;
					{
						/* nearest first, but the longer an agent has waited the more it
						 * counts, so near agents that need an update every tick cannot
						 * starve the rest of the budget */
						const float urgency = r2 > 0.0f ? ( d2 / r2 ) / ( 1.0f + (float)age * 0.25f ) : 0.0f;
						uint32_t bucket = (uint32_t)( urgency * (float)( WCN_BUCKETS - 1u ) );
						if ( bucket >= WCN_BUCKETS ) bucket = WCN_BUCKETS - 1u;
						p->candBucket[candCount] = bucket;
					}
					++candCount;
				}
			}
		}
	}

	/* 2. nearest first: a stable counting sort over distance buckets (agents stay in
	 *    ascending index order inside a bucket) */
	memset( bucketCounts, 0, sizeof( bucketCounts ) );
	for ( uint32_t k = 0; k < candCount; ++k ) ++bucketCounts[p->candBucket[k] + 1u];
	for ( uint32_t b = 0; b < WCN_BUCKETS; ++b ) bucketCounts[b + 1u] += bucketCounts[b];
	for ( uint32_t k = 0; k < candCount; ++k ) p->order[bucketCounts[p->candBucket[k]]++] = p->cand[k];

	/* 3. removals: known to the client but no longer in range or gone */
	for ( uint32_t k = 0; k <= p->mask; ++k )
		if ( p->table[k].used && !p->table[k].seen ) p->remove[removeCount++] = p->table[k].id;
	qsort( p->remove, removeCount, sizeof( uint32_t ), wcn_cmp_u32 );
	if ( removeCount > 255u ) removeCount = 255u;       /* the rest go in a later packet */

	/* 4. budget. Header and removals are charged first, then agents nearest first. */
	used = WCN_HEADER_RESERVE + ( resync ? WCN_CONFIG_BYTES : 0u );
	prevId = 0;
	for ( uint32_t k = 0; k < removeCount; ++k ) {
		used += wcn_varint_size( p->remove[k] - prevId );
		prevId = p->remove[k];
	}
	/* Three quarters of what is left goes to the best-ranked agents, nearest and
	 * stalest first. The last quarter goes to the WORST-ranked ones, walked from
	 * the far end: an agent that keeps losing to nearer, busier ones still reaches
	 * the front of that queue as the ones behind it are served, so nothing starves
	 * however much the near field changes. */
	{
		const size_t firstShare = used + ( budget - used ) * 3u / 4u;
		uint32_t stop = candCount;
		/* room left in the client's view once this packet's removals are applied */
		const uint32_t held = resync ? 0u : p->known - removeCount;
		uint32_t room = p->viewCap > held ? p->viewCap - held : 0u;
		for ( uint32_t k = 0; k < candCount; ++k ) {
			const uint32_t i = p->order[k];
			const wcnPeerEntry_t *e = resync ? NULL : wcn_peer_find( p, s->id[i] );
			const size_t cost = 3u + ( ( e && wcn_delta_fits( &s->rec[i], &e->rec ) ) ? 1u + 5u : 1u + 8u );
			if ( used + cost > firstShare ) { stop = k; break; }
			if ( !e ) { if ( !room ) continue; --room; }       /* a new agent needs a free slot in the view */
			used += cost;
			p->sel[selCount++] = i;
		}
		for ( uint32_t k = candCount; k > stop; --k ) {
			const uint32_t i = p->order[k - 1u];
			const wcnPeerEntry_t *e = resync ? NULL : wcn_peer_find( p, s->id[i] );
			const size_t cost = 3u + ( ( e && wcn_delta_fits( &s->rec[i], &e->rec ) ) ? 1u + 5u : 1u + 8u );
			if ( used + cost > budget ) continue;     /* a smaller record further in may still fit */
			if ( !e ) { if ( !room ) continue; --room; }
			used += cost;
			p->sel[selCount++] = i;
		}
		st.agentsDeferred = candCount - selCount;
	}

	if ( !selCount && !removeCount && !resync ) {
		if ( stats ) *stats = st;
		return 0u;
	}

	/* 5. write: entries in ascending userId so the id deltas stay small */
	for ( uint32_t k = 0; k < selCount; ++k ) p->selKey[k] = ( (uint64_t)s->id[p->sel[k]] << 32 ) | p->sel[k];
	qsort( p->selKey, selCount, sizeof( uint64_t ), wcn_cmp_u64 );
	for ( uint32_t k = 0; k < selCount; ++k ) p->sel[k] = (uint32_t)( p->selKey[k] & 0xffffffffu );
	pos = 0;
	out[pos++] = (uint8_t)( WCN_MARKER | ( resync ? WCN_RESYNC : 0u ) );
	out[pos++] = (uint8_t)( p->seq & 0xffu ); out[pos++] = (uint8_t)( p->seq >> 8 );
	if ( resync ) {
		const float box[8] = { s->cfg.worldMin[0], s->cfg.worldMin[1], s->cfg.worldMin[2],
			s->cfg.worldMax[0], s->cfg.worldMax[1], s->cfg.worldMax[2], s->cfg.maxSpeed, s->cfg.tickMsec };
		for ( int k = 0; k < 8; ++k ) {
			uint32_t bits;
			memcpy( &bits, &box[k], sizeof( bits ) );
			out[pos++] = (uint8_t)( bits ); out[pos++] = (uint8_t)( bits >> 8 );
			out[pos++] = (uint8_t)( bits >> 16 ); out[pos++] = (uint8_t)( bits >> 24 );
		}
	}
	tickBytes = wcn_put_varint( out + pos, s->tick ); pos += tickBytes;
	pos += wcn_put_varint( out + pos, removeCount );
	prevId = 0;
	for ( uint32_t k = 0; k < removeCount; ++k ) {
		pos += wcn_put_varint( out + pos, p->remove[k] - prevId );
		prevId = p->remove[k];
		wcn_peer_erase( p, p->remove[k] );
		++st.removals;
	}
	pos += wcn_put_varint( out + pos, selCount );
	prevId = 0;
	for ( uint32_t k = 0; k < selCount; ++k ) {
		const uint32_t i = p->sel[k];
		const wcnRec_t *r = &s->rec[i];
		wcnPeerEntry_t *e = resync ? NULL : wcn_peer_find( p, s->id[i] );
		const int asDelta = e && wcn_delta_fits( r, &e->rec );
		pos += wcn_put_varint( out + pos, s->id[i] - prevId );
		prevId = s->id[i];
		if ( asDelta ) {
			out[pos++] = 0u;
			out[pos++] = (uint8_t)(int8_t)( (int)r->x - (int)e->rec.x );
			out[pos++] = (uint8_t)(int8_t)( (int)r->y - (int)e->rec.y );
			out[pos++] = (uint8_t)(int8_t)( (int)r->z - (int)e->rec.z );
			++st.deltaRecords;
		} else {
			out[pos++] = 1u;
			out[pos++] = (uint8_t)( r->x & 0xffu ); out[pos++] = (uint8_t)( r->x >> 8 );
			out[pos++] = (uint8_t)( r->y & 0xffu ); out[pos++] = (uint8_t)( r->y >> 8 );
			out[pos++] = (uint8_t)( r->z & 0xffu ); out[pos++] = (uint8_t)( r->z >> 8 );
			++st.absoluteRecords;
		}
		out[pos++] = r->h; out[pos++] = r->s;
		e = wcn_peer_insert( p, s->id[i] );
		e->rec = *r;
		e->lastTick = s->tick;
		++st.agentsSent;
	}
	++p->seq;
	p->needResync = 0;
	st.bytes = (uint32_t)pos;
	if ( stats ) *stats = st;
	return pos;
}

/* ---- client ------------------------------------------------------------- */

typedef struct {
	uint32_t id;
	uint32_t prevTick, currTick;
	wcnRec_t prev, curr;
	uint8_t  hasPrev;
} wcnAgent_t;

typedef struct { uint32_t id; uint8_t absolute; int8_t d[3]; wcnRec_t rec; } wcnOp_t;

/* The tracked agents live in a dense array (so a frame walks only what is there,
 * not a sparse table); a hash maps an id to its index. */
struct wired_crowd_net_view {
	wired_crowd_net_env_t    env;
	wired_crowd_net_config_t cfg;
	uint32_t                 mask;
	uint32_t                *map;          /* id -> index + 1, open addressing */
	wcnAgent_t              *agents;
	uint32_t                 count;
	wcnOp_t                 *ops;
	uint32_t                *removals;
	uint32_t                *bucketStart;  /* nearest-agent selection scratch */
	uint32_t                *bucketItems;   /* per agent: its distance bucket */
	uint32_t                *bucketOrder;   /* agent indices in bucket order  */
	uint16_t                 nextSeq;
	int                      synced;
};

#define WCN_NEAR_BUCKETS 1024u
#define WCN_NEAR_UNIT    8.0f              /* distance resolution of the nearest-first order */

wired_crowd_net_view_t *wired_crowd_net_view_create( const wired_crowd_net_env_t *env, const wired_crowd_net_config_t *cfg )
{
	wired_crowd_net_view_t *v;
	uint32_t size;
	if ( !env || !env->alloc || !env->release || !cfg || !cfg->capacity ) return NULL;
	v = (wired_crowd_net_view_t *)env->alloc( sizeof( *v ) );
	if ( !v ) return NULL;
	memset( v, 0, sizeof( *v ) );
	v->env = *env; v->cfg = *cfg;
	size = wcn_pow2_at_least( cfg->capacity * 2u );
	v->mask = size - 1u;
	v->map = (uint32_t *)env->alloc( sizeof( uint32_t ) * size );
	v->agents = (wcnAgent_t *)env->alloc( sizeof( wcnAgent_t ) * cfg->capacity );
	v->ops = (wcnOp_t *)env->alloc( sizeof( wcnOp_t ) * cfg->capacity );
	v->removals = (uint32_t *)env->alloc( sizeof( uint32_t ) * 256u );
	v->bucketStart = (uint32_t *)env->alloc( sizeof( uint32_t ) * ( WCN_NEAR_BUCKETS + 1u ) );
	v->bucketItems = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	v->bucketOrder = (uint32_t *)env->alloc( sizeof( uint32_t ) * cfg->capacity );
	if ( !v->bucketOrder || !v->map || !v->agents || !v->ops || !v->removals || !v->bucketStart || !v->bucketItems ) { wired_crowd_net_view_destroy( v ); return NULL; }
	memset( v->map, 0, sizeof( uint32_t ) * size );
	return v;
}

void wired_crowd_net_view_destroy( wired_crowd_net_view_t *v )
{
	void ( *rel )( void * );
	if ( !v ) return;
	rel = v->env.release;
	if ( v->map ) rel( v->map );
	if ( v->agents ) rel( v->agents );
	if ( v->ops ) rel( v->ops );
	if ( v->removals ) rel( v->removals );
	if ( v->bucketStart ) rel( v->bucketStart );
	if ( v->bucketItems ) rel( v->bucketItems );
	if ( v->bucketOrder ) rel( v->bucketOrder );
	rel( v );
}

static wcnAgent_t *wcn_view_find( wired_crowd_net_view_t *v, uint32_t id )
{
	uint32_t slot = wcn_hash( id, v->mask );
	while ( v->map[slot] ) {
		wcnAgent_t *a = &v->agents[v->map[slot] - 1u];
		if ( a->id == id ) return a;
		slot = ( slot + 1u ) & v->mask;
	}
	return NULL;
}

static wcnAgent_t *wcn_view_insert( wired_crowd_net_view_t *v, uint32_t id )
{
	uint32_t slot = wcn_hash( id, v->mask );
	wcnAgent_t *a;
	while ( v->map[slot] ) {
		a = &v->agents[v->map[slot] - 1u];
		if ( a->id == id ) return a;
		slot = ( slot + 1u ) & v->mask;
	}
	if ( v->count >= v->cfg.capacity ) return NULL;
	a = &v->agents[v->count];
	memset( a, 0, sizeof( *a ) );
	a->id = id;
	v->map[slot] = ++v->count;
	return a;
}

static void wcn_view_map_set( wired_crowd_net_view_t *v, uint32_t id, uint32_t index )
{
	uint32_t slot = wcn_hash( id, v->mask );
	while ( v->map[slot] && v->agents[v->map[slot] - 1u].id != id ) slot = ( slot + 1u ) & v->mask;
	v->map[slot] = index + 1u;
}

static void wcn_view_erase( wired_crowd_net_view_t *v, uint32_t id )
{
	uint32_t i = wcn_hash( id, v->mask ), j, index, last;
	while ( v->map[i] && v->agents[v->map[i] - 1u].id != id ) i = ( i + 1u ) & v->mask;
	if ( !v->map[i] ) return;
	index = v->map[i] - 1u;
	/* backward-shift deletion from the hash */
	j = i;
	for ( ;; ) {
		uint32_t home;
		j = ( j + 1u ) & v->mask;
		if ( !v->map[j] ) break;
		home = wcn_hash( v->agents[v->map[j] - 1u].id, v->mask );
		if ( ( i <= j ) ? ( i < home && home <= j ) : ( i < home || home <= j ) ) continue;
		v->map[i] = v->map[j];
		i = j;
	}
	v->map[i] = 0u;
	/* the last agent moves into the freed slot of the dense array */
	last = --v->count;
	if ( index != last ) {
		v->agents[index] = v->agents[last];
		wcn_view_map_set( v, v->agents[index].id, index );
	}
}

int wired_crowd_net_decode( wired_crowd_net_view_t *v, const uint8_t *in, size_t length, uint32_t *serverTick )
{
	size_t pos = 0, n;
	uint32_t tick, removeCount, entryCount, id = 0, i;
	uint16_t seq;
	int resync;
	float box[8] = { 0 };

	/* ---- pass 1: parse and validate; nothing is applied yet ---- */
	if ( length < 4u || ( in[0] & 0xfeu ) != WCN_MARKER ) return WIRED_CROWD_NET_MALFORMED;
	resync = in[0] & WCN_RESYNC;
	seq = (uint16_t)( in[1] | ( in[2] << 8 ) );
	pos = 3u;
	if ( !resync && ( !v->synced || seq != v->nextSeq ) ) return WIRED_CROWD_NET_GAP;
	if ( resync ) {
		if ( length - pos < WCN_CONFIG_BYTES + 1u ) return WIRED_CROWD_NET_MALFORMED;
		for ( int k = 0; k < 8; ++k ) {
			const uint32_t bits = (uint32_t)in[pos] | ( (uint32_t)in[pos + 1] << 8 )
				| ( (uint32_t)in[pos + 2] << 16 ) | ( (uint32_t)in[pos + 3] << 24 );
			memcpy( &box[k], &bits, sizeof( bits ) );
			pos += 4u;
		}
		for ( int a = 0; a < 3; ++a )
			if ( !( box[3 + a] > box[a] ) || !isfinite( box[a] ) || !isfinite( box[3 + a] ) ) return WIRED_CROWD_NET_MALFORMED;
		if ( !( box[6] > 0.0f ) || !isfinite( box[6] ) || !( box[7] > 0.0f ) || !isfinite( box[7] ) ) return WIRED_CROWD_NET_MALFORMED;
	}
	if ( !( n = wcn_get_varint( in + pos, length - pos, &tick ) ) ) return WIRED_CROWD_NET_MALFORMED;
	pos += n;
	if ( !( n = wcn_get_varint( in + pos, length - pos, &removeCount ) ) || removeCount > 255u ) return WIRED_CROWD_NET_MALFORMED;
	pos += n;
	for ( i = 0; i < removeCount; ++i ) {
		uint32_t delta;
		if ( !( n = wcn_get_varint( in + pos, length - pos, &delta ) ) ) return WIRED_CROWD_NET_MALFORMED;
		pos += n;
		id += delta;
		v->removals[i] = id;
	}
	if ( !( n = wcn_get_varint( in + pos, length - pos, &entryCount ) ) || entryCount > v->cfg.capacity ) return WIRED_CROWD_NET_MALFORMED;
	pos += n;
	id = 0;
	for ( i = 0; i < entryCount; ++i ) {
		uint32_t delta;
		wcnOp_t *op = &v->ops[i];
		if ( !( n = wcn_get_varint( in + pos, length - pos, &delta ) ) ) return WIRED_CROWD_NET_MALFORMED;
		pos += n;
		if ( i && !delta ) return WIRED_CROWD_NET_MALFORMED;      /* ids strictly ascend */
		id += delta;
		op->id = id;
		if ( pos >= length ) return WIRED_CROWD_NET_MALFORMED;
		op->absolute = in[pos++] & 1u;
		if ( op->absolute ) {
			if ( length - pos < 8u ) return WIRED_CROWD_NET_MALFORMED;
			op->rec.x = (uint16_t)( in[pos] | ( in[pos + 1] << 8 ) );
			op->rec.y = (uint16_t)( in[pos + 2] | ( in[pos + 3] << 8 ) );
			op->rec.z = (uint16_t)( in[pos + 4] | ( in[pos + 5] << 8 ) );
			op->rec.h = in[pos + 6]; op->rec.s = in[pos + 7];
			pos += 8u;
		} else {
			if ( resync || length - pos < 5u ) return WIRED_CROWD_NET_MALFORMED;   /* a resync is all absolute */
			if ( !wcn_view_find( v, id ) ) return WIRED_CROWD_NET_MALFORMED;      /* delta for an agent we never had */
			for ( uint32_t r = 0; r < removeCount; ++r )                           /* ... or one this packet removes */
				if ( v->removals[r] == id ) return WIRED_CROWD_NET_MALFORMED;
			op->d[0] = (int8_t)in[pos]; op->d[1] = (int8_t)in[pos + 1]; op->d[2] = (int8_t)in[pos + 2];
			op->rec.h = in[pos + 3]; op->rec.s = in[pos + 4];
			pos += 5u;
		}
	}
	if ( pos != length ) return WIRED_CROWD_NET_MALFORMED;
	/* Capacity check before anything changes: a packet whose new agents do not fit
	 * is rejected whole, so the view never ends up half applied. */
	{
		uint32_t fresh = 0;
		if ( !resync ) {
			for ( i = 0; i < entryCount; ++i ) if ( v->ops[i].absolute && !wcn_view_find( v, v->ops[i].id ) ) ++fresh;
			if ( v->count + fresh - removeCount > v->cfg.capacity && v->count + fresh > v->cfg.capacity ) return WIRED_CROWD_NET_MALFORMED;
		}
	}

	/* ---- pass 2: apply ---- */
	if ( resync ) {
		memset( v->map, 0, sizeof( uint32_t ) * ( (size_t)v->mask + 1u ) );
		v->count = 0;
		for ( int a = 0; a < 3; ++a ) { v->cfg.worldMin[a] = box[a]; v->cfg.worldMax[a] = box[3 + a]; }
		v->cfg.maxSpeed = box[6];
		v->cfg.tickMsec = box[7];
	}
	for ( i = 0; i < removeCount; ++i ) wcn_view_erase( v, v->removals[i] );
	for ( i = 0; i < entryCount; ++i ) {
		const wcnOp_t *op = &v->ops[i];
		wcnAgent_t *e;
		if ( op->absolute ) {
			const int existed = wcn_view_find( v, op->id ) != NULL;
			e = wcn_view_insert( v, op->id );
			if ( !e ) continue;                                   /* cannot happen after the capacity check */
			if ( existed ) { e->prev = e->curr; e->prevTick = e->currTick; e->hasPrev = 1; }
			e->curr = op->rec;
		} else {
			e = wcn_view_find( v, op->id );
			e->prev = e->curr; e->prevTick = e->currTick; e->hasPrev = 1;
			e->curr.x = (uint16_t)( (int)e->curr.x + op->d[0] );
			e->curr.y = (uint16_t)( (int)e->curr.y + op->d[1] );
			e->curr.z = (uint16_t)( (int)e->curr.z + op->d[2] );
			e->curr.h = op->rec.h; e->curr.s = op->rec.s;
		}
		e->currTick = tick;
		if ( !e->hasPrev ) { e->prev = e->curr; e->prevTick = tick; }   /* a first sighting interpolates from itself */
	}
	v->nextSeq = (uint16_t)( seq + 1u );
	v->synced = 1;
	if ( serverTick ) *serverTick = tick;
	return WIRED_CROWD_NET_OK;
}

uint32_t wired_crowd_net_view_count( const wired_crowd_net_view_t *v )
{
	return v ? v->count : 0u;
}

float wired_crowd_net_view_tick_msec( const wired_crowd_net_view_t *v )
{
	return v ? v->cfg.tickMsec : 0.0f;
}

static void wcn_sample_one( const wired_crowd_net_view_t *v, const wcnAgent_t *e, float renderTick, wired_crowd_net_sample_t *o )
{
	float t = 1.0f, ha, hb, dh;
	if ( e->hasPrev && e->currTick > e->prevTick ) {
		t = ( renderTick - (float)e->prevTick ) / (float)( e->currTick - e->prevTick );
		if ( t < 0.0f ) t = 0.0f;
		if ( t > 1.0f ) t = 1.0f;
	}
	o->userId = e->id;
	o->x = wcn_dequant( e->prev.x, v->cfg.worldMin[0], v->cfg.worldMax[0] ) * ( 1.0f - t )
		+ wcn_dequant( e->curr.x, v->cfg.worldMin[0], v->cfg.worldMax[0] ) * t;
	o->y = wcn_dequant( e->prev.y, v->cfg.worldMin[1], v->cfg.worldMax[1] ) * ( 1.0f - t )
		+ wcn_dequant( e->curr.y, v->cfg.worldMin[1], v->cfg.worldMax[1] ) * t;
	o->z = wcn_dequant( e->prev.z, v->cfg.worldMin[2], v->cfg.worldMax[2] ) * ( 1.0f - t )
		+ wcn_dequant( e->curr.z, v->cfg.worldMin[2], v->cfg.worldMax[2] ) * t;
	ha = (float)e->prev.h * ( 2.0f * WCN_PI / 256.0f );
	hb = (float)e->curr.h * ( 2.0f * WCN_PI / 256.0f );
	dh = hb - ha;
	if ( dh > WCN_PI ) dh -= 2.0f * WCN_PI;
	if ( dh < -WCN_PI ) dh += 2.0f * WCN_PI;
	o->heading = ha + dh * t;
	o->speed = ( (float)e->prev.s * ( 1.0f - t ) + (float)e->curr.s * t ) * ( v->cfg.maxSpeed / 255.0f );
}

size_t wired_crowd_net_view_sample( const wired_crowd_net_view_t *v, float renderTick,
		wired_crowd_net_sample_t *out, size_t maxOut )
{
	size_t n = 0;
	for ( uint32_t k = 0; k < v->count && n < maxOut; ++k ) wcn_sample_one( v, &v->agents[k], renderTick, &out[n++] );
	return n;
}

/* The nearest maxOut agents to (cx, cy, cz), nearest first at 8-unit resolution:
 * one pass over the tracked agents to bucket them by distance (counting sort, no
 * comparison sort), then only the chosen ones are interpolated. */
size_t wired_crowd_net_view_sample_nearest( wired_crowd_net_view_t *v, float renderTick,
		float cx, float cy, float cz, wired_crowd_net_sample_t *out, size_t maxOut )
{
	size_t n = 0;
	if ( !v || !maxOut || !v->count ) return 0;
	memset( v->bucketStart, 0, sizeof( uint32_t ) * ( WCN_NEAR_BUCKETS + 1u ) );
	for ( uint32_t k = 0; k < v->count; ++k ) {
		const wcnAgent_t *e = &v->agents[k];
		const float dx = wcn_dequant( e->curr.x, v->cfg.worldMin[0], v->cfg.worldMax[0] ) - cx;
		const float dy = wcn_dequant( e->curr.y, v->cfg.worldMin[1], v->cfg.worldMax[1] ) - cy;
		const float dz = wcn_dequant( e->curr.z, v->cfg.worldMin[2], v->cfg.worldMax[2] ) - cz;
		uint32_t bucket = (uint32_t)( sqrtf( dx * dx + dy * dy + dz * dz ) * ( 1.0f / WCN_NEAR_UNIT ) );
		if ( bucket >= WCN_NEAR_BUCKETS ) bucket = WCN_NEAR_BUCKETS - 1u;
		v->bucketItems[k] = bucket;                 /* the bucket of agent k, until the sort below */
		++v->bucketStart[bucket + 1u];
	}
	for ( uint32_t b = 0; b < WCN_NEAR_BUCKETS; ++b ) v->bucketStart[b + 1u] += v->bucketStart[b];
	/* second pass: place each agent in its bucket (stable, ascending index inside a
	 * bucket), using bucketStart as the write cursor */
	for ( uint32_t k = 0; k < v->count; ++k ) v->bucketOrder[v->bucketStart[v->bucketItems[k]]++] = k;
	for ( uint32_t k = 0; k < v->count && n < maxOut; ++k ) wcn_sample_one( v, &v->agents[v->bucketOrder[k]], renderTick, &out[n++] );
	return n;
}
