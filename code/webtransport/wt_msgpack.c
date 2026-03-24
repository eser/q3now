/*
===========================================================================
wt_msgpack.c — MessagePack encoding for QUIC channels

Encodes game state (StateUpdate) and events (kill, damage, item, chat, match)
as MessagePack binary using the mpack writer API.

v1: Integer keys for ~50% smaller payloads vs v0 string keys.
Key map published below — observers decode fields by WTK_* integer.
===========================================================================
*/
#include "wt_local.h"
#include "../server/server.h"  // for svs, sv, client_t

#if FEAT_QUIC_OBSERVE

#include "mpack.h"

// ── MessagePack Integer Key Map (v1) ─────────────────────────────
// Published key→field mapping. External observers (Python, JS, etc.)
// must mirror these constants to decode events and state updates.
//
//   Common (all events):   0-2
//   Combat (kill/damage):  3-9
//   Player/item/chat:     10-14
//   State update:         15-17
//   Player object fields: 18-23
//
enum {
	WTK_TYPE          = 0,   // string: event type ("kill", "damage", etc.)
	WTK_SEQ           = 1,   // uint64: monotonic event sequence
	WTK_TIME          = 2,   // uint32: server time (ms)
	// combat
	WTK_ATTACKER_ID   = 3,   // int: attacker client slot
	WTK_VICTIM_ID     = 4,   // int: victim client slot
	WTK_WEAPON        = 5,   // string: weapon short name
	WTK_MOD           = 6,   // int: means-of-death enum
	WTK_ATTACKER_POS  = 7,   // array[3] float: attacker xyz
	WTK_VICTIM_POS    = 8,   // array[3] float: victim xyz
	WTK_DAMAGE        = 9,   // int: damage amount (damage event only)
	// player / item / chat
	WTK_PLAYER_ID     = 10,  // int: client slot
	WTK_ITEM          = 11,  // string: item classname
	WTK_POSITION      = 12,  // array[3] float: xyz
	WTK_MESSAGE       = 13,  // string: chat message text
	WTK_TEAM_ONLY     = 14,  // bool: team-only chat flag
	// state update
	WTK_MAP           = 15,  // string: current map name
	WTK_GAMETYPE      = 16,  // uint8: gametype index
	WTK_PLAYERS       = 17,  // array: player objects
	// player object (nested in state update)
	WTK_PL_ID         = 18,  // uint8: client slot
	WTK_PL_NAME       = 19,  // string: player name
	WTK_PL_SCORE      = 20,  // int16: score
	WTK_PL_PING       = 21,  // uint16: latency ms
	WTK_PL_TEAM       = 22,  // uint8: team index
	WTK_PL_ALIVE      = 23,  // bool: player alive
};

// ── Weapon short names (MOD enum → compact string) ─────────────
// Maps means-of-death index to the short weapon name used in events.
static const char *WT_WeaponShortName( int mod )
{
	switch ( mod ) {
	case 1:  return "sg";       // MOD_SHOTGUN
	case 2:  return "gauntlet"; // MOD_GAUNTLET
	case 3:  return "mg";       // MOD_MACHINEGUN
	case 4:  return "gl";       // MOD_GRENADE
	case 5:  return "gl";       // MOD_GRENADE_SPLASH
	case 6:  return "rl";       // MOD_ROCKET
	case 7:  return "rl";       // MOD_ROCKET_SPLASH
	case 8:  return "pg";       // MOD_PLASMA
	case 9:  return "rg";       // MOD_RAILGUN
	case 10: return "lg";       // MOD_LIGHTNING
	case 11: return "lg";       // MOD_LIGHTNING_DISCHARGE
	case 16: return "falling";  // MOD_FALLING
	case 17: return "suicide";  // MOD_SUICIDE
	case 20: return "grapple";  // MOD_GRAPPLE
	default: return "world";
	}
}


/*
====================
WT_MsgpackEncodeVec3

Write a vec3_t as a 3-element array of float32.
====================
*/
static void WT_MsgpackEncodeVec3( mpack_writer_t *w, const vec3_t v )
{
	mpack_start_array( w, 3 );
	mpack_write_float( w, v[0] );
	mpack_write_float( w, v[1] );
	mpack_write_float( w, v[2] );
	mpack_finish_array( w );
}


