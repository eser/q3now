// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wn_transport.c — WiredNet game transport over QUIC

Implements the player-facing network path: session handshake, snapshot and
usercmd datagrams, reliable game channels (bootstrap, download, commands,
events).  This is the sole transport — there is no UDP netchan fallback.

conn_handle_t mapping:
  CONN_INVALID          (0)               — no connection
  1..WN_MAX_CLIENTS                        — wn.game_conns[handle - 1]
  CONN_CLIENT_HANDLE    (WN_MAX_CLIENTS+1) — wtcl_array[0] (primary client cnx)
  WN_APP_CONN_BASE..    (100 + app_idx)    — wtcl_array[app_idx], per-app local
                                             client (in-process-queue; no
                                             producer emits these until spawn/P5)

Wire protocols:
  Stream 0x00 — binary TLV session control (CONNECT/ACCEPT/REFUSE/READY)
  Stream 0x03 — game events (msgpack, server→client)
  Stream 0x04+ — game reliable channels (bootstrap/download/commands)
  Datagrams   — snapshots (server→client) and usercmds (client→server)
===========================================================================
*/
#include "wn_local.h"
#include "wn_identity.h"
#include "../../net_transport.h"
#include "../../vm_local.h"     /* MAX_LOCAL_CGAME_VMS — static_assert WN_MAX_LOCAL_CLIENTS matches */

/* The per-app client-slot bound (wn_local.h) must equal the cgame VM table
 * bound so a client app slot and its cgame VM slot index agree. Kept as two
 * constants (no net→vm header dependency) and pinned equal here. */
_Static_assert( WN_MAX_LOCAL_CLIENTS == MAX_LOCAL_CGAME_VMS,
	"WN_MAX_LOCAL_CLIENTS must track MAX_LOCAL_CGAME_VMS" );
LOG_DECLARE_CHANNEL( ch_network, "network" );
LOG_DECLARE_CHANNEL( ch_network_common, "network.common" );

#if !defined(HEADLESS) && !defined(__EMSCRIPTEN__)
#include "picotls/openssl.h"    /* ptls_openssl_verify_certificate_t, override callback */
#include <openssl/sha.h>        /* SHA256 */
#include <openssl/evp.h>        /* X509_get_pubkey, i2d_PUBKEY, EVP_PKEY_free */
#include <openssl/x509.h>       /* X509, STACK_OF(X509) */
#endif

/* ── Global transport pointer (extern declared in net_transport.h) ── */
transport_t *transport = NULL;

/* Sentinel handle for the primary outgoing client connection (wtcl_array[0]). */
#define CONN_CLIENT_HANDLE  ((conn_handle_t)(WN_MAX_CLIENTS + 1))

/* WN_APP_CONN_BASE / WN_APP_CONN_COUNT now live in wn_public.h (promoted so the
 * transport_for_handle selector is visible to all conn-bearing call sites). Pin
 * the public count to the internal per-app bound. */
_Static_assert( WN_APP_CONN_COUNT == WN_MAX_LOCAL_CLIENTS,
	"WN_APP_CONN_COUNT (wn_public.h) must equal WN_MAX_LOCAL_CLIENTS (wn_local.h)" );
_Static_assert( WN_APP_SVCONN_COUNT == WN_MAX_LOCAL_CLIENTS,
	"WN_APP_SVCONN_COUNT (wn_public.h) must equal WN_MAX_LOCAL_CLIENTS (wn_local.h)" );

// ═══════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════

static wn_game_conn_t *wn_get_game_conn( conn_handle_t conn )
{
	wn_game_conn_t *gc;
	if ( conn == CONN_INVALID || conn > WN_MAX_CLIENTS )
		return NULL;
	gc = &wn.game_conns[conn - 1];
	return gc->active ? gc : NULL;
}

#if !defined(HEADLESS)
/* Catch the zero-size-array regression (the `#define wtcl wtcl_array[0]` trap):
 * if the array ever degenerates to [0], sizeof drops to 0. Pin it explicitly.
 * Client-only: wtcl_array / wn_client_state_t are #if !defined(HEADLESS). */
_Static_assert( sizeof( wtcl_array ) == WN_MAX_LOCAL_CLIENTS * sizeof( wtcl_array[0] ),
	"wtcl_array must be WN_MAX_LOCAL_CLIENTS elements (guards the zero-size-array bug)" );

/* Resolve the per-app client transport state for a conn handle. At N=1 the only
 * client handle ever passed is CONN_CLIENT_HANDLE -> slot 0 -> wtcl_array[0],
 * byte-identical to the former singleton. The WN_APP_CONN_BASE range is decoded
 * here for the N-ready path but is never produced at runtime until spawn (P5). */
static wn_client_state_t *wn_get_client_state( conn_handle_t conn )
{
	if ( conn == CONN_CLIENT_HANDLE )
		return &wtcl_array[0];
	if ( conn >= WN_APP_CONN_BASE && conn < WN_APP_CONN_BASE + WN_MAX_LOCAL_CLIENTS )
		return &wtcl_array[conn - WN_APP_CONN_BASE];
	return NULL;
}
#endif

/* Public: decode a client conn_handle to its per-app local-client slot. Lives
 * outside the HEADLESS guard (declared in wn_public.h, may be linked from
 * client recv/parse routing). CONN_CLIENT_HANDLE -> 0; WN_APP_CONN_BASE+i -> i;
 * anything else -> 0. At N=1 only CONN_CLIENT_HANDLE is ever passed -> 0. */
int WN_AppSlotForConn( conn_handle_t conn )
{
	if ( conn >= WN_APP_CONN_BASE && conn < WN_APP_CONN_BASE + WN_MAX_LOCAL_CLIENTS )
		return (int)( conn - WN_APP_CONN_BASE );
	return 0;
}

static picoquic_cnx_t *wn_get_cnx( conn_handle_t conn )
{
#if !defined(HEADLESS)
	if ( conn == CONN_CLIENT_HANDLE || ( conn >= WN_APP_CONN_BASE && conn < WN_APP_CONN_BASE + WN_MAX_LOCAL_CLIENTS ) ) {
		wn_client_state_t *c = wn_get_client_state( conn );
		return ( c && c->initialized && c->cnx ) ? c->cnx : NULL;
	}
#endif
	{
		wn_game_conn_t *gc = wn_get_game_conn( conn );
		return ( gc && gc->conn ) ? gc->conn->cnx : NULL;
	}
}

static qboolean wn_reliable_channel_allows_fixed_stream( int channel )
{
	return channel == CHAN_SESSION;
}

#define WN_GAME_REL_VERSION 1

static qboolean wn_reliable_stream_is_client_owned( uint64_t stream_id )
{
	return ( stream_id & 0x01 ) == 0;
}

static int wn_reliable_channel_direction_matches( int channel, qboolean sending_from_client )
{
	switch ( channel ) {
	case CHAN_SESSION:
		return qtrue;
	case CHAN_COMMANDS:
		return sending_from_client;
	case CHAN_BOOTSTRAP:
	case CHAN_DOWNLOAD:
	case CHAN_EVENTS:
	case CHAN_SNAPSHOT_RELIABLE:
		return !sending_from_client;   /* srv→cli only */
	case CHAN_MCP:
		return qtrue;                  /* bidi: both sides may send */
	default:
		return qfalse;
	}
}

static uint64_t wn_resolve_reliable_send_stream( picoquic_cnx_t *cnx, int channel,
	qboolean sending_from_client )
{
	if ( wn_reliable_channel_allows_fixed_stream( channel ) ) {
		return (uint64_t)channel;
	}
	if ( !wn_reliable_channel_direction_matches( channel, sending_from_client ) ) {
		return UINT64_MAX;
	}
	return picoquic_get_next_local_stream_id( cnx,
		/* unidirectional channels — no reverse data expected */
		(channel == CHAN_COMMANDS || channel == CHAN_EVENTS ||
		 channel == CHAN_SNAPSHOT_RELIABLE) ? 1 : 0 );
}

static qboolean wn_reliable_channel_is_game( int channel )
{
	switch ( channel ) {
	case CHAN_BOOTSTRAP:
	case CHAN_DOWNLOAD:
	case CHAN_COMMANDS:
	case CHAN_EVENTS:
	case CHAN_SNAPSHOT_RELIABLE:
	case CHAN_MCP:
		return qtrue;
	default:
		return qfalse;
	}
}

/*
 * Stage the 2-byte reliable header across arbitrarily-chunked picoquic stream
 * deliveries. The first chunk of a stream may carry 0, 1, or 2+ header bytes.
 * This helper consumes up to 2 bytes from (*data_io, *len_io) into the
 * partial slot's staging buffer, advances the pointers past the consumed
 * bytes, and — once the header is complete — validates and stores the
 * channel into partial->channel.
 *
 * Returns:
 *   qtrue  — header bytes consumed cleanly; caller should continue with the
 *            remaining (*data_io, *len_io) as payload. If partial->channel
 *            is still -1 on return, the header is not yet complete (needs
 *            more chunks), and the caller should return without dispatching.
 *   qfalse — header is complete but malformed; caller must free the partial
 *            slot and drop the stream.
 */
static qboolean wn_reliable_stage_header( wn_rel_partial_t *partial,
	const byte **data_io, int *len_io, uint64_t stream_id, const char *tag )
{
	while ( partial->header_bytes < 2 && *len_io > 0 ) {
		partial->header_staging[partial->header_bytes++] = (*data_io)[0];
		(*data_io)++;
		(*len_io)--;
	}
	Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC %s: stream %llu header staged %d/2 bytes\n",
		tag, (unsigned long long)stream_id, partial->header_bytes );
	if ( partial->header_bytes < 2 ) {
		return qtrue; /* not yet complete — wait for next chunk */
	}
	if ( partial->channel >= 0 ) {
		return qtrue; /* already parsed on a previous call */
	}
	if ( partial->header_staging[0] != WN_GAME_REL_VERSION ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC %s: invalid reliable header on stream %llu "
			"(version=%d expected %d) — dropped\n",
			tag, (unsigned long long)stream_id,
			(int)partial->header_staging[0], (int)WN_GAME_REL_VERSION );
		return qfalse;
	}
	if ( !wn_reliable_channel_is_game( partial->header_staging[1] ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC %s: invalid reliable header on stream %llu "
			"(channel=%d) — dropped\n",
			tag, (unsigned long long)stream_id,
			(int)partial->header_staging[1] );
		return qfalse;
	}
	partial->channel = partial->header_staging[1];
	Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC %s: stream %llu header complete chan=%d\n",
		tag, (unsigned long long)stream_id, partial->channel );
	return qtrue;
}

static wn_rel_partial_t *wn_reliable_find_partial( wn_rel_partial_t *partials, uint64_t stream_id )
{
	int i;
	for ( i = 0; i < WN_REL_PARTIAL_CAP; i++ ) {
		if ( partials[i].active && partials[i].stream_id == stream_id ) {
			return &partials[i];
		}
	}
	return NULL;
}

static wn_rel_partial_t *wn_reliable_alloc_partial( wn_rel_partial_t *partials,
	uint64_t stream_id )
{
	int i;
	for ( i = 0; i < WN_REL_PARTIAL_CAP; i++ ) {
		if ( !partials[i].active ) {
			memset( &partials[i], 0, sizeof( partials[i] ) );
			partials[i].active       = qtrue;
			partials[i].stream_id    = stream_id;
			partials[i].channel      = -1; /* filled in once header is staged */
			partials[i].header_bytes = 0;
			return &partials[i];
		}
	}
	return NULL;
}

static void wn_reliable_free_partial( wn_rel_partial_t *partial )
{
	if ( partial ) {
		memset( partial, 0, sizeof( *partial ) );
	}
}

qboolean wn_reliable_queue_push( wn_rel_msg_t *queue, volatile int *head,
	volatile int *tail, int channel, const byte *data, int len )
{
	int next_head = ( *head + 1 ) % WN_REL_QUEUE_SIZE;
	wn_rel_msg_t *msg;
	if ( next_head == *tail ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC: reliable recv queue full — message dropped (channel=%d len=%d)\n", channel, len );
		return qfalse;
	}
	// Last-line-of-defense bound: queue slots use a fixed MAX_MSGLEN data
	// buffer and `len` is derived from untrusted network framing. Every caller
	// is expected to pre-check (the in-mem + QUIC paths do), but reject an
	// out-of-range length here too rather than overflowing the queue slot if a
	// caller ever forgets. A negative len would also wrap to a huge size_t in
	// the memcpy.
	if ( len < 0 || (size_t)len > sizeof( queue[*head].data ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC: reliable push rejected oversize len=%d (max=%d, channel=%d)\n",
			len, (int)sizeof( queue[*head].data ), channel );
		return qfalse;
	}
	msg = &queue[*head];
	msg->channel = channel;
	msg->len     = len;
	memcpy( msg->data, data, (size_t)len );
	*head = next_head;
	return qtrue;
}

qboolean wn_reliable_queue_pop( wn_rel_msg_t *queue, volatile int *head,
	volatile int *tail, int *channel_out, byte *buf, int *len_out )
{
	wn_rel_msg_t *msg;
	if ( *tail == *head ) {
		return qfalse;
	}
	msg = &queue[*tail];
	if ( msg->len > *len_out ) {
		COM_ERROR( LOG_CH(ch_network), "QUIC: wn_reliable_queue_pop: dropping oversize message "
			"(chan=%d len=%d > buf=%d) — increase caller buffer\n",
			msg->channel, msg->len, *len_out );
		*tail = ( *tail + 1 ) % WN_REL_QUEUE_SIZE;
		return qfalse;
	}
	*channel_out = msg->channel;
	*len_out     = msg->len;
	memcpy( buf, msg->data, (size_t)msg->len );
	*tail = ( *tail + 1 ) % WN_REL_QUEUE_SIZE;
	return qtrue;
}

