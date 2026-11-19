#version 450
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#extension GL_GOOGLE_include_directive : require
#define WIRED_MATERIAL_GLSL 1
#include "../../../../../frontend/render_material_noise.h"
#include "../../../../core/ral_material_stage.glsl"
#include "ral_product_draw.glsl"

// Ten sampled images, including the outer atlas and physical LUT. No sampled
// image arrays or nonuniform descriptor indexing: fits the WebGPU baseline.
layout(set = 1, binding = 0) uniform texture2D stageImage0;
layout(set = 1, binding = 1) uniform sampler stageSampler0;
layout(set = 1, binding = 2) uniform texture2D stageImage1;
layout(set = 1, binding = 3) uniform sampler stageSampler1;
layout(set = 1, binding = 4) uniform texture2D stageImage2;
layout(set = 1, binding = 5) uniform sampler stageSampler2;
layout(set = 1, binding = 6) uniform texture2D stageImage3;
layout(set = 1, binding = 7) uniform sampler stageSampler3;
layout(set = 1, binding = 8) uniform texture2D stageImage4;
layout(set = 1, binding = 9) uniform sampler stageSampler4;
layout(set = 1, binding = 10) uniform texture2D stageImage5;
layout(set = 1, binding = 11) uniform sampler stageSampler5;
layout(set = 1, binding = 12) uniform texture2D stageImage6;
layout(set = 1, binding = 13) uniform sampler stageSampler6;
layout(set = 1, binding = 14) uniform texture2D stageImage7;
layout(set = 1, binding = 15) uniform sampler stageSampler7;
#ifdef WIRED_SKY_SEPARATE_FACES
// Capture borrows the six original resident faces, without repacking or
// confusing a material's base image with an atlas. Same stage evaluator.
layout(set = 0, binding = 0) uniform texture2D outerRt;
layout(set = 0, binding = 1) uniform texture2D outerBk;
layout(set = 0, binding = 2) uniform texture2D outerLf;
layout(set = 0, binding = 3) uniform texture2D outerFt;
layout(set = 0, binding = 4) uniform texture2D outerUp;
layout(set = 0, binding = 5) uniform texture2D outerDn;
layout(set = 0, binding = 6) uniform sampler outerSampler;
layout(set = 0, binding = 7) uniform texture2D physicalImage;
layout(set = 0, binding = 8) uniform sampler physicalSampler;
#else
layout(set = 1, binding = 16) uniform texture2D outerImage;
layout(set = 1, binding = 17) uniform sampler outerSampler;
layout(set = 1, binding = 18) uniform texture2D physicalImage;
layout(set = 1, binding = 19) uniform sampler physicalSampler;
#endif
layout(set = 2, binding = 1, std140) uniform SkyMaterial {
    uvec4 meta; // stage count, has outer atlas, reserved, reserved
    vec4 timeHeight; // shader seconds, cloud height, reserved, reserved
    MaterialStageParams stages[8];
} skyMaterial;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 lightmapCoord;
layout(location = 2) in vec4 color;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec3 worldNormal;
layout(location = 0) out vec4 outColor;

vec4 wired_sky_stage_sample(uint slot, vec2 uv) {
    switch (slot) {
    case 0u: return texture(sampler2D(stageImage0, stageSampler0), uv);
    case 1u: return texture(sampler2D(stageImage1, stageSampler1), uv);
    case 2u: return texture(sampler2D(stageImage2, stageSampler2), uv);
    case 3u: return texture(sampler2D(stageImage3, stageSampler3), uv);
    case 4u: return texture(sampler2D(stageImage4, stageSampler4), uv);
    case 5u: return texture(sampler2D(stageImage5, stageSampler5), uv);
    case 6u: return texture(sampler2D(stageImage6, stageSampler6), uv);
    default: return texture(sampler2D(stageImage7, stageSampler7), uv);
    }
}

