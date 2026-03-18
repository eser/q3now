/*
===========================================================================
Unlagged lag compensation for q3now.
Ported from Spearmint/mint-arena (Neil Toronto, 2006).
Adapted for Quake3e: ent->player-> renamed to ent->client->,
prediction functions stripped (not needed).
===========================================================================
*/
#include "g_local.h"

/*
============
G_ResetHistory

Clear a client's position history (call on spawn/teleport).
============
*/
void G_ResetHistory( gentity_t *ent ) {
	int		i, time;

	ent->client->topMarker = MAX_PLAYER_MARKERS - 1;
	for ( i = ent->client->topMarker, time = level.time; i >= 0; i--, time -= 50 ) {
		VectorCopy( ent->r.mins,          ent->client->playerMarkers[i].mins );
		VectorCopy( ent->r.maxs,          ent->client->playerMarkers[i].maxs );
		VectorCopy( ent->r.currentOrigin, ent->client->playerMarkers[i].origin );
		ent->client->playerMarkers[i].time = time;
	}
}


/*
============
G_StoreHistory

Record the client's current position (call after Pmove each frame).
============
*/
void G_StoreHistory( gentity_t *ent ) {
	int		head;

	ent->client->topMarker++;
	if ( ent->client->topMarker >= MAX_PLAYER_MARKERS ) {
		ent->client->topMarker = 0;
	}
	head = ent->client->topMarker;

	VectorCopy( ent->r.mins,          ent->client->playerMarkers[head].mins );
	VectorCopy( ent->r.maxs,          ent->client->playerMarkers[head].maxs );
	VectorCopy( ent->s.pos.trBase,    ent->client->playerMarkers[head].origin );
	SnapVector( ent->client->playerMarkers[head].origin );
	ent->client->playerMarkers[head].time = level.time;
}


/*
=============
TimeShiftLerp

Interpolate between two historical positions.
=============
*/
static void TimeShiftLerp( float frac, vec3_t start, vec3_t end, vec3_t result ) {
	result[0] = start[0] + frac * ( end[0] - start[0] );
	result[1] = start[1] + frac * ( end[1] - start[1] );
	result[2] = start[2] + frac * ( end[2] - start[2] );
}


/*
=================
G_TimeShiftClient

Move a client back to where they were at the specified time.
=================
*/
static void G_TimeShiftClient( gentity_t *ent, int time ) {
	int		j, k;

	// find two history entries that sandwich the target time
	j = k = ent->client->topMarker;
	do {
		if ( ent->client->playerMarkers[j].time <= time )
			break;

		k = j;
		j--;
		if ( j < 0 ) {
			j = MAX_PLAYER_MARKERS - 1;
		}
	} while ( j != ent->client->topMarker );

	if ( j != k ) {
		// save current position (once per frame)
		if ( ent->client->backupMarker.time != level.time ) {
			VectorCopy( ent->r.mins,          ent->client->backupMarker.mins );
			VectorCopy( ent->r.maxs,          ent->client->backupMarker.maxs );
			VectorCopy( ent->r.currentOrigin, ent->client->backupMarker.origin );
			ent->client->backupMarker.time = level.time;
		}

		if ( j != ent->client->topMarker ) {
			// sandwiched: interpolate
			float frac = (float)( time - ent->client->playerMarkers[j].time ) /
				(float)( ent->client->playerMarkers[k].time - ent->client->playerMarkers[j].time );

			TimeShiftLerp( frac,
				ent->client->playerMarkers[j].origin, ent->client->playerMarkers[k].origin,
				ent->r.currentOrigin );
			TimeShiftLerp( frac,
				ent->client->playerMarkers[j].mins, ent->client->playerMarkers[k].mins,
				ent->r.mins );
			TimeShiftLerp( frac,
				ent->client->playerMarkers[j].maxs, ent->client->playerMarkers[k].maxs,
				ent->r.maxs );
		} else {
			// wrapped: use oldest entry
			VectorCopy( ent->client->playerMarkers[k].origin, ent->r.currentOrigin );
			VectorCopy( ent->client->playerMarkers[k].mins,   ent->r.mins );
			VectorCopy( ent->client->playerMarkers[k].maxs,   ent->r.maxs );
		}
		trap_LinkEntity( ent );
	}
}


/*
=====================
G_TimeShiftAllClients

Rewind all clients except `skip` to the specified time.
=====================
*/
static void G_TimeShiftAllClients( int time, gentity_t *skip ) {
	int			i;
	gentity_t	*ent;

	ent = &g_entities[0];
	for ( i = 0; i < MAX_CLIENTS; i++, ent++ ) {
		if ( ent->client && ent->inuse &&
		     ent->client->sess.sessionTeam < TEAM_SPECTATOR &&
		     ent != skip ) {
			G_TimeShiftClient( ent, time );
		}
	}
}


/*
===================
G_UnTimeShiftClient

Restore a client to their real current position.
===================
*/
static void G_UnTimeShiftClient( gentity_t *ent ) {
	if ( ent->client->backupMarker.time == level.time ) {
		VectorCopy( ent->client->backupMarker.mins,   ent->r.mins );
		VectorCopy( ent->client->backupMarker.maxs,   ent->r.maxs );
		VectorCopy( ent->client->backupMarker.origin, ent->r.currentOrigin );
		ent->client->backupMarker.time = 0;
		trap_LinkEntity( ent );
	}
}


/*
=======================
G_UnTimeShiftAllClients

Restore all clients except `skip` to their real positions.
=======================
*/
static void G_UnTimeShiftAllClients( gentity_t *skip ) {
	int			i;
	gentity_t	*ent;

	ent = &g_entities[0];
	for ( i = 0; i < MAX_CLIENTS; i++, ent++ ) {
		if ( ent->client && ent->inuse &&
		     ent->client->sess.sessionTeam < TEAM_SPECTATOR &&
		     ent != skip ) {
			G_UnTimeShiftClient( ent );
		}
	}
}


/*
================
G_DoTimeShiftFor

Before a hitscan weapon fires: rewind all other clients to the
shooter's command time so traces hit where the shooter saw them.
================
*/
void G_DoTimeShiftFor( gentity_t *ent ) {
	if ( !ent->inuse || !ent->client || ( ent->r.svFlags & SVF_BOT ) ) {
		return;
	}
	if ( !g_unlagged.integer ) {
		return;
	}
	G_TimeShiftAllClients( ent->client->ps.commandTime, ent );
}


/*
==================
G_UndoTimeShiftFor

After the hitscan fire: restore all other clients to real positions.
==================
*/
void G_UndoTimeShiftFor( gentity_t *ent ) {
	if ( !ent->inuse || !ent->client || ( ent->r.svFlags & SVF_BOT ) ) {
		return;
	}
	G_UnTimeShiftAllClients( ent );
}
