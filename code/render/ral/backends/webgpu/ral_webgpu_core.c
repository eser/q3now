// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_core.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	CORE_ADAPTER_PENDING = 1,
	CORE_DEVICE_PENDING,
	CORE_READY,
	CORE_UNAVAILABLE,
	CORE_FAILED,
	CORE_LOST
} coreState_t;

struct ralWebGpuCore_s {
	uint64_t generation;
	uint64_t adapterRequestGeneration;
	uint64_t deviceRequestGeneration;
	void *userData;
	ralWebGpuHostOps_t host;
	coreState_t state;
	uintptr_t adapterIdentity;
	uintptr_t deviceIdentity;
	uintptr_t queueIdentity;
	ralCaps_t caps;
	ralCapabilityProfile_t capabilityProfile;
	ralMemoryFailureLedger_t failureLedger;
};

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static void CopyText( char *out, size_t outSize, const char *text ) {
	memset( out, 0, outSize );
	if ( text && text[0] ) {
		strncpy( out, text, outSize - 1u );
		out[outSize - 1u] = '\0';
	}
}

static qboolean LimitsValid( const ralWebGpuAdapterLimits_t *limits ) {
	return limits && limits->maxColorAttachments >= 4u
		&& limits->maxTextureDimension2D >= 4096u
		&& limits->maxTextureDimension3D >= 256u
		&& limits->maxTextureArrayLayers >= 256u
		&& limits->maxComputeInvocationsPerWorkgroup >= 256u
		&& limits->maxSampledTexturesPerShaderStage >= 16u
		&& limits->maxBindGroups >= 4u
		&& limits->maxBindingsPerBindGroup >= 8u
		&& limits->maxStorageBufferBindingSize >= UINT64_C(134217728)
		&& limits->minUniformBufferOffsetAlignment >= 16u
		&& limits->minStorageBufferOffsetAlignment >= 16u
		&& BoolValid( limits->bindingArrays )
		&& BoolValid( limits->textureCompressionBC )
		&& BoolValid( limits->textureCompressionASTC )
		&& BoolValid( limits->textureCompressionETC2 )
		&& BoolValid( limits->timestampQueries );
}

static qboolean BuildCaps( ralWebGpuCore_t *core,
		const ralWebGpuAdapterPoll_t *adapter ) {
	const ralWebGpuAdapterLimits_t *limits = &adapter->limits;
	ralCaps_t caps;
	if ( !LimitsValid( limits ) || !adapter->deviceName
			|| !adapter->deviceName[0] ) return qfalse;
	memset( &caps, 0, sizeof( caps ) );
	caps.bindlessTextures = limits->bindingArrays;
	caps.dynamicRendering = qtrue;
	caps.maxBindlessTextures = limits->bindingArrays ? 64u : 0u;
	caps.maxColorAttachments = limits->maxColorAttachments;
	caps.maxComputeWorkgroupSize = limits->maxComputeInvocationsPerWorkgroup;
	caps.maxTextureDimension2D = limits->maxTextureDimension2D;
	caps.maxTextureDimension3D = limits->maxTextureDimension3D;
	caps.maxTextureArrayLayers = limits->maxTextureArrayLayers;
	caps.maxPushConstantSize = 256u;
	caps.minUniformBufferAlignment = limits->minUniformBufferOffsetAlignment;
	caps.minStorageBufferAlignment = limits->minStorageBufferOffsetAlignment;
	caps.timestampPeriodNs = limits->timestampQueries ? 1.0f : 0.0f;
	caps.maxSamplerAnisotropy = 1.0f;
	caps.independentBlend = qtrue;
	caps.maxStorageBufferRange = limits->maxStorageBufferBindingSize;
	caps.textureCompressionBC = limits->textureCompressionBC;
	caps.textureCompressionASTC = limits->textureCompressionASTC;
	caps.textureCompressionETC2 = limits->textureCompressionETC2;
	caps.maxSampledTexturesPerShaderStage = limits->maxSampledTexturesPerShaderStage;
	caps.maxBindGroups = limits->maxBindGroups;
	caps.adapterType = adapter->adapterType;
	caps.vendorId = adapter->vendorId;
	caps.deviceId = adapter->deviceId;
	caps.offscreenPresentation = qtrue;
	CopyText( caps.vendorName, sizeof( caps.vendorName ), adapter->vendorName );
	CopyText( caps.deviceName, sizeof( caps.deviceName ), adapter->deviceName );
	CopyText( caps.apiVersion, sizeof( caps.apiVersion ), "WebGPU" );
	CopyText( caps.driverVersion, sizeof( caps.driverVersion ), "browser-managed" );
	core->caps = caps;
	return Ral_CapabilityProfileFromCaps( RAL_BACKEND_WEBGPU, &core->caps,
		core->generation, &core->capabilityProfile );
}

