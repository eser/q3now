// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <SDL3/SDL.h>
#if defined(__APPLE__)
#	include <SDL3/SDL_metal.h>
#endif
#ifdef USE_VULKAN_API
#	include "../render/ral/backends/vulkan/include/vulkan/vulkan.h"
#	include <SDL3/SDL_vulkan.h>
#endif
#ifdef _WIN32
// HGLOBAL / OpenClipboard / GlobalAlloc / SetClipboardData are used directly
// in Sys_SetClipboardBitmap below; pull them in explicitly so clang-tidy and
// any future toolchain that doesn't include them transitively still resolves.
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	undef WIN32_LEAN_AND_MEAN
#endif

#define MINSDL_MAJOR 3
#define MINSDL_MINOR 2
#define MINSDL_MICRO 0

#include "../client/client.h"
#include "../render/frontend/tr_public.h"
#include "../qcommon/wired/stalltrace.h"
#include "sdl_glw.h"
#include "sdl_icon.h"
LOG_DECLARE_CHANNEL( ch_client, "client" );

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_FATAL_ERROR,
	RSERR_UNKNOWN
} rserr_t;

typedef enum {
	WIRED_WINDOW_API_OPENGL = 1,
	WIRED_WINDOW_API_VULKAN,
	WIRED_WINDOW_API_METAL,
	WIRED_WINDOW_API_OPENGL46
} wiredWindowApi_t;

static qboolean WindowApiIsOpenGl( wiredWindowApi_t api ) {
	return api == WIRED_WINDOW_API_OPENGL || api == WIRED_WINDOW_API_OPENGL46;
}

glwstate_t glw_state;

SDL_Window *SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;
static ralPresentationHostReceipt_t s_ralPresentationReceipt;
#if defined(__APPLE__)
static SDL_MetalView s_ralMetalView = NULL;
static uint64_t s_ralPresentationGeneration;
static glconfig_t s_ralPresentationConfig;
#endif
#ifdef USE_VULKAN_API
static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
#endif

cvar_t *r_stereoEnabled;

#if defined(__APPLE__) && defined(USE_VULKAN_API)
static cvar_t *r_metalHUD;
#endif

static void RALimp_DestroyMetalView( void ) {
#if defined(__APPLE__)
	if ( s_ralMetalView ) {
		SDL_Metal_DestroyView( s_ralMetalView );
		s_ralMetalView = NULL;
	}
#endif
}

static void RALimp_ForgetPresentationReceipt( void ) {
	memset( &s_ralPresentationReceipt, 0,
		sizeof( s_ralPresentationReceipt ) );
}

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	// Skip the cursor recenter under com_automated: a non-interactive run must
	// never move the user's real cursor, even on shutdown.
	if ( glw_state.isFullscreen && !( com_automated && com_automated->integer ) ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			// NOLINTNEXTLINE(bugprone-integer-division) — pixel-aligned screen-center coordinates; integer math intentional
			SDL_WarpMouseGlobal( (float)(glw_state.desktop_width / 2), (float)(glw_state.desktop_height / 2) );
		} else {
			SDL_ShowCursor();
		}
	}

	if ( SDL_window ) {
		RALimp_DestroyMetalView();
		RALimp_ForgetPresentationReceipt();
		STALLTRACE( "SDL_DestroyWindow", SDL_DestroyWindow( SDL_window ) );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		STALLTRACE( "SDL_QuitSubSystem(VIDEO)", SDL_QuitSubSystem( SDL_INIT_VIDEO ) );
}


/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize( void )
{
	SDL_MinimizeWindow( SDL_window );
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( const char *comment )
{
}


/*
===============
FindNearestDisplay

Returns the SDL_DisplayID nearest to the given window position and size.
Returns 0 if no displays are found.
===============
*/
static SDL_DisplayID FindNearestDisplay( int *x, int *y, int w, int h )
{
	const int cx = *x + w / 2;
	const int cy = *y + h / 2;
	int i, numDisplays;
	SDL_DisplayID *displays;
	SDL_Rect *list, *m;
	SDL_DisplayID result = 0;
	int resultIdx = -1;

	displays = SDL_GetDisplays( &numDisplays );
	if ( !displays || numDisplays <= 0 )
	{
		SDL_free( displays );
		return 0;
	}

	glw_state.monitorCount = numDisplays;

	list = Z_Malloc( numDisplays * sizeof( list[0] ) );

	for ( i = 0; i < numDisplays; i++ )
	{
		SDL_GetDisplayBounds( displays[i], list + i );
	}

	// select display by window center intersection
	for ( i = 0; i < numDisplays; i++ )
	{
		m = list + i;
		if ( cx >= m->x && cx < (m->x + m->w) && cy >= m->y && cy < (m->y + m->h) )
		{
			resultIdx = i;
			break;
		}
	}

	// select display by nearest distance between window center and display center
	if ( resultIdx == -1 )
	{
		unsigned long nearest, dist;
		int dx, dy;
		nearest = ~0UL;
		for ( i = 0; i < numDisplays; i++ )
		{
			m = list + i;
			dx = (m->x + m->w/2) - cx;
			dy = (m->y + m->h/2) - cy;
			dist = ( dx * dx ) + ( dy * dy );
			if ( dist < nearest )
			{
				nearest = dist;
				resultIdx = i;
			}
		}
	}

	// adjust x and y coordinates if needed
	if ( resultIdx >= 0 )
	{
		m = list + resultIdx;
		if ( *x < m->x )
			*x = m->x;

		if ( *y < m->y )
			*y = m->y;

		result = displays[resultIdx];
	}

	Z_Free( list );
	SDL_free( displays );

	return result;
}


static SDL_HitTestResult SDL_HitTestFunc( SDL_Window *win, const SDL_Point *area, void *data )
{
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE && keys[ K_ALT ].down )
		return SDL_HITTEST_DRAGGABLE;

	return SDL_HITTEST_NORMAL;
}


