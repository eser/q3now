// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// cm_q1.c — Native Q1 hull-1 clipnode tracer.
//
// Replaces the brush-synthesis collision path for Q1 BSP29 maps.
// Algorithm ported from FTEQW engine/common/q1bsp.c (Q1BSP_RecursiveHullTrace).

#include "cm_local.h"
#include "qfiles.h"

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
}

// ---------------------------------------------------------------------------
// Content translation
// ---------------------------------------------------------------------------

static int CMQ1_ContentsToQ3( int q1leaf ) {
	switch ( q1leaf ) {
	case -1: return 0;              // EMPTY
	case -2: return CONTENTS_SOLID;
	case -3: return CONTENTS_WATER;
	case -4: return CONTENTS_SLIME;
	case -5: return CONTENTS_LAVA;
	default: return CONTENTS_SOLID; // SOLID / SKY / unknown
	}
}

// ---------------------------------------------------------------------------
// Iterative point-in-hull walk (for PointContents)
// ---------------------------------------------------------------------------

static int CMQ1_HullPointContents( int rootNode, const vec3_t p ) {
	int num = rootNode;
	while ( num >= 0 ) {
		if ( num >= cm.q1.numClipnodes )
			return -1; // walk error → EMPTY
		const q1_dclipnode_t *node = &cm.q1.clipnodes[num];
		const cplane_t *plane = &cm.planes[(int)node->planenum];
		float d;
		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct( plane->normal, p ) - plane->dist;
		num = ( d >= 0.0f ) ? (int)(short)node->children[0]
		                    : (int)(short)node->children[1];
	}
	return num; // negative Q1 leaf contents code
}

// ---------------------------------------------------------------------------
// Recursive parametric hull trace
// Ported from FTEQW Q1BSP_RecursiveHullTrace (engine/common/q1bsp.c).
// ---------------------------------------------------------------------------

#define Q1DIST_EPSILON  0.03125f

typedef enum {
	RHT_EMPTY  = 0,
	RHT_SOLID  = 1,
	RHT_IMPACT = 2,
} rhtResult_t;

typedef struct {
	vec3_t start;
	vec3_t end;
	int    checkcontents;
} q1TraceCtx_t;

