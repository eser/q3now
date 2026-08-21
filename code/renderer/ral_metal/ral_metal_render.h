// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_RENDER_H
#define WIRED_RAL_METAL_RENDER_H

#include "ral_command_lifecycle.h"
#include "ral_metal_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_RENDER_SCHEMA_VERSION 1u

typedef struct {
	void *nativeTexture;
	uintptr_t textureIdentity;
	uint64_t textureGeneration;
	ralFormat_t format;
	uint32_t width;
	uint32_t height;
	uint32_t sampleCount;
	ralLoadOp_t loadOp;
	ralStoreOp_t storeOp;
	ralClearValue_t clearValue;
} ralMetalRenderAttachment_t;

typedef struct {
	const ralMetalRenderAttachment_t *colorAttachments;
	uint32_t colorAttachmentCount;
	const ralMetalRenderAttachment_t *depthAttachment;
	uint32_t width;
	uint32_t height;
} ralMetalRenderPlan_t;

typedef struct {
	uintptr_t textureIdentity;
	uint64_t textureGeneration;
	ralFormat_t format;
	uint32_t width;
	uint32_t height;
	uint32_t sampleCount;
	ralLoadOp_t loadOp;
	ralStoreOp_t storeOp;
	ralClearValue_t clearValue;
} ralMetalRenderAttachmentReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t renderGeneration;
	uint32_t colorAttachmentCount;
	ralMetalRenderAttachmentReceipt_t colorAttachments[RAL_MAX_COLOR_ATTACHMENTS];
	qboolean hasDepthAttachment;
	ralMetalRenderAttachmentReceipt_t depthAttachment;
	ralCommandReceipt_t recording;
	ralCommandReceipt_t executable;
	ralSubmissionReceipt_t submission;
	uint64_t completionGeneration;
	qboolean ready;
} ralMetalRenderReceipt_t;

qboolean RalMetal_RenderClearPass( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalRenderPlan_t *plan, uint64_t renderGeneration,
	ralMetalRenderReceipt_t *outReceipt );
qboolean RalMetal_RenderReceiptExact( const ralMetalRenderReceipt_t *a,
	const ralMetalRenderReceipt_t *b );
qboolean RalMetal_RenderPlanValid( const ralMetalRenderPlan_t *plan );

#ifdef __cplusplus
}
#endif

#endif
