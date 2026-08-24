// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Skinned shadow depth vertex shader — renders GPU-skinned IQM casters from the
// light's perspective. Mirrors iqm_skinning.vert's 4-bone position blend, then
// applies the caster model->world matrix (set 0 SSBO, indexed by gl_InstanceIndex)
// and this cascade's light view-proj (set 1 UBO). Depth-only: no normal/texcoord
// outputs. The bone matrices are the SAME ones the main IQM pass computes for this
// entity this frame (reused at the capture seam — same pose, no recompute).

// set 1 = per-cascade cascadeMVP UBO (UNIFORM_BUFFER_DYNAMIC, bound with a
// per-cascade dynamic offset). Shared with shadow_depth.vert — same set/binding.
layout(set = 1, binding = 0, std140) uniform CascadeMVP {
	mat4 cascadeMVP;   // world -> this cascade's light clip space
};

// set 0 = per-frame caster model->world matrices (std430, mat4[]), indexed by
// gl_InstanceIndex == firstInstance. Shared with shadow_depth.vert. For a skinned
// IQM caster this is the entity's model->world ([axis|origin]); the bone matrices
// below are in model space, so model->world maps the skinned model-space vertex
// into world space (which cascadeMVP then maps into light clip space).
layout(std430, set = 0, binding = 0) readonly buffer EntityMatrices {
	mat4 matrices[];
};

// set 2 = this caster's bone matrices (128 * mat3x4 stored as vec4[3] per joint).
// Same packing as iqm_skinning.vert's BoneMatrices (UNIFORM_BUFFER_DYNAMIC, bound
// with a per-caster dynamic offset). NO mvp tail — the shadow pass uses
// cascadeMVP * model->world instead.
// boneMats[joint*3 + 0] = row 0 (m00 m01 m02 m03), +1 = row 1, +2 = row 2.
layout(set = 2, binding = 0) uniform BoneMatrices {
	vec4 boneMats[128 * 3];
};

// IQM interleaved vertex — depth-only needs position + bone weights/indices only,
// but the binding stride is the full IQM stride (the model VBO is shared with the
// main pass), so the unused attributes (normal/texcoord/tangent) are still declared
// to keep the vertex-input layout identical and let the driver fetch by offset.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;       // unused (depth-only)
layout(location = 2) in vec2 in_tex_coord;    // unused (depth-only)
layout(location = 3) in vec4 in_tangent;      // unused (depth-only)
layout(location = 4) in vec4 in_bone_weights;
layout(location = 5) in uvec4 in_bone_indices;

out gl_PerVertex {
	vec4 gl_Position;
};

vec3 transformByBone(uint idx, vec3 v) {
	return vec3(
		dot(boneMats[idx * 3u + 0u], vec4(v, 1.0)),
		dot(boneMats[idx * 3u + 1u], vec4(v, 1.0)),
		dot(boneMats[idx * 3u + 2u], vec4(v, 1.0))
	);
}

void main() {
	// blend position across up to 4 bones — identical weighting to iqm_skinning.vert
	vec3 pos = in_bone_weights.x * transformByBone(in_bone_indices.x, in_position);
	if (in_bone_weights.y > 0.0) pos += in_bone_weights.y * transformByBone(in_bone_indices.y, in_position);
	if (in_bone_weights.z > 0.0) pos += in_bone_weights.z * transformByBone(in_bone_indices.z, in_position);
	if (in_bone_weights.w > 0.0) pos += in_bone_weights.w * transformByBone(in_bone_indices.w, in_position);

	gl_Position = cascadeMVP * matrices[gl_InstanceIndex] * vec4(pos, 1.0);
}
