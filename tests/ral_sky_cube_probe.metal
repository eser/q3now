// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

// Compile the actual production consumers, not a test copy of their mapping.
#if WIRED_SKY_CUBE_PORTABLE
#include "../code/render/ral/backends/vulkan/renderer/shaders/portable/ral_opengl_product_frag_spv.msl"
#else
#include "../code/render/ral/backends/metal/shaders/ui_solid.msl"
#endif

kernel void wired_sky_cube_probe(texture2d_array<float> sky [[texture(0)]],
        sampler skySampler [[sampler(0)]],
        device const float4 *directions [[buffer(0)]],
        device float4 *samples [[buffer(1)]], uint index [[thread_position_in_grid]]) {
    float3 direction = directions[index].xyz;
    float lod = directions[index].w;
    samples[index] = float4(wired_sample_sky_radiance(sky, skySampler, direction, lod), 1.0);
}
