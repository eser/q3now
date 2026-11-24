// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_TEMPORAL_ENTITY_H
#define WIRED_RENDER_TEMPORAL_ENTITY_H

#include "../../qcommon/q_shared.h"
#include "tr_public.h"
#include "tr_types.h"

typedef struct {
	qhandle_t hModel;
	uintptr_t modelToken;
	uintptr_t modelDataToken;
	uint32_t modelType;
	uint32_t modelTopology;
	uint32_t modelAllocationGeneration;
	uint64_t modelContentDigest;
	int32_t frame;
	int32_t oldframe;
	float backlerp;
	float origin[3];
	float axis[9];
	uint32_t nonNormalizedAxes;
} temporalEntityPose_t;

typedef struct {
	uint64_t frameId;
	uint64_t previousFrameId;
	refEntityMotion_t identity;
	temporalEntityPose_t current;
	temporalEntityPose_t previous;
	qboolean valid;
	qboolean previousValid;
} temporalEntityPoseReceipt_t;

typedef struct {
	float projection[16];
	float worldModel[16];
	float origin[3];
	float axis[9];
} temporalCameraPose_t;

typedef struct {
	uint64_t frameId;
	uint64_t previousFrameId;
	temporalCameraPose_t current;
	temporalCameraPose_t previous;
	qboolean valid;
	qboolean previousValid;
} temporalCameraPoseReceipt_t;

void R_TemporalEntityCacheResetAll( void );
void R_TemporalEntityCacheResetWorld( int worldIndex );

// Begins one staged CPU transaction. Repeating the exact same begin is
// idempotent for auxiliary replay of one draw command. A different transaction
// cannot replace a pending one.
qboolean R_TemporalEntityCacheBegin( int worldIndex, uint32_t topologyEpoch,
	uint32_t temporalGeneration, uint64_t frameId );

// Records one actually-visible model pose. The output is atomic. Repeating an
// exact source/key/pose is idempotent; two distinct sources claiming one key
// poison this frame's receipt transaction.
qboolean R_TemporalEntityCacheRecord( int worldIndex, uint64_t frameId,
	uintptr_t sourceToken, const refEntityMotion_t *identity,
	const temporalEntityPose_t *current,
	temporalEntityPoseReceipt_t *outReceipt );

qboolean R_TemporalEntityCacheStageCamera( int worldIndex, uint64_t frameId,
	const temporalCameraPose_t *current,
	temporalCameraPoseReceipt_t *outReceipt );

// `submitted` publishes every staged pose atomically. False (or a poisoned
// transaction) discards staging and leaves committed poses unchanged.
qboolean R_TemporalEntityCacheFinish( int worldIndex, uint64_t frameId,
	qboolean submitted );


/* Per-draw temporal disposition, shared by every backend: whether a draw writes motion (world-static or rigid brush geometry
 * with a committed previous pose), keeps the history beneath it, or invalidates it. */
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
	qboolean deferAlphaTested;
	qboolean blended;
	qboolean transparent;
	qboolean decal;
	qboolean additive;
	qboolean ui;
	qboolean sky;
	qboolean polygonOffset;
	qboolean depthHack;
	qboolean crosshair;
	// Effect mask: draws of translucent surfaces (sort after opaque: water, glass, sprites, beams, explosion models), decals and
	// first-person (depthHack) draws do not inherit the history of the opaque surface beneath them. A blended STAGE of an opaque
	// surface (a lightmap multiply, a detail overlay) is not an effect and stays PRESERVE; they invalidate it, so the resolve filters those pixels spatially instead of reprojecting them.
	qboolean effectMask;
	// The shader animates its texture coordinates over time (tcMod scroll/rotate/stretch/turb, entity-driven translate). Such a
	// surface changes appearance without any geometric motion, so a reprojected history of it is wrong in the same way an effect
	// is (scrolling lava and water, teleporters). Under the effect mask it does not inherit history either.
	qboolean textureAnimated;
} temporalMotionDrawFacts_t;

// Pure per-draw disposition.  This does not inspect renderer-private shader or
// model structures: the eventual adapter owns the exact fact translation.
temporalMotionOutcome_t R_TemporalMotionClassify(
	const temporalMotionDrawFacts_t *facts );

#endif
