// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// cm_q1.c — Q1 collision: clipnode/leaf-contents storage + the CANONICAL
// COLLISION MODEL that is the single live Q1 collision representation.
//
// At load, the Q1 BSP loader stores this map's clipnodes and leaf contents here,
// then CMQ1_BuildCanonicalModel transpiles the hull-0 point geometry (plus the
// deflated hull-1 clip brushes) into the engine's native Q3 collision-model
// structures (cbrush_t / cbrushside_t / cplane_t / cNode_t / cLeaf_t), so the
// existing generic brush-trace (cm_trace.c) traces it with zero new trace logic.
// The canonical model IS the live path: cmTracer_q1canon dispatches every Q1
// world trace through the generic engine over the canonical arrays. There is no
// legacy clipnode runtime — a Q1 map that cannot build canonically hard-fails at
// the loader (no silent fallback). Only the clipnode PARSING remains, as the
// conversion source for the build.

#include "cm_local.h"
#include "qfiles.h"

LOG_DECLARE_CHANNEL( ch_cm_canon, "collision.canon" );

// ---------------------------------------------------------------------------
// Storage management
// ---------------------------------------------------------------------------

void CMQ1_StoreClipnodes( const q1_dclipnode_t *cn, int numCn,
                           const int *hull1Roots, const int *hull0Roots, int numSubmodels ) {
	// Free only clipnode data — do NOT call CMQ1_FreeData() here because
	// CMQ1_StoreLeafContents is called before this in bsp_q1.c and FreeData
	// would wipe leafContents before any PointContents query fires.
	if ( cm.q1.clipnodes )     { Z_Free( cm.q1.clipnodes );     cm.q1.clipnodes     = NULL; }
	if ( cm.q1.submodelRoots ) { Z_Free( cm.q1.submodelRoots ); cm.q1.submodelRoots = NULL; }
	if ( cm.q1.hull0Roots )    { Z_Free( cm.q1.hull0Roots );    cm.q1.hull0Roots    = NULL; }
	cm.q1.numClipnodes     = 0;
	cm.q1.numSubmodelRoots = 0;

	cm.q1.numClipnodes = numCn;
	cm.q1.clipnodes = (q1_dclipnode_t *)Z_Malloc( numCn * sizeof( q1_dclipnode_t ) );
	memcpy( cm.q1.clipnodes, cn, numCn * sizeof( q1_dclipnode_t ) );

	cm.q1.numSubmodelRoots = numSubmodels;
	cm.q1.submodelRoots = (int *)Z_Malloc( numSubmodels * sizeof( int ) );
	memcpy( cm.q1.submodelRoots, hull1Roots, numSubmodels * sizeof( int ) );

	cm.q1.hull0Roots = (int *)Z_Malloc( numSubmodels * sizeof( int ) );
	memcpy( cm.q1.hull0Roots, hull0Roots, numSubmodels * sizeof( int ) );

	// The Q1 loader reached here, so the loaded map IS Q1 — record that fact for
	// the canonical build (which keys on it, not on a tracer pointer identity).
	cm.q1.isQ1Format = qtrue;
}

void CMQ1_StoreLeafContents( const int *contents, int numLeafs ) {
	if ( cm.q1.leafContents ) {
		Z_Free( cm.q1.leafContents );
		cm.q1.leafContents = NULL;
	}
	cm.q1.numLeafs = numLeafs;
	cm.q1.leafContents = (int *)Z_Malloc( numLeafs * sizeof( int ) );
	memcpy( cm.q1.leafContents, contents, numLeafs * sizeof( int ) );
}

void CMQ1_FreeData( void ) {
	if ( cm.q1.clipnodes ) {
		Z_Free( cm.q1.clipnodes );
		cm.q1.clipnodes = NULL;
	}
	if ( cm.q1.submodelRoots ) {
		Z_Free( cm.q1.submodelRoots );
		cm.q1.submodelRoots = NULL;
	}
	if ( cm.q1.hull0Roots ) {
		Z_Free( cm.q1.hull0Roots );
		cm.q1.hull0Roots = NULL;
	}
	if ( cm.q1.leafContents ) {
		Z_Free( cm.q1.leafContents );
		cm.q1.leafContents = NULL;
	}
	cm.q1.numClipnodes     = 0;
	cm.q1.numSubmodelRoots = 0;
	cm.q1.numLeafs         = 0;
	cm.q1.isQ1Format       = qfalse;

	// Release the canonical collision model built for the previous Q1 map.
	CMQ1_FreeCanonicalModel();
}

// ===========================================================================
//
//  CANONICAL COLLISION MODEL  (Q1 hull-0 clipnode-to-brush conversion)
//
//  Walks the hull-0 BSP node tree (cm.nodes + cm.q1.leafContents), emits one
//  convex brush per non-empty leaf in the engine's native cbrush_t form, adds
//  q3map-style axial + edge bevels so the generic runtime plane-expansion trace
//  is correct for arbitrary boxes, and registers each brush in the leaf it came
//  from. The live hull-0 node tree is reused unchanged as the spatial structure
//  (leaf-brush list of 1 per producing leaf). The whole model is a self-contained
//  set of cm-typed arrays that the live tracer swaps in to run the STOCK generic
//  trace (cm_trace.c) — one trace engine for both Q1 and Q3.
//
// ===========================================================================

// Winding tolerances — mirror the map_q1_bsp.c bevel constants so the two
// clipnode-to-brush conversions stay numerically in sync.
#define CANON_NORMAL_EPSILON   1e-3f    // normal-component match for plane dedup
#define CANON_DIST_EPSILON     1.0f     // distance match for plane dedup / inside test
#define CANON_SNAP_EPSILON     1e-5f    // snap near-unit normal components to exactly +/-1
#define CANON_EDGE_DEGENERATE  0.5f     // normalized-length floor for edge/plane rejection
#define CANON_WINDING_EPSILON  0.1f     // point-on-plane tolerance for edge-bevel outer-hull test
#define CANON_MAP_BOUNDS       65536.0f // large-quad radius for base winding generation
#define CANON_MAX_WINDING_PTS  64
#define CANON_MAX_BRUSH_SIDES  128
// Q1 map coordinates are bounded to +/-16384; a brush AABB axis that stays at
// the map-bounds sentinel after path-plane bounding is treated as unbounded and
// gets an explicit axial box bevel from the winding AABB.
#define CANON_MAX_CLIP_DEPTH   512

typedef struct {
	int   numPts;
	float pts[CANON_MAX_WINDING_PTS][3];
} canonWinding_t;

// Canonical collision model — the live Q1 world representation. Built at load and
// swapped into cm.* by the live tracer (cmTracer_q1canon) for the duration of each
// world query. All arrays Z_Malloc'd and released by CMQ1_FreeCanonicalModel
// (called from CMQ1_FreeData).
typedef struct {
	qboolean   valid;

	int        numPlanes;
	cplane_t  *planes;

	int        numBrushSides;
	cbrushside_t *brushsides;

	int        numBrushes;
	cbrush_t  *brushes;

	int        numNodes;
	cNode_t   *nodes;

	int        numLeafs;
	cLeaf_t   *leafs;

	int        numLeafBrushes;
	int       *leafbrushes;

	// stats (per map, reported by the build)
	int        statSolidBrushes;
	int        statWaterBrushes;
	int        statSlimeBrushes;
	int        statLavaBrushes;
	int        statBoxBevels;
	int        statEdgeBevels;
	int        statLeavesRegistered;
	int        statDegenerate;
	// hull-1 clip-extraction stats
	int        statClipRegions;       // hull-1 SOLID leaf regions walked
	int        statClipDeflateDegen;  // regions dropped: thinner than the deflation
	int        statClipRedundant;     // regions dropped: contained in hull-0 solid
	int        statClipBrushes;       // clip brushes emitted (survivors)
	int        buildMsec;
} cmCanon_t;

static cmCanon_t cmCanon;

// --- growable side/plane accumulator used during the build ------------------
typedef struct {
	cplane_t     *planes;   int numPlanes;   int capPlanes;
	cbrushside_t *sides;    int numSides;    int capSides;
	cbrush_t     *brushes;  int numBrushes;  int capBrushes;
	int          *leafbrushes; int numLeafBrushes; int capLeafBrushes;
} canonBuild_t;

static void Canon_GrowPlanes( canonBuild_t *b, int need ) {
	if ( b->numPlanes + need <= b->capPlanes ) return;
	int nc = b->capPlanes ? b->capPlanes : 1024;
	while ( b->numPlanes + need > nc ) nc *= 2;
	cplane_t *np = (cplane_t *)Z_Malloc( nc * sizeof( cplane_t ) );
	if ( b->planes ) { memcpy( np, b->planes, b->numPlanes * sizeof( cplane_t ) ); Z_Free( b->planes ); }
	b->planes = np; b->capPlanes = nc;
}
static void Canon_GrowSides( canonBuild_t *b, int need ) {
	if ( b->numSides + need <= b->capSides ) return;
	int nc = b->capSides ? b->capSides : 4096;
	while ( b->numSides + need > nc ) nc *= 2;
	cbrushside_t *np = (cbrushside_t *)Z_Malloc( nc * sizeof( cbrushside_t ) );
	if ( b->sides ) { memcpy( np, b->sides, b->numSides * sizeof( cbrushside_t ) ); Z_Free( b->sides ); }
	b->sides = np; b->capSides = nc;
}
static void Canon_GrowBrushes( canonBuild_t *b, int need ) {
	if ( b->numBrushes + need <= b->capBrushes ) return;
	int nc = b->capBrushes ? b->capBrushes : 1024;
	while ( b->numBrushes + need > nc ) nc *= 2;
	cbrush_t *np = (cbrush_t *)Z_Malloc( nc * sizeof( cbrush_t ) );
	if ( b->brushes ) { memcpy( np, b->brushes, b->numBrushes * sizeof( cbrush_t ) ); Z_Free( b->brushes ); }
	b->brushes = np; b->capBrushes = nc;
}
static void Canon_GrowLeafBrushes( canonBuild_t *b, int need ) {
	if ( b->numLeafBrushes + need <= b->capLeafBrushes ) return;
	int nc = b->capLeafBrushes ? b->capLeafBrushes : 1024;
	while ( b->numLeafBrushes + need > nc ) nc *= 2;
	int *np = (int *)Z_Malloc( nc * sizeof( int ) );
	if ( b->leafbrushes ) { memcpy( np, b->leafbrushes, b->numLeafBrushes * sizeof( int ) ); Z_Free( b->leafbrushes ); }
	b->leafbrushes = np; b->capLeafBrushes = nc;
}

// Find an existing plane (normal+dist within epsilon) in the build's plane pool,
// or append a new one. Returns the plane index. Also sets type/signbits so the
// generic trace's axial fast path and signbits box-offset lookup are valid.
static int Canon_FindOrAddPlane( canonBuild_t *b, const float normal[3], float dist ) {
	int i;
	for ( i = 0; i < b->numPlanes; i++ ) {
		if ( fabsf( b->planes[i].normal[0] - normal[0] ) < CANON_NORMAL_EPSILON &&
		     fabsf( b->planes[i].normal[1] - normal[1] ) < CANON_NORMAL_EPSILON &&
		     fabsf( b->planes[i].normal[2] - normal[2] ) < CANON_NORMAL_EPSILON &&
		     fabsf( b->planes[i].dist      - dist      ) < CANON_DIST_EPSILON )
			return i;
	}
	Canon_GrowPlanes( b, 1 );
	cplane_t *p = &b->planes[b->numPlanes];
	memset( p, 0, sizeof( *p ) );
	VectorCopy( normal, p->normal );
	p->dist     = dist;
	p->type     = PlaneTypeForNormal( p->normal );
	SetPlaneSignbits( p );
	return b->numPlanes++;
}

