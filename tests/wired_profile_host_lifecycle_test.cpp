// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_host_lifecycle.h"

#include <cstdio>

#define CHECK(expr) do { if ( !(expr) ) { \
	std::fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
	return 1; \
} } while ( 0 )

int main() {
	wiredProfileHostLifecycle_t lifecycle;
	WiredProfileHostLifecycle_Init( &lifecycle );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_NO_SWAPCHAIN );

	// Resize bursts coalesce to the latest physical-pixel extent.
	WiredProfileHostLifecycle_Pixels( &lifecycle, 640, 360 );
	WiredProfileHostLifecycle_Pixels( &lifecycle, 800, 450 );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_RECREATE_PENDING );
	CHECK( lifecycle.pixelWidth == 800 && lifecycle.pixelHeight == 450 );
	WiredProfileHostLifecycle_RecreateSucceeded( &lifecycle, 1 );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_ACTIVE && lifecycle.generation == 1 );

	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralSuccess, true )
		== WIRED_PROFILE_HOST_FRAME_RENDER );
	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralSuboptimal, true )
		== WIRED_PROFILE_HOST_FRAME_RENDER_THEN_RECREATE );
	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralSuboptimal, false )
		== WIRED_PROFILE_HOST_FRAME_FATAL );
	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralOutOfDate, false )
		== WIRED_PROFILE_HOST_FRAME_RECREATE );
	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralTimeout, false )
		== WIRED_PROFILE_HOST_FRAME_SKIP );
	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralSurfaceLost, false )
		== WIRED_PROFILE_HOST_FRAME_SURFACE_LOST );
	CHECK( WiredProfileHostLifecycle_PresentResult( &lifecycle, ralSuboptimal )
		== WIRED_PROFILE_HOST_FRAME_RECREATE );
	CHECK( WiredProfileHostLifecycle_PresentResult( &lifecycle, ralSurfaceLost )
		== WIRED_PROFILE_HOST_FRAME_SURFACE_LOST );

	// Minimize is authoritative even when the window server retains a nonzero
	// drawable size. No frame action is allowed until a restore requests a new
	// generation.
	WiredProfileHostLifecycle_Minimized( &lifecycle );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_SUSPENDED );
	CHECK( WiredProfileHostLifecycle_AcquireResult( &lifecycle, ralSuccess, true )
		== WIRED_PROFILE_HOST_FRAME_SKIP );
	WiredProfileHostLifecycle_Restored( &lifecycle, 800, 450 );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_RECREATE_PENDING );

	// Preflight failure retains the previous generation; post-native failure
	// clears it. Surface loss is terminal for this first host slice.
	WiredProfileHostLifecycle_RecreateFailed( &lifecycle, ralOutOfDate, true );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_ACTIVE && lifecycle.generation == 1
		&& lifecycle.recreatePending );
	WiredProfileHostLifecycle_RecreateFailed( &lifecycle, ralOutOfDate, false );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_RECREATE_PENDING && lifecycle.generation == 0 );
	WiredProfileHostLifecycle_RecreateFailed( &lifecycle, ralSurfaceLost, false );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_FATAL );
	WiredProfileHostLifecycle_Init( &lifecycle );
	WiredProfileHostLifecycle_Pixels( &lifecycle, 800, 450 );
	WiredProfileHostLifecycle_RecreateSucceeded( &lifecycle, 2 );
	WiredProfileHostLifecycle_RecreateFailed( &lifecycle, ralErrorDeviceLost, true );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_FATAL && !lifecycle.recreatePending );

	WiredProfileHostLifecycle_Init( &lifecycle );
	WiredProfileHostLifecycle_Pixels( &lifecycle, 0, 450 );
	CHECK( lifecycle.state == WIRED_PROFILE_HOST_SUSPENDED );

	std::puts( "wired profile host lifecycle contract: PASS" );
	return 0;
}
