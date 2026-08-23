// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
ribbon.frag — primitive ribbon fragment shader

Output = textured_sample × per-vertex RGBA.

Legacy CGEN_VERTEX-equivalent halving removed. Ribbons
now render at full intensity in the linear pipeline. The push-block
`frameParams.x` field stays in the layout (carries 1.0f written
host-side) for push-range layout compatibility; a later pass drops the
field entirely.

The textured sample comes from the binding-2 sampler array, indexed
by the per-ribbon shader handle (flat-interpolated from the vertex
stage). Slot 0 is reserved for tr.whiteImage; out-of-range handles
clamp to slot 0, so unregistered or huge handles render
"untextured" (white texel = identity multiply on the colour path).
*/

// Must match PRIMITIVE_SHADER_IMAGE_MAX in renderervk/vk.h.
#define PRIMITIVE_SHADER_IMAGE_MAX 64

// Same push block ribbon.vert declares. The
// fragment shader doesn't consume any push field (the legacy
// `frameParams.x` halving was removed), but the declaration must
// match the vertex stage byte layout for Vulkan to accept the
// pipeline. A later pass will trim the dead field.
// Declaration must match ribbon.vert's set-1 effects UBO block (no field read here).
layout(set = 1, binding = 0, std140) uniform EffectsUBO {
	mat4 mvp;
	vec4 eyeWorld;
	vec4 frameParams;
	vec4 _v2;
};

// Per-shader-handle texture array. Populated from
// vk_primitive_shader_images[] by vk_init_primitive_shader_images
// at R_Init time and updated per-shader by
// vk_register_primitive_shader_image. Every slot is initialized to
// tr.whiteImage so unregistered handles render through identity.
// WebGPU-facing ABI: the image array and its common sampler are separate.
// Every ribbon/rail slot uses the same CLAMP sampler, so publishing 64 sampler
// descriptors would add no semantics and would require a sampler binding array
// on portable backends.
layout(set = 0, binding = 2) uniform texture2D shaderImages[PRIMITIVE_SHADER_IMAGE_MAX];
layout(set = 0, binding = 3) uniform sampler shaderImageSampler;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint fragShaderHandle;

layout(location = 0) out vec4 outColor;

// Precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches m1/m2/m3/m4 verbatim. linearToSRGB is unused
// here — driver DCEs it; kept for migration symmetry.
vec3 sRGBToLinear( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.04045 ) );
	vec3 lo = c / 12.92;
	vec3 hi = pow( ( c + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
	return mix( hi, lo, vec3( cutoff ) );
}

vec3 linearToSRGB( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.0031308 ) );
	vec3 lo = c * 12.92;
	vec3 hi = pow( c, vec3( 1.0 / 2.4 ) ) * 1.055 - 0.055;
	return mix( hi, lo, vec3( cutoff ) );
}

// Block 5d-followup: per-instance colour-domain texel fetch — the
// bindless mirror of gen_frag.tmpl's sampleColorTex. domain == 1
// (CD_LINEAR) → raw fetch; domain == 0 (CD_SRGB) → decode. Every
// primitive-shader image is CD_SRGB today, so this is byte-identical
// to the prior unconditional sRGBToLinear; the branch is the seam for
// future channel-map ribbon content.
vec4 sampleColorTexBindless( vec4 sampled, uint domain ) {
	vec4 c = sampled;
	if ( domain != 0u )
		return c;
	c.rgb = sRGBToLinear( c.rgb );
	return c;
}

void main() {
	// Block 5d-followup: fragShaderHandle bits 0..30 = primitive-shader
	// registry slot, bit 31 = the slot image's colour domain (packed by
	// the host in RE_AddRibbonToScene). Mask before the range clamp —
	// the packed value is > PRIMITIVE_SHADER_IMAGE_MAX when bit 31 is set.
	uint domain = fragShaderHandle >> 31u;
	uint handle = fragShaderHandle & 0x7FFFFFFFu;
	// Out-of-range handles clamp to slot 0 (tr.whiteImage) — keeps
	// the shader robust against handles that exceed the registry size.
	uint slot = handle < uint(PRIMITIVE_SHADER_IMAGE_MAX) ? handle : 0u;
	vec4 texel = sampleColorTexBindless(
		texture( sampler2D( shaderImages[slot], shaderImageSampler ), fragUV ), domain );

	// Per-vertex colour decoded to linear (display
	// domain); texel decoded per its colour domain. The rgb/a-separated
	// modulate is unchanged. Alpha stays raw.
	outColor = vec4(
		texel.rgb * sRGBToLinear( fragColor.rgb ),
		texel.a   * fragColor.a
	);
}
