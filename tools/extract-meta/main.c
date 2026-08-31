/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// main.c — extract-meta CLI entry. B2 implementation:
//
//   1. Argv parse: <mapname> [--out <dir>], strict.
//   2. Bootstrap engine subsystems via Tool_Init.
//   3. Verify maps/<map>.bsp resolves in the VFS.
//   4. Build shader index from scripts/*.shader.
//   5. Build BSP inventory: shader table → assets, entity scan →
//      noise/music/message keys.
//   6. For every needed asset: probe VFS; missing entries get a
//      resolution attempt (category swap, dir probe, default).
//   7. Emit <out>/<map>.meta (with remap rows for resolved assets)
//      and <out>/<map>.ent (entity lump verbatim).
//   8. Print human-readable summary to stderr; exit 0/1/2/3.
//
// Exit codes:
//   0 — success, all assets either available or resolved.
//   1 — files written, ≥1 asset unresolved.
//   2 — argv error / BSP not found.
//   3 — internal failure (BSP load failed, write failed, etc.).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_shared.h"
#include "qcommon.h"

#include "shader_index.h"
#include "map_inventory.h"
#include "asset_resolve.h"
#include "meta_emit.h"
#include "ent_emit.h"

void Tool_Init    ( int argc, char **argv );
void Tool_Shutdown( void );

static const char *AssetKindName(asset_kind_t k) {
	switch ( k ) {
	case ASSET_KIND_SHADER:  return "shader";
	case ASSET_KIND_TEXTURE: return "texture";
	case ASSET_KIND_SOUND:   return "sound";
	case ASSET_KIND_MUSIC:   return "music";
	}
	return "unknown";
}

typedef struct {
	int maps;
	int needed;
	int available;
	int missing;
	int resolved;
	int unresolved;
} audit_totals_t;

static int ProcessMap(const char *mapname, const char *out_dir,
	                  qboolean emit_files, qboolean strict,
	                  const shader_index_t *idx, audit_totals_t *totals) {
	map_inventory_t inv;
	if ( !MapInventory_Build( mapname, idx, &inv ) ) {
		fprintf( stderr, "extract-meta: BSP load failed: %s\n", mapname );
		return 3;
	}

	// 6. Resolve missing assets. Allocate worst-case resolutions[]
	// (one entry per inventory item — most will go unused since
	// available assets don't get a resolution row).
	resolution_t *resolutions = NULL;
	int           res_count   = 0;
	int           total       = inv.count;
	int           available   = 0;
	int           missing     = 0;
	int           resolved    = 0;
	int           unresolved  = 0;

	if ( inv.count > 0 ) {
		resolutions = (resolution_t *)calloc( (size_t)inv.count, sizeof( resolution_t ) );
		if ( !resolutions ) {
			fprintf( stderr, "extract-meta: out of memory for resolutions[]\n" );
			MapInventory_Free( &inv );
			return 3;
		}
	}

	for ( int i = 0; i < inv.count; i++ ) {
		if ( Asset_IsAvailable( &inv.entries[i], idx ) ) {
			available++;
			continue;
		}
		missing++;
		Asset_Resolve( &inv.entries[i], idx, &resolutions[ res_count ] );
		if ( resolutions[ res_count ].resolved ) {
			resolved++;
		} else {
			unresolved++;
		}
		res_count++;
	}

	// 7. Emit files unless this is a read-only audit pass.
	if ( emit_files && !MetaEmit_Write( out_dir, mapname, &inv, resolutions, res_count ) ) {
		fprintf( stderr, "extract-meta: failed to write .meta\n" );
		free( resolutions );
		MapInventory_Free( &inv );
		return 3;
	}
	if ( emit_files && !EntEmit_Write( out_dir, mapname, &inv ) ) {
		fprintf( stderr, "extract-meta: failed to write .ent\n" );
		free( resolutions );
		MapInventory_Free( &inv );
		return 3;
	}

	// 8. Report.
	fprintf( stderr, "\nextract-meta: %s\n", mapname );
	fprintf( stderr, "  needed:     %d\n", total );
	fprintf( stderr, "  available:  %d\n", available );
	fprintf( stderr, "  missing:    %d\n", missing );
	fprintf( stderr, "  resolved:   %d\n", resolved );
	fprintf( stderr, "  unresolved: %d\n", unresolved );

	if ( unresolved > 0 || ( strict && missing > 0 ) ) {
		fprintf( stderr, strict ? "\nstrict missing:\n" : "\nunresolved:\n" );
		for ( int i = 0; i < res_count; i++ ) {
			if ( strict || !resolutions[i].resolved ) {
				fprintf( stderr, "  %-7s  %s%s%s\n",
				         AssetKindName( resolutions[i].source.kind ),
				         resolutions[i].source.path,
				         resolutions[i].resolved ? " -> " : "",
				         resolutions[i].resolved ? resolutions[i].replacement : "" );
			}
		}
	}

	if ( emit_files ) {
		fprintf( stderr, "\nwrote: %s/%s.meta\n", out_dir, mapname );
		fprintf( stderr, "wrote: %s/%s.ent\n",   out_dir, mapname );
	}

	if ( totals ) {
		totals->maps++;
		totals->needed += total;
		totals->available += available;
		totals->missing += missing;
		totals->resolved += resolved;
		totals->unresolved += unresolved;
	}

	free( resolutions );
	MapInventory_Free( &inv );
	return ( strict ? missing : unresolved ) > 0 ? 1 : 0;
}

