/*
===========================================================================
Copyright (C) 1996-1997 Id Software, Inc.
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

// bsp_q1.c -- Quake 1 BSP29 format loader
//
// Phase 1: header parse, lump-bounds validation, bspFile_t allocation,
//          entity-string copy, per-lump diagnostic log.
// Phase 2: planes, shaders (texinfo+miptex), surfaces/drawVerts/
//          drawIndexes (edge-indirect → triangle fan), nodes, leafs,
//          marksurfaces, submodels, embeddedTextures (raw).
// Phase 3 (this): lightmap atlas packing (4-style shelf packer),
//          per-vertex lightmap UV, vis NULL + leaf cluster=-1,
//          synthetic Q3 rawData so RE_LoadWorldMap works unchanged.
// Phase 4 will add: clipnode → brush/brushside conversion.
// Phase 5 will add: miptex expansion + synthetic shader binding.

#include "../q_shared.h"
#include "../qcommon.h"
#include "bsp.h"
#include "../surfaceflags.h"
#include "../cm_local.h"   /* for cm.q1.leafContents in post-dup extension */
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_loading, "loading" );

#if FEAT_RECAST_NAVMESH
#include "../nav/nav_local.h"
#include "../nav/nav_coord.h"
#endif

static void *BSP_Q1_ZAlloc( size_t size ) {
	void *ptr;
	if ( size == 0 ) return NULL;
	ptr = Z_Malloc( size );
	if ( !ptr ) {
		Com_Terminate( TERM_UNRECOVERABLE, "BSP_Q1_Load: Z_Malloc(%u) failed", (unsigned)size );
	}
	return ptr;
}

// Q1 leaf contents are negative enums; translate to Q3 content-flag bitmasks.
static int BSP_Q1_ContentsToQ3( int q1contents ) {
	switch ( q1contents ) {
	case -1: return 0;               // CONTENTS_EMPTY
	case -2: return CONTENTS_SOLID;
	case -3: return CONTENTS_WATER;
	case -4: return CONTENTS_SLIME;
	case -5: return CONTENTS_LAVA;
	case -6:  // CONTENTS_SKY — treat as solid for collision
	default:  return CONTENTS_SOLID;
	}
}

static qboolean BSP_Q1_Detect( const void *buf, int len ) {
	if ( len < 4 ) return qfalse;
	return LittleLong( *(const int *)buf ) == BSP_VERSION_Q1;
}

// ---------------------------------------------------------------------------
// Lightmap shelf packer
// ---------------------------------------------------------------------------

#define LM_PAGE_W     128
#define LM_PAGE_H     128
#define LM_COMPS      3
#define LM_PAGE_BYTES ( LM_PAGE_W * LM_PAGE_H * LM_COMPS )
#define LM_MAX_PAGES  512

typedef struct {
	byte  *data[4];   // style slot 0..3 staging buffers, each numAlloced * LM_PAGE_BYTES
	int    numAlloced; // allocated page count in data[]
	int    numUsed;    // pages actually containing packed patches
	int    shelfX;
	int    shelfY;
	int    shelfH;
} LightmapPacker_t;

static void LM_Init( LightmapPacker_t *pk ) {
	int s;
	memset( pk, 0, sizeof( *pk ) );
	pk->numAlloced = 4;
	pk->numUsed    = 1;
	for ( s = 0; s < 4; s++ ) {
		pk->data[s] = BSP_Q1_ZAlloc( (size_t)pk->numAlloced * LM_PAGE_BYTES );
	}
}

static void LM_Grow( LightmapPacker_t *pk ) {
	int s;
	int newN = pk->numAlloced * 2;
	if ( newN > LM_MAX_PAGES ) {
		Com_Terminate( TERM_UNRECOVERABLE, "BSP_Q1_Load: lightmap atlas overflow (>%d pages)", LM_MAX_PAGES );
	}
	for ( s = 0; s < 4; s++ ) {
		byte *nb = BSP_Q1_ZAlloc( (size_t)newN * LM_PAGE_BYTES );
		memcpy( nb, pk->data[s], (size_t)pk->numAlloced * LM_PAGE_BYTES );
		Z_Free( pk->data[s] );
		pk->data[s] = nb;
	}
	pk->numAlloced = newN;
}

static void LM_Alloc( LightmapPacker_t *pk, int w, int h,
                      int *outPage, int *outX, int *outY ) {
	// Reserve (w+2) x (h+2) per patch — 1-texel border on each side prevents
	// GL_LINEAR bilinear sampling from bleeding into adjacent patches.
	// *outX / *outY are set to the INTERIOR origin (+1 from raw shelf position)
	// so callers (LM_FillPatch and UV computation) need no adjustment.
	int slotW = w + 2;
	int slotH = h + 2;
	if ( pk->shelfX + slotW > LM_PAGE_W ) {
		pk->shelfY += pk->shelfH;
		pk->shelfX  = 0;
		pk->shelfH  = 0;
	}
	if ( pk->shelfY + slotH > LM_PAGE_H ) {
		pk->numUsed++;
		if ( pk->numUsed > pk->numAlloced ) {
			LM_Grow( pk );
		}
		pk->shelfX = 0;
		pk->shelfY = 0;
		pk->shelfH = 0;
	}
	*outPage = pk->numUsed - 1;
	// Interior origin: +1 from raw shelf to step over the top/left border row.
	*outX    = pk->shelfX + 1;
	*outY    = pk->shelfY + 1;
	pk->shelfX += slotW;
	if ( slotH > pk->shelfH ) pk->shelfH = slotH;
}

// Write a grayscale Q1 lightmap patch into the RGB atlas for all 4 style slots.
// Q1 lighting: one grayscale byte per texel per style slot (style!=255),
// stored consecutively at base[lightofs + s * w * h].
// patchCount is the number of non-255 style slots preceding slot styleSlot.
//
// atlasX/atlasY are interior coordinates (already +1 from LM_Alloc border offset).
// After writing the interior w*h texels, a 1-texel border is replicated outward
// (rows atlasY-1 and atlasY+h, columns atlasX-1 and atlasX+w) from the edge of
// the interior. This prevents GL_LINEAR bilinear sampling from bleeding adjacent
// patches' lighting values across face boundaries.
static void LM_FillPatch( LightmapPacker_t *pk,
                          int page, int atlasX, int atlasY,
                          int w, int h,
                          const byte *lightBase, int lightLen,
                          int lightofs,
                          const byte styles[4],
                          const byte *litBase ) {
	int s, x, y;

	for ( s = 0; s < 4; s++ ) {
		byte *dst = pk->data[s] + (size_t)page * LM_PAGE_BYTES;

		if ( lightofs < 0 || styles[s] == 255 ) {
			// No light data — fill black (already zeroed from Z_Malloc)
			continue;
		}

		// How many non-255 style slots precede slot s?
		int slotOffset = 0;
		int prior;
		for ( prior = 0; prior < s; prior++ ) {
			if ( styles[prior] != 255 ) slotOffset++;
		}
		int srcOffset = lightofs + slotOffset * w * h;
		if ( srcOffset + w * h > lightLen ) {
			continue;  // out of bounds — leave black
		}
		const byte *src = lightBase + srcOffset;

		// --- Interior write ---
		for ( y = 0; y < h; y++ ) {
			for ( x = 0; x < w; x++ ) {
				byte *p  = dst + ( (atlasY + y) * LM_PAGE_W + (atlasX + x) ) * LM_COMPS;
				if ( litBase ) {
					int ti = srcOffset + y * w + x;
					p[0]   = litBase[ti * 3 + 0];
					p[1]   = litBase[ti * 3 + 1];
					p[2]   = litBase[ti * 3 + 2];
				} else {
					byte lum = src[y * w + x];
					p[0] = lum;
					p[1] = lum;
					p[2] = lum;
				}
			}
		}

		// --- Border replication (prevents bilinear bleed at patch edges) ---
		// atlasX >= 1 and atlasY >= 1 are guaranteed by LM_Alloc (+1 interior offset).

		// Top border row (atlasY-1): replicate first interior row
		for ( x = 0; x < w; x++ ) {
			const byte *src_p = dst + ( atlasY       * LM_PAGE_W + (atlasX + x) ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY - 1) * LM_PAGE_W + (atlasX + x) ) * LM_COMPS;
			dst_p[0] = src_p[0];
			dst_p[1] = src_p[1];
			dst_p[2] = src_p[2];
		}

		// Bottom border row (atlasY+h): replicate last interior row
		for ( x = 0; x < w; x++ ) {
			const byte *src_p = dst + ( (atlasY + h - 1) * LM_PAGE_W + (atlasX + x) ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY + h)     * LM_PAGE_W + (atlasX + x) ) * LM_COMPS;
			dst_p[0] = src_p[0];
			dst_p[1] = src_p[1];
			dst_p[2] = src_p[2];
		}

		// Left border column (atlasX-1): replicate first interior column
		for ( y = 0; y < h; y++ ) {
			const byte *src_p = dst + ( (atlasY + y) * LM_PAGE_W + atlasX       ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY + y) * LM_PAGE_W + (atlasX - 1) ) * LM_COMPS;
			dst_p[0] = src_p[0];
			dst_p[1] = src_p[1];
			dst_p[2] = src_p[2];
		}

		// Right border column (atlasX+w): replicate last interior column
		for ( y = 0; y < h; y++ ) {
			const byte *src_p = dst + ( (atlasY + y) * LM_PAGE_W + (atlasX + w - 1) ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY + y) * LM_PAGE_W + (atlasX + w)     ) * LM_COMPS;
			dst_p[0] = src_p[0];
			dst_p[1] = src_p[1];
			dst_p[2] = src_p[2];
		}

		// 4 corners
		// Top-left
		{
			const byte *src_p = dst + ( atlasY       * LM_PAGE_W + atlasX       ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY - 1) * LM_PAGE_W + (atlasX - 1) ) * LM_COMPS;
			dst_p[0] = src_p[0]; dst_p[1] = src_p[1]; dst_p[2] = src_p[2];
		}
		// Top-right
		{
			const byte *src_p = dst + ( atlasY       * LM_PAGE_W + (atlasX + w - 1) ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY - 1) * LM_PAGE_W + (atlasX + w)     ) * LM_COMPS;
			dst_p[0] = src_p[0]; dst_p[1] = src_p[1]; dst_p[2] = src_p[2];
		}
		// Bottom-left
		{
			const byte *src_p = dst + ( (atlasY + h - 1) * LM_PAGE_W + atlasX       ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY + h)     * LM_PAGE_W + (atlasX - 1) ) * LM_COMPS;
			dst_p[0] = src_p[0]; dst_p[1] = src_p[1]; dst_p[2] = src_p[2];
		}
		// Bottom-right
		{
			const byte *src_p = dst + ( (atlasY + h - 1) * LM_PAGE_W + (atlasX + w - 1) ) * LM_COMPS;
			byte       *dst_p = dst + ( (atlasY + h)     * LM_PAGE_W + (atlasX + w)     ) * LM_COMPS;
			dst_p[0] = src_p[0]; dst_p[1] = src_p[1]; dst_p[2] = src_p[2];
		}
	}
}

// ---------------------------------------------------------------------------
// Clipnode hull-1 brush builder
// ---------------------------------------------------------------------------

// Q1 hull-1 clipnode trees can reach 150–200+ levels in production maps (e.g. e1m1).
// 64 was too small: SOLID leaves beyond that depth were silently dropped, leaving
// floor-brush gaps that let the player fall through.
#define MAX_CLIP_DEPTH 512

typedef struct {
	int planeNum;  // *2 index into doubled plane array
	int side;      // 0=keep original normal  1=use negated normal
} BrushPlane_t;

typedef struct {
	BrushPlane_t  stack[MAX_CLIP_DEPTH];
	int           depth;
	int           clipShaderIdx;
	int           numBrushes;
	int           numBrushSides;
	int           numSolids;
	qboolean      countingOnly;
	dbrush_t     *brushOut;
	dbrushside_t *sideOut;
} ClipWalk_t;

/* Walk one subtree of the hull-1 clipnode tree.
 * Planes are pre-doubled: cn[i].planenum is already 2×Q1-plane-index.
 * Front descent (child[0]): solid is on the + side → brush needs the negated plane (side=1).
 * Back  descent (child[1]): solid is on the - side → brush needs the original plane (side=0).
 */
static void BSP_Q1_WalkClipTree( ClipWalk_t *w,
                                  const q1_dclipnode_t *cn, int numCn,
                                  int cnIdx )
{
	if ( cnIdx >= 0 ) {
		int k;
		const q1_dclipnode_t *node;

		if ( cnIdx >= numCn ) {
			Com_Log( SEV_TRACE, LOG_CH(ch_loading), S_COLOR_YELLOW "BSP_Q1_WalkClipTree: index %d >= numCn %d\n",
			            cnIdx, numCn );
			return;
		}
		if ( w->depth >= MAX_CLIP_DEPTH ) {
			Com_Log( SEV_WARN, LOG_CH(ch_loading), "BSP_Q1_WalkClipTree: depth %d >= MAX_CLIP_DEPTH; solid leaf dropped\n",
			            w->depth );
			return;
		}

		node = &cn[cnIdx];

		/* front child: if solid, + side is solid → outward normal must point negative → flip (side=1) */
		w->stack[w->depth].planeNum = (int)node->planenum;
		w->stack[w->depth].side     = 1;
		w->depth++;
		BSP_Q1_WalkClipTree( w, cn, numCn, (int)(short)node->children[0] );
		w->depth--;

		/* back child: if solid, - side is solid → outward normal points positive → keep (side=0) */
		w->stack[w->depth].planeNum = (int)node->planenum;
		w->stack[w->depth].side     = 0;
		w->depth++;
		BSP_Q1_WalkClipTree( w, cn, numCn, (int)(short)node->children[1] );
		w->depth--;
		return;
	}

	/* leaf: -2=SOLID, -1=EMPTY */
	if ( cnIdx == -2 ) {
		int k;
		w->numSolids++;
		if ( w->countingOnly ) {
			w->numBrushes++;
			w->numBrushSides += w->depth;
		} else {
			dbrush_t *b = &w->brushOut[w->numBrushes];
			b->firstSide = w->numBrushSides;
			b->numSides  = w->depth;
			b->shaderNum = w->clipShaderIdx;
			for ( k = 0; k < w->depth; k++ ) {
				dbrushside_t *s = &w->sideOut[w->numBrushSides + k];
				s->planeNum  = w->stack[k].planeNum + w->stack[k].side;
				s->shaderNum = w->clipShaderIdx;
			}
			w->numBrushes++;
			w->numBrushSides += w->depth;
		}
	}
}

// ---------------------------------------------------------------------------
// Brush AABB + leaf-brush insertion helpers
// ---------------------------------------------------------------------------

/* 3-plane intersection via Cramer's rule. Returns 1 on success, 0 if near-parallel. */
static int Q1_Intersect3Planes( const dplane_t *p1, const dplane_t *p2,
                                  const dplane_t *p3, vec3_t v )
{
	float n00 = p1->normal[0], n01 = p1->normal[1], n02 = p1->normal[2];
	float n10 = p2->normal[0], n11 = p2->normal[1], n12 = p2->normal[2];
	float n20 = p3->normal[0], n21 = p3->normal[1], n22 = p3->normal[2];

	float det = n00*(n11*n22 - n12*n21)
	          - n01*(n10*n22 - n12*n20)
	          + n02*(n10*n21 - n11*n20);
	if ( fabsf(det) < 1e-5f ) return 0;

	float inv = 1.0f / det;
	float b0 = p1->dist, b1 = p2->dist, b2 = p3->dist;

	v[0] = ( b0 *(n11*n22 - n12*n21) - n01*(b1*n22 - n12*b2) + n02*(b1*n21 - n11*b2) ) * inv;
	v[1] = ( n00*(b1*n22  - n12*b2 ) - b0 *(n10*n22 - n12*n20) + n02*(n10*b2 - b1*n20) ) * inv;
	v[2] = ( n00*(n11*b2  - b1*n21 ) - n01*(n10*b2  - b1*n20 ) + b0 *(n10*n21 - n11*n20) ) * inv;
	return 1;
}

// Tolerance for inside-test in Q1_ComputeBrushAABB.
// Cramer's rule FP error scales with map coordinate magnitude; at Q1 max scale (~4096 units)
// the error in computed vertex positions can reach ~1.0 unit.  Must match BEVEL_DIST_EPSILON
// (defined near BSP_Q1_AddBrushBevels) so AABB accuracy and dup-check tolerance stay in sync.
#define BRUSH_AABB_INSIDE_EPSILON  1.0f

/* Compute AABB for a convex brush by enumerating all valid 3-plane intersection vertices.
 * Returns 1 on success, 0 if degenerate (no valid vertices found). */
static int Q1_ComputeBrushAABB( const bspFile_t *bsp, const dbrush_t *b,
                                  vec3_t outMins, vec3_t outMaxs )
{
	vec3_t verts[512];
	int    numVerts = 0;
	int    ii, jj, kk, mm, dim;
	int    firstSide = b->firstSide;
	int    numSides  = b->numSides;

	for ( ii = 0; ii < numSides - 2; ii++ ) {
	for ( jj = ii+1; jj < numSides - 1; jj++ ) {
	for ( kk = jj+1; kk < numSides;     kk++ ) {
		const dplane_t *p1 = &bsp->planes[ bsp->brushSides[firstSide+ii].planeNum ];
		const dplane_t *p2 = &bsp->planes[ bsp->brushSides[firstSide+jj].planeNum ];
		const dplane_t *p3 = &bsp->planes[ bsp->brushSides[firstSide+kk].planeNum ];
		vec3_t v;
		qboolean inside;
		if ( !Q1_Intersect3Planes( p1, p2, p3, v ) ) continue;

		inside = qtrue;
		for ( mm = 0; mm < numSides; mm++ ) {
			const dplane_t *pm;
			if ( mm == ii || mm == jj || mm == kk ) continue;
			pm = &bsp->planes[ bsp->brushSides[firstSide+mm].planeNum ];
			/* inside brush = on negative side of each outward-facing plane */
			if ( DotProduct(v, pm->normal) - pm->dist > BRUSH_AABB_INSIDE_EPSILON ) { inside = qfalse; break; }
		}
		if ( inside && numVerts < 512 ) {
			VectorCopy( v, verts[numVerts] );
			numVerts++;
		}
	} /* kk */
	} /* jj */
	} /* ii */

	if ( numVerts == 0 ) { VectorClear(outMins); VectorClear(outMaxs); return 0; }

	VectorCopy( verts[0], outMins );
	VectorCopy( verts[0], outMaxs );
	for ( ii = 1; ii < numVerts; ii++ )
		for ( dim = 0; dim < 3; dim++ ) {
			if ( verts[ii][dim] < outMins[dim] ) outMins[dim] = verts[ii][dim];
			if ( verts[ii][dim] > outMaxs[dim] ) outMaxs[dim] = verts[ii][dim];
		}
	return 1;
}

typedef struct {
	int  **data;    /* per-leaf arrays of canonical brush indices */
	int   *count;   /* per-leaf used count */
	int   *cap;     /* per-leaf capacity */
	int    numLeafs;
} LeafBrushBuckets_t;

static void Q1_BucketAppend( LeafBrushBuckets_t *lb, int leafIdx, int brushIdx )
{
	if ( lb->count[leafIdx] >= lb->cap[leafIdx] ) {
		int  newCap  = lb->cap[leafIdx] ? lb->cap[leafIdx] * 2 : 4;
		int *newData = (int *)Z_Malloc( newCap * sizeof(int) );
		if ( lb->data[leafIdx] ) {
			memcpy( newData, lb->data[leafIdx], lb->count[leafIdx] * sizeof(int) );
			Z_Free( lb->data[leafIdx] );
		}
		lb->data[leafIdx] = newData;
		lb->cap[leafIdx]  = newCap;
	}
	lb->data[leafIdx][ lb->count[leafIdx]++ ] = brushIdx;
}

/* Insert one brush AABB into the render BSP tree, collecting which leaves it overlaps. */
static void Q1_InsertBrushIntoTree( const bspFile_t *bsp, LeafBrushBuckets_t *lb,
                                     int brushIdx,
                                     const vec3_t mins, const vec3_t maxs,
                                     int nodeIdx )
{
	const dnode_t  *node;
	const dplane_t *plane;
	vec3_t          cFront, cBack;
	float           dFront, dBack;
	int             k;

	if ( nodeIdx < 0 ) {
		int leafIdx = -(nodeIdx + 1);
		if ( leafIdx < lb->numLeafs )
			Q1_BucketAppend( lb, leafIdx, brushIdx );
		return;
	}

	node  = &bsp->nodes[ nodeIdx ];
	plane = &bsp->planes[ node->planeNum ];

	for ( k = 0; k < 3; k++ ) {
		if ( plane->normal[k] >= 0.0f ) { cFront[k] = maxs[k]; cBack[k] = mins[k]; }
		else                            { cFront[k] = mins[k]; cBack[k] = maxs[k]; }
	}
	dFront = DotProduct( cFront, plane->normal ) - plane->dist;
	dBack  = DotProduct( cBack,  plane->normal ) - plane->dist;

	/* Permissive descent: only prune a subtree when the AABB is more than 16 units
	   strictly on one side.  Brushes within the epsilon band go into both children.
	   EPS=16 matches Q1's standard stair riser, guaranteeing every leaf within one
	   step-height of a splitting plane receives the brush. */
#define Q1_BRUSH_INSERT_EPS 16.0f
	if ( dFront < -Q1_BRUSH_INSERT_EPS ) {
		Q1_InsertBrushIntoTree( bsp, lb, brushIdx, mins, maxs, node->children[1] );
	} else if ( dBack > Q1_BRUSH_INSERT_EPS ) {
		Q1_InsertBrushIntoTree( bsp, lb, brushIdx, mins, maxs, node->children[0] );
	} else {
		Q1_InsertBrushIntoTree( bsp, lb, brushIdx, mins, maxs, node->children[0] );
		Q1_InsertBrushIntoTree( bsp, lb, brushIdx, mins, maxs, node->children[1] );
	}
#undef Q1_BRUSH_INSERT_EPS
}

// ---------------------------------------------------------------------------
// Load-time diagnostic helpers
// ---------------------------------------------------------------------------

static void BSP_Q1_ParseSpawnOrigin( const char *entityStr, vec3_t out )
{
	const char *p = entityStr;
	out[0] = out[1] = out[2] = 0.0f;
	if ( !p ) return;
	while ( *p ) {
		const char *blockStart = strchr( p, '{' );
		if ( !blockStart ) break;
		const char *blockEnd = strchr( blockStart, '}' );
		if ( !blockEnd ) break;
		if ( strstr( blockStart, "info_player_start" ) &&
		     strstr( blockStart, "info_player_start" ) < blockEnd ) {
			const char *og = strstr( blockStart, "\"origin\"" );
			if ( og && og < blockEnd ) {
				og += 8;
				while ( *og == ' ' || *og == '\t' ) og++;
				if ( *og == '"' ) {
					og++;
					sscanf( og, "%f %f %f", &out[0], &out[1], &out[2] );
				}
			}
			return;
		}
		p = blockEnd + 1;
	}
}


