// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/ral/backends/metal/shaders/ui_solid.msl"

struct StageCase {
    MaterialStageParams stage;
    float4 uvTime;
    float4 sourceColor;
    float4 destinationColor;
    float4 vertexColor;
    float4 entityColor;
    float4 position;
    float4 diffuse;
};
struct StageProbeBindings {
    constant StageCase *cases [[id(0)]];
    device float4 *values [[id(1)]];
};
kernel void main0(device StageProbeBindings &bindings [[buffer(0)]],
        uint i [[thread_position_in_grid]]) {
    if (i >= 64u) return;
    constant StageCase &c = bindings.cases[i];
    float2 uv = wired_material_stage_uv(c.uvTime.xy, c.stage, c.uvTime.z, c.position.xyz);
    bindings.values[i * 4u] = float4(uv,
        wired_material_wave(c.stage.generators.z, c.stage.rgbWave, c.uvTime.z),
        wired_material_wave(c.stage.generators.w, c.stage.alphaWave, c.uvTime.z));
    bindings.values[i * 4u + 1u] = wired_material_stage_generators(c.sourceColor,
        c.stage, c.vertexColor, c.entityColor, c.diffuse.xyz, c.uvTime.w, c.uvTime.z);
    bindings.values[i * 4u + 2u] = c.sourceColor * wired_material_blend_factor(
        c.stage.meta.x, c.sourceColor, c.destinationColor) + c.destinationColor
        * wired_material_blend_factor(c.stage.meta.y, c.sourceColor, c.destinationColor);
    bindings.values[i * 4u + 3u] = float4(wired_sky_cloud_uv(normalize(c.position.xyz),
        c.diffuse.w), 0.0, 1.0);
}
