// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_backend_conformance.h"
#include "ral_metal_core.h"
#include "ral_vulkan_bridge.h"
#include "ral.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "vulkan/vulkan.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct {
	SDL_Window *window;
	uint64_t backendGeneration;
} vulkanHost_t;

static uint64_t DigestBytes( const uint8_t *bytes, uint64_t count ) {
	uint64_t digest = UINT64_C(1469598103934665603);
	uint64_t i;
	for ( i = 0u; i < count; ++i ) {
		digest ^= bytes[i];
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static void *HostGetProcAddress( void *, void *nativeInstance, const char *name ) {
	PFN_vkGetInstanceProcAddr getInstanceProcAddr =
		(PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
	if ( !getInstanceProcAddr || !name ) return NULL;
	return (void *)getInstanceProcAddr( (VkInstance)nativeInstance, name );
}

static qboolean HostCreateSurface( void *, void *platformHandle,
		void *nativeInstance, uint64_t *outNativeSurface ) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if ( !platformHandle || !nativeInstance || !outNativeSurface ) return qfalse;
	if ( !SDL_Vulkan_CreateSurface( (SDL_Window *)platformHandle,
			(VkInstance)nativeInstance, NULL, &surface ) ) return qfalse;
	static_assert( sizeof( surface ) <= sizeof( *outNativeSurface ),
		"native surface handle too wide" );
	*outNativeSurface = 0u;
	memcpy( outNativeSurface, &surface, sizeof( surface ) );
	return surface != VK_NULL_HANDLE ? qtrue : qfalse;
}

static void HostLog( void *, ralLogSeverity_t severity, const char *message ) {
	if ( severity >= RAL_LOG_ERROR )
		fprintf( stderr, "RAL_CONFORMANCE_VK severity=%d %s",
			(int)severity, message ? message : "(null)\n" );
}

static ralBackend_t *CreateVulkanBackend( vulkanHost_t *host ) {
	ralBackendCreateInfo_t createInfo;
	Uint32 extensionCount = 0u;
	char const *const *extensions;
	if ( !host || !host->window ) return NULL;
	extensions = SDL_Vulkan_GetInstanceExtensions( &extensionCount );
	if ( !extensions || extensionCount == 0u ) return NULL;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.type = RAL_BACKEND_VULKAN;
	createInfo.platformHandle = host->window;
	createInfo.host.userData = host;
	createInfo.host.getProcAddress = HostGetProcAddress;
	createInfo.host.createSurface = HostCreateSurface;
	createInfo.host.log = HostLog;
	createInfo.letBackendOwnInstance = qtrue;
	createInfo.letBackendOwnDevice = qtrue;
	createInfo.preferredDeviceIndex = -1;
	createInfo.requestFeatures.wantDescriptorIndexing = qtrue;
	createInfo.requestFeatures.wantDrawIndirectCount = qtrue;
	createInfo.requestFeatures.wantIndependentBlend = qtrue;
	createInfo.requestFeatures.wantSamplerAnisotropy = qtrue;
	createInfo.platformInstanceExtensions = extensions;
	createInfo.platformInstanceExtensionCount = extensionCount;
	return Ral_CreateBackend( &createInfo );
}

static qboolean BuildCompletedTransfer( ralBackendType_t backendType,
		const ralAllocationReceipt_t *readback,
		const ralSubmissionReceipt_t *submission, uint64_t byteCount,
		ralTransferOutcome_t outcome, ralTransferReceipt_t *out ) {
	ralTransferRequest_t request;
	ralTransferReceipt_t prepared, submitted;
	memset( &request, 0, sizeof( request ) );
	request.backendType = backendType;
	request.direction = RAL_TRANSFER_READBACK;
	request.resourceKind = RAL_TRANSFER_BUFFER;
	request.resourceIdentity = readback->ownerIdentity;
	request.resourceGeneration = readback->allocationGeneration;
	request.byteSize = byteCount;
	request.byteBudget = byteCount;
	request.queue = RAL_QUEUE_GRAPHICS;
	return ( Ral_TransferPrepare( &request, submission->generation, &prepared )
		&& Ral_TransferPublish( &prepared, outcome, submission->generation,
			&submitted )
		&& Ral_TransferComplete( &submitted, submission->generation, qtrue, out ) )
		? qtrue : qfalse;
}

static qboolean RunVulkanCopy( vulkanHost_t *host,
		ralBackendConformanceReceipt_t *outReceipt ) {
	const uint64_t byteCount = 4096u;
	ralBackend_t *backend = NULL;
	ralBuffer_t *upload = NULL, *readback = NULL;
	ralTexture_t *texture = NULL;
	ralSampler_t *sampler = NULL;
	ralCommandBuffer_t *command = NULL;
	ralFence_t *fence = NULL;
	ralCapabilityProfile_t profile;
	ralAllocationReceipt_t uploadAllocation, readbackAllocation, textureAllocation;
	ralCommandReceipt_t recording, executable;
	ralSubmissionReceipt_t submission;
	ralTransferReceipt_t transfer;
	ralBackendConformanceFacts_t facts;
	ralBackendConformanceReceipt_t receipt;
	qboolean ok = qfalse;
	void *mapped;
	uint64_t digest = 0u;
	do {
		ralBufferCreateInfo_t uploadInfo, readbackInfo;
		ralTextureCreateInfo_t textureInfo;
		ralSamplerCreateInfo_t samplerInfo;
		ralBufferCopy_t copy;
		ralSubmitInfo_t submit;
		ralCommandBuffer_t *commands[1];
		backend = CreateVulkanBackend( host );
		if ( !backend ) break;
		if ( !Ral_CapabilityProfileFromCaps( RAL_BACKEND_VULKAN,
				Ral_GetCaps( backend ), host->backendGeneration, &profile ) ) break;
		memset( &uploadInfo, 0, sizeof( uploadInfo ) );
		uploadInfo.size = byteCount;
		uploadInfo.usage = (ralBufferUsage_t)( RAL_BUFFER_TRANSFER_SRC
			| RAL_BUFFER_MAP_WRITE );
		uploadInfo.memory = RAL_MEMORY_HOST_COHERENT;
		uploadInfo.debugName = "ral.conformance.vulkan.upload";
		upload = Ral_CreateBuffer( backend, &uploadInfo );
		memset( &readbackInfo, 0, sizeof( readbackInfo ) );
		readbackInfo.size = byteCount;
		readbackInfo.usage = (ralBufferUsage_t)( RAL_BUFFER_TRANSFER_DST
			| RAL_BUFFER_MAP_READ );
		readbackInfo.memory = RAL_MEMORY_HOST_COHERENT;
		readbackInfo.debugName = "ral.conformance.vulkan.readback";
		readback = Ral_CreateBuffer( backend, &readbackInfo );
		memset( &textureInfo, 0, sizeof( textureInfo ) );
		textureInfo.type = RAL_TEXTURE_2D;
		textureInfo.format = RAL_FORMAT_R8G8B8A8_UNORM;
		textureInfo.width = 4u; textureInfo.height = 4u;
		textureInfo.depthOrArrayLayers = 1u;
		textureInfo.mipLevels = 1u; textureInfo.sampleCount = 1u;
		textureInfo.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_SAMPLED
			| RAL_TEXTURE_USAGE_TRANSFER_DST );
		textureInfo.memory = RAL_MEMORY_DEVICE_LOCAL;
		textureInfo.debugName = "ral.conformance.vulkan.texture";
		texture = Ral_CreateTexture( backend, &textureInfo );
		memset( &samplerInfo, 0, sizeof( samplerInfo ) );
		samplerInfo.minFilter = samplerInfo.magFilter = RAL_FILTER_LINEAR;
		samplerInfo.mipmapMode = RAL_MIPMAP_NEAREST;
		samplerInfo.addressU = samplerInfo.addressV = samplerInfo.addressW
			= RAL_ADDRESS_CLAMP_TO_EDGE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.compareOp = RAL_COMPARE_ALWAYS;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.debugName = "ral.conformance.vulkan.sampler";
		sampler = Ral_CreateSampler( backend, &samplerInfo );
		if ( !upload || !readback || !texture || !sampler ) break;
		mapped = Ral_MapBuffer( upload );
		if ( !mapped ) break;
		for ( uint64_t i = 0u; i < byteCount; ++i )
			((uint8_t *)mapped)[i] = (uint8_t)( ( i * 37u + 11u ) & 0xffu );
		Ral_FlushBuffer( upload, 0u, byteCount );
		Ral_UnmapBuffer( upload );
		mapped = Ral_MapBuffer( readback );
		if ( !mapped ) break;
		memset( mapped, 0, (size_t)byteCount );
		Ral_UnmapBuffer( readback );
		command = Ral_AcquireCommandBuffer( backend, RAL_QUEUE_GRAPHICS );
		if ( !command || Ral_BeginCommandBufferExact( command, &recording ) != ralSuccess )
			break;
		memset( &copy, 0, sizeof( copy ) ); copy.size = byteCount;
		Ral_CmdCopyBuffer( command, upload, readback, &copy );
		if ( Ral_EndCommandBufferExact( command, &recording, &executable )
				!= ralSuccess ) break;
		fence = Ral_CreateFence( backend );
		if ( !fence ) break;
		memset( &submit, 0, sizeof( submit ) );
		commands[0] = command;
		submit.commandBuffers = commands; submit.numCommandBuffers = 1u;
		submit.signalFence = fence;
		if ( Ral_SubmitExact( backend, RAL_QUEUE_GRAPHICS, &submit,
				&executable, &submission ) != ralSuccess ) break;
		Ral_WaitFence( fence, RAL_TIMEOUT_INFINITE );
		if ( !Ral_FenceSignaled( fence )
				|| Ral_WaitQueueIdle( backend, RAL_QUEUE_GRAPHICS ) != ralSuccess )
			break;
		mapped = Ral_MapBuffer( readback );
		if ( !mapped ) break;
		digest = DigestBytes( (const uint8_t *)mapped, byteCount );
		for ( uint64_t i = 0u; i < byteCount; ++i ) {
			if ( ((const uint8_t *)mapped)[i]
					!= (uint8_t)( ( i * 37u + 11u ) & 0xffu ) ) {
				digest = 0u; break;
			}
		}
		Ral_UnmapBuffer( readback );
		if ( digest == 0u
				|| !Ral_BufferGetAllocationReceipt( upload, &uploadAllocation )
				|| !Ral_BufferGetAllocationReceipt( readback, &readbackAllocation )
				|| !Ral_TextureGetAllocationReceipt( texture, &textureAllocation )
				|| !BuildCompletedTransfer( RAL_BACKEND_VULKAN,
					&readbackAllocation, &submission, byteCount,
					RAL_TRANSFER_OUTCOME_NATIVE_ASYNC, &transfer ) ) break;
		memset( &facts, 0, sizeof( facts ) );
		facts.backendType = RAL_BACKEND_VULKAN;
		facts.backendGeneration = host->backendGeneration;
		facts.backendIdentity = (uintptr_t)backend;
		facts.deviceIdentity = (uintptr_t)Ral_GetDeviceHandle( backend );
		facts.graphicsQueueIdentity = (uintptr_t)Ral_GetQueueHandle( backend,
			RAL_QUEUE_GRAPHICS );
		facts.capabilities = &profile;
		facts.uploadAllocation = &uploadAllocation;
		facts.readbackAllocation = &readbackAllocation;
		facts.textureAllocation = &textureAllocation;
		facts.samplerIdentity = (uintptr_t)sampler;
		facts.samplerGeneration = textureAllocation.allocationGeneration + 1u;
		facts.submission = &submission;
		facts.transfer = &transfer;
		facts.completionGeneration = submission.generation;
		facts.copiedByteCount = byteCount;
		facts.copiedByteDigest = digest;
		if ( !Ral_BackendConformanceBuild( &facts, &receipt ) ) break;
		*outReceipt = receipt;
		ok = qtrue;
	} while ( 0 );
	if ( fence ) Ral_DestroyFence( fence );
	if ( command ) Ral_DestroyCommandBuffer( command );
	if ( sampler ) Ral_DestroySampler( sampler );
	if ( texture ) Ral_DestroyTexture( texture );
	if ( readback ) Ral_DestroyBuffer( readback );
	if ( upload ) Ral_DestroyBuffer( upload );
	if ( backend ) {
		(void)Ral_WaitIdleAndDrainDeferred( backend );
		Ral_DestroyBackend( backend );
	}
	return ok;
}

static qboolean RunMetalCopy( uint64_t generation,
		ralMetalCore_t **outCore, ralMetalCoreReceipt_t *outCoreReceipt,
		ralBackendConformanceReceipt_t *outReceipt ) {
	ralMetalCoreCreateInfo_t createInfo;
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralMetalOffscreenReceipt_t offscreen;
	ralBackendConformanceFacts_t facts;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.generation = generation;
	if ( !RalMetal_CoreCreate( &createInfo, &core, &coreReceipt )
			|| !RalMetal_OffscreenConformance( core, 4096u, &offscreen ) ) {
		RalMetal_CoreDestroy( core );
		return qfalse;
	}
	memset( &facts, 0, sizeof( facts ) );
	facts.backendType = RAL_BACKEND_METAL;
	facts.backendGeneration = generation;
	facts.backendIdentity = coreReceipt.backendIdentity;
	facts.deviceIdentity = coreReceipt.deviceIdentity;
	facts.graphicsQueueIdentity = coreReceipt.queueIdentity;
	facts.capabilities = &coreReceipt.capabilityProfile;
	facts.uploadAllocation = &offscreen.uploadAllocation;
	facts.readbackAllocation = &offscreen.readbackAllocation;
	facts.textureAllocation = &offscreen.textureAllocation;
	facts.samplerIdentity = offscreen.samplerIdentity;
	facts.samplerGeneration = offscreen.samplerGeneration;
	facts.submission = &offscreen.submission;
	facts.transfer = &offscreen.transfer;
	facts.completionGeneration = offscreen.completionGeneration;
	facts.copiedByteCount = offscreen.copiedByteCount;
	facts.copiedByteDigest = offscreen.copiedByteDigest;
	if ( !Ral_BackendConformanceBuild( &facts, outReceipt ) ) {
		RalMetal_CoreDestroy( core );
		return qfalse;
	}
	*outCore = core;
	*outCoreReceipt = coreReceipt;
	return qtrue;
}

static ralMemoryFailureEvent_t DeviceLossEvent( ralBackendType_t backendType ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = backendType;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 4096u;
	event.attempt = 1u; event.maxAttempts = 1u;
	event.liveParent = qtrue;
	return event;
}

static int CheckReceiptMutations( const ralBackendConformanceReceipt_t *receipt ) {
	ralBackendConformanceReceipt_t mutated;
	ralBackendConformanceFacts_t facts;
	ralBackendConformanceReceipt_t out, before;
	CHECK( Ral_BackendConformanceReceiptExact( receipt, receipt ) );
#define MUTATE(field) do { mutated = *receipt; mutated.field++; \
	CHECK( !Ral_BackendConformanceReceiptExact( receipt, &mutated ) ); } while ( 0 )
	MUTATE( backendGeneration );
	MUTATE( capabilities.generation );
	MUTATE( uploadAllocation.allocationGeneration );
	MUTATE( readbackAllocation.allocationGeneration );
	MUTATE( textureAllocation.allocationGeneration );
	MUTATE( samplerGeneration );
	MUTATE( submission.generation );
	MUTATE( transfer.request.resourceGeneration );
	MUTATE( completionGeneration );
	MUTATE( copiedByteCount );
	MUTATE( copiedByteDigest );
#undef MUTATE
	mutated = *receipt;
	mutated.readbackAllocation.ownerIdentity
		= mutated.uploadAllocation.ownerIdentity;
	CHECK( !Ral_BackendConformanceReceiptExact( &mutated, &mutated ) );
	mutated = *receipt; mutated.ready = qfalse;
	CHECK( !Ral_BackendConformanceReceiptExact( &mutated, &mutated ) );
	memset( &facts, 0, sizeof( facts ) );
	facts.backendType = receipt->backendType;
	facts.backendGeneration = receipt->backendGeneration;
	facts.backendIdentity = 0u;
	facts.deviceIdentity = receipt->deviceIdentity;
	facts.graphicsQueueIdentity = receipt->graphicsQueueIdentity;
	facts.capabilities = &receipt->capabilities;
	facts.uploadAllocation = &receipt->uploadAllocation;
	facts.readbackAllocation = &receipt->readbackAllocation;
	facts.textureAllocation = &receipt->textureAllocation;
	facts.samplerIdentity = receipt->samplerIdentity;
	facts.samplerGeneration = receipt->samplerGeneration;
	facts.submission = &receipt->submission;
	facts.transfer = &receipt->transfer;
	facts.completionGeneration = receipt->completionGeneration;
	facts.copiedByteCount = receipt->copiedByteCount;
	facts.copiedByteDigest = receipt->copiedByteDigest;
	memset( &out, 0x5a, sizeof( out ) ); before = out;
	CHECK( !Ral_BackendConformanceBuild( &facts, &out ) );
	CHECK( memcmp( &out, &before, sizeof( out ) ) == 0 );
	return 0;
}

