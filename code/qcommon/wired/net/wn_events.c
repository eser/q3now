/*
===========================================================================
wn_events.c — Game event emission + ring buffer + recording

Events flow:
  g_combat.c → VM syscall → WN_EmitKill() → ring buffer → QUIC stream
                                                           → .q3events file
===========================================================================
*/
#include "wn_local.h"
#include "../../../server/server.h"  // for svs, sv_maxclients, client_t

#if FEAT_WIREDNET_OBSERVER

/*
====================
WN_JsonEscapeStr (local)

Escape a string for embedding in a JSON value: replaces backslash, double-quote,
and drops control characters < 0x20. Output is always null-terminated.
====================
*/
static void WN_JsonEscapeStr( const char *in, char *out, int out_size )
{
	int           i = 0, j = 0;
	unsigned char c;
	while ( (c = (unsigned char)in[i]) && j < out_size - 2 ) {
		i++;
		if ( c < 0x20 ) continue;
		// Strip Q3 color codes: ^ followed by any non-null, non-^ character
		if ( c == '^' && in[i] != '\0' && in[i] != '^' ) { i++; continue; }
		if ( c == '"' || c == '\\' ) {
			if ( j >= out_size - 3 ) break;
			out[j++] = '\\';
		}
		out[j++] = (char)c;
	}
	out[j] = '\0';
}


/*
====================
WN_JsonEventRingPush

Write a pre-formatted JSON object string to the parallel JSON event ring.
Called alongside WN_EventRingPush so /status.json never needs to decode msgpack.
====================
*/
void WN_JsonEventRingPush( const char *json )
{
	int              slot  = (int)( wn.json_write_idx % WN_JSON_RING_SIZE );
	wn_json_event_t *entry = &wn.json_events[slot];

	entry->seq = wn.json_write_idx;
	Q_strncpyz( entry->json, json, sizeof(entry->json) );
	wn.json_write_idx++;
}

#if 0  // reopen FEAT_WIREDNET_OBSERVER below — avoids re-indenting the whole file
#endif

/*
====================
WN_EventRingPush

Push an encoded event into the ring buffer.
Oldest events are silently evicted on overflow.
====================
*/
void WN_EventRingPush( const byte *data, int len )
{
	int ring_count = (int)( sizeof(wn.events) / sizeof(wn.events[0]) );
	int idx = (int)( wn.event_write_idx % ring_count );
	wn_event_t *evt = &wn.events[idx];

	evt->seq  = wn.event_seq++;
	evt->time = (uint32_t)( Sys_Milliseconds() & 0xFFFFFFFF );

	if ( len > (int)sizeof(evt->data) )
		len = (int)sizeof(evt->data);

	Com_Memcpy( evt->data, data, len );
	evt->data_len = len;

	wn.event_write_idx++;

	// Write to recording file if active
	WN_RecordEvent( data, len );

	/* Also push to game clients on the QUIC game reliable events channel. */
	{
		int i;
		for ( i = 0; i < WN_MAX_CLIENTS; i++ ) {
			wn_game_conn_t *gc = &wn.game_conns[i];
			if ( gc->active && gc->conn && gc->conn->cnx &&
			     gc->hs_state == WN_GAME_HS_ACCEPTED ) {
				if ( transport ) {
					transport->send_reliable( (conn_handle_t)( i + 1 ), CHAN_EVENTS,
						(const byte *)data, len );
				}
			}
		}
	}
}


/*
====================
WN_EventRingRead

Read events from the ring buffer starting at from_seq.
Returns the number of events written to out[].
====================
*/
int WN_EventRingRead( uint64_t from_seq, wn_event_t *out, int max_events )
{
	int ring_count = (int)( sizeof(wn.events) / sizeof(wn.events[0]) );
	int count = 0;
	uint64_t i;

	for ( i = from_seq; i < wn.event_seq && count < max_events; i++ ) {
		int idx = (int)( i % ring_count );
		wn_event_t *evt = &wn.events[idx];

		// Check if this slot still holds the expected event (not evicted)
		if ( evt->seq != i )
			continue; // gap — event was evicted

		out[count++] = *evt;
	}

	return count;
}


/*
====================
WN_EmitKill

Called from sv_game.c syscall handler when game code reports a kill.
Encodes a kill event with positions and pushes to ring buffer.
====================
*/
void WN_EmitKill( int attacker, int victim, int mod,
                    const vec3_t attacker_pos, const vec3_t victim_pos )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	len = WN_EncodeKillEvent( buf, sizeof(buf), attacker, victim, mod, attacker_pos, victim_pos );
	if ( len > 0 )
		WN_EventRingPush( buf, len );

	// JSON ring — kill events for /status.json (no positions)
	{
		char atk_esc[128], vic_esc[128], jbuf[WN_JSON_EVENT_MAX];
		int  maxcl = ( sv_maxclients && svs.clients ) ? sv_maxclients->integer : 0;
		const char *atk_name = ( attacker >= 0 && attacker < maxcl )
			? svs.clients[attacker].name : "<world>";
		const char *vic_name = ( victim >= 0 && victim < maxcl )
			? svs.clients[victim].name : "unknown";
		if ( !atk_name || !atk_name[0] ) atk_name = "<world>";
		if ( !vic_name || !vic_name[0] ) vic_name = "unknown";
		WN_JsonEscapeStr( atk_name, atk_esc, sizeof(atk_esc) );
		WN_JsonEscapeStr( vic_name, vic_esc, sizeof(vic_esc) );
		Com_sprintf( jbuf, sizeof(jbuf),
			"{\"type\":\"kill\",\"time\":%u,\"attacker\":\"%s\",\"victim\":\"%s\",\"weapon\":\"%s\"}",
			(unsigned)Sys_Milliseconds(), atk_esc, vic_esc, BG_ModShortName(mod) );
		WN_JsonEventRingPush( jbuf );
	}
}

