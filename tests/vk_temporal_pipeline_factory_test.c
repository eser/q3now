// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_pipeline_factory.h"
#include "vk_generic_specialization_contract.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

static uintptr_t s_nextRaw=100, s_nextAdopt=200, s_nextPipe=300;
static int s_failRaw, s_failAdopt, s_failPipeAt, s_createPipeCount, s_destroyCount;
static int s_destroyOrder[128], s_destroyOrderCount, s_drainFails;
static uint32_t s_lastPushCount,s_lastPushOffset,s_lastPushSize,s_lastPushStages;
static const ralBindGroupLayout_t *s_expectedLayouts[4];
static vkTemporalPipelineBlobCatalog_t s_catalog;
static ralPipelineLayout_t *s_genericLayout,*s_iqmLayout;
static const VkPipelineInputAssemblyStateCreateInfo *s_expectedIa;
static const VkPipelineRasterizationStateCreateInfo *s_expectedRs;
static const VkPipelineDepthStencilStateCreateInfo *s_expectedDs;
static const VkGraphicsPipelineCreateInfo *s_genericBase,*s_iqmBase;
static int s_splitMode;
static vkTemporalGenericCatalogEntry_t s_splitEntry;
static vkTemporalGenericCatalogEntry_t s_splitLookupEntry;
static vkTemporalIqmPipelineCatalog_t s_iqmCatalog;
static const unsigned char b0[4]={0},b1[8]={1},b2[12]={2},b3[16]={3},b4[20]={4},b5[24]={5},b6[28]={6},b7[32]={7};
#define VK_TEMPORAL_BLOB(name, size) const unsigned char name[size] = { 0 };
#define VK_TEMPORAL_PAIR(tx, family, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS)
#include "../code/renderervk/shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB

static ralPipelineLayout_t *FakeCreateLayout(ralBackend_t*b,const ralPipelineLayoutCreateInfo_t *ci){
	uint32_t i;(void)b;if(s_failRaw)return NULL;
	if(!ci||ci->numBindGroupLayouts!=4u||!ci->bindGroupLayouts)return NULL;
	for(i=0;i<4u;i++)if(ci->bindGroupLayouts[i]!=s_expectedLayouts[i])return NULL;
	s_lastPushCount=ci->pushConstantSize?1u:0u;
	s_lastPushOffset=ci->pushConstantOffset;s_lastPushSize=ci->pushConstantSize;
	s_lastPushStages=ci->pushConstantStages;
	s_nextRaw++;return(ralPipelineLayout_t*)(++s_nextAdopt);
}
static void *FakeGetLayoutHandle(const ralPipelineLayout_t *p){
	(void)p;return s_failAdopt?NULL:(void*)s_nextRaw;
}
static void FakeDestroyLayout(ralPipelineLayout_t*p){s_destroyOrder[s_destroyOrderCount++]=(int)(uintptr_t)p;}
static qboolean FakeDrain(ralBackend_t*b){(void)b;if(s_drainFails){s_drainFails--;return qfalse;}return qtrue;}
static qboolean FakeLookup(VkShaderModule m,vkTemporalShaderBlob_t*out){
	if(!out)return qfalse;
	if(s_splitMode){
		const vkTemporalGenericCatalogEntry_t *lookup=s_splitLookupEntry.ordinaryVertex.bytes?&s_splitLookupEntry:&s_splitEntry;
		if((uintptr_t)m==51)*out=lookup->ordinaryVertex;
		else if((uintptr_t)m==52)*out=lookup->ordinaryFragment;
		else if((uintptr_t)m==61)*out=s_iqmCatalog.ordinaryVertex;
		else if((uintptr_t)m==62)*out=s_iqmCatalog.ordinaryFragment;
		else return qfalse;return qtrue;
	}
	if((uintptr_t)m==51)*out=s_catalog.ordinaryVertex;
	else if((uintptr_t)m==52)*out=s_catalog.ordinaryFragment;
	else if((uintptr_t)m==61)*out=s_catalog.iqmVertex;
	else if((uintptr_t)m==62)*out=s_catalog.iqmOrdinaryFragment;
	else return qfalse;
	return qtrue;
}
static void FakeDestroyPipe(ralPipeline_t*p){s_destroyOrder[s_destroyOrderCount++]=(int)(uintptr_t)p;s_destroyCount++;}
static ralPipeline_t *FakeCreatePipe(const VkGraphicsPipelineCreateInfo *ci,ralPipelineLayout_t*l,
	const ralFormat_t*f,uint32_t n,ralFormat_t d,const vkTemporalSpirvOverrides_t*o,const char*name){
	const VkPipelineColorBlendAttachmentState *a; (void)d;(void)name;
	const VkGraphicsPipelineCreateInfo *base;
	s_createPipeCount++; if(s_failPipeAt==s_createPipeCount)return NULL;
	if(s_splitMode){
		if(s_createPipeCount<=3){if(l!=s_genericLayout)return NULL;base=s_genericBase;}
		else{if(l!=s_iqmLayout)return NULL;base=s_iqmBase;}
	}else
	if((s_createPipeCount%4)==0){if(l!=s_iqmLayout)return NULL;base=s_iqmBase;}else{if(l!=s_genericLayout)return NULL;base=s_genericBase;}
	if(!ci||!o||n!=3||f[1]!=RAL_FORMAT_R16G16_SFLOAT||f[2]!=RAL_FORMAT_R8_UNORM)return NULL;
	if(ci->flags!=base->flags||ci->stageCount!=base->stageCount||ci->pStages!=base->pStages||ci->pVertexInputState!=base->pVertexInputState||ci->pInputAssemblyState!=base->pInputAssemblyState||ci->pViewportState!=base->pViewportState||ci->pRasterizationState!=base->pRasterizationState||ci->pMultisampleState!=base->pMultisampleState||ci->pDepthStencilState!=base->pDepthStencilState||ci->pDynamicState!=base->pDynamicState||ci->layout!=base->layout||ci->renderPass!=base->renderPass||ci->subpass!=base->subpass||ci->basePipelineHandle!=base->basePipelineHandle||ci->basePipelineIndex!=base->basePipelineIndex)return NULL;
	if(ci->pInputAssemblyState!=s_expectedIa||ci->pRasterizationState!=s_expectedRs||ci->pDepthStencilState!=s_expectedDs)return NULL;
	if(!o->vertex.bytes||!o->fragment.bytes||!o->vertex.size||!o->fragment.size)return NULL;
	if(s_splitMode){
		if(s_createPipeCount==1&&(o->vertex.bytes!=s_splitEntry.ordinaryVertex.bytes||o->fragment.bytes!=s_splitEntry.ordinaryFragment.bytes))return NULL;
		if(s_createPipeCount==2&&(o->vertex.bytes!=s_splitEntry.temporalVertex.bytes||o->fragment.bytes!=s_splitEntry.temporalWriteFragment.bytes))return NULL;
		if(s_createPipeCount==3&&(o->vertex.bytes!=s_splitEntry.temporalVertex.bytes||o->fragment.bytes!=s_splitEntry.temporalInvalidateFragment.bytes))return NULL;
		if(s_createPipeCount==4&&(o->vertex.bytes!=s_iqmCatalog.ordinaryVertex.bytes||o->fragment.bytes!=s_iqmCatalog.invalidateFragment.bytes))return NULL;
	}else{
		if(s_createPipeCount==1&&(o->vertex.bytes!=s_catalog.ordinaryVertex.bytes||o->vertex.size!=s_catalog.ordinaryVertex.size||o->fragment.bytes!=s_catalog.ordinaryFragment.bytes||o->fragment.size!=s_catalog.ordinaryFragment.size))return NULL;
		if(s_createPipeCount==2&&(o->vertex.bytes!=s_catalog.temporalVertex.bytes||o->fragment.bytes!=s_catalog.temporalWriteFragment.bytes))return NULL;
		if(s_createPipeCount==3&&(o->vertex.bytes!=s_catalog.temporalVertex.bytes||o->fragment.bytes!=s_catalog.temporalInvalidateFragment.bytes))return NULL;
		if(s_createPipeCount==4&&(o->vertex.bytes!=s_catalog.iqmVertex.bytes||o->fragment.bytes!=s_catalog.iqmInvalidateFragment.bytes))return NULL;
	}
	if(!ci->pColorBlendState||ci->pColorBlendState->attachmentCount!=3)return NULL;
	a=ci->pColorBlendState->pAttachments;
	if(a[0].blendEnable||a[1].blendEnable||a[2].blendEnable)return NULL;
	if(a[1].srcColorBlendFactor||a[1].dstColorBlendFactor||a[1].colorBlendOp||a[1].srcAlphaBlendFactor||a[1].dstAlphaBlendFactor||a[1].alphaBlendOp)return NULL;
	if(a[2].srcColorBlendFactor||a[2].dstColorBlendFactor||a[2].colorBlendOp||a[2].srcAlphaBlendFactor||a[2].dstAlphaBlendFactor||a[2].alphaBlendOp)return NULL;
	if((s_splitMode?s_createPipeCount==1:s_createPipeCount%4==1)){if(a[1].colorWriteMask||a[2].colorWriteMask)return NULL;}
	else if(a[1].colorWriteMask!=(VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT)||a[2].colorWriteMask!=VK_COLOR_COMPONENT_R_BIT)return NULL;
	return(ralPipeline_t*)(++s_nextPipe);
}
static vkTemporalShaderBlob_t B(const unsigned char*p,uint32_t n){vkTemporalShaderBlob_t b={p,n};return b;}

