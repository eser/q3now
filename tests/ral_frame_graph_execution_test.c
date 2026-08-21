// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_execution.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define ID(type,value) ((type *)(uintptr_t)(value))

_Static_assert( sizeof(ralFrameGraphExecutionPlan_t) <= 131072u,
	"frame-graph execution plan must remain bounded" );

static void GraphFixture( ralBackendType_t backend,
		ralFrameGraphDescription_t *description ) {
	memset(description,0,sizeof(*description));
	description->schemaVersion=RAL_FRAME_GRAPH_SCHEMA_VERSION;
	description->generation=9u;description->backendType=backend;
	description->resourceCount=3u;
	description->resources[0]=(ralFrameGraphResource_t){
		1u,11u,(uintptr_t)100u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT,7u,1024u,256u,qtrue,
		{RAL_RESOURCE_USAGE_UNDEFINED,0u},RAL_QUEUE_COMPUTE};
	description->resources[1]=(ralFrameGraphResource_t){
		2u,12u,(uintptr_t)200u,RAL_FRAME_GRAPH_RESOURCE_TEXTURE,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT,8u,2048u,256u,qtrue,
		{RAL_RESOURCE_USAGE_UNDEFINED,0u},RAL_QUEUE_GRAPHICS};
	description->resources[2]=(ralFrameGraphResource_t){
		3u,13u,(uintptr_t)300u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_TRANSIENT,7u,512u,256u,qtrue,
		{RAL_RESOURCE_USAGE_UNDEFINED,0u},RAL_QUEUE_GRAPHICS};
	description->passCount=4u;
	description->passes[0]=(ralFrameGraphPass_t){100u,RAL_QUEUE_COMPUTE};
	description->passes[1]=(ralFrameGraphPass_t){200u,RAL_QUEUE_GRAPHICS};
	description->passes[2]=(ralFrameGraphPass_t){300u,RAL_QUEUE_TRANSFER};
	description->passes[3]=(ralFrameGraphPass_t){400u,RAL_QUEUE_GRAPHICS};
	description->useCount=7u;
	description->uses[0]=(ralFrameGraphUse_t){400u,3u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_COPY_DESTINATION,0u}};
	description->uses[1]=(ralFrameGraphUse_t){100u,1u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_STORAGE_WRITE,RAL_STAGE_COMPUTE}};
	description->uses[2]=(ralFrameGraphUse_t){400u,2u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_SAMPLED_TEXTURE,RAL_STAGE_FRAGMENT}};
	description->uses[3]=(ralFrameGraphUse_t){200u,1u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_VERTEX_BUFFER,0u}};
	description->uses[4]=(ralFrameGraphUse_t){200u,2u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_COLOR_ATTACHMENT,0u}};
	description->uses[5]=(ralFrameGraphUse_t){300u,1u,RAL_FRAME_GRAPH_ACCESS_WRITE,
		{RAL_RESOURCE_USAGE_COPY_DESTINATION,0u}};
	description->uses[6]=(ralFrameGraphUse_t){300u,2u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_COPY_SOURCE,0u}};
	description->explicitDependencyCount=1u;
	description->explicitDependencies[0]
		=(ralFrameGraphExplicitDependency_t){100u,400u};
	description->transientPolicy.backendType=backend;
	description->transientPolicy.generation=description->generation;
	description->transientPolicy.budgetBytes=8192u;
	description->transientPolicy.explicitAliasing
		=backend==RAL_BACKEND_WEBGPU?qfalse:qtrue;
}

