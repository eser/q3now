// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_WEBGPU_EXPOSURE_H
#define WIRED_RAL_WEBGPU_EXPOSURE_H
#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"
#include "ral_display_visibility.h"
#include "ral_exposure.h"
typedef struct ralWebGpuExposure_s ralWebGpuExposure_t;
qboolean RalWebGpu_ExposureCreate( ralWebGpuRuntime_t *runtime,
    ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge,
    uint64_t generation, ralWebGpuExposure_t **out );
void RalWebGpu_ExposureDestroy( ralWebGpuExposure_t *e,
    const ralWebGpuRuntimeReceipt_t *rr );
/* One outstanding frame, matching Product's submission ownership. */
qboolean RalWebGpu_ExposurePrepare( ralWebGpuExposure_t *e,
    const ralWebGpuRuntimeReceipt_t *rr, uint32_t width, uint32_t height,
    uint64_t worldGeneration, int timeMs, qboolean cameraCut,
    const ralExposureSettings_t *settings,
    const ralDisplayVisibilityPlan_t *visibility, uintptr_t *outHdr );
/* `source` 0 (or the scene HDR) meters and tonemaps the scene; another texture of the same extent (the TAA output) replaces it. */
qboolean RalWebGpu_ExposureRecord( ralWebGpuExposure_t *e,
    ralWebGpuCommand_t *command, ralWebGpuCommandReceipt_t *recording,
    uintptr_t outputTarget, uintptr_t source );
void RalWebGpu_ExposureComplete( ralWebGpuExposure_t *e, qboolean success );
void RalWebGpu_ExposureReset( ralWebGpuExposure_t *e );
qboolean RalWebGpu_ExposureSetToneMap(ralWebGpuExposure_t *e,
    int mode, float exposure);
#endif
