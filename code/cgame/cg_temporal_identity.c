// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "cg_temporal_identity.h"

#include <limits.h>

static qboolean CG_TemporalIdentitySignatureMatches(
		const cgTemporalIdentity_t *identity, const entityState_t *state,
		uint32_t continuity ) {
	return identity->entityNumber == (uint32_t)state->number
		&& identity->eType == (uint32_t)state->eType
		&& identity->modelindex == (uint32_t)state->modelindex
		&& identity->modelindex2 == (uint32_t)state->modelindex2
		&& identity->clientNum == (uint32_t)state->clientNum
		&& identity->continuity == continuity;
}

static void CG_TemporalIdentityStoreSignature( cgTemporalIdentity_t *identity,
		const entityState_t *state, uint32_t continuity ) {
	identity->entityNumber = (uint32_t)state->number;
	identity->eType = (uint32_t)state->eType;
	identity->modelindex = (uint32_t)state->modelindex;
	identity->modelindex2 = (uint32_t)state->modelindex2;
	identity->clientNum = (uint32_t)state->clientNum;
	identity->continuity = continuity;
}

qboolean CG_TemporalIdentityObserve( cgTemporalIdentity_t *identity,
		const entityState_t *state, uint32_t continuity,
		qboolean discontinuity ) {
	qboolean changed;

	if ( !identity || !state || state->number < 0
			|| state->number >= MAX_GENTITIES || identity->disabled ) {
		return qfalse;
	}

	if ( identity->generation == 0u ) {
		identity->generation = 1u;
		identity->pendingDiscontinuity = qfalse;
		CG_TemporalIdentityStoreSignature( identity, state, continuity );
		return qtrue;
	}

	changed = !CG_TemporalIdentitySignatureMatches( identity, state, continuity );
	if ( discontinuity || identity->pendingDiscontinuity || changed ) {
		if ( identity->generation == UINT32_MAX ) {
			identity->generation = 0u;
			identity->pendingDiscontinuity = qfalse;
			identity->disabled = qtrue;
			return qfalse;
		}
		identity->generation++;
	}

	identity->pendingDiscontinuity = qfalse;
	CG_TemporalIdentityStoreSignature( identity, state, continuity );
	return qtrue;
}

void CG_TemporalIdentityMarkDiscontinuity( cgTemporalIdentity_t *identity ) {
	if ( identity && !identity->disabled ) {
		identity->pendingDiscontinuity = qtrue;
	}
}

qboolean CG_TemporalIdentityBuildMotion( const cgTemporalIdentity_t *identity,
		uint32_t entityNumber, refEntityMotionRole_t role,
		refEntityMotion_t *outMotion ) {
	refEntityMotion_t candidate;

	if ( !identity || !outMotion || identity->disabled
			|| identity->generation == 0u
			|| entityNumber >= MAX_GENTITIES
			|| identity->entityNumber != entityNumber
			|| role <= REF_ENTITY_MOTION_ROLE_NONE
			|| role >= REF_ENTITY_MOTION_ROLE_COUNT ) {
		return qfalse;
	}

	candidate.structSize = sizeof( candidate );
	candidate.version = REF_ENTITY_MOTION_VERSION;
	candidate.ownerId = entityNumber;
	candidate.generation = identity->generation;
	candidate.role = (uint32_t)role;
	candidate.flags = 0u;
	*outMotion = candidate;
	return qtrue;
}

qboolean CG_TemporalIdentitySubmit( const cgTemporalIdentity_t *identity,
		uint32_t entityNumber, refEntityMotionRole_t role,
		const refEntity_t *entity, cgTemporalOrdinarySubmitFn ordinarySubmit,
		cgTemporalIdentitySubmitFn temporalSubmit ) {
	refEntityMotion_t motion;

	if ( !entity || !ordinarySubmit ) {
		return qfalse;
	}
	if ( temporalSubmit && CG_TemporalIdentityBuildMotion( identity,
			entityNumber, role, &motion ) ) {
		temporalSubmit( entity, &motion );
		return qtrue;
	}
	ordinarySubmit( entity );
	return qfalse;
}
