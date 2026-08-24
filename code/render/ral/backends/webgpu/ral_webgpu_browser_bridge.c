// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_browser_bridge.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

_Static_assert( sizeof( ralWebGpuBrowserAbiHeader_t ) == 16u,
	"browser ABI header layout drift" );
_Static_assert( sizeof( ralWebGpuBrowserGenerationRequest_t ) == 24u,
	"browser ABI generation request layout drift" );
_Static_assert( sizeof( ralWebGpuBrowserAdapterResponse_t ) == 376u,
	"browser ABI adapter response layout drift" );
_Static_assert( sizeof( ralWebGpuBrowserCanvasConfigureRequest_t ) == 56u,
	"browser ABI canvas configure layout drift" );
_Static_assert( sizeof( ralWebGpuBrowserIndexedDrawRequest_t ) == 104u,
	"browser ABI indexed draw request layout drift" );

struct ralWebGpuBrowserBridge_s {
	uint64_t generation;
	uintptr_t canvasIdentity;
	uintptr_t adapterIdentity;
	uintptr_t deviceIdentity;
	uintptr_t queueIdentity;
	void *userData;
	ralWebGpuBrowserDispatchFn dispatch;
	char vendorName[RAL_WEBGPU_BROWSER_ABI_NAME_BYTES];
	char deviceName[RAL_WEBGPU_BROWSER_ABI_NAME_BYTES];
};

static void Header( ralWebGpuBrowserAbiHeader_t *header,
		ralWebGpuBrowserOpcode_t opcode, uint32_t bytes ) {
	header->schemaVersion = RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION;
	header->opcode = (uint32_t)opcode; header->byteCount = bytes;
	header->reserved = 0u;
}

static qboolean ResponseValid( const ralWebGpuBrowserAbiHeader_t *header,
		ralWebGpuBrowserOpcode_t opcode, uint32_t bytes ) {
	return header && header->schemaVersion == RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION
		&& header->opcode == (uint32_t)opcode && header->byteCount == bytes
		&& header->reserved == 0u;
}

static qboolean Call( ralWebGpuBrowserBridge_t *bridge,
		ralWebGpuBrowserOpcode_t opcode, const void *request,
		uint32_t requestBytes, void *response, uint32_t responseBytes ) {
	return bridge && bridge->dispatch && request && requestBytes && response
		&& responseBytes && bridge->dispatch( bridge->userData, opcode, request,
			requestBytes, response, responseBytes )
		&& ResponseValid( (const ralWebGpuBrowserAbiHeader_t *)response,
			opcode, responseBytes );
}

static qboolean Identity( uint64_t source, uintptr_t *out ) {
	if ( !source || !out || source > (uint64_t)UINTPTR_MAX ) return qfalse;
	*out = (uintptr_t)source; return qtrue;
}

static qboolean BeginAdapter( void *userData, uint64_t generation ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserGenerationRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || generation != bridge->generation ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_BEGIN_ADAPTER,
		sizeof( request ) ); request.requestGeneration = generation;
	return Call( bridge, RAL_WEBGPU_BROWSER_OP_BEGIN_ADAPTER, &request,
		sizeof( request ), &response, sizeof( response ) )
		&& response.accepted == 1u;
}

static qboolean PollAdapter( void *userData, uint64_t generation,
		ralWebGpuAdapterPoll_t *out ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserGenerationRequest_t request;
	ralWebGpuBrowserAdapterResponse_t response;
	uintptr_t adapter;
	if ( !bridge || !out || generation != bridge->generation ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_POLL_ADAPTER,
		sizeof( request ) ); request.requestGeneration = generation;
	if ( !Call( bridge, RAL_WEBGPU_BROWSER_OP_POLL_ADAPTER, &request,
			sizeof( request ), &response, sizeof( response ) )
			|| response.status < RAL_WEBGPU_REQUEST_PENDING
			|| response.status > RAL_WEBGPU_REQUEST_FAILED ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->status = (ralWebGpuRequestStatus_t)response.status;
	if ( out->status != RAL_WEBGPU_REQUEST_READY ) return qtrue;
	if ( !Identity( response.adapterIdentity, &adapter )
			|| response.adapterType > RAL_ADAPTER_TYPE_CPU
			|| !memchr( response.vendorName, '\0', sizeof( response.vendorName ) )
			|| !memchr( response.deviceName, '\0', sizeof( response.deviceName ) ) )
		return qfalse;
	memcpy( bridge->vendorName, response.vendorName, sizeof( bridge->vendorName ) );
	memcpy( bridge->deviceName, response.deviceName, sizeof( bridge->deviceName ) );
	bridge->adapterIdentity = adapter;
	out->adapterIdentity = adapter;
	out->adapterType = (ralAdapterType_t)response.adapterType;
	out->vendorId = response.vendorId; out->deviceId = response.deviceId;
	out->vendorName = bridge->vendorName; out->deviceName = bridge->deviceName;
	out->limits.maxColorAttachments = response.limits.maxColorAttachments;
	out->limits.maxTextureDimension2D = response.limits.maxTextureDimension2D;
	out->limits.maxTextureDimension3D = response.limits.maxTextureDimension3D;
	out->limits.maxTextureArrayLayers = response.limits.maxTextureArrayLayers;
	out->limits.maxComputeInvocationsPerWorkgroup = response.limits.maxComputeInvocationsPerWorkgroup;
	out->limits.maxSampledTexturesPerShaderStage = response.limits.maxSampledTexturesPerShaderStage;
	out->limits.maxBindGroups = response.limits.maxBindGroups;
	out->limits.maxBindingsPerBindGroup = response.limits.maxBindingsPerBindGroup;
	out->limits.maxStorageBufferBindingSize = response.limits.maxStorageBufferBindingSize;
	out->limits.minUniformBufferOffsetAlignment = response.limits.minUniformBufferOffsetAlignment;
	out->limits.minStorageBufferOffsetAlignment = response.limits.minStorageBufferOffsetAlignment;
	out->limits.bindingArrays = (qboolean)response.limits.bindingArrays;
	out->limits.textureCompressionBC = (qboolean)response.limits.textureCompressionBC;
	out->limits.textureCompressionASTC = (qboolean)response.limits.textureCompressionASTC;
	out->limits.textureCompressionETC2 = (qboolean)response.limits.textureCompressionETC2;
	out->limits.timestampQueries = (qboolean)response.limits.timestampQueries;
	return qtrue;
}

static qboolean BeginDevice( void *userData, uintptr_t adapter,
		uint64_t generation ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserBeginDeviceRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || adapter != bridge->adapterIdentity
			|| generation != bridge->generation + 1u ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_BEGIN_DEVICE,
		sizeof( request ) ); request.adapterIdentity = adapter;
	request.requestGeneration = generation;
	return Call( bridge, RAL_WEBGPU_BROWSER_OP_BEGIN_DEVICE, &request,
		sizeof( request ), &response, sizeof( response ) )
		&& response.accepted == 1u;
}

