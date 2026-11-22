// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_BOOTSTRAP_AA_H
#define WIRED_VK_TEMPORAL_BOOTSTRAP_AA_H

#include "vk_temporal_history_store.h"
#include "../../../core/ral_command.h"
#include "../../../core/ral_pipeline.h"

// The first frame of a temporal history has nothing to reproject. It used to be a plain copy of the scene, so it was not
// anti-aliased at all. This owner runs the temporal resolve's spatial fallback filter over that frame instead, writes the filtered
// result to the resolved target the post chain reads and commits it (with the linear depth) as the history pair the next frame
// reprojects. Borrowed resources are the renderer's; the owner only creates and destroys its own views, sampler, layout, groups and
// pipeline.
typedef struct {
	ralBackend_t *backend;
	uint32_t width, height;
	uint32_t historyAllocationGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t resolvedTargetAllocationGeneration;
	ralTexture_t *currentColor;
	ralTexture_t *currentDepth;
	ralTexture_t *resolvedTarget;
	ralTextureView_t *resolvedTargetView;
	ralTexture_t *historyColor[2];
	ralTextureView_t *historyColorView[2];
	ralTexture_t *historyDepth[2];
	ralTextureView_t *historyDepthView[2];
	const uint32_t *computeSpirv;
	size_t computeSpirvSize;
} vkTemporalBootstrapAaKey_t;

typedef struct {
	vkTemporalBootstrapAaKey_t key;
	ralTextureView_t *currentColorView;
	ralTextureView_t *currentDepthView;
	ralSampler_t *sampler;
	ralBindGroupLayout_t *layout;
	ralBindGroup_t *groups[2];
	ralPipeline_t *pipeline;
	qboolean initialized;
	qboolean ready;
} vkTemporalBootstrapAaOwner_t;

void VK_TemporalBootstrapAaInit( vkTemporalBootstrapAaOwner_t *owner );
qboolean VK_TemporalBootstrapAaMatches( const vkTemporalBootstrapAaOwner_t *owner,
	const vkTemporalBootstrapAaKey_t *key );
qboolean VK_TemporalBootstrapAaHasLive( const vkTemporalBootstrapAaOwner_t *owner );
qboolean VK_TemporalBootstrapAaNeedsIdle( const vkTemporalBootstrapAaOwner_t *owner,
	const vkTemporalBootstrapAaKey_t *key );
qboolean VK_TemporalBootstrapAaEnsureAfterFence( vkTemporalBootstrapAaOwner_t *owner,
	const vkTemporalBootstrapAaKey_t *key, qboolean idleProven );
// Records the filter dispatch for one frame and leaves the written images in the sampled layout. writeIndex names the history pair
// to commit. Returns qfalse without recording anything when the owner does not match the key.
qboolean VK_TemporalBootstrapAaRecord( const vkTemporalBootstrapAaOwner_t *owner,
	const vkTemporalBootstrapAaKey_t *key, ralCommandBuffer_t *commandBuffer,
	uint32_t writeIndex, const vkTemporalHistoryStorePush_t *push );
qboolean VK_TemporalBootstrapAaReleaseAfterIdle( vkTemporalBootstrapAaOwner_t *owner,
	qboolean idleProven );

#endif
