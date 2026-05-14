// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Advanced water fragment shader — screen-space refraction + Fresnel + ripples
//
// Samples the screenMap (pre-rendered scene) with distorted UVs for refraction.
// Applies Fresnel-based blend between refraction and water surface color.
// Spatial ripple distortion from fractal noise — temporal animation comes from
// vertex deformation (deformVertexes wave) which shifts texture coordinates.

layout(set = 0, binding = 0) uniform UBO {
	vec4 eyePos;
	vec4 lightPos;
	vec4 lightColor;       // rgb + 1/(r*r)
	vec4 lightVector;      // linear dynamic light
};

layout(set = 1, binding = 0) uniform sampler2D texture0;    // water surface texture
layout(set = 2, binding = 0) uniform sampler2D screenTex;   // scene behind water (screenMap or lightmap slot)

layout(location = 0) centroid in vec2 frag_tex_coord;
layout(location = 1) in vec3 N;   // surface normal
layout(location = 2) in vec4 L;   // light vector
layout(location = 3) in vec4 V;   // view vector

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const int alpha_test_func = 0;
layout(constant_id = 1) const float alpha_test_value = 0.0;
layout(constant_id = 3) const int alpha_to_coverage = 0;
// Block 5d: per-slot colour-domain bitmask (shared spec-constant id 4 with
// gen_frag.tmpl / light_frag.tmpl). Bit 0 = the water-surface texture
// (texture0) is CD_LINEAR → raw fetch; otherwise CD_SRGB → decode. screenTex
// (set 2) is the screenmap render-pass output — always linear-radiance, so
// it is never decoded regardless of the bitmask.
layout(constant_id = 4) const int tex_domain = 0;
layout(constant_id = 5) const int abs_light = 0;
layout(constant_id = 12) const float distortion_scale = 0.02;
layout(constant_id = 13) const float tint_strength = 0.25;
layout(constant_id = 14) const float specular_power = 32.0;

// Phase 6B3'-d4-m9: precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches m1..m8 verbatim. linearToSRGB is unused here —
// driver DCEs it; kept for migration symmetry.
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

// Simple hash noise
float hash( vec2 p ) {
	return fract( sin( dot( p, vec2( 127.1, 311.7 ) ) ) * 43758.5453 );
}

float noise( vec2 p ) {
	vec2 i = floor( p );
	vec2 f = fract( p );
	f = f * f * ( 3.0 - 2.0 * f );
	return mix(
		mix( hash( i ), hash( i + vec2( 1, 0 ) ), f.x ),
		mix( hash( i + vec2( 0, 1 ) ), hash( i + vec2( 1, 1 ) ), f.x ),
		f.y );
}

// Two-octave fractal noise for ripple detail
vec2 rippleNoise( vec2 uv ) {
	vec2 n;
	n.x = noise( uv * 6.0 ) * 0.6 + noise( uv * 12.0 ) * 0.3;
	n.y = noise( uv * 6.0 + 43.0 ) * 0.6 + noise( uv * 12.0 + 43.0 ) * 0.3;
	return ( n - 0.5 ) * 2.0; // remap [-1, 1]
}

void main() {
	// Phase 6B3'-d4-m9: linear-domain water contract.
	//   - texture0 (water surface texture): sRGB-encoded colour —
	//     decoded below into `waterColor` before any blend.
	//   - screenTex (set 2): the screenmap render-pass output; gen_frag.tmpl
	//     emits linear (m8 — no special-case gamma write), so this sample
	//     is already linear-radiance and is NOT decoded.
	//   - lightColor / lightVector / lightPos / eyePos (UBO): declared for
	//     std140 layout parity with vkUniform_t.light; this shader does not
	//     consume them — nothing to decode here (lightColor is host-decoded
	//     at VK_SetLightParams anyway, m7).
	//   - N / L / V: geometry vectors, not colour.
	//   - distortion_scale / tint_strength / specular_power: scalars, not
	//     colour. The Fresnel / refraction-mix / specular math is unchanged
	//     and now operates on linear-radiance inputs, emitting linear output.

	// Screen-space UV from fragment position
	ivec2 screenSize = textureSize( screenTex, 0 );
	vec2 screenUV = gl_FragCoord.xy / vec2( screenSize );

	// Ripple distortion based on surface texture coords
	// (temporal variation comes from vertex deformation shifting frag_tex_coord)
	vec2 ripple = rippleNoise( frag_tex_coord );

	// Distort screen UV for refraction
	vec2 refractionUV = clamp( screenUV + ripple * distortion_scale, 0.001, 0.999 );

	// Sample refracted scene
	vec3 refraction = texture( screenTex, refractionUV ).rgb;

	// Sample water surface texture — Block 5d domain-aware (CD_SRGB decoded,
	// CD_LINEAR raw). The screenTex sample above is always raw (linear FB).
	vec4 waterTex = texture( texture0, frag_tex_coord );
	vec3 waterColor = ( ( tex_domain & 1 ) != 0 ) ? waterTex.rgb : sRGBToLinear( waterTex.rgb );

	// Fresnel — more reflective at glancing angles
	vec3 nV = normalize( V.xyz );
	float fresnel = 1.0 - abs( dot( normalize( N ), nV ) );
	fresnel = fresnel * fresnel; // pow2

	// Blend refraction with water tint
	vec3 tinted = mix( refraction, waterColor, tint_strength );

	// Fresnel: at steep angles see refraction, glancing angles see more surface color
	vec3 finalColor = mix( tinted, waterColor * 1.3, fresnel * 0.6 );

	// Specular highlight from light
	vec3 nL = normalize( L.xyz );
	vec3 halfVec = normalize( nL + nV );
	float spec = pow( max( dot( normalize( N ), halfVec ), 0.0 ), specular_power );
	finalColor += vec3( spec * 0.25 );

	out_color = vec4( finalColor, waterTex.a );
}
