// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
particle.frag — wired-render particle fragment shader

Per-class texture sampling.

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
// Sprite-frame (flipbook) inputs. frameSlot0 == FRAME_SLOT_NONE means the
// particle is static (frameCount <= 1) → sample particleSamplers via the
// class handle exactly as before (byte-identical). Otherwise frameSlot0 /
// frameSlot1 are sampler-array indices (the frame pool) and frameBlend in
// [0,1) interpolates between them.
layout(location = 3) flat in uint  frameSlot0;
layout(location = 4) flat in uint  frameSlot1;
layout(location = 5)      in float frameBlend;

// Same UBO that the vertex shader and compute shader bind. The
// fragment shader does not consume any field today, but the
// declaration is kept so the std140 layout matches host-side
// particleFrame_t and stays in lockstep with the vertex/compute
// mirrors. The trailing 16 B is plain padding — pad0 (offset 128)
// was the legacy `identityLight` halving factor, since dropped
// and removed in a later sweep.
layout(set = 0, binding = 0) uniform ParticleFrame {
	mat4  mvp;
	vec4  viewLeft;
	vec4  viewUp;
	vec4  eyeWorld;
	float dt;
	uint  poolSize;
	uint  numClasses;
	uint  pingPongRead;
	float invResX;       // 1/renderWidth  (screen UV from gl_FragCoord)
	float invResY;       // 1/renderHeight
	float depthValid;    // 1.0 when the shared scene-depth copy is fresh this frame
	float exposureBias;  // auto-exposure bias the tonemap re-multiplies (flare-F1 cancel)
};

// PARTICLE_SAMPLER_COUNT (96) = MAX_PARTICLE_CLASSES (64) per-class slots
// [0..63] + FRAME_POOL_SIZE (32) flipbook frame-pool slots [64..95]. Still
// a fixed bounded array (WebGPU-portable, no bindless, no atlas).
layout(set = 0, binding = 3) uniform sampler2D particleSamplers[96];

// Sentinel forwarded by particle.vert when frameCount <= 1 (static class);
// the fragment then uses the class-handle sampling path unchanged.
const uint FRAME_SLOT_NONE = 0xFFFFFFFFu;
// Shared scene-depth copy (vk.sceneDepth) — same resource the decal / gtao /
// lens consumers sample. Used for the soft-particle depth fade.
layout(set = 0, binding = 4) uniform sampler2D sceneDepthTex;

layout(location = 0) out vec4 outColor;

// Soft-particle fade band in raw reversed-Z NDC depth units. Mirrors gen_frag's
// dfade (depth_fade_scale * 0.0005); a particle dissolves over this depth gap as
// it approaches the geometry behind it. Larger = softer, wider dissolve.
const float PARTICLE_DEPTH_FADE_BAND = 2.0 * 0.0005;

// Soft-particle depth fade: dissolve the alpha as the fragment nears the world
// geometry behind it, so billboards melt into surfaces instead of hard-clipping.
// Raw reversed-Z (near≈1, far/cleared≈0): a fragment IN FRONT of the scene has
// fragDepth > sceneDepth, so depthDiff = fragDepth - sceneDepth is positive and
// smoothstep ramps 0→1 as the particle pulls away from the surface. Returns 1.0
// (no fade) when depth is not fresh this frame or there is no geometry behind.
float softParticleFade() {
	if ( depthValid < 0.5 )
		return 1.0;
	vec2  screenUV   = gl_FragCoord.xy * vec2( invResX, invResY );
	float sceneDepth = texture( sceneDepthTex, screenUV ).r;
	if ( sceneDepth <= 0.0 )           // cleared far plane → nothing behind
		return 1.0;
	float depthDiff = gl_FragCoord.z - sceneDepth;
	return smoothstep( 0.0, PARTICLE_DEPTH_FADE_BAND, depthDiff );
}

// Precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches the other shader copies verbatim. linearToSRGB is unused
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

// Per-instance colour-domain texel fetch — the
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
	// Decode the texel and the per-vertex colour to
	// linear domain. particleSamplers content is (CD_SRGB) display
	// colour by default — decoded; fragColor is a display-domain colour
	// interpolated from the class palette — always decoded. Alpha stays
	// raw (not sRGB-encoded). The texel × vertex-colour modulate and the
	// additive blend into the HDR FBO are linear-correct.
	//
	// Bit 31 of particleClassHandle is the class's
	// colour domain (packed by particle.vert); bits 0..30 are the handle.
	uint domain = particleClassHandle >> 31u;
	vec4 texel;
	if ( frameSlot0 == FRAME_SLOT_NONE ) {
		// Static class (frameCount <= 1): sample by class handle exactly
		// as before this change — byte-identical path.
		uint idx = ( particleClassHandle & 0x7FFFFFFFu ) - 1u;
		texel    = sampleColorTexBindless( particleSamplers[idx], fragUV, domain );
	} else {
		// Flipbook: sample the selected frame; when frameBlend > 0
		// interpolate with the next frame in LINEAR domain (the helper
		// already decodes sRGB→linear when domain == 0), which is
		// energy-correct for the additive HDR blend.
		vec4 t0 = sampleColorTexBindless( particleSamplers[frameSlot0], fragUV, domain );
		if ( frameBlend > 0.0 ) {
			vec4 t1 = sampleColorTexBindless( particleSamplers[frameSlot1], fragUV, domain );
			texel   = mix( t0, t1, frameBlend );
		} else {
			texel   = t0;
		}
	}
	// Soft-particle depth fade dissolves a billboard as it nears the geometry
	// behind it (smoke/sparks melting into walls). The additive particle
	// pipeline is SRC_ALPHA/ONE (vk.c), so the source alpha modulates the
	// additive RGB contribution — a fade→0 makes an additive particle
	// disappear, not just soften.
	//
	// Flipbook particles (frameSlot0 != FRAME_SLOT_NONE — today only the
	// explosion_fire impact burst) are EXEMPT: they are emitted FLUSH on the
	// impact surface (depthDiff ≈ 0), where the fade would ramp to ~0 and
	// erase the whole fireball. An impact burst is meant to sit on the
	// surface it hit, so it must not soft-dissolve. Static particles keep the
	// fade unchanged (byte-identical to before).
	float fade = ( frameSlot0 != FRAME_SLOT_NONE ) ? 1.0 : softParticleFade();

	// Exposure-invariant additive intensity for the flipbook (explosion) path —
	// the flare-hdr-retune F1 convention (vk_flares.c). The additive fireball is
	// composited into the HDR FBO, which the tonemap multiplies by exposure_bias
	// (tonemap.frag). Pre-dividing the flipbook RGB by that same exposureBias and
	// multiplying by a fixed target makes the downstream multiply cancel, so the
	// fireball lands at EXPLOSION_ADDITIVE_TARGET post-tonemap regardless of the
	// auto-exposure state — a dark and a bright scene read it at the same display
	// brightness instead of the exposure-amplified additive crossing the Lottes
	// white point (hdr_max 8.0) and washing to a flat white square. Static
	// (non-flipbook) particles take scale 1.0 → byte-identical to before.
	const float EXPLOSION_ADDITIVE_TARGET = 0.70;
	float rgbScale = ( frameSlot0 != FRAME_SLOT_NONE )
	                 ? ( EXPLOSION_ADDITIVE_TARGET / max( exposureBias, 0.001 ) )
	                 : 1.0;
	outColor   = vec4( texel.rgb * sRGBToLinear( fragColor.rgb ) * rgbScale,
	                   texel.a * fragColor.a * fade );
}
