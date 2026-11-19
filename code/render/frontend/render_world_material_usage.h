// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RENDER_WORLD_MATERIAL_USAGE_H
#define WIRED_RENDER_WORLD_MATERIAL_USAGE_H

#include "maps/map_format_registry.h"
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>
#endif

/* Load/cook-time dependency selection. Compiler-only table rows are not
 * resources; actual surfaces and fog volumes are. Validate before publishing
 * the mask, without changing the BSP or allocating a second world owner. */
static inline qboolean RenderWorldMaterialUsage_Build( const mapFile_t *map,
        byte *used, uint32_t capacity ) {
    if ( !map || !used || map->numShaders < 0
            || (uint32_t)map->numShaders > capacity
            || (map->numShaders && !map->shaders)
            || map->numSurfaces < 0 || (map->numSurfaces && !map->surfaces)
            || map->numFogs < 0 || (map->numFogs && !map->fogs) ) return qfalse;
    for ( int i = 0; i < map->numSurfaces; ++i )
        if ( map->surfaces[i].shaderNum < 0
                || map->surfaces[i].shaderNum >= map->numShaders ) return qfalse;
    memset( used, 0, (size_t)map->numShaders );
    for ( int i = 0; i < map->numSurfaces; ++i )
        used[map->surfaces[i].shaderNum] = 1;
    for ( int i = 0; i < map->numFogs; ++i )
        for ( int j = 0; j < map->numShaders; ++j )
#if defined(_WIN32)
            if ( !_stricmp( map->fogs[i].shader, map->shaders[j].shader ) )
#else
            if ( !strcasecmp( map->fogs[i].shader, map->shaders[j].shader ) )
#endif
                used[j] = 1;
    return qtrue;
}
#endif
