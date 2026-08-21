// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_transition.h — backend-neutral resource usage and transition vocabulary.
//
// This surface describes what a resource is used for, never how one native API
// synchronizes it. Vulkan lowers the states to stage/access/layout tuples;
// WebGPU validates encoder/pass usage and relies on implicit transitions. Queue
// values are logical scheduling domains, not native queue-family indices.

#ifndef WIRED_RAL_TRANSITION_H
#define WIRED_RAL_TRANSITION_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RAL_RESOURCE_USAGE_UNDEFINED = 0,
	RAL_RESOURCE_USAGE_COPY_SOURCE,
	RAL_RESOURCE_USAGE_COPY_DESTINATION,
	RAL_RESOURCE_USAGE_VERTEX_BUFFER,
	RAL_RESOURCE_USAGE_INDEX_BUFFER,
	RAL_RESOURCE_USAGE_INDIRECT_BUFFER,
	RAL_RESOURCE_USAGE_UNIFORM_BUFFER,
	RAL_RESOURCE_USAGE_SAMPLED_TEXTURE,
	RAL_RESOURCE_USAGE_STORAGE_READ,
	RAL_RESOURCE_USAGE_STORAGE_WRITE,
	RAL_RESOURCE_USAGE_STORAGE_READ_WRITE,
	RAL_RESOURCE_USAGE_COLOR_ATTACHMENT,
	RAL_RESOURCE_USAGE_DEPTH_STENCIL_READ,
	RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE,
	RAL_RESOURCE_USAGE_HOST_READ,
	RAL_RESOURCE_USAGE_HOST_WRITE,
	RAL_RESOURCE_USAGE_PRESENT,
	RAL_RESOURCE_USAGE_COUNT
} ralResourceUsage_t;

typedef struct {
	ralResourceUsage_t usage;
	// Required only for uniform, sampled and storage usages. Uses RAL_STAGE_*;
	// zero for copy, attachment, host, vertex/index/indirect and present usages.
	uint32_t shaderStages;
} ralResourceState_t;

typedef enum {
	RAL_TEXTURE_ASPECT_COLOR   = 1u << 0,
	RAL_TEXTURE_ASPECT_DEPTH   = 1u << 1,
	RAL_TEXTURE_ASPECT_STENCIL = 1u << 2
} ralTextureAspectFlagBits_t;
typedef uint32_t ralTextureAspectFlags_t;

typedef struct {
	ralBuffer_t       *buffer;
	uint64_t           offset;
	uint64_t           size;
	ralResourceState_t before;
	ralResourceState_t after;
	ralQueueType_t     sourceQueue;
	ralQueueType_t     destinationQueue;
} ralBufferTransition_t;

typedef struct {
	ralTexture_t             *texture;
	ralTextureAspectFlags_t   aspects;
	uint32_t                  baseMipLevel;
	uint32_t                  mipLevelCount;
	uint32_t                  baseArrayLayer;
	uint32_t                  arrayLayerCount;
	ralResourceState_t        before;
	ralResourceState_t        after;
	ralQueueType_t            sourceQueue;
	ralQueueType_t            destinationQueue;
} ralTextureTransition_t;

typedef struct {
	const ralBufferTransition_t  *bufferTransitions;
	uint32_t                      bufferTransitionCount;
	const ralTextureTransition_t *textureTransitions;
	uint32_t                      textureTransitionCount;
} ralResourceTransitionBatch_t;

typedef enum {
	RAL_QUEUE_TRANSFER_RESOURCE_BUFFER = 1,
	RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE = 2
} ralQueueTransferResourceType_t;

// Native-free authority shared by the release and acquire command buffers.
// A backend with one physical queue (notably WebGPU) still validates this
// logical handoff, but does not invent queue-family barriers.
typedef struct {
	const void *resourceIdentity;
	uint64_t generation;
	ralQueueTransferResourceType_t resourceType;
	ralResourceState_t before;
	ralResourceState_t after;
	ralQueueType_t sourceQueue;
	ralQueueType_t destinationQueue;
	qboolean ready;
} ralQueueTransferReceipt_t;

typedef struct {
	uint64_t nextGeneration;
	ralQueueTransferReceipt_t pending;
} ralQueueTransferLifecycle_t;

qboolean Ral_ResourceStateValidForBuffer( const ralResourceState_t *state );
qboolean Ral_ResourceStateValidForTexture( const ralResourceState_t *state );

// Pure, output-free validation helpers shared by backends and host tests.
// Resource extents come from the owning backend's opaque handle. Keeping them
// explicit makes these functions deterministic and usable by future backends.
qboolean Ral_BufferTransitionValid( const ralBufferTransition_t *transition,
	                                uint64_t bufferSize );
qboolean Ral_TextureTransitionValid( const ralTextureTransition_t *transition,
	                                 uint32_t mipLevels,
	                                 uint32_t arrayLayers,
	                                 ralTextureAspectFlags_t availableAspects );
void Ral_QueueTransferLifecycleInit( ralQueueTransferLifecycle_t *lifecycle );
qboolean Ral_QueueTransferReceiptExact( const ralQueueTransferReceipt_t *a,
	                                    const ralQueueTransferReceipt_t *b );
ralResult_t Ral_QueueTransferLifecycleRelease(
	ralQueueTransferLifecycle_t *lifecycle, const void *resourceIdentity,
	ralQueueTransferResourceType_t resourceType,
	const ralResourceState_t *before, const ralResourceState_t *after,
	ralQueueType_t sourceQueue, ralQueueType_t destinationQueue,
	ralQueueTransferReceipt_t *outReceipt );
ralResult_t Ral_QueueTransferLifecycleAcquire(
	ralQueueTransferLifecycle_t *lifecycle,
	const ralQueueTransferReceipt_t *receipt );
ralResult_t Ral_QueueTransferLifecycleCancel(
	ralQueueTransferLifecycle_t *lifecycle,
	const ralQueueTransferReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_TRANSITION_H
