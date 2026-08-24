#version 450

layout(push_constant) uniform ProductDraw {
	vec4 viewOriginDrawSpace;
	vec4 viewForward;
	vec4 viewLeft;
	vec4 viewUp;
	vec4 projectionLightmap;
	vec4 viewportAlpha;
} productDraw;

layout(set = 1, binding = 0) uniform texture2D baseTexture;
layout(set = 1, binding = 1) uniform sampler baseSampler;
layout(set = 1, binding = 2) uniform texture2D lightmapTexture;
layout(set = 1, binding = 3) uniform sampler lightmapSampler;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 lightmapCoord;
layout(location = 2) in vec4 color;
layout(location = 0) out vec4 outColor;

void main() {
	vec4 base = texture(sampler2D(baseTexture, baseSampler), texCoord);
	int alphaMode = int(productDraw.viewportAlpha.z + 0.5);
	if (alphaMode == 1 && base.a < productDraw.viewportAlpha.w) discard;
	vec3 lighting = productDraw.projectionLightmap.w >= 0.5
		? min(texture(sampler2D(lightmapTexture, lightmapSampler), lightmapCoord).rgb
			* 2.0, vec3(1.0)) : vec3(1.0);
	outColor = vec4(base.rgb * lighting * color.rgb, base.a * color.a);
}
