// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// sg_reloc_test.c -- Phase-6 load-fixup relocation gate: the two-phase load
// (Phase-A load-all -> Phase-B resolve) resolves every cross-reference to the
// CORRECT live slot, including forward-references.
//
// Compiles the REAL walker (g_save_serialize.c) + registry (g_save_registry.c) and
// builds a synthetic world of records with cross-references between them (record A
// points at record C that loads LATER — a forward reference; a NULL pointer; a
// self-reference). It serializes the world, then loads it back in two phases:
//   Phase-A: SG_ReadFields loads each record into its fixed-base slot, pointer
//            fields holding the raw index;
//   Phase-B: SG_ResolveIndices patches every pointer field to &base[index].
// Asserts every pointer lands on the RIGHT live slot (not a stale/wrong one),
// forward-references included, -1 stays NULL, and the string-intern callback
// stores the re-interned pointer.
//
// This is the mechanism the live SG_LoadWorld (g_save_world.c) drives over the real
// g_entities; proving it here proves the relocation the resume depends on.
//
// Run with: ctest -R sg_reloc

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define FEAT_PW_PORTAL            0
#define FEAT_OVERLOAD            0
#define FEAT_EARTHQUAKE_SYSTEM   1
#define FEAT_DESTROYABLE_MISSILES 0

#include "../code/game/g_save.h"
#include "../code/game/g_save_funcs.h"
#include "../code/game/g_save_localcbs.h"
#include "../code/game/g_save_serialize.h"

static int failures = 0;
#define CHECK( cond, msg ) do { \
	if ( !(cond) ) { printf( "FAIL  %s\n", (msg) ); failures++; } \
} while ( 0 )

// registry link deps (as in sg_serialize)
#define SG_STUB_DEF( fn ) void fn( void ) {}
SG_CALLBACK_LIST( SG_STUB_DEF )
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
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_trigger_q1, SG_Register_g_trigger_q1 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_mover_q1,   SG_Register_g_mover_q1 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_misc_q3,    SG_Register_g_misc_q3 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_misc_q1,    SG_Register_g_misc_q1 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_team,       SG_Register_g_team )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_target_q3,  SG_Register_g_target_q3 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_mover_q3,   SG_Register_g_mover_q3 )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_weapon,     SG_Register_g_weapon )
SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_missile,    SG_Register_g_missile )

// ── the synthetic world: an entity array where entities cross-reference ──────
// Each entity has an "enemy" pointer (an SG_ENTITY into the same array) + a string
// + a self-index for identity. A mover-with-target_ent / monster-with-enemy shape.
#define N_ENT 6
typedef struct {
	int         id;           // identity (which slot this is, for verification)
	const char *name;         // SG_STRING
	void       *enemy;        // SG_ENTITY -> another slot (or NULL)
} ent_t;

static ent_t world[N_ENT];   // the "g_entities" base

#define EOFS( m ) offsetof( ent_t, m )
static const saveField_t entFields[] = {
	{ EOFS( id ),    SG_INT,     0 },
	{ EOFS( name ),  SG_STRING,  0 },
	{ EOFS( enemy ), SG_ENTITY,  0 },
	{ 0, SG_NONE, 0 }
};

static void fillBases( sgRelocBases_t *b ) {
	memset( b, 0, sizeof( *b ) );
	b->entityBase = world; b->entityStride = sizeof( ent_t ); b->entityCount = N_ENT;
	// clients/items unused by this descriptor
}

// A trivial string-intern for the test: a static arena (the real load uses
// G_NewString). Returns a stable pointer to a copy of `content`.
static char sg_arena[4096];
static size_t sg_arenaUsed;
static char *test_intern( const char *content ) {
	size_t n = strlen( content ) + 1;
	char  *out;
	if ( sg_arenaUsed + n > sizeof( sg_arena ) ) return NULL;
	out = sg_arena + sg_arenaUsed;
	memcpy( out, content, n );
	sg_arenaUsed += n;
	return out;
}