/*
===============
GLimp_SetMode
===============
*/
static int GLW_SetMode( int mode, const char *modeFS, qboolean fullscreen,
		wiredWindowApi_t windowApi )
{
	glconfig_t *config = glw_state.config;
	int perChannelColorBits;
	int colorBits, depthBits, stencilBits;
	int i;
	SDL_DisplayID displayID;
	uint32_t semanticWidth = 0u;
	uint32_t semanticHeight = 0u;
	uint32_t semanticRefreshNumerator = 0u;
	uint32_t semanticRefreshDenominator = 0u;
	const char *semanticMode = Cvar_VariableString( "r_outputMode" );
	const char *semanticRefresh = Cvar_VariableString( "r_outputRefresh" );
	const char *windowPolicy = Cvar_VariableString( "r_windowPolicy" );
	const char *outputSelector = Cvar_VariableString( "r_output" );
	qboolean borderless = r_noborder->integer ? qtrue : qfalse;
	qboolean exclusive = fullscreen;
	int x;
	int y;
	SDL_WindowFlags flags = 0; // SDL3: windows are shown by default; SDL_WINDOW_SHOWN removed

	if ( windowPolicy && windowPolicy[0] ) {
		if ( !Q_stricmp( windowPolicy, "windowed" ) ) {
			fullscreen = qfalse; borderless = qfalse; exclusive = qfalse;
		} else if ( !Q_stricmp( windowPolicy, "borderless" ) ) {
			fullscreen = qtrue; borderless = qtrue; exclusive = qfalse;
		} else if ( !Q_stricmp( windowPolicy, "exclusive" ) ) {
			fullscreen = qtrue; borderless = qfalse; exclusive = qtrue;
		} else {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"Unknown r_windowPolicy '%s'; using compatibility cvars\n", windowPolicy );
		}
	}

	if ( windowApi == WIRED_WINDOW_API_VULKAN ) {
#ifdef USE_VULKAN_API
		flags |= SDL_WINDOW_VULKAN;
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Initializing Vulkan display\n");
#else
		return RSERR_FATAL_ERROR;
#endif
	} else if ( windowApi == WIRED_WINDOW_API_METAL ) {
#if defined(__APPLE__)
		flags |= SDL_WINDOW_METAL;
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Initializing Metal display\n" );
#else
		return RSERR_FATAL_ERROR;
#endif
	} else {
		flags |= SDL_WINDOW_OPENGL;
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Initializing OpenGL display\n");
	}

	// Request a full physical-pixel backing on HiDPI displays. Without this flag a
	// window on a scaled (e.g. 2x) display is backed at the logical (downscaled)
	// resolution, so SDL_GetWindowSizeInPixels would report the logical size and the
	// swapchain/viewport (which read pixel size) would render below native
	// resolution. With it, pixel size is the true physical size and the render path
	// runs at full resolution. On a 1x display logical == pixel, so this is a no-op.
	flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

	// If a window exists, note its display
	if ( SDL_window != NULL )
	{
		displayID = SDL_GetDisplayForWindow( SDL_window );
		if ( displayID == 0 )
		{
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_GetDisplayForWindow() failed: %s\n", SDL_GetError() );
		}
	}
	else
	{
		x = vid_xpos->integer;
		y = vid_ypos->integer;
		displayID = GLimp_ResolveConfiguredDisplay();
		if ( displayID != 0 && outputSelector && outputSelector[0] ) {
			SDL_Rect targetBounds;
			if ( SDL_GetDisplayBounds( displayID, &targetBounds ) ) {
				x = targetBounds.x + 32;
				y = targetBounds.y + 32;
			}
		}

		// find out to which display our window belongs to
		// according to previously stored \vid_xpos and \vid_ypos coordinates
		if ( displayID == 0 ) displayID = FindNearestDisplay( &x, &y, 1280, 720 );

		//Com_Log( SEV_INFO, LOG_CH(ch_client), "Selected display: %u\n", displayID );
	}

	if ( displayID != 0 )
	{
		const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode( displayID );
		if ( dm )
		{
			glw_state.desktop_width = dm->w;
			glw_state.desktop_height = dm->h;
		}
		else
		{
			// Use a practical first-launch fallback when the desktop query fails.
			glw_state.desktop_width = 1280;
			glw_state.desktop_height = 720;
		}
	}
	else
	{
		// No display was resolved; retain the same practical fallback.
		glw_state.desktop_width = 1280;
		glw_state.desktop_height = 720;
	}

	// For BORDERED windowed mode, clamp to usable display area so the
	// window does not sit behind the menu bar, taskbar, or dock.
	//
	// window-input-fix STEP 1(a) — gate on !r_noborder as well as !fullscreen.
	// A borderless window (`r_noborder=1`) is fake-fullscreen intent: it must
	// cover the menu bar / dock, not sit inside the usable strip. Without this
	// gate, a macOS borderless window sized to the usable region (clamped here)
	// + positioned at (vid_xpos,vid_ypos) leaves desktop visible at the edges.
	// Bordered-windowed mode keeps the clamp — it still wants to stay inside
	// the OS chrome.
	if ( !fullscreen && !borderless && displayID != 0 )
	{
		SDL_Rect bounds, usable;
		if ( SDL_GetDisplayBounds( displayID, &bounds ) && SDL_GetDisplayUsableBounds( displayID, &usable ) )
		{
			int displayBottom = bounds.y + bounds.h;
			int usableBottom = usable.y + usable.h;
			int effectiveBottom = ( usableBottom < displayBottom ) ? usableBottom : displayBottom;
			int availW = usable.w;
			int availH = effectiveBottom - usable.y;

			if ( availW > 0 && availW < glw_state.desktop_width )
				glw_state.desktop_width = availW;
			if ( availH > 0 && availH < glw_state.desktop_height )
				glw_state.desktop_height = availH;
		}
	}

	config->isFullscreen = fullscreen;
	glw_state.isFullscreen = fullscreen;

	Com_Log( SEV_INFO, LOG_CH(ch_client), "...setting mode %d:", mode );

	if ( semanticMode && !Q_stricmp( semanticMode, "desktop" ) ) {
		mode = -2;
		modeFS = "";
	} else if ( semanticMode && !Q_stricmp( semanticMode, "custom" ) ) {
		mode = -1;
		modeFS = "";
	}
	if ( semanticMode && WiredDisplay_ParseModeValue( semanticMode,
		&semanticWidth, &semanticHeight, &semanticRefreshNumerator,
		&semanticRefreshDenominator ) ) {
		config->vidWidth = (int)semanticWidth;
		config->vidHeight = (int)semanticHeight;
		config->windowAspect = (float)semanticWidth / (float)semanticHeight;
	} else if ( !CL_GetModeInfo( &config->vidWidth, &config->vidHeight, &config->windowAspect, mode, modeFS, glw_state.desktop_width, glw_state.desktop_height, fullscreen ) )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	if ( semanticRefresh && semanticRefresh[0]
			&& Q_stricmp( semanticRefresh, "auto" )
			&& !WiredDisplay_ParseRefreshValue( semanticRefresh,
				&semanticRefreshNumerator, &semanticRefreshDenominator ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"Invalid r_outputRefresh '%s'; using automatic refresh\n", semanticRefresh );
		semanticRefreshNumerator = semanticRefreshDenominator = 0u;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_client), " %d %d\n", config->vidWidth, config->vidHeight );

	// Destroy existing state if it exists
	if ( SDL_glContext != NULL )
	{
		SDL_GL_DestroyContext( SDL_glContext );
		SDL_glContext = NULL;
	}

	if ( SDL_window != NULL )
	{
		SDL_GetWindowPosition( SDL_window, &x, &y );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Existing window at %dx%d before being destroyed\n", x, y );
		RALimp_DestroyMetalView();
		RALimp_ForgetPresentationReceipt();
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	gw_active = qfalse;
	gw_minimized = qtrue;

	if ( fullscreen )
	{
		// SDL3: SDL_WINDOW_FULLSCREEN is now always "desktop fullscreen" by default.
		// Exclusive fullscreen is achieved by setting a mode with SDL_SetWindowFullscreenMode().
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	else if ( borderless )
	{
		flags |= SDL_WINDOW_BORDERLESS;
	}

	colorBits = r_colorbits->value;

	if ( colorBits == 0 || colorBits > 24 )
		colorBits = 24;

	if ( cl_depthbits->integer == 0 )
	{
		// implicitly assume Z-buffer depth == desktop color depth
		if ( colorBits > 16 )
			depthBits = 24;
		else
			depthBits = 16;
	}
	else
		depthBits = cl_depthbits->integer;

	stencilBits = cl_stencilbits->integer;

	// do not allow stencil if Z-buffer depth likely won't contain it
	if ( depthBits < 24 )
		stencilBits = 0;

	for ( i = 0; i < 16; i++ )
	{
		int testColorBits, testDepthBits, testStencilBits;
		int realColorBits[3];

		// 0 - default
		// 1 - minus colorBits
		// 2 - minus depthBits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorBits == 24)
						colorBits = 16;
					break;
				case 1 :
					if (depthBits == 24)
						depthBits = 16;
					else if (depthBits == 16)
						depthBits = 8;
				case 3 :
					if (stencilBits == 24)
						stencilBits = 16;
					else if (stencilBits == 16)
						stencilBits = 8;
			}
		}

		testColorBits = colorBits;
		testDepthBits = depthBits;
		testStencilBits = stencilBits;

		if ((i % 4) == 3)
		{ // reduce colorBits
			if (testColorBits == 24)
				testColorBits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthBits
			if (testDepthBits == 24)
				testDepthBits = 16;
		}

		if ((i % 4) == 1)
		{ // reduce stencilBits
			if (testStencilBits == 8)
				testStencilBits = 0;
		}

		if ( testColorBits == 24 )
			perChannelColorBits = 8;
		else
			perChannelColorBits = 4;

		if ( WindowApiIsOpenGl( windowApi ) ) {

#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
			if (perChannelColorBits == 4)
				perChannelColorBits = 0; /* Use minimum size for 16-bit color */

			/* Need alpha or else SGIs choose 36+ bit RGB mode */
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1 );
#endif

			SDL_GL_SetAttribute( SDL_GL_RED_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, testDepthBits );
			SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, testStencilBits );

			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );

			if ( r_stereoEnabled->integer )
			{
				config->stereoEnabled = qtrue;
				SDL_GL_SetAttribute( SDL_GL_STEREO, 1 );
			}
			else
			{
				config->stereoEnabled = qfalse;
				SDL_GL_SetAttribute( SDL_GL_STEREO, 0 );
			}

			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

			if ( !r_allowSoftwareGL->integer )
				SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
		}

		// SDL3: SDL_CreateWindow no longer takes x, y params. Create the window
		// first, then set its position below.
		STALLTRACE( "SDL_CreateWindow",
			SDL_window = SDL_CreateWindow( cl_title, config->vidWidth, config->vidHeight, flags ) );
		if ( SDL_window == NULL )
		{
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_CreateWindow failed: %s\n", SDL_GetError() );
			continue;
		}

		if ( !fullscreen )
		{
			// window-input-fix STEP 1(b) — position borderless windows at the
			// display origin. Borderless = fake-fullscreen intent; the window
			// covers the display from (0,0) to (desktop_w, desktop_h). The
			// (vid_xpos, vid_ypos) defaults (3, 22) were sized for bordered
			// windows to clear the title bar — leaving them in place for
			// borderless produces visible desktop strips at the top + left edges.
			// Bordered-windowed mode keeps the saved (vid_xpos, vid_ypos).
			if ( borderless )
				SDL_SetWindowPosition( SDL_window, 0, 0 );
			else
				SDL_SetWindowPosition( SDL_window, x, y );

			// Resize if the window extends beyond the display.
			// window-input-fix STEP 1(c) — skip the shrink for borderless.
			// The overflow check measures (winY + borderTop + contentH) against
			// the full display bottom (SDL_GetDisplayBounds, NOT usable). For a
			// borderless window at (0,0) sized to the full display, overflow
			// resolves to 0 — but SDL_GetWindowBordersSize behaviour on
			// borderless windows is platform-quirky (macOS in particular has
			// reported non-zero values for compositor-decorated borderless
			// surfaces in some SDL3 builds), and a stray non-zero borderTop
			// would re-introduce the size regression by shrinking the window.
			// Skip the block entirely for borderless — the fake-fullscreen
			// sizing is already exact.
			if ( !borderless )
			{
				SDL_DisplayID winDisplay = SDL_GetDisplayForWindow( SDL_window );
				SDL_Rect dBounds;
				if ( winDisplay != 0 && SDL_GetDisplayBounds( winDisplay, &dBounds ) )
				{
					int screenBottom = dBounds.y + dBounds.h;
					int winY, contentH, borderTop = 0;
					SDL_GetWindowPosition( SDL_window, NULL, &winY );
					SDL_GetWindowSize( SDL_window, NULL, &contentH );
					SDL_GetWindowBordersSize( SDL_window, &borderTop, NULL, NULL, NULL );
					int overflow = ( winY + borderTop + contentH ) - screenBottom;
					if ( overflow > 0 && contentH - overflow > 240 )
					{
						int fittedHeight = contentH - overflow;
						/* Preserve the user's requested aspect ratio while fitting
						 * the decorated window below the display edge. */
						if ( config->vidHeight > 0 )
							config->vidWidth = (int)( (int64_t)config->vidWidth
								* (int64_t)fittedHeight / config->vidHeight );
						config->vidHeight = fittedHeight;
						SDL_SetWindowSize( SDL_window, config->vidWidth, config->vidHeight );
					}
				}
			}
		}

		if ( fullscreen )
		{
			if ( !exclusive || !WindowApiIsOpenGl( windowApi ) ) {
				// Vulkan: desktop fullscreen (SDL_WINDOW_FULLSCREEN default) is sufficient —
				// the swapchain handles resolution and format independently.
				// Exclusive mode with SDL_SetWindowFullscreenMode fails on macOS/MoltenVK
				// because SDL_PIXELFORMAT_RGB24 is not a valid display mode format.
			} else {
				SDL_DisplayMode fsMode;
				SDL_zero( fsMode );

				switch ( testColorBits )
				{
					case 16: fsMode.format = SDL_PIXELFORMAT_RGB565;  break;
					case 24: fsMode.format = SDL_PIXELFORMAT_XRGB8888; break;
					default: Com_Log( SEV_DEBUG, LOG_CH(ch_client), "testColorBits is %d, can't fullscreen\n", testColorBits );
						SDL_DestroyWindow( SDL_window );
						SDL_window = NULL;
						continue;
				}

				fsMode.w = config->vidWidth;
				fsMode.h = config->vidHeight;
				fsMode.displayID = displayID;
				if ( semanticRefreshNumerator != 0u ) {
					fsMode.refresh_rate_numerator = (int)semanticRefreshNumerator;
					fsMode.refresh_rate_denominator = (int)semanticRefreshDenominator;
					fsMode.refresh_rate = (float)semanticRefreshNumerator
						/ (float)semanticRefreshDenominator;
				} else {
					// SDL3 retains the float field for compatibility with integer overrides.
					fsMode.refresh_rate = (float)Cvar_VariableIntegerValue( "r_displayRefresh" );
				}

				if ( !SDL_SetWindowFullscreenMode( SDL_window, &fsMode ) )
				{
					/* Preserve the requested exclusive policy, but atomically use
					 * target-desktop borderless as the effective runtime fallback. */
					Com_Log( SEV_WARN, LOG_CH(ch_client),
						"exclusive mode unavailable (%s); using borderless desktop until next Apply\n",
						SDL_GetError() );
					exclusive = qfalse;
					SDL_SetWindowFullscreenMode( SDL_window, NULL );
				}
			}

			// SDL3: SDL_GetWindowFullscreenMode returns const pointer (NULL on error)
			{
				const SDL_DisplayMode *curMode = SDL_GetWindowFullscreenMode( SDL_window );
				if ( curMode )
				{
					config->displayFrequency = (int)curMode->refresh_rate;
					config->vidWidth = curMode->w;
					config->vidHeight = curMode->h;
				}
			}
		}

		if ( !WindowApiIsOpenGl( windowApi ) ) {
			config->colorBits = testColorBits;
			config->depthBits = testDepthBits;
			config->stencilBits = testStencilBits;
		} else {
			if ( !SDL_glContext )
			{
				if ( ( SDL_glContext = SDL_GL_CreateContext( SDL_window ) ) == NULL )
				{
					Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_GL_CreateContext failed: %s\n", SDL_GetError( ) );
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
				}
			}

			// SDL3: SDL_GL_SetSwapInterval returns bool (true = success)
			if ( !SDL_GL_SetSwapInterval( r_swapInterval->integer ) )
			{
				Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError( ) );
			}

			SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &realColorBits[0] );
			SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &realColorBits[1] );
			SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &realColorBits[2] );
			SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &config->depthBits );
			SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &config->stencilBits );

			config->colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];
		} // OpenGL context


		Com_Log( SEV_INFO, LOG_CH(ch_client), "Using %d color bits, %d depth, %d stencil display.\n",	config->colorBits, config->depthBits, config->stencilBits );

		break;
	}

	if ( SDL_window )
	{
#ifdef USE_ICON
		// SDL3: SDL_CreateRGBSurfaceFrom removed; use SDL_CreateSurfaceFrom
		SDL_Surface *icon = SDL_CreateSurfaceFrom(
			CLIENT_WINDOW_ICON.width,
			CLIENT_WINDOW_ICON.height,
			SDL_PIXELFORMAT_RGBA32,
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width
		);
		if ( icon )
		{
			SDL_SetWindowIcon( SDL_window, icon );
			SDL_DestroySurface( icon );
		}
#endif
		{
			int logicalWidth = 0, logicalHeight = 0;
			int pixelWidth = 0, pixelHeight = 0;
			SDL_GetWindowSize( SDL_window, &logicalWidth, &logicalHeight );
			SDL_GetWindowSizeInPixels( SDL_window, &pixelWidth, &pixelHeight );
			Com_Log( SEV_INFO, LOG_CH(ch_client),
				"window-extent schema=3 requested=%dx%d logical=%dx%d pixels=%dx%d publish-ready=1\n",
				config->vidWidth, config->vidHeight,
				logicalWidth, logicalHeight, pixelWidth, pixelHeight );
		}
	}
	else
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	if ( !fullscreen && borderless )
		SDL_SetWindowHitTest( SDL_window, SDL_HitTestFunc, NULL );

	// SDL3: SDL_GetWindowSizeInPixels replaces both SDL_GL_GetDrawableSize and SDL_Vulkan_GetDrawableSize
	SDL_GetWindowSizeInPixels( SDL_window, &config->vidWidth, &config->vidHeight );

	// Logical (DPI-independent) point size — drives dpiScale in the WiredUI
	// compositor (dpiScale = vidWidth / vidWidthLogical). On a non-HiDPI display
	// this equals the physical pixel size and dpiScale comes out 1.0.
	SDL_GetWindowSize( SDL_window, &config->vidWidthLogical, &config->vidHeightLogical );

	// save render dimensions as renderer may change it in advance
	glw_state.window_width = config->vidWidth;
	glw_state.window_height = config->vidHeight;

	// SDL_WarpMouseInWindow takes window LOGICAL coordinates (not physical pixels),
	// so recenter on the logical size — on a HiDPI display warping to pixel/2 would
	// land at twice the intended position (off-window). At scale 1.0 logical ==
	// pixel, so this is the same target as before. Skipped under com_automated: a
	// non-interactive run must never move the user's real cursor at window create.
	if ( !( com_automated && com_automated->integer ) )
	{
		// NOLINTNEXTLINE(bugprone-integer-division) — window-center coordinates; integer math intentional
		SDL_WarpMouseInWindow( SDL_window, (float)(config->vidWidthLogical / 2), (float)(config->vidHeightLogical / 2) );
	}

	return RSERR_OK;
}


