// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Deterministic frame-graph compiler above the backend-neutral RAL.
// The graph owns logical scheduling facts only. Physical RAL resources and
// command buffers are joined by the later execution layer.

#ifndef WIRED_RAL_FRAME_GRAPH_H
#define WIRED_RAL_FRAME_GRAPH_H

#include "../ral_transition.h"
#include "../ral_transient.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_GRAPH_SCHEMA_VERSION 1u
#define RAL_FRAME_GRAPH_MAX_RESOURCES 32u
#define RAL_FRAME_GRAPH_MAX_PASSES 32u
#define RAL_FRAME_GRAPH_MAX_USES 128u
#define RAL_FRAME_GRAPH_MAX_EXPLICIT_DEPENDENCIES 64u
#define RAL_FRAME_GRAPH_MAX_EDGES 512u
#define RAL_FRAME_GRAPH_MAX_TRANSITIONS RAL_FRAME_GRAPH_MAX_USES
#define RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS RAL_FRAME_GRAPH_MAX_USES

typedef enum {
	RAL_FRAME_GRAPH_RESOURCE_BUFFER = 1,
	RAL_FRAME_GRAPH_RESOURCE_TEXTURE
} ralFrameGraphResourceKind_t;

typedef enum {
	RAL_FRAME_GRAPH_RESOURCE_EXTERNAL = 1,
	RAL_FRAME_GRAPH_RESOURCE_PERSISTENT,
	RAL_FRAME_GRAPH_RESOURCE_TRANSIENT
} ralFrameGraphResourceLifetime_t;

typedef enum {
	RAL_FRAME_GRAPH_ACCESS_READ = 1,
	RAL_FRAME_GRAPH_ACCESS_WRITE,
	RAL_FRAME_GRAPH_ACCESS_READ_WRITE
} ralFrameGraphAccess_t;

typedef enum {
	RAL_FRAME_GRAPH_HAZARD_EXPLICIT = 1u << 0,
	RAL_FRAME_GRAPH_HAZARD_RAW = 1u << 1,
	RAL_FRAME_GRAPH_HAZARD_WAR = 1u << 2,
	RAL_FRAME_GRAPH_HAZARD_WAW = 1u << 3,
	RAL_FRAME_GRAPH_HAZARD_STATE = 1u << 4,
	RAL_FRAME_GRAPH_HAZARD_QUEUE_HANDOFF = 1u << 5
} ralFrameGraphHazardFlagBits_t;
typedef uint32_t ralFrameGraphHazardFlags_t;

typedef struct {
	uint32_t id;
	uint64_t generation;
	uintptr_t identity;
	ralFrameGraphResourceKind_t kind;
	ralFrameGraphResourceLifetime_t lifetime;
	uint64_t compatibilityKey;
	uint64_t size;
	uint64_t alignment;
	qboolean allowAlias;
	ralResourceState_t initialState;
	ralQueueType_t initialQueue;
} ralFrameGraphResource_t;

typedef struct {
	uint32_t id;
	ralQueueType_t queue;
} ralFrameGraphPass_t;

typedef struct {
	uint32_t passId;
	uint32_t resourceId;
	ralFrameGraphAccess_t access;
	ralResourceState_t state;
} ralFrameGraphUse_t;

typedef struct {
	uint32_t sourcePassId;
	uint32_t destinationPassId;
} ralFrameGraphExplicitDependency_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	ralBackendType_t backendType;
	ralFrameGraphResource_t resources[RAL_FRAME_GRAPH_MAX_RESOURCES];
	uint32_t resourceCount;
	ralFrameGraphPass_t passes[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t passCount;
	ralFrameGraphUse_t uses[RAL_FRAME_GRAPH_MAX_USES];
	uint32_t useCount;
	// Pass declaration order is the canonical order for uses of one resource.
	// Explicit dependencies may reorder only otherwise independent passes.
	ralFrameGraphExplicitDependency_t
		explicitDependencies[RAL_FRAME_GRAPH_MAX_EXPLICIT_DEPENDENCIES];
	uint32_t explicitDependencyCount;
	ralTransientPolicy_t transientPolicy;
} ralFrameGraphDescription_t;

typedef struct {
	uint32_t sourcePassId;
	uint32_t destinationPassId;
	uint32_t resourceMask;
	ralFrameGraphHazardFlags_t hazards;
} ralFrameGraphEdge_t;

typedef struct {
	uint32_t resourceId;
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	ralFrameGraphResourceKind_t resourceKind;
	uint32_t sourcePassId;
	uint32_t destinationPassId;
	ralResourceState_t before;
	ralResourceState_t after;
	ralQueueType_t sourceQueue;
	ralQueueType_t destinationQueue;
} ralFrameGraphTransition_t;

typedef struct {
	uint32_t resourceId;
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	ralFrameGraphResourceKind_t resourceKind;
	uint32_t releasePassId;
	uint32_t acquirePassId;
	ralResourceState_t before;
	ralResourceState_t after;
	ralQueueType_t sourceQueue;
	ralQueueType_t destinationQueue;
} ralFrameGraphQueueHandoff_t;

typedef struct {
	uint32_t schemaVersion;
	ralFrameGraphDescription_t description;
	ralFrameGraphEdge_t edges[RAL_FRAME_GRAPH_MAX_EDGES];
	uint32_t edgeCount;
	uint32_t topologicalPassIds[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t topologicalPassCount;
	ralFrameGraphTransition_t transitions[RAL_FRAME_GRAPH_MAX_TRANSITIONS];
	uint32_t transitionCount;
	ralFrameGraphQueueHandoff_t queueHandoffs[RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS];
	uint32_t queueHandoffCount;
	ralTransientRequest_t transientRequests[RAL_TRANSIENT_MAX_REQUESTS];
	uint32_t transientRequestCount;
	ralTransientPlan_t transientPlan;
	qboolean ready;
} ralFrameGraphPlan_t;

qboolean RalFrameGraph_Compile( const ralFrameGraphDescription_t *description,
	ralFrameGraphPlan_t *outPlan );
qboolean RalFrameGraph_PlanExact( const ralFrameGraphPlan_t *a,
	const ralFrameGraphPlan_t *b );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_FRAME_GRAPH_H
