// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_generic_recipe_table.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static qboolean AuthorityValid( const vkTemporalRecipeBatchAuthority_t *a ) {
	return a && a->token && a->frameId && a->planGeneration
		&& a->materializationGeneration;
}

static qboolean BoolValid( VkBool32 v ) { return v == VK_FALSE || v == VK_TRUE; }
static qboolean VertexFormatValid( VkFormat f ) {
	switch ( f ) {
	case VK_FORMAT_R32_SFLOAT: case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32_SFLOAT: case VK_FORMAT_R32G32B32A32_SFLOAT:
	case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_UINT: return qtrue;
	default: return qfalse;
	}
}
static qboolean CompareValid( VkCompareOp op ) {
	return op == VK_COMPARE_OP_EQUAL || op == VK_COMPARE_OP_GREATER_OR_EQUAL
		|| op == VK_COMPARE_OP_LESS_OR_EQUAL;
}
static qboolean StencilCanonicalDisabled( const VkStencilOpState *s ) {
	return s->failOp == VK_STENCIL_OP_KEEP && s->passOp == VK_STENCIL_OP_KEEP
		&& s->depthFailOp == VK_STENCIL_OP_KEEP
		&& s->compareOp == VK_COMPARE_OP_NEVER && !s->compareMask
		&& !s->writeMask && !s->reference;
}
static qboolean PositiveZero( float v ) {
	uint32_t bits; memcpy( &bits, &v, sizeof(bits) ); return bits == 0u;
}

