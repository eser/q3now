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

// bsp.c -- BSP format registry and loading (FEAT_BSP_ABSTRACTION)

#include "../q_shared.h"
#include "../qcommon.h"
#include "bsp.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_loading, "loading" );

static bspFormat_t const *bspFormats[MAX_BSP_FORMATS];
static int numBspFormats = 0;

#define MAX_BSP_FILES 2
static bspFile_t *bspLoadedFiles[MAX_BSP_FILES];

// Persistent cache reference: keeps the current map's bspFile_t alive across
// all load-and-free callers within one map cycle. Released by BSP_ClearMapCache.
static bspFile_t *s_mapCacheBsp = NULL;

void BSP_Init( void ) {
	numBspFormats = 0;
	memset( bspFormats, 0, sizeof( bspFormats ) );

	// Register built-in formats. Q3 first: detect runs in registration order,
	// so Q3's magic-byte check fires before Q1's version-only check.
	extern const bspFormat_t bspFormatQ3;
	extern const bspFormat_t bspFormatQ1;
	BSP_RegisterFormat( &bspFormatQ3 );
	BSP_RegisterFormat( &bspFormatQ1 );
	memset( bspLoadedFiles, 0, sizeof( bspLoadedFiles ) );
}

void BSP_RegisterFormat( const bspFormat_t *format ) {
	if ( numBspFormats >= MAX_BSP_FORMATS ) {
		Com_Terminate( TERM_UNRECOVERABLE, "BSP_RegisterFormat: too many formats" );
	}
	assert( format->detect != NULL );
	assert( format->loadFunction != NULL );
	bspFormats[numBspFormats++] = format;
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

qboolean BSP_Load( const char *name, bspFile_t **bspFile, unsigned flags ) {
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
			bspLoadedFiles[i]->references++;
			*bspFile = bspLoadedFiles[i];
			return qtrue;
		}
	}

	if ( freeSlot < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: no free BSP slots for '%s'", __func__, name );
	}

	length = FS_ReadFile( name, &buf );
	if ( !buf ) {
		return qfalse;
	}

	if ( length < 8 ) {
		FS_FreeFile( buf );
		return qfalse;
	}

	for ( i = 0; i < numBspFormats; i++ ) {
		if ( bspFormats[i]->detect( buf, length ) ) {
			qboolean result = bspFormats[i]->loadFunction( bspFormats[i], name, buf, length, flags, bspFile );
			FS_FreeFile( buf );
			if ( result && *bspFile ) {
				(*bspFile)->references = 1;
				(*bspFile)->format = bspFormats[i];
				bspLoadedFiles[freeSlot] = *bspFile;
				// Release the previous map-cycle cache reference before acquiring
				// the new one. Without this, every different-map fresh load leaks
				// one reference to the previous BSP, eventually exhausting the slot pool.
				BSP_ClearMapCache();
				s_mapCacheBsp = *bspFile;
				(*bspFile)->references++;
			}
			return result;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_loading), "BSP_Load: %s has no matching format\n", name );
	FS_FreeFile( buf );
	return qfalse;
}

void BSP_Free( bspFile_t *bspFile ) {
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
	// allocated via Z_Malloc and owned by this bspFile_t. Release each
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

void BSP_ClearMapCache( void ) {
	if ( !s_mapCacheBsp )
		return;
	BSP_Free( s_mapCacheBsp );
	s_mapCacheBsp = NULL;
}

void BSP_Shutdown( void ) {
	int i;

	BSP_ClearMapCache();

	for ( i = 0; i < MAX_BSP_FILES; i++ ) {
		if ( !bspLoadedFiles[i] ) {
			continue;
		}

		bspLoadedFiles[i]->references = 1;
		BSP_Free( bspLoadedFiles[i] );
		bspLoadedFiles[i] = NULL;
	}
}

bspAssetProfile_t BSP_AssetProfileForVersion( int version ) {
	if ( version <= BSP_VERSION || version == 68 ) {
		return BSP_ASSET_PROFILE_LEGACY;
	}

	return BSP_ASSET_PROFILE_MODERN;
}

const char *BSP_DefaultSoundExtForProfile( bspAssetProfile_t profile ) {
	if ( profile == BSP_ASSET_PROFILE_LEGACY ) {
		return "wav";
	}

	return "opus";
}

const char *BSP_DefaultImageExtForProfile( bspAssetProfile_t profile ) {
	if ( profile == BSP_ASSET_PROFILE_LEGACY ) {
		return "tga";
	}

	return "png";
}
