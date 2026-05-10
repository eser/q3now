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
#include "bsp_inventory.h"
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

int main(int argc, char **argv) {
	const char *mapname = NULL;
	const char *out_dir = ".";

	// 1. Argv parse.
	if ( argc < 2 ) {
		fprintf( stderr, "usage: extract-meta <mapname> [--out <dir>]\n" );
		return 2;
	}
	mapname = argv[1];
	for ( int i = 2; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--out" ) && i + 1 < argc ) {
			out_dir = argv[++i];
		} else {
			fprintf( stderr, "extract-meta: unknown argument: %s\n", argv[i] );
			return 2;
		}
	}

	// 2. Bootstrap.
	Tool_Init( argc, argv );

	// 3. Verify BSP exists.
	char bsp_path[ MAX_QPATH ];
	Com_sprintf( bsp_path, sizeof( bsp_path ), "maps/%s.bsp", mapname );
	{
		fileHandle_t f   = 0;
		const long   len = FS_FOpenFileRead( bsp_path, &f, qfalse );
		if ( f ) FS_FCloseFile( f );
		if ( len <= 0 ) {
			fprintf( stderr, "extract-meta: BSP not found: %s\n", bsp_path );
			Tool_Shutdown();
			return 2;
		}
	}

	// 4. Shader index.
	shader_index_t *idx = ShaderIndex_Build();

	// 5. BSP inventory.
	bsp_inventory_t inv;
	if ( !BspInventory_Build( mapname, idx, &inv ) ) {
		fprintf( stderr, "extract-meta: BSP load failed: %s\n", mapname );
		ShaderIndex_Free( idx );
		Tool_Shutdown();
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
			BspInventory_Free( &inv );
			ShaderIndex_Free( idx );
			Tool_Shutdown();
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

	// 7. Emit files.
	if ( !MetaEmit_Write( out_dir, mapname, &inv, resolutions, res_count ) ) {
		fprintf( stderr, "extract-meta: failed to write .meta\n" );
		free( resolutions );
		BspInventory_Free( &inv );
		ShaderIndex_Free( idx );
		Tool_Shutdown();
		return 3;
	}
	if ( !EntEmit_Write( out_dir, mapname, &inv ) ) {
		fprintf( stderr, "extract-meta: failed to write .ent\n" );
		free( resolutions );
		BspInventory_Free( &inv );
		ShaderIndex_Free( idx );
		Tool_Shutdown();
		return 3;
	}

	// 8. Report.
	fprintf( stderr, "\nextract-meta: %s\n", mapname );
	fprintf( stderr, "  needed:     %d\n", total );
	fprintf( stderr, "  available:  %d\n", available );
	fprintf( stderr, "  missing:    %d\n", missing );
	fprintf( stderr, "  resolved:   %d\n", resolved );
	fprintf( stderr, "  unresolved: %d\n", unresolved );

	if ( unresolved > 0 ) {
		fprintf( stderr, "\nunresolved:\n" );
		for ( int i = 0; i < res_count; i++ ) {
			if ( !resolutions[i].resolved ) {
				fprintf( stderr, "  %-7s  %s\n",
				         AssetKindName( resolutions[i].source.kind ),
				         resolutions[i].source.path );
			}
		}
	}

	fprintf( stderr, "\nwrote: %s/%s.meta\n", out_dir, mapname );
	fprintf( stderr, "wrote: %s/%s.ent\n",   out_dir, mapname );

	// 9. Cleanup.
	free( resolutions );
	BspInventory_Free( &inv );
	ShaderIndex_Free( idx );
	Tool_Shutdown();

	return ( unresolved > 0 ) ? 1 : 0;
}
