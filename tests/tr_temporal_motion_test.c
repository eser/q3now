// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/renderervk/tr_temporal_motion.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

static qboolean Near( float a, float b ) {
	return fabsf( a - b ) <= 1.0e-5f;
}

static void Identity( float matrix[16] ) {
	memset( matrix, 0, 16 * sizeof( *matrix ) );
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

static void Perspective( float matrix[16] ) {
	memset( matrix, 0, 16 * sizeof( *matrix ) );
	matrix[0] = matrix[5] = 1.0f;
	matrix[10] = 1.0f;
	matrix[11] = -1.0f;
}

static void CameraPose( temporalCameraPose_t *pose ) {
	memset( pose, 0, sizeof( *pose ) );
	Perspective( pose->projection );
	Identity( pose->worldModel );
	pose->axis[0] = pose->axis[4] = pose->axis[8] = 1.0f;
}

static temporalCameraPoseReceipt_t CameraReceipt( void ) {
	temporalCameraPoseReceipt_t receipt;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.frameId = 2;
	receipt.previousFrameId = 1;
	CameraPose( &receipt.current );
	CameraPose( &receipt.previous );
	receipt.valid = qtrue;
	receipt.previousValid = qtrue;
	return receipt;
}

static temporalEntityPose_t EntityPose( void ) {
	temporalEntityPose_t pose;
	memset( &pose, 0, sizeof( pose ) );
	pose.hModel = 7;
	pose.modelToken = (uintptr_t)0x1000u;
	pose.modelDataToken = (uintptr_t)0x2000u;
	pose.modelType = 1; // MOD_BRUSH, pinned against tr_local.h by source policy.
	pose.modelTopology = 99;
	pose.frame = 3;
	pose.oldframe = 2;
	pose.backlerp = 0.25f;
	pose.axis[0] = pose.axis[4] = pose.axis[8] = 1.0f;
	return pose;
}

static temporalEntityPoseReceipt_t EntityReceipt( void ) {
	temporalEntityPoseReceipt_t receipt;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.frameId = 2;
	receipt.previousFrameId = 1;
	receipt.identity.structSize = sizeof( receipt.identity );
	receipt.identity.version = REF_ENTITY_MOTION_VERSION;
	receipt.identity.ownerId = 0;
	receipt.identity.generation = 1;
	receipt.identity.role = REF_ENTITY_MOTION_ROLE_GENERAL;
	receipt.current = EntityPose();
	receipt.previous = EntityPose();
	receipt.valid = qtrue;
	receipt.previousValid = qtrue;
	return receipt;
}

static qboolean AllBytes( const void *data, size_t size, byte value ) {
	const byte *bytes = (const byte *)data;
	for ( size_t i = 0; i < size; ++i ) if ( bytes[i] != value ) return qfalse;
	return qtrue;
}

static int ExpectMatrixReject( temporalMotionGeometry_t geometry,
		const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity ) {
	temporalMotionMatrices_t output;
	memset( &output, 0x5a, sizeof( output ) );
	CHECK( !R_TemporalMotionBuildMatrices( geometry, camera, entity, &output ) );
	CHECK( AllBytes( &output, sizeof( output ), 0x5a ) );
	return 0;
}

static temporalMotionDrawFacts_t WritableFacts(
		temporalMotionGeometry_t geometry ) {
	temporalMotionDrawFacts_t facts;
	memset( &facts, 0, sizeof( facts ) );
	facts.geometry = geometry;
	facts.visible = qtrue;
	facts.opaque = qtrue;
	facts.depthAuthoritative = qtrue;
	return facts;
}

static int TestClassification( void ) {
	temporalMotionDrawFacts_t facts;
	CHECK( R_TemporalMotionClassify( NULL ) == TEMPORAL_MOTION_PRESERVE );
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_WRITE_VALID );
	facts.geometry = TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_WRITE_VALID );

	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_UNSUPPORTED );
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_INVALIDATE_OPAQUE );
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	facts.depthAuthoritative = qfalse;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE );
#define EXPECT_INVALIDATE(field) do { \
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC ); \
	facts.field = qtrue; \
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_INVALIDATE_OPAQUE ); \
} while (0)
	EXPECT_INVALIDATE( vertexDeformed );
	EXPECT_INVALIDATE( sky );
#undef EXPECT_INVALIDATE

	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	facts.alphaTested = qtrue;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_DEFER_ATEST );

#define EXPECT_PRESERVE(field) do { \
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC ); \
	facts.field = qtrue; \
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE ); \
	facts.alphaTested = qtrue; \
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE ); \
} while (0)
	EXPECT_PRESERVE( blended );
	EXPECT_PRESERVE( transparent );
	EXPECT_PRESERVE( decal );
	EXPECT_PRESERVE( additive );
	EXPECT_PRESERVE( ui );
	EXPECT_PRESERVE( polygonOffset );
	EXPECT_PRESERVE( depthHack );
	EXPECT_PRESERVE( crosshair );
