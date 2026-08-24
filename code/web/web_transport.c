// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * Browser transport boundary for the first client frame. Raw UDP/PicoQUIC is
 * intentionally unavailable in a browser. This adapter publishes a complete,
 * fail-closed transport vtable so engine code never falls through to a native
 * socket path. A later WebTransport adapter can replace these operations
 * without changing qcommon or client ownership.
 */
#include "../qcommon/wired/net/wn_local.h"

#include <string.h>

static char s_connectError[160];

static void WebTransport_NoVoid( void ) {}
static void WebTransport_Frame( int msec ) { (void)msec; }
static void WebTransport_Listen( int port ) { (void)port; }
static void WebTransport_CompleteAdmission( conn_handle_t conn,
		qboolean accepted, netRefuseClass_t refusalClass ) {
	(void)conn; (void)accepted; (void)refusalClass;
}
static void WebTransport_Drop( conn_handle_t conn, const char *reason ) {
	(void)conn; (void)reason;
}
static conn_handle_t WebTransport_Lookup( const netadr_t *address ) {
	(void)address; return CONN_INVALID;
}
static conn_handle_t WebTransport_Connect( const char *address, int port,
		const char *userinfo ) {
	(void)address; (void)port; (void)userinfo;
	Q_strncpyz( s_connectError,
		"Browser network transport is not available in this build",
		sizeof( s_connectError ) );
	return CONN_INVALID;
}
static void WebTransport_Disconnect( conn_handle_t conn, const char *reason ) {
	(void)conn; (void)reason;
}
static qboolean WebTransport_IsConnecting( void ) { return qfalse; }
static qboolean WebTransport_GetError( char *out, int outSize,
		netConnectErrorKind_t *kind ) {
	if ( !s_connectError[0] ) return qfalse;
	if ( out && outSize > 0 ) Q_strncpyz( out, s_connectError, outSize );
	if ( kind ) *kind = NET_CONNECT_ERROR_GENERIC;
	return qtrue;
}
static void WebTransport_ClearError( void ) { s_connectError[0] = '\0'; }
static void WebTransport_SendUnreliable( conn_handle_t conn,
		const byte *data, int length ) { (void)conn; (void)data; (void)length; }
static qboolean WebTransport_RecvUnreliable( conn_handle_t *conn,
		byte *data, int *length ) {
	/* Browser builds still use the same-process transport for an integrated
	 * `map` session. The native QUIC vtable normally owns this global pull seam
	 * and drains the shared per-client snapshot ring; the browser fail-closed
	 * adapter must preserve that local route even though external UDP/QUIC is
	 * unavailable. Future WebTransport traffic can be polled before this local
	 * fallback without changing client/qcommon ownership. */
	if ( inmem_transport.recv_unreliable )
		return inmem_transport.recv_unreliable( conn, data, length );
	if ( length ) *length = 0;
	return qfalse;
}
static qboolean WebTransport_SendReliable( conn_handle_t conn, int channel,
		const byte *data, int length ) {
	(void)conn; (void)channel; (void)data; (void)length; return qfalse;
}
static qboolean WebTransport_RecvReliable( conn_handle_t *conn, int *channel,
		byte *data, int *length ) {
	(void)conn; (void)channel; (void)data; if ( length ) *length = 0;
	return qfalse;
}
static int WebTransport_Ping( conn_handle_t conn ) { (void)conn; return -1; }
static float WebTransport_Loss( conn_handle_t conn ) { (void)conn; return 0.0f; }
static int WebTransport_Bandwidth( conn_handle_t conn ) { (void)conn; return 0; }
static void WebTransport_Address( conn_handle_t conn, char *out, int outSize ) {
	(void)conn; if ( out && outSize > 0 ) out[0] = '\0';
}

#define WIRED_WEB_TRANSPORT_INITIALIZER { \
	.shutdown = WebTransport_NoVoid, .frame = WebTransport_Frame, \
	.flush_outbound = WebTransport_NoVoid, .listen = WebTransport_Listen, \
	.complete_admission = WebTransport_CompleteAdmission, \
	.drop_client = WebTransport_Drop, .lookup_by_addr = WebTransport_Lookup, \
	.connect = WebTransport_Connect, .disconnect = WebTransport_Disconnect, \
	.is_connecting = WebTransport_IsConnecting, .get_error = WebTransport_GetError, \
	.clear_error = WebTransport_ClearError, \
	.send_unreliable = WebTransport_SendUnreliable, \
	.recv_unreliable = WebTransport_RecvUnreliable, \
	.send_reliable = WebTransport_SendReliable, \
	.recv_reliable = WebTransport_RecvReliable, .get_ping = WebTransport_Ping, \
	.get_loss = WebTransport_Loss, .get_bandwidth = WebTransport_Bandwidth, \
	.get_address_string = WebTransport_Address }

transport_t quic_transport = WIRED_WEB_TRANSPORT_INITIALIZER;
wn_state_t wn;

void WN_Init( void ) {
	// SV_Init publishes callbacks, then SV_Startup calls WN_Init again when the
	// first map becomes live. Match the native transport's idempotent lifecycle;
	// resetting here would erase admission callbacks and strand local connects.
	if ( wn.initialized ) return;
	Q_SecureZeroMemory( &wn, sizeof( wn ) );
	wn.initialized = qtrue;
	wn.next_game_conn_allocation_id = 1u;
	s_connectError[0] = '\0';
	transport = &quic_transport;
}

void WN_Shutdown( void ) {
	transport = NULL;
	Q_SecureZeroMemory( &wn, sizeof( wn ) );
	s_connectError[0] = '\0';
}

void WN_RegisterCommands( void ) {}

#if FEAT_WIREDNET_OBSERVER
void WN_EmitKill( int attacker, int victim, int mod,
		const vec3_t attackerPos, const vec3_t victimPos ) {
	(void)attacker; (void)victim; (void)mod; (void)attackerPos; (void)victimPos;
}
void WN_EmitDamage( int attacker, int victim, int damage, int mod,
		const vec3_t attackerPos, const vec3_t victimPos ) {
	(void)attacker; (void)victim; (void)damage; (void)mod;
	(void)attackerPos; (void)victimPos;
}
void WN_EmitItemPickup( int client, const char *item, const vec3_t pos ) {
	(void)client; (void)item; (void)pos;
}
void WN_EmitChat( int client, const char *message, qboolean teamOnly ) {
	(void)client; (void)message; (void)teamOnly;
}
void WN_EmitMatchEvent( const char *type, const char *data ) {
	(void)type; (void)data;
}
void WN_EmitBotEvent( int bot, const char *type, int param1, int param2,
		const vec3_t pos ) {
	(void)bot; (void)type; (void)param1; (void)param2; (void)pos;
}
#endif