/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static rserr_t GLimp_StartDriverAndSetMode( int mode, const char *modeFS,
		qboolean fullscreen, wiredWindowApi_t windowApi )
{
	rserr_t err;
	const qboolean automated = com_automated && com_automated->integer;

#if defined(__APPLE__) && defined(USE_VULKAN_API)
	// MoltenVK reads this process setting while its Metal objects are created.
	// Keep the diagnostic opt-in and Vulkan-only; canonical performance runs
	// therefore carry no HUD observer overhead unless explicitly requested.
	if ( windowApi == WIRED_WINDOW_API_VULKAN )
	{
		setenv( "MTL_HUD_ENABLED", r_metalHUD && r_metalHUD->integer ? "1" : "0", 1 );
	}
#endif

	// An automated (non-interactive) run stays windowed: fullscreen forces the
	// window to the foreground, which defeats the unfocused-background intent.
	if ( fullscreen && automated )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Fullscreen not used with \\com_automated 1 (automated runs stay windowed + unfocused)\n");
		Cvar_Set( "r_fullscreen", "0" );
		fullscreen = qfalse;
	}

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		const char *driverName;

#ifdef __linux__
		// SDL3: prefer Wayland over X11 on Linux
		SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "wayland,x11" );
#endif

#if defined(__APPLE__) && defined(USE_VULKAN_API)
		// SDL3: set MoltenVK path via hint BEFORE SDL_Init so SDL can find it.
		// Homebrew installs MoltenVK separately; the bundled copy (from make install)
		// lives next to the executable in Contents/MacOS/.
		if ( windowApi == WIRED_WINDOW_API_VULKAN )
		{
			static char moltenVKPath[ MAX_OSPATH ];
			Com_sprintf( moltenVKPath, sizeof( moltenVKPath ), "%s/libMoltenVK.dylib", FS_GetInstallBinaryPath() );
			SDL_SetHint( SDL_HINT_VULKAN_LIBRARY, moltenVKPath );
			Com_Log( SEV_INFO, LOG_CH(ch_client), "SDL Vulkan: requesting MoltenVK from %s\n", moltenVKPath );
			// MoltenVK default: synchronous queue submits (vkQueueSubmit blocks until GPU
			// finishes, serializing CPU+GPU and halving throughput). Force async so the
			// CPU and GPU overlap frames. Respect any explicit user override.
			setenv( "MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", 0 );
			// MoltenVK default: present is encoded into the app's render CB, coupling
			// CAMetalLayer drawable return to the app fence. Setting to 0 uses a separate
			// internal CB for present, freeing nextDrawable for the next acquire immediately
			// after present rather than after fence signal — eliminates acquire stalls.
			setenv( "MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", 0 );
		}
