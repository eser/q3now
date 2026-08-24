// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Direct-light Cook-Torrance BRDF helpers (GGX + Smith + Schlick) — pure math,
// no bindings / varyings. Shared by the PMLIGHT lit pass (light_frag) via
// #include, and the basis ibl.glsl builds the ambient term on.
const float PI = 3.14159265359;

float DistributionGGX( float NdotH, float roughness ) {
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	return a2 / ( PI * denom * denom );
}

float GeometrySchlickGGX( float NdotV, float roughness ) {
	float r = roughness + 1.0;
	float k = ( r * r ) / 8.0;
	return NdotV / ( NdotV * ( 1.0 - k ) + k );
}

float GeometrySmith( float NdotV, float NdotL, float roughness ) {
	return GeometrySchlickGGX( NdotV, roughness ) * GeometrySchlickGGX( NdotL, roughness );
}

vec3 FresnelSchlick( float cosTheta, vec3 F0 ) {
	float t = 1.0 - cosTheta;
	float t2 = t * t;
	return F0 + ( 1.0 - F0 ) * ( t2 * t2 * t ); // pow5
}
