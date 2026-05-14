// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2013 Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro, Diego Gutierrez
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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

// Phase 6B4: relative-luma delta uses center luma as denominator,
// guarded against zero by a small epsilon. Empirically chosen
// to avoid amplifying noise in near-black pixels without
// distorting the threshold semantic for non-trivial radiance.
const float SMAA_LUMA_EPSILON = 1e-4;

void main() {
	vec2 threshold = vec2(SMAA_THRESHOLD);

	// Calculate lumas
	float L      = dot(texture(colorTex, texcoord).rgb, lumaWeights);
	float Lleft  = dot(texture(colorTex, offset[0].xy).rgb, lumaWeights);
	float Ltop   = dot(texture(colorTex, offset[0].zw).rgb, lumaWeights);

	// Threshold check — relative luma delta (HDR-aware).
	// Phase 6B4: divide neighbour delta by center luma so the
	// threshold reads as "percentage deviation" instead of an
	// absolute luma difference. Under r_hdr 1 (SFLOAT), absolute
	// deltas over-trigger in bright regions and under-trigger
	// in shadow regions; the relative form keeps the published
	// SMAA quality presets meaningful at any input magnitude.
	// Threshold 0.10 = "neighbour must differ by >=10% of L".
	// Phase 6B3'-d4-m11: the m1-m_final linear-pipeline migration
	// makes colorTex (= vk.color_image) hold linear radiance; this
	// relative-luma form already absorbs that domain shift (the
	// ratio delta/L is unitless), so no threshold re-tune was
	// needed — the local-contrast-adaptation step below uses an
	// absolute-delta ratio (finalDelta vs delta.xy), also scale-
	// invariant. Verification only; no math change.
	vec4 delta;
	delta.xy = abs(L - vec2(Lleft, Ltop));
	vec2 relDelta = delta.xy / max(L, SMAA_LUMA_EPSILON);
	vec2 edges = step(threshold, relDelta);

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
