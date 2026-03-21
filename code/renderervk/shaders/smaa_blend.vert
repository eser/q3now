#version 450
// SMAA Pass 2: Blending Weight Calculation — Vertex Shader

layout(push_constant) uniform PushConstants {
	vec4 rtMetrics; // { 1/w, 1/h, w, h }
};

layout(constant_id = 0) const int SMAA_MAX_SEARCH_STEPS = 16;

layout(location = 0) out vec2 texcoord;
layout(location = 1) out vec2 pixcoord;
layout(location = 2) out vec4 offset[3];

out gl_PerVertex { vec4 gl_Position; };

void main() {
	vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
	texcoord = vec2(pos.x, 1.0 - pos.y);

	pixcoord = texcoord * rtMetrics.zw;

	// Offsets for the searches (see @PSEUDO_GATHER4)
	offset[0] = texcoord.xyxy + rtMetrics.xyxy * vec4(-0.25, -0.125,  1.25, -0.125);
	offset[1] = texcoord.xyxy + rtMetrics.xyxy * vec4(-0.125, -0.25, -0.125,  1.25);

	// Search loop end boundaries
	offset[2] = vec4(offset[0].xz, offset[1].yw) +
	            vec4(-2.0, 2.0, -2.0, 2.0) * rtMetrics.xxyy * float(SMAA_MAX_SEARCH_STEPS);
}
