// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/renderervk/vk_temporal_iqm_payload_authoring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)

static unsigned char s_payload[TEMPORAL_IQM_SLOT_BYTES];

// Receipt-only host: payload allocation is independently covered by its owner
// contract, but the linked product implementation retains these RAL references.
ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *b,
		const ralBindGroupLayoutCreateInfo_t *ci ){(void)b;(void)ci;return NULL;}
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *l ){(void)l;}
ralBuffer_t *Ral_CreateBuffer( ralBackend_t *b,const ralBufferCreateInfo_t *ci ){
	(void)b;(void)ci;return NULL;}
void Ral_DestroyBuffer( ralBuffer_t *b ){(void)b;}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *b,
		const ralBindGroupCreateInfo_t *ci ){(void)b;(void)ci;return NULL;}
void Ral_DestroyBindGroup( ralBindGroup_t *g ){(void)g;}

struct ralFence_s { int id; };
static struct ralFence_s s_uploadFence = { 1 };
static int s_uploadCalls, s_waitCalls, s_destroyFenceCalls;
static qboolean s_failUpload;
ralFence_t *Ral_BufferUploadAsync( ralBuffer_t *buffer, uint64_t offset,
		const void *data, uint64_t size ) {
	(void)buffer;
	CHECK( offset == 0 && data == s_payload
		&& size == TEMPORAL_IQM_RECORD_SIZE );
	s_uploadCalls++;
	return s_failUpload ? NULL : &s_uploadFence;
}
void Ral_WaitFence( ralFence_t *fence, uint64_t timeoutNs ) {
	CHECK( fence == &s_uploadFence && timeoutNs == ~(uint64_t)0 );
	s_waitCalls++;
}
void Ral_DestroyFence( ralFence_t *fence ) {
	CHECK( fence == &s_uploadFence );
	s_destroyFenceCalls++;
}

