// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_iqm_exact3_factory.h"

#include <limits.h>
#include <string.h>

static qboolean BlobValid( vkTemporalShaderBlob_t blob ) {
	return blob.bytes && blob.size && !( blob.size & 3u ) ? qtrue : qfalse;
}

static qboolean BlobExact( vkTemporalShaderBlob_t a,
		vkTemporalShaderBlob_t b ) {
	return a.bytes == b.bytes && a.size == b.size ? qtrue : qfalse;
}

static qboolean BindlessValid(
		const vkTemporalIqmExact3BindlessParent_t *parent ) {
	return parent && parent->ready == qtrue && parent->backend && parent->layout
		&& parent->setIdentity
		&& parent->setGeneration
		&& parent->setGeneration != UINT64_MAX
		&& parent->backend != (ralBackend_t *)parent->layout
		&& parent->setIdentity != (const void *)parent->backend
		&& parent->setIdentity != (const void *)parent->layout
		? qtrue : qfalse;
}

static qboolean BindlessExact(
		const vkTemporalIqmExact3BindlessParent_t *a,
		const vkTemporalIqmExact3BindlessParent_t *b ) {
	return BindlessValid( a ) && BindlessValid( b )
		&& a->backend == b->backend && a->layout == b->layout
		&& a->setIdentity == b->setIdentity
		&& a->setGeneration == b->setGeneration ? qtrue : qfalse;
}

static qboolean CatalogValid(
		const vkTemporalIqmExact3ShaderCatalog_t *catalog ) {
	return catalog && catalog->generation
		&& catalog->generation != UINT32_MAX
		&& BlobValid( catalog->vertex )
		&& BlobValid( catalog->writeFragment )
		&& BlobValid( catalog->invalidateFragment )
		&& catalog->vertex.bytes != catalog->writeFragment.bytes
		&& catalog->vertex.bytes != catalog->invalidateFragment.bytes
		&& catalog->writeFragment.bytes
			!= catalog->invalidateFragment.bytes ? qtrue : qfalse;
}

static qboolean CatalogExact(
		const vkTemporalIqmExact3ShaderCatalog_t *a,
		const vkTemporalIqmExact3ShaderCatalog_t *b ) {
	return CatalogValid( a ) && CatalogValid( b )
		&& a->generation == b->generation
		&& BlobExact( a->vertex, b->vertex )
		&& BlobExact( a->writeFragment, b->writeFragment )
		&& BlobExact( a->invalidateFragment,
			b->invalidateFragment ) ? qtrue : qfalse;
}

static qboolean InputValid(
		const vkTemporalIqmExact3FactoryInput_t *input ) {
	return input && input->backend && input->device
		&& BindlessValid( &input->bindless )
		&& input->bindless.backend == input->backend
		&& input->pipelineGeneration
		&& input->pipelineGeneration != UINT32_MAX
		&& input->topologyGeneration
		&& input->topologyGeneration != UINT32_MAX
		&& input->sceneFormat == RAL_FORMAT_R16G16B16A16_SFLOAT
		&& input->depthFormat != RAL_FORMAT_UNDEFINED
		&& ( input->reversedDepth == qfalse
			|| input->reversedDepth == qtrue ) ? qtrue : qfalse;
}

static qboolean CandidateAllowed(
		const vkTemporalIqmExact3FactoryOps_t *ops,
		vkTemporalIqmExact3CandidateRole_t role, const void *identity ) {
	return identity && ops->candidateAllowed
		&& ops->candidateAllowed( role, identity,
			ops->candidateContext ) ? qtrue : qfalse;
}

static qboolean BorrowedRalAlias(
		const vkTemporalIqmExact3FactoryOwner_t *owner,
		const void *identity ) {
	return !identity || identity == owner->backend
		|| identity == owner->payloadOwner
		|| identity == owner->payloadLayout
		|| identity == owner->bindless.layout
		|| identity == owner->bindless.setIdentity ? qtrue : qfalse;
}

static void PreserveGenerationAndClear(
		vkTemporalIqmExact3FactoryOwner_t *owner ) {
	uint32_t generation;
	if ( !owner ) return;
	generation = owner->allocationGeneration;
	memset( owner, 0, sizeof( *owner ) );
	owner->allocationGeneration = generation;
	owner->initialized = qtrue;
}

