// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*****************************************************************************
 * name:		be_aas_cluster.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_cluster.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
//initialize the AAS clustering
void AAS_InitClustering(void);
//
void AAS_SetViewPortalsAsClusterPortals(void);
#endif //AASINTERN
