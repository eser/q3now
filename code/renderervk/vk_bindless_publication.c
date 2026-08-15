// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_bindless_publication.h"

#include <string.h>

static void ClearImages( vkBindlessPublicationLedger_t *ledger ) {
	memset( ledger->images, 0, sizeof( ledger->images ) );
}

static void ClearSamplers( vkBindlessPublicationLedger_t *ledger ) {
	memset( ledger->samplers, 0, sizeof( ledger->samplers ) );
}

static qboolean Bump( uint64_t *generation ) {
	if ( !generation || *generation == UINT64_MAX ) return qfalse;
	++*generation;
	return *generation ? qtrue : qfalse;
}

static qboolean SlotsValid( const uint32_t *slots,
		const void *const *identities, uint32_t count, uint32_t limit ) {
	if ( !slots || !identities || !count || count > limit ) return qfalse;
	for ( uint32_t i = 0; i < count; ++i ) {
		if ( slots[i] >= limit || !identities[i] ) return qfalse;
		for ( uint32_t j = 0; j < i; ++j )
			if ( slots[i] == slots[j] ) return qfalse;
	}
	return qtrue;
}

void VK_BindlessPublicationInit( vkBindlessPublicationLedger_t *ledger ) {
	if ( ledger ) memset( ledger, 0, sizeof( *ledger ) );
}

qboolean VK_BindlessPublicationActivateSet(
		vkBindlessPublicationLedger_t *ledger, const void *setIdentity ) {
	uintptr_t identity = (uintptr_t)setIdentity;
	if ( !ledger || !identity || identity == ledger->samplerPoolIdentity )
		return qfalse;
	if ( ledger->setIdentity == identity ) return qtrue;
	if ( !Bump( &ledger->setGeneration ) ) return qfalse;
	ledger->setIdentity = identity;
	ClearImages( ledger );
	ClearSamplers( ledger );
	return qtrue;
}

qboolean VK_BindlessPublicationActivateSamplerPool(
		vkBindlessPublicationLedger_t *ledger, const void *poolIdentity ) {
	uintptr_t identity = (uintptr_t)poolIdentity;
	if ( !ledger || !identity || identity == ledger->setIdentity ) return qfalse;
	if ( ledger->samplerPoolIdentity == identity ) return qtrue;
	if ( !Bump( &ledger->samplerPoolGeneration ) ) return qfalse;
	ledger->samplerPoolIdentity = identity;
	ClearSamplers( ledger );
	return qtrue;
}

qboolean VK_BindlessPublicationInvalidateSet(
		vkBindlessPublicationLedger_t *ledger, const void *expectedSet ) {
	if ( !ledger || !ledger->setIdentity
			|| ledger->setIdentity != (uintptr_t)expectedSet ) return qfalse;
	ledger->setIdentity = 0;
	ClearImages( ledger );
	ClearSamplers( ledger );
	return qtrue;
}

qboolean VK_BindlessPublicationInvalidateSamplerPool(
		vkBindlessPublicationLedger_t *ledger, const void *expectedPool ) {
	if ( !ledger || !ledger->samplerPoolIdentity
			|| ledger->samplerPoolIdentity != (uintptr_t)expectedPool ) return qfalse;
	ledger->samplerPoolIdentity = 0;
	ClearSamplers( ledger );
	return qtrue;
}

qboolean VK_BindlessPublicationPoisonSetAfterWrite(
		vkBindlessPublicationLedger_t *ledger, const void *expectedSet ) {
	/* The descriptor write already happened.  If its matching publication
	 * receipt cannot be authored, no earlier slot receipt may remain usable. */
	return VK_BindlessPublicationInvalidateSet( ledger, expectedSet );
}

qboolean VK_BindlessPublicationViews( vkBindlessPublicationLedger_t *ledger,
		const void *expectedSet, const uint32_t *slots,
		const void *const *viewIdentities, const void *const *ownerIdentities,
		const void *const *descriptorIdentities, const uint64_t *ownerGenerations,
		const vkBindlessPublicationKind_t *kinds, uint32_t count ) {
	uint64_t transaction;
	if ( !ledger || !ledger->setIdentity
			|| ledger->setIdentity != (uintptr_t)expectedSet
			|| !ownerIdentities || !descriptorIdentities || !ownerGenerations || !kinds
			|| !SlotsValid( slots, viewIdentities, count,
				VK_BINDLESS_PUBLICATION_IMAGE_SLOTS )
			|| ledger->transactionGeneration == UINT64_MAX
			|| ledger->publicationGeneration > UINT64_MAX - count ) return qfalse;
	for ( uint32_t i = 0; i < count; ++i ) {
		if ( !ownerIdentities[i] || !descriptorIdentities[i]
				|| !ownerGenerations[i] || kinds[i] <= VK_BINDLESS_PUBLICATION_NONE
				|| kinds[i] >= VK_BINDLESS_PUBLICATION_TOMBSTONE
				|| ledger->imageSlotGenerations[slots[i]] == UINT64_MAX ) return qfalse;
	}
	transaction = ++ledger->transactionGeneration;
	for ( uint32_t i = 0; i < count; ++i ) {
		vkBindlessPublicationEntry_t *entry = &ledger->images[slots[i]];
		entry->identity = (uintptr_t)viewIdentities[i];
		entry->ownerIdentity = (uintptr_t)ownerIdentities[i];
		entry->descriptorIdentity = (uintptr_t)descriptorIdentities[i];
		entry->ownerGeneration = ownerGenerations[i];
		entry->slotGeneration = ++ledger->imageSlotGenerations[slots[i]];
		entry->publicationGeneration = ++ledger->publicationGeneration;
		entry->transactionGeneration = transaction;
		entry->populated = qtrue;
		entry->tombstone = qfalse;
		entry->kind = kinds[i];
	}
	return qtrue;
}

