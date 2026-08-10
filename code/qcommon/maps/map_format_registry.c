// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// map_format_registry.c -- map-format registry and loading (FEAT_BSP_ABSTRACTION)
// Format-independent registry/funnel; BSP is one registered format instance.

#include "../q_shared.h"
#include "../qcommon.h"
#include "map_format_registry.h"
LOG_DECLARE_CHANNEL( ch_loading, "loading" );

static mapFormat_t const *mapFormats[MAX_MAP_FORMATS];
static int numMapFormats = 0;

// Sized for the worst-case concurrent live BSPs: one held world BSP per client
// app (app->cgameBsp, kept across frames until the next CL_CM_LoadMap /
// CL_ShutdownLevel) plus one transient slot for an INCOMING map. During a map
// change the OUTGOING map's held ref legitimately overlaps the incoming map's
// preview/mapinfo/CM/nav transient loads, so the pool must hold (held + 1).
// The client is architected for MAX_LOCAL_CGAME_VMS (= 4, vm_local.h) concurrent
// apps (clientApps[], client.h), each owning its own cgameBsp, so the bound is
// MAX_LOCAL_CGAME_VMS + 1 = 5. (Kept as a literal here: the maps layer must not
// depend on the VM-internal vm_local.h. The previous value of 2 was the
// single-app steady-state peak with ZERO margin — a third concurrent BSP, e.g. a
// transient overlapping two distinct held maps mid-transition, exhausted it with
// "no free BSP slots".)
#define MAX_BSP_FILES 5
static mapFile_t *bspLoadedFiles[MAX_BSP_FILES];

// Persistent cache reference: keeps the current map's mapFile_t alive across
// all load-and-free callers within one map cycle. Released by Map_ClearMapCache.
static mapFile_t *s_mapCacheBsp = NULL;

void Map_Init( void ) {
	numMapFormats = 0;
	memset( mapFormats, 0, sizeof( mapFormats ) );

	// Register built-in formats. Q3 first: detect runs in registration order,
	// so Q3's magic-byte check fires before Q1's version-only check.
	extern const mapFormat_t bspFormatQ3;
	extern const mapFormat_t bspFormatQ1;
	Map_RegisterFormat( &bspFormatQ3 );
	Map_RegisterFormat( &bspFormatQ1 );
	memset( bspLoadedFiles, 0, sizeof( bspLoadedFiles ) );
}

void Map_RegisterFormat( const mapFormat_t *format ) {
	if ( numMapFormats >= MAX_MAP_FORMATS ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Map_RegisterFormat: too many formats" );
	}
	assert( format->detect != NULL );
	assert( format->loadFunction != NULL );
	mapFormats[numMapFormats++] = format;
}

byte *Lit_TryLoad( const char *litPath, int expectedRGBBytes ) {
	void *litBuf = NULL;
	int   litLen;
	byte *out;

	litLen = FS_ReadFile( litPath, &litBuf );
	if ( !litBuf )
		return NULL;

	if ( litLen < 8
	  || LittleLong( ((const int *)litBuf)[0] ) != LIT_MAGIC
	  || LittleLong( ((const int *)litBuf)[1] ) != LIT_VERSION
	  || litLen - 8 != expectedRGBBytes ) {
		if ( litLen >= 8 )
			Com_Log( SEV_WARN, LOG_CH(ch_loading), "Lit_TryLoad: %s rejected (data=%d expected=%d)\n",
			         litPath, litLen - 8, expectedRGBBytes );
		FS_FreeFile( litBuf );
		return NULL;
	}

	out = Z_Malloc( expectedRGBBytes );
	memcpy( out, (const byte *)litBuf + 8, expectedRGBBytes );
	FS_FreeFile( litBuf );
	Com_Log( SEV_INFO, LOG_CH(ch_loading), "Loaded .lit colored lighting: %s (%d texels)\n",
	         litPath, expectedRGBBytes / 3 );
	return out;
}

// Read a whole file into a zone (Z_Malloc) buffer instead of a Hunk temp.
// The .bsp file buffer is held across the entire map load — CM collision build
// and the wasm game-VM instantiation, which churns the shared Hunk temp stack
// with thousands of allocations. A buffer read via FS_ReadFile lives on that
// temp stack and its header region gets reused by the churn, corrupting it
// before it is freed. Reading into the zone keeps the long-lived buffer off
// the temp stack entirely, so the churn can never touch it. Uses only the
// public FS primitives (FOpenFileRead/Read/FCloseFile) — FS_ReadFile and the
// fs_loadStack temp protocol stay intact for every other caller. Mirrors
// FS_ReadFile's trailing-'\0' guarantee. Returns the length and sets *buffer
// to a Z_Malloc'd buffer the caller must Z_Free, or -1 (and *buffer=NULL) on
// failure.
static int BSP_ReadFileZone( const char *name, void **buffer ) {
	fileHandle_t h;
	byte        *buf;
	int          len;

	*buffer = NULL;
	len = FS_FOpenFileRead( name, &h, qfalse );
	if ( h == FS_INVALID_HANDLE ) {
		return -1;
	}
	if ( len < 0 ) {
		FS_FCloseFile( h );
		return -1;
	}

	buf = Z_Malloc( len + 1 );
	if ( FS_Read( buf, len, h ) != len ) {
		Z_Free( buf );
		FS_FCloseFile( h );
		return -1;
	}
	FS_FCloseFile( h );

	buf[len] = '\0';   // trailing 0 for string ops, matching FS_ReadFile
	*buffer = buf;
	return len;
}

