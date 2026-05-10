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
 * name:		be_aas_main.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_main.h $
 *
 *****************************************************************************/

#ifdef AASINTERN

extern aas_t aasworld;

//AAS error message
void QDECL AAS_Error(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
//setup AAS with the given number of entities and clients
int AAS_Setup(void);
//shutdown AAS
void AAS_Shutdown(void);
//start a new map
int AAS_LoadMap(const char *mapname);
//start a new time frame
int AAS_StartFrame(float time);
#endif //AASINTERN

//returns true if AAS is initialized
int AAS_Initialized(void);
//returns true if the AAS file is loaded
int AAS_Loaded(void);
//returns the current time
float AAS_Time(void);
//
void AAS_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj );
