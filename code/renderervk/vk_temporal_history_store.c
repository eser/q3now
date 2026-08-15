// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_history_store.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "../renderercommon/vulkan/vulkan.h"

static qboolean PointerInSet( const void *p, const void *const *set, uint32_t n ) {
	uint32_t i;
	if ( !p ) return qfalse;
	for ( i = 0; i < n; ++i ) if ( p == set[i] ) return qtrue;
	return qfalse;
}

static uint32_t AppendKeyBorrowed( const vkTemporalHistoryStoreKey_t *key,
		const void **out, uint32_t n ) {
	uint32_t i;
	if ( key ) {
		out[n++] = key->backend; out[n++] = key->feedbackColor;
		out[n++] = key->feedbackColorView; out[n++] = key->currentDepth;
		out[n++] = key->computeSpirv;
		for ( i = 0; i < 2; ++i ) {
			out[n++] = key->historyColor[i]; out[n++] = key->historyColorView[i];
			out[n++] = key->historyDepth[i]; out[n++] = key->historyDepthView[i];
		}
	}
	return n;
}

static uint32_t Borrowed( const vkTemporalHistoryStoreKey_t *key,
		const vkTemporalHistoryStoreOwner_t *live, const void **out ) {
	uint32_t n = AppendKeyBorrowed( key, out, 0 );
	if ( live ) n = AppendKeyBorrowed( &live->key, out, n );
	if ( live ) {
		out[n++] = live->currentDepthView; out[n++] = live->sampler;
		out[n++] = live->layout; out[n++] = live->groups[0];
		out[n++] = live->groups[1]; out[n++] = live->pipeline;
	}
	return n;
}

