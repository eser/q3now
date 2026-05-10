/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// shader_index.c — enumerate scripts/*.shader, parse each as a loose
// list of `name { body }` entries, collect stage map references for
// each shader.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_shared.h"
#include "qcommon.h"
#include "arena.h"
#include "shader_index.h"

// Shared with tool_init.c — defined there, used here for arena allocs.
extern arena_t *Tool_Arena;

// FNV-1a 32-bit on the lowercased name, masked to 8 bits.
static unsigned ShaderName_Hash(const char *name) {
	unsigned h = 2166136261u;
	for ( const char *p = name; *p; p++ ) {
		unsigned char c = (unsigned char)tolower((unsigned char)*p);
		h ^= c;
		h *= 16777619u;
	}
	return h & 0xffu;
}

static char *Arena_Strdup(arena_t *a, const char *s) {
	const size_t n = strlen(s) + 1;
	char *out = (char *)Arena_Alloc( a, n, 1 );
	if ( out ) memcpy( out, s, n );
	return out;
}

// Skip if the path is a built-in or variable-form (engine generates
// these — they're never real disk assets).
static qboolean IsSkippableMapPath(const char *path) {
	if ( !path || !*path ) return qtrue;
	if ( path[0] == '$' )  return qtrue;   /* $lightmap, $whiteimage, $dlight */
	return qfalse;
}

// Walk one stage-block body, collecting any map paths into stage_maps.
// Caller has already consumed the opening '{'. Returns after consuming
// the matching '}'. Robust against malformed input — skips unknown
// directives via SkipRestOfLine, bails on EOF.
static void ParseStageBody(ComParser *parser, const char **p,
                           shader_def_t *def, char **scratch_paths,
                           int *scratch_count, int scratch_cap)
{
	while ( 1 ) {
		const char *tok = COM_ParseExt( parser, p, qtrue );
		if ( !tok || !tok[0] ) break;
		if ( tok[0] == '}' ) return;

		// Only collect map paths — ignore the rest.
		if ( !Q_stricmp( tok, "map" ) || !Q_stricmp( tok, "clampMap" ) ) {
			const char *path = COM_ParseExt( parser, p, qfalse );
			if ( !path || !path[0] ) continue;
			if ( IsSkippableMapPath( path ) ) continue;
			if ( *scratch_count < scratch_cap ) {
				scratch_paths[ (*scratch_count)++ ] = Arena_Strdup( Tool_Arena, path );
			}
			continue;
		}
		if ( !Q_stricmp( tok, "animMap" ) || !Q_stricmp( tok, "clampAnimMap" ) ) {
			// freq, then up to 8 map paths.
			(void)COM_ParseExt( parser, p, qfalse );   /* discard freq */
			for ( int i = 0; i < 8; i++ ) {
				const char *path = COM_ParseExt( parser, p, qfalse );
				if ( !path || !path[0] ) break;
				if ( IsSkippableMapPath( path ) ) continue;
				if ( *scratch_count < scratch_cap ) {
					scratch_paths[ (*scratch_count)++ ] = Arena_Strdup( Tool_Arena, path );
				}
			}
			continue;
		}
		// Unknown stage directive: skip to end of line.
		SkipRestOfLine( parser, p );
	}
	(void)def; (void)scratch_paths; (void)scratch_count;
}

// Walk one shader body. Caller has consumed shader_name + '{'.
// Returns after the matching shader-level '}'. Stage blocks (nested
// '{ ... }') feed ParseStageBody.
static void ParseShaderBody(ComParser *parser, const char **p,
                            shader_def_t *def)
{
	enum { SCRATCH_CAP = 32 };
	char *scratch[ SCRATCH_CAP ];
	int   scratch_count = 0;

	while ( 1 ) {
		const char *tok = COM_ParseExt( parser, p, qtrue );
		if ( !tok || !tok[0] ) break;
		if ( tok[0] == '}' ) break;
		if ( tok[0] == '{' ) {
			ParseStageBody( parser, p, def, scratch, &scratch_count, SCRATCH_CAP );
			continue;
		}
		// Shader-level directives (q3map_*, surfaceparm, deformVertexes,
		// cull, sort, ...) are uninteresting to the asset inventory.
		// Skip the rest of the line.
		SkipRestOfLine( parser, p );
	}

	if ( scratch_count == 0 ) {
		def->stage_maps      = NULL;
		def->stage_map_count = 0;
		return;
	}
	def->stage_maps = (char **)Arena_Alloc(
		Tool_Arena, scratch_count * sizeof(char *), sizeof(char *) );
	if ( !def->stage_maps ) {
		def->stage_map_count = 0;
		return;
	}
	memcpy( def->stage_maps, scratch, scratch_count * sizeof(char *) );
	def->stage_map_count = scratch_count;
}

