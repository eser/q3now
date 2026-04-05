/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024-2026 q3now contributors

This file is part of q3now source code.

q3now source code is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

q3now source code is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with q3now source code; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_loading_ui.c -- main loading screen renderer
//
// Layout (viewport-relative):
//   Top bar (5.8%):       q3now · gametype · fraglimit · timelimit
//   Left panel (52%):     BSP wireframe preview with entity markers
//   Right panel (48%):    Map info, streaming rows, overall bar, Vulkan badge

#include "client.h"
#include "wired/cl_wired_text.h"
#include "wired/cl_wired_background.h"

// Viewport-relative font sizes
#define LOADING_FONT_TITLE   (cls.glconfig.vidHeight * 0.028f)
#define LOADING_FONT_LABEL   (cls.glconfig.vidHeight * 0.013f)
#define LOADING_FONT_SMALL   (cls.glconfig.vidHeight * 0.011f)

// Set to 1 to enable per-function diagnostic prints during loading.
// Remove or set to 0 once debugging is complete.
#define LOADING_DIAG 1

// Diagnostic frame counter — print only on first 3 frames per map load
static int s_diagFrames;

// -----------------------------------------------------------------------
// Internal state: smoothly interpolated progress values
// -----------------------------------------------------------------------
static float s_dispGeometry;
static float s_dispShaders;
static float s_dispAudio;
static float s_dispDownload;
static float s_dispOverall;

// Pulse timer for the animated dot
static int s_pulsePhase;

// Radial glow texture (white-to-transparent falloff)
static qhandle_t s_glowShader;
static int s_glowDiagCount;

// Console notify suppression
static int s_savedNotifyTime = -1;

// Wireframe diagnostics: print once per map load
static qboolean s_wireframeDiagPrinted;

// Fade transition state
float cl_loadFadeAlpha = 0.0f;
qboolean cl_loadFading = qfalse;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

/*
================
Loading_SetColor

Fade-aware color setter. Multiplies the alpha channel by cl_loadFadeAlpha
during the fade-out transition. During normal loading (not fading),
cl_loadFadeAlpha is 1.0 so colors pass through unchanged.
================
*/
static void Loading_SetColor( const float *rgba ) {
	if ( !rgba ) {
		re.SetColor( NULL );
		return;
	}
	if ( cl_loadFadeAlpha >= 1.0f ) {
		re.SetColor( rgba );
	} else {
		vec4_t faded;
		faded[0] = rgba[0];
		faded[1] = rgba[1];
		faded[2] = rgba[2];
		faded[3] = rgba[3] * cl_loadFadeAlpha;
		re.SetColor( faded );
	}
}

/*
================
Loading_FillRect

Fade-aware SCR_FillRect. Multiplies alpha by fade factor.
================
*/
static void Loading_FillRect( float x, float y, float w, float h, const float *rgba ) {
	vec4_t c;
	Vector4Copy( rgba, c );
	if ( cl_loadFadeAlpha < 1.0f ) {
		c[3] *= cl_loadFadeAlpha;
	}
	re.SetColor( c );
	re.DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );
}

/*
================
Loading_DrawStringFaded

Fade-aware string draw. Multiplies text color alpha by fade factor.
================
*/
static void Loading_DrawStringFaded( int x, int y, float charSize,
									 const char *text, const float *rgba ) {
	vec4_t color;
	color[0] = rgba[0];
	color[1] = rgba[1];
	color[2] = rgba[2];
	color[3] = ( cl_loadFadeAlpha >= 1.0f ) ? rgba[3] : rgba[3] * cl_loadFadeAlpha;

	Text_Draw( text, x, y, FONT_UI, charSize, color, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW );
}

/*
================
Loading_DrawLine

Draws a line in real screen pixel coordinates.
================
*/
static void Loading_DrawLine( float x1, float y1, float x2, float y2,
							  float width, qhandle_t hShader ) {
	// coordinates are already real screen pixels
	re.DrawLine( x1, y1, x2, y2, width, hShader );
}

/*
================
Loading_LerpValue

Smooth interpolation for displayed progress toward target at ~8% per frame.
================
*/
static float Loading_LerpValue( float current, float target ) {
	float delta = target - current;
	if ( delta > 0.001f ) {
		current += delta * 0.08f;
		if ( current > target ) {
			current = target;
		}
	} else if ( delta < -0.001f ) {
		current = target;  // snap backwards
	}
	return current;
}

// -----------------------------------------------------------------------
// Drawing routines
// -----------------------------------------------------------------------

/*
================
Loading_DrawRadialGlow

Draws a radial glow using a pre-baked falloff texture
(gfx/loading/glow_radial — white center, transparent edge).
Color-modulated via re.SetColor, drawn with SCR_DrawPic.
Single draw call per glow.
================
*/
static void Loading_DrawRadialGlow( float cx, float cy, float radius,
									const float *rgb, float peakAlpha ) {
	vec4_t color;
	float x, y, w, h;

	if ( !s_glowShader ) {
		return;
	}

	color[0] = rgb[0];
	color[1] = rgb[1];
	color[2] = rgb[2];
	color[3] = peakAlpha;

	w = radius * 2.0f;
	h = radius * 2.0f;
	x = cx - radius;
	y = cy - radius;

	{
		if ( s_glowDiagCount < 4 ) {
			Com_Printf( "GLOW virtual: x=%.0f y=%.0f w=%.0f h=%.0f (cx=%.0f cy=%.0f r=%.0f) rgba=(%.3f %.3f %.3f %.2f)\n",
						x, y, w, h, cx, cy, radius, color[0], color[1], color[2], color[3] );
			s_glowDiagCount++;
		}
	}

	Loading_SetColor( color );
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, s_glowShader );
	Loading_SetColor( NULL );
}