static qboolean FinishParentsAfterDrain(
		vkTemporalIqmExact3FactoryOwner_t *owner,
		const vkTemporalIqmExact3FactoryOps_t *ops ) {
	if ( !owner || !ops || !owner->pendingDrain || owner->ready
			|| !owner->backend || !owner->payloadLease ) return qfalse;
	if ( !owner->payloadOwner || owner->payloadOwner->ready != qtrue
			|| owner->payloadOwner->layout != owner->payloadLayout
			|| owner->payloadOwner->ownerAllocationGeneration
				!= owner->payloadLayoutGeneration
			|| !owner->payloadOwner->layoutLeaseCount ) return qfalse;
	if ( !owner->parentsDrained ) {
		if ( !ops->drain( owner->backend ) ) return qfalse;
		owner->parentsDrained = qtrue;
	}
	if ( owner->adoptedLayout ) {
		ops->destroyLayout( owner->adoptedLayout );
		owner->adoptedLayout = NULL;
	}
	owner->rawLayout = VK_NULL_HANDLE;
	if ( !VK_TemporalIqmPayloadReleaseLayoutLease( owner->payloadOwner,
			owner->payloadLayout, owner->payloadLayoutGeneration ) ) return qfalse;
	owner->payloadLease = qfalse;
	owner->pendingDrain = qfalse;
	owner->parentsDrained = qfalse;
	PreserveGenerationAndClear( owner );
	return qtrue;
}

static qboolean FreshOwner(
		const vkTemporalIqmExact3FactoryOwner_t *owner ) {
	vkTemporalIqmExact3FactoryOwner_t fresh;
	if ( !owner || owner->initialized != qtrue ) return qfalse;
	memset( &fresh, 0, sizeof( fresh ) );
	fresh.initialized = qtrue;
	fresh.allocationGeneration = owner->allocationGeneration;
	return memcmp( owner, &fresh, sizeof( fresh ) ) == 0 ? qtrue : qfalse;
}

static qboolean OwnerValid(
		const vkTemporalIqmExact3FactoryOwner_t *owner ) {
	const void *roles[8];
	if ( !owner || owner->initialized != qtrue || owner->ready != qtrue
			|| owner->pendingDrain || owner->parentsDrained
			|| !owner->payloadLease
			|| !owner->payloadOwner || !owner->backend || !owner->device
			|| !owner->payloadLayout || !owner->payloadLayoutGeneration
			|| owner->payloadLayoutGeneration == UINT32_MAX
			|| !owner->payloadRawLayout || !owner->bindlessRawLayout
			|| !BindlessValid( &owner->bindless )
			|| !owner->rawLayout || !owner->adoptedLayout
			|| !owner->writePipeline || !owner->invalidatePipeline
			|| owner->writePipeline == owner->invalidatePipeline
			|| !CatalogValid( &owner->shaders )
			|| !owner->pipelineGeneration || !owner->topologyGeneration
			|| !owner->allocationGeneration
			|| owner->allocationGeneration == UINT32_MAX
			|| owner->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT
			|| owner->depthFormat == RAL_FORMAT_UNDEFINED
			|| owner->payloadOwner->ready != qtrue
			|| owner->payloadOwner->key.backend != owner->backend
			|| owner->bindless.backend != owner->backend
			|| owner->payloadOwner->layout != owner->payloadLayout
			|| owner->payloadOwner->ownerAllocationGeneration
				!= owner->payloadLayoutGeneration
			|| !owner->payloadOwner->layoutLeaseCount ) return qfalse;
	if ( owner->payloadRawLayout == owner->bindlessRawLayout
			|| BorrowedRalAlias( owner, owner->adoptedLayout )
			|| BorrowedRalAlias( owner, owner->writePipeline )
			|| BorrowedRalAlias( owner, owner->invalidatePipeline )
			|| owner->adoptedLayout == (ralPipelineLayout_t *)owner->writePipeline
			|| owner->adoptedLayout == (ralPipelineLayout_t *)owner->invalidatePipeline )
		return qfalse;
	roles[0]=owner->backend;roles[1]=owner->payloadOwner;
	roles[2]=owner->payloadLayout;roles[3]=owner->bindless.layout;
	roles[4]=owner->bindless.setIdentity;roles[5]=owner->adoptedLayout;
	roles[6]=owner->writePipeline;roles[7]=owner->invalidatePipeline;
	for ( uint32_t i=0;i<8u;++i ) for ( uint32_t j=0;j<i;++j )
		if ( roles[i] == roles[j] ) return qfalse;
	return qtrue;
}

