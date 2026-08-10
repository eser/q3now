// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_file.c — the trap-using half of the Phase-3 file primitive: SG_WriteFile
// / SG_ReadFile, built on the game VM's trap_FS_* syscalls. The version-header and
// RLE codec they call live in g_save_codec.c (game-type-free, unit-tested).
//
// Atomicity (case-A, GAME_API_VERSION 9): SG_WriteFile is crash-transactional. It
// writes a sibling temp file, size-validates it, then trap_FS_Rename()s temp ->
// final. The rename is the atomic commit (homepath-scoped, so temp+final are
// same-dir = a true atomic rename) — a crash mid-write leaves the temp file and
// the PRIOR final save intact, never a torn final. This is RealRTCW's temp ->
// validate -> rename pattern, enabled by the new G_FS_RENAME VM trap (the engine's
// FS_Rename already existed; the trap is thin plumbing to it).
//
// Called by nobody yet (the Phase-4 serializer is the first caller); the trap
// addition is the flagged non-byte-identical delta (an API 8->9 bump), not the
// I/O path (which no gameplay frame touches).

#include "g_local.h"
#include "g_save_file.h"

// A scratch buffer sized for the largest save payload we expect to move in one
// call. The serializer (Phase-4) streams the descriptor walk into a caller buffer
// and hands it here; this primitive only needs room for the RLE expansion of the
// header + payload during I/O. Kept file-static (no per-call allocation, no zone
// pressure). 4 MiB comfortably covers a full entity/client/level snapshot; a
// larger save is rejected rather than truncated.
#define SG_IO_SCRATCH_BYTES ( 4 * 1024 * 1024 )
static byte sg_ioScratch[SG_IO_SCRATCH_BYTES];

// Write header + payload to `qpath`, then re-open and confirm the on-disk size
// equals `expectedTotal`. Returns 1 if the write landed at the intended size, 0 on
// any FS error / short write. Shared by the atomic path (writes the temp file).
static int SG_WriteAndValidate( const char *qpath, const sgFileHeader_t *hdr,
	const uint8_t *writePayload, size_t writeBytes, int expectedTotal ) {
	fileHandle_t f;
	int          reopenLen;

	trap_FS_FOpenFile( qpath, &f, FS_WRITE );
	if ( !f ) {
		return 0;
	}
	trap_FS_Write( hdr, (int)sizeof( *hdr ), f );
	if ( writeBytes > 0 ) {
		trap_FS_Write( writePayload, (int)writeBytes, f );
	}
	trap_FS_FCloseFile( f );

	// Re-open and size-validate: the on-disk size must equal header + payload. A
	// short write (full disk, torn write) is caught here — the temp file is NOT
	// promoted, so the prior final save is never touched.
	reopenLen = trap_FS_FOpenFile( qpath, &f, FS_READ );
	if ( f ) {
		trap_FS_FCloseFile( f );
	}
	return ( reopenLen == expectedTotal );
}

