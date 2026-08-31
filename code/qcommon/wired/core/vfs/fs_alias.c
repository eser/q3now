// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "fs_alias.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wired_fs_alias_error( char *error, size_t errorSize,
	const char *format, const char *path ) {
	if ( !error || errorSize == 0 ) {
		return;
	}
	snprintf( error, errorSize, format, path ? path : "" );
	error[errorSize - 1] = '\0';
}

static bool wired_fs_alias_normalize( const char *input, char *out,
	size_t outSize ) {
	size_t i;
	size_t componentStart = 0;

	if ( !input || !input[0] || !out || outSize == 0 ) {
		return false;
	}
	if ( input[0] == '/' || input[0] == '\\' ) {
		return false;
	}

	for ( i = 0; input[i]; i++ ) {
		unsigned char c = (unsigned char)input[i];
		if ( i + 1 >= outSize || c < 32 || c == ':' ) {
			return false;
		}
		if ( c == '\\' ) {
			c = '/';
		}
		if ( c == '/' ) {
			size_t componentLength = i - componentStart;
			if ( componentLength == 0
				|| (componentLength == 1 && out[componentStart] == '.')
				|| (componentLength == 2 && out[componentStart] == '.'
					&& out[componentStart + 1] == '.') ) {
				return false;
			}
			componentStart = i + 1;
		}
		out[i] = (char)tolower( c );
	}
	if ( i == componentStart
		|| (i - componentStart == 1 && out[componentStart] == '.')
		|| (i - componentStart == 2 && out[componentStart] == '.'
			&& out[componentStart + 1] == '.') ) {
		return false;
	}
	out[i] = '\0';
	return true;
}

static bool wired_fs_alias_forbidden_extension( const char *path ) {
	static const char *const forbidden[] = {
		".cfg", ".qvm", ".wasm", ".dll", ".so", ".dylib",
		".exe", ".com", ".bat", ".cmd"
	};
	const char *extension = strrchr( path, '.' );
	size_t i;

	if ( !extension ) {
		return false;
	}
	for ( i = 0; i < sizeof( forbidden ) / sizeof( forbidden[0] ); i++ ) {
		if ( strcmp( extension, forbidden[i] ) == 0 ) {
			return true;
		}
	}
	return false;
}

bool wired_fs_alias_path_allowed( const char *qpath ) {
	char normalized[WIRED_FS_ALIAS_PATH_MAX];

	if ( !wired_fs_alias_normalize( qpath, normalized, sizeof( normalized ) ) ) {
		return false;
	}
	if ( strcmp( normalized, "fs-aliases.lua" ) == 0 ) {
		return false;
	}
	return !wired_fs_alias_forbidden_extension( normalized );
}

static int wired_fs_alias_compare( const void *a, const void *b ) {
	const wired_fs_alias_entry_t *lhs = (const wired_fs_alias_entry_t *)a;
	const wired_fs_alias_entry_t *rhs = (const wired_fs_alias_entry_t *)b;
	return strcmp( lhs->source, rhs->source );
}

static int wired_fs_alias_find_linear( const wired_fs_alias_entry_t *entries,
	size_t count, const char *source ) {
	size_t i;
	for ( i = 0; i < count; i++ ) {
		if ( strcmp( entries[i].source, source ) == 0 ) {
			return (int)i;
		}
	}
	return -1;
}

static bool wired_fs_alias_flatten( wired_fs_alias_entry_t *entries,
	size_t count, size_t index, unsigned char *state, char *error,
	size_t errorSize ) {
	int targetIndex;

	if ( state[index] == 2 ) {
		return true;
	}
	if ( state[index] == 1 ) {
		wired_fs_alias_error( error, errorSize,
			"file alias cycle includes '%s'", entries[index].source );
		return false;
	}

	state[index] = 1;
	targetIndex = wired_fs_alias_find_linear( entries, count,
		entries[index].target );
	if ( targetIndex >= 0 ) {
		if ( !wired_fs_alias_flatten( entries, count, (size_t)targetIndex,
			state, error, errorSize ) ) {
			return false;
		}
		memcpy( entries[index].target, entries[targetIndex].target,
			sizeof( entries[index].target ) );
	}
	state[index] = 2;
	return true;
}

