// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_crowd.c -- the client end of the crowd channel (capacity roadmap Phase 4).

Applies the WiredCrowdNet packets that ride next to the snapshot, keeps a
two-state buffer per agent, and each frame adds the nearest agents to the scene
as simple billboards. The billboards are the engine's own debug proxies: a game
module that wants characters reads the same samples and draws its own.

A packet that does not follow the last one (a gap) is dropped and the client asks
the server for a resync; the server answers with an absolute refresh and the
chain resumes. Nothing about the normal snapshot, entity or prediction path
changes.
===========================================================================
*/

#include "client.h"
#include "../qcommon/wired/crowd/wired_crowd_net.h"
#include "../qcommon/wired/workload_stats.h"

LOG_DECLARE_CHANNEL( ch_client, "client" );

#define CL_CROWD_VIEW_CAPACITY WIRED_CROWD_NET_VIEW_CAPACITY
#define CL_CROWD_MAX_DRAWN     4000     /* the GPU sprite pool holds 4096 headers a frame, shared with the game */
#define CL_CROWD_MIN_DISTANCE  64.0f     /* nearer than this an agent would fill the view; the game draws characters there */

static cvar_t *cl_crowdDraw;       // how many agents may become scene entities
static cvar_t *cl_crowdShow;       // master switch for the debug proxies

static void *CC_Alloc( size_t n ) { return Z_Malloc( n ); }
static void  CC_Release( void *p ) { Z_Free( p ); }

void CL_Crowd_Init( void )
{
	static const cvarDesc_t draw = CVAR_INT( "cl_crowdDraw", "256", CVAR_ARCHIVE,
		"Most crowd agents drawn as debug proxies (GPU sprite pool, shared with the "
		"game's own sprites).", 0, CL_CROWD_MAX_DRAWN );
	static const cvarDesc_t show = CVAR_BOOL( "cl_crowdShow", "1", CVAR_ARCHIVE,
		"Draw crowd agents as debug billboards." );
	cl_crowdDraw = Cvar_Register( &draw );
	cl_crowdShow = Cvar_Register( &show );
}

void CL_Crowd_Free( clientApp_t *app )
{
	if ( !app ) return;
	wired_crowd_net_view_destroy( app->cl.crowdView );
	app->cl.crowdView = NULL;
	app->cl.crowdTick = 0u;
	app->cl.crowdTickTime = 0;
	app->cl.crowdPackets = app->cl.crowdBytes = app->cl.crowdGaps = app->cl.crowdMalformed = 0u;
	app->cl.crowdResyncPending = qfalse;
}

