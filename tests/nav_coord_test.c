/*
 * nav_coord_test.c -- coordinate round-trip unit test (Phase 2 hard gate 1)
 *
 * Verifies both direct basis/handedness mappings and round-trip identity.
 * Direct expected-value oracles are essential: two symmetrically wrong
 * conversion functions can still round-trip to the input.
 *
 * Must pass before Phase 3 work begins.  Run via:
 *   ctest -R nav_coord_contract
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

static int vec3_exact( const float a[3], const float b[3] ) {
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
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

static int test_direct_mapping( const char *name,
		void (*convert)( const float in[3], float out[3] ),
		const float in[3], const float expected[3] ) {
	float out[3];

	convert( in, out );
	/* Copy/sign/permutation of exactly representable finite inputs must be
	   exact. An epsilon here could conceal axis contamination. */
	if ( !vec3_exact( out, expected ) ) {
		printf( "FAIL  %s  in=[%.4f, %.4f, %.4f]"
		        "  expected=[%.4f, %.4f, %.4f]"
		        "  out=[%.4f, %.4f, %.4f]\n",
		        name,
		        in[0], in[1], in[2],
		        expected[0], expected[1], expected[2],
		        out[0], out[1], out[2] );
		return 0;
	}
	printf( "PASS  %s\n", name );
	return 1;
}

int main( void ) {
	int failures = 0;
	int checks = 0;
	const float vecs[][3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 3.0f, 4.0f, 5.0f },
	};
	const float quakeBasis[][3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 3.0f, 4.0f, 5.0f },
	};
	const float recastExpected[][3] = {
		{ 1.0f, 0.0f,  0.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 1.0f,  0.0f },
		{ 3.0f, 5.0f, -4.0f },
	};
	const float recastBasis[][3] = {
		{ 1.0f, 0.0f,  0.0f },
		{ 0.0f, 1.0f,  0.0f },
		{ 0.0f, 0.0f,  1.0f },
		{ 3.0f, 5.0f, -4.0f },
	};
	const float quakeExpected[][3] = {
		{ 1.0f,  0.0f, 0.0f },
		{ 0.0f,  0.0f, 1.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 3.0f,  4.0f, 5.0f },
	};
	const char *quakeNames[] = { "Quake +X east", "Quake +Y north", "Quake +Z up", "Quake general" };
	const char *recastNames[] = { "Recast +X east", "Recast +Y up", "Recast +Z south", "Recast general" };
	int n = (int)(sizeof(vecs) / sizeof(vecs[0]));
	int i;

	printf( "nav_coord direct basis + round-trip contract\n" );
	printf( "--------------------------------------------\n" );
	for ( i = 0; i < n; i++ ) {
		checks++;
		if ( !test_direct_mapping( quakeNames[i], Nav_QuakeToRecast,
		                           quakeBasis[i], recastExpected[i] ) )
			failures++;
	}
	for ( i = 0; i < n; i++ ) {
		checks++;
		if ( !test_direct_mapping( recastNames[i], Nav_RecastToQuake,
		                           recastBasis[i], quakeExpected[i] ) )
			failures++;
	}
	for ( i = 0; i < n; i++ ) {
		checks++;
		if ( !test_round_trip( vecs[i] ) )
			failures++;
	}
	printf( "--------------------------------------------\n" );
	printf( "%d/%d passed\n", checks - failures, checks );

	return (failures > 0) ? 1 : 0;
}
