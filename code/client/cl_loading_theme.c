// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_loading_theme.c -- map archetype detection and loading screen color themes

#include "client.h"

clLoadingTheme_t cl_loadingTheme;

// --------------------------------------------------------------------------
// Color palettes per archetype
//
//   primary = background radial gradient center
//   secondary = background radial gradient edge
//   accent = progress bars, wireframe, text accents
//   grid = accent at 10% opacity
//
// v1→v2 colour migration, 2026-08-17.
//
// What was here: four hardcoded v1 palettes whose accents were cyan #00e5ff
// (TECH), purple #c040ff (GOTHIC), amber #ffb300 (BASE) and steel #607d8b
// (DEFAULT). None of them moved when ui_palette_accent changed, so the loading
// screen's brand label and flag markers stayed v1-cyan on a map the detector
// called TECH — which is the default fallback, i.e. most maps — no matter which
// accent the player had chosen.
//
// What the v2 design says: qw-screens.jsx ScreenLoading draws this screen in
// P.amber (the accent) with P.bone / P.boneDim / P.line for structure. There is
// no per-archetype palette in the v2 artboard at all.
//
// What is preserved and why: the archetype DETECTION (sky/mapname heuristics,
// .meta override) is real per-map identity that the v2 mockup simply never
// modelled — a single static artboard has no maps to distinguish. Flattening all
// four to $accent would silently delete that identity, so instead each archetype
// keeps a DISTINCT accent, expressed as a token-relative role rather than a
// literal:
//
//   TECH    → $accent      — the player's chosen accent, unmodified. TECH is the
//                            detector's fallback, so the common case now simply
//                            follows the theme, which is what the v2 artboard
//                            shows.
//   GOTHIC  → $accentSoft  — the desaturated/darker accent partner. Keeps the
//                            "heavier, older" read of the old purple without
//                            pinning a hue that fights every accent.
//   BASE    → $accentDim   — the dimmest accent partner: industrial, muted.
//   DEFAULT → $boneDim     — deliberately NOT accent-derived. The old steel
//                            #607d8b was a neutral "no identity detected" grey,
//                            and $boneDim is the v2 token for exactly that
//                            neutral-secondary role. It is also the one archetype
//                            that should NOT read as accent: an unrecognised map
//                            has no identity to advertise.
//
// So the four archetypes stay visually distinct AND all four now move with the
// theme (three with the accent overlay, one with the mode overlay).
//
// bgColor / primaryGlow / secondaryGlow are retained for struct compatibility
// but are DEAD as of the SCENE-backdrop change: the only field any consumer
// reads is accentColor (cl_loading_ui.c:271 brand label, :506 flag markers) —
// the loading background is now the parallax DemoBackdrop painted by
// WUI_DrawBackgroundScene, not a radial gradient. They are left at the mode
// tokens' surface values rather than the v1 literals so a future consumer that
// revives them does not resurrect the v1 palette.
// --------------------------------------------------------------------------

/* Live theme token table lookup (defined in cl_wired_parse.c). Guarded: the
 * table is empty before the first ui/_tokens.wui parse, in which case each
 * lookup leaves the caller's baked v2 fallback in place. */
extern const char *WiredToken_Find( const char *name );

/* Resolve a "#rrggbb"/"#rrggbbaa" token into out[0..2], preserving out[3].
 * Mirrors wui_clay_token_rgb in cl_wired_clay.c. */
static void cl_loading_token_rgb( const char *tokenName, vec4_t out ) {
	const char *v = WiredToken_Find( tokenName );
	unsigned    r, g, b;
	if ( v && v[0] == '#' && ( strlen( v ) == 7 || strlen( v ) == 9 )
	     && sscanf( v + 1, "%2x%2x%2x", &r, &g, &b ) == 3 ) {
		out[0] = (float) r / 255.0f;
		out[1] = (float) g / 255.0f;
		out[2] = (float) b / 255.0f;
	}
	/* else: leave the caller's fallback RGB in place */
}

/* Per-archetype accent ROLE. Index matches mapArchetype_t. See the block
 * comment above for why each archetype maps where it does. */
static const char *const archetypeAccentToken[4] = {
	"accent",      /* ARCHETYPE_TECH    */
	"accentSoft",  /* ARCHETYPE_GOTHIC  */
	"accentDim",   /* ARCHETYPE_BASE    */
	"boneDim"      /* ARCHETYPE_DEFAULT */
};

typedef struct {
	vec4_t bgColor;
	vec4_t primaryGlow;
	vec4_t secondaryGlow;
	vec4_t accentColor;   /* fallback only; overwritten from the token table */
} archetypePalette_t;

/* Fallbacks used only when the token table is not yet populated. Baked from the
 * dark-mode + amber-accent boot defaults in modfiles/ui/_tokens.wui, so a
 * pre-token-load draw degrades to the shipped theme rather than to v1 cyan.
 *   $ink #0a0908 · $panel #14110e · $line #2a2520
 *   $accent #f4a03a · $accentSoft #c87a2a · $accentDim #8a6230 · $boneDim #8a7560
 */
