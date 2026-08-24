// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_core.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uint32_t beginAdapterCount, pollAdapterCount;
	uint32_t beginDeviceCount, pollDeviceCount;
	uint32_t releaseAdapterCount, releaseDeviceCount;
	uint32_t adapterPendingPolls, devicePendingPolls;
	qboolean unavailable, badLimits, beginDeviceFails;
} fakeHost_t;

static qboolean BeginAdapter( void *user, uint64_t generation ) {
	fakeHost_t *host = (fakeHost_t *)user;
	host->beginAdapterCount++;
	return generation > 0u ? qtrue : qfalse;
}

static qboolean PollAdapter( void *user, uint64_t generation,
		ralWebGpuAdapterPoll_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	(void)generation; host->pollAdapterCount++;
	memset( out, 0, sizeof( *out ) );
	if ( host->adapterPendingPolls > 0u ) {
		host->adapterPendingPolls--;
		out->status = RAL_WEBGPU_REQUEST_PENDING; return qtrue;
	}
	if ( host->unavailable ) {
		out->status = RAL_WEBGPU_REQUEST_UNAVAILABLE; return qtrue;
	}
	out->status = RAL_WEBGPU_REQUEST_READY;
	out->adapterIdentity = (uintptr_t)0x2000u;
	out->adapterType = RAL_ADAPTER_TYPE_DISCRETE;
	out->vendorId = 0x1234u; out->deviceId = 0x5678u;
	out->vendorName = "Wired Test Vendor";
	out->deviceName = "Wired WebGPU Test Adapter";
	out->limits.maxColorAttachments = 8u;
	out->limits.maxTextureDimension2D = host->badLimits ? 2048u : 8192u;
	out->limits.maxTextureDimension3D = 2048u;
	out->limits.maxTextureArrayLayers = 2048u;
	out->limits.maxComputeInvocationsPerWorkgroup = 256u;
	out->limits.maxSampledTexturesPerShaderStage = 32u;
	out->limits.maxBindGroups = 4u;
	out->limits.maxBindingsPerBindGroup = 16u;
	out->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
	out->limits.minUniformBufferOffsetAlignment = 256u;
	out->limits.minStorageBufferOffsetAlignment = 256u;
	out->limits.bindingArrays = qfalse;
	out->limits.textureCompressionETC2 = qtrue;
	out->limits.timestampQueries = qtrue;
	return qtrue;
}

static qboolean BeginDevice( void *user, uintptr_t adapter, uint64_t generation ) {
	fakeHost_t *host = (fakeHost_t *)user;
	host->beginDeviceCount++;
	return !host->beginDeviceFails && adapter == (uintptr_t)0x2000u
		&& generation > 1u ? qtrue : qfalse;
}

static qboolean PollDevice( void *user, uint64_t generation,
		ralWebGpuDevicePoll_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	(void)generation; host->pollDeviceCount++;
	memset( out, 0, sizeof( *out ) );
	if ( host->devicePendingPolls > 0u ) {
		host->devicePendingPolls--;
		out->status = RAL_WEBGPU_REQUEST_PENDING; return qtrue;
	}
	out->status = RAL_WEBGPU_REQUEST_READY;
	out->deviceIdentity = (uintptr_t)0x3000u;
	out->queueIdentity = (uintptr_t)0x4000u;
	return qtrue;
}

static void ReleaseDevice( void *user, uintptr_t device, uintptr_t queue ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device == (uintptr_t)0x3000u && queue == (uintptr_t)0x4000u )
		host->releaseDeviceCount++;
}

static void ReleaseAdapter( void *user, uintptr_t adapter ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( adapter == (uintptr_t)0x2000u ) host->releaseAdapterCount++;
}

static ralWebGpuCoreCreateInfo_t Info( fakeHost_t *host, uint64_t generation ) {
	ralWebGpuCoreCreateInfo_t info;
	memset( &info, 0, sizeof( info ) );
	info.generation = generation; info.userData = host;
	info.host.beginAdapter = BeginAdapter; info.host.pollAdapter = PollAdapter;
	info.host.beginDevice = BeginDevice; info.host.pollDevice = PollDevice;
	info.host.releaseDevice = ReleaseDevice; info.host.releaseAdapter = ReleaseAdapter;
	return info;
}

