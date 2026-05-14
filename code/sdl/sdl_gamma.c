// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <SDL3/SDL.h>
#include "../client/client.h"
#include "sdl_glw.h"

/*
================
GLimp_InitGamma

SDL3 removed gamma ramp APIs. Gamma adjustment is not supported.
Use in-game r_gamma cvar (shader-based) or monitor settings.
================
*/
void GLimp_InitGamma( glconfig_t *config )
{
	config->deviceSupportsGamma = qfalse;
}


/*
================
GLimp_SetGamma

No-op: SDL3 removed SDL_SetWindowGammaRamp() and SDL_SetWindowBrightness().
================
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	(void)red; (void)green; (void)blue;
}


/*
================
GLW_RestoreGamma
================
*/
void GLW_RestoreGamma( void )
{
}
