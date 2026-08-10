// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wn_public.h — QUIC transport public API

All functions are called from the engine (sv_main.c, net_ip.c, sv_game.c).

Channel architecture:
  QUIC Connection (single port, single UDP socket)
  │
  ├── Stream 0x00 ── Session control (binary TLV, bidi)
  │                   Client→Server: TLV 0x01 CONNECT {userinfo, qport}
  │                   Server→Client: TLV 0x02 ACCEPT | TLV 0x03 REFUSE
  │                   Client→Server: TLV 0x05 READY (gamestate processed)
  │
  ├── Datagrams ──── Snapshot datagrams (unreliable, server→client)  [WN_GAME]
  │                   Format: [wn_sequence:u32][delta_base:u32][snapshot_data...]
  ├── Datagrams ──── Usercmd datagrams (unreliable, client→server)   [WN_GAME]
  │                   Format: [client_tick:u32][snapshot_ack:u32][cmd_count:u8][cmds...]
  │
  ├── Stream 0x03 ── Game Events (reliable, msgpack, server→client)  [WN_GAME]
  ├── Stream 0x04+ ─ Game reliable channels (server↔client)          [WN_GAME]
  ├── Stream 0x08+ ─ MCP (reliable, json-rpc 2.0, bidi)             [WN_CONTROL]
  └── Stream: HTTP ─ /health, /metrics (request-response)           [WN_HTTP]
===========================================================================
*/
#ifndef WN_PUBLIC_H
#define WN_PUBLIC_H

#include "../../q_feats.h"

#include "../../q_shared.h"
#include "../../qcommon.h"
#include "../../net_transport.h"

/*
 * Permission model — 3 orthogonal dimensions, each a single bit.
 *
 *   Connection Type:  OBSERVER (0) | PLAYER (1)
 *   Team Role:        MEMBER   (0) | LEADER (1)
 *   Authority:        USER     (0) | ADMIN  (1)
 *
 * Encoded as a uint8_t bitmask:
 *   bit 0 = connection type   (0=OBSERVER, 1=PLAYER)
 *   bit 1 = team role         (0=MEMBER,   1=LEADER)
 *   bit 2 = authority         (0=USER,     1=ADMIN)
 */
#define WN_PERM_PLAYER     (1 << 0)   // connection type: game client
#define WN_PERM_LEADER     (1 << 1)   // team role: coaching / team commands
#define WN_PERM_ADMIN      (1 << 2)   // authority: kick, map change

// Common presets
#define WN_PERM_OBSERVER_MEMBER_USER   0                                          // 0b000
#define WN_PERM_OBSERVER_LEADER_ADMIN  (WN_PERM_LEADER | WN_PERM_ADMIN)          // 0b110
#define WN_PERM_PLAYER_MEMBER_USER     (WN_PERM_PLAYER)                           // 0b001
#define WN_PERM_PLAYER_LEADER_ADMIN    (WN_PERM_PLAYER | WN_PERM_LEADER | WN_PERM_ADMIN) // 0b111

static inline qboolean WN_HasPermPlayer( uint8_t perm )  { return (perm & WN_PERM_PLAYER) != 0; }
static inline qboolean WN_HasPermLeader( uint8_t perm )  { return (perm & WN_PERM_LEADER) != 0; }
static inline qboolean WN_HasPermAdmin( uint8_t perm )   { return (perm & WN_PERM_ADMIN)  != 0; }

// ───────────────────────────────────────────────────────────────────
// Lifecycle — called from sv_init.c / sv_main.c
// ───────────────────────────────────────────────────────────────────

void        WN_Init( void );
void        WN_Shutdown( void );
void        WN_RegisterCommands( void );

// ───────────────────────────────────────────────────────────────────
// ───────────────────────────────────────────────────────────────────
// Frame processing — called from SV_Frame() and after recv drain
// ───────────────────────────────────────────────────────────────────

// Vtable-internal (Batch 3 2026-05-23). Engine code routes through
// transport->frame / transport->flush_outbound. Direct WN_* call retained
// for the wn_transport.c wn_frame / wn_flush_outbound impl wrappers only.
void        WN_ProcessTimers( void );       // retransmit, idle timeout, keepalive
void        WN_FlushOutbound( void );       // pull packets from picoquic → NET_SendPacket

