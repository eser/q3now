// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// CSM PCF sun-shadow sampling, shared by the world-surface receiver (gen_frag)
// and the PMLIGHT lit pass (light_frag) via #include — single source of truth.
// Depends on the includer providing (both shaders do, at identical layout):
//   cascadeMVP[]   — per-cascade light MVP (uniform)
//   cascadeSplits  — view-depth split planes (uniform, vec4)
//   shadowMap      — sampler2DArray (set=2, binding=0)
//   shadow_pcf     — PCF tap count spec constant (1 / 5 / 9)
//   shadow_bias    — depth bias spec constant
float sampleCascade( int c, vec3 worldPos ) {
	vec4 sc4 = cascadeMVP[c] * vec4( worldPos, 1.0 );
	vec3 sc = sc4.xyz / sc4.w;        // orthographic — w == 1
	sc.xy = sc.xy * 0.5 + 0.5;        // NDC [-1,1] -> UV [0,1]; sc.z already in [0,1]
	if ( sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0 || sc.z > 1.0 )
		return 1.0;

	float layer = float( c );
	vec2 texelSize = 1.0 / vec2( textureSize( shadowMap, 0 ).xy );
	float currentDepth = sc.z - shadow_bias;
	float shadow = 0.0;

	if ( shadow_pcf <= 1 ) {
		shadow = step( currentDepth, texture( shadowMap, vec3( sc.xy, layer ) ).r );
	} else if ( shadow_pcf <= 5 ) {
		shadow += step( currentDepth, texture( shadowMap, vec3( sc.xy, layer ) ).r );
		shadow += step( currentDepth, texture( shadowMap, vec3( sc.xy + vec2( texelSize.x, 0 ), layer ) ).r );
		shadow += step( currentDepth, texture( shadowMap, vec3( sc.xy - vec2( texelSize.x, 0 ), layer ) ).r );
		shadow += step( currentDepth, texture( shadowMap, vec3( sc.xy + vec2( 0, texelSize.y ), layer ) ).r );
		shadow += step( currentDepth, texture( shadowMap, vec3( sc.xy - vec2( 0, texelSize.y ), layer ) ).r );
		shadow /= 5.0;
	} else {
		for ( int x = -1; x <= 1; x++ ) {
			for ( int y = -1; y <= 1; y++ ) {
				shadow += step( currentDepth, texture( shadowMap, vec3( sc.xy + vec2( x, y ) * texelSize, layer ) ).r );
			}
		}
		shadow /= 9.0;
	}
	return shadow;
}

//   worldPos  = object/world-space fragment position (shadowData.xyz)
//   viewDepth = view-space depth of the fragment    (shadowData.w)
//   outCascade = the cascade index that was selected (for r_csmShowCascades viz)
float sampleShadow( vec3 worldPos, float viewDepth, out int outCascade ) {
	// branchless cascade select: how many split planes is the fragment past?
	vec4 cmp = step( cascadeSplits, vec4( viewDepth ) );
	int cascade = min( int( cmp.x + cmp.y + cmp.z + cmp.w ), 3 );
	outCascade = cascade;

	// soft cascade boundary blend: as viewDepth approaches this cascade's far
	// split, lerp toward the next cascade. The next cascade (clamped to 3, and
	// for a disabled/cleared layer) reads as 1.0, so the blend hands off cleanly
	// even at the last enabled cascade. No-op away from boundaries.
	float prevSplit  = ( cascade == 0 ) ? 0.0 : cascadeSplits[max( cascade - 1, 0 )]; // max() guards a possible eager eval of the OOB index
	float farSplit   = cascadeSplits[cascade];
	float blendRange = max( 0.1 * ( farSplit - prevSplit ), 1.0 );
	float blendT     = clamp( ( farSplit - viewDepth ) / blendRange, 0.0, 1.0 ); // 1 = interior, 0 = at the far edge

	float s0 = sampleCascade( cascade, worldPos );
	float s1 = sampleCascade( min( cascade + 1, 3 ), worldPos );
	return mix( s1, s0, blendT );
}
