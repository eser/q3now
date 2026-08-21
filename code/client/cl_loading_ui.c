// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_loading_ui.c -- main loading screen renderer
//
// Layout (viewport-relative):
//   Top bar (5.8%):       <game brand> · gametype · scorelimit · timelimit
//   Left panel (52%):     BSP wireframe preview with entity markers
//   Right panel (48%):    Map info, streaming rows, overall bar, Vulkan badge

#include "client.h"
#include "wired/ui/cl_wired_text.h"
#include "wired/ui/cl_wired_background.h"
#include "wired/ui/cl_wired_customdraw.h"   /* unified registry */
#include "wired/ui/cl_wired_compositor.h"   /* GetRootScale + GetDpiScale (rem) */
#include "wired/store/cl_wired_store.h"     /* loading.overall publisher */
LOG_DECLARE_CHANNEL( ch_client, "client" );

/* Type scale, in rem against the UI root (WiredUI_GetRootScale × dpiScale).
 *
 * These used to be a fraction of the render target (vidHeight × k), which put
 * the loading screen on a different sizing model from everything else: .wui text
 * is authored size × dpiScale, and the console is cls.con_factor. All three
 * agree only while the render target tracks the window, and r_renderScale is
 * exactly where it does not — measured at window 1440x900 / target 720x450, the
 * console ran 3.2× the height of a label in the same frame.
 *
 * Rebasing them onto vidHeightLogical × dpiScale was tried first and is recorded
 * here so it is not tried again: dpiScale IS vidHeight/vidHeightLogical, so the
 * substitution cancels back to the same number. Verified numerically. Both sides
 * were pixel ratios; only introducing a root changes the answer.
 *
 * The multiples below reproduce the old pixel sizes at the default root of 14
 * and a 900pt-tall window, within 0.2px — TITLE 25.20 → 25.20, LABEL 11.70 →
 * 11.90, SMALL 9.90 → 9.80. So this migration does not redesign the screen; it
 * changes what the numbers are relative to.
 *
 * They are also the screen's unit of measure, not only its font size: line
 * advance (× 1.6), wrap width (rw / SMALL) and text-extent estimates are all
 * expressed in them. That is what rem is for, and it is why the macros stay
 * rather than being inlined at the 36 call sites. See TASK-212.
 */
#define LOADING_REM          ( WiredUI_GetRootScale() * WiredUI_GetDpiScale() )
#define LOADING_FONT_TITLE   ( 1.80f * LOADING_REM )
#define LOADING_FONT_LABEL   ( 0.85f * LOADING_REM )
#define LOADING_FONT_SMALL   ( 0.70f * LOADING_REM )

// Set to 1 to enable per-function diagnostic prints during loading.
// Remove or set to 0 once debugging is complete.
#define LOADING_DIAG 0

#define LOADING_WIREFRAME_MAX_DRAW_EDGES    4096
#define LOADING_WIREFRAME_MAX_DRAW_MARKERS   128

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

void CL_ResetLoadingScreenState( void ) {
	s_dispGeometry = 0.0f;
	s_dispShaders = 0.0f;
	s_dispAudio = 0.0f;
	s_dispDownload = 0.0f;
	s_dispOverall = 0.0f;
	s_pulsePhase = 0;
	s_glowShader = 0;
	s_glowDiagCount = 0;
	s_wireframeDiagPrinted = qfalse;
	s_diagFrames = 0;
	cl_loadFadeAlpha = 1.0f;
	cl_loadFading = qfalse;
}

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

Fade-aware filled rect. Multiplies alpha by fade factor.
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
	int flags = TEXT_FORCECOLOR;
	if ( !cl_loadFading && cl_loadFadeAlpha >= 1.0f ) {
		flags |= TEXT_DROPSHADOW;
	}

	Text_Draw( text, x, y, FONT_UI, charSize, color, TEXT_ALIGN_LEFT, flags );
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
(gfx/ui/glow_radial — white center, transparent edge).
Color-modulated via re.SetColor, drawn with re.DrawStretchPic.
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
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "GLOW virtual: x=%.0f y=%.0f w=%.0f h=%.0f (cx=%.0f cy=%.0f r=%.0f) rgba=(%.3f %.3f %.3f %.2f)\n",
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

