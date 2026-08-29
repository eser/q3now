// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_module.h"
#include "tr_public.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#ifndef RAL_OPENGL_TEST_MODULE
#error RAL_OPENGL_TEST_MODULE is required
#endif

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
		#condition ); return 1; } } while ( 0 )

typedef qboolean (*frameReceiptFn_t)( ralOpenGlModuleFrameReceipt_t *outReceipt );
typedef qboolean (*frameReceiptExactFn_t)(
	const ralOpenGlModuleFrameReceipt_t *a,
	const ralOpenGlModuleFrameReceipt_t *b );

static unsigned int s_initCount;
static unsigned int s_shutdownCount;
static unsigned int s_presentCount;

static void TestInit( glconfig_t *config ) {
	++s_initCount;
	if ( config ) {
		config->vidWidth = 1280;
		config->vidHeight = 720;
	}
}

static void TestShutdown( qboolean unloadDll ) {
	(void)unloadDll;
	++s_shutdownCount;
}

static void TestPresent( void ) { ++s_presentCount; }
static void *RejectProc( const char *name ) { (void)name; return NULL; }

int main( void ) {
	void *library;
	GetRefAPI_t getRefApi;
	frameReceiptFn_t getFrameReceipt;
	frameReceiptExactFn_t frameReceiptExact;
	refimport_t imports;
	refexport_t *exports;
	ralOpenGlModuleFrameReceipt_t receipt, before;
	glconfig_t config;

	library = dlopen( RAL_OPENGL_TEST_MODULE, RTLD_NOW | RTLD_LOCAL );
	CHECK( library != NULL );
	getRefApi = (GetRefAPI_t)dlsym( library, "GetRefAPI" );
	getFrameReceipt = (frameReceiptFn_t)dlsym( library,
		"WiredOpenGl_GetFrameReceipt" );
	frameReceiptExact = (frameReceiptExactFn_t)dlsym( library,
		"RalOpenGl_ModuleFrameReceiptExact" );
	CHECK( getRefApi && getFrameReceipt && frameReceiptExact );

	memset( &imports, 0, sizeof( imports ) );
	CHECK( getRefApi( REF_API_VERSION, &imports ) == NULL );
	imports.GLimp_InitOpenGL46 = TestInit;
	imports.GLimp_Shutdown = TestShutdown;
	imports.GLimp_EndFrame = TestPresent;
	imports.GL_GetProcAddress = RejectProc;
	CHECK( getRefApi( REF_API_VERSION - 1, &imports ) == NULL );

	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports && !exports->initFailed && exports->Shutdown
		&& exports->BeginRegistration && exports->BeginFrame && exports->EndFrame
		&& exports->RegisterModel && exports->RegisterShader
		&& exports->RenderScene && exports->DrawStretchPic
		&& exports->CookLightingProject
		&& !exports->CookLightingProject( "/tmp" ) );
	CHECK( getRefApi( REF_API_VERSION, &imports ) == NULL );
	memset( &receipt, 0x5a, sizeof( receipt ) );
	before = receipt;
	CHECK( !getFrameReceipt( &receipt )
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	exports->BeginFrame( STEREO_CENTER );
	CHECK( exports->initFailed && s_initCount == 0u && s_presentCount == 0u );
	exports->Shutdown( REF_UNLOAD_DLL );
	CHECK( s_shutdownCount == 0u );

	exports = getRefApi( REF_API_VERSION, &imports );
	CHECK( exports && !exports->initFailed );
	memset( &config, 0, sizeof( config ) );
	exports->BeginRegistration( &config );
	CHECK( exports->initFailed && s_initCount == 1u && s_shutdownCount == 1u
		&& s_presentCount == 0u );
	exports->Shutdown( REF_UNLOAD_DLL );
	CHECK( s_shutdownCount == 1u );
	CHECK( !getFrameReceipt( &receipt ) );
	CHECK( !frameReceiptExact( &before, &before ) );
	CHECK( dlclose( library ) == 0 );
	puts( "wired OpenGL RAL renderer module ABI: PASS" );
	return 0;
}