#if FEAT_WIREDNET_OBSERVER
// Vtable-internal (Batch 3). Called by wn_frame; not by app layer.
void        WN_SendDatagrams( void );       // push game state datagrams
void        WN_PushEvents( void );          // push buffered game events on event stream
void        WN_TcpFrame( void );            // accept + handle TCP HTTP/1.1 requests
#endif

#if FEAT_WIREDNET_CONTROL
// Vtable-internal (Batch 3). Called by wn_frame; not by app layer.
void        WN_ProcessCommandQueue( void ); // process pending MCP/command requests
#endif

// ───────────────────────────────────────────────────────────────────
// Event emission — called from sv_game.c syscall handler
// Game code (g_combat.c etc.) calls these via VM syscall traps.
// ───────────────────────────────────────────────────────────────────

#if FEAT_WIREDNET_OBSERVER
void        WN_EmitKill( int attacker, int victim, int mod,
                           const vec3_t attacker_pos, const vec3_t victim_pos );
void        WN_EmitDamage( int attacker, int victim, int damage, int mod,
                             const vec3_t attacker_pos, const vec3_t victim_pos );
void        WN_EmitItemPickup( int client, const char *item, const vec3_t pos );
void        WN_EmitChat( int client, const char *msg, qboolean teamOnly );
void        WN_EmitMatchEvent( const char *type, const char *data );
void        WN_EmitBotEvent( int bot_id, const char *event_type,
                               int param1, int param2, const vec3_t pos );
#endif

// Sys_Microseconds() is declared in qcommon.h (int64_t) — no redeclaration needed.
// picoquic expects uint64_t; cast at call site.

/* Maximum QUIC datagram payload (snapshot + 8-byte tick header must fit within this).
 * = PICOQUIC_ENFORCED_INITIAL_MTU(1200) - 32 bytes QUIC header overhead.
 * Server enforces this at send time; client recv buffer is sized to match. */
#define WN_DATAGRAM_MTU   1168

/* Maximum payload in a single fragment datagram (wire format v2 — Task 7.5).
 * = WN_DATAGRAM_MTU - 11
 *   (4-byte wn_seq + 4-byte delta_base + 1-byte flags + 1-byte frag_total + 1-byte frag_index).
 * Max snapshot via 8 fragments: 8 * WN_FRAG_PAYLOAD = 9256 bytes. */
#define WN_FRAG_PAYLOAD   (WN_DATAGRAM_MTU - 11)

// ───────────────────────────────────────────────────────────────────
// QUIC game transport — called from net_ip.c / Sys_SendPacket
// ───────────────────────────────────────────────────────────────────

// Send a netchan-format packet over a QUIC game connection.
// Called from Sys_SendPacket when to->type is NA_QUIC or NA_QUIC6.
void        WN_SendGamePacketToAddr( const netadr_t *to, const void *data, int length );

// Look up a conn_handle_t for an address.  Returns CONN_INVALID when no
// active game connection exists for that address.  Type-agnostic (NA_QUIC
// and NA_IP both match the underlying IP bytes).
// Vtable-internal (Batch 3). App layer uses transport->lookup_by_addr.
conn_handle_t WN_GetConnHandleByAddr( const netadr_t *addr );

// Fill *out with the network address for a conn_handle. Returns qfalse for CONN_INVALID.
qboolean      WN_GetAddrByConnHandle( conn_handle_t conn, netadr_t *out );

/* Sentinel conn_handle for the single outgoing client connection.
 * Ordering: CONN_INVALID(0) < server slots(1..8) < CONN_CLIENT_QUIC(9).
 * Value must equal WN_MAX_CLIENTS+1 from wn_local.h. */
#define CONN_CLIENT_QUIC  ((conn_handle_t)9)

/* In-memory same-process local-app handle range (in-process-queue). A local app
 * i connects as WN_APP_CONN_BASE + i. Chosen high + disjoint from CONN_CLIENT_QUIC
 * and the 1..8 game-conn slots. Promoted to this public header so the transport
 * selector (transport_for_handle, below) is visible to all conn-bearing call
 * sites. WN_APP_CONN_COUNT mirrors wn_local.h's WN_MAX_LOCAL_CLIENTS (a
 * _Static_assert in wn_transport.c pins them equal). */
#define WN_APP_CONN_BASE   ((conn_handle_t)100)
#define WN_APP_CONN_COUNT  4

