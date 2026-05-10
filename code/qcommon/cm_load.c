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
// cmodel.c -- model loading

#include "cm_local.h"
#include "cm_patch.h"
#include "maps/bsp.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_collision, "collision" );
LOG_DECLARE_CHANNEL( ch_loading, "loading" );

#ifdef BSPC

#include "../bspc/l_qfiles.h"

void SetPlaneSignbits( cplane_t *out ) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for ( j = 0; j < 3; j++) {
		if ( out->normal[j] < 0 ) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}
#endif //BSPC

// to allow boxes to be treated as brush models, we allocate
// some extra indexes along with those needed by the map
#define	BOX_BRUSHES		1
#define	BOX_SIDES		6
#define	BOX_LEAFS		2
#define	BOX_PLANES		12

#define	LL(x) x=LittleLong(x)


clipMap_t	cm;
int			c_pointcontents;
int			c_traces, c_brush_traces, c_patch_traces;


static bspFile_t *cm_bsp;

#ifndef BSPC
cvar_t		*cm_noAreas;
cvar_t		*cm_noCurves;
cvar_t		*cm_playerCurveClip;
#endif

static cmodel_t box_model;
static cplane_t *box_planes;
static cbrush_t *box_brush;



static void	CM_InitBoxHull (void);
void	CM_FloodAreaConnections (void);


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

/*
=================
CMod_LoadShaders
=================
*/
static void CMod_LoadShaders( void ) {
	int count = cm_bsp->numShaders;
	if ( count < 1 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: map with no shaders", __func__ );
	}

	cm.shaders = Hunk_Alloc( count * sizeof( *cm.shaders ), h_high );
	cm.numShaders = count;

	memcpy( cm.shaders, cm_bsp->shaders, count * sizeof( *cm.shaders ) );
}


/*
=================
CMod_LoadSubmodels
=================
*/
static void CMod_LoadSubmodels( void ) {
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;
	int			*indexes;
	unsigned	firstBrush, numBrushes, firstSurface, numSurfaces;

	in = cm_bsp->subModels;
	count = cm_bsp->numSubModels;
	if ( count < 1 )
		Com_Terminate( TERM_CLIENT_DROP, "%s: map with no models", __func__ );

	if ( count > MAX_SUBMODELS )
		Com_Terminate( TERM_CLIENT_DROP, "%s: MAX_SUBMODELS exceeded", __func__ );

	cm.cmodels = Hunk_Alloc( count * sizeof( *cm.cmodels ), h_high );
	cm.numSubModels = count;

	for ( i = 0; i < count; i++, in++ )
	{
		out = &cm.cmodels[i];

		for ( j = 0; j < 3; j++ )
		{	// spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j] - 1;
			out->maxs[j] = in->maxs[j] + 1;
		}

		if ( i == 0 ) {
			continue;	// world model doesn't need other info
		}

		firstBrush = in->firstBrush;
		numBrushes = in->numBrushes;
		if ( (uint64_t)firstBrush + numBrushes > cm.numBrushes ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad brushes", __func__ );
		}

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numLeafBrushes = numBrushes;
		indexes = Hunk_Alloc( numBrushes * sizeof( *indexes ), h_high );
		out->leaf.firstLeafBrush = indexes - cm.leafbrushes;
		for ( j = 0 ; j < numBrushes ; j++ ) {
			indexes[j] = firstBrush + j;
		}

		firstSurface = in->firstSurface;
		numSurfaces = in->numSurfaces;
		if ( (uint64_t)firstSurface + numSurfaces > cm.numSurfaces ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad surfaces", __func__ );
		}

		out->leaf.numLeafSurfaces = numSurfaces;
		indexes = Hunk_Alloc( numSurfaces * sizeof( *indexes ), h_high );
		out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;
		for ( j = 0 ; j < numSurfaces ; j++ ) {
			indexes[j] = firstSurface + j;
		}
	}
}


/*
=================
CMod_LoadNodes

=================
*/
static void CMod_LoadNodes( void ) {
	dnode_t	*in;
	cNode_t	*out;
	int		i, j, count;
	unsigned child, num;

	in = cm_bsp->nodes;
	count = cm_bsp->numNodes;
	if ( count < 1 )
		Com_Terminate( TERM_CLIENT_DROP, "%s: map has no nodes", __func__ );

	cm.nodes = Hunk_Alloc( count * sizeof( *cm.nodes ), h_high );
	cm.numNodes = count;

	out = cm.nodes;

	for ( i = 0; i < count; i++, out++, in++ )
	{
		num = in->planeNum;
		if ( num >= cm.numPlanes )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad planeNum", __func__ );

		out->plane = cm.planes + num;
		for ( j = 0; j < 2; j++ )
		{
			child = in->children[j];
			if ( child & 0x80000000 ) {
				if ( ~child >= cm.numLeafs )
					Com_Terminate( TERM_CLIENT_DROP, "%s: bad leaf", __func__ );
			} else {
				if ( child >= count )
					Com_Terminate( TERM_CLIENT_DROP, "%s: bad node", __func__ );
			}
			out->children[j] = child;
		}
	}

}

