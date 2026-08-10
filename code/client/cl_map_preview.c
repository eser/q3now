// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_bsp_preview.c -- extract simplified 2D wireframe from BSP for loading screen

#include "client.h"
#include "../qcommon/qfiles.h"
LOG_DECLARE_CHANNEL( ch_client, "client" );

mapPreview_t cl_mapPreview;

/*
================
CL_ClearMapPreview
================
*/
void CL_ClearMapPreview( void ) {
	memset( &cl_mapPreview, 0, sizeof( cl_mapPreview ) );
}

/*
================
CL_MapPreview_AddEdge
================
*/
static void CL_MapPreview_AddEdge( float x1, float y1, float z1,
								   float x2, float y2, float z2, int type ) {
	mapPreviewEdge_t *edge;

	if ( cl_mapPreview.numEdges >= MAP_PREVIEW_MAX_EDGES ) {
		return;
	}

	edge = &cl_mapPreview.edges[cl_mapPreview.numEdges];
	edge->x1 = x1;
	edge->y1 = y1;
	edge->z1 = z1;
	edge->x2 = x2;
	edge->y2 = y2;
	edge->z2 = z2;
	edge->type = type;
	cl_mapPreview.numEdges++;
}

/*
================
CL_MapPreview_AddMarker
================
*/
static void CL_MapPreview_AddMarker( float x, float y, int type ) {
	mapPreviewMarker_t *marker;

	if ( cl_mapPreview.numMarkers >= MAP_PREVIEW_MAX_MARKERS ) {
		return;
	}

	marker = &cl_mapPreview.markers[cl_mapPreview.numMarkers];
	marker->x = x;
	marker->y = y;
	marker->type = type;
	cl_mapPreview.numMarkers++;
}

/*
================
CL_MapPreview_UpdateBounds
================
*/
static void CL_MapPreview_UpdateBounds( float x, float y ) {
	if ( cl_mapPreview.numEdges == 1 && cl_mapPreview.numMarkers == 0 ) {
		// First point — initialize bounds
		cl_mapPreview.minX = x;
		cl_mapPreview.maxX = x;
		cl_mapPreview.minY = y;
		cl_mapPreview.maxY = y;
	} else {
		if ( x < cl_mapPreview.minX ) cl_mapPreview.minX = x;
		if ( x > cl_mapPreview.maxX ) cl_mapPreview.maxX = x;
		if ( y < cl_mapPreview.minY ) cl_mapPreview.minY = y;
		if ( y > cl_mapPreview.maxY ) cl_mapPreview.maxY = y;
	}
}

/*
================
CL_MapPreview_ParseEntities

Parse the entity text lump for spawn points, items, and flags.
Entity format: { "key" "value" ... }
================
*/
static void CL_MapPreview_ParseEntities( const char *entityString, int entityLen ) {
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
				CL_MapPreview_AddMarker( origin[0], origin[1], markerType );
			}
		}
	}
}

