// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// Contract: the composed menu backdrop stays inside its per-frame emit budget.
//
// Every background layer is a stack of Clay floating quads, and the count is a
// pure function of compile-time constants: the sky is an N-band gradient, the
// floor fade is FN bands, the fog is N bands, the noise is a fixed dot count,
// the embers a fixed particle count. WUI_BG_DEMO_BACKDROP -- the preset the
// default (ANIMATED) menu gets -- composes sky + arena silhouette + embers +
// fog + vignette + noise, and each of those numbers is a literal in the
// production file.
//
// Nothing stops a future layer from raising one of those literals, or adding a
// new band stack, and nothing at runtime would complain: the frame still draws,
// it just draws more. The cost shows up as frame time on someone's machine
// weeks later, which is the failure mode this pins. Measured on macOS arm64 the
// composed backdrop is ~0.1 ms of GPU per frame at 1280x720 -- comfortably
// inside budget -- but that headroom is only true at this emit count, and the
// count is the thing a patch can change without noticing.
//
// So this reads the production source, extracts each layer's declared count,
// and checks the composed total against a budget. It is deliberately a budget
// (an upper bound) and not an equality: a layer that gets CHEAPER should not
// fail, and small deliberate increases stay possible by moving the bound in the
// same commit that spends it -- which is exactly the review moment this exists
// to create.
//
// Reading the source rather than linking the real emitter is the same tradeoff
// bg-layer-zorder-test.c makes and for the same reason: cl_wired_bg.c includes
// client.h and is not separable from the engine, so a standalone TU cannot call
// it. The constants ARE the emit count, so reading them is not a proxy.
//
// WUI_BG_SOURCE / WUI_BG_HEADER are set by CMake to the production paths.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef WUI_BG_SOURCE
#error "WUI_BG_SOURCE must be defined (absolute path to code/client/wired/ui/cl_wired_bg.c)"
#endif
#ifndef WUI_BG_HEADER
#error "WUI_BG_HEADER must be defined (absolute path to code/client/wired/ui/cl_wired_bg.h)"
#endif

#define MAX_SRC ( 1024 * 1024 )

/* The composed backdrop the ANIMATED preset draws, as measured on the tree this
 * bound was set against: 1 v2 underpaint + 64 sky bands + 30 silhouette (24
 * floor-fade bands + 4 towers + 2 arches) + 14 embers + 16 fog bands + 64 noise
 * dots + 4 vignette corners = 193, plus the BG_DARK base fill = 194.
 *
 * Headroom of 24 over that leaves room for a small layer without a code change
 * here, while still catching the class this exists for: a band stack that
 * doubles, or a new full-viewport dot/band layer. */
#define BG_EMIT_BUDGET  218

static int failures;

static void Check( const char *what, int got, int want )
{
	if ( got != want ) {
		printf( "FAIL: %s -> got %d, want %d\n", what, got, want );
		failures++;
	} else {
		printf( "ok:   %s (%d)\n", what, got );
	}
}

static void CheckLE( const char *what, int got, int bound )
{
	if ( got > bound ) {
		printf( "FAIL: %s -> %d exceeds budget %d\n", what, got, bound );
		failures++;
	} else {
		printf( "ok:   %s -> %d (budget %d, headroom %d)\n",
			what, got, bound, bound - got );
	}
}

// Read a `#define NAME <int>` out of the text. Returns -1 when absent, so a
// renamed constant collapses the total and trips the sanity floor below rather
// than silently scoring zero for that layer.
static int DefineValue( const char *src, const char *name )
{
	const char *at = src;
	size_t      nameLen = strlen( name );

	while ( ( at = strstr( at, "#define " ) ) != NULL ) {
		const char *p = at + 8;
		while ( *p == ' ' || *p == '\t' ) p++;
		if ( strncmp( p, name, nameLen ) == 0
		  && ( p[ nameLen ] == ' ' || p[ nameLen ] == '\t' ) ) {
			int value;
			if ( sscanf( p + nameLen, "%d", &value ) == 1 )
				return value;
		}
		at += 8;
	}
	return -1;
}

// Read the initialiser of a local band count, e.g. `const int N = 64;` inside
// the named function. Scoped to the function so the several `const int N` in
// this file do not collide.
static int BandCount( const char *src, const char *func, const char *decl )
{
	const char *fn = strstr( src, func );
	const char *at;
	int         value;

	if ( !fn ) return -1;
	at = strstr( fn, decl );
	if ( !at ) return -1;
	at += strlen( decl );
	while ( *at == ' ' || *at == '\t' || *at == '=' ) at++;
	if ( sscanf( at, "%d", &value ) != 1 ) return -1;
	return value;
}

// Count wui_bg_emit_rect calls inside one function body -- the fixed-shape
// emits (towers, arches) that are written out one per line rather than looped.
static int LiteralEmits( const char *src, const char *func, const char *endMarker )
{
	const char *fn = strstr( src, func );
	const char *end;
	const char *at;
	int         n = 0;

	if ( !fn ) return -1;
	end = strstr( fn, endMarker );
	if ( !end ) return -1;

	for ( at = fn; at < end; at++ ) {
		if ( strncmp( at, "wui_bg_emit_rect(", 17 ) == 0
		  || strncmp( at, "wui_bg_emit_rect (", 18 ) == 0 )
			n++;
	}
	return n;
}