static int CheckRecreate( const ralBackendConformanceReceipt_t *previous,
		const ralMemoryFailureReceipt_t *loss,
		const ralBackendConformanceReceipt_t *replacement ) {
	ralBackendRecreateReceipt_t receipt, exact, out, before;
	ralBackendConformanceReceipt_t badReplacement;
	CHECK( Ral_BackendRecreateBuild( previous, loss, replacement, &receipt ) );
	exact = receipt;
	CHECK( Ral_BackendRecreateReceiptExact( &receipt, &exact ) );
	exact.replacement.backendGeneration = exact.previous.backendGeneration;
	CHECK( !Ral_BackendRecreateReceiptExact( &exact, &exact ) );
	exact = receipt; exact.loss.action = RAL_MEMORY_RECOVERY_FAIL;
	CHECK( !Ral_BackendRecreateReceiptExact( &exact, &exact ) );
	exact = receipt; exact.loss.event.backendType = RAL_BACKEND_WEBGPU;
	CHECK( !Ral_BackendRecreateReceiptExact( &exact, &exact ) );
	memset( &out, 0x5a, sizeof( out ) ); before = out;
	badReplacement = *replacement;
	badReplacement.backendGeneration = previous->backendGeneration;
	CHECK( !Ral_BackendRecreateBuild( previous, loss, &badReplacement, &out ) );
	CHECK( memcmp( &out, &before, sizeof( out ) ) == 0 );
	return 0;
}