static qboolean PollDevice( void *userData, uint64_t generation,
		ralWebGpuDevicePoll_t *out ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserGenerationRequest_t request;
	ralWebGpuBrowserDeviceResponse_t response;
	uintptr_t device, queue;
	if ( !bridge || !out || generation != bridge->generation + 1u ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_POLL_DEVICE,
		sizeof( request ) ); request.requestGeneration = generation;
	if ( !Call( bridge, RAL_WEBGPU_BROWSER_OP_POLL_DEVICE, &request,
			sizeof( request ), &response, sizeof( response ) )
			|| response.status < RAL_WEBGPU_REQUEST_PENDING
			|| response.status > RAL_WEBGPU_REQUEST_FAILED ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->status = (ralWebGpuRequestStatus_t)response.status;
	if ( out->status != RAL_WEBGPU_REQUEST_READY ) return qtrue;
	if ( !Identity( response.deviceIdentity, &device )
			|| !Identity( response.queueIdentity, &queue )
			|| device == bridge->adapterIdentity || device == queue ) return qfalse;
	bridge->deviceIdentity = device; bridge->queueIdentity = queue;
	out->deviceIdentity = device; out->queueIdentity = queue; return qtrue;
}

static void ReleaseDevice( void *userData, uintptr_t device, uintptr_t queue ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserReleaseDeviceRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || device != bridge->deviceIdentity || queue != bridge->queueIdentity ) return;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_RELEASE_DEVICE,
		sizeof( request ) ); request.deviceIdentity = device; request.queueIdentity = queue;
	if ( Call( bridge, RAL_WEBGPU_BROWSER_OP_RELEASE_DEVICE, &request,
			sizeof( request ), &response, sizeof( response ) )
			&& response.accepted == 1u ) {
		bridge->deviceIdentity = 0; bridge->queueIdentity = 0;
	}
}

static void ReleaseAdapter( void *userData, uintptr_t adapter ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserReleaseAdapterRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || adapter != bridge->adapterIdentity ) return;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_RELEASE_ADAPTER,
		sizeof( request ) ); request.adapterIdentity = adapter;
	if ( Call( bridge, RAL_WEBGPU_BROWSER_OP_RELEASE_ADAPTER, &request,
			sizeof( request ), &response, sizeof( response ) )
			&& response.accepted == 1u ) bridge->adapterIdentity = 0;
}

static qboolean CanvasConfigure( void *userData, uintptr_t canvas,
		uintptr_t device, uint32_t width, uint32_t height, ralFormat_t format,
		ralColorSpace_t colorSpace, qboolean opaque ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCanvasConfigureRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || canvas != bridge->canvasIdentity
			|| device != bridge->deviceIdentity || !width || !height ) return qfalse;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CANVAS_CONFIGURE,
		sizeof( request ) ); request.canvasIdentity = canvas;
	request.deviceIdentity = device; request.pixelWidth = width;
	request.pixelHeight = height; request.format = (uint32_t)format;
	request.colorSpace = (uint32_t)colorSpace; request.opaqueAlpha = (uint32_t)opaque;
	return Call( bridge, RAL_WEBGPU_BROWSER_OP_CANVAS_CONFIGURE, &request,
		sizeof( request ), &response, sizeof( response ) )
		&& response.accepted == 1u;
}