int main(void){
	vkTemporalPipelineLayoutOwner_t lo;vkTemporalPipelineFactoryOwner_t fo;
	vkTemporalLayoutOps_t lops={FakeCreateLayout,FakeGetLayoutHandle,FakeDestroyLayout};
	vkTemporalPipelineFactoryOps_t pops={FakeCreatePipe,FakeDestroyPipe,FakeDrain,FakeLookup};
	const ralBindGroupLayout_t *layouts[3]={(ralBindGroupLayout_t*)11,(ralBindGroupLayout_t*)12,(ralBindGroupLayout_t*)13};
	const ralBindGroupLayout_t *payload=(ralBindGroupLayout_t*)14;
	VkPipelineColorBlendAttachmentState att;VkPipelineColorBlendStateCreateInfo cb;
	VkPipelineShaderStageCreateInfo stages[2];vkGenericSpecializationGraph_t specGraph;
	uint32_t vertexSpecWord=1,fragmentSpecWords[VK_GENERIC_FRAGMENT_SPEC_COUNT]={0};
	floatint_t specFloat;
	VkVertexInputBindingDescription bind;VkPipelineVertexInputStateCreateInfo vi;
	VkPipelineInputAssemblyStateCreateInfo ia;VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineDepthStencilStateCreateInfo ds;VkDynamicState dyns[2];VkPipelineDynamicStateCreateInfo dyn;
	VkGraphicsPipelineCreateInfo gp,iqm;vkTemporalPipelineFactoryInput_t in;
	vkTemporalPipelineFactoryOwner_t before;uint32_t oldGen;int oldDestroy;

	memset(&att,0,sizeof(att));att.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
	memset(&cb,0,sizeof(cb));cb.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;cb.attachmentCount=1;cb.pAttachments=&att;
	memset(stages,0,sizeof(stages));stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[0].module=(VkShaderModule)51;stages[1].module=(VkShaderModule)52;stages[0].pName=stages[1].pName="main";
	specFloat.f=0.85f;fragmentSpecWords[2]=specFloat.u;
	specFloat.f=2.0f;fragmentSpecWords[11]=specFloat.u;
	CHECK(VK_GenericSpecializationAuthor(&specGraph,&vertexSpecWord,fragmentSpecWords));
	stages[0].pSpecializationInfo=&specGraph.vertexInfo;stages[1].pSpecializationInfo=&specGraph.fragmentInfo;
	memset(&bind,0,sizeof(bind));bind.stride=32;bind.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
	memset(&vi,0,sizeof(vi));vi.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&bind;
	memset(&ia,0,sizeof(ia));ia.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	memset(&vp,0,sizeof(vp));vp.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;vp.viewportCount=vp.scissorCount=1;
	memset(&rs,0,sizeof(rs));rs.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_BACK_BIT;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1.0f;
	memset(&ms,0,sizeof(ms));ms.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;ms.minSampleShading=1.0f;
	memset(&ds,0,sizeof(ds));ds.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;ds.depthTestEnable=ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
	dyns[0]=VK_DYNAMIC_STATE_VIEWPORT;dyns[1]=VK_DYNAMIC_STATE_SCISSOR;memset(&dyn,0,sizeof(dyn));dyn.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;dyn.dynamicStateCount=2;dyn.pDynamicStates=dyns;
	memset(&gp,0,sizeof(gp));gp.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;gp.stageCount=2;gp.pStages=stages;gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pDepthStencilState=&ds;gp.pColorBlendState=&cb;gp.pDynamicState=&dyn;gp.layout=(VkPipelineLayout)71;gp.renderPass=(VkRenderPass)72;gp.subpass=2;gp.basePipelineIndex=-1;iqm=gp;
	{ static VkPipelineShaderStageCreateInfo iqmStages[2]; iqmStages[0]=stages[0];iqmStages[1]=stages[1];iqmStages[0].module=(VkShaderModule)61;iqmStages[1].module=(VkShaderModule)62;iqm.pStages=iqmStages; }
	memset(&in,0,sizeof(in));in.pipelineGeneration=1;in.topologyGeneration=1;in.iqmLayoutGeneration=1;in.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;in.depthFormat=RAL_FORMAT_D32_SFLOAT;in.exactTx1=qtrue;
	memset(&s_catalog,0,sizeof(s_catalog));s_catalog.ordinaryVertex=B(b0,sizeof(b0));s_catalog.ordinaryFragment=B(b1,sizeof(b1));s_catalog.temporalVertex=B(b2,sizeof(b2));s_catalog.temporalWriteFragment=B(b3,sizeof(b3));s_catalog.temporalInvalidateFragment=B(b4,sizeof(b4));s_catalog.iqmVertex=B(b5,sizeof(b5));s_catalog.iqmOrdinaryFragment=B(b6,sizeof(b6));s_catalog.iqmInvalidateFragment=B(b7,sizeof(b7));s_catalog.generation=1;
	memcpy(s_expectedLayouts,layouts,sizeof(layouts));s_expectedLayouts[3]=payload;
	VK_TemporalPipelineLayoutInit(&lo);
	CHECK(VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,1,qfalse,&lops));
	CHECK(lo.ready&&lo.leases==0&&lo.allocationGeneration==1&&s_lastPushCount==0);
	oldGen=lo.allocationGeneration;CHECK(VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,1,qfalse,&lops));CHECK(lo.allocationGeneration==oldGen);
	VK_TemporalPipelineFactoryInit(&fo);s_createPipeCount=0;
	s_genericLayout=lo.adopted;s_iqmLayout=(ralPipelineLayout_t*)9;s_expectedIa=&ia;s_expectedRs=&rs;s_expectedDs=&ds;s_genericBase=&gp;s_iqmBase=&iqm;
	CHECK(VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));
	CHECK(fo.ready&&lo.leases==1&&fo.generic[0]&&fo.generic[1]&&fo.generic[2]&&fo.iqmInvalidate);
	// Equivalent root create-info objects at different addresses are a cache hit.
	{ VkGraphicsPipelineCreateInfo gpCopy=gp,iqmCopy=iqm;int creates=s_createPipeCount;
		CHECK(VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gpCopy,&iqmCopy,&in,&s_catalog,&pops));CHECK(s_createPipeCount==creates); }
	// Same-address provenance/spec mutations reject output-atomically before create.
	before=fo;stages[0].module=(VkShaderModule)52;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);stages[0].module=(VkShaderModule)51;
	vertexSpecWord=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);vertexSpecWord=1;
	fragmentSpecWords[0]=1;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));fragmentSpecWords[0]=0;
	fragmentSpecWords[7]=1;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));fragmentSpecWords[7]=0;
	specGraph.fragmentMaps[0].constantID=5;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));specGraph.fragmentMaps[0].constantID=0;
	specGraph.fragmentMaps[7].constantID=5;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));specGraph.fragmentMaps[7].constantID=7;
	specGraph.vertexMap[0].offset=12;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);specGraph.vertexMap[0].offset=0;
	// Supported fixed-state mutation changes the deep key (forced create failure proves miss).
	rs.cullMode=VK_CULL_MODE_FRONT_BIT;s_createPipeCount=0;s_failPipeAt=1;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);rs.cullMode=VK_CULL_MODE_BACK_BIT;s_failPipeAt=0;
	// Every catalog blob pointer and size participates field-wise in the key.
	{ vkTemporalShaderBlob_t *bf[]={&s_catalog.ordinaryVertex,&s_catalog.ordinaryFragment,&s_catalog.temporalVertex,&s_catalog.temporalWriteFragment,&s_catalog.temporalInvalidateFragment,&s_catalog.iqmVertex,&s_catalog.iqmOrdinaryFragment,&s_catalog.iqmInvalidateFragment};uint32_t k;
		for(k=0;k<8;k++){vkTemporalShaderBlob_t saved=*bf[k];s_failPipeAt=1;s_createPipeCount=0;bf[k]->size+=4;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);*bf[k]=saved;
			bf[k]->bytes=(saved.bytes==b0)?b1:b0;s_failPipeAt=1;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);*bf[k]=saved;}s_failPipeAt=0; }
	// Numeric topology/pipeline/layout allocation/IQM-layout generations are keys.
	{ uint32_t savedLo=lo.allocationGeneration;in.topologyGeneration++;s_failPipeAt=1;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));in.topologyGeneration--;
		in.pipelineGeneration++;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));in.pipelineGeneration--;
		in.iqmLayoutGeneration++;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));in.iqmLayoutGeneration--;
		s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)10,&gp,&iqm,&in,&s_catalog,&pops));
		lo.allocationGeneration++;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));lo.allocationGeneration=savedLo;s_failPipeAt=0;CHECK(memcmp(&fo,&before,sizeof(fo))==0); }
	{ vkTemporalPipelineLayoutOwner_t other=lo;s_failPipeAt=1;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&other,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));s_failPipeAt=0;CHECK(memcmp(&fo,&before,sizeof(fo))==0); }
	in.dynamicDiscard=qtrue;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));in.dynamicDiscard=qfalse;
	bind.inputRate=VK_VERTEX_INPUT_RATE_INSTANCE;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));bind.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
	ia.primitiveRestartEnable=VK_TRUE;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));ia.primitiveRestartEnable=VK_FALSE;
	ms.sampleShadingEnable=VK_TRUE;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));ms.sampleShadingEnable=VK_FALSE;
	CHECK(!VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,2,qfalse,&lops));
	before=fo;s_failPipeAt=2;s_createPipeCount=0;in.pipelineGeneration=2;
	CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);s_failPipeAt=0;
	oldDestroy=s_destroyCount;s_createPipeCount=0;s_drainFails=1;
	CHECK(VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));
	CHECK(fo.ready&&fo.retiringLease&&lo.leases==2&&s_destroyCount==oldDestroy+4);
	CHECK(!VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,2,qfalse,&lops));
	CHECK(VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(!fo.retiringLease&&lo.leases==1);
	// Exact key includes formats, layout generation, IQM layout and full blob identity.
	before=fo;in.sceneFormat=RAL_FORMAT_R8G8B8A8_UNORM;s_failPipeAt=1;s_createPipeCount=0;CHECK(!VK_TemporalPipelineFactoryEnsure(&fo,&lo,(ralPipelineLayout_t*)9,&gp,&iqm,&in,&s_catalog,&pops));CHECK(memcmp(&fo,&before,sizeof(fo))==0);in.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;s_failPipeAt=0;
	s_drainFails=1;CHECK(!VK_TemporalPipelineFactoryRelease(&fo,&pops));CHECK(fo.retiringLease&&lo.leases==1);CHECK(!VK_TemporalPipelineLayoutRelease(&lo,&lops));
	CHECK(VK_TemporalPipelineFactoryRelease(&fo,&pops));CHECK(lo.leases==0);
	// Production-facing split: one exact generic key owns exactly three siblings;
	// the IQM invalidate sibling is a separate singleton with retryable drain.
	{
		vkTemporalGenericPipelineFactoryOwner_t go;
		vkTemporalIqmPipelineFactoryOwner_t io;
		vkTemporalGenericPipelineFactoryInput_t gi;
		int creates;
		memset(&gi,0,sizeof(gi));gi.key=(vkTemporalGenericKey_t){1,VK_TEMPORAL_GENERIC_PLAIN,qfalse,qfalse};
		gi.pipelineGeneration=gi.topologyGeneration=gi.catalogGeneration=1;
		gi.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;gi.depthFormat=RAL_FORMAT_D32_SFLOAT;
		CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));
		s_iqmCatalog.ordinaryVertex=B(b5,sizeof(b5));s_iqmCatalog.ordinaryFragment=B(b6,sizeof(b6));
		s_iqmCatalog.invalidateFragment=B(b7,sizeof(b7));s_iqmCatalog.generation=1;
		s_splitMode=1;s_createPipeCount=0;s_genericLayout=lo.adopted;s_iqmLayout=(ralPipelineLayout_t*)9;
		VK_TemporalGenericPipelineFactoryInit(&go);VK_TemporalIqmPipelineFactoryInit(&io);
		CHECK(VK_TemporalPipelineLayoutAcquire(&lo)); /* table-level candidate guard */
		CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
		CHECK(go.ready&&go.pipelines[0]&&go.pipelines[1]&&go.pipelines[2]&&lo.leases==2&&s_createPipeCount==3);
		// Every production specialization admission gate rejects before any
		// candidate create and leaves the live three-pipeline owner unchanged.
