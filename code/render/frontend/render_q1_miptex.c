// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_q1_miptex.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Canonical Quake palette - 768 bytes verbatim from id1/pak0.pak gfx/palette.lmp. Indices 224-255 are the fullbright range (lava and
 * fire glow); index 255 is also the transparent pixel of `{` cut-out textures. */
const byte render_q1_palette[768] = {
    /* 0-15 */
      0,  0,  0,   15, 15, 15,   31, 31, 31,   47, 47, 47,
     63, 63, 63,   75, 75, 75,   91, 91, 91,  107,107,107,
    123,123,123,  139,139,139,  155,155,155,  171,171,171,
    187,187,187,  203,203,203,  219,219,219,  235,235,235,
    /* 16-31 */
     15, 11,  7,   23, 15, 11,   31, 23, 11,   39, 27, 15,
     47, 35, 19,   55, 43, 23,   63, 47, 23,   75, 55, 27,
     83, 59, 27,   91, 67, 31,   99, 75, 31,  107, 83, 31,
    115, 87, 31,  123, 95, 35,  131,103, 35,  143,111, 35,
    /* 32-47 */
     11, 11, 15,   19, 19, 27,   27, 27, 39,   39, 39, 51,
     47, 47, 63,   55, 55, 75,   63, 63, 87,   71, 71,103,
     79, 79,115,   91, 91,127,   99, 99,139,  107,107,151,
    115,115,163,  123,123,175,  131,131,187,  139,139,203,
    /* 48-63 */
      0,  0,  0,    7,  7,  0,   11, 11,  0,   19, 19,  0,
     27, 27,  0,   35, 35,  0,   43, 43,  7,   47, 47,  7,
     55, 55,  7,   63, 63,  7,   71, 71,  7,   75, 75, 11,
     83, 83, 11,   91, 91, 11,   99, 99, 11,  107,107, 15,
    /* 64-79 */
      7,  0,  0,   15,  0,  0,   23,  0,  0,   31,  0,  0,
     39,  0,  0,   47,  0,  0,   55,  0,  0,   63,  0,  0,
     71,  0,  0,   79,  0,  0,   87,  0,  0,   95,  0,  0,
    103,  0,  0,  111,  0,  0,  119,  0,  0,  127,  0,  0,
    /* 80-95 */
     19, 19,  0,   27, 27,  0,   35, 35,  0,   47, 43,  0,
     55, 47,  0,   67, 55,  0,   75, 59,  7,   87, 67,  7,
     95, 71,  7,  107, 75, 11,  119, 83, 15,  131, 87, 19,
    139, 91, 19,  151, 95, 27,  163, 99, 31,  175,103, 35,
    /* 96-111 */
     35, 19,  7,   47, 23, 11,   59, 31, 15,   75, 35, 19,
     87, 43, 23,   99, 47, 31,  115, 55, 35,  127, 59, 43,
    143, 67, 51,  159, 79, 51,  175, 99, 47,  191,119, 47,
    207,143, 43,  223,171, 39,  239,203, 31,  255,243, 27,
    /* 112-127 */
     11,  7,  0,   27, 19,  0,   43, 35, 15,   55, 43, 19,
     71, 51, 27,   83, 55, 35,   99, 63, 43,  111, 71, 51,
    127, 83, 63,  139, 95, 71,  155,107, 83,  167,123, 95,
    183,135,107,  195,147,123,  211,163,139,  227,179,151,
    /* 128-143 */
    171,139,163,  159,127,151,  147,115,135,  139,103,123,
    127, 91,111,  119, 83, 99,  107, 75, 87,   95, 63, 75,
     87, 55, 67,   75, 47, 55,   67, 39, 47,   55, 31, 35,
     43, 23, 27,   35, 19, 19,   23, 11, 11,   15,  7,  7,
    /* 144-159 */
    187,115,159,  175,107,143,  163, 95,131,  151, 87,119,
    139, 79,107,  127, 75, 95,  115, 67, 83,  107, 59, 75,
     95, 51, 63,   83, 43, 55,   71, 35, 43,   59, 31, 35,
     47, 23, 27,   35, 19, 19,   23, 11, 11,   15,  7,  7,
    /* 160-175 */
    219,195,187,  203,179,167,  191,163,155,  175,151,139,
    163,135,123,  151,123,111,  135,111, 95,  123, 99, 83,
    107, 87, 71,   95, 75, 59,   83, 63, 51,   67, 51, 39,
     55, 43, 31,   39, 31, 23,   27, 19, 15,   15, 11,  7,
    /* 176-191 */
    111,131,123,  103,123,111,   95,115,103,   87,107, 95,
     79, 99, 87,   71, 91, 79,   63, 83, 71,   55, 75, 63,
     47, 67, 55,   43, 59, 47,   35, 51, 39,   31, 43, 31,
     23, 35, 23,   15, 27, 19,   11, 19, 11,    7, 11,  7,
    /* 192-207 */
    255,243, 27,  239,223, 23,  219,203, 19,  203,183, 15,
    187,167, 15,  171,151, 11,  155,131,  7,  139,115,  7,
    123, 99,  7,  107, 83,  0,   91, 71,  0,   75, 55,  0,
     59, 43,  0,   43, 31,  0,   27, 15,  0,   11,  7,  0,
    /* 208-223 */
      0,  0,255,   11, 11,239,   19, 19,223,   27, 27,207,
     35, 35,191,   43, 43,175,   47, 47,159,   47, 47,143,
     47, 47,127,   47, 47,111,   47, 47, 95,   43, 43, 79,
     35, 35, 63,   27, 27, 47,   19, 19, 31,   11, 11, 15,
    /* 224-239: fullbright — lava/fire glow */
     43,  0,  0,   59,  0,  0,   75,  7,  0,   95,  7,  0,
    111, 15,  0,  127, 23,  7,  147, 31,  7,  163, 39, 11,
    183, 51, 15,  195, 75, 27,  207, 99, 43,  219,127, 59,
    227,151, 79,  231,171, 95,  239,191,119,  247,211,139,
    /* 240-255: fullbright — bright fire / screens */
    167,123, 59,  183,155, 55,  199,195, 55,  231,227, 87,
    127,191,255,  171,231,255,  215,255,255,  103,  0,  0,
    139,  0,  0,  179,  0,  0,  215,  0,  0,  255,  0,  0,
    255,243,147,  255,247,199,  255,255,255,  159, 91, 83,
};

