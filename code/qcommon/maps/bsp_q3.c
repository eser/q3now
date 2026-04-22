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

#if FEAT_RECAST_NAVMESH
#include "../surfaceflags.h"
#include "../nav/nav_local.h"
#include "../nav/nav_coord.h"
#endif

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

/* =========================================================================
   BSP_Q3_ExtractNavGeometry — Q3-format nav geometry extraction callback.

   Ported from nav_bsp.cpp into pure C so all Q3-format knowledge stays in
   this file (bsp_q3.c) and the nav layer remains format-agnostic.

   Called by Nav_Geom_Extract when bsp->format->extractNavGeometry != NULL.
   Two-pass: Pass 1 counts verts/tris, Pass 2 allocates (Hunk_AllocateTempMemory)
   and fills.  Caller must call Nav_Geom_Free(geomOut) when done.
   ========================================================================= */

#if FEAT_RECAST_NAVMESH

#define NAV_BSP_MAX_BEZIER_LEVEL 5
#define NAV_BSP_BEZIER_VERTS  ( (NAV_BSP_MAX_BEZIER_LEVEL+1) * (NAV_BSP_MAX_BEZIER_LEVEL+1) )
#define NAV_BSP_BEZIER_TRIS   ( NAV_BSP_MAX_BEZIER_LEVEL * NAV_BSP_MAX_BEZIER_LEVEL * 2 )
#define NAV_BSP_BEZIER_IDXS   ( NAV_BSP_MAX_BEZIER_LEVEL * (NAV_BSP_MAX_BEZIER_LEVEL+1) * 2 )

typedef struct {
    float        verts[NAV_BSP_BEZIER_VERTS][3];
    unsigned int indexes[NAV_BSP_BEZIER_IDXS];
    int          numVerts;
    int          numIndexes;
    float        ctrl[9][3];
} NavBezier_t;

static void NavBezier_Tessellate( NavBezier_t *b, int L )
{
    int L1 = L + 1;
    int i, j, row, col, k;
    b->numVerts   = L1 * L1;
    b->numIndexes = L * L1 * 2;

    for ( i = 0; i <= L; i++ ) {
        double a = (double)i / L;
        double bv = 1.0 - a;
        for ( k = 0; k < 3; k++ ) {
            b->verts[i][k] = (float)(
                b->ctrl[0][k] * (bv*bv) +
                b->ctrl[3][k] * (2.0*bv*a) +
                b->ctrl[6][k] * (a*a) );
        }
    }

    for ( i = 1; i <= L; i++ ) {
        double a = (double)i / L;
        double bv = 1.0 - a;
        float temp[3][3];
        for ( j = 0; j < 3; j++ ) {
            int base = j * 3;
            for ( k = 0; k < 3; k++ ) {
                temp[j][k] = (float)(
                    b->ctrl[base  ][k] * (bv*bv) +
                    b->ctrl[base+1][k] * (2.0*bv*a) +
                    b->ctrl[base+2][k] * (a*a) );
            }
        }
        for ( j = 0; j <= L; j++ ) {
            double aj = (double)j / L;
            double bj = 1.0 - aj;
            for ( k = 0; k < 3; k++ ) {
                b->verts[i * L1 + j][k] = (float)(
                    temp[0][k] * (bj*bj) +
                    temp[1][k] * (2.0*bj*aj) +
                    temp[2][k] * (aj*aj) );
            }
        }
    }

    for ( row = 0; row < L; row++ ) {
        for ( col = 0; col <= L; col++ ) {
            b->indexes[ (row * L1 + col) * 2 + 0 ] = (unsigned int)((row + 1) * L1 + col);
            b->indexes[ (row * L1 + col) * 2 + 1 ] = (unsigned int)( row      * L1 + col);
        }
    }
}

typedef struct {
    float         *verts;
    int           *tris;
    unsigned char *areas;
    int            numVerts;
    int            numTris;
} NavGeomWriter_t;

static void NavGeomWriter_WriteVert( NavGeomWriter_t *w, const float r[3] )
{
    w->verts[ w->numVerts * 3 + 0 ] = r[0];
    w->verts[ w->numVerts * 3 + 1 ] = r[1];
    w->verts[ w->numVerts * 3 + 2 ] = r[2];
    w->numVerts++;
}

/* Reverse winding: Q3 is CW from above; Recast expects CCW. */
static void NavGeomWriter_WriteTri( NavGeomWriter_t *w, int a, int b, int c,
                                    unsigned char area )
{
    w->tris[ w->numTris * 3 + 0 ] = c;
    w->tris[ w->numTris * 3 + 1 ] = b;
    w->tris[ w->numTris * 3 + 2 ] = a;
    w->areas[ w->numTris ] = area;
    w->numTris++;
}

