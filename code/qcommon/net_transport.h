/*
===========================================================================
net_transport.h — Game transport vtable abstraction

Decouples engine networking from the underlying protocol (QUIC).
All engine code routes sends and receives through transport->* function
pointers. Only code/webtransport/wt_transport.c touches picoquic directly.

conn_handle_t
  Opaque 64-bit connection identifier. Replaces netadr_t + NA_QUIC routing.
    CONN_INVALID (0)   — no connection
    1..WT_MAX_CLIENTS  — server-side game-client connection (slot index 1-based)
    CONN_CLIENT        — the single outgoing client connection (non-dedicated only)

reliable_channel_t
  Semantic stream identifier; maps to QUIC stream IDs in wt_transport.c.
    CHAN_SESSION  = 0   Stream 0: connect / accept / disconnect / heartbeat
    CHAN_EVENTS   = 2   Stream 2: game events srv→cli (reliable, ordered)
    CHAN_INIT     = 4   Stream 4: configstrings + baselines at connect time
    CHAN_COMMANDS = 6   Stream 6: reliable client requests cli→srv
===========================================================================
*/
#ifndef NET_TRANSPORT_H
#define NET_TRANSPORT_H

#include "q_shared.h"

typedef uint64_t conn_handle_t;
#define CONN_INVALID ((conn_handle_t)0)

typedef enum {
	CHAN_SESSION  = 0,     /* Stream 0  – session control (bidi) */
	CHAN_EVENTS   = 2,     /* Stream 2  – game events, srv→cli */
	CHAN_INIT     = 4,     /* Stream 4  – initial state, srv→cli */
	CHAN_COMMANDS = 6,     /* Stream 6  – reliable client commands, cli→srv */
	CHAN_OBSERVE  = 0x03,  /* observe stream (existing) */
	CHAN_MCP      = 0x08,  /* MCP / JSON-RPC stream (existing) */
} reliable_channel_t;

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
 * Global transport pointer.  Set to &quic_transport during QUIC_Init().
 * NULL until the transport is initialized.
 */
extern transport_t *transport;

#endif /* NET_TRANSPORT_H */
