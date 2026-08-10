// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Shadow depth vertex shader — renders scene from light's perspective.
// Only outputs position (depth is written automatically by the rasterizer).

// cascadeMVP rides in a per-cascade UBO at set 1 (UNIFORM_BUFFER_DYNAMIC, bound with
// a per-cascade dynamic offset), not a push constant. The shadow depth pipeline has
// ZERO push constants — the WebGPU-portable surface.
layout(set = 1, binding = 0, std140) uniform CascadeMVP {
	mat4 cascadeMVP;   // world -> this cascade's light clip space
};

// Per-frame caster model->world matrices (std430, mat4[]); indexed by
// gl_InstanceIndex == firstInstance (every shadow draw uses instanceCount=1, so the
// base instance maps directly — no VK_KHR_shader_draw_parameters needed). Replaces
// the per-(entity x cascade) modelMatrix push.
layout(std430, set = 0, binding = 0) readonly buffer EntityMatrices {
	mat4 matrices[];
};

layout(location = 0) in vec3 in_position;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = cascadeMVP * matrices[gl_InstanceIndex] * vec4( in_position, 1.0 );
}
