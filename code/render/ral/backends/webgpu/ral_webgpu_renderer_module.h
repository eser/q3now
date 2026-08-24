// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_RENDERER_MODULE_H
#define WIRED_RAL_WEBGPU_RENDERER_MODULE_H

#include "ral_webgpu_product.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION 1u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t moduleGeneration;
	uint64_t frameGeneration;
	renderSubmissionReceipt_t frontend;
	ralWebGpuProductFrameReceipt_t product;
	qboolean presented;
	qboolean ready;
} ralWebGpuRendererModuleFrameReceipt_t;

Q_EXPORT ralWebGpuAsyncStatus_t WiredWebGpu_RendererPoll( void );
Q_EXPORT qboolean WiredWebGpu_GetFrameReceipt(
	ralWebGpuRendererModuleFrameReceipt_t *outReceipt );
Q_EXPORT qboolean RalWebGpu_RendererModuleFrameReceiptExact(
	const ralWebGpuRendererModuleFrameReceipt_t *a,
	const ralWebGpuRendererModuleFrameReceipt_t *b );
Q_EXPORT int WiredWebGpu_RendererSmokeBegin( void );
Q_EXPORT int WiredWebGpu_RendererSmokePoll( void );
Q_EXPORT int WiredWebGpu_RendererResize( uint32_t width, uint32_t height );

#ifdef __cplusplus
}
#endif

#endif