vec4 wired_sky_outer_sample(vec3 direction) {
#ifdef WIRED_SKY_SEPARATE_FACES
    vec2 atlasUv = wired_sky_authored_atlas_uv(direction);
    vec2 cellUv = atlasUv * vec2(3.0, 2.0);
    uint face = uint(floor(cellUv.y)) * 3u + uint(floor(cellUv.x));
    vec2 uv = fract(cellUv);
    switch (face) {
    case 0u: return texture(sampler2D(outerRt, outerSampler), uv);
    case 1u: return texture(sampler2D(outerBk, outerSampler), uv);
    case 2u: return texture(sampler2D(outerLf, outerSampler), uv);
    case 3u: return texture(sampler2D(outerFt, outerSampler), uv);
    case 4u: return texture(sampler2D(outerUp, outerSampler), uv);
    default: return texture(sampler2D(outerDn, outerSampler), uv);
    }
#else
    vec2 atlasUv = wired_sky_authored_atlas_uv_sized(direction,
        vec2(textureSize(sampler2D(outerImage, outerSampler), 0)));
    return textureLod(sampler2D(outerImage, outerSampler), atlasUv, 0.0);
#endif
}

void main() {
    vec3 direction = normalize(worldPosition - productDraw.atmosphereEyeDensity.xyz);
    vec2 cloudUv = wired_sky_cloud_uv(direction, skyMaterial.timeHeight.y);
    vec4 accumulated = vec4(0.0);
    if (skyMaterial.meta.y != 0u)
        accumulated = wired_sky_outer_sample(direction);
    for (uint i = 0u; i < min(skyMaterial.meta.x, 8u); ++i) {
        MaterialStageParams stage = skyMaterial.stages[i];
        uint imageSource = stage.meta.z & 255u;
        uint tcGen = stage.meta.z >> 8u;
        vec2 uv = imageSource == 1u || tcGen == 1u ? lightmapCoord : cloudUv;
        if (tcGen == 2u) {
            vec3 reflected = reflect(direction, normalize(worldNormal));
            uv = vec2(0.5 + reflected.y * 0.5, 0.5 - reflected.z * 0.5);
        } else if (tcGen == 3u)
            uv = vec2(dot(worldPosition, stage.tcGenVectorS.xyz),
                dot(worldPosition, stage.tcGenVectorT.xyz));
        uv = wired_material_stage_uv(uv, stage, skyMaterial.timeHeight.x, worldPosition);
        vec4 source = imageSource == 2u ? vec4(1.0) : wired_sky_stage_sample(i, uv);
        float vertexAlpha = color.a;
        if (stage.generators.y == 8u)
            vertexAlpha = floor(clamp(length(worldPosition - productDraw.atmosphereEyeDensity.xyz)
                / max(stage.tcGenVectorS.w, 0.001), 0.0, 1.0) * 255.0) / 255.0;
        source = wired_material_stage_generators(source, stage, color, vec4(1.0),
            vec3(1.0), vertexAlpha, skyMaterial.timeHeight.x);
        // Mask only this layer's blend; a per-pixel continue makes subsequent
        // implicit-derivative texture samples non-uniform in the WGSL loop.
        bool rejected = (stage.meta.w == 1u && source.a == 0.0)
            || (stage.meta.w == 2u && source.a >= stage.params.x)
            || (stage.meta.w == 3u && source.a < stage.params.x);
        if (!rejected)
            accumulated = source * wired_material_blend_factor(stage.meta.x, source, accumulated)
                + accumulated * wired_material_blend_factor(stage.meta.y, source, accumulated);
    }
    if (productDraw.atmosphereHeightSky.z > 0.0) {
        vec2 uv = vec2(atan(direction.y, direction.x) / 6.28318530718 + 0.5,
            asin(clamp(direction.z, -1.0, 1.0)) / 3.14159265359 + 0.5);
        accumulated.rgb = wired_compose_authored_physical_sky(accumulated.rgb,
            texture(sampler2D(physicalImage, physicalSampler), uv).rgb,
            clamp(productDraw.atmosphereHeightSky.z, 0.0, 1.0));
    }
    // Ordered alpha belongs to the layer composition, not a second framebuffer blend.
    outColor = vec4(wired_display_visibility(accumulated.rgb), 1.0);
}
