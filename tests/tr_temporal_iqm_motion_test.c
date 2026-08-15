// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/renderervk/tr_temporal_iqm_motion.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

static qboolean VerifierPeekCommit(
		temporalIqmSequenceVerifier_t *verifier,
		const temporalIqmDrawFacts_t *observed,
		temporalIqmSequenceEntry_t *outExpected ) {
	temporalIqmSequenceEntry_t candidate;
	if ( !outExpected || !R_TemporalIqmSequenceVerifierPeek(
			verifier, observed, &candidate )
			|| !R_TemporalIqmSequenceVerifierCommit( verifier, &candidate ) )
		return qfalse;
	*outExpected = candidate;
	return qtrue;
}

static qboolean AllBytes( const void *data, size_t size, unsigned char value ) {
	const unsigned char *bytes = (const unsigned char *)data;
	for ( size_t i = 0; i < size; ++i ) if ( bytes[i] != value ) return qfalse;
	return qtrue;
}

static void Identity4( float out[16] ) {
	memset( out, 0, 16u * sizeof( float ) );
	out[0] = out[5] = out[10] = out[15] = 1.0f;
}

static void Identity34( float out[12] ) {
	memset( out, 0, 12u * sizeof( float ) );
	out[0] = out[5] = out[10] = 1.0f;
}

static void LegacyMul34( const float *a, const float *b, float *out ) {
	out[0]=a[0]*b[0]+a[1]*b[4]+a[2]*b[8];
	out[1]=a[0]*b[1]+a[1]*b[5]+a[2]*b[9];
	out[2]=a[0]*b[2]+a[1]*b[6]+a[2]*b[10];
	out[3]=a[0]*b[3]+a[1]*b[7]+a[2]*b[11]+a[3];
	out[4]=a[4]*b[0]+a[5]*b[4]+a[6]*b[8];
	out[5]=a[4]*b[1]+a[5]*b[5]+a[6]*b[9];
	out[6]=a[4]*b[2]+a[5]*b[6]+a[6]*b[10];
	out[7]=a[4]*b[3]+a[5]*b[7]+a[6]*b[11]+a[7];
	out[8]=a[8]*b[0]+a[9]*b[4]+a[10]*b[8];
	out[9]=a[8]*b[1]+a[9]*b[5]+a[10]*b[9];
	out[10]=a[8]*b[2]+a[9]*b[6]+a[10]*b[10];
	out[11]=a[8]*b[3]+a[9]*b[7]+a[10]*b[11]+a[11];
}

static void LegacyJoint( const float r[4], const float s[3],
		const float t[3], float m[12] ) {
	float xx=2.0f*r[0]*r[0], yy=2.0f*r[1]*r[1], zz=2.0f*r[2]*r[2];
	float xy=2.0f*r[0]*r[1], xz=2.0f*r[0]*r[2], yz=2.0f*r[1]*r[2];
	float wx=2.0f*r[3]*r[0], wy=2.0f*r[3]*r[1], wz=2.0f*r[3]*r[2];
	m[0]=s[0]*(1.0f-(yy+zz)); m[1]=s[0]*(xy-wz);
	m[2]=s[0]*(xz+wy); m[3]=t[0]; m[4]=s[1]*(xy+wz);
	m[5]=s[1]*(1.0f-(xx+zz)); m[6]=s[1]*(yz-wx); m[7]=t[1];
	m[8]=s[2]*(xz-wy); m[9]=s[2]*(yz+wx);
	m[10]=s[2]*(1.0f-(xx+yy)); m[11]=t[2];
}

static void LegacySlerp( const float from[4], const float input[4],
		float fraction, float out[4] ) {
	float to[4], angle, sinAngle, backlerp, lerp;
	float cosine=from[0]*input[0]+from[1]*input[1]+from[2]*input[2]+from[3]*input[3];
	if ( cosine < 0.0f ) {
		cosine=-cosine; for ( int i=0; i<4; ++i ) to[i]=-input[i];
	} else memcpy( to, input, sizeof( to ) );
	if ( cosine < 0.999999f ) {
		angle=acosf(cosine); sinAngle=sinf(angle);
		backlerp=sinf((1.0f-fraction)*angle)/sinAngle;
		lerp=sinf(fraction*angle)/sinAngle;
	} else { backlerp=1.0f-fraction; lerp=fraction; }
	for ( int i=0; i<4; ++i ) out[i]=from[i]*backlerp+to[i]*lerp;
}

