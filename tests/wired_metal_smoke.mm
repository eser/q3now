// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sdl_metal_ral.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 201u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralSurfaceFormat_t format = {
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralPresentPreference_t preference = { RAL_PRESENT_FIFO, 3u, 3u };
	ralSwapchainCreateInfo_t createInfo;
	wiredMetalSdl_t *adapter = (wiredMetalSdl_t *)(uintptr_t)0x1234u;
	wiredMetalSdlReceipt_t receipt, before, exact, resized;
	ralMetalDrawableReceipt_t drawable, drawableBefore;
	ralMetalPresentReceipt_t presented, presentBefore;
	float clear0[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
	float clear1[4] = { 0.5f, 0.25f, 0.125f, 1.0f };
	float clear2[4] = { 0.0625f, 0.125f, 0.25f, 1.0f };

	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.desiredWidth = 1280u; createInfo.desiredHeight = 720u;
	createInfo.formatPreferences = &format; createInfo.formatPreferenceCount = 1u;
	createInfo.presentPreferences = &preference; createInfo.presentPreferenceCount = 1u;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	createInfo.desiredWidth = 0u;
	CHECK( !WiredMetalSdl_Create( core, &coreReceipt, &createInfo, 202u,
		&adapter, &receipt ) );
	CHECK( adapter == (wiredMetalSdl_t *)(uintptr_t)0x1234u
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	createInfo.desiredWidth = 1280u;
	CHECK( WiredMetalSdl_Create( core, &coreReceipt, &createInfo, 202u,
		&adapter, &receipt ) );
	CHECK( receipt.presentation.ownsLayer == qfalse
		&& receipt.presentation.selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		&& receipt.presentation.selected.colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR
		&& receipt.presentation.selected.presentMode == RAL_PRESENT_FIFO
		&& receipt.presentation.displaySyncEnabled == qtrue
		&& receipt.pixelWidth > 0u && receipt.pixelHeight > 0u );
	exact = receipt;
	CHECK( WiredMetalSdl_ReceiptExact( &receipt, &exact ) );
	exact.adapterGeneration++;
	CHECK( !WiredMetalSdl_ReceiptExact( &receipt, &exact ) );
	exact = receipt; exact.presentation.ownsLayer = qtrue;
	CHECK( !WiredMetalSdl_ReceiptExact( &exact, &exact ) );

	CHECK( WiredMetalSdl_PresentClear( adapter, &coreReceipt, &receipt,
		203u, 204u, clear0, &drawable, &presented ) );
	CHECK( drawable.width == receipt.pixelWidth && drawable.height == receipt.pixelHeight
		&& presented.presented == qtrue && presented.presentGeneration == 204u );
	CHECK( WiredMetalSdl_PresentClear( adapter, &coreReceipt, &receipt,
		205u, 206u, clear1, &drawable, &presented ) );
	memset( &drawableBefore, 0x5a, sizeof( drawableBefore ) );
	memset( &presentBefore, 0x5a, sizeof( presentBefore ) );
	exact = receipt; exact.presentation.presentationGeneration++;
	CHECK( !WiredMetalSdl_PresentClear( adapter, &coreReceipt, &exact,
		207u, 208u, clear2, &drawableBefore, &presentBefore ) );
	CHECK( ((const unsigned char *)&drawableBefore)[0] == 0x5a
		&& ((const unsigned char *)&presentBefore)[0] == 0x5a );

	before = receipt;
	CHECK( !WiredMetalSdl_Resize( adapter, &coreReceipt, &receipt,
		1600u, 900u, 202u, &resized ) );
	CHECK( WiredMetalSdl_Resize( adapter, &coreReceipt, &receipt,
		1600u, 900u, 207u, &resized ) );
	CHECK( resized.adapterGeneration == 207u
		&& resized.presentation.presentationGeneration == 207u
		&& resized.presentation.layerIdentity == receipt.presentation.layerIdentity
		&& resized.presentation.ownsLayer == qfalse
		&& resized.pixelWidth > 0u && resized.pixelHeight > 0u
		&& ( resized.pixelWidth != before.pixelWidth
			|| resized.pixelHeight != before.pixelHeight ) );
	CHECK( WiredMetalSdl_PresentClear( adapter, &coreReceipt, &resized,
		208u, 209u, clear2, &drawable, &presented ) );
	CHECK( drawable.width == resized.pixelWidth && drawable.height == resized.pixelHeight
		&& presented.presentationGeneration == 207u
		&& presented.acquireGeneration == 208u && presented.presentGeneration == 209u
		&& RalMetal_PresentReceiptExact( &presented, &presented ) );

	WiredMetalSdl_Destroy( adapter );
	RalMetal_CoreDestroy( core );
	puts( "wired native Metal SDL smoke: PASS" );
	return 0;
}