int main( void ) {
	sgRelocBases_t bases;
	uint8_t        buf[2048];
	sgStream_t     s;
	int            i;
	ent_t          loaded[N_ENT];

	fillBases( &bases );

	// Build a world with cross-references, including a FORWARD reference (slot 0
	// points at slot 5, which serializes after 0), a NULL, and a self-reference.
	memset( world, 0, sizeof( world ) );
	for ( i = 0; i < N_ENT; i++ ) {
		world[i].id = 100 + i;
		world[i].name = ( i == 3 ) ? NULL : "ent_name";   // slot 3 has a NULL string
	}
	world[0].enemy = &world[5];   // FORWARD reference (0 -> 5)
	world[1].enemy = &world[0];   // backward reference (1 -> 0)
	world[2].enemy = &world[2];   // self reference
	world[3].enemy = NULL;         // NULL pointer
	world[4].enemy = &world[1];
	world[5].enemy = &world[3];

	// ── serialize the world (each entity's fields), framed by index ──
	SG_StreamInitWrite( &s, buf, sizeof( buf ) );
	for ( i = 0; i < N_ENT; i++ ) {
		SG_StreamWriteI32( &s, i );
		SG_WriteFields( entFields, &world[i], &bases, &s );
	}
	CHECK( !s.overflow, "world write did not overflow" );

	// ── PHASE-A: load every record into its fixed-base slot (pointers = indices) ─
	memset( loaded, 0xAB, sizeof( loaded ) );  // poison
	sg_arenaUsed = 0;
	SG_StreamInitRead( &s, buf, s.len );
	for ( i = 0; i < N_ENT; i++ ) {
		int idx = SG_StreamReadI32( &s );
		CHECK( idx == i, "entity index framing" );
		CHECK( SG_ReadFields( entFields, &loaded[idx], &bases, &s, test_intern ) == 1,
			"Phase-A load record" );
	}
	CHECK( !s.overflow, "Phase-A consumed exactly" );

	// After Phase-A: scalars + strings are live; pointer fields hold raw indices.
	for ( i = 0; i < N_ENT; i++ ) {
		CHECK( loaded[i].id == 100 + i, "id loaded (Phase-A)" );
	}
	CHECK( loaded[3].name == NULL, "NULL string stayed NULL (interned)" );
	CHECK( loaded[0].name != NULL && strcmp( loaded[0].name, "ent_name" ) == 0,
		"non-NULL string re-interned via callback" );
	// The forward-reference slot still holds the raw index 5 pre-resolve.
	CHECK( *(int32_t *)&loaded[0].enemy == 5, "pointer field holds raw index pre-resolve" );

	// ── PHASE-B: resolve every pointer against the fixed base (loaded[]) ──
	{
		sgRelocBases_t lb;
		memset( &lb, 0, sizeof( lb ) );
		lb.entityBase = loaded; lb.entityStride = sizeof( ent_t ); lb.entityCount = N_ENT;
		for ( i = 0; i < N_ENT; i++ ) {
			SG_ResolveIndices( entFields, &loaded[i], &lb );
		}
	}

	// ── the payoff: every cross-reference points at the CORRECT live slot ──
	CHECK( loaded[0].enemy == &loaded[5], "FORWARD ref 0->5 resolves to correct live slot" );
	CHECK( loaded[1].enemy == &loaded[0], "backward ref 1->0 resolves" );
	CHECK( loaded[2].enemy == &loaded[2], "self ref 2->2 resolves" );
	CHECK( loaded[3].enemy == NULL,        "NULL pointer stays NULL after resolve" );
	CHECK( loaded[4].enemy == &loaded[1], "ref 4->1 resolves" );
	CHECK( loaded[5].enemy == &loaded[3], "ref 5->3 resolves" );

	// And the resolved target's identity is the RIGHT one (not a shifted slot).
	CHECK( ((ent_t *)loaded[0].enemy)->id == 105, "0's enemy is the id-105 entity" );
	CHECK( ((ent_t *)loaded[4].enemy)->id == 101, "4's enemy is the id-101 entity" );

	if ( failures == 0 ) {
		printf( "PASS  Phase-6 relocation: two-phase load (load-all -> resolve) "
			"resolves every cross-ref to the correct live slot — forward/backward/"
			"self refs + NULL, strings re-interned\n" );
		return 0;
	}
	printf( "FAILED with %d error(s)\n", failures );
	return 1;
}
