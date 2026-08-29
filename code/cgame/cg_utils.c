// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cg_utils.c -- A collection of utility functions

===========================================================================
*/
#include "cg_local.h"

qboolean CG_IsFollowing( void ) {
	return ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ? qtrue : qfalse;
}

qboolean CG_IsSpectator( void ) {
	return cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ? qtrue : qfalse;
}

qboolean CG_IsPlayerInvisible( centity_t *cent ) {
	if ( (cent->currentState.powerups & ( 1 << PW_INVIS )) || (cent->currentState.eFlags & EF_CLOAK) ) {
		return qtrue;
	}

	if ( cgs.gametype == GT_KINGOFTHEHILL && cgs.kothGhosts ) {
		if ( !(cent->currentState.powerups & ( 1 << PW_KING )) &&
			cent->muzzleFlashTime + GHOST_FLASH_TIME < cg.time ) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
===========================
CG_EvaluateVisualTrajectory

Evaluate a historical point in the same visual space as cent->lerpOrigin.
Projectile prediction, mover correction and interpolation may move the rendered
entity away from the raw server trajectory. Applying the current visual offset
to every trail sample keeps the model, trail and liquid crossings coherent.
===========================
*/
void CG_EvaluateVisualTrajectory( const centity_t *cent, int atTime, vec3_t result ) {
	vec3_t rawNow;
	vec3_t visualOffset;

	BG_EvaluateTrajectory( &cent->currentState.pos, atTime, result );
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, rawNow );
	VectorSubtract( cent->lerpOrigin, rawNow, visualOffset );
	VectorAdd( result, visualOffset, result );
}

/*
============================
CG_ProjectileTrailStepForSpacing

Convert a desired maximum world-space gap into a bounded time-grid step.
Slow projectiles retain the legacy cadence; fast projectiles receive denser
samples so the visible trail cannot lose contact with the rendered missile.
============================
*/
int CG_ProjectileTrailStepForSpacing( const centity_t *cent, float spacing, int maxStep ) {
	vec3_t velocity;
	float speed;
	int step;

	if ( maxStep < 1 ) maxStep = 1;
	if ( !cent || spacing <= 0.0f ) return maxStep;

	BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity );
	speed = VectorLength( velocity );
	if ( speed <= 1.0f ) return maxStep;

	step = (int)( 1000.0f * spacing / speed + 0.5f );
	if ( step < 8 ) step = 8;
	if ( step > maxStep ) step = maxStep;
	return step;
}

int CG_CrosshairPlayer( void ) {
	if ( cg.time > ( cg.crosshairClientTime + 1000 ) ) {
		return -1;
	}
	return cg.crosshairClientNum;
}

int CG_LastAttacker( void ) {
	if ( !cg.attackerTime ) {
		return -1;
	}
	return cg.snap->ps.persistant[PERS_LAST_ATTACKER];
}

void CG_ModernDrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled ) {
	if ( !border || !borderColor ) return;
	if ( border[0] > 0 ) CG_FillRectNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, border[0] * NORM_VSCALE, borderColor );
	if ( border[2] > 0 ) CG_FillRectNorm( x * NORM_HSCALE, (y + h - border[2]) * NORM_VSCALE, w * NORM_HSCALE, border[2] * NORM_VSCALE, borderColor );
	if ( border[3] > 0 ) CG_FillRectNorm( x * NORM_HSCALE, (y + border[0]) * NORM_VSCALE, border[3] * NORM_HSCALE, (h - border[0] - border[2]) * NORM_VSCALE, borderColor );
	if ( border[1] > 0 ) CG_FillRectNorm( (x + w - border[1]) * NORM_HSCALE, (y + border[0]) * NORM_VSCALE, border[1] * NORM_HSCALE, (h - border[0] - border[2]) * NORM_VSCALE, borderColor );
}
