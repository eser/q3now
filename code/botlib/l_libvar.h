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

/*****************************************************************************
 * name:		l_libvar.h
 *
 * desc:		botlib vars
 *
 * $Archive: /source/code/botlib/l_libvar.h $
 *
 *****************************************************************************/

//library variable
typedef struct libvar_s
{
	char		*name;
	char		*string;
	int		flags;
	qboolean	modified;	// set each time the cvar is changed
	float		value;
	struct	libvar_s *next;
} libvar_t;

//removes all library variables
void LibVarDeAllocAll(void);
//gets the library variable with the given name
libvar_t *LibVarGet( const char *var_name );
//gets the string of the library variable with the given name
const char *LibVarGetString( const char *var_name );
//gets the value of the library variable with the given name
float LibVarGetValue( const char *var_name );
//creates the library variable if not existing already and returns it
libvar_t *LibVar( const char *var_name, const char *value );
//creates the library variable if not existing already and returns the value
float LibVarValue( const char *var_name, const char *value );
//creates the library variable if not existing already and returns the integer value
int LibVarInteger( const char *var_name, const char *value, int min_v, int max_v );
//creates the library variable if not existing already and returns the value string
const char *LibVarString( const char *var_name, const char *value );
//sets the library variable
void LibVarSet( const char *var_name, const char *value );
#if 0
//returns true if the library variable has been modified
qboolean LibVarChanged( const char *var_name );
//sets the library variable to unmodified
void LibVarSetNotModified( const char *var_name );
#endif
