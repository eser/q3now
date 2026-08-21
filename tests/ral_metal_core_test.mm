// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_core.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralMetalCoreCreateInfo_t createInfo;
	ralMetalCore_t *core = (ralMetalCore_t *)(uintptr_t)0x1234u;
	ralMetalCoreReceipt_t coreReceipt, exactCore, beforeCore;
	ralMetalOffscreenReceipt_t offscreen, exactOffscreen, beforeOffscreen, second;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t loss, exactLoss, beforeLoss;
	memset( &createInfo, 0, sizeof( createInfo ) );
	memset( &coreReceipt, 0x5a, sizeof( coreReceipt ) );
	beforeCore = coreReceipt;
	CHECK( !RalMetal_CoreCreate( &createInfo, &core, &coreReceipt ) );
	CHECK( core == (ralMetalCore_t *)(uintptr_t)0x1234u );
	CHECK( memcmp( &coreReceipt, &beforeCore, sizeof( coreReceipt ) ) == 0 );
	createInfo.generation = UINT64_MAX;
	CHECK( !RalMetal_CoreCreate( &createInfo, &core, &coreReceipt ) );
	createInfo.generation = 7u;
	CHECK( RalMetal_CoreCreate( &createInfo, &core, &coreReceipt ) );
	CHECK( core != NULL && coreReceipt.backendType == RAL_BACKEND_METAL );
	CHECK( coreReceipt.caps.dynamicRendering == qtrue );
	CHECK( coreReceipt.capabilityProfile.backendType == RAL_BACKEND_METAL );
	CHECK( coreReceipt.capabilityProfile.entries[RAL_CAP_INLINE_DATA].outcome
		== RAL_CAP_OUTCOME_EMULATED );
	exactCore = coreReceipt;
	CHECK( RalMetal_CoreReceiptExact( &coreReceipt, &exactCore ) );
	exactCore.generation++;
	CHECK( !RalMetal_CoreReceiptExact( &coreReceipt, &exactCore ) );
	exactCore = coreReceipt; exactCore.queueIdentity = exactCore.deviceIdentity;
	CHECK( !RalMetal_CoreReceiptExact( &exactCore, &exactCore ) );
	exactCore = coreReceipt; exactCore.capabilityProfile.generation++;
	CHECK( !RalMetal_CoreReceiptExact( &coreReceipt, &exactCore ) );

	memset( &offscreen, 0x5a, sizeof( offscreen ) );
	beforeOffscreen = offscreen;
	CHECK( !RalMetal_OffscreenConformance( core, 0u, &offscreen ) );
	CHECK( memcmp( &offscreen, &beforeOffscreen, sizeof( offscreen ) ) == 0 );
	CHECK( RalMetal_OffscreenConformance( core, 4096u, &offscreen ) );
	CHECK( offscreen.ready == qtrue && offscreen.copiedByteCount == 4096u );
	CHECK( offscreen.uploadAllocation.memoryClass == RAL_ALLOCATION_UPLOAD );
	CHECK( offscreen.readbackAllocation.memoryClass == RAL_ALLOCATION_READBACK );
	CHECK( offscreen.textureAllocation.memoryClass == RAL_ALLOCATION_DEVICE_LOCAL );
	CHECK( offscreen.submission.commandCount == 1u );
	CHECK( offscreen.transfer.state == RAL_TRANSFER_COMPLETED );
	exactOffscreen = offscreen;
	CHECK( RalMetal_OffscreenReceiptExact( &offscreen, &exactOffscreen ) );
#define MUTATE(field) do { exactOffscreen = offscreen; exactOffscreen.field++; \
	CHECK( !RalMetal_OffscreenReceiptExact( &offscreen, &exactOffscreen ) ); } while (0)
	MUTATE( coreGeneration );
	MUTATE( uploadAllocation.allocationGeneration );
	MUTATE( readbackAllocation.allocationGeneration );
	MUTATE( textureAllocation.allocationGeneration );
	MUTATE( samplerGeneration );
	MUTATE( submission.generation );
	MUTATE( transfer.completionGeneration );
	MUTATE( completionGeneration );
	MUTATE( copiedByteCount );
	MUTATE( copiedByteDigest );
#undef MUTATE
	exactOffscreen = offscreen; exactOffscreen.ready = qfalse;
	CHECK( !RalMetal_OffscreenReceiptExact( &offscreen, &exactOffscreen ) );
	exactOffscreen = offscreen; exactOffscreen.transfer.request.resourceIdentity++;
	CHECK( !RalMetal_OffscreenReceiptExact( &exactOffscreen, &exactOffscreen ) );
	exactOffscreen = offscreen; exactOffscreen.submission.backendIdentity
		= (const ralBackend_t *)(uintptr_t)0x1234u;
	CHECK( !RalMetal_OffscreenReceiptExact( &exactOffscreen, &exactOffscreen ) );
	CHECK( RalMetal_OffscreenConformance( core, 256u, &second ) );
	CHECK( !RalMetal_OffscreenReceiptExact( &offscreen, &second ) );

	memset( &lossEvent, 0, sizeof( lossEvent ) );
	lossEvent.backendType = RAL_BACKEND_METAL;
	lossEvent.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	lossEvent.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	lossEvent.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	lossEvent.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	lossEvent.requestedBytes = 4096u;
	lossEvent.attempt = 1u;
	lossEvent.maxAttempts = 1u;
	lossEvent.liveParent = qtrue;
	memset( &loss, 0x5a, sizeof( loss ) ); beforeLoss = loss;
	lossEvent.backendType = RAL_BACKEND_WEBGPU;
	CHECK( !RalMetal_CorePublishDeviceLoss( core, &lossEvent, &loss ) );
	CHECK( memcmp( &loss, &beforeLoss, sizeof( loss ) ) == 0 );
	lossEvent.backendType = RAL_BACKEND_METAL;
	CHECK( RalMetal_CorePublishDeviceLoss( core, &lossEvent, &loss ) );
	CHECK( loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND );
	CHECK( loss.preserveLiveParent == qtrue && loss.retryAllowed == qfalse );
	exactLoss = loss;
	CHECK( Ral_MemoryFailureReceiptExact( &loss, &exactLoss ) );
	exactLoss.failureGeneration++;
	CHECK( !Ral_MemoryFailureReceiptExact( &loss, &exactLoss ) );
	exactLoss = loss; exactLoss.action = RAL_MEMORY_RECOVERY_DEFER;
	CHECK( !Ral_MemoryFailureReceiptExact( &exactLoss, &exactLoss ) );
	RalMetal_CoreDestroy( core );
	puts( "ral metal core: PASS" );
	return 0;
}