// SnapNormal: axis-snap near-unit components to exactly +/-1 (q3map SnapNormal).
static void Canon_SnapNormal( float n[3] ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		if ( fabsf( n[i] - 1.0f ) < CANON_SNAP_EPSILON ) { n[0]=0; n[1]=0; n[2]=0; n[i]= 1.0f; return; }
		if ( fabsf( n[i] + 1.0f ) < CANON_SNAP_EPSILON ) { n[0]=0; n[1]=0; n[2]=0; n[i]=-1.0f; return; }
	}
	float len = sqrtf( n[0]*n[0] + n[1]*n[1] + n[2]*n[2] );
	if ( len > 1e-8f ) { n[0]/=len; n[1]/=len; n[2]/=len; }
}

// Large quad in a plane, centred on normal*dist (q3map BaseWindingForPlane).
static void Canon_BaseWindingForPlane( const float normal[3], float dist, canonWinding_t *w ) {
	int   i, x = -1;
	float max = -1.0f;
	float vup[3] = {0,0,0}, vright[3], org[3];
	float len, d;

	for ( i = 0; i < 3; i++ ) { float v = fabsf( normal[i] ); if ( v > max ) { max = v; x = i; } }
	VectorClear( vup );
	if ( x == 2 ) vup[0] = 1.0f; else vup[2] = 1.0f;

	d = vup[0]*normal[0] + vup[1]*normal[1] + vup[2]*normal[2];
	vup[0] -= normal[0]*d; vup[1] -= normal[1]*d; vup[2] -= normal[2]*d;
	len = sqrtf( vup[0]*vup[0] + vup[1]*vup[1] + vup[2]*vup[2] );
	if ( len < 1e-8f ) { w->numPts = 0; return; }
	vup[0]/=len; vup[1]/=len; vup[2]/=len;
	vup[0]*=CANON_MAP_BOUNDS; vup[1]*=CANON_MAP_BOUNDS; vup[2]*=CANON_MAP_BOUNDS;

	vright[0] = normal[1]*vup[2] - normal[2]*vup[1];
	vright[1] = normal[2]*vup[0] - normal[0]*vup[2];
	vright[2] = normal[0]*vup[1] - normal[1]*vup[0];

	org[0] = normal[0]*dist; org[1] = normal[1]*dist; org[2] = normal[2]*dist;

	w->numPts = 4;
	w->pts[0][0]=org[0]+vup[0]-vright[0]; w->pts[0][1]=org[1]+vup[1]-vright[1]; w->pts[0][2]=org[2]+vup[2]-vright[2];
	w->pts[1][0]=org[0]-vup[0]-vright[0]; w->pts[1][1]=org[1]-vup[1]-vright[1]; w->pts[1][2]=org[2]-vup[2]-vright[2];
	w->pts[2][0]=org[0]-vup[0]+vright[0]; w->pts[2][1]=org[1]-vup[1]+vright[1]; w->pts[2][2]=org[2]-vup[2]+vright[2];
	w->pts[3][0]=org[0]+vup[0]+vright[0]; w->pts[3][1]=org[1]+vup[1]+vright[1]; w->pts[3][2]=org[2]+vup[2]+vright[2];
}

// Sutherland-Hodgman clip against the BACK half-space of (clipNormal,clipDist).
static qboolean Canon_ChopWindingInPlace( canonWinding_t *w, const float clipNormal[3], float clipDist ) {
	float dists[CANON_MAX_WINDING_PTS + 1];
	int   sides[CANON_MAX_WINDING_PTS + 1];
	int   counts[3] = {0,0,0};
	int   i, j, numOut = 0;
	float out[CANON_MAX_WINDING_PTS][3];

	if ( w->numPts < 1 ) return qfalse;
	for ( i = 0; i < w->numPts; i++ ) {
		float d = w->pts[i][0]*clipNormal[0] + w->pts[i][1]*clipNormal[1] + w->pts[i][2]*clipNormal[2] - clipDist;
		dists[i] = d;
		if      ( d >  CANON_WINDING_EPSILON ) sides[i] = 0;
		else if ( d < -CANON_WINDING_EPSILON ) sides[i] = 1;
		else                                   sides[i] = 2;
		counts[sides[i]]++;
	}
	dists[i] = dists[0]; sides[i] = sides[0];
	if ( !counts[1] && !counts[2] ) return qfalse;
	if ( !counts[0] )               return qtrue;

	for ( i = 0; i < w->numPts; i++ ) {
		const float *p1 = w->pts[i];
		const float *p2 = w->pts[(i+1) % w->numPts];
		if ( sides[i] != 0 ) {
			if ( numOut >= CANON_MAX_WINDING_PTS ) return qfalse;
			out[numOut][0]=p1[0]; out[numOut][1]=p1[1]; out[numOut][2]=p1[2]; numOut++;
		}
		if ( sides[i] == sides[i+1] ) continue;
		if ( sides[i] == 2 || sides[i+1] == 2 ) continue;
		{
			float frac = dists[i] / (dists[i] - dists[i+1]);
			if ( numOut >= CANON_MAX_WINDING_PTS ) return qfalse;
			for ( j = 0; j < 3; j++ ) out[numOut][j] = p1[j] + frac * (p2[j] - p1[j]);
			numOut++;
		}
	}
	if ( numOut < 3 ) return qfalse;
	w->numPts = numOut;
	for ( i = 0; i < numOut; i++ ) { w->pts[i][0]=out[i][0]; w->pts[i][1]=out[i][1]; w->pts[i][2]=out[i][2]; }
	return qtrue;
}

// Winding for side sideIdx of a convex brush whose outward plane set is planes[].
static qboolean Canon_CreateBrushWinding( const cplane_t *planes, int numSides, int sideIdx, canonWinding_t *wOut ) {
	int i;
	Canon_BaseWindingForPlane( planes[sideIdx].normal, planes[sideIdx].dist, wOut );
	if ( wOut->numPts < 3 ) return qfalse;
	for ( i = 0; i < numSides; i++ ) {
		if ( i == sideIdx ) continue;
		if ( !Canon_ChopWindingInPlace( wOut, planes[i].normal, planes[i].dist ) ) { wOut->numPts = 0; return qfalse; }
		if ( wOut->numPts < 3 ) { wOut->numPts = 0; return qfalse; }
	}
	return qtrue;
}

// 3-plane intersection via Cramer's rule (mirrors Q1_Intersect3Planes in
// map_q1_bsp.c). Returns 1 on success, 0 if near-parallel.
static int Canon_Intersect3Planes( const cplane_t *p1, const cplane_t *p2,
                                    const cplane_t *p3, vec3_t v ) {
	float n00=p1->normal[0], n01=p1->normal[1], n02=p1->normal[2];
	float n10=p2->normal[0], n11=p2->normal[1], n12=p2->normal[2];
	float n20=p3->normal[0], n21=p3->normal[1], n22=p3->normal[2];
	float det = n00*(n11*n22 - n12*n21) - n01*(n10*n22 - n12*n20) + n02*(n10*n21 - n11*n20);
	if ( fabsf(det) < 1e-5f ) return 0;
	float inv = 1.0f/det;
	float b0=p1->dist, b1=p2->dist, b2=p3->dist;
	v[0] = ( b0 *(n11*n22 - n12*n21) - n01*(b1*n22 - n12*b2) + n02*(b1*n21 - n11*b2) ) * inv;
	v[1] = ( n00*(b1*n22  - n12*b2 ) - b0 *(n10*n22 - n12*n20) + n02*(n10*b2 - b1*n20) ) * inv;
	v[2] = ( n00*(n11*b2  - b1*n21 ) - n01*(n10*b2  - b1*n20 ) + b0 *(n10*n21 - n11*n20) ) * inv;
	return 1;
}

