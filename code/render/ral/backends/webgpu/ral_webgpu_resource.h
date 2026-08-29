// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_RESOURCE_H
#define WIRED_RAL_WEBGPU_RESOURCE_H

#include "ral_backend_conformance.h"
#include "ral_webgpu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_RESOURCE_SCHEMA_VERSION 1u
#define RAL_WEBGPU_MAX_CONFORMANCE_BYTES UINT64_C(1048576)
#define RAL_WEBGPU_MAX_RESOURCE_BYTES UINT64_C(134217728)

typedef struct ralWebGpuResourceLayer_s ralWebGpuResourceLayer_t;
typedef struct ralWebGpuResource_s ralWebGpuResource_t;

typedef enum {
	RAL_WEBGPU_RESOURCE_BUFFER = 1,
	RAL_WEBGPU_RESOURCE_TEXTURE,
	RAL_WEBGPU_RESOURCE_SAMPLER
} ralWebGpuResourceKind_t;

typedef enum {
	RAL_WEBGPU_BUFFER_COPY_SOURCE = 1u << 0,
	RAL_WEBGPU_BUFFER_COPY_DESTINATION = 1u << 1,
	RAL_WEBGPU_BUFFER_VERTEX = 1u << 2,
	RAL_WEBGPU_BUFFER_INDEX = 1u << 3,
	RAL_WEBGPU_BUFFER_UNIFORM = 1u << 4,
	RAL_WEBGPU_BUFFER_STORAGE = 1u << 5,
	RAL_WEBGPU_BUFFER_MAP_READ = 1u << 6
} ralWebGpuBufferUsage_t;

typedef struct {
	uint64_t size;
	uint32_t usage;
	ralAllocationClass_t memoryClass;
} ralWebGpuBufferDesc_t;

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	ralFormat_t format;
	uint32_t bytesPerTexel;
} ralWebGpuTextureDesc_t;

typedef struct {
	qboolean linearMinification;
	qboolean linearMagnification;
	qboolean clampToEdge;
} ralWebGpuSamplerDesc_t;

typedef struct {
	uint32_t schemaVersion;
	ralWebGpuResourceKind_t kind;
	uint64_t backendGeneration;
	uint64_t resourceGeneration;
	uintptr_t resourceIdentity;
	uint64_t byteSize;
	ralAllocationReceipt_t allocation;
	qboolean ready;
} ralWebGpuResourceReceipt_t;

typedef enum {
	RAL_WEBGPU_WRITE_BUFFER = 1,
	RAL_WEBGPU_WRITE_TEXTURE
} ralWebGpuWriteKind_t;

typedef struct {
	uint32_t schemaVersion;
	ralWebGpuWriteKind_t kind;
	uint64_t backendGeneration;
	uint64_t resourceGeneration;
	uint64_t writeGeneration;
	uint64_t byteOffset;
	uint64_t byteSize;
	uint64_t contentDigest;
	qboolean ready;
} ralWebGpuWriteReceipt_t;

typedef qboolean ( *ralWebGpuCreateBufferFn )( void *userData,
	uintptr_t deviceIdentity, const ralWebGpuBufferDesc_t *desc,
	uintptr_t *outIdentity );
typedef qboolean ( *ralWebGpuCreateTextureFn )( void *userData,
	uintptr_t deviceIdentity, const ralWebGpuTextureDesc_t *desc,
	uintptr_t *outIdentity );
typedef qboolean ( *ralWebGpuCreateSamplerFn )( void *userData,
	uintptr_t deviceIdentity, const ralWebGpuSamplerDesc_t *desc,
	uintptr_t *outIdentity );
typedef void ( *ralWebGpuDestroyResourceFn )( void *userData,
	ralWebGpuResourceKind_t kind, uintptr_t identity );
typedef qboolean ( *ralWebGpuWriteBufferFn )( void *userData,
	uintptr_t queueIdentity, uintptr_t bufferIdentity, uint64_t byteOffset,
	const void *bytes, uint64_t byteSize );
typedef qboolean ( *ralWebGpuWriteTextureFn )( void *userData,
	uintptr_t queueIdentity, uintptr_t textureIdentity, const void *bytes,
	uint64_t byteSize, uint32_t bytesPerRow, uint32_t rowsPerImage );
