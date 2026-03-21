#version 450
// SMAA Pass 1: Luma Edge Detection — Fragment Shader
// Ported from the canonical SMAA by Jorge Jimenez et al.

layout(set = 0, binding = 0) uniform sampler2D colorTex;

layout(push_constant) uniform PushConstants {
	vec4 rtMetrics;
};

layout(constant_id = 0) const float SMAA_THRESHOLD = 0.1;

layout(location = 0) in vec2 texcoord;
layout(location = 1) in vec4 offset[3];

layout(location = 0) out vec2 out_edges;

const float SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR = 2.0;
const vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);

void main() {
	vec2 threshold = vec2(SMAA_THRESHOLD);

	// Calculate lumas
	float L      = dot(texture(colorTex, texcoord).rgb, lumaWeights);
	float Lleft  = dot(texture(colorTex, offset[0].xy).rgb, lumaWeights);
	float Ltop   = dot(texture(colorTex, offset[0].zw).rgb, lumaWeights);

	// Threshold check
	vec4 delta;
	delta.xy = abs(L - vec2(Lleft, Ltop));
	vec2 edges = step(threshold, delta.xy);

	// Discard if no edge
	if (dot(edges, vec2(1.0)) == 0.0)
		discard;

	// Calculate right and bottom deltas for local contrast adaptation
	float Lright  = dot(texture(colorTex, offset[1].xy).rgb, lumaWeights);
	float Lbottom = dot(texture(colorTex, offset[1].zw).rgb, lumaWeights);
	delta.zw = abs(L - vec2(Lright, Lbottom));

	vec2 maxDelta = max(delta.xy, delta.zw);

	// Left-left and top-top deltas
	float Lleftleft = dot(texture(colorTex, offset[2].xy).rgb, lumaWeights);
	float Ltoptop   = dot(texture(colorTex, offset[2].zw).rgb, lumaWeights);
	delta.zw = abs(vec2(Lleft, Ltop) - vec2(Lleftleft, Ltoptop));

	maxDelta = max(maxDelta, delta.zw);
	float finalDelta = max(maxDelta.x, maxDelta.y);

	// Local contrast adaptation
	edges *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);

	out_edges = edges;
}
