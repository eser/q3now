// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// PNG encoder (writer). Pairs with tr_image_png.c (the decoder).
//
// Replaces the earlier "warn + fall back to TGA" path for
// `screenshot png`. Accepts a bottom-up RGB byte buffer (the convention
// produced by vk_read_pixels / RB_ReadPixels) and writes an 8-bit
// truecolour PNG using zlib STORED blocks (DEFLATE BTYPE=00 per RFC 1951
// §3.2.4, wrapped in zlib per RFC 1950). The output is not compressed
// — STORED is essentially "pass the bytes through with a 5-byte block
// header" — but the result is a valid PNG that any image viewer can
// open. Output size is ~raw+5N/65535 bytes; smaller than BMP for the
// same dimensions and well under "1.3x raw" worst case. If real
// compression is ever needed, that is a separate turn.

#include "../../qcommon/q_shared.h"
#include "../../qcommon/wired/wired_build_stamp.h"
#include "tr_public.h"
#include "r_log.h"

R_LOG_DECLARE_CHANNEL( rch_png_write, "renderer.assets" );

// `ri` is declared in tr_public.h (line 352).

// PNG CRC32 must use the standard IEEE 802.3 polynomial table.
// qcommon/q_shared.c ships a `crc32_buffer` but its lookup table has
// several corrupted entries (see q_shared.c around line 122: 0x7EB17CBE
// where the standard is 0x7EB17CBD, etc.) — that variant is used by
// engine-internal fingerprint checks like the mapel4b shader-replace
// gate in tr_map.c and must not be touched.  PNG requires the canonical
// table, so we carry our own copy below.

static uint32_t png_crc32_table[256];
static qboolean png_crc32_table_initted = qfalse;

static void png_crc32_table_init( void ) {
	uint32_t n, k, c;
	for ( n = 0; n < 256; n++ ) {
		c = n;
		for ( k = 0; k < 8; k++ ) {
			c = ( c & 1u ) ? ( 0xEDB88320u ^ ( c >> 1 ) ) : ( c >> 1 );
		}
		png_crc32_table[n] = c;
	}
	png_crc32_table_initted = qtrue;
}

static uint32_t png_crc32( const byte *buf, size_t len ) {
	uint32_t crc = 0xFFFFFFFFu;
	if ( !png_crc32_table_initted ) png_crc32_table_init();
	while ( len-- ) {
		crc = png_crc32_table[ ( crc ^ *buf++ ) & 0xFFu ] ^ ( crc >> 8 );
	}
	return crc ^ 0xFFFFFFFFu;
}

// ── byte-order helpers ─────────────────────────────────────────────────

static void png_write_u32_be( byte *p, uint32_t v ) {
	p[0] = (byte)( v >> 24 );
	p[1] = (byte)( v >> 16 );
	p[2] = (byte)( v >>  8 );
	p[3] = (byte)( v       );
}

static void png_write_u16_le( byte *p, uint16_t v ) {
	p[0] = (byte)( v       );
	p[1] = (byte)( v >>  8 );
}

// ── chunk writer ────────────────────────────────────────────────────────
//
// PNG chunk layout: [Length 4 BE] [Type 4] [Data N] [CRC32 4 BE]. CRC32
// is computed over [Type | Data]. We write Type/Data into the destination
// buffer first, then CRC the destination region directly — no temporary
// allocation.

static void png_write_chunk( byte **dst, const char type[4], const byte *data, uint32_t len ) {
	byte *p = *dst;
	byte *crc_start;
	uint32_t crc;

	png_write_u32_be( p, len );
	p += 4;
	crc_start = p;
	memcpy( p, type, 4 );
	p += 4;
	if ( len && data ) {
		memcpy( p, data, len );
		p += len;
	}
	crc = png_crc32( crc_start, 4 + len );
	png_write_u32_be( p, crc );
	p += 4;

	*dst = p;
}

typedef struct {
	const char *key;
	const char *value;
} pngTextEntry_t;

static size_t png_text_chunk_size( const pngTextEntry_t *entry ) {
	return 12u + strlen( entry->key ) + 1u + strlen( entry->value );
}