static void LegacyPalette( const temporalIqmModelView_t *model, int frame,
		int oldframe, float backlerp, float out[TEMPORAL_IQM_BONE_ROWS][4] ) {
	temporalIqmTransform_t rel[3];
	float lerp=1.0f-backlerp;
	memset( out, 0, TEMPORAL_IQM_BONE_ROWS*4u*sizeof(float) );
	for ( uint32_t i=0; i<model->numPoses; ++i ) {
		const temporalIqmTransform_t *p=&model->poses[(uint32_t)frame*model->numPoses+i];
		const temporalIqmTransform_t *o=&model->poses[(uint32_t)oldframe*model->numPoses+i];
		if ( frame == oldframe ) rel[i]=*p;
		else {
			for ( int j=0; j<3; ++j ) {
				rel[i].translate[j]=o->translate[j]*backlerp+p->translate[j]*lerp;
				rel[i].scale[j]=o->scale[j]*backlerp+p->scale[j]*lerp;
			}
			LegacySlerp(o->rotate,p->rotate,lerp,rel[i].rotate);
		}
	}
	for ( uint32_t i=0; i<model->numPoses; ++i ) {
		float a[12], b[12]; float *dst=&out[i*3u][0];
		LegacyJoint(rel[i].rotate,rel[i].scale,rel[i].translate,a);
		if ( model->jointParents[i] >= 0 ) {
			uint32_t p=(uint32_t)model->jointParents[i];
			LegacyMul34(&model->bindJoints[p*12u],a,b);
			LegacyMul34(b,&model->inverseBindJoints[i*12u],a);
			LegacyMul34(&out[p*3u][0],a,dst);
		} else LegacyMul34(a,&model->inverseBindJoints[i*12u],dst);
	}
}

static temporalEntityPose_t Pose( int frame, float x ) {
	temporalEntityPose_t pose;
	memset( &pose, 0, sizeof( pose ) );
	pose.hModel = 7;
	pose.modelToken = (uintptr_t)0x1000u;
	pose.modelDataToken = (uintptr_t)0x2000u;
	pose.modelType = 4u;
	pose.modelTopology = 9u;
	pose.modelAllocationGeneration = 2u;
	pose.modelContentDigest = 0x1234u;
	pose.frame = frame;
	pose.oldframe = frame;
	pose.origin[0] = x;
	pose.axis[0] = pose.axis[4] = pose.axis[8] = 1.0f;
	return pose;
}

static refEntityMotion_t MotionIdentity( uint32_t owner ) {
	refEntityMotion_t identity;
	memset( &identity, 0, sizeof( identity ) );
	identity.structSize = sizeof( identity );
	identity.version = REF_ENTITY_MOTION_VERSION;
	identity.ownerId = owner;
	identity.generation = 3;
	identity.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	return identity;
}

static int TestCanonicalMvp( void ) {
	float modelView[16], projection[16], direct[16], entityMvp[16], untouched[16];
	temporalEntityPose_t pose = Pose( 1, 2.0f );
	Identity4( modelView );
	modelView[12] = 2.0f;
	Identity4( projection );
	projection[0] = 3.0f;
	projection[5] = 5.0f;
	memset( untouched, 0x5a, sizeof( untouched ) );
	CHECK( R_TemporalMotionBuildCanonicalMvp( modelView, projection, direct ) );
	CHECK( direct[0] == 3.0f && direct[5] == -5.0f && direct[12] == 6.0f );
	CHECK( R_TemporalMotionBuildCanonicalEntityMvp( projection, (float[16]){
		1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }, &pose, entityMvp ) );
	CHECK( memcmp( direct, entityMvp, sizeof( direct ) ) == 0 );
	projection[5] = NAN;
	CHECK( !R_TemporalMotionBuildCanonicalMvp( modelView, projection, untouched ) );
	CHECK( AllBytes( untouched, sizeof( untouched ), 0x5a ) );
	return 0;
}

