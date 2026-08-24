// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_DRAW_H
#define WIRED_RAL_METAL_DRAW_H

#include "ral_metal_pipeline.h"
#include "ral_metal_render.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_DRAW_SCHEMA_VERSION 1u

typedef struct {
	void *nativeBuffer;
	uintptr_t bufferIdentity;
	uint64_t bufferGeneration;
	uint64_t byteOffset;
	uint64_t byteSize;
} ralMetalDrawBuffer_t;

typedef struct {
	ralMetalPipeline_t *pipeline;
	const ralMetalPipelineReceipt_t *pipelineReceipt;
	ralMetalBindGroup_t *fragmentGroup;
	const ralMetalBindGroupReceipt_t *fragmentGroupReceipt;
	ralMetalDrawBuffer_t vertexBuffer;
	ralMetalDrawBuffer_t indexBuffer;
	const ralMetalRenderPlan_t *renderPlan;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t instanceCount;
	uint32_t firstInstance;
	uint64_t drawGeneration;
} ralMetalIndexedDrawInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t drawGeneration;
	uintptr_t pipelineIdentity;
	uint64_t pipelineGeneration;
	uintptr_t layoutIdentity;
	uint64_t layoutGeneration;
	uintptr_t groupIdentity;
	uint64_t groupGeneration;
	uintptr_t argumentBufferIdentity;
	uintptr_t sampledTextureIdentity;
	uint64_t sampledTextureGeneration;
	uintptr_t samplerIdentity;
	uint64_t samplerGeneration;
	uintptr_t vertexBufferIdentity;
	uint64_t vertexBufferGeneration;
	uint64_t vertexBufferBytes;
	uintptr_t indexBufferIdentity;
	uint64_t indexBufferGeneration;
	uint64_t indexBufferBytes;
	uintptr_t colorTextureIdentity;
	uint64_t colorTextureGeneration;
	uintptr_t depthTextureIdentity;
	uint64_t depthTextureGeneration;
	uint32_t width;
	uint32_t height;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t instanceCount;
	uint32_t firstInstance;
	ralCommandReceipt_t recording;
	ralCommandReceipt_t executable;
	ralSubmissionReceipt_t submission;
	uint64_t completionGeneration;
	uint64_t pixelCount;
	uint64_t pixelDigest;
	uint8_t expectedRgba[4];
	qboolean ready;
} ralMetalIndexedDrawReceipt_t;

// Bounded conformance draw for the canonical portable overlay ABI.  Every
// object is borrowed and must remain alive/byte-immutable through completion.
// The function emits exactly one indexed draw into an offscreen RGBA8+D32 plan;
// CAMetalLayer and product presentation are deliberately outside this surface.
qboolean RalMetal_DrawIndexedOverlay( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalIndexedDrawInfo_t *drawInfo,
	ralMetalIndexedDrawReceipt_t *outReceipt );
qboolean RalMetal_IndexedDrawReceiptExact( const ralMetalIndexedDrawReceipt_t *a,
	const ralMetalIndexedDrawReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
