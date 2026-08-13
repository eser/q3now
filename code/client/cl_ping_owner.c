// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "cl_ping_owner.h"

bool CL_PingOwnerFromBrowserSource( int source, clPingOwner_t *output ) {
	clPingOwner_t owner;

	if ( !output ) return false;
	switch ( source ) {
	case CL_PING_BROWSER_SOURCE_LOCAL:
		owner = CL_PING_OWNER_BROWSER_LOCAL;
		break;
	case CL_PING_BROWSER_SOURCE_GLOBAL:
		owner = CL_PING_OWNER_BROWSER_GLOBAL;
		break;
	case CL_PING_BROWSER_SOURCE_FAVORITES:
		owner = CL_PING_OWNER_BROWSER_FAVORITES;
		break;
	default:
		return false;
	}
	*output = owner;
	return true;
}

bool CL_PingOwnerMatchesBrowserSource( clPingOwner_t owner, int source ) {
	clPingOwner_t expected;
	return CL_PingOwnerFromBrowserSource( source, &expected ) && owner == expected;
}

bool CL_PingOwnerBrowserSource( clPingOwner_t owner, int *output ) {
	int source;

	if ( !output ) return false;
	switch ( owner ) {
	case CL_PING_OWNER_BROWSER_LOCAL:
		source = CL_PING_BROWSER_SOURCE_LOCAL;
		break;
	case CL_PING_OWNER_BROWSER_GLOBAL:
		source = CL_PING_BROWSER_SOURCE_GLOBAL;
		break;
	case CL_PING_OWNER_BROWSER_FAVORITES:
		source = CL_PING_BROWSER_SOURCE_FAVORITES;
		break;
	default:
		return false;
	}
	*output = source;
	return true;
}

bool CL_PingOwnerIsBrowser( clPingOwner_t owner ) {
	return owner == CL_PING_OWNER_BROWSER_LOCAL
		|| owner == CL_PING_OWNER_BROWSER_GLOBAL
		|| owner == CL_PING_OWNER_BROWSER_FAVORITES;
}

clPingCacheAction_t CL_PingOwnerTerminalCacheAction( clPingOwner_t owner,
	bool completed ) {
	if ( !CL_PingOwnerIsBrowser( owner ) ) return CL_PING_CACHE_NONE;
	return completed ? CL_PING_CACHE_PUBLISH : CL_PING_CACHE_CLEAR;
}

bool CL_PingIdentityMatches( clPingOwner_t actualOwner, unsigned int actualGeneration,
	clPingOwner_t expectedOwner, unsigned int expectedGeneration ) {
	return actualOwner != CL_PING_OWNER_NONE && actualGeneration != 0
		&& actualOwner == expectedOwner && actualGeneration == expectedGeneration;
}

const char *CL_PingOwnerName( clPingOwner_t owner ) {
	switch ( owner ) {
	case CL_PING_OWNER_NONE: return "none";
	case CL_PING_OWNER_MANUAL: return "manual";
	case CL_PING_OWNER_BROWSER_LOCAL: return "browser-local";
	case CL_PING_OWNER_BROWSER_GLOBAL: return "browser-global";
	case CL_PING_OWNER_BROWSER_FAVORITES: return "browser-favorites";
	default: return "invalid";
	}
}
