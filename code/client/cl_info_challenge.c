// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "cl_info_challenge.h"

#include <string.h>

void CL_InfoChallengeDerive(
	const uint8_t random[CL_INFO_CHALLENGE_RANDOM_BYTES], uint32_t generation,
	char output[CL_INFO_CHALLENGE_HEX_CHARS + 1] ) {
	static const char hex[] = "0123456789abcdef";
	uint8_t mixed[CL_INFO_CHALLENGE_RANDOM_BYTES];
	char derived[CL_INFO_CHALLENGE_HEX_CHARS + 1];

	memcpy( mixed, random, sizeof( mixed ) );
	for ( size_t i = 0; i < sizeof( mixed ); i++ ) {
		mixed[i] ^= (uint8_t)( generation >> ( ( i & 3u ) * 8 ) );
	}
	for ( size_t i = 0; i < sizeof( mixed ); i++ ) {
		derived[i * 2] = hex[mixed[i] >> 4];
		derived[i * 2 + 1] = hex[mixed[i] & 15];
	}
	derived[CL_INFO_CHALLENGE_HEX_CHARS] = '\0';
	memcpy( output, derived, sizeof( derived ) );
}