static void wn_reliable_server_consume_stream( wn_game_conn_t *gc, uint64_t stream_id,
	const byte *data, int len, qboolean fin )
{
	wn_rel_partial_t *partial;
	const byte *payload = data;
	int payload_len = len;

	/* picoquic delivers stream data in arbitrarily-sized chunks via repeated
	 * picoquic_callback_stream_data events. The 2-byte reliable header
	 * [version:1][channel:1] sits at the very start of the stream; a chunk
	 * may carry 0, 1, 2, or more header bytes. We stage header bytes across
	 * chunks until complete, then route the rest as payload. */
	partial = wn_reliable_find_partial( gc->rel_partials, stream_id );
	if ( !partial ) {
		partial = wn_reliable_alloc_partial( gc->rel_partials, stream_id );
		if ( !partial ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: no partial slots for reliable stream %llu\n",
				(unsigned long long)stream_id );
			return;
		}
		Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC game: partial slot alloc for stream %llu first_chunk=%d bytes\n",
			(unsigned long long)stream_id, len );
	}
	if ( !wn_reliable_stage_header( partial, &payload, &payload_len, stream_id, "game" ) ) {
		wn_reliable_free_partial( partial );
		return;
	}
	if ( partial->channel < 0 ) {
		/* Header still staging; nothing to append yet. If the stream already
		 * closed (fin), warn and free — we can't dispatch without a channel. */
		if ( fin ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: stream %llu closed with incomplete header "
				"(%d/2 bytes) — dropped\n",
				(unsigned long long)stream_id, partial->header_bytes );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( gc->conn->cnx, stream_id );
		}
		return;
	}
	if ( partial->len + payload_len > MAX_MSGLEN ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: reliable stream %llu overflow — dropped (%d+%d > %d)\n",
			(unsigned long long)stream_id, partial->len, payload_len, MAX_MSGLEN );
		wn_reliable_free_partial( partial );
		return;
	}
	if ( payload_len > 0 ) {
		memcpy( partial->data + partial->len, payload, (size_t)payload_len );
		partial->len += payload_len;
		Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC game: partial slot for stream %llu: %d bytes (fin=%d)\n",
			(unsigned long long)stream_id, partial->len, (int)fin );
	}
	if ( fin ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network_common), "QUIC game: partial slot for stream %llu: %d bytes COMPLETE (chan=%d)\n",
			(unsigned long long)stream_id, partial->len, partial->channel );
		if ( !wn_reliable_queue_push( gc->rel_queue, &gc->rel_head, &gc->rel_tail,
			partial->channel, partial->data, partial->len ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: reliable recv queue full for stream %llu\n",
				(unsigned long long)stream_id );
		}
		wn_reliable_free_partial( partial );
		picoquic_reset_stream_ctx( gc->conn->cnx, stream_id );
	}
}

#if !defined(HEADLESS)
static void wn_reliable_client_consume_stream( uint64_t stream_id, const byte *data,
	int len, qboolean fin )
{
	wn_rel_partial_t *partial;
	const byte *payload = data;
	int payload_len = len;

	/* See wn_reliable_server_consume_stream for rationale: the 2-byte header
	 * may arrive across multiple chunks. Stage header bytes, then append. */
	partial = wn_reliable_find_partial( wtcl_array[0].rel_partials, stream_id );
	if ( !partial ) {
		partial = wn_reliable_alloc_partial( wtcl_array[0].rel_partials, stream_id );
		if ( !partial ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: no partial slots for stream %llu\n",
				(unsigned long long)stream_id );
			return;
		}
		Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC client: partial slot alloc for stream %llu first_chunk=%d bytes\n",
			(unsigned long long)stream_id, len );
	}
	if ( !wn_reliable_stage_header( partial, &payload, &payload_len, stream_id, "client" ) ) {
		wn_reliable_free_partial( partial );
		return;
	}
	if ( partial->channel < 0 ) {
		/* Header still staging; nothing to append yet. If the stream already
		 * closed (fin), warn and free — we can't dispatch without a channel. */
		if ( fin ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: stream %llu closed with incomplete header "
				"(%d/2 bytes) — dropped\n",
				(unsigned long long)stream_id, partial->header_bytes );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl_array[0].cnx, stream_id );
		}
		return;
	}
	/* CHAN_BOOTSTRAP exceeds MAX_MSGLEN — route to the dedicated large buffer. */
	if ( partial->channel == CHAN_BOOTSTRAP ) {
		if ( wtcl_array[0].bootstrap_recv_ready ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: bootstrap already pending; stream %llu dropped\n",
				(unsigned long long)stream_id );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl_array[0].cnx, stream_id );
			return;
		}
		if ( wtcl_array[0].bootstrap_recv_len + payload_len > WN_BOOTSTRAP_MAX ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: bootstrap stream %llu overflow — dropped\n",
				(unsigned long long)stream_id );
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl_array[0].cnx, stream_id );
			return;
		}
		if ( payload_len > 0 ) {
			memcpy( wtcl_array[0].bootstrap_recv_data + wtcl_array[0].bootstrap_recv_len,
				payload, (size_t)payload_len );
			wtcl_array[0].bootstrap_recv_len += payload_len;
		}
		if ( fin ) {
			wtcl_array[0].bootstrap_recv_ready = qtrue;
			wn_reliable_free_partial( partial );
			picoquic_reset_stream_ctx( wtcl_array[0].cnx, stream_id );
		}
		return;
	}
	if ( partial->len + payload_len > MAX_MSGLEN ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: reliable stream %llu overflow — dropped (%d+%d > %d)\n",
			(unsigned long long)stream_id, partial->len, payload_len, MAX_MSGLEN );
		wn_reliable_free_partial( partial );
		return;
	}
	if ( payload_len > 0 ) {
		memcpy( partial->data + partial->len, payload, (size_t)payload_len );
		partial->len += payload_len;
		Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC client: partial slot for stream %llu: %d bytes (fin=%d)\n",
			(unsigned long long)stream_id, partial->len, (int)fin );
	}
	if ( fin ) {
		Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC client: partial slot for stream %llu: %d bytes COMPLETE (chan=%d)\n",
			(unsigned long long)stream_id, partial->len, partial->channel );
		if ( !wn_reliable_queue_push( wtcl_array[0].rel_queue, &wtcl_array[0].rel_head, &wtcl_array[0].rel_tail,
			partial->channel, partial->data, partial->len ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: reliable recv queue full for stream %llu\n",
				(unsigned long long)stream_id );
		}
		wn_reliable_free_partial( partial );
		picoquic_reset_stream_ctx( wtcl_array[0].cnx, stream_id );
	}
}
#endif

static wn_game_conn_t *WN_GameFindConnForAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->conn || !WN_GameConnIdentityAccepted( gc->pub_handle,
			gc->allocation_id ) )
			continue;
		{
			const netadr_t *a = &gc->conn->addr;
			if ( NET_IS_IPV6( a->type ) && NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._6, addr->ipv._6, 16 ) == 0 )
					return gc;
			} else if ( !NET_IS_IPV6( a->type ) && !NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._4, addr->ipv._4, 4 ) == 0 )
					return gc;
			}
		}
	}
	return NULL;
}


// ═══════════════════════════════════════════════════════════════════
// Binary TLV helpers — Stream 0 session channel
// ═══════════════════════════════════════════════════════════════════

/*
 * TLV_Write — encode one TLV message into buf.
 * Layout: [type:u8][plen:u16le][payload:plen bytes]
 * Returns total bytes written, or 0 if buf is too small.
 */
static int TLV_Write( byte *buf, int bufsize, uint8_t type,
                       const byte *payload, uint16_t plen )
{
	int total = 3 + (int)plen;
	if ( bufsize < total )
		return 0;
	buf[0] = type;
	buf[1] = (byte)( plen & 0xFF );
	buf[2] = (byte)( (plen >> 8) & 0xFF );
	if ( plen > 0 && payload )
		memcpy( buf + 3, payload, plen );
	return total;
}

/*
 * TLV_Read — parse one TLV message from data.
 * Sets *type_out, *payload_out (pointer into data), *plen_out.
 * Returns qfalse when data is too short to hold a complete message.
 */
static qboolean TLV_Read( const byte *data, int len,
                           uint8_t *type_out, const byte **payload_out,
                           uint16_t *plen_out )
{
	uint16_t plen;
	if ( len < 3 )
		return qfalse;
	plen = (uint16_t)( data[1] | ( (uint16_t)data[2] << 8 ) );
	if ( len < 3 + (int)plen )
		return qfalse;
	*type_out    = data[0];
	*plen_out    = plen;
	*payload_out = data + 3;
	return qtrue;
}


// ═══════════════════════════════════════════════════════════════════
// Game conn lifecycle
// ═══════════════════════════════════════════════════════════════════

static uint64_t WN_NextGameConnAllocationId( void ) {
	wn.next_game_conn_allocation_id++;
	if ( wn.next_game_conn_allocation_id == 0 )
		wn.next_game_conn_allocation_id++;
	return wn.next_game_conn_allocation_id;
}

wn_game_conn_t *WN_GameConnByIdentity( conn_handle_t conn,
	uint64_t allocation_id ) {
	int i;
	if ( allocation_id == 0 ) return NULL;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( WN_AllocationIdentityMatches( gc->active,
			(uint64_t)gc->pub_handle, gc->allocation_id,
			(uint64_t)conn, allocation_id ) )
			return gc;
	}
	return NULL;
}

qboolean WN_GameConnIdentityAccepted( conn_handle_t conn,
	uint64_t allocation_id ) {
	wn_game_conn_t *gc = WN_GameConnByIdentity( conn, allocation_id );
	qboolean accepted;
	if ( !gc ) return qfalse;
	accepted = gc->hs_state == WN_GAME_HS_ACCEPTED
		&& ( !gc->conn || gc->conn->admission_terminal == 1 );
	return WN_AcceptedAllocationIdentityMatches( gc->active, accepted,
		(uint64_t)gc->pub_handle, gc->allocation_id,
		(uint64_t)conn, allocation_id ) ? qtrue : qfalse;
}

static qboolean WN_QueuePendingConnect( wn_game_conn_t *gc,
	const char *userinfo ) {
	int i;
	if ( !gc || !gc->active || gc->allocation_id == 0 ) return qfalse;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_pending_connect_t *pending = &wn.pending_connects[i];
		if ( pending->pending ) continue;
		Q_SecureZeroMemory( pending, sizeof( *pending ) );
		pending->conn = gc->pub_handle;
		pending->allocation_id = gc->allocation_id;
		Q_strncpyz( pending->userinfo, userinfo ? userinfo : "",
			sizeof( pending->userinfo ) );
		pending->pending = qtrue;
		return qtrue;
	}
	return qfalse;
}

qboolean WN_QueuePendingReady( wn_game_conn_t *gc ) {
	wn_pending_ready_t *pending;
	int slot;
	if ( !gc || !gc->active || gc->allocation_id == 0 ) return qfalse;
	slot = (int)( gc - wn.game_conns );
	if ( slot < 0 || slot >= WN_MAX_CLIENTS ) return qfalse;
	pending = &wn.pending_ready[slot];
	if ( pending->pending ) {
		if ( pending->conn == gc->pub_handle
		     && pending->allocation_id == gc->allocation_id )
			return qtrue;
		Q_SecureZeroMemory( pending, sizeof( *pending ) );
	}
	pending->conn = gc->pub_handle;
	pending->allocation_id = gc->allocation_id;
	pending->pending = qtrue;
	return qtrue;
}

wn_game_conn_t *WN_GameAllocConn( wn_connection_t *conn )
{
	int i;
	int max_clients = wn.sv_wirednetMaxClients ? wn.sv_wirednetMaxClients->integer : WN_MAX_CLIENTS;
	if ( max_clients > WN_MAX_CLIENTS ) max_clients = WN_MAX_CLIENTS;

	/* E5: Enforce sv_wirednetMaxClients before allocating a slot. */
	if ( wn.num_game_conns >= max_clients ) {
		COM_WARN( LOG_CH(ch_network), "QUIC game: connection limit (%d) reached, refusing\n",
		            max_clients );
		return NULL;
	}

	for ( i = 0; i < max_clients; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->active ) {
			Q_SecureZeroMemory( gc, sizeof( *gc ) );
			gc->active   = qtrue;
			gc->allocation_id = WN_NextGameConnAllocationId();
			gc->conn     = conn;
			gc->hs_state = WN_GAME_HS_NONE;
			/* QUIC publishes slot+1 (1..WN_MAX_CLIENTS) — byte-identical to the
			 * legacy i+1 the usercmd drain used to synthesize. An in-process app
			 * conn (WN_GameAllocConnApp) overrides pub_handle to the server end
			 * WN_APP_SVCONN_BASE+slot (110+). */
			// NOLINTNEXTLINE(bugprone-misplaced-widening-cast) — small slot index widened to conn_handle_t; no precision loss
			gc->pub_handle = (conn_handle_t)( i + 1 );
			wn.num_game_conns++;
			conn->game_conn = gc;
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: allocated conn slot %d for %s\n",
				i, NET_AdrToStringwPort( &conn->addr ) );
			return gc;
		}
	}
	COM_WARN( LOG_CH(ch_network), "QUIC game: no free game_conn slots\n" );
	return NULL;
}

void WN_GameFreeConn( wn_game_conn_t *gc )
{
	conn_handle_t handle;
	uint64_t allocation_id;
	int i;
	if ( !gc || !gc->active )
		return;
	handle = gc->pub_handle;
	allocation_id = gc->allocation_id;
	if ( gc->conn )
		gc->conn->game_conn = NULL;
	/* A queued main-thread event must never survive reuse of this slot/handle. */
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.pending_connects[i].pending
		     && wn.pending_connects[i].conn == handle
		     && wn.pending_connects[i].allocation_id == allocation_id )
			Q_SecureZeroMemory( &wn.pending_connects[i],
				sizeof( wn.pending_connects[i] ) );
		if ( wn.pending_ready[i].pending
		     && wn.pending_ready[i].conn == handle
		     && wn.pending_ready[i].allocation_id == allocation_id )
			Q_SecureZeroMemory( &wn.pending_ready[i],
				sizeof( wn.pending_ready[i] ) );
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WiredNet game: freed conn slot %td\n", gc - wn.game_conns );
	Q_SecureZeroMemory( gc, sizeof( *gc ) );
	if ( wn.num_game_conns > 0 )
		wn.num_game_conns--;
}

