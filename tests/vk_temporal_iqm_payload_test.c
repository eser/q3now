// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_payload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ralBackend_s { int id; };
struct ralBuffer_s { int id; byte *memory; };
struct ralBindGroup_s { int id; };
struct ralBindGroupLayout_s { int id; };

static int failures, operation, failAt, nextId=1;
static int creates, destroys, maps, unmaps, events[64], eventCount;
static void *aliasBuffer, *aliasMap, *aliasGroup, *aliasLayout;
static ralBuffer_t *lastBuffer;
static qboolean aliasMapToBuffer, aliasGroupToBuffer, aliasGroupToMap;

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); failures++; } } while (0)

static qboolean Fail( void ) { return ++operation == failAt ? qtrue : qfalse; }

ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *backend,
		const ralBindGroupLayoutCreateInfo_t *ci ) {
	ralBindGroupLayout_t *out; (void)backend;
	if ( Fail() || !ci || ci->numEntries != 1 || ci->bindless
			|| ci->entries[0].binding != 0
			|| ci->entries[0].type != RAL_BIND_STORAGE_BUFFER
			|| ci->entries[0].count != 1
			|| ci->entries[0].stageFlags != RAL_STAGE_VERTEX ) return NULL;
	if ( aliasLayout ) return aliasLayout;
	out=calloc(1,sizeof(*out)); out->id=nextId++; creates++; return out;
}
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout ) {
	events[eventCount++]=4; destroys++; free(layout);
}
ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *ci ) {
	ralBuffer_t *out; (void)backend;
	if ( Fail() || !ci || ci->size != TEMPORAL_IQM_SLOT_BYTES
			|| ci->usage != ( RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST )
			|| ci->memory != RAL_MEMORY_DEVICE_LOCAL ) return NULL;
	if ( aliasBuffer ) { void *a=aliasBuffer; aliasBuffer=NULL; return a; }
	out=calloc(1,sizeof(*out)); out->id=nextId++;
	out->memory=malloc(TEMPORAL_IQM_SLOT_BYTES); lastBuffer=out; creates++; return out;
}
void *Ral_MapBuffer( ralBuffer_t *buffer ) {
	if ( Fail() ) return NULL;
	if ( aliasMapToBuffer ) { aliasMapToBuffer=qfalse; return buffer; }
	if ( aliasMap ) { void *a=aliasMap; aliasMap=NULL; return a; }
	maps++; return buffer->memory;
}
void Ral_UnmapBuffer( ralBuffer_t *buffer ) { (void)buffer; events[eventCount++]=2; unmaps++; }
void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	events[eventCount++]=3; destroys++; free(buffer->memory); free(buffer);
}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *backend,
		const ralBindGroupCreateInfo_t *ci ) {
	ralBindGroup_t *out; (void)backend;
	if ( Fail() || !ci || ci->numValues != 1 || !ci->layout
			|| ci->values[0].binding != 0
			|| ci->values[0].type != RAL_BIND_STORAGE_BUFFER
			|| !ci->values[0].buffer || ci->values[0].bufferOffset != 0
			|| ci->values[0].bufferRange != TEMPORAL_IQM_SLOT_BYTES ) return NULL;
	if ( aliasGroupToBuffer ) { aliasGroupToBuffer=qfalse; return (void *)lastBuffer; }
	if ( aliasGroupToMap ) { aliasGroupToMap=qfalse; return (void *)lastBuffer->memory; }
	if ( aliasGroup ) { void *a=aliasGroup; aliasGroup=NULL; return a; }
	out=calloc(1,sizeof(*out)); out->id=nextId++; creates++; return out;
}
void Ral_DestroyBindGroup( ralBindGroup_t *group ) {
	events[eventCount++]=1; destroys++; free(group);
}

static vkTemporalIqmPayloadKey_t Key( ralBackend_t *backend,
		uint64_t cap, const void *protectedIdentity ) {
	vkTemporalIqmPayloadKey_t key;
	memset(&key,0,sizeof(key)); key.backend=backend; key.maxStorageBufferRange=cap;
	key.frameCount=2;
	if(protectedIdentity){key.protectedCount=1;key.protectedIdentities[0]=protectedIdentity;}
	return key;
}

