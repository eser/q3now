// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// meta_parse.c -- tokenizer-driven parser for the .meta format.
//
// Grammar:
//   file        := '{' kv_or_block* '}'
//   kv_or_block := IDENT ( block | flat_value )
//   block       := '{' kv_pair* '}'
//   kv_pair     := STRING STRING
//   flat_value  := STRING
//
// Built on COM_Parse / COM_ParseExt (no new tokenizer). Unknown flat keys
// are silently ignored for forward compatibility (matches Q3 .arena
// behavior). Unknown nested blocks are skipped via a brace-depth counter.

#include "meta.h"
#include "../qcommon.h"
LOG_DECLARE_CHANNEL( ch_maps, "maps" );

#define META_MAX_FILE_BYTES (256 * 1024)

// ── helpers ─────────────────────────────────────────────────────────────

// Walk past a `{ ... }` block whose body we don't care about. Brace-depth
// counter; tolerates nested unknown blocks.
static void Meta_SkipBlock( ComParser *parser, const char **p ) {
	int depth = 1;
	while ( depth > 0 ) {
		const char *t = COM_ParseExt( parser, p, qtrue );
		if ( !t[0] ) return;          // EOF -- defensive bail-out
		if ( !strcmp( t, "{" ) )      depth++;
		else if ( !strcmp( t, "}" ) ) depth--;
	}
}

// Atoi with empty-string protection (returns 0). Defensive against
// COM_ParseExt returning "" on EOF when a value was expected.
static int Meta_AtoiSafe( const char *s ) {
	if ( !s || !s[0] ) return 0;
	return atoi( s );
}

// Tokenize a (possibly multi-token) gametype list. Each token is
// normalized through Meta_NormalizeGametype before being stored.
// Examples:
//   "ffa duel team"      -> { dm, duel, tdm }
//   "\"ffa tourney\""    -> { dm, duel }   (the quoted string is re-parsed)
static void Meta_ParseTypeList( meta_gametypes_t *out, const char *value ) {
	if ( !out || !value || !*value ) return;

	out->count = 0;
	const char *p = value;
	ComParser   inner = { 0 };

	while ( out->count < META_MAX_GAMETYPES ) {
		const char *tok = COM_ParseExt( &inner, &p, qfalse );
		if ( !tok[0] ) break;
		const char *canon = Meta_NormalizeGametype( tok );
		if ( !canon || !*canon ) continue;
		Q_strncpyz( out->tokens[out->count], canon, META_GAMETYPE_LEN );
		out->count++;
	}
}

