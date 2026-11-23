// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_antialiasing.h"

#include <stdio.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL tr_antialiasing line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

static rAntialiasingPlan_t Plan( int mode, int preset, int diagnostic ) {
	rAntialiasingInput_t in;
	rAntialiasingPlan_t out;
	in.mode = mode;
	in.smaaPreset = preset;
	in.temporalDiagnostic = diagnostic;
	R_AntialiasingResolve( &in, &out );
	return out;
}

int main( void ) {
	rAntialiasingPlan_t p;
	// The user-facing values: r_aa 0 off, 1 TAA, 2 SMAA (scripts, docs and saved configs depend on them).
	CHECK( R_AA_MODE_OFF == 0 && R_AA_MODE_TAA == 1 && R_AA_MODE_SMAA == 2 );
	rAntialiasingInput_t in = { R_AA_MODE_OFF, 0, 0 };

	// Defaults reproduce the pre-r_aa behaviour exactly.
	p = Plan( R_AA_MODE_OFF, 0, 0 );
	CHECK( !p.temporalRequested && p.smaaQuality == 0 );

	// Developer overrides with the mode off: r_smaa alone still drives SMAA, the
	// diagnostic alone still drives the temporal path, and together they are
	// left untouched (the harnesses rely on this).
	for ( int preset = 1; preset <= R_AA_SMAA_PRESET_MAX; ++preset ) {
		p = Plan( R_AA_MODE_OFF, preset, 0 );
		CHECK( p.smaaQuality == preset && !p.temporalRequested );
		p = Plan( R_AA_MODE_OFF, preset, 1 );
		CHECK( p.smaaQuality == preset && p.temporalRequested );
	}
	p = Plan( R_AA_MODE_OFF, 0, 1 );
	CHECK( p.temporalRequested && p.smaaQuality == 0 );

	// SMAA mode: the preset when r_smaa has one, else the default, never temporal.
	p = Plan( R_AA_MODE_SMAA, 0, 0 );
	CHECK( p.smaaQuality == R_AA_SMAA_PRESET_DEFAULT && !p.temporalRequested );
	p = Plan( R_AA_MODE_SMAA, 2, 0 );
	CHECK( p.smaaQuality == 2 && !p.temporalRequested );

	// TAA mode: temporal on, SMAA off even if r_smaa is set. It is never refused:
	// the entity-matrix payload it needs is the only path.
	p = Plan( R_AA_MODE_TAA, 0, 0 );
	CHECK( p.temporalRequested && p.smaaQuality == 0 );
	p = Plan( R_AA_MODE_TAA, 3, 0 );
	CHECK( p.temporalRequested && p.smaaQuality == 0 );
	p = Plan( R_AA_MODE_TAA, 3, 1 );
	CHECK( p.temporalRequested && p.smaaQuality == 0 );

	// Out-of-range input is harmless.
	p = Plan( 7, 0, 0 );
	CHECK( !p.temporalRequested && p.smaaQuality == 0 );
	p = Plan( -1, 99, 0 );
	CHECK( p.smaaQuality == R_AA_SMAA_PRESET_MAX && !p.temporalRequested );
	p = Plan( R_AA_MODE_OFF, -5, 0 );
	CHECK( p.smaaQuality == 0 );

	// Null arguments do not crash and an input-less call yields the all-off plan.
	R_AntialiasingResolve( NULL, &p );
	CHECK( !p.temporalRequested && p.smaaQuality == 0 );
	R_AntialiasingResolve( &in, NULL );

	puts( "PASS tr_antialiasing" );
	return 0;
}
