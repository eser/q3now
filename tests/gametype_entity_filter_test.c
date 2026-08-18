// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * Entity "gametype" key filtering — the rule that decides whether a map entity
 * tagged for specific gametypes survives G_SpawnGEntityFromSpawnVars
 * (code/game/entities/g_spawn.c).
 *
 * WHY THIS EXISTS. The filter used to be a bare substring search for the
 * gametype's shortname:
 *
 *     s = strstr( value, bg_gametypelist[g_gametype.integer].shortname );
 *
 * GT_1FCTF's shortname is "1fctf", but id/Team Arena content spells One Flag CTF
 * as "oneflag". arenam3 — the one shipped map with an authored neutral flag —
 * tags all three of its flags `"gametype" "oneflag"` / `"ctf oneflag"`, so every
 * one of them was FREED at spawn and the map reported "No team_CTF_neutralflag
 * in map" despite the BSP demonstrably containing one. Its 1FCTF round was
 * unplayable for a reason that looked like missing map content.
 *
 * The fix routes the decision through BG_GametypeBits, which matches whole
 * tokens against each gametype's parseTokens alias list. This test compiles the
 * REAL bg_gametypes.c table and matcher — not a copy — and pins both halves of
 * the contract:
 *
 *   1. every alias a shipped map actually uses resolves to its gametype, and
 *   2. matching is whole-token, so "ctf" no longer matches "1fctf" by substring.
 *
 * (2) is the quieter half and the reason strstr had to go: substring matching
 * makes containment mean equality, which silently mis-assigns entities between
 * related gametypes.
 */

#include "q_shared.h"
#include "bg_public.h"

#include <stdio.h>
#include <string.h>

/*
 * Q_strncpyz and Q_stricmp live in q_shared.c, which calls Com_Terminate and so
 * would drag the engine into this standalone TU. Only the two string helpers are
 * needed, and only Q_stricmp is on the path under test — it is reached through
 * the REAL BG_GametypeBits in the linked production bg_gametypes.c, which is the
 * logic this test exists to pin. These definitions supply the same semantics
 * (bounded, always-terminated copy; case-insensitive compare) without the engine.
 */
void Q_strncpyz( char *dest, const char *src, int destsize ) {
	if ( !dest || !src || destsize < 1 ) return;
	while ( --destsize > 0 && ( *dest++ = *src++ ) != '\0' )
		;
	*dest = '\0';
}

int Q_stricmp( const char *s1, const char *s2 ) {
	int c1, c2;
	if ( s1 == NULL ) return ( s2 == NULL ) ? 0 : -1;
	if ( s2 == NULL ) return 1;
	do {
		c1 = (unsigned char)*s1++;
		c2 = (unsigned char)*s2++;
		if ( c1 != c2 ) {
			if ( c1 >= 'a' && c1 <= 'z' ) c1 -= ( 'a' - 'A' );
			if ( c2 >= 'a' && c2 <= 'z' ) c2 -= ( 'a' - 'A' );
			if ( c1 != c2 ) return c1 < c2 ? -1 : 1;
		}
	} while ( c1 );
	return 0;
}

static int failures;

#define CHECK(expr) do { \
	if ( !(expr) ) { \
		fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		failures++; \
	} \
} while ( 0 )

/*
 * The production filter's decision, expressed exactly as g_spawn.c performs it:
 * split the space-separated token list and accept if ANY token names this
 * gametype. Kept in lockstep with that call site deliberately — the point of the
 * test is the token-vs-substring semantics, which live in BG_GametypeBits.
 */
static qboolean entity_survives( const char *gametypeKey, int gametype ) {
	const int	wantBit = 1 << gametype;
	char		tokens[MAX_TOKEN_CHARS];
	char		*s;

	Q_strncpyz( tokens, gametypeKey, sizeof( tokens ) );
	for ( s = strtok( tokens, " \t\r\n" ); s; s = strtok( NULL, " \t\r\n" ) ) {
		if ( BG_GametypeBits( s ) & wantBit ) {
			return qtrue;
		}
	}
	return qfalse;
}