static void BSP_Q1_DiagBrushes( const char *name, const bspFile_t *bsp, int enabled )
{
	static char lastBsp[MAX_QPATH] = "";
	int i, k, nPrint;

	if ( !enabled ) return;
	if ( Q_stricmp( name, lastBsp ) == 0 ) return;
	Q_strncpyz( lastBsp, name, sizeof( lastBsp ) );

	nPrint = bsp->numBrushes < 3 ? bsp->numBrushes : 3;
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] === Brush dump (first %d of %d) ===\n", nPrint, bsp->numBrushes );
	for ( i = 0; i < nPrint; i++ ) {
		const dbrush_t *b = &bsp->brushes[i];
		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] brush[%d]: %d sides shader=%d\n", i, b->numSides, b->shaderNum );
		for ( k = 0; k < b->numSides; k++ ) {
			const dbrushside_t *s = &bsp->brushSides[b->firstSide + k];
			int pn = s->planeNum;
			const dplane_t *p = &bsp->planes[pn];
			Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG]   side[%d]: pn=%d(orig=%d,s=%d) n=(%.4f,%.4f,%.4f) d=%.4f\n",
			            k, pn, pn >> 1, pn & 1,
			            p->normal[0], p->normal[1], p->normal[2], p->dist );
		}
	}

	{
		vec3_t spawnOrig, testPt;
		int nodeIdx, walkDepth, leafIdx, lb, maxLB;
		BSP_Q1_ParseSpawnOrigin( bsp->entityString, spawnOrig );
		/* Test at actual spawn (empty space where player stands), not -32Z (solid space). */
		testPt[0] = spawnOrig[0];
		testPt[1] = spawnOrig[1];
		testPt[2] = spawnOrig[2];

		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] === Q3-CM spawn trace ===\n" );
		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] spawnOrigin=(%.1f,%.1f,%.1f) testPt=spawnOrigin\n",
		            spawnOrig[0], spawnOrig[1], spawnOrig[2] );

		nodeIdx   = 0;
		walkDepth = 0;
		while ( nodeIdx >= 0 ) {
			const dnode_t *node = &bsp->nodes[nodeIdx];
			const dplane_t *pl = &bsp->planes[node->planeNum];
			float d = DotProduct( testPt, pl->normal ) - pl->dist;
			nodeIdx = ( d >= 0.0f ) ? node->children[0] : node->children[1];
			if ( ++walkDepth > 100 ) { Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] BSP walk depth limit\n" ); break; }
		}
		leafIdx = -(nodeIdx + 1);
		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] render BSP → leafIdx=%d\n", leafIdx );

		if ( leafIdx < 0 || leafIdx >= bsp->numLeafs ) {
			Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] leafIdx out of range (%d leafs)\n", bsp->numLeafs );
			return;
		}

		{
			const dleaf_t *lf = &bsp->leafs[leafIdx];
			Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG] leaf[%d]: firstLeafBrush=%d numLeafBrushes=%d\n",
			            leafIdx, lf->firstLeafBrush, lf->numLeafBrushes );

			maxLB = lf->numLeafBrushes < 8 ? lf->numLeafBrushes : 8;
			for ( lb = 0; lb < maxLB; lb++ ) {
				int bi = bsp->leafBrushes[lf->firstLeafBrush + lb];
				const dbrush_t *b;
				qboolean inside = qtrue;
				if ( bi < 0 || bi >= bsp->numBrushes ) {
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG]   leafBrush[%d]: bi=%d OUT OF RANGE\n", lb, bi );
					continue;
				}
				b = &bsp->brushes[bi];
				for ( k = 0; k < b->numSides; k++ ) {
					const dbrushside_t *s = &bsp->brushSides[b->firstSide + k];
					const dplane_t *p = &bsp->planes[s->planeNum];
					float d = DotProduct( testPt, p->normal ) - p->dist;
					if ( d > 0.0f ) inside = qfalse;
				}
				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG]   leafBrush[%d]=brush[%d] (%d sides): testPt %s\n",
				            lb, bi, b->numSides, inside ? "INSIDE(solid)" : "OUTSIDE" );
				for ( k = 0; k < b->numSides; k++ ) {
					const dbrushside_t *s = &bsp->brushSides[b->firstSide + k];
					int pn = s->planeNum;
					const dplane_t *p = &bsp->planes[pn];
					float d = DotProduct( testPt, p->normal ) - p->dist;
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG]     side[%d]: pn=%d(orig=%d,s=%d) n=(%.4f,%.4f,%.4f) dist=%.4f testD=%.4f [%s]\n",
					            k, pn, pn >> 1, pn & 1,
					            p->normal[0], p->normal[1], p->normal[2], p->dist, d,
					            d > 0.0f ? "OUTSIDE" : "inside" );
				}
			}
			if ( lf->numLeafBrushes > 8 )
				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "[Q1DIAG]   ...(%d more brushes)\n", lf->numLeafBrushes - 8 );
		}
	}
}


// ---------------------------------------------------------------------------
// Q1 PVS decompression
// ---------------------------------------------------------------------------

/*
=================
BSP_Q1_RLEDecompressVis

Decompresses one Q1 leaf PVS bitfield row into `out` (clusterBytes bytes).
Q1 RLE: non-zero bytes are literal copies; a 0x00 byte followed by count N
emits N zero bytes. Output is hard-clamped to clusterBytes — malformed data
on corrupt maps cannot overrun the buffer.
If `in` is NULL (leaf had visofs == -1) the leaf has no vis data; fill 0xFF
(fully visible), matching FTEQW Q1BSP_DecompressVis null-input behaviour.
=================
*/
static void BSP_Q1_RLEDecompressVis( const byte *in, int clusterBytes, byte *out ) {
	byte *end = out + clusterBytes;
	int   c;

	if ( !in ) {
		memset( out, 0xFF, clusterBytes );
		return;
	}

	while ( out < end ) {
		if ( *in ) {
			*out++ = *in++;
		} else {
			c   = (int)(unsigned char)in[1];
			in += 2;
			if ( c > end - out ) c = (int)( end - out );
			while ( c-- ) *out++ = 0;
		}
	}
}

// ---------------------------------------------------------------------------
// Synthetic Q3 rawData builder
// ---------------------------------------------------------------------------
// RE_LoadWorldMap casts bsp->rawData to (dheader_t*) and reads Q3 binary
// format (BSP_IDENT, BSP_VERSION=46, 17 lumps).  Q1 rawData is version 29
// with 15 lumps and would produce garbage offsets.  We replace rawData with
// a proper Q3-format blob serialised from the already-translated struct fields.

static void BSP_Q1_BuildSyntheticQ3Data( bspFile_t *bsp ) {
	// Calculate sizes of each lump payload
	size_t sz_entities    = (size_t)bsp->entityStringLength;
	size_t sz_shaders     = (size_t)bsp->numShaders     * sizeof( dshader_t );
	size_t sz_planes      = (size_t)bsp->numPlanes      * sizeof( dplane_t );
	size_t sz_nodes       = (size_t)bsp->numNodes       * sizeof( dnode_t );
	size_t sz_leafs       = (size_t)bsp->numLeafs       * sizeof( dleaf_t );
	size_t sz_leafsurfs   = (size_t)bsp->numLeafSurfaces * sizeof( int );
	size_t sz_leafbrushes = (size_t)bsp->numLeafBrushes * sizeof( int );
	size_t sz_models      = (size_t)bsp->numSubModels   * sizeof( dmodel_t );
	size_t sz_brushes     = (size_t)bsp->numBrushes     * sizeof( dbrush_t );
	size_t sz_brushsides  = (size_t)bsp->numBrushSides  * sizeof( dbrushside_t );
	size_t sz_drawverts   = (size_t)bsp->numDrawVerts   * sizeof( drawVert_t );
	size_t sz_drawindexes = (size_t)bsp->numDrawIndexes * sizeof( int );
	size_t sz_fogs        = 0;
	size_t sz_surfaces    = (size_t)bsp->numSurfaces    * sizeof( dsurface_t );
	size_t sz_lightmaps   = (size_t)bsp->numLightmapPages * LM_PAGE_BYTES;
	size_t sz_lightgrid   = 0;
	// Use bsp->numClusters, not bsp->numLeafs-1: the leaf-deduplication pass may
	// expand numLeafs after the PVS table was built against the original leaf count.
	int    visClusters     = bsp->numClusters;
	int    visClusterBytes = bsp->clusterBytes;
	size_t sz_vis          = 8 + (size_t)visClusters * visClusterBytes;

	size_t headerSz = sizeof( int ) * 2 + HEADER_LUMPS * sizeof( lump_t );  // ident+version+lumps[]

	// Align each lump to 4 bytes
#define ALIGN4(n) (((n) + 3) & ~3)
	size_t off_entities    = headerSz;
	size_t off_shaders     = off_entities    + ALIGN4( sz_entities );
	size_t off_planes      = off_shaders     + ALIGN4( sz_shaders );
	size_t off_nodes       = off_planes      + ALIGN4( sz_planes );
	size_t off_leafs       = off_nodes       + ALIGN4( sz_nodes );
	size_t off_leafsurfs   = off_leafs       + ALIGN4( sz_leafs );
	size_t off_leafbrushes = off_leafsurfs   + ALIGN4( sz_leafsurfs );
	size_t off_models      = off_leafbrushes + ALIGN4( sz_leafbrushes );
	size_t off_brushes     = off_models      + ALIGN4( sz_models );
	size_t off_brushsides  = off_brushes     + ALIGN4( sz_brushes );
	size_t off_drawverts   = off_brushsides  + ALIGN4( sz_brushsides );
	size_t off_drawindexes = off_drawverts   + ALIGN4( sz_drawverts );
	size_t off_fogs        = off_drawindexes + ALIGN4( sz_drawindexes );
	size_t off_surfaces    = off_fogs        + ALIGN4( sz_fogs );
	size_t off_lightmaps   = off_surfaces    + ALIGN4( sz_surfaces );
	size_t off_lightgrid   = off_lightmaps   + ALIGN4( sz_lightmaps );
	size_t off_vis         = off_lightgrid   + ALIGN4( sz_lightgrid );
	size_t totalSize       = off_vis         + ALIGN4( sz_vis );
#undef ALIGN4

	byte *buf = BSP_Q1_ZAlloc( totalSize );

	// Header
	*(int *)(buf + 0)              = LittleLong( BSP_IDENT );
	*(int *)(buf + 4)              = LittleLong( BSP_VERSION );

	lump_t *lumps = (lump_t *)(buf + 8);
#define SET_LUMP(idx, off, sz) \
	lumps[idx].fileofs = LittleLong( (int)(off) ); \
	lumps[idx].filelen = LittleLong( (int)(sz) )

	SET_LUMP( LUMP_ENTITIES,    off_entities,    sz_entities );
	SET_LUMP( LUMP_SHADERS,     off_shaders,     sz_shaders );
	SET_LUMP( LUMP_PLANES,      off_planes,      sz_planes );
	SET_LUMP( LUMP_NODES,       off_nodes,       sz_nodes );
	SET_LUMP( LUMP_LEAFS,       off_leafs,       sz_leafs );
	SET_LUMP( LUMP_LEAFSURFACES,  off_leafsurfs,   sz_leafsurfs );
	SET_LUMP( LUMP_LEAFBRUSHES,   off_leafbrushes, sz_leafbrushes );
	SET_LUMP( LUMP_MODELS,      off_models,      sz_models );
	SET_LUMP( LUMP_BRUSHES,     off_brushes,     sz_brushes );
	SET_LUMP( LUMP_BRUSHSIDES,  off_brushsides,  sz_brushsides );
	SET_LUMP( LUMP_DRAWVERTS,   off_drawverts,   sz_drawverts );
	SET_LUMP( LUMP_DRAWINDEXES, off_drawindexes, sz_drawindexes );
	SET_LUMP( LUMP_FOGS,        off_fogs,        sz_fogs );
	SET_LUMP( LUMP_SURFACES,    off_surfaces,    sz_surfaces );
	SET_LUMP( LUMP_LIGHTMAPS,   off_lightmaps,   sz_lightmaps );
	SET_LUMP( LUMP_LIGHTGRID,   off_lightgrid,   sz_lightgrid );
	SET_LUMP( LUMP_VISIBILITY,  off_vis,         sz_vis );
#undef SET_LUMP

	// Payload blobs
	if ( sz_entities && bsp->entityString )
		memcpy( buf + off_entities, bsp->entityString, sz_entities );
	if ( sz_shaders && bsp->shaders )
		memcpy( buf + off_shaders, bsp->shaders, sz_shaders );
	if ( sz_planes && bsp->planes )
		memcpy( buf + off_planes, bsp->planes, sz_planes );
	if ( sz_nodes && bsp->nodes )
		memcpy( buf + off_nodes, bsp->nodes, sz_nodes );
	if ( sz_leafs && bsp->leafs )
		memcpy( buf + off_leafs, bsp->leafs, sz_leafs );
	if ( sz_leafsurfs && bsp->leafSurfaces )
		memcpy( buf + off_leafsurfs, bsp->leafSurfaces, sz_leafsurfs );
	if ( sz_models && bsp->subModels )
		memcpy( buf + off_models, bsp->subModels, sz_models );
	if ( sz_brushes && bsp->brushes )
		memcpy( buf + off_brushes, bsp->brushes, sz_brushes );
	if ( sz_brushsides && bsp->brushSides )
		memcpy( buf + off_brushsides, bsp->brushSides, sz_brushsides );
	if ( sz_leafbrushes && bsp->leafBrushes )
		memcpy( buf + off_leafbrushes, bsp->leafBrushes, sz_leafbrushes );
	if ( sz_drawverts && bsp->drawVerts )
		memcpy( buf + off_drawverts, bsp->drawVerts, sz_drawverts );
	if ( sz_drawindexes && bsp->drawIndexes )
		memcpy( buf + off_drawindexes, bsp->drawIndexes, sz_drawindexes );
	if ( sz_surfaces && bsp->surfaces )
		memcpy( buf + off_surfaces, bsp->surfaces, sz_surfaces );
	if ( sz_lightmaps && bsp->lightmapData )
		memcpy( buf + off_lightmaps, bsp->lightmapData, sz_lightmaps );

	// Vis lump: 8-byte header (numClusters, clusterBytes) + flat pre-decompressed PVS table.
	assert( bsp->visibilityLength == visClusters * visClusterBytes );
	*(int *)(buf + off_vis + 0) = LittleLong( visClusters );
	*(int *)(buf + off_vis + 4) = LittleLong( visClusterBytes );
	if ( visClusters > 0 && bsp->visibility ) {
		memcpy( buf + off_vis + 8, bsp->visibility, bsp->visibilityLength );
	}

	// Replace rawData
	if ( bsp->rawData ) {
		Z_Free( bsp->rawData );
	}
	bsp->rawData   = buf;
	bsp->rawLength = (int)totalSize;
}

// ---------------------------------------------------------------------------
// Bevel plane synthesis
// ---------------------------------------------------------------------------
// BSP_Q1_AddBrushBevels — q3map2-canonical bevel generation
//
// Ports AddBrushBevels() from q3map2/map.cpp (netradiant-custom).
// Two passes per brush:
//   1. Box bevels: add the 6 axial planes derived from winding-based AABB,
//      swap them into canonical positions 0..5 in the side list.
//   2. Edge bevels: for every edge of every non-axial side (idx >= 6), try
//      the 6 slanted axials formed by cross(edgeDir, axisVec*sign).  Accept
//      a candidate only when every vertex of every side winding lies behind
//      or on the plane (the outer-hull test).
//
// Winding computation uses the same Q1_Intersect3Planes-based vertex
// enumeration as Q1_ComputeBrushAABB, then clips the large base quad for
// each side.  This avoids a full Sutherland-Hodgman implementation while
// staying correct for convex brushes.
//
// Called AFTER Step 8 dedup, BEFORE BSP_Q1_SynthesizeWorldShell.
// ---------------------------------------------------------------------------

// Epsilon constants — kept at the top so they document the tolerance contract.
#define BEVEL_NORMAL_EPSILON   1e-3f   // normal-component match for dedup
#define BEVEL_DIST_EPSILON     1.0f    // distance match for dedup / AABB inside test
#define BEVEL_SNAP_EPSILON     1e-5f   // threshold for snapping near-unit components to ±1/0
#define BEVEL_EDGE_DEGENERATE  0.5f    // VectorNormalize result below this → degenerate edge/plane
#define BEVEL_WINDING_EPSILON  0.1f    // point-on-plane tolerance for edge-bevel outer-hull test
#define BEVEL_MAP_BOUNDS       65536.0f// large-quad radius for base winding generation
// Q1 maps are bounded to ±16384 Quake units by convention; 2× gives a safe
// reject threshold for near-degenerate geometry and sentinel planes.
#define Q1_MAP_COORD_MAX       32768.0f

// Per-side winding used during bevel computation (stack allocated per brush).
#define BEVEL_MAX_WINDING_PTS  64
typedef struct {
	int   numPts;
	float pts[BEVEL_MAX_WINDING_PTS][3];
} BevelWinding_t;

// SnapNormal: if any component is within BEVEL_SNAP_EPSILON of ±1, snap it
// to exactly ±1 and zero the others; then renormalize.  This matches
// q3map2's SnapNormal() (simple axis-snapping variant).
static void Q1_SnapNormal( float n[3] ) {
	int   i;
	qboolean adjusted = qfalse;
	for ( i = 0; i < 3; i++ ) {
		if ( fabsf( n[i] - 1.0f ) < BEVEL_SNAP_EPSILON ) { n[0]=0; n[1]=0; n[2]=0; n[i]= 1.0f; adjusted=qtrue; break; }
		if ( fabsf( n[i] + 1.0f ) < BEVEL_SNAP_EPSILON ) { n[0]=0; n[1]=0; n[2]=0; n[i]=-1.0f; adjusted=qtrue; break; }
	}
	if ( !adjusted ) {
		// renormalize in case components were zeroed
		float len = sqrtf( n[0]*n[0] + n[1]*n[1] + n[2]*n[2] );
		if ( len > 1e-8f ) { n[0]/=len; n[1]/=len; n[2]/=len; }
	}
}

// Q1_BaseWindingForPlane: generate a large quad covering the map for plane
// (normal, dist).  The quad is centred on normal*dist and lies in the plane.
static void Q1_BaseWindingForPlane( const float normal[3], float dist,
                                     BevelWinding_t *w )
{
	int    i, x = -1;
	float  max = -1.0f;
	float  vup[3] = {0,0,0}, vright[3] = {0,0,0}, org[3];
	float  len;

	// find the dominant axis
	for ( i = 0; i < 3; i++ ) {
		float v = fabsf( normal[i] );
		if ( v > max ) { max = v; x = i; }
	}

	// choose an up vector not parallel to normal
	VectorClear( vup );
	if ( x == 2 ) vup[0] = 1.0f; else vup[2] = 1.0f;

	// vup = vup - normal * dot(vup, normal)
	{
		float d = vup[0]*normal[0] + vup[1]*normal[1] + vup[2]*normal[2];
		vup[0] -= normal[0]*d;
		vup[1] -= normal[1]*d;
		vup[2] -= normal[2]*d;
	}
	len = sqrtf( vup[0]*vup[0] + vup[1]*vup[1] + vup[2]*vup[2] );
	if ( len < 1e-8f ) { w->numPts = 0; return; }
	vup[0]/=len; vup[1]/=len; vup[2]/=len;
	vup[0] *= BEVEL_MAP_BOUNDS;
	vup[1] *= BEVEL_MAP_BOUNDS;
	vup[2] *= BEVEL_MAP_BOUNDS;

	// vright = cross(normal, vup)
	vright[0] = normal[1]*vup[2] - normal[2]*vup[1];
	vright[1] = normal[2]*vup[0] - normal[0]*vup[2];
	vright[2] = normal[0]*vup[1] - normal[1]*vup[0];
	// already scaled to BEVEL_MAP_BOUNDS magnitude via vup

	// org = normal * dist
	org[0] = normal[0]*dist;
	org[1] = normal[1]*dist;
	org[2] = normal[2]*dist;

	// 4 corners: (±vup ± vright) + org
	w->numPts = 4;
	w->pts[0][0] = org[0] + vup[0] - vright[0];
	w->pts[0][1] = org[1] + vup[1] - vright[1];
	w->pts[0][2] = org[2] + vup[2] - vright[2];

	w->pts[1][0] = org[0] - vup[0] - vright[0];
	w->pts[1][1] = org[1] - vup[1] - vright[1];
	w->pts[1][2] = org[2] - vup[2] - vright[2];

	w->pts[2][0] = org[0] - vup[0] + vright[0];
	w->pts[2][1] = org[1] - vup[1] + vright[1];
	w->pts[2][2] = org[2] - vup[2] + vright[2];

	w->pts[3][0] = org[0] + vup[0] + vright[0];
	w->pts[3][1] = org[1] + vup[1] + vright[1];
	w->pts[3][2] = org[2] + vup[2] + vright[2];
}

// Q1_ChopWindingInPlace: Sutherland-Hodgman clip of winding w against the
// back side of plane (clipNormal, clipDist).  Points with
// dot(p,clipNormal)-clipDist > 0 are in front (clipped away).
// Returns qfalse if the winding is completely clipped away.
static qboolean Q1_ChopWindingInPlace( BevelWinding_t *w,
                                        const float clipNormal[3], float clipDist )
{
	float  dists[BEVEL_MAX_WINDING_PTS + 1];
	int    sides[BEVEL_MAX_WINDING_PTS + 1];
	int    counts[3] = {0,0,0}; // front, back, on
	int    i, j;
	float  out[BEVEL_MAX_WINDING_PTS][3];
	int    numOut = 0;

	if ( w->numPts < 1 ) return qfalse;

	for ( i = 0; i < w->numPts; i++ ) {
		float d = w->pts[i][0]*clipNormal[0] + w->pts[i][1]*clipNormal[1]
		        + w->pts[i][2]*clipNormal[2] - clipDist;
		dists[i] = d;
		if      ( d >  BEVEL_WINDING_EPSILON ) sides[i] = 0; // front
		else if ( d < -BEVEL_WINDING_EPSILON ) sides[i] = 1; // back
		else                                   sides[i] = 2; // on
		counts[sides[i]]++;
	}
	dists[i] = dists[0];
	sides[i] = sides[0];

	if ( !counts[1] && !counts[2] ) return qfalse; // all front → fully clipped
	if ( !counts[0] )               return qtrue;   // all back → keep as-is

	for ( i = 0; i < w->numPts; i++ ) {
		const float *p1 = w->pts[i];
		const float *p2 = w->pts[(i+1) % w->numPts];

		if ( sides[i] != 0 ) { // back or on: keep
			if ( numOut >= BEVEL_MAX_WINDING_PTS ) return qfalse;
			out[numOut][0] = p1[0];
			out[numOut][1] = p1[1];
			out[numOut][2] = p1[2];
			numOut++;
		}

		if ( sides[i] == sides[i+1] ) continue;             // same side — no crossing
		if ( sides[i] == 2 || sides[i+1] == 2 ) continue; // on-plane vertex — no split needed

		// crossing: interpolate
		{
			float frac = dists[i] / (dists[i] - dists[i+1]);
			if ( numOut >= BEVEL_MAX_WINDING_PTS ) return qfalse;
			for ( j = 0; j < 3; j++ )
				out[numOut][j] = p1[j] + frac * (p2[j] - p1[j]);
			numOut++;
		}
	}

	if ( numOut < 3 ) return qfalse;

	w->numPts = numOut;
	for ( i = 0; i < numOut; i++ ) {
		w->pts[i][0] = out[i][0];
		w->pts[i][1] = out[i][1];
		w->pts[i][2] = out[i][2];
	}
	return qtrue;
}

// Q1_CreateBrushWinding: compute the winding for side sideIdx of a brush
// whose planes are planes[0..numSides-1].
// Returns qfalse if the resulting winding is degenerate.
static qboolean Q1_CreateBrushWinding( const dplane_t *planes, int numSides,
                                        int sideIdx, BevelWinding_t *wOut )
{
	int i;
	// Start with a large quad in the plane of side sideIdx.
	Q1_BaseWindingForPlane( planes[sideIdx].normal, planes[sideIdx].dist, wOut );
	if ( wOut->numPts < 3 ) return qfalse;

	// Clip against the back of every other side (inside the brush).
	for ( i = 0; i < numSides; i++ ) {
		if ( i == sideIdx ) continue;
		// ChopWindingInPlace keeps vertices where dot(p,n)≤d (BACK half-space).
		// Brush plane normals point outward, so BACK = brush interior — correct.
		if ( !Q1_ChopWindingInPlace( wOut, planes[i].normal, planes[i].dist ) ) {
			wOut->numPts = 0;
			return qfalse;
		}
		if ( wOut->numPts < 3 ) { wOut->numPts = 0; return qfalse; }
	}
	return qtrue;
}

// Q1_FindOrAddPlane: search newPlanes[0..numNew) for a matching plane;
// if found return its index.  Otherwise append and return the new index.
// origCount is the count of pre-existing planes (always copied into newPlanes
// before we start).
static int Q1_FindOrAddPlane( dplane_t *newPlanes, int *numNew,
                               const float normal[3], float dist )
{
	int i;
	for ( i = 0; i < *numNew; i++ ) {
		if ( fabsf( newPlanes[i].normal[0] - normal[0] ) < BEVEL_NORMAL_EPSILON &&
		     fabsf( newPlanes[i].normal[1] - normal[1] ) < BEVEL_NORMAL_EPSILON &&
		     fabsf( newPlanes[i].normal[2] - normal[2] ) < BEVEL_NORMAL_EPSILON &&
		     fabsf( newPlanes[i].dist      - dist      ) < BEVEL_DIST_EPSILON )
			return i;
	}
	memset( &newPlanes[*numNew], 0, sizeof( dplane_t ) );
	VectorCopy( normal, newPlanes[*numNew].normal );
	newPlanes[*numNew].dist = dist;
	return (*numNew)++;
}

