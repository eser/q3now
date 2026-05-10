/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// r_image_probe.c — offline image asset availability check.
//
// Mirrors R_FindImageFile's extension-fallback behavior (without
// loading pixel data). Intentionally header-light: pulls only
// qcommon for FS_* + Q_* + COM_StripExtension. No renderer-internal
// state, no GL/Vulkan dependencies. Linked into every renderer DLL
// (via AUX_SOURCE_DIRECTORY) and into extract-meta's qcommon_tool.

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "r_image_probe.h"

// Single source of truth for image extension priority. Order
// approximates the engine's R_FindImageFile fallback chain.
static const char *r_image_extensions[] = {
	"tga", "jpg", "jpeg", "png", "pcx", "bmp", NULL
};

qboolean R_ImageResolves( const char *name ) {
	char localName[ MAX_VFS_PATH ];
	char altName[ MAX_VFS_PATH ];

	if ( !name || !*name ) return qfalse;

	// If the name already has a known image extension, probe that
	// exact path first. (Engine R_FindImageFile uses the extension
	// to pick the matching loader before fallback.)
	const char *ext = COM_GetExtension( name );
	if ( *ext ) {
		for ( const char **e = r_image_extensions; *e; e++ ) {
			if ( !Q_stricmp( ext, *e ) ) {
				fileHandle_t f;
				if ( FS_FOpenFileRead( name, &f, qtrue ) > 0 ) {
					FS_FCloseFile( f );
					return qtrue;
				}
				break;
			}
		}
	}

	// Strip extension (if any) and try each codec extension in
	// priority order.
	COM_StripExtension( name, localName, sizeof( localName ) );

	for ( const char **e = r_image_extensions; *e; e++ ) {
		fileHandle_t f;
		Com_sprintf( altName, sizeof( altName ), "%s.%s", localName, *e );
		if ( FS_FOpenFileRead( altName, &f, qtrue ) > 0 ) {
			FS_FCloseFile( f );
			return qtrue;
		}
	}
	return qfalse;
}
