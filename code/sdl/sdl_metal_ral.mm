// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sdl_metal_ral.h"
#include "ral_presentation_host.h"
#include "ral_metal_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct wiredMetalSdl_s {
	ralMetalCore_t *core;
	SDL_Window *window;
	SDL_MetalView view;
	ralMetalPresent_t *present;
	wiredMetalSdlReceipt_t receipt;
	qboolean ownsVideoSubsystem;
};

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue ? qtrue : qfalse;
}

static qboolean ReceiptValid( const wiredMetalSdlReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == WIRED_METAL_SDL_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_METAL
		&& receipt->coreGeneration != 0u && receipt->coreGeneration != UINT64_MAX
		&& receipt->adapterGeneration != 0u && receipt->adapterGeneration != UINT64_MAX
		&& receipt->ownerIdentity != (uintptr_t)0
		&& receipt->pixelWidth != 0u && receipt->pixelHeight != 0u
		&& receipt->pixelWidth == receipt->presentation.selected.width
		&& receipt->pixelHeight == receipt->presentation.selected.height
		&& receipt->adapterGeneration == receipt->presentation.presentationGeneration
		&& receipt->coreGeneration == receipt->presentation.coreGeneration
		&& receipt->ownerIdentity != receipt->presentation.ownerIdentity
		&& receipt->presentation.ownsLayer == qfalse
		&& RalMetal_PresentLayerReceiptExact( &receipt->presentation,
			&receipt->presentation )
		&& BoolValid( receipt->hidden ) && receipt->hidden == qtrue
		&& receipt->ready == qtrue ) ? qtrue : qfalse;
}

qboolean WiredMetalSdl_ReceiptExact( const wiredMetalSdlReceipt_t *a,
		const wiredMetalSdlReceipt_t *b ) {
	return ( ReceiptValid( a ) && ReceiptValid( b )
		&& a->backendType == b->backendType
		&& a->coreGeneration == b->coreGeneration
		&& a->adapterGeneration == b->adapterGeneration
		&& a->ownerIdentity == b->ownerIdentity
		&& a->pixelWidth == b->pixelWidth && a->pixelHeight == b->pixelHeight
		&& RalMetal_PresentLayerReceiptExact( &a->presentation, &b->presentation )
		&& a->hidden == b->hidden && a->ready == b->ready ) ? qtrue : qfalse;
}

static qboolean OwnerMatches( const wiredMetalSdl_t *adapter,
		const ralMetalCoreReceipt_t *coreReceipt,
		const wiredMetalSdlReceipt_t *receipt ) {
	int pixelWidth = 0, pixelHeight = 0;
	void *layer;
	if ( !adapter || !adapter->window || !adapter->view || !adapter->present
			|| !coreReceipt || !receipt
			|| !WiredMetalSdl_ReceiptExact( receipt, &adapter->receipt )
			|| receipt->ownerIdentity != (uintptr_t)adapter
			|| !RalMetal_CoreMatchesReceipt( adapter->core, coreReceipt )
			|| !SDL_GetWindowSizeInPixels( adapter->window, &pixelWidth, &pixelHeight ) ) {
		return qfalse;
	}
	layer = SDL_Metal_GetLayer( adapter->view );
	return ( layer && (uintptr_t)layer == receipt->presentation.layerIdentity
		&& pixelWidth > 0 && pixelHeight > 0
		&& (uint32_t)pixelWidth == receipt->pixelWidth
		&& (uint32_t)pixelHeight == receipt->pixelHeight ) ? qtrue : qfalse;
}

static void BuildSdrCreateInfo( const wiredMetalSdlReceipt_t *current,
		uint32_t pixelWidth, uint32_t pixelHeight,
		ralSwapchainCreateInfo_t *outInfo, ralSurfaceFormat_t *outFormat,
		ralPresentPreference_t *outPreference ) {
	memset( outInfo, 0, sizeof( *outInfo ) );
	outFormat->format = current->presentation.selected.format;
	outFormat->colorSpace = current->presentation.selected.colorSpace;
	*outPreference = (ralPresentPreference_t){
		current->presentation.selected.presentMode,
		current->presentation.selected.requestedImageCount,
		current->presentation.selected.requestedImageCount };
	outInfo->desiredWidth = pixelWidth; outInfo->desiredHeight = pixelHeight;
	outInfo->formatPreferences = outFormat; outInfo->formatPreferenceCount = 1u;
	outInfo->presentPreferences = outPreference; outInfo->presentPreferenceCount = 1u;
	outInfo->requiredUsage = current->presentation.selected.usage;
}

