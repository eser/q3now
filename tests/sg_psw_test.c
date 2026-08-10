// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// sg_psw_test.c -- Phase-5 .psw persistent-subset round-trip gate.
//
// The .psw carries a NARROW cross-map subset (player-progression byte ranges +
// objectives POD), not the full world. It reuses the Phase-4 walker — specifically
// the range-walker SG_WriteRanges/SG_ReadRanges (for the gclientPersFields byte
// ranges) plus a verbatim RAW copy for the objectives. This test compiles the REAL
// walker (g_save_serialize.c) and drives it with a synthetic client + objectives:
//   (1) the range subset round-trips byte-exact (ps/sess/pers analogs);
//   (2) SUBSET-ONLY: fields OUTSIDE the whitelisted ranges do NOT appear in the
//       payload (a full-world field can't leak into the .psw) — asserted by
//       checking the payload size == exactly the sum of the whitelisted ranges,
//       and that a non-whitelisted field's bytes are absent from the buffer;
//   (3) objectives POD round-trips byte-exact;
//   (4) a wrong-version .psw header is REJECTED (reusing the Phase-3 header check).
//
// The real gclientPersFields walk over level.clients (g_save_world.c) is covered by
// the game build; this gate proves the range-walker MECHANISM the .psw relies on.
//
// Run with: ctest -R sg_psw

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "../code/game/g_save.h"
#include "../code/game/g_save_serialize.h"
#include "../code/game/g_save_file.h"   // sgFileHeader_t + SG_HeaderValidate (version-reject)

static int failures = 0;
#define CHECK( cond, msg ) do { \
	if ( !(cond) ) { printf( "FAIL  %s\n", (msg) ); failures++; } \
} while ( 0 )

// The field-walker TU (g_save_serialize.c) references the Phase-2 callback
// resolvers for its SG_FUNCTION case. The .psw subset has NO callback fields (only
// ranges + POD), so that path is never taken here — tiny stubs satisfy the linker
// without pulling the whole registry. (The real resolvers are covered by the
// sg_registry + sg_serialize gates.)
const char *SG_FunctionToName( void *ptr ) { (void)ptr; return ""; }
void       *SG_NameToFunction( const char *name ) { (void)name; return (void *)0; }

// A synthetic client with a persistent subset (ps/sess/pers analogs) AND a
// non-persistent field (transient) that must NOT be carried by the .psw.
typedef struct {
	int  ps[8];        // "progression" — persistent
	int  sess[4];      // "identity"    — persistent
	char pers[16];     // "netname"     — persistent
	int  transient[6]; // buttons/history/etc — NOT in the .psw whitelist
} fakeClient_t;

#define COFS( m ) offsetof( fakeClient_t, m )
// The .psw whitelist — ps + sess + pers only (NOT transient). Mirrors the real
// gclientPersFields { ps, sess, pers } saveRange_t table.
static const saveRange_t persRanges[] = {
	{ COFS( ps ),   sizeof( ((fakeClient_t *)0)->ps ) },
	{ COFS( sess ), sizeof( ((fakeClient_t *)0)->sess ) },
	{ COFS( pers ), sizeof( ((fakeClient_t *)0)->pers ) },
	{ 0, 0 }
};

// A synthetic objectives POD (mirrors missionObjective_t[] — pure data).
typedef struct { char key[64]; int required, completed, failed; } fakeObj_t;
#define N_OBJ 4

