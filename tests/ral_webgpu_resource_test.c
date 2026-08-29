// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_resource.h"
#include "ral_webgpu_lighting.h"
#include "ral_lighting_product.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uint32_t adapterPolls, devicePolls, releaseAdapterCount, releaseDeviceCount;
	uintptr_t nextResourceIdentity;
	uint32_t created[4], destroyed[4], writeBufferCount, writeTextureCount;
	uint32_t beginRoundTripCount, pollRoundTripCount, releaseOperationCount;
	uint32_t pendingPolls;
	uint64_t operationGeneration, roundTripSize;
	uintptr_t operationIdentity;
	unsigned char roundTrip[1024];
	qboolean corruptReadback, failWrite;
} fakeHost_t;

static qboolean BeginAdapter( void *user, uint64_t generation ) {
	(void)user; return generation ? qtrue : qfalse;
}

static qboolean PollAdapter( void *user, uint64_t generation,
		ralWebGpuAdapterPoll_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	(void)generation; host->adapterPolls++;
	memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY;
	out->adapterIdentity = (uintptr_t)0x2000u;
	out->adapterType = RAL_ADAPTER_TYPE_DISCRETE;
	out->vendorName = "Wired"; out->deviceName = "WebGPU resource fixture";
	out->limits.maxColorAttachments = 8u;
	out->limits.maxTextureDimension2D = 8192u;
	out->limits.maxTextureDimension3D = 2048u;
	out->limits.maxTextureArrayLayers = 2048u;
	out->limits.maxComputeInvocationsPerWorkgroup = 256u;
	out->limits.maxSampledTexturesPerShaderStage = 32u;
	out->limits.maxBindGroups = 4u;
	out->limits.maxBindingsPerBindGroup = 16u;
	out->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
	out->limits.minUniformBufferOffsetAlignment = 256u;
	out->limits.minStorageBufferOffsetAlignment = 256u;
	return qtrue;
}

static qboolean BeginDevice( void *user, uintptr_t adapter, uint64_t generation ) {
	(void)user; return adapter == (uintptr_t)0x2000u && generation > 1u;
}

static qboolean PollDevice( void *user, uint64_t generation,
		ralWebGpuDevicePoll_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	(void)generation; host->devicePolls++;
	memset( out, 0, sizeof( *out ) );
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

static uintptr_t ResourceIdentity( fakeHost_t *host,
		ralWebGpuResourceKind_t kind ) {
	host->created[kind]++;
	return ++host->nextResourceIdentity;
}

static qboolean CreateBuffer( void *user, uintptr_t device,
		const ralWebGpuBufferDesc_t *desc, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != (uintptr_t)0x3000u || !desc || !desc->size
			|| ( desc->size & 3u ) ) return qfalse;
	*out = ResourceIdentity( host, RAL_WEBGPU_RESOURCE_BUFFER ); return qtrue;
}

static qboolean CreateTexture( void *user, uintptr_t device,
		const ralWebGpuTextureDesc_t *desc, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	uint32_t expected = desc && desc->format == RAL_FORMAT_R8_UNORM ? 1u
		: desc && desc->format == RAL_FORMAT_R8G8_UNORM ? 2u
		: desc && ( desc->format == RAL_FORMAT_R8G8B8A8_UNORM
			|| desc->format == RAL_FORMAT_R8G8B8A8_SRGB
			|| desc->format == RAL_FORMAT_E5B9G9R9_UFLOAT
			|| desc->format == RAL_FORMAT_R16G16_SNORM ) ? 4u
		: desc && desc->format == RAL_FORMAT_R16G16B16A16_SFLOAT ? 8u : 0u;
	if ( device != (uintptr_t)0x3000u || !desc
			|| !expected || desc->bytesPerTexel != expected )
		return qfalse;
	*out = ResourceIdentity( host, RAL_WEBGPU_RESOURCE_TEXTURE ); return qtrue;
}

static qboolean CreateSampler( void *user, uintptr_t device,
		const ralWebGpuSamplerDesc_t *desc, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != (uintptr_t)0x3000u || !desc ) return qfalse;
	*out = ResourceIdentity( host, RAL_WEBGPU_RESOURCE_SAMPLER ); return qtrue;
}