/*
================
Loading_DrawBackground

1. Full-screen filled rect in bgColor.
2. Two radial glow overlays (left-center and right-center).
3. Horizontal grid lines at ~4% viewport height spacing.
All coordinates are viewport-relative (real screen pixels).
================
*/
static void Loading_DrawBackground( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	float y;
	vec4_t gridLine;
	// Exact spec colors — no amplification
	static const float rightGlowRGB[3] = {
		110.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f   // #6e1a1a
	};

	// Re-register every frame — Hunk_Clear between maps invalidates
	// cached handles, and RegisterShaderNoMip is a fast hash lookup
	// when the shader is already loaded.
	s_glowShader = re.RegisterShaderNoMip( "gfx/loading/glow_radial" );

	// --- 1. Full-screen dark background ---
	Loading_FillRect( 0, 0, vpW, vpH, cl_loadingTheme.bgColor );

	// --- 2. Radial glow overlays ---
	// "Size" = total quad dimensions, radius = half-size
	// Left-center glow: brighter blue #1a3a6e, alpha 0.6
	{
		static const float leftGlow[3] = {
			26.0f / 255.0f, 58.0f / 255.0f, 110.0f / 255.0f   // #1a3a6e
		};
		Loading_DrawRadialGlow( vpW * 0.266f, vpH * 0.5f, vpH * 0.3125f,
								leftGlow, 0.60f );
	}

	// Right-center glow: #6e1a1a, alpha 0.18
	Loading_DrawRadialGlow( vpW * 0.797f, vpH * 0.5f, vpH * 0.28125f,
							rightGlowRGB, 0.18f );

	// --- 3. Horizontal grid lines at ~4% viewport height spacing ---
	// Subtle texture: #1e3a4a at 8% alpha — barely visible
	gridLine[0] = 0.118f;  // 0x1e / 255
	gridLine[1] = 0.227f;  // 0x3a / 255
	gridLine[2] = 0.290f;  // 0x4a / 255
	gridLine[3] = 0.08f;

	{
		float spacing = vpH * 0.04f;
		for ( y = 0; y < vpH; y += spacing ) {
			Loading_FillRect( 0, y, vpW, 1, gridLine );
		}
	}
}

/*
================
Loading_DrawTopBar

Semi-transparent dark bar at top with game info.
y=0, h=28.
Left: "q3now" in accent color.
Middle: gametype · fraglimit · timelimit.
Right: bot difficulty dots if g_autoBots is active.
================
*/
static void Loading_DrawTopBar( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	float barH = vpH * 0.058f;           // ~28/480
	float pad  = vpW * 0.0125f;          // ~8/640
	float fontSize = LOADING_FONT_LABEL;
	vec4_t barColor = { 0.05f, 0.05f, 0.08f, 0.8f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 0.8f };
	vec4_t muted = { 1.0f, 1.0f, 1.0f, 0.5f };
	char info[256];
	const char *gtName;
	int fraglimit, timelimit, gt;
	float px;

	// Dark semi-transparent bar
	Loading_FillRect( 0, 0, vpW, barH, barColor );

	// "q3now" in accent color, left side
	Loading_DrawStringFaded( (int)pad, (int)pad, fontSize, "q3now",
					   cl_loadingTheme.accentColor );

	// Separator dot and gametype + limits
	gt = Cvar_VariableIntegerValue( "g_gametype" );
	fraglimit = Cvar_VariableIntegerValue( "fraglimit" );
	timelimit = Cvar_VariableIntegerValue( "timelimit" );

	if ( gt >= 0 && gt < GT_MAX_GAME_TYPE ) {
		gtName = bg_gametypelist[gt].name;
	} else {
		gtName = "DEATHMATCH";
	}

	// Build info string with pipe separators (bitmap font has no middle dot)
	if ( fraglimit > 0 && timelimit > 0 ) {
		Com_sprintf( info, sizeof( info ), "| %s | fraglimit %d | timelimit %d",
					 gtName, fraglimit, timelimit );
	} else if ( fraglimit > 0 ) {
		Com_sprintf( info, sizeof( info ), "| %s | fraglimit %d", gtName, fraglimit );
	} else if ( timelimit > 0 ) {
		Com_sprintf( info, sizeof( info ), "| %s | timelimit %d", gtName, timelimit );
	} else {
		Com_sprintf( info, sizeof( info ), "| %s", gtName );
	}

	// Draw info string after "q3now" label
	px = pad + 5 * fontSize + vpW * 0.006f;  // after "q3now" (5 chars) + small gap
	Loading_DrawStringFaded( (int)px, (int)pad, fontSize, info, muted );

	// Bot difficulty dots (right side) — if g_autoBots is set
	{
		int autoBots = Cvar_VariableIntegerValue( "g_autoBots" );
		if ( autoBots > 0 ) {
			int skill = Cvar_VariableIntegerValue( "g_spSkill" );
			int i;
			float dotSize = vpH * 0.0083f;   // ~4/480
			float dotGap  = vpW * 0.0109f;   // ~7/640
			vec4_t dotOn, dotOff;
			dotOn[0] = cl_loadingTheme.accentColor[0];
			dotOn[1] = cl_loadingTheme.accentColor[1];
			dotOn[2] = cl_loadingTheme.accentColor[2];
			dotOn[3] = 0.9f;
			dotOff[0] = cl_loadingTheme.accentColor[0];
			dotOff[1] = cl_loadingTheme.accentColor[1];
			dotOff[2] = cl_loadingTheme.accentColor[2];
			dotOff[3] = 0.2f;
			for ( i = 0; i < 5; i++ ) {
				float dx = vpW * 0.9375f + i * dotGap;
				Loading_FillRect( dx, vpH * 0.025f, dotSize, dotSize,
							  ( i < skill ) ? dotOn : dotOff );
			}
		}
	}
}

