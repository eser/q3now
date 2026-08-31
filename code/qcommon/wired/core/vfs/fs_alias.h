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

typedef struct wired_fs_alias_catalog_s {
	unsigned generation;
	size_t count;
	size_t capacity;
	wired_fs_alias_entry_t *entries;
} wired_fs_alias_catalog_t;

typedef bool (*wired_fs_alias_target_exists_fn)( const char *qpath, void *ctx );

bool wired_fs_alias_build( const wired_fs_alias_pair_t *pairs, size_t pairCount,
	unsigned generation, wired_fs_alias_target_exists_fn targetExists,
	void *targetExistsCtx, wired_fs_alias_entry_t *storage, size_t storageCapacity,
	wired_fs_alias_catalog_t *outCatalog, char *error, size_t errorSize );

bool wired_fs_alias_resolve( const wired_fs_alias_catalog_t *catalog,
	const char *qpath, char *out, size_t outSize );

bool wired_fs_alias_path_allowed( const char *qpath );

#endif
