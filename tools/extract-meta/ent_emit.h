/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// ent_emit.h — write the BSP's entity lump to <out>/<map>.ent.

#ifndef EXTRACT_META_ENT_EMIT_H
#define EXTRACT_META_ENT_EMIT_H

#include "q_shared.h"
#include "bsp_inventory.h"

qboolean EntEmit_Write(const char *out_dir,
                       const char *mapname,
                       const bsp_inventory_t *inv);

#endif /* EXTRACT_META_ENT_EMIT_H */
