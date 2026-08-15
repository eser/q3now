// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_iqm_motion.h"

#include <math.h>
#include <string.h>

#define TEMPORAL_IQM_MODEL_TYPE 4u
#define TEMPORAL_IQM_FNV_OFFSET UINT64_C(1469598103934665603)
#define TEMPORAL_IQM_FNV_PRIME UINT64_C(1099511628211)

_Static_assert( offsetof( temporalIqmGpuRecord_t, currentBones )
	== TEMPORAL_IQM_CURRENT_BONES_OFFSET, "IQM current palette ABI" );
_Static_assert( offsetof( temporalIqmGpuRecord_t, previousBones )
	== TEMPORAL_IQM_PREVIOUS_BONES_OFFSET, "IQM previous palette ABI" );
_Static_assert( offsetof( temporalIqmGpuRecord_t, rasterMvp )
	== TEMPORAL_IQM_RASTER_MVP_OFFSET, "IQM raster MVP ABI" );
_Static_assert( offsetof( temporalIqmGpuRecord_t, temporalCurrentMvp )
	== TEMPORAL_IQM_CURRENT_MVP_OFFSET, "IQM current MVP ABI" );
_Static_assert( offsetof( temporalIqmGpuRecord_t, temporalPreviousMvp )
	== TEMPORAL_IQM_PREVIOUS_MVP_OFFSET, "IQM previous MVP ABI" );
_Static_assert( sizeof( temporalIqmGpuRecord_t ) == TEMPORAL_IQM_RECORD_SIZE,
	"IQM record stride ABI" );
_Static_assert( _Alignof( temporalIqmGpuRecord_t ) <= TEMPORAL_IQM_RECORD_ALIGNMENT,
	"IQM record alignment ABI" );
_Static_assert( TEMPORAL_IQM_SLOT_BYTES == 3194880u,
	"IQM fixed slot byte budget" );

static qboolean Finite( const float *values, size_t count ) {
	if ( !values ) return qfalse;
	for ( size_t i = 0; i < count; ++i ) {
		if ( !isfinite( values[i] ) ) return qfalse;
	}
	return qtrue;
}

static qboolean PoseValid( const temporalEntityPose_t *pose ) {
	return pose && pose->hModel > 0 && pose->modelToken
		&& pose->modelDataToken && pose->modelType == TEMPORAL_IQM_MODEL_TYPE
		&& pose->modelTopology && isfinite( pose->backlerp )
		&& pose->backlerp >= 0.0f && pose->backlerp <= 1.0f
		&& Finite( pose->origin, 3 ) && Finite( pose->axis, 9 )
		&& pose->nonNormalizedAxes <= 1u ? qtrue : qfalse;
}

static qboolean PoseModelExact( const temporalEntityPose_t *a,
		const temporalEntityPose_t *b ) {
	return a->hModel == b->hModel && a->modelToken == b->modelToken
		&& a->modelDataToken == b->modelDataToken
		&& a->modelType == b->modelType
		&& a->modelTopology == b->modelTopology
		&& a->modelAllocationGeneration == b->modelAllocationGeneration
		&& a->modelContentDigest == b->modelContentDigest ? qtrue : qfalse;
}

static qboolean ModelViewValid( const temporalIqmModelView_t *model ) {
	if ( !model || model->validated != qtrue || !model->modelDataToken
			|| !model->numFrames
			|| !model->numJoints || model->numJoints > TEMPORAL_IQM_MAX_JOINTS
			|| model->numPoses != model->numJoints || !model->jointParents
			|| !model->bindJoints || !model->inverseBindJoints || !model->poses
			|| model->numFrames > UINT32_MAX / model->numPoses ) return qfalse;
	for ( uint32_t i = 0; i < model->numJoints; ++i ) {
		if ( model->jointParents[i] < -1
				|| model->jointParents[i] >= (int32_t)i ) return qfalse;
	}
	return Finite( model->bindJoints, (size_t)model->numJoints * 12u )
		&& Finite( model->inverseBindJoints,
			(size_t)model->numJoints * 12u ) ? qtrue : qfalse;
}

static qboolean ModelValid( const temporalIqmModelView_t *model,
		const temporalEntityPose_t *pose ) {
	return ModelViewValid( model ) && pose
		&& model->contentDigest && model->topologyGeneration
		&& model->modelAllocationGeneration
		&& pose->modelDataToken == model->modelDataToken
		&& pose->modelTopology == model->topologyGeneration
		&& pose->modelAllocationGeneration == model->modelAllocationGeneration
		&& pose->modelContentDigest == model->contentDigest ? qtrue : qfalse;
}

qboolean R_TemporalIqmStorageRangeSupported( uint64_t maxStorageBufferRange ) {
	return maxStorageBufferRange >= (uint64_t)TEMPORAL_IQM_SLOT_BYTES
		? qtrue : qfalse;
}