static void BSP_Q3_CountPlanarOrSoup( const dsurface_t *surf, int *nv, int *nt )
{
    *nv += surf->numVerts;
    *nt += surf->numIndexes / 3;
}

static void BSP_Q3_CountPatch( const dsurface_t *surf, int *nv, int *nt )
{
    int npx = (surf->patchWidth  - 1) / 2;
    int npy = (surf->patchHeight - 1) / 2;
    int n   = npx * npy;
    *nv += n * NAV_BSP_BEZIER_VERTS;
    *nt += n * NAV_BSP_BEZIER_TRIS;
}

static void BSP_Q3_FillPlanarOrSoup( NavGeomWriter_t *w,
                                     const dsurface_t *surf,
                                     const drawVert_t *drawVerts,
                                     const int        *drawIndexes,
                                     unsigned char     area )
{
    int base = w->numVerts;
    int v, i;
    float r[3];

    for ( v = 0; v < surf->numVerts; v++ ) {
        const float *xyz = drawVerts[ surf->firstVert + v ].xyz;
        Nav_QuakeToRecast( xyz, r );
        NavGeomWriter_WriteVert( w, r );
    }

    for ( i = 0; i + 2 < surf->numIndexes; i += 3 ) {
        int a = drawIndexes[ surf->firstIndex + i + 0 ];
        int b = drawIndexes[ surf->firstIndex + i + 1 ];
        int c = drawIndexes[ surf->firstIndex + i + 2 ];
        NavGeomWriter_WriteTri( w, base + a, base + b, base + c, area );
    }
}

static void BSP_Q3_FillPatch( NavGeomWriter_t  *w,
                               const dsurface_t *surf,
                               const drawVert_t *drawVerts,
                               unsigned char     area )
{
    int pw  = surf->patchWidth;
    int ph  = surf->patchHeight;
    int npx = (pw - 1) / 2;
    int npy = (ph - 1) / 2;
    int pi, pj, ci, cj, v, row, d;
    int L  = NAV_BSP_MAX_BEZIER_LEVEL;
    int L1 = L + 1;
    float r[3];

    for ( pj = 0; pj < npy; pj++ ) {
        for ( pi = 0; pi < npx; pi++ ) {
            NavBezier_t bez;
            int base;

            for ( cj = 0; cj < 3; cj++ ) {
                for ( ci = 0; ci < 3; ci++ ) {
                    int cx   = pi * 2 + ci;
                    int cy   = pj * 2 + cj;
                    int vidx = surf->firstVert + cy * pw + cx;
                    const float *xyz = drawVerts[vidx].xyz;
                    bez.ctrl[cj * 3 + ci][0] = xyz[0];
                    bez.ctrl[cj * 3 + ci][1] = xyz[1];
                    bez.ctrl[cj * 3 + ci][2] = xyz[2];
                }
            }

            NavBezier_Tessellate( &bez, L );

            base = w->numVerts;
            for ( v = 0; v < bez.numVerts; v++ ) {
                Nav_QuakeToRecast( bez.verts[v], r );
                NavGeomWriter_WriteVert( w, r );
            }

            for ( row = 0; row < L; row++ ) {
                const unsigned int *strip = &bez.indexes[ row * L1 * 2 ];
                int stripLen = L1 * 2;
                for ( d = 0; d + 2 < stripLen; d++ ) {
                    if ( d & 1 ) {
                        NavGeomWriter_WriteTri( w, base + (int)strip[d],
                                               base + (int)strip[d+2],
                                               base + (int)strip[d+1], area );
                    } else {
                        NavGeomWriter_WriteTri( w, base + (int)strip[d],
                                               base + (int)strip[d+1],
                                               base + (int)strip[d+2], area );
                    }
                }
            }
        }
    }
}

static qboolean BSP_Q3_SurfaceIsWalkable( const dsurface_t *surf,
                                           const dshader_t  *shader )
{
    if ( shader->surfaceFlags & ( SURF_NODRAW | SURF_SKY | SURF_NONSOLID ) )
        return qfalse;
    if ( surf->surfaceType == MST_FLARE || surf->surfaceType == MST_BAD )
        return qfalse;
    /* Accept solid, playerclip, and liquid surfaces.
     * Liquid brushes (lava/slime/water) lack CONTENTS_SOLID but are walkable
     * platforms with a cost penalty; excluding them leaves gaps in the navmesh
     * where liquid floors exist. Playerclip brushes are invisible but walkable —
     * excluding them leaves nav holes where architects used clip geometry. */
    if ( !( shader->contentFlags & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP |
                                      CONTENTS_LAVA  | CONTENTS_SLIME | CONTENTS_WATER ) ) )
        return qfalse;
    return qtrue;
}

