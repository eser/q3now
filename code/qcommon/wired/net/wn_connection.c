/*
===========================================================================
wn_connection.c — QUIC connection lifecycle, auth, permissions
===========================================================================
*/
#include "wn_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_network, "network" );

/*
====================
WN_FindConnection

Look up a connection by its picoquic handle.
Returns NULL if not found.
====================
*/
wn_connection_t *WN_FindConnection( picoquic_cnx_t *cnx )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( wn.connections[i].active && wn.connections[i].cnx == cnx )
			return &wn.connections[i];
	}
	return NULL;
}


/*
====================
WN_AllocConnection

Allocate a free connection slot for a new QUIC peer.
Returns NULL if all slots are in use.
====================
*/
wn_connection_t *WN_AllocConnection( picoquic_cnx_t *cnx, netadr_t *from )
{
	int i;
	for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
		if ( !wn.connections[i].active ) {
			wn_connection_t *conn = &wn.connections[i];
			memset( conn, 0, sizeof( *conn ) );
			conn->cnx = cnx;
			conn->active = qtrue;
			conn->connect_time = Sys_Microseconds();
			conn->perm = WN_PERM_OBSERVER_MEMBER_USER; // default: lowest privilege

			if ( from ) {
				conn->addr = *from;
			}

			wn.num_connections++;
			return conn;
		}
	}
	return NULL;
}


/*
====================
WN_FreeConnection

Release a connection slot back to the pool.
====================
*/
void WN_FreeConnection( wn_connection_t *conn )
{
	if ( !conn || !conn->active )
		return;

	conn->active = qfalse;

	if ( conn->game_conn )
		WN_GameFreeConn( conn->game_conn );

	conn->cnx = NULL;
	wn.num_connections--;
}



/*
====================
WN_LogConnect / WN_LogDisconnect

Print structured connection events to server console.
Format: "QUIC: client connected from X [OBSERVER+MEMBER+USER] (N/M slots)"
====================
*/
void WN_LogConnect( wn_connection_t *conn )
{
	const char *conn_type = WN_HasPermPlayer(conn->perm)  ? "PLAYER"   : "OBSERVER";
	const char *role      = WN_HasPermLeader(conn->perm)   ? "LEADER"   : "MEMBER";
	const char *auth      = WN_HasPermAdmin(conn->perm)    ? "ADMIN"    : "USER";

	Com_Log( SEV_DEBUG, LOG_CH(ch_network), "QUIC: client connected from %s [%s+%s+%s] (%d/%d slots)\n",
		NET_AdrToString( &conn->addr ),
		conn_type, role, auth,
		wn.num_connections, wn.sv_wirednetMaxClients->integer );
}

void WN_LogDisconnect( wn_connection_t *conn, const char *reason )
{
	Com_Log( SEV_INFO, LOG_CH(ch_network), "QUIC: client disconnected from %s (reason: %s)\n",
		NET_AdrToString( &conn->addr ),
		reason ? reason : "unknown" );
}



/*
====================
WN_HandleCapabilityNegotiation

Stream 0x00 dispatch — binary TLV only.
If the client sends a JSON '{' byte, reject and close.
Otherwise route to the game handshake handler (TLV CONNECT / READY).
====================
*/
void WN_HandleCapabilityNegotiation( wn_connection_t *conn, uint64_t stream_id,
                                      const byte *data, int len )
{
	/* FIN-only callback: nothing to process. */
	if ( len <= 0 )
		return;

	if ( !conn || !conn->active )
		return;

	/* Reject legacy JSON clients — binary TLV is the only accepted protocol. */
	if ( (uint8_t)data[0] == '{' ) {
		static const char err[] = "{\"error\":\"Legacy JSON protocol no longer supported\"}";
		picoquic_add_to_stream( conn->cnx, stream_id,
			(const uint8_t *)err, sizeof(err) - 1, 0 );
		picoquic_close( conn->cnx, 1 );
		return;
	}

	WN_GameHandleHandshake( conn, stream_id, data, len );
}
