// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_file.h — savegame file I/O primitive (Phase-3): version header, atomic-
// ish write, strict read-back, and an optional zero-run (RLE) codec.
//
// This is the WRITE PRIMITIVE ONLY. It moves a raw byte buffer to/from disk with
// a validated header; it does NOT know about gentity_t / the descriptor tables /
// the callback registry — that is the Phase-4 serializer, which will call these
// functions. Nothing calls them in Phase-3, so the game is byte-identical.
//
// Two layers, split by dependency:
//   - the CODEC (this header's pack/validate + RLE) is game-type-free — plain byte
//     buffers, fixed-width ints, <string.h>. It lives in g_save_codec.c and the
//     round-trip unit test compiles it directly.
//   - the FILE I/O (SG_WriteFile / SG_ReadFile) uses the game VM's trap_FS_*
//     syscalls and lives in g_save_file.c (game build only).
//
// Atomicity note (case-A, GAME_API_VERSION 9): SG_WriteFile is crash-transactional.
// It writes a sibling temp file, size-validates it, then renames temp -> final via
// the G_FS_RENAME VM trap (added in API v9; thin plumbing to the engine's existing
// homepath-scoped FS_Rename). Temp+final are same-dir, so the rename is a true
// atomic commit — a crash mid-write leaves the temp + the PRIOR final save intact,
// never a torn final. This is RealRTCW's temp -> validate -> rename pattern.

#ifndef G_SAVE_FILE_H
#define G_SAVE_FILE_H

#include <stddef.h>
#include <stdint.h>

// Fresh v1 — q3now's own format, NOT RealRTCW's version lineage. A read rejects
// any file whose magic or version does not match exactly (no migration: an
// unknown version is the corruption/version signal, same policy as the Phase-2
// resolver's unknown-name).
#define SG_SAVE_MAGIC    0x57534733u  // 'W''S''G''3' — Wired SaveGame v3-engine tag
#define SG_SAVE_VERSION  1u

// The fixed file header, written little-endian-agnostic only in that the fields
// are host-endian fixed-width ints (saves are host-endian by design; the sole
// endian-safe element is the RLE 1-byte run count). Payload follows immediately.
typedef struct {
	uint32_t magic;        // must equal SG_SAVE_MAGIC
	uint32_t version;      // must equal SG_SAVE_VERSION
	uint32_t payloadBytes; // byte count of the payload that follows the header
	uint32_t flags;        // bit0 = payload is RLE-encoded; other bits reserved 0
} sgFileHeader_t;

#define SG_FLAG_RLE  0x00000001u

// ── codec (game-type-free; g_save_codec.c) ───────────────────────────────────

// Fill a header for a payload of `payloadBytes`, with `flags`. Pure struct init.
void SG_HeaderInit( sgFileHeader_t *hdr, uint32_t payloadBytes, uint32_t flags );

// Validate a header read from disk: magic + version exact-match. Returns 1 if the
// header is a well-formed SG_SAVE_VERSION file, 0 otherwise (wrong magic/version).
int  SG_HeaderValidate( const sgFileHeader_t *hdr );

// Zero-run (RLE) codec — RealRTCW's sparse-struct optimisation. A run of 0x00
// bytes collapses to a 2-byte token { 0x00, count } where count is 1..255 (the
// count is the only endian-safe field, deliberately 1 byte). A literal non-zero
// byte is copied verbatim. Round-trips any byte buffer.
//
// SG_RLE_Encode: encode `srcLen` bytes of `src` into `dst` (capacity `dstCap`).
//   Returns the encoded length, or (size_t)-1 if it would exceed dstCap.
// SG_RLE_Decode: decode `srcLen` encoded bytes into `dst` (capacity `dstCap`).
//   Returns the decoded length, or (size_t)-1 on overflow / malformed input.
// SG_RLE_MaxEncodedSize: a safe dst capacity for encoding srcLen bytes (worst
//   case: an all-nonzero buffer stays 1:1, so srcLen is enough; but a lone zero
//   costs 2 bytes, so the true worst case is 2*srcLen — return that).
size_t SG_RLE_MaxEncodedSize( size_t srcLen );
size_t SG_RLE_Encode( const uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstCap );
size_t SG_RLE_Decode( const uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstCap );

// ── file I/O (game build only; g_save_file.c, uses trap_FS_*) ────────────────
// These are declared here but defined only where the game traps are available.
// The unit test compiles g_save_codec.c alone and does not reference them.

// SG_WriteFile: write `payload` (payloadBytes) to `qpath` under the save dir, with
// a header. If `useRLE`, the payload is zero-run encoded first. Returns 1 on
// success (header + payload written and the written size re-validated), 0 on any
// FS error or a size mismatch. Non-atomic in v1 (case (b) — no rename trap).
int SG_WriteFile( const char *qpath, const void *payload, size_t payloadBytes, int useRLE );

// SG_ReadFile: read `qpath`, validate the header (magic + version + size), and
// copy the (RLE-decoded, if flagged) payload into `outBuf` (capacity `outCap`).
// Returns the payload byte count on success, or (size_t)-1 on any error
// (missing file, bad magic/version, size mismatch, or outBuf too small).
size_t SG_ReadFile( const char *qpath, void *outBuf, size_t outCap );

#endif // G_SAVE_FILE_H
