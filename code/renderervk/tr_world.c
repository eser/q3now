// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "tr_local.h"
#include "../renderercommon/r_log.h"  // rilog-channel-mechanism Turn B — renderer.assets
#include "../renderercommon/r_q1_texture.h"

R_LOG_DECLARE_CHANNEL( rch_assets, "renderer.assets" );



/*
=================
R_CullTriSurf

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static qboolean	R_CullTriSurf( srfTriangles_t *cv ) {
	int 	boxCull;

	boxCull = R_CullLocalBox( cv->bounds );

	if ( boxCull == CULL_OUT ) {
		return qtrue;
	}
	return qfalse;
}

/*
=================
R_CullGrid

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static qboolean	R_CullGrid( srfGridMesh_t *cv ) {
	int 	boxCull;
	int 	sphereCull;

	if ( r_nocurves->integer ) {
		return qtrue;
	}

	if ( tr.currentEntityNum != REFENTITYNUM_WORLD ) {
		sphereCull = R_CullLocalPointAndRadius( cv->localOrigin, cv->meshRadius );
	} else {
		sphereCull = R_CullPointAndRadius( cv->localOrigin, cv->meshRadius );
	}

	// check for trivial reject
	if ( sphereCull == CULL_OUT )
	{
		tr.pc.c_sphere_cull_patch_out++;
		return qtrue;
	}
	// check bounding box if necessary
	if ( sphereCull == CULL_CLIP ) {
		tr.pc.c_sphere_cull_patch_clip++;

		boxCull = R_CullLocalBox( cv->meshBounds );

		if ( boxCull == CULL_OUT ) {
			tr.pc.c_box_cull_patch_out++;
			return qtrue;
		}
		if ( boxCull == CULL_IN ) {
			tr.pc.c_box_cull_patch_in++;
		} else {
			tr.pc.c_box_cull_patch_clip++;
		}
	} else {
		tr.pc.c_sphere_cull_patch_in++;
	}

	return qfalse;
}


/*
================
R_CullSurface

Tries to back face cull surfaces before they are lighted or
added to the sorting list.

This will also allow mirrors on both sides of a model without recursion.
================
*/
static qboolean	R_CullSurface( const surfaceType_t *surface, shader_t *shader ) {
	srfSurfaceFace_t *sface;
	float			d;

	if ( r_nocull->integer ) {
		return qfalse;
	}

	if ( *surface == SF_GRID ) {
		return R_CullGrid( (srfGridMesh_t *)surface );
	}

	if ( *surface == SF_TRIANGLES ) {
		return R_CullTriSurf( (srfTriangles_t *)surface );
	}

	if ( *surface != SF_FACE ) {
		return qfalse;
	}

	if ( shader->cullType == CT_TWO_SIDED ) {
		return qfalse;
	}

	// face culling
	if ( !r_facePlaneCull->integer ) {
		return qfalse;
	}

	sface = ( srfSurfaceFace_t * ) surface;
	d = DotProduct (tr.or.viewOrigin, sface->plane.normal);

	// don't cull exactly on the plane, because there are levels of rounding
	// through the BSP, ICD, and hardware that may cause pixel gaps if an
	// epsilon isn't allowed here
	if ( shader->cullType == CT_FRONT_SIDED ) {
		if ( d < sface->plane.dist - 8 ) {
			return qtrue;
		}
	} else {
		if ( d > sface->plane.dist + 8 ) {
			return qtrue;
		}
	}

	return qfalse;
}


