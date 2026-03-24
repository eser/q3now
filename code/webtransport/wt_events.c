/*
===========================================================================
wt_events.c — Game event emission + ring buffer + recording

Events flow:
  g_combat.c → VM syscall → QUIC_EmitKill() → ring buffer → QUIC stream
                                                           → .q3events file
===========================================================================
*/
#include "wt_local.h"

#if FEAT_QUIC_OBSERVE

/*
====================
WT_EventRingPush

Push an encoded event into the ring buffer.
Oldest events are silently evicted on overflow.
====================
*/
void WT_EventRingPush( const byte *data, int len )
{
	int ring_count = (int)( sizeof(wt.events) / sizeof(wt.events[0]) );
	int idx = (int)( wt.event_write_idx % ring_count );
	wt_event_t *evt = &wt.events[idx];

	evt->seq  = wt.event_seq++;
	evt->time = (uint32_t)( Sys_Milliseconds() & 0xFFFFFFFF );

	if ( len > (int)sizeof(evt->data) )
		len = (int)sizeof(evt->data);

	Com_Memcpy( evt->data, data, len );
	evt->data_len = len;

	wt.event_write_idx++;

	// Write to recording file if active
	WT_RecordEvent( data, len );
}


/*
====================
WT_EventRingRead

Read events from the ring buffer starting at from_seq.
Returns the number of events written to out[].
====================
*/
int WT_EventRingRead( uint64_t from_seq, wt_event_t *out, int max_events )
{
	int ring_count = (int)( sizeof(wt.events) / sizeof(wt.events[0]) );
	int count = 0;
	uint64_t i;

	for ( i = from_seq; i < wt.event_seq && count < max_events; i++ ) {
		int idx = (int)( i % ring_count );
		wt_event_t *evt = &wt.events[idx];

		// Check if this slot still holds the expected event (not evicted)
		if ( evt->seq != i )
			continue; // gap — event was evicted

		out[count++] = *evt;
	}

	return count;
}


/*
====================
QUIC_EmitKill

Called from sv_game.c syscall handler when game code reports a kill.
Encodes a kill event with positions and pushes to ring buffer.
====================
*/
void QUIC_EmitKill( int attacker, int victim, int mod,
                    const vec3_t attacker_pos, const vec3_t victim_pos )
{
	byte buf[256];
	int len;

	if ( !wt.initialized )
		return;

	len = WT_EncodeKillEvent( buf, sizeof(buf), attacker, victim, mod, attacker_pos, victim_pos );
	if ( len > 0 )
		WT_EventRingPush( buf, len );
}

void QUIC_EmitDamage( int attacker, int victim, int damage, int mod,
                      const vec3_t attacker_pos, const vec3_t victim_pos )
{
	byte buf[256];
	int len;

	if ( !wt.initialized )
		return;

	len = WT_EncodeDamageEvent( buf, sizeof(buf), attacker, victim, damage, mod, attacker_pos, victim_pos );
	if ( len > 0 )
		WT_EventRingPush( buf, len );
}

void QUIC_EmitItemPickup( int client, const char *item, const vec3_t pos )
{
	byte buf[256];
	int len;

	if ( !wt.initialized )
		return;

	len = WT_EncodeItemPickupEvent( buf, sizeof(buf), client, item, pos );
	if ( len > 0 )
		WT_EventRingPush( buf, len );
}

void QUIC_EmitChat( int client, const char *msg, qboolean teamOnly )
{
	byte buf[256];
	int len;

	if ( !wt.initialized )
		return;

	len = WT_EncodeChatEvent( buf, sizeof(buf), client, msg, teamOnly );
	if ( len > 0 )
		WT_EventRingPush( buf, len );
}

void QUIC_EmitMatchEvent( const char *type, const char *data )
{
	byte buf[256];
	int len;

	if ( !wt.initialized )
		return;

	// Match events use a simple map: { "type": type, "data": data, "seq": N, "time": T }
	// TODO: implement WT_EncodeMatchEvent in wt_msgpack.c
	(void)buf; (void)len; (void)type; (void)data;
}


/*
====================
QUIC_PushEvents

Push buffered events from ring buffer to all connected QUIC observers.
Called from SV_Frame at sv_quicEventRate Hz.
====================
*/
void QUIC_PushEvents( void )
{
	if ( !wt.initialized )
		return;

	// TODO: iterate connections, write events to each connection's event stream (0x03)
	// Rate-limit to sv_quicEventRate Hz
}


/*
====================
QUIC_SendDatagrams

Encode and send game state as QUIC datagrams to all observers.
Called from SV_Frame at sv_quicStateRate Hz.
====================
*/
void QUIC_SendDatagrams( void )
{
	if ( !wt.initialized )
		return;

	// TODO: encode StateUpdate, send to each connection via picoquic_queue_datagram_frame
	// Rate-limit to sv_quicStateRate Hz
}

#endif // FEAT_QUIC_OBSERVE
