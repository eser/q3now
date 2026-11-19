// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/ral/core/ral_exposure.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
	ralExposureHistoryStep_t step;
	step = Ral_ExposureHistoryStep(0, 0, 1000, 0);
	CHECK(step.reset && step.deltaSeconds == 0.0f);
	step = Ral_ExposureHistoryStep(1, 1000, 1050, 0);
	CHECK(!step.reset && fabsf(step.deltaSeconds - .05f) < .000001f);
	step = Ral_ExposureHistoryStep(1, 1000, 1000, 0);
	CHECK(!step.reset && step.deltaSeconds == 0.0f); /* pause */
	step = Ral_ExposureHistoryStep(1, 1000, 900, 0);
	CHECK(step.reset && step.deltaSeconds == 0.0f); /* rewind */
	step = Ral_ExposureHistoryStep(1, 1000, 1100, 1);
	CHECK(step.reset && step.deltaSeconds == 0.0f); /* cut/map/resize */
	step = Ral_ExposureHistoryStep(1, INT32_MIN, INT32_MAX, 0);
	CHECK(!step.reset && step.deltaSeconds == .1f); /* no signed overflow */
	step = Ral_ExposureHistoryStep(1, INT32_MAX, INT32_MIN, 0);
	CHECK(step.reset && step.deltaSeconds == 0.0f);
	/* Rejected plans do not publish a timestamp: retry uses committed time. */
	(void)Ral_ExposureHistoryStep(1, 1000, 1050, 0);
	step = Ral_ExposureHistoryStep(1, 1000, 1075, 0);
	CHECK(!step.reset && fabsf(step.deltaSeconds - .075f) < .000001f);
	puts("RAL exposure history contract: ok");
	return 0;
}
