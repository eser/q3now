// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Image-based-lighting (split-sum, Karis 2013) AMBIENT helpers — the light-
// INDEPENDENT environment term that belongs in the base pass (always-on), not in
// any per-dynamic-light pass. Binding-agnostic: the split-sum assembly takes the
// IBL samplers as parameters, so this file declares no descriptors (the consumer
// — the base pass — owns the set-2 bindings). Self-contained (uses only GLSL
// builtins + the roughness-Fresnel below); it does NOT include pbr.glsl, since
// compile.mjs #include is single-level — a consumer that wants the direct-light
// BRDF includes pbr.glsl itself, alongside this.
//
// WGSL/WebGPU-translatable (W-48): the cube/LUT samples use textureLod (explicit
// LOD), never implicit-LOD texture(); no dFdx/dFdy in conditional control flow.

// Roughness-aware Schlick for the IBL ambient specular: the max(1-roughness)
// term keeps a rough surface's grazing edges from over-brightening (a plain
// FresnelSchlick drives F->1 at the edge, which on a pre-integrated environment
// reads as a hard rim). Ambient-only; the direct BRDF uses FresnelSchlick.
vec3 fresnelSchlickRoughness( float cosTheta, vec3 F0, float roughness ) {
	float t = 1.0 - cosTheta;
	float t2 = t * t;
	vec3  Fmax = max( vec3( 1.0 - roughness ), F0 );
	return F0 + ( Fmax - F0 ) * ( t2 * t2 * t ); // pow5
}

// Assemble the split-sum IBL ambient for one fragment. Binding-agnostic — the
// caller passes the three IBL samplers + the surface terms:
//   N            surface normal (world/object space, normalized)
//   V            view vector (fragment -> eye, normalized)
//   albedo       base colour (linear)
//   roughness    [0.04,1]   metalness [0,1]   ao [0,1]
//   irradiance   diffuse irradiance cube (cosine-convolved env)
//   radiance     prefiltered specular cube (mip = roughness)
//   brdfLut      2D split-sum env-BRDF LUT (R = F0 scale, G = bias)
//   maxRadianceLod  highest radiance mip index (radiance mip count - 1)
// Returns the linear-HDR ambient radiance to add to out_color.
vec3 iblAmbient( vec3 N, vec3 V, vec3 albedo, float roughness, float metalness, float ao,
                 samplerCube irradiance, samplerCube radiance, sampler2D brdfLut,
                 float maxRadianceLod )
{
	vec3 F0    = mix( vec3( 0.04 ), albedo, metalness );
	float NdotV = max( dot( N, V ), 0.0 );

	vec3 F_amb = fresnelSchlickRoughness( NdotV, F0, roughness );
	vec3 kD    = ( vec3( 1.0 ) - F_amb ) * ( 1.0 - metalness );

	// Diffuse irradiance along the normal.
	vec3 diffuseIBL = textureLod( irradiance, N, 0.0 ).rgb * albedo;

	// Prefiltered specular along the reflection vector; roughness picks the mip.
	vec3 R = reflect( -V, N );
	vec3 prefiltered = textureLod( radiance, R, roughness * maxRadianceLod ).rgb;
	vec2 envBRDF = textureLod( brdfLut, vec2( NdotV, roughness ), 0.0 ).rg;
	vec3 specularIBL = prefiltered * ( F_amb * envBRDF.x + envBRDF.y );

	return ( kD * diffuseIBL + specularIBL ) * ao;
}
