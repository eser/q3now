// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/renderervk/vk_temporal_iqm_exact3_factory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed: %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)

static uintptr_t s_next = 0x1000u;
static unsigned char s_payloadMemory[TEMPORAL_IQM_SLOT_BYTES];
static int s_payloadFailAt, s_payloadCreates;
static int s_factoryFailAt, s_factoryCreates, s_drainFail;
static vkTemporalIqmExact3CandidateRole_t s_rejectRole;
static vkTemporalIqmExact3CandidateRole_t s_aliasRole;
static const void *s_aliasIdentity;
static int s_destroyOrder[64], s_destroyCount;
static VkDescriptorSetLayout s_rawSets[2];
static VkPushConstantRange s_rawPush;
static ralGraphicsPipelineCreateInfo_t s_pipelineCi[2];
static ralVertexBinding_t s_pipelineBinding[2];
static ralVertexAttribute_t s_pipelineAttributes[2][6];
static ralColorBlendAttachment_t s_pipelineBlends[2][3];
static const ralBindGroupLayout_t *s_pipelineSets[2][2];

static void *Next( void ) { s_next += 0x100u; return (void *)s_next; }

ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *backend,
		const ralBindGroupLayoutCreateInfo_t *ci ) {
	(void)backend; (void)ci;
	if ( ++s_payloadCreates == s_payloadFailAt ) return NULL;
	return (ralBindGroupLayout_t *)Next();
}
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout ) {
	(void)layout; s_destroyOrder[s_destroyCount++] = 40;
}
ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *ci ) {
	(void)backend; (void)ci;
	if ( ++s_payloadCreates == s_payloadFailAt ) return NULL;
	return (ralBuffer_t *)Next();
}
void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	(void)buffer; s_destroyOrder[s_destroyCount++] = 30;
}
void *Ral_MapBuffer( ralBuffer_t *buffer ) {
	(void)buffer;
	if ( ++s_payloadCreates == s_payloadFailAt ) return NULL;
	return s_payloadMemory;
}
void Ral_UnmapBuffer( ralBuffer_t *buffer ) {
	(void)buffer; s_destroyOrder[s_destroyCount++] = 31;
}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *backend,
		const ralBindGroupCreateInfo_t *ci ) {
	(void)backend; (void)ci;
	if ( ++s_payloadCreates == s_payloadFailAt ) return NULL;
	return (ralBindGroup_t *)Next();
}
void Ral_DestroyBindGroup( ralBindGroup_t *group ) {
	(void)group; s_destroyOrder[s_destroyCount++] = 20;
}

static void *GetLayoutHandle( const ralBindGroupLayout_t *layout ) {
	return (void *)((uintptr_t)layout + 1u);
}

static void *FactoryCandidate( vkTemporalIqmExact3CandidateRole_t role ) {
	if ( s_aliasRole == role ) return (void *)s_aliasIdentity;
	if ( ++s_factoryCreates == s_factoryFailAt ) return NULL;
	return Next();
}

static VkResult CreateRaw( VkDevice device,
		const VkPipelineLayoutCreateInfo *ci, VkPipelineLayout *out ) {
	(void)device;
	CHECK(ci&&out&&ci->setLayoutCount==2u&&ci->pushConstantRangeCount==1u);
	s_rawSets[0]=ci->pSetLayouts[0];s_rawSets[1]=ci->pSetLayouts[1];s_rawPush=ci->pPushConstantRanges[0];
	*out=(VkPipelineLayout)FactoryCandidate(VK_TEMPORAL_IQM_EXACT3_CANDIDATE_RAW_LAYOUT);
	return *out ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}
