// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_METAL_ATMOSPHERE_H
#define WIRED_RAL_METAL_ATMOSPHERE_H
#include "../../../frontend/render_submission.h"
#include "ral_metal_upload_cache.h"

#define WIRED_METAL_ATMOSPHERE_CAPACITY 262144u
#define WIRED_METAL_ATMOSPHERE_SECTION_BYTES (WIRED_METAL_ATMOSPHERE_CAPACITY * 16u)
typedef struct ralMetalAtmosphere_s ralMetalAtmosphere_t;
ralMetalAtmosphere_t *RalMetal_AtmosphereCreate(id<MTLDevice> device);
void RalMetal_AtmosphereDestroy(ralMetalAtmosphere_t *state);
id<MTLBuffer> RalMetal_AtmosphereArena(ralMetalAtmosphere_t *state);
float RalMetal_AtmosphereFarDistance(const atmosphereFrameState_t *state);
/* Same ordered scene queue as the raster consumer; no commit, wait or readback.
 * Upload is immutable until caller's actual scene command completes. History
 * parity/validity remain owned by the successful-submission transaction. */
bool RalMetal_AtmosphereEncode(ralMetalAtmosphere_t *state,
    id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
    const ralAtmospherePlanReceipt_t *plan, bool historyValid, bool historyParity,
    wiredMetalUpload &upload);
#endif