/*
================
Loading_DrawDivider

1px vertical divider at 52% of viewport width.
Three segments: top (#1e3a4a, 50% alpha), accent center (#00b4d8,
90% alpha), bottom (#1e3a4a, 50% alpha).
================
*/
static void Loading_DrawDivider( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	vec4_t segColor;
	vec4_t accentColor;
	float x = vpW * 0.522f;  // 52.2% of viewport (was 334/640)
	float yTop    = vpH * 0.1417f;  // was 68/480
	float yMid1   = vpH * 0.4792f;  // was 230/480
	float yMid2   = vpH * 0.5208f;  // was 250/480
	float yBottom = vpH * 0.8583f;  // was 412/480

#if LOADING_DIAG
	if ( s_diagFrames < 3 ) {
		Com_Printf( "DIAG Divider: x=%.0f yTop=%.0f yMid=%.0f yBot=%.0f\n",
			x, yTop, yMid1, yBottom );
	}
#endif
	// Use Loading_DrawLine for sub-pixel precision on high-DPI displays.
	// SCR_FillRect with 1px width can round to zero at some resolutions.
	segColor[0] = 0x1e / 255.0f;
	segColor[1] = 0x3a / 255.0f;
	segColor[2] = 0x4a / 255.0f;
	segColor[3] = 0.6f;

	accentColor[0] = 0x00 / 255.0f;
	accentColor[1] = 0xb4 / 255.0f;
	accentColor[2] = 0xd8 / 255.0f;
	accentColor[3] = 1.0f;

	// Top segment
	Loading_SetColor( segColor );
	Loading_DrawLine( x, yTop, x, yMid1, 1.0f, cls.whiteShader );
	// Accent center
	Loading_SetColor( accentColor );
	Loading_DrawLine( x, yMid1, x, yMid2, 1.5f, cls.whiteShader );
	// Bottom segment
	Loading_SetColor( segColor );
	Loading_DrawLine( x, yMid2, x, yBottom, 1.0f, cls.whiteShader );
	Loading_SetColor( NULL );
}

