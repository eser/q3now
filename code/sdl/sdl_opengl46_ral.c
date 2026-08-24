// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sdl_opengl46_ral.h"

#include <SDL3/SDL.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct wiredSdlOpenGl46_s {
	SDL_Window *window;
	SDL_GLContext context;
	ralOpenGlCore_t *core;
	wiredSdlOpenGl46Receipt_t receipt;
	uint32_t requestedWidth;
	uint32_t requestedHeight;
	uint64_t nextGeneration;
	qboolean visible;
	qboolean ownsVideoSubsystem;
};

static qboolean NextGeneration( wiredSdlOpenGl46_t *adapter,
		uint64_t *outGeneration ) {
	if ( !adapter || !outGeneration
			|| adapter->nextGeneration >= UINT64_MAX - 1u ) return qfalse;
	*outGeneration = ++adapter->nextGeneration;
	return qtrue;
}

#if !defined(__APPLE__)
static ralOpenGlProc_t ResolveGl( const char *name, void *userData ) {
	(void)userData;
	return (ralOpenGlProc_t)SDL_GL_GetProcAddress( name );
}
#endif

static qboolean ReceiptValid( const wiredSdlOpenGl46Receipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == WIRED_SDL_OPENGL46_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->adapterGeneration != 0u
		&& receipt->adapterGeneration != UINT64_MAX
		&& Ral_PresentationHostReceiptValid( &receipt->host )
		&& receipt->host.backendType == RAL_BACKEND_OPENGL
		&& RalOpenGl_CoreReceiptExact( &receipt->core, &receipt->core )
		&& receipt->ready == qtrue;
}

qboolean WiredSdlOpenGl46_ReceiptExact( const wiredSdlOpenGl46Receipt_t *a,
		const wiredSdlOpenGl46Receipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& a->adapterGeneration == b->adapterGeneration
		&& a->presentedFrames == b->presentedFrames
		&& Ral_PresentationHostReceiptExact( &a->host, &b->host )
		&& RalOpenGl_CoreReceiptExact( &a->core, &b->core );
}

static qboolean Matches( const wiredSdlOpenGl46_t *adapter,
		const wiredSdlOpenGl46Receipt_t *receipt ) {
	return adapter && WiredSdlOpenGl46_ReceiptExact( &adapter->receipt, receipt );
}

