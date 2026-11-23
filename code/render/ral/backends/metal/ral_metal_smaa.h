// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_METAL_SMAA_H
#define WIRED_RAL_METAL_SMAA_H
#import <Metal/Metal.h>

/* SMAA 1x for the native Metal renderer (r_aa 2). The three catalog passes (luma edge detection, blend-weight calculation,
 * neighbourhood resolve) are the same SPIR-V-derived MSL modules the Vulkan renderer runs; quality presets and the area/search lookup
 * textures are shared too. */
typedef struct ralMetalSmaa_s ralMetalSmaa_t;

#define RAL_METAL_SMAA_QUALITY_LOW    1
#define RAL_METAL_SMAA_QUALITY_MEDIUM 2
#define RAL_METAL_SMAA_QUALITY_HIGH   3
#define RAL_METAL_SMAA_QUALITY_ULTRA  4

ralMetalSmaa_t *RalMetal_SmaaCreate(id<MTLDevice> device);
void RalMetal_SmaaDestroy(ralMetalSmaa_t *state);
/* Encodes edge, blend and resolve passes plus the copy back into `scene` on `command` (in place, before tone mapping and UI; no commit,
 * no wait). `quality` is 1..4. On false the caller must discard the command. The cohort (intermediate textures, pipelines, argument
 * buffers) is rebuilt only when the scene texture or the quality changes. */
bool RalMetal_SmaaEncode(ralMetalSmaa_t *state, id<MTLCommandBuffer> command, id<MTLTexture> scene, int quality);
#endif
