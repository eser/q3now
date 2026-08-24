// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_core.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define RAL_METAL_COMMAND_IDENTITY_RING 16u

struct ralMetalCore_s {
	id<MTLDevice> device;
	id<MTLCommandQueue> queue;
	uint64_t generation;
	uint64_t submissionGeneration;
	uint64_t allocationGeneration;
	uint32_t commandTokenCursor;
	uint64_t commandTokens[RAL_METAL_COMMAND_IDENTITY_RING];
	ralCaps_t caps;
	ralCapabilityProfile_t capabilityProfile;
	ralMemoryFailureLedger_t failureLedger;
	qboolean deviceLost;
};

id<MTLDevice> RalMetal_CoreNativeDevice( ralMetalCore_t *core ) {
	return core ? core->device : nil;
}

id<MTLCommandQueue> RalMetal_CoreNativeQueue( ralMetalCore_t *core ) {
	return core ? core->queue : nil;
}

qboolean RalMetal_TextureFormatSupportsFeatures( const ralMetalCore_t *core,
		ralFormat_t format, ralTextureFormatFeatures_t features ) {
	ralTextureFormatFeatures_t available = 0u;
	if ( !core || !core->device || core->deviceLost == qtrue || features == 0u
			|| ( features & ~RAL_TEXTURE_FORMAT_FEATURE_ALL ) != 0u ) return qfalse;

	// Conservative Metal-family table. Publish only capabilities guaranteed by
	// the pixel-format tables for all devices accepted by this core; optional
	// families remain false until an exact device gate is added.
	switch ( format ) {
	case RAL_FORMAT_R16G16B16A16_SFLOAT:
		available = RAL_TEXTURE_FORMAT_FEATURE_SAMPLED
			| RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR
			| RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT
			| RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND
			| RAL_TEXTURE_FORMAT_FEATURE_TRANSFER_SRC
			| RAL_TEXTURE_FORMAT_FEATURE_TRANSFER_DST;
		break;
	default:
		return qfalse;
	}
	return ( available & features ) == features ? qtrue : qfalse;
}

qboolean RalMetal_CoreMatchesReceipt( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *receipt ) {
	return ( core && core->deviceLost != qtrue && receipt
		&& RalMetal_CoreReceiptExact( receipt, receipt )
		&& receipt->generation == core->generation
		&& receipt->backendIdentity == (uintptr_t)core
		&& receipt->deviceIdentity == (uintptr_t)(void *)core->device
		&& receipt->queueIdentity == (uintptr_t)(void *)core->queue ) ? qtrue : qfalse;
}

qboolean RalMetal_CoreBeginCommand( ralMetalCore_t *core, uint64_t generation,
		ralCommandLifecycle_t *outLifecycle, ralCommandReceipt_t *outRecording ) {
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording;
	uint64_t *token;
	if ( !core || !outLifecycle || !outRecording || generation == 0u
			|| generation == UINT64_MAX
			|| core->deviceLost == qtrue
			|| core->submissionGeneration == UINT64_MAX ) return qfalse;
	token = &core->commandTokens[core->commandTokenCursor];
	*token = core->generation + core->commandTokenCursor + 1u;
	Ral_CommandLifecycleInit( &lifecycle, (const ralBackend_t *)core,
		(const ralCommandBuffer_t *)token, RAL_QUEUE_GRAPHICS );
	lifecycle.generation = generation - 1u;
	if ( Ral_CommandLifecyclePublishBegin( &lifecycle, &recording ) != ralSuccess )
		return qfalse;
	*outLifecycle = lifecycle;
	*outRecording = recording;
	return qtrue;
}

