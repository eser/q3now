// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_history_store.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../code/renderercommon/vulkan/vulkan.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL Store %d: %s\n",__LINE__,#x);return 1;} }while(0)
static uintptr_t nextHandle=0x100000;
static uint32_t creates, failStep, destroys, eventCount;
static void *aliasHandle; static const void *destroyed[4096];
static uint32_t attemptStart;
static void *attemptHandles[6];
static uint32_t aliasPreviousStep = UINT32_MAX;
static struct {char k;const void *o;uint32_t a,b,c;} events[16];
static ralBindingValue_t values[2][5]; static uint32_t groupCount;
static vkTemporalHistoryStorePush_t gotPush;
static void *H(uintptr_t x){return(void*)x;}
static void *New(void){
	uint32_t step = creates - attemptStart;
	void *result;
	++creates;
	if(creates==failStep) result = aliasPreviousStep < 6
		? attemptHandles[aliasPreviousStep] : aliasHandle;
	else { nextHandle+=0x100; result=(void*)nextHandle; }
	if(step<6) attemptHandles[step]=result;
	return result;
}
static void D(const void*p){if(destroys<4096)destroyed[destroys]=p;++destroys;}
static qboolean WasDestroyed(uint32_t start,const void*p){uint32_t i;for(i=start;i<destroys&&i<4096;++i)if(destroyed[i]==p)return qtrue;return qfalse;}
ralTextureView_t*Ral_CreateTextureView(ralBackend_t*b,const ralTextureViewCreateInfo_t*c){(void)b;(void)c;return New();}
ralSampler_t*Ral_CreateSampler(ralBackend_t*b,const ralSamplerCreateInfo_t*c){(void)b;(void)c;return New();}
ralBindGroupLayout_t*Ral_CreateBindGroupLayout(ralBackend_t*b,const ralBindGroupLayoutCreateInfo_t*c){(void)b;if(!c||c->numEntries!=5)return NULL;return New();}
ralBindGroup_t*Ral_CreateBindGroup(ralBackend_t*b,const ralBindGroupCreateInfo_t*c){(void)b;if(c&&groupCount<2&&c->numValues==5)memcpy(values[groupCount++],c->values,sizeof(values[0]));return New();}
ralPipeline_t*Ral_CreateComputePipeline(ralBackend_t*b,const ralComputePipelineCreateInfo_t*c){(void)b;if(!c||c->pushConstantSize!=sizeof(vkTemporalHistoryStorePush_t))return NULL;return New();}
void Ral_DestroyTextureView(ralTextureView_t*p){D(p);}void Ral_DestroySampler(ralSampler_t*p){D(p);}void Ral_DestroyBindGroupLayout(ralBindGroupLayout_t*p){D(p);}void Ral_DestroyBindGroup(ralBindGroup_t*p){D(p);}void Ral_DestroyPipeline(ralPipeline_t*p){D(p);}
static void E(char k,const void*o,uint32_t a,uint32_t b,uint32_t c){if(eventCount<16){events[eventCount].k=k;events[eventCount].o=o;events[eventCount].a=a;events[eventCount].b=b;events[eventCount].c=c;}++eventCount;}
void Ral_CmdTransitionTexture(ralCommandBuffer_t*c,ralTexture_t*t,uint32_t a,uint32_t b,VkImageLayout l){(void)c;E('T',t,a,b,(uint32_t)l);}void Ral_CmdBindPipeline(ralCommandBuffer_t*c,ralPipeline_t*p){(void)c;E('P',p,0,0,0);}void Ral_CmdBindBindGroup(ralCommandBuffer_t*c,uint32_t s,ralBindGroup_t*g){(void)c;E('G',g,s,0,0);}void Ral_CmdPushConstants(ralCommandBuffer_t*c,uint32_t s,uint32_t o,uint32_t n,const void*d){(void)c;(void)o;if(n==sizeof(gotPush))memcpy(&gotPush,d,n);E('U',NULL,s,n,0);}void Ral_CmdDispatch(ralCommandBuffer_t*c,uint32_t x,uint32_t y,uint32_t z){(void)c;E('D',NULL,x,y,z);}

static vkTemporalHistoryStoreKey_t Key(uintptr_t base){vkTemporalHistoryStoreKey_t k;uint32_t i;memset(&k,0,sizeof(k));k.backend=H(base+1);k.worldIndex=2;k.width=1280;k.height=720;k.topologyEpoch=3;k.historyAllocationGeneration=4;k.sceneColorAttachmentGeneration=5;k.resolvedTargetAllocationGeneration=6;k.sceneFormat=RAL_FORMAT_R16G16B16A16_SFLOAT;k.feedbackColor=H(base+0x10);k.feedbackColorView=H(base+0x20);k.currentDepth=H(base+0x30);for(i=0;i<2;++i){k.historyColor[i]=H(base+0x40+i*0x40);k.historyColorView[i]=H(base+0x50+i*0x40);k.historyDepth[i]=H(base+0x60+i*0x40);k.historyDepthView[i]=H(base+0x70+i*0x40);}k.computeSpirv=(const uint32_t*)H(base+0xd0);k.computeSpirvSize=16;return k;}

