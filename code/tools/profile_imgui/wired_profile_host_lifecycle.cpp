// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_host_lifecycle.h"

void WiredProfileHostLifecycle_Init( wiredProfileHostLifecycle_t *lifecycle ) {
	if ( !lifecycle ) return;
	*lifecycle = {};
	lifecycle->state = WIRED_PROFILE_HOST_NO_SWAPCHAIN;
}

void WiredProfileHostLifecycle_Pixels( wiredProfileHostLifecycle_t *lifecycle,
	std::uint32_t width, std::uint32_t height ) {
	if ( !lifecycle || lifecycle->state == WIRED_PROFILE_HOST_FATAL
	  || lifecycle->state == WIRED_PROFILE_HOST_QUIT ) return;
	lifecycle->pixelWidth = width;
	lifecycle->pixelHeight = height;
	if ( width == 0 || height == 0 ) {
		lifecycle->state = WIRED_PROFILE_HOST_SUSPENDED;
		lifecycle->recreatePending = false;
		return;
	}
	lifecycle->state = WIRED_PROFILE_HOST_RECREATE_PENDING;
	lifecycle->recreatePending = true;
}

void WiredProfileHostLifecycle_Minimized( wiredProfileHostLifecycle_t *lifecycle ) {
	if ( !lifecycle || lifecycle->state == WIRED_PROFILE_HOST_FATAL
	  || lifecycle->state == WIRED_PROFILE_HOST_QUIT ) return;
	lifecycle->state = WIRED_PROFILE_HOST_SUSPENDED;
	lifecycle->recreatePending = false;
}

void WiredProfileHostLifecycle_Restored( wiredProfileHostLifecycle_t *lifecycle,
	std::uint32_t width, std::uint32_t height ) {
	WiredProfileHostLifecycle_Pixels( lifecycle, width, height );
}

void WiredProfileHostLifecycle_RecreateSucceeded( wiredProfileHostLifecycle_t *lifecycle,
	std::uint64_t generation ) {
	if ( !lifecycle || generation == 0 || lifecycle->pixelWidth == 0
	  || lifecycle->pixelHeight == 0 ) return;
	lifecycle->generation = generation;
	lifecycle->recreatePending = false;
	lifecycle->state = WIRED_PROFILE_HOST_ACTIVE;
}

void WiredProfileHostLifecycle_RecreateFailed( wiredProfileHostLifecycle_t *lifecycle,
	ralResult_t result, bool oldGenerationRetained ) {
	if ( !lifecycle ) return;
	if ( result == ralSurfaceLost ) {
		lifecycle->state = WIRED_PROFILE_HOST_FATAL;
		lifecycle->recreatePending = false;
		return;
	}
	if ( result == ralErrorDeviceLost ) {
		lifecycle->state = WIRED_PROFILE_HOST_FATAL;
		lifecycle->recreatePending = false;
		return;
	}
	if ( oldGenerationRetained && lifecycle->generation != 0 ) {
		lifecycle->state = WIRED_PROFILE_HOST_ACTIVE;
		lifecycle->recreatePending = true;
		return;
	}
	lifecycle->generation = 0;
	lifecycle->state = ( result == ralOutOfDate || result == ralSuboptimal )
		? WIRED_PROFILE_HOST_RECREATE_PENDING : WIRED_PROFILE_HOST_FATAL;
	lifecycle->recreatePending = lifecycle->state == WIRED_PROFILE_HOST_RECREATE_PENDING;
}

wiredProfileHostFrameAction_t WiredProfileHostLifecycle_AcquireResult(
	const wiredProfileHostLifecycle_t *lifecycle, ralResult_t result, bool hasImage ) {
	if ( !lifecycle || lifecycle->state != WIRED_PROFILE_HOST_ACTIVE )
		return WIRED_PROFILE_HOST_FRAME_SKIP;
	if ( result == ralSuccess )
		return hasImage ? WIRED_PROFILE_HOST_FRAME_RENDER : WIRED_PROFILE_HOST_FRAME_FATAL;
	if ( result == ralSuboptimal )
		return hasImage ? WIRED_PROFILE_HOST_FRAME_RENDER_THEN_RECREATE : WIRED_PROFILE_HOST_FRAME_FATAL;
	if ( result == ralTimeout ) return WIRED_PROFILE_HOST_FRAME_SKIP;
	if ( result == ralOutOfDate ) return WIRED_PROFILE_HOST_FRAME_RECREATE;
	if ( result == ralSurfaceLost ) return WIRED_PROFILE_HOST_FRAME_SURFACE_LOST;
	return WIRED_PROFILE_HOST_FRAME_FATAL;
}

wiredProfileHostFrameAction_t WiredProfileHostLifecycle_PresentResult(
	const wiredProfileHostLifecycle_t *lifecycle, ralResult_t result ) {
	if ( !lifecycle || lifecycle->state != WIRED_PROFILE_HOST_ACTIVE )
		return WIRED_PROFILE_HOST_FRAME_SKIP;
	if ( result == ralSuccess ) return WIRED_PROFILE_HOST_FRAME_RENDER;
	if ( result == ralSuboptimal || result == ralOutOfDate )
		return WIRED_PROFILE_HOST_FRAME_RECREATE;
	if ( result == ralSurfaceLost ) return WIRED_PROFILE_HOST_FRAME_SURFACE_LOST;
	return WIRED_PROFILE_HOST_FRAME_FATAL;
}