static qboolean NormalizeRecipe( const vkTemporalGenericRecipeCaptureInput_t *in,
		vkTemporalGenericRecipe_t *out ) {
	const VkGraphicsPipelineCreateInfo *b;
	const VkPipelineVertexInputStateCreateInfo *vi;
	const VkPipelineInputAssemblyStateCreateInfo *ia;
	const VkPipelineViewportStateCreateInfo *vp;
	const VkPipelineRasterizationStateCreateInfo *rs;
	const VkPipelineMultisampleStateCreateInfo *ms;
	const VkPipelineDepthStencilStateCreateInfo *ds;
	const VkPipelineColorBlendStateCreateInfo *cb;
	const VkPipelineDynamicStateCreateInfo *dyn;
	vkGenericSpecializationFacts_t facts;
	vkGenericSpecializationReceipt_t specs;
	uint32_t expectedCatalogId, i;

	if ( !in || !out || !in->base
			|| in->layoutClass != VK_TEMPORAL_RECIPE_LAYOUT_GENERIC_MAIN
			|| in->sceneFormat == RAL_FORMAT_UNDEFINED
			|| in->depthFormat == RAL_FORMAT_UNDEFINED
			|| !VK_TemporalGenericCatalogKeyId( &in->key, &expectedCatalogId )
			|| expectedCatalogId != in->catalogId ) return qfalse;
	b = in->base;
	if ( b->sType != VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
			|| b->pNext || b->flags || b->stageCount != 2u || !b->pStages
			|| b->pStages[0].sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
			|| b->pStages[1].sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
			|| b->pStages[0].pNext || b->pStages[1].pNext
			|| b->pStages[0].flags || b->pStages[1].flags
			|| b->pStages[0].stage != VK_SHADER_STAGE_VERTEX_BIT
			|| b->pStages[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT
			|| !b->pStages[0].pName || !b->pStages[1].pName
			|| strcmp( b->pStages[0].pName, "main" ) != 0
			|| strcmp( b->pStages[1].pName, "main" ) != 0
			|| b->pTessellationState || b->renderPass != VK_NULL_HANDLE
			|| b->subpass || b->basePipelineHandle != VK_NULL_HANDLE
			|| b->basePipelineIndex != -1 ) return qfalse;
	vi = b->pVertexInputState; ia = b->pInputAssemblyState;
	vp = b->pViewportState; rs = b->pRasterizationState;
	ms = b->pMultisampleState; ds = b->pDepthStencilState;
	cb = b->pColorBlendState; dyn = b->pDynamicState;
	if ( !vi || !ia || !vp || !rs || !ms || !ds || !cb || !dyn
			|| vi->sType != VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
			|| ia->sType != VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
			|| vp->sType != VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
			|| rs->sType != VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
			|| ms->sType != VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
			|| ds->sType != VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
			|| cb->sType != VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
			|| dyn->sType != VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
			|| vi->pNext || ia->pNext || vp->pNext || rs->pNext || ms->pNext
			|| ds->pNext || cb->pNext || dyn->pNext
			|| vi->flags || ia->flags || vp->flags || rs->flags || ms->flags
			|| ds->flags || cb->flags || dyn->flags
			|| vi->vertexBindingDescriptionCount > VK_TEMPORAL_RECIPE_MAX_VERTEX_BINDINGS
			|| vi->vertexAttributeDescriptionCount > VK_TEMPORAL_RECIPE_MAX_VERTEX_ATTRIBUTES
			|| ( vi->vertexBindingDescriptionCount && !vi->pVertexBindingDescriptions )
			|| ( vi->vertexAttributeDescriptionCount && !vi->pVertexAttributeDescriptions )
			|| ia->topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
			|| ia->primitiveRestartEnable || vp->viewportCount != 1u
			|| vp->scissorCount != 1u || vp->pViewports || vp->pScissors
			|| rs->rasterizerDiscardEnable || rs->polygonMode != VK_POLYGON_MODE_FILL
			|| ( rs->cullMode != VK_CULL_MODE_NONE
				&& rs->cullMode != VK_CULL_MODE_FRONT_BIT
				&& rs->cullMode != VK_CULL_MODE_BACK_BIT )
			|| ( rs->frontFace != VK_FRONT_FACE_CLOCKWISE
				&& rs->frontFace != VK_FRONT_FACE_COUNTER_CLOCKWISE )
			|| ms->rasterizationSamples != VK_SAMPLE_COUNT_1_BIT
			|| !BoolValid(rs->depthClampEnable) || !BoolValid(rs->rasterizerDiscardEnable)
			|| !BoolValid(rs->depthBiasEnable) || !isfinite(rs->depthBiasConstantFactor)
			|| !isfinite(rs->depthBiasClamp) || !isfinite(rs->depthBiasSlopeFactor)
			|| !isfinite(rs->lineWidth) || rs->lineWidth <= 0.0f
			|| !BoolValid(ms->sampleShadingEnable) || ms->sampleShadingEnable
			|| !isfinite(ms->minSampleShading) || ms->minSampleShading != 1.0f
			|| ms->pSampleMask
			|| ms->alphaToCoverageEnable || ms->alphaToOneEnable
			|| !BoolValid(ms->alphaToCoverageEnable) || !BoolValid(ms->alphaToOneEnable)
			|| !BoolValid(ds->depthTestEnable) || !BoolValid(ds->depthWriteEnable)
			|| !CompareValid(ds->depthCompareOp)
			|| !BoolValid(ds->depthBoundsTestEnable) || ds->depthBoundsTestEnable
			|| !BoolValid(ds->stencilTestEnable) || ds->stencilTestEnable
			|| !StencilCanonicalDisabled( &ds->front )
			|| !StencilCanonicalDisabled( &ds->back )
			|| !isfinite(ds->minDepthBounds) || !isfinite(ds->maxDepthBounds)
			|| ds->minDepthBounds < 0.0f || ds->maxDepthBounds > 1.0f
			|| ds->minDepthBounds > ds->maxDepthBounds
			|| !BoolValid(cb->logicOpEnable) || cb->logicOpEnable
			|| cb->logicOp != VK_LOGIC_OP_COPY || cb->attachmentCount != 1u
			|| !cb->pAttachments || cb->pAttachments[0].blendEnable
			|| !BoolValid(cb->pAttachments[0].blendEnable)
			|| cb->pAttachments[0].srcColorBlendFactor != VK_BLEND_FACTOR_ZERO
			|| cb->pAttachments[0].dstColorBlendFactor != VK_BLEND_FACTOR_ZERO
			|| cb->pAttachments[0].colorBlendOp != VK_BLEND_OP_ADD
			|| cb->pAttachments[0].srcAlphaBlendFactor != VK_BLEND_FACTOR_ZERO
			|| cb->pAttachments[0].dstAlphaBlendFactor != VK_BLEND_FACTOR_ZERO
			|| cb->pAttachments[0].alphaBlendOp != VK_BLEND_OP_ADD
			|| cb->pAttachments[0].colorWriteMask
				!= ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
					| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT )
			|| dyn->dynamicStateCount != 2u || !dyn->pDynamicStates
			|| dyn->pDynamicStates[0] != VK_DYNAMIC_STATE_VIEWPORT
			|| dyn->pDynamicStates[1] != VK_DYNAMIC_STATE_SCISSOR ) return qfalse;
	for ( i = 0; i < vi->vertexBindingDescriptionCount; ++i ) {
		if ( vi->pVertexBindingDescriptions[i].inputRate
				!= VK_VERTEX_INPUT_RATE_VERTEX ) return qfalse;
	}
	for ( i = 0; i < vi->vertexAttributeDescriptionCount; ++i ) {
		uint32_t j;
		qboolean bindingFound = qfalse;
		if ( !VertexFormatValid( vi->pVertexAttributeDescriptions[i].format ) )
			return qfalse;
		for ( j = 0; j < vi->vertexBindingDescriptionCount; ++j ) {
			if ( vi->pVertexAttributeDescriptions[i].binding
					== vi->pVertexBindingDescriptions[j].binding ) bindingFound = qtrue;
		}
		if ( !bindingFound ) return qfalse;
	}
	for ( i = 0; i < 4u; ++i ) if ( !PositiveZero( cb->blendConstants[i] ) )
		return qfalse;
	facts.textureCount = in->key.textureCount;
	facts.shaderFog = in->key.shaderFog;
	if ( !VK_GenericTemporalSpecializationValidate( b, &facts, &specs ) )
		return qfalse;

	memset( out, 0, sizeof( *out ) );
	out->catalogId = in->catalogId; out->key = in->key;
	out->specializationWords[0] = specs.vertexWord;
	memcpy( &out->specializationWords[1], specs.fragmentWords,
		sizeof( specs.fragmentWords ) );
	out->numBindings = vi->vertexBindingDescriptionCount;
	out->numAttributes = vi->vertexAttributeDescriptionCount;
	if ( out->numBindings ) memcpy( out->bindings,
		vi->pVertexBindingDescriptions,
		out->numBindings * sizeof( out->bindings[0] ) );
	if ( out->numAttributes ) memcpy( out->attributes,
		vi->pVertexAttributeDescriptions,
		out->numAttributes * sizeof( out->attributes[0] ) );
	out->topology = ia->topology; out->primitiveRestart = ia->primitiveRestartEnable;
	out->depthClampEnable = rs->depthClampEnable;
	out->rasterizerDiscardEnable = rs->rasterizerDiscardEnable;
	out->polygonMode = rs->polygonMode; out->cullMode = rs->cullMode;
	out->frontFace = rs->frontFace; out->depthBiasEnable = rs->depthBiasEnable;
	out->depthBiasConstant = rs->depthBiasConstantFactor;
	out->depthBiasClamp = rs->depthBiasClamp;
	out->depthBiasSlope = rs->depthBiasSlopeFactor; out->lineWidth = rs->lineWidth;
	out->sampleCount = ms->rasterizationSamples;
	out->sampleShadingEnable = ms->sampleShadingEnable;
	out->minSampleShading = ms->minSampleShading;
	out->sampleMaskPresent = ms->pSampleMask ? qtrue : qfalse;
	out->alphaToCoverageEnable = ms->alphaToCoverageEnable;
	out->alphaToOneEnable = ms->alphaToOneEnable;
	out->depthTestEnable = ds->depthTestEnable; out->depthWriteEnable = ds->depthWriteEnable;
	out->depthCompareOp = ds->depthCompareOp;
	out->depthBoundsTestEnable = ds->depthBoundsTestEnable;
	out->stencilTestEnable = ds->stencilTestEnable;
	out->stencilFront = ds->front; out->stencilBack = ds->back;
	out->minDepthBounds = ds->minDepthBounds; out->maxDepthBounds = ds->maxDepthBounds;
	out->logicOpEnable = cb->logicOpEnable; out->logicOp = cb->logicOp;
	out->sceneBlend = cb->pAttachments[0];
	memcpy( out->blendConstants, cb->blendConstants, sizeof( out->blendConstants ) );
	out->dynamicStates[0] = dyn->pDynamicStates[0];
	out->dynamicStates[1] = dyn->pDynamicStates[1];
	out->sceneFormat = in->sceneFormat; out->depthFormat = in->depthFormat;
	out->layoutClass = in->layoutClass;
	return qtrue;
}

