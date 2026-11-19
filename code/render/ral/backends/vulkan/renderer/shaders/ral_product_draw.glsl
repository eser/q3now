// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
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

vec3 wired_display_visibility(vec3 sceneColor) {
	// The xyz view axes consume only three components. Their std140 padding
	// channels carry the portable display plan without growing the 256-byte
	// product draw ABI: forward.w=exposure, left.w=exponent, up.w=pivot.
	vec3 exposed = max(sceneColor, vec3(0.0)) * productDraw.viewForward.w;
	float luminance = dot(exposed, vec3(0.2126, 0.7152, 0.0722));
	if (luminance > 0.0 && luminance < productDraw.viewUp.w
			&& productDraw.viewLeft.w != 1.0) {
		float curved = pow(luminance / productDraw.viewUp.w,
			productDraw.viewLeft.w) * productDraw.viewUp.w;
		exposed *= curved / luminance;
	}
	return exposed;
}

vec3 wired_compose_authored_physical_sky(vec3 authored, vec3 physical,
		float blend) {
	const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
	float authoredEnergy = dot(max(authored, vec3(0.0)), luma);
	float physicalEnergy = dot(max(physical, vec3(0.0)), luma);
	if (authoredEnergy <= 1e-5) return mix(authored, physical, blend);
	float scale = clamp(physicalEnergy / max(authoredEnergy, 0.18), 0.65, 1.75);
	vec3 physicalTint = physicalEnergy > 1e-5
		? physical / physicalEnergy : vec3(1.0);
	vec3 relit = authored * scale
		* mix(vec3(1.0), clamp(physicalTint, vec3(0.5), vec3(2.0)), 0.12)
		+ physical * 0.08;
	return mix(authored, relit, blend);
}
