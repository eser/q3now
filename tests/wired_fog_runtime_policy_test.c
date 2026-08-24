// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <stdio.h>

#include "../code/render/frontend/wired_fog_runtime_policy.h"

static int failures;

static void check_type( const char *name, int actual, int expected ) {
	if ( actual != expected ) {
		fprintf( stderr, "FAIL %s: got %d, expected %d\n", name, actual, expected );
		failures++;
	}
}

int main( void ) {
	check_type( "gate disables explicit linear",
		wired_fog_runtime_resolve_volume_type( 0, WIRED_FOG_RUNTIME_LINEAR, 2 ),
		WIRED_FOG_RUNTIME_NONE );
	check_type( "explicit linear",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_LINEAR, -1 ),
		WIRED_FOG_RUNTIME_LINEAR );
	check_type( "explicit exp",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_EXP, -1 ),
		WIRED_FOG_RUNTIME_EXP );
	check_type( "explicit exp2",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_EXP2, -1 ),
		WIRED_FOG_RUNTIME_EXP2 );
	check_type( "legacy defaults to classic",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_NONE, -1 ),
		WIRED_FOG_RUNTIME_NONE );
	check_type( "legacy linear fallback",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_NONE, 0 ),
		WIRED_FOG_RUNTIME_LINEAR );
	check_type( "legacy exp fallback",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_NONE, 1 ),
		WIRED_FOG_RUNTIME_EXP );
	check_type( "legacy exp2 fallback",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_NONE, 2 ),
		WIRED_FOG_RUNTIME_EXP2 );
	check_type( "invalid fallback stays classic",
		wired_fog_runtime_resolve_volume_type( 1, WIRED_FOG_RUNTIME_NONE, 3 ),
		WIRED_FOG_RUNTIME_NONE );
	check_type( "gate disables published global fog",
		wired_fog_runtime_resolve_global_type( 0, WIRED_FOG_RUNTIME_EXP ),
		WIRED_FOG_RUNTIME_NONE );
	check_type( "valid published global fog",
		wired_fog_runtime_resolve_global_type( 1, WIRED_FOG_RUNTIME_EXP ),
		WIRED_FOG_RUNTIME_EXP );
	check_type( "missing global producer stays inactive",
		wired_fog_runtime_resolve_global_type( 1, WIRED_FOG_RUNTIME_NONE ),
		WIRED_FOG_RUNTIME_NONE );
	check_type( "invalid global type stays inactive",
		wired_fog_runtime_resolve_global_type( 1, 4 ),
		WIRED_FOG_RUNTIME_NONE );

	if ( failures ) {
		return 1;
	}
	puts( "wired fog runtime policy: 13/13 checks passed" );
	return 0;
}