void WN_EmitDamage( int attacker, int victim, int damage, int mod,
                      const vec3_t attacker_pos, const vec3_t victim_pos )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	len = WN_EncodeDamageEvent( buf, sizeof(buf), attacker, victim, damage, mod, attacker_pos, victim_pos );
	if ( len > 0 )
		WN_EventRingPush( buf, len );
}

void WN_EmitItemPickup( int client, const char *item, const vec3_t pos )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	len = WN_EncodeItemPickupEvent( buf, sizeof(buf), client, item, pos );
	if ( len > 0 )
		WN_EventRingPush( buf, len );

	// JSON ring — item events for /status.json (no position)
	{
		char pl_esc[128], item_esc[64], jbuf[WN_JSON_EVENT_MAX];
		int  maxcl = ( sv_maxclients && svs.clients ) ? sv_maxclients->integer : 0;
		const char *pl_name = ( client >= 0 && client < maxcl )
			? svs.clients[client].name : "unknown";
		if ( !pl_name || !pl_name[0] ) pl_name = "unknown";
		WN_JsonEscapeStr( pl_name, pl_esc, sizeof(pl_esc) );
		WN_JsonEscapeStr( item ? item : "", item_esc, sizeof(item_esc) );
		Com_sprintf( jbuf, sizeof(jbuf),
			"{\"type\":\"item\",\"time\":%u,\"player\":\"%s\",\"item\":\"%s\"}",
			(unsigned)Sys_Milliseconds(), pl_esc, item_esc );
		WN_JsonEventRingPush( jbuf );
	}
}

void WN_EmitChat( int client, const char *msg, qboolean teamOnly )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	len = WN_EncodeChatEvent( buf, sizeof(buf), client, msg, teamOnly );
	if ( len > 0 )
		WN_EventRingPush( buf, len );

	// JSON ring — chat events for /status.json
	{
		char pl_esc[128], msg_esc[200], jbuf[WN_JSON_EVENT_MAX];
		int  maxcl = ( sv_maxclients && svs.clients ) ? sv_maxclients->integer : 0;
		const char *pl_name = ( client >= 0 && client < maxcl )
			? svs.clients[client].name : "unknown";
		if ( !pl_name || !pl_name[0] ) pl_name = "unknown";
		WN_JsonEscapeStr( pl_name, pl_esc, sizeof(pl_esc) );
		WN_JsonEscapeStr( msg ? msg : "", msg_esc, sizeof(msg_esc) );
		Com_sprintf( jbuf, sizeof(jbuf),
			"{\"type\":\"chat\",\"time\":%u,\"player\":\"%s\",\"message\":\"%s\",\"team\":%s}",
			(unsigned)Sys_Milliseconds(), pl_esc, msg_esc, teamOnly ? "true" : "false" );
		WN_JsonEventRingPush( jbuf );
	}
}

void WN_EmitMatchEvent( const char *type, const char *data )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	/* WN_EncodeMatchEvent is not yet implemented in wn_msgpack.c.
	 * Add after the kill/damage codec pattern: msgpack map with keys
	 * "type" (string), "data" (string), "seq" (u64), "time" (u32 ms). */
	(void)buf; (void)len; (void)type; (void)data;
}

void WN_EmitBotEvent( int bot_id, const char *event_type,
                        int param1, int param2, const vec3_t pos )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	if ( !event_type )
		event_type = "";

	len = WN_EncodeBotEvent( buf, sizeof(buf), bot_id, event_type, param1, param2, pos );
	if ( len > 0 )
		WN_EventRingPush( buf, len );
}


/*
====================
WN_PushEvents

Push buffered events from ring buffer to all connected QUIC observers.
Called from SV_Frame at sv_wirednetEventRate Hz.
====================
*/
void WN_PushEvents( void )
{
	if ( !wn.initialized )
		return;

	/* Not yet implemented: iterate wn.connections[], for each active observer
	 * call WN_EventRingRead(last_seq) and write results to its event_stream_id
	 * via picoquic_add_to_stream(). Rate-limit to sv_wirednetEventRate Hz. */
}


/*
====================
WN_SendDatagrams

Encode and send game state as QUIC datagrams to all observers.
Called from SV_Frame at sv_wirednetStateRate Hz.
====================
*/
void WN_SendDatagrams( void )
{
	if ( !wn.initialized )
		return;

	/* Not yet implemented: call WN_EncodeStateUpdate(buf, sizeof(buf)) then
	 * picoquic_queue_datagram_frame() for each active observer connection.
	 * Rate-limit to sv_wirednetStateRate Hz. */
}

#endif // FEAT_WIREDNET_OBSERVER
