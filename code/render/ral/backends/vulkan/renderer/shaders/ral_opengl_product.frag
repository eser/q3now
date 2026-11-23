#version 450
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#extension GL_GOOGLE_include_directive : require

#include "lighting_composition.glsl"
#include "../../../../core/ral_sky_cube.glsl"

#include "ral_product_draw.glsl"

layout(set = 2, binding = 1, std140) uniform ProductMaterialFactors {
	vec4 baseColor;
	vec4 factors;
	vec4 beamStartColor;
	vec4 beamEndColor;
	vec4 beamScaleOffset;
	uvec4 beamFlags; // x: beam draw, y: MSDF text (the base texture is a multi-channel distance atlas)
} productMaterial;

layout(set = 1, binding = 0) uniform texture2D baseTexture;
layout(set = 1, binding = 1) uniform sampler baseSampler;
layout(set = 1, binding = 2) uniform texture2D lightmapTexture;
layout(set = 1, binding = 3) uniform sampler lightmapSampler;
layout(set = 1, binding = 4) uniform texture2DArray staticRadianceTexture;
layout(set = 1, binding = 5) uniform sampler staticRadianceSampler;
layout(set = 1, binding = 6) uniform texture2DArray staticDirectionTexture;
layout(set = 1, binding = 7) uniform sampler staticDirectionSampler;
layout(set = 1, binding = 8) uniform texture2DArray staticVisibilityTexture;
layout(set = 1, binding = 9) uniform sampler staticVisibilitySampler;
layout(set = 1, binding = 10) uniform texture2D physicalSkyTexture;
layout(set = 1, binding = 11) uniform sampler physicalSkySampler;
layout(set = 1, binding = 12) uniform texture2DArray currentSkyRadianceTexture;
layout(set = 1, binding = 13) uniform sampler currentSkyRadianceSampler;
layout(set = 1, binding = 14) uniform texture2DArray previousSkyRadianceTexture;
layout(set = 1, binding = 15) uniform sampler previousSkyRadianceSampler;
layout(set = 2, binding = 0, std430) readonly buffer AtmosphereFroxels {
	vec4 values[];
} atmosphereFroxels;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 lightmapCoord;
layout(location = 2) in vec4 color;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec3 worldNormal;
layout(location = 0) out vec4 outColor;


vec3 wired_sample_sky_radiance(texture2DArray skyTexture,
		sampler skySampler, vec3 direction, float lod) {
	return textureLod(sampler2DArray(skyTexture, skySampler),
		wired_sky_cube_uv_layer(direction), lod).rgb;
}

// Aerial-perspective haze, same semantics as the Vulkan product composite
// (sky_surface_composite.comp wiredAerialInscatter). The 256-byte product
// draw ABI has no free vec4, so the analytic tier packs sun direction
// (snorm8 xyz) and haze weight (unorm8, top byte) into
// atmosphereFroxelGrid.w, which only the FULL froxel tier consumes as an
// arena offset. Weight 0 returns the flat media colour bit-identically.
vec3 wired_aerial_inscatter(vec3 mediaColor, vec3 viewDir) {
	uint packedHaze = productDraw.atmosphereFroxelGrid.w;
	float aerial = float(packedHaze >> 24u) / 255.0;
	if (aerial <= 0.0) return mediaColor;
	vec3 skyIn;
	if (productDraw.atmosphereHeightSky.z > 0.0) {
		vec2 skyViewUv = vec2(fract(atan(viewDir.y, viewDir.x)
			/ (2.0 * 3.141592653589793) + 0.5),
			clamp(asin(clamp(viewDir.z, -1.0, 1.0))
			/ 3.141592653589793 + 0.5, 0.0, 1.0));
		skyIn = textureLod(sampler2D(physicalSkyTexture, physicalSkySampler),
			skyViewUv, 0.0).rgb;
	} else {
		skyIn = wired_sample_sky_radiance(currentSkyRadianceTexture,
			currentSkyRadianceSampler, viewDir, 4.0);
	}
	// Six face-centre taps of the coarsest prefiltered mip: backend-invariant
	// mean (cube and 2D-array sampling read the same texels there).
	const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
	vec3 skySum = vec3(0.0);
	for (int face = 0; face < 6; ++face) {
		vec3 axis = vec3(0.0);
		axis[face / 2] = (face & 1) != 0 ? -1.0 : 1.0;
		skySum += wired_sample_sky_radiance(currentSkyRadianceTexture,
			currentSkyRadianceSampler, axis, 7.0);
	}
	float skyMean = max(dot(skySum / 6.0, luma), 1e-4);
	vec3 directional = skyIn / skyMean;
	vec3 sunDir = normalize(unpackSnorm4x8(packedHaze).xyz + vec3(1e-6));
	float cosT = dot(viewDir, sunDir);
	const float g = 0.55;
	float hg = (1.0 - g * g) / max(pow(1.0 + g * g - 2.0 * g * cosT, 1.5), 1e-4);
	vec3 tinted = mediaColor * mix(vec3(1.0),
		clamp(directional, vec3(0.35), vec3(3.0)), aerial);
	return tinted * mix(1.0, 0.75 + 0.25 * hg, aerial);
}

