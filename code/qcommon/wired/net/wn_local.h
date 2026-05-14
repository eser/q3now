// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wn_local.h — QUIC transport internal header

Shared between wn_*.c files. Not included by engine code — use wn_public.h.
===========================================================================
*/
#ifndef WN_LOCAL_H
#define WN_LOCAL_H

#include "wn_public.h"

// picoquic headers
#include "picoquic.h"
#include "picoquic_utils.h"

// ───────────────────────────────────────────────────────────────────
// QUIC context — singleton, created in WN_Init, destroyed in WN_Shutdown
// ───────────────────────────────────────────────────────────────────

#define WN_MAX_CLIENTS          8       // sv_wirednetMaxClients default
#define WN_ALPN                 "q3v69"
#define WN_EVENT_RING_SIZE      65536   // 64KB event ring buffer
#define WN_PACKET_BUF_SIZE      1536    // max UDP packet (MTU + headroom)
#define WN_MCP_JSON_BUF_SIZE    8192    // per-connection MCP buffer (grows to 32KB)

#define WN_GAME_QUEUE_SIZE      64      // per-player inbound datagram slots
#define WN_GAME_PKT_MAX         2048    // max usercmd datagram payload (Q3 MAX_PACKETLEN)

/* WN_SNAP_DGRAM_MAX: client recv buffer for snapshot datagrams.
 * Must match WN_DATAGRAM_MTU (defined in wn_public.h, included above). */
#define WN_SNAP_DGRAM_MAX       WN_DATAGRAM_MTU
#define WIREDNET_SESSION_STREAM_ID  0x00    // session control channel (binary TLV)
#define WN_REL_QUEUE_SIZE      64
#define WN_REL_PARTIAL_CAP     64
/* CHAN_BOOTSTRAP carries all configstrings + baselines and can exceed MAX_MSGLEN.
 * It uses a dedicated large buffer in wn_client_state_t instead of rel_queue. */
#define WN_BOOTSTRAP_MAX       (256 * 1024)

#define WN_JSON_RING_SIZE      256   // JSON event ring slots (pre-formatted for /status.json)
#define WN_JSON_EVENT_MAX      256   // max bytes per JSON event string
#define WN_HTTP_RATE_BUCKETS   64    // per-IP rate limit hash table size

typedef struct {
	int  channel;
	int  len;
	byte data[MAX_MSGLEN];
} wn_rel_msg_t;

typedef struct {
	qboolean active;
	uint64_t stream_id;
	int      channel;          /* -1 until header is fully staged + validated */
	int      header_bytes;     /* 0, 1, or 2 — staged header bytes seen so far */
	byte     header_staging[2];/* staging buffer for the 2-byte reliable header */
	int      len;
	byte     data[MAX_MSGLEN];
} wn_rel_partial_t;

// Handshake state for the TLV game connect flow (stream 0x00)
typedef enum {
	WN_GAME_HS_NONE,            // no handshake in progress
	WN_GAME_HS_PENDING,         // waiting for server SV_DirectConnectQUIC
	WN_GAME_HS_ACCEPTED,        // server accepted the player
	WN_GAME_HS_REFUSED,         // server refused (full, banned, etc.)
} wn_game_hs_state_t;

// One queued usercmd datagram received from a QUIC game client (server-side)
typedef struct {
	netadr_t    from;           // filled as NA_QUIC / NA_QUIC6
	int         len;
	byte        data[WN_GAME_PKT_MAX];
} wn_game_pkt_t;

// One queued snapshot datagram received from a QUIC game server (client-side only)
// Snapshots can be up to MAX_MSGLEN (16384) bytes — far larger than WN_GAME_PKT_MAX.
typedef struct {
	netadr_t    from;           // filled as NA_QUIC / NA_QUIC6
	int         len;
	byte        data[WN_SNAP_DGRAM_MAX];
} wn_snap_pkt_t;