/*
================
Loading_DrawWireframe

Left panel (52% of viewport width):
If cl_bspPreview.valid, draw all edges using lines in accent color.
Scale edges to fit within a centered area in the panel.
Entity markers: spawn=diamond, item=filled square, flag=outlined square.
Animated: 2D rotation (360deg/12s) and gentle Y float.
Legend at bottom-left.
================
*/
static void Loading_DrawWireframe( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	// Left panel occupies 0..52% of viewport
	const float panelW = vpW * 0.519f;    // was 332/640
	const float panelH = vpH * 0.875f;    // was 420/480
	const float panelY = vpH * 0.075f;    // was 36/480
	// Center a fit area in the left panel (~58% of viewport height)
	const float fitSize = vpH * 0.583f;   // was 280/480
	const float fitX = ( panelW - fitSize ) * 0.5f;
	const float fitY = panelY + ( panelH - fitSize ) * 0.5f;
	float scaleX, scaleY, scale, offX, offY;
	float rangeX, rangeY;
	int i;
	// Use Sys_Milliseconds() instead of cls.realtime for animations.
	// During server loading, cls.realtime is frozen (only updated in
	// CL_Frame which doesn't run during loading yield points).
	// Sys_Milliseconds() reads the real OS clock so animations stay smooth.
	int now = Sys_Milliseconds();
	// 2D rotation: 360 degrees over 12 seconds
	float angle = (float)( now % 12000 ) / 12000.0f * 2.0f * M_PI;
	float cosA = cos( angle );
	float sinA = sin( angle );
	// Gentle float: ±0.0125 of viewport height over 6 seconds (sine wave)
	float floatY = vpH * 0.0125f * sin( (float)( now % 6000 ) / 6000.0f * 2.0f * M_PI );
	// Center of the fit area for rotation pivot
	float pivotX = fitX + fitSize * 0.5f;
	float pivotY = fitY + fitSize * 0.5f;

#if LOADING_DIAG
	if ( s_diagFrames < 3 )
		Com_Printf( "DIAG Wireframe: valid=%d edges=%d markers=%d surfaces=%d "
			"bounds=(%.0f,%.0f)-(%.0f,%.0f)\n",
			cl_bspPreview.valid, cl_bspPreview.numEdges, cl_bspPreview.numMarkers,
			cl_bspPreview.numSurfaces,
			cl_bspPreview.minX, cl_bspPreview.minY,
			cl_bspPreview.maxX, cl_bspPreview.maxY );
#endif
	if ( !cl_bspPreview.valid || cl_bspPreview.numEdges == 0 ) {
		// No preview data — draw placeholder text
		vec4_t muted = { 1.0f, 1.0f, 1.0f, 0.25f };
		Loading_DrawStringFaded( (int)( panelW * 0.5f - vpW * 0.0625f ),
						   (int)( panelY + panelH * 0.5f ),
						   LOADING_FONT_LABEL, "loading...", muted );
		return;
	}

	// Compute scale to fit BSP bounds into the fit area
	rangeX = cl_bspPreview.maxX - cl_bspPreview.minX;
	rangeY = cl_bspPreview.maxY - cl_bspPreview.minY;
	if ( rangeX < 1.0f ) rangeX = 1.0f;
	if ( rangeY < 1.0f ) rangeY = 1.0f;

	scaleX = fitSize / rangeX;
	scaleY = fitSize / rangeY;
	scale = ( scaleX < scaleY ) ? scaleX : scaleY;

	// Center offset within the fit area
	offX = fitX + ( fitSize - rangeX * scale ) * 0.5f;
	offY = fitY + ( fitSize - rangeY * scale ) * 0.5f;

	// Height coloring: lerp per-edge color based on average Z
	// t=0.0 (floor): #1a3a5a  →  t=0.5 (mid): #00b4d8  →  t=1.0 (top): #80e0f0
	{
		float zRange = cl_bspPreview.maxZ - cl_bspPreview.minZ;
		if ( zRange < 1.0f ) zRange = 1.0f;

		for ( i = 0; i < cl_bspPreview.numEdges; i++ ) {
			const bspPreviewEdge_t *e = &cl_bspPreview.edges[i];
			float ex1 = offX + ( e->x1 - cl_bspPreview.minX ) * scale;
			float ey1 = offY + ( e->y1 - cl_bspPreview.minY ) * scale;
			float ex2 = offX + ( e->x2 - cl_bspPreview.minX ) * scale;
			float ey2 = offY + ( e->y2 - cl_bspPreview.minY ) * scale;
			float rx1 = pivotX + ( ex1 - pivotX ) * cosA - ( ey1 - pivotY ) * sinA;
			float ry1 = pivotY + ( ex1 - pivotX ) * sinA + ( ey1 - pivotY ) * cosA + floatY;
			float rx2 = pivotX + ( ex2 - pivotX ) * cosA - ( ey2 - pivotY ) * sinA;
			float ry2 = pivotY + ( ex2 - pivotX ) * sinA + ( ey2 - pivotY ) * cosA + floatY;
			// Height parameter from average Z of both vertices
			float avgZ = ( e->z1 + e->z2 ) * 0.5f;
			float t = ( avgZ - cl_bspPreview.minZ ) / zRange;
			vec4_t edgeColor;

			// 3-stop gradient: floor → mid → top
			if ( t < 0.5f ) {
				float s = t * 2.0f;  // 0..1 within first half
				edgeColor[0] = 0.102f + s * ( 0.000f - 0.102f );  // 1a → 00
				edgeColor[1] = 0.227f + s * ( 0.706f - 0.227f );  // 3a → b4
				edgeColor[2] = 0.353f + s * ( 0.847f - 0.353f );  // 5a → d8
			} else {
				float s = ( t - 0.5f ) * 2.0f;  // 0..1 within second half
				edgeColor[0] = 0.000f + s * ( 0.502f - 0.000f );  // 00 → 80
				edgeColor[1] = 0.706f + s * ( 0.878f - 0.706f );  // b4 → e0
				edgeColor[2] = 0.847f + s * ( 0.941f - 0.847f );  // d8 → f0
			}
			edgeColor[3] = 1.0f;

			Loading_SetColor( edgeColor );
			Loading_DrawLine( rx1, ry1, rx2, ry2, 1.0f, cls.whiteShader );
		}
		Loading_SetColor( NULL );
	}

	// Draw markers: apply rotation first, then float offset
	{
	float markerR1 = vpH * 0.0083f;  // spawn circle radius (was 4/480)
	float markerR2 = vpH * 0.00625f; // item circle radius (was 3/480)
	float markerS  = vpH * 0.0083f;  // flag half-size (was 4/480)
	for ( i = 0; i < cl_bspPreview.numMarkers; i++ ) {
		const bspPreviewMarker_t *m = &cl_bspPreview.markers[i];
		float rawX = offX + ( m->x - cl_bspPreview.minX ) * scale;
		float rawY = offY + ( m->y - cl_bspPreview.minY ) * scale;
		float mx = pivotX + ( rawX - pivotX ) * cosA - ( rawY - pivotY ) * sinA;
		float my = pivotY + ( rawX - pivotX ) * sinA + ( rawY - pivotY ) * cosA + floatY;

		switch ( m->type ) {
		case 0: { // spawn — yellow circle (12-segment approximation)
			vec4_t spawnColor = { 1.0f, 0.85f, 0.2f, 0.9f };
			int seg;
			Loading_SetColor( spawnColor );
			for ( seg = 0; seg < 12; seg++ ) {
				float a0 = ( M_PI * 2.0f / 12 ) * seg;
				float a1 = ( M_PI * 2.0f / 12 ) * ( seg + 1 );
				Loading_DrawLine( mx + cos(a0) * markerR1, my + sin(a0) * markerR1,
								  mx + cos(a1) * markerR1, my + sin(a1) * markerR1,
								  1.0f, cls.whiteShader );
			}
			Loading_SetColor( NULL );
			break;
		}
		case 1: { // item — red circle (12-segment)
			vec4_t itemColor = { 1.0f, 0.3f, 0.2f, 0.8f };
			int seg;
			Loading_SetColor( itemColor );
			for ( seg = 0; seg < 12; seg++ ) {
				float a0 = ( M_PI * 2.0f / 12 ) * seg;
				float a1 = ( M_PI * 2.0f / 12 ) * ( seg + 1 );
				Loading_DrawLine( mx + cos(a0) * markerR2, my + sin(a0) * markerR2,
								  mx + cos(a1) * markerR2, my + sin(a1) * markerR2,
								  1.0f, cls.whiteShader );
			}
			Loading_SetColor( NULL );
			break;
		}
		case 2: // flag — outlined rectangle (4 lines)
			Loading_SetColor( cl_loadingTheme.accentColor );
			Loading_DrawLine( mx - markerS, my - markerS, mx + markerS, my - markerS, 1.0f, cls.whiteShader );
			Loading_DrawLine( mx + markerS, my - markerS, mx + markerS, my + markerS, 1.0f, cls.whiteShader );
			Loading_DrawLine( mx + markerS, my + markerS, mx - markerS, my + markerS, 1.0f, cls.whiteShader );
			Loading_DrawLine( mx - markerS, my + markerS, mx - markerS, my - markerS, 1.0f, cls.whiteShader );
			Loading_SetColor( NULL );
			break;
		}
	}
	} // end marker variables block

	// Legend at bottom-left of left panel — colored symbols matching wireframe
	{
		vec4_t labelColor = { 1.0f, 1.0f, 1.0f, 0.5f };
		vec4_t spawnYellow = { 1.0f, 0.84f, 0.04f, 1.0f };  // #ffd60a
		vec4_t itemRed = { 0.90f, 0.22f, 0.27f, 1.0f };     // #e63946
		float legendR1 = vpH * 0.0083f;   // spawn circle radius
		float legendR2 = vpH * 0.00625f;  // item circle radius
		float legendS  = vpH * 0.0083f;   // flag half-size
		float legendGap = vpW * 0.025f;   // gap between icon and text
		float ly = panelY + panelH + vpH * 0.033f;
		float lx = vpW * 0.019f;
		int seg;

		// Spawn: yellow circle
		Loading_SetColor( spawnYellow );
		for ( seg = 0; seg < 12; seg++ ) {
			float a0 = ( M_PI * 2.0f / 12 ) * seg;
			float a1 = ( M_PI * 2.0f / 12 ) * ( seg + 1 );
			Loading_DrawLine( lx + cos(a0) * legendR1, ly + sin(a0) * legendR1,
							  lx + cos(a1) * legendR1, ly + sin(a1) * legendR1,
							  1.0f, cls.whiteShader );
		}
		Loading_SetColor( NULL );
		Loading_DrawStringFaded( (int)(lx + legendGap * 0.5f), (int)(ly - vpH * 0.0083f),
						   LOADING_FONT_LABEL, "spawn", labelColor );

		// Item: red filled circle (approximated with 3 concentric rings)
		lx += vpW * 0.125f;
		Loading_SetColor( itemRed );
		{
			float r;
			float ringStep = legendR2 / 3.0f;
			for ( r = legendR2; r >= ringStep; r -= ringStep ) {
				for ( seg = 0; seg < 12; seg++ ) {
					float a0 = ( M_PI * 2.0f / 12 ) * seg;
					float a1 = ( M_PI * 2.0f / 12 ) * ( seg + 1 );
					Loading_DrawLine( lx + cos(a0) * r, ly + sin(a0) * r,
									  lx + cos(a1) * r, ly + sin(a1) * r,
									  1.0f, cls.whiteShader );
				}
			}
		}
		Loading_SetColor( NULL );
		Loading_DrawStringFaded( (int)(lx + legendGap * 0.5f), (int)(ly - vpH * 0.0083f),
						   LOADING_FONT_LABEL, "item", labelColor );

		// Flag: red outlined square
		lx += vpW * 0.1125f;
		Loading_SetColor( itemRed );
		Loading_DrawLine( lx - legendS, ly - legendS, lx + legendS, ly - legendS, 1.0f, cls.whiteShader );
		Loading_DrawLine( lx + legendS, ly - legendS, lx + legendS, ly + legendS, 1.0f, cls.whiteShader );
		Loading_DrawLine( lx + legendS, ly + legendS, lx - legendS, ly + legendS, 1.0f, cls.whiteShader );
		Loading_DrawLine( lx - legendS, ly + legendS, lx - legendS, ly - legendS, 1.0f, cls.whiteShader );
		Loading_SetColor( NULL );
		Loading_DrawStringFaded( (int)(lx + legendGap * 0.5f), (int)(ly - vpH * 0.0083f),
						   LOADING_FONT_LABEL, "flag", labelColor );
	}

	// Console diagnostic: print once per map load
	if ( !s_wireframeDiagPrinted ) {
		Com_Printf( "WIREFRAME: %d surfaces, %d segments, %d markers\n",
			cl_bspPreview.numSurfaces, cl_bspPreview.numEdges,
			cl_bspPreview.numMarkers );
		s_wireframeDiagPrinted = qtrue;
	}
}

