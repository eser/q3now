#version 450

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
	worldPosition = in_position;
	worldNormal = normalize(in_normal);
}
