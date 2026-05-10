/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

#include <string.h>

#include "q_shared.h"
#include "category_map.h"

// Prefix-rewrite rules. Walked in declaration order; first match wins.
// Each rule is (FROM, TO, kind): if src->path starts with FROM and
// src->kind matches, rewrite to TO + suffix-after-FROM.
static const struct {
	const char  *from;
	const char  *to;
	asset_kind_t kind;
} prefix_rules[] = {
	{ "textures/gothic_block/", "textures/base_wall/",  ASSET_KIND_TEXTURE },
	{ "textures/gothic_trim/",  "textures/base_trim/",  ASSET_KIND_TEXTURE },
	{ "textures/gothic_floor/", "textures/base_floor/", ASSET_KIND_TEXTURE },
	{ "textures/gothic_wall/",  "textures/base_wall/",  ASSET_KIND_TEXTURE },
	{ "textures/organics/",     "textures/base_trim/",  ASSET_KIND_TEXTURE },
	{ "sound/world/",           "sound/items/",         ASSET_KIND_SOUND   },
	{ "sound/feedback/",        "sound/misc/",          ASSET_KIND_SOUND   },
	{ NULL, NULL, ASSET_KIND_SHADER /* sentinel */ }
};

// Category-default fallbacks. NULL value means no default — asset
// stays unresolved if the prefix rewrite and first-in-dir steps both
// fail to produce an available substitute.
static const struct {
	asset_kind_t kind;
	const char  *fallback;
} category_defaults[] = {
	{ ASSET_KIND_TEXTURE, "textures/common/default" },
	{ ASSET_KIND_SOUND,   "sound/misc/null"         },
	{ ASSET_KIND_MUSIC,   NULL                      },
	{ ASSET_KIND_SHADER,  NULL                      },
};

qboolean CategoryMap_TryRewrite(const needed_asset_t *src,
                                char *out_path, size_t out_size)
{
	if ( !src || !out_path || out_size == 0 ) {
		return qfalse;
	}
	for ( int i = 0; prefix_rules[i].from != NULL; i++ ) {
		if ( prefix_rules[i].kind != src->kind ) {
			continue;
		}
		const size_t from_len = strlen( prefix_rules[i].from );
		if ( strncmp( src->path, prefix_rules[i].from, from_len ) != 0 ) {
			continue;
		}
		// Match. Build TO + suffix.
		Com_sprintf( out_path, out_size, "%s%s",
		             prefix_rules[i].to,
		             src->path + from_len );
		return qtrue;
	}
	return qfalse;
}

qboolean CategoryMap_TryDefault(asset_kind_t kind,
                                char *out_path, size_t out_size)
{
	if ( !out_path || out_size == 0 ) {
		return qfalse;
	}
	for ( size_t i = 0; i < ARRAY_LEN( category_defaults ); i++ ) {
		if ( category_defaults[i].kind != kind ) {
			continue;
		}
		if ( !category_defaults[i].fallback ) {
			return qfalse;
		}
		Q_strncpyz( out_path, category_defaults[i].fallback, out_size );
		return qtrue;
	}
	return qfalse;
}
