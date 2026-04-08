/*
===========================================================================
wt_public.h — QUIC transport public API

All functions are called from the engine (sv_main.c, net_ip.c, sv_game.c).
None of these symbols exist when FEAT_QUIC_TRANSPORT is 0.

Channel architecture:
  QUIC Connection (single port, single UDP socket)
  │
  ├── Datagrams ──── Game State (unreliable, msgpack)           [OBSERVE]
  ├── Stream 0x00 ── Capability negotiation (json, bidi)
  ├── Stream 0x01 ── Game Handshake — replaces UDP OOB challenge/connect
  │                   Client→Server: {type:"connect", userinfo, challenge, qport}
  │                   Server→Client: {type:"connectResponse"} | {type:"refuse"}
  ├── Datagrams ──── netchan packets (unreliable, Q3 binary format) [QUIC_GAME]
  │                   Client→Server: user commands (movement, angles, buttons)
  │                   Server→Client: snapshots (entity state deltas)
  ├── Stream 0x03 ── Game Events (reliable, msgpack, server→client)
  ├── Stream 0x08+ ─ MCP (reliable, json-rpc 2.0, bidi)
  └── Stream: HTTP ─ /health, /metrics (request-response)
===========================================================================
*/
#ifndef WT_PUBLIC_H
#define WT_PUBLIC_H

#include "../qcommon/q_feats.h"

#if FEAT_QUIC_TRANSPORT

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/net_transport.h"

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
#define WT_PERM_PLAYER     (1 << 0)   // connection type: game client
#define WT_PERM_LEADER     (1 << 1)   // team role: coaching / team commands
#define WT_PERM_ADMIN      (1 << 2)   // authority: kick, map change

// Common presets
#define WT_PERM_OBSERVER_MEMBER_USER   0                                          // 0b000
#define WT_PERM_OBSERVER_LEADER_ADMIN  (WT_PERM_LEADER | WT_PERM_ADMIN)          // 0b110
#define WT_PERM_PLAYER_MEMBER_USER     (WT_PERM_PLAYER)                           // 0b001
#define WT_PERM_PLAYER_LEADER_ADMIN    (WT_PERM_PLAYER | WT_PERM_LEADER | WT_PERM_ADMIN) // 0b111

static inline qboolean WT_HasPermPlayer( uint8_t perm )  { return (perm & WT_PERM_PLAYER) != 0; }
static inline qboolean WT_HasPermLeader( uint8_t perm )  { return (perm & WT_PERM_LEADER) != 0; }
static inline qboolean WT_HasPermAdmin( uint8_t perm )   { return (perm & WT_PERM_ADMIN)  != 0; }

// ───────────────────────────────────────────────────────────────────
// Lifecycle — called from sv_init.c / sv_main.c
// ───────────────────────────────────────────────────────────────────

void        QUIC_Init( void );
void        QUIC_Shutdown( void );
void        QUIC_RegisterCommands( void );

// ───────────────────────────────────────────────────────────────────
// Packet handling — called from net_ip.c
// ───────────────────────────────────────────────────────────────────

// Check if a received UDP packet is a QUIC packet.
// buf points to the raw recvfrom data, len is the byte count.
// Returns qtrue if the packet was consumed by the QUIC subsystem.
qboolean    QUIC_CheckPacket( netadr_t *from, byte *buf, int len );

// ───────────────────────────────────────────────────────────────────
// Frame processing — called from SV_Frame() and after recv drain
// ───────────────────────────────────────────────────────────────────

void        QUIC_ProcessTimers( void );       // retransmit, idle timeout, keepalive
void        QUIC_FlushOutbound( void );       // pull packets from picoquic → NET_SendPacket

#if FEAT_QUIC_OBSERVE
void        QUIC_SendDatagrams( void );       // push game state datagrams
void        QUIC_PushEvents( void );          // push buffered game events on event stream
#endif

#if FEAT_QUIC_CONTROL
void        QUIC_ProcessCommandQueue( void ); // process pending MCP/command requests
#endif

// ───────────────────────────────────────────────────────────────────
// Event emission — called from sv_game.c syscall handler
// Game code (g_combat.c etc.) calls these via VM syscall traps.
// ───────────────────────────────────────────────────────────────────

#if FEAT_QUIC_OBSERVE
void        QUIC_EmitKill( int attacker, int victim, int mod,
                           const vec3_t attacker_pos, const vec3_t victim_pos );
void        QUIC_EmitDamage( int attacker, int victim, int damage, int mod,
                             const vec3_t attacker_pos, const vec3_t victim_pos );
void        QUIC_EmitItemPickup( int client, const char *item, const vec3_t pos );
void        QUIC_EmitChat( int client, const char *msg, qboolean teamOnly );
void        QUIC_EmitMatchEvent( const char *type, const char *data );
#if FEAT_BOT_IMPROVEMENTS
void        QUIC_EmitBotEvent( int bot_id, const char *event_type,
                               int param1, int param2, const vec3_t pos );
#endif
#endif

// Sys_Microseconds() is declared in qcommon.h (int64_t) — no redeclaration needed.
// picoquic expects uint64_t; cast at call site.

#if FEAT_QUIC_GAME

// ───────────────────────────────────────────────────────────────────
// QUIC game transport — called from net_ip.c / Sys_SendPacket
// ───────────────────────────────────────────────────────────────────

// Send a netchan-format packet over a QUIC game connection.
// Called from Sys_SendPacket when to->type is NA_QUIC or NA_QUIC6.
void        QUIC_SendGamePacketToAddr( const netadr_t *to, const void *data, int length );

// Dequeue one netchan-format packet received over any QUIC game connection.
// Fills *from (type = NA_QUIC / NA_QUIC6) and *message.
// Returns qtrue if a packet was available, qfalse when the queue is empty.
// Called from NET_Event after the UDP recvfrom loop.
qboolean    QUIC_GetGamePacket( netadr_t *from, msg_t *message );

// Look up a conn_handle_t for an address.  Returns CONN_INVALID when no
// active game connection exists for that address.  Type-agnostic (NA_QUIC
// and NA_IP both match the underlying IP bytes).
conn_handle_t QUIC_GetConnHandleByAddr( const netadr_t *addr );

#ifndef DEDICATED
// ───────────────────────────────────────────────────────────────────
// QUIC game client API — called from cl_main.c / NET_Event
// ───────────────────────────────────────────────────────────────────

// Initiate QUIC connection to game server (async; WT_ClientCallback drives it).
void        QUIC_ClientConnect( const netadr_t *serverAddr, const char *userinfo, int qport );

// Pump client QUIC timers + flush outbound.  Call from NET_Event every frame.
void        QUIC_ClientFrame( void );

// Close client QUIC connection and free resources.
void        QUIC_ClientDisconnect( void );

// True if a QUIC client connection is active (handshaking or established).
qboolean    QUIC_ClientIsConnecting( void );

// Feed a raw UDP packet to the client picoquic context (called from QUIC_CheckPacket).
qboolean    QUIC_ClientCheckPacket( const netadr_t *from, byte *buf, int len );

// Dequeue one packet from the client QUIC receive queue (called from NET_Event).
qboolean    QUIC_ClientGetPacket( netadr_t *from, msg_t *message );

// Send a datagram via the client QUIC connection (called from QUIC_SendGamePacketToAddr).
void        QUIC_ClientSendPacket( const netadr_t *to, const void *data, int length );
#endif // !DEDICATED

#endif // FEAT_QUIC_GAME

#endif // FEAT_QUIC_TRANSPORT
#endif // WT_PUBLIC_H
