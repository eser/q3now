// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_motion.h"

#include <math.h>
#include <string.h>

#define TEMPORAL_MOTION_CLIP_W_EPSILON 1.0e-6f
// modtype_t is renderer-private; keep the pure host module decoupled while
// pinning its stable MOD_BAD=0, MOD_BRUSH=1 prefix in the source policy test.
#define TEMPORAL_MOTION_MOD_BRUSH_MODEL_TYPE 1u

static qboolean FiniteFloats( const float *values, size_t count ) {
	if ( !values ) return qfalse;
	for ( size_t i = 0; i < count; ++i ) {
		if ( !isfinite( values[i] ) ) return qfalse;
	}
	return qtrue;
}

static void CanonicalizeSignedZero( float *values, size_t count ) {
	for ( size_t i = 0; i < count; ++i ) {
		if ( values[i] == 0.0f ) values[i] = 0.0f;
	}
}

// Matches myGlMultMatrix exactly.  Arrays are column-major, so this computes
// logical B * A while retaining the engine's historical argument order.
static qboolean Multiply( const float a[16], const float b[16], float out[16] ) {
	float candidate[16];
	for ( int i = 0; i < 4; ++i ) {
		for ( int j = 0; j < 4; ++j ) {
			candidate[i * 4 + j] =
				a[i * 4 + 0] * b[0 * 4 + j]
				+ a[i * 4 + 1] * b[1 * 4 + j]
				+ a[i * 4 + 2] * b[2 * 4 + j]
				+ a[i * 4 + 3] * b[3 * 4 + j];
		}
	}
	if ( !FiniteFloats( candidate, 16 ) ) return qfalse;
	memcpy( out, candidate, sizeof( candidate ) );
	return qtrue;
}

static qboolean CameraPoseValid( const temporalCameraPose_t *pose ) {
	return pose && FiniteFloats( pose->projection, 16 )
		&& FiniteFloats( pose->worldModel, 16 )
		&& FiniteFloats( pose->origin, 3 )
		&& FiniteFloats( pose->axis, 9 );
}

static qboolean CameraReceiptValid( const temporalCameraPoseReceipt_t *receipt ) {
	return receipt && receipt->valid == qtrue && receipt->previousValid == qtrue
		&& receipt->frameId != 0 && receipt->previousFrameId != 0
		&& receipt->frameId == receipt->previousFrameId + 1u
		&& CameraPoseValid( &receipt->current )
		&& CameraPoseValid( &receipt->previous );
}

static qboolean EntityPoseValid( const temporalEntityPose_t *pose ) {
	return pose && pose->hModel > 0 && pose->modelToken != 0
		&& pose->modelType != 0 && pose->modelTopology != 0
		&& isfinite( pose->backlerp )
		&& FiniteFloats( pose->origin, 3 )
		&& FiniteFloats( pose->axis, 9 )
		&& pose->nonNormalizedAxes <= 1u;
}

static qboolean ModelCompatible( const temporalEntityPose_t *current,
		const temporalEntityPose_t *previous ) {
	return current->hModel == previous->hModel
		&& current->modelToken == previous->modelToken
		&& current->modelDataToken == previous->modelDataToken
		&& current->modelType == previous->modelType
		&& current->modelTopology == previous->modelTopology
		&& current->modelAllocationGeneration
			== previous->modelAllocationGeneration
		&& current->modelContentDigest == previous->modelContentDigest;
}

static qboolean EntityReceiptValid( const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity ) {
	return entity && entity->valid == qtrue && entity->previousValid == qtrue
		&& entity->frameId == camera->frameId
		&& entity->previousFrameId == camera->previousFrameId
		&& RefEntityMotion_IsValid( &entity->identity )
		&& entity->identity.generation != 0
		&& entity->identity.ownerId < MAX_GENTITIES
		&& entity->identity.role > REF_ENTITY_MOTION_ROLE_NONE
		&& entity->identity.role < REF_ENTITY_MOTION_ROLE_COUNT
		&& EntityPoseValid( &entity->current )
		&& EntityPoseValid( &entity->previous )
		&& entity->current.modelType == TEMPORAL_MOTION_MOD_BRUSH_MODEL_TYPE
		&& ModelCompatible( &entity->current, &entity->previous );
}