static void png_write_text_chunk( byte **dst, const pngTextEntry_t *entry ) {
	byte payload[256];
	const size_t keyLength = strlen( entry->key );
	const size_t valueLength = strlen( entry->value );
	const size_t payloadLength = keyLength + 1u + valueLength;
	if ( payloadLength > sizeof( payload ) ) return;
	memcpy( payload, entry->key, keyLength );
	payload[keyLength] = '\0';
	memcpy( payload + keyLength + 1u, entry->value, valueLength );
	png_write_chunk( dst, "tEXt", payload, (uint32_t)payloadLength );
}

// ── encoder ─────────────────────────────────────────────────────────────
//
// Allocates a single Hunk temp buffer sized for the worst-case output,
// writes the PNG in-place, computes Adler32 incrementally over the
// filtered scanline stream as we emit it, then copies the final region
// into a ri.Malloc buffer the caller owns (so the Hunk LIFO stack
// closes cleanly before we hand bytes back to the caller).
//
// PNG row order is top-to-bottom; the source buffer is bottom-up (PNG
// row Y comes from source row [height - 1 - Y]). One filter byte (0 =
// None) precedes each scanline.

static qboolean R_EncodePNG_bottomup_internal( const byte *rgb_bottomup, int width, int height,
		const char *rendererBackend, byte **outBytes, int *outLen )
{
	const byte png_sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	byte	ihdr[13];
	size_t	scanline_size;
	size_t	raw_size;
	size_t	num_blocks;
	size_t	zstream_size;
	size_t	total;
	byte	*hunk_buf;
	byte	*p;
	byte	*idat_len_pos;
	byte	*idat_type_pos;
	byte	*idat_data_start;
	byte	*block_header;
	size_t	bytes_in_block;
	uint32_t adler_a, adler_b;
	int		y, x;
	uint32_t idat_data_len;
	uint32_t idat_crc;
	pngTextEntry_t textEntries[10];
	size_t textEntryCount = 0u;
	size_t textChunksSize = 0u;
	char requestedRenderer[64], mapName[MAX_QPATH], resolution[32];
	char brightness[64], pbr[16], mode[16], fullscreen[16];

	*outBytes = NULL;
	*outLen = 0;

	if ( width <= 0 || height <= 0 ) {
		return qfalse;
	}
	// guard against 32-bit overflow on platforms where size_t is 32-bit
	if ( (size_t)width > 0x10000u || (size_t)height > 0x10000u ) {
		return qfalse;
	}

	requestedRenderer[0] = mapName[0] = brightness[0] = pbr[0] = '\0';
	mode[0] = fullscreen[0] = '\0';
	(void)snprintf( resolution, sizeof( resolution ), "%dx%d", width, height );
	if ( rendererBackend && rendererBackend[0] && ri.Cvar_VariableStringBuffer ) {
		ri.Cvar_VariableStringBuffer( "cl_renderer", requestedRenderer,
			sizeof( requestedRenderer ) );
		ri.Cvar_VariableStringBuffer( "mapname", mapName, sizeof( mapName ) );
		ri.Cvar_VariableStringBuffer( "r_brightness", brightness, sizeof( brightness ) );
		ri.Cvar_VariableStringBuffer( "r_pbr", pbr, sizeof( pbr ) );
		ri.Cvar_VariableStringBuffer( "r_mode", mode, sizeof( mode ) );
		ri.Cvar_VariableStringBuffer( "r_fullscreen", fullscreen, sizeof( fullscreen ) );
	}
#define ADD_TEXT( key_, value_ ) do { \
		if ( ( value_ ) && ( value_ )[0] ) { \
			textEntries[textEntryCount].key = ( key_ ); \
			textEntries[textEntryCount++].value = ( value_ ); \
		} \
	} while ( 0 )
	if ( rendererBackend && rendererBackend[0] ) {
		ADD_TEXT( "Software", WIRED_ENGINE_TITLE );
		ADD_TEXT( "Renderer", rendererBackend );
		ADD_TEXT( "RequestedRenderer", requestedRenderer );
		ADD_TEXT( "Map", mapName );
		ADD_TEXT( "Resolution", resolution );
		ADD_TEXT( "ColorSpace", "sRGB display-referred" );
		ADD_TEXT( "r_brightness", brightness );
		ADD_TEXT( "r_pbr", pbr );
		ADD_TEXT( "r_mode", mode );
		ADD_TEXT( "r_fullscreen", fullscreen );
	}
