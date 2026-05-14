// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#if !( defined __linux__ || defined __FreeBSD__ || defined __OpenBSD__ || defined __sun || defined __APPLE__ )
#error You should include this file only on Linux/FreeBSD/Solaris platforms
#endif

#ifndef __GLW_LINUX_H__
#define __GLW_LINUX_H__

#include <X11/Xlib.h>
#include <X11/Xfuncproto.h>

typedef struct sym_s
{
	void **symbol;
	const char *name;
} sym_t;

typedef struct
{
	void *OpenGLLib; // instance of OpenGL library
	void *VulkanLib; // instance of Vulkan library
	FILE *log_fp;

	int	monitorCount;

	qboolean gammaSet;

	qboolean cdsFullscreen;

	glconfig_t *config; // feedback to renderer module

	qboolean dga_ext;

	qboolean vidmode_ext;
	qboolean vidmode_active;
	qboolean vidmode_gamma;

	qboolean randr_ext;
	qboolean randr_active;
	qboolean randr_gamma;

	qboolean desktop_ok;
	int desktop_width;
	int desktop_height;
	int desktop_x;
	int desktop_y;

} glwstate_t;

extern glwstate_t glw_state;
extern Display *dpy;
extern Window win;
extern int scrnum;

qboolean BuildGammaRampTable( unsigned char *red, unsigned char *green, unsigned char *blue, int gammaRampSize, unsigned short table[3][4096] );

// DGA extension
qboolean DGA_Init( Display *_dpy );
void DGA_Mouse( qboolean enable );
void DGA_Done( void );

// VidMode extension
qboolean VidMode_Init( void );
void VidMode_Done( void );
qboolean VidMode_SetMode( int *width, int *height, int *rate );
void VidMode_RestoreMode( void );
void VidMode_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
void VidMode_RestoreGamma( void );

// XRandR extension
qboolean RandR_Init( int x, int y, int w, int h );
void RandR_Done( void );
void RandR_UpdateMonitor( int x, int y, int w, int h );
qboolean RandR_SetMode( int *width, int *height, int *rate );
void RandR_RestoreMode( void );
void RandR_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
void RandR_RestoreGamma( void );

#endif
