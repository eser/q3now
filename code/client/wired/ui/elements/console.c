// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * elements/console.c — Console draw layer (frontend element handler) +
 * the console's presentation projection.
 *
 * V-20 (2026-05-25): relocated from code/client/wired/ui/panels/console.c.
 * Dispatched by the compositor's WUI_LAYER_CONSOLE walk via the
 * `type console_view` itemDef in modfiles/ui/console_panel.wui. Data model
 * (ring buffer, search/mark state, Con_PrintSeverity) lives in
 * code/qcommon/wired/core/console/con_buffer.c (WCE-tier, presentation-
 * agnostic, joins the headless build).
 *
 * EXTRACTION (landed): all the presentation-tier state that used
 * to live in con_buffer.c now lives HERE — the render metrics, the per-element
 * color vecs + hex parser, Con_CheckResize (which computes geometry from
 * glconfig + con_scale and pushes it to the core via Con_Reflow), the
 * toggleconsole / messagemode* commands, and the per-line severity → color
 * palette. The UI reads buffer state ONLY through the con_public.h accessors;
 * the relocated render-only fields (xadjust / vislines / color) moved into the
 * UI-local s_conUI struct.
 */

#include "../../../client.h"
#include "../../../../qcommon/wired/wired_build_stamp.h"  // WIRED_ENGINE_TITLE banner
#include "../panels/console_private.h"
#include "../../../../qcommon/wired/core/console/con_public.h"
#include <time.h>

#include "../cl_wired_text.h"
#ifndef HEADLESS
#include "../cl_wired_ui.h"
#endif

extern qboolean chat_team;
extern int      chat_playerNum;

/* ── UI-local render state (decision 1: relocated out of console_t) ───── */
static struct {
	float	xadjust;
	int		vislines;
	vec4_t	color;
} s_conUI = { 0.0f, 0, { 1.0f, 1.0f, 1.0f, 1.0f } };

/* char-grid sizes (relocated from con_buffer.c; externed in client.h, written
 * by Con_CheckResize, read by cl_keys.c / cl_main.c / this file). */
int bigchar_width;
int bigchar_height;
int smallchar_width;
int smallchar_height;

/* ── Text_Draw console metrics (relocated from con_buffer.c) ─────────── */
float con_textPointSize   = 0;
float con_lineAdvance     = SMALLCHAR_HEIGHT;
float con_textCharWidth   = 0;
float con_textNativeCharW = 0;

/* ── Per-element color backing vecs (relocated from con_buffer.c) ────── */
vec4_t con_bgColor     = { 0.063f, 0.074f, 0.074f, 0.965f };
vec4_t con_borderColor = { 0.278f, 0.470f, 0.698f, 1.0f };
vec4_t con_textColor   = { 0.886f, 0.886f, 0.886f, 1.0f };
vec4_t con_cvarColor   = { 0.278f, 0.470f, 0.698f, 1.0f };
vec4_t con_cmdColor    = { 0.309f, 0.654f, 0.741f, 1.0f };
vec4_t con_valueColor  = { 0.898f, 0.737f, 0.223f, 1.0f };

static char con_bgString[16]     = "";
static char con_borderString[16] = "";
static char con_textString[16]   = "";
static char con_cvarString[16]   = "";
static char con_cmdString[16]    = "";
static char con_valueString[16]  = "";

/* console input-field width in chars (relocated from con_buffer.c; externed
 * in client.h, also read by cl_main.c / cl_keys.c). */
int g_console_field_width;

/* log channel (relocated from con_buffer.c) */
LOG_DECLARE_CHANNEL( ch_ui_con, "ui" );

/*
================
Con_UpdateTextMetrics

Recompute point size and character width from the current smallchar_height.
Uses the unified Text_Measure API. All values are in real screen pixels.
================
*/
static void Con_UpdateTextMetrics( void ) {
	if ( cls.glconfig.vidHeight <= 0 ) return;

	/* smallchar_height is already in real screen pixels */
	con_textPointSize = (float)smallchar_height;
	con_lineAdvance   = con_textPointSize * ( con_lineheight ? con_lineheight->value : 1.0f );

	/* measure a monospace character for grid width (real pixels) */
	con_textCharWidth = Text_Measure( "M", FONT_MONO, con_textPointSize );
	if ( con_textCharWidth < 1.0f ) {
		con_textCharWidth = (float)smallchar_width;
	}

	/* native-pixel equivalent — same as con_textCharWidth now */
	con_textNativeCharW = con_textCharWidth;
	if ( con_textNativeCharW < 1.0f ) {
		con_textNativeCharW = (float)smallchar_width;
	}
}

/* ── hex color parser + per-element color refresh (relocated) ────────── */

static int Con_HexCharToInt( char c ) {
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return 10 + (c - 'a');
	if ( c >= 'A' && c <= 'F' ) return 10 + (c - 'A');
	return -1;
}

