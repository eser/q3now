// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_BROWSER_EMSCRIPTEN_H
#define WIRED_RAL_WEBGPU_BROWSER_EMSCRIPTEN_H

#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean RalWebGpu_BrowserEmscriptenCreate( uint64_t generation,
	uint64_t canvasIdentity, ralWebGpuBrowserBridge_t **outBridge );
typedef struct {
	ralWebGpuBrowserBridge_t *bridge;
	ralWebGpuRuntime_t *runtime;
	ralWebGpuRuntimeReceipt_t runtimeReceipt;
	ralWebGpuPresentation_t *presentation;
	ralWebGpuPresentationReceipt_t configuredReceipt;
	uint32_t width;
	uint32_t height;
} ralWebGpuBrowserModuleBorrow_t;

qboolean RalWebGpu_BrowserModuleBorrow(
	ralWebGpuBrowserModuleBorrow_t *outBorrow );
int RalWebGpu_BrowserModuleStart( uint32_t generation,
	uint32_t canvasIdentity, uint32_t width, uint32_t height );
int RalWebGpu_BrowserModulePoll( void );
int RalWebGpu_BrowserModuleResize( uint32_t width, uint32_t height );
void RalWebGpu_BrowserModuleStop( void );

#ifdef __cplusplus
}
#endif

#endif
