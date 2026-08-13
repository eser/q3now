// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef CL_PING_OWNER_H
#define CL_PING_OWNER_H

#include <stdbool.h>

/* Transaction ownership is independent from the shared queue's reuse policy.
 * Browser source values mirror the public AS_* ABI without pulling the engine
 * headers into this dependency-free production seam. */
enum {
	CL_PING_BROWSER_SOURCE_LOCAL = 0,
	CL_PING_BROWSER_SOURCE_GLOBAL = 2,
	CL_PING_BROWSER_SOURCE_FAVORITES = 3
};

typedef enum {
	CL_PING_OWNER_NONE = 0,
	CL_PING_OWNER_MANUAL,
	CL_PING_OWNER_BROWSER_LOCAL,
	CL_PING_OWNER_BROWSER_GLOBAL,
	CL_PING_OWNER_BROWSER_FAVORITES
} clPingOwner_t;

typedef enum {
	CL_PING_CACHE_NONE = 0,
	CL_PING_CACHE_CLEAR,
	CL_PING_CACHE_PUBLISH
} clPingCacheAction_t;

bool CL_PingOwnerFromBrowserSource( int source, clPingOwner_t *output );
bool CL_PingOwnerBrowserSource( clPingOwner_t owner, int *output );
bool CL_PingOwnerMatchesBrowserSource( clPingOwner_t owner, int source );
bool CL_PingOwnerIsBrowser( clPingOwner_t owner );
clPingCacheAction_t CL_PingOwnerTerminalCacheAction( clPingOwner_t owner,
	bool completed );
bool CL_PingIdentityMatches( clPingOwner_t actualOwner, unsigned int actualGeneration,
	clPingOwner_t expectedOwner, unsigned int expectedGeneration );
const char *CL_PingOwnerName( clPingOwner_t owner );

#endif
