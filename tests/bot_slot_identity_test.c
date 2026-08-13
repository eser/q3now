// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sv_bot_identity.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

static int failures;

#define CHECK(expr) do { \
	if ( !(expr) ) { \
		fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		failures++; \
	} \
} while ( 0 )

static void reject_parse( const char *text ) {
	uint64_t out = UINT64_C( 0x13579bdf2468ace0 );
	errno = EDOM;
	CHECK( !SV_BotIdentityParse( text, &out ) );
	CHECK( out == UINT64_C( 0x13579bdf2468ace0 ) );
	CHECK( errno == EDOM );
}

static void reject_slot( const char *text, unsigned int limit ) {
	int out = 0x13579bdf;
	CHECK( !SV_BotSlotParse( text, limit, &out ) );
	CHECK( out == 0x13579bdf );
}

int main( void ) {
	uint64_t out = 0;
	uint64_t processIssuer = 0;
	uint64_t disposableServerContainer = UINT64_C( 0xfeedface );
	uint64_t first = SV_BotIdentityNext( processIssuer );
	processIssuer = first;
	uint64_t second = SV_BotIdentityNext( first );

	CHECK( first == 1 );
	CHECK( second == 2 );
	CHECK( SV_BotIdentityNext( UINT64_MAX ) == 0 );
	CHECK( first != second );
	{
		uint64_t exhaustedIssuer = UINT64_MAX;
		uint64_t refused = SV_BotIdentityNext( exhaustedIssuer );
		if ( refused != 0 ) exhaustedIssuer = refused;
		CHECK( refused == 0 );
		CHECK( exhaustedIssuer == UINT64_MAX );
	}
	/* Server restart surrogate: resetting server-owned disposable state must not
	 * reset the process-lifetime issuer history used for the next allocation. */
	disposableServerContainer = 0;
	CHECK( disposableServerContainer == 0 );
	CHECK( SV_BotIdentityNext( processIssuer ) == second );

	errno = EDOM;
	CHECK( SV_BotIdentityParse( "1", &out ) && out == 1 );
	CHECK( errno == EDOM );
	CHECK( SV_BotIdentityParse( "18446744073709551615", &out )
		&& out == UINT64_MAX );
	reject_parse( NULL );
	reject_parse( "" );
	reject_parse( "0" );
	reject_parse( " 1" );
	reject_parse( "+1" );
	reject_parse( "01x" );
	reject_parse( "18446744073709551616" );

	{
		int slot = -1;
		CHECK( SV_BotSlotParse( "0", 64, &slot ) && slot == 0 );
		CHECK( SV_BotSlotParse( "63", 64, &slot ) && slot == 63 );
		CHECK( SV_BotSlotParse( "01", 64, &slot ) && slot == 1 );
		reject_slot( NULL, 64 );
		reject_slot( "", 64 );
		reject_slot( "-1", 64 );
		reject_slot( "+1", 64 );
		reject_slot( " 1", 64 );
		reject_slot( "64", 64 );
		reject_slot( "42949672970", 64 );
		reject_slot( "0", 0 );
	}

	CHECK( SV_BotIdentityMatches( 1, first, first ) );
	CHECK( !SV_BotIdentityMatches( 0, first, first ) );
	CHECK( !SV_BotIdentityMatches( 1, 0, first ) );
	CHECK( !SV_BotIdentityMatches( 1, first, 0 ) );
	/* Same physical slot, new allocation: a queued identity for A must not
	 * authorize the replacement B. */
	CHECK( !SV_BotIdentityMatches( 1, second, first ) );
	CHECK( SV_BotIdentityMatches( 1, second, second ) );

	if ( failures ) return 1;
	puts( "bot slot identity contract: PASS" );
	return 0;
}
