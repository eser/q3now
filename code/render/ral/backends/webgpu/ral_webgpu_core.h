// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_CORE_H
#define WIRED_RAL_WEBGPU_CORE_H

#include "ral_capability.h"
#include "ral_memory_recovery.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_CORE_SCHEMA_VERSION 1u

typedef struct ralWebGpuCore_s ralWebGpuCore_t;

typedef enum {
	RAL_WEBGPU_REQUEST_PENDING = 1,
	RAL_WEBGPU_REQUEST_READY,
	RAL_WEBGPU_REQUEST_UNAVAILABLE,
	RAL_WEBGPU_REQUEST_FAILED
} ralWebGpuRequestStatus_t;

typedef enum {
	RAL_WEBGPU_POLL_PENDING = 1,
	RAL_WEBGPU_POLL_READY,
	RAL_WEBGPU_POLL_UNAVAILABLE,
	RAL_WEBGPU_POLL_FAILED,
	RAL_WEBGPU_POLL_DEVICE_LOST
} ralWebGpuPollStatus_t;

typedef enum {
	RAL_WEBGPU_ASYNC_PENDING = 1,
	RAL_WEBGPU_ASYNC_READY,
	RAL_WEBGPU_ASYNC_FAILED,
	RAL_WEBGPU_ASYNC_DEVICE_LOST
} ralWebGpuAsyncStatus_t;

// Browser/native WebGPU objects remain opaque adapter-private identities.
// The later Emscripten bridge maps navigator.gpu Promise/callback completion
// into these bounded begin/poll transactions without blocking the main loop.
typedef struct {
	uint32_t maxColorAttachments;
	uint32_t maxTextureDimension2D;
	uint32_t maxTextureDimension3D;
	uint32_t maxTextureArrayLayers;
	uint32_t maxComputeInvocationsPerWorkgroup;
	uint32_t maxSampledTexturesPerShaderStage;
	uint32_t maxBindGroups;
	uint32_t maxBindingsPerBindGroup;
	uint64_t maxStorageBufferBindingSize;
	uint64_t minUniformBufferOffsetAlignment;
	uint64_t minStorageBufferOffsetAlignment;
	qboolean bindingArrays;
	qboolean textureCompressionBC;
	qboolean textureCompressionASTC;
	qboolean textureCompressionETC2;
	qboolean timestampQueries;
} ralWebGpuAdapterLimits_t;

typedef struct {
	ralWebGpuRequestStatus_t status;
	uintptr_t adapterIdentity;
	ralAdapterType_t adapterType;
	uint32_t vendorId;
	uint32_t deviceId;
	const char *vendorName;
	const char *deviceName;
	ralWebGpuAdapterLimits_t limits;
} ralWebGpuAdapterPoll_t;

typedef struct {
	ralWebGpuRequestStatus_t status;
	uintptr_t deviceIdentity;
	uintptr_t queueIdentity;
} ralWebGpuDevicePoll_t;

typedef qboolean ( *ralWebGpuBeginAdapterFn )( void *userData,
	uint64_t requestGeneration );
typedef qboolean ( *ralWebGpuPollAdapterFn )( void *userData,
	uint64_t requestGeneration, ralWebGpuAdapterPoll_t *out );
typedef qboolean ( *ralWebGpuBeginDeviceFn )( void *userData,
	uintptr_t adapterIdentity, uint64_t requestGeneration );
typedef qboolean ( *ralWebGpuPollDeviceFn )( void *userData,
	uint64_t requestGeneration, ralWebGpuDevicePoll_t *out );
typedef void ( *ralWebGpuReleaseDeviceFn )( void *userData,
	uintptr_t deviceIdentity, uintptr_t queueIdentity );
typedef void ( *ralWebGpuReleaseAdapterFn )( void *userData,
	uintptr_t adapterIdentity );

typedef struct {
	ralWebGpuBeginAdapterFn beginAdapter;
	ralWebGpuPollAdapterFn pollAdapter;
	ralWebGpuBeginDeviceFn beginDevice;
	ralWebGpuPollDeviceFn pollDevice;
	ralWebGpuReleaseDeviceFn releaseDevice;
	ralWebGpuReleaseAdapterFn releaseAdapter;
} ralWebGpuHostOps_t;

typedef struct {
	uint64_t generation;
	void *userData;
	ralWebGpuHostOps_t host;
} ralWebGpuCoreCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t generation;
	uint64_t adapterRequestGeneration;
	uint64_t deviceRequestGeneration;
	uintptr_t backendIdentity;
	uintptr_t adapterIdentity;
	uintptr_t deviceIdentity;
	uintptr_t queueIdentity;
	ralCaps_t caps;
	ralCapabilityProfile_t capabilityProfile;
	qboolean ready;
} ralWebGpuCoreReceipt_t;

qboolean RalWebGpu_CoreBegin( const ralWebGpuCoreCreateInfo_t *createInfo,
	ralWebGpuCore_t **outCore );
ralWebGpuPollStatus_t RalWebGpu_CorePoll( ralWebGpuCore_t *core,
	ralWebGpuCoreReceipt_t *outReceipt );
void RalWebGpu_CoreDestroy( ralWebGpuCore_t *core );
qboolean RalWebGpu_CoreReceiptExact( const ralWebGpuCoreReceipt_t *a,
	const ralWebGpuCoreReceipt_t *b );
qboolean RalWebGpu_CoreMatchesReceipt( const ralWebGpuCore_t *core,
	const ralWebGpuCoreReceipt_t *receipt );
qboolean RalWebGpu_CorePublishDeviceLoss( ralWebGpuCore_t *core,
	const ralMemoryFailureEvent_t *event, ralMemoryFailureReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
