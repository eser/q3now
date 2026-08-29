// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
atmospheric.frag — wired-render atmospheric weather fragment shader

Procedural rain streak / snow flake — no texture binding (the render
descriptor set carries only the frame UBO + pool SSBO). The vertex colour
already encodes rain-grey vs snow-white + alpha; a soft horizontal falloff
across the quad's u axis tapers the streak / flake edges so the alpha-blend
into the HDR FBO reads as a soft particle rather than a hard rectangle.

The colour is sRGB→linear decoded like decal.frag so the linear-space
alpha-blend is correct; alpha stays raw.
*/

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

// Same UBO the vertex shader binds. The fragment consumes the trailing
// soft-particle depth-fade params (invResX/Y, depthValid); the rest keeps the
// std140 layout in lockstep with the host atmFrame_t and the vertex mirror.
layout(set = 0, binding = 0) uniform AtmFrame {
	mat4  mvp;
	vec4  viewLeft;
	vec4  viewUp;
	vec4  eyeWorld;
	float dt;
	float time;
	uint  poolSize;
	uint  pingPongRead;
	vec4  boundsMin;
	vec4  boundsMax;
	vec2  worldMins;
	vec2  worldMaxs;
	vec2  invGridStep;
	uint  gridSize;
	uint  atmType;
	float distance;
	float computePad0;
	float computePad1;
	float computePad2;
	vec4  windGust;
	vec4  precipitation;
	float dustAsh;
	float indoorExposure;
	uint  climateSeed;
	uint  climatePad;
	vec4  climate;
	vec4  surfaceClimate;
	vec4  sun;
	vec4  moon;
	vec4  ambientCloud;
	vec4  cloudMedia;
	vec4  effectMeta;
	vec4  effectWorkloads[48];
	vec4  renderParams;  // xy inverse resolution, z depthValid
};

// Shared scene-depth copy (vk.sceneDepth) — same resource the decal / particle /
// gtao / lens consumers sample. Used for the soft-particle depth fade.
layout(set = 0, binding = 2) uniform sampler2D sceneDepthTex;

layout(location = 0) out vec4 outColor;

// Soft fade band in raw reversed-Z NDC depth units (mirrors gen_frag dfade).
const float ATM_DEPTH_FADE_BAND = 2.0 * 0.0005;

// Precise piecewise sRGB → linear conversion. Duplicated per shader (the
// compile.mjs preprocessor lacks #include); matches the other shader copies
// verbatim per the engine-wide unconditional linear migration.
vec3 sRGBToLinear( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.04045 ) );
	vec3 lo = c / 12.92;
	vec3 hi = pow( ( c + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
	return mix( hi, lo, vec3( cutoff ) );
}

vec3 displayVisibility( vec3 sceneColor ) {
	// The xyz view vectors/eye consume only three lanes. Their std140 padding
	// carries exposure, shadow exponent and toe pivot respectively. Vulkan
	// publishes identity here because its final tonemap owns the transform;
	// direct-present backends publish the live RAL visibility plan.
	vec3 exposed = max( sceneColor, vec3( 0.0 ) ) * viewLeft.w;
	float luminance = dot( exposed, vec3( 0.2126, 0.7152, 0.0722 ) );
	if ( luminance > 0.0 && luminance < eyeWorld.w && viewUp.w != 1.0 ) {
		float curved = pow( luminance / eyeWorld.w, viewUp.w ) * eyeWorld.w;
		exposed *= curved / luminance;
	}
	return exposed;
}

// Soft-particle depth fade: dissolve as the streak/flake nears world geometry.
// Reversed-Z (near≈1, far/cleared≈0): a fragment in front has fragDepth >
// sceneDepth. Returns 1.0 (no fade) when depth isn't fresh or nothing is behind.
float softParticleFade() {
	if ( renderParams.z < 0.5 )
		return 1.0;
	vec2  screenUV   = gl_FragCoord.xy * renderParams.xy;
	float sceneDepth = texture( sceneDepthTex, screenUV ).r;
	if ( sceneDepth <= 0.0 )
		return 1.0;
	float depthDiff = gl_FragCoord.z - sceneDepth;
	return smoothstep( 0.0, ATM_DEPTH_FADE_BAND, depthDiff );
}

void main() {
	// Horizontal in-quad taper (streak/flake shape) — orthogonal to the depth
	// fade below: the taper shapes the edge across the quad WIDTH, the depth fade
	// dissolves the streak where it crosses geometry. Both kept.
	float edge  = 1.0 - smoothstep( 0.0, 0.5, abs( fragUV.x - 0.5 ) );
	float alpha = fragColor.a * edge * softParticleFade();

	vec3 rgb = displayVisibility( sRGBToLinear( fragColor.rgb ) );
	outColor = vec4( rgb, alpha );
}
