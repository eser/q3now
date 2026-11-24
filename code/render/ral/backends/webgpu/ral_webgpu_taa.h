// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_WEBGPU_TAA_H
#define WIRED_RAL_WEBGPU_TAA_H
#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"

/* Temporal anti-aliasing for the WebGPU renderer (r_aa 1). The resolve and the first-frame bootstrap are the same catalog modules
 * the Vulkan and Metal renderers run (temporal_resolve, temporal_bootstrap_aa). WebGPU projects vertices on the CPU and has no
 * velocity attachment, so a compute kernel rebuilds camera-induced motion, validity and the reversed device depth from the scene
 * depth (the browser host's canvas depth texture) and the current/previous camera, exactly as the Metal renderer does. The result
 * is a resolved HDR texture that the tonemap reads instead of the scene colour. One frame is outstanding at a time, like Product. */
typedef struct ralWebGpuTaa_s ralWebGpuTaa_t;

typedef struct {
	float origin[3], forward[3], left[3], up[3];
	float tanHalfX, tanHalfY, zNear, zFar;
} ralWebGpuTaaCamera_t;

typedef struct {
	uint32_t width, height;
	ralWebGpuTaaCamera_t current, previous;
	float jitterNdc[2];          /* jitter applied to this frame's projection, NDC x right / y up */
	float currentJitterUv[2];    /* the same jitter in UV units, as the resolve compensates it */
	float previousJitterUv[2];
	uint8_t historyValid, readIndex, writeIndex;
} ralWebGpuTaaFrame_t;

qboolean RalWebGpu_TaaCreate( ralWebGpuRuntime_t *runtime, ralWebGpuRuntimeReceipt_t *rr,
	ralWebGpuBrowserBridge_t *bridge, uint64_t generation, ralWebGpuTaa_t **out );
void RalWebGpu_TaaDestroy( ralWebGpuTaa_t *taa, const ralWebGpuRuntimeReceipt_t *rr );
/* Records the motion pass and the resolve (or the bootstrap without history) into the frame's encoder, as one compute pass after
 * the scene passes. `sceneColor` is the HDR scene texture of this frame. On success `*outResolved` is the HDR texture to tonemap. */
qboolean RalWebGpu_TaaRecord( ralWebGpuTaa_t *taa, const ralWebGpuRuntimeReceipt_t *rr,
	ralWebGpuCommand_t *command, ralWebGpuCommandReceipt_t *recording, uintptr_t sceneColor,
	const ralWebGpuTaaFrame_t *frame, uintptr_t *outResolved, uint32_t *outDispatchCount );
/* The RGBA16F effect-mask render target of the cohort (allocated with the other targets), drawn before RalWebGpu_TaaRecord: the
 * caller clears it, draws R = 1 where a pixel must not inherit history (entities without motion, the first-person weapon, the
 * visible part of translucents) and (0, velocity uv, 0) where an entity's object velocity replaces the camera reprojection (A stays
 * 1, the clear value, wherever the camera reprojection holds). */
uintptr_t RalWebGpu_TaaEffectMask( ralWebGpuTaa_t *taa, const ralWebGpuRuntimeReceipt_t *rr, uintptr_t sceneColor,
	uint32_t width, uint32_t height );
uint32_t RalWebGpu_TaaLastFailure( void );
#endif
