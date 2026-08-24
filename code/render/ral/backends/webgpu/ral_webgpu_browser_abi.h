// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_BROWSER_ABI_H
#define WIRED_RAL_WEBGPU_BROWSER_ABI_H

#include <stdint.h>

#define RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION 1u
#define RAL_WEBGPU_BROWSER_ABI_NAME_BYTES 128u
#define RAL_WEBGPU_BROWSER_ABI_REASON_BYTES 192u

typedef enum {
	RAL_WEBGPU_BROWSER_OP_BEGIN_ADAPTER = 1,
	RAL_WEBGPU_BROWSER_OP_POLL_ADAPTER,
	RAL_WEBGPU_BROWSER_OP_BEGIN_DEVICE,
	RAL_WEBGPU_BROWSER_OP_POLL_DEVICE,
	RAL_WEBGPU_BROWSER_OP_RELEASE_DEVICE,
	RAL_WEBGPU_BROWSER_OP_RELEASE_ADAPTER,
	RAL_WEBGPU_BROWSER_OP_POLL_DEVICE_LOSS,
	RAL_WEBGPU_BROWSER_OP_CANVAS_CONFIGURE,
	RAL_WEBGPU_BROWSER_OP_CANVAS_UNCONFIGURE,
	RAL_WEBGPU_BROWSER_OP_CANVAS_ACQUIRE,
	RAL_WEBGPU_BROWSER_OP_CANVAS_PRESENT,
	RAL_WEBGPU_BROWSER_OP_CREATE_BUFFER,
	RAL_WEBGPU_BROWSER_OP_CREATE_TEXTURE,
	RAL_WEBGPU_BROWSER_OP_CREATE_SAMPLER,
	RAL_WEBGPU_BROWSER_OP_DESTROY_RESOURCE,
	RAL_WEBGPU_BROWSER_OP_WRITE_BUFFER,
	RAL_WEBGPU_BROWSER_OP_WRITE_TEXTURE,
	RAL_WEBGPU_BROWSER_OP_BEGIN_ROUND_TRIP,
	RAL_WEBGPU_BROWSER_OP_POLL_ROUND_TRIP,
	RAL_WEBGPU_BROWSER_OP_RELEASE_OPERATION,
	RAL_WEBGPU_BROWSER_OP_CREATE_SHADER_MODULE,
	RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP_LAYOUT,
	RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE_LAYOUT,
	RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE,
	RAL_WEBGPU_BROWSER_OP_DESTROY_PIPELINE_OBJECT,
	RAL_WEBGPU_BROWSER_OP_BEGIN_ENCODER,
	RAL_WEBGPU_BROWSER_OP_BEGIN_PASS,
	RAL_WEBGPU_BROWSER_OP_RECORD_INDEXED_DRAW,
	RAL_WEBGPU_BROWSER_OP_END_PASS,
	RAL_WEBGPU_BROWSER_OP_FINISH_ENCODER,
	RAL_WEBGPU_BROWSER_OP_SUBMIT,
	RAL_WEBGPU_BROWSER_OP_POLL_SUBMISSION,
	RAL_WEBGPU_BROWSER_OP_RELEASE_COMMAND_OBJECT
} ralWebGpuBrowserOpcode_t;

#pragma pack(push, 1)

