/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"
#include "wired/ui/cl_wired_ui.h"
#include "wired/hud/cl_wired_hud.h"
#include "wired/ui/cl_wired_msdf.h"
#include "wired/ui/cl_wired_fonts.h"
#include "wired/ui/cl_wired_text.h"

static qboolean	scr_initialized;		// ready to draw
static qboolean	scr_updateActive;		// are we currently inside SCR_UpdateScreen?

cvar_t		*cl_timegraph;
static cvar_t		*cl_debuggraph;
static cvar_t		*cl_graphheight;
static cvar_t		*cl_graphscale;
static cvar_t		*cl_graphshift;

/* net-stats overlay — defined in cl_net_stats.c */
void SCR_NetStatsInit( void );
void SCR_DrawPing( void );
void SCR_DrawSnaps( void );
void SCR_DrawPackets( void );

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	float	xscale;
	float	yscale;

#if 0
		// adjust for wide screens
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			*x += 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * 640 / 480 ) );
		}
#endif

	// scale for screen sizes
	xscale = cls.glconfig.vidWidth / 640.0;
	yscale = cls.glconfig.vidHeight / 480.0;
	if ( x ) {
		*x *= xscale;
	}
	if ( y ) {
		*y *= yscale;
	}
	if ( w ) {
		*w *= xscale;
	}
	if ( h ) {
		*h *= yscale;
	}
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/* SCR_DrawChar — DELETED, was static and only called by old SCR_DrawStringExt bitmap path */


/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
** Thin wrapper — delegates to Text_DrawChar (MSDF).
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	vec4_t white = {1, 1, 1, 1};

	ch &= 255;
	if ( ch == ' ' ) {
		return;
	}
	if ( y < -smallchar_height ) {
		return;
	}

	Text_DrawChar( ch, (float)x, (float)y, FONT_MONO, (float)smallchar_height, white );
}


/*
** SCR_DrawSmallString
** small string are drawn at native screen resolution
** Thin wrapper — delegates to Text_Draw (MSDF).
*/
void SCR_DrawSmallString( int x, int y, const char *s, int len ) {
	vec4_t white = {1, 1, 1, 1};
	char buf[1024];

	if ( y < -smallchar_height ) {
		return;
	}

	/* copy up to len chars so we have a NUL-terminated string for Text_Draw */
	if ( len >= (int)sizeof(buf) ) {
		len = (int)sizeof(buf) - 1;
	}
	memcpy( buf, s, len );
	buf[len] = '\0';

	Text_Draw( buf, (float)x, (float)y, FONT_MONO, (float)smallchar_height, white, TEXT_ALIGN_LEFT, 0 );
}


/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
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


/*
==================
SCR_DrawBigString
==================
*/
void SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape ) {
	vec4_t color;

	(void)noColorEscape;

	color[0] = color[1] = color[2] = 1.0f;
	color[3] = alpha;

	Text_Draw( s, (float)x, (float)y, FONT_DISPLAY, (float)BIGCHAR_WIDTH, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	int flags = 0;

	(void)noColorEscape;

	if ( forceColor ) {
		flags |= TEXT_FORCECOLOR;
	}

	Text_Draw( string, (float)x, (float)y, FONT_MONO, (float)smallchar_height, setColor, TEXT_ALIGN_LEFT, flags );
}


/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
** SCR_GetBigStringWidth
*/ 
int SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * BIGCHAR_WIDTH;
}


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
static void SCR_DrawDemoRecording( void ) {
	char	string[sizeof(clc.recordNameShort)+32];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}

	pos = FS_FTell( clc.recordfile );

	if (cl_drawRecording->integer == 1) {
		sprintf(string, "RECORDING %s: %ik", clc.recordNameShort, pos / 1024);
		Text_Draw(string, (float)(320 - strlen(string) * 4), 20.0f, FONT_DISPLAY, 8.0f, g_color_table[ColorIndex(COLOR_WHITE)], TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW);
	} else if (cl_drawRecording->integer == 2) {
		sprintf(string, "RECORDING: %ik", pos / 1024);
		Text_Draw(string, (float)(320 - strlen(string) * 4), 20.0f, FONT_DISPLAY, 8.0f, g_color_table[ColorIndex(COLOR_WHITE)], TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW);
	}
}


