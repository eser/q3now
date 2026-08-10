// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * elements/debug_overlay.c — WUI_LAYER_DEBUG_OVERLAY custom-draw handlers.
 *
 * Turn 4 V-29 (debug-overlay-migration): the 6 legacy SCR_Draw* overlay
 * helpers that used to fire imperatively at the tail of
 * SCR_DrawScreenField (cl_scrn.c) now live here as unified-registry
 * custom-draw handlers, dispatched by the compositor's
 * WUI_LAYER_DEBUG_OVERLAY walk via the declarative panels
 * modfiles/ui/debug_graph.wui + modfiles/ui/debug_netstats.wui.
 *
 *   custom:debug_demo_recording  — stateless (live clc demo state)
 *   custom:debug_voip_meter      — stateless (live VoIP state; #ifdef USE_VOIP)
 *   custom:debug_graph           — stateless (reads cl_debugGraphBuffer)
 *   custom:debug_ping            — stateful  (cross-frame ping history)
 *   custom:debug_snaps           — stateful  (cross-frame SPS history)
 *   custom:debug_packets         — stateful  (cross-frame PPS history)
 *
 * W-28 (NON-NEGOTIABLE): NO file-scope mutable state. The three stateful
 * handlers hold ALL cross-frame history + counters + computed stats in an
 * arena-owned context struct allocated from the SAME HUD arena the FPS
 * element uses (WiredHud_ArenaAlloc), created once per item and held across
 * frames (the arena is Arena_Reset only at menu teardown, never per-frame).
 * The graph buffer is hoisted into the engine-owned cl_debugGraphBuffer
 * (cl_debuggraph.h). The only file-scope statics here are const lookup
 * tables and registry typedefs — never mutable instances.
 *
 * Arena vs. legacy realloc: the legacy code malloc/realloc'd the history in
 * 128-sample blocks (unbounded). The arena has no realloc, so each window
 * holds a FIXED-CAP array of DEBUG_NETSTAT_SAMPLES (1024) samples. The
 * window closes by TIME (interval seconds), not by sample count; the cap is
 * generous (worst case ~10s × ~60 samples/s = 600 < 1024). Overflow guard:
 * if a window would exceed the cap before its time elapses, we simply stop
 * appending (clamp count at cap) — the time-based window close still fires
 * and computes stats from the samples gathered so far. This bounds memory
 * without changing the observable stats in any realistic session.
 */

#include "../../../client.h"
#include "../../../cl_debuggraph.h"
#include "cl_wired_ui_hud_private.h"   /* WiredHud_ArenaAlloc */
#include "../cl_wired_customdraw.h"
#include "../cl_wired_text.h"

#include <math.h>

#if FEAT_WIRED_UI

/* Fixed per-window sample cap (see file header — replaces the legacy
 * unbounded 128-block realloc). */
#define DEBUG_NETSTAT_SAMPLES 1024

/* clc demo-recording cvar (externed in client.h). */
/* cl_drawRecording, cl_debugMove — externed in client.h. */

/* The net-stat draw cvars live in cl_net_stats.c (SCR_NetStatsInit registers
 * them under CVAR_ARCHIVE). The handlers read them by name through
 * Cvar_VariableValue so we do not have to widen cl_net_stats.c's API or
 * create any new cvar here. */

/* =====================================================================
 * Stateless: demo-recording text overlay
 * ===================================================================== */

void WiredDebug_DemoRecording_Draw( float x, float y, float w, float h, vec4_t color )
{
	char string[ sizeof( clientActiveApp->clc.recordNameShort ) + 32 ];
	int  pos;

	(void)x; (void)y; (void)w; (void)h; (void)color;

	if ( !clientActiveApp->clc.demorecording ) {
		return;
	}
	if ( clientActiveApp->clc.spDemoRecording ) {
		return;
	}

	pos = FS_FTell( clientActiveApp->clc.recordfile );

	if ( cl_drawRecording->integer == 1 ) {
		sprintf( string, "RECORDING %s: %ik", clientActiveApp->clc.recordNameShort, pos / 1024 );
		Text_Draw( string, (float)( 320 - strlen( string ) * 4 ), 20.0f, FONT_DISPLAY, 8.0f,
		           g_color_table[ ColorIndex( COLOR_WHITE ) ], TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW );
	} else if ( cl_drawRecording->integer == 2 ) {
		sprintf( string, "RECORDING: %ik", pos / 1024 );
		Text_Draw( string, (float)( 320 - strlen( string ) * 4 ), 20.0f, FONT_DISPLAY, 8.0f,
		           g_color_table[ ColorIndex( COLOR_WHITE ) ], TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW );
	}
}

/* =====================================================================
 * Stateless: VoIP send meter (live state; only built under USE_VOIP)
 * ===================================================================== */

#ifdef USE_VOIP
void WiredDebug_VoipMeter_Draw( float x, float y, float w, float h, vec4_t color )
{
	char buffer[ 16 ];
	char string[ 256 ];
	int  i;

	(void)x; (void)y; (void)w; (void)h; (void)color;

	if ( !cl_voipShowMeter->integer )
		return;  // player doesn't want to show meter at all.
	else if ( !cl_voipSend->integer )
		return;  // not recording at the moment.
	else if ( clientActiveApp->clc.state != CA_ACTIVE )
		return;  // not connected to a server.
	else if ( !clientActiveApp->clc.voipEnabled )
		return;  // server doesn't support VoIP.
	else if ( clientActiveApp->clc.demoplaying )
		return;  // playing back a demo.
	else if ( !cl_voip->integer )
		return;  // client has VoIP support disabled.

	int limit = (int)( clientActiveApp->clc.voipPower * 10.0f );
	if ( limit > 10 )
		limit = 10;

	for ( i = 0; i < limit; i++ )
		buffer[ i ] = '*';
	while ( i < 10 )
		buffer[ i++ ] = ' ';
	buffer[ i ] = '\0';

	sprintf( string, "VoIP: [%s]", buffer );
	Text_Draw( string, (float)( 320 - strlen( string ) * 4 ), 10.0f, FONT_DISPLAY, 8.0f,
	           g_color_table[ ColorIndex( COLOR_WHITE ) ], TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW );
}
#else
/* Non-VOIP builds: register a silent no-op stub so the
 * debug_voip_meter itemDef in debug_netstats.wui resolves cleanly (the .wui
 * ships once, build-agnostic). HALT-4 satisfied: no VoIP-active code on a
 * non-VOIP build. */
static void WiredDebug_VoipMeter_Stub( float x, float y, float w, float h, vec4_t color )
{
	(void)x; (void)y; (void)w; (void)h; (void)color;
}
#endif

/* =====================================================================
 * Stateless: debug graph (reads engine-owned cl_debugGraphBuffer)
 * ===================================================================== */

void WiredDebug_Graph_Draw( float x, float y, float w, float h, vec4_t color )
{
	(void)x; (void)y; (void)w; (void)h; (void)color;

	/* Gate preserved verbatim from the legacy call site in
	 * SCR_DrawScreenField: the graph drew only when any of debuggraph /
	 * timegraph / cl_debugMove was set. (debuggraph + timegraph register as
	 * bare names in cl_scrn.c's SCR_Init; cl_debugMove keeps its cl_ prefix.) */
	if ( !(int)Cvar_VariableValue( "debuggraph" )
	  && !(int)Cvar_VariableValue( "timegraph" )
	  && !(int)Cvar_VariableValue( "cl_debugMove" ) ) {
		return;
	}

	//
	// draw the graph (verbatim from the legacy SCR_DrawDebugGraph)
	//
	int graphW = cls.glconfig.vidWidth;
	int graphX = 0;
	int graphY = cls.glconfig.vidHeight;
	int graphHeight = (int)Cvar_VariableValue( "graphheight" );
	int graphScale  = (int)Cvar_VariableValue( "graphscale" );
	int graphShift  = (int)Cvar_VariableValue( "graphshift" );

	if ( graphHeight <= 0 ) {
		return;
	}

	re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
	re.DrawStretchPic( graphX, graphY - graphHeight, graphW, graphHeight, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for ( int a = 0; a < graphW; a++ ) {
		int i = ( DEBUG_GRAPH_SAMPLES + cl_debugGraphBuffer.current - 1 - ( a % DEBUG_GRAPH_SAMPLES ) ) % DEBUG_GRAPH_SAMPLES;
		float v = cl_debugGraphBuffer.values[ i ];
		v = v * graphScale + graphShift;

		if ( v < 0 )
			v += graphHeight * ( 1 + (int)( -v / graphHeight ) );
		int hh = (int)v % graphHeight;
		re.DrawStretchPic( graphX + graphW - 1 - a, graphY - hh, 1, hh, 0, 0, 0, 0, cls.whiteShader );
	}
}

/* =====================================================================
 * Stateful: ping statistics overlay
 * ===================================================================== */

/* All cross-frame state the legacy SCR_DrawPing kept in file-scope statics,
 * relocated into an arena-owned context (W-28). The two history arrays are
 * fixed-cap (DEBUG_NETSTAT_SAMPLES) instead of the legacy 128-block realloc. */
typedef struct {
	int   pings[ DEBUG_NETSTAT_SAMPLES ];
	int   pings2[ DEBUG_NETSTAT_SAMPLES ];
	int   timeRec, timeRec2, count, count2,
	      acount, acount2, avgPing, avgPing2,
	      flux, flux2, aflux, aflux2, std, std2;
	unsigned long long int sum, sum2, asum, asum2, stsum, stsum2;
} WiredDebugPingCtx_t;

void *WiredDebug_Ping_Create( const wuiCustomDrawConfig_t *cfg )
{
	WiredDebugPingCtx_t *ctx;
	(void)cfg;
	ctx = (WiredDebugPingCtx_t *)WiredHud_ArenaAlloc( sizeof( *ctx ) );
	if ( ctx ) {
		memset( ctx, 0, sizeof( *ctx ) );
	}
	return ctx;
}

void WiredDebug_Ping_Routine( void *context, float x, float y, float w, float h, vec4_t color )
{
	WiredDebugPingCtx_t *c = (WiredDebugPingCtx_t *)context;
	char string1[ 64 ], string2[ 64 ], string3[ 64 ], string4[ 64 ], string5[ 64 ], string6[ 64 ];

	(void)x; (void)y; (void)w; (void)h; (void)color;

	if ( !(int)Cvar_VariableValue( "cl_drawPing" ) ) return;

	int timeNew  = Sys_Milliseconds();
	int currPing = clientActiveApp->cl.snap.ping;
	int interval  = (int)Cvar_VariableValue( "cl_drawPingFirstInterval" );
	int interval2 = (int)Cvar_VariableValue( "cl_drawPingSecondInterval" );
	int fontsize  = (int)Cvar_VariableValue( "cl_drawPingFontSize" );
	int posx      = (int)Cvar_VariableValue( "cl_drawPingPosX" );
	int posy      = (int)Cvar_VariableValue( "cl_drawPingPosY" );

	if ( interval  <= 0 ) interval  = 1;
	if ( interval2 <= 0 ) interval2 = 1;

	if ( !c->timeRec )
		c->timeRec = c->timeRec2 = timeNew;

	/* --- window 1 --- */
	if ( ( timeNew - c->timeRec ) / 1000 < interval ) {
		if ( c->count < DEBUG_NETSTAT_SAMPLES )
			c->pings[ c->count++ ] = currPing;
	} else if ( c->count ) {
		for ( int i = 0; i < c->count; i++ ) c->sum += c->pings[ i ];
		c->avgPing = (int)( c->sum / c->count );
		c->flux = 0;
		for ( int i = 0; i < c->count; i++ ) {
			c->stsum += (unsigned long long int)pow( (double)( c->pings[i] - c->avgPing ), 2 );
			if ( c->pings[i] > c->avgPing ) {
				c->acount++;
				c->asum += (unsigned long long int)( c->pings[i] - c->avgPing );
				if ( c->pings[i] > c->flux + c->avgPing ) c->flux = c->pings[i] - c->avgPing;
			}
		}
		// NOLINTNEXTLINE(bugprone-integer-division) — variance calc; integer division is intentional for stats display
		c->std   = (int)sqrt( (double)( c->stsum / c->count ) );
		c->aflux = c->acount ? (int)( c->asum / c->acount ) : 0;
		c->timeRec = timeNew;
		c->sum = c->count = c->asum = c->acount = c->stsum = 0;
	}

	/* --- window 2 (identical logic) --- */
	if ( ( timeNew - c->timeRec2 ) / 1000 < interval2 ) {
		if ( c->count2 < DEBUG_NETSTAT_SAMPLES )
			c->pings2[ c->count2++ ] = currPing;
	} else if ( c->count2 ) {
		for ( int i = 0; i < c->count2; i++ ) c->sum2 += c->pings2[ i ];
		c->avgPing2 = (int)( c->sum2 / c->count2 );
		c->flux2 = 0;
		for ( int i = 0; i < c->count2; i++ ) {
			c->stsum2 += (unsigned long long int)pow( (double)( c->pings2[i] - c->avgPing2 ), 2 );
			if ( c->pings2[i] > c->avgPing2 ) {
				c->acount2++;
				c->asum2 += (unsigned long long int)( c->pings2[i] - c->avgPing2 );
				if ( c->pings2[i] > c->flux2 + c->avgPing2 ) c->flux2 = c->pings2[i] - c->avgPing2;
			}
		}
		// NOLINTNEXTLINE(bugprone-integer-division) — variance calc; integer division is intentional for stats display
		c->std2   = (int)sqrt( (double)( c->stsum2 / c->count2 ) );
		c->aflux2 = c->acount2 ? (int)( c->asum2 / c->acount2 ) : 0;
		c->timeRec2 = timeNew;
		c->sum2 = c->count2 = c->asum2 = c->acount2 = c->stsum2 = 0;
	}

	Com_sprintf( string1, sizeof( string1 ), "%3ims", currPing );
	Com_sprintf( string2, sizeof( string2 ), "Interval   %3is  %3is", interval, interval2 );
	Com_sprintf( string3, sizeof( string3 ), "Mean      %3ims %3ims", c->avgPing,  c->avgPing2 );
	Com_sprintf( string4, sizeof( string4 ), "Max Spike %3ims %3ims", c->flux,     c->flux2    );
	Com_sprintf( string5, sizeof( string5 ), "Avg Spike %3ims %3ims", c->aflux,    c->aflux2   );
	Com_sprintf( string6, sizeof( string6 ), "Std Dev   %3ims %3ims", c->std,      c->std2     );

	int posxx = ( posx >= 0 && posx < 21 ) ? posx * 30 : 0;
	int posyy = ( posy >= 0 && posy < 24 ) ? posy * 20 : 0;

	SCR_DrawStringExt( posxx, posyy,                  fontsize, string1, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*1, fontsize, string2, g_color_table[4], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*2, fontsize, string3, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*3, fontsize, string4, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*4, fontsize, string5, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*5, fontsize, string6, g_color_table[7], qtrue, qtrue );
}

/* =====================================================================
 * Stateful: snapshot-rate (SPS) statistics overlay
 * ===================================================================== */

/* Per-sample record (was a file-scope typedef + realloc'd array in
 * cl_net_stats.c). Kept as a TYPE here; instances live inside the arena
 * context arrays. */
typedef struct {
	int      sps;
	int      fps;
	int      state;
	qboolean extrapolated;
} s_snaps_t;

typedef struct {
	s_snaps_t snapses[ DEBUG_NETSTAT_SAMPLES ];
	s_snaps_t snapses2[ DEBUG_NETSTAT_SAMPLES ];
	int   sps, omsg, otime, timeRec, timeRec2,
	      count, count2, avgSps, avgSps2,
	      sdrop, sdrop2, asdrop, asdrop2, dcount, dcount2,
	      stdSps, stdSps2, delayed, delayed2,
	      extrap, extrap2, fps, otimeFps,
	      avgFps, avgFps2;
	float coef, coef2, delayedF, delayedF2, extrapF, extrapF2;
	unsigned long long int sumSps, sumSps2, snsum, snsum2, stsum, stsum2, sumFps, sumFps2;
} WiredDebugSnapsCtx_t;

void *WiredDebug_Snaps_Create( const wuiCustomDrawConfig_t *cfg )
{
	WiredDebugSnapsCtx_t *ctx;
	(void)cfg;
	ctx = (WiredDebugSnapsCtx_t *)WiredHud_ArenaAlloc( sizeof( *ctx ) );
	if ( ctx ) {
		memset( ctx, 0, sizeof( *ctx ) );
	}
	return ctx;
}

void WiredDebug_Snaps_Routine( void *context, float x, float y, float w, float h, vec4_t color )
{
	WiredDebugSnapsCtx_t *c = (WiredDebugSnapsCtx_t *)context;
	char string1[ 64 ], string2[ 64 ], string3[ 64 ], string4[ 64 ],
	     string5[ 64 ], string6[ 64 ], string7[ 64 ], string8[ 64 ];

	(void)x; (void)y; (void)w; (void)h; (void)color;

	if ( !(int)Cvar_VariableValue( "cl_drawSnaps" ) ) return;

	int cmsg  = clientActiveApp->cl.snap.messageNum;
	int ctime = Sys_Milliseconds();

	if ( !c->omsg ) { c->omsg = cmsg; c->otime = ctime; c->otimeFps = ctime; }

	if ( ctime - c->otimeFps )
		{ c->fps = 1000 / ( ctime - c->otimeFps ); c->otimeFps = ctime; }

	if ( ctime - c->otime && cmsg - c->omsg )
		{ c->sps = 1000 * ( cmsg - c->omsg ) / ( ctime - c->otime ); c->omsg = cmsg; c->otime = ctime; }

	int interval  = (int)Cvar_VariableValue( "cl_drawSnapsFirstInterval" );
	int interval2 = (int)Cvar_VariableValue( "cl_drawSnapsSecondInterval" );
	int fontsize  = (int)Cvar_VariableValue( "cl_drawSnapsFontSize" );
	int posx      = (int)Cvar_VariableValue( "cl_drawSnapsPosX" );
	int posy      = (int)Cvar_VariableValue( "cl_drawSnapsPosY" );

	if ( interval  <= 0 ) interval  = 1;
	if ( interval2 <= 0 ) interval2 = 1;

	if ( !c->timeRec ) c->timeRec = c->timeRec2 = ctime;

	/* --- window 1 --- */
	if ( ( ctime - c->timeRec ) / 1000 < interval ) {
		if ( c->count < DEBUG_NETSTAT_SAMPLES ) {
			c->snapses[ c->count ].sps          = c->sps;
			c->snapses[ c->count ].state        = clientActiveApp->cl.snap.snapFlags;
			c->snapses[ c->count ].extrapolated = clientActiveApp->cl.extrapolatedSnapshot;
			c->snapses[ c->count ].fps          = c->fps;
			c->count++;
		}
	} else if ( c->count ) {
		for ( int i = 0; i < c->count; i++ ) { c->sumSps += c->snapses[i].sps; c->sumFps += c->snapses[i].fps; }
		c->avgSps = (int)( c->sumSps / c->count );
		c->avgFps = (int)( c->sumFps / c->count );
		c->coef = c->avgFps ? ( (float)c->avgSps / (float)c->avgFps / (float)interval ) : 0;
		c->sdrop = c->delayed = c->extrap = 0;
		for ( int i = 0; i < c->count; i++ ) {
			c->stsum += (unsigned long long int)pow( (double)( c->snapses[i].sps - c->avgSps ), 2 );
			if ( c->snapses[i].sps < c->avgSps ) {
				c->dcount++;
				c->snsum += (unsigned long long int)( c->avgSps - c->snapses[i].sps );
				if ( c->snapses[i].sps < c->avgSps - c->sdrop ) c->sdrop = c->avgSps - c->snapses[i].sps;
			}
			if ( c->snapses[i].state & SNAPFLAG_RATE_DELAYED ) c->delayed++;
			if ( c->snapses[i].extrapolated == qtrue )         c->extrap++;
		}
		c->delayedF = (float)c->delayed * c->coef;
		c->extrapF  = (float)c->extrap  * c->coef;
		// NOLINTNEXTLINE(bugprone-integer-division) — variance calc; integer division is intentional for stats display
		c->stdSps   = (int)sqrt( (double)( c->stsum / c->count ) );
		c->asdrop   = c->dcount ? (int)( c->snsum / c->dcount ) : 0;
		c->timeRec  = ctime;
		c->sumSps = c->count = c->stsum = c->snsum = c->dcount = c->sumFps = 0;
	}

	/* --- window 2 --- */
	if ( ( ctime - c->timeRec2 ) / 1000 < interval2 ) {
		if ( c->count2 < DEBUG_NETSTAT_SAMPLES ) {
			c->snapses2[ c->count2 ].sps          = c->sps;
			c->snapses2[ c->count2 ].state        = clientActiveApp->cl.snap.snapFlags;
			c->snapses2[ c->count2 ].extrapolated = clientActiveApp->cl.extrapolatedSnapshot;
			c->snapses2[ c->count2 ].fps          = c->fps;
			c->count2++;
		}
	} else if ( c->count2 ) {
		for ( int i = 0; i < c->count2; i++ ) { c->sumSps2 += c->snapses2[i].sps; c->sumFps2 += c->snapses2[i].fps; }
		c->avgSps2 = (int)( c->sumSps2 / c->count2 );
		c->avgFps2 = (int)( c->sumFps2 / c->count2 );
		c->coef2   = c->avgFps2 ? ( (float)c->avgSps2 / (float)c->avgFps2 / (float)interval2 ) : 0;
		c->sdrop2 = c->delayed2 = c->extrap2 = 0;
		for ( int i = 0; i < c->count2; i++ ) {
			c->stsum2 += (unsigned long long int)pow( (double)( c->snapses2[i].sps - c->avgSps2 ), 2 );
			if ( c->snapses2[i].sps < c->avgSps2 ) {
				c->dcount2++;
				c->snsum2 += (unsigned long long int)( c->avgSps2 - c->snapses2[i].sps );
				if ( c->snapses2[i].sps < c->avgSps2 - c->sdrop2 ) c->sdrop2 = c->avgSps2 - c->snapses2[i].sps;
			}
			if ( c->snapses2[i].state & SNAPFLAG_RATE_DELAYED ) c->delayed2++;
			if ( c->snapses2[i].extrapolated == qtrue )          c->extrap2++;
		}
		c->delayedF2 = (float)c->delayed2 * c->coef2;
		c->extrapF2  = (float)c->extrap2  * c->coef2;
		// NOLINTNEXTLINE(bugprone-integer-division) — variance calc; integer division is intentional for stats display
		c->stdSps2   = (int)sqrt( (double)( c->stsum2 / c->count2 ) );
		c->asdrop2   = c->dcount2 ? (int)( c->snsum2 / c->dcount2 ) : 0;
		c->timeRec2  = ctime;
		c->sumSps2 = c->count2 = c->stsum2 = c->snsum2 = c->dcount2 = c->sumFps2 = 0;
	}

	Com_sprintf( string1, sizeof( string1 ), "%3iSPS",                  c->sps );
	Com_sprintf( string2, sizeof( string2 ), "Interval   %3is   %3is",  interval,   interval2 );
	Com_sprintf( string3, sizeof( string3 ), "Mean     %3iSPS %3iSPS",  c->avgSps,  c->avgSps2   );
	Com_sprintf( string4, sizeof( string4 ), "Max Drop %3iSPS %3iSPS",  c->sdrop,   c->sdrop2    );
	Com_sprintf( string5, sizeof( string5 ), "Avg Drop %3iSPS %3iSPS",  c->asdrop,  c->asdrop2   );
	Com_sprintf( string6, sizeof( string6 ), "Std Dev  %3iSPS %3iSPS",  c->stdSps,  c->stdSps2   );
	Com_sprintf( string7, sizeof( string7 ), "Delayed  %.1fSPS %.1fSPS",c->delayedF,c->delayedF2 );
	Com_sprintf( string8, sizeof( string8 ), "Extrap.  %.1fSPS %.1fSPS",c->extrapF, c->extrapF2  );

	int posxx = ( posx >= 0 && posx < 21 ) ? posx * 30 : 0;
	int posyy = ( posy >= 0 && posy < 24 ) ? posy * 20 : 0;

	SCR_DrawStringExt( posxx, posyy,                  fontsize, string1, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*1, fontsize, string2, g_color_table[3], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*2, fontsize, string3, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*3, fontsize, string4, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*4, fontsize, string5, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*5, fontsize, string6, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*6, fontsize, string7, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*7, fontsize, string8, g_color_table[7], qtrue, qtrue );
}

/* =====================================================================
 * Stateful: outbound packet-rate (PPS) statistics overlay
 * ===================================================================== */

typedef struct {
	int   packs[ DEBUG_NETSTAT_SAMPLES ];
	int   packs2[ DEBUG_NETSTAT_SAMPLES ];
	int   pps, count, count2, timeRec, timeRec2, lastRecorded,
	      avgPps, avgPps2, mdrop, mdrop2, adrop, adrop2, dcount, dcount2,
	      std, std2;
	unsigned long long int sum, sum2, stsum, stsum2, dsum, dsum2;
} WiredDebugPacketsCtx_t;

void *WiredDebug_Packets_Create( const wuiCustomDrawConfig_t *cfg )
{
	WiredDebugPacketsCtx_t *ctx;
	(void)cfg;
	ctx = (WiredDebugPacketsCtx_t *)WiredHud_ArenaAlloc( sizeof( *ctx ) );
	if ( ctx ) {
		memset( ctx, 0, sizeof( *ctx ) );
	}
	return ctx;
}

void WiredDebug_Packets_Routine( void *context, float x, float y, float w, float h, vec4_t color )
{
	WiredDebugPacketsCtx_t *c = (WiredDebugPacketsCtx_t *)context;
	char string1[ 64 ], string2[ 64 ], string3[ 64 ], string4[ 64 ], string5[ 64 ], string6[ 64 ];

	(void)x; (void)y; (void)w; (void)h; (void)color;

	if ( !(int)Cvar_VariableValue( "cl_drawPackets" ) ) return;

	int interval  = (int)Cvar_VariableValue( "cl_drawPacketsFirstInterval" );
	int interval2 = (int)Cvar_VariableValue( "cl_drawPacketsSecondInterval" );
	int fontsize  = (int)Cvar_VariableValue( "cl_drawPacketsFontSize" );
	int posx      = (int)Cvar_VariableValue( "cl_drawPacketsPosX" );
	int posy      = (int)Cvar_VariableValue( "cl_drawPacketsPosY" );

	int newtime = Sys_Milliseconds();

	if ( !c->timeRec ) c->timeRec = c->timeRec2 = c->lastRecorded = newtime;
	if ( interval  <= 0 ) interval  = 1;
	if ( interval2 <= 0 ) interval2 = 1;

	if ( cl_sent && ( newtime - c->lastRecorded ) ) {
		c->pps = 1000 / ( newtime - c->lastRecorded );
		c->lastRecorded = newtime;
	}

	/* --- window 1 --- */
	if ( ( newtime - c->timeRec ) / 1000 < interval ) {
		if ( c->count < DEBUG_NETSTAT_SAMPLES )
			c->packs[ c->count++ ] = c->pps;
	} else if ( c->count ) {
		for ( int i = 0; i < c->count; i++ ) c->sum += c->packs[ i ];
		c->avgPps = (int)( c->sum / c->count );
		c->mdrop  = 0;
		for ( int i = 0; i < c->count; i++ ) {
			c->stsum += (unsigned long long int)pow( (double)( c->packs[i] - c->avgPps ), 2 );
			if ( c->packs[i] < c->avgPps ) {
				c->dcount++;
				c->dsum += (unsigned long long int)( c->avgPps - c->packs[i] );
				if ( c->packs[i] < c->avgPps - c->mdrop ) c->mdrop = c->avgPps - c->packs[i];
			}
		}
		// NOLINTNEXTLINE(bugprone-integer-division) — variance calc; integer division is intentional for stats display
		c->std   = (int)sqrt( (double)( c->stsum / c->count ) );
		c->adrop = c->dcount ? (int)( c->dsum / c->dcount ) : 0;
		c->timeRec = newtime;
		c->sum = c->count = c->stsum = c->dsum = c->dcount = 0;
	}

	/* --- window 2 --- */
	if ( ( newtime - c->timeRec2 ) / 1000 < interval2 ) {
		if ( c->count2 < DEBUG_NETSTAT_SAMPLES )
			c->packs2[ c->count2++ ] = c->pps;
	} else if ( c->count2 ) {
		for ( int i = 0; i < c->count2; i++ ) c->sum2 += c->packs2[ i ];
		c->avgPps2 = (int)( c->sum2 / c->count2 );
		c->mdrop2  = 0;
		for ( int i = 0; i < c->count2; i++ ) {
			c->stsum2 += (unsigned long long int)pow( (double)( c->packs2[i] - c->avgPps2 ), 2 );
			if ( c->packs2[i] < c->avgPps2 ) {
				c->dcount2++;
				c->dsum2 += (unsigned long long int)( c->avgPps2 - c->packs2[i] );
				if ( c->packs2[i] < c->avgPps2 - c->mdrop2 ) c->mdrop2 = c->avgPps2 - c->packs2[i];
			}
		}
		// NOLINTNEXTLINE(bugprone-integer-division) — variance calc; integer division is intentional for stats display
		c->std2   = (int)sqrt( (double)( c->stsum2 / c->count2 ) );
		c->adrop2 = c->dcount2 ? (int)( c->dsum2 / c->dcount2 ) : 0;
		c->timeRec2 = newtime;
		c->sum2 = c->count2 = c->stsum2 = c->dsum2 = c->dcount2 = 0;
	}

	Com_sprintf( string1, sizeof( string1 ), "%3iPPS",                  c->pps     );
	Com_sprintf( string2, sizeof( string2 ), "Interval   %3is   %3is",  interval,   interval2 );
	Com_sprintf( string3, sizeof( string3 ), "Mean     %3iPPS %3iPPS",  c->avgPps,  c->avgPps2   );
	Com_sprintf( string4, sizeof( string4 ), "Max Drop %3iPPS %3iPPS",  c->mdrop,   c->mdrop2    );
	Com_sprintf( string5, sizeof( string5 ), "Avg Drop %3iPPS %3iPPS",  c->adrop,   c->adrop2    );
	Com_sprintf( string6, sizeof( string6 ), "Std Dev  %3iPPS %3iPPS",  c->std,     c->std2      );

	int posxx = ( posx >= 0 && posx < 21 ) ? posx * 30 : 0;
	int posyy = ( posy >= 0 && posy < 24 ) ? posy * 20 : 0;

	SCR_DrawStringExt( posxx, posyy,                  fontsize, string1, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*1, fontsize, string2, g_color_table[2], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*2, fontsize, string3, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*3, fontsize, string4, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*4, fontsize, string5, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*5, fontsize, string6, g_color_table[7], qtrue, qtrue );
}

/* =====================================================================
 * Registration
 * ===================================================================== */

void WiredDebugOverlay_RegisterAll( void )
{
	wuiCustomDrawDef_t def;

	/* demo recording — stateless */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:debug_demo_recording", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = WiredDebug_DemoRecording_Draw;
	WiredUI_RegisterCustomDraw( &def );

	/* VoIP send meter — stateless. The .wui itemDef is build-agnostic; on a
	 * non-VOIP build a silent no-op stub is registered so the lookup resolves
	 * cleanly (HALT-4: no VoIP-active code on non-VOIP). */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:debug_voip_meter", sizeof( def.name ) );
	def.isStateful        = qfalse;
#ifdef USE_VOIP
	def.routine.stateless = WiredDebug_VoipMeter_Draw;
#else
	def.routine.stateless = WiredDebug_VoipMeter_Stub;
#endif
	WiredUI_RegisterCustomDraw( &def );

	/* debug graph — stateless */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:debug_graph", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = WiredDebug_Graph_Draw;
	WiredUI_RegisterCustomDraw( &def );

	/* ping stats — stateful */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:debug_ping", sizeof( def.name ) );
	def.isStateful       = qtrue;
	def.create           = WiredDebug_Ping_Create;
	def.destroy          = NULL;   /* arena bulk-reclaims at menu teardown */
	def.routine.stateful = WiredDebug_Ping_Routine;
	WiredUI_RegisterCustomDraw( &def );

	/* snaps stats — stateful */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:debug_snaps", sizeof( def.name ) );
	def.isStateful       = qtrue;
	def.create           = WiredDebug_Snaps_Create;
	def.destroy          = NULL;
	def.routine.stateful = WiredDebug_Snaps_Routine;
	WiredUI_RegisterCustomDraw( &def );

	/* packets stats — stateful */
	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:debug_packets", sizeof( def.name ) );
	def.isStateful       = qtrue;
	def.create           = WiredDebug_Packets_Create;
	def.destroy          = NULL;
	def.routine.stateful = WiredDebug_Packets_Routine;
	WiredUI_RegisterCustomDraw( &def );
}

#endif /* FEAT_WIRED_UI */