static void Identity4( float out[16] ) {
	memset(out,0,16u*sizeof(float));out[0]=out[5]=out[10]=out[15]=1.0f;
}
static void Identity34( float out[12] ) {
	memset(out,0,12u*sizeof(float));out[0]=out[5]=out[10]=1.0f;
}
static temporalEntityPose_t Pose( int frame, float x ) {
	temporalEntityPose_t p;memset(&p,0,sizeof(p));p.hModel=7;p.modelToken=0x1000;
	p.modelDataToken=0x2000;p.modelType=4;p.modelTopology=9;
	p.modelAllocationGeneration=2;p.modelContentDigest=0x1234;p.frame=frame;
	p.oldframe=frame;p.origin[0]=x;p.axis[0]=p.axis[4]=p.axis[8]=1.0f;return p;
}
static refEntityMotion_t Identity( void ) {
	refEntityMotion_t i;memset(&i,0,sizeof(i));i.structSize=sizeof(i);
	i.version=REF_ENTITY_MOTION_VERSION;i.ownerId=3;i.generation=4;
	i.role=REF_ENTITY_MOTION_ROLE_PLAYER_BODY;return i;
}
static vkBindlessOrdinaryReceipt_t Bindless( void ) {
	vkBindlessOrdinaryReceipt_t r;memset(&r,0,sizeof(r));
	r.setIdentity=0x5000;r.samplerPoolIdentity=0x5100;r.imageViewIdentity=0x5200;
	r.samplerIdentity=0x5300;r.imageOwnerIdentity=0x5400;
	r.ordinaryDescriptorIdentity=0x5500;r.imageOwnerGeneration=2;
	r.samplerDefinitionDigest=3;r.setGeneration=4;r.samplerPoolGeneration=5;
	r.imagePublicationGeneration=6;r.imageTransactionGeneration=7;
	r.samplerPublicationGeneration=8;r.samplerTransactionGeneration=9;
	r.imageSlotGeneration=10;r.samplerSlotGeneration=11;r.imageSlot=12;
	r.samplerSlot=3;r.imageBinding=0;r.samplerBinding=1;r.ready=qtrue;return r;
}
static temporalIqmDrawFacts_t Draw( temporalMotionOutcome_t outcome ) {
	temporalIqmDrawFacts_t d;memset(&d,0,sizeof(d));d.identity=Identity();
	d.currentPose=Pose(1,1.0f);d.previousPose=Pose(0,0.0f);
	d.modelContentDigest=0x1234;d.modelTopologyGeneration=9;
	d.modelAllocationGeneration=2;d.indexCount=3;d.rawVertexBuffer=0x6000;
	d.rawIndexBuffer=0x6100;d.ralVertexBuffer=0x6200;d.ralIndexBuffer=0x6300;
	d.vertexBufferBytes=680;d.indexBufferBytes=12;d.geometryAllocationGeneration=5;
	d.geometryBackend=0x6400;d.geometryGeneration=6;
	d.textureSlot=12;d.samplerSlot=3;d.bindless=Bindless();d.outcome=outcome;
	d.previousValid=outcome==TEMPORAL_MOTION_WRITE_VALID?qtrue:qfalse;
	if(!d.previousValid)d.previousPose=d.currentPose;return d;
}
static vkTemporalIqmPayloadReceipt_t Payload( void ) {
	vkTemporalIqmPayloadReceipt_t r;memset(&r,0,sizeof(r));r.backend=(void*)0x10;
	r.layout=(void*)0x20;r.buffer=(void*)0x30;r.cpuShadowIdentity=s_payload;
	r.group=(void*)0x40;r.descriptorRange=TEMPORAL_IQM_SLOT_BYTES;
	r.recordCapacity=TEMPORAL_IQM_MAX_RECORDS;r.recordBytes=TEMPORAL_IQM_RECORD_SIZE;
	r.ownerAllocationGeneration=1;r.slotAllocationGeneration=2;
	r.prepareGeneration=3;r.commandSlot=0;r.frameCount=2;r.ready=qtrue;return r;
}
static vkTemporalIqmPayloadOwner_t PayloadOwner(
		const vkTemporalIqmPayloadReceipt_t *receipt ) {
	vkTemporalIqmPayloadOwner_t owner;memset(&owner,0,sizeof(owner));
	owner.key.backend=receipt->backend;owner.key.maxStorageBufferRange=TEMPORAL_IQM_SLOT_BYTES;
	owner.key.frameCount=receipt->frameCount;owner.layout=receipt->layout;
	owner.slots[receipt->commandSlot].buffer=receipt->buffer;
	owner.slots[receipt->commandSlot].cpuShadow=receipt->cpuShadowIdentity;
	owner.slots[receipt->commandSlot].group=receipt->group;
	owner.slots[receipt->commandSlot].allocationGeneration=receipt->slotAllocationGeneration;
	owner.slots[receipt->commandSlot].prepareGeneration=receipt->prepareGeneration;
	owner.ownerAllocationGeneration=receipt->ownerAllocationGeneration;
	owner.nextOwnerAllocationGeneration=receipt->ownerAllocationGeneration;
	owner.nextSlotAllocationGeneration[receipt->commandSlot]=receipt->slotAllocationGeneration;
	owner.nextPrepareGeneration[receipt->commandSlot]=receipt->prepareGeneration;
	owner.preparedSlot=receipt->commandSlot;owner.readySlotMask=1u<<receipt->commandSlot;
	owner.initialized=owner.ready=qtrue;return owner;
}
static void Model( temporalIqmModelView_t *model, int32_t *parent,
		float bind[12], float inverse[12], temporalIqmTransform_t poses[2] ) {
	memset(model,0,sizeof(*model));*parent=-1;Identity34(bind);Identity34(inverse);
	memset(poses,0,2u*sizeof(*poses));
	for(int i=0;i<2;i++){poses[i].rotate[3]=1.0f;poses[i].scale[0]=1.0f;
		poses[i].scale[1]=1.0f;poses[i].scale[2]=1.0f;}poses[1].translate[0]=1.0f;
	model->modelDataToken=0x2000;model->contentDigest=0x1234;
	model->topologyGeneration=9;model->modelAllocationGeneration=2;
	model->numFrames=2;model->numJoints=model->numPoses=1;
	model->jointParents=parent;model->bindJoints=bind;model->inverseBindJoints=inverse;
	model->poses=poses;model->validated=qtrue;
}
static temporalCameraPoseReceipt_t Camera( void ) {
	temporalCameraPoseReceipt_t c;memset(&c,0,sizeof(c));c.frameId=2;
	c.previousFrameId=1;Identity4(c.current.projection);Identity4(c.previous.projection);
	Identity4(c.current.worldModel);Identity4(c.previous.worldModel);
	c.valid=c.previousValid=qtrue;return c;
}

