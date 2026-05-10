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
// cl_bsp_preview.c -- extract simplified 2D wireframe from BSP for loading screen

#include "client.h"
#include "../qcommon/qfiles.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_client, "client" );

bspPreview_t cl_bspPreview;

/*
================
CL_ClearBspPreview
================
*/
void CL_ClearBspPreview( void ) {
	memset( &cl_bspPreview, 0, sizeof( cl_bspPreview ) );
}

/*
================
CL_BspPreview_AddEdge
================
*/
static void CL_BspPreview_AddEdge( float x1, float y1, float z1,
								   float x2, float y2, float z2, int type ) {
	bspPreviewEdge_t *edge;

	if ( cl_bspPreview.numEdges >= BSP_PREVIEW_MAX_EDGES ) {
		return;
	}

	edge = &cl_bspPreview.edges[cl_bspPreview.numEdges];
	edge->x1 = x1;
	edge->y1 = y1;
	edge->z1 = z1;
	edge->x2 = x2;
	edge->y2 = y2;
	edge->z2 = z2;
	edge->type = type;
	cl_bspPreview.numEdges++;
}

/*
================
CL_BspPreview_AddMarker
================
*/
static void CL_BspPreview_AddMarker( float x, float y, int type ) {
	bspPreviewMarker_t *marker;

	if ( cl_bspPreview.numMarkers >= BSP_PREVIEW_MAX_MARKERS ) {
		return;
	}

	marker = &cl_bspPreview.markers[cl_bspPreview.numMarkers];
	marker->x = x;
	marker->y = y;
	marker->type = type;
	cl_bspPreview.numMarkers++;
}

/*
================
CL_BspPreview_UpdateBounds
================
*/
static void CL_BspPreview_UpdateBounds( float x, float y ) {
	if ( cl_bspPreview.numEdges == 1 && cl_bspPreview.numMarkers == 0 ) {
		// First point — initialize bounds
		cl_bspPreview.minX = x;
		cl_bspPreview.maxX = x;
		cl_bspPreview.minY = y;
		cl_bspPreview.maxY = y;
	} else {
		if ( x < cl_bspPreview.minX ) cl_bspPreview.minX = x;
		if ( x > cl_bspPreview.maxX ) cl_bspPreview.maxX = x;
		if ( y < cl_bspPreview.minY ) cl_bspPreview.minY = y;
		if ( y > cl_bspPreview.maxY ) cl_bspPreview.maxY = y;
	}
}

/*
================
CL_BspPreview_ParseEntities

Parse the entity text lump for spawn points, items, and flags.
Entity format: { "key" "value" ... }
================
*/
static void CL_BspPreview_ParseEntities( const char *entityString, int entityLen ) {
	const char *p;
	char key[MAX_VALUE];
	char value[MAX_VALUE];
	char classname[MAX_VALUE];
	float origin[3];
	qboolean hasOrigin;
	int markerType;

	if ( !entityString || entityLen <= 0 ) {
		return;
	}

	p = entityString;

	while ( p && *p ) {
		// Skip whitespace
		while ( *p && ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) ) {
			p++;
		}
		if ( !*p ) break;

		// Expect opening brace
		if ( *p != '{' ) {
			break;
		}
		p++;

		classname[0] = '\0';
		origin[0] = origin[1] = origin[2] = 0.0f;
		hasOrigin = qfalse;

		// Parse key-value pairs
		while ( 1 ) {
			// Skip whitespace
			while ( *p && ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) ) {
				p++;
			}
			if ( !*p ) break;

			// Check for closing brace
			if ( *p == '}' ) {
				p++;
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

			// Process key-value pair
			if ( !Q_stricmp( key, "classname" ) ) {
				Q_strncpyz( classname, value, sizeof( classname ) );
			} else if ( !Q_stricmp( key, "origin" ) ) {
				if ( sscanf( value, "%f %f %f", &origin[0], &origin[1], &origin[2] ) == 3 ) {
					hasOrigin = qtrue;
				}
			}
		}

		// Determine marker type from classname
		if ( classname[0] && hasOrigin ) {
			markerType = -1;

			if ( !Q_stricmp( classname, "info_player_deathmatch" ) ||
			     !Q_stricmp( classname, "info_player_start" ) ) {
				markerType = 0; // spawn
			} else if ( !Q_stricmpn( classname, "item_", 5 ) ||
			            !Q_stricmpn( classname, "weapon_", 7 ) ||
			            !Q_stricmpn( classname, "ammo_", 5 ) ||
			            !Q_stricmpn( classname, "holdable_", 9 ) ) {
				markerType = 1; // item
			} else if ( !Q_stricmpn( classname, "team_CTF_", 9 ) ) {
				markerType = 2; // flag
			}

			if ( markerType >= 0 ) {
				CL_BspPreview_AddMarker( origin[0], origin[1], markerType );
			}
		}
	}
}