static void EntityModelMatrix( const temporalEntityPose_t *pose, float out[16] ) {
	out[0] = pose->axis[0]; out[4] = pose->axis[3]; out[8] = pose->axis[6];
	out[1] = pose->axis[1]; out[5] = pose->axis[4]; out[9] = pose->axis[7];
	out[2] = pose->axis[2]; out[6] = pose->axis[5]; out[10] = pose->axis[8];
	out[12] = pose->origin[0]; out[13] = pose->origin[1]; out[14] = pose->origin[2];
	out[3] = out[7] = out[11] = 0.0f;
	out[15] = 1.0f;
}

static qboolean BuildOne( const temporalCameraPose_t *camera,
		const temporalEntityPose_t *entity, float out[16] ) {
	float candidate[16];
	if ( !camera || !out ) return qfalse;
	if ( entity ) {
		if ( !R_TemporalMotionBuildCanonicalEntityMvp( camera->projection,
				camera->worldModel, entity, candidate ) ) return qfalse;
		memcpy( out, candidate, sizeof( candidate ) );
		return qtrue;
	}
	return R_TemporalMotionBuildCanonicalMvp( camera->worldModel,
		camera->projection, out );
}

qboolean R_TemporalMotionBuildCanonicalMvp( const float modelView[16],
		const float projection[16], float outMvp[16] ) {
	float canonicalProjection[16], candidate[16];
	if ( !modelView || !projection || !outMvp
			|| !FiniteFloats( modelView, 16 )
			|| !FiniteFloats( projection, 16 ) ) return qfalse;
	memcpy( canonicalProjection, projection, sizeof( canonicalProjection ) );
	canonicalProjection[5] = -canonicalProjection[5];
	if ( !Multiply( modelView, canonicalProjection, candidate ) ) return qfalse;
	CanonicalizeSignedZero( candidate, 16 );
	memcpy( outMvp, candidate, sizeof( candidate ) );
	return qtrue;
}

qboolean R_TemporalMotionBuildCanonicalEntityMvp(
		const float projection[16], const float cameraWorldModel[16],
		const temporalEntityPose_t *entity, float outMvp[16] ) {
	float model[16], modelView[16], candidate[16];
	if ( !projection || !cameraWorldModel || !entity || !outMvp
			|| !FiniteFloats( projection, 16 )
			|| !FiniteFloats( cameraWorldModel, 16 )
			|| !EntityPoseValid( entity ) ) return qfalse;
	EntityModelMatrix( entity, model );
	if ( !Multiply( model, cameraWorldModel, modelView )
			|| !R_TemporalMotionBuildCanonicalMvp( modelView, projection,
				candidate ) ) {
		return qfalse;
	}
	memcpy( outMvp, candidate, sizeof( candidate ) );
	return qtrue;
}

temporalMotionOutcome_t R_TemporalMotionClassify(
		const temporalMotionDrawFacts_t *facts ) {
	qboolean supportedGeometry;
	if ( !facts || !facts->visible ) return TEMPORAL_MOTION_PRESERVE;
	if ( facts->ui || facts->transparent || facts->blended || facts->decal
			|| facts->additive || !facts->opaque || facts->polygonOffset
			|| facts->depthHack || facts->crosshair ) return TEMPORAL_MOTION_PRESERVE;
	if ( !facts->depthAuthoritative ) return TEMPORAL_MOTION_PRESERVE;
	if ( facts->alphaTested ) return TEMPORAL_MOTION_DEFER_ATEST;
	supportedGeometry = facts->geometry == TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC
		|| facts->geometry == TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID;
	if ( supportedGeometry && !facts->vertexDeformed && !facts->sky ) {
		return TEMPORAL_MOTION_WRITE_VALID;
	}
	return TEMPORAL_MOTION_INVALIDATE_OPAQUE;
}

