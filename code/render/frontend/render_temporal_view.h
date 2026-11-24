// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_TEMPORAL_VIEW_H
#define WIRED_RENDER_TEMPORAL_VIEW_H

/* The frame input every renderer gives the temporal history planner (ral_temporal.c): which scene drives the history, the frame
 * number and the camera cut. Header-only, renderer-neutral.
 *
 * - Primary scene: cgame marks the full-window centre-eye gameplay scene with RDF_TEMPORAL_PRIMARY (cg_draw.c clears it for split
 *   screen and stereo eyes). Only that scene, with a world, accumulates history; worldless UI/model scenes and other views do not.
 * - Frame number: a counter of every rendered frame, not of temporal frames. Frames without a temporal plan (menu, pause, loading
 *   screens) then leave a gap, and the planner resets the history instead of reusing an image from before the gap.
 * - Camera cut: a cut (RDF_CAMERA_CUT, or the RDF_HYPERSPACE teleport effect) on any frame, one without a temporal plan included, is
 *   latched and cuts the next temporal frame; the latch clears only when a temporal frame commits. */

#include "../../qcommon/q_shared.h"
#include "tr_types.h"

static inline qboolean RenderTemporal_PrimaryScene( uint32_t rdflags ) {
	return ( rdflags & RDF_TEMPORAL_PRIMARY ) && !( rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;
}

static inline qboolean RenderTemporal_SceneCuts( uint32_t rdflags ) {
	return ( rdflags & ( RDF_CAMERA_CUT | RDF_HYPERSPACE ) ) ? qtrue : qfalse;
}

typedef struct {
	qboolean pending;
} renderTemporalCutLatch_t;

/* Once per rendered frame, whether or not it plans a temporal frame. */
static inline void RenderTemporal_ObserveCut( renderTemporalCutLatch_t *latch, qboolean frameCut ) {
	if ( latch && frameCut ) latch->pending = qtrue;
}

static inline qboolean RenderTemporal_CutPending( const renderTemporalCutLatch_t *latch ) {
	return latch && latch->pending ? qtrue : qfalse;
}

/* A temporal frame committed: the cut it carried is consumed. */
static inline void RenderTemporal_CutConsumed( renderTemporalCutLatch_t *latch ) {
	if ( latch ) latch->pending = qfalse;
}

#endif
