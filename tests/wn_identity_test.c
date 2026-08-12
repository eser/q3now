// SPDX-License-Identifier: GPL-2.0-or-later

#include "../code/qcommon/wired/net/wn_identity.h"

#include <stdio.h>

static int failures;

static void expect( int condition, const char *name ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", name );
		failures++;
	}
}

int main( void ) {
	expect( WN_AllocationIdentityMatches( 1, 3, 41, 3, 41 ),
		"exact live allocation accepted" );
	expect( !WN_AllocationIdentityMatches( 1, 3, 42, 3, 41 ),
		"recycled handle with stale allocation rejected" );
	expect( !WN_AllocationIdentityMatches( 1, 4, 41, 3, 41 ),
		"different handle rejected" );
	expect( !WN_AllocationIdentityMatches( 0, 3, 41, 3, 41 ),
		"inactive allocation rejected" );
	expect( !WN_AllocationIdentityMatches( 1, 3, 0, 3, 0 ),
		"zero identity never aliases" );
	expect( WN_AcceptedAllocationIdentityMatches( 1, 1, 3, 42, 3, 42 ),
		"accepted exact long-lived association routes" );
	expect( !WN_AcceptedAllocationIdentityMatches( 1, 0, 3, 42, 3, 42 ),
		"pre-admission traffic rejected" );
	expect( !WN_AcceptedAllocationIdentityMatches( 1, 1, 3, 43, 3, 42 ),
		"accepted recycled handle cannot route to stale client" );
	if ( failures ) return 1;
	puts( "wired connection identity contract: PASS" );
	return 0;
}