/*
=================
CM_BoundBrush

=================
*/
static void CM_BoundBrush( cbrush_t *b ) {
	int i, j;
	// Initialize to universe bounds — brushes without a constraining plane
	// in a given axis direction are treated as infinite in that direction.
	b->bounds[0][0] = b->bounds[0][1] = b->bounds[0][2] = -65536.0f;
	b->bounds[1][0] = b->bounds[1][1] = b->bounds[1][2] =  65536.0f;
	for ( i = 0; i < b->numsides; i++ ) {
		const cplane_t *p = b->sides[i].plane;
		for ( j = 0; j < 3; j++ ) {
			if ( p->normal[j] > 0.999f ) {
				if ( p->dist < b->bounds[1][j] )
					b->bounds[1][j] = p->dist;
			} else if ( p->normal[j] < -0.999f ) {
				if ( -p->dist > b->bounds[0][j] )
					b->bounds[0][j] = -p->dist;
			}
		}
	}
}


/*
=================
CMod_LoadBrushes

=================
*/
static void CMod_LoadBrushes( void ) {
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count, nDropped;
	unsigned	firstSide, numSides;

	in = cm_bsp->brushes;
	count = cm_bsp->numBrushes;

	cm.brushes = Hunk_Alloc( ( BOX_BRUSHES + count ) * sizeof( *cm.brushes ), h_high );
	cm.numBrushes = count;

	out = cm.brushes;
	nDropped = 0;

	for ( i = 0; i < count; i++, out++, in++ ) {
		firstSide = in->firstSide;
		numSides = in->numSides;
		if ( (uint64_t)firstSide + numSides > cm.numBrushSides )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad brushsides", __func__ );

		out->sides = cm.brushsides + firstSide;
		out->numsides = numSides;

		out->shaderNum = in->shaderNum;
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum );
		}
		out->contents = cm.shaders[out->shaderNum].contentFlags;

		CM_BoundBrush( out );

		// Degenerate brushes whose half-space intersection is empty have inverted
		// bounds after CM_BoundBrush.  Stamp sentinel bounds so the AABB pre-check
		// in CM_TestBoxInBrush unconditionally rejects them.
		if ( out->bounds[0][0] > out->bounds[1][0] ||
		     out->bounds[0][1] > out->bounds[1][1] ||
		     out->bounds[0][2] > out->bounds[1][2] ) {
			out->bounds[0][0] = out->bounds[0][1] = out->bounds[0][2] =  99999.0f;
			out->bounds[1][0] = out->bounds[1][1] = out->bounds[1][2] = -99999.0f;
			nDropped++;
		}
	}

	if ( nDropped > 0 )
		Com_Log( SEV_INFO, LOG_CH(ch_loading),
		         "CM_LoadBrushes: dropped %d degenerate brushes (inverted bounds)\n", nDropped );
}


/*
=================
CMod_LoadLeafs
=================
*/
static void CMod_LoadLeafs( void )
{
	unsigned	firstLeafBrush, numLeafBrushes, firstLeafSurface, numLeafSurfaces;
	dleaf_t *in = cm_bsp->leafs;
	int count = cm_bsp->numLeafs;
	if ( count < 1 )
		Com_Terminate( TERM_CLIENT_DROP, "%s: map with no leafs", __func__ );

	cm.leafs = Hunk_Alloc( ( BOX_LEAFS + count ) * sizeof( *cm.leafs ), h_high );
	cm.numLeafs = count;

	cLeaf_t *out = cm.leafs;
	for ( int i = 0; i < count; i++, in++, out++ )
	{
		out->cluster = in->cluster;
		if ( out->cluster + 1U > INT_MAX - 63U )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad cluster", __func__ );

		out->area = in->area;
		if ( out->area + 1U > MAX_MAP_AREAS )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad area", __func__ );

		firstLeafBrush = in->firstLeafBrush;
		numLeafBrushes = in->numLeafBrushes;
		if ( (uint64_t)firstLeafBrush + numLeafBrushes > cm.numLeafBrushes )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad leafbrushes", __func__ );

		out->firstLeafBrush = firstLeafBrush;
		out->numLeafBrushes = numLeafBrushes;

		firstLeafSurface = in->firstLeafSurface;
		numLeafSurfaces = in->numLeafSurfaces;
		if ( (uint64_t)firstLeafSurface + numLeafSurfaces > cm.numLeafSurfaces )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad leafsurfaces", __func__ );

		out->firstLeafSurface = firstLeafSurface;
		out->numLeafSurfaces = numLeafSurfaces;

		if ( out->cluster >= cm.numClusters )
			cm.numClusters = out->cluster + 1;
		if ( out->area >= cm.numAreas )
			cm.numAreas = out->area + 1;
	}

	cm.areas = Hunk_Alloc( cm.numAreas * sizeof( *cm.areas ), h_high );
	cm.areaPortals = Hunk_Alloc( cm.numAreas * cm.numAreas * sizeof( *cm.areaPortals ), h_high );
}


