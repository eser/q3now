// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef SV_BOT_IDENTITY_H
#define SV_BOT_IDENTITY_H

#include <stdint.h>

/* Dependency-light allocation identity seam shared by server production code
 * and the host contract. Zero means either "no allocation" or exhausted issuer;
 * callers must fail closed instead of recycling an old identity. */
uint64_t SV_BotIdentityNext( uint64_t current );
int SV_BotSlotParse( const char *text, unsigned int limit, int *out );
int SV_BotIdentityParse( const char *text, uint64_t *out );
int SV_BotIdentityMatches( int activeBot, uint64_t current, uint64_t expected );

#endif