static void CanvasUnconfigure( void *userData, uintptr_t canvas ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCanvasRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || canvas != bridge->canvasIdentity ) return;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CANVAS_UNCONFIGURE,
		sizeof( request ) ); request.canvasIdentity = canvas;
	(void)Call( bridge, RAL_WEBGPU_BROWSER_OP_CANVAS_UNCONFIGURE, &request,
		sizeof( request ), &response, sizeof( response ) );
}

static qboolean CanvasAcquire( void *userData, uintptr_t canvas,
		uint64_t frameGeneration, uintptr_t *outTexture ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCanvasAcquireRequest_t request;
	ralWebGpuBrowserCanvasAcquireResponse_t response;
	if ( !bridge || canvas != bridge->canvasIdentity || !frameGeneration
			|| !outTexture ) return qfalse;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CANVAS_ACQUIRE,
		sizeof( request ) ); request.canvasIdentity = canvas;
	request.frameGeneration = frameGeneration;
	return Call( bridge, RAL_WEBGPU_BROWSER_OP_CANVAS_ACQUIRE, &request,
		sizeof( request ), &response, sizeof( response ) )
		&& response.accepted == 1u
		&& Identity( response.textureIdentity, outTexture );
}

static qboolean CanvasPresent( void *userData, uintptr_t canvas,
		uintptr_t texture, uint64_t frameGeneration,
		uint64_t submissionGeneration ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCanvasPresentRequest_t request;
	ralWebGpuBrowserStatusResponse_t response;
	if ( !bridge || canvas != bridge->canvasIdentity || !texture
			|| !frameGeneration || !submissionGeneration ) return qfalse;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CANVAS_PRESENT,
		sizeof( request ) ); request.canvasIdentity = canvas;
	request.textureIdentity = texture; request.frameGeneration = frameGeneration;
	request.submissionGeneration = submissionGeneration;
	return Call( bridge, RAL_WEBGPU_BROWSER_OP_CANVAS_PRESENT, &request,
		sizeof( request ), &response, sizeof( response ) )
		&& response.accepted == 1u;
}

static qboolean IdentityCall( ralWebGpuBrowserBridge_t *bridge,
		ralWebGpuBrowserOpcode_t opcode, void *request, uint32_t requestBytes,
		uintptr_t *outIdentity ) {
	ralWebGpuBrowserIdentityResponse_t response;
	uint64_t identity;
	if ( !bridge || !request || !outIdentity ) return qfalse;
	memset( &response, 0, sizeof( response ) );
	if ( !Call( bridge, opcode, request, requestBytes, &response,
			sizeof( response ) ) || response.accepted != 1u ) return qfalse;
	identity = response.identity;
	return Identity( identity, outIdentity );
}

static qboolean StatusCall( ralWebGpuBrowserBridge_t *bridge,
		ralWebGpuBrowserOpcode_t opcode, void *request, uint32_t requestBytes ) {
	ralWebGpuBrowserStatusResponse_t response;
	memset( &response, 0, sizeof( response ) );
	return Call( bridge, opcode, request, requestBytes, &response,
		sizeof( response ) ) && response.accepted == 1u;
}

static qboolean CreateBuffer( void *userData, uintptr_t device,
		const ralWebGpuBufferDesc_t *desc, uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCreateBufferRequest_t request;
	if ( !bridge || !desc || device != bridge->deviceIdentity || !desc->size
			|| desc->size > RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_BUFFER,
		sizeof( request ) ); request.deviceIdentity = device;
	request.byteSize = desc->size; request.usage = desc->usage;
	request.memoryClass = (uint32_t)desc->memoryClass;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_BUFFER,
		&request, sizeof( request ), outIdentity );
}

static qboolean CreateTexture( void *userData, uintptr_t device,
		const ralWebGpuTextureDesc_t *desc, uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCreateTextureRequest_t request;
	if ( !bridge || !desc || device != bridge->deviceIdentity || !desc->width
			|| !desc->height || !desc->depth || !desc->bytesPerTexel ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_TEXTURE,
		sizeof( request ) ); request.deviceIdentity = device;
	request.width = desc->width; request.height = desc->height;
	request.depth = desc->depth; request.bytesPerTexel = desc->bytesPerTexel;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_TEXTURE,
		&request, sizeof( request ), outIdentity );
}

static qboolean CreateSampler( void *userData, uintptr_t device,
		const ralWebGpuSamplerDesc_t *desc, uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserCreateSamplerRequest_t request;
	if ( !bridge || !desc || device != bridge->deviceIdentity ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_SAMPLER,
		sizeof( request ) ); request.deviceIdentity = device;
	request.linearMinification = desc->linearMinification;
	request.linearMagnification = desc->linearMagnification;
	request.clampToEdge = desc->clampToEdge;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_SAMPLER,
		&request, sizeof( request ), outIdentity );
}

static void DestroyResource( void *userData, ralWebGpuResourceKind_t kind,
		uintptr_t identity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserDestroyObjectRequest_t request;
	if ( !bridge || kind < RAL_WEBGPU_RESOURCE_BUFFER
			|| kind > RAL_WEBGPU_RESOURCE_SAMPLER || !identity ) return;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_DESTROY_RESOURCE,
		sizeof( request ) ); request.kind = (uint32_t)kind;
	request.identity = identity;
	(void)StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_DESTROY_RESOURCE,
		&request, sizeof( request ) );
}