#endif

		// SDL3: SDL_Init returns bool (true = success)
		bool sdlInitOk;
		STALLTRACE( "SDL_Init(VIDEO)", sdlInitOk = SDL_Init( SDL_INIT_VIDEO ) );
		if ( !sdlInitOk )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError() );
			return RSERR_FATAL_ERROR;
		}

		{
			int sdlver = SDL_GetVersion();
			int sdlmaj = SDL_VERSIONNUM_MAJOR( sdlver );
			int sdlmin = SDL_VERSIONNUM_MINOR( sdlver );
			int sdlmic = SDL_VERSIONNUM_MICRO( sdlver );
			if ( sdlver < SDL_VERSIONNUM( MINSDL_MAJOR, MINSDL_MINOR, MINSDL_MICRO ) )
			{
				Com_Terminate( TERM_UNRECOVERABLE, "SDL3 runtime version %d.%d.%d is older than required %d.%d.%d",
					sdlmaj, sdlmin, sdlmic,
					MINSDL_MAJOR, MINSDL_MINOR, MINSDL_MICRO );
			}
			driverName = SDL_GetCurrentVideoDriver();
			Com_Log( SEV_INFO, LOG_CH(ch_client), "SDL version: %d.%d.%d (compiled against %d.%d.%d)\n",
				sdlmaj, sdlmin, sdlmic,
				SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION );
		}
		Com_Log( SEV_INFO, LOG_CH(ch_client), "SDL using driver \"%s\"\n", driverName );
	}
	if ( WindowApiIsOpenGl( windowApi ) ) {
		SDL_GL_ResetAttributes();
		if ( windowApi == WIRED_WINDOW_API_OPENGL46
				&& ( !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 )
					|| !SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 6 )
					|| !SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_CORE )
					|| !SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS,
						SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG ) ) ) {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"SDL OpenGL 4.6 Core attributes rejected: %s\n", SDL_GetError() );
			return RSERR_FATAL_ERROR;
		}
	}

	GLimp_DisplayCatalogMarkDirty( GLIMP_DISPLAY_DIRTY_TOPOLOGY );
	GLimp_DisplayCatalogReconcile();

	err = GLW_SetMode( mode, modeFS, fullscreen, windowApi );
	if ( err == RSERR_OK ) {
		GLimp_DisplayCatalogMarkDirty( GLIMP_DISPLAY_DIRTY_ACTIVE_OUTPUT
			| GLIMP_DISPLAY_DIRTY_SCALE | GLIMP_DISPLAY_DIRTY_FULLSCREEN );
		GLimp_DisplayCatalogReconcile();
	}

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			Com_Log( SEV_INFO, LOG_CH(ch_client), "...WARNING: fullscreen unavailable in this mode\n" );
			return err;
		case RSERR_INVALID_MODE:
			Com_Log( SEV_INFO, LOG_CH(ch_client), "...WARNING: could not set the given mode (%d)\n", mode );
			return err;
		default:
			break;
	}

	return RSERR_OK;
}


