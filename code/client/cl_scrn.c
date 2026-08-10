// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"
#include "cl_debuggraph.h"
#include "wired/ui/cl_wired_ui.h"
#include "wired/ui/cl_wired_compositor.h"
#include "wired/ui/cl_wired_ui_hud_state.h"
#include "wired/ui/cl_wired_msdf.h"
#include "wired/ui/cl_wired_fonts.h"
#include "wired/ui/cl_wired_text.h"

static qboolean	scr_initialized;		// ready to draw
static qboolean	scr_updateActive;		// are we currently inside SCR_UpdateScreen?

/* cl_timegraph stays a non-static global: the timegraph producer in cl_main.c
 * reads it to feed SCR_DebugGraph. The debuggraph / graphheight / graphscale /
 * graphshift cvars are still registered below (SCR_Init) but read by name in
 * the relocated debug_graph custom-draw handler (Turn 4 V-29), so no named
 * pointers are kept here. */
cvar_t		*cl_timegraph;

/* net-stats overlay cvars — registered in cl_net_stats.c. The draw helpers
 * (SCR_DrawPing/Snaps/Packets) relocated to the WUI_LAYER_DEBUG_OVERLAY
 * custom-draw handlers in code/client/wired/ui/elements/debug_overlay.c
 * (Turn 4 V-29). */
void SCR_NetStatsInit( void );

/* The legacy 640x480 virtual-coordinate draw helpers were removed: the UI
 * draws in physical-pixel space (WiredUI/Clay), so the old virtual scaling and
 * the pic/rect/char helpers built on it had zero callers. */


/*
==================
SCR_DrawStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	int flags = TEXT_DROPSHADOW;

	(void)noColorEscape;

	if ( forceColor ) {
		flags |= TEXT_FORCECOLOR;
	}

	Text_Draw( string, (float)x, (float)y, FONT_DISPLAY, size, setColor, TEXT_ALIGN_LEFT, flags );
}


//===============================================================================

/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

/* Engine-owned graph ring buffer (Turn 4 V-29). Was a pair of bare file-scope
 * statics (`current` / `values[1024]`); hoisted into the named struct in
 * cl_debuggraph.h for explicit ownership (W-28). Single writer: SCR_DebugGraph
 * below. Single reader: the debug_graph custom-draw handler
 * (code/client/wired/ui/elements/debug_overlay.c). The legacy
 * SCR_DrawDebugGraph render moved there verbatim. */
debugGraphBuffer_t cl_debugGraphBuffer;

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph( float value )
{
	cl_debugGraphBuffer.values[cl_debugGraphBuffer.current] = value;
	cl_debugGraphBuffer.current = (cl_debugGraphBuffer.current + 1) % ARRAY_LEN(cl_debugGraphBuffer.values);
}

//=============================================================================

static const cvarDesc_t scrnDescs[] = {
	/* 0 */ CVAR_BOOL( "timegraph",   "0",  CVAR_CHEAT, NULL ),
	/* 1 */ CVAR_BOOL( "debuggraph",  "0",  CVAR_CHEAT, NULL ),
	/* 2 */ CVAR_INT(  "graphheight", "32", CVAR_CHEAT, NULL, 0, 0 ),
	/* 3 */ CVAR_INT(  "graphscale",  "1",  CVAR_CHEAT, NULL, 0, 0 ),
	/* 4 */ CVAR_INT(  "graphshift",  "0",  CVAR_CHEAT, NULL, 0, 0 ),
};

enum {
	SCR_TIMEGRAPH, SCR_DEBUGGRAPH, SCR_GRAPHHEIGHT, SCR_GRAPHSCALE, SCR_GRAPHSHIFT,
	SCR_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( scrnDescs ) == SCR_CVAR_COUNT, "scrnDescs/enum mismatch" );
static cvar_t *scrnHandles[SCR_CVAR_COUNT];


/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	Cvar_RegisterTable( scrnDescs, ARRAY_LEN( scrnDescs ), scrnHandles );
	cl_timegraph   = scrnHandles[SCR_TIMEGRAPH];
	/* debuggraph / graphheight / graphscale / graphshift remain registered
	 * (the table above) but are read by name in the relocated debug_graph
	 * custom-draw handler — no named pointers cached here. */

	SCR_NetStatsInit();
	scr_initialized = qtrue;
}


