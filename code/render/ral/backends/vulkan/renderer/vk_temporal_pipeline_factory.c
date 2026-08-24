// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_pipeline_factory.h"
#include "vk_generic_specialization_contract.h"

#include <limits.h>
#include <string.h>

static qboolean BlobValid( vkTemporalShaderBlob_t b ) {
	return b.bytes && b.size && ( b.size & 3u ) == 0u;
}

static qboolean BlobEqual( vkTemporalShaderBlob_t a, vkTemporalShaderBlob_t b ) {
	return a.bytes == b.bytes && a.size == b.size ? qtrue : qfalse;
}

#if defined(WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY)
static qboolean CatalogEqual( const vkTemporalPipelineBlobCatalog_t *a,
		const vkTemporalPipelineBlobCatalog_t *b ) {
	return a->generation == b->generation
		&& BlobEqual(a->ordinaryVertex,b->ordinaryVertex)
		&& BlobEqual(a->ordinaryFragment,b->ordinaryFragment)
		&& BlobEqual(a->temporalVertex,b->temporalVertex)
		&& BlobEqual(a->temporalWriteFragment,b->temporalWriteFragment)
		&& BlobEqual(a->temporalInvalidateFragment,b->temporalInvalidateFragment)
		&& BlobEqual(a->iqmVertex,b->iqmVertex)
		&& BlobEqual(a->iqmOrdinaryFragment,b->iqmOrdinaryFragment)
		&& BlobEqual(a->iqmInvalidateFragment,b->iqmInvalidateFragment);
}
#endif

static void FingerprintBytes( uint64_t *hash, const void *data, size_t size ) {
	const unsigned char *p = (const unsigned char *)data;
	while ( size-- ) {
		*hash ^= (uint64_t)*p++;
		*hash *= UINT64_C(1099511628211);
	}
}

static void FingerprintU32( uint64_t *hash, uint32_t value ) {
	FingerprintBytes( hash, &value, sizeof(value) );
}
static void FingerprintU64( uint64_t *hash, uint64_t value ) {
	FingerprintBytes( hash, &value, sizeof(value) );
}
static void FingerprintFloat( uint64_t *hash, float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof(bits) );
	FingerprintU32( hash, bits );
}

static void FingerprintStencil( uint64_t *hash, const VkStencilOpState *s ) {
	FingerprintU32(hash,(uint32_t)s->failOp); FingerprintU32(hash,(uint32_t)s->passOp);
	FingerprintU32(hash,(uint32_t)s->depthFailOp); FingerprintU32(hash,(uint32_t)s->compareOp);
	FingerprintU32(hash,s->compareMask); FingerprintU32(hash,s->writeMask);
	FingerprintU32(hash,s->reference);
}