// Shared medium sample: RGB is in-scattering, W is transmittance. Emissive
// beams consume W only; fog colour is not another additive light source.
vec4 wired_atmosphere_sample() {
	if (productDraw.viewOriginDrawSpace.w >= 0.5) return vec4(0.0, 0.0, 0.0, 1.0);
	if (productDraw.atmosphereFroxelGrid.z > 0u) {
		uvec3 grid = productDraw.atmosphereFroxelGrid.xyz;
		uint x = min(uint(gl_FragCoord.x) / 16u, grid.x - 1u);
		uint y = min(uint(gl_FragCoord.y) / 16u, grid.y - 1u);
		float farDistance = max(productDraw.atmosphereColorVisibility.w, 4.01);
		float viewDistance = dot(worldPosition
			- productDraw.atmosphereEyeDensity.xyz, productDraw.viewForward.xyz);
		float slice = clamp(log(max(viewDistance, 4.0) / 4.0)
			/ log(farDistance / 4.0), 0.0, 0.9999);
		uint z = min(uint(slice * float(grid.z)), grid.z - 1u);
		return atmosphereFroxels.values[productDraw.atmosphereFroxelGrid.w
			+ (z * grid.y + y) * grid.x + x];
	}
	float density = productDraw.atmosphereEyeDensity.w;
	if (productDraw.atmosphereColorVisibility.w > 0.0)
		density = max(density, 3.912023 / productDraw.atmosphereColorVisibility.w);
	if (density <= 0.0) return vec4(0.0, 0.0, 0.0, 1.0);
	float distanceToEye = length(worldPosition - productDraw.atmosphereEyeDensity.xyz);
	float heightWeight = exp(-productDraw.atmosphereHeightSky.y
		* max(worldPosition.z - productDraw.atmosphereHeightSky.x, 0.0));
	float transmittance = clamp(exp(-density * heightWeight * distanceToEye), 0.0, 1.0);
	vec3 inscatter = wired_aerial_inscatter(productDraw.atmosphereColorVisibility.rgb,
		normalize(worldPosition - productDraw.atmosphereEyeDensity.xyz));
	return vec4(inscatter * (1.0 - transmittance), transmittance);
}

