// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_METAL_BLOOM_H
#define WIRED_RAL_METAL_BLOOM_H
#include "../../core/ral_bloom.h"
#import <Metal/Metal.h>
typedef struct ralMetalBloom_s ralMetalBloom_t;
ralMetalBloom_t *RalMetal_BloomCreate(id<MTLDevice> device);
void RalMetal_BloomDestroy(ralMetalBloom_t *state);
/* One ordered command queue, no commit/wait/readback. Encodes in-place scene
 * composition before tone mapping/UI. On false caller must discard command.
 * Native encoders retain the previous cohort during resize/reconfiguration. */
bool RalMetal_BloomEncode(ralMetalBloom_t *state, id<MTLCommandBuffer> command,
    id<MTLTexture> scene, const ralBloomSettings_t *settings);
#endif
