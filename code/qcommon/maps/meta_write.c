// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// meta_write.c -- canonical .meta file serializer.
//
// Writes a map_meta_t out as a fresh .meta file. Used by future migration
// tooling (.arena -> .meta bulk converter). The serializer always emits
// canonical key names; legacy aliases never appear in writer output.
//
// Field omission rules: zero-valued numeric fields and empty-string
// fields are skipped to keep the file lean. The remap block is omitted
// when the table is NULL or empty. The reader treats absence as "use
// default", so round-tripping defaults -> file -> parse yields a
// structurally identical map_meta_t.

#include "meta.h"
#include "../qcommon.h"
LOG_DECLARE_CHANNEL( ch_maps, "maps" );

static void WriteString( fileHandle_t f, const char *key, const char *value ) {
	if ( !value || !value[0] ) return;
	FS_Printf( f, "\t%s\t\"%s\"\n", key, value );
}

static void WriteInt( fileHandle_t f, const char *key, int value ) {
	if ( value == 0 ) return;
	FS_Printf( f, "\t%s\t%d\n", key, value );
}

static void WriteGametypes( fileHandle_t f, const meta_gametypes_t *gt ) {
	if ( !gt || gt->count <= 0 ) return;
	FS_Printf( f, "\ttype\t\"" );
	for ( int i = 0; i < gt->count; i++ ) {
		FS_Printf( f, "%s%s", ( i ? " " : "" ), gt->tokens[i] );
	}
	FS_Printf( f, "\"\n" );
}

// Compare two remap_entry_t* by their `src` string (lexical, case-folded).
// Used for stable, hand-edit-friendly diffs in serialized .meta files.
static int RemapEntryCompare( const void *a, const void *b ) {
	const remap_entry_t *ea = *(const remap_entry_t * const *)a;
	const remap_entry_t *eb = *(const remap_entry_t * const *)b;
	return Q_stricmp( ea->src, eb->src );
}

// Materialize all entries from a hash-bucketed table into a flat
// pointer array on the caller's stack, sorted by src. Returns the
// number of entries written. Caller passes a sufficiently large buffer.
static int RemapTable_FlattenSorted( const remap_table_t *t,
                                     const remap_entry_t **out, int cap ) {
	if ( !t || cap <= 0 ) return 0;
	int n = 0;
	for ( int b = 0; b < META_REMAP_BUCKETS; b++ ) {
		for ( const remap_entry_t *e = t->buckets[b]; e; e = e->next ) {
			if ( n >= cap ) return n;   // truncate gracefully
			out[n++] = e;
		}
	}
	qsort( out, n, sizeof( const remap_entry_t * ), RemapEntryCompare );
	return n;
}

static const char *RemapKindKeyword( remap_kind_t k ) {
	switch ( k ) {
		case REMAP_KIND_SHADER:  return "shaders";
		case REMAP_KIND_TEXTURE: return "textures";
		case REMAP_KIND_SOUND:   return "sounds";
		case REMAP_KIND_MUSIC:   return "music";
		default:                 return NULL;
	}
}

static void WriteRemapKindBlock( fileHandle_t f, remap_kind_t kind, const remap_table_t *t ) {
	if ( !t || t->count <= 0 ) return;
	const char *kw = RemapKindKeyword( kind );
	if ( !kw ) return;

	const remap_entry_t *entries[META_MAX_REMAPS];
	int n = RemapTable_FlattenSorted( t, entries, META_MAX_REMAPS );
	if ( n <= 0 ) return;

	FS_Printf( f, "\t\t%s {\n", kw );
	for ( int i = 0; i < n; i++ ) {
		FS_Printf( f, "\t\t\t\"%s\"\t\"%s\"\n", entries[i]->src, entries[i]->dst );
	}
	FS_Printf( f, "\t\t}\n" );
}

// Emits the typed `remap { shaders { ... } textures { ... } ... }`
// block in fixed kind order. Skips entirely empty sets and any kinds
// with zero entries.
static void WriteRemapSet( fileHandle_t f, const remap_set_t *s ) {
	if ( RemapSet_IsEmpty( s ) ) return;
	FS_Printf( f, "\tremap {\n" );
	WriteRemapKindBlock( f, REMAP_KIND_SHADER,  s->tables[REMAP_KIND_SHADER]  );
	WriteRemapKindBlock( f, REMAP_KIND_TEXTURE, s->tables[REMAP_KIND_TEXTURE] );
	WriteRemapKindBlock( f, REMAP_KIND_SOUND,   s->tables[REMAP_KIND_SOUND]   );
	WriteRemapKindBlock( f, REMAP_KIND_MUSIC,   s->tables[REMAP_KIND_MUSIC]   );
	FS_Printf( f, "\t}\n" );
}

qboolean Meta_WriteToFile( const map_meta_t *m, const char *path ) {
	if ( !m || !path || !*path ) return qfalse;

	fileHandle_t f = FS_FOpenFileWrite( path );
	if ( f == FS_INVALID_HANDLE ) {
		Com_Log( SEV_WARN, LOG_CH(ch_maps),
			"Meta_WriteToFile: cannot open '%s' for writing\n", path );
		return qfalse;
	}

	FS_Printf( f, "{\n" );

	// legacy/canonical .arena fields
	WriteString( f, "map",      m->mapname );
	WriteString( f, "longname", m->longname );
	WriteInt   ( f, "scorelimit", m->scorelimit );
	WriteInt   ( f, "timelimit",  m->timelimit );
	WriteString( f, "bots",     m->bots );
	WriteGametypes( f, &m->type );

	// presentation
	WriteString( f, "series",     m->series );
	WriteString( f, "archetype",  m->archetype );
	WriteString( f, "author",     m->author );
	WriteInt   ( f, "year",       m->year );
	WriteString( f, "quote",      m->quote );

	// balance hints
	WriteInt   ( f, "players_min", m->players_min );
	WriteInt   ( f, "players_max", m->players_max );
	WriteString( f, "weapon",      m->meta_weapon );
	WriteInt   ( f, "item_nodes",  m->item_nodes );

	// asset substitutions
	WriteRemapSet( f, &m->remap );

	FS_Printf( f, "}\n" );
	FS_FCloseFile( f );
	return qtrue;
}