// Compute a convex brush AABB from the interior vertices formed by all valid
// 3-plane intersections that lie inside every half-space (mirrors
// Q1_ComputeBrushAABB). Returns 1 if the brush is genuinely bounded (>=1 valid
// vertex), 0 if degenerate/open (no interior vertex) — the caller SKIPS such
// brushes, exactly as the shipping loader's tree walk does. This is what
// filters out the map-spanning "outer void / thin-slab" leaves whose split
// planes do not close a bounded convex volume.
#define CANON_AABB_INSIDE_EPSILON  1.0f
static int Canon_ComputeBrushAABB( const cplane_t *planes, int numSides,
                                   vec3_t outMins, vec3_t outMaxs ) {
	vec3_t verts[512];
	int    numVerts = 0;
	int    ii, jj, kk, mm, dim;

	for ( ii = 0; ii < numSides - 2; ii++ )
	for ( jj = ii+1; jj < numSides - 1; jj++ )
	for ( kk = jj+1; kk < numSides;     kk++ ) {
		vec3_t v;
		if ( !Canon_Intersect3Planes( &planes[ii], &planes[jj], &planes[kk], v ) ) continue;
		qboolean inside = qtrue;
		for ( mm = 0; mm < numSides; mm++ ) {
			if ( mm == ii || mm == jj || mm == kk ) continue;
			if ( DotProduct(v, planes[mm].normal) - planes[mm].dist > CANON_AABB_INSIDE_EPSILON ) { inside = qfalse; break; }
		}
		if ( inside && numVerts < 512 ) { VectorCopy( v, verts[numVerts] ); numVerts++; }
	}

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

// ---------------------------------------------------------------------------
// Hull-0 SOLID/liquid leaf walk. Descends cm.nodes accumulating the plane-path;
// at each non-empty leaf emits one convex brush. Front child (d>=0 side) means
// the leaf interior is on the plane's + side, so the outward brush plane is the
// NEGATED node plane; back child keeps the plane as-is. Mirrors the hull-1
// BSP_Q1_WalkClipTree orientation contract (map_q1_bsp.c) applied to hull-0.
// ---------------------------------------------------------------------------
typedef struct {
	int   planeIdx[CANON_MAX_CLIP_DEPTH]; // index into cm.planes
	int   side[CANON_MAX_CLIP_DEPTH];     // 0 = keep node normal, 1 = negate
	int   depth;
	canonBuild_t *b;
	int   overflow;
	vec3_t worldMins, worldMaxs;          // clip bound (world AABB + margin)
} canonWalk_t;

// Map a Q3 contents flag to a canonical shaderNum-independent contents value we
// store on the brush. The leaf contents were converted from the Q1 leaf enum at
// load by the Q1 BSP loader (BSP_Q1_ContentsToQ3: Q1 leaf enum ->
// CONTENTS_SOLID/WATER/SLIME/LAVA, SKY folded to SOLID) and stored in
// cm.q1.leafContents, so the mapping is already total and centralized; we consume
// the result directly.
static int Canon_LeafContents( int leafnum ) {
	if ( leafnum >= 0 && leafnum < cm.q1.numLeafs && cm.q1.leafContents )
		return cm.q1.leafContents[leafnum];
	return 0;
}

// Emit one convex brush from a set of outward-facing planes: close open axes
// against the world box, compute the interior-vertex AABB (skip if degenerate),
// add box + edge bevels, and append the finished brush. Leaf registration is a
// later AABB-based pass. Shared by the hull-0 solid walk and the hull-1 clip
// walk (which pre-deflates its planes). Returns the new brush index, or -1 if
// the region was degenerate/open and skipped.
static int Canon_EmitBrushFromPlanes( canonBuild_t *b, cplane_t *local, int nLocal,
                                      int contents, const vec3_t worldMins, const vec3_t worldMaxs ) {
	int i;
	if ( nLocal < 1 ) return -1;

	// --- clip open brushes to the world box ---------------------------------
	// A leaf's split-plane path only PARTIALLY bounds it: rock, walls that reach
	// the map edge, and the outer void extend to infinity on unconstrained axes.
	// Close every brush against the 6 world-AABB planes (inflated by a margin so
	// nothing at the true boundary is clipped away). Any axis the path already
	// bounds tighter keeps its own plane; the world planes only cap open axes.
	for ( i = 0; i < 3 && nLocal < CANON_MAX_BRUSH_SIDES - 1; i++ ) {
		float np[3];
		np[0]=np[1]=np[2]=0; np[i]= 1.0f;
		{ cplane_t pl; memset(&pl,0,sizeof(pl)); VectorCopy(np,pl.normal); pl.dist = worldMaxs[i]; local[nLocal++]=pl; }
		np[0]=np[1]=np[2]=0; np[i]=-1.0f;
		{ cplane_t pl; memset(&pl,0,sizeof(pl)); VectorCopy(np,pl.normal); pl.dist = -worldMins[i]; local[nLocal++]=pl; }
	}

	// --- bounded-brush AABB from interior 3-plane vertices ------------------
	// A hull-0 solid leaf whose split-plane path does NOT close a bounded convex
	// volume (the outer void, thin slabs, walls that reach the map edge) yields
	// zero interior vertices — SKIP it (return -1; the caller counts it). For clip
	// regions a zero-vertex result after deflation means the region was thinner
	// than the hull-1 expansion, i.e. a pure expansion artifact with no real solid.
	vec3_t wmins, wmaxs;
	if ( !Canon_ComputeBrushAABB( local, nLocal, wmins, wmaxs ) ) {
		return -1;
	}

	// Windings (for edge-bevel outer-hull tests) computed from the path planes.
	canonWinding_t windings[CANON_MAX_BRUSH_SIDES];
	for ( i = 0; i < nLocal; i++ )
		Canon_CreateBrushWinding( local, nLocal, i, &windings[i] );

	// --- open the brush; write path sides -----------------------------------
	Canon_GrowBrushes( b, 1 );
	int brushIdx = b->numBrushes;
	cbrush_t *br = &b->brushes[brushIdx];
	memset( br, 0, sizeof( *br ) );
	br->contents = contents;
	br->shaderNum = 0;

	int firstSide = b->numSides;
	int numSides  = 0;

	// Emit path planes as sides (order 0..nLocal-1 for now; box bevels below
	// SWAP the 6 axial planes into slots 0..5, satisfying the trace's
	// "first six sides are axial" contract for the capsule/position paths).
	// The plane pool (b->planes) keeps growing/reallocating while brushes are
	// built, so a raw cplane_t* stored now would dangle. Stash the plane INDEX
	// in side->plane and rebase every side to a real cplane_t* in one finalize
	// pass, once the plane array has stopped moving (see CMQ1_BuildCanonicalModel).
	for ( i = 0; i < nLocal; i++ ) {
		int pi = Canon_FindOrAddPlane( b, local[i].normal, local[i].dist );
		Canon_GrowSides( b, 1 );
		cbrushside_t *s = &b->sides[b->numSides];
		memset( s, 0, sizeof( *s ) );
		s->plane = (cplane_t *)(intptr_t)pi; // plane index, rebased to a pointer at finalize
		b->numSides++;
		numSides++;
	}

	// --- Box bevels: ensure all 6 axial planes present, swap into slots 0..5 -
	// Rebuild a local plane array reflecting current sides (path planes only so
	// far) for the winding/outer-hull tests during edge beveling.
	// We work in terms of plane indices held in side->plane (as int).
	{
		int order = 0, axis, dir;
		for ( axis = 0; axis < 3; axis++ ) {
			for ( dir = -1; dir <= 1; dir += 2 ) {
				float tgt = (dir == 1) ? 1.0f : -1.0f;
				int found = -1, si;
				for ( si = 0; si < numSides; si++ ) {
					int pi = (int)(intptr_t)b->sides[firstSide + si].plane;
					const cplane_t *p = &b->planes[pi];
					if ( p->normal[axis] == tgt &&
					     p->normal[(axis+1)%3] == 0.0f &&
					     p->normal[(axis+2)%3] == 0.0f ) { found = si; break; }
				}
				if ( found < 0 ) {
					float bn[3] = {0,0,0};
					bn[axis] = tgt;
					float bd = (dir == 1) ? wmaxs[axis] : -wmins[axis];
					int pi = Canon_FindOrAddPlane( b, bn, bd );
					Canon_GrowSides( b, 1 );
					cbrushside_t *s = &b->sides[b->numSides];
					memset( s, 0, sizeof( *s ) );
					s->plane = (cplane_t *)(intptr_t)pi;
					b->numSides++;
					numSides++;
					cmCanon.statBoxBevels++;
					found = numSides - 1;
				}
				if ( found != order ) {
					cbrushside_t tmp = b->sides[firstSide + order];
					b->sides[firstSide + order] = b->sides[firstSide + found];
					b->sides[firstSide + found] = tmp;
				}
				order++;
			}
		}
	}

	// --- Edge bevels (only for brushes with non-axial sides) ----------------
	if ( numSides > 6 ) {
		// Rebuild local plane set + windings including the box bevels.
		cplane_t lp[CANON_MAX_BRUSH_SIDES];
		int nlp = numSides < CANON_MAX_BRUSH_SIDES ? numSides : CANON_MAX_BRUSH_SIDES;
		for ( i = 0; i < nlp; i++ ) {
			int pi = (int)(intptr_t)b->sides[firstSide + i].plane;
			lp[i] = b->planes[pi];
		}
		for ( i = 0; i < nlp; i++ )
			Canon_CreateBrushWinding( lp, nlp, i, &windings[i] );

		int si;
		for ( si = 6; si < nlp; si++ ) {
			canonWinding_t *cw = &windings[si];
			int j;
			if ( cw->numPts < 2 ) continue;
			for ( j = 0; j < cw->numPts; j++ ) {
				int jnext = (j + 1) % cw->numPts;
				float vec[3], vecLen;
				int eaxis, edir;
				vec[0] = cw->pts[j][0] - cw->pts[jnext][0];
				vec[1] = cw->pts[j][1] - cw->pts[jnext][1];
				vec[2] = cw->pts[j][2] - cw->pts[jnext][2];
				vecLen = sqrtf( vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2] );
				if ( vecLen < CANON_EDGE_DEGENERATE ) continue;
				vec[0]/=vecLen; vec[1]/=vecLen; vec[2]/=vecLen;
				Canon_SnapNormal( vec );
				if ( (vec[0]==-1.0f||vec[0]==1.0f) || (vec[1]==-1.0f||vec[1]==1.0f) ||
				     (vec[2]==-1.0f||vec[2]==1.0f) || (vec[0]==0.0f&&vec[1]==0.0f) ||
				     (vec[1]==0.0f&&vec[2]==0.0f) || (vec[2]==0.0f&&vec[0]==0.0f) ) continue;

				for ( eaxis = 0; eaxis < 3; eaxis++ ) {
					for ( edir = -1; edir <= 1; edir += 2 ) {
						float vec2[3] = {0,0,0};
						float cn[3], cd, nlen;
						int allBehind, k, pi;
						vec2[eaxis] = (float)edir;
						cn[0] = vec[1]*vec2[2] - vec[2]*vec2[1];
						cn[1] = vec[2]*vec2[0] - vec[0]*vec2[2];
						cn[2] = vec[0]*vec2[1] - vec[1]*vec2[0];
						nlen = sqrtf( cn[0]*cn[0] + cn[1]*cn[1] + cn[2]*cn[2] );
						if ( nlen < CANON_EDGE_DEGENERATE ) continue;
						cn[0]/=nlen; cn[1]/=nlen; cn[2]/=nlen;
						Canon_SnapNormal( cn );
						cd = cw->pts[j][0]*cn[0] + cw->pts[j][1]*cn[1] + cw->pts[j][2]*cn[2];
						// dedup vs existing sides of this brush
						{
							qboolean dup = qfalse;
							for ( k = 0; k < nlp; k++ ) {
								const cplane_t *ep = &lp[k];
								if ( fabsf(ep->normal[0]-cn[0]) < CANON_NORMAL_EPSILON &&
								     fabsf(ep->normal[1]-cn[1]) < CANON_NORMAL_EPSILON &&
								     fabsf(ep->normal[2]-cn[2]) < CANON_NORMAL_EPSILON &&
								     fabsf(ep->dist     -cd    ) < CANON_DIST_EPSILON ) { dup = qtrue; break; }
							}
							if ( dup ) continue;
						}
						// outer-hull test: every vertex of every side behind/on the candidate
						allBehind = 1;
						for ( k = 0; k < nlp && allBehind; k++ ) {
							canonWinding_t *wk = &windings[k];
							int vi, inFront = 0;
							if ( wk->numPts == 0 ) continue;
							for ( vi = 0; vi < wk->numPts; vi++ ) {
								float d = wk->pts[vi][0]*cn[0] + wk->pts[vi][1]*cn[1] + wk->pts[vi][2]*cn[2] - cd;
								if ( d > CANON_WINDING_EPSILON ) { inFront = 1; break; }
							}
							if ( inFront ) { allBehind = 0; break; }
						}
						if ( !allBehind ) continue;
						if ( nlp >= CANON_MAX_BRUSH_SIDES ) goto done_bevels;
						pi = Canon_FindOrAddPlane( b, cn, cd );
						Canon_GrowSides( b, 1 );
						cbrushside_t *s = &b->sides[b->numSides];
						memset( s, 0, sizeof( *s ) );
						s->plane = (cplane_t *)(intptr_t)pi;
						b->numSides++;
						numSides++;
						cmCanon.statEdgeBevels++;
						// append plane + winding so later edges test against it
						lp[nlp] = b->planes[pi];
						Canon_CreateBrushWinding( lp, nlp + 1, nlp, &windings[nlp] );
						nlp++;
					}
				}
			}
		}
	}
done_bevels:

	br->numsides  = numSides;
	// Stash the side-array offset in checkcount temporarily; finalize rebases it
	// into a real sides pointer and clears checkcount back to 0. (cbrush_t has no
	// firstSide field — the live loader points cbrush_t.sides straight at the
	// side array; we mirror that after the side array stops reallocating.)
	br->checkcount = firstSide;

	// --- bounds from winding AABB (already have wmins/wmaxs) -----------------
	// Box bevels made the brush bounded on all axes; use the winding AABB which
	// exactly matches the convex volume. The generic trace reads b->bounds for
	// the leaf cull and the axial early-out.
	VectorCopy( wmins, br->bounds[0] );
	VectorCopy( wmaxs, br->bounds[1] );

	// Commit the brush (brushIdx == the pre-increment count).
	b->numBrushes++;
	return brushIdx;
}

// Hull-0 solid/liquid leaf brush emission: build the outward-plane set from the
// accumulated node path and emit. Leaf registration is a later AABB-based pass.
static void Canon_EmitLeafBrush( canonWalk_t *w, int contents ) {
	canonBuild_t *b = w->b;
	int i;

	if ( w->depth < 1 ) return; // no bounding planes -> whole-world empty path

	cplane_t local[CANON_MAX_BRUSH_SIDES];
	int      nLocal = 0;
	for ( i = 0; i < w->depth && nLocal < CANON_MAX_BRUSH_SIDES; i++ ) {
		const cplane_t *np = &cm.planes[ w->planeIdx[i] ];
		cplane_t pl;
		memset( &pl, 0, sizeof( pl ) );
		if ( w->side[i] ) {
			pl.normal[0] = -np->normal[0]; pl.normal[1] = -np->normal[1]; pl.normal[2] = -np->normal[2];
			pl.dist      = -np->dist;
		} else {
			VectorCopy( np->normal, pl.normal );
			pl.dist = np->dist;
		}
		Canon_SnapNormal( pl.normal );
		{
			int k; qboolean dup = qfalse;
			for ( k = 0; k < nLocal; k++ ) {
				if ( fabsf(local[k].normal[0]-pl.normal[0]) < CANON_NORMAL_EPSILON &&
				     fabsf(local[k].normal[1]-pl.normal[1]) < CANON_NORMAL_EPSILON &&
				     fabsf(local[k].normal[2]-pl.normal[2]) < CANON_NORMAL_EPSILON &&
				     fabsf(local[k].dist     -pl.dist     ) < CANON_DIST_EPSILON ) { dup = qtrue; break; }
			}
			if ( dup ) continue;
		}
		local[nLocal++] = pl;
	}

	int brushIdx = Canon_EmitBrushFromPlanes( b, local, nLocal, contents, w->worldMins, w->worldMaxs );
	if ( brushIdx < 0 ) { cmCanon.statDegenerate++; return; }

	switch ( contents & (CONTENTS_SOLID|CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA) ) {
		case CONTENTS_WATER: cmCanon.statWaterBrushes++; break;
		case CONTENTS_SLIME: cmCanon.statSlimeBrushes++; break;
		case CONTENTS_LAVA:  cmCanon.statLavaBrushes++;  break;
		default:             cmCanon.statSolidBrushes++; break;
	}
	cmCanon.statLeavesRegistered++;
}

