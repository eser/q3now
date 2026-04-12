/*
===========================================================================
ai_movement.c — Bot movement system

Five integrated components for bots operating under modern physics:

  1. BotPredictTrajectory  — forward Euler simulation of PM_AirMove
  2. BotAirSteer           — in-flight correction via air-control steering
  3. (Strafe jump)         — delegated to BotStrafeJumpCheck in ai_dmq3.c
  4. BotDoubleJumpThink    — coordinated two-jump sequence (pm_jump_z)
  5. BotWallJumpThink      — wall detection + jump-off (pm_walljumps)

BotMovementThink() is the master controller; it replaces the bare
BotStrafeJumpCheck() call in ai_dmnet.c.
===========================================================================
*/
#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/aasfile.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
#include "ai_main.h"
#include "ai_movement.h"
#include "ai_dmq3.h"

/*
 * Modern physics parameters — defined as plain globals in bg_pmove.c.
 * No extern header exists for these; declare them here.
 */
extern float pm_airaccelerate;
extern float pm_airstopaccelerate;
extern float pm_aircontrol;
extern float pm_strafeaccelerate;
extern float pm_wishspeed;
extern float pm_jump_z;
extern int   pm_walljumps;


/*
===================
BotPredictTrajectory

Forward Euler simulation of air movement (PM_AirMove).
No world-geometry tracing — free-flight approximation.

Each step replicates:
  - PM_CmdScale  (ps->speed = 320, no upmove)
  - PM_Accelerate (Q2 style)
  - PM_Aircontrol (pure forward / backward only)
  - gravity integration

Landing is detected when origin[2] drops to or below startOrigin[2]
with negative z-velocity.  Callers may do a single downward trace from
result.origin to find the actual ground contact point.
===================
*/
bot_trajectory_t BotPredictTrajectory( const vec3_t startOrigin,
                                        const vec3_t startVelocity,
                                        const vec3_t wishdir,
                                        float        forwardmove,
                                        float        rightmove,
                                        float        gravity )
{
	bot_trajectory_t result;
	vec3_t  origin, velocity;
	float   dt;
	int     step, i;

	VectorCopy( startOrigin,   origin );
	VectorCopy( startVelocity, velocity );
	VectorCopy( startOrigin,   result.origin );
	VectorCopy( startVelocity, result.velocity );
	result.time   = 0.0f;
	result.landed = qfalse;

	dt = TRAJ_STEP_MS / 1000.0f;   /* 0.008 s at 125 Hz */

	for ( step = 0; step < TRAJ_MAX_STEPS; step++ ) {
		float fmax = fabsf( forwardmove );
		if ( fabsf( rightmove ) > fmax ) fmax = fabsf( rightmove );

		if ( fmax > 0.5f ) {
			vec3_t  wishvel, wdir, rightdir;
			float   total, scale, wishspeedRaw, wishspeed, accel;
			float   currentspeed, addspeed, accelspeed;
			qboolean pureForward, pureStrafe;

			/* Right vector: 90° CW from wishdir in XY plane */
			rightdir[0] = -wishdir[1];
			rightdir[1] =  wishdir[0];
			rightdir[2] =  0.0f;

			/* Build wishvel = forward*fmove + right*smove (PM_AirMove) */
			wishvel[0] = wishdir[0]*forwardmove + rightdir[0]*rightmove;
			wishvel[1] = wishdir[1]*forwardmove + rightdir[1]*rightmove;
			wishvel[2] = 0.0f;

			/* PM_CmdScale equivalent — ps->speed assumed 320 */
			total = sqrtf( forwardmove*forwardmove + rightmove*rightmove );
			scale = 320.0f * fmax / ( 127.0f * total );

			VectorCopy( wishvel, wdir );
			wishspeedRaw = VectorNormalize( wdir );
			wishspeed    = wishspeedRaw * scale;

			pureStrafe  = ( fabsf(rightmove) > 0.5f && fabsf(forwardmove) < 0.5f );
			pureForward = ( fabsf(rightmove) < 0.5f );

			if ( pureStrafe ) {
				/* movementDir 2 or 6: cap to pm_wishspeed, use strafeaccel */
				if ( wishspeed > pm_wishspeed ) wishspeed = pm_wishspeed;
				accel = pm_strafeaccelerate;
			} else {
				accel = ( DotProduct( velocity, wdir ) < 0.0f )
				        ? pm_airstopaccelerate : pm_airaccelerate;
			}

			/* PM_Accelerate (Q2 style) */
			currentspeed = DotProduct( velocity, wdir );
			addspeed = wishspeed - currentspeed;
			if ( addspeed > 0.0f ) {
				accelspeed = accel * dt * wishspeed;
				if ( accelspeed > addspeed ) accelspeed = addspeed;
				for ( i = 0; i < 3; i++ )
					velocity[i] += accelspeed * wdir[i];
			}

			/* PM_Aircontrol — only for pure forward / backward input */
			if ( pureForward && pm_aircontrol > 0.0f && wishspeedRaw > 0.0f ) {
				float zspeed, speed, dot, k;
				zspeed = velocity[2];
				velocity[2] = 0.0f;
				speed = VectorNormalize( velocity );
				if ( speed > 0.0f ) {
					dot = DotProduct( velocity, wdir );
					k   = 32.0f * pm_aircontrol * dot * dot * dt;
					if ( dot > 0.0f ) {
						for ( i = 0; i < 2; i++ )
							velocity[i] = velocity[i] * speed + wdir[i] * k;
						VectorNormalize( velocity );
					}
					for ( i = 0; i < 2; i++ ) velocity[i] *= speed;
				}
				velocity[2] = zspeed;
			}
		}

		/* Gravity */
		velocity[2] -= gravity * dt;

		/* Integrate position */
		for ( i = 0; i < 3; i++ ) origin[i] += velocity[i] * dt;

		result.time += (float)TRAJ_STEP_MS;

		/* Approximate landing: descended below start height */
		if ( velocity[2] < 0.0f && origin[2] <= startOrigin[2] ) {
			result.landed = qtrue;
			break;
		}
	}

	VectorCopy( origin,   result.origin );
	VectorCopy( velocity, result.velocity );
	return result;
}