/*
================
Loading_DrawMapInfo

Right panel (x=347..626, y=40..470):
Archetype tag, large map name, author+year, flavor quote,
3-cell stats grid.
================
*/
static void Loading_DrawMapInfo( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	const float rx = vpW * 0.542f;     // right panel x start (was 347/640)
	const float rw = vpW * 0.436f;     // right panel usable width (was 279/640)
	float y = vpH * 0.1f;              // was 48/480
	float pad = vpH * 0.0167f;         // general padding (~8/480)
#if LOADING_DIAG
	if ( s_diagFrames < 3 ) {
		const char *mapCvar = Cvar_VariableString( "mapname" );
		Com_Printf( "DIAG MapInfo: mapname=\"%s\" longName=\"%s\" author=\"%s\" "
			"quote=\"%s\" sky=\"%s\" players=%d-%d weapon=\"%s\" items=%d hasMeta=%d\n",
			mapCvar ? mapCvar : "(null)",
			cl_mapInfo.longName, cl_mapInfo.author, cl_mapInfo.quote,
			cl_mapInfo.sky, cl_mapInfo.playersMin, cl_mapInfo.playersMax,
			cl_mapInfo.metaWeapon, cl_mapInfo.itemNodes, cl_mapInfo.hasMetaFile );
	}
#endif
	// Colors from mockup
	vec4_t cyanTag    = { 0.00f, 0.71f, 0.85f, 0.80f };  // #00b4d8 @ 80%
	vec4_t nameWhite  = { 0.91f, 0.96f, 0.99f, 1.00f };  // #e8f4fd
	vec4_t authorBlue = { 0.33f, 0.47f, 0.67f, 1.00f };  // #5577aa
	vec4_t quoteText  = { 0.29f, 0.42f, 0.53f, 1.00f };  // #4a6a88
	vec4_t quoteBorder = { 0.10f, 0.23f, 0.32f, 1.00f };  // #1a3a52
	vec4_t cellBorder = { 0.12f, 0.23f, 0.29f, 1.00f };  // #1e3a4a
	vec4_t labelMuted = { 0.29f, 0.42f, 0.53f, 1.00f };  // #4a6a88
	const char *name;
	int gt;
	const char *gtName;
	const char *archName;

	// --- Archetype tag (small label, cyan) ---
	// Format: "GAMETYPE | ARCHETYPE"
	gt = Cvar_VariableIntegerValue( "g_gametype" );
	if ( gt >= 0 && gt < GT_MAX_GAME_TYPE ) {
		gtName = bg_gametypelist[gt].name;
	} else {
		gtName = "DEATHMATCH";
	}
	archName = cl_mapInfo.archetype[0] ? cl_mapInfo.archetype : "TECH";
	{
		char tagLine[128];
		char gtUpper[64];
		char archUpper[32];
		Q_strncpyz( gtUpper, gtName, sizeof(gtUpper) );
		Q_strupr( gtUpper );
		Q_strncpyz( archUpper, archName, sizeof(archUpper) );
		Q_strupr( archUpper );
		Com_sprintf( tagLine, sizeof(tagLine), "%s | %s", gtUpper, archUpper );
		Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_LABEL, tagLine, cyanTag );
	}

	// --- Map name (large text, near-white) ---
	// Use mapname cvar, uppercase. Progressive scaling.
	y = vpH * 0.129f;  // was 62/480
	{
		char mapUp[128];
		const char *mapCvar = Cvar_VariableString( "mapname" );
		int nameLen;
		float charSize;

		if ( mapCvar && mapCvar[0] ) {
			Q_strncpyz( mapUp, mapCvar, sizeof(mapUp) );
		} else {
			name = cl_mapInfo.longName[0] ? cl_mapInfo.longName : cl_mapInfo.mapName;
			Q_strncpyz( mapUp, name[0] ? name : "LOADING", sizeof(mapUp) );
		}
		Q_strupr( mapUp );
		nameLen = (int)strlen( mapUp );

		// Progressive scaling to fit panel width
		if ( nameLen > 18 ) {
			charSize = LOADING_FONT_TITLE * 0.625f;  // was 10/16
		} else if ( nameLen > 12 ) {
			charSize = LOADING_FONT_TITLE * 0.75f;   // was 12/16
		} else {
			charSize = LOADING_FONT_TITLE;
		}
		Loading_DrawStringFaded( (int)rx, (int)y, charSize, mapUp,
						   nameWhite );
		y += charSize + vpH * 0.0125f;
	}

	// --- Author + year (small label, blue) ---
	{
		char authorLine[128];
		const char *auth = cl_mapInfo.author[0] ? cl_mapInfo.author : "unknown";
		const char *yr   = cl_mapInfo.year[0] ? cl_mapInfo.year : "--";
		Com_sprintf( authorLine, sizeof(authorLine), "%s | %s", auth, yr );
		Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_LABEL, authorLine,
						   authorBlue );
	}

	// --- Flavor quote (with 2px left border) ---
	y = vpH * 0.2167f;  // was 104/480
	if ( cl_mapInfo.quote[0] ) {
		char line1[128], line2[128];
		float quoteIndent = pad;
		int textX = (int)( rx + quoteIndent );
		float charW = LOADING_FONT_SMALL;  // approximate char width at small scale
		int maxChars = (int)( ( rw - quoteIndent - 2 ) / charW );
		int len = (int)strlen( cl_mapInfo.quote );

		// Word-wrap: split at last space before maxChars
		if ( len > maxChars ) {
			int splitAt = maxChars;
			while ( splitAt > 0 && cl_mapInfo.quote[splitAt] != ' ' ) {
				splitAt--;
			}
			if ( splitAt == 0 ) splitAt = maxChars;  // no space found, hard break
			Q_strncpyz( line1, cl_mapInfo.quote, splitAt + 1 );
			Q_strncpyz( line2, cl_mapInfo.quote + splitAt + 1, sizeof(line2) );

			// 2px left border rect, height for 2 lines
			Loading_FillRect( rx, y, 2, vpH * 0.0708f, quoteBorder );
			Loading_DrawStringFaded( textX, (int)(y + vpH * 0.0083f), LOADING_FONT_SMALL, line1,
							   quoteText );
			Loading_DrawStringFaded( textX, (int)(y + vpH * 0.0375f), LOADING_FONT_SMALL, line2,
							   quoteText );
		} else {
			// Single line fits
			Loading_FillRect( rx, y, 2, vpH * 0.0458f, quoteBorder );
			Loading_DrawStringFaded( textX, (int)(y + vpH * 0.0083f), LOADING_FONT_SMALL, cl_mapInfo.quote,
							   quoteText );
		}
	}

	// --- Stats grid (3 equal columns with per-column accent) ---
	y += vpH * 0.0125f;
	if ( y < vpH * 0.2958f ) y = vpH * 0.2958f;  // was 142/480
	{
		float cellW = rw / 3.0f;
		float cellH = vpH * 0.1f;  // was 48/480
		char stat[32];
		int col;

		// Per-column accent colors (border at 15% alpha, bg at 6% alpha)
		vec4_t colBorder[3] = {
			{ 0.00f, 0.706f, 0.847f, 0.15f },  // cyan — PLAYERS
			{ 0.90f, 0.22f, 0.27f, 0.15f },     // red — WEAPON
			{ 1.00f, 0.84f, 0.04f, 0.15f }      // yellow — ITEMS
		};
		vec4_t colBg[3] = {
			{ 0.00f, 0.706f, 0.847f, 0.06f },
			{ 0.90f, 0.22f, 0.27f, 0.06f },
			{ 1.00f, 0.84f, 0.04f, 0.06f }
		};

		const char *labels[3] = { "PLAYERS", "WEAPON", "ITEMS" };
		const char *values[3];
		char statBufs[3][32];

		// Build value strings
		if ( cl_mapInfo.playersMin > 0 || cl_mapInfo.playersMax > 0 ) {
			Com_sprintf( statBufs[0], sizeof(statBufs[0]), "%d-%d",
						 cl_mapInfo.playersMin, cl_mapInfo.playersMax );
		} else {
			Q_strncpyz( statBufs[0], "--", sizeof(statBufs[0]) );
		}
		values[0] = statBufs[0];

		if ( cl_mapInfo.metaWeapon[0] ) {
			Q_strncpyz( statBufs[1], cl_mapInfo.metaWeapon, sizeof(statBufs[1]) );
		} else {
			Q_strncpyz( statBufs[1], "--", sizeof(statBufs[1]) );
		}
		values[1] = statBufs[1];

		if ( cl_mapInfo.itemNodes > 0 ) {
			Com_sprintf( statBufs[2], sizeof(statBufs[2]), "%d", cl_mapInfo.itemNodes );
		} else {
			Q_strncpyz( statBufs[2], "--", sizeof(statBufs[2]) );
		}
		values[2] = statBufs[2];

		// Draw each cell
		for ( col = 0; col < 3; col++ ) {
			float cx = rx + col * cellW;
			int valLen = (int)strlen( values[col] );
			int lblLen = (int)strlen( labels[col] );

			// Background tint
			Loading_FillRect( cx, y, cellW, cellH, colBg[col] );

			// Cell border (top, bottom, left, right)
			Loading_FillRect( cx, y, cellW, 1, colBorder[col] );
			Loading_FillRect( cx, y + cellH - 1, cellW, 1, colBorder[col] );
			Loading_FillRect( cx, y, 1, cellH, colBorder[col] );
			Loading_FillRect( cx + cellW - 1, y, 1, cellH, colBorder[col] );

			// Value: centered horizontally, padded from top
			Loading_DrawStringFaded( (int)( cx + cellW * 0.5f - valLen * LOADING_FONT_TITLE * 0.375f ),
							   (int)( y + pad ), LOADING_FONT_TITLE * 0.75f,
							   values[col], nameWhite );

			// Label: centered horizontally, padded from bottom
			Loading_DrawStringFaded( (int)( cx + cellW * 0.5f - lblLen * LOADING_FONT_LABEL * 0.5f ),
							   (int)( y + cellH - pad - LOADING_FONT_LABEL ), LOADING_FONT_LABEL,
							   labels[col], labelMuted );
		}
	}
}

