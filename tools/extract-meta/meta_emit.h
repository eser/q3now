/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// meta_emit.h — populate a map_meta_t and write it to disk.

#ifndef EXTRACT_META_META_EMIT_H
#define EXTRACT_META_META_EMIT_H

#include "q_shared.h"
#include "map_inventory.h"
#include "asset_resolve.h"

qboolean MetaEmit_Write(const char *out_dir,
                        const char *mapname,
                        const map_inventory_t *inv,
                        const resolution_t *resolutions,
                        int resolution_count);

#endif /* EXTRACT_META_META_EMIT_H */
