// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// con_buffer.c — WiredConsole core: buffer + history + scrollback + mark/search.
//
// Relocated 2026-05-25 from code/client/wired/ui/panels/console_state.c to
// code/qcommon/wired/core/console/ as part of WiredUI Render Frame Unification.
// The file's content is intact; only its location moved.
//
// EXTRACTION (landed): this file is now presentation-agnostic.
// All UI-tier touches (cls.glconfig, Text_Measure, smallchar_*, KEYCATCH_*,
// Key_GetCatcher/SetCatcher, the render metrics, the color vecs, the
// messagemode/toggleconsole commands, Con_CheckResize's glconfig half) were
// extracted to code/client/wired/ui/elements/console.c. con_buffer.c keeps
// only the ring buffer, history, scrollback, search, mark, and the geometry
// reflow (param-injected via Con_Reflow). It includes ZERO client/* headers
// and joins the dedicated-server (qcommon_ded) target.
//
// Severity-as-data: each logical line stores a log_severity_t in
// con.severities[] (parallel to text[]). Con_PrintSeverity tags the print;
// the UI colors each line from Con_GetLineSeverity(). The old text bracket
// "[WARN] " injection by the console log sink is gone.

#include "../../../q_shared.h"
#include "../../../qcommon.h"
#include "con_private.h"
#include "con_public.h"
#include "../../../arena.h"
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

/* The print currently in flight tags each line it emits with this severity.
 * CL_ConsolePrint sets SEV_INFO (the "no special severity" default that the
 * UI renders at the console default color); Con_PrintSeverity sets the real
 * severity for log lines. */
static log_severity_t s_printSeverity = SEV_INFO;

cvar_t		*con_lineheight;

// forward declarations
void Con_SearchClose( void );
void Con_MarkOpen( void );
void Con_MarkClose( void );
qboolean Con_IsMarkActive( void );
qboolean Con_MarkCellIsSelected( int row, int col );

/* Console state lives in a persistent arena so it survives Hunk_ClearLevel().
   All code uses `con.field` — the macro expands to (*s_con).field which is
   an lvalue, so assignments (con.field = ...) compile correctly.           */
static arena_t   *s_conArena = NULL;
console_t        *s_con      = NULL;

cvar_t		*con_conspeed;
cvar_t		*con_autoclear;
cvar_t		*con_notifytime;
cvar_t		*con_notifylines;
cvar_t		*con_scale;
cvar_t		*con_anim;
cvar_t		*con_clock;
cvar_t		*con_fade;
cvar_t		*con_fps;
cvar_t		*cl_consoleHeight;
cvar_t		*cl_consoleType;
cvar_t		*con_timestamp;

/* per-element console colors.  Each cvar stores a hex string in the
 * form "RRGGBB" or "RRGGBBAA"; we parse them lazily every frame
 * (string comparison guards against rebuilding the vec4 when the
   value has not changed). */
cvar_t		*con_colBG;
cvar_t		*con_colBorder;
cvar_t		*con_colText;
cvar_t		*con_colCVar;
cvar_t		*con_colCmd;
cvar_t		*con_colValue;

/* the color backing vecs, their string caches, the hex parser,
 * Con_UpdateColor(s), g_console_field_width, Con_ToggleConsole_f, and the
 * Con_MessageMode*_f handlers all moved to the UI projection
 * (code/client/wired/ui/elements/console.c) — they touched Key_SetCatcher,
 * chat fields, cgvm, or vec4_t render colors. */

/*
================
Con_Clear_f
================
*/
static void Con_Clear_f( void ) {
	for ( int i = 0 ; i < con.linewidth ; i++ ) {
		con.text[i] = ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ';
	}

	for ( int i = 0 ; i < con.totallines && i < CON_MAX_TOTALLINES ; i++ ) {
		con.severities[i] = SEV_INFO;
	}

	con.x = 0;
	con.current = 0;
	con.newline = qtrue;

	Con_Bottom();		// go to end
}


/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f( void )
{
	if ( Cmd_Argc() != 2 )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "usage: dumpConsole <filename>\n" );
		return;
	}

	char	filename[ MAX_OSPATH ];
	Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".txt" );

	const char *ext;
	if ( !FS_AllowedExtension( filename, qfalse, &ext ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "%s: Invalid filename extension '%s'.\n", __func__, ext );
		return;
	}

	fileHandle_t f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "ERROR: couldn't open %s.\n", filename );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui), "Dumped console text to %s.\n", filename );

	int n, l;
	if ( con.current >= con.totallines ) {
		n = con.totallines;
		l = con.current + 1;
	} else {
		n = con.current + 1;
		l = 0;
	}

	int bufferlen = con.linewidth + ARRAY_LEN( Q_NEWLINE ) * sizeof( char );
	char *buffer = Hunk_AllocateTempMemory( bufferlen );

	// write the remaining lines
	buffer[ bufferlen - 1 ] = '\0';

	for ( int i = 0; i < n ; i++, l++ )
	{
		short *line = con.text + (l % con.totallines) * con.linewidth;
		// store line
		for( int x = 0; x < con.linewidth; x++ )
			buffer[ x ] = line[ x ] & 0xff;
		buffer[ con.linewidth ] = '\0';
		// terminate on ending space characters
		for ( int x = con.linewidth - 1 ; x >= 0 ; x-- ) {
			if ( buffer[ x ] == ' ' )
				buffer[ x ] = '\0';
			else
				break;
		}
		{ qstring_t _b_qs = QS_WrapExisting( buffer, bufferlen ); QS_Append( &_b_qs, Q_NEWLINE ); }
		FS_Write( buffer, strlen( buffer ), f );
	}

	Hunk_FreeTempMemory( buffer );
	FS_FCloseFile( f );
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	for ( int i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		con.times[i] = 0;
	}
}