qboolean RalMetal_CorePublishSubmission( ralMetalCore_t *core,
		ralCommandLifecycle_t *commandLifecycle,
		const ralCommandReceipt_t *executable,
		ralSubmissionReceipt_t *outSubmission ) {
	ralSubmissionLifecycle_t submissionLifecycle;
	ralCommandLifecycle_t *commands[1];
	ralSubmissionReceipt_t submission;
	if ( !core || !commandLifecycle || !executable || !outSubmission
			|| core->submissionGeneration == UINT64_MAX ) return qfalse;
	if ( commandLifecycle->commandIdentity != (const ralCommandBuffer_t *)
			&core->commandTokens[core->commandTokenCursor] ) return qfalse;
	Ral_SubmissionLifecycleInit( &submissionLifecycle, (const ralBackend_t *)core,
		RAL_QUEUE_GRAPHICS );
	submissionLifecycle.generation = core->submissionGeneration;
	commands[0] = commandLifecycle;
	if ( Ral_SubmissionLifecyclePublish( &submissionLifecycle, commands, executable,
			1u, &submission ) != ralSuccess ) return qfalse;
	core->submissionGeneration = submissionLifecycle.generation;
	core->commandTokenCursor = ( core->commandTokenCursor + 1u )
		% RAL_METAL_COMMAND_IDENTITY_RING;
	*outSubmission = submission;
	return qtrue;
}

static qboolean CoreReceiptValid( const ralMetalCoreReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == RAL_METAL_CORE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_METAL
		&& receipt->generation != 0u && receipt->generation != UINT64_MAX
		&& receipt->backendIdentity != (uintptr_t)0
		&& receipt->deviceIdentity != (uintptr_t)0
		&& receipt->queueIdentity != (uintptr_t)0
		&& receipt->backendIdentity != receipt->deviceIdentity
		&& receipt->backendIdentity != receipt->queueIdentity
		&& receipt->deviceIdentity != receipt->queueIdentity
		&& receipt->argumentBufferTier >= 1u && receipt->argumentBufferTier <= 2u
		&& receipt->caps.dynamicRendering == qtrue
		&& receipt->caps.maxColorAttachments >= 4u
		&& receipt->caps.maxTextureDimension2D >= 4096u
		&& receipt->caps.deviceName[0] != '\0'
		&& receipt->ready == qtrue ) ? qtrue : qfalse;
}

static void CopyDeviceName( char out[256], NSString *name ) {
	const char *utf8 = name ? [name UTF8String] : NULL;
	memset( out, 0, 256u );
	if ( utf8 ) {
		strncpy( out, utf8, 255u );
		out[255] = '\0';
	}
}

static void BuildCaps( id<MTLDevice> device, ralCaps_t *caps ) {
	memset( caps, 0, sizeof( *caps ) );
	caps->bindlessTextures = device.argumentBuffersSupport == MTLArgumentBuffersTier2
		? qtrue : qfalse;
	caps->dynamicRendering = qtrue;
	caps->maxBindlessTextures = caps->bindlessTextures ? 4096u : 64u;
	caps->maxColorAttachments = 8u;
	caps->maxComputeWorkgroupSize = 1024u;
	caps->maxTextureDimension2D = 16384u;
	caps->maxTextureDimension3D = 2048u;
	caps->maxTextureArrayLayers = 2048u;
	// RAL inline bytes lower to a backend-owned constant/uniform buffer.
	caps->maxPushConstantSize = 256u;
	caps->minUniformBufferAlignment = 256u;
	caps->minStorageBufferAlignment = 16u;
	caps->maxSamplerAnisotropy = 16.0f;
	caps->independentBlend = qtrue;
	caps->maxStorageBufferRange = (uint64_t)device.maxBufferLength;
	caps->maxSampledTexturesPerShaderStage = 128u;
	caps->maxBindGroups = 8u;
	caps->adapterType = RAL_ADAPTER_TYPE_INTEGRATED;
	caps->offscreenPresentation = qtrue;
	strncpy( caps->vendorName, "Apple Inc.", sizeof( caps->vendorName ) - 1u );
	strncpy( caps->driverVersion, "Metal", sizeof( caps->driverVersion ) - 1u );
	if ( [device respondsToSelector:@selector(recommendedMaxWorkingSetSize)] ) {
		caps->deviceLocalMemoryBytes = (uint64_t)device.recommendedMaxWorkingSetSize;
		caps->hostVisibleDeviceLocalMemoryBytes = caps->deviceLocalMemoryBytes;
	}
	if ( [device respondsToSelector:@selector(supportsBCTextureCompression)] )
		caps->textureCompressionBC = device.supportsBCTextureCompression ? qtrue : qfalse;
	if ( [device respondsToSelector:@selector(supportsFamily:)] ) {
		caps->textureCompressionASTC = [device supportsFamily:MTLGPUFamilyApple2]
			? qtrue : qfalse;
		caps->textureCompressionETC2 = caps->textureCompressionASTC;
	}
	CopyDeviceName( caps->deviceName, device.name );
	strncpy( caps->apiVersion, "Metal 3", sizeof( caps->apiVersion ) - 1u );
}

