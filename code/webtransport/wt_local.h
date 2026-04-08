/*
===========================================================================
wt_local.h — QUIC transport internal header

Shared between wt_*.c files. Not included by engine code — use wt_public.h.
===========================================================================
*/
#ifndef WT_LOCAL_H
#define WT_LOCAL_H

#include "wt_public.h"

#if FEAT_QUIC_TRANSPORT

// picoquic headers
#include "picoquic.h"
#include "picoquic_utils.h"

// ───────────────────────────────────────────────────────────────────
// QUIC context — singleton, created in QUIC_Init, destroyed in QUIC_Shutdown
// ───────────────────────────────────────────────────────────────────

#define WT_MAX_CLIENTS          8       // sv_quicMaxClients default
#define WT_ALPN                 "q3v69"
#define WT_EVENT_RING_SIZE      65536   // 64KB event ring buffer
#define WT_PACKET_BUF_SIZE      1536    // max UDP packet (MTU + headroom)
#define WT_MCP_JSON_BUF_SIZE    8192    // per-connection MCP buffer (grows to 32KB)

#if FEAT_QUIC_GAME
#define WT_GAME_QUEUE_SIZE      64      // per-player inbound datagram slots
#define WT_GAME_PKT_MAX         2048    // max netchan fragment (Q3 MAX_PACKETLEN)
#define QUIC_GAME_STREAM_ID     0x01    // client-initiated bidi stream for game handshake

// Handshake state for Stream 0x01 (game connect flow)
typedef enum {
	WT_GAME_HS_NONE,            // no handshake in progress
	WT_GAME_HS_PENDING,         // waiting for server SV_DirectConnectQUIC
	WT_GAME_HS_ACCEPTED,        // server accepted the player
	WT_GAME_HS_REFUSED,         // server refused (full, banned, etc.)
} wt_game_hs_state_t;

// One queued netchan datagram received from a QUIC game client
typedef struct {
	netadr_t    from;           // filled as NA_QUIC / NA_QUIC6
	int         len;
	byte        data[WT_GAME_PKT_MAX];
} wt_game_pkt_t;

// Per-player QUIC game connection state (one slot per connected game client)
typedef struct wt_game_conn_s {
	qboolean            active;
	struct wt_connection_s *conn;           // owning QUIC connection (back-pointer)
	wt_game_hs_state_t  hs_state;
	uint64_t            handshake_stream_id; // Stream 0x01 handle

	// Single-producer (QUIC callback), single-consumer (NET_Event main loop)
	// lock-free ring: head written by producer, tail read by consumer
	wt_game_pkt_t       recv_queue[WT_GAME_QUEUE_SIZE];
	volatile int        recv_head;          // next write slot
	volatile int        recv_tail;          // next read slot
} wt_game_conn_t;
#endif // FEAT_QUIC_GAME

// Connection state — one per QUIC peer
typedef struct wt_connection_s {
	picoquic_cnx_t  *cnx;               // picoquic connection handle
	netadr_t         addr;               // remote address
	uint8_t          perm;               // permission bitmask (WT_PERM_*)
	uint64_t         connect_time;       // Sys_Microseconds at connect
	qboolean         authenticated;      // capability negotiation complete
	qboolean         active;             // slot in use

	// stream handles
	uint64_t         control_stream_id;  // Stream 0x00 (capability)
	uint64_t         event_stream_id;    // Stream 0x03 (server→client events)
#if FEAT_QUIC_GAME
	uint64_t         game_stream_id;     // Stream 0x01 (game handshake)
	wt_game_conn_t  *game_conn;          // non-NULL when WT_PERM_PLAYER is set
#endif
} wt_connection_t;

// Event ring buffer entry
typedef struct wt_event_s {
	uint64_t         seq;                // monotonic event sequence
	uint32_t         time;               // server time (ms)
	byte             data[256];          // msgpack-encoded event payload
	int              data_len;           // actual payload length
} wt_event_t;

// Global QUIC state
typedef struct wt_state_s {
	picoquic_quic_t *quic;               // picoquic context
	qboolean         initialized;        // QUIC_Init succeeded

	// cvars
	cvar_t          *sv_quic;            // master enable
	cvar_t          *sv_quicAuthToken;   // auth tokens
	cvar_t          *sv_quicMaxClients;  // max QUIC connections
	cvar_t          *sv_quicStateRate;   // datagram rate (Hz)
	cvar_t          *sv_quicEventRate;   // event push rate (Hz)

	// connections
	wt_connection_t  connections[WT_MAX_CLIENTS];
	int              num_connections;

	// event ring buffer
	wt_event_t       events[WT_EVENT_RING_SIZE / sizeof(wt_event_t)];
	uint64_t         event_write_idx;    // next write position (wraps)
	uint64_t         event_seq;          // monotonic sequence counter

	// stats
	int              dropped_packets;    // demux misroute counter

	// timing
	uint64_t         last_datagram_time; // last state datagram send
	uint64_t         last_event_time;    // last event push

	// packet buffer — QUIC-owned copy of incoming packet data
	byte             recv_buf[WT_PACKET_BUF_SIZE];

#if FEAT_QUIC_GAME
	// Game connection state — one per player QUIC connection
	wt_game_conn_t   game_conns[WT_MAX_CLIENTS];
	int              num_game_conns;
#endif
} wt_state_t;

extern wt_state_t wt;

