// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// Contract: the ANIMATED background layer emits the procedural SCENE pass, and
// the retired rect stack has no emitter left anywhere.
//
// The composed backdrop was rewritten once: ~200 Clay rects (the v2
// "DemoBackdrop" — WUI_BG_DEMO_BACKDROP, a stack of sky / arena / ember / fog
// bands) became ONE procedural full-viewport pass, menubg.frag, reached through
// WUI_DrawBackgroundScene. The rect stack was left in the tree as dead code.
//
// When the background layers were later given their own content, the emit site
// for WUI_LAYER_BG_ANIMATED was wired to the RETIRED constant instead of the
// live emitter. Both call the same public API and both compile, so nothing
// complained — but the rect stack has no clock. Its sky, fog and floor are
// static bands, and the only moving parts are 14 ember quads at alpha ~4/255
// over a #0a0403 underpaint. Every `backdrop animated` menu (preferences and
// the ten settings panels beside it, all of which take the zero-value default
// preset) therefore rendered a backdrop that was neither animated nor visible:
// measured warm-tone coverage over the lower half of the frame was 0.1%,
// against 99.8% once the scene pass is actually the thing being emitted.
//
// Nothing at runtime can catch this. Both paths dispatch cleanly, both produce
// draw calls, and the result is a picture rather than an error — the failure
// mode is "the screen is very dark", which no assertion sees. What IS checkable
// is the property the call site must have, so this reads the production source
// and requires that the ANIMATED layer reach WUI_DrawBackgroundScene and that
// WUI_BG_DEMO_BACKDROP be referenced by nobody.
//
// WUI_CLAY_SOURCE / WUI_BG_SOURCE / WUI_BG_HEADER are set by CMake to the
// absolute paths of the production files.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WUI_CLAY_SOURCE
#error "WUI_CLAY_SOURCE must be defined (absolute path to code/client/wired/ui/cl_wired_clay.c)"
#endif
#ifndef WUI_BG_SOURCE
#error "WUI_BG_SOURCE must be defined (absolute path to code/client/wired/ui/cl_wired_bg.c)"
#endif
#ifndef WUI_BG_HEADER
#error "WUI_BG_HEADER must be defined (absolute path to code/client/wired/ui/cl_wired_bg.h)"
#endif

#define MAX_SRC  ( 4 * 1024 * 1024 )

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

static size_t ReadAll( const char *path, char *dst, size_t cap )
{
	FILE   *f = fopen( path, "rb" );
	size_t  n;
	if ( !f ) return 0;
	n = fread( dst, 1, cap - 1, f );
	fclose( f );
	dst[ n ] = '\0';
	return n;
}

// Count non-overlapping occurrences of `needle` in `hay`.
static int CountOf( const char *hay, const char *needle )
{
	int         n   = 0;
	size_t      len = strlen( needle );
	const char *p   = hay;
	while ( ( p = strstr( p, needle ) ) != NULL ) { n++; p += len; }
	return n;
}

// The body of the `if ( WiredUI_LayerVisible( WUI_LAYER_BG_ANIMATED ) )` emit
// arm, as a bounded slice of the compositor source. Reading a slice rather than
// the whole file is what stops the check passing just because the right call
// happens to exist somewhere else in a 6000-line file.
//
// The arm is found by its EMIT-SITE spelling — the visibility test used as an
// `if` condition inside the background pass, which is the only place the layer
// decides what to draw. It ends at the next statement that emits, i.e. the
// scrim arm. Both endpoints are matched on the full parenthesised form so the
// earlier file-scope declaration of wui_clay_menu_scrim cannot terminate the
// slice before it starts.
static const char *AnimatedArm( const char *src, size_t *outLen )
{
	const char *at = strstr( src, "if ( WiredUI_LayerVisible( WUI_LAYER_BG_ANIMATED ) )" );
	const char *end;
	if ( !at ) return NULL;
	end = strstr( at, "if ( wui_clay_menu_scrim )" );
	if ( !end ) end = at + strlen( at );
	if ( end <= at ) return NULL;
	*outLen = (size_t)( end - at );
	return at;
}

// Does `slice` contain `needle` in CODE, rather than only inside a comment?
// The emit arms here carry long explanatory comments that legitimately NAME the
// retired constant while not emitting it, so a plain strstr would report the
// prose. Strips /* */ and // spans, then searches what is left.
static int ContainsInCode( const char *slice, size_t len, const char *needle )
{
	char  *code = (char *)malloc( len + 1 );
	size_t i, o = 0;
	int    found;
	if ( !code ) return 0;
	for ( i = 0; i < len; i++ ) {
		if ( slice[i] == '/' && i + 1 < len && slice[i+1] == '*' ) {
			i += 2;
			while ( i + 1 < len && !( slice[i] == '*' && slice[i+1] == '/' ) ) i++;
			i++;                     /* loop's i++ steps past the '/' */
			continue;
		}
		if ( slice[i] == '/' && i + 1 < len && slice[i+1] == '/' ) {
			while ( i < len && slice[i] != '\n' ) i++;
			continue;
		}
		code[ o++ ] = slice[i];
	}
	code[ o ] = '\0';
	found = strstr( code, needle ) != NULL;
	free( code );
	return found;
}