#ifdef USE_PMLIGHT
qboolean R_LightCullBounds( const dlight_t* dl, const vec3_t mins, const vec3_t maxs )
{
	if ( dl->linear ) {
		if (dl->transformed[0] - dl->radius > maxs[0] && dl->transformed2[0] - dl->radius > maxs[0] )
			return qtrue;
		if (dl->transformed[0] + dl->radius < mins[0] && dl->transformed2[0] + dl->radius < mins[0] )
			return qtrue;

		if (dl->transformed[1] - dl->radius > maxs[1] && dl->transformed2[1] - dl->radius > maxs[1] )
			return qtrue;
		if (dl->transformed[1] + dl->radius < mins[1] && dl->transformed2[1] + dl->radius < mins[1] )
			return qtrue;

		if (dl->transformed[2] - dl->radius > maxs[2] && dl->transformed2[2] - dl->radius > maxs[2] )
			return qtrue;
		if (dl->transformed[2] + dl->radius < mins[2] && dl->transformed2[2] + dl->radius < mins[2] )
			return qtrue;

		return qfalse;
	}

	if (dl->transformed[0] - dl->radius > maxs[0])
		return qtrue;
	if (dl->transformed[0] + dl->radius < mins[0])
		return qtrue;

	if (dl->transformed[1] - dl->radius > maxs[1])
		return qtrue;
	if (dl->transformed[1] + dl->radius < mins[1])
		return qtrue;

	if (dl->transformed[2] - dl->radius > maxs[2])
		return qtrue;
	if (dl->transformed[2] + dl->radius < mins[2])
		return qtrue;

	return qfalse;
}


static qboolean R_LightCullFace( const srfSurfaceFace_t* face, const dlight_t* dl )
{
	float d = DotProduct( dl->transformed, face->plane.normal ) - face->plane.dist;
	if ( dl->linear )
	{
		float d2 = DotProduct( dl->transformed2, face->plane.normal ) - face->plane.dist;
		if ( (d < -dl->radius) && (d2 < -dl->radius) )
			return qtrue;
		if ( (d > dl->radius) && (d2 > dl->radius) )
			return qtrue;
	}
	else
	{
		if ( (d < -dl->radius) || (d > dl->radius) )
			return qtrue;
	}

	return qfalse;
}


static qboolean R_LightCullSurface( const surfaceType_t* surface, const dlight_t* dl )
{
	switch (*surface) {
	case SF_FACE:
		return R_LightCullFace( (const srfSurfaceFace_t*)surface, dl );
	case SF_GRID: {
		const srfGridMesh_t* grid = (const srfGridMesh_t*)surface;
		return R_LightCullBounds( dl, grid->meshBounds[0], grid->meshBounds[1] );
		}
	case SF_TRIANGLES: {
		const srfTriangles_t* tris = (const srfTriangles_t*)surface;
		return R_LightCullBounds( dl, tris->bounds[0], tris->bounds[1] );
		}
	default:
		return qfalse;
	};
}
#endif // USE_PMLIGHT


/*
======================
R_AddWorldSurface
======================
*/
static void R_AddWorldSurface( msurface_t *surf, int dlightBits ) {
	if ( surf->viewCount == tr.viewCount ) {
		return;		// already in this view
	}

	surf->viewCount = tr.viewCount;
	// FIXME: bmodel fog?

	// try to cull before dlighting or adding
	if ( R_CullSurface( surf->data, surf->shader ) ) {
		return;
	}

	shader_t *drawShader = surf->shader;
	if ( *surf->data == SF_FACE ) {
		const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)surf->data;
		if ( face->altShader
		     && tr.currentEntity != NULL
		     && tr.currentEntity != &tr.worldEntity
		     && tr.currentEntity->e.frame != 0 ) {
			drawShader = face->altShader;
		}
		/* Time-driven animation is handled at draw time via GPU array (shader_t.q1AnimArray) */
	}

	// Per-pixel dynamic lights are the only path: this world surface goes straight to
	// drawSurfs with no fake-dlight bits (the per-light contribution is added later in
	// the lit pass). dlightBits is unused.
	surf->vcVisible = tr.viewCount;
	R_AddDrawSurf( surf->data, drawShader, surf->fogIndex, 0 );

	// r_unbakeStaticLights: also add this visible world surface to the fp union so the
	// extracted BSP static lights (which drive no PMLIGHT surface walk) can light it.
	// No-op unless the cvar + Forward+ are on and the map has static lights.
	R_AddStaticLitWorldSurf( surf->data, drawShader, surf->fogIndex );
}


