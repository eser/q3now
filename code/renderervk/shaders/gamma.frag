#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
#if defined(USE_SSAO) || defined(USE_GODRAYS)
layout(set = 1, binding = 0) uniform sampler2D depthMap;
#endif

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float obScale = 2.0;
// saturation = 1.0 is identity; below desaturates toward
// luma, above super-saturates. Wired from r_saturation->value.
// 8-bit framebuffer clamps any pixel exceeding [0, 1] after
// the mix; super-saturation may visibly clip.
layout(constant_id = 2) const float saturation = 1.0;
//
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
#ifdef USE_SSAO
layout(constant_id = 14) const float ssao_intensity = 1.0;
layout(constant_id = 15) const float zNear = 4.0;
layout(constant_id = 16) const float zFar = 4096.0;
#endif
#ifdef USE_TONEMAP
// tonemap_mode is wired from r_tonemap->integer host-side. Mode 0
// disables the tonemap pipeline variant entirely (varIdx bit unset
// in vk.c), so this shader path only runs for modes 1..3.
//   1 = Reinhard, 2 = ACES filmic, 3 = Uncharted 2.
layout(constant_id = 17) const int tonemap_mode = 1;
// tonemap_exposure is wired from r_tonemapExposure->value host-side.
layout(constant_id = 18) const float tonemap_exposure = 1.0;
#endif
#ifdef USE_COLOR_GRADING
layout(constant_id = 19) const float cg_tint_r = 1.0;
layout(constant_id = 20) const float cg_tint_g = 1.0;
layout(constant_id = 21) const float cg_tint_b = 1.0;
layout(constant_id = 22) const float cg_saturation = 1.0;
layout(constant_id = 23) const float cg_contrast = 1.0;
#endif
#ifdef USE_FXAA
layout(constant_id = 24) const float fxaa_subpix = 0.75;
layout(constant_id = 25) const float fxaa_edgeThreshold = 0.125;
#endif
#ifdef USE_GODRAYS
layout(constant_id = 26) const int godrays_samples = 64;
layout(constant_id = 27) const float godrays_density = 1.0;
layout(push_constant) uniform GodRayParams {
	vec2 sunScreenPos;   // sun position in UV space (0-1)
	float intensity;     // ray brightness
	float decay;         // falloff per sample
} godray;
#endif

const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

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

#ifdef USE_SSAO
// Reconstruct linear depth from Z-buffer value
float linearDepth( float z ) {
	return zNear * zFar / ( zFar - z * ( zFar - zNear ) );
}

