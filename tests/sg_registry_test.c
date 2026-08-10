// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// sg_registry_test.c -- savegame callback registry round-trip + completeness gate
// (Phase-2 hard gate).
//
// Compiles the REAL resolver source (code/game/g_save_registry.c) against stub
// callback symbols, so it exercises the shipping SG_NameToFunction /
// SG_FunctionToName code — not a copy. The callback NAME lists are the single
// source shared with the game build: TIER 1 from SG_CALLBACK_LIST (g_save_funcs.h),
// TIER 2 from the SG_LOCAL_CB_<file> macros (g_save_localcbs.h). The game build
// separately guarantees each name is a real symbol (a typo'd name fails to link);
// this test guarantees the resolution ALGORITHM is a correct bijection over the
// full registered set, in both tiers and both directions, plus completeness.
//
// This file provides the callback stub symbols (void-void definitions of every
// registered name) AND is linked with g_save_registry.c, which contributes the
// real resolvers + central table. The four feature flags that gate some
// file-static callbacks are pinned to their default-shipping state so the expected
// set is deterministic.
//
// Run with: ctest -R sg_registry

#include <stdio.h>
#include <string.h>
#include <stddef.h>

// Pin the feature flags that gate file-static callbacks to the DEFAULT shipping
// state (q_feats.h: MISSIONPACK undefined -> portal + overload OFF; earthquake ON;
// destroyable-missiles OFF). The stub sub-lists below are built with these pins,
// so the expected total below is the default-build registered set exactly.
#define FEAT_PW_PORTAL            0
#define FEAT_OVERLOAD            0
#define FEAT_EARTHQUAKE_SYSTEM   1
#define FEAT_DESTROYABLE_MISSILES 0

#include "../code/game/g_save_funcs.h"
#include "../code/game/g_save_localcbs.h"

// ── stub callback symbols ────────────────────────────────────────────────────
// One distinct, addressable no-op per callback name. TIER 1: from SG_CALLBACK_LIST.
// Each stub has a unique address, which is exactly what the pointer<->name gate
// needs. Signature is irrelevant (the registry stores void*); void(void) is fine.
#define SG_STUB_DEF( fn ) void fn( void ) {}
SG_CALLBACK_LIST( SG_STUB_DEF )

// TIER 2 stubs: the file-static callbacks. In the game build these are `static`
// inside their .c file; here they are plain stubs so the sub-tables can take &Fn.
SG_LOCAL_CB_g_trigger_q1( SG_STUB_DEF )
SG_LOCAL_CB_g_mover_q1( SG_STUB_DEF )
SG_LOCAL_CB_g_misc_q3( SG_STUB_DEF )
SG_LOCAL_CB_g_misc_q1( SG_STUB_DEF )
SG_LOCAL_CB_g_team( SG_STUB_DEF )
SG_LOCAL_CB_g_target_q3( SG_STUB_DEF )
SG_LOCAL_CB_g_mover_q3( SG_STUB_DEF )
SG_LOCAL_CB_g_weapon( SG_STUB_DEF )
SG_LOCAL_CB_g_missile( SG_STUB_DEF )
#undef SG_STUB_DEF

// ── the file-local registration hooks (TIER 2), over the stub symbols ────────
// SG_DEFINE_LOCAL_REGISTRY builds a { name, &fn } sub-table + a SG_Register_<file>
// hook — the SAME macro the game files use. g_save_registry.c walks these hooks;
// linking them here reproduces the real registration exactly.
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_trigger_q1, SG_Register_g_trigger_q1 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_mover_q1,   SG_Register_g_mover_q1 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_misc_q3,    SG_Register_g_misc_q3 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_misc_q1,    SG_Register_g_misc_q1 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_team,       SG_Register_g_team )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_target_q3,  SG_Register_g_target_q3 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_mover_q3,   SG_Register_g_mover_q3 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_weapon,     SG_Register_g_weapon )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_missile,    SG_Register_g_missile )

// ── the expected name set (every registered callback, both tiers) ────────────
// Built from the very same macros, so the "expected" list can never silently
// diverge from what is registered — this table's job is to drive the round-trip
// over each name and to count the total.
#define SG_NAME_ROW( fn ) #fn,
static const char *sg_expectedNames[] = {
	SG_CALLBACK_LIST( SG_NAME_ROW )
	SG_LOCAL_CB_g_trigger_q1( SG_NAME_ROW )
	SG_LOCAL_CB_g_mover_q1( SG_NAME_ROW )
	SG_LOCAL_CB_g_misc_q3( SG_NAME_ROW )
	SG_LOCAL_CB_g_misc_q1( SG_NAME_ROW )
	SG_LOCAL_CB_g_team( SG_NAME_ROW )
	SG_LOCAL_CB_g_target_q3( SG_NAME_ROW )
	SG_LOCAL_CB_g_mover_q3( SG_NAME_ROW )
	SG_LOCAL_CB_g_weapon( SG_NAME_ROW )
	SG_LOCAL_CB_g_missile( SG_NAME_ROW )
};
#undef SG_NAME_ROW
static const size_t sg_numExpected =
	sizeof( sg_expectedNames ) / sizeof( sg_expectedNames[0] );

