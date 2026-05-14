// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
beam.frag — primitive beam fragment shader

Output = sampled texel × per-vertex color (with fade alpha already
premultiplied host-side). Same shared-sampler-array texturing scheme
ribbon uses; binding index differs (beam: 1, ribbon: 2) because beam's
descriptor set has no points SSBO. Slot 0 is reserved for tr.whiteImage,
so unregistered or out-of-range handles render "untextured" (white
texel × color = color).

Beam does NOT apply CGEN_VERTEX-equivalent halving (it never did,
and Phase 6B3'-a's linear-pipeline migration confirms this stays
the model for all primitives). Per-vertex color arrives via the
`fragColor` varying — beam.vert linearly interpolates between
hdr.startColor and hdr.endColor across the beam's length — and the
texel-modulate is the only color transformation in the fragment
stage. The push-block `frameParams.x` word is reserved/unused — it
was the legacy identityLight halving factor, dropped in Phase
6B3'-a and the named field removed in the Block 9 sweep; the word
stays only so the push range stays byte-compatible across stages.
*/

#define PRIMITIVE_SHADER_IMAGE_MAX 64

// Same push block beam.vert declares; beam.frag does not consume any
// push field today (texel × color is the entire computation), but the
// declaration must match the vertex stage byte layout for Vulkan to
// accept the pipeline.
layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 eyeWorld;
	vec4 frameParams;
	vec4 stageParams;     // Phase 5G — per-draw stage index, kept in
	                      // sync with beam.vert's push declaration.
};

layout(set = 0, binding = 1) uniform sampler2D shaderImages[PRIMITIVE_SHADER_IMAGE_MAX];

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint fragShaderHandle;
// Phase 5G: per-stage image slot. Set in beam.vert from the stage's
// PrimitiveStageGPU.imageSlot. When every stage of a multi-stage
// shader samples the same image (the common case for additive
// scrolling textures registered through RE_RegisterPrimitiveShader)
// this equals fragShaderHandle; when stages use distinct images, the
// vertex shader resolves the right slot per draw.
layout(location = 3) flat in uint fragImageSlot;

layout(location = 0) out vec4 outColor;

// Phase 6B3'-d4-m5: precise piecewise sRGB <-> linear conversion.
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
// future channel-map beam content.
vec4 sampleColorTexBindless( sampler2D s, vec2 uv, uint domain ) {
	vec4 c = texture( s, uv );
	if ( domain != 0u )
		return c;
	c.rgb = sRGBToLinear( c.rgb );
	return c;
}

void main() {
	// Block 5d-followup: fragImageSlot bits 0..30 = primitive-shader
	// registry slot (per-stage; from PrimitiveStageGPU.imageSlot), bit
	// 31 = that image's colour domain (packed host-side in
	// RE_RegisterPrimitiveShader). Mask before the range clamp.
	uint domain = fragImageSlot >> 31u;
	uint handle = fragImageSlot & 0x7FFFFFFFu;
	uint slot = handle < uint(PRIMITIVE_SHADER_IMAGE_MAX) ? handle : 0u;
	vec4 texel = sampleColorTexBindless( shaderImages[slot], fragUV, domain );

	// Phase 6B3'-d4-m5: per-vertex colour decoded to linear. Alpha stays
	// raw. fragColor is beam.vert's linear interpolation of the
	// (display-domain) hdr.startColor..endColor endpoints — decoded
	// here. Texel decoded per its colour domain. Texel × colour modulate
	// + additive HDR blend stay linear-correct.
	outColor = vec4( texel.rgb * sRGBToLinear( fragColor.rgb ),
	                 texel.a * fragColor.a );
}