/*
===================
BotAirSteer

While airborne, continuously corrects trajectory toward the current
navigation goal.

Strategy: predict the coast trajectory (zero input) to find where the
bot will land, compare with goal, steer by facing the correction
direction and pressing forward. pm_aircontrol = 150 redirects velocity
toward the facing direction with pure forward input — far more effective
than mixing forwardmove + rightmove (which would trigger strafe mode
instead of air-control mode).

Skipped when the bot has a recently visible enemy (combat aim takes
priority) and when predicted landing is close enough (< 24 units error).
===================
*/
void BotAirSteer( bot_state_t *bs )
{
	bot_goal_t       goal;
	bot_trajectory_t coast;
	vec3_t           corrDir, corrAngles;
	float            errorDist;

	if ( bs->cur_ps.groundEntityNum != ENTITYNUM_NONE )
		return; /* on ground */

	/* Skip during active combat — don't fight aim system */
	if ( bs->enemy >= 0 && bs->enemyvisible_time > bs->ltime - 1.0f )
		return;

	/* Where will we land with current velocity and no further input? */
	coast = BotPredictTrajectory( bs->origin, bs->velocity,
	                               vec3_origin, 0.0f, 0.0f, 800.0f );

	if ( !trap_BotGetTopGoal( bs->gs, &goal ) )
		return;

	/* Horizontal error: predicted landing → goal */
	VectorSubtract( goal.origin, coast.origin, corrDir );
	corrDir[2] = 0.0f;
	errorDist  = VectorLength( corrDir );

	if ( errorDist < 24.0f )
		return; /* within tolerance */

	if ( errorDist > 600.0f )
		return; /* goal is too far away to be the landing target */

	VectorNormalize( corrDir );
	vectoangles( corrDir, corrAngles );
	bs->ideal_viewangles[YAW] = corrAngles[YAW];

	/* Full forward input — air control steers velocity toward facing dir */
	trap_EA_MoveForward( bs->client );
}


/*
===================
BotShouldDoubleJump

Returns qtrue if the bot's top goal is between 100-200 units above and
within 400 horizontal units — the window where double-jump adds height
that a normal jump cannot reach but which AAS considers unreachable.
===================
*/
qboolean BotShouldDoubleJump( bot_state_t *bs )
{
	bot_goal_t goal;
	vec3_t     diff;
	float      heightNeeded, horizDist;

	if ( pm_jump_z <= 0.0f ) return qfalse;
	if ( bs->cur_ps.groundEntityNum == ENTITYNUM_NONE ) return qfalse;

	if ( !trap_BotGetTopGoal( bs->gs, &goal ) ) return qfalse;

	VectorSubtract( goal.origin, bs->origin, diff );
	heightNeeded = diff[2];
	diff[2] = 0.0f;
	horizDist = VectorLength( diff );

	/* Standard jump max height ~112 units; double jump ~213 units */
	if ( heightNeeded < 100.0f || heightNeeded > 200.0f ) return qfalse;
	if ( horizDist > 400.0f ) return qfalse;

	return qtrue;
}