qboolean R_TemporalIqmNormalizeFrameTuple( uint32_t numFrames,
		int32_t frame, int32_t oldframe, float backlerp,
		int32_t *outFrame, int32_t *outOldFrame, float *outBacklerp ) {
	int32_t normalizedFrame, normalizedOld;
	if ( !numFrames || numFrames > INT32_MAX || frame < 0 || oldframe < 0
			|| !isfinite( backlerp ) || backlerp < 0.0f || backlerp > 1.0f
			|| !outFrame || !outOldFrame || !outBacklerp ) return qfalse;
	normalizedFrame = frame % (int32_t)numFrames;
	normalizedOld = oldframe % (int32_t)numFrames;
	*outFrame = normalizedFrame;
	*outOldFrame = normalizedOld;
	*outBacklerp = backlerp == 0.0f ? 0.0f : backlerp;
	return qtrue;
}

qboolean R_TemporalIqmDecodeInfluence( uint32_t indexFormat,
		const void *indexData, uint32_t weightFormat, const void *weightData,
		uint32_t vertexIndex, uint32_t numJoints, uint8_t outIndices[4],
		float outWeights[4] ) {
	uint8_t indices[4];
	float weights[4], sum = 0.0f;
	if ( !indexData || !weightData || !outIndices || !outWeights
			|| !numJoints || numJoints > TEMPORAL_IQM_MAX_JOINTS
			|| ( indexFormat != TEMPORAL_IQM_SCALAR_UBYTE
				&& indexFormat != TEMPORAL_IQM_SCALAR_INT )
			|| ( weightFormat != TEMPORAL_IQM_SCALAR_UBYTE
				&& weightFormat != TEMPORAL_IQM_SCALAR_FLOAT ) ) return qfalse;
	for ( uint32_t i = 0; i < 4; ++i ) {
		uint64_t offset = (uint64_t)vertexIndex * 4u + i;
		int32_t sourceIndex;
		if ( offset > SIZE_MAX / sizeof( int32_t ) ) return qfalse;
		if ( weightFormat == TEMPORAL_IQM_SCALAR_UBYTE )
			weights[i] = (float)((const uint8_t *)weightData)[(size_t)offset]
				/ 255.0f;
		else memcpy( &weights[i], (const uint8_t *)weightData
			+ (size_t)offset * sizeof( float ), sizeof( weights[i] ) );
		if ( !isfinite( weights[i] ) || weights[i] < 0.0f ) return qfalse;
		if ( indexFormat == TEMPORAL_IQM_SCALAR_UBYTE )
			sourceIndex = ((const uint8_t *)indexData)[(size_t)offset];
		else memcpy( &sourceIndex, (const uint8_t *)indexData
			+ (size_t)offset * sizeof( int32_t ), sizeof( sourceIndex ) );
		if ( i == 0u || weights[i] > 0.0f ) {
			if ( sourceIndex < 0 || sourceIndex > UINT8_MAX
					|| (uint32_t)sourceIndex >= numJoints ) return qfalse;
			indices[i] = (uint8_t)sourceIndex;
		} else {
			indices[i] = 0u;
		}
		sum += weights[i];
	}
	if ( !isfinite( sum ) || sum <= 0.0f ) return qfalse;
	memcpy( outIndices, indices, sizeof( indices ) );
	memcpy( outWeights, weights, sizeof( weights ) );
	return qtrue;
}

static void Matrix34Multiply( const float *a, const float *b, float *out ) {
	out[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8];
	out[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9];
	out[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10];
	out[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3];
	out[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8];
	out[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9];
	out[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10];
	out[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7];
	out[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8];
	out[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9];
	out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
	out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];
}

static void JointToMatrix( const float rot[4], const float scale[3],
		const float trans[3], float out[12] ) {
	float xx = 2.0f * rot[0] * rot[0];
	float yy = 2.0f * rot[1] * rot[1];
	float zz = 2.0f * rot[2] * rot[2];
	float xy = 2.0f * rot[0] * rot[1];
	float xz = 2.0f * rot[0] * rot[2];
	float yz = 2.0f * rot[1] * rot[2];
	float wx = 2.0f * rot[3] * rot[0];
	float wy = 2.0f * rot[3] * rot[1];
	float wz = 2.0f * rot[3] * rot[2];
	out[0] = scale[0] * ( 1.0f - ( yy + zz ) );
	out[1] = scale[0] * ( xy - wz );
	out[2] = scale[0] * ( xz + wy ); out[3] = trans[0];
	out[4] = scale[1] * ( xy + wz );
	out[5] = scale[1] * ( 1.0f - ( xx + zz ) );
	out[6] = scale[1] * ( yz - wx ); out[7] = trans[1];
	out[8] = scale[2] * ( xz - wy );
	out[9] = scale[2] * ( yz + wx );
	out[10] = scale[2] * ( 1.0f - ( xx + yy ) ); out[11] = trans[2];
}

static void QuatSlerp( const float from[4], const float toInput[4],
		float fraction, float out[4] ) {
	float angle, cosAngle, sinAngle, backlerp, lerp;
	float to[4];
	cosAngle = from[0] * toInput[0] + from[1] * toInput[1]
		+ from[2] * toInput[2] + from[3] * toInput[3];
	if ( cosAngle < 0.0f ) {
		cosAngle = -cosAngle;
		to[0] = -toInput[0]; to[1] = -toInput[1];
		to[2] = -toInput[2]; to[3] = -toInput[3];
	} else {
		memcpy( to, toInput, sizeof( to ) );
	}
	if ( cosAngle < 0.999999f ) {
		angle = acosf( cosAngle );
		sinAngle = sinf( angle );
		backlerp = sinf( ( 1.0f - fraction ) * angle ) / sinAngle;
		lerp = sinf( fraction * angle ) / sinAngle;
	} else {
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}
	out[0] = from[0] * backlerp + to[0] * lerp;
	out[1] = from[1] * backlerp + to[1] * lerp;
	out[2] = from[2] * backlerp + to[2] * lerp;
	out[3] = from[3] * backlerp + to[3] * lerp;
}