// ---------------------------------------------------------------------------
// BSP_Q1_RemoveRedundantPlanes — Step 8.7
// Eliminate intermediate-navigation planes that do not geometrically bound
// the convex solid-leaf volume.  Uses the winding-clip test from ericw-tools
// RemoveRedundantPlanes (common/decompile.cc:236): for each plane P, clip a
// large quad in P against all other planes; if the winding becomes empty then
// P's face lies entirely outside the brush and is redundant.
// Called AFTER BSP_Q1_AddBrushBevels, which ensures every brush has at least
// 6 axial bounding planes before this test runs.
// ---------------------------------------------------------------------------
static void BSP_Q1_RemoveRedundantPlanes( bspFile_t *bsp ) {
#define Q1_RRP_MAX_SIDES 128
	int bi, si;
	int nRemoved = 0;
	int nDropped = 0;

	dbrushside_t *newSides = (dbrushside_t *)Z_Malloc( bsp->numBrushSides * sizeof(dbrushside_t) );
	int           writeIdx = 0;

	for ( bi = 0; bi < bsp->numBrushes; bi++ ) {
		dbrush_t *b     = &bsp->brushes[bi];
		int       first = b->firstSide;
		int       num   = b->numSides;
		qboolean  keep[Q1_RRP_MAX_SIDES];
		dplane_t  planes[Q1_RRP_MAX_SIDES];
		int       nKeep = 0;

		// Emit oversized brushes unchanged to avoid stack overrun.
		if ( num > Q1_RRP_MAX_SIDES ) {
			b->firstSide = writeIdx;
			memcpy( &newSides[writeIdx], &bsp->brushSides[first],
					num * sizeof(dbrushside_t) );
			writeIdx += num;
			continue;
		}

		// Build local plane array for Q1_CreateBrushWinding.
		for ( si = 0; si < num; si++ )
			planes[si] = bsp->planes[ bsp->brushSides[first + si].planeNum ];

		// Winding-clip test per side: if the large quad in plane si clips to
		// empty when intersected with all other planes, the plane's face lies
		// entirely outside the brush → redundant intermediate-navigation plane.
		for ( si = 0; si < num; si++ ) {
			BevelWinding_t w;
			keep[si] = Q1_CreateBrushWinding( planes, num, si, &w );
			if ( !keep[si] ) nRemoved++;
			else             nKeep++;
		}

		// AddBrushBevels guarantees ≥6 axial sides; fewer than 4 surviving
		// means a degenerate zero-volume brush — drop it.
		if ( nKeep < 4 ) {
			Com_Log( SEV_WARN, LOG_CH(ch_loading),
				"Q1RemoveRedundantPlanes: brush %d has %d sides after reduction — dropped\n",
				bi, nKeep );
			b->numSides  = 0;
			b->firstSide = 0;
			nDropped++;
			continue;
		}

		// Emit surviving sides into the new buffer.
		b->firstSide = writeIdx;
		b->numSides  = nKeep;
		for ( si = 0; si < num; si++ )
			if ( keep[si] )
				newSides[writeIdx++] = bsp->brushSides[first + si];
	}

	Z_Free( bsp->brushSides );
	bsp->brushSides    = newSides;
	bsp->numBrushSides = writeIdx;

	// Compact brushes array: remove entries with numSides == 0.
	if ( nDropped > 0 ) {
		int sm;
		int *remap  = (int *)Z_Malloc( bsp->numBrushes * sizeof(int) );
		int *newIdx = (int *)Z_Malloc( bsp->numBrushes * sizeof(int) );
		int  newCount = 0;
		dbrush_t *nb;
		for ( bi = 0; bi < bsp->numBrushes; bi++ ) {
			if ( bsp->brushes[bi].numSides == 0 ) {
				remap[bi] = -1;
			} else {
				remap[bi]        = newCount;
				newIdx[newCount] = bi;
				newCount++;
			}
		}
		nb = (dbrush_t *)Z_Malloc( newCount * sizeof(dbrush_t) );
		for ( bi = 0; bi < newCount; bi++ )
			nb[bi] = bsp->brushes[ newIdx[bi] ];
		Z_Free( bsp->brushes );
		bsp->brushes    = nb;
		bsp->numBrushes = newCount;
		for ( sm = 0; sm < bsp->numSubModels; sm++ ) {
			int oFirst = bsp->subModels[sm].firstBrush;
			int oNum   = bsp->subModels[sm].numBrushes;
			int newFirst = -1, newNum = 0;
			for ( bi = oFirst; bi < oFirst + oNum; bi++ ) {
				if ( remap[bi] >= 0 ) {
					if ( newFirst < 0 ) newFirst = remap[bi];
					newNum++;
				}
			}
			bsp->subModels[sm].firstBrush = (newFirst >= 0) ? newFirst : 0;
			bsp->subModels[sm].numBrushes = newNum;
		}
		Z_Free( remap );
		Z_Free( newIdx );
	}

	Com_Log( SEV_TRACE, LOG_CH(ch_loading),
		"Q1RemoveRedundantPlanes: %d brushes processed, %d redundant planes removed, "
		"%d brushes dropped (degenerate)\n",
		bsp->numBrushes + nDropped, nRemoved, nDropped );
#undef Q1_RRP_MAX_SIDES
}

/* KNOWN LIMITATION: Q1 hull-1 expansion (~56 units Z, 30 units XY) inverts plane
   extents for brushes thinner than the expansion on any axis.  Such brushes are
   detected in the inversion check below and skipped from bevel emission.  Their
   original (inverted) face planes remain but produce zero-volume CM behavior.
   Affects very thin stair steps and architectural detail in some Q1 maps.
   Workaround would require fusing thin brushes with neighbors — out of scope. */
static void BSP_Q1_AddBrushBevels( bspFile_t *bsp ) {
	int          bi, si, wi;
	int          numBrushes       = bsp->numBrushes;
	int          origNumPlanes    = bsp->numPlanes;
	int          newSideIdx       = 0;
	int          numNewPlanes     = origNumPlanes;  // grows as we append
	int          totalBoxBevels      = 0;
	int          totalEdgeBevels     = 0;
	int          numDegenerateBrushes = 0;
	int          hull1ClipShaderNum = -1;
	dbrushside_t *newSides;
	dplane_t     *newPlanes;

	if ( numBrushes == 0 ) return;

	// Upper-bound allocation:
	//   structural bound pass: ≤ 6 axial planes per brush (shared with box-bevel budget)
	//   box bevels:  ≤ 6 per brush (structural pass pre-fills missing ones, no net extra)
	//   edge bevels: ≤ 6*edges per brush; edges ≤ numSides*BEVEL_MAX_WINDING_PTS
	// Use a generous cap and realloc if needed — in practice brushes have ≤ 32 sides.
	{
		int maxExtra = numBrushes * ( 6 + 64 ); // 6 box/structural + up to 64 edge bevels per brush
		newSides  = (dbrushside_t *)Z_Malloc( ( bsp->numBrushSides + maxExtra ) * sizeof( dbrushside_t ) );
		newPlanes = (dplane_t     *)Z_Malloc( ( origNumPlanes + maxExtra )       * sizeof( dplane_t     ) );
	}

	// Copy original planes — Q1_FindOrAddPlane searches from index 0.
	memcpy( newPlanes, bsp->planes, origNumPlanes * sizeof( dplane_t ) );

	// Identify the hull-1 clip shader once so per-brush guards are a single int compare.
	{
		int shi;
		for ( shi = 0; shi < bsp->numShaders; shi++ ) {
			if ( Q_stricmp( bsp->shaders[shi].shader, "*q1_hull1_clip" ) == 0 ) {
				hull1ClipShaderNum = shi;
				break;
			}
		}
	}

	for ( bi = 0; bi < numBrushes; bi++ ) {
		dbrush_t   *br        = &bsp->brushes[bi];
		int         oldFirst  = br->firstSide;
		int         oldCount  = br->numSides;
		int         brFirstNewSide; // index into newSides[] where this brush starts
		int         brNumSides;     // live count (grows as we add bevels)
		int         axis, dir;
		float       mins[3], maxs[3];
		int         order;

		// --- Pack existing sides ------------------------------------------------
		brFirstNewSide = newSideIdx;
		brNumSides     = oldCount;
		memcpy( &newSides[newSideIdx], &bsp->brushSides[oldFirst],
		        oldCount * sizeof( dbrushside_t ) );
		br->firstSide = newSideIdx;
		newSideIdx   += oldCount;

		// --- Collect the planes for this brush (from original arrays) -----------
		// We work from bsp->brushSides[oldFirst..] / bsp->planes[] for windings
		// because the new arrays are still being built.
		// Build a local plane array for Q1_CreateBrushWinding.
		// Max sides per Q1 brush is small (< 64).
#define BEVEL_MAX_BRUSH_SIDES  128
		dplane_t  bplanes[BEVEL_MAX_BRUSH_SIDES];
		int       bnumSides = 0;
		for ( si = 0; si < oldCount && bnumSides < BEVEL_MAX_BRUSH_SIDES; si++ ) {
			bplanes[bnumSides++] = bsp->planes[ bsp->brushSides[oldFirst + si].planeNum ];
		}

		// SnapNormal pre-pass on each original plane in this brush.
		for ( si = 0; si < bnumSides; si++ )
			Q1_SnapNormal( bplanes[si].normal );

		// --- Brush-tight AABB from hull-0 render vertices ---------------------
		// Hull-1 planes are expanded ~16-32 units outward from hull-0 geometry,
		// so hull-0 surface vertices lie ~16 units INSIDE hull-1 planes
		// (DotProduct(v,n)-dist ≈ -16).  A tolerance of 1.0 accepts them while
		// rejecting vertices that are genuinely outside this brush's column.
		// When no vertices are found (clip-only brushes), we fall back to the
		// submodel AABB below.
		vec3_t   btMins, btMaxs;
		qboolean btFound = qfalse;
		{
			int vi;
			for ( vi = 0; vi < bsp->numDrawVerts; vi++ ) {
				const float *pos  = bsp->drawVerts[vi].xyz;
				qboolean     inside = qtrue;
				int          psi2;
				for ( psi2 = 0; psi2 < bnumSides && inside; psi2++ ) {
					if ( DotProduct( pos, bplanes[psi2].normal ) - bplanes[psi2].dist > 1.0f )
						inside = qfalse;
				}
				if ( !inside ) continue;
				if ( !btFound ) {
					VectorCopy( pos, btMins );
					VectorCopy( pos, btMaxs );
					btFound = qtrue;
				} else {
					int ax;
					for ( ax = 0; ax < 3; ax++ ) {
						if ( pos[ax] < btMins[ax] ) btMins[ax] = pos[ax];
						if ( pos[ax] > btMaxs[ax] ) btMaxs[ax] = pos[ax];
					}
				}
			}
		}

		// --- Structural bound pass: ensure all 6 axial planes are present ------
		// Hull-1 brushes clipped only in X/Y (e.g. walls that span the full map
		// height) have no Z-bounding clipnode plane.  Without one, the base winding
		// retains the full MAP_BOUNDS Z extent, so bevel planes land at the world
		// edge (dist ≈ 65511) instead of the real face.  Add any missing axial
		// planes, using brush-tight hull-0 AABB where available; fall back to
		// submodel AABB for invisible clip-only brushes (btFound == qfalse).
		{
			int       sm_idx;
			dmodel_t *sm = NULL;
			int       smAxis, smDir;

			for ( sm_idx = 0; sm_idx < bsp->numSubModels; sm_idx++ ) {
				dmodel_t *candidate = &bsp->subModels[sm_idx];
				if ( bi >= candidate->firstBrush &&
				     bi <  candidate->firstBrush + candidate->numBrushes ) {
					sm = candidate;
					break;
				}
			}

			if ( sm ) {
				for ( smAxis = 0; smAxis < 3; smAxis++ ) {
					for ( smDir = -1; smDir <= 1; smDir += 2 ) {
						float    tgt = (smDir == 1) ? 1.0f : -1.0f;
						qboolean hasPlane = qfalse;
						int      psi;

						for ( psi = 0; psi < bnumSides; psi++ ) {
							if ( bplanes[psi].normal[smAxis] == tgt &&
							     bplanes[psi].normal[(smAxis+1)%3] == 0.0f &&
							     bplanes[psi].normal[(smAxis+2)%3] == 0.0f ) {
								hasPlane = qtrue;
								break;
							}
						}

						if ( !hasPlane && bnumSides < BEVEL_MAX_BRUSH_SIDES && btFound ) {
							float n[3] = {0.0f, 0.0f, 0.0f};
							float d;
							int   planeIdx;

							n[smAxis] = tgt;
							d = (smDir == 1) ? btMaxs[smAxis] : -btMins[smAxis];

							// Add to local bplanes[] so winding computation clips against it.
							bplanes[bnumSides].normal[0] = n[0];
							bplanes[bnumSides].normal[1] = n[1];
							bplanes[bnumSides].normal[2] = n[2];
							bplanes[bnumSides].dist      = d;
							bnumSides++;

							// Add as a real CM brush side.
							planeIdx = Q1_FindOrAddPlane( newPlanes, &numNewPlanes, n, d );
							memset( &newSides[newSideIdx], 0, sizeof( dbrushside_t ) );
							newSides[newSideIdx].planeNum  = planeIdx;
							newSides[newSideIdx].shaderNum = br->shaderNum;
							newSideIdx++;
							brNumSides++;
						}
					}
				}
			}
		}

		// --- Compute windings for all sides ------------------------------------
		BevelWinding_t windings[BEVEL_MAX_BRUSH_SIDES];
		for ( si = 0; si < bnumSides; si++ ) {
			Q1_CreateBrushWinding( bplanes, bnumSides, si, &windings[si] );
		}

		// --- Compute AABB from windings ----------------------------------------
		{
			qboolean first = qtrue;
			for ( si = 0; si < bnumSides; si++ ) {
				for ( wi = 0; wi < windings[si].numPts; wi++ ) {
					const float *v = windings[si].pts[wi];
					if ( first ) {
						mins[0]=v[0]; mins[1]=v[1]; mins[2]=v[2];
						maxs[0]=v[0]; maxs[1]=v[1]; maxs[2]=v[2];
						first = qfalse;
					} else {
						int ax;
						for ( ax = 0; ax < 3; ax++ ) {
							if ( v[ax] < mins[ax] ) mins[ax]=v[ax];
							if ( v[ax] > maxs[ax] ) maxs[ax]=v[ax];
						}
					}
				}
			}
			if ( first ) continue; // degenerate brush — no winding vertices
		}

		// --- Inversion check: skip bevel emission for truly degenerate brushes ---
		// Fires only when the winding-derived AABB is geometrically inverted
		// (maxs < mins on some axis), which indicates no valid convex interior.
		// Structural bound sides committed above are retained for CM representation.
		{
			int ax;
			for ( ax = 0; ax < 3; ax++ ) {
				if ( maxs[ax] < mins[ax] ) {
					numDegenerateBrushes++;
					goto next_brush;
				}
			}
		}

		// =======================================================================
		// Pass 1: Box bevels (q3map2-canonical).
		// For each of the 6 axial directions: check if a side already has that
		// normal (exact match after snap).  If not, add one.  Regardless, swap
		// the matching side into position order (0..5).
		// =======================================================================
		order = 0;
		for ( axis = 0; axis < 3; axis++ ) {
			for ( dir = -1; dir <= 1; dir += 2 ) {
				int  found = -1; // index into brFirstNewSide+[0..brNumSides)
				float tgt  = (dir == 1) ? 1.0f : -1.0f;

				// Search ALL sides (0..brNumSides-1), including structural bound sides
				// (oldCount..brNumSides-1).  Structural sides are only present when no
				// clipnode plane existed for that axis, so finding one means the axis is
				// already bounded — skip emission and just swap it into canonical order.
				for ( si = 0; si < brNumSides; si++ ) {
					const dplane_t *p = &newPlanes[ newSides[brFirstNewSide + si].planeNum ];
					if ( p->normal[axis] == tgt &&
					     p->normal[(axis+1)%3] == 0.0f &&
					     p->normal[(axis+2)%3] == 0.0f ) {
						found = si;
						break;
					}
				}

				if ( found < 0 ) {
					// Add a new axial bevel side.
					float bevelNormal[3] = {0.0f, 0.0f, 0.0f};
					float bevelDist;
					int   planeIdx;

					bevelNormal[axis] = tgt;
					bevelDist = (dir == 1) ? maxs[axis] : -mins[axis];

					planeIdx = Q1_FindOrAddPlane( newPlanes, &numNewPlanes,
					                              bevelNormal, bevelDist );

					memset( &newSides[newSideIdx], 0, sizeof( dbrushside_t ) );
					newSides[newSideIdx].planeNum  = planeIdx;
					newSides[newSideIdx].shaderNum = br->shaderNum;
					newSideIdx++;
					brNumSides++;
					totalBoxBevels++;

					found = brNumSides - 1; // just appended
				}

				// Swap into canonical order (position 'order').
				if ( found != order ) {
					dbrushside_t tmp = newSides[brFirstNewSide + order];
					newSides[brFirstNewSide + order] = newSides[brFirstNewSide + found];
					newSides[brFirstNewSide + found] = tmp;
				}
				order++;
			}
		}

		// =======================================================================
		// Pass 2: Edge bevels.
		// Only run when the brush has non-axial sides (sides beyond the 6 box
		// bevel sides).  Same early-exit as q3map2: pure-axial brush → skip.
		// We also need to rebuild the bplanes/windings arrays to include the
		// newly added box-bevel sides so the outer-hull test works correctly.
		// =======================================================================
		if ( brNumSides <= 6 ) goto next_brush;

		/* Hull-1 expanded brushes place edge-bevel planes at the expansion
		   boundary (~15u XY, ~56u Z), not at visual geometry. This produces
		   phantom vertical walls on ramps and slopes. Box bevels are sufficient
		   for Q1 hull-1 collision correctness. */
		if ( hull1ClipShaderNum >= 0 && br->shaderNum == hull1ClipShaderNum ) goto next_brush;

		// Rebuild local plane array to include box-bevel sides.
		{
			int newBnumSides = brNumSides < BEVEL_MAX_BRUSH_SIDES ? brNumSides : BEVEL_MAX_BRUSH_SIDES;
			for ( si = 0; si < newBnumSides; si++ ) {
				bplanes[si] = newPlanes[ newSides[brFirstNewSide + si].planeNum ];
			}
			// Rebuild windings for ALL sides (box bevels now have proper planes).
			for ( si = 0; si < newBnumSides; si++ ) {
				Q1_CreateBrushWinding( bplanes, newBnumSides, si, &windings[si] );
			}
			// Update bnumSides for edge-bevel pass.
			bnumSides = newBnumSides;
		}

		// Iterate non-axial sides (positions 6 and beyond).
		for ( si = 6; si < bnumSides; si++ ) {
			BevelWinding_t *w = &windings[si];
			int j;

			if ( w->numPts < 2 ) continue;

			for ( j = 0; j < w->numPts; j++ ) {
				// Edge direction: from pts[j] to pts[(j+1)%numPts]
				int    jnext = (j + 1) % w->numPts;
				float  vec[3];
				float  vecLen;
				int    eaxis, edir;

				vec[0] = w->pts[j][0] - w->pts[jnext][0];
				vec[1] = w->pts[j][1] - w->pts[jnext][1];
				vec[2] = w->pts[j][2] - w->pts[jnext][2];

				vecLen = sqrtf( vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2] );
				if ( vecLen < BEVEL_EDGE_DEGENERATE ) continue;
				vec[0]/=vecLen; vec[1]/=vecLen; vec[2]/=vecLen;

				Q1_SnapNormal( vec );

				// Skip axis-aligned edges (they would only duplicate box bevels).
				if ( (vec[0] == -1.0f || vec[0] == 1.0f) ||
				     (vec[1] == -1.0f || vec[1] == 1.0f) ||
				     (vec[2] == -1.0f || vec[2] == 1.0f) ||
				     (vec[0] == 0.0f && vec[1] == 0.0f)  ||
				     (vec[1] == 0.0f && vec[2] == 0.0f)  ||
				     (vec[2] == 0.0f && vec[0] == 0.0f) )
					continue;

				// Try the 6 slanted axials: cross(vec, axisVec*dir).
				for ( eaxis = 0; eaxis < 3; eaxis++ ) {
					for ( edir = -1; edir <= 1; edir += 2 ) {
						float vec2[3] = {0.0f, 0.0f, 0.0f};
						float candNormal[3];
						float candDist;
						float nlen;
						int   allBehind;
						int   k;
						int   planeIdx;

						vec2[eaxis] = (float)edir;

						// candidate normal = cross(vec, vec2)
						candNormal[0] = vec[1]*vec2[2] - vec[2]*vec2[1];
						candNormal[1] = vec[2]*vec2[0] - vec[0]*vec2[2];
						candNormal[2] = vec[0]*vec2[1] - vec[1]*vec2[0];

						nlen = sqrtf( candNormal[0]*candNormal[0] +
						              candNormal[1]*candNormal[1] +
						              candNormal[2]*candNormal[2] );
						if ( nlen < BEVEL_EDGE_DEGENERATE ) continue;
						candNormal[0]/=nlen; candNormal[1]/=nlen; candNormal[2]/=nlen;

						Q1_SnapNormal( candNormal );

						// dist = dot(winding vertex j, candNormal)
						candDist = w->pts[j][0]*candNormal[0] +
						           w->pts[j][1]*candNormal[1] +
						           w->pts[j][2]*candNormal[2];

						// Deduplicate: check if this plane already exists as a side.
						{
							qboolean dup = qfalse;
							for ( k = 0; k < bnumSides; k++ ) {
								const dplane_t *ep = &newPlanes[ newSides[brFirstNewSide + k].planeNum ];
								if ( fabsf(ep->normal[0]-candNormal[0]) < BEVEL_NORMAL_EPSILON &&
								     fabsf(ep->normal[1]-candNormal[1]) < BEVEL_NORMAL_EPSILON &&
								     fabsf(ep->normal[2]-candNormal[2]) < BEVEL_NORMAL_EPSILON &&
								     fabsf(ep->dist     - candDist    ) < BEVEL_DIST_EPSILON ) {
									dup = qtrue; break;
								}
							}
							if ( dup ) continue;
						}

						// Outer-hull test: all winding vertices of all sides must be
						// on or behind the candidate plane.
						// q3map2 logic: if any vertex is in front (d > 0.1) → reject.
						// If a winding has no points behind (minBack > -0.1) → it lies
						// ON the plane, which is OK (skip with "break" in q3map2 means
						// "this side is on the bevel plane, don't reject because of it").
						allBehind = 1;
						for ( k = 0; k < bnumSides && allBehind; k++ ) {
							BevelWinding_t *wk = &windings[k];
							int             vi;
							float           minBack = 0.0f;
							int             inFront = 0;

							if ( wk->numPts == 0 ) continue;

							for ( vi = 0; vi < wk->numPts; vi++ ) {
								float d = wk->pts[vi][0]*candNormal[0] +
								          wk->pts[vi][1]*candNormal[1] +
								          wk->pts[vi][2]*candNormal[2] - candDist;
								if ( d > BEVEL_WINDING_EPSILON ) { inFront = 1; break; }
								if ( d < minBack ) minBack = d;
							}

							if ( inFront ) { allBehind = 0; break; }
							// if minBack > -0.1 → winding is coplanar with bevel → OK (continue)
						}

						if ( !allBehind ) continue;

						// Valid edge bevel — add it.
						planeIdx = Q1_FindOrAddPlane( newPlanes, &numNewPlanes,
						                              candNormal, candDist );

						if ( bnumSides >= BEVEL_MAX_BRUSH_SIDES ) goto next_brush;

						memset( &newSides[newSideIdx], 0, sizeof( dbrushside_t ) );
						newSides[newSideIdx].planeNum  = planeIdx;
						newSides[newSideIdx].shaderNum = br->shaderNum;
						newSideIdx++;
						brNumSides++;
						totalEdgeBevels++;

						// Append the new plane/winding so subsequent edges test
						// against it (prevents adding duplicate slants from other edges).
						bplanes[bnumSides]  = newPlanes[planeIdx];
						Q1_CreateBrushWinding( bplanes, bnumSides + 1, bnumSides,
						                       &windings[bnumSides] );
						bnumSides++;
					}
				}
			}
		}

next_brush:
		br->numSides = brNumSides;
		/* DIAGB: log firstSide/numSides/planes for first 3 brushes + brushes 710 and 631 */
		if ( bi < 3 || bi == 710 || bi == 631 ) {
			int dk;
			Com_Log( SEV_TRACE, LOG_CH(ch_loading),
			    "DIAGB brush[%d]: firstSide=%d numSides=%d btFound=%d\n",
			    bi, (int)br->firstSide, brNumSides, (int)btFound );
			for ( dk = 0; dk < brNumSides && dk < 4; dk++ ) {
				const dbrushside_t *ds = &newSides[br->firstSide + dk];
				const dplane_t     *dp = &newPlanes[ds->planeNum];
				Com_Log( SEV_TRACE, LOG_CH(ch_loading),
				    "  DIAGB   side[%d] pn=%d n=(%.3f,%.3f,%.3f) dist=%.3f\n",
				    dk, ds->planeNum,
				    dp->normal[0], dp->normal[1], dp->normal[2], dp->dist );
			}
		}
		/* DIAGB end */
#undef BEVEL_MAX_BRUSH_SIDES
	}

	Z_Free( bsp->brushSides );
	Z_Free( bsp->planes );
	bsp->brushSides    = newSides;
	bsp->numBrushSides = newSideIdx;
	bsp->planes        = newPlanes;
	bsp->numPlanes     = numNewPlanes;

	Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_AddBrushBevels: %d brushes processed, "
	         "%d box bevels + %d edge bevels added, "
	         "%d degenerate brushes skipped\n",
	         numBrushes, totalBoxBevels, totalEdgeBevels, numDegenerateBrushes );
}

