// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
wired_scene_eval.c -- cinematic-scene evaluator (sample -> view) (plain-C)

See wired_scene_eval.h for the contract. Given a loaded wiredScene_t and a
clock, produce {origin, angles, fov}. Pure CPU: WiredCurve_GetValue /
GetFirstDerivative sample the splines; vectoangles turns the facing vector into
view angles. No renderer / RAL / VM.

Clock model (WAIT time-warp): event timeMs are wall-clock offsets from playback
start. The eye path is parameterized in SECONDS 0..totalTimeSec (Start ran
WiredCurve_SetConstantSpeed). A WAIT event at wall time timeMs holds the sampled
PATH clock for its fparam seconds: while inside the hold window the path time
freezes, and every later frame's path time is shifted back by the accumulated
wait. So pathSec(wall) = wallElapsedSec - (waits fully passed) - (partial hold).

Facing: no active target -> normalize(eye tangent); an active TARGET ->
normalize(targetPos - eyePos) look-at. Either can degenerate to a zero vector
(single/coincident knots, or target coincident with the eye) — guarded: reuse
the last good forward instead of feeding a zero vector to vectoangles.

fov: the authored fov{start->end over fovLenSec} baseline lerp holds until a
WSCENE_EV_FOV fires; from then the event lerp (from the fov at fire time, to the
event target, over the event length) wins.

FADEOUT / FADEIN / SCENE / STOP are surfaced as sticky signal bits + payloads;
this module never fades the screen or loads a chained scene.
===========================================================================
*/

#include "wired_scene_eval.h"

#include <string.h>
#include <math.h>

#ifndef WIRED_SCENE_STANDALONE
#include "../../q_shared.h"   /* vec macros, vectoangles, Q_stricmp */
#else
/* Standalone unit-test build: no q_shared.h platform chain. Provide the exact
   vec macros + the two math helpers the evaluator uses (identical semantics to
   q_shared.h / q_math.c). Mirrors wired_curve.c / wired_scene.c gating. */
#include <stdlib.h>

#ifndef PITCH
#define PITCH 0
#define YAW   1
#define ROLL  2
#endif

typedef float vec_t;
typedef vec_t vec3_t[3];

#define DotProduct(x,y)        ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)  ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorCopy(a,b)        ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorScale(v,s,o)     ((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define VectorClear(a)         ((a)[0]=(a)[1]=(a)[2]=0)

/* Case-insensitive string compare (the subset q_shared's Q_stricmp provides). */
static int WiredSceneEval_stricmp( const char *a, const char *b ) {
	int c1, c2;
	do {
		c1 = (unsigned char)*a++; c2 = (unsigned char)*b++;
		if ( c1 >= 'A' && c1 <= 'Z' ) c1 += 'a' - 'A';
		if ( c2 >= 'A' && c2 <= 'Z' ) c2 += 'a' - 'A';
		if ( c1 != c2 ) return c1 - c2;
	} while ( c1 );
	return 0;
}
#define Q_stricmp WiredSceneEval_stricmp

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* vectoangles: identical to q_math.c:386 (PITCH = -pitch, YAW = atan2, ROLL = 0). */
static void vectoangles( const vec3_t value1, vec3_t angles ) {
	float yaw, pitch, forward;
	if ( value1[1] == 0 && value1[0] == 0 ) {
		yaw = 0;
		pitch = ( value1[2] > 0 ) ? 90.0f : 270.0f;
	} else {
		if ( value1[0] ) {
			yaw = (float)( atan2( value1[1], value1[0] ) * 180 / M_PI );
		} else if ( value1[1] > 0 ) {
			yaw = 90.0f;
		} else {
			yaw = 270.0f;
		}
		if ( yaw < 0 ) yaw += 360.0f;

		forward = (float)sqrt( value1[0]*value1[0] + value1[1]*value1[1] );
		pitch = (float)( atan2( value1[2], forward ) * 180 / M_PI );
		if ( pitch < 0 ) pitch += 360.0f;
	}
	angles[PITCH] = -pitch;
	angles[YAW]   = yaw;
	angles[ROLL]  = 0;
}
#endif  /* WIRED_SCENE_STANDALONE */

