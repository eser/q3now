// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

_Static_assert( sizeof(ralFrameGraphPlan_t) <= 65536u,
	"bounded frame-graph plan must remain host/stack safe" );

static ralResourceState_t State( ralResourceUsage_t usage, uint32_t stages ) {
	ralResourceState_t state;
	state.usage = usage;
	state.shaderStages = stages;
	return state;
}

static void Fixture( ralBackendType_t backend,
		ralFrameGraphDescription_t *description ) {
	memset( description, 0, sizeof( *description ) );
	description->schemaVersion = RAL_FRAME_GRAPH_SCHEMA_VERSION;
	description->generation = 9u;
	description->backendType = backend;
	description->resourceCount = 3u;
	description->resources[0] = (ralFrameGraphResource_t){
		1u, 11u, (uintptr_t)100u, RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, 7u, 1024u, 256u, qtrue,
		{ RAL_RESOURCE_USAGE_UNDEFINED, 0u }, RAL_QUEUE_COMPUTE
	};
	description->resources[1] = (ralFrameGraphResource_t){
		2u, 12u, (uintptr_t)200u, RAL_FRAME_GRAPH_RESOURCE_TEXTURE,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, 8u, 2048u, 256u, qtrue,
		{ RAL_RESOURCE_USAGE_UNDEFINED, 0u }, RAL_QUEUE_GRAPHICS
	};
	description->resources[2] = (ralFrameGraphResource_t){
		3u, 13u, (uintptr_t)300u, RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT, 7u, 512u, 256u, qtrue,
		{ RAL_RESOURCE_USAGE_UNDEFINED, 0u }, RAL_QUEUE_GRAPHICS
	};
	description->passCount = 4u;
	description->passes[0] = (ralFrameGraphPass_t){ 100u, RAL_QUEUE_COMPUTE };
	description->passes[1] = (ralFrameGraphPass_t){ 200u, RAL_QUEUE_GRAPHICS };
	description->passes[2] = (ralFrameGraphPass_t){ 300u, RAL_QUEUE_TRANSFER };
	description->passes[3] = (ralFrameGraphPass_t){ 400u, RAL_QUEUE_GRAPHICS };
	description->useCount = 7u;
	// Intentionally not grouped by pass: compilation is keyed by stable IDs.
	description->uses[0] = (ralFrameGraphUse_t){ 400u, 3u,
		RAL_FRAME_GRAPH_ACCESS_WRITE, State(RAL_RESOURCE_USAGE_COPY_DESTINATION,0u) };
	description->uses[1] = (ralFrameGraphUse_t){ 100u, 1u,
		RAL_FRAME_GRAPH_ACCESS_WRITE, State(RAL_RESOURCE_USAGE_STORAGE_WRITE,RAL_STAGE_COMPUTE) };
	description->uses[2] = (ralFrameGraphUse_t){ 400u, 2u,
		RAL_FRAME_GRAPH_ACCESS_READ, State(RAL_RESOURCE_USAGE_SAMPLED_TEXTURE,RAL_STAGE_FRAGMENT) };
	description->uses[3] = (ralFrameGraphUse_t){ 200u, 1u,
		RAL_FRAME_GRAPH_ACCESS_READ, State(RAL_RESOURCE_USAGE_VERTEX_BUFFER,0u) };
	description->uses[4] = (ralFrameGraphUse_t){ 200u, 2u,
		RAL_FRAME_GRAPH_ACCESS_WRITE, State(RAL_RESOURCE_USAGE_COLOR_ATTACHMENT,0u) };
	description->uses[5] = (ralFrameGraphUse_t){ 300u, 1u,
		RAL_FRAME_GRAPH_ACCESS_WRITE, State(RAL_RESOURCE_USAGE_COPY_DESTINATION,0u) };
	description->uses[6] = (ralFrameGraphUse_t){ 300u, 2u,
		RAL_FRAME_GRAPH_ACCESS_READ, State(RAL_RESOURCE_USAGE_COPY_SOURCE,0u) };
	description->explicitDependencyCount = 1u;
	description->explicitDependencies[0]
		= (ralFrameGraphExplicitDependency_t){ 100u, 400u };
	description->transientPolicy.backendType = backend;
	description->transientPolicy.generation = description->generation;
	description->transientPolicy.budgetBytes = 8192u;
	description->transientPolicy.explicitAliasing
		= backend == RAL_BACKEND_WEBGPU ? qfalse : qtrue;
}