// SG_WriteFile — header + (optionally RLE-encoded) payload to `qpath`, ATOMICALLY.
// Case-A (GAME_API_VERSION 9, trap_FS_Rename): write a sibling temp file, size-
// validate it, then rename temp -> qpath. The rename is the atomic commit — a
// crash mid-write leaves the temp file and the PRIOR `qpath` save intact (never a
// torn final save). Returns 1 on success, 0 on any FS error / size mismatch (in
// which case `qpath` is unchanged).
int SG_WriteFile( const char *qpath, const void *payload, size_t payloadBytes, int useRLE ) {
	sgFileHeader_t hdr;
	const uint8_t *writePayload = (const uint8_t *)payload;
	size_t         writeBytes   = payloadBytes;
	uint32_t       flags        = 0;
	int            expectedTotal;
	char           tempPath[MAX_QPATH];

	// Optional zero-run encode into the scratch buffer.
	if ( useRLE ) {
		size_t enc = SG_RLE_Encode( (const uint8_t *)payload, payloadBytes,
			sg_ioScratch, sizeof( sg_ioScratch ) );
		if ( enc == (size_t)-1 ) {
			return 0;  // encoded payload would overflow the scratch buffer
		}
		writePayload = sg_ioScratch;
		writeBytes   = enc;
		flags        = SG_FLAG_RLE;
	}

	// Header describes the ON-DISK payload (post-encode length).
	SG_HeaderInit( &hdr, (uint32_t)writeBytes, flags );
	expectedTotal = (int)sizeof( hdr ) + (int)writeBytes;

	// Write to a sibling temp file first (same home dir as the final, so the rename
	// below is a same-directory atomic rename, not a cross-device copy).
	Com_sprintf( tempPath, sizeof( tempPath ), "%s.tmp", qpath );

	if ( !SG_WriteAndValidate( tempPath, &hdr, writePayload, writeBytes, expectedTotal ) ) {
		return 0;  // temp write failed/short — final save untouched
	}

	// Atomic commit: promote temp -> final. FS_Rename is homepath-scoped, so both
	// paths are same-dir; the rename is atomic (with a copy+delete fallback in the
	// engine only if the OS rename fails — same-dir it should not).
	trap_FS_Rename( tempPath, qpath );

	// Confirm the final now exists at the intended size (the rename landed).
	{
		fileHandle_t f;
		int finalLen = trap_FS_FOpenFile( qpath, &f, FS_READ );
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		if ( finalLen != expectedTotal ) {
			return 0;  // rename did not land (should not happen same-dir)
		}
	}
	return 1;
}

// SG_ReadFile — validate header, read + (RLE-decode) payload into outBuf.
// Returns the payload byte count, or (size_t)-1 on any error.
size_t SG_ReadFile( const char *qpath, void *outBuf, size_t outCap ) {
	fileHandle_t   f;
	sgFileHeader_t hdr;
	int            len;
	size_t         diskPayload;

	len = trap_FS_FOpenFile( qpath, &f, FS_READ );
	if ( !f ) {
		return (size_t)-1;  // missing / unreadable
	}

	// Must be at least a header.
	if ( len < (int)sizeof( hdr ) ) {
		trap_FS_FCloseFile( f );
		return (size_t)-1;
	}

	trap_FS_Read( &hdr, (int)sizeof( hdr ), f );
	if ( !SG_HeaderValidate( &hdr ) ) {
		trap_FS_FCloseFile( f );
		return (size_t)-1;  // wrong magic / version
	}

	// The header's payload count must match the actual on-disk remainder — a
	// mismatch means truncation or a torn write.
	diskPayload = (size_t)( len - (int)sizeof( hdr ) );
	if ( (size_t)hdr.payloadBytes != diskPayload ) {
		trap_FS_FCloseFile( f );
		return (size_t)-1;
	}
	if ( diskPayload > sizeof( sg_ioScratch ) ) {
		trap_FS_FCloseFile( f );
		return (size_t)-1;  // payload larger than the I/O scratch
	}

	if ( diskPayload > 0 ) {
		trap_FS_Read( sg_ioScratch, (int)diskPayload, f );
	}
	trap_FS_FCloseFile( f );

	// Decode (or copy) into the caller's buffer.
	if ( hdr.flags & SG_FLAG_RLE ) {
		size_t dec = SG_RLE_Decode( sg_ioScratch, diskPayload,
			(uint8_t *)outBuf, outCap );
		if ( dec == (size_t)-1 ) {
			return (size_t)-1;  // malformed RLE or outBuf too small
		}
		return dec;
	}

	if ( diskPayload > outCap ) {
		return (size_t)-1;  // caller buffer too small
	}
	if ( diskPayload > 0 ) {
		memcpy( outBuf, sg_ioScratch, diskPayload );
	}
	return diskPayload;
}
