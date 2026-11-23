// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_WEBGPU_SMAA_H
#define WIRED_RAL_WEBGPU_SMAA_H
#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"

/* SMAA for the WebGPU renderer (r_aa 2). The three passes (edge detection, blend weights, neighbourhood resolve) are the catalog
 * WGSL modules the Vulkan and Metal renderers use, with the same quality presets and lookup textures. Like the Metal renderer it
 * filters the HDR scene before tone mapping; the result is a separate HDR texture that the tonemap reads instead of the scene. */
typedef struct ralWebGpuSmaa_s ralWebGpuSmaa_t;

qboolean RalWebGpu_SmaaCreate( ralWebGpuRuntime_t *runtime, ralWebGpuRuntimeReceipt_t *rr,
	ralWebGpuBrowserBridge_t *bridge, uint64_t generation, ralWebGpuSmaa_t **out );
void RalWebGpu_SmaaDestroy( ralWebGpuSmaa_t *smaa, const ralWebGpuRuntimeReceipt_t *rr );
/* `quality` 1 low .. 4 ultra (r_smaa; 0 selects 3). Rebuilds the pipelines when it changes; call between frames. */
qboolean RalWebGpu_SmaaSetQuality( ralWebGpuSmaa_t *smaa, ralWebGpuRuntimeReceipt_t *rr, int quality );
/* Records the three render passes after the scene passes. */
qboolean RalWebGpu_SmaaRecord( ralWebGpuSmaa_t *smaa, const ralWebGpuRuntimeReceipt_t *rr,
	ralWebGpuCommand_t *command, ralWebGpuCommandReceipt_t *recording, uintptr_t sceneColor,
	uint32_t width, uint32_t height, uintptr_t *outResolved );
uint32_t RalWebGpu_SmaaLastFailure( void );
#endif