// ---------------------------------------------------------------------------
// World shell brush synthesis
// ---------------------------------------------------------------------------
// Q1 hull-1 uses half-space clipnodes for the outer solid boundary of each map.
// These produce "degenerate" brushes (no bounded vertex polytope), so Step 9's
// AABB-based tree-walk insertion skips them.  This function synthesises 6
// axis-aligned slab brushes that replace the missing solid shell.
//
// Each slab sits SD units outside bsp->subModels[0].mins/maxs.  After Step 8.5
// de-expansion the effective blocking distance shrinks by ≤32 units but stays
// well outside the playable area.  Q3 CM re-expansion restores them to the
// correct hull-1 blocking boundary at runtime.
//
// Must be called AFTER Step 8 (brush dedup) so firstBrush/numBrushes are set,
// and BEFORE Step 9 (tree walk) so the new brushes get inserted into leaves.
static void BSP_Q1_SynthesizeWorldShell( bspFile_t *bsp )
{
	static const float SD = 256.0f;
	int clipShaderIdx, axis, sign;
	int oldNB, oldNS, oldNP;
	int brushIdx, sideIdx, planeIdx;
	dbrush_t     *newBrushes;
	dbrushside_t *newSides;
	dplane_t     *newPlanes;
	vec3_t        wmins, wmaxs;

	if ( bsp->numSubModels < 1 ) return;

	clipShaderIdx = bsp->numShaders - 1;  // *q1_hull1_clip added last in Step 6
	VectorCopy( bsp->subModels[0].mins, wmins );
	VectorCopy( bsp->subModels[0].maxs, wmaxs );

	oldNB = bsp->numBrushes;
	oldNS = bsp->numBrushSides;
	oldNP = bsp->numPlanes;

	newBrushes = (dbrush_t     *)Z_Malloc( ( oldNB + 6  ) * sizeof( dbrush_t     ) );
	newSides   = (dbrushside_t *)Z_Malloc( ( oldNS + 36 ) * sizeof( dbrushside_t ) );
	newPlanes  = (dplane_t     *)Z_Malloc( ( oldNP + 36 ) * sizeof( dplane_t     ) );

	memcpy( newBrushes, bsp->brushes,    oldNB * sizeof( dbrush_t     ) );
	memcpy( newSides,   bsp->brushSides, oldNS * sizeof( dbrushside_t ) );
	memcpy( newPlanes,  bsp->planes,     oldNP * sizeof( dplane_t     ) );

	Z_Free( bsp->brushes    ); bsp->brushes    = newBrushes;
	Z_Free( bsp->brushSides ); bsp->brushSides = newSides;
	Z_Free( bsp->planes     ); bsp->planes     = newPlanes;

	brushIdx = oldNB;
	sideIdx  = oldNS;
	planeIdx = oldNP;

	for ( axis = 0; axis < 3; axis++ ) {
		int ax1 = ( axis + 1 ) % 3;
		int ax2 = ( axis + 2 ) % 3;

		for ( sign = 0; sign < 2; sign++ ) {
			float ref  = ( sign == 0 ) ? wmins[axis] : wmaxs[axis];
			// ndir: direction the main face normal points (inward = toward world interior)
			float ndir = ( sign == 0 ) ? 1.0f : -1.0f;
			dbrush_t *b = &bsp->brushes[brushIdx++];
			b->firstSide = sideIdx;
			b->numSides  = 6;
			b->shaderNum = clipShaderIdx;

#define SHELL_SIDE( nx, ny, nz, plDist ) \
			do { \
				dplane_t     *pl   = &bsp->planes[planeIdx]; \
				dbrushside_t *side = &bsp->brushSides[sideIdx++]; \
				memset( pl, 0, sizeof(*pl) ); \
				pl->normal[0] = (nx); pl->normal[1] = (ny); pl->normal[2] = (nz); \
				pl->dist = (plDist); \
				side->planeNum  = planeIdx++; \
				side->shaderNum = clipShaderIdx; \
			} while(0)

			// s0: main face — inward normal at ref ± SD (blocks void escape)
			{
				float d = ( sign == 0 ) ? ( ref - SD ) : -( ref + SD );
				float nx = (axis==0) ? ndir : 0.0f;
				float ny = (axis==1) ? ndir : 0.0f;
				float nz = (axis==2) ? ndir : 0.0f;
				SHELL_SIDE( nx, ny, nz, d );
			}
			// s1: opposite cap — closes the slab 2*SD further out
			{
				float d = ( sign == 0 ) ? -( ref - 2.0f*SD ) : ( ref + 2.0f*SD );
				float nx = (axis==0) ? -ndir : 0.0f;
				float ny = (axis==1) ? -ndir : 0.0f;
				float nz = (axis==2) ? -ndir : 0.0f;
				SHELL_SIDE( nx, ny, nz, d );
			}
			// s2/s3: bound along first perpendicular axis
			SHELL_SIDE( ax1==0 ? 1.0f:0.0f, ax1==1 ? 1.0f:0.0f, ax1==2 ? 1.0f:0.0f,   wmaxs[ax1]+SD );
			SHELL_SIDE( ax1==0 ?-1.0f:0.0f, ax1==1 ?-1.0f:0.0f, ax1==2 ?-1.0f:0.0f, -(wmins[ax1]-SD) );
			// s4/s5: bound along second perpendicular axis
			SHELL_SIDE( ax2==0 ? 1.0f:0.0f, ax2==1 ? 1.0f:0.0f, ax2==2 ? 1.0f:0.0f,   wmaxs[ax2]+SD );
			SHELL_SIDE( ax2==0 ?-1.0f:0.0f, ax2==1 ?-1.0f:0.0f, ax2==2 ?-1.0f:0.0f, -(wmins[ax2]-SD) );
#undef SHELL_SIDE
		}
	}

	bsp->numBrushes    = brushIdx;    // oldNB + 6
	bsp->numBrushSides = sideIdx;
	bsp->numPlanes     = planeIdx;

	// Shell brushes were appended at oldNB..oldNB+5, which is AFTER any
	// non-worldspawn (door/plat/button) submodel brushes that the dedup pass
	// compacted into indices just below oldNB.  Naively doing numBrushes+=6
	// would stretch worldspawn's range over those submodel brushes, causing
	// Step 9's tree walk to bake them into world BSP leaves as permanent solids.
	//
	// Fix: insert the 6 shell brushes at worldEnd (= subModels[0].firstBrush +
	// numBrushes), shift any trailing non-worldspawn brushes up by 6, and
	// update every non-zero submodel's firstBrush accordingly.  Worldspawn's
	// range then covers only its own canonicals + these 6 shells.
	{
		int worldEnd = bsp->subModels[0].firstBrush + bsp->subModels[0].numBrushes;
		int tail     = oldNB - worldEnd;  // non-worldspawn brushes between worldEnd and oldNB
		if ( tail > 0 ) {
			int        i;
			dbrush_t   saved[6];
			memcpy( saved, &bsp->brushes[oldNB], 6 * sizeof(dbrush_t) );
			memmove( &bsp->brushes[worldEnd + 6], &bsp->brushes[worldEnd],
			         tail * sizeof(dbrush_t) );
			memcpy( &bsp->brushes[worldEnd], saved, 6 * sizeof(dbrush_t) );
			for ( i = 1; i < bsp->numSubModels; i++ )
				bsp->subModels[i].firstBrush += 6;
		}
	}
	bsp->subModels[0].numBrushes += 6;

	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: synthesized 6 world shell brushes "
	         "(bounds (%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) margin=%.0f)\n",
	         wmins[0], wmins[1], wmins[2], wmaxs[0], wmaxs[1], wmaxs[2], SD );
}

// ---------------------------------------------------------------------------
// Liquid brush synthesis
// ---------------------------------------------------------------------------

/* Register or find a shader by name; returns its index in bsp->shaders[]. */
static int BSP_Q1_FindOrAddLiquidShader( bspFile_t *bsp, const char *sname,
                                          int surfFlags, int contFlags ) {
	int i;
	dshader_t *ns;
	for ( i = 0; i < bsp->numShaders; i++ ) {
		if ( !Q_stricmp( bsp->shaders[i].shader, sname ) )
			return i;
	}
	ns = (dshader_t *)Z_Malloc( ( bsp->numShaders + 1 ) * sizeof( dshader_t ) );
	memcpy( ns, bsp->shaders, bsp->numShaders * sizeof( dshader_t ) );
	Z_Free( bsp->shaders );
	bsp->shaders = ns;
	i = bsp->numShaders++;
	Q_strncpyz( bsp->shaders[i].shader, sname, sizeof( bsp->shaders[i].shader ) );
	bsp->shaders[i].surfaceFlags = surfFlags;
	bsp->shaders[i].contentFlags = contFlags;
	return i;
}

/* Emit one AABB brush per hull-0 liquid leaf so PM_SetWaterLevel and
 * the damage system see Q3 CONTENTS_WATER/SLIME/LAVA in those volumes.
 * Called AFTER SynthesizeWorldShell (brushes/sides/planes arrays exist)
 * and BEFORE Step 9 (tree walk assigns brushes to leaves). */
static void BSP_Q1_SynthesizeLiquidBrushes( bspFile_t *bsp,
                                              const byte *base,
                                              const lump_t *leafLump ) {
	const q1_dleaf_t *rawLeafs;
	int numRaw, numLiquid, i;
	int waterShader, slimeShader, lavaShader;
	int oldNB, oldNS, oldNP, addB, addS, addP;
	int brushIdx, sideIdx, planeIdx;
	int nWater, nSlime, nLava;
	dbrush_t     *newBrushes;
	dbrushside_t *newSides;
	dplane_t     *newPlanes;

	rawLeafs = (const q1_dleaf_t *)( base + leafLump->fileofs );
	numRaw   = leafLump->filelen / (int)sizeof( q1_dleaf_t );

	numLiquid = 0;
	for ( i = 0; i < numRaw; i++ ) {
		int c = LittleLong( rawLeafs[i].contents );
		if ( c == -3 || c == -4 || c == -5 ) numLiquid++;
	}
	if ( numLiquid == 0 || bsp->numSubModels < 1 ) return;

	waterShader = BSP_Q1_FindOrAddLiquidShader( bsp, "*q1_water", SURF_NODRAW, CONTENTS_WATER );
	slimeShader = BSP_Q1_FindOrAddLiquidShader( bsp, "*q1_slime", SURF_NODRAW, CONTENTS_SLIME );
	lavaShader  = BSP_Q1_FindOrAddLiquidShader( bsp, "*q1_lava",  SURF_NODRAW, CONTENTS_LAVA  );

	oldNB = bsp->numBrushes;
	oldNS = bsp->numBrushSides;
	oldNP = bsp->numPlanes;
	addB  = numLiquid;
	addS  = numLiquid * 6;
	addP  = numLiquid * 6;

	newBrushes = (dbrush_t     *)Z_Malloc( ( oldNB + addB ) * sizeof( dbrush_t     ) );
	newSides   = (dbrushside_t *)Z_Malloc( ( oldNS + addS ) * sizeof( dbrushside_t ) );
	newPlanes  = (dplane_t     *)Z_Malloc( ( oldNP + addP ) * sizeof( dplane_t     ) );

	memcpy( newBrushes, bsp->brushes,    oldNB * sizeof( dbrush_t     ) );
	memcpy( newSides,   bsp->brushSides, oldNS * sizeof( dbrushside_t ) );
	memcpy( newPlanes,  bsp->planes,     oldNP * sizeof( dplane_t     ) );

	Z_Free( bsp->brushes );    bsp->brushes    = newBrushes;
	Z_Free( bsp->brushSides ); bsp->brushSides = newSides;
	Z_Free( bsp->planes );     bsp->planes     = newPlanes;

	brushIdx = oldNB;
	sideIdx  = oldNS;
	planeIdx = oldNP;
	nWater = nSlime = nLava = 0;

	for ( i = 0; i < numRaw && i < bsp->numLeafs; i++ ) {
		int c = LittleLong( rawLeafs[i].contents );
		int shaderIdx, axis, sign;
		vec3_t mins, maxs;
		dbrush_t *b;

		if      ( c == -3 ) { shaderIdx = waterShader; nWater++; }
		else if ( c == -4 ) { shaderIdx = slimeShader; nSlime++; }
		else if ( c == -5 ) { shaderIdx = lavaShader;  nLava++;  }
		else continue;

		mins[0] = (float)bsp->leafs[i].mins[0];
		mins[1] = (float)bsp->leafs[i].mins[1];
		mins[2] = (float)bsp->leafs[i].mins[2];
		maxs[0] = (float)bsp->leafs[i].maxs[0];
		maxs[1] = (float)bsp->leafs[i].maxs[1];
		maxs[2] = (float)bsp->leafs[i].maxs[2];

		b = &bsp->brushes[brushIdx++];
		b->firstSide = sideIdx;
		b->numSides  = 6;
		b->shaderNum = shaderIdx;

		for ( axis = 0; axis < 3; axis++ ) {
			for ( sign = 0; sign < 2; sign++ ) {
				dplane_t     *pl   = &bsp->planes[planeIdx];
				dbrushside_t *side = &bsp->brushSides[sideIdx++];
				memset( pl, 0, sizeof( *pl ) );
				pl->normal[axis] = ( sign == 0 ) ? 1.0f : -1.0f;
				pl->dist         = ( sign == 0 ) ? maxs[axis] : -mins[axis];
				side->planeNum   = planeIdx++;
				side->shaderNum  = shaderIdx;
			}
		}
	}

	bsp->numBrushes    = brushIdx;
	bsp->numBrushSides = sideIdx;
	bsp->numPlanes     = planeIdx;

	// Insert liquid brushes at worldEnd (after worldspawn + shell brushes)
	// using the same memmove/insert pattern as BSP_Q1_SynthesizeWorldShell.
	{
		int worldEnd = bsp->subModels[0].firstBrush + bsp->subModels[0].numBrushes;
		int tail     = oldNB - worldEnd;
		if ( tail > 0 ) {
			int j;
			dbrush_t *saved = (dbrush_t *)Z_Malloc( numLiquid * sizeof( dbrush_t ) );
			memcpy( saved, &bsp->brushes[oldNB], numLiquid * sizeof( dbrush_t ) );
			memmove( &bsp->brushes[worldEnd + numLiquid], &bsp->brushes[worldEnd],
			         tail * sizeof( dbrush_t ) );
			memcpy( &bsp->brushes[worldEnd], saved, numLiquid * sizeof( dbrush_t ) );
			Z_Free( saved );
			for ( j = 1; j < bsp->numSubModels; j++ )
				bsp->subModels[j].firstBrush += numLiquid;
		}
	}
	bsp->subModels[0].numBrushes += numLiquid;

	if ( nWater + nSlime + nLava > 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_Load: synthesized %d liquid brushes "
		         "(%d water, %d slime, %d lava)\n",
		         nWater + nSlime + nLava, nWater, nSlime, nLava );
	}
}

// ---------------------------------------------------------------------------
// Entity-string diagnostic helper
// ---------------------------------------------------------------------------

/* Scan entityString and log classname + all key-value pairs for every entity
   whose "model" key matches one of the NULL-terminated targets[] strings. */
static void BSP_Q1_LogEntitiesByModel( const char *entityString,
                                        const char * const *targets ) {
	const char *p = entityString;
	int         entNum = 0;

	if ( !p ) return;

	while ( *p ) {
		const char *bstart = strchr( p, '{' );
		if ( !bstart ) break;
		const char *bend = strchr( bstart + 1, '}' );
		if ( !bend ) break;

		/* Check whether any target model key appears inside this block. */
		qboolean hit = qfalse;
		int ti;
		for ( ti = 0; targets[ti]; ti++ ) {
			char needle[32];
			Com_sprintf( needle, sizeof( needle ), "\"model\" \"%s\"", targets[ti] );
			const char *found = strstr( bstart, needle );
			if ( found && found < bend ) { hit = qtrue; break; }
		}

		if ( hit ) {
			Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load entity #%d:\n", entNum );
			const char *kp = bstart + 1;
			while ( kp < bend ) {
				/* Skip whitespace / newlines */
				while ( kp < bend && ( *kp == ' ' || *kp == '\t' ||
				                       *kp == '\n' || *kp == '\r' ) ) kp++;
				if ( kp >= bend || *kp == '}' ) break;
				if ( *kp != '"' ) { kp++; continue; }

				/* Read key */
				char key[64] = { 0 };
				kp++;   /* skip opening quote */
				int ki = 0;
				while ( kp < bend && *kp != '"' && ki < 63 ) key[ki++] = *kp++;
				key[ki] = '\0';
				if ( kp < bend ) kp++;   /* skip closing quote */

				/* Skip inter-token whitespace */
				while ( kp < bend && ( *kp == ' ' || *kp == '\t' ) ) kp++;

				/* Read value */
				char val[256] = { 0 };
				if ( kp < bend && *kp == '"' ) {
					kp++;   /* skip opening quote */
					int vi = 0;
					while ( kp < bend && *kp != '"' && vi < 255 ) val[vi++] = *kp++;
					val[vi] = '\0';
					if ( kp < bend ) kp++;   /* skip closing quote */
				}

				if ( key[0] )
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  \"%s\" \"%s\"\n", key, val );
			}
		}

		p = bend + 1;
		entNum++;
	}
}

// ---------------------------------------------------------------------------
// Entity string classname prefixing
// ---------------------------------------------------------------------------

static void BSP_Q1_PrefixClassnames( bspFile_t *bsp ) {
	static const char KEY[]       = "\"classname\"";
	static const char PREFIX[]    = "q1_";
	static const char ORIGIN_KV[] = "\n\"q1Origin\" \"1\"";
	const int         keyLen      = (int)sizeof(KEY) - 1;       // 11
	const int         pfxLen      = (int)sizeof(PREFIX) - 1;    // 3
	const int         originLen   = (int)sizeof(ORIGIN_KV) - 1; // 15
	const char       *src         = bsp->entityString;
	int               srcLen      = bsp->entityStringLength;
	int               cnCount     = 0;  // classname values needing prefix
	int               blCount     = 0;  // entity block '{' count (for q1Origin injection)
	const char       *p;
	char             *dst, *d;

	// First pass: count both types of insertion
	for ( p = src; *p; p++ ) {
		if ( *p == '{' ) blCount++;
	}
	p = src;
	while ( *p ) {
		const char *found = strstr( p, KEY );
		if ( !found ) break;
		p = found + keyLen;
		while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) p++;
		if ( *p == '"' ) cnCount++;
	}
	if ( cnCount == 0 && blCount == 0 ) return;

	dst = Z_Malloc( srcLen + cnCount * pfxLen + blCount * originLen + 1 );
	if ( !dst ) return;
	d = dst;
	p = src;

	// Second pass: unified copy with both insertions
	while ( *p ) {
		const char *nextBrace = strchr( p, '{' );
		const char *nextKey   = strstr( p, KEY );
		qboolean    hitBrace;
		const char *nextHit;

		if ( nextBrace && nextKey ) {
			hitBrace = (nextBrace <= nextKey) ? qtrue : qfalse;
			nextHit  = hitBrace ? nextBrace : nextKey;
		} else if ( nextBrace ) {
			hitBrace = qtrue;
			nextHit  = nextBrace;
		} else if ( nextKey ) {
			hitBrace = qfalse;
			nextHit  = nextKey;
		} else {
			while ( *p ) *d++ = *p++;
			break;
		}

		if ( hitBrace ) {
			// Copy up to and including '{', then inject q1Origin key-value
			int span = (int)(nextHit - p) + 1;
			memcpy( d, p, span );
			d += span;
			p  = nextHit + 1;
			memcpy( d, ORIGIN_KV, originLen );
			d += originLen;
		} else {
			// Copy up to and including KEY, then handle classname prefix
			int span = (int)(nextHit - p) + keyLen;
			memcpy( d, p, span );
			d += span;
			p = nextHit + keyLen;
			while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) *d++ = *p++;
			if ( *p == '"' ) {
				/* worldspawn keeps its bare classname */
				if ( strncmp( p + 1, "worldspawn\"", 11 ) == 0 ) {
					*d++ = *p++;
				} else {
					*d++ = *p++;
					memcpy( d, PREFIX, pfxLen );
					d += pfxLen;
				}
			}
		}
	}
	*d = '\0';

	Z_Free( bsp->entityString );
	bsp->entityString       = dst;
	bsp->entityStringLength = (int)( d - dst );
}

// ---------------------------------------------------------------------------
// Q1 liquid → Q3 shader remap table
// ---------------------------------------------------------------------------

typedef struct {
	const char *q1Prefix;     /* miptex name prefix, e.g. "*water" */
	const char *targetShader; /* Q3 shader path to assign instead   */
} liquidRemap_t;

static const liquidRemap_t s_liquidRemaps[] = {
	{ "*water0",    "textures/liquids/clear_ripple1" },
	{ "*lava",   	"textures/liquids/lavacrust"     },
	{ "*slime",  	"textures/liquids/slime2"        },
	{ "*tele",   	"textures/liquids/teleporter_q1" },
	{ "*",       	"textures/liquids/clear_ripple1" }, /* catch-all: any other * liquid */
};
#define NUM_LIQUID_REMAPS ( (int)( sizeof(s_liquidRemaps) / sizeof(s_liquidRemaps[0]) ) )

// ---------------------------------------------------------------------------
// PVS post-processing
// ---------------------------------------------------------------------------

// AABB touch test with 1-unit tolerance — mirrors Q1_MnodesAdjacent in tr_bsp.c
static qboolean Q1_RawLeafAABBTouch( const dleaf_t *a, const dleaf_t *b ) {
	int d;
	for ( d = 0; d < 3; d++ )
		if ( a->maxs[d] + 1 < b->mins[d] || b->maxs[d] + 1 < a->mins[d] )
			return qfalse;
	return qtrue;
}