/*
================
Loading_DrawTopBar

Semi-transparent dark bar at top with game info.
y=0, h=28.
Left: game brand (cl_gamename, "Wired" fallback) in accent color.
Middle: gametype · scorelimit · timelimit.
Right: bot difficulty dots if g_autoBots is active.
================
*/
static void Loading_DrawTopBar( float rx, float ry, float rw, float rh ) {
	/* Positions come from the passed rect (legacy fractions were
	 * `0, 0, vpW, vpH*0.058` — call sites pass these explicitly now).
	 * vpW/vpH stay for the vp-relative LAYOUT ratios — padding, marker radii,
	 * panel offsets — which are genuinely a share of the viewport. Type is no
	 * longer among them: LOADING_FONT_* is rem against the UI root now, so it
	 * tracks the same scale as the rest of the UI rather than the render
	 * target. See the macro note at the top of this file. */
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	float pad  = vpW * 0.0125f;          // vp-relative pad (~8/640)
	float fontSize = LOADING_FONT_LABEL;
	vec4_t barColor = { 0.05f, 0.05f, 0.08f, 0.8f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 0.8f };
	vec4_t muted = { 1.0f, 1.0f, 1.0f, 0.5f };
	char info[256];
	const char *serverInfo = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[CS_SERVERINFO];
	(void) vpH;                          // reserved for vp-relative absolute layout sizing

	// Dark semi-transparent bar (covers the supplied rect)
	Loading_FillRect( rx, ry, rw, rh, barColor );

	// Game name in accent color, left side. Sourced from cl_gamename, populated
	// by cgame at init from bg_public.h's GAMENAME_FOR_MASTER. Falls back to
	// "Wired" if cgame hasn't initialized yet (engine self-identifies).
	{
		const char *brand = Cvar_VariableString( "cl_gamename" );
		if ( !*brand ) brand = "Wired";
		Loading_DrawStringFaded( (int)( rx + pad ), (int)( ry + pad ), fontSize, brand,
						   cl_loadingTheme.accentColor );
	}

	// Separator dot and gametype + limits
	int gt = atoi( Info_ValueForKey( serverInfo, "g_gametype" ) );
	int scorelimit = atoi( Info_ValueForKey( serverInfo, "g_scorelimit" ) );
	int timelimit = atoi( Info_ValueForKey( serverInfo, "g_timelimit" ) );
	const char *gtName;

	if ( gt >= 0 && gt < GT_MAX_GAME_TYPE ) {
		gtName = bg_gametypelist[gt].name;
	} else {
		gtName = "DEATHMATCH";
	}

	// Build info string with pipe separators (bitmap font has no middle dot)
	if ( scorelimit > 0 && timelimit > 0 ) {
		Com_sprintf( info, sizeof( info ), "| %s | scorelimit %d | timelimit %d",
					 gtName, scorelimit, timelimit );
	} else if ( scorelimit > 0 ) {
		Com_sprintf( info, sizeof( info ), "| %s | scorelimit %d", gtName, scorelimit );
	} else if ( timelimit > 0 ) {
		Com_sprintf( info, sizeof( info ), "| %s | timelimit %d", gtName, timelimit );
	} else {
		Com_sprintf( info, sizeof( info ), "| %s", gtName );
	}

	// Draw info string after the brand label (assume ~5 chars; longer game
	// names will overlap with the info string until the layout is generalized).
	float px = rx + pad + 5 * fontSize + vpW * 0.006f;  // after brand label + small gap
	Loading_DrawStringFaded( (int)px, (int)( ry + pad ), fontSize, info, muted );

	// // Bot difficulty dots (right side) — if g_autoBots is set
	// {
	// 	int autoBots = Cvar_VariableIntegerValue( "g_autoBots" );
	// 	if ( autoBots > 0 ) {
	// 		int skill = Cvar_VariableIntegerValue( "g_skill" );
	// 		int i;
	// 		float dotSize = vpH * 0.0083f;   // ~4/480
	// 		float dotGap  = vpW * 0.0109f;   // ~7/640
	// 		vec4_t dotOn, dotOff;
	// 		dotOn[0] = cl_loadingTheme.accentColor[0];
	// 		dotOn[1] = cl_loadingTheme.accentColor[1];
	// 		dotOn[2] = cl_loadingTheme.accentColor[2];
	// 		dotOn[3] = 0.9f;
	// 		dotOff[0] = cl_loadingTheme.accentColor[0];
	// 		dotOff[1] = cl_loadingTheme.accentColor[1];
	// 		dotOff[2] = cl_loadingTheme.accentColor[2];
	// 		dotOff[3] = 0.2f;
	// 		for ( i = 0; i < 5; i++ ) {
	// 			float dx = vpW * 0.9375f + i * dotGap;
	// 			Loading_FillRect( dx, vpH * 0.025f, dotSize, dotSize,
	// 						  ( i < skill ) ? dotOn : dotOff );
	// 		}
	// 	}
	// }
}

/*
================
Loading_DrawDivider

1px vertical divider at 52% of viewport width.
Three segments: top (#1e3a4a, 50% alpha), accent center (#00b4d8,
90% alpha), bottom (#1e3a4a, 50% alpha).
================
*/