static void DestroyRaw( VkDevice device, VkPipelineLayout layout ) {
	(void)device;(void)layout;s_destroyOrder[s_destroyCount++]=4;
}
static ralPipelineLayout_t *AdoptRaw( ralBackend_t *backend, void *raw,
		const char *name ) {
	(void)backend;(void)raw;(void)name;
	return (ralPipelineLayout_t *)FactoryCandidate(
		VK_TEMPORAL_IQM_EXACT3_CANDIDATE_ADOPTED_LAYOUT);
}
static void DestroyAdopted( ralPipelineLayout_t *layout ) {
	(void)layout;s_destroyOrder[s_destroyCount++]=3;
}
static ralPipeline_t *CreatePipeline( ralBackend_t *backend,
		const ralGraphicsPipelineCreateInfo_t *ci ) {
	int index=s_pipelineCi[0].vertexSpirv?1:0;
	(void)backend;CHECK(index<2&&ci);
	s_pipelineCi[index]=*ci;s_pipelineBinding[index]=ci->vertexBindings[0];
	memcpy(s_pipelineAttributes[index],ci->vertexAttributes,sizeof(s_pipelineAttributes[index]));
	memcpy(s_pipelineBlends[index],ci->colorBlends,sizeof(s_pipelineBlends[index]));
	s_pipelineSets[index][0]=ci->bindGroupLayouts[0];s_pipelineSets[index][1]=ci->bindGroupLayouts[1];
	s_pipelineCi[index].vertexBindings=&s_pipelineBinding[index];
	s_pipelineCi[index].vertexAttributes=s_pipelineAttributes[index];
	s_pipelineCi[index].colorBlends=s_pipelineBlends[index];
	s_pipelineCi[index].bindGroupLayouts=s_pipelineSets[index];
	return (ralPipeline_t *)FactoryCandidate(index
		? VK_TEMPORAL_IQM_EXACT3_CANDIDATE_INVALIDATE_PIPELINE
		: VK_TEMPORAL_IQM_EXACT3_CANDIDATE_WRITE_PIPELINE);
}
static void DestroyPipeline( ralPipeline_t *pipeline ) {
	(void)pipeline;s_destroyOrder[s_destroyCount++]=2;
}
static qboolean Drain( ralBackend_t *backend ) {
	(void)backend;s_destroyOrder[s_destroyCount++]=1;
	if(s_drainFail){s_drainFail=0;return qfalse;}return qtrue;
}
static qboolean Allowed( vkTemporalIqmExact3CandidateRole_t role,
		const void *identity, const void *context ) {
	(void)context;
	return role==s_rejectRole || (s_aliasIdentity && identity==s_aliasIdentity)
		? qfalse:qtrue;
}

static const vkTemporalIqmExact3FactoryOps_t s_ops={
	GetLayoutHandle,CreateRaw,DestroyRaw,AdoptRaw,DestroyAdopted,
	CreatePipeline,DestroyPipeline,Drain,Allowed,NULL
};

static void ResetFactoryFakes( void ) {
	s_factoryFailAt=0;s_factoryCreates=0;s_rejectRole=0;s_aliasRole=0;
	s_aliasIdentity=NULL;s_drainFail=0;s_destroyCount=0;
	memset(s_destroyOrder,0,sizeof(s_destroyOrder));
	memset(s_pipelineCi,0,sizeof(s_pipelineCi));
	memset(s_pipelineBinding,0,sizeof(s_pipelineBinding));
	memset(s_pipelineAttributes,0,sizeof(s_pipelineAttributes));
	memset(s_pipelineBlends,0,sizeof(s_pipelineBlends));
	memset(s_pipelineSets,0,sizeof(s_pipelineSets));
}

static void MakePayload( vkTemporalIqmPayloadOwner_t *payload ) {
	vkTemporalIqmPayloadKey_t key;
	memset(&key,0,sizeof(key));key.backend=(ralBackend_t *)0x10;
	key.frameCount=2u;key.maxStorageBufferRange=TEMPORAL_IQM_SLOT_BYTES;
	VK_TemporalIqmPayloadInit(payload);s_payloadFailAt=0;s_payloadCreates=0;
	CHECK(VK_TemporalIqmPayloadPrepareAfterFence(payload,&key,0u,qfalse,qfalse));
}