static void Canon_WalkNode( canonWalk_t *w, int num ) {
	if ( num < 0 ) {
		int leafnum  = -1 - num;
		int contents = Canon_LeafContents( leafnum );
		// Emit a brush for any non-empty leaf; the contents bit set decides the
		// brush contents (SOLID vs the liquid volumes) so MASK_WATER etc. work.
		if ( contents != 0 )
			Canon_EmitLeafBrush( w, contents );
		return;
	}
	if ( num >= cm.numNodes ) return;
	if ( w->depth >= CANON_MAX_CLIP_DEPTH ) { w->overflow++; return; }

	const cNode_t *node = &cm.nodes[num];
	int planeIdx = (int)( node->plane - cm.planes );

	// front child (children[0]): interior on + side -> outward plane = negated
	w->planeIdx[w->depth] = planeIdx;
	w->side[w->depth]     = 1;
	w->depth++;
	Canon_WalkNode( w, node->children[0] );
	w->depth--;

	// back child (children[1]): interior on - side -> outward plane = as-is
	w->planeIdx[w->depth] = planeIdx;
	w->side[w->depth]     = 0;
	w->depth++;
	Canon_WalkNode( w, node->children[1] );
	w->depth--;
}

// ---------------------------------------------------------------------------
// AABB-based leaf-brush insertion. The generic trace descends the node tree
// with box-expansion (offset) and near-boundary both-side descent, so a brush
// registered only in its producing leaf would be MISSED for a box straddling a
// leaf boundary (tunnel). Mirror the shipping loader's Q1_InsertBrushIntoTree
// (map_q1_bsp.c): insert each brush AABB into EVERY canonical leaf it overlaps
// (leaf-brush of 1 per producing leaf becomes shared registration wherever the
// spatial structure requires it), with a step-height epsilon band so any leaf
// within one stair-riser of a splitting plane also receives the brush.
// ---------------------------------------------------------------------------
typedef struct {
	int  **data;    // per-leaf arrays of canonical brush indices
	int   *count;
	int   *cap;
	int    numLeafs;
} canonLeafBuckets_t;

