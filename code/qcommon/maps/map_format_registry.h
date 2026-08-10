// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// map_format_registry.h -- pluggable map-format abstraction (FEAT_BSP_ABSTRACTION)
// The registry is format-independent; BSP is one registered format instance.

#ifndef __MAP_FORMAT_REGISTRY_H
#define __MAP_FORMAT_REGISTRY_H

#include "../q_shared.h"
#include "../qfiles.h"

// Maximum registered map formats
#define MAX_MAP_FORMATS 8

// Quake 1 BSP29 — version is the first (and only) u32 in the header; no ident field
#define BSP_VERSION_Q1   29
#define HEADER_LUMPS_Q1  15

// Forward declarations
typedef struct mapFormat_s mapFormat_t;
typedef struct mapFile_s mapFile_t;

// Forward declaration for the nav geometry type.
// Full definition is in code/qcommon/nav/nav_local.h.
// Used by mapFormat_t.extractNavGeometry; bsp.h itself need not know the layout.
struct navGeom_s;

// Forward declaration for the format-neutral mover descriptor.
// Full definition is in code/qcommon/nav/nav_local.h.
// Used (by pointer) in mapFormat_t.extractMovers; the registry need not know the layout.
struct navMoverDesc_s;

typedef enum {
	MAP_ASSET_PROFILE_MODERN = 0,
	MAP_ASSET_PROFILE_LEGACY
} mapAssetProfile_t;

// BSP file - parsed representation of a BSP map
typedef struct mapFile_s {
	char		name[MAX_QPATH];
	int			ident;
	int			version;
	int			references;

	// Format that loaded this file.  Set by Map_Load after successful load.
	// NULL only if the file was loaded before this field was added.
	const mapFormat_t *format;

	// Flags this entry was PARSED with. Consulted on every cache lookup: a cached
	// entry may only answer a request whose needs it already satisfied.
	// MAP_LOAD_FLAG_RENDER_ONLY is a RESTRICTION — it suppresses the parser's
	// collision side-effects — so an entry parsed WITH it cannot serve a request
	// made WITHOUT it, because those side-effects would be silently skipped. The
	// reverse is a superset and takes the fast path unchanged.
	unsigned	loadFlags;

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

} mapFile_t;

// Flags for Map_Load.
#define MAP_LOAD_FLAGS_NONE        0u
// Skip collision-system side-effects (CMQ1_StoreClipnodes / CMQ1_StoreLeafContents).
// Set by callers that load a BSP purely for rendering or asset extraction and must not
// overwrite the live world collision tree.
#define MAP_LOAD_FLAG_RENDER_ONLY  (1u << 0)

// Map-format loader interface (one instance per registered format, e.g. BSP)
typedef struct mapFormat_s {
	const char	*name;			// e.g. "Quake 3"
	int			ident;			// magic number (e.g. BSP_IDENT)
	int			version;		// BSP version (e.g. BSP_VERSION)
	// Returns qtrue if this format owns the given raw buffer.
	// Must not be NULL — asserted in Map_RegisterFormat.
	qboolean	(*detect)( const void *buf, int len );
	qboolean	(*loadFunction)( const mapFormat_t *format, const char *name,
					const void *data, int length, unsigned flags, mapFile_t **bspFile );
	// NULL if this format does not support nav geometry extraction.
	// When NULL, Nav_LoadMap logs a warning and nav.ready stays qfalse.
	qboolean	(*extractNavGeometry)( const mapFile_t *bsp,
	                                   struct navGeom_s *outGeom );
	// Fill out[] with up to maxOut format-neutral vertical-mover descriptors
	// (elevators/platforms) recognized from THIS format's own mover classnames +
	// geometry; return the count written. The nav OMC builder consumes the
	// descriptors without knowing any classname or reading any entity origin key —
	// all format divergence lives here in the adapter. NULL if the format has no
	// mover extraction (the OMC builder then emits no platform connections).
	int		(*extractMovers)( const mapFile_t *bsp,
	                          struct navMoverDesc_s *out, int maxOut );
	// Collision tracer this format's maps use, declared by the format
	// (e.g. &cmTracer_q3). Opaque here (const void *) — the maps layer carries
	// the pointer but never dereferences it; the collision module (cm_load)
	// casts it back to cmTracer_t and installs it. A pointer, so many formats
	// may share one tracer instance (N:1). Must be non-NULL (cm_load asserts).
	const void	*tracer;
} mapFormat_t;

// .lit sidecar loader — shared by Q1 and Q3 paths
#define LIT_MAGIC    ( ('Q') | ('L' << 8) | ('I' << 16) | ('T' << 24) )
#define LIT_VERSION  1
// Load a .lit file and return a Z_Malloc'd RGB buffer of expectedRGBBytes, or NULL.
byte		*Lit_TryLoad( const char *litPath, int expectedRGBBytes );

// Public API
void		Map_Init( void );
void		Map_RegisterFormat( const mapFormat_t *format );
qboolean	Map_Load( const char *name, mapFile_t **bspFile, unsigned flags );
void		Map_Free( mapFile_t *bspFile );
void		Map_ClearMapCache( void );
void		Map_Shutdown( void );
mapAssetProfile_t Map_AssetProfileForVersion( int version );
const char	*Map_DefaultSoundExtForProfile( mapAssetProfile_t profile );
const char	*Map_DefaultImageExtForProfile( mapAssetProfile_t profile );

#endif // __MAP_FORMAT_REGISTRY_H
