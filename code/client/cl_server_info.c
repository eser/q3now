// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "client.h"

#include <limits.h>
#include <string.h>

typedef struct {
	const char *name;
	char *value;
	size_t valueSize;
	qboolean seen;
} serverInfoField_t;

static unsigned char CL_ServerInfoLowerASCII( unsigned char ch ) {
	if ( ch >= 'A' && ch <= 'Z' ) return (unsigned char)( ch + ( 'a' - 'A' ) );
	return ch;
}

static qboolean CL_ServerInfoKeyEquals( const char *a, size_t aLength, const char *b ) {
	const size_t bLength = strlen( b );
	if ( aLength != bLength ) return qfalse;
	for ( size_t i = 0; i < aLength; i++ ) {
		if ( CL_ServerInfoLowerASCII( (unsigned char)a[i] )
		  != CL_ServerInfoLowerASCII( (unsigned char)b[i] ) ) return qfalse;
	}
	return qtrue;
}

static qboolean CL_ServerInfoCopyField( serverInfoField_t *fields, size_t fieldCount,
	const char *key, size_t keyLength, const char *value, size_t valueLength ) {
	for ( size_t i = 0; i < fieldCount; i++ ) {
		if ( !CL_ServerInfoKeyEquals( key, keyLength, fields[i].name ) ) {
			continue;
		}
		if ( fields[i].seen || valueLength >= fields[i].valueSize ) {
			return qfalse;
		}
		memcpy( fields[i].value, value, valueLength );
		fields[i].value[valueLength] = '\0';
		fields[i].seen = qtrue;
		break;
	}
	return qtrue;
}

static qboolean CL_ServerInfoParseUnsigned( const char *text, int maximum, int *value ) {
	int parsed = 0;

	if ( !text || !text[0] || maximum < 0 || !value ) {
		return qfalse;
	}
	for ( const unsigned char *p = (const unsigned char *)text; *p; p++ ) {
		const int digit = *p - '0';
		if ( digit < 0 || digit > 9 || digit > maximum
		  || parsed > ( maximum - digit ) / 10 ) {
			return qfalse;
		}
		parsed = parsed * 10 + digit;
	}
	*value = parsed;
	return qtrue;
}

static qboolean CL_ServerInfoValueByteIsSafe( unsigned char ch ) {
	return ch >= 32 && ch < 127 && ch != '"' && ch != ';';
}

/* Validate the request/response authority before the payload parser or any
 * browser cache sees the row.  Challenge keys are case-insensitive like the
 * rest of the historical info-string keys, but the unpredictable value is an
 * exact, case-sensitive token.  A duplicate challenge is ambiguous authority
 * and is therefore rejected rather than accepting whichever value appears
 * first. */
qboolean CL_ServerInfoChallengeMatches( const char *info, const char *expectedChallenge ) {
	const char *cursor;
	size_t expectedLength;
	qboolean seen = qfalse;

	if ( !info || !expectedChallenge || !expectedChallenge[0]
	  || strlen( info ) >= MAX_INFO_STRING ) return qfalse;
	expectedLength = strlen( expectedChallenge );
	cursor = info;
	while ( *cursor ) {
		const char *key;
		const char *value;
		size_t keyLength;
		size_t valueLength;

		if ( *cursor++ != '\\' ) return qfalse;
		key = cursor;
		while ( *cursor && *cursor != '\\' ) {
			if ( !CL_ServerInfoValueByteIsSafe( (unsigned char)*cursor ) ) return qfalse;
			cursor++;
		}
		keyLength = (size_t)( cursor - key );
		if ( keyLength == 0 || *cursor++ != '\\' ) return qfalse;
		value = cursor;
		while ( *cursor && *cursor != '\\' ) {
			if ( !CL_ServerInfoValueByteIsSafe( (unsigned char)*cursor ) ) return qfalse;
			cursor++;
		}
		valueLength = (size_t)( cursor - value );
		if ( CL_ServerInfoKeyEquals( key, keyLength, "challenge" ) ) {
			if ( seen || valueLength != expectedLength
			  || memcmp( value, expectedChallenge, expectedLength ) != 0 ) return qfalse;
			seen = qtrue;
		}
	}
	return seen;
}

