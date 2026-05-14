// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
particle.frag — wired-render particle fragment shader

Phase 5: per-class texture sampling.

The vertex shader emits the particle's class handle as a flat-
interpolated uint (location 2). The handle is the same across all
six vertices of a single particle (gl_InstanceIndex is constant per
quad), so it is uniform across the draw's invocations within the
same primitive — flat is uniform-correct, no GL_EXT_nonuniform_qualifier
needed.

Binding 3 is a fixed-size 64-element COMBINED_IMAGE_SAMPLER array
populated host-side from each class's resolved
shader→stages[0]→bundle[0]→image[0] (with three-tier fallback —
class shader, defaultShader, tr.whiteImage). Slot index = handle - 1.

Output = sampled texel × interpolated vertex color. The vertex color
already carries per-particle alpha-fade (mix(palette[0], palette[0]*colorEndMult, age))
and the class-level palette tint, so multiplication is the correct
composition.
*/

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint particleClassHandle;

// Same UBO that the vertex shader and compute shader bind. The
// fragment shader does not consume any field today, but the
// declaration is kept so the std140 layout matches host-side
// particleFrame_t and stays in lockstep with the vertex/compute
// mirrors. The trailing 16 B is plain padding — pad0 (offset 128)
// was the legacy `identityLight` halving factor, dropped Phase
// 6B3'-a and removed in the Block 9 sweep.
layout(set = 0, binding = 0) uniform ParticleFrame {
	mat4  mvp;
	vec4  viewLeft;
	vec4  viewUp;
	vec4  eyeWorld;
	float dt;
	uint  poolSize;
	uint  numClasses;
	uint  pingPongRead;
	float pad0;
	float pad1;
	float pad2;
	float pad3;
};

layout(set = 0, binding = 3) uniform sampler2D particleSamplers[64];

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
// (CD_LINEAR) → raw fetch (channel-map / pre-decoded content);
// domain == 0 (CD_SRGB) → decode. Every particle-class image is
// CD_SRGB today, so this is byte-identical to the prior unconditional
// sRGBToLinear; the branch is the seam for future channel-map sprites.
vec4 sampleColorTexBindless( sampler2D s, vec2 uv, uint domain ) {
	vec4 c = texture( s, uv );
	if ( domain != 0u )
		return c;
	c.rgb = sRGBToLinear( c.rgb );
	return c;
}

void main() {
	// Phase 6B3'-d4-m5: decode the texel and the per-vertex colour to
	// linear domain. particleSamplers content is (CD_SRGB) display
	// colour by default — decoded; fragColor is a display-domain colour
	// interpolated from the class palette — always decoded. Alpha stays
	// raw (not sRGB-encoded). The texel × vertex-colour modulate and the
	// additive blend into the HDR FBO are linear-correct.
	//
	// Block 5d-followup: bit 31 of particleClassHandle is the class's
	// colour domain (packed by particle.vert); bits 0..30 are the handle.
	uint domain = particleClassHandle >> 31u;
	uint idx    = ( particleClassHandle & 0x7FFFFFFFu ) - 1u;
	vec4 texel  = sampleColorTexBindless( particleSamplers[idx], fragUV, domain );
	outColor    = vec4( texel.rgb * sRGBToLinear( fragColor.rgb ),
	                    texel.a * fragColor.a );
}