/*
================
Loading_DrawStreamingRow

Draws a single asset row: label left, percentage right,
and a 2px progress bar 14px below the label baseline.
================
*/
static void Loading_DrawStreamingRow( float x, float y, float width,
									  const char *label, float pct ) {
	float vpH = (float)cls.glconfig.vidHeight;
	vec4_t labelColor = { 0.29f, 0.42f, 0.53f, 1.00f };  // #4a6a88
	vec4_t pctColor   = { 0.00f, 0.71f, 0.85f, 1.00f };  // #00b4d8
	vec4_t barBg      = { 1.00f, 1.00f, 1.00f, 0.05f };   // rgba(1,1,1,0.05)
	vec4_t barFg      = { 0.00f, 0.706f, 0.847f, 0.90f };  // #00b4d8 @ 90%
	char pctStr[8];
	int pctLen;
	float barY;
	float fgWidth;

	// Reset any leaked color state from prior draw calls
	Loading_SetColor( NULL );

	// Label on left (small font)
	Loading_DrawStringFaded( (int)x, (int)y, LOADING_FONT_LABEL, label, labelColor );

	// Percentage on right in cyan
	Com_sprintf( pctStr, sizeof(pctStr), "%d%%", (int)( pct * 100.0f ) );
	pctLen = (int)strlen( pctStr );
	Loading_DrawStringFaded( (int)( x + width - pctLen * LOADING_FONT_LABEL ), (int)y, LOADING_FONT_LABEL,
					   pctStr, pctColor );

	// 2px progress bar below label baseline
	barY = y + vpH * 0.029f;  // was 14/480
	Loading_FillRect( x, barY, width, 2, barBg );
	fgWidth = pct * width;
	if ( fgWidth > 0.5f ) {
		Loading_FillRect( x, barY, fgWidth, 2, barFg );
	}
}

