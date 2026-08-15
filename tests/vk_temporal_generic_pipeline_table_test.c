// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_generic_pipeline_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL pipeline table %d: %s\n",__LINE__,#x);return 1;} } while(0)
#define VK_TEMPORAL_BLOB(name, size) const unsigned char name[size] = { 0 };
#define VK_TEMPORAL_PAIR(tx, family, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS)
#include "../code/renderervk/shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB

typedef struct {
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription binding;
	VkPipelineVertexInputStateCreateInfo vi;
	VkPipelineInputAssemblyStateCreateInfo ia;
	VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;
	VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineDepthStencilStateCreateInfo ds;
	VkPipelineColorBlendAttachmentState attachment;
	VkPipelineColorBlendStateCreateInfo cb;
	VkDynamicState states[2];
	VkPipelineDynamicStateCreateInfo dynamic;
	VkGraphicsPipelineCreateInfo gp;
	vkGenericSpecializationGraph_t specs;
	uint32_t vsWord, fsWords[VK_GENERIC_FRAGMENT_SPEC_COUNT];
} Fixture;

typedef struct {
	ralPipeline_t **published;
	qboolean *depthFade;
	int32_t *createCount;
	ralPipeline_t *candidate;
	ralPipeline_t *expectedRetained;
	vkTemporalCachedMainMaterializeResult_t result;
	qboolean captureCallOk, attempted, valid;
	int events[8], eventCount, destroys;
	ralPipeline_t *destroyed[2];
} RebuildFixture;

static void RebuildCreate(void*opaque){RebuildFixture*f=opaque;f->events[f->eventCount++]=1;
	*f->published=f->candidate;*f->depthFade=qfalse;*f->createCount+=5u;}
static qboolean RebuildState(void*opaque,qboolean*a,qboolean*v){RebuildFixture*f=opaque;
	f->events[f->eventCount++]=2;if(!f->captureCallOk)return qfalse;*a=f->attempted;*v=f->valid;return qtrue;}
static vkTemporalCachedMainMaterializeResult_t RebuildMaterialize(void*opaque,ralPipeline_t*retained){RebuildFixture*f=opaque;
	f->events[f->eventCount++]=3;if(retained!=f->expectedRetained)return VK_TEMPORAL_CACHED_MAIN_REJECTED;return f->result;}
static RebuildFixture *s_rebuild;
static void RebuildDestroy(ralPipeline_t*p){s_rebuild->events[s_rebuild->eventCount++]=4;
	s_rebuild->destroyed[s_rebuild->destroys++]=p;}
static void RebuildDecline(void*opaque){RebuildFixture*f=opaque;f->events[f->eventCount++]=5;}

