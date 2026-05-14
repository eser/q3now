// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <SDL3/SDL.h>
#ifdef USE_VULKAN_API
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
#include "../renderercommon/tr_public.h"
#include "sdl_glw.h"
#include "sdl_icon.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_client, "client" );

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_FATAL_ERROR,
	RSERR_UNKNOWN
} rserr_t;

glwstate_t glw_state;

SDL_Window *SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;
#ifdef USE_VULKAN_API
static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
#endif

cvar_t *r_stereoEnabled;
cvar_t *in_nograb;

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			// NOLINTNEXTLINE(bugprone-integer-division) — pixel-aligned screen-center coordinates; integer math intentional
			SDL_WarpMouseGlobal( (float)(glw_state.desktop_width / 2), (float)(glw_state.desktop_height / 2) );
		} else {
			SDL_ShowCursor();
		}
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
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
static int GLW_SetMode( int mode, const char *modeFS, qboolean fullscreen, qboolean vulkan )
{
	glconfig_t *config = glw_state.config;
	int perChannelColorBits;
	int colorBits, depthBits, stencilBits;
	int i;
	SDL_DisplayID displayID;
	int x;
	int y;
	SDL_WindowFlags flags = 0; // SDL3: windows are shown by default; SDL_WINDOW_SHOWN removed

#ifdef USE_VULKAN_API
	if ( vulkan ) {
		flags |= SDL_WINDOW_VULKAN;
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Initializing Vulkan display\n");
	} else
#endif
	{
		flags |= SDL_WINDOW_OPENGL;
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Initializing OpenGL display\n");
	}

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

		// find out to which display our window belongs to
		// according to previously stored \vid_xpos and \vid_ypos coordinates
		displayID = FindNearestDisplay( &x, &y, 640, 480 );

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
			glw_state.desktop_width = 640;
			glw_state.desktop_height = 480;
		}
	}
	else
	{
		glw_state.desktop_width = 640;
		glw_state.desktop_height = 480;
	}

	// For windowed mode, clamp to usable display area so the window
	// does not extend behind the menu bar, taskbar, or dock
	if ( !fullscreen && displayID != 0 )
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

	if ( !CL_GetModeInfo( &config->vidWidth, &config->vidHeight, &config->windowAspect, mode, modeFS, glw_state.desktop_width, glw_state.desktop_height, fullscreen ) )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), " invalid mode\n" );
		return RSERR_INVALID_MODE;
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
	else if ( r_noborder->integer )
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

#ifdef USE_VULKAN_API
		if ( !vulkan )