#undef ADD_TEXT
	for ( size_t entryIndex = 0u; entryIndex < textEntryCount; ++entryIndex )
		textChunksSize += png_text_chunk_size( &textEntries[entryIndex] );

	scanline_size = (size_t)1 + (size_t)3 * (size_t)width;
	raw_size      = scanline_size * (size_t)height;
	// Block budget must match the EMIT rollover exactly. EMIT closes the
	// current STORED block and opens a fresh one the instant bytes_in_block
	// hits 65535 — *including* on the very last input byte when raw_size is an
	// exact multiple of 65535. That leaves a trailing empty block which
	// CLOSE_BLOCK(1) then finalises, so the real count is raw_size/65535 + 1.
	// A plain ceil( raw_size / 65535 ) under-counts the exact-multiple case by
	// one and lets the extra 5-byte block header (and the 8-byte tail it
	// shifts) run past the Hunk allocation. raw_size >= 4 here (width,height>0),
	// so the +1 never over-allocates: for non-multiples it equals the ceil.
	num_blocks    = raw_size / 65535u + 1u;
	zstream_size  = 2u                   // zlib header
	              + num_blocks * 5u      // STORED block headers (BFINAL+BTYPE | LEN | NLEN)
	              + raw_size
	              + 4u;                  // Adler32 trailer

	total = 8u                            // PNG signature
	      + 4u + 4u + 13u + 4u            // IHDR chunk
	      + textChunksSize                 // diagnostic tEXt chunks
	      + 4u + 4u + zstream_size + 4u   // IDAT chunk
	      + 4u + 4u + 0u    + 4u;         // IEND chunk

	hunk_buf = (byte *)ri.Hunk_AllocateTempMemory( total );
	p = hunk_buf;

	// ── PNG signature ─────────────────────────────────────────────────
	memcpy( p, png_sig, 8 );
	p += 8;

	// ── IHDR ──────────────────────────────────────────────────────────
	png_write_u32_be( ihdr + 0, (uint32_t)width );
	png_write_u32_be( ihdr + 4, (uint32_t)height );
	ihdr[8]  = 8;   // bit depth: 8 bits per channel
	ihdr[9]  = 2;   // colour type: 2 = truecolour (RGB)
	ihdr[10] = 0;   // compression method: 0 = DEFLATE (only valid value)
	ihdr[11] = 0;   // filter method: 0 (only valid value; per-scanline filter byte)
	ihdr[12] = 0;   // interlace: 0 = none
	png_write_chunk( &p, "IHDR", ihdr, 13 );
	for ( size_t entryIndex = 0u; entryIndex < textEntryCount; ++entryIndex )
		png_write_text_chunk( &p, &textEntries[entryIndex] );

	// ── IDAT (zlib stream of STORED blocks) ───────────────────────────
	idat_len_pos = p;
	png_write_u32_be( p, 0 );  // placeholder, patched after we know the length
	p += 4;
	idat_type_pos = p;
	memcpy( p, "IDAT", 4 );
	p += 4;
	idat_data_start = p;

	// zlib header (RFC 1950 §2.2): CMF = 0x78 (deflate, 32K window),
	// FLG = 0x01 (FCHECK such that (CMF*256 + FLG) % 31 == 0, FLEVEL=0,
	// FDICT=0). 0x7801 / 31 = 991 exactly.
	*p++ = 0x78;
	*p++ = 0x01;

	// STORED blocks. RFC 1951 §3.2.4: each block is
	//   1 byte: BFINAL (bit 0) | BTYPE=00 (bits 1-2) | padding (bits 3-7)
	//   2 bytes LEN (little-endian)
	//   2 bytes NLEN = ~LEN (little-endian)
	//   LEN bytes literal data
	// LEN <= 65535. We open blocks on demand and close them when 65535
	// bytes have been emitted or when all input is consumed.
	adler_a = 1;
	adler_b = 0;
	block_header = NULL;
	bytes_in_block = 0;

	// Macros keep the inner loop tight without obscuring it. EMIT writes
	// one literal byte into the current STORED block, updates Adler32,
	// and rolls over to a new block when we hit the 65535-byte cap.
#define BEGIN_BLOCK() do { \
		block_header = p; \
		*p++ = 0x00;   /* BFINAL=0, BTYPE=00 — patch BFINAL when this is the final block */ \
		p += 4;        /* LEN/NLEN patched on close */ \
		bytes_in_block = 0; \
	} while (0)