/*
=================
CMod_LoadPlanes
=================
*/
static void CMod_LoadPlanes( void )
{
	dplane_t *in = cm_bsp->planes;
	int count = cm_bsp->numPlanes;
	if ( count < 1 )
		Com_Terminate( TERM_CLIENT_DROP, "%s: map with no planes", __func__ );

	cm.planes = Hunk_Alloc( ( BOX_PLANES + count ) * sizeof( *cm.planes ), h_high );
	cm.numPlanes = count;

	cplane_t *out = cm.planes;

	for ( int i = 0; i < count; i++, in++, out++ )
	{
		int bits = 0;
		for ( int j = 0; j < 3; j++ )
		{
			out->normal[j] = in->normal[j];
			if ( out->normal[j] < 0 )
				bits |= 1<<j;
		}

		out->dist = in->dist;
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}


/*
=================
CMod_LoadLeafBrushes
=================
*/
static void CMod_LoadLeafBrushes( void )
{
	int *in = cm_bsp->leafBrushes;
	int count = cm_bsp->numLeafBrushes;

	cm.leafbrushes = Hunk_Alloc( (count + BOX_BRUSHES) * sizeof( *cm.leafbrushes ), h_high );
	cm.numLeafBrushes = count;

	int *out = cm.leafbrushes;

	for ( int i = 0; i < count; i++, in++, out++ ) {
		unsigned j = *in;
		if ( j >= cm.numBrushes )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad brush", __func__ );
		*out = j;
	}
}


/*
=================
CMod_LoadLeafSurfaces
=================
*/
static void CMod_LoadLeafSurfaces( void )
{
	int *in = cm_bsp->leafSurfaces;
	int count = cm_bsp->numLeafSurfaces;

	cm.leafsurfaces = Hunk_Alloc( count * sizeof( *cm.leafsurfaces ), h_high );
	cm.numLeafSurfaces = count;

	int *out = cm.leafsurfaces;

	for ( int i = 0; i < count; i++, in++, out++ ) {
		unsigned j = *in;
		if ( j >= cm.numSurfaces )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad surface", __func__ );
		*out = j;
	}
}


/*
=================
CMod_LoadBrushSides
=================
*/
static void CMod_LoadBrushSides( void )
{
	dbrushside_t *in = cm_bsp->brushSides;
	int count = cm_bsp->numBrushSides;

	cm.brushsides = Hunk_Alloc( ( BOX_SIDES + count ) * sizeof( *cm.brushsides ), h_high );
	cm.numBrushSides = count;

	cbrushside_t *out = cm.brushsides;

	for ( int i = 0; i < count; i++, in++, out++ ) {
		unsigned num = in->planeNum;
		if ( num >= cm.numPlanes ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad planeNum", __func__ );
		}
		out->plane = &cm.planes[num];
		out->shaderNum = in->shaderNum;
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum );
		}
		out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;
	}
}


/*
=================
CMod_LoadEntityString

Load the entity string. Prefers an external override file at
maps/<mapname>.ent (plain-text Q3 entity grammar) over the BSP's
embedded entity lump. The .ent override is server-side only; clients
never read it. There is no checksum or hot-reload — the file is read
once per map load, exactly like the BSP lump.
=================
*/
static void CMod_LoadEntityString( void ) {
	char         path[MAX_QPATH];
	fileHandle_t f;
	int          extLen;

	Com_sprintf( path, sizeof( path ), "maps/%s.ent", cm.name );
	extLen = FS_FOpenFileRead( path, &f, qfalse );

	if ( f != FS_INVALID_HANDLE && extLen > 0 ) {
		cm.entityString = Hunk_Alloc( extLen + 1, h_high );
		FS_Read( cm.entityString, extLen, f );
		cm.entityString[extLen] = '\0';
		cm.numEntityChars = extLen;
		FS_FCloseFile( f );
		Com_Log( SEV_DEBUG, LOG_CH(ch_loading),
			"Loaded external entities from %s (%d bytes)\n", path, extLen );
		return;
	}
	if ( f != FS_INVALID_HANDLE ) {
		FS_FCloseFile( f );
	}

	cm.entityString = Hunk_Alloc( cm_bsp->entityStringLength + 1, h_high );
	cm.numEntityChars = cm_bsp->entityStringLength;
	memcpy( cm.entityString, cm_bsp->entityString, cm_bsp->entityStringLength );
	cm.entityString[cm_bsp->entityStringLength] = 0;
}


/*
=================
CMod_LoadVisibility
=================
*/
static void CMod_LoadVisibility( void ) {
	unsigned numClusters, clusterBytes, len;

	len = PAD( cm.numClusters, 64 ) >> 3;
	cm.novis = Hunk_Alloc( len, h_high );
	memset( cm.novis, 0xff, len );

	len = cm_bsp->visibilityLength;
	if ( !len ) {
		return;
	}

	numClusters = cm_bsp->numClusters;
	clusterBytes = cm_bsp->clusterBytes;

	if ( (uint64_t)numClusters * clusterBytes > len ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: lump too short", __func__ );
	}
	if ( numClusters < cm.numClusters ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad numClusters", __func__ );
	}
	if ( clusterBytes < (numClusters + 7) >> 3 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad clusterBytes", __func__ );
	}

	cm.visibility = Hunk_Alloc( len, h_high );
	cm.numClusters = numClusters;
	cm.clusterBytes = clusterBytes;
	memcpy( cm.visibility, cm_bsp->visibility, len );
}

//==================================================================