#endif
		{

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

		// SDL3: SDL_CreateWindow no longer takes x, y params.
		// Create window first, then set position.
		if ( ( SDL_window = SDL_CreateWindow( cl_title, config->vidWidth, config->vidHeight, flags ) ) == NULL )
		{
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_CreateWindow failed: %s\n", SDL_GetError() );
			continue;
		}

		if ( !fullscreen )
		{
			SDL_SetWindowPosition( SDL_window, x, y );

			// Resize if the window extends beyond the display
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
						config->vidHeight = contentH - overflow;
						SDL_SetWindowSize( SDL_window, config->vidWidth, config->vidHeight );
					}
				}
			}
		}

		if ( fullscreen )
		{
#ifdef USE_VULKAN_API
			if ( vulkan )
			{
				// Vulkan: desktop fullscreen (SDL_WINDOW_FULLSCREEN default) is sufficient —
				// the swapchain handles resolution and format independently.
				// Exclusive mode with SDL_SetWindowFullscreenMode fails on macOS/MoltenVK
				// because SDL_PIXELFORMAT_RGB24 is not a valid display mode format.
			}
			else
#endif
			{
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
				// SDL3: refresh_rate is float
				fsMode.refresh_rate = (float)Cvar_VariableIntegerValue( "r_displayRefresh" );

				if ( !SDL_SetWindowFullscreenMode( SDL_window, &fsMode ) )
				{
					Com_Log( SEV_DEBUG, LOG_CH(ch_client), "SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError() );
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
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

#ifdef USE_VULKAN_API
		if ( vulkan )
		{
			config->colorBits = testColorBits;
			config->depthBits = testDepthBits;
			config->stencilBits = testStencilBits;
		}
		else
#endif
		{
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
		} // if ( !vulkan )


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
	}
	else
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	if ( !fullscreen && r_noborder->integer )
		SDL_SetWindowHitTest( SDL_window, SDL_HitTestFunc, NULL );

	// SDL3: SDL_GetWindowSizeInPixels replaces both SDL_GL_GetDrawableSize and SDL_Vulkan_GetDrawableSize
	SDL_GetWindowSizeInPixels( SDL_window, &config->vidWidth, &config->vidHeight );

	// save render dimensions as renderer may change it in advance
	glw_state.window_width = config->vidWidth;
	glw_state.window_height = config->vidHeight;

	// SDL3: SDL_WarpMouseInWindow takes float coords
	// NOLINTNEXTLINE(bugprone-integer-division) — pixel-aligned window-center coordinates; integer math intentional
	SDL_WarpMouseInWindow( SDL_window, (float)(glw_state.window_width / 2), (float)(glw_state.window_height / 2) );

	return RSERR_OK;
}


/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static rserr_t GLimp_StartDriverAndSetMode( int mode, const char *modeFS, qboolean fullscreen, qboolean vulkan )
{
	rserr_t err;

	if ( fullscreen && in_nograb->integer )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Fullscreen not allowed with \\in_nograb 1\n");
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
		if ( vulkan )
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
		if ( !SDL_Init( SDL_INIT_VIDEO ) )
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

	err = GLW_SetMode( mode, modeFS, fullscreen, vulkan );

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
void GLimp_Init( glconfig_t *config )
{
	rserr_t err;

#ifndef _WIN32
	InitSig();
#endif

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "GLimp_Init()\n" );

	glw_state.config = config; // feedback renderer configuration

	{
		static const cvarDesc_t d = CVAR_BOOL( "in_nograb", "0", 0,
			"Do not capture mouse in game, may be useful during online streaming." );
		in_nograb = Cvar_Register( &d );
	}

	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	{
		static const cvarDesc_t d = CVAR_BOOL( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH,
			"Enable stereo rendering for techniques like shutter glasses." );
		r_stereoEnabled = Cvar_Register( &d );
	}

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer, qfalse );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Terminate( TERM_UNRECOVERABLE, "GLimp_Init() - could not load OpenGL subsystem" );
			return;
		}

		if ( r_mode->integer != 3 || ( r_fullscreen->integer && atoi( r_modeFullscreen->string ) != 3 ) )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Setting \\r_mode %d failed, falling back on \\r_mode %d\n", r_mode->integer, 3 );
			if ( GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer, qfalse ) != RSERR_OK )
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

#ifndef _WIN32
	InitSig();
#endif

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "VKimp_Init()\n" );

	{
		static const cvarDesc_t d = CVAR_BOOL( "in_nograb", "0", CVAR_ARCHIVE,
			"Do not capture mouse in game, may be useful during online streaming." );
		in_nograb = Cvar_Register( &d );
	}

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	{
		static const cvarDesc_t d = CVAR_BOOL( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH,
			"Enable stereo rendering for techniques like shutter glasses." );
		r_stereoEnabled = Cvar_Register( &d );
	}

	// feedback to renderer configuration
	glw_state.config = config;

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer, qtrue /* Vulkan */ );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Terminate( TERM_UNRECOVERABLE, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}

		Com_Log( SEV_INFO, LOG_CH(ch_client), "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, 3 );

		err = GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer, qtrue /* Vulkan */ );
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
void *VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	return qvkGetInstanceProcAddr( instance, name );
}


/*
===============
VK_CreateSurface
===============
*/
qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
	// SDL3: SDL_Vulkan_CreateSurface adds VkAllocationCallbacks* parameter (pass NULL)
	if ( SDL_Vulkan_CreateSurface( SDL_window, instance, NULL, surface ) )
		return qtrue;
	else
		return qfalse;
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

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			// NOLINTNEXTLINE(bugprone-integer-division) — pixel-aligned screen-center coordinates; integer math intentional
			SDL_WarpMouseGlobal( (float)(glw_state.desktop_width / 2), (float)(glw_state.desktop_height / 2) );
		} else {
			SDL_ShowCursor();
		}
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
}
#endif // USE_VULKAN_API


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
#ifdef DEDICATED
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
#ifndef DEDICATED
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
#ifndef DEDICATED
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
#ifdef DEDICATED
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
#endif
}
