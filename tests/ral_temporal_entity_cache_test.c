// SPDX-License-Identifier: GPL-3.0-or-later

#include "../code/renderervk/tr_temporal_entity_cache.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

static refEntityMotion_t Identity( uint32_t owner, uint32_t generation,
		uint32_t role ) {
	refEntityMotion_t id;
	memset( &id, 0, sizeof( id ) );
	id.structSize = sizeof( id );
	id.version = REF_ENTITY_MOTION_VERSION;
	id.ownerId = owner;
	id.generation = generation;
	id.role = role;
	return id;
}

static temporalEntityPose_t Pose( float x ) {
	temporalEntityPose_t p;
	memset( &p, 0, sizeof( p ) );
	p.hModel = 7;
	p.modelToken = 0x1000u;
	p.modelDataToken = 0x2000u;
	p.modelType = 2;
	p.modelTopology = 99;
	p.frame = 3;
	p.oldframe = 2;
	p.backlerp = 0.25f;
	p.origin[0] = x;
	p.axis[0] = p.axis[4] = p.axis[8] = 1.0f;
	return p;
}

static temporalCameraPose_t Camera( float x ) {
	temporalCameraPose_t c;
	memset( &c, 0, sizeof( c ) );
	c.projection[0] = c.projection[5] = c.projection[10] = c.projection[15] = 1.0f;
	c.worldModel[0] = c.worldModel[5] = c.worldModel[10] = c.worldModel[15] = 1.0f;
	c.origin[0] = x;
	c.axis[0] = c.axis[4] = c.axis[8] = 1.0f;
	return c;
}

static int TopologyCutsPrevious( temporalEntityPose_t changed ) {
	refEntityMotion_t id = Identity( 0, 1, REF_ENTITY_MOTION_ROLE_GENERAL );
	temporalEntityPoseReceipt_t receipt;
	temporalEntityPose_t first = Pose( 1.0f );
	R_TemporalEntityCacheResetAll();
	CHECK( R_TemporalEntityCacheBegin( 0, 1, 1, 1 ) );
	CHECK( R_TemporalEntityCacheRecord( 0, 1, 1, &id, &first, &receipt ) );
	CHECK( R_TemporalEntityCacheFinish( 0, 1, qtrue ) );
	CHECK( R_TemporalEntityCacheBegin( 0, 1, 1, 2 ) );
	CHECK( R_TemporalEntityCacheRecord( 0, 2, 2, &id, &changed, &receipt ) );
	CHECK( !receipt.previousValid );
	CHECK( R_TemporalEntityCacheFinish( 0, 2, qtrue ) );
	return 0;
}

static int CacheGenerationCutsPrevious( uint32_t nextTopology,
		uint32_t nextTemporal ) {
	refEntityMotion_t id = Identity( 0, 1, REF_ENTITY_MOTION_ROLE_GENERAL );
	temporalEntityPoseReceipt_t receipt;
	temporalEntityPose_t pose = Pose( 1.0f );
	R_TemporalEntityCacheResetAll();
	CHECK( R_TemporalEntityCacheBegin( 0, 3, 5, 1 ) );
	CHECK( R_TemporalEntityCacheRecord( 0, 1, 1, &id, &pose, &receipt ) );
	CHECK( R_TemporalEntityCacheFinish( 0, 1, qtrue ) );
	CHECK( R_TemporalEntityCacheBegin( 0, nextTopology, nextTemporal, 2 ) );
	CHECK( R_TemporalEntityCacheRecord( 0, 2, 2, &id, &pose, &receipt ) );
	CHECK( !receipt.previousValid );
	CHECK( R_TemporalEntityCacheFinish( 0, 2, qtrue ) );
	return 0;
}