/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
static void GLimp_InitForApi( glconfig_t *config, wiredWindowApi_t windowApi )
{
	rserr_t err;

	/* InitSig() used to be called here (and in VKimp_Init below), and it
	 * OVERWROTE the crash handler that Crash_Init -> Sys_InstallCrashHandler
	 * installs earlier in startup. The handler it replaced writes the structured
	 * JSON crash report; the one it installed writes only a text backtrace to a
	 * fixed /tmp path. Renderer init runs after Crash_Init, so the weaker handler
	 * always won: on macOS and Linux a GUI client crash produced NO crash_*.json
	 * at all, while Windows — excluded from this call by the old #ifndef _WIN32 —
	 * did produce one. Measured with the `crash` command: signal 11, text log
	 * only, no report.
	 *
	 * Removed rather than reordered. The crash handler needs exactly one owner,
	 * and renderer startup is not it; a second installer would silently win again
	 * the next time startup order changes. */

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "GLimp_Init()\n" );

	glw_state.config = config; // feedback renderer configuration

	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	{
		static const cvarDesc_t d = CVAR_BOOL( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH,
			"Enable stereo rendering for techniques like shutter glasses." );
	r_stereoEnabled = Cvar_Register( &d );
	}

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string,
		r_fullscreen->integer, windowApi );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Terminate( TERM_UNRECOVERABLE, "GLimp_Init() - could not load OpenGL subsystem" );
			return;
		}

		if ( r_mode->integer != 13 || ( r_fullscreen->integer && atoi( r_modeFullscreen->string ) != 13 ) )
		{
			// Use the modern 1280x720 table entry when the requested mode cannot
			// be created. Valid user-selected legacy/custom modes are not rejected.
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Setting \\r_mode %d failed, falling back on \\r_mode %d\n", r_mode->integer, 13 );
			if ( GLimp_StartDriverAndSetMode( 13, "", r_fullscreen->integer,
					windowApi ) != RSERR_OK )
			{
				// Nothing worked, give up
				Com_Terminate( TERM_UNRECOVERABLE, "GLimp_Init() - could not load OpenGL subsystem" );
				return;
			}
		}
	}

	// These values force the UI to disable driver selection
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	HandleEvents();

	Key_ClearStates();
}