static qboolean WriteBuffer( void *userData, uintptr_t queue,
		uintptr_t buffer, uint64_t byteOffset, const void *bytes,
		uint64_t byteSize ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserWriteBufferRequest_t request;
	if ( !bridge || queue != bridge->queueIdentity || !buffer || !bytes
			|| !byteSize || ( byteOffset & 3u ) || ( byteSize & 3u ) ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_WRITE_BUFFER,
		sizeof( request ) ); request.queueIdentity = queue;
	request.resourceIdentity = buffer; request.byteOffset = byteOffset;
	request.dataOffset = (uint64_t)(uintptr_t)bytes; request.byteSize = byteSize;
	return StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_WRITE_BUFFER,
		&request, sizeof( request ) );
}

static qboolean WriteTexture( void *userData, uintptr_t queue,
		uintptr_t texture, const void *bytes, uint64_t byteSize,
		uint32_t bytesPerRow, uint32_t rowsPerImage ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserWriteTextureRequest_t request;
	if ( !bridge || queue != bridge->queueIdentity || !texture || !bytes
			|| !byteSize || !bytesPerRow || !rowsPerImage ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_WRITE_TEXTURE,
		sizeof( request ) ); request.queueIdentity = queue;
	request.resourceIdentity = texture;
	request.dataOffset = (uint64_t)(uintptr_t)bytes; request.byteSize = byteSize;
	request.bytesPerRow = bytesPerRow; request.rowsPerImage = rowsPerImage;
	return StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_WRITE_TEXTURE,
		&request, sizeof( request ) );
}

static qboolean BeginRoundTrip( void *userData, uintptr_t device,
		uintptr_t queue, uintptr_t upload, uintptr_t readback,
		const void *bytes, uint64_t byteSize, uint64_t operationGeneration,
		uintptr_t *outOperation ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserRoundTripBeginRequest_t request;
	if ( !bridge || device != bridge->deviceIdentity
			|| queue != bridge->queueIdentity || !upload || !readback || !bytes
			|| !byteSize || !operationGeneration ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_BEGIN_ROUND_TRIP,
		sizeof( request ) ); request.deviceIdentity = device;
	request.queueIdentity = queue; request.uploadBufferIdentity = upload;
	request.readbackBufferIdentity = readback;
	request.dataOffset = (uint64_t)(uintptr_t)bytes; request.byteSize = byteSize;
	request.operationGeneration = operationGeneration;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_BEGIN_ROUND_TRIP,
		&request, sizeof( request ), outOperation );
}

static qboolean PollRoundTrip( void *userData, uintptr_t operation,
		uint64_t operationGeneration, void *outBytes, uint64_t capacity,
		uint64_t *outByteSize, ralWebGpuAsyncStatus_t *outStatus ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserRoundTripPollRequest_t request;
	ralWebGpuBrowserAsyncResponse_t response;
	if ( !bridge || !operation || !operationGeneration || !outBytes || !capacity
			|| !outByteSize || !outStatus ) return qfalse;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_POLL_ROUND_TRIP,
		sizeof( request ) ); request.operationIdentity = operation;
	request.operationGeneration = operationGeneration;
	request.outputOffset = (uint64_t)(uintptr_t)outBytes; request.capacity = capacity;
	if ( !Call( bridge, RAL_WEBGPU_BROWSER_OP_POLL_ROUND_TRIP, &request,
			sizeof( request ), &response, sizeof( response ) )
			|| response.status < RAL_WEBGPU_ASYNC_PENDING
			|| response.status > RAL_WEBGPU_ASYNC_DEVICE_LOST
			|| response.byteSize > capacity ) return qfalse;
	*outStatus = (ralWebGpuAsyncStatus_t)response.status;
	*outByteSize = response.byteSize; return qtrue;
}

static void ReleaseOperation( void *userData, uintptr_t operation ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserIdentityRequest_t request;
	if ( !bridge || !operation ) return;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_RELEASE_OPERATION,
		sizeof( request ) ); request.identity = operation;
	(void)StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_RELEASE_OPERATION,
		&request, sizeof( request ) );
}

static qboolean CreateShaderModule( void *userData, uintptr_t device,
		const ralWebGpuShaderModuleDesc_t *desc, uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserShaderModuleRequest_t request;
	if ( !bridge || device != bridge->deviceIdentity || !desc || !desc->code
			|| !desc->byteCount || !desc->entryPoint || !desc->entryPoint[0]
			|| strlen( desc->entryPoint ) >= sizeof( request.entryPoint ) ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_SHADER_MODULE,
		sizeof( request ) ); request.deviceIdentity = device;
	request.codeOffset = (uint64_t)(uintptr_t)desc->code;
	request.digestLane0 = desc->digest.lane0; request.digestLane1 = desc->digest.lane1;
	request.stage = desc->stage; request.byteCount = desc->byteCount;
	strncpy( request.entryPoint, desc->entryPoint,
		sizeof( request.entryPoint ) - 1u );
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_SHADER_MODULE,
		&request, sizeof( request ), outIdentity );
}

