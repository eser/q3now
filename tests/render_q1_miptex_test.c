// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* Quake 1 embedded textures for the RAL material path (render_q1_miptex.h): miptex lookup in a BSP29 image (non power-of-two extents
 * accepted, out-of-range offsets refused), palette expansion (fullbright colour kept, `{` index 255 transparent), embedded references
 * and the world-material -> reference mapping a cook and a runtime share. */

#include "render_q1_miptex.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_failures;
#define CHECK( x ) do { if ( !( x ) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); s_failures++; } } while ( 0 )

static void PutU32( byte *p, uint32_t v ) { p[0] = (byte)v; p[1] = (byte)( v >> 8 ); p[2] = (byte)( v >> 16 ); p[3] = (byte)( v >> 24 ); }

/* A texture lump with two miptex: "carch03" 16x12 (not a power of two) and "{grate" 8x8. */
static size_t BuildLump( byte *lump ) {
	const uint32_t first = 4u + 2u * 4u, second = first + 40u + 16u * 12u;
	memset( lump, 0, 1024 );
	PutU32( lump, 2u ); PutU32( lump + 4, first ); PutU32( lump + 8, second );
	memcpy( lump + first, "carch03", 7 ); PutU32( lump + first + 16, 16u ); PutU32( lump + first + 20, 12u ); PutU32( lump + first + 24, 40u );
	for ( uint32_t i = 0u; i < 16u * 12u; ++i ) lump[first + 40u + i] = (byte)( i == 0u ? 230u : 1u );   /* pixel 0 fullbright */
	memcpy( lump + second, "{grate", 6 ); PutU32( lump + second + 16, 8u ); PutU32( lump + second + 20, 8u ); PutU32( lump + second + 24, 40u );
	for ( uint32_t i = 0u; i < 64u; ++i ) lump[second + 40u + i] = (byte)( i == 0u ? 255u : 2u );        /* pixel 0 cut out */
	return second + 40u + 64u;
}

int main( void ) {
	static byte bsp[4096], lump[1024];
	const size_t lumpLength = BuildLump( lump );
	const uint32_t lumpOffset = 4u + 15u * 8u;
	const byte *pixels;
	uint32_t width, height;
	byte rgba[16u * 12u * 4u];
	char image[MAX_QPATH];
	mapFile_t map;

	memset( bsp, 0, sizeof( bsp ) );
	PutU32( bsp, 29u );
	PutU32( bsp + 4 + 2 * 8, lumpOffset ); PutU32( bsp + 4 + 2 * 8 + 4, (uint32_t)lumpLength );
	memcpy( bsp + lumpOffset, lump, lumpLength );

	CHECK( RenderQ1Miptex_IsReference( "maps/e1m5.bsp#carch03" ) );
	CHECK( !RenderQ1Miptex_IsReference( "textures/base_wall/concrete" ) );

	CHECK( RenderQ1Miptex_Find( bsp, sizeof( bsp ), "CARCH03", &pixels, &width, &height ) && width == 16u && height == 12u );
	CHECK( !RenderQ1Miptex_Find( bsp, sizeof( bsp ), "missing", &pixels, &width, &height ) );
	CHECK( !RenderQ1Miptex_Find( bsp, lumpOffset + 8u, "carch03", &pixels, &width, &height ) );   /* lump past the file end */
	bsp[0] = 30u;   /* not BSP29 / BSP2 */
	CHECK( !RenderQ1Miptex_Find( bsp, sizeof( bsp ), "carch03", &pixels, &width, &height ) );
	bsp[0] = 29u;

	CHECK( RenderQ1Miptex_Find( bsp, sizeof( bsp ), "carch03", &pixels, &width, &height ) );
	RenderQ1Miptex_ExpandRgba8( "carch03", pixels, width * height, rgba );
	CHECK( rgba[0] == render_q1_palette[230u * 3u] && rgba[3] == 255u );            /* fullbright keeps its colour */
	CHECK( rgba[4] == render_q1_palette[3u] && rgba[7] == 255u );
	CHECK( RenderQ1Miptex_Find( bsp, sizeof( bsp ), "{grate", &pixels, &width, &height ) && width == 8u );
	RenderQ1Miptex_ExpandRgba8( "{grate", pixels, width * height, rgba );
	CHECK( rgba[3] == 0u && rgba[7] == 255u );                                    /* index 255 cut out */

	memset( &map, 0, sizeof( map ) );
	snprintf( map.name, sizeof( map.name ), "maps/e1m5.bsp" );
	CHECK( !RenderQ1Miptex_MaterialImage( &map, "carch03", image ) );             /* no embedded lump: a Quake 3 map */
	map.embeddedTextures = lump; map.embeddedTexturesLength = (int)lumpLength;
	CHECK( RenderQ1Miptex_MaterialImage( &map, "CARCH03", image ) && !strcmp( image, "maps/e1m5.bsp#carch03" ) );
	CHECK( !RenderQ1Miptex_MaterialImage( &map, "textures/base_wall/concrete", image ) );

	if ( s_failures ) return 1;
	printf( "render q1 miptex contract: PASS\n" );
	return 0;
}
