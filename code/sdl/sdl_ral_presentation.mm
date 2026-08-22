// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sdl_ral_presentation.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct wiredSdlRalPresentationHost_s {
	SDL_Window *window;
	SDL_MetalView view;
	ralPresentationHostReceipt_t receipt;
	uint32_t requestedWidth;
	uint32_t requestedHeight;
	uint64_t nextGeneration;
	qboolean allowVisible;
	qboolean ownsVideoSubsystem;
};

static qboolean NextGeneration( wiredSdlRalPresentationHost_t *host,
		uint64_t *outGeneration ) {
	if ( !host || !outGeneration || host->nextGeneration == UINT64_MAX - 1u ) {
		return qfalse;
	}
	*outGeneration = ++host->nextGeneration;
	return qtrue;
}

static qboolean BuildReceipt( wiredSdlRalPresentationHost_t *host,
		uint64_t ownerGeneration, uint64_t surfaceGeneration,
		ralPresentationHostReceipt_t *outReceipt ) {
	ralPresentationHostReceipt_t receipt;
	SDL_WindowFlags flags;
	int logicalWidth = 0, logicalHeight = 0, pixelWidth = 0, pixelHeight = 0;
	if ( !host || !outReceipt || !host->window || !host->view
			|| !SDL_GetWindowSize( host->window, &logicalWidth, &logicalHeight )
			|| !SDL_GetWindowSizeInPixels( host->window, &pixelWidth, &pixelHeight )
			|| logicalWidth <= 0 || logicalHeight <= 0
			|| !Ral_PresentationExtentValid( (uint32_t)logicalWidth,
				(uint32_t)logicalHeight )
			|| pixelWidth <= 0 || pixelHeight <= 0
			|| !Ral_PresentationExtentValid( (uint32_t)pixelWidth,
				(uint32_t)pixelHeight ) ) return qfalse;
	flags = SDL_GetWindowFlags( host->window );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL;
	receipt.ownerGeneration = ownerGeneration;
	receipt.surfaceGeneration = surfaceGeneration;
	receipt.ownerIdentity = (uintptr_t)host->window;
	receipt.logicalWidth = (uint32_t)logicalWidth;
	receipt.logicalHeight = (uint32_t)logicalHeight;
	receipt.pixelWidth = (uint32_t)pixelWidth;
	receipt.pixelHeight = (uint32_t)pixelHeight;
	receipt.contentScaleX = (float)pixelWidth / (float)logicalWidth;
	receipt.contentScaleY = (float)pixelHeight / (float)logicalHeight;
	receipt.visible = ( flags & ( SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED ) )
		? qfalse : qtrue;
	receipt.ready = qtrue;
	if ( !Ral_PresentationHostReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

static void CloseNative( wiredSdlRalPresentationHost_t *host ) {
	if ( !host ) return;
	if ( host->view ) SDL_Metal_DestroyView( host->view );
	if ( host->window ) SDL_DestroyWindow( host->window );
	if ( host->ownsVideoSubsystem ) SDL_QuitSubSystem( SDL_INIT_VIDEO );
	host->view = NULL; host->window = NULL;
	host->ownsVideoSubsystem = qfalse;
	memset( &host->receipt, 0, sizeof( host->receipt ) );
}

static qboolean HostOpen( void *context,
		const ralPresentationHostOpenInfo_t *info,
		ralPresentationHostReceipt_t *outReceipt ) {
	wiredSdlRalPresentationHost_t *host =
		(wiredSdlRalPresentationHost_t *)context;
	ralPresentationHostReceipt_t receipt;
	uint64_t ownerGeneration, surfaceGeneration;
	if ( !host || !outReceipt || !Ral_PresentationHostOpenInfoValid( info )
			|| info->backendType != RAL_BACKEND_METAL ) return qfalse;
	if ( Ral_PresentationHostReceiptValid( &host->receipt ) ) {
		*outReceipt = host->receipt;
		return qtrue;
	}
	if ( !NextGeneration( host, &ownerGeneration )
			|| !NextGeneration( host, &surfaceGeneration ) ) return qfalse;
	if ( ( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) == 0u ) {
		if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) ) return qfalse;
		host->ownsVideoSubsystem = qtrue;
	}
	host->window = SDL_CreateWindow( "Wired RAL presentation host",
		(int)host->requestedWidth, (int)host->requestedHeight,
		SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN );
	if ( !host->window ) goto fail;
	host->view = SDL_Metal_CreateView( host->window );
	if ( !host->view || !SDL_Metal_GetLayer( host->view ) ) goto fail;
	if ( info->requestVisible && host->allowVisible ) {
		if ( !SDL_ShowWindow( host->window ) || !SDL_SyncWindow( host->window ) ) {
			goto fail;
		}
	}
	if ( !BuildReceipt( host, ownerGeneration, surfaceGeneration, &receipt ) ) {
		goto fail;
	}
	host->receipt = receipt;
	*outReceipt = receipt;
	return qtrue;
fail:
	CloseNative( host );
	return qfalse;
}