static qboolean CreateBindGroupLayout( void *userData, uintptr_t device,
		uint32_t group, const ralWebGpuBindGroupLayoutEntry_t *entries,
		uint32_t entryCount, uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserBindGroupLayoutRequest_t request;
	ralWebGpuBrowserBindEntryAbi_t converted[RAL_SHADER_ABI_MAX_BINDINGS_PER_GROUP];
	if ( !bridge || device != bridge->deviceIdentity || !outIdentity
			|| entryCount > RAL_SHADER_ABI_MAX_BINDINGS_PER_GROUP
			|| ( entryCount && !entries ) ) return qfalse;
	memset( converted, 0, sizeof( converted ) );
	for ( uint32_t i = 0u; i < entryCount; ++i ) {
		converted[i].binding = entries[i].binding;
		converted[i].bindingClass = (uint32_t)entries[i].bindingClass;
		converted[i].arrayCount = entries[i].arrayCount;
		converted[i].stageFlags = entries[i].stageFlags;
		converted[i].minBufferBindingSize = entries[i].minBufferBindingSize;
		converted[i].viewDimension = (uint32_t)entries[i].viewDimension;
		converted[i].sampleType = (uint32_t)entries[i].sampleType;
		converted[i].storageTextureFormat = (uint32_t)entries[i].storageTextureFormat;
		converted[i].dynamicOffset = entries[i].dynamicOffset;
	}
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP_LAYOUT,
		sizeof( request ) ); request.deviceIdentity = device;
	request.entriesOffset = entryCount ? (uint64_t)(uintptr_t)converted : 0u;
	request.group = group; request.entryCount = entryCount;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP_LAYOUT,
		&request, sizeof( request ), outIdentity );
}

static qboolean CreatePipelineLayout( void *userData, uintptr_t device,
		const uintptr_t *layouts, uint32_t layoutCount,
		uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserPipelineLayoutRequest_t request;
	uint64_t converted[RAL_SHADER_ABI_MAX_BIND_GROUPS];
	if ( !bridge || device != bridge->deviceIdentity || !outIdentity
			|| layoutCount > RAL_SHADER_ABI_MAX_BIND_GROUPS
			|| ( layoutCount && !layouts ) ) return qfalse;
	memset( converted, 0, sizeof( converted ) );
	for ( uint32_t i = 0u; i < layoutCount; ++i ) {
		if ( !layouts[i] ) return qfalse; converted[i] = layouts[i];
	}
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE_LAYOUT,
		sizeof( request ) ); request.deviceIdentity = device;
	request.layoutsOffset = layoutCount ? (uint64_t)(uintptr_t)converted : 0u;
	request.layoutCount = layoutCount;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE_LAYOUT,
		&request, sizeof( request ), outIdentity );
}

static void CopyStencil( ralWebGpuBrowserStencilAbi_t *out,
		const ralStencilOpState_t *source ) {
	out->failOp = source->failOp; out->passOp = source->passOp;
	out->depthFailOp = source->depthFailOp; out->compareOp = source->compareOp;
	out->compareMask = source->compareMask; out->writeMask = source->writeMask;
	out->reference = source->reference;
}

