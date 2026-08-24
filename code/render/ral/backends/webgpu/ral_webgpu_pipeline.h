// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_PIPELINE_H
#define WIRED_RAL_WEBGPU_PIPELINE_H

#include "ral_pipeline.h"
#include "ral_webgpu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_PIPELINE_SCHEMA_VERSION 1u

typedef struct ralWebGpuPipeline_s ralWebGpuPipeline_t;

typedef enum {
	RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE = 1,
	RAL_WEBGPU_PIPELINE_OBJECT_BIND_GROUP_LAYOUT,
	RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE_LAYOUT,
	RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE
} ralWebGpuPipelineObjectKind_t;

typedef struct {
	const char *code;
	uint32_t byteCount;
} ralWebGpuWgslModule_t;

typedef struct {
	uint32_t binding;
	ralShaderBindingClass_t bindingClass;
	uint32_t arrayCount;
	uint32_t stageFlags;
	uint64_t minBufferBindingSize;
	ralShaderTextureViewDimension_t viewDimension;
	ralShaderTextureSampleType_t sampleType;
	ralFormat_t storageTextureFormat;
	qboolean dynamicOffset;
} ralWebGpuBindGroupLayoutEntry_t;

typedef struct {
	uint32_t stage;
	const char *entryPoint;
	const char *code;
	uint32_t byteCount;
	ralShaderDigest_t digest;
} ralWebGpuShaderModuleDesc_t;

typedef struct {
	ralShaderPipelineKind_t kind;
	uintptr_t shaderModules[RAL_SHADER_ABI_MAX_MODULES];
	uint32_t shaderModuleCount;
	uintptr_t bindGroupLayouts[RAL_SHADER_ABI_MAX_BIND_GROUPS];
	uint32_t bindGroupLayoutCount;
	uintptr_t pipelineLayout;
	const ralGraphicsPipelineCreateInfo_t *graphicsState;
	const ralComputePipelineCreateInfo_t *computeState;
	const ralShaderSpecValue_t *specValues;
	uint32_t specValueCount;
} ralWebGpuNativePipelineDesc_t;

typedef qboolean ( *ralWebGpuCreateShaderModuleFn )( void *userData,
	uintptr_t deviceIdentity, const ralWebGpuShaderModuleDesc_t *desc,
	uintptr_t *outIdentity );
typedef qboolean ( *ralWebGpuCreateBindGroupLayoutFn )( void *userData,
	uintptr_t deviceIdentity, uint32_t group,
	const ralWebGpuBindGroupLayoutEntry_t *entries, uint32_t entryCount,
	uintptr_t *outIdentity );
typedef qboolean ( *ralWebGpuCreatePipelineLayoutFn )( void *userData,
	uintptr_t deviceIdentity, const uintptr_t *groupLayouts,
	uint32_t groupLayoutCount, uintptr_t *outIdentity );
typedef qboolean ( *ralWebGpuCreatePipelineFn )( void *userData,
	uintptr_t deviceIdentity, const ralWebGpuNativePipelineDesc_t *desc,
	uintptr_t *outIdentity );
typedef void ( *ralWebGpuDestroyPipelineObjectFn )( void *userData,
	ralWebGpuPipelineObjectKind_t kind, uintptr_t identity );

typedef struct {
	ralWebGpuCreateShaderModuleFn createShaderModule;
	ralWebGpuCreateBindGroupLayoutFn createBindGroupLayout;
	ralWebGpuCreatePipelineLayoutFn createPipelineLayout;
	ralWebGpuCreatePipelineFn createPipeline;
	ralWebGpuDestroyPipelineObjectFn destroyObject;
} ralWebGpuPipelineHostOps_t;

typedef struct {
	void *userData;
	ralWebGpuPipelineHostOps_t host;
	const ralShaderAbiManifest_t *manifest;
	const ralShaderVariantAbi_t *variant;
	const ralWebGpuWgslModule_t *modules;
	uint32_t moduleCount;
	const ralGraphicsPipelineCreateInfo_t *graphicsState;
	const ralComputePipelineCreateInfo_t *computeState;
	uint64_t generation;
} ralWebGpuPipelineCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	ralShaderPipelineKind_t kind;
	uint64_t backendGeneration;
	uint64_t generation;
	uintptr_t pipelineIdentity;
	uintptr_t pipelineLayoutIdentity;
	uintptr_t shaderModuleIdentities[RAL_SHADER_ABI_MAX_MODULES];
	ralShaderDigest_t shaderModuleDigests[RAL_SHADER_ABI_MAX_MODULES];
	uint32_t shaderModuleCount;
	uintptr_t bindGroupLayoutIdentities[RAL_SHADER_ABI_MAX_BIND_GROUPS];
	uint32_t bindGroupLayoutCount;
	uint32_t bindingCount;
	ralShaderPipelineKey_t pipelineKey;
	qboolean ready;
} ralWebGpuPipelineReceipt_t;

qboolean RalWebGpu_PipelineCreate( ralWebGpuCore_t *core,
	const ralWebGpuCoreReceipt_t *coreReceipt,
	const ralWebGpuPipelineCreateInfo_t *createInfo,
	ralWebGpuPipeline_t **outPipeline,
	ralWebGpuPipelineReceipt_t *outReceipt );
qboolean RalWebGpu_PipelineDestroy( ralWebGpuCore_t *core,
	ralWebGpuPipeline_t *pipeline,
	const ralWebGpuPipelineReceipt_t *authority );
qboolean RalWebGpu_PipelineReceiptExact(
	const ralWebGpuPipelineReceipt_t *a,
	const ralWebGpuPipelineReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
