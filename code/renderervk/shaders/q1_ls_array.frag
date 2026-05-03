#version 450

// lightstyle blend + texture array animation — fragment shader
// Selects the current and next animation frame from a sampler2DArray,
// cross-fades between them, then multiplies by the 4-style lightmap sum.
//
// UBO layout mirrors vkUniform_t exactly (std140, 9 vec4s):
//   [0]  eyePos
//   [1]  light.pos   — x = tr.refdef.time (ms), y = float(numAnimFrames)
//   [2]  light.color
//   [3]  light.vector
//   [4]  fogDistanceVector
//   [5]  fogDepthVector
//   [6]  fogEyeT
//   [7]  fogColor
//   [8]  q1StyleIntensities   <- x=slot0, y=slot1, z=slot2, w=slot3

layout(set = 0, binding = 0) uniform UBO {
	vec4 _eyePos;
	vec4 _animPad;          // x = tr.refdef.time (ms); y = numAnimFrames
	vec4 _pad1;
	vec4 _pad2;
	vec4 _fogDist;
	vec4 _fogDepth;
	vec4 _fogEyeT;
	vec4 _fogColor;
	vec4 q1StyleIntensities;
};

layout(set = 1, binding = 0) uniform sampler2DArray animArray;
layout(set = 2, binding = 0) uniform sampler2D lm0;
layout(set = 3, binding = 0) uniform sampler2D lm1;
layout(set = 4, binding = 0) uniform sampler2D lm2;
layout(set = 5, binding = 0) uniform sampler2D lm3;
// set=6 intentionally unused (no diffuseMapNext in the array pipeline)

layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 2) centroid in vec2 frag_tex_coord1;

layout(location = 0) out vec4 out_color;

void main() {
	float t        = _animPad.x * (1.0 / 100.0);   // convert ms → frame units (100 ms / frame)
	float nf       = max(1.0, _animPad.y);
	float animPos  = mod(t, nf);
	int   frame    = int(animPos);
	int   nextFr   = int(mod(float(frame) + 1.0, nf));
	float blend    = fract(animPos);

	vec4 diffuseA = texture(animArray, vec3(frag_tex_coord0, float(frame)));
	vec4 diffuseB = texture(animArray, vec3(frag_tex_coord0, float(nextFr)));
	vec4 diffuse  = mix(diffuseA, diffuseB, blend);

	vec4 s0 = texture(lm0, frag_tex_coord1) * q1StyleIntensities.x;
	vec4 s1 = texture(lm1, frag_tex_coord1) * q1StyleIntensities.y;
	vec4 s2 = texture(lm2, frag_tex_coord1) * q1StyleIntensities.z;
	vec4 s3 = texture(lm3, frag_tex_coord1) * q1StyleIntensities.w;

	vec4 lm = s0 + s1 + s2 + s3;

	out_color = diffuse * lm;
}