static rhtResult_t CMQ1_RecursiveHullTrace( const q1TraceCtx_t *ctx,
                                             int num,
                                             float p1f, float p2f,
                                             const vec3_t p1, const vec3_t p2,
                                             trace_t *tr ) {
REENTER:
	// Leaf node
	if ( num < 0 ) {
		int contents = CMQ1_ContentsToQ3( num );
		if ( contents & ctx->checkcontents ) {
			tr->contents = contents;
			if ( tr->allsolid ) tr->startsolid = qtrue;
			return RHT_SOLID;
		}
		tr->allsolid = qfalse;
		return RHT_EMPTY;
	}

	if ( num >= cm.q1.numClipnodes )
		return RHT_EMPTY; // malformed tree guard

	const q1_dclipnode_t *node = &cm.q1.clipnodes[num];
	const cplane_t *plane = &cm.planes[(int)node->planenum];

	float t1, t2;
	if ( plane->type < 3 ) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	} else {
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	// Trivial same-side descent — tail-call via goto to avoid stack growth
	if ( t1 >= 0 && t2 >= 0 ) { num = (int)(short)node->children[0]; goto REENTER; }
	if ( t1 <  0 && t2 <  0 ) { num = (int)(short)node->children[1]; goto REENTER; }

	// Segment crosses plane — recompute t1/t2 from full trace endpoints for numeric stability
	float ft1, ft2;
	if ( plane->type < 3 ) {
		ft1 = ctx->start[plane->type] - plane->dist;
		ft2 = ctx->end[plane->type]   - plane->dist;
	} else {
		ft1 = DotProduct( plane->normal, ctx->start ) - plane->dist;
		ft2 = DotProduct( plane->normal, ctx->end   ) - plane->dist;
	}

	int side = ( ft1 < 0 ) ? 1 : 0;

	float denom = ft1 - ft2;
	float midf;
	if ( fabsf( denom ) < 1e-6f ) {
		midf = p1f;
	} else {
		midf = ft1 / denom;
		if ( midf < p1f ) midf = p1f;
		if ( midf > p2f ) midf = p2f;
	}

	vec3_t mid;
	for ( int i = 0; i < 3; i++ )
		mid[i] = ctx->start[i] + midf * ( ctx->end[i] - ctx->start[i] );

	// Recurse near side first
	rhtResult_t r = CMQ1_RecursiveHullTrace( ctx,
	    (int)(short)node->children[side], p1f, midf, p1, mid, tr );
	if ( r != RHT_EMPTY && !tr->allsolid )
		return r;

	// Recurse far side
	r = CMQ1_RecursiveHullTrace( ctx,
	    (int)(short)node->children[side^1], midf, p2f, mid, p2, tr );
	if ( r != RHT_SOLID )
		return r;

	// Far side is solid — this is the impact point.
	// Record the plane facing toward the trace start.
	if ( side ) {
		// started behind plane (ft1 < 0) → flip to face start
		tr->plane.dist      = -plane->dist;
		tr->plane.normal[0] = -plane->normal[0];
		tr->plane.normal[1] = -plane->normal[1];
		tr->plane.normal[2] = -plane->normal[2];
	} else {
		tr->plane.dist      =  plane->dist;
		VectorCopy( plane->normal, tr->plane.normal );
	}

	// Recompute fraction using the (possibly flipped) impact plane + epsilon nudge.
	float nt1 = DotProduct( tr->plane.normal, ctx->start ) - tr->plane.dist;
	float nt2 = DotProduct( tr->plane.normal, ctx->end   ) - tr->plane.dist;
	float nd   = nt1 - nt2;
	if ( fabsf( nd ) > 1e-6f )
		midf = ( nt1 - Q1DIST_EPSILON ) / nd;

	if ( !isfinite( midf ) )
		midf = 1.0f;  /* malformed BSP plane → treat as miss */
	tr->fraction = midf < 0.0f ? 0.0f : ( midf > 1.0f ? 1.0f : midf );
	for ( int i = 0; i < 3; i++ )
		tr->endpos[i] = ctx->start[i] + tr->fraction * ( ctx->end[i] - ctx->start[i] );
	return RHT_IMPACT;
}

// ---------------------------------------------------------------------------
// Hull-0 BSP node walkers (actual BSP face geometry, used for bullets / LOS)
// ---------------------------------------------------------------------------

// Point-in-tree walk using cm.nodes[] (hull-0 BSP nodes loaded from Q1 NODES lump).
// Children encode leaf as -(leafnum+1); leafnum indexes cm.q1.leafContents[].
static int CMQ1_NodePointContents( int rootNode, const vec3_t p ) {
	int num = rootNode;
	while ( num >= 0 ) {
		if ( num >= cm.numNodes ) return 0;
		const cNode_t *node = &cm.nodes[num];
		const cplane_t *plane = node->plane;
		float d;
		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct( plane->normal, p ) - plane->dist;
		num = node->children[d >= 0.0f ? 0 : 1];
	}
	int leafnum = -(num + 1);
	if ( leafnum >= 0 && leafnum < cm.q1.numLeafs )
		return cm.q1.leafContents[leafnum];
	return 0;
}