static int ExecutionFixture( ralBackendType_t backend,
		ralFrameGraphDescription_t *graphDescription, ralFrameGraphPlan_t *graph,
		ralFrameGraphExecutionDescription_t *execution ) {
	GraphFixture(backend,graphDescription);
	CHECK(RalFrameGraph_Compile(graphDescription,graph));
	memset(execution,0,sizeof(*execution));
	execution->schemaVersion=RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION;
	execution->generation=19u;
	execution->backendIdentity=ID(ralBackend_t,0x1000u);
	execution->graphPlan=graph;
	execution->physicalQueueForLogical[RAL_QUEUE_GRAPHICS]=RAL_QUEUE_GRAPHICS;
	execution->physicalQueueForLogical[RAL_QUEUE_COMPUTE]
		=backend==RAL_BACKEND_WEBGPU?RAL_QUEUE_GRAPHICS:RAL_QUEUE_COMPUTE;
	execution->physicalQueueForLogical[RAL_QUEUE_TRANSFER]
		=backend==RAL_BACKEND_WEBGPU?RAL_QUEUE_GRAPHICS:RAL_QUEUE_TRANSFER;
	execution->timelineBaseValues[RAL_QUEUE_GRAPHICS]=10u;
	execution->timelineBaseValues[RAL_QUEUE_COMPUTE]=20u;
	execution->timelineBaseValues[RAL_QUEUE_TRANSFER]=30u;
	execution->bindingCount=3u;
	execution->bindings[0]=(ralFrameGraphResourceBinding_t){
		1u,(uintptr_t)100u,11u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		ID(ralBuffer_t,0x2000u),NULL,1024u,0u,0u,0u};
	execution->bindings[1]=(ralFrameGraphResourceBinding_t){
		2u,(uintptr_t)200u,12u,RAL_FRAME_GRAPH_RESOURCE_TEXTURE,
		NULL,ID(ralTexture_t,0x3000u),0u,RAL_TEXTURE_ASPECT_COLOR,4u,2u};
	execution->bindings[2]=(ralFrameGraphResourceBinding_t){
		3u,(uintptr_t)300u,13u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		ID(ralBuffer_t,0x4000u),NULL,512u,0u,0u,0u};
	return 0;
}

static qboolean WaitHas( const ralFrameGraphSubmissionBatch_t *batch,
		ralQueueType_t queue, uint64_t value ) {
	uint32_t i;
	for(i=0u;i<batch->waitCount;++i)
		if(batch->waits[i].sourcePhysicalQueue==queue
				&&batch->waits[i].value==value)return qtrue;
	return qfalse;
}

