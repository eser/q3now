// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "web_main.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../client/cl_display_catalog.h"
#include "../render/ral/backends/webgpu/ral_webgpu_browser_emscripten.h"
#include "../render/ral/backends/webgpu/ral_webgpu_renderer_module.h"
#include "web_authored_content.h"

#include <math.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

static wiredWebClientReceipt_t s_client = {
	WIRED_WEB_CLIENT_SCHEMA_VERSION, WIRED_WEB_CLIENT_IDLE, 0u, 0.0
};
static qboolean s_comInitialized;
static uint64_t s_webSurfaceGeneration;
#ifdef __EMSCRIPTEN__
uint32_t WiredWebUi_TextReceipt( void );
uint32_t WiredWebUi_HudReceipt( void );
uint32_t WiredWebUi_MenuSemanticReceipt( void );
uint32_t WiredWebClay_Receipt( void );
uint32_t WiredWebUi_HudExtent( void );
uint32_t WiredWebUi_ServerReceipt( void );
uint32_t WiredWebUi_ServerRowCenter( void );
void WiredWebUi_EnableServerFixture( int count );
uint32_t WiredWebUi_LayerReceipt( void );
void WiredWebUi_ActivateLayerFixture( int kind );
uint32_t WiredWebClay_ServerLayoutP99Micros( void );
int WiredUI_GetMenuStackDepth( void );
int WiredWebUi_RootCount( void );
int WiredWebUi_ActivateRoot( int index );
uint32_t WiredWebUi_RootReceipt( int index );
void WiredWebUi_ResetBridge( void );
int WiredWebUi_MenuActive( void );
#endif

static qboolean WiredWeb_PublishPresentationResolution( uint32_t changeFlags ) {
	ralWebGpuBrowserModuleBorrow_t browser;
	wiredDisplayCatalog_t catalog;
	wiredDisplayExtentDomains_t extents;
	wiredDisplayResolutionReceipt_t resolution;
	const wiredDisplayCatalog_t *activeCatalog;
	uint64_t nextSurfaceGeneration;

	memset( &browser, 0, sizeof( browser ) );
	if ( !RalWebGpu_BrowserModuleBorrow( &browser )
			|| !browser.configuredReceipt.configured
			|| !WiredDisplay_BuildWebCurrentScreen(
				browser.configuredReceipt.cssWidth,
				browser.configuredReceipt.cssHeight,
				browser.configuredReceipt.pixelWidth,
				browser.configuredReceipt.pixelHeight, 1u, 1u, 0,
				&catalog, &extents )
			|| WiredDisplay_PublishCatalog( &catalog ) < 0 ) return qfalse;
	activeCatalog = WiredDisplay_GetActiveCatalog();
	nextSurfaceGeneration = s_webSurfaceGeneration + 1u;
	if ( !activeCatalog || nextSurfaceGeneration == 0u
			|| !WiredDisplay_BuildResolutionReceipt( activeCatalog->generation,
				nextSurfaceGeneration, changeFlags,
				extents.logicalWidth, extents.logicalHeight,
				extents.presentationWidth, extents.presentationHeight,
				extents.renderWidth, extents.renderHeight, &resolution )
			|| !WiredDisplay_PublishResolutionReceipt( &resolution ) ) return qfalse;
	s_webSurfaceGeneration = nextSurfaceGeneration;
	return qtrue;
}

