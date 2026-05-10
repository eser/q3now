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
#ifndef _WIN32
#  error You should not be including this file on this platform
#endif

#ifndef __GLW_WIN_H__
#define __GLW_WIN_H__

#include <windows.h>

typedef struct
{
	HDC     hDC;			// handle to device context
	HGLRC   hGLRC;			// handle to GL rendering context

	HINSTANCE   OpenGLLib;  // HINSTANCE for the OpenGL library
	HINSTANCE   VulkanLib;  // HINSTANCE for the Vulkan library

	qboolean	pixelFormatSet;

	int			desktopBitsPixel;
	int			desktopWidth;
	int			desktopHeight;
	int			desktopX;		// can be negative
	int			desktopY;		// can be negative

	RECT		workArea;

	HMONITOR	hMonitor;		// current monitor
	TCHAR		displayName[CCHDEVICENAME];
	qboolean	deviceSupportsGamma;
	qboolean	gammaSet;

	qboolean	cdsFullscreen;
	int			monitorCount;

	FILE		*log_fp;	// TODO: implement?

	glconfig_t	*config;	// feedback to renderer module

} glwstate_t;

extern glwstate_t glw_state;

#endif
