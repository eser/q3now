// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_EXPOSURE_H
#define WIRED_RAL_EXPOSURE_H
#include <math.h>
#include <stdint.h>

/* Canonical exposure.comp / tonemap.frag storage layout. No native handles. */
typedef struct {
	float exposure_bias, key, pctLow, pctHigh;
	float rateUp, rateDown, minExp, maxExp;
	int32_t autoEnabled;
	float brightness, sunScreenX, sunScreenY;
	float sunrayIntensity, sunrayDecay, shadowExponent, shadowPivot;
} ralExposureBlock_t;

typedef struct {
	int32_t enabled;
	float key, pctLow, pctHigh, rateUp, rateDown, minExp, maxExp;
} ralExposureSettings_t;

typedef struct {
	int reset;
	float deltaSeconds;
} ralExposureHistoryStep_t;

/* Use scene time, not wall time: paused scenes must not adapt. Owners supply
 * discontinuities (world/extent/device/AE changes and explicit camera cuts).
 * Planning does not publish history; only successfully submitted work may
 * commit its timestamp. A detected cut must remain invalid after cancellation. */
static inline ralExposureHistoryStep_t Ral_ExposureHistoryStep(
		int historyValid, int32_t previousTimeMs, int32_t sceneTimeMs,
		int discontinuity ) {
	ralExposureHistoryStep_t step;
	step.reset = !historyValid || discontinuity || sceneTimeMs < previousTimeMs;
	step.deltaSeconds = step.reset ? 0.0f : fminf( 0.1f,
		(float)((int64_t)sceneTimeMs - previousTimeMs) * 0.001f );
	return step;
}

static inline int Ral_ExposureSettingsValid( const ralExposureSettings_t *s ) {
	return s && ( s->enabled == 0 || s->enabled == 1 )
		&& isfinite(s->key) && s->key >= 0.01f && s->key <= 1.0f
		&& isfinite(s->pctLow) && s->pctLow >= 0.0f && s->pctLow <= 1.0f
		&& isfinite(s->pctHigh) && s->pctHigh >= 0.0f && s->pctHigh <= 1.0f
		&& isfinite(s->rateUp) && s->rateUp >= 0.0f && s->rateUp <= 100.0f
		&& isfinite(s->rateDown) && s->rateDown >= 0.0f && s->rateDown <= 100.0f
		&& isfinite(s->minExp) && s->minExp >= 0.01f && s->minExp <= 1.0f
		&& isfinite(s->maxExp) && s->maxExp >= 1.0f && s->maxExp <= 64.0f;
}
#endif