static void CanonBucket_Append( canonLeafBuckets_t *lb, int leafIdx, int brushIdx ) {
	if ( leafIdx < 0 || leafIdx >= lb->numLeafs ) return;
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

#define CANON_BRUSH_INSERT_EPS 16.0f

static void Canon_InsertBrushIntoTree( canonLeafBuckets_t *lb, int brushIdx,
                                       const vec3_t mins, const vec3_t maxs, int nodeIdx ) {
	if ( nodeIdx < 0 ) {
		int leafIdx = -1 - nodeIdx;
		CanonBucket_Append( lb, leafIdx, brushIdx );
		return;
	}
	if ( nodeIdx >= cm.numNodes ) return;

	const cNode_t  *node  = &cm.nodes[nodeIdx];
	const cplane_t *plane = node->plane;
	vec3_t cFront, cBack;
	float  dFront, dBack;
	int    k;

	for ( k = 0; k < 3; k++ ) {
		if ( plane->normal[k] >= 0.0f ) { cFront[k] = maxs[k]; cBack[k] = mins[k]; }
		else                            { cFront[k] = mins[k]; cBack[k] = maxs[k]; }
	}
	dFront = DotProduct( cFront, plane->normal ) - plane->dist;
	dBack  = DotProduct( cBack,  plane->normal ) - plane->dist;

	if ( dFront < -CANON_BRUSH_INSERT_EPS ) {
		Canon_InsertBrushIntoTree( lb, brushIdx, mins, maxs, node->children[1] );
	} else if ( dBack > CANON_BRUSH_INSERT_EPS ) {
		Canon_InsertBrushIntoTree( lb, brushIdx, mins, maxs, node->children[0] );
	} else {
		Canon_InsertBrushIntoTree( lb, brushIdx, mins, maxs, node->children[0] );
		Canon_InsertBrushIntoTree( lb, brushIdx, mins, maxs, node->children[1] );
	}
}

// ===========================================================================
//
//  HULL-1 CLIP EXTRACTION
//
//  Q1 mappers' clip brushes exist only in the hull-1/hull-2 clipnode trees, not
//  in hull-0 — so the hull-0-derived canonical model misses them, and after the
//  live switch players/bots would walk into mapper-fenced regions. This walks the
//  hull-1 clipnode tree the same way as the hull-0 walk, deflates each SOLID
//  region by the exact inverse of Q1 qbsp's hull-1 box expansion to recover the
//  hull-0-space surface, drops regions that are pure expansion artifacts or that
//  merely re-cover the hull-0 solid, and emits the survivors as
//  CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP brushes (blocked by player/monster
//  masks, invisible to hull-0/point and shot masks). Runtime plane re-expansion
//  by the actual box then reproduces the mapper's intended clip boundary.
//
// ===========================================================================

// Q1 qbsp hull-1 box. Expansion pushed each surface plane outward by the box's
// support in the plane normal; deflation is the exact inverse: for plane (n,d),
// d_hull0 = d_hull1 + sum_k n[k]*(n[k]>0 ? mins[k] : maxs[k]). Axial planes
// deflate exactly; non-axial planes deflate by the support offset (bounded corner
// error, measured by the differential gate).
static const float CANON_HULL1_MINS[3] = { -16.0f, -16.0f, -24.0f };
static const float CANON_HULL1_MAXS[3] = {  16.0f,  16.0f,  32.0f };

static float Canon_Hull1DeflateOffset( const float n[3] ) {
	float off = 0.0f;
	int k;
	for ( k = 0; k < 3; k++ )
		off += n[k] * ( n[k] > 0.0f ? CANON_HULL1_MINS[k] : CANON_HULL1_MAXS[k] );
	return off;
}

// Point-in-hull-0-solid test: walk the live hull-0 node tree (cm.nodes +
// cm.q1.leafContents) — the authoritative hull-0 solid definition. Returns true
// if the point lands in a CONTENTS_SOLID hull-0 leaf.
static qboolean Canon_PointInHull0Solid( const vec3_t p ) {
	int num = 0;
	while ( num >= 0 ) {
		if ( num >= cm.numNodes ) return qfalse;
		const cNode_t *node = &cm.nodes[num];
		const cplane_t *plane = node->plane;
		float d = ( plane->type < 3 ) ? ( p[plane->type] - plane->dist )
		                              : ( DotProduct( plane->normal, p ) - plane->dist );
		num = node->children[ d >= 0.0f ? 0 : 1 ];
	}
	int leafnum = -1 - num;
	if ( leafnum >= 0 && leafnum < cm.q1.numLeafs )
		return ( cm.q1.leafContents[leafnum] & CONTENTS_SOLID ) != 0;
	return qfalse;
}

// Hull-1 clip walk state: same plane-path accumulation as the hull-0 walk, but
// over cm.q1.clipnodes.
typedef struct {
	int   planeIdx[CANON_MAX_CLIP_DEPTH]; // index into cm.planes (clipnode planenum)
	int   side[CANON_MAX_CLIP_DEPTH];     // 0 = keep node normal, 1 = negate
	int   depth;
	canonBuild_t *b;
	int   overflow;
	vec3_t worldMins, worldMaxs;
} canonClipWalk_t;

// Emit an explicit-plane clip brush: append the deflated planes as sides, add
// any missing axial box bevels from the deflated AABB (swapped into slots 0..5),
// and set bounds explicitly. Unlike Canon_EmitBrushFromPlanes this does NOT run
// edge-bevel winding generation — a clip cell's deflated planes can be thinner
// than the box (the deflation intentionally under-shrinks; runtime re-expansion
// restores the hull-1 boundary), so winding vertices are unreliable. Q1 mapper
// clips are axis-aligned in practice, so box bevels are sufficient; the 6 axial
// bevels also satisfy the trace's "first six sides are axial" contract.
static int Canon_EmitClipBrushExplicit( canonBuild_t *b, const cplane_t *deflPlanes, int nPlanes,
                                        const vec3_t deflMins, const vec3_t deflMaxs,
                                        const vec3_t boundsMins, const vec3_t boundsMaxs ) {
	int i;
	Canon_GrowBrushes( b, 1 );
	int brushIdx = b->numBrushes;
	cbrush_t *br = &b->brushes[brushIdx];
	memset( br, 0, sizeof( *br ) );
	br->contents = CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP;

	int firstSide = b->numSides;
	int numSides  = 0;
	for ( i = 0; i < nPlanes; i++ ) {
		int pi = Canon_FindOrAddPlane( b, deflPlanes[i].normal, deflPlanes[i].dist );
		Canon_GrowSides( b, 1 );
		cbrushside_t *s = &b->sides[b->numSides];
		memset( s, 0, sizeof( *s ) );
		s->plane = (cplane_t *)(intptr_t)pi;
		b->numSides++;
		numSides++;
	}

	// Box bevels: ensure all 6 axial planes present at the DEFLATED AABB (so
	// runtime re-expansion by the box restores the hull-1 axial boundary), swap
	// into canonical order 0..5.
	{
		int order = 0, axis, dir;
		for ( axis = 0; axis < 3; axis++ ) {
			for ( dir = -1; dir <= 1; dir += 2 ) {
				float tgt = (dir == 1) ? 1.0f : -1.0f;
				int found = -1, si;
				for ( si = 0; si < numSides; si++ ) {
					int pi = (int)(intptr_t)b->sides[firstSide + si].plane;
					const cplane_t *p = &b->planes[pi];
					if ( p->normal[axis] == tgt && p->normal[(axis+1)%3] == 0.0f && p->normal[(axis+2)%3] == 0.0f ) { found = si; break; }
				}
				if ( found < 0 ) {
					float bn[3] = {0,0,0};
					bn[axis] = tgt;
					float bd = (dir == 1) ? deflMaxs[axis] : -deflMins[axis];
					int pi = Canon_FindOrAddPlane( b, bn, bd );
					Canon_GrowSides( b, 1 );
					cbrushside_t *s = &b->sides[b->numSides];
					memset( s, 0, sizeof( *s ) );
					s->plane = (cplane_t *)(intptr_t)pi;
					b->numSides++;
					numSides++;
					cmCanon.statBoxBevels++;
					found = numSides - 1;
				}
				if ( found != order ) {
					cbrushside_t tmp = b->sides[firstSide + order];
					b->sides[firstSide + order] = b->sides[firstSide + found];
					b->sides[firstSide + found] = tmp;
				}
				order++;
			}
		}
	}

	br->numsides   = numSides;
	br->checkcount = firstSide; // rebased at finalize
	// Bounds are the CONSERVATIVE hull-1 AABB: they only ever over-include, so the
	// leaf-cull never drops a brush the box could touch (the per-plane trace does
	// the exact test). A deflated AABB thinner than the box would under-cull.
	VectorCopy( boundsMins, br->bounds[0] );
	VectorCopy( boundsMaxs, br->bounds[1] );
	b->numBrushes++;
	return brushIdx;
}

// Emit one deflated clip region as a canonical CONTENTS_PLAYERCLIP|MONSTERCLIP
// brush, unless it is a pure expansion artifact or a redundant re-cover of the
// hull-0 solid set. Geometry (AABB, box bevels) is derived from the VALID
// undeflated hull-1 cell; only the emitted plane distances are deflated, so a
// clip cell thinner than the box survives (runtime re-expansion restores it).
static void Canon_EmitClipRegion( canonClipWalk_t *w ) {
	canonBuild_t *b = w->b;
	int i;
	if ( w->depth < 1 ) return;

	cmCanon.statClipRegions++;

	// --- undeflated hull-1 outward-plane set (always geometrically valid) ----
	cplane_t hp[CANON_MAX_BRUSH_SIDES];
	int      nHP = 0;
	for ( i = 0; i < w->depth && nHP < CANON_MAX_BRUSH_SIDES - 6; i++ ) {
		const cplane_t *np = &cm.planes[ w->planeIdx[i] ];
		cplane_t pl;
		memset( &pl, 0, sizeof( pl ) );
		if ( w->side[i] ) {
			pl.normal[0] = -np->normal[0]; pl.normal[1] = -np->normal[1]; pl.normal[2] = -np->normal[2];
			pl.dist      = -np->dist;
		} else {
			VectorCopy( np->normal, pl.normal );
			pl.dist = np->dist;
		}
		Canon_SnapNormal( pl.normal );
		int k; qboolean dup = qfalse;
		for ( k = 0; k < nHP; k++ ) {
			if ( fabsf(hp[k].normal[0]-pl.normal[0]) < CANON_NORMAL_EPSILON &&
			     fabsf(hp[k].normal[1]-pl.normal[1]) < CANON_NORMAL_EPSILON &&
			     fabsf(hp[k].normal[2]-pl.normal[2]) < CANON_NORMAL_EPSILON &&
			     fabsf(hp[k].dist     -pl.dist     ) < CANON_DIST_EPSILON ) { dup = qtrue; break; }
		}
		if ( !dup ) hp[nHP++] = pl;
	}
	if ( nHP < 1 ) { cmCanon.statClipDeflateDegen++; return; }

	// World-close the hull-1 planes so an open clip cell (spanning to the map
	// edge) has a bounded AABB. The hull-1 cell is always a valid convex volume,
	// so this AABB is well-formed.
	cplane_t hpClosed[CANON_MAX_BRUSH_SIDES];
	int nHPClosed = nHP;
	memcpy( hpClosed, hp, nHP * sizeof( cplane_t ) );
	for ( i = 0; i < 3; i++ ) {
		cplane_t pl;
		memset(&pl,0,sizeof(pl)); pl.normal[i]= 1.0f; pl.dist= w->worldMaxs[i]; hpClosed[nHPClosed++]=pl;
		memset(&pl,0,sizeof(pl)); pl.normal[i]=-1.0f; pl.dist=-w->worldMins[i]; hpClosed[nHPClosed++]=pl;
	}
	vec3_t h1min, h1max;
	if ( !Canon_ComputeBrushAABB( hpClosed, nHPClosed, h1min, h1max ) ) {
		// Genuinely degenerate hull-1 cell (should be rare) — nothing to emit.
		cmCanon.statClipDeflateDegen++;
		return;
	}

	// Deflated AABB = hull-1 AABB shrunk by the box support on each axis. This is
	// exactly the inverse of the axial expansion; it may invert on a thin axis
	// (cell thinner than the box), which is fine — the deflated PLANES still
	// re-expand to the hull-1 boundary. Box bevels are placed at this deflated
	// AABB (clamped so an inverted axis collapses to its center, keeping the
	// axial bevel planes well-ordered).
	vec3_t dmin, dmax;
	for ( i = 0; i < 3; i++ ) {
		dmin[i] = h1min[i] - CANON_HULL1_MINS[i]; // + (+16 / +24)  → shrink low side inward
		dmax[i] = h1max[i] - CANON_HULL1_MAXS[i]; // - (+16 / +32)  → shrink high side inward
		if ( dmax[i] < dmin[i] ) {
			float c = 0.5f * ( h1min[i] + h1max[i] );
			dmin[i] = dmax[i] = c;
		}
	}

	// Redundant-wall-copy test: if the hull-1 cell is (approximately) contained in
	// the hull-0 solid set it is a re-cover of real geometry — drop it. Conservative
	// (KEEP when any sample escapes the hull-0 solid): clip ∪ solid is monotone-safe.
	{
		vec3_t c;
		for ( i = 0; i < 3; i++ ) c[i] = 0.5f * ( h1min[i] + h1max[i] );
		qboolean allInsideSolid = Canon_PointInHull0Solid( c );
		if ( allInsideSolid ) {
			int cx, cy, cz;
			for ( cx = 0; cx <= 1 && allInsideSolid; cx++ )
			for ( cy = 0; cy <= 1 && allInsideSolid; cy++ )
			for ( cz = 0; cz <= 1 && allInsideSolid; cz++ ) {
				vec3_t v;
				v[0] = cx ? h1max[0]-1.0f : h1min[0]+1.0f;
				v[1] = cy ? h1max[1]-1.0f : h1min[1]+1.0f;
				v[2] = cz ? h1max[2]-1.0f : h1min[2]+1.0f;
				if ( !Canon_PointInHull0Solid( v ) ) allInsideSolid = qfalse;
			}
		}
		if ( allInsideSolid ) {
			int s;
			for ( s = 0; s < 5 && allInsideSolid; s++ ) {
				vec3_t v;
				v[0] = h1min[0] + ( h1max[0]-h1min[0] ) * ( 0.2f + 0.15f*s );
				v[1] = h1min[1] + ( h1max[1]-h1min[1] ) * ( 0.3f + 0.1f*s );
				v[2] = h1min[2] + ( h1max[2]-h1min[2] ) * ( 0.25f + 0.12f*s );
				if ( !Canon_PointInHull0Solid( v ) ) allInsideSolid = qfalse;
			}
		}
		if ( allInsideSolid ) { cmCanon.statClipRedundant++; return; }
	}

	// Deflated plane set: same normals, distances shifted inward by the box support.
	cplane_t defl[CANON_MAX_BRUSH_SIDES];
	for ( i = 0; i < nHP; i++ ) {
		defl[i] = hp[i];
		defl[i].dist += Canon_Hull1DeflateOffset( hp[i].normal );
	}

	Canon_EmitClipBrushExplicit( b, defl, nHP, dmin, dmax, h1min, h1max );
	cmCanon.statClipBrushes++;
}

static void Canon_WalkClipNode( canonClipWalk_t *w, int num ) {
	if ( num < 0 ) {
		// Clipnode leaf: -2 = SOLID (the only code that fences a box hull).
		if ( num == -2 )
			Canon_EmitClipRegion( w );
		return;
	}
	if ( num >= cm.q1.numClipnodes ) return;
	if ( w->depth >= CANON_MAX_CLIP_DEPTH ) { w->overflow++; return; }

	const q1_dclipnode_t *node = &cm.q1.clipnodes[num];
	int planeIdx = (int)node->planenum; // already doubled at load → indexes cm.planes

	// front child (children[0], d>=0 side): interior on + side → outward = negated
	w->planeIdx[w->depth] = planeIdx;
	w->side[w->depth]     = 1;
	w->depth++;
	Canon_WalkClipNode( w, (int)(short)node->children[0] );
	w->depth--;

	// back child (children[1]): interior on - side → outward = as-is
	w->planeIdx[w->depth] = planeIdx;
	w->side[w->depth]     = 0;
	w->depth++;
	Canon_WalkClipNode( w, (int)(short)node->children[1] );
	w->depth--;
}

// ---------------------------------------------------------------------------
// CMQ1_BuildCanonicalModel — construct the canonical collision model at load.
// This is the ONLY live Q1 collision representation. A no-op on non-Q1 maps
// (Q3 ships its own native model); idempotent (frees any prior model first).
// On a Q1 map it MUST succeed — the canonical model is what live collision
// dispatches through — so a degenerate/missing precondition hard-fails the load
// loud (Com_Terminate) rather than leaving the map on an untested path. There is
// no legacy fallback: the retired clipnode runtime is gone.
// ---------------------------------------------------------------------------
void CMQ1_BuildCanonicalModel( void ) {
	CMQ1_FreeCanonicalModel();

	// Not a Q1 map — nothing to transpile (Q3's native model already ships).
	// This is the explicit format fact carried from the loader, not a guess from
	// a tracer pointer's address.
	if ( !cm.q1.isQ1Format ) return;

	// A Q1 map whose hull-0 tree or leaf-contents are missing cannot build the
	// canonical model, and there is no fallback to route it onto. Fail the load
	// loud, naming the map and the failing precondition.
	if ( !cm.numNodes )
		Com_Terminate( TERM_CLIENT_DROP,
			"%s: Q1 map '%s' has no hull-0 nodes — cannot build canonical collision model",
			__func__, cm.name );
	if ( !cm.q1.leafContents || cm.q1.numLeafs <= 0 )
		Com_Terminate( TERM_CLIENT_DROP,
			"%s: Q1 map '%s' has no leaf contents (numLeafs=%d) — cannot build canonical collision model",
			__func__, cm.name, cm.q1.numLeafs );

	int startMsec = Com_Milliseconds();

	memset( &cmCanon, 0, sizeof( cmCanon ) );

	canonBuild_t b;
	memset( &b, 0, sizeof( b ) );

	// Seed the plane pool with the live hull-0 planes so path-plane indices
	// (from cm.planes) map 1:1 and node planes rebase cleanly. Bevel planes are
	// appended past cm.numPlanes and deduped against everything.
	Canon_GrowPlanes( &b, cm.numPlanes + 1 );
	memcpy( b.planes, cm.planes, cm.numPlanes * sizeof( cplane_t ) );
	b.numPlanes = cm.numPlanes;

	// One canonical leaf per hull-0 leaf (indexed identically to cm.nodes leaf
	// resolution: leafnum = -1 - child). numLeafSurfaces forced to 0 so the
	// generic trace's patch loop is skipped (Q1 world has no patches anyway).
	//
	// cluster/area are COPIED from the loaded cm.leafs, 1:1: the canonical node
	// tree is a topology-identical copy of cm.nodes, so canonical leaf L resolves
	// to the same hull-0 leaf as loaded leaf L (both keyed on the same -1-child
	// encoding), and cm.numLeafs == cm.q1.numLeafs (both from the same post-dedup
	// bsp->numLeafs). Adopt-at-load makes these canonical leaves the live cm.world
	// leaves, so PVS/area (CM_LeafCluster/CM_LeafArea) must read real cluster/area
	// here, not the stubs the pre-adopt swap left them as.
	int numLeafs = cm.q1.numLeafs;
	cLeaf_t *leafs = (cLeaf_t *)Z_Malloc( numLeafs * sizeof( cLeaf_t ) );
	memset( leafs, 0, numLeafs * sizeof( cLeaf_t ) );
	int li;
	for ( li = 0; li < numLeafs; li++ ) {
		// 1:1 leaf correspondence (see comment above). Guard the copy so a defensive
		// out-of-range index cannot read past the loaded array.
		if ( li < cm.numLeafs ) {
			leafs[li].cluster = cm.leafs[li].cluster;
			leafs[li].area    = cm.leafs[li].area;
		} else {
			leafs[li].cluster = -1;
			leafs[li].area    = 0;
		}
		leafs[li].firstLeafBrush   = 0;
		leafs[li].numLeafBrushes   = 0;
		leafs[li].firstLeafSurface = 0;
		leafs[li].numLeafSurfaces  = 0;
	}

	// Walk the whole hull-0 tree from the world root (node 0), emitting one
	// brush per non-empty leaf and registering it in that leaf.
	canonWalk_t w;
	memset( &w, 0, sizeof( w ) );
	w.b = &b;
	// World clip box = world submodel AABB + a generous margin. Open solid leaves
	// (rock, edge walls, the enclosing void) are closed against this so the model
	// covers the full solid volume the legacy hull-0 leaf-contents walk reports as
	// solid — which extends past the tight content AABB into the surrounding
	// basement/void. A convex solid leaf is solid throughout its open extent, so a
	// large margin only ever extends a brush WITHIN its own leaf (never into empty
	// space), matching legacy. Sized to cover deep basements (e.g. e1m2's solid
	// column reaches ~280u below the content Z-min); 4096 leaves ample headroom
	// while staying well inside the ±32768 Q1 map coordinate bound.
	{
		static const float CANON_WORLD_MARGIN = 4096.0f;
		int ax;
		for ( ax = 0; ax < 3; ax++ ) {
			w.worldMins[ax] = cm.cmodels[0].mins[ax] - CANON_WORLD_MARGIN;
			w.worldMaxs[ax] = cm.cmodels[0].maxs[ax] + CANON_WORLD_MARGIN;
		}
	}
	Canon_WalkNode( &w, 0 );
	if ( w.overflow )
		Com_Log( SEV_WARN, LOG_CH(ch_cm_canon),
			"CMQ1_BuildCanonicalModel: clip-depth overflow on %d leaves (dropped)\n", w.overflow );

	// --- hull-1 clip extraction --------------------------------------------
	// Runs AFTER the hull-0 solid brushes exist so the containment test can query
	// the hull-0 solid set. Walks the hull-1 clipnode world root, deflates each
	// SOLID region, and appends the survivors as PLAYERCLIP|MONSTERCLIP brushes.
	if ( cm.q1.clipnodes && cm.q1.numClipnodes > 0 && cm.q1.numSubmodelRoots > 0 ) {
		canonClipWalk_t cw;
		memset( &cw, 0, sizeof( cw ) );
		cw.b = &b;
		VectorCopy( w.worldMins, cw.worldMins );
		VectorCopy( w.worldMaxs, cw.worldMaxs );
		int clipRoot = cm.q1.submodelRoots[0]; // world hull-1 root
		if ( clipRoot >= 0 && clipRoot < cm.q1.numClipnodes )
			Canon_WalkClipNode( &cw, clipRoot );
		if ( cw.overflow )
			Com_Log( SEV_WARN, LOG_CH(ch_cm_canon),
				"CMQ1_BuildCanonicalModel: clip-walk depth overflow on %d regions (dropped)\n", cw.overflow );
	}

	// --- finalize: rebase side plane indices to real cplane_t* -------------
	// The build stored plane INDICES in side->plane; convert to pointers into
	// the final plane array, and set each brush's sides pointer + resolve the
	// firstSide offset stashed in checkcount.
	for ( int bi = 0; bi < b.numBrushes; bi++ ) {
		cbrush_t *br = &b.brushes[bi];
		int firstSide = br->checkcount;       // stashed offset into b.sides
		br->sides     = &b.sides[firstSide];
		br->checkcount = 0;
	}
	for ( int si = 0; si < b.numSides; si++ ) {
		int pi = (int)(intptr_t)b.sides[si].plane;
		b.sides[si].plane = &b.planes[pi];
		b.sides[si].shaderNum = 0;
		b.sides[si].surfaceFlags = 0;
	}

	// --- insert every brush AABB into all overlapping leaves ---------------
	// This is the shared registration the trace's box-expansion descent needs.
	{
		canonLeafBuckets_t lb;
		memset( &lb, 0, sizeof( lb ) );
		lb.numLeafs = numLeafs;
		lb.data  = (int **)Z_Malloc( numLeafs * sizeof(int*) );
		lb.count = (int  *)Z_Malloc( numLeafs * sizeof(int) );
		lb.cap   = (int  *)Z_Malloc( numLeafs * sizeof(int) );
		memset( lb.data,  0, numLeafs * sizeof(int*) );
		memset( lb.count, 0, numLeafs * sizeof(int) );
		memset( lb.cap,   0, numLeafs * sizeof(int) );

		for ( int bi = 0; bi < b.numBrushes; bi++ )
			Canon_InsertBrushIntoTree( &lb, bi, b.brushes[bi].bounds[0], b.brushes[bi].bounds[1], 0 );

		// Flatten buckets into a single leafbrushes[] array + set leaf ranges.
		for ( int li2 = 0; li2 < numLeafs; li2++ ) {
			if ( lb.count[li2] == 0 ) { leafs[li2].firstLeafBrush = 0; leafs[li2].numLeafBrushes = 0; continue; }
			Canon_GrowLeafBrushes( &b, lb.count[li2] );
			leafs[li2].firstLeafBrush = b.numLeafBrushes;
			leafs[li2].numLeafBrushes = lb.count[li2];
			memcpy( &b.leafbrushes[b.numLeafBrushes], lb.data[li2], lb.count[li2] * sizeof(int) );
			b.numLeafBrushes += lb.count[li2];
			Z_Free( lb.data[li2] );
		}
		Z_Free( lb.data ); Z_Free( lb.count ); Z_Free( lb.cap );
	}

	// --- copy the live hull-0 node tree, rebasing plane pointers -----------
	// Node topology (children, leaf encoding) is identical to cm.nodes; only the
	// plane pointer is rebased into the canonical plane pool (same index).
	cNode_t *nodes = (cNode_t *)Z_Malloc( cm.numNodes * sizeof( cNode_t ) );
	for ( int ni = 0; ni < cm.numNodes; ni++ ) {
		int pIdx = (int)( cm.nodes[ni].plane - cm.planes );
		nodes[ni].plane = &b.planes[pIdx];
		nodes[ni].children[0] = cm.nodes[ni].children[0];
		nodes[ni].children[1] = cm.nodes[ni].children[1];
	}

	cmCanon.valid          = qtrue;
	cmCanon.numPlanes      = b.numPlanes;
	cmCanon.planes         = b.planes;
	cmCanon.numBrushSides  = b.numSides;
	cmCanon.brushsides     = b.sides;
	cmCanon.numBrushes     = b.numBrushes;
	cmCanon.brushes        = b.brushes;
	cmCanon.numNodes       = cm.numNodes;
	cmCanon.nodes          = nodes;
	cmCanon.numLeafs       = numLeafs;
	cmCanon.leafs          = leafs;
	cmCanon.numLeafBrushes = b.numLeafBrushes;
	cmCanon.leafbrushes    = b.leafbrushes;
	cmCanon.buildMsec      = Com_Milliseconds() - startMsec;

	// --- ADOPT-AT-LOAD: install the canonical arrays as the live WORLD-trace view.
	// The world trace + PVS/area now read cm.world (these canonical arrays) directly,
	// with no per-trace swap. The loaded cm.* arrays stay intact and untouched as the
	// SUBMODEL view (doors/plats keep their base-relative leafbrush offsets and raw
	// side/plane pointers valid). The canonical set is internally self-consistent
	// (nodes->leafs->leafbrushes->brushes->brushsides->planes all canonical), so the
	// world walk is closed over these arrays.
	cm.world.numNodes       = cmCanon.numNodes;       cm.world.nodes       = cmCanon.nodes;
	cm.world.numLeafs       = cmCanon.numLeafs;        cm.world.leafs       = cmCanon.leafs;
	cm.world.numLeafBrushes = cmCanon.numLeafBrushes;  cm.world.leafbrushes = cmCanon.leafbrushes;
	cm.world.numBrushes     = cmCanon.numBrushes;      cm.world.brushes     = cmCanon.brushes;
	cm.world.numBrushSides  = cmCanon.numBrushSides;   cm.world.brushsides  = cmCanon.brushsides;
	cm.world.numPlanes      = cmCanon.numPlanes;       cm.world.planes      = cmCanon.planes;

	Com_Log( SEV_INFO, LOG_CH(ch_cm_canon),
		"[CMCANON] map=%s brushes=%d (solid=%d water=%d slime=%d lava=%d clip=%d) sides=%d "
		"boxBevels=%d edgeBevels=%d leavesReg=%d degenerate=%d nodes=%d leafs=%d "
		"planes=%d buildMs=%d\n",
		cm.name, cmCanon.numBrushes,
		cmCanon.statSolidBrushes, cmCanon.statWaterBrushes, cmCanon.statSlimeBrushes, cmCanon.statLavaBrushes,
		cmCanon.statClipBrushes,
		cmCanon.numBrushSides, cmCanon.statBoxBevels, cmCanon.statEdgeBevels,
		cmCanon.statLeavesRegistered, cmCanon.statDegenerate,
		cmCanon.numNodes, cmCanon.numLeafs, cmCanon.numPlanes, cmCanon.buildMsec );

	Com_Log( SEV_INFO, LOG_CH(ch_cm_canon),
		"[CMCANON-CLIP] map=%s hull1Regions=%d deflateDegen=%d redundantDropped=%d clipBrushesEmitted=%d\n",
		cm.name, cmCanon.statClipRegions, cmCanon.statClipDeflateDegen,
		cmCanon.statClipRedundant, cmCanon.statClipBrushes );

	// --- Canonical model live: all Q1 collision runs on the generic brush engine
	// over the canonical(+clip) model; submodels/special handles stay on the generic
	// engine over the live cm arrays. This is the only Q1 collision path. The Q1
	// format already registered cmTracer_q1canon as its tracer at load; re-affirm it
	// here (idempotent) so the dispatch pointer is unambiguously the canonical one
	// now that the model is valid.
	cm.tracer = &cmTracer_q1canon;
	Com_Log( SEV_INFO, LOG_CH(ch_cm_canon),
		"[CMCANON] live collision dispatch on canonical model (tracer=q1canon)\n" );
}

void CMQ1_FreeCanonicalModel( void ) {
	if ( cmCanon.planes )      Z_Free( cmCanon.planes );
	if ( cmCanon.brushsides )  Z_Free( cmCanon.brushsides );
	if ( cmCanon.brushes )     Z_Free( cmCanon.brushes );
	if ( cmCanon.nodes )       Z_Free( cmCanon.nodes );
	if ( cmCanon.leafs )       Z_Free( cmCanon.leafs );
	if ( cmCanon.leafbrushes ) Z_Free( cmCanon.leafbrushes );
	memset( &cmCanon, 0, sizeof( cmCanon ) );
}

// ===========================================================================
//
//  ORIGIN-SPACE NAV GEOMETRY  (canonical collision brushes -> face soup)
//
//  Emits the navmesh bake input from the SAME canonical collision brushes the
//  live tracer uses, so mesh and trace share ONE collision source. The z-origin
//  convention is made structural (a walkable poly's z is the standing-player
//  ORIGIN z); the XY player-radius is handled by Recast's own walkableRadius
//  erosion (the standard pipeline), not by pre-expanding the geometry.
//
//  Principles:
//   1. FULL FACE SOUP. Every face of every brush is emitted (solid + clip as
//      obstruction; liquid brushes contribute contents-typed floor). Recast's
//      rcMarkWalkableTriangles marks up-faces walkable; wall/side/down faces
//      rasterize as SOLID spans that clip floors and clearance.
//   2. BURIED-FACE CULL BY LEAF ADJACENCY. A solid brush's face is internal iff
//      the region just across it is SOLID (both-sides-solid). Tested exactly by
//      the hull-0 leaf-contents walk (Q1) / native point-contents (Q3) — no
//      sampling heuristic. Internal faces are dropped so no phantom floor is
//      born inside solid. Faces reaching the world-closure margin are dropped by
//      the content-envelope reject.
//   3. UNIFORM Z TRANSLATION by |MINS_Z|. Pure translation of the whole soup:
//      floors move to the standing-origin height and every floor-to-ceiling gap
//      is preserved, so Recast's walkH/crouchH span scan keeps discriminating
//      crouch (40<=gap<56) untouched by construction.
//   4. NO XY PRE-EXPANSION. walkableRadius = 15 (the ORIGINAL value, whose
//      semantics many downstream constants encode) erodes the walkable area from
//      walls and deletes sub-player-width tops (thin wall/clip caps) — exactly
//      the phantom-sheet class an expanded soup could not remove.
//
//  Q1 uses the canonical model; Q3's native cm.brushes ARE canonical — one
//  emitter, both formats, one origin-space pipeline.
//
// ===========================================================================

// Player box (bg_public.h standing dimensions, mirrored ABI-neutrally). Only
// |MINS_Z| is used (the Z origin lift); XY is Recast's erosion job now.
static const float CM_NAV_BOX_MINS[3] = { -15.0f, -15.0f, -24.0f };

// Nav area IDs (mirror navAreaId_t so cm need not include nav headers).
#define CM_NAVAREA_GROUND  1
#define CM_NAVAREA_WATER   2
#define CM_NAVAREA_LAVA    6

// Step just outside the unexpanded face for the leaf-adjacency solid probe.
#define CM_NAV_ADJ_PROBE   2.0f

// Buried-under-higher-floor cull (bg_public.h box + step tolerance).  Drops the
// crouch-height standing box down an up-face's column; if it rests a climb-step
// or more above the face's own standing origin, a higher floor owns the column
// and the face is buried.  Crouch top height keeps genuine low-ceiling floors.
#define CM_NAV_STAND_HW       15.0f  // PLAYER_WIDTH (footprint half-extent)
#define CM_NAV_CROUCH_MAXS_Z  16.0f  // CROUCH_MAXS_Z (fit under low ceilings)
#define CM_NAV_BURIED_UP      64.0f  // start the drop this far above the face origin
#define CM_NAV_STAND_TOL      18.0f  // NAV_WALKABLE_CLIMB: vertical step slack

// Leaf-adjacency solid test: is the region at p SOLID in the true collision?
// Q1 walks the hull-0 leaf-contents tree; Q3 uses native brush point-contents.
// Clip and liquid contents are NOT solid here, so a clip/liquid face against
// empty space is kept (obstruction / water edge); only both-sides-SOLID internal
// faces are culled.
static qboolean CM_NavPointSolid( const vec3_t p ) {
	if ( cmCanon.valid )
		return Canon_PointInHull0Solid( p );
	return ( CMQ3_PointContents( p, 0 ) & CONTENTS_SOLID ) != 0;
}

// Public wrapper over the nav solid test, for the navmesh bake's ceiling-less
// column guard (nav_impl.cpp).  Exposes the SAME test the internal face cull
// uses so the bake and the soup agree on what "solid" means -- one definition,
// two callers, rather than a second reimplementation drifting from this one.
qboolean CM_NavPointSolidPublic( const vec3_t p ) {
	return CM_NavPointSolid( p );
}

// Obstruction test for the clip-brush adjacency cull: is the region at p SOLID
// OR PLAYER-CLIP (i.e. inside the obstruction mass)? Uses the live canonical
// point-contents so it sees the clip volumes too. A clip face whose neighbor is
// obstruction is internal (buried between stacked clip/solid) and is culled, so
// only clip faces bordering reachable empty space emit as fences.
static qboolean CM_NavPointObstruction( const vec3_t p ) {
	return ( CM_PointContents( p, 0 ) & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) ) != 0;
}

