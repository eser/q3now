#version 450

// IQM GPU skinning vertex shader
// Transforms vertices using bone matrices from a UBO.
// Uses the same push-constant MVP as the standard pipeline.

// 64 bytes — same as every other renderervk vertex shader
layout(push_constant) uniform Transform {
	mat4 mvp;
};

// set 0, binding 0 — bone matrices (128 * mat3x4 stored as vec4[3] per joint)
// Each bone matrix is a 3x4 affine matrix stored as 3 rows of vec4
// boneMats[joint*3 + 0] = row 0  (m00 m01 m02 m03)
// boneMats[joint*3 + 1] = row 1  (m10 m11 m12 m13)
// boneMats[joint*3 + 2] = row 2  (m20 m21 m22 m23)
layout(set = 0, binding = 0) uniform BoneMatrices {
	vec4 boneMats[128 * 3]; // 128 joints * 3 rows each
};

// vertex inputs — interleaved IQM vertex
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec4 in_tangent;       // tangent (xyz) + bitangent sign (w)
layout(location = 4) in vec4 in_bone_weights;
layout(location = 5) in uvec4 in_bone_indices;

// outputs to fragment shader
layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec4 frag_tangent;     // bone-transformed tangent + sign

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

vec3 transformNormalByBone(uint idx, vec3 n) {
	// normal transform uses the 3x3 part only (no translation)
	return vec3(
		dot(boneMats[idx * 3u + 0u].xyz, n),
		dot(boneMats[idx * 3u + 1u].xyz, n),
		dot(boneMats[idx * 3u + 2u].xyz, n)
	);
}

void main() {
	// blend position across up to 4 bones
	vec3 pos = vec3(0.0);
	vec3 nrm = vec3(0.0);
	vec3 tan = vec3(0.0);

	pos += in_bone_weights.x * transformByBone(in_bone_indices.x, in_position);
	nrm += in_bone_weights.x * transformNormalByBone(in_bone_indices.x, in_normal);
	tan += in_bone_weights.x * transformNormalByBone(in_bone_indices.x, in_tangent.xyz);

	if (in_bone_weights.y > 0.0) {
		pos += in_bone_weights.y * transformByBone(in_bone_indices.y, in_position);
		nrm += in_bone_weights.y * transformNormalByBone(in_bone_indices.y, in_normal);
		tan += in_bone_weights.y * transformNormalByBone(in_bone_indices.y, in_tangent.xyz);
	}
	if (in_bone_weights.z > 0.0) {
		pos += in_bone_weights.z * transformByBone(in_bone_indices.z, in_position);
		nrm += in_bone_weights.z * transformNormalByBone(in_bone_indices.z, in_normal);
		tan += in_bone_weights.z * transformNormalByBone(in_bone_indices.z, in_tangent.xyz);
	}
	if (in_bone_weights.w > 0.0) {
		pos += in_bone_weights.w * transformByBone(in_bone_indices.w, in_position);
		nrm += in_bone_weights.w * transformNormalByBone(in_bone_indices.w, in_normal);
		tan += in_bone_weights.w * transformNormalByBone(in_bone_indices.w, in_tangent.xyz);
	}

	gl_Position = mvp * vec4(pos, 1.0);
	frag_tex_coord = in_tex_coord;
	frag_normal = normalize(nrm);
	frag_tangent = vec4(normalize(tan), in_tangent.w);
}