static ralMemoryFailureEvent_t DeviceLoss( void ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = RAL_BACKEND_WEBGPU;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 4096u; event.attempt = 1u; event.maxAttempts = 1u;
	event.liveParent = qtrue;
	return event;
}

int main( void ) {
	fakeHost_t host = { 0 }, unavailable = { 0 }, invalid = { 0 };
	ralWebGpuCore_t *core = NULL, *replacement = NULL, *other = NULL;
	ralWebGpuCoreCreateInfo_t info;
	ralWebGpuCoreReceipt_t receipt, exact, sentinel, output;
	ralMemoryFailureEvent_t event;
	ralMemoryFailureReceipt_t loss;
	host.adapterPendingPolls = 1u; host.devicePendingPolls = 1u;
	info = Info( &host, 7u );
	CHECK( RalWebGpu_CoreBegin( &info, &core ) );
	memset( &sentinel, 0xa5, sizeof( sentinel ) ); output = sentinel;
	CHECK( RalWebGpu_CorePoll( core, &output ) == RAL_WEBGPU_POLL_PENDING );
	CHECK( !memcmp( &output, &sentinel, sizeof( output ) ) );
	CHECK( RalWebGpu_CorePoll( core, &output ) == RAL_WEBGPU_POLL_PENDING );
	CHECK( host.beginDeviceCount == 1u );
	CHECK( RalWebGpu_CorePoll( core, &output ) == RAL_WEBGPU_POLL_PENDING );
	CHECK( RalWebGpu_CorePoll( core, &receipt ) == RAL_WEBGPU_POLL_READY );
	CHECK( RalWebGpu_CoreMatchesReceipt( core, &receipt ) );
	CHECK( receipt.caps.maxTextureDimension2D == 8192u );
	CHECK( receipt.caps.adapterType == RAL_ADAPTER_TYPE_DISCRETE );
	CHECK( receipt.capabilityProfile.entries[RAL_CAP_INLINE_DATA].outcome
		== RAL_CAP_OUTCOME_EMULATED );
	CHECK( receipt.capabilityProfile.entries[RAL_CAP_ASYNC_COMPUTE].outcome
		== RAL_CAP_OUTCOME_EMULATED );
	CHECK( receipt.capabilityProfile.entries[RAL_CAP_BINDING_ARRAYS].outcome
		== RAL_CAP_OUTCOME_EMULATED );
	exact = receipt;
	CHECK( RalWebGpu_CoreReceiptExact( &receipt, &exact ) );
	exact.queueIdentity++;
	CHECK( !RalWebGpu_CoreReceiptExact( &receipt, &exact ) );
	event = DeviceLoss();
	CHECK( RalWebGpu_CorePublishDeviceLoss( core, &event, &loss ) );
	CHECK( loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND );
	CHECK( RalWebGpu_CorePoll( core, &output ) == RAL_WEBGPU_POLL_DEVICE_LOST );
	CHECK( !RalWebGpu_CorePublishDeviceLoss( core, &event, &loss ) );
	info = Info( &host, 8u );
	CHECK( RalWebGpu_CoreBegin( &info, &replacement ) );
	while ( RalWebGpu_CorePoll( replacement, &output ) == RAL_WEBGPU_POLL_PENDING ) {}
	CHECK( RalWebGpu_CoreMatchesReceipt( replacement, &output ) );
	CHECK( output.generation == receipt.generation + 1u );

	unavailable.unavailable = qtrue; info = Info( &unavailable, 1u );
	CHECK( RalWebGpu_CoreBegin( &info, &other ) );
	CHECK( RalWebGpu_CorePoll( other, &sentinel ) == RAL_WEBGPU_POLL_UNAVAILABLE );
	CHECK( unavailable.beginDeviceCount == 0u );
	RalWebGpu_CoreDestroy( other ); other = NULL;
	invalid.badLimits = qtrue; info = Info( &invalid, 2u );
	CHECK( RalWebGpu_CoreBegin( &info, &other ) );
	CHECK( RalWebGpu_CorePoll( other, &sentinel ) == RAL_WEBGPU_POLL_FAILED );
	CHECK( invalid.beginDeviceCount == 0u );
	RalWebGpu_CoreDestroy( other );
	RalWebGpu_CoreDestroy( replacement );
	RalWebGpu_CoreDestroy( core );
	CHECK( host.releaseDeviceCount == 2u && host.releaseAdapterCount == 2u );
	puts( "ral WebGPU async core contract: PASS" );
	return 0;
}
