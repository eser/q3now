// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "q_shared.h"
#include "bg_public.h"

#include <stdio.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "FAIL hitscan impact material line %d: %s\n", __LINE__, #x ); \
	return 1; \
} } while ( 0 )

_Static_assert( HITSCAN_IMPACT_DEFAULT == 0, "wire fallback must remain zero" );
_Static_assert( HITSCAN_IMPACT_MATERIAL_COUNT == 3, "coarse material wire range" );

int main( void ) {
	CHECK( BG_HitscanImpactMaterialForSurfaceFlags( 0 ) == HITSCAN_IMPACT_DEFAULT );
	CHECK( BG_HitscanImpactMaterialForSurfaceFlags( SURF_METALSTEPS ) == HITSCAN_IMPACT_METAL );
	CHECK( BG_HitscanImpactMaterialForSurfaceFlags( SURF_DUST ) == HITSCAN_IMPACT_DUST );
	CHECK( BG_HitscanImpactMaterialForSurfaceFlags( SURF_METALSTEPS | SURF_DUST ) == HITSCAN_IMPACT_METAL );
	CHECK( BG_HitscanImpactMaterialForSurfaceFlags( SURF_NOIMPACT ) == HITSCAN_IMPACT_DEFAULT );
	puts( "PASS hitscan impact material contract" );
	return 0;
}
