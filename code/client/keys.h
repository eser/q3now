// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
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