void GLimp_Init( glconfig_t *config ) {
	GLimp_InitForApi( config, WIRED_WINDOW_API_OPENGL );
}

void GLimp_InitOpenGL46( glconfig_t *config ) {
	GLimp_InitForApi( config, WIRED_WINDOW_API_OPENGL46 );
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( cl_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapWindow( SDL_window );
	}
}


/*
===============
GL_GetProcAddress

Used by opengl renderers to resolve all qgl* function pointers
===============
*/
void *GL_GetProcAddress( const char *symbol )
{
	return SDL_GL_GetProcAddress( symbol );
}


#ifdef USE_VULKAN_API
/*
===============
VKimp_Init

This routine is responsible for initializing the OS specific portions
of Vulkan
===============
*/
void VKimp_Init( glconfig_t *config )
{
	rserr_t err;

	/* InitSig() removed — see the note in GLimp_Init above. This was the path
	 * that actually ran for the Vulkan client, i.e. the shipping one. */

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "VKimp_Init()\n" );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	{
		static const cvarDesc_t d = CVAR_BOOL( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH,
			"Enable stereo rendering for techniques like shutter glasses." );
		r_stereoEnabled = Cvar_Register( &d );
	}

#if defined(__APPLE__)
	{
		static const cvarDesc_t d = CVAR_BOOL( "r_metalHUD", "0", CVAR_ARCHIVE | CVAR_LATCH,
			"Enable Apple's Metal Performance HUD for Vulkan diagnostics." );
		r_metalHUD = Cvar_Register( &d );
	}
#endif

	// feedback to renderer configuration
	glw_state.config = config;

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string,
		r_fullscreen->integer, WIRED_WINDOW_API_VULKAN );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Terminate( TERM_UNRECOVERABLE, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}

		// Use the modern 1280x720 table entry when the requested mode cannot
		// be created. Valid user-selected legacy/custom modes are not rejected.
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, 13 );

		err = GLimp_StartDriverAndSetMode( 13, "", r_fullscreen->integer,
			WIRED_WINDOW_API_VULKAN );
		if( err != RSERR_OK )
		{
			// Nothing worked, give up
			Com_Terminate( TERM_UNRECOVERABLE, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}
	}

	// SDL3: SDL_Vulkan_GetVkGetInstanceProcAddr returns SDL_FunctionPointer; cast required
	qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

	if ( qvkGetInstanceProcAddr == NULL )
	{
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		Com_Terminate( TERM_UNRECOVERABLE, "VKimp_Init: qvkGetInstanceProcAddr is NULL" );
	}

	// These values force the UI to disable driver selection
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	HandleEvents();

	Key_ClearStates();
}


/*
===============
VK_GetInstanceProcAddr
===============
*/
void *VK_GetInstanceProcAddr( void *nativeInstance, const char *name )
{
	return qvkGetInstanceProcAddr( (VkInstance)nativeInstance, name );
}

const char *const *VK_GetInstanceExtensions( uint32_t *count )
{
	return SDL_Vulkan_GetInstanceExtensions( count );
}


/*
===============
VK_CreateSurface
===============
*/
qboolean VK_CreateSurface( void *nativeInstance, uint64_t *outNativeSurface )
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	if ( outNativeSurface ) *outNativeSurface = 0;
	if ( !nativeInstance || !outNativeSurface ) return qfalse;
	// SDL3: SDL_Vulkan_CreateSurface adds VkAllocationCallbacks* parameter (pass NULL)
	if ( !SDL_Vulkan_CreateSurface( SDL_window, (VkInstance)nativeInstance,
		NULL, &surface ) || surface == VK_NULL_HANDLE ) return qfalse;
	*outNativeSurface = (uint64_t)(uintptr_t)surface;
	return qtrue;
}


/*
===============
VKimp_Shutdown
===============
*/
void VKimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	// Skip the cursor recenter under com_automated: a non-interactive run must
	// never move the user's real cursor, even on shutdown.
	if ( glw_state.isFullscreen && !( com_automated && com_automated->integer ) ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			// NOLINTNEXTLINE(bugprone-integer-division) — pixel-aligned screen-center coordinates; integer math intentional
			SDL_WarpMouseGlobal( (float)(glw_state.desktop_width / 2), (float)(glw_state.desktop_height / 2) );
		} else {
			SDL_ShowCursor();
		}
	}

	if ( SDL_window ) {
		RALimp_DestroyMetalView();
		RALimp_ForgetPresentationReceipt();
		STALLTRACE( "SDL_DestroyWindow", SDL_DestroyWindow( SDL_window ) );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		STALLTRACE( "SDL_QuitSubSystem(VIDEO)", SDL_QuitSubSystem( SDL_INIT_VIDEO ) );
}
#endif // USE_VULKAN_API

static qboolean RALimp_NextPresentationGeneration( uint64_t *outGeneration ) {
#if defined(__APPLE__)
	if ( !outGeneration || s_ralPresentationGeneration == UINT64_MAX - 1u ) {
		return qfalse;
	}
	*outGeneration = ++s_ralPresentationGeneration;
	return qtrue;
#else
	(void)outGeneration;
	return qfalse;
#endif
}