// Add a world surface that the host frame-current cull (R_AddWorldSurfacesFlat) already
// marked visible, WITHOUT R_CullSurface — the host AABB-frustum + backface test already
// culled, so the per-surface cull is retired on this path. Mirrors R_AddWorldSurface's
// post-cull tail: the SF_FACE altShader resolution + the PMLIGHT vcVisible + R_AddDrawSurf.
// Each surface is visited once (the flat loop), so the multi-leaf viewCount-dedup is
// unneeded; viewCount is still set (the GPU cull-verify reads it, and downstream PMLIGHT
// R_AddLitSurface gates on vcVisible).
static void R_AddWorldSurfaceVisible( msurface_t *surf )
{
	shader_t *drawShader = surf->shader;

	surf->viewCount = tr.viewCount;

	if ( *surf->data == SF_FACE ) {
		const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)surf->data;
		if ( face->altShader
		     && tr.currentEntity != NULL
		     && tr.currentEntity != &tr.worldEntity
		     && tr.currentEntity->e.frame != 0 ) {
			drawShader = face->altShader;
		}
	}

	// Per-pixel dynamic lights are the only path: add the surface with no fake-dlight
	// bits (the per-light contribution is added later in the lit pass).
	surf->vcVisible = tr.viewCount;
	R_AddDrawSurf( surf->data, drawShader, surf->fogIndex, 0 );

	// r_unbakeStaticLights: also add this visible world surface to the fp union so the
	// extracted BSP static lights (which drive no PMLIGHT surface walk) can light it.
	// No-op unless the cvar + Forward+ are on and the map has static lights.
	R_AddStaticLitWorldSurf( surf->data, drawShader, surf->fogIndex );
}


/*
=============================================================
	PM LIGHTING
=============================================================
*/
#ifdef USE_PMLIGHT
static void R_AddLitSurface( msurface_t *surf, const dlight_t *light )
{
	// since we're not worried about offscreen lights casting into the frustum (ATM !!!)
	// only add the "lit" version of this surface if it was already added to the view
	//if ( surf->viewCount != tr.viewCount )
	//	return;

	// surfaces that were faceculled will still have the current viewCount in vcBSP
	// because that's set to indicate that it's BEEN vis tested at all, to avoid
	// repeated vis tests, not whether it actually PASSED the vis test or not
	// only light surfaces that are GENUINELY visible, as opposed to merely in a visible LEAF
	if ( surf->vcVisible != tr.viewCount ) {
		return;
	}

	if ( surf->shader->lightingStage < 0 ) {
		return;
	}

	if ( surf->lightCount == tr.lightCount )
		return;

	surf->lightCount = tr.lightCount;

	if ( R_LightCullSurface( surf->data, light ) ) {
		tr.pc.c_lit_culls++;
		return;
	}

	// R_AddLitSurf builds the Forward+ deduped union (world + entity, plain-only) —
	// see R_AddLitSurf / R_AddForwardPlusUnionSurf in tr_main.c. The per-(entity,
	// surface) scan-dedup there covers both paths uniformly (the md3 entity path
	// can't use a surface-resident stamp; the scan handles both).
	R_AddLitSurf( surf->data, surf->shader, surf->fogIndex );
}


