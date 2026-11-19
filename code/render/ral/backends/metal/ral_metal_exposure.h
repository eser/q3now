// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_METAL_EXPOSURE_H
#define WIRED_RAL_METAL_EXPOSURE_H
#include "../../core/ral_exposure.h"
#import <Metal/Metal.h>
typedef struct ralMetalExposure_s ralMetalExposure_t;
ralMetalExposure_t *RalMetal_ExposureCreate( id<MTLDevice> device );
void RalMetal_ExposureDestroy( ralMetalExposure_t *state );
/* Caller uses one ordered command queue. No commit/wait/readback here.
 * Output is +1 owned and per-command, never overwritten by another frame.
 * Cancellation before commit leaves GPU history unchanged. */
bool RalMetal_ExposureEncode( ralMetalExposure_t *state,
	id<MTLCommandBuffer> command, id<MTLTexture> scene,
	const ralExposureSettings_t *settings, float dt, bool reset,
	id<MTLBuffer> *outExposure );
#endif