#define SPLIT_REJECT_SLOT(slot,bad) do { \
		uint32_t savedWord=fragmentSpecWords[(slot)]; \
		vkTemporalGenericPipelineFactoryOwner_t liveOwner=go; \
		int createBefore=s_createPipeCount; \
		fragmentSpecWords[(slot)]=(bad); \
		CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops)); \
		CHECK(memcmp(&go,&liveOwner,sizeof(go))==0&&s_createPipeCount==createBefore); \
		fragmentSpecWords[(slot)]=savedWord; \
	} while(0)
		{ uint32_t savedWord=vertexSpecWord;vkTemporalGenericPipelineFactoryOwner_t liveOwner=go;int createBefore=s_createPipeCount;
			vertexSpecWord=0u;CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
			CHECK(memcmp(&go,&liveOwner,sizeof(go))==0&&s_createPipeCount==createBefore);vertexSpecWord=savedWord; }
		SPLIT_REJECT_SLOT(0,1u);
		SPLIT_REJECT_SLOT(1,0x80000000u);
		SPLIT_REJECT_SLOT(2,fragmentSpecWords[2]^1u);
		SPLIT_REJECT_SLOT(3,1u);
		SPLIT_REJECT_SLOT(4,4u);
		SPLIT_REJECT_SLOT(5,1u);
		SPLIT_REJECT_SLOT(6,8u);
		SPLIT_REJECT_SLOT(7,1u);
		SPLIT_REJECT_SLOT(8,0xbf800000u);
		SPLIT_REJECT_SLOT(9,0x7f800000u);
		SPLIT_REJECT_SLOT(10,1u);
		SPLIT_REJECT_SLOT(11,0u);
		SPLIT_REJECT_SLOT(12,1u);
		SPLIT_REJECT_SLOT(13,1u);
		SPLIT_REJECT_SLOT(14,3u);