/*
=================
CMod_LoadPatches
=================
*/
#define	MAX_PATCH_VERTS		1024
static void CMod_LoadPatches( void ) {
	dsurface_t *in = cm_bsp->surfaces;
	int count = cm_bsp->numSurfaces;
	vec3_t points[MAX_PATCH_VERTS];

	cm.numSurfaces = count;
	// cm.surfaces is `cPatch_t **` (array of pointers, one slot per surface,
	// NULL for non-patch surfaces). Using `sizeof(cPatch_t *)` rather than
	// `sizeof(cm.surfaces[0])` documents that we're reserving room for
	// pointers (not for cPatch_t structs) and dodges clang-tidy's
	// bugprone-sizeof-expression "sizeof(A*); pointer to aggregate" check.
	cm.surfaces = Hunk_Alloc( cm.numSurfaces * sizeof( cPatch_t * ), h_high );

	drawVert_t *dv = cm_bsp->drawVerts;
	unsigned totalVerts = cm_bsp->numDrawVerts;

	// scan through all the surfaces, but only load patches,
	// not planar faces
	for ( int i = 0 ; i < count ; i++, in++ ) {
		if ( in->surfaceType != MST_PATCH ) {
			continue;		// ignore other surfaces
		}
		// FIXME: check for non-colliding patches

		cPatch_t *patch = Hunk_Alloc( sizeof( *patch ), h_high );
		cm.surfaces[ i ] = patch;

		// load the full drawverts onto the stack
		unsigned width = in->patchWidth;
		unsigned height = in->patchHeight;
		if ( (uint64_t)width * height > MAX_PATCH_VERTS ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: MAX_PATCH_VERTS", __func__ );
		}

		unsigned firstVert = in->firstVert;
		unsigned numVerts = width * height;
		if ( (uint64_t)firstVert + numVerts > totalVerts ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad firstVert", __func__ );
		}

		drawVert_t *dv_p = dv + firstVert;
		for ( int j = 0 ; j < numVerts ; j++, dv_p++ ) {
			points[j][0] = dv_p->xyz[0];
			points[j][1] = dv_p->xyz[1];
			points[j][2] = dv_p->xyz[2];
		}

		unsigned shaderNum = in->shaderNum;
		if ( shaderNum >= cm.numShaders ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad shaderNum", __func__ );
		}
		patch->contents = cm.shaders[shaderNum].contentFlags;
		patch->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;

		// create the internal facet structure
		patch->pc = CM_GeneratePatchCollide( width, height, points );
	}
}

//==================================================================


#if 0
static uint32_t CM_LumpChecksum( const lump_t *lump ) {
	return LittleLong( Com_BlockChecksum( cmod_base + lump->fileofs, lump->filelen ) );
}


static uint32_t CM_Checksum( const dheader_t *header ) {
	uint32_t checksums[11];

	checksums[0] = CM_LumpChecksum( &header->lumps[LUMP_SHADERS] );
	checksums[1] = CM_LumpChecksum( &header->lumps[LUMP_LEAFS] );
	checksums[2] = CM_LumpChecksum( &header->lumps[LUMP_LEAFBRUSHES] );
	checksums[3] = CM_LumpChecksum( &header->lumps[LUMP_LEAFSURFACES] );
	checksums[4] = CM_LumpChecksum( &header->lumps[LUMP_PLANES] );
	checksums[5] = CM_LumpChecksum( &header->lumps[LUMP_BRUSHSIDES] );
	checksums[6] = CM_LumpChecksum( &header->lumps[LUMP_BRUSHES] );
	checksums[7] = CM_LumpChecksum( &header->lumps[LUMP_MODELS] );
	checksums[8] = CM_LumpChecksum( &header->lumps[LUMP_NODES] );
	checksums[9] = CM_LumpChecksum( &header->lumps[LUMP_SURFACES] );
	checksums[10] = CM_LumpChecksum( &header->lumps[LUMP_DRAWVERTS] );

	return LittleLong( Com_BlockChecksum( checksums, ARRAY_LEN( checksums ) * 4 ) );
}
#endif

static void CM_ValidateTree_r( byte *visited, int node ) {
	while ( node >= 0 ) {
		if ( visited[node] )
			Com_Terminate( TERM_CLIENT_DROP, "%s: cycle encountered", __func__ );
		visited[node] = 1;
		CM_ValidateTree_r( visited, cm.nodes[node].children[0] );
		node = cm.nodes[node].children[1];
	}
}

static void CM_ValidateTree( void ) {
	byte *visited = Hunk_AllocateTempMemory( cm.numNodes );
	memset( visited, 0, cm.numNodes );
	CM_ValidateTree_r( visited, 0 );
	Hunk_FreeTempMemory( visited );
}


/*
===============================================================================

RUNTIME TRIANGLE SOUP REGISTRATION

Used by IQM models (and any other caller holding a raw triangle mesh)
to register collision geometry with cm_trace. Handles returned here can
be passed directly to CM_BoxTrace / CM_TransformedBoxTrace.

===============================================================================
*/

/*
====================
CM_ClearTriangleSoups

Reset the runtime triangle-soup table. The patchCollide_t payloads live
in the hunk (h_high), so they are automatically released on the next
CM_ClearMap / Hunk_ClearLevel cycle.
====================
*/
void CM_ClearTriangleSoups( void ) {
	memset( cm.triSoups, 0, sizeof( cm.triSoups ) );
	cm.numTriSoups = 0;
}