/*
===================
BotDoubleJumpThink

Coordinates the two-jump sequence for targets 100-200 units above.

Phase 1 (on ground, no prior jump): issue first jump.
Phase 2 (airborne): wait for landing.
Phase 3 (on ground, STAT_JUMPTIME > 0): issue second jump — bg_pmove
         detects the open window and adds +pm_jump_z to velocity.

If STAT_JUMPTIME expires before the second jump is issued, the sequence
aborts cleanly; the bot reverts to normal navigation next frame.
===================
*/
void BotDoubleJumpThink( bot_state_t *bs )
{
	qboolean onGround = ( bs->cur_ps.groundEntityNum != ENTITYNUM_NONE );

	if ( !bs->doublejump.wantDoubleJump ) {
		if ( BotShouldDoubleJump( bs ) )
			bs->doublejump.wantDoubleJump = qtrue;
		else
			return;
	}

	/* Phase 1: issue first jump */
	if ( !bs->doublejump.firstJumpDone ) {
		if ( onGround ) {
			trap_EA_Jump( bs->client );
			bs->doublejump.firstJumpDone = qtrue;
#if FEAT_WIREDNET_OBSERVER
			trap_WiredNet_EmitBotEvent( bs->entitynum, "doublejump_start",
			                            1, 0, bs->origin );
#endif
		}
		return;
	}

	/* Phase 2: airborne — wait */
	if ( !onGround )
		return;

	/* Phase 3: landed — check window */
	if ( bs->cur_ps.stats[STAT_JUMPTIME] > 0 ) {
		trap_EA_Jump( bs->client );
#if FEAT_WIREDNET_OBSERVER
		trap_WiredNet_EmitBotEvent( bs->entitynum, "doublejump_second",
		                            1, (int)pm_jump_z, bs->origin );
#endif
	}
	/* Window expired or second jump issued — always reset */
	bs->doublejump.wantDoubleJump = qfalse;
	bs->doublejump.firstJumpDone  = qfalse;
	bs->doublejump.landTime       = 0;
}


/*
===================
BotFindWallJumpOpportunity

Scans 8 horizontal directions at 40-unit distance for solid walls.
On a hit, records the wall normal and point, sets wantWallJump = qtrue.

Called when airborne with a high target above.  bg_pmove's
PM_CheckWallJump does its own 1-unit forward trace; the bot just needs
to face toward the wall and press jump — the physics engine triggers
the wall jump automatically.
===================
*/
qboolean BotFindWallJumpOpportunity( bot_state_t *bs )
{
	bot_goal_t goal;
	trace_t    trace;
	vec3_t     testEnd, dir;
	float      heightNeeded;
	int        i;

	if ( !pm_walljumps )                                         return qfalse;
	if ( bs->cur_ps.groundEntityNum != ENTITYNUM_NONE )          return qfalse;
	if ( bs->cur_ps.stats[STAT_WALLJUMPS] >= MAX_WALLJUMPS )     return qfalse;

	if ( !trap_BotGetTopGoal( bs->gs, &goal ) )                  return qfalse;
	heightNeeded = goal.origin[2] - bs->origin[2];
	if ( heightNeeded < 50.0f )                                  return qfalse;

	/* Don't wall-jump when falling too fast */
	if ( bs->cur_ps.velocity[2] < WALLJUMP_BOOST * -2 )         return qfalse;

	for ( i = 0; i < 8; i++ ) {
		float angle = i * 45.0f * ( M_PI / 180.0f );
		dir[0] = cosf( angle );
		dir[1] = sinf( angle );
		dir[2] = 0.0f;

		VectorMA( bs->origin, 40.0f, dir, testEnd );
		trap_Trace( &trace, bs->origin, NULL, NULL, testEnd,
		            bs->entitynum, MASK_PLAYERSOLID );

		if ( trace.fraction < 1.0f &&
		     ( trace.contents & (CONTENTS_SOLID | CONTENTS_PLAYERCLIP) ) &&
		     !( trace.surfaceFlags & SURF_SKY ) )
		{
			VectorCopy( trace.plane.normal, bs->walljump.wallNormal );
			VectorCopy( trace.endpos,       bs->walljump.wallPoint  );
			bs->walljump.wantWallJump = qtrue;
			return qtrue;
		}
	}

	return qfalse;
}


