// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../code/qcommon/wired/core/vfs/fs_qpath.h"

#include <stdio.h>

static int failures;

static void expect_true( int condition, const char *name ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", name );
		failures++;
	}
}

static void test_rejected_qpaths( void ) {
	static const char *const rejected[] = {
		"..",
		"../secret",
		"..\\secret",
		"base/..",
		"base\\..",
		"base/../secret",
		"base\\..\\secret",
		"base//..//secret",
		"base\\\\..\\\\secret",
		"base/..\\secret",
		"base\\../secret",
		"/../secret",
		"\\..\\secret",
		"base/child/..",
		"::",
		"base::secret",
		"base/child::leaf"
	};
	size_t i;

	expect_true( !wired_fs_qpath_is_safe( NULL ), "null qpath rejected" );
	for ( i = 0; i < sizeof( rejected ) / sizeof( rejected[0] ); i++ ) {
		expect_true( !wired_fs_qpath_is_safe( rejected[i] ), "unsafe qpath rejected" );
	}
}

static void test_preserved_qpaths( void ) {
	static const char *const accepted[] = {
		"",
		".",
		"./",
		"base",
		"base/file.cfg",
		"base\\file.cfg",
		"base//child///file",
		"base\\\\child/file",
		"foo..bar",
		"...",
		"..hidden",
		"hidden..",
		"base/.../file",
		"base/./file",
		"base/.hidden/file",
		"base:leaf"
	};
	size_t i;

	for ( i = 0; i < sizeof( accepted ) / sizeof( accepted[0] ); i++ ) {
		expect_true( wired_fs_qpath_is_safe( accepted[i] ), "non-parent qpath preserved" );
	}
}

int main( void ) {
	test_rejected_qpaths();
	test_preserved_qpaths();
	if ( failures ) {
		fprintf( stderr, "%d VFS qpath assertion(s) failed\n", failures );
		return 1;
	}
	puts( "VFS qpath contract: PASS" );
	return 0;
}