/*
================
CL_BuildBspPreview

Open the BSP file and extract a simplified 2D wireframe for the
loading screen preview.  Only MST_PLANAR surfaces are included.
================
*/
void CL_BuildBspPreview( const char *mapname ) {
	CL_ClearBspPreview();

	if ( !mapname || !mapname[0] ) {
		return;
	}

	// Build BSP path
	char bspPath[MAX_QPATH];
	Com_sprintf( bspPath, sizeof( bspPath ), "maps/%s.bsp", mapname );

	bspFile_t *bsp;
	if ( !BSP_Load( bspPath, &bsp, BSP_LOAD_FLAG_RENDER_ONLY ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildBspPreview: could not open %s\n", bspPath );
		return;
	}

	drawVert_t *drawVerts = bsp->drawVerts;
	int numDrawVerts = bsp->numDrawVerts;
	dsurface_t *surfaces = bsp->surfaces;
	int numSurfaces = bsp->numSurfaces;
	const char *entityString = bsp->entityString;

	if ( numDrawVerts <= 0 || !drawVerts ) {
		BSP_Free( bsp );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildBspPreview: no draw verts in %s\n", bspPath );
		return;
	}

	if ( numSurfaces <= 0 || !surfaces ) {
		BSP_Free( bsp );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildBspPreview: no surfaces in %s\n", bspPath );
		return;
	}

	// --- Pass 1: Compute total XY bounds across all planar surfaces ---
	qboolean boundsInit = qfalse;
	cl_bspPreview.numSurfaces = 0;
	{
		float totalMinX = 0, totalMinY = 0, totalMaxX = 0, totalMaxY = 0;
		float totalRangeX, totalRangeY;

		for ( int i = 0; i < numSurfaces; i++ ) {
			int firstVert, numVerts;
			if ( surfaces[i].surfaceType != MST_PLANAR ) continue;
			firstVert = surfaces[i].firstVert;
			numVerts = surfaces[i].numVerts;
			if ( numVerts < 3 || firstVert < 0 || firstVert + numVerts > numDrawVerts ) continue;
			for ( int j = 0; j < numVerts; j++ ) {
				float vx = drawVerts[firstVert + j].xyz[0];
				float vy = drawVerts[firstVert + j].xyz[1];
				if ( !boundsInit ) {
					totalMinX = totalMaxX = vx;
					totalMinY = totalMaxY = vy;
					boundsInit = qtrue;
				} else {
					if ( vx < totalMinX ) totalMinX = vx;
					if ( vx > totalMaxX ) totalMaxX = vx;
					if ( vy < totalMinY ) totalMinY = vy;
					if ( vy > totalMaxY ) totalMaxY = vy;
				}
			}
		}
		totalRangeX = totalMaxX - totalMinX;
		totalRangeY = totalMaxY - totalMinY;
		if ( totalRangeX < 1.0f ) totalRangeX = 1.0f;
		if ( totalRangeY < 1.0f ) totalRangeY = 1.0f;

		// --- Pass 2: Build edges, skipping oversized surfaces (skybox/void) ---
		boundsInit = qfalse;

		for ( int i = 0; i < numSurfaces; i++ ) {
			int firstVert, numVerts;
			float sMinX, sMinY, sMaxX, sMaxY;

			if ( surfaces[i].surfaceType != MST_PLANAR ) {
				continue;
			}

			cl_bspPreview.numSurfaces++;

			firstVert = surfaces[i].firstVert;
			numVerts = surfaces[i].numVerts;

			if ( numVerts < 3 ) {
				continue;
			}
			if ( firstVert < 0 || firstVert + numVerts > numDrawVerts ) {
				continue;
			}

			// Compute this surface's XY extent
			sMinX = sMaxX = drawVerts[firstVert].xyz[0];
			sMinY = sMaxY = drawVerts[firstVert].xyz[1];
			for ( int j = 1; j < numVerts; j++ ) {
				float vx = drawVerts[firstVert + j].xyz[0];
				float vy = drawVerts[firstVert + j].xyz[1];
				if ( vx < sMinX ) sMinX = vx;
				if ( vx > sMaxX ) sMaxX = vx;
				if ( vy < sMinY ) sMinY = vy;
				if ( vy > sMaxY ) sMaxY = vy;
			}

			// Skip surfaces spanning >50% of the total map in both axes
			// (skybox/void brushes)
			if ( ( sMaxX - sMinX ) > totalRangeX * 0.5f &&
				 ( sMaxY - sMinY ) > totalRangeY * 0.5f ) {
				continue;
			}

			// Add edges between consecutive vertices (top-down XY, preserve Z)
			for ( int j = 0; j < numVerts; j++ ) {
				int v0 = firstVert + j;
				int v1 = firstVert + ( ( j + 1 ) % numVerts );
				float x1 = drawVerts[v0].xyz[0];
				float y1 = drawVerts[v0].xyz[1];
				float z1 = drawVerts[v0].xyz[2];
				float x2 = drawVerts[v1].xyz[0];
				float y2 = drawVerts[v1].xyz[1];
				float z2 = drawVerts[v1].xyz[2];

				if ( x1 == x2 && y1 == y2 ) {
					continue;
				}

				CL_BspPreview_AddEdge( x1, y1, z1, x2, y2, z2, 1 );

				// Update XY bounding box
				if ( !boundsInit ) {
					cl_bspPreview.minX = x1; cl_bspPreview.maxX = x1;
					cl_bspPreview.minY = y1; cl_bspPreview.maxY = y1;
					cl_bspPreview.minZ = z1; cl_bspPreview.maxZ = z1;
					boundsInit = qtrue;
				}
				if ( x1 < cl_bspPreview.minX ) cl_bspPreview.minX = x1;
				if ( x1 > cl_bspPreview.maxX ) cl_bspPreview.maxX = x1;
				if ( y1 < cl_bspPreview.minY ) cl_bspPreview.minY = y1;
				if ( y1 > cl_bspPreview.maxY ) cl_bspPreview.maxY = y1;
				if ( x2 < cl_bspPreview.minX ) cl_bspPreview.minX = x2;
				if ( x2 > cl_bspPreview.maxX ) cl_bspPreview.maxX = x2;
				if ( y2 < cl_bspPreview.minY ) cl_bspPreview.minY = y2;
				if ( y2 > cl_bspPreview.maxY ) cl_bspPreview.maxY = y2;
				// Update Z range
				if ( z1 < cl_bspPreview.minZ ) cl_bspPreview.minZ = z1;
				if ( z1 > cl_bspPreview.maxZ ) cl_bspPreview.maxZ = z1;
				if ( z2 < cl_bspPreview.minZ ) cl_bspPreview.minZ = z2;
				if ( z2 > cl_bspPreview.maxZ ) cl_bspPreview.maxZ = z2;

				if ( cl_bspPreview.numEdges >= BSP_PREVIEW_MAX_EDGES ) {
					break;
				}
			}

			if ( cl_bspPreview.numEdges >= BSP_PREVIEW_MAX_EDGES ) {
				break;
			}
		}
	}

	// --- Read LUMP_ENTITIES ---
	if ( entityString && bsp->entityStringLength > 0 ) {
		CL_BspPreview_ParseEntities( entityString, bsp->entityStringLength );
	}

	// Expand bounds to include markers
	for ( int i = 0; i < cl_bspPreview.numMarkers; i++ ) {
		float mx = cl_bspPreview.markers[i].x;
		float my = cl_bspPreview.markers[i].y;

		if ( !boundsInit ) {
			cl_bspPreview.minX = mx;
			cl_bspPreview.maxX = mx;
			cl_bspPreview.minY = my;
			cl_bspPreview.maxY = my;
			boundsInit = qtrue;
		} else {
			if ( mx < cl_bspPreview.minX ) cl_bspPreview.minX = mx;
			if ( mx > cl_bspPreview.maxX ) cl_bspPreview.maxX = mx;
			if ( my < cl_bspPreview.minY ) cl_bspPreview.minY = my;
			if ( my > cl_bspPreview.maxY ) cl_bspPreview.maxY = my;
		}
	}

	BSP_Free( bsp );

	if ( cl_bspPreview.numEdges > 0 || cl_bspPreview.numMarkers > 0 ) {
		cl_bspPreview.valid = qtrue;
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildBspPreview: %s -> %d edges, %d markers\n",
			bspPath, cl_bspPreview.numEdges, cl_bspPreview.numMarkers );
	}
}