/*
================
Con_Reflow

Pure ring-buffer re-lay. Re-lay the console text (and its parallel
severity rows) into a new line width. Geometry is param-injected: the UI
computes targetLineWidth + visPage from glconfig/con_scale and pushes them
here, so the core reads NO glconfig, NO con_scale, NO render metrics.

  targetLineWidth — already clamped to >= 40 by the caller (DEFAULT min).
  visPage         — visible-rows page size, used by scrollback navigation.

The legacy text reflow copies row-by-row (it does not re-wrap), so each
old logical row maps 1:1 to a new row. con.severities[] follows that exact
src/dst index math; rows with no source are reset to SEV_INFO.
================
*/
void Con_Reflow( int targetLineWidth, int visPage )
{
	short	tbuf[CON_TEXTSIZE];
	static log_severity_t sevbuf[CON_MAX_TOTALLINES];

	if ( targetLineWidth < 40 )
		targetLineWidth = 40;

	int oldwidth = con.linewidth;
	int oldtotallines = con.totallines;
	int oldcurrent = con.current;

	con.linewidth = targetLineWidth;
	con.totallines = CON_TEXTSIZE / con.linewidth;
	con.vispage = visPage;

	/* first-time / degenerate layout: nothing to migrate */
	if ( oldwidth <= 0 || oldtotallines <= 0 ) {
		for ( int i = 0; i < CON_TEXTSIZE; i++ )
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
		for ( int i = 0; i < con.totallines && i < CON_MAX_TOTALLINES; i++ )
			con.severities[i] = SEV_INFO;
		Con_ClearNotify();
		con.current = 0;
		con.display = con.current;
		return;
	}

	int numchars = oldwidth;
	if ( numchars > con.linewidth )
		numchars = con.linewidth;

	int numlines;
	if ( oldcurrent > oldtotallines )
		numlines = oldtotallines;
	else
		numlines = oldcurrent + 1;

	if ( numlines > con.totallines )
		numlines = con.totallines;

	memcpy( tbuf, con.text, CON_TEXTSIZE * sizeof( short ) );
	/* snapshot the old severity rows so the row-copy can read them after
	   the new layout has been cleared */
	for ( int i = 0; i < oldtotallines && i < CON_MAX_TOTALLINES; i++ )
		sevbuf[i] = con.severities[i];

	for ( int i = 0; i < CON_TEXTSIZE; i++ )
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	for ( int i = 0; i < con.totallines && i < CON_MAX_TOTALLINES; i++ )
		con.severities[i] = SEV_INFO;

	for ( int i = 0; i < numlines; i++ )
	{
		int srcRow = (oldcurrent - i + oldtotallines) % oldtotallines;
		int dstRow = numlines - 1 - i;

		short *src = &tbuf[ srcRow * oldwidth ];
		short *dst = &con.text[ dstRow * con.linewidth ];
		for ( int j = 0; j < numchars; j++ )
			*dst++ = *src++;

		/* severity follows the text 1:1: old logical row srcRow → new dstRow.
		   New rows in [0,totallines) are addressed directly by index since
		   con.current is reset to numlines-1 (dstRow == new line index). */
		if ( srcRow < CON_MAX_TOTALLINES && dstRow >= 0 && dstRow < CON_MAX_TOTALLINES )
			con.severities[dstRow] = sevbuf[srcRow];
	}

	Con_ClearNotify();

	con.current = numlines - 1;
	con.display = con.current;
}


