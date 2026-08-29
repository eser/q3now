// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_atmosphere_frame_graph.h"

#include <limits.h>
#include <string.h>

static ralResourceState_t AtmosphereState( ralResourceUsage_t usage ) {
	ralResourceState_t state;
	state.usage = usage;
	state.shaderStages = usage == RAL_RESOURCE_USAGE_UNDEFINED
		? 0u : RAL_STAGE_COMPUTE;
	return state;
}

static void AtmosphereResource( ralFrameGraphResource_t *resource,
		const ralAtmosphereFrameGraphRequest_t *request, uint32_t id,
		ralFrameGraphResourceKind_t kind,
		ralFrameGraphResourceLifetime_t lifetime, uint64_t size,
		qboolean allowAlias, ralResourceState_t initial ) {
	memset( resource, 0, sizeof( *resource ) );
	resource->id = id;
	resource->generation = request->generation;
	resource->identity = request->resourceIdentityBase + id;
	resource->kind = kind;
	resource->lifetime = lifetime;
	resource->compatibilityKey = lifetime == RAL_FRAME_GRAPH_RESOURCE_TRANSIENT
		? 0x41544d425546ull : 0u;
	resource->size = size;
	resource->alignment = lifetime == RAL_FRAME_GRAPH_RESOURCE_TRANSIENT
		? 256u : 0u;
	resource->allowAlias = allowAlias;
	resource->initialState = initial;
	resource->initialQueue = RAL_QUEUE_GRAPHICS;
}

static void AtmosphereUse( ralFrameGraphUse_t *use, uint32_t passId,
		uint32_t resourceId, ralFrameGraphAccess_t access,
		ralResourceUsage_t usage ) {
	use->passId = passId;
	use->resourceId = resourceId;
	use->access = access;
	use->state = AtmosphereState( usage );
}

