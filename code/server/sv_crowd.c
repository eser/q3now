// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
sv_crowd.c -- the server-side crowd service (capacity roadmap doc-1, Phase 3/4).

Owns the engine-side population (WiredCrowd), steps it once per game tick on the
WiredTasks scheduler, and feeds the crowd channel (WiredCrowdNet): one quantisation
per tick, one packet per client assembled from it.

The service is deliberately engine-only. It does not create gentities; the players
and everything the Q3 mechanics and client prediction touch stay in the normal
entity channel. What the crowd adds is a population that does not count against
the 1024-entity and per-snapshot limits.

Console control (the game module will drive the same entry points later):
  crowd_spawn <count> [radius]   scatter agents around the first human client
  crowd_clear                    remove every agent
  crowd_stats                    population, tiers and last-step numbers
Cvars: sv_crowdBudget (bytes per client packet), sv_crowdRadius (interest radius).
===========================================================================
*/

#include "server.h"
#include <math.h>
#include "../qcommon/wired/crowd/wired_crowd.h"
#include "../qcommon/wired/crowd/wired_crowd_net.h"
#include "../qcommon/wired/tasks/wired_tasks_engine.h"
#include "../qcommon/wired/workload_stats.h"

LOG_DECLARE_CHANNEL( ch_server, "server" );

#define SV_CROWD_MAX_AGENTS   65536u
#define SV_CROWD_PACKET_BYTES 4096u

static cvar_t *sv_crowdBudget;
static cvar_t *sv_crowdRadius;
static cvar_t *sv_crowdMaxPromoted;   // 0 (default) = promotion off until a game module opts in
static cvar_t *sv_crowdAuto;      // spawn this many agents once a human is in the game (test and benchmark aid)

static wired_crowd_t            *s_crowd;
static wired_crowd_net_server_t *s_netServer;
static wired_crowd_net_peer_t   *s_peers[MAX_CLIENTS];
static uint32_t                  s_nextUserId = 1u;
static uint32_t                  s_tick;
static uint32_t                  s_rng = 0x9e3779b9u;
static wired_crowd_net_config_t  s_netConfig;

/* Agents the game has promoted to real entities. They are out of the crowd (and so
 * out of the crowd channel) while promoted; the game owns what they are. */
#define SC_MAX_PROMOTED    128
#define SC_ENTITY_RESERVE  64     /* entity slots left for everything else the game spawns */
static wired_crowd_agent_t s_promoted[SC_MAX_PROMOTED];
static int                 s_promotedCount;

/* Packets assembled in one parallel batch before the snapshot loop sends them. */
static uint8_t  s_packet[MAX_CLIENTS][SV_CROWD_PACKET_BYTES];
static size_t   s_packetLen[MAX_CLIENTS];
static qboolean s_packetReady[MAX_CLIENTS];
static uint32_t s_packetAgents[MAX_CLIENTS];

static void *SC_Alloc( size_t n ) { return Z_Malloc( n ); }
static void  SC_Release( void *p ) { Z_Free( p ); }

/* A tiny deterministic generator: the crowd must not depend on the thread that
 * happens to run a step, and spawning must be reproducible from the seed. */
static uint32_t SC_Rand( void ) { s_rng = s_rng * 1664525u + 1013904223u; return s_rng >> 8; }
static float SC_RandF( float lo, float hi ) { return lo + ( hi - lo ) * (float)( SC_Rand() & 0xffffu ) / 65535.0f; }

/* Ground field. Stepping an agent must not cost a collision trace: a few thousand
 * agents a tick would spend more time in the BSP than in the whole step. The world
 * is immutable while a crowd exists, so the surfaces under every cell are traced
 * once, when the crowd is created, and the step then reads a table. A cell keeps
 * up to SC_GROUND_LAYERS surfaces (floors above floors); a query picks the highest
 * one an agent can step up onto from where it stands. The field is read-only after
 * the build, so workers read it freely. */