static vkTemporalIqmExact3FactoryInput_t Input(
		const vkTemporalIqmPayloadOwner_t *payload ) {
	vkTemporalIqmExact3FactoryInput_t input;
	memset(&input,0,sizeof(input));input.backend=payload->key.backend;
	input.device=(VkDevice)0x20;input.bindless.backend=input.backend;
	input.bindless.layout=(ralBindGroupLayout_t *)0x30;
	input.bindless.setIdentity=(const void *)0x40;input.bindless.setGeneration=5u;
	input.bindless.ready=qtrue;
	input.pipelineGeneration=7u;input.topologyGeneration=8u;
	input.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;
	input.depthFormat=RAL_FORMAT_D32_SFLOAT;input.reversedDepth=qtrue;
	return input;
}

static vkTemporalIqmExact3ShaderCatalog_t Catalog( void ) {
	static const uint32_t v[]={1,2},w[]={3,4},i[]={5,6};
	vkTemporalIqmExact3ShaderCatalog_t c;
	memset(&c,0,sizeof(c));c.vertex.bytes=(const unsigned char *)v;c.vertex.size=sizeof(v);
	c.writeFragment.bytes=(const unsigned char *)w;c.writeFragment.size=sizeof(w);
	c.invalidateFragment.bytes=(const unsigned char *)i;c.invalidateFragment.size=sizeof(i);
	c.generation=9u;return c;
}

static void CheckPipeline( const ralGraphicsPipelineCreateInfo_t *ci,
		const vkTemporalIqmExact3FactoryInput_t *input,
		const vkTemporalIqmExact3ShaderCatalog_t *catalog, qboolean invalidate,
		const vkTemporalIqmPayloadOwner_t *payload ) {
	CHECK(ci->vertexSpirv==(const uint32_t *)catalog->vertex.bytes&&ci->vertexSpirvSize==catalog->vertex.size);
	CHECK(ci->fragmentSpirv==(const uint32_t *)(invalidate?catalog->invalidateFragment.bytes:catalog->writeFragment.bytes));
	CHECK(ci->vertexBindings&&ci->numVertexBindings==1u&&ci->vertexBindings[0].stride==68u);
	CHECK(ci->vertexBindings[0].binding==0u&&ci->vertexBindings[0].inputRate==RAL_VERTEX_INPUT_PER_VERTEX&&ci->numVertexAttributes==6u);
	{const uint32_t offsets[]={0,12,24,32,48,64};const ralFormat_t formats[]={RAL_FORMAT_R32G32B32_SFLOAT,RAL_FORMAT_R32G32B32_SFLOAT,RAL_FORMAT_R32G32_SFLOAT,RAL_FORMAT_R32G32B32A32_SFLOAT,RAL_FORMAT_R32G32B32A32_SFLOAT,RAL_FORMAT_R8G8B8A8_UINT};for(uint32_t n=0;n<6;n++)CHECK(ci->vertexAttributes[n].location==n&&ci->vertexAttributes[n].binding==0u&&ci->vertexAttributes[n].offset==offsets[n]&&ci->vertexAttributes[n].format==formats[n]);}
	CHECK(ci->topology==RAL_TOPOLOGY_TRIANGLE_LIST&&ci->raster.polygonMode==RAL_POLYGON_FILL&&ci->raster.cullMode==RAL_CULL_FRONT);
	CHECK(ci->raster.frontFace==RAL_FRONT_FACE_CW&&ci->depthStencil.depthTestEnable&&ci->depthStencil.depthWriteEnable);
	CHECK(ci->raster.lineWidth==1.0f&&!ci->raster.depthBiasEnable&&!ci->raster.depthClampEnable);
	CHECK(ci->depthStencil.depthCompareOp==RAL_COMPARE_GREATER_EQUAL&&!ci->depthStencil.stencilTestEnable&&ci->numColorFormats==3u&&ci->sampleCount==1u&&ci->depthFormat==input->depthFormat);
	CHECK(ci->colorFormats[0]==input->sceneFormat&&ci->colorFormats[1]==RAL_FORMAT_R16G16_SFLOAT&&ci->colorFormats[2]==RAL_FORMAT_R8_UNORM);
	CHECK(ci->numColorBlends==3u&&ci->colorBlends[0].writeMask==RAL_COLOR_WRITE_ALL);
	CHECK(ci->colorBlends[1].writeMask==(RAL_COLOR_WRITE_R|RAL_COLOR_WRITE_G)&&ci->colorBlends[2].writeMask==RAL_COLOR_WRITE_R);
	for(uint32_t n=0;n<3;n++)CHECK(!ci->colorBlends[n].blendEnable&&ci->colorBlends[n].writeMaskExplicit);
	CHECK(ci->bindGroupLayouts[0]==payload->layout&&ci->bindGroupLayouts[1]==input->bindless.layout&&ci->numBindGroupLayouts==2u);
	CHECK(ci->pushConstantSize==8u&&ci->pushConstantStages==RAL_STAGE_FRAGMENT&&ci->externalLayout);
}

