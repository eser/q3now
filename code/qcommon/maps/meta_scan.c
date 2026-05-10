/*
===========================================================================
Copyright (C) 2024 Wired engine contributors

This file is part of Quake III Arena source code.
Released under GPLv2 — see meta.h for the full notice.
===========================================================================
*/

// meta_scan.c -- map enumeration. Source of truth is FS_ListFiles("maps", ".bsp");
// for each .bsp, attempt to load its sidecar metadata in priority order:
//   1. maps/<name>.meta
//   2. scripts/<name>.arena   (legacy fallback)
//   3. defaults
//
// Orphan .meta or .arena without a corresponding .bsp are intentionally
// invisible. The maps_list[] array is reset and repopulated on every
// Maps_ScanAll call.

#include "meta.h"
#include "../qcommon.h"
LOG_DECLARE_CHANNEL( ch_maps, "maps" );

map_meta_t maps_list[META_MAX_MAPS];
int        maps_count = 0;

// qcommon's FS_FileExists only checks fs_homepath; we need to find files
// inside pk3/sw3z packs as well. FS_ReadFile with a NULL buffer returns
// the file size if findable, 0 (or <0) otherwise — exactly the test we want.
static qboolean Maps_SidecarExists( const char *path ) {
	return FS_ReadFile( path, NULL ) > 0;
}

qboolean Maps_LoadMetaFor( const char *mapname, map_meta_t *out ) {
	if ( !out || !mapname || !*mapname ) return qfalse;

	char path[MAX_QPATH];
	Q_strncpyz( out->mapname, mapname, sizeof( out->mapname ) );

	// 1. Prefer maps/<mapname>.meta
	Com_sprintf( path, sizeof( path ), "maps/%s.meta", mapname );
	if ( Maps_SidecarExists( path ) ) {
		if ( Meta_ParseFromFile( out, path ) ) {
			out->source = META_SOURCE_META;
			return qtrue;
		}
		Com_Log( SEV_WARN, LOG_CH(ch_maps),
			"%s present but failed to parse; using defaults.\n", path );
		Meta_SetDefaults( out, mapname );
		out->source = META_SOURCE_BROKEN;
		return qfalse;
	}

	// 2. Legacy fallback: scripts/<mapname>.arena
	Com_sprintf( path, sizeof( path ), "scripts/%s.arena", mapname );
	if ( Maps_SidecarExists( path ) ) {
		if ( Meta_ParseFromFile( out, path ) ) {
			out->source = META_SOURCE_ARENA;
			return qtrue;
		}
		// Fall through to defaults if .arena parsing failed (rare).
	}

	// 3. Neither — defaults.
	Meta_SetDefaults( out, mapname );
	out->source = META_SOURCE_NONE;
	return qfalse;
}

void Maps_ScanAll( void ) {
	maps_count = 0;
	Maps_ResetArena();   // reclaim previous remap_table_t storage

	int    numBsps = 0;
	char **bsps = FS_ListFiles( "maps", ".bsp", &numBsps );
	if ( !bsps ) return;

	for ( int i = 0; i < numBsps && maps_count < META_MAX_MAPS; i++ ) {
		const char *fname = bsps[i];
		if ( !fname || !*fname ) continue;

		// Skip alternate-layout BSPs (e.g. ztn3dm1-ho.bsp) at the
		// scanner level? No — we treat every BSP as its own arena; the
		// UI is free to filter. Keeping all bsps here matches the
		// existing WiredFeeder behavior.

		char mapname[MAX_QPATH];
		COM_StripExtension( fname, mapname, sizeof( mapname ) );
		if ( !mapname[0] ) continue;

		map_meta_t *m = &maps_list[maps_count++];
		Maps_LoadMetaFor( mapname, m );
	}

	FS_FreeFileList( bsps );

	Com_Log( SEV_DEBUG, LOG_CH(ch_maps),
		"Maps_ScanAll: %d maps enumerated\n", maps_count );
}

const map_meta_t *Maps_FindByName( const char *mapname ) {
	if ( !mapname || !*mapname ) return NULL;
	for ( int i = 0; i < maps_count; i++ ) {
		if ( !Q_stricmp( maps_list[i].mapname, mapname ) ) {
			return &maps_list[i];
		}
	}
	return NULL;
}

void Maps_AddOrRefresh( const char *mapname ) {
	if ( !mapname || !*mapname ) return;

	// Replace existing entry if present.
	for ( int i = 0; i < maps_count; i++ ) {
		if ( !Q_stricmp( maps_list[i].mapname, mapname ) ) {
			// Note: existing remap_table_t* is leaked into the arena until
			// the next Maps_ResetArena. That's acceptable — Maps_AddOrRefresh
			// is called rarely (late-arrival custom maps), the arena is 1MB
			// reserve, and FS_Restart resets the whole arena. No live
			// dangling pointer because R_SetActiveRemap is called by the
			// caller (SV_SpawnServer) AFTER this returns, picking up the
			// fresh remap.
			Maps_LoadMetaFor( mapname, &maps_list[i] );
			return;
		}
	}

	// Not found — append if room.
	if ( maps_count >= META_MAX_MAPS ) {
		Com_Log( SEV_WARN, LOG_CH(ch_maps),
			"Maps_AddOrRefresh: maps_list[] full (%d), cannot add '%s'\n",
			maps_count, mapname );
		return;
	}
	Maps_LoadMetaFor( mapname, &maps_list[maps_count] );
	maps_count++;
}
