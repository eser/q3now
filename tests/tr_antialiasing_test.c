// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_antialiasing.h"

#include <limits.h>
#include <stdio.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL tr_antialiasing line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

static rAntialiasingPlan_t Plan( int mode, int diagnostic ) {
	rAntialiasingInput_t in;
	rAntialiasingPlan_t out;
	in.mode = mode;
	in.temporalDiagnostic = diagnostic;
	R_AntialiasingResolve( &in, &out );
	return out;
}

int main( void ) {
	rAntialiasingPlan_t p;
	rAntialiasingInput_t in = { R_AA_MODE_OFF, 0 };

	// TAA is the only technique: r_aa 1 and 2 are the same thing, and the
	// symbolic constants line up with the documented cvar values.
	CHECK( R_AA_MODE_OFF == 0 );
	CHECK( R_AA_MODE_TAA == 1 );

	// Defaults reproduce the pre-r_aa behaviour exactly: nothing temporal.
	p = Plan( R_AA_MODE_OFF, 0 );
	CHECK( !p.temporalRequested );

	// r_aa 1 and r_aa 2 both request TAA. Many scripts and docs pass `r_aa 2`.
	p = Plan( 1, 0 );
	CHECK( p.temporalRequested );
	p = Plan( 2, 0 );
	CHECK( p.temporalRequested );
	p = Plan( R_AA_MODE_TAA, 0 );
	CHECK( p.temporalRequested );

	// The diagnostic cvar (r_temporalInputTest) forces the temporal path on
	// whatever the mode says, and never switches it off for a TAA mode.
	p = Plan( 0, 1 );
	CHECK( p.temporalRequested );
	p = Plan( 1, 1 );
	CHECK( p.temporalRequested );
	p = Plan( 2, 1 );
	CHECK( p.temporalRequested );

	// Negative values behave as off; the diagnostic still overrides them.
	p = Plan( -1, 0 );
	CHECK( !p.temporalRequested );
	p = Plan( -5, 0 );
	CHECK( !p.temporalRequested );
	p = Plan( INT_MIN, 0 );
	CHECK( !p.temporalRequested );
	p = Plan( -1, 1 );
	CHECK( p.temporalRequested );
	p = Plan( INT_MIN, 1 );
	CHECK( p.temporalRequested );

	// Large values are harmless and behave as TAA (the cvar range clamps to
	// 0..2, but the resolver must not rely on that).
	p = Plan( 3, 0 );
	CHECK( p.temporalRequested );
	p = Plan( 7, 0 );
	CHECK( p.temporalRequested );
	p = Plan( 99, 0 );
	CHECK( p.temporalRequested );
	p = Plan( INT_MAX, 0 );
	CHECK( p.temporalRequested );
	p = Plan( INT_MAX, 1 );
	CHECK( p.temporalRequested );

	// Any non-zero diagnostic value counts as the diagnostic being on.
	p = Plan( 0, 2 );
	CHECK( p.temporalRequested );
	p = Plan( 0, -1 );
	CHECK( p.temporalRequested );

	// Exhaustive sweep around the documented range: only mode <= 0 with the
	// diagnostic off is non-temporal.
	for ( int mode = -3; mode <= 5; ++mode ) {
		p = Plan( mode, 0 );
		CHECK( p.temporalRequested == ( mode >= 1 ? 1 : 0 ) );
		p = Plan( mode, 1 );
		CHECK( p.temporalRequested );
	}

	// The resolver is a pure function of its input: a stale plan is overwritten.
	p.temporalRequested = 1;
	{
		rAntialiasingInput_t off = { 0, 0 };
		R_AntialiasingResolve( &off, &p );
		CHECK( !p.temporalRequested );
	}

	// Null arguments do not crash and an input-less call yields the all-off plan.
	p.temporalRequested = 1;
	R_AntialiasingResolve( NULL, &p );
	CHECK( !p.temporalRequested );
	R_AntialiasingResolve( &in, NULL );

	puts( "PASS tr_antialiasing" );
	return 0;
}
