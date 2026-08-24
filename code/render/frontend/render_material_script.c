// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_material_script.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct {
	const char *cursor;
	const char *end;
} materialTokenizer_t;

static qboolean CopyToken( char *target, size_t size,
		const char *start, size_t length ) {
	if ( !target || !size || length >= size ) return qfalse;
	memcpy( target, start, length ); target[length] = '\0';
	return qtrue;
}

static qboolean NextToken( materialTokenizer_t *parser,
		char token[MAX_QPATH] ) {
	const char *start;
	if ( !parser || !token ) return qfalse;
	for ( ;; ) {
		while ( parser->cursor < parser->end
				&& (unsigned char)*parser->cursor <= ' ' ) parser->cursor++;
		if ( parser->cursor + 1 < parser->end
				&& parser->cursor[0] == '/' && parser->cursor[1] == '/' ) {
			parser->cursor += 2;
			while ( parser->cursor < parser->end
					&& *parser->cursor != '\n' ) parser->cursor++;
			continue;
		}
		if ( parser->cursor + 1 < parser->end
				&& parser->cursor[0] == '/' && parser->cursor[1] == '*' ) {
			parser->cursor += 2;
			while ( parser->cursor + 1 < parser->end
					&& !( parser->cursor[0] == '*' && parser->cursor[1] == '/' ) )
				parser->cursor++;
			if ( parser->cursor + 1 >= parser->end ) return qfalse;
			parser->cursor += 2; continue;
		}
		break;
	}
	if ( parser->cursor >= parser->end ) return qfalse;
	if ( *parser->cursor == '{' || *parser->cursor == '}' ) {
		token[0] = *parser->cursor++; token[1] = '\0'; return qtrue;
	}
	if ( *parser->cursor == '"' ) {
		start = ++parser->cursor;
		while ( parser->cursor < parser->end && *parser->cursor != '"' )
			parser->cursor++;
		if ( parser->cursor >= parser->end
				|| !CopyToken( token, MAX_QPATH, start,
					(size_t)( parser->cursor - start ) ) ) return qfalse;
		parser->cursor++; return qtrue;
	}
	start = parser->cursor;
	while ( parser->cursor < parser->end
			&& (unsigned char)*parser->cursor > ' '
			&& *parser->cursor != '{' && *parser->cursor != '}' )
		parser->cursor++;
	return CopyToken( token, MAX_QPATH, start,
		(size_t)( parser->cursor - start ) );
}

static qboolean UsableImage( const char *name ) {
	return name && name[0] && name[0] != '$' && strcmp( name, "-" )
		? qtrue : qfalse;
}

static void StoreEntry( renderMaterialScriptCatalog_t *catalog,
		const renderMaterialScriptEntry_t *entry ) {
	uint32_t i;
	if ( !entry->name[0] || !entry->imageName[0] ) return;
	for ( i = 0u; i < catalog->count; ++i ) {
		if ( !strcasecmp( catalog->entries[i].name, entry->name ) ) {
			catalog->entries[i] = *entry; return;
		}
	}
	if ( catalog->count < RENDER_MATERIAL_SCRIPT_MAX_ENTRIES )
		catalog->entries[catalog->count++] = *entry;
}