/*
==================
WN_GameAllocConnApp — server-side game_conn for an in-process app (no QUIC).

Mirror of WN_GameAllocConn for the in-memory transport: allocates a
wn.game_conns[] slot with conn=NULL (no owning picoquic connection) and stamps
pub_handle = WN_APP_CONN_BASE+app_slot so the server's svs.clients[].quic_conn
match and the outbound transport_for_handle() routing both resolve to the
in-memory backend for this conn. The recv_queue/rel_queue rings are otherwise
identical to a QUIC game_conn — WN_ServerRecvUsercmd drains them unchanged.
==================
*/
wn_game_conn_t *WN_GameAllocConnApp( int app_slot )
{
	int i;
	int max_clients = wn.sv_wirednetMaxClients ? wn.sv_wirednetMaxClients->integer : WN_MAX_CLIENTS;
	if ( max_clients > WN_MAX_CLIENTS ) max_clients = WN_MAX_CLIENTS;

	if ( wn.num_game_conns >= max_clients ) {
		COM_WARN( LOG_CH(ch_network), "in-mem game: connection limit (%d) reached, refusing\n", max_clients );
		return NULL;
	}
	for ( i = 0; i < max_clients; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->active ) {
			Q_SecureZeroMemory( gc, sizeof( *gc ) );
			gc->active     = qtrue;
			gc->allocation_id = WN_NextGameConnAllocationId();
			gc->conn       = NULL;   /* in-process: no owning picoquic connection */
			gc->hs_state   = WN_GAME_HS_PENDING;
			gc->pub_handle = (conn_handle_t)( WN_APP_SVCONN_BASE + app_slot );
			wn.num_game_conns++;
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "in-mem game: allocated conn slot %d (handle %llu) for app %d\n",
				i, (unsigned long long)gc->pub_handle, app_slot );
			return gc;
		}
	}
	COM_WARN( LOG_CH(ch_network), "in-mem game: no free game_conn slots\n" );
	return NULL;
}

/*
==================
WN_GameConnByAppHandle — find the server-side game_conn for a 100+ app handle.

The in-memory send path (client→server usercmds) and drop path need to locate
the game_conn whose pub_handle matches the app handle. Linear scan of
wn.game_conns[]; returns NULL if no active match.
==================
*/
wn_game_conn_t *WN_GameConnByAppHandle( conn_handle_t conn )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.game_conns[i].active && wn.game_conns[i].pub_handle == conn )
			return &wn.game_conns[i];
	}
	return NULL;
}

/*
==================
WN_ConnectApp — in-process app connect producer (in-memory transport).

The first emitter of a WN_APP_CONN_BASE+slot handle. Same-process, so it wires
BOTH ends synchronously:
  (client) initialize wtcl_array[app_slot] as a connected client end — no
           picoquic, synthetic NA_LOOPBACK server_addr. Its recv_queue/rel_queue
           are fed by the server's in-mem send path; drained by the existing
           client recv vtable (which shares wtcl_array[app_slot]).
  (server) allocate a game_conns[] slot (WN_GameAllocConnApp) and enqueue a
           pending connect so WN_DrainPendingConnects → SV_OnPlayerConnect admits
           the app exactly like a QUIC client.
Returns the public handle (WN_APP_CONN_BASE+app_slot), or CONN_INVALID on failure.
==================
*/
conn_handle_t WN_ConnectApp( int app_slot, const char *userinfo )
{
	wn_game_conn_t *gc;
	conn_handle_t   handle;

	if ( app_slot < 0 || app_slot >= WN_MAX_LOCAL_CLIENTS ) {
		COM_WARN( LOG_CH(ch_network), "WN_ConnectApp: app_slot %d out of range\n", app_slot );
		return CONN_INVALID;
	}

#if !defined(HEADLESS)
	/* Client end — mirror the wtcl_array init WN_ClientConnect does, minus
	 * picoquic. Synthetic NA_LOOPBACK server_addr so the integrated host reads
	 * as a same-process loopback peer on the server side (SV_IsHostClient). */
	if ( wtcl_array[app_slot].initialized ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WN_ConnectApp: app %d already connected\n", app_slot );
		return (conn_handle_t)( WN_APP_CONN_BASE + app_slot );
	}
	Q_SecureZeroMemory( &wtcl_array[app_slot], sizeof( wtcl_array[app_slot] ) );
	wtcl_array[app_slot].connect_failed  = qfalse;
	wtcl_array[app_slot].connect_error[0] = '\0';
	wtcl_array[app_slot].quic              = NULL;   /* in-process: no picoquic */
	wtcl_array[app_slot].cnx               = NULL;
	wtcl_array[app_slot].server_addr.type  = NA_LOOPBACK;
	wtcl_array[app_slot].initialized       = qtrue;
#endif

	/* Server end — allocate a game_conn slot (pub_handle = server-end 110+slot)
	 * and enqueue admission. SV_OnPlayerConnect stores that handle in
	 * svs.clients[].quic_conn, so the server's outbound transport_for_handle()
	 * routes back through inmem_transport. */
	gc = WN_GameAllocConnApp( app_slot );
	if ( !gc ) {
#if !defined(HEADLESS)
		Q_SecureZeroMemory( &wtcl_array[app_slot], sizeof( wtcl_array[app_slot] ) );
#endif
		return CONN_INVALID;
	}
	handle = gc->pub_handle;        /* server end (110+slot) — for admission */
	gc->handshake_stream_id = 0;    /* unused for in-process */

	/* Enqueue pending connect for main-thread consumption (same queue + drain
	 * as QUIC; SV_OnPlayerConnect is transport-agnostic). The server-end handle
	 * is what the server admits + stores. */
	if ( !WN_QueuePendingConnect( gc, userinfo ) ) {
		COM_WARN( LOG_CH(ch_network), "WN_ConnectApp: pending_connects overflow — app %d dropped\n", app_slot );
		WN_GameFreeConn( gc );
#if !defined(HEADLESS)
		Q_SecureZeroMemory( &wtcl_array[app_slot], sizeof( wtcl_array[app_slot] ) );
#endif
		return CONN_INVALID;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_network), "in-mem: app %d connected (client handle %llu, server handle %llu)\n",
		app_slot, (unsigned long long)( WN_APP_CONN_BASE + app_slot ), (unsigned long long)handle );

	/* Return the CLIENT-end handle (100+slot) — what the client stores in
	 * clc.quic_conn and what WN_AppSlotForConn decodes back to this app. */
	return (conn_handle_t)( WN_APP_CONN_BASE + app_slot );
}


// ═══════════════════════════════════════════════════════════════════
// Datagram handling
// ═══════════════════════════════════════════════════════════════════

void WN_GameHandleDatagram( wn_connection_t *conn, const byte *data, int len )
{
	wn_game_conn_t *gc;
	int             next_head;
	wn_game_pkt_t  *pkt;

	if ( len <= 0 || len > WN_GAME_PKT_MAX ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: datagram length %d out of range — dropped\n", len );
		return;
	}
	gc = conn ? conn->game_conn : NULL;
	if ( !gc || gc->conn != conn
	     || !WN_GameConnIdentityAccepted( gc->pub_handle, gc->allocation_id ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: datagram from non-game connection — dropped\n" );
		return;
	}
	next_head = ( gc->recv_head + 1 ) % WN_GAME_QUEUE_SIZE;
	if ( next_head == gc->recv_tail ) {
		wn.dropped_packets++;
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: recv queue full for %s — dropped\n",
			NET_AdrToStringwPort( &conn->addr ) );
		return;
	}
	pkt           = &gc->recv_queue[gc->recv_head];
	pkt->from     = conn->addr;
	pkt->from.type = ( conn->addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
	pkt->len      = len;
	memcpy( pkt->data, data, len );
	gc->recv_head = next_head;
}


// ═══════════════════════════════════════════════════════════════════
// Handshake — Stream 0x00 (binary TLV only)
// ═══════════════════════════════════════════════════════════════════

static void WN_GameSendAdmission( wn_connection_t *conn, wn_game_conn_t *gc,
	qboolean accepted, netRefuseClass_t refusalClass ) {
	byte payload[4];
	byte tlv[16];
	int tlvLen;
	int sendResult = -1;

	if ( !conn || !conn->cnx ) return;
	if ( conn->admission_terminal != 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_network),
			"QUIC game: duplicate admission send terminal_close=1\n" );
		conn->session_failed = qtrue;
#if !defined(__EMSCRIPTEN__)
		picoquic_close( conn->cnx, 1 );
#endif
		return;
	}
	if ( accepted ) {
		int slot;
		if ( !gc || gc->hs_state != WN_GAME_HS_PENDING ) return;
		slot = (int)( gc - wn.game_conns );
		payload[0] = (byte)slot;
		payload[1] = 20;
		payload[2] = 0;
		payload[3] = 0;
		tlvLen = TLV_Write( tlv, (int)sizeof( tlv ), 0x02, payload, 4 );
	} else {
		payload[0] = (byte)refusalClass;
		tlvLen = TLV_Write( tlv, (int)sizeof( tlv ), 0x03, payload, 1 );
	}
	if ( tlvLen > 0 ) {
		sendResult = picoquic_add_to_stream( conn->cnx, WIREDNET_SESSION_STREAM_ID,
			tlv, (size_t)tlvLen, 0 );
	}
	Q_SecureZeroMemory( payload, sizeof( payload ) );
	Q_SecureZeroMemory( tlv, sizeof( tlv ) );
	if ( tlvLen <= 0 || sendResult != 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_network),
			"QUIC game: admission send failed result=%d terminal_close=1\n",
			sendResult );
		conn->session_failed = qtrue;
#if !defined(__EMSCRIPTEN__)
		picoquic_close( conn->cnx, 1 );
#endif
		if ( !accepted && gc )
			WN_GameFreeConn( gc );
		return;
	}
	if ( gc )
		gc->hs_state = accepted ? WN_GAME_HS_ACCEPTED : WN_GAME_HS_REFUSED;
	conn->admission_terminal = accepted ? 1 : 2;
	Com_Log( SEV_DEBUG, LOG_CH(ch_network),
		"QUIC game: admission result=%s class=%d\n",
		accepted ? "accepted" : "refused", accepted ? 0 : (int)refusalClass );
	/* WN_GameSendAdmission can run inside picoquic's receive callback. Do not
	 * re-enter the packet pump here: the normal post-receive/server-frame flush
	 * sends REFUSE, then WN_FlushOutbound retires the recyclable game slot. */
	if ( !accepted ) conn->refusal_teardown_pending = qtrue;
}

static void WN_GameSessionFail( wn_connection_t *conn, int failureClass ) {
	if ( !conn || conn->session_failed ) return;
	conn->session_failed = qtrue;
	Q_SecureZeroMemory( conn->session_recv_data,
		sizeof( conn->session_recv_data ) );
	conn->session_recv_len = 0;
	Com_Log( SEV_WARN, LOG_CH(ch_network),
		"QUIC game: malformed session stream class=%d terminal_close=1\n",
		failureClass );
#if !defined(__EMSCRIPTEN__)
	if ( conn->cnx ) picoquic_close( conn->cnx, 1 );
#endif
}

static void WN_GameHandleHandshakeMessage( wn_connection_t *conn,
	uint64_t stream_id, const byte *data, int len )
{
	uint8_t         tlv_type;
	const byte     *payload;
	uint16_t        plen;
	wn_game_conn_t *gc;

	if ( !conn || !conn->active )
		return;

	/* Binary TLV path */
	if ( !TLV_Read( data, len, &tlv_type, &payload, &plen ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: incomplete TLV on handshake stream — ignored\n" );
		return;
	}

	if ( tlv_type == 0x05 ) { /* READY: client has processed gamestate, ready to enter world */
		wn_game_conn_t *gc = conn->game_conn;
		if ( plen != 0 || !gc || !gc->active
		     || gc->hs_state != WN_GAME_HS_ACCEPTED
		     || conn->admission_terminal != 1 ) {
			WN_GameSessionFail( conn, 5 );
			return;
		}
		if ( !WN_QueuePendingReady( gc ) ) {
			WN_GameSessionFail( conn, 10 );
			return;
		}
		Com_Log( SEV_DEBUG, LOG_CH(ch_network),
			"QUIC game: TLV READY allocation=%llu — enqueued\n",
			(unsigned long long)gc->allocation_id );
		return;
	}

	if ( tlv_type != 0x01 ) {
		WN_GameSessionFail( conn, 1 );
		return;
	}
	if ( conn->admission_terminal != 0 ) {
		WN_GameSessionFail( conn, 9 );
		return;
	}

	/* TLV 0x01 CONNECT: version:u16le, userinfo_len:u16le, userinfo:bytes */
	if ( plen < 4 ) {
		WN_GameSessionFail( conn, 2 );
		return;
	}
	{
		char     userinfo[MAX_INFO_STRING];
		uint16_t userinfo_len;
		int      slot;

		/* version = payload[0..1] (currently ignored — validated later) */
		userinfo_len = (uint16_t)( payload[2] | ( (uint16_t)payload[3] << 8 ) );
		if ( payload[0] != 0x00 || payload[1] != 0x01
		     || userinfo_len != plen - 4
		     || userinfo_len >= (uint16_t)sizeof( userinfo ) ) {
			WN_GameSessionFail( conn, 3 );
			return;
		}
		memcpy( userinfo, payload + 4, userinfo_len );
		userinfo[userinfo_len] = '\0';

		gc = conn->game_conn;
		if ( !gc ) {
			gc = WN_GameAllocConn( conn );
			if ( !gc ) {
				WN_GameSendAdmission( conn, NULL, qfalse, NET_REFUSE_SERVER_FULL );
				Q_SecureZeroMemory( userinfo, sizeof( userinfo ) );
				return;
			}
		}
		if ( gc->hs_state != WN_GAME_HS_NONE ) {
			Q_SecureZeroMemory( userinfo, sizeof( userinfo ) );
			WN_GameSessionFail( conn, 4 );
			return;
		}

		gc->handshake_stream_id = WIREDNET_SESSION_STREAM_ID;
		gc->hs_state            = WN_GAME_HS_PENDING;

		slot        = (int)( gc - wn.game_conns );
		/* Enqueue pending connect with the allocation identity. */
		if ( !WN_QueuePendingConnect( gc, userinfo ) ) {
			COM_WARN( LOG_CH(ch_network),
				"QUIC game: pending_connects overflow — TLV CONNECT from %s dropped\n",
				NET_AdrToStringwPort( &conn->addr ) );
			WN_GameSendAdmission( conn, gc, qfalse, NET_REFUSE_GENERIC );
			Q_SecureZeroMemory( userinfo, sizeof( userinfo ) );
			return;
		}
		Q_SecureZeroMemory( userinfo, sizeof( userinfo ) );
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC game: TLV CONNECT from %s → slot %d, VM admission pending\n",
			NET_AdrToStringwPort( &conn->addr ), slot );
	}
}

