/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

#include <stdio.h>
#include <string.h>

#include "q_shared.h"
#include "ent_emit.h"

qboolean EntEmit_Write(const char *out_dir,
                       const char *mapname,
                       const bsp_inventory_t *inv)
{
	if ( !out_dir || !mapname || !inv ) return qfalse;

	char path[ MAX_OSPATH ];
	Com_sprintf( path, sizeof( path ), "%s/%s.ent", out_dir, mapname );

	FILE *fp = fopen( path, "wb" );
	if ( !fp ) {
		fprintf( stderr, "extract-meta: EntEmit_Write fopen failed: %s\n", path );
		return qfalse;
	}

	// Strip a trailing NUL byte if the BSP included one (idTech 3
	// historically NUL-terminates the entity lump). The .ent file
	// reader on the engine side handles both, but keeping it text-only
	// makes diffs / hand-editing easier.
	int n = inv->entity_string_length;
	if ( n > 0 && inv->entity_string && inv->entity_string[ n - 1 ] == '\0' ) {
		n--;
	}
	if ( n > 0 && inv->entity_string ) {
		const size_t written = fwrite( inv->entity_string, 1, (size_t)n, fp );
		if ( (int)written != n ) {
			fclose( fp );
			fprintf( stderr, "extract-meta: EntEmit_Write short write\n" );
			return qfalse;
		}
	}
	// Append a trailing newline regardless — single dev-edit convention.
	fputc( '\n', fp );

	fclose( fp );
	return qtrue;
}
