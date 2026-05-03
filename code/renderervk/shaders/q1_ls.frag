#version 450

// lightstyle blend — fragment shader
// Blends 4 per-slot lightmaps weighted by q1StyleIntensities from UBO,
// then multiplies by the diffuse texture.
//
// UBO layout mirrors vkUniform_t exactly (std140, 9 vec4s):
//   [0]  eyePos
//   [1]  light.pos   / ent.color[0]
//   [2]  light.color / ent.color[1]
//   [3]  light.vector/ ent.color[2]
//   [4]  fogDistanceVector
//   [5]  fogDepthVector
//   [6]  fogEyeT
//   [7]  fogColor
//   [8]  q1StyleIntensities   <- x=slot0, y=slot1, z=slot2, w=slot3

layout(set = 0, binding = 0) uniform UBO {
	vec4 _eyePos;
	vec4 _animPad;          // x = animBlend (0..1 within a 100ms frame); y/z/w unused
	vec4 _pad1;
	vec4 _pad2;
	vec4 _fogDist;
	vec4 _fogDepth;
	vec4 _fogEyeT;
	vec4 _fogColor;
	vec4 q1StyleIntensities;
};

layout(set = 1, binding = 0) uniform sampler2D diffuseMap;
layout(set = 2, binding = 0) uniform sampler2D lm0;
layout(set = 3, binding = 0) uniform sampler2D lm1;
layout(set = 4, binding = 0) uniform sampler2D lm2;
layout(set = 5, binding = 0) uniform sampler2D lm3;
layout(set = 6, binding = 0) uniform sampler2D diffuseMapNext; // next anim frame; same as diffuseMap when not animated

layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 2) centroid in vec2 frag_tex_coord1;

layout(location = 0) out vec4 out_color;

void main() {
	float animBlend = _animPad.x;
	vec4 diffuseA = texture(diffuseMap,     frag_tex_coord0);
	vec4 diffuseB = texture(diffuseMapNext, frag_tex_coord0);
	vec4 diffuse  = mix(diffuseA, diffuseB, animBlend);

	vec4 s0 = texture(lm0, frag_tex_coord1) * q1StyleIntensities.x;
	vec4 s1 = texture(lm1, frag_tex_coord1) * q1StyleIntensities.y;
	vec4 s2 = texture(lm2, frag_tex_coord1) * q1StyleIntensities.z;
	vec4 s3 = texture(lm3, frag_tex_coord1) * q1StyleIntensities.w;

	vec4 lm = s0 + s1 + s2 + s3;

	out_color = diffuse * lm;
}
