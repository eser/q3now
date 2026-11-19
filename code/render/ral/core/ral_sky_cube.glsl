// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_SKY_CUBE_GLSL
#define WIRED_RAL_SKY_CUBE_GLSL

// Radiance arrays use GL/Vulkan cube order (+X,-X,+Y,-Y,+Z,-Z).
// This is NOT the Q3 authored rt/bk/lf/ft/up/dn asset UV convention.
// Both GLSL (translated offline to WGSL/MSL/GL) and native Metal use this
// definition. Metal includes it after `using namespace metal`.
#if defined(__METAL_VERSION__)
#define WIRED_SKY_VEC2 float2
#define WIRED_SKY_VEC3 float3
#else
#define WIRED_SKY_VEC2 vec2
#define WIRED_SKY_VEC3 vec3
#endif

// Face-local coordinates are [-1,1], with v increasing down the stored image.
WIRED_SKY_VEC3 wired_sky_cube_direction(uint face, WIRED_SKY_VEC2 uv) {
    float u = uv.x, v = uv.y;
    WIRED_SKY_VEC3 dir;
    if      (face == 0u) dir = WIRED_SKY_VEC3( 1.0, -v, -u);
    else if (face == 1u) dir = WIRED_SKY_VEC3(-1.0, -v,  u);
    else if (face == 2u) dir = WIRED_SKY_VEC3( u,  1.0,  v);
    else if (face == 3u) dir = WIRED_SKY_VEC3( u, -1.0, -v);
    else if (face == 4u) dir = WIRED_SKY_VEC3( u, -v,  1.0);
    else                dir = WIRED_SKY_VEC3(-u, -v, -1.0);
    return normalize(dir);
}

// Nonzero world direction -> normalized texture UV and array layer.
WIRED_SKY_VEC3 wired_sky_cube_uv_layer(WIRED_SKY_VEC3 dir) {
    WIRED_SKY_VEC3 a = abs(dir);
    WIRED_SKY_VEC2 uv;
    uint face;
    if (a.x >= a.y && a.x >= a.z) {
        if (dir.x >= 0.0) { face = 0u; uv = WIRED_SKY_VEC2(-dir.z, -dir.y) / a.x; }
        else             { face = 1u; uv = WIRED_SKY_VEC2( dir.z, -dir.y) / a.x; }
    } else if (a.y >= a.z) {
        if (dir.y >= 0.0) { face = 2u; uv = WIRED_SKY_VEC2( dir.x,  dir.z) / a.y; }
        else             { face = 3u; uv = WIRED_SKY_VEC2( dir.x, -dir.z) / a.y; }
    } else if (dir.z >= 0.0) {
        face = 4u; uv = WIRED_SKY_VEC2(dir.x, -dir.y) / a.z;
    } else {
        face = 5u; uv = WIRED_SKY_VEC2(-dir.x, -dir.y) / a.z;
    }
    return WIRED_SKY_VEC3(uv * 0.5 + 0.5, float(face));
}

#undef WIRED_SKY_VEC2
#undef WIRED_SKY_VEC3
#endif