static qboolean ReceiptValid( const ralWebGpuCoreReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_WEBGPU_CORE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->generation && receipt->generation != UINT64_MAX
		&& receipt->adapterRequestGeneration == receipt->generation
		&& receipt->deviceRequestGeneration == receipt->generation + 1u
		&& receipt->backendIdentity && receipt->adapterIdentity
		&& receipt->deviceIdentity && receipt->queueIdentity
		&& receipt->backendIdentity != receipt->adapterIdentity
		&& receipt->backendIdentity != receipt->deviceIdentity
		&& receipt->adapterIdentity != receipt->deviceIdentity
		&& receipt->deviceIdentity != receipt->queueIdentity
		&& receipt->caps.dynamicRendering == qtrue
		&& receipt->caps.maxColorAttachments >= 4u
		&& receipt->caps.maxTextureDimension2D >= 4096u
		&& !strcmp( receipt->caps.apiVersion, "WebGPU" )
		&& Ral_CapabilityProfileExact( &receipt->capabilityProfile,
			&receipt->capabilityProfile )
		&& receipt->ready == qtrue;
}

qboolean RalWebGpu_CoreBegin( const ralWebGpuCoreCreateInfo_t *createInfo,
		ralWebGpuCore_t **outCore ) {
	ralWebGpuCore_t *core;
	if ( !createInfo || !outCore || !createInfo->generation
			|| createInfo->generation >= UINT64_MAX - 1u
			|| !createInfo->host.beginAdapter || !createInfo->host.pollAdapter
			|| !createInfo->host.beginDevice || !createInfo->host.pollDevice
			|| !createInfo->host.releaseDevice
			|| !createInfo->host.releaseAdapter ) return qfalse;
	core = (ralWebGpuCore_t *)calloc( 1u, sizeof( *core ) );
	if ( !core ) return qfalse;
	core->generation = createInfo->generation;
	core->adapterRequestGeneration = createInfo->generation;
	core->userData = createInfo->userData;
	core->host = createInfo->host;
	Ral_MemoryFailureLedgerInit( &core->failureLedger );
	if ( !core->host.beginAdapter( core->userData,
			core->adapterRequestGeneration ) ) {
		free( core ); return qfalse;
	}
	core->state = CORE_ADAPTER_PENDING;
	*outCore = core;
	return qtrue;
}