static qboolean ExactLiveKey(
		const vkTemporalIqmExact3FactoryOwner_t *owner,
		vkTemporalIqmPayloadOwner_t *payloadOwner,
		const vkTemporalIqmExact3FactoryInput_t *input,
		const vkTemporalIqmExact3ShaderCatalog_t *shaders ) {
	return OwnerValid( owner ) && owner->payloadOwner == payloadOwner
		&& owner->backend == input->backend && owner->device == input->device
		&& BindlessExact( &owner->bindless, &input->bindless )
		&& owner->pipelineGeneration == input->pipelineGeneration
		&& owner->topologyGeneration == input->topologyGeneration
		&& owner->sceneFormat == input->sceneFormat
		&& owner->depthFormat == input->depthFormat
		&& owner->reversedDepth == input->reversedDepth
		&& CatalogExact( &owner->shaders, shaders ) ? qtrue : qfalse;
}

static void FillPipelineInfo( ralGraphicsPipelineCreateInfo_t *ci,
		const vkTemporalIqmExact3FactoryInput_t *input,
		const vkTemporalIqmExact3ShaderCatalog_t *shaders,
		const ralBindGroupLayout_t *const layouts[2],
		ralPipelineLayout_t *adopted, qboolean invalidate,
		const ralVertexBinding_t *binding,
		const ralVertexAttribute_t attributes[6],
		const ralColorBlendAttachment_t blends[3] ) {
	memset( ci, 0, sizeof( *ci ) );
	ci->vertexSpirv = (const uint32_t *)shaders->vertex.bytes;
	ci->vertexSpirvSize = shaders->vertex.size;
	ci->fragmentSpirv = (const uint32_t *)( invalidate
		? shaders->invalidateFragment.bytes : shaders->writeFragment.bytes );
	ci->fragmentSpirvSize = invalidate
		? shaders->invalidateFragment.size : shaders->writeFragment.size;
	ci->vertexBindings = binding; ci->numVertexBindings = 1u;
	ci->vertexAttributes = attributes; ci->numVertexAttributes = 6u;
	ci->topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	ci->raster.polygonMode = RAL_POLYGON_FILL;
	ci->raster.cullMode = RAL_CULL_FRONT;
	ci->raster.frontFace = RAL_FRONT_FACE_CW;
	ci->raster.lineWidth = 1.0f;
	ci->depthStencil.depthTestEnable = qtrue;
	ci->depthStencil.depthWriteEnable = qtrue;
	ci->depthStencil.depthCompareOp = input->reversedDepth
		? RAL_COMPARE_GREATER_EQUAL : RAL_COMPARE_LESS_EQUAL;
	ci->colorBlends = blends; ci->numColorBlends = 3u;
	ci->colorFormats[0] = input->sceneFormat;
	ci->colorFormats[1] = RAL_FORMAT_R16G16_SFLOAT;
	ci->colorFormats[2] = RAL_FORMAT_R8_UNORM;
	ci->numColorFormats = 3u;
	ci->depthFormat = input->depthFormat; ci->sampleCount = 1u;
	ci->bindGroupLayouts = layouts; ci->numBindGroupLayouts = 2u;
	ci->pushConstantSize = VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE;
	ci->pushConstantStages = RAL_STAGE_FRAGMENT;
	ci->externalLayout = adopted;
	ci->debugName = invalidate ? "wired-temporal-iqm-exact3-invalidate"
		: "wired-temporal-iqm-exact3-write";
}