static qboolean CreatePipeline( void *userData, uintptr_t device,
		const ralWebGpuNativePipelineDesc_t *desc, uintptr_t *outIdentity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserPipelineRequest_t request;
	if ( !bridge || device != bridge->deviceIdentity || !desc || !outIdentity
			|| desc->shaderModuleCount > RAL_SHADER_ABI_MAX_MODULES
			|| desc->bindGroupLayoutCount > RAL_SHADER_ABI_MAX_BIND_GROUPS
			|| desc->specValueCount > RAL_SHADER_ABI_MAX_SPEC_CONSTANTS
			|| ( desc->specValueCount && !desc->specValues )
			|| !desc->pipelineLayout ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE,
		sizeof( request ) ); request.deviceIdentity = device;
	request.pipelineLayoutIdentity = desc->pipelineLayout;
	request.kind = desc->kind; request.shaderModuleCount = desc->shaderModuleCount;
	request.bindGroupLayoutCount = desc->bindGroupLayoutCount;
	request.specValueCount = desc->specValueCount;
	for ( uint32_t i = 0u; i < desc->shaderModuleCount; ++i ) {
		if ( !desc->shaderModules[i] ) return qfalse;
		request.shaderModules[i] = desc->shaderModules[i];
	}
	for ( uint32_t i = 0u; i < desc->bindGroupLayoutCount; ++i ) {
		if ( !desc->bindGroupLayouts[i] ) return qfalse;
		request.bindGroupLayouts[i] = desc->bindGroupLayouts[i];
	}
	for ( uint32_t i = 0u; i < desc->specValueCount; ++i ) {
		request.specValues[i].constantId = desc->specValues[i].constantId;
		request.specValues[i].value = desc->specValues[i].value;
	}
	if ( desc->kind == RAL_SHADER_PIPELINE_GRAPHICS ) {
		const ralGraphicsPipelineCreateInfo_t *graphics = desc->graphicsState;
		if ( !graphics || desc->computeState
				|| graphics->numVertexBindings > RAL_SHADER_ABI_MAX_VERTEX_INPUTS
				|| graphics->numVertexAttributes > RAL_SHADER_ABI_MAX_VERTEX_INPUTS
				|| graphics->numColorBlends > RAL_MAX_COLOR_ATTACHMENTS
				|| graphics->numColorFormats > RAL_MAX_COLOR_ATTACHMENTS
				|| ( graphics->numVertexBindings && !graphics->vertexBindings )
				|| ( graphics->numVertexAttributes && !graphics->vertexAttributes )
				|| ( graphics->numColorBlends && !graphics->colorBlends ) ) return qfalse;
		request.topology = graphics->topology;
		request.polygonMode = graphics->raster.polygonMode;
		request.cullMode = graphics->raster.cullMode;
		request.frontFace = graphics->raster.frontFace;
		request.depthBiasEnable = graphics->raster.depthBiasEnable;
		request.depthBiasConstant = graphics->raster.depthBiasConstant;
		request.depthBiasSlope = graphics->raster.depthBiasSlope;
		request.depthBiasClamp = graphics->raster.depthBiasClamp;
		request.depthClampEnable = graphics->raster.depthClampEnable;
		request.lineWidth = graphics->raster.lineWidth;
		request.depthTestEnable = graphics->depthStencil.depthTestEnable;
		request.depthWriteEnable = graphics->depthStencil.depthWriteEnable;
		request.depthCompareOp = graphics->depthStencil.depthCompareOp;
		request.stencilTestEnable = graphics->depthStencil.stencilTestEnable;
		CopyStencil( &request.stencilFront, &graphics->depthStencil.stencilFront );
		CopyStencil( &request.stencilBack, &graphics->depthStencil.stencilBack );
		request.numColorFormats = graphics->numColorFormats;
		request.depthFormat = graphics->depthFormat;
		request.sampleCount = graphics->sampleCount;
		request.vertexBindingCount = graphics->numVertexBindings;
		request.vertexAttributeCount = graphics->numVertexAttributes;
		request.colorBlendCount = graphics->numColorBlends;
		for ( uint32_t i = 0u; i < graphics->numColorFormats; ++i )
			request.colorFormats[i] = graphics->colorFormats[i];
		for ( uint32_t i = 0u; i < graphics->numVertexBindings; ++i ) {
			request.vertexBindings[i].binding = graphics->vertexBindings[i].binding;
			request.vertexBindings[i].stride = graphics->vertexBindings[i].stride;
			request.vertexBindings[i].inputRate = graphics->vertexBindings[i].inputRate;
		}
		for ( uint32_t i = 0u; i < graphics->numVertexAttributes; ++i ) {
			request.vertexAttributes[i].location = graphics->vertexAttributes[i].location;
			request.vertexAttributes[i].binding = graphics->vertexAttributes[i].binding;
			request.vertexAttributes[i].format = graphics->vertexAttributes[i].format;
			request.vertexAttributes[i].offset = graphics->vertexAttributes[i].offset;
		}
		for ( uint32_t i = 0u; i < graphics->numColorBlends; ++i ) {
			const ralColorBlendAttachment_t *source = &graphics->colorBlends[i];
			ralWebGpuBrowserBlendAbi_t *target = &request.colorBlends[i];
			target->blendEnable = source->blendEnable; target->srcColor = source->srcColor;
			target->dstColor = source->dstColor; target->colorOp = source->colorOp;
			target->srcAlpha = source->srcAlpha; target->dstAlpha = source->dstAlpha;
			target->alphaOp = source->alphaOp; target->writeMask = source->writeMask;
			target->writeMaskExplicit = source->writeMaskExplicit;
		}
	} else if ( desc->kind == RAL_SHADER_PIPELINE_COMPUTE ) {
		if ( !desc->computeState || desc->graphicsState ) return qfalse;
	} else return qfalse;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE,
		&request, sizeof( request ), outIdentity );
}

static void DestroyPipelineObject( void *userData,
		ralWebGpuPipelineObjectKind_t kind, uintptr_t identity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserDestroyObjectRequest_t request;
	if ( !bridge || kind < RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE
			|| kind > RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE || !identity ) return;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_DESTROY_PIPELINE_OBJECT,
		sizeof( request ) ); request.kind = kind; request.identity = identity;
	(void)StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_DESTROY_PIPELINE_OBJECT,
		&request, sizeof( request ) );
}

static qboolean BeginEncoder( void *userData, uintptr_t device,
		uintptr_t *outEncoder ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserBeginEncoderRequest_t request;
	if ( !bridge || device != bridge->deviceIdentity ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_BEGIN_ENCODER,
		sizeof( request ) ); request.deviceIdentity = device;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_BEGIN_ENCODER,
		&request, sizeof( request ), outEncoder );
}

static qboolean BeginPass( void *userData, uintptr_t encoder,
		ralWebGpuPassKind_t kind, uintptr_t target, uintptr_t *outPass ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserBeginPassRequest_t request;
	if ( !bridge || !encoder || !target || kind < RAL_WEBGPU_PASS_RENDER
			|| kind > RAL_WEBGPU_PASS_COMPUTE ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_BEGIN_PASS,
		sizeof( request ) ); request.encoderIdentity = encoder;
	request.targetIdentity = target; request.passKind = kind;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_BEGIN_PASS,
		&request, sizeof( request ), outPass );
}

