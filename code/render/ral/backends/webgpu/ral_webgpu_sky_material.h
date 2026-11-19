// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_WEBGPU_SKY_MATERIAL_H
#define WIRED_RAL_WEBGPU_SKY_MATERIAL_H
#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"
#include "../../../frontend/render_material_stage_gpu.h"
typedef struct ralWebGpuSkyMaterial_s ralWebGpuSkyMaterial_t;
qboolean RalWebGpu_SkyMaterialCreate(ralWebGpuRuntime_t *runtime,
    ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge,
    uint64_t generation, qboolean depthEnabled, ralWebGpuSkyMaterial_t **out);
// Product owns the single outstanding submit. Prepare/reset/destroy require
// its completed fence; each recorded draw has independent uniform storage.
qboolean RalWebGpu_SkyMaterialPrepare(ralWebGpuSkyMaterial_t *sky,
    const ralWebGpuRuntimeReceipt_t *rr, uint32_t drawCount);
qboolean RalWebGpu_SkyMaterialRecord(ralWebGpuSkyMaterial_t *sky,
    const ralWebGpuRuntimeReceipt_t *rr, ralWebGpuCommand_t *command,
    const renderProductDrawGpuParams_t *drawParams,
    const renderSkyMaterialGpuParams_t *materialParams,
    const uintptr_t textures[10], const uintptr_t samplers[10],
    const ralWebGpuIndexedDraw_t *geometry,
    const ralWebGpuCommandReceipt_t *recording, ralWebGpuCommandReceipt_t *updated);
void RalWebGpu_SkyMaterialReset(ralWebGpuSkyMaterial_t *sky,
    const ralWebGpuRuntimeReceipt_t *rr);
void RalWebGpu_SkyMaterialDestroy(ralWebGpuSkyMaterial_t *sky,
    const ralWebGpuRuntimeReceipt_t *rr);
#endif