static void R_RecursiveLightNode( const mnode_t* node )
{
	qboolean children[2];
	msurface_t** mark;
	msurface_t* surf;
	float d;
	do {
		// if the node wasn't marked as potentially visible, exit
		if ( node->visframe != tr.visCount )
			return;

		if ( node->contents != CONTENTS_NODE )
			break;

		children[0] = children[1] = qfalse;

		d = DotProduct( tr.light->origin, node->plane->normal ) - node->plane->dist;
		if ( d > -tr.light->radius ) {
			children[0] = qtrue;
		}
		if ( d < tr.light->radius ) {
			children[1] = qtrue;
		}

		if ( tr.light->linear ) {
			d = DotProduct( tr.light->origin2, node->plane->normal ) - node->plane->dist;
			if ( d > -tr.light->radius ) {
				children[0] = qtrue;
			}
			if ( d < tr.light->radius ) {
				children[1] = qtrue;
			}
		}

		if ( children[0] && children[1] ) {
			R_RecursiveLightNode( node->children[0] );
			node = node->children[1];
		}
		else if ( children[0] ) {
			node = node->children[0];
		}
		else if ( children[1] ) {
			node = node->children[1];
		}
		else {
			return;
		}

	} while ( 1 );

	tr.pc.c_lit_leafs++;

	// add the individual surfaces
	int c = node->nummarksurfaces;
	mark = node->firstmarksurface;
	while ( c-- ) {
		// the surface may have already been added if it spans multiple leafs
		surf = *mark;
		R_AddLitSurface( surf, tr.light );
		mark++;
	}
}
#endif // USE_PMLIGHT


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
=================
R_AddBrushModelSurfaces
=================
*/
void R_AddBrushModelSurfaces ( trRefEntity_t *ent ) {
	bmodel_t	*bmodel;
	int			clip;
	const model_t		*pModel;
	int			i;

	pModel = R_GetModelByHandle( ent->e.hModel );

	bmodel = pModel->bmodel;

	clip = R_CullLocalBox( bmodel->bounds );
	if ( clip == CULL_OUT ) {
		return;
	}

	{
		dlight_t *dl;
		int s;

		for ( s = 0; s < bmodel->numSurfaces; s++ ) {
			R_AddWorldSurface( bmodel->firstSurface + s, 0 );
		}

		R_SetupEntityLighting( &tr.refdef, ent );

		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.or );

		for ( i = 0; i < tr.viewParms.num_dlights; i++ ) {
			dl = &tr.viewParms.dlights[i];
			if ( !R_LightCullBounds( dl, bmodel->bounds[0], bmodel->bounds[1] ) ) {
				tr.lightCount++;
				tr.light = dl;
				for ( s = 0; s < bmodel->numSurfaces; s++ ) {
					R_AddLitSurface( bmodel->firstSurface + s, dl );
				}
			}
		}
	}
}


/*
=============================================================

	WORLD MODEL

=============================================================
*/


/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode( mnode_t *node, unsigned int planeBits, unsigned int dlightBits ) {

	do {
		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount) {
			return;
		}

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?

		if ( !r_nocull->integer ) {
			int		r;

			if ( planeBits & 1 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~1;			// all descendants will also be in front
				}
			}

			if ( planeBits & 2 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~2;			// all descendants will also be in front
				}
			}

			if ( planeBits & 4 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~4;			// all descendants will also be in front
				}
			}

			if ( planeBits & 8 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~8;			// all descendants will also be in front
				}
			}

		}

		if ( node->contents != CONTENTS_NODE ) {
			break;
		}

		// node is just a decision point, so go down both sides
		// since we don't care about sort orders, just go positive to negative
		// (the legacy per-node dlightBits split is retired — per-pixel dynamic lights
		// don't use dlightBits, so the recursion carries 0).

		// recurse down the children, front side first
		R_RecursiveWorldNode( node->children[0], planeBits, 0 );

		// tail recurse
		node = node->children[1];
	} while ( 1 );

	{
		// leaf node, so add mark surfaces
		int			c;
		msurface_t	*surf, **mark;

		tr.pc.c_leafs++;

		// add to z buffer bounds
		if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--) {
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddWorldSurface( surf, dlightBits );
			mark++;
		}
	}
}


/*
===============
R_PointInLeaf
===============
*/
static mnode_t *R_PointInLeaf( const vec3_t p ) {
	mnode_t		*node;
	float		d;
	const cplane_t	*plane;

	if ( !tr.world ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_PointInLeaf: bad model");
	}

	node = tr.world->nodes;
	while( 1 ) {
		if (node->contents != CONTENTS_NODE ) {
			break;
		}
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) {
			node = node->children[0];
		} else {
			node = node->children[1];
		}
	}

	return node;
}

