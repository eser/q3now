// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_history_consume.h"
#include "vulkan/vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL history consume %d: %s\n",__LINE__,#x); return 1; } } while(0)

struct ralBackend_s { int id; };
struct ralCommandBuffer_s { int id; };
struct ralTexture_s { int id; };
struct ralTextureView_s { int id; };
struct ralSampler_s { int id; };
struct ralBindGroupLayout_s { int id; };
struct ralBindGroup_s { int id; };
struct ralPipeline_s { int id; };
struct ralBuffer_s { int id; unsigned char bytes[VK_TEMPORAL_HISTORY_WITNESS_BYTES];
	uint32_t usage; ralMemoryType_t memory; };

static int nextId=100, creates, destroys, maps, unmaps, dispatches, transitions, copies;
static ralBuffer_t *aliasBuffer;
static ralTextureView_t *aliasView;
static ralBindGroup_t *boundGroup;
static uint32_t pushed[16], pushedSize;
static ralResourceUsage_t transitionBefore[256], transitionAfter[256];

ralTextureView_t *Ral_CreateTextureView(ralBackend_t *b,const ralTextureViewCreateInfo_t *ci){
	ralTextureView_t *v; (void)b; if(!ci||!ci->texture)return NULL; if(aliasView)return aliasView; v=calloc(1,sizeof(*v)); v->id=nextId++; creates++; return v;
}
void Ral_DestroyTextureView(ralTextureView_t *v){if(v){destroys++;free(v);}}
ralSampler_t *Ral_CreateSampler(ralBackend_t *b,const ralSamplerCreateInfo_t *ci){ralSampler_t*s;(void)b;if(!ci)return NULL;s=calloc(1,sizeof(*s));s->id=nextId++;creates++;return s;}
void Ral_DestroySampler(ralSampler_t*s){if(s){destroys++;free(s);}}
ralBindGroupLayout_t *Ral_CreateBindGroupLayout(ralBackend_t*b,const ralBindGroupLayoutCreateInfo_t*ci){ralBindGroupLayout_t*l;(void)b;if(!ci||ci->numEntries!=6)return NULL;l=calloc(1,sizeof(*l));l->id=nextId++;creates++;return l;}
void Ral_DestroyBindGroupLayout(ralBindGroupLayout_t*l){if(l){destroys++;free(l);}}
ralBuffer_t *Ral_CreateBuffer(ralBackend_t*b,const ralBufferCreateInfo_t*ci){ralBuffer_t*x;(void)b;if(aliasBuffer)return aliasBuffer;if(!ci||ci->size!=VK_TEMPORAL_HISTORY_WITNESS_BYTES)return NULL;if(ci->usage==(RAL_BUFFER_STORAGE|RAL_BUFFER_TRANSFER_SRC)){if(ci->memory!=RAL_MEMORY_DEVICE_LOCAL)return NULL;}else if(ci->usage==(RAL_BUFFER_TRANSFER_DST|RAL_BUFFER_MAP_READ)){if(ci->memory!=RAL_MEMORY_HOST_COHERENT)return NULL;}else return NULL;x=calloc(1,sizeof(*x));x->id=nextId++;x->usage=ci->usage;x->memory=ci->memory;creates++;return x;}
void Ral_DestroyBuffer(ralBuffer_t*b){if(b){destroys++;free(b);}}
ralResult_t Ral_BufferMapBegin(ralBuffer_t*b,const ralBufferMapRequest_t*r,ralBufferMapTicket_t*t){if(!b||!r||!t||b->usage!=(RAL_BUFFER_TRANSFER_DST|RAL_BUFFER_MAP_READ)||r->mode!=RAL_MAP_READ||r->offset!=0||r->size!=VK_TEMPORAL_HISTORY_WITNESS_BYTES)return ralErrorInvalidArgument;memset(t,0,sizeof(*t));t->bufferIdentity=b;t->generation=1;t->request=*r;t->status=RAL_BUFFER_MAP_READY;t->mappedRange=b->bytes;maps++;return ralSuccess;}
ralResult_t Ral_BufferMapUnmap(ralBuffer_t*b,const ralBufferMapTicket_t*t){if(!b||!t||t->bufferIdentity!=b||t->mappedRange!=b->bytes)return ralErrorInvalidArgument;unmaps++;return ralSuccess;}
ralBindGroup_t *Ral_CreateBindGroup(ralBackend_t*b,const ralBindGroupCreateInfo_t*ci){ralBindGroup_t*g;(void)b;if(!ci||ci->numValues!=6)return NULL;g=calloc(1,sizeof(*g));g->id=nextId++;creates++;return g;}
void Ral_DestroyBindGroup(ralBindGroup_t*g){if(g){destroys++;free(g);}}
ralPipeline_t *Ral_CreateComputePipeline(ralBackend_t*b,const ralComputePipelineCreateInfo_t*ci){ralPipeline_t*p;(void)b;if(!ci||!ci->computeSpirv||!ci->computeSpirvSize||ci->pushConstantSize==0)return NULL;p=calloc(1,sizeof(*p));p->id=nextId++;creates++;return p;}
void Ral_DestroyPipeline(ralPipeline_t*p){if(p){destroys++;free(p);}}
void Ral_CmdBindPipeline(ralCommandBuffer_t*c,ralPipeline_t*p){(void)c;(void)p;}
void Ral_CmdBindBindGroup(ralCommandBuffer_t*c,uint32_t i,ralBindGroup_t*g){(void)c;if(i==0)boundGroup=g;}
void Ral_CmdPushConstants(ralCommandBuffer_t*c,uint32_t s,uint32_t o,uint32_t z,const void*d){(void)c;(void)s;(void)o;pushedSize=z;memset(pushed,0,sizeof(pushed));if(d&&z<=sizeof(pushed))memcpy(pushed,d,z);}
void Ral_CmdDispatch(ralCommandBuffer_t*c,uint32_t x,uint32_t y,uint32_t z){(void)c;if(x==1&&y==1&&z==1)dispatches++;}
ralResult_t Ral_CmdTransitionResources(ralCommandBuffer_t*c,const ralResourceTransitionBatch_t*b){(void)c;if(!b||b->bufferTransitionCount!=1||!b->bufferTransitions)return ralErrorInvalidArgument;transitionBefore[transitions]=b->bufferTransitions[0].before.usage;transitionAfter[transitions]=b->bufferTransitions[0].after.usage;transitions++;return ralSuccess;}
void Ral_CmdCopyBuffer(ralCommandBuffer_t*c,ralBuffer_t*s,ralBuffer_t*d,const ralBufferCopy_t*r){(void)c;if(!s||!d||!r||r->size!=VK_TEMPORAL_HISTORY_WITNESS_BYTES)abort();memcpy(d->bytes,s->bytes,VK_TEMPORAL_HISTORY_WITNESS_BYTES);copies++;}

