// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// r_image_probe.c — offline image asset availability check.
//
// Mirrors R_FindImageFile's extension-fallback behavior (without
// loading pixel data). Intentionally header-light: pulls only
// qcommon for FS_* + Q_*. No renderer-internal
// state, no GL/Vulkan dependencies. Linked into every renderer DLL
// (via AUX_SOURCE_DIRECTORY) and into extract-meta's qcommon_tool.

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
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

	// Explicit extensions are exact. Format/path compatibility is declared in
	// fs-aliases.lua and resolved by FS_FOpenFileRead; only extensionless legacy
	// material references use the bounded codec probe below.
	const char *ext = COM_GetExtension( name );
	if ( *ext ) {
		fsResolvedResource_t resolved;
		if ( !FS_ResolveResource( name, &resolved ) ) {
			return qfalse;
		}
		ext = COM_GetExtension( resolved.canonicalPath );
		for ( const char **e = r_image_extensions; *e; e++ ) {
			if ( !Q_stricmp( ext, *e ) ) {
				return qtrue;
			}
		}
		return qfalse;
	}

	Q_strncpyz( localName, name, sizeof( localName ) );

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
