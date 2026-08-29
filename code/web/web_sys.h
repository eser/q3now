// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_WEB_SYS_H
#define WIRED_WEB_SYS_H

#ifdef __cplusplus
extern "C" {
#endif

void WiredWeb_InputKey( int key, int down );
void WiredWeb_InputChar( int codepoint );
void WiredWeb_InputMouse( float dx, float dy );
void WiredWeb_InputPointer( float x, float y, int down );
void WiredWeb_InputWheel( float deltaY );

#ifdef __cplusplus
}
#endif

#endif
