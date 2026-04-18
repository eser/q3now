#version 450
// SMAA Pass 1: Edge Detection — Vertex Shader

layout(push_constant) uniform PushConstants {
	vec4 rtMetrics; // { 1/w, 1/h, w, h }
};

layout(location = 0) out vec2 texcoord;
layout(location = 1) out vec4 offset[3];

out gl_PerVertex { vec4 gl_Position; };

void main() {
	// fullscreen triangle: 3 vertices cover entire screen
	vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
	texcoord = pos;

	// neighbor offsets for edge detection
	offset[0] = texcoord.xyxy + rtMetrics.xyxy * vec4(-1.0, 0.0, 0.0, -1.0);
	offset[1] = texcoord.xyxy + rtMetrics.xyxy * vec4( 1.0, 0.0, 0.0,  1.0);
	offset[2] = texcoord.xyxy + rtMetrics.xyxy * vec4(-2.0, 0.0, 0.0, -2.0);
}