bool wired_fs_alias_build( const wired_fs_alias_pair_t *pairs, size_t pairCount,
	unsigned generation, wired_fs_alias_target_exists_fn targetExists,
	void *targetExistsCtx, wired_fs_alias_entry_t *storage, size_t storageCapacity,
	wired_fs_alias_catalog_t *outCatalog, char *error, size_t errorSize ) {
	unsigned char state[WIRED_FS_ALIAS_MAX_ENTRIES];
	size_t i;

	if ( error && errorSize ) {
		error[0] = '\0';
	}
	if ( !storage || !outCatalog || pairCount > storageCapacity
		|| pairCount > WIRED_FS_ALIAS_MAX_ENTRIES || (pairCount && !pairs) ) {
		wired_fs_alias_error( error, errorSize,
			"file alias catalog exceeds capacity at '%s'", "" );
		return false;
	}

	memset( storage, 0, storageCapacity * sizeof( *storage ) );
	memset( state, 0, sizeof( state ) );

	for ( i = 0; i < pairCount; i++ ) {
		size_t j;
		if ( !wired_fs_alias_path_allowed( pairs[i].source )
			|| !wired_fs_alias_normalize( pairs[i].source, storage[i].source,
				sizeof( storage[i].source ) ) ) {
			wired_fs_alias_error( error, errorSize,
				"unsafe file alias source '%s'", pairs[i].source );
			return false;
		}
		if ( !wired_fs_alias_path_allowed( pairs[i].target )
			|| !wired_fs_alias_normalize( pairs[i].target, storage[i].target,
				sizeof( storage[i].target ) ) ) {
			wired_fs_alias_error( error, errorSize,
				"unsafe file alias target '%s'", pairs[i].target );
			return false;
		}
		if ( strcmp( storage[i].source, storage[i].target ) == 0 ) {
			wired_fs_alias_error( error, errorSize,
				"self-referential file alias '%s'", storage[i].source );
			return false;
		}
		for ( j = 0; j < i; j++ ) {
			if ( strcmp( storage[j].source, storage[i].source ) == 0 ) {
				wired_fs_alias_error( error, errorSize,
					"duplicate file alias source '%s'", storage[i].source );
				return false;
			}
		}
	}

	for ( i = 0; i < pairCount; i++ ) {
		if ( !wired_fs_alias_flatten( storage, pairCount, i, state,
			error, errorSize ) ) {
			return false;
		}
	}
	if ( targetExists ) {
		for ( i = 0; i < pairCount; i++ ) {
			if ( !targetExists( storage[i].target, targetExistsCtx ) ) {
				wired_fs_alias_error( error, errorSize,
					"file alias target is missing '%s'", storage[i].target );
				return false;
			}
		}
	}

	qsort( storage, pairCount, sizeof( *storage ), wired_fs_alias_compare );
	outCatalog->generation = generation ? generation : 1;
	outCatalog->count = pairCount;
	outCatalog->capacity = storageCapacity;
	outCatalog->entries = storage;
	return true;
}

bool wired_fs_alias_resolve( const wired_fs_alias_catalog_t *catalog,
	const char *qpath, char *out, size_t outSize ) {
	char normalized[WIRED_FS_ALIAS_PATH_MAX];
	size_t low;
	size_t high;

	if ( !catalog || !out || outSize == 0
		|| !wired_fs_alias_normalize( qpath, normalized, sizeof( normalized ) ) ) {
		return false;
	}
	low = 0;
	high = catalog->count;
	while ( low < high ) {
		size_t mid = low + (high - low) / 2;
		int comparison = strcmp( normalized, catalog->entries[mid].source );
		if ( comparison == 0 ) {
			size_t length = strlen( catalog->entries[mid].target );
			if ( length + 1 > outSize ) {
				return false;
			}
			memcpy( out, catalog->entries[mid].target, length + 1 );
			return true;
		}
		if ( comparison < 0 ) {
			high = mid;
		} else {
			low = mid + 1;
		}
	}
	return false;
}