typedef struct {
	uint32_t schemaVersion;
	uint32_t opcode;
	uint32_t byteCount;
	uint32_t reserved;
} ralWebGpuBrowserAbiHeader_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t requestGeneration;
} ralWebGpuBrowserGenerationRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t accepted;
	uint32_t reserved;
} ralWebGpuBrowserStatusResponse_t;

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
	uint32_t bindingArrays;
	uint32_t textureCompressionBC;
	uint32_t textureCompressionASTC;
	uint32_t textureCompressionETC2;
	uint32_t timestampQueries;
	uint32_t reserved;
} ralWebGpuBrowserLimitsAbi_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t status;
	uint32_t adapterType;
	uint64_t adapterIdentity;
	uint32_t vendorId;
	uint32_t deviceId;
	ralWebGpuBrowserLimitsAbi_t limits;
	char vendorName[RAL_WEBGPU_BROWSER_ABI_NAME_BYTES];
	char deviceName[RAL_WEBGPU_BROWSER_ABI_NAME_BYTES];
} ralWebGpuBrowserAdapterResponse_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t adapterIdentity;
	uint64_t requestGeneration;
} ralWebGpuBrowserBeginDeviceRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t status;
	uint32_t reserved;
	uint64_t deviceIdentity;
	uint64_t queueIdentity;
} ralWebGpuBrowserDeviceResponse_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t queueIdentity;
} ralWebGpuBrowserReleaseDeviceRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t adapterIdentity;
} ralWebGpuBrowserReleaseAdapterRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint32_t lost;
	uint32_t recoverable;
	char reason[RAL_WEBGPU_BROWSER_ABI_REASON_BYTES];
} ralWebGpuBrowserLossResponse_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t canvasIdentity;
	uint64_t deviceIdentity;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t format;
	uint32_t colorSpace;
	uint32_t opaqueAlpha;
	uint32_t reserved;
} ralWebGpuBrowserCanvasConfigureRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t canvasIdentity;
} ralWebGpuBrowserCanvasRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t canvasIdentity;
	uint64_t frameGeneration;
} ralWebGpuBrowserCanvasAcquireRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t accepted;
	uint32_t reserved;
	uint64_t textureIdentity;
} ralWebGpuBrowserCanvasAcquireResponse_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t canvasIdentity;
	uint64_t textureIdentity;
	uint64_t frameGeneration;
	uint64_t submissionGeneration;
} ralWebGpuBrowserCanvasPresentRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t accepted;
	uint32_t reserved;
	uint64_t identity;
} ralWebGpuBrowserIdentityResponse_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t byteSize;
	uint32_t usage;
	uint32_t memoryClass;
} ralWebGpuBrowserCreateBufferRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t bytesPerTexel;
} ralWebGpuBrowserCreateTextureRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint32_t linearMinification;
	uint32_t linearMagnification;
	uint32_t clampToEdge;
	uint32_t reserved;
} ralWebGpuBrowserCreateSamplerRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t kind;
	uint32_t reserved;
	uint64_t identity;
} ralWebGpuBrowserDestroyObjectRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t queueIdentity;
	uint64_t resourceIdentity;
	uint64_t byteOffset;
	uint64_t dataOffset;
	uint64_t byteSize;
} ralWebGpuBrowserWriteBufferRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t queueIdentity;
	uint64_t resourceIdentity;
	uint64_t dataOffset;
	uint64_t byteSize;
	uint32_t bytesPerRow;
	uint32_t rowsPerImage;
} ralWebGpuBrowserWriteTextureRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t queueIdentity;
	uint64_t uploadBufferIdentity;
	uint64_t readbackBufferIdentity;
	uint64_t dataOffset;
	uint64_t byteSize;
	uint64_t operationGeneration;
} ralWebGpuBrowserRoundTripBeginRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t operationIdentity;
	uint64_t operationGeneration;
	uint64_t outputOffset;
	uint64_t capacity;
} ralWebGpuBrowserRoundTripPollRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint32_t status;
	uint32_t reserved;
	uint64_t byteSize;
} ralWebGpuBrowserAsyncResponse_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t identity;
} ralWebGpuBrowserIdentityRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t codeOffset;
	uint64_t digestLane0;
	uint64_t digestLane1;
	uint32_t stage;
	uint32_t byteCount;
	char entryPoint[64];
} ralWebGpuBrowserShaderModuleRequest_t;

typedef struct {
	uint32_t binding;
	uint32_t bindingClass;
	uint32_t arrayCount;
	uint32_t stageFlags;
	uint64_t minBufferBindingSize;
	uint32_t viewDimension;
	uint32_t sampleType;
	uint32_t storageTextureFormat;
	uint32_t dynamicOffset;
} ralWebGpuBrowserBindEntryAbi_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t entriesOffset;
	uint32_t group;
	uint32_t entryCount;
} ralWebGpuBrowserBindGroupLayoutRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t layoutsOffset;
	uint32_t layoutCount;
	uint32_t reserved;
} ralWebGpuBrowserPipelineLayoutRequest_t;