void main() {
	vec4 base = texture(sampler2D(baseTexture, baseSampler), texCoord);
	if (productMaterial.beamFlags.y != 0u) {
		// MSDF text, as the Metal UI MSDF fragment: the median of the three channels is the signed distance; coverage is its
		// screen-space anti-aliased step. The flag is per draw, so the derivative stays in uniform control flow.
		float signedDistance = max(min(base.r, base.g), min(max(base.r, base.g), base.b)) - 0.5;
		float screenWidth = max(fwidth(signedDistance), 1.0 / 1024.0);
		base = vec4(1.0, 1.0, 1.0, clamp(signedDistance / screenWidth + 0.5, 0.0, 1.0));
	}
	if (productMaterial.beamFlags.x != 0u) {
		// Canonical beam.frag: interpolate authored colour first; apply
		// texture/descriptor coverage once in the native SRC_ALPHA blend.
		vec3 linearColor = mix(pow((color.rgb + 0.055) / 1.055, vec3(2.4)),
			color.rgb / 12.92, lessThanEqual(color.rgb, vec3(0.04045)));
		outColor = vec4(base.rgb * linearColor * wired_atmosphere_sample().w, base.a * color.a);
		return;
	}
	if (productDraw.atmosphereHeightSky.w > 0.5
			&& productDraw.emissiveRadiance.w >= 0.0) {
		vec3 direction = normalize(worldPosition
			- productDraw.atmosphereEyeDensity.xyz);
		vec3 current = wired_sample_sky_radiance(currentSkyRadianceTexture,
			currentSkyRadianceSampler, direction, 0.0);
		vec3 previous = wired_sample_sky_radiance(previousSkyRadianceTexture,
			previousSkyRadianceSampler, direction, 0.0);
		base = vec4(mix(previous, current,
			clamp(productDraw.emissiveRadiance.w, 0.0, 1.0)), 1.0);
	}
	if (productDraw.atmosphereHeightSky.w > 0.5
			&& productDraw.atmosphereHeightSky.z > 0.0) {
		vec3 direction = normalize(worldPosition
			- productDraw.atmosphereEyeDensity.xyz);
		vec2 physicalUv = vec2(atan(direction.y, direction.x)
			/ (2.0 * 3.141592653589793) + 0.5,
			asin(clamp(direction.z, -1.0, 1.0))
			/ 3.141592653589793 + 0.5);
		vec3 physical = texture(sampler2D(physicalSkyTexture,
			physicalSkySampler), physicalUv).rgb;
		outColor = vec4(wired_display_visibility(
			wired_compose_authored_physical_sky(base.rgb, physical,
				clamp(productDraw.atmosphereHeightSky.z, 0.0, 1.0))), 1.0);
		return;
	}
	if (productDraw.atmosphereHeightSky.w > 0.5
			&& productDraw.emissiveRadiance.w >= 0.0) {
		outColor = vec4(wired_display_visibility(base.rgb), 1.0);
		return;
	}
	int alphaMode = int(productDraw.viewportAlpha.z + 0.5);
	base *= productMaterial.baseColor;
	if (alphaMode == 1 && base.a < productDraw.viewportAlpha.w) discard;
	vec3 localSh = max(productDraw.localSh[0].rgb
		+ productDraw.localSh[1].rgb * worldNormal.x
		+ productDraw.localSh[2].rgb * worldNormal.y
		+ productDraw.localSh[3].rgb * worldNormal.z, vec3(0.0));
	vec3 lighting;
	if (productDraw.staticLighting.x == 2u) {
		vec3 address = vec3(lightmapCoord, float(productDraw.staticLighting.y));
		vec3 radiance = texture(sampler2DArray(staticRadianceTexture,
			staticRadianceSampler), address).rgb;
		vec2 encodedDirection = texture(sampler2DArray(staticDirectionTexture,
			staticDirectionSampler), address).rg;
		if (productDraw.staticLighting.z == 2u)
			encodedDirection = encodedDirection * 0.5 + 0.5;
		lighting = wired_lighting_directional_static_indirect(radiance,
			encodedDirection, worldNormal);
		if (productDraw.staticLighting.w != 0u)
			lighting *= texture(sampler2DArray(staticVisibilityTexture,
				staticVisibilitySampler), address).r;
	} else if (productDraw.staticLighting.x == 1u
			|| productDraw.projectionLightmap.w >= 0.5) {
		// r_lightmapBoost (projectionLightmap.w) in linear space with the established hue-preserving overflow, as the Metal
		// wired_boost_legacy_lightmap: scale, then normalise all channels by the peak when it exceeds one.
		vec3 boosted = texture(sampler2D(lightmapTexture, lightmapSampler), lightmapCoord).rgb
			* max(productDraw.projectionLightmap.w, 1.0);
		float peak = max(boosted.r, max(boosted.g, boosted.b));
		lighting = peak > 1.0 ? boosted / peak : boosted;
	} else lighting = productDraw.localSh[0].w >= 0.5
		? localSh : vec3(1.0);
	vec3 lit = base.rgb * lighting * color.rgb
		+ base.rgb * productDraw.emissiveRadiance.rgb;
	if (productMaterial.factors.x >= 0.0) {
		float metallic = clamp(productMaterial.factors.x, 0.0, 1.0);
		float roughness = clamp(productMaterial.factors.y, 0.0, 1.0);
		float occlusion = clamp(productMaterial.factors.z, 0.0, 1.0);
		float environmentBlend = clamp(productMaterial.factors.w, 0.0, 1.0);
		vec3 normal = normalize(worldNormal);
		vec3 viewDirection = normalize(productDraw.atmosphereEyeDensity.xyz
			- worldPosition);
		vec3 reflected = reflect(-viewDirection, normal);
		vec3 currentSpecular = wired_sample_sky_radiance(
			currentSkyRadianceTexture, currentSkyRadianceSampler,
			reflected, roughness * float(textureQueryLevels(sampler2DArray(
				currentSkyRadianceTexture, currentSkyRadianceSampler)) - 1));
		vec3 previousSpecular = wired_sample_sky_radiance(
			previousSkyRadianceTexture, previousSkyRadianceSampler,
			reflected, roughness * float(textureQueryLevels(sampler2DArray(
				previousSkyRadianceTexture, previousSkyRadianceSampler)) - 1));
		vec3 f0 = mix(vec3(0.04), base.rgb, metallic);
		float nDotV = max(dot(normal, viewDirection), 0.0);
		vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - nDotV, 5.0);
		vec3 specularIbl = mix(previousSpecular, currentSpecular,
			environmentBlend) * fresnel;
		// Lightmap/local SH/vertex lighting above owns diffuse. Environment
		// contributes only the independent view-dependent reflection term.
		lit += specularIbl * occlusion;
	}
	vec4 media = wired_atmosphere_sample();
	lit = lit * media.w + ((alphaMode == 3 || alphaMode == 4) ? vec3(0.0) : media.rgb);
	if (productDraw.viewOriginDrawSpace.w < 0.5)
		lit = wired_display_visibility(lit);
	outColor = vec4(lit, base.a * color.a);
}