void VK_TemporalGenericRecipeTableInit( vkTemporalGenericRecipeTable_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalGenericRecipeTablePrepare(
		vkTemporalGenericRecipeTable_t *owner, uint32_t capacity,
		const vkTemporalRecipeBatchAuthority_t *authority,
		const vkTemporalRecipeTableOps_t *ops ) {
	vkTemporalGenericRecipe_t *candidate = NULL;
	uint32_t nextEpoch;
	size_t bytes;
	if ( !owner ) return qfalse;
	owner->armed = qfalse;
	memset( &owner->authority, 0, sizeof( owner->authority ) );
	if ( !capacity || !AuthorityValid( authority ) || !ops
			|| !ops->alloc || !ops->free ) return qfalse;
	if ( owner->records ) {
		if ( owner->capacity != capacity ) return qfalse;
		owner->authority = *authority; owner->armed = qtrue; return qtrue;
	}
	if ( owner->ownerEpoch == UINT32_MAX
			|| (size_t)capacity > SIZE_MAX / sizeof( *candidate ) ) return qfalse;
	bytes = (size_t)capacity * sizeof( *candidate );
	candidate = (vkTemporalGenericRecipe_t *)ops->alloc( bytes );
	if ( !candidate ) return qfalse;
	memset( candidate, 0, bytes );
	nextEpoch = owner->ownerEpoch + 1u;
	owner->records = candidate; owner->capacity = capacity;
	owner->ownerEpoch = nextEpoch; owner->authority = *authority;
	owner->armed = qtrue;
	return qtrue;
}

void VK_TemporalGenericRecipeTableDisarm( vkTemporalGenericRecipeTable_t *owner ) {
	if ( !owner ) return;
	owner->armed = qfalse; memset( &owner->authority, 0, sizeof( owner->authority ) );
}

qboolean VK_TemporalGenericRecipeTableCapture(
		vkTemporalGenericRecipeTable_t *owner,
		const vkTemporalGenericRecipeCaptureInput_t *input,
		vkTemporalGenericRecipeReceipt_t *outReceipt ) {
	vkTemporalGenericRecipe_t candidate;
	vkTemporalGenericRecipeReceipt_t receipt;
	uint32_t nextGeneration;
	if ( !owner || !owner->records || !owner->armed
			|| !AuthorityValid( &owner->authority ) || !input
			|| !outReceipt || input->slot >= owner->capacity ) return qfalse;
	owner->records[input->slot].captureAttempted = qtrue;
	owner->records[input->slot].valid = qfalse;
	if ( owner->lastEntryGeneration == UINT32_MAX
			|| !NormalizeRecipe( input, &candidate ) ) return qfalse;
	nextGeneration = owner->lastEntryGeneration + 1u;
	candidate.ownerEpoch = owner->ownerEpoch; candidate.slot = input->slot;
	candidate.entryGeneration = nextGeneration;
	candidate.capturedAuthority = owner->authority; candidate.valid = qtrue;
	candidate.captureAttempted = qtrue;
	owner->records[input->slot] = candidate;
	owner->lastEntryGeneration = nextGeneration;
	receipt.ownerEpoch = owner->ownerEpoch; receipt.slot = input->slot;
	receipt.entryGeneration = nextGeneration; receipt.catalogId = input->catalogId;
	*outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalGenericRecipeTableGetSlotReceipt(
		const vkTemporalGenericRecipeTable_t *owner, uint32_t slot,
		vkTemporalGenericRecipeReceipt_t *outReceipt ) {
	vkTemporalGenericRecipeReceipt_t receipt;
	if ( !owner || !owner->records || !outReceipt || slot >= owner->capacity
			|| !owner->records[slot].valid ) return qfalse;
	receipt.ownerEpoch = owner->records[slot].ownerEpoch;
	receipt.slot = slot;
	receipt.entryGeneration = owner->records[slot].entryGeneration;
	receipt.catalogId = owner->records[slot].catalogId;
	*outReceipt = receipt; return qtrue;
}

qboolean VK_TemporalGenericRecipeTableGet(
		const vkTemporalGenericRecipeTable_t *owner, uint32_t ownerEpoch,
		uint32_t slot, uint32_t entryGeneration,
		vkTemporalGenericRecipe_t *outRecipe ) {
	if ( !owner || !owner->records || !outRecipe || !ownerEpoch
			|| ownerEpoch != owner->ownerEpoch || slot >= owner->capacity
			|| !entryGeneration || !owner->records[slot].valid
			|| owner->records[slot].ownerEpoch != ownerEpoch
			|| owner->records[slot].slot != slot
			|| owner->records[slot].entryGeneration != entryGeneration ) return qfalse;
	*outRecipe = owner->records[slot]; return qtrue;
}

qboolean VK_TemporalGenericRecipeTableMarkAttempted(
		vkTemporalGenericRecipeTable_t *owner, uint32_t slot ) {
	if ( !owner || !owner->records || !owner->armed
			|| slot >= owner->capacity ) return qfalse;
	owner->records[slot].captureAttempted = qtrue;
	owner->records[slot].valid = qfalse;
	return qtrue;
}

qboolean VK_TemporalGenericRecipeTableGetSlotState(
		const vkTemporalGenericRecipeTable_t *owner, uint32_t slot,
		qboolean *outAttempted, qboolean *outValid ) {
	qboolean attempted, valid;
	if ( !owner || !owner->records || slot >= owner->capacity
			|| !outAttempted || !outValid ) return qfalse;
	attempted = owner->records[slot].captureAttempted;
	valid = owner->records[slot].valid;
	*outAttempted = attempted;
	*outValid = valid;
	return qtrue;
}

static void RepairViewPointers( vkTemporalGenericRecipeView_t *view ) {
	view->specialization.vertexInfo.pMapEntries = view->specialization.vertexMap;
	view->specialization.vertexInfo.pData = &view->vertexWord;
	view->specialization.fragmentInfo.pMapEntries = view->specialization.fragmentMaps;
	view->specialization.fragmentInfo.pData = view->fragmentWords;
	view->stages[0].pName = "main";
	view->stages[0].pSpecializationInfo = &view->specialization.vertexInfo;
	view->stages[1].pName = "main";
	view->stages[1].pSpecializationInfo = &view->specialization.fragmentInfo;
	view->vertexInput.pVertexBindingDescriptions = view->bindings;
	view->vertexInput.pVertexAttributeDescriptions = view->attributes;
	view->colorBlend.pAttachments = &view->sceneBlend;
	view->dynamic.pDynamicStates = view->dynamicStates;
	view->gp.pStages = view->stages;
	view->gp.pVertexInputState = &view->vertexInput;
	view->gp.pInputAssemblyState = &view->inputAssembly;
	view->gp.pViewportState = &view->viewport;
	view->gp.pRasterizationState = &view->rasterization;
	view->gp.pMultisampleState = &view->multisample;
	view->gp.pDepthStencilState = &view->depthStencil;
	view->gp.pColorBlendState = &view->colorBlend;
	view->gp.pDynamicState = &view->dynamic;
}

qboolean VK_TemporalGenericRecipeBuildView(
		const vkTemporalGenericRecipe_t *recipe, VkShaderModule ordinaryVertex,
		VkShaderModule ordinaryFragment, VkPipelineLayout layout,
		vkTemporalGenericRecipeView_t *outView ) {
	vkTemporalGenericRecipeView_t candidate;
	vkTemporalGenericCatalogEntry_t catalog;
	vkGenericSpecializationFacts_t facts;
	uint32_t expectedCatalogId;
	if ( !recipe || !outView || !recipe->valid || !recipe->ownerEpoch
			|| !recipe->entryGeneration || !ordinaryVertex || !ordinaryFragment
			|| !layout || recipe->layoutClass != VK_TEMPORAL_RECIPE_LAYOUT_GENERIC_MAIN
			|| recipe->numBindings > VK_TEMPORAL_RECIPE_MAX_VERTEX_BINDINGS
			|| recipe->numAttributes > VK_TEMPORAL_RECIPE_MAX_VERTEX_ATTRIBUTES
			|| recipe->sceneFormat == RAL_FORMAT_UNDEFINED
			|| recipe->depthFormat == RAL_FORMAT_UNDEFINED
			|| !VK_TemporalGenericCatalogKeyId( &recipe->key, &expectedCatalogId )
			|| expectedCatalogId != recipe->catalogId
			|| !VK_TemporalGenericCatalogSelect( &recipe->key, &catalog ) ) return qfalse;
	memset( &candidate, 0, sizeof(candidate) );
	candidate.vertexWord = recipe->specializationWords[0];
	memcpy( candidate.fragmentWords, &recipe->specializationWords[1],
		sizeof(candidate.fragmentWords) );
	if ( !VK_GenericSpecializationAuthor( &candidate.specialization,
			&candidate.vertexWord, candidate.fragmentWords ) ) return qfalse;
	candidate.stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	candidate.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	candidate.stages[0].module = ordinaryVertex;
	candidate.stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	candidate.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	candidate.stages[1].module = ordinaryFragment;
	candidate.vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	candidate.vertexInput.vertexBindingDescriptionCount = recipe->numBindings;
	candidate.vertexInput.vertexAttributeDescriptionCount = recipe->numAttributes;
	memcpy( candidate.bindings, recipe->bindings,
		recipe->numBindings * sizeof(candidate.bindings[0]) );
	memcpy( candidate.attributes, recipe->attributes,
		recipe->numAttributes * sizeof(candidate.attributes[0]) );
	candidate.inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	candidate.inputAssembly.topology = recipe->topology;
	candidate.inputAssembly.primitiveRestartEnable = recipe->primitiveRestart;
	candidate.viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	candidate.viewport.viewportCount = 1u;
	candidate.viewport.scissorCount = 1u;
	candidate.rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	candidate.rasterization.depthClampEnable = recipe->depthClampEnable;
	candidate.rasterization.rasterizerDiscardEnable = recipe->rasterizerDiscardEnable;
	candidate.rasterization.polygonMode = recipe->polygonMode;
	candidate.rasterization.cullMode = recipe->cullMode;
	candidate.rasterization.frontFace = recipe->frontFace;
	candidate.rasterization.depthBiasEnable = recipe->depthBiasEnable;
	candidate.rasterization.depthBiasConstantFactor = recipe->depthBiasConstant;
	candidate.rasterization.depthBiasClamp = recipe->depthBiasClamp;
	candidate.rasterization.depthBiasSlopeFactor = recipe->depthBiasSlope;
	candidate.rasterization.lineWidth = recipe->lineWidth;
	candidate.multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	candidate.multisample.rasterizationSamples = recipe->sampleCount;
	candidate.multisample.sampleShadingEnable = recipe->sampleShadingEnable;
	candidate.multisample.minSampleShading = recipe->minSampleShading;
	candidate.multisample.alphaToCoverageEnable = recipe->alphaToCoverageEnable;
	candidate.multisample.alphaToOneEnable = recipe->alphaToOneEnable;
	candidate.depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	candidate.depthStencil.depthTestEnable = recipe->depthTestEnable;
	candidate.depthStencil.depthWriteEnable = recipe->depthWriteEnable;
	candidate.depthStencil.depthCompareOp = recipe->depthCompareOp;
	candidate.depthStencil.depthBoundsTestEnable = recipe->depthBoundsTestEnable;
	candidate.depthStencil.stencilTestEnable = recipe->stencilTestEnable;
	candidate.depthStencil.front = recipe->stencilFront;
	candidate.depthStencil.back = recipe->stencilBack;
	candidate.depthStencil.minDepthBounds = recipe->minDepthBounds;
	candidate.depthStencil.maxDepthBounds = recipe->maxDepthBounds;
	candidate.sceneBlend = recipe->sceneBlend;
	candidate.colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	candidate.colorBlend.logicOpEnable = recipe->logicOpEnable;
	candidate.colorBlend.logicOp = recipe->logicOp;
	candidate.colorBlend.attachmentCount = 1u;
	memcpy( candidate.colorBlend.blendConstants, recipe->blendConstants,
		sizeof(candidate.colorBlend.blendConstants) );
	candidate.dynamicStates[0] = recipe->dynamicStates[0];
	candidate.dynamicStates[1] = recipe->dynamicStates[1];
	candidate.dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	candidate.dynamic.dynamicStateCount = 2u;
	candidate.gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	candidate.gp.stageCount = 2u;
	candidate.gp.layout = layout;
	candidate.gp.renderPass = VK_NULL_HANDLE;
	candidate.gp.basePipelineHandle = VK_NULL_HANDLE;
	candidate.gp.basePipelineIndex = -1;
	RepairViewPointers( &candidate );
	facts.textureCount = recipe->key.textureCount;
	facts.shaderFog = recipe->key.shaderFog;
	if ( !VK_GenericTemporalSpecializationValidate( &candidate.gp, &facts, NULL ) )
		return qfalse;
	*outView = candidate;
	RepairViewPointers( outView );
	return qtrue;
}

qboolean VK_TemporalGenericRecipeTableEvictRange(
		vkTemporalGenericRecipeTable_t *owner, uint32_t first, uint32_t end ) {
	uint32_t i;
	if ( !owner || first > end ) return qfalse;
	if ( !owner->records ) return qtrue;
	if ( end > owner->capacity ) return qfalse;
	for ( i = first; i < end; ++i )
		memset( &owner->records[i], 0, sizeof( owner->records[i] ) );
	return qtrue;
}

qboolean VK_TemporalGenericRecipeTableRelease(
		vkTemporalGenericRecipeTable_t *owner,
		const vkTemporalRecipeTableOps_t *ops ) {
	uint32_t epoch, generation;
	if ( !owner || !ops || !ops->free ) return qfalse;
	epoch = owner->ownerEpoch; generation = owner->lastEntryGeneration;
	if ( owner->records ) ops->free( owner->records );
	memset( owner, 0, sizeof( *owner ) );
	owner->ownerEpoch = epoch; owner->lastEntryGeneration = generation;
	return qtrue;
}
