// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_MOTION_H
#define WIRED_TR_TEMPORAL_MOTION_H

#include "tr_temporal_entity_cache.h"

typedef enum {
	TEMPORAL_MOTION_PRESERVE = 0,
	TEMPORAL_MOTION_WRITE_VALID,
	TEMPORAL_MOTION_INVALIDATE_OPAQUE,
	TEMPORAL_MOTION_DEFER_ATEST
} temporalMotionOutcome_t;

typedef enum {
	TEMPORAL_MOTION_GEOMETRY_UNSUPPORTED = 0,
	TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC,
	TEMPORAL_MOTION_GEOMETRY_MOD_BRUSH_RIGID
} temporalMotionGeometry_t;

typedef struct {
	temporalMotionGeometry_t geometry;
	qboolean visible;
	qboolean opaque;
	qboolean depthAuthoritative;
	qboolean vertexDeformed;
	qboolean alphaTested;
	qboolean blended;
	qboolean transparent;
	qboolean decal;
	qboolean additive;
	qboolean ui;
	qboolean sky;
	qboolean polygonOffset;
	qboolean depthHack;
	qboolean crosshair;
} temporalMotionDrawFacts_t;

typedef struct {
	float currentMvp[16];
	float previousMvp[16];
} temporalMotionMatrices_t;

typedef struct {
	float currentClip[4];
	float previousClip[4];
	float currentUv[2];
	float previousUv[2];
	float velocity[2];
} temporalMotionSample_t;

// Pure per-draw disposition.  This does not inspect renderer-private shader or
// model structures: the eventual adapter owns the exact fact translation.
temporalMotionOutcome_t R_TemporalMotionClassify(
	const temporalMotionDrawFacts_t *facts );

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
