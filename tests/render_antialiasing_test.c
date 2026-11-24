// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* r_aa -> anti-aliasing plan (render_antialiasing.h), the mapping all four renderers run: 0 off, 1 TAA, 2 SMAA at the fixed preset;
 * nothing else turns SMAA on, and the temporal diagnostic forces the temporal path without touching SMAA. */

#include "render_antialiasing.h"

#include <stdio.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL render_antialiasing line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

int main( void ) {
	renderAntialiasingPlan_t p;

	// The user-facing values: r_aa 0 off, 1 TAA, 2 SMAA (scripts, docs and saved configs depend on them).
	CHECK( RENDER_AA_OFF == 0 && RENDER_AA_TAA == 1 && RENDER_AA_SMAA == 2 );
	CHECK( RENDER_AA_SMAA_QUALITY == 3 );

	// 0: nothing runs.
	p = RenderAntialiasing_Resolve( RENDER_AA_OFF, 0 );
	CHECK( !p.temporalRequested && p.smaaQuality == 0 );

	// 1: TAA only.
	p = RenderAntialiasing_Resolve( RENDER_AA_TAA, 0 );
	CHECK( p.temporalRequested && p.smaaQuality == 0 );

	// 2: SMAA only, at the one preset.
	p = RenderAntialiasing_Resolve( RENDER_AA_SMAA, 0 );
	CHECK( !p.temporalRequested && p.smaaQuality == RENDER_AA_SMAA_QUALITY );

	// The diagnostic forces the temporal path and never changes SMAA.
	p = RenderAntialiasing_Resolve( RENDER_AA_OFF, 1 );
	CHECK( p.temporalRequested && p.smaaQuality == 0 );
	p = RenderAntialiasing_Resolve( RENDER_AA_SMAA, 1 );
	CHECK( p.temporalRequested && p.smaaQuality == RENDER_AA_SMAA_QUALITY );

	// Out-of-range modes behave as off.
	p = RenderAntialiasing_Resolve( 7, 0 );
	CHECK( !p.temporalRequested && p.smaaQuality == 0 );
	p = RenderAntialiasing_Resolve( -1, 0 );
	CHECK( !p.temporalRequested && p.smaaQuality == 0 );

	puts( "PASS render_antialiasing" );
	return 0;
}