static qboolean BuildHostReceipt( wiredSdlOpenGl46_t *adapter,
		uint64_t ownerGeneration, uint64_t surfaceGeneration,
		ralPresentationHostReceipt_t *outReceipt ) {
	ralPresentationHostReceipt_t receipt;
	SDL_WindowFlags flags;
	int logicalWidth = 0, logicalHeight = 0, pixelWidth = 0, pixelHeight = 0;
	if ( !adapter || !adapter->window || !adapter->context || !outReceipt
			|| !SDL_GetWindowSize( adapter->window, &logicalWidth, &logicalHeight )
			|| !SDL_GetWindowSizeInPixels( adapter->window, &pixelWidth, &pixelHeight )
			|| logicalWidth <= 0 || logicalHeight <= 0
			|| pixelWidth <= 0 || pixelHeight <= 0 ) return qfalse;
	flags = SDL_GetWindowFlags( adapter->window );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_OPENGL;
	receipt.ownerGeneration = ownerGeneration;
	receipt.surfaceGeneration = surfaceGeneration;
	// The public receipt carries only adapter-owned opaque identities. SDL
	// window/context types remain private to this platform unit.
	receipt.ownerIdentity = (uintptr_t)adapter;
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

static void CloseNative( wiredSdlOpenGl46_t *adapter ) {
	if ( !adapter ) return;
	// RAL children own GL entry points tied to this exact context. Destroy them
	// before the native context, then destroy the window owner last.
	if ( adapter->core ) RalOpenGl_CoreDestroy( adapter->core );
	adapter->core = NULL;
	if ( adapter->context ) SDL_GL_DestroyContext( adapter->context );
	adapter->context = NULL;
	if ( adapter->window ) SDL_DestroyWindow( adapter->window );
	adapter->window = NULL;
	if ( adapter->ownsVideoSubsystem ) SDL_QuitSubSystem( SDL_INIT_VIDEO );
	adapter->ownsVideoSubsystem = qfalse;
	memset( &adapter->receipt, 0, sizeof( adapter->receipt ) );
}

static wiredSdlOpenGl46Status_t OpenNative( wiredSdlOpenGl46_t *adapter ) {
#if defined(__APPLE__)
	// Apple's system OpenGL implementation tops out at 4.1 Core. The canonical
	// adapter is 4.6-only and must never silently fall back to that API.
	(void)adapter;
	return WIRED_SDL_OPENGL46_UNSUPPORTED;
#else
	wiredSdlOpenGl46Receipt_t receipt;
	ralOpenGlCoreCreateInfo_t coreInfo;
	uint64_t adapterGeneration, ownerGeneration, surfaceGeneration;
	int actualMajor = 0, actualMinor = 0, actualProfile = 0;
	SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if ( !adapter || !NextGeneration( adapter, &adapterGeneration )
			|| !NextGeneration( adapter, &ownerGeneration )
			|| !NextGeneration( adapter, &surfaceGeneration ) )
		return WIRED_SDL_OPENGL46_FAILED;
	if ( !adapter->visible ) flags |= SDL_WINDOW_HIDDEN;
	if ( ( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) == 0u ) {
		if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) )
			return WIRED_SDL_OPENGL46_FAILED;
		adapter->ownsVideoSubsystem = qtrue;
	}
	SDL_GL_ResetAttributes();
	if ( !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 )
			|| !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 6 )
			|| !SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK,
				SDL_GL_CONTEXT_PROFILE_CORE )
			|| !SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS,
				SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG )
			|| !SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 )
			|| !SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 )
			|| !SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 ) ) goto fail;
	adapter->window = SDL_CreateWindow( "Wired OpenGL 4.6 RAL",
		(int)adapter->requestedWidth, (int)adapter->requestedHeight, flags );
	if ( !adapter->window ) goto fail;
	adapter->context = SDL_GL_CreateContext( adapter->window );
	if ( !adapter->context
			|| !SDL_GL_MakeCurrent( adapter->window, adapter->context )
			|| !SDL_GL_GetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, &actualMajor )
			|| !SDL_GL_GetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, &actualMinor )
			|| !SDL_GL_GetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, &actualProfile )
			|| actualMajor != 4 || actualMinor < 6
			|| !( actualProfile & SDL_GL_CONTEXT_PROFILE_CORE ) ) goto fail;
	memset( &coreInfo, 0, sizeof( coreInfo ) );
	coreInfo.generation = adapterGeneration;
	coreInfo.contextIdentity = (uintptr_t)adapter->context;
	coreInfo.resolveProc = ResolveGl;
	if ( !RalOpenGl_CoreCreate( &coreInfo, &adapter->core, &receipt.core )
			|| !BuildHostReceipt( adapter, ownerGeneration, surfaceGeneration,
				&receipt.host ) ) goto fail;
	receipt.schemaVersion = WIRED_SDL_OPENGL46_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_OPENGL;
	receipt.adapterGeneration = adapterGeneration;
	receipt.presentedFrames = 0u;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) goto fail;
	adapter->receipt = receipt;
	return WIRED_SDL_OPENGL46_READY;
fail:
	CloseNative( adapter );
	return WIRED_SDL_OPENGL46_FAILED;
#endif
}