/*
================
Loading_DrawStreamingRows

Four asset label rows in the right panel, starting at y=210.
  geometry, tex · shaders, audio, download · pak files
Spaced 22px apart with lerp interpolation.
================
*/
static void Loading_DrawStreamingRows( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	const float rx = vpW * 0.542f;     // was 347/640
	const float rw = vpW * 0.436f;     // was 279/640
	float y = vpH * 0.4375f;           // was 210/480
	float rowSpacing = vpH * 0.0458f;  // was 22/480

#if LOADING_DIAG
	if ( s_diagFrames < 3 )
		Com_Printf( "DIAG Streaming: geo=%.3f shd=%.3f aud=%.3f dl=%.3f overall=%.3f phase=\"%s\"\n",
			cl_loadProgress.geometry, cl_loadProgress.shaders,
			cl_loadProgress.audio, cl_loadProgress.download,
			cl_loadProgress.overall,
			cl_loadProgress.phase ? cl_loadProgress.phase : "(null)" );
#endif
	// Lerp displayed values toward actual
	s_dispGeometry = Loading_LerpValue( s_dispGeometry, cl_loadProgress.geometry );
	s_dispShaders  = Loading_LerpValue( s_dispShaders,  cl_loadProgress.shaders );
	s_dispAudio    = Loading_LerpValue( s_dispAudio,    cl_loadProgress.audio );
	s_dispDownload = Loading_LerpValue( s_dispDownload, cl_loadProgress.download );
	s_dispOverall  = Loading_LerpValue( s_dispOverall,  cl_loadProgress.overall );

	Loading_DrawStreamingRow( rx, y, rw, "geometry", s_dispGeometry );
	y += rowSpacing;
	Loading_DrawStreamingRow( rx, y, rw, "tex | shaders", s_dispShaders );
	y += rowSpacing;
	Loading_DrawStreamingRow( rx, y, rw, "audio", s_dispAudio );
	y += rowSpacing;
	Loading_DrawStreamingRow( rx, y, rw, "download | pak files", s_dispDownload );
}