static qboolean EdgeHas( const ralFrameGraphPlan_t *plan, uint32_t source,
		uint32_t destination, ralFrameGraphHazardFlags_t flags,
		uint32_t resourceMask ) {
	uint32_t i;
	for ( i = 0u; i < plan->edgeCount; ++i )
		if ( plan->edges[i].sourcePassId == source
				&& plan->edges[i].destinationPassId == destination
				&& (plan->edges[i].hazards & flags) == flags
				&& (plan->edges[i].resourceMask & resourceMask) == resourceMask ) return qtrue;
	return qfalse;
}

static int PositiveAndMutations( void ) {
	ralFrameGraphDescription_t description,badDescription;
	ralFrameGraphPlan_t plan,exact,badPlan,beforePlan;
	Fixture( RAL_BACKEND_VULKAN, &description );
	CHECK( RalFrameGraph_Compile( &description, &plan ) );
	CHECK( plan.ready == qtrue && plan.schemaVersion == RAL_FRAME_GRAPH_SCHEMA_VERSION );
	CHECK( plan.topologicalPassCount == 4u
		&& plan.topologicalPassIds[0] == 100u && plan.topologicalPassIds[1] == 200u
		&& plan.topologicalPassIds[2] == 300u && plan.topologicalPassIds[3] == 400u );
	CHECK( EdgeHas(&plan,100u,200u,RAL_FRAME_GRAPH_HAZARD_RAW,1u<<0) );
	CHECK( EdgeHas(&plan,100u,300u,RAL_FRAME_GRAPH_HAZARD_WAW,1u<<0) );
	CHECK( EdgeHas(&plan,200u,300u,RAL_FRAME_GRAPH_HAZARD_WAR,1u<<0) );
	CHECK( EdgeHas(&plan,200u,400u,RAL_FRAME_GRAPH_HAZARD_RAW,1u<<1) );
	CHECK( EdgeHas(&plan,300u,400u,
		RAL_FRAME_GRAPH_HAZARD_STATE|RAL_FRAME_GRAPH_HAZARD_QUEUE_HANDOFF,1u<<1) );
	CHECK( EdgeHas(&plan,100u,400u,RAL_FRAME_GRAPH_HAZARD_EXPLICIT,0u) );
	CHECK( plan.transitionCount == 7u && plan.queueHandoffCount == 4u );
	CHECK( plan.queueHandoffs[0].resourceId == 1u
		&& plan.queueHandoffs[0].sourceQueue == RAL_QUEUE_COMPUTE
		&& plan.queueHandoffs[0].destinationQueue == RAL_QUEUE_GRAPHICS );
	CHECK( plan.queueHandoffs[1].resourceId == 1u
		&& plan.queueHandoffs[1].sourceQueue == RAL_QUEUE_GRAPHICS
		&& plan.queueHandoffs[1].destinationQueue == RAL_QUEUE_TRANSFER );
	CHECK( plan.queueHandoffs[2].resourceId == 2u
		&& plan.queueHandoffs[2].sourceQueue == RAL_QUEUE_GRAPHICS
		&& plan.queueHandoffs[2].destinationQueue == RAL_QUEUE_TRANSFER );
	CHECK( plan.queueHandoffs[3].resourceId == 2u
		&& plan.queueHandoffs[3].sourceQueue == RAL_QUEUE_TRANSFER
		&& plan.queueHandoffs[3].destinationQueue == RAL_QUEUE_GRAPHICS );
	CHECK( plan.transientRequestCount == 3u
		&& plan.transientRequests[0].firstPass == 0u
		&& plan.transientRequests[0].lastPass == 2u
		&& plan.transientRequests[2].firstPass == 3u );
	CHECK( plan.transientPlan.outcome == RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS
		&& plan.transientPlan.slotCount == 2u && plan.transientPlan.aliasCount == 1u
		&& plan.transientPlan.peakBytes == 3072u );
	exact = plan;
	CHECK( RalFrameGraph_PlanExact( &plan, &exact ) );
#define MUTATE_PLAN(statement) do { badPlan=plan; statement; CHECK(!RalFrameGraph_PlanExact(&badPlan,&badPlan)); } while(0)
	MUTATE_PLAN( badPlan.edgeCount-- );
	MUTATE_PLAN( badPlan.edges[0].hazards ^= RAL_FRAME_GRAPH_HAZARD_RAW );
	MUTATE_PLAN( badPlan.topologicalPassIds[0] = 200u );
	MUTATE_PLAN( badPlan.transitions[0].destinationPassId = 400u );
	MUTATE_PLAN( badPlan.queueHandoffs[0].resourceGeneration++ );
	MUTATE_PLAN( badPlan.transientRequests[0].lastPass++ );
	MUTATE_PLAN( badPlan.transientPlan.peakBytes++ );
	MUTATE_PLAN( badPlan.ready = qfalse );
#undef MUTATE_PLAN
#define REJECT(statement) do { badDescription=description; statement; memset(&badPlan,0x5a,sizeof(badPlan)); beforePlan=badPlan; \
	CHECK(!RalFrameGraph_Compile(&badDescription,&badPlan)); CHECK(memcmp(&badPlan,&beforePlan,sizeof(badPlan))==0); } while(0)
	REJECT( badDescription.schemaVersion++ );
	REJECT( badDescription.generation = UINT64_MAX );
	REJECT( badDescription.backendType = (ralBackendType_t)99 );
	REJECT( badDescription.resourceCount = RAL_FRAME_GRAPH_MAX_RESOURCES+1u );
	REJECT( badDescription.passCount = RAL_FRAME_GRAPH_MAX_PASSES+1u );
	REJECT( badDescription.useCount = RAL_FRAME_GRAPH_MAX_USES+1u );
	REJECT( badDescription.explicitDependencyCount
		= RAL_FRAME_GRAPH_MAX_EXPLICIT_DEPENDENCIES+1u );
	REJECT( badDescription.resources[0].id = 0u );
	REJECT( badDescription.resources[0].generation = 0u );
	REJECT( badDescription.resources[0].identity = (uintptr_t)0 );
	REJECT( badDescription.resources[0].kind = (ralFrameGraphResourceKind_t)99 );
	REJECT( badDescription.resources[0].lifetime = (ralFrameGraphResourceLifetime_t)99 );
	REJECT( badDescription.resources[1].id = badDescription.resources[0].id );
	REJECT( badDescription.resources[1].identity = badDescription.resources[0].identity );
	REJECT( badDescription.resources[0].compatibilityKey = 0u );
	REJECT( badDescription.resources[0].size = 0u );
	REJECT( badDescription.resources[0].alignment = 3u );
	REJECT( badDescription.resources[0].initialState.usage=RAL_RESOURCE_USAGE_COPY_DESTINATION );
	REJECT( badDescription.resources[0].allowAlias = (qboolean)2 );
	REJECT( badDescription.resources[0].lifetime=RAL_FRAME_GRAPH_RESOURCE_PERSISTENT;
		badDescription.resources[0].compatibilityKey=0u;
		badDescription.resources[0].allowAlias=qtrue );
	REJECT( badDescription.passes[1].id = badDescription.passes[0].id );
	REJECT( badDescription.passes[0].id = 0u );
	REJECT( badDescription.passes[0].queue = (ralQueueType_t)99 );
	REJECT( badDescription.uses[0] = badDescription.uses[1] );
	REJECT( badDescription.uses[0].passId = 999u );
	REJECT( badDescription.uses[0].resourceId = 999u );
	REJECT( badDescription.uses[0].access = (ralFrameGraphAccess_t)99 );
	REJECT( badDescription.uses[0].state.usage = RAL_RESOURCE_USAGE_UNDEFINED );
	REJECT( badDescription.useCount = 5u );
	REJECT( badDescription.explicitDependencies[0].destinationPassId = 100u );
	REJECT( badDescription.explicitDependencyCount=2u;
		badDescription.explicitDependencies[1]=badDescription.explicitDependencies[0] );
	REJECT( badDescription.explicitDependencies[0].sourcePassId=400u;
		badDescription.explicitDependencies[0].destinationPassId=100u );
	REJECT( badDescription.transientPolicy.generation++ );
	REJECT( badDescription.transientPolicy.backendType = RAL_BACKEND_METAL );
	REJECT( badDescription.transientPolicy.budgetBytes = 1024u );