// Per-player QUIC game connection state (one slot per connected game client)
typedef struct wn_game_conn_s {
	qboolean            active;
	struct wn_connection_s *conn;           // owning QUIC connection (back-pointer)
	wn_game_hs_state_t  hs_state;
	uint64_t            handshake_stream_id; // Stream 0x00 handle (session control)

	// Single-producer (QUIC callback), single-consumer (NET_Event main loop)
	// lock-free ring: head written by producer, tail read by consumer
	wn_game_pkt_t       recv_queue[WN_GAME_QUEUE_SIZE];
	volatile int        recv_head;          // next write slot
	volatile int        recv_tail;          // next read slot

	/* Reliable stream receive queue. QUIC callbacks accumulate bytes per stream
	 * in rel_partials[] and publish complete messages to rel_queue[]. */
	wn_rel_msg_t       rel_queue[WN_REL_QUEUE_SIZE];
	volatile int       rel_head;
	volatile int       rel_tail;
	wn_rel_partial_t   rel_partials[WN_REL_PARTIAL_CAP];
} wn_game_conn_t;

// Connection state — one per QUIC peer
typedef struct wn_connection_s {
	picoquic_cnx_t  *cnx;               // picoquic connection handle
	netadr_t         addr;               // remote address
	uint8_t          perm;               // permission bitmask (WN_PERM_*)
	uint64_t         connect_time;       // Sys_Microseconds at connect
	qboolean         authenticated;      // capability negotiation complete
	qboolean         active;             // slot in use

	// stream handles
	uint64_t         control_stream_id;  // Stream 0x00 (session control, binary TLV)
	uint64_t         event_stream_id;    // Stream 0x03 (server→client events)
	wn_game_conn_t  *game_conn;          // non-NULL when WN_PERM_PLAYER is set

	// coaching.tick state
	uint64_t         last_tick_json_seq; // JSON ring cursor: events already sent to this client
} wn_connection_t;

// Event ring buffer entry
typedef struct wn_event_s {
	uint64_t         seq;                // monotonic event sequence
	uint32_t         time;               // server time (ms)
	byte             data[256];          // msgpack-encoded event payload
	int              data_len;           // actual payload length
} wn_event_t;

// JSON event ring entry — pre-formatted string for /status.json (avoids msgpack decode on HTTP path)
typedef struct {
	uint64_t         seq;                // matches wn.json_write_idx at push time
	char             json[WN_JSON_EVENT_MAX]; // null-terminated JSON object string
} wn_json_event_t;

// Per-IP HTTP rate limit bucket (fixed open-addressing table)
typedef struct {
	uint32_t         ip_hash;            // djb2 hash of IP bytes (excludes port)
	int              count;              // requests in current 1-second window
	int              reset_time;         // Sys_Milliseconds() at window start
	qboolean         used;               // slot is occupied
} wn_rate_bucket_t;

// Pending game-client connect entry (written by QUIC I/O thread, consumed on main thread)
typedef struct {
	qboolean         pending;
	conn_handle_t    conn;
	char             userinfo[MAX_INFO_STRING];
} wn_pending_connect_t;

