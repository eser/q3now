// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WIRED_VK_TEMPORAL_PIPELINE_FACTORY_H
#define WIRED_VK_TEMPORAL_PIPELINE_FACTORY_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../include/vulkan/vulkan.h"
#include "../../../core/ral_pipeline.h"
#include "../../../core/ral_resource.h"
#include "vk_temporal_shader_cohort.h"
#include "vk_temporal_generic_catalog.h"

typedef struct {
	vkTemporalShaderBlob_t vertex;
	vkTemporalShaderBlob_t fragment;
} vkTemporalSpirvOverrides_t;

typedef struct {
	ralPipelineLayout_t *(*create)( ralBackend_t *,
		const ralPipelineLayoutCreateInfo_t * );
	void *(*getHandle)( const ralPipelineLayout_t * );
	void (*destroy)( ralPipelineLayout_t * );
} vkTemporalLayoutOps_t;

typedef struct {
	ralBackend_t *backend;
	VkDevice device;
	const ralBindGroupLayout_t *borrowedLayouts[3];
	const ralBindGroupLayout_t *payloadLayout;
	uint32_t payloadLayoutGeneration;
	VkPipelineLayout raw;
	ralPipelineLayout_t *adopted;
	uint32_t allocationGeneration;
	uint32_t leases;
	qboolean fog;
	qboolean ready;
} vkTemporalPipelineLayoutOwner_t;

void VK_TemporalPipelineLayoutInit( vkTemporalPipelineLayoutOwner_t *owner );
qboolean VK_TemporalPipelineLayoutEnsure( vkTemporalPipelineLayoutOwner_t *owner,
	ralBackend_t *backend, VkDevice device,
	const ralBindGroupLayout_t *const borrowedLayouts[3],
	const ralBindGroupLayout_t *payloadLayout, uint32_t payloadLayoutGeneration,
	qboolean fog, const vkTemporalLayoutOps_t *ops );
qboolean VK_TemporalPipelineLayoutAcquire( vkTemporalPipelineLayoutOwner_t *owner );
qboolean VK_TemporalPipelineLayoutReleaseLease( vkTemporalPipelineLayoutOwner_t *owner );
qboolean VK_TemporalPipelineLayoutRelease( vkTemporalPipelineLayoutOwner_t *owner,
	const vkTemporalLayoutOps_t *ops );

typedef ralPipeline_t *(*vkTemporalExactCreateFn)(
	const VkGraphicsPipelineCreateInfo *, ralPipelineLayout_t *,
	const ralFormat_t *, uint32_t, ralFormat_t,
	const vkTemporalSpirvOverrides_t *, const char * );
typedef void (*vkTemporalPipelineDestroyFn)( ralPipeline_t * );

typedef struct {
	vkTemporalExactCreateFn create;
	// destroy enqueues Ral_DestroyPipeline-style deferred destruction. drain
	// must flush that same backend before any leased parent layout is released.
	vkTemporalPipelineDestroyFn destroy;
	qboolean (*drain)( ralBackend_t * );
	qboolean (*lookup)( VkShaderModule, vkTemporalShaderBlob_t * );
	// False means the returned pointer aliases externally-live ownership; the
	// factory rejects it without destroying that borrowed handle.
	qboolean (*candidateAllowed)( ralPipeline_t *, const void * );
	const void *candidateContext;
} vkTemporalPipelineFactoryOps_t;

#if defined(WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY)
typedef struct {
	vkTemporalShaderBlob_t ordinaryVertex;
	vkTemporalShaderBlob_t ordinaryFragment;
	vkTemporalShaderBlob_t temporalVertex;
	vkTemporalShaderBlob_t temporalWriteFragment;
	vkTemporalShaderBlob_t temporalInvalidateFragment;
	vkTemporalShaderBlob_t iqmVertex;
	vkTemporalShaderBlob_t iqmOrdinaryFragment;
	vkTemporalShaderBlob_t iqmInvalidateFragment;
	uint32_t generation;
} vkTemporalPipelineBlobCatalog_t;

typedef struct {
	uint32_t pipelineGeneration;
	uint32_t topologyGeneration;
	uint32_t iqmLayoutGeneration;
	ralFormat_t sceneFormat;
	ralFormat_t depthFormat;
	qboolean exactTx1;
	qboolean alphaTested;
	qboolean depthOnly;
	qboolean blended;
	qboolean special;
	qboolean dynamicDiscard;
} vkTemporalPipelineFactoryInput_t;

typedef struct {
	ralPipeline_t *generic[3];
	ralPipeline_t *iqmInvalidate;
	vkTemporalPipelineLayoutOwner_t *layoutOwner;
	uint32_t pipelineGeneration;
	uint32_t topologyGeneration;
	uint32_t blobGeneration;
	uint32_t allocationGeneration;
	uint32_t layoutAllocationGeneration;
	uint64_t genericBaseFingerprint;
	uint64_t iqmBaseFingerprint;
	ralFormat_t sceneFormat;
	ralFormat_t depthFormat;
	ralPipelineLayout_t *iqmLayout;
	uint32_t iqmLayoutGeneration;
	vkTemporalPipelineBlobCatalog_t blobs;
	vkTemporalPipelineLayoutOwner_t *retiringLayout;
	qboolean retiringLease;
	qboolean ready;
} vkTemporalPipelineFactoryOwner_t;