typedef struct {
	uint32_t binding;
	uint32_t stride;
	uint32_t inputRate;
	uint32_t reserved;
} ralWebGpuBrowserVertexBindingAbi_t;

typedef struct {
	uint32_t location;
	uint32_t binding;
	uint32_t format;
	uint32_t offset;
} ralWebGpuBrowserVertexAttributeAbi_t;

typedef struct {
	uint32_t blendEnable;
	uint32_t srcColor;
	uint32_t dstColor;
	uint32_t colorOp;
	uint32_t srcAlpha;
	uint32_t dstAlpha;
	uint32_t alphaOp;
	uint32_t writeMask;
	uint32_t writeMaskExplicit;
} ralWebGpuBrowserBlendAbi_t;

typedef struct {
	uint32_t constantId;
	uint32_t value;
} ralWebGpuBrowserSpecValueAbi_t;

typedef struct {
	uint32_t failOp;
	uint32_t passOp;
	uint32_t depthFailOp;
	uint32_t compareOp;
	uint32_t compareMask;
	uint32_t writeMask;
	uint32_t reference;
} ralWebGpuBrowserStencilAbi_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
	uint64_t pipelineLayoutIdentity;
	uint64_t shaderModules[2];
	uint64_t bindGroupLayouts[8];
	uint32_t kind;
	uint32_t shaderModuleCount;
	uint32_t bindGroupLayoutCount;
	uint32_t topology;
	uint32_t polygonMode;
	uint32_t cullMode;
	uint32_t frontFace;
	uint32_t depthBiasEnable;
	float depthBiasConstant;
	float depthBiasSlope;
	float depthBiasClamp;
	uint32_t depthClampEnable;
	float lineWidth;
	uint32_t depthTestEnable;
	uint32_t depthWriteEnable;
	uint32_t depthCompareOp;
	uint32_t stencilTestEnable;
	ralWebGpuBrowserStencilAbi_t stencilFront;
	ralWebGpuBrowserStencilAbi_t stencilBack;
	uint32_t colorFormats[8];
	uint32_t numColorFormats;
	uint32_t depthFormat;
	uint32_t sampleCount;
	uint32_t vertexBindingCount;
	uint32_t vertexAttributeCount;
	uint32_t colorBlendCount;
	uint32_t specValueCount;
	ralWebGpuBrowserVertexBindingAbi_t vertexBindings[16];
	ralWebGpuBrowserVertexAttributeAbi_t vertexAttributes[16];
	ralWebGpuBrowserBlendAbi_t colorBlends[8];
	ralWebGpuBrowserSpecValueAbi_t specValues[32];
} ralWebGpuBrowserPipelineRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t deviceIdentity;
} ralWebGpuBrowserBeginEncoderRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t encoderIdentity;
	uint64_t targetIdentity;
	uint32_t passKind;
	uint32_t reserved;
} ralWebGpuBrowserBeginPassRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t passIdentity;
	uint64_t pipelineIdentity;
	uint64_t vertexBufferIdentity;
	uint64_t indexBufferIdentity;
	uint64_t textureIdentity;
	uint64_t secondaryTextureIdentity;
	uint64_t samplerIdentity;
	uint64_t contentDigest;
	uint32_t drawKind;
	uint32_t textured;
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t reserved;
} ralWebGpuBrowserIndexedDrawRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t queueIdentity;
	uint64_t commandBufferIdentity;
	uint64_t submissionGeneration;
} ralWebGpuBrowserSubmitRequest_t;

typedef struct {
	ralWebGpuBrowserAbiHeader_t header;
	uint64_t submissionIdentity;
	uint64_t submissionGeneration;
} ralWebGpuBrowserPollSubmissionRequest_t;

#pragma pack(pop)

#endif
