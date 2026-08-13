// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef CL_INFO_CHALLENGE_H
#define CL_INFO_CHALLENGE_H

#include <stddef.h>
#include <stdint.h>

#define CL_INFO_CHALLENGE_RANDOM_BYTES 16
#define CL_INFO_CHALLENGE_HEX_CHARS 32

/* XOR the little-endian generation pattern across all random bytes, then emit
 * lowercase hex.  For every fixed generation this is a 128-bit bijection. */
void CL_InfoChallengeDerive(
	const uint8_t random[CL_INFO_CHALLENGE_RANDOM_BYTES], uint32_t generation,
	char output[CL_INFO_CHALLENGE_HEX_CHARS + 1] );

#endif