// ───────────────────────────────────────────────────────────────────
// Internal functions (wt_*.c cross-references)
// ───────────────────────────────────────────────────────────────────

// wt_connection.c
wt_connection_t *WT_FindConnection( picoquic_cnx_t *cnx );
wt_connection_t *WT_AllocConnection( picoquic_cnx_t *cnx, netadr_t *from );
void             WT_FreeConnection( wt_connection_t *conn );
uint8_t          WT_ParseAuthToken( const char *token_str );
void             WT_LogConnect( wt_connection_t *conn );
void             WT_LogDisconnect( wt_connection_t *conn, const char *reason );

// wt_connection.c (continued)
void             WT_HandleCapabilityNegotiation( wt_connection_t *conn, uint64_t stream_id,
                                                  const byte *data, int len );

// wt_demux.c
qboolean         WT_IsQuicPacket( const byte *buf, int len );

#if FEAT_QUIC_OBSERVE
// wt_events.c
void             WT_EventRingPush( const byte *data, int len );
int              WT_EventRingRead( uint64_t from_seq, wt_event_t *out, int max_events );

// wt_recording.c
void             WT_RecordInit( void );
void             WT_RecordEvent( const byte *data, int len );
void             WT_RecordShutdown( void );

// wt_datagrams.c
int              WT_EncodeStateUpdate( byte *buf, int buf_size );

// wt_msgpack.c
int              WT_EncodeKillEvent( byte *buf, int buf_size,
                                     int attacker, int victim, int mod,
                                     const vec3_t attacker_pos, const vec3_t victim_pos );
int              WT_EncodeDamageEvent( byte *buf, int buf_size,
                                       int attacker, int victim, int damage, int mod,
                                       const vec3_t attacker_pos, const vec3_t victim_pos );
int              WT_EncodeItemPickupEvent( byte *buf, int buf_size,
                                           int client, const char *item, const vec3_t pos );
int              WT_EncodeChatEvent( byte *buf, int buf_size,
                                     int client, const char *msg, qboolean teamOnly );
#if FEAT_BOT_IMPROVEMENTS
int              WT_EncodeBotEvent( byte *buf, int buf_size,
                                    int bot_id, const char *event_type,
                                    int param1, int param2, const vec3_t pos );
#endif
#endif

#if FEAT_QUIC_CONTROL
// wt_mcp.c
void             WT_McpHandleMessage( wt_connection_t *conn, uint64_t stream_id,
                                       const byte *data, int len );
#endif

#if FEAT_QUIC_HTTP
// wt_http.c
void             WT_HttpHandleRequest( wt_connection_t *conn, uint64_t stream_id,
                                        const byte *data, int len );
#endif

#if FEAT_QUIC_GAME
// wt_transport.c — game transport (netchan datagrams + stream 0x01 handshake)
wt_game_conn_t  *WT_GameAllocConn( wt_connection_t *conn );
void             WT_GameFreeConn( wt_game_conn_t *gc );
void             WT_GameHandleDatagram( wt_connection_t *conn, const byte *data, int len );
void             WT_GameHandleHandshake( wt_connection_t *conn, uint64_t stream_id,
                                          const byte *data, int len );
conn_handle_t    QUIC_GetConnHandleByAddr( const netadr_t *addr );
#endif

#if FEAT_QUIC_GAME && !defined(DEDICATED)
/*
 * wt_client_state_t — game-client QUIC context.
 * Created when a non-dedicated client connects to a remote server via QUIC.
 * Separate from wt_state_t (server context) so that listen-server + loopback
 * clients are not affected.
 */
typedef struct {
	qboolean         initialized;    // QUIC_ClientConnect() succeeded
	picoquic_quic_t *quic;           // picoquic CLIENT context
	picoquic_cnx_t  *cnx;           // connection to game server

	netadr_t         server_addr;   // server NA_IP/NA_IP6 address

	// userinfo/qport captured at connect time (for stream 0x01 message)
	char             userinfo[MAX_INFO_STRING];
	int              qport;

	// SPSC receive queue: QUIC callback thread produces, NET_Event consumes
	wt_game_pkt_t    recv_queue[WT_GAME_QUEUE_SIZE];
	volatile int     recv_head;
	volatile int     recv_tail;

	// Stream 0x01 response buffer
	char             hs_buf[MAX_INFO_STRING + 256];
	int              hs_len;

	// Scratch recv buffer for incoming QUIC UDP packets
	byte             recv_buf[WT_PACKET_BUF_SIZE];
} wt_client_state_t;

extern wt_client_state_t wtcl;

// wt_client.c
void     QUIC_ClientConnect( const netadr_t *serverAddr, const char *userinfo, int qport );
void     QUIC_ClientFrame( void );     // pump picoquic client timers + flush outbound
void     QUIC_ClientDisconnect( void );
qboolean QUIC_ClientIsConnecting( void );
qboolean QUIC_ClientCheckPacket( const netadr_t *from, byte *buf, int len );
qboolean QUIC_ClientGetPacket( netadr_t *from, msg_t *message );
void     QUIC_ClientSendPacket( const netadr_t *to, const void *data, int length );
#endif

/* transport_t vtable — implemented in wt_transport.c */
#include "../qcommon/net_transport.h"
extern transport_t quic_transport;

#endif // FEAT_QUIC_TRANSPORT
#endif // WT_LOCAL_H