#ifdef USE_VOIP
/*
=================
SCR_DrawVoipMeter
=================
*/
static void SCR_DrawVoipMeter( void ) {
	char	buffer[16];
	char	string[256];
	int i;

	if (!cl_voipShowMeter->integer)
		return;  // player doesn't want to show meter at all.
	else if (!cl_voipSend->integer)
		return;  // not recording at the moment.
	else if (clc.state != CA_ACTIVE)
		return;  // not connected to a server.
	else if (!clc.voipEnabled)
		return;  // server doesn't support VoIP.
	else if (clc.demoplaying)
		return;  // playing back a demo.
	else if (!cl_voip->integer)
		return;  // client has VoIP support disabled.

	int limit = (int) (clc.voipPower * 10.0f);
	if (limit > 10)
		limit = 10;

	for (i = 0; i < limit; i++)
		buffer[i] = '*';
	while (i < 10)
		buffer[i++] = ' ';
	buffer[i] = '\0';

	sprintf( string, "VoIP: [%s]", buffer );
	Text_Draw( string, (float)(320 - strlen( string ) * 4), 10.0f, FONT_DISPLAY, 8.0f, g_color_table[ ColorIndex( COLOR_WHITE ) ], TEXT_ALIGN_LEFT, TEXT_FORCECOLOR | TEXT_DROPSHADOW );
}
#endif


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

static	int			current;
static	float		values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph( float value )
{
	values[current] = value;
	current = (current + 1) % ARRAY_LEN(values);
}


