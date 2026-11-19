// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_BLOOM_H
#define WIRED_RAL_BLOOM_H
#include <stdint.h>
#include <math.h>
#include <float.h>
#define WIRED_BLOOM_MAX_LEVELS 4u
#define WIRED_BLOOM_ENERGY_SCALE 2.5f
/* Linear scene radiance, before tonemap and UI. Shader implementations come
 * from the canonical bloom/downsample/upsample/composite shader catalog. */
typedef struct {
    uint32_t enabled, levels, thresholdMode;
    float threshold, intensity;
} ralBloomSettings_t;
typedef struct {
    uint32_t levels, passCount;
    uint32_t width[1u + WIRED_BLOOM_MAX_LEVELS];
    uint32_t height[1u + WIRED_BLOOM_MAX_LEVELS];
    float compositeScale;
} ralBloomPlan_t;
static inline uint32_t Ral_BloomNextExtent(uint32_t extent) {
    return extent > 1u ? extent / 2u : 1u;
}
static inline int Ral_BloomSettingsValid(const ralBloomSettings_t *s) {
    return s && s->enabled <= 1u && s->levels >= 1u
        && s->levels <= WIRED_BLOOM_MAX_LEVELS && s->thresholdMode <= 2u
        && isfinite(s->threshold) && s->threshold >= 0.0f
        && isfinite(s->intensity) && s->intensity >= 0.0f
        && s->intensity <= FLT_MAX / WIRED_BLOOM_ENERGY_SCALE;
}
static inline int Ral_BloomPlanBuild(const ralBloomSettings_t *s,
        uint32_t width, uint32_t height, ralBloomPlan_t *out) {
    ralBloomPlan_t p = {0u, 0u, {0u}, {0u}, 0.0f};
    if (!out || !width || !height || !Ral_BloomSettingsValid(s)) return 0;
    p.width[0] = width; p.height[0] = height;
    if (s->enabled) {
        p.levels = s->levels;
        p.passCount = 2u * p.levels + 1u;
        p.compositeScale = s->intensity * WIRED_BLOOM_ENERGY_SCALE;
        for (uint32_t i = 1u; i <= p.levels; ++i) {
            p.width[i] = Ral_BloomNextExtent(p.width[i-1u]);
            p.height[i] = Ral_BloomNextExtent(p.height[i-1u]);
        }
    }
    *out = p;
    return 1;
}
#endif