static int TestInfluenceDecode( void ) {
	uint8_t indices[4], byteWeights[4] = { 255, 0, 0, 0 };
	float weights[4], floatWeights[4] = { 0.5f, 0.5f, 0.0f, 0.0f };
	unsigned char unalignedIndices[1u + 4u * sizeof( int32_t )];
	unsigned char unalignedWeights[1u + 4u * sizeof( float )];
	int32_t ints[4] = { 1, 2, 9999, -77 };
	memcpy( unalignedIndices + 1, ints, sizeof( ints ) );
	memcpy( unalignedWeights + 1, floatWeights, sizeof( floatWeights ) );
	CHECK( R_TemporalIqmDecodeInfluence( TEMPORAL_IQM_SCALAR_INT,
		unalignedIndices + 1, TEMPORAL_IQM_SCALAR_FLOAT,
		unalignedWeights + 1, 0, 3, indices, weights ) );
	CHECK( indices[0] == 1 && indices[1] == 2
		&& indices[2] == 0 && indices[3] == 0 );
	CHECK( weights[0] == 0.5f && weights[1] == 0.5f );
	ints[0] = -1;
	memcpy( unalignedIndices + 1, ints, sizeof( ints ) );
	CHECK( !R_TemporalIqmDecodeInfluence( TEMPORAL_IQM_SCALAR_INT,
		unalignedIndices + 1, TEMPORAL_IQM_SCALAR_FLOAT,
		unalignedWeights + 1, 0, 3, indices, weights ) );
	ints[0] = 1; ints[1] = 256;
	memcpy( unalignedIndices + 1, ints, sizeof( ints ) );
	CHECK( !R_TemporalIqmDecodeInfluence( TEMPORAL_IQM_SCALAR_INT,
		unalignedIndices + 1, TEMPORAL_IQM_SCALAR_FLOAT,
		unalignedWeights + 1, 0, 3, indices, weights ) );
	{
		uint8_t byteIndices[4] = { 1, 255, 255, 255 };
		CHECK( R_TemporalIqmDecodeInfluence( TEMPORAL_IQM_SCALAR_UBYTE,
			byteIndices, TEMPORAL_IQM_SCALAR_UBYTE, byteWeights,
			0, 3, indices, weights ) );
		CHECK( indices[0] == 1 && indices[1] == 0 && indices[2] == 0
			&& indices[3] == 0 );
		byteWeights[0] = 0;
		CHECK( !R_TemporalIqmDecodeInfluence( TEMPORAL_IQM_SCALAR_UBYTE,
			byteIndices, TEMPORAL_IQM_SCALAR_UBYTE, byteWeights,
			0, 3, indices, weights ) );
	}
	return 0;
}

static int TestRecord( void ) {
	int32_t parent = -1;
	float bind[12], inverse[12], jittered[16];
	temporalIqmTransform_t poses[2];
	temporalIqmModelView_t model;
	temporalCameraPoseReceipt_t camera;
	temporalEntityPoseReceipt_t entity;
	temporalIqmGpuRecord_t record, snapshot;
	qboolean previousValid = qfalse;
	Identity34( bind ); Identity34( inverse ); Identity4( jittered );
	memset( poses, 0, sizeof( poses ) );
	for ( int i = 0; i < 2; ++i ) {
		poses[i].rotate[3] = 1.0f;
		poses[i].scale[0] = poses[i].scale[1] = poses[i].scale[2] = 1.0f;
	}
	poses[1].translate[0] = 1.0f;
	memset( &model, 0, sizeof( model ) );
	model.modelDataToken = (uintptr_t)0x2000u;
	model.contentDigest = 0x1234u;
	model.topologyGeneration = 9;
	model.modelAllocationGeneration = 2;
	model.numFrames = 2;
	model.numJoints = model.numPoses = 1;
	model.jointParents = &parent;
	model.bindJoints = bind;
	model.inverseBindJoints = inverse;
	model.poses = poses;
	model.validated = qtrue;
	memset( &camera, 0, sizeof( camera ) );
	camera.frameId = 2; camera.previousFrameId = 1;
	Identity4( camera.current.projection ); Identity4( camera.previous.projection );
	Identity4( camera.current.worldModel ); Identity4( camera.previous.worldModel );
	camera.valid = camera.previousValid = qtrue;
	memset( &entity, 0, sizeof( entity ) );
	entity.frameId = 2; entity.previousFrameId = 1;
	entity.identity = MotionIdentity( 1 );
	entity.current = Pose( 1, 2.0f );
	entity.previous = Pose( 0, 1.0f );
	entity.valid = entity.previousValid = qtrue;
	CHECK( R_TemporalIqmBuildGpuRecord( &model, &camera, &entity, jittered,
		&record, &previousValid ) );
	CHECK( previousValid == qtrue );
	CHECK( record.currentBones[0][3] == 1.0f );
	CHECK( record.previousBones[0][3] == 0.0f );
	CHECK( record.rasterMvp[5] == -1.0f );
	CHECK( record.rasterMvp[12] == 2.0f );
	CHECK( record.temporalPreviousMvp[12] == 1.0f );
	for ( uint32_t i = 3; i < TEMPORAL_IQM_BONE_ROWS; ++i ) {
		CHECK( record.currentBones[i][0] == 0.0f );
		CHECK( record.previousBones[i][3] == 0.0f );
	}
	entity.previousValid = qfalse;
	CHECK( R_TemporalIqmBuildGpuRecord( &model, &camera, &entity, jittered,
		&record, &previousValid ) );
	CHECK( previousValid == qfalse );
	CHECK( memcmp( record.currentBones, record.previousBones,
		sizeof( record.currentBones ) ) == 0 );
	CHECK( memcmp( record.temporalCurrentMvp, record.temporalPreviousMvp,
		sizeof( record.temporalCurrentMvp ) ) == 0 );
	// A malformed previous frame is a conservative invalidate, never a current
	// record failure. A malformed current frame remains output-atomic failure.
	entity.previousValid = qtrue;
	entity.previous.frame = 99;
	CHECK( R_TemporalIqmBuildGpuRecord( &model, &camera, &entity, jittered,
		&record, &previousValid ) );
	CHECK( previousValid == qfalse );
	entity.current.frame = 99;
	snapshot = record;
	CHECK( !R_TemporalIqmBuildGpuRecord( &model, &camera, &entity, jittered,
		&record, &previousValid ) );
	CHECK( memcmp( &record, &snapshot, sizeof( record ) ) == 0 );
	entity.current.frame = 1;
	entity.previous.frame = 0;
	snapshot = record;
	model.numJoints = TEMPORAL_IQM_MAX_JOINTS + 1u;
	CHECK( !R_TemporalIqmBuildGpuRecord( &model, &camera, &entity, jittered,
		&record, &previousValid ) );
	CHECK( memcmp( &record, &snapshot, sizeof( record ) ) == 0 );
	model.numJoints = model.numPoses = 1;
	model.numFrames = UINT32_MAX;
	model.numJoints = model.numPoses = TEMPORAL_IQM_MAX_JOINTS;
	CHECK( !R_TemporalIqmBuildGpuRecord( &model, &camera, &entity, jittered,
		&record, &previousValid ) );
	return 0;
}