/*
=================
Q1_PostProcessPVS

Two passes applied to the decompressed Q1 PVS, after leaf and marksurface data
are fully loaded but before the renderer reads the visibility lump.

Pass 1 — Q1_ExpandPVSWithNeighbours
  For each cluster i, OR the PVS of every spatially adjacent cluster into i's
  PVS row.  Adjacent clusters are detected by AABB touch (1-unit tolerance),
  which is exact for the snug BSP leaf grid.  This softens sharp pop-in at PVS
  cell boundaries where the Q1 vis compiler created hard edges.

Pass 2 — Q1_MergeLiquidPVS  (two-hop)
  For each liquid cluster L, for each non-liquid cluster N1 adjacent to L:
    - L.pvs  |= N1.pvs  (liquid sees the room above)
    - N1.pvs |= L.pvs   (one-hop neighbour sees through liquid surface)
    - For each non-liquid cluster N2 adjacent to N1 (excluding L itself):
        N2.pvs |= L.pvs  (two-hop: standing-area leaf above rim also sees water)

  The two-hop extension is needed because the typical Q1 pool layout is
  three layers deep: [water leaf] → [rim leaf] → [standing-area leaf].
  Pass 1 cannot bridge this gap because these are all distinct PVS cells.
  The liquid→non-liquid direction (L.pvs |= N1.pvs) does not need a second
  hop: once the player is submerged, Q1's native PVS handles the rest.

Pass 2 runs after Pass 1 so that liquid leaves absorb the already-widened PVS
of their air neighbours.

Adjacency is AABB-touch, not shared-surface: Q1 faces are one-sided and belong
to exactly one leaf, so two adjacent leaves rarely share any marksurface index.
AABB-touch is O(numClusters²) but runs once at load time; for Q1 maps this is
≤4M comparisons and completes in well under a millisecond.
=================
*/
static void Q1_PostProcessPVS( bspFile_t        *bsp,
                                const q1_dleaf_t *rawLeafs ) {
	const int  numClusters  = bsp->numClusters;
	const int  clusterBytes = bsp->clusterBytes;
	int        i, j, k, b;
	byte      *scratch;

	if ( numClusters <= 0 || clusterBytes <= 0 || !bsp->visibility )
		return;

	scratch = (byte *)Z_Malloc( (size_t)numClusters * clusterBytes );

	// ---- Pass 1: widen every cluster's PVS with all adjacent clusters ----
	memcpy( scratch, bsp->visibility, (size_t)numClusters * clusterBytes );

	for ( i = 0; i < numClusters; i++ ) {
		const dleaf_t *li  = &bsp->leafs[i + 1];
		byte          *dst = scratch + (size_t)i * clusterBytes;

		for ( j = 0; j < numClusters; j++ ) {
			if ( j == i ) continue;
			const dleaf_t *lj = &bsp->leafs[j + 1];

			if ( !Q1_RawLeafAABBTouch( li, lj ) ) continue;

			const byte *src = bsp->visibility + (size_t)j * clusterBytes;
			for ( b = 0; b < clusterBytes; b++ )
				dst[b] |= src[b];
		}
	}
	memcpy( bsp->visibility, scratch, (size_t)numClusters * clusterBytes );

	// ---- Pass 2: liquid clusters absorb non-liquid neighbour PVS ----
	memcpy( scratch, bsp->visibility, (size_t)numClusters * clusterBytes );

	for ( i = 0; i < numClusters; i++ ) {
		const int      leafIdx = i + 1;
		const int      c       = LittleLong( rawLeafs[leafIdx].contents );
		if ( c != -3 && c != -4 && c != -5 ) continue; // skip non-liquid

		const dleaf_t *li  = &bsp->leafs[leafIdx];
		byte          *dst = scratch + (size_t)i * clusterBytes;

		for ( j = 0; j < numClusters; j++ ) {
			if ( j == i ) continue;
			const int nc = LittleLong( rawLeafs[j + 1].contents );
			if ( nc == -3 || nc == -4 || nc == -5 ) continue; // skip liquid

			const dleaf_t *lj  = &bsp->leafs[j + 1];
			if ( !Q1_RawLeafAABBTouch( li, lj ) ) continue;

			const byte *srcLiquid    = bsp->visibility + (size_t)i * clusterBytes;
			const byte *srcNonLiquid = bsp->visibility + (size_t)j * clusterBytes;
			byte       *dstNonLiquid = scratch          + (size_t)j * clusterBytes;
			for ( b = 0; b < clusterBytes; b++ ) {
				dst[b]          |= srcNonLiquid[b]; // liquid sees above-water
				dstNonLiquid[b] |= srcLiquid[b];   // one-hop neighbour sees liquid
			}

			// Two-hop: non-liquid clusters adjacent to N1 also absorb liquid PVS.
			// Only the non-liquid→liquid direction needs this extra reach.
			for ( k = 0; k < numClusters; k++ ) {
				if ( k == i || k == j ) continue;
				const int nc2 = LittleLong( rawLeafs[k + 1].contents );
				if ( nc2 == -3 || nc2 == -4 || nc2 == -5 ) continue;
				const dleaf_t *lk = &bsp->leafs[k + 1];
				if ( !Q1_RawLeafAABBTouch( lj, lk ) ) continue;
				byte *dstN2 = scratch + (size_t)k * clusterBytes;
				for ( b = 0; b < clusterBytes; b++ )
					dstN2[b] |= srcLiquid[b];
			}
		}
	}
	memcpy( bsp->visibility, scratch, (size_t)numClusters * clusterBytes );

	Z_Free( scratch );

	Com_Log( SEV_TRACE, LOG_CH(ch_loading),
		"Q1_PostProcessPVS: %d clusters x %d clusterBytes, "
		"pop-in + liquid PVS expansion applied\n",
		numClusters, clusterBytes );
}

// ---------------------------------------------------------------------------
// Main loader
// ---------------------------------------------------------------------------

