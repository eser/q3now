// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "fs_qpath.h"

static bool wired_fs_qpath_separator( char c ) {
	return c == '/' || c == '\\';
}

bool wired_fs_qpath_is_safe( const char *qpath ) {
	const char *component;
	const char *cursor;

	if ( !qpath ) {
		return false;
	}

	component = qpath;
	for ( cursor = qpath; ; cursor++ ) {
		if ( cursor[0] == ':' && cursor[1] == ':' ) {
			return false;
		}

		if ( cursor[0] == '\0' || wired_fs_qpath_separator( cursor[0] ) ) {
			if ( cursor - component == 2 && component[0] == '.' && component[1] == '.' ) {
				return false;
			}
			if ( cursor[0] == '\0' ) {
				return true;
			}
			component = cursor + 1;
		}
	}
}
