// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// MVP migrated off the MSDF 132-byte push constant into the per-draw MSDF UBO at
// set 3 (UNIFORM_BUFFER_DYNAMIC, std140). Field order matches vk_msdf_ubo_t and
// msdf.frag's block exactly. The vertex stage reads only mvp; the rest is the
// fragment stage's (declared here for a matching block layout).
layout(set = 3, binding = 0, std140) uniform MsdfUBO {
    mat4  mvp;
    float outlineWidth;
    float glowWidth;
    vec2  shadowOffset;
    vec4  outlineColor;
    vec4  glowColor;
    vec4  shadowColor;
    uint  bindless_packed_slot;
} msdf;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color0;
layout(location = 2) in vec2 in_tex_coord0;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) out vec2 frag_tex_coord0;

void main() {
    gl_Position = msdf.mvp * vec4(in_position, 1.0);
    frag_color0 = in_color0;
    frag_tex_coord0 = in_tex_coord0;
}