static qboolean BSP_Q1_Load( const bspFormat_t *format, const char *name,
	const void *data, int length, unsigned flags, bspFile_t **bspFile ) {
	int           i, j;
	q1_dheader_t  header;
	bspFile_t     *bsp;
	const byte    *base;
	int           diagEnabled;
	diagEnabled = Cvar_VariableIntegerValue( "q1_trace_debug" );

	*bspFile = NULL;

	if ( length < (int)sizeof( q1_dheader_t ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s has truncated header\n", name );
		return qfalse;
	}

	memcpy( &header, data, sizeof( q1_dheader_t ) );
	header.version = LittleLong( header.version );
	for ( i = 0; i < HEADER_LUMPS_Q1; i++ ) {
		header.lumps[i].fileofs = LittleLong( header.lumps[i].fileofs );
		header.lumps[i].filelen = LittleLong( header.lumps[i].filelen );
	}

	if ( header.version != BSP_VERSION_Q1 ) {
		return qfalse;
	}

	for ( i = 0; i < HEADER_LUMPS_Q1; i++ ) {
		uint32_t ofs = header.lumps[i].fileofs;
		uint32_t len = header.lumps[i].filelen;
		if ( (uint64_t)ofs + len > (uint64_t)length ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s lump %i out of range (ofs=%u len=%u file=%i)\n",
				name, i, ofs, len, length );
			return qfalse;
		}
	}

	base = (const byte *)data;

	bsp = BSP_Q1_ZAlloc( sizeof( *bsp ) );
	memset( bsp, 0, sizeof( *bsp ) );
	Q_strncpyz( bsp->name, name, sizeof( bsp->name ) );
	bsp->ident    = 0;  // Q1 has no ident field
	bsp->version  = header.version;
	bsp->checksum = LittleLong( Com_BlockChecksum( data, length ) );
	// rawData/rawLength set at end by BSP_Q1_BuildSyntheticQ3Data

	// Q1 supports 4 lightstyle slots per face (blended at render time)
	bsp->lightmapStyles = 4;

	Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_Load: %s version=%d lumps:\n", name, header.version );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  entities:     %8d bytes\n",
		header.lumps[LUMP_Q1_ENTITIES].filelen );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  planes:       %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_PLANES].filelen,
		header.lumps[LUMP_Q1_PLANES].filelen / (int)sizeof( q1_dplane_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  textures:     %8d bytes\n",
		header.lumps[LUMP_Q1_TEXTURES].filelen );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  vertexes:     %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_VERTEXES].filelen,
		header.lumps[LUMP_Q1_VERTEXES].filelen / (int)sizeof( q1_dvertex_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  visibility:   %8d bytes\n",
		header.lumps[LUMP_Q1_VISIBILITY].filelen );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  nodes:        %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_NODES].filelen,
		header.lumps[LUMP_Q1_NODES].filelen / (int)sizeof( q1_dnode_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  texinfo:      %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_TEXINFO].filelen,
		header.lumps[LUMP_Q1_TEXINFO].filelen / (int)sizeof( q1_dtexinfo_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  faces:        %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_FACES].filelen,
		header.lumps[LUMP_Q1_FACES].filelen / (int)sizeof( q1_dface_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  lighting:     %8d bytes\n",
		header.lumps[LUMP_Q1_LIGHTING].filelen );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  clipnodes:    %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_CLIPNODES].filelen,
		header.lumps[LUMP_Q1_CLIPNODES].filelen / (int)sizeof( q1_dclipnode_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  leafs:        %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_LEAFS].filelen,
		header.lumps[LUMP_Q1_LEAFS].filelen / (int)sizeof( q1_dleaf_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  marksurfaces: %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_MARKSURFACES].filelen,
		header.lumps[LUMP_Q1_MARKSURFACES].filelen / (int)sizeof( short ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  edges:        %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_EDGES].filelen,
		header.lumps[LUMP_Q1_EDGES].filelen / (int)sizeof( q1_dedge_t ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  surfedges:    %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_SURFEDGES].filelen,
		header.lumps[LUMP_Q1_SURFEDGES].filelen / (int)sizeof( int ) );
	Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  models:       %8d bytes (%d)\n",
		header.lumps[LUMP_Q1_MODELS].filelen,
		header.lumps[LUMP_Q1_MODELS].filelen / (int)sizeof( q1_dmodel_t ) );

	// ---- Entity string ----
	{
		const lump_t *l = &header.lumps[LUMP_Q1_ENTITIES];
		bsp->entityStringLength = l->filelen;
		bsp->entityString = BSP_Q1_ZAlloc( l->filelen + 1 );
		if ( l->filelen ) {
			memcpy( bsp->entityString, base + l->fileofs, l->filelen );
		}
		bsp->entityString[l->filelen] = 0;
		BSP_Q1_PrefixClassnames( bsp );

		{
			static const char * const bridgeTargets[] = { "*3", "*6", "*14", "*15", NULL };
			BSP_Q1_LogEntitiesByModel( bsp->entityString, bridgeTargets );
		}
	}

	// ---- Planes ----
	{
		const lump_t      *l  = &header.lumps[LUMP_Q1_PLANES];
		const q1_dplane_t *in = (const q1_dplane_t *)( base + l->fileofs );
		dplane_t          *out;

		if ( l->filelen % (int)sizeof( q1_dplane_t ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s funny planes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numPlanes = l->filelen / (int)sizeof( q1_dplane_t );
		bsp->planes    = BSP_Q1_ZAlloc( bsp->numPlanes * sizeof( dplane_t ) );
		out = bsp->planes;
		for ( i = 0; i < bsp->numPlanes; i++, in++, out++ ) {
			for ( j = 0; j < 3; j++ ) {
				out->normal[j] = LittleFloat( in->normal[j] );
			}
			out->dist = LittleFloat( in->dist );
			// Q1 plane.type is dropped — Q3 recomputes planeType from normal at use
		}
	}

	// ---- Shaders (from texinfo + miptex names) ----
	{
		const lump_t         *texLump     = &header.lumps[LUMP_Q1_TEXTURES];
		const lump_t         *texinfoLump = &header.lumps[LUMP_Q1_TEXINFO];
		const q1_dtexinfo_t  *tin         = (const q1_dtexinfo_t *)( base + texinfoLump->fileofs );
		dshader_t            *out;

		// Build miptex name table from texture lump
		int   numMipTex = 0;
		char  (*miptexNames)[16] = NULL;

		if ( texLump->filelen >= (int)sizeof( int ) ) {
			const byte *mipBase    = base + texLump->fileofs;
			numMipTex              = LittleLong( *(const int *)mipBase );
			const int  *mipOffsets = (const int *)( mipBase + sizeof( int ) );

			if ( numMipTex > 0 ) {
				miptexNames = Hunk_AllocateTempMemory( numMipTex * 16 );
				for ( i = 0; i < numMipTex; i++ ) {
					int ofs = LittleLong( mipOffsets[i] );
					if ( ofs < 0 || ofs + (int)sizeof( q1_miptex_t ) > texLump->filelen ) {
						Q_strncpyz( miptexNames[i], "noshader", 16 );
						continue;
					}
					const q1_miptex_t *mt = (const q1_miptex_t *)( mipBase + ofs );
					Q_strncpyz( miptexNames[i], mt->name, 16 );
				}
			}
		}

		if ( texinfoLump->filelen % (int)sizeof( q1_dtexinfo_t ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s funny texinfo lump size\n", name );
			if ( miptexNames ) Hunk_FreeTempMemory( miptexNames );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numShaders = texinfoLump->filelen / (int)sizeof( q1_dtexinfo_t );
		bsp->shaders    = BSP_Q1_ZAlloc( bsp->numShaders * sizeof( dshader_t ) );
		out = bsp->shaders;

		for ( i = 0; i < bsp->numShaders; i++, tin++, out++ ) {
			int miptexIdx = LittleLong( tin->miptex );
			const char *texName;
			if ( miptexIdx >= 0 && miptexIdx < numMipTex && miptexNames ) {
				texName = miptexNames[miptexIdx];
			} else {
				texName = "noshader";
			}
			Q_strncpyz( out->shader, texName, sizeof( out->shader ) );

			if ( texName[0] == '*' ) {
				int ri;
				for ( ri = 0; ri < NUM_LIQUID_REMAPS; ri++ ) {
					size_t plen = strlen( s_liquidRemaps[ri].q1Prefix );
					if ( Q_stricmpn( texName, s_liquidRemaps[ri].q1Prefix, plen ) == 0 ) {
						Q_strncpyz( out->shader, s_liquidRemaps[ri].targetShader, sizeof( out->shader ) );
						if ( ri == NUM_LIQUID_REMAPS - 1 ) {
							Com_Log( SEV_WARN, LOG_CH(ch_loading),
							         "BSP_Q1_Load: unmapped liquid '%s' → water\n", texName );
						}
						break;
					}
				}

			}

			out->surfaceFlags = 0;
			out->contentFlags = CONTENTS_SOLID;

			if ( texName[0] == '*' ) {
				// Liquid: water by default; refine by name
				out->contentFlags  = CONTENTS_WATER;
				out->surfaceFlags |= SURF_NONSOLID;
				if ( strstr( texName, "lava" ) )  out->contentFlags = CONTENTS_LAVA;
				if ( strstr( texName, "slime" ) ) out->contentFlags = CONTENTS_SLIME;
			} else if ( !Q_stricmpn( texName, "sky", 3 ) ) {
				out->surfaceFlags |= SURF_SKY | SURF_NOLIGHTMAP;
				out->contentFlags  = CONTENTS_SOLID;
			} else if ( texName[0] == '{' ) {
				out->surfaceFlags |= SURF_ALPHASHADOW;
			}

			// texinfo.flags bit 0 = TEX_SPECIAL (no lightmap on this surface)
			if ( LittleLong( tin->flags ) & 1 ) {
				out->surfaceFlags |= SURF_NOLIGHTMAP;
			}
		}

		if ( miptexNames ) {
			Hunk_FreeTempMemory( miptexNames );
		}
	}

	// ---- BSPX extension: per-vertex / per-face smooth normals ----
	// VERTEXNORMALS (3 floats per Q1 vertex) and FACENORMALS (3 floats per Q1 face)
	// are optional BSPX lumps produced by modern Q1 BSP compilers (ericw-tools, etc.).
	// If present they replace flat face-plane normals on draw verts, enabling per-pixel
	// lighting from dlights and later a directional lightmap stage.
	// Vanilla Q1 maps have neither lump; absence is handled silently.
	const float *bspxVertexNormals    = NULL;
	int          bspxVertexNormalCount = 0;
	const float *bspxFaceNormals      = NULL;
	int          bspxFaceNormalCount  = 0;
	{
		uint32_t endOfs = 0;
		for ( i = 0; i < HEADER_LUMPS_Q1; i++ ) {
			uint32_t e = (uint32_t)header.lumps[i].fileofs + (uint32_t)header.lumps[i].filelen;
			if ( e > endOfs ) endOfs = e;
		}
		uint32_t bspxOfs = ( endOfs + 3u ) & ~3u;

		if ( (int)bspxOfs + 8 <= length ) {
			const byte *p = base + bspxOfs;
			if ( p[0]=='B' && p[1]=='S' && p[2]=='P' && p[3]=='X' ) {
				uint32_t numBspxLumps = LittleLong( *(const int *)(p + 4) );
				// Each BSPX lump directory entry: 24-byte name + uint32 fileofs + uint32 filelen
				uint64_t dirBytes = (uint64_t)numBspxLumps * 32u;
				if ( numBspxLumps <= 256 && (int)( bspxOfs + 8u + dirBytes ) <= length ) {
					const byte *dir = p + 8;
					int nV = (int)( header.lumps[LUMP_Q1_VERTEXES].filelen
					                / (int)sizeof( q1_dvertex_t ) );
					int nF = (int)( header.lumps[LUMP_Q1_FACES].filelen
					                / (int)sizeof( q1_dface_t ) );
					uint32_t n;
					Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_Load: BSPX extension: %u lumps at offset %u\n",
					         numBspxLumps, bspxOfs );
					for ( n = 0; n < numBspxLumps; n++ ) {
						const byte *entry = dir + n * 32;
						char  lname[25];
						memcpy( lname, entry, 24 );
						lname[24] = '\0';
						uint32_t lofs = LittleLong( *(const int *)(entry + 24) );
						uint32_t llen = LittleLong( *(const int *)(entry + 28) );
						if ( (uint64_t)lofs + llen > (uint64_t)length ) {
							Com_Log( SEV_WARN, LOG_CH(ch_loading), "BSP_Q1_Load: BSPX lump '%s' out of range\n", lname );
							continue;
						}
						if ( !strcmp( lname, "VERTEXNORMALS" ) ) {
							int expected = nV * 12;
							if ( (int)llen == expected && nV > 0 ) {
								bspxVertexNormals     = (const float *)( base + lofs );
								bspxVertexNormalCount = nV;
								Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_Load: BSPX VERTEXNORMALS: %d normals\n", nV );
							} else {
								Com_Log( SEV_WARN, LOG_CH(ch_loading),
								         "BSP_Q1_Load: BSPX VERTEXNORMALS size mismatch "
								         "(got %u expected %d)\n", llen, expected );
							}
						} else if ( !strcmp( lname, "FACENORMALS" ) ) {
							int expected = nF * 12;
							if ( (int)llen == expected && nF > 0 ) {
								bspxFaceNormals     = (const float *)( base + lofs );
								bspxFaceNormalCount = nF;
								Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_Load: BSPX FACENORMALS: %d normals\n", nF );
							} else {
								Com_Log( SEV_WARN, LOG_CH(ch_loading),
								         "BSP_Q1_Load: BSPX FACENORMALS size mismatch "
								         "(got %u expected %d)\n", llen, expected );
							}
						}
					}
				}
			}
		}
	}

	// ---- Surfaces + DrawVerts + DrawIndexes + Lightmap atlas ----
	// Q1 face → edge → vertex indirection flattened to per-surface vertex ranges.
	// DrawIndexes are face-relative (0..numedges-1), matching Q3 convention.
	// Lightmap patches are packed into a shelf atlas (4 parallel style pages).
	{
		const lump_t        *faceLump    = &header.lumps[LUMP_Q1_FACES];
		const lump_t        *edgeLump    = &header.lumps[LUMP_Q1_EDGES];
		const lump_t        *sedgeLump   = &header.lumps[LUMP_Q1_SURFEDGES];
		const lump_t        *vertLump    = &header.lumps[LUMP_Q1_VERTEXES];
		const lump_t        *texinfoLump = &header.lumps[LUMP_Q1_TEXINFO];
		const lump_t        *lightLump   = &header.lumps[LUMP_Q1_LIGHTING];

		const q1_dface_t    *faces   = (const q1_dface_t    *)( base + faceLump->fileofs );
		const q1_dedge_t    *edges   = (const q1_dedge_t    *)( base + edgeLump->fileofs );
		const int           *sedges  = (const int           *)( base + sedgeLump->fileofs );
		const q1_dvertex_t  *verts   = (const q1_dvertex_t  *)( base + vertLump->fileofs );
		const q1_dtexinfo_t *tinfos  = (const q1_dtexinfo_t *)( base + texinfoLump->fileofs );
		const byte          *lightBase = base + lightLump->fileofs;
		int                  lightLen  = lightLump->filelen;

		/* Try to load a .lit sidecar file for per-texel RGB colored lighting */
		const byte *litBase = NULL;
		if ( lightLen > 0 ) {
			char litBase64[MAX_QPATH], litPath[MAX_QPATH];
			COM_StripExtension( name, litBase64, sizeof( litBase64 ) );
			Com_sprintf( litPath, sizeof( litPath ), "%s.lit", litBase64 );
			litBase = Lit_TryLoad( litPath, 3 * lightLen );
		}

		int numFaces    = faceLump->filelen / (int)sizeof( q1_dface_t );
		int numEdges    = edgeLump->filelen / (int)sizeof( q1_dedge_t );
		int numSedges   = sedgeLump->filelen / (int)sizeof( int );
		int numVerts    = vertLump->filelen / (int)sizeof( q1_dvertex_t );
		int numTexinfos = texinfoLump->filelen / (int)sizeof( q1_dtexinfo_t );

		/* Miptex lump for UV normalisation — dimensions looked up per face */
		const lump_t *textureLump  = &header.lumps[LUMP_Q1_TEXTURES];
		const byte   *mipLumpBase  = NULL;
		int           mipLumpCount = 0;
		if ( textureLump->filelen >= (int)sizeof( int ) ) {
			mipLumpBase  = base + textureLump->fileofs;
			mipLumpCount = LittleLong( *(const int *)mipLumpBase );
			if ( mipLumpCount < 0 ) mipLumpCount = 0;
		}

		// Pass 1: count total verts and indexes (one flat surface per face).
		int totalVerts   = 0;
		int totalIndexes = 0;
		for ( i = 0; i < numFaces; i++ ) {
			int n = (short)LittleShort( faces[i].numedges );
			if ( n < 3 ) continue;
			totalVerts   += n;
			totalIndexes += ( n - 2 ) * 3;
		}

		int totalSurfaces = numFaces;

		bsp->numSurfaces    = totalSurfaces;
		bsp->surfaces       = BSP_Q1_ZAlloc( totalSurfaces * sizeof( dsurface_t ) );
		bsp->numDrawVerts   = totalVerts;
		bsp->drawVerts      = BSP_Q1_ZAlloc( totalVerts * sizeof( drawVert_t ) );
		bsp->numDrawIndexes = totalIndexes;
		bsp->drawIndexes    = BSP_Q1_ZAlloc( totalIndexes * sizeof( int ) );
		// 4 style bytes per vertex, parallel to drawVerts[]
		bsp->drawVertLightstyles = BSP_Q1_ZAlloc( (size_t)totalVerts * 4 );
		memset( bsp->drawVertLightstyles, 255, (size_t)totalVerts * 4 );

		// Initialise shelf packer
		LightmapPacker_t packer;
		LM_Init( &packer );

		// Pass 2: fill
		int vertCursor        = 0;
		int indexCursor       = 0;
		int litFaces          = 0;
		int darkFaces         = 0;
		int flippedCount      = 0;
		int directCount       = 0;
		int emittedCount      = 0;
		int skippedCount      = 0;   /* n < 3 */
		int diag_side0_done   = 0;   /* winding diagnostic: side=0 faces logged so far */
		int diag_side1_done   = 0;   /* winding diagnostic: side=1 faces logged so far */
		int texinfoClampCount = 0;   /* texinfoIdx out of range, clamped to 0 */
		int skipLogCount      = 0;   /* how many skipped faces we've logged */
		for ( i = 0; i < numFaces; i++ ) {
			const q1_dface_t *f          = &faces[i];
			int               n          = (short)LittleShort( f->numedges );
			int               firstEdge  = LittleLong( f->firstedge );
			int               texinfoIdx = (short)LittleShort( f->texinfo );
			dsurface_t       *s          = &bsp->surfaces[i];

			if ( n < 3 ) {
				s->surfaceType = MST_BAD;
				skippedCount++;
				if ( skipLogCount < 10 ) {
					char skipTex[17] = "?";
					int rawTi = (texinfoIdx >= 0 && texinfoIdx < numTexinfos)
					            ? (int)LittleLong( tinfos[texinfoIdx].miptex ) : -1;
					if ( mipLumpBase && rawTi >= 0 && rawTi < mipLumpCount ) {
						const int *mipOfs = (const int *)( mipLumpBase + sizeof( int ) );
						int ofs = LittleLong( mipOfs[rawTi] );
						if ( ofs >= 0 && ofs + 16 <= textureLump->filelen ) {
							Q_strncpyz( skipTex, (const char *)( mipLumpBase + ofs ), sizeof(skipTex) );
						}
					}
					Com_Log( SEV_TRACE, LOG_CH(ch_loading),
					         "BSP_Q1_Load: skip face[%d]: numedges=%d (<3), texinfo=%d tex='%s'\n",
					         i, n, texinfoIdx, skipTex );
					skipLogCount++;
				}
				continue;
			}
			if ( texinfoIdx < 0 || texinfoIdx >= numTexinfos ) {
				texinfoClampCount++;
				texinfoIdx = 0;
			}

			const q1_dtexinfo_t *ti = &tinfos[texinfoIdx];
			float vecs[2][4];
			int k;
			for ( j = 0; j < 2; j++ ) {
				for ( k = 0; k < 4; k++ ) {
					vecs[j][k] = LittleFloat( ti->vecs[j][k] );
				}
			}

			/* Miptex width/height for UV normalisation (pixel-space → [0,1]) */
			float texW = 64.0f, texH = 64.0f;
			{
				int mIdx = LittleLong( ti->miptex );
				if ( mipLumpBase && mIdx >= 0 && mIdx < mipLumpCount ) {
					const int *mipOffsets = (const int *)( mipLumpBase + sizeof( int ) );
					int ofs = LittleLong( mipOffsets[mIdx] );
					if ( ofs >= 0 && ofs + (int)sizeof( q1_miptex_t ) <= textureLump->filelen ) {
						const q1_miptex_t *mt = (const q1_miptex_t *)( mipLumpBase + ofs );
						unsigned int w = (unsigned int)LittleLong( (int)mt->width );
						unsigned int h = (unsigned int)LittleLong( (int)mt->height );
						if ( w > 0 ) texW = (float)w;
						if ( h > 0 ) texH = (float)h;
					}
				}
			}

			/* Face normal: Q1 plane normal, negated when face.side == 1 */
			vec3_t faceNormal;
			{
				int planeNum = (int)(short)LittleShort( f->planenum );
				int faceSide = (int)(short)LittleShort( f->side );
				if ( planeNum >= 0 && planeNum < bsp->numPlanes ) {
					VectorCopy( bsp->planes[planeNum].normal, faceNormal );
				} else {
					VectorSet( faceNormal, 0.0f, 0.0f, 1.0f );
				}
				if ( faceSide ) {
					VectorNegate( faceNormal, faceNormal );
					flippedCount++;
				} else {
					directCount++;
				}
			}

			// Decode face lightmap metadata
			byte  styles[4];
			int   lightofs  = LittleLong( f->lightofs );
			qboolean hasLightmap = qtrue;
			for ( j = 0; j < 4; j++ ) {
				styles[j] = f->styles[j];
			}
			// Sky and TEX_SPECIAL surfaces carry no lightmap
			if ( bsp->numShaders > texinfoIdx ) {
				if ( bsp->shaders[texinfoIdx].surfaceFlags & SURF_NOLIGHTMAP ) {
					hasLightmap = qfalse;
				}
			}
			if ( lightofs < 0 ) {
				hasLightmap = qfalse;
			}

			// ---- Compute texcoord extent across all face verts ----
			float min_s =  1e30f, max_s = -1e30f;
			float min_t =  1e30f, max_t = -1e30f;
			for ( int e = 0; e < n; e++ ) {
				int se   = LittleLong( sedges[firstEdge + e] );
				int vIdx = ( se >= 0 )
					? (int)(unsigned short)LittleShort( edges[se].v[0] )
					: (int)(unsigned short)LittleShort( edges[-se].v[1] );
				if ( vIdx < 0 || vIdx >= numVerts ) vIdx = 0;
				float x = LittleFloat( verts[vIdx].point[0] );
				float y = LittleFloat( verts[vIdx].point[1] );
				float z = LittleFloat( verts[vIdx].point[2] );
				float s_coord = x*vecs[0][0] + y*vecs[0][1] + z*vecs[0][2] + vecs[0][3];
				float t_coord = x*vecs[1][0] + y*vecs[1][1] + z*vecs[1][2] + vecs[1][3];
				if ( s_coord < min_s ) min_s = s_coord;
				if ( s_coord > max_s ) max_s = s_coord;
				if ( t_coord < min_t ) min_t = t_coord;
				if ( t_coord > max_t ) max_t = t_coord;
			}

			// Lightmap patch size in texels (Q1: 1 texel per 16 world units)
			int lm_min_s = (int)floorf( min_s / 16.0f );
			int lm_max_s = (int)ceilf(  max_s / 16.0f );
			int lm_min_t = (int)floorf( min_t / 16.0f );
			int lm_max_t = (int)ceilf(  max_t / 16.0f );
			int lm_w     = lm_max_s - lm_min_s + 1;
			int lm_h     = lm_max_t - lm_min_t + 1;
			// Clamp to page size (should never exceed 128 for normal maps)
			if ( lm_w > LM_PAGE_W ) lm_w = LM_PAGE_W;
			if ( lm_h > LM_PAGE_H ) lm_h = LM_PAGE_H;
			if ( lm_w < 1 ) lm_w = 1;
			if ( lm_h < 1 ) lm_h = 1;

			// Allocate atlas slot and fill lightmap data
			int atlasPage = 0, atlasX = 0, atlasY = 0;
			if ( hasLightmap ) {
				LM_Alloc( &packer, lm_w, lm_h, &atlasPage, &atlasX, &atlasY );
				LM_FillPatch( &packer, atlasPage, atlasX, atlasY,
				              lm_w, lm_h, lightBase, lightLen, lightofs, styles,
				              litBase );
				if ( litFaces < 20 ) {
					Com_Log( SEV_TRACE, LOG_CH(ch_loading),
					         "lightstyle face[%d]: styles=[%d,%d,%d,%d]"
					         " (255=unused slot)\n",
					         i, styles[0], styles[1], styles[2], styles[3] );
				}
				litFaces++;
			} else {
				darkFaces++;
			}

			int baseVert  = vertCursor;
			int baseIndex = indexCursor;

			// ---- Direct fan triangulation (all faces including liquid) ----
			for ( int e = 0; e < n; e++ ) {
				int se   = LittleLong( sedges[firstEdge + e] );
				int vIdx = ( se >= 0 )
					? (int)(unsigned short)LittleShort( edges[se].v[0] )
					: (int)(unsigned short)LittleShort( edges[-se].v[1] );
				if ( vIdx < 0 || vIdx >= numVerts ) vIdx = 0;

				float xyz[3];
				for ( j = 0; j < 3; j++ )
					xyz[j] = LittleFloat( verts[vIdx].point[j] );

				drawVert_t *dv = &bsp->drawVerts[vertCursor];
				VectorCopy( xyz, dv->xyz );

				float ps = xyz[0]*vecs[0][0] + xyz[1]*vecs[0][1] + xyz[2]*vecs[0][2] + vecs[0][3];
				float pt = xyz[0]*vecs[1][0] + xyz[1]*vecs[1][1] + xyz[2]*vecs[1][2] + vecs[1][3];

				// atlasX/atlasY are interior coordinates (LM_Alloc adds +1 border offset).
				// UV formula needs no adjustment — atlasX/atlasY already point to interior.
				float lm_u = (float)(atlasX) + (ps / 16.0f - (float)lm_min_s) + 0.5f;
				float lm_v = (float)(atlasY) + (pt / 16.0f - (float)lm_min_t) + 0.5f;
				dv->lightmap[0] = lm_u / (float)LM_PAGE_W;
				dv->lightmap[1] = lm_v / (float)LM_PAGE_H;
				dv->st[0]       = ps / texW;
				dv->st[1]       = pt / texH;

				if ( bspxVertexNormals && vIdx < bspxVertexNormalCount ) {
					dv->normal[0] = LittleFloat( bspxVertexNormals[vIdx*3+0] );
					dv->normal[1] = LittleFloat( bspxVertexNormals[vIdx*3+1] );
					dv->normal[2] = LittleFloat( bspxVertexNormals[vIdx*3+2] );
				} else if ( bspxFaceNormals && i < bspxFaceNormalCount ) {
					dv->normal[0] = LittleFloat( bspxFaceNormals[i*3+0] );
					dv->normal[1] = LittleFloat( bspxFaceNormals[i*3+1] );
					dv->normal[2] = LittleFloat( bspxFaceNormals[i*3+2] );
				} else {
					VectorCopy( faceNormal, dv->normal );
				}
				dv->color.rgba[0] = dv->color.rgba[1] = dv->color.rgba[2] = dv->color.rgba[3] = 255;

				byte *ls = bsp->drawVertLightstyles + vertCursor * 4;
				for ( j = 0; j < 4; j++ )
					ls[j] = hasLightmap ? styles[j] : 255;

				vertCursor++;
			}

			for ( int t = 0; t < n - 2; t++ ) {
				bsp->drawIndexes[indexCursor++] = 0;
				bsp->drawIndexes[indexCursor++] = t + 1;
				bsp->drawIndexes[indexCursor++] = t + 2;
			}

			/* Winding-vs-normal diagnostic: first 5 of each side type (non-liquid only).
			   Checks whether (v1-v0)×(v2-v0) from the first emitted triangle
			   agrees with the (already-flipped) faceNormal written to dv->normal.
			   The renderer's zero-normal fallback uses the REVERSE cross product
			   (v2-v0)×(v1-v0), so a "match=yes" here means the renderer gets
			   an OPPOSITE (inward) plane normal → face will be culled incorrectly. */
			{
				int faceSide_d = (int)(short)LittleShort( f->side );
				qboolean wantLog = ( faceSide_d == 0 && diag_side0_done < 5 ) ||
				                   ( faceSide_d == 1 && diag_side1_done < 5 );
				if ( wantLog && n >= 3 ) {
					const float *p0 = bsp->drawVerts[baseVert + 0].xyz;
					const float *p1 = bsp->drawVerts[baseVert + 1].xyz;
					const float *p2 = bsp->drawVerts[baseVert + 2].xyz;
					vec3_t e1, e2, windNormal;
					VectorSubtract( p1, p0, e1 );
					VectorSubtract( p2, p0, e2 );
					CrossProduct( e1, e2, windNormal );
					VectorNormalize( windNormal );
					float dot = DotProduct( windNormal, faceNormal );
					const char *match = ( dot >  0.9f ) ? "yes"
					                  : ( dot < -0.9f ) ? "opposite"
					                                    : "skewed";
					Com_Log( SEV_TRACE, LOG_CH(ch_loading),
					         "BSP_Q1 winding face[%d] side=%d:"
					         " winding_normal=(%.3f,%.3f,%.3f)"
					         " face_normal=(%.3f,%.3f,%.3f)"
					         " dot=%.3f match=%s\n",
					         i, faceSide_d,
					         windNormal[0], windNormal[1], windNormal[2],
					         faceNormal[0], faceNormal[1], faceNormal[2],
					         dot, match );
					if ( faceSide_d == 0 ) diag_side0_done++;
					else                   diag_side1_done++;
				}
			}

			{
				s->shaderNum      = texinfoIdx;
				s->fogNum         = -1;
				s->surfaceType    = MST_PLANAR;
				s->firstVert      = baseVert;
				s->numVerts       = vertCursor  - baseVert;
				s->firstIndex     = baseIndex;
				s->numIndexes     = indexCursor - baseIndex;
				s->lightmapNum    = hasLightmap ? atlasPage : -1;
				s->lightmapX      = atlasX;
				s->lightmapY      = atlasY;
				s->lightmapWidth  = lm_w;
				s->lightmapHeight = lm_h;
				VectorClear( s->lightmapOrigin );
				for ( j = 0; j < 3; j++ ) {
					VectorClear( s->lightmapVecs[j] );
				}
				/* Supply the pre-flipped face normal so the renderer's ParseFace
				   reads a valid unit vector from lightmapVecs[2] and skips its
				   cross-product fallback, which uses (v2-v0)×(v1-v0) — the negated
				   winding — and produces an inverted culling plane for all Q1 faces. */
				VectorCopy( faceNormal, s->lightmapVecs[2] );
				s->patchWidth  = 0;
				s->patchHeight = 0;

				/* UV diagnostic: log normalised st range of the first valid surface */
				if ( i == 0 && s->numVerts >= 3 ) {
					float minS =  1e30f, maxS = -1e30f;
					float minT =  1e30f, maxT = -1e30f;
					int vi;
					for ( vi = baseVert; vi < vertCursor; vi++ ) {
						float sv = bsp->drawVerts[vi].st[0];
						float tv = bsp->drawVerts[vi].st[1];
						if ( sv < minS ) minS = sv;
						if ( sv > maxS ) maxS = sv;
						if ( tv < minT ) minT = tv;
						if ( tv > maxT ) maxT = tv;
					}
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: UV sample: surface[0] st range = (%.3f..%.3f, %.3f..%.3f)\n",
					            minS, maxS, minT, maxT );
				}
			}

			emittedCount++;
		}

		// Transfer finalised atlas pages to bspFile_t
		int totalPages = packer.numUsed;
		bsp->numLightmapPages = totalPages;
		bsp->lightmapPageSize = LM_PAGE_BYTES;

		// Style 0 → bsp->lightmapData (renderer uses this as primary)
		bsp->lightmapData = BSP_Q1_ZAlloc( (size_t)totalPages * LM_PAGE_BYTES );
		memcpy( bsp->lightmapData, packer.data[0], (size_t)totalPages * LM_PAGE_BYTES );

		// Styles 1..3 → styledLightmapData (used by Part D shader blend)
		int s;
		for ( s = 1; s < 4; s++ ) {
			bsp->styledLightmapData[s] = BSP_Q1_ZAlloc( (size_t)totalPages * LM_PAGE_BYTES );
			memcpy( bsp->styledLightmapData[s], packer.data[s], (size_t)totalPages * LM_PAGE_BYTES );
			bsp->numStyledLightmapPages[s] = totalPages;
		}

		// Free staging buffers
		for ( s = 0; s < 4; s++ ) {
			Z_Free( packer.data[s] );
			packer.data[s] = NULL;
		}

		if ( litBase ) {
			Z_Free( (void *)litBase );
			litBase = NULL;
		}

		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: lightmap atlas: %d pages, %d lit faces, %d unlit faces\n",
		            totalPages, litFaces, darkFaces );
		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: face normals: %d flipped (side=1), %d direct (side=0)\n",
		            flippedCount, directCount );
		Com_Log( SEV_TRACE, LOG_CH(ch_loading),
		         "BSP_Q1_Load: Q1 faces: %d total, %d emitted, %d skipped"
		         " (n<3=%d texinfoClamp=%d lightofs-1=not-a-skip styles=not-a-skip)\n",
		         numFaces, emittedCount, skippedCount,
		         skippedCount, texinfoClampCount );
	}

	// ---- Nodes ----
	{
		const lump_t     *l  = &header.lumps[LUMP_Q1_NODES];
		const q1_dnode_t *in = (const q1_dnode_t *)( base + l->fileofs );
		dnode_t          *out;

		if ( l->filelen % (int)sizeof( q1_dnode_t ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s funny nodes lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numNodes = l->filelen / (int)sizeof( q1_dnode_t );
		bsp->nodes    = BSP_Q1_ZAlloc( bsp->numNodes * sizeof( dnode_t ) );
		out = bsp->nodes;

		for ( i = 0; i < bsp->numNodes; i++, in++, out++ ) {
			out->planeNum = LittleLong( in->planenum );
			for ( j = 0; j < 2; j++ ) {
				short c = (short)LittleShort( in->children[j] );
				// Q1: negative child value → ~c is leaf index (0-based in Q1)
				// Q3: negative child → -(leafIdx + 1)
				if ( c < 0 ) {
					out->children[j] = -( (~c) + 1 );
				} else {
					out->children[j] = c;
				}
			}
			for ( j = 0; j < 3; j++ ) {
				out->mins[j] = (short)LittleShort( in->mins[j] );
				out->maxs[j] = (short)LittleShort( in->maxs[j] );
			}
		}
	}

	// ---- Leafs ----
	// Each Q1 leaf (except leaf 0 which is always solid) gets a unique cluster ID
	// equal to its index minus 1 (0..numLeafs-2).  The vis table is decompressed
	// from the Q1 RLE PVS and written as a flat numClusters×clusterBytes array into
	// bsp->visibility, which BSP_Q1_BuildSyntheticQ3Data then embeds in the Q3 vis lump.
	{
		const lump_t     *l  = &header.lumps[LUMP_Q1_LEAFS];
		const q1_dleaf_t *in = (const q1_dleaf_t *)( base + l->fileofs );
		dleaf_t          *out;

		if ( l->filelen % (int)sizeof( q1_dleaf_t ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s funny leafs lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numLeafs = l->filelen / (int)sizeof( q1_dleaf_t );
		bsp->leafs    = BSP_Q1_ZAlloc( bsp->numLeafs * sizeof( dleaf_t ) );

		const q1_dleaf_t *rawLeafs = in;   // saved for vis decompression below
		out = bsp->leafs;

		for ( i = 0; i < bsp->numLeafs; i++, in++, out++ ) {
			// Leaf 0 is always solid in Q1; CM_ClusterPVS returns cm.novis for cluster=-1.
			out->cluster = ( i == 0 ) ? -1 : i - 1;
			out->area    = 0;   // Q1 has no portal areas
			for ( j = 0; j < 3; j++ ) {
				out->mins[j] = (short)LittleShort( in->mins[j] );
				out->maxs[j] = (short)LittleShort( in->maxs[j] );
			}
			out->firstLeafSurface = (int)(unsigned short)LittleShort( in->firstmarksurface );
			out->numLeafSurfaces  = (int)(unsigned short)LittleShort( in->nummarksurfaces );
			out->firstLeafBrush   = 0;  // Phase 4
			out->numLeafBrushes   = 0;  // Phase 4
		}

		// Store Q3-translated leaf contents for native Q1 PointContents queries.
		// Skipped for render-only loads (prop BSPs) to protect the live world collision tree.
		if ( !(flags & BSP_LOAD_FLAG_RENDER_ONLY) ) {
			int *leafQ3Contents = (int *)Z_Malloc( bsp->numLeafs * sizeof( int ) );
			for ( i = 0; i < bsp->numLeafs; i++ )
				leafQ3Contents[i] = BSP_Q1_ContentsToQ3( LittleLong( rawLeafs[i].contents ) );
			CMQ1_StoreLeafContents( leafQ3Contents, bsp->numLeafs );
			Z_Free( leafQ3Contents );
		}

		// Decompress Q1 per-leaf PVS into a flat cluster × clusterBytes table.
		{
			const lump_t *vl          = &header.lumps[LUMP_Q1_VISIBILITY];
			const byte   *visBase     = ( vl->filelen > 0 ) ? (const byte *)( base + vl->fileofs ) : NULL;
			int           numClusters  = bsp->numLeafs - 1;
			int           clusterBytes = ( numClusters > 0 ) ? ( numClusters + 7 ) >> 3 : 0;
			int           li;

			if ( numClusters > 0 ) {
				byte *flatPVS = BSP_Q1_ZAlloc( (size_t)numClusters * clusterBytes );
				for ( li = 1; li < bsp->numLeafs; li++ ) {
					byte       *dst    = flatPVS + ( li - 1 ) * clusterBytes;
					int         visofs = LittleLong( rawLeafs[li].visofs );
					const byte *src    = ( visBase && visofs != -1 ) ? visBase + visofs : NULL;
					BSP_Q1_RLEDecompressVis( src, clusterBytes, dst );
				}
				bsp->numClusters      = numClusters;
				bsp->clusterBytes     = clusterBytes;
				bsp->visibility       = flatPVS;
				bsp->visibilityLength = numClusters * clusterBytes;
			} else {
				bsp->numClusters      = 0;
				bsp->clusterBytes     = 0;
				bsp->visibility       = NULL;
				bsp->visibilityLength = 0;
			}
		}
	}

	// ---- Leaf surfaces (marksurfaces) ----
	{
		const lump_t           *l  = &header.lumps[LUMP_Q1_MARKSURFACES];
		const unsigned short   *in = (const unsigned short *)( base + l->fileofs );

		if ( l->filelen % (int)sizeof( unsigned short ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s funny marksurfaces lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numLeafSurfaces = l->filelen / (int)sizeof( unsigned short );
		bsp->leafSurfaces    = BSP_Q1_ZAlloc( bsp->numLeafSurfaces * sizeof( int ) );
		for ( i = 0; i < bsp->numLeafSurfaces; i++ ) {
			bsp->leafSurfaces[i] = (int)LittleShort( in[i] );
		}

	}

	// ---- PVS post-processing ----
	// Must run after both leafs (PVS decompressed) and marksurfaces are loaded,
	// and before the leaf deduplication pass that expands bsp->leafs.
	{
		const q1_dleaf_t *rawLeafs =
			(const q1_dleaf_t *)( base + header.lumps[LUMP_Q1_LEAFS].fileofs );
		Q1_PostProcessPVS( bsp, rawLeafs );
	}

	// ---- Submodels ----
	{
		const lump_t      *l  = &header.lumps[LUMP_Q1_MODELS];
		const q1_dmodel_t *in = (const q1_dmodel_t *)( base + l->fileofs );
		dmodel_t          *out;

		if ( l->filelen % (int)sizeof( q1_dmodel_t ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_loading), "BSP_Q1_Load: %s funny models lump size\n", name );
			BSP_Free( bsp );
			return qfalse;
		}
		bsp->numSubModels = l->filelen / (int)sizeof( q1_dmodel_t );
		bsp->subModels    = BSP_Q1_ZAlloc( bsp->numSubModels * sizeof( dmodel_t ) );
		out = bsp->subModels;

		for ( i = 0; i < bsp->numSubModels; i++, in++, out++ ) {
			for ( j = 0; j < 3; j++ ) {
				out->mins[j] = LittleFloat( in->mins[j] );
				out->maxs[j] = LittleFloat( in->maxs[j] );
			}
			out->firstSurface = LittleLong( in->firstface );
			out->numSurfaces  = LittleLong( in->numfaces );
			out->firstBrush   = 0;  // Phase 4
			out->numBrushes   = 0;  // Phase 4
		}
	}

	// ---- Bridge diagnostics: submodel enumeration, coplanar count, entity proximity ----
	{
		const q1_dface_t    *rawFaces  = (const q1_dface_t *)
		                                 ( base + header.lumps[LUMP_Q1_FACES].fileofs );
		const q1_dtexinfo_t *rawTinfos = (const q1_dtexinfo_t *)
		                                 ( base + header.lumps[LUMP_Q1_TEXINFO].fileofs );
		const q1_dplane_t   *rawPlanes = (const q1_dplane_t *)
		                                 ( base + header.lumps[LUMP_Q1_PLANES].fileofs );
		int numRawFaces  = header.lumps[LUMP_Q1_FACES].filelen  / (int)sizeof( q1_dface_t );
		int numRawTinfos = header.lumps[LUMP_Q1_TEXINFO].filelen / (int)sizeof( q1_dtexinfo_t );
		int numRawPlanes = header.lumps[LUMP_Q1_PLANES].filelen  / (int)sizeof( q1_dplane_t );

		const lump_t *texLump = &header.lumps[LUMP_Q1_TEXTURES];
		const byte   *mipBase = ( texLump->filelen >= (int)sizeof(int) )
		                        ? base + texLump->fileofs : NULL;
		int mipCount = mipBase ? LittleLong( *(const int *)mipBase ) : 0;
		if ( mipCount < 0 ) mipCount = 0;

		/* 1: submodel enumeration — flag any AABB overlapping bridge (480,2000,80) ±300 */
		Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: submodel enumeration (%d total):\n",
		         bsp->numSubModels );
		for ( i = 0; i < bsp->numSubModels; i++ ) {
			const dmodel_t *sm = &bsp->subModels[i];
			char firstTex[17] = "?";
			int fi = sm->firstSurface;
			if ( fi >= 0 && fi < numRawFaces ) {
				int tiIdx = (int)(short)LittleShort( rawFaces[fi].texinfo );
				if ( tiIdx >= 0 && tiIdx < numRawTinfos ) {
					int mIdx = LittleLong( rawTinfos[tiIdx].miptex );
					if ( mipBase && mIdx >= 0 && mIdx < mipCount ) {
						const int *mipOfs = (const int *)( mipBase + sizeof(int) );
						int ofs = LittleLong( mipOfs[mIdx] );
						if ( ofs >= 0 && ofs + 16 <= texLump->filelen )
							Q_strncpyz( firstTex, (const char *)( mipBase + ofs ),
							            sizeof(firstTex) );
					}
				}
			}
			const float BX = 480.0f, BY = 2000.0f, BZ = 80.0f, TOL = 300.0f;
			qboolean nearBridge = (
			    sm->maxs[0] >= BX - TOL && sm->mins[0] <= BX + TOL &&
			    sm->maxs[1] >= BY - TOL && sm->mins[1] <= BY + TOL &&
			    sm->maxs[2] >= BZ - TOL && sm->mins[2] <= BZ + TOL );
			Com_Log( SEV_TRACE, LOG_CH(ch_loading),
			         "  [%d] mins=(%.0f,%.0f,%.0f) maxs=(%.0f,%.0f,%.0f)"
			         " firstSurf=%d numSurf=%d tex='%s'%s\n",
			         i,
			         sm->mins[0], sm->mins[1], sm->mins[2],
			         sm->maxs[0], sm->maxs[1], sm->maxs[2],
			         sm->firstSurface, sm->numSurfaces, firstTex,
			         nearBridge ? "  <-- NEAR BRIDGE" : "" );
		}

		/* 2: coplanar surface count — Q1 planes referenced by 2+ raw faces */
		if ( numRawPlanes > 0 ) {
			int *planeRefCount = (int *)Z_Malloc( numRawPlanes * sizeof(int) );
			memset( planeRefCount, 0, numRawPlanes * sizeof(int) );
			for ( i = 0; i < numRawFaces; i++ ) {
				int pn = (int)(short)LittleShort( rawFaces[i].planenum );
				if ( pn >= 0 && pn < numRawPlanes ) planeRefCount[pn]++;
			}
			int coplanarPairs = 0;
			for ( i = 0; i < numRawPlanes; i++ ) {
				if ( planeRefCount[i] > 1 ) coplanarPairs++;
			}
			Com_Log( SEV_TRACE, LOG_CH(ch_loading),
			         "BSP_Q1_Load: %d planes shared by 2+ faces (potential z-fight);"
			         " %d total Q1 planes\n",
			         coplanarPairs, numRawPlanes );
			Z_Free( planeRefCount );
		}

		/* 3: entity proximity — classes and origins within 200 units of (480,2000,80) */
		{
			const float BX = 480.0f, BY = 2000.0f, BZ = 80.0f, RANGE = 200.0f;
			const char *p = bsp->entityString;
			int entNear = 0;
			Com_Log( SEV_TRACE, LOG_CH(ch_loading),
			         "BSP_Q1_Load: entities within 200 units of bridge (480,2000,80):\n" );
			while ( p && *p ) {
				const char *bstart = strchr( p, '{' );
				if ( !bstart ) break;
				const char *bend = strchr( bstart, '}' );
				if ( !bend ) break;

				char cls[64] = "?";
				const char *ck = strstr( bstart, "\"classname\"" );
				if ( ck && ck < bend ) {
					ck = strchr( ck + 11, '"' );
					if ( ck && ck < bend ) {
						int ci = 0; ck++;
						while ( *ck && *ck != '"' && ci < 63 ) cls[ci++] = *ck++;
						cls[ci] = '\0';
					}
				}

				float ox = 0.0f, oy = 0.0f, oz = 0.0f;
				const char *ok = strstr( bstart, "\"origin\"" );
				if ( ok && ok < bend ) {
					ok = strchr( ok + 8, '"' );
					if ( ok && ok < bend ) {
						ok++;
						sscanf( ok, "%f %f %f", &ox, &oy, &oz );
					}
				}

				if ( fabsf(ox - BX) <= RANGE &&
				     fabsf(oy - BY) <= RANGE &&
				     fabsf(oz - BZ) <= RANGE ) {
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  class='%s' origin=(%.0f,%.0f,%.0f)\n",
					         cls, ox, oy, oz );
					entNear++;
				}
				p = bend + 1;
			}
			if ( entNear == 0 )
				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  (none within 200 units)\n" );
		}
	}


	// ---- Embedded textures (raw copy for Phase 5) ----
	{
		const lump_t *l = &header.lumps[LUMP_Q1_TEXTURES];
		if ( l->filelen > 0 ) {
			bsp->embeddedTexturesLength = l->filelen;
			bsp->embeddedTextures       = BSP_Q1_ZAlloc( l->filelen );
			memcpy( bsp->embeddedTextures, base + l->fileofs, l->filelen );
			bsp->numEmbeddedTextures = LittleLong( *(const int *)bsp->embeddedTextures );
		}
	}

	// ---- Leaf deduplication: convert Q1 DAG to strict tree ----
	// Q1 lets the solid leaf (leaf 0) be a child of many nodes simultaneously.
	// R_SetParent stores node->parent on first visit and errors on re-visit,
	// treating this valid DAG as a cycle.  Duplicate every multiply-referenced
	// leaf so the resulting tree has only one parent per node.
	{
		int *leafRefCount = (int *)Z_Malloc( bsp->numLeafs * sizeof(int) );
		int  extraLeaves = 0, sharedLeaves = 0;

		memset( leafRefCount, 0, bsp->numLeafs * sizeof(int) );
		for ( i = 0; i < bsp->numNodes; i++ ) {
			int j;
			for ( j = 0; j < 2; j++ ) {
				int c = bsp->nodes[i].children[j];
				if ( c < 0 ) {
					int leafIdx = -(c + 1);
					if ( leafIdx < bsp->numLeafs )
						leafRefCount[leafIdx]++;
				}
			}
		}
		for ( i = 0; i < bsp->numLeafs; i++ ) {
			if ( leafRefCount[i] > 1 ) {
				sharedLeaves++;
				extraLeaves += leafRefCount[i] - 1;
			}
		}

		if ( extraLeaves > 0 ) {
			int oldNumLeafs = bsp->numLeafs;
			int newNumLeafs = oldNumLeafs + extraLeaves;
			int nextSlot    = oldNumLeafs;
			dleaf_t *newLeafs = (dleaf_t *)Z_Malloc( newNumLeafs * sizeof(dleaf_t) );
			memcpy( newLeafs, bsp->leafs, oldNumLeafs * sizeof(dleaf_t) );
			Z_Free( bsp->leafs );
			bsp->leafs    = newLeafs;
			bsp->numLeafs = newNumLeafs;

			memset( leafRefCount, 0, oldNumLeafs * sizeof(int) );
			for ( i = 0; i < bsp->numNodes; i++ ) {
				int j;
				for ( j = 0; j < 2; j++ ) {
					int c = bsp->nodes[i].children[j];
					if ( c < 0 ) {
						int leafIdx = -(c + 1);
						if ( leafIdx < oldNumLeafs ) {
							leafRefCount[leafIdx]++;
							if ( leafRefCount[leafIdx] > 1 ) {
								memcpy( &bsp->leafs[nextSlot], &bsp->leafs[leafIdx], sizeof(dleaf_t) );
								bsp->nodes[i].children[j] = -(nextSlot + 1);
								nextSlot++;
							}
						}
					}
				}
			}
			Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: leaf-dup: numLeafs %d -> %d (+%d unique copies for %d shared refs)\n",
				oldNumLeafs, newNumLeafs, extraLeaves, sharedLeaves );

			/* Extend leafContents[] to cover post-dup leaf indices.
			   Duplicated leaf copies (index >= oldNumLeafs) inherit the Q3 contents
			   of the original leaf they were copied from, recovered via cluster mapping:
			     bsp->leafs[i].cluster == i - 1  for i > 0  (Q1 convention)
			     bsp->leafs[i].cluster == -1     for i == 0 (solid leaf)
			   After dup, copied leaves have the same cluster as their source. */
			if ( !(flags & BSP_LOAD_FLAG_RENDER_ONLY) ) {
				int newCount = bsp->numLeafs;
				int *ext     = (int *)Z_Malloc( newCount * sizeof( int ) );
				int  extIdx;
				memcpy( ext, cm.q1.leafContents, oldNumLeafs * sizeof( int ) );
				for ( extIdx = oldNumLeafs; extIdx < newCount; extIdx++ ) {
					int cluster = bsp->leafs[extIdx].cluster;
					int origIdx = ( cluster < 0 ) ? 0 : ( cluster + 1 );
					ext[extIdx] = ( origIdx < oldNumLeafs ) ? ext[origIdx] : 0;
				}
				CMQ1_StoreLeafContents( ext, newCount );
				Z_Free( ext );
			}
		}
		Z_Free( leafRefCount );
	}

	// ---- Clipnodes — hull 1 only (Phase 4.1) ----
	// Traverse hull-1 clipnode tree, emit one brush per SOLID leaf.
	// Each brush's plane set is accumulated on a stack during recursive descent.
	{
		const lump_t   *cnLump = &header.lumps[LUMP_Q1_CLIPNODES];
		qboolean        buildBrushes = qtrue;

		if ( cnLump->filelen % (int)sizeof( q1_dclipnode_t ) ) {
			Com_Log( SEV_INFO, LOG_CH(ch_loading), S_COLOR_YELLOW "BSP_Q1_Load: %s clipnodes lump not aligned; skipping brush conversion\n",
			            name );
			buildBrushes = qfalse;
		}

		if ( buildBrushes ) {
			const q1_dclipnode_t *cnRaw = (const q1_dclipnode_t *)( base + cnLump->fileofs );
			int               numClipnodes = cnLump->filelen / (int)sizeof( q1_dclipnode_t );
			q1_dclipnode_t   *cn;
			int              *hull1Root;
			int              *hull0Root;
			int               s;

			cn = (q1_dclipnode_t *)Z_Malloc( numClipnodes * sizeof( q1_dclipnode_t ) );
			for ( i = 0; i < numClipnodes; i++ ) {
				cn[i].planenum    = (uint32_t)LittleLong( (int)cnRaw[i].planenum );
				cn[i].children[0] = (short)LittleShort( cnRaw[i].children[0] );
				cn[i].children[1] = (short)LittleShort( cnRaw[i].children[1] );
			}

			hull1Root = (int *)Z_Malloc( bsp->numSubModels * sizeof(int) );
			hull0Root = (int *)Z_Malloc( bsp->numSubModels * sizeof(int) );
			{
				const q1_dmodel_t *q1mods =
				    (const q1_dmodel_t *)( base + header.lumps[LUMP_Q1_MODELS].fileofs );
				for ( i = 0; i < bsp->numSubModels; i++ ) {
					hull1Root[i] = LittleLong( q1mods[i].headnode[1] );
					hull0Root[i] = LittleLong( q1mods[i].headnode[0] );
				}
			}
			Com_Log( SEV_TRACE, LOG_CH(ch_loading),
			    "BSP_Q1_Load: clipnodes: %d, submodel 0 hull1 root = %d, hull0 root = %d\n",
			    numClipnodes,
			    bsp->numSubModels > 0 ? hull1Root[0] : -1,
			    bsp->numSubModels > 0 ? hull0Root[0] : -1 );

			// Clipnode dedup (same pattern as leaf dedup)
			{
				int *cnRef = (int *)Z_Malloc( numClipnodes * sizeof(int) );
				int  extra = 0;
				memset( cnRef, 0, numClipnodes * sizeof(int) );
				for ( i = 0; i < numClipnodes; i++ ) {
					if ( cn[i].children[0] >= 0 ) cnRef[ (int)cn[i].children[0] ]++;
					if ( cn[i].children[1] >= 0 ) cnRef[ (int)cn[i].children[1] ]++;
				}
				for ( i = 0; i < numClipnodes; i++ )
					if ( cnRef[i] > 1 ) extra += cnRef[i] - 1;

				if ( extra > 0 ) {
					int origCount = numClipnodes;
					int cursor;
					q1_dclipnode_t *grown = (q1_dclipnode_t *)Z_Malloc(
					    ( origCount + extra ) * sizeof( q1_dclipnode_t ) );
					memcpy( grown, cn, origCount * sizeof( q1_dclipnode_t ) );
					Z_Free( cn );
					cn = grown;
					memset( cnRef, 0, origCount * sizeof(int) );
					cursor = origCount;
					for ( i = 0; i < origCount; i++ ) {
						int kk;
						for ( kk = 0; kk < 2; kk++ ) {
							int c = (int)(short)cn[i].children[kk];
							if ( c < 0 || c >= origCount ) continue;
							cnRef[c]++;
							if ( cnRef[c] > 1 ) {
								memcpy( &cn[cursor], &cn[c], sizeof( q1_dclipnode_t ) );
								cn[i].children[kk] = (short)cursor;
								cursor++;
							}
						}
					}
					numClipnodes = cursor;
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: clipnode-dup: %d -> %d (+%d)\n",
					            origCount, numClipnodes, extra );
				}
				Z_Free( cnRef );
			}

			// Expand planes to pairs: [2i]=original, [2i+1]=negated.
			// Node and clipnode planeNums are multiplied by 2.
			// Brushside planeNum = 2×origIdx + side (0=keep, 1=negate).
			{
				int nOrig = bsp->numPlanes;
				dplane_t *expanded = (dplane_t *)Z_Malloc( 2 * nOrig * sizeof( dplane_t ) );
				for ( i = 0; i < nOrig; i++ ) {
					expanded[2*i] = bsp->planes[i];
					expanded[2*i+1].normal[0] = -bsp->planes[i].normal[0];
					expanded[2*i+1].normal[1] = -bsp->planes[i].normal[1];
					expanded[2*i+1].normal[2] = -bsp->planes[i].normal[2];
					expanded[2*i+1].dist      = -bsp->planes[i].dist;
				}
				Z_Free( bsp->planes );
				bsp->planes    = expanded;
				bsp->numPlanes = 2 * nOrig;
				for ( i = 0; i < bsp->numNodes; i++ )
					bsp->nodes[i].planeNum *= 2;
				for ( i = 0; i < numClipnodes; i++ )
					cn[i].planenum = (uint32_t)( cn[i].planenum * 2 );
			}

			// Add clip shader (solid-clip, no draw surface)
			{
				int clipShaderIdx;
				dshader_t *ns = (dshader_t *)Z_Malloc( ( bsp->numShaders + 1 ) * sizeof( dshader_t ) );
				memcpy( ns, bsp->shaders, bsp->numShaders * sizeof( dshader_t ) );
				Z_Free( bsp->shaders );
				bsp->shaders  = ns;
				clipShaderIdx = bsp->numShaders++;
				Q_strncpyz( bsp->shaders[clipShaderIdx].shader, "*q1_hull1_clip",
				    sizeof( bsp->shaders[clipShaderIdx].shader ) );
				bsp->shaders[clipShaderIdx].surfaceFlags = SURF_NODRAW;
				/* Q1 hull-1 is player collision only — bullets use hull-0 point traces.
				 * CONTENTS_PLAYERCLIP: in MASK_PLAYERSOLID but NOT in MASK_SHOT,
				 * so bullets pass through to visible world geometry. */
				bsp->shaders[clipShaderIdx].contentFlags = CONTENTS_PLAYERCLIP;

				// Two-pass brush emission
				{
					ClipWalk_t walk;
					memset( &walk, 0, sizeof( walk ) );
					walk.clipShaderIdx = clipShaderIdx;
					walk.countingOnly  = qtrue;

					for ( s = 0; s < bsp->numSubModels; s++ ) {
						int root = hull1Root[s];
						if ( root >= 0 && root < numClipnodes )
							BSP_Q1_WalkClipTree( &walk, cn, numClipnodes, root );
					}

					bsp->numBrushes    = walk.numBrushes;
					bsp->brushes       = (dbrush_t *)BSP_Q1_ZAlloc(
					    bsp->numBrushes * sizeof( dbrush_t ) );
					bsp->numBrushSides = walk.numBrushSides;
					bsp->brushSides    = (dbrushside_t *)BSP_Q1_ZAlloc(
					    bsp->numBrushSides * sizeof( dbrushside_t ) );

					memset( &walk, 0, sizeof( walk ) );
					walk.clipShaderIdx = clipShaderIdx;
					walk.countingOnly  = qfalse;
					walk.brushOut      = bsp->brushes;
					walk.sideOut       = bsp->brushSides;

					for ( s = 0; s < bsp->numSubModels; s++ ) {
						int root       = hull1Root[s];
						int brushStart = walk.numBrushes;
						if ( root >= 0 && root < numClipnodes )
							BSP_Q1_WalkClipTree( &walk, cn, numClipnodes, root );
						bsp->subModels[s].firstBrush = brushStart;
						bsp->subModels[s].numBrushes = walk.numBrushes - brushStart;
					}

					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: hull1 brushes: %d (%d solid leaves), %d brushsides\n",
					            bsp->numBrushes, walk.numSolids, bsp->numBrushSides );
				}
			}

			// Store clipnodes + per-submodel hull roots for native Q1 tracing.
			// Skipped for render-only loads; Z_Free runs unconditionally to avoid leaks.
			if ( !(flags & BSP_LOAD_FLAG_RENDER_ONLY) )
				CMQ1_StoreClipnodes( cn, numClipnodes, hull1Root, hull0Root, bsp->numSubModels );

			Z_Free( hull0Root );
			Z_Free( hull1Root );
			Z_Free( cn );

			// --- Step 8: Brush dedup ---
			// Collapse geometrically identical brushes by sorting each brush's
			// plane-side set and comparing for equality via FNV-1a hash.
			if ( bsp->numBrushes > 0 ) {
				int   origBrushes = bsp->numBrushes;
				int   origSides   = bsp->numBrushSides;
				int  *brushRemap  = (int *)Z_Malloc( origBrushes * sizeof(int) );
				int **sortedSides = (int **)Z_Malloc( origBrushes * sizeof(int*) );
				int   bi, si, ci;

				// Build sorted-sides array per brush
				for ( bi = 0; bi < origBrushes; bi++ ) {
					const dbrush_t *b = &bsp->brushes[bi];
					int n = b->numSides;
					int *arr = (int *)Z_Malloc( n * sizeof(int) );
					for ( si = 0; si < n; si++ )
						arr[si] = bsp->brushSides[b->firstSide + si].planeNum;
					// Insertion sort (n ≤ ~64, fast enough)
					for ( si = 1; si < n; si++ ) {
						int key = arr[si], qi = si - 1;
						while ( qi >= 0 && arr[qi] > key ) { arr[qi+1] = arr[qi]; qi--; }
						arr[qi+1] = key;
					}
					sortedSides[bi] = arr;
				}

				// Hash-table dedup (FNV-1a, 2048 buckets, chained)
				#define DEDUP_BUCKETS 2048
				typedef struct DedupEntry_s { int brushIdx; struct DedupEntry_s *next; } DedupEntry_t;
				DedupEntry_t **ht = (DedupEntry_t **)Z_Malloc( DEDUP_BUCKETS * sizeof(DedupEntry_t*) );
				memset( ht, 0, DEDUP_BUCKETS * sizeof(DedupEntry_t*) );

				int numCanon = 0;
				for ( bi = 0; bi < origBrushes; bi++ ) {
					int n = bsp->brushes[bi].numSides;
					const int *ss = sortedSides[bi];
					// FNV-1a hash
					uint32_t h = 0x811c9dc5u;
					for ( si = 0; si < n; si++ ) { h ^= (uint32_t)ss[si]; h *= 0x01000193u; }
					uint32_t bucket = h & (DEDUP_BUCKETS - 1);

					qboolean found = qfalse;
					DedupEntry_t *e = ht[bucket];
					while ( e ) {
						int cbi = e->brushIdx;
						if ( bsp->brushes[cbi].numSides == n ) {
							qboolean eq = qtrue;
							for ( ci = 0; ci < n; ci++ )
								if ( sortedSides[cbi][ci] != ss[ci] ) { eq = qfalse; break; }
							if ( eq ) { brushRemap[bi] = cbi; found = qtrue; break; }
						}
						e = e->next;
					}
					if ( !found ) {
						DedupEntry_t *ne = (DedupEntry_t *)Z_Malloc( sizeof(DedupEntry_t) );
						ne->brushIdx  = bi;
						ne->next      = ht[bucket];
						ht[bucket]    = ne;
						brushRemap[bi] = bi;
						numCanon++;
					}
				}
				#undef DEDUP_BUCKETS

				// Free hash table entries
				{
					int bk;
					for ( bk = 0; bk < 2048; bk++ ) {
						DedupEntry_t *e = ht[bk];
						while ( e ) { DedupEntry_t *nx = e->next; Z_Free(e); e = nx; }
					}
					Z_Free( ht );
				}

				// Assign compact new indices (canonical brushes only)
				int *newIdx = (int *)Z_Malloc( origBrushes * sizeof(int) );
				{
					int counter = 0;
					for ( bi = 0; bi < origBrushes; bi++ )
						newIdx[bi] = (brushRemap[bi] == bi) ? counter++ : -1;
					// Map duplicates to their canonical's compact index
					for ( bi = 0; bi < origBrushes; bi++ )
						if ( newIdx[bi] == -1 ) newIdx[bi] = newIdx[brushRemap[bi]];
				}

				// Build compacted brushes and brushSides arrays
				{
					int newNumSides = 0;
					for ( bi = 0; bi < origBrushes; bi++ )
						if ( brushRemap[bi] == bi ) newNumSides += bsp->brushes[bi].numSides;

					dbrush_t     *nb = (dbrush_t *)BSP_Q1_ZAlloc( numCanon * sizeof(dbrush_t) );
					dbrushside_t *ns = (dbrushside_t *)BSP_Q1_ZAlloc( newNumSides * sizeof(dbrushside_t) );
					int sc = 0;
					for ( bi = 0; bi < origBrushes; bi++ ) {
						if ( brushRemap[bi] == bi ) {
							const dbrush_t *ob = &bsp->brushes[bi];
							int ni = newIdx[bi];
							nb[ni].firstSide = sc;
							nb[ni].numSides  = ob->numSides;
							nb[ni].shaderNum = ob->shaderNum;
							memcpy( &ns[sc], &bsp->brushSides[ob->firstSide],
							        ob->numSides * sizeof(dbrushside_t) );
							sc += ob->numSides;
						}
					}
					Z_Free( bsp->brushes );
					Z_Free( bsp->brushSides );
					bsp->brushes       = nb;
					bsp->brushSides    = ns;
					bsp->numBrushes    = numCanon;
					bsp->numBrushSides = newNumSides;
				}

				// Update submodel brush ranges to use compact indices
				{
					int sm;
					for ( sm = 0; sm < bsp->numSubModels; sm++ ) {
						int oFirst = bsp->subModels[sm].firstBrush;
						int oNum   = bsp->subModels[sm].numBrushes;
						int newFirst = -1, newCount = 0;
						for ( bi = oFirst; bi < oFirst + oNum; bi++ ) {
							if ( brushRemap[bi] == bi ) {
								if ( newFirst == -1 ) newFirst = newIdx[bi];
								newCount++;
							}
						}
						bsp->subModels[sm].firstBrush = (newFirst >= 0) ? newFirst : 0;
						bsp->subModels[sm].numBrushes = newCount;
					}
				}

				// Free scratch
				for ( bi = 0; bi < origBrushes; bi++ ) Z_Free( sortedSides[bi] );
				Z_Free( sortedSides );
				Z_Free( brushRemap );
				Z_Free( newIdx );

				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: brush-dedup: %d -> %d unique (%d -> %d sides)\n",
				            origBrushes, bsp->numBrushes, origSides, bsp->numBrushSides );
			}

			// Add axis-aligned bevel planes to clipnode-derived brushes so Q3 CM
			// box traces can slide past stair risers and ramp edges.
			BSP_Q1_AddBrushBevels( bsp );

			// --- Step 8.7: Remove redundant hull-1 brush planes (post-bevel, winding-clip) ---
			if ( bsp->numBrushes > 0 )
				BSP_Q1_RemoveRedundantPlanes( bsp );

			// Synthesize world-shell brushes to replace the 30 degenerate clipnodes
			// (outer solid hull) that Q1_ComputeBrushAABB skips in Step 9.
			BSP_Q1_SynthesizeWorldShell( bsp );

			// Synthesize AABB brushes for hull-0 liquid leaves so PM_SetWaterLevel
			// and the damage system see CONTENTS_WATER/SLIME/LAVA in those volumes.
			BSP_Q1_SynthesizeLiquidBrushes( bsp, base, &header.lumps[LUMP_Q1_LEAFS] );

			// --- Step 9: q3map2-style brush-to-BSP-tree insertion ---
			// For each canonical world brush, walk the render BSP and record
			// which leaves its AABB overlaps.  Only submodel 0 (world) brushes
			// are mapped; non-world submodel brushes are found via firstBrush/numBrushes.
			if ( bsp->numBrushes > 0 && bsp->numNodes > 0 ) {
				int worldFirst = bsp->subModels[0].firstBrush;
				int worldCount = bsp->subModels[0].numBrushes;
				int numLeafs   = bsp->numLeafs;
				int bi;

				LeafBrushBuckets_t lb;
				lb.numLeafs = numLeafs;
				lb.data  = (int **)Z_Malloc( numLeafs * sizeof(int*) );
				lb.count = (int  *)Z_Malloc( numLeafs * sizeof(int) );
				lb.cap   = (int  *)Z_Malloc( numLeafs * sizeof(int) );
				memset( lb.data,  0, numLeafs * sizeof(int*) );
				memset( lb.count, 0, numLeafs * sizeof(int) );
				memset( lb.cap,   0, numLeafs * sizeof(int) );

				int degenCount        = 0;
				int maxPerLeaf        = 0;
				int boundaryLeafCount = 0;

				for ( bi = worldFirst; bi < worldFirst + worldCount; bi++ ) {
					vec3_t mins, maxs;
					if ( !Q1_ComputeBrushAABB( bsp, &bsp->brushes[bi], mins, maxs ) ) {
						degenCount++;
						continue;
					}
					// Small epsilon inflate to catch flush-touching cases
					int k;
					for ( k = 0; k < 3; k++ ) { mins[k] -= 0.5f; maxs[k] += 0.5f; }
					Q1_InsertBrushIntoTree( bsp, &lb, bi, mins, maxs, 0 );
				}

				{
					int emptyLeaves = 0, sparseLeaves = 0, denseLeaves = 0, li;
					for ( li = 0; li < numLeafs; li++ ) {
						int n = lb.count[li];
						if      ( n == 0 ) emptyLeaves++;
						else if ( n <= 2 ) sparseLeaves++;
						else               denseLeaves++;
					}
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: Step 9 tree walk coverage (pre-shell):\n" );
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  empty leaves (0 brushes): %d / %d\n", emptyLeaves, numLeafs );
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  sparse (1-2 brushes):     %d / %d\n", sparseLeaves, numLeafs );
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "  dense (3+ brushes):       %d / %d\n", denseLeaves, numLeafs );
				}

				// Proximity-based shell brush insertion.
				// Shell brushes occupy [wmins-512, wmins-256] — outside BSP tree spatial
				// coverage — so the tree walk above inserts them into no leaves.
				// Add all 6 shell brushes to any leaf whose AABB is within SHELL_PROXIMITY
				// units of a world boundary face.  Interior leaves are untouched.
				{
					static const float SHELL_PROXIMITY = 512.0f;
					int shellStart = worldFirst + worldCount - 6;
					int si, li;
					for ( li = 0; li < numLeafs; li++ ) {
						qboolean isBoundary = qfalse;
						int axis;
						for ( axis = 0; axis < 3; axis++ ) {
							float leafMin = (float)bsp->leafs[li].mins[axis];
							float leafMax = (float)bsp->leafs[li].maxs[axis];
							float distMin = leafMin - bsp->subModels[0].mins[axis];
							float distMax = bsp->subModels[0].maxs[axis] - leafMax;
							if ( distMin >= 0.0f && distMin <= SHELL_PROXIMITY ) isBoundary = qtrue;
							if ( distMax >= 0.0f && distMax <= SHELL_PROXIMITY ) isBoundary = qtrue;
						}
						if ( isBoundary ) {
							for ( si = 0; si < 6; si++ )
								Q1_BucketAppend( &lb, li, shellStart + si );
							boundaryLeafCount++;
						}
					}

					Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: shell brushes inserted into %d / %d leaves (proximity %.0f)\n",
					            boundaryLeafCount, numLeafs, SHELL_PROXIMITY );
					if ( boundaryLeafCount == 0 )
						Com_Log( SEV_INFO, LOG_CH(ch_loading), S_COLOR_RED "BSP_Q1_Load: ERROR proximity pass found zero boundary"
						         " leaves — shell brushes unreachable, CM escape possible\n" );


				}

				// Flatten per-leaf buckets into bsp->leafBrushes
				{
					int totalEntries = 0, li;
					for ( li = 0; li < numLeafs; li++ ) totalEntries += lb.count[li];

					bsp->numLeafBrushes = totalEntries;
					bsp->leafBrushes    = (int *)BSP_Q1_ZAlloc( totalEntries * sizeof(int) );

					int cursor = 0;
					for ( li = 0; li < numLeafs; li++ ) {
						bsp->leafs[li].firstLeafBrush = cursor;
						bsp->leafs[li].numLeafBrushes = lb.count[li];
						if ( lb.count[li] > maxPerLeaf ) maxPerLeaf = lb.count[li];
						if ( lb.count[li] > 0 ) {
							memcpy( &bsp->leafBrushes[cursor], lb.data[li],
							        lb.count[li] * sizeof(int) );
							cursor += lb.count[li];
						}
					}
				}

				// Free buckets
				{
					int li;
					for ( li = 0; li < numLeafs; li++ ) if ( lb.data[li] ) Z_Free( lb.data[li] );
					Z_Free( lb.data );
					Z_Free( lb.count );
					Z_Free( lb.cap );
				}

				if ( degenCount > 0 )
					Com_Log( SEV_TRACE, LOG_CH(ch_loading), S_COLOR_YELLOW "BSP_Q1_Load: %d degenerate brushes skipped in tree walk\n",
					            degenCount );

				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: hull1 final: %d unique brushes, %d sides, %d leafBrush entries\n",
				            bsp->numBrushes, bsp->numBrushSides, bsp->numLeafBrushes );
				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: leafBrush entries per leaf: avg %.1f, max %d\n",
				            numLeafs > 0 ? (float)bsp->numLeafBrushes / numLeafs : 0.0f, maxPerLeaf );

			}

			// --- Step 8.5: De-expand hull-1 brush planes back to world geometry ---
			// Q1 hull-1 is a Minkowski sum: each plane dist already incorporates the
			// player bbox expansion.  Q3 CM re-expands brush planes by the player bbox,
			// causing double-expansion (walls 16 u thicker, floors 24 u thicker).
			// Fix: append new de-expanded planes to bsp->planes and re-point brushsides.
			// Render BSP nodes keep using the original hull-1 planes (indices 0..2N-1).
			// NOTE: Step 9 (leaf-brush insertion) runs first so brush AABBs are computed
			// from hull-1 planes (conservatively correct); de-expansion only affects CM.
			if ( bsp->numBrushSides > 0 ) {
				// XY uses exact Q3 player bbox (±15): the round-trip de-expand+re-expand
				// yields the hull-1 distance exactly, so adjacent brushes share a
				// zero-width boundary (no gap).
				// Z uses Q3 bbox + 0.125 epsilon so the effective floor is 0.125 u below
				// the hull-1 boundary, giving d=+0.125 at spawn (startout=true).
				static const float q1H1Mins[3] = { -15.0f, -15.0f, -24.125f };
				static const float q1H1Maxs[3] = {  15.0f,  15.0f,  32.125f };
				int oldNP = bsp->numPlanes;
				int np    = oldNP;
				int si;
				dplane_t *allPlanes = (dplane_t *)Z_Malloc(
				    ( oldNP + bsp->numBrushSides ) * sizeof( dplane_t ) );
				memcpy( allPlanes, bsp->planes, oldNP * sizeof( dplane_t ) );
				for ( si = 0; si < bsp->numBrushSides; si++ ) {
					dbrushside_t *s = &bsp->brushSides[si];
					// Only hull-1 clip brushes need de-expansion; liquid/water brushes
					// are already in world space and must not be contracted.
					if ( !( bsp->shaders[s->shaderNum].contentFlags &
					        ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) ) )
						continue;
					const dplane_t *op = &bsp->planes[s->planeNum];
					float offset = 0.0f;
					int k;
					for ( k = 0; k < 3; k++ )
						offset += op->normal[k] *
						          ( op->normal[k] > 0.0f ? q1H1Mins[k] : q1H1Maxs[k] );
					allPlanes[np].normal[0] = op->normal[0];
					allPlanes[np].normal[1] = op->normal[1];
					allPlanes[np].normal[2] = op->normal[2];
					allPlanes[np].dist      = op->dist + offset;
					s->planeNum = np++;
				}
				Z_Free( bsp->planes );
				bsp->planes    = allPlanes;
				bsp->numPlanes = np;
				Com_Log( SEV_TRACE, LOG_CH(ch_loading), "BSP_Q1_Load: hull-1 de-expansion: %d brush planes (total %d)\n",
				            bsp->numBrushSides, np );
			}
		}
	}

	BSP_Q1_DiagBrushes( name, bsp, diagEnabled );
	if ( diagEnabled )
		Cvar_Set( "q1_trace_debug", "0" );

	// ---- Build synthetic Q3 binary rawData ----
	// RE_LoadWorldMap casts rawData as (dheader_t*) expecting Q3 format.
	// This replaces the Q1 raw bytes with a valid Q3 blob built from our
	// already-translated struct fields.
	BSP_Q1_BuildSyntheticQ3Data( bsp );

	Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Q1_Load: translated %d surfaces, %d verts, %d indexes, %d shaders\n",
		bsp->numSurfaces, bsp->numDrawVerts, bsp->numDrawIndexes, bsp->numShaders );

	*bspFile = bsp;
	return qtrue;
}