/*
==================
Cmd_CompleteTxtName
==================
*/
static void Cmd_CompleteTxtName(const char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


/* Con_HexCharToInt / Con_ParseHexColor / Con_UpdateColor /
 * Con_UpdateColors moved to the UI projection (elements/console.c) — they
 * write vec4_t render colors, pure presentation. The color cvars are still
 * REGISTERED here (the s_conDescs table) and their handles assigned to the
 * con_col* ptr globals; the UI externs those ptrs and parses them. */


static const cvarDesc_t s_conDescs[] = {
	/*  0 */ CVAR_FLOAT(  "con_notifytime",   "5",        CVAR_ARCHIVE,    "Defines how long messages (from players or the system) are on the screen (in seconds).", 0, 0 ),
	/*  1 */ CVAR_INT(    "con_notifylines",  "3",        CVAR_ARCHIVE,    "Defines the number of lines to display in the notify area.", 1, NUM_CON_TIMES - 1 ),
	/*  2 */ CVAR_FLOAT(  "scr_conspeed",     "3",        CVAR_ARCHIVE,    "Console opening/closing scroll speed.", 0, 0 ),
	/*  3 */ CVAR_BOOL(   "con_autoclear",    "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Enable/disable clearing console input text when console is closed." ),
	/*  4 */ CVAR_FLOAT(  "con_scale",        "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Console font size scale.", 0.5f, 8.0f ),
	/*  5 */ CVAR_FLOAT(  "con_lineheight",   "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Notify/console line height multiplier (1.0 = tight, 2.0 = double-spaced).", 0.5f, 4.0f ),
	/*  6 */ CVAR_BOOL(   "con_anim",         "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Animate console open/close. 0 = instant snap." ),
	/*  7 */ CVAR_FLOAT(  "cl_consoleHeight", "0.5",      CVAR_ARCHIVE,    "Fraction of screen height covered by the console when open (0.1-1.0).", 0.1f, 1.0f ),
	/*  8 */ CVAR_INT(    "cl_consoleType",   "0",        CVAR_ARCHIVE,    "Console background style: 0=themed (con_colBG), 1=classic Q3 shader.", 0, 1 ),
	/*  9 */ CVAR_BOOL(   "con_clock",        "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Draw wall-clock HH:MM:SS in the top-right corner of the console." ),
	/* 10 */ CVAR_BOOL(   "con_fade",         "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Fade notify lines to transparent before they expire instead of popping." ),
	/* 11 */ CVAR_BOOL(   "con_fps",          "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Draw current FPS in the top-right corner of the console." ),
	/* 12 */ CVAR_BOOL(   "con_timestamp",    "1",        CVAR_ARCHIVE | CVAR_NODEFAULT, "Prefix each console line with a HH:MM:SS timestamp." ),
	/* 13 */ CVAR_STRING( "con_colBG",        "101013F6", CVAR_ARCHIVE,    "Console background color (hex RRGGBB or RRGGBBAA)." ),
	/* 14 */ CVAR_STRING( "con_colBorder",    "4778B2FF", CVAR_ARCHIVE,    "Console border color (hex RRGGBB or RRGGBBAA)." ),
	/* 15 */ CVAR_STRING( "con_colText",      "E2E2E2",   CVAR_ARCHIVE,    "Console text color (hex RRGGBB)." ),
	/* 16 */ CVAR_STRING( "con_colCVar",      "4778B2",   CVAR_ARCHIVE,    "Console input color for cvar names (hex RRGGBB)." ),
	/* 17 */ CVAR_STRING( "con_colCmd",       "4FA7BD",   CVAR_ARCHIVE,    "Console input color for command names (hex RRGGBB)." ),
	/* 18 */ CVAR_STRING( "con_colValue",     "E5BC39",   CVAR_ARCHIVE,    "Console input color for cvar values (hex RRGGBB)." ),
};
enum {
	CON_NOTIFYTIME, CON_NOTIFYLINES, CON_CONSPEED, CON_AUTOCLEAR,
	CON_SCALE, CON_LINEHEIGHT, CON_ANIM, CON_HEIGHT, CON_TYPE,
	CON_CLOCK, CON_FADE, CON_FPS, CON_TIMESTAMP,
	CON_COLBG, CON_COLBORDER, CON_COLTEXT, CON_COLCVAR, CON_COLCMD, CON_COLVALUE,
	CON_CVAR_COUNT
};
_Static_assert( ARRAY_LEN( s_conDescs ) == CON_CVAR_COUNT, "s_conDescs/enum mismatch" );
static cvar_t *s_conHandles[CON_CVAR_COUNT];


/*
================
Con_Init
================
*/
void Con_Init( void )
{
	/* Allocate console state from the persistent arena if not already done.
	   May have been lazily allocated by CL_ConsolePrint before this call.
	   The arena survives Hunk_ClearLevel() so text and scroll position are
	   preserved across map transitions.  On re-entry (renderer restart),
	   the arena already exists — just re-init cvars and commands below. */
	if ( !s_conArena ) {
		s_conArena = Arena_Create( "Console", CONSOLE_ARENA_SIZE );
		s_con = Arena_AllocType( s_conArena, console_t );
		memset( s_con, 0, sizeof( console_t ) );
	} else if ( !s_con ) {
		/* arena was created by lazy init, s_con was set there; shouldn't be NULL here */
		s_con = Arena_AllocType( s_conArena, console_t );
		memset( s_con, 0, sizeof( console_t ) );
	}

	Cvar_RegisterTable( s_conDescs, ARRAY_LEN( s_conDescs ), s_conHandles );
	con_notifytime    = s_conHandles[CON_NOTIFYTIME];
	con_notifylines   = s_conHandles[CON_NOTIFYLINES];
	con_conspeed      = s_conHandles[CON_CONSPEED];
	con_autoclear     = s_conHandles[CON_AUTOCLEAR];
	con_scale         = s_conHandles[CON_SCALE];
	con_lineheight    = s_conHandles[CON_LINEHEIGHT];
	con_anim          = s_conHandles[CON_ANIM];
	cl_consoleHeight  = s_conHandles[CON_HEIGHT];
	cl_consoleType    = s_conHandles[CON_TYPE];
	con_clock         = s_conHandles[CON_CLOCK];
	con_fade          = s_conHandles[CON_FADE];
	con_fps           = s_conHandles[CON_FPS];
	con_timestamp     = s_conHandles[CON_TIMESTAMP];
	/* per-element console colors */
	con_colBG         = s_conHandles[CON_COLBG];
	con_colBorder     = s_conHandles[CON_COLBORDER];
	con_colText       = s_conHandles[CON_COLTEXT];
	con_colCVar       = s_conHandles[CON_COLCVAR];
	con_colCmd        = s_conHandles[CON_COLCMD];
	con_colValue      = s_conHandles[CON_COLVALUE];

	/* the presentation-only setup (Con_UpdateColors, the
	 * g_consoleField clear/width, and the toggleconsole / messagemode*
	 * command registrations) moved to Con_InitProjection() in the UI
	 * (elements/console.c), called once from the client init path right
	 * after Con_Init. Core registers only presentation-agnostic commands. */
	Cmd_AddCommand( "clear", Con_Clear_f );
	Cmd_AddCommand( "dumpConsole", Con_Dump_f );
	Cmd_SetCommandCompletionFunc( "dumpConsole", Cmd_CompleteTxtName );
}


/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void )
{
	Cmd_RemoveCommand( "clear" );
	Cmd_RemoveCommand( "dumpConsole" );
	/* toggleconsole / messagemode* removes live in Con_ShutdownProjection()
	 * (UI), mirroring Con_InitProjection(). */
}


/*
===============
Con_Fixup
===============
*/
static void Con_Fixup( void )
{
	int filled;

	if ( con.current >= con.totallines ) {
		filled = con.totallines;
	} else {
		filled = con.current + 1;
	}

	if ( filled <= con.vispage ) {
		con.display = con.current;
	} else if ( con.current - con.display > filled - con.vispage ) {
		con.display = con.current - filled + con.vispage;
	} else if ( con.display > con.current ) {
		con.display = con.current;
	}
}


