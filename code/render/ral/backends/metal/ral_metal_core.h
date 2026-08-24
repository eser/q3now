// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_CORE_H
#define WIRED_RAL_METAL_CORE_H

#include "ral_allocation.h"
#include "ral_capability.h"
#include "ral_command_lifecycle.h"
#include "ral_memory_recovery.h"
#include "ral_resource.h"
#include "ral_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_CORE_SCHEMA_VERSION 1u

typedef struct ralMetalCore_s ralMetalCore_t;

typedef struct {
	uint64_t generation;
} ralMetalCoreCreateInfo_t;

// Native identities are opaque equality tokens. Consumers must never
// dereference them; Metal objects and their ownership remain backend-local.
typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t generation;
	uintptr_t backendIdentity;
	uintptr_t deviceIdentity;
	uintptr_t queueIdentity;
	uint32_t argumentBufferTier;
	ralCaps_t caps;
	ralCapabilityProfile_t capabilityProfile;
	qboolean ready;
} ralMetalCoreReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	ralAllocationReceipt_t uploadAllocation;
	ralAllocationReceipt_t readbackAllocation;
	ralAllocationReceipt_t textureAllocation;
	uintptr_t samplerIdentity;
	uint64_t samplerGeneration;
	ralSubmissionReceipt_t submission;
	ralTransferReceipt_t transfer;
	uint64_t completionGeneration;
	uint64_t copiedByteCount;
	uint64_t copiedByteDigest;
	qboolean ready;
} ralMetalOffscreenReceipt_t;

qboolean RalMetal_CoreCreate( const ralMetalCoreCreateInfo_t *createInfo,
	ralMetalCore_t **outCore, ralMetalCoreReceipt_t *outReceipt );
void RalMetal_CoreDestroy( ralMetalCore_t *core );
qboolean RalMetal_CoreReceiptExact( const ralMetalCoreReceipt_t *a,
	const ralMetalCoreReceipt_t *b );
qboolean RalMetal_TextureFormatSupportsFeatures( const ralMetalCore_t *core,
	ralFormat_t format, ralTextureFormatFeatures_t features );
qboolean RalMetal_OffscreenConformance( ralMetalCore_t *core,
	uint64_t byteCount, ralMetalOffscreenReceipt_t *outReceipt );
qboolean RalMetal_OffscreenReceiptExact( const ralMetalOffscreenReceipt_t *a,
	const ralMetalOffscreenReceipt_t *b );
qboolean RalMetal_CorePublishDeviceLoss( ralMetalCore_t *core,
	const ralMemoryFailureEvent_t *event, ralMemoryFailureReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