void VK_TemporalPipelineFactoryInit( vkTemporalPipelineFactoryOwner_t *owner );
qboolean VK_TemporalPipelineFactoryEnsure( vkTemporalPipelineFactoryOwner_t *owner,
	vkTemporalPipelineLayoutOwner_t *layoutOwner, ralPipelineLayout_t *iqmLayout,
	const VkGraphicsPipelineCreateInfo *genericBase,
	const VkGraphicsPipelineCreateInfo *iqmBase,
	const vkTemporalPipelineFactoryInput_t *input,
	const vkTemporalPipelineBlobCatalog_t *blobs,
	const vkTemporalPipelineFactoryOps_t *ops );
qboolean VK_TemporalPipelineFactoryRelease( vkTemporalPipelineFactoryOwner_t *owner,
	const vkTemporalPipelineFactoryOps_t *ops );
#endif

// Exact 40-pair generic ownership. One owner represents exactly one catalog
// key and owns only PRESERVE/WRITE/INVALIDATE; IQM is a separate singleton.
typedef struct {
	ralPipeline_t *pipelines[3];
	vkTemporalPipelineLayoutOwner_t *layoutOwner;
	vkTemporalGenericCatalogEntry_t catalog;
	uint32_t pipelineGeneration, topologyGeneration, catalogGeneration;
	uint32_t allocationGeneration, layoutAllocationGeneration;
	uint64_t baseFingerprint;
	ralFormat_t sceneFormat, depthFormat;
	vkTemporalPipelineLayoutOwner_t *retiringLayout;
	qboolean retiringLease, ready;
} vkTemporalGenericPipelineFactoryOwner_t;

typedef struct {
	vkTemporalGenericKey_t key;
	uint32_t pipelineGeneration, topologyGeneration, catalogGeneration;
	ralFormat_t sceneFormat, depthFormat;
	qboolean alphaTested, depthOnly, blended, special, dynamicDiscard;
} vkTemporalGenericPipelineFactoryInput_t;

void VK_TemporalGenericPipelineFactoryInit( vkTemporalGenericPipelineFactoryOwner_t *owner );
qboolean VK_TemporalGenericPipelineFactoryEnsure( vkTemporalGenericPipelineFactoryOwner_t *owner,
	vkTemporalPipelineLayoutOwner_t *layoutOwner,
	const VkGraphicsPipelineCreateInfo *base,
	const vkTemporalGenericPipelineFactoryInput_t *input,
	const vkTemporalPipelineFactoryOps_t *ops );
qboolean VK_TemporalGenericPipelineFactoryRelease( vkTemporalGenericPipelineFactoryOwner_t *owner,
	const vkTemporalPipelineFactoryOps_t *ops );
// Batch lifecycle seam. The caller must hold an external parent-layout lease
// across Ensure (including candidate failure). Retire only enqueues child
// destruction; after one same-backend drain, FinalizeAfterDrain releases this
// owner's lease. Both calls are retry-safe.
qboolean VK_TemporalGenericPipelineFactoryRetire(
	vkTemporalGenericPipelineFactoryOwner_t *owner,
	const vkTemporalPipelineFactoryOps_t *ops );
qboolean VK_TemporalGenericPipelineFactoryFinalizeAfterDrain(
	vkTemporalGenericPipelineFactoryOwner_t *owner );

typedef struct {
	vkTemporalShaderBlob_t ordinaryVertex, ordinaryFragment;
	vkTemporalShaderBlob_t invalidateFragment;
	uint32_t generation;
} vkTemporalIqmPipelineCatalog_t;

typedef struct {
	ralPipeline_t *pipeline;
	ralBackend_t *backend;
	ralPipelineLayout_t *layout;
	uint32_t pipelineGeneration, topologyGeneration, layoutGeneration;
	uint32_t catalogGeneration, allocationGeneration;
	uint64_t baseFingerprint;
	ralFormat_t sceneFormat, depthFormat;
	vkTemporalIqmPipelineCatalog_t catalog;
	qboolean ready, pendingDrain;
} vkTemporalIqmPipelineFactoryOwner_t;

void VK_TemporalIqmPipelineFactoryInit( vkTemporalIqmPipelineFactoryOwner_t *owner );
qboolean VK_TemporalIqmPipelineFactoryEnsure( vkTemporalIqmPipelineFactoryOwner_t *owner,
	ralBackend_t *backend, ralPipelineLayout_t *layout, uint32_t layoutGeneration,
	const VkGraphicsPipelineCreateInfo *base, uint32_t pipelineGeneration,
	uint32_t topologyGeneration, ralFormat_t sceneFormat, ralFormat_t depthFormat,
	const vkTemporalIqmPipelineCatalog_t *catalog,
	const vkTemporalPipelineFactoryOps_t *ops );
qboolean VK_TemporalIqmPipelineFactoryRelease( vkTemporalIqmPipelineFactoryOwner_t *owner,
	ralBackend_t *backend, const vkTemporalPipelineFactoryOps_t *ops );

// Borrowed-lifetime precondition for a future caller: iqmLayout must outlive
// FactoryRelease plus its successful deferred drain. The payload composite BGL
// must outlive FactoryRelease, then PipelineLayoutRelease.

// Definition-only direct-SPIR-V seam. Ordinary gpInfo callers continue through
// the historical lookup wrapper and pass no override.
ralPipeline_t *vk_ral_create_pipeline_from_gpinfo_exact_spirv(
	const VkGraphicsPipelineCreateInfo *ci, ralPipelineLayout_t *layout,
	const ralFormat_t *formats, uint32_t count, ralFormat_t depth,
	const vkTemporalSpirvOverrides_t *overrides, const char *debugName );
qboolean vk_temporal_pipeline_lookup_blob( VkShaderModule module,
	vkTemporalShaderBlob_t *out );

#endif
