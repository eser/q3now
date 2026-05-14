// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*****************************************************************************
 * name:		be_aas_file.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_file.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
//loads the AAS file with the given name
int AAS_LoadAASFile(char *filename);
//writes an AAS file with the given name
qboolean AAS_WriteAASFile(char *filename);
//dumps the loaded AAS data
void AAS_DumpAASData(void);
#endif //AASINTERN