// Recursive parametric ray trace through cm.nodes[] (hull-0).
// Mirrors CMQ1_RecursiveHullTrace but reads cNode_t (plane pointer, 32-bit children).
static rhtResult_t CMQ1_RecursiveNodeTrace( const q1TraceCtx_t *ctx,
                                             int num,
                                             float p1f, float p2f,
                                             const vec3_t p1, const vec3_t p2,
                                             trace_t *tr ) {
REENTER:
	if ( num < 0 ) {
		int leafnum = -(num + 1);
		int contents = 0;
		if ( leafnum >= 0 && leafnum < cm.q1.numLeafs )
			contents = cm.q1.leafContents[leafnum];
		if ( contents & ctx->checkcontents ) {
			tr->contents = contents;
			if ( tr->allsolid ) tr->startsolid = qtrue;
			return RHT_SOLID;
		}
		tr->allsolid = qfalse;
		return RHT_EMPTY;
	}

	if ( num >= cm.numNodes )
		return RHT_EMPTY;

	const cNode_t *node = &cm.nodes[num];
	const cplane_t *plane = node->plane;

	float t1, t2;
	if ( plane->type < 3 ) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	} else {
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	if ( t1 >= 0 && t2 >= 0 ) { num = node->children[0]; goto REENTER; }
	if ( t1 <  0 && t2 <  0 ) { num = node->children[1]; goto REENTER; }

	float ft1, ft2;
	if ( plane->type < 3 ) {
		ft1 = ctx->start[plane->type] - plane->dist;
		ft2 = ctx->end[plane->type]   - plane->dist;
	} else {
		ft1 = DotProduct( plane->normal, ctx->start ) - plane->dist;
		ft2 = DotProduct( plane->normal, ctx->end   ) - plane->dist;
	}

	int side = ( ft1 < 0 ) ? 1 : 0;
	float denom = ft1 - ft2;
	float midf;
	if ( fabsf( denom ) < 1e-6f ) {
		midf = p1f;
	} else {
		midf = ft1 / denom;
		if ( midf < p1f ) midf = p1f;
		if ( midf > p2f ) midf = p2f;
	}

	vec3_t mid;
	for ( int i = 0; i < 3; i++ )
		mid[i] = ctx->start[i] + midf * ( ctx->end[i] - ctx->start[i] );

	rhtResult_t r = CMQ1_RecursiveNodeTrace( ctx,
	    node->children[side], p1f, midf, p1, mid, tr );
	if ( r != RHT_EMPTY && !tr->allsolid )
		return r;

	r = CMQ1_RecursiveNodeTrace( ctx,
	    node->children[side^1], midf, p2f, mid, p2, tr );
	if ( r != RHT_SOLID )
		return r;

	// Far side is solid — record impact plane facing the trace start
	if ( side ) {
		tr->plane.dist      = -plane->dist;
		tr->plane.normal[0] = -plane->normal[0];
		tr->plane.normal[1] = -plane->normal[1];
		tr->plane.normal[2] = -plane->normal[2];
	} else {
		tr->plane.dist = plane->dist;
		VectorCopy( plane->normal, tr->plane.normal );
	}

	// Recompute fraction with epsilon nudge (same as clipnode walker)
	float nt1 = DotProduct( tr->plane.normal, ctx->start ) - tr->plane.dist;
	float nt2 = DotProduct( tr->plane.normal, ctx->end   ) - tr->plane.dist;
	float nd   = nt1 - nt2;
	if ( fabsf( nd ) > 1e-6f )
		midf = ( nt1 - Q1DIST_EPSILON ) / nd;

	if ( !isfinite( midf ) )
		midf = 1.0f;
	tr->fraction = midf < 0.0f ? 0.0f : ( midf > 1.0f ? 1.0f : midf );
	for ( int i = 0; i < 3; i++ )
		tr->endpos[i] = ctx->start[i] + tr->fraction * ( ctx->end[i] - ctx->start[i] );
	return RHT_IMPACT;
}

// ---------------------------------------------------------------------------
// CMQ1_Trace — vtable Trace entry
// ---------------------------------------------------------------------------