static qboolean BSP_Q3_ExtractNavGeometry( const bspFile_t  *bsp,
                                            struct navGeom_s *outGeom )
{
    navGeom_t *geom = (navGeom_t *)outGeom;
    int numVerts = 0;
    int numTris  = 0;
    int i;
    NavGeomWriter_t wr;

    /* --- Pass 1: count --- */
    for ( i = 0; i < bsp->numSurfaces; i++ ) {
        const dsurface_t *surf   = &bsp->surfaces[i];
        const dshader_t  *shader = &bsp->shaders[ surf->shaderNum ];

        if ( !BSP_Q3_SurfaceIsWalkable( surf, shader ) )
            continue;

        switch ( surf->surfaceType ) {
        case MST_PLANAR:
        case MST_TRIANGLE_SOUP:
            BSP_Q3_CountPlanarOrSoup( surf, &numVerts, &numTris );
            break;
        case MST_PATCH:
            BSP_Q3_CountPatch( surf, &numVerts, &numTris );
            break;
        default:
            break;
        }
    }

    memset( geom, 0, sizeof(*geom) );

    if ( numVerts == 0 || numTris == 0 ) {
        Com_Printf( S_COLOR_YELLOW "[NAV] no walkable tris extracted from %s\n", bsp->name );
        return qfalse;
    }

    /* Warn if tri count changed >20% vs the last extraction of this map
     * (session-only memory — detects filter regressions without a full rebuild). */
    {
        static char s_lastMap[256];
        static int  s_lastTris;
        if ( *s_lastMap && Q_stricmp( s_lastMap, bsp->name ) == 0 && s_lastTris > 0 ) {
            int delta = numTris - s_lastTris;
            if ( delta < 0 ) delta = -delta;
            if ( delta * 100 / s_lastTris > 20 ) {
                Com_Printf( S_COLOR_YELLOW "[NAV] tri count changed >20%% on %s: was %d, now %d\n",
                            bsp->name, s_lastTris, numTris );
            }
        }
        Q_strncpyz( s_lastMap, bsp->name, sizeof(s_lastMap) );
        s_lastTris = numTris;
    }

    /* --- Allocate in LIFO order: verts (1st), tris (2nd), areas (3rd/top) --- */
    geom->verts = (float *)        Hunk_AllocateTempMemory( numVerts * 3 * (int)sizeof(float) );
    geom->tris  = (int *)          Hunk_AllocateTempMemory( numTris  * 3 * (int)sizeof(int) );
    geom->areas = (unsigned char *)Hunk_AllocateTempMemory( numTris      * (int)sizeof(unsigned char) );

    /* --- Pass 2: fill --- */
    wr.verts    = geom->verts;
    wr.tris     = geom->tris;
    wr.areas    = geom->areas;
    wr.numVerts = 0;
    wr.numTris  = 0;

    for ( i = 0; i < bsp->numSurfaces; i++ ) {
        const dsurface_t *surf   = &bsp->surfaces[i];
        const dshader_t  *shader = &bsp->shaders[ surf->shaderNum ];

        if ( !BSP_Q3_SurfaceIsWalkable( surf, shader ) )
            continue;

        unsigned char area;
        if ( shader->contentFlags & CONTENTS_WATER )
            area = (unsigned char)NAVAREA_WATER;
        else if ( shader->contentFlags & ( CONTENTS_LAVA | CONTENTS_SLIME ) )
            area = (unsigned char)NAVAREA_LAVA;   /* Phase 1 §8: lava and slime share one area ID */
        else
            area = (unsigned char)NAVAREA_GROUND;

        switch ( surf->surfaceType ) {
        case MST_PLANAR:
        case MST_TRIANGLE_SOUP:
            BSP_Q3_FillPlanarOrSoup( &wr, surf,
                                     bsp->drawVerts, bsp->drawIndexes, area );
            break;
        case MST_PATCH:
            BSP_Q3_FillPatch( &wr, surf, bsp->drawVerts, area );
            break;
        default:
            break;
        }
    }

    geom->numVerts = wr.numVerts;
    geom->numTris  = wr.numTris;

    Com_Printf( "[NAV] %s: %d verts, %d tris extracted\n",
                bsp->name, geom->numVerts, geom->numTris );
    return qtrue;
}

#endif /* FEAT_RECAST_NAVMESH */

const bspFormat_t bspFormatQ3 = {
	"Quake 3",
	BSP_IDENT,
	BSP_VERSION,
	BSP_Q3_Load,
#if FEAT_RECAST_NAVMESH
	BSP_Q3_ExtractNavGeometry
#else
	NULL
#endif
};