#undef EXPECT_PRESERVE
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	facts.visible = qfalse;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE );
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	facts.opaque = qfalse;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE );
	facts.alphaTested = qtrue;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE );
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	facts.depthAuthoritative = qfalse;
	facts.alphaTested = qtrue;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE );

	// Blending takes precedence over the ATEST-only deferred lane.
	facts = WritableFacts( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC );
	facts.alphaTested = facts.blended = qtrue;
	CHECK( R_TemporalMotionClassify( &facts ) == TEMPORAL_MOTION_PRESERVE );
	return 0;
}

static int TestWorldStaticOracle( void ) {
	temporalCameraPoseReceipt_t camera = CameraReceipt();
	temporalMotionMatrices_t matrices;
	temporalMotionSample_t sample;
	const float point[3] = { 0.0f, 0.0f, -2.0f };
	camera.current.worldModel[12] = 1.0f;
	camera.current.worldModel[13] = -1.0f;
	CHECK( R_TemporalMotionBuildMatrices( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC,
		&camera, NULL, &matrices ) );
	CHECK( Near( matrices.currentMvp[0], 1.0f ) );
	CHECK( Near( matrices.currentMvp[5], -1.0f ) );
	CHECK( Near( matrices.currentMvp[11], -1.0f ) );
	CHECK( Near( matrices.currentMvp[12], 1.0f ) );
	CHECK( Near( matrices.currentMvp[13], 1.0f ) );
	CHECK( R_TemporalMotionEvaluatePoint( &matrices, point, &sample ) );
	CHECK( Near( sample.currentUv[0], 0.75f ) );
	CHECK( Near( sample.currentUv[1], 0.75f ) );
	CHECK( Near( sample.previousUv[0], 0.5f ) );
	CHECK( Near( sample.previousUv[1], 0.5f ) );
	CHECK( Near( sample.velocity[0], 0.25f ) );
	CHECK( Near( sample.velocity[1], 0.25f ) );

	camera = CameraReceipt();
	CHECK( R_TemporalMotionBuildMatrices( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC,
		&camera, NULL, &matrices ) );
	CHECK( R_TemporalMotionEvaluatePoint( &matrices, point, &sample ) );
	CHECK( Near( sample.currentUv[0], 0.5f ) && Near( sample.currentUv[1], 0.5f ) );
	CHECK( Near( sample.previousUv[0], 0.5f ) && Near( sample.previousUv[1], 0.5f ) );
	CHECK( sample.velocity[0] == 0.0f && !signbit( sample.velocity[0] ) );
	CHECK( sample.velocity[1] == 0.0f && !signbit( sample.velocity[1] ) );
	return 0;
}

static int TestMatrixOrderAndProjection( void ) {
	temporalCameraPoseReceipt_t camera = CameraReceipt();
	temporalEntityPoseReceipt_t entity = EntityReceipt();
	temporalCameraPoseReceipt_t cameraSnapshot;
	temporalEntityPoseReceipt_t entitySnapshot;
	temporalMotionMatrices_t matrices;
	temporalMotionMatrices_t matricesSnapshot;
	temporalMotionSample_t sample;
	const float point[3] = { 1.0f, 2.0f, -2.0f };
	float pointSnapshot[3];

	// Non-commuting V/M transforms, asymmetric projections and distinct w.
	Perspective( camera.current.projection );
	camera.current.projection[0] = 2.0f;
	camera.current.projection[5] = 4.0f;
	camera.current.projection[8] = 0.25f;
	camera.current.projection[9] = -0.5f;
	Identity( camera.current.worldModel );
	camera.current.worldModel[0] = 2.0f;
	camera.current.worldModel[12] = 10.0f;
	entity.current.origin[0] = 3.0f;
	entity.current.origin[1] = 1.0f;

	Perspective( camera.previous.projection );
	camera.previous.projection[0] = 1.0f;
	camera.previous.projection[5] = 2.0f;
	camera.previous.projection[8] = -0.25f;
	camera.previous.projection[9] = 0.75f;
	Identity( camera.previous.worldModel );
	camera.previous.worldModel[0] = 3.0f;
	camera.previous.worldModel[12] = -4.0f;
	entity.previous.origin[0] = 1.0f;
	entity.previous.origin[1] = -1.0f;
	entity.previous.origin[2] = -2.0f;
	cameraSnapshot = camera;
	entitySnapshot = entity;

	CHECK( R_TemporalMotionBuildMatrices(
		TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID, &camera, &entity, &matrices ) );
	CHECK( memcmp( &camera, &cameraSnapshot, sizeof( camera ) ) == 0 );
	CHECK( memcmp( &entity, &entitySnapshot, sizeof( entity ) ) == 0 );
	matricesSnapshot = matrices;
	memcpy( pointSnapshot, point, sizeof( point ) );
	CHECK( R_TemporalMotionEvaluatePoint( &matrices, point, &sample ) );
	CHECK( memcmp( &matrices, &matricesSnapshot, sizeof( matrices ) ) == 0 );
	CHECK( memcmp( point, pointSnapshot, sizeof( point ) ) == 0 );
	CHECK( Near( sample.currentClip[0], 35.5f ) );
	CHECK( Near( sample.currentClip[1], -11.0f ) );
	CHECK( Near( sample.currentClip[3], 2.0f ) );
	CHECK( Near( sample.previousClip[0], 3.0f ) );
	CHECK( Near( sample.previousClip[1], -5.0f ) );
	CHECK( Near( sample.previousClip[3], 4.0f ) );
	CHECK( Near( sample.currentUv[0], 9.375f ) );
	CHECK( Near( sample.currentUv[1], -2.25f ) );
	CHECK( Near( sample.previousUv[0], 0.875f ) );
	CHECK( Near( sample.previousUv[1], -0.125f ) );
	CHECK( Near( sample.velocity[0], 8.5f ) );
	CHECK( Near( sample.velocity[1], -2.125f ) );
	CHECK( Near( sample.currentUv[0] - sample.velocity[0], sample.previousUv[0] ) );
	CHECK( Near( sample.currentUv[1] - sample.velocity[1], sample.previousUv[1] ) );
	return 0;
}