static int DedicatedQueuesAndMutations( void ) {
	ralFrameGraphDescription_t graphDescription;
	ralFrameGraphPlan_t graph,otherGraph,badGraph;
	ralFrameGraphExecutionDescription_t description,badDescription;
	ralFrameGraphExecutionPlan_t plan,badPlan,beforePlan;
	CHECK(ExecutionFixture(RAL_BACKEND_VULKAN,&graphDescription,&graph,&description)==0);
	CHECK(RalFrameGraphExecution_Compile(&description,&plan));
	CHECK(plan.ready==qtrue&&plan.passCount==4u&&plan.submissionBatchCount==4u);
	CHECK(plan.passes[0].passId==100u&&plan.passes[0].physicalQueue==RAL_QUEUE_COMPUTE);
	CHECK(plan.passes[1].passId==200u&&plan.passes[1].physicalQueue==RAL_QUEUE_GRAPHICS);
	CHECK(plan.preBufferTransitionCount==2u&&plan.preTextureTransitionCount==1u);
	CHECK(plan.acquireBufferTransitionCount==2u
		&&plan.releaseBufferTransitionCount==2u
		&&plan.acquireTextureTransitionCount==2u
		&&plan.releaseTextureTransitionCount==2u);
	CHECK(plan.passes[0].preBufferTransitionCount==1u
		&&plan.passes[0].releaseBufferTransitionCount==1u);
	CHECK(plan.passes[1].acquireBufferTransitionCount==1u
		&&plan.passes[1].preTextureTransitionCount==1u);
	CHECK(plan.submissionBatches[0].signalValue==21u
		&&plan.submissionBatches[1].signalValue==11u
		&&plan.submissionBatches[2].signalValue==31u
		&&plan.submissionBatches[3].signalValue==12u);
	CHECK(WaitHas(&plan.submissionBatches[1],RAL_QUEUE_COMPUTE,21u));
	CHECK(WaitHas(&plan.submissionBatches[2],RAL_QUEUE_GRAPHICS,11u));
	CHECK(WaitHas(&plan.submissionBatches[2],RAL_QUEUE_COMPUTE,21u));
	CHECK(WaitHas(&plan.submissionBatches[3],RAL_QUEUE_COMPUTE,21u));
	CHECK(WaitHas(&plan.submissionBatches[3],RAL_QUEUE_TRANSFER,31u));
	CHECK(plan.timelineFinalValues[RAL_QUEUE_GRAPHICS]==12u
		&&plan.timelineFinalValues[RAL_QUEUE_COMPUTE]==21u
		&&plan.timelineFinalValues[RAL_QUEUE_TRANSFER]==31u);
	CHECK(RalFrameGraphExecution_PlanExact(&plan,&plan));
#define MUTATE_PLAN(statement) do{badPlan=plan;statement;CHECK(!RalFrameGraphExecution_PlanExact(&badPlan,&badPlan));}while(0)
	MUTATE_PLAN(badPlan.passCount--);
	MUTATE_PLAN(badPlan.passes[1].physicalQueue=RAL_QUEUE_TRANSFER);
	MUTATE_PLAN(badPlan.preBufferTransitions[0].size++);
	MUTATE_PLAN(badPlan.acquireTextureTransitions[0].arrayLayerCount--);
	MUTATE_PLAN(badPlan.releaseBufferTransitions[0].after.usage=RAL_RESOURCE_USAGE_COPY_SOURCE);
	MUTATE_PLAN(badPlan.submissionBatches[1].waitCount=0u);
	MUTATE_PLAN(badPlan.submissionBatches[0].signalValue++);
	MUTATE_PLAN(badPlan.timelineFinalValues[RAL_QUEUE_GRAPHICS]++);
	MUTATE_PLAN(badPlan.ready=qfalse);
#undef MUTATE_PLAN
#define REJECT(statement) do{badDescription=description;statement;memset(&badPlan,0x5a,sizeof(badPlan));beforePlan=badPlan;CHECK(!RalFrameGraphExecution_Compile(&badDescription,&badPlan));CHECK(memcmp(&badPlan,&beforePlan,sizeof(badPlan))==0);}while(0)
	REJECT(badDescription.schemaVersion++);
	REJECT(badDescription.generation=UINT64_MAX);
	REJECT(badDescription.backendIdentity=NULL);
	REJECT(badDescription.bindingCount--);
	REJECT(badDescription.bindings[0].resourceId=99u);
	REJECT(badDescription.bindings[0].resourceIdentity++);
	REJECT(badDescription.bindings[0].resourceGeneration++);
	REJECT(badDescription.bindings[0].resourceKind=RAL_FRAME_GRAPH_RESOURCE_TEXTURE);
	REJECT(badDescription.bindings[0].buffer=NULL);
	REJECT(badDescription.bindings[0].texture=ID(ralTexture_t,0x99u));
	REJECT(badDescription.bindings[0].bufferSize=0u);
	REJECT(badDescription.bindings[0].bufferSize=512u);
	REJECT(badDescription.bindings[1].texture=NULL);
	REJECT(badDescription.bindings[1].textureAspects=0u);
	REJECT(badDescription.bindings[1].textureMipLevels=0u);
	REJECT(badDescription.bindings[1].textureArrayLayers=0u);
	REJECT(badDescription.bindings[2].buffer=badDescription.bindings[0].buffer);
	REJECT(badDescription.bindings[2].resourceId=badDescription.bindings[0].resourceId);
	REJECT(badDescription.bindings[3].resourceId=44u);
	REJECT(badDescription.physicalQueueForLogical[0]=(ralQueueType_t)99);
	REJECT(badDescription.timelineBaseValues[0]=UINT64_MAX);
#undef REJECT
	badGraph=graph;badGraph.transitions[0].resourceGeneration++;
	badDescription=description;badDescription.graphPlan=&badGraph;
	CHECK(!RalFrameGraphExecution_Compile(&badDescription,&badPlan));
	GraphFixture(RAL_BACKEND_VULKAN,&graphDescription);
	CHECK(RalFrameGraph_Compile(&graphDescription,&otherGraph));
	otherGraph.description.generation++;
	badDescription=description;badDescription.graphPlan=&otherGraph;
	CHECK(!RalFrameGraphExecution_Compile(&badDescription,&badPlan));
	return 0;
}