void WN_GameHandleHandshake( wn_connection_t *conn, uint64_t stream_id,
	const byte *data, int len, qboolean fin ) {
	if ( !conn || !conn->active || conn->session_failed ) return;
	if ( len < 0 || len > WN_SESSION_BUFFER_MAX - conn->session_recv_len ) {
		WN_GameSessionFail( conn, 6 );
		return;
	}
	if ( len > 0 ) {
		memcpy( conn->session_recv_data + conn->session_recv_len, data,
			(size_t)len );
		conn->session_recv_len += len;
	}
	while ( conn->session_recv_len >= 3 ) {
		int payloadLen = conn->session_recv_data[1]
			| ( (int)conn->session_recv_data[2] << 8 );
		int totalLen = 3 + payloadLen;
		if ( totalLen > WN_SESSION_BUFFER_MAX ) {
			WN_GameSessionFail( conn, 7 );
			return;
		}
		if ( conn->session_recv_len < totalLen ) break;
		WN_GameHandleHandshakeMessage( conn, stream_id,
			conn->session_recv_data, totalLen );
		if ( conn->session_failed ) return;
		memmove( conn->session_recv_data,
			conn->session_recv_data + totalLen,
			(size_t)( conn->session_recv_len - totalLen ) );
		conn->session_recv_len -= totalLen;
		Q_SecureZeroMemory( conn->session_recv_data + conn->session_recv_len,
			(size_t)totalLen );
	}
	if ( fin && conn->session_recv_len != 0 )
		WN_GameSessionFail( conn, 8 );
}


void WN_GameHandleReliable( wn_connection_t *conn, uint64_t stream_id,
                               const byte *data, int len, qboolean fin )
{
	wn_game_conn_t *gc = conn ? conn->game_conn : NULL;
	if ( !gc || gc->conn != conn
	     || !WN_GameConnIdentityAccepted( gc->pub_handle, gc->allocation_id ) )
		return;
	wn_reliable_server_consume_stream( gc, stream_id, data, len, fin );
}


// ═══════════════════════════════════════════════════════════════════
// Public game packet API
// ═══════════════════════════════════════════════════════════════════

void WN_SendGamePacketToAddr( const netadr_t *to, const void *data, int length )
{
#if defined(__EMSCRIPTEN__)
	(void)to; (void)data; (void)length;
	return;
#else
	wn_game_conn_t *gc;

	if ( !wn.initialized || !wn.quic ) {
#if !defined(HEADLESS)
		goto try_client;
#else
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WN_SendGamePacketToAddr: QUIC not initialized\n" );
		return;
#endif
	}

	gc = WN_GameFindConnForAddr( to );
	if ( gc && gc->conn && gc->conn->cnx ) {
		picoquic_queue_datagram_frame( gc->conn->cnx, (size_t)length,
		                               (const uint8_t *)data );
		return;
	}

#if !defined(HEADLESS)
try_client:
	WN_ClientSendPacket( to, data, length );
#else
	Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WN_SendGamePacketToAddr: no game conn for %s\n",
		NET_AdrToStringwPort( to ) );
#endif
#endif
}

/*
==================
WN_GetConnHandleByAddr

Look up a conn_handle_t for a network address. Returns CONN_INVALID when
no active game connection exists for that address. Type-agnostic (NA_QUIC
and NA_IP both match the underlying IP bytes).

Used in sv_client.c / sv_main.c to replace NET_IS_QUIC() checks.
==================
*/
conn_handle_t WN_GetConnHandleByAddr( const netadr_t *addr )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !gc->conn || !WN_GameConnIdentityAccepted( gc->pub_handle,
			gc->allocation_id ) )
			continue;
		{
			const netadr_t *a = &gc->conn->addr;
			if ( NET_IS_IPV6( a->type ) && NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._6, addr->ipv._6, 16 ) == 0 )
					// NOLINTNEXTLINE(bugprone-misplaced-widening-cast) — small client index widened to conn_handle_t; no precision loss
					return (conn_handle_t)(i + 1);
			} else if ( !NET_IS_IPV6( a->type ) && !NET_IS_IPV6( addr->type ) ) {
				if ( memcmp( a->ipv._4, addr->ipv._4, 4 ) == 0 )
					// NOLINTNEXTLINE(bugprone-misplaced-widening-cast) — small client index widened to conn_handle_t; no precision loss
					return (conn_handle_t)(i + 1);
			}
		}
	}
	return CONN_INVALID;
}

/*
==================
WN_GetAddrByConnHandle

Fill *out with the remote address for the given conn_handle.
Returns qtrue on success, qfalse when the handle is invalid.
==================
*/
qboolean WN_GetAddrByConnHandle( conn_handle_t conn, netadr_t *out )
{
#if !defined(HEADLESS)
	if ( conn == CONN_CLIENT_HANDLE ) {
		if ( wtcl_array[0].initialized ) {
			*out = wtcl_array[0].server_addr;
			return qtrue;
		}
		return qfalse;
	}
#endif
	if ( conn != CONN_INVALID && conn <= WN_MAX_CLIENTS ) {
		wn_game_conn_t *gc = &wn.game_conns[conn - 1];
		if ( gc->active && gc->conn ) {
			*out = gc->conn->addr;
			return qtrue;
		}
	}
	/* In-memory same-process app (in-process-queue L4): an in-process app handle
	 * — either end — resolves to a synthetic NA_LOOPBACK address so
	 * SV_OnPlayerConnect admits it as a local client (the server then keys
	 * host-identity on slot, not type — see SV_IsHostClient). The server-end
	 * handle (110+slot) is the one that reaches SV_OnPlayerConnect; the client
	 * end (100+slot) is decoded too for symmetry / metrics callers. */
	if ( ( conn >= WN_APP_CONN_BASE   && conn < WN_APP_CONN_BASE   + WN_APP_CONN_COUNT ) ||
	     ( conn >= WN_APP_SVCONN_BASE  && conn < WN_APP_SVCONN_BASE + WN_APP_SVCONN_COUNT ) ) {
		memset( out, 0, sizeof( *out ) );
		out->type = NA_LOOPBACK;
		return qtrue;
	}
	return qfalse;
}

/*
==================
WN_DrainPendingConnects

Called from the main thread (WN_ProcessTimers) to consume pending game
connects enqueued by the QUIC I/O thread and call transport->accept_callback.
==================
*/
void WN_DrainPendingConnects( void )
{
	int      i;
	qboolean was_pending[WN_MAX_CLIENTS];

	if ( !transport || !transport->accept_callback )
		return;

	/* Snapshot pending flags before draining.  accept_callback may re-queue
	 * a connection back into pending_connects (e.g., via WN_RequeueConnect
	 * during async spawn); without the snapshot the loop would re-process it
	 * in the same drain cycle. */
	for ( i = 0; i < WN_MAX_CLIENTS; i++ )
		was_pending[i] = wn.pending_connects[i].pending;

	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( was_pending[i] ) {
			conn_handle_t conn = wn.pending_connects[i].conn;
			uint64_t      allocationId = wn.pending_connects[i].allocation_id;
			char          ui[MAX_INFO_STRING];
			Q_strncpyz( ui, wn.pending_connects[i].userinfo, sizeof(ui) );
			Q_SecureZeroMemory( &wn.pending_connects[i],
				sizeof( wn.pending_connects[i] ) );
			if ( WN_GameConnByIdentity( conn, allocationId ) ) {
				transport->accept_callback( conn, allocationId, ui );
			} else {
				Com_Log( SEV_WARN, LOG_CH(ch_network),
					"QUIC game: stale pending CONNECT rejected allocation=%llu\n",
					(unsigned long long)allocationId );
			}
			Q_SecureZeroMemory( ui, sizeof( ui ) );
		}
	}
}

/*
==================
WN_RequeueConnect

Re-enqueue a pending game-client connection that could not be admitted yet
(e.g., the server is in mid-spawn and gvm is not yet valid).  The connection
stays alive at the QUIC level; the next WN_DrainPendingConnects call will
retry accept_callback.

Called from SV_OnPlayerConnect when svs.spawn.phase != SPAWN_IDLE.
==================
*/
void WN_RequeueConnect( conn_handle_t conn, uint64_t allocationId,
	const char *userinfo )
{
	wn_game_conn_t *gc = WN_GameConnByIdentity( conn, allocationId );
	if ( !gc ) {
		Com_Log( SEV_WARN, LOG_CH(ch_network),
			"QUIC game: stale CONNECT requeue rejected allocation=%llu\n",
			(unsigned long long)allocationId );
		return;
	}
	if ( WN_QueuePendingConnect( gc, userinfo ) ) return;
	/* All slots occupied — cannot hold the connection. Drop through the
	 * per-handle backend (transport_for_handle), not the global `transport`
	 * (QUIC) vtable: an in-process app conn (WN_APP_*_BASE range) belongs to
	 * inmem_transport, and dropping it via the QUIC drop_client would target the
	 * wrong backend. Mirrors the transport_for_handle(conn)->drop_client pattern
	 * in sv_client.c. */
	Com_Log( SEV_INFO, LOG_CH(ch_network), "*** WN_RequeueConnect: no free slot for conn %llu — dropping ***\n",
		(unsigned long long)conn );
	if ( transport )
		transport_for_handle( conn )->drop_client( conn, "Server starting up" );
}

/*
==================
WN_DrainPendingReady

Called from the main thread (WN_ProcessTimers) to consume pending READY
events enqueued by the QUIC I/O thread and call transport->ready_callback.
==================
*/
void WN_DrainPendingReady( void )
{
	int i;
	if ( !transport || !transport->ready_callback )
		return;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.pending_ready[i].pending ) {
			conn_handle_t conn = wn.pending_ready[i].conn;
			uint64_t allocationId = wn.pending_ready[i].allocation_id;
			Q_SecureZeroMemory( &wn.pending_ready[i],
				sizeof( wn.pending_ready[i] ) );
			if ( WN_GameConnIdentityAccepted( conn, allocationId ) ) {
				transport->ready_callback( conn, allocationId );
			} else {
				Com_Log( SEV_WARN, LOG_CH(ch_network),
					"QUIC game: stale READY rejected allocation=%llu\n",
					(unsigned long long)allocationId );
			}
		}
	}
}

/*
==================
WN_DrainPendingFrees

Free in-process game_conns marked for deferred teardown (in-process-queue B4).
Called from sv_main.c AFTER SV_DrainQUICReliableCommands so a disconnecting
in-mem host's in-band "disconnect" reliable command is processed (→ SV_DropClient
→ CS_ZOMBIE) before its game_conn is released — the server sees the explicit
disconnect instead of waiting for SV_CheckTimeouts to reap an orphan.

Only in-mem conns ever set pending_free (wn_inmem_disconnect); a QUIC conn is
torn down through the transport drop_client path, so this is a no-op (zero
pending_free flags) for the QUIC server — byte-identical.
==================
*/
void WN_DrainPendingFrees( void )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( gc->active && gc->pending_free )
			WN_GameFreeConn( gc );   /* clears active + zeroes the slot */
	}
}

#if !defined(HEADLESS)
/*
==================
WN_ResetInmemClientRings

Drain (queues only) the in-process app's client + server rings on a localReconnect
map→map transition (in-process-queue B4). The host stays CA_CONNECTED across the
transition (CL_MapLoading localReconnect branch — the conn is NOT torn down), so
the rings carry stale snapshot/usercmd/reliable datagrams from the previous map.
Reset the head/tail indices + the bootstrap-ready flag to discard those remnants
deterministically (they were self-healing via serverId rejection + per-frame
drain, but this makes the boundary explicit).

Drains ONLY the queues — keeps initialized / server_addr / quic(==NULL) so the
connection stays live. Both ends:
  client end  — wtcl_array[slot].recv_queue + rel_queue + bootstrap_recv buffer
  server end  — game_conns[slot's pub_handle].recv_queue + rel_queue
so neither a stale srv→cli snapshot nor a stale cli→srv usercmd survives.

Gated by the caller on WN_HasInmemClient(), so a QUIC host never calls this —
byte-identical for QUIC.
==================
*/
void WN_ResetInmemClientRings( int app_slot )
{
	wn_game_conn_t *gc;

	if ( app_slot < 0 || app_slot >= WN_MAX_LOCAL_CLIENTS )
		return;

	/* Client end — wtcl_array[slot] queues + bootstrap, NOT the whole struct. */
	{
		wn_client_state_t *c = &wtcl_array[app_slot];
		c->recv_head = c->recv_tail = 0;
		c->rel_head  = c->rel_tail  = 0;
		c->bootstrap_recv_ready = qfalse;
		c->bootstrap_recv_len   = 0;
	}

	/* Server end — the game_conn for this app's server-end handle. */
	gc = WN_GameConnByAppHandle( (conn_handle_t)( WN_APP_SVCONN_BASE + app_slot ) );
	if ( gc && gc->active ) {
		gc->recv_head = gc->recv_tail = 0;
		gc->rel_head  = gc->rel_tail  = 0;
	}
}
#endif