int main( void ) {
	refEntityMotion_t id = Identity( 0, 1, REF_ENTITY_MOTION_ROLE_GENERAL );
	temporalEntityPose_t p1 = Pose( 1.0f ), p2 = Pose( 2.0f ), changed;
	temporalEntityPoseReceipt_t er, sentinel;
	temporalCameraPose_t c1 = Camera( 1.0f ), c2 = Camera( 2.0f );
	temporalCameraPoseReceipt_t cr;

	R_TemporalEntityCacheResetAll();
	CHECK( R_TemporalEntityCacheBegin( 0, 4, 8, 10 ) );
	CHECK( R_TemporalEntityCacheBegin( 0, 4, 8, 10 ) );
	CHECK( R_TemporalEntityCacheStageCamera( 0, 10, &c1, &cr ) );
	CHECK( cr.valid && !cr.previousValid && cr.previousFrameId == 0 );
	CHECK( R_TemporalEntityCacheRecord( 0, 10, 11, &id, &p1, &er ) );
	CHECK( er.valid && !er.previousValid && er.previousFrameId == 0
		&& er.identity.ownerId == 0 );
	CHECK( R_TemporalEntityCacheRecord( 0, 10, 11, &id, &p1, &er ) );
	CHECK( R_TemporalEntityCacheFinish( 0, 10, qtrue ) );

	CHECK( R_TemporalEntityCacheBegin( 0, 4, 8, 11 ) );
	CHECK( R_TemporalEntityCacheStageCamera( 0, 11, &c2, &cr ) );
	CHECK( cr.previousValid && cr.previousFrameId == 10
		&& cr.previous.origin[0] == 1.0f );
	CHECK( R_TemporalEntityCacheRecord( 0, 11, 12, &id, &p2, &er ) );
	CHECK( er.previousValid && er.previousFrameId == 10
		&& er.previous.origin[0] == 1.0f );
	CHECK( R_TemporalEntityCacheFinish( 0, 11, qfalse ) == qfalse );

	// Cancel leaves the prior committed pose/camera authoritative.
	CHECK( R_TemporalEntityCacheBegin( 0, 4, 8, 11 ) );
	CHECK( R_TemporalEntityCacheStageCamera( 0, 11, &c2, &cr ) );
	CHECK( cr.previousValid && cr.previousFrameId == 10
		&& cr.previous.origin[0] == 1.0f );
	CHECK( R_TemporalEntityCacheRecord( 0, 11, 13, &id, &p2, &er ) );
	CHECK( er.previousValid && er.previousFrameId == 10
		&& er.previous.origin[0] == 1.0f );
	CHECK( R_TemporalEntityCacheFinish( 0, 11, qtrue ) );

	// A distinct source claiming one tuple poisons the whole transaction.
	CHECK( R_TemporalEntityCacheBegin( 0, 4, 8, 12 ) );
	CHECK( R_TemporalEntityCacheRecord( 0, 12, 20, &id, &p2, &er ) );
	CHECK( !R_TemporalEntityCacheRecord( 0, 12, 21, &id, &p2, &er ) );
	CHECK( !R_TemporalEntityCacheFinish( 0, 12, qtrue ) );

	// Invalid input rejects output-atomically.
	memset( &sentinel, 0x5a, sizeof( sentinel ) );
	er = sentinel;
	id.generation = 0;
	CHECK( R_TemporalEntityCacheBegin( 1, 1, 1, 1 ) );
	CHECK( !R_TemporalEntityCacheRecord( 1, 1, 1, &id, &p1, &er ) );
	CHECK( memcmp( &er, &sentinel, sizeof( er ) ) == 0 );
	id.generation = 1;
	p1.origin[0] = NAN;
	CHECK( !R_TemporalEntityCacheRecord( 1, 1, 1, &id, &p1, &er ) );
	CHECK( memcmp( &er, &sentinel, sizeof( er ) ) == 0 );
	p1 = Pose( 1.0f );
	p1.modelTopology = 0;
	CHECK( !R_TemporalEntityCacheRecord( 1, 1, 1, &id, &p1, &er ) );
	CHECK( memcmp( &er, &sentinel, sizeof( er ) ) == 0 );
	CHECK( !R_TemporalEntityCacheFinish( 1, 1, qfalse ) );
	CHECK( R_TemporalEntityCacheBegin( 1, 1, 1, 1 ) );
	memset( &cr, 0x5a, sizeof( cr ) );
	c1.projection[2] = NAN;
	CHECK( !R_TemporalEntityCacheStageCamera( 1, 1, &c1, &cr ) );
	for ( size_t i = 0; i < sizeof( cr ); ++i ) CHECK( ((byte *)&cr)[i] == 0x5a );
	CHECK( !R_TemporalEntityCacheFinish( 1, 1, qfalse ) );

	// Every renderer topology identity component independently cuts history.
	changed = Pose( 2.0f ); changed.hModel++; CHECK( !TopologyCutsPrevious( changed ) );
	changed = Pose( 2.0f ); changed.modelToken++; CHECK( !TopologyCutsPrevious( changed ) );
	changed = Pose( 2.0f ); changed.modelDataToken++; CHECK( !TopologyCutsPrevious( changed ) );
	changed = Pose( 2.0f ); changed.modelType++; CHECK( !TopologyCutsPrevious( changed ) );
	changed = Pose( 2.0f ); changed.modelTopology++; CHECK( !TopologyCutsPrevious( changed ) );
	changed = Pose( 2.0f ); changed.modelAllocationGeneration = 1; CHECK( !TopologyCutsPrevious( changed ) );
	changed = Pose( 2.0f ); changed.modelContentDigest = 1; CHECK( !TopologyCutsPrevious( changed ) );
	CHECK( !CacheGenerationCutsPrevious( 4, 5 ) );
	CHECK( !CacheGenerationCutsPrevious( 3, 6 ) );

	// Frame gaps, cache-generation changes and entity-generation changes cut history.
	R_TemporalEntityCacheResetAll();
	CHECK( R_TemporalEntityCacheBegin( 0, 1, 1, 1 ) );
	CHECK( R_TemporalEntityCacheRecord( 0, 1, 1, &id, &p2, &er ) );
	CHECK( R_TemporalEntityCacheFinish( 0, 1, qtrue ) );
	CHECK( R_TemporalEntityCacheBegin( 0, 1, 2, 3 ) );
	id.generation = 2;
	CHECK( R_TemporalEntityCacheRecord( 0, 3, 2, &id, &p2, &er ) );
	CHECK( !er.previousValid && er.previousFrameId == 0 );
	CHECK( R_TemporalEntityCacheFinish( 0, 3, qtrue ) );

	puts( "ral temporal entity cache contract: ok" );
	return 0;
}