static int TestTopLeftSignAndSignedZero( void ) {
	temporalCameraPoseReceipt_t camera = CameraReceipt();
	temporalEntityPoseReceipt_t entity = EntityReceipt();
	temporalMotionMatrices_t matrices;
	temporalMotionSample_t sample;
	const float point[3] = { 0.0f, 0.0f, -2.0f };
	entity.current.origin[0] = 1.0f;  // right
	entity.current.origin[1] = -1.0f; // down after the single Vulkan Y flip
	CHECK( R_TemporalMotionBuildMatrices(
		TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID, &camera, &entity, &matrices ) );
	CHECK( R_TemporalMotionEvaluatePoint( &matrices, point, &sample ) );
	CHECK( Near( sample.currentUv[0], 0.75f ) );
	CHECK( Near( sample.currentUv[1], 0.75f ) );
	CHECK( Near( sample.previousUv[0], 0.5f ) );
	CHECK( Near( sample.previousUv[1], 0.5f ) );
	CHECK( Near( sample.velocity[0], 0.25f ) );
	CHECK( Near( sample.velocity[1], 0.25f ) );

	entity.current = entity.previous;
	entity.current.axis[1] = -0.0f;
	CHECK( R_TemporalMotionBuildMatrices(
		TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID, &camera, &entity, &matrices ) );
	for ( size_t i = 0; i < 16; ++i ) {
		if ( matrices.currentMvp[i] == 0.0f ) CHECK( !signbit( matrices.currentMvp[i] ) );
		if ( matrices.previousMvp[i] == 0.0f ) CHECK( !signbit( matrices.previousMvp[i] ) );
	}
	CHECK( R_TemporalMotionEvaluatePoint( &matrices, point, &sample ) );
	for ( size_t i = 0; i < 2; ++i ) {
		if ( sample.velocity[i] == 0.0f ) CHECK( !signbit( sample.velocity[i] ) );
	}
	return 0;
}

static int TestReceiptRejection( void ) {
	temporalCameraPoseReceipt_t camera = CameraReceipt(), changedCamera;
	temporalEntityPoseReceipt_t entity = EntityReceipt(), changed;
	temporalMotionMatrices_t matrices;

	CHECK( ExpectMatrixReject( TEMPORAL_MOTION_GEOMETRY_UNSUPPORTED,
		&camera, &entity ) == 0 );
	CHECK( ExpectMatrixReject( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC,
		&camera, &entity ) == 0 );
	CHECK( ExpectMatrixReject( TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID,
		&camera, NULL ) == 0 );
	CHECK( R_TemporalMotionBuildMatrices( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC,
		&camera, NULL, &matrices ) );

#define REJECT_CAMERA(statement) do { \
	changedCamera = camera; statement; \
	CHECK( ExpectMatrixReject( TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC, \
		&changedCamera, NULL ) == 0 ); \
} while (0)
	REJECT_CAMERA( changedCamera.valid = qfalse );
	REJECT_CAMERA( changedCamera.previousValid = qfalse );
	REJECT_CAMERA( changedCamera.frameId = 0 );
	REJECT_CAMERA( changedCamera.previousFrameId = 0 );
	REJECT_CAMERA( changedCamera.frameId = 3 );
	REJECT_CAMERA( changedCamera.current.projection[8] = NAN );
	REJECT_CAMERA( changedCamera.previous.projection[9] = INFINITY );
	REJECT_CAMERA( changedCamera.current.worldModel[0] = FLT_MAX;
		changedCamera.current.projection[0] = FLT_MAX );