/* Server-side handle range for the SAME in-process apps. An in-memory connection
 * has two ends, exactly like a QUIC loopback connection has two distinct handles:
 *   client end  = WN_APP_CONN_BASE   + slot  (100..103) — wtcl_array[slot],
 *                 decoded by wn_get_client_state / WN_AppSlotForConn.
 *   server end  = WN_APP_SVCONN_BASE + slot  (110..113) — game_conns[].pub_handle,
 *                 stored in svs.clients[].quic_conn.
 * Both ranges route to inmem_transport (transport_for_handle). The split lets the
 * in-memory send path infer DIRECTION from the handle range (server end → push to
 * the client recv ring; client end → push to the server recv ring), mirroring how
 * QUIC's wn_get_cnx distinguishes the client cnx (handle 9/100+) from a server
 * game-conn cnx (1..8). Without the split, handle 100 would mean "client end" to
 * wn_get_client_state yet "server game-conn" to pub_handle — a direction collision. */
#define WN_APP_SVCONN_BASE ((conn_handle_t)110)
#define WN_APP_SVCONN_COUNT 4

/* The two transport backends. quic_transport is the reference (picoquic/UDP);
 * inmem_transport is the same-process buffer-pass backend (in-process-queue).
 * `transport` (net_transport.h) still points at quic_transport for the global
 * single-backend ops (lifecycle, recv pump, connecting client). */
extern transport_t quic_transport;
extern transport_t inmem_transport;

/* Per-conn transport SELECTION (in-process-queue routing). A WN_APP_CONN_BASE+i
 * handle is an in-memory same-process app; everything else (CONN_CLIENT_QUIC,
 * game-conns 1..WN_MAX_CLIENTS) is QUIC. At N=1 NO producer emits a 100+ handle
 * (that lands in B2), so this always returns &quic_transport — byte-identical to
 * the bare `transport` global. Use for conn-FIRST-arg ops (send/disconnect/
 * drop_client/metrics); recv pull-iterators and conn-less client ops stay on the
 * `transport` global. */
static ID_INLINE transport_t *transport_for_handle( conn_handle_t conn ) {
	if ( conn >= WN_APP_CONN_BASE && conn < WN_APP_CONN_BASE + WN_APP_CONN_COUNT )
		return &inmem_transport;           /* client end of an in-process app */
	if ( conn >= WN_APP_SVCONN_BASE && conn < WN_APP_SVCONN_BASE + WN_APP_SVCONN_COUNT )
		return &inmem_transport;           /* server end of an in-process app */
	return &quic_transport;
}

/* Map a client conn_handle to its per-app local-client slot index (in-process-
 * queue). CONN_CLIENT_QUIC and the WN_APP_CONN_BASE range both decode here; any
 * other handle returns 0 (primary). Lets the client recv/parse tier route a
 * datagram to its owning clientApps[i] without knowing the transport's handle
 * scheme. At N=1 the only client handle in flight is CONN_CLIENT_QUIC -> 0. */
int           WN_AppSlotForConn( conn_handle_t conn );

// In-memory transport (in-process-queue): connect an in-process app. Wires both
// the client end (wtcl_array[app_slot]) and the server end (a game_conns[] slot
// + a pending admission), returning the public handle WN_APP_CONN_BASE+app_slot
// (or CONN_INVALID). The first producer of a 100+ handle; called from
// wn_inmem_connect. SV_OnPlayerConnect admits it transport-agnostically.
conn_handle_t WN_ConnectApp( int app_slot, const char *userinfo );

// Locate the server-side game_conn whose pub_handle matches a 100+ app handle
// (in-memory transport send/drop paths). NULL if no active match.
struct wn_game_conn_s *WN_GameConnByAppHandle( conn_handle_t conn );

// True if the active client connection is in-process (in-memory backend, no
// picoquic). The in-mem host produces no UDP traffic, so select() never wakes
// NET_Event to drive the client recv consumers — NET_Sleep pumps them every
// frame when this is true. A QUIC client returns qfalse (byte-identical).
qboolean WN_HasInmemClient( void );

// Drain (queues only) the in-process app's client + server rings on a
// localReconnect map→map transition — discards stale prev-map snapshot/usercmd/
// reliable remnants while keeping the conn live. Caller gates on WN_HasInmemClient
// (no-op / never called for a QUIC host → byte-identical).
void WN_ResetInmemClientRings( int app_slot );

// Dequeue one user-command datagram from any active server game connection.
// Called directly by SV_DrainQUICUsercmds — bypasses transport vtable so the
// client's recv_unreliable path (snapshots) is never contaminated with user cmds.
qboolean      WN_ServerRecvUsercmd( conn_handle_t *conn_out, byte *buf, int *len_out );

// Dequeue one reliable message sent from the client to the server.
// Semantic channel is returned via *channel_out.
// Direct call (Batch 3 deviation from Q5 ratification): the unified
// transport->recv_reliable shim would mix server-side cli→srv and
// client-side srv→cli queues in listen-server mode. Splitting the vtable
// into recv_reliable_client / recv_reliable_server is a future cleanup.
qboolean      WN_ServerRecvReliable( conn_handle_t *conn_out, int *channel_out,
	byte *buf, int *len_out );

// Drain pending game-client connects on the main thread, calling transport->accept_callback.
void          WN_DrainPendingConnects( void );

// Re-enqueue a connection that could not be admitted yet (server mid-spawn).
// Called from SV_OnPlayerConnect when svs.spawn.phase != SPAWN_IDLE.
void          WN_RequeueConnect( conn_handle_t conn, const char *userinfo );

// Drain pending game-client ready events (TLV 0x05), calling transport->ready_callback.
void          WN_DrainPendingReady( void );

// Free in-process game_conns marked for deferred teardown (in-process-queue B4).
// Call AFTER SV_DrainQUICReliableCommands so a disconnecting in-mem host's in-band
// "disconnect" command is processed before its game_conn is released. No-op for QUIC.
void          WN_DrainPendingFrees( void );

#ifndef HEADLESS
// Send TLV 0x05 READY on the session stream to the connected server, on the
// given in-process client's own handle. The server transitions that client
// CS_PRIMED -> CS_ACTIVE on receipt (transport-only; no cgame involved).
void          WN_ClientSendReady( int app_slot );

// Dequeue one reliable message sent from the server to the client.
// Semantic channel is returned via *channel_out. app_slot selects which
// in-process client's srv->cli ring to drain (wtcl_array[app_slot]); the call is
// kept separate from the server-side cli->srv drain so the two directions never
// cross in listen-server mode.
qboolean      WN_ClientRecvReliable( int app_slot, int *channel_out, byte *buf, int *len_out );

// Consume the pending CHAN_BOOTSTRAP message (configstrings + baselines) for the
// given in-process client slot. *data_out points into an internal buffer valid
// until the next connect on that slot. Returns qfalse if no bootstrap is pending.
qboolean      WN_ClientConsumeBootstrap( int app_slot, const byte **data_out, int *len_out );
#endif

#ifndef HEADLESS
// ───────────────────────────────────────────────────────────────────
// QUIC game client API — called from cl_main.c / NET_Event
// ───────────────────────────────────────────────────────────────────

// Initiate QUIC connection to game server (async; WN_ClientCallback drives it).
void        WN_ClientConnect( const netadr_t *serverAddr, const char *userinfo, int qport );

// Pump client QUIC timers + flush outbound.  Call from NET_Event every frame.
void        WN_ClientFrame( void );

// Close client QUIC connection and free resources.
// Vtable-internal (Batch 3). App layer uses transport->disconnect(CONN_CLIENT_QUIC,...).
void        WN_ClientDisconnect( void );

// Check if a connect-phase error was recorded.  Copies the error string to
// *out (if non-NULL) and returns qtrue.  Returns qfalse if no error.
// Vtable-internal (Batch 3). App layer uses transport->get_error.
qboolean    WN_ClientHasError( char *out, int outSize );

// Consume the pending error — call before CL_Disconnect to avoid it being
// wiped by the disconnect path before the dialog reads it.
// Vtable-internal (Batch 3). App layer uses transport->clear_error.
void        WN_ClientClearError( void );

// True if a QUIC client connection is active (handshaking or established).
// Vtable-internal (Batch 3). App layer uses transport->is_connecting.
qboolean    WN_ClientIsConnecting( void );

// Feed a raw UDP packet to the client picoquic context (called from WN_DemuxPacket).
qboolean    WN_ClientCheckPacket( const netadr_t *from, byte *buf, int len );

// Send a datagram via the client QUIC connection (called from WN_SendGamePacketToAddr).
void        WN_ClientSendPacket( const netadr_t *to, const void *data, int length );
#endif // !HEADLESS

#endif // WN_PUBLIC_H
