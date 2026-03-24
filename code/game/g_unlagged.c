/*
===========================================================================
Unlagged lag compensation for q3now.
Based on Unlagged 2.01 by Neil "haste" Toronto.
Ported from Smokin' Guns / OpenArena references.

When FEAT_UNLAGGED=0 the basic path compiles: G_ResetHistory,
G_StoreHistory, G_TimeShiftClient (static), G_DoTimeShiftFor,
G_UndoTimeShiftFor — simple backward reconciliation using commandTime.

When FEAT_UNLAGGED=1 the full feature set is enabled:
  - debug output in G_TimeShiftClient
  - per-weapon delag bitmask in G_DoTimeShiftFor
  - angle interpolation via TimeShiftAnglesLerp
  - angle + animation state in G_StoreHistory / G_ResetHistory
  - G_UnTimeShiftClient exported (non-static) for g_combat.c
  - server-side prediction: G_PredictPlayerMove and helpers
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

	ent->client->historyHead = NUM_CLIENT_HISTORY - 1;
	for ( i = ent->client->historyHead, time = level.time; i >= 0; i--, time -= 50 ) {
		VectorCopy( ent->r.mins, ent->client->history[i].mins );
		VectorCopy( ent->r.maxs, ent->client->history[i].maxs );
		VectorCopy( ent->r.currentOrigin, ent->client->history[i].origin );
#if FEAT_UNLAGGED
		VectorCopy( ent->r.currentAngles, ent->client->history[i].currentAngles );
		VectorCopy( ent->client->ps.viewangles, ent->client->history[i].viewangles );
		VectorClear( ent->client->history[i].legs_angles );
		VectorClear( ent->client->history[i].torso_angles );
		memset( &ent->client->history[i].legs, 0, sizeof( ent->client->history[i].legs ) );
		memset( &ent->client->history[i].torso, 0, sizeof( ent->client->history[i].torso ) );
#endif
		ent->client->history[i].time = time;
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

	ent->client->historyHead++;
	if ( ent->client->historyHead >= NUM_CLIENT_HISTORY ) {
		ent->client->historyHead = 0;
	}

	head = ent->client->historyHead;

	VectorCopy( ent->r.mins, ent->client->history[head].mins );
	VectorCopy( ent->r.maxs, ent->client->history[head].maxs );
	VectorCopy( ent->s.pos.trBase, ent->client->history[head].origin );
	SnapVector( ent->client->history[head].origin );
#if FEAT_UNLAGGED
	VectorCopy( ent->r.currentAngles, ent->client->history[head].currentAngles );
	VectorCopy( ent->client->ps.viewangles, ent->client->history[head].viewangles );
	VectorClear( ent->client->history[head].legs_angles );
	VectorClear( ent->client->history[head].torso_angles );
	memset( &ent->client->history[head].legs, 0, sizeof( ent->client->history[head].legs ) );
	memset( &ent->client->history[head].torso, 0, sizeof( ent->client->history[head].torso ) );
#endif
	ent->client->history[head].time = level.time;
}


/*
=============
TimeShiftLerp

Interpolate between two historical positions.
Returns a vector "frac" times the distance between "start" and "end".
=============
*/
static void TimeShiftLerp( float frac, vec3_t start, vec3_t end, vec3_t result ) {
	result[0] = start[0] + frac * ( end[0] - start[0] );
	result[1] = start[1] + frac * ( end[1] - start[1] );
	result[2] = start[2] + frac * ( end[2] - start[2] );
}


#if FEAT_UNLAGGED
/*
==================
TimeShiftAnglesLerp

Interpolate between two historical angles using direction vectors
to avoid gimbal artifacts near angle wrapping boundaries.
==================
*/
static void TimeShiftAnglesLerp( float frac, vec3_t start, vec3_t end, vec3_t result ) {
	vec3_t	dir1, dir2, temp;
	float	comp = 1.0f - frac;

	AngleVectors( start, dir1, NULL, NULL );
	AngleVectors( end, dir2, NULL, NULL );

	temp[0] = frac * dir1[0] + comp * dir2[0];
	temp[1] = frac * dir1[1] + comp * dir2[1];
	temp[2] = frac * dir1[2] + comp * dir2[2];

	vectoangles( temp, result );
}
#endif


