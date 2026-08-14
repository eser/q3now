// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_ENTITY_CACHE_H
#define WIRED_TR_TEMPORAL_ENTITY_CACHE_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "../renderercommon/tr_types.h"

typedef struct {
	qhandle_t hModel;
	uintptr_t modelToken;
	uintptr_t modelDataToken;
	uint32_t modelType;
	uint32_t modelTopology;
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

#endif
