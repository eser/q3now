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
 * name:		be_aas_reach.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_reach.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
//initialize calculating the reachabilities
void AAS_InitReachability(void);
//continue calculating the reachabilities
int AAS_ContinueInitReachability(float time);
#if 0
int AAS_BestReachableLinkArea(aas_link_t *areas);
#endif
#endif //AASINTERN

//returns true if the are has reachabilities to other areas
int AAS_AreaReachability(int areanum);
//returns the best reachable area and goal origin for a bounding box at the given origin
int AAS_BestReachableArea(vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t goalorigin);
//returns the best jumppad area from which the bbox at origin is reachable
int AAS_BestReachableFromJumpPadArea(vec3_t origin, vec3_t mins, vec3_t maxs);
//returns the next reachability using the given model
int AAS_NextModelReachability(int num, int modelnum);
//returns the total area of the ground faces of the given area
float AAS_AreaGroundFaceArea(int areanum);
//returns true if the area is crouch only
int AAS_AreaCrouch(int areanum);
//returns true if a player can swim in this area
int AAS_AreaSwim(int areanum);
//returns true if the area is filled with a liquid
int AAS_AreaLiquid(int areanum);
//returns true if the area contains lava
int AAS_AreaLava(int areanum);
//returns true if the area contains slime
int AAS_AreaSlime(int areanum);
//returns true if the area has one or more ground faces
int AAS_AreaGrounded(int areanum);
//returns true if the area is a jump pad
int AAS_AreaJumpPad(int areanum);
//returns true if the area is donotenter
int AAS_AreaDoNotEnter(int areanum);
