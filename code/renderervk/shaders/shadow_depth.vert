// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Shadow depth vertex shader — renders scene from light's perspective.
// Only outputs position (depth is written automatically by the rasterizer).

layout(push_constant) uniform Transform {
	mat4 cascadeMVP;   // world -> this cascade's light clip space
	mat4 modelMatrix;  // caster model -> world (identity for the worldspawn batch;
	                   // the entity's [axis|origin] for inline brush-model casters)
};

layout(location = 0) in vec3 in_position;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = cascadeMVP * modelMatrix * vec4( in_position, 1.0 );
}
