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
// cl_mapinfo.c -- parse .meta sidecar files and extract worldspawn entity data

#include "client.h"
#include "../qcommon/qfiles.h"

clMapInfo_t cl_mapInfo;

/*
================
CL_ClearMapInfo
================
*/
void CL_ClearMapInfo( void ) {
	memset( &cl_mapInfo, 0, sizeof( cl_mapInfo ) );
}

/*
================
CL_MapInfo_ParseMeta

Parse a .meta sidecar file with key=value lines.
Lines starting with '#' or empty lines are skipped.
================
*/
static void CL_MapInfo_ParseMeta( const char *text, int len ) {
	const char *p, *end;
	char line[1024];
	char key[256];
	char value[768];

	if ( !text || len <= 0 ) {
		return;
	}

	p = text;
	end = text + len;

	while ( p < end ) {
		int lineLen = 0;

		// Read one line
		while ( p < end && *p != '\n' && *p != '\r' ) {
			if ( lineLen < (int)(sizeof(line) - 1) ) {
				line[lineLen++] = *p;
			}
			p++;
		}
		line[lineLen] = '\0';

		// Skip line endings
		while ( p < end && ( *p == '\n' || *p == '\r' ) ) {
			p++;
		}

		// Skip empty lines and comments
		if ( lineLen == 0 || line[0] == '#' ) {
			continue;
		}

		// Find the '=' separator
		{
			char *eq = strchr( line, '=' );
			int keyLen, valLen;

			if ( !eq ) {
				continue;
			}

			keyLen = (int)(eq - line);
			if ( keyLen <= 0 || keyLen >= (int)sizeof(key) ) {
				continue;
			}
			memcpy( key, line, keyLen );
			key[keyLen] = '\0';

			valLen = lineLen - keyLen - 1;
			if ( valLen < 0 ) {
				valLen = 0;
			}
			if ( valLen >= (int)sizeof(value) ) {
				valLen = (int)sizeof(value) - 1;
			}
			if ( valLen > 0 ) {
				memcpy( value, eq + 1, valLen );
			}
			value[valLen] = '\0';
		}

		// Strip leading/trailing whitespace from key
		{
			char *s = key;
			while ( *s == ' ' || *s == '\t' ) s++;
			if ( s != key ) {
				memmove( key, s, strlen(s) + 1 );
			}
			{
				int kl = (int)strlen(key);
				while ( kl > 0 && ( key[kl-1] == ' ' || key[kl-1] == '\t' ) ) {
					key[--kl] = '\0';
				}
			}
		}

		// Strip leading/trailing whitespace from value
		{
			char *s = value;
			while ( *s == ' ' || *s == '\t' ) s++;
			if ( s != value ) {
				memmove( value, s, strlen(s) + 1 );
			}
			{
				int vl = (int)strlen(value);
				while ( vl > 0 && ( value[vl-1] == ' ' || value[vl-1] == '\t' ) ) {
					value[--vl] = '\0';
				}
			}
		}

		// Match known keys
		if ( !Q_stricmp( key, "long_name" ) ) {
			Q_strncpyz( cl_mapInfo.longName, value, sizeof( cl_mapInfo.longName ) );
		} else if ( !Q_stricmp( key, "series" ) ) {
			Q_strncpyz( cl_mapInfo.series, value, sizeof( cl_mapInfo.series ) );
		} else if ( !Q_stricmp( key, "archetype" ) ) {
			Q_strncpyz( cl_mapInfo.archetype, value, sizeof( cl_mapInfo.archetype ) );
		} else if ( !Q_stricmp( key, "players_min" ) ) {
			cl_mapInfo.playersMin = atoi( value );
		} else if ( !Q_stricmp( key, "players_max" ) ) {
			cl_mapInfo.playersMax = atoi( value );
		} else if ( !Q_stricmp( key, "meta_weapon" ) ) {
			Q_strncpyz( cl_mapInfo.metaWeapon, value, sizeof( cl_mapInfo.metaWeapon ) );
		} else if ( !Q_stricmp( key, "item_nodes" ) ) {
			cl_mapInfo.itemNodes = atoi( value );
		} else if ( !Q_stricmp( key, "quote" ) ) {
			Q_strncpyz( cl_mapInfo.quote, value, sizeof( cl_mapInfo.quote ) );
		} else if ( !Q_stricmp( key, "author" ) ) {
			Q_strncpyz( cl_mapInfo.author, value, sizeof( cl_mapInfo.author ) );
		} else if ( !Q_stricmp( key, "year" ) ) {
			Q_strncpyz( cl_mapInfo.year, value, sizeof( cl_mapInfo.year ) );
		} else if ( !Q_stricmp( key, "sky" ) ) {
			Q_strncpyz( cl_mapInfo.sky, value, sizeof( cl_mapInfo.sky ) );
		}
	}

	cl_mapInfo.hasMetaFile = qtrue;
}

