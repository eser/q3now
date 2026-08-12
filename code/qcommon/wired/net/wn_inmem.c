// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wn_inmem.c — in-memory same-process transport backend (in-process-queue)

The SECOND transport_t implementation. Lets same-process apps (the integrated
host + N local client-apps) exchange game traffic WITHOUT loopback-QUIC
(picoquic + 127.0.0.1 UDP + AES) — a buffer-pass over in-process ring queues.

  B1 — inert scaffold (replaced).
  B2 — connect / accept / disconnect (the first 100+ handle producer).
  B3 — send/recv buffer-pass (the per-app ring queues).
B2+B3 are landed together: the integrated host (app slot 0) moves off
loopback-QUIC onto this backend with a working data path, in one playable step.

TWO-ENDED HANDLE MODEL (see wn_public.h)
  A same-process connection has two ends, like a QUIC loopback connection's two
  distinct handles. transport_for_handle() routes BOTH ends here:
    client end  = WN_APP_CONN_BASE   + slot  (100..103)  — wtcl_array[slot]
    server end  = WN_APP_SVCONN_BASE + slot  (110..113)  — game_conns[].pub_handle
  The send path infers DIRECTION from the handle's range:
    server-end handle  → push into the CLIENT recv ring  (server→client: snapshots,
                         gamestate/bootstrap, reliable events)
    client-end handle  → push into the SERVER recv ring  (client→server: usercmds,
                         reliable commands)

RECV
  The integrated host is app slot 0. The client-side recv path
  (WN_ClientRecvReliable / WN_ClientConsumeBootstrap / wn_recv_unreliable) is
  hardwired to wtcl_array[0] and is driven by the global `transport`
  (quic_transport). Because the in-mem send pushes server→client data into the
  SAME wtcl_array[0] rings the QUIC vtable drains, the host's client-side recv
  works through the existing pump unchanged. The recv_* entries below exist for
  completeness when transport_for_handle routes a recv to this backend.

NO CRYPTO, NO SOCKET, NO FRAGMENTATION DIVERGENCE
  The byte-movement is a memcpy into a ring slot. The datagram framing
  (snapshot tier headers, the reliable [version:1][channel:1] prefix is NOT
  applied here — send_reliable carries the channel out-of-band via the ring
  slot's channel field, mirroring how the QUIC client publishes a complete
  reliable message into rel_queue). MTU stays WN_DATAGRAM_MTU so the host's
  snapshots fragment identically to QUIC and the existing reassembly path
  handles them with no change.
===========================================================================
*/
#include "wn_local.h"          /* transport_t, conn_handle_t, inmem_transport decl (via wn_public.h) */

LOG_DECLARE_CHANNEL( ch_network, "network" );