/* Normalize `v` in place. Returns a length that is 0 for ANY degenerate input
   — zero-length OR non-finite (NaN/Inf, e.g. a tangent divided by a zero time
   span) — so a single `len <= eps` test at the call site catches them all and
   `v` is never left holding NaN/Inf. Well-formed vectors normalize as usual. */
static float WiredSceneEval_Normalize( float v[3] ) {
	float lenSq = DotProduct( v, v );
	float len;
	if ( !isfinite( lenSq ) || lenSq <= 1e-12f ) {
		return 0.0f;   /* degenerate: leave v untouched, signal via 0 length */
	}
	len = (float)sqrt( lenSq );
	{
		float inv = 1.0f / len;
		v[0] *= inv; v[1] *= inv; v[2] *= inv;
	}
	return len;
}

/* ── event time-ordering ──────────────────────────────────────────────────
   events[] are in FILE order; build evOrder[] = indices sorted by ascending
   timeMs WITHOUT mutating def->events[]. Stable insertion sort (N <= 64, and a
   stable order keeps same-timeMs events in file order — the authored intent). */
static void WiredSceneEval_OrderEvents( wiredScenePlayback_t *pb ) {
	const wiredScene_t *def = pb->def;
	int i, j;

	pb->numEvents = def->numEvents;
	if ( pb->numEvents > WIRED_MAX_SCENE_EVENTS ) {
		pb->numEvents = WIRED_MAX_SCENE_EVENTS;
	}
	for ( i = 0; i < pb->numEvents; i++ ) {
		pb->evOrder[i] = i;
	}
	for ( i = 1; i < pb->numEvents; i++ ) {
		int key = pb->evOrder[i];
		int keyT = def->events[key].timeMs;
		j = i - 1;
		while ( j >= 0 && def->events[ pb->evOrder[j] ].timeMs > keyT ) {
			pb->evOrder[j+1] = pb->evOrder[j];
			j--;
		}
		pb->evOrder[j+1] = key;
	}
}

