// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
//
// Backend-neutral temporal-frame sequencing.  This contract owns only jitter,
// history ping-pong identity and invalidation.  Renderers own matrices, motion
// vectors, history images and the eventual resolve algorithm.

#ifndef WIRED_RAL_TEMPORAL_H
#define WIRED_RAL_TEMPORAL_H

#include <stdint.h>

#define RAL_TEMPORAL_JITTER_PHASES 8u

typedef enum {
	RAL_TEMPORAL_RESET_NONE       = 0,
	RAL_TEMPORAL_RESET_ACTIVATED  = 1u << 0,
	RAL_TEMPORAL_RESET_DISABLED   = 1u << 1,
	RAL_TEMPORAL_RESET_EXTENT     = 1u << 2,
	RAL_TEMPORAL_RESET_VIEW       = 1u << 3,
	RAL_TEMPORAL_RESET_TOPOLOGY   = 1u << 4,
	RAL_TEMPORAL_RESET_CAMERA_CUT = 1u << 5,
	RAL_TEMPORAL_RESET_FRAME_GAP  = 1u << 6
} ralTemporalResetFlags_t;

typedef struct {
	uint64_t frameId;
	uint32_t width;
	uint32_t height;
	uint32_t viewId;
	uint32_t topologyEpoch;
	uint8_t  enabled;
	uint8_t  cameraCut;
} ralTemporalFrameInput_t;

typedef struct {
	uint64_t frameId;
	uint32_t generation;
	uint32_t resetMask;
	uint8_t  enabled;
	uint8_t  historyValid;
	uint8_t  historyReadIndex;
	uint8_t  historyWriteIndex;
	uint8_t  jitterPhase;
	float    sceneJitterPixels[2];
	float    sceneJitterUv[2];
	float    previousJitterPixels[2];
	float    uiJitterPixels[2];
} ralTemporalFramePlan_t;

typedef struct {
	uint64_t lastCommittedFrame;
	uint32_t generation;
	uint32_t width;
	uint32_t height;
	uint32_t viewId;
	uint32_t topologyEpoch;
	uint8_t  active;
	uint8_t  historyValid;
	uint8_t  lastWriteIndex;
	uint8_t  nextJitterPhase;
	float    previousJitterPixels[2];

	uint8_t  pending;
	ralTemporalFramePlan_t pendingPlan;
	uint32_t pendingWidth;
	uint32_t pendingHeight;
	uint32_t pendingViewId;
	uint32_t pendingTopologyEpoch;
} ralTemporalState_t;

void Ral_TemporalInit( ralTemporalState_t *state );

// Plans one frame without publishing it as valid history.  The returned scene
// jitter is expressed both in physical pixels and normalized UV units; the
// renderer applies its backend/projection Y convention.  UI jitter is always
// zero so HUD/menu composition remains outside temporal sampling.  Invalid
// input or a second Begin while a plan is pending leaves state/output untouched.
int Ral_TemporalBeginFrame( ralTemporalState_t *state,
		const ralTemporalFrameInput_t *input, ralTemporalFramePlan_t *outPlan );

// Commit advances the jitter sequence and makes the written history slot
// readable.  Cancel discards the pending plan without advancing any identity.
// Both operations require the exact pending frame ID.
int Ral_TemporalCommitFrame( ralTemporalState_t *state, uint64_t frameId );
int Ral_TemporalCancelFrame( ralTemporalState_t *state, uint64_t frameId );

#endif // WIRED_RAL_TEMPORAL_H
