// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_METAL_TAA_H
#define WIRED_RAL_METAL_TAA_H
#import <Metal/Metal.h>

/* Temporal anti-aliasing for the native Metal renderer (r_aa 1). The resolve and the first-frame bootstrap are the same catalog MSL
 * modules the Vulkan renderer runs (temporal_resolve, temporal_bootstrap_aa), fed with the same quality flags and thresholds. The Metal
 * scene pass has no velocity attachment, so a small compute kernel rebuilds the camera-induced motion, validity and a reversed device
 * depth from the depth buffer and the current/previous view parameters. */
typedef struct ralMetalTaa_s ralMetalTaa_t;

typedef struct {
	uint32_t width, height;
	float currentView[20];      /* ralMetalWorldViewParams_t of this frame, jitter-free (5 x float4) */
	float previousView[20];     /* the previous frame's, valid only when historyValid */
	float jitterNdc[2];         /* the sub-pixel jitter already applied to this frame's projection, NDC x right / y up */
	float currentJitterUv[2];   /* the same jitter as the resolve expects it (UV units) */
	float previousJitterUv[2];
	uint8_t historyValid;
	uint8_t readIndex, writeIndex;
} ralMetalTaaFrame_t;

/* `motionLibrary` is the product library that holds wired_taa_motion_compute. */
ralMetalTaa_t *RalMetal_TaaCreate(id<MTLDevice> device, id<MTLLibrary> motionLibrary);
void RalMetal_TaaDestroy(ralMetalTaa_t *state);
/* Encodes motion, resolve (or the bootstrap when there is no history) and the copy of the result back into `scene`, in place, before
 * bloom and tone mapping. `depth` is the Depth32Float scene depth of the frame. On false the caller must discard the command. */
bool RalMetal_TaaEncode(ralMetalTaa_t *state, id<MTLCommandBuffer> command, id<MTLTexture> scene, id<MTLTexture> depth,
    const ralMetalTaaFrame_t *frame);
#endif