static qboolean ParseFile( renderMaterialScriptCatalog_t *catalog,
		const char *text, size_t size ) {
	materialTokenizer_t parser = { text, text + size };
	char token[MAX_QPATH], value[MAX_QPATH];
	while ( NextToken( &parser, token ) ) {
		renderMaterialScriptEntry_t entry;
		int depth = 0;
		char skyPrefix[MAX_QPATH] = "";
		memset( &entry, 0, sizeof( entry ) );
		(void)snprintf( entry.name, sizeof( entry.name ), "%s", token );
		entry.alphaMode = RENDER_ALPHA_OPAQUE;
		entry.alphaCutoff = 0.5f; entry.depthWrite = qtrue;
		if ( !NextToken( &parser, token ) || strcmp( token, "{" ) ) continue;
		depth = 1;
		while ( depth && NextToken( &parser, token ) ) {
			if ( !strcmp( token, "{" ) ) { depth++; continue; }
			if ( !strcmp( token, "}" ) ) { depth--; continue; }
			if ( depth == 1 && !strcasecmp( token, "surfaceparm" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "sky" ) ) entry.sky = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "skyparms" ) ) {
				char ignored[MAX_QPATH];
				if ( !NextToken( &parser, value )
						|| !NextToken( &parser, ignored )
						|| !NextToken( &parser, ignored ) ) return qfalse;
				if ( UsableImage( value ) )
					(void)snprintf( skyPrefix, sizeof( skyPrefix ), "%s", value );
				entry.sky = qtrue; continue;
			}
			if ( depth != 2 ) continue;
			if ( !strcasecmp( token, "map" )
					|| !strcasecmp( token, "clampmap" ) ) {
				qboolean clamp = !strcasecmp( token, "clampmap" );
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !entry.imageName[0] && UsableImage( value ) ) {
					(void)snprintf( entry.imageName, sizeof( entry.imageName ),
						"%s", value ); entry.clampToEdge = clamp;
				}
			} else if ( !strcasecmp( token, "animmap" ) ) {
				if ( !NextToken( &parser, value ) || !NextToken( &parser, value ) )
					return qfalse;
				if ( !entry.imageName[0] && UsableImage( value ) )
					(void)snprintf( entry.imageName, sizeof( entry.imageName ),
						"%s", value );
			} else if ( !strcasecmp( token, "alphafunc" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				entry.alphaMode = RENDER_ALPHA_MASK;
				entry.alphaCutoff = !strcasecmp( value, "ge128" ) ? 0.5f : 0.0f;
			} else if ( !strcasecmp( token, "blendfunc" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				entry.alphaMode = RENDER_ALPHA_BLEND; entry.depthWrite = qfalse;
			} else if ( !strcasecmp( token, "depthwrite" ) ) {
				entry.depthWrite = qtrue;
			}
		}
		if ( entry.sky && skyPrefix[0] )
			(void)snprintf( entry.imageName, sizeof( entry.imageName ),
				"%s_up", skyPrefix );
		StoreEntry( catalog, &entry );
	}
	return qtrue;
}

qboolean RenderMaterialScript_Load( renderMaterialScriptCatalog_t *catalog,
		const refimport_t *imports ) {
	char **files;
	int count = 0;
	if ( !catalog || !imports ) return qfalse;
	memset( catalog, 0, sizeof( *catalog ) );
	/* A focused/offscreen host may intentionally provide no VFS. Direct image
	 * registration remains valid there; the product host supplies the complete
	 * cohort and gets the populated script catalog. */
	if ( !imports->FS_ListFiles || !imports->FS_FreeFileList
			|| !imports->FS_ReadFile || !imports->FS_FreeFile ) {
		catalog->ready = qtrue; return qtrue;
	}
	files = imports->FS_ListFiles( "scripts", ".shader", &count );
	for ( int i = 0; files && i < count; ++i ) {
		char path[MAX_QPATH]; void *text = NULL;
		int bytes = snprintf( path, sizeof( path ), "scripts/%s", files[i] );
		if ( bytes <= 0 || bytes >= (int)sizeof( path ) ) continue;
		bytes = imports->FS_ReadFile( path, &text );
		if ( bytes > 0 && text ) (void)ParseFile( catalog,
			(const char *)text, (size_t)bytes );
		if ( text ) imports->FS_FreeFile( text );
	}
	if ( files ) imports->FS_FreeFileList( files );
	catalog->ready = qtrue;
	return qtrue;
}

qboolean RenderMaterialScript_Lookup(
		const renderMaterialScriptCatalog_t *catalog, const char *name,
		renderMaterialScriptEntry_t *outEntry ) {
	if ( !catalog || !catalog->ready || !name || !outEntry ) return qfalse;
	for ( uint32_t i = 0u; i < catalog->count; ++i ) {
		if ( !strcasecmp( catalog->entries[i].name, name ) ) {
			*outEntry = catalog->entries[i]; return qtrue;
		}
	}
	return qfalse;
}
