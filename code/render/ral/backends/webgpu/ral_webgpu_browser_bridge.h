// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_BROWSER_BRIDGE_H
#define WIRED_RAL_WEBGPU_BROWSER_BRIDGE_H

#include "ral_webgpu_browser_abi.h"
#include "ral_webgpu_pipeline.h"
#include "ral_webgpu_presentation.h"
#include "ral_webgpu_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef qboolean ( *ralWebGpuBrowserDispatchFn )( void *userData,
	ralWebGpuBrowserOpcode_t opcode, const void *request,
	uint32_t requestBytes, void *response, uint32_t responseBytes );

typedef struct ralWebGpuBrowserBridge_s ralWebGpuBrowserBridge_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	uintptr_t canvasIdentity;
	void *userData;
	ralWebGpuBrowserDispatchFn dispatch;
} ralWebGpuBrowserBridgeCreateInfo_t;

typedef struct {
	qboolean lost;
	qboolean recoverable;
	char reason[RAL_WEBGPU_BROWSER_ABI_REASON_BYTES];
} ralWebGpuBrowserLoss_t;

typedef struct {
	uint32_t binding;
	ralWebGpuBrowserBindResourceKind_t kind;
	uintptr_t resourceIdentity;
	/* Buffers use byte offset/size. Textures use base mip/mip count; zeroes
	 * request the resource's default full view. */
	uint64_t offset;
	uint64_t byteSize;
} ralWebGpuBrowserBindResource_t;

qboolean RalWebGpu_BrowserBridgeCreate(
	const ralWebGpuBrowserBridgeCreateInfo_t *createInfo,
	ralWebGpuBrowserBridge_t **outBridge );
void RalWebGpu_BrowserBridgeDestroy( ralWebGpuBrowserBridge_t *bridge );
qboolean RalWebGpu_BrowserBridgeBuildCoreInfo(
	ralWebGpuBrowserBridge_t *bridge, ralWebGpuCoreCreateInfo_t *outInfo );
qboolean RalWebGpu_BrowserBridgeBuildPresentationInfo(
	ralWebGpuBrowserBridge_t *bridge,
	ralWebGpuPresentationCreateInfo_t *outInfo );
qboolean RalWebGpu_BrowserBridgeBuildResourceInfo(
	ralWebGpuBrowserBridge_t *bridge,
	ralWebGpuResourceLayerCreateInfo_t *outInfo );
qboolean RalWebGpu_BrowserBridgeBuildCommandInfo(
	ralWebGpuBrowserBridge_t *bridge, ralWebGpuCommandCreateInfo_t *outInfo );
qboolean RalWebGpu_BrowserBridgeApplyPipelineInfo(
	ralWebGpuBrowserBridge_t *bridge, ralWebGpuPipelineCreateInfo_t *info );
qboolean RalWebGpu_BrowserBridgePollLoss(
	ralWebGpuBrowserBridge_t *bridge, ralWebGpuBrowserLoss_t *outLoss );
qboolean RalWebGpu_BrowserBridgeCreateBindGroup(
	ralWebGpuBrowserBridge_t *bridge, uintptr_t layoutIdentity,
	const ralWebGpuBrowserBindResource_t *entries, uint32_t entryCount,
	uintptr_t *outIdentity );
void RalWebGpu_BrowserBridgeDestroyBindGroup(
	ralWebGpuBrowserBridge_t *bridge, uintptr_t identity );

typedef struct {
	float originRadius[4];
	uint32_t identity[4];
} ralWebGpuBrowserLensSource_t;
/* params: origin/forward/left/up/projection/viewportOpticsCount, six vec4.
 * projection.w is the exact finite-far depth scale used by the world pass.
 * Success with query=0 means bounded backpressure, not a visible fallback. */
qboolean RalWebGpu_BrowserBridgeBeginLensVisibility(
	ralWebGpuBrowserBridge_t *bridge, uint64_t scope, uint64_t frame,
	const float params[24], const ralWebGpuBrowserLensSource_t *sources,
	uint32_t count, uintptr_t *outQuery );
ralWebGpuAsyncStatus_t RalWebGpu_BrowserBridgePollLensVisibility(
	ralWebGpuBrowserBridge_t *bridge, uint64_t scope, uint64_t frame,
	uintptr_t query, float values[RAL_WEBGPU_BROWSER_LENS_SOURCE_LIMIT] );
qboolean RalWebGpu_BrowserBridgeResetLensVisibility(
	ralWebGpuBrowserBridge_t *bridge, uint64_t scope );
void RalWebGpu_BrowserBridgeReleaseLensVisibility(
	ralWebGpuBrowserBridge_t *bridge, uintptr_t query );

#ifdef __cplusplus
}
#endif

#endif
