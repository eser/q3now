/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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

#include "q_shared.h"
#include "qcommon.h"
#include "cm_polylib.h"

#define	MAX_SUBMODELS			256
#define	BOX_MODEL_HANDLE		(cm.numSubModels)
#define CAPSULE_MODEL_HANDLE	(cm.numSubModels + 1)

// Triangle-soup collision handles live above the inline model range so
// that an IQM (or any other dynamic triangle mesh) can be registered at
// runtime and used with CM_BoxTrace / CM_TransformedBoxTrace like any
// brush model.
#define MAX_TRI_SOUPS			64
#define TRI_SOUP_HANDLE_BASE	(MAX_SUBMODELS + 2)
#define TRI_SOUP_HANDLE_END		(TRI_SOUP_HANDLE_BASE + MAX_TRI_SOUPS)


// forced double-precison functions
#define DotProductDP(x,y)		((double)(x)[0]*(y)[0]+(double)(x)[1]*(y)[1]+(double)(x)[2]*(y)[2])
#define VectorSubtractDP(a,b,c)	((c)[0]=(double)((a)[0]-(b)[0]),(c)[1]=(double)((a)[1]-(b)[1]),(c)[2]=(double)((a)[2]-(b)[2]))
#define VectorAddDP(a,b,c)		((c)[0]=(double)((a)[0]+(b)[0]),(c)[1]=(double)((a)[1]+(b)[1]),(c)[2]=(double)((a)[2]+(b)[2]))


static ID_INLINE double DotProductDPf( const float *v1, const float *v2 ) {
	double x[3], y[3];
	VectorCopy( v1, x );
	VectorCopy( v2, y );
	return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];
}


static ID_INLINE void CrossProductDP( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	double d1[3], d2[3];
	VectorCopy( v1, d1 );
	VectorCopy( v2, d2 );
	cross[0] = d1[1]*d2[2] - d1[2]*d2[1];
	cross[1] = d1[2]*d2[0] - d1[0]*d2[2];
	cross[2] = d1[0]*d2[1] - d1[1]*d2[0];
}


static ID_INLINE vec_t VectorNormalizeDP( vec3_t v ) {
	double	length, ilength, d[3];

	VectorCopy( v, d );
	length = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];

	if ( length ) {
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		ilength = 1.0/(double)sqrt( length );
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		v[0] = d[0] * ilength;
		v[1] = d[1] * ilength;
		v[2] = d[2] * ilength;
	}
		
	return length;
}


typedef struct {
	cplane_t	*plane;
	int			children[2];		// negative numbers are leafs
} cNode_t;

typedef struct {
	int			cluster;
	int			area;

	int			firstLeafBrush;
	int			numLeafBrushes;

	int			firstLeafSurface;
	int			numLeafSurfaces;
} cLeaf_t;

typedef struct cmodel_s {
	vec3_t		mins, maxs;
	cLeaf_t		leaf;			// submodels don't reference the main tree
} cmodel_t;

// Forward declaration so cmTracer_t can reference const sphere_t * before the full def.
typedef struct sphere_s sphere_t;

#define MAX_Q1_HULLS  4

typedef struct {
	int                numClipnodes;
	q1_dclipnode_t    *clipnodes;
	int                numSubmodelRoots;
	int               *submodelRoots;   // hull-1 clipnode roots, one per submodel
	int               *hull0Roots;      // hull-0 BSP node roots, one per submodel
	int                numLeafs;
	int               *leafContents;   // Q3 CONTENTS_* per BSP leaf
} cmQ1Data_t;

typedef struct {
	const char *name;
	void (*Trace)( trace_t *results, const vec3_t start, const vec3_t end,
	               const vec3_t mins, const vec3_t maxs, clipHandle_t model,
	               const vec3_t origin, int brushmask, qboolean capsule,
	               const sphere_t *sphere );
	int  (*PointContents)( const vec3_t p, clipHandle_t model );
	void (*BiSphereTrace)( trace_t *results, const vec3_t start, const vec3_t end,
	                       float startRad, float endRad, clipHandle_t model,
	                       int brushmask );
} cmTracer_t;

// Registered triangle-soup collision entry. The cmodel_t is used for
// CM_ModelBounds responses; the patchCollide_t holds the actual facet
// geometry and is consumed by CM_TraceThroughPatchCollide.
typedef struct cTriSoup_s {
	qboolean			inUse;
	char				name[MAX_QPATH];
	cmodel_t			cmod;		// bounds only, leaf is unused
	struct patchCollide_s	*pc;
} cTriSoup_t;

typedef struct {
	cplane_t	*plane;
	int			surfaceFlags;
	int			shaderNum;
} cbrushside_t;

typedef struct {
	int			shaderNum;		// the shader that determined the contents
	int			contents;
	vec3_t		bounds[2];
	int			numsides;
	cbrushside_t	*sides;
	int			checkcount;		// to avoid repeated testings
} cbrush_t;


typedef struct {
	int			checkcount;				// to avoid repeated testings
	int			surfaceFlags;
	int			contents;
	struct patchCollide_s	*pc;
} cPatch_t;


typedef struct {
	int			floodnum;
	int			floodvalid;
} cArea_t;

