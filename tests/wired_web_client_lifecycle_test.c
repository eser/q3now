// SPDX-License-Identifier: GPL-3.0-or-later

#include "../code/web/web_main.h"
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "../code/render/ral/backends/webgpu/ral_webgpu_browser_emscripten.h"
#include "../code/render/ral/backends/webgpu/ral_webgpu_renderer_module.h"
#include "../code/client/client.h"
#include "../code/client/cl_display_catalog.h"

#include <stdio.h>
#include <string.h>

static int initCalls, frameCalls, shutdownCalls;
static qboolean rendererReady, lastNoDelay;
static char initCommand[512];
static clientApp_t browserApp;
static wiredDisplayCatalog_t activeCatalog;
static wiredDisplayResolutionReceipt_t activeResolution;

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

void Com_Init( char *commandLine ) {
	snprintf( initCommand, sizeof( initCommand ), "%s", commandLine );
	initCalls++;
}
void Com_Frame( qboolean noDelay ) {
	lastNoDelay = noDelay;
	frameCalls++;
}
void Com_Shutdown( void ) { shutdownCalls++; }
qboolean RalWebGpu_BrowserModuleBorrow( ralWebGpuBrowserModuleBorrow_t *out ) {
	if ( !rendererReady ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->runtime = (void *)1; out->presentation = (void *)2;
	out->width = 1280u; out->height = 720u;
	out->configuredReceipt.configured = qtrue;
	out->configuredReceipt.cssWidth = 1280u;
	out->configuredReceipt.cssHeight = 720u;
	out->configuredReceipt.pixelWidth = 1280u;
	out->configuredReceipt.pixelHeight = 720u;
	return qtrue;
}
int WiredDisplay_BuildWebCurrentScreen( uint32_t cssWidth, uint32_t cssHeight,
		uint32_t pixelWidth, uint32_t pixelHeight, uint32_t densityNumerator,
		uint32_t densityDenominator, int fullscreenAvailable,
		wiredDisplayCatalog_t *catalog, wiredDisplayExtentDomains_t *extents ) {
	(void)densityNumerator; (void)densityDenominator; (void)fullscreenAvailable;
	memset( catalog, 0, sizeof( *catalog ) );
	catalog->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	catalog->generation = 1u; catalog->displayCount = catalog->modeCount = 1u;
	memset( extents, 0, sizeof( *extents ) );
	extents->logicalWidth = cssWidth; extents->logicalHeight = cssHeight;
	extents->presentationWidth = extents->renderWidth = extents->uiWidth = pixelWidth;
	extents->presentationHeight = extents->renderHeight = extents->uiHeight = pixelHeight;
	return 1;
}
int WiredDisplay_PublishCatalog( const wiredDisplayCatalog_t *catalog ) {
	activeCatalog = *catalog; return 1;
}
const wiredDisplayCatalog_t *WiredDisplay_GetActiveCatalog( void ) {
	return activeCatalog.displayCount ? &activeCatalog : NULL;
}
int WiredDisplay_BuildResolutionReceipt( uint64_t catalogGeneration,
		uint64_t surfaceGeneration, uint32_t changeFlags,
		uint32_t logicalWidth, uint32_t logicalHeight,
		uint32_t presentationWidth, uint32_t presentationHeight,
		uint32_t renderWidth, uint32_t renderHeight,
		wiredDisplayResolutionReceipt_t *receipt ) {
	memset( receipt, 0, sizeof( *receipt ) );
	receipt->schemaVersion = WIRED_DISPLAY_CATALOG_SCHEMA_VERSION;
	receipt->catalogGeneration = catalogGeneration;
	receipt->surfaceGeneration = surfaceGeneration; receipt->changeFlags = changeFlags;
	receipt->extents.logicalWidth = logicalWidth; receipt->extents.logicalHeight = logicalHeight;
	receipt->extents.presentationWidth = receipt->extents.uiWidth = presentationWidth;
	receipt->extents.presentationHeight = receipt->extents.uiHeight = presentationHeight;
	receipt->extents.renderWidth = renderWidth; receipt->extents.renderHeight = renderHeight;
	return 1;
}
int WiredDisplay_PublishResolutionReceipt(
		const wiredDisplayResolutionReceipt_t *receipt ) {
	if ( receipt->surfaceGeneration <= activeResolution.surfaceGeneration ) return 0;
	activeResolution = *receipt; return 1;
}
qboolean Sys_GetFileStats( const char *path, fileOffset_t *size,
		fileTime_t *mtime, fileTime_t *ctime ) {
	if ( !strcmp( path, "/base/gameclwasm32.wasm" )
			|| !strcmp( path, "/base/gamesvwasm32.wasm" ) ) {
		*size = 4096; *mtime = 1; *ctime = 1;
		return qtrue;
	}
	return qfalse;
}
int FS_ReadFile( const char *qpath, void **buffer ) {
	(void)buffer;
	return !strcmp( qpath, "maps/arena17.bsp" ) ? 4096 : -1;
}
clientApp_t *CL_ActiveApp( void ) { return &browserApp; }
qboolean WiredWebGpu_GetFrameReceipt(
		ralWebGpuRendererModuleFrameReceipt_t *receipt ) {
	memset( receipt, 0, sizeof( *receipt ) );
	receipt->presented = receipt->ready = qtrue;
	receipt->product.worldDrawCount = 1u;
	receipt->product.uiDrawCount = 1u;
	return qtrue;
}
uint32_t WiredWebAuthored_Receipt( void ) { return 31u; }

int main( void ) {
	wiredWebClientReceipt_t receipt;
	CHECK( WiredWeb_ClientStatus() == WIRED_WEB_CLIENT_IDLE );
	CHECK( WiredWeb_AuthoredReceiptProbe() == 0u );
	CHECK( WiredWeb_ClientStart() == 0 );
	CHECK( WiredWeb_ClientStatus() == WIRED_WEB_CLIENT_FAILED );
	WiredWeb_ClientShutdown();
	rendererReady = qtrue;
	CHECK( WiredWeb_ClientStart() == 1 );
	CHECK( initCalls == 1 );
	CHECK( activeResolution.surfaceGeneration == 1u );
	CHECK( strstr( initCommand, "r_customwidth 1280" ) != NULL );
	CHECK( strstr( initCommand, "r_customheight 720" ) != NULL );
	CHECK( WiredWeb_ClientStart() == 1 && initCalls == 1 );
	CHECK( WiredWeb_ClientPresentationChanged() == 1 );
	CHECK( activeResolution.surfaceGeneration == 2u );
	CHECK( WiredWeb_ClientFrame( 10.0 ) == 1 );
	CHECK( WiredWeb_ClientFrame( 9.0 ) == 0 );
	CHECK( WiredWeb_ClientFrame( 11.0 ) == 1 );
	CHECK( frameCalls == 2 && lastNoDelay == qfalse );
	CHECK( WiredWeb_ClientReceipt( &receipt ) == 1 );
	CHECK( receipt.frameCount == 2u && receipt.lastFrameTimeMs == 11.0 );
	CHECK( WiredWeb_ContentProbe() == ( WIRED_WEB_GAMECL_MODULE_READY
		| WIRED_WEB_GAMESV_MODULE_READY | WIRED_WEB_ARENA17_CONTENT_READY ) );
	CHECK( WiredWeb_AuthoredReceiptProbe() == 31u );
	browserApp.state = CA_ACTIVE;
	snprintf( browserApp.cl.mapname, sizeof( browserApp.cl.mapname ),
		"maps/arena17.bsp" );
	CHECK( WiredWeb_ArenaReceiptProbe() == ( WIRED_WEB_ARENA_CLIENT_ACTIVE
		| WIRED_WEB_ARENA_MAP_EXACT | WIRED_WEB_ARENA_PRESENTED
		| WIRED_WEB_ARENA_WORLD_SUBMITTED | WIRED_WEB_ARENA_UI_SUBMITTED ) );
	WiredWeb_ClientShutdown(); WiredWeb_ClientShutdown();
	CHECK( shutdownCalls == 1 );
	CHECK( WiredWeb_ClientStatus() == WIRED_WEB_CLIENT_STOPPED );
	puts( "wired Web client lifecycle contract: PASS" );
	return 0;
}
