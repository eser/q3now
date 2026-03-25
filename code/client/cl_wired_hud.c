/*
===========================================================================
cl_wired_hud.c — Wired UI HUD: client-side element rendering

Phase 3: SuperHUD elements run in the client. Game state is pushed by
cgame each frame via trap_WiredUI_PushHudState(). Elements read from
the wiredHud global instead of cgame globals.
===========================================================================
*/

#include "client.h"
#include "cl_wired_hud.h"
#include "cl_wired_fonts.h"

#if FEAT_WIRED_UI

// ── global HUD state ─────────────────────────────────────────────────

wiredHudState_t  wired_hudStateStorage;
wiredHudState_t *wiredHud = &wired_hudStateStorage;

// ── state receiver (called from cl_cgame.c trap handler) ─────────────

void WiredHud_ReceiveState( wiredHudState_t *state ) {
	if ( !state ) return;
	Com_Memcpy( &wired_hudStateStorage, state, sizeof( wiredHudState_t ) );
	wired_hudStateStorage.valid = qtrue;
}

// ── init / shutdown ──────────────────────────────────────────────────

void WiredHud_Init( void ) {
	Com_Memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
	CG_LoadFonts();
	Com_Printf( "WiredHud: initialized (Phase 3, %d fonts loaded)\n",
		CG_FontIndexFromName( NULL ) );  // returns 0 for unknown, proves fonts loaded
}

void WiredHud_Shutdown( void ) {
	Com_Memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
}

// ── prototype FPS element ─────────────────────────────────────────────
// Minimal fps counter to prove the state bridge + client rendering pipeline.
// This will be replaced by the full SuperHUD element migration in Step 4.

#define WIREDHUD_FPS_FRAMES  4

static struct {
	float   timeAverage;
	int     framesNum;
	int     timePrev;
} wiredHudFps;

static void WiredHud_DrawFps( int realtime ) {
	float fps_val;
	int fps_int;
	char buf[32];
	float x, y, charSize;
	vec4_t color = { 1.0f, 1.0f, 1.0f, 0.5f };

	// fps calculation (same algorithm as SuperHUD fps element)
	if ( wiredHudFps.timePrev == 0 ) {
		wiredHudFps.timePrev = realtime;
		return;
	}
	wiredHudFps.timeAverage *= wiredHudFps.framesNum;
	wiredHudFps.timeAverage += realtime - wiredHudFps.timePrev;
	wiredHudFps.timeAverage /= ++wiredHudFps.framesNum;
	wiredHudFps.timePrev = realtime;

	if ( wiredHudFps.framesNum > WIREDHUD_FPS_FRAMES ) {
		wiredHudFps.framesNum = WIREDHUD_FPS_FRAMES;
	}

	if ( wiredHudFps.timeAverage <= 0 ) return;

	fps_val = 1000.0f / wiredHudFps.timeAverage;
	fps_int = (int)( fps_val + 0.5f );

	Com_sprintf( buf, sizeof( buf ), "%dfps", fps_int );

	// draw below SuperHUD fps — using proper font system
	charSize = 8.0f;
	x = 640.0f - 2.0f;  // right-aligned
	y = 16.0f;

	// green tint to distinguish from SuperHUD's white fps
	color[0] = 0.2f; color[1] = 1.0f; color[2] = 0.4f; color[3] = 0.8f;

	CG_FontSelect( CG_FontIndexFromName( "sansman" ) );
	CG_OSPDrawString( x, y, buf, color, charSize, charSize * 1.5f, 0,
		DS_HRIGHT | DS_PROPORTIONAL, NULL );
}

// ── per-frame HUD rendering ─────────────────────────────────────────

void WiredHud_Routine( int realtime ) {
	if ( !wiredHud->valid ) return;

	// prototype: draw fps counter via client rendering
	WiredHud_DrawFps( realtime );

	// TODO Step 4: iterate hudOverlay menus, call migrated element routines
}

#endif // FEAT_WIRED_UI