int main( void )
{
	static char  src[ MAX_SRC ];
	static char  hdr[ MAX_SRC ];
	FILE        *f;
	size_t       len;
	int          sky, floorBands, fog, noise, embers;
	int          silhouetteLiterals, silhouette, composed, withBase;

	f = fopen( WUI_BG_SOURCE, "rb" );
	if ( !f ) { printf( "FAIL: cannot open %s\n", WUI_BG_SOURCE ); return 1; }
	len = fread( src, 1, sizeof( src ) - 1, f );
	fclose( f );
	src[ len ] = '\0';

	Check( "production source is non-empty", len > 4096, 1 );
	Check( "production source is the background file",
		strstr( src, "WUI_DrawBackgroundLayered" ) != NULL, 1 );

	// The preset this budget is about must still compose the layers it names.
	// If a layer leaves the preset the total below stops describing what ships.
	{
		FILE *hf = fopen( WUI_BG_HEADER, "rb" );
		size_t hlen = 0;
		if ( hf ) { hlen = fread( hdr, 1, sizeof( hdr ) - 1, hf ); fclose( hf ); }
		hdr[ hlen ] = '\0';
		Check( "header readable", hlen > 512, 1 );
		{
			const char *preset = strstr( hdr, "#define WUI_BG_DEMO_BACKDROP" );
			int         haveAll = 0;
			if ( preset ) {
				const char *stop = strstr( preset, "PARALLAX" );
				size_t      span = stop ? (size_t)( stop - preset ) : 0;
				char       *block = span ? (char *)malloc( span + 1 ) : NULL;
				if ( block ) {
					memcpy( block, preset, span );
					block[ span ] = '\0';
					haveAll = strstr( block, "WUI_BG_LAYER_SKY" )
					       && strstr( block, "WUI_BG_LAYER_ARENA_SILHOUETTE" )
					       && strstr( block, "WUI_BG_LAYER_EMBERS" )
					       && strstr( block, "WUI_BG_LAYER_FOG" )
					       && strstr( block, "WUI_BG_LAYER_VIGNETTE" )
					       && strstr( block, "WUI_BG_LAYER_NOISE" ) ? 1 : 0;
					free( block );
				}
			}
			Check( "DEMO_BACKDROP still composes the six budgeted layers", haveAll, 1 );
		}
	}

	sky        = BandCount( src, "wui_bg_emit_v2_sky",        "const int N" );
	fog        = BandCount( src, "wui_bg_emit_v2_fog",        "const int N" );
	floorBands = BandCount( src, "wui_bg_emit_v2_arena_silhouette", "const int   FN" );
	noise      = DefineValue( src, "WUI_BG_NOISE_DOT_COUNT" );
	embers     = DefineValue( src, "WUI_BG_EMBER_COUNT" );
	silhouetteLiterals = LiteralEmits( src, "wui_bg_emit_v2_arena_silhouette",
	                                   "static void wui_bg_emit_v2_embers" );

	// Every count has to have actually parsed. Without this a renamed constant
	// would score -1, shrink the total, and pass the budget vacuously.
	Check( "sky band count parsed",        sky > 0, 1 );
	Check( "fog band count parsed",        fog > 0, 1 );
	Check( "floor fade band count parsed", floorBands > 0, 1 );
	Check( "noise dot count parsed",       noise > 0, 1 );
	Check( "ember count parsed",           embers > 0, 1 );
	Check( "silhouette literal emits parsed", silhouetteLiterals > 0, 1 );

	if ( failures ) {
		printf( "\n%d check(s) failed before budget evaluation\n", failures );
		return 1;
	}

	// silhouette = the floor fade band stack + the fixed tower/arch rects. The
	// literal scan counts the floor-fade call too (it is inside the loop), so
	// subtract that one and add the whole stack.
	silhouette = ( silhouetteLiterals - 1 ) + floorBands;

	// +1 for the v2 underpaint WUI_DrawBackgroundLayered lays down whenever any
	// of sky/silhouette/embers/fog fire.
	composed = 1 + sky + silhouette + embers + fog + noise + 4 /* vignette corners */;
	// A menu on the ANIMATED preset also has BG_DARK up, which adds the base fill.
	withBase = composed + 1;

	printf( "\n  sky bands            %4d\n", sky );
	printf( "  silhouette           %4d  (%d floor bands + %d fixed)\n",
		silhouette, floorBands, silhouetteLiterals - 1 );
	printf( "  embers               %4d\n", embers );
	printf( "  fog bands            %4d\n", fog );
	printf( "  noise dots           %4d\n", noise );
	printf( "  vignette corners     %4d\n", 4 );
	printf( "  v2 underpaint        %4d\n", 1 );
	printf( "  ------------------------\n" );
	printf( "  DEMO_BACKDROP        %4d\n", composed );
	printf( "  + BG_DARK base       %4d\n\n", withBase );

	CheckLE( "composed backdrop emit count (ANIMATED preset menu)",
		withBase, BG_EMIT_BUDGET );

	// The sky is the single largest contributor, and a gradient is the easiest
	// thing to make more expensive by "just adding a few more bands". Bound it
	// on its own so that growth has to be argued for separately rather than
	// hiding inside the total's headroom.
	CheckLE( "sky gradient band count", sky, 64 );

	if ( failures ) {
		printf( "\n%d check(s) failed\n", failures );
		return 1;
	}
	printf( "\nall checks passed\n" );
	return 0;
}