#if !defined(HEADLESS)

/*
==================
WN_ClientSendReady

Send TLV 0x05 READY on the session stream (CHAN_SESSION = stream 0) to inform
the server that the client finished loading and is ready to enter the world.
==================
*/
void WN_ClientSendReady( int app_slot )
{
	/* TLV: [type:u8=0x05][len_lo:u8=0x00][len_hi:u8=0x00] — no payload */
	byte          ready_tlv[3] = { 0x05, 0x00, 0x00 };
	conn_handle_t client_conn;
	transport_t  *ready_transport;

	if ( app_slot < 0 || app_slot >= WN_MAX_LOCAL_CLIENTS )
		return;

	/* Route READY on this client's own handle. An in-process client (no picoquic)
	 * uses its WN_APP_CONN_BASE+slot handle → inmem_transport; a QUIC client uses
	 * CONN_CLIENT_HANDLE → the QUIC backend. The integrated host is the in-process
	 * client at slot 0, so it sends on WN_APP_CONN_BASE+0 exactly as before. */
	if ( wtcl_array[app_slot].initialized && wtcl_array[app_slot].quic == NULL )
		client_conn = (conn_handle_t)( WN_APP_CONN_BASE + app_slot );
	else
		client_conn = (conn_handle_t)CONN_CLIENT_HANDLE;

	ready_transport = transport_for_handle( client_conn );
	if ( !ready_transport->send_reliable( client_conn,
		CHAN_SESSION, ready_tlv, (int)sizeof( ready_tlv ) ) ) {
		wn_client_state_t *client = &wtcl_array[app_slot];
		qboolean networkClient = client->quic != NULL;
		if ( !networkClient && ready_transport->disconnect )
			ready_transport->disconnect( client_conn, "READY send failed" );
		client->accept_pending = qfalse;
		client->connect_failed = qtrue;
		client->connect_error_kind = NET_CONNECT_ERROR_GENERIC;
		Q_strncpyz( client->connect_error, "Unable to complete server admission",
			sizeof( client->connect_error ) );
		client->pending_disconnect = networkClient;
		Com_Log( SEV_WARN, LOG_CH(ch_network),
			"QUIC client: READY send failed deferred_disconnect=%d\n",
			networkClient ? 1 : 0 );
	}
}

wn_client_state_t wtcl_array[WN_MAX_LOCAL_CLIENTS];

/* Recv-queue overflow counter (picoquic_callback_datagram drop site).
 * Single-producer: incremented by the picoquic callback thread.
 * Single-consumer for diagnostic readback: net_quic_status main thread via
 * WN_GetRecvQueueFullDrops(). uint32_t reads/writes are atomic on the
 * platforms we target — no fence required for surfacing a counter. */
static volatile uint32_t wn_recv_queue_full_drops = 0;

uint32_t WN_GetRecvQueueFullDrops( void )
{
	return wn_recv_queue_full_drops;
}

static void WN_ClientSessionFail( int failureClass ) {
	wn_client_state_t *client = &wtcl_array[0];
	Q_SecureZeroMemory( client->session_recv_data,
		sizeof( client->session_recv_data ) );
	client->session_recv_len = 0;
	client->accept_pending = qfalse;
	client->connect_failed = qtrue;
	client->connect_error_kind = NET_CONNECT_ERROR_GENERIC;
	Q_strncpyz( client->connect_error, "Invalid server admission response",
		sizeof( client->connect_error ) );
	client->pending_disconnect = qtrue;
	Com_Log( SEV_WARN, LOG_CH(ch_network),
		"QUIC client: malformed session stream class=%d deferred_disconnect=1\n",
		failureClass );
}

static void WN_ClientConsumeSession( const byte *data, int len, qboolean fin ) {
	wn_client_state_t *client = &wtcl_array[0];

	if ( len < 0 || len > WN_SESSION_BUFFER_MAX - client->session_recv_len ) {
		WN_ClientSessionFail( 1 );
		return;
	}
	if ( len > 0 ) {
		memcpy( client->session_recv_data + client->session_recv_len, data,
			(size_t)len );
		client->session_recv_len += len;
	}
	while ( client->session_recv_len >= 3 ) {
		byte *message = client->session_recv_data;
		int payloadLen = message[1] | ( (int)message[2] << 8 );
		int totalLen = 3 + payloadLen;
		if ( totalLen > WN_SESSION_BUFFER_MAX ) {
			WN_ClientSessionFail( 2 );
			return;
		}
		if ( client->session_recv_len < totalLen ) break;
		if ( client->admission_terminal != 0 ) {
			WN_ClientSessionFail( 3 );
			return;
		}
		if ( message[0] == 0x02 ) {
			if ( payloadLen != 4 ) {
				WN_ClientSessionFail( 4 );
				return;
			}
			client->admission_terminal = 1;
			client->accept_pending = qtrue;
			Com_Log( SEV_DEBUG, LOG_CH(ch_network),
				"QUIC client: TLV ACCEPT received\n" );
		} else if ( message[0] == 0x03 ) {
			netRefuseClass_t refusalClass;
			if ( payloadLen != 1 ) {
				WN_ClientSessionFail( 5 );
				return;
			}
			refusalClass = (netRefuseClass_t)message[3];
			client->admission_terminal = 2;
			client->connect_failed = qtrue;
			switch ( refusalClass ) {
			case NET_REFUSE_AUTH:
				client->connect_error_kind = NET_CONNECT_ERROR_AUTH_REFUSED;
				Q_strncpyz( client->connect_error, "Server authentication failed",
					sizeof( client->connect_error ) );
				break;
			case NET_REFUSE_SERVER_FULL:
				client->connect_error_kind = NET_CONNECT_ERROR_SERVER_FULL;
				Q_strncpyz( client->connect_error, "Server is full",
					sizeof( client->connect_error ) );
				break;
			case NET_REFUSE_GENERIC:
				client->connect_error_kind = NET_CONNECT_ERROR_GENERIC;
				Q_strncpyz( client->connect_error, "Connection refused",
					sizeof( client->connect_error ) );
				break;
			default:
				WN_ClientSessionFail( 6 );
				return;
			}
			client->pending_disconnect = qtrue;
			Com_Log( SEV_WARN, LOG_CH(ch_network),
				"QUIC connect refused class=%d deferred_disconnect=1\n",
				(int)refusalClass );
		} else {
			WN_ClientSessionFail( 7 );
			return;
		}
		memmove( client->session_recv_data,
			client->session_recv_data + totalLen,
			(size_t)( client->session_recv_len - totalLen ) );
		client->session_recv_len -= totalLen;
		Q_SecureZeroMemory( client->session_recv_data + client->session_recv_len,
			(size_t)totalLen );
	}
	if ( fin && client->session_recv_len != 0 )
		WN_ClientSessionFail( 8 );
}

static int WN_ClientCallback(
	picoquic_cnx_t            *cnx,
	uint64_t                   stream_id,
	uint8_t                   *bytes,
	size_t                     length,
	picoquic_call_back_event_t event,
	void                      *callback_ctx,
	void                      *v_ctx )
{
	(void)callback_ctx;
	(void)v_ctx;

	Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC client CB: event=%d stream=%llu conn=%s len=%zu\n",
		(int)event, (unsigned long long)stream_id,
		cnx ? "yes" : "no", length );

	switch ( event ) {

	case picoquic_callback_ready:
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: connected to server %s\n",
			NET_AdrToString( &wtcl_array[0].server_addr ) );
		{
			/* Send TLV 0x01 CONNECT on session stream 0x00:
			 * version:u16le=0x0100, userinfo_len:u16le, userinfo:bytes */
			uint16_t ulen = (uint16_t)strlen( wtcl_array[0].userinfo );
			byte     connect_pl[MAX_INFO_STRING + 4];
			byte     connect_tlv[MAX_INFO_STRING + 8];
			int      tlv_len;
			int      send_result = -1;

			connect_pl[0] = 0x00; /* version lo */
			connect_pl[1] = 0x01; /* version hi */
			connect_pl[2] = (byte)( ulen & 0xFF );
			connect_pl[3] = (byte)( (ulen >> 8) & 0xFF );
			if ( ulen > 0 )
				memcpy( connect_pl + 4, wtcl_array[0].userinfo, ulen );
			tlv_len = TLV_Write( connect_tlv, (int)sizeof(connect_tlv),
				0x01, connect_pl, (uint16_t)( 4 + ulen ) );
			if ( tlv_len > 0 ) {
				send_result = picoquic_add_to_stream( cnx, WIREDNET_SESSION_STREAM_ID,
					connect_tlv, (size_t)tlv_len, 0 );
			}
			if ( tlv_len <= 0 || send_result != 0 ) {
				wtcl_array[0].connect_failed = qtrue;
				wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_GENERIC;
				Q_strncpyz( wtcl_array[0].connect_error,
					"Unable to start server admission",
					sizeof( wtcl_array[0].connect_error ) );
				wtcl_array[0].pending_disconnect = qtrue;
				Com_Log( SEV_WARN, LOG_CH(ch_network),
					"QUIC client: CONNECT send failed result=%d deferred_disconnect=1\n",
					send_result );
			} else {
				Com_Log( SEV_DEBUG, LOG_CH(ch_network),
					"QUIC client: sent TLV CONNECT on stream 0x%02X (%d bytes)\n",
					(unsigned)WIREDNET_SESSION_STREAM_ID, tlv_len );
			}
			Q_SecureZeroMemory( wtcl_array[0].userinfo,
				sizeof( wtcl_array[0].userinfo ) );
			Q_SecureZeroMemory( connect_pl, sizeof( connect_pl ) );
			Q_SecureZeroMemory( connect_tlv, sizeof( connect_tlv ) );
		}
		break;

	case picoquic_callback_stream_data:
	case picoquic_callback_stream_fin:
		if ( stream_id == WIREDNET_SESSION_STREAM_ID ) {
			WN_ClientConsumeSession( (const byte *)bytes, (int)length,
				event == picoquic_callback_stream_fin );
			break;
		}
		/* Non-session game-reliable message from server.
		 * Each logical message uses its own QUIC stream and starts with a small
		 * game envelope that identifies the semantic channel. */
		wn_reliable_client_consume_stream( stream_id, bytes, (int)length,
			event == picoquic_callback_stream_fin );
		break;

	case picoquic_callback_datagram:
		if ( length <= 0 || length > WN_SNAP_DGRAM_MAX ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC client: datagram len %zu out of range — dropped\n", length );
			break;
		}
		{
			int            next_head = ( wtcl_array[0].recv_head + 1 ) % WN_GAME_QUEUE_SIZE;
			wn_snap_pkt_t *pkt;
			if ( next_head == wtcl_array[0].recv_tail ) {
				/* Rate-limited surfacing of recv-queue overflow. First drop per
				 * process emits a one-shot SEV_WARN; subsequent drops emit at
				 * most once per 5 s, batched with an accumulated count. */
				static unsigned int last_warn_ms     = 0;
				static uint32_t     drops_since_warn = 0;
				unsigned int        now_ms           = (unsigned int)Sys_Milliseconds();
				uint32_t            total;

				total = ++wn_recv_queue_full_drops;
				drops_since_warn++;

				if ( total == 1 ) {
					Com_Log( SEV_WARN, LOG_CH(ch_network),
						"QUIC client: recv queue full — first drop this session "
						"(queue capacity=%d, datagram len=%zu)\n",
						WN_GAME_QUEUE_SIZE, length );
					last_warn_ms     = now_ms;
					drops_since_warn = 0;
				} else if ( now_ms - last_warn_ms >= 5000 ) {
					Com_Log( SEV_WARN, LOG_CH(ch_network),
						"QUIC client: recv queue full — %u dropped since last warning "
						"(queue capacity=%d, total since start=%u)\n",
						(unsigned)drops_since_warn, WN_GAME_QUEUE_SIZE, (unsigned)total );
					last_warn_ms     = now_ms;
					drops_since_warn = 0;
				}
				break;
			}
			pkt            = &wtcl_array[0].recv_queue[wtcl_array[0].recv_head];
			pkt->from      = wtcl_array[0].server_addr;
			pkt->from.type = ( wtcl_array[0].server_addr.type == NA_IP6 ) ? NA_QUIC6 : NA_QUIC;
			pkt->len       = (int)length;
			memcpy( pkt->data, bytes, length );
			wtcl_array[0].recv_head = next_head;
		}
		break;

	case picoquic_callback_close:
	case picoquic_callback_application_close:
	case picoquic_callback_stateless_reset:
		{
			uint64_t local_err  = cnx ? picoquic_get_local_error( cnx )  : 0;
			uint64_t remote_err = cnx ? picoquic_get_remote_error( cnx ) : 0;
			Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC client: connection closed (event=%d local_err=%llu remote_err=%llu)\n",
				(int)event, (unsigned long long)local_err, (unsigned long long)remote_err );
		}
		/* Do NOT call WN_ClientDisconnect() here — we are inside a picoquic
		 * callback fired from picoquic_prepare_packet_ex. Calling picoquic_free
		 * at this point corrupts the splay tree that picoquic_prepare_packet_ex
		 * is still walking (use-after-free → SIGSEGV).
		 * Set pending_disconnect; WN_ClientFlushOutbound checks the flag
		 * after picoquic_prepare_next_packet_ex returns and does cleanup safely. */
		wtcl_array[0].pending_disconnect = qtrue;
		break;

	default:
		break;
	}
	return 0;
}

