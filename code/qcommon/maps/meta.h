// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// meta.h -- unified per-map metadata and asset-remap, replacing the legacy
// scripts/<map>.arena + scripts/arenas.txt machinery.
//
// A .meta file is a strict superset of .arena: every legacy .arena key
// remains parseable (and is normalized to its canonical name on read).
// The new format adds presentation metadata, balance hints, and a
// `remap { src dst ... }` block for asset substitutions.
//
// Resolution order at load time (per map):
//   1. maps/<mapname>.meta  -> source = META_SOURCE_META
//   2. scripts/<mapname>.arena  -> source = META_SOURCE_ARENA   (legacy fallback)
//   3. neither -> Meta_SetDefaults, source = META_SOURCE_NONE
//
// Storage lifetime: maps_list[] entries and their remap_table_t allocations
// live in Maps_Arena (created in Com_Init, reset on FS_Restart, destroyed in
// Com_Shutdown). They survive Hunk_ClearLevel, so the active map's remap
// table is valid for the whole match.

#ifndef __META_H
#define __META_H

#include "../q_shared.h"

#define META_MAX_GAMETYPES   8
#define META_MAX_REMAPS      256
#define META_MAX_MAPS        1024
#define META_REMAP_BUCKETS   64   // power-of-two; remap_table_s::buckets[] size
#define META_GAMETYPE_LEN    16
#define META_ARCHETYPE_LEN   16
#define META_WEAPON_LEN      8

typedef enum {
	META_SOURCE_NONE = 0,
	META_SOURCE_META,    // loaded from maps/<x>.meta
	META_SOURCE_ARENA,   // loaded from scripts/<x>.arena (legacy fallback)
	META_SOURCE_BROKEN   // .meta file existed but failed to parse; defaults populated
} meta_source_t;

typedef struct {
	char tokens[META_MAX_GAMETYPES][META_GAMETYPE_LEN];
	int  count;
} meta_gametypes_t;

typedef struct remap_entry_s {
	char                  src[MAX_QPATH];
	char                  dst[MAX_QPATH];
	struct remap_entry_s *next;
} remap_entry_t;

typedef struct remap_table_s {
	remap_entry_t *buckets[META_REMAP_BUCKETS];   // collision chain
	int            count;
} remap_table_t;

// Asset categories that the remap system distinguishes. Kept in sync
// with tools/extract-meta's ASSET_KIND_* enum (it reuses these values
// directly via static_assert / numeric equivalence).
typedef enum {
	REMAP_KIND_SHADER  = 0,
	REMAP_KIND_TEXTURE = 1,
	REMAP_KIND_SOUND   = 2,
	REMAP_KIND_MUSIC   = 3,
	REMAP_KIND_COUNT
} remap_kind_t;

// Typed remap container. Each kind has its own optional sub-table; an
// entry like `tables[REMAP_KIND_TEXTURE] == NULL` means "no texture
// substitutions in this map". An entirely empty remap_set_t (all four
// pointers NULL) is also valid and round-trips through the parser.
typedef struct {
	remap_table_t *tables[REMAP_KIND_COUNT];
} remap_set_t;

typedef struct map_meta_s {
	char              mapname[MAX_QPATH];

	// legacy .arena keys (canonical names)
	char              longname[64];
	int               scorelimit;            // was: fraglimit
	int               timelimit;
	char              bots[128];             // raw space-separated string
	meta_gametypes_t  type;

	// presentation
	char              series[32];
	char              archetype[META_ARCHETYPE_LEN];
	char              author[64];
	int               year;
	char              quote[96];

	// balance hints
	int               players_min;
	int               players_max;
	char              meta_weapon[META_WEAPON_LEN];
	int               item_nodes;

	// asset substitutions — typed sub-tables, all-NULL when no
	// `remap { ... }` block was parsed.
	remap_set_t       remap;

	meta_source_t     source;
} map_meta_t;

// ── core parsing ────────────────────────────────────────────────────────
qboolean    Meta_ParseFromBuffer (map_meta_t *out, const char *buf, const char *path_for_errors);
qboolean    Meta_ParseFromFile   (map_meta_t *out, const char *path);
void        Meta_SetDefaults     (map_meta_t *out, const char *mapname);

// ── queries ─────────────────────────────────────────────────────────────
qboolean    Meta_HasGametype     (const map_meta_t *m, const char *canonical_gt);

// ── alias normalization (used by parser, exposed for tests/migration) ──
const char *Meta_NormalizeKey      (const char *legacy_or_canonical);
const char *Meta_NormalizeGametype (const char *legacy_or_canonical);

// ── remap_table_t primitives (Maps_Arena lifetime) ─────────────────────
remap_table_t *Remap_Create  (void);
void           Remap_Add     (remap_table_t *t, const char *src, const char *dst);
const char    *Remap_Lookup  (const remap_table_t *t, const char *src); // NULL if no hit
void           Remap_Free    (remap_table_t *t);  // arena-backed, hint only

// ── remap_set_t typed container ────────────────────────────────────────
const char    *RemapSet_Lookup  (const remap_set_t *s, remap_kind_t kind, const char *src);
void           RemapSet_Add     (remap_set_t *s,       remap_kind_t kind, const char *src, const char *dst);
qboolean       RemapSet_IsEmpty (const remap_set_t *s);

// ── active remap (renderer hook source) ─────────────────────────────────
void           R_SetActiveRemapSet (const remap_set_t *s);  // NULL clears
const char    *R_TryRemapShader    (const char *name);
const char    *R_TryRemapTexture   (const char *name);
const char    *S_TryRemapSound     (const char *name);  // declared, not yet consumed
const char    *S_TryRemapMusic     (const char *name);  // declared, not yet consumed
void           R_PushNullRemap     (void);                // recursion-guard helper
void           R_PopRemap          (void);

// Engine-side cross-DLL adapter used by cl_main.c when filling
// rimp.MetaRemap_Lookup. Takes `int kind` (not `remap_kind_t`) so the
// signature matches the function pointer in tr_public.h verbatim and
// renderer code does not need to include meta.h to know the enum size.
// Internally casts to remap_kind_t.
const char    *MetaRemap_LookupAdapter (int kind, const char *name);

// ── arena lifecycle (called by Com_Init / Com_Shutdown / FS_Restart) ───
void           Maps_InitArena     (void);
void           Maps_ShutdownArena (void);
void           Maps_ResetArena    (void);

// ── map enumeration ─────────────────────────────────────────────────────
extern map_meta_t maps_list[META_MAX_MAPS];
extern int        maps_count;

void                Maps_ScanAll       (void);
qboolean            Maps_LoadMetaFor   (const char *mapname, map_meta_t *out);
const map_meta_t   *Maps_FindByName    (const char *mapname);   // NULL if not in scan
void                Maps_AddOrRefresh  (const char *mapname);   // late-arrival entry add

// ── serialization (for migration tooling, future round) ────────────────
qboolean    Meta_WriteToFile      (const map_meta_t *m, const char *path);

#endif // __META_H
