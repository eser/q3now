// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_resource.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	TRANSACTION_IDLE = 0,
	TRANSACTION_PENDING,
	TRANSACTION_FAILED,
	TRANSACTION_LOST
} transactionState_t;

struct ralWebGpuResource_s {
	ralWebGpuResourceLayer_t *owner;
	ralWebGpuResource_t *next;
	ralWebGpuResourceKind_t kind;
	uintptr_t identity;
	uint64_t generation;
	uint64_t byteSize;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t bytesPerTexel;
	ralFormat_t format;
	ralAllocationReceipt_t allocation;
	qboolean live;
};

typedef struct {
	uintptr_t operationIdentity;
	uint64_t operationGeneration;
	uint64_t byteCount;
	unsigned char *expected;
	unsigned char *readback;
	ralWebGpuResource_t *upload;
	ralWebGpuResource_t *readbackResource;
	ralWebGpuResource_t *texture;
	ralWebGpuResource_t *sampler;
	ralWebGpuResourceReceipt_t uploadReceipt;
	ralWebGpuResourceReceipt_t readbackReceipt;
	ralWebGpuResourceReceipt_t textureReceipt;
	ralWebGpuResourceReceipt_t samplerReceipt;
	ralSubmissionReceipt_t submission;
	ralTransferReceipt_t submittedTransfer;
} conformanceTransaction_t;

struct ralWebGpuResourceLayer_s {
	ralWebGpuCore_t *core;
	ralWebGpuCoreReceipt_t coreReceipt;
	void *userData;
	ralWebGpuResourceHostOps_t host;
	uint64_t allocationGeneration;
	uint64_t resourceGeneration;
	uint64_t submissionGeneration;
	uint64_t writeGeneration;
	uint32_t liveResourceCount;
	ralWebGpuResource_t *resources;
	transactionState_t transactionState;
	conformanceTransaction_t transaction;
};

static qboolean LayerReady( const ralWebGpuResourceLayer_t *layer ) {
	return layer && RalWebGpu_CoreMatchesReceipt( layer->core,
		&layer->coreReceipt );
}

static uint64_t HashBytes( const void *bytes, uint64_t byteSize ) {
	const unsigned char *source = (const unsigned char *)bytes;
	uint64_t digest = UINT64_C(1469598103934665603);
	for ( uint64_t i = 0u; i < byteSize; ++i ) {
		digest ^= source[i]; digest *= UINT64_C(1099511628211);
	}
	return digest ? digest : 1u;
}