void VK_TemporalIqmExact3FactoryInit(
		vkTemporalIqmExact3FactoryOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalIqmExact3FactoryEnsure(
		vkTemporalIqmExact3FactoryOwner_t *owner,
		vkTemporalIqmPayloadOwner_t *payloadOwner,
		const vkTemporalIqmExact3FactoryInput_t *input,
		const vkTemporalIqmExact3ShaderCatalog_t *shaders,
		const vkTemporalIqmExact3FactoryOps_t *ops ) {
	const ralBindGroupLayout_t *ralSets[2];
	ralPipelineLayoutCreateInfo_t layoutInfo;
	ralVertexBinding_t binding;
	ralVertexAttribute_t attributes[6];
	ralColorBlendAttachment_t blends[3];
	ralGraphicsPipelineCreateInfo_t ci;
	void *payloadRaw, *bindlessRaw;
	qboolean rawAllowed = qfalse, adoptedOwned = qfalse;
	qboolean writeOwned = qfalse, invalidateOwned = qfalse;
	uint32_t nextGeneration;

	if ( !owner || owner->initialized != qtrue || !payloadOwner
			|| !InputValid( input ) || !CatalogValid( shaders ) || !ops
			|| !ops->getBindGroupLayoutHandle || !ops->createLayout || !ops->getLayoutHandle
			|| !ops->destroyLayout || !ops->createPipeline
			|| !ops->destroyPipeline || !ops->drain
			|| !ops->candidateAllowed ) return qfalse;
	if ( owner->pendingDrain ) {
		(void)FinishParentsAfterDrain( owner, ops );
		return qfalse;
	}
	if ( owner->ready ) return ExactLiveKey( owner, payloadOwner,
		input, shaders );
	if ( !FreshOwner( owner )
			|| owner->allocationGeneration >= UINT32_MAX - 1u ) return qfalse;
	if ( payloadOwner->key.backend != input->backend
			|| input->bindless.backend != input->backend ) return qfalse;

	owner->payloadOwner = payloadOwner; owner->backend = input->backend;
	owner->device = input->device; owner->bindless = input->bindless;
	if ( !VK_TemporalIqmPayloadAcquireLayoutLease( payloadOwner,
			&owner->payloadLayout, &owner->payloadLayoutGeneration ) ) goto fail;
	owner->payloadLease = qtrue;
	{
		const void *borrowed[4] = { owner->backend, owner->payloadOwner,
			owner->payloadLayout, owner->bindless.layout };
		for ( uint32_t i = 0; i < 4u; ++i ) for ( uint32_t j = 0; j < i; ++j )
			if ( borrowed[i] == borrowed[j] ) goto fail;
	}
	payloadRaw = ops->getBindGroupLayoutHandle( owner->payloadLayout );
	bindlessRaw = ops->getBindGroupLayoutHandle( input->bindless.layout );
	if ( !payloadRaw || !bindlessRaw || payloadRaw == bindlessRaw ) goto fail;
	owner->payloadRawLayout = payloadRaw;
	owner->bindlessRawLayout = bindlessRaw;

	ralSets[0] = owner->payloadLayout;
	ralSets[1] = input->bindless.layout;
	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.bindGroupLayouts = ralSets;
	layoutInfo.numBindGroupLayouts = 2u;
	layoutInfo.pushConstantSize = VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE;
	layoutInfo.pushConstantStages = RAL_STAGE_FRAGMENT;
	layoutInfo.debugName = "wired-temporal-iqm-exact3-layout";
	owner->adoptedLayout = ops->createLayout( input->backend, &layoutInfo );
	if ( !owner->adoptedLayout ) goto fail;
	adoptedOwned = CandidateAllowed( ops,
		VK_TEMPORAL_IQM_EXACT3_CANDIDATE_ADOPTED_LAYOUT,
		owner->adoptedLayout );
	if ( !adoptedOwned || BorrowedRalAlias( owner,
			owner->adoptedLayout ) ) {
		adoptedOwned = qfalse; goto fail;
	}
	owner->rawLayout = (VkPipelineLayout)ops->getLayoutHandle(
		owner->adoptedLayout );
	if ( !owner->rawLayout ) goto fail;
	rawAllowed = CandidateAllowed( ops,
		VK_TEMPORAL_IQM_EXACT3_CANDIDATE_RAW_LAYOUT,
		(const void *)owner->rawLayout );
	if ( !rawAllowed ) goto fail;

	binding.binding = 0u;
	binding.stride = VK_TEMPORAL_IQM_EXACT3_VERTEX_STRIDE;
	binding.inputRate = RAL_VERTEX_INPUT_PER_VERTEX;
	attributes[0] = (ralVertexAttribute_t){0u,0u,RAL_FORMAT_R32G32B32_SFLOAT,0u};
	attributes[1] = (ralVertexAttribute_t){1u,0u,RAL_FORMAT_R32G32B32_SFLOAT,12u};
	attributes[2] = (ralVertexAttribute_t){2u,0u,RAL_FORMAT_R32G32_SFLOAT,24u};
	attributes[3] = (ralVertexAttribute_t){3u,0u,RAL_FORMAT_R32G32B32A32_SFLOAT,32u};
	attributes[4] = (ralVertexAttribute_t){4u,0u,RAL_FORMAT_R32G32B32A32_SFLOAT,48u};
	attributes[5] = (ralVertexAttribute_t){5u,0u,RAL_FORMAT_R8G8B8A8_UINT,64u};
	memset( blends, 0, sizeof( blends ) );
	blends[0].writeMask = RAL_COLOR_WRITE_ALL;
	blends[1].writeMask = RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G;
	blends[2].writeMask = RAL_COLOR_WRITE_R;
	for ( uint32_t i = 0; i < 3u; ++i ) blends[i].writeMaskExplicit = qtrue;
	ralSets[0] = owner->payloadLayout; ralSets[1] = input->bindless.layout;
	FillPipelineInfo( &ci, input, shaders, ralSets, owner->adoptedLayout,
		qfalse, &binding, attributes, blends );
	owner->writePipeline = ops->createPipeline( input->backend, &ci );
	writeOwned = CandidateAllowed( ops,
		VK_TEMPORAL_IQM_EXACT3_CANDIDATE_WRITE_PIPELINE,
		owner->writePipeline );
	if ( !writeOwned || BorrowedRalAlias( owner, owner->writePipeline )
			|| owner->writePipeline == (ralPipeline_t *)owner->adoptedLayout ) {
		writeOwned = qfalse; goto fail;
	}
	FillPipelineInfo( &ci, input, shaders, ralSets, owner->adoptedLayout,
		qtrue, &binding, attributes, blends );
	owner->invalidatePipeline = ops->createPipeline( input->backend, &ci );
	invalidateOwned = CandidateAllowed( ops,
		VK_TEMPORAL_IQM_EXACT3_CANDIDATE_INVALIDATE_PIPELINE,
		owner->invalidatePipeline );
	if ( !invalidateOwned || BorrowedRalAlias( owner,
			owner->invalidatePipeline )
			|| owner->invalidatePipeline == (ralPipeline_t *)owner->adoptedLayout
			|| owner->invalidatePipeline == owner->writePipeline ) {
		invalidateOwned = qfalse; goto fail;
	}

	nextGeneration = owner->allocationGeneration + 1u;
	owner->shaders = *shaders;
	owner->pipelineGeneration = input->pipelineGeneration;
	owner->topologyGeneration = input->topologyGeneration;
	owner->allocationGeneration = nextGeneration;
	owner->sceneFormat = input->sceneFormat;
	owner->depthFormat = input->depthFormat;
	owner->reversedDepth = input->reversedDepth;
	owner->ready = qtrue;
	if ( !OwnerValid( owner ) ) {
		owner->ready = qfalse;
		owner->allocationGeneration = nextGeneration - 1u;
		goto fail;
	}
	return qtrue;

fail:
	if ( invalidateOwned && owner->invalidatePipeline )
		ops->destroyPipeline( owner->invalidatePipeline );
	owner->invalidatePipeline = NULL;
	if ( writeOwned && owner->writePipeline )
		ops->destroyPipeline( owner->writePipeline );
	owner->writePipeline = NULL;
	if ( owner->payloadLease ) {
		if ( !adoptedOwned ) owner->adoptedLayout = NULL;
		if ( !rawAllowed ) owner->rawLayout = VK_NULL_HANDLE;
		owner->pendingDrain = qtrue;
		owner->parentsDrained = ( writeOwned || invalidateOwned )
			? qfalse : qtrue;
		(void)FinishParentsAfterDrain( owner, ops );
		return qfalse;
	}
	PreserveGenerationAndClear( owner );
	return qfalse;
}

static qboolean ReceiptValid(
		const vkTemporalIqmExact3FactoryReceipt_t *receipt ) {
	const void *roles[8];
	if ( !receipt || receipt->ready != qtrue || !receipt->backend
			|| !receipt->device || !receipt->payloadOwner
			|| !receipt->payloadLayout || !receipt->payloadLayoutGeneration
			|| receipt->payloadLayoutGeneration == UINT32_MAX
			|| !receipt->payloadRawLayout || !receipt->bindlessRawLayout
			|| receipt->payloadRawLayout == receipt->bindlessRawLayout
			|| !BindlessValid( &receipt->bindless )
			|| receipt->bindless.backend != receipt->backend
			|| !receipt->rawLayout || !receipt->adoptedLayout
			|| !receipt->writePipeline || !receipt->invalidatePipeline
			|| receipt->writePipeline == receipt->invalidatePipeline
			|| !receipt->shaderGeneration
			|| receipt->shaderGeneration == UINT32_MAX
			|| !receipt->pipelineGeneration
			|| receipt->pipelineGeneration == UINT32_MAX
			|| !receipt->topologyGeneration
			|| receipt->topologyGeneration == UINT32_MAX
			|| !receipt->allocationGeneration
			|| receipt->allocationGeneration == UINT32_MAX
			|| receipt->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT
			|| receipt->depthFormat == RAL_FORMAT_UNDEFINED
			|| ( receipt->reversedDepth != qfalse
				&& receipt->reversedDepth != qtrue ) ) return qfalse;
	roles[0]=receipt->backend;roles[1]=receipt->payloadOwner;
	roles[2]=receipt->payloadLayout;roles[3]=receipt->bindless.layout;
	roles[4]=receipt->bindless.setIdentity;roles[5]=receipt->adoptedLayout;
	roles[6]=receipt->writePipeline;roles[7]=receipt->invalidatePipeline;
	for ( uint32_t i=0;i<8u;++i ) for ( uint32_t j=0;j<i;++j )
		if ( roles[i] == roles[j] ) return qfalse;
	return qtrue;
}

qboolean VK_TemporalIqmExact3FactoryGetReceipt(
		const vkTemporalIqmExact3FactoryOwner_t *owner,
		vkTemporalIqmExact3FactoryReceipt_t *outReceipt ) {
	vkTemporalIqmExact3FactoryReceipt_t receipt;
	if ( !OwnerValid( owner ) || !outReceipt ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.backend = owner->backend;
	receipt.device = owner->device;
	receipt.payloadOwner = owner->payloadOwner;
	receipt.payloadLayout = owner->payloadLayout;
	receipt.payloadRawLayout = owner->payloadRawLayout;
	receipt.bindlessRawLayout = owner->bindlessRawLayout;
	receipt.payloadLayoutGeneration = owner->payloadLayoutGeneration;
	receipt.bindless = owner->bindless;
	receipt.rawLayout = owner->rawLayout;
	receipt.adoptedLayout = owner->adoptedLayout;
	receipt.writePipeline = owner->writePipeline;
	receipt.invalidatePipeline = owner->invalidatePipeline;
	receipt.shaderGeneration = owner->shaders.generation;
	receipt.pipelineGeneration = owner->pipelineGeneration;
	receipt.topologyGeneration = owner->topologyGeneration;
	receipt.allocationGeneration = owner->allocationGeneration;
	receipt.sceneFormat = owner->sceneFormat;
	receipt.depthFormat = owner->depthFormat;
	receipt.reversedDepth = owner->reversedDepth;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalIqmExact3FactoryReceiptExact(
		const vkTemporalIqmExact3FactoryReceipt_t *a,
		const vkTemporalIqmExact3FactoryReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& a->backend == b->backend
		&& a->device == b->device
		&& a->payloadOwner == b->payloadOwner
		&& a->payloadLayout == b->payloadLayout
		&& a->payloadRawLayout == b->payloadRawLayout
		&& a->bindlessRawLayout == b->bindlessRawLayout
		&& a->payloadLayoutGeneration == b->payloadLayoutGeneration
		&& BindlessExact( &a->bindless, &b->bindless )
		&& a->rawLayout == b->rawLayout
		&& a->adoptedLayout == b->adoptedLayout
		&& a->writePipeline == b->writePipeline
		&& a->invalidatePipeline == b->invalidatePipeline
		&& a->shaderGeneration == b->shaderGeneration
		&& a->pipelineGeneration == b->pipelineGeneration
		&& a->topologyGeneration == b->topologyGeneration
		&& a->allocationGeneration == b->allocationGeneration
		&& a->sceneFormat == b->sceneFormat
		&& a->depthFormat == b->depthFormat
		&& a->reversedDepth == b->reversedDepth
		&& a->ready == b->ready ? qtrue : qfalse;
}

qboolean VK_TemporalIqmExact3FactoryRelease(
		vkTemporalIqmExact3FactoryOwner_t *owner,
		const vkTemporalIqmExact3FactoryOps_t *ops ) {
	if ( !owner || owner->initialized != qtrue || !ops
			|| !ops->destroyPipeline || !ops->drain
			|| !ops->destroyLayout ) return qfalse;
	if ( owner->pendingDrain ) return FinishParentsAfterDrain( owner, ops );
	if ( !owner->ready ) return FreshOwner( owner );
	if ( !OwnerValid( owner ) ) return qfalse;
	ops->destroyPipeline( owner->invalidatePipeline );
	ops->destroyPipeline( owner->writePipeline );
	owner->invalidatePipeline = NULL; owner->writePipeline = NULL;
	owner->ready = qfalse; owner->pendingDrain = qtrue;
	return FinishParentsAfterDrain( owner, ops );
}
