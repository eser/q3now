// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 71u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt, staleCore;
	ralSurfaceFormat_t sdrFormat = {
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralSurfaceFormat_t hdrFormat = {
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_COLORSPACE_DISPLAY_P3 };
	ralSurfaceFormat_t unsupportedFormat = {
		RAL_FORMAT_A2B10G10R10_UNORM, RAL_COLORSPACE_HDR10_ST2084 };
	ralPresentMode_t fifo = RAL_PRESENT_FIFO;
	ralPresentMode_t immediate = RAL_PRESENT_IMMEDIATE;
	ralSwapchainCreateInfo_t createInfo;
	ralMetalPresent_t *present = (ralMetalPresent_t *)(uintptr_t)0x1234u;
	ralMetalPresentLayerReceipt_t layerReceipt, layerBefore, exactLayer;
	ralMetalDrawableReceipt_t drawableReceipt, drawableBefore, exactDrawable;
	ralMetalPresentReceipt_t presentReceipt, presentBefore, exactPresent;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t lossReceipt;
	float sdrClear[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
	float hdrClear[4] = { 2.0f, 1.25f, 0.5f, 1.0f };
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.desiredWidth = 16u; createInfo.desiredHeight = 16u;
	createInfo.formatPreferences = &sdrFormat; createInfo.formatPreferenceCount = 1u;
	createInfo.presentModePreferences = &fifo; createInfo.presentModePreferenceCount = 1u;
	createInfo.desiredImageCount = 3u;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	memset( &layerReceipt, 0x5a, sizeof( layerReceipt ) ); layerBefore = layerReceipt;
	createInfo.desiredWidth = 0u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	CHECK( present == (ralMetalPresent_t *)(uintptr_t)0x1234u
		&& memcmp( &layerReceipt, &layerBefore, sizeof( layerReceipt ) ) == 0 );
	createInfo.desiredWidth = 16u; createInfo.formatPreferences = &unsupportedFormat;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.formatPreferences = &sdrFormat; createInfo.desiredImageCount = 4u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.desiredImageCount = 3u;
	ralPresentMode_t unsupportedMode = RAL_PRESENT_MAILBOX;
	createInfo.presentModePreferences = &unsupportedMode;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.presentModePreferences = &fifo;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_SAMPLED;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	CHECK( layerReceipt.selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		&& layerReceipt.selected.colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR
		&& layerReceipt.selected.presentMode == RAL_PRESENT_FIFO
		&& layerReceipt.displaySyncEnabled == qtrue
		&& layerReceipt.extendedDynamicRange == qfalse
		&& layerReceipt.ownsLayer == qtrue );
	exactLayer = layerReceipt;
	CHECK( RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) );
#define MUTATE_LAYER(field) do { exactLayer = layerReceipt; exactLayer.field++; \
	CHECK( !RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) ); } while (0)
	MUTATE_LAYER( coreGeneration ); MUTATE_LAYER( presentationGeneration );
	MUTATE_LAYER( selected.width ); MUTATE_LAYER( selected.imageCount );