ralWebGpuPollStatus_t RalWebGpu_CorePoll( ralWebGpuCore_t *core,
		ralWebGpuCoreReceipt_t *outReceipt ) {
	if ( !core ) return RAL_WEBGPU_POLL_FAILED;
	if ( core->state == CORE_LOST ) return RAL_WEBGPU_POLL_DEVICE_LOST;
	if ( core->state == CORE_UNAVAILABLE ) return RAL_WEBGPU_POLL_UNAVAILABLE;
	if ( core->state == CORE_FAILED ) return RAL_WEBGPU_POLL_FAILED;
	if ( core->state == CORE_ADAPTER_PENDING ) {
		ralWebGpuAdapterPoll_t adapter;
		memset( &adapter, 0, sizeof( adapter ) );
		if ( !core->host.pollAdapter( core->userData,
				core->adapterRequestGeneration, &adapter ) ) {
			core->state = CORE_FAILED; return RAL_WEBGPU_POLL_FAILED;
		}
		if ( adapter.status == RAL_WEBGPU_REQUEST_PENDING )
			return RAL_WEBGPU_POLL_PENDING;
		if ( adapter.status == RAL_WEBGPU_REQUEST_UNAVAILABLE ) {
			core->state = CORE_UNAVAILABLE; return RAL_WEBGPU_POLL_UNAVAILABLE;
		}
		if ( adapter.status != RAL_WEBGPU_REQUEST_READY
				|| !adapter.adapterIdentity ) {
			core->state = CORE_FAILED; return RAL_WEBGPU_POLL_FAILED;
		}
		core->adapterIdentity = adapter.adapterIdentity;
		if ( !BuildCaps( core, &adapter ) ) {
			core->host.releaseAdapter( core->userData, core->adapterIdentity );
			core->adapterIdentity = (uintptr_t)0;
			core->state = CORE_FAILED; return RAL_WEBGPU_POLL_FAILED;
		}
		core->deviceRequestGeneration = core->generation + 1u;
		if ( !core->host.beginDevice( core->userData, core->adapterIdentity,
				core->deviceRequestGeneration ) ) {
			core->state = CORE_FAILED; return RAL_WEBGPU_POLL_FAILED;
		}
		core->state = CORE_DEVICE_PENDING;
		return RAL_WEBGPU_POLL_PENDING;
	}
	if ( core->state == CORE_DEVICE_PENDING ) {
		ralWebGpuDevicePoll_t device;
		memset( &device, 0, sizeof( device ) );
		if ( !core->host.pollDevice( core->userData,
				core->deviceRequestGeneration, &device ) ) {
			core->state = CORE_FAILED; return RAL_WEBGPU_POLL_FAILED;
		}
		if ( device.status == RAL_WEBGPU_REQUEST_PENDING )
			return RAL_WEBGPU_POLL_PENDING;
		if ( device.status == RAL_WEBGPU_REQUEST_UNAVAILABLE ) {
			core->state = CORE_UNAVAILABLE; return RAL_WEBGPU_POLL_UNAVAILABLE;
		}
		if ( device.status != RAL_WEBGPU_REQUEST_READY
				|| !device.deviceIdentity || !device.queueIdentity
				|| device.deviceIdentity == core->adapterIdentity
				|| device.deviceIdentity == device.queueIdentity ) {
			core->state = CORE_FAILED; return RAL_WEBGPU_POLL_FAILED;
		}
		core->deviceIdentity = device.deviceIdentity;
		core->queueIdentity = device.queueIdentity;
		core->state = CORE_READY;
	}
	if ( core->state == CORE_READY ) {
		ralWebGpuCoreReceipt_t receipt;
		if ( !outReceipt ) return RAL_WEBGPU_POLL_FAILED;
		memset( &receipt, 0, sizeof( receipt ) );
		receipt.schemaVersion = RAL_WEBGPU_CORE_SCHEMA_VERSION;
		receipt.backendType = RAL_BACKEND_WEBGPU;
		receipt.generation = core->generation;
		receipt.adapterRequestGeneration = core->adapterRequestGeneration;
		receipt.deviceRequestGeneration = core->deviceRequestGeneration;
		receipt.backendIdentity = (uintptr_t)core;
		receipt.adapterIdentity = core->adapterIdentity;
		receipt.deviceIdentity = core->deviceIdentity;
		receipt.queueIdentity = core->queueIdentity;
		receipt.caps = core->caps;
		receipt.capabilityProfile = core->capabilityProfile;
		receipt.ready = qtrue;
		if ( !ReceiptValid( &receipt ) ) return RAL_WEBGPU_POLL_FAILED;
		*outReceipt = receipt;
		return RAL_WEBGPU_POLL_READY;
	}
	return RAL_WEBGPU_POLL_FAILED;
}

void RalWebGpu_CoreDestroy( ralWebGpuCore_t *core ) {
	if ( !core ) return;
	if ( core->deviceIdentity ) core->host.releaseDevice( core->userData,
		core->deviceIdentity, core->queueIdentity );
	if ( core->adapterIdentity ) core->host.releaseAdapter( core->userData,
		core->adapterIdentity );
	memset( core, 0, sizeof( *core ) );
	free( core );
}

qboolean RalWebGpu_CoreReceiptExact( const ralWebGpuCoreReceipt_t *a,
		const ralWebGpuCoreReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean RalWebGpu_CoreMatchesReceipt( const ralWebGpuCore_t *core,
		const ralWebGpuCoreReceipt_t *receipt ) {
	return core && core->state == CORE_READY && ReceiptValid( receipt )
		&& receipt->backendIdentity == (uintptr_t)core
		&& receipt->generation == core->generation
		&& receipt->adapterIdentity == core->adapterIdentity
		&& receipt->deviceIdentity == core->deviceIdentity
		&& receipt->queueIdentity == core->queueIdentity;
}

qboolean RalWebGpu_CorePublishDeviceLoss( ralWebGpuCore_t *core,
		const ralMemoryFailureEvent_t *event,
		ralMemoryFailureReceipt_t *outReceipt ) {
	ralMemoryFailureLedger_t candidate;
	ralMemoryFailureReceipt_t receipt;
	if ( !core || !event || !outReceipt || core->state != CORE_READY
			|| event->backendType != RAL_BACKEND_WEBGPU
			|| event->cause != RAL_MEMORY_FAILURE_DEVICE_LOST ) return qfalse;
	candidate = core->failureLedger;
	if ( !Ral_MemoryFailureLedgerPublish( &candidate, event )
			|| !Ral_MemoryFailureLedgerGet( &candidate, &receipt )
			|| receipt.action != RAL_MEMORY_RECOVERY_RECREATE_BACKEND
			|| receipt.retryAllowed != qfalse
			|| !Ral_MemoryFailureReceiptExact( &receipt, &receipt ) ) return qfalse;
	core->failureLedger = candidate;
	core->state = CORE_LOST;
	*outReceipt = receipt;
	return qtrue;
}