static qboolean RALimp_BuildPresentationReceipt( uint64_t ownerGeneration,
		uint64_t surfaceGeneration,
		ralPresentationHostReceipt_t *outReceipt ) {
#if defined(__APPLE__)
	ralPresentationHostReceipt_t receipt;
	SDL_WindowFlags flags;
	int logicalWidth = 0, logicalHeight = 0, pixelWidth = 0, pixelHeight = 0;
	if ( !outReceipt || !SDL_window || !s_ralMetalView
			|| !SDL_GetWindowSize( SDL_window, &logicalWidth, &logicalHeight )
			|| !SDL_GetWindowSizeInPixels( SDL_window, &pixelWidth, &pixelHeight )
			|| logicalWidth <= 0 || logicalHeight <= 0
			|| pixelWidth <= 0 || pixelHeight <= 0 ) return qfalse;
	flags = SDL_GetWindowFlags( SDL_window );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL;
	receipt.ownerGeneration = ownerGeneration;
	receipt.surfaceGeneration = surfaceGeneration;
	receipt.ownerIdentity = (uintptr_t)SDL_window;
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
#else
	(void)ownerGeneration; (void)surfaceGeneration; (void)outReceipt;
	return qfalse;
#endif
}

qboolean RALimp_PresentationOpen( void *context,
		const ralPresentationHostOpenInfo_t *info,
		ralPresentationHostReceipt_t *outReceipt ) {
#if defined(__APPLE__)
	ralPresentationHostReceipt_t receipt;
	rserr_t error;
	uint64_t ownerGeneration, surfaceGeneration;
	(void)context;
	if ( !outReceipt || !Ral_PresentationHostOpenInfoValid( info )
			|| info->backendType != RAL_BACKEND_METAL ) return qfalse;
	if ( Ral_PresentationHostReceiptValid( &s_ralPresentationReceipt ) ) {
		if ( info->requestVisible && !s_ralPresentationReceipt.visible ) {
			if ( !SDL_ShowWindow( SDL_window ) || !SDL_SyncWindow( SDL_window ) ) {
				return qfalse;
			}
		} else if ( !info->requestVisible && s_ralPresentationReceipt.visible ) {
			if ( !SDL_HideWindow( SDL_window ) || !SDL_SyncWindow( SDL_window ) ) {
				return qfalse;
			}
		}
		return RALimp_PresentationRefresh( context, &s_ralPresentationReceipt,
			outReceipt );
	}
	if ( SDL_window || s_ralMetalView
			|| !RALimp_NextPresentationGeneration( &ownerGeneration )
			|| !RALimp_NextPresentationGeneration( &surfaceGeneration ) ) return qfalse;
	memset( &s_ralPresentationConfig, 0, sizeof( s_ralPresentationConfig ) );
	glw_state.config = &s_ralPresentationConfig;
	error = GLimp_StartDriverAndSetMode( r_mode->integer,
		r_modeFullscreen->string, r_fullscreen->integer, WIRED_WINDOW_API_METAL );
	if ( error != RSERR_OK ) return qfalse;
	s_ralMetalView = SDL_Metal_CreateView( SDL_window );
	if ( !s_ralMetalView || !SDL_Metal_GetLayer( s_ralMetalView ) ) {
		GLimp_Shutdown( qtrue );
		return qfalse;
	}
	if ( info->requestVisible && ( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_HIDDEN ) ) {
		if ( !SDL_ShowWindow( SDL_window ) || !SDL_SyncWindow( SDL_window ) ) {
			GLimp_Shutdown( qtrue );
			return qfalse;
		}
	}
	IN_Init(); HandleEvents(); Key_ClearStates();
	if ( !RALimp_BuildPresentationReceipt( ownerGeneration, surfaceGeneration,
			&receipt ) ) {
		GLimp_Shutdown( qtrue );
		return qfalse;
	}
	s_ralPresentationReceipt = receipt;
	*outReceipt = receipt;
	return qtrue;
#else
	(void)context; (void)info; (void)outReceipt;
	return qfalse;
#endif
}

qboolean RALimp_PresentationRefresh( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationHostReceipt_t *outReceipt ) {
#if defined(__APPLE__)
	ralPresentationHostReceipt_t candidate;
	uint64_t nextGeneration;
	(void)context;
	if ( !outReceipt
			|| !Ral_PresentationHostReceiptExact( currentReceipt,
				&s_ralPresentationReceipt )
			|| !RALimp_BuildPresentationReceipt( currentReceipt->ownerGeneration,
				currentReceipt->surfaceGeneration, &candidate ) ) return qfalse;
	if ( candidate.logicalWidth != currentReceipt->logicalWidth
			|| candidate.logicalHeight != currentReceipt->logicalHeight
			|| candidate.pixelWidth != currentReceipt->pixelWidth
			|| candidate.pixelHeight != currentReceipt->pixelHeight
			|| candidate.contentScaleX != currentReceipt->contentScaleX
			|| candidate.contentScaleY != currentReceipt->contentScaleY
			|| candidate.visible != currentReceipt->visible ) {
		if ( !RALimp_NextPresentationGeneration( &nextGeneration )
				|| !RALimp_BuildPresentationReceipt( currentReceipt->ownerGeneration,
					nextGeneration, &candidate ) ) return qfalse;
	}
	s_ralPresentationReceipt = candidate;
	*outReceipt = candidate;
	return qtrue;
#else
	(void)context; (void)currentReceipt; (void)outReceipt;
	return qfalse;
#endif
}

qboolean RALimp_PresentationBorrow( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationSurfaceBorrow_t *outBorrow ) {
#if defined(__APPLE__)
	ralPresentationSurfaceBorrow_t borrow;
	void *layer;
	(void)context;
	if ( !outBorrow || !Ral_PresentationHostReceiptExact( currentReceipt,
			&s_ralPresentationReceipt ) || !s_ralMetalView ) return qfalse;
	layer = SDL_Metal_GetLayer( s_ralMetalView );
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
#else
	(void)context; (void)currentReceipt; (void)outBorrow;
	return qfalse;
#endif
}

qboolean RALimp_PresentationClose( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationHostCloseMode_t mode ) {
	(void)context;
	if ( !Ral_PresentationHostReceiptExact( currentReceipt,
			&s_ralPresentationReceipt )
			|| ( mode != RAL_PRESENTATION_HOST_KEEP_OWNER
				&& mode != RAL_PRESENTATION_HOST_DESTROY_OWNER ) ) return qfalse;
	if ( mode == RAL_PRESENTATION_HOST_KEEP_OWNER ) return qtrue;
	GLimp_Shutdown( qtrue );
	RALimp_ForgetPresentationReceipt();
#if defined(__APPLE__)
	memset( &s_ralPresentationConfig, 0, sizeof( s_ralPresentationConfig ) );
#endif
	return qtrue;
}