int main( void ) {
	temporalIqmDrawFacts_t draw=Draw(TEMPORAL_MOTION_WRITE_VALID);
	temporalIqmSequenceAuthority_t authority={7,2,0,0,2};
	temporalIqmSequence_t sequence;
	vkTemporalIqmPayloadReceipt_t payload=Payload();
	vkTemporalIqmPayloadOwner_t payloadOwner=PayloadOwner(&payload);
	temporalCameraPoseReceipt_t camera=Camera();float jittered[16];
	temporalIqmModelView_t model;int32_t parent;float bind[12],inverse[12];
	temporalIqmTransform_t poses[2];vkTemporalIqmPayloadAuthor_t author,before;
	vkTemporalIqmPayloadContentReceipt_t receipt,mutation;
	Identity4(jittered);Model(&model,&parent,bind,inverse,poses);
	CHECK(R_TemporalIqmSequenceBuild(&authority,&draw,1,&sequence));
	memset(s_payload,0x7f,sizeof(s_payload));
	VK_TemporalIqmPayloadAuthorInit(&author);before=author;
	{vkTemporalIqmPayloadReceipt_t bad=payload;bad.commandSlot=1;
		CHECK(!VK_TemporalIqmPayloadAuthorBegin(&author,&sequence,&bad,&camera,jittered));
		CHECK(memcmp(&author,&before,sizeof(author))==0);}
	CHECK(VK_TemporalIqmPayloadAuthorBegin(&author,&sequence,&payload,&camera,jittered));
	{unsigned char snapshot[TEMPORAL_IQM_RECORD_SIZE];memcpy(snapshot,s_payload,sizeof(snapshot));
		CHECK(!VK_TemporalIqmPayloadAuthorWrite(&author,1,&model));
		CHECK(memcmp(snapshot,s_payload,sizeof(snapshot))==0&&author.poisoned);
		CHECK(VK_TemporalIqmPayloadAuthorCancel(&author));}
	memset(s_payload,0,sizeof(s_payload));
	CHECK(VK_TemporalIqmPayloadAuthorBegin(&author,&sequence,&payload,&camera,jittered));
	CHECK(VK_TemporalIqmPayloadAuthorWrite(&author,0,&model));
	CHECK(!VK_TemporalIqmPayloadAuthorWrite(&author,0,&model));
	CHECK(author.poisoned&&VK_TemporalIqmPayloadAuthorCancel(&author));
	memset(s_payload,0,sizeof(s_payload));
	CHECK(VK_TemporalIqmPayloadAuthorBegin(&author,&sequence,&payload,&camera,jittered));
	CHECK(VK_TemporalIqmPayloadAuthorWrite(&author,0,&model));
	s_failUpload=qtrue;before=author;memset(&receipt,0xa5,sizeof(receipt));
	CHECK(!VK_TemporalIqmPayloadAuthorSeal(&author,&receipt));
	CHECK(author.poisoned&&s_uploadCalls==1&&s_waitCalls==0&&s_destroyFenceCalls==0);
	CHECK(((unsigned char*)&receipt)[0]==0xa5);
	CHECK(VK_TemporalIqmPayloadAuthorCancel(&author));
	s_failUpload=qfalse;memset(s_payload,0,sizeof(s_payload));
	CHECK(VK_TemporalIqmPayloadAuthorBegin(&author,&sequence,&payload,&camera,jittered));
	CHECK(VK_TemporalIqmPayloadAuthorWrite(&author,0,&model));
	CHECK(VK_TemporalIqmPayloadAuthorSeal(&author,&receipt));
	CHECK(s_uploadCalls==2&&s_waitCalls==1&&s_destroyFenceCalls==1);
	CHECK(receipt.ready&&receipt.recordCount==1&&receipt.sequenceDigest==sequence.orderedDigest);
	CHECK(VK_TemporalIqmPayloadContentReceiptExact(&receipt,&receipt));
	CHECK(VK_TemporalIqmPayloadContentRevalidate(&receipt,&payloadOwner));
#define BAD(field) do{mutation=receipt;mutation.field++;CHECK(!VK_TemporalIqmPayloadContentReceiptExact(&receipt,&mutation));}while(0)
	BAD(authority.token);BAD(payload.prepareGeneration);BAD(camera.frameId);
	BAD(jitteredProjection[0]);BAD(sequenceDigest);BAD(contentDigest);BAD(recordCount);
#undef BAD
	s_payload[0]^=1u;CHECK(!VK_TemporalIqmPayloadContentRevalidate(&receipt,&payloadOwner));s_payload[0]^=1u;
	CHECK(VK_TemporalIqmPayloadContentRevalidate(&receipt,&payloadOwner));
	{unsigned char exact[TEMPORAL_IQM_RECORD_SIZE];memcpy(exact,s_payload,sizeof(exact));
		CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&payloadOwner,&payloadOwner.key,0,qtrue,qfalse));
		memcpy(s_payload,exact,sizeof(exact));
		CHECK(!VK_TemporalIqmPayloadContentRevalidate(&receipt,&payloadOwner));}
	CHECK(VK_TemporalIqmPayloadAuthorCancel(&author));
	puts("vk temporal IQM payload authoring contract: PASS");return 0;
}
