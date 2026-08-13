// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_PROFILE_IMGUI_SDL3_H
#define WIRED_PROFILE_IMGUI_SDL3_H

#include <stdint.h>

union SDL_Event;

typedef struct {
	uint32_t handledEvents;
	uint32_t keyboardEvents;
	uint32_t textEvents;
	uint32_t mouseEvents;
	uint32_t focusEvents;
} wiredProfileImGuiSdl3Receipt_t;

// Translate one real SDL3 event into the current Dear ImGui IO queue. This
// adapter creates no SDL window and owns no renderer resources. Unsupported or
// malformed events leave `receipt` untouched and return zero.
int WiredProfileImGuiSdl3_ProcessEvent( const union SDL_Event *event,
		wiredProfileImGuiSdl3Receipt_t *receipt );

#endif // WIRED_PROFILE_IMGUI_SDL3_H
