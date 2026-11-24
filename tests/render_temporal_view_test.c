// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* Temporal frame input shared by every renderer (render_temporal_view.h): only the primary scene with a world drives the history,
 * and a camera cut seen on any frame holds until a temporal frame commits. */

#include "render_temporal_view.h"

#include <stdio.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL render_temporal_view line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

int main( void ) {
	renderTemporalCutLatch_t latch = { qfalse };

	CHECK( RenderTemporal_PrimaryScene( RDF_TEMPORAL_PRIMARY ) );
	CHECK( RenderTemporal_PrimaryScene( RDF_TEMPORAL_PRIMARY | RDF_CAMERA_CUT ) );
	CHECK( !RenderTemporal_PrimaryScene( 0u ) );                                       // split screen, stereo eye, other scenes
	CHECK( !RenderTemporal_PrimaryScene( RDF_TEMPORAL_PRIMARY | RDF_NOWORLDMODEL ) );  // worldless UI / model scene

	CHECK( RenderTemporal_SceneCuts( RDF_CAMERA_CUT ) );
	CHECK( RenderTemporal_SceneCuts( RDF_HYPERSPACE ) );
	CHECK( !RenderTemporal_SceneCuts( RDF_TEMPORAL_PRIMARY ) );

	// A cut on a frame without a temporal plan (menu, loading) holds through frames without one.
	CHECK( !RenderTemporal_CutPending( &latch ) );
	RenderTemporal_ObserveCut( &latch, qtrue );
	RenderTemporal_ObserveCut( &latch, qfalse );
	RenderTemporal_ObserveCut( &latch, qfalse );
	CHECK( RenderTemporal_CutPending( &latch ) );
	// The temporal frame that carried it commits: consumed.
	RenderTemporal_CutConsumed( &latch );
	CHECK( !RenderTemporal_CutPending( &latch ) );
	RenderTemporal_ObserveCut( &latch, qfalse );
	CHECK( !RenderTemporal_CutPending( &latch ) );

	CHECK( !RenderTemporal_CutPending( NULL ) );
	RenderTemporal_ObserveCut( NULL, qtrue );
	RenderTemporal_CutConsumed( NULL );
	return 0;
}