typedef qboolean ( *ralWebGpuBeginRoundTripFn )( void *userData,
	uintptr_t deviceIdentity, uintptr_t queueIdentity,
	uintptr_t uploadBufferIdentity, uintptr_t readbackBufferIdentity,
	const void *bytes, uint64_t byteSize, uint64_t operationGeneration,
	uintptr_t *outOperationIdentity );
typedef qboolean ( *ralWebGpuPollRoundTripFn )( void *userData,
	uintptr_t operationIdentity, uint64_t operationGeneration,
	void *outBytes, uint64_t capacity, uint64_t *outByteSize,
	ralWebGpuAsyncStatus_t *outStatus );
typedef void ( *ralWebGpuReleaseOperationFn )( void *userData,
	uintptr_t operationIdentity );

typedef struct {
	ralWebGpuCreateBufferFn createBuffer;
	ralWebGpuCreateTextureFn createTexture;
	ralWebGpuCreateSamplerFn createSampler;
	ralWebGpuDestroyResourceFn destroyResource;
	ralWebGpuWriteBufferFn writeBuffer;
	ralWebGpuWriteTextureFn writeTexture;
	ralWebGpuBeginRoundTripFn beginRoundTrip;
	ralWebGpuPollRoundTripFn pollRoundTrip;
	ralWebGpuReleaseOperationFn releaseOperation;
} ralWebGpuResourceHostOps_t;

typedef struct {
	void *userData;
	ralWebGpuResourceHostOps_t host;
} ralWebGpuResourceLayerCreateInfo_t;

qboolean RalWebGpu_ResourcesCreate( ralWebGpuCore_t *core,
	const ralWebGpuCoreReceipt_t *coreReceipt,
	const ralWebGpuResourceLayerCreateInfo_t *createInfo,
	ralWebGpuResourceLayer_t **outLayer );
void RalWebGpu_ResourcesDestroy( ralWebGpuResourceLayer_t *layer );
qboolean RalWebGpu_CreateBuffer( ralWebGpuResourceLayer_t *layer,
	const ralWebGpuBufferDesc_t *desc, ralWebGpuResource_t **outResource,
	ralWebGpuResourceReceipt_t *outReceipt );
qboolean RalWebGpu_CreateTexture( ralWebGpuResourceLayer_t *layer,
	const ralWebGpuTextureDesc_t *desc, ralWebGpuResource_t **outResource,
	ralWebGpuResourceReceipt_t *outReceipt );
qboolean RalWebGpu_CreateSampler( ralWebGpuResourceLayer_t *layer,
	const ralWebGpuSamplerDesc_t *desc, ralWebGpuResource_t **outResource,
	ralWebGpuResourceReceipt_t *outReceipt );
qboolean RalWebGpu_DestroyResource( ralWebGpuResourceLayer_t *layer,
	ralWebGpuResource_t *resource, const ralWebGpuResourceReceipt_t *authority );
qboolean RalWebGpu_ResourceReceiptExact( const ralWebGpuResourceReceipt_t *a,
	const ralWebGpuResourceReceipt_t *b );
qboolean RalWebGpu_WriteBuffer( ralWebGpuResourceLayer_t *layer,
	ralWebGpuResource_t *resource,
	const ralWebGpuResourceReceipt_t *authority, uint64_t byteOffset,
	const void *bytes, uint64_t byteSize, ralWebGpuWriteReceipt_t *outReceipt );
qboolean RalWebGpu_WriteTexture( ralWebGpuResourceLayer_t *layer,
	ralWebGpuResource_t *resource,
	const ralWebGpuResourceReceipt_t *authority, const void *bytes,
	uint64_t byteSize, uint32_t bytesPerRow, uint32_t rowsPerImage,
	ralWebGpuWriteReceipt_t *outReceipt );
qboolean RalWebGpu_WriteReceiptExact( const ralWebGpuWriteReceipt_t *a,
	const ralWebGpuWriteReceipt_t *b );

qboolean RalWebGpu_OffscreenConformanceBegin(
	ralWebGpuResourceLayer_t *layer, uint64_t byteCount );
ralWebGpuAsyncStatus_t RalWebGpu_OffscreenConformancePoll(
	ralWebGpuResourceLayer_t *layer,
	ralBackendConformanceReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