int main(int argc, char **argv) {
	const char *mapname = NULL;
	const char *out_dir = ".";
	qboolean all_maps = qfalse;
	qboolean audit_only = qfalse;
	qboolean strict = qfalse;

	for ( int i = 1; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--out" ) && i + 1 < argc ) {
			out_dir = argv[++i];
		} else if ( !strcmp( argv[i], "--all-maps" ) ) {
			all_maps = qtrue;
		} else if ( !strcmp( argv[i], "--audit-only" ) ) {
			audit_only = qtrue;
		} else if ( !strcmp( argv[i], "--strict" ) ) {
			strict = qtrue;
		} else if ( argv[i][0] != '-' && !mapname ) {
			mapname = argv[i];
		} else {
			fprintf( stderr, "extract-meta: unknown argument: %s\n", argv[i] );
			return 2;
		}
	}
	if ( ( all_maps && mapname ) || ( !all_maps && !mapname ) ) {
		fprintf( stderr, "usage: extract-meta (<mapname> | --all-maps) [--strict] [--audit-only] [--out <dir>]\n" );
		return 2;
	}

	Tool_Init( argc, argv );
	shader_index_t *idx = ShaderIndex_Build();
	audit_totals_t totals = { 0 };
	int result = 0;

	if ( all_maps ) {
		int count = 0;
		char **files = FS_ListFiles( "maps", ".bsp", &count );
		if ( !files || count <= 0 ) {
			fprintf( stderr, "extract-meta: no maps/*.bsp found\n" );
			result = 2;
		} else {
			for ( int i = 0; i < count; i++ ) {
				char name[ MAX_QPATH ];
				COM_StripExtension( files[i], name, sizeof( name ) );
				const int map_result = ProcessMap( name, out_dir, !audit_only, strict, idx, &totals );
				if ( map_result > result ) result = map_result;
			}
			FS_FreeFileList( files );
		}
	} else {
		char bsp_path[ MAX_QPATH ];
		fileHandle_t file = 0;
		Com_sprintf( bsp_path, sizeof( bsp_path ), "maps/%s.bsp", mapname );
		const long length = FS_FOpenFileRead( bsp_path, &file, qfalse );
		if ( file ) FS_FCloseFile( file );
		if ( length <= 0 ) {
			fprintf( stderr, "extract-meta: BSP not found: %s\n", bsp_path );
			result = 2;
		} else {
			result = ProcessMap( mapname, out_dir, !audit_only, strict, idx, &totals );
		}
	}

	if ( all_maps && totals.maps > 0 ) {
		fprintf( stderr, "\nextract-meta aggregate:\n" );
		fprintf( stderr, "  maps:       %d\n", totals.maps );
		fprintf( stderr, "  needed:     %d\n", totals.needed );
		fprintf( stderr, "  available:  %d\n", totals.available );
		fprintf( stderr, "  missing:    %d\n", totals.missing );
		fprintf( stderr, "  resolved:   %d\n", totals.resolved );
		fprintf( stderr, "  unresolved: %d\n", totals.unresolved );
	}

	ShaderIndex_Free( idx );
	Tool_Shutdown();

	return result;
}