/*
====================
CM_FindTriSoupByName

Return an existing tri-soup handle if one is already registered for
`name`, otherwise 0 (invalid).
====================
*/
static clipHandle_t CM_FindTriSoupByName( const char *name ) {
	for ( int i = 0; i < MAX_TRI_SOUPS; i++ ) {
		cTriSoup_t *ts = &cm.triSoups[i];
		if ( !ts->inUse ) {
			continue;
		}
		if ( !Q_stricmp( ts->name, name ) ) {
			return (clipHandle_t)( TRI_SOUP_HANDLE_BASE + i );
		}
	}
	return 0;
}

/*
====================
CM_RegisterTriangleSoup

Generate a patchCollide_t from the given triangle mesh and insert it
into the runtime triangle-soup table. The returned handle is valid
until CM_ClearMap is called (collision data lives in the h_high hunk).
Returns 0 on failure.
====================
*/
clipHandle_t CM_RegisterTriangleSoup( const char *name, const vec3_t *vertexes,
	int numVertexes, const int *indexes, int numIndexes ) {
	if ( !name || !name[0] ) {
		return 0;
	}
	if ( !vertexes || numVertexes <= 0 || !indexes || numIndexes <= 0 ) {
		return 0;
	}
	if ( numIndexes % 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_collision), "CM_RegisterTriangleSoup: %s numIndexes %i not a multiple of 3\n",
			name, numIndexes );
		return 0;
	}

	// Reuse existing slot if already registered.
	clipHandle_t handle = CM_FindTriSoupByName( name );
	if ( handle ) {
		return handle;
	}

	// Find a free slot.
	int slot = -1;
	for ( int i = 0; i < MAX_TRI_SOUPS; i++ ) {
		if ( !cm.triSoups[i].inUse ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_collision), "CM_RegisterTriangleSoup: %s exceeds MAX_TRI_SOUPS (%i)\n",
			name, MAX_TRI_SOUPS );
		return 0;
	}

	// CM_GenerateTriangleSoupCollide takes non-const pointers (it never
	// modifies them, but the signature is historical). Duplicate onto
	// temporary hunk memory to honor the const contract at our API.
	vec3_t *mutableVerts = Hunk_AllocateTempMemory( numVertexes * sizeof( vec3_t ) );
	int *mutableIndexes = Hunk_AllocateTempMemory( numIndexes * sizeof( int ) );
	memcpy( mutableVerts, vertexes, numVertexes * sizeof( vec3_t ) );
	memcpy( mutableIndexes, indexes, numIndexes * sizeof( int ) );

	struct patchCollide_s *pc = CM_GenerateTriangleSoupCollide( numVertexes, mutableVerts,
		numIndexes, mutableIndexes );

	Hunk_FreeTempMemory( mutableIndexes );
	Hunk_FreeTempMemory( mutableVerts );

	if ( !pc ) {
		return 0;
	}

	cTriSoup_t *ts = &cm.triSoups[slot];
	memset( ts, 0, sizeof( *ts ) );
	ts->inUse = qtrue;
	Q_strncpyz( ts->name, name, sizeof( ts->name ) );
	ts->pc = pc;

	// Populate bounds on the embedded cmodel_t so CM_ModelBounds works.
	for ( int j = 0; j < 3; j++ ) {
		ts->cmod.mins[j] = pc->bounds[0][j];
		ts->cmod.maxs[j] = pc->bounds[1][j];
	}

	if ( slot >= cm.numTriSoups ) {
		cm.numTriSoups = slot + 1;
	}

	return (clipHandle_t)( TRI_SOUP_HANDLE_BASE + slot );
}

/*
====================
CM_IqmReadUShort / CM_IqmReadFloat3

Helpers for reading little-endian IQM vertex streams into native
values. IQM files are always little-endian on disk.
====================
*/
static uint16_t CM_IqmReadUShort( const byte *p ) {
	return (uint16_t)( p[0] | ( p[1] << 8 ) );
}

static void CM_IqmReadFloat3( const byte *p, vec3_t out ) {
	union {
		uint32_t u;
		float f;
	} v;
	for ( int i = 0; i < 3; i++ ) {
		v.u = (uint32_t)p[0] | ( (uint32_t)p[1] << 8 )
			| ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 );
		out[i] = v.f;
		p += 4;
	}
}