// Growable Quake-space triangle soup accumulator.
typedef struct {
	float *verts;  int numVerts;  int capVerts;   // 3 floats each, Quake xyz
	int   *tris;   int numTris;   int capTris;     // 3 ints each
	unsigned char *areas; // one per tri
} cmNavSoup_t;

static void CMNav_GrowVerts( cmNavSoup_t *s, int need ) {
	if ( s->numVerts + need <= s->capVerts ) return;
	int nc = s->capVerts ? s->capVerts : 8192;
	while ( s->numVerts + need > nc ) nc *= 2;
	float *nv = (float *)Z_Malloc( nc * 3 * sizeof(float) );
	if ( s->verts ) { memcpy( nv, s->verts, s->numVerts * 3 * sizeof(float) ); Z_Free( s->verts ); }
	s->verts = nv; s->capVerts = nc;
}
static void CMNav_GrowTris( cmNavSoup_t *s, int need ) {
	if ( s->numTris + need <= s->capTris ) return;
	int nc = s->capTris ? s->capTris : 8192;
	while ( s->numTris + need > nc ) nc *= 2;
	int *nt = (int *)Z_Malloc( nc * 3 * sizeof(int) );
	unsigned char *na = (unsigned char *)Z_Malloc( nc * sizeof(unsigned char) );
	if ( s->tris )  { memcpy( nt, s->tris, s->numTris * 3 * sizeof(int) ); Z_Free( s->tris ); }
	if ( s->areas ) { memcpy( na, s->areas, s->numTris * sizeof(unsigned char) ); Z_Free( s->areas ); }
	s->tris = nt; s->areas = na; s->capTris = nc;
}
static int CMNav_AddVert( cmNavSoup_t *s, const float v[3] ) {
	CMNav_GrowVerts( s, 1 );
	int idx = s->numVerts;
	s->verts[idx*3+0] = v[0]; s->verts[idx*3+1] = v[1]; s->verts[idx*3+2] = v[2];
	s->numVerts++;
	return idx;
}
static void CMNav_AddTri( cmNavSoup_t *s, int a, int b, int c, unsigned char area ) {
	CMNav_GrowTris( s, 1 );
	s->tris[s->numTris*3+0] = a; s->tris[s->numTris*3+1] = b; s->tris[s->numTris*3+2] = c;
	s->areas[s->numTris] = area;
	s->numTris++;
}