typedef struct {
	char		name[MAX_QPATH];

	int			numShaders;
	dshader_t	*shaders;

	int			numBrushSides;
	cbrushside_t *brushsides;

	int			numPlanes;
	cplane_t	*planes;

	int			numNodes;
	cNode_t		*nodes;

	int			numLeafs;
	cLeaf_t		*leafs;

	int			numLeafBrushes;
	int			*leafbrushes;

	int			numLeafSurfaces;
	int			*leafsurfaces;

	int			numSubModels;
	cmodel_t	*cmodels;

	int			numBrushes;
	cbrush_t	*brushes;

	int			numClusters;
	int			clusterBytes;
	byte		*visibility;
	byte		*novis;		// clusterBytes of 0xff

	int			numEntityChars;
	char		*entityString;

	int			numAreas;
	cArea_t		*areas;
	int			*areaPortals;	// [ numAreas*numAreas ] reference counts

	int			numSurfaces;
	cPatch_t	**surfaces;			// non-patches will be NULL

	// Runtime-registered triangle soups (IQM and other dynamic
	// triangle meshes). Indexed by (handle - TRI_SOUP_HANDLE_BASE).
	cTriSoup_t	triSoups[MAX_TRI_SOUPS];
	int			numTriSoups;

	int			floodvalid;
	int			checkcount;					// incremented on each trace

	unsigned int checksum;

	cmQ1Data_t          q1;
	const cmTracer_t   *tracer;
} clipMap_t;


// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define	SURFACE_CLIP_EPSILON	(0.125)

extern	clipMap_t	cm;
extern	int			c_pointcontents;
extern	int			c_traces, c_brush_traces, c_patch_traces;
extern	cvar_t		*cm_noAreas;
extern	cvar_t		*cm_noCurves;
extern	cvar_t		*cm_playerCurveClip;

extern cmTracer_t cmTracer_q3;
extern cmTracer_t cmTracer_q1;

// cm_q1.c — Q1 hull data management called from bsp_q1.c during load
void CMQ1_StoreClipnodes( const q1_dclipnode_t *cn, int numCn,
                          const int *hull1Roots, const int *hull0Roots, int numSubmodels );
void CMQ1_StoreLeafContents( const int *contents, int numLeafs );
void CMQ1_FreeData( void );

// cm_trace.c — internal Q3 trace (used by cmTracer_q3 vtable entry and Q1 fallback)
void CM_Trace( trace_t *results, const vec3_t start, const vec3_t end,
               const vec3_t mins, const vec3_t maxs, clipHandle_t model,
               const vec3_t origin, int brushmask, qboolean capsule,
               const sphere_t *sphere );

// cm_test.c — Q3 brush-walk point-contents impl (vtable entry for cmTracer_q3)
int CMQ3_PointContents( const vec3_t p, clipHandle_t model );

// cm_trace.c — Q3 bisphere impl (vtable entry for cmTracer_q3)
void CMQ3_BiSphereTrace( trace_t *results, const vec3_t start, const vec3_t end,
                         float startRad, float endRad, clipHandle_t model, int brushmask );

// cm_test.c

// Used for oriented capsule collision detection
typedef struct sphere_s
{
	qboolean	use;
	float		radius;
	float		halfheight;
	vec3_t		offset;
} sphere_t;

typedef struct {
	float	startRadius;
	float	endRadius;
} biSphere_t;

typedef struct {
	vec3_t		start;
	vec3_t		end;
	vec3_t		size[2];	// size of the box being swept through the model
	vec3_t		offsets[8];	// [signbits][x] = either size[0][x] or size[1][x]
	float		maxOffset;	// longest corner length from origin
	vec3_t		extents;	// greatest of abs(size[0]) and abs(size[1])
	vec3_t		bounds[2];	// enclosing box of start and end surrounding by size
	vec3_t		modelOrigin;// origin of the model tracing through
	int			contents;	// ored contents of the model tracing through
	qboolean	isPoint;	// optimized case
	trace_t		trace;		// returned from trace call
	sphere_t	sphere;		// sphere for oriendted capsule collision
	traceType_t	type;		// replaces sphere.use boolean
	biSphere_t	biSphere;	// for TT_BISPHERE
	qboolean	testLateralCollision;
} traceWork_t;

typedef struct leafList_s {
	int		count;
	int		maxcount;
	qboolean	overflowed;
	int		*list;
	vec3_t	bounds[2];
	int		lastLeaf;		// for overflows where each leaf can't be stored individually
	void	(*storeLeafs)( struct leafList_s *ll, int nodenum );
} leafList_t;


int CM_BoxBrushes( const vec3_t mins, const vec3_t maxs, cbrush_t **list, int listsize );

void CM_StoreLeafs( leafList_t *ll, int nodenum );
void CM_StoreBrushes( leafList_t *ll, int nodenum );

void CM_BoxLeafnums_r( leafList_t *ll, int nodenum );

cmodel_t	*CM_ClipHandleToModel( clipHandle_t handle );
qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 );
qboolean CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point );

// cm_patch.c

struct patchCollide_s	*CM_GeneratePatchCollide( int width, int height, vec3_t *points );
struct patchCollide_s	*CM_GenerateTriangleSoupCollide( int numVertexes, vec3_t *vertexes,
	int numIndexes, int *indexes );
void CM_TraceThroughPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc );
qboolean CM_PositionTestInPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc );
void CM_ClearLevelPatches( void );
void CM_TriangleSoupCollideSelfTest( void );

// cm_load.c — runtime triangle soup / IQM registration
clipHandle_t	CM_RegisterTriangleSoup( const char *name, const vec3_t *vertexes,
	int numVertexes, const int *indexes, int numIndexes );
clipHandle_t	CM_LoadIQMGeometry( const char *name );
void			CM_ClearTriangleSoups( void );
