// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_module.h"
#include "sdl_ral_presentation.h"
#include "tr_public.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#ifndef RAL_METAL_TEST_MODULE
#error RAL_METAL_TEST_MODULE is required
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
	__FILE__,__LINE__,#x); return 1; } } while (0)

typedef qboolean (*frameReceiptFn_t)( ralMetalModuleFrameReceipt_t *outReceipt );
typedef qboolean (*frameReceiptExactFn_t)(
	const ralMetalModuleFrameReceipt_t *a,
	const ralMetalModuleFrameReceipt_t *b );

static qboolean RejectOpen( void *context,
		const ralPresentationHostOpenInfo_t *info,
		ralPresentationHostReceipt_t *outReceipt ) {
	(void)context; (void)info; (void)outReceipt;
	return qfalse;
}

int main( void ) {
	void *library;
	GetRefAPI_t getRefApi;
	frameReceiptFn_t getFrameReceipt;
	frameReceiptExactFn_t frameReceiptExact;
	refimport_t imports;
	refexport_t *exports;
	wiredSdlRalPresentationHost_t *presentationHost = NULL;
	ralPresentationHostReceipt_t hostAfterKeep;
	ralPresentationHostOpenFn realOpen;
	glconfig_t config;
	ralMetalModuleFrameReceipt_t receipt, first, before, exact;
	int frontEnd = -1, backEnd = -1;

	library = dlopen( RAL_METAL_TEST_MODULE, RTLD_NOW | RTLD_LOCAL );
	CHECK( library != NULL );
	getRefApi = (GetRefAPI_t)dlsym( library, "GetRefAPI" );
	getFrameReceipt = (frameReceiptFn_t)dlsym( library, "WiredMetal_GetFrameReceipt" );
	frameReceiptExact = (frameReceiptExactFn_t)dlsym( library,
		"RalMetal_ModuleFrameReceiptExact" );
	CHECK( getRefApi != NULL && getFrameReceipt != NULL && frameReceiptExact != NULL );
	memset( &imports, 0, sizeof( imports ) );
	CHECK( getRefApi( REF_API_VERSION, &imports ) == NULL );
	CHECK( WiredSdlRalPresentationHost_Create( 1280u, 720u, qfalse, 701u,
		&presentationHost, &imports.PresentationHost ) );
	CHECK( getRefApi( REF_API_VERSION - 1, &imports ) == NULL );
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->initFailed == qfalse );
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	exports->BeginFrame( STEREO_CENTER );
	CHECK( exports->initFailed == qtrue && !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	exports->Shutdown( REF_UNLOAD_DLL );
	realOpen = imports.PresentationHost.open;
	imports.PresentationHost.open = RejectOpen;
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->initFailed == qfalse );
	memset( &config, 0, sizeof( config ) );
	exports->BeginRegistration( &config );
	CHECK( exports->initFailed == qtrue
		&& !WiredSdlRalPresentationHost_GetReceipt( presentationHost,
			&hostAfterKeep ) );
	exports->Shutdown( REF_UNLOAD_DLL );
	imports.PresentationHost.open = realOpen;
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->Shutdown && exports->BeginRegistration
		&& exports->EndRegistration && exports->BeginFrame && exports->EndFrame
		&& exports->ClearScene && exports->RegisterShader && exports->GetConfig
		&& exports->GetMemoryBudget && exports->initFailed == qfalse );
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	CHECK( !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	memset( &config, 0, sizeof( config ) );
	exports->BeginRegistration( &config );
	CHECK( exports->initFailed == qfalse );
	CHECK( config.vidWidth > 0 && config.vidHeight > 0 );
	CHECK( strstr( config.renderer_string, "native Metal RAL" ) != NULL );
	CHECK( exports->GetConfig() != NULL );
	exports->EndRegistration(); exports->BeginFrame( STEREO_CENTER );
	exports->EndFrame( &frontEnd, &backEnd );
	CHECK( exports->initFailed == qfalse && frontEnd == 0 && backEnd == 0 );
	CHECK( getFrameReceipt( &receipt )
		&& frameReceiptExact( &receipt, &receipt )
		&& receipt.presentation.presented == qtrue
		&& receipt.frame.state == RAL_FRAME_SHELL_PRESENTED );
	first = receipt; exact = receipt;
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !frameReceiptExact( &receipt, &exact ) ); } while (0)
	MUTATE( moduleGeneration ); MUTATE( frameGeneration );
	MUTATE( frame.ownerGeneration ); MUTATE( host.surfaceGeneration );
	MUTATE( surface.surfaceIdentity );
	MUTATE( drawable.acquireGeneration ); MUTATE( presentation.presentGeneration );
#undef MUTATE

	exports->Shutdown( REF_LEVEL_ONLY );
	CHECK( WiredSdlRalPresentationHost_RequestResize( presentationHost,
		800u, 450u ) );
	memset( &config, 0, sizeof( config ) ); exports->BeginRegistration( &config );
	exports->BeginFrame( STEREO_CENTER ); exports->EndFrame( NULL, NULL );
	CHECK( getFrameReceipt( &receipt ) && receipt.frameGeneration > first.frameGeneration
		&& receipt.moduleGeneration == first.moduleGeneration
		&& receipt.host.surfaceGeneration > first.host.surfaceGeneration
		&& receipt.host.logicalWidth == 800u && receipt.host.logicalHeight == 450u
		&& receipt.surface.surfaceIdentity == first.surface.surfaceIdentity );
	exports->Shutdown( REF_KEEP_WINDOW );
	CHECK( WiredSdlRalPresentationHost_GetReceipt( presentationHost,
		&hostAfterKeep )
		&& hostAfterKeep.ownerIdentity == receipt.host.ownerIdentity );
	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports != NULL && exports->initFailed == qfalse );
	memset( &config, 0, sizeof( config ) ); exports->BeginRegistration( &config );
	exports->BeginFrame( STEREO_CENTER ); exports->EndFrame( NULL, NULL );
	CHECK( getFrameReceipt( &exact )
		&& exact.moduleGeneration > receipt.moduleGeneration
		&& exact.host.ownerIdentity == receipt.host.ownerIdentity
		&& exact.surface.surfaceIdentity == receipt.surface.surfaceIdentity );
	exports->EndFrame( NULL, NULL );
	CHECK( exports->initFailed == qtrue );
	receipt = exact;
	CHECK( getFrameReceipt( &exact ) && frameReceiptExact( &receipt, &exact ) );
	exports->Shutdown( REF_UNLOAD_DLL );
	before = receipt;
	CHECK( !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	CHECK( !WiredSdlRalPresentationHost_GetReceipt( presentationHost,
		&hostAfterKeep ) );
	CHECK( dlclose( library ) == 0 );
	WiredSdlRalPresentationHost_Destroy( presentationHost );
	puts( "wired Metal renderer module: PASS" );
	return 0;
}