static qboolean Con_ParseHexColor( const char *str, vec4_t out ) {
	if ( str == NULL || out == NULL )
		return qfalse;

	char buf[16];
	int nyb[8];

	/* trim leading whitespace and '#' */
	while ( *str == ' ' || *str == '\t' )
		str++;
	if ( *str == '#' )
		str++;

	int len = 0;
	while ( str[len] && len < (int)sizeof(buf) - 1 ) {
		if ( str[len] == ' ' || str[len] == '\t' )
			break;
		buf[len] = str[len];
		len++;
	}
	buf[len] = '\0';

	if ( len != 6 && len != 8 )
		return qfalse;

	for ( int i = 0; i < len; i++ ) {
		nyb[i] = Con_HexCharToInt( buf[i] );
		if ( nyb[i] < 0 )
			return qfalse;
	}

	out[0] = (float)( (nyb[0] << 4) | nyb[1] ) / 255.0f;
	out[1] = (float)( (nyb[2] << 4) | nyb[3] ) / 255.0f;
	out[2] = (float)( (nyb[4] << 4) | nyb[5] ) / 255.0f;
	if ( len == 8 ) {
		out[3] = (float)( (nyb[6] << 4) | nyb[7] ) / 255.0f;
	} else {
		out[3] = 1.0f;
	}
	return qtrue;
}

static void Con_UpdateColor( cvar_t *cv, char *lastString, int lastSize,
                             vec4_t out, const vec4_t fallback )
{
	if ( cv == NULL )
		return;

	if ( strcmp( cv->string, lastString ) == 0 )
		return;

	Q_strncpyz( lastString, cv->string, lastSize );

	if ( cv->string[0] == '\0' ) {
		Vector4Copy( fallback, out );
		return;
	}

	if ( !Con_ParseHexColor( cv->string, out ) ) {
		COM_WARN( LOG_CH(ch_ui_con), "WARNING: invalid hex color in %s: '%s'\n",
			cv->name, cv->string );
		Vector4Copy( fallback, out );
	}
}

void Con_UpdateColors( void ) {
	static const vec4_t defBG     = { 0.063f, 0.074f, 0.074f, 0.965f };
	static const vec4_t defBorder = { 0.278f, 0.470f, 0.698f, 1.0f };
	static const vec4_t defText   = { 0.886f, 0.886f, 0.886f, 1.0f };
	static const vec4_t defCVar   = { 0.278f, 0.470f, 0.698f, 1.0f };
	static const vec4_t defCmd    = { 0.309f, 0.654f, 0.741f, 1.0f };
	static const vec4_t defValue  = { 0.898f, 0.737f, 0.223f, 1.0f };

	Con_UpdateColor( con_colBG,     con_bgString,     sizeof(con_bgString),
	                 con_bgColor,     defBG );
	Con_UpdateColor( con_colBorder, con_borderString, sizeof(con_borderString),
	                 con_borderColor, defBorder );
	Con_UpdateColor( con_colText,   con_textString,   sizeof(con_textString),
	                 con_textColor,   defText );
	Con_UpdateColor( con_colCVar,   con_cvarString,   sizeof(con_cvarString),
	                 con_cvarColor,   defCVar );
	Con_UpdateColor( con_colCmd,    con_cmdString,    sizeof(con_cmdString),
	                 con_cmdColor,    defCmd );
	Con_UpdateColor( con_colValue,  con_valueString,  sizeof(con_valueString),
	                 con_valueColor,  defValue );
}

/* ── severity → color palette (severity-as-data) ─────────── */

static void Con_SeverityColor( log_severity_t sev, vec4_t out ) {
	switch ( sev ) {
	case SEV_WARN:
		Vector4Set( out, 0.90f, 0.75f, 0.20f, 1.0f );   /* yellow */
		break;
	case SEV_ERROR:
	case SEV_FATAL:
		Vector4Set( out, 0.90f, 0.25f, 0.20f, 1.0f );   /* red */
		break;
	case SEV_DEBUG:
	case SEV_TRACE:
		Vector4Set( out, 0.55f, 0.55f, 0.55f, 1.0f );   /* dim */
		break;
	default:                                            /* SEV_INFO etc. */
		Vector4Copy( con_textColor, out );              /* console default */
		break;
	}
}

/* ── relocated console commands (toggleconsole / messagemode*) ───────── */

void Con_ToggleConsole_f( void ) {
	if ( con_autoclear->integer ) {
		Field_Clear( &g_consoleField );
	}

	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_CONSOLE );
}