// With the default flags pinned above: central 65 + trigger 18 + mover_q1 18 +
// misc_q3 3 (portal off, drops 3) + misc_q1 6 + team 0 (overload off, drops 5) +
// target_q3 2 (location + earthquake) + mover_q3 1 + weapon 1 + missile 0
// (destroyable off) = 114. Enabling MISSIONPACK re-adds portal(3)+obelisk(5)=8 ->
// 122; that build variant is covered by the game's own compile, not this gate.
#define SG_EXPECTED_TOTAL 114

int main( void ) {
	size_t i, j;
	int failures = 0;

	printf( "sg_registry_test: %zu registered names (expect %d)\n",
		sg_numExpected, SG_EXPECTED_TOTAL );

	if ( sg_numExpected != SG_EXPECTED_TOTAL ) {
		printf( "FAIL  registered-name count %zu != expected %d\n",
			sg_numExpected, SG_EXPECTED_TOTAL );
		failures++;
	}

	// (1) name -> ptr -> name round-trip, for every registered name.
	for ( i = 0; i < sg_numExpected; i++ ) {
		const char *name = sg_expectedNames[i];
		void *ptr = SG_NameToFunction( name );
		if ( ptr == NULL ) {
			printf( "FAIL  SG_NameToFunction(\"%s\") == NULL (name not registered)\n", name );
			failures++;
			continue;
		}
		const char *back = SG_FunctionToName( ptr );
		if ( strcmp( back, name ) != 0 ) {
			printf( "FAIL  round-trip name: \"%s\" -> ptr -> \"%s\"\n", name, back );
			failures++;
		}
	}

	// (2) ptr -> name -> ptr round-trip, for every registered name's pointer.
	for ( i = 0; i < sg_numExpected; i++ ) {
		void *ptr = SG_NameToFunction( sg_expectedNames[i] );
		if ( ptr == NULL ) {
			continue; // already reported in (1)
		}
		const char *name = SG_FunctionToName( ptr );
		void *back = SG_NameToFunction( name );
		if ( back != ptr ) {
			printf( "FAIL  round-trip ptr: %s -> \"%s\" -> different ptr\n",
				sg_expectedNames[i], name );
			failures++;
		}
	}

	// (3) distinctness: every registered pointer is unique (no two names alias the
	// same address — a copy/paste bug would fuse two callbacks).
	for ( i = 0; i < sg_numExpected; i++ ) {
		void *pi = SG_NameToFunction( sg_expectedNames[i] );
		for ( j = i + 1; j < sg_numExpected; j++ ) {
			void *pj = SG_NameToFunction( sg_expectedNames[j] );
			if ( pi != NULL && pi == pj ) {
				printf( "FAIL  distinct: \"%s\" and \"%s\" resolve to the same ptr\n",
					sg_expectedNames[i], sg_expectedNames[j] );
				failures++;
			}
		}
	}

	// (4) NULL / empty / unknown handling (the "no callback" + version-signal paths).
	if ( SG_NameToFunction( NULL ) != NULL ) {
		printf( "FAIL  SG_NameToFunction(NULL) != NULL\n" ); failures++;
	}
	if ( SG_NameToFunction( "" ) != NULL ) {
		printf( "FAIL  SG_NameToFunction(\"\") != NULL\n" ); failures++;
	}
	if ( SG_NameToFunction( "NoSuchCallback_xyzzy" ) != NULL ) {
		printf( "FAIL  unknown name resolved to non-NULL (should be NULL / HALT signal)\n" );
		failures++;
	}
	if ( SG_FunctionToName( NULL )[0] != '\0' ) {
		printf( "FAIL  SG_FunctionToName(NULL) != \"\"\n" ); failures++;
	}
	{
		// an unregistered non-NULL address -> "" (not a stale name).
		int local;
		if ( SG_FunctionToName( &local )[0] != '\0' ) {
			printf( "FAIL  SG_FunctionToName(unknown ptr) != \"\"\n" ); failures++;
		}
	}

	// (5) the mandatory hardest case must resolve (Q3_ReturnToPos1 — a mover think
	// reassigned at runtime, not re-derivable from moverState).
	if ( SG_NameToFunction( "Q3_ReturnToPos1" ) == NULL ) {
		printf( "FAIL  Q3_ReturnToPos1 not registered (the canonical need-registry case)\n" );
		failures++;
	}

	if ( failures == 0 ) {
		printf( "PASS  all %zu callbacks round-trip in both tiers + directions; "
			"completeness, distinctness, and NULL/unknown paths verified\n",
			sg_numExpected );
		return 0;
	}
	printf( "FAILED with %d error(s)\n", failures );
	return 1;
}