qboolean Ral_AtmosphereFrameGraphBuild(
		const ralAtmosphereFrameGraphRequest_t *request,
		ralFrameGraphPlan_t *outPlan ) {
	ralFrameGraphDescription_t description;
	uint64_t froxelBytes, outputBytes;
	uint32_t use = 0u;
	qboolean cloudsActive;
	if ( !request || !outPlan
			|| request->schemaVersion
				!= RAL_ATMOSPHERE_FRAME_GRAPH_SCHEMA_VERSION
			|| request->generation == 0u || request->generation == UINT64_MAX
			|| request->resourceIdentityBase == 0u
			|| request->resourceIdentityBase > UINTPTR_MAX - 9u
			|| request->transientBudgetBytes == 0u
			|| !Ral_AtmospherePlanReceiptExact( &request->atmosphere,
				&request->atmosphere )
			|| request->atmosphere.selectedTier != RAL_ATMOSPHERE_TIER_FULL
			|| request->atmosphere.froxelCount == 0u
			|| request->atmosphere.compositeCount != 1u
			|| !( request->atmosphere.passMask
				& RAL_ATMOSPHERE_PASS_SINGLE_COMPOSITE ) ) return qfalse;
	froxelBytes = (uint64_t)request->atmosphere.froxelCount * 16u;
	outputBytes = (uint64_t)request->atmosphere.froxelWidth
		* RAL_ATMOSPHERE_FROXEL_TILE_SIZE
		* request->atmosphere.froxelHeight
		* RAL_ATMOSPHERE_FROXEL_TILE_SIZE * 8u;
	if ( froxelBytes == 0u || outputBytes == 0u ) return qfalse;
	cloudsActive = request->atmosphere.cloudsActive
		&& ( request->atmosphere.passMask & RAL_ATMOSPHERE_PASS_CLOUDS )
		? qtrue : qfalse;
	memset( &description, 0, sizeof( description ) );
	description.schemaVersion = RAL_FRAME_GRAPH_SCHEMA_VERSION;
	description.generation = request->generation;
	description.backendType = request->atmosphere.backendType;
	description.resourceCount = cloudsActive ? 9u : 8u;
	AtmosphereResource( &description.resources[0], request,
		RAL_ATMOSPHERE_RESOURCE_SCENE_HDR, RAL_FRAME_GRAPH_RESOURCE_TEXTURE,
		RAL_FRAME_GRAPH_RESOURCE_EXTERNAL, outputBytes, qfalse,
		AtmosphereState( RAL_RESOURCE_USAGE_SAMPLED_TEXTURE ) );
	AtmosphereResource( &description.resources[1], request,
		RAL_ATMOSPHERE_RESOURCE_SCENE_DEPTH, RAL_FRAME_GRAPH_RESOURCE_TEXTURE,
		RAL_FRAME_GRAPH_RESOURCE_EXTERNAL, outputBytes / 2u, qfalse,
		AtmosphereState( RAL_RESOURCE_USAGE_SAMPLED_TEXTURE ) );
	AtmosphereResource( &description.resources[2], request,
		RAL_ATMOSPHERE_RESOURCE_MEDIA, RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, froxelBytes, qtrue,
		AtmosphereState( RAL_RESOURCE_USAGE_UNDEFINED ) );
	AtmosphereResource( &description.resources[3], request,
		RAL_ATMOSPHERE_RESOURCE_LIT_MEDIA, RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, froxelBytes, qtrue,
		AtmosphereState( RAL_RESOURCE_USAGE_UNDEFINED ) );
	AtmosphereResource( &description.resources[4], request,
		RAL_ATMOSPHERE_RESOURCE_INTEGRATED, RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, froxelBytes, qtrue,
		AtmosphereState( RAL_RESOURCE_USAGE_UNDEFINED ) );
	AtmosphereResource( &description.resources[5], request,
		RAL_ATMOSPHERE_RESOURCE_HISTORY_PREVIOUS,
		RAL_FRAME_GRAPH_RESOURCE_BUFFER, RAL_FRAME_GRAPH_RESOURCE_PERSISTENT,
		froxelBytes, qfalse, AtmosphereState( RAL_RESOURCE_USAGE_STORAGE_READ ) );
	AtmosphereResource( &description.resources[6], request,
		RAL_ATMOSPHERE_RESOURCE_HISTORY_NEXT,
		RAL_FRAME_GRAPH_RESOURCE_BUFFER, RAL_FRAME_GRAPH_RESOURCE_PERSISTENT,
		froxelBytes, qfalse, AtmosphereState( RAL_RESOURCE_USAGE_STORAGE_READ ) );
	AtmosphereResource( &description.resources[7], request,
		RAL_ATMOSPHERE_RESOURCE_COMPOSED_HDR, RAL_FRAME_GRAPH_RESOURCE_TEXTURE,
		RAL_FRAME_GRAPH_RESOURCE_EXTERNAL, outputBytes, qfalse,
		AtmosphereState( RAL_RESOURCE_USAGE_SAMPLED_TEXTURE ) );
	if ( cloudsActive )
		AtmosphereResource( &description.resources[8], request,
			RAL_ATMOSPHERE_RESOURCE_CLOUD_MEDIA,
			RAL_FRAME_GRAPH_RESOURCE_BUFFER,
			RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, froxelBytes, qtrue,
			AtmosphereState( RAL_RESOURCE_USAGE_UNDEFINED ) );
	description.passCount = cloudsActive ? 5u : 4u;
	description.passes[0] = (ralFrameGraphPass_t){
		RAL_ATMOSPHERE_PASS_ID_MEDIA_INJECT, RAL_QUEUE_COMPUTE };
	description.passes[1] = (ralFrameGraphPass_t){
		RAL_ATMOSPHERE_PASS_ID_LIGHT_INJECT, RAL_QUEUE_COMPUTE };
	if ( cloudsActive ) description.passes[2] = (ralFrameGraphPass_t){
		RAL_ATMOSPHERE_PASS_ID_CLOUD_INJECT, RAL_QUEUE_COMPUTE };
	description.passes[cloudsActive ? 3u : 2u] = (ralFrameGraphPass_t){
		RAL_ATMOSPHERE_PASS_ID_INTEGRATE, RAL_QUEUE_COMPUTE };
	description.passes[cloudsActive ? 4u : 3u] = (ralFrameGraphPass_t){
		RAL_ATMOSPHERE_PASS_ID_COMPOSITE, RAL_QUEUE_COMPUTE };
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_MEDIA_INJECT,
		RAL_ATMOSPHERE_RESOURCE_MEDIA, RAL_FRAME_GRAPH_ACCESS_WRITE,
		RAL_RESOURCE_USAGE_STORAGE_WRITE );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_LIGHT_INJECT,
		RAL_ATMOSPHERE_RESOURCE_MEDIA, RAL_FRAME_GRAPH_ACCESS_READ,
		RAL_RESOURCE_USAGE_STORAGE_READ );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_LIGHT_INJECT,
		RAL_ATMOSPHERE_RESOURCE_LIT_MEDIA, RAL_FRAME_GRAPH_ACCESS_WRITE,
		RAL_RESOURCE_USAGE_STORAGE_WRITE );
	if ( cloudsActive ) {
		AtmosphereUse( &description.uses[use++],
			RAL_ATMOSPHERE_PASS_ID_CLOUD_INJECT,
			RAL_ATMOSPHERE_RESOURCE_LIT_MEDIA, RAL_FRAME_GRAPH_ACCESS_READ,
			RAL_RESOURCE_USAGE_STORAGE_READ );
		AtmosphereUse( &description.uses[use++],
			RAL_ATMOSPHERE_PASS_ID_CLOUD_INJECT,
			RAL_ATMOSPHERE_RESOURCE_CLOUD_MEDIA, RAL_FRAME_GRAPH_ACCESS_WRITE,
			RAL_RESOURCE_USAGE_STORAGE_WRITE );
	}
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_INTEGRATE,
		cloudsActive ? RAL_ATMOSPHERE_RESOURCE_CLOUD_MEDIA
			: RAL_ATMOSPHERE_RESOURCE_LIT_MEDIA, RAL_FRAME_GRAPH_ACCESS_READ,
		RAL_RESOURCE_USAGE_STORAGE_READ );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_INTEGRATE,
		RAL_ATMOSPHERE_RESOURCE_HISTORY_PREVIOUS, RAL_FRAME_GRAPH_ACCESS_READ,
		RAL_RESOURCE_USAGE_STORAGE_READ );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_INTEGRATE,
		RAL_ATMOSPHERE_RESOURCE_INTEGRATED, RAL_FRAME_GRAPH_ACCESS_WRITE,
		RAL_RESOURCE_USAGE_STORAGE_WRITE );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_INTEGRATE,
		RAL_ATMOSPHERE_RESOURCE_HISTORY_NEXT, RAL_FRAME_GRAPH_ACCESS_WRITE,
		RAL_RESOURCE_USAGE_STORAGE_WRITE );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_COMPOSITE,
		RAL_ATMOSPHERE_RESOURCE_SCENE_HDR, RAL_FRAME_GRAPH_ACCESS_READ,
		RAL_RESOURCE_USAGE_SAMPLED_TEXTURE );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_COMPOSITE,
		RAL_ATMOSPHERE_RESOURCE_SCENE_DEPTH, RAL_FRAME_GRAPH_ACCESS_READ,
		RAL_RESOURCE_USAGE_SAMPLED_TEXTURE );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_COMPOSITE,
		RAL_ATMOSPHERE_RESOURCE_INTEGRATED, RAL_FRAME_GRAPH_ACCESS_READ,
		RAL_RESOURCE_USAGE_STORAGE_READ );
	AtmosphereUse( &description.uses[use++], RAL_ATMOSPHERE_PASS_ID_COMPOSITE,
		RAL_ATMOSPHERE_RESOURCE_COMPOSED_HDR, RAL_FRAME_GRAPH_ACCESS_WRITE,
		RAL_RESOURCE_USAGE_STORAGE_WRITE );
	description.useCount = use;
	description.transientPolicy.backendType = description.backendType;
	description.transientPolicy.generation = description.generation;
	description.transientPolicy.budgetBytes = request->transientBudgetBytes;
	description.transientPolicy.explicitAliasing = description.backendType
		== RAL_BACKEND_WEBGPU ? qfalse : qtrue;
	return RalFrameGraph_Compile( &description, outPlan );
}