static qboolean FrameTupleValid( const temporalIqmModelView_t *model,
		int32_t frame, int32_t oldframe, float backlerp ) {
	return model && frame >= 0 && oldframe >= 0
		&& (uint32_t)frame < model->numFrames
		&& (uint32_t)oldframe < model->numFrames
		&& isfinite( backlerp ) && backlerp >= 0.0f && backlerp <= 1.0f
		? qtrue : qfalse;
}

qboolean R_TemporalIqmBuildPaletteTuple( const temporalIqmModelView_t *model,
		int32_t frame, int32_t oldframe, float backlerp,
		float out[TEMPORAL_IQM_BONE_ROWS][4] ) {
	temporalIqmTransform_t relative[TEMPORAL_IQM_MAX_JOINTS];
	float candidate[TEMPORAL_IQM_BONE_ROWS][4];
	float lerp = 1.0f - backlerp;
	if ( !out || !ModelViewValid( model )
			|| !FrameTupleValid( model, frame, oldframe, backlerp ) ) return qfalse;
	memset( candidate, 0, sizeof( candidate ) );
	for ( uint32_t i = 0; i < model->numPoses; ++i ) {
		const temporalIqmTransform_t *pose = &model->poses[
			(uint32_t)frame * model->numPoses + i];
		const temporalIqmTransform_t *old = &model->poses[
			(uint32_t)oldframe * model->numPoses + i];
		if ( !Finite( pose->translate, 3 ) || !Finite( pose->rotate, 4 )
				|| !Finite( pose->scale, 3 ) || !Finite( old->translate, 3 )
				|| !Finite( old->rotate, 4 ) || !Finite( old->scale, 3 ) ) return qfalse;
		if ( oldframe == frame ) {
			memcpy( relative[i].translate, pose->translate,
				sizeof( relative[i].translate ) );
			memcpy( relative[i].rotate, pose->rotate,
				sizeof( relative[i].rotate ) );
			memcpy( relative[i].scale, pose->scale,
				sizeof( relative[i].scale ) );
			continue;
		}
		for ( int j = 0; j < 3; ++j ) {
			relative[i].translate[j] = old->translate[j] * backlerp
				+ pose->translate[j] * lerp;
			relative[i].scale[j] = old->scale[j] * backlerp
				+ pose->scale[j] * lerp;
		}
		QuatSlerp( old->rotate, pose->rotate, lerp, relative[i].rotate );
	}
	for ( uint32_t i = 0; i < model->numPoses; ++i ) {
		float local[12], intermediate[12];
		float *dst = &candidate[i * 3u][0];
		JointToMatrix( relative[i].rotate, relative[i].scale,
			relative[i].translate, local );
		if ( model->jointParents[i] >= 0 ) {
			uint32_t parent = (uint32_t)model->jointParents[i];
			Matrix34Multiply( &model->bindJoints[parent * 12u], local,
				intermediate );
			Matrix34Multiply( intermediate, &model->inverseBindJoints[i * 12u],
				local );
			Matrix34Multiply( &candidate[parent * 3u][0], local, dst );
		} else {
			Matrix34Multiply( local, &model->inverseBindJoints[i * 12u], dst );
		}
	}
	if ( !Finite( &candidate[0][0], TEMPORAL_IQM_BONE_ROWS * 4u ) ) return qfalse;
	memcpy( out, candidate, sizeof( candidate ) );
	return qtrue;
}

qboolean R_TemporalIqmBuildPalette( const temporalIqmModelView_t *model,
		const temporalEntityPose_t *entity, float out[TEMPORAL_IQM_BONE_ROWS][4] ) {
	if ( !PoseValid( entity ) || !ModelValid( model, entity ) ) return qfalse;
	return R_TemporalIqmBuildPaletteTuple( model, entity->frame,
		entity->oldframe, entity->backlerp, out );
}

static qboolean CurrentCameraValid( const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity ) {
	return camera && entity && camera->valid == qtrue && entity->valid == qtrue
		&& camera->frameId && camera->frameId == entity->frameId
		&& Finite( camera->current.projection, 16 )
		&& Finite( camera->current.worldModel, 16 ) && PoseValid( &entity->current )
		&& RefEntityMotion_IsValid( &entity->identity ) ? qtrue : qfalse;
}

