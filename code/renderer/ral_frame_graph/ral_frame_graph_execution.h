// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Generation-bound physical execution schedule for a compiled frame graph.
// This layer resolves logical graph resources to RAL wrappers and produces
// deterministic transition, ownership-transfer and submission facts. It does
// not record or submit commands; the product execution adapter consumes this
// exact schedule in a later leaf.

#ifndef WIRED_RAL_FRAME_GRAPH_EXECUTION_H
#define WIRED_RAL_FRAME_GRAPH_EXECUTION_H

#include "ral_frame_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION 1u
#define RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT 3u
#define RAL_FRAME_GRAPH_MAX_SUBMISSION_WAITS RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT

typedef struct {
	uint32_t resourceId;
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	ralFrameGraphResourceKind_t resourceKind;
	ralBuffer_t *buffer;
	ralTexture_t *texture;
	// Whole-resource ranges only. The authoritative owner supplies the exact
	// capacity/subresource facts for the generation carried above.
	uint64_t bufferSize;
	ralTextureAspectFlags_t textureAspects;
	uint32_t textureMipLevels;
	uint32_t textureArrayLayers;
} ralFrameGraphResourceBinding_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	const ralBackend_t *backendIdentity;
	// Borrowed and byte-immutable until every derived execution receipt is
	// retired. Exact validation never dereferences receipt-carried identities.
	const ralFrameGraphPlan_t *graphPlan;
	// Map logical GRAPHICS/COMPUTE/TRANSFER domains to the physical RAL queue
	// used by the backend. A WebGPU-shaped backend maps all three to one queue.
	ralQueueType_t physicalQueueForLogical[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	// Last signal value already published on each physical queue timeline.
	uint64_t timelineBaseValues[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	ralFrameGraphResourceBinding_t bindings[RAL_FRAME_GRAPH_MAX_RESOURCES];
	uint32_t bindingCount;
} ralFrameGraphExecutionDescription_t;

typedef struct {
	ralQueueType_t sourcePhysicalQueue;
	uint64_t value;
} ralFrameGraphSubmissionWait_t;

typedef struct {
	uint32_t passId;
	uint32_t topologicalIndex;
	ralQueueType_t logicalQueue;
	ralQueueType_t physicalQueue;
	uint32_t submissionBatchIndex;
	uint32_t firstPreBufferTransition;
	uint32_t preBufferTransitionCount;
	uint32_t firstPreTextureTransition;
	uint32_t preTextureTransitionCount;
	uint32_t firstAcquireBufferTransition;
	uint32_t acquireBufferTransitionCount;
	uint32_t firstAcquireTextureTransition;
	uint32_t acquireTextureTransitionCount;
	uint32_t firstReleaseBufferTransition;
	uint32_t releaseBufferTransitionCount;
	uint32_t firstReleaseTextureTransition;
	uint32_t releaseTextureTransitionCount;
} ralFrameGraphExecutionPass_t;

typedef struct {
	uint32_t firstExecutionPass;
	uint32_t executionPassCount;
	ralQueueType_t physicalQueue;
	ralFrameGraphSubmissionWait_t waits[RAL_FRAME_GRAPH_MAX_SUBMISSION_WAITS];
	uint32_t waitCount;
	uint64_t signalValue;
} ralFrameGraphSubmissionBatch_t;

typedef struct {
	uint32_t schemaVersion;
	ralFrameGraphExecutionDescription_t description;
	ralFrameGraphExecutionPass_t passes[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t passCount;
	ralBufferTransition_t preBufferTransitions[RAL_FRAME_GRAPH_MAX_TRANSITIONS];
	uint32_t preBufferTransitionCount;
	ralTextureTransition_t preTextureTransitions[RAL_FRAME_GRAPH_MAX_TRANSITIONS];
	uint32_t preTextureTransitionCount;
	ralBufferTransition_t acquireBufferTransitions[RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS];
	uint32_t acquireBufferTransitionCount;
	ralTextureTransition_t acquireTextureTransitions[RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS];
	uint32_t acquireTextureTransitionCount;
	ralBufferTransition_t releaseBufferTransitions[RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS];
	uint32_t releaseBufferTransitionCount;
	ralTextureTransition_t releaseTextureTransitions[RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS];
	uint32_t releaseTextureTransitionCount;
	ralFrameGraphSubmissionBatch_t submissionBatches[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t submissionBatchCount;
	uint64_t timelineFinalValues[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	qboolean ready;
} ralFrameGraphExecutionPlan_t;

qboolean RalFrameGraphExecution_Compile(
	const ralFrameGraphExecutionDescription_t *description,
	ralFrameGraphExecutionPlan_t *outPlan );
qboolean RalFrameGraphExecution_PlanExact(
	const ralFrameGraphExecutionPlan_t *a,
	const ralFrameGraphExecutionPlan_t *b );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_FRAME_GRAPH_EXECUTION_H
