// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_MOTION_PAYLOAD_H
#define WIRED_TR_TEMPORAL_MOTION_PAYLOAD_H

#include "tr_temporal_motion.h"
#include "../renderer/ral/ral_resource.h"

#include <stddef.h>

#define TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES 4u
#define TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS 65536u

// Exact std430 record consumed by a future temporal vertex variant.  The
// current/previous matrices are unjittered receipt matrices.  Outcome selects
// WRITE_VALID or INVALIDATE_OPAQUE; the remaining values never enter this
// buffer.  The reserved tail makes the record an exact 9-vec4 / 144-byte stride.
typedef struct {
	float currentMvp[16];
	float previousMvp[16];
	uint32_t outcome;
	uint32_t reserved[3];
} temporalMotionGpuPayload_t;

_Static_assert( offsetof( temporalMotionGpuPayload_t, currentMvp ) == 0,
	"temporal payload current MVP offset drift" );
_Static_assert( offsetof( temporalMotionGpuPayload_t, previousMvp ) == 64,
	"temporal payload previous MVP offset drift" );
_Static_assert( offsetof( temporalMotionGpuPayload_t, outcome ) == 128,
	"temporal payload outcome offset drift" );
_Static_assert( sizeof( temporalMotionGpuPayload_t ) == 144,
	"temporal payload std430 stride drift" );
_Static_assert( TEMPORAL_MOTION_WRITE_VALID == 1,
	"temporal payload WRITE_VALID GPU ABI drift" );
_Static_assert( TEMPORAL_MOTION_INVALIDATE_OPAQUE == 2,
	"temporal payload INVALIDATE_OPAQUE GPU ABI drift" );

typedef struct {
	ralBuffer_t *buffer;
	ralBindGroup_t *bindGroup;
	void *mapped;
	const ralBuffer_t *entityBuffer;
	uint32_t entityAllocationGeneration;
	uint32_t capacity;
	uint32_t allocationGeneration;
	uint32_t lastSlot;
	qboolean hasAppends;
	qboolean resetAfterFence;
	qboolean begun;
	qboolean ready;
} temporalMotionPayloadFrame_t;

typedef struct {
	ralBackend_t *backend;
	ralBindGroupLayout_t *layout;
	temporalMotionPayloadFrame_t frames[TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES];
	uint32_t frameCount;
	uint32_t layoutAllocationGeneration;
	qboolean ready;
} temporalMotionPayloadOwner_t;

void R_TemporalMotionPayloadInit( temporalMotionPayloadOwner_t *owner );

// Materializes one command-frame slot.  The composite set-3 shape is exact:
// binding 0 is the caller's existing entity-matrix STORAGE_BUFFER and binding 1
// is this owner's temporal STORAGE_BUFFER, both VERTEX-visible.  Capacity is in
// 144-byte records.  Replacement is candidate-first and preserves the live
// generation on every failure.  Changing entityBuffer rebuilds the bind group
// even when the temporal buffer can be reused.
qboolean R_TemporalMotionPayloadEnsure( temporalMotionPayloadOwner_t *owner,
	ralBackend_t *backend, uint32_t frameCount, uint32_t frameIndex,
	uint32_t capacity, const ralBuffer_t *entityBuffer,
	uint32_t entityAllocationGeneration );

// Reusing a command-frame slot is legal only after its submission fence has
// completed.  ResetAfterFence publishes that authority; BeginFrame consumes it.
qboolean R_TemporalMotionPayloadResetAfterFence(
	temporalMotionPayloadOwner_t *owner, uint32_t frameIndex );
qboolean R_TemporalMotionPayloadBeginFrame(
	temporalMotionPayloadOwner_t *owner, uint32_t frameIndex );

// Drops the composite bind group after the frame fence and before the borrowed
// entity buffer is destroyed/replaced. The temporal buffer remains reusable;
// a later Ensure rebuilds only the group when capacity still suffices.
qboolean R_TemporalMotionPayloadDetachEntityBuffer(
	temporalMotionPayloadOwner_t *owner, uint32_t frameIndex,
	const ralBuffer_t *entityBuffer, uint32_t entityAllocationGeneration );

// Writes at the absolute entity-matrix slot used as firstInstance.  Slots must
// increase strictly within a begun frame; gaps are intentionally allowed.
// WRITE_VALID requires finite matrices. INVALIDATE_OPAQUE requires matrices ==
// NULL and authors canonical all-zero matrices. Other outcomes reject.  The
// mapped record, owner state and outSlot are unchanged on failure.
qboolean R_TemporalMotionPayloadAppendAt(
	temporalMotionPayloadOwner_t *owner, uint32_t frameIndex,
	uint32_t absoluteEntMatSlot, temporalMotionOutcome_t outcome,
	const temporalMotionMatrices_t *matrices, uint32_t *outSlot );

ralBindGroup_t *R_TemporalMotionPayloadGetBindGroup(
	const temporalMotionPayloadOwner_t *owner, uint32_t frameIndex );

// Read-only layout publication for the inert temporal pipeline-layout owner.
// The generation is nonzero, monotonic across Release/re-enable, and changes
// exactly when a new composite layout is published. Outputs are atomic.
qboolean R_TemporalMotionPayloadGetLayout(
	const temporalMotionPayloadOwner_t *owner,
	const ralBindGroupLayout_t **outLayout, uint32_t *outGeneration );

void R_TemporalMotionPayloadRelease( temporalMotionPayloadOwner_t *owner );

#endif
