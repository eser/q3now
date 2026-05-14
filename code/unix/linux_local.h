// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
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