int main( void )
{
	static char clay[ MAX_SRC ];
	static char bg  [ MAX_SRC ];
	static char hdr [ MAX_SRC ];
	size_t      clayLen, bgLen, hdrLen;
	size_t      armLen = 0;
	const char *arm;

	clayLen = ReadAll( WUI_CLAY_SOURCE, clay, sizeof( clay ) );
	bgLen   = ReadAll( WUI_BG_SOURCE,   bg,   sizeof( bg   ) );
	hdrLen  = ReadAll( WUI_BG_HEADER,   hdr,  sizeof( hdr  ) );

	// Sources we failed to read would make every search below come up empty and
	// let the file pass vacuously, so establish we have the real things first.
	Check( "compositor source is non-empty", clayLen > 4096, 1 );
	Check( "background source is non-empty", bgLen   > 4096, 1 );
	Check( "background header is non-empty", hdrLen  > 512,  1 );
	Check( "compositor source is the compositor",
		strstr( clay, "WiredUI_CompositorEmitFrame" ) != NULL, 1 );

	// ── 1. The ANIMATED layer emits the SCENE pass ────────────────────────
	// The scene emitter has to exist and be reachable. It is the only path to
	// re.DrawMenuBackdrop, so if it has no caller the shader never runs.
	Check( "scene emitter is defined",
		strstr( bg, "void WUI_DrawBackgroundScene(" ) != NULL, 1 );
	Check( "scene emitter reaches the procedural backdrop",
		strstr( bg, "WUI_EmitSceneBackdrop(" ) != NULL, 1 );

	arm = AnimatedArm( clay, &armLen );
	Check( "found the ANIMATED emit arm in the compositor", arm != NULL, 1 );

	if ( arm ) {
		Check( "ANIMATED layer emits the procedural scene",
			ContainsInCode( arm, armLen, "WUI_DrawBackgroundScene(" ), 1 );
		// The specific regression: the retired rect stack wired into this arm.
		// Checked in CODE only — the arm carries a comment that names the
		// constant precisely to explain why it must not be emitted here.
		Check( "ANIMATED layer does NOT emit the retired rect stack",
			ContainsInCode( arm, armLen, "WUI_BG_DEMO_BACKDROP" ), 0 );
	}

	// WUI_DrawBackgroundScene must be CALLED, not merely declared. A declaration
	// and a definition both mention the name, so require an occurrence beyond
	// those two — i.e. an actual call site in the compositor.
	Check( "scene emitter has a call site in the compositor",
		CountOf( clay, "WUI_DrawBackgroundScene(" ) >= 1, 1 );

	// ── 2. The retired rect stack reaches no background emit ──────────────
	// WUI_BG_DEMO_BACKDROP legitimately survives in two places: the header
	// DEFINES it, and WUI_BackgroundParseFlags maps the modder-facing `effects
	// "demo"` keyword onto it — a public authoring API that has to keep
	// resolving. What must never happen again is the compositor handing it to
	// WUI_DrawBackgroundLayered as a layer's content, so pin that shape
	// directly rather than banning the name.
	Check( "compositor never passes the retired stack to a background emit",
		strstr( clay, "WUI_DrawBackgroundLayered( 0.0f, 0.0f, bw, bh, WUI_BG_DEMO_BACKDROP )" ) == NULL, 1 );
	Check( "retired stack survives only as the header definition and the flag parser",
		CountOf( bg, "WUI_BG_DEMO_BACKDROP" ) <= 1, 1 );

	// ── 3. The scene pass composites in the right colour space ────────────
	// menubg.frag writes into the LINEAR UI colour buffer that gamma.frag later
	// encodes to sRGB. Its palette constants are sRGB design tokens, so they have
	// to be decoded first. Skipping that decode is invisible at runtime — the
	// backdrop simply comes out at roughly value^(1/2.2), a washed-out mid-brown
	// that swallows the menu's contrast — so pin the decode at the source.
#ifdef WUI_MENUBG_SHADER
	{
		static char frag[ MAX_SRC ];
		size_t      fragLen = ReadAll( WUI_MENUBG_SHADER, frag, sizeof( frag ) );
		Check( "menubg shader is non-empty", fragLen > 512, 1 );
		Check( "menubg shader decodes its sRGB palette to linear",
			strstr( frag, "srgbToLinear" ) != NULL, 1 );
	}
#endif

	if ( failures ) {
		printf( "\n%d check(s) failed\n", failures );
		return 1;
	}
	printf( "\nall checks passed\n" );
	return 0;
}