#if FEAT_RECAST_NAVMESH

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

/* Reverse winding: Q1 is CW from above; Recast expects CCW. */
static void NavGeomWriter_WriteTri( NavGeomWriter_t *w, int a, int b, int c,
                                    unsigned char area )
{
	w->tris[ w->numTris * 3 + 0 ] = c;
	w->tris[ w->numTris * 3 + 1 ] = b;
	w->tris[ w->numTris * 3 + 2 ] = a;
	w->areas[ w->numTris ] = area;
	w->numTris++;
}

static void BSP_Q1_CountPlanarOrSoup( const dsurface_t *surf, int *nv, int *nt )
{
	*nv += surf->numVerts;
	*nt += surf->numIndexes / 3;
}

static void BSP_Q1_FillPlanarOrSoup( NavGeomWriter_t  *w,
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

static qboolean BSP_Q1_SurfaceIsWalkable( const dsurface_t *surf,
                                           const dshader_t  *shader )
{
	if ( shader->surfaceFlags & ( SURF_NODRAW | SURF_SKY | SURF_NONSOLID ) )
		return qfalse;
	if ( surf->surfaceType == MST_FLARE || surf->surfaceType == MST_BAD )
		return qfalse;
	if ( !( shader->contentFlags & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP |
	                                  CONTENTS_LAVA  | CONTENTS_SLIME | CONTENTS_WATER ) ) )
		return qfalse;
	return qtrue;
}

