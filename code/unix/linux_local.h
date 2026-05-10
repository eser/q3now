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
#ifndef __LINUX_LOCAL_H__
#define __LINUX_LOCAL_H__

// Input subsystem

void IN_Init (void);
void IN_Frame (void);
void IN_Shutdown (void);


void IN_JoyMove( void );
void IN_StartupJoystick( void );

// OpenGL subsystem
qboolean QGL_Init( const char *dllname );
void QGL_Shutdown( qboolean unloadDLL );

// Vulkan subsystem
qboolean QVK_Init( void );
void QVK_Shutdown( qboolean unloadDLL );


// bk001130 - win32
// void IN_JoystickCommands (void);

char *strlwr (char *s);

// signals.c
void InitSig(void);

#endif // __LINUX_LOCAL_H__