/*
================
Loading_DrawWireframe

Left panel (52% of viewport width):
If cl_mapPreview.valid, draw all edges using lines in accent color.
Scale edges to fit within a centered area in the panel.
Entity markers: spawn=diamond, item=filled square, flag=outlined square.
Animated: 2D rotation (360deg/12s) and gentle Y float.
Legend at bottom-left.
================
*/
static void Loading_DrawWireframe( float rx, float ry, float rw, float rh ) {
	/* rect = left panel area (legacy `0, vpH*0.075, vpW*0.519,
	 * vpH*0.875`). Helper computes a centered fit area inside the
	 * supplied rect — sub-region math is rect-relative. fitSize stays
	 * vp-relative for consistent absolute size across resolutions
	 * (legacy used vpH*0.583). */
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	const float panelW = rw;
	const float panelH = rh;
	const float panelY = ry;
	// Center a fit area in the panel (~58% of viewport height)
	const float fitSize = vpH * 0.583f;   // vp-relative, ~280/480
	const float fitX = rx + ( panelW - fitSize ) * 0.5f;
	const float fitY = panelY + ( panelH - fitSize ) * 0.5f;
	float scaleX, scaleY, scale, offX, offY;
	float rangeX, rangeY;
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
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "DIAG Wireframe: valid=%d edges=%d markers=%d surfaces=%d "
			"bounds=(%.0f,%.0f)-(%.0f,%.0f)\n",
			cl_mapPreview.valid, cl_mapPreview.numEdges, cl_mapPreview.numMarkers,
			cl_mapPreview.numSurfaces,
			cl_mapPreview.minX, cl_mapPreview.minY,
			cl_mapPreview.maxX, cl_mapPreview.maxY );