static int TestNonUniformPalette( void ) {
	int32_t parent = -1;
	float bind[12], inverse[12], bones[TEMPORAL_IQM_BONE_ROWS][4];
	temporalIqmTransform_t pose;
	temporalIqmModelView_t model;
	temporalEntityPose_t entity = Pose( 0, 0.0f );
	Identity34( bind ); Identity34( inverse );
	memset( &pose, 0, sizeof( pose ) );
	pose.rotate[3] = 1.0f;
	pose.scale[0] = 2.0f; pose.scale[1] = 3.0f; pose.scale[2] = 4.0f;
	memset( &model, 0, sizeof( model ) );
	model.modelDataToken = (uintptr_t)0x2000u;
	model.contentDigest = 0x1234u;
	model.topologyGeneration = 9;
	model.modelAllocationGeneration = 2;
	model.numFrames = model.numJoints = model.numPoses = 1;
	model.jointParents = &parent; model.bindJoints = bind;
	model.inverseBindJoints = inverse; model.poses = &pose; model.validated = qtrue;
	CHECK( R_TemporalIqmBuildPalette( &model, &entity, bones ) );
	CHECK( bones[0][0] == 2.0f && bones[0][1] == 0.0f );
	CHECK( bones[1][1] == 3.0f && bones[1][2] == 0.0f );
	CHECK( bones[2][2] == 4.0f && bones[2][3] == 0.0f );
	parent = 0;
	CHECK( !R_TemporalIqmBuildPalette( &model, &entity, bones ) );
	return 0;
}

