// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Versioned native-free shader interface and artifact provenance schema.

#ifndef WIRED_RAL_SHADER_ABI_H
#define WIRED_RAL_SHADER_ABI_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_SHADER_ABI_SCHEMA_VERSION 1u
#define RAL_SHADER_ABI_MAX_MODULES 2u
#define RAL_SHADER_ABI_MAX_BINDINGS 64u
#define RAL_SHADER_ABI_MAX_BIND_GROUPS 8u
#define RAL_SHADER_ABI_MAX_BINDINGS_PER_GROUP 64u
#define RAL_SHADER_ABI_MAX_BINDING_ARRAY_COUNT 4096u
#define RAL_SHADER_ABI_MAX_VERTEX_INPUTS 16u
#define RAL_SHADER_ABI_MAX_SPEC_CONSTANTS 32u

typedef enum {
	RAL_SHADER_PIPELINE_GRAPHICS = 1,
	RAL_SHADER_PIPELINE_COMPUTE = 2
} ralShaderPipelineKind_t;

typedef enum {
	RAL_SHADER_ARTIFACT_SPIRV = 0,
	RAL_SHADER_ARTIFACT_MSL = 1,
	RAL_SHADER_ARTIFACT_WGSL = 2,
	RAL_SHADER_ARTIFACT_COUNT = 3
} ralShaderArtifactTarget_t;

typedef struct {
	uint64_t lane0;
	uint64_t lane1;
} ralShaderDigest_t;

typedef struct {
	ralShaderArtifactTarget_t target;
	uint32_t byteCount;
	ralShaderDigest_t digest;
} ralShaderArtifactAbi_t;

typedef struct {
	uint32_t stage; // exactly one RAL_STAGE_* bit
	const char *entryPoint;
	ralShaderDigest_t sourceDigest;
	ralShaderArtifactAbi_t artifacts[ RAL_SHADER_ARTIFACT_COUNT ];
} ralShaderModuleAbi_t;

// Deliberately matches the portable WebGPU/Vulkan intersection. Combined
// image-samplers and unbounded arrays require explicit lowering in later
// manifests rather than being smuggled through this schema.
typedef enum {
	RAL_SHADER_BIND_UNIFORM_BUFFER = 1,
	RAL_SHADER_BIND_STORAGE_BUFFER_READ,
	RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE,
	RAL_SHADER_BIND_SAMPLED_TEXTURE,
	RAL_SHADER_BIND_STORAGE_TEXTURE_READ,
	RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE,
	RAL_SHADER_BIND_FILTERING_SAMPLER,
	RAL_SHADER_BIND_COMPARISON_SAMPLER
} ralShaderBindingClass_t;

typedef enum {
	RAL_SHADER_VIEW_2D = 1,
	RAL_SHADER_VIEW_2D_ARRAY,
	RAL_SHADER_VIEW_CUBE,
	RAL_SHADER_VIEW_CUBE_ARRAY,
	RAL_SHADER_VIEW_3D
} ralShaderTextureViewDimension_t;

typedef enum {
	RAL_SHADER_SAMPLE_FLOAT = 1,
	RAL_SHADER_SAMPLE_UNFILTERABLE_FLOAT,
	RAL_SHADER_SAMPLE_DEPTH,
	RAL_SHADER_SAMPLE_SINT,
	RAL_SHADER_SAMPLE_UINT
} ralShaderTextureSampleType_t;

typedef struct {
	uint32_t set;
	uint32_t binding;
	ralShaderBindingClass_t bindingClass;
	uint32_t arrayCount; // bounded; zero is never silently treated as unbounded
	uint32_t stageFlags;
	uint64_t minBufferBindingSize;
	ralShaderTextureViewDimension_t viewDimension;
	ralShaderTextureSampleType_t sampleType;
	ralFormat_t storageTextureFormat;
	qboolean dynamicOffset;
} ralShaderBindingAbi_t;

typedef struct {
	uint32_t location;
	ralFormat_t format;
} ralShaderVertexInputAbi_t;

// Vulkan consumes this as push constants. WebGPU consumes the same bytes from
// the declared uniform-buffer binding. A manifest with inline bytes must carry
// an exact matching uniform binding so the fallback cannot be invented later.
typedef struct {
	uint32_t byteSize;
	uint32_t stageFlags;
	uint32_t webgpuUniformSet;
	uint32_t webgpuUniformBinding;
} ralShaderInlineDataAbi_t;

typedef struct {
	uint32_t constantId;
	uint32_t stageFlags;
	uint32_t defaultValue;
} ralShaderSpecConstantAbi_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	ralShaderPipelineKind_t pipelineKind;
	const ralShaderModuleAbi_t *modules;
	uint32_t moduleCount;
	const ralShaderBindingAbi_t *bindings;
	uint32_t bindingCount;
	const ralShaderVertexInputAbi_t *vertexInputs;
	uint32_t vertexInputCount;
	ralShaderInlineDataAbi_t inlineData;
	const ralShaderSpecConstantAbi_t *specConstants;
	uint32_t specConstantCount;
} ralShaderAbiManifest_t;

typedef struct {
	uint32_t constantId;
	uint32_t value;
} ralShaderSpecValue_t;

typedef struct {
	ralShaderArtifactTarget_t artifactTarget;
	uint64_t generation;
	ralShaderDigest_t semanticStateDigest;
	const ralShaderSpecValue_t *specValues;
	uint32_t specValueCount;
} ralShaderVariantAbi_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t manifestGeneration;
	uint64_t variantGeneration;
	ralShaderArtifactTarget_t artifactTarget;
	ralShaderDigest_t digest;
	qboolean ready;
} ralShaderPipelineKey_t;

qboolean Ral_ShaderDigestValid( const ralShaderDigest_t *digest );
qboolean Ral_ShaderDigestExact( const ralShaderDigest_t *a,
	                           const ralShaderDigest_t *b );
ralResult_t Ral_ShaderArtifactDigest( const void *bytes, uint32_t byteCount,
	                                 ralShaderDigest_t *outDigest );
qboolean Ral_ShaderAbiManifestValid( const ralShaderAbiManifest_t *manifest );
qboolean Ral_ShaderVariantAbiValid( const ralShaderAbiManifest_t *manifest,
	                                const ralShaderVariantAbi_t *variant );
qboolean Ral_ShaderPipelineKeyExact( const ralShaderPipelineKey_t *a,
	                                const ralShaderPipelineKey_t *b );
ralResult_t Ral_ShaderAbiBuildPipelineKey( const ralShaderAbiManifest_t *manifest,
	                                      const ralShaderVariantAbi_t *variant,
	                                      ralShaderPipelineKey_t *outKey );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_SHADER_ABI_H
