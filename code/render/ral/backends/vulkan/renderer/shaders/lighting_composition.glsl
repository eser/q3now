// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

// Backend-portable lighting decomposition. This file owns arithmetic only:
// descriptor/resource selection remains in the RAL backend and the CPU-side
// composition receipt decides which authority and terms are legal.

#define WIRED_LIGHTING_TERM_DYNAMIC_DIRECT  (1 << 0)
#define WIRED_LIGHTING_TERM_STATIC_INDIRECT (1 << 1)
#define WIRED_LIGHTING_TERM_LOCAL_SH        (1 << 2)
#define WIRED_LIGHTING_TERM_SPECULAR_IBL    (1 << 3)

#define WIRED_LIGHTING_DEBUG_FINAL           0
#define WIRED_LIGHTING_DEBUG_DYNAMIC_DIRECT  1
#define WIRED_LIGHTING_DEBUG_STATIC_INDIRECT 2
#define WIRED_LIGHTING_DEBUG_LOCAL_SH        3
#define WIRED_LIGHTING_DEBUG_SPECULAR_IBL    4

vec3 wired_lighting_decode_oct( vec2 encoded ) {
	vec2 f = encoded * 2.0 - 1.0;
	vec3 n = vec3( f, 1.0 - abs( f.x ) - abs( f.y ) );
	if ( n.z < 0.0 ) {
		vec2 folded = ( 1.0 - abs( n.yx ) ) * sign( n.xy );
		n.xy = folded;
	}
	return normalize( n );
}

// The directional product is diffuse indirect irradiance only. Material-normal
// response happens here; final direct light and specular never enter this value.
vec3 wired_lighting_directional_static_indirect( vec3 radiance,
		vec2 encodedDirection, vec3 materialNormal ) {
	vec3 dominantDirection = wired_lighting_decode_oct( encodedDirection );
	return max( radiance, vec3( 0.0 ) )
		* max( dot( normalize( materialNormal ), dominantDirection ), 0.0 );
}

vec3 wired_lighting_compose( vec3 dynamicDirect, vec3 staticIndirect,
		vec3 localSh, vec3 specularIbl, int activeTermMask, int debugView ) {
	if ( debugView == WIRED_LIGHTING_DEBUG_DYNAMIC_DIRECT ) return dynamicDirect;
	if ( debugView == WIRED_LIGHTING_DEBUG_STATIC_INDIRECT ) return staticIndirect;
	if ( debugView == WIRED_LIGHTING_DEBUG_LOCAL_SH ) return localSh;
	if ( debugView == WIRED_LIGHTING_DEBUG_SPECULAR_IBL ) return specularIbl;
	vec3 result = vec3( 0.0 );
	if ( ( activeTermMask & WIRED_LIGHTING_TERM_DYNAMIC_DIRECT ) != 0 )
		result += dynamicDirect;
	if ( ( activeTermMask & WIRED_LIGHTING_TERM_STATIC_INDIRECT ) != 0 )
		result += staticIndirect;
	if ( ( activeTermMask & WIRED_LIGHTING_TERM_LOCAL_SH ) != 0 )
		result += localSh;
	if ( ( activeTermMask & WIRED_LIGHTING_TERM_SPECULAR_IBL ) != 0 )
		result += specularIbl;
	return result;
}
