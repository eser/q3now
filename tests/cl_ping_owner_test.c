// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "cl_ping_owner.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); failures++; } } while ( 0 )

int main( void ) {
	clPingOwner_t owner;
	clPingOwner_t sentinel = CL_PING_OWNER_BROWSER_FAVORITES;
	int source;
	int sourceSentinel = 77;

	CHECK( CL_PingOwnerFromBrowserSource( 0, &owner ) );
	CHECK( owner == CL_PING_OWNER_BROWSER_LOCAL );
	CHECK( CL_PingOwnerFromBrowserSource( 2, &owner ) );
	CHECK( owner == CL_PING_OWNER_BROWSER_GLOBAL );
	CHECK( CL_PingOwnerFromBrowserSource( 3, &owner ) );
	CHECK( owner == CL_PING_OWNER_BROWSER_FAVORITES );
	owner = sentinel;
	CHECK( !CL_PingOwnerFromBrowserSource( 1, &owner ) );
	CHECK( owner == sentinel );
	CHECK( !CL_PingOwnerFromBrowserSource( 4, NULL ) );
	CHECK( CL_PingOwnerBrowserSource( CL_PING_OWNER_BROWSER_LOCAL, &source ) && source == 0 );
	CHECK( CL_PingOwnerBrowserSource( CL_PING_OWNER_BROWSER_GLOBAL, &source ) && source == 2 );
	CHECK( CL_PingOwnerBrowserSource( CL_PING_OWNER_BROWSER_FAVORITES, &source ) && source == 3 );
	source = sourceSentinel;
	CHECK( !CL_PingOwnerBrowserSource( CL_PING_OWNER_MANUAL, &source ) );
	CHECK( source == sourceSentinel );
	CHECK( !CL_PingOwnerBrowserSource( (clPingOwner_t)99, &source ) );
	CHECK( source == sourceSentinel );
	CHECK( !CL_PingOwnerBrowserSource( CL_PING_OWNER_BROWSER_LOCAL, NULL ) );

	CHECK( !CL_PingOwnerIsBrowser( CL_PING_OWNER_MANUAL ) );
	CHECK( !CL_PingOwnerIsBrowser( CL_PING_OWNER_NONE ) );
	CHECK( CL_PingOwnerIsBrowser( CL_PING_OWNER_BROWSER_LOCAL ) );
	CHECK( CL_PingOwnerIsBrowser( CL_PING_OWNER_BROWSER_GLOBAL ) );
	CHECK( CL_PingOwnerIsBrowser( CL_PING_OWNER_BROWSER_FAVORITES ) );
	CHECK( !CL_PingOwnerIsBrowser( (clPingOwner_t)99 ) );
	CHECK( CL_PingOwnerTerminalCacheAction( CL_PING_OWNER_NONE, false ) == CL_PING_CACHE_NONE );
	CHECK( CL_PingOwnerTerminalCacheAction( CL_PING_OWNER_MANUAL, true ) == CL_PING_CACHE_NONE );
	CHECK( CL_PingOwnerTerminalCacheAction( CL_PING_OWNER_BROWSER_LOCAL, false ) == CL_PING_CACHE_CLEAR );
	CHECK( CL_PingOwnerTerminalCacheAction( CL_PING_OWNER_BROWSER_GLOBAL, true ) == CL_PING_CACHE_PUBLISH );
	CHECK( CL_PingOwnerTerminalCacheAction( CL_PING_OWNER_BROWSER_FAVORITES, true ) == CL_PING_CACHE_PUBLISH );
	CHECK( CL_PingOwnerTerminalCacheAction( (clPingOwner_t)99, false ) == CL_PING_CACHE_NONE );

	CHECK( CL_PingOwnerMatchesBrowserSource( CL_PING_OWNER_BROWSER_LOCAL, 0 ) );
	CHECK( CL_PingOwnerMatchesBrowserSource( CL_PING_OWNER_BROWSER_GLOBAL, 2 ) );
	CHECK( CL_PingOwnerMatchesBrowserSource( CL_PING_OWNER_BROWSER_FAVORITES, 3 ) );
	CHECK( !CL_PingOwnerMatchesBrowserSource( CL_PING_OWNER_MANUAL, 0 ) );
	CHECK( !CL_PingOwnerMatchesBrowserSource( CL_PING_OWNER_BROWSER_GLOBAL, 3 ) );
	CHECK( CL_PingIdentityMatches( CL_PING_OWNER_BROWSER_GLOBAL, 9,
		CL_PING_OWNER_BROWSER_GLOBAL, 9 ) );
	CHECK( !CL_PingIdentityMatches( CL_PING_OWNER_BROWSER_GLOBAL, 9,
		CL_PING_OWNER_BROWSER_GLOBAL, 10 ) );
	CHECK( !CL_PingIdentityMatches( CL_PING_OWNER_BROWSER_GLOBAL, 9,
		CL_PING_OWNER_BROWSER_LOCAL, 9 ) );
	CHECK( !CL_PingIdentityMatches( CL_PING_OWNER_NONE, 9,
		CL_PING_OWNER_NONE, 9 ) );
	CHECK( !CL_PingIdentityMatches( CL_PING_OWNER_BROWSER_GLOBAL, 0,
		CL_PING_OWNER_BROWSER_GLOBAL, 0 ) );

	CHECK( strcmp( CL_PingOwnerName( CL_PING_OWNER_NONE ), "none" ) == 0 );
	CHECK( strcmp( CL_PingOwnerName( CL_PING_OWNER_MANUAL ), "manual" ) == 0 );
	CHECK( strcmp( CL_PingOwnerName( CL_PING_OWNER_BROWSER_LOCAL ), "browser-local" ) == 0 );
	CHECK( strcmp( CL_PingOwnerName( CL_PING_OWNER_BROWSER_GLOBAL ), "browser-global" ) == 0 );
	CHECK( strcmp( CL_PingOwnerName( CL_PING_OWNER_BROWSER_FAVORITES ), "browser-favorites" ) == 0 );
	CHECK( strcmp( CL_PingOwnerName( (clPingOwner_t)99 ), "invalid" ) == 0 );

	return failures ? 1 : 0;
}