/*
================
Map_ValidateNeutral

Shared, format-independent post-parse validation of the neutral mapFile_t. The
invariant "every leaf-surface index is in range" is the same for Q3, Q1, and any
future format — so it is enforced ONCE here at the funnel, over the neutral
struct, instead of being (re)duplicated and independently forgotten in each
format loader. Consumers of the neutral array (collision, nav) can then trust it.

House policy is REJECT, not clamp: a clamp would silently mask a corrupt map.
A bad index terminates the load (matching the consumers' existing "bad surface"
hard-fail), naming the map and the offending value.

NOTE: this validates the NEUTRAL array only. The renderer re-parses the raw Q3
lumps directly (not this struct), so it keeps its own check — see R_LoadMarksurfaces.
================
*/
static void Map_ValidateNeutral( const mapFile_t *map ) {
	int i;

	// every leaf-surface index must reference a real surface. Unsigned compare:
	// a corrupt sentinel is 0xFFFFFFFF (Q3) or a 16-bit 0xFFFF widened to int
	// (Q1) — both read as a huge unsigned, caught here; a signed compare would
	// see them as -1 and miss the bound.
	for ( i = 0; i < map->numLeafSurfaces; i++ ) {
		if ( (unsigned)map->leafSurfaces[i] >= (unsigned)map->numSurfaces ) {
			Com_Terminate( TERM_CLIENT_DROP,
				"%s: '%s' has bad leaf-surface index %u (numSurfaces %d)",
				__func__, map->name, (unsigned)map->leafSurfaces[i], map->numSurfaces );
		}
	}

	// every leaf's [firstLeafSurface, +numLeafSurfaces) range must lie within the
	// leaf-surface array (mirrors the consumers' per-leaf range check). uint64_t
	// add so a huge firstLeafSurface + numLeafSurfaces cannot wrap past the bound.
	for ( i = 0; i < map->numLeafs; i++ ) {
		uint64_t first = (uint64_t)(unsigned)map->leafs[i].firstLeafSurface;
		uint64_t num   = (uint64_t)(unsigned)map->leafs[i].numLeafSurfaces;
		if ( first + num > (uint64_t)(unsigned)map->numLeafSurfaces ) {
			Com_Terminate( TERM_CLIENT_DROP,
				"%s: '%s' leaf %d has bad leaf-surface range (%llu+%llu > %d)",
				__func__, map->name, i, (unsigned long long)first,
				(unsigned long long)num, map->numLeafSurfaces );
		}
	}
}

qboolean Map_Load( const char *name, mapFile_t **bspFile, unsigned flags ) {
	void		*buf;
	int			length;
	int			i;
	int			freeSlot;

	*bspFile = NULL;
	freeSlot = -1;

	for ( i = 0; i < MAX_BSP_FILES; i++ ) {
		if ( !bspLoadedFiles[i] ) {
			if ( freeSlot < 0 ) {
				freeSlot = i;
			}
			continue;
		}

		if ( !Q_stricmp( bspLoadedFiles[i]->name, name ) ) {
			// A cached entry may only answer a request whose needs it already met.
			// RENDER_ONLY is a restriction: the parser skips its collision
			// side-effects under it (map_q1_bsp.c gates CMQ1_StoreLeafContents and
			// CMQ1_StoreClipnodes on exactly this flag), so an entry parsed WITH it
			// does not satisfy a request made WITHOUT it. Serving one anyway is how a
			// metadata read silently starved the server's canonical collision build.
			// Fall through to a fresh parse for that case only; the superset case
			// (a RENDER_ONLY request hitting a fully-parsed entry) still hits the
			// fast path, as does every same-flags repeat.
			if ( ( bspLoadedFiles[i]->loadFlags & ~flags ) == 0 ) {
				bspLoadedFiles[i]->references++;
				*bspFile = bspLoadedFiles[i];
				return qtrue;
			}
			continue;
		}
	}

	if ( freeSlot < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: no free BSP slots for '%s'", __func__, name );
	}

	// Zone-allocate (not a Hunk temp): this buffer is held across the whole
	// map load while the wasm game VMs instantiate and churn the shared temp
	// stack — a temp buffer here gets its header reused mid-load. See
	// BSP_ReadFileZone above.
	length = BSP_ReadFileZone( name, &buf );
	if ( !buf ) {
		return qfalse;
	}

	if ( length < 8 ) {
		Z_Free( buf );
		return qfalse;
	}

	for ( i = 0; i < numMapFormats; i++ ) {
		if ( mapFormats[i]->detect( buf, length ) ) {
			qboolean result = mapFormats[i]->loadFunction( mapFormats[i], name, buf, length, flags, bspFile );
			Z_Free( buf );
			if ( result && *bspFile ) {
				// shared, format-independent validation of the parsed neutral
				// struct (rejects bad leaf-surface indices for every format)
				Map_ValidateNeutral( *bspFile );

				(*bspFile)->references = 1;
				(*bspFile)->format = mapFormats[i];
				(*bspFile)->loadFlags = flags;
				bspLoadedFiles[freeSlot] = *bspFile;
				// Release the previous map-cycle cache reference before acquiring
				// the new one. Without this, every different-map fresh load leaks
				// one reference to the previous BSP, eventually exhausting the slot pool.
				Map_ClearMapCache();
				s_mapCacheBsp = *bspFile;
				(*bspFile)->references++;
			}
			return result;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_loading), "Map_Load: %s has no matching format\n", name );
	Z_Free( buf );
	return qfalse;
}

