// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
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