wiredSdlOpenGl46Status_t WiredSdlOpenGl46_Create(
		const wiredSdlOpenGl46CreateInfo_t *createInfo,
		wiredSdlOpenGl46_t **outAdapter,
		wiredSdlOpenGl46Receipt_t *outReceipt ) {
	wiredSdlOpenGl46_t *candidate;
	wiredSdlOpenGl46Status_t status;
	if ( !createInfo || !outAdapter || !outReceipt
			|| !Ral_PresentationExtentValid( createInfo->logicalWidth,
				createInfo->logicalHeight )
			|| createInfo->logicalWidth > 16384u
			|| createInfo->logicalHeight > 16384u
			|| createInfo->firstGeneration == 0u
			|| createInfo->firstGeneration >= UINT64_MAX - 3u
			|| ( createInfo->visible != qfalse && createInfo->visible != qtrue ) )
		return WIRED_SDL_OPENGL46_FAILED;
	candidate = (wiredSdlOpenGl46_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return WIRED_SDL_OPENGL46_FAILED;
	candidate->requestedWidth = createInfo->logicalWidth;
	candidate->requestedHeight = createInfo->logicalHeight;
	candidate->visible = createInfo->visible;
	candidate->nextGeneration = createInfo->firstGeneration - 1u;
	status = OpenNative( candidate );
	if ( status != WIRED_SDL_OPENGL46_READY ) {
		CloseNative( candidate );
		free( candidate );
		return status;
	}
	*outAdapter = candidate;
	*outReceipt = candidate->receipt;
	return WIRED_SDL_OPENGL46_READY;
}

void WiredSdlOpenGl46_Destroy( wiredSdlOpenGl46_t *adapter ) {
	if ( !adapter ) return;
	CloseNative( adapter );
	memset( adapter, 0, sizeof( *adapter ) );
	free( adapter );
}

qboolean WiredSdlOpenGl46_RequestResize( wiredSdlOpenGl46_t *adapter,
		const wiredSdlOpenGl46Receipt_t *current, uint32_t logicalWidth,
		uint32_t logicalHeight, wiredSdlOpenGl46Receipt_t *outReceipt ) {
	wiredSdlOpenGl46Receipt_t candidate;
	uint64_t surfaceGeneration;
	if ( !Matches( adapter, current ) || !outReceipt
			|| !Ral_PresentationExtentValid( logicalWidth, logicalHeight )
			|| logicalWidth > 16384u || logicalHeight > 16384u
			|| !NextGeneration( adapter, &surfaceGeneration ) ) return qfalse;
	if ( !SDL_SetWindowSize( adapter->window, (int)logicalWidth,
			(int)logicalHeight ) || !SDL_SyncWindow( adapter->window ) ) return qfalse;
	candidate = adapter->receipt;
	adapter->requestedWidth = logicalWidth;
	adapter->requestedHeight = logicalHeight;
	if ( !BuildHostReceipt( adapter, current->host.ownerGeneration,
			surfaceGeneration, &candidate.host ) ) return qfalse;
	adapter->receipt = candidate;
	*outReceipt = candidate;
	return qtrue;
}

qboolean WiredSdlOpenGl46_Present( wiredSdlOpenGl46_t *adapter,
		const wiredSdlOpenGl46Receipt_t *current,
		wiredSdlOpenGl46Receipt_t *outReceipt ) {
	wiredSdlOpenGl46Receipt_t candidate;
	if ( !Matches( adapter, current ) || !outReceipt
			|| current->presentedFrames == UINT64_MAX
			|| !SDL_GL_SwapWindow( adapter->window ) ) return qfalse;
	candidate = adapter->receipt;
	candidate.presentedFrames++;
	adapter->receipt = candidate;
	*outReceipt = candidate;
	return qtrue;
}

qboolean WiredSdlOpenGl46_RecreateAfterLoss( wiredSdlOpenGl46_t *adapter,
		const wiredSdlOpenGl46Receipt_t *current,
		const ralMemoryFailureEvent_t *lossEvent,
		wiredSdlOpenGl46Receipt_t *outReceipt ) {
	ralMemoryFailureReceipt_t loss;
	wiredSdlOpenGl46Status_t status;
	if ( !Matches( adapter, current ) || !lossEvent || !outReceipt
			|| !RalOpenGl_CorePublishDeviceLoss( adapter->core, lossEvent, &loss )
			|| loss.action != RAL_MEMORY_RECOVERY_RECREATE_BACKEND ) return qfalse;
	CloseNative( adapter );
	status = OpenNative( adapter );
	if ( status != WIRED_SDL_OPENGL46_READY ) return qfalse;
	*outReceipt = adapter->receipt;
	return qtrue;
}

qboolean WiredSdlOpenGl46_BorrowCore( wiredSdlOpenGl46_t *adapter,
		const wiredSdlOpenGl46Receipt_t *current, ralOpenGlCore_t **outCore,
		ralOpenGlCoreReceipt_t *outReceipt ) {
	if ( !Matches( adapter, current ) || !outCore || !outReceipt ) return qfalse;
	*outCore = adapter->core;
	*outReceipt = adapter->receipt.core;
	return qtrue;
}
