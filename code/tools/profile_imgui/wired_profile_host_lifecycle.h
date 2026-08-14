// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_PROFILE_HOST_LIFECYCLE_H
#define WIRED_PROFILE_HOST_LIFECYCLE_H

#include "ral_types.h"

#include <cstdint>

enum wiredProfileHostState_t {
	WIRED_PROFILE_HOST_NO_SWAPCHAIN,
	WIRED_PROFILE_HOST_RECREATE_PENDING,
	WIRED_PROFILE_HOST_ACTIVE,
	WIRED_PROFILE_HOST_SUSPENDED,
	WIRED_PROFILE_HOST_FATAL,
	WIRED_PROFILE_HOST_QUIT
};

enum wiredProfileHostFrameAction_t {
	WIRED_PROFILE_HOST_FRAME_SKIP,
	WIRED_PROFILE_HOST_FRAME_RENDER,
	WIRED_PROFILE_HOST_FRAME_RENDER_THEN_RECREATE,
	WIRED_PROFILE_HOST_FRAME_RECREATE,
	WIRED_PROFILE_HOST_FRAME_SURFACE_LOST,
	WIRED_PROFILE_HOST_FRAME_FATAL
};

struct wiredProfileHostLifecycle_t {
	wiredProfileHostState_t state;
	std::uint32_t pixelWidth;
	std::uint32_t pixelHeight;
	std::uint64_t generation;
	bool recreatePending;
};

void WiredProfileHostLifecycle_Init( wiredProfileHostLifecycle_t *lifecycle );
void WiredProfileHostLifecycle_Pixels( wiredProfileHostLifecycle_t *lifecycle,
	std::uint32_t width, std::uint32_t height );
void WiredProfileHostLifecycle_Minimized( wiredProfileHostLifecycle_t *lifecycle );
void WiredProfileHostLifecycle_Restored( wiredProfileHostLifecycle_t *lifecycle,
	std::uint32_t width, std::uint32_t height );
void WiredProfileHostLifecycle_RecreateSucceeded( wiredProfileHostLifecycle_t *lifecycle,
	std::uint64_t generation );
void WiredProfileHostLifecycle_RecreateFailed( wiredProfileHostLifecycle_t *lifecycle,
	ralResult_t result, bool oldGenerationRetained );
wiredProfileHostFrameAction_t WiredProfileHostLifecycle_AcquireResult(
	const wiredProfileHostLifecycle_t *lifecycle, ralResult_t result, bool hasImage );
wiredProfileHostFrameAction_t WiredProfileHostLifecycle_PresentResult(
	const wiredProfileHostLifecycle_t *lifecycle, ralResult_t result );

#endif
