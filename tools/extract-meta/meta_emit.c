/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

// meta_emit.c — write <out_dir>/<map>.meta as a plain file via stdio,
// bypassing the engine FS write path (which routes everything to
// fs_homepath). Output format mirrors the engine's meta_write.c
// exactly, so files round-trip cleanly through Meta_ParseFromBuffer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_shared.h"
#include "qcommon.h"
#include "maps/meta.h"
#include "meta_emit.h"

static remap_kind_t AssetKindToRemapKind(asset_kind_t k) {
	switch ( k ) {
	case ASSET_KIND_SHADER:  return REMAP_KIND_SHADER;
	case ASSET_KIND_TEXTURE: return REMAP_KIND_TEXTURE;
	case ASSET_KIND_SOUND:   return REMAP_KIND_SOUND;
	case ASSET_KIND_MUSIC:   return REMAP_KIND_MUSIC;
	}
	return REMAP_KIND_COUNT;
}

static void EmitString(FILE *fp, const char *key, const char *value) {
	if ( !value || !value[0] ) return;
	fprintf( fp, "\t%s\t\"%s\"\n", key, value );
}

static void EmitInt(FILE *fp, const char *key, int value) {
	if ( value == 0 ) return;
	fprintf( fp, "\t%s\t%d\n", key, value );
}

static void EmitGametypes(FILE *fp, const meta_gametypes_t *gt) {
	if ( !gt || gt->count <= 0 ) return;
	fprintf( fp, "\ttype\t\"" );
	for ( int i = 0; i < gt->count; i++ ) {
		fprintf( fp, "%s%s", ( i ? " " : "" ), gt->tokens[i] );
	}
	fprintf( fp, "\"\n" );
}

static int RemapEntryCompare(const void *a, const void *b) {
	const remap_entry_t *ea = *(const remap_entry_t * const *)a;
	const remap_entry_t *eb = *(const remap_entry_t * const *)b;
	return Q_stricmp( ea->src, eb->src );
}

static const char *RemapKindKeyword(remap_kind_t k) {
	switch ( k ) {
	case REMAP_KIND_SHADER:  return "shaders";
	case REMAP_KIND_TEXTURE: return "textures";
	case REMAP_KIND_SOUND:   return "sounds";
	case REMAP_KIND_MUSIC:   return "music";
	default:                 return NULL;
	}
}

static void EmitRemapKindBlock(FILE *fp, remap_kind_t kind, const remap_table_t *t) {
	if ( !t || t->count <= 0 ) return;
	const char *kw = RemapKindKeyword( kind );
	if ( !kw ) return;

	const remap_entry_t *flat[ META_MAX_REMAPS ];
	int n = 0;
	for ( int b = 0; b < (int)ARRAY_LEN( t->buckets ); b++ ) {
		for ( const remap_entry_t *e = t->buckets[b]; e; e = e->next ) {
			if ( n >= (int)ARRAY_LEN( flat ) ) break;
			flat[n++] = e;
		}
	}
	if ( n <= 0 ) return;
	qsort( flat, (size_t)n, sizeof( flat[0] ), RemapEntryCompare );

	fprintf( fp, "\t\t%s {\n", kw );
	for ( int i = 0; i < n; i++ ) {
		fprintf( fp, "\t\t\t\"%s\"\t\"%s\"\n", flat[i]->src, flat[i]->dst );
	}
	fprintf( fp, "\t\t}\n" );
}

static void EmitRemapSet(FILE *fp, const remap_set_t *s) {
	if ( RemapSet_IsEmpty( s ) ) return;
	fprintf( fp, "\tremap {\n" );
	EmitRemapKindBlock( fp, REMAP_KIND_SHADER,  s->tables[REMAP_KIND_SHADER]  );
	EmitRemapKindBlock( fp, REMAP_KIND_TEXTURE, s->tables[REMAP_KIND_TEXTURE] );
	EmitRemapKindBlock( fp, REMAP_KIND_SOUND,   s->tables[REMAP_KIND_SOUND]   );
	EmitRemapKindBlock( fp, REMAP_KIND_MUSIC,   s->tables[REMAP_KIND_MUSIC]   );
	fprintf( fp, "\t}\n" );
}

