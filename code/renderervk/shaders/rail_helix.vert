// #version 450 provided by rail_common.glsl

/*
===============
rail_helix.vert — reads compute-generated vertices from SSBO

No vertex input bindings — all data fetched via gl_VertexIndex.
Each quad = 6 vertices (2 triangles): indices 0,1,2, 2,1,3
===============
*/

// rail_common.glsl is prepended by compile.sh

layout(std430, set = 0, binding = 1) readonly buffer OutputVerts {
	GPUVertex verts[];
};

layout(push_constant) uniform PushConstants {
	mat4 mvp;
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

out gl_PerVertex {
	vec4 gl_Position;
};

// 6 vertices per quad (2 triangles): v0,v1,v2, v2,v1,v3
const int quadIndices[6] = int[6](0, 1, 2, 2, 1, 3);

void main() {
	uint quadIdx = uint(gl_VertexIndex) / 6;
	uint localIdx = quadIndices[gl_VertexIndex % 6];
	uint idx = quadIdx * 4 + localIdx;

	gl_Position = mvp * verts[idx].pos;
	fragUV = verts[idx].uv;
	fragColor = verts[idx].color;
}
