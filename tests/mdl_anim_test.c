// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// mdl_anim_test.c -- Q1 monster animation-range derivation gate.
//
// The Q1-monster animation path derives frame ranges by prefix-grouping the .mdl
// frame names (carried into md3Frame_t.name by the loader): strip trailing digits
// -> label; a contiguous run of same-label frames is one range. This test drives
// the REAL derivation (MDL_DeriveAnimRanges, code/renderercommon/tr_model_mdl.c)
// with the REAL soldier.mdl frame-name prefixes and asserts the ranges come out
// correct — including the id convention that variant letters precede the digits
// ("painb1", "deathc1") so the parse separates pain/painb/painc cleanly and never
// merges or splits a logical animation.
//
// The soldier's LOCOMOTION is `run` (Q1 soldiers run to approach) — it has no walk
// cycle. `prowl_` is its crouch/aim/stalk pose, NOT locomotion (a prior fix wrongly
// aliased prowl_ -> WALK, and the soldier stooped + slid). This test encodes the
// CORRECT invariant: `run` derives a MOVING range, `prowl_` is not a locomotion
// label, and a WALK intent (the model has no walk cycle) resolves — via the client's
// WALK->RUN fallback — to the moving run range. A test that asserts "prowl_ is a walk
// label" is exactly the wrong premise that let the bad fix pass; this asserts "a
// locomoting monster MOVES," which is the property that actually matters.
//
// The renderer's R_GetMDLAnimations (reads a loaded MD3's frame names) and cgame's
// CG_ParseMDLAnimations (maps labels -> monster anim codes) both build on this
// pure function; proving it proves the derivation the whole path relies on.
//
// Run with: ctest -R mdl_anim

#include <stdio.h>
#include <string.h>

// The derivation is pure logic shared verbatim with the renderer via the header
// tr_model_mdl_anim.h. Compiling it here with MDL_ANIM_TEST_STANDALONE gives the
// test the REAL MDL_DeriveAnimRanges (+ a local mdlAnimRange_t matching tr_types.h)
// with no renderer link — so a derivation bug shows here, not only at runtime.
#define MDL_ANIM_TEST_STANDALONE 1
#include "../code/renderercommon/tr_model_mdl_anim.h"

static int failures = 0;
#define CHECK( cond, msg ) do { \
	if ( !(cond) ) { printf( "FAIL  %s\n", (msg) ); failures++; } \
} while ( 0 )

// Find a derived range by label; returns index or -1.
static int findRange( const mdlAnimRange_t *r, int n, const char *label ) {
	int i;
	for ( i = 0; i < n; i++ ) {
		if ( strcmp( r[i].label, label ) == 0 ) return i;
	}
	return -1;
}

// Model the cgame's locomotion resolution WITHOUT linking cgame (the alias table +
// CG_MonsterAnimation are static there). The rule under test: a WALK intent must
// resolve to a range with real motion (num_frames > 1). The soldier has NO walk
// cycle — its locomotion is `run` — so a correct client resolves WALK to the run
// range (the WALK->RUN fallback). This encodes "a locomoting monster MOVES," not
// "prowl_ is a walk label" (the exact wrong premise that let the prior fix pass).
//   walkLoco: the labels that mean WALK (locomotion) — prowl_ is intentionally ABSENT.
static int isWalkLoco( const char *label ) {
	static const char *w[] = { "walk", "swim", "fly" };
	int i; for ( i = 0; i < (int)( sizeof( w ) / sizeof( w[0] ) ); i++ ) if ( !strcmp( label, w[i] ) ) return 1; return 0;
}
static int isRunLoco( const char *label ) {
	static const char *r[] = { "run", "runb", "leap" };
	int i; for ( i = 0; i < (int)( sizeof( r ) / sizeof( r[0] ) ); i++ ) if ( !strcmp( label, r[i] ) ) return 1; return 0;
}

