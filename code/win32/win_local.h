// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// win_local.h: Win32 OS-tier header (main entry, console, shared helpers).
//
// The Win32 window/input/surface backend has been retired in favour of the
// SDL3 backend (code/sdl). What remains here is the OS-syscall tier shared by
// win_main.c, win_shared.c, win_syscon.c and win_console_attach.c: the Windows
// headers, the WinVars_t process globals, the console API, and the few window
// hooks that win_main.c's crash handler and frame loop still call (now provided
// by the SDL backend).

#include <windows.h>

#include <shlobj.h>

#undef open
#define open _open
#undef close
#define close _close
#undef write
#define write _write

#define T TEXT
#ifdef UNICODE
LPWSTR AtoW( const char *s );
const char *WtoA( const LPWSTR s );
#else
#define AtoW(S) (S)
#define WtoA(S) (S)
#endif

void	Sys_CreateConsole( const char *title, int xPos, int yPos, qboolean usePos );
void	Sys_DestroyConsole( void );

// window procedure / console pump
void HandleConsoleEvents( void );

// win_console_attach.c — reattach stdio to parent console (client only;
// dedicated is console subsystem and doesn't need this).
void Sys_AttachParentConsole( void );

void Conbuf_AppendText( const char *msg );
void Conbuf_BeginPrint( void );
void Conbuf_EndPrint( void );

typedef struct
{
	HINSTANCE		hInstance;
	HWND			hWnd;

	// Multi-monitor tracking
	RECT			conRect;
#ifndef HEADLESS
	RECT			winRect;
	qboolean		winRectValid;

	int				borderless;

	// when we get a windows message, we store the time off so keyboard processing
	// can know the exact time of an event. NANOSECONDS (sysEvent_t.evTime is ns;
	// Com_EventLoop divides by 1e6) — stamped from Sys_NanoTime(), so it must be
	// 64-bit (ns overflows 32-bit in ~4s).
	uint64_t		sysMsgTime;
#endif
} WinVars_t;

extern WinVars_t	g_wv;

// Provided by the SDL backend (code/sdl), called from win_main.c: the frame
// loop pumps input each frame, and the crash handler restores gamma / hides the
// fullscreen window on an unhandled exception.
void IN_Frame( void );
void GLW_HideFullscreenWindow( void );
void GLW_RestoreGamma( void );