// ── flat-key dispatch ───────────────────────────────────────────────────
//
// Called for each `KEY VALUE` pair at the top level of a .meta file.
// The key has not yet been normalized; we do that here so that legacy
// names (e.g. "fraglimit") and canonical names (e.g. "scorelimit") both
// reach the same field.
//
// Unknown keys are silently ignored — preserves Q3's forward-compat
// behavior when older builds parse newer .arena files.
static void Meta_DispatchFlatKey( map_meta_t *m, const char *raw_key, const char *value, const char *path_for_errors ) {
	const char *key = Meta_NormalizeKey( raw_key );

	if ( !Q_stricmp( key, "map" ) ) {
		// For .arena/.meta files that name their map; we usually ignore
		// since Maps_LoadMetaFor sets `mapname` from the filesystem.
		// But if the struct has an empty mapname (defensive), populate.
		if ( !m->mapname[0] ) {
			Q_strncpyz( m->mapname, value, sizeof( m->mapname ) );
		}
	}
	else if ( !Q_stricmp( key, "longname" ) ) {
		Q_strncpyz( m->longname, value, sizeof( m->longname ) );
	}
	else if ( !Q_stricmp( key, "scorelimit" ) ) {
		m->scorelimit = Meta_AtoiSafe( value );
	}
	else if ( !Q_stricmp( key, "timelimit" ) ) {
		m->timelimit = Meta_AtoiSafe( value );
	}
	else if ( !Q_stricmp( key, "bots" ) ) {
		Q_strncpyz( m->bots, value, sizeof( m->bots ) );
	}
	else if ( !Q_stricmp( key, "type" ) ) {
		Meta_ParseTypeList( &m->type, value );
	}
	else if ( !Q_stricmp( key, "series" ) ) {
		Q_strncpyz( m->series, value, sizeof( m->series ) );
	}
	else if ( !Q_stricmp( key, "archetype" ) ) {
		Q_strncpyz( m->archetype, value, sizeof( m->archetype ) );
	}
	else if ( !Q_stricmp( key, "author" ) ) {
		Q_strncpyz( m->author, value, sizeof( m->author ) );
	}
	else if ( !Q_stricmp( key, "year" ) ) {
		m->year = Meta_AtoiSafe( value );
	}
	else if ( !Q_stricmp( key, "quote" ) ) {
		Q_strncpyz( m->quote, value, sizeof( m->quote ) );
	}
	else if ( !Q_stricmp( key, "players_min" ) ) {
		m->players_min = Meta_AtoiSafe( value );
	}
	else if ( !Q_stricmp( key, "players_max" ) ) {
		m->players_max = Meta_AtoiSafe( value );
	}
	else if ( !Q_stricmp( key, "weapon" ) || !Q_stricmp( key, "meta_weapon" ) ) {
		Q_strncpyz( m->meta_weapon, value, sizeof( m->meta_weapon ) );
	}
	else if ( !Q_stricmp( key, "item_nodes" ) ) {
		m->item_nodes = Meta_AtoiSafe( value );
	}
	else {
		// Forward-compat: log only in developer builds.
		Com_Log( SEV_DEBUG, LOG_CH(ch_maps),
			"Meta_Parse(%s): ignoring unknown key '%s'\n",
			path_for_errors ? path_for_errors : "(buf)", raw_key );
	}

	// Last-wins note: if both a legacy alias and its canonical name appear
	// in the same file, the later occurrence overwrites. We log in
	// developer mode so authors can spot accidental dual-key usage.
	if ( Q_stricmp( raw_key, key ) != 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_maps),
			"Meta_Parse(%s): legacy key '%s' aliased to '%s'\n",
			path_for_errors ? path_for_errors : "(buf)", raw_key, key );
	}
}

// ── nested-block dispatch ──────────────────────────────────────────────

// Parse the body of a `remap { ... }` block. Reads `src dst` token pairs
// until a closing brace or EOF.
// Map a kind keyword to its remap_kind_t. Returns -1 for unknown.
static int Meta_KindFromKeyword( const char *kw ) {
	if ( !kw ) return -1;
	if ( !Q_stricmp( kw, "shaders" )  ) return REMAP_KIND_SHADER;
	if ( !Q_stricmp( kw, "textures" ) ) return REMAP_KIND_TEXTURE;
	if ( !Q_stricmp( kw, "sounds" )   ) return REMAP_KIND_SOUND;
	if ( !Q_stricmp( kw, "music" )    ) return REMAP_KIND_MUSIC;
	return -1;
}

// Read `src dst` pairs into the given kind sub-table until `}` or EOF.
static void Meta_ParseRemapKindBody( map_meta_t *m, remap_kind_t kind,
                                     ComParser *parser, const char **p,
                                     const char *path_for_errors ) {
	while ( 1 ) {
		const char *tok = COM_ParseExt( parser, p, qtrue );
		if ( !tok[0] ) {
			Com_Log( SEV_WARN, LOG_CH(ch_maps),
				"Meta_Parse(%s): EOF inside remap kind body\n",
				path_for_errors ? path_for_errors : "(buf)" );
			return;
		}
		if ( !strcmp( tok, "}" ) ) return;

		char src[MAX_QPATH];
		Q_strncpyz( src, tok, sizeof( src ) );

		const char *dst = COM_ParseExt( parser, p, qfalse );
		if ( !dst[0] ) {
			Com_Log( SEV_WARN, LOG_CH(ch_maps),
				"Meta_Parse(%s): unpaired remap entry '%s'\n",
				path_for_errors ? path_for_errors : "(buf)", src );
			return;
		}
		RemapSet_Add( &m->remap, kind, src, dst );
	}
}

