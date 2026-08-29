#version 450

// tonemap.frag — scene-radiance post-process pass.
//
// Reads vk.color_image (HDR linear scene + bloom composition),
// writes vk.tonemapped_image (LDR linear). Runs between bloom
// composition and the gamma encode pass.
//
// All scene-radiance work concentrates here (effects that
// operate on the colour values themselves — not display encoding):
//   * exposure_bias (r_brightness)
//   * SSAO (depth-aware darken)
//   * sunrays (depth-aware additive)
//   * tonemap operator (PBR Neutral / AgX / Lottes / Reinhard)
//   * colour grading (tint / saturation / contrast)
//   * saturation mix (r_saturation)
// FXAA is removed — SMAA replaces it as the engine's AA path.
//
// gamma.frag downstream remains a thin "linear -> sRGB encode
// + framebuffer-bit-depth dither" pass.

layout(set = 0, binding = 0) uniform sampler2D texture0;
#ifdef USE_SHOW_AO
// Diagnostic-only GTAO isolation input. This is the denoised visibility field
// produced by the RAL compute pass, not the final scene colour.
layout(set = 3, binding = 0) uniform sampler2D aoMap;
#endif
#ifdef USE_SUNRAYS
layout(set = 1, binding = 0) uniform sampler2D depthMap;
#endif

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

// Per-frame scene-exposure block (set 2). Mirrors vk_exposure_block_t and the
// std430 block in exposure.comp: all 16 fields are 4-byte scalars, so std140 packs
// them tightly at offsets 0..63. exposure_bias arrives here from the
// auto-exposure compute (or r_brightness when auto is off), so it is read from the
// UBO rather than baked as a specialization constant. Declared unconditionally
// because every tonemap variant links the same set-2 layout; the sunray fields are
// read only under USE_SUNRAYS.
layout(set = 2, binding = 0) uniform ExposureBlock {
	float exposure_bias;
	float key;
	float pctLow;
	float pctHigh;
	float rateUp;
	float rateDown;
	float minExp;
	float maxExp;
	int   autoEnabled;
	float brightness;
	float sunScreenX;
	float sunScreenY;
	float sunrayIntensity;
	float sunrayDecay;
	float shadowExponent;
	float shadowPivot;
} eb;

// Spec constant IDs match the host-side spec_entries[] in vk.c.
// Gamma + dither IDs (0, 7, 8, 9, 10) are not declared here; the
// driver silently ignores entries whose constantID isn't referenced
// by the bound shader (vk.c documents this behaviour for the
// previously-shared FragSpecData struct).
layout(constant_id = 2) const float saturation = 1.0;
// HDR10 display output. hdr_mode == 1 when the swapchain
// colorspace is HDR10_ST2084 (the tonemap operator uses an HDR shoulder
// peaking at hdr_peak_norm = r_hdrPeakLuminance / 100, instead of rolling
// off at 1.0). hdr_peak_norm is in "graphics-white" units — graphics
// white (output 1.0) becomes ~100 nits in the gamma pass, the peak
// becomes r_hdrPeakLuminance nits. Declared unconditionally (used only
// inside applyTonemap under USE_TONEMAP; ignored elsewhere).
layout(constant_id = 12) const int   hdr_mode      = 0;
layout(constant_id = 13) const float hdr_peak_norm = 10.0;
#ifdef USE_TONEMAP
// tonemap_mode is wired from r_tonemap->integer host-side. Mode 0
// disables the tonemap pipeline variant entirely (varIdx bit unset
// in vk.c), so this shader path only runs for modes 1..4.
//   1 = PBR Neutral, 2 = AgX, 3 = Lottes, 4 = Reinhard.
layout(constant_id = 17) const int tonemap_mode = 1;
// tonemap_exposure is wired from r_tonemapExposure->value host-side.
layout(constant_id = 18) const float tonemap_exposure = 1.0;
// Lottes (mode 3) configurable filmic parameters wired from
// r_lottes_* host-side. IDs 28-32 skip past color grading
// (19-23), FXAA (24-25), and sunrays (26-27) — the lowest free
// range. Defaults match Timothy Lottes's GDC 2016 canonical curve.
layout(constant_id = 28) const float lottes_contrast = 1.6;
layout(constant_id = 29) const float lottes_shoulder = 0.977;
layout(constant_id = 30) const float lottes_mid_in   = 0.18;
layout(constant_id = 31) const float lottes_mid_out  = 0.267;
layout(constant_id = 32) const float lottes_hdr_max  = 8.0;
#endif
#ifdef USE_COLOR_GRADING
layout(constant_id = 19) const float cg_tint_r = 1.0;
layout(constant_id = 20) const float cg_tint_g = 1.0;
layout(constant_id = 21) const float cg_tint_b = 1.0;
layout(constant_id = 22) const float cg_saturation = 1.0;
layout(constant_id = 23) const float cg_contrast = 1.0;
#endif
// USE_FXAA removed (SMAA replaces it). Spec id 24 is reclaimed from
// the retired FXAA range for chromatic aberration (below); id 25
// (show_ao, a FEAT_SSAO debug slot) stays free — no post-process
// variant declares it, so the host's spec entry is ignored.
//
// Chromatic aberration (lens fringe) strength, wired from
// r_chromaticAberration->value host-side (FragSpecData.chromatic_strength,
// spec entry id 24). 0 = off — main() takes the single-sample input path,
// byte-identical to no effect; the default keeps every existing golden put.
// Declared unconditionally (like `saturation`): it acts at the input sample
// every tonemap variant runs, so it is not gated behind any USE_* #ifdef.
layout(constant_id = 24) const float chromatic_strength = 0.0;
#ifdef USE_SUNRAYS
layout(constant_id = 26) const int sunrays_samples = 64;
layout(constant_id = 27) const float sunrays_density = 1.0;
// Bright-pass threshold for the radial blur: raw HDR scene values below this
// contribute nothing (per-channel soft-knee excess), so only the over-bright sun disc
// / sky seed the shafts. Same raw-HDR-luminance space as bloom's 0.32 bright-pass.
// Spec id 33 (the next free id after the Lottes block 28-32); left at the shader
// default since the host emits no entry for it.
layout(constant_id = 33) const float sunrays_threshold = 0.32;
// sunScreenPos / intensity / decay come from the set-2 ExposureBlock (eb.sunScreenX/Y,
// eb.sunrayIntensity, eb.sunrayDecay) rather than a push constant.
#endif