#undef SPLIT_REJECT_SLOT
		creates=s_createPipeCount;CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));CHECK(s_createPipeCount==creates);
		// Active ATEST is excluded even though every ordinary source module was
		// compiled from the historical ATEST-capable generic template.
		gi.alphaTested=qtrue;before=fo;CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));gi.alphaTested=qfalse;
		// USE_FOG shader variants read their authored UBO and are independent of
		// the build-level FEAT_FOG_SYSTEM push range. A no-push layout admits both.
		{ gi.key.shaderFog=qtrue;gi.pipelineGeneration++;fragmentSpecWords[10]=1u;
			CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));s_createPipeCount=0;
			CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
			CHECK(go.ready&&s_createPipeCount==3);gi.key.shaderFog=qfalse;fragmentSpecWords[10]=0u;
			gi.pipelineGeneration++;CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));
			s_createPipeCount=0;
			CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops)); }
		// A requested key-B cannot borrow the key-A ordinary module provenance.
		{ vkTemporalGenericPipelineFactoryOwner_t live=go;s_splitLookupEntry=go.catalog;
			gi.key=(vkTemporalGenericKey_t){2,VK_TEMPORAL_GENERIC_CL,qtrue,qfalse};
			CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));creates=s_createPipeCount;
			CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
			CHECK(memcmp(&go,&live,sizeof(go))==0&&s_createPipeCount==creates);memset(&s_splitLookupEntry,0,sizeof(s_splitLookupEntry)); }
		// Candidate failure on a different exact key preserves the live generation.
		{ vkTemporalGenericPipelineFactoryOwner_t live=go;gi.key=(vkTemporalGenericKey_t){0,VK_TEMPORAL_GENERIC_ENT,qtrue,qfalse};CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));
			s_createPipeCount=0;s_failPipeAt=2;CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
			CHECK(memcmp(&go,&live,sizeof(go))==0);s_failPipeAt=0; }
		// TX0 entity, TX1 environment and TX2 CL are separate catalog
		// identities; each replacement owns exactly three generic siblings.
		{ const vkTemporalGenericKey_t keys[]={
			{0,VK_TEMPORAL_GENERIC_ENT,qtrue,qfalse},
			{1,VK_TEMPORAL_GENERIC_PLAIN,qtrue,qfalse},
			{2,VK_TEMPORAL_GENERIC_CL,qfalse,qfalse}};uint32_t k;
			for(k=0;k<3;k++){gi.key=keys[k];gi.pipelineGeneration++;CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));
				s_createPipeCount=0;s_drainFails=(k==0);
				if(k==0)CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
				else CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
				CHECK(s_createPipeCount==3&&go.ready);if(k==0){CHECK(go.retiringLease);CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));CHECK(!go.retiringLease);}}
		}
		io.allocationGeneration=UINT32_MAX;creates=s_createPipeCount;
		CHECK(!VK_TemporalIqmPipelineFactoryEnsure(&io,(ralBackend_t*)1,(ralPipelineLayout_t*)9,1,&iqm,1,1,
			RAL_FORMAT_R16G16B16A16_SFLOAT,RAL_FORMAT_D32_SFLOAT,&s_iqmCatalog,&pops));
		CHECK(io.allocationGeneration==UINT32_MAX&&s_createPipeCount==creates);io.allocationGeneration=0;
		CHECK(VK_TemporalIqmPipelineFactoryEnsure(&io,(ralBackend_t*)1,(ralPipelineLayout_t*)9,1,&iqm,1,1,
			RAL_FORMAT_R16G16B16A16_SFLOAT,RAL_FORMAT_D32_SFLOAT,&s_iqmCatalog,&pops));
		CHECK(io.ready&&s_createPipeCount==4);
		// A failed replacement drain never reports a clean publication, and an
		// immediate disable can retry without wedging ready+retiring ownership.
		gi.pipelineGeneration++;s_createPipeCount=0;s_drainFails=1;
		CHECK(!VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));
		CHECK(go.ready&&go.retiringLease&&lo.leases==3);
		s_drainFails=1;CHECK(!VK_TemporalGenericPipelineFactoryRelease(&go,&pops));
		CHECK(go.ready&&go.retiringLease&&lo.leases==3);
		CHECK(VK_TemporalGenericPipelineFactoryRelease(&go,&pops)&&lo.leases==1);
		CHECK(VK_TemporalPipelineLayoutReleaseLease(&lo)&&lo.leases==0);
		s_drainFails=1;CHECK(!VK_TemporalIqmPipelineFactoryRelease(&io,(ralBackend_t*)1,&pops));
		CHECK(!io.ready&&io.pendingDrain);CHECK(VK_TemporalIqmPipelineFactoryRelease(&io,(ralBackend_t*)1,&pops));
		s_splitMode=0;
	}
	CHECK(VK_TemporalPipelineLayoutRelease(&lo,&lops));
	CHECK(s_destroyOrderCount>=1&&s_destroyOrder[s_destroyOrderCount-1]>200);
	// Build-level fog layout authors the exact sole FS 64/32 range.
	VK_TemporalPipelineLayoutInit(&lo);CHECK(VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,3,qtrue,&lops));
	CHECK(s_lastPushCount==1&&s_lastPushOffset==64&&s_lastPushSize==32&&s_lastPushStages==RAL_STAGE_FRAGMENT);
	{ vkTemporalGenericPipelineFactoryOwner_t go;vkTemporalGenericPipelineFactoryInput_t gi;uint32_t shaderFog;
		memset(&gi,0,sizeof(gi));gi.key=(vkTemporalGenericKey_t){2,VK_TEMPORAL_GENERIC_PLAIN,qfalse,qfalse};
		gi.pipelineGeneration=gi.topologyGeneration=gi.catalogGeneration=1;gi.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;gi.depthFormat=RAL_FORMAT_D32_SFLOAT;
		s_splitMode=1;s_genericLayout=lo.adopted;VK_TemporalGenericPipelineFactoryInit(&go);
		CHECK(VK_TemporalPipelineLayoutAcquire(&lo)); /* table-level candidate guard */
		for(shaderFog=0;shaderFog<2;shaderFog++){gi.key.shaderFog=(qboolean)shaderFog;fragmentSpecWords[10]=shaderFog;gi.pipelineGeneration++;
			CHECK(VK_TemporalGenericCatalogSelect(&gi.key,&s_splitEntry));s_createPipeCount=0;
			CHECK(VK_TemporalGenericPipelineFactoryEnsure(&go,&lo,&gp,&gi,&pops));CHECK(go.ready&&s_createPipeCount==3);}
		fragmentSpecWords[10]=0u;
		CHECK(VK_TemporalGenericPipelineFactoryRelease(&go,&pops));
		CHECK(VK_TemporalPipelineLayoutReleaseLease(&lo));s_splitMode=0; }
	CHECK(VK_TemporalPipelineLayoutRelease(&lo,&lops));
	// Layout candidate failures are output-atomic.
	VK_TemporalPipelineLayoutInit(&lo);before=fo;s_failRaw=1;CHECK(!VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,1,qfalse,&lops));CHECK(!lo.ready);s_failRaw=0;s_failAdopt=1;CHECK(!VK_TemporalPipelineLayoutEnsure(&lo,(ralBackend_t*)1,(VkDevice)2,layouts,payload,1,qfalse,&lops));CHECK(!lo.ready);s_failAdopt=0;
	puts("vk temporal pipeline factory contract: PASS");return 0;
}
