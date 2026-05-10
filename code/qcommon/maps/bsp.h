/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// bsp.h -- pluggable BSP format abstraction (FEAT_BSP_ABSTRACTION)

#ifndef __BSP_H
#define __BSP_H

#include "../q_shared.h"
#include "../qfiles.h"

// Maximum registered BSP formats
#define MAX_BSP_FORMATS 8

// Quake 1 BSP29 — version is the first (and only) u32 in the header; no ident field
#define BSP_VERSION_Q1   29
#define HEADER_LUMPS_Q1  15

// Forward declarations
typedef struct bspFormat_s bspFormat_t;
typedef struct bspFile_s bspFile_t;

// Forward declaration for the nav geometry type.
// Full definition is in code/qcommon/nav/nav_local.h.
// Used by bspFormat_t.extractNavGeometry; bsp.h itself need not know the layout.
struct navGeom_s;

typedef enum {
	BSP_ASSET_PROFILE_MODERN = 0,
	BSP_ASSET_PROFILE_LEGACY
} bspAssetProfile_t;

// BSP file - parsed representation of a BSP map
typedef struct bspFile_s {
	char		name[MAX_QPATH];
	int			ident;
	int			version;
	int			references;

	// Format that loaded this file.  Set by BSP_Load after successful load.
	// NULL only if the file was loaded before this field was added.
	const bspFormat_t *format;

	int			checksum;
	int			rawLength;
	byte		*rawData;

	// Entity string
	int			entityStringLength;
	char		*entityString;

	// Shaders
	int			numShaders;
	dshader_t	*shaders;

	// Planes
	int			numPlanes;
	dplane_t	*planes;

	// Nodes
	int			numNodes;
	dnode_t		*nodes;

	// Leafs
	int			numLeafs;
	dleaf_t		*leafs;

	// Leaf surfaces
	int			numLeafSurfaces;
	int			*leafSurfaces;

	// Leaf brushes
	int			numLeafBrushes;
	int			*leafBrushes;

	// Models (submodels)
	int			numSubModels;
	dmodel_t	*subModels;

	// Brushes
	int			numBrushes;
	dbrush_t	*brushes;

	// Brush sides
	int			numBrushSides;
	dbrushside_t *brushSides;

	// Surfaces
	int			numSurfaces;
	dsurface_t	*surfaces;

	// Fogs
	int			numFogs;
	dfog_t		*fogs;

	// Draw verts
	int			numDrawVerts;
	drawVert_t	*drawVerts;

	// Draw indexes
	int			numDrawIndexes;
	int			*drawIndexes;

	// Visibility
	int			numClusters;
	int			clusterBytes;
	int			visibilityLength;
	byte		*visibility;

	// Lightmaps (raw data)
	int			numLightmapPages;
	int			lightmapPageSize;	// typically 128*128*3
	byte		*lightmapData;

	// Light grid
	int			numGridPoints;
	byte		*lightGridData;

	// Q1-prep: embedded texture block (BSP29 miptex lump; NULL for Q3)
	byte		*embeddedTextures;
	int			embeddedTexturesLength;
	int			numEmbeddedTextures;

	// Q1-prep: styled lightmaps (BSP29 has 4 style slots per surface; NULL for Q3)
	int			lightmapStyles;
	byte		*styledLightmapData[4];
	int			numStyledLightmapPages[4];

	// Q1-prep: per-vertex lightstyle slot indices (4 bytes per drawVert; NULL for Q3)
	// Index values 0..63 reference global lightstyle table; 255 = unused slot.
	byte		*drawVertLightstyles;

} bspFile_t;

// Flags for BSP_Load.
#define BSP_LOAD_FLAGS_NONE        0u
// Skip collision-system side-effects (CMQ1_StoreClipnodes / CMQ1_StoreLeafContents).
// Set by callers that load a BSP purely for rendering or asset extraction and must not
// overwrite the live world collision tree.
#define BSP_LOAD_FLAG_RENDER_ONLY  (1u << 0)

// BSP format loader interface
typedef struct bspFormat_s {
	const char	*name;			// e.g. "Quake 3"
	int			ident;			// magic number (e.g. BSP_IDENT)
	int			version;		// BSP version (e.g. BSP_VERSION)
	// Returns qtrue if this format owns the given raw buffer.
	// Must not be NULL — asserted in BSP_RegisterFormat.
	qboolean	(*detect)( const void *buf, int len );
	qboolean	(*loadFunction)( const bspFormat_t *format, const char *name,
					const void *data, int length, unsigned flags, bspFile_t **bspFile );
	// NULL if this format does not support nav geometry extraction.
	// When NULL, Nav_LoadMap logs a warning and nav.ready stays qfalse.
	qboolean	(*extractNavGeometry)( const bspFile_t *bsp,
	                                   struct navGeom_s *outGeom );
} bspFormat_t;

// .lit sidecar loader — shared by Q1 and Q3 paths
#define LIT_MAGIC    ( ('Q') | ('L' << 8) | ('I' << 16) | ('T' << 24) )
#define LIT_VERSION  1
// Load a .lit file and return a Z_Malloc'd RGB buffer of expectedRGBBytes, or NULL.
byte		*Lit_TryLoad( const char *litPath, int expectedRGBBytes );

// Public API
void		BSP_Init( void );
void		BSP_RegisterFormat( const bspFormat_t *format );
qboolean	BSP_Load( const char *name, bspFile_t **bspFile, unsigned flags );
void		BSP_Free( bspFile_t *bspFile );
void		BSP_ClearMapCache( void );
void		BSP_Shutdown( void );
bspAssetProfile_t BSP_AssetProfileForVersion( int version );
const char	*BSP_DefaultSoundExtForProfile( bspAssetProfile_t profile );
const char	*BSP_DefaultImageExtForProfile( bspAssetProfile_t profile );

#endif // __BSP_H
