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
#include "keycodes.h"

typedef struct {
	qboolean	down;
	qboolean	bound;
	int			repeats;		// if > 1, it is autorepeating
	char		*binding;
} qkey_t;

extern	qboolean	key_overstrikeMode;
extern	qkey_t		keys[MAX_KEYS];

extern  int         anykeydown;

// NOTE TTimo the declaration of field_t and Field_Clear is now in qcommon/qcommon.h

void Key_WriteBindings( fileHandle_t f );
void Key_SetBinding( int keynum, const char *binding );
const char *Key_GetBinding( int keynum );
void Key_ParseBinding( int key, qboolean down, unsigned time );

int Key_GetKey( const char *binding );
const char *Key_KeynumToString( int keynum );
int Key_StringToKeynum( const char *str );

qboolean Key_IsDown( int keynum );
void Key_ClearStates( void );

qboolean Key_GetOverstrikeMode( void );
void Key_SetOverstrikeMode( qboolean state );

void Com_InitKeyCommands( void );
