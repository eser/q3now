/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_shared.h"
#include "qcommon.h"
#include "qfiles.h"
#include "maps/bsp.h"
#include "shader_index.h"
#include "bsp_inventory.h"

// Strip recognized sound-file extensions in place. Engine looks up
// noise/music keys against any of these, so the asset key in the
// inventory must be the bare base name.
static void StripSoundExtension(char *path) {
	const size_t n = strlen(path);
	if ( n >= 4 ) {
		const char *tail = path + n - 4;
		if ( !Q_stricmp( tail, ".wav" )
		  || !Q_stricmp( tail, ".ogg" )
		  || !Q_stricmp( tail, ".mp3" ) ) {
			path[ n - 4 ] = '\0';
		}
	}
}

static qboolean InventoryGrow(bsp_inventory_t *inv) {
	const int newcap = ( inv->capacity == 0 ) ? 64 : inv->capacity * 2;
	needed_asset_t *next = (needed_asset_t *)realloc(
		inv->entries, (size_t)newcap * sizeof( needed_asset_t ) );
	if ( !next ) return qfalse;
	inv->entries  = next;
	inv->capacity = newcap;
	return qtrue;
}

static qboolean InventoryHas(const bsp_inventory_t *inv,
                             asset_kind_t kind, const char *path) {
	for ( int i = 0; i < inv->count; i++ ) {
		if ( inv->entries[i].kind == kind
		  && !Q_stricmp( inv->entries[i].path, path ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

// Append an entry, dedup'd. Empty path or NULL skipped silently.
static void InventoryAdd(bsp_inventory_t *inv,
                         asset_kind_t kind, const char *path) {
	if ( !path || !*path ) return;
	if ( InventoryHas( inv, kind, path ) ) return;
	if ( inv->count >= inv->capacity ) {
		if ( !InventoryGrow( inv ) ) return;
	}
	inv->entries[ inv->count ].kind = kind;
	Q_strncpyz( inv->entries[ inv->count ].path, path,
	            sizeof( inv->entries[ inv->count ].path ) );
	inv->count++;
}

// Walk the entity lump, collecting noise/music keys and worldspawn
// message. The lump is a sequence of `{ key value key value ... }`
// blocks separated by whitespace; classname determines which keys
// are interesting. Defensively treats any structural surprise as
// "skip the rest of this entity block".
static void ScanEntities(bsp_inventory_t *inv,
                         const char *src, int src_len)
{
	(void)src_len;
	if ( !src || !*src ) return;

	ComParser  parser;
	const char *p = src;
	COM_BeginParseSession( &parser, "<bsp entities>" );

	while ( 1 ) {
		const char *tok = COM_Parse( &parser, &p );
		if ( !tok || !tok[0] ) break;
		if ( tok[0] != '{' ) continue;   /* skip stray tokens */

		// Inside an entity. Collect classname + interesting keys until '}'.
		char classname[ MAX_QPATH ];
		classname[0] = '\0';

		// Buffer interesting key/value pairs in a small fixed-size list;
		// classname might appear after them, so resolve at end-of-block.
		struct { char key[32]; char val[ MAX_QPATH ]; } pending[ 32 ];
		int                                              npending = 0;

		while ( 1 ) {
			const char *key = COM_Parse( &parser, &p );
			if ( !key || !key[0] ) goto entity_done;
			if ( key[0] == '}' ) break;

			char keybuf[ 32 ];
			Q_strncpyz( keybuf, key, sizeof( keybuf ) );

			const char *val = COM_Parse( &parser, &p );
			if ( !val || !val[0] ) goto entity_done;
			if ( val[0] == '}' ) break;

			if ( !Q_stricmp( keybuf, "classname" ) ) {
				Q_strncpyz( classname, val, sizeof( classname ) );
				continue;
			}
			if ( npending < 32 ) {
				Q_strncpyz( pending[ npending ].key, keybuf,
				            sizeof( pending[ npending ].key ) );
				Q_strncpyz( pending[ npending ].val, val,
				            sizeof( pending[ npending ].val ) );
				npending++;
			}
		}

		// Resolve pending keys against the classname.
		for ( int i = 0; i < npending; i++ ) {
			const char *k = pending[i].key;
			char        v[ MAX_QPATH ];
			Q_strncpyz( v, pending[i].val, sizeof( v ) );

			// noise / noise1..noise9
			if ( !Q_stricmp( k, "noise" )
			  || ( !Q_stricmpn( k, "noise", 5 ) && k[5] >= '1' && k[5] <= '9' && k[6] == '\0' ) ) {
				// Q3 convention: a leading '*' means the sound is
				// on the model (e.g. "*falling1"); not a real file
				// path. Engine resolves these via the player-model
				// sound table; tool ignores them.
				if ( v[0] == '*' ) continue;
				StripSoundExtension( v );
				InventoryAdd( inv, ASSET_KIND_SOUND, v );
				continue;
			}
			// music — worldspawn only
			if ( !Q_stricmp( k, "music" ) && !Q_stricmp( classname, "worldspawn" ) ) {
				StripSoundExtension( v );
				InventoryAdd( inv, ASSET_KIND_MUSIC, v );
				continue;
			}
			// worldspawn message → meta longname
			if ( !Q_stricmp( k, "message" ) && !Q_stricmp( classname, "worldspawn" ) ) {
				Q_strncpyz( inv->worldspawn_message, v,
				            sizeof( inv->worldspawn_message ) );
				continue;
			}
		}

entity_done: ;
	}
}

qboolean BspInventory_Build(const char *mapname,
                            const struct shader_index_s *idx,
                            bsp_inventory_t *out)
{
	if ( !mapname || !*mapname || !out ) return qfalse;
	memset( out, 0, sizeof( *out ) );

	char path[ MAX_QPATH ];
	Com_sprintf( path, sizeof( path ), "maps/%s.bsp", mapname );

	bspFile_t *bsp = NULL;
	if ( !BSP_Load( path, &bsp, BSP_LOAD_FLAG_RENDER_ONLY ) || !bsp ) {
		return qfalse;
	}

	// 1. Walk shader table. Each dshader_t.shader becomes:
	//    - the shader entry itself,
	//    - all of its stage_maps (if the shader is in our index), or
	//    - a same-named texture (bare-shader fallback).
	for ( int i = 0; i < bsp->numShaders; i++ ) {
		const char *sname = bsp->shaders[i].shader;
		if ( !sname || !*sname ) continue;
		// q3map sentinel for unassigned/null surfaces — not a real
		// asset reference. Engine substitutes the default shader at
		// load time, so neither the shader entry nor a bare-shader
		// texture fallback should land in the inventory.
		if ( !Q_stricmp( sname, "noshader" ) ) continue;
		InventoryAdd( out, ASSET_KIND_SHADER, sname );

		// Stage-map paths from a known shader become TEXTURE entries.
		// The bare-shader → same-named-image fallback is NOT mirrored
		// as a second TEXTURE row — that was producing duplicate
		// "shader X / texture X" rows in the unresolved listing.
		// Asset_IsAvailable for ASSET_KIND_SHADER now probes both
		// the shader index AND R_ImageResolves, so a single SHADER
		// entry covers both engine-side resolution paths.
		const shader_def_t *def = ShaderIndex_Find( idx, sname );
		if ( def && def->stage_map_count > 0 ) {
			for ( int s = 0; s < def->stage_map_count; s++ ) {
				if ( !def->stage_maps[s] ) continue;
				// Normalize on intake: strip the literal image
				// extension so downstream Asset_IsAvailable can do
				// extension-less shader lookups directly. Mirrors
				// the engine's R_FindShader which COM_StripExtension's
				// image references before its hash lookup.
				char stripped[ MAX_QPATH ];
				COM_StripExtension( def->stage_maps[s],
				                    stripped, sizeof( stripped ) );
				InventoryAdd( out, ASSET_KIND_TEXTURE, stripped );
			}
		}
	}

	// 2. Copy entity string out before BSP_Free reclaims it.
	if ( bsp->entityStringLength > 0 && bsp->entityString ) {
		out->entity_string = (char *)malloc( (size_t)bsp->entityStringLength + 1 );
		if ( out->entity_string ) {
			memcpy( out->entity_string, bsp->entityString, (size_t)bsp->entityStringLength );
			out->entity_string[ bsp->entityStringLength ] = '\0';
			out->entity_string_length = bsp->entityStringLength;
		}
	}

	// 3. Scan the (now tool-owned) entity string for sound/music refs.
	if ( out->entity_string ) {
		ScanEntities( out, out->entity_string, out->entity_string_length );
	}

	BSP_Free( bsp );
	return qtrue;
}

void BspInventory_Free(bsp_inventory_t *inv) {
	if ( !inv ) return;
	free( inv->entries );
	free( inv->entity_string );
	memset( inv, 0, sizeof( *inv ) );
}
