/*
===========================================================================
Copyright (C) 2024 Wired engine contributors

This file is part of Quake III Arena source code.
Released under GPLv2 — see meta.h for the full notice.
===========================================================================
*/

// meta_aliases.c -- legacy-to-canonical name normalization for .meta keys
// and gametype tokens. Linear scans over short tables; hot-path cost is
// negligible at parse time and zero at gameplay time.

#include "meta.h"
#include "../qcommon.h"

typedef struct {
	const char *legacy;
	const char *canonical;
} meta_alias_t;

static const meta_alias_t key_aliases[] = {
	{ "fraglimit", "scorelimit" },
	{ NULL,        NULL         }
};

static const meta_alias_t gametype_aliases[] = {
	{ "ffa",     "dm"   },
	{ "team",    "tdm"  },
	{ "tourney", "duel" },
	{ NULL,      NULL   }
};

static const char *NormalizeAgainst( const meta_alias_t *table, const char *in ) {
	if ( !in ) return NULL;
	for ( int i = 0; table[i].legacy; i++ ) {
		if ( !Q_stricmp( in, table[i].legacy ) ) {
			return table[i].canonical;
		}
	}
	return in;   // pass-through: already canonical or unknown
}

const char *Meta_NormalizeKey( const char *legacy_or_canonical ) {
	return NormalizeAgainst( key_aliases, legacy_or_canonical );
}

const char *Meta_NormalizeGametype( const char *legacy_or_canonical ) {
	return NormalizeAgainst( gametype_aliases, legacy_or_canonical );
}