int main( void ) {
	/* ---- The arenam3 regression, verbatim from its entity lump ------------ */
	/* Its neutral flag is tagged "oneflag"; red/blue are "ctf oneflag" and
	 * "oneflag ctf". All three must survive in GT_1FCTF, and the two team flags
	 * must also survive in GT_CTF. */
	CHECK( entity_survives( "oneflag",     GT_1FCTF ) );
	CHECK( entity_survives( "ctf oneflag", GT_1FCTF ) );
	CHECK( entity_survives( "oneflag ctf", GT_1FCTF ) );
	CHECK( entity_survives( "ctf oneflag", GT_CTF ) );
	CHECK( entity_survives( "oneflag ctf", GT_CTF ) );

	/* The neutral flag is 1FCTF-only: it must NOT appear in plain CTF. This is
	 * the assertion the old strstr could not make, because "oneflag" contains no
	 * "ctf" but "ctf oneflag" does — token matching is what separates them. */
	CHECK( !entity_survives( "oneflag", GT_CTF ) );

	/* ---- Whole-token matching, the substring trap ------------------------- */
	/* "1fctf" CONTAINS "ctf" as a substring. The asymmetry below is the whole
	 * point, and it is only expressible with token matching:
	 *
	 *   - "ctf" IS an accepted alias of GT_1FCTF (gt_1fctf_tokens lists it, so
	 *     1FCTF inherits a CTF map's furniture — deliberate, and the reason
	 *     arenam3's "ctf oneflag" flags work in both modes).
	 *   - "1fctf" is NOT an alias of GT_CTF. Under strstr it matched anyway,
	 *     because "1fctf" contains "ctf" — a 1FCTF-only entity leaked into plain
	 *     CTF. Token matching is what stops that. */
	CHECK( !entity_survives( "1fctf", GT_CTF ) );
	CHECK(  entity_survives( "1fctf", GT_1FCTF ) );
	CHECK(  entity_survives( "ctf",   GT_CTF ) );
	CHECK(  entity_survives( "ctf",   GT_1FCTF ) );

	/* ---- Other aliases shipped content relies on -------------------------- */
	CHECK( entity_survives( "ffa",        GT_DEATHMATCH ) );
	CHECK( entity_survives( "tourney",    GT_DUEL ) );
	CHECK( entity_survives( "team",       GT_TDM ) );
	CHECK( entity_survives( "tdm",        GT_TDM ) );

	/* A token no gametype claims must not resolve to one. */
	CHECK( !entity_survives( "nosuchgametype", GT_DEATHMATCH ) );
	CHECK( !entity_survives( "",               GT_DEATHMATCH ) );

	/* Whitespace-separated lists are parsed, not pattern-matched: extra spaces
	 * and tabs must not change the verdict. */
	CHECK( entity_survives( "  ctf \t oneflag  ", GT_1FCTF ) );

	/* ---- BG_GametypeBits directly ---------------------------------------- */
	/* A token shared by several gametypes reports every one of them. */
	CHECK( ( BG_GametypeBits( "oneflag" ) & ( 1 << GT_1FCTF ) ) != 0 );
	CHECK( ( BG_GametypeBits( "oneflag" ) & ( 1 << GT_CTF ) )   == 0 );
	CHECK( ( BG_GametypeBits( "ctf" )     & ( 1 << GT_CTF ) )   != 0 );
	/* "ctf" names BOTH CTF and 1FCTF (see the asymmetry note above); "1fctf"
	 * names only 1FCTF, which is exactly what substring matching got wrong. */
	CHECK( ( BG_GametypeBits( "ctf" )     & ( 1 << GT_1FCTF ) ) != 0 );
	CHECK( ( BG_GametypeBits( "1fctf" )   & ( 1 << GT_CTF ) )   == 0 );
	CHECK( BG_GametypeBits( "nosuchgametype" ) == 0 );

	/* Alias lookup is case-insensitive (Q_stricmp), as map content is not
	 * consistent about case. */
	CHECK( entity_survives( "OneFlag", GT_1FCTF ) );
	CHECK( entity_survives( "CTF",     GT_CTF ) );

	if ( failures ) {
		fprintf( stderr, "%d check(s) failed\n", failures );
		return 1;
	}
	printf( "gametype entity filter contract: all checks passed\n" );
	return 0;
}
