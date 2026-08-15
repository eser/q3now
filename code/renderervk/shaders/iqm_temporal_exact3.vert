// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#version 450

// H5a definition-only exact IQM payload.  The fixed record ABI is shared with
// temporalIqmGpuRecord_t; gl_InstanceIndex is the exact record index supplied
// as firstInstance by the eventual active path.
const uint TEMPORAL_IQM_MAX_RECORDS = 256u;

struct TemporalIqmRecord {
	vec4 currentBones[128 * 3];
	vec4 previousBones[128 * 3];
	mat4 rasterMvp;
	mat4 temporalCurrentMvp;
	mat4 temporalPreviousMvp;
};

layout(std430, set = 0, binding = 0) readonly buffer TemporalIqmPayload {
	TemporalIqmRecord records[];
} temporalIqmPayload;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec4 in_tangent;
layout(location = 4) in vec4 in_bone_weights;
layout(location = 5) in uvec4 in_bone_indices;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec4 frag_tangent;
layout(location = 10) out vec4 temporalCurrentClip;
layout(location = 11) out vec4 temporalPreviousClip;

out gl_PerVertex {
	vec4 gl_Position;
};

vec4 currentBoneRow( uint recordIndex, uint index, uint row ) {
	uint offset = index * 3u + row;
	return temporalIqmPayload.records[recordIndex].currentBones[offset];
}

vec4 previousBoneRow( uint recordIndex, uint index, uint row ) {
	uint offset = index * 3u + row;
	return temporalIqmPayload.records[recordIndex].previousBones[offset];
}

vec3 transformCurrentPosition( uint recordIndex, uint index, vec3 position ) {
	return vec3(
		dot( currentBoneRow( recordIndex, index, 0u ), vec4( position, 1.0 ) ),
		dot( currentBoneRow( recordIndex, index, 1u ), vec4( position, 1.0 ) ),
		dot( currentBoneRow( recordIndex, index, 2u ), vec4( position, 1.0 ) ) );
}

vec3 transformPreviousPosition( uint recordIndex, uint index, vec3 position ) {
	return vec3(
		dot( previousBoneRow( recordIndex, index, 0u ), vec4( position, 1.0 ) ),
		dot( previousBoneRow( recordIndex, index, 1u ), vec4( position, 1.0 ) ),
		dot( previousBoneRow( recordIndex, index, 2u ), vec4( position, 1.0 ) ) );
}

vec3 transformDirection( uint recordIndex, uint index, vec3 direction ) {
	return vec3(
		dot( currentBoneRow( recordIndex, index, 0u ).xyz, direction ),
		dot( currentBoneRow( recordIndex, index, 1u ).xyz, direction ),
		dot( currentBoneRow( recordIndex, index, 2u ).xyz, direction ) );
}

vec3 skinCurrentPosition( uint recordIndex ) {
	vec3 value = in_bone_weights.x
		* transformCurrentPosition( recordIndex, in_bone_indices.x, in_position );
	if ( in_bone_weights.y > 0.0 ) value += in_bone_weights.y
		* transformCurrentPosition( recordIndex, in_bone_indices.y, in_position );
	if ( in_bone_weights.z > 0.0 ) value += in_bone_weights.z
		* transformCurrentPosition( recordIndex, in_bone_indices.z, in_position );
	if ( in_bone_weights.w > 0.0 ) value += in_bone_weights.w
		* transformCurrentPosition( recordIndex, in_bone_indices.w, in_position );
	return value;
}

vec3 skinPreviousPosition( uint recordIndex ) {
	vec3 value = in_bone_weights.x
		* transformPreviousPosition( recordIndex, in_bone_indices.x, in_position );
	if ( in_bone_weights.y > 0.0 ) value += in_bone_weights.y
		* transformPreviousPosition( recordIndex, in_bone_indices.y, in_position );
	if ( in_bone_weights.z > 0.0 ) value += in_bone_weights.z
		* transformPreviousPosition( recordIndex, in_bone_indices.z, in_position );
	if ( in_bone_weights.w > 0.0 ) value += in_bone_weights.w
		* transformPreviousPosition( recordIndex, in_bone_indices.w, in_position );
	return value;
}

vec3 skinDirection( uint recordIndex, vec3 direction ) {
	vec3 value = in_bone_weights.x
		* transformDirection( recordIndex, in_bone_indices.x, direction );
	if ( in_bone_weights.y > 0.0 ) value += in_bone_weights.y
		* transformDirection( recordIndex, in_bone_indices.y, direction );
	if ( in_bone_weights.z > 0.0 ) value += in_bone_weights.z
		* transformDirection( recordIndex, in_bone_indices.z, direction );
	if ( in_bone_weights.w > 0.0 ) value += in_bone_weights.w
		* transformDirection( recordIndex, in_bone_indices.w, direction );
	return value;
}

void main() {
	uint recordIndex = gl_InstanceIndex;
	if ( recordIndex >= TEMPORAL_IQM_MAX_RECORDS ) {
		gl_Position = vec4( 0.0, 0.0, -2.0, 1.0 );
		frag_tex_coord = vec2( 0.0 );
		frag_normal = vec3( 0.0 );
		frag_tangent = vec4( 0.0 );
		temporalCurrentClip = vec4( 0.0 );
		temporalPreviousClip = vec4( 0.0 );
		return;
	}

	vec3 currentPosition = skinCurrentPosition( recordIndex );
	vec3 previousPosition = skinPreviousPosition( recordIndex );
	vec3 currentNormal = skinDirection( recordIndex, in_normal );
	vec3 currentTangent = skinDirection( recordIndex, in_tangent.xyz );

	gl_Position = temporalIqmPayload.records[recordIndex].rasterMvp
		* vec4( currentPosition, 1.0 );
	temporalCurrentClip = temporalIqmPayload.records[recordIndex].temporalCurrentMvp
		* vec4( currentPosition, 1.0 );
	temporalPreviousClip = temporalIqmPayload.records[recordIndex].temporalPreviousMvp
		* vec4( previousPosition, 1.0 );
	frag_tex_coord = in_tex_coord;
	frag_normal = normalize( currentNormal );
	frag_tangent = vec4( normalize( currentTangent ), in_tangent.w );
}
