// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_PRESENTATION_H
#define WIRED_RAL_WEBGPU_PRESENTATION_H

#include "ral_webgpu_command.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_PRESENTATION_SCHEMA_VERSION 1u
#define RAL_WEBGPU_DPR_ONE_Q16 65536u

typedef struct ralWebGpuPresentation_s ralWebGpuPresentation_t;

typedef qboolean ( *ralWebGpuCanvasConfigureFn )( void *userData,
	uintptr_t canvasIdentity, uintptr_t deviceIdentity, uint32_t pixelWidth,
	uint32_t pixelHeight, ralFormat_t format, ralColorSpace_t colorSpace,
	qboolean opaqueAlpha );
typedef void ( *ralWebGpuCanvasUnconfigureFn )( void *userData,
	uintptr_t canvasIdentity );
typedef qboolean ( *ralWebGpuCanvasAcquireFn )( void *userData,
	uintptr_t canvasIdentity, uint64_t frameGeneration,
	uintptr_t *outTextureIdentity );
typedef qboolean ( *ralWebGpuCanvasPresentFn )( void *userData,
	uintptr_t canvasIdentity, uintptr_t textureIdentity,
	uint64_t frameGeneration, uint64_t submissionGeneration );

typedef struct {
	ralWebGpuCanvasConfigureFn configure;
	ralWebGpuCanvasUnconfigureFn unconfigure;
	ralWebGpuCanvasAcquireFn acquire;
	ralWebGpuCanvasPresentFn present;
} ralWebGpuPresentationHostOps_t;

typedef struct {
	void *userData;
	uintptr_t canvasIdentity;
	ralWebGpuPresentationHostOps_t host;
} ralWebGpuPresentationCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uint64_t presentationGeneration;
	uintptr_t canvasIdentity;
	uint32_t cssWidth;
	uint32_t cssHeight;
	uint32_t dprQ16;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	ralFormat_t format;
	ralColorSpace_t colorSpace;
	qboolean opaqueAlpha;
	qboolean configured;
	qboolean suspended;
	qboolean ready;
} ralWebGpuPresentationReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uint64_t presentationGeneration;
	uint64_t frameGeneration;
	uintptr_t canvasIdentity;
	uintptr_t textureIdentity;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	qboolean ready;
} ralWebGpuFrameReceipt_t;

qboolean RalWebGpu_PresentationCreate( ralWebGpuCore_t *core,
	const ralWebGpuCoreReceipt_t *coreReceipt,
	const ralWebGpuPresentationCreateInfo_t *createInfo,
	ralWebGpuPresentation_t **outPresentation );
void RalWebGpu_PresentationDestroy( ralWebGpuPresentation_t *presentation );
qboolean RalWebGpu_PresentationConfigure(
	ralWebGpuPresentation_t *presentation, uint32_t cssWidth,
	uint32_t cssHeight, uint32_t dprQ16, ralFormat_t format,
	ralColorSpace_t colorSpace, qboolean opaqueAlpha,
	ralWebGpuPresentationReceipt_t *outReceipt );
qboolean RalWebGpu_PresentationAcquire(
	ralWebGpuPresentation_t *presentation,
	const ralWebGpuPresentationReceipt_t *authority,
	ralWebGpuFrameReceipt_t *outFrame );
qboolean RalWebGpu_PresentationPresent(
	ralWebGpuPresentation_t *presentation,
	const ralWebGpuFrameReceipt_t *frame,
	const ralWebGpuSubmissionReceipt_t *submission );
qboolean RalWebGpu_PresentationReceiptExact(
	const ralWebGpuPresentationReceipt_t *a,
	const ralWebGpuPresentationReceipt_t *b );
qboolean RalWebGpu_FrameReceiptExact( const ralWebGpuFrameReceipt_t *a,
	const ralWebGpuFrameReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