#undef MUTATE_LAYER
	exactLayer = layerReceipt; exactLayer.ownsLayer = qfalse;
	CHECK( !RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) );
	exactLayer = layerReceipt; exactLayer.ownerIdentity = exactLayer.layerIdentity;
	CHECK( !RalMetal_PresentLayerReceiptExact( &exactLayer, &exactLayer ) );

	memset( &drawableReceipt, 0x5a, sizeof( drawableReceipt ) );
	drawableBefore = drawableReceipt;
	staleCore = coreReceipt; staleCore.generation++;
	CHECK( !RalMetal_PresentAcquire( present, &staleCore, &layerReceipt, 73u,
		&drawableReceipt ) );
	exactLayer = layerReceipt; exactLayer.presentationGeneration++;
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &exactLayer, 73u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 73u,
		&drawableReceipt ) );
	CHECK( drawableReceipt.width == 16u && drawableReceipt.height == 16u
		&& drawableReceipt.format == RAL_FORMAT_B8G8R8A8_UNORM );
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 74u,
		&drawableBefore ) );
	CHECK( !RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 74u, &exactLayer ) );
	exactDrawable = drawableReceipt;
	CHECK( RalMetal_DrawableReceiptExact( &drawableReceipt, &exactDrawable ) );
	exactDrawable.textureIdentity++;
	CHECK( !RalMetal_DrawableReceiptExact( &drawableReceipt, &exactDrawable ) );
	memset( &presentReceipt, 0x5a, sizeof( presentReceipt ) ); presentBefore = presentReceipt;
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&exactDrawable, sdrClear, 75u, &presentReceipt ) );
	CHECK( memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) == 0 );
	sdrClear[0] = NAN;
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, sdrClear, 75u, &presentReceipt ) );
	sdrClear[0] = 0.125f;
	CHECK( memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) == 0 );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, sdrClear, 75u, &presentReceipt ) );
	CHECK( presentReceipt.presented == qtrue
		&& presentReceipt.submission.commands[0].state == RAL_COMMAND_SUBMITTED
		&& presentReceipt.clearDigest != 0u );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, sdrClear, 76u, &presentBefore ) );
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 73u,
		&drawableBefore ) );
	exactPresent = presentReceipt;
	CHECK( RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) );
#define MUTATE_PRESENT(field) do { exactPresent = presentReceipt; exactPresent.field++; \
	CHECK( !RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) ); } while (0)
	MUTATE_PRESENT( coreGeneration ); MUTATE_PRESENT( presentationGeneration );
	MUTATE_PRESENT( acquireGeneration ); MUTATE_PRESENT( presentGeneration );
	MUTATE_PRESENT( completionGeneration ); MUTATE_PRESENT( submission.generation );
	MUTATE_PRESENT( clearDigest );
#undef MUTATE_PRESENT

	createInfo.desiredWidth = 32u; createInfo.desiredHeight = 16u;
	createInfo.formatPreferences = &hdrFormat;
	createInfo.presentModePreferences = &immediate;
	createInfo.desiredImageCount = 2u;
	CHECK( RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 80u, &layerReceipt ) );
	CHECK( layerReceipt.selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
		&& layerReceipt.selected.colorSpace == RAL_COLORSPACE_DISPLAY_P3
		&& layerReceipt.selected.presentMode == RAL_PRESENT_IMMEDIATE
		&& layerReceipt.displaySyncEnabled == qfalse
		&& layerReceipt.extendedDynamicRange == qtrue
		&& layerReceipt.selected.width == 32u && layerReceipt.selected.imageCount == 2u );
	CHECK( !RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 80u, &exactLayer ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 81u,
		&drawableReceipt ) );
	CHECK( drawableReceipt.format == RAL_FORMAT_R16G16B16A16_SFLOAT );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, hdrClear, 82u, &presentReceipt ) );
	CHECK( presentReceipt.colorSpace == RAL_COLORSPACE_DISPLAY_P3
		&& presentReceipt.presentMode == RAL_PRESENT_IMMEDIATE );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 83u,
		&drawableReceipt ) );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, hdrClear, 82u, &presentBefore ) );
	memset( &lossEvent, 0, sizeof( lossEvent ) );
	lossEvent.backendType = RAL_BACKEND_METAL;
	lossEvent.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	lossEvent.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	lossEvent.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	lossEvent.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	lossEvent.requestedBytes = 64u; lossEvent.attempt = 1u;
	lossEvent.maxAttempts = 1u; lossEvent.liveParent = qtrue;
	CHECK( RalMetal_CorePublishDeviceLoss( core, &lossEvent, &lossReceipt ) );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, hdrClear, 84u, &presentBefore ) );
	RalMetal_PresentDestroy( present );
	RalMetal_CoreDestroy( core );
	puts( "ral metal presentation: PASS" );
	return 0;
}