void Map_Free( mapFile_t *bspFile ) {
	int i;

	if ( !bspFile ) {
		return;
	}

	bspFile->references--;
	if ( bspFile->references > 0 ) {
		return;
	}

	for ( i = 0; i < MAX_BSP_FILES; i++ ) {
		if ( bspLoadedFiles[i] == bspFile ) {
			bspLoadedFiles[i] = NULL;
			break;
		}
	}

	// Every lump buffer populated by a format loader (e.g. bsp_q3.c) is
	// allocated via Z_Malloc and owned by this mapFile_t. Release each
	// buffer we allocated, then the top-level struct.
	if ( bspFile->entityString ) {
		Z_Free( bspFile->entityString );
	}
	if ( bspFile->shaders ) {
		Z_Free( bspFile->shaders );
	}
	if ( bspFile->planes ) {
		Z_Free( bspFile->planes );
	}
	if ( bspFile->nodes ) {
		Z_Free( bspFile->nodes );
	}
	if ( bspFile->leafs ) {
		Z_Free( bspFile->leafs );
	}
	if ( bspFile->leafSurfaces ) {
		Z_Free( bspFile->leafSurfaces );
	}
	if ( bspFile->leafBrushes ) {
		Z_Free( bspFile->leafBrushes );
	}
	if ( bspFile->subModels ) {
		Z_Free( bspFile->subModels );
	}
	if ( bspFile->brushes ) {
		Z_Free( bspFile->brushes );
	}
	if ( bspFile->brushSides ) {
		Z_Free( bspFile->brushSides );
	}
	if ( bspFile->surfaces ) {
		Z_Free( bspFile->surfaces );
	}
	if ( bspFile->drawVerts ) {
		Z_Free( bspFile->drawVerts );
	}
	if ( bspFile->drawIndexes ) {
		Z_Free( bspFile->drawIndexes );
	}
	if ( bspFile->visibility ) {
		Z_Free( bspFile->visibility );
	}
	if ( bspFile->lightmapData ) {
		Z_Free( bspFile->lightmapData );
	}
	if ( bspFile->lightGridData ) {
		Z_Free( bspFile->lightGridData );
	}
	if ( bspFile->embeddedTextures ) {
		Z_Free( bspFile->embeddedTextures );
	}
	for ( i = 0; i < 4; i++ ) {
		if ( bspFile->styledLightmapData[i] ) {
			Z_Free( bspFile->styledLightmapData[i] );
		}
	}
	if ( bspFile->rawData ) {
		Z_Free( bspFile->rawData );
	}
	if ( bspFile->fogs ) {
		Z_Free( bspFile->fogs );
	}
	if ( bspFile->drawVertLightstyles ) {
		Z_Free( bspFile->drawVertLightstyles );
	}

	Z_Free( bspFile );
}

void Map_ClearMapCache( void ) {
	if ( !s_mapCacheBsp )
		return;
	Map_Free( s_mapCacheBsp );
	s_mapCacheBsp = NULL;
}

void Map_Shutdown( void ) {
	int i;

	Map_ClearMapCache();

	for ( i = 0; i < MAX_BSP_FILES; i++ ) {
		if ( !bspLoadedFiles[i] ) {
			continue;
		}

		bspLoadedFiles[i]->references = 1;
		Map_Free( bspLoadedFiles[i] );
		bspLoadedFiles[i] = NULL;
	}
}

mapAssetProfile_t Map_AssetProfileForVersion( int version ) {
	if ( version <= BSP_VERSION || version == 68 ) {
		return MAP_ASSET_PROFILE_LEGACY;
	}

	return MAP_ASSET_PROFILE_MODERN;
}

const char *Map_DefaultSoundExtForProfile( mapAssetProfile_t profile ) {
	if ( profile == MAP_ASSET_PROFILE_LEGACY ) {
		return "wav";
	}

	return "opus";
}

const char *Map_DefaultImageExtForProfile( mapAssetProfile_t profile ) {
	if ( profile == MAP_ASSET_PROFILE_LEGACY ) {
		return "tga";
	}

	return "png";
}