static qboolean RecordIndexedDraw( void *userData, uintptr_t pass,
		const ralWebGpuIndexedDraw_t *draw ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserIndexedDrawRequest_t request;
	if ( !bridge || !pass || !draw || !draw->pipelineIdentity
			|| !draw->vertexBufferIdentity || !draw->indexBufferIdentity
			|| !draw->indexCount || !draw->instanceCount || !draw->contentDigest )
		return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_RECORD_INDEXED_DRAW,
		sizeof( request ) ); request.passIdentity = pass;
	request.pipelineIdentity = draw->pipelineIdentity;
	request.vertexBufferIdentity = draw->vertexBufferIdentity;
	request.indexBufferIdentity = draw->indexBufferIdentity;
	request.textureIdentity = draw->textureIdentity;
	request.secondaryTextureIdentity = draw->secondaryTextureIdentity;
	request.samplerIdentity = draw->samplerIdentity;
	request.contentDigest = draw->contentDigest; request.drawKind = draw->kind;
	request.textured = draw->textured; request.firstIndex = draw->firstIndex;
	request.indexCount = draw->indexCount; request.instanceCount = draw->instanceCount;
	return StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_RECORD_INDEXED_DRAW,
		&request, sizeof( request ) );
}

static qboolean EndPass( void *userData, uintptr_t pass ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserIdentityRequest_t request;
	if ( !bridge || !pass ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_END_PASS,
		sizeof( request ) ); request.identity = pass;
	return StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_END_PASS,
		&request, sizeof( request ) );
}

static qboolean FinishEncoder( void *userData, uintptr_t encoder,
		uintptr_t *outCommandBuffer ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserIdentityRequest_t request;
	if ( !bridge || !encoder ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_FINISH_ENCODER,
		sizeof( request ) ); request.identity = encoder;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_FINISH_ENCODER,
		&request, sizeof( request ), outCommandBuffer );
}

static qboolean Submit( void *userData, uintptr_t queue,
		uintptr_t commandBuffer, uint64_t submissionGeneration,
		uintptr_t *outSubmission ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserSubmitRequest_t request;
	if ( !bridge || queue != bridge->queueIdentity || !commandBuffer
			|| !submissionGeneration ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_SUBMIT,
		sizeof( request ) ); request.queueIdentity = queue;
	request.commandBufferIdentity = commandBuffer;
	request.submissionGeneration = submissionGeneration;
	return IdentityCall( bridge, RAL_WEBGPU_BROWSER_OP_SUBMIT,
		&request, sizeof( request ), outSubmission );
}

static qboolean PollSubmission( void *userData, uintptr_t submission,
		uint64_t submissionGeneration, ralWebGpuAsyncStatus_t *outStatus ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserPollSubmissionRequest_t request;
	ralWebGpuBrowserAsyncResponse_t response;
	if ( !bridge || !submission || !submissionGeneration || !outStatus )
		return qfalse;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_POLL_SUBMISSION,
		sizeof( request ) ); request.submissionIdentity = submission;
	request.submissionGeneration = submissionGeneration;
	if ( !Call( bridge, RAL_WEBGPU_BROWSER_OP_POLL_SUBMISSION, &request,
			sizeof( request ), &response, sizeof( response ) )
			|| response.status < RAL_WEBGPU_ASYNC_PENDING
			|| response.status > RAL_WEBGPU_ASYNC_DEVICE_LOST
			|| response.byteSize ) return qfalse;
	*outStatus = (ralWebGpuAsyncStatus_t)response.status; return qtrue;
}