/*
==============
R_ClusterPVS
==============
*/
static const byte *R_ClusterPVS (int cluster) {
	if ( !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters ) {
		return tr.world->novis;
	}

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

/*
=================
R_inPVS
=================
*/
qboolean R_inPVS( const vec3_t p1, const vec3_t p2 ) {
	const mnode_t *leaf;
	const byte	*vis;

	leaf = R_PointInLeaf( p1 );
	if ( leaf->cluster < 0 )
		return qfalse;
	vis = ri.CM_ClusterPVS( leaf->cluster );
	leaf = R_PointInLeaf( p2 );
	if ( leaf->cluster < 0 )
		return qfalse;

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) ) {
		return qfalse;
	}
	return qtrue;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void R_MarkLeaves (void) {
	const byte	*vis;
	mnode_t	*leaf, *parent;
	int		i;
	int		cluster;

	// lockpvs lets designers walk around to determine the
	// extent of the current pvs
	if ( r_lockpvs->integer ) {
		return;
	}

	// current viewcluster
	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again

	// if r_showCluster was just turned on, remark everything
	static int s_showcluster_mod = -1;
	int showcluster_changed = ( r_showCluster->modificationCount != s_showcluster_mod );

	if ( tr.viewCluster == cluster && !tr.refdef.areamaskModified
		&& !showcluster_changed ) {
		return;
	}

	if ( showcluster_changed || r_showCluster->integer ) {
		s_showcluster_mod = r_showCluster->modificationCount;
		if ( r_showCluster->integer ) {
			R_LOG( rch_assets, SEV_INFO, "cluster:%i  area:%i\n", cluster, leaf->area );
		}
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if ( r_novis->integer || tr.viewCluster == -1 ) {
		for (i=0 ; i<tr.world->numnodes ; i++) {
			if (tr.world->nodes[i].contents != CONTENTS_SOLID) {
				tr.world->nodes[i].visframe = tr.visCount;
			}
		}
		return;
	}

	vis = R_ClusterPVS (tr.viewCluster);

	for (i=0,leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++) {
		cluster = leaf->cluster;
		if ( cluster < 0 || cluster >= tr.world->numClusters ) {
			continue;
		}

		// check general pvs
		if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
			continue;
		}

		// check for door connection
		if ( leaf->area >= 0 && (tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7))) ) {
			continue;		// not visible
		}

		parent = leaf;
		do {
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}


// ── GPU-driven world batch decomposition: host frame-current cull drives the draw ──
//
// When r_gpuBatchDecomp is ON, the world cull is a LEAF-GATE + a FLAT per-surface cull,
// NOT R_RecursiveWorldNode's fused per-node descent. The leaf-gate marks reached surfaces
// (PVS + node-frustum prune — the descent is kept here; a flat-scan recursion removal is a
// later refinement) + accumulates visBounds; then a flat host pass reproduces the GPU
// cull's per-surface AABB-frustum + backface test (vk_cull_host_derive_visible) over the
// reached set and adds the survivors to drawSurfs — frame-current (no readback). The GPU
// cull/decompose stay VERIFICATION-ONLY: their host readback is ≥3-frame-lagged (the
// matched-sequence pattern exists for that reason), unusable to drive a live draw; the GPU
// ⊆ asserts instead verify the host re-derivation matches the GPU arithmetic. The result is
// the frustum-tightened subset of the recursion's set (the per-surface AABB frustum is finer
// than R_CullSurface's grid/tri bounds). OFF: R_RecursiveWorldNode, unchanged.
//
// Reached-marker: a per-surface byte array (NOT surf->viewCount — that's set in the add
// pass). dlightBits: under USE_PMLIGHT the world passes dlight=0, so the flat pass needs
// none; LEGACY_DLIGHTS keeps the recursion (this path is PMLIGHT-only, guarded by the cvar).
static byte *s_flat_reached;        // per-surface reached marker (1 = in a reached leaf)
static int   s_flat_reached_cap;