EMSCRIPTEN_KEEPALIVE int WiredWeb_ClientStart( void ) {
	ralWebGpuBrowserModuleBorrow_t browser;
	/* Com_Init tokenizes this buffer in place. Keep browser defaults explicit;
	 * display enumeration and window ownership do not belong to this target. */
	char commandLine[] =
		"+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 "
		"+set r_customheight 720 +set r_modeFullscreen -2 "
		"+set vm_game 0 +set vm_cgame 0 +set sv_pure 0 +map arena17";

	if ( s_client.status == WIRED_WEB_CLIENT_RUNNING ) return 1;
	if ( s_client.status != WIRED_WEB_CLIENT_IDLE
			&& s_client.status != WIRED_WEB_CLIENT_STOPPED ) return 0;
	memset( &browser, 0, sizeof( browser ) );
	if ( !RalWebGpu_BrowserModuleBorrow( &browser ) || !browser.runtime
			|| !browser.presentation || browser.width != 1280u
			|| browser.height != 720u ) {
		s_client.status = WIRED_WEB_CLIENT_FAILED;
		return 0;
	}

	Com_Init( commandLine );
	s_comInitialized = qtrue;
#ifdef __EMSCRIPTEN__
	WiredWebUi_ResetBridge();
#endif
	if ( !WiredWeb_PublishPresentationResolution(
			WIRED_DISPLAY_CHANGE_TOPOLOGY | WIRED_DISPLAY_CHANGE_EXTENT ) ) {
		Com_Shutdown();
		s_comInitialized = qfalse;
		s_client.status = WIRED_WEB_CLIENT_FAILED;
		return 0;
	}
	s_client.schemaVersion = WIRED_WEB_CLIENT_SCHEMA_VERSION;
	s_client.status = WIRED_WEB_CLIENT_RUNNING;
	s_client.frameCount = 0u;
	s_client.lastFrameTimeMs = 0.0;
	return 1;
}

EMSCRIPTEN_KEEPALIVE int WiredWeb_ClientFrame( double frameTimeMs ) {
	if ( s_client.status != WIRED_WEB_CLIENT_RUNNING
			|| !isfinite( frameTimeMs ) || frameTimeMs < 0.0
			|| ( s_client.frameCount != 0u
				&& frameTimeMs < s_client.lastFrameTimeMs ) ) return 0;
	Com_Frame( qfalse );
	s_client.frameCount++;
	s_client.lastFrameTimeMs = frameTimeMs;
	return 1;
}

EMSCRIPTEN_KEEPALIVE void WiredWeb_ClientShutdown( void ) {
	if ( s_comInitialized ) {
		Com_Shutdown();
		s_comInitialized = qfalse;
	}
	if ( s_client.status != WIRED_WEB_CLIENT_IDLE )
		s_client.status = WIRED_WEB_CLIENT_STOPPED;
}

EMSCRIPTEN_KEEPALIVE int WiredWeb_ClientStatus( void ) {
	return (int)s_client.status;
}

EMSCRIPTEN_KEEPALIVE int WiredWeb_ClientPresentationChanged( void ) {
	if ( !s_comInitialized || s_client.status != WIRED_WEB_CLIENT_RUNNING ) return 0;
	return WiredWeb_PublishPresentationResolution( WIRED_DISPLAY_CHANGE_EXTENT );
}

int WiredWeb_ClientReceipt( wiredWebClientReceipt_t *outReceipt ) {
	if ( !outReceipt ) return 0;
	*outReceipt = s_client;
	return 1;
}