/*
====================
CM_LoadIQMGeometry

Read an IQM file from the VFS and register its position vertex stream
and triangle indexes as a triangle-soup collision handle. Does not
touch the renderer — this is a standalone header walker so that
collision can be wired up independently of any GPU upload path.

Returns 0 on failure (file missing, wrong magic, missing position
stream, missing triangles, etc.). Prints a diagnostic to the console
on Developer builds.
====================
*/
clipHandle_t CM_LoadIQMGeometry( const char *name ) {
#define IQM_MAGIC_STRING	"INTERQUAKEMODEL"
#define IQM_POSITION_ATTR	0
#define IQM_FLOAT_FORMAT	7

	if ( !name || !name[0] ) {
		return 0;
	}

	clipHandle_t handle = CM_FindTriSoupByName( name );
	if ( handle ) {
		return handle;
	}

	union {
		const byte	*b;
		void		*v;
	} buf;
	int fileSize = FS_ReadFile( name, &buf.v );
	if ( !buf.v || fileSize <= 0 ) {
		return 0;
	}
	const byte *base = buf.b;

	if ( fileSize < 16 + 27 * 4 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "CM_LoadIQMGeometry: %s truncated header\n", name );
		FS_FreeFile( buf.v );
		return 0;
	}
	// strncmp with the 16-byte magic comparison is bounded by the fileSize check above;
	// IQM_MAGIC_STRING is exactly 16 bytes (no NUL needed in the source).
	// NOLINTNEXTLINE(bugprone-suspicious-string-compare,bugprone-not-null-terminated-result)
	if ( strncmp( (const char *)base, IQM_MAGIC_STRING, 16 ) != 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "CM_LoadIQMGeometry: %s wrong magic\n", name );
		FS_FreeFile( buf.v );
		return 0;
	}

	uint32_t		version, filesize;
	uint32_t		num_vertexes, num_vertexarrays, ofs_vertexarrays;
	uint32_t		num_triangles, ofs_triangles;
	{
		const byte *h = base + 16;
		version = LittleLong( *(const uint32_t *)( h +  0 ) );
		filesize = LittleLong( *(const uint32_t *)( h +  4 ) );
		/* flags */
		/* num_text, ofs_text */
		/* num_meshes, ofs_meshes */
		num_vertexarrays = LittleLong( *(const uint32_t *)( h + 32 ) );
		num_vertexes     = LittleLong( *(const uint32_t *)( h + 36 ) );
		ofs_vertexarrays = LittleLong( *(const uint32_t *)( h + 40 ) );
		num_triangles    = LittleLong( *(const uint32_t *)( h + 44 ) );
		ofs_triangles    = LittleLong( *(const uint32_t *)( h + 48 ) );
	}
	(void)version;
	if ( filesize > (uint32_t)fileSize ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "CM_LoadIQMGeometry: %s filesize mismatch\n", name );
		FS_FreeFile( buf.v );
		return 0;
	}

	if ( num_vertexes == 0 || num_triangles == 0 ) {
		FS_FreeFile( buf.v );
		return 0;
	}

	// Locate the position vertex array (type == 0, format == float, size == 3).
	qboolean posFound = qfalse;
	uint32_t posOffset = 0;
	{
		const byte *va = base + ofs_vertexarrays;
		for ( int i = 0; i < (int)num_vertexarrays; i++ ) {
			uint32_t type, format, size, offset;
			if ( (uint64_t)ofs_vertexarrays + (uint64_t)( i + 1 ) * 20 > filesize ) {
				break;
			}
			type   = LittleLong( *(const uint32_t *)( va +  0 ) );
			/* flags */
			format = LittleLong( *(const uint32_t *)( va +  8 ) );
			size   = LittleLong( *(const uint32_t *)( va + 12 ) );
			offset = LittleLong( *(const uint32_t *)( va + 16 ) );
			va += 20;
			if ( type == IQM_POSITION_ATTR && format == IQM_FLOAT_FORMAT && size == 3 ) {
				posOffset = offset;
				posFound = qtrue;
				break;
			}
		}
	}
	if ( !posFound ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "CM_LoadIQMGeometry: %s no position vertex array\n", name );
		FS_FreeFile( buf.v );
		return 0;
	}
	if ( (uint64_t)posOffset + (uint64_t)num_vertexes * 12 > filesize ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "CM_LoadIQMGeometry: %s position array out of range\n", name );
		FS_FreeFile( buf.v );
		return 0;
	}
	if ( (uint64_t)ofs_triangles + (uint64_t)num_triangles * 12 > filesize ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "CM_LoadIQMGeometry: %s triangle array out of range\n", name );
		FS_FreeFile( buf.v );
		return 0;
	}

	// Copy positions to a temporary vec3_t array.
	vec3_t *verts = Hunk_AllocateTempMemory( num_vertexes * sizeof( vec3_t ) );
	{
		const byte *p = base + posOffset;
		for ( int i = 0; i < (int)num_vertexes; i++, p += 12 ) {
			CM_IqmReadFloat3( p, verts[i] );
		}
	}

	// Copy triangle indexes to a temporary int array.
	int *tris = Hunk_AllocateTempMemory( num_triangles * 3 * sizeof( int ) );
	{
		const byte *p = base + ofs_triangles;
		for ( int i = 0; i < (int)num_triangles; i++, p += 12 ) {
			tris[i * 3 + 0] = (int)LittleLong( *(const uint32_t *)( p + 0 ) );
			tris[i * 3 + 1] = (int)LittleLong( *(const uint32_t *)( p + 4 ) );
			tris[i * 3 + 2] = (int)LittleLong( *(const uint32_t *)( p + 8 ) );
		}
	}

	FS_FreeFile( buf.v );

	handle = CM_RegisterTriangleSoup( name, (const vec3_t *)verts,
		(int)num_vertexes, tris, (int)num_triangles * 3 );

	Hunk_FreeTempMemory( tris );
	Hunk_FreeTempMemory( verts );

	return handle;

#undef IQM_MAGIC_STRING
#undef IQM_POSITION_ATTR
#undef IQM_FLOAT_FORMAT
}


