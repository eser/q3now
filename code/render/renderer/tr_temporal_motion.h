// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_MOTION_H
#define WIRED_TR_TEMPORAL_MOTION_H

#include "../frontend/render_temporal_entity.h"


typedef struct {
	float currentMvp[16];
	float previousMvp[16];
} temporalMotionMatrices_t;

// Lowest shared primitive for ordinary and temporal IQM raster transforms.
// `modelView` is already composed. The helper copies `projection`, performs
// exactly one Vulkan Y flip, and multiplies in historical myGlMultMatrix order.
qboolean R_TemporalMotionBuildCanonicalMvp( const float modelView[16],
	const float projection[16], float outMvp[16] );

// Shared Vulkan-clip-space entity MVP authority. `projection` may be either
// the canonical unjittered camera projection or the exact current raster
// projection. The helper copies it, applies the renderer's single Y flip, and
// evaluates entity-model * camera-world * projection in the historical
// myGlMultMatrix order. Failure leaves `outMvp` byte-identical.
qboolean R_TemporalMotionBuildCanonicalEntityMvp(
	const float projection[16], const float cameraWorldModel[16],
	const temporalEntityPose_t *entity, float outMvp[16] );

typedef struct {
	float currentClip[4];
	float previousClip[4];
	float currentUv[2];
	float previousUv[2];
	float velocity[2];
} temporalMotionSample_t;


// Builds Vulkan-clip-space current/previous MVPs from unjittered receipts.
// WORLD_STATIC requires entity == NULL.  MOD_BRUSH_RIGID requires an exact,
// model-compatible entity receipt; brush geometry is frame-tuple-independent.
// Outputs are atomic.
qboolean R_TemporalMotionBuildMatrices( temporalMotionGeometry_t geometry,
	const temporalCameraPoseReceipt_t *camera,
	const temporalEntityPoseReceipt_t *entity,
	temporalMotionMatrices_t *outMatrices );

// Evaluates one shared local/world point through both matrices.  UVs are
// top-left-oriented and velocity is currentUv - previousUv.  Outputs are atomic.
qboolean R_TemporalMotionEvaluatePoint(
	const temporalMotionMatrices_t *matrices, const float point[3],
	temporalMotionSample_t *outSample );

#endif