const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

#ifdef USE_TONEMAP
// PBR Neutral (Khronos glTF 2.0 sample viewer, 2024). Minimal-
// manipulation hue-preserving operator: inputs below ~0.8 pass
// through nearly unchanged, the shoulder above ~0.8 desaturates
// softly toward display white. Best fit for LDR-authored content
// because mid-tones are preserved verbatim.
vec3 tonemapPBRNeutral( vec3 color ) {
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min( color.r, min( color.g, color.b ) );
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max( color.r, max( color.g, color.b ) );
	if ( peak < startCompression )
		return color;

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / ( peak + d - startCompression );
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / ( desaturation * ( peak - newPeak ) + 1.0 );
	return mix( color, newPeak * vec3( 1.0 ), g );
}

// AgX (Troy Sobotka, 2023). Hue-preserving sigmoid tonemap. Inset
// matrix rotates RGB into a colourspace that survives the log-space
// sigmoid without hue shift; outset restores chroma after.
// Matrices are column-major per GLSL convention; values are the
// canonical AgX reference used in Three.js / Bevy / Godot.
const mat3 agxInset = mat3(
	0.856627153315983,  0.0951212405381588, 0.0482516061458583,  // col 0
	0.137318972929847,  0.761241990602591,  0.101439036467562,   // col 1
	0.11189821299995,   0.0767994186031903, 0.811302368396859    // col 2
);
const mat3 agxOutset = mat3(
	 1.1271005818144368,   -0.11060664309660323, -0.016493938717834573,  // col 0
	-0.1413297634984383,    1.157823702216272,   -0.016493938717834257,  // col 1
	-0.1413297634984383,   -0.11060664309660324,  1.2519364065950405     // col 2
);
// 6th-order polynomial approximation of the canonical AgX sigmoid
// (Iestyn Bleasdale-Shepherd's coefficients).
vec3 agxSigmoid( vec3 x ) {
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return + 15.5    * x4 * x2
	       - 40.14   * x4 * x
	       + 31.96   * x4
	       -  6.868  * x2 * x
	       +  0.4298 * x2
	       +  0.1191 * x
	       -  0.00232;
}
vec3 tonemapAgX( vec3 color ) {
	// Inset rotation (slight desaturation for hue-preservation headroom).
	color = agxInset * color;
	// Log-space encoding across [-10 EV, +6.5 EV] around 18% grey.
	const float minEv = -12.47393;
	const float maxEv =   4.026069;
	color = log2( max( color, vec3( 1e-10 ) ) );
	color = clamp( ( color - minEv ) / ( maxEv - minEv ), 0.0, 1.0 );
	// Sigmoid.
	color = agxSigmoid( color );
	// Outset rotation (restores chroma lost in the inset).
	color = agxOutset * color;
	// No pow(x, 2.2) over-correction here.
	// gamma.frag's downstream pipeline does NOT perform an sRGB
	// encode — the hardware swapchain (B8G8R8A8_SRGB under r_fbo 1)
	// does. The prior pow() was darkening output redundantly,
	// making AgX always look dim relative to PBR Neutral, Lottes,
	// and Reinhard. Output now matches the other operators' linear
	// scene-radiance contract. max() clamp retained as a safety
	// guard against tiny negative values from the log-space +
	// sigmoid + outset matrix combination.
	return max( color, vec3( 0.0 ) );
}

