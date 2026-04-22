/*
 * nav_coord_test.c -- coordinate round-trip unit test (Phase 2 hard gate 1)
 *
 * Verifies: Nav_RecastToQuake(Nav_QuakeToRecast(v)) == v
 * for vectors [1,0,0], [0,1,0], [0,0,1], [3,4,5].
 *
 * Must pass before Phase 3 work begins.  Run via:
 *   ctest -R nav_coord_round_trip
 * or directly:
 *   ./nav_coord_test
 */
#include <stdio.h>
#include <math.h>

#include "../code/qcommon/nav/nav_coord.h"

#define EPSILON 1e-6f

static int vec3_equal( const float a[3], const float b[3] ) {
	return (fabsf(a[0]-b[0]) < EPSILON)
	    && (fabsf(a[1]-b[1]) < EPSILON)
	    && (fabsf(a[2]-b[2]) < EPSILON);
}

static int test_round_trip( const float in[3] ) {
	float recast[3];
	float out[3];

	Nav_QuakeToRecast( in, recast );
	Nav_RecastToQuake( recast, out );

	if ( !vec3_equal( in, out ) ) {
		printf( "FAIL  in=[%.4f, %.4f, %.4f]"
		        "  recast=[%.4f, %.4f, %.4f]"
		        "  out=[%.4f, %.4f, %.4f]\n",
		        in[0], in[1], in[2],
		        recast[0], recast[1], recast[2],
		        out[0], out[1], out[2] );
		return 0;
	}
	printf( "PASS  [%.4f, %.4f, %.4f]\n", in[0], in[1], in[2] );
	return 1;
}

int main( void ) {
	int failures = 0;
	const float vecs[][3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 3.0f, 4.0f, 5.0f },
	};
	int n = (int)(sizeof(vecs) / sizeof(vecs[0]));
	int i;

	printf( "nav_coord round-trip test\n" );
	printf( "--------------------------\n" );
	for ( i = 0; i < n; i++ ) {
		if ( !test_round_trip( vecs[i] ) )
			failures++;
	}
	printf( "--------------------------\n" );
	printf( "%d/%d passed\n", n - failures, n );

	return (failures > 0) ? 1 : 0;
}
