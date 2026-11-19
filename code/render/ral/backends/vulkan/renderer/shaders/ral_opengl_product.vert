#version 450
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

layout(set = 2, binding = 1, std140) uniform ProductMaterialFactors {
	vec4 baseColor;
	vec4 factors;
	vec4 beamStartColor;
	vec4 beamEndColor;
	vec4 beamScaleOffset;
	uvec4 beamFlags;
} productMaterial;

layout(push_constant) uniform ProductDraw {
	vec4 viewOriginDrawSpace;
	vec4 viewForward;
	vec4 viewLeft;
	vec4 viewUp;
	vec4 projectionLightmap;
	vec4 viewportAlpha;
	vec4 atmosphereEyeDensity;
	vec4 atmosphereColorVisibility;
	vec4 atmosphereHeightSky;
	uvec4 atmosphereFroxelGrid;
	vec4 localSh[4];
	uvec4 staticLighting;
	vec4 emissiveRadiance;
} productDraw;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec2 in_lightmap_coord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in vec3 in_normal;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec2 lightmapCoord;
layout(location = 2) out vec4 color;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 worldNormal;

void main() {
	if (productDraw.viewOriginDrawSpace.w >= 0.5) {
		gl_Position = vec4(in_position.x / productDraw.viewportAlpha.x * 2.0 - 1.0,
			1.0 - in_position.y / productDraw.viewportAlpha.y * 2.0, 0.0, 1.0);
	} else {
		vec3 delta = in_position - productDraw.viewOriginDrawSpace.xyz;
		float forward = dot(delta, productDraw.viewForward.xyz);
		gl_Position = vec4(-dot(delta, productDraw.viewLeft.xyz)
				* productDraw.projectionLightmap.x,
			dot(delta, productDraw.viewUp.xyz) * productDraw.projectionLightmap.y,
			forward - productDraw.projectionLightmap.z, forward);
	}
	texCoord = in_tex_coord;
	lightmapCoord = in_lightmap_coord;
	color = in_color;
	// Sky binds SkyMaterial at this slot; its bytes are not beam operands.
	if (productDraw.atmosphereHeightSky.w < 0.5 && productMaterial.beamFlags.x != 0u) {
		color = mix(productMaterial.beamStartColor, productMaterial.beamEndColor, in_tex_coord.x);
		texCoord = in_tex_coord * productMaterial.beamScaleOffset.xy + productMaterial.beamScaleOffset.zw;
	}
	worldPosition = in_position;
	worldNormal = normalize(in_normal);
}