// Flat scan over the leaf array (the BSP leaves, world->nodes[numDecisionNodes..numnodes) —
// the loader stores decision nodes then leaves in one array, R_LoadNodesAndLeafs) to build
// the reached-surface set + visBounds. Per leaf: PVS gate (visframe == visCount, set by
// R_MarkLeaves on leaves + ancestors) + a 4-plane frustum reject (BoxOnPlaneSide r==2 on any
// plane → skip) + visBounds accumulation + marksurface reached-marking. NO node recursion,
// NO planeBits subtree prune. This gives up the recursion's whole-subtree prune + per-plane
// bit-clearing, so the reached set is LOOSER (⊇ the recursion's set), which is SAFE and
// absorbed: a looser visBounds only pushes the far plane OUT (zFar = max-corner-distance,
// never clips), and the per-surface AABB-frustum cull (vk_cull_host_derive_visible /
// cull.comp) re-rejects any over-admitted surface — the looser reached set does not change
// the final visible set. Marks into reachedOut[surf - world->surfaces] (cap = reachedCap,
// dedup by index).
//
// PERF TRADEOFF (honest, measured arena-class): a cache-friendly linear scan with no
// pointer-chasing tree descent and no recursion stack, BUT it gives up the recursion's
// whole-subtree prune — it visits every leaf (a cheap visframe int-compare) and does the
// 4-plane frustum test on every PVS-pass leaf, where the recursion would reject whole
// subtrees at one ancestor test. Measured arena1: 2109 leaves, 355 pass the PVS gate, 321
// frustum-reached — sub-ms either way (the per-surface GPU frustum is the real cull, and the
// PVS gate rejects ~83% of leaves before any plane test), so MEASURED-NEUTRAL on arena-class.
// A regression-risk vector exists on high-leaf-count / low-frustum-coverage maps (e.g. tens
// of thousands of leaves, few visible): the scan is O(total leaves) visframe checks + O(PVS-
// pass leaves) plane tests with no subtree early-out. This is acknowledged and lands against
// the future high-draw-map-class perf gate (a coarse cluster/area pre-reject is a separate
// optional optimization), NOT claimed away.
static void R_MarkReachedLeaves( const world_t *world, byte *reachedOut, int reachedCap )
{
	int li;
	const int firstLeaf = world->numDecisionNodes;
	const int lastLeaf  = world->numnodes;

	for ( li = firstLeaf; li < lastLeaf; li++ ) {
		mnode_t *leaf = &world->nodes[ li ];
		int c;
		msurface_t **mark;

		if ( leaf->visframe != tr.visCount )
			continue;   // not in the PVS / areamask

		if ( !r_nocull->integer ) {
			// fully outside ANY frustum plane → the whole leaf is invisible
			if ( BoxOnPlaneSide( leaf->mins, leaf->maxs, &tr.viewParms.frustum[0] ) == 2 ) continue;
			if ( BoxOnPlaneSide( leaf->mins, leaf->maxs, &tr.viewParms.frustum[1] ) == 2 ) continue;
			if ( BoxOnPlaneSide( leaf->mins, leaf->maxs, &tr.viewParms.frustum[2] ) == 2 ) continue;
			if ( BoxOnPlaneSide( leaf->mins, leaf->maxs, &tr.viewParms.frustum[3] ) == 2 ) continue;
		}

		if ( leaf->mins[0] < tr.viewParms.visBounds[0][0] ) tr.viewParms.visBounds[0][0] = leaf->mins[0];
		if ( leaf->mins[1] < tr.viewParms.visBounds[0][1] ) tr.viewParms.visBounds[0][1] = leaf->mins[1];
		if ( leaf->mins[2] < tr.viewParms.visBounds[0][2] ) tr.viewParms.visBounds[0][2] = leaf->mins[2];
		if ( leaf->maxs[0] > tr.viewParms.visBounds[1][0] ) tr.viewParms.visBounds[1][0] = leaf->maxs[0];
		if ( leaf->maxs[1] > tr.viewParms.visBounds[1][1] ) tr.viewParms.visBounds[1][1] = leaf->maxs[1];
		if ( leaf->maxs[2] > tr.viewParms.visBounds[1][2] ) tr.viewParms.visBounds[1][2] = leaf->maxs[2];

		mark = leaf->firstmarksurface;
		c = leaf->nummarksurfaces;
		while ( c-- ) {
			ptrdiff_t idx = *mark - world->surfaces;
			if ( idx >= 0 && idx < reachedCap )
				reachedOut[ idx ] = 1;   // dedup by index (a multi-leaf surface marked once)
			mark++;
		}
	}
}