static int s_allocs,s_frees,s_creates,s_destroys,s_drains,s_failAt,s_alias,s_drainFails;
static vkTemporalGenericCatalogEntry_t s_entry;
static ralPipeline_t *s_first,*s_forced;
static void *Alloc(size_t n){s_allocs++;return malloc(n);}
static void Free(void*p){s_frees++;free(p);}
static qboolean Drain(ralBackend_t*b){(void)b;s_drains++;if(s_drainFails){s_drainFails--;return qfalse;}return qtrue;}
static void Destroy(ralPipeline_t*p){if(p)s_destroys++;}
static qboolean Lookup(VkShaderModule m,vkTemporalShaderBlob_t*out){
	if(!out)return qfalse;
	if((uintptr_t)m==1u)*out=s_entry.ordinaryVertex;
	else if((uintptr_t)m==2u)*out=s_entry.ordinaryFragment;
	else return qfalse;
	return qtrue;
}
static ralPipeline_t *Create(const VkGraphicsPipelineCreateInfo*ci,
		ralPipelineLayout_t*l,const ralFormat_t*f,uint32_t n,ralFormat_t d,
		const vkTemporalSpirvOverrides_t*o,const char*name){
	(void)l;(void)d;(void)name;s_creates++;
	if(s_failAt==s_creates)return NULL;
	if(!ci||!o||n!=3u||f[1]!=RAL_FORMAT_R16G16_SFLOAT
			||f[2]!=RAL_FORMAT_R8_UNORM)return NULL;
	if(s_forced)return s_forced;
	if(s_alias&&s_first)return s_first;
	{ ralPipeline_t*p=(ralPipeline_t*)(uintptr_t)(0x100u+(uint32_t)s_creates);
		if(!s_first)s_first=p;return p; }
}
static void FloatWord(uint32_t*out,float v){memcpy(out,&v,sizeof(v));}
static qboolean MakeFixture(Fixture*f){
	memset(f,0,sizeof(*f));f->vsWord=1u;FloatWord(&f->fsWords[2],0.85f);
	FloatWord(&f->fsWords[8],1.0f);FloatWord(&f->fsWords[9],1.0f);
	FloatWord(&f->fsWords[11],2.0f);
	if(!VK_GenericSpecializationAuthor(&f->specs,&f->vsWord,f->fsWords))return qfalse;
	f->stages[0].sType=f->stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	f->stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;f->stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
	f->stages[0].module=(VkShaderModule)(uintptr_t)1;f->stages[1].module=(VkShaderModule)(uintptr_t)2;
	f->stages[0].pName=f->stages[1].pName="main";
	f->stages[0].pSpecializationInfo=&f->specs.vertexInfo;
	f->stages[1].pSpecializationInfo=&f->specs.fragmentInfo;
	f->binding=(VkVertexInputBindingDescription){0,16,VK_VERTEX_INPUT_RATE_VERTEX};
	f->vi.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	f->vi.vertexBindingDescriptionCount=1;f->vi.pVertexBindingDescriptions=&f->binding;
	f->ia.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	f->ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	f->vp.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	f->vp.viewportCount=f->vp.scissorCount=1;
	f->rs.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	f->rs.polygonMode=VK_POLYGON_MODE_FILL;f->rs.cullMode=VK_CULL_MODE_BACK_BIT;
	f->rs.frontFace=VK_FRONT_FACE_CLOCKWISE;f->rs.lineWidth=1.0f;
	f->ms.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	f->ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;f->ms.minSampleShading=1.0f;
	f->ds.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	f->ds.depthTestEnable=f->ds.depthWriteEnable=VK_TRUE;
	f->ds.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;f->ds.maxDepthBounds=1.0f;
	f->attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT
		|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
	f->cb.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	f->cb.logicOp=VK_LOGIC_OP_COPY;f->cb.attachmentCount=1;
	f->cb.pAttachments=&f->attachment;
	f->states[0]=VK_DYNAMIC_STATE_VIEWPORT;f->states[1]=VK_DYNAMIC_STATE_SCISSOR;
	f->dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	f->dynamic.dynamicStateCount=2;f->dynamic.pDynamicStates=f->states;
	f->gp.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	f->gp.stageCount=2;f->gp.pStages=f->stages;f->gp.pVertexInputState=&f->vi;
	f->gp.pInputAssemblyState=&f->ia;f->gp.pViewportState=&f->vp;
	f->gp.pRasterizationState=&f->rs;f->gp.pMultisampleState=&f->ms;
	f->gp.pDepthStencilState=&f->ds;f->gp.pColorBlendState=&f->cb;
	f->gp.pDynamicState=&f->dynamic;f->gp.layout=(VkPipelineLayout)(uintptr_t)3;
	f->gp.basePipelineIndex=-1;return qtrue;
}