static qboolean WiredWeb_ContentFileReady( const char *path ) {
	fileOffset_t size = 0;
	fileTime_t mtime = 0;
	fileTime_t ctime = 0;
	return Sys_GetFileStats( path, &size, &mtime, &ctime ) && size > 0
		? qtrue : qfalse;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_ContentProbe( void ) {
	uint32_t flags = 0u;
	if ( !s_comInitialized ) return 0u;
	// This receipt must be observational: opening and closing Emscripten side
	// modules here invalidates dynamic-linker function-table ownership before
	// the VM loader performs the authoritative dlopen/dlsym sequence.
	if ( WiredWeb_ContentFileReady( "/base/gameclwasm32.wasm" ) )
		flags |= WIRED_WEB_GAMECL_MODULE_READY;
	if ( WiredWeb_ContentFileReady( "/base/gamesvwasm32.wasm" ) )
		flags |= WIRED_WEB_GAMESV_MODULE_READY;
	if ( FS_ReadFile( "maps/arena17.bsp", NULL ) > 0 )
		flags |= WIRED_WEB_ARENA17_CONTENT_READY;
	return flags;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_ArenaReceiptProbe( void ) {
	const clientApp_t *app;
	ralWebGpuRendererModuleFrameReceipt_t renderer;
	uint32_t flags = 0u;
	if ( !s_comInitialized ) return 0u;
	app = CL_ActiveApp();
	if ( app && app->state == CA_ACTIVE ) flags |= WIRED_WEB_ARENA_CLIENT_ACTIVE;
	if ( app && ( !strcmp( app->cl.mapname, "arena17" )
			|| !strcmp( app->cl.mapname, "maps/arena17.bsp" ) ) )
		flags |= WIRED_WEB_ARENA_MAP_EXACT;
	memset( &renderer, 0, sizeof( renderer ) );
	if ( WiredWebGpu_GetFrameReceipt( &renderer ) ) {
		if ( renderer.presented && renderer.ready )
			flags |= WIRED_WEB_ARENA_PRESENTED;
		if ( renderer.product.worldDrawCount > 0u )
			flags |= WIRED_WEB_ARENA_WORLD_SUBMITTED;
		if ( renderer.product.uiDrawCount > 0u )
			flags |= WIRED_WEB_ARENA_UI_SUBMITTED;
	}
	return flags;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_AuthoredReceiptProbe( void ) {
	return s_comInitialized ? WiredWebAuthored_Receipt() : 0u;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiTextReceiptProbe( void ) {
	ralWebGpuRendererModuleFrameReceipt_t renderer;
	uint32_t flags = 0u;
#ifdef __EMSCRIPTEN__
	if ( s_comInitialized ) flags = WiredWebUi_TextReceipt();
#endif
	memset( &renderer, 0, sizeof( renderer ) );
	if ( WiredWebGpu_GetFrameReceipt( &renderer )
			&& renderer.frontend.uiPrimitiveCount > 16u ) flags |= 0x8u;
	return flags;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiMenuReceiptProbe( void ) {
#ifdef __EMSCRIPTEN__
	uint32_t flags = 0u;
	if ( !s_comInitialized ) return 0u;
	if ( WiredWebUi_MenuActive() ) flags |= 0x1u;
	if ( Key_GetCatcher() & KEYCATCH_UI ) flags |= 0x2u;
	flags |= WiredWebClay_Receipt() << 2;
	flags |= WiredWebUi_MenuSemanticReceipt() << 16;
	return flags;
#endif
	return 0u;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiHudReceiptProbe( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_HudReceipt() : 0u;
#endif
	return 0u;
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiHudExtentProbe( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_HudExtent() : 0u;
#endif
	return 0u;
}

EMSCRIPTEN_KEEPALIVE void WiredWeb_UiEnableServerFixture( int count ) {
#ifdef __EMSCRIPTEN__
	if ( s_comInitialized ) WiredWebUi_EnableServerFixture( count );
#else
	(void)count;
#endif
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiServerReceiptProbe( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_ServerReceipt() : 0u;
#else
	return 0u;
#endif
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiServerRowCenterProbe( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_ServerRowCenter() : 0u;
#else
	return 0u;
#endif
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiServerP99MicrosProbe( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebClay_ServerLayoutP99Micros() : 0u;
#else
	return 0u;
#endif
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiLayerReceiptProbe( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_LayerReceipt() : 0u;
#else
	return 0u;
#endif
}

EMSCRIPTEN_KEEPALIVE void WiredWeb_UiActivateLayerFixture( int kind ) {
#ifdef __EMSCRIPTEN__
	if ( s_comInitialized ) WiredWebUi_ActivateLayerFixture( kind );
#else
	(void)kind;
#endif
}

EMSCRIPTEN_KEEPALIVE int WiredWeb_UiRootCount( void ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_RootCount() : 0;
#else
	return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE int WiredWeb_UiActivateRoot( int index ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_ActivateRoot( index ) : 0;
#else
	(void)index;
	return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE uint32_t WiredWeb_UiRootReceiptProbe( int index ) {
#ifdef __EMSCRIPTEN__
	return s_comInitialized ? WiredWebUi_RootReceipt( index ) : 0u;
#else
	(void)index;
	return 0u;
#endif
}