static qboolean HostRefresh( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationHostReceipt_t *outReceipt ) {
	wiredSdlRalPresentationHost_t *host =
		(wiredSdlRalPresentationHost_t *)context;
	ralPresentationHostReceipt_t candidate;
	uint64_t generation;
	qboolean changed = qfalse;
	if ( !host || !outReceipt
			|| !Ral_PresentationHostReceiptExact( currentReceipt,
				&host->receipt ) ) return qfalse;
	if ( host->requestedWidth != currentReceipt->logicalWidth
			|| host->requestedHeight != currentReceipt->logicalHeight ) {
		if ( !SDL_SetWindowSize( host->window, (int)host->requestedWidth,
				(int)host->requestedHeight ) || !SDL_SyncWindow( host->window ) ) {
			return qfalse;
		}
		changed = qtrue;
	}
	if ( !BuildReceipt( host, currentReceipt->ownerGeneration,
			currentReceipt->surfaceGeneration, &candidate ) ) return qfalse;
	if ( changed || candidate.logicalWidth != currentReceipt->logicalWidth
			|| candidate.logicalHeight != currentReceipt->logicalHeight
			|| candidate.pixelWidth != currentReceipt->pixelWidth
			|| candidate.pixelHeight != currentReceipt->pixelHeight
			|| candidate.contentScaleX != currentReceipt->contentScaleX
			|| candidate.contentScaleY != currentReceipt->contentScaleY
			|| candidate.visible != currentReceipt->visible ) {
		if ( !NextGeneration( host, &generation )
				|| !BuildReceipt( host, currentReceipt->ownerGeneration,
					generation, &candidate ) ) return qfalse;
	}
	host->receipt = candidate;
	*outReceipt = candidate;
	return qtrue;
}

static qboolean HostBorrow( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationSurfaceBorrow_t *outBorrow ) {
	wiredSdlRalPresentationHost_t *host =
		(wiredSdlRalPresentationHost_t *)context;
	ralPresentationSurfaceBorrow_t borrow;
	void *layer;
	if ( !host || !outBorrow
			|| !Ral_PresentationHostReceiptExact( currentReceipt,
				&host->receipt ) || !host->view ) return qfalse;
	layer = SDL_Metal_GetLayer( host->view );
	if ( !layer ) return qfalse;
	memset( &borrow, 0, sizeof( borrow ) );
	borrow.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	borrow.backendType = RAL_BACKEND_METAL;
	borrow.ownerGeneration = currentReceipt->ownerGeneration;
	borrow.surfaceGeneration = currentReceipt->surfaceGeneration;
	borrow.ownerIdentity = currentReceipt->ownerIdentity;
	borrow.surfaceIdentity = (uintptr_t)layer;
	borrow.ready = qtrue;
	if ( !Ral_PresentationSurfaceBorrowValid( &borrow ) ) return qfalse;
	*outBorrow = borrow;
	return qtrue;
}

static qboolean HostClose( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationHostCloseMode_t mode ) {
	wiredSdlRalPresentationHost_t *host =
		(wiredSdlRalPresentationHost_t *)context;
	if ( !host || !Ral_PresentationHostReceiptExact( currentReceipt,
			&host->receipt )
			|| ( mode != RAL_PRESENTATION_HOST_KEEP_OWNER
				&& mode != RAL_PRESENTATION_HOST_DESTROY_OWNER ) ) return qfalse;
	if ( mode == RAL_PRESENTATION_HOST_DESTROY_OWNER ) CloseNative( host );
	return qtrue;
}

qboolean WiredSdlRalPresentationHost_Create( uint32_t logicalWidth,
		uint32_t logicalHeight, qboolean allowVisible, uint64_t firstGeneration,
		wiredSdlRalPresentationHost_t **outHost,
		ralPresentationHostImports_t *outImports ) {
	wiredSdlRalPresentationHost_t *host;
	ralPresentationHostImports_t imports;
	if ( !outHost || !outImports || logicalWidth == 0u || logicalHeight == 0u
			|| logicalWidth > 16384u || logicalHeight > 16384u
			|| !Ral_PresentationExtentValid( logicalWidth, logicalHeight )
			|| ( allowVisible != qfalse && allowVisible != qtrue )
			|| firstGeneration == 0u || firstGeneration >= UINT64_MAX - 2u ) {
		return qfalse;
	}
	host = (wiredSdlRalPresentationHost_t *)calloc( 1u, sizeof( *host ) );
	if ( !host ) return qfalse;
	host->requestedWidth = logicalWidth;
	host->requestedHeight = logicalHeight;
	host->allowVisible = allowVisible;
	host->nextGeneration = firstGeneration - 1u;
	memset( &imports, 0, sizeof( imports ) );
	imports.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	imports.context = host;
	imports.open = HostOpen; imports.refresh = HostRefresh;
	imports.borrow = HostBorrow; imports.close = HostClose;
	if ( !Ral_PresentationHostImportsValid( &imports ) ) { free( host ); return qfalse; }
	*outHost = host;
	*outImports = imports;
	return qtrue;
}

qboolean WiredSdlRalPresentationHost_RequestResize(
		wiredSdlRalPresentationHost_t *host, uint32_t logicalWidth,
		uint32_t logicalHeight ) {
	if ( !host || !Ral_PresentationHostReceiptValid( &host->receipt )
			|| logicalWidth == 0u || logicalHeight == 0u
			|| logicalWidth > 16384u || logicalHeight > 16384u
			|| !Ral_PresentationExtentValid( logicalWidth, logicalHeight ) ) return qfalse;
	host->requestedWidth = logicalWidth;
	host->requestedHeight = logicalHeight;
	return qtrue;
}

qboolean WiredSdlRalPresentationHost_GetReceipt(
		wiredSdlRalPresentationHost_t *host,
		ralPresentationHostReceipt_t *outReceipt ) {
	if ( !host || !outReceipt
			|| !Ral_PresentationHostReceiptValid( &host->receipt ) ) return qfalse;
	*outReceipt = host->receipt;
	return qtrue;
}

void WiredSdlRalPresentationHost_Destroy(
		wiredSdlRalPresentationHost_t *host ) {
	if ( !host ) return;
	CloseNative( host );
	memset( host, 0, sizeof( *host ) );
	free( host );
}
