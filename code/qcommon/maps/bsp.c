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

// bsp.c -- BSP format registry and loading (FEAT_BSP_ABSTRACTION)

#include "../q_shared.h"
#include "../qcommon.h"
#include "bsp.h"

static bspFormat_t const *bspFormats[MAX_BSP_FORMATS];
static int numBspFormats = 0;

#define MAX_BSP_FILES 2
static bspFile_t *bspLoadedFiles[MAX_BSP_FILES];

void BSP_Init( void ) {
	numBspFormats = 0;
	memset( bspFormats, 0, sizeof( bspFormats ) );

	// Register built-in formats
	extern const bspFormat_t bspFormatQ3;
	BSP_RegisterFormat( &bspFormatQ3 );
	memset( bspLoadedFiles, 0, sizeof( bspLoadedFiles ) );
}

void BSP_RegisterFormat( const bspFormat_t *format ) {
	if ( numBspFormats >= MAX_BSP_FORMATS ) {
		Com_Error( ERR_FATAL, "BSP_RegisterFormat: too many formats" );
	}
	bspFormats[numBspFormats++] = format;
}

qboolean BSP_Load( const char *name, bspFile_t **bspFile ) {
	void		*buf;
	int			length;
	int			ident;
	int			version;
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
		Com_Error( ERR_DROP, "%s: no free BSP slots for '%s'", __func__, name );
	}

	length = FS_ReadFile( name, &buf );
	if ( !buf ) {
		return qfalse;
	}

	if ( length < 8 ) {
		FS_FreeFile( buf );
		return qfalse;
	}

	ident = LittleLong( ((int *)buf)[0] );
	version = LittleLong( ((int *)buf)[1] );

	for ( i = 0; i < numBspFormats; i++ ) {
		if ( bspFormats[i]->ident == ident && bspFormats[i]->version == version ) {
			qboolean result = bspFormats[i]->loadFunction( bspFormats[i], name, buf, length, bspFile );
			FS_FreeFile( buf );
			if ( result && *bspFile ) {
				(*bspFile)->ident = ident;
				(*bspFile)->version = version;
				(*bspFile)->references = 1;
				(*bspFile)->format = bspFormats[i];
				bspLoadedFiles[freeSlot] = *bspFile;
			}
			return result;
		}
	}

	Com_Printf( "BSP_Load: %s has unrecognized format (ident=%d, version=%d)\n", name, ident, version );
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
	if ( bspFile->rawData ) {
		Z_Free( bspFile->rawData );
	}
	if ( bspFile->fogs ) {
		Z_Free( bspFile->fogs );
	}

	Z_Free( bspFile );
}

void BSP_Shutdown( void ) {
	int i;

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