static void ReleaseCommandObject( void *userData,
		ralWebGpuCommandObjectKind_t kind, uintptr_t identity ) {
	ralWebGpuBrowserBridge_t *bridge = userData;
	ralWebGpuBrowserDestroyObjectRequest_t request;
	if ( !bridge || kind < RAL_WEBGPU_COMMAND_OBJECT_ENCODER
			|| kind > RAL_WEBGPU_COMMAND_OBJECT_SUBMISSION || !identity ) return;
	memset( &request, 0, sizeof( request ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_RELEASE_COMMAND_OBJECT,
		sizeof( request ) ); request.kind = kind; request.identity = identity;
	(void)StatusCall( bridge, RAL_WEBGPU_BROWSER_OP_RELEASE_COMMAND_OBJECT,
		&request, sizeof( request ) );
}

qboolean RalWebGpu_BrowserBridgeCreate(
		const ralWebGpuBrowserBridgeCreateInfo_t *createInfo,
		ralWebGpuBrowserBridge_t **outBridge ) {
	ralWebGpuBrowserBridge_t *bridge;
	if ( !createInfo || !outBridge
			|| createInfo->schemaVersion != RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION
			|| !createInfo->generation || createInfo->generation >= UINT64_MAX - 1u
			|| !createInfo->canvasIdentity || !createInfo->dispatch ) return qfalse;
	bridge = calloc( 1u, sizeof( *bridge ) ); if ( !bridge ) return qfalse;
	bridge->generation = createInfo->generation;
	bridge->canvasIdentity = createInfo->canvasIdentity;
	bridge->userData = createInfo->userData; bridge->dispatch = createInfo->dispatch;
	*outBridge = bridge; return qtrue;
}

void RalWebGpu_BrowserBridgeDestroy( ralWebGpuBrowserBridge_t *bridge ) {
	if ( !bridge ) return;
	memset( bridge, 0, sizeof( *bridge ) ); free( bridge );
}

qboolean RalWebGpu_BrowserBridgeBuildCoreInfo(
		ralWebGpuBrowserBridge_t *bridge, ralWebGpuCoreCreateInfo_t *outInfo ) {
	if ( !bridge || !outInfo ) return qfalse;
	memset( outInfo, 0, sizeof( *outInfo ) ); outInfo->generation = bridge->generation;
	outInfo->userData = bridge; outInfo->host.beginAdapter = BeginAdapter;
	outInfo->host.pollAdapter = PollAdapter; outInfo->host.beginDevice = BeginDevice;
	outInfo->host.pollDevice = PollDevice; outInfo->host.releaseDevice = ReleaseDevice;
	outInfo->host.releaseAdapter = ReleaseAdapter; return qtrue;
}

qboolean RalWebGpu_BrowserBridgeBuildPresentationInfo(
		ralWebGpuBrowserBridge_t *bridge,
		ralWebGpuPresentationCreateInfo_t *outInfo ) {
	if ( !bridge || !outInfo ) return qfalse;
	memset( outInfo, 0, sizeof( *outInfo ) ); outInfo->userData = bridge;
	outInfo->canvasIdentity = bridge->canvasIdentity;
	outInfo->host.configure = CanvasConfigure;
	outInfo->host.unconfigure = CanvasUnconfigure;
	outInfo->host.acquire = CanvasAcquire; outInfo->host.present = CanvasPresent;
	return qtrue;
}

qboolean RalWebGpu_BrowserBridgeBuildResourceInfo(
		ralWebGpuBrowserBridge_t *bridge,
		ralWebGpuResourceLayerCreateInfo_t *outInfo ) {
	if ( !bridge || !outInfo ) return qfalse;
	memset( outInfo, 0, sizeof( *outInfo ) ); outInfo->userData = bridge;
	outInfo->host.createBuffer = CreateBuffer;
	outInfo->host.createTexture = CreateTexture;
	outInfo->host.createSampler = CreateSampler;
	outInfo->host.destroyResource = DestroyResource;
	outInfo->host.writeBuffer = WriteBuffer;
	outInfo->host.writeTexture = WriteTexture;
	outInfo->host.beginRoundTrip = BeginRoundTrip;
	outInfo->host.pollRoundTrip = PollRoundTrip;
	outInfo->host.releaseOperation = ReleaseOperation; return qtrue;
}

qboolean RalWebGpu_BrowserBridgeBuildCommandInfo(
		ralWebGpuBrowserBridge_t *bridge, ralWebGpuCommandCreateInfo_t *outInfo ) {
	if ( !bridge || !outInfo ) return qfalse;
	memset( outInfo, 0, sizeof( *outInfo ) ); outInfo->userData = bridge;
	outInfo->host.beginEncoder = BeginEncoder; outInfo->host.beginPass = BeginPass;
	outInfo->host.recordIndexedDraw = RecordIndexedDraw;
	outInfo->host.endPass = EndPass; outInfo->host.finishEncoder = FinishEncoder;
	outInfo->host.submit = Submit; outInfo->host.pollSubmission = PollSubmission;
	outInfo->host.releaseObject = ReleaseCommandObject; return qtrue;
}

qboolean RalWebGpu_BrowserBridgeApplyPipelineInfo(
		ralWebGpuBrowserBridge_t *bridge, ralWebGpuPipelineCreateInfo_t *info ) {
	if ( !bridge || !info ) return qfalse;
	info->userData = bridge; info->host.createShaderModule = CreateShaderModule;
	info->host.createBindGroupLayout = CreateBindGroupLayout;
	info->host.createPipelineLayout = CreatePipelineLayout;
	info->host.createPipeline = CreatePipeline;
	info->host.destroyObject = DestroyPipelineObject; return qtrue;
}

qboolean RalWebGpu_BrowserBridgePollLoss(
		ralWebGpuBrowserBridge_t *bridge, ralWebGpuBrowserLoss_t *outLoss ) {
	ralWebGpuBrowserGenerationRequest_t request;
	ralWebGpuBrowserLossResponse_t response;
	if ( !bridge || !outLoss || !bridge->deviceIdentity ) return qfalse;
	memset( &request, 0, sizeof( request ) ); memset( &response, 0, sizeof( response ) );
	Header( &request.header, RAL_WEBGPU_BROWSER_OP_POLL_DEVICE_LOSS,
		sizeof( request ) ); request.requestGeneration = bridge->generation;
	if ( !Call( bridge, RAL_WEBGPU_BROWSER_OP_POLL_DEVICE_LOSS, &request,
			sizeof( request ), &response, sizeof( response ) )
			|| response.deviceIdentity != bridge->deviceIdentity
			|| response.lost > 1u || response.recoverable > 1u
			|| !memchr( response.reason, '\0', sizeof( response.reason ) ) ) return qfalse;
	memset( outLoss, 0, sizeof( *outLoss ) ); outLoss->lost = response.lost;
	outLoss->recoverable = response.recoverable;
	memcpy( outLoss->reason, response.reason, sizeof( outLoss->reason ) );
	return qtrue;
}