static int SharedQueueCoalescing( void ) {
	ralFrameGraphDescription_t graphDescription;
	ralFrameGraphPlan_t graph;
	ralFrameGraphExecutionDescription_t description,bad;
	ralFrameGraphExecutionPlan_t plan;
	CHECK(ExecutionFixture(RAL_BACKEND_WEBGPU,&graphDescription,&graph,&description)==0);
	CHECK(RalFrameGraphExecution_Compile(&description,&plan));
	CHECK(plan.passCount==4u&&plan.submissionBatchCount==1u);
	CHECK(plan.submissionBatches[0].physicalQueue==RAL_QUEUE_GRAPHICS
		&&plan.submissionBatches[0].executionPassCount==4u
		&&plan.submissionBatches[0].waitCount==0u
		&&plan.submissionBatches[0].signalValue==11u);
	CHECK(plan.preBufferTransitionCount==4u&&plan.preTextureTransitionCount==3u);
	CHECK(plan.acquireBufferTransitionCount==0u
		&&plan.releaseBufferTransitionCount==0u
		&&plan.acquireTextureTransitionCount==0u
		&&plan.releaseTextureTransitionCount==0u);
	CHECK(plan.timelineFinalValues[RAL_QUEUE_GRAPHICS]==11u
		&&plan.timelineFinalValues[RAL_QUEUE_COMPUTE]==20u
		&&plan.timelineFinalValues[RAL_QUEUE_TRANSFER]==30u);
	CHECK(RalFrameGraphExecution_PlanExact(&plan,&plan));
	bad=description;bad.physicalQueueForLogical[RAL_QUEUE_TRANSFER]=RAL_QUEUE_TRANSFER;
	CHECK(!RalFrameGraphExecution_Compile(&bad,&plan));
	return 0;
}

static int ImportedDedicatedFirstUseRejects( void ) {
	ralFrameGraphDescription_t graphDescription;
	ralFrameGraphPlan_t graph;
	ralFrameGraphExecutionDescription_t description;
	ralFrameGraphExecutionPlan_t plan;
	memset(&graphDescription,0,sizeof(graphDescription));
	graphDescription.schemaVersion=RAL_FRAME_GRAPH_SCHEMA_VERSION;
	graphDescription.generation=51u;graphDescription.backendType=RAL_BACKEND_VULKAN;
	graphDescription.resourceCount=1u;
	graphDescription.resources[0]=(ralFrameGraphResource_t){
		1u,2u,(uintptr_t)3u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		RAL_FRAME_GRAPH_RESOURCE_EXTERNAL,0u,256u,0u,qfalse,
		{RAL_RESOURCE_USAGE_COPY_DESTINATION,0u},RAL_QUEUE_TRANSFER};
	graphDescription.passCount=1u;
	graphDescription.passes[0]=(ralFrameGraphPass_t){4u,RAL_QUEUE_GRAPHICS};
	graphDescription.useCount=1u;
	graphDescription.uses[0]=(ralFrameGraphUse_t){4u,1u,RAL_FRAME_GRAPH_ACCESS_READ,
		{RAL_RESOURCE_USAGE_VERTEX_BUFFER,0u}};
	CHECK(RalFrameGraph_Compile(&graphDescription,&graph));
	memset(&description,0,sizeof(description));
	description.schemaVersion=RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION;
	description.generation=5u;description.backendIdentity=ID(ralBackend_t,6u);
	description.graphPlan=&graph;
	description.physicalQueueForLogical[0]=RAL_QUEUE_GRAPHICS;
	description.physicalQueueForLogical[1]=RAL_QUEUE_COMPUTE;
	description.physicalQueueForLogical[2]=RAL_QUEUE_TRANSFER;
	description.bindingCount=1u;
	description.bindings[0]=(ralFrameGraphResourceBinding_t){
		1u,(uintptr_t)3u,2u,RAL_FRAME_GRAPH_RESOURCE_BUFFER,
		ID(ralBuffer_t,7u),NULL,256u,0u,0u,0u};
	CHECK(!RalFrameGraphExecution_Compile(&description,&plan));
	description.physicalQueueForLogical[RAL_QUEUE_TRANSFER]=RAL_QUEUE_GRAPHICS;
	CHECK(RalFrameGraphExecution_Compile(&description,&plan));
	CHECK(plan.submissionBatchCount==1u&&plan.preBufferTransitionCount==1u);
	return 0;
}

int main( void ) {
	CHECK(DedicatedQueuesAndMutations()==0);
	CHECK(SharedQueueCoalescing()==0);
	CHECK(ImportedDedicatedFirstUseRejects()==0);
	puts("ral frame graph execution: PASS");
	return 0;
}
