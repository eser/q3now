// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Shadow depth vertex shader for alpha-tested (cut-out) casters. Transforms the
// caster position into the cascade's light clip space exactly like
// shadow_depth.vert, and additionally forwards the diffuse texture coordinate and
// a packed bindless-index/alpha-func value so the fragment shader can sample the
// diffuse alpha and discard holed-out fragments. Opaque casters use the
// position-only shadow_depth.vert instead; this variant carries the extra
// per-vertex attributes only the cut-out path needs.

layout(set = 1, binding = 0, std140) uniform CascadeMVP {
	mat4 cascadeMVP;   // world -> this cascade's light clip space
};

layout(std430, set = 0, binding = 0) readonly buffer EntityMatrices {
	mat4 matrices[];
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
// (bindless diffuse slot in the low 24 bits | alpha-test func in the high 8 bits),
// carried as a bit-cast float (the RAL maps R32_SFLOAT, not R32_UINT). Constant
// across a surface, so the recovered uint is forwarded flat.
layout(location = 2) in float in_packed;

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) flat out uint out_packed;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	out_texcoord = in_texcoord;
	out_packed   = floatBitsToUint( in_packed );
	gl_Position  = cascadeMVP * matrices[gl_InstanceIndex] * vec4( in_position, 1.0 );
}
