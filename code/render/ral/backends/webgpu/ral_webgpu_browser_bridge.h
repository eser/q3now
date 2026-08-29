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

#ifdef __cplusplus
}
#endif

#endif