static qboolean KeyValid( const vkTemporalHistoryStoreKey_t *key ) {
	const void *roles[13];
	uint32_t i, j, n = 0;
	if ( !key || !key->backend || key->worldIndex < 0 || !key->width
			|| !key->height || !key->topologyEpoch
			|| !key->historyAllocationGeneration
			|| !key->sceneColorAttachmentGeneration
			|| !key->resolvedTargetAllocationGeneration
			|| key->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT
			|| !key->feedbackColor || !key->feedbackColorView
			|| !key->currentDepth || !key->computeSpirv
			|| !key->computeSpirvSize
			|| key->computeSpirvSize % sizeof( uint32_t ) ) return qfalse;
	roles[n++] = key->backend; roles[n++] = key->feedbackColor;
	roles[n++] = key->feedbackColorView; roles[n++] = key->currentDepth;
	roles[n++] = key->computeSpirv;
	for ( i = 0; i < 2; ++i ) {
		roles[n++] = key->historyColor[i]; roles[n++] = key->historyColorView[i];
		roles[n++] = key->historyDepth[i]; roles[n++] = key->historyDepthView[i];
	}
	for ( i = 0; i < n; ++i ) {
		if ( !roles[i] ) return qfalse;
		for ( j = i + 1; j < n; ++j ) if ( roles[i] == roles[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean KeyEqual( const vkTemporalHistoryStoreKey_t *a,
		const vkTemporalHistoryStoreKey_t *b ) {
	uint32_t i;
	if ( !a || !b || a->backend != b->backend || a->worldIndex != b->worldIndex
			|| a->width != b->width || a->height != b->height
			|| a->topologyEpoch != b->topologyEpoch
			|| a->historyAllocationGeneration != b->historyAllocationGeneration
			|| a->sceneColorAttachmentGeneration != b->sceneColorAttachmentGeneration
			|| a->resolvedTargetAllocationGeneration != b->resolvedTargetAllocationGeneration
			|| a->sceneFormat != b->sceneFormat
			|| a->feedbackColor != b->feedbackColor
			|| a->feedbackColorView != b->feedbackColorView
			|| a->currentDepth != b->currentDepth
			|| a->computeSpirv != b->computeSpirv
			|| a->computeSpirvSize != b->computeSpirvSize ) return qfalse;
	for ( i = 0; i < 2; ++i ) if ( a->historyColor[i] != b->historyColor[i]
			|| a->historyColorView[i] != b->historyColorView[i]
			|| a->historyDepth[i] != b->historyDepth[i]
			|| a->historyDepthView[i] != b->historyDepthView[i] ) return qfalse;
	return qtrue;
}

static void DestroyOwned( vkTemporalHistoryStoreOwner_t *owner ) {
	if ( owner->pipeline ) Ral_DestroyPipeline( owner->pipeline );
	if ( owner->groups[1] && owner->groups[1] != owner->groups[0] )
		Ral_DestroyBindGroup( owner->groups[1] );
	if ( owner->groups[0] ) Ral_DestroyBindGroup( owner->groups[0] );
	if ( owner->layout ) Ral_DestroyBindGroupLayout( owner->layout );
	if ( owner->sampler ) Ral_DestroySampler( owner->sampler );
	if ( owner->currentDepthView ) Ral_DestroyTextureView( owner->currentDepthView );
}

static void DestroyOwnedProtected( vkTemporalHistoryStoreOwner_t *owner,
		const vkTemporalHistoryStoreKey_t *a,
		const vkTemporalHistoryStoreKey_t *b ) {
	const void *protectedSet[26], *values[6];
	void **roles[6];
	qboolean suppress[6] = { qfalse, qfalse, qfalse, qfalse, qfalse, qfalse };
	uint32_t n = 0, i, j;
	if ( !owner ) return;
	n = AppendKeyBorrowed( a, protectedSet, n );
	n = AppendKeyBorrowed( b, protectedSet, n );
	roles[0]=(void **)&owner->currentDepthView; roles[1]=(void **)&owner->sampler;
	roles[2]=(void **)&owner->layout; roles[3]=(void **)&owner->groups[0];
	roles[4]=(void **)&owner->groups[1]; roles[5]=(void **)&owner->pipeline;
	for ( i = 0; i < 6; ++i ) {
		values[i] = *roles[i];
		if ( PointerInSet( values[i], protectedSet, n ) ) suppress[i] = qtrue;
	}
	for ( i = 0; i < 6; ++i ) for ( j = i + 1; j < 6; ++j )
		if ( values[i] && values[i] == values[j] )
			suppress[i] = suppress[j] = qtrue;
	for ( i = 0; i < 6; ++i ) if ( suppress[i] ) *roles[i] = NULL;
	DestroyOwned( owner );
}

static qboolean OwnerValidExact( const vkTemporalHistoryStoreOwner_t *owner,
		const vkTemporalHistoryStoreKey_t *key ) {
	const void *roles[19];
	uint32_t i, j, n;
	if ( !owner || !owner->initialized || !owner->ready
			|| !owner->allocationGeneration || !KeyValid( &owner->key )
			|| !KeyValid( key ) || !KeyEqual( &owner->key, key ) ) return qfalse;
	n = AppendKeyBorrowed( &owner->key, roles, 0 );
	roles[n++] = owner->currentDepthView; roles[n++] = owner->sampler;
	roles[n++] = owner->layout; roles[n++] = owner->groups[0];
	roles[n++] = owner->groups[1]; roles[n++] = owner->pipeline;
	for ( i = 0; i < n; ++i ) {
		if ( !roles[i] ) return qfalse;
		for ( j = i + 1; j < n; ++j ) if ( roles[i] == roles[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean CandidateAliases( const vkTemporalHistoryStoreOwner_t *live,
		const vkTemporalHistoryStoreOwner_t *candidate,
		const vkTemporalHistoryStoreKey_t *key ) {
	const void *protectedSet[40], *roles[6];
	uint32_t n, i, j;
	n = Borrowed( key, live, protectedSet );
	roles[0] = candidate->currentDepthView; roles[1] = candidate->sampler;
	roles[2] = candidate->layout; roles[3] = candidate->groups[0];
	roles[4] = candidate->groups[1]; roles[5] = candidate->pipeline;
	for ( i = 0; i < 6; ++i ) if ( roles[i] ) {
		if ( PointerInSet( roles[i], protectedSet, n ) ) return qtrue;
		for ( j = 0; j < i; ++j ) if ( roles[i] == roles[j] ) return qtrue;
	}
	return qfalse;
}

static void DestroyCandidate( vkTemporalHistoryStoreOwner_t *candidate,
		const vkTemporalHistoryStoreOwner_t *live,
		const vkTemporalHistoryStoreKey_t *key ) {
	const void *protectedSet[40], *seen[6];
	void **roles[6];
	uint32_t n, i, j;
	n = Borrowed( key, live, protectedSet );
	roles[0]=(void **)&candidate->currentDepthView; roles[1]=(void **)&candidate->sampler;
	roles[2]=(void **)&candidate->layout; roles[3]=(void **)&candidate->groups[0];
	roles[4]=(void **)&candidate->groups[1]; roles[5]=(void **)&candidate->pipeline;
	for ( i = 0; i < 6; ++i ) {
		seen[i] = *roles[i];
		if ( PointerInSet( seen[i], protectedSet, n ) ) *roles[i] = NULL;
		for ( j = 0; j < i; ++j ) if ( seen[i] && seen[i] == seen[j] )
			*roles[i] = NULL;
	}
	DestroyOwned( candidate );
}

void VK_TemporalHistoryStoreInit( vkTemporalHistoryStoreOwner_t *owner ) {
	if ( owner ) { memset( owner, 0, sizeof( *owner ) ); owner->initialized = qtrue; }
}

qboolean VK_TemporalHistoryStoreHasLive( const vkTemporalHistoryStoreOwner_t *owner ) {
	return owner && owner->initialized && ( owner->currentDepthView || owner->sampler
		|| owner->layout || owner->groups[0] || owner->groups[1] || owner->pipeline )
		? qtrue : qfalse;
}

qboolean VK_TemporalHistoryStoreMatchesExact(
		const vkTemporalHistoryStoreOwner_t *owner,
		const vkTemporalHistoryStoreKey_t *key ) {
	return OwnerValidExact( owner, key );
}

qboolean VK_TemporalHistoryStoreNeedsIdle(
		const vkTemporalHistoryStoreOwner_t *owner,
		const vkTemporalHistoryStoreKey_t *key ) {
	return owner && owner->initialized && KeyValid( key )
		&& VK_TemporalHistoryStoreHasLive( owner )
		&& !VK_TemporalHistoryStoreMatchesExact( owner, key ) ? qtrue : qfalse;
}

qboolean VK_TemporalHistoryStoreEnsureAfterFence(
		vkTemporalHistoryStoreOwner_t *owner,
		const vkTemporalHistoryStoreKey_t *key, qboolean idleProven ) {
	vkTemporalHistoryStoreOwner_t c;
	ralTextureViewCreateInfo_t vci;
	ralSamplerCreateInfo_t sci;
	ralBindEntry_t entries[5];
	ralBindGroupLayoutCreateInfo_t lci;
	ralComputePipelineCreateInfo_t pci;
	const ralBindGroupLayout_t *layouts[1];
	uint32_t i, generation;
	if ( !owner || !owner->initialized || !KeyValid( key ) ) return qfalse;
	if ( VK_TemporalHistoryStoreMatchesExact( owner, key ) ) return qtrue;
	if ( VK_TemporalHistoryStoreHasLive( owner ) && !idleProven ) return qfalse;
	if ( owner->allocationGeneration == UINT32_MAX ) return qfalse;
	generation = owner->allocationGeneration + 1u;
	memset( &c, 0, sizeof( c ) ); c.initialized=qtrue; c.key=*key;
	c.allocationGeneration = generation;
	memset( &vci, 0, sizeof( vci ) ); vci.texture=key->currentDepth;
	vci.viewType=RAL_TEXTURE_2D; vci.format=RAL_FORMAT_UNDEFINED;
	c.currentDepthView=Ral_CreateTextureView(key->backend,&vci);
	if(!c.currentDepthView||CandidateAliases(owner,&c,key))goto fail;
	memset(&sci,0,sizeof(sci)); sci.minFilter=RAL_FILTER_NEAREST;
	sci.magFilter=RAL_FILTER_NEAREST;sci.mipmapMode=RAL_MIPMAP_NEAREST;
	sci.addressU=sci.addressV=sci.addressW=RAL_ADDRESS_CLAMP_TO_EDGE;
	sci.maxAnisotropy=1.0f;sci.debugName="wired-temporal-history-sampler";
	c.sampler=Ral_CreateSampler(key->backend,&sci);
	if(!c.sampler||CandidateAliases(owner,&c,key))goto fail;
	memset(entries,0,sizeof(entries));
	entries[0]=(ralBindEntry_t){0,RAL_BIND_SAMPLED_TEXTURE,1,RAL_STAGE_COMPUTE};
	entries[1]=(ralBindEntry_t){1,RAL_BIND_SAMPLED_TEXTURE,1,RAL_STAGE_COMPUTE};
	entries[2]=(ralBindEntry_t){2,RAL_BIND_SAMPLER,1,RAL_STAGE_COMPUTE};
	entries[3]=(ralBindEntry_t){3,RAL_BIND_STORAGE_TEXTURE,1,RAL_STAGE_COMPUTE};
	entries[4]=(ralBindEntry_t){4,RAL_BIND_STORAGE_TEXTURE,1,RAL_STAGE_COMPUTE};
	memset(&lci,0,sizeof(lci));lci.entries=entries;lci.numEntries=5;
	lci.debugName="wired-temporal-history-store-bgl";
	c.layout=Ral_CreateBindGroupLayout(key->backend,&lci);
	if(!c.layout||CandidateAliases(owner,&c,key))goto fail;
	for(i=0;i<2;++i){ralBindingValue_t v[5];ralBindGroupCreateInfo_t g;
		memset(v,0,sizeof(v));v[0]=(ralBindingValue_t){.binding=0,.type=RAL_BIND_SAMPLED_TEXTURE,.textureView=key->feedbackColorView};
		v[1]=(ralBindingValue_t){.binding=1,.type=RAL_BIND_SAMPLED_TEXTURE,.textureView=c.currentDepthView};
		v[2]=(ralBindingValue_t){.binding=2,.type=RAL_BIND_SAMPLER,.sampler=c.sampler};
		v[3]=(ralBindingValue_t){.binding=3,.type=RAL_BIND_STORAGE_TEXTURE,.textureView=key->historyColorView[i]};
		v[4]=(ralBindingValue_t){.binding=4,.type=RAL_BIND_STORAGE_TEXTURE,.textureView=key->historyDepthView[i]};
		memset(&g,0,sizeof(g));g.layout=c.layout;g.values=v;g.numValues=5;
		g.debugName=i?"wired-temporal-history-store-bg-1":"wired-temporal-history-store-bg-0";
		c.groups[i]=Ral_CreateBindGroup(key->backend,&g);
		if(!c.groups[i]||CandidateAliases(owner,&c,key))goto fail;}
	layouts[0]=c.layout;memset(&pci,0,sizeof(pci));pci.computeSpirv=key->computeSpirv;
	pci.computeSpirvSize=key->computeSpirvSize;pci.bindGroupLayouts=layouts;
	pci.numBindGroupLayouts=1;pci.pushConstantSize=sizeof(vkTemporalHistoryStorePush_t);
	pci.debugName="wired-temporal-history-store-cs";
	c.pipeline=Ral_CreateComputePipeline(key->backend,&pci);
	if(!c.pipeline||CandidateAliases(owner,&c,key))goto fail;
	c.ready=qtrue;DestroyOwnedProtected(owner,&owner->key,key);*owner=c;return qtrue;
fail: DestroyCandidate(&c,owner,key);return qfalse;
}

qboolean VK_TemporalHistoryStoreRecord(
		const vkTemporalHistoryStoreOwner_t *owner,
		const vkTemporalHistoryStoreKey_t *expectedKey,
		uint32_t expectedAllocationGeneration,
		ralCommandBuffer_t *cb, uint32_t writeIndex,
		const vkTemporalHistoryStorePush_t *push ) {
	if(!owner||!expectedAllocationGeneration
			||owner->allocationGeneration!=expectedAllocationGeneration
			||!VK_TemporalHistoryStoreMatchesExact(owner,expectedKey)
			||!cb||writeIndex>1||!push
			||push->extent[0]!=owner->key.width||push->extent[1]!=owner->key.height
			||!isfinite(push->zNear)||!isfinite(push->zFar)
			||push->zNear<=0.0f||push->zFar<=push->zNear)return qfalse;
	Ral_CmdTransitionTexture(cb,owner->key.feedbackColor,
		RAL_PIPELINE_STAGE_TRANSFER_BIT|RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	Ral_CmdTransitionTexture(cb,owner->key.historyColor[writeIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_IMAGE_LAYOUT_GENERAL);
	Ral_CmdTransitionTexture(cb,owner->key.historyDepth[writeIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_IMAGE_LAYOUT_GENERAL);
	Ral_CmdBindPipeline(cb,owner->pipeline);Ral_CmdBindBindGroup(cb,0,owner->groups[writeIndex]);
	Ral_CmdPushConstants(cb,RAL_STAGE_COMPUTE,0,sizeof(*push),push);
	Ral_CmdDispatch(cb,(push->extent[0]+7u)/8u,(push->extent[1]+7u)/8u,1);
	Ral_CmdTransitionTexture(cb,owner->key.historyColor[writeIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	Ral_CmdTransitionTexture(cb,owner->key.historyDepth[writeIndex],
		RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return qtrue;
}

qboolean VK_TemporalHistoryStoreReleaseAfterIdle(
		vkTemporalHistoryStoreOwner_t *owner, qboolean idleProven ) {
	uint32_t generation;
	if(!owner||!owner->initialized||!idleProven)return qfalse;
	generation=owner->allocationGeneration;
	DestroyOwnedProtected(owner,&owner->key,NULL);memset(owner,0,sizeof(*owner));
	owner->initialized=qtrue;owner->allocationGeneration=generation;return qtrue;
}
