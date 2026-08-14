// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "cg_temporal_identity.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "FAIL cgame temporal identity line %d: %s\n", __LINE__, #x ); \
	return 1; \
} } while ( 0 )

_Static_assert( REF_ENTITY_MOTION_ROLE_NONE == 0, "role NONE" );
_Static_assert( REF_ENTITY_MOTION_ROLE_GENERAL == 1, "role GENERAL" );
_Static_assert( REF_ENTITY_MOTION_ROLE_CREATURE_BODY == 2, "role CREATURE" );
_Static_assert( REF_ENTITY_MOTION_ROLE_PLAYER_BODY == 3, "role PLAYER_BODY" );
_Static_assert( REF_ENTITY_MOTION_ROLE_PLAYER_LEGS == 4, "role LEGS" );
_Static_assert( REF_ENTITY_MOTION_ROLE_PLAYER_TORSO == 5, "role TORSO" );
_Static_assert( REF_ENTITY_MOTION_ROLE_PLAYER_HEAD == 6, "role HEAD" );
_Static_assert( REF_ENTITY_MOTION_ROLE_ITEM_PRIMARY == 7, "role ITEM_PRIMARY" );
_Static_assert( REF_ENTITY_MOTION_ROLE_ITEM_BARREL == 8, "role ITEM_BARREL" );
_Static_assert( REF_ENTITY_MOTION_ROLE_ITEM_SECONDARY == 9, "role ITEM_SECONDARY" );
_Static_assert( REF_ENTITY_MOTION_ROLE_MOVER_PRIMARY == 10, "role MOVER_PRIMARY" );
_Static_assert( REF_ENTITY_MOTION_ROLE_MOVER_SECONDARY == 11, "role MOVER_SECONDARY" );
_Static_assert( REF_ENTITY_MOTION_ROLE_GRAPPLE == 12, "role GRAPPLE" );

static int ordinaryCalls, temporalCalls;
static refEntityMotion_t submittedMotion;

static void OrdinarySubmit( const refEntity_t *entity ) {
	if ( entity ) ordinaryCalls++;
}

static void TemporalSubmit( const refEntity_t *entity,
		const refEntityMotion_t *motion ) {
	if ( entity && motion ) {
		temporalCalls++;
		submittedMotion = *motion;
	}
}

static entityState_t MakeState( int number ) {
	entityState_t state;
	memset( &state, 0, sizeof( state ) );
	state.number = number;
	state.eType = ET_GENERAL;
	state.modelindex = 3;
	state.modelindex2 = 4;
	state.clientNum = 5;
	state.weapon = 6;
	state.solid = 7;
	return state;
}

int main( void ) {
	cgTemporalIdentity_t identity;
	entityState_t state;
	refEntityMotion_t motion, before;
	refEntity_t entity;

	memset( &identity, 0, sizeof( identity ) );
	memset( &entity, 0, sizeof( entity ) );
	state = MakeState( 0 );

	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 1u && !identity.disabled );
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 1u );

	// Weapon and collision changes are pose/gameplay state, not incarnation.
	state.weapon++;
	state.solid++;
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 1u );

	state.eType = ET_MOVER;
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 2u );
	state.modelindex++;
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 3u );
	state.modelindex2++;
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 4u );
	state.clientNum++;
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 11u, qfalse ) );
	CHECK( identity.generation == 5u );
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 12u, qfalse ) );
	CHECK( identity.generation == 6u );

	// Snapshot disappearance/reappearance is queued, then consumed once.
	CG_TemporalIdentityMarkDiscontinuity( &identity );
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 12u, qfalse ) );
	CHECK( identity.generation == 7u && !identity.pendingDiscontinuity );

	// EF_TELEPORT/non-interpolated transition cuts the observed incarnation.
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 12u, qtrue ) );
	CHECK( identity.generation == 8u );

	// Servercount/map-restart may mark through multiple lifecycle seams; the
	// pending bit ensures the next observed state advances exactly once.
	CG_TemporalIdentityMarkDiscontinuity( &identity );
	CG_TemporalIdentityMarkDiscontinuity( &identity );
	CHECK( CG_TemporalIdentityObserve( &identity, &state, 12u, qfalse ) );
	CHECK( identity.generation == 9u && !identity.pendingDiscontinuity );

	memset( &motion, 0, sizeof( motion ) );
	CHECK( CG_TemporalIdentityBuildMotion( &identity, 0u,
		REF_ENTITY_MOTION_ROLE_GENERAL, &motion ) );
	CHECK( motion.ownerId == 0u && motion.generation == 9u
		&& motion.role == REF_ENTITY_MOTION_ROLE_GENERAL
		&& RefEntityMotion_IsValid( &motion ) );

	before = motion;
	CHECK( !CG_TemporalIdentityBuildMotion( &identity, 1u,
		REF_ENTITY_MOTION_ROLE_GENERAL, &motion ) );
	CHECK( !memcmp( &motion, &before, sizeof( motion ) ) );
	CHECK( !CG_TemporalIdentityBuildMotion( &identity, 0u,
		REF_ENTITY_MOTION_ROLE_NONE, &motion ) );
	CHECK( !memcmp( &motion, &before, sizeof( motion ) ) );

	ordinaryCalls = temporalCalls = 0;
	CHECK( CG_TemporalIdentitySubmit( &identity, 0u,
		REF_ENTITY_MOTION_ROLE_GENERAL, &entity,
		OrdinarySubmit, TemporalSubmit ) );
	CHECK( temporalCalls == 1 && ordinaryCalls == 0
		&& submittedMotion.ownerId == 0u );
	CHECK( !CG_TemporalIdentitySubmit( &identity, 0u,
		REF_ENTITY_MOTION_ROLE_NONE, &entity,
		OrdinarySubmit, TemporalSubmit ) );
	CHECK( temporalCalls == 1 && ordinaryCalls == 1 );

	identity.generation = UINT32_MAX;
	CHECK( !CG_TemporalIdentityObserve( &identity, &state, 12u, qtrue ) );
	CHECK( identity.disabled && identity.generation == 0u );
	CHECK( !CG_TemporalIdentitySubmit( &identity, 0u,
		REF_ENTITY_MOTION_ROLE_GENERAL, &entity,
		OrdinarySubmit, TemporalSubmit ) );
	CHECK( temporalCalls == 1 && ordinaryCalls == 2 );

	puts( "PASS cgame temporal identity producer contract" );
	return 0;
}
