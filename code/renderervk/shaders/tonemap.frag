#version 450

// tonemap.frag — Phase 6B3'-c1: scene-radiance post-process pass.
//
// Reads vk.color_image (HDR linear scene + bloom composition),
// writes vk.tonemapped_image (LDR linear). Runs between bloom
// composition and the gamma encode pass.
//
// All scene-radiance work concentrates here (effects that
// operate on the colour values themselves — not display encoding):
//   * exposure_bias (r_brightness)
//   * SSAO (depth-aware darken)
//   * godrays (depth-aware additive)
//   * tonemap operator (PBR Neutral / AgX / Lottes / Reinhard)
//   * colour grading (tint / saturation / contrast)
//   * saturation mix (r_saturation)
// FXAA removed in Phase 6B3'-e — SMAA replaces it as the engine's
// AA path.
//
// gamma.frag downstream remains a thin "linear -> sRGB encode
// + framebuffer-bit-depth dither" pass.

layout(set = 0, binding = 0) uniform sampler2D texture0;
#if defined(USE_SSAO) || defined(USE_GODRAYS)
layout(set = 1, binding = 0) uniform sampler2D depthMap;
#endif

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

// Spec constant IDs match the host-side spec_entries[] in vk.c.
// Gamma + dither IDs (0, 7, 8, 9, 10) are not declared here; the
// driver silently ignores entries whose constantID isn't referenced
// by the bound shader (vk.c documents this behaviour for the
// previously-shared FragSpecData struct).
layout(constant_id = 1) const float exposure_bias = 1.0;
layout(constant_id = 2) const float saturation = 1.0;
// Phase 6B3'-d8: HDR10 display output. hdr_mode == 1 when the swapchain
// colorspace is HDR10_ST2084 (the tonemap operator uses an HDR shoulder
// peaking at hdr_peak_norm = r_hdrPeakLuminance / 100, instead of rolling
// off at 1.0). hdr_peak_norm is in "graphics-white" units — graphics
// white (output 1.0) becomes ~100 nits in the gamma pass, the peak
// becomes r_hdrPeakLuminance nits. Declared unconditionally (used only
// inside applyTonemap under USE_TONEMAP; ignored elsewhere).
layout(constant_id = 12) const int   hdr_mode      = 0;
layout(constant_id = 13) const float hdr_peak_norm = 10.0;
#ifdef USE_SSAO
layout(constant_id = 14) const float ssao_intensity = 1.0;
layout(constant_id = 15) const float zNear = 4.0;
layout(constant_id = 16) const float zFar = 4096.0;
#endif
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
// (19-23), FXAA (24-25), and godrays (26-27) — the lowest free
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
// Phase 6B3'-e: USE_FXAA removed (SMAA replaces it). Spec constant IDs
// 24..25 are FREE for future use.
#ifdef USE_GODRAYS
layout(constant_id = 26) const int godrays_samples = 64;
layout(constant_id = 27) const float godrays_density = 1.0;
layout(push_constant) uniform GodRayParams {
	vec2 sunScreenPos;
	float intensity;
	float decay;
} godray;
#endif

const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

#ifdef USE_SSAO
// Reconstruct linear depth from Z-buffer value
float linearDepth( float z ) {
	return zNear * zFar / ( zFar - z * ( zFar - zNear ) );
}

// Screen-space ambient occlusion — 8-sample hemisphere kernel
float computeSSAO() {
	vec2 texelSize = 1.0 / vec2( textureSize( depthMap, 0 ) );
	float centerDepth = linearDepth( texture( depthMap, frag_tex_coord ).r );

	const float radius = 5.0;
	const vec2 samples[8] = vec2[8](
		vec2( -0.94201,  -0.39906 ),
		vec2(  0.94558,  -0.76890 ),
		vec2( -0.09418,  -0.92938 ),
		vec2(  0.34495,   0.29387 ),
		vec2( -0.91588,   0.45771 ),
		vec2( -0.81544,  -0.87912 ),
		vec2(  0.19984,   0.78641 ),
		vec2(  0.44323,  -0.97511 )
	);

	float angle = fract( sin( dot( gl_FragCoord.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
	float ca = cos( angle * 6.283 );
	float sa = sin( angle * 6.283 );
	mat2 rot = mat2( ca, sa, -sa, ca );

	float occlusion = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		vec2 offset = rot * samples[i] * radius * texelSize;
		float sampleDepth = linearDepth( texture( depthMap, frag_tex_coord + offset ).r );
		float diff = centerDepth - sampleDepth;
		float rangeCheck = smoothstep( 0.0, 1.0, radius * 2.0 / abs( diff ) );
		occlusion += step( 0.5, diff ) * rangeCheck;
	}

	return 1.0 - ( occlusion / 8.0 ) * ssao_intensity;
}
#endif

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
	// Phase 6B3'-d3 fix A: removed pow(x, 2.2) over-correction.
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
	vec3 a      = vec3( lottes_contrast );
	vec3 d      = vec3( lottes_shoulder );
	vec3 hdrMax = vec3( lottes_hdr_max );
	vec3 midIn  = vec3( lottes_mid_in );
	vec3 midOut = vec3( lottes_mid_out );

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

// Phase 6B3'-d8: peak-aware PBR Neutral for HDR10 output. The SDR curve's
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
	color *= vec3( cg_tint_r, cg_tint_g, cg_tint_b );

	float luma = dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
	color = mix( vec3( luma ), color, cg_saturation );

	color = ( color - 0.5 ) * cg_contrast + 0.5;

	return clamp( color, 0.0, 1.0 );
}
#endif

#ifdef USE_GODRAYS
vec3 computeGodRays() {
	vec2 deltaUV = ( frag_tex_coord - godray.sunScreenPos ) * godrays_density / float( godrays_samples );
	vec2 uv = frag_tex_coord;
	float illumination = 0.0;
	float weight = 1.0;

	for ( int i = 0; i < godrays_samples; i++ ) {
		uv -= deltaUV;

		if ( uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 )
			break;

		float depth = texture( depthMap, uv ).r;
#ifdef USE_REVERSED_DEPTH
		float isSky = step( depth, 0.001 );
#else
		float isSky = step( 0.999, depth );
#endif

		illumination += isSky * weight;
		weight *= godray.decay;
	}

	illumination /= float( godrays_samples );
	return vec3( illumination * godray.intensity );
}
#endif

// Phase 6B3'-e: applyFXAA() removed entirely. SMAA (smaa_edge / smaa_blend
// / smaa_resolve) is the AA path going forward.

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;

	// Pre-tonemap exposure bias driven by r_brightness->value
	// (Phase 6B3'-a). Default 1.0 = no boost (linear identity).
	base *= exposure_bias;

#ifdef USE_SSAO
	base *= computeSSAO();
#endif

#ifdef USE_GODRAYS
	base += computeGodRays();
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