qboolean VK_BindlessPublicationSamplers( vkBindlessPublicationLedger_t *ledger,
		const void *expectedSet, const void *expectedPool, const uint32_t *slots,
		const void *const *samplerIdentities, const uint64_t *definitionDigests,
		uint32_t count ) {
	uint64_t transaction;
	if ( !ledger || !ledger->setIdentity || !ledger->samplerPoolIdentity
			|| ledger->setIdentity != (uintptr_t)expectedSet
			|| ledger->samplerPoolIdentity != (uintptr_t)expectedPool
			|| !definitionDigests
			|| !SlotsValid( slots, samplerIdentities, count,
				VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS )
			|| ledger->transactionGeneration == UINT64_MAX
			|| ledger->publicationGeneration > UINT64_MAX - count ) return qfalse;
	for ( uint32_t i = 0; i < count; ++i )
		if ( !definitionDigests[i]
				|| ledger->samplerSlotGenerations[slots[i]] == UINT64_MAX ) return qfalse;
	transaction = ++ledger->transactionGeneration;
	for ( uint32_t i = 0; i < count; ++i ) {
		vkBindlessPublicationEntry_t *entry = &ledger->samplers[slots[i]];
		entry->identity = (uintptr_t)samplerIdentities[i];
		entry->definitionDigest = definitionDigests[i];
		entry->slotGeneration = ++ledger->samplerSlotGenerations[slots[i]];
		entry->publicationGeneration = ++ledger->publicationGeneration;
		entry->transactionGeneration = transaction;
		entry->populated = qtrue;
		entry->tombstone = qfalse;
		entry->kind = VK_BINDLESS_PUBLICATION_LEGACY_EXACT;
	}
	return qtrue;
}

qboolean VK_BindlessPublicationTombstoneImage(
		vkBindlessPublicationLedger_t *ledger, const void *expectedSet,
		uint32_t slot ) {
	vkBindlessPublicationEntry_t *entry;
	if ( !ledger || !ledger->setIdentity
			|| ledger->setIdentity != (uintptr_t)expectedSet
			|| slot >= VK_BINDLESS_PUBLICATION_IMAGE_SLOTS
			|| ledger->transactionGeneration == UINT64_MAX
			|| ledger->publicationGeneration == UINT64_MAX
			|| ledger->imageSlotGenerations[slot] == UINT64_MAX ) return qfalse;
	entry = &ledger->images[slot];
	memset( entry, 0, sizeof( *entry ) );
	entry->publicationGeneration = ++ledger->publicationGeneration;
	entry->transactionGeneration = ++ledger->transactionGeneration;
	entry->slotGeneration = ++ledger->imageSlotGenerations[slot];
	entry->kind = VK_BINDLESS_PUBLICATION_TOMBSTONE;
	entry->tombstone = qtrue;
	return qtrue;
}