// Dispatcher for the body of a `remap { ... }` block. Reads kind
// keywords (shaders/textures/sounds/music) followed by `{ src dst ... }`
// blocks. Unknown kind keywords are skipped via brace depth (forward
// compat). The legacy single-block grammar (raw `src dst` pairs at the
// top level of `remap`) is no longer accepted: when the next-after-kw
// token is not `{`, we log a warning, drain the rest of the remap
// block, and return — the rest of the .meta still parses normally.
static void Meta_ParseRemapBlock( map_meta_t *m, ComParser *parser,
                                  const char **p, const char *path_for_errors ) {
	while ( 1 ) {
		const char *tok = COM_ParseExt( parser, p, qtrue );
		if ( !tok[0] ) {
			Com_Log( SEV_WARN, LOG_CH(ch_maps),
				"Meta_Parse(%s): EOF inside remap { ... } block\n",
				path_for_errors ? path_for_errors : "(buf)" );
			return;
		}
		if ( !strcmp( tok, "}" ) ) return;

		// `tok` should be a kind keyword. Save the kind name so we can
		// log it if rejected.
		char kind_kw[MAX_TOKEN_CHARS];
		Q_strncpyz( kind_kw, tok, sizeof( kind_kw ) );

		const char *next = COM_ParseExt( parser, p, qtrue );
		if ( !next[0] || strcmp( next, "{" ) != 0 ) {
			// Legacy single-block grammar (`"src" "dst"` pairs without
			// kind keyword + `{`) — log and drain until matching `}`.
			Com_Log( SEV_WARN, LOG_CH(ch_maps),
				"Meta_Parse(%s): old-format remap block detected at "
				"token '%s' (expected a kind keyword like 'shaders {'). "
				"Skipping this remap block; rest of file parses normally.\n",
				path_for_errors ? path_for_errors : "(buf)", kind_kw );

			// Drain the outer remap block. Token `next` was either `}`
			// or another flat token; treat it as part of the body and
			// keep reading until we see the closing brace.
			int depth = 1;
			if ( next[0] && !strcmp( next, "}" ) ) depth = 0;
			while ( depth > 0 ) {
				const char *t = COM_ParseExt( parser, p, qtrue );
				if ( !t[0] ) return;
				if ( !strcmp( t, "{" ) )      depth++;
				else if ( !strcmp( t, "}" ) ) depth--;
			}
			return;
		}

		int kind = Meta_KindFromKeyword( kind_kw );
		if ( kind < 0 ) {
			// Unknown kind keyword — skip the inner block, keep going.
			Com_Log( SEV_DEBUG, LOG_CH(ch_maps),
				"Meta_Parse(%s): skipping unknown remap kind '%s'\n",
				path_for_errors ? path_for_errors : "(buf)", kind_kw );
			Meta_SkipBlock( parser, p );
			continue;
		}

		Meta_ParseRemapKindBody( m, (remap_kind_t)kind, parser, p, path_for_errors );
	}
}

// Dispatch nested-block by name. Unknown blocks are skipped.
static void Meta_DispatchBlock( map_meta_t *m, const char *block_name, ComParser *parser, const char **p, const char *path_for_errors ) {
	if ( !Q_stricmp( block_name, "remap" ) ) {
		Meta_ParseRemapBlock( m, parser, p, path_for_errors );
		return;
	}

	// Unknown block — skip to keep the parser in sync.
	Com_Log( SEV_DEBUG, LOG_CH(ch_maps),
		"Meta_Parse(%s): skipping unknown block '%s'\n",
		path_for_errors ? path_for_errors : "(buf)", block_name );
	Meta_SkipBlock( parser, p );
}

// ── public API ─────────────────────────────────────────────────────────