qboolean R_TemporalMotionBuildMatrices( temporalMotionGeometry_t geometry,
		const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity,
		temporalMotionMatrices_t *outMatrices ) {
	temporalMotionMatrices_t candidate;
	if ( !outMatrices || !CameraReceiptValid( camera ) ) return qfalse;
	if ( geometry == TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC ) {
		if ( entity ) return qfalse;
		if ( !BuildOne( &camera->current, NULL, candidate.currentMvp )
				|| !BuildOne( &camera->previous, NULL, candidate.previousMvp ) ) {
			return qfalse;
		}
	} else if ( geometry == TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID ) {
		if ( !EntityReceiptValid( camera, entity ) ) return qfalse;
		if ( !BuildOne( &camera->current, &entity->current, candidate.currentMvp )
				|| !BuildOne( &camera->previous, &entity->previous,
					candidate.previousMvp ) ) {
			return qfalse;
		}
	} else {
		return qfalse;
	}
	CanonicalizeSignedZero( candidate.currentMvp, 16 );
	CanonicalizeSignedZero( candidate.previousMvp, 16 );
	*outMatrices = candidate;
	return qtrue;
}

static qboolean TransformPoint( const float matrix[16], const float point[3],
		float clip[4] ) {
	for ( int i = 0; i < 4; ++i ) {
		clip[i] = point[0] * matrix[i + 0 * 4]
			+ point[1] * matrix[i + 1 * 4]
			+ point[2] * matrix[i + 2 * 4]
			+ matrix[i + 3 * 4];
	}
	return FiniteFloats( clip, 4 );
}

qboolean R_TemporalMotionEvaluatePoint(
		const temporalMotionMatrices_t *matrices, const float point[3],
		temporalMotionSample_t *outSample ) {
	temporalMotionSample_t candidate;
	float currentNdc[2], previousNdc[2];
	if ( !matrices || !point || !outSample
			|| !FiniteFloats( matrices->currentMvp, 16 )
			|| !FiniteFloats( matrices->previousMvp, 16 )
			|| !FiniteFloats( point, 3 ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	if ( !TransformPoint( matrices->currentMvp, point, candidate.currentClip )
			|| !TransformPoint( matrices->previousMvp, point,
				candidate.previousClip )
			|| candidate.currentClip[3] <= TEMPORAL_MOTION_CLIP_W_EPSILON
			|| candidate.previousClip[3] <= TEMPORAL_MOTION_CLIP_W_EPSILON ) {
		return qfalse;
	}
	for ( int i = 0; i < 2; ++i ) {
		currentNdc[i] = candidate.currentClip[i] / candidate.currentClip[3];
		previousNdc[i] = candidate.previousClip[i] / candidate.previousClip[3];
		candidate.currentUv[i] = currentNdc[i] * 0.5f + 0.5f;
		candidate.previousUv[i] = previousNdc[i] * 0.5f + 0.5f;
		candidate.velocity[i] = candidate.currentUv[i] - candidate.previousUv[i];
	}
	if ( !FiniteFloats( currentNdc, 2 ) || !FiniteFloats( previousNdc, 2 )
			|| !FiniteFloats( candidate.currentUv, 2 )
			|| !FiniteFloats( candidate.previousUv, 2 )
			|| !FiniteFloats( candidate.velocity, 2 ) ) return qfalse;
	CanonicalizeSignedZero( candidate.currentClip, 4 );
	CanonicalizeSignedZero( candidate.previousClip, 4 );
	CanonicalizeSignedZero( candidate.currentUv, 2 );
	CanonicalizeSignedZero( candidate.previousUv, 2 );
	CanonicalizeSignedZero( candidate.velocity, 2 );
	*outSample = candidate;
	return qtrue;
}