int main(void){
	Fixture fixture;vkTemporalGenericRecipeTable_t recipes;
	vkTemporalGenericPipelineTable_t table;
	vkTemporalRecipeTableOps_t recipeOps={Alloc,Free};
	vkTemporalGenericPipelineTableMemoryOps_t memoryOps={Alloc,Free};
	vkTemporalPipelineFactoryOps_t factoryOps;
	vkTemporalRecipeBatchAuthority_t authority={1,2,3,4};
	vkTemporalGenericRecipeCaptureInput_t capture;
	vkTemporalGenericRecipeReceipt_t recipeReceipt;
	vkTemporalGenericRecipe_t recipe;vkTemporalGenericRecipeView_t view;
	vkTemporalGenericPipelineTableEnsureInput_t input;
	vkTemporalGenericPipelineReceipt_t receipt,before;
	vkTemporalPipelineLayoutOwner_t layout;
	ralPipeline_t *got[3]={0};uint32_t id;int creates;
	memset(&factoryOps,0,sizeof(factoryOps));factoryOps.create=Create;
	factoryOps.destroy=Destroy;factoryOps.drain=Drain;factoryOps.lookup=Lookup;
	CHECK(MakeFixture(&fixture));
	CHECK(VK_TemporalGenericCatalogSelect(&(vkTemporalGenericKey_t){1,VK_TEMPORAL_GENERIC_PLAIN,qfalse,qfalse},&s_entry));
	CHECK(VK_TemporalGenericCatalogKeyId(&s_entry.key,&id));
	VK_TemporalGenericRecipeTableInit(&recipes);
	CHECK(VK_TemporalGenericRecipeTablePrepare(&recipes,4,&authority,&recipeOps));
	memset(&capture,0,sizeof(capture));capture.slot=1;capture.catalogId=id;
	capture.key=s_entry.key;capture.base=&fixture.gp;
	capture.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;
	capture.depthFormat=RAL_FORMAT_D32_SFLOAT;
	capture.layoutClass=VK_TEMPORAL_RECIPE_LAYOUT_GENERIC_MAIN;
	CHECK(VK_TemporalGenericRecipeTableCapture(&recipes,&capture,&recipeReceipt));
	CHECK(VK_TemporalGenericRecipeTableGet(&recipes,recipeReceipt.ownerEpoch,1,
		recipeReceipt.entryGeneration,&recipe));
	CHECK(VK_TemporalGenericRecipeBuildView(&recipe,(VkShaderModule)(uintptr_t)1,
		(VkShaderModule)(uintptr_t)2,(VkPipelineLayout)(uintptr_t)3,&view));
	memset(&layout,0,sizeof(layout));layout.backend=(ralBackend_t*)(uintptr_t)7;
	layout.adopted=(ralPipelineLayout_t*)(uintptr_t)8;layout.allocationGeneration=1;
	layout.borrowedSets[0]=(VkDescriptorSetLayout)(uintptr_t)10;
	layout.borrowedSets[1]=(VkDescriptorSetLayout)(uintptr_t)11;
	layout.borrowedSets[2]=(VkDescriptorSetLayout)(uintptr_t)12;
	layout.payloadLayout=(ralBindGroupLayout_t*)(uintptr_t)13;
	layout.ready=qtrue;
	VK_TemporalGenericPipelineTableInit(&table);
	CHECK(VK_TemporalGenericPipelineTablePrepare(&table,4,&layout,&memoryOps));
	CHECK(layout.leases==1&&VK_TemporalGenericPipelineTableHasLive(&table));
	memset(&input,0,sizeof(input));input.slot=1;input.recipe=&recipe;
	input.recipeReceipt=&recipeReceipt;input.base=&view.gp;
	input.ordinaryPipeline=(ralPipeline_t*)(uintptr_t)9;
	input.topologyGeneration=5;input.moduleRegistryGeneration=6;
	CHECK(VK_TemporalGenericPipelineTableEnsureSlot(&table,&input,&factoryOps,&receipt));
	CHECK(s_creates==3&&VK_TemporalGenericPipelineTableSlotReady(&table,1));
	before=(vkTemporalGenericPipelineReceipt_t){0};
	CHECK(VK_TemporalGenericPipelineTableGetSlotReceipt(&table,1,&before));
	CHECK(memcmp(&before,&receipt,sizeof(receipt))==0);
	CHECK(VK_TemporalGenericPipelineTableGet(&table,&receipt,got));
	CHECK(got[0]&&got[1]&&got[2]&&got[0]!=got[1]&&got[0]!=got[2]&&got[1]!=got[2]);
	creates=s_creates;CHECK(VK_TemporalGenericPipelineTableEnsureSlot(&table,&input,&factoryOps,&receipt));
	CHECK(s_creates==creates);
	before=receipt;input.moduleRegistryGeneration++;CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input,&factoryOps,&receipt));
	CHECK(memcmp(&receipt,&before,sizeof(receipt))==0);input.moduleRegistryGeneration--;
	// Externally-owned aliases are rejected without routing them through destroy.
	// Cover the current ordinary, another slot's ordinary, and another slot's
	// live exact3 child independently while keeping slot 1's receipt valid.
	{
		vkTemporalGenericRecipeCaptureInput_t capture0=capture;
		vkTemporalGenericRecipeReceipt_t recipeReceipt0;
		vkTemporalGenericRecipe_t recipe0;vkTemporalGenericRecipeView_t view0;
		vkTemporalGenericPipelineTableEnsureInput_t input0=input;
		vkTemporalGenericPipelineReceipt_t rejected={77};ralPipeline_t *still[3]={0};
		capture0.slot=0;
		CHECK(VK_TemporalGenericRecipeTableCapture(&recipes,&capture0,&recipeReceipt0));
		CHECK(VK_TemporalGenericRecipeTableGet(&recipes,recipeReceipt0.ownerEpoch,0,
			recipeReceipt0.entryGeneration,&recipe0));
		CHECK(VK_TemporalGenericRecipeBuildView(&recipe0,(VkShaderModule)(uintptr_t)1,
			(VkShaderModule)(uintptr_t)2,(VkPipelineLayout)(uintptr_t)3,&view0));
		input0.slot=0;input0.recipe=&recipe0;input0.recipeReceipt=&recipeReceipt0;
		input0.base=&view0.gp;input0.ordinaryPipeline=(ralPipeline_t*)(uintptr_t)19;
		creates=s_creates;s_destroys=0;
		s_forced=input0.ordinaryPipeline;
		CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input0,&factoryOps,&rejected));
		CHECK(s_creates==creates+1&&s_destroys==0&&rejected.tableGeneration==77);
		s_forced=input.ordinaryPipeline;creates=s_creates;
		CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input0,&factoryOps,&rejected));
		CHECK(s_creates==creates+1&&s_destroys==0);
		s_forced=got[0];creates=s_creates;
		CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input0,&factoryOps,&rejected));
		CHECK(s_creates==creates+1&&s_destroys==0);
		input0.retainedOrdinaryPipeline=(ralPipeline_t*)(uintptr_t)29;
		s_forced=input0.retainedOrdinaryPipeline;creates=s_creates;
		CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input0,&factoryOps,&rejected));
		CHECK(s_creates==creates+1&&s_destroys==0);
		s_forced=NULL;
		CHECK(VK_TemporalGenericPipelineTableGet(&table,&receipt,still));
		CHECK(still[0]==got[0]&&still[1]==got[1]&&still[2]==got[2]);
	}
	// A candidate aliasing another role is destroyed once and never published.
	CHECK(VK_TemporalGenericPipelineTableReleaseRangeAfterIdle(&table,1,2,
		layout.backend,&factoryOps));CHECK(!VK_TemporalGenericPipelineTableSlotReady(&table,1));
	s_alias=1;s_first=NULL;s_creates=0;s_destroys=0;
	CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input,&factoryOps,&receipt));
	CHECK(!VK_TemporalGenericPipelineTableSlotReady(&table,1)&&s_destroys==1);
	s_alias=0;s_first=NULL;s_creates=0;s_destroys=0;s_failAt=2;
	CHECK(!VK_TemporalGenericPipelineTableEnsureSlot(&table,&input,&factoryOps,&receipt));
	CHECK(!VK_TemporalGenericPipelineTableSlotReady(&table,1)&&s_destroys==1);
	s_failAt=0;
	s_first=NULL;s_creates=0;CHECK(VK_TemporalGenericPipelineTableEnsureSlot(
		&table,&input,&factoryOps,&receipt));
	s_drainFails=1;CHECK(!VK_TemporalGenericPipelineTableReleaseRangeAfterIdle(
		&table,1,2,layout.backend,&factoryOps));
	CHECK(!VK_TemporalGenericPipelineTableSlotReady(&table,1));
	memset(&before,0,sizeof(before));before.tableGeneration=91;
	CHECK(!VK_TemporalGenericPipelineTableGetSlotReceipt(&table,1,&before));
	CHECK(before.tableGeneration==91);
	CHECK(VK_TemporalGenericPipelineTableReleaseRangeAfterIdle(
		&table,1,2,layout.backend,&factoryOps));
	CHECK(VK_TemporalGenericPipelineTableReleaseAfterIdle(&table,layout.backend,
		&factoryOps,&memoryOps));CHECK(layout.leases==0&&!table.slots&&s_drains>=2);
	// The product cached-MAIN transaction is executable, not merely source-policy
	// prose: success retires old only after capture+exact3; every failure restores
	// ordinary publication/depth/count and declines without retry authority.
	{
		vkTemporalCachedMainRebuildOps_t rops;
		RebuildFixture rf;ralPipeline_t *published=(ralPipeline_t*)(uintptr_t)50;
		qboolean depthFade=qtrue,deferred=qfalse;int32_t count=7;
		memset(&rops,0,sizeof(rops));rops.createAndCapture=RebuildCreate;
		rops.captureState=RebuildState;rops.materialize=RebuildMaterialize;
		rops.destroy=RebuildDestroy;rops.decline=RebuildDecline;
		memset(&rf,0,sizeof(rf));rf.published=&published;rf.depthFade=&depthFade;
		rf.createCount=&count;rf.candidate=(ralPipeline_t*)(uintptr_t)60;
		rf.expectedRetained=published;rf.result=VK_TEMPORAL_CACHED_MAIN_READY;
		rf.captureCallOk=rf.attempted=rf.valid=qtrue;s_rebuild=&rf;
		CHECK(VK_TemporalCachedMainRebuild(&published,&depthFade,&count,&rf,&rops,&deferred));
		CHECK(published==rf.candidate&&!depthFade&&count==7&&deferred&&rf.destroys==1);
		CHECK(rf.destroyed[0]==rf.expectedRetained&&rf.events[0]==1&&rf.events[1]==2
			&&rf.events[2]==3&&rf.events[3]==4);
		published=(ralPipeline_t*)(uintptr_t)50;depthFade=qtrue;count=7;deferred=qfalse;
		memset(rf.events,0,sizeof(rf.events));rf.eventCount=rf.destroys=0;
		rf.candidate=(ralPipeline_t*)(uintptr_t)61;rf.expectedRetained=published;
		rf.result=VK_TEMPORAL_CACHED_MAIN_REJECTED;
		CHECK(!VK_TemporalCachedMainRebuild(&published,&depthFade,&count,&rf,&rops,&deferred));
		CHECK(published==rf.expectedRetained&&depthFade&&count==7&&deferred&&rf.destroys==1);
		CHECK(rf.destroyed[0]==rf.candidate&&rf.events[3]==4&&rf.events[4]==5);
		published=(ralPipeline_t*)(uintptr_t)50;depthFade=qtrue;count=7;deferred=qtrue;
		memset(rf.events,0,sizeof(rf.events));rf.eventCount=rf.destroys=0;
		rf.candidate=NULL;rf.expectedRetained=published;
		CHECK(!VK_TemporalCachedMainRebuild(&published,&depthFade,&count,&rf,&rops,&deferred));
		CHECK(published==rf.expectedRetained&&depthFade&&count==7&&!deferred&&!rf.destroys);
		published=(ralPipeline_t*)(uintptr_t)50;depthFade=qtrue;count=7;deferred=qfalse;
		memset(rf.events,0,sizeof(rf.events));rf.eventCount=rf.destroys=0;
		rf.candidate=(ralPipeline_t*)(uintptr_t)62;rf.expectedRetained=published;
		rf.result=VK_TEMPORAL_CACHED_MAIN_DEFERRED_FAILURE;
		CHECK(!VK_TemporalCachedMainRebuild(&published,&depthFade,&count,&rf,&rops,&deferred));
		CHECK(published==rf.expectedRetained&&depthFade&&count==7&&deferred&&rf.destroys==1);
	}
	CHECK(VK_TemporalGenericRecipeTableRelease(&recipes,&recipeOps));
	CHECK(s_allocs==s_frees);
	puts("vk temporal generic pipeline table contract: PASS");return 0;
}