/*
================
CL_MapInfo_ParseWorldspawn

Parse the BSP entity lump to extract worldspawn keys
(message, sky, author) as a fallback when no .meta file exists.
================
*/
static void CL_MapInfo_ParseWorldspawn( const char *entityString, int entityLen ) {
	const char *p;
	char key[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];

	if ( !entityString || entityLen <= 0 ) {
		return;
	}

	p = entityString;

	// Skip whitespace to first entity
	while ( *p && ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) ) {
		p++;
	}

	// Expect opening brace for worldspawn (first entity)
	if ( *p != '{' ) {
		return;
	}
	p++;

	// Parse key-value pairs of worldspawn only
	while ( 1 ) {
		// Skip whitespace
		while ( *p && ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) ) {
			p++;
		}
		if ( !*p ) break;

		// Closing brace ends worldspawn
		if ( *p == '}' ) {
			break;
		}

		// Parse key (quoted string)
		if ( *p != '"' ) {
			p++;
			continue;
		}
		p++; // skip opening quote
		{
			int i = 0;
			while ( *p && *p != '"' && i < (int)(sizeof(key) - 1) ) {
				key[i++] = *p++;
			}
			key[i] = '\0';
		}
		if ( *p == '"' ) p++; // skip closing quote

		// Skip whitespace between key and value
		while ( *p && ( *p == ' ' || *p == '\t' ) ) {
			p++;
		}

		// Parse value (quoted string)
		if ( *p != '"' ) {
			continue;
		}
		p++; // skip opening quote
		{
			int i = 0;
			while ( *p && *p != '"' && i < (int)(sizeof(value) - 1) ) {
				value[i++] = *p++;
			}
			value[i] = '\0';
		}
		if ( *p == '"' ) p++; // skip closing quote

		// Extract worldspawn keys we care about
		if ( !Q_stricmp( key, "message" ) ) {
			Q_strncpyz( cl_mapInfo.longName, value, sizeof( cl_mapInfo.longName ) );
		} else if ( !Q_stricmp( key, "sky" ) || !Q_stricmp( key, "_sky" ) ||
		            !Q_stricmp( key, "skybox" ) ) {
			Q_strncpyz( cl_mapInfo.sky, value, sizeof( cl_mapInfo.sky ) );
		} else if ( !Q_stricmp( key, "author" ) ) {
			Q_strncpyz( cl_mapInfo.author, value, sizeof( cl_mapInfo.author ) );
		}
	}
}

/*
================
CL_LoadMapInfo

Load map metadata from a .meta sidecar file, falling back to
worldspawn entity data from the BSP when no .meta is available.
================
*/
void CL_LoadMapInfo( const char *mapname ) {
	CL_ClearMapInfo();

	if ( !mapname || !mapname[0] ) {
		return;
	}

	Q_strncpyz( cl_mapInfo.mapName, mapname, sizeof( cl_mapInfo.mapName ) );

	// --- Try .meta sidecar file ---
	char path[MAX_QPATH];
	Com_sprintf( path, sizeof( path ), "maps/%s.meta", mapname );
	fileHandle_t f;
	int fileLen = FS_FOpenFileRead( path, &f, qtrue );

	if ( f && fileLen > 0 ) {
		char *text = (char *)Z_Malloc( fileLen + 1 );
		if ( FS_Read( text, fileLen, f ) == fileLen ) {
			text[fileLen] = '\0';
			CL_MapInfo_ParseMeta( text, fileLen );
			Com_DPrintf( "CL_LoadMapInfo: loaded %s\n", path );
		}
		Z_Free( text );
		FS_FCloseFile( f );
	} else {
		if ( f ) {
			FS_FCloseFile( f );
		}
	}

	// --- Fallback: extract from BSP entity lump ---
	if ( !cl_mapInfo.hasMetaFile || cl_mapInfo.longName[0] == '\0' || cl_mapInfo.sky[0] == '\0' ) {
		char bspPath[MAX_QPATH];
		bspFile_t *bsp;

		Com_sprintf( bspPath, sizeof( bspPath ), "maps/%s.bsp", mapname );
		if ( BSP_Load( bspPath, &bsp ) ) {
			if ( bsp->entityString && bsp->entityStringLength > 0 ) {
				CL_MapInfo_ParseWorldspawn( bsp->entityString, bsp->entityStringLength );
			}
			BSP_Free( bsp );
		} else {
			Com_DPrintf( "CL_LoadMapInfo: no BSP found at %s\n", bspPath );
		}
	}

	// --- Count item markers from bspPreview if itemNodes not set ---
	if ( cl_mapInfo.itemNodes == 0 && cl_bspPreview.valid ) {
		int count = 0;
		for ( int i = 0; i < cl_bspPreview.numMarkers; i++ ) {
			if ( cl_bspPreview.markers[i].type == 1 ) { // type 1 = item
				count++;
			}
		}
		cl_mapInfo.itemNodes = count;
	}

	Com_DPrintf( "CL_LoadMapInfo: %s -> longName=\"%s\", author=\"%s\", sky=\"%s\", items=%d, meta=%s\n",
		cl_mapInfo.mapName,
		cl_mapInfo.longName,
		cl_mapInfo.author,
		cl_mapInfo.sky,
		cl_mapInfo.itemNodes,
		cl_mapInfo.hasMetaFile ? "yes" : "no" );
}
