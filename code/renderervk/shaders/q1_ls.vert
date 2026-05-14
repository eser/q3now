// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// lightstyle blend — vertex shader
// Passes position, diffuse UV, and lightmap UV to fragment stage.

layout(push_constant) uniform Transform {
	mat4 mvp;
};

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_tex_coord0;
layout(location = 3) in vec2 in_tex_coord1;

layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;

void main() {
	gl_Position = mvp * vec4(in_position, 1.0);
	frag_tex_coord0 = in_tex_coord0;
	frag_tex_coord1 = in_tex_coord1;
}