int main(void) {
	vkTemporalHistoryStoreOwner_t o, before, mutated;
	vkTemporalHistoryStoreKey_t k = Key(0x1000), k2;
	vkTemporalHistoryStorePush_t p = {{1280,720},4,8192};
	const void *protectedSet[32];
	const void *keyBorrowed[13];
	uint32_t protectedCount = 0, d0, c0, i, j, role;

	VK_TemporalHistoryStoreInit(&o);
	attemptStart=creates; memset(attemptHandles,0,sizeof(attemptHandles));
	CHECK(VK_TemporalHistoryStoreEnsureAfterFence(&o,&k,qfalse));
	CHECK(o.ready&&o.allocationGeneration==1&&groupCount==2);
	CHECK(values[0][0].textureView==k.feedbackColorView
		&&values[0][1].textureView==o.currentDepthView
		&&values[0][2].sampler==o.sampler
		&&values[0][3].textureView==k.historyColorView[0]
		&&values[0][4].textureView==k.historyDepthView[0]
		&&values[1][3].textureView==k.historyColorView[1]
		&&values[1][4].textureView==k.historyDepthView[1]);
	before=o;
	CHECK(VK_TemporalHistoryStoreEnsureAfterFence(&o,&k,qfalse));
	CHECK(memcmp(&o,&before,sizeof(o))==0);

	eventCount=0;
	CHECK(VK_TemporalHistoryStoreRecord(&o,&k,o.allocationGeneration,H(0x9000),1,&p));
	CHECK(eventCount==9);
	CHECK(events[0].k=='T'&&events[0].o==k.feedbackColor
		&&events[0].a==(RAL_PIPELINE_STAGE_TRANSFER_BIT|RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
		&&events[0].b==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[0].c==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	CHECK(events[1].o==k.historyColor[1]
		&&events[1].a==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[1].b==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[1].c==VK_IMAGE_LAYOUT_GENERAL);
	CHECK(events[2].o==k.historyDepth[1]
		&&events[2].a==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[2].b==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[2].c==VK_IMAGE_LAYOUT_GENERAL);
	CHECK(events[3].k=='P'&&events[3].o==o.pipeline);
	CHECK(events[4].k=='G'&&events[4].o==o.groups[1]&&events[4].a==0);
	CHECK(events[5].k=='U'&&events[5].a==RAL_STAGE_COMPUTE
		&&events[5].b==sizeof(p)&&memcmp(&gotPush,&p,sizeof(p))==0);
	CHECK(events[6].k=='D'&&events[6].a==160&&events[6].b==90&&events[6].c==1);
	CHECK(events[7].o==k.historyColor[1]
		&&events[7].a==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[7].b==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[7].c==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	CHECK(events[8].o==k.historyDepth[1]
		&&events[8].a==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[8].b==RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&&events[8].c==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	c0=eventCount;
	p.zNear=NAN;
	CHECK(!VK_TemporalHistoryStoreRecord(&o,&k,o.allocationGeneration,H(1),1,&p)
		&&eventCount==c0);
	p.zNear=4; p.extent[0]++;
	CHECK(!VK_TemporalHistoryStoreRecord(&o,&k,o.allocationGeneration,H(1),1,&p)
		&&eventCount==c0);
	p.extent[0]=1280;
	CHECK(!VK_TemporalHistoryStoreRecord(&o,&k,o.allocationGeneration+1,H(1),1,&p)
		&&eventCount==c0);
	k2=k; k2.sceneColorAttachmentGeneration++;
	CHECK(!VK_TemporalHistoryStoreRecord(&o,&k2,o.allocationGeneration,H(1),1,&p)
		&&eventCount==c0);
	for(role=0;role<6;++role) {
		mutated=o;
		switch(role) {
		case 0: mutated.currentDepthView=NULL; break;
		case 1: mutated.sampler=NULL; break;
		case 2: mutated.layout=NULL; break;
		case 3: mutated.groups[0]=NULL; break;
		case 4: mutated.groups[1]=NULL; break;
		default: mutated.pipeline=NULL; break;
		}
		CHECK(!VK_TemporalHistoryStoreRecord(&mutated,&k,o.allocationGeneration,
			H(1),1,&p)&&eventCount==c0);
	}
	keyBorrowed[0]=k.backend; keyBorrowed[1]=k.feedbackColor;
	keyBorrowed[2]=k.feedbackColorView; keyBorrowed[3]=k.currentDepth;
	keyBorrowed[4]=k.computeSpirv;
	for(i=0;i<2;++i) {
		keyBorrowed[5+i*4]=k.historyColor[i];
		keyBorrowed[6+i*4]=k.historyColorView[i];
		keyBorrowed[7+i*4]=k.historyDepth[i];
		keyBorrowed[8+i*4]=k.historyDepthView[i];
	}
	for(role=0;role<6;++role) for(j=0;j<13;++j) {
		mutated=o;
		switch(role) {
		case 0: mutated.currentDepthView=(ralTextureView_t*)keyBorrowed[j]; break;
		case 1: mutated.sampler=(ralSampler_t*)keyBorrowed[j]; break;
		case 2: mutated.layout=(ralBindGroupLayout_t*)keyBorrowed[j]; break;
		case 3: mutated.groups[0]=(ralBindGroup_t*)keyBorrowed[j]; break;
		case 4: mutated.groups[1]=(ralBindGroup_t*)keyBorrowed[j]; break;
		default: mutated.pipeline=(ralPipeline_t*)keyBorrowed[j]; break;
		}
		d0=destroys;
		CHECK(!VK_TemporalHistoryStoreRecord(&mutated,&k,o.allocationGeneration,
			H(1),1,&p)&&eventCount==c0);
		CHECK(VK_TemporalHistoryStoreReleaseAfterIdle(&mutated,qtrue));
		CHECK(!WasDestroyed(d0,keyBorrowed[j]));
	}

	k2=Key(0x3000);
	CHECK(VK_TemporalHistoryStoreNeedsIdle(&o,&k2));
	CHECK(!VK_TemporalHistoryStoreEnsureAfterFence(&o,&k2,qfalse));
	CHECK(memcmp(&o,&before,sizeof(o))==0);
	for(i=0;i<6;++i) {
		attemptStart=creates; memset(attemptHandles,0,sizeof(attemptHandles));
		failStep=creates+i+1; aliasHandle=NULL; aliasPreviousStep=UINT32_MAX;
		CHECK(!VK_TemporalHistoryStoreEnsureAfterFence(&o,&k2,qtrue));
		CHECK(memcmp(&o,&before,sizeof(o))==0);
	}

	protectedSet[protectedCount++]=k2.backend;
	protectedSet[protectedCount++]=k2.feedbackColor;
	protectedSet[protectedCount++]=k2.feedbackColorView;
	protectedSet[protectedCount++]=k2.currentDepth;
	protectedSet[protectedCount++]=k2.computeSpirv;
	for(i=0;i<2;++i) {
		protectedSet[protectedCount++]=k2.historyColor[i];
		protectedSet[protectedCount++]=k2.historyColorView[i];
		protectedSet[protectedCount++]=k2.historyDepth[i];
		protectedSet[protectedCount++]=k2.historyDepthView[i];
	}
	protectedSet[protectedCount++]=k.backend;
	protectedSet[protectedCount++]=k.feedbackColor;
	protectedSet[protectedCount++]=k.feedbackColorView;
	protectedSet[protectedCount++]=k.currentDepth;
	protectedSet[protectedCount++]=k.computeSpirv;
	for(i=0;i<2;++i) {
		protectedSet[protectedCount++]=k.historyColor[i];
		protectedSet[protectedCount++]=k.historyColorView[i];
		protectedSet[protectedCount++]=k.historyDepth[i];
		protectedSet[protectedCount++]=k.historyDepthView[i];
	}
	protectedSet[protectedCount++]=o.currentDepthView;
	protectedSet[protectedCount++]=o.sampler;
	protectedSet[protectedCount++]=o.layout;
	protectedSet[protectedCount++]=o.groups[0];
	protectedSet[protectedCount++]=o.groups[1];
	protectedSet[protectedCount++]=o.pipeline;
	CHECK(protectedCount==32);
	for(role=0;role<6;++role) for(j=0;j<protectedCount;++j) {
		attemptStart=creates; memset(attemptHandles,0,sizeof(attemptHandles));
		failStep=creates+role+1; aliasHandle=(void*)protectedSet[j];
		aliasPreviousStep=UINT32_MAX; d0=destroys;
		CHECK(!VK_TemporalHistoryStoreEnsureAfterFence(&o,&k2,qtrue));
		CHECK(memcmp(&o,&before,sizeof(o))==0);
		CHECK(!WasDestroyed(d0,protectedSet[j]));
	}
	for(role=1;role<6;++role) {
		attemptStart=creates; memset(attemptHandles,0,sizeof(attemptHandles));
		failStep=creates+role+1; aliasHandle=NULL; aliasPreviousStep=0;
		CHECK(!VK_TemporalHistoryStoreEnsureAfterFence(&o,&k2,qtrue));
		CHECK(memcmp(&o,&before,sizeof(o))==0);
	}
	failStep=0; aliasHandle=NULL; aliasPreviousStep=UINT32_MAX;
	attemptStart=creates; memset(attemptHandles,0,sizeof(attemptHandles));
	CHECK(VK_TemporalHistoryStoreEnsureAfterFence(&o,&k2,qtrue));
	CHECK(o.allocationGeneration==2);
	CHECK(!VK_TemporalHistoryStoreReleaseAfterIdle(&o,qfalse));
	CHECK(VK_TemporalHistoryStoreHasLive(&o));
	CHECK(VK_TemporalHistoryStoreReleaseAfterIdle(&o,qtrue));
	CHECK(!VK_TemporalHistoryStoreHasLive(&o)&&o.allocationGeneration==2);
	return 0;
}