static qboolean AllocationBuild( ralWebGpuResourceLayer_t *layer,
		uintptr_t identity, ralAllocationClass_t memoryClass, uint64_t size,
		uint64_t alignment, ralAllocationReceipt_t *out ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	if ( !layer || !identity || !size || !alignment
			|| layer->allocationGeneration == UINT64_MAX ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	memset( &facts, 0, sizeof( facts ) );
	request.memoryClass = memoryClass;
	request.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request.size = size;
	request.alignment = alignment;
	request.ownerIdentity = identity;
	request.ownerGeneration = layer->coreReceipt.generation;
	request.allowFallback = qtrue;
	facts.backendType = RAL_BACKEND_WEBGPU;
	facts.placement = RAL_ALLOCATION_PLACEMENT_MANAGED;
	facts.committedSize = ( size + alignment - 1u ) & ~( alignment - 1u );
	facts.actualAlignment = alignment;
	facts.allocationGeneration = ++layer->allocationGeneration;
	facts.deviceLocal = memoryClass == RAL_ALLOCATION_DEVICE_LOCAL ? qtrue : qfalse;
	facts.hostVisible = memoryClass == RAL_ALLOCATION_UPLOAD
		|| memoryClass == RAL_ALLOCATION_READBACK ? qtrue : qfalse;
	facts.hostCoherent = facts.hostVisible;
	return Ral_AllocationReceiptBuild( &request, &facts, out );
}

static qboolean ReceiptValid( const ralWebGpuResourceReceipt_t *receipt ) {
	static const ralAllocationReceipt_t empty;
	if ( !receipt || receipt->schemaVersion != RAL_WEBGPU_RESOURCE_SCHEMA_VERSION
			|| receipt->kind < RAL_WEBGPU_RESOURCE_BUFFER
			|| receipt->kind > RAL_WEBGPU_RESOURCE_SAMPLER
			|| !receipt->backendGeneration
			|| receipt->backendGeneration == UINT64_MAX
			|| !receipt->resourceGeneration
			|| receipt->resourceGeneration == UINT64_MAX
			|| !receipt->resourceIdentity || receipt->ready != qtrue ) return qfalse;
	if ( receipt->kind == RAL_WEBGPU_RESOURCE_SAMPLER )
		return receipt->byteSize == 0u
			&& !memcmp( &receipt->allocation, &empty, sizeof( empty ) );
	return receipt->byteSize != 0u
		&& Ral_AllocationReceiptExact( &receipt->allocation,
			&receipt->allocation )
		&& receipt->allocation.backendType == RAL_BACKEND_WEBGPU
		&& receipt->allocation.ownerIdentity == receipt->resourceIdentity
		&& receipt->allocation.ownerGeneration == receipt->backendGeneration
		&& receipt->allocation.requestedSize == receipt->byteSize;
}

static void BuildResourceReceipt( const ralWebGpuResource_t *resource,
		ralWebGpuResourceReceipt_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = RAL_WEBGPU_RESOURCE_SCHEMA_VERSION;
	out->kind = resource->kind;
	out->backendGeneration = resource->owner->coreReceipt.generation;
	out->resourceGeneration = resource->generation;
	out->resourceIdentity = resource->identity;
	out->byteSize = resource->byteSize;
	out->allocation = resource->allocation;
	out->ready = qtrue;
}

static qboolean PublishResource( ralWebGpuResourceLayer_t *layer,
		ralWebGpuResourceKind_t kind, uintptr_t identity, uint64_t byteSize,
		ralAllocationClass_t memoryClass, uint64_t alignment,
		ralWebGpuResource_t **outResource, ralWebGpuResourceReceipt_t *outReceipt ) {
	ralWebGpuResource_t *resource;
	ralWebGpuResourceReceipt_t receipt;
	if ( !LayerReady( layer ) || !identity || !outResource || !outReceipt
			|| layer->resourceGeneration == UINT64_MAX ) return qfalse;
	resource = (ralWebGpuResource_t *)calloc( 1u, sizeof( *resource ) );
	if ( !resource ) return qfalse;
	resource->owner = layer;
	resource->kind = kind;
	resource->identity = identity;
	resource->generation = ++layer->resourceGeneration;
	resource->byteSize = byteSize;
	resource->live = qtrue;
	if ( kind != RAL_WEBGPU_RESOURCE_SAMPLER
			&& !AllocationBuild( layer, identity, memoryClass, byteSize,
				alignment, &resource->allocation ) ) {
		free( resource ); return qfalse;
	}
	BuildResourceReceipt( resource, &receipt );
	if ( !ReceiptValid( &receipt ) ) { free( resource ); return qfalse; }
	resource->next = layer->resources;
	layer->resources = resource;
	layer->liveResourceCount++;
	*outResource = resource;
	*outReceipt = receipt;
	return qtrue;
}

qboolean RalWebGpu_ResourcesCreate( ralWebGpuCore_t *core,
		const ralWebGpuCoreReceipt_t *coreReceipt,
		const ralWebGpuResourceLayerCreateInfo_t *createInfo,
		ralWebGpuResourceLayer_t **outLayer ) {
	ralWebGpuResourceLayer_t *layer;
	if ( !core || !coreReceipt || !createInfo || !outLayer
			|| !RalWebGpu_CoreMatchesReceipt( core, coreReceipt )
			|| !createInfo->host.createBuffer || !createInfo->host.createTexture
			|| !createInfo->host.createSampler
			|| !createInfo->host.destroyResource
			|| !createInfo->host.writeBuffer
			|| !createInfo->host.writeTexture
			|| !createInfo->host.beginRoundTrip
			|| !createInfo->host.pollRoundTrip
			|| !createInfo->host.releaseOperation ) return qfalse;
	layer = (ralWebGpuResourceLayer_t *)calloc( 1u, sizeof( *layer ) );
	if ( !layer ) return qfalse;
	layer->core = core;
	layer->coreReceipt = *coreReceipt;
	layer->userData = createInfo->userData;
	layer->host = createInfo->host;
	layer->allocationGeneration = coreReceipt->generation;
	layer->resourceGeneration = coreReceipt->generation;
	layer->submissionGeneration = coreReceipt->generation;
	layer->writeGeneration = coreReceipt->generation;
	*outLayer = layer;
	return qtrue;
}

qboolean RalWebGpu_CreateBuffer( ralWebGpuResourceLayer_t *layer,
		const ralWebGpuBufferDesc_t *desc, ralWebGpuResource_t **outResource,
		ralWebGpuResourceReceipt_t *outReceipt ) {
	uintptr_t identity = (uintptr_t)0;
	if ( !LayerReady( layer ) || !desc || !outResource || !outReceipt
			|| !desc->size || desc->size > RAL_WEBGPU_MAX_RESOURCE_BYTES
			|| ( desc->size & 3u ) != 0u || !desc->usage
			|| ( desc->usage & ~(uint32_t)( RAL_WEBGPU_BUFFER_COPY_SOURCE
				| RAL_WEBGPU_BUFFER_COPY_DESTINATION | RAL_WEBGPU_BUFFER_VERTEX
				| RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_UNIFORM
				| RAL_WEBGPU_BUFFER_STORAGE | RAL_WEBGPU_BUFFER_MAP_READ ) ) != 0u
			|| desc->memoryClass < RAL_ALLOCATION_DEVICE_LOCAL
			|| desc->memoryClass > RAL_ALLOCATION_READBACK
			|| !layer->host.createBuffer( layer->userData,
				layer->coreReceipt.deviceIdentity, desc, &identity )
			|| !identity ) return qfalse;
	if ( !PublishResource( layer, RAL_WEBGPU_RESOURCE_BUFFER, identity,
			desc->size, desc->memoryClass, 4u, outResource, outReceipt ) ) {
		layer->host.destroyResource( layer->userData,
			RAL_WEBGPU_RESOURCE_BUFFER, identity ); return qfalse;
	}
	return qtrue;
}

qboolean RalWebGpu_CreateTexture( ralWebGpuResourceLayer_t *layer,
		const ralWebGpuTextureDesc_t *desc, ralWebGpuResource_t **outResource,
		ralWebGpuResourceReceipt_t *outReceipt ) {
	uintptr_t identity = (uintptr_t)0;
	uint64_t texels, byteSize;
	uint32_t expectedBytes = desc && desc->format == RAL_FORMAT_R8_UNORM ? 1u
		: desc && desc->format == RAL_FORMAT_R8G8_UNORM ? 2u
		: desc && ( desc->format == RAL_FORMAT_R8G8B8A8_UNORM
			|| desc->format == RAL_FORMAT_R8G8B8A8_SRGB
			|| desc->format == RAL_FORMAT_E5B9G9R9_UFLOAT
			|| desc->format == RAL_FORMAT_R16G16_SNORM ) ? 4u
		: desc && desc->format == RAL_FORMAT_R16G16B16A16_SFLOAT ? 8u : 0u;
	if ( !LayerReady( layer ) || !desc || !outResource || !outReceipt
			|| !desc->width || !desc->height || !desc->depth
			|| !expectedBytes || desc->bytesPerTexel != expectedBytes
			|| desc->width > layer->coreReceipt.caps.maxTextureDimension2D
			|| desc->height > layer->coreReceipt.caps.maxTextureDimension2D
			|| desc->depth > layer->coreReceipt.caps.maxTextureDimension3D
			|| desc->width > UINT64_MAX / desc->height ) return qfalse;
	texels = (uint64_t)desc->width * desc->height;
	if ( texels > UINT64_MAX / desc->depth ) return qfalse;
	texels *= desc->depth;
	if ( texels > UINT64_MAX / desc->bytesPerTexel ) return qfalse;
	byteSize = texels * desc->bytesPerTexel;
	if ( byteSize > RAL_WEBGPU_MAX_RESOURCE_BYTES
			|| !layer->host.createTexture( layer->userData,
				layer->coreReceipt.deviceIdentity, desc, &identity )
			|| !identity ) return qfalse;
	if ( !PublishResource( layer, RAL_WEBGPU_RESOURCE_TEXTURE, identity,
			byteSize, RAL_ALLOCATION_DEVICE_LOCAL, 4u,
			outResource, outReceipt ) ) {
		layer->host.destroyResource( layer->userData,
			RAL_WEBGPU_RESOURCE_TEXTURE, identity ); return qfalse;
	}
	( *outResource )->width = desc->width;
	( *outResource )->height = desc->height;
	( *outResource )->depth = desc->depth;
	( *outResource )->bytesPerTexel = desc->bytesPerTexel;
	( *outResource )->format = desc->format;
	return qtrue;
}

qboolean RalWebGpu_CreateSampler( ralWebGpuResourceLayer_t *layer,
		const ralWebGpuSamplerDesc_t *desc, ralWebGpuResource_t **outResource,
		ralWebGpuResourceReceipt_t *outReceipt ) {
	uintptr_t identity = (uintptr_t)0;
	if ( !LayerReady( layer ) || !desc || !outResource || !outReceipt
			|| ( desc->linearMinification != qfalse
				&& desc->linearMinification != qtrue )
			|| ( desc->linearMagnification != qfalse
				&& desc->linearMagnification != qtrue )
			|| ( desc->clampToEdge != qfalse && desc->clampToEdge != qtrue )
			|| !layer->host.createSampler( layer->userData,
				layer->coreReceipt.deviceIdentity, desc, &identity )
			|| !identity ) return qfalse;
	if ( !PublishResource( layer, RAL_WEBGPU_RESOURCE_SAMPLER, identity,
			0u, RAL_ALLOCATION_DEVICE_LOCAL, 1u, outResource, outReceipt ) ) {
		layer->host.destroyResource( layer->userData,
			RAL_WEBGPU_RESOURCE_SAMPLER, identity ); return qfalse;
	}
	return qtrue;
}

qboolean RalWebGpu_ResourceReceiptExact( const ralWebGpuResourceReceipt_t *a,
		const ralWebGpuResourceReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean WriteReceiptValid( const ralWebGpuWriteReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_WEBGPU_RESOURCE_SCHEMA_VERSION
		&& receipt->kind >= RAL_WEBGPU_WRITE_BUFFER
		&& receipt->kind <= RAL_WEBGPU_WRITE_TEXTURE
		&& receipt->backendGeneration && receipt->resourceGeneration
		&& receipt->writeGeneration && receipt->writeGeneration != UINT64_MAX
		&& receipt->byteSize && receipt->contentDigest
		&& receipt->ready == qtrue;
}

qboolean RalWebGpu_WriteReceiptExact( const ralWebGpuWriteReceipt_t *a,
		const ralWebGpuWriteReceipt_t *b ) {
	return WriteReceiptValid( a ) && WriteReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static void BuildWriteReceipt( ralWebGpuResourceLayer_t *layer,
		const ralWebGpuResource_t *resource, ralWebGpuWriteKind_t kind,
		uint64_t byteOffset, const void *bytes, uint64_t byteSize,
		ralWebGpuWriteReceipt_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = RAL_WEBGPU_RESOURCE_SCHEMA_VERSION;
	out->kind = kind; out->backendGeneration = layer->coreReceipt.generation;
	out->resourceGeneration = resource->generation;
	out->writeGeneration = ++layer->writeGeneration;
	out->byteOffset = byteOffset; out->byteSize = byteSize;
	out->contentDigest = HashBytes( bytes, byteSize ); out->ready = qtrue;
}

qboolean RalWebGpu_WriteBuffer( ralWebGpuResourceLayer_t *layer,
		ralWebGpuResource_t *resource,
		const ralWebGpuResourceReceipt_t *authority, uint64_t byteOffset,
		const void *bytes, uint64_t byteSize,
		ralWebGpuWriteReceipt_t *outReceipt ) {
	ralWebGpuWriteReceipt_t candidate;
	if ( !LayerReady( layer ) || !resource || resource->owner != layer
			|| resource->live != qtrue || resource->kind != RAL_WEBGPU_RESOURCE_BUFFER
			|| !authority || !RalWebGpu_ResourceReceiptExact( authority, authority )
			|| authority->resourceGeneration != resource->generation
			|| authority->resourceIdentity != resource->identity || !bytes
			|| !byteSize || ( byteOffset & 3u ) || ( byteSize & 3u )
			|| byteOffset > resource->byteSize
			|| byteSize > resource->byteSize - byteOffset || !outReceipt
			|| layer->writeGeneration == UINT64_MAX ) return qfalse;
	if ( !layer->host.writeBuffer( layer->userData,
			layer->coreReceipt.queueIdentity, resource->identity, byteOffset,
			bytes, byteSize ) ) return qfalse;
	BuildWriteReceipt( layer, resource, RAL_WEBGPU_WRITE_BUFFER, byteOffset,
		bytes, byteSize, &candidate );
	if ( !WriteReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate; return qtrue;
}

qboolean RalWebGpu_WriteTexture( ralWebGpuResourceLayer_t *layer,
		ralWebGpuResource_t *resource,
		const ralWebGpuResourceReceipt_t *authority, const void *bytes,
		uint64_t byteSize, uint32_t bytesPerRow, uint32_t rowsPerImage,
		ralWebGpuWriteReceipt_t *outReceipt ) {
	ralWebGpuWriteReceipt_t candidate;
	uint64_t minimumRow, requiredBytes;
	if ( !LayerReady( layer ) || !resource || resource->owner != layer
			|| resource->live != qtrue || resource->kind != RAL_WEBGPU_RESOURCE_TEXTURE
			|| !authority || !RalWebGpu_ResourceReceiptExact( authority, authority )
			|| authority->resourceGeneration != resource->generation
			|| authority->resourceIdentity != resource->identity || !bytes
			|| !byteSize || !bytesPerRow
			|| !rowsPerImage || rowsPerImage != resource->height
			|| !resource->depth || !resource->width || !resource->bytesPerTexel
			|| resource->width > UINT32_MAX / resource->bytesPerTexel
			|| !outReceipt || layer->writeGeneration == UINT64_MAX ) return qfalse;
	minimumRow = (uint64_t)resource->width * resource->bytesPerTexel;
	requiredBytes = (uint64_t)bytesPerRow * rowsPerImage * resource->depth;
	if ( bytesPerRow < minimumRow || ( rowsPerImage > 1u && ( bytesPerRow & 255u ) )
			|| requiredBytes != byteSize ) return qfalse;
	if ( !layer->host.writeTexture( layer->userData,
			layer->coreReceipt.queueIdentity, resource->identity, bytes, byteSize,
			bytesPerRow, rowsPerImage ) ) return qfalse;
	BuildWriteReceipt( layer, resource, RAL_WEBGPU_WRITE_TEXTURE, 0u,
		bytes, byteSize, &candidate );
	if ( !WriteReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate; return qtrue;
}

qboolean RalWebGpu_DestroyResource( ralWebGpuResourceLayer_t *layer,
		ralWebGpuResource_t *resource,
		const ralWebGpuResourceReceipt_t *authority ) {
	ralWebGpuResourceReceipt_t current;
	ralWebGpuResource_t **link;
	if ( !layer || !resource || resource->owner != layer
			|| resource->live != qtrue || !authority ) return qfalse;
	BuildResourceReceipt( resource, &current );
	if ( !RalWebGpu_ResourceReceiptExact( &current, authority ) ) return qfalse;
	link = &layer->resources;
	while ( *link && *link != resource ) link = &( *link )->next;
	if ( *link != resource ) return qfalse;
	layer->host.destroyResource( layer->userData, resource->kind,
		resource->identity );
	*link = resource->next;
	resource->live = qfalse;
	if ( layer->liveResourceCount ) layer->liveResourceCount--;
	memset( resource, 0, sizeof( *resource ) );
	free( resource );
	return qtrue;
}

static void DestroyTransactionResources( ralWebGpuResourceLayer_t *layer ) {
	conformanceTransaction_t *tx = &layer->transaction;
	if ( tx->operationIdentity ) layer->host.releaseOperation( layer->userData,
		tx->operationIdentity );
	if ( tx->sampler ) RalWebGpu_DestroyResource( layer, tx->sampler,
		&tx->samplerReceipt );
	if ( tx->texture ) RalWebGpu_DestroyResource( layer, tx->texture,
		&tx->textureReceipt );
	if ( tx->readbackResource ) RalWebGpu_DestroyResource( layer,
		tx->readbackResource, &tx->readbackReceipt );
	if ( tx->upload ) RalWebGpu_DestroyResource( layer, tx->upload,
		&tx->uploadReceipt );
	free( tx->readback );
	free( tx->expected );
	memset( tx, 0, sizeof( *tx ) );
}

void RalWebGpu_ResourcesDestroy( ralWebGpuResourceLayer_t *layer ) {
	if ( !layer ) return;
	if ( layer->transactionState != TRANSACTION_IDLE )
		DestroyTransactionResources( layer );
	while ( layer->resources ) {
		ralWebGpuResourceReceipt_t receipt;
		BuildResourceReceipt( layer->resources, &receipt );
		if ( !RalWebGpu_DestroyResource( layer, layer->resources, &receipt ) )
			break;
	}
	memset( layer, 0, sizeof( *layer ) );
	free( layer );
}

static uint64_t DigestBytes( const unsigned char *bytes, uint64_t count ) {
	uint64_t digest = UINT64_C(1469598103934665603), i;
	for ( i = 0u; i < count; ++i ) {
		digest ^= bytes[i]; digest *= UINT64_C(1099511628211);
	}
	return digest;
}

qboolean RalWebGpu_OffscreenConformanceBegin(
		ralWebGpuResourceLayer_t *layer, uint64_t byteCount ) {
	conformanceTransaction_t *tx;
	ralWebGpuBufferDesc_t uploadDesc, readbackDesc;
	ralWebGpuTextureDesc_t textureDesc;
	ralWebGpuSamplerDesc_t samplerDesc;
	ralCommandLifecycle_t command;
	ralSubmissionLifecycle_t submissionLifecycle;
	ralCommandLifecycle_t *commands[1];
	ralCommandReceipt_t recording, executable;
	ralTransferRequest_t transferRequest;
	ralTransferReceipt_t prepared;
	unsigned char textureBytes[1024];
	uint64_t i;
	if ( !LayerReady( layer ) || !byteCount
			|| byteCount > RAL_WEBGPU_MAX_CONFORMANCE_BYTES
			|| ( byteCount & 3u ) != 0u
			|| layer->transactionState != TRANSACTION_IDLE
			|| layer->submissionGeneration >= UINT64_MAX - 1u ) return qfalse;
	tx = &layer->transaction;
	memset( tx, 0, sizeof( *tx ) );
	tx->expected = (unsigned char *)malloc( (size_t)byteCount );
	tx->readback = (unsigned char *)malloc( (size_t)byteCount );
	if ( !tx->expected || !tx->readback ) goto fail;
	for ( i = 0u; i < byteCount; ++i )
		tx->expected[i] = (unsigned char)( ( i * 37u + 11u ) & 0xffu );
	memset( &uploadDesc, 0, sizeof( uploadDesc ) );
	uploadDesc.size = byteCount;
	uploadDesc.usage = RAL_WEBGPU_BUFFER_COPY_SOURCE;
	uploadDesc.memoryClass = RAL_ALLOCATION_UPLOAD;
	readbackDesc = uploadDesc;
	readbackDesc.usage = RAL_WEBGPU_BUFFER_COPY_DESTINATION
		| RAL_WEBGPU_BUFFER_MAP_READ;
	readbackDesc.memoryClass = RAL_ALLOCATION_READBACK;
	memset( &textureDesc, 0, sizeof( textureDesc ) );
	textureDesc.width = 4u; textureDesc.height = 4u;
	textureDesc.depth = 1u; textureDesc.format = RAL_FORMAT_R8G8B8A8_UNORM;
	textureDesc.bytesPerTexel = 4u;
	memset( &samplerDesc, 0, sizeof( samplerDesc ) );
	samplerDesc.linearMinification = qtrue;
	samplerDesc.linearMagnification = qtrue;
	if ( !RalWebGpu_CreateBuffer( layer, &uploadDesc, &tx->upload,
			&tx->uploadReceipt )
			|| !RalWebGpu_CreateBuffer( layer, &readbackDesc,
				&tx->readbackResource, &tx->readbackReceipt )
			|| !RalWebGpu_CreateTexture( layer, &textureDesc, &tx->texture,
				&tx->textureReceipt )
			|| !RalWebGpu_CreateSampler( layer, &samplerDesc, &tx->sampler,
				&tx->samplerReceipt ) ) goto fail;
	memset( textureBytes, 0, sizeof( textureBytes ) );
	for ( i = 0u; i < 4u; ++i )
		memcpy( textureBytes + i * 256u, tx->expected,
			(size_t)( byteCount < 16u ? byteCount : 16u ) );
	if ( !layer->host.writeTexture( layer->userData,
			layer->coreReceipt.queueIdentity, tx->texture->identity,
			textureBytes, sizeof( textureBytes ), 256u, 4u ) ) goto fail;
	tx->operationGeneration = layer->submissionGeneration + 1u;
	if ( !layer->host.beginRoundTrip( layer->userData,
			layer->coreReceipt.deviceIdentity, layer->coreReceipt.queueIdentity,
			tx->upload->identity, tx->readbackResource->identity,
			tx->expected, byteCount, tx->operationGeneration,
			&tx->operationIdentity ) || !tx->operationIdentity ) goto fail;
	Ral_CommandLifecycleInit( &command,
		(const ralBackend_t *)layer->coreReceipt.backendIdentity,
		(const ralCommandBuffer_t *)tx->operationIdentity, RAL_QUEUE_GRAPHICS );
	if ( Ral_CommandLifecyclePublishBegin( &command, &recording ) != ralSuccess
			|| Ral_CommandLifecyclePublishEnd( &command, &recording,
				&executable ) != ralSuccess ) goto fail;
	Ral_SubmissionLifecycleInit( &submissionLifecycle,
		(const ralBackend_t *)layer->coreReceipt.backendIdentity,
		RAL_QUEUE_GRAPHICS );
	submissionLifecycle.generation = layer->submissionGeneration;
	commands[0] = &command;
	if ( Ral_SubmissionLifecyclePublish( &submissionLifecycle, commands,
			&executable, 1u, &tx->submission ) != ralSuccess ) goto fail;
	memset( &transferRequest, 0, sizeof( transferRequest ) );
	transferRequest.backendType = RAL_BACKEND_WEBGPU;
	transferRequest.direction = RAL_TRANSFER_READBACK;
	transferRequest.resourceKind = RAL_TRANSFER_BUFFER;
	transferRequest.resourceIdentity = tx->readbackReceipt.resourceIdentity;
	transferRequest.resourceGeneration =
		tx->readbackReceipt.allocation.allocationGeneration;
	transferRequest.byteSize = byteCount;
	transferRequest.byteBudget = byteCount;
	transferRequest.queue = RAL_QUEUE_GRAPHICS;
	if ( !Ral_TransferPrepare( &transferRequest, tx->submission.generation,
			&prepared )
			|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
				tx->submission.generation, &tx->submittedTransfer ) ) goto fail;
	layer->submissionGeneration = submissionLifecycle.generation;
	tx->byteCount = byteCount;
	layer->transactionState = TRANSACTION_PENDING;
	return qtrue;
fail:
	DestroyTransactionResources( layer );
	return qfalse;
}

ralWebGpuAsyncStatus_t RalWebGpu_OffscreenConformancePoll(
		ralWebGpuResourceLayer_t *layer,
		ralBackendConformanceReceipt_t *outReceipt ) {
	conformanceTransaction_t *tx;
	ralWebGpuAsyncStatus_t hostStatus = RAL_WEBGPU_ASYNC_FAILED;
	ralBackendConformanceFacts_t facts;
	ralBackendConformanceReceipt_t receipt;
	ralTransferReceipt_t completed;
	uint64_t readCount = 0u, digest;
	if ( !layer || layer->transactionState == TRANSACTION_FAILED )
		return RAL_WEBGPU_ASYNC_FAILED;
	if ( layer->transactionState == TRANSACTION_LOST )
		return RAL_WEBGPU_ASYNC_DEVICE_LOST;
	if ( layer->transactionState != TRANSACTION_PENDING )
		return RAL_WEBGPU_ASYNC_FAILED;
	if ( !LayerReady( layer ) ) {
		DestroyTransactionResources( layer );
		layer->transactionState = TRANSACTION_LOST;
		return RAL_WEBGPU_ASYNC_DEVICE_LOST;
	}
	tx = &layer->transaction;
	if ( !layer->host.pollRoundTrip( layer->userData,
			tx->operationIdentity, tx->operationGeneration, tx->readback,
			tx->byteCount, &readCount, &hostStatus ) ) hostStatus = RAL_WEBGPU_ASYNC_FAILED;
	if ( hostStatus == RAL_WEBGPU_ASYNC_PENDING )
		return RAL_WEBGPU_ASYNC_PENDING;
	if ( hostStatus != RAL_WEBGPU_ASYNC_READY || !outReceipt
			|| readCount != tx->byteCount
			|| memcmp( tx->expected, tx->readback, (size_t)tx->byteCount )
			|| !Ral_TransferComplete( &tx->submittedTransfer,
				tx->submission.generation, qtrue, &completed ) ) {
		DestroyTransactionResources( layer );
		layer->transactionState = hostStatus == RAL_WEBGPU_ASYNC_DEVICE_LOST
			? TRANSACTION_LOST : TRANSACTION_FAILED;
		return hostStatus == RAL_WEBGPU_ASYNC_DEVICE_LOST
			? RAL_WEBGPU_ASYNC_DEVICE_LOST : RAL_WEBGPU_ASYNC_FAILED;
	}
	digest = DigestBytes( tx->readback, tx->byteCount );
	memset( &facts, 0, sizeof( facts ) );
	facts.backendType = RAL_BACKEND_WEBGPU;
	facts.backendGeneration = layer->coreReceipt.generation;
	facts.backendIdentity = layer->coreReceipt.backendIdentity;
	facts.deviceIdentity = layer->coreReceipt.deviceIdentity;
	facts.graphicsQueueIdentity = layer->coreReceipt.queueIdentity;
	facts.capabilities = &layer->coreReceipt.capabilityProfile;
	facts.uploadAllocation = &tx->uploadReceipt.allocation;
	facts.readbackAllocation = &tx->readbackReceipt.allocation;
	facts.textureAllocation = &tx->textureReceipt.allocation;
	facts.samplerIdentity = tx->samplerReceipt.resourceIdentity;
	facts.samplerGeneration = tx->samplerReceipt.resourceGeneration;
	facts.submission = &tx->submission;
	facts.transfer = &completed;
	facts.completionGeneration = tx->submission.generation;
	facts.copiedByteCount = tx->byteCount;
	facts.copiedByteDigest = digest;
	if ( !Ral_BackendConformanceBuild( &facts, &receipt )
			|| !Ral_BackendConformanceReceiptExact( &receipt, &receipt ) ) {
		DestroyTransactionResources( layer );
		layer->transactionState = TRANSACTION_FAILED;
		return RAL_WEBGPU_ASYNC_FAILED;
	}
	DestroyTransactionResources( layer );
	layer->transactionState = TRANSACTION_IDLE;
	*outReceipt = receipt;
	return RAL_WEBGPU_ASYNC_READY;
}
