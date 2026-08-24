// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "web_main.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
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