static byte *s_flat_visible;        // per-surface host-cull visible flag (GPU-equivalent)
static int   s_flat_visible_cap;

// The flat cutover producer: leaf-gate marks reached + visBounds, then the HOST reproduces
// cull.comp's per-surface test FRAME-CURRENT (backface only) over the reached set — the
// GPU-equivalent visible set, produced host-side this frame (vk_cull_host_derive_visible,
// reusing the host-coherent cull AABB SSBO + this-frame viewOrigin). Then
// R_AddWorldSurfaceVisible adds each visible surface to drawSurfs WITHOUT re-culling (the
// host test already culled — R_CullSurface is RETIRED on this path). The result MATCHES the
// recursion's set: the recursion frustum-culls at LEAF granularity (R_MarkReachedLeaves
// reproduces that leaf-frustum reject) and backface-culls per surface (R_CullSurface);
// there is NO per-surface frustum cull on either side. (An earlier per-surface AABB-frustum
// test here was a TIGHTER cull the recursion never had — it dropped on-screen surfaces in
// leaves that straddle the frustum edge, the surface-dropout / black-void bug — and was
// removed.) dlight=0 (PMLIGHT).
static void R_AddWorldSurfacesFlat( void )
{
	int n = tr.world->numsurfaces, i;

	if ( n > s_flat_reached_cap ) {
		if ( s_flat_reached ) ri.Free( s_flat_reached );
		s_flat_reached     = ri.Malloc( n );
		s_flat_reached_cap = n;
	}
	if ( n > s_flat_visible_cap ) {
		if ( s_flat_visible ) ri.Free( s_flat_visible );
		s_flat_visible     = ri.Malloc( n );
		s_flat_visible_cap = n;
	}
	if ( !s_flat_reached || !s_flat_visible ) {
		// alloc failed — fall back to the recursion (never leave the world undrawn)
		R_RecursiveWorldNode( tr.world->nodes, 15, ( 1ULL << tr.refdef.num_dlights ) - 1 );
		return;
	}
	memset( s_flat_reached, 0, (size_t)n );

	// flat leaf-array scan: mark reached surfaces + accumulate visBounds (frame-current)
	R_MarkReachedLeaves( tr.world, s_flat_reached, n );

	// host frame-current cull: backface-test the reached set using the host-coherent AABB
	// SSBO + this-frame viewOrigin (no per-surface frustum — the recursion frustum-culls at
	// leaf granularity only, which R_MarkReachedLeaves already applied). Frame-current visible
	// set, no lagged readback.
	if ( !vk_cull_host_derive_visible( tr.viewParms.frustum, tr.viewParms.or.origin,
	                                   s_flat_reached, s_flat_visible, n ) ) {
		// host cull unavailable (no AABB SSBO) — fall back to the recursion.
		R_RecursiveWorldNode( tr.world->nodes, 15, ( 1ULL << tr.refdef.num_dlights ) - 1 );
		return;
	}

	// add the visible surfaces to drawSurfs — NO re-cull (the host test culled). Sets
	// surf->viewCount + surf->vcVisible + R_AddDrawSurf, mirroring R_AddWorldSurface's
	// post-cull tail. The per-surface order within a (shader,fog) is irrelevant.
	for ( i = 0; i < n; i++ ) {
		if ( s_flat_visible[i] )
			R_AddWorldSurfaceVisible( &tr.world->surfaces[i] );
	}
}