void CMQ1_Trace( trace_t *results, const vec3_t start, const vec3_t end,
                  const vec3_t mins, const vec3_t maxs, clipHandle_t model,
                  const vec3_t origin, int brushmask, qboolean capsule,
                  const sphere_t *sphere ) {
	(void)maxs; (void)origin; (void)capsule; (void)sphere;

	// Delegate special handles (box model, capsule, tri-soup) to Q3 brush path.
	if ( model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE ||
	     ( model >= TRI_SOUP_HANDLE_BASE && model < TRI_SOUP_HANDLE_END ) ) {
		CM_Trace( results, start, end, mins, maxs, model, origin, brushmask, capsule, sphere );
		return;
	}

	cm.checkcount++;
	c_traces++;

	memset( results, 0, sizeof( *results ) );
	results->fraction = 1.0f;
	results->allsolid = qtrue; // cleared when first EMPTY leaf is reached

	if ( !cm.q1.clipnodes || !cm.q1.submodelRoots ) {
		// No Q1 data loaded yet; return a miss.
		VectorCopy( end, results->endpos );
		return;
	}

	int subIdx = (int)model;
	if ( subIdx < 0 || subIdx >= cm.q1.numSubmodelRoots ) {
		VectorCopy( end, results->endpos );
		return;
	}

	/* Select hull:
	 *   MASK_PLAYERSOLID / MASK_DEADSOLID include CONTENTS_PLAYERCLIP → hull-1
	 *     (world shrunk by player half-extents, used for box-movement collision).
	 *   All other masks (MASK_SHOT, MASK_SOLID, etc.) → hull-0
	 *     (actual BSP node tree, stops bullets at visible face positions).
	 */
	qboolean useHull1 = ( brushmask & CONTENTS_PLAYERCLIP ) != 0 || !cm.q1.hull0Roots;
	int rootNode = useHull1 ? cm.q1.submodelRoots[subIdx]
	                        : cm.q1.hull0Roots[subIdx];

	if ( rootNode < 0 ) {
		results->allsolid = qfalse;
		VectorCopy( end, results->endpos );
		return;
	}

	q1TraceCtx_t ctx;
	VectorCopy( start, ctx.start );
	VectorCopy( end,   ctx.end   );
	ctx.checkcontents = brushmask;

	// Point-trace case
	if ( start[0] == end[0] && start[1] == end[1] && start[2] == end[2] ) {
		int contents = useHull1
		    ? CMQ1_ContentsToQ3( CMQ1_HullPointContents( rootNode, start ) )
		    : CMQ1_NodePointContents( rootNode, start );
		if ( contents & brushmask ) {
			results->startsolid = qtrue;
			results->allsolid   = qtrue;
			results->fraction   = 0.0f;
			results->contents   = contents;
		} else {
			results->allsolid = qfalse;
		}
		VectorCopy( start, results->endpos );
		return;
	}

	if ( useHull1 ) {
		CMQ1_RecursiveHullTrace( &ctx, rootNode, 0.0f, 1.0f, start, end, results );
	} else {
		CMQ1_RecursiveNodeTrace( &ctx, rootNode, 0.0f, 1.0f, start, end, results );
	}

	if ( results->fraction == 1.0f )
		VectorCopy( end, results->endpos );
}

// ---------------------------------------------------------------------------
// CMQ1_PointContents — vtable PointContents entry
// ---------------------------------------------------------------------------

int CMQ1_PointContents( const vec3_t p, clipHandle_t model ) {
	if ( !cm.numNodes )
		return 0;

	// Submodels and special handles: use Q3 brush path (sub-models don't have
	// liquid volumes that need Q1 point-contents resolution).
	if ( model ) {
		return CMQ3_PointContents( p, model );
	}

	// Walk render BSP (cm.nodes → cm.leafs) — mirrors FTEQW Q1_ModelPointContents.
	// headnode[0] is a NODES lump index, not a CLIPNODES index, so CMQ1_HullPointContents
	// (which walks cm.q1.clipnodes[]) must not be used for world PointContents.
	if ( cm.q1.leafContents && cm.q1.numLeafs > 0 ) {
		int leafnum = CM_PointLeafnum( p );
		if ( leafnum >= 0 && leafnum < cm.q1.numLeafs )
			return cm.q1.leafContents[leafnum];
	}
	return 0;
}

// ---------------------------------------------------------------------------
// CMQ1_BiSphereTrace — vtable BiSphereTrace entry
// ---------------------------------------------------------------------------

void CMQ1_BiSphereTrace( trace_t *results, const vec3_t start, const vec3_t end,
                          float startRad, float endRad, clipHandle_t model,
                          int brushmask ) {
	// Q1 has no bi-sphere hull; delegate to Q3 brush system.
	CMQ3_BiSphereTrace( results, start, end, startRad, endRad, model, brushmask );
}

// ---------------------------------------------------------------------------
// cmTracer_q1 — Q1 vtable
// ---------------------------------------------------------------------------

cmTracer_t cmTracer_q1 = {
	"q1",
	CMQ1_Trace,
	CMQ1_PointContents,
	CMQ1_BiSphereTrace,
};
