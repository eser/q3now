// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../code/qcommon/wired/core/vfs/fs_alias.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_true( int condition, const char *name ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", name );
		failures++;
	}
}

static bool target_exists( const char *qpath, void *ctx ) {
	const char *expected = (const char *)ctx;
	return strcmp( qpath, expected ) == 0;
}

static void test_flatten_and_resolve( void ) {
	wired_fs_alias_pair_t pairs[2] = {
		{ "sound/weapons/ric1.wav", "weapons/machinegun/sounds/ric1.opus" },
		{ "sound/weapons/machinegun/ric1.wav",
		  "weapons/machinegun/sounds/ric1.opus" }
	};
	wired_fs_alias_entry_t storage[4];
	wired_fs_alias_catalog_t catalog;
	char error[256];
	char resolved[WIRED_FS_ALIAS_PATH_MAX];

	expect_true( wired_fs_alias_build( pairs, 2, 7, target_exists,
		"weapons/machinegun/sounds/ric1.opus", storage, 4, &catalog,
		error, sizeof( error ) ), "valid catalog builds" );
	expect_true( catalog.generation == 7, "generation preserved" );
	expect_true( wired_fs_alias_resolve( &catalog, "SOUND\\WEAPONS\\RIC1.WAV",
		resolved, sizeof( resolved ) ), "separator and case normalize" );
	expect_true( strcmp( resolved, "weapons/machinegun/sounds/ric1.opus" ) == 0,
		"chain flattened to canonical target" );
	expect_true( !wired_fs_alias_resolve( &catalog, "sound/weapons/ric2.wav",
		resolved, sizeof( resolved ) ), "unmapped path preserved by caller" );
}

static void test_rejections( void ) {
	wired_fs_alias_entry_t storage[4];
	wired_fs_alias_catalog_t catalog;
	char error[256];
	wired_fs_alias_pair_t cycle[2] = {
		{ "a/sound.opus", "b/sound.opus" },
		{ "b/sound.opus", "a/sound.opus" }
	};
	wired_fs_alias_pair_t forbidden = {
		"scripts/legacy.lua", "vm/cgame.wasm"
	};
	wired_fs_alias_pair_t traversal = {
		"textures/../secret.png", "textures/safe.png"
	};
	wired_fs_alias_pair_t missing = {
		"textures/legacy.png", "textures/missing.png"
	};
	wired_fs_alias_pair_t absolute = {
		"/textures/legacy.png", "textures/safe.png"
	};
	wired_fs_alias_pair_t config = {
		"configs/legacy.cfg", "configs/current.cfg"
	};
	wired_fs_alias_pair_t duplicates[2] = {
		{ "textures/legacy.png", "textures/first.png" },
		{ "TEXTURES\\LEGACY.PNG", "textures/second.png" }
	};

	expect_true( !wired_fs_alias_build( cycle, 2, 1, NULL, NULL, storage, 4,
		&catalog, error, sizeof( error ) ), "cycle rejected" );
	expect_true( strstr( error, "cycle" ) != NULL, "cycle diagnostic" );
	expect_true( !wired_fs_alias_build( &forbidden, 1, 1, NULL, NULL, storage, 4,
		&catalog, error, sizeof( error ) ), "VM target rejected" );
	expect_true( !wired_fs_alias_build( &traversal, 1, 1, NULL, NULL, storage, 4,
		&catalog, error, sizeof( error ) ), "traversal rejected" );
	expect_true( !wired_fs_alias_build( &missing, 1, 1, target_exists,
		"textures/other.png", storage, 4, &catalog, error, sizeof( error ) ),
		"missing target rejected" );
	expect_true( !wired_fs_alias_build( &absolute, 1, 1, NULL, NULL, storage, 4,
		&catalog, error, sizeof( error ) ), "absolute path rejected" );
	expect_true( !wired_fs_alias_build( &config, 1, 1, NULL, NULL, storage, 4,
		&catalog, error, sizeof( error ) ), "config alias rejected" );
	expect_true( !wired_fs_alias_build( duplicates, 2, 1, NULL, NULL, storage, 4,
		&catalog, error, sizeof( error ) ), "normalized duplicate rejected" );
	expect_true( !wired_fs_alias_path_allowed( "fs-aliases.lua" ),
		"authored alias manifest cannot alias itself" );
}

static void test_exact_extensions( void ) {
	wired_fs_alias_pair_t pair = {
		"textures/panel.tga", "textures/panel.png"
	};
	wired_fs_alias_entry_t storage[1];
	wired_fs_alias_catalog_t catalog;
	char error[256];
	char resolved[WIRED_FS_ALIAS_PATH_MAX];

	expect_true( wired_fs_alias_build( &pair, 1, 3, NULL, NULL, storage, 1,
		&catalog, error, sizeof( error ) ), "extension-changing alias builds" );
	expect_true( wired_fs_alias_resolve( &catalog, "textures/panel.tga",
		resolved, sizeof( resolved ) ), "exact source extension resolves" );
	expect_true( strcmp( resolved, "textures/panel.png" ) == 0,
		"target extension preserved" );
	expect_true( !wired_fs_alias_resolve( &catalog, "textures/panel",
		resolved, sizeof( resolved ) ), "extensionless spelling does not match" );
}

int main( void ) {
	test_flatten_and_resolve();
	test_rejections();
	test_exact_extensions();
	if ( failures ) {
		fprintf( stderr, "%d file alias assertion(s) failed\n", failures );
		return 1;
	}
	puts( "VFS file alias contract: PASS" );
	return 0;
}