qboolean RalMetal_CoreCreate( const ralMetalCoreCreateInfo_t *createInfo,
		ralMetalCore_t **outCore, ralMetalCoreReceipt_t *outReceipt ) {
	ralMetalCore_t *candidate;
	ralMetalCoreReceipt_t receipt;
	if ( !createInfo || !outCore || !outReceipt || createInfo->generation == 0u
			|| createInfo->generation == UINT64_MAX ) return qfalse;
	@autoreleasepool {
		candidate = (ralMetalCore_t *)calloc( 1u, sizeof( *candidate ) );
		if ( !candidate ) return qfalse;
		candidate->device = [MTLCreateSystemDefaultDevice() retain];
		if ( !candidate->device ) {
			free( candidate );
			return qfalse;
		}
		candidate->queue = [candidate->device newCommandQueue];
		if ( !candidate->queue ) {
			[candidate->device release];
			free( candidate );
			return qfalse;
		}
		candidate->generation = createInfo->generation;
		Ral_MemoryFailureLedgerInit( &candidate->failureLedger );
		BuildCaps( candidate->device, &candidate->caps );
		if ( !Ral_CapabilityProfileFromCaps( RAL_BACKEND_METAL, &candidate->caps,
				candidate->generation, &candidate->capabilityProfile ) ) {
			[candidate->queue release];
			[candidate->device release];
			free( candidate );
			return qfalse;
		}
		memset( &receipt, 0, sizeof( receipt ) );
		receipt.schemaVersion = RAL_METAL_CORE_SCHEMA_VERSION;
		receipt.backendType = RAL_BACKEND_METAL;
		receipt.generation = candidate->generation;
		receipt.backendIdentity = (uintptr_t)candidate;
		receipt.deviceIdentity = (uintptr_t)(void *)candidate->device;
		receipt.queueIdentity = (uintptr_t)(void *)candidate->queue;
		receipt.argumentBufferTier = (uint32_t)candidate->device.argumentBuffersSupport + 1u;
		receipt.caps = candidate->caps;
		receipt.capabilityProfile = candidate->capabilityProfile;
		receipt.ready = qtrue;
		if ( !CoreReceiptValid( &receipt ) ) {
			[candidate->queue release];
			[candidate->device release];
			free( candidate );
			return qfalse;
		}
	}
	*outCore = candidate;
	*outReceipt = receipt;
	return qtrue;
}

void RalMetal_CoreDestroy( ralMetalCore_t *core ) {
	if ( !core ) return;
	@autoreleasepool {
		[core->queue release];
		[core->device release];
	}
	memset( core, 0, sizeof( *core ) );
	free( core );
}

qboolean RalMetal_CoreReceiptExact( const ralMetalCoreReceipt_t *a,
		const ralMetalCoreReceipt_t *b ) {
	return ( CoreReceiptValid( a ) && CoreReceiptValid( b )
		&& a->backendType == b->backendType && a->generation == b->generation
		&& a->backendIdentity == b->backendIdentity
		&& a->deviceIdentity == b->deviceIdentity
		&& a->queueIdentity == b->queueIdentity
		&& a->argumentBufferTier == b->argumentBufferTier
		&& !memcmp( &a->caps, &b->caps, sizeof( a->caps ) )
		&& Ral_CapabilityProfileExact( &a->capabilityProfile,
			&b->capabilityProfile ) ) ? qtrue : qfalse;
}