static void DestroyResource( void *user, ralWebGpuResourceKind_t kind,
		uintptr_t identity ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( kind >= RAL_WEBGPU_RESOURCE_BUFFER && kind <= RAL_WEBGPU_RESOURCE_SAMPLER
			&& identity ) host->destroyed[kind]++;
}

static qboolean WriteBuffer( void *user, uintptr_t queue, uintptr_t buffer,
		uint64_t offset, const void *bytes, uint64_t byteSize ) {
	fakeHost_t *host = (fakeHost_t *)user; (void)offset;
	if ( queue != (uintptr_t)0x4000u || !buffer || !bytes || !byteSize )
		return qfalse;
	if ( host->failWrite ) return qfalse;
	host->writeBufferCount++; return qtrue;
}

static qboolean WriteTexture( void *user, uintptr_t queue, uintptr_t texture,
		const void *bytes, uint64_t byteSize, uint32_t bytesPerRow,
		uint32_t rowsPerImage ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( queue != (uintptr_t)0x4000u || !texture || !bytes || !byteSize
			|| !bytesPerRow || ( bytesPerRow & 255u ) || !rowsPerImage
			|| byteSize % ( (uint64_t)bytesPerRow * rowsPerImage ) )
		return qfalse;
	host->writeTextureCount++; return qtrue;
}

static qboolean BeginRoundTrip( void *user, uintptr_t device, uintptr_t queue,
		uintptr_t upload, uintptr_t readback, const void *bytes,
		uint64_t byteSize, uint64_t generation, uintptr_t *outOperation ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != (uintptr_t)0x3000u || queue != (uintptr_t)0x4000u
			|| !upload || !readback || !bytes || !byteSize
			|| byteSize > sizeof( host->roundTrip ) || !generation ) return qfalse;
	memcpy( host->roundTrip, bytes, (size_t)byteSize );
	host->roundTripSize = byteSize; host->operationGeneration = generation;
	host->operationIdentity = (uintptr_t)( 0x9000u + generation );
	host->beginRoundTripCount++; *outOperation = host->operationIdentity;
	return qtrue;
}

static qboolean PollRoundTrip( void *user, uintptr_t operation,
		uint64_t generation, void *outBytes, uint64_t capacity,
		uint64_t *outByteSize, ralWebGpuAsyncStatus_t *outStatus ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( operation != host->operationIdentity
			|| generation != host->operationGeneration || !outBytes
			|| capacity < host->roundTripSize || !outByteSize || !outStatus )
		return qfalse;
	host->pollRoundTripCount++;
	if ( host->pendingPolls ) {
		host->pendingPolls--; *outStatus = RAL_WEBGPU_ASYNC_PENDING; return qtrue;
	}
	memcpy( outBytes, host->roundTrip, (size_t)host->roundTripSize );
	if ( host->corruptReadback ) ((unsigned char *)outBytes)[0] ^= 0xffu;
	*outByteSize = host->roundTripSize;
	*outStatus = RAL_WEBGPU_ASYNC_READY;
	return qtrue;
}

static void ReleaseOperation( void *user, uintptr_t operation ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( operation == host->operationIdentity ) host->releaseOperationCount++;
}

static ralWebGpuCoreCreateInfo_t CoreInfo( fakeHost_t *host,
		uint64_t generation ) {
	ralWebGpuCoreCreateInfo_t info;
	memset( &info, 0, sizeof( info ) );
	info.generation = generation; info.userData = host;
	info.host.beginAdapter = BeginAdapter; info.host.pollAdapter = PollAdapter;
	info.host.beginDevice = BeginDevice; info.host.pollDevice = PollDevice;
	info.host.releaseDevice = ReleaseDevice; info.host.releaseAdapter = ReleaseAdapter;
	return info;
}