/* Direction predicates on the two-ended handle ranges. */
static ID_INLINE qboolean wn_inmem_is_server_end( conn_handle_t conn ) {
	return (qboolean)( conn >= WN_APP_SVCONN_BASE && conn < WN_APP_SVCONN_BASE + WN_APP_SVCONN_COUNT );
}
static ID_INLINE qboolean wn_inmem_is_client_end( conn_handle_t conn ) {
	return (qboolean)( conn >= WN_APP_CONN_BASE && conn < WN_APP_CONN_BASE + WN_APP_CONN_COUNT );
}
static ID_INLINE int wn_inmem_client_slot( conn_handle_t conn ) {
	if ( wn_inmem_is_server_end( conn ) ) return (int)( conn - WN_APP_SVCONN_BASE );
	if ( wn_inmem_is_client_end( conn ) ) return (int)( conn - WN_APP_CONN_BASE );
	return -1;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */
static void wn_inmem_shutdown( void ) { }
static void wn_inmem_frame( int msec ) { (void)msec; }
static void wn_inmem_flush_outbound( void ) { }

/* ── Server ────────────────────────────────────────────────────────────── */
static void wn_inmem_listen( int port ) { (void)port; }

static void wn_inmem_drop_client( conn_handle_t conn, const char *reason ) {
	/* Free the server-side game_conn for this in-process app. The client end
	 * (wtcl_array[slot]) is torn down by the client's own disconnect path. */
	wn_game_conn_t *gc = WN_GameConnByAppHandle(
		wn_inmem_is_client_end( conn )
			? (conn_handle_t)( WN_APP_SVCONN_BASE + wn_inmem_client_slot( conn ) )
			: conn );
	(void)reason;
	if ( gc )
		WN_GameFreeConn( gc );
}

static void wn_inmem_complete_admission( conn_handle_t conn, qboolean accepted,
	netRefuseClass_t refusalClass ) {
	wn_game_conn_t *gc = WN_GameConnByAppHandle( conn );
	int slot = wn_inmem_client_slot( conn );
	if ( gc ) gc->hs_state = accepted ? WN_GAME_HS_ACCEPTED : WN_GAME_HS_REFUSED;
#if !defined(HEADLESS)
	if ( !accepted && slot >= 0 && slot < WN_MAX_LOCAL_CLIENTS ) {
		wtcl_array[slot].connect_failed = qtrue;
		wtcl_array[slot].connect_error_kind = refusalClass == NET_REFUSE_AUTH
			? NET_CONNECT_ERROR_AUTH_REFUSED
			: ( refusalClass == NET_REFUSE_SERVER_FULL
				? NET_CONNECT_ERROR_SERVER_FULL : NET_CONNECT_ERROR_GENERIC );
		Q_strncpyz( wtcl_array[slot].connect_error,
			refusalClass == NET_REFUSE_AUTH ? "Server authentication failed"
				: ( refusalClass == NET_REFUSE_SERVER_FULL ? "Server is full"
					: "Connection refused" ),
			sizeof( wtcl_array[slot].connect_error ) );
	}
#else
	(void)slot;
	(void)refusalClass;
#endif
	if ( !accepted && gc )
		WN_GameFreeConn( gc );
}

static conn_handle_t wn_inmem_lookup_by_addr( const netadr_t *addr ) { (void)addr; return CONN_INVALID; }

/* ── Client ────────────────────────────────────────────────────────────── */
/* connect: emit a WN_APP_CONN_BASE+slot client handle and drive in-process
 * admission via WN_ConnectApp. The integrated host always uses app slot 0. */
static conn_handle_t wn_inmem_connect( const char *address, int port, const char *userinfo ) {
	(void)address; (void)port;
	return WN_ConnectApp( 0, userinfo );
}

static void wn_inmem_disconnect( conn_handle_t conn, const char *reason ) {
	int slot = wn_inmem_client_slot( conn );
	(void)reason;
	if ( slot < 0 )
		return;
	/* B4 disconnect-ordering: DEFER the server-side game_conn free for EVERY app
	 * slot, the integrated host (slot 0) included. The client's CL_Disconnect
	 * pushed an in-band "disconnect" reliable command into this conn's rel_queue
	 * (cl_main.c) just before calling us; freeing the game_conn now would memset
	 * that command away and the server would only reap the orphaned svs.clients[]
	 * slot via SV_CheckTimeouts. Instead mark pending_free — the next server frame
	 * drains the disconnect command (SV_DrainQUICReliableCommands → SV_DropClient)
	 * and THEN WN_DrainPendingFrees releases the conn. That ordering is fixed in
	 * sv_main.c (reliable-drain precedes pending-free within one SV_Frame). The
	 * rel_queue lives in wn.game_conns[] (not wtcl_array), so the client-end
	 * teardown below does not disturb it.
	 *
	 * Slot 0 (host) was previously freed IMMEDIATELY on the theory that the host's
	 * own server slot is never run through SV_DropClient, so a deferred pending_free
	 * would never be drained and the conn would leak. Under SCD phase-1 that theory
	 * no longer holds: a host disconnect is CLIENT-ONLY (CL_Disconnect does not
	 * SV_Shutdown — the server keeps running and keeps getting SV_Frames), so the
	 * queued "disconnect" command DOES reach SV_DrainQUICReliableCommands → the host
	 * client is dropped explicitly. And the free no longer depends on SV_DropClient
	 * at all: WN_DrainPendingFrees releases any active conn whose pending_free flag
	 * is set, on the flag alone. The immediate free was therefore destroying the
	 * disconnect the server needed to see. (At an actual server shutdown the whole
	 * wn game-conn table is torn down wholesale, so a still-pending free is not a
	 * leak there either.) */
	{
		wn_game_conn_t *gc = WN_GameConnByAppHandle( (conn_handle_t)( WN_APP_SVCONN_BASE + slot ) );
		if ( gc )
			gc->pending_free = qtrue;
	}
#if !defined(HEADLESS)
	Q_SecureZeroMemory( &wtcl_array[slot], sizeof( wtcl_array[slot] ) );
#endif
}

#if !defined(HEADLESS)
static qboolean wn_inmem_is_connecting( void ) {
	/* The in-process host completes "connecting" synchronously inside connect()
	 * (no handshake round-trip). Report not-connecting so CL_CheckForResend
	 * issues the connect exactly once, mirroring the QUIC fast-path intent. */
	return qfalse;
}
static qboolean wn_inmem_get_error( char *out, int outSize,
	netConnectErrorKind_t *kind ) {
	return WN_ClientHasError( out, outSize, kind );
}
static void wn_inmem_clear_error( void ) { }
#endif

/* ── Unreliable datagrams (snapshots srv→cli, usercmds cli→srv) ─────────── */

/* Shared SPSC datagram-ring push for the two recv rings the in-mem send path
 * feeds (the client snapshot ring wn_snap_pkt_t, the server usercmd ring
 * wn_game_pkt_t). The two ring element types differ only in their data[]
 * capacity; their leading {netadr_t from; int len; byte data[]} layout is
 * identical and the next_head/full/advance bookkeeping is identical, so the
 * caller resolves the destination slot fields (from the ring's current head)
 * and hands them here. Behavior-identical to the former hand-rolled blocks:
 * the bound check stays at the call site (each ring has its own max), this only
 * performs the full-check + stamp + advance. */
static void wn_inmem_recv_ring_push( volatile int *head, volatile int *tail,
	netadr_t *slot_from, int *slot_len, byte *slot_data,
	const byte *data, int len, const char *full_what ) {
	int next_head = ( *head + 1 ) % WN_GAME_QUEUE_SIZE;
	if ( next_head == *tail ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: %s recv queue full — datagram dropped\n", full_what );
		return;
	}
	memset( slot_from, 0, sizeof( *slot_from ) );
	slot_from->type = NA_LOOPBACK;
	*slot_len = len;
	memcpy( slot_data, data, (size_t)len );
	*head = next_head;
}

static void wn_inmem_send_unreliable( conn_handle_t conn, const byte *data, int len ) {
	int slot = wn_inmem_client_slot( conn );
	if ( slot < 0 || len <= 0 )
		return;

	if ( wn_inmem_is_server_end( conn ) ) {
		/* server → client: push into the client snapshot ring (wtcl_array[slot]).
		 * Mirror of the picoquic client recv callback push. */
#if !defined(HEADLESS)
		wn_client_state_t *c = &wtcl_array[slot];
		wn_snap_pkt_t *pkt;
		if ( !c->initialized )
			return;
		if ( len > WN_SNAP_DGRAM_MAX ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: srv→cli datagram %d > %d — dropped\n", len, WN_SNAP_DGRAM_MAX );
			return;
		}
		pkt = &c->recv_queue[c->recv_head];
		wn_inmem_recv_ring_push( &c->recv_head, &c->recv_tail,
			&pkt->from, &pkt->len, pkt->data, data, len, "client" );
#endif
		return;
	}

	if ( wn_inmem_is_client_end( conn ) ) {
		/* client → server: push into the server game_conn usercmd ring.
		 * Mirror of WN_GameHandleDatagram. */
		wn_game_conn_t *gc = WN_GameConnByAppHandle( (conn_handle_t)( WN_APP_SVCONN_BASE + slot ) );
		wn_game_pkt_t *pkt;
		if ( !gc || !WN_GameConnIdentityAccepted( gc->pub_handle,
			gc->allocation_id ) )
			return;
		if ( len > WN_GAME_PKT_MAX ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: cli→srv datagram %d > %d — dropped\n", len, WN_GAME_PKT_MAX );
			return;
		}
		pkt = &gc->recv_queue[gc->recv_head];
		wn_inmem_recv_ring_push( &gc->recv_head, &gc->recv_tail,
			&pkt->from, &pkt->len, pkt->data, data, len, "server" );
		return;
	}
}

static qboolean wn_inmem_recv_unreliable( conn_handle_t *conn_out, byte *buf, int *len_out ) {
	/* This is the inmem transport's vtable recv_unreliable slot (see
	 * inmem_transport below). It is NOT on the live recv path at N=1: recv
	 * pull-iterators stay on the global `transport` pointer (the QUIC vtable),
	 * whose recv_unreliable already drains wtcl_array[0] for every slot. This slot
	 * exists for interface completeness and is reached only if a future per-handle
	 * recv router dispatches an in-process snapshot pull here; the slot-0 drain
	 * below is the body it would run then. */
#if !defined(HEADLESS)
	wn_client_state_t *c = &wtcl_array[0];
	if ( c->initialized && c->recv_tail != c->recv_head ) {
		wn_snap_pkt_t *pkt = &c->recv_queue[c->recv_tail];
		if ( pkt->len <= *len_out ) {
			*conn_out = (conn_handle_t)WN_APP_CONN_BASE;   /* client end of app 0 */
			*len_out  = pkt->len;
			memcpy( buf, pkt->data, (size_t)pkt->len );
			c->recv_tail = ( c->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
			return qtrue;
		}
		c->recv_tail = ( c->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;   /* oversized — discard */
	}
#else
	(void)conn_out; (void)buf; (void)len_out;
#endif
	return qfalse;
}

/* ── Reliable streams ──────────────────────────────────────────────────── */
static qboolean wn_inmem_send_reliable( conn_handle_t conn, int channel,
	const byte *data, int len ) {
	int slot = wn_inmem_client_slot( conn );
	if ( slot < 0 || len <= 0 )
		return qfalse;

	if ( wn_inmem_is_server_end( conn ) ) {
		/* server → client reliable. */
#if !defined(HEADLESS)
		wn_client_state_t *c = &wtcl_array[slot];
		if ( !c->initialized )
			return qfalse;
		if ( channel == CHAN_BOOTSTRAP ) {
			/* Bypass rel_queue → dedicated large buffer (mirror of the QUIC
			 * client bootstrap path). The whole message arrives in one call. */
			if ( c->bootstrap_recv_ready ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: bootstrap already pending — dropped\n" );
				return qfalse;
			}
			if ( len > WN_BOOTSTRAP_MAX ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: bootstrap %d > %d — dropped\n", len, WN_BOOTSTRAP_MAX );
				return qfalse;
			}
			memcpy( c->bootstrap_recv_data, data, (size_t)len );
			c->bootstrap_recv_len   = len;
			c->bootstrap_recv_ready = qtrue;
			return qtrue;
		}
		/* Non-bootstrap reliable messages land in the fixed wn_rel_msg_t.data
		 * (MAX_MSGLEN). Bound the length BEFORE the ring push memcpy — the QUIC
		 * backend rejects an over-length reliable payload (wn_send_reliable);
		 * the ring push itself does not check, so an oversize message (e.g. a
		 * tier-3 snapshot = 9 + MAX_MSGLEN > MAX_MSGLEN) would overflow the slot. */
		if ( len > MAX_MSGLEN ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: srv→cli reliable %d > %d (channel=%d) — dropped\n", len, MAX_MSGLEN, channel );
			return qfalse;
		}
		return wn_reliable_queue_push( c->rel_queue, &c->rel_head,
			&c->rel_tail, channel, data, len );
#endif
		return qfalse;
	}

	if ( wn_inmem_is_client_end( conn ) ) {
		/* client → server reliable. */
		wn_game_conn_t *gc = WN_GameConnByAppHandle( (conn_handle_t)( WN_APP_SVCONN_BASE + slot ) );
		if ( !gc || !WN_GameConnIdentityAccepted( gc->pub_handle,
			gc->allocation_id ) )
			return qfalse;
		if ( channel == CHAN_SESSION ) {
			/* Session-control TLV. The only client→server session TLV is 0x05
			 * READY (client finished loading the gamestate). Mirror the QUIC
			 * handshake's pending_ready enqueue so WN_DrainPendingReady →
			 * ready_callback (SV_OnPlayerReady) transitions the server client
			 * CS_PRIMED → CS_ACTIVE — without it the host stalls at CA_PRIMED. */
			if ( len == 3 && data[0] == 0x05 && data[1] == 0 && data[2] == 0 )
				return WN_QueuePendingReady( gc );
			return qfalse;
		}
		/* CHAN_COMMANDS etc. → the server reliable command ring. Bound the
		 * length BEFORE the ring push memcpy (wn_rel_msg_t.data is MAX_MSGLEN and
		 * wn_reliable_queue_push does not check), mirroring the QUIC backend's
		 * over-length reject in wn_send_reliable. */
		if ( len > MAX_MSGLEN ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem: cli→srv reliable %d > %d (channel=%d) — dropped\n", len, MAX_MSGLEN, channel );
			return qfalse;
		}
		return wn_reliable_queue_push( gc->rel_queue, &gc->rel_head,
			&gc->rel_tail, channel, data, len );
	}
	return qfalse;
}

static qboolean wn_inmem_recv_reliable( conn_handle_t *conn_out, int *channel_out, byte *buf, int *len_out ) {
	/* The inmem transport's vtable recv_reliable slot (see inmem_transport below).
	 * Like recv_unreliable above, it is NOT on the live recv path at N=1 — the
	 * global client reliable path (WN_ClientRecvReliable) already drains
	 * wtcl_array[0]. Kept for interface completeness; reached only if a future
	 * per-handle recv router dispatches an in-process reliable pull here. */
#if !defined(HEADLESS)
	wn_client_state_t *c = &wtcl_array[0];
	if ( c->initialized &&
	     wn_reliable_queue_pop( c->rel_queue, &c->rel_head, &c->rel_tail, channel_out, buf, len_out ) ) {
		*conn_out = (conn_handle_t)WN_APP_CONN_BASE;
		return qtrue;
	}
#else
	(void)conn_out; (void)channel_out; (void)buf; (void)len_out;
#endif
	return qfalse;
}

/* ── Metrics (synthetic — an in-process conn has no network) ────────────── */
static int   wn_inmem_get_ping( conn_handle_t conn ) { (void)conn; return 0; }   /* 0 ms */
static float wn_inmem_get_loss( conn_handle_t conn ) { (void)conn; return 0.0f; } /* lossless */
static int   wn_inmem_get_bandwidth( conn_handle_t conn ) { (void)conn; return 0; }
static void  wn_inmem_get_address_string( conn_handle_t conn, char *buf, int buflen ) {
	(void)conn;
	if ( buf && buflen > 0 )
		Q_strncpyz( buf, "in-mem", buflen );
}

transport_t inmem_transport = {
	/* Lifecycle */
	wn_inmem_shutdown,
	wn_inmem_frame,
	wn_inmem_flush_outbound,

	/* Server */
	wn_inmem_listen,
	NULL,              /* accept_callback — admission runs via the global transport's callback */
	NULL,              /* ready_callback  — unused (synchronous in-process admit) */
	NULL,              /* closed_callback — in-memory teardown is synchronous */
	wn_inmem_complete_admission,
	wn_inmem_drop_client,
	NULL,              /* drain_usercmds  — server drains via WN_ServerRecvUsercmd (shared ring) */
	wn_inmem_lookup_by_addr,

	/* Client */
	wn_inmem_connect,
	wn_inmem_disconnect,
#if !defined(HEADLESS)
	wn_inmem_is_connecting,
	wn_inmem_get_error,
	wn_inmem_clear_error,
#else
	NULL,              /* is_connecting — client-only, NULL in dedicated build */
	NULL,              /* get_error     — client-only, NULL in dedicated build */
	NULL,              /* clear_error   — client-only, NULL in dedicated build */
#endif

	/* Unreliable */
	wn_inmem_send_unreliable,
	wn_inmem_recv_unreliable,

	/* Reliable */
	wn_inmem_send_reliable,
	wn_inmem_recv_reliable,

	/* Metrics */
	wn_inmem_get_ping,
	wn_inmem_get_loss,
	wn_inmem_get_bandwidth,
	wn_inmem_get_address_string,
};