#define Q1_LUMP_COUNT 15u
#define Q1_LUMP_TEXTURES 2u
#define Q1_MIPTEX_NAME 16u

static uint32_t ReadU32( const byte *p ) {
	return (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 );
}

qboolean RenderQ1Miptex_IsReference( const char *name ) {
	return name && strchr( name, RENDER_Q1_MIPTEX_REFERENCE_SEPARATOR ) ? qtrue : qfalse;
}

/* BSP29 ("version 29") and BSP2 ("BSP2" / "2PSB") share the 15-lump directory; the texture lump is lump 2 in both. */
static qboolean TextureLump( const byte *bsp, size_t length, const byte **outLump, uint32_t *outLength ) {
	uint32_t offset, size;
	if ( !bsp || length < 4u + Q1_LUMP_COUNT * 8u ) return qfalse;
	if ( ReadU32( bsp ) != 29u && memcmp( bsp, "BSP2", 4 ) && memcmp( bsp, "2PSB", 4 ) ) return qfalse;
	offset = ReadU32( bsp + 4u + Q1_LUMP_TEXTURES * 8u );
	size = ReadU32( bsp + 4u + Q1_LUMP_TEXTURES * 8u + 4u );
	if ( offset > length || size > length - offset || size < 4u ) return qfalse;
	*outLump = bsp + offset; *outLength = size;
	return qtrue;
}

/* Quake 1 miptex extents are multiples of 16 in practice, not powers of two (e1m5's carch03 is 256x192). */
static qboolean ValidExtent( uint32_t v ) {
	return v >= 1u && v <= 4096u;
}

qboolean RenderQ1Miptex_Find( const byte *bsp, size_t bspLength, const char *member,
		const byte **outPixels, uint32_t *outWidth, uint32_t *outHeight ) {
	const byte *lump;
	uint32_t lumpLength, count;
	if ( !member || !member[0] || !outPixels || !outWidth || !outHeight
			|| !TextureLump( bsp, bspLength, &lump, &lumpLength ) ) return qfalse;
	count = ReadU32( lump );
	if ( count > 4096u || 4u + (uint64_t)count * 4u > lumpLength ) return qfalse;
	for ( uint32_t i = 0u; i < count; ++i ) {
		const uint32_t offset = ReadU32( lump + 4u + i * 4u );
		char name[Q1_MIPTEX_NAME + 1u];
		uint32_t width, height, pixels;
		if ( offset == UINT32_MAX || (uint64_t)offset + Q1_MIPTEX_NAME + 24u > lumpLength ) continue;   /* -1: missing texture */
		memcpy( name, lump + offset, Q1_MIPTEX_NAME ); name[Q1_MIPTEX_NAME] = '\0';
		if ( strcasecmp( name, member ) ) continue;
		width = ReadU32( lump + offset + Q1_MIPTEX_NAME );
		height = ReadU32( lump + offset + Q1_MIPTEX_NAME + 4u );
		pixels = ReadU32( lump + offset + Q1_MIPTEX_NAME + 8u );
		if ( !ValidExtent( width ) || !ValidExtent( height ) || !pixels
				|| (uint64_t)offset + pixels + (uint64_t)width * height > lumpLength ) return qfalse;
		*outPixels = lump + offset + pixels; *outWidth = width; *outHeight = height;
		return qtrue;
	}
	return qfalse;
}

