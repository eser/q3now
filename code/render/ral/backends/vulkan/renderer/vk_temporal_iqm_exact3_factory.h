// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_IQM_EXACT3_FACTORY_H
#define WIRED_VK_TEMPORAL_IQM_EXACT3_FACTORY_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../include/vulkan/vulkan.h"
#include "../../../core/ral_pipeline.h"
#include "vk_temporal_iqm_payload.h"
#include "vk_temporal_shader_cohort.h"

#define VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE 8u
#define VK_TEMPORAL_IQM_EXACT3_VERTEX_STRIDE 68u

_Static_assert( VK_TEMPORAL_IQM_EXACT3_VERTEX_STRIDE
	== TEMPORAL_IQM_VERTEX_STRIDE, "IQM exact3 vertex stride parity" );

typedef struct {
	uint32_t imageSlot;
	uint32_t samplerSlot;
} vkTemporalIqmExact3Push_t;

_Static_assert( sizeof( vkTemporalIqmExact3Push_t )
	== VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE, "IQM exact3 push size" );
_Static_assert( offsetof( vkTemporalIqmExact3Push_t, imageSlot ) == 0u,
	"IQM exact3 image slot offset" );
_Static_assert( offsetof( vkTemporalIqmExact3Push_t, samplerSlot ) == 4u,
	"IQM exact3 sampler slot offset" );

typedef enum {
	VK_TEMPORAL_IQM_EXACT3_CANDIDATE_RAW_LAYOUT = 1,
	VK_TEMPORAL_IQM_EXACT3_CANDIDATE_ADOPTED_LAYOUT,
	VK_TEMPORAL_IQM_EXACT3_CANDIDATE_WRITE_PIPELINE,
	VK_TEMPORAL_IQM_EXACT3_CANDIDATE_INVALIDATE_PIPELINE
} vkTemporalIqmExact3CandidateRole_t;

typedef struct {
	ralBackend_t *backend;
	ralBindGroupLayout_t *layout;
	const void *setIdentity;
	uint64_t setGeneration;
	qboolean ready;
} vkTemporalIqmExact3BindlessParent_t;

typedef struct {
	vkTemporalShaderBlob_t vertex;
	vkTemporalShaderBlob_t writeFragment;
	vkTemporalShaderBlob_t invalidateFragment;
	uint32_t generation;
} vkTemporalIqmExact3ShaderCatalog_t;

typedef struct {
	ralBackend_t *backend;
	VkDevice device;
	vkTemporalIqmExact3BindlessParent_t bindless;
	uint32_t pipelineGeneration;
	uint32_t topologyGeneration;
	ralFormat_t sceneFormat;
	ralFormat_t depthFormat;
	qboolean reversedDepth;
} vkTemporalIqmExact3FactoryInput_t;

typedef struct {
	void *(*getBindGroupLayoutHandle)( const ralBindGroupLayout_t * );
	ralPipelineLayout_t *(*createLayout)( ralBackend_t *,
		const ralPipelineLayoutCreateInfo_t * );
	void *(*getLayoutHandle)( const ralPipelineLayout_t * );
	void (*destroyLayout)( ralPipelineLayout_t * );
	ralPipeline_t *(*createPipeline)( ralBackend_t *,
		const ralGraphicsPipelineCreateInfo_t * );
	void (*destroyPipeline)( ralPipeline_t * );
	qboolean (*drain)( ralBackend_t * );
	qboolean (*candidateAllowed)( vkTemporalIqmExact3CandidateRole_t,
		const void *, const void * );
	const void *candidateContext;
} vkTemporalIqmExact3FactoryOps_t;

typedef struct {
	vkTemporalIqmPayloadOwner_t *payloadOwner;
	ralBackend_t *backend;
	VkDevice device;
	ralBindGroupLayout_t *payloadLayout;
	uint32_t payloadLayoutGeneration;
	vkTemporalIqmExact3BindlessParent_t bindless;
	VkPipelineLayout rawLayout;
	void *payloadRawLayout;
	void *bindlessRawLayout;
	ralPipelineLayout_t *adoptedLayout;
	ralPipeline_t *writePipeline;
	ralPipeline_t *invalidatePipeline;
	vkTemporalIqmExact3ShaderCatalog_t shaders;
	uint32_t pipelineGeneration;
	uint32_t topologyGeneration;
	uint32_t allocationGeneration;
	ralFormat_t sceneFormat;
	ralFormat_t depthFormat;
	qboolean reversedDepth;
	qboolean payloadLease;
	qboolean pendingDrain;
	qboolean parentsDrained;
	qboolean initialized;
	qboolean ready;
} vkTemporalIqmExact3FactoryOwner_t;

typedef struct {
	ralBackend_t *backend;
	VkDevice device;
	vkTemporalIqmPayloadOwner_t *payloadOwner;
	ralBindGroupLayout_t *payloadLayout;
	void *payloadRawLayout;
	void *bindlessRawLayout;
	uint32_t payloadLayoutGeneration;
	vkTemporalIqmExact3BindlessParent_t bindless;
	VkPipelineLayout rawLayout;
	ralPipelineLayout_t *adoptedLayout;
	ralPipeline_t *writePipeline;
	ralPipeline_t *invalidatePipeline;
	uint32_t shaderGeneration;
	uint32_t pipelineGeneration;
	uint32_t topologyGeneration;
	uint32_t allocationGeneration;
	ralFormat_t sceneFormat;
	ralFormat_t depthFormat;
	qboolean reversedDepth;
	qboolean ready;
} vkTemporalIqmExact3FactoryReceipt_t;

void VK_TemporalIqmExact3FactoryInit(
	vkTemporalIqmExact3FactoryOwner_t *owner );
qboolean VK_TemporalIqmExact3FactoryEnsure(
	vkTemporalIqmExact3FactoryOwner_t *owner,
	vkTemporalIqmPayloadOwner_t *payloadOwner,
	const vkTemporalIqmExact3FactoryInput_t *input,
	const vkTemporalIqmExact3ShaderCatalog_t *shaders,
	const vkTemporalIqmExact3FactoryOps_t *ops );
qboolean VK_TemporalIqmExact3FactoryGetReceipt(
	const vkTemporalIqmExact3FactoryOwner_t *owner,
	vkTemporalIqmExact3FactoryReceipt_t *outReceipt );
qboolean VK_TemporalIqmExact3FactoryReceiptExact(
	const vkTemporalIqmExact3FactoryReceipt_t *a,
	const vkTemporalIqmExact3FactoryReceipt_t *b );
qboolean VK_TemporalIqmExact3FactoryRelease(
	vkTemporalIqmExact3FactoryOwner_t *owner,
	const vkTemporalIqmExact3FactoryOps_t *ops );

#endif