static ralWebGpuResourceLayerCreateInfo_t ResourceInfo( fakeHost_t *host ) {
	ralWebGpuResourceLayerCreateInfo_t info;
	memset( &info, 0, sizeof( info ) ); info.userData = host;
	info.host.createBuffer = CreateBuffer; info.host.createTexture = CreateTexture;
	info.host.createSampler = CreateSampler; info.host.destroyResource = DestroyResource;
	info.host.writeBuffer = WriteBuffer; info.host.writeTexture = WriteTexture;
	info.host.beginRoundTrip = BeginRoundTrip;
	info.host.pollRoundTrip = PollRoundTrip;
	info.host.releaseOperation = ReleaseOperation;
	return info;
}

static qboolean ReadyCore( fakeHost_t *host, uint64_t generation,
		ralWebGpuCore_t **outCore, ralWebGpuCoreReceipt_t *outReceipt ) {
	ralWebGpuCoreCreateInfo_t info = CoreInfo( host, generation );
	if ( !RalWebGpu_CoreBegin( &info, outCore ) ) return qfalse;
	while ( RalWebGpu_CorePoll( *outCore, outReceipt ) == RAL_WEBGPU_POLL_PENDING ) {}
	return RalWebGpu_CoreMatchesReceipt( *outCore, outReceipt );
}

static ralMemoryFailureEvent_t DeviceLoss( void ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = RAL_BACKEND_WEBGPU;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 64u; event.attempt = 1u; event.maxAttempts = 1u;
	event.liveParent = qtrue;
	return event;
}