static void MakeKey(vkTemporalHistoryConsumeKey_t*k,ralBackend_t*b,ralTexture_t*t,ralTextureView_t*v){
	memset(k,0,sizeof(*k));k->backend=b;k->worldIndex=2;k->width=64;k->height=32;k->topologyEpoch=3;k->historyAllocationGeneration=4;
	k->currentColor=&t[0];k->currentDepth=&t[1];
	for(unsigned i=0;i<2;i++){k->historyColor[i]=&t[2+i*2];k->historyDepth[i]=&t[3+i*2];k->historyColorView[i]=&v[i*2];k->historyDepthView[i]=&v[1+i*2];}
}

static void MakePlanView(const vkTemporalHistoryConsumeKey_t*k,ralTemporalFramePlan_t*p,temporalHistoryFrameView_t*v){
	memset(p,0,sizeof(*p));p->enabled=1;p->historyValid=1;p->frameId=9;p->generation=5;p->historyReadIndex=0;p->historyWriteIndex=1;
	memset(v,0,sizeof(*v));v->historyValid=qtrue;v->readIndex=0;v->writeIndex=1;v->readColor=k->historyColor[0];v->readColorView=k->historyColorView[0];v->readDepth=k->historyDepth[0];v->readDepthView=k->historyDepthView[0];v->writeColor=k->historyColor[1];v->writeColorView=k->historyColorView[1];v->writeDepth=k->historyDepth[1];v->writeDepthView=k->historyDepthView[1];
	v->committed.valid=qtrue;v->committed.frameId=8;v->committed.worldIndex=2;v->committed.planGeneration=5;v->committed.allocationGeneration=4;v->committed.historyIndex=0;v->committed.width=64;v->committed.height=32;v->committed.topologyEpoch=3;v->committed.color=v->readColor;v->committed.colorView=v->readColorView;v->committed.depth=v->readDepth;v->committed.depthView=v->readDepthView;
}