// Lottes (Timothy Lottes, "Advanced Techniques and Optimization of
// HDR Color Pipelines", GDC 2016). Configurable filmic curve.
// Contrast and shoulder shape the curve; mid_in/mid_out anchor its
// middle; hdr_max sets the input value mapped to display white.
// All five parameters are spec constants (IDs 28-32), so b and c
// fold to constants after pipeline specialisation.
vec3 tonemapLottes( vec3 color ) {
	vec3 a;
	vec3 d;
	vec3 hdrMax;
	vec3 midIn;
	vec3 midOut;
	a.x = lottes_contrast;
	a.y = lottes_contrast;
	a.z = lottes_contrast;
	d.x = lottes_shoulder;
	d.y = lottes_shoulder;
	d.z = lottes_shoulder;
	hdrMax.x = lottes_hdr_max;
	hdrMax.y = lottes_hdr_max;
	hdrMax.z = lottes_hdr_max;
	midIn.x = lottes_mid_in;
	midIn.y = lottes_mid_in;
	midIn.z = lottes_mid_in;
	midOut.x = lottes_mid_out;
	midOut.y = lottes_mid_out;
	midOut.z = lottes_mid_out;

	vec3 b = ( -pow( midIn, a ) + pow( hdrMax, a ) * midOut ) /
	         ( ( pow( hdrMax, a * d ) - pow( midIn, a * d ) ) * midOut );
	vec3 c = ( pow( hdrMax, a * d ) * pow( midIn, a ) - pow( hdrMax, a ) * pow( midIn, a * d ) * midOut ) /
	         ( ( pow( hdrMax, a * d ) - pow( midIn, a * d ) ) * midOut );

	return pow( color, a ) / ( pow( color, a * d ) * b + c );
}

// Reinhard (Reinhard et al., 1985). Preserved as historical
// reference. Hue-shifts and compresses mid-tones aggressively;
// not hue-preserving like the modern operators above.
vec3 tonemapReinhard( vec3 color ) {
	return color / ( 1.0 + color );
}

// Peak-aware PBR Neutral for HDR10 output. The SDR curve's
// shoulder/ceiling at 1.0 is rescaled to `peak` (= hdr_peak_norm): inputs
// below ~0.76*peak pass through unchanged (toe + mid-tones preserved, so
// diffuse white / UI stays at graphics-white ≈ 100 nits), highlights roll
// off softly toward `peak` (≈ r_hdrPeakLuminance nits) instead of clipping
// at 1.0. With peak == 1.0 this is bit-identical to tonemapPBRNeutral().
vec3 tonemapPBRNeutralHDR( vec3 color, float peak ) {
	float startCompression = ( 0.8 - 0.04 ) * peak;
	const float desaturation = 0.15;

	float x = min( color.r, min( color.g, color.b ) );
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float pk = max( color.r, max( color.g, color.b ) );
	if ( pk < startCompression )
		return color;

	float d = peak - startCompression;
	float newPeak = peak - d * d / ( pk + d - startCompression );
	color *= newPeak / pk;

	float g = 1.0 - 1.0 / ( desaturation * ( pk - newPeak ) + 1.0 );
	return mix( color, newPeak * vec3( 1.0 ), g );
}

