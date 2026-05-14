// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// bg_gametypes.c -- gametype definition table (shared by cgame, game, ui)

#include "../qcommon/q_shared.h"
#include "bg_public.h"

// parse token lists (NULL-terminated)
static char *gt_dm_tokens[]        = { "dm", "ffa", NULL };
static char *gt_duel_tokens[]      = { "duel", "tourney", "tournament", NULL };
static char *gt_koth_tokens[]      = { "koth", "ffa", NULL };
static char *gt_lms_tokens[]       = { "lms", "lastman", NULL };
static char *gt_tdm_tokens[]       = { "tdm", "team", NULL };
static char *gt_ctf_tokens[]       = { "ctf", NULL };
static char *gt_1fctf_tokens[]     = { "1fctf", "oneflag", "ctf", NULL };
static char *gt_obelisk_tokens[]   = { "overload", "obelisk", "ob", NULL };
static char *gt_harvester_tokens[] = { "harvester", "harv", NULL };
#if FEAT_FREEZETAG
static char *gt_freezetag_tokens[] = { "freezetag", "freeze", "ft", NULL };
#endif

// Gametype definitions — indexed by gametype_t
ggametype_t	bg_gametypelist[] =
{
	// GT_DEATHMATCH
	{
		/* name                 */ "Deathmatch",
		/* shortname            */ "dm",
		/* parseTokens          */ gt_dm_tokens,
		/* hudToken             */ "gt_ffa",
	},

	// GT_DUEL
	{
		/* name                 */ "Duel",
		/* shortname            */ "duel",
		/* parseTokens          */ gt_duel_tokens,
		/* hudToken             */ "gt_duel",
	},

	// GT_KINGOFTHEHILL
	{
		/* name                 */ "King of the Hill",
		/* shortname            */ "koth",
		/* parseTokens          */ gt_koth_tokens,
		/* hudToken             */ "gt_ffa",
	},

	// GT_LASTMANSTANDING
	{
		/* name                 */ "Last Man Standing",
		/* shortname            */ "lms",
		/* parseTokens          */ gt_lms_tokens,
		/* hudToken             */ "gt_ffa",
	},

	// GT_TDM
	{
		/* name                 */ "Team Deathmatch",
		/* shortname            */ "tdm",
		/* parseTokens          */ gt_tdm_tokens,
		/* hudToken             */ "gt_tdm",
	},

	// GT_CTF
	{
		/* name                 */ "Capture the Flag",
		/* shortname            */ "ctf",
		/* parseTokens          */ gt_ctf_tokens,
		/* hudToken             */ "gt_ctf",
	},

	// GT_1FCTF
	{
		/* name                 */ "One Flag CTF",
		/* shortname            */ "1fctf",
		/* parseTokens          */ gt_1fctf_tokens,
		/* hudToken             */ "gt_ctf",
	},

	// GT_OBELISK
	{
		/* name                 */ "Overload",
		/* shortname            */ "ob",
		/* parseTokens          */ gt_obelisk_tokens,
		/* hudToken             */ "gt_ctf",
	},

	// GT_HARVESTER
	{
		/* name                 */ "Harvester",
		/* shortname            */ "harv",
		/* parseTokens          */ gt_harvester_tokens,
		/* hudToken             */ "gt_ctf",
	},

#if FEAT_FREEZETAG
	// GT_FREEZETAG
	{
		/* name                 */ "Freeze Tag",
		/* shortname            */ "ft",
		/* parseTokens          */ gt_freezetag_tokens,
		/* hudToken             */ "gt_freezetag",
	},
#endif
};

// Match a single token against every gametype's parseTokens list.
// Returns a bitmask with a bit set for each gametype that accepts the token.
int BG_GametypeBits( const char *token ) {
	int		bits = 0;
	int		i;

	for ( i = 0; i < GT_MAX_GAME_TYPE; i++ ) {
		char **t;
		for ( t = bg_gametypelist[i].parseTokens; *t; t++ ) {
			if ( Q_stricmp( token, *t ) == 0 ) {
				bits |= 1 << i;
				break;
			}
		}
	}

	return bits;
}

// Find the first gametype whose parseTokens list contains the given token.
// Returns the gametype_t value, or -1 if no match.
int BG_GametypeForToken( const char *token ) {
	int		i;

	for ( i = 0; i < GT_MAX_GAME_TYPE; i++ ) {
		char **t;
		for ( t = bg_gametypelist[i].parseTokens; *t; t++ ) {
			if ( Q_stricmp( token, *t ) == 0 ) {
				return i;
			}
		}
	}

	return -1;
}

qboolean BG_IsTeamGametype( gametype_t gametype ) {
	return gametype >= GT_TDM;
}
