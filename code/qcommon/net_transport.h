/*
===========================================================================
net_transport.h — Game transport vtable abstraction

Decouples engine networking from the underlying protocol (QUIC).
All engine code routes sends and receives through transport->* function
pointers. Only code/qcommon/wired/net/wn_transport.c touches picoquic directly.

conn_handle_t
  Opaque 64-bit connection identifier. Replaces netadr_t + NA_QUIC routing.
    CONN_INVALID (0)   — no connection
    1..WN_MAX_CLIENTS  — server-side game-client connection (slot index 1-based)
    CONN_CLIENT        — the single outgoing client connection (non-dedicated only)

reliable_channel_t
  Semantic reliable message channel.

  For QUIC/WebTransport:
    CHAN_SESSION stays on stream 0 for session TLVs.
    Other game-reliable messages use fresh QUIC streams with a tiny
    application header carrying version + semantic channel.
    This keeps stream IDs transport-private and avoids coupling app
    routing to QUIC stream numbering.
===========================================================================
*/
#ifndef NET_TRANSPORT_H
#define NET_TRANSPORT_H

#include "q_shared.h"

typedef uint64_t conn_handle_t;
#define CONN_INVALID ((conn_handle_t)0)

typedef enum {
	/* Game Protocol (0x00–0x0F) — engine core */
	CHAN_SESSION            = 0x00, /* Session control TLVs on stream 0                              */
	CHAN_EVENTS             = 0x02, /* srv→cli  Kill, damage, chat, vote (reliable, MessagePack)     */
	CHAN_BOOTSTRAP          = 0x04, /* srv→cli  Configstrings + baselines at connect (reliable)      */
	CHAN_COMMANDS           = 0x06, /* cli→srv  say, callvote, buy — reliable client requests        */
	CHAN_SNAPSHOT_RELIABLE  = 0x08, /* srv→cli  Oversize snapshots on reliable stream (mode 1)       */
	/* 0x0A–0x0F: game protocol expansion (voice, replay, …)                                        */

	/* Content Delivery (0x20–0x2F) */
	CHAN_DOWNLOAD           = 0x20, /* srv→cli  File download blocks (reliable)                      */
	/* 0x22–0x2F: content delivery expansion                                                        */

	/* Observation (0x40–0x4F) — spectator / dashboard */
	CHAN_OBSERVE            = 0x40, /* srv→cli  Observer state push (MessagePack)                    */
	/* 0x42–0x4F: observation expansion                                                             */

	/* Services (0x60–0x6F) — fully independent layers */
	CHAN_MCP                = 0x60, /* bidi  JSON-RPC 2.0, AI coaching, bot control                  */
	/* 0x62–0x6F: services expansion                                                                */
} reliable_channel_t;

typedef enum {
	WN_BOOTSTRAP_MSG_STATE = 1,
} wn_bootstrap_msg_type_t;

typedef enum {
	WN_BOOTSTRAP_SEC_ACK = 1,
	WN_BOOTSTRAP_SEC_SERVER_CMDS = 2,
	WN_BOOTSTRAP_SEC_CONFIGSTRINGS = 3,
	WN_BOOTSTRAP_SEC_BASELINES = 4,
	WN_BOOTSTRAP_SEC_CLIENT_INFO = 5,
} wn_bootstrap_section_t;

typedef enum {
	WN_DOWNLOAD_MSG_BLOCK = 1,
	WN_DOWNLOAD_MSG_ERROR = 2,
} wn_download_msg_type_t;

typedef struct {
	/* ── Lifecycle ──────────────────────────────────────────────── */
	void          (*init)( void );
	void          (*shutdown)( void );
	void          (*frame)( int msec );

	/* ── Server ─────────────────────────────────────────────────── */
	void          (*listen)( int port );
	/*
	 * accept_callback: called when a new game-client connection has
	 * completed the QUIC handshake and sent a CONNECT message.
	 * userinfo: the client's info string (from Stream 0 CONNECT payload).
	 * Set to NULL until Phase B wires in SV_OnPlayerConnect.
	 */
	void          (*accept_callback)( conn_handle_t conn, const char *userinfo );
	/*
	 * ready_callback: called when a connected game-client sends TLV 0x05 READY,
	 * signalling it has processed the gamestate and is ready to enter the world.
	 * Set to NULL until Phase B2 wires in SV_OnPlayerReady.
	 */
	void          (*ready_callback)( conn_handle_t conn );
	void          (*drop_client)( conn_handle_t conn, const char *reason );

	/* ── Client ─────────────────────────────────────────────────── */
	conn_handle_t (*connect)( const char *address, int port, const char *userinfo );
	void          (*disconnect)( conn_handle_t conn, const char *reason );

	/* ── Unreliable datagrams — snapshots and usercmds ──────────── */
	void          (*send_unreliable)( conn_handle_t conn, const byte *data, int len );
	qboolean      (*recv_unreliable)( conn_handle_t *conn_out, byte *buf, int *len_out );

	/* ── Reliable streams ───────────────────────────────────────── */
	void          (*send_reliable)( conn_handle_t conn, int channel,
	                                const byte *data, int len );
	qboolean      (*recv_reliable)( conn_handle_t *conn_out, int *channel_out,
	                                byte *buf, int *len_out );

	/* ── Per-connection metrics (from QUIC congestion control) ──── */
	int           (*get_ping)( conn_handle_t conn );       /* RTT ms; -1 = unknown */
	float         (*get_loss)( conn_handle_t conn );       /* 0.0–1.0 */
	int           (*get_bandwidth)( conn_handle_t conn );  /* kbps estimate */
	void          (*get_address_string)( conn_handle_t conn, char *buf, int buflen );
} transport_t;

/*
 * Global transport pointer.  Set to &quic_transport during WN_Init().
 * NULL until the transport is initialized.
 */
extern transport_t *transport;

#endif /* NET_TRANSPORT_H */