int main( void ) {
	vulkanHost_t vulkanHost;
	ralBackendConformanceReceipt_t vulkanFirst, vulkanSecond;
	ralBackendConformanceReceipt_t metalFirst, metalSecond, fallback;
	ralMemoryFailureReceipt_t vulkanLoss, metalLoss;
	ralMemoryFailureEvent_t event;
	ralMetalCore_t *metalCore = NULL, *metalReplacement = NULL;
	ralMetalCoreReceipt_t metalCoreReceipt, metalReplacementReceipt;
	memset( &vulkanHost, 0, sizeof( vulkanHost ) );
	CHECK( SDL_Init( SDL_INIT_VIDEO ) );
	vulkanHost.window = SDL_CreateWindow( "Wired RAL backend conformance",
		64, 64, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN );
	CHECK( vulkanHost.window != NULL );
	vulkanHost.backendGeneration = 11u;
	CHECK( RunVulkanCopy( &vulkanHost, &vulkanFirst ) );
	vulkanHost.backendGeneration = 12u;
	CHECK( RunVulkanCopy( &vulkanHost, &vulkanSecond ) );
	CHECK( RunMetalCopy( 21u, &metalCore, &metalCoreReceipt, &metalFirst ) );
	event = DeviceLossEvent( RAL_BACKEND_METAL );
	CHECK( RalMetal_CorePublishDeviceLoss( metalCore, &event, &metalLoss ) );
	RalMetal_CoreDestroy( metalCore ); metalCore = NULL;
	CHECK( RunMetalCopy( 22u, &metalReplacement, &metalReplacementReceipt,
		&metalSecond ) );
	CHECK( metalCoreReceipt.backendType == metalReplacementReceipt.backendType );
	event = DeviceLossEvent( RAL_BACKEND_VULKAN );
	CHECK( Ral_MemoryFailureDecide( &event, 1u, &vulkanLoss ) );
	CHECK( CheckReceiptMutations( &vulkanFirst ) == 0 );
	CHECK( CheckReceiptMutations( &metalFirst ) == 0 );
	CHECK( CheckRecreate( &vulkanFirst, &vulkanLoss, &vulkanSecond ) == 0 );
	CHECK( CheckRecreate( &metalFirst, &metalLoss, &metalSecond ) == 0 );
	CHECK( Ral_BackendConformanceCompatible( &vulkanFirst, &metalFirst ) );
	CHECK( vulkanFirst.capabilities.entries[RAL_CAP_INLINE_DATA].outcome
		== RAL_CAP_OUTCOME_NATIVE );
	CHECK( metalFirst.capabilities.entries[RAL_CAP_INLINE_DATA].outcome
		== RAL_CAP_OUTCOME_EMULATED );
	fallback = metalFirst;
	if ( fallback.capabilities.entries[RAL_CAP_ASYNC_COMPUTE].outcome
			== RAL_CAP_OUTCOME_EMULATED ) {
		fallback.capabilities.entries[RAL_CAP_ASYNC_COMPUTE].outcome
			= RAL_CAP_OUTCOME_NATIVE;
	} else {
		fallback.capabilities.entries[RAL_CAP_ASYNC_COMPUTE].outcome
			= RAL_CAP_OUTCOME_EMULATED;
	}
	fallback.capabilities.entries[RAL_CAP_ASYNC_COMPUTE].limit = 1u;
	CHECK( Ral_BackendConformanceCompatible( &vulkanFirst, &fallback ) );
	fallback.capabilities.entries[RAL_CAP_DYNAMIC_RENDERING].outcome
		= RAL_CAP_OUTCOME_DISABLED;
	fallback.capabilities.entries[RAL_CAP_DYNAMIC_RENDERING].limit = 0u;
	CHECK( !Ral_BackendConformanceCompatible( &vulkanFirst, &fallback ) );
	RalMetal_CoreDestroy( metalReplacement );
	SDL_DestroyWindow( vulkanHost.window );
	SDL_Quit();
	puts( "ral live Vulkan + native Metal conformance: PASS" );
	return 0;
}