vec3 applyTonemap( vec3 color ) {
	color *= tonemap_exposure;
	if ( tonemap_mode == 1 )
		// PBR Neutral (default): a true peak-aware HDR shoulder under HDR10.
		return ( hdr_mode == 1 ) ? tonemapPBRNeutralHDR( color, hdr_peak_norm ) : tonemapPBRNeutral( color );
	else if ( tonemap_mode == 2 )
		// AgX: under HDR10 this still uses the SDR sigmoid — its log-space
		// encoding bakes a [0,1] target, so output stays ≤ graphics white
		// (~100 nits). A true HDR-extended AgX (wider bright-end log range)
		// is a future refinement; for now AgX-on-HDR10 looks like AgX-on-SDR
		// at a brighter paper-white, with no extended-highlight range.
		return tonemapAgX( color );
	else if ( tonemap_mode == 3 )
		// Lottes: output is display-referred [0,1]; same note as AgX — under
		// HDR10 it uses the SDR curve (lottes_hdr_max is the *input* white
		// point, not the output range). HDR-extended Lottes is a refinement.
		return tonemapLottes( color );
	else if ( tonemap_mode == 4 )
		// Reinhard (legacy reference): SDR curve under HDR10 too. Extended
		// Reinhard (L_white from r_hdrPeakLuminance) is a refinement.
		return tonemapReinhard( color );
	else
		return color;  // mode 0 / out-of-range: identity passthrough
}
#endif

#ifdef USE_COLOR_GRADING
vec3 applyColorGrading( vec3 color ) {
	vec3 colorGradeTint;
	colorGradeTint.r = cg_tint_r;
	colorGradeTint.g = cg_tint_g;
	colorGradeTint.b = cg_tint_b;
	color *= colorGradeTint;

	float luma = dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
	color = mix( vec3( luma ), color, cg_saturation );

	color = ( color - 0.5 ) * cg_contrast + 0.5;

	return clamp( color, 0.0, 1.0 );
}
#endif

#ifdef USE_SUNRAYS
// Bright-source radial blur (crepuscular shafts), gated to the sky. March from this
// pixel toward the sun's screen position. A tap seeds shafts only if it is BOTH
// over-bright AND at sky depth: god-rays are sunlight scattered from the sky, so the
// bright sun disc (bright + sky depth) produces shafts while bright in-scene geometry
// (lights, glowing items, lava — bright but at geometry depth in front of the sky)
// does not. The bright-pass is a soft-knee subtraction (mirroring bloom.frag) in raw
// pre-exposure HDR luminance, the same space bloom's 0.32 default lives in.
vec3 computeSunRays() {
	vec2 deltaUV = ( frag_tex_coord - vec2( eb.sunScreenX, eb.sunScreenY ) ) * sunrays_density / float( sunrays_samples );
	vec2 uv = frag_tex_coord;
	vec3 illumination = vec3( 0.0 );
	float weight = 1.0;

	for ( int i = 0; i < sunrays_samples; i++ ) {
		uv -= deltaUV;

		if ( uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 )
			break;

		// Sky gate: only far-plane (sky) taps can seed shafts. The engine renders with a
		// reversed-Z depth buffer (depth compare GREATER_OR_EQUAL, depth cleared to 0),
		// so the far plane — and therefore the sky — is at depth ~0, not 1.
		float depth = texture( depthMap, uv ).r;
		float isSky = step( depth, 0.001 );

		// Bright-pass: per-channel excess above the threshold, so only the over-bright
		// sky/sun survive and mid-tone sky contributes nothing.
		vec3 sceneColor = texture( texture0, uv ).rgb;
		vec3 threshold;
		threshold.x = sunrays_threshold;
		threshold.y = sunrays_threshold;
		threshold.z = sunrays_threshold;
		vec3 bright = max( sceneColor - threshold, vec3( 0.0 ) );

		illumination += bright * isSky * weight;
		weight *= eb.sunrayDecay;
	}

	illumination /= float( sunrays_samples );
	return illumination * eb.sunrayIntensity;
}
#endif

// applyFXAA() is removed entirely. SMAA (smaa_edge / smaa_blend
// / smaa_resolve) is the AA path going forward.

