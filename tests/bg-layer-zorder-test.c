// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// Contract: every background quad in cl_wired_bg.c is pinned to the background
// z-plane.
//
// Each background quad is a Clay floating attach-to-root element, so Clay treats
// it as its own tree root and sorts the roots by zIndex before painting. Within
// one zIndex the sort is stable, so emit order survives — and emit order is the
// painter's order the file is written in: the opaque underpaint first, the
// atmosphere over it, the vignette last.
//
// That only holds while every quad names the same zIndex. The v2 layers routed
// through wui_bg_emit_rect and carried WUI_BG_SCENE_ZINDEX (-10); the legacy six
// (base / grid / scanlines / glow_rays / noise / vignette) opened CLAY() inline
// and left .zIndex at its 0 default. With bg_dark and bg_animated both visible —
// what the default preset gives a menu opened over attract — the legacy BASE
// fill is an opaque full-viewport rgba(8,12,16,255) emitted FIRST, yet it sorted
// to the FRONT of the entire -10 stack and repainted the animated backdrop to
// near-black. The background read as "not rendering at all": ~190 quads were
// dispatched and drawn every frame, underneath one opaque quad.
//
// Nothing at runtime can see this. The zIndex is a literal at each CLAY() call
// site, a quad that carries the wrong one still draws, and the result is a
// picture rather than an error — which is why the bug survived a screenshot
// harness. What is checkable is the property the call sites must have, so this
// test reads the production source and requires that every `.floating = {` block
// in it names WUI_BG_SCENE_ZINDEX. Adding a background layer without the zIndex
// reintroduces the regression and fails here.
//
// WUI_BG_SOURCE is set by CMake to the absolute path of the production file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WUI_BG_SOURCE
#error "WUI_BG_SOURCE must be defined (absolute path to code/client/wired/ui/cl_wired_bg.c)"
#endif
#ifndef WUI_BG_HEADER
#error "WUI_BG_HEADER must be defined (absolute path to code/client/wired/ui/cl_wired_bg.h)"
#endif

#define MAX_SRC  ( 1024 * 1024 )

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

// Line number of the byte offset `at`, 1-based — so a failure names the call
// site rather than making the reader go find it.
static int LineOf( const char *src, size_t at )
{
	size_t i;
	int    line = 1;
	for ( i = 0; i < at; i++ )
		if ( src[i] == '\n' ) line++;
	return line;
}

int main( void )
{
	static char  src[ MAX_SRC ];
	FILE        *f;
	size_t       len, i;
	int          floatingBlocks = 0;
	int          pinnedBlocks   = 0;
	int          firstBadLine   = 0;

	f = fopen( WUI_BG_SOURCE, "rb" );
	if ( !f ) {
		printf( "FAIL: cannot open %s\n", WUI_BG_SOURCE );
		return 1;
	}
	len = fread( src, 1, sizeof( src ) - 1, f );
	fclose( f );
	src[ len ] = '\0';

	// A source we failed to actually read would make every count below zero and
	// the whole file pass vacuously, so establish that we have the real thing.
	Check( "production source is non-empty", len > 4096, 1 );
	Check( "production source is the background file",
		strstr( src, "WUI_DrawBackgroundLayered" ) != NULL, 1 );
	Check( "z-plane constant is referenced at all",
		strstr( src, "WUI_BG_SCENE_ZINDEX" ) != NULL, 1 );

	// Walk every `.floating = {` block and require WUI_BG_SCENE_ZINDEX inside it.
	// Every background quad in this file is a floating attach-to-root element —
	// that is what makes it its own sortable Clay tree root — so the floating
	// block is the exact place the z-plane has to be named.
	for ( i = 0; i + 1 < len; i++ ) {
		const char *open, *close;
		int         depth;
		size_t      j;

		if ( strncmp( src + i, ".floating", 9 ) != 0 )
			continue;
		open = strchr( src + i, '{' );
		if ( !open )
			continue;

		// Find the matching brace so a nested initialiser (.offset, .attachPoints)
		// does not end the block early.
		depth = 0;
		close = NULL;
		for ( j = (size_t)( open - src ); j < len; j++ ) {
			if ( src[j] == '{' ) depth++;
			else if ( src[j] == '}' ) {
				depth--;
				if ( depth == 0 ) { close = src + j; break; }
			}
		}
		if ( !close )
			continue;

		floatingBlocks++;
		{
			size_t blockLen = (size_t)( close - open );
			char  *block    = (char *)malloc( blockLen + 1 );
			if ( !block ) { printf( "FAIL: out of memory\n" ); return 1; }
			memcpy( block, open, blockLen );
			block[ blockLen ] = '\0';
			if ( strstr( block, "WUI_BG_SCENE_ZINDEX" ) != NULL ) {
				pinnedBlocks++;
			} else if ( firstBadLine == 0 ) {
				firstBadLine = LineOf( src, i );
			}
			free( block );
		}
		i = (size_t)( close - src );
	}

	// The file emits base, two grid passes, scanlines, glow_rays, noise, vignette
	// and the shared wui_bg_emit_rect helper. If this count collapses, the walk
	// stopped finding call sites and the equality below would pass on nothing.
	Check( "found the background call sites", floatingBlocks >= 8, 1 );

	if ( firstBadLine )
		printf( "     first unpinned .floating block at %s:%d\n",
			WUI_BG_SOURCE, firstBadLine );
	Check( "every .floating background quad names WUI_BG_SCENE_ZINDEX",
		pinnedBlocks, floatingBlocks );

	// The constant has to stay negative: the menu content root is floating at
	// zIndex 0, so a "fix" that equalised the background by moving every quad to
	// 0 would satisfy the check above and put the opaque underpaint over the menu.
	// Read the value out of the header rather than including it — the header
	// pulls q_shared.h and this stays a standalone TU — but read it from the
	// production text so the check cannot drift away from what ships.
	{
		static char  hdr[ MAX_SRC ];
		FILE        *hf = fopen( WUI_BG_HEADER, "rb" );
		int          z  = 0;
		int          parsed = 0;
		if ( hf ) {
			size_t      hlen = fread( hdr, 1, sizeof( hdr ) - 1, hf );
			const char *def;
			fclose( hf );
			hdr[ hlen ] = '\0';
			def = strstr( hdr, "#define WUI_BG_SCENE_ZINDEX" );
			if ( def && sscanf( def, "#define WUI_BG_SCENE_ZINDEX %*[ (]%d", &z ) == 1 )
				parsed = 1;
		}
		Check( "z-plane constant parsed from the header", parsed, 1 );
		Check( "z-plane constant is negative", z < 0, 1 );
	}

	if ( failures ) {
		printf( "\n%d check(s) failed\n", failures );
		return 1;
	}
	printf( "\nall checks passed (%d background quads pinned)\n", pinnedBlocks );
	return 0;
}