int main(void){
	struct ralBackend_s backend={1};struct ralCommandBuffer_s cb={2};struct ralTexture_s tex[6];struct ralTextureView_s views[4];
	vkTemporalHistoryConsumeOwner_t owner,before;vkTemporalHistoryConsumeKey_t key;ralTemporalFramePlan_t plan,plan2,bootstrap;temporalHistoryFrameView_t view,view2;
	temporalHistoryCommittedReceipt_t committed[2],goodCommitted,badCommitted;vkTemporalHistoryConsumeTicket_t ticket,ticket0,sentinel;vkTemporalHistoryConsumeReceipt_t receipt,receipt2,receiptSentinel;static const uint32_t spirv[]={0x07230203,0};uint32_t*w,nearBits,farBits;
	for(int i=0;i<6;i++)tex[i].id=i+1;for(int i=0;i<4;i++)views[i].id=i+20;
	MakeKey(&key,&backend,tex,views);MakePlanView(&key,&plan,&view);memset(committed,0,sizeof(committed));
	VK_TemporalHistoryConsumeInit(&owner);CHECK(!VK_TemporalHistoryConsumeHasLive(&owner));CHECK(VK_TemporalHistoryConsumeArm(&owner,2));
	{
		vkTemporalHistoryConsumeOwner_t aliasOwner, aliasBefore;
		VK_TemporalHistoryConsumeInit(&aliasOwner);CHECK(VK_TemporalHistoryConsumeArm(&aliasOwner,1));aliasBefore=aliasOwner;
		aliasView=&views[0];CHECK(!VK_TemporalHistoryConsumeEnsureAfterFence(&aliasOwner,&key,2,0,spirv,sizeof(spirv)));aliasView=NULL;
		CHECK(memcmp(&aliasOwner,&aliasBefore,sizeof(aliasOwner))==0);
	}
	CHECK(VK_TemporalHistoryConsumeEnsureAfterFence(&owner,&key,2,0,spirv,sizeof(spirv)));CHECK(owner.ready&&owner.slots[0].state==VK_TEMPORAL_HISTORY_CONSUME_READY);CHECK(VK_TemporalHistoryConsumeHasLive(&owner));before=owner;CHECK(!VK_TemporalHistoryConsumeReleaseAfterIdle(&owner,qfalse));CHECK(memcmp(&owner,&before,sizeof(owner))==0);memcpy(&nearBits,&(float){0.1f},sizeof(nearBits));memcpy(&farBits,&(float){100.f},sizeof(farBits));
	bootstrap=plan;bootstrap.historyValid=0;before=owner;CHECK(!VK_TemporalHistoryConsumeRecord(&owner,&cb,0,&bootstrap,&view,0.1f,100.f));CHECK(dispatches==0&&memcmp(&owner,&before,sizeof(owner))==0);
	CHECK(VK_TemporalHistoryConsumeRecord(&owner,&cb,0,&plan,&view,0.1f,100.f));CHECK(dispatches==1&&transitions==5&&copies==1);CHECK(boundGroup==owner.slots[0].groups[0]&&pushedSize==48&&pushed[0]==64&&pushed[1]==32&&pushed[2]==9&&pushed[3]==0&&pushed[4]==2&&pushed[5]==5&&pushed[6]==4&&pushed[7]==1&&pushed[8]==0&&pushed[9]==1&&pushed[10]==nearBits&&pushed[11]==farBits);CHECK(transitionBefore[0]==RAL_RESOURCE_USAGE_UNDEFINED&&transitionAfter[0]==RAL_RESOURCE_USAGE_STORAGE_WRITE&&transitionBefore[1]==RAL_RESOURCE_USAGE_STORAGE_WRITE&&transitionAfter[1]==RAL_RESOURCE_USAGE_COPY_SOURCE&&transitionBefore[2]==RAL_RESOURCE_USAGE_UNDEFINED&&transitionAfter[2]==RAL_RESOURCE_USAGE_COPY_DESTINATION&&transitionBefore[3]==RAL_RESOURCE_USAGE_COPY_DESTINATION&&transitionAfter[3]==RAL_RESOURCE_USAGE_HOST_READ&&transitionBefore[4]==RAL_RESOURCE_USAGE_COPY_SOURCE&&transitionAfter[4]==RAL_RESOURCE_USAGE_STORAGE_WRITE);CHECK(!VK_TemporalHistoryConsumeAcceptStore(&owner,0,qfalse));CHECK(owner.slots[0].state==VK_TEMPORAL_HISTORY_CONSUME_READY);
	CHECK(VK_TemporalHistoryConsumeRecord(&owner,&cb,0,&plan,&view,0.1f,100.f));CHECK(VK_TemporalHistoryConsumeAcceptStore(&owner,0,qtrue));
	memset(&sentinel,0xa5,sizeof(sentinel));ticket=sentinel;committed[1]=view.committed;committed[1].frameId=9;committed[1].historyIndex=1;committed[1].color=key.historyColor[1];committed[1].colorView=key.historyColorView[1];committed[1].depth=key.historyDepth[1];committed[1].depthView=key.historyDepthView[1];
	CHECK(!VK_TemporalHistoryConsumeResolveSubmit(&owner,0,qfalse,NULL,&ticket));CHECK(memcmp(&ticket,&sentinel,sizeof(ticket))==0&&owner.slots[0].state==VK_TEMPORAL_HISTORY_CONSUME_READY&&owner.capturesRemaining==2);
	goodCommitted=committed[1];
	for(unsigned mutation=0;mutation<13;mutation++){
		CHECK(VK_TemporalHistoryConsumeRecord(&owner,&cb,0,&plan,&view,0.1f,100.f));CHECK(VK_TemporalHistoryConsumeAcceptStore(&owner,0,qtrue));badCommitted=goodCommitted;
		switch(mutation){case 0:badCommitted.valid=qfalse;break;case 1:badCommitted.worldIndex++;break;case 2:badCommitted.frameId++;break;case 3:badCommitted.planGeneration++;break;case 4:badCommitted.allocationGeneration++;break;case 5:badCommitted.historyIndex=0;break;case 6:badCommitted.width++;break;case 7:badCommitted.height++;break;case 8:badCommitted.topologyEpoch++;break;case 9:badCommitted.color=NULL;break;case 10:badCommitted.colorView=NULL;break;case 11:badCommitted.depth=NULL;break;default:badCommitted.depthView=NULL;break;}
		committed[1]=badCommitted;ticket=sentinel;CHECK(!VK_TemporalHistoryConsumeResolveSubmit(&owner,0,qtrue,committed,&ticket));CHECK(memcmp(&ticket,&sentinel,sizeof(ticket))==0&&owner.slots[0].state==VK_TEMPORAL_HISTORY_CONSUME_READY&&owner.capturesRemaining==2);
	}
	committed[1]=goodCommitted;
	CHECK(VK_TemporalHistoryConsumeRecord(&owner,&cb,0,&plan,&view,0.1f,100.f));CHECK(VK_TemporalHistoryConsumeAcceptStore(&owner,0,qtrue));
	CHECK(VK_TemporalHistoryConsumeResolveSubmit(&owner,0,qtrue,committed,&ticket));CHECK(ticket.previousCommitted.frameId==8&&ticket.historyWriteIndex==1&&ticket.submitted);ticket0=ticket;
	// Adjacent capture: slot 0 remains SUBMITTED while the independently
	// fence-complete slot 1 is prepared, records the next physical history pair,
	// and reaches SUBMITTED. Same-key preparation must not serialize all slots.
	CHECK(VK_TemporalHistoryConsumeEnsureAfterFence(&owner,&key,2,1,spirv,sizeof(spirv)));
	CHECK(owner.slots[1].gpuBuffer&&owner.slots[1].readbackBuffer&&!owner.slots[1].hostReadable);
	plan2=plan;plan2.frameId=10;plan2.historyReadIndex=1;plan2.historyWriteIndex=0;
	view2=view;view2.readIndex=1;view2.writeIndex=0;view2.committed=committed[1];
	view2.readColor=key.historyColor[1];view2.readColorView=key.historyColorView[1];view2.readDepth=key.historyDepth[1];view2.readDepthView=key.historyDepthView[1];
	view2.writeColor=key.historyColor[0];view2.writeColorView=key.historyColorView[0];view2.writeDepth=key.historyDepth[0];view2.writeDepthView=key.historyDepthView[0];
	CHECK(VK_TemporalHistoryConsumeRecord(&owner,&cb,1,&plan2,&view2,0.1f,100.f));CHECK(boundGroup==owner.slots[1].groups[1]&&pushed[0]==64&&pushed[1]==32&&pushed[2]==10&&pushed[3]==0&&pushed[4]==2&&pushed[5]==5&&pushed[6]==4&&pushed[7]==1&&pushed[8]==1&&pushed[9]==0&&pushed[10]==nearBits&&pushed[11]==farBits);CHECK(VK_TemporalHistoryConsumeAcceptStore(&owner,1,qtrue));
	committed[0]=committed[1];committed[0].frameId=10;committed[0].historyIndex=0;committed[0].color=key.historyColor[0];committed[0].colorView=key.historyColorView[0];committed[0].depth=key.historyDepth[0];committed[0].depthView=key.historyDepthView[0];
	CHECK(VK_TemporalHistoryConsumeResolveSubmit(&owner,1,qtrue,committed,&ticket));CHECK(ticket.previousCommitted.frameId==9&&ticket.historyWriteIndex==0&&owner.capturesRemaining==0);
	before=owner;CHECK(!VK_TemporalHistoryConsumeReleaseAfterIdle(&owner,qtrue));CHECK(memcmp(&owner,&before,sizeof(owner))==0);
	memset(&receiptSentinel,0xa5,sizeof(receiptSentinel));receipt=receiptSentinel;CHECK(!VK_TemporalHistoryConsumeCompleteAfterFence(&owner,0,qfalse,&receipt));CHECK(memcmp(&receipt,&receiptSentinel,sizeof(receipt))==0);
	w=(uint32_t*)owner.slots[0].readbackBuffer->bytes;memset(w,0,VK_TEMPORAL_HISTORY_WITNESS_BYTES);w[0]=VK_TEMPORAL_HISTORY_WITNESS_MAGIC;w[1]=1;w[2]=9;w[4]=2;w[5]=5;w[6]=4;w[7]=0;w[8]=1;w[9]=64;w[10]=32;w[11]=1;w[13]=0x3f800000u;w[14]=2;w[16]=0x3f800000u;
	CHECK(VK_TemporalHistoryConsumeCompleteAfterFence(&owner,0,qtrue,&receipt));CHECK(receipt.ready&&receipt.previousColorRG==2&&owner.slots[1].state==VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED);
	owner.slots[0].ticket=ticket0;owner.slots[0].state=VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED;memset(owner.slots[0].readbackBuffer->bytes,0x7f,VK_TEMPORAL_HISTORY_WITNESS_BYTES);CHECK(VK_TemporalHistoryConsumeCompleteAfterFence(&owner,0,qtrue,&receipt));CHECK(!receipt.ready);
	owner.slots[0].ticket=ticket0;owner.slots[0].state=VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED;w=(uint32_t*)owner.slots[0].readbackBuffer->bytes;memset(w,0,VK_TEMPORAL_HISTORY_WITNESS_BYTES);w[0]=VK_TEMPORAL_HISTORY_WITNESS_MAGIC;w[1]=1;w[2]=9;w[4]=2;w[5]=5;w[6]=4;w[7]=0;w[8]=1;w[9]=64;w[10]=32;CHECK(VK_TemporalHistoryConsumeCompleteAfterFence(&owner,0,qtrue,&receipt));CHECK(!receipt.ready);
	owner.slots[0].ticket=ticket0;owner.slots[0].state=VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED;w=(uint32_t*)owner.slots[0].readbackBuffer->bytes;memset(w,0,VK_TEMPORAL_HISTORY_WITNESS_BYTES);w[0]=VK_TEMPORAL_HISTORY_WITNESS_MAGIC;w[1]=1;w[2]=9;w[4]=2;w[5]=5;w[6]=4;w[7]=0;w[8]=1;w[9]=64;w[10]=32;w[11]=1;w[13]=0x7f800000u;w[14]=2;w[16]=0x3f800000u;CHECK(VK_TemporalHistoryConsumeCompleteAfterFence(&owner,0,qtrue,&receipt));CHECK(!receipt.ready);
	owner.slots[0].ticket=ticket0;owner.slots[0].state=VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED;w=(uint32_t*)owner.slots[0].readbackBuffer->bytes;memset(w,0,VK_TEMPORAL_HISTORY_WITNESS_BYTES);w[0]=VK_TEMPORAL_HISTORY_WITNESS_MAGIC;w[1]=1;w[2]=9;w[4]=2;w[5]=5;w[6]=4;w[7]=0;w[8]=1;w[9]=64;w[10]=32;w[11]=1;w[13]=0x3f800000u;w[14]=2;w[16]=0x7f800000u;CHECK(VK_TemporalHistoryConsumeCompleteAfterFence(&owner,0,qtrue,&receipt));CHECK(!receipt.ready);
	w=(uint32_t*)owner.slots[1].readbackBuffer->bytes;memset(w,0,VK_TEMPORAL_HISTORY_WITNESS_BYTES);w[0]=VK_TEMPORAL_HISTORY_WITNESS_MAGIC;w[1]=1;w[2]=10;w[4]=2;w[5]=5;w[6]=4;w[7]=1;w[8]=0;w[9]=64;w[10]=32;w[11]=3;w[13]=0x40000000u;w[14]=1;w[16]=0x3f800000u;
	CHECK(VK_TemporalHistoryConsumeCompleteAfterFence(&owner,1,qtrue,&receipt2));CHECK(receipt2.ready&&receipt2.previousColorRG==1&&owner.slots[0].state==VK_TEMPORAL_HISTORY_CONSUME_READY);
	CHECK(VK_TemporalHistoryConsumeReleaseAfterIdle(&owner,qtrue));CHECK(!owner.ready&&maps==unmaps&&maps==6&&destroys>0);puts("PASS temporal history consume contract");return 0;
}