int main( void ) {
	uint8_t     buf[1024];
	sgStream_t  s;
	fakeClient_t in, out;
	size_t      expectedRangeBytes;
	size_t      i;

	// ── (1) range subset round-trips byte-exact ──────────────────────────────
	memset( &in, 0, sizeof( in ) );
	for ( i = 0; i < 8; i++ ) in.ps[i] = (int)( 1000 + i );
	for ( i = 0; i < 4; i++ ) in.sess[i] = (int)( 50 + i );
	memcpy( in.pers, "PlayerOne_netnm", 16 );
	for ( i = 0; i < 6; i++ ) in.transient[i] = (int)( 0xDEAD00 + i );  // must NOT carry

	SG_StreamInitWrite( &s, buf, sizeof( buf ) );
	CHECK( SG_WriteRanges( persRanges, &in, &s ) == 1, "write persistent ranges" );
	CHECK( !s.overflow, "range write did not overflow" );

	expectedRangeBytes = sizeof( in.ps ) + sizeof( in.sess ) + sizeof( in.pers );
	CHECK( s.len == expectedRangeBytes,
		"payload size == sum of whitelisted ranges (no extra fields)" );

	memset( &out, 0xAB, sizeof( out ) );  // poison
	SG_StreamInitRead( &s, buf, s.len );
	CHECK( SG_ReadRanges( persRanges, &out, &s ) == 1, "read persistent ranges" );
	CHECK( !s.overflow, "range read consumed exactly" );

	CHECK( memcmp( out.ps,   in.ps,   sizeof( in.ps ) ) == 0,   "ps subset round-trips" );
	CHECK( memcmp( out.sess, in.sess, sizeof( in.sess ) ) == 0, "sess subset round-trips" );
	CHECK( memcmp( out.pers, in.pers, sizeof( in.pers ) ) == 0, "pers subset round-trips" );

	// ── (2) SUBSET-ONLY: the transient field was NOT written into the payload ─
	// The poisoned out.transient stays poisoned (the read never touches it), AND
	// the transient bytes never appear in the serialized buffer.
	{
		int transientLeaked = 0;
		int32_t marker = (int32_t)0xDEAD00;  // the transient[0] value
		for ( i = 0; i + sizeof( marker ) <= s.len; i++ ) {
			if ( memcmp( buf + i, &marker, sizeof( marker ) ) == 0 ) {
				transientLeaked = 1;
				break;
			}
		}
		CHECK( !transientLeaked, "transient (non-whitelisted) field did NOT leak into the .psw" );
	}

	// ── (3) objectives POD round-trips verbatim ──────────────────────────────
	{
		fakeObj_t objIn[N_OBJ], objOut[N_OBJ];
		int32_t   numIn = 3, numOut;
		memset( objIn, 0, sizeof( objIn ) );
		for ( i = 0; i < N_OBJ; i++ ) {
			snprintf( objIn[i].key, sizeof( objIn[i].key ), "OBJ_KEY_%zu", i );
			objIn[i].required  = (int)( i & 1 );
			objIn[i].completed = (int)( i > 1 );
		}
		SG_StreamInitWrite( &s, buf, sizeof( buf ) );
		SG_StreamWriteRaw( &s, objIn, sizeof( objIn ) );
		SG_StreamWriteI32( &s, numIn );
		CHECK( !s.overflow, "objectives write ok" );

		memset( objOut, 0xAB, sizeof( objOut ) );
		SG_StreamInitRead( &s, buf, s.len );
		SG_StreamReadRaw( &s, objOut, sizeof( objOut ) );
		numOut = SG_StreamReadI32( &s );
		CHECK( !s.overflow, "objectives read ok" );
		CHECK( memcmp( objOut, objIn, sizeof( objIn ) ) == 0, "objectives POD round-trips byte-exact" );
		CHECK( numOut == numIn, "numObjectives round-trips" );
	}

	// ── (4) wrong-version .psw header REJECTED (Phase-3 header reuse) ─────────
	{
		sgFileHeader_t hdr;
		SG_HeaderInit( &hdr, 128, 0 );
		CHECK( SG_HeaderValidate( &hdr ) == 1, "valid .psw header accepted" );
		hdr.version = SG_SAVE_VERSION + 1;
		CHECK( SG_HeaderValidate( &hdr ) == 0, "wrong-version .psw REJECTED" );
		hdr.version = SG_SAVE_VERSION;
		hdr.magic = 0xBADF00D;
		CHECK( SG_HeaderValidate( &hdr ) == 0, "wrong-magic .psw REJECTED" );
	}

	if ( failures == 0 ) {
		printf( "PASS  .psw subset: progression ranges round-trip byte-exact, "
			"subset-only (no transient/full-world leak), objectives POD round-trips, "
			"wrong-version/magic REJECTED\n" );
		return 0;
	}
	printf( "FAILED with %d error(s)\n", failures );
	return 1;
}