static int TestLegacyPaletteParity( void ) {
	int32_t parents[3] = { -1, 0, 1 };
	float bind[36], inverse[36];
	temporalIqmTransform_t poses[6];
	temporalIqmModelView_t model;
	float actual[TEMPORAL_IQM_BONE_ROWS][4];
	float expected[TEMPORAL_IQM_BONE_ROWS][4];
	memset( bind, 0, sizeof( bind ) ); memset( inverse, 0, sizeof( inverse ) );
	memset( poses, 0, sizeof( poses ) ); memset( &model, 0, sizeof( model ) );
	for ( int i=0; i<3; ++i ) {
		Identity34(&bind[i*12]); Identity34(&inverse[i*12]);
		bind[i*12+0]=1.0f+0.1f*i; bind[i*12+5]=1.0f+0.2f*i;
		bind[i*12+10]=1.0f+0.3f*i; bind[i*12+3]=(float)i*0.25f;
		for ( int f=0; f<2; ++f ) {
			temporalIqmTransform_t *p=&poses[f*3+i];
			p->translate[0]=(float)(f+i)*0.3f; p->translate[1]=(float)(f-i)*0.2f;
			p->scale[0]=1.0f+0.2f*i; p->scale[1]=2.0f+0.1f*f;
			p->scale[2]=0.5f+0.3f*i; p->rotate[3]=1.0f;
		}
	}
	poses[3].rotate[3]=-1.0f; // negative-dot shortest-path branch
	poses[4].rotate[0]=0.0005f; poses[4].rotate[3]=sqrtf(1.0f-0.00000025f);
	poses[5].rotate[1]=sinf(0.3f); poses[5].rotate[3]=cosf(0.3f);
	model.modelDataToken=1; model.numFrames=2; model.numJoints=model.numPoses=3;
	model.jointParents=parents; model.bindJoints=bind; model.inverseBindJoints=inverse;
	model.poses=poses; model.validated=qtrue;
	LegacyPalette(&model,1,0,0.375f,expected);
	CHECK(R_TemporalIqmBuildPaletteTuple(&model,1,0,0.375f,actual));
	CHECK(memcmp(actual,expected,sizeof(actual))==0);
	LegacyPalette(&model,1,1,0.875f,expected);
	CHECK(R_TemporalIqmBuildPaletteTuple(&model,1,1,0.875f,actual));
	CHECK(memcmp(actual,expected,sizeof(actual))==0);
	// Ordinary palette validity is structural and independent of H5 provenance;
	// a generated animated zero scale is legal. Exact H5 pose admission still
	// rejects this zero-provenance model view.
	poses[4].scale[0]=0.0f;
	LegacyPalette(&model,1,0,0.25f,expected);
	CHECK(R_TemporalIqmBuildPaletteTuple(&model,1,0,0.25f,actual));
	CHECK(memcmp(actual,expected,sizeof(actual))==0);
	{
		temporalEntityPose_t exactPose=Pose(1,0.0f);
		CHECK(!R_TemporalIqmBuildPalette(&model,&exactPose,actual));
	}
	return 0;
}

static temporalIqmDrawFacts_t Draw( uint32_t ordinal, uint32_t entityIndex,
		temporalMotionOutcome_t outcome ) {
	temporalIqmDrawFacts_t draw;
	memset( &draw, 0, sizeof( draw ) );
	draw.ordinal = ordinal;
	draw.sourceDrawSurfOrdinal = ordinal;
	draw.entityIndex = entityIndex;
	draw.identity = MotionIdentity( entityIndex );
	draw.currentPose = Pose( 1, (float)entityIndex );
	draw.previousPose = Pose( 0, (float)entityIndex - 1.0f );
	draw.modelContentDigest = 0x1234u;
	draw.modelTopologyGeneration = 9;
	draw.modelAllocationGeneration = 2;
	draw.surfaceIndex = ordinal;
	draw.firstIndex = ordinal * 3u;
	draw.indexCount = 3;
	draw.rawVertexBuffer = (uintptr_t)( 0x10000u + entityIndex * 0x100u );
	draw.rawIndexBuffer = (uintptr_t)( 0x20000u + entityIndex * 0x100u );
	draw.ralVertexBuffer = (uintptr_t)( 0x30000u + entityIndex * 0x100u );
	draw.ralIndexBuffer = (uintptr_t)( 0x40000u + entityIndex * 0x100u );
	draw.vertexBufferBytes = 680;
	draw.indexBufferBytes = 4096;
	draw.geometryBackend = 0x45000u;
	draw.geometryGeneration = 6;
	draw.geometryAllocationGeneration = 4;
	draw.textureSlot = 12;
	draw.samplerSlot = 3;
	draw.bindless.setIdentity=0x5000;draw.bindless.samplerPoolIdentity=0x5100;
	draw.bindless.imageViewIdentity=0x5200+entityIndex;
	draw.bindless.samplerIdentity=0x5300;
	draw.bindless.imageOwnerIdentity=0x5400+entityIndex;
	draw.bindless.ordinaryDescriptorIdentity=0x5500+entityIndex;
	draw.bindless.imageOwnerGeneration=2;draw.bindless.samplerDefinitionDigest=3;
	draw.bindless.setGeneration=4;draw.bindless.samplerPoolGeneration=5;
	draw.bindless.imagePublicationGeneration=6;draw.bindless.imageTransactionGeneration=7;
	draw.bindless.samplerPublicationGeneration=8;draw.bindless.samplerTransactionGeneration=9;
	draw.bindless.imageSlotGeneration=10;draw.bindless.samplerSlotGeneration=11;
	draw.bindless.imageSlot=draw.textureSlot;draw.bindless.samplerSlot=draw.samplerSlot;
	draw.bindless.imageBinding=0;draw.bindless.samplerBinding=1;draw.bindless.ready=qtrue;
	draw.outcome = outcome;
	draw.previousValid = outcome == TEMPORAL_MOTION_WRITE_VALID ? qtrue : qfalse;
	if ( !draw.previousValid ) draw.previousPose = draw.currentPose;
	return draw;
}

