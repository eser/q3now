// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef __GLW_LINUX_H__
#define __GLW_LINUX_H__

#include <SDL3/SDL.h>
#include "../client/cl_display_catalog.h"

#define USE_JOYSTICK

typedef struct
{
	FILE *log_fp;

	qboolean isFullscreen;

	glconfig_t *config; // feedback to renderer module

	int desktop_width;
	int desktop_height;

	int window_width;
	int window_height;

	int monitorCount;

} glwstate_t;

extern SDL_Window *SDL_window;
extern glwstate_t glw_state;

#define GLIMP_DISPLAY_DIRTY_TOPOLOGY WIRED_DISPLAY_CHANGE_TOPOLOGY
#define GLIMP_DISPLAY_DIRTY_ACTIVE_OUTPUT WIRED_DISPLAY_CHANGE_ACTIVE_OUTPUT
#define GLIMP_DISPLAY_DIRTY_SCALE WIRED_DISPLAY_CHANGE_SCALE
#define GLIMP_DISPLAY_DIRTY_COLOR WIRED_DISPLAY_CHANGE_COLOR
#define GLIMP_DISPLAY_DIRTY_FULLSCREEN WIRED_DISPLAY_CHANGE_FULLSCREEN
#define GLIMP_DISPLAY_DIRTY_EXTENT WIRED_DISPLAY_CHANGE_EXTENT

const wiredDisplayCatalog_t *GLimp_GetDisplayCatalog( void );
SDL_DisplayID GLimp_ResolveConfiguredDisplay( void );
void GLimp_DisplayCatalogMarkDirty( uint32_t flags );
void GLimp_DisplayCatalogReconcile( void );

void IN_Init( void );
void IN_Shutdown( void );

#endif