void CL_Crowd_Parse( clientApp_t *app, msg_t *msg )
{
	uint8_t packet[4096];
	const int length = MSG_ReadShort( msg );
	uint32_t tick = 0;
	int result;
	if ( length <= 0 || length > (int)sizeof( packet ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad crowd packet size %d", __func__, length );
		return;
	}
	/* The message is bit-packed, so the byte count read so far says nothing about
	 * how many of the payload bytes remain; a read past the end is what shows it. */
	MSG_ReadData( msg, packet, length );
	if ( msg->readcount > msg->cursize ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: crowd packet runs past the message (%d bytes)", __func__, length );
		return;
	}
	if ( !app->cl.crowdView ) {
		wired_crowd_net_env_t env;
		wired_crowd_net_config_t cfg;
		env.alloc = CC_Alloc; env.release = CC_Release;
		memset( &cfg, 0, sizeof( cfg ) );
		cfg.capacity = CL_CROWD_VIEW_CAPACITY;
		/* Everything else is learned from the server's first (resync) packet. */
		cfg.maxSpeed = 1.0f; cfg.tickMsec = 1.0f;
		for ( int a = 0; a < 3; ++a ) { cfg.worldMin[a] = -1.0f; cfg.worldMax[a] = 1.0f; }
		app->cl.crowdView = wired_crowd_net_view_create( &env, &cfg );
		if ( !app->cl.crowdView ) return;
	}
	result = wired_crowd_net_decode( app->cl.crowdView, packet, (size_t)length, &tick );
	if ( result == WIRED_CROWD_NET_OK ) {
		app->cl.crowdResyncPending = qfalse;
		app->cl.crowdTick = tick;
		app->cl.crowdTickTime = app->cl.serverTime;
		++app->cl.crowdPackets;
		app->cl.crowdBytes += (uint32_t)length;
		/* one bounded row per 200 packets, never per packet */
		if ( app->cl.crowdPackets % 200u == 0u ) {
			Com_Log( SEV_INFO, LOG_CH( ch_client ),
				"crowd client v1: packets=%u bytes=%u gaps=%u malformed=%u last_tick=%u last_packet_bytes=%d tracked=%u\n",
				app->cl.crowdPackets, app->cl.crowdBytes, app->cl.crowdGaps, app->cl.crowdMalformed, tick, length,
				(unsigned)wired_crowd_net_view_count( app->cl.crowdView ) );
		}
		return;
	}
	if ( result == WIRED_CROWD_NET_GAP ) ++app->cl.crowdGaps; else ++app->cl.crowdMalformed;
	/* Packets already in flight when the gap was seen are rejected too; one request
	 * is enough until the server's refresh arrives. It is asked again only if two
	 * seconds pass without an answer, so a lost request cannot strand the view. */
	if ( app->cl.crowdResyncPending && app->cl.serverTime - app->cl.crowdResyncTime < 2000 ) return;
	app->cl.crowdResyncPending = qtrue;
	app->cl.crowdResyncTime = app->cl.serverTime;
	/* A gap, or a packet from before this client had the box: ask for a refresh.
	 * Anything else that does not decode is dropped the same way; the server's
	 * answer rebuilds the state from scratch. */
	Com_Log( SEV_DEBUG, LOG_CH( ch_client ), "crowd: packet rejected (%s), requesting resync\n",
		result == WIRED_CROWD_NET_GAP ? "gap" : "malformed" );
	CL_AddReliableCommand( app, "crowdresync", qfalse );
}

static void CL_Crowd_AddToSceneImpl( clientApp_t *app, const refdef_t *fd, int *drawnOut );

/* The cost of the crowd on the render frame: sampling, ordering and the scene
 * entities it adds. Measured as one workload so a larger cl_crowdDraw shows up as
 * a number, not a feeling. */
void CL_Crowd_AddToScene( clientApp_t *app, const refdef_t *fd )
{
	int drawn = 0;
	const int64_t t0 = wired_workload_clock();
	CL_Crowd_AddToSceneImpl( app, fd, &drawn );
	wired_workload_end( WIRED_WL_CL_CROWD_SCENE, t0, (uint64_t)drawn );
}

static void CL_Crowd_AddToSceneImpl( clientApp_t *app, const refdef_t *fd, int *drawnOut )
{
	static wired_crowd_net_sample_t samples[CL_CROWD_VIEW_CAPACITY];
	const wired_crowd_net_view_t *view = app->cl.crowdView;   /* read-only here */
	float tickMsec, renderTick;
	size_t n;
	int drawn = 0, limit;
	if ( !view || !cl_crowdShow || !cl_crowdShow->integer || !cl_crowdDraw ) return;
	limit = cl_crowdDraw->integer;
	if ( limit <= 0 ) return;
	tickMsec = wired_crowd_net_view_tick_msec( view );
	if ( !( tickMsec > 0.0f ) ) return;
	/* The server's tick now, estimated from the clock, drawn two ticks behind so
	 * the interpolation has two real states to blend. */
	renderTick = (float)app->cl.crowdTick
		+ (float)( app->cl.serverTime - app->cl.crowdTickTime ) / tickMsec - 2.0f;
	n = wired_crowd_net_view_sample_nearest( app->cl.crowdView, renderTick, fd->vieworg[0], fd->vieworg[1], fd->vieworg[2],
		samples, (size_t)limit + 64u );   /* a few spare: the ones inside the minimum distance are skipped */
	if ( !re.AddSpriteToScene ) return;       /* a renderer without the GPU sprite pool draws no proxies */
	for ( size_t k = 0; k < n && drawn < limit; ++k ) {
		const wired_crowd_net_sample_t *s = &samples[k];
		spriteDesc_t sprite;
		{
			const float dx = s->x - fd->vieworg[0], dy = s->y - fd->vieworg[1], dz = s->z - fd->vieworg[2];
			if ( dx * dx + dy * dy + dz * dz < CL_CROWD_MIN_DISTANCE * CL_CROWD_MIN_DISTANCE ) continue;
		}
		/* One 48-byte header in the per-frame sprite buffer, drawn instanced on the
		 * GPU: no scene entity, no per-agent CPU draw state. Shader 0 is the pool's
		 * default sprite. */
		memset( &sprite, 0, sizeof( sprite ) );
		VectorSet( sprite.origin, s->x, s->y, s->z + 28.0f );
		sprite.radius = 10.0f;
		sprite.rgba[0] = 0.35f + 0.65f * ( s->speed > 0.0f ? 1.0f : 0.0f );
		sprite.rgba[1] = 0.8f - 0.5f * ( (float)drawn / (float)limit );
		sprite.rgba[2] = 0.3f;
		sprite.rgba[3] = 1.0f;
		re.AddSpriteToScene( &sprite );
		++drawn;
	}
	*drawnOut = drawn;
}