static uint64_t DigestBytes( const uint8_t *bytes, uint64_t count ) {
	uint64_t digest = UINT64_C(1469598103934665603);
	uint64_t i;
	for ( i = 0u; i < count; ++i ) {
		digest ^= bytes[i];
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static qboolean OffscreenReceiptValid( const ralMetalOffscreenReceipt_t *receipt );

static qboolean BuildAllocation( ralMetalCore_t *core, uintptr_t identity,
		ralAllocationClass_t memoryClass, uint64_t size,
		ralAllocationReceipt_t *out ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset( &request, 0, sizeof( request ) );
	memset( &facts, 0, sizeof( facts ) );
	request.memoryClass = memoryClass;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = size;
	request.alignment = 256u;
	request.ownerIdentity = identity;
	request.ownerGeneration = core->generation;
	request.allowFallback = qtrue;
	facts.backendType = RAL_BACKEND_METAL;
	facts.placement = RAL_ALLOCATION_PLACEMENT_MANAGED;
	facts.committedSize = ( size + 255u ) & ~UINT64_C(255);
	facts.actualAlignment = 256u;
	facts.allocationGeneration = ++core->allocationGeneration;
	facts.deviceLocal = memoryClass == RAL_ALLOCATION_DEVICE_LOCAL ? qtrue : qfalse;
	facts.hostVisible = ( memoryClass == RAL_ALLOCATION_UPLOAD
		|| memoryClass == RAL_ALLOCATION_READBACK ) ? qtrue : qfalse;
	facts.hostCoherent = facts.hostVisible;
	return Ral_AllocationReceiptBuild( &request, &facts, out );
}

qboolean RalMetal_OffscreenConformance( ralMetalCore_t *core,
		uint64_t byteCount, ralMetalOffscreenReceipt_t *outReceipt ) {
	ralMetalOffscreenReceipt_t receipt;
	if ( !core || !core->device || !core->queue || !outReceipt || byteCount == 0u
			|| core->deviceLost == qtrue
			|| byteCount > UINT64_C(1048576)
			|| core->submissionGeneration >= UINT64_MAX - 1u
			|| core->allocationGeneration >= UINT64_MAX - 3u ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	@autoreleasepool {
		id<MTLBuffer> upload = [core->device newBufferWithLength:(NSUInteger)byteCount
			options:MTLResourceStorageModeShared];
		id<MTLBuffer> readback = [core->device newBufferWithLength:(NSUInteger)byteCount
			options:MTLResourceStorageModeShared];
		MTLTextureDescriptor *textureInfo = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
			width:4u height:4u mipmapped:NO];
		textureInfo.storageMode = MTLStorageModePrivate;
		textureInfo.usage = MTLTextureUsageShaderRead;
		id<MTLTexture> texture = [core->device newTextureWithDescriptor:textureInfo];
		MTLSamplerDescriptor *samplerInfo = [[[MTLSamplerDescriptor alloc] init] autorelease];
		samplerInfo.minFilter = MTLSamplerMinMagFilterLinear;
		samplerInfo.magFilter = MTLSamplerMinMagFilterLinear;
		id<MTLSamplerState> sampler = [core->device newSamplerStateWithDescriptor:samplerInfo];
		id<MTLCommandBuffer> nativeCommand = [core->queue commandBuffer];
		if ( !upload || !readback || !texture || !sampler || !nativeCommand ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}
		uint8_t *sourceBytes = (uint8_t *)upload.contents;
		for ( uint64_t i = 0u; i < byteCount; ++i )
			sourceBytes[i] = (uint8_t)( ( i * 37u + 11u ) & 0xffu );
		memset( readback.contents, 0, (size_t)byteCount );

		if ( !BuildAllocation( core, (uintptr_t)(void *)upload,
				RAL_ALLOCATION_UPLOAD, byteCount, &receipt.uploadAllocation )
			|| !BuildAllocation( core, (uintptr_t)(void *)readback,
				RAL_ALLOCATION_READBACK, byteCount, &receipt.readbackAllocation )
			|| !BuildAllocation( core, (uintptr_t)(void *)texture,
				RAL_ALLOCATION_DEVICE_LOCAL, 4u * 4u * 4u, &receipt.textureAllocation ) ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}

		ralCommandLifecycle_t commandLifecycle;
		ralSubmissionLifecycle_t submissionLifecycle;
		ralCommandReceipt_t recording, executable;
		ralCommandLifecycle_t *commands[1];
		uint64_t *commandToken = &core->commandTokens[core->commandTokenCursor];
		*commandToken = core->generation + core->commandTokenCursor + 1u;
		Ral_CommandLifecycleInit( &commandLifecycle, (const ralBackend_t *)core,
			(const ralCommandBuffer_t *)commandToken, RAL_QUEUE_GRAPHICS );
		if ( Ral_CommandLifecyclePublishBegin( &commandLifecycle, &recording ) != ralSuccess ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}
		id<MTLBlitCommandEncoder> blit = [nativeCommand blitCommandEncoder];
		if ( !blit ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}
		[blit copyFromBuffer:upload sourceOffset:0u toBuffer:readback
			destinationOffset:0u size:(NSUInteger)byteCount];
		[blit endEncoding];
		if ( Ral_CommandLifecyclePublishEnd( &commandLifecycle, &recording,
				&executable ) != ralSuccess ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}

		__block qboolean completionHandlerRan = qfalse;
		[nativeCommand addCompletedHandler:^(id<MTLCommandBuffer> completed) {
			completionHandlerRan = completed.status == MTLCommandBufferStatusCompleted
				? qtrue : qfalse;
		}];
		[nativeCommand commit];
		[nativeCommand waitUntilCompleted];
		if ( nativeCommand.status != MTLCommandBufferStatusCompleted
				|| completionHandlerRan != qtrue
				|| memcmp( upload.contents, readback.contents, (size_t)byteCount ) != 0 ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}

		Ral_SubmissionLifecycleInit( &submissionLifecycle, (const ralBackend_t *)core,
			RAL_QUEUE_GRAPHICS );
		submissionLifecycle.generation = core->submissionGeneration;
		commands[0] = &commandLifecycle;
		if ( Ral_SubmissionLifecyclePublish( &submissionLifecycle, commands, &executable,
				1u, &receipt.submission ) != ralSuccess ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}
		ralTransferRequest_t transferRequest;
		ralTransferReceipt_t prepared, submitted;
		memset( &transferRequest, 0, sizeof( transferRequest ) );
		transferRequest.backendType = RAL_BACKEND_METAL;
		transferRequest.direction = RAL_TRANSFER_READBACK;
		transferRequest.resourceKind = RAL_TRANSFER_BUFFER;
		transferRequest.resourceIdentity = (uintptr_t)(void *)readback;
		transferRequest.resourceGeneration = receipt.readbackAllocation.allocationGeneration;
		transferRequest.byteSize = byteCount;
		transferRequest.byteBudget = byteCount;
		transferRequest.queue = RAL_QUEUE_GRAPHICS;
		if ( !Ral_TransferPrepare( &transferRequest, receipt.submission.generation, &prepared )
				|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_MANAGED_ASYNC,
					receipt.submission.generation, &submitted )
				|| !Ral_TransferComplete( &submitted, receipt.submission.generation,
					qtrue, &receipt.transfer ) ) {
			[upload release]; [readback release]; [texture release]; [sampler release];
			return qfalse;
		}
		receipt.schemaVersion = RAL_METAL_CORE_SCHEMA_VERSION;
		receipt.backendType = RAL_BACKEND_METAL;
		receipt.coreGeneration = core->generation;
		receipt.samplerIdentity = (uintptr_t)(void *)sampler;
		receipt.samplerGeneration = receipt.textureAllocation.allocationGeneration + 1u;
		receipt.completionGeneration = receipt.submission.generation;
		receipt.copiedByteCount = byteCount;
		receipt.copiedByteDigest = DigestBytes( (const uint8_t *)readback.contents, byteCount );
		receipt.ready = qtrue;
		core->submissionGeneration = submissionLifecycle.generation;
		core->commandTokenCursor++;
		[upload release]; [readback release]; [texture release]; [sampler release];
	}
	if ( !OffscreenReceiptValid( &receipt )
			|| !RalMetal_OffscreenReceiptExact( &receipt, &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

static qboolean OffscreenReceiptValid( const ralMetalOffscreenReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == RAL_METAL_CORE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_METAL
		&& receipt->coreGeneration != 0u && receipt->coreGeneration != UINT64_MAX
		&& receipt->uploadAllocation.backendType == RAL_BACKEND_METAL
		&& receipt->readbackAllocation.backendType == RAL_BACKEND_METAL
		&& receipt->textureAllocation.backendType == RAL_BACKEND_METAL
		&& receipt->samplerIdentity != (uintptr_t)0
		&& receipt->samplerGeneration != 0u && receipt->samplerGeneration != UINT64_MAX
		&& Ral_SubmissionReceiptValid( &receipt->submission )
		&& receipt->submission.backendIdentity != NULL
		&& receipt->transfer.state == RAL_TRANSFER_COMPLETED
		&& receipt->transfer.request.backendType == RAL_BACKEND_METAL
		&& receipt->transfer.request.resourceIdentity == receipt->readbackAllocation.ownerIdentity
		&& receipt->transfer.request.resourceGeneration
			== receipt->readbackAllocation.allocationGeneration
		&& receipt->completionGeneration == receipt->submission.generation
		&& receipt->copiedByteCount != 0u && receipt->copiedByteDigest != 0u
		&& receipt->ready == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_OffscreenReceiptExact( const ralMetalOffscreenReceipt_t *a,
		const ralMetalOffscreenReceipt_t *b ) {
	return ( OffscreenReceiptValid( a ) && OffscreenReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& Ral_AllocationReceiptExact( &a->uploadAllocation, &b->uploadAllocation )
		&& Ral_AllocationReceiptExact( &a->readbackAllocation, &b->readbackAllocation )
		&& Ral_AllocationReceiptExact( &a->textureAllocation, &b->textureAllocation )
		&& a->samplerIdentity == b->samplerIdentity
		&& a->samplerGeneration == b->samplerGeneration
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission )
		&& Ral_TransferReceiptExact( &a->transfer, &b->transfer )
		&& a->completionGeneration == b->completionGeneration
		&& a->copiedByteCount == b->copiedByteCount
		&& a->copiedByteDigest == b->copiedByteDigest && a->ready == b->ready )
		? qtrue : qfalse;
}

qboolean RalMetal_CorePublishDeviceLoss( ralMetalCore_t *core,
		const ralMemoryFailureEvent_t *event, ralMemoryFailureReceipt_t *outReceipt ) {
	ralMemoryFailureLedger_t candidate;
	ralMemoryFailureReceipt_t receipt;
	if ( !core || !event || !outReceipt || event->backendType != RAL_BACKEND_METAL
			|| event->cause != RAL_MEMORY_FAILURE_DEVICE_LOST
			|| core->deviceLost == qtrue ) return qfalse;
	candidate = core->failureLedger;
	if ( !Ral_MemoryFailureLedgerPublish( &candidate, event )
			|| !Ral_MemoryFailureLedgerGet( &candidate, &receipt )
			|| receipt.action != RAL_MEMORY_RECOVERY_RECREATE_BACKEND
			|| !Ral_MemoryFailureReceiptExact( &receipt, &receipt ) ) return qfalse;
	core->failureLedger = candidate;
	core->deviceLost = qtrue;
	*outReceipt = receipt;
	return qtrue;
}