/*
=============
R_AddWorldSurfaces
=============
*/
void R_AddWorldSurfaces( void ) {
#ifdef USE_PMLIGHT
	dlight_t* dl;
#endif

	if ( !r_drawWorld->integer ) {
		return;
	}

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	// determine which leaves are in the PVS / areamask
	R_MarkLeaves ();

	// clear out the visible min/max
	ClearBounds( tr.viewParms.visBounds[0], tr.viewParms.visBounds[1] );

	// perform frustum culling and add all the potentially visible surfaces
	if ( tr.refdef.num_dlights > MAX_DLIGHTS ) {
		tr.refdef.num_dlights = MAX_DLIGHTS;
	}

	// r_gpuBatchDecomp ON drives the world cull from the host frame-current re-derivation
	// (leaf-gate + flat host AABB cull, walk-free), retiring R_RecursiveWorldNode's
	// per-surface cull. OFF (default) = the recursion, byte-identical. PMLIGHT-only (the
	// flat path passes dlight=0); LEGACY_DLIGHTS keeps the recursion.
#ifdef USE_PMLIGHT
	if ( r_gpuBatchDecomp->integer ) {
		R_AddWorldSurfacesFlat();
	} else
#endif
	{
		R_RecursiveWorldNode( tr.world->nodes, 15, ( 1ULL << tr.refdef.num_dlights ) - 1 );
	}

	// snapshot this (main world) view's per-surface visibility for the GPU cull verify: the
	// cull walk has now set surf->viewCount (PVS+leaf-frustum reached) and surf->vcVisible
	// (== drawSurfs membership) for every world surface. Only the first call per frame (main
	// view) is captured. No-op unless the GPU cull is built. (Renderer→renderervk; the
	// vk_cull_* implementation lives in vk.c.)
	vk_cull_capture_world( &tr.viewParms, tr.world, tr.viewCount );

#ifdef USE_PMLIGHT
	// Per-pixel dynamic lights: walk each light's lit surfaces. When r_dynamiclight is
	// off the dlight list is empty (the master gate clears it), so the loop below is a
	// no-op — no early-return needed now that the fake tier is gone.

	// "transform" all the dlights so that dl->transformed is actually populated
	// (even though HERE it's == dl->origin) so we can always use R_LightCullBounds
	// instead of having copypasted versions for both world and local cases

	R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.viewParms.world );
	for ( int i = 0; i < tr.viewParms.num_dlights; i++ )
	{
		dl = &tr.viewParms.dlights[i];
		dl->head = dl->tail = NULL;
		if ( R_CullDlight( dl ) == CULL_OUT ) {
			tr.pc.c_light_cull_out++;
			continue;
		}
		tr.pc.c_light_cull_in++;
		tr.lightCount++;
		tr.light = dl;
		R_RecursiveLightNode( tr.world->nodes );
	}
#endif // USE_PMLIGHT
}


/*
=================
R_ReleaseWorldCullStatics

Free + NULL the world-cull scratch statics that hold TAG_RENDERER blocks
(ri.Malloc), and zero their caps. Called from vk_release_resources (the
renderer-teardown path that runs on map-transition / vid_restart / shutdown),
so the block these statics point at is invalidated WITH the pointer — mirroring
vk_shadow_snap_release_cpu (vk.c). Without this, ri.FreeAll → Z_FreeTags(
TAG_RENDERER) reclaims the block but leaves the static dangling; the next map's
lazy-grow re-frees that stale pointer → "Z_Free: freed a pointer without ZONEID".
Idempotent (guarded), so the full-teardown path is safe to call it too.

s_flat_reached/s_flat_visible back the LIVE host-cull (R_AddWorldSurfacesFlat,
r_gpuBatchDecomp).
=================
*/
void R_ReleaseWorldCullStatics( void )
{
	if ( s_flat_reached )  { ri.Free( s_flat_reached );  s_flat_reached  = NULL; }
	if ( s_flat_visible )  { ri.Free( s_flat_visible );  s_flat_visible  = NULL; }
	s_flat_reached_cap = 0;
	s_flat_visible_cap = 0;
}
