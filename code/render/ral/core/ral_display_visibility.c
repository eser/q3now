// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_display_visibility.h"

#include <math.h>
#include <string.h>

static float Lerp( float a, float b, float t ) {
	return a + ( b - a ) * t;
}

qboolean Ral_DisplayVisibilityPlanValid(
		const ralDisplayVisibilityPlan_t *plan ) {
	if ( !plan || plan->schemaVersion != RAL_DISPLAY_VISIBILITY_SCHEMA_VERSION
			|| !isfinite( plan->userBrightness )
			|| !isfinite( plan->exposureScale )
			|| !isfinite( plan->shadowExponent )
			|| !isfinite( plan->shadowPivot ) ) return qfalse;
	return plan->userBrightness >= RAL_DISPLAY_BRIGHTNESS_MIN
		&& plan->userBrightness <= RAL_DISPLAY_BRIGHTNESS_MAX
		&& plan->exposureScale >= 0.75f && plan->exposureScale <= 1.60f
		&& plan->shadowExponent >= 0.30f && plan->shadowExponent <= 1.35f
		&& plan->shadowPivot == RAL_DISPLAY_SHADOW_PIVOT ? qtrue : qfalse;
}

qboolean Ral_DisplayVisibilityPlanBuild( float userBrightness,
		ralDisplayVisibilityPlan_t *outPlan ) {
	ralDisplayVisibilityPlan_t plan;
	float t;
	if ( !outPlan || !isfinite( userBrightness )
			|| userBrightness < RAL_DISPLAY_BRIGHTNESS_MIN
			|| userBrightness > RAL_DISPLAY_BRIGHTNESS_MAX ) return qfalse;
	memset( &plan, 0, sizeof( plan ) );
	plan.schemaVersion = RAL_DISPLAY_VISIBILITY_SCHEMA_VERSION;
	plan.userBrightness = userBrightness;
	plan.shadowPivot = RAL_DISPLAY_SHADOW_PIVOT;
	if ( userBrightness <= RAL_DISPLAY_BRIGHTNESS_AUTHORED ) {
		t = userBrightness;
		plan.exposureScale = Lerp( 0.75f, 1.0f, t );
		plan.shadowExponent = Lerp( 1.35f, 1.0f, t );
	} else {
		/* Above identity, retain a continuous float scalar: logarithmic growth
		 * gives useful fine control near 1 while accepting values such as 5.2
		 * without snapping to modes or letting highlights grow unbounded. */
		t = log2f( userBrightness );
		plan.exposureScale = 1.0f + 0.12f * t;
		if ( plan.exposureScale > 1.60f ) plan.exposureScale = 1.60f;
		plan.shadowExponent = 1.0f / ( 1.0f + 0.35f * t );
		if ( plan.shadowExponent < 0.30f ) plan.shadowExponent = 0.30f;
	}
	if ( !Ral_DisplayVisibilityPlanValid( &plan ) ) return qfalse;
	*outPlan = plan;
	return qtrue;
}

qboolean Ral_DisplayVisibilityApplyToe(
		const ralDisplayVisibilityPlan_t *plan, float exposedLuminance,
		float *outLuminance ) {
	float result;
	if ( !Ral_DisplayVisibilityPlanValid( plan ) || !outLuminance
			|| !isfinite( exposedLuminance ) || exposedLuminance < 0.0f )
		return qfalse;
	result = exposedLuminance;
	if ( exposedLuminance > 0.0f && exposedLuminance < plan->shadowPivot ) {
		result = powf( exposedLuminance / plan->shadowPivot,
			plan->shadowExponent ) * plan->shadowPivot;
	}
	if ( !isfinite( result ) || result < 0.0f ) return qfalse;
	*outLuminance = result;
	return qtrue;
}

qboolean Ral_DisplayVisibilityComposeExposure(
		const ralDisplayVisibilityPlan_t *plan, float adaptedExposure,
		float *outExposure ) {
	float result;
	if ( !Ral_DisplayVisibilityPlanValid( plan ) || !outExposure
			|| !isfinite( adaptedExposure ) || adaptedExposure < 0.0f )
		return qfalse;
	result = adaptedExposure * plan->exposureScale;
	if ( !isfinite( result ) || result < 0.0f ) return qfalse;
	*outExposure = result;
	return qtrue;
}

qboolean Ral_DisplayVisibilityApplyRgb(
		const ralDisplayVisibilityPlan_t *plan, const float exposedRgb[3],
		float outRgb[3] ) {
	static const float luminanceWeights[3] = { 0.2126f, 0.7152f, 0.0722f };
	float luminance = 0.0f, curved, scale = 1.0f, result[3];
	uint32_t channel;
	if ( !Ral_DisplayVisibilityPlanValid( plan ) || !exposedRgb || !outRgb )
		return qfalse;
	for ( channel = 0u; channel < 3u; ++channel ) {
		if ( !isfinite( exposedRgb[channel] ) || exposedRgb[channel] < 0.0f )
			return qfalse;
		luminance += exposedRgb[channel] * luminanceWeights[channel];
	}
	if ( !Ral_DisplayVisibilityApplyToe( plan, luminance, &curved ) )
		return qfalse;
	if ( luminance > 0.0f ) scale = curved / luminance;
	for ( channel = 0u; channel < 3u; ++channel ) {
		result[channel] = exposedRgb[channel] * scale;
		if ( !isfinite( result[channel] ) || result[channel] < 0.0f )
			return qfalse;
	}
	memcpy( outRgb, result, sizeof( result ) );
	return qtrue;
}