/*
================
GLW_HideFullscreenWindow
================
*/
void GLW_HideFullscreenWindow( void ) {
	if ( SDL_window && glw_state.isFullscreen ) {
		SDL_HideWindow( SDL_window );
	}
}


/*
===============
Sys_GetClipboardData
===============
*/
char *Sys_GetClipboardData( void )
{
#ifdef HEADLESS
	return NULL;
#else
	char *data = NULL;
	char *cliptext;

	if ( ( cliptext = SDL_GetClipboardText() ) != NULL ) {
		if ( cliptext[0] != '\0' ) {
			size_t bufsize = strlen( cliptext ) + 1;

			data = Z_Malloc( bufsize );
			Q_strncpyz( data, cliptext, bufsize );

			// find first listed char and set to '\0'
			strtok( data, "\n\r\b" );
		}
		SDL_free( cliptext );
	}
	return data;
#endif
}


/*
===============
Sys_FlashWindow

Briefly flashes the engine window in its system taskbar / dock so the
user notices that a match has started.  Implemented via SDL3's
SDL_FlashWindow on all platforms that support it — on platforms where
SDL has no flash support the call becomes a no-op.
===============
*/
void Sys_FlashWindow( void )
{
#ifndef HEADLESS
	if ( SDL_window != NULL ) {
		SDL_FlashWindow( SDL_window, SDL_FLASH_BRIEFLY );
	}
#endif
}


/*
===============
Sys_BeepAttention

Emits a single system-level attention beep.  Used by cl_matchAlerts
bit 4.  On SDL3 we do not have a portable "system beep", so we fall
back to writing the BEL character to stderr which is almost universally
routed to the current terminal's bell.  On Windows this is augmented
by MessageBeep via the win32 backend.
===============
*/
void Sys_BeepAttention( void )
{
#ifndef HEADLESS
	/* SDL3 has no direct system beep API; fall back to the BEL control
	   character which both terminals and modern desktop environments
	   interpret as an attention signal. */
	fputc( '\a', stderr );
	fflush( stderr );
#endif
}


/*
===============
Sys_SetClipboardData

Places the provided plain-text string onto the system clipboard.
Used by console mark-mode copy and the help/search tooling.
===============
*/
void Sys_SetClipboardData( const char *text )
{
#ifdef HEADLESS
	(void)text;
#else
	if ( text == NULL )
		return;
	/* SDL3: SDL_SetClipboardText returns bool (true on success) */
	SDL_SetClipboardText( text );
#endif
}


/*
===============
Sys_SetClipboardBitmap
===============
*/
void Sys_SetClipboardBitmap( const byte *bitmap, int length )
{
#ifdef _WIN32
	HGLOBAL hMem;
	byte *ptr;

	if ( !OpenClipboard( NULL ) )
		return;

	EmptyClipboard();
	hMem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, length );
	if ( hMem != NULL ) {
		ptr = ( byte* )GlobalLock( hMem );
		if ( ptr != NULL ) {
			memcpy( ptr, bitmap, length );
		}
		GlobalUnlock( hMem );
		SetClipboardData( CF_DIB, hMem );
	}
	CloseClipboard();
#else
	(void)bitmap;
	(void)length;
#endif
}


/*
===============
Sys_SetClipboardImagePNG

cross-platform image clipboard via SDL3.  We register
an image/png data callback with SDL_SetClipboardData; SDL serves the
bytes to the active windowing system (X11 SelectionRequest, Wayland
data-device offer, Win32 RegisterClipboardFormat etc.).  The callback
holds the PNG bytes until SDL invokes the cleanup callback (selection
loss or a subsequent SDL_SetClipboardData call).

The macOS Obj-C++ "Pasteboard" path is the architectural follow-up that
Eser owns separately — this implementation covers the Linux X11 default
build (and any platform whose SDL backend implements PNG selections).
===============
*/
#ifndef _WIN32
static byte *s_sdl_clipboard_png_data = NULL;
static size_t s_sdl_clipboard_png_len = 0;

static const void *Sys_ClipboardImagePNG_Callback( void *userdata, const char *mime_type, size_t *size )
{
	(void)userdata;
	if ( mime_type == NULL ) {
		return NULL;
	}
	if ( Q_stricmp( mime_type, "image/png" ) != 0 ) {
		return NULL;
	}
	if ( size ) {
		*size = s_sdl_clipboard_png_len;
	}
	return s_sdl_clipboard_png_data;
}

static void Sys_ClipboardImagePNG_Cleanup( void *userdata )
{
	(void)userdata;
	if ( s_sdl_clipboard_png_data ) {
		free( s_sdl_clipboard_png_data );
		s_sdl_clipboard_png_data = NULL;
	}
	s_sdl_clipboard_png_len = 0;
}
#endif

void Sys_SetClipboardImagePNG( const byte *png, int length )
{
#ifdef _WIN32
	// Windows clipboard image path is CF_DIB / Sys_SetClipboardBitmap.
	// PNG-on-Windows-clipboard is a separate follow-up if ever needed.
	(void)png;
	(void)length;
#elif defined(__APPLE__)
	// macOS Cocoa pasteboard is the architectural Obj-C++ follow-up —
	// R_ScreenShot_f already returns before reaching here on macOS.
	(void)png;
	(void)length;
#else
	const char *mime_types[1] = { "image/png" };

	if ( png == NULL || length <= 0 ) {
		return;
	}

	// Free any previous payload before replacing — SDL keeps the
	// userdata pointer alive across callback invocations, so we cannot
	// simply hand it a stack buffer.
	if ( s_sdl_clipboard_png_data ) {
		free( s_sdl_clipboard_png_data );
		s_sdl_clipboard_png_data = NULL;
	}
	s_sdl_clipboard_png_data = (byte *)malloc( (size_t)length );
	if ( s_sdl_clipboard_png_data == NULL ) {
		s_sdl_clipboard_png_len = 0;
		return;
	}
	memcpy( s_sdl_clipboard_png_data, png, (size_t)length );
	s_sdl_clipboard_png_len = (size_t)length;

	SDL_SetClipboardData( Sys_ClipboardImagePNG_Callback,
	                      Sys_ClipboardImagePNG_Cleanup,
	                      NULL,
	                      mime_types,
	                      1 );
#endif
}
