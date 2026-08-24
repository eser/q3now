// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// mvp rides in the set-0 vkUniform_t ring (filled by VK_PushUniform /
// VK_PushUniformScratch, which stamp mvp from vk_world.mvp), not a VS push constant.
// mvp lands at host offset 480 (FEAT_SHADOW_MAPPING build); pad the std140 prefix to
// it. color.vert reads only mvp, so the prefix is opaque padding.
layout(set = 0, binding = 0) uniform UBO {
	vec4 _pad_to_mvp[30];   // 0 -> 480 (480 bytes)
	mat4 mvp;               // host offset 480
} ubo;

layout(location = 0) in vec3 in_position;
//layout(location = 1) in vec4 in_color;
//layout(location = 2) in vec2 in_tex_coord0;
//layout(location = 3) in vec2 in_tex_coord1;

//layout(location = 0) out vec4 frag_color;
//layout(location = 1) out vec2 frag_tex_coord0;
//layout(location = 2) out vec2 frag_tex_coord1;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = ubo.mvp * vec4(in_position, 1.0);

	//frag_color = in_color;
	//frag_tex_coord0 = in_tex_coord0;
}