/*
==============
SCR_DrawDebugGraph
==============
*/
static void SCR_DrawDebugGraph( void )
{
	//
	// draw the graph
	//
	int w = cls.glconfig.vidWidth;
	int x = 0;
	int y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
	re.DrawStretchPic(x, y - cl_graphheight->integer,
		w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for (int a=0 ; a<w ; a++)
	{
		int i = (ARRAY_LEN(values)+current-1-(a % ARRAY_LEN(values))) % ARRAY_LEN(values);
		float v = values[i];
		v = v * cl_graphscale->integer + cl_graphshift->integer;

		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		int h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);

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
	qboolean uiFullscreen;

	re.BeginFrame( stereoFrame );

	uiFullscreen = (UI_VM_ACTIVE && UI_CALL_IS_FULLSCREEN());

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( uiFullscreen || cls.state < CA_LOADING ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			// draw vertical bars on sides for legacy mods
			const int w = (cls.glconfig.vidWidth - ((cls.glconfig.vidHeight * 640) / 480)) /2;
			re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
			re.DrawStretchPic( 0, 0, w, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.DrawStretchPic( cls.glconfig.vidWidth - w, 0, w, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.SetColor( NULL );
		}
	}

	// ── Loading screen takes priority over UI ──
	// When a local map load is active, draw the loading screen regardless
	// of UI VM state.  After CL_ShutdownAll() the UI VM may be inactive
	// for several frames — without this guard the old framebuffer content
	// (levelshot) bleeds through.
	if ( cl_loadProgress.startTime > 0 ) {
		if ( cls.rendererStarted && cls.state >= CA_CONNECTING && cls.state <= CA_PRIMED ) {
			if ( !cl_loadFading || cl_loadFadeAlpha <= 0.0f ) {
				cl_loadFadeAlpha = 1.0f;
				cl_loadFading = qfalse;
			}
			if ( cgvm && cls.state == CA_PRIMED ) {
				CL_PROF(cgr, CL_CGameRendering( stereoFrame ));
			}
			CL_DrawLoadingScreen();
		} else if ( cls.state == CA_ACTIVE ) {
			if ( cls.realtime - cl_loadProgress.startTime < 0 ) { // ( com_developer->integer ? 1500 : 350 )
				// Still within minimum display time — draw loading screen at full opacity
				cl_loadFadeAlpha = 1.0f;
				CL_PROF(cgr, CL_CGameRendering( stereoFrame ));
				CL_DrawLoadingScreen();
			} else if ( !cl_loadFading ) {
				// Minimum display time expired — start fade transition
				cl_loadFading = qtrue;
				cl_loadFadeAlpha = 1.0f;
				// Render game frame underneath, loading screen overlay on top
				CL_PROF(cgr, CL_CGameRendering( stereoFrame ));
				CL_DrawLoadingScreen();
			} else {
				// Fade in progress — game renders first, loading screen dissolves on top
				cl_loadFadeAlpha -= cls.frametime * 0.002f;  // 500ms total
				if ( cl_loadFadeAlpha <= 0.0f ) {
					// Fade complete — clean up
					cl_loadFadeAlpha = 0.0f;
					cl_loadFading = qfalse;
					CL_LoadingScreenFinished();
				}
				CL_PROF(cgr, CL_CGameRendering( stereoFrame ));
				if ( cl_loadFading ) {
					CL_DrawLoadingScreen();
				}
#if FEAT_WIRED_UI
				if ( !cl_loadFading && wiredHud->valid ) {
					CL_PROF(wui, WiredUI_Refresh( cls.realtime ));
					CL_PROF(whud, WiredHud_Routine( cls.realtime ));
				}
#endif
				if ( !cl_loadFading ) {
					SCR_DrawDemoRecording();
#ifdef USE_VOIP
					SCR_DrawVoipMeter();
#endif
				}
			}
		} else {
			// CA_DISCONNECTED / CA_CONNECTING / CA_CHALLENGING / CA_CONNECTED:
			// Only paint a dark fill when a UI or cgame catcher is active.
			// When neither catcher is set, Con_DrawConsole (called below) renders
			// the console fullscreen and its own background covers this region —
			// painting here would be an overdraw with no visible effect.
			if ( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
				vec4_t dark = { 0.031f, 0.047f, 0.063f, 1.0f };
				re.SetColor( dark );
				re.DrawStretchPic( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
				re.SetColor( NULL );
			}
		}
	}
	// ── Normal UI / game rendering (when not loading) ──
	else if ( UI_VM_ACTIVE && !uiFullscreen ) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			if ( cl_loadProgress.startTime <= 0 ) {
				S_StopAllSounds();
				UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			}
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			if ( Q_stricmp( cls.servername, "localhost" ) ) {
				// Remote connection: show the UI connection dialog
				UI_CALL_REFRESH( cls.realtime );
				UI_CALL_CONNECT( qfalse );
			}
			break;
		case CA_LOADING:
		case CA_PRIMED:
			if ( !cl_loadFading || cl_loadFadeAlpha <= 0.0f ) {
				cl_loadFadeAlpha = 1.0f;
				cl_loadFading = qfalse;
			}
			// Remote server loading: draw loading screen
			// (localhost loading is handled by the priority block above)
			CL_DrawLoadingScreen();
			if ( cgvm && cls.state == CA_PRIMED ) {
				CL_PROF(cgr, CL_CGameRendering( stereoFrame ));
			}
			break;
		case CA_ACTIVE:
			CL_PROF(cgr, CL_CGameRendering( stereoFrame ));
#if FEAT_WIRED_UI
			if ( wiredHud->valid ) {
				CL_PROF(wui, WiredUI_Refresh( cls.realtime ));
				CL_PROF(whud, WiredHud_Routine( cls.realtime ));
			}
#endif
			SCR_DrawDemoRecording();
#ifdef USE_VOIP
			SCR_DrawVoipMeter();
#endif
			break;
		}
	}

	// the menu draws next
	if ( Key_GetCatcher( ) & KEYCATCH_UI && UI_VM_ACTIVE ) {
		CL_PROF(wui, UI_CALL_REFRESH( cls.realtime ));
	}

	// console draws next
	CL_PROF(cons, Con_DrawConsole ());

	// debug graph can be drawn on top of anything
	if ( cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph ();
	}
	SCR_DrawPing();
	SCR_DrawSnaps();
	SCR_DrawPackets();
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
