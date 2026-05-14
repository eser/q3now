// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// gamma.frag — Phase 6B3'-c1: thin display-encoding pass.
//
// Reads vk.tonemapped_image (LDR linear), writes the swapchain
// image. All scene-radiance work (exposure, tonemap, SSAO, godrays,
// colour grading, saturation) moved to tonemap.frag in
// Phase 6B3'-c1. This pass only does:
//   * gamma encoding (linear -> sRGB) via r_gamma
//   * framebuffer-bit-depth dither (r_dither)

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
// Phase 6B3'-d: srgb_swapchain == 1 when the swapchain image format is
// VK_FORMAT_*_SRGB and the hardware applies sRGB encoding on present.
// In that mode the shader writes pre-encode values so the user-side
// r_gamma curve still bends the output, but the natural linear -> sRGB
// step lives in hardware (replacing the legacy shader pow encode under
// the UNORM swapchain path).
layout(constant_id = 11) const int srgb_swapchain = 0;
// Phase 6B3'-d8: hdr_mode == 1 when the swapchain colorspace is
// VK_COLOR_SPACE_HDR10_ST2084_EXT — the shader BT.709->BT.2020 + PQ
// (ST.2084) encodes instead of sRGB. hdr_peak_norm is the display peak
// in "graphics-white" units (= r_hdrPeakLuminance / 100); graphics
// white (the value 1.0, e.g. tonemapped diffuse white / UI) maps to
// GRAPHICS_WHITE_NITS, scene highlights up to hdr_peak_norm map up to
// hdr_peak_norm * GRAPHICS_WHITE_NITS = r_hdrPeakLuminance nits.
layout(constant_id = 12) const int   hdr_mode      = 0;
layout(constant_id = 13) const float hdr_peak_norm = 10.0;

const int bayerSize = 8;
const float bayerMatrix[bayerSize * bayerSize] = {
	0,  32, 8,  40, 2,  34, 10, 42,
	48, 16, 56, 24, 50, 18, 58, 26,
	12, 44, 4,  36, 14, 46, 6,  38,
	60, 28, 52, 20, 62, 30, 54, 22,
	3,  35, 11, 43, 1,  33, 9,  41,
	51, 19, 59, 27, 49, 17, 57, 25,
	15, 47, 7,  39, 13, 45, 5,  37,
	63, 31, 55, 23, 61, 29, 53, 21
};

float threshold() {
	ivec2 coordDenormalized = ivec2(gl_FragCoord.xy);
	ivec2 bayerCoord = coordDenormalized % bayerSize;
	float bayerSample = bayerMatrix[bayerCoord.x + bayerCoord.y * bayerSize];
	float threshold = (bayerSample + 0.5) / float(bayerSize * bayerSize);
	return threshold;
}

vec3 dither(vec3 color) {
	ivec3 depth = ivec3(depth_r, depth_g, depth_b);
	vec3 cDenormalized = color * depth;
	vec3 cLow = floor(cDenormalized);
	vec3 cFractional = cDenormalized - cLow;
	vec3 cDithered = cLow + step(threshold(), cFractional);
	return cDithered / depth;
}

// Phase 6B3'-d8: SMPTE ST.2084 (PQ) EOTF^-1, per Rec. ITU-R BT.2100.
// Input L is luminance normalised to the 10000-nit PQ range, clamped
// to [0,1]; output is the PQ-encoded code value in [0,1].
const float PQ_c1 = 0.8359375;        // 3424 / 4096
const float PQ_c2 = 18.8515625;       // 2413 / 128
const float PQ_c3 = 18.6875;          // 2392 / 128
const float PQ_m1 = 0.1593017578125;  // 2610 / 16384
const float PQ_m2 = 78.84375;         // 2523 / 32
vec3 pqEncode( vec3 L ) {
	L = clamp( L, vec3(0.0), vec3(1.0) );
	vec3 Lm1 = pow( L, vec3(PQ_m1) );
	return pow( ( PQ_c1 + PQ_c2 * Lm1 ) / ( 1.0 + PQ_c3 * Lm1 ), vec3(PQ_m2) );
}
// BT.709 -> BT.2020 primary conversion (linear light, D65). BT.709's
// gamut is a subset of BT.2020's, so this never produces out-of-[0,L]
// values. Column-major per GLSL.
const mat3 bt709_to_bt2020 = mat3(
	0.627403895934699,  0.069097289358232, 0.016391438875150,  // col 0
	0.329283038377884,  0.919540395075459, 0.088013307877226,  // col 1
	0.043313065687417,  0.011362315566309, 0.895595253247624   // col 2
);
const float GRAPHICS_WHITE_NITS = 100.0;

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;

	if ( hdr_mode == 1 ) {
		// HDR10 (BT.2020 + PQ). tonemap.frag produced scene-referred
		// linear in roughly [0, hdr_peak_norm] (graphics white = 1.0;
		// the 2D/HUD pass composited at [0,1]). r_gamma stays a user-side
		// linear curve adjustment. Scale graphics-white to ~100 nits and
		// the peak to r_hdrPeakLuminance nits, convert to BT.2020, PQ-
		// encode. Dither below would clamp the [0,1] PQ code to the
		// swapchain bit depth (10bpc → depth_r/g/b = 1023).
		vec3 lin  = ( gamma != 1.0 ) ? pow( max( base, vec3(0.0) ), vec3(gamma) ) : max( base, vec3(0.0) );
		vec3 nits = lin * GRAPHICS_WHITE_NITS;                 // graphics-white = 1.0 → 100 nits
		nits = bt709_to_bt2020 * nits;                          // primaries (linear)
		out_color = vec4( pqEncode( nits / 10000.0 ), 1.0 );    // PQ EOTF^-1 (normalised to 10000 nit)
	}
	else if ( srgb_swapchain == 1 ) {
		// Hardware encodes sRGB on present. r_gamma is a user-side
		// curve adjustment in linear space; the spec constant value
		// is 1.0 / r_gamma (vk.c FragSpecData fill), so the same
		// pow(base, gamma) shape used on the UNORM path also brightens
		// midtones as r_gamma rises here. Identity r_gamma -> linear
		// passthrough -> hardware does the encode.
		if ( gamma != 1.0 ) {
			out_color = vec4(pow(base, vec3(gamma)), 1);
		} else {
			out_color = vec4(base, 1);
		}
	} else {
		// Phase 6B3'-d3 fix B: explicit sRGB OETF for the UNORM
		// swapchain path. The hardware does not encode sRGB on UNORM
		// swapchains; the shader must do it itself. Apply the user-
		// gamma curve first, then sRGB-encode the result for display.
		// pow(x, 1/2.2) is the simplified sRGB OETF; the piecewise-
		// precise OETF is more accurate near black but adds ~5 lines
		// for marginal benefit. max() clamp guards against negatives
		// that would NaN under fractional pow.
		vec3 curved = (gamma != 1.0) ? pow(base, vec3(gamma)) : base;
		out_color = vec4(pow(max(curved, vec3(0.0)), vec3(1.0 / 2.2)), 1);
	}

	if ( ditherMode == 1 ) {
		out_color.rgb = dither(out_color.rgb);
	}
}