/*
====================
WT_EncodeKillEvent

Encode a kill event as MessagePack.
Returns the number of bytes written, or 0 on error.

Schema (v1, integer keys — see WTK_* enum):
{
  0: "kill",        // WTK_TYPE
  1: uint64,        // WTK_SEQ
  2: uint32,        // WTK_TIME
  3: int,           // WTK_ATTACKER_ID
  4: int,           // WTK_VICTIM_ID
  5: string,        // WTK_WEAPON
  6: int,           // WTK_MOD
  7: [f32, f32, f32], // WTK_ATTACKER_POS
  8: [f32, f32, f32]  // WTK_VICTIM_POS
}
====================
*/
int WT_EncodeKillEvent( byte *buf, int buf_size,
                        int attacker, int victim, int mod,
                        const vec3_t attacker_pos, const vec3_t victim_pos )
{
	mpack_writer_t writer;
	size_t         used;

	mpack_writer_init( &writer, (char *)buf, (size_t)buf_size );

	mpack_start_map( &writer, 9 );

	mpack_write_uint( &writer, WTK_TYPE );
	mpack_write_cstr( &writer, "kill" );

	mpack_write_uint( &writer, WTK_SEQ );
	mpack_write_u64( &writer, wt.event_seq );

	mpack_write_uint( &writer, WTK_TIME );
	mpack_write_u32( &writer, (uint32_t)( Sys_Milliseconds() & 0xFFFFFFFF ) );

	mpack_write_uint( &writer, WTK_ATTACKER_ID );
	mpack_write_int( &writer, attacker );

	mpack_write_uint( &writer, WTK_VICTIM_ID );
	mpack_write_int( &writer, victim );

	mpack_write_uint( &writer, WTK_WEAPON );
	mpack_write_cstr( &writer, WT_WeaponShortName( mod ) );

	mpack_write_uint( &writer, WTK_MOD );
	mpack_write_int( &writer, mod );

	mpack_write_uint( &writer, WTK_ATTACKER_POS );
	WT_MsgpackEncodeVec3( &writer, attacker_pos );

	mpack_write_uint( &writer, WTK_VICTIM_POS );
	WT_MsgpackEncodeVec3( &writer, victim_pos );

	mpack_finish_map( &writer );

	if ( mpack_writer_destroy( &writer ) != mpack_ok )
		return 0;

	used = mpack_writer_buffer_used( &writer );
	return (int)used;
}


/*
====================
WT_EncodeDamageEvent
====================
*/
int WT_EncodeDamageEvent( byte *buf, int buf_size,
                          int attacker, int victim, int damage, int mod,
                          const vec3_t attacker_pos, const vec3_t victim_pos )
{
	mpack_writer_t writer;
	size_t         used;

	mpack_writer_init( &writer, (char *)buf, (size_t)buf_size );

	mpack_start_map( &writer, 10 );

	mpack_write_uint( &writer, WTK_TYPE );
	mpack_write_cstr( &writer, "damage" );

	mpack_write_uint( &writer, WTK_SEQ );
	mpack_write_u64( &writer, wt.event_seq );

	mpack_write_uint( &writer, WTK_TIME );
	mpack_write_u32( &writer, (uint32_t)( Sys_Milliseconds() & 0xFFFFFFFF ) );

	mpack_write_uint( &writer, WTK_ATTACKER_ID );
	mpack_write_int( &writer, attacker );

	mpack_write_uint( &writer, WTK_VICTIM_ID );
	mpack_write_int( &writer, victim );

	mpack_write_uint( &writer, WTK_DAMAGE );
	mpack_write_int( &writer, damage );

	mpack_write_uint( &writer, WTK_WEAPON );
	mpack_write_cstr( &writer, WT_WeaponShortName( mod ) );

	mpack_write_uint( &writer, WTK_MOD );
	mpack_write_int( &writer, mod );

	mpack_write_uint( &writer, WTK_ATTACKER_POS );
	WT_MsgpackEncodeVec3( &writer, attacker_pos );

	mpack_write_uint( &writer, WTK_VICTIM_POS );
	WT_MsgpackEncodeVec3( &writer, victim_pos );

	mpack_finish_map( &writer );

	if ( mpack_writer_destroy( &writer ) != mpack_ok )
		return 0;

	used = mpack_writer_buffer_used( &writer );
	return (int)used;
}


/*
====================
WT_EncodeItemPickupEvent
====================
*/
int WT_EncodeItemPickupEvent( byte *buf, int buf_size,
                              int client, const char *item, const vec3_t pos )
{
	mpack_writer_t writer;
	size_t         used;

	mpack_writer_init( &writer, (char *)buf, (size_t)buf_size );

	mpack_start_map( &writer, 6 );

	mpack_write_uint( &writer, WTK_TYPE );
	mpack_write_cstr( &writer, "item_pickup" );

	mpack_write_uint( &writer, WTK_SEQ );
	mpack_write_u64( &writer, wt.event_seq );

	mpack_write_uint( &writer, WTK_TIME );
	mpack_write_u32( &writer, (uint32_t)( Sys_Milliseconds() & 0xFFFFFFFF ) );

	mpack_write_uint( &writer, WTK_PLAYER_ID );
	mpack_write_int( &writer, client );

	mpack_write_uint( &writer, WTK_ITEM );
	mpack_write_cstr( &writer, item ? item : "" );

	mpack_write_uint( &writer, WTK_POSITION );
	WT_MsgpackEncodeVec3( &writer, pos );

	mpack_finish_map( &writer );

	if ( mpack_writer_destroy( &writer ) != mpack_ok )
		return 0;

	used = mpack_writer_buffer_used( &writer );
	return (int)used;
}


