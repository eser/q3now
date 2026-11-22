// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_bootstrap_aa.h"

#include <math.h>
#include <string.h>

#include "../include/vulkan/vulkan.h"

static qboolean KeyValid( const vkTemporalBootstrapAaKey_t *key ) {
	const void *roles[14];
	uint32_t i, j, n = 0;
	if ( !key || !key->backend || !key->width || !key->height
			|| !key->historyAllocationGeneration
			|| !key->sceneColorAttachmentGeneration
			|| !key->resolvedTargetAllocationGeneration
			|| !key->currentColor || !key->currentDepth
			|| !key->resolvedTarget || !key->resolvedTargetView
			|| !key->computeSpirv || !key->computeSpirvSize
			|| key->computeSpirvSize % sizeof( uint32_t ) ) return qfalse;
	roles[n++] = key->backend; roles[n++] = key->currentColor;
	roles[n++] = key->currentDepth; roles[n++] = key->resolvedTarget;
	roles[n++] = key->resolvedTargetView; roles[n++] = key->computeSpirv;
	for ( i = 0; i < 2; ++i ) {
		roles[n++] = key->historyColor[i]; roles[n++] = key->historyColorView[i];
		roles[n++] = key->historyDepth[i]; roles[n++] = key->historyDepthView[i];
	}
	// Eight of the ten roles per pair plus the six shared ones must be distinct non-null handles.
	for ( i = 0; i < n; ++i ) {
		if ( !roles[i] ) return qfalse;
		for ( j = i + 1; j < n; ++j ) if ( roles[i] == roles[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean KeyEqual( const vkTemporalBootstrapAaKey_t *a,
		const vkTemporalBootstrapAaKey_t *b ) {
	uint32_t i;
	if ( !a || !b || a->backend != b->backend || a->width != b->width
			|| a->height != b->height
			|| a->historyAllocationGeneration != b->historyAllocationGeneration
			|| a->sceneColorAttachmentGeneration != b->sceneColorAttachmentGeneration
			|| a->resolvedTargetAllocationGeneration != b->resolvedTargetAllocationGeneration
			|| a->currentColor != b->currentColor || a->currentDepth != b->currentDepth
			|| a->resolvedTarget != b->resolvedTarget
			|| a->resolvedTargetView != b->resolvedTargetView
			|| a->computeSpirv != b->computeSpirv
			|| a->computeSpirvSize != b->computeSpirvSize ) return qfalse;
	for ( i = 0; i < 2; ++i ) if ( a->historyColor[i] != b->historyColor[i]
			|| a->historyColorView[i] != b->historyColorView[i]
			|| a->historyDepth[i] != b->historyDepth[i]
			|| a->historyDepthView[i] != b->historyDepthView[i] ) return qfalse;
	return qtrue;
}

static void DestroyOwned( vkTemporalBootstrapAaOwner_t *owner ) {
	uint32_t i;
	if ( owner->pipeline ) Ral_DestroyPipeline( owner->pipeline );
	for ( i = 0; i < 2; ++i )
		if ( owner->groups[i] ) Ral_DestroyBindGroup( owner->groups[i] );
	if ( owner->layout ) Ral_DestroyBindGroupLayout( owner->layout );
	if ( owner->sampler ) Ral_DestroySampler( owner->sampler );
	if ( owner->currentDepthView ) Ral_DestroyTextureView( owner->currentDepthView );
	if ( owner->currentColorView ) Ral_DestroyTextureView( owner->currentColorView );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

void VK_TemporalBootstrapAaInit( vkTemporalBootstrapAaOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_TemporalBootstrapAaHasLive( const vkTemporalBootstrapAaOwner_t *owner ) {
	return owner && ( owner->currentColorView || owner->currentDepthView
		|| owner->sampler || owner->layout || owner->groups[0]
		|| owner->groups[1] || owner->pipeline ) ? qtrue : qfalse;
}

qboolean VK_TemporalBootstrapAaMatches( const vkTemporalBootstrapAaOwner_t *owner,
		const vkTemporalBootstrapAaKey_t *key ) {
	return owner && owner->initialized && owner->ready && KeyValid( key )
		&& KeyEqual( &owner->key, key ) && owner->currentColorView
		&& owner->currentDepthView && owner->sampler && owner->layout
		&& owner->groups[0] && owner->groups[1] && owner->pipeline
		? qtrue : qfalse;
}

qboolean VK_TemporalBootstrapAaNeedsIdle( const vkTemporalBootstrapAaOwner_t *owner,
		const vkTemporalBootstrapAaKey_t *key ) {
	return owner && owner->initialized && KeyValid( key )
		&& VK_TemporalBootstrapAaHasLive( owner )
		&& !VK_TemporalBootstrapAaMatches( owner, key ) ? qtrue : qfalse;
}

qboolean VK_TemporalBootstrapAaEnsureAfterFence( vkTemporalBootstrapAaOwner_t *owner,
		const vkTemporalBootstrapAaKey_t *key, qboolean idleProven ) {
	vkTemporalBootstrapAaOwner_t c;
	ralTextureViewCreateInfo_t vci;
	ralSamplerCreateInfo_t sci;
	ralBindEntry_t entries[6];
	ralBindGroupLayoutCreateInfo_t lci;
	ralComputePipelineCreateInfo_t pci;
	const ralBindGroupLayout_t *layouts[1];
	uint32_t i;
	if ( !owner || !owner->initialized || !KeyValid( key ) ) return qfalse;
	if ( VK_TemporalBootstrapAaMatches( owner, key ) ) return qtrue;
	if ( VK_TemporalBootstrapAaHasLive( owner ) && !idleProven ) return qfalse;
	DestroyOwned( owner );
	memset( &c, 0, sizeof( c ) );
	c.initialized = qtrue;
	c.key = *key;
	memset( &vci, 0, sizeof( vci ) );
	vci.viewType = RAL_TEXTURE_2D;
	vci.texture = key->currentColor;
	c.currentColorView = Ral_CreateTextureView( key->backend, &vci );
	if ( !c.currentColorView ) goto fail;
	vci.texture = key->currentDepth;
	c.currentDepthView = Ral_CreateTextureView( key->backend, &vci );
	if ( !c.currentDepthView ) goto fail;
	memset( &sci, 0, sizeof( sci ) );
	sci.minFilter = sci.magFilter = RAL_FILTER_NEAREST;
	sci.mipmapMode = RAL_MIPMAP_NEAREST;
	sci.addressU = sci.addressV = sci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
	sci.maxAnisotropy = 1.0f;
	sci.debugName = "wired-temporal-bootstrap-aa-sampler";
	c.sampler = Ral_CreateSampler( key->backend, &sci );
	if ( !c.sampler ) goto fail;
	memset( entries, 0, sizeof( entries ) );
	entries[0] = (ralBindEntry_t){ 0, RAL_BIND_SAMPLED_TEXTURE, 1, RAL_STAGE_COMPUTE };
	entries[1] = (ralBindEntry_t){ 1, RAL_BIND_SAMPLED_TEXTURE, 1, RAL_STAGE_COMPUTE };
	entries[2] = (ralBindEntry_t){ 2, RAL_BIND_SAMPLER, 1, RAL_STAGE_COMPUTE };
	for ( i = 3; i < 6; ++i )
		entries[i] = (ralBindEntry_t){ i, RAL_BIND_STORAGE_TEXTURE, 1, RAL_STAGE_COMPUTE };
	memset( &lci, 0, sizeof( lci ) );
	lci.entries = entries; lci.numEntries = 6;
	lci.debugName = "wired-temporal-bootstrap-aa-bgl";
	c.layout = Ral_CreateBindGroupLayout( key->backend, &lci );
	if ( !c.layout ) goto fail;
	for ( i = 0; i < 2; ++i ) {
		ralBindingValue_t v[6];
		ralBindGroupCreateInfo_t g;
		memset( v, 0, sizeof( v ) );
		v[0] = (ralBindingValue_t){ .binding = 0, .type = RAL_BIND_SAMPLED_TEXTURE, .textureView = c.currentColorView };
		v[1] = (ralBindingValue_t){ .binding = 1, .type = RAL_BIND_SAMPLED_TEXTURE, .textureView = c.currentDepthView };
		v[2] = (ralBindingValue_t){ .binding = 2, .type = RAL_BIND_SAMPLER, .sampler = c.sampler };
		v[3] = (ralBindingValue_t){ .binding = 3, .type = RAL_BIND_STORAGE_TEXTURE, .textureView = key->resolvedTargetView };
		v[4] = (ralBindingValue_t){ .binding = 4, .type = RAL_BIND_STORAGE_TEXTURE, .textureView = key->historyColorView[i] };
		v[5] = (ralBindingValue_t){ .binding = 5, .type = RAL_BIND_STORAGE_TEXTURE, .textureView = key->historyDepthView[i] };
		memset( &g, 0, sizeof( g ) );
		g.layout = c.layout; g.values = v; g.numValues = 6;
		g.debugName = i ? "wired-temporal-bootstrap-aa-bg-1" : "wired-temporal-bootstrap-aa-bg-0";
		c.groups[i] = Ral_CreateBindGroup( key->backend, &g );
		if ( !c.groups[i] ) goto fail;
	}
	layouts[0] = c.layout;
	memset( &pci, 0, sizeof( pci ) );
	pci.computeSpirv = key->computeSpirv; pci.computeSpirvSize = key->computeSpirvSize;
	pci.bindGroupLayouts = layouts; pci.numBindGroupLayouts = 1;
	pci.pushConstantSize = sizeof( vkTemporalHistoryStorePush_t );
	pci.debugName = "wired-temporal-bootstrap-aa-cs";
	c.pipeline = Ral_CreateComputePipeline( key->backend, &pci );
	if ( !c.pipeline ) goto fail;
	c.ready = qtrue;
	*owner = c;
	return qtrue;
fail:
	DestroyOwned( &c );
	return qfalse;
}

qboolean VK_TemporalBootstrapAaRecord( const vkTemporalBootstrapAaOwner_t *owner,
		const vkTemporalBootstrapAaKey_t *key, ralCommandBuffer_t *cb,
		uint32_t writeIndex, const vkTemporalHistoryStorePush_t *push ) {
	if ( !VK_TemporalBootstrapAaMatches( owner, key ) || !cb || writeIndex > 1
			|| !push || push->extent[0] != owner->key.width
			|| push->extent[1] != owner->key.height
			|| !isfinite( push->zNear ) || !isfinite( push->zFar )
			|| push->zNear <= 0.0f || push->zFar <= push->zNear ) return qfalse;
	Ral_CmdTransitionTexture( cb, owner->key.currentColor,
		RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( cb, owner->key.resolvedTarget,
		RAL_PIPELINE_STAGE_ALL_COMMANDS_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_GENERAL );
	Ral_CmdTransitionTexture( cb, owner->key.historyColor[writeIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_GENERAL );
	Ral_CmdTransitionTexture( cb, owner->key.historyDepth[writeIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_GENERAL );
	Ral_CmdBindPipeline( cb, owner->pipeline );
	Ral_CmdBindBindGroup( cb, 0, owner->groups[writeIndex] );
	Ral_CmdPushConstants( cb, RAL_STAGE_COMPUTE, 0, sizeof( *push ), push );
	Ral_CmdDispatch( cb, ( push->extent[0] + 7u ) / 8u, ( push->extent[1] + 7u ) / 8u, 1u );
	Ral_CmdTransitionTexture( cb, owner->key.resolvedTarget,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Ral_CmdTransitionTexture( cb, owner->key.currentColor,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	return qtrue;
}

qboolean VK_TemporalBootstrapAaReleaseAfterIdle( vkTemporalBootstrapAaOwner_t *owner,
		qboolean idleProven ) {
	if ( !owner || !owner->initialized ) return qfalse;
	if ( VK_TemporalBootstrapAaHasLive( owner ) ) {
		if ( !idleProven ) return qfalse;
		DestroyOwned( owner );
	}
	return qtrue;
}
