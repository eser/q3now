// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_PIPELINE_H
#define WIRED_RAL_METAL_PIPELINE_H

#include "ral_metal_bind_group.h"
#include "ral_shader_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_PIPELINE_SCHEMA_VERSION 1u

typedef struct {
	const char *vertexSourcePath;
	const char *fragmentSourcePath;
	const char *vertexLibraryPath;
	const char *fragmentLibraryPath;
	ralShaderDigest_t expectedVertexSourceDigest;
	ralShaderDigest_t expectedFragmentSourceDigest;
	ralMetalBindLayout_t *fragmentLayout;
	const ralMetalBindLayoutReceipt_t *fragmentLayoutReceipt;
	ralFormat_t colorFormat;
	ralFormat_t depthFormat;
	uint32_t sampleCount;
	uint64_t generation;
} ralMetalPipelineCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t pipelineGeneration;
	uintptr_t pipelineIdentity;
	ralShaderDigest_t vertexSourceDigest;
	ralShaderDigest_t fragmentSourceDigest;
	ralShaderDigest_t vertexLibraryDigest;
	ralShaderDigest_t fragmentLibraryDigest;
	uint32_t vertexSourceBytes;
	uint32_t fragmentSourceBytes;
	uint32_t vertexLibraryBytes;
	uint32_t fragmentLibraryBytes;
	uintptr_t fragmentLayoutIdentity;
	uint64_t fragmentLayoutGeneration;
	ralFormat_t colorFormat;
	ralFormat_t depthFormat;
	uint32_t sampleCount;
	uint32_t vertexStride;
	ralShaderVertexInputAbi_t vertexInputs[3];
	uint32_t vertexInputOffsets[3];
	qboolean blendEnabled;
	qboolean ready;
} ralMetalPipelineReceipt_t;

typedef struct ralMetalPipeline_s ralMetalPipeline_t;

// The fragment layout is borrowed and must remain alive/byte-immutable until
// the pipeline is destroyed.  The owner retains its native libraries, pipeline
// state and depth state; receipts contain no Objective-C/Metal types.

qboolean RalMetal_PipelineCreate( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPipelineCreateInfo_t *createInfo,
	ralMetalPipeline_t **outPipeline, ralMetalPipelineReceipt_t *outReceipt );
void RalMetal_PipelineDestroy( ralMetalPipeline_t *pipeline );
qboolean RalMetal_PipelineReceiptExact( const ralMetalPipelineReceipt_t *a,
	const ralMetalPipelineReceipt_t *b );
qboolean RalMetal_PipelineMatchesReceipt( const ralMetalPipeline_t *pipeline,
	const ralMetalPipelineReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
