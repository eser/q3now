// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// Shadow depth vertex shader for translucent (blended) casters. Identical to
// shadow_depth.vert except that the caster's shadow opacity travels in the entity
// matrix: element [0][3] of an affine model->world matrix is always 0, so the host
// stores the opacity (0..1) there and this shader reads it back and clears it before
// transforming. No push constant or extra descriptor is needed, so the pipeline keeps
// the opaque casters' layout.

layout(set = 1, binding = 0, std140) uniform CascadeMVP {
	mat4 cascadeMVP;   // world -> this cascade's light clip space
};

layout(std430, set = 0, binding = 0) readonly buffer EntityMatrices {
	mat4 matrices[];
};

layout(location = 0) in vec3 in_position;
layout(location = 0) flat out float out_opacity;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	mat4 m = matrices[gl_InstanceIndex];
	out_opacity = m[0][3];
	m[0][3] = 0.0;
	gl_Position = cascadeMVP * m * vec4( in_position, 1.0 );
}