int main( void ) {
	fakeHost_t host = { 0 }, replacementHost = { 0 };
	ralWebGpuCore_t *core = NULL, *replacement = NULL;
	ralWebGpuCoreReceipt_t coreReceipt, replacementReceipt;
	ralWebGpuResourceLayerCreateInfo_t resourceInfo;
	ralWebGpuResourceLayer_t *layer = NULL, *replacementLayer = NULL;
	ralWebGpuResource_t *buffer = NULL;
	ralWebGpuResource_t *texture = NULL;
	ralWebGpuBufferDesc_t bufferDesc;
	ralWebGpuTextureDesc_t textureDesc;
	ralWebGpuResourceReceipt_t bufferReceipt, textureReceipt, stale;
	ralWebGpuWriteReceipt_t write, writeExact, unchangedWrite;
	ralLightingProductRequest_t lightingRequest;
	ralLightingArtifactReceipt_t lightingArtifact;
	ralLightingRuntimePlan_t lightingPlan;
	ralWebGpuLighting_t *lighting = NULL;
	ralWebGpuLightingReceipt_t lightingReceipt;
	ralStaticLightingCapabilities_t lightingCaps = { qtrue, qtrue, qtrue, qtrue };
	ralLightVec3Q16_t lightingRadiance[2] = {
		{ 4 * RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, 0 },
		{ RAL_LIGHT_Q16_ONE, 2 * RAL_LIGHT_Q16_ONE, 3 * RAL_LIGHT_Q16_ONE }
	};
	ralLightVec3Q16_t lightingDirection[2] = {
		{ 0, 0, RAL_LIGHT_Q16_ONE }, { RAL_LIGHT_Q16_ONE, 0, RAL_LIGHT_Q16_ONE }
	};
	uint8_t lightingVisibility[2] = { 255u, 128u };
	uint8_t lightingRadianceBytes[16], lightingDirectionBytes[8], lightingBytes[512];
	unsigned char bufferBytes[16] = { 0u };
	unsigned char textureBytes[1024] = { 0u };
	ralBackendConformanceReceipt_t output, sentinel;
	ralMemoryFailureEvent_t event;
	ralMemoryFailureReceipt_t loss;
	host.nextResourceIdentity = (uintptr_t)0x5000u;
	CHECK( ReadyCore( &host, 7u, &core, &coreReceipt ) );
	resourceInfo = ResourceInfo( &host );
	CHECK( RalWebGpu_ResourcesCreate( core, &coreReceipt, &resourceInfo, &layer ) );
	memset( &bufferDesc, 0, sizeof( bufferDesc ) );
	bufferDesc.size = 16u; bufferDesc.usage = RAL_WEBGPU_BUFFER_VERTEX;
	bufferDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	CHECK( RalWebGpu_CreateBuffer( layer, &bufferDesc, &buffer, &bufferReceipt ) );
	CHECK( RalWebGpu_ResourceReceiptExact( &bufferReceipt, &bufferReceipt ) );
	CHECK( bufferReceipt.allocation.outcome == RAL_ALLOCATION_OUTCOME_EMULATED );
	CHECK( RalWebGpu_WriteBuffer( layer, buffer, &bufferReceipt, 0u,
		bufferBytes, sizeof( bufferBytes ), &write ) );
	writeExact = write; CHECK( RalWebGpu_WriteReceiptExact( &write, &writeExact ) );
	memset( &unchangedWrite, 0xa5, sizeof( unchangedWrite ) ); write = unchangedWrite;
	host.failWrite = qtrue;
	CHECK( !RalWebGpu_WriteBuffer( layer, buffer, &bufferReceipt, 0u,
		bufferBytes, sizeof( bufferBytes ), &write )
		&& !memcmp( &write, &unchangedWrite, sizeof( write ) ) );
	host.failWrite = qfalse;
	stale = bufferReceipt; stale.resourceGeneration++;
	CHECK( !RalWebGpu_WriteBuffer( layer, buffer, &stale, 0u, bufferBytes,
		sizeof( bufferBytes ), &write )
		&& !memcmp( &write, &unchangedWrite, sizeof( write ) ) );
	CHECK( !RalWebGpu_WriteBuffer( layer, buffer, &bufferReceipt, 4u,
		bufferBytes, sizeof( bufferBytes ), &write ) );
	memset( &textureDesc, 0, sizeof( textureDesc ) ); textureDesc.width = 64u;
	textureDesc.height = 4u; textureDesc.depth = 1u;
	textureDesc.format = RAL_FORMAT_R8G8B8A8_UNORM; textureDesc.bytesPerTexel = 4u;
	CHECK( RalWebGpu_CreateTexture( layer, &textureDesc, &texture, &textureReceipt ) );
	CHECK( RalWebGpu_WriteTexture( layer, texture, &textureReceipt, textureBytes,
		sizeof( textureBytes ), 256u, 4u, &write ) );
	CHECK( write.kind == RAL_WEBGPU_WRITE_TEXTURE && host.writeBufferCount == 1u );
	memset( &lightingRequest, 0, sizeof( lightingRequest ) );
	lightingRequest.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
	lightingRequest.artifactGeneration = 21u;
	lightingRequest.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	lightingRequest.bake.bakeGeneration = 1u;
	lightingRequest.bake.staticIndirectKey = 2u;
	lightingRequest.bake.producerVersion = 3u;
	lightingRequest.bake.radianceHash = 4u;
	lightingRequest.bake.directionHash = 5u;
	lightingRequest.bake.patchCount = 2u;
	lightingRequest.bake.linkCount = 1u;
	lightingRequest.bake.completedBounces = 4u;
	lightingRequest.bake.ready = qtrue;
	lightingRequest.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	lightingRequest.pageWidth = 1u; lightingRequest.pageHeight = 1u;
	lightingRequest.pageCount = 2u; lightingRequest.indirectRadiance = lightingRadiance;
	lightingRequest.dominantDirection = lightingDirection;
	lightingRequest.texelCount = 2u;
	lightingRequest.stationaryVisibility = lightingVisibility;
	lightingRequest.stationaryVisibilityCount = 2u;
	CHECK( Ral_LightingProductWrite( &lightingRequest, lightingRadianceBytes,
		sizeof( lightingRadianceBytes ), lightingDirectionBytes,
		sizeof( lightingDirectionBytes ), lightingBytes, sizeof( lightingBytes ),
		&lightingArtifact ) );
	CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_WEBGPU, 22u, lightingBytes,
		lightingArtifact.byteLength, &lightingArtifact, &lightingCaps, &lightingPlan ) );
	CHECK( RalWebGpu_LightingUpload( layer, lightingBytes,
		lightingArtifact.byteLength, &lightingPlan, &lighting, &lightingReceipt ) );
	CHECK( RalWebGpu_LightingReceiptExact( &lightingReceipt, &lightingReceipt )
		&& host.writeTextureCount == 4u );
	CHECK( RalWebGpu_LightingDestroy( layer, lighting, &lightingReceipt ) );
	CHECK( !RalWebGpu_DestroyResource( layer, buffer, &stale ) );
	CHECK( RalWebGpu_DestroyResource( layer, buffer, &bufferReceipt ) );
	bufferDesc.size = 15u;
	CHECK( !RalWebGpu_CreateBuffer( layer, &bufferDesc, &buffer, &bufferReceipt ) );
	host.pendingPolls = 1u;
	CHECK( RalWebGpu_OffscreenConformanceBegin( layer, 64u ) );
	memset( &sentinel, 0xa5, sizeof( sentinel ) ); output = sentinel;
	CHECK( RalWebGpu_OffscreenConformancePoll( layer, &output )
		== RAL_WEBGPU_ASYNC_PENDING );
	CHECK( !memcmp( &output, &sentinel, sizeof( output ) ) );
	CHECK( RalWebGpu_OffscreenConformancePoll( layer, &output )
		== RAL_WEBGPU_ASYNC_READY );
	CHECK( Ral_BackendConformanceReceiptExact( &output, &output ) );
	CHECK( output.backendType == RAL_BACKEND_WEBGPU );
	CHECK( output.transfer.outcome == RAL_TRANSFER_OUTCOME_NATIVE_ASYNC );
	CHECK( output.copiedByteCount == 64u && output.copiedByteDigest != 0u );
	CHECK( host.writeTextureCount == 5u && host.releaseOperationCount == 1u );

	CHECK( RalWebGpu_OffscreenConformanceBegin( layer, 64u ) );
	event = DeviceLoss();
	CHECK( RalWebGpu_CorePublishDeviceLoss( core, &event, &loss ) );
	output = sentinel;
	CHECK( RalWebGpu_OffscreenConformancePoll( layer, &output )
		== RAL_WEBGPU_ASYNC_DEVICE_LOST );
	CHECK( !memcmp( &output, &sentinel, sizeof( output ) ) );
	CHECK( !RalWebGpu_OffscreenConformanceBegin( layer, 64u ) );

	replacementHost.nextResourceIdentity = (uintptr_t)0xa000u;
	CHECK( ReadyCore( &replacementHost, 8u, &replacement, &replacementReceipt ) );
	resourceInfo = ResourceInfo( &replacementHost );
	CHECK( !RalWebGpu_ResourcesCreate( replacement, &coreReceipt,
		&resourceInfo, &replacementLayer ) );
	CHECK( RalWebGpu_ResourcesCreate( replacement, &replacementReceipt,
		&resourceInfo, &replacementLayer ) );
	replacementHost.corruptReadback = qtrue;
	CHECK( RalWebGpu_OffscreenConformanceBegin( replacementLayer, 64u ) );
	CHECK( RalWebGpu_OffscreenConformancePoll( replacementLayer, &output )
		== RAL_WEBGPU_ASYNC_FAILED );
	CHECK( replacementHost.releaseOperationCount == 1u );
	RalWebGpu_ResourcesDestroy( replacementLayer );
	RalWebGpu_ResourcesDestroy( layer );
	RalWebGpu_CoreDestroy( replacement );
	RalWebGpu_CoreDestroy( core );
	CHECK( host.created[RAL_WEBGPU_RESOURCE_BUFFER]
		== host.destroyed[RAL_WEBGPU_RESOURCE_BUFFER] );
	CHECK( host.created[RAL_WEBGPU_RESOURCE_TEXTURE]
		== host.destroyed[RAL_WEBGPU_RESOURCE_TEXTURE] );
	CHECK( host.created[RAL_WEBGPU_RESOURCE_SAMPLER]
		== host.destroyed[RAL_WEBGPU_RESOURCE_SAMPLER] );
	puts( "ral WebGPU resource/async conformance contract: PASS" );
	return 0;
}