static void WN_ClientFlushOutbound( void )
{
#if defined(__EMSCRIPTEN__)
	return;
#else
	byte                     send_buf[WN_PACKET_BUF_SIZE];
	size_t                   send_len;
	size_t                   send_msg_size;
	struct sockaddr_storage  addr_to;
	struct sockaddr_storage  addr_from;
	int                      if_index;
	uint64_t                 current_time;
	picoquic_connection_id_t log_cid;
	picoquic_cnx_t          *last_cnx = NULL;
	netadr_t                 to;
	int                      ret;

	if ( !wtcl_array[0].initialized || !wtcl_array[0].quic )
		return;

	// Clear-before-send contract: anything in net_lastSendError after the
	// loop is from this frame's sends — no timestamp, no race window.
	NET_ClearLastSendError();

	current_time = picoquic_current_time();

	while ( 1 ) {
		send_len = 0;
		ret = picoquic_prepare_next_packet_ex(
			wtcl_array[0].quic, current_time,
			send_buf, sizeof(send_buf), &send_len,
			&addr_to, &addr_from, &if_index,
			&log_cid, &last_cnx, &send_msg_size );

		Com_Log( SEV_TRACE, LOG_CH(ch_network), "WN_ClientFlushOutbound: prepare ret=%d send_len=%zu\n", ret, send_len );

		if ( ret != 0 || send_len == 0 )
			break;

		memset( &to, 0, sizeof(to) );
		if ( addr_to.ss_family == AF_INET ) {
			struct sockaddr_in *v4 = (struct sockaddr_in *)&addr_to;
			to.type = NA_IP;
			memcpy( to.ipv._4, &v4->sin_addr.s_addr, 4 );
			to.port = v4->sin_port;
		}
#if FEAT_IPV6
		else if ( addr_to.ss_family == AF_INET6 ) {
			struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&addr_to;
			to.type     = NA_IP6;
			to.port     = v6->sin6_port;
			to.scope_id = (uint32_t)v6->sin6_scope_id;
			memcpy( to.ipv._6, &v6->sin6_addr, 16 );
		}
#endif
		else {
			continue;
		}
		NET_SendPacket( NS_CLIENT, (int)send_len, send_buf, &to );
	}

	// Promote any sendto error from this frame to the connect-error slot.
	if ( NET_HasLastSendError() && !wtcl_array[0].connect_failed ) {
		Com_Log( SEV_INFO, LOG_CH(ch_network), "*** WN_ClientFlushOutbound: send error → connect_failed: %s ***\n",
			NET_LastSendError() );
		Q_strncpyz( wtcl_array[0].connect_error, NET_LastSendError(),
		            sizeof( wtcl_array[0].connect_error ) );
		wtcl_array[0].connect_failed = qtrue;
		wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_GENERIC;
	}

	// Deferred disconnect: if the close callback fired during the loop above,
	// now that we are outside picoquic it is safe to call picoquic_free.
	if ( wtcl_array[0].pending_disconnect ) {
		Com_Log( SEV_INFO, LOG_CH(ch_network), "*** WN_ClientFlushOutbound: pending_disconnect → WN_ClientDisconnect ***\n" );
		wtcl_array[0].pending_disconnect = qfalse;
		WN_ClientDisconnect();
	}
#endif
}

/* ═══════════════════════════════════════════════════════════════════════
 * TOFU (Trust On First Use) certificate verifier — client only.
 *
 * On first connect: extract SubjectPublicKeyInfo DER → SHA-256 hex,
 * accept the cert, record "addr:port fingerprint" in known_servers.txt.
 * On subsequent connects: reject if fingerprint changes.
 * cvar wn_cert_verify=0 disables TOFU (dev / offline mode).
 * ═══════════════════════════════════════════════════════════════════════ */

#if !defined(__EMSCRIPTEN__)
typedef struct {
	ptls_openssl_override_verify_certificate_t base;
	char addr[128];    /* "ip:port" key in known_servers.txt */
} wn_tofu_ctx_t;

static ptls_openssl_verify_certificate_t s_tofu_verif;
static wn_tofu_ctx_t                     s_tofu_ctx;

/* SHA-256 of SubjectPublicKeyInfo DER, hex-encoded into hex_out. */
static void wn_tofu_fingerprint( X509 *cert, char *hex_out, int hex_size )
{
	EVP_PKEY      *pkey;
	unsigned char *der_buf = NULL;
	int            der_len;
	unsigned char  digest[SHA256_DIGEST_LENGTH];
	int            i;

	pkey = X509_get_pubkey( cert );
	if ( !pkey ) {
		Q_strncpyz( hex_out, "error:no-pubkey", hex_size );
		return;
	}
	der_len = i2d_PUBKEY( pkey, &der_buf );
	EVP_PKEY_free( pkey );
	if ( der_len <= 0 || !der_buf ) {
		Q_strncpyz( hex_out, "error:i2d-failed", hex_size );
		return;
	}
	SHA256( der_buf, (size_t)der_len, digest );
	OPENSSL_free( der_buf );

	for ( i = 0; i < SHA256_DIGEST_LENGTH && i * 2 < hex_size - 1; i++ )
		sprintf( hex_out + i * 2, "%02x", (unsigned)digest[i] );
	hex_out[i * 2] = '\0';
}

static void wn_tofu_file_path( char *out, int size )
{
	const char *home = Cvar_VariableString( "fs_homepath" );
	if ( !home || !*home ) home = ".";
	Com_sprintf( out, size, "%s/known_servers.txt", home );
}

/*
 * Returns: 0 = found + fingerprint matches,
 *          1 = not found (first-time connect),
 *         -1 = found but fingerprint mismatch.
 */
static int wn_tofu_check( const char *addr, const char *fp )
{
	char   path[MAX_OSPATH];
	char   line[512];
	FILE  *f;
	size_t alen;

	wn_tofu_file_path( path, sizeof(path) );
	f = fopen( path, "r" );
	if ( !f )
		return 1; /* no file yet — first-time connect */

	alen = strlen( addr );
	while ( fgets( line, sizeof(line), f ) ) {
		char *p = line;
		while ( *p == ' ' || *p == '\t' ) p++;
		if ( *p == '#' || *p == '\n' || *p == '\r' || *p == '\0' )
			continue;
		if ( strncmp( p, addr, alen ) == 0 && (p[alen] == ' ' || p[alen] == '\t') ) {
			char  *stored = p + alen;
			size_t flen;
			while ( *stored == ' ' || *stored == '\t' ) stored++;
			flen = strlen( stored );
			while ( flen > 0 && (stored[flen-1] == '\n' || stored[flen-1] == '\r') )
				stored[--flen] = '\0';
			fclose( f );
			return strcmp( stored, fp ) == 0 ? 0 : -1;
		}
	}
	fclose( f );
	return 1; /* not found */
}

static void wn_tofu_save( const char *addr, const char *fp )
{
	char  path[MAX_OSPATH];
	FILE *f;

	wn_tofu_file_path( path, sizeof(path) );
	f = fopen( path, "a" );
	if ( !f ) {
		COM_WARN( LOG_CH(ch_network), "QUIC TOFU: could not write %s\n", path );
		return;
	}
	fseek( f, 0, SEEK_END );
	if ( ftell( f ) == 0 )
		fprintf( f, "# WiredNet known servers\n# address:port sha256_fingerprint\n" );
	fprintf( f, "%s %s\n", addr, fp );
	fclose( f );
}

static int tofu_override_cb(
	ptls_openssl_override_verify_certificate_t *self,
	ptls_t *tls, int ret, int ossl_ret,
	X509 *cert, STACK_OF(X509) *chain )
{
	wn_tofu_ctx_t *ctx = (wn_tofu_ctx_t *)self;
	char           fp[SHA256_DIGEST_LENGTH * 2 + 1];
	int            chk;

	(void)tls; (void)ret; (void)ossl_ret; (void)chain;

	if ( !cert ) {
		COM_ERROR( LOG_CH(ch_network), "QUIC TOFU: server presented no certificate\n" );
		return -1;
	}

	wn_tofu_fingerprint( cert, fp, sizeof(fp) );
	chk = wn_tofu_check( ctx->addr, fp );

	if ( chk == 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC TOFU: new server %s — trusting (SPKI SHA256: %.16s...)\n",
			ctx->addr, fp );
		wn_tofu_save( ctx->addr, fp );
		return 0;
	}
	if ( chk == 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH( ch_network ), "QUIC TOFU: cert OK for %s\n", ctx->addr );
		return 0;
	}
	COM_ERROR( LOG_CH( ch_network ),
			   "QUIC TOFU: certificate changed for %s!\n"
			   "  Got SPKI SHA256: %.16s...\n"
			   "  Delete entry in known_servers.txt to reconnect.\n",
			   ctx->addr, fp );
	return -1;
}

static void tofu_free_fn( ptls_verify_certificate_t *ctx )
{
	ptls_openssl_dispose_verify_certificate( (ptls_openssl_verify_certificate_t *)ctx );
}

void WN_ClientConnect( const netadr_t *serverAddr,
                          const char *userinfo, int qport )
{
	uint64_t              current_time;
	struct sockaddr_storage ss;
	int                   ss_len = 0;

	if ( wtcl_array[0].initialized ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WN_ClientConnect: already connected\n" );
		return;
	}

	Q_SecureZeroMemory( &wtcl_array[0], sizeof( wtcl_array[0] ) );
	// Reset connect-error state so a retry starts clean (ED1 fix).
	wtcl_array[0].connect_failed  = qfalse;
	wtcl_array[0].connect_error[0] = '\0';
	wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_NONE;
	Q_strncpyz( wtcl_array[0].userinfo, userinfo, sizeof(wtcl_array[0].userinfo) );
	wtcl_array[0].qport       = qport;
	wtcl_array[0].server_addr = *serverAddr;

	current_time = picoquic_current_time();

	wtcl_array[0].quic = picoquic_create(
		1, NULL, NULL, NULL,
		WN_ALPN, NULL, NULL, NULL, NULL, NULL,
		current_time, NULL, NULL, NULL, 0 );

	if ( !wtcl_array[0].quic ) {
		COM_ERROR( LOG_CH(ch_network), "WN_ClientConnect: picoquic_create failed\n" );
		wtcl_array[0].connect_failed = qtrue;
		wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_GENERIC;
		Q_strncpyz( wtcl_array[0].connect_error, "picoquic_create failed",
		            sizeof( wtcl_array[0].connect_error ) );
		Q_SecureZeroMemory( wtcl_array[0].userinfo,
			sizeof( wtcl_array[0].userinfo ) );
		return;
	}

	/* Certificate verification: TOFU by default; bypassed when wn_cert_verify=0. */
	{
		static const cvarDesc_t d = CVAR_BOOL( "wn_cert_verify", "1", CVAR_ARCHIVE,
			"Verify TLS certificate on QUIC connections. Set to 0 to use TOFU (trust on first use)." );
		Cvar_Register( &d );
	}
	if ( Cvar_VariableIntegerValue( "wn_cert_verify" ) == 0 ) {
		picoquic_set_null_verifier( wtcl_array[0].quic );
	} else {
		/* Build addr string for known_servers.txt lookup.
		 * Handle loopback before the NA_LOOPBACK→127.0.0.1 fixup below. */
		if ( serverAddr->type == NA_LOOPBACK ) {
			Com_sprintf( s_tofu_ctx.addr, sizeof(s_tofu_ctx.addr), "127.0.0.1:%d",
				(int)ntohs( serverAddr->port ) );
		} else if ( serverAddr->type == NA_IP ) {
			Com_sprintf( s_tofu_ctx.addr, sizeof(s_tofu_ctx.addr), "%d.%d.%d.%d:%d",
				(int)serverAddr->ipv._4[0], (int)serverAddr->ipv._4[1],
				(int)serverAddr->ipv._4[2], (int)serverAddr->ipv._4[3],
				(int)ntohs( serverAddr->port ) );
		} else {
			Q_strncpyz( s_tofu_ctx.addr, "unknown", sizeof(s_tofu_ctx.addr) );
		}
		s_tofu_ctx.base.cb = tofu_override_cb;
		ptls_openssl_init_verify_certificate( &s_tofu_verif, NULL );
		s_tofu_verif.override_callback = &s_tofu_ctx.base;
		picoquic_set_verify_certificate_callback( wtcl_array[0].quic,
			&s_tofu_verif.super, tofu_free_fn );
	}
	picoquic_set_default_datagram_priority( wtcl_array[0].quic, 1 );
	/* Advertise datagram support.  Without this the local_parameters.max_datagram_frame_size
	 * defaults to 0, and any received datagram triggers FRAME_FORMAT_ERROR (error 7) even
	 * though picoquic_queue_datagram_frame only guards large datagrams on the send side. */
	picoquic_set_default_tp_value( wtcl_array[0].quic, picoquic_tp_max_datagram_frame_size,
	                               WN_SNAP_DGRAM_MAX );
	/* Loopback (integrated server): client and server are the same process.
	 * An idle timeout would fire during long map loads (WAMR init, bot AI,
	 * AAS load) and force a spurious reconnect.  Use 1 hour — effectively
	 * infinite.  Note: picoquic's idle_timeout=0 path falls through to its
	 * own 30s handshake default, so 0 is NOT "infinite" here; use explicit
	 * large value.  Remote servers keep the 30-second timeout so dead
	 * connections are detected promptly. */
	if ( serverAddr->type == NA_LOOPBACK ) {
		picoquic_set_default_idle_timeout( wtcl_array[0].quic, 3600000 );   /* 1 hour */
	} else {
		picoquic_set_default_idle_timeout( wtcl_array[0].quic, 30000 );
	}

	memset( &ss, 0, sizeof(ss) );
	if ( serverAddr->type == NA_IP ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, serverAddr->ipv._4, 4 );
		v4->sin_port = serverAddr->port;
		ss_len = sizeof(struct sockaddr_in);
	}
	else if ( serverAddr->type == NA_LOOPBACK ) {
		/* localhost — map to 127.0.0.1 so picoquic can send real UDP datagrams */
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss;
		v4->sin_family      = AF_INET;
		v4->sin_addr.s_addr = htonl( INADDR_LOOPBACK );  /* 127.0.0.1 */
		v4->sin_port        = serverAddr->port;
		ss_len = sizeof(struct sockaddr_in);
		/* update stored addr so WN_ClientCallback route-checks pass */
		wtcl_array[0].server_addr.type        = NA_IP;
		wtcl_array[0].server_addr.ipv._4[0]   = 127;
		wtcl_array[0].server_addr.ipv._4[1]   = 0;
		wtcl_array[0].server_addr.ipv._4[2]   = 0;
		wtcl_array[0].server_addr.ipv._4[3]   = 1;
		wtcl_array[0].server_addr.port        = serverAddr->port;
	}
