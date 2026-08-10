// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// lightstyle blend — vertex shader
// Passes position, diffuse UV, and lightmap UV to fragment stage.

// mvp rides in the set-0 vkUniform_t ring (filled by VK_PushUniform, which stamps mvp
// from vk_world.mvp), not a VS push constant. mvp at host offset 480
// (FEAT_SHADOW_MAPPING build); pad the std140 prefix. This stage reads
// only mvp; q1_ls.frag declares the same set-0 UBO with its own fields
// (q1StyleIntensities@128 + packed_indices@544) — per-stage member sets differ,
// which is permitted, and the shared offsets are consistent (both index the real
// vkUniform_t layout).
layout(set = 0, binding = 0) uniform UBO {
	vec4 _pad_to_mvp[30];   // 0 -> 480 (480 bytes)
	mat4 mvp;               // host offset 480
} ubo;

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_tex_coord0;
layout(location = 3) in vec2 in_tex_coord1;

layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;

void main() {
	gl_Position = ubo.mvp * vec4(in_position, 1.0);
	frag_tex_coord0 = in_tex_coord0;
	frag_tex_coord1 = in_tex_coord1;
}
