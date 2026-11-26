// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_FS_ALIAS_H
#define WIRED_FS_ALIAS_H

#include <stdbool.h>
#include <stddef.h>

#define WIRED_FS_ALIAS_MAX_ENTRIES 2048
#define WIRED_FS_ALIAS_PATH_MAX 128

typedef struct wired_fs_alias_pair_s {
	char source[WIRED_FS_ALIAS_PATH_MAX];
	char target[WIRED_FS_ALIAS_PATH_MAX];
} wired_fs_alias_pair_t;

typedef struct wired_fs_alias_entry_s {
	char source[WIRED_FS_ALIAS_PATH_MAX];
	char target[WIRED_FS_ALIAS_PATH_MAX];
} wired_fs_alias_entry_t;

/* A catalog's generation is its content identity, not a counter: equal catalogs (the same flattened, normalized source -> target
 * pairs, in any manifest order) have the same generation in every process and on every platform, and different catalogs differ.
 * It enters persistent keys (cooked material and texture bindings), which a per-process install counter made stale whenever an
 * extra install (fs_restart, a content scope change) shifted it. Never 0; the empty catalog has WIRED_FS_ALIAS_EMPTY_GENERATION. */
#define WIRED_FS_ALIAS_EMPTY_GENERATION 0x80ddc1f7u

typedef struct wired_fs_alias_catalog_s {
	unsigned generation;
	size_t count;
	size_t capacity;
	wired_fs_alias_entry_t *entries;
} wired_fs_alias_catalog_t;

typedef bool (*wired_fs_alias_target_exists_fn)( const char *qpath, void *ctx );

/* The content identity of built (normalized, flattened, sorted) entries: FNV-1a 64 over the entry count and each source and target
 * string up to its NUL followed by a 0 separator, folded to 32 bits; never 0. */
unsigned wired_fs_alias_digest( const wired_fs_alias_entry_t *entries, size_t count );

bool wired_fs_alias_build( const wired_fs_alias_pair_t *pairs, size_t pairCount,
	wired_fs_alias_target_exists_fn targetExists,
	void *targetExistsCtx, wired_fs_alias_entry_t *storage, size_t storageCapacity,
	wired_fs_alias_catalog_t *outCatalog, char *error, size_t errorSize );

bool wired_fs_alias_resolve( const wired_fs_alias_catalog_t *catalog,
	const char *qpath, char *out, size_t outSize );

bool wired_fs_alias_path_allowed( const char *qpath );

#endif