void RenderQ1Miptex_ExpandRgba8( const char *member, const byte *indexed, uint32_t pixelCount, byte *outRgba ) {
	const qboolean cutout = member && member[0] == '{' ? qtrue : qfalse;
	for ( uint32_t i = 0u; i < pixelCount; ++i ) {
		const byte index = indexed[i];
		byte *out = outRgba + i * 4u;
		if ( cutout && index == 255u ) { out[0] = out[1] = out[2] = out[3] = 0u; continue; }
		out[0] = render_q1_palette[index * 3u]; out[1] = render_q1_palette[index * 3u + 1u];
		out[2] = render_q1_palette[index * 3u + 2u]; out[3] = 255u;
	}
}

qboolean RenderQ1Miptex_DecodeImage( const refimport_t *imports, const char *name, byte **outPixels,
		uint32_t *outWidth, uint32_t *outHeight, char outResolved[MAX_QPATH] ) {
	char container[MAX_QPATH];
	const char *member;
	void *file = NULL;
	const byte *indexed;
	uint32_t width, height;
	int length;
	byte *rgba;
	if ( !imports || !imports->FS_ReadFile || !imports->FS_FreeFile || !imports->Malloc || !name
			|| !outPixels || !outWidth || !outHeight || strlen( name ) >= MAX_QPATH ) return qfalse;
	member = strchr( name, RENDER_Q1_MIPTEX_REFERENCE_SEPARATOR );
	if ( !member || member == name || !member[1] || (size_t)( member - name ) >= sizeof( container ) ) return qfalse;
	memcpy( container, name, (size_t)( member - name ) ); container[member - name] = '\0';
	member++;
	length = imports->FS_ReadFile( container, &file );
	if ( length <= 0 || !file ) return qfalse;
	if ( !RenderQ1Miptex_Find( (const byte *)file, (size_t)length, member, &indexed, &width, &height ) ) {
		imports->FS_FreeFile( file ); return qfalse;
	}
	rgba = (byte *)imports->Malloc( (size_t)width * height * 4u );
	if ( !rgba ) { imports->FS_FreeFile( file ); return qfalse; }
	RenderQ1Miptex_ExpandRgba8( member, indexed, width * height, rgba );
	imports->FS_FreeFile( file );
	*outPixels = rgba; *outWidth = width; *outHeight = height;
	if ( outResolved ) (void)snprintf( outResolved, MAX_QPATH, "%s", name );
	return qtrue;
}

qboolean RenderQ1Miptex_MaterialImage( const mapFile_t *map, const char *materialName, char outImage[MAX_QPATH] ) {
	const byte *lump;
	uint32_t count;
	if ( !map || !map->embeddedTextures || map->embeddedTexturesLength < 4 || !materialName || !materialName[0]
			|| !outImage || strlen( materialName ) > Q1_MIPTEX_NAME
			|| strlen( map->name ) + 1u + strlen( materialName ) >= MAX_QPATH ) return qfalse;
	lump = map->embeddedTextures;
	count = ReadU32( lump );
	if ( count > 4096u || 4u + (uint64_t)count * 4u > (uint64_t)map->embeddedTexturesLength ) return qfalse;
	for ( uint32_t i = 0u; i < count; ++i ) {
		const uint32_t offset = ReadU32( lump + 4u + i * 4u );
		char name[Q1_MIPTEX_NAME + 1u];
		if ( offset == UINT32_MAX || (uint64_t)offset + Q1_MIPTEX_NAME > (uint64_t)map->embeddedTexturesLength ) continue;
		memcpy( name, lump + offset, Q1_MIPTEX_NAME ); name[Q1_MIPTEX_NAME] = '\0';
		if ( strcasecmp( name, materialName ) ) continue;
		(void)snprintf( outImage, MAX_QPATH, "%s%c%s", map->name, RENDER_Q1_MIPTEX_REFERENCE_SEPARATOR, name );
		return qtrue;
	}
	return qfalse;
}
