// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_ENTITY_METADATA_H
#define G_ENTITY_METADATA_H

#include "../qcommon/wired/entity/metadata.h"

const wiredMetadataRegistry_t *G_EntityMetadataRegistry( void );
qboolean G_EntityMetadataSaveAdapterValid( void );
qboolean G_EntityMetadataInspect( int entityNum, const char *fieldName );

#endif
