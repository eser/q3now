// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "cg_refentity_temporal.h"
#include "cg_public.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "FAIL ref entity motion line %d: %s\n", __LINE__, #x ); \
	return 1; \
} } while ( 0 )

_Static_assert( sizeof( refEntityMotion_t ) == 24, "wire POD must remain 24 bytes" );
_Static_assert( CG_R_ADDREFENTITYTOSCENETEMPORAL == 232, "optional trap slot changed" );
_Static_assert( offsetof( refEntityMotion_t, structSize ) == 0, "structSize offset" );
_Static_assert( offsetof( refEntityMotion_t, version ) == 4, "version offset" );
_Static_assert( offsetof( refEntityMotion_t, ownerId ) == 8, "ownerId offset" );
_Static_assert( offsetof( refEntityMotion_t, generation ) == 12, "generation offset" );
_Static_assert( offsetof( refEntityMotion_t, role ) == 16, "role offset" );
_Static_assert( offsetof( refEntityMotion_t, flags ) == 20, "flags offset" );

static int discoverMode, discoverCalls, callCount, lastTrap, badKey;
static const refEntity_t *lastEntity;
static const refEntityMotion_t *lastMotion;

static qboolean FakeGetValue( char *value, int valueSize, const char *key ) {
	discoverCalls++;
	if ( strcmp( key, "trap_R_AddRefEntityToSceneTemporal" ) ) {
		badKey = 1;
		return qfalse;
	}
	if ( discoverMode == 0 ) return qfalse;
	if ( discoverMode == 1 ) snprintf( value, (size_t)valueSize, "232" );
	else if ( discoverMode == 2 ) snprintf( value, (size_t)valueSize, "999" );
	else if ( discoverMode == 3 ) snprintf( value, (size_t)valueSize, "232junk" );
	else if ( discoverMode == 4 ) snprintf( value, (size_t)valueSize, "0232" );
	else snprintf( value, (size_t)valueSize, "+232" );
	return qtrue;
}

static void FakeCall( int trap, const refEntity_t *entity,
		const refEntityMotion_t *motion ) {
	callCount++;
	lastTrap = trap;
	lastEntity = entity;
	lastMotion = motion;
}

static void ResetCalls( void ) {
	discoverCalls = callCount = badKey = 0;
	lastTrap = 0;
	lastEntity = NULL;
	lastMotion = NULL;
}

int main( void ) {
	refEntity_t entity;
	refEntityMotion_t motion = {
		sizeof( refEntityMotion_t ), REF_ENTITY_MOTION_VERSION,
		42u, 7u, 3u, 0u
	};
	cgTemporalEntityDispatch_t state;
	cgTemporalEntityDispatch_t before;
	refEntityMotion_t owned, ownedBefore;
	qboolean hasTemporal;

	memset( &entity, 0, sizeof( entity ) );
	CHECK( RefEntityMotion_IsValid( &motion ) );
	CHECK( RefEntityMotion_CanAppend( &entity, &motion, 0u, 1u ) );
	CHECK( !RefEntityMotion_CanAppend( &entity, &motion, 1u, 1u ) );
	memset( &owned, 0xa5, sizeof( owned ) );
	hasTemporal = qfalse;
	CHECK( RefEntityMotion_CopyOwned( &owned, &hasTemporal, &motion ) );
	CHECK( hasTemporal && !memcmp( &owned, &motion, sizeof( owned ) ) );
	RefEntityMotion_ClearOwned( &owned, &hasTemporal );
	CHECK( !hasTemporal );
	CHECK( owned.structSize == 0 && owned.version == 0 && owned.ownerId == 0
		&& owned.generation == 0 && owned.role == 0 && owned.flags == 0 );

	ResetCalls();
	discoverMode = 1;
	state.optionalTrap = CG_REFENTITY_TEMPORAL_UNKNOWN;
	state.glconfigGeneration = 1u;
	CHECK( CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( !badKey && discoverCalls == 1 && callCount == 1 && lastTrap == 232 );
	CHECK( lastEntity == &entity && lastMotion == &motion );
	CHECK( CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( discoverCalls == 1 && callCount == 2 );
	discoverMode = 0;
	CHECK( CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		2u, FakeGetValue, FakeCall ) );
	CHECK( discoverCalls == 2 && callCount == 3 && lastTrap == 20 );
	CHECK( lastEntity == &entity && lastMotion == NULL );

	ResetCalls();
	discoverMode = 0;
	state.optionalTrap = CG_REFENTITY_TEMPORAL_UNKNOWN;
	state.glconfigGeneration = 1u;
	CHECK( CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( state.optionalTrap == CG_REFENTITY_TEMPORAL_ABSENT );
	CHECK( discoverCalls == 1 && callCount == 1 && lastTrap == 20 );
	CHECK( lastEntity == &entity && lastMotion == NULL );

	ResetCalls();
	discoverMode = 2;
	state.optionalTrap = CG_REFENTITY_TEMPORAL_UNKNOWN;
	state.glconfigGeneration = 1u;
	CHECK( CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( callCount == 1 && lastTrap == 20 && lastMotion == NULL );
	for ( discoverMode = 3; discoverMode <= 5; ++discoverMode ) {
		ResetCalls();
		state.optionalTrap = CG_REFENTITY_TEMPORAL_UNKNOWN;
		state.glconfigGeneration = 1u;
		CHECK( CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
			1u, FakeGetValue, FakeCall ) );
		CHECK( callCount == 1 && lastTrap == 20 && lastMotion == NULL );
	}

	ResetCalls();
	state.optionalTrap = CG_REFENTITY_TEMPORAL_UNKNOWN;
	state.glconfigGeneration = 1u;
	before = state;
	motion.flags = 1u;
	memset( &owned, 0x3c, sizeof( owned ) );
	ownedBefore = owned;
	hasTemporal = qfalse;
	CHECK( !RefEntityMotion_CopyOwned( &owned, &hasTemporal, &motion ) );
	CHECK( !memcmp( &owned, &ownedBefore, sizeof( owned ) ) && !hasTemporal );
	CHECK( !CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( !memcmp( &state, &before, sizeof( state ) ) );
	CHECK( discoverCalls == 0 && callCount == 0 );
	motion.flags = 0u;
	motion.structSize = 20u;
	CHECK( !CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	motion.structSize = sizeof( motion );
	motion.version++;
	CHECK( !CG_TemporalEntityDispatch( &state, &entity, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( !CG_TemporalEntityDispatch( &state, NULL, &motion, 20, 232,
		1u,
		FakeGetValue, FakeCall ) );
	CHECK( discoverCalls == 0 && callCount == 0 );

	puts( "PASS optional atomic temporal entity identity contract" );
	return 0;
}