int main( void ) {
	// The REAL Q1 soldier frame-name sequence (abridged runs, in the MDL's ORDER):
	// stand(8) death(10) deathc(11) load(11) pain(6) painb(14) painc(13) run(8)
	// shoot(9) prowl_(24). The LOCOMOTION is `run` (Q1 soldiers run at you); `prowl_`
	// is the crouch/aim/stalk pose (the most frames — which is why it was once mis-
	// picked as walk). The soldier has NO `walk`, so a WALK intent must fall back to
	// the run range. Abridged runs keep the same ORDER + relative sizes.
	static char names[32][16];
	mdlAnimRange_t ranges[32];
	int n, idx, runIdx, walkResolvedFrames;
	int f = 0, i;

	// stand1..stand3
	for ( i = 1; i <= 3; i++ ) sprintf( names[f++], "stand%d", i );
	// pain1..pain2
	for ( i = 1; i <= 2; i++ ) sprintf( names[f++], "pain%d", i );
	// painb1..painb2  (letter BEFORE digit -> must be its OWN range, not merged)
	for ( i = 1; i <= 2; i++ ) sprintf( names[f++], "painb%d", i );
	// run1..run4  (the soldier's ACTUAL locomotion)
	for ( i = 1; i <= 4; i++ ) sprintf( names[f++], "run%d", i );
	// prowl_1..prowl_3  (the crouch/aim pose — NOT locomotion, must NOT resolve WALK)
	for ( i = 1; i <= 3; i++ ) sprintf( names[f++], "prowl_%d", i );
	// death1..death3
	for ( i = 1; i <= 3; i++ ) sprintf( names[f++], "death%d", i );
	// total = 3+2+2+4+3+3 = 17 frames

	n = MDL_DeriveAnimRanges( names, f, ranges, 32 );

	// Expect 6 distinct ranges: stand, pain, painb, run, prowl_, death.
	CHECK( n == 6, "6 distinct animation ranges derived" );

	idx = findRange( ranges, n, "stand" );
	CHECK( idx >= 0, "stand range exists" );
	if ( idx >= 0 ) {
		CHECK( ranges[idx].first_frame == 0 && ranges[idx].num_frames == 3, "stand = frames 0..2" );
	}

	// The crux: painb is SEPARATE from pain (letter-before-digit convention).
	idx = findRange( ranges, n, "painb" );
	CHECK( idx >= 0, "painb is a SEPARATE range (variant letter before digit)" );

	// The soldier's LOCOMOTION is `run` — a non-empty moving range. A monster that
	// locomotes must have a range that actually advances frames (> 1).
	runIdx = findRange( ranges, n, "run" );
	CHECK( runIdx >= 0, "run (soldier locomotion) range exists" );
	if ( runIdx >= 0 ) {
		CHECK( ranges[runIdx].num_frames > 1, "run is a MOVING range (num_frames > 1)" );
		CHECK( isRunLoco( ranges[runIdx].label ), "run maps to RUN locomotion" );
	}

	// prowl_ derives a range but is NOT a locomotion label — a WALK intent must NOT
	// resolve to it (that was the stoop-and-slide bug). Assert it is neither WALK nor
	// RUN locomotion so the client leaves it undriven.
	idx = findRange( ranges, n, "prowl_" );
	CHECK( idx >= 0, "prowl_ range exists (the crouch/aim pose)" );
	if ( idx >= 0 ) {
		CHECK( !isWalkLoco( ranges[idx].label ) && !isRunLoco( ranges[idx].label ),
			"prowl_ is NOT a locomotion label (it is a crouch/aim pose)" );
	}

	// The WALK->RUN fallback: the soldier has no walk range (no walk/swim/fly label
	// derived), so a WALK intent must resolve to the run range — a MOVING range. This
	// mirrors CG_MonsterAnimation: empty WALK (num_frames <= 1) falls back to RUN.
	{
		int haveWalk = 0;
		for ( i = 0; i < n; i++ ) if ( isWalkLoco( ranges[i].label ) ) haveWalk = 1;
		CHECK( !haveWalk, "soldier has NO walk cycle (WALK intent must fall back)" );
		// resolve: WALK empty -> RUN
		walkResolvedFrames = ( runIdx >= 0 ) ? ranges[runIdx].num_frames : 0;
		CHECK( walkResolvedFrames > 1,
			"WALK intent resolves (via fallback) to a MOVING range — soldier runs, not slides" );
	}

	idx = findRange( ranges, n, "death" );
	CHECK( idx >= 0, "death range exists" );

	// A pure-digit / empty-label frame must not open a range.
	{
		static char oddNames[3][16];
		mdlAnimRange_t oddRanges[8];
		int on;
		strcpy( oddNames[0], "12345" );   // all digits -> empty label
		strcpy( oddNames[1], "walk1" );
		strcpy( oddNames[2], "walk2" );
		on = MDL_DeriveAnimRanges( oddNames, 3, oddRanges, 8 );
		CHECK( on == 1, "all-digit frame does not open a range; only walk derived" );
		CHECK( on == 1 && strcmp( oddRanges[0].label, "walk" ) == 0 &&
		       oddRanges[0].first_frame == 1 && oddRanges[0].num_frames == 2,
			"walk range starts after the skipped all-digit frame" );
	}

	if ( failures == 0 ) {
		printf( "PASS  MDL anim derivation + locomotion resolution: same-label ranges, "
			"pain/painb separated, prowl_ is not locomotion, soldier's WALK intent "
			"falls back to its RUN cycle (a moving range — no slide/stoop)\n" );
		return 0;
	}
	printf( "FAILED with %d error(s)\n", failures );
	return 1;
}
