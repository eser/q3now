// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_codec.c — the game-type-free half of the Phase-3 file primitive: the
// version-header pack/validate and the zero-run (RLE) codec. Depends only on
// <string.h>/<stdint.h>/g_save_file.h — NO game types, NO traps — so the
// round-trip unit test compiles it directly. The trap-using file I/O
// (SG_WriteFile / SG_ReadFile) lives in g_save_file.c.
//
// Phase-3 byte-identical: nothing calls these until the Phase-4 serializer.

#include <string.h>
#include <stdint.h>

#include "g_save_file.h"

// ── version header ───────────────────────────────────────────────────────────

void SG_HeaderInit( sgFileHeader_t *hdr, uint32_t payloadBytes, uint32_t flags ) {
	hdr->magic        = SG_SAVE_MAGIC;
	hdr->version      = SG_SAVE_VERSION;
	hdr->payloadBytes = payloadBytes;
	hdr->flags        = flags;
}

int SG_HeaderValidate( const sgFileHeader_t *hdr ) {
	if ( hdr == NULL ) {
		return 0;
	}
	// Exact-match: a wrong magic is not-a-save; a wrong version is a save this
	// build cannot read. Neither is migrated — both are rejected (the load-path
	// version/corruption signal, mirroring the Phase-2 resolver's unknown-name).
	if ( hdr->magic != SG_SAVE_MAGIC || hdr->version != SG_SAVE_VERSION ) {
		return 0;
	}
	return 1;
}

// ── zero-run (RLE) codec ─────────────────────────────────────────────────────
// Encoding: a run of N (1..255) consecutive 0x00 bytes -> the 2-byte token
// { 0x00, N }. A non-zero byte B -> the single literal byte B. A zero run longer
// than 255 is split into successive max-255 tokens. This exploits the sparse-
// zeroed savegame structs (large zero regions collapse ~128:1) while any non-zero
// data is at worst 1:1. Only the run count is endian-sensitive and it is one byte,
// so the stream is endian-portable even though the payload it wraps is host-endian.

size_t SG_RLE_MaxEncodedSize( size_t srcLen ) {
	// Worst case is an alternating buffer where every zero is isolated: each lone
	// 0x00 becomes a 2-byte token, so the encoded stream can be up to 2*srcLen.
	return srcLen * 2;
}

size_t SG_RLE_Encode( const uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstCap ) {
	size_t si = 0, di = 0;

	while ( si < srcLen ) {
		if ( src[si] == 0x00 ) {
			// Count the zero run, capped at 255 per token.
			size_t run = 0;
			while ( si < srcLen && src[si] == 0x00 && run < 255 ) {
				si++;
				run++;
			}
			if ( di + 2 > dstCap ) {
				return (size_t)-1;
			}
			dst[di++] = 0x00;
			dst[di++] = (uint8_t)run;
		} else {
			if ( di + 1 > dstCap ) {
				return (size_t)-1;
			}
			dst[di++] = src[si++];
		}
	}
	return di;
}

size_t SG_RLE_Decode( const uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstCap ) {
	size_t si = 0, di = 0;

	while ( si < srcLen ) {
		if ( src[si] == 0x00 ) {
			// A zero token is 2 bytes: { 0x00, count }. A trailing lone 0x00 with
			// no count byte is malformed input.
			uint8_t run;
			if ( si + 1 >= srcLen ) {
				return (size_t)-1;
			}
			run = src[si + 1];
			// A count of 0 is malformed (the encoder never emits an empty run).
			if ( run == 0 ) {
				return (size_t)-1;
			}
			if ( di + run > dstCap ) {
				return (size_t)-1;
			}
			memset( dst + di, 0x00, run );
			di += run;
			si += 2;
		} else {
			if ( di + 1 > dstCap ) {
				return (size_t)-1;
			}
			dst[di++] = src[si++];
		}
	}
	return di;
}
