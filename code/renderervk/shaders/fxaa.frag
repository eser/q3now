#version 450
// FXAA 3.11 — Fast Approximate Anti-Aliasing
// Based on Timothy Lottes' FXAA algorithm (NVIDIA)
// Adapted for Vulkan/SPIR-V from FTEQW's implementation

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float fxaa_subpix = 0.75;     // subpixel AA amount (0=off, 1=full)
layout(constant_id = 1) const float fxaa_edgeThreshold = 0.125;
layout(constant_id = 2) const float fxaa_edgeThresholdMin = 0.0625;

const vec3 luma = vec3( 0.299, 0.587, 0.114 );

void main() {
	vec2 texelSize = 1.0 / vec2( textureSize( texture0, 0 ) );

	// Sample center and 4 neighbors
	vec3 rgbM  = texture( texture0, frag_tex_coord ).rgb;
	vec3 rgbNW = texture( texture0, frag_tex_coord + vec2( -1.0, -1.0 ) * texelSize ).rgb;
	vec3 rgbNE = texture( texture0, frag_tex_coord + vec2(  1.0, -1.0 ) * texelSize ).rgb;
	vec3 rgbSW = texture( texture0, frag_tex_coord + vec2( -1.0,  1.0 ) * texelSize ).rgb;
	vec3 rgbSE = texture( texture0, frag_tex_coord + vec2(  1.0,  1.0 ) * texelSize ).rgb;

	float lumaNW = dot( rgbNW, luma );
	float lumaNE = dot( rgbNE, luma );
	float lumaSW = dot( rgbSW, luma );
	float lumaSE = dot( rgbSE, luma );
	float lumaM  = dot( rgbM,  luma );

	float lumaMin = min( lumaM, min( min( lumaNW, lumaNE ), min( lumaSW, lumaSE ) ) );
	float lumaMax = max( lumaM, max( max( lumaNW, lumaNE ), max( lumaSW, lumaSE ) ) );
	float lumaRange = lumaMax - lumaMin;

	// Early exit for low-contrast areas
	if ( lumaRange < max( fxaa_edgeThresholdMin, lumaMax * fxaa_edgeThreshold ) ) {
		out_color = vec4( rgbM, 1.0 );
		return;
	}

	// Compute edge direction
	vec2 dir;
	dir.x = -( ( lumaNW + lumaNE ) - ( lumaSW + lumaSE ) );
	dir.y =  ( ( lumaNW + lumaSW ) - ( lumaNE + lumaSE ) );

	float dirReduce = max( ( lumaNW + lumaNE + lumaSW + lumaSE ) * ( 0.25 * 0.5 ), 1.0 / 128.0 );
	float rcpDirMin = 1.0 / ( min( abs( dir.x ), abs( dir.y ) ) + dirReduce );
	dir = min( vec2( 8.0 ), max( vec2( -8.0 ), dir * rcpDirMin ) ) * texelSize;

	// Sample along the detected edge
	vec3 rgbA = 0.5 * (
		texture( texture0, frag_tex_coord + dir * ( 1.0 / 3.0 - 0.5 ) ).rgb +
		texture( texture0, frag_tex_coord + dir * ( 2.0 / 3.0 - 0.5 ) ).rgb );

	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		texture( texture0, frag_tex_coord + dir * ( 0.0 / 3.0 - 0.5 ) ).rgb +
		texture( texture0, frag_tex_coord + dir * ( 3.0 / 3.0 - 0.5 ) ).rgb );

	float lumaB = dot( rgbB, luma );

	// Use wider sample if it doesn't go outside the luma range
	if ( lumaB < lumaMin || lumaB > lumaMax ) {
		out_color = vec4( rgbA, 1.0 );
	} else {
		out_color = vec4( rgbB, 1.0 );
	}

	// Subpixel aliasing reduction
	float lumaL = ( lumaNW + lumaNE + lumaSW + lumaSE ) * 0.25;
	float rangeL = abs( lumaL - lumaM );
	float blendL = max( 0.0, ( rangeL / lumaRange ) - 0.25 ) * ( 1.0 / 0.75 );
	blendL = min( 1.0, blendL ) * fxaa_subpix;

	vec3 rgbL = ( rgbNW + rgbNE + rgbSW + rgbSE + rgbM ) * 0.2;
	out_color.rgb = mix( out_color.rgb, rgbL, blendL );
}
