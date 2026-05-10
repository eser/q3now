/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// category_map.h — prefix-rewrite tables and category-default lookups
// used by asset_resolve.c when an asset path is missing from the VFS.
//
// Two independent tables:
//
//   prefix_rules[]      — "if path starts with FROM, try TO instead",
//                         scoped to a single asset_kind_t per rule.
//                         Walked in order; first match wins.
//
//   category_defaults[] — "fall back to this asset for the category".
//                         One entry per kind, NULL value means no
//                         default for that kind (asset stays unresolved).

#ifndef EXTRACT_META_CATEGORY_MAP_H
#define EXTRACT_META_CATEGORY_MAP_H

#include "q_shared.h"
#include "bsp_inventory.h"   /* needed_asset_t, asset_kind_t */

// Try the prefix-rewrite table for src->path. On match, write the
// rewritten path into out_path (NUL-terminated, ≤ out_size bytes) and
// return qtrue. On miss, leave out_path untouched and return qfalse.
qboolean CategoryMap_TryRewrite(const needed_asset_t *src,
                                char *out_path, size_t out_size);

// Look up the category default for `kind`. On hit, write the default
// path (without extension; resolver appends one) into out_path and
// return qtrue. On miss (no default for kind), return qfalse.
qboolean CategoryMap_TryDefault(asset_kind_t kind,
                                char *out_path, size_t out_size);

#endif /* EXTRACT_META_CATEGORY_MAP_H */
