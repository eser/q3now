#version 450
// SMAA Pass 3: Neighborhood Blending — Vertex Shader

layout(push_constant) uniform PushConstants {
	vec4 rtMetrics; // { 1/w, 1/h, w, h }
};

layout(location = 0) out vec2 texcoord;
layout(location = 1) out vec4 offset;

out gl_PerVertex { vec4 gl_Position; };

void main() {
	vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
	texcoord = vec2(pos.x, 1.0 - pos.y);

	offset = texcoord.xyxy + rtMetrics.xyxy * vec4(1.0, 0.0, 0.0, 1.0);
}
