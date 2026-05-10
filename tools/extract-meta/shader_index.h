/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// shader_index.h — index of every shader visible to the engine VFS,
// keyed by name. Built once per `extract-meta` run by enumerating
// `scripts/*.shader` and parsing the loose Q3 shader grammar.
//
// Storage: a 256-bucket hash table over Tool_Arena. Bucket index is
// FNV-1a 32-bit of the lowercased shader name, masked to 8 bits.
// Collisions resolved by linear chain via the `next` pointer.
//
// Ownership: every allocation here lives in Tool_Arena. The arena is
// freed wholesale at tool exit (Tool_Shutdown). ShaderIndex_Free
// resets the in-arena state for re-use within a run; it does NOT
// release arena memory.

#ifndef EXTRACT_META_SHADER_INDEX_H
#define EXTRACT_META_SHADER_INDEX_H

#include "q_shared.h"

typedef struct shader_def_s {
	char                  name[MAX_QPATH];
	char                **stage_maps;       // arena-owned strings
	int                   stage_map_count;
	struct shader_def_s  *next;             // bucket chain
} shader_def_t;

typedef struct shader_index_s {
	shader_def_t *buckets[256];
	int           total_count;
} shader_index_t;

// Build the index by enumerating scripts/*.shader. Returns NULL on
// allocator failure (arena full); caller treats NULL as "no shaders
// available", which is conservative and lets the asset-resolve
// fallback chain still work.
shader_index_t *ShaderIndex_Build(void);

qboolean              ShaderIndex_Has (const shader_index_t *idx, const char *name);
const shader_def_t   *ShaderIndex_Find(const shader_index_t *idx, const char *name);

// No-op today (arena owns memory). Provided so future changes to the
// allocator don't force every caller site to be touched.
void ShaderIndex_Free(shader_index_t *idx);

#endif /* EXTRACT_META_SHADER_INDEX_H */