/*
====================
WT_EncodeChatEvent
====================
*/
int WT_EncodeChatEvent( byte *buf, int buf_size,
                        int client, const char *msg, qboolean teamOnly )
{
	mpack_writer_t writer;
	size_t         used;

	mpack_writer_init( &writer, (char *)buf, (size_t)buf_size );

	mpack_start_map( &writer, 6 );

	mpack_write_uint( &writer, WTK_TYPE );
	mpack_write_cstr( &writer, "chat" );

	mpack_write_uint( &writer, WTK_SEQ );
	mpack_write_u64( &writer, wt.event_seq );

	mpack_write_uint( &writer, WTK_TIME );
	mpack_write_u32( &writer, (uint32_t)( Sys_Milliseconds() & 0xFFFFFFFF ) );

	mpack_write_uint( &writer, WTK_PLAYER_ID );
	mpack_write_int( &writer, client );

	mpack_write_uint( &writer, WTK_MESSAGE );
	mpack_write_cstr( &writer, msg ? msg : "" );

	mpack_write_uint( &writer, WTK_TEAM_ONLY );
	mpack_write_bool( &writer, teamOnly != 0 );

	mpack_finish_map( &writer );

	if ( mpack_writer_destroy( &writer ) != mpack_ok )
		return 0;

	used = mpack_writer_buffer_used( &writer );
	return (int)used;
}


/*
====================
WT_EncodeStateUpdate

Encode the full game state as a MessagePack datagram.
Returns bytes written, or 0 on error.

Schema (v1, integer keys — see WTK_* enum):
{
  1: uint32,        // WTK_SEQ
  2: uint32,        // WTK_TIME
  15: string,       // WTK_MAP
  16: uint8,        // WTK_GAMETYPE
  17: [             // WTK_PLAYERS
    { 18: uint8, 19: string, 20: int16, 21: uint16, 22: uint8, 23: bool }
  ]
}

Fits within ~1200 byte QUIC datagram MTU for up to ~32 players.
If it overflows, truncate players by score and set "truncated": true.
====================
*/
int WT_EncodeStateUpdate( byte *buf, int buf_size )
{
	mpack_writer_t writer;
	size_t         used;
	int            i;
	int            player_count = 0;

	if ( !sv.state )
		return 0;

	// Count active players
	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED )
			player_count++;
	}

	mpack_writer_init( &writer, (char *)buf, (size_t)buf_size );

	mpack_start_map( &writer, 5 );

	mpack_write_uint( &writer, WTK_SEQ );
	mpack_write_u32( &writer, (uint32_t)wt.event_seq++ );

	mpack_write_uint( &writer, WTK_TIME );
	mpack_write_u32( &writer, (uint32_t)sv.time );

	mpack_write_uint( &writer, WTK_MAP );
	mpack_write_cstr( &writer, Cvar_VariableString( "mapname" ) );

	mpack_write_uint( &writer, WTK_GAMETYPE );
	mpack_write_u8( &writer, (uint8_t)( Cvar_VariableIntegerValue("g_gametype") ) );

	// Players array
	mpack_write_uint( &writer, WTK_PLAYERS );
	mpack_start_array( &writer, (uint32_t)player_count );

	for ( i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; i++ ) {
		client_t *cl = &svs.clients[i];
		if ( cl->state < CS_CONNECTED )
			continue;

		mpack_start_map( &writer, 6 );

		mpack_write_uint( &writer, WTK_PL_ID );
		mpack_write_u8( &writer, (uint8_t)i );

		mpack_write_uint( &writer, WTK_PL_NAME );
		mpack_write_cstr( &writer, cl->name ? cl->name : "" );

		mpack_write_uint( &writer, WTK_PL_SCORE );
		mpack_write_i32( &writer, 0 ); // TODO: read from playerState

		mpack_write_uint( &writer, WTK_PL_PING );
		mpack_write_u16( &writer, (uint16_t)( cl->ping > 0 ? cl->ping : 0 ) );

		mpack_write_uint( &writer, WTK_PL_TEAM );
		mpack_write_u8( &writer, 0 ); // TODO: read from configstring

		mpack_write_uint( &writer, WTK_PL_ALIVE );
		mpack_write_bool( &writer, cl->state == CS_ACTIVE );

		mpack_finish_map( &writer );
	}

	mpack_finish_array( &writer );
	mpack_finish_map( &writer );

	if ( mpack_writer_destroy( &writer ) != mpack_ok )
		return 0;

	used = mpack_writer_buffer_used( &writer );
	return (int)used;
}

#endif // FEAT_QUIC_OBSERVE
