// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
WiredCrowdNet -- the crowd channel (capacity roadmap doc-1, Phases 4 and 5).

Crowd agents cannot travel as entityState_t: the protocol carries at most 1024
entities and 512 per snapshot (256 on the client), and a crowd is far larger. They
get their own interest-managed channel that rides next to the normal snapshot and
leaves it, and client prediction, untouched.

What the channel does
  * Quantisation. A position is three 16-bit values over the world box, a heading
    one byte, a speed one byte. A record is 8 bytes absolute or 5 bytes when sent
    as a delta against what that client last received.
  * Encode once, share. The server quantises every agent ONCE per tick into a
    record cache. Each client's packet is assembled from that cache, so the cost of
    N clients is N packet assemblies, not N full encodings (Phase 5).
  * Per-client interest management. An agent is sent to a client only inside that
    client's radius, nearest first, at a rate that falls with distance (every
    tick, every 2nd, every 8th), under a byte budget. Over budget, the farthest
    agents wait for the next packet; nothing is dropped silently and nothing grows.
  * Deltas against a per-client baseline: what that client was last sent for the
    agent. The packets of one client form a chain (a sequence number in the
    header); a client that sees a gap rejects the packet and asks for a resync, and
    the server then sends every record absolute. A lost packet therefore costs a
    full refresh, never a wrong position. Over a reliable ordered stream there is
    no gap and the chain just works.
  * Client side: a snapshot buffer per agent and interpolation between the last
    two received states, so a client at 10 Hz still draws smooth motion.

Everything is deterministic and allocation-free after create. The module links
against nothing of the engine.
===========================================================================
*/

#ifndef WIRED_CROWD_NET_H
#define WIRED_CROWD_NET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void *( *alloc )( size_t bytes );
	void  ( *release )( void *memory );
} wired_crowd_net_env_t;

/* How many agents one client's view can track. The server never lets a peer hold
 * more than this: a client whose interest set is larger keeps its nearest agents
 * and the rest wait outside, so a crowd bigger than a client can hold degrades to
 * "nearest first" instead of to a rejected packet. */
#define WIRED_CROWD_NET_VIEW_CAPACITY 16384u

typedef struct {
	uint32_t capacity;                 /* maximum number of agents on the server    */
	uint32_t viewCapacity;             /* what one client can track; 0 = WIRED_CROWD_NET_VIEW_CAPACITY */
	float    worldMin[3], worldMax[3]; /* quantisation box                          */
	float    maxSpeed;                 /* speed byte range                          */
	float    tickMsec;                 /* milliseconds per server tick; clients time interpolation with it */
	uint32_t rateEvery[3];             /* send every Nth tick for near/mid/far band */
	float    bandRadius[3];            /* distance bounds of the three bands        */
} wired_crowd_net_config_t;

typedef struct wired_crowd_net_server wired_crowd_net_server_t;
typedef struct wired_crowd_net_peer   wired_crowd_net_peer_t;     /* one client's baseline */
typedef struct wired_crowd_net_view   wired_crowd_net_view_t;     /* client-side buffers   */

/* ---- server ---- */

wired_crowd_net_server_t *wired_crowd_net_server_create( const wired_crowd_net_env_t *env, const wired_crowd_net_config_t *config );
void                      wired_crowd_net_server_destroy( wired_crowd_net_server_t *server );

/* Quantise the population once for this tick. Arrays are the crowd's SoA state;
 * userId is the agent's stable identity (it is what travels on the wire). */
void wired_crowd_net_server_tick( wired_crowd_net_server_t *server, uint32_t serverTick, uint32_t count,
		const uint32_t *userId, const float *x, const float *y, const float *z,
		const float *vx, const float *vy );

wired_crowd_net_peer_t *wired_crowd_net_peer_create( const wired_crowd_net_env_t *env, const wired_crowd_net_config_t *config );
void                    wired_crowd_net_peer_destroy( wired_crowd_net_peer_t *peer );

/* The client asked for a resync (it saw a gap, or it just connected): forget the
 * baseline, so the next packet carries every agent in range as an absolute
 * record. */
void wired_crowd_net_peer_resync( wired_crowd_net_peer_t *peer );

typedef struct {
	uint32_t agentsConsidered;   /* inside the radius                         */
	uint32_t agentsSent;
	uint32_t agentsDeferred;     /* due but over budget                       */
	uint32_t absoluteRecords;
	uint32_t deltaRecords;
	uint32_t removals;
	uint32_t bytes;
} wired_crowd_net_encode_stats_t;

/* Assemble one packet for a viewer. Returns the packet size in bytes, 0 when
 * nothing is due, or SIZE_MAX if `capacity` is too small for even the header. */
size_t wired_crowd_net_encode( wired_crowd_net_server_t *server, wired_crowd_net_peer_t *peer,
		float viewerX, float viewerY, float radius, size_t budgetBytes,
		uint8_t *out, size_t capacity, wired_crowd_net_encode_stats_t *stats );

/* ---- client ---- */

wired_crowd_net_view_t *wired_crowd_net_view_create( const wired_crowd_net_env_t *env, const wired_crowd_net_config_t *config );
void                    wired_crowd_net_view_destroy( wired_crowd_net_view_t *view );

/* Apply one packet. A resync packet is self-describing: it carries the
 * quantisation box and speed range, replacing whatever the view was created with
 * (the creation config only has to supply the capacity). Returns WIRED_CROWD_NET_OK, WIRED_CROWD_NET_GAP when the
 * sequence number does not follow the last applied packet (the caller asks the
 * server for a resync; the view is unchanged), or WIRED_CROWD_NET_MALFORMED. A
 * rejected packet never changes the view. */
#define WIRED_CROWD_NET_OK        0
#define WIRED_CROWD_NET_GAP       1
#define WIRED_CROWD_NET_MALFORMED 2
int wired_crowd_net_decode( wired_crowd_net_view_t *view, const uint8_t *data, size_t length, uint32_t *serverTick );

typedef struct {
	uint32_t userId;
	float x, y, z, heading, speed;
} wired_crowd_net_sample_t;

/* Milliseconds per server tick, as announced by the server's resync packet (0
 * until the first one arrives). */
float wired_crowd_net_view_tick_msec( const wired_crowd_net_view_t *view );

/* Agents the view currently tracks. */
uint32_t wired_crowd_net_view_count( const wired_crowd_net_view_t *view );

/* Milliseconds per server tick, as announced by the server's resync packet (the
 * creation value until the first one arrives). */
float wired_crowd_net_view_tick_msec( const wired_crowd_net_view_t *view );

/* The nearest `maxOut` agents to (cx, cy, cz), interpolated at renderTick, nearest
 * first at 8-unit resolution. Cost is two passes over the tracked agents (counting
 * sort by distance) plus interpolation of the chosen ones only: what a renderer
 * that can draw a few hundred agents wants, instead of sampling and sorting all of
 * them every frame. */
size_t wired_crowd_net_view_sample_nearest( wired_crowd_net_view_t *view, float renderTick,
		float cx, float cy, float cz, wired_crowd_net_sample_t *out, size_t maxOut );

/* Interpolated agents at `renderTick` (fractional server ticks). Writes up to
 * maxOut samples and returns the count. */
size_t wired_crowd_net_view_sample( const wired_crowd_net_view_t *view, float renderTick,
		wired_crowd_net_sample_t *out, size_t maxOut );

#ifdef __cplusplus
}
#endif

#endif