/*
==========================
CL_ParseServerInfoResponse

Parse the untrusted infoResponse payload into a temporary server row.  The
destination is committed only after the complete wire contract is valid, so a
malformed response cannot partially mutate a cached browser entry.
==========================
*/
qboolean CL_ParseServerInfoResponse( const char *info, int expectedProtocol,
	serverInfo_t *server ) {
	char protocol[16] = "";
	char clients[16] = "";
	char maxClients[16] = "";
	char gameType[16] = "";
	char netType[16] = "";
	char minPing[16] = "";
	char maxPing[16] = "";
	char punkbuster[16] = "";
	char humanPlayers[16] = "";
	char needPass[16] = "";
	serverInfo_t parsed;
	serverInfoField_t fields[] = {
		{ "protocol", protocol, sizeof( protocol ), qfalse },
		{ "hostname", parsed.hostName, sizeof( parsed.hostName ), qfalse },
		{ "mapname", parsed.mapName, sizeof( parsed.mapName ), qfalse },
		{ "clients", clients, sizeof( clients ), qfalse },
		{ "sv_maxclients", maxClients, sizeof( maxClients ), qfalse },
		{ "gametype", gameType, sizeof( gameType ), qfalse },
		{ "game", parsed.game, sizeof( parsed.game ), qfalse },
		{ "nettype", netType, sizeof( netType ), qfalse },
		{ "minping", minPing, sizeof( minPing ), qfalse },
		{ "maxping", maxPing, sizeof( maxPing ), qfalse },
		{ "punkbuster", punkbuster, sizeof( punkbuster ), qfalse },
		{ "g_humanplayers", humanPlayers, sizeof( humanPlayers ), qfalse },
		{ "g_needpass", needPass, sizeof( needPass ), qfalse }
	};
	const char *cursor;
	int parsedProtocol;

	if ( !info || !server || expectedProtocol < 0 || strlen( info ) >= MAX_INFO_STRING ) {
		return qfalse;
	}
	memset( &parsed, 0, sizeof( parsed ) );
	cursor = info;
	while ( *cursor ) {
		const char *key;
		const char *value;
		size_t keyLength;
		size_t valueLength;

		if ( *cursor++ != '\\' ) {
			return qfalse;
		}
		key = cursor;
		while ( *cursor && *cursor != '\\' ) {
			if ( !CL_ServerInfoValueByteIsSafe( (unsigned char)*cursor ) ) return qfalse;
			cursor++;
		}
		keyLength = (size_t)( cursor - key );
		if ( keyLength == 0 || *cursor++ != '\\' ) {
			return qfalse;
		}
		value = cursor;
		while ( *cursor && *cursor != '\\' ) {
			if ( !CL_ServerInfoValueByteIsSafe( (unsigned char)*cursor ) ) return qfalse;
			cursor++;
		}
		valueLength = (size_t)( cursor - value );
		if ( !CL_ServerInfoCopyField( fields, sizeof( fields ) / sizeof( fields[0] ),
			key, keyLength, value, valueLength ) ) {
			return qfalse;
		}
	}

	if ( !fields[0].seen || !fields[1].seen || !parsed.hostName[0]
	  || !fields[2].seen || !parsed.mapName[0]
	  || !fields[3].seen || !fields[4].seen || !fields[5].seen ) {
		return qfalse;
	}
	if ( !CL_ServerInfoParseUnsigned( protocol, INT_MAX, &parsedProtocol )
	  || parsedProtocol != expectedProtocol
	  || !CL_ServerInfoParseUnsigned( clients, MAX_CLIENTS, &parsed.clients )
	  || !CL_ServerInfoParseUnsigned( maxClients, MAX_CLIENTS, &parsed.maxClients )
	  || parsed.maxClients < 1 || parsed.clients > parsed.maxClients
	  || !CL_ServerInfoParseUnsigned( gameType, GT_MAX_GAME_TYPE - 1, &parsed.gameType ) ) {
		return qfalse;
	}
	if ( fields[7].seen && !CL_ServerInfoParseUnsigned( netType, 2, &parsed.netType ) ) return qfalse;
	if ( fields[8].seen && !CL_ServerInfoParseUnsigned( minPing, INT_MAX, &parsed.minPing ) ) return qfalse;
	if ( fields[9].seen && !CL_ServerInfoParseUnsigned( maxPing, INT_MAX, &parsed.maxPing ) ) return qfalse;
	if ( parsed.maxPing && parsed.minPing > parsed.maxPing ) return qfalse;
	if ( fields[10].seen && !CL_ServerInfoParseUnsigned( punkbuster, 1, &parsed.punkbuster ) ) return qfalse;
	if ( fields[11].seen
	  && ( !CL_ServerInfoParseUnsigned( humanPlayers, parsed.clients, &parsed.g_humanplayers ) ) ) return qfalse;
	if ( fields[12].seen && !CL_ServerInfoParseUnsigned( needPass, 1, &parsed.g_needpass ) ) return qfalse;

	*server = parsed;
	return qtrue;
}