#if FEAT_IPV6
	else if ( serverAddr->type == NA_IP6 ) {
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&ss;
		v6->sin6_family   = AF_INET6;
		v6->sin6_port     = serverAddr->port;
		v6->sin6_scope_id = serverAddr->scope_id;
		memcpy( &v6->sin6_addr, serverAddr->ipv._6, 16 );
		ss_len = sizeof(struct sockaddr_in6);
	}
#endif
	else {
		COM_ERROR( LOG_CH(ch_network), "WN_ClientConnect: unsupported address type %d\n",
			serverAddr->type );
		picoquic_free( wtcl_array[0].quic );
		wtcl_array[0].quic = NULL;
		wtcl_array[0].connect_failed = qtrue;
		wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_GENERIC;
		Q_strncpyz( wtcl_array[0].connect_error,
		            va( "unsupported address type %d", serverAddr->type ),
		            sizeof( wtcl_array[0].connect_error ) );
		Q_SecureZeroMemory( wtcl_array[0].userinfo,
			sizeof( wtcl_array[0].userinfo ) );
		return;
	}
	(void)ss_len;

	wtcl_array[0].cnx = picoquic_create_client_cnx(
		wtcl_array[0].quic, (struct sockaddr *)&ss,
		current_time, 0, NULL,
		WN_ALPN, WN_ClientCallback, NULL );

	if ( !wtcl_array[0].cnx ) {
		/* picoquic_create_client_cnx already calls picoquic_start_client_cnx internally.
		 * NULL here means TLS init failed (ALPN missing, crypto error, etc.). */
		COM_ERROR( LOG_CH(ch_network), "WN_ClientConnect: picoquic_create_client_cnx failed\n" );
		picoquic_free( wtcl_array[0].quic );
		wtcl_array[0].quic = NULL;
		wtcl_array[0].connect_failed = qtrue;
		wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_GENERIC;
		Q_strncpyz( wtcl_array[0].connect_error, "picoquic_create_client_cnx failed (TLS/crypto error)",
		            sizeof( wtcl_array[0].connect_error ) );
		Q_SecureZeroMemory( wtcl_array[0].userinfo,
			sizeof( wtcl_array[0].userinfo ) );
		return;
	}

	wtcl_array[0].initialized = qtrue;
	Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC client: connecting to %s...\n",
		NET_AdrToStringwPort( serverAddr ) );
}
#endif

qboolean WN_ClientHasError( char *out, int outSize,
	netConnectErrorKind_t *kind )
{
	if ( !wtcl_array[0].connect_failed )
		return qfalse;
	if ( out && outSize > 0 )
		Q_strncpyz( out, wtcl_array[0].connect_error, outSize );
	if ( kind )
		*kind = wtcl_array[0].connect_error_kind;
	return qtrue;
}

void WN_ClientClearError( void )
{
	wtcl_array[0].connect_failed   = qfalse;
	wtcl_array[0].connect_error[0] = '\0';
	wtcl_array[0].connect_error_kind = NET_CONNECT_ERROR_NONE;
}

void WN_ClientFrame( void )
{
	if ( !wtcl_array[0].initialized || !wtcl_array[0].quic )
		return;
	WN_ClientFlushOutbound();
}

void WN_ClientDisconnect( void )
{
#if defined(__EMSCRIPTEN__)
	Q_SecureZeroMemory( &wtcl_array[0], sizeof( wtcl_array[0] ) );
#else
	picoquic_cnx_t  *cnx;
	picoquic_quic_t *quic;
	char connectError[sizeof( wtcl_array[0].connect_error )];
	netConnectErrorKind_t connectErrorKind;
	qboolean connectFailed;

	if ( !wtcl_array[0].initialized ) {
		Q_SecureZeroMemory( wtcl_array[0].userinfo,
			sizeof( wtcl_array[0].userinfo ) );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_network), "*** WN_ClientDisconnect: called while initialized ***\n" );

	/* Zero state BEFORE calling into picoquic. picoquic_free triggers
	 * picoquic_delete_cnx → picoquic_connection_disconnect → callback_close
	 * → re-enters WN_ClientDisconnect. Without this guard the re-entrant
	 * call sees wtcl_array[0].quic != NULL and calls picoquic_free a second time. */
	cnx  = wtcl_array[0].cnx;
	quic = wtcl_array[0].quic;
	connectFailed = wtcl_array[0].connect_failed;
	connectErrorKind = wtcl_array[0].connect_error_kind;
	Q_strncpyz( connectError, wtcl_array[0].connect_error,
		sizeof( connectError ) );
	Q_SecureZeroMemory( &wtcl_array[0], sizeof( wtcl_array[0] ) );

	if ( cnx )
		picoquic_close( cnx, 0 );
	if ( quic )
		picoquic_free( quic );
	if ( connectFailed ) {
		wtcl_array[0].connect_failed = qtrue;
		wtcl_array[0].connect_error_kind = connectErrorKind;
		Q_strncpyz( wtcl_array[0].connect_error, connectError,
			sizeof( wtcl_array[0].connect_error ) );
	}
	Q_SecureZeroMemory( connectError, sizeof( connectError ) );
#endif
}

qboolean WN_ClientIsConnecting( void )
{
	return wtcl_array[0].initialized && wtcl_array[0].quic != NULL;
}

qboolean WN_ClientCheckPacket( const netadr_t *from, byte *buf, int len )
{
#if defined(__EMSCRIPTEN__)
	(void)from; (void)buf; (void)len;
	return qfalse;
#else
	uint64_t               current_time;
	struct sockaddr_storage ss_from;
	struct sockaddr_in      ss_to;

	if ( !wtcl_array[0].initialized || !wtcl_array[0].quic )
		return qfalse;
	if ( len <= 0 || len > WN_PACKET_BUF_SIZE )
		return qfalse;

	memset( &ss_from, 0, sizeof(ss_from) );
	if ( from->type == NA_IP ) {
		struct sockaddr_in *v4 = (struct sockaddr_in *)&ss_from;
		v4->sin_family = AF_INET;
		memcpy( &v4->sin_addr.s_addr, from->ipv._4, 4 );
		v4->sin_port = from->port;
	}
#if FEAT_IPV6
	else if ( from->type == NA_IP6 ) {
		struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&ss_from;
		v6->sin6_family   = AF_INET6;
		v6->sin6_port     = from->port;
		v6->sin6_scope_id = from->scope_id;
		memcpy( &v6->sin6_addr, from->ipv._6, 16 );
	}
#endif
	else {
		return qfalse;
	}

	memset( &ss_to, 0, sizeof(ss_to) );
	ss_to.sin_family      = AF_INET;
	ss_to.sin_addr.s_addr = INADDR_ANY;
	ss_to.sin_port        = from->port;

	current_time = picoquic_current_time();
	memcpy( wtcl_array[0].recv_buf, buf, len );

	picoquic_incoming_packet(
		wtcl_array[0].quic, wtcl_array[0].recv_buf, (size_t)len,
		(struct sockaddr *)&ss_from,
		(struct sockaddr *)&ss_to,
		0, 0, current_time );

	WN_ClientFlushOutbound();
	return qtrue;
#endif
}

void WN_ClientSendPacket( const netadr_t *to, const void *data, int length )
{
#if defined(__EMSCRIPTEN__)
	(void)to; (void)data; (void)length;
#else
	if ( !wtcl_array[0].initialized || !wtcl_array[0].cnx ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WN_ClientSendPacket: no client connection\n" );
		return;
	}
	{
		/* Verify destination matches our server (type-agnostic IP compare) */
		const netadr_t *a     = &wtcl_array[0].server_addr;
		qboolean        match = qfalse;
		if ( NET_IS_IPV6( a->type ) && NET_IS_IPV6( to->type ) )
			match = ( memcmp( a->ipv._6, to->ipv._6, 16 ) == 0 );
		else if ( !NET_IS_IPV6( a->type ) && !NET_IS_IPV6( to->type ) )
			match = ( memcmp( a->ipv._4, to->ipv._4, 4 ) == 0 );
		if ( !match ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_network), "WN_ClientSendPacket: address mismatch\n" );
			return;
		}
	}
	picoquic_queue_datagram_frame( wtcl_array[0].cnx, (size_t)length, (const uint8_t *)data );
#endif
}

#endif /* !HEADLESS */


// ═══════════════════════════════════════════════════════════════════
// Transport vtable implementation
// ═══════════════════════════════════════════════════════════════════

static void wn_shutdown( void )   { /* WN_Shutdown() called directly */              }

/*
 * wn_frame — transport->frame vtable target.
 *
 * Phase ordering (transport-internal invariant, do not reorder):
 *   1. WN_ProcessTimers      — picoquic retransmit / idle / keepalive timers
 *   2. WN_FlushOutbound      — pull packets from picoquic → NET_SendPacket
 *   3. WN_SendDatagrams      — observer state datagrams (FEAT_WIREDNET_OBSERVER)
 *   4. WN_PushEvents         — observer event stream pushes
 *   5. WN_TcpFrame           — HTTP observer accept/handle
 *   6. WN_ProcessCommandQueue — MCP command queue drain (FEAT_WIREDNET_CONTROL)
 *
 * Called from sv_main.c POST-GAME-FRAME phase (after VM_Call GAME_RUN_FRAME +
 * SV_SendClientMessages). Replaces the previous direct WN_* call sequence at
 * sv_main.c:1296-1304.
 *
 * NOT included here (intentional):
 *  - WN_ClientFrame()         — pumps from net_ip.c NET_Event every poll
 *                                (independent scheduling). Including here would
 *                                cause double-pump in listen-server mode.
 *  - WN_DrainPendingConnects/Ready — admission machinery; must fire AFTER
 *                                spawn-guard and BEFORE game frame so newly-
 *                                connected clients are present in the snapshot.
 *                                transport->frame fires after game frame, so
 *                                drains stay direct in sv_main.c:1159+1162.
 *  - WN_FlushOutbound (pre-spawn) — separate transport->flush_outbound() field
 *                                covers the spawn-safe ACK-keepalive flush at
 *                                sv_main.c:1148.
 *
 * Reference: GAME_TRANSPORT.md "Layers" + sv_main.c:1145-1156 spawn-safety
 * inline doc.
 */
static void wn_frame( int msec )
{
	(void)msec;
	WN_ProcessTimers();
	WN_FlushOutbound();
#if FEAT_WIREDNET_OBSERVER
	WN_SendDatagrams();
	WN_PushEvents();
	WN_TcpFrame();
#endif
#if FEAT_WIREDNET_CONTROL
	WN_ProcessCommandQueue();
#endif
}

/* flush_outbound vtable target — pre-spawn-guard ACK keepalive flush. */
static void wn_flush_outbound( void )
{
	WN_FlushOutbound();
}

static void wn_listen( int port ) { (void)port; /* QUIC already bound in WN_Init */ }

static void wn_complete_admission( conn_handle_t conn, qboolean accepted,
	netRefuseClass_t refusalClass ) {
	wn_game_conn_t *gc = wn_get_game_conn( conn );
	if ( !gc || !gc->conn ) return;
	WN_GameSendAdmission( gc->conn, gc, accepted, refusalClass );
}

static void wn_drop_client( conn_handle_t conn, const char *reason )
{
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	Com_Log( SEV_INFO, LOG_CH(ch_network), "*** wn_drop_client: conn=%llu reason_present=%d ***\n",
		(unsigned long long)conn, reason && reason[0] ? 1 : 0 );
	if ( cnx )
		picoquic_close( cnx, 0 );
}

static conn_handle_t wn_connect( const char *address, int port, const char *userinfo )
{
#if !defined(HEADLESS)
	netadr_t adr;
	/* "loopback" is returned by NET_AdrToString for NA_LOOPBACK addresses.
	 * Pass it through as-is so WN_ClientConnect handles the 127.0.0.1 mapping. */
	if ( !Q_stricmp( address, "loopback" ) ) {
		memset( &adr, 0, sizeof(adr) );
		adr.type = NA_LOOPBACK;
	} else if ( !NET_StringToAdr( address, &adr, NA_IP ) ) {
		return CONN_INVALID;
	}
	adr.port = BigShort( (short)port );
	WN_ClientConnect( &adr, userinfo, 0 );
	return CONN_CLIENT_HANDLE;
#else
	(void)address; (void)port; (void)userinfo;
	return CONN_INVALID;
#endif
}

static void wn_disconnect( conn_handle_t conn, const char *reason )
{
#if !defined(HEADLESS)
	if ( conn == CONN_CLIENT_HANDLE ) {
		Com_Log( SEV_INFO, LOG_CH(ch_network), "*** wn_disconnect: CONN_CLIENT_HANDLE reason=%s ***\n",
			reason ? reason : "NULL" );
		WN_ClientDisconnect();
		return;
	}
#endif
	wn_drop_client( conn, reason );
}

/* Client-side state-query vtable targets — Batch 3 rewire. */
#if !defined(HEADLESS)
static qboolean wn_is_connecting( void )
{
	return WN_ClientIsConnecting();
}

