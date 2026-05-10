/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// bsp_inventory.h — collected list of assets a single BSP needs.
//
// Built once per `extract-meta <map>` run. Driven by:
//
//   1. The shader table embedded in the BSP (dshader_t array) — every
//      reference there expands to the shader entry itself plus, where
//      we have a shader_index_t, every stage map the shader names.
//   2. The entity lump, scanned for `noise*`, `music`, and (on
//      worldspawn) `message` keys.
//
// Inventory ownership: the entity-string copy and the dynamic
// entries[] array are tool-side malloc/realloc, freed by
// BspInventory_Free. shader_index_t pointers (stage_maps) are NOT
// referenced from inventory_t — they're owned by the index, freed
// by ShaderIndex_Free at tool teardown.

#ifndef EXTRACT_META_BSP_INVENTORY_H
#define EXTRACT_META_BSP_INVENTORY_H

#include "q_shared.h"

typedef enum {
	ASSET_KIND_SHADER  = 0,
	ASSET_KIND_TEXTURE = 1,
	ASSET_KIND_SOUND   = 2,
	ASSET_KIND_MUSIC   = 3
} asset_kind_t;

typedef struct {
	asset_kind_t kind;
	char         path[MAX_QPATH];
} needed_asset_t;

typedef struct {
	needed_asset_t *entries;
	int             count;
	int             capacity;

	// Raw entity lump, copied out of the BSP before BSP_Free. ent_emit
	// writes this verbatim. Length is the BSP-reported length (may
	// include a trailing NUL, which ent_emit strips).
	char           *entity_string;
	int             entity_string_length;

	// worldspawn `message` value, if present. Empty if not.
	char            worldspawn_message[64];
} bsp_inventory_t;

// Forward decl — full type lives in shader_index.h. Avoids
// transitive header bloat in callers that only need the inventory.
struct shader_index_s;

qboolean BspInventory_Build(const char *mapname,
                            const struct shader_index_s *idx,
                            bsp_inventory_t *out);
void     BspInventory_Free(bsp_inventory_t *inv);

#endif /* EXTRACT_META_BSP_INVENTORY_H */
