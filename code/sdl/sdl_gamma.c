/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/

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