/* Look up a target by name (Q_stricmp), returning its index or -1. */
static int WiredSceneEval_FindTarget( const wiredScene_t *def, const char *name ) {
	int i;
	for ( i = 0; i < def->numTargets; i++ ) {
		if ( !Q_stricmp( def->targets[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}

/* True length of a spline path in world units — the sum of straight-line
   segment lengths between successive knots. Zero if all knots coincide, which
   is one of the SetConstantSpeed divide-by-zero cases (scale = totalTime/0). */
static float WiredSceneEval_PathSpan( const wiredCurve_t *c ) {
	float total = 0.0f, d[3];
	int   i;
	for ( i = 0; i < c->numKnots - 1; i++ ) {
		VectorSubtract( c->values[i+1], c->values[i], d );
		total += (float)sqrt( DotProduct( d, d ) );
	}
	return total;
}

/* Whether every adjacent knot time strictly increases. SetConstantSpeed (and
   GetFirstDerivative under it) divides by per-segment time spans, so a repeated
   time (span 0) produces NaN in the rewritten times[] — even when the positions
   differ, so PathSpan alone does not catch it. Guard the reparam on this too. */
static int WiredSceneEval_TimesStrictlyIncrease( const wiredCurve_t *c ) {
	int i;
	for ( i = 0; i < c->numKnots - 1; i++ ) {
		if ( c->times[i+1] <= c->times[i] ) {
			return 0;
		}
	}
	return 1;
}

/* Force times[] to be STRICTLY increasing by nudging any knot that is <= its
   predecessor just above it. The spline basis divides by per-segment time spans
   (CR_Basis/TCB_Basis: s = (t-T[i])/(T[i+1]-T[i])), so a zero span yields a 0/0
   NaN in GetValue itself (not just the derivative). A tiny epsilon bump removes
   every zero span while leaving the sampled path visually unchanged. Runs after
   any reparam so no downstream sample can NaN. */
static void WiredSceneEval_EnforceStrictTimes( wiredCurve_t *c ) {
	int   i;
	float eps = 1e-4f;
	for ( i = 1; i < c->numKnots; i++ ) {
		if ( c->times[i] <= c->times[i-1] ) {
			c->times[i] = c->times[i-1] + eps;
		}
	}
}

/* Linearly rescale a curve's authored times[] from [first, last] onto
   [0, totalTimeSec] so it is sampled on the same seconds clock as the (dolly-
   reparameterized) eye path. Unlike the eye path this is NOT an arc-length
   dolly: a look-at target just needs to reach its keyframes at the same
   fraction of the cutscene, so a plain time-domain rescale lines it up with the
   playback clock. Leaves times[] strictly increasing (no zero spans). No-op for
   <2 knots. */
static void WiredSceneEval_RescaleTimesToSeconds( wiredCurve_t *c, float totalTimeSec ) {
	float first, span;
	int   i;
	if ( c->numKnots < 2 || totalTimeSec <= 0.0f ) {
		return;
	}
	first = c->times[0];
	span  = c->times[ c->numKnots - 1 ] - first;
	if ( span > 0.0f ) {
		for ( i = 0; i < c->numKnots; i++ ) {
			c->times[i] = ( ( c->times[i] - first ) / span ) * totalTimeSec;
		}
	}
	/* degenerate authored timeline (span 0) leaves times[] equal — the strict
	   pass below turns that into a finite ramp. */
	WiredSceneEval_EnforceStrictTimes( c );
}

void WiredScenePlayback_Start( wiredScenePlayback_t *pb, wiredScene_t *def, int startMs ) {
	int i;

	if ( !pb || !def ) {
		return;
	}
	memset( pb, 0, sizeof( *pb ) );
	pb->def     = def;
	pb->startMs = startMs;
	pb->active  = 1;

	/* Reparameterize the eye path so it is sampled directly in seconds by the
	   playback clock (destructive: rewrites def->eyePath.times[]). SetConstant-
	   Speed divides by both the total arc length AND per-segment time spans, so
	   it is only safe when the path has a nonzero span AND strictly increasing
	   knot times; otherwise it would write NaN/Inf into times[] (poisoning the
	   whole cutscene). In any degenerate case fall back to a plain, always-finite
	   seconds rescale. A single knot is already fully determined — no timing. */
	if ( def->eyePath.numKnots >= 2 && def->totalTimeSec > 0.0f ) {
		if ( WiredSceneEval_PathSpan( &def->eyePath ) > 1e-6f
		  && WiredSceneEval_TimesStrictlyIncrease( &def->eyePath ) ) {
			WiredCurve_SetConstantSpeed( &def->eyePath, def->totalTimeSec );
			/* SetConstantSpeed can still tie two times if a segment had zero arc
			   length (coincident positions); keep spans strictly nonzero. */
			WiredSceneEval_EnforceStrictTimes( &def->eyePath );
		} else {
			WiredSceneEval_RescaleTimesToSeconds( &def->eyePath, def->totalTimeSec );
		}
	}

	/* Put each look-at target on the SAME 0..totalTimeSec seconds clock, so
	   sampling eye + target at the same pathSec keeps them in sync. */
	for ( i = 0; i < def->numTargets; i++ ) {
		WiredSceneEval_RescaleTimesToSeconds( &def->targets[i].path, def->totalTimeSec );
	}

	WiredSceneEval_OrderEvents( pb );

	pb->curTargetIndex = -1;                 /* no look-at target yet */
	pb->fovEventActive = 0;
	pb->curFov         = def->hasFov ? def->fovStart : 90.0f;
	pb->accumulatedWaitSec = 0.0f;
	pb->haveLastForward    = 0;
	pb->signals            = WIRED_SCENE_SIGNAL_NONE;

	/* no actor bindings until BindActors runs — every target uses its spline */
	for ( i = 0; i < WIRED_MAX_SCENE_TARGETS; i++ ) {
		pb->actorEntity[i] = -1;
	}
}

void WiredScenePlayback_BindActors( wiredScenePlayback_t *pb, const int *entityNums, int nActors ) {
	int i;

	if ( !pb || !entityNums ) {
		return;
	}
	if ( nActors > WIRED_MAX_SCENE_TARGETS ) {
		nActors = WIRED_MAX_SCENE_TARGETS;
	}
	for ( i = 0; i < nActors; i++ ) {
		/* positional: actor arg i binds look-at target slot i. A negative number
		   is an explicit "leave this slot on its spline". Slots past the scene's
		   numTargets are harmless (Eval only consults a slot that a TARGET event
		   makes active). */
		pb->actorEntity[i] = entityNums[i];
	}
}

/* Authored baseline fov (the static fov{start->end over fovLenSec} block)
   evaluated at wall time `wallSec`. Used both as the pre-event output and to
   seed a FOV event that fires while the baseline is still lerping. */
static float WiredSceneEval_AuthoredFov( const wiredScene_t *def, float wallSec ) {
	float f;
	if ( !def->hasFov ) {
		return 90.0f;   /* no authored fov: a sane default */
	}
	if ( def->fovLenSec <= 0.0f ) {
		return def->fovEnd;
	}
	f = wallSec / def->fovLenSec;
	if ( f < 0.0f ) f = 0.0f;
	if ( f > 1.0f ) f = 1.0f;
	return def->fovStart + ( def->fovEnd - def->fovStart ) * f;
}

/* The fov of a single FOV-event lerp {from -> target over lenSec, from startSec}
   evaluated at wall time `wallSec`. */
static float WiredSceneEval_EventFovAt( float from, float target, float lenSec,
                                      float startSec, float wallSec ) {
	float f;
	if ( lenSec <= 0.0f ) {
		return target;   /* instant */
	}
	f = ( wallSec - startSec ) / lenSec;
	if ( f < 0.0f ) f = 0.0f;
	if ( f > 1.0f ) f = 1.0f;
	return from + ( target - from ) * f;
}

/*
Re-derive the whole event-driven cursor from scratch for wall time `wallSec`,
and return the WAIT-warped PATH time (seconds) to sample the eye path at. This
is STATELESS w.r.t. prior Eval calls: every derived field (curTargetIndex, the
fovEvent* group, accumulatedWaitSec) is reset at the top and rebuilt by one
forward pass over the time-ordered events, so calling Eval at an EARLIER nowMs
correctly un-fires later events (the header's "safe to call at any nowMs").
Only the host signals are sticky (latched by design; the host drains them).

  wallSec  -- elapsed wall-clock seconds since startMs (clamped >= 0).
  returns  -- the path time (seconds) to sample the eye path at.

WAIT handling: the path clock is wall time minus the total HELD time so far.
Held time is the measure of the UNION of every WAIT's hold window [w, w+dur)
intersected with [0, wallSec] — a union, NOT a sum, so overlapping/nested
windows hold at most 1:1 (never faster) and the path clock stays monotone-non-
decreasing. Windows are merged in the time-ordered pass: a window that starts
at or before the running union's end just extends it.
*/
static float WiredSceneEval_Resolve( wiredScenePlayback_t *pb, float wallSec ) {
	const wiredScene_t *def = pb->def;
	float heldSec   = 0.0f;   /* measure of the union of hold windows <= wallSec */
	float elapsedWaitSec = 0.0f;  /* union measure of FULLY-elapsed windows      */
	float unionStart = 0.0f, unionEnd = 0.0f;  /* current merged hold window      */
	int   haveUnion  = 0;
	int   oi;

	/* reset the per-call derived cursor (signals stay latched) */
	pb->curTargetIndex = -1;
	pb->fovEventActive = 0;
	/* director state defaults for a scene: third-person off, HUD hidden, not
	   frozen — the events toggle these on; re-derived so a rewind un-does them. */
	pb->thirdPerson  = 0;
	pb->hudVisible   = 0;
	pb->playerFrozen = 0;
	pb->captionFired = 0;
	pb->captionKey[0] = '\0';

	for ( oi = 0; oi < pb->numEvents; oi++ ) {
		const wiredSceneEvent_t *ev = &def->events[ pb->evOrder[oi] ];
		float evWallSec = ev->timeMs * 0.001f;

		/* An event fires once wall time reaches its authored trigger time. */
		if ( wallSec < evWallSec ) {
			break;   /* time-ordered: nothing later has fired either */
		}

		switch ( ev->type ) {
			case WSCENE_EV_WAIT: {
				float dur = ev->fparam;   /* hold seconds */
				float ws, we;
				if ( dur < 0.0f ) dur = 0.0f;
				ws = evWallSec;
				we = evWallSec + dur;
				if ( !haveUnion ) {
					unionStart = ws; unionEnd = we; haveUnion = 1;
				} else if ( ws <= unionEnd ) {
					if ( we > unionEnd ) unionEnd = we;   /* overlap -> extend */
				} else {
					/* disjoint: bank the finished window, start a new one */
					float clampedEnd = ( unionEnd < wallSec ) ? unionEnd : wallSec;
					heldSec += clampedEnd - unionStart;
					if ( unionEnd <= wallSec ) elapsedWaitSec += unionEnd - unionStart;
					unionStart = ws; unionEnd = we;
				}
				break;
			}
			case WSCENE_EV_TARGET: {
				int idx = WiredSceneEval_FindTarget( def, ev->sparam );
				if ( idx >= 0 ) {
					pb->curTargetIndex = idx;   /* unknown name leaves the previous */
				}
				break;
			}
			case WSCENE_EV_FOV: {
				/* seed from the fov value at THIS event's fire time: the authored
				   baseline, or (if a prior FOV event is still active) that event's
				   lerp — so chained FOV events start where the previous left off,
				   independent of frame timing. */
				float fireSec = ev->timeMs * 0.001f;
				float seed = pb->fovEventActive
				    ? WiredSceneEval_EventFovAt( pb->fovEventFrom, pb->fovEventTarget,
				                               pb->fovEventLenSec,
				                               pb->fovEventStartMs * 0.001f, fireSec )
				    : WiredSceneEval_AuthoredFov( def, fireSec );
				pb->fovEventFrom    = seed;
				pb->fovEventActive  = 1;
				pb->fovEventTarget  = ev->fparam;
				pb->fovEventLenSec  = ( ev->fparam2 > 0.0f ) ? ev->fparam2 : 0.0f;
				pb->fovEventStartMs = ev->timeMs;
				break;
			}
			case WSCENE_EV_FADEOUT:
				pb->signals    |= WIRED_SCENE_SIGNAL_FADEOUT;
				pb->fadeSeconds = ev->fparam;
				break;
			case WSCENE_EV_FADEIN:
				pb->signals    |= WIRED_SCENE_SIGNAL_FADEIN;
				pb->fadeSeconds = ev->fparam;
				break;
			case WSCENE_EV_SCENE:
				pb->signals |= WIRED_SCENE_SIGNAL_CHAIN;
				{
					int n = (int)sizeof( pb->chainName ) - 1;
					strncpy( pb->chainName, ev->sparam, (size_t)n );
					pb->chainName[n] = '\0';
				}
				break;
			case WSCENE_EV_STOP:
				pb->signals |= WIRED_SCENE_SIGNAL_STOP;
				break;
			/* director state — last event wins (re-derived each pass) */
			case WSCENE_EV_THIRDPERSON:
				pb->thirdPerson = ( ev->fparam != 0.0f );
				break;
			case WSCENE_EV_HUD:
				pb->hudVisible = ( ev->fparam != 0.0f );
				break;
			case WSCENE_EV_PLAYERFREEZE:
				pb->playerFrozen = ( ev->fparam != 0.0f );
				break;
			case WSCENE_EV_CAPTION:
				/* newest caption at-or-before nowMs is the active one; the host
				   logs/shows it (captionFired latches per Eval, see WiredScene_Eval). */
				pb->captionFired = 1;
				{
					int n = (int)sizeof( pb->captionKey ) - 1;
					strncpy( pb->captionKey, ev->sparam, (size_t)n );
					pb->captionKey[n] = '\0';
				}
				break;
			case WSCENE_EV_FEATHER:
			default:
				break;   /* FEATHER is folded into the arc-length base; no cursor */
		}
	}

	/* bank the last still-open merged window (clamped to wallSec) */
	if ( haveUnion ) {
		float clampedEnd = ( unionEnd < wallSec ) ? unionEnd : wallSec;
		heldSec += clampedEnd - unionStart;
		if ( unionEnd <= wallSec ) elapsedWaitSec += unionEnd - unionStart;
	}

	/* accumulatedWaitSec reports the wait time that has FULLY elapsed (the
	   permanent shift); heldSec additionally includes the partial hold of any
	   still-open window, which is what actually warps the current sample. */
	pb->accumulatedWaitSec = elapsedWaitSec;

	{
		float pathSec = wallSec - heldSec;   /* monotone by construction */
		if ( pathSec < 0.0f ) pathSec = 0.0f;
		return pathSec;
	}
}

/* Resolve the output fov at `wallSec`: a fired FOV event's lerp overrides the
   authored baseline (which itself covers the no-fov-block default). */
static float WiredSceneEval_Fov( const wiredScenePlayback_t *pb, float wallSec ) {
	if ( pb->fovEventActive ) {
		return WiredSceneEval_EventFovAt( pb->fovEventFrom, pb->fovEventTarget,
		                                pb->fovEventLenSec,
		                                pb->fovEventStartMs * 0.001f, wallSec );
	}
	return WiredSceneEval_AuthoredFov( pb->def, wallSec );
}

/* Player-relative transform: interpret `offset` (a sampled eye-path/target knot)
   as an OFFSET from the anchor. Rotate its XY around Z by anchorYaw (the yaw the
   player looks along — orbital/over-the-shoulder), keep Z, then add anchorOrigin.
   Yaw is degrees (q3 convention). world = identity when the caller skips this. */
static void WiredSceneEval_PlayerTransform( const float offset[3],
                                            const float anchorOrigin[3], float anchorYaw,
                                            float out[3] ) {
	float rad = anchorYaw * (float)( M_PI / 180.0 );
	float c = (float)cos( rad );
	float s = (float)sin( rad );
	out[0] = anchorOrigin[0] + offset[0] * c - offset[1] * s;
	out[1] = anchorOrigin[1] + offset[0] * s + offset[1] * c;
	out[2] = anchorOrigin[2] + offset[2];
}

int WiredScene_EvalAnchored( wiredScenePlayback_t *pb, int nowMs,
                             const float anchorOrigin[3], float anchorYaw,
                             const float (*actorOrigins)[3], const int *actorValid,
                             float out_origin[3], float out_angles[3], float *out_fov ) {
	const wiredScene_t *def;
	float wallSec, pathSec;
	float eyePos[3], forward[3];
	int   playerSpace;

	if ( !pb || !pb->def || !out_origin || !out_angles || !out_fov ) {
		return 0;
	}
	def = pb->def;
	playerSpace = ( def->cameraSpace == WSCENE_SPACE_PLAYER && anchorOrigin != NULL );

	wallSec = ( nowMs - pb->startMs ) * 0.001f;
	if ( wallSec < 0.0f ) wallSec = 0.0f;

	/* Advance cursor + get the WAIT-warped path time. */
	pathSec = WiredSceneEval_Resolve( pb, wallSec );

	/* ── origin: sample the eye path (GetValue is boundary-clamped) ─────── */
	WiredCurve_GetValue( &def->eyePath, pathSec, eyePos );
	if ( playerSpace ) {
		float world[3];
		WiredSceneEval_PlayerTransform( eyePos, anchorOrigin, anchorYaw, world );
		VectorCopy( world, eyePos );   /* eyePos is now the world eye point */
	}
	VectorCopy( eyePos, out_origin );

	/* ── facing: look-at active target, else the eye tangent ───────────── */
	if ( pb->curTargetIndex >= 0 && pb->curTargetIndex < def->numTargets ) {
		float targetPos[3];
		int   slot  = pb->curTargetIndex;
		/* A slot is actor-driven THIS FRAME only if the host supplied a live origin
		   for it (actorOrigins != NULL AND actorValid[slot]). actorValid lets the
		   host bind an actor persistently (pb->actorEntity) yet fall back to the
		   spline on a frame the actor is not live (e.g. outside the local PVS, so its
		   origin would be stale/zero). With actorValid NULL, any non-NULL actorOrigins
		   slot is taken as valid (the world unit tests use this shorthand). */
		int   bound = ( actorOrigins != NULL && pb->actorEntity[slot] >= 0
		                && ( actorValid == NULL || actorValid[slot] ) );
		if ( bound ) {
			/* actor-bound target: look at the LIVE entity's world origin (supplied by
			   the host), not the authored spline. Already world-space — no player
			   transform (that would double-apply the anchor). */
			VectorCopy( actorOrigins[slot], targetPos );
		} else {
			WiredCurve_GetValue( &def->targets[ slot ].path, pathSec, targetPos );
			if ( playerSpace ) {
				/* transform the target into the SAME world frame as the eye */
				float world[3];
				WiredSceneEval_PlayerTransform( targetPos, anchorOrigin, anchorYaw, world );
				VectorCopy( world, targetPos );
			}
		}
		VectorSubtract( targetPos, eyePos, forward );
	} else {
		WiredCurve_GetFirstDerivative( &def->eyePath, pathSec, forward );
		if ( playerSpace ) {
			/* the tangent is a direction — rotate by yaw only (no translate) */
			float dir[3] = { 0.0f, 0.0f, 0.0f };
			WiredSceneEval_PlayerTransform( forward, dir, anchorYaw, forward );
		}
	}

	if ( WiredSceneEval_Normalize( forward ) <= 1e-6f ) {
		/* degenerate facing — zero OR non-finite (single knot, coincident-time
		   knots giving a NaN/Inf tangent, or a target sitting on the eye): reuse
		   the last good forward so the view never snaps to a default or NaN. */
		if ( pb->haveLastForward ) {
			VectorCopy( pb->lastForward, forward );
		} else {
			forward[0] = 1.0f; forward[1] = 0.0f; forward[2] = 0.0f;
		}
	} else {
		VectorCopy( forward, pb->lastForward );
		pb->haveLastForward = 1;
	}

	vectoangles( forward, out_angles );
	out_angles[ROLL] = 0.0f;   /* no authored roll in the data model */

	/* ── fov: authored baseline until a FOV event overrides ────────────── */
	pb->curFov = WiredSceneEval_Fov( pb, wallSec );
	*out_fov   = pb->curFov;

	/* ── running? STOP signal or the eye path finished ends the cutscene ── */
	if ( pb->signals & WIRED_SCENE_SIGNAL_STOP ) {
		pb->active = 0;
	} else if ( def->eyePath.numKnots >= 2 && WiredCurve_IsDone( &def->eyePath, pathSec ) ) {
		pb->active = 0;
	}

	return pb->active;
}

/* World-space wrapper: no anchor and no actor binding (world scenes ignore the
   anchor; a player-space scene called this way falls back to world since
   anchorOrigin is NULL). Byte-identical to the pre-actor-binding behavior. */
int WiredScene_Eval( wiredScenePlayback_t *pb, int nowMs,
                      float out_origin[3], float out_angles[3], float *out_fov ) {
	return WiredScene_EvalAnchored( pb, nowMs, NULL, 0.0f, NULL, NULL, out_origin, out_angles, out_fov );
}
