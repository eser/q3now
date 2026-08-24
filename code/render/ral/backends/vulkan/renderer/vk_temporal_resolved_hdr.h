// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_RESOLVED_HDR_H
#define WIRED_VK_TEMPORAL_RESOLVED_HDR_H

#include "../../../core/ral_backend.h"
#include "../../../core/ral_resource.h"

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *currentSceneColor;
	ralBindGroup_t *currentPostprocessGroup;
	ralBindGroup_t *currentHistogramGroup;
	const ralBindGroupLayout_t *postprocessLayout;
	const ralBindGroupLayout_t *histogramLayout;
	const ralBuffer_t *histogramBuffer;
	int32_t worldIndex;
	uint32_t width, height, topologyEpoch, planGeneration;
	uint32_t sceneColorAttachmentGeneration;
	ralFormat_t sceneFormat;
	ralFilter_t filter;
} vkTemporalResolvedHdrInput_t;

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *target;
	ralTextureView_t *targetView;
	ralBindGroup_t *postprocessGroup;
	ralBindGroup_t *histogramGroup;
	ralTexture_t *currentSceneColor;
	ralBindGroup_t *currentPostprocessGroup;
	ralBindGroup_t *currentHistogramGroup;
	int32_t worldIndex;
	uint32_t width, height, topologyEpoch, planGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t allocationGeneration;
	ralFormat_t sceneFormat;
	qboolean ready;
} vkTemporalResolvedHdrReceipt_t;

typedef struct {
	vkTemporalResolvedHdrInput_t key;
	ralTexture_t *target;
	ralTextureView_t *targetView;
	ralSampler_t *sampler;
	ralBindGroup_t *postprocessGroup;
	ralBindGroup_t *histogramGroup;
	uint32_t allocationGeneration;
	qboolean initialized;
	qboolean ready;
} vkTemporalResolvedHdrOwner_t;

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *attachment;
	ralBindGroup_t *postprocessGroup;
	ralBindGroup_t *histogramGroup;
	uint64_t batchToken;
	uint64_t frameId;
	uint32_t commandSlot;
	uint32_t frameCount;
	int32_t worldIndex;
	uint32_t width, height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t targetAllocationGeneration;
	ralFormat_t sceneFormat;
	qboolean resolved;
} vkHdrPostprocessSource_t;

typedef enum {
	VK_TEMPORAL_RESOLVED_HDR_PRODUCER_NONE = 0,
	VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY = 1,
	VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE = 2
} vkTemporalResolvedHdrProducer_t;

// A future resolve producer may publish this only after it has populated the
// exact target receipt. H2a intentionally has no such product publisher.
typedef struct {
	uint64_t batchToken;
	uint64_t frameId;
	uint64_t contentSerial;
	uint32_t commandSlot;
	uint32_t frameCount;
	int32_t worldIndex;
	uint32_t width, height, topologyEpoch, planGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t targetAllocationGeneration;
	ralBackend_t *backend;
	ralTexture_t *sourceSceneColor;
	ralBindGroup_t *sourcePostprocessGroup;
	ralBindGroup_t *sourceHistogramGroup;
	ralTexture_t *target;
	ralTextureView_t *targetView;
	ralBindGroup_t *postprocessGroup;
	ralBindGroup_t *histogramGroup;
	ralFormat_t sceneFormat;
	vkTemporalResolvedHdrProducer_t producer;
	qboolean submitted;
	qboolean valid;
} vkTemporalResolvedHdrContentReceipt_t;

void VK_TemporalResolvedHdrInit( vkTemporalResolvedHdrOwner_t *owner );
qboolean VK_TemporalResolvedHdrNeedsIdle(
	const vkTemporalResolvedHdrOwner_t *owner,
	const vkTemporalResolvedHdrInput_t *input );
qboolean VK_TemporalResolvedHdrEnsureAfterFence(
	vkTemporalResolvedHdrOwner_t *owner,
	const vkTemporalResolvedHdrInput_t *input, qboolean idleProven );
qboolean VK_TemporalResolvedHdrGetReceipt(
	const vkTemporalResolvedHdrOwner_t *owner,
	vkTemporalResolvedHdrReceipt_t *outReceipt );
qboolean VK_TemporalResolvedHdrHasLive(
	const vkTemporalResolvedHdrOwner_t *owner );
qboolean VK_TemporalResolvedHdrReleaseAfterIdle(
	vkTemporalResolvedHdrOwner_t *owner, qboolean idleProven );

// Pure, output-atomic all-consumer router. Resource readiness alone never
// selects the resolved cohort: content must match every target identity.
// Product H2a always passes content == NULL, therefore all consumers retain
// the current-scene fallback without changing bloom/tonemap/histogram.
qboolean VK_TemporalResolvedHdrRouteSource(
	const vkHdrPostprocessSource_t *current,
	const vkTemporalResolvedHdrReceipt_t *target,
	const vkTemporalResolvedHdrContentReceipt_t *content,
	vkHdrPostprocessSource_t *outSource );

// H2b's command-local producer builds this receipt only after recording the
// exact full-extent current->resolved copy.  The pre-submit receipt may route
// the four postprocess consumers in that same command buffer; a separate copy
// becomes durable only when ResolveContentSubmit observes accepted submission.
qboolean VK_TemporalResolvedHdrBuildContentReceipt(
	const vkHdrPostprocessSource_t *current,
	const vkTemporalResolvedHdrReceipt_t *target,
	uint64_t contentSerial,
	vkTemporalResolvedHdrContentReceipt_t *outContent );
qboolean VK_TemporalResolvedHdrBuildContentReceiptForProducer(
	const vkHdrPostprocessSource_t *current,
	const vkTemporalResolvedHdrReceipt_t *target,
	uint64_t contentSerial, vkTemporalResolvedHdrProducer_t producer,
	vkTemporalResolvedHdrContentReceipt_t *outContent );
qboolean VK_TemporalResolvedHdrResolveContentSubmit(
	const vkTemporalResolvedHdrContentReceipt_t *recorded,
	qboolean submitted,
	vkTemporalResolvedHdrContentReceipt_t *outSubmitted );

#endif
