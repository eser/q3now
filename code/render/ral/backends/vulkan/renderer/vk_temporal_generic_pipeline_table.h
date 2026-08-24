// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_GENERIC_PIPELINE_TABLE_H
#define WIRED_VK_TEMPORAL_GENERIC_PIPELINE_TABLE_H

#include "vk_temporal_generic_recipe_table.h"
#include "vk_temporal_pipeline_factory.h"

typedef struct {
	vkTemporalGenericPipelineFactoryOwner_t factory;
	ralPipeline_t *ordinaryPipeline;
	uint32_t recipeOwnerEpoch;
	uint32_t recipeEntryGeneration;
	uint32_t catalogId;
	uint32_t moduleRegistryGeneration;
	uint32_t entryGeneration;
	qboolean occupied;
} vkTemporalGenericPipelineSlot_t;

typedef struct {
	vkTemporalGenericPipelineSlot_t *slots;
	uint32_t capacity;
	uint32_t allocationGeneration;
	uint32_t lastEntryGeneration;
	vkTemporalPipelineLayoutOwner_t *layoutOwner;
	uint32_t layoutAllocationGeneration;
	qboolean guardLease;
} vkTemporalGenericPipelineTable_t;

typedef struct {
	void *(*alloc)( size_t bytes );
	void (*free)( void *memory );
} vkTemporalGenericPipelineTableMemoryOps_t;

typedef struct {
	uint32_t slot;
	const vkTemporalGenericRecipe_t *recipe;
	const vkTemporalGenericRecipeReceipt_t *recipeReceipt;
	const VkGraphicsPipelineCreateInfo *base;
	ralPipeline_t *ordinaryPipeline;
	// Optional still-live ordinary pipeline retained during candidate-first
	// cached-MAIN replacement. Exact3 candidates may alias neither ordinary.
	ralPipeline_t *retainedOrdinaryPipeline;
	uint32_t topologyGeneration;
	uint32_t moduleRegistryGeneration;
} vkTemporalGenericPipelineTableEnsureInput_t;

typedef struct {
	uint32_t tableGeneration;
	uint32_t slot;
	uint32_t entryGeneration;
	uint32_t recipeOwnerEpoch;
	uint32_t recipeEntryGeneration;
	uint32_t factoryAllocationGeneration;
	uint32_t catalogId;
} vkTemporalGenericPipelineReceipt_t;

typedef enum {
	VK_TEMPORAL_CACHED_MAIN_REJECTED = 0,
	VK_TEMPORAL_CACHED_MAIN_DEFERRED_FAILURE,
	VK_TEMPORAL_CACHED_MAIN_READY
} vkTemporalCachedMainMaterializeResult_t;

typedef struct {
	void (*createAndCapture)( void *context );
	qboolean (*captureState)( void *context, qboolean *attempted,
		qboolean *valid );
	vkTemporalCachedMainMaterializeResult_t (*materialize)( void *context,
		ralPipeline_t *retainedOrdinaryPipeline );
	void (*destroy)( ralPipeline_t *pipeline );
	void (*decline)( void *context );
} vkTemporalCachedMainRebuildOps_t;

void VK_TemporalGenericPipelineTableInit(
	vkTemporalGenericPipelineTable_t *owner );
qboolean VK_TemporalGenericPipelineTablePrepare(
	vkTemporalGenericPipelineTable_t *owner, uint32_t capacity,
	vkTemporalPipelineLayoutOwner_t *layoutOwner,
	const vkTemporalGenericPipelineTableMemoryOps_t *memoryOps );
qboolean VK_TemporalGenericPipelineTableEnsureSlot(
	vkTemporalGenericPipelineTable_t *owner,
	const vkTemporalGenericPipelineTableEnsureInput_t *input,
	const vkTemporalPipelineFactoryOps_t *factoryOps,
	vkTemporalGenericPipelineReceipt_t *outReceipt );
qboolean VK_TemporalGenericPipelineTableGet(
	const vkTemporalGenericPipelineTable_t *owner,
	const vkTemporalGenericPipelineReceipt_t *receipt,
	ralPipeline_t *outPipelines[3] );
qboolean VK_TemporalGenericPipelineTableGetSlotReceipt(
	const vkTemporalGenericPipelineTable_t *owner, uint32_t slot,
	vkTemporalGenericPipelineReceipt_t *outReceipt );
qboolean VK_TemporalGenericPipelineTablePreflightSlot(
	const vkTemporalGenericPipelineTable_t *owner, uint32_t slot,
	const ralPipeline_t *expectedOrdinary,
	vkTemporalGenericPipelineReceipt_t *outReceipt,
	ralPipeline_t *outPipelines[3] );
qboolean VK_TemporalGenericPipelineTableSlotReady(
	const vkTemporalGenericPipelineTable_t *owner, uint32_t slot );
qboolean VK_TemporalGenericPipelineTableHasLive(
	const vkTemporalGenericPipelineTable_t *owner );
qboolean VK_TemporalGenericPipelineTableReleaseRangeAfterIdle(
	vkTemporalGenericPipelineTable_t *owner, uint32_t first, uint32_t end,
	ralBackend_t *backend, const vkTemporalPipelineFactoryOps_t *factoryOps );
qboolean VK_TemporalGenericPipelineTableReleaseAfterIdle(
	vkTemporalGenericPipelineTable_t *owner, ralBackend_t *backend,
	const vkTemporalPipelineFactoryOps_t *factoryOps,
	const vkTemporalGenericPipelineTableMemoryOps_t *memoryOps );

// Candidate-first ordinary MAIN rebuild used only at the post-idle safe
// boundary. The callback authors/captures the exact product gpInfo while live;
// failure restores the old scene pipeline and all tracked scalar state.
qboolean VK_TemporalCachedMainRebuild(
	ralPipeline_t **ordinaryPipeline, qboolean *depthFade,
	int32_t *pipelineCreateCount, void *context,
	const vkTemporalCachedMainRebuildOps_t *ops, qboolean *outDeferredWork );

#endif