// Parse one shader file's full body. Each iteration consumes a
// shader_name + '{ body }' triple; on any structural surprise, skip
// to the next opening '{' / '}' and try again.
static void ParseShaderFile(shader_index_t *idx, const char *src,
                            const char *file_for_errors)
{
	ComParser  parser;
	const char *p = src;
	COM_BeginParseSession( &parser, file_for_errors );

	while ( 1 ) {
		const char *name = COM_ParseExt( &parser, &p, qtrue );
		if ( !name || !name[0] ) break;

		// Save name across the next ParseExt call.
		char shader_name[ MAX_QPATH ];
		Q_strncpyz( shader_name, name, sizeof( shader_name ) );

		const char *brace = COM_ParseExt( &parser, &p, qtrue );
		if ( !brace || brace[0] != '{' ) {
			// Malformed — skip to next opening brace and retry.
			while ( brace && brace[0] && brace[0] != '{' ) {
				brace = COM_ParseExt( &parser, &p, qtrue );
			}
			if ( !brace || brace[0] != '{' ) break;
		}

		shader_def_t *def = (shader_def_t *)Arena_Alloc(
			Tool_Arena, sizeof( *def ), sizeof( void * ) );
		if ( !def ) return;
		Q_strncpyz( def->name, shader_name, sizeof( def->name ) );
		def->stage_maps      = NULL;
		def->stage_map_count = 0;
		def->next            = NULL;

		ParseShaderBody( &parser, &p, def );

		// Insert into bucket — last-defined wins (engine behavior).
		const unsigned bucket = ShaderName_Hash( def->name );
		shader_def_t **link = &idx->buckets[ bucket ];
		while ( *link ) {
			if ( !Q_stricmp( (*link)->name, def->name ) ) {
				// Replace existing entry with the newer one.
				def->next = (*link)->next;
				*link     = def;
				goto inserted;
			}
			link = &(*link)->next;
		}
		*link = def;
		idx->total_count++;
inserted: ;
	}
}

shader_index_t *ShaderIndex_Build(void) {
	if ( !Tool_Arena ) return NULL;

	shader_index_t *idx = (shader_index_t *)Arena_Alloc(
		Tool_Arena, sizeof( *idx ), sizeof( void * ) );
	if ( !idx ) return NULL;
	memset( idx, 0, sizeof( *idx ) );

	int    n     = 0;
	char **files = FS_ListFiles( "scripts", ".shader", &n );
	if ( !files || n == 0 ) {
		if ( files ) FS_FreeFileList( files );
		return idx;   /* empty index is valid */
	}

	for ( int i = 0; i < n; i++ ) {
		if ( !files[i] ) continue;
		char fullpath[ MAX_QPATH ];
		Com_sprintf( fullpath, sizeof( fullpath ), "scripts/%s", files[i] );

		void *buf = NULL;
		const long len = FS_ReadFile( fullpath, &buf );
		if ( len <= 0 || !buf ) {
			if ( buf ) FS_FreeFile( buf );
			continue;
		}
		ParseShaderFile( idx, (const char *)buf, fullpath );
		FS_FreeFile( buf );
	}

	FS_FreeFileList( files );
	return idx;
}

qboolean ShaderIndex_Has(const shader_index_t *idx, const char *name) {
	return ShaderIndex_Find( idx, name ) != NULL;
}

const shader_def_t *ShaderIndex_Find(const shader_index_t *idx, const char *name) {
	if ( !idx || !name || !*name ) return NULL;
	const unsigned bucket = ShaderName_Hash( name );
	for ( const shader_def_t *d = idx->buckets[ bucket ]; d; d = d->next ) {
		if ( !Q_stricmp( d->name, name ) ) return d;
	}
	return NULL;
}

void ShaderIndex_Free(shader_index_t *idx) {
	(void)idx;   /* arena-owned; freed wholesale at Tool_Shutdown */
}
