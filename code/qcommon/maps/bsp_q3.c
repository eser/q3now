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

// bsp_q3.c -- Quake 3 BSP format loader (FEAT_BSP_ABSTRACTION)
//
// Parses a raw Q3 BSP (IBSP v46) byte buffer into a bspFile_t.
// Runs in parallel with CM_LoadMap/RE_LoadWorldMap, which still consume
// raw lump data directly. BSP_Q3_Load provides a format-agnostic entry
// point for future loaders (headless tools, alternative map pipelines,
// etc.) that want an already-parsed, byteswapped representation.
//
// All allocations go through Z_Malloc so BSP_Free can release them
// symmetrically with matching Z_Free calls, independent of the hunk
// state.

#include "../q_shared.h"
#include "../qcommon.h"
#include "bsp.h"

// Lightmap page dimensions (matches id's stock Q3 lightmap layout).
#define BSP_LIGHTMAP_PAGE_SIZE	( 128 * 128 * 3 )
// Light grid entries are 8 bytes each (ambient + directed colors + dir).
#define BSP_LIGHTGRID_ENTRY_SIZE	8

static void *BSP_ZAlloc( size_t size ) {
	void *ptr;
	if ( size == 0 ) {
		return NULL;
	}
	ptr = Z_Malloc( size );
	if ( !ptr ) {
		Com_Error( ERR_FATAL, "BSP_Q3_Load: Z_Malloc(%u) failed", (unsigned)size );
	}
	return ptr;
}

