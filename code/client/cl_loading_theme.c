/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/
// cl_loading_theme.c -- map archetype detection and loading screen color themes

#include "client.h"

clLoadingTheme_t cl_loadingTheme;

// --------------------------------------------------------------------------
// Color palettes per archetype (from spec)
//
//   primary = background radial gradient center
//   secondary = background radial gradient edge
//   accent = progress bars, wireframe, text accents
//   grid = accent at 10% opacity
// --------------------------------------------------------------------------

typedef struct {
	vec4_t bgColor;
	vec4_t primaryGlow;
	vec4_t secondaryGlow;
	vec4_t accentColor;
} archetypePalette_t;

static const archetypePalette_t palettes[4] = {
	// ARCHETYPE_TECH  -- bg 080c10, primary 0a1a2e, secondary 6e1a1a, accent cyan #00e5ff
	// NOLINTBEGIN(misc-redundant-expression) — `0.0f/255.0f` and `255.0f/255.0f` are kept for column alignment with adjacent components in each row
	{
		{ 8.0f/255.0f, 12.0f/255.0f, 16.0f/255.0f, 1.0f },
		{ 10.0f/255.0f, 26.0f/255.0f, 46.0f/255.0f, 1.0f },
		{ 110.0f/255.0f, 26.0f/255.0f, 26.0f/255.0f, 1.0f },
		{ 0.0f/255.0f, 229.0f/255.0f, 255.0f/255.0f, 1.0f }
	},
	// ARCHETYPE_GOTHIC  -- bg 060410, primary 0e0a1e, secondary 2a1a4e, accent purple #c040ff
	{
		{ 6.0f/255.0f, 4.0f/255.0f, 16.0f/255.0f, 1.0f },
		{ 14.0f/255.0f, 10.0f/255.0f, 30.0f/255.0f, 1.0f },
		{ 42.0f/255.0f, 26.0f/255.0f, 78.0f/255.0f, 1.0f },
		{ 192.0f/255.0f, 64.0f/255.0f, 255.0f/255.0f, 1.0f }
	},
	// ARCHETYPE_BASE  -- bg 0c0a04, primary 1a1200, secondary 3a2000, accent amber #ffb300
	{
		{ 12.0f/255.0f, 10.0f/255.0f, 4.0f/255.0f, 1.0f },
		{ 26.0f/255.0f, 18.0f/255.0f, 0.0f/255.0f, 1.0f },
		{ 58.0f/255.0f, 32.0f/255.0f, 0.0f/255.0f, 1.0f },
		{ 255.0f/255.0f, 179.0f/255.0f, 0.0f/255.0f, 1.0f }
	},
	// ARCHETYPE_DEFAULT  -- bg 060810, primary 080c10, secondary 0a1a1a, accent steel #607d8b
	{
		{ 6.0f/255.0f, 8.0f/255.0f, 16.0f/255.0f, 1.0f },
		{ 8.0f/255.0f, 12.0f/255.0f, 16.0f/255.0f, 1.0f },
		{ 10.0f/255.0f, 26.0f/255.0f, 26.0f/255.0f, 1.0f },
		{ 96.0f/255.0f, 125.0f/255.0f, 139.0f/255.0f, 1.0f }
	}
	// NOLINTEND(misc-redundant-expression)
};

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
	Vector4Copy( pal->accentColor,   cl_loadingTheme.accentColor );

	// grid = accent color at 10 % opacity
	cl_loadingTheme.gridColor[0] = pal->accentColor[0];
	cl_loadingTheme.gridColor[1] = pal->accentColor[1];
	cl_loadingTheme.gridColor[2] = pal->accentColor[2];
	cl_loadingTheme.gridColor[3] = 0.10f;
}