// Global QUIC state
typedef struct wn_state_s {
	picoquic_quic_t *quic;               // picoquic context
	qboolean         initialized;        // WN_Init succeeded

	// cvars
	cvar_t          *sv_wirednetAuthToken;   // auth tokens
	cvar_t          *sv_wirednetMaxClients;  // max QUIC connections
	cvar_t          *sv_wirednetStateRate;   // datagram rate (Hz)
	cvar_t          *sv_wirednetEventRate;   // event push rate (Hz)
	cvar_t          *sv_observerEventCount;  // max events in /status.json (default 100)
	cvar_t          *sv_httpRateLimit;       // max HTTP requests/sec per IP (default 30)

	// connections
	wn_connection_t  connections[WN_MAX_CLIENTS];
	int              num_connections;

	// msgpack event ring buffer
	wn_event_t       events[WN_EVENT_RING_SIZE / sizeof(wn_event_t)];
	uint64_t         event_write_idx;    // next write position (wraps)
	uint64_t         event_seq;          // monotonic sequence counter

	// JSON event ring — parallel to msgpack ring, pre-formatted for /status.json
	wn_json_event_t  json_events[WN_JSON_RING_SIZE];
	uint64_t         json_write_idx;     // next write position (wraps)

	// Per-IP HTTP rate limit table
	wn_rate_bucket_t http_rate[WN_HTTP_RATE_BUCKETS];

	// TCP HTTP listener file descriptors (-1 = not bound)
	int              tcp4_fd;
	int              tcp6_fd;

	// stats
	int              dropped_packets;    // demux misroute counter

	// timing
	uint64_t         last_datagram_time; // last state datagram send
	uint64_t         last_event_time;    // last event push

	// packet buffer — QUIC-owned copy of incoming packet data
	byte             recv_buf[WN_PACKET_BUF_SIZE];

	// Game connection state — one per player QUIC connection
	wn_game_conn_t   game_conns[WN_MAX_CLIENTS];
	int              num_game_conns;

	// Pending connects: written by QUIC I/O thread, drained on main thread
	wn_pending_connect_t pending_connects[WN_MAX_CLIENTS];

	/* Pending ready events: set when a client sends TLV 0x05 READY.
	 * Index corresponds to game_conns[] slot (0-based). */
	qboolean             pending_ready[WN_MAX_CLIENTS];
} wn_state_t;

extern wn_state_t wn;

// ───────────────────────────────────────────────────────────────────
// Internal functions (wn_*.c cross-references)
// ───────────────────────────────────────────────────────────────────

// wn_connection.c
wn_connection_t *WN_FindConnection( picoquic_cnx_t *cnx );
wn_connection_t *WN_AllocConnection( picoquic_cnx_t *cnx, netadr_t *from );
void             WN_FreeConnection( wn_connection_t *conn );
void             WN_LogConnect( wn_connection_t *conn );
void             WN_LogDisconnect( wn_connection_t *conn, const char *reason );

// wn_connection.c (continued)
void             WN_HandleCapabilityNegotiation( wn_connection_t *conn, uint64_t stream_id,
                                                  const byte *data, int len );

// wn_transport.c
qboolean         WN_IsWiredNetPacket( const byte *buf, int len );

#if FEAT_WIREDNET_OBSERVER
// wn_events.c
void             WN_EventRingPush( const byte *data, int len );
int              WN_EventRingRead( uint64_t from_seq, wn_event_t *out, int max_events );
void             WN_JsonEventRingPush( const char *json );

// wn_recording.c
void             WN_RecordInit( void );
void             WN_RecordEvent( const byte *data, int len );
void             WN_RecordShutdown( void );

// wn_transport.c
int              WN_EncodeStateUpdate( byte *buf, int buf_size );

// wn_msgpack.c
int              WN_EncodeKillEvent( byte *buf, int buf_size,
                                     int attacker, int victim, int mod,
                                     const vec3_t attacker_pos, const vec3_t victim_pos );
int              WN_EncodeDamageEvent( byte *buf, int buf_size,
                                       int attacker, int victim, int damage, int mod,
                                       const vec3_t attacker_pos, const vec3_t victim_pos );
int              WN_EncodeItemPickupEvent( byte *buf, int buf_size,
                                           int client, const char *item, const vec3_t pos );
int              WN_EncodeChatEvent( byte *buf, int buf_size,
                                     int client, const char *msg, qboolean teamOnly );
int              WN_EncodeBotEvent( byte *buf, int buf_size,
                                    int bot_id, const char *event_type,
                                    int param1, int param2, const vec3_t pos );
#endif

#if FEAT_WIREDNET_CONTROL
// wn_mcp.c
void             WN_McpHandleMessage( wn_connection_t *conn, uint64_t stream_id,
                                       const byte *data, int len );
int              WN_McpDispatch( const char *json_in, int json_len,
                                  char *response, int response_size,
                                  wn_connection_t *conn );
#endif

#if FEAT_WIREDNET_OBSERVER
// wn_http.c
void             WN_HttpHandleRequest( wn_connection_t *conn, uint64_t stream_id,
                                        const byte *data, int len );
#endif

