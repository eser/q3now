// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_background.c -- Shared 3-layer background: base fill, radial glows, grid lines.
*/

#include "../../client.h"
#include "cl_wired_draw.h"
#include "cl_wired_background.h"

#if FEAT_WIRED_UI

#include <stdio.h>   /* sscanf for token colour parsing */

/* Live theme token table lookup (defined in cl_wired_parse.c). The v2
 * palette overlay rewrites these entries on ui_palette_mode/accent change,
 * so reading at draw time picks up the active values. */
extern const char *WiredToken_Find( const char *name );

/* Resolve a theme token's colour by name into an RGB vec (alpha untouched),
 * mirroring cl_wired_clay.c's wui_clay_token_rgb so this render path stays
 * theme-driven. Falls back to the caller's default RGB on miss/unparseable. */
static void wui_bg_token_rgb( const char *tokenName, float outRGB[3] )
{
	const char *v = WiredToken_Find( tokenName );
	unsigned    r, g, b;
	if ( v && v[0] == '#' && ( strlen( v ) == 7 || strlen( v ) == 9 )
	     && sscanf( v + 1, "%2x%2x%2x", &r, &g, &b ) == 3 ) {
		outRGB[0] = (float) r / 255.0f;
		outRGB[1] = (float) g / 255.0f;
		outRGB[2] = (float) b / 255.0f;
	}
}

static qhandle_t wui_radialGlow = 0;

void WUI_BackgroundInit( void ) {
	wui_radialGlow = re.RegisterShaderNoMip( "gfx/ui/glow_radial" );
	// Phase 7.15.4-a class-B pin: a cached WiredUI background handle bound every
	// menu frame without re-resolving residency — the texture-LRU must not evict
	// it. Dark in 7.15.4-a (no reader yet).
	if ( re.PinShaderImages ) re.PinShaderImages( wui_radialGlow );
}

void WUI_DrawBackground( float x, float y, float w, float h ) {
	float vpH = (float)cls.glconfig.vidHeight;

	/* Layer 1: dark base fill */
	{
		vec4_t bg = { 0.031f, 0.047f, 0.063f, 1.0f };  /* $bg fallback (#080c10) */
		wui_bg_token_rgb( "bg", bg );  /* theme-driven; follows ui_palette_mode */
		re.SetColor( bg );
		re.DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cls.whiteShader );
		re.SetColor( NULL );
	}

	/* Layer 2: radial glows */
	if ( wui_radialGlow ) {
		vec4_t leftColor  = { 0.102f, 0.227f, 0.431f, 0.6f  };
		vec4_t rightColor = { 0.431f, 0.102f, 0.102f, 0.18f };

		re.SetColor( leftColor );
		re.DrawStretchPic( x - w*0.1f, y + h*0.1f, w*0.7f, h*0.8f, 0, 0, 1, 1, wui_radialGlow );
		re.SetColor( rightColor );
		re.DrawStretchPic( x + w*0.55f, y + h*0.1f, w*0.6f, h*0.8f, 0, 0, 1, 1, wui_radialGlow );
		re.SetColor( NULL );
	}

	/* Layer 3: horizontal grid lines */
	{
		vec4_t gridColor = { 0.118f, 0.227f, 0.290f, 0.15f };
		WUI_DrawScanlines( x, y, w, h, gridColor, vpH * 0.04f );
	}
}

#endif // FEAT_WIRED_UI
