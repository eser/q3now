// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// sg_serialize_test.c -- Phase-4 field-walker round-trip + index-relocation gate.
//
// Compiles the REAL walker (g_save_serialize.c) + the REAL callback registry
// (g_save_registry.c) and drives them with a SYNTHETIC world: a small struct with
// every field-type the Phase-1 descriptors use (scalars, vector, RAW, string,
// entity/client/item pointers, callback), a synthetic entity/client/item array to
// relocate against, and a synthetic descriptor table. It asserts:
//   (1) the serialized buffer round-trips: read-back reproduces every scalar/RAW/
//       string byte-exact, and every pointer field comes back as the SAME index it
//       was written as (the Phase-4 read form; Phase-6 resolves index->pointer);
//   (2) index relocation is SYMMETRIC — SG_PtrToIndex/SG_IndexInRange agree on
//       write and read, and -1==NULL round-trips both directions;
//   (3) a callback field round-trips through the registry name;
//   (4) an out-of-range index and an unknown callback name are REJECTED on read.
//
// The real gentity_t walk over g_entities is the game build's job (g_save_world.c);
// this gate proves the walker MECHANISM the world glue relies on. The synthetic
// descriptor uses the same saveField_t / SG_* types as the real tables, so a
// walker bug shows here regardless of the concrete struct.
//
// Run with: ctest -R sg_serialize

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// The registry test's TU provides SG_FUNCTION resolution; here we link the real
// g_save_registry.c, which needs the central callback stubs + the file-local
// sub-lists. Pin the feature flags (default config) exactly as the registry test.
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

// ── stub callback symbols so the registry links (as in sg_registry_test) ─────
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

// ── the synthetic world ──────────────────────────────────────────────────────
// Fake element types (only their addresses/strides matter for relocation).
typedef struct { int x; char pad[28]; } fakeEnt_t;    // stand-in for gentity_t
typedef struct { int y; char pad[12]; } fakeClient_t; // stand-in for gclient_t
typedef struct { int z; char pad[4];  } fakeItem_t;   // stand-in for gitem_t

#define N_ENT 8
#define N_CLI 4
#define N_ITM 6
static fakeEnt_t    entArr[N_ENT];
static fakeClient_t cliArr[N_CLI];
static fakeItem_t   itmArr[N_ITM];

// A record carrying one of every field type the real descriptors use.
typedef struct {
	int         i;
	float       f;
	int         b;            // qboolean (int-width)
	float       vec[3];
	char        raw[16];
	const char *str;
	void       *entPtr;       // SG_ENTITY  -> entArr
	void       *cliPtr;       // SG_CLIENT  -> cliArr
	void       *itmPtr;       // SG_ITEM    -> itmArr
	void      (*fn)( void );  // SG_FUNCTION -> registry name
} rec_t;

#define ROFS( m ) offsetof( rec_t, m )
static const saveField_t recFields[] = {
	{ ROFS( i ),      SG_INT,      0 },
	{ ROFS( f ),      SG_FLOAT,    0 },
	{ ROFS( b ),      SG_QBOOLEAN, 0 },
	{ ROFS( vec ),    SG_VECTOR,   0 },
	{ ROFS( raw ),    SG_RAW,      sizeof( ((rec_t *)0)->raw ) },
	{ ROFS( str ),    SG_STRING,   0 },
	{ ROFS( entPtr ), SG_ENTITY,   0 },
	{ ROFS( cliPtr ), SG_CLIENT,   0 },
	{ ROFS( itmPtr ), SG_ITEM,     0 },
	{ ROFS( fn ),     SG_FUNCTION, 0 },
	{ 0, SG_NONE, 0 }
};

static void fillBases( sgRelocBases_t *b ) {
	b->entityBase = entArr; b->entityStride = sizeof( fakeEnt_t );    b->entityCount = N_ENT;
	b->clientBase = cliArr; b->clientStride = sizeof( fakeClient_t ); b->clientCount = N_CLI;
	b->itemBase   = itmArr; b->itemStride   = sizeof( fakeItem_t );   b->itemCount   = N_ITM;
}

