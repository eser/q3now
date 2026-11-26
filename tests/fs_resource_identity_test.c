// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Contract of the resolved-resource identities (fs_resource_identity.c). Cooked
// materials key on them, so the format is pinned: changing it makes every
// cooked material stale and must be deliberate.

#include "../code/qcommon/wired/core/vfs/fs_resource_identity.h"

#include <stdio.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

int main( void ) {
	const char *path = "textures/liquids/pool3d_5.jpg";
	const uint64_t member = wired_fs_member_identity( 1u, 0xA0638E6Eu, 46091u, path );

	// 1. Known answers (an independent FNV-1a 64 over kind, LE crc32, LE size64, folded path).
	CHECK( member == UINT64_C( 9389672585318753497 ) );
	CHECK( wired_fs_identity_path( WIRED_FS_IDENTITY_SEED, path ) == UINT64_C( 7176174696705176958 ) );

	// 2. The member identity depends on the member alone: the same content, size and path in a
	//    repacked archive (whatever else changed in it) is the same identity.
	CHECK( wired_fs_member_identity( 1u, 0xA0638E6Eu, 46091u, path ) == member );

	// 3. Anything that describes the member's bytes or place changes it.
	CHECK( wired_fs_member_identity( 1u, 0xA0638E6Fu, 46091u, path ) != member );   // content
	CHECK( wired_fs_member_identity( 1u, 0xA0638E6Eu, 46092u, path ) != member );   // size
	CHECK( wired_fs_member_identity( 0u, 0xA0638E6Eu, 46091u, path ) != member );   // archive kind (CRC-32 vs CRC-32C)
	CHECK( wired_fs_member_identity( 1u, 0xA0638E6Eu, 46091u, "textures/liquids/pool3d_6.jpg" ) != member );

	// 4. Paths fold separators and ASCII case, as the VFS lookup does.
	CHECK( wired_fs_member_identity( 1u, 0xA0638E6Eu, 46091u, "Textures\\Liquids/Pool3d_5.JPG" ) == member );
	CHECK( wired_fs_identity_path( WIRED_FS_IDENTITY_SEED, "Textures\\Liquids/Pool3d_5.JPG" )
		== wired_fs_identity_path( WIRED_FS_IDENTITY_SEED, path ) );

	puts( "fs resource identity contract: PASS" );
	return 0;
}