/*
===============
Con_Linefeed

Move to newline only when we _really_ need this
===============
*/
static void Con_NewLine( void )
{
	// follow last line
	if ( con.display == con.current )
		con.display++;
	con.current++;

	short *s = &con.text[ ( con.current % con.totallines ) * con.linewidth ];
	for ( int i = 0; i < con.linewidth ; i++ )
		*s++ = (ColorIndex(COLOR_WHITE)<<8) | ' ';

	/* severity-as-data: every line of the in-flight print carries the same
	 * s_printSeverity (the whole print, including word-wrapped rows). */
	con.severities[ con.current % con.totallines ] = s_printSeverity;

	con.x = 0;
}


/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed( qboolean skipnotify )
{
	// mark time for transparent overlay
	if ( con.current >= 0 )	{
		if ( skipnotify )
			con.times[ con.current % NUM_CON_TIMES ] = 0;
		else
			con.times[ con.current % NUM_CON_TIMES ] = Sys_Milliseconds();
	}

	if ( con.newline ) {
		Con_NewLine();
	} else {
		con.newline = qtrue;
		con.x = 0;
	}

	Con_Fixup();
}


/*
================
Con_PrintInternal

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window

The line severity tagged onto every emitted row is whatever s_printSeverity
the public entry point set before calling here.
================
*/
static void Con_PrintInternal( const char *txt ) {
	qboolean skipnotify = qfalse;		// NERVE - SMF

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}

	// for some demos we don't want to ever show anything on the console.
	// cl_noprint is a client cvar; the core looks it up by name so it stays
	// glconfig/client-header free (returns 0 when absent — headless / early).
	if ( Cvar_VariableIntegerValue( "cl_noprint" ) ) {
		return;
	}

	/* Lazily allocate the arena on first print (may happen before Con_Init
	   due to early Com_Log calls at engine startup).  Con_Init completes
	   the setup (cvars, cmd registration) later. */
	if ( !s_con ) {
		if ( !s_conArena ) {
			s_conArena = Arena_Create( "Console", CONSOLE_ARENA_SIZE );
			s_con = Arena_AllocType( s_conArena, console_t );
			memset( s_con, 0, sizeof( console_t ) );
		} else {
			return;   /* arena exists but s_con is NULL — shouldn't happen */
		}
	}

	if ( !con.initialized ) {
		/* Minimal core default geometry so early prints (before the UI ever
		 * calls Con_Reflow) have a valid ring layout. NO glconfig, NO
		 * con_scale, NO render metrics — the UI pushes the real geometry via
		 * Con_Reflow on its first frame. Headless never had glconfig anyway,
		 * so this is the only layout the dedicated build ever uses. */
		con.viswidth   = -9999;
		con.linewidth  = DEFAULT_CONSOLE_WIDTH;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		con.vispage    = 4;
		for ( int i = 0; i < con.totallines && i < CON_MAX_TOTALLINES; i++ )
			con.severities[i] = SEV_INFO;
		con.initialized = qtrue;
	}

	/* Tag the current line at entry so a print that appends to an existing
	 * line (no leading newline) still colors that row; Con_NewLine() tags
	 * each subsequent row of this print. */
	con.severities[ con.current % con.totallines ] = s_printSeverity;

	int colorIndex = ColorIndex( COLOR_WHITE );

	int c;
	while ( (c = (byte)*txt) != 0 ) {
		if ( Q_IsColorString( txt ) && *(txt+1) != '\n' ) {
			colorIndex = ColorIndexFromChar( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		int l;
		for ( l = 0 ; l < con.linewidth ; l++ ) {
			if ( txt[l] <= ' ' ) {
				break;
			}
		}

		// word wrap — only if the word fits on a fresh line.
		// With timestamps (9 cols), a word of length l requires l + 9 <= linewidth.
		// If the word is too long to fit even on a fresh line, let overflow
		// handle it char-by-char instead of cascading single-char wraps.
		{
			int ts_cols = ( con_timestamp && con_timestamp->integer ) ? 9 : 0;
			if ( l < con.linewidth - ts_cols && ( con.x + l >= con.linewidth ) ) {
				Con_Linefeed( skipnotify );
			}
		}

		txt++;

		switch( c )
		{
		case '\n':
			Con_Linefeed( skipnotify );
			break;
		case '\r':
			con.x = 0;
			break;
		default:
			if ( con.newline ) {
				Con_NewLine();
				Con_Fixup();
				con.newline = qfalse;
			}
			// display character and advance
			{
				int y = con.current % con.totallines;
				con.text[y * con.linewidth + con.x ] = (colorIndex << 8) | (c & 255);
				con.x++;
				if ( con.x >= con.linewidth ) {
					Con_Linefeed( skipnotify );
				}
			}
			break;
		}
	}

	// mark time for transparent overlay
	if ( con.current >= 0 ) {
		if ( skipnotify ) {
			int prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 )
				prev = NUM_CON_TIMES - 1;
			con.times[ prev ] = 0;
		} else {
			con.times[ con.current % NUM_CON_TIMES ] = Sys_Milliseconds();
		}
	}
}


/*
================
CL_ConsolePrint

Engine-wide print entry for non-log callers. Tags lines SEV_INFO (no
special severity → console default color). Signature unchanged.
================
*/
void CL_ConsolePrint( const char *txt ) {
	s_printSeverity = SEV_INFO;
	Con_PrintInternal( txt );
}


/*
================
Con_PrintSeverity

Severity-carrying print entry (severity-as-data). The log console sink
routes Com_Log lines here so the UI can color each line by severity
without a text bracket being injected into the buffer.
================
*/
void Con_PrintSeverity( log_severity_t sev, const char *txt ) {
	s_printSeverity = sev;
	Con_PrintInternal( txt );
	s_printSeverity = SEV_INFO;
}