static void MutateReceipt( const vkTemporalIqmExact3FactoryReceipt_t *receipt ) {
	vkTemporalIqmExact3FactoryReceipt_t m;
#define BAD(field,value) do{m=*receipt;m.field=(value);CHECK(!VK_TemporalIqmExact3FactoryReceiptExact(receipt,&m));}while(0)
	BAD(backend,NULL);BAD(device,VK_NULL_HANDLE);BAD(payloadOwner,NULL);
	BAD(payloadOwner,(vkTemporalIqmPayloadOwner_t *)0xdead);
	BAD(payloadLayout,NULL);BAD(payloadRawLayout,NULL);BAD(bindlessRawLayout,NULL);
	BAD(payloadLayoutGeneration,0u);BAD(bindless.backend,NULL);
	BAD(bindless.layout,NULL);BAD(bindless.setIdentity,NULL);BAD(bindless.setGeneration,0u);
	BAD(bindless.ready,qfalse);BAD(rawLayout,VK_NULL_HANDLE);
	BAD(adoptedLayout,NULL);BAD(writePipeline,NULL);BAD(invalidatePipeline,NULL);
	BAD(shaderGeneration,0u);BAD(pipelineGeneration,0u);BAD(topologyGeneration,0u);
	BAD(allocationGeneration,0u);BAD(sceneFormat,RAL_FORMAT_R8G8B8A8_UNORM);
	BAD(depthFormat,RAL_FORMAT_UNDEFINED);BAD(reversedDepth,qfalse);BAD(ready,qfalse);
#undef BAD
	memset(&m,0,sizeof(m));CHECK(!VK_TemporalIqmExact3FactoryReceiptExact(&m,&m));
#define SELF_BAD(stmt) do{m=*receipt;stmt;CHECK(!VK_TemporalIqmExact3FactoryReceiptExact(&m,&m));}while(0)
	SELF_BAD(m.payloadRawLayout=m.bindlessRawLayout);
	SELF_BAD(m.bindless.backend=(ralBackend_t *)0xbeef);
	SELF_BAD(m.payloadOwner=(vkTemporalIqmPayloadOwner_t *)m.backend);
	SELF_BAD(m.payloadLayout=(ralBindGroupLayout_t *)m.backend);
	SELF_BAD(m.bindless.layout=(ralBindGroupLayout_t *)m.payloadLayout);
	SELF_BAD(m.bindless.setIdentity=m.backend);
	SELF_BAD(m.bindless.setIdentity=m.bindless.layout);
	SELF_BAD(m.adoptedLayout=(ralPipelineLayout_t *)m.payloadLayout);
	SELF_BAD(m.adoptedLayout=(ralPipelineLayout_t *)m.bindless.setIdentity);
	SELF_BAD(m.writePipeline=(ralPipeline_t *)m.adoptedLayout);
	SELF_BAD(m.invalidatePipeline=m.writePipeline);
	SELF_BAD(m.payloadLayoutGeneration=UINT32_MAX);
	SELF_BAD(m.shaderGeneration=UINT32_MAX);
	SELF_BAD(m.pipelineGeneration=UINT32_MAX);
	SELF_BAD(m.topologyGeneration=UINT32_MAX);
	SELF_BAD(m.allocationGeneration=UINT32_MAX);
	SELF_BAD(m.reversedDepth=(qboolean)2);
#undef SELF_BAD
}

