// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_CORE_VFS_FS_RESOURCE_IDENTITY_H
#define WIRED_CORE_VFS_FS_RESOURCE_IDENTITY_H

#include <stddef.h>
#include <stdint.h>

/* Stable identities of resolved resources (fsResolvedResource_t.sourceId).
 * Persistent caches (cooked materials) key on them, so an identity must change
 * exactly when the resource's bytes can have changed, and never because an
 * unrelated file did. All values are FNV-1a 64 over explicit little-endian
 * fields, the same on every platform. */

#define WIRED_FS_IDENTITY_SEED UINT64_C( 1469598103934665603 )

uint64_t wired_fs_identity_bytes( uint64_t hash, const void *bytes, size_t count );

/* Folds '\\' to '/' and ASCII upper case to lower case. */
uint64_t wired_fs_identity_path( uint64_t hash, const char *path );

/* An archive member, from its own content checksum as the archive stores it
 * (CRC-32 for pk3, CRC-32C for sw3z), its uncompressed size and its path. The
 * archive's other members do not take part: repacking an archive with a
 * changed unrelated file keeps every unchanged member's identity. */
uint64_t wired_fs_member_identity( uint8_t archiveKind, uint32_t contentCrc,
	uint64_t size, const char *path );

/* A loose file, from its size, its modification time (seconds since the epoch,
 * as the OS reports it) and its OS path. Editing the file changes the size or
 * the time, so a product cooked from the old bytes stops matching; touching it
 * without a change also does, which costs one recook and is never stale. The
 * leading domain tag keeps loose identities apart from archive members'. */
uint64_t wired_fs_loose_identity( uint64_t size, int64_t modifiedTime,
	const char *osPath );

#endif
