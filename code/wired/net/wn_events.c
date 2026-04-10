/*
===========================================================================
wn_events.c — Game event emission + ring buffer + recording

Events flow:
  g_combat.c → VM syscall → WN_EmitKill() → ring buffer → QUIC stream
                                                           → .q3events file
===========================================================================
*/
#include "wn_local.h"

#if FEAT_WIREDNET_OBSERVE

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

#if FEAT_BOT_IMPROVEMENTS
void WN_EmitBotEvent( int bot_id, const char *event_type,
                        int param1, int param2, const vec3_t pos )
{
	byte buf[256];
	int len;

	if ( !wn.initialized )
		return;

	len = WN_EncodeBotEvent( buf, sizeof(buf), bot_id, event_type, param1, param2, pos );
	if ( len > 0 )
		WN_EventRingPush( buf, len );
}
#endif


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

#endif // FEAT_WIREDNET_OBSERVE