qboolean MetaEmit_Write(const char *out_dir,
                        const char *mapname,
                        const bsp_inventory_t *inv,
                        const resolution_t *resolutions,
                        int resolution_count)
{
	if ( !out_dir || !mapname || !inv ) return qfalse;

	map_meta_t meta;
	Meta_SetDefaults( &meta, mapname );

	if ( inv->worldspawn_message[0] ) {
		Q_strncpyz( meta.longname, inv->worldspawn_message,
		            sizeof( meta.longname ) );
	}

	for ( int i = 0; i < resolution_count; i++ ) {
		const resolution_t *r = &resolutions[i];
		if ( !r->resolved ) continue;
		const remap_kind_t rk = AssetKindToRemapKind( r->source.kind );
		if ( rk >= REMAP_KIND_COUNT ) continue;
		RemapSet_Add( &meta.remap, rk, r->source.path, r->replacement );
	}

	char out_path[ MAX_OSPATH ];
	Com_sprintf( out_path, sizeof( out_path ), "%s/%s.meta", out_dir, mapname );

	FILE *fp = fopen( out_path, "wb" );
	if ( !fp ) {
		fprintf( stderr, "extract-meta: MetaEmit_Write fopen failed: %s\n", out_path );
		return qfalse;
	}

	fprintf( fp, "{\n" );

	EmitString  ( fp, "map",        meta.mapname );
	EmitString  ( fp, "longname",   meta.longname );
	EmitInt     ( fp, "scorelimit", meta.scorelimit );
	EmitInt     ( fp, "timelimit",  meta.timelimit );
	EmitString  ( fp, "bots",       meta.bots );
	EmitGametypes( fp,             &meta.type );

	EmitString  ( fp, "series",     meta.series );
	EmitString  ( fp, "archetype",  meta.archetype );
	EmitString  ( fp, "author",     meta.author );
	EmitInt     ( fp, "year",       meta.year );
	EmitString  ( fp, "quote",      meta.quote );

	EmitInt     ( fp, "players_min", meta.players_min );
	EmitInt     ( fp, "players_max", meta.players_max );
	EmitString  ( fp, "weapon",      meta.meta_weapon );
	EmitInt     ( fp, "item_nodes",  meta.item_nodes );

	EmitRemapSet( fp, &meta.remap );

	fprintf( fp, "}\n" );
	fclose( fp );

	// Round-trip check: read the file back via stdio (same path) and
	// hand the buffer to Meta_ParseFromBuffer. Avoids the engine FS
	// (Meta_ParseFromFile) which would only see homepath/baseq3.
	FILE *rfp = fopen( out_path, "rb" );
	if ( !rfp ) {
		fprintf( stderr, "extract-meta: round-trip fopen failed for %s\n", out_path );
		return qfalse;
	}
	fseek( rfp, 0, SEEK_END );
	const long rlen = ftell( rfp );
	fseek( rfp, 0, SEEK_SET );
	// 256 KB upper bound mirrors meta_parse.c's META_MAX_FILE_BYTES
	// (the constant is private to that TU; reproducing the value here
	// keeps the round-trip check honest for tool-emitted .meta files).
	if ( rlen <= 0 || rlen >= ( 256 * 1024 ) ) {
		fclose( rfp );
		fprintf( stderr, "extract-meta: round-trip size invalid (%ld) for %s\n",
		         rlen, out_path );
		return qfalse;
	}
	char *buf = (char *)malloc( (size_t)rlen + 1 );
	if ( !buf ) { fclose( rfp ); return qfalse; }
	if ( fread( buf, 1, (size_t)rlen, rfp ) != (size_t)rlen ) {
		free( buf ); fclose( rfp );
		fprintf( stderr, "extract-meta: round-trip short read for %s\n", out_path );
		return qfalse;
	}
	buf[ rlen ] = '\0';
	fclose( rfp );

	map_meta_t check;
	Meta_SetDefaults( &check, mapname );
	const qboolean ok = Meta_ParseFromBuffer( &check, buf, out_path );
	free( buf );
	if ( !ok ) {
		fprintf( stderr, "extract-meta: round-trip parse FAILED for %s\n", out_path );
		return qfalse;
	}
	return qtrue;
}