static int TestSequence( void ) {
	temporalIqmSequenceAuthority_t authority = { 7, 20, 0, 1, 2 };
	temporalIqmDrawFacts_t draws[3], mutation;
	temporalIqmSequence_t sequence, untouched;
	temporalIqmSequenceVerifier_t verifier;
	temporalIqmSequenceEntry_t entry, entrySnapshot;
	draws[0] = Draw( 0, 2, TEMPORAL_MOTION_WRITE_VALID );
	draws[1] = Draw( 1, 2, TEMPORAL_MOTION_WRITE_VALID );
	draws[1].surfaceIndex = 1; draws[1].firstIndex = 3;
	draws[2] = Draw( 2, 3, TEMPORAL_MOTION_INVALIDATE_OPAQUE );
	CHECK( R_TemporalIqmSequenceBuild( &authority, draws, 3, &sequence ) );
	CHECK( sequence.ready && sequence.drawCount == 3 && sequence.entityCount == 2 );
	CHECK( sequence.entries[0].recordIndex == 0 );
	CHECK( sequence.entries[1].recordIndex == 0 );
	CHECK( sequence.entries[2].recordIndex == 1 );
	CHECK( sequence.orderedDigest != 0 );
	CHECK( R_TemporalIqmSequenceExact( &sequence, &sequence ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	CHECK( R_TemporalIqmSequenceVerifierPeek( &verifier, &draws[0], &entry ) );
	CHECK( verifier.cursor == 0 );
	CHECK( R_TemporalIqmSequenceVerifierCommit( &verifier, &entry ) );
	CHECK( verifier.cursor == 1 );
	for ( uint32_t i = 1; i < 3; ++i ) {
		CHECK( VerifierPeekCommit( &verifier, &draws[i], &entry ) );
		CHECK( entry.recordIndex == sequence.entries[i].recordIndex );
	}
	CHECK( R_TemporalIqmSequenceVerifierFinish( &verifier ) );

	memset( &untouched, 0x5a, sizeof( untouched ) );
	draws[1].currentPose.frame = 0;
	CHECK( !R_TemporalIqmSequenceBuild( &authority, draws, 3, &untouched ) );
	CHECK( AllBytes( &untouched, sizeof( untouched ), 0x5a ) );
	draws[1].currentPose.frame = 1;
	draws[1].sourceDrawSurfOrdinal = draws[0].sourceDrawSurfOrdinal;
	CHECK( !R_TemporalIqmSequenceBuild( &authority, draws, 3, &untouched ) );
	CHECK( AllBytes( &untouched, sizeof( untouched ), 0x5a ) );
	draws[1].sourceDrawSurfOrdinal = 1;
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	mutation = draws[0]; mutation.textureSlot++;
	memset( &entry, 0x5a, sizeof( entry ) ); entrySnapshot = entry;
	CHECK( !VerifierPeekCommit( &verifier, &mutation, &entry ) );
	CHECK( memcmp( &entry, &entrySnapshot, sizeof( entry ) ) == 0 );
	CHECK( !R_TemporalIqmSequenceVerifierFinish( &verifier ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	mutation=draws[0];mutation.geometryGeneration++;
	CHECK(!VerifierPeekCommit(&verifier,&mutation,&entry));
	CHECK(!R_TemporalIqmSequenceVerifierFinish(&verifier));
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	mutation=draws[0];mutation.geometryBackend++;
	CHECK(!VerifierPeekCommit(&verifier,&mutation,&entry));
	CHECK(!R_TemporalIqmSequenceVerifierFinish(&verifier));
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	CHECK( VerifierPeekCommit( &verifier, &draws[0], &entry ) );
	CHECK( VerifierPeekCommit( &verifier, &draws[1], &entry ) );
	CHECK( VerifierPeekCommit( &verifier, &draws[2], &entry ) );
	CHECK( !VerifierPeekCommit( &verifier, &draws[2], &entry ) );
	CHECK( !R_TemporalIqmSequenceVerifierFinish( &verifier ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	CHECK( VerifierPeekCommit( &verifier, &draws[0], &entry ) );
	CHECK( !R_TemporalIqmSequenceVerifierFinish( &verifier ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	CHECK( !VerifierPeekCommit( &verifier, &draws[1], &entry ) );
	CHECK( !R_TemporalIqmSequenceVerifierFinish( &verifier ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	CHECK( VerifierPeekCommit( &verifier, &draws[0], &entry ) );
	CHECK( !VerifierPeekCommit( &verifier, &draws[0], &entry ) );
	CHECK( !R_TemporalIqmSequenceVerifierFinish( &verifier ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	for ( uint32_t i = 0; i < 3; ++i )
		CHECK( VerifierPeekCommit( &verifier, &draws[i], &entry ) );
	CHECK( R_TemporalIqmSequenceVerifierFinish( &verifier ) );
	CHECK( !VerifierPeekCommit( &verifier, &draws[0], &entry ) );
	mutation = sequence.entries[0].facts;
	sequence.entries[0].facts.bindless.imageSlotGeneration++;
	CHECK( !R_TemporalIqmSequenceExact( &sequence, &sequence ) );
	sequence.entries[0].facts = mutation;
	CHECK( R_TemporalIqmSequenceExact( &sequence, &sequence ) );
	mutation = sequence.entries[0].facts;
	sequence.entries[0].facts.sourceDrawSurfOrdinal++;
	CHECK( !R_TemporalIqmSequenceExact( &sequence, &sequence ) );
	sequence.entries[0].facts = mutation;
	CHECK( R_TemporalIqmSequenceExact( &sequence, &sequence ) );
	CHECK( R_TemporalIqmSequenceVerifierBegin( &verifier, &sequence ) );
	{
		temporalIqmSequence_t replacement;
		mutation = draws[0]; draws[0].surfaceIndex++;
		CHECK( R_TemporalIqmSequenceBuild( &authority, draws, 3, &replacement ) );
		sequence = replacement;
		CHECK( !R_TemporalIqmSequenceVerifierPeek(
			&verifier, &draws[0], &entry ) && verifier.poisoned );
		draws[0] = mutation;
		CHECK( R_TemporalIqmSequenceBuild( &authority, draws, 3, &sequence ) );
	}
	mutation=draws[1];draws[1].identity.ownerId++;
	CHECK(!R_TemporalIqmSequenceBuild(&authority,draws,3,&untouched));draws[1]=mutation;
	mutation=draws[2];draws[2].entityIndex=draws[0].entityIndex;
	CHECK(!R_TemporalIqmSequenceBuild(&authority,draws,3,&untouched));draws[2]=mutation;
	mutation=draws[2];draws[2].identity=draws[0].identity;
	CHECK(!R_TemporalIqmSequenceBuild(&authority,draws,3,&untouched));draws[2]=mutation;
	mutation=draws[0];draws[0].vertexBufferBytes++;
	CHECK(!R_TemporalIqmSequenceBuild(&authority,draws,3,&untouched));draws[0]=mutation;
	mutation=draws[0];draws[0].indexBufferBytes--;
	CHECK(!R_TemporalIqmSequenceBuild(&authority,draws,3,&untouched));draws[0]=mutation;
	mutation=draws[0];draws[0].firstIndex=1024;
	CHECK(!R_TemporalIqmSequenceBuild(&authority,draws,3,&untouched));draws[0]=mutation;

	// No wrap/truncate at the exact fixed bound.
	{
		temporalIqmDrawFacts_t maxDraws[TEMPORAL_IQM_MAX_DRAWS];
		for ( uint32_t i = 0; i < TEMPORAL_IQM_MAX_DRAWS; ++i )
			maxDraws[i] = Draw( i, i, TEMPORAL_MOTION_WRITE_VALID );
		CHECK( R_TemporalIqmSequenceBuild( &authority, maxDraws,
			TEMPORAL_IQM_MAX_DRAWS, &sequence ) );
		CHECK( sequence.entityCount == TEMPORAL_IQM_MAX_RECORDS );
		CHECK( !R_TemporalIqmSequenceBuild( &authority, maxDraws,
			TEMPORAL_IQM_MAX_DRAWS + 1u, &untouched ) );
	}
	return 0;
}

static int TestProductAdmission( void ) {
	temporalIqmProductAdmissionFacts_t facts, mutation;
	qboolean *fields;
	memset( &facts, 0, sizeof( facts ) );
	facts.primaryCommand = qtrue;
	facts.iqmSurface = qtrue;
	facts.entityIndex = 0;
	facts.worldEntityIndex = 1023;
	facts.gpuDirect = qtrue;
	facts.ordinaryAvailable = qtrue;
	facts.h5Eligible = qtrue;
	facts.entityExact = qtrue;
	facts.modelExact = qtrue;
	facts.geometryExact = qtrue;
	facts.opaqueStage0Exact = qtrue;
	facts.bindlessExact = qtrue;
	CHECK( R_TemporalIqmClassifyProductSurface( &facts )
		== TEMPORAL_IQM_PRODUCT_ADMIT );
	mutation = facts; mutation.primaryCommand = qfalse;
	CHECK( R_TemporalIqmClassifyProductSurface( &mutation )
		== TEMPORAL_IQM_PRODUCT_NONE );
	mutation = facts; mutation.iqmSurface = qfalse;
	CHECK( R_TemporalIqmClassifyProductSurface( &mutation )
		== TEMPORAL_IQM_PRODUCT_NONE );
	mutation = facts; mutation.gpuDirect = qfalse;
	CHECK( R_TemporalIqmClassifyProductSurface( &mutation )
		== TEMPORAL_IQM_PRODUCT_NONE );
	mutation = facts; mutation.entityIndex = mutation.worldEntityIndex;
	CHECK( R_TemporalIqmClassifyProductSurface( &mutation )
		== TEMPORAL_IQM_PRODUCT_POISON );
	mutation = facts; mutation.entityIndex = -1;
	CHECK( R_TemporalIqmClassifyProductSurface( &mutation )
		== TEMPORAL_IQM_PRODUCT_POISON );
	{
		temporalIqmProductAdmissionFacts_t mixed[2] = { facts, facts };
		uint32_t admitted = 0;
		mixed[0].gpuDirect = qfalse;
		for ( size_t i = 0; i < 2u; ++i ) {
			temporalIqmProductAdmission_t result =
				R_TemporalIqmClassifyProductSurface( &mixed[i] );
			CHECK( result == ( i ? TEMPORAL_IQM_PRODUCT_ADMIT
				: TEMPORAL_IQM_PRODUCT_NONE ) );
			if ( result == TEMPORAL_IQM_PRODUCT_ADMIT ) admitted++;
		}
		CHECK( admitted == 1u );
	}
	fields = &facts.ordinaryAvailable;
	for ( size_t i = 0; i < 7u; ++i ) {
		mutation = facts;
		( (qboolean *)&mutation.ordinaryAvailable )[i] = qfalse;
		CHECK( fields[i] == qtrue );
		CHECK( R_TemporalIqmClassifyProductSurface( &mutation )
			== TEMPORAL_IQM_PRODUCT_POISON );
	}
	CHECK( R_TemporalIqmClassifyProductSurface( NULL )
		== TEMPORAL_IQM_PRODUCT_NONE );
	return 0;
}

int main( void ) {
	int32_t frame, oldframe;
	float backlerp;
	CHECK( TEMPORAL_IQM_RECORD_SIZE == 12480u );
	CHECK( TEMPORAL_IQM_SLOT_BYTES == 3194880u );
	CHECK( !R_TemporalIqmStorageRangeSupported( 0u ) );
	CHECK( !R_TemporalIqmStorageRangeSupported( TEMPORAL_IQM_SLOT_BYTES - 1u ) );
	CHECK( R_TemporalIqmStorageRangeSupported( TEMPORAL_IQM_SLOT_BYTES ) );
	CHECK( R_TemporalIqmStorageRangeSupported( UINT64_MAX ) );
	CHECK( offsetof( temporalIqmGpuRecord_t, previousBones ) == 6144u );
	CHECK( offsetof( temporalIqmGpuRecord_t, rasterMvp ) == 12288u );
	CHECK( offsetof( temporalIqmGpuRecord_t, temporalPreviousMvp ) == 12416u );
	CHECK( R_TemporalIqmNormalizeFrameTuple( 3, 7, 8, 0.25f,
		&frame, &oldframe, &backlerp ) );
	CHECK( frame == 1 && oldframe == 2 && backlerp == 0.25f );
	CHECK( !R_TemporalIqmNormalizeFrameTuple( 3, -1, 0, 0.0f,
		&frame, &oldframe, &backlerp ) );
	CHECK( TestCanonicalMvp() == 0 );
	CHECK( TestInfluenceDecode() == 0 );
	CHECK( TestRecord() == 0 );
	CHECK( TestNonUniformPalette() == 0 );
	CHECK( TestLegacyPaletteParity() == 0 );
	CHECK( TestSequence() == 0 );
	CHECK( TestProductAdmission() == 0 );
	puts( "tr_temporal_iqm_motion_test: PASS" );
	return 0;
}