#define SC_GROUND_LAYERS   4
#define SC_GROUND_CELL     64.0f
#define SC_GROUND_MAX_CELLS 120000u
#define SC_STEP_UP         48.0f

typedef struct {
	float    minX, minY, inv;
	int      cellsX, cellsY;
	uint8_t *count;
	float   *z;
} scGround_t;

static scGround_t s_ground;

static void SC_GroundFree( void )
{
	if ( s_ground.count ) Z_Free( s_ground.count );
	if ( s_ground.z ) Z_Free( s_ground.z );
	memset( &s_ground, 0, sizeof( s_ground ) );
}

/* One surface at or below z under (x, y); false when there is none. */
static qboolean SC_TraceDown( float x, float y, float fromZ, float floorZ, float *hitZ, qboolean *startSolid )
{
	trace_t tr;
	vec3_t start, end;
	VectorSet( start, x, y, fromZ );
	VectorSet( end, x, y, floorZ );
	CM_BoxTrace( &tr, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
	*startSolid = tr.startsolid ? qtrue : qfalse;
	if ( tr.startsolid || tr.fraction >= 1.0f ) return qfalse;
	*hitZ = tr.endpos[2];
	return qtrue;
}

static void SC_BuildGround( const vec3_t mins, const vec3_t maxs )
{
	float cell = SC_GROUND_CELL;
	size_t cells;
	const int64_t t0 = Sys_Milliseconds();
	SC_GroundFree();
	for ( ;; ) {
		const float w = maxs[0] - mins[0], h = maxs[1] - mins[1];
		s_ground.cellsX = (int)ceilf( w / cell ); s_ground.cellsY = (int)ceilf( h / cell );
		if ( s_ground.cellsX < 1 ) s_ground.cellsX = 1;
		if ( s_ground.cellsY < 1 ) s_ground.cellsY = 1;
		if ( (size_t)s_ground.cellsX * (size_t)s_ground.cellsY <= SC_GROUND_MAX_CELLS ) break;
		cell *= 1.5f;
	}
	s_ground.minX = mins[0]; s_ground.minY = mins[1]; s_ground.inv = 1.0f / cell;
	cells = (size_t)s_ground.cellsX * (size_t)s_ground.cellsY;
	s_ground.count = (uint8_t *)Z_Malloc( cells );
	s_ground.z = (float *)Z_Malloc( cells * SC_GROUND_LAYERS * sizeof( float ) );
	for ( int cy = 0; cy < s_ground.cellsY; ++cy ) {
		for ( int cx = 0; cx < s_ground.cellsX; ++cx ) {
			const size_t index = (size_t)cy * (size_t)s_ground.cellsX + (size_t)cx;
			const float x = mins[0] + ( (float)cx + 0.5f ) * cell, y = mins[1] + ( (float)cy + 0.5f ) * cell;
			float z = maxs[2] + 16.0f;
			int found = 0;
			for ( int guard = 0; guard < 96 && z > mins[2] - 64.0f && found < SC_GROUND_LAYERS; ++guard ) {
				float hit;
				qboolean solid;
				if ( SC_TraceDown( x, y, z, mins[2] - 64.0f, &hit, &solid ) ) {
					s_ground.z[index * SC_GROUND_LAYERS + (size_t)found++] = hit;
					z = hit - 40.0f;          /* continue below this surface, past a thin floor */
				} else if ( solid ) {
					z -= 40.0f;               /* started inside solid: walk down out of it */
				} else {
					break;                    /* nothing further below */
				}
			}
			s_ground.count[index] = (uint8_t)found;
		}
	}
	Com_Log( SEV_INFO, LOG_CH( ch_server ), "crowd: ground field %dx%d cells of %.0f units over (%.0f %.0f %.0f)-(%.0f %.0f %.0f) built in %d ms\n",
		s_ground.cellsX, s_ground.cellsY, cell, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
		(int)( Sys_Milliseconds() - t0 ) );
}

/* Ground height under (x, y): the highest surface the agent can step up to from
 * hintZ. NaN where it cannot stand: no floor in the cell (the void, outside the
 * map) or only surfaces higher than a step above it (walls and their tops), which
 * is what stops agents walking through geometry. Pure read of the table built
 * above. */
static float SC_Ground( void *ctx, float x, float y, float hintZ )
{
	int cx, cy;
	size_t index;
	(void)ctx;
	if ( !s_ground.count ) return hintZ;
	cx = (int)floorf( ( x - s_ground.minX ) * s_ground.inv );
	cy = (int)floorf( ( y - s_ground.minY ) * s_ground.inv );
	if ( cx < 0 || cx >= s_ground.cellsX || cy < 0 || cy >= s_ground.cellsY ) return NAN;
	index = (size_t)cy * (size_t)s_ground.cellsX + (size_t)cx;
	{
		const int n = s_ground.count[index];
		const float *layer = &s_ground.z[index * SC_GROUND_LAYERS];
		for ( int k = 0; k < n; ++k )                 /* layers are stored top to bottom */
			if ( layer[k] <= hintZ + SC_STEP_UP ) return layer[k];
		return NAN;
	}
}

static qboolean SC_HasHuman( void )
{
	for ( int i = 0; i < sv_maxclients->integer; ++i )
		if ( svs.clients[i].state == CS_ACTIVE && svs.clients[i].netchan.remoteAddress.type != NA_BOT ) return qtrue;
	return qfalse;
}

static void SC_DestroyPeer( int client )
{
	wired_crowd_net_peer_destroy( s_peers[client] );
	s_peers[client] = NULL;
}

void SV_Crowd_Clear( void )
{
	s_promotedCount = 0;
	memset( s_packetReady, 0, sizeof( s_packetReady ) );
	for ( int i = 0; i < MAX_CLIENTS; ++i ) if ( s_peers[i] ) SC_DestroyPeer( i );
	wired_crowd_net_server_destroy( s_netServer );
	s_netServer = NULL;
	wired_crowd_destroy( s_crowd );
	s_crowd = NULL;
	SC_GroundFree();
	s_nextUserId = 1u;
	s_tick = 0u;
}

static qboolean SC_Ensure( void )
{
	vec3_t mins, maxs;
	wired_crowd_env_t env;
	wired_crowd_config_t cfg;
	if ( s_crowd ) return qtrue;
	if ( sv.state != SS_GAME ) return qfalse;
	CM_ModelBounds( CM_InlineModel( 0 ), mins, maxs );
	SC_BuildGround( mins, maxs );

	memset( &env, 0, sizeof( env ) );
	env.alloc = SC_Alloc; env.release = SC_Release;
	env.ground_z = SC_Ground;
	env.tasks = WiredTasks_Get();

	memset( &cfg, 0, sizeof( cfg ) );
	cfg.capacity = SV_CROWD_MAX_AGENTS;
	cfg.cellSize = 96.0f;
	cfg.avoidRadius = 48.0f;
	cfg.maxSpeed = 180.0f;
	cfg.maxAccel = 700.0f;
	cfg.tierRadius[0] = 1536.0f; cfg.tierRadius[1] = 3072.0f; cfg.tierRadius[2] = 6144.0f;
	cfg.promoteRadius = 384.0f;
	cfg.maxPromotions = 32;
	cfg.grain = 256;
	cfg.worldMin[0] = mins[0] - 256.0f; cfg.worldMin[1] = mins[1] - 256.0f;
	cfg.worldMax[0] = maxs[0] + 256.0f; cfg.worldMax[1] = maxs[1] + 256.0f;
	s_crowd = wired_crowd_create( &env, &cfg );
	if ( !s_crowd ) return qfalse;

	{
		wired_crowd_net_env_t nenv;
		nenv.alloc = SC_Alloc; nenv.release = SC_Release;
		memset( &s_netConfig, 0, sizeof( s_netConfig ) );
		s_netConfig.capacity = SV_CROWD_MAX_AGENTS;
		VectorSet( s_netConfig.worldMin, cfg.worldMin[0], cfg.worldMin[1], mins[2] - 512.0f );
		VectorSet( s_netConfig.worldMax, cfg.worldMax[0], cfg.worldMax[1], maxs[2] + 512.0f );
		s_netConfig.maxSpeed = cfg.maxSpeed;
		s_netConfig.tickMsec = (float)SV_FrameMsec();
		s_netConfig.rateEvery[0] = 1; s_netConfig.rateEvery[1] = 2; s_netConfig.rateEvery[2] = 8;
		s_netConfig.bandRadius[0] = 1024.0f; s_netConfig.bandRadius[1] = 2048.0f; s_netConfig.bandRadius[2] = 4096.0f;
		s_netServer = wired_crowd_net_server_create( &nenv, &s_netConfig );
		if ( !s_netServer ) { wired_crowd_destroy( s_crowd ); s_crowd = NULL; return qfalse; }
	}
	return qtrue;
}

/* ---- per game tick -------------------------------------------------------- */

void SV_Crowd_Frame( int frameMsec )
{
	wired_crowd_vec3_t viewers[MAX_CLIENTS];
	uint32_t viewerCount = 0, count;
	const uint32_t seed = (uint32_t)sv.time;
	if ( sv.state != SS_GAME || frameMsec <= 0 ) return;
	if ( !s_crowd && sv_crowdAuto && sv_crowdAuto->integer > 0 && SC_HasHuman() ) {
		/* the same entry the console uses, driven by a cvar so a sanctioned gate
		 * run can load a crowd without a console */
		char cmd[48];
		Com_sprintf( cmd, sizeof( cmd ), "crowd_spawn %d 2400\n", sv_crowdAuto->integer );
		Cbuf_ExecuteText( EXEC_NOW, cmd );
	}
	if ( !s_crowd ) return;

	for ( int i = 0; i < sv_maxclients->integer && viewerCount < MAX_CLIENTS; ++i ) {
		const client_t *c = &svs.clients[i];
		const playerState_t *ps;
		if ( c->state != CS_ACTIVE || c->netchan.remoteAddress.type == NA_BOT ) continue;
		ps = SV_GameClientNum( i );
		viewers[viewerCount].x = ps->origin[0];
		viewers[viewerCount].y = ps->origin[1];
		viewers[viewerCount].z = ps->origin[2];
		++viewerCount;
	}

	count = wired_crowd_count( s_crowd );
	{
		/* The agents choose a new wander goal on a staggered schedule. */
		const uint32_t *id = wired_crowd_user_id( s_crowd );
		const float *px = wired_crowd_pos_x( s_crowd ), *py = wired_crowd_pos_y( s_crowd );
		for ( uint32_t i = 0; i < count; ++i ) {
			if ( ( ( s_tick + id[i] ) & 63u ) != 0u ) continue;
			{
				const uint32_t h = ( id[i] * 2654435761u ) ^ ( ( seed + s_tick ) * 40503u );
				wired_crowd_goal_t g;
				g.x = px[i] + (float)( (int)( h & 0x7ffu ) - 1024 );
				g.y = py[i] + (float)( (int)( ( h >> 11 ) & 0x7ffu ) - 1024 );
				g.speed = 40.0f + (float)( ( h >> 22 ) & 0x7fu );
				wired_crowd_set_goal( s_crowd, i, g );
			}
		}
	}

	{
		const int64_t t0 = wired_workload_clock();
		wired_crowd_step( s_crowd, (float)frameMsec * 0.001f, viewers, viewerCount );
		wired_workload_end( WIRED_WL_SV_CROWD_STEP, t0, (uint64_t)count );
	}
	{
		const int64_t t0 = wired_workload_clock();
		wired_crowd_net_server_tick( s_netServer, ++s_tick, wired_crowd_count( s_crowd ), wired_crowd_user_id( s_crowd ),
			wired_crowd_pos_x( s_crowd ), wired_crowd_pos_y( s_crowd ), wired_crowd_pos_z( s_crowd ),
			wired_crowd_vel_x( s_crowd ), wired_crowd_vel_y( s_crowd ) );
		wired_workload_end( WIRED_WL_SV_CROWD_QUANTISE, t0, (uint64_t)count );
	}

	/* a client that left takes its baseline with it */
	for ( int i = 0; i < MAX_CLIENTS; ++i )
		if ( s_peers[i] && ( i >= sv_maxclients->integer || svs.clients[i].state != CS_ACTIVE ) ) SC_DestroyPeer( i );
}

/* ---- per client packet ---------------------------------------------------- */

typedef struct {
	int   clients[MAX_CLIENTS];
	float x[MAX_CLIENTS], y[MAX_CLIENTS];
	float radius;
	size_t budget;
} scBatch_t;

/* One client per chunk. Each chunk reads the shared per-tick records and writes
 * only its own peer and its own packet buffer, so the batch is independent of the
 * worker count. Peers are created before the batch (the zone allocator is not
 * thread-safe) and nothing in here logs or raises an error. */
static void SC_EncodeBody( void *p, uint32_t chunk, uint32_t begin, uint32_t end )
{
	const scBatch_t *b = (const scBatch_t *)p;
	(void)chunk;
	for ( uint32_t i = begin; i < end; ++i ) {
		const int c = b->clients[i];
		wired_crowd_net_encode_stats_t st;
		const size_t n = wired_crowd_net_encode( s_netServer, s_peers[c], b->x[i], b->y[i], b->radius,
			b->budget, s_packet[c], sizeof( s_packet[c] ), &st );
		s_packetLen[c] = ( n == (size_t)-1 ) ? 0u : n;
		s_packetAgents[c] = st.agentsSent;
	}
}

/* Called by the snapshot loop with the clients that will be sent a snapshot in
 * this pass: their crowd packets are encoded together, on the task scheduler. */
void SV_Crowd_PrepareClients( const int *clients, int count )
{
	static scBatch_t batch;
	int n = 0;
	uint64_t agents = 0;
	int64_t t0;
	if ( !s_crowd || !s_netServer || !wired_crowd_count( s_crowd ) ) return;
	batch.radius = sv_crowdRadius->value;
	batch.budget = (size_t)sv_crowdBudget->integer;
	for ( int k = 0; k < count; ++k ) {
		const int c = clients[k];
		const client_t *cl = &svs.clients[c];
		if ( c < 0 || c >= MAX_CLIENTS || cl->state != CS_ACTIVE || cl->netchan.remoteAddress.type == NA_BOT ) continue;
		if ( !s_peers[c] ) {
			wired_crowd_net_env_t nenv;
			nenv.alloc = SC_Alloc; nenv.release = SC_Release;
			s_peers[c] = wired_crowd_net_peer_create( &nenv, &s_netConfig );
			if ( !s_peers[c] ) continue;
		}
		batch.clients[n] = c;
		batch.x[n] = SV_GameClientNum( c )->origin[0];
		batch.y[n] = SV_GameClientNum( c )->origin[1];
		++n;
	}
	if ( !n ) return;
	t0 = wired_workload_clock();
	wired_tasks_parallel_for( WiredTasks_Get(), (uint32_t)n, 1u, SC_EncodeBody, &batch );
	for ( int i = 0; i < n; ++i ) { s_packetReady[batch.clients[i]] = qtrue; agents += s_packetAgents[batch.clients[i]]; }
	wired_workload_end( WIRED_WL_SV_CROWD_ENCODE, t0, agents );
}

void SV_Crowd_WriteToClient( client_t *client, msg_t *msg )
{
	const int num = (int)( client - svs.clients );
	const playerState_t *ps;
	uint8_t packet[SV_CROWD_PACKET_BYTES];
	wired_crowd_net_encode_stats_t st;
	size_t n;
	int64_t t0;
	if ( !s_crowd || !s_netServer || !wired_crowd_count( s_crowd ) ) return;
	if ( client->state != CS_ACTIVE || client->netchan.remoteAddress.type == NA_BOT ) return;
	if ( num < 0 || num >= MAX_CLIENTS ) return;
	if ( !s_peers[num] ) {
		wired_crowd_net_env_t nenv;
		nenv.alloc = SC_Alloc; nenv.release = SC_Release;
		s_peers[num] = wired_crowd_net_peer_create( &nenv, &s_netConfig );
		if ( !s_peers[num] ) return;
	}
	if ( s_packetReady[num] ) {
		/* assembled by the batch before the snapshot loop */
		s_packetReady[num] = qfalse;
		n = s_packetLen[num];
		if ( !n ) return;
		memcpy( packet, s_packet[num], n );
	} else {
		ps = SV_GameClientNum( num );
		t0 = wired_workload_clock();
		n = wired_crowd_net_encode( s_netServer, s_peers[num], ps->origin[0], ps->origin[1],
			sv_crowdRadius->value, (size_t)sv_crowdBudget->integer, packet, sizeof( packet ), &st );
		wired_workload_end( WIRED_WL_SV_CROWD_ENCODE, t0, (uint64_t)st.agentsSent );
		if ( !n || n == (size_t)-1 ) return;
	}
	/* The packet must fit beside the snapshot already written. The encoder has
	 * already advanced this client's delta chain, so a packet that is not sent
	 * would leave a gap: refresh the baseline instead of waiting for the client to
	 * notice. */
	if ( msg->cursize + (int)n + 4 >= msg->maxsize ) {
		wired_crowd_net_peer_resync( s_peers[num] );
		return;
	}
	MSG_WriteByte( msg, svc_crowd );
	MSG_WriteShort( msg, (int)n );
	MSG_WriteData( msg, packet, (int)n );
}

/* The client saw a gap in its crowd packets and asks for a full refresh. */
void SV_Crowd_Resync( client_t *client )
{
	const int num = (int)( client - svs.clients );
	if ( num >= 0 && num < MAX_CLIENTS && s_peers[num] ) wired_crowd_net_peer_resync( s_peers[num] );
}

/* ---- promotion ------------------------------------------------------------ */

/* How many more agents may be promoted now: the cvar, the table size, and the
 * entity slots that are really free (the 1024 limit is shared with players,
 * items, missiles and everything the game spawns). */
int SV_Crowd_PromotionBudget( void )
{
	int budget = sv_crowdMaxPromoted ? sv_crowdMaxPromoted->integer : 0;
	const int freeEntities = MAX_GENTITIES - sv.num_entities - SC_ENTITY_RESERVE;
	if ( budget > SC_MAX_PROMOTED ) budget = SC_MAX_PROMOTED;
	if ( budget > freeEntities + s_promotedCount ) budget = freeEntities + s_promotedCount;
	budget -= s_promotedCount;
	return budget > 0 ? budget : 0;
}

/* The nearest agents inside the promotion radius after the last step, nearest first. */
int SV_Crowd_Promotable( uint32_t *userIds, float (*positions)[3], int max )
{
	uint32_t n = 0;
	const uint32_t *list;
	int out = 0;
	if ( !s_crowd || max <= 0 ) return 0;
	list = wired_crowd_promotions( s_crowd, &n );
	for ( uint32_t k = 0; k < n && out < max; ++k ) {
		const uint32_t i = list[k];
		userIds[out] = wired_crowd_user_id( s_crowd )[i];
		positions[out][0] = wired_crowd_pos_x( s_crowd )[i];
		positions[out][1] = wired_crowd_pos_y( s_crowd )[i];
		positions[out][2] = wired_crowd_pos_z( s_crowd )[i];
		++out;
	}
	return out;
}

/* Take an agent out of the crowd for the game. Fails over budget or for an id that
 * is not in the crowd (so a double promotion cannot duplicate it). */
static qboolean SV_Crowd_Promote( uint32_t userId, wired_crowd_agent_t *out )
{
	wired_crowd_agent_t agent;
	if ( !s_crowd || SV_Crowd_PromotionBudget() <= 0 ) return qfalse;
	if ( !wired_crowd_take( s_crowd, userId, &agent ) ) return qfalse;
	s_promoted[s_promotedCount++] = agent;
	if ( out ) *out = agent;
	return qtrue;
}

/* Give a promoted agent back to the crowd (its entity went away). The state may
 * have been updated by the game: it returns where the entity was. */
static qboolean SV_Crowd_Demote( const wired_crowd_agent_t *state )
{
	int slot = -1;
	if ( !s_crowd || !state ) return qfalse;
	for ( int i = 0; i < s_promotedCount; ++i ) if ( s_promoted[i].userId == state->userId ) { slot = i; break; }
	if ( slot < 0 ) return qfalse;                     /* only what was promoted can come back */
	if ( wired_crowd_put( s_crowd, state ) == UINT32_MAX ) return qfalse;
	s_promoted[slot] = s_promoted[--s_promotedCount];
	return qtrue;
}

/* ---- the game's view ------------------------------------------------------ */

int SV_Crowd_GamePromotable( crowdAgentRef_t *out, int max )
{
	uint32_t ids[SC_MAX_PROMOTED];
	float pos[SC_MAX_PROMOTED][3];
	int n;
	if ( max > SC_MAX_PROMOTED ) max = SC_MAX_PROMOTED;
	n = SV_Crowd_Promotable( ids, pos, max );
	for ( int i = 0; i < n; ++i ) {
		out[i].userId = (int)ids[i];
		VectorCopy( pos[i], out[i].origin );
	}
	return n;
}

qboolean SV_Crowd_GamePromote( int userId, crowdAgentState_t *out )
{
	wired_crowd_agent_t a;
	if ( !SV_Crowd_Promote( (uint32_t)userId, &a ) ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->userId = (int)a.userId;
	out->origin[0] = a.position.x; out->origin[1] = a.position.y; out->origin[2] = a.position.z;
	out->velocity[0] = a.vx; out->velocity[1] = a.vy;
	out->goal[0] = a.goal.x; out->goal[1] = a.goal.y; out->goalSpeed = a.goal.speed;
	return qtrue;
}

qboolean SV_Crowd_GameDemote( const crowdAgentState_t *s )
{
	wired_crowd_agent_t a;
	memset( &a, 0, sizeof( a ) );
	a.userId = (uint32_t)s->userId;
	a.position.x = s->origin[0]; a.position.y = s->origin[1]; a.position.z = s->origin[2];
	a.vx = s->velocity[0]; a.vy = s->velocity[1];
	a.goal.x = s->goal[0]; a.goal.y = s->goal[1]; a.goal.speed = s->goalSpeed;
	return SV_Crowd_Demote( &a );
}

/* ---- console -------------------------------------------------------------- */

static void SV_CrowdPromote_f( void )
{
	uint32_t ids[SC_MAX_PROMOTED];
	float pos[SC_MAX_PROMOTED][3];
	int want = Cmd_Argc() > 1 ? atoi( Cmd_Argv( 1 ) ) : 1, got, taken = 0;
	if ( want > SC_MAX_PROMOTED ) want = SC_MAX_PROMOTED;
	got = SV_Crowd_Promotable( ids, pos, want );
	for ( int i = 0; i < got; ++i ) {
		if ( !SV_Crowd_Promote( ids[i], NULL ) ) break;
		++taken;
		Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: promoted agent %u at (%.0f %.0f %.0f)\n",
			ids[i], pos[i][0], pos[i][1], pos[i][2] );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: promoted %d of %d candidates (%d promoted in all, %d more allowed)\n",
		taken, got, s_promotedCount, SV_Crowd_PromotionBudget() );
}

static void SV_CrowdDemote_f( void )
{
	const int count = s_promotedCount;
	int back = 0;
	for ( int i = count - 1; i >= 0; --i ) if ( SV_Crowd_Demote( &s_promoted[i] ) ) ++back;
	Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: demoted %d of %d promoted agents\n", back, count );
}

static void SV_CrowdSpawn_f( void )
{
	const int want = Cmd_Argc() > 1 ? atoi( Cmd_Argv( 1 ) ) : 0;
	const float radius = Cmd_Argc() > 2 ? (float)atof( Cmd_Argv( 2 ) ) : 2000.0f;
	vec3_t center = { 0, 0, 0 };
	int added = 0;
	if ( want <= 0 ) { Com_Log( SEV_INFO, LOG_CH(ch_server), "usage: crowd_spawn <count> [radius]\n" ); return; }
	if ( !SC_Ensure() ) { Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: no running game\n" ); return; }
	for ( int i = 0; i < sv_maxclients->integer; ++i ) {
		if ( svs.clients[i].state == CS_ACTIVE && svs.clients[i].netchan.remoteAddress.type != NA_BOT ) {
			VectorCopy( SV_GameClientNum( i )->origin, center );
			break;
		}
	}
	/* Scatter only onto ground an agent can stand on; bounded attempts, so a small
	 * or mostly-void map cannot spin here. */
	for ( int attempts = 0; added < want && attempts < want * 16 + 64; ++attempts ) {
		wired_crowd_vec3_t p;
		float z;
		p.x = center[0] + SC_RandF( -radius, radius );
		p.y = center[1] + SC_RandF( -radius, radius );
		z = SC_Ground( NULL, p.x, p.y, center[2] );
		if ( z != z ) continue;
		p.z = z;
		if ( wired_crowd_add( s_crowd, p, s_nextUserId ) == UINT32_MAX ) break;
		++s_nextUserId;
		++added;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: spawned %d agents (%u total)\n", added, wired_crowd_count( s_crowd ) );
}

static void SV_CrowdClear_f( void )
{
	SV_Crowd_Clear();
	Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: cleared\n" );
}

static void SV_CrowdStats_f( void )
{
	wired_crowd_report_t r;
	if ( !s_crowd ) { Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: empty\n" ); return; }
	wired_crowd_report( s_crowd, &r );
	Com_Log( SEV_INFO, LOG_CH(ch_server), "crowd: %u agents  full %u  half %u  far %u  dormant %u  stepped %u  promotable %u  hash %016llx\n",
		wired_crowd_count( s_crowd ), r.tierCount[0], r.tierCount[1], r.tierCount[2], r.tierCount[3],
		r.simulated, r.promotionCount, (unsigned long long)r.stateHash );
}

void SV_Crowd_Init( void )
{
	static qboolean done;
	if ( done ) return;
	done = qtrue;
	{
		static const cvarDesc_t b = CVAR_INT( "sv_crowdBudget", "2048", CVAR_ARCHIVE,
			"Bytes of crowd channel one client packet may carry.", 256, 3500 );
		static const cvarDesc_t r = CVAR_FLOAT( "sv_crowdRadius", "4096", CVAR_ARCHIVE,
			"Interest radius of the crowd channel, in world units.", 256.0f, 16384.0f );
		static const cvarDesc_t a = CVAR_INT( "sv_crowdAuto", "0", 0,
			"Spawn this many crowd agents once a human is in the game (0 = off). A test "
			"and benchmark aid so a scripted run can load a crowd.", 0, 65536 );
		static const cvarDesc_t m = CVAR_INT( "sv_crowdMaxPromoted", "0", 0,
			"Most crowd agents that may be promoted to real entities at once (0 = promotion "
			"off). Also limited by the entity slots that are free.", 0, 128 );
		sv_crowdAuto = Cvar_Register( &a );
		sv_crowdMaxPromoted = Cvar_Register( &m );
		sv_crowdBudget = Cvar_Register( &b );
		sv_crowdRadius = Cvar_Register( &r );
	}
	Cmd_AddCommand( "crowd_spawn", SV_CrowdSpawn_f );
	Cmd_AddCommand( "crowd_clear", SV_CrowdClear_f );
	Cmd_AddCommand( "crowd_stats", SV_CrowdStats_f );
	Cmd_AddCommand( "crowd_promote", SV_CrowdPromote_f );
	Cmd_AddCommand( "crowd_demote", SV_CrowdDemote_f );
}