qboolean R_TemporalIqmBuildGpuRecord( const temporalIqmModelView_t *model,
		const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity,
		const float jitteredProjection[16], temporalIqmGpuRecord_t *outRecord,
		qboolean *outPreviousValid ) {
	temporalIqmGpuRecord_t candidate;
	qboolean previousValid;
	if ( !outRecord || !outPreviousValid || !jitteredProjection
			|| !Finite( jitteredProjection, 16 )
			|| !CurrentCameraValid( camera, entity )
			|| !ModelValid( model, &entity->current ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	if ( !R_TemporalIqmBuildPalette( model, &entity->current,
			candidate.currentBones )
			|| !R_TemporalMotionBuildCanonicalEntityMvp( jitteredProjection,
				camera->current.worldModel, &entity->current, candidate.rasterMvp )
			|| !R_TemporalMotionBuildCanonicalEntityMvp(
				camera->current.projection, camera->current.worldModel,
				&entity->current, candidate.temporalCurrentMvp ) ) return qfalse;
	previousValid = camera->previousValid == qtrue
		&& entity->previousValid == qtrue && camera->previousFrameId
		&& camera->frameId == camera->previousFrameId + 1u
		&& entity->previousFrameId == camera->previousFrameId
		&& PoseValid( &entity->previous )
		&& PoseModelExact( &entity->current, &entity->previous );
	if ( previousValid ) {
		if ( !R_TemporalIqmBuildPalette( model, &entity->previous,
				candidate.previousBones )
				|| !Finite( camera->previous.projection, 16 )
				|| !Finite( camera->previous.worldModel, 16 )
				|| !R_TemporalMotionBuildCanonicalEntityMvp(
					camera->previous.projection, camera->previous.worldModel,
					&entity->previous, candidate.temporalPreviousMvp ) ) {
			previousValid = qfalse;
		}
	}
	if ( !previousValid ) {
		memcpy( candidate.previousBones, candidate.currentBones,
			sizeof( candidate.previousBones ) );
		memcpy( candidate.temporalPreviousMvp, candidate.temporalCurrentMvp,
			sizeof( candidate.temporalPreviousMvp ) );
	}
	*outRecord = candidate;
	*outPreviousValid = previousValid;
	return qtrue;
}

static qboolean AuthorityValid( const temporalIqmSequenceAuthority_t *a ) {
	return a && a->token && a->frameId && a->worldIndex >= 0
		&& a->frameCount && a->commandSlot < a->frameCount ? qtrue : qfalse;
}

static qboolean IdentityEqual( const refEntityMotion_t *a,
		const refEntityMotion_t *b ) {
	return a->structSize == b->structSize && a->version == b->version
		&& a->ownerId == b->ownerId && a->generation == b->generation
		&& a->role == b->role && a->flags == b->flags ? qtrue : qfalse;
}

static qboolean PoseEqual( const temporalEntityPose_t *a,
		const temporalEntityPose_t *b ) {
	return a->hModel == b->hModel && a->modelToken == b->modelToken
		&& a->modelDataToken == b->modelDataToken && a->modelType == b->modelType
		&& a->modelTopology == b->modelTopology
		&& a->modelAllocationGeneration == b->modelAllocationGeneration
		&& a->modelContentDigest == b->modelContentDigest
		&& a->frame == b->frame
		&& a->oldframe == b->oldframe
		&& memcmp( &a->backlerp, &b->backlerp, sizeof( a->backlerp ) ) == 0
		&& memcmp( a->origin, b->origin, sizeof( a->origin ) ) == 0
		&& memcmp( a->axis, b->axis, sizeof( a->axis ) ) == 0
		&& a->nonNormalizedAxes == b->nonNormalizedAxes ? qtrue : qfalse;
}

static qboolean EntityTupleEqual( const temporalIqmDrawFacts_t *a,
		const temporalIqmDrawFacts_t *b ) {
	return a->entityIndex == b->entityIndex
		&& IdentityEqual( &a->identity, &b->identity )
		&& PoseEqual( &a->currentPose, &b->currentPose )
		&& PoseEqual( &a->previousPose, &b->previousPose )
		&& a->modelContentDigest == b->modelContentDigest
		&& a->modelTopologyGeneration == b->modelTopologyGeneration
		&& a->modelAllocationGeneration == b->modelAllocationGeneration
		&& a->previousValid == b->previousValid ? qtrue : qfalse;
}

temporalIqmProductAdmission_t R_TemporalIqmClassifyProductSurface(
		const temporalIqmProductAdmissionFacts_t *facts ) {
	if ( !facts || facts->primaryCommand != qtrue )
		return TEMPORAL_IQM_PRODUCT_NONE;
	if ( facts->iqmSurface != qtrue ) return TEMPORAL_IQM_PRODUCT_NONE;
	if ( facts->gpuDirect != qtrue ) return TEMPORAL_IQM_PRODUCT_NONE;
	if ( facts->entityIndex < 0
			|| facts->entityIndex == facts->worldEntityIndex
			|| (uint32_t)facts->entityIndex >= TEMPORAL_IQM_ENTITY_INDEX_LIMIT )
		return TEMPORAL_IQM_PRODUCT_POISON;
	if ( facts->ordinaryAvailable == qtrue
			&& facts->h5Eligible == qtrue && facts->entityExact == qtrue
			&& facts->modelExact == qtrue && facts->geometryExact == qtrue
			&& facts->opaqueStage0Exact == qtrue
			&& facts->bindlessExact == qtrue )
		return TEMPORAL_IQM_PRODUCT_ADMIT;
	return TEMPORAL_IQM_PRODUCT_POISON;
}

qboolean R_TemporalIqmDrawFactsValid( const temporalIqmDrawFacts_t *f ) {
	if ( !f || f->entityIndex >= TEMPORAL_IQM_ENTITY_INDEX_LIMIT
			|| !RefEntityMotion_IsValid( &f->identity )
			|| !PoseValid( &f->currentPose ) || !f->modelContentDigest
			|| !f->modelTopologyGeneration || !f->modelAllocationGeneration
			|| !f->indexCount
			|| f->currentPose.modelTopology != f->modelTopologyGeneration
			|| f->currentPose.modelAllocationGeneration
				!= f->modelAllocationGeneration
			|| f->currentPose.modelContentDigest != f->modelContentDigest
			|| f->firstIndex > UINT32_MAX - f->indexCount
			|| !f->rawVertexBuffer || !f->rawIndexBuffer
			|| !f->ralVertexBuffer || !f->ralIndexBuffer
			|| f->rawVertexBuffer == f->rawIndexBuffer
			|| f->ralVertexBuffer == f->ralIndexBuffer
			|| !f->vertexBufferBytes || !f->indexBufferBytes
			|| f->vertexBufferBytes % TEMPORAL_IQM_VERTEX_STRIDE
			|| f->indexBufferBytes % sizeof( uint32_t )
			|| ( (uint64_t)f->firstIndex + f->indexCount )
				* sizeof( uint32_t ) > f->indexBufferBytes
			|| !f->geometryBackend || !f->geometryGeneration
			|| f->geometryGeneration == UINT32_MAX
			|| !f->geometryAllocationGeneration
			|| f->geometryAllocationGeneration == UINT32_MAX
			|| f->textureSlot >= TEMPORAL_IQM_TEXTURE_SLOTS
			|| f->samplerSlot >= TEMPORAL_IQM_SAMPLER_SLOTS
			|| !VK_BindlessPublicationReceiptExact( &f->bindless, &f->bindless )
			|| f->bindless.imageSlot != f->textureSlot
			|| f->bindless.samplerSlot != f->samplerSlot
			|| ( f->outcome != TEMPORAL_MOTION_WRITE_VALID
				&& f->outcome != TEMPORAL_MOTION_INVALIDATE_OPAQUE )
			|| f->previousValid > qtrue ) return qfalse;
	if ( f->outcome == TEMPORAL_MOTION_WRITE_VALID
			&& ( f->previousValid != qtrue || !PoseValid( &f->previousPose )
				|| !PoseModelExact( &f->currentPose, &f->previousPose ) ) ) return qfalse;
	if ( f->outcome == TEMPORAL_MOTION_INVALIDATE_OPAQUE
			&& ( f->previousValid != qfalse
				|| !PoseEqual( &f->currentPose, &f->previousPose ) ) ) return qfalse;
	return qtrue;
}

static qboolean FactsEqual( const temporalIqmDrawFacts_t *a,
		const temporalIqmDrawFacts_t *b ) {
	return a->ordinal == b->ordinal
		&& a->sourceDrawSurfOrdinal == b->sourceDrawSurfOrdinal
		&& EntityTupleEqual( a, b )
		&& a->surfaceIndex == b->surfaceIndex && a->firstIndex == b->firstIndex
		&& a->indexCount == b->indexCount
		&& a->rawVertexBuffer == b->rawVertexBuffer
		&& a->rawIndexBuffer == b->rawIndexBuffer
		&& a->ralVertexBuffer == b->ralVertexBuffer
		&& a->ralIndexBuffer == b->ralIndexBuffer
		&& a->vertexBufferBytes == b->vertexBufferBytes
		&& a->indexBufferBytes == b->indexBufferBytes
		&& a->geometryBackend == b->geometryBackend
		&& a->geometryGeneration == b->geometryGeneration
		&& a->geometryAllocationGeneration == b->geometryAllocationGeneration
		&& a->textureSlot == b->textureSlot && a->samplerSlot == b->samplerSlot
		&& VK_BindlessPublicationReceiptExact( &a->bindless, &b->bindless )
		&& a->outcome == b->outcome ? qtrue : qfalse;
}

static uint64_t FoldBytes( uint64_t hash, const void *data, size_t size ) {
	const unsigned char *bytes = (const unsigned char *)data;
	for ( size_t i = 0; i < size; ++i ) {
		hash ^= bytes[i];
		hash *= TEMPORAL_IQM_FNV_PRIME;
	}
	return hash;
}

static uint64_t FoldPose( uint64_t hash, const temporalEntityPose_t *pose ) {
	hash = FoldBytes( hash, &pose->hModel, sizeof( pose->hModel ) );
	hash = FoldBytes( hash, &pose->modelToken, sizeof( pose->modelToken ) );
	hash = FoldBytes( hash, &pose->modelDataToken, sizeof( pose->modelDataToken ) );
	hash = FoldBytes( hash, &pose->modelType, sizeof( pose->modelType ) );
	hash = FoldBytes( hash, &pose->modelTopology, sizeof( pose->modelTopology ) );
	hash = FoldBytes( hash, &pose->modelAllocationGeneration,
		sizeof( pose->modelAllocationGeneration ) );
	hash = FoldBytes( hash, &pose->modelContentDigest,
		sizeof( pose->modelContentDigest ) );
	hash = FoldBytes( hash, &pose->frame, sizeof( pose->frame ) );
	hash = FoldBytes( hash, &pose->oldframe, sizeof( pose->oldframe ) );
	hash = FoldBytes( hash, &pose->backlerp, sizeof( pose->backlerp ) );
	hash = FoldBytes( hash, pose->origin, sizeof( pose->origin ) );
	hash = FoldBytes( hash, pose->axis, sizeof( pose->axis ) );
	return FoldBytes( hash, &pose->nonNormalizedAxes,
		sizeof( pose->nonNormalizedAxes ) );
}

static uint64_t EntryDigest( uint64_t hash,
		const temporalIqmSequenceEntry_t *entry ) {
	const temporalIqmDrawFacts_t *f = &entry->facts;
	hash = FoldBytes( hash, &entry->recordIndex, sizeof( entry->recordIndex ) );
	hash = FoldBytes( hash, &f->ordinal, sizeof( f->ordinal ) );
	hash = FoldBytes( hash, &f->sourceDrawSurfOrdinal,
		sizeof( f->sourceDrawSurfOrdinal ) );
	hash = FoldBytes( hash, &f->entityIndex, sizeof( f->entityIndex ) );
	hash = FoldBytes( hash, &f->identity.structSize, sizeof( f->identity.structSize ) );
	hash = FoldBytes( hash, &f->identity.version, sizeof( f->identity.version ) );
	hash = FoldBytes( hash, &f->identity.ownerId, sizeof( f->identity.ownerId ) );
	hash = FoldBytes( hash, &f->identity.generation, sizeof( f->identity.generation ) );
	hash = FoldBytes( hash, &f->identity.role, sizeof( f->identity.role ) );
	hash = FoldBytes( hash, &f->identity.flags, sizeof( f->identity.flags ) );
	hash = FoldPose( hash, &f->currentPose );
	hash = FoldPose( hash, &f->previousPose );
	hash = FoldBytes( hash, &f->modelContentDigest, sizeof( f->modelContentDigest ) );
	hash = FoldBytes( hash, &f->modelTopologyGeneration,
		sizeof( f->modelTopologyGeneration ) );
	hash = FoldBytes( hash, &f->modelAllocationGeneration,
		sizeof( f->modelAllocationGeneration ) );
	hash = FoldBytes( hash, &f->surfaceIndex, sizeof( f->surfaceIndex ) );
	hash = FoldBytes( hash, &f->firstIndex, sizeof( f->firstIndex ) );
	hash = FoldBytes( hash, &f->indexCount, sizeof( f->indexCount ) );
	hash = FoldBytes( hash, &f->rawVertexBuffer, sizeof( f->rawVertexBuffer ) );
	hash = FoldBytes( hash, &f->rawIndexBuffer, sizeof( f->rawIndexBuffer ) );
	hash = FoldBytes( hash, &f->ralVertexBuffer, sizeof( f->ralVertexBuffer ) );
	hash = FoldBytes( hash, &f->ralIndexBuffer, sizeof( f->ralIndexBuffer ) );
	hash = FoldBytes( hash, &f->vertexBufferBytes, sizeof( f->vertexBufferBytes ) );
	hash = FoldBytes( hash, &f->indexBufferBytes, sizeof( f->indexBufferBytes ) );
	hash = FoldBytes( hash, &f->geometryBackend, sizeof( f->geometryBackend ) );
	hash = FoldBytes( hash, &f->geometryGeneration,
		sizeof( f->geometryGeneration ) );
	hash = FoldBytes( hash, &f->geometryAllocationGeneration,
		sizeof( f->geometryAllocationGeneration ) );
	hash = FoldBytes( hash, &f->textureSlot, sizeof( f->textureSlot ) );
	hash = FoldBytes( hash, &f->samplerSlot, sizeof( f->samplerSlot ) );
	hash = FoldBytes( hash, &f->bindless.setIdentity,
		sizeof( f->bindless.setIdentity ) );
	hash = FoldBytes( hash, &f->bindless.samplerPoolIdentity,
		sizeof( f->bindless.samplerPoolIdentity ) );
	hash = FoldBytes( hash, &f->bindless.imageViewIdentity,
		sizeof( f->bindless.imageViewIdentity ) );
	hash = FoldBytes( hash, &f->bindless.samplerIdentity,
		sizeof( f->bindless.samplerIdentity ) );
	hash = FoldBytes( hash, &f->bindless.imageOwnerIdentity,
		sizeof( f->bindless.imageOwnerIdentity ) );
	hash = FoldBytes( hash, &f->bindless.ordinaryDescriptorIdentity,
		sizeof( f->bindless.ordinaryDescriptorIdentity ) );
	hash = FoldBytes( hash, &f->bindless.imageOwnerGeneration,
		sizeof( f->bindless.imageOwnerGeneration ) );
	hash = FoldBytes( hash, &f->bindless.samplerDefinitionDigest,
		sizeof( f->bindless.samplerDefinitionDigest ) );
	hash = FoldBytes( hash, &f->bindless.setGeneration,
		sizeof( f->bindless.setGeneration ) );
	hash = FoldBytes( hash, &f->bindless.samplerPoolGeneration,
		sizeof( f->bindless.samplerPoolGeneration ) );
	hash = FoldBytes( hash, &f->bindless.imagePublicationGeneration,
		sizeof( f->bindless.imagePublicationGeneration ) );
	hash = FoldBytes( hash, &f->bindless.imageTransactionGeneration,
		sizeof( f->bindless.imageTransactionGeneration ) );
	hash = FoldBytes( hash, &f->bindless.samplerPublicationGeneration,
		sizeof( f->bindless.samplerPublicationGeneration ) );
	hash = FoldBytes( hash, &f->bindless.samplerTransactionGeneration,
		sizeof( f->bindless.samplerTransactionGeneration ) );
	hash = FoldBytes( hash, &f->bindless.imageSlotGeneration,
		sizeof( f->bindless.imageSlotGeneration ) );
	hash = FoldBytes( hash, &f->bindless.samplerSlotGeneration,
		sizeof( f->bindless.samplerSlotGeneration ) );
	hash = FoldBytes( hash, &f->bindless.imageBinding,
		sizeof( f->bindless.imageBinding ) );
	hash = FoldBytes( hash, &f->bindless.samplerBinding,
		sizeof( f->bindless.samplerBinding ) );
	hash = FoldBytes( hash, &f->bindless.ready,
		sizeof( f->bindless.ready ) );
	hash = FoldBytes( hash, &f->outcome, sizeof( f->outcome ) );
	hash = FoldBytes( hash, &f->previousValid, sizeof( f->previousValid ) );
	return hash;
}

qboolean R_TemporalIqmSequenceBuild(
		const temporalIqmSequenceAuthority_t *authority,
		const temporalIqmDrawFacts_t *draws, uint32_t drawCount,
		temporalIqmSequence_t *outSequence ) {
	temporalIqmSequence_t candidate;
	if ( !outSequence || !AuthorityValid( authority )
			|| drawCount > TEMPORAL_IQM_MAX_DRAWS
			|| ( drawCount && !draws ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.authority = *authority;
	candidate.orderedDigest = TEMPORAL_IQM_FNV_OFFSET;
	for ( uint32_t i = 0; i < drawCount; ++i ) {
		temporalIqmSequenceEntry_t *entry = &candidate.entries[i];
		uint32_t recordIndex = UINT32_MAX;
		if ( draws[i].ordinal != i || !R_TemporalIqmDrawFactsValid( &draws[i] )
				|| ( i && draws[i].sourceDrawSurfOrdinal
					<= draws[i - 1u].sourceDrawSurfOrdinal ) ) return qfalse;
		{
			for ( uint32_t j = 0; j < i; ++j ) {
				qboolean sameIndex = candidate.entries[j].facts.entityIndex
					== draws[i].entityIndex;
				qboolean sameIdentity = IdentityEqual(
					&candidate.entries[j].facts.identity, &draws[i].identity );
				if ( candidate.entries[j].recordIndex == UINT32_MAX ) continue;
				if ( sameIndex != sameIdentity ) return qfalse;
				if ( sameIndex ) {
					if ( !EntityTupleEqual( &candidate.entries[j].facts, &draws[i] ) )
						return qfalse;
					recordIndex = candidate.entries[j].recordIndex;
					break;
				}
			}
			if ( recordIndex == UINT32_MAX ) {
				if ( candidate.entityCount >= TEMPORAL_IQM_MAX_RECORDS ) return qfalse;
				recordIndex = candidate.entityCount++;
			}
		}
		entry->facts = draws[i];
		entry->recordIndex = recordIndex;
		candidate.orderedDigest = EntryDigest( candidate.orderedDigest, entry );
	}
	candidate.drawCount = drawCount;
	candidate.ready = qtrue;
	if ( !candidate.orderedDigest ) candidate.orderedDigest = 1u;
	*outSequence = candidate;
	return qtrue;
}

static qboolean SequenceValid( const temporalIqmSequence_t *sequence ) {
	uint64_t digest = TEMPORAL_IQM_FNV_OFFSET;
	uint32_t entityCount = 0;
	if ( !sequence || sequence->ready != qtrue
			|| !AuthorityValid( &sequence->authority )
			|| sequence->drawCount > TEMPORAL_IQM_MAX_DRAWS
			|| sequence->entityCount > TEMPORAL_IQM_MAX_RECORDS ) return qfalse;
	for ( uint32_t i = 0; i < sequence->drawCount; ++i ) {
		const temporalIqmSequenceEntry_t *entry = &sequence->entries[i];
		uint32_t recordIndex = UINT32_MAX;
		if ( entry->facts.ordinal != i || !R_TemporalIqmDrawFactsValid( &entry->facts )
				|| ( i && entry->facts.sourceDrawSurfOrdinal
					<= sequence->entries[i - 1u].facts.sourceDrawSurfOrdinal ) )
			return qfalse;
		for ( uint32_t j = 0; j < i; ++j ) {
			const temporalIqmSequenceEntry_t *prior = &sequence->entries[j];
			qboolean sameIndex = prior->facts.entityIndex
				== entry->facts.entityIndex;
			qboolean sameIdentity = IdentityEqual( &prior->facts.identity,
				&entry->facts.identity );
			if ( sameIndex != sameIdentity ) return qfalse;
			if ( sameIndex ) {
				if ( !EntityTupleEqual( &prior->facts, &entry->facts ) ) return qfalse;
				recordIndex = prior->recordIndex; break;
			}
		}
		if ( recordIndex == UINT32_MAX ) {
			if ( entityCount >= TEMPORAL_IQM_MAX_RECORDS ) return qfalse;
			recordIndex = entityCount++;
		}
		if ( entry->recordIndex != recordIndex ) return qfalse;
		digest = EntryDigest( digest, entry );
	}
	if ( !digest ) digest = 1u;
	return sequence->entityCount == entityCount
		&& sequence->orderedDigest == digest ? qtrue : qfalse;
}

qboolean R_TemporalIqmSequenceExact(
		const temporalIqmSequence_t *a, const temporalIqmSequence_t *b ) {
	if ( !SequenceValid( a ) || !SequenceValid( b )
			|| a->authority.token != b->authority.token
			|| a->authority.frameId != b->authority.frameId
			|| a->authority.worldIndex != b->authority.worldIndex
			|| a->authority.commandSlot != b->authority.commandSlot
			|| a->authority.frameCount != b->authority.frameCount
			|| a->drawCount != b->drawCount || a->entityCount != b->entityCount
			|| a->orderedDigest != b->orderedDigest ) return qfalse;
	for ( uint32_t i = 0; i < a->drawCount; ++i )
		if ( a->entries[i].recordIndex != b->entries[i].recordIndex
				|| !FactsEqual( &a->entries[i].facts, &b->entries[i].facts ) )
			return qfalse;
	return qtrue;
}

qboolean R_TemporalIqmSequenceEntryExact(
		const temporalIqmSequence_t *sequence, uint32_t ordinal,
		const temporalIqmSequenceEntry_t *entry ) {
	return SequenceValid( sequence ) && entry && ordinal < sequence->drawCount
		&& sequence->entries[ordinal].recordIndex == entry->recordIndex
		&& FactsEqual( &sequence->entries[ordinal].facts, &entry->facts )
		? qtrue : qfalse;
}

qboolean R_TemporalIqmSequenceEntryEqual(
		const temporalIqmSequenceEntry_t *a,
		const temporalIqmSequenceEntry_t *b ) {
	return a && b && a->recordIndex == b->recordIndex
		&& FactsEqual( &a->facts, &b->facts ) ? qtrue : qfalse;
}

qboolean R_TemporalIqmSequenceVerifierBegin(
		temporalIqmSequenceVerifier_t *verifier,
		const temporalIqmSequence_t *expected ) {
	temporalIqmSequenceVerifier_t candidate;
	if ( !verifier || !expected
			|| !R_TemporalIqmSequenceExact( expected, expected ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.expected = expected;
	candidate.expectedDigest = expected->orderedDigest;
	candidate.active = qtrue;
	*verifier = candidate;
	return qtrue;
}

qboolean R_TemporalIqmSequenceVerifierPeek(
		temporalIqmSequenceVerifier_t *verifier,
		const temporalIqmDrawFacts_t *observed,
		temporalIqmSequenceEntry_t *outExpected ) {
	temporalIqmSequenceEntry_t candidate;
	if ( !verifier || verifier->active != qtrue ) return qfalse;
	if ( !observed || !outExpected || verifier->poisoned
			|| !verifier->expected
			|| !R_TemporalIqmSequenceExact(
				verifier->expected, verifier->expected )
			|| verifier->expectedDigest != verifier->expected->orderedDigest
			|| verifier->cursor >= verifier->expected->drawCount ) {
		verifier->poisoned = qtrue; return qfalse;
	}
	candidate = verifier->expected->entries[verifier->cursor];
	if ( !FactsEqual( &candidate.facts, observed ) ) {
		verifier->poisoned = qtrue;
		return qfalse;
	}
	*outExpected = candidate;
	return qtrue;
}

qboolean R_TemporalIqmSequenceVerifierCommit(
		temporalIqmSequenceVerifier_t *verifier,
		const temporalIqmSequenceEntry_t *expected ) {
	if ( !verifier || verifier->active != qtrue ) return qfalse;
	if ( !expected || verifier->poisoned
			|| !verifier->expected
			|| !R_TemporalIqmSequenceExact(
				verifier->expected, verifier->expected )
			|| verifier->expectedDigest != verifier->expected->orderedDigest
			|| verifier->cursor >= verifier->expected->drawCount
			|| !R_TemporalIqmSequenceEntryEqual(
				&verifier->expected->entries[verifier->cursor], expected ) ) {
		verifier->poisoned = qtrue;
		return qfalse;
	}
	verifier->cursor++;
	return qtrue;
}

qboolean R_TemporalIqmSequenceVerifierFinish(
		temporalIqmSequenceVerifier_t *verifier ) {
	if ( !verifier || verifier->active != qtrue || verifier->poisoned
			|| !verifier->expected
			|| !R_TemporalIqmSequenceExact(
				verifier->expected, verifier->expected )
			|| verifier->expectedDigest != verifier->expected->orderedDigest
			|| verifier->cursor != verifier->expected->drawCount ) return qfalse;
	verifier->active = qfalse;
	return qtrue;
}
