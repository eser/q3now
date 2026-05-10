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
 * name:		be_aas_entity.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_entity.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
//invalidates all entity infos
void AAS_InvalidateEntities(void);
//unlink not updated entities
void AAS_UnlinkInvalidEntities(void);
//resets the entity AAS and BSP links (sets areas and leaves pointers to NULL)
void AAS_ResetEntityLinks(void);
//updates an entity
int AAS_UpdateEntity(int ent, bot_entitystate_t *state);
//gives the entity data used for collision detection
#if 0
void AAS_EntityBSPData(int entnum, bsp_entdata_t *entdata);
#endif
#endif //AASINTERN
#if 0
//returns the size of the entity bounding box in mins and maxs
void AAS_EntitySize(int entnum, vec3_t mins, vec3_t maxs);
#endif
//returns the BSP model number of the entity
int AAS_EntityModelNum(int entnum);
//returns the origin of an entity with the given model number
int AAS_OriginOfMoverWithModelNum(int modelnum, vec3_t origin);
#if 0
//returns the best reachable area the entity is situated in
int AAS_BestReachableEntityArea(int entnum);
#endif
//returns the info of the given entity
void AAS_EntityInfo(int entnum, aas_entityinfo_t *info);
//returns the next entity
int AAS_NextEntity(int entnum);
#if 0
//returns the origin of the entity
void AAS_EntityOrigin(int entnum, vec3_t origin);
#endif
//returns the entity type
int AAS_EntityType(int entnum);
//returns the model index of the entity
int AAS_EntityModelindex(int entnum);
