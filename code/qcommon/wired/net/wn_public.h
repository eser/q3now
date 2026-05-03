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

void        WN_ProcessTimers( void );       // retransmit, idle timeout, keepalive
void        WN_FlushOutbound( void );       // pull packets from picoquic → NET_SendPacket

#if FEAT_WIREDNET_OBSERVER
void        WN_SendDatagrams( void );       // push game state datagrams
void        WN_PushEvents( void );          // push buffered game events on event stream
void        WN_TcpFrame( void );            // accept + handle TCP HTTP/1.1 requests
#endif

#if FEAT_WIREDNET_CONTROL
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

/* Maximum payload in a single fragment datagram.
 * = WN_DATAGRAM_MTU - 10 (8-byte sequence header + 2-byte frag_total/frag_index).
 * Max snapshot via 8 fragments: 8 * WN_FRAG_PAYLOAD = 9264 bytes. */
#define WN_FRAG_PAYLOAD   (WN_DATAGRAM_MTU - 10)

// ───────────────────────────────────────────────────────────────────
// QUIC game transport — called from net_ip.c / Sys_SendPacket
// ───────────────────────────────────────────────────────────────────

// Send a netchan-format packet over a QUIC game connection.
// Called from Sys_SendPacket when to->type is NA_QUIC or NA_QUIC6.
void        WN_SendGamePacketToAddr( const netadr_t *to, const void *data, int length );

// Formerly dequeued netchan-format game packets; now returns qfalse always.
// Server usercmd datagrams are consumed exclusively by WN_ServerRecvUsercmd
// (called from SV_DrainQUICUsercmds) and must not be drained by NET_Event.
// Client snapshot datagrams are consumed exclusively by wn_recv_unreliable
// (called from CL_CheckSnapshotDatagrams).  The NET_Event loop that calls
// this function is retained for compatibility but is a no-op.
qboolean    WN_GetGamePacket( netadr_t *from, msg_t *message );

// Look up a conn_handle_t for an address.  Returns CONN_INVALID when no
// active game connection exists for that address.  Type-agnostic (NA_QUIC
// and NA_IP both match the underlying IP bytes).
conn_handle_t WN_GetConnHandleByAddr( const netadr_t *addr );

// Fill *out with the network address for a conn_handle. Returns qfalse for CONN_INVALID.
qboolean      WN_GetAddrByConnHandle( conn_handle_t conn, netadr_t *out );

/* Sentinel conn_handle for the single outgoing client connection.
 * Ordering: CONN_INVALID(0) < server slots(1..8) < CONN_CLIENT_QUIC(9).
 * Value must equal WN_MAX_CLIENTS+1 from wn_local.h. */
#define CONN_CLIENT_QUIC  ((conn_handle_t)9)

// Dequeue one user-command datagram from any active server game connection.
// Called directly by SV_DrainQUICUsercmds — bypasses transport vtable so the
// client's recv_unreliable path (snapshots) is never contaminated with user cmds.
qboolean      WN_ServerRecvUsercmd( conn_handle_t *conn_out, byte *buf, int *len_out );

// Dequeue one reliable message sent from the client to the server.
// Semantic channel is returned via *channel_out.
qboolean      WN_ServerRecvReliable( conn_handle_t *conn_out, int *channel_out,
	byte *buf, int *len_out );

// Drain pending game-client connects on the main thread, calling transport->accept_callback.
void          WN_DrainPendingConnects( void );

// Re-enqueue a connection that could not be admitted yet (server mid-spawn).
// Called from SV_OnPlayerConnect when svs.spawn.phase != SPAWN_IDLE.
void          WN_RequeueConnect( conn_handle_t conn, const char *userinfo );

// Drain pending game-client ready events (TLV 0x05), calling transport->ready_callback.
void          WN_DrainPendingReady( void );

#ifndef DEDICATED
// Send TLV 0x05 READY on the session stream to the connected server.
void          WN_ClientSendReady( void );

// Dequeue one reliable message sent from the server to the client.
// Semantic channel is returned via *channel_out.
qboolean      WN_ClientRecvReliable( int *channel_out, byte *buf, int *len_out );

// Consume the pending CHAN_BOOTSTRAP message (configstrings + baselines).
// *data_out points into an internal buffer valid until the next WN_ClientConnect.
// Returns qfalse if no bootstrap is pending.
qboolean      WN_ClientConsumeBootstrap( const byte **data_out, int *len_out );
#endif

#ifndef DEDICATED
// ───────────────────────────────────────────────────────────────────
// QUIC game client API — called from cl_main.c / NET_Event
// ───────────────────────────────────────────────────────────────────

// Initiate QUIC connection to game server (async; WN_ClientCallback drives it).
void        WN_ClientConnect( const netadr_t *serverAddr, const char *userinfo, int qport );

// Pump client QUIC timers + flush outbound.  Call from NET_Event every frame.
void        WN_ClientFrame( void );

// Close client QUIC connection and free resources.
void        WN_ClientDisconnect( void );

// Check if a connect-phase error was recorded.  Copies the error string to
// *out (if non-NULL) and returns qtrue.  Returns qfalse if no error.
qboolean    WN_ClientHasError( char *out, int outSize );

// Consume the pending error — call before CL_Disconnect to avoid it being
// wiped by the disconnect path before the dialog reads it.
void        WN_ClientClearError( void );

// True if a QUIC client connection is active (handshaking or established).
qboolean    WN_ClientIsConnecting( void );

// Feed a raw UDP packet to the client picoquic context (called from WN_DemuxPacket).
qboolean    WN_ClientCheckPacket( const netadr_t *from, byte *buf, int len );

// Dequeue one packet from the client QUIC receive queue (called from NET_Event).
qboolean    WN_ClientGetPacket( netadr_t *from, msg_t *message );

// Send a datagram via the client QUIC connection (called from WN_SendGamePacketToAddr).
void        WN_ClientSendPacket( const netadr_t *to, const void *data, int length );
#endif // !DEDICATED

#endif // WN_PUBLIC_H