/*
================
Loading_DrawOverallBar

Phase label in the right panel bottom area (y~370),
followed by a 3px overall progress bar.
================
*/
static void Loading_DrawOverallBar( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	const float rx = vpW * 0.542f;     // was 347/640
	const float rw = vpW * 0.436f;     // was 279/640
	const float by = vpH * 0.7708f;    // was 370/480
#if LOADING_DIAG
	if ( s_diagFrames < 3 )
		Com_Printf( "DIAG OverallBar: overall=%.3f phase=\"%s\" rx=%.0f by=%.0f\n",
			cl_loadProgress.overall,
			cl_loadProgress.phase ? cl_loadProgress.phase : "(null)",
			rx, by );
#endif
	vec4_t phaseColor = { 0.16f, 0.29f, 0.41f, 1.00f };  // #2a4a68
	vec4_t barBg      = { 1.00f, 1.00f, 1.00f, 0.04f };   // rgba(1,1,1,0.04)
	vec4_t barFg      = { 0.00f, 0.71f, 0.85f, 1.00f };   // #00b4d8
	float barY;
	float fgWidth;

	// Phase label
	if ( cl_loadProgress.phase && cl_loadProgress.phase[0] ) {
		Loading_DrawStringFaded( (int)rx, (int)by, LOADING_FONT_LABEL,
						   cl_loadProgress.phase, phaseColor );
	}

	// Lerp overall toward actual
	s_dispOverall = Loading_LerpValue( s_dispOverall, cl_loadProgress.overall );

	// 3px overall progress bar below the phase label
	barY = by + vpH * 0.029f;
	Loading_FillRect( rx, barY, rw, 3, barBg );
	fgWidth = s_dispOverall * rw;
	if ( fgWidth > 0.5f ) {
		Loading_FillRect( rx, barY, fgWidth, 3, barFg );
	}

	// Keep pulse timer alive using real clock (cls.frametime frozen during loading)
	s_pulsePhase = Sys_Milliseconds() % 2000;
}

/*
================
Loading_DrawVulkanBadge

Small cyan dot + Vulkan version string at bottom of right panel (~y=400).
No background badge — just the dot and text.
================
*/
static void Loading_DrawVulkanBadge( void ) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	const float rx = vpW * 0.542f;     // was 347/640
	const float by = vpH * 0.833f;     // was 400/480
	float dotSize = vpH * 0.0125f;     // was 6/480
#if LOADING_DIAG
	if ( s_diagFrames < 3 )
		Com_Printf( "DIAG VulkanBadge: version=\"%s\" rx=%.0f by=%.0f\n",
			cls.glconfig.version_string[0] ? cls.glconfig.version_string : "(empty)",
			rx, by );
#endif
	vec4_t dotCyan = { 0.00f, 0.71f, 0.85f, 1.00f };  // #00b4d8
	vec4_t textColor = { 0.23f, 0.35f, 0.47f, 1.00f };  // #3a5a78
	const char *verStr;
	char buf[64];

	// Small cyan dot (circle simulated as filled rect)
	Loading_FillRect( rx, by + 1, dotSize, dotSize, dotCyan );

	// Build version string: "Vulkan X.Y.Z | GPU-driven particles"
	verStr = cls.glconfig.version_string;
	if ( verStr && verStr[0] ) {
		Com_sprintf( buf, sizeof(buf), "%s | GPU-driven particles", verStr );
	} else {
		Q_strncpyz( buf, "Vulkan | GPU-driven particles", sizeof(buf) );
	}
	Loading_DrawStringFaded( (int)(rx + dotSize * 1.5f), (int)by, LOADING_FONT_LABEL,
					   buf, textColor );
}

// -----------------------------------------------------------------------
// Main entry point
// -----------------------------------------------------------------------

/*
================
CL_LoadingScreenFinished

Called when the loading screen is no longer being drawn (transition to
active gameplay). Restores console notify time if it was suppressed.
================
*/
void CL_LoadingScreenFinished( void ) {
	if ( s_savedNotifyTime >= 0 ) {
		Cvar_Set( "con_notifytime", va( "%d", s_savedNotifyTime ) );
		s_savedNotifyTime = -1;
	}
	// Clear the loading flag so the dark-background guard in
	// SCR_DrawScreenField stops suppressing the UI.
	cl_loadProgress.startTime = 0;
	// Reset shader handle — Hunk_Clear between maps invalidates all
	// registered shaders.
	s_glowShader = 0;
	s_glowDiagCount = 0;
	s_wireframeDiagPrinted = qfalse;
	s_diagFrames = 0;
	// Reset fade transition state
	cl_loadFadeAlpha = 1.0f;
	cl_loadFading = qfalse;
}

/*
================
CL_DrawLoadingScreen

Main loading screen renderer, called from SCR_DrawScreenField during
CA_LOADING state. Renders in real screen pixel coordinates (viewport-relative).
================
*/
void CL_DrawLoadingScreen( void ) {
	// Suppress console notify text during loading
	if ( s_savedNotifyTime == -1 ) {
		s_savedNotifyTime = Cvar_VariableIntegerValue( "con_notifytime" );
		Cvar_Set( "con_notifytime", "0" );
	}

	// Background with theme colors, grid overlay
	Loading_DrawBackground();

	// Top bar with game info
	Loading_DrawTopBar();

	// Vertical divider between panels
	Loading_DrawDivider();

	// Left panel: BSP wireframe with rotation and float animation
	Loading_DrawWireframe();

	// Right panel: map info, stats grid
	Loading_DrawMapInfo();

	// Right panel: streaming progress rows
	Loading_DrawStreamingRows();

	// Right panel: overall bar with phase label and pulse dot
	Loading_DrawOverallBar();

#if LOADING_DIAG
	s_diagFrames++;
#endif
}