#undef REJECT
	badDescription=description;
	badDescription.passes[2].queue=RAL_QUEUE_COMPUTE;
	badDescription.uses[5].access=RAL_FRAME_GRAPH_ACCESS_READ_WRITE;
	badDescription.uses[5].state=State(RAL_RESOURCE_USAGE_STORAGE_READ_WRITE,RAL_STAGE_COMPUTE);
	CHECK(RalFrameGraph_Compile(&badDescription,&badPlan));
	CHECK(EdgeHas(&badPlan,100u,300u,
		RAL_FRAME_GRAPH_HAZARD_RAW|RAL_FRAME_GRAPH_HAZARD_WAW,1u<<0));
	return 0;
}

static int ExternalResourceFixture( void ) {
	ralFrameGraphDescription_t description;
	ralFrameGraphPlan_t plan;
	memset(&description,0,sizeof(description));
	description.schemaVersion=RAL_FRAME_GRAPH_SCHEMA_VERSION;
	description.generation=31u;description.backendType=RAL_BACKEND_GL43;
	description.resourceCount=1u;
	description.resources[0]=(ralFrameGraphResource_t){
		9u,41u,(uintptr_t)900u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_EXTERNAL,0u,1024u,0u,qfalse,
		{RAL_RESOURCE_USAGE_HOST_WRITE,0u},RAL_QUEUE_GRAPHICS};
	description.passCount=2u;
	description.passes[0]=(ralFrameGraphPass_t){10u,RAL_QUEUE_GRAPHICS};
	description.passes[1]=(ralFrameGraphPass_t){20u,RAL_QUEUE_GRAPHICS};
	description.useCount=2u;
	description.uses[0]=(ralFrameGraphUse_t){10u,9u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_VERTEX_BUFFER,0u}};
	description.uses[1]=(ralFrameGraphUse_t){20u,9u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_VERTEX_BUFFER,0u}};
	description.explicitDependencyCount=1u;
	description.explicitDependencies[0]
		=(ralFrameGraphExplicitDependency_t){10u,20u};
	CHECK(RalFrameGraph_Compile(&description,&plan));
	CHECK(plan.transitionCount==1u&&plan.transitions[0].sourcePassId==0u
		&& plan.transitions[0].destinationPassId==10u);
	CHECK(plan.queueHandoffCount==0u&&plan.transientRequestCount==0u
		&& plan.transientPlan.ready==qfalse);
	CHECK(RalFrameGraph_PlanExact(&plan,&plan));
	plan.transientPlan.peakBytes=1u;
	CHECK(!RalFrameGraph_PlanExact(&plan,&plan));
	description.transientPolicy.generation=1u;
	CHECK(!RalFrameGraph_Compile(&description,&plan));
	return 0;
}

static int WebGpuLogicalQueueFixture( void ) {
	ralFrameGraphDescription_t description;
	ralFrameGraphPlan_t plan;
	Fixture( RAL_BACKEND_WEBGPU, &description );
	CHECK( RalFrameGraph_Compile( &description, &plan ) );
	CHECK( plan.queueHandoffCount == 4u );
	CHECK( plan.transientPlan.outcome == RAL_TRANSIENT_PLAN_MANAGED_DISJOINT
		&& plan.transientPlan.slotCount == 3u && plan.transientPlan.aliasCount == 0u );
	CHECK( RalFrameGraph_PlanExact( &plan, &plan ) );
	description.transientPolicy.explicitAliasing = qtrue;
	CHECK( !RalFrameGraph_Compile( &description, &plan ) );
	return 0;
}

int main( void ) {
	CHECK( PositiveAndMutations() == 0 );
	CHECK( ExternalResourceFixture() == 0 );
	CHECK( WebGpuLogicalQueueFixture() == 0 );
	puts( "ral frame graph: PASS" );
	return 0;
}