/*
===================
BotWallJumpThink

Faces the detected wall (opposite of wall normal) and presses forward
+ jump.  bg_pmove's PM_CheckWallJump traces 1 unit forward from origin
using the player's actual facing direction; once the bot is close
enough and facing the wall, the wall jump fires automatically.

Clears wantWallJump if the bot lands without wall-jumping.
===================
*/
void BotWallJumpThink( bot_state_t *bs )
{
	vec3_t toWall, wallAngles;

	if ( !bs->walljump.wantWallJump ) return;

	if ( bs->cur_ps.groundEntityNum != ENTITYNUM_NONE ) {
		/* Landed before wall jump executed */
		bs->walljump.wantWallJump = qfalse;
		return;
	}

	/* Face toward the wall: negate the outward normal */
	VectorNegate( bs->walljump.wallNormal, toWall );
	toWall[2] = 0.0f;
	if ( VectorLength( toWall ) < 0.01f ) {
		bs->walljump.wantWallJump = qfalse;
		return;
	}
	VectorNormalize( toWall );
	vectoangles( toWall, wallAngles );
	bs->ideal_viewangles[YAW] = wallAngles[YAW];

	/* Move toward wall + hold jump — PM_WallJump triggers on 1-unit trace hit */
	trap_EA_MoveForward( bs->client );
	trap_EA_Jump( bs->client );

#if FEAT_WIREDNET_OBSERVER
	trap_WiredNet_EmitBotEvent( bs->entitynum, "walljump",
	                             1, 0, bs->origin );
#endif
}