static qboolean BSP_Q3_Load( const bspFormat_t *format, const char *name,
	const void *data, int length, bspFile_t **bspFile ) {
	int			i, j;
	dheader_t	header;
	bspFile_t	*bsp;
	const byte	*base;

	*bspFile = NULL;

	if ( length < (int)sizeof( dheader_t ) ) {
		Com_Printf( "BSP_Q3_Load: %s has truncated header\n", name );
		return qfalse;
	}

	// Byteswap the fixed-size header in place on a local copy.
	memcpy( &header, data, sizeof( dheader_t ) );
	header.ident = LittleLong( header.ident );
	header.version = LittleLong( header.version );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		header.lumps[i].fileofs = LittleLong( header.lumps[i].fileofs );
		header.lumps[i].filelen = LittleLong( header.lumps[i].filelen );
	}

	if ( header.ident != format->ident || header.version != format->version ) {
		return qfalse;
	}

	// Validate every lump range against the file bounds.
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		uint32_t ofs = header.lumps[i].fileofs;
		uint32_t len = header.lumps[i].filelen;
		if ( (uint64_t)ofs + len > (uint64_t)length ) {
			Com_Printf( "BSP_Q3_Load: %s lump %i out of range (ofs=%u len=%u file=%i)\n",
				name, i, ofs, len, length );
			return qfalse;
		}
	}

	base = (const byte *)data;

	bsp = BSP_ZAlloc( sizeof( *bsp ) );
	memset( bsp, 0, sizeof( *bsp ) );
	Q_strncpyz( bsp->name, name, sizeof( bsp->name ) );
	bsp->checksum = LittleLong( Com_BlockChecksum( data, length ) );
	bsp->rawLength = length;
	bsp->rawData = BSP_ZAlloc( length );
	memcpy( bsp->rawData, data, length );

	// ---- Entity string ----
	{
		const lump_t *l = &header.lumps[LUMP_ENTITIES];
		bsp->entityStringLength = l->filelen;
		bsp->entityString = BSP_ZAlloc( l->filelen + 1 );
		if ( l->filelen ) {
			memcpy( bsp->entityString, base + l->fileofs, l->filelen );
		}
		bsp->entityString[l->filelen] = 0;
	}

	// ---- Shaders ----
	{
		const lump_t *l = &header.lumps[LUMP_SHADERS];
		const dshader_t *in = (const dshader_t *)( base + l->fileofs );
		dshader_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny shader lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numShaders = l->filelen / sizeof( *in );
		bsp->shaders = BSP_ZAlloc( bsp->numShaders * sizeof( *bsp->shaders ) );
		out = bsp->shaders;
		for ( i = 0; i < bsp->numShaders; i++, in++, out++ ) {
			Q_strncpyz( out->shader, in->shader, sizeof( out->shader ) );
			out->surfaceFlags = LittleLong( in->surfaceFlags );
			out->contentFlags = LittleLong( in->contentFlags );
		}
	}

	// ---- Planes ----
	{
		const lump_t *l = &header.lumps[LUMP_PLANES];
		const dplane_t *in = (const dplane_t *)( base + l->fileofs );
		dplane_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny planes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numPlanes = l->filelen / sizeof( *in );
		bsp->planes = BSP_ZAlloc( bsp->numPlanes * sizeof( *bsp->planes ) );
		out = bsp->planes;
		for ( i = 0; i < bsp->numPlanes; i++, in++, out++ ) {
			for ( j = 0; j < 3; j++ ) {
				out->normal[j] = LittleFloat( in->normal[j] );
			}
			out->dist = LittleFloat( in->dist );
		}
	}

	// ---- Nodes ----
	{
		const lump_t *l = &header.lumps[LUMP_NODES];
		const dnode_t *in = (const dnode_t *)( base + l->fileofs );
		dnode_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny nodes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numNodes = l->filelen / sizeof( *in );
		bsp->nodes = BSP_ZAlloc( bsp->numNodes * sizeof( *bsp->nodes ) );
		out = bsp->nodes;
		for ( i = 0; i < bsp->numNodes; i++, in++, out++ ) {
			out->planeNum = LittleLong( in->planeNum );
			out->children[0] = LittleLong( in->children[0] );
			out->children[1] = LittleLong( in->children[1] );
			for ( j = 0; j < 3; j++ ) {
				out->mins[j] = LittleLong( in->mins[j] );
				out->maxs[j] = LittleLong( in->maxs[j] );
			}
		}
	}

	// ---- Leafs ----
	{
		const lump_t *l = &header.lumps[LUMP_LEAFS];
		const dleaf_t *in = (const dleaf_t *)( base + l->fileofs );
		dleaf_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny leafs lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numLeafs = l->filelen / sizeof( *in );
		bsp->leafs = BSP_ZAlloc( bsp->numLeafs * sizeof( *bsp->leafs ) );
		out = bsp->leafs;
		for ( i = 0; i < bsp->numLeafs; i++, in++, out++ ) {
			out->cluster = LittleLong( in->cluster );
			out->area = LittleLong( in->area );
			for ( j = 0; j < 3; j++ ) {
				out->mins[j] = LittleLong( in->mins[j] );
				out->maxs[j] = LittleLong( in->maxs[j] );
			}
			out->firstLeafSurface = LittleLong( in->firstLeafSurface );
			out->numLeafSurfaces = LittleLong( in->numLeafSurfaces );
			out->firstLeafBrush = LittleLong( in->firstLeafBrush );
			out->numLeafBrushes = LittleLong( in->numLeafBrushes );
		}
	}

	// ---- Leaf surfaces ----
	{
		const lump_t *l = &header.lumps[LUMP_LEAFSURFACES];
		const int *in = (const int *)( base + l->fileofs );
		int *out;
		if ( l->filelen % sizeof( int ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny leafsurfaces lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numLeafSurfaces = l->filelen / sizeof( int );
		bsp->leafSurfaces = BSP_ZAlloc( bsp->numLeafSurfaces * sizeof( *bsp->leafSurfaces ) );
		out = bsp->leafSurfaces;
		for ( i = 0; i < bsp->numLeafSurfaces; i++ ) {
			out[i] = LittleLong( in[i] );
		}
	}

	// ---- Leaf brushes ----
	{
		const lump_t *l = &header.lumps[LUMP_LEAFBRUSHES];
		const int *in = (const int *)( base + l->fileofs );
		int *out;
		if ( l->filelen % sizeof( int ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny leafbrushes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numLeafBrushes = l->filelen / sizeof( int );
		bsp->leafBrushes = BSP_ZAlloc( bsp->numLeafBrushes * sizeof( *bsp->leafBrushes ) );
		out = bsp->leafBrushes;
		for ( i = 0; i < bsp->numLeafBrushes; i++ ) {
			out[i] = LittleLong( in[i] );
		}
	}

	// ---- Submodels ----
	{
		const lump_t *l = &header.lumps[LUMP_MODELS];
		const dmodel_t *in = (const dmodel_t *)( base + l->fileofs );
		dmodel_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny models lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numSubModels = l->filelen / sizeof( *in );
		bsp->subModels = BSP_ZAlloc( bsp->numSubModels * sizeof( *bsp->subModels ) );
		out = bsp->subModels;
		for ( i = 0; i < bsp->numSubModels; i++, in++, out++ ) {
			for ( j = 0; j < 3; j++ ) {
				out->mins[j] = LittleFloat( in->mins[j] );
				out->maxs[j] = LittleFloat( in->maxs[j] );
			}
			out->firstSurface = LittleLong( in->firstSurface );
			out->numSurfaces = LittleLong( in->numSurfaces );
			out->firstBrush = LittleLong( in->firstBrush );
			out->numBrushes = LittleLong( in->numBrushes );
		}
	}

	// ---- Brushes ----
	{
		const lump_t *l = &header.lumps[LUMP_BRUSHES];
		const dbrush_t *in = (const dbrush_t *)( base + l->fileofs );
		dbrush_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny brushes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numBrushes = l->filelen / sizeof( *in );
		bsp->brushes = BSP_ZAlloc( bsp->numBrushes * sizeof( *bsp->brushes ) );
		out = bsp->brushes;
		for ( i = 0; i < bsp->numBrushes; i++, in++, out++ ) {
			out->firstSide = LittleLong( in->firstSide );
			out->numSides = LittleLong( in->numSides );
			out->shaderNum = LittleLong( in->shaderNum );
		}
	}

	// ---- Brush sides ----
	{
		const lump_t *l = &header.lumps[LUMP_BRUSHSIDES];
		const dbrushside_t *in = (const dbrushside_t *)( base + l->fileofs );
		dbrushside_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny brushsides lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numBrushSides = l->filelen / sizeof( *in );
		bsp->brushSides = BSP_ZAlloc( bsp->numBrushSides * sizeof( *bsp->brushSides ) );
		out = bsp->brushSides;
		for ( i = 0; i < bsp->numBrushSides; i++, in++, out++ ) {
			out->planeNum = LittleLong( in->planeNum );
			out->shaderNum = LittleLong( in->shaderNum );
		}
	}

	// ---- Draw verts ----
	{
		const lump_t *l = &header.lumps[LUMP_DRAWVERTS];
		const drawVert_t *in = (const drawVert_t *)( base + l->fileofs );
		drawVert_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny drawverts lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numDrawVerts = l->filelen / sizeof( *in );
		bsp->drawVerts = BSP_ZAlloc( bsp->numDrawVerts * sizeof( *bsp->drawVerts ) );
		out = bsp->drawVerts;
		for ( i = 0; i < bsp->numDrawVerts; i++, in++, out++ ) {
			for ( j = 0; j < 3; j++ ) {
				out->xyz[j] = LittleFloat( in->xyz[j] );
				out->normal[j] = LittleFloat( in->normal[j] );
			}
			out->st[0] = LittleFloat( in->st[0] );
			out->st[1] = LittleFloat( in->st[1] );
			out->lightmap[0] = LittleFloat( in->lightmap[0] );
			out->lightmap[1] = LittleFloat( in->lightmap[1] );
			// Vertex color bytes: no byteswap needed.
			out->color.rgba[0] = in->color.rgba[0];
			out->color.rgba[1] = in->color.rgba[1];
			out->color.rgba[2] = in->color.rgba[2];
			out->color.rgba[3] = in->color.rgba[3];
		}
	}

	// ---- Draw indexes ----
	{
		const lump_t *l = &header.lumps[LUMP_DRAWINDEXES];
		const int *in = (const int *)( base + l->fileofs );
		int *out;
		if ( l->filelen % sizeof( int ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny drawindexes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numDrawIndexes = l->filelen / sizeof( int );
		bsp->drawIndexes = BSP_ZAlloc( bsp->numDrawIndexes * sizeof( *bsp->drawIndexes ) );
		out = bsp->drawIndexes;
		for ( i = 0; i < bsp->numDrawIndexes; i++ ) {
			out[i] = LittleLong( in[i] );
		}
	}

	// ---- Surfaces ----
	{
		const lump_t *l = &header.lumps[LUMP_SURFACES];
		const dsurface_t *in = (const dsurface_t *)( base + l->fileofs );
		dsurface_t *out;
		int k;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny surfaces lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numSurfaces = l->filelen / sizeof( *in );
		bsp->surfaces = BSP_ZAlloc( bsp->numSurfaces * sizeof( *bsp->surfaces ) );
		out = bsp->surfaces;
		for ( i = 0; i < bsp->numSurfaces; i++, in++, out++ ) {
			out->shaderNum = LittleLong( in->shaderNum );
			out->fogNum = LittleLong( in->fogNum );
			out->surfaceType = LittleLong( in->surfaceType );
			out->firstVert = LittleLong( in->firstVert );
			out->numVerts = LittleLong( in->numVerts );
			out->firstIndex = LittleLong( in->firstIndex );
			out->numIndexes = LittleLong( in->numIndexes );
			out->lightmapNum = LittleLong( in->lightmapNum );
			out->lightmapX = LittleLong( in->lightmapX );
			out->lightmapY = LittleLong( in->lightmapY );
			out->lightmapWidth = LittleLong( in->lightmapWidth );
			out->lightmapHeight = LittleLong( in->lightmapHeight );
			for ( j = 0; j < 3; j++ ) {
				out->lightmapOrigin[j] = LittleFloat( in->lightmapOrigin[j] );
				for ( k = 0; k < 3; k++ ) {
					out->lightmapVecs[j][k] = LittleFloat( in->lightmapVecs[j][k] );
				}
			}
			out->patchWidth = LittleLong( in->patchWidth );
			out->patchHeight = LittleLong( in->patchHeight );
		}
	}

	// ---- Fogs ----
	{
		const lump_t *l = &header.lumps[LUMP_FOGS];
		const dfog_t *in = (const dfog_t *)( base + l->fileofs );
		dfog_t *out;
		if ( l->filelen % sizeof( *in ) ) {
			Com_Printf( "BSP_Q3_Load: %s funny fogs lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numFogs = l->filelen / sizeof( *in );
		bsp->fogs = BSP_ZAlloc( bsp->numFogs * sizeof( *bsp->fogs ) );
		out = bsp->fogs;
		for ( i = 0; i < bsp->numFogs; i++, in++, out++ ) {
			Q_strncpyz( out->shader, in->shader, sizeof( out->shader ) );
			out->brushNum = LittleLong( in->brushNum );
			out->visibleSide = LittleLong( in->visibleSide );
		}
	}

	// ---- Visibility ----
	{
		const lump_t *l = &header.lumps[LUMP_VISIBILITY];
		if ( l->filelen >= VIS_HEADER ) {
			const byte *in = base + l->fileofs;
			bsp->numClusters = LittleLong( ((const int *)in)[0] );
			bsp->clusterBytes = LittleLong( ((const int *)in)[1] );
			bsp->visibilityLength = l->filelen - VIS_HEADER;
			if ( bsp->visibilityLength > 0 ) {
				bsp->visibility = BSP_ZAlloc( bsp->visibilityLength );
				memcpy( bsp->visibility, in + VIS_HEADER, bsp->visibilityLength );
			}
		}
	}

	// ---- Lightmaps ----
	{
		const lump_t *l = &header.lumps[LUMP_LIGHTMAPS];
		if ( l->filelen > 0 ) {
			if ( l->filelen % BSP_LIGHTMAP_PAGE_SIZE ) {
				Com_Printf( "BSP_Q3_Load: %s lightmap lump not a multiple of %d\n",
					name, BSP_LIGHTMAP_PAGE_SIZE );
				BSP_Free( bsp );
				return qfalse;
			}
			bsp->numLightmapPages = l->filelen / BSP_LIGHTMAP_PAGE_SIZE;
			bsp->lightmapPageSize = BSP_LIGHTMAP_PAGE_SIZE;
			bsp->lightmapData = BSP_ZAlloc( l->filelen );
			memcpy( bsp->lightmapData, base + l->fileofs, l->filelen );
		}
	}

	// ---- Light grid ----
	{
		const lump_t *l = &header.lumps[LUMP_LIGHTGRID];
		if ( l->filelen > 0 ) {
			if ( l->filelen % BSP_LIGHTGRID_ENTRY_SIZE ) {
				Com_Printf( "BSP_Q3_Load: %s light grid lump not a multiple of %d\n",
					name, BSP_LIGHTGRID_ENTRY_SIZE );
				BSP_Free( bsp );
				return qfalse;
			}
			bsp->numGridPoints = l->filelen / BSP_LIGHTGRID_ENTRY_SIZE;
			bsp->lightGridData = BSP_ZAlloc( l->filelen );
			memcpy( bsp->lightGridData, base + l->fileofs, l->filelen );
		}
	}

	*bspFile = bsp;
	return qtrue;
}

const bspFormat_t bspFormatQ3 = {
	"Quake 3",
	BSP_IDENT,
	BSP_VERSION,
	BSP_Q3_Load
};
