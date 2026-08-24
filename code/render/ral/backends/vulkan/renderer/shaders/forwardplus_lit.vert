// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Forward+ world-space lit vertex shader (the tiled-lighting consumer).
//
// Mirrors light_vert.tmpl's vertex INPUTS exactly (xyz@0, texcoord@1, normal@2 —
// the 3 attributes vk_bind_lighting binds for BOTH world and entity lit surfaces),
// so the fp draw consumes the same tess geometry the PMLIGHT additive pass does.
// The DIFFERENCE from light_vert: it emits WORLD-space position + normal + view
// (instead of the per-light object-space L vector), because the fragment iterates
// the tile's lights from the world-space dlight SSBO and sums them in ONE pass.
// Worldspawn lit surfaces have an identity model transform → in_position/in_normal
// are world-space; entity surfaces are mapped through modelMatrix (set-0 UBO) so
// world_pos/world_normal are world-space for them too (the union carries both).

layout(set = 0, binding = 0) uniform UBO {
	vec4 eyePos;              // xyz = world eye position (vertex)
	vec4 _pad_light[3];       // light/ent union (unused here; keeps the 128 B prefix)
	vec4 fogDistanceVector;
	vec4 fogDepthVector;
	vec4 fogEyeT;
	vec4 fogColor;
	// pad over the host's cascade/modelMatrix span to mvp @ 480 (the light_vert
	// no-shadow stride; the fp pipeline is built without the shadow tail).
	vec4 _pad_to_modelMatrix[18];   // 128 -> 416
	mat4 modelMatrix;               // offset 416 — entity model->world (identity for worldspawn)
	mat4 mvp;                       // offset 480
};

layout(location = 0) in vec3 in_position;   // object/rest-space (entity) or world-space (worldspawn)
layout(location = 1) in vec2 in_tex_coord;  // the lighting bundle's diffuse UV
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec3 world_pos;
layout(location = 2) out vec3 world_normal;
layout(location = 3) out vec3 world_view;   // fragment -> eye

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position    = mvp * vec4( in_position, 1.0 );
	frag_tex_coord = in_tex_coord;
	// world-space position via modelMatrix (identity for worldspawn → no-op; the
	// entity model->world for model surfaces). Same modelMatrix the USE_SHADOWMAP
	// receiver uses to put shadowData in world space.
	vec3 wp        = ( modelMatrix * vec4( in_position, 1.0 ) ).xyz;
	world_pos      = wp;
	// world-space normal: the 3x3 of modelMatrix (rotation; worldspawn identity).
	world_normal   = mat3( modelMatrix ) * in_normal;
	world_view     = eyePos.xyz - wp;
}
