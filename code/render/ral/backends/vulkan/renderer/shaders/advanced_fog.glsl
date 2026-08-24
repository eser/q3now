// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Portable advanced-distance-fog model shared by the base, additive-light,
// and Forward+ fragment paths. gl_FragCoord.w is reciprocal clip W after
// interpolation; Wired's perspective projection makes its reciprocal the
// positive fragment view depth without another varying or backend-specific
// reconstruction.
bool wired_advanced_fog_enabled() {
	int fogType = int( advancedFogTypeFarEnabled.x + 0.5 );
	return advancedFogTypeFarEnabled.z > 0.5 && fogType >= 1 && fogType <= 3;
}

float wired_advanced_fog_amount() {
	if ( !wired_advanced_fog_enabled() )
		return 0.0;

	float viewDepth = 1.0 / max( gl_FragCoord.w, 0.000001 );
	int fogType = int( advancedFogTypeFarEnabled.x + 0.5 );

	// FT_LINEAR: Spearmint's fixed-function path uses start=0, end=farClip.
	if ( fogType == 1 ) {
		if ( advancedFogTypeFarEnabled.y <= 0.0 )
			return 0.0;
		return clamp( viewDepth / advancedFogTypeFarEnabled.y, 0.0, 1.0 );
	}

	float opticalDepth = max( advancedFogColorDensity.w, 0.0 ) * viewDepth;
	if ( fogType == 2 ) // FT_EXP
		return clamp( 1.0 - exp( -opticalDepth ), 0.0, 1.0 );

	// FT_EXP2
	return clamp( 1.0 - exp( -( opticalDepth * opticalDepth ) ), 0.0, 1.0 );
}
