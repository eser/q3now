// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_debuggraph.h — engine-owned debug-graph ring buffer.

The 1024-sample circular graph buffer used to be a pair of bare file-scope
statics (`current` / `values[1024]`) inside cl_scrn.c, fed single-writer via
SCR_DebugGraph(float) and consumed by the (now relocated) overlay draw helper.

Turn 4 V-29 (debug-overlay-migration): the consume side moved to the
WUI_LAYER_DEBUG_OVERLAY custom-draw handler in
code/client/wired/ui/elements/debug_overlay.c. To keep the buffer out of bare
file-scope statics (W-28: explicit ownership), it is hoisted into this named
engine-owned struct. SCR_DebugGraph (the single writer) still lives in
cl_scrn.c and writes cl_debugGraphBuffer; the producers (cl_main.c timegraph,
cl_input.c debugMove) and the client.h:574 extern of SCR_DebugGraph are
unchanged — no signature change.
*/

#ifndef CL_DEBUGGRAPH_H
#define CL_DEBUGGRAPH_H

#include "../qcommon/q_shared.h"

#define DEBUG_GRAPH_SAMPLES 1024

typedef struct {
	int   current;
	float values[ DEBUG_GRAPH_SAMPLES ];
} debugGraphBuffer_t;

/* Single owner: defined in cl_scrn.c, written only by SCR_DebugGraph,
 * read by the debug_graph custom-draw handler. */
extern debugGraphBuffer_t cl_debugGraphBuffer;

void SCR_DebugGraph( float value );

#endif /* CL_DEBUGGRAPH_H */