qboolean WiredMetalSdl_Create( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		wiredMetalSdl_t **outAdapter, wiredMetalSdlReceipt_t *outReceipt ) {
	wiredMetalSdl_t *candidate;
	wiredMetalSdlReceipt_t receipt;
	ralSwapchainCreateInfo_t nativeInfo;
	int logicalWidth = 0, logicalHeight = 0;
	int pixelWidth = 0, pixelHeight = 0;
	void *layer;
	if ( !outAdapter || !outReceipt || !core || !coreReceipt || !createInfo
			|| createInfo->desiredWidth == 0u || createInfo->desiredHeight == 0u
			|| createInfo->desiredWidth > 16384u || createInfo->desiredHeight > 16384u
			|| !Ral_PresentationExtentValid( createInfo->desiredWidth,
				createInfo->desiredHeight )
			|| generation == 0u || generation == UINT64_MAX
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt ) ) return qfalse;
	candidate = (wiredMetalSdl_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	candidate->core = core;
	if ( ( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) == 0u ) {
		if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) ) { free( candidate ); return qfalse; }
		candidate->ownsVideoSubsystem = qtrue;
	}
	candidate->window = SDL_CreateWindow( "Wired native Metal smoke",
		(int)createInfo->desiredWidth, (int)createInfo->desiredHeight,
		SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN );
	if ( !candidate->window ) goto fail;
	candidate->view = SDL_Metal_CreateView( candidate->window );
	if ( !candidate->view ) goto fail;
	layer = SDL_Metal_GetLayer( candidate->view );
	if ( !layer
			|| !SDL_GetWindowSize( candidate->window,
				&logicalWidth, &logicalHeight )
			|| !SDL_GetWindowSizeInPixels( candidate->window,
				&pixelWidth, &pixelHeight )
			|| logicalWidth <= 0 || logicalHeight <= 0
			|| pixelWidth <= 0 || pixelHeight <= 0
			|| !Ral_PresentationExtentValid( (uint32_t)logicalWidth,
				(uint32_t)logicalHeight )
			|| !Ral_PresentationExtentValid( (uint32_t)pixelWidth,
				(uint32_t)pixelHeight ) ) goto fail;
	nativeInfo = *createInfo;
	nativeInfo.desiredWidth = (uint32_t)pixelWidth;
	nativeInfo.desiredHeight = (uint32_t)pixelHeight;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = WIRED_METAL_SDL_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.adapterGeneration = generation; receipt.ownerIdentity = (uintptr_t)candidate;
	receipt.pixelWidth = (uint32_t)pixelWidth; receipt.pixelHeight = (uint32_t)pixelHeight;
	if ( !RalMetal_PresentAdoptBorrowedLayer( core, coreReceipt, &nativeInfo, generation,
			layer, &candidate->present, &receipt.presentation ) ) goto fail;
	receipt.hidden = qtrue; receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) goto fail;
	candidate->receipt = receipt;
	*outAdapter = candidate; *outReceipt = receipt;
	return qtrue;
fail:
	if ( candidate->present ) RalMetal_PresentDestroy( candidate->present );
	if ( candidate->view ) SDL_Metal_DestroyView( candidate->view );
	if ( candidate->window ) SDL_DestroyWindow( candidate->window );
	if ( candidate->ownsVideoSubsystem ) SDL_QuitSubSystem( SDL_INIT_VIDEO );
	free( candidate );
	return qfalse;
}

