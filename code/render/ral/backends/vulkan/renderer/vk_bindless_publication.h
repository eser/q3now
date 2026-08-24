// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_BINDLESS_PUBLICATION_H
#define WIRED_VK_BINDLESS_PUBLICATION_H

#include "../../../../../qcommon/q_shared.h"
#include <stdint.h>

#define VK_BINDLESS_PUBLICATION_IMAGE_SLOTS 4096u
#define VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS 32u

typedef enum {
	VK_BINDLESS_PUBLICATION_NONE = 0,
	VK_BINDLESS_PUBLICATION_LEGACY_EXACT,
	VK_BINDLESS_PUBLICATION_RESIDENT,
	VK_BINDLESS_PUBLICATION_COARSE,
	VK_BINDLESS_PUBLICATION_PLACEHOLDER,
	VK_BINDLESS_PUBLICATION_RESERVED,
	VK_BINDLESS_PUBLICATION_TOMBSTONE
} vkBindlessPublicationKind_t;

typedef struct {
	uintptr_t identity;
	uintptr_t ownerIdentity;
	uintptr_t descriptorIdentity;
	uint64_t ownerGeneration;
	uint64_t definitionDigest;
	uint64_t slotGeneration;
	uint64_t publicationGeneration;
	uint64_t transactionGeneration;
	vkBindlessPublicationKind_t kind;
	qboolean populated;
	qboolean tombstone;
} vkBindlessPublicationEntry_t;

typedef struct {
	uintptr_t setIdentity;
	uintptr_t samplerPoolIdentity;
	uint64_t setGeneration;
	uint64_t samplerPoolGeneration;
	uint64_t publicationGeneration;
	uint64_t transactionGeneration;
	uint64_t imageSlotGenerations[VK_BINDLESS_PUBLICATION_IMAGE_SLOTS];
	uint64_t samplerSlotGenerations[VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS];
	vkBindlessPublicationEntry_t images[VK_BINDLESS_PUBLICATION_IMAGE_SLOTS];
	vkBindlessPublicationEntry_t samplers[VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS];
} vkBindlessPublicationLedger_t;

typedef struct {
	uintptr_t setIdentity;
	uintptr_t samplerPoolIdentity;
	uintptr_t imageViewIdentity;
	uintptr_t samplerIdentity;
	uintptr_t imageOwnerIdentity;
	uintptr_t ordinaryDescriptorIdentity;
	uint64_t imageOwnerGeneration;
	uint64_t samplerDefinitionDigest;
	uint64_t setGeneration;
	uint64_t samplerPoolGeneration;
	uint64_t imagePublicationGeneration;
	uint64_t imageTransactionGeneration;
	uint64_t samplerPublicationGeneration;
	uint64_t samplerTransactionGeneration;
	uint64_t imageSlotGeneration;
	uint64_t samplerSlotGeneration;
	uint32_t imageSlot;
	uint32_t samplerSlot;
	uint32_t imageBinding;
	uint32_t samplerBinding;
	qboolean ready;
} vkBindlessOrdinaryReceipt_t;

void VK_BindlessPublicationInit( vkBindlessPublicationLedger_t *ledger );
qboolean VK_BindlessPublicationActivateSet(
	vkBindlessPublicationLedger_t *ledger, const void *setIdentity );
qboolean VK_BindlessPublicationActivateSamplerPool(
	vkBindlessPublicationLedger_t *ledger, const void *poolIdentity );
qboolean VK_BindlessPublicationInvalidateSet(
	vkBindlessPublicationLedger_t *ledger, const void *expectedSet );
qboolean VK_BindlessPublicationInvalidateSamplerPool(
	vkBindlessPublicationLedger_t *ledger, const void *expectedPool );
qboolean VK_BindlessPublicationPoisonSetAfterWrite(
	vkBindlessPublicationLedger_t *ledger, const void *expectedSet );
qboolean VK_BindlessPublicationViews( vkBindlessPublicationLedger_t *ledger,
	const void *expectedSet, const uint32_t *slots,
	const void *const *viewIdentities, const void *const *ownerIdentities,
	const void *const *descriptorIdentities, const uint64_t *ownerGenerations,
	const vkBindlessPublicationKind_t *kinds, uint32_t count );
qboolean VK_BindlessPublicationSamplers( vkBindlessPublicationLedger_t *ledger,
	const void *expectedSet, const void *expectedPool, const uint32_t *slots,
	const void *const *samplerIdentities, const uint64_t *definitionDigests,
	uint32_t count );
qboolean VK_BindlessPublicationTombstoneImage(
	vkBindlessPublicationLedger_t *ledger, const void *expectedSet,
	uint32_t slot );
qboolean VK_BindlessPublicationQueryOrdinary(
	const vkBindlessPublicationLedger_t *ledger,
	const void *expectedSet, const void *expectedPool, uint32_t imageSlot,
	const void *expectedView, const void *expectedOwner,
	const void *expectedDescriptor, uint64_t expectedOwnerGeneration,
	uint32_t samplerSlot, const void *expectedSampler,
	uint64_t expectedSamplerDefinitionDigest,
	vkBindlessOrdinaryReceipt_t *outReceipt );
qboolean VK_BindlessPublicationReceiptExact(
	const vkBindlessOrdinaryReceipt_t *a,
	const vkBindlessOrdinaryReceipt_t *b );

#endif