static void Con_MessageMode_f( void ) {
	chat_playerNum = -1;
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

static void Con_MessageMode2_f( void ) {
	chat_playerNum = -1;
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

static void Con_MessageMode3_f( void ) {
	chat_playerNum = clientActiveApp->cgvm ? VM_Call( clientActiveApp->cgvm, 0, CG_CROSSHAIR_PLAYER ) : -1;
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

static void Con_MessageMode4_f( void ) {
	chat_playerNum = clientActiveApp->cgvm ? VM_Call( clientActiveApp->cgvm, 0, CG_LAST_ATTACKER ) : -1;
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

/*
================
Con_CheckResize  (UI projection)

Computes the console geometry from glconfig + con_scale, refreshes the render
metrics, then pushes the pure layout (line width + visible page) to the core
ring buffer via Con_Reflow. The core does the buffer re-lay; this side owns
all the presentation math (glconfig, char sizes, field width).
================
*/
static int  con_viswidth = 0;   /* last glconfig width we laid out for */

void Con_CheckResize( void )
{
	static int old_width, old_vispage;
	static int s_scale_mod = -1, s_lineheight_mod = -1;
	static float s_con_factor = -1.0f;

	if ( con_viswidth == cls.glconfig.vidWidth
	     && con_scale->modificationCount == s_scale_mod
	     && con_lineheight->modificationCount == s_lineheight_mod
	     && cls.con_factor == s_con_factor ) {
		return;
	}
	s_scale_mod = con_scale->modificationCount;
	s_lineheight_mod = con_lineheight->modificationCount;
	s_con_factor = cls.con_factor;

	float scale = con_scale->value;

	con_viswidth = cls.glconfig.vidWidth;

	smallchar_width = SMALLCHAR_WIDTH * scale * cls.con_factor;
	smallchar_height = SMALLCHAR_HEIGHT * scale * cls.con_factor;
	bigchar_width = BIGCHAR_WIDTH * scale * cls.con_factor;
	bigchar_height = BIGCHAR_HEIGHT * scale * cls.con_factor;

	Con_UpdateTextMetrics();

	if ( cls.glconfig.vidWidth == 0 ) // video hasn't been initialized yet
	{
		int width = DEFAULT_CONSOLE_WIDTH * scale;
		if ( width < 40 ) width = 40;
		g_console_field_width = DEFAULT_CONSOLE_WIDTH;
		Con_Reflow( width, 4 );
	}
	else
	{
		int width;
		if ( con_textCharWidth > 0 ) {
			width = (int)( (float)cls.glconfig.vidWidth / con_textNativeCharW ) - 2;
		} else {
			/* Text system not ready yet (early init) — use a reasonable default */
			width = DEFAULT_CONSOLE_WIDTH;
		}

		if ( width < 40 ) width = 40;

		g_console_field_width = width;
		g_consoleField.widthInChars = g_console_field_width;

		int vispage = cls.glconfig.vidHeight / ( (int)con_lineAdvance * 2 ) - 1;

		if ( old_vispage == vispage && old_width == width )
			return;

		old_vispage = vispage;
		old_width = width;

		Con_Reflow( width, vispage );
	}
}

/* ── UI close hook (presentation half of Con_Close) ──────────────────── */

static void Con_CloseProjection( void ) {
	Field_Clear( &g_consoleField );
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_CONSOLE );
}

/* ── UI lifecycle (presentation-only registrations) ──────────────────── */

void Con_InitProjection( void ) {
	Con_UpdateColors();

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );
	Cmd_AddCommand( "messagemode",  Con_MessageMode_f );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
	Cmd_AddCommand( "messagemode3", Con_MessageMode3_f );
	Cmd_AddCommand( "messagemode4", Con_MessageMode4_f );

	Con_SetCloseHook( Con_CloseProjection );
}

void Con_ShutdownProjection( void ) {
	Cmd_RemoveCommand( "toggleconsole" );
	Cmd_RemoveCommand( "messagemode" );
	Cmd_RemoveCommand( "messagemode2" );
	Cmd_RemoveCommand( "messagemode3" );
	Cmd_RemoveCommand( "messagemode4" );
	Con_SetCloseHook( NULL );
}

/* ── Mark-mode keyboard handler (UI tier) ────────────────────────────────
 *
 * Relocated from con_buffer.c (WCE core) because it depends on client-tier
 * keycodes (keycodes.h, reachable here via client.h → keys.h). It is a thin
 * translation layer: keystrokes → core mark API (Con_MarkOpen/Close,
 * Con_MarkMove, Con_MarkCopyAndClose). Core owns all the mark state and the
 * cursor arithmetic; the UI never touches mark fields directly. Behavior is
 * identical to the old core handler. Returns qtrue if the key was consumed. */
qboolean Con_MarkKey( int key, qboolean ctrlDown, qboolean shiftDown ) {
	/* Ctrl+M toggles — handled even if not currently active */
	if ( ctrlDown && (key == 'm' || key == 'M') ) {
		if ( Con_IsMarkActive() ) {
			Con_MarkClose();
		} else {
			Con_MarkOpen();
		}
		return qtrue;
	}

	if ( !Con_IsMarkActive() ) {
		return qfalse;
	}

	/* ESC exits without copying */
	if ( key == K_ESCAPE ) {
		Con_MarkClose();
		return qtrue;
	}

	/* Ctrl+C or Enter copies and exits */
	if ( (ctrlDown && (key == 'c' || key == 'C')) ||
	     key == K_ENTER || key == K_KP_ENTER ) {
		Con_MarkCopyAndClose();
		return qtrue;
	}

	switch ( key ) {
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		Con_MarkMove( CON_MARK_LEFT,  shiftDown, qfalse );
		return qtrue;

	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		Con_MarkMove( CON_MARK_RIGHT, shiftDown, qfalse );
		return qtrue;

	case K_UPARROW:
	case K_KP_UPARROW:
		Con_MarkMove( CON_MARK_UP,    shiftDown, qfalse );
		return qtrue;

	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		Con_MarkMove( CON_MARK_DOWN,  shiftDown, qfalse );
		return qtrue;

	case K_HOME:
	case K_KP_HOME:
		Con_MarkMove( CON_MARK_HOME,  shiftDown, ctrlDown );
		return qtrue;

	case K_END:
	case K_KP_END:
		Con_MarkMove( CON_MARK_END,   shiftDown, ctrlDown );
		return qtrue;

	case K_PGUP:
	case K_KP_PGUP:
		Con_MarkMove( CON_MARK_PGUP,  shiftDown, qfalse );
		return qtrue;

	case K_PGDN:
	case K_KP_PGDN:
		Con_MarkMove( CON_MARK_PGDN,  shiftDown, qfalse );
		return qtrue;
	}

	/* Block everything else while in mark mode */
	return qtrue;
}

/* ── Coordinate helpers ─────────────────────────────────────────────── */

static float Con_NativeToVirtualX( float nativeX ) {
	return nativeX;
}

static float Con_NativeToVirtualY( float nativeY ) {
	return nativeY;
}

/* ── Input prompt ───────────────────────────────────────────────────── */

static void Con_DrawInput( void ) {
	if ( clientActiveApp->state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	int lineWidth = Con_GetLineWidth();
	conSearchView_t sv;
	Con_GetSearchState( &sv );

	int y = s_conUI.vislines - ( (int)con_lineAdvance * 3 );
	float cw = con_textNativeCharW;
	float vcw = con_textCharWidth;
	float vy = Con_NativeToVirtualY( (float)y );

	if ( sv.active ) {
		static vec4_t searchColor  = { 1.0f, 1.0f, 0.0f, 1.0f };
		static vec4_t noMatchColor = { 1.0f, 0.3f, 0.3f, 1.0f };
		char   info[512];
		vec4_t *color;

		if ( sv.pattern[0] && sv.matchCount == 0 )
			color = &noMatchColor;
		else
			color = &searchColor;

		{
			float vxa = Con_NativeToVirtualX( s_conUI.xadjust );

			Text_DrawChar( 'F', vxa + 1 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( 'i', vxa + 2 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( 'n', vxa + 3 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( 'd', vxa + 4 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( ':', vxa + 5 * vcw, vy, FONT_MONO, con_textPointSize, *color );

			int x = 7;
			for ( int i = 0; sv.pattern[i] && x < lineWidth - 16; i++, x++ ) {
				Text_DrawChar( sv.pattern[i], vxa + x * vcw, vy, FONT_MONO, con_textPointSize, s_conUI.color );
			}

			if ( (int)( cls.realtime >> 8 ) & 1 ) {
				Text_DrawChar( '_', vxa + x * vcw, vy, FONT_MONO, con_textPointSize, s_conUI.color );
			}

			if ( sv.pattern[0] ) {
				Com_sprintf( info, sizeof( info ), "(%d matches)", sv.matchCount );
				int len = strlen( info );
				for ( int i = 0; i < len; i++ ) {
					Text_DrawChar( info[i], vxa + ( lineWidth - len + i ) * vcw, vy, FONT_MONO, con_textPointSize, *color );
				}
			}

			re.SetColor( NULL );
			return;
		}
	}

	{
		float vxa = Con_NativeToVirtualX( s_conUI.xadjust );
		Text_DrawChar( ']', vxa + 1 * vcw, vy, FONT_MONO, con_textPointSize, s_conUI.color );

		Field_Draw( &g_consoleField, s_conUI.xadjust + 2 * cw, y,
			cls.glconfig.vidWidth - 3 * smallchar_width, qtrue, qtrue );

		{
			const char *buf = g_consoleField.buffer;
			int start = 0;
			int end = 0;
			char token[128];
			qboolean knownCvar = qfalse;
			qboolean knownCmd  = qfalse;
			const float *highlightColor = NULL;
			char helpText[MAX_STRING_CHARS];

			if ( buf[start] == '/' || buf[start] == '\\' )
				start++;

			while ( buf[start] == ' ' || buf[start] == '\t' )
				start++;

			end = start;
			while ( buf[end] != '\0' && buf[end] != ' ' && buf[end] != '\t' )
				end++;

			int tokLen = end - start;

			if ( tokLen > 0 && tokLen < (int)sizeof( token ) ) {
				for ( int k = 0; k < tokLen; k++ )
					token[k] = buf[start + k];
				token[tokLen] = '\0';

				knownCvar = Help_IsKnownCvar( token );
				if ( !knownCvar )
					knownCmd = Help_IsKnownCommand( token );

				if ( knownCvar )
					highlightColor = con_cvarColor;
				else if ( knownCmd )
					highlightColor = con_cmdColor;

				int tokenStartCharOffset = start - g_consoleField.scroll;
				if ( tokenStartCharOffset >= 0 && highlightColor != NULL ) {
					float tx = s_conUI.xadjust + (2 + tokenStartCharOffset) * cw;
					float tw = tokLen * cw;
					float ty = y + (int)con_textPointSize - 2;
					re.SetColor( highlightColor );
					re.DrawStretchPic( tx, ty, tw, 2, 0, 0, 1, 1, cls.whiteShader );
					re.SetColor( NULL );
				}
			}

			if ( tokLen > 0 && Help_LookupText( token, helpText, sizeof( helpText ) ) ) {
				float hy  = (float)( y + (int)con_lineAdvance + 2 );
				float hvy = Con_NativeToVirtualY( hy );
				int hLen  = (int)strlen( helpText );
				if ( hLen > lineWidth - 2 )
					hLen = lineWidth - 2;
				for ( int hi = 0; hi < hLen; hi++ ) {
					Text_DrawChar( helpText[hi],
						vxa + (1 + hi) * vcw, hvy,
						FONT_MONO, con_textPointSize, con_textColor );
				}
			}
		}
	}
}

/* ── Notify overlay ─────────────────────────────────────────────────── */

static void Con_DrawNotify( void )
{
	if ( clientActiveApp->state == CA_LOADING || cl_loadProgress.startTime > 0 ) {
		return;
	}

	float vcw = con_textCharWidth;
	int currentColorIndex = ColorIndex( COLOR_WHITE );
	re.SetColor( g_color_table[ currentColorIndex ] );

	const short *buffer    = Con_GetBuffer();
	int          lineWidth = Con_GetLineWidth();
	int          totalLines = Con_GetTotalLines();
	int          current   = Con_GetCurrentLine();
	if ( !buffer || lineWidth <= 0 || totalLines <= 0 )
		return;

	/* notify timestamps are stored on the Sys_Milliseconds() clock by the
	 * core (Con_Linefeed); compare on the same monotonic basis (NOT
	 * cls.realtime, which has a different zero point). */
	int nowMs = Sys_Milliseconds();

	int v = cl_conYOffset->integer;
	for ( int i = current - con_notifylines->integer; i <= current; i++ )
	{
		int linelength = 0;

		if ( i < 0 )
			continue;
		int time = Con_GetNotifyTime( i );
		if ( time == 0 )
			continue;
		time = nowMs - time;
		if ( time >= con_notifytime->value * 1000 )
			continue;
		const short *text = buffer + (i % totalLines) * lineWidth;

		if ( clientActiveApp->cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
			continue;
		}

		{
			float notifyAlpha = 1.0f;

			if ( con_fade->integer ) {
				float total     = con_notifytime->value * 1000.0f;
				float fadeStart = total * 0.75f;
				if ( (float)time >= fadeStart ) {
					notifyAlpha = 1.0f - ( (float)time - fadeStart ) / ( total - fadeStart );
					if ( notifyAlpha < 0.0f ) notifyAlpha = 0.0f;
				}
			}

			float vxa = Con_NativeToVirtualX( cl_conXOffset->integer + s_conUI.xadjust );
			float vy  = Con_NativeToVirtualY( (float)v );
			for ( int x = 0; x < lineWidth; x++ ) {
				vec4_t drawColor;
				if ( ( text[x] & 0xff ) == ' ' )
					continue;
				int colorIndex = ( text[x] >> 8 ) & 63;
				currentColorIndex = colorIndex;
				Vector4Copy( g_color_table[ colorIndex ], drawColor );
				drawColor[3] *= notifyAlpha;
				Text_DrawChar( text[x] & 0xff, vxa + (x+1)*vcw, vy,
				               FONT_MONO, con_textPointSize, drawColor );
				linelength++;
			}
		}

		if ( linelength > 0 ) {
			v += (int)con_lineAdvance;
		}
	}

	re.SetColor( NULL );

	if ( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
		return;
	}

	if ( Key_GetCatcher() & KEYCATCH_MESSAGE )
	{
		int skip;
		if ( chat_team )
		{
			vec4_t chatColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			Text_Draw( "say_team:", (float)smallchar_width, (float)v, FONT_DISPLAY,
			           (float)bigchar_height, chatColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
			skip = 10;
		}
		else
		{
			vec4_t chatColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			Text_Draw( "say:", (float)smallchar_width, (float)v, FONT_DISPLAY,
			           (float)bigchar_height, chatColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
			skip = 5;
		}

		{
			int fieldX = skip * bigchar_width;
			int fieldY = v;
			int fieldW = cls.glconfig.vidWidth - ( skip + 1 ) * bigchar_width;
			Field_BigDraw( &chatField, fieldX, fieldY, fieldW, qtrue, qtrue );
		}
	}
}

/* ── Status bar (FPS + clock) ───────────────────────────────────────── */

static int Con_ComputeFPS( void ) {
	enum { CON_FPS_FRAMES = 4 };
	static int      samples[4];
	static int      head;
	static int      previous;
	static qboolean seeded;

	int now = Sys_Milliseconds();
	if ( !seeded ) {
		previous = now;
		seeded   = qtrue;
		return 0;
	}
	int dt = now - previous;
	previous = now;
	samples[ head++ % CON_FPS_FRAMES ] = dt;
	int total = 0;
	for ( int i = 0; i < CON_FPS_FRAMES; i++ ) total += samples[i];
	if ( total <= 0 ) total = 1;
	return 1000 * CON_FPS_FRAMES / total;
}

static void Con_DrawStatus( void ) {
	const float     y    = Con_NativeToVirtualY( 2.0f );
	const float     gap  = con_textNativeCharW * 2.0f;
	static const vec4_t statusColor = { 0.6f, 0.6f, 0.6f, 0.9f };

	if ( !con_fps->integer && !con_clock->integer )
		return;

	char  buf[32];
	float x = (float)cls.glconfig.vidWidth - con_textNativeCharW;

	if ( con_fps->integer ) {
		int fps = Con_ComputeFPS();
		Com_sprintf( buf, sizeof(buf), "%d fps", fps );
		x -= Text_Measure( buf, FONT_MONO, con_textPointSize );
		Text_Draw( buf, Con_NativeToVirtualX( x ), y, FONT_MONO,
		           con_textPointSize, statusColor, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
		x -= gap;
	}

	if ( con_clock->integer ) {
		qtime_t qt;
		Com_RealTime( &qt );
		Com_sprintf( buf, sizeof(buf), "%02d:%02d:%02d",
		             qt.tm_hour, qt.tm_min, qt.tm_sec );
		x -= Text_Measure( buf, FONT_MONO, con_textPointSize );
		Text_Draw( buf, Con_NativeToVirtualX( x ), y, FONT_MONO,
		           con_textPointSize, statusColor, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
	}
}

/* ── Full console background + text ────────────────────────────────── */

static void Con_DrawSolidConsole( float frac ) {

	static float conColorValue[4]                         = { 0.0, 0.0, 0.0, 0.0 };
	static char  conColorString[ MAX_CVAR_VALUE_STRING ]  = { '\0' };

	int lines = cls.glconfig.vidHeight * frac;
	if ( lines <= 0 )
		return;

	if ( re.FinishBloom )
		re.FinishBloom();

	if ( lines > cls.glconfig.vidHeight )
		lines = cls.glconfig.vidHeight;

	float wf = (float)cls.glconfig.vidWidth;
	float yf = frac * (float)cls.glconfig.vidHeight;

	s_conUI.xadjust = 0;

	Con_UpdateColors();

	if ( yf < 1.0 ) {
		yf = 0;
	} else {
		if ( cl_consoleType->integer == 1 ) {
			re.SetColor( g_color_table[ ColorIndex( COLOR_WHITE ) ] );
			re.DrawStretchPic( 0, 0, wf, yf, 0, 0, 1, 1, cls.consoleShader );
		} else if ( con_colBG && con_colBG->string[0] ) {
			re.SetColor( con_bgColor );
			re.DrawStretchPic( 0, 0, wf, yf, 0, 0, 1, 1, cls.whiteShader );
		} else if ( cl_conColor->string[0] ) {
			if ( strcmp( cl_conColor->string, conColorString ) != 0 )
			{
				char buf[ MAX_CVAR_VALUE_STRING ];
				char *v[4];
				Q_strncpyz( conColorString, cl_conColor->string, sizeof( conColorString ) );
				Q_strncpyz( buf, cl_conColor->string, sizeof( buf ) );
				Com_Split( buf, v, 4, ' ' );
				for ( int i = 0; i < 4; i++ ) {
					conColorValue[ i ] = Q_atof( v[ i ] ) / 255.0f;
					if ( conColorValue[ i ] > 1.0f )      conColorValue[ i ] = 1.0f;
					else if ( conColorValue[ i ] < 0.0f ) conColorValue[ i ] = 0.0f;
				}
			}
			re.SetColor( conColorValue );
			re.DrawStretchPic( 0, 0, wf, yf, 0, 0, 1, 1, cls.whiteShader );
		} else {
			re.SetColor( g_color_table[ ColorIndex( COLOR_WHITE ) ] );
			re.DrawStretchPic( 0, 0, wf, yf, 0, 0, 1, 1, cls.consoleShader );
		}
	}

	if ( con_colBorder && con_colBorder->string[0] ) {
		re.SetColor( con_borderColor );
	} else {
		re.SetColor( g_color_table[ ColorIndex( COLOR_RED ) ] );
	}
	re.DrawStretchPic( 0, yf, wf, 2, 0, 0, 1, 1, cls.whiteShader );

	{
		// NOLINTBEGIN(bugprone-integer-division) — ARRAY_LEN expands to a sizeof/sizeof integer expression; result is a chars-count multiplied by char-width
		float verVX = Con_NativeToVirtualX( (float)cls.glconfig.vidWidth
		              - ( ARRAY_LEN( WIRED_ENGINE_TITLE ) ) * con_textNativeCharW );
		// NOLINTEND(bugprone-integer-division)
		float verVY = Con_NativeToVirtualY( (float)(lines - (int)con_lineAdvance) );
		Text_Draw( WIRED_ENGINE_TITLE, verVX, verVY, FONT_MONO,
		           con_textPointSize, colorWhite, TEXT_ALIGN_LEFT, 0 );
	}

	s_conUI.vislines = lines;
	int rows = lines / smallchar_width - 1;

	const short *buffer     = Con_GetBuffer();
	int          lineWidth  = Con_GetLineWidth();
	int          totalLines = Con_GetTotalLines();
	int          current    = Con_GetCurrentLine();
	int          display    = Con_GetDisplayLine();
	conSearchView_t sv;
	Con_GetSearchState( &sv );
	qboolean     markActive = Con_IsMarkActive();

	if ( !buffer || lineWidth <= 0 || totalLines <= 0 )
		return;

	int y   = lines - ((int)con_lineAdvance * 4);
	int row = display;

	{
		float vcw = con_textCharWidth;
		float vxa = Con_NativeToVirtualX( s_conUI.xadjust );

		if ( display != current )
		{
			float vy = Con_NativeToVirtualY( (float)y );
			for ( int x = 0; x < lineWidth; x += 4 )
				Text_DrawChar( '^', vxa + (x+1)*vcw, vy,
				               FONT_MONO, con_textPointSize, g_color_table[ ColorIndex( COLOR_RED ) ] );
			y -= (int)con_lineAdvance;
			row--;
		}

#ifdef USE_CURL
		if ( download.progress[ 0 ] )
		{
			float dlVY = Con_NativeToVirtualY( (float)(lines - (int)con_lineAdvance) );
			int dlLen = strlen( download.progress );
			for ( int x = 0; x < dlLen; x++ )
			{
				Text_DrawChar( download.progress[x], ( x + 1 ) * vcw, dlVY,
				               FONT_MONO, con_textPointSize, g_color_table[ ColorIndex( COLOR_CYAN ) ] );
			}
		}
#endif

		for ( int i = 0; i < rows; i++, y -= (int)con_lineAdvance, row-- )
		{
			if ( row < 0 )
				break;

			if ( current - row >= totalLines )
				continue;

			if ( sv.active && sv.line >= 0 && row == sv.line ) {
				static vec4_t searchBg = { 0.9f, 0.75f, 0.0f, 0.35f };
				re.SetColor( searchBg );
				re.DrawStretchPic( s_conUI.xadjust, (float)y, wf, (float)(int)con_lineAdvance,
				                   0, 0, 1, 1, cls.whiteShader );

				if ( sv.pattern[0] ) {
					char lineBuf[CON_LINEBUF_SIZE];
					const short *lineText = buffer + (row % totalLines) * lineWidth;
					int llen = lineWidth < CON_LINEBUF_SIZE - 1 ? lineWidth : CON_LINEBUF_SIZE - 1;
					for ( int xi = 0; xi < llen; xi++ ) lineBuf[xi] = (char)(lineText[xi] & 0xff);
					lineBuf[llen] = '\0';

					const char *match = Q_stristr( lineBuf, sv.pattern );
					if ( match ) {
						int spanStart = (int)(match - lineBuf);
						int spanLen   = (int)strlen( sv.pattern );
						static vec4_t matchBg = { 1.0f, 0.85f, 0.0f, 0.65f };
						re.SetColor( matchBg );
						re.DrawStretchPic( s_conUI.xadjust + (float)(spanStart + 1) * vcw, (float)y,
						                   (float)spanLen * vcw, (float)(int)con_lineAdvance,
						                   0, 0, 1, 1, cls.whiteShader );
					}
				}

				re.SetColor( NULL );
			}

			if ( markActive ) {
				static vec4_t markBg = { 0.25f, 0.40f, 0.75f, 0.55f };
				int runStart = -1;
				for ( int cx = 0; cx <= lineWidth; cx++ ) {
					qboolean inside = ( cx < lineWidth )
						? Con_MarkCellIsSelected( row, cx )
						: qfalse;
					if ( inside && runStart < 0 ) {
						runStart = cx;
					} else if ( !inside && runStart >= 0 ) {
						float px = s_conUI.xadjust + (runStart + 1) * vcw;
						float pw = (cx - runStart) * vcw;
						re.SetColor( markBg );
						re.DrawStretchPic( px, y, pw, (int)con_lineAdvance,
						                   0, 0, 1, 1, cls.whiteShader );
						runStart = -1;
					}
				}
			}

			const short *text = buffer + (row % totalLines) * lineWidth;
			float vy = Con_NativeToVirtualY( (float)y );

			/* severity-as-data: the line's default color comes from its log
			 * severity (SEV_INFO → console default). The per-char ^N high-byte
			 * code still OVERRIDES this default per character, so explicitly
			 * colored text is unchanged; only the un-coded default shifts. */
			vec4_t sevColor;
			Con_SeverityColor( Con_GetLineSeverity( row ), sevColor );
			int defaultColorIndex = ColorIndex( COLOR_WHITE );

			for ( int x = 0; x < lineWidth; x++ ) {
				if ( ( text[x] & 0xff ) == ' ' )
					continue;
				int colorIndex = ( text[ x ] >> 8 ) & 63;
				/* an explicit ^N code overrides; otherwise use the severity
				 * default color for this line. */
				const float *charColor = ( colorIndex != defaultColorIndex )
					? g_color_table[ colorIndex ]
					: sevColor;
				Text_DrawChar( text[x] & 0xff, vxa + (x + 1) * vcw, vy,
				               FONT_MONO, con_textPointSize, charColor );
			}
		}
	}

#ifndef HEADLESS
	if ( frac >= 1.0f && clientActiveApp->state < CA_ACTIVE ) {
		if ( !WiredUI_IsHealthy() ) {
			static const vec4_t colorOffline = { 1.0f, 0.85f, 0.0f, 1.0f };
			float bx = Con_NativeToVirtualX( s_conUI.xadjust + con_textNativeCharW );
			float by = Con_NativeToVirtualY( (float)(lines - (int)con_lineAdvance) );
			Text_Draw( "[WiredUI offline]  Press Escape or type 'wired_recover' to reload menus.",
			           bx, by, FONT_MONO, con_textPointSize, colorOffline, TEXT_ALIGN_LEFT, 0 );
		}
		{
			int failTime = WiredUI_GetLastRecoveryFailTime();
			if ( failTime != 0 && ( cls.realtime - failTime ) < 3000 ) {
				static const vec4_t colorFail = { 1.0f, 0.25f, 0.2f, 1.0f };
				float bx = Con_NativeToVirtualX( s_conUI.xadjust + con_textNativeCharW );
				float by = Con_NativeToVirtualY( (float)(lines - (int)con_lineAdvance * 2) );
				Text_Draw( "[WiredUI reload failed.  Type 'wired_reload' or 'wired_recover' to retry.]",
				           bx, by, FONT_MONO, con_textPointSize, colorFail, TEXT_ALIGN_LEFT, 0 );
			}
		}
	}
#endif

	Con_DrawInput();
	Con_DrawStatus();

	re.SetColor( NULL );
}

/* ── Public entry point ─────────────────────────────────────────────── */

void Con_DrawConsole( void ) {

	Con_CheckResize();

	// CA_CINEMATIC > CA_ACTIVE in the connstate_t order, so the < CA_ACTIVE check
	// already excludes cinematic; the explicit comparison is redundant.
	//
	// WiredUI-offline recovery backdrop: when WiredUI is unhealthy at a bare
	// disconnected/sub-active screen (no menu, no loading bar), the console is
	// the only surface, forced fully visible so the user can type
	// 'wired_recover'. When WiredUI is healthy the attract layer is the base
	// surface — the console only appears when toggled open (handled by the
	// display-fraction path below, whose target height is the view mode).
	if ( clientActiveApp->state < CA_ACTIVE ) {
		if ( !( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) ) {
			if ( cl_loadProgress.startTime <= 0
			  && !WiredUI_IsHealthy() ) {
				Con_DrawSolidConsole( 1.0 );
				return;
			}
		}
	}

	float displayFrac = Con_GetDisplayFrac();
	if ( displayFrac ) {
		Con_DrawSolidConsole( displayFrac );
	} else {
		if ( clientActiveApp->state == CA_ACTIVE ) {
			Con_DrawNotify();
		}
	}
}