#endif
	if ( !cl_mapPreview.valid || cl_mapPreview.numEdges == 0 ) {
		// No preview data — draw placeholder text
		vec4_t muted = { 1.0f, 1.0f, 1.0f, 0.25f };
		Loading_DrawStringFaded( (int)( rx + panelW * 0.5f - vpW * 0.0625f ),
						   (int)( panelY + panelH * 0.5f ),
						   LOADING_FONT_LABEL, "loading...", muted );
		return;
	}

	// Compute scale to fit BSP bounds into the fit area
	rangeX = cl_mapPreview.maxX - cl_mapPreview.minX;
	rangeY = cl_mapPreview.maxY - cl_mapPreview.minY;
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
		float zRange = cl_mapPreview.maxZ - cl_mapPreview.minZ;
		int edgeStep = 1;
		if ( zRange < 1.0f ) zRange = 1.0f;
		if ( cl_mapPreview.numEdges > LOADING_WIREFRAME_MAX_DRAW_EDGES ) {
			edgeStep = ( cl_mapPreview.numEdges + LOADING_WIREFRAME_MAX_DRAW_EDGES - 1 ) /
				LOADING_WIREFRAME_MAX_DRAW_EDGES;
		}

		for ( int i = 0; i < cl_mapPreview.numEdges; i += edgeStep ) {
			const mapPreviewEdge_t *e = &cl_mapPreview.edges[i];
			float ex1 = offX + ( e->x1 - cl_mapPreview.minX ) * scale;
			float ey1 = offY + ( e->y1 - cl_mapPreview.minY ) * scale;
			float ex2 = offX + ( e->x2 - cl_mapPreview.minX ) * scale;
			float ey2 = offY + ( e->y2 - cl_mapPreview.minY ) * scale;
			float rx1 = pivotX + ( ex1 - pivotX ) * cosA - ( ey1 - pivotY ) * sinA;
			float ry1 = pivotY + ( ex1 - pivotX ) * sinA + ( ey1 - pivotY ) * cosA + floatY;
			float rx2 = pivotX + ( ex2 - pivotX ) * cosA - ( ey2 - pivotY ) * sinA;
			float ry2 = pivotY + ( ex2 - pivotX ) * sinA + ( ey2 - pivotY ) * cosA + floatY;
			// Height parameter from average Z of both vertices
			float avgZ = ( e->z1 + e->z2 ) * 0.5f;
			float t = ( avgZ - cl_mapPreview.minZ ) / zRange;
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
	int markerStep = 1;
	float markerR1 = vpH * 0.0083f;  // spawn circle radius (was 4/480)
	float markerR2 = vpH * 0.00625f; // item circle radius (was 3/480)
	float markerS  = vpH * 0.0083f;  // flag half-size (was 4/480)
	if ( cl_mapPreview.numMarkers > LOADING_WIREFRAME_MAX_DRAW_MARKERS ) {
		markerStep = ( cl_mapPreview.numMarkers + LOADING_WIREFRAME_MAX_DRAW_MARKERS - 1 ) /
			LOADING_WIREFRAME_MAX_DRAW_MARKERS;
	}
	for ( int i = 0; i < cl_mapPreview.numMarkers; i += markerStep ) {
		const mapPreviewMarker_t *m = &cl_mapPreview.markers[i];
		float rawX = offX + ( m->x - cl_mapPreview.minX ) * scale;
		float rawY = offY + ( m->y - cl_mapPreview.minY ) * scale;
		float mx = pivotX + ( rawX - pivotX ) * cosA - ( rawY - pivotY ) * sinA;
		float my = pivotY + ( rawX - pivotX ) * sinA + ( rawY - pivotY ) * cosA + floatY;

		switch ( m->type ) {
		case 0: { // spawn — yellow circle (12-segment approximation)
			vec4_t spawnColor = { 1.0f, 0.85f, 0.2f, 0.9f };
			Loading_SetColor( spawnColor );
			for ( int seg = 0; seg < 12; seg++ ) {
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
			Loading_SetColor( itemColor );
			for ( int seg = 0; seg < 12; seg++ ) {
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
		float lx = rx + vpW * 0.019f;   /* anchor at panel's rx */

		// Spawn: yellow circle
		Loading_SetColor( spawnYellow );
		for ( int seg = 0; seg < 12; seg++ ) {
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
			/* 3 concentric rings. An int counter (not a float decrement)
			 * makes the iteration count exact and impossible to hang: with a
			 * float loop, a zero legendR2 gives ringStep 0 and `r -= 0` never
			 * terminates. legendR2 can be 0 for a degenerate rect, so guard by
			 * construction rather than trusting the radius. */
			float ringStep = legendR2 / 3.0f;
			for ( int ring = 0; ring < 3; ring++ ) {
				float r = legendR2 - (float) ring * ringStep;
				for ( int seg = 0; seg < 12; seg++ ) {
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
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "WIREFRAME: %d surfaces, %d segments, %d markers\n",
			cl_mapPreview.numSurfaces, cl_mapPreview.numEdges,
			cl_mapPreview.numMarkers );
		s_wireframeDiagPrinted = qtrue;
	}
}

/* forward declaration for the stats-grid helper extracted
 * out of Loading_DrawMapInfo. Definition follows Loading_DrawMapInfo. */
void Loading_DrawMapInfoStatsGrid( float rx, float y, float rw, float cellH );

/*
================
Loading_DrawMapInfo

Right panel (x=347..626, y=40..470):
Archetype tag, large map name, author+year, flavor quote,
3-cell stats grid.
================
*/
static void Loading_DrawMapInfo( float rx, float ry, float rw, float rh ) {
	/* rect = right panel block area (legacy `vpW*0.542,
	 * vpH*0.1, vpW*0.436, ~vpH*0.25`). Helper stacks title/author/
	 * flavor-quote downward starting at ry. Sub-region math
	 * (font tiers, padding, line spacing) stays vp-relative for
	 * resolution-consistent absolute sizing. */
	float vpH = (float)cls.glconfig.vidHeight;
	float y = ry;
	float pad = vpH * 0.0167f;         // general padding (~8/480) — vp-relative
	(void) rh;                         // helper draws what fits; rect height is a max-extent hint
	const char *info = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[CS_SERVERINFO];
#if LOADING_DIAG
	if ( s_diagFrames < 3 ) {
		const char *mapCvar = Info_ValueForKey( info, "mapname" );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "DIAG MapInfo: mapname=\"%s\" longName=\"%s\" author=\"%s\" "
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
	// --- Archetype tag (small label, cyan) ---
	// Format: "GAMETYPE | ARCHETYPE"
	int gt = atoi( Info_ValueForKey( info, "g_gametype" ) );
	const char *gtName;
	if ( gt >= 0 && gt < GT_MAX_GAME_TYPE ) {
		gtName = bg_gametypelist[gt].name;
	} else {
		gtName = "DEATHMATCH";
	}
	const char *archName = cl_mapInfo.archetype[0] ? cl_mapInfo.archetype : "TECH";
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
	y = ry + vpH * 0.029f;  // was `vpH*0.129` absolute; ry+0.029 preserves byte-identical at legacy ry=vpH*0.1
	{
		char mapUp[128];
		char shortUp[64];
		const char *mapCvar = Info_ValueForKey( info, "mapname" );
		const char *shortName = ( mapCvar && mapCvar[0] ) ? mapCvar : cl_mapInfo.mapName;

		if ( shortName[0] && cl_mapInfo.longName[0] ) {
			Q_strncpyz( shortUp, shortName, sizeof(shortUp) );
			Q_strupr( shortUp );
			Com_sprintf( mapUp, sizeof(mapUp), "%s - %s", shortUp, cl_mapInfo.longName );
		} else if ( shortName[0] ) {
			Q_strncpyz( mapUp, shortName, sizeof(mapUp) );
			Q_strupr( mapUp );
		// } else if ( cl_mapInfo.longName[0] ) {
		// 	Q_strncpyz( mapUp, cl_mapInfo.longName, sizeof(mapUp) );
		} else {
			Q_strncpyz( mapUp, "LOADING", sizeof(mapUp) );
		}
		int nameLen = (int)strlen( mapUp );

		// Progressive scaling to fit panel width
		float charSize;
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
	y = ry + vpH * 0.1167f;  // was `vpH*0.2167` absolute; ry+0.1167 preserves byte-identical at legacy ry=vpH*0.1
	if ( cl_mapInfo.quote[0] ) {
		char line1[128], line2[128];
		float quoteIndent = pad;
		int textX = (int)( rx + quoteIndent );
		float charW = LOADING_FONT_SMALL;  // approximate char width at small scale
		int maxChars = (int)( ( rw - quoteIndent - 2 ) / charW );
		int len = (int)strlen( cl_mapInfo.quote );

		// Clamp the wrap column to the line buffer: maxChars is derived from
		// panel width / font size and can exceed sizeof(line1)-1 on wide aspect
		// ratios, which would let Q_strncpyz(line1, quote, splitAt+1) write past
		// line1[128] (splitAt is bounded only by maxChars below).
		if ( maxChars > (int)sizeof( line1 ) - 1 ) {
			maxChars = (int)sizeof( line1 ) - 1;
		}
		if ( maxChars < 1 ) {
			maxChars = 1;
		}

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
	// extracted into Loading_DrawMapInfoStatsGrid so the
	// custom:loading_mapinfo_stats compositor entry can wrap the same
	// rendering. Inline call preserved here so the legacy SCR path
	// continues drawing identical output. This whole file
	// retires once the legacy path is removed.
	y += vpH * 0.0125f;
	if ( y < ry + vpH * 0.1958f ) y = ry + vpH * 0.1958f;  // was `vpH*0.2958` absolute
	Loading_DrawMapInfoStatsGrid( rx, y, rw, vpH * 0.1f );
}

/*
================
Loading_DrawMapInfoStatsGrid

Extracted from Loading_DrawMapInfo. Renders the 3-cell
stats grid (PLAYERS / WEAPON / ITEMS) inside the given rect with per-
column accent colours. Pure draw — reads cl_mapInfo + clientActiveApp->cl.gameState
state internally; no per-frame parameters beyond the rect.

Wrapped by the custom:loading_mapinfo_stats unified-registry entry.
*/
void Loading_DrawMapInfoStatsGrid( float rx, float y, float rw, float cellH )
{
	float vpH    = (float) cls.glconfig.vidHeight;
	float pad    = vpH * 0.0167f;
	float cellW  = rw / 3.0f;

	vec4_t nameWhite   = { 0.91f, 0.96f, 0.99f, 1.00f };
	vec4_t labelMuted  = { 0.29f, 0.42f, 0.53f, 1.00f };
	vec4_t colBorder[3] = {
		{ 0.00f, 0.706f, 0.847f, 0.15f },  // cyan — PLAYERS
		{ 0.90f, 0.22f, 0.27f, 0.15f },    // red — WEAPON
		{ 1.00f, 0.84f, 0.04f, 0.15f }     // yellow — ITEMS
	};
	vec4_t colBg[3] = {
		{ 0.00f, 0.706f, 0.847f, 0.06f },
		{ 0.90f, 0.22f, 0.27f, 0.06f },
		{ 1.00f, 0.84f, 0.04f, 0.06f }
	};

	const char *info     = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[ CS_SERVERINFO ];
	const char *labels[3] = { "PLAYERS", "WEAPON", "ITEMS" };
	const char *values[3];
	char        statBufs[3][32];

	(void) info;  /* reserved for future stats — unused after extraction */

	if ( cl_mapInfo.playersMin > 0 || cl_mapInfo.playersMax > 0 ) {
		Com_sprintf( statBufs[0], sizeof( statBufs[0] ), "%d-%d",
		             cl_mapInfo.playersMin, cl_mapInfo.playersMax );
	} else {
		Q_strncpyz( statBufs[0], "--", sizeof( statBufs[0] ) );
	}
	values[0] = statBufs[0];

	if ( cl_mapInfo.metaWeapon[0] ) {
		Q_strncpyz( statBufs[1], cl_mapInfo.metaWeapon, sizeof( statBufs[1] ) );
	} else {
		Q_strncpyz( statBufs[1], "--", sizeof( statBufs[1] ) );
	}
	values[1] = statBufs[1];

	if ( cl_mapInfo.itemNodes > 0 ) {
		Com_sprintf( statBufs[2], sizeof( statBufs[2] ), "%d", cl_mapInfo.itemNodes );
	} else {
		Q_strncpyz( statBufs[2], "--", sizeof( statBufs[2] ) );
	}
	values[2] = statBufs[2];

	for ( int col = 0; col < 3; col++ ) {
		float cx     = rx + col * cellW;
		int   valLen = (int) strlen( values[ col ] );
		int   lblLen = (int) strlen( labels[ col ] );

		Loading_FillRect( cx, y, cellW, cellH, colBg[ col ] );
		Loading_FillRect( cx, y, cellW, 1, colBorder[ col ] );
		Loading_FillRect( cx, y + cellH - 1, cellW, 1, colBorder[ col ] );
		Loading_FillRect( cx, y, 1, cellH, colBorder[ col ] );
		Loading_FillRect( cx + cellW - 1, y, 1, cellH, colBorder[ col ] );

		Loading_DrawStringFaded( (int)( cx + cellW * 0.5f - valLen * LOADING_FONT_TITLE * 0.375f ),
		                         (int)( y + pad ),
		                         LOADING_FONT_TITLE * 0.75f,
		                         values[ col ], nameWhite );

		Loading_DrawStringFaded( (int)( cx + cellW * 0.5f - lblLen * LOADING_FONT_LABEL * 0.5f ),
		                         (int)( y + cellH - pad - LOADING_FONT_LABEL ),
		                         LOADING_FONT_LABEL,
		                         labels[ col ], labelMuted );
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

	// Reset any leaked color state from prior draw calls
	Loading_SetColor( NULL );

	// Label on left (small font)
	Loading_DrawStringFaded( (int)x, (int)y, LOADING_FONT_LABEL, label, labelColor );

	// Percentage on right in cyan
	Com_sprintf( pctStr, sizeof(pctStr), "%d%%", (int)( pct * 100.0f ) );
	int pctLen = (int)strlen( pctStr );
	Loading_DrawStringFaded( (int)( x + width - pctLen * LOADING_FONT_LABEL ), (int)y, LOADING_FONT_LABEL,
					   pctStr, pctColor );

	// 2px progress bar below label baseline
	float barY = y + vpH * 0.029f;  // was 14/480
	Loading_FillRect( x, barY, width, 2, barBg );
	float fgWidth = pct * width;
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
static void Loading_DrawStreamingRows( float rx, float ry, float rw, float rh ) {
	/* rect = right panel area for 4 progress rows (legacy
	 * `vpW*0.542, vpH*0.4375, vpW*0.436, vpH*0.137`). Row spacing
	 * stays vp-relative so line-height consistency matches legacy
	 * across resolutions. */
	float vpH = (float)cls.glconfig.vidHeight;
	float y = ry;
	float rowSpacing = vpH * 0.0458f;  // ~22/480 — vp-relative
	(void) rh;                         // helper draws 4 rows with vp-relative spacing

#if LOADING_DIAG
	if ( s_diagFrames < 3 )
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "DIAG Streaming: geo=%.3f shd=%.3f aud=%.3f dl=%.3f overall=%.3f phase=\"%s\"\n",
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
================
Loading_DrawVulkanBadge

Small cyan dot + Vulkan version string at bottom of right panel (~y=400).
No background badge — just the dot and text.
================
*/
static void Loading_DrawVulkanBadge( float rx, float ry, float rw, float rh ) {
	/* rx/ry are the badge anchor (legacy `vpW*0.542, vpH*0.833`).
	 * dotSize stays vp-relative for resolution-consistent legibility. */
	float vpH = (float)cls.glconfig.vidHeight;
	const float by = ry;
	float dotSize = vpH * 0.0125f;     // ~6/480 — vp-relative for legibility
	(void) rw; (void) rh;              // helper draws fixed-size content within
#if LOADING_DIAG
	if ( s_diagFrames < 3 )
		Com_Log( SEV_INFO, LOG_CH(ch_client), "DIAG VulkanBadge: version=\"%s\" rx=%.0f by=%.0f\n",
			cls.glconfig.version_string[0] ? cls.glconfig.version_string : "(empty)",
			rx, by );
#endif
	vec4_t dotCyan = { 0.00f, 0.71f, 0.85f, 1.00f };  // #00b4d8
	vec4_t textColor = { 0.23f, 0.35f, 0.47f, 1.00f };  // #3a5a78

	// Small cyan dot (circle simulated as filled rect)
	Loading_FillRect( rx, by + 1, dotSize, dotSize, dotCyan );

	// Build version string: "Vulkan X.Y.Z | GPU-driven particles"
	const char *verStr = cls.glconfig.version_string;
	char buf[64];
	if ( verStr && verStr[0] ) {
		Com_sprintf( buf, sizeof(buf), "%s | GPU-driven particles", verStr );
	} else {
		Q_strncpyz( buf, "Vulkan | GPU-driven particles", sizeof(buf) );
	}
	Loading_DrawStringFaded( (int)(rx + dotSize * 1.5f), (int)by, LOADING_FONT_LABEL,
					   buf, textColor );
}

/*
================
Loading_DrawServerInfoStrip

Remote-only: hostname, PURE badge, MOTD.
Skipped for localhost/LAN/listen servers.
Placed in the right panel between streaming rows and the overall bar.
================
*/
static void Loading_DrawServerInfoStrip( float rx, float ry, float rw, float rh ) {
	/* rect carries the right-panel mid-section (legacy
	 * `vpW*0.542, vpH*0.63, vpW*0.436, dynamic`). Helper still uses
	 * vp-relative font sizes (legacy line-spacing math against the
	 * font macros, which depend on cls.glconfig). */
	float y = ry;
	(void) rh;                         // helper stacks until conditional content ends
	vec4_t hostColor = { 0.91f, 0.96f, 0.99f, 0.70f };   // near-white @ 70%
	vec4_t pureColor = { 0.00f, 0.71f, 0.85f, 0.80f };   // cyan accent
	vec4_t motdColor = { 0.29f, 0.42f, 0.53f, 0.80f };   // muted blue

	if ( !Q_stricmp( clientActiveApp->servername, "localhost" ) ) {
		return;
	}

	const char *info    = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[CS_SERVERINFO];
	const char *sysInfo = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[CS_SYSTEMINFO];
	const char *motd    = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[CS_MOTD];

	const char *hostname = Info_ValueForKey( info, "sv_hostname" );
	if ( hostname && hostname[0] ) {
		char cleanHost[256];
		Q_strncpyz( cleanHost, hostname, sizeof( cleanHost ) );
		Q_CleanStr( cleanHost );
		Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_LABEL, cleanHost, hostColor );
		y += LOADING_FONT_LABEL * 1.6f;
	}

	const char *pureFl = Info_ValueForKey( sysInfo, "sv_pure" );
	if ( pureFl && pureFl[0] == '1' ) {
		Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_SMALL, "PURE SERVER", pureColor );
		y += LOADING_FONT_SMALL * 1.6f;
	}

	if ( motd && motd[0] ) {
		char line1[256], line2[256];
		int maxChars = (int)( rw / LOADING_FONT_SMALL );
		int len = (int)strlen( motd );

		// Clamp the wrap column to the line buffers: maxChars is derived from
		// panel width / font size and is NOT otherwise bounded, so on extreme
		// aspect ratios it can exceed sizeof(line1)-1 and let the line1 copy
		// and the line2[maxChars] truncation writes overflow the 256-byte
		// stack buffers on untrusted (server CS_MOTD) data. Cap at 252 so the
		// `maxChars` index and the `maxChars-3..maxChars` ellipsis writes all
		// stay in bounds.
		if ( maxChars > (int)sizeof( line1 ) - 4 ) {
			maxChars = (int)sizeof( line1 ) - 4;
		}
		if ( maxChars < 1 ) {
			maxChars = 1;
		}

		if ( len > maxChars ) {
			int splitAt = maxChars;
			while ( splitAt > 0 && motd[splitAt] != ' ' ) {
				splitAt--;
			}
			if ( splitAt == 0 ) {
				splitAt = maxChars;
			}
			Q_strncpyz( line1, motd, splitAt + 1 );
			Q_strncpyz( line2, motd + splitAt + 1, sizeof( line2 ) );
			if ( (int)strlen( line2 ) > maxChars ) {
				line2[maxChars - 3] = '.';
				line2[maxChars - 2] = '.';
				line2[maxChars - 1] = '.';
				line2[maxChars] = '\0';
			}
			Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_SMALL, line1, motdColor );
			y += LOADING_FONT_SMALL * 1.5f;
			Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_SMALL, line2, motdColor );
		} else {
			Loading_DrawStringFaded( (int)rx, (int)y, LOADING_FONT_SMALL, motd, motdColor );
		}
	}
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
	CL_ResetLoadingScreenState();
}

/*
================
CL_PublishLoadingState

Extracted per-frame publisher (was inline at the top
of CL_DrawLoadingScreen). Fires from CL_Frame BEFORE SCR_UpdateScreen
runs the compositor emit walk, so storeBind / bindwidth reads see
current-frame values (eliminates the one-frame lag the render-time
publisher had).

Gated on clientActiveApp->state range matching the legacy CL_DrawLoadingScreen
invocation conditions (CA_CONNECTING through CA_PRIMED): outside that
range the function no-ops, preserving legacy behavior of "store keys
untouched when not loading." Eventually CL_DrawLoadingScreen
retires, publisher stays.
================
*/
void CL_PublishLoadingState( void ) {
	wuiStoreEntry_t *e;

	/* (2026-06-10) the only caller of CL_LoadingScreenFinished was lost
	 * when the legacy load screen retired, so cl_loadProgress.startTime never
	 * reset to 0 once gameplay began — which permanently suppressed the
	 * console notify overlay (Con_DrawNotify bails while startTime > 0). Fire
	 * the finish hook once the load completes (the loading bar is still
	 * flagged up but the client has reached CA_ACTIVE). Idempotent: the hook
	 * zeroes startTime, so the guard does not re-trigger on later frames. This
	 * runs every frame from CL_Frame (before the loading-range gate below),
	 * which is the closest always-on hook this file owns to the CA_ACTIVE
	 * transition. */
	if ( cl_loadProgress.startTime > 0 && clientActiveApp->state == CA_ACTIVE ) {
		CL_LoadingScreenFinished();
	}

	if ( clientActiveApp->state < CA_CONNECTING || clientActiveApp->state > CA_PRIMED ) {
		return;
	}

	e = WiredStore_Set( "loading.overall" );
	if ( e ) e->value = cl_loadProgress.overall;
	e = WiredStore_Set( "loading.geometry" );
	if ( e ) e->value = cl_loadProgress.geometry;
	e = WiredStore_Set( "loading.shaders" );
	if ( e ) e->value = cl_loadProgress.shaders;
	e = WiredStore_Set( "loading.audio" );
	if ( e ) e->value = cl_loadProgress.audio;
	e = WiredStore_Set( "loading.download" );
	if ( e ) e->value = cl_loadProgress.download;
	e = WiredStore_Set( "loading.phase" );
	if ( e ) Q_strncpyz( e->text,
		cl_loadProgress.phase ? cl_loadProgress.phase : "",
		sizeof( e->text ) );
}

/* CL_DrawLoadingScreen retired. Compositor is the sole loading-
 * screen renderer via loading_screen.wmenu: 7 custom-draws (wireframe,
 * streaming_rows, mapinfo_stats, topbar, maptitle_block, vulkan_badge,
 * server_info_strip) + 2 bindwidth bar items + 1 storeBind phase text +
 * 1 static footer strip. Per-frame state published from CL_Frame via
 * CL_PublishLoadingState. Helper functions
 * (Loading_DrawTopBar/Wireframe/etc) STAY — sole callers are the
 * Loading_CustomDraw_* wrappers (which parameterized them).
 *
 * Loading_DrawBackground / Loading_DrawDivider retired with this fn —
 * the wmenu's `backcolor 0.04 0.06 0.10 1` + `loading_divider` itemDef
 * cover both. Loading_DrawOverallBar retired separately below — wmenu's
 * bindwidth bar items + storeBind phase text replace it. */

/* ── unified custom-draw wrappers ──────────────────────
 * Stateless adapters wrapping the existing Loading_Draw* helpers so the
 * compositor's CUSTOM dispatch can render the loading screen segments
 * declaratively from loading_screen.wmenu. The legacy CL_DrawLoadingScreen
 * path stays intact (cl_scrn.c CA_LOADING branch); both fire during the
 * transitional double-dispatch cycle. Whole file retires once the legacy path is removed.
 *
 * All seven wrappers now pass (x, y, w, h) through: the .wui item's rect,
 * resolved to pixels by the compositor, drives POSITION. (This paragraph
 * used to say the opposite — written before the helpers were
 * parameterised, and left behind when they were.)
 *
 * SIZE no longer bypasses it the way it did. Type is rem against the UI
 * root (LOADING_FONT_* at the top of this file), the same scale .wui text
 * uses, so it no longer drifts from the authored layout when the render
 * target stops tracking the window — which r_renderScale, or any path that
 * sets vidHeight independently, makes it do.
 *
 * The paddings and marker radii are still vp-relative, and deliberately:
 * they are a share of the viewport, not a share of the type. See TASK-212.
 */

static void Loading_CustomDraw_Wireframe( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	Loading_DrawWireframe( x, y, w, h );
}

static void Loading_CustomDraw_StreamingRows( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	Loading_DrawStreamingRows( x, y, w, h );
}

static void Loading_CustomDraw_MapInfoStats( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	/* Use the wmenu-author-supplied rect directly — the extracted helper
	 * is fully parameterised (unlike the other two wrappers which still
	 * use legacy fixed-layout coords). A later cleanup pass will likely
	 * parameterise Wireframe + StreamingRows similarly. */
	Loading_DrawMapInfoStatsGrid( x, y, w, h );
}

/* parameterized thin wrappers — the wmenu-
 * author-supplied rect (resolved to pixels by the compositor) flows
 * through into the legacy helper. Each helper now accepts (rx, ry,
 * rw, rh) and uses them for positions; vp-relative font/padding stays
 * absolute for resolution-consistent legibility. Eventually helpers +
 * wrappers stay; the legacy CL_DrawLoadingScreen
 * call chain retires. */
static void Loading_CustomDraw_TopBar( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	Loading_DrawTopBar( x, y, w, h );
}

static void Loading_CustomDraw_MapTitleBlock( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	/* Loading_DrawMapInfo emits the cohesive block: map title (progressive
	 * font tiers) + author/year + flavor quote (word-wrapped 1-2 lines
	 * with the 2px left-border accent). Composition + wrap stay in the
	 * legacy helper — see 7.30 dispatch Option C rationale. */
	Loading_DrawMapInfo( x, y, w, h );
}

static void Loading_CustomDraw_VulkanBadge( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	Loading_DrawVulkanBadge( x, y, w, h );
}

static void Loading_CustomDraw_ServerInfoStrip( float x, float y, float w, float h, vec4_t color )
{
	( void ) color;
	/* Skips for localhost; conditional sub-elements (hostname / PURE badge /
	 * MOTD word-wrap) all gated inside the legacy helper. */
	Loading_DrawServerInfoStrip( x, y, w, h );
}

void WiredLoadingCustomDraws_RegisterAll( void )
{
	wuiCustomDrawDef_t def;

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_wireframe", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_Wireframe;
	WiredUI_RegisterCustomDraw( &def );

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_streaming_rows", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_StreamingRows;
	WiredUI_RegisterCustomDraw( &def );

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_mapinfo_stats", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_MapInfoStats;
	WiredUI_RegisterCustomDraw( &def );

	/* 7.30 Option C: 4 additional wrappers for the remaining legacy regions */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_topbar", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_TopBar;
	WiredUI_RegisterCustomDraw( &def );

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_maptitle_block", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_MapTitleBlock;
	WiredUI_RegisterCustomDraw( &def );

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_vulkan_badge", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_VulkanBadge;
	WiredUI_RegisterCustomDraw( &def );

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:loading_server_info_strip", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = Loading_CustomDraw_ServerInfoStrip;
	WiredUI_RegisterCustomDraw( &def );
}
