/*
===========================================================================
wt_public.h — QUIC transport public API

All functions are called from the engine (sv_main.c, net_ip.c, sv_game.c).
None of these symbols exist when FEAT_QUIC_TRANSPORT is 0.

Channel architecture:
  QUIC Connection (single port, single UDP socket)
  │
  ├── Datagrams ──── Game State (unreliable, msgpack)
  ├── Stream 0x00 ── Capability negotiation (json, bidi)
  ├── Stream 0x03 ── Game Events (reliable, msgpack, server→client)
  ├── Stream 0x08+ ─ MCP (reliable, json-rpc 2.0, bidi)
  └── Stream: HTTP ─ /health, /metrics (request-response)
===========================================================================
*/
#ifndef WT_PUBLIC_H
#define WT_PUBLIC_H

#include "../game/q_feats.h"

#if FEAT_QUIC_TRANSPORT

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

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
#define WT_PERM_ADMIN      (1 << 2)   // authority: rcon, kick, map change

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

#endif // FEAT_QUIC_TRANSPORT
#endif // WT_PUBLIC_H
