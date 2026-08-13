// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "cl_info_challenge.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); failures++; } } while ( 0 )

static int lowercase_hex32( const char *text ) {
	if ( !text || strlen( text ) != CL_INFO_CHALLENGE_HEX_CHARS ) return 0;
	for ( size_t i = 0; text[i]; i++ ) {
		if ( !( text[i] >= '0' && text[i] <= '9' )
		  && !( text[i] >= 'a' && text[i] <= 'f' ) ) return 0;
	}
	return 1;
}

int main( void ) {
	uint8_t zero[CL_INFO_CHALLENGE_RANDOM_BYTES] = { 0 };
	uint8_t entropy[CL_INFO_CHALLENGE_RANDOM_BYTES];
	char first[CL_INFO_CHALLENGE_HEX_CHARS + 1];
	char second[CL_INFO_CHALLENGE_HEX_CHARS + 1];
	char high[CL_INFO_CHALLENGE_HEX_CHARS + 1];
	char guard[CL_INFO_CHALLENGE_HEX_CHARS + 3];

	for ( size_t i = 0; i < sizeof( entropy ); i++ ) entropy[i] = (uint8_t)i;
	CL_InfoChallengeDerive( zero, 0x01020304u, first );
	CHECK( strcmp( first, "04030201040302010403020104030201" ) == 0 );
	CL_InfoChallengeDerive( zero, 0x01020305u, second );
	CHECK( strcmp( second, "05030201050302010503020105030201" ) == 0 );
	CHECK( strcmp( first, second ) != 0 );
	CHECK( lowercase_hex32( first ) && lowercase_hex32( second ) );
	CL_InfoChallengeDerive( zero, 0, first );
	CHECK( strcmp( first, "00000000000000000000000000000000" ) == 0 );
	CL_InfoChallengeDerive( zero, 0x80000000u, high );
	CHECK( strcmp( high, "00000080000000800000008000000080" ) == 0 );
	CL_InfoChallengeDerive( entropy, 1, first );
	CHECK( strcmp( first, "010102030505060709090a0b0d0d0e0f" ) == 0 );
	memset( guard, 'Z', sizeof( guard ) );
	CL_InfoChallengeDerive( zero, 1, guard + 1 );
	CHECK( guard[0] == 'Z' && guard[sizeof( guard ) - 1] == 'Z' );
	return failures ? 1 : 0;
}