/* NODRAW is intentionally allowed here: clip brushes are invisible but blocking. */
static qboolean BSP_Q1_BrushIsWalkable( const dbrush_t *br, const dshader_t *sh )
{
	if ( sh->surfaceFlags & ( SURF_SKY | SURF_NONSOLID ) )
		return qfalse;
	if ( !( sh->contentFlags & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP |
	                              CONTENTS_LAVA  | CONTENTS_SLIME | CONTENTS_WATER ) ) )
		return qfalse;
	if ( br->numSides < 4 )
		return qfalse;
	return qtrue;
}

#define MAX_BRUSH_FACE_VERTS 64

/* Extract the polygon on face sideIdx of brush br, clipped by all other brush planes.
 * Vertices are sorted CCW when viewed from inside the brush so that after
 * NavGeomWriter_WriteTri's reversal the final winding is CCW from outside (Recast CCW).
 * Returns vertex count; returns 0 for degenerate faces. */
static int BSP_Q1_BrushFacePolygon( const dbrush_t *br, int sideIdx,
                                     const dplane_t *allPlanes, const dbrushside_t *allSides,
                                     vec3_t outPoly[MAX_BRUSH_FACE_VERTS] )
{
	int            firstSide    = br->firstSide;
	int            numSides     = br->numSides;
	int            facePlaneIdx = allSides[firstSide + sideIdx].planeNum;
	const dplane_t *faceP       = &allPlanes[facePlaneIdx];
	vec3_t         raw[MAX_BRUSH_FACE_VERTS];
	float          angles[MAX_BRUSH_FACE_VERTS];
	int            numRaw = 0;
	int            ii, jj, mm;

	for ( ii = 0; ii < numSides && numRaw < MAX_BRUSH_FACE_VERTS; ii++ ) {
		if ( ii == sideIdx ) continue;
		for ( jj = ii + 1; jj < numSides && numRaw < MAX_BRUSH_FACE_VERTS; jj++ ) {
			if ( jj == sideIdx ) continue;
			const dplane_t *p2 = &allPlanes[ allSides[firstSide + ii].planeNum ];
			const dplane_t *p3 = &allPlanes[ allSides[firstSide + jj].planeNum ];
			vec3_t     v;
			qboolean   inside;
			if ( !Q1_Intersect3Planes( faceP, p2, p3, v ) ) continue;
			inside = qtrue;
			for ( mm = 0; mm < numSides && inside; mm++ ) {
				const dplane_t *pm;
				if ( mm == sideIdx || mm == ii || mm == jj ) continue;
				pm = &allPlanes[ allSides[firstSide + mm].planeNum ];
				if ( DotProduct( v, pm->normal ) - pm->dist > 0.01f )
					inside = qfalse;
			}
			if ( inside &&
			     fabsf( v[0] ) <= Q1_MAP_COORD_MAX &&
			     fabsf( v[1] ) <= Q1_MAP_COORD_MAX &&
			     fabsf( v[2] ) <= Q1_MAP_COORD_MAX ) {
				// VectorCopy is `((b)[0]=…,(b)[1]=…,(b)[2]=…)` and would
				// evaluate `numRaw++` three times if the post-increment were
				// inside the macro argument — separating fixes that latent bug.
				VectorCopy( v, raw[numRaw] );
				numRaw++;
			}
		}
	}

	if ( numRaw < 3 ) return 0;

	/* Centroid for angle sorting. */
	vec3_t centroid = { 0, 0, 0 };
	for ( ii = 0; ii < numRaw; ii++ ) {
		centroid[0] += raw[ii][0];
		centroid[1] += raw[ii][1];
		centroid[2] += raw[ii][2];
	}
	centroid[0] /= (float)numRaw;
	centroid[1] /= (float)numRaw;
	centroid[2] /= (float)numRaw;

	/* Reference direction: first vertex relative to centroid, normalized. */
	vec3_t refNorm;
	{
		vec3_t ref;
		float  refLen;
		VectorSubtract( raw[0], centroid, ref );
		refLen = VectorLength( ref );
		if ( refLen < 1e-4f ) return 0;
		VectorScale( ref, 1.0f / refLen, refNorm );
	}

	/* viewNormal = inward normal; atan2 gives CCW order from inside. */
	vec3_t viewNormal;
	VectorNegate( faceP->normal, viewNormal );

	for ( ii = 0; ii < numRaw; ii++ ) {
		vec3_t d, cross;
		float  dlen, cosA, sinA;
		VectorSubtract( raw[ii], centroid, d );
		dlen = VectorLength( d );
		if ( dlen < 1e-4f ) { angles[ii] = 0.0f; continue; }
		VectorScale( d, 1.0f / dlen, d );
		cosA = DotProduct( d, refNorm );
		CrossProduct( d, refNorm, cross );
		sinA = DotProduct( cross, viewNormal );
		angles[ii] = atan2f( sinA, cosA );
	}

	/* Insertion sort by angle ascending. */
	for ( ii = 1; ii < numRaw; ii++ ) {
		float  av = angles[ii];
		vec3_t rv;
		VectorCopy( raw[ii], rv );
		jj = ii - 1;
		while ( jj >= 0 && angles[jj] > av ) {
			angles[jj+1] = angles[jj];
			VectorCopy( raw[jj], raw[jj+1] );
			jj--;
		}
		angles[jj+1] = av;
		VectorCopy( rv, raw[jj+1] );
	}

	for ( ii = 0; ii < numRaw; ii++ )
		VectorCopy( raw[ii], outPoly[ii] );
	return numRaw;
}

static qboolean BSP_Q1_ExtractNavGeometry( const bspFile_t  *bsp,
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

		if ( !BSP_Q1_SurfaceIsWalkable( surf, shader ) )
			continue;

		switch ( surf->surfaceType ) {
		case MST_PLANAR:
		case MST_TRIANGLE_SOUP:
			BSP_Q1_CountPlanarOrSoup( surf, &numVerts, &numTris );
			break;
		default:
			break;
		}
	}

	/* --- Pass 1 (brushes): count triangles from hull-1 clip geometry --- */
	{
		const dmodel_t *sm0       = &bsp->subModels[0];
		int             shellFirst = sm0->firstBrush + sm0->numBrushes - 6;
		int             b, s;
		for ( b = sm0->firstBrush; b < sm0->firstBrush + sm0->numBrushes; b++ ) {
			const dbrush_t  *br = &bsp->brushes[b];
			const dshader_t *sh = &bsp->shaders[br->shaderNum];
			if ( b >= shellFirst ) continue;
			if ( !BSP_Q1_BrushIsWalkable( br, sh ) ) continue;
			for ( s = 0; s < br->numSides; s++ ) {
				vec3_t polyBuf[MAX_BRUSH_FACE_VERTS];
				int    nv = BSP_Q1_BrushFacePolygon( br, s, bsp->planes, bsp->brushSides, polyBuf );
				if ( nv < 3 ) continue;
				numVerts += nv;
				numTris  += nv - 2;
			}
		}
	}

	memset( geom, 0, sizeof(*geom) );

	if ( numVerts == 0 || numTris == 0 ) {
		COM_WARN( LOG_CH(ch_loading), "[NAV] no walkable tris extracted from %s\n", bsp->name );
		return qfalse;
	}

	{
		static char s_lastMap[256];
		static int  s_lastTris;
		if ( *s_lastMap && Q_stricmp( s_lastMap, bsp->name ) == 0 && s_lastTris > 0 ) {
			int delta = numTris - s_lastTris;
			if ( delta < 0 ) delta = -delta;
			if ( delta * 100 / s_lastTris > 20 ) {
				COM_WARN( LOG_CH(ch_loading), "[NAV] tri count changed >20%% on %s: was %d, now %d\n",
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
		unsigned char area;

		if ( !BSP_Q1_SurfaceIsWalkable( surf, shader ) )
			continue;

		if ( shader->contentFlags & CONTENTS_WATER )
			area = (unsigned char)NAVAREA_WATER;
		else if ( shader->contentFlags & ( CONTENTS_LAVA | CONTENTS_SLIME ) )
			area = (unsigned char)NAVAREA_LAVA;
		else
			area = (unsigned char)NAVAREA_GROUND;

		switch ( surf->surfaceType ) {
		case MST_PLANAR:
		case MST_TRIANGLE_SOUP:
			BSP_Q1_FillPlanarOrSoup( &wr, surf,
			                         bsp->drawVerts, bsp->drawIndexes, area );
			break;
		default:
			break;
		}
	}

	/* --- Pass 2 (brushes): emit triangles from hull-1 clip geometry --- */
	{
		const dmodel_t *sm0        = &bsp->subModels[0];
		int             shellFirst  = sm0->firstBrush + sm0->numBrushes - 6;
		int             skipped     = 0;
		int             b, s, v;
		for ( b = sm0->firstBrush; b < sm0->firstBrush + sm0->numBrushes; b++ ) {
			const dbrush_t  *br = &bsp->brushes[b];
			const dshader_t *sh = &bsp->shaders[br->shaderNum];
			unsigned char    area;
			if ( b >= shellFirst ) { skipped++; continue; }
			if ( !BSP_Q1_BrushIsWalkable( br, sh ) ) { skipped++; continue; }
			if ( sh->contentFlags & CONTENTS_WATER )
				area = (unsigned char)NAVAREA_WATER;
			else if ( sh->contentFlags & ( CONTENTS_LAVA | CONTENTS_SLIME ) )
				area = (unsigned char)NAVAREA_LAVA;
			else
				area = (unsigned char)NAVAREA_GROUND;
			for ( s = 0; s < br->numSides; s++ ) {
				vec3_t poly[MAX_BRUSH_FACE_VERTS];
				float  r[3];
				int    base, nv;
				nv = BSP_Q1_BrushFacePolygon( br, s, bsp->planes, bsp->brushSides, poly );
				if ( nv < 3 ) continue;
				base = wr.numVerts;
				for ( v = 0; v < nv; v++ ) {
					Nav_QuakeToRecast( poly[v], r );
					NavGeomWriter_WriteVert( &wr, r );
				}
				for ( v = 1; v + 1 < nv; v++ )
					NavGeomWriter_WriteTri( &wr, base, base + v, base + v + 1, area );
			}
		}
		Com_Log( SEV_INFO, LOG_CH(ch_loading), "[NAV] %s: Q1 brush extraction skipped %d brushes (shell+non-walkable)\n",
		         bsp->name, skipped );
	}

	geom->numVerts = wr.numVerts;
	geom->numTris  = wr.numTris;

	/* Layer B: drop triangles that reference any out-of-bounds vertex.
	 * Such vertices arise when Q1_BrushFacePolygon's inside check passes
	 * due to FP cancellation on nearly-degenerate plane intersections.
	 * Must run BEFORE rcCalcBounds so inflated coords never reach Recast. */
	{
		int      rTri, wTri   = 0;
		int      dropped      = 0;
		float    firstBad[3]  = { 0, 0, 0 };

		for ( rTri = 0; rTri < geom->numTris; rTri++ ) {
			const int i0 = geom->tris[rTri * 3 + 0];
			const int i1 = geom->tris[rTri * 3 + 1];
			const int i2 = geom->tris[rTri * 3 + 2];
			qboolean  bad = qfalse;
			int       vi;

			for ( vi = 0; vi < 3 && !bad; vi++ ) {
				const int   idx = (vi == 0) ? i0 : (vi == 1) ? i1 : i2;
				const float *v  = &geom->verts[idx * 3];
				if ( fabsf( v[0] ) > Q1_MAP_COORD_MAX ||
				     fabsf( v[1] ) > Q1_MAP_COORD_MAX ||
				     fabsf( v[2] ) > Q1_MAP_COORD_MAX ) {
					bad = qtrue;
					if ( !dropped ) {
						firstBad[0] = v[0];
						firstBad[1] = v[1];
						firstBad[2] = v[2];
					}
				}
			}

			if ( bad ) {
				dropped++;
				continue;
			}

			if ( wTri != rTri ) {
				geom->tris[wTri * 3 + 0] = i0;
				geom->tris[wTri * 3 + 1] = i1;
				geom->tris[wTri * 3 + 2] = i2;
				geom->areas[wTri]         = geom->areas[rTri];
			}
			wTri++;
		}

		if ( dropped > 0 ) {
			Com_Log( SEV_WARN, LOG_CH(ch_loading),
			    "[NAV] %s: dropped %d degenerate triangles (first bad vert: %.1f %.1f %.1f)\n",
			    bsp->name, dropped, firstBad[0], firstBad[1], firstBad[2] );

			/* Zero out remaining out-of-bounds verts so rcCalcBounds does
			 * not inflate the heightfield bounding box. */
			{
				int vi;
				for ( vi = 0; vi < geom->numVerts; vi++ ) {
					float *v = &geom->verts[vi * 3];
					if ( fabsf( v[0] ) > Q1_MAP_COORD_MAX ||
					     fabsf( v[1] ) > Q1_MAP_COORD_MAX ||
					     fabsf( v[2] ) > Q1_MAP_COORD_MAX ) {
						v[0] = v[1] = v[2] = 0.0f;
					}
				}
			}

			geom->numTris = wTri;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_loading), "[NAV] %s: %d verts, %d tris extracted\n",
	            bsp->name, geom->numVerts, geom->numTris );
	return qtrue;
}

#endif /* FEAT_RECAST_NAVMESH */

const bspFormat_t bspFormatQ1 = {
	"Quake 1 BSP29",
	0,               // no ident field in BSP29
	BSP_VERSION_Q1,
	BSP_Q1_Detect,
	BSP_Q1_Load,
#if FEAT_RECAST_NAVMESH
	BSP_Q1_ExtractNavGeometry
#else
	NULL
#endif
};