/*
================
CL_BuildMapPreview

Open the BSP file and extract a simplified 2D wireframe for the
loading screen preview.  Only MST_PLANAR surfaces are included.
================
*/
void CL_BuildMapPreview( const char *mapname ) {
	CL_ClearMapPreview();

	if ( !mapname || !mapname[0] ) {
		return;
	}

	// Build BSP path
	char bspPath[MAX_QPATH];
	Com_sprintf( bspPath, sizeof( bspPath ), "maps/%s.bsp", mapname );

	mapFile_t *bsp;
	if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAG_RENDER_ONLY ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildMapPreview: could not open %s\n", bspPath );
		return;
	}

	drawVert_t *drawVerts = bsp->drawVerts;
	int numDrawVerts = bsp->numDrawVerts;
	dsurface_t *surfaces = bsp->surfaces;
	int numSurfaces = bsp->numSurfaces;
	const char *entityString = bsp->entityString;

	if ( numDrawVerts <= 0 || !drawVerts ) {
		Map_Free( bsp );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildMapPreview: no draw verts in %s\n", bspPath );
		return;
	}

	if ( numSurfaces <= 0 || !surfaces ) {
		Map_Free( bsp );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildMapPreview: no surfaces in %s\n", bspPath );
		return;
	}

	// --- Pass 1: Compute total XY bounds across all planar surfaces ---
	qboolean boundsInit = qfalse;
	cl_mapPreview.numSurfaces = 0;
	{
		float totalMinX = 0, totalMinY = 0, totalMaxX = 0, totalMaxY = 0;
		float totalRangeX, totalRangeY;

		for ( int i = 0; i < numSurfaces; i++ ) {
			int firstVert, numVerts;
			if ( surfaces[i].surfaceType != MST_PLANAR ) continue;
			firstVert = surfaces[i].firstVert;
			numVerts = surfaces[i].numVerts;
			/* Overflow-safe bound check: `firstVert + numVerts > numDrawVerts`
			 * would overflow signed int for file-controlled lump values near
			 * INT_MAX and wrap negative, passing the guard. Compare via
			 * subtraction instead (numVerts already >0 and numDrawVerts >0, so
			 * numDrawVerts - numVerts cannot overflow). */
			if ( numVerts < 3 || firstVert < 0
			  || numVerts > numDrawVerts || firstVert > numDrawVerts - numVerts ) continue;
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

			cl_mapPreview.numSurfaces++;

			firstVert = surfaces[i].firstVert;
			numVerts = surfaces[i].numVerts;

			if ( numVerts < 3 ) {
				continue;
			}
			/* Overflow-safe bound check (see the matching guard above). */
			if ( firstVert < 0
			  || numVerts > numDrawVerts || firstVert > numDrawVerts - numVerts ) {
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

				CL_MapPreview_AddEdge( x1, y1, z1, x2, y2, z2, 1 );

				// Update XY bounding box
				if ( !boundsInit ) {
					cl_mapPreview.minX = x1; cl_mapPreview.maxX = x1;
					cl_mapPreview.minY = y1; cl_mapPreview.maxY = y1;
					cl_mapPreview.minZ = z1; cl_mapPreview.maxZ = z1;
					boundsInit = qtrue;
				}
				if ( x1 < cl_mapPreview.minX ) cl_mapPreview.minX = x1;
				if ( x1 > cl_mapPreview.maxX ) cl_mapPreview.maxX = x1;
				if ( y1 < cl_mapPreview.minY ) cl_mapPreview.minY = y1;
				if ( y1 > cl_mapPreview.maxY ) cl_mapPreview.maxY = y1;
				if ( x2 < cl_mapPreview.minX ) cl_mapPreview.minX = x2;
				if ( x2 > cl_mapPreview.maxX ) cl_mapPreview.maxX = x2;
				if ( y2 < cl_mapPreview.minY ) cl_mapPreview.minY = y2;
				if ( y2 > cl_mapPreview.maxY ) cl_mapPreview.maxY = y2;
				// Update Z range
				if ( z1 < cl_mapPreview.minZ ) cl_mapPreview.minZ = z1;
				if ( z1 > cl_mapPreview.maxZ ) cl_mapPreview.maxZ = z1;
				if ( z2 < cl_mapPreview.minZ ) cl_mapPreview.minZ = z2;
				if ( z2 > cl_mapPreview.maxZ ) cl_mapPreview.maxZ = z2;

				if ( cl_mapPreview.numEdges >= MAP_PREVIEW_MAX_EDGES ) {
					break;
				}
			}

			if ( cl_mapPreview.numEdges >= MAP_PREVIEW_MAX_EDGES ) {
				break;
			}
		}
	}

	// --- Read LUMP_ENTITIES ---
	if ( entityString && bsp->entityStringLength > 0 ) {
		CL_MapPreview_ParseEntities( entityString, bsp->entityStringLength );
	}

	// Expand bounds to include markers
	for ( int i = 0; i < cl_mapPreview.numMarkers; i++ ) {
		float mx = cl_mapPreview.markers[i].x;
		float my = cl_mapPreview.markers[i].y;

		if ( !boundsInit ) {
			cl_mapPreview.minX = mx;
			cl_mapPreview.maxX = mx;
			cl_mapPreview.minY = my;
			cl_mapPreview.maxY = my;
			boundsInit = qtrue;
		} else {
			if ( mx < cl_mapPreview.minX ) cl_mapPreview.minX = mx;
			if ( mx > cl_mapPreview.maxX ) cl_mapPreview.maxX = mx;
			if ( my < cl_mapPreview.minY ) cl_mapPreview.minY = my;
			if ( my > cl_mapPreview.maxY ) cl_mapPreview.maxY = my;
		}
	}

	Map_Free( bsp );

	if ( cl_mapPreview.numEdges > 0 || cl_mapPreview.numMarkers > 0 ) {
		cl_mapPreview.valid = qtrue;
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_BuildMapPreview: %s -> %d edges, %d markers\n",
			bspPath, cl_mapPreview.numEdges, cl_mapPreview.numMarkers );
	}
}