int main( void ) {
	vkTemporalIqmPayloadOwner_t payload;
	vkTemporalIqmExact3FactoryOwner_t owner,before;
	vkTemporalIqmExact3FactoryInput_t input;
	vkTemporalIqmExact3ShaderCatalog_t catalog;
	vkTemporalIqmExact3FactoryReceipt_t receipt;
	MakePayload(&payload);input=Input(&payload);catalog=Catalog();
	{vkTemporalIqmExact3FactoryOps_t noOracle=s_ops;noOracle.candidateAllowed=NULL;
		VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&noOracle));
		CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);}
	VK_TemporalIqmExact3FactoryInit(&owner);owner.allocationGeneration=UINT32_MAX-1u;
	before=owner;ResetFactoryFakes();CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
	CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);
	VK_TemporalIqmExact3FactoryInit(&owner);owner.rawLayout=(VkPipelineLayout)0x99;
	before=owner;ResetFactoryFakes();CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
	CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);
	CHECK(!VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops));
	{vkTemporalIqmExact3FactoryInput_t bad=input;
		VK_TemporalIqmExact3FactoryInit(&owner);bad.bindless.backend=(ralBackend_t *)0x55;before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&bad,&catalog,&s_ops));CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);
		bad=input;bad.backend=(ralBackend_t *)0x56;bad.bindless.backend=bad.backend;VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&bad,&catalog,&s_ops));CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);}
	{vkTemporalIqmExact3FactoryInput_t bad=input;
		bad.bindless.layout=(ralBindGroupLayout_t *)&payload;
		VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&bad,&catalog,&s_ops));
		CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0&&payload.layoutLeaseCount==0u);
		bad=input;bad.bindless.layout=payload.layout;
		VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&bad,&catalog,&s_ops));
		CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0&&payload.layoutLeaseCount==0u);}
	{vkTemporalIqmExact3FactoryInput_t bad=input;
		bad.bindless.setIdentity=bad.backend;
		VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&bad,&catalog,&s_ops));
		CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);
		bad=input;bad.bindless.setIdentity=bad.bindless.layout;
		VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&bad,&catalog,&s_ops));
		CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);}
	{vkTemporalIqmExact3ShaderCatalog_t bad=catalog;bad.writeFragment.bytes=catalog.vertex.bytes;
		VK_TemporalIqmExact3FactoryInit(&owner);before=owner;ResetFactoryFakes();
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&bad,&s_ops));CHECK(!memcmp(&owner,&before,sizeof(owner))&&s_factoryCreates==0);}
	VK_TemporalIqmExact3FactoryInit(&owner);ResetFactoryFakes();
	CHECK(VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
	CHECK(owner.ready&&owner.payloadLease&&payload.layoutLeaseCount==1u&&s_factoryCreates==4);
	CHECK(s_rawSets[0]==(VkDescriptorSetLayout)GetLayoutHandle(payload.layout));
	CHECK(s_rawSets[1]==(VkDescriptorSetLayout)GetLayoutHandle(input.bindless.layout));
	CHECK(s_rawPush.offset==0u&&s_rawPush.size==8u&&s_rawPush.stageFlags==VK_SHADER_STAGE_FRAGMENT_BIT);
	CheckPipeline(&s_pipelineCi[0],&input,&catalog,qfalse,&payload);
	CheckPipeline(&s_pipelineCi[1],&input,&catalog,qtrue,&payload);
	CHECK(VK_TemporalIqmExact3FactoryGetReceipt(&owner,&receipt));
	CHECK(VK_TemporalIqmExact3FactoryReceiptExact(&receipt,&receipt));MutateReceipt(&receipt);
	{int creates=s_factoryCreates;CHECK(VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));CHECK(s_factoryCreates==creates);}
	before=owner;input.bindless.setGeneration++;
	CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
	CHECK(memcmp(&owner,&before,sizeof(owner))==0);input.bindless.setGeneration--;
	CHECK(!VK_TemporalIqmPayloadReleaseAfterIdle(&payload,qtrue));
	s_drainFail=1;CHECK(!VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops));
	CHECK(owner.pendingDrain&&!owner.ready&&payload.layoutLeaseCount==1u);
	CHECK(VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops));
	CHECK(payload.layoutLeaseCount==0u&&s_destroyCount>=6);
	CHECK(s_destroyOrder[0]==2&&s_destroyOrder[1]==2&&s_destroyOrder[2]==1&&s_destroyOrder[3]==1);
	CHECK(s_destroyOrder[4]==3&&s_destroyOrder[5]==4);

	// Every create leg fails without publishing; a post-pipeline drain failure
	// retains the exact parents/lease until retry.
	for(int fail=1;fail<=4;fail++){
		ResetFactoryFakes();VK_TemporalIqmExact3FactoryInit(&owner);s_factoryFailAt=fail;
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
		CHECK(!owner.ready&&!VK_TemporalIqmExact3FactoryGetReceipt(&owner,&receipt));
		CHECK(!owner.pendingDrain&&payload.layoutLeaseCount==0u);
	}
	ResetFactoryFakes();VK_TemporalIqmExact3FactoryInit(&owner);s_factoryFailAt=4;s_drainFail=1;
	CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
	CHECK(owner.pendingDrain&&owner.payloadLease&&payload.layoutLeaseCount==1u);
	CHECK(VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops)&&payload.layoutLeaseCount==0u);

	// Rejected/aliased candidates are borrowed: never invoke their role's
	// destructor, never double-destroy, and never publish a partial pair.
	for(int role=1;role<=4;role++){
		ResetFactoryFakes();VK_TemporalIqmExact3FactoryInit(&owner);s_rejectRole=(vkTemporalIqmExact3CandidateRole_t)role;
		CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));CHECK(!owner.ready);
		if(owner.pendingDrain)CHECK(VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops));
		CHECK(payload.layoutLeaseCount==0u);
	}
	{const void *aliases[]={payload.key.backend,payload.layout,input.bindless.layout,
		input.bindless.setIdentity};
		for(size_t a=0;a<sizeof(aliases)/sizeof(aliases[0]);a++)for(int role=2;role<=4;role++){
			ResetFactoryFakes();VK_TemporalIqmExact3FactoryInit(&owner);s_aliasRole=(vkTemporalIqmExact3CandidateRole_t)role;s_aliasIdentity=aliases[a];
			CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
			if(owner.pendingDrain)CHECK(VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops));CHECK(payload.layoutLeaseCount==0u);
		}}
	ResetFactoryFakes();VK_TemporalIqmExact3FactoryInit(&owner);
	s_aliasRole=VK_TEMPORAL_IQM_EXACT3_CANDIDATE_INVALIDATE_PIPELINE;
	// The deterministic fake returns the WRITE identity at this point.
	s_aliasIdentity=(const void *)(s_next+0x300u);
	CHECK(!VK_TemporalIqmExact3FactoryEnsure(&owner,&payload,&input,&catalog,&s_ops));
	if(owner.pendingDrain)CHECK(VK_TemporalIqmExact3FactoryRelease(&owner,&s_ops));

	CHECK(VK_TemporalIqmPayloadReleaseAfterIdle(&payload,qtrue));
	puts("vk temporal IQM exact3 factory contract: PASS");return 0;
}