void Meta_SetDefaults( map_meta_t *out, const char *mapname ) {
	if ( !out ) return;
	memset( out, 0, sizeof( *out ) );
	if ( mapname ) {
		Q_strncpyz( out->mapname, mapname, sizeof( out->mapname ) );
	}
	out->source = META_SOURCE_NONE;
}

qboolean Meta_ParseFromBuffer( map_meta_t *out, const char *buf, const char *path_for_errors ) {
	if ( !out || !buf ) return qfalse;

	// Caller is responsible for filling mapname before us; we only zero
	// the rest of the struct so a partial parse leaves clean fields.
	char savedMapname[MAX_QPATH];
	Q_strncpyz( savedMapname, out->mapname, sizeof( savedMapname ) );
	memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->mapname, savedMapname, sizeof( out->mapname ) );

	const char *p = buf;
	ComParser   parser = { 0 };

	// Top-level must open with `{`.
	const char *tok = COM_Parse( &parser, &p );
	if ( !tok[0] || strcmp( tok, "{" ) != 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_maps),
			"Meta_Parse(%s): expected '{' at top level, got '%s'\n",
			path_for_errors ? path_for_errors : "(buf)", tok );
		return qfalse;
	}

	while ( 1 ) {
		// Read either a key or `}`.
		tok = COM_ParseExt( &parser, &p, qtrue );
		if ( !tok[0] ) {
			Com_Log( SEV_WARN, LOG_CH(ch_maps),
				"Meta_Parse(%s): EOF before closing '}'\n",
				path_for_errors ? path_for_errors : "(buf)" );
			return qfalse;
		}
		if ( !strcmp( tok, "}" ) ) {
			return qtrue;   // success
		}

		// `tok` is a key; peek next to decide flat-vs-block.
		char key[MAX_TOKEN_CHARS];
		Q_strncpyz( key, tok, sizeof( key ) );

		// Save cursor to allow rewind for flat-value path.
		const char *savedCursor = p;
		ComParser   savedParser = parser;
		const char *peek = COM_ParseExt( &parser, &p, qtrue );

		if ( peek[0] && !strcmp( peek, "{" ) ) {
			// Block dispatch.
			Meta_DispatchBlock( out, key, &parser, &p, path_for_errors );
		} else {
			// Rewind and read a normal value.
			p = savedCursor;
			parser = savedParser;
			const char *value = COM_ParseExt( &parser, &p, qfalse );
			if ( !value[0] ) {
				Com_Log( SEV_WARN, LOG_CH(ch_maps),
					"Meta_Parse(%s): missing value for key '%s'\n",
					path_for_errors ? path_for_errors : "(buf)", key );
				return qfalse;
			}
			Meta_DispatchFlatKey( out, key, value, path_for_errors );
		}
	}
}

qboolean Meta_ParseFromFile( map_meta_t *out, const char *path ) {
	if ( !out || !path || !*path ) return qfalse;

	void *raw = NULL;
	int   len = FS_ReadFile( path, &raw );
	if ( len <= 0 || !raw ) {
		if ( raw ) FS_FreeFile( raw );
		return qfalse;
	}
	if ( len >= META_MAX_FILE_BYTES ) {
		Com_Log( SEV_WARN, LOG_CH(ch_maps),
			"Meta_Parse: '%s' exceeds %d bytes; skipping.\n",
			path, META_MAX_FILE_BYTES );
		FS_FreeFile( raw );
		return qfalse;
	}

	qboolean ok = Meta_ParseFromBuffer( out, (const char *)raw, path );
	FS_FreeFile( raw );
	return ok;
}

qboolean Meta_HasGametype( const map_meta_t *m, const char *canonical_gt ) {
	if ( !m || !canonical_gt || !*canonical_gt ) return qfalse;
	for ( int i = 0; i < m->type.count; i++ ) {
		if ( !Q_stricmp( m->type.tokens[i], canonical_gt ) ) return qtrue;
	}
	return qfalse;
}
