// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_PROFILE_IMGUI_H
#define WIRED_PROFILE_IMGUI_H

#include <stdint.h>

#include "ral_profile.h"

typedef struct {
	uint32_t topologyEpoch;
	uint32_t laneRows;
	uint32_t plottedLanes;
	uint32_t plottedSamples;
} wiredProfileImGuiReceipt_t;

// Draw the tool-neutral RAL snapshot and frame-major history into the current
// Dear ImGui frame. This draw adapter owns no window or renderer backend and is
// therefore reusable by the future editor/debug host; SDL3 event translation
// lives in its sibling adapter. Invalid input leaves `receipt` untouched and
// returns zero.
int WiredProfileImGui_Draw( const ralProfileSnapshot_t *snapshot,
		const double *historyMs, uint32_t historyFrames,
		wiredProfileImGuiReceipt_t *receipt );

#endif // WIRED_PROFILE_IMGUI_H