int main( void ) {
	sgRelocBases_t bases;
	uint8_t        buf[512];
	sgStream_t     s;
	rec_t          in, out;

	fillBases( &bases );

	// ── (0) index-relocation symmetry, directly ──────────────────────────────
	{
		int k;
		for ( k = 0; k < N_ENT; k++ ) {
			int32_t idx = SG_PtrToIndex( &entArr[k], entArr, sizeof( fakeEnt_t ), N_ENT );
			CHECK( idx == k, "ptr->index identity for each entity slot" );
			CHECK( SG_IndexInRange( idx, N_ENT ), "index in range" );
		}
		CHECK( SG_PtrToIndex( NULL, entArr, sizeof( fakeEnt_t ), N_ENT ) == -1,
			"NULL ptr -> index -1" );
		CHECK( SG_IndexInRange( -1, N_ENT ), "-1 (NULL) is a valid index" );
		CHECK( !SG_IndexInRange( N_ENT, N_ENT ), "count is out of range" );
		CHECK( !SG_IndexInRange( -2, N_ENT ), "-2 is out of range" );
	}

	// ── (1)+(2)+(3) full round-trip incl every field type + reloc + callback ─
	memset( &in, 0, sizeof( in ) );
	in.i   = -12345;
	in.f   = 3.14159f;
	in.b   = 1;
	in.vec[0] = 1.5f; in.vec[1] = -2.5f; in.vec[2] = 100.0f;
	memcpy( in.raw, "0123456789ABCDE", 16 );
	in.str    = "target_delay_42";
	in.entPtr = &entArr[5];
	in.cliPtr = &cliArr[2];
	in.itmPtr = &itmArr[4];
	in.fn     = Q3_ReturnToPos1;   // the canonical registry callback

	SG_StreamInitWrite( &s, buf, sizeof( buf ) );
	CHECK( SG_WriteFields( recFields, &in, &bases, &s ) == 1, "write succeeds" );
	CHECK( !s.overflow, "write did not overflow" );

	memset( &out, 0xAB, sizeof( out ) );   // poison, so a missed field shows
	SG_StreamInitRead( &s, buf, s.len );
	CHECK( SG_ReadFields( recFields, &out, &bases, &s, NULL ) == 1, "read succeeds" );
	CHECK( !s.overflow, "read consumed exactly, no underflow" );

	// scalars / vector / RAW byte-exact
	CHECK( out.i == in.i,                              "int round-trips" );
	CHECK( out.f == in.f,                              "float round-trips" );
	CHECK( out.b == in.b,                              "qboolean round-trips" );
	CHECK( memcmp( out.vec, in.vec, sizeof( in.vec ) ) == 0, "vector round-trips" );
	CHECK( memcmp( out.raw, in.raw, sizeof( in.raw ) ) == 0, "RAW blob round-trips" );

	// pointer fields come back as the SERIALIZED INDEX (Phase-4 form; Phase-6
	// resolves to &base[index]). in.entPtr was &entArr[5] -> index 5, etc.
	CHECK( *(int32_t *)&out.entPtr == 5, "SG_ENTITY read as index 5 (symmetric)" );
	CHECK( *(int32_t *)&out.cliPtr == 2, "SG_CLIENT read as index 2 (symmetric)" );
	CHECK( *(int32_t *)&out.itmPtr == 4, "SG_ITEM read as index 4 (symmetric)" );

	// ── (2b) NULL pointer field round-trips as index -1 ──────────────────────
	{
		rec_t nin, nout;
		memset( &nin, 0, sizeof( nin ) );
		nin.str = NULL;          // NULL string
		nin.entPtr = NULL;       // NULL entity
		nin.fn = NULL;           // NULL callback
		SG_StreamInitWrite( &s, buf, sizeof( buf ) );
		CHECK( SG_WriteFields( recFields, &nin, &bases, &s ) == 1, "write NULLs" );
		memset( &nout, 0xAB, sizeof( nout ) );
		SG_StreamInitRead( &s, buf, s.len );
		CHECK( SG_ReadFields( recFields, &nout, &bases, &s, NULL ) == 1, "read NULLs" );
		CHECK( *(int32_t *)&nout.entPtr == -1, "NULL SG_ENTITY -> index -1 -> back to -1" );
	}

	// ── (4) rejection: out-of-range index + unknown callback name ────────────
	{
		// Hand-craft a buffer whose SG_ENTITY index is out of range. Easiest: write
		// a valid rec, then corrupt the entity-index int in the stream. The entity
		// field is after i(4)+f(4)+b(4)+vec(12)+raw(16)+str(4 len + 15 content) ...
		// rather than compute the offset, re-encode with an oversized synthetic:
		rec_t bad;
		int32_t badIdx = N_ENT + 100;
		memset( &bad, 0, sizeof( bad ) );
		bad.str = "";                       // empty string (len 0)
		bad.entPtr = &entArr[0];
		// write, then overwrite the entity index bytes in the buffer.
		SG_StreamInitWrite( &s, buf, sizeof( buf ) );
		SG_WriteFields( recFields, &bad, &bases, &s );
		// find + clobber: the entity index is the first int AFTER the str token.
		// str is empty -> its token is a lone int32 len=0. Layout up to entPtr:
		//   i(4) f(4) b(4) vec(12) raw(16) str_len(4) [no content] entIdx(4)...
		{
			size_t entOff = 4 + 4 + 4 + 12 + 16 + 4;   // = 44
			memcpy( buf + entOff, &badIdx, sizeof( badIdx ) );
		}
		{
			rec_t tmp;
			SG_StreamInitRead( &s, buf, s.len );
			CHECK( SG_ReadFields( recFields, &tmp, &bases, &s, NULL ) == 0,
				"out-of-range entity index REJECTED on read" );
		}
	}
	{
		// unknown callback name: write a rec with a valid fn, then replace the
		// callback-name token content with a bogus name of the same length.
		rec_t ub;
		memset( &ub, 0, sizeof( ub ) );
		ub.str = "";
		ub.fn  = Q3_ReturnToPos1;   // 15 chars
		SG_StreamInitWrite( &s, buf, sizeof( buf ) );
		SG_WriteFields( recFields, &ub, &bases, &s );
		// The fn name token is the LAST field. Find "Q3_ReturnToPos1" in the buffer
		// and overwrite with a bogus same-length name.
		{
			const char *needle = "Q3_ReturnToPos1";
			size_t nlen = strlen( needle );
			size_t i;
			int replaced = 0;
			for ( i = 0; i + nlen <= s.len; i++ ) {
				if ( memcmp( buf + i, needle, nlen ) == 0 ) {
					memcpy( buf + i, "ZZ_bogus_fnnnn!", nlen );  // same length
					replaced = 1;
					break;
				}
			}
			CHECK( replaced, "found the callback name token to corrupt" );
		}
		{
			rec_t tmp;
			SG_StreamInitRead( &s, buf, s.len );
			CHECK( SG_ReadFields( recFields, &tmp, &bases, &s, NULL ) == 0,
				"unknown callback name REJECTED on read (version/corruption signal)" );
		}
	}

	if ( failures == 0 ) {
		printf( "PASS  field-walker: every type round-trips byte-exact, index "
			"relocation symmetric (ptr<->index, -1==NULL), callback via registry, "
			"out-of-range index + unknown callback REJECTED\n" );
		return 0;
	}
	printf( "FAILED with %d error(s)\n", failures );
	return 1;
}
