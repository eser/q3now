// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_DISPLAY_VISIBILITY_H
#define WIRED_RAL_DISPLAY_VISIBILITY_H

#include "ral.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_DISPLAY_VISIBILITY_SCHEMA_VERSION 1u
#define RAL_DISPLAY_BRIGHTNESS_MIN 0.0f
#define RAL_DISPLAY_BRIGHTNESS_AUTHORED 1.0f
#define RAL_DISPLAY_BRIGHTNESS_MAX 32.0f
#define RAL_DISPLAY_SHADOW_PIVOT 0.25f

typedef struct {
	uint32_t schemaVersion;
	float userBrightness;
	float exposureScale;
	float shadowExponent;
	float shadowPivot;
} ralDisplayVisibilityPlan_t;

qboolean Ral_DisplayVisibilityPlanBuild( float userBrightness,
	ralDisplayVisibilityPlan_t *outPlan );
qboolean Ral_DisplayVisibilityPlanValid(
	const ralDisplayVisibilityPlan_t *plan );
qboolean Ral_DisplayVisibilityApplyToe(
	const ralDisplayVisibilityPlan_t *plan, float exposedLuminance,
	float *outLuminance );
qboolean Ral_DisplayVisibilityComposeExposure(
	const ralDisplayVisibilityPlan_t *plan, float adaptedExposure,
	float *outExposure );
qboolean Ral_DisplayVisibilityApplyRgb(
	const ralDisplayVisibilityPlan_t *plan, const float exposedRgb[3],
	float outRgb[3] );

#ifdef __cplusplus
}
#endif

#endif