static unsigned char CMNav_AreaForContents( int contents ) {
	if ( contents & CONTENTS_WATER )                   return CM_NAVAREA_WATER;
	if ( contents & ( CONTENTS_LAVA|CONTENTS_SLIME ) ) return CM_NAVAREA_LAVA;
	return CM_NAVAREA_GROUND;
}

// Emit ALL faces of one brush into the soup (unexpanded), culling internal
// (both-sides-solid) faces by leaf adjacency and content-envelope escapes.
// Returns the number of faces emitted.
static int CMNav_EmitBrush( cmNavSoup_t *soup, const cbrush_t *br, float zLift,
                            const vec3_t envMins, const vec3_t envMaxs, int *outCulledInternal,
                            int *outClippedFaces ) {
	int i;
	int n = br->numsides;
	if ( n < 4 || n > CANON_MAX_BRUSH_SIDES ) return 0;

	// Local plane set (unexpanded — walkableRadius handles the player radius).
	cplane_t local[CANON_MAX_BRUSH_SIDES];
	for ( i = 0; i < n; i++ ) {
		const cplane_t *sp = br->sides[i].plane;
		VectorCopy( sp->normal, local[i].normal );
		local[i].dist     = sp->dist;
		local[i].type     = sp->type;
		local[i].signbits = sp->signbits;
	}

	unsigned char area = CMNav_AreaForContents( br->contents );
	qboolean brushIsSolid = ( br->contents & CONTENTS_SOLID ) != 0;
	int emitted = 0;

	for ( i = 0; i < n; i++ ) {
		const cplane_t *sp = br->sides[i].plane;

		canonWinding_t w;
		if ( !Canon_CreateBrushWinding( local, n, i, &w ) ) continue;
		if ( w.numPts < 3 ) continue;

		// Content-envelope CLIP + centroid: a face whose winding escapes the content
		// AABB is unbounded (world-closure margin). Keeping the geom bounds tight for
		// the OMC producers (which scan them) is the reason this test exists, and
		// CLIPPING serves that intent better than discarding: what survives is inside
		// the envelope by construction, while the part of the face that lies within
		// the world is retained.
		//
		// Discarding the whole face silently deleted real floor. A half-space clip
		// brush (a legitimate Q1 hull-1 product: few bounding planes, so some extent
		// runs to the ±65536 world closure) can carry a FINITE top surface over a
		// room while its winding corners sit far outside the envelope. The old
		// `escaped -> continue` dropped that top entirely, leaving the room with
		// collision floor but no navmesh — and because the rejection was counted
		// nowhere, the summary line looked healthy. Clip instead, and count it.
		//
		// The chop is the same helper + loop shape Canon_CreateBrushWinding uses.
		// Canon_ChopWindingInPlace keeps the BACK half-space of (normal,dist), so the
		// envelope's six inward-facing planes are +axis at envMaxs and -axis at
		// -envMins. A chop that leaves fewer than 3 points means the face has nothing
		// inside the envelope at all — that is the correct residual drop.
		vec3_t cen = { 0, 0, 0 };
		{
			qboolean escaped = qfalse;
			int wi2, ax;
			for ( wi2 = 0; wi2 < w.numPts; wi2++ ) {
				if ( w.pts[wi2][0] < envMins[0] || w.pts[wi2][0] > envMaxs[0] ||
				     w.pts[wi2][1] < envMins[1] || w.pts[wi2][1] > envMaxs[1] ||
				     w.pts[wi2][2] < envMins[2] || w.pts[wi2][2] > envMaxs[2] )
					escaped = qtrue;
			}
			if ( escaped ) {
				for ( ax = 0; ax < 3; ax++ ) {
					float nrm[3];
					nrm[0] = nrm[1] = nrm[2] = 0.0f;
					nrm[ax] = 1.0f;
					if ( !Canon_ChopWindingInPlace( &w, nrm, envMaxs[ax] ) ) { w.numPts = 0; break; }
					nrm[ax] = -1.0f;
					if ( !Canon_ChopWindingInPlace( &w, nrm, -envMins[ax] ) ) { w.numPts = 0; break; }
				}
				if ( w.numPts < 3 ) continue;   // nothing of this face lies inside
				if ( outClippedFaces ) (*outClippedFaces)++;
			}
			for ( wi2 = 0; wi2 < w.numPts; wi2++ ) {
				cen[0] += w.pts[wi2][0]; cen[1] += w.pts[wi2][1]; cen[2] += w.pts[wi2][2];
			}
			float inv = 1.0f / (float)w.numPts;
			cen[0] *= inv; cen[1] *= inv; cen[2] *= inv;
		}

		// Leaf-adjacency buried-face cull: a face whose outward neighbor region is
		// obstruction (a shared wall between two solid leaves, a buried brush top,
		// or an internal face inside stacked clip volumes) is internal — no navmesh
		// surface lives there. Probe just OUTSIDE the face along its normal; SOLID
		// brushes test hull-0 solid, CLIP brushes test solid-or-clip.
		//
		// SAMPLED ACROSS THE FACE, not at the centroid alone: a brush side is emitted
		// as ONE face, so a LARGE floor face can span open walkable ground while its
		// CENTROID happens to fall under a pillar/obstruction rising from that floor.
		// A single centroid probe then culls the WHOLE face — dropping real, standable
		// coverage (the arena spawn-pocket floor).  Recast already carves the pillar's
		// footprint out of a rasterized floor via the pillar's own walls, so the fix
		// is to keep such a face: cull only when the face is buried across its EXTENT
		// (centroid AND every vertex, each pulled slightly inward), i.e. a genuinely
		// internal/buried face where all of it is under obstruction.
		{
			qboolean allInternal = qtrue;
			/* Sample points: centroid + each vertex nudged toward the centroid so the
			 * probe stays on the face interior, each offset just outside along the
			 * face normal. */
			for ( int sIdx = -1; sIdx < w.numPts; sIdx++ ) {
				vec3_t base;
				if ( sIdx < 0 ) {
					base[0] = cen[0]; base[1] = cen[1]; base[2] = cen[2];
				} else {
					base[0] = w.pts[sIdx][0] + ( cen[0] - w.pts[sIdx][0] ) * 0.25f;
					base[1] = w.pts[sIdx][1] + ( cen[1] - w.pts[sIdx][1] ) * 0.25f;
					base[2] = w.pts[sIdx][2] + ( cen[2] - w.pts[sIdx][2] ) * 0.25f;
				}
				vec3_t probe = { base[0] + sp->normal[0] * CM_NAV_ADJ_PROBE,
				                 base[1] + sp->normal[1] * CM_NAV_ADJ_PROBE,
				                 base[2] + sp->normal[2] * CM_NAV_ADJ_PROBE };
				qboolean internal = brushIsSolid ? CM_NavPointSolid( probe )
				                                 : CM_NavPointObstruction( probe );
				if ( !internal ) { allInternal = qfalse; break; }
			}
			if ( allInternal ) {
				if ( outCulledInternal ) (*outCulledInternal)++;
				continue;   // whole face buried — internal, no navmesh surface here
			}
		}

		// Area typing: a walkable (up-facing) SOLID floor sitting under water is a
		// swim/wade surface — tag it WATER so the follower and path cost treat it as
		// such (the liquid volume itself is not geometry). A floor submerged under
		// LAVA or SLIME is NOT a standing surface at all: a bot on it is dead, and
		// the player's collision hull never reaches the pit bottom (a clip brush or
		// the kill-volume stops it well above). Emitting that deep pit bottom as a
		// walkable floor produces a phantom poly the standing box can never validate
		// against (its origin rests dozens of units above the emitted z). So DROP the
		// up-face outright when it lies under lava/slime — the real walkable surface
		// in that column, if any (a clip-brush top), emits its own up-face at the
		// correct height. Non-up faces keep the brush's base area; they are
		// obstruction and never become floor polys.
		unsigned char faceArea = area;
		if ( sp->normal[2] > 0.70710678f ) {
			vec3_t above = { cen[0], cen[1], cen[2] + 2.0f };
			int ac = CM_PointContents( above, 0 );
			if ( ac & ( CONTENTS_LAVA|CONTENTS_SLIME ) )
				continue;   // lava/slime-submerged floor — not walkable, skip
			else if ( ac & CONTENTS_WATER )               faceArea = CM_NAVAREA_WATER;

			// Buried-under-higher-floor cull.  The 2u leaf-adjacency probe above only
			// catches a face buried by an IMMEDIATELY-adjacent solid; it misses a
			// deep up-face sitting under a SEPARATE, higher surface across an air gap
			// (a pit bottom below a player-clip cap, a lower shelf under an upper
			// floor).  A player stands on the HIGHER surface, so this deeper face is
			// not the column's walkable floor — emitting it makes a phantom poly tens
			// of units below the real standing floor (a severed, unreachable island
			// and the nav_validate tail).  Detect it the same way nav_validate does:
			// drop the standing box down the column from well above; if it rests a
			// full climb-step or more ABOVE this face's own standing origin, a higher
			// floor owns the column and this face is buried — drop it.  The box uses
			// the crouch top height so a genuine low-ceiling floor (which the box
			// rests ON, the ceiling merely above it) is preserved; only faces with a
			// higher standable surface fail.  WATER floors are exempt.
			if ( faceArea != CM_NAVAREA_WATER ) {
				const vec3_t bmins = { -CM_NAV_STAND_HW, -CM_NAV_STAND_HW, CM_NAV_BOX_MINS[2] };
				const vec3_t bmaxs = {  CM_NAV_STAND_HW,  CM_NAV_STAND_HW, CM_NAV_CROUCH_MAXS_Z };
				const float faceOrigin = cen[2] - CM_NAV_BOX_MINS[2];   // face z + |MINS_Z|
				vec3_t bst = { cen[0], cen[1], faceOrigin + CM_NAV_BURIED_UP };
				vec3_t ben = { cen[0], cen[1], faceOrigin - CM_NAV_STAND_TOL };
				trace_t btr;
				CM_BoxTrace( &btr, bst, ben, bmins, bmaxs, 0, MASK_PLAYERSOLID, qfalse );
				if ( !btr.startsolid && !btr.allsolid && btr.fraction < 1.0f &&
				     btr.endpos[2] > faceOrigin + CM_NAV_STAND_TOL )
					continue;   // box rests a step+ above — a higher floor buries this
			}
		}

		// Emit as a triangle fan, translating Z to origin (uniform lift). Winding
		// is reversed (c,b,a) so the Recast face normal matches the Quake outward
		// normal after Nav_QuakeToRecast — up-faces read as walkable, walls/ceilings
		// as obstruction.
		int firstIdx = -1, prevIdx = -1, wi;
		for ( wi = 0; wi < w.numPts; wi++ ) {
			float v[3] = { w.pts[wi][0], w.pts[wi][1], w.pts[wi][2] + zLift };
			int idx = CMNav_AddVert( soup, v );
			if ( wi == 0 ) firstIdx = idx;
			else if ( wi >= 2 ) CMNav_AddTri( soup, idx, prevIdx, firstIdx, faceArea );
			prevIdx = idx;
		}
		emitted++;
	}
	return emitted;
}

