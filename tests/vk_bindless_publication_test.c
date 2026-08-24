// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "../code/render/ral/backends/vulkan/renderer/vk_bindless_publication.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

static vkBindlessPublicationLedger_t s_ledger;

int main( void ) {
	const void *set=(void *)0x1000u, *pool=(void *)0x2000u;
	uint32_t imageSlots[2]={7,9}, samplerSlots[2]={3,4};
	const void *views[2]={(void *)0x3000u,(void *)0x3001u};
	const void *owners[2]={(void *)0x4000u,(void *)0x4001u};
	const void *descriptors[2]={(void *)0x5000u,(void *)0x5001u};
	const void *samplers[2]={(void *)0x6000u,(void *)0x6001u};
	uint64_t ownerGens[2]={11,12}, samplerDigests[2]={21,22};
	vkBindlessPublicationKind_t kinds[2]={
		VK_BINDLESS_PUBLICATION_LEGACY_EXACT,
		VK_BINDLESS_PUBLICATION_RESIDENT };
	vkBindlessOrdinaryReceipt_t receipt, snapshot, mutation;
	vkBindlessPublicationLedger_t ledgerSnapshot;
	memset(&receipt,0x5a,sizeof(receipt)); snapshot=receipt;
	VK_BindlessPublicationInit(&s_ledger);
	CHECK(!VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,7,views[0],
		owners[0],descriptors[0],ownerGens[0],3,samplers[0],samplerDigests[0],&receipt));
	CHECK(memcmp(&receipt,&snapshot,sizeof(receipt))==0);
	CHECK(VK_BindlessPublicationActivateSamplerPool(&s_ledger,pool));
	CHECK(VK_BindlessPublicationActivateSet(&s_ledger,set));
	CHECK(s_ledger.setGeneration==1 && s_ledger.samplerPoolGeneration==1);
	CHECK(!VK_BindlessPublicationActivateSet(&s_ledger,pool));
	CHECK(VK_BindlessPublicationViews(&s_ledger,set,imageSlots,views,owners,
		descriptors,ownerGens,kinds,2));
	CHECK(s_ledger.images[7].transactionGeneration
		==s_ledger.images[9].transactionGeneration);
	CHECK(s_ledger.images[7].slotGeneration==1
		&& s_ledger.images[9].slotGeneration==1);
	CHECK(VK_BindlessPublicationSamplers(&s_ledger,set,pool,samplerSlots,
		samplers,samplerDigests,2));
	CHECK(VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,7,views[0],
		owners[0],descriptors[0],ownerGens[0],3,samplers[0],samplerDigests[0],&receipt));
	CHECK(receipt.ready && receipt.imageBinding==0 && receipt.samplerBinding==1);
	CHECK(receipt.imageSlotGeneration==1 && receipt.samplerSlotGeneration==1);
	#define MUTATE_EXACT(field) do { mutation=receipt; mutation.field++; \
		CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation)); } while (0)
	MUTATE_EXACT(setIdentity);
	MUTATE_EXACT(samplerPoolIdentity);
	MUTATE_EXACT(imageViewIdentity);
	MUTATE_EXACT(samplerIdentity);
	MUTATE_EXACT(imageOwnerIdentity);
	MUTATE_EXACT(ordinaryDescriptorIdentity);
	MUTATE_EXACT(imageOwnerGeneration);
	MUTATE_EXACT(samplerDefinitionDigest);
	MUTATE_EXACT(setGeneration);
	MUTATE_EXACT(samplerPoolGeneration);
	MUTATE_EXACT(imagePublicationGeneration);
	MUTATE_EXACT(imageTransactionGeneration);
	MUTATE_EXACT(samplerPublicationGeneration);
	MUTATE_EXACT(samplerTransactionGeneration);
	MUTATE_EXACT(imageSlotGeneration);
	MUTATE_EXACT(samplerSlotGeneration);
	MUTATE_EXACT(imageSlot);
	MUTATE_EXACT(samplerSlot);
	MUTATE_EXACT(imageBinding);
	MUTATE_EXACT(samplerBinding);
	#undef MUTATE_EXACT
	memset(&mutation,0,sizeof(mutation));
	CHECK(!VK_BindlessPublicationReceiptExact(&mutation,&mutation));
	mutation=receipt; mutation.ready=qfalse;
	CHECK(!VK_BindlessPublicationReceiptExact(&mutation,&mutation));
	CHECK(!VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,9,views[1],
		owners[1],descriptors[1],ownerGens[1],4,samplers[1],samplerDigests[1],&snapshot));
	mutation=receipt; mutation.samplerDefinitionDigest++;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageOwnerGeneration++;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.ordinaryDescriptorIdentity++;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.setIdentity=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerPoolIdentity=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageViewIdentity=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerIdentity=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageOwnerIdentity=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.ordinaryDescriptorIdentity=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageOwnerGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerDefinitionDigest=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.setGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerPoolGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imagePublicationGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageTransactionGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerPublicationGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerTransactionGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageSlotGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerSlotGeneration=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageSlot=VK_BINDLESS_PUBLICATION_IMAGE_SLOTS;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerSlot=VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.imageBinding=1;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));
	mutation=receipt; mutation.samplerBinding=0;
	CHECK(!VK_BindlessPublicationReceiptExact(&receipt,&mutation));

	// Multi-view validation is output-atomic and slot-unique.
	ledgerSnapshot=s_ledger; imageSlots[1]=imageSlots[0];
	CHECK(!VK_BindlessPublicationViews(&s_ledger,set,imageSlots,views,owners,
		descriptors,ownerGens,kinds,2));
	CHECK(memcmp(&s_ledger,&ledgerSnapshot,sizeof(s_ledger))==0);
	imageSlots[1]=9; kinds[0]=VK_BINDLESS_PUBLICATION_TOMBSTONE;
	CHECK(!VK_BindlessPublicationViews(&s_ledger,set,imageSlots,views,owners,
		descriptors,ownerGens,kinds,2));
	CHECK(memcmp(&s_ledger,&ledgerSnapshot,sizeof(s_ledger))==0);
	kinds[0]=VK_BINDLESS_PUBLICATION_LEGACY_EXACT;

	CHECK(VK_BindlessPublicationTombstoneImage(&s_ledger,set,7));
	CHECK(s_ledger.images[7].tombstone
		&& s_ledger.images[7].slotGeneration==2);
	CHECK(!VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,7,views[0],
		owners[0],descriptors[0],ownerGens[0],3,samplers[0],samplerDigests[0],&snapshot));
	CHECK(VK_BindlessPublicationInvalidateSamplerPool(&s_ledger,pool));
	CHECK(!s_ledger.samplerPoolIdentity && !s_ledger.samplers[3].populated);
	CHECK(VK_BindlessPublicationActivateSamplerPool(&s_ledger,pool));
	CHECK(s_ledger.samplerPoolGeneration==2);
	CHECK(VK_BindlessPublicationInvalidateSet(&s_ledger,set));
	CHECK(!s_ledger.setIdentity && !s_ledger.images[9].populated);
	CHECK(VK_BindlessPublicationActivateSet(&s_ledger,set));
	CHECK(s_ledger.setGeneration==2);

	// Every non-exact publication kind is an explicit ordinary-query decline.
	for ( int kind=VK_BINDLESS_PUBLICATION_RESIDENT;
			kind<VK_BINDLESS_PUBLICATION_TOMBSTONE; ++kind ) {
		vkBindlessPublicationKind_t nonExact=(vkBindlessPublicationKind_t)kind;
		CHECK(VK_BindlessPublicationViews(&s_ledger,set,imageSlots,views,owners,
			descriptors,ownerGens,&nonExact,1));
		CHECK(!VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,7,views[0],
			owners[0],descriptors[0],ownerGens[0],3,samplers[0],samplerDigests[0],&snapshot));
	}

	// A real descriptor write followed by ledger saturation poisons every stale
	// receipt instead of leaving the previous LEGACY_EXACT tuple queryable.
	VK_BindlessPublicationInit(&s_ledger);
	CHECK(VK_BindlessPublicationActivateSamplerPool(&s_ledger,pool));
	CHECK(VK_BindlessPublicationActivateSet(&s_ledger,set));
	CHECK(VK_BindlessPublicationViews(&s_ledger,set,imageSlots,views,owners,
		descriptors,ownerGens,kinds,1));
	CHECK(VK_BindlessPublicationSamplers(&s_ledger,set,pool,samplerSlots,
		samplers,samplerDigests,1));
	CHECK(VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,7,views[0],
		owners[0],descriptors[0],ownerGens[0],3,samplers[0],samplerDigests[0],&receipt));
	s_ledger.imageSlotGenerations[7]=UINT64_MAX;
	ledgerSnapshot=s_ledger;
	CHECK(!VK_BindlessPublicationViews(&s_ledger,set,imageSlots,
		views,owners,descriptors,ownerGens,kinds,1));
	CHECK(memcmp(&s_ledger,&ledgerSnapshot,sizeof(s_ledger))==0);
	CHECK(VK_BindlessPublicationPoisonSetAfterWrite(&s_ledger,set));
	CHECK(!VK_BindlessPublicationQueryOrdinary(&s_ledger,set,pool,7,views[0],
		owners[0],descriptors[0],ownerGens[0],3,samplers[0],samplerDigests[0],&snapshot));
	puts("vk_bindless_publication_test: PASS");
	return 0;
}