static qboolean wn_get_error( char *out, int outSize,
	netConnectErrorKind_t *kind )
{
	return WN_ClientHasError( out, outSize, kind );
}

static void wn_clear_error( void )
{
	WN_ClientClearError();
}
#endif

/* lookup_by_addr vtable target — addr → conn_handle reverse lookup. */
static conn_handle_t wn_lookup_by_addr( const netadr_t *addr )
{
	return WN_GetConnHandleByAddr( addr );
}

static void wn_send_unreliable( conn_handle_t conn, const byte *data, int len )
{
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( cnx )
		picoquic_queue_datagram_frame( cnx, (size_t)len, (const uint8_t *)data );
}

/*
 * WN_ServerRecvUsercmd — server-only datagram drain.
 *
 * Reads one datagram from any active game_conn's recv_queue (user commands
 * sent by game clients).  Called directly by SV_DrainQUICUsercmds; never
 * called from the client frame path.
 *
 * Keeping this separate from wn_recv_unreliable prevents the loopback
 * cross-contamination where CL_CheckSnapshotDatagrams would accidentally
 * dequeue user-command datagrams instead of snapshot datagrams.
 */
qboolean WN_ServerRecvUsercmd( conn_handle_t *conn_out,
	uint64_t *allocationIdOut, byte *buf, int *len_out )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc  = &wn.game_conns[i];
		wn_game_pkt_t  *pkt;

		if ( !WN_GameConnIdentityAccepted( gc->pub_handle, gc->allocation_id )
		     || gc->recv_tail == gc->recv_head )
			continue;

		pkt = &gc->recv_queue[gc->recv_tail];
		if ( pkt->len > *len_out ) {
			/* oversized — discard */
			gc->recv_tail = ( gc->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
			continue;
		}

		/* Return the published handle (QUIC: slot+1, byte-identical; in-mem:
		 * server end WN_APP_SVCONN_BASE+app_slot, 110+) so the server's
		 * svs.clients[].quic_conn match (and the outbound transport_for_handle
		 * routing) stays consistent for both. */
		*conn_out = gc->pub_handle;
		if ( allocationIdOut ) *allocationIdOut = gc->allocation_id;
		*len_out  = pkt->len;
		memcpy( buf, pkt->data, pkt->len );
		gc->recv_tail = ( gc->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
		return qtrue;
	}
	return qfalse;
}

/*
 * wn_recv_unreliable — client-side datagram drain (vtable entry).
 *
 * Called only from CL_CheckSnapshotDatagrams via transport->recv_unreliable.
 * Reads snapshots from a per-client recv ring — never touches gc->recv_queue.
 *
 * Scans all in-process client slots (0..WN_MAX_LOCAL_CLIENTS-1); pops one
 * datagram from the first non-empty ring and returns its per-slot handle. The
 * returned handle is decoded back to clientApps[slot] by WN_AppSlotForConn in
 * CL_CheckSnapshotDatagrams (cl_parse.c), so each client's snapshots route to its
 * own clientApp_t.
 *
 * Slot 0 (the integrated host) returns the historical client handle
 * CONN_CLIENT_HANDLE (9), not WN_APP_CONN_BASE+0 — keeping the host's recv routing
 * unchanged and reserving the WN_APP_CONN_BASE range for additional in-process
 * clients (slot i>0 returns WN_APP_CONN_BASE+i).
 *
 * Only slot 0 is initialized today; slots 1..3 are BSS-zero (initialized==qfalse,
 * head==tail==0) and are skipped, so the loop finds slot 0 and returns 9 with the
 * same pop logic as a single-client drain.
 */
static qboolean wn_recv_unreliable( conn_handle_t *conn_out, byte *buf, int *len_out )
{
#if !defined(HEADLESS)
	int slot;
	for ( slot = 0; slot < WN_MAX_LOCAL_CLIENTS; slot++ ) {
		wn_client_state_t *c = &wtcl_array[slot];
		wn_snap_pkt_t     *pkt;
		if ( !c->initialized || c->recv_tail == c->recv_head )
			continue;
		pkt = &c->recv_queue[c->recv_tail];
		if ( pkt->len <= *len_out ) {
			*conn_out = ( slot == 0 ) ? (conn_handle_t)CONN_CLIENT_HANDLE
			                          : (conn_handle_t)( WN_APP_CONN_BASE + slot );
			*len_out  = pkt->len;
			memcpy( buf, pkt->data, pkt->len );
			c->recv_tail = ( c->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
			return qtrue;
		}
		/* oversized — discard and keep scanning this drain call */
		c->recv_tail = ( c->recv_tail + 1 ) % WN_GAME_QUEUE_SIZE;
	}
#else
	(void)conn_out; (void)buf; (void)len_out;
#endif
	return qfalse;
}

static qboolean wn_send_reliable( conn_handle_t conn, int channel,
                                 const byte *data, int len )
{
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	uint64_t        stream_id;
	qboolean        sending_from_client;
	byte            framed_inline[MAX_MSGLEN + 2];
	byte           *framed     = framed_inline;
	qboolean        framed_heap = qfalse;
	const byte     *send_data = data;
	int             send_len  = len;
	int ret;
	if ( !cnx || !data || len <= 0 )
		return qfalse;
	sending_from_client = ( conn == CONN_CLIENT_HANDLE ) ? qtrue : qfalse;
	stream_id = wn_resolve_reliable_send_stream( cnx, channel, sending_from_client );
	if ( stream_id == UINT64_MAX ) {
		COM_WARN( LOG_CH(ch_network), "QUIC: invalid reliable channel %d for conn %llu\n",
			channel, (unsigned long long)conn );
		return qfalse;
	}
	if ( !wn_reliable_channel_allows_fixed_stream( channel ) ) {
		/* CHAN_BOOTSTRAP may exceed MAX_MSGLEN — allow up to WN_BOOTSTRAP_MAX. */
		int max_payload = ( channel == CHAN_BOOTSTRAP ) ? WN_BOOTSTRAP_MAX : MAX_MSGLEN;
		if ( !wn_reliable_channel_is_game( channel ) || len > max_payload ) {
			COM_WARN( LOG_CH(ch_network), "QUIC: invalid reliable payload for channel %d\n",
				channel );
			return qfalse;
		}
		if ( channel == CHAN_BOOTSTRAP ) {
			framed = (byte *)Z_Malloc( len + 2 );
			framed_heap = qtrue;
		}
		framed[0] = WN_GAME_REL_VERSION;
		framed[1] = (byte)channel;
		memcpy( framed + 2, data, (size_t)len );
		send_data = framed;
		send_len  = len + 2;
	}
	/* Reliable app messages are one logical message per QUIC stream. Session
	 * control remains on stream 0; other channels use fresh local streams and
	 * rely on stream FIN to delimit the message. */
	ret = picoquic_add_to_stream( cnx, stream_id, send_data, (size_t)send_len,
		wn_reliable_channel_allows_fixed_stream( channel ) ? 0 : 1 );
	if ( framed_heap ) Z_Free( framed );
	Com_Log( SEV_TRACE, LOG_CH(ch_network_common), "QUIC: wn_send_reliable conn=%llu channel=%d len=%d ret=%d\n",
		(unsigned long long)conn, channel, len, ret );
	return ret == 0 ? qtrue : qfalse;
}

qboolean WN_ServerRecvReliable( conn_handle_t *conn_out,
	uint64_t *allocationIdOut, int *channel_out, byte *buf, int *len_out )
{
	int i;
	int channel;

	/* Server-side: drain game_conns rel_recv slots */
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		wn_game_conn_t *gc = &wn.game_conns[i];
		if ( !WN_GameConnIdentityAccepted( gc->pub_handle, gc->allocation_id ) )
			continue;
		channel = -1;
		if ( !wn_reliable_queue_pop( gc->rel_queue, &gc->rel_head, &gc->rel_tail,
			&channel, buf, len_out ) ) {
			continue;
		}
		/* Published handle (QUIC slot+1 byte-identical; in-mem server-end 110+)
		 * so the server's svs.clients[].quic_conn match stays consistent. */
		*conn_out    = gc->pub_handle;
		if ( allocationIdOut ) *allocationIdOut = gc->allocation_id;
		*channel_out = channel;
		return qtrue;
	}

	return qfalse;
}

#if !defined(HEADLESS)
/* Return a pointer to the pending bootstrap payload and mark it consumed.
 * The pointer is valid until the next WN_ClientConnect call.
 * Returns qfalse if no bootstrap is pending. */
/* WN_HasInmemClient — true if any active client connection is in-process (no
 * picoquic). An in-process client produces no UDP traffic, so select() never
 * wakes NET_Event to drive the client recv consumers; NET_Sleep uses this to pump
 * them every frame instead. A QUIC client has quic != NULL and is unaffected
 * (returns qfalse for a pure-QUIC client).
 *
 * Scans all client slots so the pump fires when any in-process client is live.
 * Only slot 0 is initialized today (slots 1..3 are BSS-zero, initialized==qfalse),
 * so this returns the single-client result. */
qboolean WN_HasInmemClient( void )
{
	int slot;
	for ( slot = 0; slot < WN_MAX_LOCAL_CLIENTS; slot++ ) {
		if ( wtcl_array[slot].initialized && wtcl_array[slot].quic == NULL )
			return qtrue;
	}
	return qfalse;
}

qboolean WN_ClientConsumeBootstrap( int app_slot, const byte **data_out, int *len_out )
{
	wn_client_state_t *c;
	if ( app_slot < 0 || app_slot >= WN_MAX_LOCAL_CLIENTS )
		return qfalse;
	c = &wtcl_array[app_slot];
	if ( !c->initialized || !c->bootstrap_recv_ready )
		return qfalse;
	*data_out = c->bootstrap_recv_data;
	*len_out  = c->bootstrap_recv_len;
	c->bootstrap_recv_ready = qfalse;
	c->bootstrap_recv_len   = 0;
	return qtrue;
}

/* Drain one reliable message from a specific client's srv->cli ring
 * (wtcl_array[app_slot].rel_queue). This is the client-side reliable receive ONLY
 * — it never touches the server-side game_conns[].rel_queue (cli->srv commands),
 * keeping the two directions separate in listen-server mode. */
qboolean WN_ClientRecvReliable( int app_slot, int *channel_out, byte *buf, int *len_out )
{
	wn_client_state_t *c;
	if ( app_slot < 0 || app_slot >= WN_MAX_LOCAL_CLIENTS )
		return qfalse;
	c = &wtcl_array[app_slot];
	if ( c->initialized &&
	     wn_reliable_queue_pop( c->rel_queue, &c->rel_head, &c->rel_tail,
	         channel_out, buf, len_out ) ) {
		return qtrue;
	}
	return qfalse;
}
#endif

static qboolean wn_recv_reliable( conn_handle_t *conn_out, int *channel_out,
	byte *buf, int *len_out )
{
	/* The server drains its direction through WN_ServerRecvReliable, which also
	 * returns allocation identity. This vtable slot is client-only so it cannot
	 * discard that identity or cross-route cli->srv traffic in a listen server. */
#if !defined(HEADLESS)
	if ( WN_ClientRecvReliable( 0, channel_out, buf, len_out ) ) {
		if ( conn_out ) *conn_out = CONN_CLIENT_HANDLE;
		return qtrue;
	}
#endif
	return qfalse;
}

static int wn_get_ping( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( !cnx )
		return -1;
	picoquic_get_default_path_quality( cnx, &q );
	return (int)( q.rtt / 1000 );
}

static float wn_get_loss( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( !cnx )
		return 0.0f;
	picoquic_get_default_path_quality( cnx, &q );
	return ( q.sent > 0 ) ? (float)q.lost / (float)q.sent : 0.0f;
}

static int wn_get_bandwidth( conn_handle_t conn )
{
	picoquic_path_quality_t q;
	picoquic_cnx_t *cnx = wn_get_cnx( conn );
	if ( !cnx )
		return 0;
	picoquic_get_default_path_quality( cnx, &q );
	return (int)( q.receive_rate_estimate * 8 / 1000 );
}

static void wn_get_address_string( conn_handle_t conn, char *buf, int buflen )
{
#if !defined(HEADLESS)
	if ( conn == CONN_CLIENT_HANDLE ) {
		Q_strncpyz( buf, NET_AdrToStringwPort( &wtcl_array[0].server_addr ), buflen );
		return;
	}
#endif
	{
		wn_game_conn_t *gc = wn_get_game_conn( conn );
		if ( gc && gc->conn )
			Q_strncpyz( buf, NET_AdrToStringwPort( &gc->conn->addr ), buflen );
		else
			Q_strncpyz( buf, "<unknown>", buflen );
	}
}

#if !defined(__EMSCRIPTEN__)
transport_t quic_transport = {
	/* Lifecycle */
	wn_shutdown,
	wn_frame,
	wn_flush_outbound,

	/* Server */
	wn_listen,
	NULL,              /* accept_callback — set by caller */
	NULL,              /* ready_callback  — set by caller */
	NULL,              /* closed_callback — set by caller */
	wn_complete_admission,
	wn_drop_client,
	NULL,              /* drain_usercmds  — registered by server (sv_init.c) */
	wn_lookup_by_addr,

	/* Client */
	wn_connect,
	wn_disconnect,
#if !defined(HEADLESS)
	wn_is_connecting,
	wn_get_error,
	wn_clear_error,
#else
	NULL,              /* is_connecting — client-only, NULL in dedicated build */
	NULL,              /* get_error     — client-only, NULL in dedicated build */
	NULL,              /* clear_error   — client-only, NULL in dedicated build */
#endif

	/* Unreliable */
	wn_send_unreliable,
	wn_recv_unreliable,

	/* Reliable */
	wn_send_reliable,
	wn_recv_reliable,

	/* Metrics */
	wn_get_ping,
	wn_get_loss,
	wn_get_bandwidth,
	wn_get_address_string,
};
#endif