/*
===================
BotMovementThink

Master movement controller.  Called after trap_BotMoveToGoal has issued
its normal navigation EA commands.  Overrides or supplements those
commands based on movement priority.

Replaces the bare BotStrafeJumpCheck() call in ai_dmnet.c so that all
movement goes through a single coordination point.

Priority (highest first):
  1. Active wall jump: wantWallJump already set from last frame
  2. New wall jump opportunity: airborne + high target + wall nearby
  3. Strafe jump: BotStrafeJumpCheck() (existing logic)
  4. Double jump: target 100-200 units above + double-jump window
  5. Air steering: general in-flight trajectory correction
===================
*/
void BotMovementThink( bot_state_t *bs, bot_moveresult_t *moveresult )
{
	qboolean airborne = ( bs->cur_ps.groundEntityNum == ENTITYNUM_NONE );

	/* Reset wall-jump attempt on landing */
	if ( !airborne )
		bs->walljump.wantWallJump = qfalse;

	/* ── Edge-detection: grounded + moving toward void ──────────────── *
	 * Phase A: if currently in a back-off window (set by a previous      *
	 * detection), keep backing up and let the AAS try a different path.  *
	 * Phase B: probe 4 frames ahead; if nothing solid within 600 units   *
	 * below the probe point, start the back-off window and back up.      *
	 * 600 units catches all Q3 maps including deep void shafts.          */
	if ( !airborne ) {
		VectorCopy( bs->origin, bs->last_ground_pos );  /* keep last safe position current */

		/* persistent void-ledge check: if within 200 units of a previously fatal position, back off */
		{
			int vi;
			for ( vi = 0; vi < bs->num_void_spots; vi++ ) {
				vec3_t diff;
				VectorSubtract( bs->origin, bs->void_spots[vi], diff );
				diff[2] = 0.0f;  /* horizontal distance only */
				if ( VectorLengthSquared( diff ) < (200.0f * 200.0f) ) {
					bs->edge_block_until = FloatTime() + 0.8f;
					trap_BotResetAvoidReach( bs->ms );
					trap_EA_ResetInput( bs->client );
					trap_EA_MoveBack( bs->client );
					return;
				}
			}
		}

		if ( FloatTime() < bs->edge_block_until ) {
			/* Still in back-off window: cancel forward movement, back up */
			trap_EA_ResetInput( bs->client );
			trap_EA_MoveBack( bs->client );
			return;
		}

		{
			vec3_t      hvel, probeEnd, downEnd;
			float       hspeed;
			bsp_trace_t tr;
			vec3_t      mins = {-15, -15, -24}, maxs = {15, 15, 32};

			hvel[0] = bs->velocity[0];
			hvel[1] = bs->velocity[1];
			hvel[2] = 0.0f;
			hspeed  = VectorLength( hvel );

			if ( hspeed > 10.0f ) {
				float  dt    = bs->thinktime > 0.001f ? bs->thinktime : 0.05f;
				float  probe = hspeed * dt * 8.0f;   /* 8-frame lookahead */
				vec3_t fwd;
				if ( probe < 96.0f ) probe = 96.0f;  /* minimum 96 units */

				VectorCopy( hvel, fwd );
				VectorNormalize( fwd );
				VectorMA( bs->origin, probe, fwd, probeEnd );
				VectorCopy( probeEnd, downEnd );
				downEnd[2] -= 600.0f;   /* deep enough for all Q3 map voids */

				BotAI_Trace( &tr, probeEnd, mins, maxs, downEnd,
				             bs->entitynum, CONTENTS_SOLID | CONTENTS_PLAYERCLIP );

				if ( tr.fraction >= 1.0f ) {
					/* Void ahead — back off for 0.8s and try a different path */
					bs->edge_block_until = FloatTime() + 0.8f;
					trap_BotResetAvoidReach( bs->ms );
					trap_EA_ResetInput( bs->client );
					trap_EA_MoveBack( bs->client );
					return;
				}
			}
		}
	}

	/* ── Airborne void check ────────────────────────────────────────── *
	 * Applies ONLY to random combat jumps (traveltype WALK/CROUCH/0).   *
	 * AAS-directed jumps (BARRIERJUMP, JUMP, WALKOFFLEDGE, STRAFEJUMP,  *
	 * JUMPPAD, etc.) are skipped — they have a computed landing target.  *
	 * Runs on both ascending and descending arcs so steering starts at   *
	 * the beginning of the jump, maximising correction time.            */
	if ( airborne ) {
		int tt = moveresult->traveltype & TRAVELTYPE_MASK;
		if ( tt < TRAVEL_BARRIERJUMP ) {   /* 0/INVALID/WALK/CROUCH only */
			vec3_t      hvel, probeEnd, downEnd;
			float       hspeed;
			bsp_trace_t tr;
			vec3_t      mins = {-15, -15, -24}, maxs = {15, 15, 32};

			hvel[0] = bs->velocity[0];
			hvel[1] = bs->velocity[1];
			hvel[2] = 0.0f;
			hspeed  = VectorLength( hvel );

			if ( hspeed > 50.0f ) {
				float  dt    = bs->thinktime > 0.001f ? bs->thinktime : 0.05f;
				float  probe = hspeed * dt * 6.0f;
				vec3_t fwd;
				if ( probe < 64.0f ) probe = 64.0f;

				VectorCopy( hvel, fwd );
				VectorNormalize( fwd );
				VectorMA( bs->origin, probe, fwd, probeEnd );
				VectorCopy( probeEnd, downEnd );
				downEnd[2] -= 800.0f;

				BotAI_Trace( &tr, probeEnd, mins, maxs, downEnd,
				             bs->entitynum, CONTENTS_SOLID | CONTENTS_PLAYERCLIP );

				if ( tr.fraction >= 1.0f ) {
					/* Void ahead during a combat jump.
					 * The bot is still facing toward the void, so MoveBack
					 * immediately activates PM_Aircontrol in the reverse direction,
					 * pushing velocity away from void without waiting for a turn. */
					trap_EA_ResetInput( bs->client );
					trap_EA_MoveBack( bs->client );
					return;
				}
			}
		}
	}

	/* Priority 1 & 2: wall jump */
	if ( airborne ) {
		if ( bs->walljump.wantWallJump ) {
			BotWallJumpThink( bs );
			return;
		}
		if ( BotFindWallJumpOpportunity( bs ) ) {
			BotWallJumpThink( bs );
			return;
		}
	}

	/* Priority 3: strafe jump — uses existing ai_dmq3.c implementation
	   which already accounts for air control angle geometry.     */
	BotStrafeJumpCheck( bs, moveresult );
	if ( bs->strafejump_active ) {
		/* Double jump can chain with the first landing of a strafe run */
		if ( bs->doublejump.wantDoubleJump )
			BotDoubleJumpThink( bs );
		return;
	}

	/* Priority 4: double jump for high targets */
	if ( bs->doublejump.wantDoubleJump || BotShouldDoubleJump( bs ) ) {
		BotDoubleJumpThink( bs );
		if ( airborne )
			BotAirSteer( bs );
		return;
	}

	/* Priority 5: generic air steering (no special movement active) */
	if ( airborne )
		BotAirSteer( bs );
}