#define CLOSE_BLOCK( is_final ) do { \
		if ( is_final ) *block_header = 0x01; \
		png_write_u16_le( block_header + 1, (uint16_t)bytes_in_block ); \
		png_write_u16_le( block_header + 3, (uint16_t)( ~(uint16_t)bytes_in_block ) ); \
	} while (0)

#define EMIT( b ) do { \
		byte _b = (byte)( b ); \
		*p++ = _b; \
		adler_a = ( adler_a + _b ) % 65521u; \
		adler_b = ( adler_b + adler_a ) % 65521u; \
		bytes_in_block++; \
		if ( bytes_in_block == 65535u ) { \
			CLOSE_BLOCK( 0 ); \
			BEGIN_BLOCK(); \
		} \
	} while (0)

	BEGIN_BLOCK();

	// PNG is top-down. Source is bottom-up: PNG row Y reads source row
	// (height - 1 - Y).
	for ( y = 0; y < height; y++ ) {
		const byte *src_row = rgb_bottomup + (size_t)( height - 1 - y ) * (size_t)width * 3u;
		EMIT( 0 );  // filter byte: 0 = None
		for ( x = 0; x < width * 3; x++ ) {
			EMIT( src_row[x] );
		}
	}

	CLOSE_BLOCK( 1 );

#undef BEGIN_BLOCK
#undef CLOSE_BLOCK
#undef EMIT

	// Adler32 trailer (big-endian)
	png_write_u32_be( p, ( adler_b << 16 ) | adler_a );
	p += 4;

	// Patch IDAT chunk: length + CRC32 of [type | data]
	idat_data_len = (uint32_t)( p - idat_data_start );
	idat_crc = png_crc32( idat_type_pos, 4u + idat_data_len );
	png_write_u32_be( idat_len_pos, idat_data_len );
	png_write_u32_be( p, idat_crc );
	p += 4;

	// ── IEND ──────────────────────────────────────────────────────────
	png_write_chunk( &p, "IEND", NULL, 0 );

	// Copy out to a caller-owned buffer so the Hunk LIFO stack can close
	// cleanly. Screenshots are infrequent — the extra memcpy is negligible.
	*outLen = (int)( p - hunk_buf );
	*outBytes = (byte *)ri.Malloc( *outLen );
	memcpy( *outBytes, hunk_buf, *outLen );
	ri.Hunk_FreeTempMemory( hunk_buf );

	return qtrue;
}

// ── public entry points ─────────────────────────────────────────────────

qboolean R_EncodePNG( const byte *rgb_bottomup, int width, int height,
                     byte **outBytes, int *outLen )
{
	return R_EncodePNG_bottomup_internal( rgb_bottomup, width, height,
		NULL, outBytes, outLen );
}

qboolean R_EncodeScreenshotPNG( const byte *rgb_bottomup, int width, int height,
		const char *rendererBackend, byte **outBytes, int *outLen ) {
	return R_EncodePNG_bottomup_internal( rgb_bottomup, width, height,
		rendererBackend, outBytes, outLen );
}

qboolean R_SavePNG( const char *fileName, const byte *rgb_bottomup, int width, int height )
{
	byte *bytes = NULL;
	int   len = 0;

	if ( !R_EncodePNG_bottomup_internal( rgb_bottomup, width, height,
			NULL, &bytes, &len ) ) {
		R_LOG( rch_png_write, SEV_WARN, "R_SavePNG: encode failed (%dx%d)\n", width, height );
		return qfalse;
	}
	ri.FS_WriteFile( fileName, bytes, len );
	ri.Free( bytes );
	return qtrue;
}

qboolean R_SaveScreenshotPNG( const char *fileName, const byte *rgb_bottomup,
		int width, int height, const char *rendererBackend ) {
	byte *bytes = NULL;
	int len = 0;
	if ( !R_EncodePNG_bottomup_internal( rgb_bottomup, width, height,
			rendererBackend, &bytes, &len ) ) {
		R_LOG( rch_png_write, SEV_WARN,
			"R_SaveScreenshotPNG: encode failed (%dx%d)\n", width, height );
		return qfalse;
	}
	ri.FS_WriteFile( fileName, bytes, len );
	ri.Free( bytes );
	return qtrue;
}