qboolean WiredMetalSdl_Resize( wiredMetalSdl_t *adapter,
		const ralMetalCoreReceipt_t *coreReceipt,
		const wiredMetalSdlReceipt_t *currentReceipt,
		uint32_t logicalWidth, uint32_t logicalHeight, uint64_t generation,
		wiredMetalSdlReceipt_t *outReceipt ) {
	wiredMetalSdlReceipt_t receipt;
	ralSwapchainCreateInfo_t createInfo;
	ralSurfaceFormat_t format;
	ralPresentPreference_t preference;
	int logicalActualWidth = 0, logicalActualHeight = 0;
	int pixelWidth = 0, pixelHeight = 0;
	if ( !outReceipt || !OwnerMatches( adapter, coreReceipt, currentReceipt )
			|| logicalWidth == 0u || logicalHeight == 0u
			|| logicalWidth > 16384u || logicalHeight > 16384u
			|| !Ral_PresentationExtentValid( logicalWidth, logicalHeight )
			|| generation <= currentReceipt->adapterGeneration
			|| generation == UINT64_MAX ) return qfalse;
	if ( !SDL_SetWindowSize( adapter->window, (int)logicalWidth, (int)logicalHeight )
			|| !SDL_SyncWindow( adapter->window )
			|| !SDL_GetWindowSize( adapter->window,
				&logicalActualWidth, &logicalActualHeight )
			|| !SDL_GetWindowSizeInPixels( adapter->window, &pixelWidth, &pixelHeight )
			|| logicalActualWidth <= 0 || logicalActualHeight <= 0
			|| !Ral_PresentationExtentValid( (uint32_t)logicalActualWidth,
				(uint32_t)logicalActualHeight )
			|| pixelWidth <= 0 || pixelHeight <= 0
			|| !Ral_PresentationExtentValid( (uint32_t)pixelWidth,
				(uint32_t)pixelHeight ) ) return qfalse;
	BuildSdrCreateInfo( currentReceipt, (uint32_t)pixelWidth, (uint32_t)pixelHeight,
		&createInfo, &format, &preference );
	receipt = *currentReceipt;
	if ( !RalMetal_PresentReconfigure( adapter->present, coreReceipt,
			&currentReceipt->presentation, &createInfo, generation,
			&receipt.presentation ) ) return qfalse;
	receipt.adapterGeneration = generation;
	receipt.pixelWidth = (uint32_t)pixelWidth; receipt.pixelHeight = (uint32_t)pixelHeight;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	adapter->receipt = receipt; *outReceipt = receipt;
	return qtrue;
}

qboolean WiredMetalSdl_PresentClear( wiredMetalSdl_t *adapter,
		const ralMetalCoreReceipt_t *coreReceipt,
		const wiredMetalSdlReceipt_t *adapterReceipt,
		uint64_t acquireGeneration, uint64_t presentGeneration,
		const float clearColor[4], ralMetalDrawableReceipt_t *outDrawable,
		ralMetalPresentReceipt_t *outPresent ) {
	ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t presented;
	float maxRgb;
	if ( !outDrawable || !outPresent || !clearColor
			|| !OwnerMatches( adapter, coreReceipt, adapterReceipt )
			|| acquireGeneration == 0u || acquireGeneration == UINT64_MAX
			|| presentGeneration <= acquireGeneration || presentGeneration == UINT64_MAX ) {
		return qfalse;
	}
	maxRgb = adapterReceipt->presentation.extendedDynamicRange == qtrue ? 16.0f : 1.0f;
	for ( uint32_t i = 0u; i < 4u; ++i ) if ( !isfinite( clearColor[i] ) ) return qfalse;
	if ( clearColor[0] < 0.0f || clearColor[0] > maxRgb
			|| clearColor[1] < 0.0f || clearColor[1] > maxRgb
			|| clearColor[2] < 0.0f || clearColor[2] > maxRgb
			|| clearColor[3] < 0.0f || clearColor[3] > 1.0f ) return qfalse;
	if ( !RalMetal_PresentAcquire( adapter->present, coreReceipt,
			&adapterReceipt->presentation, acquireGeneration, &drawable )
			|| !RalMetal_PresentClearAndSubmit( adapter->present, coreReceipt,
				&adapterReceipt->presentation, &drawable, clearColor,
				presentGeneration, &presented ) ) return qfalse;
	*outDrawable = drawable; *outPresent = presented;
	return qtrue;
}

void WiredMetalSdl_Destroy( wiredMetalSdl_t *adapter ) {
	if ( !adapter ) return;
	RalMetal_PresentDestroy( adapter->present );
	SDL_Metal_DestroyView( adapter->view );
	SDL_DestroyWindow( adapter->window );
	if ( adapter->ownsVideoSubsystem ) SDL_QuitSubSystem( SDL_INIT_VIDEO );
	memset( adapter, 0, sizeof( *adapter ) );
	free( adapter );
}