static void ResetFake( void ) {
	operation=failAt=creates=destroys=maps=unmaps=eventCount=0;
	aliasBuffer=aliasMap=aliasGroup=aliasLayout=NULL;
	lastBuffer=NULL; aliasMapToBuffer=aliasGroupToBuffer=aliasGroupToMap=qfalse;
}

int main( void ) {
	ralBackend_t backend={1}, other={2};
	vkTemporalIqmPayloadOwner_t owner, before;
	vkTemporalIqmPayloadReceipt_t receipt, receiptBefore;
	ralBindGroupLayout_t *leasedLayout=NULL;
	uint32_t leasedGeneration=0;
	temporalIqmGpuRecord_t record;
	vkTemporalIqmPayloadKey_t key=Key(&backend,TEMPORAL_IQM_SLOT_BYTES,(void *)0x7000u);

	VK_TemporalIqmPayloadInit(&owner);
	CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,
		&(vkTemporalIqmPayloadKey_t){.backend=&backend,.maxStorageBufferRange=TEMPORAL_IQM_SLOT_BYTES-1,.frameCount=2},0,qtrue,qfalse));
	CHECK(!VK_TemporalIqmPayloadHasLive(&owner));
	ResetFake(); CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qfalse,qfalse));
	CHECK(VK_TemporalIqmPayloadHasLive(&owner));
	CHECK(creates==3 && maps==0 && owner.ownerAllocationGeneration==1
		&& owner.readySlotMask==1u);
	memset(&receipt,0x5a,sizeof(receipt));
	CHECK(VK_TemporalIqmPayloadGetReceipt(&owner,0,&receipt));
	CHECK(receipt.ready && receipt.descriptorRange==TEMPORAL_IQM_SLOT_BYTES
		&& receipt.commandSlot==0 && receipt.slotAllocationGeneration==1
		&& receipt.prepareGeneration==1 && receipt.recordCapacity==256
		&& receipt.recordBytes==12480);
	memset(&record,0x3c,sizeof(record)); (void)record;
	receiptBefore=receipt; CHECK(VK_TemporalIqmPayloadReceiptExact(&receipt,&receiptBefore));
	#define MUTATE_RECEIPT(field) do { receiptBefore=receipt; receiptBefore.field++; \
		CHECK(!VK_TemporalIqmPayloadReceiptExact(&receipt,&receiptBefore)); } while(0)
	MUTATE_RECEIPT(backend); MUTATE_RECEIPT(layout); MUTATE_RECEIPT(buffer);
	MUTATE_RECEIPT(cpuShadowIdentity); MUTATE_RECEIPT(group);
	MUTATE_RECEIPT(descriptorRange); MUTATE_RECEIPT(recordCapacity);
	MUTATE_RECEIPT(recordBytes); MUTATE_RECEIPT(ownerAllocationGeneration);
	MUTATE_RECEIPT(slotAllocationGeneration); MUTATE_RECEIPT(prepareGeneration);
	MUTATE_RECEIPT(commandSlot); MUTATE_RECEIPT(frameCount);
	#undef MUTATE_RECEIPT
	receiptBefore=receipt; receiptBefore.ready=qfalse;
	CHECK(!VK_TemporalIqmPayloadReceiptExact(&receipt,&receiptBefore));
	memset(&receiptBefore,0,sizeof(receiptBefore));
	CHECK(!VK_TemporalIqmPayloadReceiptExact(&receiptBefore,&receiptBefore));
	CHECK(!VK_TemporalIqmPayloadGetReceipt(&owner,1,&receiptBefore));
	CHECK(VK_TemporalIqmPayloadAcquireLayoutLease(&owner,&leasedLayout,&leasedGeneration));
	CHECK(leasedLayout==owner.layout && leasedGeneration==owner.ownerAllocationGeneration);
	CHECK(!VK_TemporalIqmPayloadReleaseAfterIdle(&owner,qtrue));
	CHECK(!VK_TemporalIqmPayloadReleaseLayoutLease(&owner,leasedLayout,leasedGeneration+1));
	{
		vkTemporalIqmPayloadKey_t leasedReplacement=key; leasedReplacement.backend=&other;
		before=owner; ResetFake();
		CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&leasedReplacement,0,qtrue,qtrue));
		CHECK(memcmp(&owner,&before,sizeof(owner))==0 && destroys==0);
	}
	CHECK(VK_TemporalIqmPayloadReleaseLayoutLease(&owner,leasedLayout,leasedGeneration));
	CHECK(!VK_TemporalIqmPayloadReleaseLayoutLease(&owner,leasedLayout,leasedGeneration));
	{
		vkTemporalIqmPayloadOwner_t saturated=owner, saturatedBefore;
		saturated.nextSlotAllocationGeneration[1]=UINT32_MAX-1u;
		saturatedBefore=saturated; ResetFake();
		CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&saturated,&key,1,qfalse,qfalse));
		CHECK(memcmp(&saturated,&saturatedBefore,sizeof(saturated))==0);
	}

	// Same-key reuse is fence-gated, allocates nothing, and prepares only the slot.
	ResetFake(); before=owner;
	CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,1,qfalse,qfalse));
	CHECK(operation==2 && owner.preparedSlot==1 && owner.readySlotMask==3u
		&& ((byte*)owner.slots[1].cpuShadow)[0]==0);
	CHECK(VK_TemporalIqmPayloadGetReceipt(&owner,1,&receiptBefore));
	CHECK(receiptBefore.prepareGeneration==1);
	before=owner; ResetFake();
	CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,1,qfalse,qfalse));
	CHECK(memcmp(&owner,&before,sizeof(owner))==0);
	CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,1,qtrue,qfalse));
	CHECK(operation==0 && owner.slots[1].prepareGeneration==2);
	CHECK(VK_TemporalIqmPayloadGetReceipt(&owner,1,&receiptBefore));
	CHECK(!VK_TemporalIqmPayloadReceiptExact(&receipt,&receiptBefore));
	{
		vkTemporalIqmPayloadOwner_t saturated=owner, saturatedBefore;
		saturated.nextPrepareGeneration[1]=UINT32_MAX-1u; saturatedBefore=saturated;
		CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&saturated,&key,1,qtrue,qfalse));
		CHECK(memcmp(&saturated,&saturatedBefore,sizeof(saturated))==0);
		saturated=owner; saturated.nextOwnerAllocationGeneration=UINT32_MAX-1u;
		vkTemporalIqmPayloadKey_t changed=key; changed.backend=&other;
		saturatedBefore=saturated;
		CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&saturated,&changed,0,qtrue,qtrue));
		CHECK(memcmp(&saturated,&saturatedBefore,sizeof(saturated))==0);
	}

	// Key replacement is output-atomic without global idle and exact with it.
	vkTemporalIqmPayloadKey_t replacement=key; replacement.backend=&other;
	before=owner; CHECK(VK_TemporalIqmPayloadNeedsIdle(&owner,&replacement));
	CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&replacement,0,qtrue,qfalse));
	CHECK(memcmp(&owner,&before,sizeof(owner))==0);
	{
		void *oldRoles[7]={owner.layout,owner.slots[0].buffer,owner.slots[0].cpuShadow,
			owner.slots[0].group,owner.slots[1].buffer,owner.slots[1].cpuShadow,
			owner.slots[1].group};
		for(int i=0;i<7;++i){
			ResetFake(); aliasLayout=oldRoles[i];
			CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&replacement,0,qtrue,qtrue));
			CHECK(memcmp(&owner,&before,sizeof(owner))==0);
			ResetFake(); aliasBuffer=oldRoles[i];
			CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&replacement,0,qtrue,qtrue));
			CHECK(memcmp(&owner,&before,sizeof(owner))==0);
			ResetFake(); aliasGroup=oldRoles[i];
			CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&replacement,0,qtrue,qtrue));
			CHECK(memcmp(&owner,&before,sizeof(owner))==0);
		}
	}
	for(int step=1;step<=3;++step){
		ResetFake(); failAt=step;
		CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&replacement,0,qtrue,qtrue));
		CHECK(memcmp(&owner,&before,sizeof(owner))==0);
	}
	ResetFake(); CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&replacement,0,qtrue,qtrue));
	CHECK(owner.ownerAllocationGeneration==2 && owner.readySlotMask==1u
		&& destroys==5 && unmaps==0);

	// Every owned-resource creation step fails candidate-first and leaves a fresh owner empty.
	CHECK(VK_TemporalIqmPayloadReleaseAfterIdle(&owner,qtrue));
	for(int step=1;step<=3;++step){
		ResetFake(); failAt=step; VK_TemporalIqmPayloadInit(&owner); before=owner;
		CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qtrue,qfalse));
		CHECK(memcmp(&owner,&before,sizeof(owner))==0);
		CHECK(!VK_TemporalIqmPayloadHasLive(&owner));
	}

	// Protected and cross-role aliases reject without destroying borrowed roles.
	ResetFake(); VK_TemporalIqmPayloadInit(&owner); aliasBuffer=(void*)0x7000u;
	CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qtrue,qfalse));
	CHECK(destroys==1); /* owned layout only */
	ResetFake(); VK_TemporalIqmPayloadInit(&owner); aliasLayout=(void*)0x7000u;
	CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qtrue,qfalse));
	CHECK(destroys==0);
	ResetFake(); VK_TemporalIqmPayloadInit(&owner); aliasGroupToBuffer=qtrue;
	CHECK(!VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qtrue,qfalse));
	CHECK(destroys==2 && unmaps==0);

	// Receipt failure is output-atomic; release requires idle and orders BG before buffer.
	ResetFake(); VK_TemporalIqmPayloadInit(&owner);
	CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qtrue,qfalse));
	CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,1,qfalse,qfalse));
	CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qtrue,qfalse));
	{
		vkTemporalIqmPayloadOwner_t invalid=owner;
		invalid.slots[0].cpuShadow=NULL;
		CHECK(VK_TemporalIqmPayloadHasLive(&invalid));
		CHECK(VK_TemporalIqmPayloadNeedsIdle(&invalid,&key));
		CHECK(!VK_TemporalIqmPayloadReleaseAfterIdle(&invalid,qtrue));
	}
	receiptBefore=receipt; owner.slots[0].allocationGeneration=0;
	CHECK(!VK_TemporalIqmPayloadGetReceipt(&owner,0,&receipt));
	CHECK(memcmp(&receipt,&receiptBefore,sizeof(receipt))==0);
	owner.slots[0].allocationGeneration=1; before=owner;
	CHECK(!VK_TemporalIqmPayloadReleaseAfterIdle(&owner,qfalse));
	CHECK(memcmp(&owner,&before,sizeof(owner))==0);
	eventCount=0; CHECK(VK_TemporalIqmPayloadReleaseAfterIdle(&owner,qtrue));
	for(int i=0;i<4;i+=2) CHECK(events[i]==1 && events[i+1]==3);
	CHECK(!VK_TemporalIqmPayloadHasLive(&owner));
	ResetFake(); CHECK(VK_TemporalIqmPayloadPrepareAfterFence(&owner,&key,0,qfalse,qfalse));
	CHECK(owner.ownerAllocationGeneration==2 && owner.slots[0].allocationGeneration==2
		&& owner.slots[0].prepareGeneration==3);
	CHECK(VK_TemporalIqmPayloadReleaseAfterIdle(&owner,qtrue));

	CHECK(!R_TemporalIqmStorageRangeSupported(0));
	CHECK(!R_TemporalIqmStorageRangeSupported(TEMPORAL_IQM_SLOT_BYTES-1u));
	CHECK(R_TemporalIqmStorageRangeSupported(TEMPORAL_IQM_SLOT_BYTES));
	CHECK(R_TemporalIqmStorageRangeSupported(UINT64_MAX));
	if(failures)return 1; puts("vk_temporal_iqm_payload_test: PASS"); return 0;
}