#undef REJECT_CAMERA

#define REJECT_ENTITY(statement) do { \
	changed = entity; statement; \
	CHECK( ExpectMatrixReject( TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID, \
		&camera, &changed ) == 0 ); \
} while (0)
	REJECT_ENTITY( changed.valid = qfalse );
	REJECT_ENTITY( changed.previousValid = qfalse );
	REJECT_ENTITY( changed.frameId++ );
	REJECT_ENTITY( changed.previousFrameId++ );
	REJECT_ENTITY( changed.identity.structSize-- );
	REJECT_ENTITY( changed.identity.version++ );
	REJECT_ENTITY( changed.identity.flags = 1 );
	REJECT_ENTITY( changed.identity.generation = 0 );
	REJECT_ENTITY( changed.identity.ownerId = MAX_GENTITIES );
	REJECT_ENTITY( changed.identity.role = REF_ENTITY_MOTION_ROLE_NONE );
	REJECT_ENTITY( changed.identity.role = REF_ENTITY_MOTION_ROLE_COUNT );
	REJECT_ENTITY( changed.current.hModel++ );
	REJECT_ENTITY( changed.current.modelToken++ );
	REJECT_ENTITY( changed.current.modelDataToken++ );
	REJECT_ENTITY( changed.current.modelType++ );
	REJECT_ENTITY( changed.current.modelTopology++ );
	REJECT_ENTITY( changed.current.origin[0] = NAN );
	REJECT_ENTITY( changed.previous.axis[0] = INFINITY );
	REJECT_ENTITY( changed.current.nonNormalizedAxes = 2 );
	REJECT_ENTITY( changed.current.modelType = 2; changed.previous.modelType = 2 );
#undef REJECT_ENTITY

	// Transform-only motion and non-normalized axis state changes remain rigid.
	changed = entity;
	changed.current.origin[0] = 4.0f;
	changed.current.axis[0] = 2.0f;
	changed.current.nonNormalizedAxes = 1;
	changed.current.frame++;
	changed.current.oldframe++;
	changed.current.backlerp = 0.5f;
	CHECK( R_TemporalMotionBuildMatrices(
		TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID, &camera, &changed, &matrices ) );
	return 0;
}

static int TestPointRejectionAndBounds( void ) {
	temporalMotionMatrices_t matrices, changed;
	temporalMotionSample_t output;
	const float point[3] = { 4.0f, 0.0f, 0.0f };
	Identity( matrices.currentMvp );
	Identity( matrices.previousMvp );
	CHECK( R_TemporalMotionEvaluatePoint( &matrices, point, &output ) );
	CHECK( output.currentUv[0] > 1.0f ); // clip-crossing vertices stay eligible.

#define REJECT_POINT(statement) do { \
	changed = matrices; statement; memset( &output, 0x5a, sizeof( output ) ); \
	CHECK( !R_TemporalMotionEvaluatePoint( &changed, point, &output ) ); \
	CHECK( AllBytes( &output, sizeof( output ), 0x5a ) ); \
} while (0)
	REJECT_POINT( changed.currentMvp[15] = 0.0f );
	REJECT_POINT( changed.currentMvp[15] = -1.0f );
	REJECT_POINT( changed.currentMvp[15] = 0.5e-6f );
	REJECT_POINT( changed.previousMvp[15] = 0.5e-6f );
	REJECT_POINT( changed.currentMvp[0] = NAN );
	REJECT_POINT( changed.previousMvp[0] = INFINITY );
#undef REJECT_POINT
	{
		float badPoint[3] = { NAN, 0.0f, 0.0f };
		memset( &output, 0x5a, sizeof( output ) );
		CHECK( !R_TemporalMotionEvaluatePoint( &matrices, badPoint, &output ) );
		CHECK( AllBytes( &output, sizeof( output ), 0x5a ) );
	}
	return 0;
}

int main( void ) {
	CHECK( TestClassification() == 0 );
	CHECK( TestWorldStaticOracle() == 0 );
	CHECK( TestMatrixOrderAndProjection() == 0 );
	CHECK( TestTopLeftSignAndSignedZero() == 0 );
	CHECK( TestReceiptRejection() == 0 );
	CHECK( TestPointRejectionAndBounds() == 0 );
	puts( "temporal motion policy/math contract: ok" );
	return 0;
}