/*
==================
SCR_Done
==================
*/
void SCR_Done( void ) {
	scr_initialized = qfalse;
}


//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
static void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	int64_t scr_t0 = Sys_Microseconds();
	int scr_buckets_begin = cl_prof.cgr + cl_prof.whud + cl_prof.wui + cl_prof.cons;

	re.BeginFrame( stereoFrame );

#if FEAT_WIRED_UI
	/* V-15..V-19 (2026-05-25): single dispatch authority. The compositor
	 * owns the per-layer walk; the WORLD_VIEWPORT layer drives 3D scene
	 * rendering (V-19) and cinematic blit, the HUD layer drives the
	 * Turn 2 V-11 HUD tick + sync, the CONSOLE layer drives V-20
	 * elements/console.c, and the OVERLAY layer drives V-24 cursor /
	 * tooltip / transient sub-passes.
	 *
	 * V-17 retired: CL_CGameRendering direct call (now under
	 * WORLD_VIEWPORT walk), WiredHud_Routine pre-call (now in tick),
	 * separate WiredUI_Refresh tick (now in WiredUI_TickFrame), and
	 * V-18 retired: Con_DrawConsole direct call (now under CONSOLE
	 * walk via elements/console.c).
	 *
	 * Turn 4 V-29 (debug-overlay-migration): the 6 SCR_Draw* debug-overlay
	 * helpers (DemoRecording / VoipMeter / DebugGraph / Ping / Snaps /
	 * Packets) that used to fire here are now WUI_LAYER_DEBUG_OVERLAY
	 * custom-draw handlers (code/client/wired/ui/elements/debug_overlay.c),
	 * dispatched under the compositor walk via the declarative panels
	 * modfiles/ui/debug_graph.wui + debug_netstats.wui. This body is now just
	 * re.BeginFrame + WiredUI_RenderFrame + the profiling bookkeeping. */
	CL_PROF(wui, WiredUI_RenderFrame());
#endif

	cl_prof.scrextra += (int)(Sys_Microseconds() - scr_t0)
	                  - (cl_prof.cgr + cl_prof.whud + cl_prof.wui + cl_prof.cons - scr_buckets_begin);
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void CL_AbortFrame( void ) {
	// Reset the SCR_UpdateScreen guard flag.
	// Called from Com_Frame's longjmp recovery path so that an ERR_DROP
	// thrown mid-render does not leave us permanently "in an update".
	scr_updateActive = qfalse;
}

void SCR_UpdateScreen( void ) {
	static int framecount;
	static int next_frametime;

	if ( !scr_initialized )
		return; // not initialized yet

	if ( framecount == cls.framecount ) {
		int ms = Sys_Milliseconds();
		if ( next_frametime && ms - next_frametime < 0 ) {
			re.ThrottleBackend();
		} else {
			next_frametime = ms + 16; // limit to 60 FPS
		}
	} else {
		next_frametime = 0;
		framecount = cls.framecount;
	}

	// There are several legitimate cases where SCR_UpdateScreen is reached
	// twice in one frame (e.g. connect -> kicked -> connect). Instead of a
	// fatal error on recursion, just bail out silently.
	//
	// Why set to 1 and 0 explicitly (not increment/decrement)?
	// Because one of the calls below might invoke Com_Error, which will in
	// turn call longjmp and abort the current frame, meaning the end of
	// this function is not always reached. Com_Frame's longjmp handler
	// calls CL_AbortFrame() to reset this flag on that path.
	if ( scr_updateActive ) {
		return;
	}
	scr_updateActive = qtrue;

	// If there is no VM, there are also no rendering commands issued. Stop the renderer in
	// that case.
	if ( UI_VM_ACTIVE )
	{
		// XXX
		int in_anaglyphMode = Cvar_VariableIntegerValue("r_anaglyphMode");
		// if running in stereo, we need to draw the frame twice
		if ( cls.glconfig.stereoEnabled || in_anaglyphMode) {
			SCR_DrawScreenField( STEREO_LEFT );
			SCR_DrawScreenField( STEREO_RIGHT );
		} else {
			SCR_DrawScreenField( STEREO_CENTER );
		}

		if ( com_speeds->integer ) {
			CL_PROF(endframe, re.EndFrame( &time_frontend, &time_backend ));
		} else {
			CL_PROF(endframe, re.EndFrame( NULL, NULL ));
		}
	}

	scr_updateActive = qfalse;
}