// Screen-space ambient occlusion — 8-sample hemisphere kernel
float computeSSAO() {
	vec2 texelSize = 1.0 / vec2( textureSize( depthMap, 0 ) );
	float centerDepth = linearDepth( texture( depthMap, frag_tex_coord ).r );

	// Poisson disc samples scaled by kernel radius (in texels)
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

	// Per-pixel random rotation from fragment position
	float angle = fract( sin( dot( gl_FragCoord.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
	float ca = cos( angle * 6.283 );
	float sa = sin( angle * 6.283 );
	mat2 rot = mat2( ca, sa, -sa, ca );

	float occlusion = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		vec2 offset = rot * samples[i] * radius * texelSize;
		float sampleDepth = linearDepth( texture( depthMap, frag_tex_coord + offset ).r );

		// Depth difference: positive = sample is behind center (occluded)
		float diff = centerDepth - sampleDepth;

		// Only count occlusion for nearby geometry (avoid halos at depth edges)
		float rangeCheck = smoothstep( 0.0, 1.0, radius * 2.0 / abs( diff ) );
		occlusion += step( 0.5, diff ) * rangeCheck;
	}

	return 1.0 - ( occlusion / 8.0 ) * ssao_intensity;
}
#endif

#ifdef USE_TONEMAP
// Reinhard tone mapping
vec3 tonemapReinhard( vec3 color ) {
	return color / ( 1.0 + color );
}

// ACES filmic (Krzysztof Narkowicz approximation)
vec3 tonemapACES( vec3 color ) {
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp( ( color * ( a * color + b ) ) / ( color * ( c * color + d ) + e ), 0.0, 1.0 );
}

// Uncharted 2 filmic (John Hable)
vec3 unchartedMap( vec3 x ) {
	const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ( ( x * ( A * x + C * B ) + D * E ) / ( x * ( A * x + B ) + D * F ) ) - E / F;
}
vec3 tonemapUncharted2( vec3 color ) {
	const float W = 11.2; // linear white point
	return unchartedMap( color ) / unchartedMap( vec3( W ) );
}

vec3 applyTonemap( vec3 color ) {
	color *= tonemap_exposure;
	if ( tonemap_mode == 2 )
		return tonemapACES( color );
	else if ( tonemap_mode == 3 )
		return tonemapUncharted2( color );
	else
		return tonemapReinhard( color );  // mode == 1
}
#endif

#ifdef USE_COLOR_GRADING
vec3 applyColorGrading( vec3 color ) {
	// Tint
	color *= vec3( cg_tint_r, cg_tint_g, cg_tint_b );

	// Saturation
	float luma = dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
	color = mix( vec3( luma ), color, cg_saturation );

	// Contrast (around midpoint 0.5)
	color = ( color - 0.5 ) * cg_contrast + 0.5;

	return clamp( color, 0.0, 1.0 );
}
#endif

#ifdef USE_GODRAYS
// Screen-space crepuscular rays via depth-based sky detection
vec3 computeGodRays() {
	vec2 deltaUV = ( frag_tex_coord - godray.sunScreenPos ) * godrays_density / float( godrays_samples );
	vec2 uv = frag_tex_coord;
	float illumination = 0.0;
	float weight = 1.0;

	for ( int i = 0; i < godrays_samples; i++ ) {
		uv -= deltaUV;

		// Clamp to screen bounds
		if ( uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 )
			break;

		// Sample depth — sky is at max depth (reversed depth: 0.0, normal: 1.0)
		float depth = texture( depthMap, uv ).r;
#ifdef USE_REVERSED_DEPTH
		float isSky = step( depth, 0.001 );
#else
		float isSky = step( 0.999, depth );
#endif

		// Accumulate sky samples with decay falloff
		illumination += isSky * weight;
		weight *= godray.decay;
	}

	illumination /= float( godrays_samples );
	return vec3( illumination * godray.intensity );
}
#endif

#ifdef USE_FXAA
vec3 applyFXAA() {
	vec2 texelSize = 1.0 / vec2( textureSize( texture0, 0 ) );
	const vec3 lumaW = vec3( 0.299, 0.587, 0.114 );

	vec3 rgbM  = texture( texture0, frag_tex_coord ).rgb;
	vec3 rgbNW = texture( texture0, frag_tex_coord + vec2( -1.0, -1.0 ) * texelSize ).rgb;
	vec3 rgbNE = texture( texture0, frag_tex_coord + vec2(  1.0, -1.0 ) * texelSize ).rgb;
	vec3 rgbSW = texture( texture0, frag_tex_coord + vec2( -1.0,  1.0 ) * texelSize ).rgb;
	vec3 rgbSE = texture( texture0, frag_tex_coord + vec2(  1.0,  1.0 ) * texelSize ).rgb;

	float lumaNW = dot( rgbNW, lumaW );
	float lumaNE = dot( rgbNE, lumaW );
	float lumaSW = dot( rgbSW, lumaW );
	float lumaSE = dot( rgbSE, lumaW );
	float lumaM  = dot( rgbM,  lumaW );

	float lumaMin = min( lumaM, min( min( lumaNW, lumaNE ), min( lumaSW, lumaSE ) ) );
	float lumaMax = max( lumaM, max( max( lumaNW, lumaNE ), max( lumaSW, lumaSE ) ) );
	float lumaRange = lumaMax - lumaMin;

	if ( lumaRange < max( 0.0625, lumaMax * fxaa_edgeThreshold ) )
		return rgbM;

	vec2 dir;
	dir.x = -( ( lumaNW + lumaNE ) - ( lumaSW + lumaSE ) );
	dir.y =  ( ( lumaNW + lumaSW ) - ( lumaNE + lumaSE ) );
	float dirReduce = max( ( lumaNW + lumaNE + lumaSW + lumaSE ) * 0.125, 1.0 / 128.0 );
	float rcpDirMin = 1.0 / ( min( abs( dir.x ), abs( dir.y ) ) + dirReduce );
	dir = clamp( dir * rcpDirMin, -8.0, 8.0 ) * texelSize;

	vec3 rgbA = 0.5 * (
		texture( texture0, frag_tex_coord + dir * ( 1.0 / 3.0 - 0.5 ) ).rgb +
		texture( texture0, frag_tex_coord + dir * ( 2.0 / 3.0 - 0.5 ) ).rgb );
	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		texture( texture0, frag_tex_coord - dir * 0.5 ).rgb +
		texture( texture0, frag_tex_coord + dir * 0.5 ).rgb );

	float lumaB = dot( rgbB, lumaW );
	vec3 result = ( lumaB < lumaMin || lumaB > lumaMax ) ? rgbA : rgbB;

	// Subpixel blending
	float lumaL = ( lumaNW + lumaNE + lumaSW + lumaSE ) * 0.25;
	float blendL = clamp( ( abs( lumaL - lumaM ) / lumaRange - 0.25 ) * ( 1.0 / 0.75 ), 0.0, 1.0 ) * fxaa_subpix;
	return mix( result, ( rgbNW + rgbNE + rgbSW + rgbSE + rgbM ) * 0.2, blendL );
}
#endif

void main() {
	// Sample color — FXAA replaces point sample with edge-aware multi-sample
#ifdef USE_FXAA
	vec3 base = applyFXAA();
#else
	vec3 base = texture(texture0, frag_tex_coord).rgb;
#endif

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
		//   saturation = 0 → luma   (grayscale)
		//   saturation = 1 → base   (identity, branch skipped above)
		//   saturation > 1 → luma + saturation * (base - luma)
		//                  → super-saturated; clamps on 8-bit fb.
		base = mix(luma, base, saturation);
	}

	if ( gamma != 1.0 )
	{
		out_color = vec4(pow(base, vec3(gamma)) * obScale, 1);
	}
	else
	{
		out_color = vec4(base * obScale, 1);
	}

	if ( ditherMode == 1 ) {
		out_color.rgb = dither(out_color.rgb);
	}
}