qboolean VK_BindlessPublicationQueryOrdinary(
		const vkBindlessPublicationLedger_t *ledger,
		const void *expectedSet, const void *expectedPool, uint32_t imageSlot,
		const void *expectedView, const void *expectedOwner,
		const void *expectedDescriptor, uint64_t expectedOwnerGeneration,
		uint32_t samplerSlot, const void *expectedSampler,
		uint64_t expectedSamplerDefinitionDigest,
		vkBindlessOrdinaryReceipt_t *outReceipt ) {
	vkBindlessOrdinaryReceipt_t candidate;
	const vkBindlessPublicationEntry_t *image, *sampler;
	if ( !ledger || !outReceipt || !expectedSet || !expectedPool || !expectedView
			|| !expectedOwner || !expectedDescriptor || !expectedOwnerGeneration
			|| !expectedSampler || !expectedSamplerDefinitionDigest
			|| imageSlot >= VK_BINDLESS_PUBLICATION_IMAGE_SLOTS
			|| samplerSlot >= VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS
			|| ledger->setIdentity != (uintptr_t)expectedSet
			|| ledger->samplerPoolIdentity != (uintptr_t)expectedPool
			|| !ledger->setGeneration || !ledger->samplerPoolGeneration ) return qfalse;
	image = &ledger->images[imageSlot]; sampler = &ledger->samplers[samplerSlot];
	if ( image->populated != qtrue || image->tombstone
			|| sampler->populated != qtrue || sampler->tombstone
			|| image->kind != VK_BINDLESS_PUBLICATION_LEGACY_EXACT
			|| image->identity != (uintptr_t)expectedView
			|| image->ownerIdentity != (uintptr_t)expectedOwner
			|| image->descriptorIdentity != (uintptr_t)expectedDescriptor
			|| image->ownerGeneration != expectedOwnerGeneration
			|| sampler->identity != (uintptr_t)expectedSampler
			|| sampler->definitionDigest != expectedSamplerDefinitionDigest
			|| !image->slotGeneration || !sampler->slotGeneration
			|| !image->publicationGeneration || !image->transactionGeneration
			|| !sampler->publicationGeneration || !sampler->transactionGeneration )
		return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.setIdentity = ledger->setIdentity;
	candidate.samplerPoolIdentity = ledger->samplerPoolIdentity;
	candidate.imageViewIdentity = image->identity;
	candidate.samplerIdentity = sampler->identity;
	candidate.imageOwnerIdentity = image->ownerIdentity;
	candidate.ordinaryDescriptorIdentity = image->descriptorIdentity;
	candidate.imageOwnerGeneration = image->ownerGeneration;
	candidate.samplerDefinitionDigest = sampler->definitionDigest;
	candidate.setGeneration = ledger->setGeneration;
	candidate.samplerPoolGeneration = ledger->samplerPoolGeneration;
	candidate.imagePublicationGeneration = image->publicationGeneration;
	candidate.imageTransactionGeneration = image->transactionGeneration;
	candidate.samplerPublicationGeneration = sampler->publicationGeneration;
	candidate.samplerTransactionGeneration = sampler->transactionGeneration;
	candidate.imageSlotGeneration = image->slotGeneration;
	candidate.samplerSlotGeneration = sampler->slotGeneration;
	candidate.imageSlot = imageSlot; candidate.samplerSlot = samplerSlot;
	candidate.imageBinding = 0u; candidate.samplerBinding = 1u;
	candidate.ready = qtrue;
	*outReceipt = candidate;
	return qtrue;
}

static qboolean ReceiptValid( const vkBindlessOrdinaryReceipt_t *r ) {
	return r && r->ready == qtrue
		&& r->setIdentity && r->samplerPoolIdentity
		&& r->imageViewIdentity && r->samplerIdentity
		&& r->imageOwnerIdentity && r->ordinaryDescriptorIdentity
		&& r->imageOwnerGeneration && r->samplerDefinitionDigest
		&& r->setGeneration && r->samplerPoolGeneration
		&& r->imagePublicationGeneration && r->imageTransactionGeneration
		&& r->samplerPublicationGeneration && r->samplerTransactionGeneration
		&& r->imageSlotGeneration && r->samplerSlotGeneration
		&& r->imageSlot < VK_BINDLESS_PUBLICATION_IMAGE_SLOTS
		&& r->samplerSlot < VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS
		&& r->imageBinding == 0u && r->samplerBinding == 1u ? qtrue : qfalse;
}

qboolean VK_BindlessPublicationReceiptExact(
		const vkBindlessOrdinaryReceipt_t *a,
		const vkBindlessOrdinaryReceipt_t *b ) {
	if ( !ReceiptValid( a ) || !ReceiptValid( b ) ) return qfalse;
	return a->setIdentity == b->setIdentity
		&& a->samplerPoolIdentity == b->samplerPoolIdentity
		&& a->imageViewIdentity == b->imageViewIdentity
		&& a->samplerIdentity == b->samplerIdentity
		&& a->imageOwnerIdentity == b->imageOwnerIdentity
		&& a->ordinaryDescriptorIdentity == b->ordinaryDescriptorIdentity
		&& a->imageOwnerGeneration == b->imageOwnerGeneration
		&& a->samplerDefinitionDigest == b->samplerDefinitionDigest
		&& a->setGeneration == b->setGeneration
		&& a->samplerPoolGeneration == b->samplerPoolGeneration
		&& a->imagePublicationGeneration == b->imagePublicationGeneration
		&& a->imageTransactionGeneration == b->imageTransactionGeneration
		&& a->samplerPublicationGeneration == b->samplerPublicationGeneration
		&& a->samplerTransactionGeneration == b->samplerTransactionGeneration
		&& a->imageSlotGeneration == b->imageSlotGeneration
		&& a->samplerSlotGeneration == b->samplerSlotGeneration
		&& a->imageSlot == b->imageSlot && a->samplerSlot == b->samplerSlot
		&& a->imageBinding == b->imageBinding
		&& a->samplerBinding == b->samplerBinding
		&& a->ready == b->ready ? qtrue : qfalse;
}
