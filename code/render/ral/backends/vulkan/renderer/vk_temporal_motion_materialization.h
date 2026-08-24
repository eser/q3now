// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_MOTION_MATERIALIZATION_H
#define WIRED_VK_TEMPORAL_MOTION_MATERIALIZATION_H

#include "tr_temporal_motion_payload.h"
#include "tr_temporal_motion_targets.h"
#include "vk_temporal_pipeline_factory.h"

typedef struct {
	ralBackend_t *backend;
	VkDevice device;
	const ralBindGroupLayout_t *borrowedLayouts[3];
	const temporalMotionPayloadOwner_t *payload;
	int32_t worldIndex;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	qboolean fog;
} vkTemporalMotionMaterializationInput_t;

typedef struct {
	int32_t worldIndex;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	uint32_t payloadLayoutGeneration;
	uint32_t targetAllocationGeneration;
	uint32_t pipelineLayoutAllocationGeneration;
	uint32_t allocationGeneration;
	qboolean ready;
} vkTemporalMotionMaterializationReceipt_t;

typedef struct {
	ralTexture_t *scene;
	ralTexture_t *depth;
	ralTexture_t *velocity;
	ralTextureView_t *velocityView;
	ralTexture_t *validity;
	ralTextureView_t *validityView;
	ralPipelineLayout_t *pipelineLayout;
	VkPipelineLayout rawPipelineLayout;
	uint32_t targetAllocationGeneration;
	uint32_t pipelineLayoutAllocationGeneration;
	uint32_t allocationGeneration;
} vkTemporalMotionMaterializationProductView_t;

typedef struct {
	temporalMotionTargets_t targets;
	vkTemporalPipelineLayoutOwner_t pipelineLayout;
	vkTemporalMotionMaterializationInput_t key;
	const ralBindGroupLayout_t *payloadLayout;
	uint32_t payloadLayoutGeneration;
	uint32_t allocationGeneration;
	qboolean initialized;
	qboolean ready;
} vkTemporalMotionMaterialization_t;

void VK_TemporalMotionMaterializationInit(
	vkTemporalMotionMaterialization_t *owner );

// Reports whether replacing this exact live resource cohort requires a global
// device-idle proof. Semantic-only world/plan changes reuse the same resources.
qboolean VK_TemporalMotionMaterializationNeedsIdle(
	const vkTemporalMotionMaterialization_t *owner,
	const vkTemporalMotionMaterializationInput_t *input );

// Candidate-first, post-fence materialization only. This function creates no
// pipeline, shader module, pass, command, bind or draw authority. When a live
// resource cohort changes, idleProven must be true. GetReceipt output is
// atomic; a failed replacement clears aggregate ready and retains safe partial
// subowners solely for retry or ordered cleanup (no aggregate rollback claim).
qboolean VK_TemporalMotionMaterializationEnsureAfterFence(
	vkTemporalMotionMaterialization_t *owner,
	const vkTemporalMotionMaterializationInput_t *input,
	qboolean idleProven, const vkTemporalLayoutOps_t *layoutOps );

qboolean VK_TemporalMotionMaterializationGetReceipt(
	const vkTemporalMotionMaterialization_t *owner,
	vkTemporalMotionMaterializationReceipt_t *outReceipt );

// Generation-bound product view for conditional MAIN segmentation. The caller
// supplies the exact aggregate receipt; resources are borrowed and remain valid
// only while that receipt and the owner remain ready.
qboolean VK_TemporalMotionMaterializationGetProductView(
	const vkTemporalMotionMaterialization_t *owner,
	const vkTemporalMotionMaterializationReceipt_t *receipt,
	ralTexture_t *scene, ralTexture_t *depth,
	vkTemporalMotionMaterializationProductView_t *outView );

void VK_TemporalMotionMaterializationInvalidateReceipt(
	vkTemporalMotionMaterialization_t *owner );

qboolean VK_TemporalMotionMaterializationHasLive(
	const vkTemporalMotionMaterialization_t *owner );

// The payload composite layout is borrowed. Release this owner after global
// idle and before releasing the A2a payload/adopted-entMat runtime.
qboolean VK_TemporalMotionMaterializationReleaseAfterIdle(
	vkTemporalMotionMaterialization_t *owner, qboolean idleProven,
	const vkTemporalLayoutOps_t *layoutOps );

#endif