// This definition-only cohort accepts a deliberately narrow, bounded Vk create
// graph. Hash both pointer identity and pointed-to state so in-place mutations
// cannot hit the idempotent path under an unchanged caller generation.
static qboolean BaseFingerprint( const VkGraphicsPipelineCreateInfo *base,
		uint64_t *out ) {
	uint64_t hash = UINT64_C(1469598103934665603);
	uint32_t i;
	if ( !base || !out || base->pNext || base->flags || base->stageCount != 2 || !base->pStages
			|| !base->pVertexInputState || !base->pInputAssemblyState
			|| !base->pViewportState || !base->pRasterizationState
			|| !base->pMultisampleState || !base->pDepthStencilState
			|| !base->pColorBlendState || !base->pDynamicState
			|| base->pTessellationState
			|| base->pVertexInputState->pNext || base->pVertexInputState->flags
			|| base->pInputAssemblyState->pNext || base->pInputAssemblyState->flags
			|| base->pViewportState->pNext || base->pViewportState->flags
			|| base->pRasterizationState->pNext || base->pRasterizationState->flags
			|| base->pMultisampleState->pNext || base->pMultisampleState->flags
			|| base->pDepthStencilState->pNext || base->pDepthStencilState->flags
			|| base->pColorBlendState->pNext || base->pColorBlendState->flags
			|| base->pDynamicState->pNext || base->pDynamicState->flags
			|| base->pStages[0].stage != VK_SHADER_STAGE_VERTEX_BIT
			|| base->pStages[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT
			|| base->pVertexInputState->vertexBindingDescriptionCount > 8u
			|| base->pVertexInputState->vertexAttributeDescriptionCount > 8u
			|| base->pViewportState->viewportCount != 1u
			|| base->pViewportState->scissorCount != 1u
			|| base->pColorBlendState->attachmentCount != 1u
			|| !base->pColorBlendState->pAttachments
			|| base->pDynamicState->dynamicStateCount != 2u
			|| !base->pDynamicState->pDynamicStates
			|| base->pInputAssemblyState->primitiveRestartEnable
			|| base->pInputAssemblyState->topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
			|| base->pRasterizationState->rasterizerDiscardEnable
			|| base->pRasterizationState->polygonMode != VK_POLYGON_MODE_FILL
			|| (base->pRasterizationState->cullMode != VK_CULL_MODE_NONE
				&& base->pRasterizationState->cullMode != VK_CULL_MODE_FRONT_BIT
				&& base->pRasterizationState->cullMode != VK_CULL_MODE_BACK_BIT)
			|| (base->pRasterizationState->frontFace != VK_FRONT_FACE_CLOCKWISE
				&& base->pRasterizationState->frontFace != VK_FRONT_FACE_COUNTER_CLOCKWISE)
			|| base->pMultisampleState->rasterizationSamples != VK_SAMPLE_COUNT_1_BIT
			|| base->pMultisampleState->sampleShadingEnable
			|| base->pMultisampleState->pSampleMask
			|| base->pMultisampleState->alphaToCoverageEnable
			|| base->pMultisampleState->alphaToOneEnable
			|| base->pDepthStencilState->depthBoundsTestEnable
			|| base->pColorBlendState->logicOpEnable ) return qfalse;
	if ( !((base->pDynamicState->pDynamicStates[0] == VK_DYNAMIC_STATE_VIEWPORT
			&& base->pDynamicState->pDynamicStates[1] == VK_DYNAMIC_STATE_SCISSOR)
			|| (base->pDynamicState->pDynamicStates[1] == VK_DYNAMIC_STATE_VIEWPORT
			&& base->pDynamicState->pDynamicStates[0] == VK_DYNAMIC_STATE_SCISSOR)) ) return qfalse;
	FingerprintU32(&hash,(uint32_t)base->flags);
	FingerprintU64(&hash,(uint64_t)(uintptr_t)base->layout);
	FingerprintU64(&hash,(uint64_t)(uintptr_t)base->renderPass);
	FingerprintU32(&hash,base->subpass);
	FingerprintU64(&hash,(uint64_t)(uintptr_t)base->basePipelineHandle);
	FingerprintU32(&hash,(uint32_t)base->basePipelineIndex);
	for ( i = 0; i < base->stageCount; ++i ) {
		const VkPipelineShaderStageCreateInfo *stage = &base->pStages[i];
		const VkSpecializationInfo *si = stage->pSpecializationInfo;
		if ( stage->pNext || stage->flags || !stage->pName || strcmp( stage->pName, "main" ) != 0 ) return qfalse;
		FingerprintU32(&hash,(uint32_t)stage->flags);
		FingerprintU32(&hash,(uint32_t)stage->stage);
		FingerprintU64(&hash,(uint64_t)(uintptr_t)stage->module);
		FingerprintBytes( &hash, stage->pName, 5u );
		if ( si ) {
			if ( si->mapEntryCount > 16u || si->dataSize > 128u
					|| (si->mapEntryCount && !si->pMapEntries)
					|| (si->dataSize && !si->pData) ) return qfalse;
			FingerprintU32(&hash,si->mapEntryCount);
			FingerprintU64(&hash,(uint64_t)si->dataSize);
			for ( uint32_t j=0; j<si->mapEntryCount; ++j ) {
				FingerprintU32(&hash,si->pMapEntries[j].constantID);
				FingerprintU32(&hash,si->pMapEntries[j].offset);
				FingerprintU64(&hash,(uint64_t)si->pMapEntries[j].size);
			}
			FingerprintBytes( &hash, si->pData, si->dataSize );
		} else {
			FingerprintU32(&hash,0u); FingerprintU64(&hash,0u);
		}
	}
	FingerprintU32(&hash,(uint32_t)base->pVertexInputState->flags);
	FingerprintU32(&hash,base->pVertexInputState->vertexBindingDescriptionCount);
	FingerprintU32(&hash,base->pVertexInputState->vertexAttributeDescriptionCount);
	if ( base->pVertexInputState->vertexBindingDescriptionCount
			&& !base->pVertexInputState->pVertexBindingDescriptions ) return qfalse;
	if ( base->pVertexInputState->vertexAttributeDescriptionCount
			&& !base->pVertexInputState->pVertexAttributeDescriptions ) return qfalse;
	for(i=0;i<base->pVertexInputState->vertexBindingDescriptionCount;++i){
		const VkVertexInputBindingDescription *b=&base->pVertexInputState->pVertexBindingDescriptions[i];
		if(b->inputRate!=VK_VERTEX_INPUT_RATE_VERTEX)return qfalse;
		FingerprintU32(&hash,b->binding);FingerprintU32(&hash,b->stride);FingerprintU32(&hash,(uint32_t)b->inputRate);
	}
	for(i=0;i<base->pVertexInputState->vertexAttributeDescriptionCount;++i){
		const VkVertexInputAttributeDescription *a=&base->pVertexInputState->pVertexAttributeDescriptions[i];
		FingerprintU32(&hash,a->location);FingerprintU32(&hash,a->binding);FingerprintU32(&hash,(uint32_t)a->format);FingerprintU32(&hash,a->offset);
	}
	FingerprintU32(&hash,(uint32_t)base->pInputAssemblyState->flags);
	FingerprintU32(&hash,(uint32_t)base->pInputAssemblyState->topology);
	FingerprintU32(&hash,(uint32_t)base->pInputAssemblyState->primitiveRestartEnable);
	FingerprintU32(&hash,(uint32_t)base->pRasterizationState->flags);
	FingerprintU32(&hash,(uint32_t)base->pRasterizationState->depthClampEnable);
	FingerprintU32(&hash,(uint32_t)base->pRasterizationState->polygonMode);
	FingerprintU32(&hash,(uint32_t)base->pRasterizationState->cullMode);
	FingerprintU32(&hash,(uint32_t)base->pRasterizationState->frontFace);
	FingerprintU32(&hash,(uint32_t)base->pRasterizationState->depthBiasEnable);
	FingerprintFloat(&hash,base->pRasterizationState->depthBiasConstantFactor);
	FingerprintFloat(&hash,base->pRasterizationState->depthBiasClamp);
	FingerprintFloat(&hash,base->pRasterizationState->depthBiasSlopeFactor);
	FingerprintFloat(&hash,base->pRasterizationState->lineWidth);
	FingerprintU32(&hash,(uint32_t)base->pMultisampleState->rasterizationSamples);
	FingerprintU32(&hash,(uint32_t)base->pDepthStencilState->flags);
	FingerprintU32(&hash,(uint32_t)base->pDepthStencilState->depthTestEnable);
	FingerprintU32(&hash,(uint32_t)base->pDepthStencilState->depthWriteEnable);
	FingerprintU32(&hash,(uint32_t)base->pDepthStencilState->depthCompareOp);
	FingerprintU32(&hash,(uint32_t)base->pDepthStencilState->stencilTestEnable);
	FingerprintStencil(&hash,&base->pDepthStencilState->front);
	FingerprintStencil(&hash,&base->pDepthStencilState->back);
	FingerprintU32(&hash,(uint32_t)base->pColorBlendState->flags);
	{
		const VkPipelineColorBlendAttachmentState *a=base->pColorBlendState->pAttachments;
		FingerprintU32(&hash,(uint32_t)a->blendEnable);FingerprintU32(&hash,(uint32_t)a->srcColorBlendFactor);
		FingerprintU32(&hash,(uint32_t)a->dstColorBlendFactor);FingerprintU32(&hash,(uint32_t)a->colorBlendOp);
		FingerprintU32(&hash,(uint32_t)a->srcAlphaBlendFactor);FingerprintU32(&hash,(uint32_t)a->dstAlphaBlendFactor);
		FingerprintU32(&hash,(uint32_t)a->alphaBlendOp);FingerprintU32(&hash,(uint32_t)a->colorWriteMask);
	}
	FingerprintU32(&hash,(uint32_t)base->pDynamicState->flags);
	FingerprintU32(&hash,(uint32_t)base->pDynamicState->pDynamicStates[0]);
	FingerprintU32(&hash,(uint32_t)base->pDynamicState->pDynamicStates[1]);
	*out = hash;
	return qtrue;
}

void VK_TemporalPipelineLayoutInit( vkTemporalPipelineLayoutOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalPipelineLayoutEnsure( vkTemporalPipelineLayoutOwner_t *owner,
		ralBackend_t *backend, VkDevice device,
		const ralBindGroupLayout_t *const borrowedLayouts[3],
		const ralBindGroupLayout_t *payloadLayout, uint32_t payloadLayoutGeneration,
		qboolean fog, const vkTemporalLayoutOps_t *ops ) {
	const ralBindGroupLayout_t *layouts[4];
	ralPipelineLayoutCreateInfo_t ci;
	VkPipelineLayout candidateRaw = VK_NULL_HANDLE;
	ralPipelineLayout_t *candidateAdopted = NULL;
	uint32_t nextGeneration;
	uint32_t i;

	if ( !owner || !backend || device == VK_NULL_HANDLE || !borrowedLayouts
			|| !payloadLayout || !payloadLayoutGeneration || !ops || !ops->create
			|| !ops->getHandle || !ops->destroy ) return qfalse;
	for ( i = 0; i < 3; ++i ) if ( !borrowedLayouts[i] ) return qfalse;
	if ( owner->ready && owner->backend == backend && owner->device == device
			&& owner->payloadLayout == payloadLayout
			&& owner->payloadLayoutGeneration == payloadLayoutGeneration
			&& owner->fog == fog
			&& memcmp( owner->borrowedLayouts, borrowedLayouts,
				sizeof(owner->borrowedLayouts) ) == 0 ) return qtrue;
	if ( owner->ready && ( owner->leases || owner->backend != backend || owner->device != device ) ) return qfalse;
	if ( owner->allocationGeneration == UINT32_MAX ) return qfalse;
	memcpy( layouts, borrowedLayouts, sizeof(owner->borrowedLayouts) );
	layouts[3] = payloadLayout;
	memset( &ci, 0, sizeof( ci ) );
	ci.bindGroupLayouts = layouts;
	ci.numBindGroupLayouts = 4u;
	// Fog state is part of the ordinary set-0 UBO; temporal layouts are push-free.
	ci.debugName = "wired-temporal-main-layout";
	candidateAdopted = ops->create( backend, &ci );
	if ( !candidateAdopted || candidateAdopted == owner->adopted ) goto fail;
	candidateRaw = (VkPipelineLayout)ops->getHandle( candidateAdopted );
	if ( candidateRaw == VK_NULL_HANDLE || candidateRaw == owner->raw ) goto fail;
	nextGeneration = owner->allocationGeneration + 1u;
	if ( owner->adopted ) ops->destroy( owner->adopted );
	owner->backend = backend;
	owner->device = device;
	memcpy( owner->borrowedLayouts, borrowedLayouts,
		sizeof(owner->borrowedLayouts) );
	owner->payloadLayout = payloadLayout;
	owner->payloadLayoutGeneration = payloadLayoutGeneration;
	owner->raw = candidateRaw;
	owner->adopted = candidateAdopted;
	owner->allocationGeneration = nextGeneration;
	owner->fog = fog;
	owner->ready = qtrue;
	return qtrue;
fail:
	if ( candidateAdopted && candidateAdopted != owner->adopted )
		ops->destroy( candidateAdopted );
	return qfalse;
}

qboolean VK_TemporalPipelineLayoutAcquire( vkTemporalPipelineLayoutOwner_t *owner ) {
	if ( !owner || !owner->ready || !owner->adopted || owner->leases == UINT32_MAX ) return qfalse;
	owner->leases++;
	return qtrue;
}
qboolean VK_TemporalPipelineLayoutReleaseLease( vkTemporalPipelineLayoutOwner_t *owner ) {
	if ( !owner || !owner->ready || !owner->leases ) return qfalse;
	owner->leases--;
	return qtrue;
}
qboolean VK_TemporalPipelineLayoutRelease( vkTemporalPipelineLayoutOwner_t *owner,
		const vkTemporalLayoutOps_t *ops ) {
	uint32_t generation;
	if ( !owner || !ops || !ops->destroy || owner->leases ) return qfalse;
	generation = owner->allocationGeneration;
	if ( owner->adopted ) ops->destroy( owner->adopted );
	memset( owner, 0, sizeof( *owner ) );
	owner->allocationGeneration = generation;
	return qtrue;
}

#if defined(WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY)
void VK_TemporalPipelineFactoryInit( vkTemporalPipelineFactoryOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

static void DestroyCandidates( ralPipeline_t **p, uint32_t count,
		const vkTemporalPipelineFactoryOps_t *ops,
		const vkTemporalPipelineFactoryOwner_t *live ) {
	uint32_t i, j;
	for ( i = count; i > 0; --i ) {
		ralPipeline_t *v = p[i - 1u];
		qboolean duplicate = qfalse;
		if ( !v ) continue;
		for ( j = i; j < count; ++j ) if ( p[j] == v ) duplicate = qtrue;
		if ( live && ( live->iqmInvalidate == v || live->generic[0] == v
				|| live->generic[1] == v || live->generic[2] == v ) ) duplicate = qtrue;
		if ( !duplicate ) ops->destroy( v );
	}
}

qboolean VK_TemporalPipelineFactoryEnsure( vkTemporalPipelineFactoryOwner_t *owner,
		vkTemporalPipelineLayoutOwner_t *layoutOwner, ralPipelineLayout_t *iqmLayout,
		const VkGraphicsPipelineCreateInfo *genericBase,
		const VkGraphicsPipelineCreateInfo *iqmBase,
		const vkTemporalPipelineFactoryInput_t *input,
		const vkTemporalPipelineBlobCatalog_t *blobs,
		const vkTemporalPipelineFactoryOps_t *ops ) {
	ralPipeline_t *candidate[4] = { NULL, NULL, NULL, NULL };
	vkTemporalSpirvOverrides_t override;
	ralFormat_t formats[3];
	ralColorBlendAttachment_t sceneBlend;
	vkTemporalShaderRecipeInput_t ri;
	vkTemporalShaderRecipe_t recipe;
	VkGraphicsPipelineCreateInfo exact;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineColorBlendAttachmentState vkBlends[3];
	uint32_t i, nextGeneration;
	vkTemporalShaderBlob_t baseVs, baseFs, iqmVs, iqmFs;
	const vkGenericSpecializationFacts_t genericFacts = { 1u, qfalse, qfalse };
	uint64_t genericFingerprint, iqmFingerprint;

	if ( !owner || !layoutOwner || !layoutOwner->ready || !layoutOwner->adopted
			|| !iqmLayout || !genericBase || !iqmBase || !input || !blobs || !ops
			|| !ops->create || !ops->destroy || !ops->drain || !ops->lookup || !input->pipelineGeneration
			|| !input->topologyGeneration || !input->iqmLayoutGeneration
			|| !blobs->generation || !input->exactTx1
			|| input->alphaTested || input->depthOnly || input->blended
			|| input->special || input->dynamicDiscard || layoutOwner->fog
			|| input->sceneFormat == RAL_FORMAT_UNDEFINED
			|| input->depthFormat == RAL_FORMAT_UNDEFINED
			|| !genericBase->pColorBlendState || genericBase->pColorBlendState->attachmentCount != 1
			|| !genericBase->pColorBlendState->pAttachments
			|| !iqmBase->pColorBlendState || iqmBase->pColorBlendState->attachmentCount != 1
			|| !iqmBase->pColorBlendState->pAttachments
			|| genericBase->pColorBlendState->pAttachments[0].blendEnable
			|| iqmBase->pColorBlendState->pAttachments[0].blendEnable
			|| !VK_GenericTemporalSpecializationValidate( genericBase, &genericFacts, NULL )
			|| !BaseFingerprint( genericBase, &genericFingerprint )
			|| !BaseFingerprint( iqmBase, &iqmFingerprint ) ) return qfalse;
	if ( owner->retiringLease ) {
		if ( !ops->drain( owner->retiringLayout->backend ) ) return qfalse;
		VK_TemporalPipelineLayoutReleaseLease( owner->retiringLayout );
		owner->retiringLayout = NULL;
		owner->retiringLease = qfalse;
	}
	if ( !BlobValid(blobs->ordinaryVertex) || !BlobValid(blobs->ordinaryFragment)
			|| !BlobValid(blobs->temporalVertex) || !BlobValid(blobs->temporalWriteFragment)
			|| !BlobValid(blobs->temporalInvalidateFragment) || !BlobValid(blobs->iqmVertex)
			|| !BlobValid(blobs->iqmOrdinaryFragment)
			|| !BlobValid(blobs->iqmInvalidateFragment) ) return qfalse;
	if ( genericBase->stageCount != 2 || !genericBase->pStages
			|| genericBase->pStages[0].stage != VK_SHADER_STAGE_VERTEX_BIT
			|| genericBase->pStages[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT
			|| iqmBase->stageCount != 2 || !iqmBase->pStages
			|| iqmBase->pStages[0].stage != VK_SHADER_STAGE_VERTEX_BIT
			|| iqmBase->pStages[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT
			|| !ops->lookup(genericBase->pStages[0].module,&baseVs)
			|| !ops->lookup(genericBase->pStages[1].module,&baseFs)
			|| !ops->lookup(iqmBase->pStages[0].module,&iqmVs)
			|| !ops->lookup(iqmBase->pStages[1].module,&iqmFs)
			|| !BlobEqual(baseVs,blobs->ordinaryVertex)
			|| !BlobEqual(baseFs,blobs->ordinaryFragment)
			|| !BlobEqual(iqmVs,blobs->iqmVertex)
			|| !BlobEqual(iqmFs,blobs->iqmOrdinaryFragment) ) return qfalse;
	if ( owner->ready && owner->layoutOwner == layoutOwner
			&& owner->pipelineGeneration == input->pipelineGeneration
			&& owner->topologyGeneration == input->topologyGeneration
			&& owner->blobGeneration == blobs->generation
			&& owner->layoutAllocationGeneration == layoutOwner->allocationGeneration
			&& owner->genericBaseFingerprint == genericFingerprint
			&& owner->iqmBaseFingerprint == iqmFingerprint
			&& owner->sceneFormat == input->sceneFormat && owner->depthFormat == input->depthFormat
			&& owner->iqmLayout == iqmLayout
			&& owner->iqmLayoutGeneration == input->iqmLayoutGeneration
			&& CatalogEqual( &owner->blobs, blobs ) ) return qtrue;
	if ( owner->allocationGeneration == UINT32_MAX ) return qfalse;
	memset( &sceneBlend, 0, sizeof( sceneBlend ) );
	sceneBlend.writeMask = RAL_COLOR_WRITE_ALL;
	sceneBlend.writeMaskExplicit = qtrue;
	memset( &ri, 0, sizeof( ri ) );
	ri.sceneFormat = input->sceneFormat;
	ri.sceneBlend = sceneBlend;
	for ( i = 0; i < 3; ++i ) ri.genericSetLayouts[i] = layoutOwner->borrowedLayouts[i];
	ri.genericSetLayouts[3] = layoutOwner->payloadLayout;
	ri.iqmSetLayouts[0] = (const void *)1;
	ri.iqmSetLayouts[1] = (const void *)2;
	ri.ordinaryVertex = blobs->ordinaryVertex; ri.ordinaryFragment = blobs->ordinaryFragment;
	ri.temporalVertex = blobs->temporalVertex; ri.temporalWriteFragment = blobs->temporalWriteFragment;
	ri.temporalInvalidateFragment = blobs->temporalInvalidateFragment;
	ri.iqmVertex = blobs->iqmVertex; ri.iqmInvalidateFragment = blobs->iqmInvalidateFragment;
	for ( i = 0; i < 3; ++i ) {
		ri.kind = (vkTemporalShaderRecipeKind_t)(VK_TEMPORAL_SHADER_PRESERVE + i);
		if ( !VK_TemporalShaderRecipeBuild( &ri, &recipe ) ) goto fail;
		memcpy( formats, recipe.colorFormats, sizeof( formats ) );
		override.vertex = recipe.vertex; override.fragment = recipe.fragment;
		exact = *genericBase;
		colorBlend = *genericBase->pColorBlendState;
		memset( vkBlends, 0, sizeof(vkBlends) );
		vkBlends[0] = genericBase->pColorBlendState->pAttachments[0];
		vkBlends[1].colorWriteMask = recipe.colorBlends[1].writeMask;
		vkBlends[2].colorWriteMask = recipe.colorBlends[2].writeMask;
		colorBlend.attachmentCount = 3;
		colorBlend.pAttachments = vkBlends;
		exact.pColorBlendState = &colorBlend;
		candidate[i] = ops->create( &exact, layoutOwner->adopted, formats, 3,
			input->depthFormat, &override, "wired-temporal-generic" );
		if ( !candidate[i] ) goto fail;
	}
	ri.kind = VK_TEMPORAL_SHADER_IQM_INVALIDATE;
	if ( !VK_TemporalShaderRecipeBuild( &ri, &recipe ) ) goto fail;
	override.vertex = recipe.vertex; override.fragment = recipe.fragment;
	exact = *iqmBase;
	colorBlend = *iqmBase->pColorBlendState;
	memset( vkBlends, 0, sizeof(vkBlends) );
	vkBlends[0] = iqmBase->pColorBlendState->pAttachments[0];
	vkBlends[1].colorWriteMask = recipe.colorBlends[1].writeMask;
	vkBlends[2].colorWriteMask = recipe.colorBlends[2].writeMask;
	colorBlend.attachmentCount = 3;
	colorBlend.pAttachments = vkBlends;
	exact.pColorBlendState = &colorBlend;
	candidate[3] = ops->create( &exact, iqmLayout, recipe.colorFormats, 3,
		input->depthFormat, &override, "wired-temporal-iqm-invalidate" );
	if ( !candidate[3] ) goto fail;
	for ( i = 0; i < 4; ++i ) {
		uint32_t j;
		for ( j = 0; j < i; ++j ) if ( candidate[i] == candidate[j] ) goto fail;
		if ( owner->ready && (candidate[i] == owner->iqmInvalidate
				|| candidate[i] == owner->generic[0] || candidate[i] == owner->generic[1]
				|| candidate[i] == owner->generic[2]) ) goto fail;
	}
	if ( !VK_TemporalPipelineLayoutAcquire( layoutOwner ) ) goto fail;
	nextGeneration = owner->allocationGeneration + 1u;
	if ( owner->ready ) {
		ralPipeline_t *old[4] = {owner->generic[0],owner->generic[1],owner->generic[2],owner->iqmInvalidate};
		vkTemporalPipelineLayoutOwner_t *oldLayout = owner->layoutOwner;
		DestroyCandidates( old, 4, ops, NULL );
		owner->retiringLayout = oldLayout;
		owner->retiringLease = qtrue;
	}
	memcpy( owner->generic, candidate, sizeof(owner->generic) );
	owner->iqmInvalidate = candidate[3];
	owner->layoutOwner = layoutOwner;
	owner->pipelineGeneration = input->pipelineGeneration;
	owner->topologyGeneration = input->topologyGeneration;
	owner->blobGeneration = blobs->generation;
	owner->allocationGeneration = nextGeneration;
	owner->layoutAllocationGeneration = layoutOwner->allocationGeneration;
	owner->genericBaseFingerprint = genericFingerprint;
	owner->iqmBaseFingerprint = iqmFingerprint;
	owner->sceneFormat = input->sceneFormat;
	owner->depthFormat = input->depthFormat;
	owner->iqmLayout = iqmLayout;
	owner->iqmLayoutGeneration = input->iqmLayoutGeneration;
	owner->blobs = *blobs;
	owner->ready = qtrue;
	if ( owner->retiringLease && ops->drain( owner->retiringLayout->backend ) ) {
		VK_TemporalPipelineLayoutReleaseLease( owner->retiringLayout );
		owner->retiringLayout = NULL;
		owner->retiringLease = qfalse;
	}
	return qtrue;
fail:
	DestroyCandidates( candidate, 4, ops, owner );
	return qfalse;
}

qboolean VK_TemporalPipelineFactoryRelease( vkTemporalPipelineFactoryOwner_t *owner,
		const vkTemporalPipelineFactoryOps_t *ops ) {
	ralPipeline_t *old[4];
	vkTemporalPipelineLayoutOwner_t *layout;
	if ( !owner || !ops || !ops->destroy || !ops->drain ) return qfalse;
	if ( owner->retiringLease ) {
		if ( !ops->drain( owner->retiringLayout->backend ) ) return qfalse;
		VK_TemporalPipelineLayoutReleaseLease( owner->retiringLayout );
		owner->retiringLayout=NULL;
		owner->retiringLease=qfalse;
	}
	if ( owner->ready ) {
		old[0]=owner->generic[0]; old[1]=owner->generic[1]; old[2]=owner->generic[2]; old[3]=owner->iqmInvalidate;
		layout = owner->layoutOwner;
		DestroyCandidates( old, 4, ops, NULL );
		owner->generic[0]=owner->generic[1]=owner->generic[2]=NULL;
		owner->iqmInvalidate=NULL;
		owner->ready=qfalse;
		owner->retiringLayout=layout;
		owner->retiringLease=layout ? qtrue : qfalse;
	}
	if ( owner->retiringLease ) {
		if ( !ops->drain( owner->retiringLayout->backend ) ) return qfalse;
		VK_TemporalPipelineLayoutReleaseLease( owner->retiringLayout );
	}
	memset( owner, 0, sizeof( *owner ) );
	return qtrue;
}
#endif

static void DestroyGenericCandidates( ralPipeline_t **p, uint32_t count,
		const vkTemporalPipelineFactoryOps_t *ops,
		const vkTemporalGenericPipelineFactoryOwner_t *live ) {
	uint32_t i,j;
	for(i=count;i>0;--i){ralPipeline_t *v=p[i-1u];qboolean duplicate=qfalse;
		if(!v)continue;for(j=i;j<count;++j)if(p[j]==v)duplicate=qtrue;
		if(live&&(live->pipelines[0]==v||live->pipelines[1]==v||live->pipelines[2]==v))duplicate=qtrue;
		if(!duplicate)ops->destroy(v);}
}

static qboolean GenericEntryEqual( const vkTemporalGenericCatalogEntry_t *a,
		const vkTemporalGenericCatalogEntry_t *b ) {
	return a->key.textureCount == b->key.textureCount
		&& a->key.family == b->key.family
		&& a->key.environment == b->key.environment
		&& a->key.shaderFog == b->key.shaderFog
		&& BlobEqual(a->ordinaryVertex,b->ordinaryVertex)
		&& BlobEqual(a->ordinaryFragment,b->ordinaryFragment)
		&& BlobEqual(a->temporalVertex,b->temporalVertex)
		&& BlobEqual(a->temporalWriteFragment,b->temporalWriteFragment)
		&& BlobEqual(a->temporalInvalidateFragment,b->temporalInvalidateFragment);
}

void VK_TemporalGenericPipelineFactoryInit( vkTemporalGenericPipelineFactoryOwner_t *owner ) {
	if ( owner ) memset(owner,0,sizeof(*owner));
}

qboolean VK_TemporalGenericPipelineFactoryEnsure( vkTemporalGenericPipelineFactoryOwner_t *owner,
		vkTemporalPipelineLayoutOwner_t *layoutOwner,
		const VkGraphicsPipelineCreateInfo *base,
		const vkTemporalGenericPipelineFactoryInput_t *input,
		const vkTemporalPipelineFactoryOps_t *ops ) {
	vkTemporalGenericCatalogEntry_t catalog;
	vkTemporalShaderRecipeInput_t ri;
	vkTemporalShaderRecipe_t recipe;
	vkTemporalSpirvOverrides_t override;
	vkTemporalShaderBlob_t ordinaryVs, ordinaryFs;
	ralPipeline_t *candidate[3]={NULL,NULL,NULL};
	VkGraphicsPipelineCreateInfo exact;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineColorBlendAttachmentState vkBlends[3];
	ralColorBlendAttachment_t sceneBlend;
	vkGenericSpecializationFacts_t specializationFacts;
	uint64_t fingerprint;
	uint32_t i,nextGeneration;
	if(!owner||!layoutOwner||!layoutOwner->ready||!layoutOwner->adopted||!base||!input||!ops
			||!ops->create||!ops->destroy||!ops->drain||!ops->lookup
			||!layoutOwner->leases
			||!input->pipelineGeneration||!input->topologyGeneration||!input->catalogGeneration
			||input->sceneFormat==RAL_FORMAT_UNDEFINED||input->depthFormat==RAL_FORMAT_UNDEFINED
			||(input->alphaTested&&input->key.textureCount!=0u)
			||input->depthOnly||input->blended||input->special||input->dynamicDiscard
			||!base->pColorBlendState||base->pColorBlendState->attachmentCount!=1u
			||!base->pColorBlendState->pAttachments||base->pColorBlendState->pAttachments[0].blendEnable
			||!VK_TemporalGenericCatalogSelect(&input->key,&catalog)) return qfalse;
	specializationFacts.textureCount=input->key.textureCount;
	specializationFacts.shaderFog=input->key.shaderFog;
	specializationFacts.alphaTested=input->alphaTested;
	if(!VK_GenericTemporalSpecializationValidate(base,&specializationFacts,NULL)
			||!BaseFingerprint(base,&fingerprint))return qfalse;
	if(base->stageCount!=2u||!base->pStages
			||!ops->lookup(base->pStages[0].module,&ordinaryVs)
			||!ops->lookup(base->pStages[1].module,&ordinaryFs)
			||!BlobEqual(ordinaryVs,catalog.ordinaryVertex)
			||!BlobEqual(ordinaryFs,catalog.ordinaryFragment)) return qfalse;
	if(owner->retiringLease){
		if(!ops->drain(owner->retiringLayout->backend))return qfalse;
		VK_TemporalPipelineLayoutReleaseLease(owner->retiringLayout);
		owner->retiringLayout=NULL;owner->retiringLease=qfalse;
	}
	if(owner->ready&&owner->layoutOwner==layoutOwner
			&&owner->pipelineGeneration==input->pipelineGeneration
			&&owner->topologyGeneration==input->topologyGeneration
			&&owner->catalogGeneration==input->catalogGeneration
			&&owner->layoutAllocationGeneration==layoutOwner->allocationGeneration
			&&owner->baseFingerprint==fingerprint&&owner->sceneFormat==input->sceneFormat
			&&owner->depthFormat==input->depthFormat&&GenericEntryEqual(&owner->catalog,&catalog))return qtrue;
	if(owner->allocationGeneration==UINT32_MAX)return qfalse;
	memset(&sceneBlend,0,sizeof(sceneBlend));sceneBlend.writeMask=RAL_COLOR_WRITE_ALL;sceneBlend.writeMaskExplicit=qtrue;
	memset(&ri,0,sizeof(ri));ri.sceneFormat=input->sceneFormat;ri.sceneBlend=sceneBlend;ri.fog=layoutOwner->fog;
	for(i=0;i<3u;++i)ri.genericSetLayouts[i]=layoutOwner->borrowedLayouts[i];
	ri.genericSetLayouts[3]=layoutOwner->payloadLayout;
	ri.ordinaryVertex=catalog.ordinaryVertex;ri.ordinaryFragment=catalog.ordinaryFragment;
	ri.temporalVertex=catalog.temporalVertex;ri.temporalWriteFragment=catalog.temporalWriteFragment;
	ri.temporalInvalidateFragment=catalog.temporalInvalidateFragment;
	for(i=0;i<3u;++i){
		ri.kind=(vkTemporalShaderRecipeKind_t)(VK_TEMPORAL_SHADER_PRESERVE+i);
		if(!VK_TemporalShaderRecipeBuild(&ri,&recipe))goto fail;
		override.vertex=recipe.vertex;override.fragment=recipe.fragment;
		exact=*base;colorBlend=*base->pColorBlendState;memset(vkBlends,0,sizeof(vkBlends));
		vkBlends[0]=base->pColorBlendState->pAttachments[0];
		vkBlends[1].colorWriteMask=recipe.colorBlends[1].writeMask;
		vkBlends[2].colorWriteMask=recipe.colorBlends[2].writeMask;
		colorBlend.attachmentCount=3u;colorBlend.pAttachments=vkBlends;exact.pColorBlendState=&colorBlend;
		candidate[i]=ops->create(&exact,layoutOwner->adopted,recipe.colorFormats,3u,
			input->depthFormat,&override,"wired-temporal-generic");
		if(!candidate[i])goto fail;
		for(uint32_t j=0;j<i;++j)if(candidate[i]==candidate[j])goto fail;
		if(owner->ready&&(candidate[i]==owner->pipelines[0]||candidate[i]==owner->pipelines[1]
				||candidate[i]==owner->pipelines[2]))goto fail;
		if(ops->candidateAllowed
				&& !ops->candidateAllowed(candidate[i],ops->candidateContext)){
			// A rejected candidate is an alias of a live pipeline owned outside
			// this factory. Never route that borrowed handle through cleanup.
			candidate[i]=NULL;goto fail;
		}
	}
	if(!VK_TemporalPipelineLayoutAcquire(layoutOwner))goto fail;
	nextGeneration=owner->allocationGeneration+1u;
	if(owner->ready){
		DestroyGenericCandidates(owner->pipelines,3u,ops,NULL);
		owner->retiringLayout=owner->layoutOwner;owner->retiringLease=qtrue;
	}
	memcpy(owner->pipelines,candidate,sizeof(candidate));owner->layoutOwner=layoutOwner;
	owner->catalog=catalog;owner->pipelineGeneration=input->pipelineGeneration;
	owner->topologyGeneration=input->topologyGeneration;owner->catalogGeneration=input->catalogGeneration;
	owner->allocationGeneration=nextGeneration;owner->layoutAllocationGeneration=layoutOwner->allocationGeneration;
	owner->baseFingerprint=fingerprint;owner->sceneFormat=input->sceneFormat;owner->depthFormat=input->depthFormat;owner->ready=qtrue;
	if(owner->retiringLease){
		// Publication is not reported complete while retired children still hold
		// their parent lease. A later Ensure/Release retries the same drain.
		if(!ops->drain(owner->retiringLayout->backend))return qfalse;
		VK_TemporalPipelineLayoutReleaseLease(owner->retiringLayout);
		owner->retiringLayout=NULL;owner->retiringLease=qfalse;
	}
	return qtrue;
fail:
	DestroyGenericCandidates(candidate,3u,ops,owner);return qfalse;
}

qboolean VK_TemporalGenericPipelineFactoryRelease( vkTemporalGenericPipelineFactoryOwner_t *owner,
		const vkTemporalPipelineFactoryOps_t *ops ) {
	if(!owner||!ops||!ops->destroy||!ops->drain)return qfalse;
	if(!VK_TemporalGenericPipelineFactoryRetire(owner,ops))return qfalse;
	if(owner->retiringLease&&!ops->drain(owner->retiringLayout->backend))return qfalse;
	return VK_TemporalGenericPipelineFactoryFinalizeAfterDrain(owner);
}

qboolean VK_TemporalGenericPipelineFactoryRetire(
		vkTemporalGenericPipelineFactoryOwner_t *owner,
		const vkTemporalPipelineFactoryOps_t *ops ) {
	if(!owner||!ops||!ops->destroy||!ops->drain)return qfalse;
	if(owner->retiringLease){
		// An earlier Retire already enqueued this owner's children. The table's
		// shared drain must cover all such owners exactly once.
		if(!owner->ready)return qtrue;
		// A replacement whose first drain failed remains readable but cannot be
		// retired until the old generation's deferred children are flushed.
		if(!owner->retiringLayout
				||!ops->drain(owner->retiringLayout->backend))return qfalse;
		if(!VK_TemporalPipelineLayoutReleaseLease(owner->retiringLayout))return qfalse;
		owner->retiringLayout=NULL;owner->retiringLease=qfalse;
	}
	if(!owner->ready)return qtrue;
	DestroyGenericCandidates(owner->pipelines,3u,ops,NULL);
	owner->pipelines[0]=owner->pipelines[1]=owner->pipelines[2]=NULL;
	owner->retiringLayout=owner->layoutOwner;
	owner->retiringLease=owner->retiringLayout?qtrue:qfalse;
	owner->ready=qfalse;
	return owner->retiringLease;
}

qboolean VK_TemporalGenericPipelineFactoryFinalizeAfterDrain(
		vkTemporalGenericPipelineFactoryOwner_t *owner ) {
	uint32_t generation;
	if(!owner||owner->ready)return qfalse;
	if(owner->retiringLease){
		if(!owner->retiringLayout
				||!VK_TemporalPipelineLayoutReleaseLease(owner->retiringLayout))return qfalse;
	}
	generation=owner->allocationGeneration;
	memset(owner,0,sizeof(*owner));
	owner->allocationGeneration=generation;
	return qtrue;
}

void VK_TemporalIqmPipelineFactoryInit( vkTemporalIqmPipelineFactoryOwner_t *owner ) {
	if(owner)memset(owner,0,sizeof(*owner));
}

qboolean VK_TemporalIqmPipelineFactoryEnsure( vkTemporalIqmPipelineFactoryOwner_t *owner,
		ralBackend_t *backend,ralPipelineLayout_t *layout,uint32_t layoutGeneration,const VkGraphicsPipelineCreateInfo *base,
		uint32_t pipelineGeneration,uint32_t topologyGeneration,ralFormat_t sceneFormat,ralFormat_t depthFormat,
		const vkTemporalIqmPipelineCatalog_t *catalog,const vkTemporalPipelineFactoryOps_t *ops ) {
	vkTemporalShaderBlob_t baseVs,baseFs;vkTemporalShaderRecipeInput_t ri;vkTemporalShaderRecipe_t recipe;
	vkTemporalSpirvOverrides_t override;VkGraphicsPipelineCreateInfo exact;VkPipelineColorBlendStateCreateInfo cb;
	VkPipelineColorBlendAttachmentState blends[3];ralPipeline_t *candidate;uint64_t fingerprint;
	if(!owner||!backend||!layout||!layoutGeneration||!base||!pipelineGeneration||!topologyGeneration
			||sceneFormat==RAL_FORMAT_UNDEFINED||depthFormat==RAL_FORMAT_UNDEFINED||!catalog||!catalog->generation
			||!ops||!ops->create||!ops->destroy||!ops->drain||!ops->lookup
			||!BlobValid(catalog->ordinaryVertex)||!BlobValid(catalog->ordinaryFragment)||!BlobValid(catalog->invalidateFragment)
			||!BaseFingerprint(base,&fingerprint)||!base->pColorBlendState||base->pColorBlendState->attachmentCount!=1u
			||base->pColorBlendState->pAttachments[0].blendEnable||!ops->lookup(base->pStages[0].module,&baseVs)
			||!ops->lookup(base->pStages[1].module,&baseFs)||!BlobEqual(baseVs,catalog->ordinaryVertex)
			||!BlobEqual(baseFs,catalog->ordinaryFragment))return qfalse;
	if(owner->allocationGeneration==UINT32_MAX)return qfalse;
	if(owner->pendingDrain){if(!ops->drain(owner->backend))return qfalse;owner->pendingDrain=qfalse;}
	if(owner->ready&&(owner->layout!=layout||owner->backend!=backend))return qfalse;
	if(owner->ready&&owner->pipelineGeneration==pipelineGeneration&&owner->topologyGeneration==topologyGeneration
			&&owner->layoutGeneration==layoutGeneration&&owner->catalogGeneration==catalog->generation
			&&owner->baseFingerprint==fingerprint&&owner->sceneFormat==sceneFormat&&owner->depthFormat==depthFormat
			&&BlobEqual(owner->catalog.ordinaryVertex,catalog->ordinaryVertex)
			&&BlobEqual(owner->catalog.ordinaryFragment,catalog->ordinaryFragment)
			&&BlobEqual(owner->catalog.invalidateFragment,catalog->invalidateFragment))return qtrue;
	memset(&ri,0,sizeof(ri));ri.kind=VK_TEMPORAL_SHADER_IQM_INVALIDATE;ri.sceneFormat=sceneFormat;
	ri.sceneBlend.writeMask=RAL_COLOR_WRITE_ALL;ri.sceneBlend.writeMaskExplicit=qtrue;
	ri.iqmSetLayouts[0]=(const void*)1;ri.iqmSetLayouts[1]=(const void*)2;
	ri.iqmVertex=catalog->ordinaryVertex;ri.iqmInvalidateFragment=catalog->invalidateFragment;
	if(!VK_TemporalShaderRecipeBuild(&ri,&recipe))return qfalse;
	override.vertex=recipe.vertex;override.fragment=recipe.fragment;exact=*base;cb=*base->pColorBlendState;
	memset(blends,0,sizeof(blends));blends[0]=base->pColorBlendState->pAttachments[0];
	blends[1].colorWriteMask=recipe.colorBlends[1].writeMask;blends[2].colorWriteMask=recipe.colorBlends[2].writeMask;
	cb.attachmentCount=3u;cb.pAttachments=blends;exact.pColorBlendState=&cb;
	candidate=ops->create(&exact,layout,recipe.colorFormats,3u,depthFormat,&override,"wired-temporal-iqm-invalidate");
	if(!candidate||candidate==owner->pipeline){if(candidate&&candidate!=owner->pipeline)ops->destroy(candidate);return qfalse;}
	if(owner->ready){ops->destroy(owner->pipeline);owner->pendingDrain=qtrue;}
	owner->pipeline=candidate;owner->backend=backend;owner->layout=layout;owner->pipelineGeneration=pipelineGeneration;
	owner->topologyGeneration=topologyGeneration;owner->layoutGeneration=layoutGeneration;
	owner->catalogGeneration=catalog->generation;owner->allocationGeneration++;
	owner->baseFingerprint=fingerprint;owner->sceneFormat=sceneFormat;owner->depthFormat=depthFormat;owner->catalog=*catalog;owner->ready=qtrue;
	return qtrue;
}

qboolean VK_TemporalIqmPipelineFactoryRelease( vkTemporalIqmPipelineFactoryOwner_t *owner,
		ralBackend_t *backend,const vkTemporalPipelineFactoryOps_t *ops ) {
	if(!owner||!ops||!ops->destroy||!ops->drain)return qfalse;
	if(owner->pendingDrain){if(!ops->drain(owner->backend))return qfalse;owner->pendingDrain=qfalse;}
	if(owner->ready){if(backend!=owner->backend)return qfalse;ops->destroy(owner->pipeline);owner->ready=qfalse;owner->pendingDrain=qtrue;}
	if(owner->pendingDrain){if(!ops->drain(owner->backend))return qfalse;owner->pendingDrain=qfalse;}
	memset(owner,0,sizeof(*owner));return qtrue;
}
