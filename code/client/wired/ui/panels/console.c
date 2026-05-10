/*
 * panels/console.c — Console draw layer (frontend)
 *
 * Rendering-only functions extracted from cl_console.c.
 * Data model (ring buffer, CL_ConsolePrint, search/mark state) lives in
 * cl_console.c.  Shared state is declared in cl_console_private.h.
 */

#include "../../../client.h"
#include "console_private.h"
#include <time.h>

#include "../cl_wired_text.h"
#ifndef DEDICATED
#include "../cl_wired_ui.h"
#endif

extern qboolean chat_team;
extern int      chat_playerNum;

/* ── Coordinate helpers ─────────────────────────────────────────────── */

static float Con_NativeToVirtualX( float nativeX ) {
	return nativeX;
}

static float Con_NativeToVirtualY( float nativeY ) {
	return nativeY;
}

/* ── Input prompt ───────────────────────────────────────────────────── */

static void Con_DrawInput( void ) {
	if ( cls.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	int y = con.vislines - ( (int)con_lineAdvance * 3 );
	float cw = con_textNativeCharW;
	float vcw = con_textCharWidth;
	float vy = Con_NativeToVirtualY( (float)y );

	if ( con.searchActive ) {
		static vec4_t searchColor  = { 1.0f, 1.0f, 0.0f, 1.0f };
		static vec4_t noMatchColor = { 1.0f, 0.3f, 0.3f, 1.0f };
		char   info[512];
		vec4_t *color;

		if ( con.searchPattern[0] && con.searchMatchCount == 0 )
			color = &noMatchColor;
		else
			color = &searchColor;

		{
			float vxa = Con_NativeToVirtualX( con.xadjust );

			Text_DrawChar( 'F', vxa + 1 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( 'i', vxa + 2 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( 'n', vxa + 3 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( 'd', vxa + 4 * vcw, vy, FONT_MONO, con_textPointSize, *color );
			Text_DrawChar( ':', vxa + 5 * vcw, vy, FONT_MONO, con_textPointSize, *color );

			int x = 7;
			for ( int i = 0; con.searchPattern[i] && x < con.linewidth - 16; i++, x++ ) {
				Text_DrawChar( con.searchPattern[i], vxa + x * vcw, vy, FONT_MONO, con_textPointSize, con.color );
			}

			if ( (int)( cls.realtime >> 8 ) & 1 ) {
				Text_DrawChar( '_', vxa + x * vcw, vy, FONT_MONO, con_textPointSize, con.color );
			}

			if ( con.searchPattern[0] ) {
				Com_sprintf( info, sizeof( info ), "(%d matches)", con.searchMatchCount );
				int len = strlen( info );
				for ( int i = 0; i < len; i++ ) {
					Text_DrawChar( info[i], vxa + ( con.linewidth - len + i ) * vcw, vy, FONT_MONO, con_textPointSize, *color );
				}
			}

			re.SetColor( NULL );
			return;
		}
	}

	{
		float vxa = Con_NativeToVirtualX( con.xadjust );
		Text_DrawChar( ']', vxa + 1 * vcw, vy, FONT_MONO, con_textPointSize, con.color );

		Field_Draw( &g_consoleField, con.xadjust + 2 * cw, y,
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
					float tx = con.xadjust + (2 + tokenStartCharOffset) * cw;
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
				if ( hLen > con.linewidth - 2 )
					hLen = con.linewidth - 2;
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
	if ( cls.state == CA_LOADING || cl_loadProgress.startTime > 0 ) {
		return;
	}

	float vcw = con_textCharWidth;
	int currentColorIndex = ColorIndex( COLOR_WHITE );
	re.SetColor( g_color_table[ currentColorIndex ] );

	int v = cl_conYOffset->integer;
	for ( int i = con.current - con_notifylines->integer; i <= con.current; i++ )
	{
		int linelength = 0;

		if ( i < 0 )
			continue;
		int time = con.times[i % NUM_CON_TIMES];
		if ( time == 0 )
			continue;
		time = cls.realtime - time;
		if ( time >= con_notifytime->value * 1000 )
			continue;
		short *text = con.text + (i % con.totallines) * con.linewidth;

		if ( cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
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

			float vxa = Con_NativeToVirtualX( cl_conXOffset->integer + con.xadjust );
			float vy  = Con_NativeToVirtualY( (float)v );
			for ( int x = 0; x < con.linewidth; x++ ) {
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

	con.xadjust = 0;

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
		              - ( ARRAY_LEN( WIRED_ENGINE_VERSION ) ) * con_textNativeCharW );
		// NOLINTEND(bugprone-integer-division)
		float verVY = Con_NativeToVirtualY( (float)(lines - (int)con_lineAdvance) );
		Text_Draw( WIRED_ENGINE_VERSION, verVX, verVY, FONT_MONO,
		           con_textPointSize, colorWhite, TEXT_ALIGN_LEFT, 0 );
	}

	con.vislines = lines;
	int rows = lines / smallchar_width - 1;

	int y   = lines - ((int)con_lineAdvance * 4);
	int row = con.display;

	{
		float vcw = con_textCharWidth;
		float vxa = Con_NativeToVirtualX( con.xadjust );

		if ( con.display != con.current )
		{
			float vy = Con_NativeToVirtualY( (float)y );
			for ( int x = 0; x < con.linewidth; x += 4 )
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

			if ( con.current - row >= con.totallines )
				continue;

			if ( con.searchActive && con.searchLine >= 0 && row == con.searchLine ) {
				static vec4_t searchBg = { 0.9f, 0.75f, 0.0f, 0.35f };
				re.SetColor( searchBg );
				re.DrawStretchPic( con.xadjust, (float)y, wf, (float)(int)con_lineAdvance,
				                   0, 0, 1, 1, cls.whiteShader );

				if ( con.searchPattern[0] ) {
					char lineBuf[CON_LINEBUF_SIZE];
					short *lineText = con.text + (row % con.totallines) * con.linewidth;
					int llen = con.linewidth < CON_LINEBUF_SIZE - 1 ? con.linewidth : CON_LINEBUF_SIZE - 1;
					for ( int xi = 0; xi < llen; xi++ ) lineBuf[xi] = (char)(lineText[xi] & 0xff);
					lineBuf[llen] = '\0';

					const char *match = Q_stristr( lineBuf, con.searchPattern );
					if ( match ) {
						int spanStart = (int)(match - lineBuf);
						int spanLen   = (int)strlen( con.searchPattern );
						static vec4_t matchBg = { 1.0f, 0.85f, 0.0f, 0.65f };
						re.SetColor( matchBg );
						re.DrawStretchPic( con.xadjust + (float)(spanStart + 1) * vcw, (float)y,
						                   (float)spanLen * vcw, (float)(int)con_lineAdvance,
						                   0, 0, 1, 1, cls.whiteShader );
					}
				}

				re.SetColor( NULL );
			}

			if ( con.markActive ) {
				static vec4_t markBg = { 0.25f, 0.40f, 0.75f, 0.55f };
				int runStart = -1;
				for ( int cx = 0; cx <= con.linewidth; cx++ ) {
					qboolean inside = ( cx < con.linewidth )
						? Con_MarkCellIsSelected( row, cx )
						: qfalse;
					if ( inside && runStart < 0 ) {
						runStart = cx;
					} else if ( !inside && runStart >= 0 ) {
						float px = con.xadjust + (runStart + 1) * vcw;
						float pw = (cx - runStart) * vcw;
						re.SetColor( markBg );
						re.DrawStretchPic( px, y, pw, (int)con_lineAdvance,
						                   0, 0, 1, 1, cls.whiteShader );
						runStart = -1;
					}
				}
			}

			short *text = con.text + (row % con.totallines) * con.linewidth;
			float vy = Con_NativeToVirtualY( (float)y );

			for ( int x = 0; x < con.linewidth; x++ ) {
				if ( ( text[x] & 0xff ) == ' ' )
					continue;
				int colorIndex = ( text[ x ] >> 8 ) & 63;
				Text_DrawChar( text[x] & 0xff, vxa + (x + 1) * vcw, vy,
				               FONT_MONO, con_textPointSize, g_color_table[ colorIndex ] );
			}
		}
	}

#ifndef DEDICATED
	if ( frac >= 1.0f && cls.state < CA_ACTIVE ) {
		if ( !WiredUI_IsHealthy() ) {
			static const vec4_t colorOffline = { 1.0f, 0.85f, 0.0f, 1.0f };
			float bx = Con_NativeToVirtualX( con.xadjust + con_textNativeCharW );
			float by = Con_NativeToVirtualY( (float)(lines - (int)con_lineAdvance) );
			Text_Draw( "[WiredUI offline]  Press Escape or type 'wired_recover' to reload menus.",
			           bx, by, FONT_MONO, con_textPointSize, colorOffline, TEXT_ALIGN_LEFT, 0 );
		}
		{
			int failTime = WiredUI_GetLastRecoveryFailTime();
			if ( failTime != 0 && ( cls.realtime - failTime ) < 3000 ) {
				static const vec4_t colorFail = { 1.0f, 0.25f, 0.2f, 1.0f };
				float bx = Con_NativeToVirtualX( con.xadjust + con_textNativeCharW );
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
	if ( cls.state < CA_ACTIVE ) {
		if ( !( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) ) {
			if ( cls.state != CA_LOADING && cls.state != CA_PRIMED
			     && cl_loadProgress.startTime <= 0 ) {
				Con_DrawSolidConsole( 1.0 );
				return;
			}
		}
	}

	if ( con.displayFrac ) {
		Con_DrawSolidConsole( con.displayFrac );
	} else {
		if ( cls.state == CA_ACTIVE ) {
			Con_DrawNotify();
		}
	}
}
