// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WIRED_VK_TEMPORAL_GENERIC_RECIPE_TABLE_H
#define WIRED_VK_TEMPORAL_GENERIC_RECIPE_TABLE_H

#include "vk_generic_specialization_contract.h"
#include "vk_temporal_generic_catalog.h"
#include "../../../core/ral_resource.h"

#include <stddef.h>
#include <stdint.h>

enum {
	VK_TEMPORAL_RECIPE_MAX_VERTEX_BINDINGS = 8,
	VK_TEMPORAL_RECIPE_MAX_VERTEX_ATTRIBUTES = 8
};

typedef enum {
	VK_TEMPORAL_RECIPE_LAYOUT_INVALID = 0,
	VK_TEMPORAL_RECIPE_LAYOUT_GENERIC_MAIN = 1
} vkTemporalRecipeLayoutClass_t;

typedef struct {
	uint64_t token;
	uint64_t frameId;
	uint32_t planGeneration;
	uint32_t materializationGeneration;
} vkTemporalRecipeBatchAuthority_t;

typedef struct {
	uint32_t ownerEpoch;
	uint32_t slot;
	uint32_t entryGeneration;
	uint32_t catalogId;
	vkTemporalGenericKey_t key;
	qboolean alphaTested;
	uint32_t specializationWords[VK_GENERIC_TOTAL_SPEC_COUNT];
	VkVertexInputBindingDescription bindings[VK_TEMPORAL_RECIPE_MAX_VERTEX_BINDINGS];
	VkVertexInputAttributeDescription attributes[VK_TEMPORAL_RECIPE_MAX_VERTEX_ATTRIBUTES];
	uint32_t numBindings, numAttributes;
	VkPrimitiveTopology topology;
	qboolean primitiveRestart;
	qboolean depthClampEnable, rasterizerDiscardEnable;
	VkPolygonMode polygonMode;
	VkCullModeFlags cullMode;
	VkFrontFace frontFace;
	qboolean depthBiasEnable;
	float depthBiasConstant, depthBiasClamp, depthBiasSlope, lineWidth;
	VkSampleCountFlagBits sampleCount;
	qboolean sampleShadingEnable;
	float minSampleShading;
	qboolean sampleMaskPresent, alphaToCoverageEnable, alphaToOneEnable;
	qboolean depthTestEnable, depthWriteEnable;
	VkCompareOp depthCompareOp;
	qboolean depthBoundsTestEnable, stencilTestEnable;
	VkStencilOpState stencilFront, stencilBack;
	float minDepthBounds, maxDepthBounds;
	qboolean logicOpEnable;
	VkLogicOp logicOp;
	VkPipelineColorBlendAttachmentState sceneBlend;
	float blendConstants[4];
	VkDynamicState dynamicStates[2];
	ralFormat_t sceneFormat, depthFormat;
	vkTemporalRecipeLayoutClass_t layoutClass;
	vkTemporalRecipeBatchAuthority_t capturedAuthority;
	qboolean captureAttempted;
	qboolean valid;
} vkTemporalGenericRecipe_t;

typedef struct {
	uint32_t ownerEpoch;
	uint32_t slot;
	uint32_t entryGeneration;
	uint32_t catalogId;
} vkTemporalGenericRecipeReceipt_t;

typedef struct {
	vkTemporalGenericRecipe_t *records;
	uint32_t capacity;
	uint32_t ownerEpoch;
	uint32_t lastEntryGeneration;
	vkTemporalRecipeBatchAuthority_t authority;
	qboolean armed;
} vkTemporalGenericRecipeTable_t;

typedef struct {
	void *(*alloc)( size_t bytes );
	void (*free)( void *memory );
} vkTemporalRecipeTableOps_t;

typedef struct {
	uint32_t slot;
	uint32_t catalogId;
	vkTemporalGenericKey_t key;
	qboolean alphaTested;
	const VkGraphicsPipelineCreateInfo *base;
	ralFormat_t sceneFormat, depthFormat;
	vkTemporalRecipeLayoutClass_t layoutClass;
} vkTemporalGenericRecipeCaptureInput_t;

// Synchronous, self-contained Vulkan view rebuilt from one pointer-free
// recipe. Every pointer in gp targets storage owned by this view; no pointer
// reaches back into the recipe or the original create_pipeline stack graph.
typedef struct {
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewport;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState sceneBlend;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineDynamicStateCreateInfo dynamic;
	vkGenericSpecializationGraph_t specialization;
	uint32_t vertexWord;
	uint32_t fragmentWords[VK_GENERIC_FRAGMENT_SPEC_COUNT];
	VkVertexInputBindingDescription bindings[VK_TEMPORAL_RECIPE_MAX_VERTEX_BINDINGS];
	VkVertexInputAttributeDescription attributes[VK_TEMPORAL_RECIPE_MAX_VERTEX_ATTRIBUTES];
	VkDynamicState dynamicStates[2];
	VkGraphicsPipelineCreateInfo gp;
} vkTemporalGenericRecipeView_t;

void VK_TemporalGenericRecipeTableInit( vkTemporalGenericRecipeTable_t *owner );
qboolean VK_TemporalGenericRecipeTablePrepare(
	vkTemporalGenericRecipeTable_t *owner, uint32_t capacity,
	const vkTemporalRecipeBatchAuthority_t *authority,
	const vkTemporalRecipeTableOps_t *ops );
void VK_TemporalGenericRecipeTableDisarm( vkTemporalGenericRecipeTable_t *owner );
qboolean VK_TemporalGenericRecipeTableCapture(
	vkTemporalGenericRecipeTable_t *owner,
	const vkTemporalGenericRecipeCaptureInput_t *input,
	vkTemporalGenericRecipeReceipt_t *outReceipt );
qboolean VK_TemporalGenericRecipeTableGetSlotReceipt(
	const vkTemporalGenericRecipeTable_t *owner, uint32_t slot,
	vkTemporalGenericRecipeReceipt_t *outReceipt );
qboolean VK_TemporalGenericRecipeTableGet(
	const vkTemporalGenericRecipeTable_t *owner, uint32_t ownerEpoch,
	uint32_t slot, uint32_t entryGeneration,
	vkTemporalGenericRecipe_t *outRecipe );
qboolean VK_TemporalGenericRecipeTableMarkAttempted(
	vkTemporalGenericRecipeTable_t *owner, uint32_t slot );
qboolean VK_TemporalGenericRecipeTableGetSlotState(
	const vkTemporalGenericRecipeTable_t *owner, uint32_t slot,
	qboolean *outAttempted, qboolean *outValid );
qboolean VK_TemporalGenericRecipeBuildView(
	const vkTemporalGenericRecipe_t *recipe, VkShaderModule ordinaryVertex,
	VkShaderModule ordinaryFragment, VkPipelineLayout layout,
	vkTemporalGenericRecipeView_t *outView );
qboolean VK_TemporalGenericRecipeTableEvictRange(
	vkTemporalGenericRecipeTable_t *owner, uint32_t first, uint32_t end );
qboolean VK_TemporalGenericRecipeTableRelease(
	vkTemporalGenericRecipeTable_t *owner,
	const vkTemporalRecipeTableOps_t *ops );

#endif
