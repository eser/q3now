// SPDX-License-Identifier: GPL-2.0-or-later

#include "../code/client/client.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_true( qboolean condition, const char *name ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", name );
		failures++;
	}
}

static void expect_rejected_unchanged( const char *info, const char *name ) {
	serverInfo_t before;
	serverInfo_t after;
	memset( &before, 0x5a, sizeof( before ) );
	after = before;
	expect_true( !CL_ParseServerInfoResponse( info, 74, &after ), name );
	expect_true( memcmp( &before, &after, sizeof( before ) ) == 0, "reject does not partially mutate" );
}

int main( void ) {
	serverInfo_t row;
	char boundary[256];
	char badGameType[256];
	char overlong[MAX_INFO_STRING + 32];
	const char *overlongPrefix =
		"\\protocol\\74\\hostname\\TooLong\\mapname\\arena1\\clients\\0"
		"\\sv_maxclients\\8\\gametype\\0\\padding\\";
	const char *valid = "\\challenge\\abc123\\protocol\\74\\hostname\\Q0 Server"
		"\\mapname\\arena7\\clients\\3\\g_humanplayers\\2\\sv_maxclients\\8"
		"\\gametype\\4\\game\\q0mod\\nettype\\1\\minping\\10\\maxping\\250"
		"\\punkbuster\\0\\g_needpass\\1";

	memset( &row, 0xcc, sizeof( row ) );
	expect_true( CL_ServerInfoChallengeMatches( valid, "abc123" ),
		"exact challenge accepted" );
	expect_true( !CL_ServerInfoChallengeMatches( valid, "abc124" ),
		"wrong challenge rejected" );
	expect_true( !CL_ServerInfoChallengeMatches(
		"\\protocol\\74\\hostname\\No Challenge", "abc123" ),
		"missing challenge rejected" );
	expect_true( !CL_ServerInfoChallengeMatches(
		"\\challenge\\abc123\\ChAlLeNgE\\abc123\\protocol\\74", "abc123" ),
		"case-variant duplicate challenge rejected" );
	expect_true( !CL_ServerInfoChallengeMatches(
		"\\challenge\\abc123;quit\\protocol\\74", "abc123;quit" ),
		"unsafe challenge token rejected" );
	expect_true( CL_ParseServerInfoResponse( valid, 74, &row ), "valid response accepted" );
	expect_true( strcmp( row.hostName, "Q0 Server" ) == 0, "hostname copied" );
	expect_true( strcmp( row.mapName, "arena7" ) == 0, "map copied" );
	expect_true( strcmp( row.game, "q0mod" ) == 0, "game copied" );
	expect_true( row.clients == 3 && row.maxClients == 8, "client bounds parsed" );
	expect_true( row.gameType == 4 && row.netType == 1, "gametype and nettype parsed" );
	expect_true( row.g_humanplayers == 2 && row.g_needpass == 1, "optional bounds parsed" );
	expect_true( CL_ParseServerInfoResponse(
		"\\PrOtOcOl\\74\\HoStNaMe\\Mixed Case\\MaPnAmE\\arena7\\ClIeNtS\\1"
		"\\Sv_MaXcLiEnTs\\8\\GaMeTyPe\\0", 74, &row ),
		"historical case-insensitive keys accepted" );

	snprintf( boundary, sizeof( boundary ),
		"\\protocol\\74\\hostname\\Boundary\\mapname\\arena1\\clients\\%d"
		"\\sv_maxclients\\%d\\gametype\\%d", MAX_CLIENTS, MAX_CLIENTS,
		GT_MAX_GAME_TYPE - 1 );
	expect_true( CL_ParseServerInfoResponse( boundary, 74, &row ), "valid upper bounds accepted" );

	snprintf( badGameType, sizeof( badGameType ),
		"\\protocol\\74\\hostname\\BadGT\\mapname\\arena1\\clients\\0"
		"\\sv_maxclients\\8\\gametype\\%d", GT_MAX_GAME_TYPE );
	expect_rejected_unchanged( badGameType, "gametype table OOB rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74junk\\hostname\\BadProto\\mapname\\arena1\\clients\\0"
		"\\sv_maxclients\\8\\gametype\\0", "protocol trailing junk rejected" );
	expect_rejected_unchanged(
		"\\protocol\\999999999999999999999\\hostname\\Overflow\\mapname\\arena1"
		"\\clients\\0\\sv_maxclients\\8\\gametype\\0", "protocol overflow rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\Negative\\mapname\\arena1\\clients\\-1"
		"\\sv_maxclients\\8\\gametype\\0", "negative clients rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\TooMany\\mapname\\arena1\\clients\\9"
		"\\sv_maxclients\\8\\gametype\\0", "clients above max rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\Overflow\\mapname\\arena1\\clients\\2147483648"
		"\\sv_maxclients\\8\\gametype\\0", "client overflow rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\NoSlots\\mapname\\arena1\\clients\\0"
		"\\sv_maxclients\\0\\gametype\\0", "zero maxclients rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\Humans\\mapname\\arena1\\clients\\1"
		"\\g_humanplayers\\2\\sv_maxclients\\8\\gametype\\0", "human count above clients rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\Duplicate\\hostname\\Shadow\\mapname\\arena1"
		"\\clients\\0\\sv_maxclients\\8\\gametype\\0", "duplicate authority rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\HostName\\Duplicate\\hostname\\Shadow\\mapname\\arena1"
		"\\clients\\0\\sv_maxclients\\8\\gametype\\0", "case-variant duplicate rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\Injected;quit\\mapname\\arena1\\clients\\0"
		"\\sv_maxclients\\8\\gametype\\0", "unsafe display text rejected" );
	expect_rejected_unchanged(
		"\\protocol\\74\\hostname\\MissingMap\\clients\\0\\sv_maxclients\\8"
		"\\gametype\\0", "missing required map rejected" );

	memset( overlong, 'x', sizeof( overlong ) );
	memcpy( overlong, overlongPrefix, strlen( overlongPrefix ) );
	overlong[MAX_INFO_STRING] = '\0';
	expect_rejected_unchanged( overlong, "complete overlong wire payload rejected" );

	if ( failures ) {
		fprintf( stderr, "%d server-info contract assertion(s) failed\n", failures );
		return 1;
	}
	puts( "server-info parser contract: PASS" );
	return 0;
}
