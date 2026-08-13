// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sv_bot_identity.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

uint64_t SV_BotIdentityNext( uint64_t current ) {
	return current == UINT64_MAX ? UINT64_C( 0 ) : current + UINT64_C( 1 );
}

int SV_BotSlotParse( const char *text, unsigned int limit, int *out ) {
	const unsigned char *cursor = (const unsigned char *)text;
	unsigned int parsed = 0;

	if ( !text || !text[0] || !out || limit == 0 ) return 0;
	for ( ; *cursor; cursor++ ) {
		unsigned int digit;
		if ( *cursor < '0' || *cursor > '9' ) return 0;
		digit = (unsigned int)( *cursor - '0' );
		if ( parsed > ( UINT_MAX - digit ) / 10u ) return 0;
		parsed = parsed * 10u + digit;
	}
	if ( parsed >= limit ) return 0;
	*out = (int)parsed;
	return 1;
}

int SV_BotIdentityParse( const char *text, uint64_t *out ) {
	const unsigned char *cursor = (const unsigned char *)text;
	char *end = NULL;
	unsigned long long parsed;
	int savedErrno = errno;

	if ( !text || !text[0] || !out ) return 0;
	for ( ; *cursor; cursor++ ) {
		if ( *cursor < '0' || *cursor > '9' ) return 0;
	}

	errno = 0;
	parsed = strtoull( text, &end, 10 );
	if ( errno == ERANGE || !end || *end != '\0' || parsed == 0
#if ULLONG_MAX > UINT64_MAX
	  || parsed > UINT64_MAX
#endif
	) {
		errno = savedErrno;
		return 0;
	}
	*out = (uint64_t)parsed;
	errno = savedErrno;
	return 1;
}

int SV_BotIdentityMatches( int activeBot, uint64_t current, uint64_t expected ) {
	return activeBot && current != 0 && expected != 0 && current == expected;
}