// NOLINTBEGIN(misc-redundant-expression) — `0.0f/255.0f` and `255.0f/255.0f` are kept for column alignment with adjacent components in each row
static const archetypePalette_t palettes[4] = {
	// ARCHETYPE_TECH    -- accent $accent      #f4a03a
	{
		{ 10.0f/255.0f, 9.0f/255.0f, 8.0f/255.0f, 1.0f },
		{ 20.0f/255.0f, 17.0f/255.0f, 14.0f/255.0f, 1.0f },
		{ 42.0f/255.0f, 37.0f/255.0f, 32.0f/255.0f, 1.0f },
		{ 244.0f/255.0f, 160.0f/255.0f, 58.0f/255.0f, 1.0f }
	},
	// ARCHETYPE_GOTHIC  -- accent $accentSoft  #c87a2a
	{
		{ 10.0f/255.0f, 9.0f/255.0f, 8.0f/255.0f, 1.0f },
		{ 20.0f/255.0f, 17.0f/255.0f, 14.0f/255.0f, 1.0f },
		{ 42.0f/255.0f, 37.0f/255.0f, 32.0f/255.0f, 1.0f },
		{ 200.0f/255.0f, 122.0f/255.0f, 42.0f/255.0f, 1.0f }
	},
	// ARCHETYPE_BASE    -- accent $accentDim   #8a6230
	{
		{ 10.0f/255.0f, 9.0f/255.0f, 8.0f/255.0f, 1.0f },
		{ 20.0f/255.0f, 17.0f/255.0f, 14.0f/255.0f, 1.0f },
		{ 42.0f/255.0f, 37.0f/255.0f, 32.0f/255.0f, 1.0f },
		{ 138.0f/255.0f, 98.0f/255.0f, 48.0f/255.0f, 1.0f }
	},
	// ARCHETYPE_DEFAULT -- accent $boneDim     #8a7560 (neutral, NOT accent-derived)
	{
		{ 10.0f/255.0f, 9.0f/255.0f, 8.0f/255.0f, 1.0f },
		{ 20.0f/255.0f, 17.0f/255.0f, 14.0f/255.0f, 1.0f },
		{ 42.0f/255.0f, 37.0f/255.0f, 32.0f/255.0f, 1.0f },
		{ 138.0f/255.0f, 117.0f/255.0f, 96.0f/255.0f, 1.0f }
	}
};
// NOLINTEND(misc-redundant-expression)

/*
=================
CL_DetectArchetype

Determine the map archetype from (in priority order):
  1. Explicit .meta archetype field
  2. Sky texture name heuristics
  3. Map name prefix heuristics
  4. Default fallback: ARCHETYPE_TECH (per spec)
=================
*/
mapArchetype_t CL_DetectArchetype( const clMapInfo_t *info ) {

	// --- 1. Explicit archetype from .meta file ---
	if ( info->archetype[0] ) {
		if ( !Q_stricmp( info->archetype, "tech" ) )
			return ARCHETYPE_TECH;
		if ( !Q_stricmp( info->archetype, "gothic" ) )
			return ARCHETYPE_GOTHIC;
		if ( !Q_stricmp( info->archetype, "base" ) )
			return ARCHETYPE_BASE;
		return ARCHETYPE_DEFAULT;
	}

	// --- 2. Sky texture name heuristics ---
	if ( info->sky[0] ) {
		if ( Q_stristr( info->sky, "gothic" ) ||
			 Q_stristr( info->sky, "hell" ) ||
			 Q_stristr( info->sky, "castle" ) ) {
			return ARCHETYPE_GOTHIC;
		}
		if ( Q_stristr( info->sky, "base" ) ||
			 Q_stristr( info->sky, "military" ) ||
			 Q_stristr( info->sky, "lab" ) ) {
			return ARCHETYPE_BASE;
		}
	}

	// --- 3. Map name prefix heuristics ---
	if ( info->mapName[0] ) {
		if ( !Q_stricmpn( info->mapName, "pro-", 4 ) ||
			 !Q_stricmpn( info->mapName, "cpm", 3 ) ||
			 !Q_stricmpn( info->mapName, "vq3", 3 ) ) {
			return ARCHETYPE_TECH;
		}
	}

	// --- 4. Default fallback ---
	return ARCHETYPE_TECH;
}

/*
=================
CL_ApplyLoadingTheme

Detect the archetype for the current map and fill cl_loadingTheme
with the corresponding color palette.  Grid color is derived from
the accent color at 10 % opacity.
=================
*/
void CL_ApplyLoadingTheme( const clMapInfo_t *info ) {
	const archetypePalette_t	*pal;
	mapArchetype_t				arch;

	arch = CL_DetectArchetype( info );
	pal  = &palettes[arch];

	cl_loadingTheme.archetype = arch;

	Vector4Copy( pal->bgColor,       cl_loadingTheme.bgColor );
	Vector4Copy( pal->primaryGlow,   cl_loadingTheme.primaryGlow );
	Vector4Copy( pal->secondaryGlow, cl_loadingTheme.secondaryGlow );

	/* Accent starts from the baked v2 fallback, then follows the LIVE theme
	 * token for this archetype's role — so a ui_palette_accent / ui_palette_mode
	 * swap repaints the loading screen without a recompile. Called on every map
	 * load, which is also every point at which the loading screen is about to be
	 * drawn, so the value is never staler than the current theme. */
	Vector4Copy( pal->accentColor,   cl_loadingTheme.accentColor );
	cl_loading_token_rgb( archetypeAccentToken[arch], cl_loadingTheme.accentColor );


	/* grid = accent color at 10 % opacity. Derived from the RESOLVED accent
	 * above, not from pal->accentColor — the latter is only the pre-token
	 * fallback, so sourcing it here would peg the grid to the baked literal
	 * while the accent itself followed the theme. */
	cl_loadingTheme.gridColor[0] = cl_loadingTheme.accentColor[0];
	cl_loadingTheme.gridColor[1] = cl_loadingTheme.accentColor[1];
	cl_loadingTheme.gridColor[2] = cl_loadingTheme.accentColor[2];
	cl_loadingTheme.gridColor[3] = 0.10f;
}
