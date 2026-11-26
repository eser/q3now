// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "fs_resource_identity.h"

uint64_t wired_fs_identity_bytes( uint64_t hash, const void *bytes, size_t count ) {
	const unsigned char *cursor = (const unsigned char *)bytes;
	size_t i;
	for ( i = 0; i < count; i++ ) {
		hash ^= cursor[i];
		hash *= UINT64_C( 1099511628211 );
	}
	return hash;
}

uint64_t wired_fs_identity_path( uint64_t hash, const char *path ) {
	while ( path && *path ) {
		unsigned char c = (unsigned char)*path++;
		if ( c == '\\' ) {
			c = '/';
		} else if ( c >= 'A' && c <= 'Z' ) {
			c = (unsigned char)( c - 'A' + 'a' );
		}
		hash = wired_fs_identity_bytes( hash, &c, 1 );
	}
	return hash;
}

static uint64_t wired_fs_identity_u32( uint64_t hash, uint32_t value ) {
	unsigned char bytes[4];
	size_t i;
	for ( i = 0; i < 4; i++ ) bytes[i] = (unsigned char)( value >> ( i * 8u ) );
	return wired_fs_identity_bytes( hash, bytes, sizeof( bytes ) );
}

static uint64_t wired_fs_identity_u64( uint64_t hash, uint64_t value ) {
	unsigned char bytes[8];
	size_t i;
	for ( i = 0; i < 8; i++ ) bytes[i] = (unsigned char)( value >> ( i * 8u ) );
	return wired_fs_identity_bytes( hash, bytes, sizeof( bytes ) );
}

uint64_t wired_fs_member_identity( uint8_t archiveKind, uint32_t contentCrc,
		uint64_t size, const char *path ) {
	uint64_t hash = wired_fs_identity_bytes( WIRED_FS_IDENTITY_SEED, &archiveKind, 1 );
	hash = wired_fs_identity_u32( hash, contentCrc );
	hash = wired_fs_identity_u64( hash, size );
	return wired_fs_identity_path( hash, path );
}

uint64_t wired_fs_loose_identity( uint64_t size, int64_t modifiedTime,
		const char *osPath ) {
	static const unsigned char looseTag = 0xFFu;   /* never an archive kind */
	uint64_t hash = wired_fs_identity_bytes( WIRED_FS_IDENTITY_SEED, &looseTag, 1 );
	hash = wired_fs_identity_u64( hash, size );
	hash = wired_fs_identity_u64( hash, (uint64_t)modifiedTime );
	return wired_fs_identity_path( hash, osPath );
}
