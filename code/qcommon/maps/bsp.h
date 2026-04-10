/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 q3now contributors

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

// Forward declarations
typedef struct bspFormat_s bspFormat_t;
typedef struct bspFile_s bspFile_t;

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

	// Grid bounds (from world model)
	// These are derived during loading, not stored in the file

} bspFile_t;

// BSP format loader interface
typedef struct bspFormat_s {
	const char	*name;			// e.g. "Quake 3"
	int			ident;			// magic number (e.g. BSP_IDENT)
	int			version;		// BSP version (e.g. BSP_VERSION)
	qboolean	(*loadFunction)( const bspFormat_t *format, const char *name,
					const void *data, int length, bspFile_t **bspFile );
} bspFormat_t;

// Public API
void		BSP_Init( void );
void		BSP_RegisterFormat( const bspFormat_t *format );
qboolean	BSP_Load( const char *name, bspFile_t **bspFile );
void		BSP_Free( bspFile_t *bspFile );
void		BSP_Shutdown( void );
bspAssetProfile_t BSP_AssetProfileForVersion( int version );
const char	*BSP_DefaultSoundExtForProfile( bspAssetProfile_t profile );
const char	*BSP_DefaultImageExtForProfile( bspAssetProfile_t profile );

#endif // __BSP_H