// Chromatic aberration: sample R/G/B from radially-offset UVs so the colour
// channels fringe apart toward the screen edge (a lens dispersion look). The
// offset points along (uv - center), grows with radial distance (zero at the
// centre, maximum at the corners), and scales with chromatic_strength. R is
// pulled outward, B inward, G stays put — the classic red/blue split. The
// offset UVs are clamped to [0,1] so an edge tap never reads outside the frame.
// Only called when chromatic_strength > 0 (main() keeps the single-sample path
// off, so the effect is exactly free and byte-identical when disabled).
vec3 sampleChromatic( vec2 uv ) {
	// MAX_OFFSET is the peak channel separation in UV units at strength 1.0,
	// reached at the frame corner (radial ~0.707). 0.015 UV ≈ 19 px across a
	// 1280-wide frame; the corner net (×0.707) is ~14 px at strength 1.0, ~7 px
	// at the reference 0.5 — a visible, clearly-measurable fringe that stands
	// clear of the engine's cold-launch capture jitter (the enabled-state visual
	// gate relies on it), still tasteful and imperceptible near centre. Tunable:
	// lower for a subtler look, higher for a stronger cinematic dispersion.
	const float MAX_OFFSET = 0.015;
	vec2  dir    = uv - vec2( 0.5 );        // from screen centre
	float radial = length( dir );           // 0 at centre, ~0.707 at corners
	vec2  offset = dir * ( chromatic_strength * radial * MAX_OFFSET );
	float r = texture( texture0, clamp( uv + offset, 0.0, 1.0 ) ).r;
	float g = texture( texture0, uv ).g;
	float b = texture( texture0, clamp( uv - offset, 0.0, 1.0 ) ).b;
	return vec3( r, g, b );
}

void main() {
#ifdef USE_SHOW_AO
	float ao = clamp( texture( aoMap, frag_tex_coord ).r, 0.0, 1.0 );
	out_color = vec4( ao, ao, ao, 1.0 );
	return;
#endif
	// Chromatic aberration acts here, at the input read (a per-channel radial
	// UV offset), before exposure / tonemap / grade / saturation run on `base`.
	// strength 0 (default) keeps the single-sample path — byte-identical to no
	// effect, so the existing goldens do not move.
	vec3 base = ( chromatic_strength > 0.0 )
		? sampleChromatic( frag_tex_coord )
		: texture( texture0, frag_tex_coord ).rgb;

	// Pre-tonemap exposure bias. With auto-exposure on this is the histogram-derived
	// adapted value the reduce compute wrote into the UBO; with auto off it is
	// r_brightness. Default 1.0 = no boost (linear identity).
	base *= eb.exposure_bias;

	// User shadow visibility is a display transform, not ambient light. Remap
	// luminance only below the fixed pivot and rescale RGB uniformly: hue/chroma,
	// mathematical black, the pivot, reference white and highlights stay stable.
	// exponent 1.0 (r_brightness 1) is exact identity.
	float shadowLuma = dot( max( base, vec3( 0.0 ) ),
		vec3( 0.2126, 0.7152, 0.0722 ) );
	if ( eb.shadowExponent != 1.0 && shadowLuma > 0.0
			&& shadowLuma < eb.shadowPivot ) {
		float normalized = shadowLuma / eb.shadowPivot;
		float curved = pow( normalized, eb.shadowExponent ) * eb.shadowPivot;
		base *= curved / shadowLuma;
	}

	// Legacy per-pixel SSAO fully retired: the modern GTAO (gen_frag.tmpl,
	// screen-space visibility applied to the IBL-specular indirect term) is the
	// sole AO path. The old computeSSAO() here multiplied the ENTIRE base by an
	// 8-sample depth-delta kernel that hit exactly 0.0 at full occlusion, stamping
	// pure-black speckles across the frame (per-pixel hash rotation + per-frame
	// zFar → intermittent grain). The call was removed to kill the grain and the
	// double-AO; the dead scaffolding (computeSSAO/linearDepth, the ssao_intensity
	// spec constant, the ssaoZNear/ssaoZFar UBO fields, and the USE_SSAO tonemap
	// variants) is now removed too, completing the SSAO→GTAO migration.

#ifdef USE_SUNRAYS
	base += computeSunRays();
#endif

#ifdef USE_TONEMAP
	base = applyTonemap( base );
#endif

#ifdef USE_COLOR_GRADING
	base = applyColorGrading( base );
#endif

	if ( saturation != 1.0 )
	{
		vec3 luma = vec3(dot(base, sRGB));
		// mix(luma, base, saturation):
		//   saturation = 0 -> luma   (grayscale)
		//   saturation = 1 -> base   (identity, branch skipped above)
		//   saturation > 1 -> super-saturated; clamps on 8-bit fb.
		base = mix(luma, base, saturation);
	}

	out_color = vec4(base, 1);
}