/*
=================
G_TimeShiftClient

Move a client back to where they were at the specified "time".

When FEAT_UNLAGGED=0 this is a simple origin+bbox rewind.
When FEAT_UNLAGGED=1 it additionally interpolates angles, stores
animation state, and can print debug output.
=================
*/
#if FEAT_UNLAGGED
void G_TimeShiftClient( gentity_t *ent, int time, qboolean debug, gentity_t *debugger ) {
#else
static void G_TimeShiftClient( gentity_t *ent, int time ) {
#endif
	int		j, k;
#if FEAT_UNLAGGED
	char	msg[2048];
#endif

	// find two history entries that sandwich the target time
	j = k = ent->client->historyHead;
	do {
		if ( ent->client->history[j].time <= time )
			break;

		k = j;
		j--;
		if ( j < 0 ) {
			j = NUM_CLIENT_HISTORY - 1;
		}
	} while ( j != ent->client->historyHead );

	// if we got past the first iteration, we've sandwiched (or wrapped)
	if ( j != k ) {
		// save current position once per frame
		if ( ent->client->savedHistory.time != level.time ) {
			VectorCopy( ent->r.mins, ent->client->savedHistory.mins );
			VectorCopy( ent->r.maxs, ent->client->savedHistory.maxs );
			VectorCopy( ent->r.currentOrigin, ent->client->savedHistory.origin );
#if FEAT_UNLAGGED
			VectorCopy( ent->r.currentAngles, ent->client->savedHistory.currentAngles );
			VectorCopy( ent->client->ps.viewangles, ent->client->savedHistory.viewangles );
#endif
			ent->client->savedHistory.time = level.time;
		}

		if ( j != ent->client->historyHead ) {
			// sandwiched: interpolate
			float frac = (float)( time - ent->client->history[j].time ) /
				(float)( ent->client->history[k].time - ent->client->history[j].time );

			// interpolate origin
			TimeShiftLerp( frac,
				ent->client->history[j].origin, ent->client->history[k].origin,
				ent->r.currentOrigin );

			// interpolate bounding box (for ducking)
			TimeShiftLerp( frac,
				ent->client->history[j].mins, ent->client->history[k].mins,
				ent->r.mins );

			TimeShiftLerp( frac,
				ent->client->history[j].maxs, ent->client->history[k].maxs,
				ent->r.maxs );

#if FEAT_UNLAGGED
			// interpolate angles
			TimeShiftAnglesLerp( frac,
				ent->client->history[k].currentAngles,
				ent->client->history[j].currentAngles,
				ent->r.currentAngles );

			TimeShiftAnglesLerp( frac,
				ent->client->history[k].viewangles,
				ent->client->history[j].viewangles,
				ent->client->ps.viewangles );

			if ( debug && debugger != NULL ) {
				Com_sprintf( msg, sizeof( msg ),
					"print \"^1Rec: time: %d, j: %d, k: %d, origin: %0.2f %0.2f %0.2f\n"
					"^2frac: %0.4f, origin1: %0.2f %0.2f %0.2f, origin2: %0.2f %0.2f %0.2f\n"
					"^7level.time: %d, est time: %d, level.time delta: %d, est real ping: %d\n\"",
					time, ent->client->history[j].time, ent->client->history[k].time,
					ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
					frac,
					ent->client->history[j].origin[0],
					ent->client->history[j].origin[1],
					ent->client->history[j].origin[2],
					ent->client->history[k].origin[0],
					ent->client->history[k].origin[1],
					ent->client->history[k].origin[2],
					level.time, level.time + debugger->client->frameOffset,
					level.time - time,
					level.time + debugger->client->frameOffset - time );

				trap_SendServerCommand( debugger - g_entities, msg );
			}
#endif

			// recalculate absmin and absmax
			trap_LinkEntity( ent );
		} else {
			// wrapped: grab the earliest entry
			VectorCopy( ent->client->history[k].origin, ent->r.currentOrigin );
			VectorCopy( ent->client->history[k].mins, ent->r.mins );
			VectorCopy( ent->client->history[k].maxs, ent->r.maxs );

#if FEAT_UNLAGGED
			VectorCopy( ent->client->history[k].currentAngles, ent->r.currentAngles );
			VectorCopy( ent->client->history[k].viewangles, ent->client->ps.viewangles );
#endif

			// recalculate absmin and absmax
			trap_LinkEntity( ent );
		}
	}
#if FEAT_UNLAGGED
	else {
		// no reconciliation happened (client using negative timenudge)
		if ( debug && debugger != NULL ) {
			Com_sprintf( msg, sizeof( msg ),
				"print \"^1No rec: time: %d, j: %d, k: %d, origin: %0.2f %0.2f %0.2f\n"
				"^2frac: %0.4f, origin1: %0.2f %0.2f %0.2f, origin2: %0.2f %0.2f %0.2f\n"
				"^7level.time: %d, est time: %d, level.time delta: %d, est real ping: %d\n\"",
				time, level.time, level.time,
				ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
				0.0f,
				ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
				ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
				level.time, level.time + debugger->client->frameOffset,
				level.time - time,
				level.time + debugger->client->frameOffset - time );

			trap_SendServerCommand( debugger - g_entities, msg );
		}
	}
#endif
}


/*
=====================
G_TimeShiftAllClients

Rewind all clients except "skip" to the specified time.
=====================
*/
static void G_TimeShiftAllClients( int time, gentity_t *skip ) {
	int			i;
	gentity_t	*ent;
#if FEAT_UNLAGGED
	qboolean	debug = ( skip != NULL && skip->client &&
				skip->client->pers.debugDelag );
#endif

	ent = &g_entities[0];
	for ( i = 0; i < MAX_CLIENTS; i++, ent++ ) {
		if ( ent->client && ent->inuse &&
		     ent->client->sess.sessionTeam < TEAM_SPECTATOR &&
		     ent != skip ) {
#if FEAT_UNLAGGED
			G_TimeShiftClient( ent, time, debug, skip );
#else
			G_TimeShiftClient( ent, time );
#endif
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
#if FEAT_UNLAGGED
	int wpflags[WP_NUM_WEAPONS] = { 0, 0, 2, 4, 0, 0, 8, 16, 0 };
	int wpflag;
#endif
	int time;

	// don't time shift for mistakes or bots
	if ( !ent->inuse || !ent->client || ( ent->r.svFlags & SVF_BOT ) ) {
		return;
	}

#if FEAT_UNLAGGED
	if ( !g_unlagged.integer ) {
		return;
	}

	wpflag = wpflags[ent->client->ps.weapon];

	// if delag is enabled server-side and the client wants it (globally or per-weapon)
	if ( g_delagHitscan.integer &&
	     ( ent->client->pers.delag & 1 || ent->client->pers.delag & wpflag ) ) {
		// full lag compensation, offset by client's time nudge
		time = ent->client->attackTime + ent->client->pers.cmdTimeNudge;
	} else {
		// basic 50ms rewind using frame offset
		time = level.previousTime + ent->client->frameOffset;
	}
#else
	// basic path: rewind to the shooter's command time
	time = ent->client->ps.commandTime;
#endif

	G_TimeShiftAllClients( time, ent );
}


/*
===================
G_UnTimeShiftClient

Restore a client to their real current position.
When FEAT_UNLAGGED=1 this is non-static (exported for g_combat.c).
===================
*/
#if FEAT_UNLAGGED
void G_UnTimeShiftClient( gentity_t *ent ) {
#else
static void G_UnTimeShiftClient( gentity_t *ent ) {
#endif
	if ( ent->client->savedHistory.time == level.time ) {
		VectorCopy( ent->client->savedHistory.mins, ent->r.mins );
		VectorCopy( ent->client->savedHistory.maxs, ent->r.maxs );
		VectorCopy( ent->client->savedHistory.origin, ent->r.currentOrigin );
#if FEAT_UNLAGGED
		VectorCopy( ent->client->savedHistory.currentAngles, ent->r.currentAngles );
		VectorCopy( ent->client->savedHistory.viewangles, ent->client->ps.viewangles );
#endif
		ent->client->savedHistory.time = 0;

		// recalculate absmin and absmax
		trap_LinkEntity( ent );
	}
}


/*
=======================
G_UnTimeShiftAllClients

Restore all clients except "skip" to their real positions.
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
==================
G_UndoTimeShiftFor

After the hitscan fire: restore all other clients to real positions.
==================
*/
void G_UndoTimeShiftFor( gentity_t *ent ) {
	// don't un-time shift for mistakes or bots
	if ( !ent->inuse || !ent->client || ( ent->r.svFlags & SVF_BOT ) ) {
		return;
	}

	G_UnTimeShiftAllClients( ent );
}


/* ===================================================================
   Server-side prediction (FEAT_UNLAGGED only)
   Used to extrapolate player positions forward when the server
   needs to predict where a player will be.
   =================================================================== */

#if FEAT_UNLAGGED

/*
===========================
G_PredictPlayerClipVelocity

Slide on the impacting surface.
===========================
*/

#define OVERCLIP	1.001f

void G_PredictPlayerClipVelocity( vec3_t in, vec3_t normal, vec3_t out ) {
	float	backoff;

	// find the magnitude of the vector "in" along "normal"
	backoff = DotProduct( in, normal );

	// tilt the plane a bit to avoid floating-point error issues
	if ( backoff < 0 ) {
		backoff *= OVERCLIP;
	} else {
		backoff /= OVERCLIP;
	}

	// slide along
	VectorMA( in, -backoff, normal, out );
}


/*
========================
G_PredictPlayerSlideMove

Advance the given entity frametime seconds, sliding as appropriate.
========================
*/
#define MAX_CLIP_PLANES 5

qboolean G_PredictPlayerSlideMove( gentity_t *ent, float frametime ) {
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, velocity, origin;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;

	numbumps = 4;

	VectorCopy( ent->s.pos.trDelta, primal_velocity );
	VectorCopy( primal_velocity, velocity );
	VectorCopy( ent->s.pos.trBase, origin );

	VectorCopy( velocity, endVelocity );

	time_left = frametime;
	numplanes = 0;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {

		// calculate position we are trying to move to
		VectorMA( origin, time_left, velocity, end );

		// see if we can make it there
		trap_Trace( &trace, origin, ent->r.mins, ent->r.maxs, end,
			ent->s.number, ent->clipmask );

		if ( trace.allsolid ) {
			// entity is completely trapped in another solid
			VectorClear( velocity );
			VectorCopy( origin, ent->s.pos.trBase );
			return qtrue;
		}

		if ( trace.fraction > 0 ) {
			// actually covered some distance
			VectorCopy( trace.endpos, origin );
		}

		if ( trace.fraction == 1 ) {
			break;		// moved the entire distance
		}

		time_left -= time_left * trace.fraction;

		if ( numplanes >= MAX_CLIP_PLANES ) {
			// this shouldn't really happen
			VectorClear( velocity );
			VectorCopy( origin, ent->s.pos.trBase );
			return qtrue;
		}

		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		for ( i = 0; i < numplanes; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99 ) {
				VectorAdd( trace.plane.normal, velocity, velocity );
				break;
			}
		}

		if ( i < numplanes ) {
			continue;
		}

		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

		// modify velocity so it parallels all of the clip planes

		// find a plane that it enters
		for ( i = 0; i < numplanes; i++ ) {
			into = DotProduct( velocity, planes[i] );
			if ( into >= 0.1 ) {
				continue;		// move doesn't interact with the plane
			}

			// slide along the plane
			G_PredictPlayerClipVelocity( velocity, planes[i], clipVelocity );

			// slide along the plane
			G_PredictPlayerClipVelocity( endVelocity, planes[i], endClipVelocity );

			// see if there is a second plane that the new move enters
			for ( j = 0; j < numplanes; j++ ) {
				if ( j == i ) {
					continue;
				}

				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1 ) {
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				G_PredictPlayerClipVelocity( clipVelocity, planes[j], clipVelocity );
				G_PredictPlayerClipVelocity( endClipVelocity, planes[j], endClipVelocity );

				// see if it goes back into the first clip plane
				if ( DotProduct( clipVelocity, planes[i] ) >= 0 ) {
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct( planes[i], planes[j], dir );
				VectorNormalize( dir );
				d = DotProduct( dir, velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct( planes[i], planes[j], dir );
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the new move enters
				for ( k = 0; k < numplanes; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}

					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1 ) {
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a triple plane interaction
					VectorClear( velocity );
					VectorCopy( origin, ent->s.pos.trBase );
					return qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	VectorCopy( endVelocity, velocity );
	VectorCopy( origin, ent->s.pos.trBase );

	return ( bumpcount != 0 );
}


/*
============================
G_PredictPlayerStepSlideMove

Advance the given entity frametime seconds, stepping and sliding
as appropriate.
============================
*/
#define STEPSIZE 18

void G_PredictPlayerStepSlideMove( gentity_t *ent, float frametime ) {
	vec3_t		start_o, start_v;
	vec3_t		down, up;
	trace_t		trace;
	float		stepSize;

	VectorCopy( ent->s.pos.trBase, start_o );
	VectorCopy( ent->s.pos.trDelta, start_v );

	if ( !G_PredictPlayerSlideMove( ent, frametime ) ) {
		// not clipped, so forget stepping
		return;
	}

	VectorCopy( start_o, up );
	up[2] += STEPSIZE;

	// test the player position if they were a stepheight higher
	trap_Trace( &trace, start_o, ent->r.mins, ent->r.maxs, up,
		ent->s.number, ent->clipmask );
	if ( trace.allsolid ) {
		return;		// can't step up
	}

	stepSize = trace.endpos[2] - start_o[2];

	// try slidemove from this position
	VectorCopy( trace.endpos, ent->s.pos.trBase );
	VectorCopy( start_v, ent->s.pos.trDelta );

	G_PredictPlayerSlideMove( ent, frametime );

	// push down the final amount
	VectorCopy( ent->s.pos.trBase, down );
	down[2] -= stepSize;
	trap_Trace( &trace, ent->s.pos.trBase, ent->r.mins, ent->r.maxs, down,
		ent->s.number, ent->clipmask );
	if ( !trace.allsolid ) {
		VectorCopy( trace.endpos, ent->s.pos.trBase );
	}
	if ( trace.fraction < 1.0 ) {
		G_PredictPlayerClipVelocity( ent->s.pos.trDelta, trace.plane.normal,
			ent->s.pos.trDelta );
	}
}


/*
===================
G_PredictPlayerMove

Advance the given entity frametime seconds, stepping and sliding
as appropriate.  This is the entry point to the server-side-only
prediction code.
===================
*/
void G_PredictPlayerMove( gentity_t *ent, float frametime ) {
	G_PredictPlayerStepSlideMove( ent, frametime );
}

#endif /* FEAT_UNLAGGED */
