// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2013 Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro, Diego Gutierrez
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// SMAA Pass 3: Neighborhood Blending — Fragment Shader
// Ported from the canonical SMAA by Jorge Jimenez et al.

layout(set = 0, binding = 0) uniform sampler2D colorTex;
layout(set = 1, binding = 0) uniform sampler2D blendTex;

layout(push_constant) uniform PushConstants {
	vec4 rtMetrics;
};

layout(location = 0) in vec2 texcoord;
layout(location = 1) in vec4 offset;

layout(location = 0) out vec4 out_color;

void main() {
	// Phase 6B3'-d4-m11: this pass does a weight-normalised bilinear
	// blend of colorTex (= vk.color_image) samples. Post the m1-m_final
	// linear-pipeline migration colorTex holds linear radiance, so the
	// blend is now colorimetrically correct (a weighted blend of
	// sRGB-encoded values — the pre-migration state — gave the wrong
	// midpoint). blendTex carries SMAA blend weights (geometry data),
	// never decoded. No shader change in m11 — correctness improved for
	// free by the upstream domain change.

	// Fetch the blending weights for current pixel:
	//   a.x = right   (from offset.xy)
	//   a.y = top      (from offset.zw)
	//   a.w = bottom   (from texcoord) .x component
	//   a.z = left     (from texcoord) .z component
	vec4 a;
	a.x = texture(blendTex, offset.xy).a;  // Right
	a.y = texture(blendTex, offset.zw).g;  // Top
	a.wz = texture(blendTex, texcoord).xz; // Bottom / Left

	// Early out if no blending needed
	if (dot(a, vec4(1.0)) < 1e-5) {
		out_color = textureLod(colorTex, texcoord, 0.0);
		return;
	}

	// Determine dominant direction: horizontal vs vertical
	bool h = max(a.x, a.z) > max(a.y, a.w);

	// Calculate the blending offsets
	vec4 blendingOffset = vec4(0.0, a.y, 0.0, a.w);
	vec2 blendingWeight = a.yw;
	if (h) {
		blendingOffset = vec4(a.x, 0.0, a.z, 0.0);
		blendingWeight = a.xz;
	}
	blendingWeight /= dot(blendingWeight, vec2(1.0));

	// Calculate the texture coordinates
	vec4 blendingCoord = blendingOffset * vec4(rtMetrics.xy, -rtMetrics.xy) + texcoord.xyxy;

	// Bilinear filtering to mix current pixel with chosen neighbor
	out_color  = blendingWeight.x * textureLod(colorTex, blendingCoord.xy, 0.0);
	out_color += blendingWeight.y * textureLod(colorTex, blendingCoord.zw, 0.0);
}
