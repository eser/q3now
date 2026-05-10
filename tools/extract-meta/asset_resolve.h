/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// asset_resolve.h — given a needed_asset_t, decide whether it's
// available via the engine VFS and, if not, find a substitute.

#ifndef EXTRACT_META_ASSET_RESOLVE_H
#define EXTRACT_META_ASSET_RESOLVE_H

#include "q_shared.h"
#include "bsp_inventory.h"
#include "shader_index.h"

typedef struct {
	needed_asset_t  source;
	char            replacement[MAX_QPATH];   // empty if unresolved
	qboolean        resolved;
	const char     *resolution_method;        // static string literal
} resolution_t;

// Cheap availability check: returns qtrue if the asset can be served
// out of the current VFS. Used to skip resolution entirely for hits.
qboolean Asset_IsAvailable(const needed_asset_t *a,
                           const shader_index_t *idx);

// Try to find a substitute. Walks the strategy ladder:
//   A. Category swap (category_map.c)        → "swap_category"
//   B. First in target dir (best-effort)     → "first_in_dir"
//   C. Category default (category_map.c)     → "category_default"
//   D. Give up                               → "unresolved"
void Asset_Resolve(const needed_asset_t *missing,
                   const shader_index_t *idx,
                   resolution_t *out);

#endif /* EXTRACT_META_ASSET_RESOLVE_H */