// CM_BuildNavGeometry — assemble the origin-space triangle soup for the current
// map from the canonical collision brushes (unexpanded; Recast erodes by the
// player radius). Verts are QUAKE-space (the nav layer converts to Recast).
// Allocates with Z_Malloc; free with CM_FreeNavGeometry. Returns qtrue on success.
qboolean CM_BuildNavGeometry( float **outVerts, int *outNumVerts,
                              int **outTris, unsigned char **outAreas, int *outNumTris ) {
	const cbrush_t *brushes = NULL;
	int numBrushes = 0;

	if ( cmCanon.valid ) {           // Q1: canonical model (solids + liquids + clip)
		brushes = cmCanon.brushes; numBrushes = cmCanon.numBrushes;
	} else {                          // Q3 (or Q1 build failed): native brushes ARE canonical
		brushes = cm.brushes; numBrushes = cm.numBrushes;
	}
	if ( !brushes || numBrushes <= 0 ) return qfalse;

	const float zLift = -CM_NAV_BOX_MINS[2]; // |MINS_Z| = 24: foot -> standing origin

	// Content envelope: the canonical model closes open solid leaves against a
	// large world-clip margin and Q1 adds a world shell — those must NOT feed nav
	// geometry (they'd balloon the geom bounds the OMC producers scan). Restrict
	// to the world content AABB + a modest margin.
	vec3_t envMins, envMaxs;
	{
		static const float NAV_CONTENT_MARGIN = 64.0f;
		int ax;
		for ( ax = 0; ax < 3; ax++ ) {
			envMins[ax] = cm.cmodels[0].mins[ax] - NAV_CONTENT_MARGIN;
			envMaxs[ax] = cm.cmodels[0].maxs[ax] + NAV_CONTENT_MARGIN;
		}
	}

	cmNavSoup_t soup;
	memset( &soup, 0, sizeof( soup ) );

	int brushesUsed = 0, facesEmitted = 0, culledInternal = 0;
	int skippedContents = 0, skippedEnvelope = 0, clippedFaces = 0;
	for ( int bi = 0; bi < numBrushes; bi++ ) {
		const cbrush_t *br = &brushes[bi];
		int c = br->contents;
		// Obstruction geometry only: SOLID and PLAYER-CLIP brushes (both block the
		// box and rasterize as walls/floors/clearance). Liquid brushes are the
		// contents SOURCE (read live via CM_PointContents by the water-edge logic),
		// NOT geometry — their AABB-volume faces would otherwise become phantom
		// floors at the liquid SURFACE, where the box drops through to the real
		// solid floor below. The walkable surface in liquid is the solid floor
		// under it; area-typing that floor as WATER is done from contents below.
		// MONSTERCLIP-only brushes (no player content) are skipped so bots plan on
		// the player-reachable surface.
		if ( !( c & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) ) ) {
			skippedContents++;
			continue;
		}
		if ( br->bounds[0][0] > envMaxs[0] || br->bounds[1][0] < envMins[0] ||
		     br->bounds[0][1] > envMaxs[1] || br->bounds[1][1] < envMins[1] ||
		     br->bounds[0][2] > envMaxs[2] || br->bounds[1][2] < envMins[2] ) {
			skippedEnvelope++;
			continue;
		}
		int fe = CMNav_EmitBrush( &soup, br, zLift, envMins, envMaxs, &culledInternal, &clippedFaces );
		if ( fe > 0 ) { brushesUsed++; facesEmitted += fe; }
	}

	if ( soup.numTris == 0 ) {
		if ( soup.verts ) Z_Free( soup.verts );
		if ( soup.tris )  Z_Free( soup.tris );
		if ( soup.areas ) Z_Free( soup.areas );
		return qfalse;
	}

	*outVerts = soup.verts; *outNumVerts = soup.numVerts;
	*outTris  = soup.tris;  *outAreas = soup.areas; *outNumTris = soup.numTris;

	Com_Log( SEV_INFO, LOG_CH(ch_cm_canon),
		"[CMNAVGEOM] map=%s source=%s brushesUsed=%d faces=%d culledInternal=%d "
		"skipContents=%d skipEnvelope=%d clippedFaces=%d verts=%d tris=%d zLift=%.0f\n",
		cm.name, cmCanon.valid ? "canonical" : "native-q3",
		brushesUsed, facesEmitted, culledInternal, skippedContents, skippedEnvelope,
		clippedFaces, soup.numVerts, soup.numTris, zLift );
	return qtrue;
}

void CM_FreeNavGeometry( float *verts, int *tris, unsigned char *areas ) {
	if ( verts ) Z_Free( verts );
	if ( tris )  Z_Free( tris );
	if ( areas ) Z_Free( areas );
}

// ===========================================================================
//
//  cmTracer_q1canon — LIVE canonical collision dispatch (single trace engine)
//
//  The ONLY Q1 collision path. Since adopt-at-load installed the canonical(+clip)
//  arrays as cm.world, the generic engine (cm_trace.c / cm_test.c) traces the
//  canonical world for model 0 and the loaded submodel arrays for model N — the
//  world/submodel view split lives in CM_Trace itself now. So this tracer is a
//  plain pass-through to the generic engine: identical to the Q3 tracer, no
//  per-trace field swap. (The former Canon_SwapIn/Out and the TraceStock/
//  PointContentsStock wrappers are retired; the swap was the migration artifact,
//  the adopt-at-load install replaces it.) Q1 and Q3 now share ONE collision
//  engine AND one dispatch shape.
//
// ===========================================================================

static void CMQ1Canon_Trace( trace_t *results, const vec3_t start, const vec3_t end,
                             const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                             const vec3_t origin, int brushmask, qboolean capsule,
                             const sphere_t *sphere ) {
	CM_Trace( results, start, end, mins, maxs, model, origin, brushmask, capsule, sphere );
}

static int CMQ1Canon_PointContents( const vec3_t p, clipHandle_t model ) {
	return CMQ3_PointContents( p, model );
}

static void CMQ1Canon_BiSphereTrace( trace_t *results, const vec3_t start, const vec3_t end,
                                     float startRad, float endRad, clipHandle_t model, int brushmask ) {
	CMQ3_BiSphereTrace( results, start, end, startRad, endRad, model, brushmask );
}

cmTracer_t cmTracer_q1canon = {
	"q1canon",
	CMQ1Canon_Trace,
	CMQ1Canon_PointContents,
	CMQ1Canon_BiSphereTrace,
};
