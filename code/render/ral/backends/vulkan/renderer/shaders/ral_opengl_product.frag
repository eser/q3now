#version 450
#extension GL_GOOGLE_include_directive : require

#include "lighting_composition.glsl"

layout(push_constant) uniform ProductDraw {
	vec4 viewOriginDrawSpace;
	vec4 viewForward;
	vec4 viewLeft;
	vec4 viewUp;
	vec4 projectionLightmap;
	vec4 viewportAlpha;
	vec4 atmosphereEyeDensity;
	vec4 atmosphereColorVisibility;
	vec4 atmosphereHeightCloud;
	uvec4 atmosphereFroxelGrid;
	vec4 localSh[4];
	uvec4 staticLighting;
	vec4 emissiveRadiance;
} productDraw;

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
layout(set = 2, binding = 0, std430) readonly buffer AtmosphereFroxels {
	vec4 values[];
} atmosphereFroxels;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 lightmapCoord;
layout(location = 2) in vec4 color;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec3 worldNormal;
layout(location = 0) out vec4 outColor;

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

void main() {
	vec4 base = texture(sampler2D(baseTexture, baseSampler), texCoord);
	int alphaMode = int(productDraw.viewportAlpha.z + 0.5);
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
		lighting = min(texture(sampler2D(lightmapTexture, lightmapSampler),
			lightmapCoord).rgb * 2.0, vec3(1.0));
	} else lighting = productDraw.localSh[0].w >= 0.5
		? localSh : vec3(1.0);
	vec3 lit = base.rgb * lighting * color.rgb
		+ base.rgb * productDraw.emissiveRadiance.rgb;
	if (productDraw.viewOriginDrawSpace.w < 0.5
			&& productDraw.atmosphereFroxelGrid.z > 0u) {
		uvec3 grid = productDraw.atmosphereFroxelGrid.xyz;
		uint x = min(uint(gl_FragCoord.x) / 16u, grid.x - 1u);
		uint y = min(uint(gl_FragCoord.y) / 16u, grid.y - 1u);
		float farDistance = max(productDraw.atmosphereColorVisibility.w, 4.01);
		float viewDistance = length(worldPosition
			- productDraw.atmosphereEyeDensity.xyz);
		float slice = clamp(log(max(viewDistance, 4.0) / 4.0)
			/ log(farDistance / 4.0), 0.0, 0.9999);
		uint z = min(uint(slice * float(grid.z)), grid.z - 1u);
		vec4 media = atmosphereFroxels.values[productDraw.atmosphereFroxelGrid.w
			+ (z * grid.y + y) * grid.x + x];
		outColor = vec4(wired_display_visibility(lit * media.w + media.rgb),
			base.a * color.a);
		return;
	}
	float density = productDraw.atmosphereEyeDensity.w;
	if (productDraw.atmosphereColorVisibility.w > 0.0)
		density = max(density,
			3.912023 / productDraw.atmosphereColorVisibility.w);
	if (productDraw.viewOriginDrawSpace.w < 0.5 && density > 0.0) {
		float distanceToEye = length(worldPosition
			- productDraw.atmosphereEyeDensity.xyz);
		float heightWeight = exp(-productDraw.atmosphereHeightCloud.y
			* max(worldPosition.z - productDraw.atmosphereHeightCloud.x, 0.0));
		float transmittance = exp(-density * heightWeight * distanceToEye);
		lit = mix(productDraw.atmosphereColorVisibility.rgb, lit,
			clamp(transmittance, 0.0, 1.0));
	}
	if (productDraw.viewOriginDrawSpace.w < 0.5)
		lit = wired_display_visibility(lit);
	outColor = vec4(lit, base.a * color.a);
}
