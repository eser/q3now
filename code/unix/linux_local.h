// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef __LINUX_LOCAL_H__
#define __LINUX_LOCAL_H__

// Unix OS-tier header. The X11 window/input/surface backend has been retired in
// favour of the SDL3 backend (code/sdl); what remains here is what the OS-tier
// files (unix_main.c, unix_shared.c) share.

// pumped each frame by unix_main.c's game loop; provided by the SDL backend.
void IN_Frame (void);

char *strlwr (char *s);

#endif // __LINUX_LOCAL_H__