/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) {
	if ( !name || !name[0] ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: NULL name", __func__ );
	}

#ifndef BSPC
	{
		static const cvarDesc_t cmDescs[] = {
			/* 0 */ CVAR_BOOL( "cm_noAreas",         "0", CVAR_CHEAT,                   "Do not use areaportals, all areas are connected." ),
			/* 1 */ CVAR_BOOL( "cm_noCurves",        "0", CVAR_CHEAT,                   "Do not collide against curves." ),
			/* 2 */ CVAR_BOOL( "cm_playerCurveClip", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_CHEAT, "Collide player against curves." ),
		};
		enum { CM_NO_AREAS, CM_NO_CURVES, CM_PLAYER_CURVE_CLIP, CM_CVAR_COUNT };
		_Static_assert( ARRAY_LEN( cmDescs ) == CM_CVAR_COUNT, "cmDescs/enum mismatch" );
		static cvar_t *cmHandles[CM_CVAR_COUNT];
		Cvar_RegisterTable( cmDescs, ARRAY_LEN( cmDescs ), cmHandles );
		cm_noAreas         = cmHandles[CM_NO_AREAS];
		cm_noCurves        = cmHandles[CM_NO_CURVES];
		cm_playerCurveClip = cmHandles[CM_PLAYER_CURVE_CLIP];
	}
#endif

	Com_Log( SEV_DEBUG, LOG_CH(ch_collision), "%s( '%s', %i )\n", __func__, name, clientload );

	if ( !strcmp( cm.name, name ) && clientload ) {
		*checksum = cm.checksum;
		return;
	}

	// free old stuff
	CM_ClearMap();

#if 0
	if ( !name[0] ) {
		cm.numLeafs = 1;
		cm.numClusters = 1;
		cm.numAreas = 1;
		cm.cmodels = Hunk_Alloc( sizeof( *cm.cmodels ), h_high );
		*checksum = 0;
		return;
	}
#endif

	if ( !BSP_Load( name, &cm_bsp, BSP_LOAD_FLAGS_NONE ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: couldn't load %s", __func__, name );
	}

	*checksum = cm.checksum = cm_bsp->checksum;

	int bspVersion = cm_bsp->version;
	cm.tracer = ( bspVersion == BSP_VERSION_Q1 ) ? &cmTracer_q1 : &cmTracer_q3;

	if ( BSP_AssetProfileForVersion( bspVersion ) == BSP_ASSET_PROFILE_LEGACY ) {
		Cvar_Set( "com_mapAssetProfile", "legacy" );
	} else {
		Cvar_Set( "com_mapAssetProfile", "modern" );
	}
	Cvar_Set( "com_mapBspVersion", va( "%d", bspVersion ) );

	// pre-calculate some stuff
	cm.numBrushes = cm_bsp->numBrushes;
	cm.numSurfaces = cm_bsp->numSurfaces;

	// load into heap
	CMod_LoadShaders();
	CMod_LoadLeafBrushes();
	CMod_LoadLeafSurfaces();
	CMod_LoadPlanes();
	CMod_LoadBrushSides();
	CMod_LoadBrushes();
	CMod_LoadSubmodels();
	CMod_LoadLeafs();
	CMod_LoadNodes();
	CMod_LoadEntityString();
	CMod_LoadVisibility();
	CMod_LoadPatches();

	BSP_Free( cm_bsp );
	cm_bsp = NULL;

	// check for cycles so we don't overflow stack
	CM_ValidateTree();

	CM_InitBoxHull();

	CM_FloodAreaConnections();

#ifndef BSPC
#ifdef _DEBUG
	CM_TriangleSoupCollideSelfTest();
#endif
#endif

	// allow this to be cached if it is loaded by the server
	if ( !clientload ) {
		Q_strncpyz( cm.name, name, sizeof( cm.name ) );
	}
}


/*
==================
CM_ClearMap
==================
*/
void CM_ClearMap( void ) {
	// Release any stale BSP reference left by a crash-interrupted CM_LoadMap.
	// Normally CM_LoadMap calls BSP_Free(cm_bsp) before returning; if a longjmp
	// (Com_Error) fires between BSP_Load and that BSP_Free, cm_bsp retains a
	// reference that would otherwise keep the BSP slot occupied indefinitely.
	if ( cm_bsp ) {
		BSP_Free( cm_bsp );
		cm_bsp = NULL;
	}

	CMQ1_FreeData();
	memset( &cm, 0, sizeof( cm ) );
	CM_ClearLevelPatches();
	CM_ClearTriangleSoups();

	Cvar_Set( "com_mapAssetProfile", "modern" );
	Cvar_Set( "com_mapBspVersion", "0" );
}


/*
==================
CM_ClipHandleToModel
==================
*/
cmodel_t *CM_ClipHandleToModel( clipHandle_t handle ) {
	if ( handle < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CM_ClipHandleToModel: bad handle %i", handle );
	}
	if ( handle < cm.numSubModels ) {
		return &cm.cmodels[handle];
	}
	if ( handle == BOX_MODEL_HANDLE ) {
		return &box_model;
	}
	if ( handle >= TRI_SOUP_HANDLE_BASE && handle < TRI_SOUP_HANDLE_END ) {
		int idx = handle - TRI_SOUP_HANDLE_BASE;
		if ( !cm.triSoups[idx].inUse ) {
			Com_Terminate( TERM_CLIENT_DROP, "CM_ClipHandleToModel: stale triangle-soup handle %i", handle );
		}
		return &cm.triSoups[idx].cmod;
	}
	if ( handle < MAX_SUBMODELS ) {
		Com_Terminate( TERM_CLIENT_DROP, "CM_ClipHandleToModel: bad handle %i < %i < %i",
			cm.numSubModels, handle, MAX_SUBMODELS );
	}
	Com_Terminate( TERM_CLIENT_DROP, "CM_ClipHandleToModel: bad handle %i", handle + MAX_SUBMODELS );

	return NULL;
}


/*
==================
CM_InlineModel
==================
*/
clipHandle_t CM_InlineModel( int index ) {
	if ( index < 0 || index >= cm.numSubModels ) {
		Com_Terminate( TERM_CLIENT_DROP, "CM_InlineModel: bad number");
	}
	return index;
}


int CM_NumClusters( void ) {
	return cm.numClusters;
}


int CM_NumInlineModels( void ) {
	return cm.numSubModels;
}


char *CM_EntityString( void ) {
	return cm.entityString;
}


int CM_LeafCluster( int leafnum ) {
	if ( leafnum < 0 || leafnum >= cm.numLeafs ) {
		Com_Terminate( TERM_CLIENT_DROP, "CM_LeafCluster: bad number" );
	}
	return cm.leafs[leafnum].cluster;
}


int CM_LeafArea( int leafnum ) {
	if ( leafnum < 0 || leafnum >= cm.numLeafs ) {
		Com_Terminate( TERM_CLIENT_DROP, "CM_LeafArea: bad number" );
	}
	return cm.leafs[leafnum].area;
}


int CM_NumBrushes( void ) {
	return cm.numBrushes;
}


void CM_GetBrushData( int idx, int *contents, int *shaderNum, const char **shaderName,
					  float mins[3], float maxs[3], int *numsides ) {
	const cbrush_t *b;
	if ( idx < 0 || idx >= cm.numBrushes )
		Com_Terminate( TERM_CLIENT_DROP, "CM_GetBrushData: bad index %d", idx );
	b = &cm.brushes[idx];
	*contents  = b->contents;
	*shaderNum = b->shaderNum;
	*shaderName = ( b->shaderNum >= 0 && b->shaderNum < cm.numShaders )
				  ? cm.shaders[b->shaderNum].shader : "";
	mins[0] = b->bounds[0][0]; mins[1] = b->bounds[0][1]; mins[2] = b->bounds[0][2];
	maxs[0] = b->bounds[1][0]; maxs[1] = b->bounds[1][1]; maxs[2] = b->bounds[1][2];
	*numsides = b->numsides;
}


void CM_GetBrushSideData( int brushIdx, int sideIdx, int *planeNum, float normal[3],
						   float *dist, int *shaderNum, const char **shaderName ) {
	const cbrush_t     *b;
	const cbrushside_t *s;
	if ( brushIdx < 0 || brushIdx >= cm.numBrushes )
		Com_Terminate( TERM_CLIENT_DROP, "CM_GetBrushSideData: bad brush index %d", brushIdx );
	b = &cm.brushes[brushIdx];
	if ( sideIdx < 0 || sideIdx >= b->numsides )
		Com_Terminate( TERM_CLIENT_DROP, "CM_GetBrushSideData: bad side index %d", sideIdx );
	s = &b->sides[sideIdx];
	*planeNum  = (int)( s->plane - cm.planes );
	normal[0]  = s->plane->normal[0];
	normal[1]  = s->plane->normal[1];
	normal[2]  = s->plane->normal[2];
	*dist      = s->plane->dist;
	*shaderNum = s->shaderNum;
	*shaderName = ( s->shaderNum >= 0 && s->shaderNum < cm.numShaders )
				  ? cm.shaders[s->shaderNum].shader : "";
}

//=======================================================================


/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
static void CM_InitBoxHull( void )
{
	box_planes = &cm.planes[cm.numPlanes];

	box_brush = &cm.brushes[cm.numBrushes];
	box_brush->numsides = 6;
	box_brush->sides = cm.brushsides + cm.numBrushSides;
	box_brush->contents = CONTENTS_BODY;

	box_model.leaf.numLeafBrushes = 1;
//	box_model.leaf.firstLeafBrush = cm.numBrushes;
	box_model.leaf.firstLeafBrush = cm.numLeafBrushes;
	cm.leafbrushes[cm.numLeafBrushes] = cm.numBrushes;

	for ( int i = 0; i < 6; i++ )
	{
		int side = i & 1;

		// brush sides
		cbrushside_t *s = &cm.brushsides[cm.numBrushSides + i];
		s->plane = cm.planes + ( cm.numPlanes + i * 2 + side );
		s->surfaceFlags = 0;

		// planes
		cplane_t *p = &box_planes[i * 2];
		p->type = i >> 1;
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i >> 1] = 1;

		p = &box_planes[i * 2 + 1];
		p->type = 3 + ( i >> 1 );
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i >> 1] = -1;

		SetPlaneSignbits( p );
	}
}


/*
===================
CM_TempBoxModel

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
Capsules are handled differently though.
===================
*/
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule ) {

	VectorCopy( mins, box_model.mins );
	VectorCopy( maxs, box_model.maxs );

	if ( capsule ) {
		return CAPSULE_MODEL_HANDLE;
	}

	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	VectorCopy( mins, box_brush->bounds[0] );
	VectorCopy( maxs, box_brush->bounds[1] );

	return BOX_MODEL_HANDLE;
}


/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	cmodel_t *cmod;

	cmod = CM_ClipHandleToModel( model );
	VectorCopy( cmod->mins, mins );
	VectorCopy( cmod->maxs, maxs );
}
