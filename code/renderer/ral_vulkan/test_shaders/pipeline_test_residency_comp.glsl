// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Samples the currently bound texture view at view-local LOD 0 and stores the
// packed RGBA result. The residency test binds a baseMip=2 view, so this proves
// the portable sparse-view update selects the coarse parent rather than mip 0.

#version 450

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform texture2D residencyTexture;
layout(set = 0, binding = 1) uniform sampler residencySampler;
layout(std430, set = 0, binding = 2) buffer OutputData { uint packedRgba; } outputData;

void main() {
	vec4 sampleValue = textureLod(sampler2D(residencyTexture, residencySampler), vec2(0.5), 0.0);
	outputData.packedRgba = packUnorm4x8(sampleValue);
}