/*
================
Con_GetLineSeverity

Severity of an absolute line index (mirrors con.text ring addressing).
Returns SEV_INFO when the buffer is not yet allocated.
================
*/
log_severity_t Con_GetLineSeverity( int line ) {
	if ( !s_con || con.totallines <= 0 )
		return SEV_INFO;
	return con.severities[ ((line % con.totallines) + con.totallines) % con.totallines ];
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down. Geometry-injected: consoleKeyActive is whether the
console key-catcher is held by the caller; frameMsec is the real frame
time in ms. Core reads no Key_* / cls state.
==================
*/
void Con_RunConsole( qboolean consoleKeyActive, int frameMsec )
{
	// decide on the destination height of the console. FULL covers the screen
	// (attract <-> full-console pair); HALF is the upper-portion overlay
	// (cl_consoleHeight) that leaves the underlying state visible below.
	if ( consoleKeyActive )
		con.finalFrac = ( con.viewMode == CON_VIEW_FULL ) ? 1.0f : cl_consoleHeight->value;
	else
		con.finalFrac = 0.0;	// none visible

	// instant snap when animation is disabled
	if ( !con_anim->integer ) {
		con.displayFrac = con.finalFrac;
		return;
	}

	// scroll towards the destination height
	if ( con.finalFrac < con.displayFrac )
	{
		con.displayFrac -= con_conspeed->value * frameMsec * 0.001;
		if ( con.finalFrac > con.displayFrac )
			con.displayFrac = con.finalFrac;

	}
	else if ( con.finalFrac > con.displayFrac )
	{
		con.displayFrac += con_conspeed->value * frameMsec * 0.001;
		if ( con.finalFrac < con.displayFrac )
			con.displayFrac = con.finalFrac;
	}
}


void Con_PageUp( int lines )
{
	if ( lines == 0 )
		lines = con.vispage - 2;

	con.display -= lines;

	Con_Fixup();
}


void Con_PageDown( int lines )
{
	if ( lines == 0 )
		lines = con.vispage - 2;

	con.display += lines;

	Con_Fixup();
}


void Con_Top( void )
{
	// this is generally incorrect but will be adjusted in Con_Fixup()
	con.display = con.current - con.totallines;

	Con_Fixup();
}


void Con_Bottom( void )
{
	con.display = con.current;

	Con_Fixup();
}


/* UI close hook. The presentation half of Con_Close (clearing the input
 * field, dropping the KEYCATCH_CONSOLE bit) lives in the UI projection; it
 * registers this hook in Con_InitProjection(). NULL on headless / before the
 * UI initializes — core then just collapses the buffer view. */
static void (*s_conCloseHook)( void ) = NULL;

void Con_SetCloseHook( void (*hook)( void ) )
{
	s_conCloseHook = hook;
}

void Con_Close( void )
{
	/* No com_cl_running guard: every caller is client-side (cl_*.c) and runs
	 * only with the client subsystem up, so the guard was redundant defense.
	 * Dropping it keeps con_buffer.c free of the client-only com_cl_running
	 * cvar (headless-build tier rule). Zeroing the frac + clearing notify is
	 * harmless regardless of client state. */
	Con_SearchClose();
	Con_ClearNotify();
	con.finalFrac = 0.0;			// none visible
	con.displayFrac = 0.0;

	/* presentation half (field clear + key-catcher drop) — UI-provided */
	if ( s_conCloseHook )
		s_conCloseHook();
}


/*
================
Con_SoftClose

Collapses the console visually (displayFrac/finalFrac = 0) and clears
notify lines, but preserves the KEYCATCH_CONSOLE bit so the user does
not have to re-open the console after a map transition.  Use this in
place of Con_Close() on any code path that is NOT a deliberate "hide
the console from the user" action (e.g. map-change, cgame init).
================
*/
void Con_SoftClose( void )
{
	/* No com_cl_running guard — see Con_Close (client-only callers; keeps
	 * the client-tier cvar out of the headless-capable core). */
	con.finalFrac   = 0.0f;
	con.displayFrac = 0.0f;
	Con_ClearNotify();
	/* KEYCATCH_CONSOLE deliberately preserved */
}


/*
==============================================================================

CONSOLE SEARCH

==============================================================================
*/

/*
================
Con_LineToString

Extract ASCII text from a console line into a buffer.
Strips Q3 color codes (high byte) and trailing spaces.
================
*/
static void Con_LineToString( int line, char *buf, int bufSize ) {
	short *text = con.text + ( line % con.totallines ) * con.linewidth;
	int len = con.linewidth;
	if ( len >= bufSize )
		len = bufSize - 1;

	for ( int x = 0; x < len; x++ )
		buf[x] = text[x] & 0xff;

	buf[len] = '\0';

	// trim trailing spaces
	for ( int x = len - 1; x >= 0 && buf[x] == ' '; x-- )
		buf[x] = '\0';
}


/*
================
Con_SearchFind

Search for the current pattern starting from startLine, going in direction.
Returns the matching line index, or -1 if not found.
direction: -1 = backward (toward older lines), +1 = forward (toward newer lines)
================
*/
static int Con_SearchFind( int startLine, int direction ) {
	if ( !con.searchPattern[0] )
		return -1;

	char lineBuf[CON_LINEBUF_SIZE];

	/* Valid line range: [oldest, con.current].
	   Use modular arithmetic so backward search wraps correctly even when
	   the ring buffer is full (oldest > 0). The old code used
	   "while (line < 0) line += numLines" which never triggered for a full
	   buffer since oldest is positive, causing backward wrap to silently
	   fail. */
	int numLines, oldest;
	if ( con.current >= con.totallines ) {
		numLines = con.totallines;
		oldest   = con.current - con.totallines + 1;
	} else {
		numLines = con.current + 1;
		oldest   = 0;
	}

	/* Clamp startLine into the valid window — it can go stale if new lines
	   arrived after the last search (con.current advanced). */
	if ( startLine < oldest )   startLine = oldest;
	if ( startLine > con.current ) startLine = con.current;

	/* pos is an offset within [0, numLines-1] where 0=oldest, numLines-1=newest.
	   Step by direction each iteration; modular wrap stays within valid range.
	   Loop numLines-1 times — visits every line except startLine itself. */
	int pos = startLine - oldest;
	for ( int i = 0; i < numLines - 1; i++ ) {
		pos = ( pos + direction + numLines ) % numLines;
		int line = oldest + pos;

		Con_LineToString( line, lineBuf, sizeof( lineBuf ) );
		if ( Q_stristr( lineBuf, con.searchPattern ) )
			return line;
	}

	return -1;
}


/*
================
Con_SearchCountMatches

Count total occurrences of the search pattern in the console buffer.
================
*/
static int Con_SearchCountMatches( void ) {
	if ( !con.searchPattern[0] )
		return 0;

	char	lineBuf[CON_LINEBUF_SIZE];

	int count = 0;

	int numLines;
	if ( con.current >= con.totallines )
		numLines = con.totallines;
	else
		numLines = con.current + 1;

	for ( int i = 0; i < numLines; i++ ) {
		int line;
		if ( con.current >= con.totallines )
			line = ( con.current + 1 + i ) % con.totallines;
		else
			line = i;

		Con_LineToString( line, lineBuf, sizeof( lineBuf ) );
		if ( Q_stristr( lineBuf, con.searchPattern ) )
			count++;
	}

	return count;
}


/*
================
Con_SearchUpdate

Re-run search from current match position after pattern changes.
================
*/
static int Con_VisibleRows( void ) {
	// Visible content rows used to center a search match. con.vispage is the
	// visible-rows page size pushed by the UI via Con_Reflow (presentation-
	// agnostic — no pixel height / render metrics in core). The legacy code
	// derived this from console pixel height minus the reserved bottom slots;
	// vispage carries the same geometry without the glconfig dependency.
	int vis = con.vispage > 0 ? con.vispage : 1;
	return vis < 1 ? 1 : vis;
}

static void Con_ScrollToLine( int line ) {
	int half = Con_VisibleRows() / 2;
	con.display = line + 1 + half;
	if ( con.display > con.current )
		con.display = con.current;
	Con_Fixup();
}

static void Con_SearchUpdate( void ) {
	if ( !con.searchPattern[0] ) {
		con.searchLine = -1;
		con.searchMatchCount = 0;
		return;
	}

	con.searchMatchCount = Con_SearchCountMatches();

	// first check if current display line matches (don't jump away needlessly)
	if ( con.display >= 0 && con.current - con.display < con.totallines ) {
		char	lineBuf[CON_LINEBUF_SIZE];
		Con_LineToString( con.display, lineBuf, sizeof( lineBuf ) );
		if ( Q_stristr( lineBuf, con.searchPattern ) ) {
			con.searchLine = con.display;
			return;
		}
	}

	// search backward from current display position
	con.searchLine = Con_SearchFind( con.display, -1 );

	if ( con.searchLine >= 0 )
		Con_ScrollToLine( con.searchLine );
}


/*
================
Con_SearchOpen

Activate console search mode (Ctrl-F).
================
*/
void Con_SearchOpen( void ) {
	con.searchActive = qtrue;
	con.searchPattern[0] = '\0';
	con.searchCursor = 0;
	con.searchLine = -1;
	con.searchMatchCount = 0;
}


/*
================
Con_SearchClose

Deactivate console search mode.
================
*/
void Con_SearchClose( void ) {
	con.searchActive = qfalse;
	con.searchPattern[0] = '\0';
	con.searchCursor = 0;
	con.searchLine = -1;
	con.searchMatchCount = 0;
}


/*
================
Con_SearchNext

Jump to next (forward=qtrue) or previous (forward=qfalse) match.
================
*/
void Con_SearchNext( qboolean forward ) {
	if ( !con.searchActive || !con.searchPattern[0] )
		return;

	int startLine = ( con.searchLine >= 0 ) ? con.searchLine : con.display;
	int dir = forward ? 1 : -1;

	int found = Con_SearchFind( startLine, dir );
	if ( found >= 0 ) {
		con.searchLine = found;
		Con_ScrollToLine( found );
	}
}


/*
================
Con_SearchChar

Handle a character typed into the search bar.
================
*/
void Con_SearchChar( int ch ) {
	if ( !con.searchActive )
		return;

	if ( ch == '\b' || ch == 127 ) {
		// backspace
		if ( con.searchCursor > 0 ) {
			con.searchCursor--;
			con.searchPattern[con.searchCursor] = '\0';
			Con_SearchUpdate();
		} else {
			Con_SearchClose();
		}
		return;
	}

	if ( ch < 32 )
		return;

	if ( con.searchCursor < (int)sizeof( con.searchPattern ) - 1 ) {
		con.searchPattern[con.searchCursor] = ch;
		con.searchCursor++;
		con.searchPattern[con.searchCursor] = '\0';
		Con_SearchUpdate();
	}
}


/*
================
Con_IsSearchActive
================
*/
qboolean Con_IsSearchActive( void ) {
	return con.searchActive;
}


/*
================
Con_SearchLine

Returns the currently highlighted search line, or -1.
================
*/
int Con_SearchLine( void ) {
	return con.searchActive ? con.searchLine : -1;
}


/*
==============================================================================

CONSOLE MARK MODE (Ctrl+M text selection)

Provides a keyboard-only text selection interface for the console buffer:
  Ctrl+M        Toggle mark mode.  Initial selection anchors at the current
                bottom-most visible line.
  Arrows        Move the "end" marker (with Shift) or move and reset the
                anchor (without Shift).
  Home / End    Jump to line start / end of the current row.
  PgUp / PgDn   Move the end marker by one page.
  Ctrl+C / Enter  Copy selection to the system clipboard with color codes
                stripped, then exit mark mode.
  Esc           Exit mark mode without copying.

The selection rectangle is drawn with a distinct background color in
Con_DrawSolidConsole.  Lines are stored as absolute indices into the
ring buffer, matching con.current / con.display semantics.

==============================================================================
*/

/*
================
Con_MarkValidLine

Clamp an absolute line index to the portion of the scrollback buffer that
still has live data available.  Returns the (possibly clamped) index.
================
*/
static int Con_MarkValidLine( int line ) {
	int firstValid;

	firstValid = con.current - (con.totallines - 1);
	if ( firstValid < 0 ) {
		firstValid = 0;
	}

	if ( line < firstValid ) {
		line = firstValid;
	}
	if ( line > con.current ) {
		line = con.current;
	}
	return line;
}


/*
================
Con_MarkClampCol

Clamp a column index to [0, linewidth).
================
*/
static int Con_MarkClampCol( int col ) {
	if ( col < 0 ) {
		col = 0;
	}
	if ( col >= con.linewidth ) {
		col = con.linewidth - 1;
	}
	return col;
}


/*
================
Con_MarkEnsureVisible

Scroll the console display so that the current end-of-selection row is
within the visible region.
================
*/
static void Con_MarkEnsureVisible( void ) {
	int visibleRows = con.vispage;
	if ( visibleRows <= 0 ) {
		visibleRows = 1;
	}

	if ( con.markEndLine > con.display ) {
		con.display = con.markEndLine;
	} else if ( con.markEndLine < con.display - visibleRows + 1 ) {
		con.display = con.markEndLine + visibleRows - 1;
	}

	Con_Fixup();
}


/*
================
Con_MarkOpen

Activate mark mode.  The anchor and cursor both start at the bottom-most
line currently on screen, column 0.
================
*/
void Con_MarkOpen( void ) {
	con.markActive = qtrue;
	con.markStartLine = con.display;
	con.markStartCol = 0;
	con.markEndLine = con.display;
	con.markEndCol = 0;
}


/*
================
Con_MarkClose
================
*/
void Con_MarkClose( void ) {
	con.markActive = qfalse;
}


/*
================
Con_IsMarkActive
================
*/
qboolean Con_IsMarkActive( void ) {
	return con.markActive;
}


/*
================
Con_MarkGetRange

Returns the normalized selection rectangle:
  line1,col1 = top-left (earliest)
  line2,col2 = bottom-right (latest)
================
*/
static void Con_MarkGetRange( int *line1, int *col1, int *line2, int *col2 ) {
	int sl = con.markStartLine;
	int sc = con.markStartCol;
	int el = con.markEndLine;
	int ec = con.markEndCol;

	if ( sl < el || ( sl == el && sc <= ec ) ) {
		*line1 = sl; *col1 = sc;
		*line2 = el; *col2 = ec;
	} else {
		*line1 = el; *col1 = ec;
		*line2 = sl; *col2 = sc;
	}
}


/*
================
Con_MarkRowIsSelected

Returns qtrue if (row, col) falls inside the normalized selection.
================
*/
qboolean Con_MarkCellIsSelected( int row, int col ) {
	if ( !con.markActive ) {
		return qfalse;
	}

	int l1, c1, l2, c2;
	Con_MarkGetRange( &l1, &c1, &l2, &c2 );

	if ( row < l1 || row > l2 ) {
		return qfalse;
	}

	if ( l1 == l2 ) {
		return (col >= c1 && col <= c2) ? qtrue : qfalse;
	}

	if ( row == l1 ) {
		return (col >= c1) ? qtrue : qfalse;
	}
	if ( row == l2 ) {
		return (col <= c2) ? qtrue : qfalse;
	}
	return qtrue;
}


/*
================
Con_MarkCopySelection

Extract the currently selected text (stripping any embedded color bytes,
which live in the high byte of each short in con.text) and push it to the
system clipboard.
================
*/
static void Con_MarkCopySelection( void ) {
	if ( !con.markActive ) {
		return;
	}

	int l1, c1, l2, c2;
	char buf[CON_TEXTSIZE + 4];
	int bufPos = 0;
	Con_MarkGetRange( &l1, &c1, &l2, &c2 );

	for ( int line = l1; line <= l2; line++ ) {
		/* validate line is inside the live scrollback */
		if ( con.current - line >= con.totallines ) {
			continue;
		}

		short *text = con.text + (line % con.totallines) * con.linewidth;

		int startCol = (line == l1) ? c1 : 0;
		int endCol   = (line == l2) ? c2 : (con.linewidth - 1);

		for ( int col = startCol; col <= endCol; col++ ) {
			char ch = (char)(text[col] & 0xff);
			/* color codes are stored in the high byte, not inline — no need
			   to strip ^x sequences here, but leave the guard in place so
			   paste targets never see unrenderable bytes. */
			if ( ch == '\0' ) {
				ch = ' ';
			}
			if ( bufPos < (int)sizeof( buf ) - 1 ) {
				buf[bufPos++] = ch;
			}
		}

		if ( line != l2 && bufPos < (int)sizeof( buf ) - 2 ) {
			buf[bufPos++] = '\r';
			buf[bufPos++] = '\n';
		}
	}

	buf[bufPos] = '\0';

	/* Sys_SetClipboardData is a client/UI platform symbol (no dedicated-server
	 * definition). Mark/copy is a visible-console interaction that never fires
	 * on a headless server, but the symbol still has to link — guard it out of
	 * the HEADLESS (qcommon_ded) build. */
#ifndef HEADLESS
	Sys_SetClipboardData( buf );
#endif
}


/*
================
Con_MarkMove

Move the mark cursor (markEnd) by one logical step. The UI tier translates
keystrokes into a conMarkMove_t + modifier flags and calls this; core owns
all the cursor arithmetic so no client-tier keycodes leak into qcommon.

  extendSel  — qtrue keeps the anchor (Shift-select); qfalse collapses the
               anchor onto the cursor after the move.
  toExtreme  — the Ctrl modifier for HOME/END (Ctrl+Home → buffer top,
               Ctrl+End → current line); ignored for every other move.

Each branch ends by (optionally) collapsing the anchor and scrolling the
cursor row back into view — identical to the old Con_MarkKey behavior.
================
*/
void Con_MarkMove( conMarkMove_t move, qboolean extendSel, qboolean toExtreme ) {
	if ( !con.markActive ) {
		return;
	}

	switch ( move ) {
	case CON_MARK_LEFT:
		if ( con.markEndCol > 0 ) {
			con.markEndCol--;
		} else if ( con.markEndLine > Con_MarkValidLine( 0 ) ) {
			con.markEndLine--;
			con.markEndCol = con.linewidth - 1;
		}
		break;

	case CON_MARK_RIGHT:
		if ( con.markEndCol < con.linewidth - 1 ) {
			con.markEndCol++;
		} else if ( con.markEndLine < con.current ) {
			con.markEndLine++;
			con.markEndCol = 0;
		}
		break;

	case CON_MARK_UP:
		con.markEndLine = Con_MarkValidLine( con.markEndLine - 1 );
		con.markEndCol = Con_MarkClampCol( con.markEndCol );
		break;

	case CON_MARK_DOWN:
		con.markEndLine = Con_MarkValidLine( con.markEndLine + 1 );
		con.markEndCol = Con_MarkClampCol( con.markEndCol );
		break;

	case CON_MARK_HOME:
		con.markEndCol = 0;
		if ( toExtreme ) {
			con.markEndLine = Con_MarkValidLine( 0 );
		}
		break;

	case CON_MARK_END:
		con.markEndCol = con.linewidth - 1;
		if ( toExtreme ) {
			con.markEndLine = con.current;
		}
		break;

	case CON_MARK_PGUP:
		{
			int step = con.vispage > 0 ? con.vispage : 1;
			con.markEndLine = Con_MarkValidLine( con.markEndLine - step );
			con.markEndCol = Con_MarkClampCol( con.markEndCol );
		}
		break;

	case CON_MARK_PGDN:
		{
			int step = con.vispage > 0 ? con.vispage : 1;
			con.markEndLine = Con_MarkValidLine( con.markEndLine + step );
			con.markEndCol = Con_MarkClampCol( con.markEndCol );
		}
		break;
	}

	if ( !extendSel ) {
		con.markStartLine = con.markEndLine;
		con.markStartCol = con.markEndCol;
	}
	Con_MarkEnsureVisible();
}


/*
================
Con_MarkCopyAndClose

Copy the current selection to the clipboard and leave mark mode. The UI tier
binds this to Ctrl+C / Enter.
================
*/
void Con_MarkCopyAndClose( void ) {
	Con_MarkCopySelection();
	Con_MarkClose();
}


/*
==============================================================================

READ-ONLY BUFFER ACCESSORS

The UI projection (elements/console.c) reads buffer state ONLY through these
— it never dereferences console_t or the `con` macro. Scalars are copied out;
the POD view structs are filled. Nothing internal (glconfig, vec4_t colors,
cvars) is exposed.

==============================================================================
*/

const short* Con_GetBuffer( void ) {
	return s_con ? con.text : NULL;
}

int Con_GetLineWidth( void ) {
	return s_con ? con.linewidth : 0;
}

int Con_GetTotalLines( void ) {
	return s_con ? con.totallines : 0;
}

int Con_GetCurrentLine( void ) {
	return s_con ? con.current : 0;
}

int Con_GetDisplayLine( void ) {
	return s_con ? con.display : 0;
}

float Con_GetDisplayFrac( void ) {
	return s_con ? con.displayFrac : 0.0f;
}

void Con_SetViewMode( conViewMode_t mode ) {
	if ( s_con ) {
		con.viewMode = mode;
	}
}

conViewMode_t Con_GetViewMode( void ) {
	return s_con ? con.viewMode : CON_VIEW_FULL;
}

int Con_GetNotifyTime( int idx ) {
	if ( !s_con )
		return 0;
	return con.times[ ((idx % NUM_CON_TIMES) + NUM_CON_TIMES) % NUM_CON_TIMES ];
}

qboolean Con_GetSearchState( conSearchView_t *out ) {
	if ( !out )
		return qfalse;
	if ( !s_con ) {
		memset( out, 0, sizeof( *out ) );
		return qfalse;
	}
	out->active     = con.searchActive;
	Q_strncpyz( out->pattern, con.searchPattern, sizeof( out->pattern ) );
	out->matchCount = con.searchMatchCount;
	out->line       = con.searchLine;
	return con.searchActive;
}

qboolean Con_GetMarkState( conMarkView_t *out ) {
	if ( !out )
		return qfalse;
	if ( !s_con ) {
		memset( out, 0, sizeof( *out ) );
		return qfalse;
	}
	out->active    = con.markActive;
	out->startLine = con.markStartLine;
	out->startCol  = con.markStartCol;
	out->endLine   = con.markEndLine;
	out->endCol    = con.markEndCol;
	return con.markActive;
}