// wn_transport.c — WiredNet game transport (snapshot/usercmd datagrams + session handshake)
	wn_game_conn_t  *WN_GameAllocConn( wn_connection_t *conn );
	void             WN_GameFreeConn( wn_game_conn_t *gc );
	void             WN_GameHandleDatagram( wn_connection_t *conn, const byte *data, int len );
	void             WN_GameHandleHandshake( wn_connection_t *conn, uint64_t stream_id,
	                                          const byte *data, int len );
	conn_handle_t    WN_GetConnHandleByAddr( const netadr_t *addr );
	void             WN_DrainPendingConnects( void );
	void             WN_GameHandleReliable( wn_connection_t *conn, uint64_t stream_id,
	                                         const byte *data, int len, qboolean fin );

#if !defined(DEDICATED)
/*
 * wn_client_state_t — game-client QUIC context.
 * Created when a non-dedicated client connects to a remote server via QUIC.
 * Separate from wn_state_t (server context) so that listen-server + loopback
 * clients are not affected.
 */
typedef struct {
	qboolean         initialized;    // WN_ClientConnect() succeeded
	picoquic_quic_t *quic;           // picoquic CLIENT context
	picoquic_cnx_t  *cnx;           // connection to game server

	netadr_t         server_addr;   // server NA_IP/NA_IP6 address

	// userinfo/qport captured at connect time (for TLV CONNECT message)
	char             userinfo[MAX_INFO_STRING];
	int              qport;

	// SPSC receive queue: QUIC callback thread produces, NET_Event consumes
	wn_snap_pkt_t    recv_queue[WN_GAME_QUEUE_SIZE];
	volatile int     recv_head;
	volatile int     recv_tail;

	// TLV session channel (stream 0x00): set when ACCEPT received
	qboolean         accept_pending;

	/* Reliable stream receive queue for server->client messages. */
	wn_rel_msg_t       rel_queue[WN_REL_QUEUE_SIZE];
	volatile int       rel_head;
	volatile int       rel_tail;
	wn_rel_partial_t   rel_partials[WN_REL_PARTIAL_CAP];

	// Scratch recv buffer for incoming QUIC UDP packets
	byte             recv_buf[WN_PACKET_BUF_SIZE];

	// Connect-phase error — set by WN_ClientConnect / WN_ClientFlushOutbound,
	// consumed by WN_ClientHasError / WN_ClientClearError.
	qboolean         connect_failed;
	char             connect_error[512];

	/* CHAN_BOOTSTRAP: dedicated large recv buffer — bypasses rel_queue because
	 * configstrings + baselines can far exceed MAX_MSGLEN (16 KB).
	 * Written by wn_reliable_client_consume_stream, consumed + cleared by
	 * WN_ClientConsumeBootstrap(). Only one bootstrap is ever pending at a time. */
	byte             bootstrap_recv_data[WN_BOOTSTRAP_MAX];
	int              bootstrap_recv_len;
	qboolean         bootstrap_recv_ready;

	// Deferred disconnect — set by the picoquic_callback_close handler.
	// WN_ClientDisconnect() must NOT be called from inside a picoquic
	// callback (picoquic_free while inside picoquic_prepare_packet_ex causes
	// splay-tree use-after-free). Set this flag instead; WN_ClientFlushOutbound
	// calls WN_ClientDisconnect() after picoquic_prepare_next_packet_ex returns.
	qboolean         pending_disconnect;
} wn_client_state_t;

extern wn_client_state_t wtcl;

// wn_transport.c
void     WN_ClientConnect( const netadr_t *serverAddr, const char *userinfo, int qport );
void     WN_ClientFrame( void );     // pump picoquic client timers + flush outbound
void     WN_ClientDisconnect( void );
qboolean WN_ClientIsConnecting( void );
qboolean WN_ClientCheckPacket( const netadr_t *from, byte *buf, int len );
qboolean WN_ClientGetPacket( netadr_t *from, msg_t *message );
void     WN_ClientSendPacket( const netadr_t *to, const void *data, int length );
#endif

/* transport_t vtable — implemented in wn_transport.c */
#include "../../net_transport.h"
extern transport_t quic_transport;

#endif // WN_LOCAL_H
